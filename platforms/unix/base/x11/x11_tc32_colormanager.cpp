/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_tc32_colormanager.h"

#define DO_FAST_PREMULTIPLY


unsigned long X11TC32ColorManager::GetPixelValue(int red, int green, int blue, int alfa, const XColor **color)
{
	if( m_argb_visual )
		return (alfa << 24) | (red << 16) | (green << 8) | blue;
	else
		return (0xFF << 24) | (red << 16) | (green << 8) | blue;
}

void X11TC32ColorManager::GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha)
{
	alpha = m_argb_visual ? ((pixel & 0xff000000) >> 24) : 0xFF;
	red = (pixel & 0xff0000) >> 16;
	green = (pixel & 0xff00) >> 8;
	blue = pixel & 0xff;
}

void X11TC32ColorManager::PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply)
{
	uint32_t *dest32 = (uint32_t *)dest;
	if (m_swap_endianness)
	{
		for (int x=0; x<pixels; x++)
		{
			uint32_t pixel = *data++;
			*dest32++ = (pixel >> 24) | ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8) | (pixel << 24);
		}
	}
	else
	{
		memcpy(dest, data, pixels*4);
		// Should be combined with the memory copy 
		if( m_argb_visual && premultiply )
		{
			Premultiply(true, dest, pixels);
		}
	}
}

void X11TC32ColorManager::GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply)
{
	const uint32_t *src32 = (const uint32_t *)src;
	if (m_swap_endianness)
	{
		for (int x=0; x<pixels; x++)
		{
			uint32_t pixel = *src32++;
			*data ++ = 0xff000000 | (pixel >> 24) | ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8) | (pixel << 24);
		}
	}
	else
	{
		if( m_argb_visual )
		{
			memcpy(data, src, pixels*4);
			// Should be combined with the memory copy 
			if( premultiply )
			{
				Premultiply(false, data, pixels);
			}
		}
		else
		{
			for (int x=0; x<pixels; x++)
			{
				*data++ = 0xff000000 | *src32++;
			}
		}
	}
}


void X11TC32ColorManager::PutPixel(XImage *img, int x, int y, int r, int g, int b, int a)
{
	uint32_t *dest = (uint32_t *)(img->data+y*img->bytes_per_line)+x;

	uint32_t pixel;
	if( m_argb_visual )
		pixel = (a << 24) | (r << 16) | (g << 8) | b;
	else
		pixel = (r << 16) | (g << 8) | b;

	if (m_swap_endianness)
		pixel = ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8) | (pixel << 24);

	*dest = pixel;
}

void X11TC32ColorManager::GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a)
{
	uint32_t pixel = *((const uint32_t *)(img->data+y*img->bytes_per_line)+x);
	if (m_swap_endianness)
		pixel = (pixel >> 24) | ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8);
	a = (pixel >> 24) & 0xff;
	r = (pixel >> 16) & 0xff;
	g = (pixel >> 8) & 0xff;
	b = pixel & 0xff;
}

bool X11TC32ColorManager::Premultiply(bool premultiply, void* data, unsigned int size)
{	
	if( !m_argb_visual )
	{
		return false;
	}

	UINT32 *d = (UINT32*)data;
	bool modified = false;
	unsigned int val, r, g, b, alpha;

	if( premultiply )
	{
		for (unsigned int i=0; i<size; i++)
		{
			val = d[i];
			if( (val&0xFF000000) != 0xFF000000 )
			{
				alpha = (val >> 24);
#if defined(DO_FAST_PREMULTIPLY)
				if( alpha == 0 )
				{
					d[i] = 0;
				}
				else
				{
					g = val & 0x0000FF00; // green
					val &= 0x00FF00FF;    // red and blue
					alpha++;
					g *= alpha;
					g >>= 8;
					val *= alpha;
					val >>= 8;
					alpha--;
					d[i] = (alpha << 24) | (val & 0x00FF00FF) | (g & 0x0000FF00);
				}
#else
				r = (val >> 16) & 0xFF;
				g = (val >> 8) & 0xFF;
				b = val & 0xFF;
						
				// This is the correct version
				//r = (r * alpha) / 255;
				//g = (g * alpha) / 255;
				//b = (b * alpha) / 255;

				// Almost correct
				r = (r * alpha) >> 8;
				g = (g * alpha) >> 8;
				b = (b * alpha) >> 8;

				d[i] = (val & 0xFF000000) | r << 16 | g << 8 | b;
#endif

				modified = true;
			}
		}
	}
	else
	{
		for (unsigned int i=0; i<size; i++)
		{
			val = d[i];
			if( (val&0xFF000000) != 0xFF000000 )
			{
				alpha = (val >> 24);
#if defined(DO_FAST_PREMULTIPLY)
				if( alpha == 0 )
				{
					d[i] = 0;
				}
				else
				{
					/*
					// This fails sometimes. Like SimAquarium widget
					g = val & 0x0000FF00; // green
					val &= 0x00FF00FF;    // red and blue
					g *= 255;
					g /= alpha;
					val *= 255;
					val /= alpha;
					d[i] = (alpha << 24) | (val & 0x00FF00FF) | (g & 0x0000FF00);
					*/					
					r = (((val >> 16) & 0xFF) * 255) / alpha;
					g = (((val >> 8) & 0xFF) * 255) / alpha;
					b = ((val & 0xFF) * 255) / alpha;
					d[i] = (alpha << 24) | r << 16 | g << 8 | b;
					
					modified = true;
				}
#else
				if( alpha != 0)
				{
					r = (val >> 16) & 0xFF;
					g = (val >> 8) & 0xFF;
					b = val & 0xFF;

					// This is the correct version
					//r = (r * 255) / alpha;
					//g = (g * 255) / alpha;
					//b = (b * 255) / alpha;

					// Almost correct
					r = (r << 8) / alpha;
					g = (g << 8) / alpha;
					b = (b << 8) / alpha;

					d[i] = (val & 0xFF000000) | r << 16 | g << 8 | b;
					modified = true;
				}
#endif
			}
		}
	}

	return modified;
}




void X11TC32ColorManager::BlendLine( const void *src, void *dest, const UINT8 *srcalpha, UINT8 *destalpha, int pixels)
{
	const UINT32 *src32 = (const UINT32 *)src;
	UINT32 *dest32 = (UINT32 *)dest;
	while (pixels > 0)
	{
		int o = *srcalpha;
		switch (o)
		{
		case 0:
			break;
		case 255:
			*dest32 = *src32;
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
			UINT32 s = *src32;
			UINT32 d = *dest32;
			*dest32 = (((((s & 0xff0000) >> 16) * o + ((d & 0xff0000) >> 16) * t) + 127) / 255 << 16) |
				(((((s & 0xff00) >> 8) * o + ((d & 0xff00) >> 8) * t) + 127) / 255 << 8) |
				((((s & 0xff) * o + (d & 0xff) * t) + 127) / 255);
			break;
		}
		}
		src32++;
		dest32++;
		srcalpha++;
		if (destalpha)
			destalpha++;
		pixels--;
	}
}

OP_STATUS X11TC32ColorManager::ScaleImage(
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
		int drows = expandY ? ((sy+1) * dtotheight / srcheight) - (dy+yoffs) : 1;
		int srows = expandY ? 1 : (((dy+yoffs+1) * srcheight + dtotheight-1) / dtotheight) - sy;

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
			int dcols = expandX ? ((sx+1) * dtotwidth / srcwidth) - (dx+xoffs) : 1;
			int scols = expandX ? 1 : ((((dx+xoffs)+1) * srcwidth + dtotwidth-1) / dtotwidth) - sx;
			if (dcols + dx > destwidth)
				dcols = destwidth - dx;
			if (scols + sx > srcwidth)
				scols = srcwidth - sx;
			if (dcols <= 0 || scols <= 0)
				break;
			int a_total=0, r_total=0, g_total=0, b_total=0;
			for (int yy=0; yy<srows; yy++)
			{
				const uint32_t *sptr = (const uint32_t *)((uint8_t *)srcbuf+(sy+yy) * srcbpl)+sx;
				for (int xx=0; xx<scols; xx++)
				{
					uint32_t pixel = *sptr++;
					if (m_swap_endianness)
						pixel = (pixel >> 24) | ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8);
					a_total += (pixel >> 24) & 0xff;
					r_total += (pixel >> 16) & 0xff;
					g_total += (pixel >> 8) & 0xff;
					b_total += pixel & 0xff;
				}
			}
			int numPixels = srows * scols;
			a_total /= numPixels;
			r_total /= numPixels;
			g_total /= numPixels;
			b_total /= numPixels;
			
			if(!m_argb_visual)
				a_total = 0xFF;

			uint32_t pixel = (a_total << 24) | (r_total << 16) | (g_total << 8) | b_total;
			if (m_swap_endianness)
				pixel = ((pixel & 0xff0000) >> 8) | ((pixel & 0xff00) << 8) |(pixel << 24);
			for (int yy=0; yy<drows; yy++)
			{
				if (dy+yy < 0)
					continue;
				uint32_t *dptr = (uint32_t *)((uint8_t *)destbuf+(dy+yy)*destbpl)+dx;
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
