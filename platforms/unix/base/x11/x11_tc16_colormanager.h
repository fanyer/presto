/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2002-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_COLORMANAGER_TC16_H_
#define X11_COLORMANAGER_TC16_H_

#include "x11_colormanager.h"


/** TrueColor 16 bit color manager.
 * Expected image format: 16 bits per pixel
 * Red mask:0xf800  Green mask:0x7e0 Blue mask:0x1f
 */
class X11TC16ColorManager : public X11ColorManager
{
private:
	bool swap_endianness;

public:
	X11TC16ColorManager(bool swap) { swap_endianness = swap; }

	unsigned long GetPixelValue(int red, int green, int blue, int alfa, const XColor **color=0);
	void GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha);

	void PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply);
	void GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply);
	void PutPixel(XImage *img, int x, int y, int r, int g, int b, int a);
	void GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a);
	void BlendLine(
		const void *src, void *dest,
		const UINT8 *srcalpha, UINT8 *destalpha, int pixels);
	OP_STATUS ScaleImage(
		XImage *src, XImage *dest,
		void *srcbuf, void *destbuf,
		int srcwidth, int srcheight, int srcbpl,
		int destwidth, int destheight, int destbpl,
		int xoffs, int yoffs,
		int dtotwidth, int dtotheight);
};

#endif // X11_COLORMANAGER_TC16_H_
