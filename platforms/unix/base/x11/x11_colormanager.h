/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2002-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_COLORMANAGER_H_
#define X11_COLORMANAGER_H_

#include "platforms/utilix/x11_all.h"


/**
 * An abstract class for an X11 color manager. An implementation of this class
 * converts (both ways) between pixel values and red/green/blue color components.
 */
class X11ColorManager
{
public:
	virtual ~X11ColorManager() { }

	/**
	 * Find a suitable pixel value for the red/green/blue combination in the
	 * colormap. If Visual type is PseudoColor AND the color argument is
	 * non-NULL: return the pointer to the actual XColor object.
	 */
	virtual unsigned long GetPixelValue(int red, int green, int blue, int alfa, const XColor **color=0) = 0;
	/**
	 * Look up a pixel value in the colormap to find the corresponding red,
	 * green and blue color values.
	 */
	virtual void GetRGB(unsigned long pixel, int &red, int &green, int &blue, int &alpha) = 0;

	/** Set pixels in a specific line.
	 * The destination can be specified in two ways, and it is up to the
	 * implementation to use the most appropriate one: It may be specified
	 * as an XImage via the 'img' parameter, but this parameter may be NULL
	 * when the destination isn't an XImage, but e.g. a shared pixmap.
	 * Therefore there is a second a way of passing the destination buffer:
	 * via 'dest'. 'dest' will never be NULL.
	 *
	 * @param img The destination image. This parameter may be NULL in cases
	 * where the destination isn't an XImage.
	 * @param dest Destination buffer. Will never be NULL. Will point at the
	 * right line, so the 'line' parameter should be ignored if this parameter
	 * is used.
	 * @param data Source buffer, 32 bits per pixel, with the alpha byte as the
	 * highest byte, followed by the channels for red, green and blue.
	 * @param line Line number in the image to manipulate. This parameter
	 * should be ignored if the 'dest' parameter is used, but it is essential
	 * information if the 'img' parameter is used.
	 * @param pixels Number of pixels per line.
	 * @param premultiply Informs code to premultiply. Only used for ARGB visuals
	 */
	virtual void PutLine(XImage *img, void *dest, const UINT32 *data, int line, int pixels, bool premultiply) = 0;

	/** Get pixels in a specific line.
	 * The source can be specified in two ways, and it is up to the
	 * implementation to use the most appropriate one: It may be specified
	 * as an XImage via the 'img' parameter, but this parameter may be NULL
	 * when the source isn't an XImage, but e.g. a shared pixmap. Therefore
	 * there is a second a way of passing the source buffer: via 'src'. 'src'
	 * will never be NULL.
	 *
	 * @param img The destination image. This parameter may be NULL in cases
	 * where the destination isn't an XImage.
	 * @param src Source buffer. Will never be NULL. Will point at the right
	 * line, so the 'line' parameter should be ignored if this parameter
	 * is used.
	 * @param data Destination buffer, 32 bits per pixel, with the alpha byte
	 * as the highest byte, followed by the channels for red, green and blue.
	 * @param line Line number in the image to manipulate. This parameter
	 * should be ignored if the 'src' parameter is used, but it is essential
	 * information if the 'img' parameter is used.
	 * @param pixels Number of pixels per line.
	 * @param premultiply Informs code to premultiply. Only used for ARGB visuals
	 */
	virtual void GetLine(XImage *img, const void *src, UINT32 *data, int line, int pixels, bool premultiply) = 0;

	/** Set the pixel at the specific coordinate to the specified RGB value. */
	virtual void PutPixel(XImage *img, int x, int y, int r, int g, int b, int a) = 0;

	/** Get the ARGB value of the pixel at the specified coordinate. */
	virtual void GetPixel(XImage *img, int x, int y, int &r, int &g, int &b, int &a) = 0;

	/** Premultiply or de-premultiply a buffer. Returns true if atleast one pixel was modified
	 */
	virtual bool Premultiply(bool premultiply, void* data, unsigned int size) {return false;}

	/** Blend src with alpha mask on dest */
	virtual void BlendLine(
		const void *src, void *dest,
		const UINT8 *srcalpha, UINT8 *destalpha, int pixels) = 0;

	/** Put a scaled representation of one image buffer into another image
	 * buffer.
	 * The images can be specified in two ways, and it is up to the
	 * implementation to use the most appropriate one: They may be specified
	 * as two XImage pointers via the 'src' and 'dest' parameters, but these
	 * parameters may be NULL when the source or destination aren't XImage
	 * object, but e.g. shared pixmaps. Therefore there is a second a way of
	 * passing the source and destination buffers: via 'srcbuf' and 'destbuf'.
	 * 'srcbuf' and 'destbuf' will never be NULL.
	 * @param src Source image. May be NULL if the source isn't an XImage
	 * @param dest Destination image. May be NULL if the destination isn't an
	 * XImage.
	 * @param srcbuf Source buffer. WIll never be NULL.
	 * @param destbuf Destination buffer. Will never be NULL.
	 * @param srcwidth Image width for the source. May be deduced from the
	 * 'src' XImage too, if present
	 * @param srcheight Image height for the source. May be deduced from the
	 * 'src' XImage too, if present
	 * @param srcbpl Bytes per scanline in the source. May be deduced from the
	 * 'src' XImage too, if present
	 * @param destwidth Image width for the destination. May be deduced from
	 * the 'dest' XImage too, if present
	 * @param destheight Image height for the destination. May be deduced from
	 * the 'dest' XImage too, if present
	 * @param destbpl Bytes per scanline in the destination. May be deduced
	 * from the 'dest' XImage too, if present
	 * @param xoffs X offset. Specified in the coordinate system of the
	 * destination image.
	 * @param yoffs Y offset. Specified in the coordinate system of the
	 * destination image.
	 * @param dtotwidth Total width of the destination image if the entire
	 * 'src' were to be scaled. Needed to avoid rounding errors when scaling
	 * fragments of an image.
	 * @param dtotheight Total height of the destination image if the entire
	 * 'src' were to be scaled. Needed to avoid rounding errors when scaling
	 * fragments of an image.
	 */
	virtual OP_STATUS ScaleImage(
		XImage *src, XImage *dest,
		void *srcbuf, void *destbuf,
		int srcwidth, int srcheight, int srcbpl,
		int destwidth, int destheight, int destbpl,
		int xoffs, int yoffs,
		int dtotwidth, int dtotheight) = 0;

	/**
	 * Return true if the color model is indexed and the color cells may
	 * change. This is the case for e.g. PseudoColor visuals.
	 */
	virtual bool IsChangeableColorModel() { return false; }

	/** Return true if this is an instance of the generic color manager
	 * This method shouldn't be here (ugly), but right now it has to.
	 */
	virtual bool IsGeneric() { return false; }

	/**
	 * Returns true the if the alpha byte of a pixel can store the alpha
	 * value. This happpens if we use a 32 bit visual
	 *
	 */
	virtual bool HasAlphaSupport() const { return false; }
};

#endif // X11_COLORMANAGER_H_
