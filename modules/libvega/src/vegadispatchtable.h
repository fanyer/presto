/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGADISPATCHTABLE_H
#define VEGADISPATCHTABLE_H

#ifdef VEGA_SUPPORT
#if defined(VEGA_USE_ASM)

/** Collection of faster versions of common VEGA functions that rely on
 * specific CPU features where possible and fall back to regular C++
 * when not. */
class VEGADispatchTable
{
private:
	/** Initialize the function pointers depending on available CPU features. */
	void PopulateHooks();

public:
	/** Create a dispatch table depending on available CPU architecture and features. */
	VEGADispatchTable();

	/*
	 * Compositing
	 */

	void (*CompOver_8888)(UINT32* dp, const UINT32* sp, unsigned len);
	void (*CompConstOver_8888)(UINT32* dp, UINT32 s, unsigned len);
	void (*CompConstOverMask_8888)(UINT32* dp, UINT32 s, const UINT8* mask, unsigned len);
	void (*CompOverConstMask_8888)(UINT32* dp, const UINT32* sp, UINT32 mask, unsigned len);

	/*
	 * Sampling using a `nearest (neighbor)' filter.
	 */

	void (*Sampler_NearestX_Opaque_8888)(UINT32* color, const UINT32* data,
										 unsigned cnt, INT32 csx, INT32 cdx);
	void (*Sampler_NearestX_CompOver_8888)(UINT32* color, const UINT32* data,
										   unsigned cnt, INT32 csx, INT32 cdx);
	void (*Sampler_NearestX_CompOverMask_8888)(UINT32* color, const UINT32* data, const UINT8* mask,
											   unsigned cnt, INT32 csx, INT32 cdx);
	void (*Sampler_NearestX_CompOverConstMask_8888)(UINT32* color, const UINT32* data, UINT32 mask,
													unsigned cnt, INT32 csx, INT32 cdx);
	void (*Sampler_NearestXY_CompOver_8888)(UINT32* color, const UINT32* data,
											unsigned count, unsigned dataPixelStride,
											INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void (*Sampler_NearestXY_CompOverMask_8888)(UINT32* color, const UINT32* data, const UINT8* mask,
												unsigned count, unsigned dataPixelStride,
												INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	/*
	 * Sampling using a `linear interpolation' filter.
	 */

	void (*Sampler_LerpX_8888)(UINT32* dst, const UINT32* src, INT32 csx, INT32 cdx, unsigned dstlen, unsigned srclen);
	void (*Sampler_LerpY_8888)(UINT32* dst, const UINT32* src1, const UINT32* src2, INT32 frc_y, unsigned len);
	void (*Sampler_LerpXY_Opaque_8888)(UINT32* color, const UINT32* data,
									   unsigned count, unsigned dataPixelStride,
									   INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void (*Sampler_LerpXY_CompOver_8888)(UINT32* color, const UINT32* data,
										 unsigned count, unsigned dataPixelStride,
										 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	void (*Sampler_LerpXY_CompOverMask_8888)(UINT32* color, const UINT32* data, const UINT8* mask,
											 unsigned count, unsigned dataPixelStride,
											 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	/*
	 * Format conversion
	 */

	void (*ConvertFrom_BGRA8888)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_RGBA8888)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_ABGR8888)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_ARGB8888)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_BGRA4444)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_RGBA4444)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_ABGR4444)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_RGB565)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_BGR565)(UINT32* dst, const void* src, unsigned num);
	void (*ConvertFrom_RGB888)(UINT32* dst, const void* src, unsigned num);

	/*
	 * Data movement
	 */

	void (*StoreTo_8888)(UINT32* dst, UINT32 s, unsigned len);
	void (*MoveFromTo_8888)(UINT32* dst, const UINT32* src, unsigned len);

private:
#ifdef VEGA_VERIFY_SIMD
	bool VerifySSSE3();
#endif // VEGA_VERIFY_SIMD
};

#endif // VEGA_USE_ASM

#endif // VEGA_SUPPORT
#endif // VEGADISPATCHTABLE_H

