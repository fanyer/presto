/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAPIXELFORMAT_H
#define VEGAPIXELFORMAT_H

typedef UINT32 VEGA_PIXEL;

#include "modules/libvega/vegapixelstore.h"
#include "modules/libvega/src/vegapixelformat_utils.h"
#include "modules/libvega/src/vegasampler.h"
#include "modules/libvega/src/vegadispatchtable.h"

// The number of fractional bits used by image interpolation (in VEGAImage)
#define VEGA_SAMPLER_FRACBITS		12

#define VEGA_SAMPLE_INT(v)			((v) >> VEGA_SAMPLER_FRACBITS)
#define VEGA_SAMPLE_FRAC(v)			(((v) >> (VEGA_SAMPLER_FRACBITS-8))&0xff)

#include "modules/libvega/src/vegapixelformat_8888.inl"
#include "modules/libvega/src/vegapixelformat_rgb565a8.inl"

#if defined VEGA_INTERNAL_FORMAT_BGRA8888
#define VEGA_PIXEL_FORMAT_CLASS		VEGAPixelFormat_BGRA8888
#elif defined VEGA_INTERNAL_FORMAT_RGBA8888
#define VEGA_PIXEL_FORMAT_CLASS		VEGAPixelFormat_RGBA8888
#elif defined VEGA_INTERNAL_FORMAT_RGB565A8
#define VEGA_PIXEL_FORMAT_CLASS		VEGAPixelFormat_RGB565A8
#define VEGA_USE_GENERIC_SWBUFFER
#endif // Internal pixel format configuration

#define VEGAPixelPremultiply		VEGA_PIXEL_FORMAT_CLASS::Premultiply
#define VEGAPixelPremultiplyFast	VEGA_PIXEL_FORMAT_CLASS::PremultiplyFast
#define VEGAPixelUnpremultiplyFast	VEGA_PIXEL_FORMAT_CLASS::UnpremultiplyFast

#define VEGA_PACK_RGB				VEGA_PIXEL_FORMAT_CLASS::PackRGB
#define VEGA_PACK_ARGB				VEGA_PIXEL_FORMAT_CLASS::PackARGB

#define VEGA_UNPACK_A				VEGA_PIXEL_FORMAT_CLASS::UnpackA
#define VEGA_UNPACK_R				VEGA_PIXEL_FORMAT_CLASS::UnpackR
#define VEGA_UNPACK_G				VEGA_PIXEL_FORMAT_CLASS::UnpackG
#define VEGA_UNPACK_B				VEGA_PIXEL_FORMAT_CLASS::UnpackB

#define VEGAConstPixelAccessor		VEGA_PIXEL_FORMAT_CLASS::ConstAccessor
#define VEGAPixelAccessor			VEGA_PIXEL_FORMAT_CLASS::Accessor

#define VEGAPixelPtr				VEGA_PIXEL_FORMAT_CLASS::Pointer

static inline VEGA_PIXEL ColorToVEGAPixel(UINT32 col)
{
	VEGA_PIXEL packed_color = VEGA_PACK_ARGB(col>>24, (col>>16)&0xff, (col>>8)&0xff, col&0xff);
#ifdef USE_PREMULTIPLIED_ALPHA
	packed_color = VEGAPixelPremultiply(packed_color);
#endif // USE_PREMULTIPLIED_ALPHA
	return packed_color;
}

// Temporarily (?) used by some pieces of the HW codepaths. Rectify if needed.
#define VEGAPixelPremultiply_BGRA8888 VEGAPixelFormat_BGRA8888::Premultiply

#endif // VEGAPIXELFORMAT_H
