/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef VEGA_PRINTER_LISTENER_SUPPORT

#include "platforms/mac/pi/MacVegaPrinterListener.h"
#include "platforms/mac/pi/MacOpFont.h"
#include "platforms/mac/CocoaVegaDefines.h"
#include "platforms/mac/util/MachOCompatibility.h"

MacVegaPrinterListener::MacVegaPrinterListener()
	: m_ctx(NULL)
	, m_port(NULL)
	, m_font(NULL)
{
}

MacVegaPrinterListener::~MacVegaPrinterListener()
{
}

void MacVegaPrinterListener::SetClipRect(const OpRect& clip)
{
	CGContextRestoreGState(m_ctx);
	CGContextSaveGState(m_ctx);
	CGContextClipToRect(m_ctx, CGRectMake(clip.x, m_winHeight-(clip.y+clip.height), clip.width, clip.height));
	CGContextSetRGBFillColor(m_ctx, m_red, m_green, m_blue, m_alpha);
	CGContextSetRGBStrokeColor(m_ctx, m_red, m_green, m_blue, m_alpha);
}

void MacVegaPrinterListener::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	m_red=((float)red)/255.0;
	m_green=((float)green)/255.0;
	m_blue=((float)blue)/255.0;
	m_alpha=((float)alpha)/255.0;
	CGContextSetRGBFillColor(m_ctx, m_red, m_green, m_blue, m_alpha);
	CGContextSetRGBStrokeColor(m_ctx, m_red, m_green, m_blue, m_alpha);
}

void MacVegaPrinterListener::SetFont(OpFont* font)
{
	m_font = font ? (MacOpFont*)((VEGAOpFont*)font)->getVegaFont() : NULL;
}

void MacVegaPrinterListener::DrawRect(const OpRect& rect, UINT32 width)
{
	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, width);
	}
	CGContextStrokeRect(m_ctx, CGRectMake(rect.x, m_winHeight-(rect.y+rect.height), rect.width, rect.height));
	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, 1);
	}
}

void MacVegaPrinterListener::FillRect(const OpRect& rect)
{
	CGContextFillRect(m_ctx, CGRectMake(rect.x, m_winHeight-(rect.y+rect.height), rect.width, rect.height));
}

void MacVegaPrinterListener::DrawEllipse(const OpRect& rect, UINT32 width)
{
	CGRect cgrect = CGRectMake(rect.x, rect.y, rect.width, rect.height);
	cgrect.origin.y = m_winHeight - cgrect.origin.y - cgrect.size.height;

	float cx = cgrect.origin.x + (cgrect.size.width / 2);
	float cy = cgrect.origin.y + (cgrect.size.height / 2);
	float radius = cgrect.size.width / 2;

	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, width);
	}
	if(cgrect.size.width != cgrect.size.height)
	{
		cy = cy * cgrect.size.width / cgrect.size.height;
		CGContextScaleCTM(m_ctx, 1.0, cgrect.size.height/cgrect.size.width);
	}

	CGContextAddArc(m_ctx, cx, cy, radius, 0, 2*M_PI, 0);

	CGContextStrokePath(m_ctx);

	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, 1);
	}
	if(cgrect.size.width != cgrect.size.height)
	{
		CGContextScaleCTM(m_ctx, 1.0, cgrect.size.width/cgrect.size.height);
	}
}

void MacVegaPrinterListener::FillEllipse(const OpRect& rect)
{
	CGRect cgrect = CGRectMake(rect.x, rect.y, rect.width, rect.height);
	cgrect.origin.y = m_winHeight - cgrect.origin.y - cgrect.size.height;

	float cx = cgrect.origin.x + (cgrect.size.width / 2);
	float cy = cgrect.origin.y + (cgrect.size.height / 2);
	float radius = cgrect.size.width / 2;

	if(cgrect.size.width != cgrect.size.height)
	{
		cy = cy * cgrect.size.width / cgrect.size.height;
		CGContextScaleCTM(m_ctx, 1.0, cgrect.size.height/cgrect.size.width);
	}

	CGContextAddArc(m_ctx, cx, cy, radius, 0, 2*M_PI, 0);

	CGContextFillPath(m_ctx);

	if(cgrect.size.width != cgrect.size.height)
	{
		CGContextScaleCTM(m_ctx, 1.0, cgrect.size.width/cgrect.size.height);
	}
}

void MacVegaPrinterListener::DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width)
{
	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, width);
	}

	CGContextBeginPath(m_ctx);

	CGContextMoveToPoint(m_ctx, from.x, m_winHeight - from.y);
	CGContextAddLineToPoint(m_ctx, to.x, m_winHeight - to.y);

	CGContextStrokePath(m_ctx);

	if(width != 1)
	{
		CGContextSetLineWidth(m_ctx, 1);
	}
}

void MacVegaPrinterListener::DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width)
{
	CGContextSaveGState(m_ctx);
	CGContextTranslateCTM(m_ctx, 0, m_winHeight);
	OpPoint pt(pos.x, pos.y);
	if (m_font)
	{
		CGContextSetFont(m_ctx, m_font->GetCGFontRef());
		CGContextSetFontSize(m_ctx, m_font->GetSize());
		m_font->DrawStringUsingFallback(pt, text, len, extra_char_spacing, m_ctx);
	}
	CGContextRestoreGState(m_ctx);
}

static void DeleteBuffer(void *info, const void *data, size_t size)
{
	free(info);
}

void MacVegaPrinterListener::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	CGColorSpaceRef device = CGColorSpaceCreateDeviceRGB();
	size_t data_size = bitmap->GetBytesPerLine()*bitmap->Height();
	void* bitmap_data = malloc(data_size);
	const void* source_data = const_cast<OpBitmap*>(bitmap)->GetPointer(OpBitmap::ACCESS_READONLY);
	memcpy(bitmap_data, source_data, data_size);
	const_cast<OpBitmap*>(bitmap)->ReleasePointer(FALSE);
	
	CGDataProviderRef provider = CGDataProviderCreateWithData(bitmap_data, bitmap_data, data_size, DeleteBuffer);
	CGImageRef image = CGImageCreate(bitmap->Width(), bitmap->Height(), 8, 32, bitmap->GetBytesPerLine(), device, kCGBitmapByteOrderVegaInternal, provider, NULL, true, kCGRenderingIntentAbsoluteColorimetric);
	CGContextDrawImage(m_ctx, CGRectMake(source.x+p.x, m_winHeight-(source.y+p.y+source.height), source.width, source.height), image);
	CFRelease(provider);
	CFRelease(device);
	CGImageRelease(image);
}

void MacVegaPrinterListener::SetGraphicsContext(CGrafPtr port)
{
#ifndef SIXTY_FOUR_BIT
	if (m_port) {
		CGContextRestoreGState(m_ctx);
		QDEndCGContext(m_port, &m_ctx);
	}
	m_port = port;
	m_ctx = NULL;
	if (m_port) {
		Rect bounds;
		GetPortBounds(m_port, &bounds);
		m_winHeight = bounds.bottom-bounds.top;
		QDBeginCGContext(m_port, &m_ctx);
		SetColor(0,0,0,255);
		CGContextSaveGState(m_ctx);
	}
#endif
}

void MacVegaPrinterListener::SetGraphicsContext(CGContextRef context)
{
	if (m_ctx)
	{
		CGContextRestoreGState(m_ctx);
	}
	m_ctx = context;
	if (m_ctx)
	{
		SetColor(0,0,0,255);
		CGContextSaveGState(m_ctx);
	}
}

#endif // VEGA_PRINTER_LISTENER_SUPPORT

