/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef PI_CAMERA

// no need to update any project file
#pragma comment(lib, "vfw32.lib")

#include <Vfw.h>
#include <dbt.h>

#define OUR_GUID_ENTRY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } };
//Just like DEFINE_GUID with INITGUID defined
//Needed for IAMPluginControl calls

#include <uuids.h>
#undef OUR_GUID_ENTRY

#include "WindowsOpCamera.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "platforms/windows/pi/WindowsOpCamera_DirectShow.h"
#include "modules/console/opconsoleengine.h"
#include "modules/img/src/imagedecoderjpg.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/pi/OpBitmap.h"

#ifdef WINDOWS_CAMERA_GRAPH_TEST
#include "platforms/windows/prefs/PrefsCollectionMSWIN.h"
#endif

//why Vega #defines/#undefs this in every .cpp is beyond me...
//when the owner of Vega moves this to header, remove this and corresponding
//#undef and use the one gfx people maintain.
#define VEGA_CLAMP_U8(v) (((v) <= 255) ? (((v) >= 0) ? (v) : 0) : 255)

/* static */
OP_STATUS OpCameraManager::Create(OpCameraManager** new_camera_manager)
{
	OP_ASSERT(new_camera_manager != NULL);
	*new_camera_manager = OP_NEW(WindowsOpCameraManager, ());
	if(*new_camera_manager == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

extern HINSTANCE hInst; //from opera.cpp, this process HINSTANCE

// Error code translation for moving through adapter

static OP_CAMERA_STATUS HR2CAM(HRESULT hr)
{
	if (hr == E_OUTOFMEMORY)
		return OpCameraStatus::ERR_NO_MEMORY;
	if (hr == VFW_E_CANNOT_CONNECT)
		return OpCameraStatus::ERR_NOT_AVAILABLE;
	if (FAILED(hr))
		return OpCameraStatus::ERR;

	return OpCameraStatus::OK;
};

static OP_CAMERA_STATUS CAM2HR(OP_CAMERA_STATUS status)
{
	if (status == OpCameraStatus::ERR_NO_MEMORY)
		return E_OUTOFMEMORY;
	if (status == OpCameraStatus::ERR_NOT_AVAILABLE)
		return VFW_E_CANNOT_CONNECT;
	if (OpStatus::IsError(status))
		return E_FAIL;

	return S_OK;
};

#define METHOD_CALL //OutputDebugStringA( __FUNCTION__ "\n" )

WindowsOpCameraManager::WindowsOpCameraManager()
	: m_notify_sink(NULL)
	, m_pnp_notify(NULL)
	, m_device_listener(NULL)
{
	static ATOM notify_class = 0;

	if (!notify_class)
	{
		WNDCLASS wc = {};
		wc.lpfnWndProc = NotifyProc;
		wc.hInstance = hInst;
		wc.lpszClassName = L"CameraNotifyCallback";
		notify_class = RegisterClass(&wc);
	}
	m_notify_sink = CreateWindowEx(0, MAKEINTATOM(notify_class), L"Opera", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
}

WindowsOpCameraManager::~WindowsOpCameraManager()
{
	if (IsWindow(m_notify_sink))
		SendMessage(m_notify_sink, WM_CLOSE, 0, 0);
}

OP_STATUS WindowsOpCameraManager::GetCameraDevices(OpVector<OpCamera>* camera_devices)
{
	UINT32 count = m_cameras.GetCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		WindowsOpCamera* cam = m_cameras.Get(i);
		if (cam && cam->IsConnected())
			camera_devices->Add(cam);
	}

	return OpStatus::OK;
}

void WindowsOpCameraManager::_InitializePnP(HWND hWnd)
{
	UpdatePnPList(m_cameras);

	OP_ASSERT(m_pnp_notify == NULL);

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

	ZeroMemory( &NotificationFilter, sizeof(NotificationFilter) );
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

	m_pnp_notify =
		RegisterDeviceNotification(hWnd, &NotificationFilter,
		DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);

#ifdef WINDOWS_CAMERA_GRAPH_TEST
	if (g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::BanDefaultDirectShowRenderers) != FALSE)
#else
	//Always disable the renderers
#endif
	{
		// Ban all the system-provided renderers. If we don't do this
		// then the "magical" and "intelligent" IGraphBuilder::Render
		// used inside OpDirectShowCamera::CreateGraph will create,
		// in some bizzare filters configurations[1], a graph with
		// Opera Sink not connected, and VMR (or similar) present
		// and connected. This in turn will create separate window
		// (HWND) with the output, as a result of IGraphBuilder::Run call.
		//
		// There is a problem here, as this calls are proccess-wide
		// and can produce unexpected results in some other parts of
		// the code.
		//
		// On the other hand, we render the video with gstream and here
		// we need the video from camera rendered onto OpBitmap and not
		// DirectX surface. Assumption here is, if we ever wanted
		// DirectShow again, we would use our own sink anyway.
		//
		// [1] Here, "bizzare" configuration is one, where cross section
		// of filter set on output pin of camera and filter set on input
		// ping of Opera Sink is empty.

		OpComPtr<IAMPluginControl> control;
		HRESULT hr = control.CoCreateInstance(CLSID_DirectShowPluginControl);
		if (SUCCEEDED(hr))
		{
			control->SetDisabled(CLSID_VideoMixingRenderer, TRUE);
			control->SetDisabled(CLSID_VideoMixingRenderer9, TRUE);
			control->SetDisabled(CLSID_VideoRendererDefault, TRUE);
			control->SetDisabled(CLSID_EnhancedVideoRenderer, TRUE);
			control->SetDisabled(CLSID_VideoRenderer, TRUE);
		}
	}
}

void WindowsOpCameraManager::_UninitializePnP()
{
	if (m_pnp_notify)
		UnregisterDeviceNotification(m_pnp_notify);

	m_pnp_notify = NULL;
}

void WindowsOpCameraManager::_OnDeviceChange(WPARAM wParam, LPARAM lParam)
{
	// Note to the future maintainer:
	//
	// Half of the notifications use DEV_BROADCAST_HEADER structure;
	// the other half has the lParam set to zero. If you ever expand
	// the switch to inlude something like DBT_QUERYCHANGECONFIG, be
	// sure to reflect that on the condition below: either change it,
	// or move the dev_iface test into the switch.

	PDEV_BROADCAST_DEVICEINTERFACE dev_iface = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(lParam);

	if (!dev_iface || dev_iface->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
		return;

	WindowsOpCamera* cam = NULL;

	switch (wParam)
	{
	case DBT_DEVICEARRIVAL:
	case DBT_DEVICEREMOVECOMPLETE:
		UpdatePnPList(m_cameras);
		cam = FindPnPCamera(dev_iface->dbcc_name, m_cameras);

		if (cam)
		{
			if (wParam == DBT_DEVICEARRIVAL && cam->IsConnected())
				OnCameraAdded(cam);
			else if (!cam->IsConnected())
				OnCameraRemoved(cam);
		}

		return;
	};
}

void WindowsOpCameraManager::OnCameraAdded(WindowsOpCamera* nfo)
{
#if 0
	// Used rarely, not even in the debug, but will be handy,
	// once Core supports camera hotplugging. After that can be removed
	UINT32 count = m_cameras.GetCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		WindowsOpCamera* cam = m_cameras.Get(i);
		wchar_t buffer[1024] = L"";
		if (cam)
		{
			wchar_t c = L' ';
			if (nfo && *cam == *nfo) c = L'>';

			if (cam->GetPath().CStr())
				swprintf_s(buffer, L"%c %6d %s [%s]\n", c, cam->GetDevice(), cam->GetName().CStr(), cam->GetPath().CStr());
			else
				swprintf_s(buffer, L"%c %6d %s [-]\n", c, cam->GetDevice(), cam->GetName().CStr());
		}
		else swprintf_s(buffer, L"  ------ -\n");

		OutputDebugString(buffer);
	}
#endif

	if (nfo && m_device_listener)
	{
		m_device_listener->OnCameraAttached(nfo);
	}
}

void WindowsOpCameraManager::OnCameraRemoved(WindowsOpCamera* nfo)
{
#if 0
	// Used rarely, not even in the debug, but will be handy,
	// once Core supports camera hotplugging. After that can be removed
	UINT32 count = m_cameras.GetCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		WindowsOpCamera* cam = m_cameras.Get(i);
		wchar_t buffer[1024] = L"";
		if (cam)
		{
			wchar_t c = L' ';
			if (nfo && *cam == *nfo) c = L'<';

			if (cam->GetPath().CStr())
				swprintf_s(buffer, L"%c %6d %s [%s]\n", c, cam->GetDevice(), cam->GetName().CStr(), cam->GetPath().CStr());
			else
				swprintf_s(buffer, L"%c %6d %s [-]\n", c, cam->GetDevice(), cam->GetName().CStr());
		}
		else swprintf_s(buffer, L"  ------ -\n");

		OutputDebugString(buffer);
	}
#endif

	if (nfo && m_device_listener)
	{
		m_device_listener->OnCameraDetached(nfo);
	}
}

// The compare function below is enhanced to favour USB devices
// over other types of devices. Once we have an infrastucture
// allowing users to knowingly choose a device, those enhancements
// should be removed and the function should back down to device id
// comparison.

// --- ENHANCEMENT ----
static bool CameraIsUSBDevice(const WindowsOpCamera* cam)
{
	static const uni_char usb_path[] = L"\\\\?\\USB#";
	return cam->GetPath().Compare(usb_path, ARRAY_SIZE(usb_path)-1) == 0;
}
// --- /ENHANCEMENT ----

static int CameraOrder(const WindowsOpCamera** left, const WindowsOpCamera** right)
{
	if (!left)
		return right ? -1 : 0;
	if (!right)
		return 1;

	if (!*left)
		return *right ? -1 : 0;
	if (!*right)
		return 1;

	const WindowsOpCamera* _left = *left;
	const WindowsOpCamera* _right = *right;

// --- ENHANCEMENT ----
	// if one (and only one) camera is an USB device
	// then it wins compared to any PCI and otherwise
	// attached device
	bool _left_is_usb = CameraIsUSBDevice(_left);
	bool _right_is_usb = CameraIsUSBDevice(_right);

	if (_left_is_usb != _right_is_usb)
	{
		//non-USB is greater than USB, so that USB is sorted at the begining
		return _left_is_usb ? -1 : 1;
	}
// --- /ENHANCEMENT ----

	return _left->GetDevice() - _right->GetDevice();
}

void WindowsOpCameraManager::UpdatePnPList(OpVector<WindowsOpCamera>& cams)
{
	DWORD device = 0;
	UINT32 count = cams.GetCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		WindowsOpCamera* cam = cams.Get(i);
		if (cam)
			cam->InvalidateDevice();
	}

	OpComPtr<ICreateDevEnum> cde;
	if (SUCCEEDED(cde.CoCreateInstance(CLSID_SystemDeviceEnum)))
	{
		OpComPtr<IEnumMoniker> mons;
		OpComPtr<IMoniker> mon;
		if (SUCCEEDED(cde->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &mons, 0)) && mons)
		{
			mons->Reset();
			ULONG fetched;
			HRESULT hr;

			while(hr = mons->Next(1, &mon, &fetched), hr==S_OK && fetched == 1)
			{
				OpComPtr<IPropertyBag> bag;
				if (FAILED(mon->BindToStorage(NULL, NULL, IID_IPropertyBag, (void **)&bag)))
				{
					mon = NULL;
					device++;
					continue;
				}

				bool use_name = false;
				OpString data;
				OpComVariant var;
				hr = bag->Read(L"DevicePath", &var, NULL);
				if (FAILED(hr))
				{
					hr = bag->Read(L"FriendlyName", &var, NULL);
					use_name = true;
				}

				if (SUCCEEDED(hr))
					hr = var.ChangeType(VT_BSTR);

				if (SUCCEEDED(hr))
				{
					OpStatus::Ignore(data.Set(V_BSTR(&var)));
					if (!use_name)
						data.MakeUpper();
					var.Clear();
				}

				WindowsOpCamera* found = NULL;
				if (use_name)
				{
					for (UINT32 i = 0; i < count; ++i)
					{
						WindowsOpCamera* cam = cams.Get(i);
						if (cam &&
							cam->GetPath().Compare(OpStringC()) == 0 &&
							cam->GetName().Compare(data) == 0)
						{
							found = cam;
							break;
						}
					}
				}
				else
				{
					for (UINT32 i = 0; i < count; ++i)
					{
						WindowsOpCamera* cam = cams.Get(i);
						if (cam && cam->GetPath().Compare(data) == 0)
						{
							found = cam;
							break;
						}
					}
				}

				if (found)
				{
					found->ValidateDevice(device);
					mon = NULL;
					device++;
					continue;
				}

				OpAutoPtr<WindowsOpCamera> cam = OP_NEW(WindowsOpCamera, (bag, device));

				if (OpStatus::IsSuccess(cams.Add(cam.get())))
					cam.release();

				mon = NULL;
				device++;
			}
		}
	}

	// Sort cameras based on m_device
	// As a side effect, objects with camera device id
	// 0xFFFFFFFF (representing devices once seen, but
	// now detached) will be sorted at the end 
	cams.QSort(CameraOrder);
}

inline WindowsOpCamera* WindowsOpCameraManager::FindPnPCamera(const wchar_t* path, const OpVector<WindowsOpCamera>& cams)
{
	OpString key;
	if (OpStatus::IsError(key.Set(path)))
		return NULL;

	key.MakeUpper();

	UINT32 count = cams.GetCount();
	for (UINT32 i = 0; i < count; i++)
	{
		WindowsOpCamera* cam = cams.Get(i);
		if (cam && cam->GetPath().Compare(key) == 0)
			return cam;
	}

	return NULL;
}

LRESULT CALLBACK WindowsOpCameraManager::NotifyProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		// g_op_camera_manager is still NULL here
		PostMessage(hWnd, DirectShow::WMU_PNP_INIT, 0, 0);
		return 0;

	case DirectShow::WMU_PNP_INIT:
		WindowsOpCameraManager::InitializePnP(hWnd);
		return 0;

	case DirectShow::WMU_UPDATE:
		WindowsOpCamera::OnCameraUpdateFrame(lParam);
		return 0;

	case DirectShow::WMU_RESIZE:
		WindowsOpCamera::OnCameraFrameResize(lParam);
		return 0;

	case DirectShow::WMU_MEDIA_EVENT:
		WindowsOpCamera::OnMediaEvent(lParam);
		return 0;

	case WM_DESTROY:
		WindowsOpCameraManager::UninitializePnP();
		return 0;

	case WM_DEVICECHANGE:
		WindowsOpCameraManager::OnDeviceChange(wParam, lParam);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	};
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void WindowsOpCamera::GetProperty(IPropertyBag* bag, LPCWSTR propname, OpString& prop)
{
	prop.Empty();

	OpComVariant var;

	if (FAILED(bag->Read(propname, &var, NULL)))
		return;
	if (FAILED(var.ChangeType(VT_BSTR)))
		return;

	prop.Append(V_BSTR(&var));
}

WindowsOpCamera::WindowsOpCamera(IPropertyBag* bag, DWORD device)
	: m_preview_listener(NULL)
	, m_device(device)
{
	GetProperty(bag, L"FriendlyName", m_name);
	GetProperty(bag, L"Description", m_descr);
	GetProperty(bag, L"DevicePath", m_path);
	m_path.MakeUpper();
}

WindowsOpCamera::~WindowsOpCamera()
{
}

void WindowsOpCamera::_OnMediaEvent()
{
	long code = 0;
	LONG_PTR p1 = 0, p2 = 0;

	if (!m_event)
		return;

	while (SUCCEEDED(m_event->GetEvent(&code, &p1, &p2, 0)))
	{
#if 0
		// Used rarely, not even in the debug, but will be handy,
		// once Core supports camera hotplugging. After that we might
		// consider removing media event support entirely.
		char buffer[1024];
		switch (code)
		{
		case EC_CLOCK_CHANGED:
			strcpy_s(buffer, "WMU_MEDIA_EVENT: EC_CLOCK_CHANGED\n");
			break;
		case EC_DEVICE_LOST:
			{
				IUnknown* unk = reinterpret_cast<IUnknown*>(p1);
				OpComQIPtr<IBaseFilter> filter(unk);
				if (filter)
				{
					FILTER_INFO info = {};
					if (SUCCEEDED(filter->QueryFilterInfo(&info)))
					{
						if (info.pGraph)
							info.pGraph->Release();

						sprintf_s(buffer, "WMU_MEDIA_EVENT: EC_DEVICE_LOST, 0x%016x '%S' %s\n", p1, info.achName, p2 ? "available" : "removed");
						break;
					}
				}

				sprintf_s(buffer, "WMU_MEDIA_EVENT: EC_DEVICE_LOST, 0x%016x %s\n", p1, p2 ? "available" : "removed");
			}
			break;
		default:
			sprintf_s(buffer, "WMU_MEDIA_EVENT: 0x%08x, 0x%016x 0x%016x\n", code, p1, p2);
		};
		OutputDebugStringA(buffer);
#endif

		m_event->FreeEventParams(code, p1, p2);
	}
}

// OpCamera
OP_CAMERA_STATUS WindowsOpCamera::Enable(OpCameraPreviewListener* preview_listener)
{
	METHOD_CALL;
	if (!IsConnected())
		return OpCameraStatus::ERR_NOT_AVAILABLE;

	if (IsEnabled())
		Disable();

	//WindowsOpCameraManager::SetListener(this);

#ifdef OPERA_CONSOLE
	Str::LocaleString string_id = m_name.IsEmpty() ? Str::S_STARTED_UNNAMED_CAMERA : Str::S_STARTED_CAMERA;

	OpConsoleEngine::Message msg(OpConsoleEngine::Internal, OpConsoleEngine::Information);
	OP_STATUS status = I18n::Format(msg.context, string_id, m_name.CStr());

	if (OpStatus::IsSuccess(status))
	{
		TRAPD(rc, g_console->PostMessageL(&msg));
	}
#endif

	m_preview_listener = preview_listener;

	OpComPtr<DirectShow::ICamera> camera;
	HRESULT hr = OpComObject<DirectShow::impl::OpDirectShowCamera>::Create(&camera);
	if (FAILED(hr))
		return HR2CAM(hr);

	hr = camera->CreateGraph(this, m_device, &m_control);
	if (FAILED(hr))
		return HR2CAM(hr);

	if (!m_control)
		return OpCameraStatus::ERR;

	hr = m_control->QueryInterface(&m_event);
	if (FAILED(hr))
		return HR2CAM(hr);

	if (!m_event)
		return OpCameraStatus::ERR;

	m_event->SetNotifyWindow(
		reinterpret_cast<OAHWND>(WindowsOpCameraManager::GetSyncWindow()),
		DirectShow::WMU_MEDIA_EVENT,
		reinterpret_cast<LONG_PTR>(this)
		);

	long flags = 0;
	if (SUCCEEDED(m_event->GetNotifyFlags(&flags)) && (flags & AM_MEDIAEVENT_NONOTIFY))
		m_event->SetNotifyFlags(flags & ~AM_MEDIAEVENT_NONOTIFY);

	m_camera = camera;

	//dump the graph
#ifdef DEBUG_DSHOW_GRAPH_DUMP
	OpComPtr<IGraphBuilder> gb;
	OLE(m_control.QueryInterface(&gb));
	OLE(DirectShow::debug::DumpGraph(gb));
#endif

	hr = m_control->Run();
	if (FAILED(hr))
	{
		Disable();
		return HR2CAM(hr);
	}

	return OpCameraStatus::OK;
}

void WindowsOpCamera::Disable()
{
	METHOD_CALL;
	m_preview_listener = NULL;
	m_camera = NULL;

	if (m_control)
		m_control->Stop();

	m_control = NULL;

	if (m_event)
		m_event->SetNotifyWindow(0, 0, 0);

	m_event = NULL;
}

OP_CAMERA_STATUS WindowsOpCamera::GetCurrentPreviewFrame(OpBitmap** frame)
{
	if (!IsConnected())
		return OpCameraStatus::ERR_NOT_AVAILABLE;
	if (!IsEnabled())
		return OpCameraStatus::ERR_DISABLED;

	return HR2CAM(m_camera->GetCurrentPreviewFrame(frame));
}

OP_CAMERA_STATUS WindowsOpCamera::GetPreviewDimensions(Resolution* dimensions)
{
	if (!IsEnabled())
		return OpCameraStatus::ERR_DISABLED;

	return HR2CAM(m_camera->GetPreviewDimensions(dimensions));
}

namespace DirectShow
{
	const GUID MEDIASUBTYPE_4CC_I420 = { 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };

	namespace trans
	{
		//3 bytes per pixel, 1 pixel per sample
		class OpTransformRGB24: public OpTransform< OpTransformRGB24 >
		{
		public:
			static long Stride(long width) { return BitmapStride(width * 3); }
			static size_t LineOffset(long height, long y, long stride) { return (height - y - 1) * stride; }

			static void TransformLine(long width, unsigned char * dst, const unsigned char * src)
			{
				for (long x = 0; x < width; ++x)
				{
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = 0xFF;
				}
			}

			static bool Supported(REFIID mediasubtype)
			{
				return mediasubtype == MEDIASUBTYPE_RGB24 || 0; //there was a performance warning of int->bool conversion
			}
		};

		//4 bytes per pixel, 1 pixel per sample
		class OpTransformRGB32: public OpTransform< OpTransformRGB32 >
		{
		public:
			static long Stride(long width) { return BitmapStride(width * 4); }
			static size_t LineOffset(long height, long y, long stride) { return (height - y - 1) * stride; }

			static void TransformLine(long width, unsigned char * dst, const unsigned char * src)
			{
				op_memcpy(dst, src, width * 4);
			}

			static bool Supported(REFIID mediasubtype)
			{
				return mediasubtype == MEDIASUBTYPE_RGB32 || 0;
			}
		};

		// 2 bytes per pixel, 2 pixels per sample
		class OpTransformYUY2: public OpTransform< OpTransformYUY2 >
		{
		public:
			static long Stride(long width) { return BitmapStride(width * 2); }
			static size_t LineOffset(long, long y, long stride) { return y * stride; }
			static void TransformLine(long width, unsigned char * dst, const unsigned char * src)
			{
				for (int x = 0; x < width; x += 2)
				{
					int Y1, U, Y2, V, UV;
					Y1 = *src++;
					U  = *src++;
					Y2 = *src++;
					V  = *src++;

					Y1 -= 16;
					Y2 -= 16;
					U  -= 128;
					V  -= 128;

					Y1 = 1164 * Y1;
					Y2 = 1164 * Y2;
					UV =  391 * U + 813 * V;

					V = 1596 * V;
					U = 2018 * U;

					*dst++ = VEGA_CLAMP_U8((Y1 + U)/1000);
					*dst++ = VEGA_CLAMP_U8((Y1 - UV)/1000);
					*dst++ = VEGA_CLAMP_U8((Y1 + V)/1000);
					*dst++ = 0xFF;

					*dst++ = VEGA_CLAMP_U8((Y2 + U)/1000);
					*dst++ = VEGA_CLAMP_U8((Y2 - UV)/1000);
					*dst++ = VEGA_CLAMP_U8((Y2 + V)/1000);
					*dst++ = 0xFF;
				}
			}

			static bool Supported(REFIID mediasubtype)
			{
				return mediasubtype == MEDIASUBTYPE_YUY2 ||
					mediasubtype == MEDIASUBTYPE_YUYV;
			}
		};

		//planar, Y plane followed by U and V planes, Y size is width x height, U and V size quarter that
		class OpTransformI420
		{
		public:
			static long BitmapStride(long raw_bytes_per_line) { return ((raw_bytes_per_line + 3) >> 2) << 2; }

			static void TransformLine(long width, unsigned char * dst, const unsigned char * luma, const unsigned char* chroma_u, const unsigned char* chroma_v)
			{
				for (int x = 0; x < width; x += 2)
				{
					int Y1, U, Y2, V, UV;
					Y1 = *luma++;
					Y2 = *luma++;

					U  = *chroma_u++;
					V  = *chroma_v++;

					Y1 -= 16;
					Y2 -= 16;
					U  -= 128;
					V  -= 128;

					Y1 = 1164 * Y1;
					Y2 = 1164 * Y2;
					UV =  391 * U + 813 * V;

					V = 1596 * V;
					U = 2018 * U;

					*dst++ = VEGA_CLAMP_U8((Y1 + U)/1000);
					*dst++ = VEGA_CLAMP_U8((Y1 - UV)/1000);
					*dst++ = VEGA_CLAMP_U8((Y1 + V)/1000);
					*dst++ = 0xFF;

					*dst++ = VEGA_CLAMP_U8((Y2 + U)/1000);
					*dst++ = VEGA_CLAMP_U8((Y2 - UV)/1000);
					*dst++ = VEGA_CLAMP_U8((Y2 + V)/1000);
					*dst++ = 0xFF;
				}
			}

			static HRESULT Transform(const unsigned char* src_data, long /* src_bytes */, OpBitmap *frame, long width, long height)
			{
				//This function will be called only, if OpBitmap supports pointers.
				//If current implementation of OpBitmap does not support pointers,
				//the OpDirectShowCamera::SetMediaType will start asserting and
				//OpDirectShowCamera::Receive will not call any transforms...

				unsigned char * dst_data = reinterpret_cast<unsigned char*>(frame->GetPointer());
				unsigned long dst_stride = width * 4; // ABGR32

				long luma_stride = width;
				long chroma_stride = width / 2;
				const unsigned char* src_chroma_u = src_data + luma_stride * height;
				const unsigned char* src_chroma_v = src_chroma_u + chroma_stride * height/2;

				for (long y = 0; y < height; ++y)
				{
					unsigned char * dst = dst_data + y * dst_stride;
					const unsigned char * luma = src_data + y * luma_stride;

					//this will take every chroma line twice, once for y mod 2 == 0, second for y mod 2 == 1
					//which is exactly, what is needed for this planar sampling
					const unsigned char * chroma_u = src_chroma_u + (y/2) * chroma_stride;
					const unsigned char * chroma_v = src_chroma_v + (y/2) * chroma_stride;

					TransformLine(width, dst, luma, chroma_u, chroma_v);
				}

				frame->ReleasePointer();

				return S_OK;
			}

			static bool Supported(REFIID mediasubtype)
			{
				return mediasubtype == MEDIASUBTYPE_4CC_I420 || 
					mediasubtype == MEDIASUBTYPE_IYUV;
			}
		};

		//intra-jpeg encoding, using core 
		class OpTransformJPEG
		{
			class JpegTransform: public ImageDecoderListener
			{
				OpBitmap *m_frame;
				unsigned char * dst_data;
				long m_width;
				long m_height;
				long m_outwidth;
			public:
				JpegTransform(OpBitmap *frame, long width, long height)
					: m_frame(frame)
					, m_width(width)
					, m_height(height)
					, m_outwidth(width)
					, dst_data(NULL)
				{
					//here, only OpBitmaps supporting pointers are allowed
					dst_data = reinterpret_cast<unsigned char*>(m_frame->GetPointer());
				}

				~JpegTransform()
				{
					if (dst_data)
						m_frame->ReleasePointer();
				}

				void OnLineDecoded(void* data, INT32 line, INT32 lineHeight)
				{
					if (!dst_data)
						return;
					op_memcpy(dst_data + line * m_width * 4, data, m_outwidth * 4);
				}

				BOOL OnInitMainFrame(INT32 width, INT32 height)
				{
					BOOL ret = width == m_width && height == m_height;
					if (m_outwidth > width)
						m_outwidth = width;

					return ret;
				}

				void OnNewFrame(const ImageFrameData& image_frame_data) {}
				void OnAnimationInfo(INT32 nrOfRepeats) {}
				void OnDecodingFinished() {}
				void OnMetaData(ImageMetaData, const char*) {}
				virtual void OnICCProfileData(const UINT8* data, unsigned datalen) {}
			};
		public:
			static HRESULT Transform(const unsigned char* src_data, long src_bytes, OpBitmap *frame, long width, long height)
			{
				int resend_bytes = 0;
				ImageDecoderJpg jpgdecoder;
				JpegTransform listener(frame, width, height);
				jpgdecoder.SetImageDecoderListener(&listener);

				return CAM2HR(jpgdecoder.DecodeData(src_data, src_bytes, FALSE, resend_bytes));
			}

			static bool Supported(REFIID mediasubtype)
			{
				return mediasubtype == MEDIASUBTYPE_MJPG || 0;
			}
		};

		//contents must be in sync with coll::OpInputMediaTypes::formats below
		static FormVtbl _forms[] = {
			{ OpTransformRGB24::Supported, OpTransformRGB24::Transform },
			{ OpTransformRGB32::Supported, OpTransformRGB32::Transform },
			{ OpTransformYUY2::Supported,  OpTransformYUY2::Transform },
			{ OpTransformJPEG::Supported,  OpTransformJPEG::Transform },
			{ OpTransformI420::Supported,  OpTransformI420::Transform }
		};

		const FormVtbl* forms = _forms;
		const size_t form_count = ARRAY_SIZE(_forms);
	}

	namespace coll
	{
		// contents must be in sync with contents of trans::_forms above
		// notable difference: YUYV missing here, while it is supported by OpXFormYUY2
		//                     this happens, because the documentation for YUY2 and YUYV
		//                     says they are the same, but I couldn't find any camera
		//                     with YUYV to actually check it.
		//
		//                    Exactly the same goes for I420 and IYUV.
		const MediaFormat OpInputMediaTypes::formats[] =
		{
			MediaFormat( MEDIASUBTYPE_RGB24,    24 ),
			MediaFormat( MEDIASUBTYPE_RGB32,    32 ),
			MediaFormat( MEDIASUBTYPE_YUY2,     16, 0x32595559 ),
			MediaFormat( MEDIASUBTYPE_MJPG,     24, 0x47504a4d ),
			MediaFormat( MEDIASUBTYPE_4CC_I420, 12, 0x30323449 ), //media subtype not defined in uuids.h
		};
		const size_t OpInputMediaTypes::format_count = ARRAY_SIZE(OpInputMediaTypes::formats);

		ULONG OpInputMediaTypes::CollectionSize() const
		{
#ifdef WINDOWS_CAMERA_GRAPH_TEST
			if (m_prefs_forced_format)
				return sizes.GetCount();
#endif
			return sizes.GetCount() * format_count;
		}

		HRESULT OpInputMediaTypes::CopyItem(ULONG pos, AM_MEDIA_TYPE** ppOut)
		{
			size_t s_count = sizes.GetCount();
#ifdef WINDOWS_CAMERA_GRAPH_TEST
			const MediaFormat& fmt = m_prefs_forced_format ? *m_prefs_forced_format : formats[pos / s_count];
#else
			const MediaFormat& fmt = formats[pos / s_count];
#endif
			const FrameSize* ref = sizes.Get(pos % s_count);
			ULONG linesize = (((ref->width * fmt.bitcount / 8) + 3) >> 2) << 2;

			AM_MEDIA_TYPE stencil = {};
			VIDEOINFOHEADER header = {};

			stencil.pbFormat = reinterpret_cast<BYTE*>(&header);
			stencil.cbFormat = sizeof(header);

			header.bmiHeader.biSize = sizeof(header.bmiHeader);

			fmt.FillMediaType(stencil, header);
			ref->FillMediaType(stencil, header, ref->height * linesize);

			*ppOut = mtype::CreateMediaType(&stencil);

			if (!*ppOut)
				return E_OUTOFMEMORY;

			return S_OK;
		}

		HRESULT OpInputMediaTypes::FinalCreate()
		{
#ifdef WINDOWS_CAMERA_GRAPH_TEST
			GUID subtype = impl::OpDirectShowCamera::OverridenCameraFormat();
			if (subtype != GUID_NULL)
			{
				for (size_t i = 0; i < trans::form_count; i++)
				{
					const MediaFormat* format = formats + i;
					if (format->subtype == subtype)
					{
						m_prefs_forced_format = format;
						break;
					}
				}
			}
#endif
			return S_OK;
		}
	};

	namespace impl
	{
		namespace
		{
			wchar_t vendor_name[] = L"Opera Software ASA";
			wchar_t pin_name[] = L"Input0"; //must not exceed MAX_PIN_NAME
			enum
			{
				PIN_NAME_SIZE = sizeof(pin_name),
				VENDOR_NAME_SIZE = sizeof(vendor_name)
			};
		};

		OpDirectShowCamera::~OpDirectShowCamera()
		{
			mtype::DeleteMediaType(media_type);
			media_type = NULL;
			OP_DELETE(frame);
			OP_DELETE(backbuffer);
		}

		HRESULT OpDirectShowCamera::FinalCreate()
		{
			return CAM2HR(camera_mutex.Init());
		}

		HRESULT OpDirectShowCamera::SetMediaType(const AM_MEDIA_TYPE* pmt, bool notify)
		{
			METHOD_CALL;

			if (media_type)
			{
				mtype::DeleteMediaType(media_type);
				media_type = NULL;
			}

			OpCamera::Resolution res;

			curr_xform = NULL;

#ifdef WINDOWS_CAMERA_GRAPH_TEST
			{
				GUID subtype = OverridenCameraFormat();
				if (subtype != GUID_NULL && subtype != pmt->subtype)
					return VFW_E_INVALID_MEDIA_TYPE;
			}
#endif
			for (size_t i = 0; i < trans::form_count; i++)
			{
				const trans::FormVtbl* transform = trans::forms + i;
				if (transform->Supported(pmt->subtype))
				{
					curr_xform = transform;
					break;
				}
			}

			if (!curr_xform) // now, someone called SetMediaType on media type not vetted by QueryAccept....
				return VFW_E_INVALID_MEDIA_TYPE;

			media_type = mtype::CreateMediaType(pmt);
			if (!media_type)
				return E_OUTOFMEMORY;

			if (media_type->formattype == FORMAT_VideoInfo)
			{
				VIDEOINFOHEADER* header = reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
				res.x = header->bmiHeader.biWidth;
				res.y = header->bmiHeader.biHeight;
			}
			else if (media_type->formattype == FORMAT_VideoInfo2)
			{
				VIDEOINFOHEADER2* header = reinterpret_cast<VIDEOINFOHEADER2*>(media_type->pbFormat);
				res.x = header->bmiHeader.biWidth;
				res.y = header->bmiHeader.biHeight;
			}

			if (res.x == 0 || res.y == 0)
				return E_FAIL;

			if (res.x != frame_res.x || res.y != frame_res.y || !frame)
			{
				DesktopMutexLock on(camera_mutex);

				frame_res = res;
				OP_DELETE(frame);
				frame = NULL;

				OP_CAMERA_STATUS tmp = OpBitmap::Create(&frame, res.x, res.y);
				if (OpStatus::IsError(tmp))
				{
					if (backbuffer_status == BACKBUFFER_FRESH)
					{
						OP_DELETE(backbuffer);
						backbuffer = NULL;
					}
					else
						backbuffer_status = BACKBUFFER_STALE;

					return CAM2HR(tmp);
				}

				OP_ASSERT(frame->Supports(OpBitmap::SUPPORTS_POINTER) && "Bitmaps without SUPPORTS_POINTER are rather pointless here...");
				if (!frame->Supports(OpBitmap::SUPPORTS_POINTER))
				{
					if (backbuffer_status == BACKBUFFER_FRESH)
					{
						OP_DELETE(backbuffer);
						backbuffer = NULL;
					}
					else
						backbuffer_status = BACKBUFFER_STALE;

					OP_DELETE(frame);
					frame = NULL;
					return E_FAIL;
				}

				if (backbuffer_status == BACKBUFFER_FRESH)
				{
					OP_DELETE(backbuffer);
					backbuffer = NULL;

					tmp = OpBitmap::Create(&backbuffer, res.x, res.y);
					if (OpStatus::IsError(tmp))
						return CAM2HR(tmp);

					OP_ASSERT(backbuffer->Supports(OpBitmap::SUPPORTS_POINTER) && "Bitmaps without SUPPORTS_POINTER are rather pointless here...");
					if (!backbuffer->Supports(OpBitmap::SUPPORTS_POINTER))
					{
						OP_DELETE(frame);
						OP_DELETE(backbuffer);
						frame = NULL;
						backbuffer = NULL;
						return E_FAIL;
					}
				}
				else
					backbuffer_status = BACKBUFFER_STALE;
			}

			if (parent && notify)
				parent->OnCameraFrameResize();

			return S_OK;
		}

		HRESULT OpDirectShowCamera::EnsureInputPinPresent()
		{
			if (in_pin)
				return S_OK;

			METHOD_CALL;
			return OpComObjectEx<OpDirectShowCameraPin>::Create(this, &in_pin);
		}

		HRESULT OpDirectShowCamera::ReadSupportedSizes(IPin* pin)
		{
			METHOD_CALL;
			HRESULT hr = S_OK;
			ULONG fetched = 0;

			OpComPtr<IEnumMediaTypes> types;
			OLE(pin->EnumMediaTypes(&types));

			AM_MEDIA_TYPE* type = NULL;

			sizes.DeleteAll();

			while(hr = types->Next(1, &type, &fetched), hr==S_OK && fetched == 1)
			{
				OpAutoPtr<coll::FrameSize> size = OP_NEW(coll::FrameSize, ());

				if (type->majortype == MEDIATYPE_Video && type->cbFormat && type->pbFormat)
				{
					if (type->formattype == FORMAT_VideoInfo)
					{
						VIDEOINFOHEADER* pvi = reinterpret_cast<VIDEOINFOHEADER*>(type->pbFormat);
						size->CopyFromFormat(pvi);
					}
					else if (type->formattype == FORMAT_VideoInfo2)
					{
						VIDEOINFOHEADER2* pvi = reinterpret_cast<VIDEOINFOHEADER2*>(type->pbFormat);
						size->CopyFromFormat(pvi);
					}
				}

				mtype::DeleteMediaType(type);
				type = NULL;


				if (size->width == 0 || size->height == 0)
					continue;

				UINT32 i = 0, len = sizes.GetCount();
				for (; i < len; ++i)
				{
					const coll::FrameSize* ref = sizes.Get(i);
					if (!ref)
						continue;
					if (*ref == *size)
						break;
				}

				if (i == len)
				{
					if (OpStatus::IsError(sizes.Add(size.get())))
						continue;
					size.release();
				}
			}

			return S_OK;
		}

		static inline HRESULT GetBaseFilter(DWORD device, IBaseFilter** ppOut)
		{
			HRESULT hr = S_OK;
			ULONG fetched = 0;

			OpComPtr<ICreateDevEnum> cde;
			OpComPtr<IEnumMoniker> mons;
			OpComPtr<IMoniker> mon;

			OLE(cde.CoCreateInstance(CLSID_SystemDeviceEnum));
			OLE(cde->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &mons, 0));

			OLE(mons->Reset());
			if (device)
				hr = mons->Skip(device); //skip the #[device] monikers from the begining

			hr = mons->Next(1, &mon, &fetched);
			if (hr!=S_OK || fetched != 1)
				return SUCCEEDED(hr) ? E_FAIL : hr;

			return mon->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)ppOut);
		}

		STDMETHODIMP OpDirectShowCamera::CreateGraph(WindowsOpCamera* parent, DWORD device, IMediaControl** ppmc)
		{
			METHOD_CALL;
			HRESULT hr = S_OK;

			this->parent = NULL;

#if defined WINDOWS_CAMERA_GRAPH_TEST && defined OPERA_CONSOLE
			{
				GUID subtype = OverridenCameraFormat();
				if (subtype != GUID_NULL)
				{
					OpConsoleEngine::Message msg(OpConsoleEngine::Internal, OpConsoleEngine::Information);

					msg.context.Set(L"Overriding camera format");
					msg.message.Append(g_pcmswin->GetStringPref(PrefsCollectionMSWIN::SupportedCameraFormat));

					TRAPD(rc, g_console->PostMessageL(&msg));
				}
			}
#endif
			OpComPtr<IGraphBuilder> gb;

			OpComPtr<IBaseFilter> opera, cam;
			OpComPtr<IPin> cam_out;

			OLE(EnsureInputPinPresent());

			OLE(GetBaseFilter(device, &cam));
			OLE(GetBaseObject()->QueryInterface(&opera));

			OLE(gb.CoCreateInstance(CLSID_FilterGraph));

			OLE(gb->AddFilter(cam, L"Camera Source"));
			OLE(gb->AddFilter(opera, L"Opera Sink"));

			{
				OpComPtr<ICaptureGraphBuilder2> cgb;
				OLE(cgb.CoCreateInstance(CLSID_CaptureGraphBuilder2));
				OLE(cgb->SetFiltergraph(gb));
				OLE(cgb->FindPin(cam, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, &cam_out));
			}

			OLE(ReadSupportedSizes(cam_out));
			OLE(gb->Render(cam_out));

			OLE(gb.QueryInterface(ppmc));

			this->parent = parent;

			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::GetCurrentPreviewFrame(OpBitmap** ppFrame)
		{
			DesktopMutexLock on(camera_mutex);

			if (frame_dirty)
			{
				/*
				 if the frame is dirty, we are going to check if it is also
				 stale and recreate the bitmap here.

				 this code is inside the dirty check, because if it wouldn't and
				 the stale buffer would not be dirty at the same time, then we
				 are going to show, for split of a second, a black rectangle
				 (or multicolored rainbow of uninitailized bitmap buffer)
				 instead of perfectly valid last frame from the cam from before
				 it changed it's size...
				*/

				if (backbuffer_status == BACKBUFFER_STALE)
				{
					OP_DELETE(backbuffer);
					backbuffer = NULL;

					//we might have failed in the SetMediaType
					//due to the error with frame creation
					if (!frame)
						return E_FAIL;

					OP_STATUS tmp = OpBitmap::Create(&backbuffer, frame_res.x, frame_res.y);
					if (OpStatus::IsError(tmp))
						return CAM2HR(tmp);

					OP_ASSERT(backbuffer->Supports(OpBitmap::SUPPORTS_POINTER) && "Bitmaps without SUPPORTS_POINTER are rather pointless here...");
					if (!backbuffer->Supports(OpBitmap::SUPPORTS_POINTER))
					{
						OP_DELETE(frame);
						OP_DELETE(backbuffer);
						frame = NULL;
						backbuffer = NULL;
						return E_FAIL;
					}
				}

				//after refreshing the buffer (think low quality chain store fish dept. here)
				//we are changing the status to ACCESSED, so that it will not be meddled with
				//in the SetMediaType
				backbuffer_status = BACKBUFFER_ACCESSED;

				OpBitmap* tmp = backbuffer;
				backbuffer = frame;
				frame = tmp;
				frame_dirty = false;
			}
			else
			{
				//we want to retain the STALE status for next frame_dirty, but change to ACCESS otherwise
				if (backbuffer_status != BACKBUFFER_STALE)
					backbuffer_status = BACKBUFFER_ACCESSED;
			}

			*ppFrame = backbuffer;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::GetPreviewDimensions(OpCamera::Resolution* dimensions)
		{
			if (!dimensions)
				return E_POINTER;

			DesktopMutexLock on(camera_mutex);

			*dimensions = frame_res;

			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::Stop()
		{
			METHOD_CALL;
			is_playing = false;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::Pause()
		{
			METHOD_CALL;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::Run(REFERENCE_TIME tStart)
		{
			METHOD_CALL;
			is_playing = true;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *State)
		{
			METHOD_CALL;
			return E_NOTIMPL;
		}

		STDMETHODIMP OpDirectShowCamera::EnumPins(IEnumPins **ppEnum)
		{
			METHOD_CALL;
			return OpComObjectEx< coll::OpSinglePinEnum >::Create(in_pin, ppEnum);
		}

		STDMETHODIMP OpDirectShowCamera::FindPin(LPCWSTR Id, IPin **ppPin)
		{
			METHOD_CALL;
			if (wcscmp(Id, pin_name) == 0)
				return in_pin.CopyTo(ppPin);
			return VFW_E_NOT_FOUND;
		}

		STDMETHODIMP OpDirectShowCamera::QueryFilterInfo(FILTER_INFO *pInfo)
		{
			METHOD_CALL;
			if (!pInfo)
				return E_POINTER;
			pInfo->pGraph = graph;
			if (pInfo->pGraph)
				pInfo->pGraph->AddRef();

			if (name.IsEmpty())
				pInfo->achName[0] = 0;
			else
			{
				wcsncpy(pInfo->achName, name.CStr(), ARRAY_SIZE(pInfo->achName));
				pInfo->achName[ARRAY_SIZE(pInfo->achName)-1] = 0;
			}
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
		{
			METHOD_CALL;

			//this does not addrefs the graph; if we would addref it here,
			//it would never go down to zero (in which case it would never
			//call JoinFilterGraph(NULL, NULL) on this, nor release the last
			//reference to this, or to the cam source filter)
			graph = pGraph;

			name.Empty();
			if (pName)
				name.Append(pName);

			return S_OK;
		}

		STDMETHODIMP OpDirectShowCamera::QueryVendorInfo(LPWSTR *pVendorInfo)
		{
			METHOD_CALL;

			if (!pVendorInfo)
				return E_POINTER;

			*pVendorInfo = (LPWSTR)CoTaskMemAlloc(VENDOR_NAME_SIZE);

			if (!*pVendorInfo)
				return E_OUTOFMEMORY;

			op_memcpy(*pVendorInfo, vendor_name, VENDOR_NAME_SIZE);
			return S_OK;
		}

		HRESULT OpDirectShowCamera::Connect(IPin* pin, const AM_MEDIA_TYPE* pmt)
		{
			METHOD_CALL;
			return SetMediaType(pmt, false);
		}

		HRESULT OpDirectShowCamera::Disconnect()
		{
			METHOD_CALL;
			mtype::DeleteMediaType(media_type);
			media_type = NULL;

			return S_OK;
		}

		HRESULT OpDirectShowCamera::Receive(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps)
		{
			HRESULT hr = S_OK;

			if (sampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED)
			{
				OLE(SetMediaType(sampleProps.pMediaType));
			}

			if (!parent)
				return S_OK;

			if (!curr_xform)
				return VFW_E_INVALID_MEDIA_TYPE;

			if (!frame)
				return E_FAIL;

			DesktopMutexLock critSection(camera_mutex);

			OLE(trans::Form(*curr_xform, pSample, frame, frame_res.x, frame_res.y));
			frame_dirty = true;

			OpStatus::Ignore(critSection.Release()); //as in lock's d.tor

			parent->OnCameraUpdateFrame();

			return S_OK;
		}

		HRESULT OpDirectShowCamera::QueryMediaType(AM_MEDIA_TYPE* pmt)
		{
			METHOD_CALL;
			return mtype::CopyMediaType(pmt, media_type);
		}

#ifdef WINDOWS_CAMERA_GRAPH_TEST
		GUID OpDirectShowCamera::OverridenCameraFormat()
		{
			OpStringC format = g_pcmswin->GetStringPref(PrefsCollectionMSWIN::SupportedCameraFormat);

			if (format.HasContent())
			{
				if (format.CompareI("RGB24") == 0)
					return MEDIASUBTYPE_RGB24;
				else if (format.CompareI("RGB32") == 0)
					return MEDIASUBTYPE_RGB32;
				else if (format.CompareI("YUY2") == 0)
					return MEDIASUBTYPE_YUY2;
				else if (format.CompareI("MJPG") == 0)
					return MEDIASUBTYPE_MJPG;
				else if (format.CompareI("I420") == 0)
					return MEDIASUBTYPE_4CC_I420;
			}

			return GUID_NULL;
		}
#endif

		STDMETHODIMP OpDirectShowCameraPin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
		{
			METHOD_CALL;
			return E_FAIL; // method for output pins only, anyone calling it on a PINDIR_INPUT pin is (gravely) mistaken
		}

		STDMETHODIMP OpDirectShowCameraPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
		{
			METHOD_CALL;
			if (QueryAccept(pmt) == S_FALSE)
				return VFW_E_TYPE_NOT_ACCEPTED;

			if (IsConnected())
				return VFW_E_ALREADY_CONNECTED;

			if (IsStarted())
				return VFW_E_NOT_STOPPED;

			OLE_(m_parent->Connect(pConnector, pmt));

			m_connection = pConnector;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::Disconnect()
		{
			METHOD_CALL;
			if (IsStarted())
				return VFW_E_NOT_STOPPED;

			m_connection = NULL;
			m_allocator = NULL;

			return m_parent->Disconnect();
		}

		STDMETHODIMP OpDirectShowCameraPin::ConnectedTo(IPin **pPin)
		{
			METHOD_CALL;
			if (!IsConnected())
				return VFW_E_NOT_CONNECTED;
			return m_connection.CopyTo(pPin);
		}

		STDMETHODIMP OpDirectShowCameraPin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
		{
			METHOD_CALL;
			if (!IsConnected())
			{
				op_memset(pmt, 0, sizeof(AM_MEDIA_TYPE));
				return VFW_E_NOT_CONNECTED;
			}

			if (!pmt)
				return E_POINTER;

			return m_parent->QueryMediaType(pmt);
		}

		STDMETHODIMP OpDirectShowCameraPin::QueryPinInfo(PIN_INFO *pInfo)
		{
			METHOD_CALL;
			if (!pInfo)
				return E_POINTER;

			OLE_(m_parent->GetBaseObject()->QueryInterface(&pInfo->pFilter));
			pInfo->dir = PINDIR_INPUT;
			uni_strncpy(pInfo->achName, pin_name, ARRAY_SIZE(pInfo->achName));
			pInfo->achName[ARRAY_SIZE(pInfo->achName)-1] = 0;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::QueryDirection(PIN_DIRECTION *pPinDir)
		{
			METHOD_CALL;
			if (!pPinDir)
				return E_POINTER;

			*pPinDir = PINDIR_INPUT;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::QueryId(LPWSTR *Id)
		{
			METHOD_CALL;

			if (!Id)
				return E_POINTER;

			*Id = (LPWSTR)CoTaskMemAlloc(PIN_NAME_SIZE);

			if (!*Id)
				return E_OUTOFMEMORY;

			op_memcpy(*Id, pin_name, PIN_NAME_SIZE);
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::QueryAccept(const AM_MEDIA_TYPE *pmt)
		{
			METHOD_CALL;
			if (pmt->majortype == MEDIATYPE_Video)
			{
				if (pmt->formattype == FORMAT_VideoInfo ||
					pmt->formattype == FORMAT_VideoInfo2)
				{
#ifdef WINDOWS_CAMERA_GRAPH_TEST
					GUID subtype = OpDirectShowCamera::OverridenCameraFormat();
					if (subtype != GUID_NULL && subtype != pmt->subtype)
						return S_FALSE;
#endif
					for (size_t i = 0; i < trans::form_count; i++)
					{
						if (trans::forms[i].Supported(pmt->subtype))
							return S_OK;
					}
				}
			}
			return S_FALSE;
		}

		STDMETHODIMP OpDirectShowCameraPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
		{
			METHOD_CALL;
			return OpComObjectEx< coll::OpInputMediaTypesEnum >::Create(m_parent->sizes, ppEnum);
		}

		STDMETHODIMP OpDirectShowCameraPin::QueryInternalConnections(IPin **apPin, ULONG *nPin)
		{
			METHOD_CALL;
			if (!nPin)
				return E_POINTER;
			*nPin = 0;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::EndOfStream()
		{
			METHOD_CALL;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::BeginFlush()
		{
			METHOD_CALL;
			m_flushing = true;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::EndFlush()
		{
			METHOD_CALL;
			m_flushing = false;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double)
		{
			METHOD_CALL;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::GetAllocator(IMemAllocator **ppAllocator)
		{
			METHOD_CALL;
			if (!m_allocator)
				OLE_(m_allocator.CoCreateInstance(CLSID_MemoryAllocator));

			return m_allocator.CopyTo(ppAllocator);
		}

		STDMETHODIMP OpDirectShowCameraPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL)
		{
			METHOD_CALL;
			m_allocator = pAllocator;
			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
		{
			METHOD_CALL;
			return E_NOTIMPL;
		}

		HRESULT OpDirectShowCameraPin::GetSampleProps(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps)
		{
			/* Check for IMediaSample2 */
			OpComPtr<IMediaSample2> pSample2;
			if (SUCCEEDED(pSample->QueryInterface(&pSample2)))
			{
				OLE_(pSample2->GetProperties(sizeof(sampleProps), (PBYTE)&sampleProps));
				pSample2 = NULL;
			}
			else
			{
				/*  Get the properties the hard way */
				sampleProps.cbData = sizeof(sampleProps);
				sampleProps.dwTypeSpecificFlags = 0;
				sampleProps.dwStreamId = AM_STREAM_MEDIA;
				sampleProps.dwSampleFlags = 0;
				if (S_OK == pSample->IsDiscontinuity())
					sampleProps.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;

				if (S_OK == pSample->IsPreroll())
					sampleProps.dwSampleFlags |= AM_SAMPLE_PREROLL;

				if (S_OK == pSample->IsSyncPoint())
					sampleProps.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;

				if (SUCCEEDED(pSample->GetTime(&sampleProps.tStart, &sampleProps.tStop)))
						sampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID | AM_SAMPLE_STOPVALID;

				if (S_OK == pSample->GetMediaType(&sampleProps.pMediaType))
					sampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;

				pSample->GetPointer(&sampleProps.pbBuffer);
				sampleProps.lActual = pSample->GetActualDataLength();
				sampleProps.cbBuffer = pSample->GetSize();
			}

			/* Has the format changed in this sample */
			if (sampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED)
			{
				if (QueryAccept(sampleProps.pMediaType) == S_FALSE)
				{
					return VFW_E_INVALIDMEDIATYPE;
				}
			}

			return S_OK;
		}

		STDMETHODIMP OpDirectShowCameraPin::Receive(IMediaSample *pSample)
		{
			if (!IsConnected() || !IsStarted())
				return VFW_E_WRONG_STATE;

			if (IsFlushing())
				return S_FALSE;

			AM_SAMPLE2_PROPERTIES sampleProps = {};
			OLE_(GetSampleProps(pSample, sampleProps));

			return m_parent->Receive(pSample, sampleProps);
		}

		STDMETHODIMP OpDirectShowCameraPin::ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed)
		{
			if (!pSamples)
				return E_POINTER;

			HRESULT hr = S_OK;
			*nSamplesProcessed = 0;
			while (nSamples-- > 0)
			{
				hr = Receive(pSamples[*nSamplesProcessed]);

				/*  S_FALSE means don't send any more */
				if (hr != S_OK)
					break;

				(*nSamplesProcessed)++;
			}
			return hr;
		}

		STDMETHODIMP OpDirectShowCameraPin::ReceiveCanBlock()
		{
			METHOD_CALL;
			return S_OK;
		}
	}

#ifdef DEBUG_DSHOW_GRAPH_DUMP
	namespace debug
	{
		typedef struct {
			CHAR   *szName;
			GUID    guid;
		} GUIDSTRINGENTRY;

#define OUR_GUID_ENTRY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) { #name, { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } } },

		GUIDSTRINGENTRY g_GuidNames[] = {
#include <uuids.h>
		};

#undef OUR_GUID_ENTRY

		const size_t g_cGuidNames = ARRAY_SIZE(g_GuidNames);

		static inline char *FindGuidName(const GUID &guid)
		{
			if (guid == GUID_NULL) {
				return "GUID_NULL";
			}

			for (size_t i = 0; i < g_cGuidNames; i++)
			{
				if (g_GuidNames[i].guid == guid)
				{
					return g_GuidNames[i].szName;
				}
			}

			GUID copy = guid;
			copy.Data1 = 0;
			static const GUID FCC = {0x00000000, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
			if (copy == FCC)
			{
				static char buffer[100];
				strcpy_s(buffer, "MEDIASUBTYPE_XXXX (Four CC)");
				const char* ccguid = (const char*)&guid;
				buffer[13] = ccguid[0];
				buffer[14] = ccguid[1];
				buffer[15] = ccguid[2];
				buffer[16] = ccguid[3];
				return buffer;
			}

			static char buffer[100];
			sprintf_s(buffer, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
				guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
			return buffer;
		}

		static inline void DumpMediaType(const AM_MEDIA_TYPE& media, OpString& msg)
		{
			char * guid;

			guid = FindGuidName(media.majortype);  if (op_strncmp(guid, "MEDIATYPE_", 10) == 0) guid += 10;    msg.Append(guid); msg.Append("/");
			guid = FindGuidName(media.subtype);    if (op_strncmp(guid, "MEDIASUBTYPE_", 13) == 0) guid += 13; msg.Append(guid); msg.Append(" ");
			guid = FindGuidName(media.formattype); if (op_strncmp(guid, "FORMAT_", 7) == 0) guid += 7;         msg.Append(guid);
			if (media.bFixedSizeSamples)
			{
				msg.AppendFormat(" [%d]", media.lSampleSize);
			}
			long width = -1, height = -1;
			ULONG bps = 0;
			REFERENCE_TIME tpf = 1;
			if (media.formattype == FORMAT_VideoInfo)
			{
				VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)media.pbFormat;
				height = vih->bmiHeader.biHeight;
				width = vih->bmiHeader.biWidth;
				bps = vih->dwBitRate;
				tpf = vih->AvgTimePerFrame;
			}
			else if (media.formattype == FORMAT_VideoInfo2)
			{
				VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)media.pbFormat;
				height = vih->bmiHeader.biHeight;
				width = vih->bmiHeader.biWidth;
				bps = vih->dwBitRate;
				tpf = vih->AvgTimePerFrame;
			}
			if (width != -1)
			{
				const wchar_t* size = L"B/s";
				float majorbps = (float)bps;
				majorbps /= 8;
				if (majorbps > 1024)
				{
					size = L"kiB/s"; majorbps /= 1024;
					if (majorbps > 1024)
					{
						size = L"MiB/s"; majorbps /= 1024;
						if (majorbps > 1024)
						{
							size = L"GiB/s"; majorbps /= 1024;
						}
					}
				}
				static const REFERENCE_TIME second = 10000000;
				float fps = second;
				fps /= tpf;
				msg.AppendFormat(" %dx%d", width, height);
				msg.AppendFormat(" %.1f %s", majorbps, size);
				msg.AppendFormat(" %.1f fps", fps);
			}
		}

		static inline void DumpPin(ULONG pinid, ULONG& id, IPin* pin, OpString& msg)
		{
			++id;

			PIN_INFO pinfo = {};

			if (FAILED(pin->QueryPinInfo(&pinfo)))
			{
				pinfo.dir = PINDIR_INPUT;
				wcscpy_s(pinfo.achName, L"<unknown>");
			}
			if (pinfo.pFilter)
				pinfo.pFilter->Release();

			OpComPtr<IPin> pin2;
			if (FAILED(pin->ConnectedTo(&pin2)) || !pin2)
			{
				msg.AppendFormat(L"    %d.%d. %s [%s] (not connected)\n", pinid, id, pinfo.dir == PINDIR_INPUT ? L"Input " : L"Output", pinfo.achName);

				return;
			}

			FILTER_INFO finfo = {};
			PIN_INFO pinfo2 = {};
			if (FAILED(pin2->QueryPinInfo(&pinfo2)))
			{
				pinfo.dir = PINDIR_INPUT;
				wcscpy_s(finfo.achName, L"<unknown pin>");
			}

			if (!pinfo2.pFilter || FAILED(pinfo2.pFilter->QueryFilterInfo(&finfo)))
				wcscpy_s(finfo.achName, L"<unknown filter>");

			if (pinfo2.pFilter)
				pinfo2.pFilter->Release();
			if (finfo.pGraph)
				finfo.pGraph->Release();

			msg.AppendFormat(L"    %d.%d. %s [%s] connected to %s [%s] of [%s]", pinid, id, pinfo.dir == PINDIR_INPUT ? L"Input " : L"Output",
				pinfo.achName, pinfo2.dir == PINDIR_INPUT ? L"input" : L"output", pinfo2.achName, finfo.achName);

			if (pinfo.dir == PINDIR_INPUT)
			{
				AM_MEDIA_TYPE media = {};
				if (SUCCEEDED(pin->ConnectionMediaType(&media)))
				{
					msg.Append(L" with ");
					DumpMediaType(media, msg);
				}
				mtype::FreeMediaType(media);
			}
			msg.Append(L"\n");
		}

		static inline void DumpFilter(ULONG& pinid, IBaseFilter* filter, OpString& msg)
		{
			HRESULT hr = S_OK;

			FILTER_INFO finfo = {};

			++pinid;

			if (FAILED(filter->QueryFilterInfo(&finfo)))
				wcscpy_s(finfo.achName, L"<unknown>");

			if (finfo.pGraph)
				finfo.pGraph->Release();

			//swprintf_s(buffer, L"%d. [%s]\n", pinid, finfo.achName);

			GUID guid;
			if (SUCCEEDED(filter->GetClassID(&guid)))
			{
				// If the filter successfully presented its class id,
				// print it as well, so debugging of the issues such as
				// filter ban is easier. We do not use functions like
				// StringFromCLSID (as they require addtional memory).
				// Instead, we'll print it directly as a bracketed 8-4-4-4-12 UUID[1]
				//
				// [1] http://en.wikipedia.org/wiki/Universally_unique_identifier

				msg.AppendFormat(L"%d. [%s] {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
					pinid, finfo.achName,
					
					guid.Data1, //8 digits
					guid.Data2, //4 digits
					guid.Data3, //4 digits
					guid.Data4[0], guid.Data4[1], //4 digits
					guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] //12 digits
					);
			}
			else
			{
				msg.AppendFormat(L"%d. [%s]\n", pinid, finfo.achName);
			}

			OpComPtr<IEnumPins> pins;
			OpComPtr<IPin> pin;
			if (SUCCEEDED(filter->EnumPins(&pins)))
			{
				ULONG fetched = 0;
				ULONG id = 0;
				while (hr = pins->Next(1, &pin, &fetched), hr == S_OK && fetched == 1)
				{
					DumpPin(pinid, id, pin, msg);
					pin = NULL;
				}
			}
		}

		static inline HRESULT DumpGraph(IGraphBuilder* gb)
		{
#ifdef OPERA_CONSOLE
			HRESULT hr = S_OK;
			if (!g_console || !gb)
				return S_OK;

			// FIXME: Severity?
			OpConsoleEngine::Message msg(OpConsoleEngine::Internal, OpConsoleEngine::Information);

			msg.context.Set(L"DirectShow graph dump");

			OpComPtr<IEnumFilters> fenum;
			OpComPtr<IBaseFilter> filter;
			OLE_(gb->EnumFilters(&fenum));
			ULONG fetched = 0;
			ULONG pinid = 0;

			while (hr = fenum->Next(1, &filter, &fetched), hr == S_OK && fetched == 1)
			{
				DumpFilter(pinid, filter, msg.message);
				filter = NULL;
			}

			TRAPD(rc, g_console->PostMessageL(&msg));
#endif

			return S_OK;
		}
	}
#endif //DEBUG_DSHOW_GRAPH_DUMP
};
#undef METHOD_CALL
#undef VEGA_CLAMP_U8

#endif // PI_CAMERA
