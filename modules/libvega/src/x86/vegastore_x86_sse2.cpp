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
#include "modules/libvega/src/x86/vegaremainder_x86.h"

extern "C" void Store_SSE2(UINT32* dst, UINT32 rgba, unsigned len)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	// Guard against short runs where the setup cost will be too
	// high to benefit from the unrolling and 128 bit stores.
	// It's also set high enough to avoid an extra check when
	// aligning memory.
	if (len > 15)
	{
		// Aligned memory accesses can be significantly faster,
		// write single pixels util we get to an aligned memory
		// address.
		int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			len -= unaligned;
			switch (unaligned)
			{
			case 3: *dst++ = rgba;
			case 2: *dst++ = rgba;
			case 1: *dst++ = rgba;
			}
		}

		// Duplicate the source color four times in a 128 bit register.
		// This is needed for both the unrolled and normal
		// inner loop.
		__m128i src = _mm_cvtsi32_si128(rgba);
		src = _mm_shuffle_epi32(src, _MM_SHUFFLE(0, 0, 0, 0));

		// Unrolled inner loop that stores 32 pixels at a time.
		unsigned int length = len / 32;
		while (length--)
		{
			_mm_store_si128((__m128i *)(dst + 0 * 4), src);
			_mm_store_si128((__m128i *)(dst + 1 * 4), src);
			_mm_store_si128((__m128i *)(dst + 2 * 4), src);
			_mm_store_si128((__m128i *)(dst + 3 * 4), src);
			_mm_store_si128((__m128i *)(dst + 4 * 4), src);
			_mm_store_si128((__m128i *)(dst + 5 * 4), src);
			_mm_store_si128((__m128i *)(dst + 6 * 4), src);
			_mm_store_si128((__m128i *)(dst + 7 * 4), src);

			dst += 32;
		}

		// Handle the remaining pixels four at a time.
		len &= 31;
		length = len / 4;
		while (length--)
		{
			_mm_store_si128((__m128i *)dst, src);
			dst += 4;
		}

		// Leave the last 1-3 pixels for the regular loop.
		len &= 3;
	}

	// One pixel at a time.
	while (len--)
		*dst++ = rgba;
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
