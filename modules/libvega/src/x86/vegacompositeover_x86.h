/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGACOMPOSITEOVER_X86_H
#define VEGACOMPOSITEOVER_X86_H

#include <tmmintrin.h>

// Perform (dst.rgba * (256 - src.a) on two pixels (word components).
static op_force_inline __m128i CompOverW(__m128i src, __m128i dst)
{
	// Broadcast the alpha channel to all the words.
	__m128i alpha = _mm_shuffle_epi8(src, g_xmm_constants.broadcast_word_alpha);

	// Subtract each component from 256.
	__m128i res = _mm_subs_epu16(g_xmm_constants._256, alpha);

	// Multiply [dst0][dst1] with one minus src alpha.
	// Then shift down the result to bytes again.
	return MulW(dst, res);
}

// Perform (src.rgba + (dst.rgba * (256 - src.a)) on four pixels.
static op_force_inline __m128i CompOver4(__m128i src, __m128i dst)
{
	// Unpack the destination into two registers with word components.
	// This expansion is necessary for two reasons:
	//	- Multiplication needs to be in 16 bit to not lose precision
	//	  and to match the available instruction set.
	//	- We're subtracting from 256 which doesn't fit in 8 bits.
	const __m128i zero = _mm_setzero_si128();
	__m128i res01 = CompOverW(_mm_unpacklo_epi8(src, zero), _mm_unpacklo_epi8(dst, zero));
	__m128i res23 = CompOverW(_mm_unpackhi_epi8(src, zero), _mm_unpackhi_epi8(dst, zero));

	// Pack the destination pixels back into a 128 bit register again
	// and add with the source pixels.
	return _mm_adds_epu8(_mm_packus_epi16(res01, res23), src);
}

// Perform ((mask * src.rgba) + (dst.rgba * (256 - (mask * src.a))) on four pixels.
static op_force_inline __m128i CompOverConstMask4(__m128i src, __m128i dst, __m128i mask)
{
	// Unpack the destination into two registers with word components.
	// This expansion is necessary for two reasons:
	//	- Multiplication needs to be in 16 bit to not lose precision
	//	  and to match the available instruction set.
	//	- We're subtracting from 256 which doesn't fit in 8 bits.
	const __m128i zero = _mm_setzero_si128();

	// Unpack the two lower source pixels into word components and
	// multiply with the mask.
	__m128i src01 = MulW(_mm_unpacklo_epi8(src, zero), mask);
	__m128i res01 = CompOverW(src01, _mm_unpacklo_epi8(dst, zero));

	// Unpack the two higher source pixels into word components and
	// multiply with the mask.
	__m128i src23 = MulW(_mm_unpackhi_epi8(src, zero), mask);
	__m128i res23 = CompOverW(src23, _mm_unpackhi_epi8(dst, zero));

	// Pack the destination pixels back into a 128 bit register again.
	// Add with the source pixels.
	return _mm_adds_epu8(_mm_packus_epi16(res01, res23), _mm_packus_epi16(src01, src23));
}

// Perform ((mask * src.rgba) + (dst.rgba * (256 - (mask * src.a))) on four pixels.
static op_force_inline __m128i CompOverMask4(__m128i src, __m128i dst, const UINT8* mask)
{
	// Convert the four byte mask values to word components spread over
	// two registers and add 1.
	__m128i mask4 = _mm_cvtsi32_si128(*(UINT32 *)mask);
	__m128i mask01 = _mm_add_epi16(_mm_shuffle_epi8(mask4, g_xmm_constants.broadcast_mask_lo), g_xmm_constants._1);
	__m128i mask23 = _mm_add_epi16(_mm_shuffle_epi8(mask4, g_xmm_constants.broadcast_mask_hi), g_xmm_constants._1);

	// Unpack the destination into two registers with word components.
	// This expansion is necessary for two reasons:
	//	- Multiplication needs to be in 16 bit to not loose precision
	//	  and to match the available instruction set.
	//	- We're subtracting from 256 which doesn't fit in 8 bits.
	const __m128i zero = _mm_setzero_si128();

	// Unpack the two lower source pixels into word components.
	// Multiply with the mask.
	__m128i src01 = MulW(_mm_unpacklo_epi8(src, zero), mask01);
	__m128i res01 = CompOverW(src01, _mm_unpacklo_epi8(dst, zero));

	// Unpack the two higher source pixels into word components.
	// Multiply with the mask.
	__m128i src23 = MulW(_mm_unpackhi_epi8(src, zero), mask23);
	__m128i res23 = CompOverW(src23, _mm_unpackhi_epi8(dst, zero));

	// Pack the destination pixels back into a 128 bit register again.
	// Add with the source pixels.
	return _mm_adds_epu8(_mm_packus_epi16(res01, res23), _mm_packus_epi16(src01, src23));
}

// Perform (src.rgba + (dst.rgba * (256 - src.a)) on four pixels, with
// pre-calculated (1 - src.a) already duplicated into each word.
static op_force_inline __m128i CompConstOver4(__m128i src, __m128i dst, __m128i om_src_alpha)
{
	// Unpack the destination into two registers with word components.
	// dst * (1 - src.a).
	const __m128i zero = _mm_setzero_si128();
	__m128i result = _mm_packus_epi16(MulW(_mm_unpacklo_epi8(dst, zero), om_src_alpha),
									  MulW(_mm_unpackhi_epi8(dst, zero), om_src_alpha));

	// Add with the source pixels.
	return _mm_adds_epu8(result, src);
}

// Modulate the source pixel with a mask.
static op_force_inline __m128i CompMaskSource4(__m128i mask, __m128i src)
{
	// Split the mask up into word components over two registers.
	// Add 1 to each component.
	__m128i m01 = _mm_add_epi16(_mm_shuffle_epi8(mask, g_xmm_constants.broadcast_mask_lo), g_xmm_constants._1);
	__m128i m23 = _mm_add_epi16(_mm_shuffle_epi8(mask, g_xmm_constants.broadcast_mask_hi), g_xmm_constants._1);

	// Multiply the source pixel and the four mask values.
	return _mm_packus_epi16(MulW(src, m01), MulW(src, m23));
}

#endif // VEGACOMPOSITEOVER_X86_H
