/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM)

#include "vegacompositeover_armv6.h"
#include "vegasampler_arm.h"

.arch armv6			// Support ARMv6 instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text


// void Sampler_NearestX_ARMv6(void* dst, const void* src, unsigned int cnt, INT32 csx, INT32 cdx);
//  r0 = dst
//  r1 = src
//  r2 = cnt
//  r3 = csx
//  [sp] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_ARMv6
	push			{r4 - r11}

	ldr				r4, [sp, #32]			// r4 = cdx

	// Align destination to a cache line boundary
	cmp				r2, #0
snx_head_loop:
	beq				snx_done				// If cnt == 0...
	ands			r12, r0, #31			// Are we aligned yet?
	beq				snx_aligned

	Load1DSample	r12, r1, r3, r4			// r12 = src[(csx >> 12) * 4]
	str				r12, [r0], #4			// Store destination
	subs			r2, r2, #1
	b				snx_head_loop

snx_aligned:
	subs			r2, r2, #8
	bcc				snx_tail

	cmp				r4, #1<<12				// Can we do a plain memory copy?
	beq				snx_plain_copy


snx_main_loop:
	// Un-rolled to sample 8 pixels per iteration
	Load8x1DSamples	r1, r3, r4				// r5-r12 = src0..src7
	subs			r2, r2, #8				// cnt -= 8
	stmia			r0!, {r5 - r12}			// Store destination
	bcs				snx_main_loop

snx_tail:
	adds			r2, r2, #8
	beq				snx_done

snx_tail_loop:
	Load1DSample	r12, r1, r3, r4			// r12 = src[(csx >> 12) * 4]
	str				r12, [r0], #4			// Store destination
	subs			r2, r2, #1				// cnt--
	bne				snx_tail_loop

	b				snx_done


snx_plain_copy:
	mov				r5, r3, asr #12			// r5 = csx >> 12
	add				r1,	r1, r5, lsl #2		// src += csx >> 12

snx_copy_loop:
	// Pre-load source data
	pld				[r1, #32*4]

	// Un-rolled to sample 8 pixels per iteration
	ldmia			r1!, {r5 - r12}			// r5-r12 = src0..src7
	subs			r2, r2, #8				// cnt -= 8
	stmia			r0!, {r5 - r12}			// Store destination
	bcs				snx_copy_loop

	adds			r2, r2, #8
	beq				snx_done

snx_copy_tail_loop:
	ldr				r12, [r1], #4			// r12 = *src++
	subs			r2, r2, #1				// cnt--
	str				r12, [r0], #4			// *dst++ = r12
	bne				snx_copy_tail_loop

snx_done:
	pop				{r4 - r11}
FUNC_EPILOGUE


// void Sampler_NearestX_CompOver_ARMv6(void* dst, const void* src, unsigned int cnt, INT32 csx, INT32 cdx);
//  r0 = dst
//  r1 = src
//  r2 = cnt
//  r3 = csx
//  [sp] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOver_ARMv6
	push			{r4 - r11, r14}

	ldr				r4, [sp, #36]			// r4 = cdx

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Just to make the zero/one switches false later if we're already aligned
	mov				r8, #0x80000000

	// Align destination to a cache line boundary
	cmp				r2, #0
snxco_headloop:
	beq				snxco_done				// If cnt == 0...
	ands			r12, r0, #15			// Are we aligned yet?
	beq				snxco_aligned

	// Sample source and increment source coordinate
	Load1DSample	r8, r1, r3, r4			// r8 = src[(csx >> 12) * 4]

	ldr				r9, [r0]				// r9 = dst0
	CompOver		r9, r8					// dst0 = src0 + (256 - src0.a) * dst0
	str				r9, [r0], #4			// *dst++ = dst0

	subs			r2, r2, #1
	b				snxco_headloop

snxco_aligned:
	subs			r2, r2, #4
	bcc				snxco_tail


snxco_blend_loop:
	// Should we try to switch to transparent/opaque operation?
	movs			r8, r8, lsr #24
	beq				snxco_switch_to_transparent
	cmp				r8, #0xff
	beq				snxco_switch_to_opaque

	// Preload destination data
	pld				[r0, #16*4]

	Load1DSample	r7, r1, r3, r4			// r7 = src0
	Load1DSample	r8, r1, r3, r4			// r8 = src1

	// We enter the blending loop here when switching from opaque/transparent
snxco_blend_loop_entry:
	ldm				r0, {r9, r14}			// Load dst0 - dst1
	CompOver		r9, r7					// dst0 = src0 + (256 - src0.a) * dst0
	CompOver		r14, r8					// dst1 = src1 + (256 - src1.a) * dst1
	stmia			r0!, {r9, r14}			// Store dst0 - dst1

	Load1DSample	r7, r1, r3, r4			// r7 = src2
	Load1DSample	r8, r1, r3, r4			// r8 = src3
	ldm				r0, {r9, r14}			// Load dst2 - dst3
	CompOver		r9, r7					// dst2 = src2 + (256 - src2.a) * dst2
	CompOver		r14, r8					// dst3 = src3 + (256 - src3.a) * dst3
	stmia			r0!, {r9, r14}			// Store dst2 - dst3

	subs			r2, r2, #4
	bcs				snxco_blend_loop

	b				snxco_tail


	// This is the target when exiting the snxco_opaque_loop and
	// snxco_transparent_loop loops.
	// NOTE: When entering this point, the following must be fulfilled:
	//  r7 = src0
	//  r8 = src1
	//  r3 = correct csx + 2 * cdx
	//  r9 = cdx * 2
snxco_switch_to_blend:
	sub				r3, r3, r9				// csx -= cdx * 2
	b				snxco_blend_loop_entry


snxco_switch_to_opaque:
	mov				r9, r4, asl #1			// r9 = cdx * 2

snxco_opaque_loop:
	Load1DSample	r7, r1, r3, r4			// r7  = src0
	Load1DSample	r8, r1, r3, r4			// r8  = src1
	Load1DSample	r11, r1, r3, r4			// r11 = src2
	Load1DSample	r12, r1, r3, r4			// r12 = src3

	// Still fully opaque?
	and				r6, r7, r8
	and				r14, r11, r12
	and				r6, r6, r14
	adds			r6, r6, #0x01000000		// If alpha != 0xff, carry is 0 after this operation
	bcc				snxco_switch_to_blend

	stmia			r0!, {r7,r8,r11,r12}	// Store src0..src3 to destination

	subs			r2, r2, #4				// len -= 4
	bcs				snxco_opaque_loop

	b				snxco_tail


snxco_switch_to_transparent:
	mov				r9, r4, asl #1			// r9 = cdx * 2

snxco_transparent_loop:
	Load1DSample	r7, r1, r3, r4			// r7  = src0
	Load1DSample	r8, r1, r3, r4			// r8  = src1
	Load1DSample	r11, r1, r3, r4			// r11 = src2
	Load1DSample	r12, r1, r3, r4			// r12 = src3

	// Make sure that we are still fully transparent
	// Note: we only have to check if all pixles are exactly zero, since we
	// use premultiplied alpha (a == 0 -> r,g,b = 0,0,0)
	orr				r6, r7, r8
	orr				r14, r11, r12
	orrs			r6, r6, r14
	bne				snxco_switch_to_blend		// If alpha != 0x00, switch to blending

	subs			r2, r2, #4				// len -= 4
	add				r0, r0, #4*4			// dst += 4
	bcs				snxco_transparent_loop


snxco_tail:
	adds			r2, r2, #4
	beq				snxco_done

snxco_tail_loop:
	// Sample source and increment source coordinate
	Load1DSample	r7, r1, r3, r4			// r7 = src[(csx >> 12) * 4]

	ldr				r9, [r0]				// r9 = dst0
	CompOver		r9, r7					// dst0 = src0 + (256 - src0.a) * dst0
	str				r9, [r0], #4			// *dst++ = dst0

	subs			r2, r2, #1
	bne				snxco_tail_loop

snxco_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestX_CompOverMask_ARMv6(void* dst, const void* src, const UINT8* alpha, unsigned int cnt, INT32 csx, INT32 cdx);
//  r0 = dst
//  r1 = src
//  r2 = alpha
//  r3 = cnt
//  [sp] = csx
//  [sp+4] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOverMask_ARMv6
	push			{r4 - r11, r14}

	mov				r9, r2					// r9 = alpha
	movs			r2, r3					// r2 = cnt
	beq				snxcom_done

	ldr				r3, [sp, #36]			// r3 = csx
	ldr				r4, [sp, #40]			// r4 = cdx

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Align mask to a word boundary
	cmp				r2, #0
snxcom_head_loop:
	beq				snxcom_done
	ands			r12, r9, #3				// Are we aligned yet?
	beq				snxcom_aligned

	ldrb			r12, [r9], #1			// r12 = mask0
	Load1DSample	r7, r1, r3, r4			// r7 = src0
	add				r12, r12, #1			// r12 = mask0 + 1
	Mask			r6, r11, r7, r12		// r6/r11 = masked src0
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0], #4			// Store dst0

	subs			r2, r2, #1
	b				snxcom_head_loop

snxcom_aligned:
	subs			r2, r2, #4
	bcc				snxcom_tail


snxcom_main_loop:
	// Preload data
	pld				[r0, #16*4]
	pld				[r9, #16]

	ldr				r14, [r9], #4			// r12 = mask3..mask0

	Load1DSample	r7, r1, r3, r4			// r7 = src0

snxcom_switch_to_main:
	cmp				r14, #0x00000000
	beq				snxcom_switch_to_mask0	// Switch to zero-mask mode?
	cmp				r14, #0xffffffff
	beq				snxcom_switch_to_mask1	// Switch to one-mask mode?

	and				r12, r14, #255			// r12 = mask0
	mov				r14, r14, lsr #8		// r14 >>= 8
	cmp				r7, #0
	beq				1f						// Skip this sample if alpha == 0
	add				r12, r12, #1			// r12 = mask0 + 1
	Mask			r6, r11, r7, r12		// r6/r11 = masked src0
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0]				// Store dst0
1:
	add				r0, r0, #4				// dst++
	Load1DSample	r7, r1, r3, r4			// r7 = src1
	and				r12, r14, #255			// r12 = mask1
	mov				r14, r14, lsr #8		// r14 >>= 8
	cmp				r7, #0
	beq				2f						// Skip this sample if alpha == 0
	add				r12, r12, #1			// r12 = mask1 + 1
	Mask			r6, r11, r7, r12		// r6/r11 = masked src1
	ldr				r7, [r0]				// r7 = dst1
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst1
	str				r7, [r0]				// Store dst1
2:
	add				r0, r0, #4				// dst++
	Load1DSample	r7, r1, r3, r4			// r7 = src2
	and				r12, r14, #255
	mov				r14, r14, lsr #8		// r14 >>= 8
	cmp				r7, #0
	beq				3f						// Skip this sample if alpha == 0
	add				r12, r12, #1			// r12 = mask2 + 1
	Mask			r6, r11, r7, r12		// r6/r11 = masked src2
	ldr				r7, [r0]				// r7 = dst2
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst2
	str				r7, [r0]				// Store dst2
3:
	Load1DSample	r7, r1, r3, r4			// r7 = src3
	add				r0, r0, #4				// dst++
	cmp				r7, #0
	add				r12, r14, #1			// r12 = mask3 + 1
	beq				4f						// Skip this sample if alpha == 0
	Mask			r6, r11, r7, r12		// r6/r11 = masked src3
	ldr				r7, [r0]				// r7 = dst3
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst3
	str				r7, [r0]				// Store dst3
4:
	add				r0, r0, #4				// dst++

	subs			r2, r2, #4
	bcs				snxcom_main_loop

	b				snxcom_tail


snxcom_mask0_loop:
	ldr				r14, [r9], #4			// r12 = mask3..mask0

	Load1DSample	r7, r1, r3, r4			// r7 = src0

	cmp				r14, #0x00000000
	bne				snxcom_switch_to_main	// Back to normal operation if mask is not 0

snxcom_switch_to_mask0:
	add				r3, r3, r4				// csx += cdx
	add				r0, r0, #4*4			// dst += 4
	add				r3, r3, r4, lsl #1		// csx += cdx * 2
	subs			r2, r2, #4
	bcs				snxcom_mask0_loop

	b				snxcom_tail


snxcom_mask1_loop:
	// Preload data
	pld				[r0, #32*4]
	pld				[r9, #32]

	ldr				r14, [r9], #4			// r12 = mask3..mask0

	Load1DSample	r7, r1, r3, r4			// r7 = src0

	cmp				r14, #0xffffffff
	bne				snxcom_switch_to_main	// Back to normal operation if mask is not 255

snxcom_switch_to_mask1:
	cmp				r7, #0
	beq				1f						// Skip this sample if alpha == 0
	ldr				r8, [r0]				// r8 = dst0
	CompOver		r8, r7					// Composite over
	str				r8, [r0]				// Store dst0
1:
	add				r0, r0, #4				// dst++
	Load1DSample	r7, r1, r3, r4			// r7 = src1
	cmp				r7, #0
	beq				2f						// Skip this sample if alpha == 0
	ldr				r8, [r0]				// r8 = dst1
	CompOver		r8, r7					// Composite over
	str				r8, [r0]				// Store dst1
2:
	add				r0, r0, #4				// dst++
	Load1DSample	r7, r1, r3, r4			// r7 = src2
	cmp				r7, #0
	beq				3f						// Skip this sample if alpha == 0
	ldr				r8, [r0]				// r8 = dst2
	CompOver		r8, r7					// Composite over
	str				r8, [r0]				// Store dst2
3:
	add				r0, r0, #4				// dst++
	Load1DSample	r7, r1, r3, r4			// r7 = src3
	cmp				r7, #0
	beq				4f						// Skip this sample if alpha == 0
	ldr				r8, [r0]				// r8 = dst3
	CompOver		r8, r7					// Composite over
	str				r8, [r0]				// Store dst3
4:
	add				r0, r0, #4				// dst++

	subs			r2, r2, #4
	bcs				snxcom_mask1_loop


snxcom_tail:
	adds			r2, r2, #4
	beq				snxcom_done

snxcom_tail_loop:
	// Load mask and source
	ldrb			r12, [r9], #1			// r12 = mask0
	Load1DSample	r7, r1, r3, r4			// r7  = src0
	add				r12, r12, #1			// r12 = mask0 + 1

	Mask			r6, r11, r7, r12		// r6/r11 = masked src0
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0], #4			// Store dst0

	subs			r2, r2, #1
	bne				snxcom_tail_loop


snxcom_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestX_CompOverConstMask_ARMv6(void* dst, const void* src, unsigned int cnt, INT32 csx, INT32 cdx, UINT32 opacity);
//  r0 = dst
//  r1 = src
//  r2 = opacity
//  r3 = cnt
//  [sp] = csx
//  [sp+4] = cdx
FUNC_PROLOGUE_ARM Sampler_NearestX_CompOverConstMask_ARMv6
	push			{r4 - r11, r14}

	ldr				r4, [sp, #36]			// r4 = csx
	ldr				r6, [sp, #40]			// r6 = cdx

	cmp				r2, #0
	beq				snxcocm_done			// Do nothing if opacity = 0
	cmp				r2, #254
	bls				snxcocm_normal

	// If opacity >= 255, do simple CompOver instead
	push			{lr}
	push			{r6}
	mov				r2, r3
	mov				r3, r4
	bl				Sampler_NearestX_CompOver_ARMv6
	add				sp, sp, #4
	pop				{lr}
	b				snxcocm_done

snxcocm_normal:
	add				r2, r2, #1				// Mask = opacity + 1

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Align destination to a cache line boundary
	cmp				r3, #0
snxcocm_head_loop:
	beq				snxcocm_done			// If cnt == 0...
	ands			r12, r0, #15			// Are we aligned yet?
	beq				snxcocm_aligned

	// Sample source and increment source coordinate
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r8, r9, r7, r2			// r8/r9 = masked src0

	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r8, r9, r7			// dst0 = src0 + (256 - src0.a) * dst0
	str				r7, [r0], #4			// *dst++ = dst0

	subs			r3, r3, #1
	b				snxcocm_head_loop

snxcocm_aligned:
	subs			r3, r3, #4
	bcc				snxcocm_tail


snxcocm_main_loop:
	// Preload data
	pld				[r1, #16*4]
	pld				[r0, #16*4]

	// Pass 1 - two samples
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r8, r9, r7, r2			// r8/r9 = masked src0
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r11, r14, r7, r2		// r11/r14 = masked src1
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r8, r9, r7			// dst0 = src0 + (256 - src0.a) * dst0
	ldr				r8, [r0, #4]			// r7 = dst1
	CompOverMasked	r8, r11, r14, r8		// dst1 = src1 + (256 - src1.a) * dst1
	stmia			r0!, {r7, r8}			// Store dst0-dst1


	// Pass 2 - two samples
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r8, r9, r7, r2			// r8/r9 = masked src0
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r11, r14, r7, r2		// r11/r14 = masked src1
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r8, r9, r7			// dst0 = src0 + (256 - src0.a) * dst0
	ldr				r8, [r0, #4]			// r7 = dst1
	CompOverMasked	r8, r11, r14, r8		// dst1 = src1 + (256 - src1.a) * dst1
	stmia			r0!, {r7, r8}			// Store dst0-dst1

	subs			r3, r3, #4
	bcs				snxcocm_main_loop


snxcocm_tail:
	adds			r3, r3, #4
	beq				snxcocm_done

snxcocm_tail_loop:
	// Sample source and increment source coordinate
	Load1DSample	r7, r1, r4, r6			// r7 = src[(csx >> 12) * 4]
	Mask			r8, r9, r7, r2			// r8/r9 = masked src0

	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r8, r9, r7			// dst0 = src0 + (256 - src0.a) * dst0
	str				r7, [r0], #4			// *dst++ = dst0

	subs			r3, r3, #1
	bne				snxcocm_tail_loop


snxcocm_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestXY_CompOver_ARMv6(void* dst, const void* src, unsigned int cnt, unsigned int stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
//  r0 = dst
//  r1 = src
//  r2 = cnt
//  r3 = stride
//  [sp] = csx
//  [sp+4] = cdx
//  [sp+8] = csy
//  [sp+12] = cdy
FUNC_PROLOGUE_ARM Sampler_NearestXY_CompOver_ARMv6
	push			{r4 - r11, r14}

	cmp				r2, #0
	beq				snxyco_done

	ldr				r4, [sp, #36]						// r4 = csx
	ldr				r7, [sp, #44]						// r7 = csy
	ldr				r8, [sp, #48]						// r8 = cdy

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16				// r10 = 0x00ff00ff

	subs			r2, r2, #4
	bcc				snxyco_tail

	mov				r14, #0x80000000					// Force blended operation on first run

snxyco_main_loop:
	// Should we try to switch to transparent/opaque operation?
	movs			r14, r14, lsr #24
	beq				snxyco_switch_to_transparent
	cmp				r14, #0xff
	beq				snxyco_switch_to_opaque

snxyco_blend_loop_entry:
	// Preload destination data
	pld				[r0, #16*4]

	// Pixel 0
	ldr				r5, [sp, #40]						// r5  = cdx (clobbered by CompOver)
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src0 (and increment source coordinates)
	ldr				r9, [r0]							// r9  = dst0
	CompOver		r9, r14								// r9  = src0 + (256 - src0.a) * dst0
	str				r9, [r0], #4						// Store to destination (and increment destination pointer)

	// Pixel 1
	ldr				r5, [sp, #40]						// r5  = cdx (clobbered by CompOver)
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src1 (and increment source coordinates)
	ldr				r9, [r0]							// r9  = dst1
	CompOver		r9, r14								// r9  = src1 + (256 - src1.a) * dst1
	str				r9, [r0], #4						// Store to destination (and increment destination pointer)

	// Pixel 2
	ldr				r5, [sp, #40]						// r5  = cdx (clobbered by CompOver)
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src2 (and increment source coordinates)
	ldr				r9, [r0]							// r9  = dst2
	CompOver		r9, r14								// r9  = src2 + (256 - src2.a) * dst2
	str				r9, [r0], #4						// Store to destination (and increment destination pointer)

	// Pixel 3
	ldr				r5, [sp, #40]						// r5  = cdx (clobbered by CompOver)
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src3 (and increment source coordinates)
	ldr				r9, [r0]							// r9  = dst3
	CompOver		r9, r14								// r9  = src3 + (256 - src3.a) * dst3
	str				r9, [r0], #4						// Store to destination (and increment destination pointer)

	subs			r2, r2, #4
	bcs				snxyco_main_loop
	b				snxyco_tail


snxyco_switch_to_blend:
	sub				r4, r4, r5, asl #2					// csx -= 4*cdx
	sub				r7, r7, r8, asl #2					// csy -= 4*cdy
	b				snxyco_blend_loop_entry


snxyco_switch_to_opaque:
	ldr				r5, [sp, #40]						// r5 = cdx

snxyco_opaque_loop:
	// Sample source and increment source coordinates (x4)
	Load2DSample	r6, r1, r3, r4, r7, r5, r8, r12		// r6 = src0
	Load2DSample	r9, r1, r3, r4, r7, r5, r8, r12		// r9 = src1
	Load2DSample	r11, r1, r3, r4, r7, r5, r8, r12	// r11 = src2
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src3

	// Check that we are still fully opaque
	and				r12, r6, r9
	and				r12, r12, r11
	and				r12, r12, r14
	adds			r12, r12, #0x01000000				// If alpha != 0xff, carry is 0 after this operation
	bcc				snxyco_switch_to_blend

	// Write to destination
	stmia			r0!, {r6, r9, r11, r14}

	subs			r2, r2, #4
	bcs				snxyco_opaque_loop
	b				snxyco_tail


snxyco_switch_to_transparent:
	ldr				r5, [sp, #40]						// r5 = cdx

snxyco_transparent_loop:
	// Sample source and increment source coordinates (x4)
	Load2DSample	r6, r1, r3, r4, r7, r5, r8, r12		// r6 = src0
	Load2DSample	r9, r1, r3, r4, r7, r5, r8, r12		// r9 = src1
	Load2DSample	r11, r1, r3, r4, r7, r5, r8, r12	// r11 = src2
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src3

	// Check that we are still fully transparent
	orr				r6, r6, r9
	orr				r11, r11, r14
	orrs			r6, r6, r11
	bne				snxyco_switch_to_blend

	add				r0, r0, #4*4						// dst += 4

	subs			r2, r2, #4
	bcs				snxyco_transparent_loop


snxyco_tail:
	adds			r2, r2, #4
	beq				snxyco_done

snxyco_tail_loop:
	ldr				r5, [sp, #40]						// r5  = cdx (clobbered by CompOver)
	Load2DSample	r14, r1, r3, r4, r7, r5, r8, r12	// r14 = src0 (and increment source coordinates)
	ldr				r9, [r0]							// r9  = dst0
	CompOver		r9, r14								// r9  = src0 + (256 - src0.a) * dst0
	str				r9, [r0], #4						// Store to destination (and increment destination pointer)

	subs			r2, r2, #1
	bne				snxyco_tail_loop

snxyco_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void Sampler_NearestXY_CompOverMask_ARMv6(void* dst, const void* src, const UINT8* alpha, unsigned int cnt, unsigned int stride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
//  r0 = dst
//  r1 = src
//  r2 = alpha
//  r3 = cnt
//  [sp] = stride
//  [sp+4] = csx
//  [sp+8] = cdx
//  [sp+12] = csy
//  [sp+16] = cdy
FUNC_PROLOGUE_ARM Sampler_NearestXY_CompOverMask_ARMv6
	push			{r4 - r11, r14}

	mov				r9, r2					// r9 = alpha
	movs			r2, r3					// r2 = cnt
	beq				snxycom_done

	ldr				r3, [sp, #36]			// r3 = stride
	ldr				r4, [sp, #40]			// r4 = csx
	ldr				r7, [sp, #48]			// r7 = csy

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Load cdx/cdy - needs reloading in the loop due to clobbering
	ldr				r5, [sp, #44]			// r5 = cdx
	ldr				r6, [sp, #52]			// r6 = cdy

snxycom_main_loop:
	// Load mask
	ldrb			r12, [r9], #1			// r12 = mask

	// Special fast case
	cmp				r12, #0
	beq				snxycom_next

	// Sample source
	mov				r14, r7, asr #12		// r14 = csy >> 12
	mov				r5, r4, asr #12			// r5 = csx >> 12
	mla				r14, r14, r3, r5		// r14 = (csy >> 12) * stride + (csx >> 12)
	add				r12, r12, #1			// r12 = mask + 1
	ldr				r14, [r1, r14, lsl #2]	// r14 = src[((csx >> 12) + (csy >> 12) * stride) * 4]

	// Apply mask to source
	Mask			r6, r11, r14, r12		// r6/r11 = masked src

	// Load destination
	ldr				r14, [r0]

	// Composite over
	CompOverMasked	r14, r6, r11, r14

	// Reload clobbered cdx/cdy
	ldr				r5, [sp, #44]			// r5 = cdx
	ldr				r6, [sp, #52]			// r6 = cdy

	// Store destination
	str				r14, [r0]

snxycom_next:
	add				r0, r0, #4				// dst++
	subs			r2, r2, #1				// cnt--
	add				r4, r4, r5				// csx += cdx
	add				r7, r7, r6				// csy += cdy
	bne				snxycom_main_loop

snxycom_done:
	pop			{r4 - r11, r14}
FUNC_EPILOGUE


#endif // ARCHITECTURE_ARM
