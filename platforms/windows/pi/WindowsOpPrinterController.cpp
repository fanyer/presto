/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "WindowsOpPrinterController.h"
#include "WindowsOpScreenInfo.h"

#include "modules/pi/OpBitmap.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

#include "platforms/windows/WindowsVegaPrinterListener.h"

WindowsOpPrinterController::WindowsOpPrinterController()
	: m_printerHDC(0),
	  m_painter(NULL),
	  m_backbuffer(NULL),
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	  m_printerListener(NULL),
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	  m_scale(0),
	  m_printerDPIx(0),
	  m_printerDPIy(0),
	  m_screenDPIx(0),
	  m_screenDPIy(0),
	  m_paperWidth(0),
	  m_paperHeight(0),
	  m_leftOffset(0),
	  m_topOffset(0),
	  m_printingPage(FALSE)
{
}
	
OP_STATUS WindowsOpPrinterController::Init(HDC hdc)
{
	m_printerHDC	= hdc;
	m_painter = NULL;

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	m_printerListener = OP_NEW(WindowsVegaPrinterListener, (m_printerHDC));
	if (!m_printerListener)
		return OpStatus::ERR_NO_MEMORY;
#endif // VEGA_PRINTER_LISTENER_SUPPORT

	m_printerDPIx	= GetDeviceCaps(m_printerHDC, LOGPIXELSX);
	m_printerDPIy	= GetDeviceCaps(m_printerHDC, LOGPIXELSY);

	m_paperWidth	= GetDeviceCaps(m_printerHDC, PHYSICALWIDTH);
	m_paperHeight	= GetDeviceCaps(m_printerHDC, PHYSICALHEIGHT);

	m_leftOffset    = GetDeviceCaps(m_printerHDC, PHYSICALOFFSETX);
	m_topOffset     = GetDeviceCaps(m_printerHDC, PHYSICALOFFSETY);
	m_rightOffset	= m_paperWidth - GetDeviceCaps(m_printerHDC, HORZRES) - m_leftOffset;
	m_bottomOffset	= m_paperHeight - GetDeviceCaps(m_printerHDC, VERTRES) - m_topOffset;

	g_op_screen_info->GetDPI(&m_screenDPIx, &m_screenDPIy, NULL);

    SetBkMode(m_printerHDC,			TRANSPARENT);
	SetMapMode(m_printerHDC,		MM_ANISOTROPIC);

	SetWindowExtEx(m_printerHDC, m_screenDPIx * 10, m_screenDPIy * 10, NULL);

	SetScale(100);

	return OpStatus::OK;
}
	
WindowsOpPrinterController::~WindowsOpPrinterController()
{
	OP_ASSERT(!m_printingPage);

	if (m_painter)
		m_backbuffer->ReleasePainter();
	OP_DELETE(m_backbuffer);
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	OP_DELETE(m_printerListener);
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

const OpPainter* WindowsOpPrinterController::GetPainter()
{
	if (!m_painter)
	{
		UINT32 w, h;
		GetSize(&w, &h);
		if (OpStatus::IsSuccess(OpBitmap::Create(&m_backbuffer, w, h, FALSE, TRUE, 0, 0, TRUE)))
			m_painter = m_backbuffer->GetPainter();
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
		if (m_painter)
			((VEGAOpPainter*)m_painter)->setPrinterListener(m_printerListener);
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	}

	return (const OpPainter*)m_painter;
}

OP_STATUS WindowsOpPrinterController::BeginPage()
{
	OP_ASSERT(!m_printingPage);
	if (StartPage(m_printerHDC) > 0)
	{
		// clear the bitmap to prepare for a new page
		if (m_backbuffer)
		{
			m_backbuffer->SetColor(NULL, TRUE, NULL);
		}

		// Need to set attributes for each page. StartPage resets the attributes, but only in win98, win95...
         SetBkMode(m_printerHDC,         TRANSPARENT);
         SetMapMode(m_printerHDC,        MM_ANISOTROPIC);
         SetWindowExtEx(m_printerHDC,    m_screenDPIx * 10,              m_screenDPIy * 10,              NULL);
         SetViewportExtEx(m_printerHDC,  m_printerDPIx * m_scale / 10,   m_printerDPIy * m_scale / 10,   NULL);

		 m_printingPage = TRUE;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS WindowsOpPrinterController::EndPage()
{
	if (!m_printingPage)
		return OpStatus::OK;
	m_printingPage = FALSE;
#ifndef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_backbuffer)
	{
		// print the backbuffer as an image
		PrintVegaBitmap(m_printerHDC, m_backbuffer, OpRect(0,0,m_backbuffer->Width(),m_backbuffer->Height()), OpPoint(0,0));
	}
#endif // !VEGA_PRINTER_LISTENER_SUPPORT
	return ::EndPage(m_printerHDC) > 0 ? OpStatus::OK : OpStatus::ERR;
}

void WindowsOpPrinterController::GetSize(UINT32* width, UINT32* height)
{
	if (m_scale <= 0) 
	{
		SetScale(100);
	}

	*width	= m_printerDPIx ? m_paperWidth	* m_screenDPIx / m_printerDPIx * 100 / m_scale : 0;
	*height	= m_printerDPIy ? m_paperHeight	* m_screenDPIy / m_printerDPIy * 100 / m_scale : 0;
}

void WindowsOpPrinterController::GetOffsets(UINT32* left, UINT32* top, UINT32* right, UINT32* bottom)
{
	if (m_scale <= 0) 
	{
		SetScale(100);
	}

	*left	= m_printerDPIx ? m_leftOffset	 * m_screenDPIx / m_printerDPIx * 100 / m_scale : 0;
	*top	= m_printerDPIy ? m_topOffset	 * m_screenDPIy / m_printerDPIy * 100 / m_scale : 0;
	*right	= m_printerDPIx ? m_rightOffset	 * m_screenDPIx / m_printerDPIx * 100 / m_scale : 0;
	*bottom	= m_printerDPIy ? m_bottomOffset * m_screenDPIy / m_printerDPIy * 100 / m_scale : 0;
}

void WindowsOpPrinterController::SetScale(UINT32 scale)
{
	m_scale = scale;
	SetViewportExtEx(m_printerHDC, m_printerDPIx * m_scale / 10, m_printerDPIy * m_scale / 10, NULL);

}

BOOL WindowsOpPrinterController::IsMonochrome()
{
	return (::GetDeviceCaps(m_printerHDC, NUMCOLORS) == 2) ? TRUE : FALSE;
}

