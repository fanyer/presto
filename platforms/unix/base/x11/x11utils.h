/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef __X11UTILS_H__
#define __X11UTILS_H__

#include "modules/util/simset.h"

#include "platforms/utilix/x11_all.h"


class X11ColorManager;
class DesktopWindow;

namespace X11Utils
{
	enum XlfdField
	{
		FONT_NAME_REGISTRY,
		FOUNDRY,
		FAMILY_NAME,
		WEIGHT_NAME,
		SLANT,
		SETWIDTH_NAME,
		ADD_STYLE_NAME,
		PIXEL_SIZE,
		POINT_SIZE,
		RESOLUTION_X,
		RESOLUTION_Y,
		SPACING,
		AVERAGE_WIDTH,
		CHAR_SET_REGISTRY,
		CHAR_SET_ENCODING,
		NUM_XLFD_FIELDS
	};

	/** Horrible class. Get rid of it. */
	class Int : public Link
	{
	public:
		int i;
		Int(int i) : i(i) { }
	};

	/** Structure useful for dithering. */
	struct ColorError
	{
		int r, g, b;
		inline void Adjust(int &red, int &green, int &blue) {
			red -= r;
			green -= g;
			blue -= b;
			
			if ((red | green | blue) & ~0xff)
			{
				if (red < 0) {
					r = -red;
					red = 0;
				}
				else if (red > 0xff) {
					r = red - 0xff;
					red = 0xff;
				}
				else r = 0;
				if (green < 0) {
					g = -green;
					green = 0;
				}
				else if (green > 0xff) {
					g = green - 0xff;
					green = 0xff;
				}
				else g = 0;
				if (blue < 0) {
					b = -blue;
					blue = 0;
				}
				else if (blue > 0xff) {
					b = blue - 0xff;
					blue = 0xff;
				}
				else b = 0;
			}
			else
			{
				r = 0;
				g = 0;
				b = 0;
			}
		}
		inline void AddError(int red, int green, int blue) {
			r += red;
			g += green;
			b += blue;
		}
	};


	/**
	 * Returns the DesktopWindow that corresponds to the X11 window
	 *
	 * @param x11_window X11 window handle
	 *
	 * @return A pointer to the DesktopWindow. Can be NULL
	 */
	DesktopWindow* GetDesktopWindowFromX11Window(X11Types::Window x11_window);

    /**
     * Returns the X11 window handle that corresponds to the DesktopWindow
     *
     * @param desktop_window A pointer to a DesktopWindow
     *
     * @return An X11 window handle. Can be NULL
     */
    X11Types::Window GetX11WindowFromDesktopWindow(DesktopWindow *desktop_window);

	/**
	 * Converts a standard X11 command line rectangle spec. of
	 * format: [<width>{xX}<height>][{+-}<xoffset>{+-}<yoffset>]
	 * to an OpRect. We will always constrain the size to the screen size
	 * but limit_width and limit_height can reduce it even more if the 
	 * input spec does not contain one or both of those values.
	 *
	 * @param argument Typically command line data
	 * @param limit_width Only used if width component of argument is missing.
	 *        Max. width will be set to a fraction of the screen size.
	 * @param limit_height Only used if height component of argument is missing.
	 *        Max. height will be set to a fraction of the screen size.
	 *
	 * @return The parsed values.
	 */
	OpRect ParseGeometry(const OpString8& argument, BOOL limit_width=TRUE, 
						 BOOL limit_height=TRUE);

	OP_STATUS StripFoundry(const char *name, OpString8 &family);
	OP_STATUS ExtractFoundry(const char *name, OpString8 &foundry);

	bool ValidateXlfd(const char *xlfd);
	const char *GetPointerFromXlfd(const char *xlfd, XlfdField field,
								   int *length=0);
	int GetIntValueFromXlfd(const char *xlfd, XlfdField field);

	/**
	 * Convert an XLFD string to a family name on the format
	 * "Familyname [Foundry]" (e.g. "Helvetica [Adobe]").
	 */
	OP_STATUS XlfdToFamily(const char *xlfd, OpString8 &family);

	bool IsXlfdMonospace(const char *xlfd);

	int WeightStringToCss(const char *str, int len);

	OP_STATUS SetSmoothSize(const char *xlfd, int pixelsize,
							OpString8 &smoothfont);
#ifndef X11_CLEAN_NAMESPACE
	XImage *CreateScaledAlphaMask(X11Types::Display *display, XImage *src,
								  double scale_x, double scale_y);
	OP_STATUS ScaleImage(XImage *src, XImage *dest, X11ColorManager *sman,
						 X11ColorManager *dman, int xoffs, int yoffs,
						 int dtotwidth, int dtotheight);
#endif // X11_CLEAN_NAMESPACE

#ifndef VEGA_OPPAINTER_SUPPORT
	OP_STATUS ScaleImage(const char *src, int src_bpl, int sWidth, int sHeight,
						 char *dst, int dst_bpl, int dWidth, int dHeight,
						 int depth, int bpp, bool byteswap, int xoffs, int yoffs,
						 int dtotwidth, int dtotheight);
#endif // !VEGA_OPPAINTER_SUPPORT

	bool ScaleMask(const uint8_t *src, int src_bpl, int sWidth, int sHeight,
				   uint8_t *dst, int dst_bpl, int dWidth, int dHeight,
				   int xoffs, int yoffs, int dtotwidth, int dtotheight);
}

#endif // __X11UTILS_H__
