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
#include "modules/libvega/src/x86/vegacompositeover_x86.h"
#include "modules/libvega/src/x86/vegaremainder_x86.h"

extern "C" void CompOver_SSSE3(UINT32* dst, const UINT32* src, unsigned len)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	__m128i src4, dst4;
	if (len > 3)
	{
		// Use slow memory accesses and convert pixel per pixel until it's aligned.
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder(src4, dst4, src, dst, unaligned);
			StoreRemainder(CompOver4(src4, dst4), dst, unaligned);

			src += unaligned;
			dst += unaligned;
			len -= unaligned;
		}

		// We'll work in 128 bits, or 4 pixels at a time.
		unsigned int length = len / 4;

		if (!IsPtrAligned(src, 16))
		{
			while (length-- > 0)
			{
				src4 = _mm_loadu_si128((const __m128i *)src);
				dst4 = _mm_load_si128((const __m128i *)dst);

				_mm_store_si128((__m128i *)dst, CompOver4(src4, dst4));

				src += 4;
				dst += 4;
			}
		}
		else
		{
			while (length-- > 0)
			{
				src4 = _mm_load_si128((const __m128i *)src);
				dst4 = _mm_load_si128((const __m128i *)dst);

				_mm_store_si128((__m128i *)dst, CompOver4(src4, dst4));

				src += 4;
				dst += 4;
			}
		}
	}

	// Take care of the remaining 0-3 pixels that wasn't
	// handled in the loop.
	unsigned int length = len & 3;
	if (length)
	{
		// Perform the operation on 1-3 real pixels and
		// 3 to 1 garbage pixels. This is loss in performance
		// if it was 1 pixel, but a win if it was 2 or 3.
		LoadRemainder(src4, dst4, src, dst, length);
		StoreRemainder(CompOver4(src4, dst4), dst, length);
	}
}

extern "C" void CompConstOver_SSSE3(UINT32* dst, UINT32 src, unsigned len)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	// Pre-calculate (256 - source alpha) in word components.
	__m128i src4 = _mm_cvtsi32_si128(src);
	__m128i om_src_alpha = _mm_sub_epi16(g_xmm_constants._256, _mm_shuffle_epi8(src4, g_xmm_constants.broadcast_alpha));

	// Duplicate the source color into all four pixels.
	src4 = _mm_shuffle_epi32(src4, _MM_SHUFFLE(0, 0, 0, 0));

	// Align the pointer.
	__m128i dst4;
	if (len > 3)
	{
		// Use slow memory accesses and convert pixel per pixel until it's aligned.
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder(dst4, dst, unaligned);
			StoreRemainder(CompConstOver4(src4, dst4, om_src_alpha), dst, unaligned);

			dst += unaligned;
			len -= unaligned;
		}

		// 4 pixels at a time.
		unsigned int length = len / 4;
		while (length-- > 0)
		{
			dst4 = _mm_load_si128((const __m128i *)dst);
			_mm_store_si128((__m128i *)dst, CompConstOver4(src4, dst4, om_src_alpha));

			dst += 4;
		}
	}

	// Take care of the remaining 0-3 pixels that wasn't
	// handled in the loop.
	unsigned int length = len & 3;
	if (length)
	{
		// Perform the operation on 1-3 real pixels and
		// 3 to 1 garbage pixels. This is loss in performance
		// if it was 1 pixel, but a win if it was 2 or 3.
		LoadRemainder(dst4, dst, length);
		StoreRemainder(CompConstOver4(src4, dst4, om_src_alpha), dst, length);
	}
}

extern "C" void CompConstOverMask_SSSE3(UINT32* dst, UINT32 src, const UINT8* mask, unsigned len)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	// Pre-calculate (256 - source alpha) in word components.
	// Expand the source color into two pixels with word components.
	__m128i src2 = _mm_shuffle_epi8(_mm_cvtsi32_si128(src), g_xmm_constants.broadcast_src);

	// If the pointer isn't dword aligned it's impossible to get it quad dword aligned
	// instead we'll have to use unaligned memory accesses for all the pixels.
	__m128i dst4;
	if (len > 3)
	{
		// Use slow memory accesses and convert pixel per pixel until it's aligned.
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			UINT32 m = 0;
			LoadRemainder(dst4, m, dst, mask, unaligned);
			StoreRemainder(CompOver4(CompMaskSource4(_mm_cvtsi32_si128(m), src2), dst4), dst, unaligned);

			mask += unaligned;
			dst += unaligned;
			len -= unaligned;
		}

		// 4 pixels at a time, aligned memory access.
		unsigned int length = len / 4;
		while (length-- > 0)
		{
			// Read up four mask values.
			// Multiply the source color with the mask values.
			dst4 = _mm_load_si128((const __m128i *)dst);
			_mm_store_si128((__m128i *)dst, CompOver4(CompMaskSource4(_mm_cvtsi32_si128(*((UINT32 *)mask)), src2), dst4));

			mask += 4;
			dst += 4;
		}
	}

	// Take care of the remaining 0-3 pixels that wasn't
	// handled in the loop.
	unsigned int length = len & 3;
	if (length)
	{
		// Perform the operation on 1-3 real pixels and
		// 3 to 1 garbage pixels. This is loss in performance
		// if it was 1 pixel, but a win if it was 2 or 3.
		UINT32 m = 0;
		LoadRemainder(dst4, m, dst, mask, length);
		StoreRemainder(CompOver4(CompMaskSource4(_mm_cvtsi32_si128(m), src2), dst4), dst, length);
	}
}

extern "C" void CompOverConstMask_SSSE3(UINT32* dst, const UINT32* src, UINT32 mask, unsigned len)
{
	OP_ASSERT(IsPtrAligned(dst, 4));

	// Unpack mask + 1 into word components.
	__m128i mask4 = _mm_shuffle_epi8(_mm_cvtsi32_si128(mask + 1), g_xmm_constants.broadcast_word);

	__m128i src4, dst4;
	if (len > 3)
	{
		// Use slow memory accesses and convert pixel per pixel until it's aligned.
		unsigned int unaligned = PixelsUntilAligned(dst);
		if (unaligned)
		{
			LoadRemainder(src4, dst4, src, dst, unaligned);
			StoreRemainder(CompOverConstMask4(src4, dst4, mask4), dst, unaligned);

			src += unaligned;
			dst += unaligned;
			len -= unaligned;
		}

		// We'll work in 128 bits, or 4 pixels at a time.
		unsigned int length = len / 4;

		if (!IsPtrAligned(src, 16))
		{
			while (length-- > 0)
			{
				src4 = _mm_loadu_si128((const __m128i *)src);
				dst4 = _mm_load_si128((const __m128i *)dst);

				_mm_store_si128((__m128i *)dst, CompOverConstMask4(src4, dst4, mask4));

				src += 4;
				dst += 4;
			}
		}
		else
		{
			while (length-- > 0)
			{
				src4 = _mm_load_si128((const __m128i *)src);
				dst4 = _mm_load_si128((const __m128i *)dst);

				_mm_store_si128((__m128i *)dst, CompOverConstMask4(src4, dst4, mask4));

				src += 4;
				dst += 4;
			}
		}
	}

	// Take care of the remaining 0-3 pixels that wasn't
	// handled in the loop.
	unsigned int length = len & 3;
	if (length)
	{
		// Perform the operation on 1-3 real pixels and
		// 3 to 1 garbage pixels. This is loss in performance
		// if it was 1 pixel, but a win if it was 2 or 3.
		LoadRemainder(src4, dst4, src, dst, length);
		StoreRemainder(CompOverConstMask4(src4, dst4, mask4), dst, length);
	}
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
