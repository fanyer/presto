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

// Linear interpolation of 8 pixels.
//
// Note:
//  The code is optimized to avoid CPU stalling.
//
// Input:
//  {d0, d1, d2, d3} = {src1.r, src1.g, src1.b, src1.a}
//  {d4, d5, d6, d7} = {src2.r, src2.g, src2.b, src2.a}
//  q14 = frc_y
//
// Output:
//  d4 = src1.r + ((src2.r - src1.r) * frc_y / 256
//  d5 = src1.g + ((src2.g - src1.g) * frc_y / 256
//  d6 = src1.b + ((src2.b - src1.b) * frc_y / 256
//  d7 = src1.a + ((src2.a - src1.a) * frc_y / 256
//
// Side effects:
//  q10 - q13 = scrambled (used for intermediate 16bit r values)
.macro SamplerLerp8
	vsubl.u8		q10, d4, d0				// src2.r - src1.r
	vsubl.u8		q11, d5, d1				// src2.g - src1.g
	vsubl.u8		q12, d6, d2				// src2.b - src1.b
	vsubl.u8		q13, d7, d3				// src2.a - src1.a

	vmul.s16		q10, q10, q14			// * frc_y
	vmul.s16		q11, q11, q14			// * frc_y
	vmul.s16		q12, q12, q14			// * frc_y
	vmul.s16		q13, q13, q14			// * frc_y

	// Put in a strange order to avoid stalls
	vshrn.s16		d4, q10, #8				// / 256
	vshrn.s16		d5, q11, #8				// / 256
	vadd.u8			d4, d0, d4				// + src1.r
	vshrn.s16		d6, q12, #8				// / 256
	vadd.u8			d5, d1, d5				// + src1.g
	vshrn.s16		d7, q13, #8				// / 256
	vadd.u8			d6, d2, d6				// + src1.b
	vadd.u8			d7, d3, d7				// + src1.a
.endm

// Calculate pixel index and fraction and write to memory
//
// Input:
//  r1 = src
//  r2 = csx
//  r3 = cdx
//  r10 = pointer to wehre to write pixel data [i]
//  r11 = pointer to where to write pixel data [i + 1]
//  r12 = pointer to where to write fraction data
//
// Parameters:
//  index - index of frc and dst to write to
//
// Side effects:
//  r2 (csx) stepped by cdx
//  r6 (scrambled)
//  r7 (scrambled)
.macro SamplerLerpXLoadOne index
	SampleToInt		r6, r2							// r6 = pixel_index
	SampleToFrc		r7, r2							// r7 = fraction
	add				r6, r1, r6, lsl #2				// r6 = src + pixel_index
	strb			r7, [r12, #\index]				// Write fraction to memory
	add				r2, r2, r3						// Step csx by cdx
	ldm				r6, {r6, r7}					// Read 2 words into {r6, r7}
	str				r6, [r10, #4*\index]			// Write r6 into src[i] array
	str				r7, [r11, #4*\index]			// Write r7 into src[i+1] array
.endm

// Load 8 pixels from src, starting from csx stepping cdx every time.
// Store pixel data beginning at r11 and fraction data at r12
//
// Input:
//  r1 = src
//  r2 = csx
//  r3 = cdx
//  r10 = pointer to wehre to write pixel data [i]
//  r11 = pointer to where to write pixel data [i + 1]
//  r12 = pointer to where to write fraction data
//
// Side effects:
//  r2 (csx) is stepped 8 * cdx times.
//  r8 (scrambled)
//  r6 (scrambled)
.macro SamplerLerpXLoad
	SamplerLerpXLoadOne 0
	SamplerLerpXLoadOne 1
	SamplerLerpXLoadOne 2
	SamplerLerpXLoadOne 3
	SamplerLerpXLoadOne 4
	SamplerLerpXLoadOne 5
	SamplerLerpXLoadOne 6
	SamplerLerpXLoadOne 7
.endm

// Load pixels. Loading is limited either by csx stepped up to end or
// that dstlen pixels were read.
//
// Input:
//  r1 = src
//  r2 = csx
//  r3 = cdx
//  r4 = dstlen
//  r5 = end
//  r10 = pointer to where to write pixel data [i]
//  r11 = pointer to where to write pixel data [i + 1]
//  r12 = pointer to where to write fraction data
//
// Output:
//  r8 = num pixels read
//
// Side effects:
//  r6 (scrambled)
//  r7 (scrambled)
//  r9 (scrambled) = tmplen
.macro SamplerLerpXLoadRemainder
	// r8 = iteration counter
	mov				r8, #0
	// tmplen = dstlen
	mov				r9, r4
read_remainderx_loop:
	SampleToInt		r6, r2							// r6 = pixel_index
	SampleToFrc		r7, r2							// r7 = fraction
	add				r6, r1, r6, lsl #2				// r6 = src + pixel_index
	strb			r7, [r12, r8]					// Write fraction to memory
	add				r2, r2, r3						// Step csx by cdx
	ldm				r6, {r6, r7}					// Read 2 words into {r6, r7}
	str				r6, [r10, r8, lsl #2]			// Write r6 into src[i] array
	str				r7, [r11, r8, lsl #2]			// Write r7 into src[i+1] array

	// i++
	add				r8, r8, #1

	// Branch if csx < end and tmplen > 0

	// Branch if tmplen > 0
	mov				r6, #0
	subs			r9, r9, #1						// --tmplen
	movne			r6, #1							// branch if tmplen != 0

	// Don't branch if csx >= end
	cmp				r2, r5
	andhs			r6, #0


	movs			r6, r6							// Update the Z flag
	bne				read_remainderx_loop
.endm

.macro SampleToInt dst src
	mov				\dst, \src, lsr 12
.endm

// Extract the fraction part of a vega sample.
//
// Parameters:
//  dst = write to this register*
//  src = read from this register*
//
//  * dst and src can be the same register.
//
// Output:
//  dst = fraction part of src*
//
//  * Note: The 8 least significant bits of the destination register will
//          contain the fraction value. The 24 most significant bits will
//          contain undefined data. To make the dst register only contain
//          the fraction value you need to bitwise-and it with 0xff.
.macro SampleToFrc dst src
	mov				\dst, \src, lsr 4
	// Uncomment the following line if you want to clear garbage bits.
	//and				\dst, \dst, #0xff
.endm

.macro IntToSample dst src
	mov				\dst, \src, lsl 12
.endm

// Load SIMD registers (d0-d7, q14) from memory (r10, r11, r12)
//
// Input:
//  r10 = pointer to src1 (src[i], 32 bit)
//  r11 = pointer to src2 (src[i+1], 32 bit)
//  r12 = pointer to frc (frc[i], 8 bit)
//
// Output:
//  {d0 - d3} = src1 pixels
//  {d4 - d7} = src2 pixels
//  q14 = fractions
.macro SamplerLerpXLoadSIMD
	// Read 8 32 bit pixels from [r10] and store interleaved in {d0 - d3}
	// d0 = {src[i].r0 - src[i].r7}
	// d1 = {src[i].g0 - src[i].g7}
	// d2 = {src[i].b0 - src[i].b7}
	// d3 = {src[i].a0 - src[i].a7}
	vld4.u8			{d0 - d3}, [r10]

	// Read 8 32 bit pixels from [r11] and store interleaved in {d4 - d7}.
	// d4 = {src[i+1].r0 - src[i+1].r7}
	// d5 = {src[i+1].g0 - src[i+1].g7}
	// d6 = {src[i+1].b0 - src[i+1].b7}
	// d7 = {src[i+1].a0 - src[i+1].a7}
	vld4.u8			{d4 - d7}, [r11]

	// Read 8 8 bit fractions from [r12] and store them as 16 bit words in q14
	vld1.u8			d28, [r12]
	vmovl.u8		q14, d28
.endm

// void Sampler_LerpX_NEON(void* dst, const void* src, INT32 csx, INT32 cdx, unsigned int dstlen, unsigned int srclen);
// Input:
//  r0 = dst
//  r1 = src
//  r2 = csx
//  r3 = cdx
//  [sp] = dstlen
//  [sp+4] = srclen
FUNC_PROLOGUE_ARM Sampler_LerpX_NEON
	push			{r4, r5}

	// Load stack varibales.
	// r4 = dstlen
	// r5 = srclen (replaced with end)
	ldr				r4, [sp, 8]
	ldr				r5, [sp, 12]

	push			{r6, r7, r8, r9, r10, r11}

	// while (dstlen && csx < 0)
	// {
	//     *dst++ src[0]
	//     csx += cdx;
	//     dstlen--;
	// }

	// Assumption: dstlen > 0
	cmp				r2, #0
	bge				post_pre

	// r11 = src[0] (temporary)
	ldr				r11, [r1]
pre_loop:

	mov				r12, #1								// branch state (branch if 1)
	add				r2, r2, r3							// csx += cdx
	subs			r4, r4, #1							// dstlen--
	str				r11, [r0], #4						// *dst++ = src[0]

	moveq			r12, #0								// don't branch if dstlen == 0
	cmp				r2, #0
	movpl			r12, #0								// don't branch if csx >= 0
	cmp				r12, #0
	bne				pre_loop

post_pre:

	// Skip lerping if dstlen is zero
	cmp				r4, #0
	beq				post_post

	// Overwrite srclen with vega sample of srclen-1.
	// r5 = end = (srclen - 1)
	sub				r5, r5, #1

	// Allocate 16 32 bit words and 8 8 bit words on the stack.
	// r10 = src+1 pixel buffer pointer (8 32 bit words)
	// r11 = src pixel buffer pointer (8 32 bit words)
	// r12 = frc buffer pointer (8 8 bit words)
	sub				sp, sp, #72
	mov				r10, sp
	add				r11, sp, #32
	add				r12, sp, #64

	// Convert r5 (end) to a vega sample.
	IntToSample		r5, r5

	// Calculate the future remainder of dstlen and push it to the stack.
	and				r6, r4, #7
	push			{r6}

	// Make iterating over dstlen step 8 32 bit words per iteration.
	movs			r4, r4, lsr #3

	// Go to remainderx in case we don't have 8 32 bit words to work with.
	beq				remainderx

	// We can only use fast lerping if we have at least 8 pixels to read left.
	// r8 = csx + (cdx * 8) - cdx (temporary)
	add				r8, r2, r3, lsl 3					// r8 = csx + (cdx<<3)
	sub				r8, r8, r3							// r8 -= cdx
	cmp				r8, r5
	bhs				remainderx							// if r8 > r5 goto remainderx

lerp_loop:

	//  Preload 8 pixels into [r11] and 8 fractions into [r12]
	SamplerLerpXLoad

	// Read 8 32 bit pixels from [r10] and store interleaved in {d0 - d3}
	// d0 = {src[i].r0 - src[i].r7}
	// d1 = {src[i].g0 - src[i].g7}
	// d2 = {src[i].b0 - src[i].b7}
	// d3 = {src[i].a0 - src[i].a7}
	vld4.u8			{d0 - d3}, [r10]

	// Read 8 32 bit pixels from [r11] and store interleaved in {d4 - d7}.
	// d4 = {src[i+1].r0 - src[i+1].r7}
	// d5 = {src[i+1].g0 - src[i+1].g7}
	// d6 = {src[i+1].b0 - src[i+1].b7}
	// d7 = {src[i+1].a0 - src[i+1].a7}
	vld4.u8			{d4 - d7}, [r11]

	// Read 8 8 bit fractions from [r12] and store them as 16 bit words in q14
	vld1.u8			d28, [r12]
	vmovl.u8		q14, d28

	// Lerp into {d0 - d3}
	// src[i] + ((src[i+1] - src[i]) * frc / 256)
	SamplerLerp8

	// Write lerp result ({d0 - d3}) into dst
	vst4.u8			{d4 - d7}, [r0]!

	// Determine if we are to iterate once more.
	// To re-iterate, `(dstlen - 1) != 0 && (csx + (cdx << 3) < end` must hold.
	// To do this r6 is used to keep track if we want to branch or not. If 0, we
	// branch, else not.
	mov				r6, #0
	subs			r4, r4, #1							// --dstlen
	movne			r6, #1								// r6 = r4 == 0 ? 1 : 0

	add				r8, r2, r3, lsl 3					// r8 = csx + (cdx << 3)
	sub				r8, r8, r3							// r8 -= cdx
	cmp				r8, r5
	movhs			r6, #0								// if (r8 >= r5) r6 = 0

	movs			r6, r6								// Update Z status
	bne				lerp_loop							// Branch if Z is not set

remainderx:
	// Assumptions:
	//
	// One or more of these holds:
	//  * csx + cdx > end  - no more to read
	//  * dstlen == 0      - no place to write

	// Get the remainder from the stack
	pop				{r8}

	// Calculate new dstlen (pixel counted, not 8 pixel counted)
	add				r4, r8, r4, lsl 3

	// If dstlen == 0 then no more reading / writing
	cmp				r4, #0
	beq				post_lerp

	// Skip remainder if dstlen == 0 or csx >= end.
	cmp				r2, r5
	bhs				post_lerp

	// Preload up to 8 pixels and fractions. Loading is limited to either stepping
	// up to end or remainder.
	//
	// Output:
	// r8 = num words read
	SamplerLerpXLoadRemainder
	movs			r8, r8
	beq				post_lerp

	// Load fractions into SIMD registers
	// r8 = pixels_read
	SamplerLerpXLoadSIMD

	// Lerp into {d0 - d3}
	// src[i] + ((src[i+1] - src[i]) * frc / 256)
	SamplerLerp8

	// dstlen -= pixels_read
	sub				r4, r4, r8

	sub				r6, r8, #1							// StoreRemNeon expects 0-6
	StoreRemNeon	r0, r6, r7
	add				r0, r0, r8, lsl 2					// Move r0 (dst pointer) r8 words

post_lerp:
	// Deallocate stack memory used for src and frc buffers.
	add				sp, sp, #72

	movs			r4, r4
	beq				post_post

	// while (dstlen > 0)
	// {
	//     *dpix++ = spix[srclen - 1];
	//
	//     csx += cdx;
	//     dstlen--;
	// }

	// Load last pixel into r8
	SampleToInt		r5, r5
	ldr				r8, [r1, r5, lsl 2]
post_lerp_loop:
	// Fill dst with dstlen pixels using the last one from src
	str				r8, [r0], #4

	subs			r4, r4, #1
	bne				post_lerp_loop

post_post:

	pop				{r6, r7, r8, r9, r10, r11}
	pop				{r4, r5}
FUNC_EPILOGUE

// void Sampler_LerpY_NEON(UINT32* dst, const UINT32* src1, const UINT32* src2, INT32 frc_y, unsigned int len);
// Input:
//  r0 = dst
//  r1 = src1
//  r2 = src2
//  r3 = frc_y
//  [sp] = len
FUNC_PROLOGUE_ARM Sampler_LerpY_NEON
	push			{r4, r5}

	// r4 = len (loaded from stack)
	ldr				r4, [sp, 8]

	// Store the 8bit frc_y (r3) in 16bit form in q14 and since q14 = {d28, d29}
	// we can do this by storing the 32bit (frc_y << 16 | frc_y) into both d28
	// and d29.
	orr				r3, r3, r3, lsl #16
	vmov			d28, r3, r3
	vmov			d29, d28

	// Align destination to a cache-line boundary
	ands			r3, r0, #31				// How many bytes passed cache-line boundary?
	beq				sly_aligned

	mov				r3, r3, lsr #2			// How many pixels passed cache-line bondary?
	rsb				r3, r3, #8				// r3 = num_unaligned_pixels
	cmp				r3, r4
	movgt			r3, r4					// r3 = MIN(num_unaligned_pixels, len)

	// The index should be 0-6.
	sub				r5, r3, #1

	LoadRemSrcs		 r1 r2 r5 r12

	SamplerLerp8

	StoreRemNeon	 r0 r5 r12

	sub				r4, r4, r3				// len -= num_unaligned_pixels
	add				r0, r0, r3, asl #2		// dst += num_unaligned_pixels
	add				r1, r1, r3, asl #2		// src1 += num_unaligned_pixels
	add				r2, r2, r3, asl #2		// src2 += num_unaligned_pixels

sly_aligned:

	// Prepare for unrolled operation
	subs			r4, r4, #8
	bcc				sly_tail

sly_inner_loop:
	// Pre-load source data 1
	pld				[r1, #16*8]

	// Read 8 pixels from src1
	// d0 = {src1.r0 - src1.r7}
	// d1 = {src1.g0 - src1.g7}
	// d2 = {src1.b0 - src1.b7}
	// d3 = {src1.a0 - src1.a7}
	vld4.u8			{d0 - d3}, [r1]!

	// Pre-load source data 2
	pld				[r2, #16*8]

	// Read 8 pixels from src2
	// d4 = {src2.r0 - src2.r7}
	// d5 = {src2.g0 - src2.g7}
	// d6 = {src2.b0 - src2.b7}
	// d7 = {src2.a0 - src2.a7}
	vld4.u8			{d4 - d7}, [r2]!

	SamplerLerp8

	vst4.u8			{d4 - d7}, [r0,:256]!

	subs			r4, r4, #8
	bcs				sly_inner_loop

sly_tail:
	adds			r4, r4, #8
	beq				sly_done

	sub				r5, r4, #1
	LoadRemSrcs		r1 r2 r5 r12

	SamplerLerp8

	StoreRemNeon	 r0 r5 r12

sly_done:
	pop				{r4, r5}
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM_NEON
