/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGASAMPLER_ARM_H
#define VEGASAMPLER_ARM_H

// Load1DSample
//
// Parameters:
//  out - destination register
//  src - pointer to source data
//  csx
//  cdx
//
// Output:
//  out - read pixel
//
// Side effects:
//  csx - stepped by cdx
.macro Load1DSample out src csx cdx
	mov				\out, \csx, asr #12
	add				\csx, \csx, \cdx				// csx += cdx
	ldr				\out, [\src, \out, lsl #2]		// out = src[(csx >> 12) * 4]
.endm

// Load8x1DSamples
//
// Parameters:
//  dst - pointer to where to store read samples
//  src - pointer to source
//  csx
//  cdx
//
// Output:
//  {r5 - r12} - {src0 - src7}
//
// Side effects:
//  csx - stepped by cdx 8 times
.macro Load8x1DSamples src csx cdx
	//Unoptimized version:
	//Load1DSample	r5, \src, \csx, \cdx		// r5  = src0
	//Load1DSample	r6, \src, \csx, \cdx		// r6  = src1
	//Load1DSample	r7, \src, \csx, \cdx		// r7  = src2
	//Load1DSample	r8, \src, \csx, \cdx		// r8  = src3
	//Load1DSample	r9, \src, \csx, \cdx		// r9  = src4
	//Load1DSample	r10, \src, \csx, \cdx		// r10 = src5
	//Load1DSample	r11, \src, \csx, \cdx		// r11 = src6
	//Load1DSample	r12, \src, \csx, \cdx		// r12 = src7

	// 8x un-rolled version (optimized for minimum pipeline stalls)
	mov				r5, \csx, asr #12			// r5  = csx >> 12
	add				r6, \csx, \cdx				// r6  = csx + cdx
	ldr				r5, [\src, r5, lsl #2]		// r5 = src0
	add				r7, \csx, \cdx, lsl #1		// r7  = csx + 2 * cdx
	add				r8, r6, \cdx, lsl #1		// r8  = csx + 3 * cdx
	mov				r6, r6, asr #12				// r6  = (csx + cdx) >> 12
	add				r9, r7, \cdx, lsl #1		// r9  = csx + 4 * cdx
	ldr				r6, [\src, r6, lsl #2]		// r6 = src1
	mov				r7, r7, asr #12				// r7  = (csx + 2 * cdx) >> 12
	add				r10, r8, \cdx, lsl #1		// r10 = csx + 5 * cdx
	ldr				r7, [\src, r7, lsl #2]		// r7 = src2
	mov				r8, r8, asr #12				// r8  = (csx + 3 * cdx) >> 12
	add				r11, r9, \cdx, lsl #1		// r11 = csx + 6 * cdx
	ldr				r8, [\src, r8, lsl #2]		// r8 = src3
	mov				r9, r9, asr #12				// r9  = (csx + 4 * cdx) >> 12
	add				r12, r10, \cdx, lsl #1		// r11 = csx + 7 * cdx
	ldr				r9, [\src, r9, lsl #2]		// r9 = src4
	mov				r10, r10, asr #12			// r10 = (csx + 5 * cdx) >> 12
	mov				r11, r11, asr #12			// r11 = (csx + 6 * cdx) >> 12
	ldr				r10, [\src, r10, lsl #2]	// r10 = src5
	add				\csx, r12, \cdx				// csx = csx + 8 * cdx
	mov				r12, r12, asr #12			// r12 = (csx + 7 * cdx) >> 12
	ldr				r11, [\src, r11, lsl #2]	// r11 = src6
	ldr				r12, [\src, r12, lsl #2]	// r12 = src7
.endm

// Load2DSample
//
// Parameters:
//  out - destination register
//  src - source pointer
//  stride
//  csx
//  csy
//  cdx
//  cdy
//
// Output:
//  out - sampled pixel value
//
// Side effects:
//  csx - stepped by cdx
//  csy - stepped by cdy
//  tmp
.macro Load2DSample out src stride csx csy cdx cdy tmp
	mov				\out, \csy, asr #12			// out = csy >> 12
	mov				\tmp, \csx, asr #12			// r12 = csx >> 12
	mla				\out, \out, \stride, \tmp	// out = (csy >> 12) * stride + (csx >> 12)
	add				\csx, \csx, \cdx			// csx += cdx
	add				\csy, \csy, \cdy			// csy += cdy
	ldr				\out, [\src, \out, lsl #2]	// out = src[((csx >> 12) + (csy >> 12) * stride) * 4]
.endm

#endif // VEGASAMPLER_ARM_H
