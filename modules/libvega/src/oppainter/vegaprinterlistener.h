/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAPRINTERLISTENER_H
#define VEGAPRINTERLISTENER_H

#if defined(VEGA_SUPPORT) && defined(VEGA_PRINTER_LISTENER_SUPPORT)
class VEGAPrinterListener
{
	friend class VEGAOpPainter;
public:
	VEGAPrinterListener() {}
	virtual ~VEGAPrinterListener() {}

	// Set rendering states
	virtual void SetClipRect(const OpRect& clip) = 0;
	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha) = 0;
	virtual void SetFont(OpFont* font) = 0;

	virtual void DrawRect(const OpRect& rect, UINT32 width) = 0;
	virtual void FillRect(const OpRect& rect) = 0;
	virtual void DrawEllipse(const OpRect& rect, UINT32 width) = 0;
	virtual void FillEllipse(const OpRect& rect) = 0;
	//virtual void DrawPolygon(const OpPoint* points, int count, UINT32 width) = 0;

	virtual void DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width) = 0;
	virtual void DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width) = 0;
	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p) = 0;
	//virtual void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) = 0;
	//virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight) = 0;
};
#endif // VEGA_SUPPORT && VEGA_PRINTER_LISTENER_SUPPORT

#endif // VEGAPRINTERLISTENER_H

