/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAREMAINDER_X86_H
#define VEGAREMAINDER_X86_H

// Get the number of pixels until the pointer is SSE aligned.
static op_force_inline unsigned int PixelsUntilAligned(const UINT32* ptr)
{
	// SSE memory access needs to be 16 byte aligned.
	unsigned int numPixels = ((intptr_t)ptr & 15);
	if (numPixels)
	{
		// Convert from byte count to pixel count.
		numPixels /= sizeof(UINT32);

		// We'll get aligned if we process (4 - remainder) pixels first.
		numPixels = sizeof(UINT32) - numPixels;
	}

	return numPixels;
}

// Sample the pixel in an image 'src' at the fixed point position 'csx', then
// increment the fixed point position with 'cdx'.
static op_force_inline UINT32 SampleX(const UINT32* src, INT32& csx, INT32 cdx)
{
	UINT32 result = src[VEGA_SAMPLE_INT(csx)];
	csx += cdx;
	return result;
}

// Sample the pixel in an image 'src' where the pixel width is 'stride', at the
// fixed point position 'csx, csy', then increment the fixed point positions
// with 'cdx, cdy'.
static op_force_inline UINT32 SampleXY(const UINT32* src, unsigned int stride,
	INT32& csx, INT32 cdx, INT32& csy, INT32 cdy)
{
	UINT32 result = src[VEGA_SAMPLE_INT(csy) * stride + VEGA_SAMPLE_INT(csx)];
	csx += cdx;
	csy += cdy;
	return result;
}

// Fill up the register 'd' with 1-3 pixels by reading from memory and
// "pushing" them into a register that can hold four pixels, one at a time.
static op_force_inline void LoadRemainder(__m128i& d, const UINT32* dst, unsigned int remainder)
{
	// Shift the resulting register that holds four pixels up one step.
	// Merge the newly read pixel with the resulting register in the
	// least significant dword.
	switch (remainder)
	{
	case 3:	d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 2:	d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 1:	d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	}
}

// Fill up the registers 's' and 'd' with 1-3 pixels by reading from memory and
// "pushing" them into a register that can hold four pixels, one at a time.
static op_force_inline void LoadRemainder(__m128i& s, __m128i& d,
	const UINT32* src, const UINT32* dst, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	}
}

static op_force_inline void LoadRemainder(__m128i& s, UINT32& m,
	const UINT32* src, const UINT8* mask, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		m <<= 8;
		m |= (UINT32)*mask++;
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		m <<= 8;
		m |= (UINT32)*mask++;
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(*src++), s);
		m <<= 8;
		m |= (UINT32)*mask++;
	}
}

static op_force_inline void LoadRemainder_SamplerX(__m128i& s, __m128i& d,
	const UINT32* src, const UINT32* dst, INT32& csx, INT32 cdx, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	}
}

static op_force_inline void LoadRemainder_SamplerX(__m128i& s, __m128i& d,
	UINT8* m, const UINT32* src, const UINT32* dst, const UINT8* alpha,
	INT32& csx, INT32 cdx, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[2] = *alpha++;
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[1] = *alpha++;
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(SampleX(src, csx, cdx)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[0] = *alpha++;
	}
}

static op_force_inline void LoadRemainder_SamplerXY(__m128i& s, __m128i& d,
	const UINT32* src, const UINT32* dst, unsigned int stride,
	INT32& csx, INT32 cdx, INT32& csy, INT32 cdy, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
	}
}

static op_force_inline void LoadRemainder_SamplerXY(__m128i& s, __m128i& d, UINT8* m,
	const UINT32* src, const UINT32* dst, const UINT8* alpha,
	unsigned int stride, INT32& csx, INT32 cdx, INT32& csy, INT32 cdy, unsigned int remainder)
{
	switch (remainder)
	{
	case 3:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[2] = *alpha++;
	case 2:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[1] = *alpha++;
	case 1:
		s = Merge<4>(_mm_cvtsi32_si128(SampleXY(src, stride, csx, cdx, csy, cdy)), s);
		d = Merge<4>(_mm_cvtsi32_si128(*dst++), d);
		m[0] = *alpha++;
	}
}

// "Pop" the pixels one at a time and write them out to the destination memory.
static op_force_inline void StoreRemainder(__m128i result, UINT32* dst, unsigned int remainder)
{
	// Store out the least significant dword from the result,
	// then shift the result down one pixel.
	switch (remainder)
	{
	case 3:
		dst[2] = _mm_cvtsi128_si32(result);
		result = _mm_srli_si128(result, 4);
	case 2:
		dst[1] = _mm_cvtsi128_si32(result);
		result = _mm_srli_si128(result, 4);
	case 1:
		dst[0] = _mm_cvtsi128_si32(result);
		result = _mm_srli_si128(result, 4);
	}
}

#endif // VEGAREMAINDER_X86_H
