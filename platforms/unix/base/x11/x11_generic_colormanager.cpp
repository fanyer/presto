/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "platforms/unix/base/x11/x11_generic_colormanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_imagemanager.h"
#include "platforms/unix/base/x11/x11_opbitmap.h"
#include "platforms/unix/base/x11/x11utils.h"

#include <limits.h>


X11GenericColorManager::X11GenericColorManager(X11Types::Display *display, int screen)
	: display(display),
	  color_array(0),
	  reverse_color_array(0),
	  colormap(None),
	  screen(screen),
	  num_colors(0),
	  bits_red(0),
	  bits_green(0),
	  bits_blue(0),
	  shift_red(0),
	  shift_green(0),
	  shift_blue(0)
{
}

X11GenericColorManager::~X11GenericColorManager()
{
	OP_DELETEA(color_array);
	OP_DELETEA(reverse_color_array);
}

OP_STATUS X11GenericColorManager::Init(X11Types::Colormap cmap, const XVisualInfo &visinfo)
{
	OP_ASSERT(cmap != None);
	if (cmap == None)
		return OpStatus::ERR;

	colormap = cmap;
	visualinfo = visinfo;

	if (visualinfo.c_class == TrueColor)
	{
		AnalyzeMask(visualinfo.red_mask, shift_red, bits_red);
		AnalyzeMask(visualinfo.green_mask, shift_green, bits_green);
		AnalyzeMask(visualinfo.blue_mask, shift_blue, bits_blue);
	}
	else if (visualinfo.c_class == PseudoColor ||
			 visualinfo.c_class == StaticColor)
	{
		num_colors = DisplayCells(display, screen);
		if (num_colors == 0)
		{
			OP_ASSERT(FALSE);
			printf("opera: No cells in colormap. "
				   "Falling back to black/white display\n");
		}
		else
		{
			if (num_colors > 256)
				num_colors = 256;
			color_array = OP_NEWA(XColor, num_colors);
			if (!color_array)
				return OpStatus::ERR_NO_MEMORY;

			reverse_color_array = OP_NEWA(uint8_t, 32768);
			if (!reverse_color_array)
				return OpStatus::ERR_NO_MEMORY;

			for (unsigned int i=0; i<num_colors; i++)
			{
				color_array[i].pixel = i;
				color_array[i].flags = 0;
			}
			if (!XQueryColors(display, colormap, color_array, num_colors))
			{
				OP_ASSERT(FALSE);
				printf("opera: Call to XQueryColors() failed. "
					   "Falling back to black/white display\n");
				OP_DELETEA(color_array);
				color_array = 0;
				num_colors = 0;
			}
			GenerateReverseColorLookupArray();
		}
	}

	return OpStatus::OK;
}

unsigned long X11GenericColorManager::GetPixelValue(int red, int green, int blue, int alfa, const XColor **color)
{
	static XColor tmp_color;
	if (visualinfo.c_class == TrueColor)
	{
		if (color && visualinfo.depth == 8)
		{
			tmp_color.red = red >> (8 - bits_red) << (16 - bits_red);
			tmp_color.green = green >> (8 - bits_green) << (16 - bits_green);
			tmp_color.blue = blue >> (8 - bits_blue) << (16 - bits_blue);
			tmp_color.flags = DoRed | DoGreen | DoBlue;
			*color = &tmp_color;
		}

		if( visualinfo.depth == 32 )
		{
			return (0xFF000000 & (alfa << 24)) |
				(red >> (8 - bits_red)) << shift_red |
				(green >> (8 - bits_green)) << shift_green |
				(blue >> (8 - bits_blue)) << shift_blue;
		}
		else
		{
			return (red >> (8 - bits_red)) << shift_red |
				(green >> (8 - bits_green)) << shift_green |
				(blue >> (8 - bits_blue)) << shift_blue;
		}
	}
	else if (reverse_color_array)
	{
		int rev_index =
			((red & 0xf8) << 7) |
			((green & 0xf8) << 2) |
			((blue & 0xf8) >> 3);
		unsigned long index = reverse_color_array[rev_index];
		if (color)
			*color = &color_array[index];
//		printf("rgb %d %d %d => index %d = rgb %d %d %d\n", red, green, blue, index, color_array[index].red>>8, color_array[index].green>>8, color_array[index].blue>>8);
		return index;
	}

	unsigned long depth = visualinfo.depth;
	unsigned long intensity = red*77 + green*150 + blue*29;
	if (visualinfo.c_class == StaticGray && depth > 1)
	{
		intensity <<= 16;
		if (depth >= 32)
			return intensity;
		return intensity >> (32 - depth);
	}
	else
	{
		if (intensity > 32767)
			return WhitePixel(display, screen);
		else
			return BlackPixel(display, screen);
	}
}

void X11GenericColorManager::GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha)
{
	if (visualinfo.c_class == TrueColor)
	{
		red = ((pixel >> shift_red) << (8 - bits_red)) & 0xff;
		green = ((pixel >> shift_green) << (8 - bits_green)) & 0xff;
		blue = ((pixel >> shift_blue) << (8 - bits_blue)) & 0xff;
		
		if (visualinfo.depth == 32)
		{
			alpha = (pixel >> 24);
		}
		else
		{
			alpha = 0xFF;
		}

		for (int i=bits_red; i<8; i+= bits_red)
			red |= red >> i;
		for (int i=bits_green; i<8; i+= bits_green)
			green |= green >> i;
		for (int i=bits_blue; i<8; i+= bits_blue)
			blue |= blue >> i;
		return;
	}
	else if (color_array)
	{
		if (pixel >= num_colors)
			pixel = 0;

		red = color_array[pixel].red >> 8;
		green = color_array[pixel].green >> 8;
		blue = color_array[pixel].blue >> 8;
		alpha = 0xFF;
	}
	else
	{
		unsigned long depth = visualinfo.depth;
		if (visualinfo.c_class == StaticGray && depth > 1)
		{
			unsigned long intensity;
			if (depth >= 32)
				intensity = pixel;
			else
				intensity = (pixel << (32 - depth));
			int i;
			for (i=32-depth; i>24; i-=depth)
			{
				intensity |= pixel << i;
			}
			red = green = blue = intensity >> 24;
		}
		else
		{
			if (pixel == BlackPixel(display, screen))
				red = green = blue = 0;
			else
				red = green = blue = 0xff;
		}
		alpha = 0xFF;
	}
}

void X11GenericColorManager::PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply)
{
	OP_ASSERT(img);
	if (!img)
		return;
	int w = img->width;
	for (int x=0; x<w; x++)
	{
		UINT32 pixel = *data++;
		int a = (pixel >> 24) & 0xff;
		int r = (pixel >> 16) & 0xff;
		int g = (pixel >> 8) & 0xff;
		int b = pixel & 0xff;
		XPutPixel(img, x, line, GetPixelValue(r, g, b, a));
	}
}

void X11GenericColorManager::GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply)
{
	OP_ASSERT(img);
	if (!img)
		return;
	int w = img->width;
	for (int x=0; x<w; x++)
	{
		unsigned long pixel = XGetPixel(img, x, line);
		int r, g, b, a;
		GetRGB(pixel, r, g, b, a);
		*data ++ = (a << 24) | (r << 16) | (g << 8) | b;
	}
}

void X11GenericColorManager::PutPixel(XImage *img, int x, int y, int r, int g, int b, int a)
{
	XPutPixel(img, x, y, X11GenericColorManager::GetPixelValue(r, g, b, a));
}

void X11GenericColorManager::GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a)
{
	unsigned long pixel = XGetPixel(img, x, y);
	GetRGB(pixel, r, g, b, a);
}

OP_STATUS X11GenericColorManager::ScaleImage(
	XImage *src, XImage *dest,
	void *srcbuf, void *destbuf,
	int srcwidth, int srcheight, int srcbpl,
	int destwidth, int destheight, int destbpl,
	int xoffs, int yoffs,
	int dtotwidth, int dtotheight)
{
	return X11Utils::ScaleImage(src, dest, this, this, xoffs, yoffs, dtotwidth, dtotheight);
}

void X11GenericColorManager::GenerateReverseColorLookupArray()
{
#ifdef DEBUG
	printf("Generating reverse color lookup array\n");

	/* Perform a sanity check: do the colorcells on the server have the same
	   values as our local copy? */

	XColor *tmparray = OP_NEWA(XColor, num_colors);
	for (unsigned int i=0; i<num_colors; i++)
	{
		tmparray[i].pixel = i;
		tmparray[i].flags = 0;
	}
	XQueryColors(display, colormap, tmparray, num_colors);

	for (unsigned int i=0; i<num_colors; i++)
	{
		if (tmparray[i].pixel != color_array[i].pixel)
			printf("Pixel mismatch at index %d: %ld != %ld\n", i, tmparray[i].pixel, color_array[i].pixel);
		if (tmparray[i].red != color_array[i].red)
			printf("Red mismatch at index %d: %d != %d\n", i, tmparray[i].red, color_array[i].red);
		if (tmparray[i].green != color_array[i].green)
			printf("Green mismatch at index %d: %d != %d\n", i, tmparray[i].green, color_array[i].green);
		if (tmparray[i].blue != color_array[i].blue)
			printf("Blue mismatch at index %d: %d != %d\n", i, tmparray[i].blue, color_array[i].blue);
	}
	OP_DELETEA(tmparray);
#endif // DEBUG

	for (int r=0; r<32; r++)
	{
		for (int g=0; g<32; g++)
		{
			for (int b=0; b<32; b++)
			{
				int lowest_distance = INT_MAX;
				uint8_t closest_ix = 0;
				unsigned int i;
				for (i=0; i<num_colors; i++)
				{
					int distance = abs((color_array[i].red >> 11) - r) +
						abs((color_array[i].green >> 11) - g) +
						abs((color_array[i].blue >> 11) - b);

					if (distance == 0)
					{
						// We cannot get closer than this
						closest_ix = i;
						break;
					}
					if (distance < lowest_distance)
					{
						lowest_distance = distance;
						closest_ix = i;
					}
				}
				reverse_color_array[(r << 10) | (g << 5) | b] = closest_ix;
			}
		}
	}
#ifdef DEBUG
	printf("Done\n");
#endif
}

void X11GenericColorManager::AnalyzeMask(unsigned int mask, int &shift, int &bits)
{
	shift = 0;
	bits = 0;

	int count = 0;
	unsigned int m = mask;
	while ((m & 1) == 0 && count < 32)
	{
		m >>= 1;
		count ++;
	}
	if (count == 32)
		return;

	shift = count;

	while (m)
	{
		m >>= 1;
		bits ++;
	}
}

#endif // !VEGA_OPPAINTER_SUPPORT
