/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#include "x11_tc16_colormanager.h"


unsigned long X11TC16ColorManager::GetPixelValue(int red, int green, int blue, int alfa, const XColor **color)
{
	// alfa ignored
	return ((red << 8) & 0xf800) | ((green << 3) & 0x07e0) | ((blue >> 3) & 0x001f);
}

void X11TC16ColorManager::GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha)
{
	red = (pixel & 0xf800) >> 8;
	red |= red >> 5;
	green = (pixel & 0x07e0) >> 3;
	green |= green >> 6;
	blue = (pixel & 0x001f) << 3;
	blue |= blue >> 5;
	alpha = 0xFF;
}

void X11TC16ColorManager::PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply)
{
	int w = pixels;
	UINT16 *dest16 = (UINT16 *)dest;
	if (swap_endianness)
	{
		for (int x=0; x<w; x++)
		{
			UINT32 pixel = *data++;
			uint16_t p = ((pixel >> 8) & 0xf800) |
				((pixel >> 5) & 0x7e0) |
				((pixel & 0xf8) >> 3);
			p = (p << 8) | (p >> 8);
			*dest16++ = p;
		}
	}
	else
	{
		for (int x=0; x<w; x++)
		{
			UINT32 pixel = *data++;
			*dest16++ = ((pixel >> 8) & 0xf800) |
				((pixel >> 5) & 0x7e0) |
				((pixel & 0xf8) >> 3);
		}
	}
}

void X11TC16ColorManager::GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply)
{
	int w = pixels;
	const UINT16 *src16 = (const UINT16 *)src;
	if (swap_endianness)
	{
		for (int x=0; x<w; x++)
		{
			uint16_t pixel = *src16++;
			pixel = (pixel >> 8) | (pixel << 8);
			UINT32 result = 0xff000000 |
				((pixel & 0xf800) << 8) |
				((pixel & 0x7e0) << 5) |
				((pixel & 0x1f) << 3);
			*data ++ = result |
				((result & 0xe00000) >> 5) |
				((result & 0xc000) >> 6) |
				((result & 0xe0) >> 5);
		}
	}
	else
	{
		for (int x=0; x<w; x++)
		{
			UINT32 pixel = *src16++;
			UINT32 result = 0xff000000 |
				((pixel & 0xf800) << 8) |
				((pixel & 0x7e0) << 5) |
				((pixel & 0x1f) << 3);
			*data ++ = result |
				((result & 0xe00000) >> 5) |
				((result & 0xc000) >> 6) |
				((result & 0xe0) >> 5);
		}
	}
}

void X11TC16ColorManager::PutPixel(XImage *img, int x, int y, int r, int g, int b, int a)
{
	// Alpha ignored here
	uint16_t *dest = (UINT16 *)(img->data+y*img->bytes_per_line)+x;
	uint16_t pixel = ((r << 8) & 0xf800) | ((g << 3) & 0x7e0) | ((b >> 3) & 0x1f);
	if (swap_endianness)
		pixel = (pixel >> 8) | (pixel << 8);
	*dest = pixel;
}

void X11TC16ColorManager::GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a)
{
	UINT16 pixel = *((const UINT16 *)(img->data+y*img->bytes_per_line)+x);
	if (swap_endianness)
		pixel = (pixel >> 8) | (pixel << 8);
	a = 0xFF;
	r = (pixel & 0xf800) >> 8;
	r |= r >> 5;
	g = (pixel & 0x7e0) >> 3;
	g |= g >> 6;
	b = (pixel & 0x1f) << 3;
	b |= b >> 5;
}

void X11TC16ColorManager::BlendLine(
	const void *src, void *dest,
	const UINT8 *srcalpha, UINT8 *destalpha, int pixels)
{
	const UINT16 *src16 = (const UINT16 *)src;
	UINT16 *dest16 = (UINT16 *)dest;
	while (pixels > 0)
	{
		int o = *srcalpha;
		switch (o)
		{
		case 0:
			break;
		case 255:
			*dest16 = *src16;
			if (destalpha)
				*destalpha = 255;
			break;
		default:
		{
			int t = 255 - o;
			if (destalpha)
			{
				t = t * *destalpha / 255;
				*destalpha = ((((255 - o) * *destalpha) + o * 255) + 127) / 255;
			}
			UINT16 s = *src16;
			UINT16 d = *dest16;
			*dest16 = (((((s & 0xf800) >> 11) * o + ((d & 0xf800) >> 11) * t) + 127) / 255 << 11) |
				(((((s & 0x7e0) >> 5) * o + ((d & 0x7e0) >> 5) * t ) + 127) / 255 << 5) |
				((((s & 0x1f) * o + (d & 0x1f) * t) + 127) / 255);
			break;
		}
		}
		src16++;
		dest16++;
		srcalpha++;
		if (destalpha)
			destalpha++;
		pixels--;
	}
}

OP_STATUS X11TC16ColorManager::ScaleImage(
	XImage *src, XImage *dest,
	void *srcbuf, void *destbuf,
	int srcwidth, int srcheight, int srcbpl,
	int destwidth, int destheight, int destbpl,
	int xoffs, int yoffs,
	int dtotwidth, int dtotheight)
{
	bool expandX = dtotwidth > srcwidth;
	bool expandY = dtotheight > srcheight;
	int sy = yoffs * srcheight / dtotheight;
	int dy = sy * dtotheight / srcheight - yoffs;

	for (;;)
	{
		int drows = expandY ? ((sy+1) * dtotheight / srcheight) -
			(dy+yoffs) : 1;
		int srows = expandY ? 1 : (((dy+yoffs+1) * srcheight +
									dtotheight-1) / dtotheight) - sy;

		if (drows + dy > destheight)
			drows = destheight - dy;
		if (srows + sy > srcheight)
			srows = srcheight - sy;
		if (drows <= 0 || srows <= 0)
			break;

		int sx = xoffs * srcwidth / dtotwidth;
		int dx = sx * dtotwidth / srcwidth - xoffs;
		for (;;)
		{
			int dcols = expandX ? ((sx+1) * dtotwidth / srcwidth) -
				(dx+xoffs) : 1;
			int scols = expandX ? 1 : ((((dx+xoffs)+1) * srcwidth +
										dtotwidth-1) / dtotwidth) - sx;
			if (dcols + dx > destwidth)
				dcols = destwidth - dx;
			if (scols + sx > srcwidth)
				scols = srcwidth - sx;
			if (dcols <= 0 || scols <= 0)
				break;
			int r_total=0, g_total=0, b_total=0;
			for (int yy=0; yy<srows; yy++)
			{
				const uint16_t *sptr =
					(const uint16_t *)((UINT8 *)srcbuf+(sy+yy)*srcbpl)+sx;
				for (int xx=0; xx<scols; xx++)
				{
					uint16_t pixel = *sptr++;
					if (swap_endianness)
						pixel = (pixel >> 8) | (pixel << 8);
					r_total += pixel >> 11;
					g_total += (pixel >> 5) & 0x3f;
					b_total += pixel & 0x1f;
				}
			}
			int numPixels = srows * scols;
			r_total /= numPixels;
			g_total /= numPixels;
			b_total /= numPixels;
			uint16_t pixel = (r_total << 11) | (g_total << 5) | b_total;
			if (swap_endianness)
				pixel = (pixel >> 8) | (pixel << 8);
			for (int yy=0; yy<drows; yy++)
			{
				if (dy+yy < 0)
					continue;
				uint16_t *dptr =
					(uint16_t *)((UINT8 *)destbuf+(dy+yy)*destbpl)+dx;
				for (int xx=0; xx<dcols; xx++)
				{
					if (dx+xx < 0)
					{
						dptr ++;
						continue;
					}
					*dptr++ = pixel;
				}
			}
			dx += dcols;
			sx += scols;
		}
		dy += drows;
		sy += srows;
	}

	return OpStatus::OK;
}
