/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2002-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_COLORMANAGER_GENERIC_H_
#define X11_COLORMANAGER_GENERIC_H_

#include "x11_colormanager.h"

#include "platforms/utilix/x11_all.h"


/**
 * The X11 generic (and somewhat slow) color manager, that should work an all
 * kinds of X servers.
 */
class X11GenericColorManager : public X11ColorManager
{
private:
	XVisualInfo visualinfo;
	X11Types::Display *display;
	XColor *color_array;
	uint8_t *reverse_color_array;
	X11Types::Colormap colormap;
	int screen;
	unsigned int num_colors;
	int bits_red;
	int bits_green;
	int bits_blue;
	int shift_red;
	int shift_green;
	int shift_blue;

	static void AnalyzeMask(unsigned int mask, int &shift, int &bits);

public:
	X11GenericColorManager(X11Types::Display *display, int screen);
	~X11GenericColorManager();

	OP_STATUS Init(X11Types::Colormap cmap, const XVisualInfo &visinfo);

	// Implementation of X11ColorManager
	unsigned long GetPixelValue(int red, int green, int blue, int alfa, const XColor **color=0);
	void GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha);
	void PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply);
	void GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply);
	void PutPixel(XImage *img, int x, int y, int r, int g, int b, int a);
	void GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a);

	void BlendLine(
		const void *src, void *dest,
		const UINT8 *srcalpha, UINT8 *destalpha, int pixels) {
		OP_ASSERT(!"BlendLine() not supported");
	}

	OP_STATUS ScaleImage(
		XImage *src, XImage *dest,
		void *srcbuf, void *destbuf,
		int srcwidth, int srcheight, int srcbpl,
		int destwidth, int destheight, int destbpl,
		int xoffs, int yoffs,
		int dtotwidth, int dtotheight);
	bool IsChangeableColorModel() {
		return color_array && visualinfo.c_class == PseudoColor;
	}

	bool IsGeneric() { return true; }

	// Implementation specific methods:
	void GenerateReverseColorLookupArray();
};

#endif // X11_COLORMANAGER_GENERIC_H_
