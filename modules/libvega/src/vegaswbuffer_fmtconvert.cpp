/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/src/vegaswbuffer.h"
#include "modules/libvega/src/vegapixelformat.h"

typedef void (PackFunctionRGB)(UINT8* p, unsigned r, unsigned g, unsigned b);
typedef void (PackFunctionARGB)(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b);
typedef void (UnpackFunctionRGB)(const UINT8* p, unsigned &r, unsigned &g, unsigned &b);
typedef void (UnpackFunctionARGB)(const UINT8* p, unsigned &a, unsigned &r, unsigned &g, unsigned &b);

#define UnpackFormatConvert_RGB(UNPCK, BPP, store, dst, dstPixelPadding, rect_width, rect_height)\
do \
{\
	const UINT8* src = (UINT8*)store->buffer;\
	unsigned srcPixelPadding = store->stride - store->width * BPP;\
	unsigned sr, sg, sb;\
\
	for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
	{\
		for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
		{\
			UNPCK(src, sr, sg, sb);\
			dst.StoreRGB(sr, sg, sb);\
\
			src += BPP;\
			dst++;\
		}\
\
		src += srcPixelPadding;\
		dst += dstPixelPadding;\
	}\
}\
while(0)

#define UnpackFormatConvert_ARGB(UNPCK, BPP, store, dst, dstPixelPadding, rect_width, rect_height)\
do \
{\
	const UINT8* src = (UINT8*)store->buffer;\
	unsigned srcPixelPadding = store->stride - store->width * BPP;\
	unsigned sa, sr, sg, sb;\
\
	for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
	{\
		for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
		{\
			UNPCK(src, sa, sr, sg, sb);\
			dst.StoreARGB(sa, sr, sg, sb);\
\
			src += BPP;\
			dst++;\
		}\
\
		src += srcPixelPadding;\
		dst += dstPixelPadding;\
	}\
}\
while(0)

void UnpackFormatConvert_A(VEGAPixelStore* store, VEGAPixelAccessor dst, unsigned dstPixelPadding, unsigned width, unsigned height)
{
	const UINT8* src = (UINT8*)store->buffer;
	unsigned srcPixelPadding = store->stride - store->width;

	for (unsigned yp = 0; yp < height; ++yp)
	{
		for (unsigned xp = 0; xp < width; ++xp)
		{
			dst.StoreARGB(*src++, 0, 0, 0);
			dst++;
		}
		src += srcPixelPadding;
		dst += dstPixelPadding;
	}
}

void UnpackFormatConvert_LA(VEGAPixelStore* store, VEGAPixelAccessor dst, unsigned dstPixelPadding, unsigned width, unsigned height)
{
	const UINT8* src = (UINT8*)store->buffer;
	unsigned srcPixelPadding = store->stride - store->width * 2;

	for (unsigned yp = 0; yp < height; ++yp)
	{
		for (unsigned xp = 0; xp < width; ++xp)
		{
			dst.StoreARGB(src[0], src[1], src[1], src[1]);
			src += 2;
			dst++;
		}
		src += srcPixelPadding;
		dst += dstPixelPadding;
	}
}

#ifdef VEGA_USE_ASM
#define FMTCONVERT_ARGB(SRC, BPP) VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvert_ ## SRC(store, dst, dstPixelPadding, width, height)
#define FMTCONVERT_RGB(SRC, BPP) VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvert_ ## SRC(store, dst, dstPixelPadding, width, height)
#else
#define FMTCONVERT_ARGB(SRC, BPP) UnpackFormatConvert_ARGB(VEGAFormatUnpack::SRC ## _Unpack, BPP, store, dst, dstPixelPadding, width, height)
#define FMTCONVERT_RGB(SRC, BPP) UnpackFormatConvert_RGB(VEGAFormatUnpack::SRC ## _Unpack, BPP, store, dst, dstPixelPadding, width, height)
#endif // VEGA_USE_ASM

void VEGASWBuffer::CopyFromPixelStore(VEGAPixelStore* store)
{
	// Check if it's just a single memory block copy and handle that separately.
#if defined(VEGA_INTERNAL_FORMAT_BGRA8888) || defined(VEGA_INTERNAL_FORMAT_RGBA8888)
	bool matchesInternalFormat = VEGA_PIXEL_FORMAT_CLASS::IsCompatible(store->format);
	if (matchesInternalFormat && GetPixelStride() == store->stride / 4)
	{
		op_memcpy(ptr.rgba, store->buffer, store->stride * height - (store->stride - width * 4));
		return;
	}
#endif

	VEGAPixelAccessor dst(ptr);
	unsigned dstPixelPadding = GetPixelStride() - width;

	switch (store->format)
	{
	case VPSF_BGRA8888:
		FMTCONVERT_ARGB(BGRA8888, 4);
		break;

	case VPSF_RGBA8888:
		FMTCONVERT_ARGB(RGBA8888, 4);
		break;

	case VPSF_ABGR8888:
		FMTCONVERT_ARGB(ABGR8888, 4);
		break;

	case VPSF_ARGB8888:
		FMTCONVERT_ARGB(ARGB8888, 4);
		break;

	case VPSF_RGBA4444:
		FMTCONVERT_ARGB(RGBA4444, 2);
		break;

	case VPSF_BGRA4444:
		FMTCONVERT_ARGB(BGRA4444, 2);
		break;

	case VPSF_ABGR4444:
		FMTCONVERT_ARGB(ABGR4444, 2);
		break;

	case VPSF_RGB565:
		FMTCONVERT_RGB(RGB565, 2);
		break;

	case VPSF_BGR565:
		FMTCONVERT_RGB(BGR565, 2);
		break;

	case VPSF_RGBA5551:
		UnpackFormatConvert_ARGB(VEGAFormatUnpack::RGBA5551_Unpack, 2, store, dst, dstPixelPadding, width, height);
		break;

	case VPSF_RGB888:
		FMTCONVERT_RGB(RGB888, 3);
		break;

	case VPSF_ALPHA8:
		UnpackFormatConvert_A(store, dst, dstPixelPadding, width, height);
		break;

	case VPSF_LUMINANCE8_ALPHA8:
		UnpackFormatConvert_LA(store, dst, dstPixelPadding, width, height);
		break;

	case VPSF_BGRX8888:
		UnpackFormatConvert_RGB(VEGAFormatUnpack::BGRX8888_Unpack, 4, store, dst, dstPixelPadding, width, height);
		break;

	case VPSF_BGRA5551:
	default:
		// Someone need to implement something
		OP_ASSERT(!"Unsupported pixel store format");
		break;
	}
}

void RGB565_Pack(UINT8* p, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = (UINT16)VEGAPixelFormat_RGB565A8::PackRGB(r, g, b);
}

void BGR565_Pack(UINT8* p, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = ((b&0xf8)<<8)|((g&0xfc)<<3)|(r>>3);
}

void RGBA8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT32*)p = (a<<24)|(b<<16)|(g<<8)|r;
}

void ABGR4444_as_RGBA8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	unsigned qa = (a >> 4) * 17;
	unsigned qr = (r >> 4) * 17;
	unsigned qg = (g >> 4) * 17;
	unsigned qb = (b >> 4) * 17;
	*(UINT32*)p = (qa<<24)|(qb<<16)|(qg<<8)|qr;
}

void ABGR4444_as_BGRA8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	unsigned qa = (a >> 4) * 17;
	unsigned qr = (r >> 4) * 17;
	unsigned qg = (g >> 4) * 17;
	unsigned qb = (b >> 4) * 17;
	*(UINT32*)p = (qa<<24)|(qr<<16)|(qg<<8)|qb;
}

void ABGR8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT32*)p = (r<<24)|(g<<16)|(b<<8)|a;
}

void ARGB8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT32*)p = (b<<24)|(g<<16)|(r<<8)|a;
}

void BGRX8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT32*)p = (r<<16)|(g<<8)|b;
}

void RGB565_as_BGRX8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	unsigned qr = (r >> 3) * 33L / 4;
	unsigned qg = (g >> 2) * 65L / 16;
	unsigned qb = (b >> 3) * 33L / 4;
	*(UINT32*)p = (qr<<16)|(qg<<8)|qb;
}

void ABGR4444_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = ((r&0xf0)<<8)|((g&0xf0)<<4)|(b&0xf0)|(a>>4);
}

void RGBA4444_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = (r>>4)|(g&0xf0)|((b&0xf0)<<4)|((a&0xf0)<<8);
}

void BGRA4444_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = ((a&0xf0)<<8)|((r&0xf0)<<4)|(b&0xf0)|(b>>4);
}

void RGB888_Pack(UINT8* p, unsigned r, unsigned g, unsigned b)
{
	p[0] = r;
	p[1] = g;
	p[2] = b;
}

void RGBA5551_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = ((r&0xf8)<<8)|((g&0xf8)<<3)|((b&0xf8)>>2)|(a>>7);
}

void BGRA5551_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	*(UINT16*)p = ((b&0xf8)<<8)|((g&0xf8)<<3)|((r&0xf8)>>2)|(a>>7);
}

void RGBA5551_as_RGBA8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	unsigned qa = (a >> 7) * 255;
	unsigned qr = ((r >> 3) * 33) >> 2;
	unsigned qg = ((g >> 3) * 33) >> 2;
	unsigned qb = ((b >> 3) * 33) >> 2;
	*(UINT32*)p = (qa<<24)|(qb<<16)|(qg<<8)|qr;
}

void RGBA5551_as_BGRA8888_Pack(UINT8* p, unsigned a, unsigned r, unsigned g, unsigned b)
{
	unsigned qa = (a >> 7) * 255;
	unsigned qr = ((r >> 3) * 33) >> 2;
	unsigned qg = ((g >> 3) * 33) >> 2;
	unsigned qb = ((b >> 3) * 33) >> 2;
	*(UINT32*)p = (qa<<24)|(qr<<16)|(qg<<8)|qb;
}

#define PackFormatConvert_Palette_RGB(PCK, BPP, pal)\
do\
{\
	UINT8* dstPal = (UINT8*)g_memory_manager->GetTempBuf2k();\
	for (unsigned i = 0; i < 256; ++i)\
	{\
		VEGA_PIXEL px = pal[i];\
\
		unsigned red = VEGA_UNPACK_R(px);\
		unsigned green = VEGA_UNPACK_G(px);\
		unsigned blue = VEGA_UNPACK_B(px);\
		PCK(dstPal, red, green, blue);\
\
		dstPal += BPP;\
	}\
}\
while(0)

#define PackFormatConvert_Palette_ARGB(PCK, BPP, pal, premultiplyA)\
do\
{\
	UINT8* dstPal = (UINT8*)g_memory_manager->GetTempBuf2k();\
	if (premultiplyA)\
		for (unsigned i = 0; i < 256; ++i)\
		{\
			VEGA_PIXEL px = pal[i];\
\
			unsigned alpha = VEGA_UNPACK_A(px);\
			unsigned red = (VEGA_UNPACK_R(px) * alpha + 127) / 255;\
			unsigned green = (VEGA_UNPACK_G(px) * alpha + 127) / 255;\
			unsigned blue = (VEGA_UNPACK_B(px) * alpha + 127) /255;\
			PCK(dstPal, alpha, red, green, blue);\
\
			dstPal += BPP;\
		}\
	else\
		for (unsigned i = 0; i < 256; ++i)\
		{\
			VEGA_PIXEL px = pal[i];\
\
			unsigned alpha = VEGA_UNPACK_A(px);\
			unsigned red = VEGA_UNPACK_R(px);\
			unsigned green = VEGA_UNPACK_G(px);\
			unsigned blue = VEGA_UNPACK_B(px);\
			PCK(dstPal, alpha, red, green, blue);\
\
			dstPal += BPP;\
		}\
}\
while(0)

#define PackFormatConvert_Palette_BuiltIn(FMT_CLASS, PTYPE, BPP, pal, premultiplyA)\
do {\
	FMT_CLASS::Pointer dp;\
	OpStatus::Ignore(FMT_CLASS::CreateBufferFromData(dp, g_memory_manager->GetTempBuf2k(), 256, 1, false));\
	FMT_CLASS::Accessor dst(dp);\
\
	if (premultiplyA)\
		for (unsigned i = 0; i < 256; ++i)\
		{\
			VEGA_PIXEL px = pal[i];\
\
			unsigned alpha = VEGA_UNPACK_A(px);\
			unsigned red = (VEGA_UNPACK_R(px) * alpha + 127) / 255;\
			unsigned green = (VEGA_UNPACK_G(px) * alpha + 127) / 255;\
			unsigned blue = (VEGA_UNPACK_B(px) * alpha + 127) /255;\
			dst.StoreARGB(alpha, red, green, blue);\
			dst++;\
		}\
	else\
		for (unsigned i = 0; i < 256; ++i)\
		{\
			VEGA_PIXEL px = pal[i];\
\
			unsigned alpha = VEGA_UNPACK_A(px);\
			unsigned red = VEGA_UNPACK_R(px);\
			unsigned green = VEGA_UNPACK_G(px);\
			unsigned blue = VEGA_UNPACK_B(px);\
			dst.StoreARGB(alpha, red, green, blue);\
			dst++;\
		}\
}\
while(0)

#define PackFormatConvert_BuiltIn(FMT_CLASS, PTYPE, BPP, store, src, rect_width, rect_height, srcPixelStride, flipY, premultiplyA)\
do { \
	FMT_CLASS::Pointer dp = { (PTYPE*)store->buffer };\
	FMT_CLASS::Accessor dst(dp);\
	signed dstPixelPadding = store->stride / BPP - store->width;\
	if (flipY)\
	{\
		dst += (rect_width + dstPixelPadding) * (rect_height - 1);\
		dstPixelPadding = -(dstPixelPadding + static_cast<signed>(rect_width) * 2);\
	}\
\
	unsigned sa, sr, sg, sb;\
	if (premultiplyA)\
		for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
		{\
			for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
			{\
				src.LoadUnpack(sa, sr, sg, sb);\
				sr = (sr * sa + 127) / 255;\
				sg = (sg * sa + 127) / 255;\
				sb = (sb * sa + 127) / 255;\
				dst.StoreARGB(sa, sr, sg, sb);\
\
				src++;\
				dst++;\
			}\
\
			src += srcPixelPadding;\
			dst += dstPixelPadding;\
		}\
	else\
		for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
		{\
			for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
			{\
				src.LoadUnpack(sa, sr, sg, sb);\
				dst.StoreARGB(sa, sr, sg, sb);\
\
				src++;\
				dst++;\
			}\
\
			src += srcPixelPadding;\
			dst += dstPixelPadding;\
		}\
}\
while(0)

#define PackFormatConvert_RGB(PCK, BPP, store, src, rect_width, rect_height, srcPixelPadding, flipY)\
do\
{\
	UINT8* dst = (UINT8*)store->buffer;\
	signed dstPixelPadding = store->stride - store->width * BPP;\
	if (flipY)\
	{\
		dst += (rect_width * BPP + dstPixelPadding) * (rect_height - 1);\
		dstPixelPadding = -(dstPixelPadding + static_cast<signed>(rect_width * 2 * BPP));\
	}\
\
	unsigned sa, sr, sg, sb;\
	for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
	{\
		for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
		{\
			src.LoadUnpack(sa, sr, sg, sb);\
			PCK(dst, sr, sg, sb);\
\
			dst += BPP;\
			src++;\
		}\
\
		src += srcPixelPadding;\
		dst += dstPixelPadding;\
	}\
}\
while(0)

#define PackFormatConvert_ARGB(PCK, BPP, store, src, rect_width, rect_height, srcPixelPadding, flipY, premultiplyA)\
do \
{\
	UINT8* dst = (UINT8*)store->buffer;\
	signed dstPixelPadding = store->stride - store->width * BPP;\
	if (flipY)\
	{\
		dst += (rect_width * BPP + dstPixelPadding) * (rect_height - 1);\
		dstPixelPadding = -(dstPixelPadding + static_cast<signed>(rect_width * 2 * BPP));\
	}\
\
	unsigned sa, sr, sg, sb;\
	if (premultiplyA)\
		for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
		{\
			for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
			{\
				src.LoadUnpack(sa, sr, sg, sb);\
				sr = (sr * sa + 127) / 255;\
				sg = (sg * sa + 127) / 255;\
				sb = (sb * sa + 127) / 255;\
				PCK(dst, sa, sr, sg, sb);\
\
				dst += BPP;\
				src++;\
			}\
\
			src += srcPixelPadding;\
			dst += dstPixelPadding;\
		}\
	else\
		for (unsigned yp = 0; yp < (unsigned)rect_height; ++yp)\
		{\
			for (unsigned xp = 0; xp < (unsigned)rect_width; ++xp)\
			{\
				src.LoadUnpack(sa, sr, sg, sb);\
				PCK(dst, sa, sr, sg, sb);\
\
				dst += BPP;\
				src++;\
			}\
\
			src += srcPixelPadding;\
			dst += dstPixelPadding;\
		}\
\
}\
while(0)

void PackFormatConvert_A(VEGAPixelStore* store, VEGAConstPixelAccessor src, unsigned width, unsigned height, unsigned srcPixelPadding, bool flipY)
{
	UINT8* dst = (UINT8*)store->buffer;
	signed dstPixelPadding = store->stride - store->width;
	if (flipY)
	{
		dst += (width + dstPixelPadding) * (height - 1);
		dstPixelPadding = -(dstPixelPadding + static_cast<signed>(width) * 2);
	}

	unsigned sa;
	for (unsigned yp = 0; yp < height; ++yp)
	{
		for (unsigned xp = 0; xp < width; ++xp)
		{
			src.LoadUnpack(sa);
			*dst++ = sa;
			src++;
		}

		src += srcPixelPadding;
		dst += dstPixelPadding;
	}
}

void PackFormatConvert_LA(VEGAPixelStore* store, VEGAConstPixelAccessor src, unsigned width, unsigned height, unsigned srcPixelPadding, bool flipY)
{
	UINT8* dst = (UINT8*)store->buffer;
	signed dstPixelPadding = store->stride - store->width * 2;
	if (flipY)
	{
		dst += (width * 2 + dstPixelPadding) * (height - 1);
		dstPixelPadding = -(dstPixelPadding + static_cast<signed>(width) * 2 * 2);
	}

	unsigned sa, sl;
	for (unsigned yp = 0; yp < height; ++yp)
	{
		for (unsigned xp = 0; xp < width; ++xp)
		{
			src.LoadUnpack(sa, sl);
			*dst++ = sl;
			*dst++ = sa;
			src++;
		}

		src += srcPixelPadding;
		dst += dstPixelPadding;
	}
}

void VEGASWBuffer::CopyRectToPixelStore(VEGAPixelStore* store, const OpRect& rect,
										bool flipY, bool premultiplyAlpha,
										VEGAPixelStoreFormat requestedFormat/* = VPSF_SAME*/) const
{
	unsigned store_bpp = VPSF_BytesPerPixel(store->format);

	VEGAPixelStore subarea_store;
	if (rect.x != 0 || rect.y != 0)
	{
		// Create a "substore".
		subarea_store = *store;

		subarea_store.buffer = (UINT8*)store->buffer + rect.x * store_bpp + rect.y * store->stride;
		subarea_store.width = rect.width;
		subarea_store.height = rect.height;

		store = &subarea_store;
	}

	if (IsIndexed())
	{
		const VEGA_PIXEL *srcPal = GetPalette();

		switch (store->format)
		{
		case VPSF_BGRA8888:
			if (requestedFormat == VPSF_ABGR4444)
				PackFormatConvert_Palette_ARGB(ABGR4444_as_RGBA8888_Pack, 4, srcPal, premultiplyAlpha);
			else if (requestedFormat == VPSF_RGBA5551)
				PackFormatConvert_Palette_ARGB(RGBA5551_as_RGBA8888_Pack, 4, srcPal, premultiplyAlpha);
			else
			{
				PackFormatConvert_Palette_BuiltIn(VEGAPixelFormat_BGRA8888, UINT32, 4, srcPal, premultiplyAlpha);
			}
			break;

		case VPSF_RGBA8888:
			if (requestedFormat == VPSF_ABGR4444)
				PackFormatConvert_Palette_ARGB(ABGR4444_as_RGBA8888_Pack, 4, srcPal, premultiplyAlpha);
			else if (requestedFormat == VPSF_RGBA5551)
				PackFormatConvert_Palette_ARGB(RGBA5551_as_RGBA8888_Pack, 4, srcPal, premultiplyAlpha);
			else
			{
				OP_ASSERT(requestedFormat == VPSF_SAME);
				PackFormatConvert_Palette_ARGB(RGBA8888_Pack, 4, srcPal, premultiplyAlpha);
			}
			break;

		case VPSF_BGRX8888:
			if (requestedFormat == VPSF_RGB565)
				PackFormatConvert_Palette_ARGB(RGB565_as_BGRX8888_Pack, 4, srcPal, premultiplyAlpha);
			else
			{
				OP_ASSERT(requestedFormat == VPSF_SAME);
				PackFormatConvert_Palette_ARGB(BGRX8888_Pack, 4, srcPal, premultiplyAlpha);
			}
			break;

		case VPSF_ABGR8888:
			PackFormatConvert_Palette_ARGB(ABGR8888_Pack, 4, srcPal, premultiplyAlpha);
			break;

		case VPSF_RGBA4444:
			PackFormatConvert_Palette_ARGB(RGBA4444_Pack, 2, srcPal, premultiplyAlpha);
			break;

		case VPSF_BGRA4444:
			PackFormatConvert_Palette_ARGB(BGRA4444_Pack, 2, srcPal, premultiplyAlpha);
			break;

		case VPSF_ABGR4444:
			PackFormatConvert_Palette_ARGB(ABGR4444_Pack, 2, srcPal, premultiplyAlpha);
			break;

		case VPSF_RGB565:
			PackFormatConvert_Palette_RGB(RGB565_Pack, 2, srcPal);
			break;

		case VPSF_BGR565:
			PackFormatConvert_Palette_RGB(BGR565_Pack, 2, srcPal);
			break;

		case VPSF_RGB888:
			PackFormatConvert_Palette_RGB(RGB888_Pack, 3, srcPal);
			break;

		case VPSF_RGBA5551:
			PackFormatConvert_Palette_ARGB(RGBA5551_Pack, 2, srcPal, premultiplyAlpha);
			break;

		case VPSF_ARGB8888:
			PackFormatConvert_Palette_ARGB(ARGB8888_Pack, 4, srcPal, premultiplyAlpha);
			break;

		case VPSF_ALPHA8:
		case VPSF_LUMINANCE8_ALPHA8:
		case VPSF_BGRA5551:
		default:
			// Someone need to implement something
			OP_ASSERT(!"Unsupported pixel store format");
			break;
		}

		UINT8* dst = (UINT8*)store->buffer;
		unsigned dstPixelPadding = store->stride - store->width * store_bpp;

		const UINT8* isrc = GetIndexedPtr(rect.x, rect.y);
		UINT8* pal = (UINT8*)g_memory_manager->GetTempBuf2k();

		signed srcPixelPadding = GetPixelStride() - rect.width;
		if (flipY)
		{
			isrc += GetPixelStride() * (rect.height - 1);
			srcPixelPadding = -srcPixelPadding;
		}

		for (unsigned yp = 0; yp < (unsigned)rect.height; ++yp)
		{
			for (unsigned xp = 0; xp < (unsigned)rect.width; ++xp)
			{
				for (unsigned b = 0; b < store_bpp; ++b)
					*(dst + b) = pal[*isrc * store_bpp + b];

				dst += store_bpp;
				isrc++;
			}

			isrc += srcPixelPadding;
			dst += dstPixelPadding;
		}
	}
	else
	{
		// Check if it's just a single memory block copy and handle that separately.
#if defined(VEGA_INTERNAL_FORMAT_BGRA8888) || defined(VEGA_INTERNAL_FORMAT_RGBA8888)
		bool matchesInternalFormat = VEGA_PIXEL_FORMAT_CLASS::IsCompatible(store->format);
		bool copyEntireBuffer = rect.x == 0 && rect.y == 0 && (unsigned)rect.width == width && (unsigned)rect.height == height;
		if (matchesInternalFormat && requestedFormat == VPSF_SAME && copyEntireBuffer && !flipY && !premultiplyAlpha && GetPixelStride() == store->stride / 4)
		{
			op_memcpy(store->buffer, ptr.rgba, store->stride * height - (store->stride - width * 4));
			return;
		}
#endif

		VEGAConstPixelAccessor src = GetConstAccessor(rect.x, rect.y);
		unsigned srcPixelPadding = GetPixelStride() - rect.width;

		switch (store->format)
		{
		case VPSF_BGRA8888:
			if (requestedFormat == VPSF_ABGR4444)
				PackFormatConvert_ARGB(ABGR4444_as_BGRA8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			else if (requestedFormat == VPSF_RGBA5551)
				PackFormatConvert_ARGB(RGBA5551_as_BGRA8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			else
			{
				PackFormatConvert_BuiltIn(VEGAPixelFormat_BGRA8888, UINT32, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			}
			break;

		case VPSF_RGBA8888:
			if (requestedFormat == VPSF_ABGR4444)
				PackFormatConvert_ARGB(ABGR4444_as_RGBA8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			else if (requestedFormat == VPSF_RGBA5551)
				PackFormatConvert_ARGB(RGBA5551_as_RGBA8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			else
			{
				OP_ASSERT(requestedFormat == VPSF_SAME);
				PackFormatConvert_BuiltIn(VEGAPixelFormat_RGBA8888, UINT32, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			}
			break;

		case VPSF_ABGR8888:
			PackFormatConvert_ARGB(ABGR8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_RGBA4444:
			PackFormatConvert_ARGB(RGBA4444_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_ABGR4444:
			PackFormatConvert_ARGB(ABGR4444_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_RGB565:
			PackFormatConvert_RGB(RGB565_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY);
			break;

		case VPSF_BGR565:
			PackFormatConvert_RGB(BGR565_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY);
			break;

		case VPSF_RGB888:
			PackFormatConvert_RGB(RGB888_Pack, 3, store, src, rect.width, rect.height, srcPixelPadding, flipY);
			break;

		case VPSF_RGBA5551:
			PackFormatConvert_ARGB(RGBA5551_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_ALPHA8:
			PackFormatConvert_A(store, src, rect.width, rect.height, srcPixelPadding, flipY);
			break;

		case VPSF_LUMINANCE8_ALPHA8:
			PackFormatConvert_LA(store, src, rect.width, rect.height, srcPixelPadding, flipY);
			break;

		case VPSF_ARGB8888:
			PackFormatConvert_ARGB(ARGB8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_BGRX8888:
			if (requestedFormat == VPSF_RGB565)
				PackFormatConvert_ARGB(RGB565_as_BGRX8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			else
			{
				OP_ASSERT(requestedFormat == VPSF_SAME);
				PackFormatConvert_ARGB(BGRX8888_Pack, 4, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			}
			break;

		case VPSF_BGRA4444:
			PackFormatConvert_ARGB(BGRA4444_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		case VPSF_BGRA5551:
			PackFormatConvert_ARGB(BGRA5551_Pack, 2, store, src, rect.width, rect.height, srcPixelPadding, flipY, premultiplyAlpha);
			break;

		default:
			// Someone need to implement something
			OP_ASSERT(!"Unsupported pixel store format");
			break;
		}
	}
}

void VEGASWBuffer::CopyToPixelStore(VEGAPixelStore* store, const OpRect* update_rects, unsigned int num_rects) const
{
	//If no update rects are supplied the whole buffer is copied.
	if (update_rects == NULL || num_rects == 0)
	{
		OpRect rect(0,0,width,height);
		CopyRectToPixelStore(store, rect);
	}
	else
	{
		for (unsigned int i = 0; i < num_rects; ++i)
		{
			CopyRectToPixelStore(store, update_rects[i]);
		}
	}
}

#endif // VEGA_SUPPORT
