/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef MAC_VEGA_PRINTER_LISTENER_H
#define MAC_VEGA_PRINTER_LISTENER_H

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
#include "modules/libvega/src/oppainter/vegaprinterlistener.h"
class MacOpFont;

class MacVegaPrinterListener : public VEGAPrinterListener
{
public:
	MacVegaPrinterListener();
	virtual ~MacVegaPrinterListener();

	virtual void SetClipRect(const OpRect& clip);
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

	void SetGraphicsContext(CGrafPtr ctx);
	void SetGraphicsContext(CGContextRef ctx);
	CGContextRef GetCGContext() { return m_ctx; }
	float GetWindowHeight() { return m_winHeight; }
	void SetWindowHeight(float height) { m_winHeight = height; }
private:
	CGContextRef m_ctx;
	CGrafPtr m_port;
	MacOpFont* m_font;
	float m_winHeight;
	float m_red;
	float m_green;
	float m_blue;
	float m_alpha;
};

#endif // VEGA_PRINTER_LISTENER_SUPPORT

#endif // MAC_VEGA_PRINTER_LISTENER_H
