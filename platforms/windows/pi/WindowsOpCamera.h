/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OP_CAMERA_H
#define WINDOWS_OP_CAMERA_H

#ifdef PI_CAMERA

#include "modules/pi/device_api/OpCamera.h"
#include "modules/hardcore/timer/optimer.h"

#include <ole2.h>
#include <dshow.h>
#include <dvdmedia.h> //for FORMAT_VideoInfo2
#include "platforms/windows/utils/com_helpers.h"

class WindowsOpCameraManager;
class WindowsOpCamera;

namespace DirectShow
{
	// Messages to synchronize Core calls and react to media events
	enum
	{
		WMU_UPDATE      = WM_USER,    // call OpCameraPreviewListener::OnCameraUpdateFrame
		WMU_RESIZE      = WM_USER+1,  // call OpCameraPreviewListener::OnCameraFrameResize
		WMU_MEDIA_EVENT = WM_USER+2,  // the media event is ready in the graph
		WMU_PNP_INIT    = WM_USER+3
	};

	MIDL_INTERFACE("565301F4-24B3-4F13-BD6F-FCA8CAAD0907") ICamera: IUnknown
	{
		STDMETHOD(CreateGraph)(WindowsOpCamera* parent, DWORD device, IMediaControl** ppmc) PURE;
		STDMETHOD(GetCurrentPreviewFrame)(OpBitmap** frame) PURE;
		STDMETHOD(GetPreviewDimensions)(OpCamera::Resolution* dimensions);
	};
}

class WindowsOpCameraManager : public OpCameraManager
{
	// Window procedure to synchronize calls to OpCameraPreviewListener
	static LRESULT CALLBACK NotifyProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static WindowsOpCameraManager* global() { return static_cast<WindowsOpCameraManager*>(g_op_camera_manager); }

public:
	WindowsOpCameraManager();
	~WindowsOpCameraManager();

	/* virtual */ OP_STATUS GetCameraDevices(OpVector<OpCamera>* camera_devices);
	/* virtual */ void SetCameraDeviceListener(OpCameraDeviceListener* listener) { m_device_listener = listener; }

	static HWND GetSyncWindow() { return global()->m_notify_sink; }

private:
	HDEVNOTIFY                    m_pnp_notify;  // PnP notify token
	HWND                          m_notify_sink; // helper window for synchronizing Core calls, as well as pEvent and PnP notifications
	OpAutoVector<WindowsOpCamera> m_cameras;     // all cameras, as seen from the start of Opera, including those that are currently disconnected
	OpCameraDeviceListener*       m_device_listener; // listener which will be used to inform core that camera has been attached / dettached

	static inline void InitializePnP(HWND hWnd) { global()->_InitializePnP(hWnd); }
	static inline void UninitializePnP() { global()->_UninitializePnP(); }
	static inline void OnDeviceChange(WPARAM wParam, LPARAM lParam) { global()->_OnDeviceChange(wParam, lParam); }

	void _InitializePnP(HWND hWnd);
	void _UninitializePnP();
	void _OnDeviceChange(WPARAM wParam, LPARAM lParam);
	void inline OnCameraAdded(WindowsOpCamera* nfo);
	void inline OnCameraRemoved(WindowsOpCamera* nfo);
	static inline void UpdatePnPList(OpVector<WindowsOpCamera>& cams);
	static inline WindowsOpCamera* FindPnPCamera(const wchar_t* path, const OpVector<WindowsOpCamera>& cams);
};

class WindowsOpCamera
	: public OpCamera
{
public:
	WindowsOpCamera(IPropertyBag* bag, DWORD device);
	~WindowsOpCamera();

	// Parent API
	void InvalidateDevice() { m_device = (DWORD)-1; }
	void ValidateDevice(DWORD device) { m_device = device; }
	DWORD GetDevice() const { return m_device; }
	bool IsConnected() const { return m_device != (DWORD)-1; }
	const OpStringC& GetPath() const { return m_path; }
	const OpStringC& GetName() const { return m_name; }

	// OpCamera
	virtual OP_CAMERA_STATUS Enable(OpCameraPreviewListener* preview_listener);
	virtual void Disable();
	virtual OP_CAMERA_STATUS GetCurrentPreviewFrame(OpBitmap** frame);
	virtual OP_CAMERA_STATUS GetPreviewDimensions(Resolution* dimensions);

	// to be called on any thread
	void OnCameraUpdateFrame() { PostMessage(WindowsOpCameraManager::GetSyncWindow(), DirectShow::WMU_UPDATE, 0, reinterpret_cast<LPARAM>(this)); }
	void OnCameraFrameResize() { PostMessage(WindowsOpCameraManager::GetSyncWindow(), DirectShow::WMU_RESIZE, 0, reinterpret_cast<LPARAM>(this)); }

	// to be called ONLY on the Core thread from NotifyProc
	static void OnCameraUpdateFrame(LPARAM _this) { reinterpret_cast<WindowsOpCamera*>(_this)->_OnCameraUpdateFrame(); }
	static void OnCameraFrameResize(LPARAM _this) { reinterpret_cast<WindowsOpCamera*>(_this)->_OnCameraFrameResize(); }

	static void OnMediaEvent(LPARAM _this) { reinterpret_cast<WindowsOpCamera*>(_this)->_OnMediaEvent(); }

	BOOL operator == (const WindowsOpCamera& right) const
	{
		if (m_path.CStr() && right.m_path.CStr())
			return m_path == right.m_path;
		else if (!m_path.CStr() && !right.m_path.CStr())
			return m_name.CompareI(right.m_name) == 0;

		return FALSE;
	}

private:
	inline void GetProperty(IPropertyBag* bag, LPCWSTR propname, OpString& prop);
	bool IsEnabled() const { return m_camera && m_control; }

	void _OnCameraUpdateFrame()
	{
		if (m_preview_listener)
			m_preview_listener->OnCameraUpdateFrame(this);
	}
	void _OnCameraFrameResize()
	{
		if (m_preview_listener)
			m_preview_listener->OnCameraFrameResize(this);
	}

	void _OnMediaEvent();

	OpCameraPreviewListener*		m_preview_listener;
	OpString						m_name;			      // Name of the camera
	OpString						m_descr;			  // Description of the camera
	OpString                        m_path;
	DWORD							m_device;             // index of the camera (or 0xFFFFFFFF, if the camera hardware is not present in the PnP tree)
	OpComPtr<DirectShow::ICamera>	m_camera;
	OpComPtr<IMediaControl>			m_control;            // the DirectShow filter graph, exposed as IMediaControl to have access to Play/Stop
	OpComPtr<IMediaEventEx>			m_event;              // the DirectShow event source - another facet of filter graph
};

#endif // PI_CAMERA
#endif // WINDOWS_OP_CAMERA_H
