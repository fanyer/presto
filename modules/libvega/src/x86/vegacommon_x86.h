/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGACOMMON_X86_H
#define VEGACOMMON_X86_H

#include <emmintrin.h>

struct VEGAXmmConstants
{
	void Init();

	// Word constants expanded eight times to fill the register.
	__m128i _1;
	__m128i _256;

	// Duplicate the first word or dword across the whole register.
	__m128i broadcast_word;

	// Broadcast a byte and expand to word components.
	__m128i broadcast_b2w_lo;
	__m128i broadcast_b2w_hi;

	// Expand the src alpha as word components into two quadwords.
	__m128i broadcast_alpha;

	// Broadcast the src alpha for two pixels.
	__m128i broadcast_word_alpha;

	// Expand the first dword as word components into two quadwords.
	__m128i broadcast_src;

	// Expand the alpha/mask component as word component into two pixels.
	__m128i broadcast_mask_lo;
	__m128i broadcast_mask_hi;

	// Permutation pattern to convert RGBA colors in different component
	// configurations to native format (BGRA).
	__m128i bgra_pattern;
	__m128i rgba_pattern;
	__m128i abgr_pattern;
	__m128i argb_pattern;
};

extern VEGAXmmConstants g_xmm_constants;

// Check if a pointer is suitable for fast memory access.
//
// There are generally both unaligned and aligned memory loads and stores
// available in the instruction set. Trying to use an aligned access with
// a misaligned pointer will in most cases cause a segmentation fault.
// However, on most architectures using an aligned memory operation results
// in improved performance. The improvement is less dramatic with newer
// architectures like Core i7, but still noticable if bandwidth constrained.
static op_force_inline bool IsPtrAligned(const void* ptr, unsigned int align)
{
	return !((intptr_t)ptr & (align - 1));
}

// Multiply the word components and then shift away the fraction.
static op_force_inline __m128i MulW(__m128i a, __m128i b)
{
	return _mm_srli_epi16(_mm_mullo_epi16(a, b), 8);
}

// Expand the two lower pixels into word component and subtract.
static op_force_inline __m128i SubW_Lo(__m128i a, __m128i b, __m128i zero)
{
	return _mm_sub_epi16(_mm_unpacklo_epi8(b, zero), _mm_unpacklo_epi8(a, zero));
}

// Expand the two higher pixels into word components and subtract.
static op_force_inline __m128i SubW_Hi(__m128i a, __m128i b, __m128i zero)
{
	return _mm_sub_epi16(_mm_unpackhi_epi8(b, zero), _mm_unpackhi_epi8(a, zero));
}

// Merge the two variables by first shifting 'a' and then or:ing the
// bits together. Note that the shift operation work on whole bytes, not bits.
template <int shift>
static op_force_inline __m128i Merge(__m128i dst, __m128i tmp)
{
	return _mm_or_si128(dst, _mm_slli_si128(tmp, shift));
}

#endif // VEGACOMMON_X86_H
