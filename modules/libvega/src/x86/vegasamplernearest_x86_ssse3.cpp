/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)

#include <tmmintrin.h>

#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/src/x86/vegacommon_x86.h"
#include "modules/libvega/src/x86/vegacompositeover_x86.h"
#include "modules/libvega/src/x86/vegaremainder_x86.h"

static op_force_inline __m128i Sampler_NearestX4(const UINT32* src, INT32& csx, INT32 cdx)
{
	// Get four samples.
	__m128i s0 = _mm_cvtsi32_si128(SampleX(src, csx, cdx));
	__m128i s1 = _mm_cvtsi32_si128(SampleX(src, csx, cdx));
	__m128i s2 = _mm_cvtsi32_si128(SampleX(src, csx, cdx));
	__m128i s3 = _mm_cvtsi32_si128(SampleX(src, csx, cdx));

	// Merge them into one register.
	s0 = Merge<4>(s0, s1);
	s2 = Merge<4>(s2, s3);
	s0 = Merge<8>(s0, s2);
	return s0;
}

static op_force_inline __m128i Sampler_NearestXY4(const UINT32* src, unsigned int stride, INT32& csx, INT32 cdx, INT32& csy, INT32 cdy)
{
	// Get four samples.
	__m128i s0 = _mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy));
	__m128i s1 = _mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy));
	__m128i s2 = _mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy));
	__m128i s3 = _mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy));

	// Merge them into one register.
	s0 = Merge<4>(s0, s1);
	s2 = Merge<4>(s2, s3);
	s0 = Merge<8>(s0, s2);
	return s0;
}

extern "C" void Sampler_NearestX_SSSE3(UINT32* dst, const UINT32* src, unsigned int cnt, INT32 csx, INT32 cdx)
{
	// Simple unrolling is the best we can do in this case and 4 seems to be the sweet spot.
	unsigned int length = cnt / 4;
	while (length--)
	{
		*dst++ = SampleX(src, csx, cdx);
		*dst++ = SampleX(src, csx, cdx);
		*dst++ = SampleX(src, csx, cdx);
		*dst++ = SampleX(src, csx, cdx);
	}

	// Take care of the remaining few pixels.
	cnt &= 3;
	while (cnt--)
		*dst++ = SampleX(src, csx, cdx);
}

extern "C" void Sampler_NearestX_CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned cnt, INT32 csx, INT32 cdx)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	__m128i src4, dst4;

	// Check if we can use aligned memory access.
	if (cnt > 3)
	{
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder_SamplerX(src4, dst4, src, dst, csx, cdx, unaligned);
			StoreRemainder(CompOver4(src4, dst4), dst, unaligned);

			dst += unaligned;
			cnt -= unaligned;
		}

		unsigned int length = cnt / 4;
		while (length-- > 0)
		{
			src4 = Sampler_NearestX4(src, csx, cdx);
			dst4 = _mm_load_si128((const __m128i *)dst);

			_mm_store_si128((__m128i *)dst, CompOver4(src4, dst4));

			dst += 4;
		}
	}

	unsigned int length = cnt & 3;
	if (length)
	{
		LoadRemainder_SamplerX(src4, dst4, src, dst, csx, cdx, length);
		StoreRemainder(CompOver4(src4, dst4), dst, length);
	}
}

extern "C" void Sampler_NearestX_CompOverMask_SSSE3(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, INT32 csx, INT32 cdx)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	if (cnt > 3)
	{
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			__m128i src4, dst4;
			UINT8 mask[4];
			LoadRemainder_SamplerX(src4, dst4, mask, src, dst, alpha, csx, cdx, unaligned);
			StoreRemainder(CompOverMask4(src4, dst4, mask), dst, unaligned);

			dst += unaligned;
			alpha += unaligned;
			cnt -= unaligned;
		}

		unsigned int length = cnt / 4;
		while (length-- > 0)
		{
			__m128i src4 = Sampler_NearestX4(src, csx, cdx);
			__m128i dst4 = _mm_load_si128((const __m128i *)dst);

			_mm_store_si128((__m128i *)dst, CompOverMask4(src4, dst4, alpha));

			dst += 4;
			alpha += 4;
		}
	}

	unsigned int length = cnt & 3;
	if (length)
	{
		__m128i src4, dst4;
		UINT8 mask[4];
		LoadRemainder_SamplerX(src4, dst4, mask, src, dst, alpha, csx, cdx, length);
		StoreRemainder(CompOverMask4(src4, dst4, mask), dst, length);
	}
}

extern "C" void Sampler_NearestX_CompOverConstMask_SSSE3(UINT32* dst, const UINT32* src, UINT32 opacity, unsigned cnt, INT32 csx, INT32 cdx)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	// Unpack the mask into word components.
	__m128i mask4 = _mm_cvtsi32_si128(opacity + 1);
	mask4 = _mm_shuffle_epi8(mask4, g_xmm_constants.broadcast_word);

	__m128i src4, dst4;
	if (cnt > 3)
	{
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder_SamplerX(src4, dst4, src, dst, csx, cdx, unaligned);
			StoreRemainder(CompOverConstMask4(src4, dst4, mask4), dst, unaligned);

			dst += unaligned;
			cnt -= unaligned;
		}

		unsigned int length = cnt / 4;
		while (length-- > 0)
		{
			src4 = Sampler_NearestX4(src, csx, cdx);
			dst4 = _mm_load_si128((const __m128i *)dst);

			_mm_store_si128((__m128i *)dst, CompOverConstMask4(src4, dst4, mask4));

			dst += 4;
		}
	}

	unsigned int length = cnt & 3;
	if (length)
	{
		LoadRemainder_SamplerX(src4, dst4, src, dst, csx, cdx, length);
		StoreRemainder(CompOverConstMask4(src4, dst4, mask4), dst, length);
	}
}

extern "C" void Sampler_NearestXY_CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	__m128i src4, dst4;
	if (cnt > 3)
	{
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder_SamplerXY(src4, dst4, src, dst, stride, csx, cdx, csy, cdy, unaligned);
			StoreRemainder(CompOver4(src4, dst4), dst, unaligned);

			dst += unaligned;
			cnt -= unaligned;
		}

		unsigned int length = cnt / 4;
		while (length-- > 0)
		{
			src4 = Sampler_NearestXY4(src, stride, csx, cdx, csy, cdy);
			dst4 = _mm_load_si128((const __m128i *)dst);

			_mm_store_si128((__m128i *)dst, CompOver4(src4, dst4));

			dst += 4;
		}
	}

	unsigned int length = cnt & 3;
	if (length)
	{
		LoadRemainder_SamplerXY(src4, dst4, src, dst, stride, csx, cdx, csy, cdy, length);
		StoreRemainder(CompOver4(src4, dst4), dst, length);
	}
}

extern "C" void Sampler_NearestXY_CompOverMask_SSSE3(UINT32* dst, const UINT32* src, const UINT8* alpha, unsigned cnt, unsigned stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	__m128i src4, dst4;
	if (cnt > 3)
	{
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			UINT8 mask[4];
			LoadRemainder_SamplerXY(src4, dst4, mask, src, dst, alpha, stride, csx, cdx, csy, cdy, unaligned);
			StoreRemainder(CompOverMask4(src4, dst4, mask), dst, unaligned);

			dst += unaligned;
			alpha += unaligned;
			cnt -= unaligned;
		}

		unsigned int length = cnt / 4;
		while (length-- > 0)
		{
			src4 = Sampler_NearestXY4(src, stride, csx, cdx, csy, cdy);
			dst4 = _mm_load_si128((const __m128i *)dst);

			_mm_store_si128((__m128i *)dst, CompOverMask4(src4, dst4, alpha));

			dst += 4;
			alpha += 4;
		}
	}

	unsigned int length = cnt & 3;
	if (length)
	{
		UINT8 mask[4];
		LoadRemainder_SamplerXY(src4, dst4, mask, src, dst, alpha, stride, csx, cdx, csy, cdy, length);
		StoreRemainder(CompOverMask4(src4, dst4, mask), dst, length);
	}
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
