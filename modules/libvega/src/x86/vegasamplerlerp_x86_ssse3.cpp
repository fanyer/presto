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
#include "modules/libvega/src/x86/vegaremainder_x86.h"

// Expand the two lower pixels into word components and then linearly
// interpolate between them.
static op_force_inline __m128i LerpLo(__m128i a, __m128i b, __m128i m)
{
	return MulW(SubW_Lo(a, b, _mm_setzero_si128()), m);
}

// Expand the two higher pixels into word components and then linearly
// interpolate between them.
static op_force_inline __m128i LerpHi(__m128i a, __m128i b, __m128i m)
{
	return MulW(SubW_Hi(a, b, _mm_setzero_si128()), m);
}

// (src_b - src_a) * frc + src_a, but only on the lower two pixels.
static op_force_inline __m128i Lerp2(__m128i src_a, __m128i src_b, __m128i frc)
{
	// Unpack the fraction into two registers expanded to word components.
	__m128i frc01 = _mm_shuffle_epi8(frc, g_xmm_constants.broadcast_b2w_lo);

	// (src_b - src_a) * frc.
	__m128i result = _mm_packus_epi16(LerpLo(src_a, src_b, frc01), _mm_setzero_si128());

	// Accumulate.
	return _mm_add_epi8(src_a, result);
}

// (src_b - src_a) * frc + src_a, on four pixels.
static op_force_inline __m128i Lerp4(__m128i src_a, __m128i src_b, __m128i frc)
{
	// Unpack the fraction into two registers expanded to word components.
	__m128i frc01 = _mm_shuffle_epi8(frc, g_xmm_constants.broadcast_b2w_lo);
	__m128i frc23 = _mm_shuffle_epi8(frc, g_xmm_constants.broadcast_b2w_hi);

	// (src_b - src_a) * frc.
	__m128i result = _mm_packus_epi16(LerpLo(src_a, src_b, frc01), LerpHi(src_a, src_b, frc23));

	// Accumulate.
	return _mm_add_epi8(src_a, result);
}

// Hack:
// MSVC2010 isn't generating good code for Sampler_LerpX_SSSE3().
// When building 32 bit with Full Optimization, it's 20+% slower using
// inline functions than macros. It's spilling to memory instead of
// using registers.
#define GET_SOURCE(color, next, frc)				\
{													\
	int_px = csx >> VEGA_SAMPLER_FRACBITS;			\
	color = _mm_cvtsi32_si128(src[int_px + 0]);		\
	next = _mm_cvtsi32_si128(src[int_px + 1]);		\
	int_frc = csx >> (VEGA_SAMPLER_FRACBITS - 8);	\
	int_frc &= 255;									\
	frc = _mm_cvtsi32_si128(int_frc);				\
}

#define MERGE_SOURCE(shift)				\
{										\
	color4 = Merge<shift>(color4, c4);	\
	next4 = Merge<shift>(next4, n4);	\
	frc4 = Merge<shift>(frc4, f4);		\
}

extern "C" void Sampler_LerpX_SSSE3(UINT32* dst, const UINT32* src, INT32 csx, INT32 cdx, unsigned dst_len, unsigned src_len)
{
	// Left border color if interpolating into the first source pixel.
	while (dst_len && csx < 0)
	{
		*dst++ = src[0];

		csx += cdx;
		dst_len--;
	}

	// The edge source index that we can't go past.
	INT32 end_csx = VEGA_INTTOSAMPLE(src_len - 1);

	// We're stepping four pixels at a time, but the fixed point position is already
	// one step ahead when the comparison is done.
	INT32 cdx3 = cdx * 3;

	// Help the compiler to avoid spilling into memory needlessly.
	__m128i color4, next4, frc4, c4, n4, f4;
	INT32 int_px, int_frc;

	// Inner loop processing four pixels at a time.
	unsigned int dst_len4 = dst_len / 4;
	while (dst_len4 && (csx + cdx3 < end_csx))
	{
		// Read up four pixels, their "next" pixel and the fractional part for each.
		// color4 will hold src[int_px + 0] for four consequtive pixels.
		// next4 will hold src[int_px + 1] for four consequtive pixels.
		// frc4 will the the fraction.
		GET_SOURCE(color4, next4, frc4);
		csx += cdx;
		GET_SOURCE(c4, n4, f4);
		MERGE_SOURCE(4);
		csx += cdx;
		GET_SOURCE(c4, n4, f4);
		MERGE_SOURCE(8);
		csx += cdx;
		GET_SOURCE(c4, n4, f4);
		MERGE_SOURCE(12);
		csx += cdx;

		// Linearly interpolate the four pixels and write the result out to
		// memory. The performance hit of using unaligned memory writes is hidden
		// by the amount of processing done per memory write.
		_mm_storeu_si128((__m128i *)dst, Lerp4(color4, next4, frc4));

		dst += 4;
		--dst_len4;
	}

	// Restore the count as we might have exited the unrolled loop
	// early due to (csx > cdx).
	dst_len &= 3;
	dst_len += dst_len4 * 4;

	// Handle the remainder of pixels one at a time.
	while ((dst_len > 0) && (csx < end_csx))
	{
		GET_SOURCE(color4, next4, frc4);
		csx += cdx;

		*dst++ = _mm_cvtsi128_si32(Lerp2(color4, next4, frc4));
		--dst_len;
	}

	// Right border color if interpolating after the last source pixel.
	while (dst_len-- > 0)
		*dst++ = src[src_len - 1];
}

static inline __m128i LerpConstFrac(__m128i a, __m128i b, __m128i frc)
{
	// (b - a) * frc
	//
	// Expand the four pixels into two separate registers as word components.
	// Then do the subtract and multiply with the fraction.
	// Finally pack them back into one register as byte components.
	const __m128i zero = _mm_setzero_si128();
	__m128i result = _mm_packus_epi16(MulW(SubW_Lo(a, b, zero), frc), MulW(SubW_Hi(a, b, zero), frc));

	// Accumulate.
	return _mm_add_epi8(a, result);
}

extern "C" void Sampler_LerpY_SSSE3(UINT32* dst, const UINT32* src_a, const UINT32* src_b, INT32 frc_y, unsigned len)
{
	// Expand the fraction 8 times as words.
	__m128i frc4 = _mm_shuffle_epi8(_mm_cvtsi32_si128(frc_y), g_xmm_constants.broadcast_word);

	unsigned int length = len / 4;
	unsigned int offset = 0;

	// Check if we need to use unaligned memory accesses.
	__m128i src_a4, src_b4;
	if (!IsPtrAligned(dst, 16) || !IsPtrAligned(src_a, 16) || !IsPtrAligned(src_b, 16))
	{
		while (length--)
		{
			src_a4 = _mm_loadu_si128((const __m128i *)(src_a + offset));
			src_b4 = _mm_loadu_si128((const __m128i *)(src_b + offset));

			_mm_storeu_si128((__m128i *)(dst + offset), LerpConstFrac(src_a4, src_b4, frc4));

			offset += 4;
		}
	}
	else
	{
		while (length--)
		{
			src_a4 = _mm_load_si128((const __m128i *)(src_a + offset));
			src_b4 = _mm_load_si128((const __m128i *)(src_b + offset));

			_mm_store_si128((__m128i *)(dst + offset), LerpConstFrac(src_a4, src_b4, frc4));

			offset += 4;
		}
	}

	// Handle the remaining 0-3 pixels.
	length = len & 3;
	if (length)
	{
		LoadRemainder(src_a4, src_b4, src_a + offset, src_b + offset, length);
		StoreRemainder(LerpConstFrac(src_a4, src_b4, frc4), dst + offset, length);
	}
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
