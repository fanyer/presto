/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGACOMPOSITEOVER_NEON_H
#define VEGACOMPOSITEOVER_NEON_H

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM)

// Initialize constants used by Composite Over
//
// Output:
//  q15 - Composite Over constant
.macro InitCompOver
	// Prepare a register with 256's.
	vmov.u16		q15, #256			// q15 = [256]..[256]
.endm


// Initialize constants used by Mask
//
// Parameters:
//  mask - 8 bit mask word (0-255)
//  tmp - tmp register
//
// Output:
//  q14 - mask (in u16 format)
//
// Side effects:
//  tmp
.macro InitMask mask tmp
	// Allocate 8 bytes on the stack
	sub				sp, sp, #8

	// Prepere two registers containing as [MMmm] where mm = MM == 16 bit mask + 1 part
	add				\mask, \mask, #1
	orr				\mask, \mask, \mask, lsl #16
	mov				\tmp, \mask

	// Store the mask on the allocated stack memory
	stm				sp, {\mask, \tmp}

	// Load the mask into NEON registers
	vld1.u16		d28, [sp]
	vmov			d29, d28			// q14(d28-d29) = mask (in u16 format)

	// Free stack memory
	add				sp, sp, #8
.endm


// CompositeOver on 8 consecutive pixels.
//
// dst.rgba = src.rgba + (256 - src.a) * dst.rgba
//
// Input:
//	d0 - src red channel
//	d1 - src green channel
//	d2 - src blue channel
//	d3 - src alpha channel
//
//	d4 - dst red channel
//	d5 - dst green channel
//	d6 - dst blue channel
//	d7 - dst alpha channel
//
//	q15 - [256]..[256] in u16
//
// Output:
//	{d4 - d7} - will hold the result.
//
// Side effects:
//  {q8 - q12} ({d16 - d25})
.macro CompOver8
	// Subtract and widen.
	vsubw.u8		q8, q15, d3			// q8(d16-d17) = [256-src0.a]..[256-src7.a]

	// This is an unrolled variant of the above, optimized for Cortex A8 scheduling
	vmovl.u8		q9, d4				// q9(d18-d19) = [dst0.r]..[dst7.r]
	vmul.u16		q9, q8, q9			// q9(d18-d19) = [(256-src0.a)*dst0.r]..[(256-src7.a)*dst7.r]

	vmovl.u8		q10, d5				// q10(d20-d21) = [dst0.g]..[dst7.g]
	vmul.u16		q10, q8, q10		// q10(d20-d21) = [(256-src0.a)*dst0.g]..[(256-src7.a)*dst7.g]

	vmovl.u8		q11, d6				// q11(d22-d23) = [dst0.b]..[dst7.b]
	vmul.u16		q11, q8, q11		// q11(d22-d23) = [(256-src0.a)*dst0.b]..[(256-src7.a)*dst7.b]

	vmovl.u8		q12, d7				// q12(d24-d25) = [dst0.a]..[dst7.a]
	vmul.u16		q12, q8, q12		// q12(d24-d25) = [(256-src0.a)*dst0.a]..[(256-src7.a)*dst7.a]

	vshrn.u16		d4, q9, #8			// Shift down and narrow
	vshrn.u16		d5, q10, #8			// Shift down and narrow
	vshrn.u16		d6, q11, #8			// Shift down and narrow
	vadd.u8			d4, d0, d4			// d4 = [src0.r+(256-src0.a)*dst0.r]..[src7.r+(256-src7.a)*dst7.r]
	vadd.u8			d5, d1, d5			// d5 = [src0.g+(256-src0.a)*dst0.g]..[src7.g+(256-src7.a)*dst7.g]
	vshrn.u16		d7, q12, #8			// Shift down and narrow
	vadd.u8			d6, d2, d6			// d6 = [src0.b+(256-src0.a)*dst0.b]..[src7.b+(256-src7.a)*dst7.b]
	vadd.u8			d7, d3, d7			// d7 = [src0.a+(256-src0.a)*dst0.a]..[src7.a+(256-src7.a)*dst7.a]
.endm


// Mask 8 consecutive pixels.
//
// src.rgba = mask * src.rgba
//
// Input:
//	d0 - src red channel
//	d1 - src green channel
//	d2 - src blue channel
//	d3 - src alpha channel
//
//  q14 - mask (8 x u16)
//
// Output:
//	{d0 - d3} - will hold the result.
//
// Side effects:
//  {q8 - q11} ({d16 - d23})
.macro Mask8
	// Muliply source by mask
	vmovl.u8		q8, d0				// q8(d16-d17) = [src0.r]..[src7.r]
	vmul.u16		q8, q14, q8			// q8(d16-d17) = [mask*src0.r]..[mask*src7.r]
	vmovl.u8		q9, d1				// q9(d18-d19) = [src0.g]..[src7.g]
	vmul.u16		q9, q14, q9			// q9(d18-d19) = [mask*src0.g]..[mask*src7.g]
	vmovl.u8		q10, d2				// q10(d20-d21) = [src0.b]..[src7.b]
	vmul.u16		q10, q14, q10		// q10(d20-d21) = [mask*src0.b]..[mask*src7.b]
	vmovl.u8		q11, d3				// q11(d22-d23) = [src0.a]..[src7.a]
	vmul.u16		q11, q14, q11		// q11(d22-d23) = [mask*src0.a]..[mask*src7.a]

	// Shift 16-bit masked data to 8-bit precision
	vshrn.u16		d0, q8, #8			// d0 = [(mask*src0.r)>>8]..[(mask*src7.r)>>8]
	vshrn.u16		d1, q9, #8			// d1 = [(mask*src0.g)>>8]..[(mask*src7.g)>>8]
	vshrn.u16		d2, q10, #8			// d2 = [(mask*src0.b)>>8]..[(mask*src7.b)>>8]
	vshrn.u16		d3, q11, #8			// d3 = [(mask*src0.a)>>8]..[(mask*src7.a)>>8]
.endm


// Load from memory and store in NEON registers.
//
// 8 8 bit words will be read from [src1] and stored in {d0 - d3}
// 8 8 bit words will be read from [src2] and stored in {d4 - d7}
//
// Parameters:
//  src1 - memory pointer
//  src2 - memory pointer
//
// Output:
//  {d0 - d3} - data read from src1
//  {d4 - d7} - data read from src2
.macro LoadNeon src1 src2
	// Load up sampled source
	// d0 = [src.r0-src.r7]
	// d1 = [src.g0-src.g7]
	// d2 = [src.b0-src.b7]
	// d3 = [src.a0-src.a7]
	vld4.u8			{d0 - d3}, [\src1]

	// Load up destination
	// d4 = [dst.r0-dst.r7]
	// d5 = [dst.g0-dst.g7]
	// d6 = [dst.b0-dst.b7]
	// d7 = [dst.a0-dst.a7]
	vld4.u8			{d4 - d7}, [\src2]
.endm


// Store content of NEON registers {d4 - d7} into [dst].
//
// Parameters:
//  dst - memory pointer
//
// Output:
//  [dst] - 
.macro StoreNeon dst
	// Store the result to destination interleaved, then increment
	// the pointer
	vst4.u8			{d4 - d7}, [\dst]!
.endm


// StoreRemNeon - Store remainding pixel data
//
// Parameters:
//  dst - dst pointer
//  count - 0-counted remainder count (value between 0 and 6)
//
// Input:
//  r0 - dst pointer
//
// Output:
//  [dst] - 8 pixels will be written to [dst] interleaved.
//
// Side effects:
//  tmp
.macro StoreRemNeon dst count tmp
	adr				\tmp, 8f
	ldr				pc, [\tmp, \count, lsl #2]
8:
	.word 1f
	.word 2f
	.word 3f
	.word 4f
	.word 5f
	.word 6f
	.word 7f
7:
	add				\tmp, \dst, #24		// \tmp = dst + 24
	vst4.u8			{d4[6], d5[6], d6[6], d7[6]}, [\tmp]
6:
	add				\tmp, \dst, #20
	vst4.u8			{d4[5], d5[5], d6[5], d7[5]}, [\tmp]
5:
	add				\tmp, \dst, #16
	vst4.u8			{d4[4], d5[4], d6[4], d7[4]}, [\tmp]
4:
	add				\tmp, \dst, #12
	vst4.u8			{d4[3], d5[3], d6[3], d7[3]}, [\tmp]
3:
	add				\tmp, \dst, #8
	vst4.u8			{d4[2], d5[2], d6[2], d7[2]}, [\tmp]
2:
	add				\tmp, \dst, #4
	vst4.u8			{d4[1], d5[1], d6[1], d7[1]}, [\tmp]
1:
	vst4.u8			{d4[0], d5[0], d6[0], d7[0]}, [\dst]
.endm


// Load `count + 1' pixels from two sources. `count' must be between 0 and 6.
//
// Parameters
//  src1 - memory pointer
//  src2 - memory pointer
//  count - "number of pixels to read" - 1
//  tmp - temporary register to use
//
// Labels:
//	$0 - table label
//	$1 - case label
//
// Output:
//	{d0 - d3} - will be loaded with src1 pixels de-interleaved.
//	{d4 - d7} - will be loaded with src2 pixels de-interleaved.
//	q14 - will be loaded with mask (+1, u16).
//
// Side effects:
//   tmp
.macro LoadRemSrcs src1 src2 count tmp
	adr				\tmp, 8f
	ldr				pc, [\tmp, \count, lsl #2]
8:
	.word 1f
	.word 2f
	.word 3f
	.word 4f
	.word 5f
	.word 6f
	.word 7f
7:
	add				\tmp, \src1, #24							// \tmp = src + 24
	vld4.u8			{d0[6], d1[6], d2[6], d3[6]}, [\tmp]
	add				\tmp, \src2, #24							// \tmp = dst + 24
	vld4.u8			{d4[6], d5[6], d6[6], d7[6]}, [\tmp]
6:
	add				\tmp, \src1, #20
	vld4.u8			{d0[5], d1[5], d2[5], d3[5]}, [\tmp]
	add				\tmp, \src2, #20
	vld4.u8			{d4[5], d5[5], d6[5], d7[5]}, [\tmp]
5:
	add				\tmp, \src1, #16
	vld4.u8			{d0[4], d1[4], d2[4], d3[4]}, [\tmp]
	add				\tmp, \src2, #16
	vld4.u8			{d4[4], d5[4], d6[4], d7[4]}, [\tmp]
4:
	add				\tmp, \src1, #12
	vld4.u8			{d0[3], d1[3], d2[3], d3[3]}, [\tmp]
	add				\tmp, \src2, #12
	vld4.u8			{d4[3], d5[3], d6[3], d7[3]}, [\tmp]
3:
	add				\tmp, \src1, #8
	vld4.u8			{d0[2], d1[2], d2[2], d3[2]}, [\tmp]
	add				\tmp, \src2, #8
	vld4.u8			{d4[2], d5[2], d6[2], d7[2]}, [\tmp]
2:
	add				\tmp, \src1, #4
	vld4.u8			{d0[1], d1[1], d2[1], d3[1]}, [\tmp]
	add				\tmp, \src2, #4
	vld4.u8			{d4[1], d5[1], d6[1], d7[1]}, [\tmp]
1:
	vld4.u8			{d0[0], d1[0], d2[0], d3[0]}, [\src1]
	vld4.u8			{d4[0], d5[0], d6[0], d7[0]}, [\src2]
.endm

#endif // ARCHITECTURE_ARM

#endif // VEGACOMPOSITEOVER_NEON_H
