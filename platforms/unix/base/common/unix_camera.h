/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */
#ifndef UNIX_OP_CAMERA_H
#define UNIX_OP_CAMERA_H

#ifdef PI_CAMERA

#include "modules/pi/device_api/OpCamera.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/img/image.h"
#include "platforms/posix/posix_selector.h"

#include <linux/videodev2.h>

class OpBitmap;
class UnixOpCamera;

class UnixOpCameraManager
	: public OpCameraManager
#ifndef __FreeBSD__
	, public PosixSelectListener
#endif // __FreeBSD__
{
public:
#ifndef __FreeBSD__	
	static const int MAX_NL_MSG_LEN = 256;
	static const int NETLINK_MONITOR_KERNEL_GROUP = 1;
#endif // __FreeBSD__

	UnixOpCameraManager();
	~UnixOpCameraManager();

	virtual OP_STATUS GetCameraDevices(OpVector<OpCamera>* camera_devices);
	virtual void SetCameraDeviceListener(OpCameraDeviceListener* listener) { m_device_listener = listener; }

#ifndef __FreeBSD__
	// PosixSelectListener
	virtual void OnReadReady(int fd);
	virtual void OnError(int fd, int err) {}
	virtual void OnDetach(int fd) {}
#endif // __FreeBSD__

private:
	void GetCameraDevices();
	OP_STATUS AddCamera(const char* dev_node_name);
	OP_STATUS RemoveCamera(const char* dev_node_name);

#ifndef __FreeBSD__
	void InitNetlinkListener();
	void ParseBuffer(char* buffer);
	void ParseLine(OpStringC8 line);
#endif // __FreeBSD__

	OpCameraDeviceListener* m_device_listener;
	OpAutoVector<UnixOpCamera> m_cameras;

#ifndef __FreeBSD__
	int m_netlink_socket;
#endif // __FreeBSD__
};

/**
 * \brief Implementation of OpCamera PI for UNIX.
 *
 * This implementation uses video4linux2 API to speak with the camera.
 */
class UnixOpCamera: 
	public OpCamera,
	public OpTimerListener
{
public:

	UnixOpCamera();
	virtual ~UnixOpCamera();

	OP_STATUS Init(OpStringC8 devpath);

	/**
	 * @see OpCamera
	 */
	virtual OP_CAMERA_STATUS Enable(OpCameraPreviewListener* preview_listener);
	virtual void Disable();
	virtual OP_CAMERA_STATUS GetCurrentPreviewFrame(OpBitmap** frame);
	virtual OP_CAMERA_STATUS GetPreviewDimensions(Resolution* dimensions);

	/**
	 * @see OpTimer
	 */
	virtual void OnTimeOut(OpTimer* timer);

	OpStringC8 GetDevPath() const { return m_devpath; }

private:

	/**
	 * \brief Describes capture's device frame format.
	 *
	 * Each capture device can capture frames in different formats. For example we
	 * can get directly a frame in ARGB, or if we're out of luck in JPEG.
	 * CORE expects a standard ARGB format, that's why we need to maintain a list
	 * of formats we are able to convert to ARGB and therefore we support.
	 */
	class FrameFormat
	{
	public:

		virtual ~FrameFormat() {}

		virtual OP_STATUS Init() = 0;

		/**
		 * Return platform dependent pixel format identification code for this format.
		 * This can be code for YUYV or for compressed JPEG as defined by video4linux.
		 */
		virtual UINT32 GetPixelFormat() const = 0;

		/**
		 * Preferred preview width for this frame type.
		 */
		virtual UINT32 GetPreferredWidth() const { return 800; }

		/**
		 * Preferred preview height for this frame type.
		 */
		virtual UINT32 GetPreferredHeight() const { return 600; }

		/**
		 * Converts src image which has format encoded as four byte code e.g. 'YUYV'
		 * to ARGB.
		 *
		 * @param src_h Height of the source image.
		 * @param src_bpl Bytes per line of the source image.
		 * @param src Source image data in defined by driver format.
		 * @param bmp[out] Should contain source image as ARGB bitmap, if bmp is NULL
		 *   function should create in it OpBitmap of appropriate size, else it should
		 *   place the converted image in already created bmp.
		 */
		virtual OP_STATUS ConvertToARGB(unsigned src_h, unsigned src_bpl, const void* src, 
										OpBitmap*& bmp) = 0;

	protected:
		
		static void PackARGB(UINT32* x, UINT8 a, UINT8 r, UINT8 g, UINT8 b)
		{
			*x = (a << 24) | (r << 16) | (g << 8) | b;
		}

		static int Clamp(int x) 
		{ 
			return max(0, min(255, x)); 
		}
	};

	/**
	 * YUYV also known as YUY2 in Windows is a format where each four bytes encode 2 pixels
	 * in ARGB format. From our experiments it looks like this is the most widely supported
	 * format.
	 */
	class YUYVFrameFormat:
		public FrameFormat
	{
	public:

		virtual OP_STATUS Init() { return OpStatus::OK; }
		virtual UINT32 GetPixelFormat() const { return V4L2_PIX_FMT_YUYV; }
		virtual OP_STATUS ConvertToARGB(unsigned src_h, unsigned src_bpl, const void* src, 
										OpBitmap*& bmp);
	};

	/**
	 * Some cameras also support compressed formats like JPEG, I'm using our img module to
	 * deal with that. On *nix there are drivers which the best they can get from the camera
	 * is the JPEG stream, so it's good to support it as well.
	 */
	class JPEGFrameFormat:
		public FrameFormat,
		public ImageDecoderListener
	{
	public:
		
		JPEGFrameFormat():
			m_width(0), 
			m_height(0),
			m_bmp(NULL) {}

		virtual OP_STATUS Init() { return OpStatus::OK; }
		virtual UINT32 GetPixelFormat() const { return V4L2_PIX_FMT_JPEG; }
		virtual OP_STATUS ConvertToARGB(unsigned src_h, unsigned src_bpl, const void* src, 
										OpBitmap*& bmp);
		virtual void OnICCProfileData(const UINT8* data, unsigned datalen) {}

	private:
		
		virtual void OnLineDecoded(void* data, INT32 line, INT32 lineHeight);
		virtual BOOL OnInitMainFrame(INT32 width, INT32 height);
		virtual void OnNewFrame(const ImageFrameData& image_frame_data) {}
		virtual void OnAnimationInfo(INT32 nrOfRepeats) {}
		virtual void OnDecodingFinished() {}
		virtual void OnMetaData(ImageMetaData, const char*) {}

		INT32 m_width, m_height;
		OpBitmap** m_bmp;
	};

	class BA81FrameFormat:
		public FrameFormat
	{
	public:

		virtual OP_STATUS Init() { return OpStatus::OK; }
		virtual UINT32 GetPixelFormat() const { return V4L2_PIX_FMT_SBGGR8; }
		virtual OP_STATUS ConvertToARGB(unsigned src_h, unsigned src_bpl, const void* src, 
										OpBitmap*& bmp);

	private:

		class Array
		{
		public:
			Array(const UINT8* ptr, unsigned max_x, unsigned max_y) :
				m_ptr(ptr), m_max_x(max_x), m_max_y(max_y) {}

			int Get(int x, int y) const
			{
				return m_ptr[x * m_max_y + y];
			}

			int GetEdge(int x, int y) const
			{
				if (x < 0)
					x += 2;
				else if (static_cast<unsigned>(x) >= m_max_x)
					x -= 2;
				if (y < 0)
					y += 2;
				else if (static_cast<unsigned>(y) >= m_max_y)
					y -= 2;
				return Get(x, y);
			}

		private:
			const UINT8* m_ptr;
			const unsigned m_max_x;
			const unsigned m_max_y;
		};

		inline void HandlePixel(const Array& array, UINT32*& dstn, int x, int y);
		inline void HandleEdgePixel(const Array& array, UINT32*& dstn, int x, int y);
	};

	/**
	 * Open camera device.
	 */
	OP_STATUS Open();

	/**
	 * Close camera device and unmap camera memory from user space.
	 */
	void Close();

	/**
	 * Set camera to start capture and start the capture timer.
	 */
	OP_STATUS StartCapture();

	/**
	 * Set camera to stop capture and stop the capture timer.
	 */
	OP_STATUS StopCapture();

	/**
	 * We use streaming io to communicate with the camera, this method maps
	 * driver space buffers into user space buffers.
	 */
	OP_STATUS InitStreamingIO();

	/**
	 * We map camera memory in our user space, this function unmaps this memory.
	 */
	void UnmapBuffers();

	/**
	 * Each supported format is represented by a class with operations supporting
	 * this format. T should be a type of this class. Object of type T will be created
	 * and then used for supporting format which T describes.
	 */
	template <typename T>
	OP_STATUS RegisterFormat()
	{
		T* fmt = OP_NEW(T, ());
		RETURN_OOM_IF_NULL(fmt);

		if (OpStatus::IsError(fmt->Init()) ||
			OpStatus::IsError(m_supported_fmts.Add(fmt)))
		{
			OP_DELETE(fmt);
			return OpStatus::ERR;
		}

		return OpStatus::OK;
	}

	/**
	 * If we know what image formats our capture device supports, we need
	 * to set the preferred format on the device. If none of our preferred
	 * formats is supported we return an error.
	 *
	 * @param[in] fmts List of image formats supported by the device.
	 */
	OP_STATUS SetImageFormat(const OpVector<v4l2_fmtdesc>& fmts);

	/**
	 * Different devices support different image formats, we need to ask 
	 * the device about all the formats it supports and then pick the best 
	 * format from which conversion to ARGB we support.
	 *
	 * @param[out] fmts Filled with image formats supported by the device.
	 */
	OP_STATUS GetImageFormats(OpVector<v4l2_fmtdesc>& fmts);

	/**
	 * Check if video device has capture and streaming capabilities.
	 *
	 * @param[out] has TRUE iff device has all necessary stuff.
	 */
	OP_STATUS HasCapabilities(bool& has);

	/**
	 * Ask driver what is the time interval in [s] between grabbing
	 * frames. The result should be interpreted as (numerator/denominator) [s].
	 */
	OP_STATUS GetTimePerFrame(UINT32& numerator, UINT32& denominator);

	/**
	 * If the frame rate is too high (for example 40 fps), we want to change it
	 * to something lower (default is 25 fps) to not eat too much CPU time.
	 * Drivers may support frame rates changing, but there are drivers that
	 * don't support it and also drivers that support it, but even if we try to
	 * change it to something ours the driver may discard the change, because
	 * there's no mode in the driver with desired frame rate.
	 */
	OP_STATUS SetTimePerFrame(UINT32 numerator, UINT32 denominator);

	/**
	 * Check if camera device supports format described by four byte code.
	 * For example the code for YUYV 4:2:2 is v4l2_fourcc('Y', 'U', 'Y', 'V').
	 *
	 * @param fmt Four byte code of format we want to check for support.
	 * @param devfmts Descriptions of formats supported by the device.
	 */
	bool SupportsImageFormat(UINT32 fmt, const OpVector<v4l2_fmtdesc>& devfmts);

	/**
	 * Check if format returned by VIDIOC_G_FMT query has valid values.
	 * FIXME: I'm not sure, but I think some drivers can return bytesperline
	 * equal to 0, probably when compressed formats come into play. This needs
	 * further investigation.
	 */
	bool FormatValid(v4l2_format& fmt)
	{ 
		return fmt.fmt.pix.sizeimage && fmt.fmt.pix.width &&
			   fmt.fmt.pix.height && fmt.fmt.pix.bytesperline &&
			   fmt.fmt.pix.pixelformat == m_best_format->GetPixelFormat();
	}

	/**
	 * We request number of buffers in the driver to which the device
	 * captures frames. Buffers wander between an incoming and outgoing
	 * queues, they can be queued and dequeued.
	 */
	static const UINT32 DRIVER_BUFFERS_COUNT = 4;
	static const UINT32 MIN_BUFFERS_COUNT = 1;

	struct Buffer
	{
		Buffer():
			ptr(NULL), size(0) {}

		void*  ptr;
		size_t size;
	} m_buffers[DRIVER_BUFFERS_COUNT]; /**< Device buffers are mapped here. */

	FrameFormat*              m_best_format;    /**< Format that was choosen for device. */
	OpAutoVector<FrameFormat> m_supported_fmts; /**< Information about supported formats. */ 

	OpString8 m_devpath;         /**< Device path, for example /dev/video0. */
	OpBitmap* m_bitmap;          /**< Bitmap for camera frame. */
	OpTimer   m_timer;           /**< Capture timer. */
	bool      m_enabled;         /**< TRUE iff camera is enabled. */
	int       m_fd;              /**< File descriptor for camera device. */
	unsigned  m_size, m_bpl;     /**< Frame size and bytes per line. */
	unsigned  m_width, m_height; /**< Frame resolution. */
	unsigned  m_capture_timeout; /**< Camera capture timeout in ms. */
	unsigned  m_num_buffers;	 /**< Number of buffers allocated, can change. */
	bool	  m_can_change_timeperframe;

	OpCameraPreviewListener* m_preview_listener;
};

#endif // PI_CAMERA
#endif // !UNIX_OP_CAMERA_H
