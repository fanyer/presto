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

.arch armv7-a		// Support ARMv7 instructions
.fpu neon			// Support NEON instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text

// Load `count + 1' pixels from dst and src. `count' must be between 0 and 6.
//
// Parameters:
//	dst - dst pointer.
//	src - src pointer.
//	count - remainder count, between 0 and 6.
//
// Labels:
//	$0 - table label
//	$1 - case label
//
// Output:
//	{d0 - d3} - will be loaded with src pixels de-interleaved.
//	{d4 - d7} - will be loaded with dst pixels de-interleaved.
//	q14 - will be loaded with mask (+1, u16).
//
// Side effects:
//   tmp
.macro LoadRemDstSrc dst src count tmp
	LoadRemSrcs \src, \dst, \count, \tmp
.endm

// Load `count + 1' pixels from dst.
//
// Parameters:
//	dst - dst pointer.
//	count - remainder count, between 0 and 6.
//
// Labels:
//	$0 - table label
//	$1 - case label
//
// Output:
//	{d0 - d3} - will be loaded with src pixels de-interleaved.
//	{d4 - d7} - will be loaded with dst pixels de-interleaved.
//	q14 - will be loaded with mask (+1, u16).
//
// Side effects:
//   r12
.macro LoadRemDst dst count
	adr				r12, 8f
	ldr				pc, [r12, \count, lsl #2]
8:
	.word 1f
	.word 2f
	.word 3f
	.word 4f
	.word 5f
	.word 6f
	.word 7f
7:
	add				r12, \dst, #24							// r12 = dst + 24
	vld4.u8			{d4[6], d5[6], d6[6], d7[6]}, [r12]
6:
	add				r12, \dst, #20
	vld4.u8			{d4[5], d5[5], d6[5], d7[5]}, [r12]
5:
	add				r12, \dst, #16
	vld4.u8			{d4[4], d5[4], d6[4], d7[4]}, [r12]
4:
	add				r12, \dst, #12
	vld4.u8			{d4[3], d5[3], d6[3], d7[3]}, [r12]
3:
	add				r12, \dst, #8
	vld4.u8			{d4[2], d5[2], d6[2], d7[2]}, [r12]
2:
	add				r12, \dst, #4
	vld4.u8			{d4[1], d5[1], d6[1], d7[1]}, [r12]
1:
	vld4.u8			{d4[0], d5[0], d6[0], d7[0]}, [\dst]
.endm

// Load `count + 1' pixels from dst and opacity values from mask.
//
// Parameters:
//	dst - dst pointer.
//	mask - mask pointer
//	count - remainder count, between 0 and 6.
//
// Labels:
//	$0 - table label
//	$1 - case label
//
// Output:
//	{d0 - d3} - will be loaded with src pixels de-interleaved.
//	{d4 - d7} - will be loaded with dst pixels de-interleaved.
//	q14 - will be loaded with mask (+1, u16).
//
// Side effects:
//   r12
.macro LoadRemDstMask dst mask count
	adr				r12, 8f
	vmov.u8			d29, #1
	ldr				pc, [r12, \count, lsl #2]
8:
	.word 1f
	.word 2f
	.word 3f
	.word 4f
	.word 5f
	.word 6f
	.word 7f
7:
	add				r12, \dst, #24							// r12 = dst + 24
	vld4.u8			{d4[6], d5[6], d6[6], d7[6]}, [r12]
	add				r12, \mask, #6							// r12 = mask + 6
	vld1.u8			{d28[6]}, [r12]
6:
	add				r12, \dst, #20
	vld4.u8			{d4[5], d5[5], d6[5], d7[5]}, [r12]
	add				r12, \mask, #5
	vld1.u8			{d28[5]}, [r12]
5:
	add				r12, \dst, #16
	vld4.u8			{d4[4], d5[4], d6[4], d7[4]}, [r12]
	add				r12, \mask, #4
	vld1.u8			{d28[4]}, [r12]
4:
	add				r12, \dst, #12
	vld4.u8			{d4[3], d5[3], d6[3], d7[3]}, [r12]
	add				r12, \mask, #3
	vld1.u8			{d28[3]}, [r12]
3:
	add				r12, \dst, #8
	vld4.u8			{d4[2], d5[2], d6[2], d7[2]}, [r12]
	add				r12, \mask, #2
	vld1.u8			{d28[2]}, [r12]
2:
	add				r12, \dst, #4
	vld4.u8			{d4[1], d5[1], d6[1], d7[1]}, [r12]
	add				r12, \mask, #1
	vld1.u8			{d28[1]}, [r12]
1:
	vld4.u8			{d4[0], d5[0], d6[0], d7[0]}, [\dst]
	vld1.u8			{d28[0]}, [\mask]
	vaddl.u8		q14, d28, d29
.endm

// void CompOver_NEON(void* dst, const void* src, unsigned int len);
// r0 = dst
// r1 = src
// r2 = len
FUNC_PROLOGUE_ARM CompOver_NEON
	push			{r4-r11,r14}

	InitCompOver

	// Prepare for un-rolled operation
	subs			r2, r2, #8
	bcc				co_tail

	// Align destination to a cache-line boundary
	// NOTE: The number of unaligned pixels that we need to process is in the
	// range 0-7 (0 means already aligned), which is always less than the
	// total number of pixels to process, since we have already guaranteed that
	// len >= 8 at this stage.
	ands			r4, r0, #31
	beq				co_aligned				// Already aligned?
	mov				r4, r4, lsr #2
	rsb				r4, r4, #8				// r4 = num_unaligned_pixels (1-7)

	// The index should be 0-6.
	sub				r3, r4, #1

	// Load up d0-d3 with src pixels and d4-d7 with dst pixels.
	LoadRemDstSrc	r0, r1, r3, r12

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r3, r12

	add				r0, r0, r4, asl #2		// dst += num_unaligned_pixels
	add				r1, r1, r4, asl #2		// src += num_unaligned_pixels
	subs			r2, r2, r4				// len -= num_unaligned_pixels
	bcc				co_tail


co_aligned:
	// Load one pixel into r12 (for checking its alpha value)
	ldr				r12, [r1]

co_blend_loop:
	// Should we try to switch to transparent/opaque operation?
	movs			r12, r12, lsr #24
	beq				co_switch_to_transparent
	cmp				r12, #0xff
	beq				co_switch_to_opaque

co_switch_to_blend:
	// Preload data
	pld				[r1, #32*4]
	pld				[r0, #32*4]

	// ---- Un-rolled step 1: 8 pixels ----

	// Load up d0-d3 with de-interleaved src pixels, and increment pointer.
	vld4.u8			{d0 - d3}, [r1]!

	// Load up d4-d7 with de-interleaved dst pixels.
	vld4.u8			{d4 - d7}, [r0,:256]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result to destination interleaved, and increment pointer.
	vst4.u8			{d4 - d7}, [r0,:256]!

	subs			r2, r2, #8				// len -= 8
	bcc				co_tail

	// ---- Un-rolled step 2: 8 pixels ----

	// Load one pixel into r12 (for checking its alpha value later)
	ldr				r12, [r1, #7*4]

	// Load up d0-d3 with de-interleaved src pixels, and increment pointer.
	vld4.u8			{d0 - d3}, [r1]!

	// Load up d4-d7 with de-interleaved dst pixels.
	vld4.u8			{d4 - d7}, [r0,:256]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result to destination interleaved, and increment pointer.
	vst4.u8			{d4 - d7}, [r0,:256]!

	subs			r2, r2, #8				// len -= 8
	bcs				co_blend_loop

	b				co_tail


co_switch_to_opaque:

co_opaque_loop:
	// Preload source data
	pld				[r1, #32*4]				// Preload-distance chosen empirically

	ldm				r1, {r5-r11,r14}		// Load src0..src7 into ARM registers
	vld4.u8			{d0 - d3}, [r1]			// Load src0..src7 into NEON registers
											// Note: This costs about 1 cycle, but
											//  saves cycles later when storing the
											//  data to the destination.

	// Make sure that we are still fully opaque
	and				r3, r5, r6
	and				r12, r7, r8
	and				r3, r3, r9
	and				r12, r12, r10
	and				r3, r3, r11
	and				r12, r12, r14
	and				r3, r3, #0xff000000
	and				r3, r3, r12
	cmp				r3, #0xff000000
	bne				co_switch_to_blend		// If alpha != 0xff, switch to blending

	add				r1, r1, #8*4			// src += 8

	vst4.u8			{d0 - d3}, [r0,:256]!	// Store src0..src7 to destination

	subs			r2, r2, #8				// len -= 8

	bcs				co_opaque_loop			// while (len > 0)

	b				co_tail


co_switch_to_transparent:

co_transparent_loop:
	// Preload source data
	pld				[r1, #32*4]				// Preload-distance chosen empirically

	ldm				r1, {r5-r11,r14}		// Load src0..src7 into ARM registers

	// Make sure that we are still fully transparent
	// Note: we only have to check if all pixles are exactly zero, since we
	// use premultiplied alpha (a == 0 -> r,g,b = 0,0,0)
	orr				r5, r5, r6
	orr				r6, r7, r8
	orr				r7, r9, r10
	orr				r8, r11, r14
	orr				r5, r5, r6
	orr				r6, r7, r8
	orrs			r5, r5, r6
	bne				co_switch_to_blend		// If alpha != 0x00, switch to blending

	subs			r2, r2, #8				// len -= 8
	add				r1, r1, #8*4			// src += 8
	add				r0, r0, #8*4			// dst += 8

	bcs				co_transparent_loop


co_tail:
	adds			r2, r2, #8
	beq				co_done

	// For the remainder we first load up the registers with partial data,
	// 1-7 lanes individually. Then we run the operation as if it was 8
	// pixels.

	// The index should be 0-6.
	sub				r3, r2, #1

	// Load up d0-d3 with src pixels and d4-d7 with dst pixels.
	LoadRemDstSrc	r0, r1, r3, r12

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r3, r12

co_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void CompConstOver_NEON(void* dst, UINT32 src, unsigned int len);
// r0 = dst
// r1 = src
// r2 = len
FUNC_PROLOGUE_ARM CompConstOver_NEON
	push			{r4 - r8}

	// Fast cases
	movs			r3, r1, lsr #24
	beq				cco_done			// Nothing to do if alpha = 0
	cmp				r3, #255
	beq				cco_fill			// Pure fill if alpha = 255

	InitCompOver

	// Duplicate src and convert it to interleaved format.
	// Note: We transfer via the stack to utilize the de-interleaving
	// functionality of NEON.
	sub				r4, sp, #32
	sub				r5, sp, #16
	sub				sp, sp, #32
	mov				r6, r1
	mov				r7, r1
	mov				r8, r1
	stm				r4, {r1, r6, r7, r8}
	stm				r5, {r1, r6, r7, r8}
	vld4.u8			{d0 - d3}, [r4]		// d0 - d3 = [src]x8
	add				sp, sp, #32

	// Prepare for un-rolled operation
	subs			r2, r2, #8
	bcc				cco_remainder

cco_inner_loop:
	// Preload destination data
	pld				[r0, #32*4]

	// Load up destination.
	// q2_lo/d4 = [dst.r0-dst.r7]
	// q2_hi/d5 = [dst.g0-dst.g7]
	// q3_lo/d6 = [dst.b0-dst.b7]
	// q3_hi/d7 = [dst.a0-dst.a7]
	vld4.u8			{d4 - d7}, [r0]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d4 - d7}, [r0]!

	subs			r2, r2, #8
	bcs				cco_inner_loop

cco_remainder:
	// Check if there are any pixels left to process.
	adds			r2, #8
	beq				cco_done

	// For the remainder we first load up the registers with partial data,
	// 1-7 lanes individually. Then we run the operation as if it was 8
	// pixels.

	// The index should be 0-6.
	sub				r3, r2, #1

	// Load up d4-d7 with dst pixels.
	LoadRemDst		r0, r3

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r3, r12

	b				cco_done


cco_fill:
	subs			r2, r2, #8
	bcc				cco_fill_tail

	// Load source color into seven more registers
	mov				r3, r1
	mov				r4, r1
	mov				r5, r1
	mov				r6, r1
	mov				r7, r1
	mov				r8, r1
	mov				r12, r1

cco_fill_loop:
	stmia			r0!, {r1,r3-r8,r12}	// Store to destination
	subs			r2, r2, #8			// len -= 8
	bcs				cco_fill_loop


cco_fill_tail:
	adds			r2, r2, #8
	beq				cco_done

cco_fill_tail_loop:
	str				r1, [r0], #4		// Store to destination
	subs			r2, r2, #1			// len--
	bne				cco_fill_tail_loop


cco_done:
	pop				{r4 - r8}
FUNC_EPILOGUE


//void CompConstOverMask_NEON(void* dst, unsigned int src, const UINT8* mask, unsigned int len);
// r0 = dst
// r1 = src
// r2 = mask
// r3 = len
FUNC_PROLOGUE_ARM CompConstOverMask_NEON
	push			{r4 - r7}
	vpush			{q4 - q7}

	cmp				r1, #0
	beq				ccom_done			// Do nothing if alpha == 0

	InitCompOver

	// Load up source de-interleaved, one color channel per register.
	// Note: We transfer via the stack to utilize the de-interleaving
	// functionality of NEON.
	// q4 = [src.r0-src.r7] in u16
	// q5 = [src.g0-src.g7] in u16
	// q6 = [src.b0-src.b7] in u16
	// q7 = [src.a0-src.a7] in u16
	sub				r4, sp, #32
	sub				r5, sp, #16
	sub				sp, sp, #32
	mov				r6, r1
	mov				r7, r1
	mov				r12, r1
	stm				r4, {r1, r6 - r7, r12}
	stm				r5, {r1, r6 - r7, r12}
	vld4.u8			{d8 - d11}, [r4]
	add				sp, sp, #32
	vmovl.u8		q7, d11
	vmovl.u8		q6, d10
	vmovl.u8		q5, d9
	vmovl.u8		q4, d8

	// Prepare for un-rolled operation
	subs			r3, r3, #8
	bcc				ccom_remainder

	// Align mask to a word boundary
	// NOTE: The number of unaligned pixels that we need to process is in the
	// range 0-3 (0 means already aligned), which is always less than the
	// total number of pixels to process, since we have already guaranteed that
	// len >= 8 at this stage.
	ands			r4, r2, #3
	beq				ccom_aligned			// Already aligned?
	rsb				r4, r4, #4			// r4 = num_unaligned_pixels (1-3)

	// The index should be 0-6 (NOTE: In fact, always 0-2).
	sub				r5, r4, #1

	// Load up q14 with mask.
	// Load up d4-d7 with dst pixels.
	LoadRemDstMask	r0, r2, r5

	// Multiply source by mask.
	vmul.u16		q8, q14, q4			// q9(d18-d19) = [mask0*src.r]..[mask7*src.r]
	vmul.u16		q9, q14, q5			// q10(d20-d21) = [mask0*src.g]..[mask7*src.g]
	vmul.u16		q10, q14, q6		// q11(d22-d23) = [mask0*src.b]..[mask7*src.b]
	vmul.u16		q11, q14, q7		// q12(d24-d25) = [mask0*src.a]..[mask7*src.a]
	vshrn.u16		d0, q8, #8			// d0 = [(mask0*src.r)>>8]..[(mask7*src.r)>>8]
	vshrn.u16		d1, q9, #8			// d1 = [(mask0*src.g)>>8]..[(mask7*src.g)>>8]
	vshrn.u16		d2, q10, #8			// d2 = [(mask0*src.b)>>8]..[(mask7*src.b)>>8]
	vshrn.u16		d3, q11, #8			// d3 = [(mask0*src.a)>>8]..[(mask7*src.a)>>8]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r5, r12

	add				r0, r0, r4, lsl #2	// dst += num_unaligned_pixels
	add				r1, r1, r4, lsl #2	// src += num_unaligned_pixels
	add				r2, r2, r4			// mask += num_unaligned_pixels
	subs			r3, r3, r4			// len -= num_unaligned_pixels
	bcc				ccom_remainder

ccom_aligned:

ccom_inner_loop:
	// Preload data
	pld				[r0, #32*4]
	pld				[r2, #32]

	// Load the mask into q14 (8x u16).
	// Then increment the pointer.
	// q14 = [mask0-mask7]
	vld1.u8			d28, [r2]			// d28 = [mask0-1]..[mask7-1] in u8
	vmov.u8			d29, #1
	ldmia			r2!, {r4, r5}		// Load the masks into r4,r5
	vaddl.u8		q14, d28, d29		// q14 = [mask0]..[mask7] in u16

	// Check if we should switch to zero/one mask operation
	orrs			r6, r4, r5
	beq				ccom_switch_to_mask0

ccom_switch_to_main:
	// Multiply source by mask
	vmul.u16		q11, q14, q7		// q12(d24-d25) = [mask0*src.a]..[mask7*src.a]
	vmul.u16		q10, q14, q6		// q11(d22-d23) = [mask0*src.b]..[mask7*src.b]
	vmul.u16		q9, q14, q5			// q10(d20-d21) = [mask0*src.g]..[mask7*src.g]
	vmul.u16		q8, q14, q4			// q9(d18-d19) = [mask0*src.r]..[mask7*src.r]

	// Load up destination.
	// q2_lo/d4 = [dst.r0-dst.r7]
	// q2_hi/d5 = [dst.g0-dst.g7]
	// q3_lo/d6 = [dst.b0-dst.b7]
	// q3_hi/d7 = [dst.a0-dst.a7]
	vld4.u8			{d4 - d7}, [r0]

	// Shift 16-bit masked data to 8-bit precision
	vshrn.u16		d3, q11, #8			// d3 = [(mask0*src.a)>>8]..[(mask7*src.a)>>8]
	vshrn.u16		d2, q10, #8			// d2 = [(mask0*src.b)>>8]..[(mask7*src.b)>>8]
	vshrn.u16		d1, q9, #8			// d1 = [(mask0*src.g)>>8]..[(mask7*src.g)>>8]
	vshrn.u16		d0, q8, #8			// d0 = [(mask0*src.r)>>8]..[(mask7*src.r)>>8]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d4 - d7}, [r0]!

	subs			r3, r3, #8
	bcs				ccom_inner_loop

	b				ccom_remainder


ccom_mask0_loop:
	// Preload data
	pld				[r2, #32]

	// Load mask
	vld1.u8			d28, [r2]			// d28 = [mask0-1]..[mask7-1] in u8
	vmov.u8			d29, #1
	ldmia			r2!, {r4, r5}		// Load the masks into r4,r5
	vaddl.u8		q14, d28, d29		// q14 = [mask0]..[mask7] in u16

	// Check if we still have a zero mask
	orrs			r4, r4, r5
	bne				ccom_switch_to_main	// Back to normal operation if mask is not 0

ccom_switch_to_mask0:
	add				r0, r0, #8*4			// dst += 8
	subs			r3, r3, #8
	bcs				ccom_mask0_loop

	b				ccom_remainder


ccom_remainder:
	// Check if there are any pixels left to process.
	adds			r3, r3, #8
	beq				ccom_done

	// For the remainder we first load up the registers with partial data,
	// 1-7 lanes individually. Then we run the operation as if it was 8
	// pixels.

	// The index should be 0-6.
	sub				r3, r3, #1

	// Load up q14 with mask.
	// Load up d4-d7 with dst pixels.
	LoadRemDstMask	r0, r2, r3

	// Multiply source by mask.
	vmul.u16		q8, q14, q4			// q9(d18-d19) = [mask0*src.r]..[mask7*src.r]
	vmul.u16		q9, q14, q5			// q10(d20-d21) = [mask0*src.g]..[mask7*src.g]
	vmul.u16		q10, q14, q6		// q11(d22-d23) = [mask0*src.b]..[mask7*src.b]
	vmul.u16		q11, q14, q7		// q12(d24-d25) = [mask0*src.a]..[mask7*src.a]
	vshrn.u16		d0, q8, #8			// d0 = [(mask0*src.r)>>8]..[(mask7*src.r)>>8]
	vshrn.u16		d1, q9, #8			// d1 = [(mask0*src.g)>>8]..[(mask7*src.g)>>8]
	vshrn.u16		d2, q10, #8			// d2 = [(mask0*src.b)>>8]..[(mask7*src.b)>>8]
	vshrn.u16		d3, q11, #8			// d3 = [(mask0*src.a)>>8]..[(mask7*src.a)>>8]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r3, r12

ccom_done:
	vpop			{q4 - q7}
	pop				{r4 - r7}
FUNC_EPILOGUE


//void CompOverConstMask_NEON(void* dst, const void* src, unsigned int mask, unsigned int len);
// r0 = dst
// r1 = src
// r2 = mask (1-256)
// r3 = len
FUNC_PROLOGUE_ARM CompOverConstMask_NEON
	// Do nothing if mask <= 1 (i.e. zero)
	cmp				r2, #1
	ble				cocm_exit

	// Plain composite over if mask >= 256
	cmp				r2, #256
	blt				cocm_normal
	mov				r2, r3				// r2 = len
	push			{lr}
	bl				CompOver_NEON
	pop				{lr}
	b				cocm_exit

cocm_normal:
	push			{r4 - r5}

	// Prepare the mask (q14) and an all-256's register (q15).
	orr				r2, r2, r2, lsl #16
	vmov.u16		q15, #256			// q15 = [256]..[256]
	mov				r4, r2
	vmov			d28, r2, r4
	vmov			d29, r2, r4			// q14(d28-d29) = mask x8 (in u16 format)

	subs			r3, r3, #8
	bcc				cocm_remainder

cocm_inner_loop:
	// Preload data
	pld				[r1, #32*4]
	pld				[r0, #32*4]

	// Load up source de-interleaved, one color channel per register.
	// Then increment the pointer.
	// q0_lo/d0 = [src.r0-src.r7]
	// q0_hi/d1 = [src.g0-src.g7]
	// q1_lo/d2 = [src.b0-src.b7]
	// q1_hi/d3 = [src.a0-src.a7]
	vld4.u8			{d0 - d3}, [r1]!

	// Multiply source by mask.
	Mask8

	// Load up destination.
	// q2_lo/d4 = [dst.r0-dst.r7]
	// q2_hi/d5 = [dst.g0-dst.g7]
	// q3_lo/d6 = [dst.b0-dst.b7]
	// q3_hi/d7 = [dst.a0-dst.a7]
	vld4.u8			{d4 - d7}, [r0]

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d4 - d7}, [r0]!

	subs			r3, r3, #8
	bcs				cocm_inner_loop

cocm_remainder:
	// Check if there are any pixels left to process.
	adds			r3, r3, #8
	beq				cocm_done

	// For the remainder we first load up the registers with partial data,
	// 1-7 lanes individually. Then we run the operation as if it was 8
	// pixels.

	// The index should be 0-6.
	sub				r3, r3, #1

	// Load up d0-d3 with src pixels and d4-d7 with dst pixels.
	LoadRemDstSrc	r0, r1, r3, r12

	// Multiply source by mask.
	Mask8

	// dst.rgba = src.rgba + (256 - src.a) * dst.rgba.
	CompOver8

	// Store the result back to memory.
	StoreRemNeon	r0, r3, r12

cocm_done:
	pop				{r4 - r5}
cocm_exit:
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM_NEON
