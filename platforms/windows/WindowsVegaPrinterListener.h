/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSVEGAPRINTERLISTENER_H
#define WINDOWSVEGAPRINTERLISTENER_H

void PrintVegaBitmap(HDC hdc, const OpBitmap* bitmap, const OpRect& source, OpPoint p);

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
#include "modules/libvega/src/oppainter/vegaprinterlistener.h"

class WindowsVegaPrinterListener : public VEGAPrinterListener
{
public:
	WindowsVegaPrinterListener(HDC dc);
	virtual ~WindowsVegaPrinterListener();

	virtual void SetClipRect(const OpRect& clip);
	BOOL MakeRect(const OpRect &rect, RECT& r);
	void CreateAndSelectPen();
	void CreateAndSelectBrush();
	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha);
	virtual void SetFont(OpFont* font);

	virtual void DrawRect(const OpRect& rect, UINT32 width);
	virtual void FillRect(const OpRect& rect);
	virtual void DrawEllipse(const OpRect& rect, UINT32 width);
	virtual void FillEllipse(const OpRect& rect);
	virtual void DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width);
	virtual void DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width);
	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p);

	HDC getHDC(){return hdc;}
private:
	HDC hdc;
	HFONT oldhfont;
	COLORREF color;
	BOOL pen_created;
	BOOL brush_created;
	BOOL text_color_set;
	HPEN pen;
	BOOL system_color_pen;
	HBRUSH brush;
	BOOL system_color_brush;
};

#endif // VEGA_PRINTER_LISTENER_SUPPORT
#endif // WINDOWSVEGAPRINTERLISTENER_H
