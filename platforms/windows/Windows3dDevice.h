/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS3DDEVICE_H
#define WINDOWS3DDEVICE_H

#ifdef VEGA_BACKEND_OPENGL

#include "platforms/vega_backends/opengl/vegagldevice.h"

OP_STATUS GetBackendIdentifier(OpString& section, unsigned int backend, VEGABlocklistDevice::DataProvider* provider);

class WindowsGlDevice : public VEGAGlDevice
{
public:
    WindowsGlDevice(VEGAWindow* nativeWin);
	virtual ~WindowsGlDevice();

	virtual OP_STATUS Construct(VEGABlocklistFileEntry* blocklist_entry);

	virtual BlocklistType GetBlocklistType() const { return WinGL; }
	DataProvider* CreateDataProvider();
	OP_STATUS GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider);
	virtual OP_STATUS createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin);

	virtual OP_STATUS Init(){return OpStatus::OK;}

	virtual unsigned int getDeviceType() const;
	virtual void* getGLFunction(const char* funcName);

	void activateDC(HDC dc);
	OP_STATUS InitDevice();

	bool hasSwapCopy() {return m_has_swapcopy;}
private:
	OP_STATUS createGLWindow(HWND& hwin, HDC& dc, HGLRC& rc, const uni_char* winclass, unsigned int width, unsigned int height, bool visible);
	OP_STATUS initializeGLWindow(HWND hwin, HDC& dc);

	HWND m_hwin;
	HDC m_dc;
	HGLRC m_rc;
	HDC m_activeDC;
	HMODULE m_opengl32_dll;
	bool m_initialized;
	VEGAWindow* m_window;
	bool m_has_swapcopy;
};

#endif // VEGA_BACKEND_OPENGL
#endif // WINDOWS3DDEVICE_H
