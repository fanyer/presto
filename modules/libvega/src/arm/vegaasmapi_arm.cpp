/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_USE_ASM) && defined(ARCHITECTURE_ARM)

#include "modules/libvega/src/vegadispatchtable.h"
#include "modules/libvega/src/arm/vegaconfig_arm.h"

extern "C" {
	// These are all written for PreMultiplied data and assume a 32 bit pixel format where alpha shift is 24,
	// i.e. VEGAPixelFormat_8888<16,8,0,24> or VEGAPixelFormat_8888<0,8,16,24>.

	// ARMv6 versions
	void MoveFrom_ARMv6(UINT32* dst, const UINT32* src, unsigned len);

	void CompOver_ARMv6(UINT32* dst, const UINT32* src, unsigned len);
	void CompConstOver_ARMv6(UINT32* dst, unsigned src, unsigned len);
	void CompConstOverMask_ARMv6(UINT32* dst, unsigned src, const UINT8* mask, unsigned len);
	void CompOverConstMask_ARMv6(UINT32* dst, const UINT32* src, unsigned mask, unsigned len);

	void Sampler_NearestX_ARMv6(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOver_ARMv6(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverMask_ARMv6(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverConstMask_ARMv6(UINT32* dst, const UINT32* src, UINT32 alpha, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestXY_CompOver_ARMv6(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_NearestXY_CompOverMask_ARMv6(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	void Sampler_LerpX_ARMv6(UINT32* dst, const UINT32* src, INT32 csx, INT32 cdx, unsigned dst_len, unsigned src_len);
	void Sampler_LerpY_ARMv6(UINT32* dst, const UINT32* src_a, const UINT32* src_b, INT32 frc_y, unsigned len);

	void Convert_BGRA8888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_RGBA8888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_ABGR8888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_ARGB8888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_BGRA4444_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_RGBA4444_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_ABGR4444_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_ARGB4444_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_RGB565_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_BGR565_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_RGB888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);
	void Convert_BGR888_to_BGRA8888_ARMv6(UINT32* dst, const void* src, unsigned num);

#ifdef ARCHITECTURE_ARM_NEON
	// ARM NEON versions
	void MoveFrom_NEON(UINT32* dst, const UINT32* src, unsigned len);

	void CompOver_NEON(UINT32* dst, const UINT32* src, unsigned len);
	void CompConstOver_NEON(UINT32* dst, unsigned src, unsigned len);
	void CompConstOverMask_NEON(UINT32* dst, unsigned src, const UINT8* mask, unsigned len);
	void CompOverConstMask_NEON(UINT32* dst, const UINT32* src, unsigned mask, unsigned len);

	void Sampler_NearestX_CompOver_NEON(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverMask_NEON(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverConstMask_NEON(UINT32* dst, const UINT32* src, UINT32 alpha, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestXY_CompOver_NEON(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_NearestXY_CompOverMask_NEON(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	void Sampler_LerpX_NEON(UINT32* dst, const UINT32* src, INT32 csx, INT32 cdx, unsigned dst_len, unsigned src_len);
	void Sampler_LerpY_NEON(UINT32* dst, const UINT32* src_a, const UINT32* src_b, INT32 frc_y, unsigned len);

	void Convert_BGRA8888_to_BGRA8888_NEON(UINT32* dst, const void* src, unsigned num);
	void Convert_RGBA8888_to_BGRA8888_NEON(UINT32* dst, const void* src, unsigned num);
	void Convert_ABGR8888_to_BGRA8888_NEON(UINT32* dst, const void* src, unsigned num);
	void Convert_ARGB8888_to_BGRA8888_NEON(UINT32* dst, const void* src, unsigned num);
#endif // ARCHITECTURE_ARM_NEON
}

// Map ARMv6 BGRA conversion routines to RGBA based on their functionality
#define Convert_BGRA8888_to_RGBA8888_ARMv6 Convert_RGBA8888_to_BGRA8888_ARMv6	// Reverse RGB order
#define Convert_RGBA8888_to_RGBA8888_ARMv6 Convert_BGRA8888_to_BGRA8888_ARMv6	// Memory copy
#define Convert_ABGR8888_to_RGBA8888_ARMv6 Convert_ARGB8888_to_BGRA8888_ARMv6	// Reverse byte order
#define Convert_ARGB8888_to_RGBA8888_ARMv6 Convert_ABGR8888_to_BGRA8888_ARMv6	// Shift alpha into bits 24-31
#define Convert_BGRA4444_to_RGBA8888_ARMv6 Convert_RGBA4444_to_BGRA8888_ARMv6	// Reverse RGB order
#define Convert_RGBA4444_to_RGBA8888_ARMv6 Convert_BGRA4444_to_BGRA8888_ARMv6	// 1:1
#define Convert_ABGR4444_to_RGBA8888_ARMv6 Convert_ARGB4444_to_BGRA8888_ARMv6	// Reverse byte order
#define Convert_RGB565_to_RGBA8888_ARMv6   Convert_BGR565_to_BGRA8888_ARMv6		// 1:1
#define Convert_BGR565_to_RGBA8888_ARMv6   Convert_RGB565_to_BGRA8888_ARMv6		// Reverse RGB order
#define Convert_RGB888_to_RGBA8888_ARMv6   Convert_BGR888_to_BGRA8888_ARMv6		// 1:1
#define Convert_BGR888_to_RGBA8888_ARMv6   Convert_RGB888_to_BGRA8888_ARMv6		// Reverse RGB order

#ifdef ARCHITECTURE_ARM_NEON
// Map NEON BGRA conversion routines to RGBA based on their functionality
#define Convert_BGRA8888_to_RGBA8888_NEON Convert_RGBA8888_to_BGRA8888_NEON		// Reverse RGB order
#define Convert_RGBA8888_to_RGBA8888_NEON Convert_BGRA8888_to_BGRA8888_NEON		// Memory copy
#define Convert_ABGR8888_to_RGBA8888_NEON Convert_ARGB8888_to_BGRA8888_NEON		// Reverse byte order
#define Convert_ARGB8888_to_RGBA8888_NEON Convert_ABGR8888_to_BGRA8888_NEON		// Shift alpha into bits 24-31
#endif // ARCHITECTURE_ARM_NEON

void VEGADispatchTable::PopulateHooks()
{
	if (g_op_system_info->GetCPUArchitecture() >= OpSystemInfo::CPU_ARCHITECTURE_ARMV6)
	{
		MoveFromTo_8888 = MoveFrom_ARMv6;
		CompOver_8888 = CompOver_ARMv6;
		CompConstOver_8888 = CompConstOver_ARMv6;
		CompConstOverMask_8888 = CompConstOverMask_ARMv6;
		CompOverConstMask_8888 = CompOverConstMask_ARMv6;

		Sampler_NearestX_Opaque_8888 = Sampler_NearestX_ARMv6;
		Sampler_NearestX_CompOver_8888 = Sampler_NearestX_CompOver_ARMv6;
		Sampler_NearestX_CompOverMask_8888 = Sampler_NearestX_CompOverMask_ARMv6;
		Sampler_NearestX_CompOverConstMask_8888 = Sampler_NearestX_CompOverConstMask_ARMv6;
		Sampler_NearestXY_CompOver_8888 = Sampler_NearestXY_CompOver_ARMv6;
		Sampler_NearestXY_CompOverMask_8888 = Sampler_NearestXY_CompOverMask_ARMv6;

		Sampler_LerpX_8888 = Sampler_LerpX_ARMv6;
		Sampler_LerpY_8888 = Sampler_LerpY_ARMv6;

#if defined(VEGA_INTERNAL_FORMAT_BGRA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_BGRA8888_ARMv6;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_BGRA8888_ARMv6;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_BGRA8888_ARMv6;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_BGRA8888_ARMv6;
		ConvertFrom_BGRA4444 = Convert_BGRA4444_to_BGRA8888_ARMv6;
		ConvertFrom_RGBA4444 = Convert_RGBA4444_to_BGRA8888_ARMv6;
		ConvertFrom_ABGR4444 = Convert_ABGR4444_to_BGRA8888_ARMv6;
		ConvertFrom_RGB565   = Convert_RGB565_to_BGRA8888_ARMv6;
		ConvertFrom_BGR565   = Convert_BGR565_to_BGRA8888_ARMv6;
		ConvertFrom_RGB888   = Convert_RGB888_to_BGRA8888_ARMv6;
#elif defined(VEGA_INTERNAL_FORMAT_RGBA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_RGBA8888_ARMv6;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_RGBA8888_ARMv6;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_RGBA8888_ARMv6;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_RGBA8888_ARMv6;
		ConvertFrom_BGRA4444 = Convert_BGRA4444_to_RGBA8888_ARMv6;
		ConvertFrom_RGBA4444 = Convert_RGBA4444_to_RGBA8888_ARMv6;
		ConvertFrom_ABGR4444 = Convert_ABGR4444_to_RGBA8888_ARMv6;
		ConvertFrom_RGB565   = Convert_RGB565_to_RGBA8888_ARMv6;
		ConvertFrom_BGR565   = Convert_BGR565_to_RGBA8888_ARMv6;
		ConvertFrom_RGB888   = Convert_RGB888_to_RGBA8888_ARMv6;
#endif
	}

#ifdef ARCHITECTURE_ARM_NEON
	if (g_op_system_info->GetCPUFeatures() & OpSystemInfo::CPU_FEATURES_ARM_NEON)
	{
		MoveFromTo_8888 = MoveFrom_NEON;

		CompOver_8888 = CompOver_NEON;
		CompConstOver_8888 = CompConstOver_NEON;
		CompConstOverMask_8888 = CompConstOverMask_NEON;
		CompOverConstMask_8888 = CompOverConstMask_NEON;

		Sampler_NearestX_CompOver_8888 = Sampler_NearestX_CompOver_NEON;
		Sampler_NearestX_CompOverMask_8888 = Sampler_NearestX_CompOverMask_NEON;
		Sampler_NearestX_CompOverConstMask_8888 = Sampler_NearestX_CompOverConstMask_NEON;
		Sampler_NearestXY_CompOver_8888 = Sampler_NearestXY_CompOver_NEON;
		Sampler_NearestXY_CompOverMask_8888 = Sampler_NearestXY_CompOverMask_NEON;

		Sampler_LerpX_8888 = Sampler_LerpX_NEON;
		Sampler_LerpY_8888 = Sampler_LerpY_NEON;

#if defined(VEGA_INTERNAL_FORMAT_BGRA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_BGRA8888_NEON;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_BGRA8888_NEON;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_BGRA8888_NEON;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_BGRA8888_NEON;
#elif defined(VEGA_INTERNAL_FORMAT_RGBA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_RGBA8888_NEON;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_RGBA8888_NEON;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_RGBA8888_NEON;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_RGBA8888_NEON;
#endif
	}
#endif // ARCHITECTURE_ARM_NEON
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_ARM)
