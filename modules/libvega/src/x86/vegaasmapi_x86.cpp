/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)

#include "modules/libvega/src/vegadispatchtable.h"
#include "modules/pi/OpSystemInfo.h"

extern "C" {
	void VEGAInitXmmConstants(void);

	void Store_SSE2(UINT32* dst, UINT32 rgba, unsigned len);

	void CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned len);
	void CompConstOver_SSSE3(UINT32* dst, UINT32 src, unsigned len);
	void CompConstOverMask_SSSE3(UINT32* dst, UINT32 src, const UINT8* mask, unsigned len);
	void CompOverConstMask_SSSE3(UINT32* dst, const UINT32* src, unsigned mask, unsigned len);
	void Sampler_NearestX_SSSE3(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverMask_SSSE3(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestX_CompOverConstMask_SSSE3(UINT32* dst, const UINT32* src, UINT32 opacity, unsigned cnt, INT32 csx, INT32 cdx);
	void Sampler_NearestXY_CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_NearestXY_CompOverMask_SSSE3(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_LerpX_SSSE3(UINT32* dst, const UINT32* src, INT32 csx, INT32 cdx, unsigned dst_len, unsigned src_len);
	void Sampler_LerpY_SSSE3(UINT32* dst, const UINT32* src_a, const UINT32* src_b, INT32 frc_y, unsigned len);
	void Sampler_LerpXY_Opaque_SSE2(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_LerpXY_CompOver_SSE2(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Sampler_LerpXY_CompOverMask_SSE2(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void Convert_BGRA8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num);
	void Convert_RGBA8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num);
	void Convert_ABGR8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num);
	void Convert_ARGB8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num);
}

// Map SSSE3 BGRA conversion routines to RGBA based on their functionality.
#define Convert_BGRA8888_to_RGBA8888_SSSE3 Convert_RGBA8888_to_BGRA8888_SSSE3	// Reverse RGB order.
#define Convert_RGBA8888_to_RGBA8888_SSSE3 Convert_BGRA8888_to_BGRA8888_SSSE3	// Memory copy.
#define Convert_ABGR8888_to_RGBA8888_SSSE3 Convert_ARGB8888_to_BGRA8888_SSSE3	// Reverse byte order.
#define Convert_ARGB8888_to_RGBA8888_SSSE3 Convert_ABGR8888_to_BGRA8888_SSSE3	// Shift alpha into bits 24-31.

#ifdef VEGA_VERIFY_SIMD
bool VEGADispatchTable::VerifySSSE3()
{
	const unsigned RUN_LENGTH = 16;
	UINT32 src[RUN_LENGTH];
	UINT32 dst_c[RUN_LENGTH];
	UINT32 dst_ssse3[RUN_LENGTH];

	op_memset(src, 32, sizeof(UINT32) * RUN_LENGTH);
	op_memset(dst_c, 16, sizeof(UINT32) * RUN_LENGTH);
	op_memset(dst_ssse3, 16, sizeof(UINT32) * RUN_LENGTH);

	CompOver_8888(dst_c, src, RUN_LENGTH);
	CompOver_SSSE3(dst_ssse3, src, RUN_LENGTH);

	if (0 != op_memcmp(dst_c, dst_ssse3, sizeof(UINT32) * RUN_LENGTH))
		return false;
	else
		return true;
}
#endif // VEGA_VERIFY_SIMD

void VEGADispatchTable::PopulateHooks()
{
	if (g_op_system_info->GetCPUFeatures() & OpSystemInfo::CPU_FEATURES_IA32_SSE2)
	{
		VEGAInitXmmConstants();

		StoreTo_8888 = Store_SSE2;

		Sampler_LerpXY_Opaque_8888 = Sampler_LerpXY_Opaque_SSE2;
		Sampler_LerpXY_CompOver_8888 = Sampler_LerpXY_CompOver_SSE2;
		Sampler_LerpXY_CompOverMask_8888 = Sampler_LerpXY_CompOverMask_SSE2;
	}

	if (g_op_system_info->GetCPUFeatures() & OpSystemInfo::CPU_FEATURES_IA32_SSSE3
#ifdef VEGA_VERIFY_SIMD
		&& VerifySSSE3()
#endif // VEGA_VERIFY_SIMD
	   )
	{
		CompOver_8888 = CompOver_SSSE3;
		CompConstOver_8888 = CompConstOver_SSSE3;
		CompConstOverMask_8888 = CompConstOverMask_SSSE3;
		CompOverConstMask_8888 = CompOverConstMask_SSSE3;
		Sampler_NearestX_Opaque_8888 = Sampler_NearestX_SSSE3;
		Sampler_NearestX_CompOver_8888 = Sampler_NearestX_CompOver_SSSE3;
		Sampler_NearestX_CompOverMask_8888 = Sampler_NearestX_CompOverMask_SSSE3;
		Sampler_NearestX_CompOverConstMask_8888 = Sampler_NearestX_CompOverConstMask_SSSE3;
		Sampler_NearestXY_CompOver_8888 = Sampler_NearestXY_CompOver_SSSE3;
		Sampler_NearestXY_CompOverMask_8888 = Sampler_NearestXY_CompOverMask_SSSE3;
		Sampler_LerpX_8888 = Sampler_LerpX_SSSE3;
		Sampler_LerpY_8888 = Sampler_LerpY_SSSE3;

#if defined(VEGA_INTERNAL_FORMAT_BGRA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_BGRA8888_SSSE3;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_BGRA8888_SSSE3;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_BGRA8888_SSSE3;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_BGRA8888_SSSE3;
#elif defined(VEGA_INTERNAL_FORMAT_RGBA8888)
		ConvertFrom_BGRA8888 = Convert_BGRA8888_to_RGBA8888_SSSE3;
		ConvertFrom_RGBA8888 = Convert_RGBA8888_to_RGBA8888_SSSE3;
		ConvertFrom_ABGR8888 = Convert_ABGR8888_to_RGBA8888_SSSE3;
		ConvertFrom_ARGB8888 = Convert_ARGB8888_to_RGBA8888_SSSE3;
#endif
	}
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
