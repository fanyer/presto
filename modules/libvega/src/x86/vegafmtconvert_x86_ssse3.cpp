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

// Convert 4 pixels with unaligned memory accesses.
static op_force_inline void Convert4u(UINT32* dst, const UINT32* src, const __m128i& pattern)
{
	__m128i src4 = _mm_loadu_si128((const __m128i *)src);
	_mm_storeu_si128((__m128i*)dst, _mm_shuffle_epi8(src4, pattern));
}

// Convert 4 pixels with aligned memory accesses.
static op_force_inline void Convert4a(UINT32* dst, const UINT32* src, const __m128i& pattern)
{
	__m128i src4 = _mm_load_si128((const __m128i *)src);
	_mm_store_si128((__m128i *)dst, _mm_shuffle_epi8(src4, pattern));
}

// Convert 4 pixels with aligned memory access, in-place.
static op_force_inline void Convert4a(UINT32* dst, const __m128i& pattern)
{
	__m128i src4 = _mm_load_si128((const __m128i *)dst);
	_mm_store_si128((__m128i *)dst, _mm_shuffle_epi8(src4, pattern));
}

// Convert a continuous array of pixels by transforming them with a shuffle pattern.
// The pattern is a 16 byte register holding each source bytes new byte position in it.
static inline void Convert(UINT32* dst, const UINT32* src, unsigned num, const __m128i& pattern)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	unsigned int offset = 0, length;

	// Check if it's an inplace conversion.
	if ((dst == src) && (num > 3))
	{
		// Process one pixel at a time until we reach an aligned address.
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			__m128i s;
			LoadRemainder(s, src, unaligned);
			StoreRemainder(_mm_shuffle_epi8(s, pattern), dst, unaligned);

			offset += unaligned;
			num -= unaligned;
		}

		// Unrolled loop handling 32 pixels at a time.
		length = num / 32;
		while (length--)
		{
			Convert4a(dst + offset + 0 * 4, pattern);
			Convert4a(dst + offset + 1 * 4, pattern);
			Convert4a(dst + offset + 2 * 4, pattern);
			Convert4a(dst + offset + 3 * 4, pattern);
			Convert4a(dst + offset + 4 * 4, pattern);
			Convert4a(dst + offset + 5 * 4, pattern);
			Convert4a(dst + offset + 6 * 4, pattern);
			Convert4a(dst + offset + 7 * 4, pattern);

			offset += 8 * 4;
		}

		// Handle 4 pixels at a time.
		num &= 31;
		length = num / 4;
		while (length--)
		{
			Convert4a(dst + offset, pattern);

			offset += 4;
		}
	}
	else
	{
		// Check if we have to use slow memory accesses.
		if (!IsPtrAligned(dst, 16) || !IsPtrAligned(src, 16))
		{
			// Unrolled loop handling 32 pixels at a time.
			length = num / 32;
			while (length--)
			{
				Convert4u(dst + offset + 0 * 4, src + offset + 0 * 4, pattern);
				Convert4u(dst + offset + 1 * 4, src + offset + 1 * 4, pattern);
				Convert4u(dst + offset + 2 * 4, src + offset + 2 * 4, pattern);
				Convert4u(dst + offset + 3 * 4, src + offset + 3 * 4, pattern);
				Convert4u(dst + offset + 4 * 4, src + offset + 4 * 4, pattern);
				Convert4u(dst + offset + 5 * 4, src + offset + 5 * 4, pattern);
				Convert4u(dst + offset + 6 * 4, src + offset + 6 * 4, pattern);
				Convert4u(dst + offset + 7 * 4, src + offset + 7 * 4, pattern);

				offset += 8 * 4;
			}

			// Handle 4 pixels at a time.
			num &= 31;
			length = num / 4;
			while (length--)
			{
				Convert4u(dst + offset, src + offset, pattern);

				offset += 4;
			}
		}
		else
		{
			// Unrolled loop handling 32 pixels at a time.
			length = num / 32;
			while (length--)
			{
				Convert4a(dst + offset + 0 * 4, src + offset + 0 * 4, pattern);
				Convert4a(dst + offset + 1 * 4, src + offset + 1 * 4, pattern);
				Convert4a(dst + offset + 2 * 4, src + offset + 2 * 4, pattern);
				Convert4a(dst + offset + 3 * 4, src + offset + 3 * 4, pattern);
				Convert4a(dst + offset + 4 * 4, src + offset + 4 * 4, pattern);
				Convert4a(dst + offset + 5 * 4, src + offset + 5 * 4, pattern);
				Convert4a(dst + offset + 6 * 4, src + offset + 6 * 4, pattern);
				Convert4a(dst + offset + 7 * 4, src + offset + 7 * 4, pattern);

				offset += 8 * 4;
			}

			// Handle 4 pixels at a time.
			num &= 31;
			length = num / 4;
			while (length--)
			{
				Convert4a(dst + offset, src + offset, pattern);

				offset += 4;
			}
		}
	}

	// Take care of the last 0 to 3 pixels.
	length = num & 3;
	if (length)
	{
		__m128i s;
		LoadRemainder(s, src + offset, length);
		__m128i result = _mm_shuffle_epi8(s, pattern);
		StoreRemainder(result, dst + offset, length);
	}
}

extern "C" void Convert_BGRA8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num)
{
	Convert(dst, static_cast<const UINT32*>(src), num, g_xmm_constants.bgra_pattern);
}

extern "C" void Convert_RGBA8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num)
{
	Convert(dst, static_cast<const UINT32*>(src), num, g_xmm_constants.rgba_pattern);
}

extern "C" void Convert_ABGR8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num)
{
	Convert(dst, static_cast<const UINT32*>(src), num, g_xmm_constants.abgr_pattern);
}

extern "C" void Convert_ARGB8888_to_BGRA8888_SSSE3(UINT32* dst, const void* src, unsigned num)
{
	Convert(dst, static_cast<const UINT32*>(src), num, g_xmm_constants.argb_pattern);
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
