/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/display/colconv.h"

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"

#include "modules/img/image.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#endif

typedef UINT32 FIXED1616;
#define TO_FIXED(a) (FIXED1616)(a * 65536)
#define FROM_FIXED(a) (UINT32(a) >> 16)

#ifdef DISPLAY_FALLBACK_PAINTING

#ifdef USE_PREMULTIPLIED_ALPHA
#error "Does not support premultiplied alpha yet"
#endif // USE_PREMULTIPLIED_ALPHA

/**
// Contains a system wich examines the painter and bitmaps supported functions
// and selects the fastest way to do things. (f.eks. blitting with alpha)

// If you gets a crash in this file send me a mail (emil@opera.com)

// Uses 16.16 fixedpoint to speed things up. Especially on older machines with slow FPU's
*/

typedef void (*BlitFunctionScaled)(UINT32* src, UINT32* dst, UINT32 len, FIXED1616 dx);
typedef void (*BlitFunction)(UINT32* src, UINT32* dst, UINT32 len);

// == 8 bit deph functions =============================================

// 8bpp to 8bpp
void BlitHLineCopyScaled8(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT8* src8 = (UINT8*) src32;
	UINT8* dst8 = (UINT8*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		*dst8 = src8[FROM_FIXED(fx)];
		dst8++;
		fx += dx;
	}
}


// == 16 bit deph functions (BGR 565) =============================================

// 16bpp to 16bpp
void BlitHLineCopyScaled16(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT16* src16 = (UINT16*) src32;
	UINT16* dst16 = (UINT16*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		*dst16 = src16[FROM_FIXED(fx)];
		dst16++;
		fx += dx;
	}
}

// 16bpp to 16bpp
void BlitHLineCopy16(UINT32* src32, UINT32* dst32, UINT32 len)
{
	op_memcpy(dst32, src32, len * 2);
}

// 32bpp to 16bpp
void BlitHLineAlphaScaled16(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT16* dst16 = (UINT16*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT8* srccol = (UINT8*) &src32[FROM_FIXED(fx)];
		UINT32 alpha = srccol[3];
		UINT8 dstcol[3] = { COL_565_R(*dst16), COL_565_G(*dst16), COL_565_B(*dst16) };
		dstcol[0] = dstcol[0] + (alpha * (srccol[0] - dstcol[0])>>8); // Blue
		dstcol[1] = dstcol[1] + (alpha * (srccol[1] - dstcol[1])>>8); // Green
		dstcol[2] = dstcol[2] + (alpha * (srccol[2] - dstcol[2])>>8); // Red
		*dst16 = COL_8888_TO_565( (*((UINT32*)&dstcol[0])) );
		dst16++;
		fx += dx;
	}
}

// 32bpp to 16bpp
void BlitHLineAlpha16(UINT32* src32, UINT32* dst32, UINT32 len)
{
	UINT8* src = (UINT8*) src32;
	UINT16* dst16 = (UINT16*) dst32;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT8 dstcol[3] = { COL_565_R(*dst16), COL_565_G(*dst16), COL_565_B(*dst16) };
		UINT32 alpha = src[3];
		dstcol[0] = dstcol[0] + (alpha * (src[0] - dstcol[0])>>8); // Blue
		dstcol[1] = dstcol[1] + (alpha * (src[1] - dstcol[1])>>8); // Green
		dstcol[2] = dstcol[2] + (alpha * (src[2] - dstcol[2])>>8); // Red
		*dst16 = COL_8888_TO_565( (*((UINT32*)&dstcol[0])) );
		dst16++;
		src += 4;
	}
}

// 32bpp to 16bpp
void BlitHLineTransparentScaled16(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT16* dst16 = (UINT16*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 srccol = src32[FROM_FIXED(fx)];
		if (((UINT8*)&srccol)[3]) // Check the alpha value
			dst16[i] = COL_8888_TO_565( srccol );
		fx += dx;
	}
}

// 32bpp to 16bpp
void BlitHLineTransparent16(UINT32* src32, UINT32* dst32, UINT32 len)
{
	UINT16* dst16 = (UINT16*) dst32;
	for(UINT32 i = 0; i < len; i++)
	{
		if (((UINT8*)src32)[3]) // Check the alpha value
			dst16[i] = COL_8888_TO_565( *src32 );
		src32 ++;
	}
}

// == 24 bit deph functions (BGR) =============================================

// 24bpp to 24bpp
void BlitHLineCopyScaled24(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT8* dst = (UINT8*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 col = src32[FROM_FIXED(fx)];
		dst[0] = ((UINT8*)&col)[0];
		dst[1] = ((UINT8*)&col)[1];
		dst[2] = ((UINT8*)&col)[2];
		dst += 3;
		fx += dx;
	}
}

// 24bpp to 24bpp
void BlitHLineCopy24(UINT32* src32, UINT32* dst32, UINT32 len)
{
	op_memcpy(dst32, src32, len * 3);
}

// 32bpp to 24bpp
void BlitHLineAlphaScaled24(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT8* dst = (UINT8*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT8* srccol = (UINT8*) &src32[FROM_FIXED(fx)];
		UINT32 alpha = srccol[3];
		dst[0] = dst[0] + (alpha * (srccol[0] - dst[0])>>8); // Blue
		dst[1] = dst[1] + (alpha * (srccol[1] - dst[1])>>8); // Green
		dst[2] = dst[2] + (alpha * (srccol[2] - dst[2])>>8); // Red
		dst += 3;
		fx += dx;
	}
}

// 32bpp to 24bpp
void BlitHLineAlpha24(UINT32* src32, UINT32* dst32, UINT32 len)
{
	UINT8* src = (UINT8*) src32;
	UINT8* dst = (UINT8*) dst32;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 alpha = src[3];
		dst[0] = dst[0] + (alpha * (src[0] - dst[0])>>8); // Blue
		dst[1] = dst[1] + (alpha * (src[1] - dst[1])>>8); // Green
		dst[2] = dst[2] + (alpha * (src[2] - dst[2])>>8); // Red
		dst += 3;
		src += 4;
	}
}

// 32bpp to 24bpp
void BlitHLineTransparentScaled24(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	UINT8* dst = (UINT8*) dst32;
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 col = src32[FROM_FIXED(fx)];
		if (((UINT8*)&col)[3]) // Check the alpha value
		{
			dst[0] = ((UINT8*)&col)[0];
			dst[1] = ((UINT8*)&col)[1];
			dst[2] = ((UINT8*)&col)[2];
		}
		dst += 3;
		fx += dx;
	}
}

// 32bpp to 24bpp
void BlitHLineTransparent24(UINT32* src32, UINT32* dst32, UINT32 len)
{
	UINT8* src8 = (UINT8*) src32;
	UINT8* dst8 = (UINT8*) dst32;
	for(UINT32 i = 0; i < len; i++)
	{
		if (src8[3]) // Check the alpha value
		{
			dst8[0] = src8[0];
			dst8[1] = src8[1];
			dst8[2] = src8[2];
		}
		dst8 += 3;
		src8 += 4;
	}
}

// == 32 bit deph functions (BGRA) =============================================

// 32bpp to 32bpp
void BlitHLineCopyScaled32(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		dst32[i] = src32[FROM_FIXED(fx)];
		fx += dx;
	}
}

// 32bpp to 32bpp
void BlitHLineCopy32(UINT32* src32, UINT32* dst32, UINT32 len)
{
	op_memcpy(dst32, src32, len * 4);
}

// 32bpp to 32bpp
void BlitHLineAlphaScaled32(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	// Endianness-independent:
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 *srccol = &src32[FROM_FIXED(fx)];
		int alpha = *srccol >> 24;

		unsigned char d = *dst32;
		UINT32 blue = d + (alpha * ((*srccol & 0xff) - d)>>8);
		d = *dst32 >> 8;
		UINT32 green = d + (alpha * (((*srccol >> 8) & 0xff) - d)>>8);
		d = *dst32 >> 16;
		UINT32 red = d + (alpha * (((*srccol >> 16) & 0xff) - d)>>8);
        d = *dst32 >> 24;
        alpha        = d + (alpha * ((              0xff) - d)>>8);

		*dst32++ = blue | (green << 8) | (red << 16) | (alpha << 24);
		fx += dx;
	}
}

#ifdef REALLY_UGLY_COLOR_HACK

// 32bpp to 32bpp
void BlitHLineAlpha32UglyColorHack(UINT32* src32, UINT32* dst32, UINT32 len)
{
	// Endianness-independent way of doing it:
	for (UINT32 i=0; i<len; i++)
	{
        UINT32 src = *src32++;
        int alpha = src >> 24;
        
		if (*dst32 == REALLY_UGLY_COLOR)
		{
			if (alpha)
			{
				*dst32 = src;
			}
			*dst32++;
		}
		else
		{
			unsigned char d = *dst32;
			UINT32 blue  = d + (alpha * (((src      ) & 0xff) - d)>>8);
			d = *dst32 >>  8;
			UINT32 green = d + (alpha * (((src >>  8) & 0xff) - d)>>8);
			d = *dst32 >> 16;
			UINT32 red   = d + (alpha * (((src >> 16) & 0xff) - d)>>8);
			d = *dst32 >> 24;
			alpha        = d + (alpha * ((              0xff) - d)>>8);
        
			*dst32++ = blue | (green << 8) | (red << 16) | (alpha << 24);
		}
	}
}

#endif // REALLY_UGLY_COLOR_HACK

// 32bpp to 32bpp
void BlitHLineAlpha32(UINT32* src32, UINT32* dst32, UINT32 len)
{
	// Endianness-independent way of doing it:
	for (UINT32 i=0; i<len; i++)
	{
        UINT32 src = *src32++;
        int alpha = src >> 24;
        
        unsigned char d = *dst32;
        UINT32 blue  = d + (alpha * (((src      ) & 0xff) - d)>>8);
        d = *dst32 >>  8;
        UINT32 green = d + (alpha * (((src >>  8) & 0xff) - d)>>8);
        d = *dst32 >> 16;
        UINT32 red   = d + (alpha * (((src >> 16) & 0xff) - d)>>8);
        d = *dst32 >> 24;
        alpha        = d + (alpha * ((              0xff) - d)>>8);
        
        *dst32++ = blue | (green << 8) | (red << 16) | (alpha << 24);
	}
}


// 32bpp to 32bpp
void BlitHLineTransparentScaled32(UINT32* src32, UINT32* dst32, UINT32 len, FIXED1616 dx)
{
	// Little-endian:
	FIXED1616 fx = 0;
	for(UINT32 i = 0; i < len; i++)
	{
		UINT32 srccol = src32[FROM_FIXED(fx)];
		// Wich one is fastest ?
		if (srccol & 0xFF000000) // Check the alpha value, works on all machines
			dst32[i] = srccol;
		fx += dx;
	}
}

// 32bpp to 32bpp
void BlitHLineTransparent32(UINT32* src32, UINT32* dst32, UINT32 len)
{
	for(UINT32 i = 0; i < len; i++)
	{
		// Wich one is fastest ?
		if (*src32 & 0xFF000000) // Check the alpha value, works on all machines
			*dst32 = *src32;
		dst32 ++;
		src32 ++;
	}
}

// Blits srcbitmap to dstbitmap using the given blitfunction.

void DrawBitmapClipped(OpBitmap* srcbitmap, OpBitmap* dstbitmap,
					   const OpRect &source,
					   const OpPoint &destpoint,
					   BlitFunction bltfunc)
{
	UINT8* line = (UINT8*) dstbitmap->GetPointer();
	UINT8* src = (UINT8*) srcbitmap->GetPointer();
	OP_ASSERT(line != NULL);
	if(line == NULL)
	{
		return; // Should not happen!
	}
	int src_bpp = srcbitmap->GetBpp();
	int dst_bpp = dstbitmap->GetBpp();
	int src_bypp = src_bpp == 15 ? 2 : src_bpp >> 3;
	int dst_bypp = dst_bpp == 15 ? 2 : dst_bpp >> 3;
	int x = destpoint.x;
	int y = destpoint.y;
	int src_bpl = srcbitmap->GetBytesPerLine();
	src = &src[source.x * src_bypp + (source.y * src_bpl)]; // Move sourcepointer to match sourcerect
	int xlen = source.width;
	if(x < 0) // Clip to the left
	{
		src += (0-x) * src_bypp;
		xlen -= 0-x;
		x = 0;
	}
	if (x < (int)dstbitmap->Width())
	{
		if (x + xlen > (int)dstbitmap->Width()) // Clip to the right
		{
			xlen -= (x + xlen) - dstbitmap->Width();
		}
		// Run from top to bottom and call the horizontal blitfunction each row
		int dst_h = dstbitmap->Height();
		int dst_bpl = dstbitmap->GetBytesPerLine();
		for (int i = 0; i < source.height; i++)
		{
			if (i + y >= 0 && i + y < dst_h) // Clipping
			{
				UINT32* dstpek = (UINT32*) &line[x * dst_bypp + (y + i) * dst_bpl];
				UINT32* srcpek = (UINT32*) src;
				bltfunc(srcpek, dstpek, xlen);
			}
			src += src_bpl;
		}
	}

	dstbitmap->ReleasePointer();
	srcbitmap->ReleasePointer();
}

// Blits srcbitmap to dstbitmap using the given blitfunction.

void DrawBitmapClippedScaled(OpBitmap* srcbitmap, OpBitmap* dstbitmap,
							 const OpRect &source,
							 const OpRect &dest,
							 BlitFunctionScaled bltfunc)
{
	UINT8* line = (UINT8*) dstbitmap->GetPointer();
	UINT8* src = (UINT8*) srcbitmap->GetPointer();
	int src_bypp = srcbitmap->GetBpp() == 15 ? 2 : srcbitmap->GetBpp() >> 3;
	int dst_bypp = dstbitmap->GetBpp() == 15 ? 2 : dstbitmap->GetBpp() >> 3;
	int x = dest.x;
	int y = dest.y;
	int src_bpl = srcbitmap->GetBytesPerLine();
	src = &src[source.x * src_bypp + source.y * src_bpl]; // Move sourcepointer to match sourcerect
	int xlen = dest.width;
	int subpixelsize = 0;
	int dst_w = dstbitmap->Width();
	int dst_h = dstbitmap->Height();
	if (x < 0) // Clip to the left
	{
		int oldx = x;
		src += int(float(-x) * float(source.width)/float(dest.width)) * src_bypp;
		xlen -= -x;
		x = 0;

		// Calculate subpixel accurcy. (If an image is uupscaled and positioned on x<0, the first pixel may
		// be smaller than the rest. Theirfore we need a special case for the first pixel each row.
		float segmentsize = float(dest.width)/float(source.width);
		float xtemp = float(-oldx) / segmentsize;
		subpixelsize = (int)(segmentsize - float(xtemp - int(xtemp)) * segmentsize);
		subpixelsize+=2;
		if(subpixelsize == int(segmentsize))
			subpixelsize = 0;
	}
	if(x >= dst_w) // Extreme case
	{
		dstbitmap->ReleasePointer();
		srcbitmap->ReleasePointer();
		return;
	}

	if (x + xlen > dst_w) // Clip to the right
	{
		xlen -= (x + xlen) - dst_w;
	}
	FIXED1616 sy = 0; // Where we are and reads pixels
	FIXED1616 dx = (source.width << 16) / dest.width; //Calculate the values we must step with, in order to get
	FIXED1616 dy = (source.height << 16) / dest.height; //the right pixels from the sourcebitmap.
	// Run from top to bottom and call the horizontal blitfunction each row
	int dst_bpl = dstbitmap->GetBytesPerLine();
	for (int i = 0; i < dest.height; i++)
	{
		if (i + y >= 0 && i + y < dst_h) // Clipping
		{
			int xofs = 0;
			int srcxofs = 0;
			int pixels_to_blit = xlen;
			BOOL skip_rest = FALSE;
			if(subpixelsize != 0) // If the first pixel is smaller than the others case
			{
				// Make sure we don't blit more than we should
				int sublength = subpixelsize;
				if(sublength > xlen)
					sublength = xlen;

				UINT32* dstpek = (UINT32*) &line[x * dst_bypp + (y + i) * dst_bpl];
				UINT32* srcpek = (UINT32*) &src[int(sy >> 16) * src_bpl];
				bltfunc(srcpek, dstpek, sublength, dx);

				// Offset the values, so that the rest of the pixels is placed right.
				srcxofs = 1;
				xofs = (int)sublength;
				pixels_to_blit -= sublength;
				if(pixels_to_blit <= 0)
					skip_rest = TRUE;
			}
			if(skip_rest == FALSE) // Blit the rest
			{
				UINT32* dstpek = (UINT32*) &line[(x + xofs) * dst_bypp + (y + i) * dst_bpl];
				UINT32* srcpek = (UINT32*) &src[srcxofs * src_bypp + int(sy >> 16) * src_bpl];
				bltfunc(srcpek, dstpek, pixels_to_blit, dx);
			}
		}
		sy += dy;
	}

	dstbitmap->ReleasePointer();
	srcbitmap->ReleasePointer();
}

// Blits srcbitmap to dstbitmap using the given blitfunction.

OP_STATUS DrawBitmapClippedGetLine(const OpBitmap* srcbitmap, OpBitmap* dstbitmap,
								   const OpRect &source,
								   const OpPoint &destpoint,
								   BlitFunction bltfunc)
{
	int x = destpoint.x;
	int y = destpoint.y;
	int xlen = source.width;
	int ofs_src = source.x; // Move sourceoffset to match sourcerect
	int dst_w = dstbitmap->Width();
	int dst_h = dstbitmap->Height();
	if(x < 0) // Clip to the left
	{
		ofs_src += 0-x;
		xlen -= 0-x;
		x = 0;
	}

	if (x >= dst_w) // Extreme case
	{
		return OpStatus::OK;
	}

	if(x + xlen > dst_w) // Clip to the right
	{
		xlen -= (x + xlen) - dst_w;
	}
	int src_bypp = 4;//srcbitmap->GetBpp() == 15 ? 2 : srcbitmap->GetBpp() >> 3;
	int dst_bypp = 4;//dstbitmap->GetBpp() == 15 ? 2 : dstbitmap->GetBpp() >> 3;

	UINT8* line = OP_NEWA(UINT8, dst_w * dst_bypp );
	UINT8* src = OP_NEWA(UINT8, srcbitmap->Width() * src_bypp);

	if (line == NULL || src == NULL)
	{
		OP_DELETEA(line);
		OP_DELETEA(src);
		return OpStatus::ERR_NO_MEMORY;
	}

	BOOL src_has_alpha_or_is_transparent = srcbitmap->HasAlpha() || srcbitmap->IsTransparent();

	// Run from top to bottom and call the horizontal blitfunction each row
	for(int i = 0; i < source.height; i++)
	{
		if (i + y >= 0 && i + y < dst_h) // Clipping
		{
			if(src_has_alpha_or_is_transparent)
				dstbitmap->GetLineData(line, y + i);
			srcbitmap->GetLineData(src, i + source.y);
	
			UINT32* dstpek = (UINT32*) &line[x * dst_bypp];
			UINT32* srcpek = (UINT32*) &src[ofs_src * src_bypp];
			bltfunc(srcpek, dstpek, xlen);
			
			dstbitmap->AddLine(line, y + i);
		}
	}

	OP_DELETEA(line);
	OP_DELETEA(src);

	return OpStatus::OK;
}

// Blits srcbitmap to dstbitmap using the given blitfunction.

OP_STATUS DrawBitmapClippedScaledGetLine(const OpBitmap* srcbitmap, OpBitmap* dstbitmap,
										 const OpRect &source,
										 const OpRect &dest,
										 BlitFunctionScaled bltfunc)
{
	int x = dest.x;
	int y = dest.y;
	int ofs_src = source.x; // Move sourceoffset to match sourcerect
	int xlen = dest.width;
	int dst_w = dstbitmap->Width();
	int dst_h = dstbitmap->Height();
	if (x < 0) // Clip to the left
	{
		ofs_src += int(float(0-x) * float(source.width)/float(dest.width));
		xlen -= 0-x;
		x = 0;
	}

	if (x >= dst_w) // Extreme case
	{
		return OpStatus::OK;
	}

	if (x + xlen > dst_w) // Clip to the right
	{
		xlen -= (x + xlen) - dst_w;
	}
	FIXED1616 sy = 0; // Where we are and reads pixels
	FIXED1616 dx = (source.width << 16) / dest.width; //Calculate the values we must step with, in order to get
	FIXED1616 dy = (source.height << 16) / dest.height; //the right pixels from the sourcebitmap.

	int src_bypp = 4;//srcbitmap->GetBpp() == 15 ? 2 : srcbitmap->GetBpp() >> 3;
	int dst_bypp = 4;//dstbitmap->GetBpp() == 15 ? 2 : dstbitmap->GetBpp() >> 3;

	UINT8* line = OP_NEWA(UINT8, dst_w * dst_bypp);
	UINT8* src = OP_NEWA(UINT8, srcbitmap->Width() * src_bypp);

	if (src == NULL || line == NULL)
	{
		OP_DELETEA(line);
		OP_DELETEA(src);
		return OpStatus::ERR_NO_MEMORY;
	}

	BOOL src_has_alpha_or_is_transparent = srcbitmap->HasAlpha() || srcbitmap->IsTransparent();

	// Run from top to bottom and call the horizontal blitfunction each row
	for (int i = 0; i < dest.height; i++)
	{
		if (i + y >= 0 && i + y < dst_h) // Clipping
		{
			if(src_has_alpha_or_is_transparent)
				dstbitmap->GetLineData(line, y + i);
			srcbitmap->GetLineData(src, source.y + int(sy >> 16));

			UINT32* dstpek = (UINT32*) &line[x * dst_bypp];
			UINT32* srcpek = (UINT32*) &src[ofs_src * src_bypp];
			bltfunc(srcpek, dstpek, xlen, dx);

			dstbitmap->AddLine(line, y + i);
		}
		sy += dy;
	}

	OP_DELETEA(line);
	OP_DELETEA(src);
	return OpStatus::OK;
}

BlitFunctionScaled SelectBlitFunctionScaled(BOOL is_transparent, BOOL has_alpha, int dst_bpp)
{
	if (dst_bpp == 32)
	{
		if (has_alpha)
			return BlitHLineAlphaScaled32;
		else if (is_transparent)
			return BlitHLineTransparentScaled32;
		else
			return BlitHLineCopyScaled32;
	}
	else if (dst_bpp == 24)
	{
		if (has_alpha)
			return BlitHLineAlphaScaled24;
		else if (is_transparent)
			return BlitHLineTransparentScaled24;
		else
			return BlitHLineCopyScaled24;
	}
	else if (dst_bpp == 16)
	{
		if (has_alpha)
			return BlitHLineAlphaScaled16;
		else if (is_transparent)
			return BlitHLineTransparentScaled16;
		else
			return BlitHLineCopyScaled16;
	}
	else if (dst_bpp == 8)
	{
		return BlitHLineCopyScaled8;
	}
	return 0; // PANIC!
}

BlitFunction SelectBlitFunction(BOOL is_transparent, BOOL has_alpha, int dst_bpp, BOOL destination_has_alpha)
{
	if (dst_bpp == 32)
	{
		if (has_alpha)
#ifdef REALLY_UGLY_COLOR_HACK
			return destination_has_alpha ? BlitHLineAlpha32UglyColorHack : BlitHLineAlpha32;
#else
			return BlitHLineAlpha32;
#endif
		else if (is_transparent)
			return BlitHLineTransparent32;
		else
			return BlitHLineCopy32;
	}
	else if (dst_bpp == 24)
	{
		if (has_alpha)
			return BlitHLineAlpha24;
		else if (is_transparent)
			return BlitHLineTransparent24;
		else
			return BlitHLineCopy24;
	}
	else if (dst_bpp == 16)
	{
		if (has_alpha)
			return BlitHLineAlpha16;
		else if (is_transparent)
			return BlitHLineTransparent16;
		else
			return BlitHLineCopy16;
	}
	return 0; // PANIC!
}

BlitFunction SelectBlitFunction(BOOL is_transparent, BOOL has_alpha, int dst_bpp)
{
	return SelectBlitFunction(is_transparent, has_alpha, dst_bpp, FALSE);
}

// *Internal blit*
// Creating a backgroundbitmap from the painter. Blits the sourcebitmap to it, and blits it back to the painter.

OP_STATUS BlitImage_UsingBackgroundAndPointer(OpPainter* painter, OpBitmap* bitmap,
											  const OpRect &source, const OpRect &dest,
											  BOOL is_transparent, BOOL has_alpha)
{
	BOOL needscale = TRUE;
	if (dest.width == source.width && dest.height == source.height)
//	if (dest.width * painter->GetScale() == source.width && dest.height * painter->GetScale() == source.height)
	{
		needscale = FALSE;
	}

	OpBitmap* screenbitmap = painter->CreateBitmapFromBackground(dest);
	if (screenbitmap == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	UINT8 bpp = screenbitmap->GetBpp();

	BOOL use_pointer = bitmap->GetBpp() == 32 && bitmap->Supports(OpBitmap::SUPPORTS_POINTER) && screenbitmap->Supports(OpBitmap::SUPPORTS_POINTER);

	if (needscale)
	{
		OpRect dest2(0,0,dest.width,dest.height);
		if (use_pointer) // Access memory directly
		{
			DrawBitmapClippedScaled(bitmap, screenbitmap,
												source,
												dest2,
												SelectBlitFunctionScaled(is_transparent, has_alpha, bpp));
		}
		else // AddLine, GetLineData
		{
            OP_STATUS err = DrawBitmapClippedScaledGetLine(bitmap, screenbitmap, source, dest2, SelectBlitFunctionScaled(is_transparent, has_alpha, 32));
            if (OpStatus::IsError(err))
            {
                OP_DELETE(screenbitmap);
                return err;
            }
		}
	}
	else
	{
		BOOL destination_has_alpha = FALSE;
		if (painter->IsUsingOffscreenbitmap() && painter->GetOffscreenBitmap()->HasAlpha())
			destination_has_alpha = TRUE;

		if (use_pointer) // Access memory directly
		{
			DrawBitmapClipped(bitmap, screenbitmap, source, OpPoint(0,0), SelectBlitFunction(is_transparent, has_alpha, bpp, destination_has_alpha));
		}
		else // AddLine, GetLineData
		{
            OP_STATUS err = DrawBitmapClippedGetLine(bitmap, screenbitmap, source, OpPoint(0,0), SelectBlitFunction(is_transparent, has_alpha, 32, destination_has_alpha));
            if (OpStatus::IsError(err))
            {
                OP_DELETE(screenbitmap);
                return err;
            }
		}
	}

	// Blit the result back to the screen
	painter->DrawBitmap(screenbitmap, OpPoint(dest.x, dest.y));

	OP_DELETE(screenbitmap);
	return OpStatus::OK;
}

#endif // DISPLAY_FALLBACK_PAINTING

OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value)
{
	if (!effect || bitmap == NULL || bitmap->GetBytesPerLine() == 0)
	{
		return bitmap;
	}

	int width = bitmap->Width();
	int height = bitmap->Height();

	OpBitmap* temp_bitmap = NULL;

	if (OpStatus::IsError(OpBitmap::Create(&temp_bitmap, width, height)))
	{
		return bitmap;
	}

	BOOL support_pointer = FALSE;

	if (bitmap->GetBpp() == 32 && temp_bitmap->GetBpp() == 32 &&
		bitmap->Supports(OpBitmap::SUPPORTS_POINTER) &&
		temp_bitmap->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		support_pointer = TRUE;
	}
	if (effect & (Image::EFFECT_DISABLED | Image::EFFECT_GLOW | Image::EFFECT_BLEND))
	{
		int colorfactor[3] = { DISPLAY_GLOW_COLOR_FACTOR };
		int glow_r = effect_value * colorfactor[0] / 255;
		int glow_g = effect_value * colorfactor[1] / 255;
		int glow_b = effect_value * colorfactor[2] / 255;

		int lineOffset = 0;

		if (support_pointer)
		{
			temp_bitmap->ForceAlpha();
		}

		UINT8* src_line;
		UINT8* dst_line;

		if (support_pointer)
		{
			src_line = (UINT8*) bitmap->GetPointer();
			dst_line = (UINT8*) temp_bitmap->GetPointer();
			lineOffset = bitmap->GetBytesPerLine();
		}
		else
		{
			src_line = OP_NEWA(UINT8, width * 4);
			dst_line = src_line;
		}

		if (!src_line || !dst_line)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			OP_DELETE(temp_bitmap);
			return bitmap;
		}

		for (int y=0; y<height; y++)
		{
			if (!support_pointer)
			{
				bitmap->GetLineData(src_line, y);
			}
			UINT32* src = (UINT32*) src_line;
			UINT32* dst = (UINT32*) dst_line;
			for (int x=0; x<width; x++)
			{
				UINT32 color = *src;
				if (effect & Image::EFFECT_DISABLED)
				{
#ifdef USE_PREMULTIPLIED_ALPHA
					color = ((color >> 1) & 0x7f7f7f7f);
#else
					color = ((color >> 1) & 0xff000000) | (color & 0x00ffffff);
#endif
				}
				if (effect & Image::EFFECT_GLOW)
				{
					int r = (color >> 16) & 0xff;
					int g = (color >> 8) & 0xff;
					int b = (color >> 0) & 0xff;

#ifdef USE_PREMULTIPLIED_ALPHA
					int a = (color >> 24) & 0xff;

					r += (glow_r*a)/255;
					r = MIN(a, r);
					g += (glow_g*a)/255;
					g = MIN(a, g);
					b += (glow_b*a)/255;
					b = MIN(a, b);
#else // !USE_PREMULTIPLIED_ALPHA
					r += glow_r;
					r = MIN(255, r);
					g += glow_g;
					g = MIN(255, g);
					b += glow_b;
					b = MIN(255, b);
#endif // !USE_PREMULTIPLIED_ALPHA
					color = (color & 0xff000000) | (r << 16) | (g << 8) | b;
				}
				if (effect & Image::EFFECT_BLEND)
				{
#ifdef USE_PREMULTIPLIED_ALPHA
					UINT32 dstcol = (((color >> 24) * effect_value / 100) << 24);
					dstcol |= ((((color >> 16)&0xff) * effect_value / 100) << 16);
					dstcol |= ((((color >> 8)&0xff) * effect_value / 100) << 8);
					dstcol |= ((color&0xff) * effect_value / 100);
					color = dstcol;
#else // !USE_PREMULTIPLIED_ALPHA
					color = (((color >> 24) * effect_value / 100) << 24) | (color & 0x00ffffff);
#endif // !USE_PREMULTIPLIED_ALPHA
				}
				*dst = color;

				++src;
				++dst;
			}
			if (support_pointer)
			{
				src_line += lineOffset;
				dst_line += lineOffset;
			}
			else
			{
				temp_bitmap->AddLine(dst_line, y);
			}
		}
		if (support_pointer)
		{
			bitmap->ReleasePointer();
			temp_bitmap->ReleasePointer();
		}
		else
		{
			OP_DELETEA(src_line);
		}
	}
	return temp_bitmap;
}

// Makes a bigger bitmap if the size is very small. (To optimize)
OpBitmap* CreateTileOptimizedBitmap(OpBitmap* srcbitmap, INT32 new_width, INT32 new_height)
{
	OpBitmap* dstbitmap = NULL;
	int new_w = new_width;
	int new_h = new_height;
	int src_w = srcbitmap->Width();
	int src_h = srcbitmap->Height();
	BOOL src_transparent = srcbitmap->IsTransparent();
	BOOL src_alpha = srcbitmap->HasAlpha();

#ifdef VEGA_OPPAINTER_SUPPORT
	if (OpStatus::IsError(OpBitmap::Create(&dstbitmap, new_w, new_h, src_transparent, src_alpha, 0, 0)))
		return srcbitmap;
	int i, j;
	for(j = 0; j < new_h; j += src_h)
	{
		for(i = 0; i < new_w; i += src_w)
		{
			OP_STATUS err = OpStatus::OK;
			if (i+src_w > new_w || j+src_h > new_h)
				err = ((VEGAOpBitmap*)dstbitmap)->CopyRect(OpPoint(i, j), OpRect(0,0,new_w-i,new_h-j), srcbitmap);
			else
				err = ((VEGAOpBitmap*)dstbitmap)->CopyRect(OpPoint(i, j), OpRect(0,0,src_w,src_h), srcbitmap);
			if (OpStatus::IsError(err))
			{
				OP_DELETE(dstbitmap);
				return srcbitmap;
			}
		}
	}
#else // !VEGA_OPPAINTER_SUPPORT
	if(!srcbitmap->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		if (OpStatus::IsError(OpBitmap::Create(&dstbitmap, new_w, new_h, src_transparent, src_alpha, 0, 0, TRUE)))
			return srcbitmap;

		if (src_alpha || src_transparent)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL rc =
#endif
			dstbitmap->SetColor(NULL, TRUE, NULL);
			OP_ASSERT(rc); // Always true for all_transparent == TRUE
		}
		OpPainter* painter = dstbitmap->GetPainter();
		if(painter)
		{
			int i, j;
			for(j = 0; j < new_h; j += src_h)
			{
				for(i = 0; i < new_w; i += src_w)
				{
					painter->DrawBitmap(srcbitmap, OpPoint(i, j));
				}
			}
			dstbitmap->ReleasePainter();
		}
	}
	else
	{
		UINT32 src_bytes_per_line = srcbitmap->GetBytesPerLine();

		OP_STATUS status = OpBitmap::Create(&dstbitmap, new_w, new_h, src_transparent, src_alpha);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(dstbitmap);
			return srcbitmap;
		}

		UINT8* src = (UINT8*) srcbitmap->GetPointer();
		UINT8* dst = (UINT8*) dstbitmap->GetPointer();

		int src_bpp = srcbitmap->GetBpp();
		int dst_bpp = dstbitmap->GetBpp();
		int src_bypp = src_bpp == 15 ? 2 : src_bpp >> 3;
		int dst_bypp = dst_bpp == 15 ? 2 : dst_bpp >> 3;
		int dst_h = dstbitmap->Height();
		UINT32 dst_bytes_per_line = dstbitmap->GetBytesPerLine();

		for (int j = 0; j < new_h; j++)
		{
			UINT8* srcpek = &src[(j % src_h) * src_bytes_per_line];
			UINT8* dstpek = &dst[(j % dst_h) * dst_bytes_per_line];
			for (int i = 0; i < new_w; i += src_w)
			{
				op_memcpy(&dstpek[i * dst_bypp], srcpek, src_w * src_bypp);
			}
		}
		srcbitmap->ReleasePointer();
		dstbitmap->ReleasePointer();
	}
#endif // !VEGA_OPPAINTER_SUPPORT
	return dstbitmap;
}
