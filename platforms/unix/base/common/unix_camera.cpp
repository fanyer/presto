/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef PI_CAMERA

#include "unix_camera.h"

#include "platforms/quix/commandline/StartupSettings.h"

#include "platforms/posix/posix_native_util.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/pi/OpBitmap.h"
#include "modules/img/src/imagedecoderjpg.h"
#include "modules/hardcore/timer/optimer.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#ifndef __FreeBSD__
#include <linux/netlink.h>
#endif // __FreeBSD__

namespace
{
	const unsigned CAMERA_CAPTURE_TIMEOUT   = 40; /**< Default equal to 25 fps. */
	const char DEVICE_V4L_PATH_FORMAT[]		= "/dev/%s";
#if defined(__FreeBSD__)
	const uni_char V4L_DEVICES_SEARCH_DIR[] = UNI_L("/dev");
	const uni_char DEVICE_V4L_PATTERN[]     = UNI_L("video*");
#else
	const uni_char V4L_DEVICES_SEARCH_DIR[] = UNI_L("/sys/class/video4linux");
	const uni_char DEVICE_V4L_PATTERN[]     = UNI_L("*");
#endif

	int xioctl(int fd, int request, void* arg)
	{
		int r;

		do r = ioctl (fd, request, arg);
		while (r == -1 && errno == EINTR);

		return r;
	}

	void LogInfo(const char *fmt, ...)
	{
		if (!g_startup_settings.debug_camera)
			return;

		va_list args;
		va_start(args, fmt);
		fprintf(stderr, "CAMERA> info: ");
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
		va_end(args);
	}

	void LogError(const char *fmt, ...)
	{
		if (!g_startup_settings.debug_camera)
			return;

		va_list args;
		va_start(args, fmt);
		fprintf(stderr, "CAMERA> error: ");
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
		va_end(args);
	}
}

OP_STATUS OpCameraManager::Create(OpCameraManager** new_camera_manager)
{
	*new_camera_manager = OP_NEW(UnixOpCameraManager, ());
	RETURN_OOM_IF_NULL(*new_camera_manager);

	return OpStatus::OK;
}

UnixOpCameraManager::UnixOpCameraManager()
	: m_device_listener(NULL)
#ifndef __FreeBSD__
	, m_netlink_socket(-1)
#endif // __FreeBSD__
{
	GetCameraDevices();
}

UnixOpCameraManager::~UnixOpCameraManager()
{
#ifndef __FreeBSD__
	if (m_netlink_socket >= 0)
	{
		if (g_posix_selector)
		{
			g_posix_selector->Detach(this);
		}
		close(m_netlink_socket);
		m_netlink_socket = -1;
	}
#endif // __FreeBSD__
}

#ifndef __FreeBSD__

void UnixOpCameraManager::InitNetlinkListener()
{
	OP_ASSERT(m_netlink_socket < 0);
	m_netlink_socket = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (m_netlink_socket < 0)
	{
		return;
	}

	sockaddr_nl socket_addr;
	socket_addr.nl_family = AF_NETLINK;
	socket_addr.nl_pad = 0;
	socket_addr.nl_pid = getpid();
	socket_addr.nl_groups = NETLINK_MONITOR_KERNEL_GROUP;
	int bind_socket = bind(m_netlink_socket, reinterpret_cast<sockaddr*>(&socket_addr), sizeof(socket_addr));
	if (bind_socket < 0)
	{
		close(m_netlink_socket);
		m_netlink_socket = -1;
		return;
	}
	if (OpStatus::IsError(g_posix_selector->Watch(m_netlink_socket, PosixSelector::READ, this)))
	{
		close(m_netlink_socket);
		m_netlink_socket = -1;
	}
}

void UnixOpCameraManager::ParseBuffer(char* buffer)
{
	OpStringC8 str(buffer);
	int bytes_to_parse = str.Length();
	int bytes_parsed = 0;

	while (bytes_parsed < bytes_to_parse)
	{
		OpStringC8 str(buffer);
		int line_end = str.Find("\n");
		if (line_end != KNotFound)
		{
			buffer[line_end] = '\0';
			bytes_parsed += line_end + 1;
		}
		else
		{
			bytes_parsed += str.Length();
		}
		ParseLine(buffer);
		buffer += bytes_parsed;
	}
}

void UnixOpCameraManager::ParseLine(OpStringC8 line)
{
	// Messages from kernel are in the following format:
	// add@/devices/pci0000:00/0000:00:12.2/usb1/1-1/1-1:1.0/video4linux/video0
	// remove@/devices/pci0000:00/0000:00:12.2/usb1/1-1/1-1:1.0/video4linux/video1

	int v4lpos = line.Find("video4linux/video");
	if (v4lpos == KNotFound)
	{
		return;
	}

	OpStringC8 node_name = line.SubString(v4lpos + 12); // 12 == strlen(video4linux/)

	if (line.Compare("add@/", 5) == 0)
	{
		if (OpStatus::IsError(AddCamera(node_name)))
		{
			LogError("adding camera device failed: %s", node_name.CStr());
		}
	}
	else if (line.Compare("remove@/", 8) == 0)
	{
		if (OpStatus::IsError(RemoveCamera(node_name)))
		{
			LogError("removing camera device failed: %s", node_name.CStr());
		}
	}
}

void UnixOpCameraManager::OnReadReady(int fd)
{
    OP_ASSERT(fd == m_netlink_socket);
    fd_set rfds;
    timeval tv;
    FD_ZERO(&rfds);
    FD_SET(m_netlink_socket, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

    while (select(m_netlink_socket + 1, &rfds, NULL, NULL, &tv) == 1)
    {
        char buff[MAX_NL_MSG_LEN];
        int bytes_read = recv(m_netlink_socket, buff, MAX_NL_MSG_LEN - 1, 0);
        buff[bytes_read] = '\0';
		ParseBuffer(buff);
	}
}

#endif // __FreeBSD__

void UnixOpCameraManager::GetCameraDevices()
{
	OpFolderLister* lister = NULL;
	RETURN_VOID_IF_ERROR(OpFolderLister::Create(&lister));
	OpAutoPtr<OpFolderLister> anchor(lister);

	RETURN_VOID_IF_ERROR(lister->Construct(V4L_DEVICES_SEARCH_DIR, DEVICE_V4L_PATTERN));

	while (lister->Next())
	{
		OpString8 node_name;
		if (OpStatus::IsSuccess(node_name.Set(lister->GetFileName())))
		{
			OpStatus::Ignore(AddCamera(node_name.CStr()));
		}
	}
}

OP_STATUS UnixOpCameraManager::GetCameraDevices(OpVector<OpCamera>* camera_devices)
{
	LogInfo("get camera devices");

	camera_devices->Clear();

	for (unsigned i = 0; i < m_cameras.GetCount(); ++i)
	{
		OpCamera* camera = m_cameras.Get(i);
		OpStatus::Ignore(camera_devices->Add(camera));
	}

#ifndef __FreeBSD__
	InitNetlinkListener();
#endif // __FreeBSD__
	return OpStatus::OK;
}

OP_STATUS UnixOpCameraManager::AddCamera(const char* dev_node_name)
{
	OpString8 devpath;
	RETURN_IF_ERROR(devpath.AppendFormat(DEVICE_V4L_PATH_FORMAT, dev_node_name));

	struct stat st; 
	if (stat(devpath.CStr(), &st) == -1 || !S_ISCHR(st.st_mode))
	{
		return OpStatus::ERR;
	}

	UnixOpCamera* camera = OP_NEW(UnixOpCamera, ());
	RETURN_OOM_IF_NULL(camera);
	OpAutoPtr<UnixOpCamera> camera_auto_ptr(camera);

	RETURN_IF_ERROR(camera->Init(devpath));
	RETURN_IF_ERROR(m_cameras.Add(camera));
	camera_auto_ptr.release();

	LogInfo("added camera: %s", devpath.CStr());

	if (m_device_listener)
	{
		m_device_listener->OnCameraAttached(camera);
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpCameraManager::RemoveCamera(const char* dev_node_name)
{
	OpString8 devpath;
	RETURN_IF_ERROR(devpath.AppendFormat(DEVICE_V4L_PATH_FORMAT, dev_node_name));

	for (unsigned i = 0; i < m_cameras.GetCount(); ++i)
	{
		UnixOpCamera* camera = m_cameras.Get(i);
		if (camera->GetDevPath().Compare(devpath) == 0)
		{
			LogInfo("removing camera: %s", devpath.CStr());
			if (m_device_listener)
			{
				m_device_listener->OnCameraDetached(camera);
			}
			m_cameras.Delete(i);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

UnixOpCamera::UnixOpCamera():
	m_best_format(NULL),
	m_bitmap(NULL),
	m_enabled(false),
	m_fd(-1),
	m_size(0),
	m_bpl(0),
	m_width(0),
	m_height(0),
	m_capture_timeout(CAMERA_CAPTURE_TIMEOUT),
	m_num_buffers(DRIVER_BUFFERS_COUNT),
	m_preview_listener(NULL)
{
	m_timer.SetTimerListener(this);
}

UnixOpCamera::~UnixOpCamera()
{
	Disable();
	OP_DELETE(m_bitmap);
}

OP_STATUS UnixOpCamera::Init(OpStringC8 devpath)
{
	RETURN_IF_ERROR(m_devpath.Set(devpath));

	// Formats should be added here in order of preference. That means if for example 
	// format A is registered before format B and camera device supports only A and B, 
	// A will be chosen as the format we want frames in from the camera.

	RegisterFormat<YUYVFrameFormat>();
	RegisterFormat<BA81FrameFormat>();
	RegisterFormat<JPEGFrameFormat>();

	return OpStatus::OK;
}

OP_CAMERA_STATUS UnixOpCamera::Enable(OpCameraPreviewListener* preview_listener)
{
	if (m_enabled)
		return OpStatus::OK;
	
	LogInfo("enable camera");

	RETURN_IF_ERROR(Open());

	bool has_caps = false;
	if (OpStatus::IsError(HasCapabilities(has_caps)) || !has_caps)
	{
		Close();
		return OpStatus::ERR;
	}

	OpAutoVector<v4l2_fmtdesc> fmts;
	if (OpStatus::IsError(GetImageFormats(fmts)) ||
		OpStatus::IsError(SetImageFormat(fmts)))
	{
		Close();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(InitStreamingIO()) ||
		OpStatus::IsError(StartCapture()))
	{
		Close();
		return OpStatus::ERR;
	}
	
	m_enabled          = true;
	m_preview_listener = preview_listener;

	LogInfo("camera enabled");

	return OpStatus::OK;
}

void UnixOpCamera::Disable()
{
	if (!m_enabled)
		return;

	LogInfo("disable camera");

	m_timer.Stop();
	Close();

	m_enabled          = false;
	m_preview_listener = NULL;

	LogInfo("camera disabled");
}

OP_CAMERA_STATUS UnixOpCamera::GetCurrentPreviewFrame(OpBitmap** frame)
{
	if (!m_enabled)
		return OpCameraStatus::ERR_DISABLED;

	v4l2_buffer buf;
	memset(&buf, 0, sizeof(v4l2_buffer));
	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	*frame = m_bitmap;

	if (xioctl(m_fd, VIDIOC_DQBUF, &buf) == -1)
	{
		switch (errno)
		{
			case EAGAIN:
			{
				LogInfo("frame not ready yet");

				v4l2_buffer buf;
				for (unsigned k = 0; k < m_num_buffers; ++k)
				{
					memset(&buf, 0, sizeof(v4l2_buffer));
					buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
					buf.memory = V4L2_MEMORY_MMAP;
					buf.index = k;

					if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) == -1)
					{
						LogError("ioctl() VIDIOC_QUERYBUF: %s", strerror(errno));
						break;
					}

					LogInfo("buffer %d: %s, %s, %s", k, 
						((buf.flags & V4L2_BUF_FLAG_MAPPED) ? 
							"resides in device memory and is mapped" :
							"isn't mapped properly"),
						((buf.flags & V4L2_BUF_FLAG_QUEUED) ?
							"waits in incoming queue" :
							"isn't queued yet, not good"),
						((buf.flags & V4L2_BUF_FLAG_DONE)) ?
							"can be dequeued now" :
							"can't be dequeued currently");
				}

				break;
			}
			default:
				Disable();
				LogError("ioctl() VIDIOC_DQBUF: %s", strerror(errno));
				return OpStatus::ERR;
		}
	}
	else
	{
		if (OpStatus::IsError(m_best_format->ConvertToARGB(m_height, m_bpl, 
					m_buffers[buf.index].ptr, m_bitmap)))
		{
			LogError("converting frame");	
			return OpStatus::ERR;
		}

		if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1)
		{
			LogError("ioctl() VIDIOC_QBUF: %s", strerror(errno));
			return OpStatus::ERR;
		}
	}

	m_timer.Start(m_capture_timeout);

	return OpStatus::OK;
}

OP_CAMERA_STATUS UnixOpCamera::GetPreviewDimensions(Resolution* dimensions)
{
	if (!m_enabled)
		return OpCameraStatus::ERR_DISABLED;

	dimensions->x = m_width;
	dimensions->y = m_height;

	LogInfo("preview dimensions are %dx%d", m_width, m_height);
	
	return OpCameraStatus::OK;
}

void UnixOpCamera::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(&m_timer == timer);
	
	if (m_preview_listener)
		m_preview_listener->OnCameraUpdateFrame(this);
}

OP_STATUS UnixOpCamera::Open()
{
	LogInfo("open camera: %s", m_devpath.CStr());

	if ((m_fd = open(m_devpath.CStr(), O_RDWR|O_NONBLOCK, 0)) == -1) 
	{
		LogError("open(%s): %s", m_devpath.CStr(), strerror(errno));
		return OpStatus::ERR;
	}

	LogInfo("camera opened");

	return OpStatus::OK;
}

void UnixOpCamera::Close()
{
	LogInfo("close camera");

	UnmapBuffers();

	if (close(m_fd) == -1)
		LogError("close(): %s", strerror(errno));
	
	LogInfo("camera closed");
}

void UnixOpCamera::UnmapBuffers()
{
	for (unsigned i = 0; i < m_num_buffers; ++i)
	{
		if (m_buffers[i].ptr && 
			munmap(m_buffers[i].ptr, m_buffers[i].size) == -1)
		{
			LogError("munmap(): %s", strerror(errno));
		}

		m_buffers[i].ptr  = NULL;
		m_buffers[i].size = 0;
	}
}


bool UnixOpCamera::SupportsImageFormat(UINT32 fmt, const OpVector<v4l2_fmtdesc>& devfmts)
{
	for (unsigned i = 0; i < devfmts.GetCount(); ++i)
	{
		if (devfmts.Get(i)->pixelformat == fmt)	
			return true;
	}
	return false;
}

OP_STATUS UnixOpCamera::StartCapture()
{
	for (unsigned i = 0; i < m_num_buffers; ++i)
	{
		if (!m_buffers[i].ptr)
			break;

		v4l2_buffer buf;
		memset(&buf, 0, sizeof(v4l2_buffer));
		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1)
		{
			LogError("ioctl() VIDIOC_QBUF: %s", strerror(errno));
			return OpStatus::ERR;
		}
	}

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(m_fd, VIDIOC_STREAMON, &type) == -1)
	{
		LogError("ioctl() VIDIOC_STREAMON: %s", strerror(errno));
		return OpStatus::ERR;
	}

	m_timer.Start(m_capture_timeout);

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::StopCapture()
{
	m_timer.Stop();

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(m_fd, VIDIOC_STREAMOFF, &type) == -1)
	{
		LogError("ioctl() VIDIOC_STREAMOFF: %s", strerror(errno));
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::InitStreamingIO()
{
	v4l2_requestbuffers req;
	memset(&req, 0, sizeof(v4l2_requestbuffers));
	req.count  = m_num_buffers;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (xioctl(m_fd, VIDIOC_REQBUFS, &req) == -1)
	{
		LogError("ioctl() VIDIOC_REQBUFS: %s", strerror(errno));
		return OpStatus::ERR;
	}

	if (req.count < MIN_BUFFERS_COUNT)
	{
		LogError("insufficient buffers count");
		return OpStatus::ERR;
	}

	if (req.count != m_num_buffers)
	{
		LogInfo("we wanted %d buffers, but only %d available/needed", m_num_buffers, req.count);
		m_num_buffers = req.count;
	}

	LogInfo("number of buffers allocated: %d", m_num_buffers);

	bool was_error = false;
	for (unsigned i = 0; i < req.count; ++i)
	{
		v4l2_buffer buf;
		memset(&buf, 0, sizeof(v4l2_buffer));
		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) == -1)
		{
			LogError("ioctl() VIDIOC_QUERYBUF: %s", strerror(errno));
			was_error = true; break;
		}

		m_buffers[i].size = buf.length;
		m_buffers[i].ptr  = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, 
				MAP_SHARED, m_fd, buf.m.offset);

		if (m_buffers[i].ptr == MAP_FAILED)
		{
			LogError("mmap(): %s", strerror(errno));
			was_error = true; break;
		}
	}

	if (was_error)
	{
		UnmapBuffers();
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::SetImageFormat(const OpVector<v4l2_fmtdesc>& fmts)
{
	v4l2_format fmt;
	memset(&fmt, 0, sizeof(v4l2_format));

	unsigned i = 0;
	for (; i < fmts.GetCount(); ++i)
	{
		LogInfo("device supports format %X: %s", fmts.Get(i)->pixelformat, 
                                                 fmts.Get(i)->description);
	}

	for (i = 0; i < m_supported_fmts.GetCount(); ++i)
	{
		if (SupportsImageFormat(m_supported_fmts.Get(i)->GetPixelFormat(), fmts))
		{
			m_best_format = m_supported_fmts.Get(i);

			LogInfo("format number %X chosen", m_best_format->GetPixelFormat());

			break;
		}
	}

	if (i == m_supported_fmts.GetCount())
	{
		LogError("unsupported camera image formats\n");
		return OpStatus::ERR;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(m_fd, VIDIOC_G_FMT, &fmt) == -1)
	{
		LogError("ioctl() VIDIOC_G_FMT: %s", strerror(errno));
		return OpStatus::ERR;
	}

	LogInfo("VIDIOC_G_FMT");
	LogInfo(" fmt.fmt.pix.pixelformat = %X", fmt.fmt.pix.pixelformat);
	LogInfo(" fmt.fmt.pix.sizeimage = %d", fmt.fmt.pix.sizeimage);
	LogInfo(" fmt.fmt.pix.width = %d", fmt.fmt.pix.width);
	LogInfo(" fmt.fmt.pix.height = %d", fmt.fmt.pix.height);
	LogInfo(" fmt.fmt.pix.bytesperline = %d", fmt.fmt.pix.bytesperline);
	LogInfo(" fmt.fmt.pix.field = %d", fmt.fmt.pix.field);

	if (!FormatValid(fmt))
	{
		fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.pixelformat = m_best_format->GetPixelFormat();
		fmt.fmt.pix.width       = m_best_format->GetPreferredWidth();
		fmt.fmt.pix.height      = m_best_format->GetPreferredHeight();

		if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) == -1)
		{
			LogError("ioctl() VIDIOC_S_FMT: %s", strerror(errno));
			return OpStatus::ERR;
		}

		if (!FormatValid(fmt))
		{
			LogError("can't set chosen format %X", fmt.fmt.pix.pixelformat);
			LogInfo(" fmt.fmt.pix.pixelformat = %X", fmt.fmt.pix.pixelformat);
			LogInfo(" fmt.fmt.pix.sizeimage = %d", fmt.fmt.pix.sizeimage);
			LogInfo(" fmt.fmt.pix.width = %d", fmt.fmt.pix.width);
			LogInfo(" fmt.fmt.pix.height = %d", fmt.fmt.pix.height);
			LogInfo(" fmt.fmt.pix.bytesperline = %d", fmt.fmt.pix.bytesperline);
			LogInfo(" fmt.fmt.pix.field = %d", fmt.fmt.pix.field);
			return OpStatus::ERR;
		}
	}

	m_size   = fmt.fmt.pix.sizeimage;
	m_width  = fmt.fmt.pix.width;
	m_height = fmt.fmt.pix.height;
	m_bpl    = fmt.fmt.pix.bytesperline;

	LogInfo("resolution is %dx%d", m_width, m_height);

	UINT32 numerator = 0, denominator = 0;
	if (OpStatus::IsSuccess(GetTimePerFrame(numerator, denominator)) &&
		numerator != 0 && denominator != 0)
	{
		UINT32 dev_capture_timeout = 0;
		dev_capture_timeout = (numerator * 1000) / denominator;

		LogInfo("current device capture timeout: %d ms", dev_capture_timeout);
		LogInfo("current device frame rate: %.2f fps", 1000.0 / dev_capture_timeout);

		if (dev_capture_timeout < CAMERA_CAPTURE_TIMEOUT)
		{
			OpStatus::Ignore(SetTimePerFrame(1, 1000 / CAMERA_CAPTURE_TIMEOUT));

			if (OpStatus::IsSuccess(GetTimePerFrame(numerator, denominator)))
			{
				unsigned tmp = (numerator * 1000) / denominator;
				LogInfo("after change current device capture timeout: %d ms", tmp);
				LogInfo("after chagne current device frame rate: %.2f fps", 1000.0 / tmp);
			}

			// If we succeeded or not changing the time per frame, in any case we 
			// want to ask about frames with CAMERA_CAPTURE_TIMEOUT timeout, not faster!

			m_capture_timeout = CAMERA_CAPTURE_TIMEOUT;
		}
		else
		{
			// Current time per frame set in the device is ok for us, we switch 
			// to that. So now our capture timeout is >= CAMERA_CAPTURE_TIMEOUT.

			m_capture_timeout = dev_capture_timeout;
		}
	}

	LogInfo("we grab with timeout: %d ms", m_capture_timeout);
	LogInfo("we grab with frame rate: %.2f fps", 1000.0 / m_capture_timeout);

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::GetImageFormats(OpVector<v4l2_fmtdesc>& fmts)
{
	int ret = 0, fmtnum = 0;
	do
	{
		v4l2_fmtdesc* fmt = OP_NEW(v4l2_fmtdesc, ());
		RETURN_OOM_IF_NULL(fmt);
		memset(fmt, 0, sizeof(v4l2_fmtdesc));
		fmt->type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt->index = fmtnum++;

		// The ioctl with VIDIOC_ENUM_FMT should set errno to EINVAL 
		// when there's no format with supplied index (fmt->index).

		if ((ret = xioctl(m_fd, VIDIOC_ENUM_FMT, fmt)) != -1)
		{
			OP_STATUS err = fmts.Add(fmt);
			if (OpStatus::IsError(err))
			{
				OP_DELETE(fmt);
				return err;
			}
		}
		else
		{
			// If the error is different than EINVAL then it's a real error.

			OP_DELETE(fmt);
			if (errno != EINVAL)
			{
				LogError("ioctl() VIDIOC_ENUM_FMT: %s", strerror(errno));
				return OpStatus::ERR;
			}
		}
	}
	while (ret != -1);

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::HasCapabilities(bool& has)
{
	v4l2_capability caps;

	if (xioctl(m_fd, VIDIOC_QUERYCAP, &caps) == -1)
	{
		LogError("ioctl() VIDIOC_QUERYCAP: %s", strerror(errno));
		return OpStatus::ERR;
	}

	has = true;

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		LogInfo("not a capture device");
		has = false;
	}

	if (!(caps.capabilities & V4L2_CAP_STREAMING))
	{
		LogInfo("device doesn't support streaming io");
		has = false;
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::YUYVFrameFormat::ConvertToARGB(unsigned src_h, 
		unsigned src_bpl, const void* src, OpBitmap*& bmp)
{
	// In YUYV format each 4 bytes encode two 4 bytes pixels in an ARGB format.
	// That's why width of the ARGB bitmap should be (src_bpl / 4) * 2.

	if (!bmp)
		RETURN_IF_ERROR(OpBitmap::Create(&bmp, (src_bpl / 4) * 2, src_h));

	const UINT32* srcn = static_cast<const UINT32*>(src);
	UINT32* dstn = static_cast<UINT32*>(bmp->GetPointer());

	int c1, c2, c3, c4, c5, c6, c7, c8, c9, y0, u0, y1, v0; 
	for (unsigned h = 0; h < src_h; ++h)
	{
		for (unsigned i = 0; i < (src_bpl / 4); ++i)
		{
			UINT32 w = *srcn++;

			y0 =  w & 0x000000ff;
			u0 = (w & 0x0000ff00) >> 8;
			y1 = (w & 0x00ff0000) >> 16;
			v0 = (w & 0xff000000) >> 24;

			c1 = y0 - 16;
			c2 = y1 - 16;
			c3 = v0 - 128;
			c4 = u0 - 128;
			c5 = 1.164 * c1;
			c6 = 1.164 * c2;
			c7 = 1.596 * c3;
			c8 = 0.391 * c4 + 0.813 * c3;
			c9 = 2.018 * c4;
			
			PackARGB(dstn++, 0xFF, Clamp(c5 + c7), Clamp(c5 - c8), Clamp(c5 + c9));
			PackARGB(dstn++, 0xFF, Clamp(c6 + c7), Clamp(c6 - c8), Clamp(c6 + c9));
		}
	}

	bmp->ReleasePointer();

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::JPEGFrameFormat::ConvertToARGB(unsigned src_h, 
		unsigned src_bpl, const void* src, OpBitmap*& bmp)
{
	m_bmp = &bmp;

	int resend_bytes = 0;
	ImageDecoderJpg jpgdecoder;
	jpgdecoder.SetImageDecoderListener(this);
	RETURN_IF_ERROR(jpgdecoder.DecodeData(static_cast<const BYTE*>(src), 
	                                      src_h * src_bpl, 
	                                      FALSE, resend_bytes));

	return OpStatus::OK;
}

void UnixOpCamera::JPEGFrameFormat::OnLineDecoded(void* data, INT32 line, 
		INT32 lineHeight)
{
	if (*m_bmp)
	{
		UINT32* imgdata = static_cast<UINT32*>((*m_bmp)->GetPointer());

		for (int i = 0; i < m_width; ++i)
			imgdata[line * m_width + i] = (static_cast<UINT32*>(data))[i];

		(*m_bmp)->ReleasePointer();
	}
}

BOOL UnixOpCamera::JPEGFrameFormat::OnInitMainFrame(INT32 width, INT32 height)
{
	m_width = width;
	m_height = height;

	if (!(*m_bmp))
		RETURN_VALUE_IF_ERROR(OpBitmap::Create(m_bmp, width, height), FALSE);

	return TRUE;
}

void UnixOpCamera::BA81FrameFormat::HandlePixel(const Array& array, UINT32*& dstn, int x, int y)
{
	UINT8 r = 0, g = 0, b = 0;

	// We calculate compensation value which will be used to even out the rounding errors
	UINT8 c = (x ^ y) >> 1 & 1;

	if ((x & 0x00000001) == 0)
	{
		if ((y & 0x00000001) == 0)
		{
			b = array.Get(x, y);
			r = (array.Get(x-1, y-1) + array.Get(x-1, y+1) + array.Get(x+1, y-1) + array.Get(x+1, y+1) + 1 + c) >> 2;
			g = (array.Get(x, y-1) + array.Get(x-1, y) + array.Get(x, y+1) + array.Get(x+1, y) + 2 - c) >> 2;
		}
		else
		{
			g = array.Get(x, y);
			b = (array.Get(x, y-1) + array.Get(x, y+1) + c) >> 1;
			r = (array.Get(x-1, y) + array.Get(x+1, y) + 1 - c) >> 1;
		}
	}
	else
	{
		if ((y & 0x00000001) == 0)
		{
			g = array.Get(x, y);
			r = (array.Get(x, y-1) + array.Get(x, y+1) + c) >> 1;
			b = (array.Get(x-1, y) + array.Get(x+1, y) + 1 - c) >> 1;
		}
		else
		{
			r = array.Get(x, y);
			b = (array.Get(x-1, y-1) + array.Get(x-1, y+1) + array.Get(x+1, y-1) + array.Get(x+1, y+1) + 2 - c) >> 2;
			g = (array.Get(x, y-1) + array.Get(x-1, y) + array.Get(x, y+1) + array.Get(x+1, y) + 1 + c) >> 2;
		}
	}
	PackARGB(dstn++, 0xFF, r, g, b);
}

void UnixOpCamera::BA81FrameFormat::HandleEdgePixel(const Array& array, UINT32*& dstn, int x, int y)
{
	UINT8 r = 0, g = 0, b = 0;

	// We calculate compensation value which will be used to even out the rounding errors
	UINT8 c = (x ^ y) >> 1 & 1;

	if ((x & 0x00000001) == 0)
	{
		if ((y & 0x00000001) == 0)
		{
			b = array.GetEdge(x, y);
			r = (array.GetEdge(x-1, y-1) + array.GetEdge(x-1, y+1) + array.GetEdge(x+1, y-1) + array.GetEdge(x+1, y+1) + 1 + c) >> 2;
			g = (array.GetEdge(x, y-1) + array.GetEdge(x-1, y) + array.GetEdge(x, y+1) + array.GetEdge(x+1, y) + 2 - c) >> 2;
		}
		else
		{
			g = array.GetEdge(x, y);
			b = (array.GetEdge(x, y-1) + array.GetEdge(x, y+1) + c) >> 1;
			r = (array.GetEdge(x-1, y) + array.GetEdge(x+1, y) + 1 - c) >> 1;
		}
	}
	else
	{
		if ((y & 0x00000001) == 0)
		{
			g = array.GetEdge(x, y);
			r = (array.GetEdge(x, y-1) + array.GetEdge(x, y+1) + c) >> 1;
			b = (array.GetEdge(x-1, y) + array.GetEdge(x+1, y) + 1 - c) >> 1;
		}
		else
		{
			r = array.GetEdge(x, y);
			b = (array.GetEdge(x-1, y-1) + array.GetEdge(x-1, y+1) + array.GetEdge(x+1, y-1) + array.GetEdge(x+1, y+1) + 2 - c) >> 2;
			g = (array.GetEdge(x, y-1) + array.GetEdge(x-1, y) + array.GetEdge(x, y+1) + array.GetEdge(x+1, y) + 1 + c) >> 2;
		}
	}
	PackARGB(dstn++, 0xFF, r, g, b);
}

OP_STATUS UnixOpCamera::BA81FrameFormat::ConvertToARGB(unsigned src_h, 
		unsigned src_bpl, const void* src, OpBitmap*& bmp)
{
	if (src_h < 2 || src_bpl < 2)
	{
		return OpStatus::ERR;
	}

	Array array(static_cast<const UINT8*>(src), src_h, src_bpl);

	if (!bmp)
		RETURN_IF_ERROR(OpBitmap::Create(&bmp, src_bpl, src_h));

	UINT32* dstn = static_cast<UINT32*>(bmp->GetPointer());

	// handle first row
	for (UINT32 i = 0; i < src_bpl; ++i)
	{
		HandleEdgePixel(array, dstn, 0, i);
	}

	for (UINT32 h = 1; h < src_h - 1; ++h)
	{
		// handle first column
		HandleEdgePixel(array, dstn, h, 0);

		for (UINT32 i = 1; i < src_bpl - 1; ++i)
		{
			HandlePixel(array, dstn, h, i);
		}

		// handle last column
		HandleEdgePixel(array, dstn, h, src_bpl - 1);
	}

	// handle last row
	for (UINT32 i = 0; i < src_bpl; ++i)
	{
		HandleEdgePixel(array, dstn, src_h - 1, i);
	}

	bmp->ReleasePointer();

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::GetTimePerFrame(UINT32& numerator, UINT32& denominator)
{
	numerator = 0;
	denominator = 0;

	v4l2_streamparm strmprm;
	memset(&strmprm, 0, sizeof(v4l2_streamparm));
	strmprm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(m_fd, VIDIOC_G_PARM, &strmprm) == -1)
	{
		LogError("ioctl() VIDIOC_G_PARM: %s", strerror(errno));		
		return OpStatus::ERR;
	}

	numerator = strmprm.parm.capture.timeperframe.numerator;
	denominator = strmprm.parm.capture.timeperframe.denominator;
	m_can_change_timeperframe = strmprm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME;

	return OpStatus::OK;
}

OP_STATUS UnixOpCamera::SetTimePerFrame(UINT32 numerator, UINT32 denominator)
{
	if (numerator == 0 || denominator == 0)
		return OpStatus::ERR;

	if (!m_can_change_timeperframe)
	{
		LogInfo("can't change time per frame");
		return OpStatus::ERR;
	}

	v4l2_streamparm strmprm;
	memset(&strmprm, 0, sizeof(v4l2_streamparm));

	strmprm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	strmprm.parm.capture.timeperframe.numerator = numerator;
	strmprm.parm.capture.timeperframe.denominator = denominator;

	if (xioctl(m_fd, VIDIOC_S_PARM, &strmprm) == -1)
	{
		LogError("ioctl() VIDIOC_S_PARM: %s", strerror(errno));
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

#endif // PI_CAMERA
