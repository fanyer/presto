/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/pi/OpBitmap.h"

void PrintVegaBitmap(HDC hdc, const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	UINT32 width = bitmap->Width();
	UINT32 height = bitmap->Height();

	BITMAPINFO bmi;

	memset(&bmi, 0, sizeof(BITMAPINFO));

	bmi.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes			= 1;
	bmi.bmiHeader.biWidth			= width;
	bmi.bmiHeader.biHeight			= height;
	bmi.bmiHeader.biBitCount		= 24;
	bmi.bmiHeader.biCompression		= BI_RGB;

	UINT8* bits = new UINT8[width * height * 4];

	if (bits)
	{
		// Seems like some printer drivers have a problem with 32 bits, so we downgrade
		// to 24 bits, loosing the alpha channel that we don't need anyway

		UINT32 i;
		UINT32 x;
		UINT32 y;

		UINT8* src;
		UINT8* dst;

		UINT32 src_bytes_per_line = width * 4;
		UINT32 dst_bytes_per_line = (((width * 3) + 3) >> 2) * 4;

		for (i = 0; i < height; i++)
		{
			bitmap->GetLineData(bits + width * 4 * i, height - i - 1);
		}

		src = bits;
		dst = bits;

		for (y = 0; y < height; y++)
		{
			UINT8* src_line = src;
			UINT8* dst_line = dst;

			for (x = 0; x < width; x++)
			{
				if (src_line[3] == 0)
				{
					dst_line[0] = 0xff;
					dst_line[1] = 0xff;
					dst_line[2] = 0xff;
				}
				else
				{
					dst_line[0] = src_line[0];
					dst_line[1] = src_line[1];
					dst_line[2] = src_line[2];
				}

				src_line += 4;
				dst_line += 3;
			}

			src += src_bytes_per_line;
			dst += dst_bytes_per_line;
		}

		StretchDIBits(hdc, p.x, p.y, source.width, source.height, source.x, source.y, source.width, source.height, bits, &bmi, DIB_RGB_COLORS, SRCCOPY);
	
		delete [] bits;
	}
}

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
#include "platforms/windows/windowsvegaprinterlistener.h"
#ifdef SUPPORT_TEXT_DIRECTION
#include "WindowsComplexScript.h"
#endif
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "platforms/windows/pi/GdiOpFont.h"

WindowsVegaPrinterListener::WindowsVegaPrinterListener(HDC dc) : hdc(dc), oldhfont(NULL), color(0), 
	pen_created(FALSE), brush_created(FALSE), text_color_set(FALSE), 
	pen(NULL), system_color_pen(TRUE), brush(NULL), system_color_brush(TRUE)
{}

WindowsVegaPrinterListener::~WindowsVegaPrinterListener()
{
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	SelectObject(hdc, GetStockObject(BLACK_PEN));
	if (oldhfont)
		SelectObject(hdc, oldhfont);
	if(pen && !system_color_pen)
		DeleteObject(pen);
	if(brush && !system_color_brush)
		DeleteObject(brush);
}

void WindowsVegaPrinterListener::SetClipRect(const OpRect& clip)
{
	SelectClipRgn(hdc, NULL);
	IntersectClipRect(hdc, clip.x, clip.y, clip.x + clip.width, clip.y + clip.height);
}

BOOL WindowsVegaPrinterListener::MakeRect(const OpRect &rect, RECT& r)
{
	r.left = rect.x;
	r.top = rect.y;
	r.right = rect.x + rect.width;
	r.bottom = rect.y + rect.height;

	// win9x only supports 16 bit coordinates, so we need to clip

	if (r.left < -16000)
		r.left = -16000;
	if (r.top < -16000)
		r.top = -16000;
	if (r.right > 16000)
		r.right = 16000;
	if (r.bottom > 16000)
		r.bottom = 16000;

	// also, return FALSE if totally out of range

	if (r.left > 16000 || r.top > 16000 || r.right < -16000 || r.bottom < -16000)
		return FALSE;

	return TRUE;
}

void WindowsVegaPrinterListener::CreateAndSelectPen()
{
	if (pen_created)
		return;

	HPEN oldpen = system_color_pen ? NULL : pen;

	if (color == 0) // black
	{
		pen = (HPEN)GetStockObject(BLACK_PEN);	
		system_color_pen = TRUE;
	}
	else if (color == OP_RGB(255, 255, 255)) // white
	{
		pen = (HPEN)GetStockObject(WHITE_PEN);	
		system_color_pen = TRUE;
	}
	else
	{
		pen = CreatePen(PS_SOLID, 1, color);
		system_color_pen = FALSE;
	}
	
	SelectObject(hdc, pen);

	if (oldpen)
		DeleteObject(oldpen);

	pen_created = TRUE;
}

void WindowsVegaPrinterListener::CreateAndSelectBrush()
{
	if (brush_created)
		return;

	HBRUSH oldbrush = system_color_brush ? NULL : brush;

	if (color == 0) // black
	{
		brush = (HBRUSH)GetStockObject(BLACK_BRUSH);	
		system_color_brush = TRUE;
	}
	else if (color == OP_RGB(255, 255, 255)) // white
	{
		brush = (HBRUSH)GetStockObject(WHITE_BRUSH);
		system_color_brush = TRUE;
	}
	else
	{
		brush = CreateSolidBrush(color);
		system_color_brush = FALSE;
	}

	SelectObject(hdc, brush);

	if (oldbrush)
		DeleteObject(oldbrush);

	brush_created = TRUE;
}

void WindowsVegaPrinterListener::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	COLORREF oldcolor = color;

	color = RGB(red, green, blue);

	if (oldcolor == color)
		return;
	
	pen_created = brush_created = text_color_set = FALSE;
}

void WindowsVegaPrinterListener::SetFont(OpFont* font)
{
	VEGAFont* vega_font = static_cast<VEGAOpFont*>(font)->getVegaFont();
	GdiOpFont* gdi_font = static_cast<GdiOpFont*>(vega_font);
	HFONT tmpfnt = gdi_font->SetFontOnHDC(hdc);

	if(oldhfont == NULL)	// We must store the very first original fontobject
		oldhfont = tmpfnt;
}

void WindowsVegaPrinterListener::DrawRect(const OpRect& rect, UINT32 width)
{
	// FIXME: width > 1
	RECT r;
	if (!MakeRect(rect, r))
		return;

	CreateAndSelectBrush();
	::FrameRect(hdc, &r, brush);
}
void WindowsVegaPrinterListener::FillRect(const OpRect& rect)
{
	RECT r;
	if (!MakeRect(rect, r))
		return;

	CreateAndSelectBrush();
	::FillRect(hdc, &r, brush);
}
void WindowsVegaPrinterListener::DrawEllipse(const OpRect& rect, UINT32 width)
{
	// FIXME: width > 1
	RECT r;
	if (!MakeRect(rect, r))
		return;

	CreateAndSelectPen();
	SelectObject(hdc, GetStockObject(NULL_BRUSH)); //Set a transparent brush
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
	SelectObject(hdc, brush); //Set back the current brush
}
void WindowsVegaPrinterListener::FillEllipse(const OpRect& rect)
{
	RECT r;
	if (!MakeRect(rect, r))
		return;

	CreateAndSelectPen();
	CreateAndSelectBrush();
	Ellipse(hdc, r.left, r.top, r.right, r.bottom);
}

void WindowsVegaPrinterListener::DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width)
{
	HPEN tmppen;
	HPEN oldpen = NULL;
	
	LOGBRUSH lbrush={BS_SOLID, color, 0};
	tmppen=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_FLAT, width, &lbrush, 0, NULL);
	oldpen = (HPEN)SelectObject(hdc, tmppen);

	MoveToEx(hdc, from.x, from.y, NULL);
	LineTo(hdc, to.x, to.y);

	SelectObject(hdc, oldpen); //Set back the current pen
	DeleteObject(tmppen);
}

void WindowsVegaPrinterListener::DrawString(const OpPoint& pos,const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width)
{
	if (!text_color_set)
	{
		SetTextColor(hdc, color);
		text_color_set = TRUE;
	}
	if (extra_char_spacing != 0)
		SetTextCharacterExtra(hdc, extra_char_spacing);

#ifdef SUPPORT_TEXT_DIRECTION
	WIN32ComplexScriptSupport::TextOut(hdc, pos.x, pos.y, text, len);
#else
	TextOut(hdc, pos.x, pos.y, text, len);
#endif

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(hdc, 0);
}
void WindowsVegaPrinterListener::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	PrintVegaBitmap(hdc, bitmap, source, p);
}

#endif // VEGA_PRINTER_LISTENER_SUPPORT
