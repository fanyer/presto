/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * File : OpSkinUtils.cpp
 *
 * Description : 
 */
#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/OpSkinUtils.h"

#include "modules/img/image.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/util/gen_math.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegarenderer.h"
#include "modules/display/vis_dev.h"
#endif // VEGA_SUPPORT

#ifdef SKIN_SKIN_COLOR_THEME


void OpSkinUtils::ConvertRGBToHSL(UINT32 color, double& h, double& s, double& l)
{
	double v;
	// hue is calcualted same way as in HSV
	ConvertRGBToHSV(color, h, s, v);

	//convert p to range [0,1]
	double r = ((color >> 16) & 0xff) / 255.0;
	double g = ((color >> 8) & 0xff) / 255.0;
	double b = (color & 0xff) / 255.0;

	double Max = MAX(MAX(r, g), b);
	double Min = MIN(MIN(r, g), b);

	// set lighteness
	l = Max/2 + Min/2;

	double c = Max - Min;
	if (c == 0)
		s = 0;
	else
		s = (l <= 0.5 ? c / (2 * l) : c / (2 - 2 * l));
}

UINT32 OpSkinUtils::ConvertHSLToRGB(double h, double s, double l)
{
	double R, G, B;
	R = G = B = 0.0;

	// this is conversion algorithm described in 
	// http://www.w3.org/TR/2008/WD-css3-color-20080721/#hsl-color	

	h = h <    0 ? 0 :
		h >  360 ? 360 :
		h;
	s = s <  0 ? 0 :
		s >  1 ? 1 :
		s;
	l = l <  0 ? 0 :
		l >  1 ? 1 :
		l;

	if (s == 0)
	{
		R = G = B = l;
	}
	else
	{
		double q = l < 0.5 ? l * (s + 1) : l + s - l * s;
		double p = l * 2 - q;

		h /= 360.0;

		double c[3]; 
		c[0] = h + 1.0/3.0;
		c[1] = h;
		c[2] = h - 1.0/3.0;

		for (int i = 0; i < 3; i++)
		{
			double t = c[i] < 0 ? c[i] + 1 : c[i] > 1 ? c[i] - 1 : c[i];
			c[i] = (t < 1.0/6.0) ? p + (q - p) * 6 * t :
				   (t < 1.0/2.0) ? q :
				   (t < 2.0/3.0) ? p + (q - p) * 6 * (2.0/3.0 - t) :
								   p;
		}

		R = c[0];
		G = c[1];
		B = c[2];
	}

	UINT8 r = (UINT8)OpRound(R * 255);
	UINT8 g = (UINT8)OpRound(G * 255);
	UINT8 b = (UINT8)OpRound(B * 255);

	return (r << 16) | (g << 8) | b;
}

void OpSkinUtils::ConvertRGBToHSV(UINT32 color, double& h, double& s, double& v)
{
	//convert p to range [0,1]
	double r = ((color >> 16) & 0xff) / 255.0;
	double g = ((color >> 8) & 0xff) / 255.0;
	double b = (color & 0xff) / 255.0;

	double Max = MAX(MAX(r, g), b);
	double Min = MIN(MIN(r, g), b);

	// set value/brightness v
	v = Max;

	// calculate saturation s
	if (Max != 0)
		s = (Max - Min) / Max;
	else
		s = 0;

	// calculate hue h
	if (s == 0)
		h = 0; // UNDEFINED!!
	else
	{
		double delta = Max - Min;
		if (r == Max)
			h = (g - b) / delta;		// resulting color is between yellow and magenta
		else if (g == Max)
			h = 2 + (b - r) / delta;	// resulting color is between cyan and yellow
		else if (b == Max)
			h = 4 + (r - g) / delta;	// resulting color is between magenta and cyan

		h *= 60;						// convert hue to degrees

		if (h < 0)						// make sure positive
			h += 360;
	}
}

UINT32 OpSkinUtils::ConvertHSVToRGB(double h, double s, double v)
{
	double R, G, B;

	R = G = B = 0;

	if (s == 0)
	{
		R = v;
		G = v;
		B = v;
	}
	else
	{
		int i;
		double f, p, q, t;

		if (h == 360)
			h = 0;
		h /= 60;			// h now in [0,6)
		i = int(op_floor(h));
		f = h - i;
		p = v * (1 - s);
		q = v * (1 - (s * f));
		t = v * (1 - (s * (1 - f)));

		switch (i)
		{
			case 0: R = v;
				    G = t;
					B = p;
					break;
			case 1: R = q;
				    G = v;
					B = p;
					break;
			case 2: R = p;
				    G = v;
					B = t;
					break;
			case 3: R = p;
				    G = q;
					B = v;
					break;
			case 4: R = t;
				    G = p;
					B = v;
					break;
			case 5: R = v;
				    G = p;
					B = q;
		}
	}

	UINT8 r = (UINT8)OpRound(R * 255);
	UINT8 g = (UINT8)OpRound(G * 255);
	UINT8 b = (UINT8)OpRound(B * 255);

	// don't set any alpha value
	return (r << 16) | (g << 8) | b;
}

void OpSkinUtils::ConvertBGRToHSV(UINT32 color, double& h, double& s, double& v)
{
	ConvertRGBToHSV(((color & 0xff) << 16) | (color & 0xff00) | ((color >> 16) & 0xff), h, s, v);
}

UINT32 OpSkinUtils::ConvertHSVToBGR(double h, double s, double v)
{
	UINT32 color = ConvertHSVToRGB(h, s, v);

	return (((color & 0xff) << 16) | (color & 0xff00) | ((color >> 16) & 0xff));
}

void OpSkinUtils::ColorizeHSV(double& h, double& s, double& v, double& colorize_h, double& colorize_s, double& colorize_v)
{
	if (colorize_s == 0)
	{
		s = 0;
	}
	else
	{
		h = colorize_h;
		if (colorize_s > s)
		{
			s += (colorize_s - s) * (1 - v);
		
			if (s < 0)
				s = 0;
			else if (s > 1)
				s = 1;
		}
		else
			s = colorize_s;
	}
}

UINT32 OpSkinUtils::ColorizePixel(UINT32 color, double colorize_h, double colorize_s, double colorize_v)
{
	double h, s, v;

	ConvertRGBToHSV(color, h, s, v);

	ColorizeHSV(h, s, v, colorize_h, colorize_s,  colorize_v);

	return (color & 0xff000000) | ConvertHSVToRGB(h, s, v);
}
 
UINT32 OpSkinUtils::ColorizePixelDarker(UINT32 color, double colorize_h, double colorize_s, double colorize_v)
{
	UINT8 r = static_cast<UINT8>(((color >> 16) & 0xff) * 0.95);
	UINT8 g = static_cast<UINT8>(((color >> 8) & 0xff) * 0.95);
	UINT8 b = static_cast<UINT8>((color & 0xff) * 0.95);

	return ColorizePixel((color & 0xff000000) | (r << 16) | (g << 8) | b, colorize_h, colorize_s, colorize_v);
}

#endif // SKIN_SKIN_COLOR_THEME

void OpSkinUtils::CopyBitmap(OpBitmap* src_bitmap, OpBitmap* dst_bitmap)
{
	INT32 width = src_bitmap->Width();
	INT32 height = src_bitmap->Height();

	UINT32* pixels = OP_NEWA(UINT32, width);
	if (!pixels)
		return;

	INT32 y;

	for (y = 0; y < height; y++)
	{
		src_bitmap->GetLineData(pixels, y);
		dst_bitmap->AddLine(pixels, y);
	}

	OP_DELETEA(pixels);
}


void OpSkinUtils::ScaleBitmap(OpBitmap* src_bitmap, OpBitmap* dst_bitmap)
{
#ifdef VEGA_SUPPORT
	VEGARenderer renderer;
	RETURN_VOID_IF_ERROR(renderer.Init(dst_bitmap->Width(), dst_bitmap->Height()));
	VEGAFill* img;
	VEGARenderTarget* rt;
	VEGATransform trans;
	VEGATransform itrans;
	trans.loadScale(VEGA_INTTOFIX(dst_bitmap->Width())/src_bitmap->Width(), VEGA_INTTOFIX(dst_bitmap->Height())/src_bitmap->Height());
	itrans.loadScale(VEGA_INTTOFIX(src_bitmap->Width())/dst_bitmap->Width(), VEGA_INTTOFIX(src_bitmap->Height())/dst_bitmap->Height());
	RETURN_VOID_IF_ERROR(renderer.createImage(&img, src_bitmap));
	if (OpStatus::IsError(renderer.createBitmapRenderTarget(&rt, dst_bitmap)))
	{
		OP_DELETE(img);
		return;
	}
	renderer.setRenderTarget(rt);
	img->setTransform(trans, itrans);
	renderer.setFill(img);
	renderer.clear(0, 0, dst_bitmap->Width(), dst_bitmap->Height(), 0);
	renderer.fillRect(0,0,dst_bitmap->Width(), dst_bitmap->Height());
	renderer.setFill(NULL);
	renderer.setRenderTarget(NULL);
	OP_DELETE(img);
	VEGARenderTarget::Destroy(rt);
#else // !VEGA_SUPPORT
#define GetPixel(x, y)			src_pixels[src_width * y + x]
#define PutPixel(x, pixel)		dst_pixels[x] = pixel
#define GetAlpha(pixel)			((pixel >> 24) & 0xff)
#ifdef USE_PREMULTIPLIED_ALPHA
#define GetBlue(pixel)			(GetAlpha(pixel)?((((pixel >> 16) & 0xff)*255)/GetAlpha(pixel)):0)
#define GetGreen(pixel)			(GetAlpha(pixel)?((((pixel >> 8) & 0xff)*255)/GetAlpha(pixel)):0)
#define GetRed(pixel)			(GetAlpha(pixel)?(((pixel & 0xff)*255)/GetAlpha(pixel)):0)
#define MakePixel(a, b, g, r)	(a << 24 | ((b*a)/255) << 16 | ((g*a)/255) << 8 | ((r*a)/255))
#else
#define GetBlue(pixel)			((pixel >> 16) & 0xff)
#define GetGreen(pixel)			((pixel >> 8) & 0xff)
#define GetRed(pixel)			(pixel & 0xff)
#define MakePixel(a, b, g, r)	(a << 24 | b << 16 | g << 8 | r)
#endif // PREMULTIPLIED_ALPHA

	INT32 src_width = src_bitmap->Width();
	INT32 src_height = src_bitmap->Height();

	INT32 dst_width = dst_bitmap->Width();
	INT32 dst_height = dst_bitmap->Height();

	UINT32* src_pixels = OP_NEWA(UINT32, src_width * src_height);
	if (!src_pixels)
		return;

	UINT32* dst_pixels = OP_NEWA(UINT32, dst_width);
	if (dst_pixels)
	{
		INT32 x,y;

		for (y = 0; y < src_height; y++)
		{
			src_bitmap->GetLineData(src_pixels + src_width * y, y);
		}

		double x_factor = (double) src_width /	(double) dst_width;
		double y_factor = (double) src_height / (double) dst_height;

		for (y = 0; y < dst_height; y++)
		{
			for (x = 0; x < dst_width; x++)
			{
				INT32 floor_x = (INT32) op_floor(x * x_factor);
				INT32 floor_y = (INT32) op_floor(y * y_factor);

				INT32 ceil_x = floor_x + 1;

				if (ceil_x >= src_width)
					ceil_x = floor_x;

				INT32 ceil_y = floor_y + 1;

				if (ceil_y >= src_height)
					ceil_y = floor_y;

				double fraction_x = x * x_factor - floor_x;
				double fraction_y = y * y_factor - floor_y;

				double one_minus_x = 1.0 - fraction_x;
				double one_minus_y = 1.0 - fraction_y;

				UINT32 p1 = GetPixel(floor_x, floor_y);
				UINT32 p2 = GetPixel(ceil_x, floor_y);
				UINT32 p3 = GetPixel(floor_x, ceil_y);
				UINT32 p4 = GetPixel(ceil_x, ceil_y);

				UINT8 a1 = GetAlpha(p1);
				UINT8 a2 = GetAlpha(p2);
				UINT8 a3 = GetAlpha(p3);
				UINT8 a4 = GetAlpha(p4);

				UINT8 b1 = GetBlue(p1);
				UINT8 b2 = GetBlue(p2);
				UINT8 b3 = GetBlue(p3);
				UINT8 b4 = GetBlue(p4);

				UINT8 g1 = GetGreen(p1);
				UINT8 g2 = GetGreen(p2);
				UINT8 g3 = GetGreen(p3);
				UINT8 g4 = GetGreen(p4);

				UINT8 r1 = GetRed(p1);
				UINT8 r2 = GetRed(p2);
				UINT8 r3 = GetRed(p3);
				UINT8 r4 = GetRed(p4);

				// if any pixels are fully transparent, their RGB value is undefined, so we use
				// average of any of others instead.

				INT32 total_b = 0;
				INT32 total_g = 0;
				INT32 total_r = 0;
				INT32 count = 0;

				if (a1)
				{
					total_b += b1;
					total_g += g1;
					total_r += r1;
					count++;
				}
				if (a2)
				{
					total_b += b2;
					total_g += g2;
					total_r += r2;
					count++;
				}
				if (a3)
				{
					total_b += b3;
					total_g += g3;
					total_r += r3;
					count++;
				}
				if (a4)
				{
					total_b += b4;
					total_g += g4;
					total_r += r4;
					count++;
				}

				if (count)
				{
					total_b /= count;
					total_g /= count;
					total_r /= count;

					if (!a1)
					{
						b1 = total_b;
						g1 = total_g;
						r1 = total_r;
					}
					if (!a2)
					{
						b2 = total_b;
						g2 = total_g;
						r2 = total_r;
					}
					if (!a3)
					{
						b3 = total_b;
						g3 = total_g;
						r3 = total_r;
					}
					if (!a4)
					{
						b4 = total_b;
						g4 = total_g;
						r4 = total_r;
					}
				}

				UINT8 a12 = (UINT8)	(one_minus_x * a1 + fraction_x * a2);
				UINT8 a34 = (UINT8)	(one_minus_x * a3 + fraction_x * a4);
				UINT8 alpha = (UINT8)	(one_minus_y * (double)(a12) + fraction_y * (double)(a34));

				b1 = (UINT8)	(one_minus_x * b1 + fraction_x * b2);
				b2 = (UINT8)	(one_minus_x * b3 + fraction_x * b4);
				UINT8 blue = (UINT8)	(one_minus_y * (double)(b1) + fraction_y * (double)(b2));

				g1 = (UINT8)	(one_minus_x * g1 + fraction_x * g2);
				g2 = (UINT8)	(one_minus_x * g3 + fraction_x * g4);
				UINT8 green = (UINT8)	(one_minus_y * (double)(g1) + fraction_y * (double)(g2));

				r1 = (UINT8)	(one_minus_x * r1 + fraction_x * r2);
				r2 = (UINT8)	(one_minus_x * r3 + fraction_x * r4);
				UINT8 red = (UINT8)	(one_minus_y * (double)(r1) + fraction_y * (double)(r2));

				UINT32 pixel = MakePixel(alpha, blue, green, red);

				PutPixel(x, pixel);
			}

			dst_bitmap->AddLine(dst_pixels, y);
		}

		OP_DELETEA(dst_pixels);
	}
	OP_DELETEA(src_pixels);
#endif // VEGA_SUPPORT
}

OpBitmap* OpSkinUtils::MirrorBitmap(OpBitmap* src_bitmap)
{
	const INT32 width = src_bitmap->Width();
	const INT32 height = src_bitmap->Height();

	OpBitmap* dst_bitmap_ptr = NULL;
	RETURN_VALUE_IF_ERROR(OpBitmap::Create(&dst_bitmap_ptr, width, height,
				src_bitmap->IsTransparent(), src_bitmap->HasAlpha()), NULL);
	OpAutoPtr<OpBitmap> dst_bitmap(dst_bitmap_ptr);

	OpAutoArray<UINT32> line(OP_NEWA(UINT32, width));
	if (line.get() == NULL)
		return NULL;

	for (INT32 y = 0; y < height; ++y)
	{
		if (!src_bitmap->GetLineData(line.get(), y))
			return NULL;
		for (INT32 x = 0; x < width / 2; ++x)
			op_swap(line[x], line[width - x - 1]);
		RETURN_VALUE_IF_ERROR(dst_bitmap->AddLine(line.get(), y), NULL);
	}

	return dst_bitmap.release();
}

INT32 OpSkinUtils::GetAverageColorOfBitmap(OpBitmap *bmp)
{
	INT32 avg_color = OP_RGB(0, 0, 0);
	if(bmp->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		UINT32* data = (UINT32*)bmp->GetPointer(OpBitmap::ACCESS_READONLY);

		UINT32 sum_alpha = 0;
		UINT32 sum_red = 0;
		UINT32 sum_green = 0;
		UINT32 sum_blue = 0;

		// limit the sizes a bit
		UINT32 pixels = MIN(bmp->Height(), 1280) * MIN(bmp->Width(), 1024);

		if (data)
		{
			for (unsigned int y = 0; y < pixels; ++y)
			{
				sum_alpha	+= (*data >> 24) & 0xff;
				sum_red		+= (*data >> 16) & 0xff;
				sum_green	+= (*data >> 8) & 0xff;
				sum_blue	+= *data & 0xff;
				data++;
			}
			bmp->ReleasePointer();
		}
		sum_alpha	/= pixels;
		sum_red		/= pixels;
		sum_green	/= pixels;
		sum_blue	/= pixels;
		if (sum_alpha > 127)
			avg_color = OP_RGB(sum_red, sum_green, sum_blue);
	}
	return avg_color;
}


#endif // SKIN_SUPPORT
