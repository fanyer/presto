/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef X11_COLORMANAGER_TC32_H_
#define X11_COLORMANAGER_TC32_H_

#include "x11_colormanager.h"


/** TrueColor 32 bit color manager.
 * Expected image format: 32 bits per pixel
 * Red mask:0xff0000  Green mask:0xff00 Blue mask:0xff
 */
class X11TC32ColorManager : public X11ColorManager
{
public:
	X11TC32ColorManager(bool swap, bool argb) { m_swap_endianness = swap; m_argb_visual = argb; }

	unsigned long GetPixelValue(int red, int green, int blue, int alfa, const XColor **color=0);
	void GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha);

	void PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply);
	void GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply);
	void PutPixel(XImage *img, int x, int y, int r, int g, int b, int a);
	void GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a);
	bool Premultiply(bool premultiply, void* data, unsigned int size);
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

	bool HasAlphaSupport() const { return m_argb_visual; }

private:
	bool m_swap_endianness;
	bool m_argb_visual;

};

#endif // X11_COLORMANAGER_TC32_H_
