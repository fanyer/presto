/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)

#include "modules/libvega/src/vegapixelformat.h"

#include "modules/libvega/src/x86/vegacommon_x86.h"

static op_force_inline __m128i Lerp(__m128i a, __m128i b, __m128i t)
{
	__m128i ba = _mm_sub_epi16(b, a);
	__m128i t_ba = _mm_srli_epi16(_mm_mullo_epi16(ba, t), 8);
	return _mm_add_epi8(a, t_ba);
}

// This function is a lot more complicated than one would hope for
// since we needs to cater for the cases where fx == 0 or fy == 0
// could lead to accesses outside the source if dual-pixel access are
// done in all cases (VEGAImage generates that kind of requests).
static op_force_inline __m128i LerpRows(const UINT32* data, unsigned dataPixelStride,
										INT32 frac_x, INT32 frac_y)
{
	const __m128i zeros = _mm_setzero_si128();

	// This branch structure was chosen because it seemed to reduce
	// the impact of it being there at all - so if changing it verify
	// that there are no adverse effects.
	if (frac_y)
	{
		__m128i row0, row1;
		if (frac_x)
		{
			// Load row-samples and expand components to 16-bit.
			row0 = _mm_loadl_epi64((__m128i*)data);
			row1 = _mm_loadl_epi64((__m128i*)(data + dataPixelStride));
		}
		else
		{
			// Load row-samples and expand components to 16-bit.
			row0 = _mm_cvtsi32_si128(*data);
			row1 = _mm_cvtsi32_si128(*(data + dataPixelStride));
		}

		row0 = _mm_unpacklo_epi8(row0, zeros);
		row1 = _mm_unpacklo_epi8(row1, zeros);

		// Linearly interpolate between the two row-samples.
		return Lerp(row0, row1, _mm_set1_epi16(frac_y));
	}
	else
	{
		__m128i row0;
		if (frac_x)
			row0 = _mm_loadl_epi64((__m128i*)data);
		else
			row0 = _mm_cvtsi32_si128(*data);

		return _mm_unpacklo_epi8(row0, zeros);
	}
}

static op_force_inline __m128i Bilerp2(const UINT32* data, unsigned dataPixelStride,
									   INT32& csx, INT32& csy, INT32 cdx, INT32 cdy)
{
	const UINT32* s0addr = data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);

	int s0_fx = VEGA_SAMPLE_FRAC(csx);
	__m128i s0 = LerpRows(s0addr, dataPixelStride, s0_fx, VEGA_SAMPLE_FRAC(csy));

	csx += cdx;
	csy += cdy;

	const UINT32* s1addr = data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);

	int s1_fx = VEGA_SAMPLE_FRAC(csx);
	__m128i s1 = LerpRows(s1addr, dataPixelStride, s1_fx, VEGA_SAMPLE_FRAC(csy));

	csx += cdx;
	csy += cdy;

	// Interleave samples 0 and 1.
	__m128i r0 = _mm_unpacklo_epi64(s0, s1);
	__m128i r1 = _mm_unpackhi_epi64(s0, s1);

	// Setup x-weights.
	__m128i vfx = _mm_unpacklo_epi64(_mm_shufflelo_epi16(_mm_cvtsi32_si128(s0_fx),
														 _MM_SHUFFLE(0, 0, 0, 0)),
									 _mm_shufflelo_epi16(_mm_cvtsi32_si128(s1_fx),
														 _MM_SHUFFLE(0, 0, 0, 0)));

	// Interpolate between "column" samples.
	return Lerp(r0, r1, vfx);
}

static op_force_inline __m128i Bilerp1(const UINT32* data, unsigned dataPixelStride,
									   INT32 csx, INT32 csy)
{
	const UINT32* s0addr = data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);

	int s0_fx = VEGA_SAMPLE_FRAC(csx);
	__m128i s0 = LerpRows(s0addr, dataPixelStride, s0_fx, VEGA_SAMPLE_FRAC(csy));

	// Deinterleave
	const __m128i zeros = _mm_setzero_si128();
	__m128i r0 = _mm_unpacklo_epi64(s0, zeros);
	__m128i r1 = _mm_unpackhi_epi64(s0, zeros);

	// Setup x-weights.
	__m128i vfx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(s0_fx),
									  _MM_SHUFFLE(0, 0, 0, 0));

	// Interpolate between "column" samples.
	return Lerp(r0, r1, vfx);
}

extern "C" void Sampler_LerpXY_Opaque_SSE2(UINT32* color, const UINT32* data,
										   unsigned cnt, unsigned dataPixelStride,
										   INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	unsigned cnt2 = cnt / 2;
	while (cnt2)
	{
		__m128i s01 = Bilerp2(data, dataPixelStride, csx, csy, cdx, cdy);

		_mm_storel_epi64((__m128i*)color,
						 _mm_packus_epi16(s01, _mm_setzero_si128()));

		color += 2;
		cnt2--;
	}

	if (cnt & 1)
	{
		__m128i s0 = Bilerp1(data, dataPixelStride, csx, csy);

		*color = _mm_cvtsi128_si32(_mm_packus_epi16(s0, _mm_setzero_si128()));
	}
}

// Perform (dst.rgba * (256 - src.a) on two pixels (word components).
static op_force_inline __m128i CompOverW_SSE2(__m128i src, __m128i dst)
{
	// Broadcast the alpha channel to all the words.
	__m128i alpha = _mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3));
	alpha = _mm_shufflehi_epi16(alpha, _MM_SHUFFLE(3, 3, 3, 3));

	// Subtract each component from 256.
	__m128i res = _mm_subs_epu16(g_xmm_constants._256, alpha);

	// Multiply [dst0][dst1] with one minus src alpha.
	// Then shift down the result to bytes again.
	return MulW(dst, res);
}

static op_force_inline __m128i CompOver2_SSE2(__m128i src, __m128i dst)
{
	const __m128i zeros = _mm_setzero_si128();

	dst = _mm_unpacklo_epi8(dst, zeros);

	// d' = d * (256 - sa)
	dst = CompOverW_SSE2(src, dst);

	// d = d' + s
	dst = _mm_adds_epu8(dst, src);

	return _mm_packus_epi16(dst, zeros);
}

extern "C" void Sampler_LerpXY_CompOver_SSE2(UINT32* color, const UINT32* data,
											 unsigned cnt, unsigned dataPixelStride,
											 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	unsigned cnt2 = cnt / 2;
	while (cnt2)
	{
		__m128i s01 = Bilerp2(data, dataPixelStride, csx, csy, cdx, cdy);

		__m128i dst = _mm_loadl_epi64((__m128i*)color);

		dst = CompOver2_SSE2(s01, dst);

		_mm_storel_epi64((__m128i*)color, dst);

		color += 2;
		cnt2--;
	}

	if (cnt & 1)
	{
		__m128i s0 = Bilerp1(data, dataPixelStride, csx, csy);

		__m128i dst = _mm_cvtsi32_si128(*color);

		dst = CompOver2_SSE2(s0, dst);

		*color = _mm_cvtsi128_si32(dst);
	}
}

extern "C" void Sampler_LerpXY_CompOverMask_SSE2(UINT32* color, const UINT32* data, const UINT8* mask,
												 unsigned cnt, unsigned dataPixelStride,
												 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	unsigned cnt2 = cnt / 2;
	while (cnt2)
	{
		int a0 = *mask++;
		int a1 = *mask++;

		if (a0 || a1)
		{
			__m128i s01 = Bilerp2(data, dataPixelStride, csx, csy, cdx, cdy);

			// Modulate with mask.
			__m128i a_0 = _mm_cvtsi32_si128(a0 + 1);
			a_0 = _mm_shufflelo_epi16(a_0, _MM_SHUFFLE(0, 0, 0, 0));
			__m128i a_1 = _mm_cvtsi32_si128(a1 + 1);
			a_1 = _mm_shufflelo_epi16(a_1, _MM_SHUFFLE(0, 0, 0, 0));
			__m128i a_01 = _mm_unpacklo_epi64(a_0, a_1);

			s01 = MulW(s01, a_01);

			// Composite
			__m128i dst = _mm_loadl_epi64((__m128i*)color);

			dst = CompOver2_SSE2(s01, dst);

			_mm_storel_epi64((__m128i*)color, dst);
		}
		else
		{
			csx += 2 * cdx;
			csy += 2 * cdy;
		}

		color += 2;
		cnt2--;
	}

	if ((cnt & 1) && *mask)
	{
		__m128i s0 = Bilerp1(data, dataPixelStride, csx, csy);

		// Modulate with mask.
		__m128i a8 = _mm_cvtsi32_si128(*mask + 1);
		a8 = _mm_shufflelo_epi16(a8, _MM_SHUFFLE(0, 0, 0, 0));

		s0 = MulW(s0, a8);

		// Composite
		__m128i dst = _mm_cvtsi32_si128(*color);

		dst = CompOver2_SSE2(s0, dst);

		*color = _mm_cvtsi128_si32(dst);
	}
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
