/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM_NEON)

#include "vegacompositeover_neon.h"
#include "vegasampler_arm.h"

.arch armv7-a		// Support ARMv7 instructions
.fpu neon			// Support NEON instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text

// Copy 8 1D Samples from src to dst.
//
// Samples are read by stepping csx with cdx for every sample and written to
// an indexed array.
//
// Parameters:
//  dst - pointer to where to store read samples
//  src - pointer to source
//  csx
//  cdx
//
// Output:
//  [dst] - src0 - src7
//
// Side effects:
//  {r5 - r12}
//  csx - stepped by cdx 8 times
.macro Copy8x1DSamples dst src csx cdx
	Load8x1DSamples	\src, \csx, \cdx
	stm				\dst, {r5 - r12}			// [dst] = src0 - src7
.endm

// Copy 8 2D Samples from src to dst.
//
// Samples are read by stepping csx with cdx for every sample and written to
// an indexed array.
//
// Parameters:
//  dst - destination pointer
//  src - source pointer
//  stride
//  csx
//  csy
//  cdx
//  cdy
//
// Input:
//  r1 - src pointer
//
// Output:
//  [sp] - {src0 - src7}
//
// Side effects:
//	{r6 - r8}
.macro Copy8x2DSamples buf src stride csx csy cdx cdy
	Load2DSample	r6, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	Load2DSample	r7, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	stmia			\buf!, {r6 - r7}
	Load2DSample	r6, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	Load2DSample	r7, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	stmia			\buf!, {r6 - r7}
	Load2DSample	r6, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	Load2DSample	r7, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	stmia			\buf!, {r6 - r7}
	Load2DSample	r6, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	Load2DSample	r7, \src, \stride, \csx, \csy, \cdx, \cdy, r8
	stm				\buf, {r6 - r7}
	sub				\buf, \buf, #8*3
.endm

// LoadRem1DSamplesDst
//
// Parameters:
//  csx
//  cdx
//  dst
//  src
//
// Input:
//  r3 - 0-counted remainder count (value between 0 and 6)
//
// Output:
//  {d0 - d3} - sampled source data (src0..src7)
//  {d4 - d7} - destination data (dst0..dst7)
//
// Side effects:
//  {r11 - r12}
.macro LoadRem1DSamplesDst dst src csx cdx
	sub				sp, sp, #16*4

	ldr				r11, [\dst, #0*4]			// r11 = dst0
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src0
	str				r11, [sp, #8*4]
	cmp				r3, #0
	str				r12, [sp, #0*4]
	beq				1f

	ldr				r11, [\dst, #1*4]			// r11 = dst1
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src1
	str				r11, [sp, #9*4]
	cmp				r3, #1
	str				r12, [sp, #1*4]
	beq				1f

	ldr				r11, [\dst, #2*4]			// r11 = dst2
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src2
	str				r11, [sp, #10*4]
	cmp				r3, #2
	str				r12, [sp, #2*4]
	beq				1f

	ldr				r11, [\dst, #3*4]			// r11 = dst3
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src3
	str				r11, [sp, #11*4]
	cmp				r3, #3
	str				r12, [sp, #3*4]
	beq				1f

	ldr				r11, [\dst, #4*4]			// r11 = dst4
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src4
	str				r11, [sp, #12*4]
	cmp				r3, #4
	str				r12, [sp, #4*4]
	beq				1f

	ldr				r11, [\dst, #5*4]			// r11 = dst5
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src5
	str				r11, [sp, #13*4]
	cmp				r3, #5
	str				r12, [sp, #5*4]
	beq				1f

	ldr				r11, [\dst, #6*4]			// r11 = dst6
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src6
	str				r11, [sp, #14*4]
	str				r12, [sp, #6*4]

1:
	vld4.u8			{d0 - d3}, [sp]!			// Load src0-src7
	vld4.u8			{d4 - d7}, [sp]!			// Load dst0-dst7
.endm

// LoadRem2DSamplesDst
//
// Parameters:
//  dst
//  src
//  csx
//  csy
//  cdx
//  cdy
//
// Input:
//  r2 - remainder count
//
// Output:
//  {d0 - d3} - read sampled source pixels
//  {d4 - d7} - read destination pixels
//
// Side effects:
//  r8, r10 - r12
//  csx - stepped by cdx `count' times
//  csy - stepped by cdy `count' times
.macro LoadRem2DSamplesDst dst src stride csx csy cdx cdy
	sub				sp, sp, #16*4

	ldr				r11, [\dst, #0*4]								// r11 = dst0
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src0
	str				r11, [sp, #8*4]
	cmp				r2, #0
	str				r10, [sp, #0*4]
	beq				1f

	ldr				r11, [\dst, #1*4]								// r11 = dst1
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src1
	str				r11, [sp, #9*4]
	cmp				r2, #1
	str				r10, [sp, #1*4]
	beq				1f

	ldr				r11, [\dst, #2*4]								// r11 = dst2
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src2
	str				r11, [sp, #10*4]
	cmp				r2, #2
	str				r10, [sp, #2*4]
	beq				1f

	ldr				r11, [\dst, #3*4]								// r11 = dst3
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src3
	str				r11, [sp, #11*4]
	cmp				r2, #3
	str				r10, [sp, #3*4]
	beq				1f

	ldr				r11, [\dst, #4*4]								// r11 = dst4
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src4
	str				r11, [sp, #12*4]
	cmp				r2, #4
	str				r10, [sp, #4*4]
	beq				1f

	ldr				r11, [\dst, #5*4]								// r11 = dst5
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src5
	str				r11, [sp, #13*4]
	cmp				r2, #5
	str				r10, [sp, #5*4]
	beq				1f

	ldr				r11, [\dst, #6*4]								// r11 = dst6
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src6
	str				r11, [sp, #14*4]
	str				r10, [sp, #6*4]

1:
	vld4.u8			{d0 - d3}, [sp]!								// Load src0-src7
	vld4.u8			{d4 - d7}, [sp]!								// Load dst0-dst7
.endm

// LoadNearestMask1DDstRemainderARM
//
// Parameters:
//  dst
//  src
//  csx
//  cdx
//  count
//
// Output:
//  {d0 - d3} - read sampled source pixels
//  {d4 - d7} - read destination pixels
//  q14 - mask
//
// Side effects:
//  r11 - r12
//  csx - stepped by cdx `count' times
.macro LoadRem1DSamplesMaskDst dst src csx cdx count
	sub				sp, sp, #16*4

	ldr				r11, [\dst, #0*4]			// r11 = dst0
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src0
	vld1.u8			{d28[0]}, [r2]!				// d28[0] = mask0
	str				r11, [sp, #8*4]
	cmp				\count, #0
	str				r12, [sp, #0*4]
	beq				1f

	ldr				r11, [\dst, #1*4]			// r11 = dst1
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src1
	vld1.u8			{d28[1]}, [r2]!				// d28[1] = mask1
	str				r11, [sp, #9*4]
	cmp				\count, #1
	str				r12, [sp, #1*4]
	beq				1f

	ldr				r11, [\dst, #2*4]			// r11 = dst2
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src2
	vld1.u8			{d28[2]}, [r2]!				// d28[2] = mask2
	str				r11, [sp, #10*4]
	cmp				\count, #2
	str				r12, [sp, #2*4]
	beq				1f

	ldr				r11, [\dst, #3*4]			// r11 = dst3
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src3
	vld1.u8			{d28[3]}, [r2]!				// d28[3] = mask3
	str				r11, [sp, #11*4]
	cmp				\count, #3
	str				r12, [sp, #3*4]
	beq				1f

	ldr				r11, [\dst, #4*4]			// r11 = dst4
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src4
	vld1.u8			{d28[4]}, [r2]!				// d28[4] = mask4
	str				r11, [sp, #12*4]
	cmp				\count, #4
	str				r12, [sp, #4*4]
	beq				1f

	ldr				r11, [\dst, #5*4]			// r11 = dst5
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src5
	vld1.u8			{d28[5]}, [r2]!				// d28[5] = mask5
	str				r11, [sp, #13*4]
	cmp				\count, #5
	str				r12, [sp, #5*4]
	beq				1f

	ldr				r11, [\dst, #6*4]			// r11 = dst6
	Load1DSample	r12, \src, \csx, \cdx		// r12 = src6
	vld1.u8			{d28[6]}, [r2]!				// d28[6] = mask6
	str				r11, [sp, #14*4]
	str				r12, [sp, #6*4]

1:
	vld4.u8			{d0 - d3}, [sp]!			// Load src0..src7
	vmov.u8			d29, #1
	vld4.u8			{d4 - d7}, [sp]!			// Load dst0..dst7
	vaddl.u8		q14, d28, d29				// q14 = (mask0..mask7)+1 (u16)
.endm

// LoadNearestMaskDstRemainderARM
//
// Parameters:
//  dst - destination pointer
//  src - source pointer
//  mask - mask pointer
//  stride
//  csx
//  csy
//  cdx
//  cdy
//
// Output:
//  {d0 - d3} - read sampled source pixels
//  {d4 - d7} - read destination pixels
//  q14 - mask
//
// Side effects:
//  r8, r10 - r11
.macro LoadRem2DSamplesMaskDst dst src mask stride csx csy cdx cdy count
	sub				sp, sp, #16*4

	ldr				r11, [\dst, #0*4]								// r11 = dst0
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src0
	vld1.u8			{d28[0]}, [\mask]!								// d28[0] = mask0
	str				r11, [sp, #8*4]
	cmp				\count, #0
	str				r10, [sp, #0*4]
	beq				1f

	ldr				r11, [\dst, #1*4]								// r11 = dst1
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src1
	vld1.u8			{d28[1]}, [\mask]!								// d28[1] = mask1
	str				r11, [sp, #9*4]
	cmp				\count, #1
	str				r10, [sp, #1*4]
	beq				1f

	ldr				r11, [\dst, #2*4]								// r11 = dst2
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src2
	vld1.u8			{d28[2]}, [\mask]!								// d28[2] = mask2
	str				r11, [sp, #10*4]
	cmp				\count, #2
	str				r10, [sp, #2*4]
	beq				1f

	ldr				r11, [\dst, #3*4]								// r11 = dst3
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src3
	vld1.u8			{d28[3]}, [\mask]!								// d28[3] = mask3
	str				r11, [sp, #11*4]
	cmp				\count, #3
	str				r10, [sp, #3*4]
	beq				1f

	ldr				r11, [\dst, #4*4]								// r11 = dst4
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src4
	vld1.u8			{d28[4]}, [\mask]!								// d28[4] = mask4
	str				r11, [sp, #12*4]
	cmp				\count, #4
	str				r10, [sp, #4*4]
	beq				1f

	ldr				r11, [\dst, #5*4]								// r11 = dst5
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src5
	vld1.u8			{d28[5]}, [\mask]!								// d28[5] = mask5
	str				r11, [sp, #13*4]
	cmp				\count, #5
	str				r10, [sp, #5*4]
	beq				1f

	ldr				r11, [\dst, #6*4]								// r11 = dst6
	Load2DSample	r10, \src, \stride, \csx, \csy, \cdx, \cdy, r8	// r10 = src6
	vld1.u8			{d28[6]}, [\mask]!								// d28[6] = mask6
	str				r11, [sp, #14*4]
	str				r10, [sp, #6*4]

1:
	vld4.u8			{d0 - d3}, [sp]!								// Load src0..src7
	vmov.u8			d29, #1
	vld4.u8			{d4 - d7}, [sp]!								// Load dst0..dst7
	vaddl.u8		q14, d28, d29									// q14 = (mask0..mask7)+1 (u16)
.endm

// Mask 8 pixels
//
// Input:
//  dst - destination pixels pointer
//  src - source pixel pointer
//  mask - mask pointer
//
// Output:
//  {d0 - d3} - 8 masked source pixels
//  {d4 - d7} - 8 destination pixels
//
// Side effects:
//  {q4 - q5}
//  {q9 - q12}
//  q14
.macro LoadAndMask8 dst src mask
	// Load up sampled source, as u16
	// q4 = [src.r0-src.r7]
	// q5 = [src.g0-src.g7]
	// q6 = [src.b0-src.b7]
	// q7 = [src.a0-src.a7]
	vld4.u8			{d0 - d3}, [\src]
	vmovl.u8		q4, d0
	vmovl.u8		q5, d1

	// Load the mask into q14 (8x u16).
	// Then increment the pointer.
	// q14 = [mask0-mask7]
	vld1.u8			d28, [\mask]!			// d28 = [mask0-1]..[mask7-1] in u8
	vmov.u8			d29, #1
	vaddl.u8		q14, d28, d29			// q14 = [mask0]..[mask7] in u16

	vmovl.u8		q6, d2
	vmovl.u8		q7, d3

	// Multiply source by mask
	vmul.u16		q9, q14, q4				// q9(d18-d19) = [mask0*src.r]..[mask7*src.r]
	vmul.u16		q10, q14, q5			// q10(d20-d21) = [mask0*src.g]..[mask7*src.g]
	vmul.u16		q11, q14, q6			// q11(d22-d23) = [mask0*src.b]..[mask7*src.b]
	vmul.u16		q12, q14, q7			// q12(d24-d25) = [mask0*src.a]..[mask7*src.a]

	// Load up destination
	// d4 = [dst.r0-dst.r7]
	// d5 = [dst.g0-dst.g7]
	// d6 = [dst.b0-dst.b7]
	// d7 = [dst.a0-dst.a7]
	vld4.u8			{d4 - d7}, [\dst]

	// Shift 16-bit masked data to 8-bit precision
	vshrn.u16		d0, q9, #8				// d0 = [(mask0*src.r)>>8]..[(mask7*src.r)>>8]
	vshrn.u16		d1, q10, #8				// d1 = [(mask0*src.g)>>8]..[(mask7*src.g)>>8]
	vshrn.u16		d2, q11, #8				// d2 = [(mask0*src.b)>>8]..[(mask7*src.b)>>8]
	vshrn.u16		d3, q12, #8				// d3 = [(mask0*src.a)>>8]..[(mask7*src.a)>>8]
.endm

// Mask source
//
// Input:
//  {d0 - d3} - {src.r - src.a}
//  q14 - mask
//
// Output:
//  {d0 - d1} - masked source
//
// Side effects:
//  {q9 - q12}
.macro MaskSourceRem
	vmovl.u8		q4, d0					// q4(d8-d9)   = [src0.r]..[src7.r]
	vmovl.u8		q5, d1					// q5(d10-d11) = [src0.g]..[src7.g]
	vmovl.u8		q6, d2					// q6(d12-d13) = [src0.b]..[src7.b]
	vmovl.u8		q7, d3					// q7(d14-d15) = [src0.a]..[src7.a]
	vmul.u16		q9, q14, q4				// q9(d18-d19)  = [mask0*src.r]..[mask7*src.r]
	vmul.u16		q10, q14, q5			// q10(d20-d21) = [mask0*src.g]..[mask7*src.g]
	vmul.u16		q11, q14, q6			// q11(d22-d23) = [mask0*src.b]..[mask7*src.b]
	vmul.u16		q12, q14, q7			// q12(d24-d25) = [mask0*src.a]..[mask7*src.a]
	vshrn.u16		d0, q9, #8				// d0 = [(mask0*src.r)>>8]..[(mask7*src.r)>>8]
	vshrn.u16		d1, q10, #8				// d1 = [(mask0*src.g)>>8]..[(mask7*src.g)>>8]
	vshrn.u16		d2, q11, #8				// d2 = [(mask0*src.b)>>8]..[(mask7*src.b)>>8]
	vshrn.u16		d3, q12, #8				// d3 = [(mask0*src.a)>>8]..[(mask7*src.a)>>8]
.endm

// void Sampler_NearestX_CompOver_NEON(void* dst, const void* src, unsigned int cnt, INT32 csx, INT32 cdx);
//  r0 = dst
//  r1 = src
//  r2 = cnt
//  r3 = csx
//  [sp] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOver_NEON
	push			{r4 - r11, r14}

	ldr				r4, [sp, #36]			// r4 = cdx

	// Allocate some space on the stack for ARM->NEON data transfer
	sub				sp, sp, #8*4

	InitCompOver

	// Just to make the zero/one switches false later
	mov				r12, #0x80000000

	subs			r2, r2, #8
	bcc				snxco_tail

snxco_main_loop:
	// Should we try to switch to transparent/opaque operation?
	movs			r12, r12, lsr #24
	beq				snxco_switch_to_transparent
	cmp				r12, #0xff
	beq				snxco_switch_to_opaque

	// Preload destination data
	pld				[r0, #8*4]

	// Sample source (r5 - r12 = src0 - src7), increment source coordinate.
	Copy8x1DSamples	sp, r1, r3, r4

snxco_switch_to_blend:
	LoadNeon		sp, r0

	// Composite over
	CompOver8

	// Store pixels
	StoreNeon		r0

	subs			r2, r2, #8
	bcs				snxco_main_loop

	b				snxco_tail

snxco_switch_to_opaque:

snxco_opaque_loop:
	// Sample source (r5 - r12 = src0 - src7), increment source coordinate.
	Copy8x1DSamples	sp, r1, r3, r4

	// Make sure that we are still fully opaque
	// Note: We preserve r12 for the comparison in snxco_mainloop
	and				r14, r5, r6
	vmov			d0, r5, r6				// d0 = src0..src1
	and				r5, r7, r8
	vmov			d1, r7, r8				// d1 = src2..src3
	and				r6, r9, r10
	and				r7, r11, r12
	and				r14, r14, r5
	and				r5, r6, r7
	and				r14, r14, #0xff000000
	vmov			d2, r9, r10				// d2 = src4..src5
	and				r14, r14, r5
	vmov			d3, r11, r12			// d3 = src6..src7
	cmp				r14, #0xff000000
	bne				snxco_switch_to_blend	// If alpha != 0xff, switch to blending

	vst1.u8			{d0 - d3}, [r0]!		// Store src0..src7 to destionation

	subs			r2, r2, #8				// len -= 8
	bcs				snxco_opaque_loop

	b				snxco_tail


snxco_switch_to_transparent:

snxco_transparent_loop:
	// Sample source (r5 - r12 = src0 - src7), increment source coordinate.
	Copy8x1DSamples	sp, r1, r3, r4

	// Make sure that we are still fully transparent
	// Note 1: We only have to check if all pixles are exactly zero, since we
	// use premultiplied alpha (a == 0 -> r,g,b = 0,0,0)
	// Note 2: We preserve r12 for the comparison in snxco_mainloop
	orr				r5, r5, r6
	orr				r6, r7, r8
	orr				r7, r9, r10
	orr				r8, r11, r12
	orr				r5, r5, r6
	orr				r6, r7, r8
	orrs			r6, r5, r6
	bne				snxco_switch_to_blend	// If alpha != 0x00, switch to blending

	subs			r2, r2, #8				// len -= 8
	add				r0, r0, #8*4			// dst += 8
	bcs				snxco_transparent_loop


snxco_tail:
	adds			r2, r2, #8
	beq				snxco_done

	mov				r14, r3					// r14 = csx
	sub				r3, r2, #1				// Index in range [0,6]

	LoadRem1DSamplesDst r0, r1, r14, r4

	CompOver8

	StoreRemNeon	r0, r3, r12

snxco_done:
	add				sp, sp, #8*4
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestX_CompOverMask_NEON(void* dst, const void* src, const UINT8* alpha, unsigned int cnt, INT32 csx, INT32 cdx);
//  r0 = dst
//  r1 = src
//  r2 = alpha
//  r3 = cnt
//  [sp] = csx
//  [sp+4] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOverMask_NEON
	push			{r4 - r11, r14}

	ldr				r4, [sp, #40]			// r4 = cdx
	ldr				r14, [sp, #36]			// r14 = csx

	// Allocate some space on the stack for ARM->NEON data transfer
	sub				sp, sp, #8*4

	// Prepare a register with 256's.
	vmov.u16		q15, #256				// q15 = [256]..[256]

	subs			r3, r3, #8
	bcc				snxcom_tail

snxcom_main_loop:
	// Preload destination data
	pld				[r0, #8*4]

	// Sample source (r5 - r12 = src0 - src7), increment source coordinate.
	Copy8x1DSamples	sp, r1, r14, r4

	// Mask 8 source pixels and load 8 destination pixels into NEON registers
	LoadAndMask8	r0, sp, r2

	// Composite over
	CompOver8

	// Store the result to destination interleaved, then increment
	// the pointer
	vst4.u8			{d4 - d7}, [r0]!

	subs			r3, r3, #8
	bcs				snxcom_main_loop

snxcom_tail:
	adds			r3, r3, #8
	beq				snxcom_done

	sub				r3, r3, #1				// Index in range [0,6]

	LoadRem1DSamplesMaskDst r0, r1, r14, r4, r3

	// Mask source
	MaskSourceRem

	// Composite over
	CompOver8

	StoreRemNeon	r0, r3, r12

snxcom_done:
	add				sp, sp, #8*4
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestX_CompOverConstMask_NEON(void *dst, void *src, int count, int csx, int cdx, UINT32 opacity)
//  r0 = dst
//  r1 = src
//  r2 = opacity (tmp)
//  r3 = count (tmp)
//  [sp] = csx
//  [sp + 4] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOverConstMask_NEON
	push			{r4 - r12, r14}

	// r5 = opacity (only used during setup phase)
	// r2 = count
	// r3 = csx
	// r4 = cdx
	mov				r5, r2
	mov				r2, r3
	ldr				r3, [sp, #40]
	ldr				r4, [sp, #44]

	// Initialize masking and compositing
	InitMask		r5, r6
	InitCompOver

	// r14 = remainder count
	and				r14, r2, #7
	lsrs			r2, #3					// count /= 8

	// Allocate 8 4 byte words on the stack
	// r5 = line buffer
	sub				sp, #8*4

	// Skip main loop if there is no 8 pixels to work with
	beq				snxcocm_tail

snxcocm_main_loop:
	// Load input data
	Copy8x1DSamples	sp, r1, r3, r4
	LoadNeon		sp, r0

	// Mask and composite over 8 pixels
	Mask8
	CompOver8

	// Store pixels
	StoreNeon		r0

	subs			r2, r2, #1
	bne				snxcocm_main_loop

snxcocm_tail:
	cmp				r14, #0
	beq				snxcocm_done

	// Load remainder
	mov				r5, r3					// r5 = csx
	subs			r3, r14, #1				// r3 = remcnt - 1
	LoadRem1DSamplesDst	r0, r1, r5, r4

	// Mask and composite over 8 pixels
	Mask8
	CompOver8

	// Store remainder
	StoreRemNeon	r0, r3, r12

snxcocm_done:
	// Free 8 4 byte words on the stack
	add				sp, #8*4

	pop				{r4 - r12, r14}
FUNC_EPILOGUE


// void Sampler_NearestXY_CompOver_NEON(void* dst, const void* src, unsigned int cnt, unsigned int stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
//  r0 = dst
//  r1 = src
//  r2 = cnt
//  r3 = stride
//
// Stack:
//  csx
//  cdx
//  csy
//  cdy
FUNC_PROLOGUE_ARM Sampler_NearestXY_CompOver_NEON
	push			{r4 - r11, r14}

	// Load parameters from the stack
	ldr				r4, [sp, #36]			// r4 = csx
	ldr				r12, [sp, #40]			// r12 = cdx
	ldr				r5, [sp, #44]			// r5 = csy
	ldr				r14, [sp, #48]			// r14 = cdy

	// Allocate some space on the stack for ARM->NEON data transfer
	sub				sp, sp, #8*4

	// Prepare a register with 256's.
	vmov.u16		q15, #256				// q15 = [256]..[256]

	subs			r2, r2, #8
	bcc				snxyco_tail

snxyco_main_loop:
	// Preload destination data
	pld				[r0, #8*4]

	// Sample source, increment source coordinate and store samples on the stack
	Copy8x2DSamples	sp, r1, r3, r4, r5, r12, r14
	LoadNeon		sp, r0

	// Composite over
	CompOver8

	// Store pixels
	StoreNeon		r0

	subs			r2, r2, #8
	bcs				snxyco_main_loop

snxyco_tail:
	adds			r2, r2, #8
	beq				snxyco_done

	sub				r2, r2, #1				// Index in range [0,6]

	LoadRem2DSamplesDst	r0, r1, r3, r4, r5, r12, r14

	CompOver8

	StoreRemNeon	r0, r2, r12

snxyco_done:
	add				sp, sp, #8*4
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestXY_CompOverMask_NEON(void* dst, const void* src, const UINT8* alpha, unsigned int cnt, unsigned int stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
//  r0 = dst
//  r1 = src
//  r2 = mask
//  r3 = cnt
//
// Stack:
//  stride
//  csx
//  cdx
//  csy
//  cdy
FUNC_PROLOGUE_ARM Sampler_NearestXY_CompOverMask_NEON
	push			{r4 - r11, r14}

	// Load parameters from the stack
	ldr				r9, [sp, #36]			// r9 = stride
	ldr				r4, [sp, #40]			// r4 = csx
	ldr				r12, [sp, #44]			// r12 = cdx
	ldr				r5, [sp, #48]			// r5 = csy
	ldr				r14, [sp, #52]			// r14 = cdy

	// Allocate some space on the stack for ARM->NEON data transfer
	sub				sp, sp, #8*4

	// Prepare a register with 256's.
	vmov.u16		q15, #256				// q15 = [256]..[256]

	subs			r3, r3, #8
	bcc				snxycom_tail

snxycom_main_loop:
	// Preload destination data
	pld				[r0, #8*4]

	// Sample source, increment source coordinate and store samples on the stack
	Copy8x2DSamples	sp, r1, r9, r4, r5, r12, r14

	// Mask 8 source pixels and load 8 destination pixels into NEON registers
	LoadAndMask8	r0, sp, r2

	// Composite over
	CompOver8

	// Store the result to destination interleaved, then increment
	// the pointer
	vst4.u8			{d4 - d7}, [r0]!

	subs			r3, r3, #8
	bcs				snxycom_main_loop

snxycom_tail:
	adds			r3, r3, #8
	beq				snxycom_done

	sub				r3, r3, #1				// Index in range [0,6]

	LoadRem2DSamplesMaskDst r0, r1, r2, r9, r4, r5, r12, r14, r3

	// Mask source pixels
	MaskSourceRem

	// Composite over
	CompOver8

	StoreRemNeon	r0, r3, r12

snxycom_done:
	// Free stack allocated memory
	add				sp, sp, #8*4

	pop				{r4 - r11, r14}
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM_NEON
