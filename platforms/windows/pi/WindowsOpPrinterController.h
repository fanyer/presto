/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPPRINTERCONTROLLER_H
#define WINDOWSOPPRINTERCONTROLLER_H

#include "modules/pi/OpPrinterController.h"

class WindowsOpPrinterController : public OpPrinterController
{
public:
	WindowsOpPrinterController();
	
	/**
	   Windows specific initialization method.
	   @param hdc this hdc is used to initialize the painter, which assumes ownership of the hdc.
	   @return OpStatus::OK if everything went ok, error code always.
	*/
	
	OP_STATUS Init(HDC hdc);
	
	~WindowsOpPrinterController();

	WindowsOpPrinterController* GetCopy();
	const OpPainter* GetPainter();
	
	OP_STATUS BeginPage();

	OP_STATUS EndPage();

	void GetSize(UINT32* w, UINT32* h);
	void GetOffsets(UINT32* w, UINT32* h, UINT32* right, UINT32* bottom);
	void GetDPI(UINT32* x, UINT32* y) { *x = m_screenDPIx; *y = m_screenDPIy; }
	void SetScale(UINT32 scale);
	
	UINT32 GetScale() { return m_scale; }

	BOOL IsMonochrome();

private:
	HDC m_printerHDC;
	OpPainter* m_painter;
	OpBitmap* m_backbuffer;
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	class WindowsVegaPrinterListener* m_printerListener;
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	UINT32 m_scale;
	UINT32 m_printerDPIx;
	UINT32 m_printerDPIy;
	UINT32 m_screenDPIx;
	UINT32 m_screenDPIy;
	UINT32 m_paperWidth;
	UINT32 m_paperHeight;
	UINT32 m_leftOffset;
	UINT32 m_topOffset;
	UINT32 m_rightOffset;
	UINT32 m_bottomOffset;
	BOOL m_printingPage;
};

#endif // WINDOWSOPPRINTERCONTROLLER_H
