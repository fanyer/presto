/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_USE_ASM)

#include "modules/libvega/src/vegapixelformat.h"

VEGADispatchTable::VEGADispatchTable()
{
	// Add fallback hooks.
	CompOver_8888 = VEGA_PIXEL_FORMAT_CLASS::CompOver;
	CompConstOver_8888 = VEGA_PIXEL_FORMAT_CLASS::CompConstOver;
	CompConstOverMask_8888 = VEGA_PIXEL_FORMAT_CLASS::CompConstOverMask;
	CompOverConstMask_8888 = VEGA_PIXEL_FORMAT_CLASS::CompOverConstMask;

	StoreTo_8888 = VEGAPixelAccessor::StoreTo;
	MoveFromTo_8888 = VEGAPixelAccessor::MoveFromTo;

	Sampler_NearestX_Opaque_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestX_Opaque;
	Sampler_NearestX_CompOver_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestX_CompOver;
	Sampler_NearestX_CompOverMask_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestX_CompOverMask;
	Sampler_NearestX_CompOverConstMask_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestX_CompOverConstMask;
	Sampler_NearestXY_CompOver_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestXY_CompOver;
	Sampler_NearestXY_CompOverMask_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_NearestXY_CompOverMask;

	Sampler_LerpX_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_LerpX;
	Sampler_LerpY_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_LerpY;
	Sampler_LerpXY_Opaque_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_LerpXY_Opaque;
	Sampler_LerpXY_CompOver_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_LerpXY_CompOver;
	Sampler_LerpXY_CompOverMask_8888 = VEGA_PIXEL_FORMAT_CLASS::Sampler_LerpXY_CompOverMask;

	ConvertFrom_RGBA8888 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::RGBA8888_Unpack, 4>;
	ConvertFrom_BGRA8888 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::BGRA8888_Unpack, 4>;
	ConvertFrom_ABGR8888 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::ABGR8888_Unpack, 4>;
	ConvertFrom_ARGB8888 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::ARGB8888_Unpack, 4>;
	ConvertFrom_BGRA4444 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::BGRA4444_Unpack, 2>;
	ConvertFrom_RGBA4444 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::RGBA4444_Unpack, 2>;
	ConvertFrom_ABGR4444 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGBA<VEGAFormatUnpack::ABGR4444_Unpack, 2>;
	ConvertFrom_RGB565 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGB<VEGAFormatUnpack::RGB565_Unpack, 2>;
	ConvertFrom_BGR565 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGB<VEGAFormatUnpack::BGR565_Unpack, 2>;
	ConvertFrom_RGB888 = VEGA_PIXEL_FORMAT_CLASS::UnpackFormatConvertRGB<VEGAFormatUnpack::RGB888_Unpack, 3>;

	// Add accelerated hooks depending on CPU architecture and available features.
	PopulateHooks();
}

#endif // defined(VEGA_SUPPORT) && defined(VEGA_USE_ASM)
