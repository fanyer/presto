/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11utils.h"

#include "modules/encodings/encoders/encoder-utility.h"

#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/x11_colormanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

namespace X11Utils
{
	DesktopWindow* GetDesktopWindowFromX11Window(X11Types::Window x11_window)
	{
		DesktopWindow* dw = 0;

		X11Widget* widget = g_x11->GetWidgetList()->FindWidget(x11_window);
		if (widget)
		{  
			OpWindow* op_window = widget->GetOpWindow();
			if (op_window)
			{
				dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(op_window);
			}
		}
	
		return dw;
	}

	X11Types::Window GetX11WindowFromDesktopWindow(DesktopWindow *desktop_window)
	{
		if (!desktop_window)
			return 0;

		X11OpWindow* window = static_cast<X11OpWindow*>(desktop_window->GetOpWindow());
		if (window && !window->GetWidget())
		{
			/* This does happen for widgets/gadgets.  This is most likely
			 * a bug in the gadget code, but we'll try to recover here
			 * anyway.
			 */
			X11OpWindow * realwindow = window->GetNativeWindow();
			if (realwindow->IsToplevel())
				window = realwindow;
		};
		if (!window || !window->GetWidget())
			return 0;

		return window->GetWidget()->GetWindowHandle();
	}

	OpRect ParseGeometry(const OpString8& argument, BOOL limit_width, BOOL limit_height)
	{
		if (argument.IsEmpty())
			return OpRect();

		unsigned int min_width  = 200;
		unsigned int min_height = 200;
		unsigned int max_width  = g_x11->GetScreenWidth(g_x11->GetDefaultScreen());
		unsigned int max_height = g_x11->GetScreenHeight(g_x11->GetDefaultScreen());

		OpRect default_rectangle(0,0,max_width,max_height);

		if (limit_width)
		{
			if (default_rectangle.width >= 1600)
				default_rectangle.width = 1300;
			else if (default_rectangle.width >= 1024)
				default_rectangle.width = 900;
		}
		if (limit_height)
		{
			if (default_rectangle.height >= 768 )
				default_rectangle.height = (int)((float)default_rectangle.height / 1.15);
		}

		default_rectangle.x = (max_width - default_rectangle.width) / 2;
		default_rectangle.y = (max_height - default_rectangle.height) / 2;

		int x, y;
		unsigned int w, h;
		
		int m = XParseGeometry( argument.CStr(), &x, &y, &w, &h );
		if ((m & XValue) == 0)
			x = default_rectangle.x;
		
		if ((m & YValue) == 0)
			y = default_rectangle.y;
		
		if ((m & WidthValue) == 0)
			w = default_rectangle.width;
		
		if ((m & HeightValue) == 0)
			h = default_rectangle.height;

		w = MIN(w,max_width);
		h = MIN(h,max_height);
		w = MAX(w,min_width);
		h = MAX(h,min_height);
		
		OpRect result(x,y,w,h);
		return result;
	}

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	void lookup_sbcs_table(const unsigned char *sbcs_table, long tablelen,
						   uni_char character, char *out)
	{
		const unsigned char *found =
			(const unsigned char *) bsearch(
				&character, sbcs_table, tablelen / 3, 3, unichar_compare);
		*out = found ? found[2] : 0;
	}
#else
#warning "Some code below depends on having the table manager"
#endif // ENCODINGS_HAVE_TABLE_DRIVEN

	OP_STATUS StripFoundry(const char *name, OpString8 &family)
	{
		RETURN_IF_ERROR(family.Set(name));
		if(family.IsEmpty())
			return OpStatus::OK;
		char *ptr = family.CStr() + family.Length() - 1;
		if (*ptr != ']')
			return OpStatus::OK;

		int bracketlevel = 1;
		bool success = false;
		while (--ptr > family.CStr())
		{
			char c = *ptr;
			if (c == '[')
			{
				bracketlevel --;
				if (bracketlevel == 0)
				{
					success = true;
					break;
				}
			}
			else if (c == ']')
			{
				bracketlevel ++;
			}
		}
		ptr --;
		if (success && *ptr == ' ')
		{
			OpString8 tmp;
			RETURN_IF_ERROR(tmp.Set(family.CStr(), ptr - family.CStr()));
			RETURN_IF_ERROR(family.Set(tmp.CStr()));
		}

		return OpStatus::OK;
	}

	OP_STATUS ExtractFoundry(const char *name, OpString8 &foundry)
	{
		OpString8 family;
		RETURN_IF_ERROR(StripFoundry(name, family));
		int namelen = strlen(name);
		if (family.Length() + 3 < namelen)
		{
			int foundrylen = namelen - family.Length() - 3;
			return foundry.Set(name+family.Length()+2, foundrylen);
		}

		return foundry.Set(""); // really needs to be a "", not an Empty().
	}

	bool ValidateXlfd(const char *xlfd)
	{
		int dashcount = 0;
		const char *ptr = strchr(xlfd, '-');
		while (ptr)
		{
			dashcount ++;
			if (dashcount == PIXEL_SIZE || dashcount == POINT_SIZE ||
				dashcount == RESOLUTION_X || dashcount == RESOLUTION_Y)
			{
				int val = atoi(ptr+1);
				if (!val && ptr[1] != '0')
					return false;
			}

			ptr = strchr(ptr+1, '-');
		}
		return dashcount == 14;
	}

	const char *GetPointerFromXlfd(const char *xlfd, XlfdField field,
								   int *length)
	{
		const char *p = xlfd;
		for (int dash=0; p && dash<(int)field; dash++)
		{
			p = strchr(p, '-');
			if (p) p++;
			else break;
		}
		if (p && length)
		{
			const char *next = strchr(p, '-');
			if (next)
				*length = next - p;
			else
				*length = strlen(p);
		}
		return p;
	}

	int GetIntValueFromXlfd(const char *xlfd, XlfdField field)
	{
		const char *p = GetPointerFromXlfd(xlfd, field);
		if (!p)
			return -1;
		return atoi(p);
	}

	OP_STATUS XlfdToFamily(const char *xlfd, OpString8 &family)
	{
		int famlen;
		int foundrylen;
		const char *famptr = GetPointerFromXlfd(xlfd, FAMILY_NAME, &famlen);
		const char *foundryptr = GetPointerFromXlfd(xlfd, FOUNDRY, &foundrylen);

		if (famptr && foundryptr)
		{
			if (!family.Reserve(famlen + foundrylen + 4))
				return OpStatus::ERR_NO_MEMORY;
			char *cfam = family.CStr();
			memcpy(cfam, famptr, famlen);
			memcpy(cfam + famlen, " [", 2);
			memcpy(cfam + famlen + 2, foundryptr, foundrylen);
			memcpy(cfam + famlen + 2 + foundrylen, "]", 2); // include the '\0' terminator
			// Capitalize family and foundry names:
			cfam[0] = toupper(family[0]);
			cfam[famlen + 2] = toupper(family[famlen + 2]);
			return OpStatus::OK;
		}

		OP_ASSERT(!"Failed to find Xlfd");
		return family.Set(xlfd);
	}

	bool IsXlfdMonospace(const char *xlfd)
	{
		const char *p = GetPointerFromXlfd(xlfd, SPACING);
		if (p)
		{
			char c = tolower(*p);
			if (c == 'm' || c == 'c')
				return true;
		}
		return false;
	}

	int WeightStringToCss(const char *str, int len)
	{
		if (len >= 4)
		{
			char c = tolower(*str);
			switch (c)
			{
			case 'm': // medium
				return 4;
			case 'b':
			{
				if (strncasecmp(str+1, "ol", 2) == 0) // bold
					return 7;
				else if (strncasecmp(str+1, "la", 2) == 0) // black
					return 9;
				else if (strncasecmp(str+1, "oo", 2) == 0) // book
					return 4;
				break;
			}
			case 'r': // regular
				return 4;
			case 'd': // debibold or demi
				return 6;
			case 'l': // light
				return 2;
			}
		}
		return 0;
	}

	OP_STATUS SetSmoothSize(const char *xlfd, int pixelsize, OpString8 &smoothfont)
	{
		smoothfont.Empty();
		const char *p = xlfd;
		int dashcount = 0;
		while (*p)
		{
			if (*p == '-')
			{
				dashcount ++;
			}
			RETURN_IF_ERROR(smoothfont.Append(p, 1));
			p ++;
			if (dashcount == PIXEL_SIZE)
			{
				int this_size = atoi(p);
				if (this_size == 0 && *p == '0')
				{
					char tmp[20];
					sprintf(tmp, "%d-0-", pixelsize);
					RETURN_IF_ERROR(smoothfont.Append(tmp));
					p = strchr(p, '-');
					if (!p)
						break;
					p = strchr(p+1, '-');
					if (!p)
						break;
					p++;
					dashcount += 2;
				}
			}
		}
		return OpStatus::OK;
	}

	inline bool GetPixel(void *buf, int depth)
	{
		switch (depth)
		{
		case 8:
			return !!*(unsigned char *)buf;
		case 16:
			return !!*(unsigned short *)buf;
		case 24:
		{
			unsigned char *b = (unsigned char *)buf;
			return b[0] || b[1] || b[2];
		}
		case 32:
			return !!*(unsigned int *)buf;
		}

		OP_ASSERT(!"unsupported image depth");
		return 0;
	}

	XImage *CreateScaledAlphaMask(X11Types::Display *display, XImage *src, double scale_x, double scale_y)
	{
		int swidth = src->width;
		int sheight = src->height;
		int sdepth = src->bits_per_pixel;
		int sbpl = src->bytes_per_line;
		int dwidth = UnixUtils::CalculateScaledSize(swidth, scale_x);
		int dheight = UnixUtils::CalculateScaledSize(sheight, scale_y);
	
		// Set to zero to let XCreateImage() figure out value. Fix for bug #259515 
		int dbpl = 0; // dwidth;

		X11Types::Visual *vis = g_x11->GetX11Visual(0).visual;
		bool smaller = scale_y < 1.0;

		char *dest_data = (char *)malloc(dwidth * dheight);
		if (!dest_data)
			return 0;

		XImage *dest = XCreateImage(display, vis, 8, src->format, src->xoffset,
									dest_data, dwidth, dheight, 8/*src->bitmap_pad*/, dbpl);
		if (!dest)
		{
			free(dest_data);
			return 0;
		}

		int sumwidth = dwidth + (int)(scale_x*2.0) + 2;
		int sumheight = dheight + (int)(scale_y*2.0) + 2;
		int sumoffset_x = (int)(scale_x + 0.5);
		int sumoffset_y = (int)(scale_y + 0.5);
		double *pixelsums = OP_NEWA(double, sumwidth * sumheight);
		if (!pixelsums)
		{
			XDestroyImage(dest);
			return 0;
		}
		double *sumptr = pixelsums;
		for (int i=sumwidth * sumheight; i>0; i--)
			*sumptr++ = 0.0;

		double dy = scale_y;
		int bytes_per_pixel = sbpl / swidth;
		double radius = scale_y/1.2;

		for (int sy=0; sy<sheight; sy++, dy+=scale_y)
		{
			char *sptr = (char *)src->data + sbpl * sy;

			double y_thisweight;
			double y_nextweight;
			if (smaller)
			{
				if ((int)(dy+scale_y) != (int)dy)
					y_thisweight = ((double)((int)dy+1) - dy);
				else
					y_thisweight = scale_y;
				y_nextweight = scale_y - y_thisweight;
				if (sy == 0)
					y_thisweight = 1.0;
			}
			else
			{
				y_thisweight = ((double)((int)dy+1) - dy);
				y_nextweight = 1 - y_thisweight;
			}

			double dx = scale_x;
			for (int sx=0; sx<swidth; sx++, dx+=scale_x)
			{
				bool pixel = GetPixel(sptr, sdepth);
				sptr += bytes_per_pixel;
				if (!pixel)
					continue;

				double x_thisweight;
				double x_nextweight;
				if (smaller)
				{
					if ((int)(dx+scale_x) != (int)dx)
						x_thisweight = ((double)((int)dx+1) - dx);
					else
						x_thisweight = scale_x;
					x_nextweight = scale_x - x_thisweight;
					if (sx == 0)
						x_thisweight = 1.0;
				}
				else
				{
					x_thisweight = ((double)((int)dx+1) - dx);
					x_nextweight = 1 - x_thisweight;
				}

				int cy = (int)dy;
				int cx = (int)dx;

				if (smaller)
				{
					pixelsums[cy * sumwidth + cx]       += y_thisweight * x_thisweight;
					pixelsums[cy * sumwidth + cx+1]     += y_thisweight * x_nextweight;
					pixelsums[(cy+1) * sumwidth + cx]   += y_nextweight * x_thisweight;
					pixelsums[(cy+1) * sumwidth + cx+1] += y_nextweight * x_nextweight;
				}
				else
				{
					int end = (int)(dx + radius) - (int)cx;
					for (int y=0; y<=end; y++)
					{
						for (int x=0; x<=end; x++)
						{
							double distance = hypot(x, y);
							if (distance > radius)
								distance = radius;
							double value = 1.8*(radius-distance)/radius;

							if (value > 1.0)
								value = 1.0;
							if (value > 0.0)
							{
								double value1 = value * x_thisweight * y_thisweight;
								double valuex = value * x_nextweight * y_thisweight;
								double valuey = value * x_thisweight * y_nextweight;
								double valuexy = value * x_nextweight * y_nextweight;

#if 0
								OP_ASSERT(cy-y >= 0 && cx-x >= 0 &&
										  cy+y+1 < sumheight && cx+x+1 < sumwidth);
#endif
								pixelsums[(cy+y)*sumwidth+cx+x] += value1;
								pixelsums[(cy+y)*sumwidth+cx+x+1] += valuex;
								pixelsums[(cy+y+1)*sumwidth+cx+x] += valuey;
								pixelsums[(cy+y+1)*sumwidth+cx+x+1] += valuexy;

								if (x > 0)
								{
									pixelsums[(cy+y)*sumwidth+cx-x] += value1;
									pixelsums[(cy+y)*sumwidth+cx-x+1] += valuex;
									pixelsums[(cy+y+1)*sumwidth+cx-x] += valuey;
									pixelsums[(cy+y+1)*sumwidth+cx-x+1] += valuexy;
								}

								if (y > 0)
								{
									pixelsums[(cy-y)*sumwidth+cx+x] += value1;
									pixelsums[(cy-y)*sumwidth+cx+x+1] += valuex;
									pixelsums[(cy-y+1)*sumwidth+cx+x] += valuey;
									pixelsums[(cy-y+1)*sumwidth+cx+x+1] += valuexy;
								}

								if (x > 0 && y > 0)
								{
									pixelsums[(cy-y)*sumwidth+cx-x] += value1;
									pixelsums[(cy-y)*sumwidth+cx-x+1] += valuex;
									pixelsums[(cy-y+1)*sumwidth+cx-x] += valuey;
									pixelsums[(cy-y+1)*sumwidth+cx-x+1] += valuexy;
								}
							}
						}
					}
				}
			}
		}

		if (smaller)
		{
			for (int y=0; y<sumoffset_y; y++)
			{
				double *sptr = pixelsums + y * sumwidth;
				double *dptr = pixelsums + sumoffset_x * sumwidth;
				for (int x=0; x<sumwidth; x++)
					*dptr++ += *sptr++ / 2.0;
			}
			for (int y=sumoffset_y+dheight; y<sumheight; y++)
			{
				double *sptr = pixelsums + y * sumwidth;
				double *dptr = pixelsums + (sumoffset_x+dheight-1) * sumwidth;
				for (int x=0; x<sumwidth; x++)
					*dptr++ += *sptr++ / 2.0;
			}
			for (int y=sumoffset_y+dheight-1; y>=sumoffset_y; y--)
			{
				double *sptr = pixelsums + y * sumwidth;
				double *dptr = sptr + sumoffset_x;
				for (int x=0; x<sumoffset_x; x++)
					*dptr += *sptr++ / 2.0;
				sptr += dwidth;
				dptr += dwidth-1;
				for (int x=sumwidth-sumoffset_x; x<sumwidth; x++)
					*dptr += *sptr++ / 2.0;
			}
		}

		double max = 0.0;
		for (int y=0; y<dheight; y++)
		{
			sumptr = pixelsums + (y+sumoffset_y) * sumwidth + sumoffset_x;
			for (int x=0; x<dwidth; x++)
			{
				double res = *sumptr++ * 256.0;
				if (res > max)
					max = res;
			}
		}
		double gain;
		if (max < 256.0 && max > 1.0 )
			gain = 260.0 / max;
		else
			gain = 1.0;

		unsigned char *dptr = (unsigned char *)dest->data;
		for (int y=0; y<dheight; y++)
		{
			sumptr = pixelsums + (y+sumoffset_y) * sumwidth + sumoffset_x;
			for (int x=0; x<dwidth; x++)
			{
				unsigned int value = (int)(*sumptr++ * 256.0 * gain);
				if (value > 255)
					value = 255;

				if (value >= 128)
				{
					// Enhance opaque points (points with high alpha values)
					int x = 255-value;
					value = 255 - x*x/128;
				}
				*dptr++ = value;
			}
		}

		OP_DELETEA(pixelsums);

		return dest;
	}

#define __SMOOTHSCALING__
	OP_STATUS ScaleImage(XImage *src, XImage *dest, X11ColorManager *sman,
						 X11ColorManager *dman, int xoffs, int yoffs,
						 int dtotwidth, int dtotheight)
	{
		bool expandX = dtotwidth > src->width;
		bool expandY = dtotheight > src->height;
		int sy = yoffs * src->height / dtotheight;
		int dy = sy * dtotheight / src->height - yoffs;

		ColorError *yerr = 0;
		ColorError *xerr = 0;

		if (dest->depth == 8)
		{
			yerr = OP_NEWA(ColorError, dest->width);
			if (!yerr)
				return OpStatus::ERR_NO_MEMORY;
			xerr = OP_NEWA(ColorError, dtotheight / src->height + 1);
			if (!xerr)
			{
				OP_DELETEA(yerr);
				return OpStatus::ERR_NO_MEMORY;
			}
			memset(yerr, 0, dest->width*sizeof(ColorError));
		}

		for (;;)
		{
			int drows = expandY ? ((sy+1) * dtotheight / src->height) - (dy+yoffs) : 1;
			int srows = expandY ? 1 : (((dy+yoffs+1) * src->height + dtotheight-1) / dtotheight) - sy;

			if (drows + dy > dest->height)
				drows = dest->height - dy;
			if (srows + sy > src->height)
				srows = src->height - sy;
			if (drows <= 0 || srows <= 0)
				break;

			if (xerr)
				memset(xerr, 0, drows*sizeof(ColorError));

			int sx = xoffs * src->width / dtotwidth;
			int dx = sx * dtotwidth / src->width - xoffs;
			for (;;)
			{
				int dcols = expandX ? ((sx+1) * dtotwidth / src->width) - (dx+xoffs) : 1;
				int scols = expandX ? 1 : ((((dx+xoffs)+1) * src->width + dtotwidth-1) / dtotwidth) - sx;
				if (dcols + dx > dest->width)
					dcols = dest->width - dx;
				if (scols + sx > src->width)
					scols = src->width - sx;
				if (dcols <= 0 || scols <= 0)
					break;
#ifdef __SMOOTHSCALING__
				int a_total= 0, r_total=0, g_total=0, b_total=0;
				for (int yy=0; yy<srows; yy++)
				{
					for (int xx=0; xx<scols; xx++)
					{
						int a, r, g, b;
						sman->GetPixel(src, sx+xx, sy+yy, r, g, b, a);
						a_total += a;
						r_total += r;
						g_total += g;
						b_total += b;
					}
				}
				int numPixels = srows * scols;
				a_total /= numPixels;
				r_total /= numPixels;
				g_total /= numPixels;
				b_total /= numPixels;
#else
				int a_total, r_total, g_total, b_total;
				sman->GetPixel(src, sx, sy, r_total, g_total, b_total, a_total);
#endif // _SMOOTHSCALING_
				const XColor *xcolor = 0;
				unsigned long pixel = dman->GetPixelValue(r_total, g_total, b_total, a_total, &xcolor);
				for (int yy=0; yy<drows; yy++)
				{
					if (dy+yy < 0)
						continue;
					for (int xx=0; xx<dcols; xx++)
					{
						if (dx+xx < 0)
							continue;
						if (xcolor && xerr && yerr)
						{
							// Perform dithering
							int a=a_total, r=r_total, g=g_total, b=b_total;
							xerr[yy].Adjust(r, g, b);
							yerr[dx+xx].Adjust(r, g, b);
							pixel = dman->GetPixelValue(r, g, b, a, &xcolor);
							OP_ASSERT(xcolor);
							if (xcolor)
							{
								int r_err = (xcolor->red >> 8) - r;
								int g_err = (xcolor->green >> 8) - g;
								int b_err = (xcolor->blue >> 8) - b;
								int xr = r_err / 2;
								int xg = g_err / 2;
								int xb = b_err / 2;
								xerr[yy].AddError(xr, xg, xb);
								yerr[dx+xx].AddError(r_err-xr, g_err-xg, b_err-xb);
							}
						}
						XPutPixel(dest, dx+xx, dy+yy, pixel);
					}
				}
				dx += dcols;
				sx += scols;
			}
			dy += drows;
			sy += srows;
		}

		OP_DELETEA(yerr);
		OP_DELETEA(xerr);

		return OpStatus::OK;
	}

#ifndef VEGA_OPPAINTER_SUPPORT
	OP_STATUS ScaleImage(const char *src, int src_bpl, int sWidth, int sHeight,
						 char *dst, int dst_bpl, int dWidth, int dHeight,
						 int depth, int bpp, bool byteswap, int xoffs, int yoffs,
						 int dtotwidth, int dtotheight)
	{
		if (bpp != 8)
		{
			OP_ASSERT(!"Unsupported pixel depth");
			return OpStatus::ERR;
		}

		bool expandX = dtotwidth > sWidth;
		bool expandY = dtotheight > sHeight;
		int sy = yoffs * sHeight / dtotheight;
		int dy = sy * dtotheight / sHeight - yoffs;
		for (;;)
		{
			int drows = expandY ? ((sy+1) * dtotheight / sHeight) - (dy+yoffs) : 1;
			int srows = expandY ? 1 : (((dy+yoffs+1) * sHeight + dtotheight-1) / dtotheight) - sy;
			if (drows + dy > dHeight)
				drows = dHeight - dy;
			if (srows + sy > sHeight)
				srows = sHeight - sy;
			if (drows <= 0 || srows <= 0)
				break;

			int sx = xoffs * sWidth / dtotwidth;
			int dx = sx * dtotwidth / sWidth - xoffs;
			for (;;)
			{
				int dcols = expandX ? ((sx+1) * dtotwidth / sWidth) - (dx+xoffs) : 1;
				int scols = expandX ? 1 : ((((dx+xoffs)+1) * sWidth + dtotwidth-1) / dtotwidth) - sx;
				if (dcols + dx > dWidth)
					dcols = dWidth - dx;
				if (scols + sx > sWidth)
					scols = sWidth - sx;
				if (dcols <= 0 || scols <= 0)
					break;
#ifdef __SMOOTHSCALING__
				int r_total=0, g_total=0, b_total=0;
				for (int yy=0; yy<srows; yy++)
				{
					for (int xx=0; xx<scols; xx++)
					{
						unsigned int cur = *((const uint8_t *)src+(sy+yy)*src_bpl+sx+xx);
						int r, g, b, a;
						g_x11_colormanager->GetRGB(cur, r, g, b, a);
						r_total += r;
						g_total += g;
						b_total += b;
					}
				}
				int numPixels = srows * scols;
				r_total /= numPixels;
				g_total /= numPixels;
				b_total /= numPixels;
				unsigned int pixel = g_x11_colormanager->GetPixelValue(r_total, g_total, b_total, 0xFF);
#else
				unsigned int pixel = *((const uint8_t *)src+sy*src_bpl+sx);
#endif // _SMOOTHSCALING_
				for (int yy=0; yy<drows; yy++)
				{
					if (dy+yy < 0)
						continue;
					for (int xx=0; xx<dcols; xx++)
					{
						if (dx+xx < 0)
							continue;
						*((uint8_t *)dst+(dy+yy)*dst_bpl+dx+xx) = pixel;
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
#endif // !VEGA_OPPAINTER_SUPPORT
	
	bool ScaleMask(const uint8_t *src, int src_bpl, int sWidth, int sHeight,
				   uint8_t *dst, int dst_bpl, int dWidth, int dHeight,
				   int xoffs, int yoffs, int dtotwidth, int dtotheight)
	{
		bool expandX = dtotwidth > sWidth;
		bool expandY = dtotheight > sHeight;
		int sy = yoffs * sHeight / dtotheight;
		int dy = sy * dtotheight / sHeight - yoffs;
		for (;;)
		{
			int drows = expandY ? ((sy+1) * dtotheight / sHeight) - (dy+yoffs) : 1;
			int srows = expandY ? 1 : (((dy+yoffs+1) * sHeight + dtotheight-1) / dtotheight) - sy;
			if (drows + dy > dHeight)
				drows = dHeight - dy;
			if (srows + sy > sHeight)
				srows = sHeight - sy;
			if (drows <= 0 || srows <= 0)
				break;

			int sx = xoffs * sWidth / dtotwidth;
			int dx = sx * dtotwidth / sWidth - xoffs;

			for (;;)
			{
				int dcols = expandX ? ((sx+1) * dtotwidth / sWidth) - (dx+xoffs) : 1;
				int scols = expandX ? 1 : ((((dx+xoffs)+1) * sWidth + dtotwidth-1) / dtotwidth) - sx;
				if (dcols + dx > dWidth)
					dcols = dWidth - dx;
				if (scols + sx > sWidth)
					scols = sWidth - sx;
				if (dcols <= 0 || scols <= 0)
					break;
#ifdef __SMOOTHSCALING__
				bool pixel = true;
				for (int yy=0; yy<srows; yy++)
				{
					const uint8_t *sptr = src+(sy+yy)*src_bpl+(sx>>3);
					int mask = 1 << (sx & 7);
					uint8_t byte = *sptr ++;
					for (int xx=0; xx<scols; xx++)
					{
						if (mask == 0x100)
						{
							mask = 1;
							byte = *sptr ++;
						}
						if (!(byte & mask))
						{
							pixel = false;
							goto outer;
						}
						mask <<= 1;
					}
				}
			outer:
#else
				bool pixel = src[sy*src_bpl+(sx>>3)] & (1 << (sx & 7));
#endif
				for (int yy=0; yy<drows; yy++)
				{
					if (dy+yy < 0)
						continue;
					int xx = 0;
					if (dx+xx < 0)
						xx = -dx;
					if (xx < dcols)
					{
						uint8_t *dptr = dst+(dy+yy)*dst_bpl+((dx+xx)>>3);
						int mask = 1 << ((dx+xx) & 7);
						uint8_t byte = *dptr;

						for (; xx<dcols; xx++)
						{
							if (mask == 0x100)
							{
								mask = 1;
								*dptr++ = byte;
								byte = *dptr;
							}
							if (pixel)
								byte |= mask;
							else
								byte &= ~mask;
							mask <<= 1;
						}
						if (mask != 1)
							*dptr = byte;
					}
				}
				dx += dcols;
				sx += scols;
			}
			dy += drows;
			sy += srows;
		}

		return true;
	}
}
