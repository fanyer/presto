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

.arch armv6			// Support ARMv6 instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text


// void CompOver_ARMv6(void* dst, const void* src, unsigned int len);
//  r0 = dst
//  r1 = src
//  r2 = len
FUNC_PROLOGUE_ARM CompOver_ARMv6
	push			{r4 - r11, r14}

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Align destination to a cache line boundary
	cmp				r2, #0
co_headloop:
	beq				co_done					// If cnt == 0...
	ands			r12, r0, #31			// Are we aligned yet?
	beq				co_aligned

	ldr				r3, [r1], #4			// r3 = *src++
	ldr				r4, [r0]				// r4 = *dst
	CompOver		r4, r3					// dst0 = src0 + (256 - src0.a) * dst0
	subs			r2, r2, #1				// len--
	str				r4, [r0], #4			// *dst++ = r4

	b				co_headloop

co_aligned:
	subs			r2, r2, #8
	bcc				co_tail

	// Load one pixel into r3 (for checking its alpha value)
	ldr				r3, [r1]

co_blend_loop:
	// Should we try to switch to transparent/opaque operation?
	movs			r3, r3, lsr #24
	beq				co_switch_to_transparent
	cmp				r3, #0xff
	beq				co_switch_to_opaque


co_switch_to_blend:
	// Preload source & destination data
	pld				[r0, #32*4]				// Preload-distance chosen empirically
	pld				[r1, #32*4]

	// Un-rolled, so that we process 8 pixels per loop
	ldmia			r1!, {r3, r4, r7}		// Load src0 - src2
	ldm				r0, {r8, r9, r14}		// Load dst0 - dst2
	CompOver		r8, r3					// dst0 = src0 + (256 - src0.a) * dst0
	CompOver		r9, r4					// dst1 = src1 + (256 - src1.a) * dst1
	CompOver		r14, r7					// dst2 = src2 + (256 - src2.a) * dst2
	stmia			r0!, {r8, r9, r14}		// Store dst0 - dst2

	ldmia			r1!, {r3, r4, r7}		// Load src3 - src5
	ldm				r0, {r8, r9, r14}		// Load dst3 - dst5
	CompOver		r8, r3					// dst3 = src3 + (256 - src3.a) * dst3
	CompOver		r9, r4					// dst4 = src4 + (256 - src4.a) * dst4
	CompOver		r14, r7					// dst5 = src5 + (256 - src5.a) * dst5
	stmia			r0!, {r8, r9, r14}		// Store dst3 - dst5

	ldmia			r1!, {r3, r4}			// Load src6 - src7
	ldm				r0, {r7, r8}			// Load dst6 - dst7
	CompOver		r7, r3					// dst6 = src6 + (256 - src6.a) * dst6
	CompOver		r8, r4					// dst7 = src7 + (256 - src7.a) * dst7
	stmia			r0!, {r7, r8}			// Store dst6 - dst7

	subs			r2, r2, #8				// len -= 8
	bcs				co_blend_loop

	b				co_tail


co_switch_to_opaque:

co_opaque_loop:
	// Preload source data
	pld				[r1, #32*4]				// Preload-distance chosen empirically

	ldm				r1, {r4-r9,r11,r14}		// Load src0..src7

	// Make sure that we are still fully opaque
	and				r3, r4, r5
	and				r12, r6, r7
	and				r3, r3, r8
	and				r12, r12, r9
	and				r3, r3, r11
	and				r12, r12, r14
	and				r3, r3, #0xff000000
	and				r3, r3, r12
	cmp				r3, #0xff000000
	bne				co_switch_to_blend		// If alpha != 0xff, switch to blending

	stmia			r0!, {r4-r9,r11,r14}	// Store src0..src7 to destination

	subs			r2, r2, #8				// len -= 8
	add				r1, r1, #8*4			// src += 8

	bcs				co_opaque_loop			// while (len > 0)

	b				co_tail


	.align			6						// Align code to a cache-line boundary

co_switch_to_transparent:

co_transparent_loop:
	// Preload source data
	pld				[r1, #32*4]				// Preload-distance chosen empirically

	ldm				r1, {r4-r9,r11,r14}

	// Make sure that we are still fully transparent
	// Note: we only have to check if all pixles are exactly zero, since we
	// use premultiplied alpha (a == 0 -> r,g,b = 0,0,0)
	orr				r4, r4, r5
	orr				r5, r6, r7
	orr				r6, r8, r9
	orr				r7, r11, r14
	orr				r4, r4, r5
	orr				r5, r6, r7
	orrs			r4, r4, r5
	bne				co_switch_to_blend		// If alpha != 0x00, switch to blending

	subs			r2, r2, #8				// len -= 8
	add				r1, r1, #8*4			// src += 8
	add				r0, r0, #8*4			// dst += 8

	bcs				co_transparent_loop


co_tail:
	adds			r2, r2, #8
	beq				co_done

co_tailloop:
	ldr				r3, [r1], #4			// r3 = *src++
	ldr				r4, [r0]				// r4 = *dst
	CompOver		r4, r3					// dst0 = src0 + (256 - src0.a) * dst0
	subs			r2, r2, #1				// len--
	str				r4, [r0], #4			// *dst++ = r4
	bne				co_tailloop

co_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


// void CompConstOver_ARMv6(void* dst, UINT32 src, unsigned int len);
// r0 = dst
// r1 = src
// r2 = len
FUNC_PROLOGUE_ARM CompConstOver_ARMv6
	push			{r4 - r11, r14}

	// Fast cases
	movs			r3, r1, lsr #24
	beq				cco_done				// Nothing to do if alpha = 0
	cmp				r3, #255
	beq				cco_fill				// Pure fill if alpha = 255

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	subs			r2, r2, #3				// len -=3 (prepare un-rolled loop)
	bcc				cco_tail

	// Pre-process constant values from the source color (for the unrolled
	// CompOver operations below)
	and				r9, r1, r10				// r9  = src.rb  (..RR..BB)
	bic				r7, r1, r10				// r7  = src.ag  (AA..GG..)
	mov				r5, r1, lsr #24
	rsb				r5, r5, #256			// r5  = 256 - src.a
	mov				r9, r9, asl #8			// r9  = src.rb  (RR..BB..)

cco_main_loop:
	// Preload data
	pld				[r0, #32*4]

	// Load destination (three samples, dst0-dst2)
	ldm				r0, {r3, r4, r14}

	// Composite over, src + (256 - src.a) * dst
//	CompOver		r1, r3
//	CompOver		r1, r4
//	CompOver		r1, r14

	// Optimized/interleaved variant of the above
	bic				r12, r3, r10			// r12 = dst0.ag  (AA..GG..)
	and				r8, r3, r10				// r8  = dst0.rb  (..RR..BB)
	mla				r6, r8, r5, r9			// r6  = dst0.rb * (256-src.a) + src.rb
	mov				r12, r12, lsr #8		// r12 = dst0.ag  (..AA..GG)
	mla				r11, r12, r5, r7		// r11 = dst0.ag * (256-src.a) + src.ag
	bic				r12, r4, r10			// r12 = dst1.ag  (AA..GG..)
	and				r8, r4, r10				// r8  = dst1.rb  (..RR..BB)
	and				r3, r10, r6, lsr #8
	mov				r12, r12, lsr #8		// r12 = dst1.ag  (..AA..GG)
	bic				r4, r11, r10
	mla				r6, r8, r5, r9			// r6  = dst1.rb * (256-src.a) + src.rb
	mla				r11, r12, r5, r7		// r11 = dst1.ag * (256-src.a) + src.ag
	bic				r12, r14, r10			// r12 = dst2.ag  (AA..GG..)
	and				r8, r14, r10			// r8  = dst2.rb  (..RR..BB)
	orr				r3, r3, r4				// dst0 = combined result
	mov				r12, r12, lsr #8		// r12 = dst2.ag  (..AA..GG)
	and				r6, r10, r6, lsr #8
	bic				r11, r11, r10
	orr				r4, r6, r11				// dst1 = combined result
	mla				r14, r8, r5, r9			// r14  = dst2.rb * (256-src.a) + src.rb
	mla				r11, r12, r5, r7		// r11 = dst2.ag * (256-src.a) + src.ag
	and				r14, r10, r14, lsr #8
	bic				r11, r11, r10
	orr				r14, r14, r11			// dst2 = combined result

	// Store destination
	stmia			r0!, {r3, r4, r14}

	// Next iteration
	subs			r2, r2, #3				// len -= 3
	bcs				cco_main_loop

cco_tail:
	adds			r2, r2, #3
	beq				cco_done

cco_tail_loop:
	ldr				r4, [r0]				// dst0 = *dst
	CompOver		r4, r1					// dst0 = src + (256 - src.a) * dst0
	str				r4, [r0], #4			// *dst++ = dst0
	subs			r2, r2, #1				// len--
	bne				cco_tail_loop

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
	mov				r9, r1

cco_fill_loop:
	stmia			r0!, {r1, r3 - r9}		// Store to destination
	subs			r2, r2, #8				// len -= 8
	bcs				cco_fill_loop


cco_fill_tail:
	adds			r2, r2, #8
	beq				cco_done

cco_fill_tail_loop:
	str				r1, [r0], #4			// Store to destination
	subs			r2, r2, #1				// len--
	bne				cco_fill_tail_loop


cco_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


//void CompConstOverMask_ARMv6(void* dst, unsigned int src, const UINT8* mask, unsigned int len);
// r0 = dst
// r1 = src
// r2 = mask
// r3 = len
FUNC_PROLOGUE_ARM CompConstOverMask_ARMv6
	push			{r4 - r11, r14}

	cmp				r1, #0
	beq				ccom_done				// Do nothing if alpha == 0

	mov				r9, r2					// r9 = mask
	mov				r2, r3					// r2 = len

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	// Align mask to a word boundary
	cmp				r2, #0
ccom_head_loop:
	beq				ccom_done
	ands			r12, r9, #3				// Are we aligned yet?
	beq				ccom_aligned

	ldrb			r12, [r9], #1			// r12 = mask0
	cmp				r12, #0
	beq				1f
	add				r12, r12, #1			// r12 = mask0 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0]				// Store dst0
1:
	add				r0, r0, #4				// dst++
	subs			r2, r2, #1
	b				ccom_head_loop

ccom_aligned:
	subs			r2, r2, #4
	bcc				ccom_tail

ccom_main_loop:
	// Preload data
	pld				[r0, #16*4]
	pld				[r9, #16]

	ldr				r14, [r9], #4			// r12 = mask3..mask0

ccom_switch_to_main:
	cmp				r14, #0x00000000
	beq				ccom_switch_to_mask0	// Switch to zero-mask mode?
	cmp				r14, #0xffffffff
	beq				ccom_switch_to_mask1	// Switch to one-mask mode?

	and				r12, r14, #255			// r12 = mask0
	mov				r14, r14, lsr #8		// r14 >>= 8
	add				r12, r12, #1			// r12 = mask0 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src0
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0], #4			// Store dst0

	and				r12, r14, #255			// r12 = mask1
	mov				r14, r14, lsr #8		// r14 >>= 8
	add				r12, r12, #1			// r12 = mask1 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src1
	ldr				r7, [r0]				// r7 = dst1
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst1
	str				r7, [r0], #4			// Store dst1

	and				r12, r14, #255			// r12 = mask2
	mov				r14, r14, lsr #8		// r14 >>= 8
	add				r12, r12, #1			// r12 = mask2 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src2
	ldr				r7, [r0]				// r7 = dst2
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst2
	str				r7, [r0], #4			// Store dst2

	add				r12, r14, #1			// r12 = mask3 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src3
	ldr				r7, [r0]				// r7 = dst3
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst3
	str				r7, [r0], #4			// Store dst3

	subs			r2, r2, #4
	bcs				ccom_main_loop

	b				ccom_tail


ccom_mask0_loop:
	ldr				r14, [r9], #4			// r12 = mask3..mask0

	cmp				r14, #0x00000000
	bne				ccom_switch_to_main		// Back to normal operation if mask is not 0

ccom_switch_to_mask0:
	add				r0, r0, #4*4			// dst += 4
	subs			r2, r2, #4
	bcs				ccom_mask0_loop

	b				ccom_tail


ccom_mask1_loop:
	// Preload data
	pld				[r0, #32*4]
	pld				[r9, #32]

	ldr				r14, [r9], #4			// r12 = mask3..mask0

	cmp				r14, #0xffffffff
	bne				ccom_switch_to_main		// Back to normal operation if mask is not 255

ccom_switch_to_mask1:
	ldm				r0, {r4,r7,r8,r14}		// Load dst0..dst3
	CompOver		r4, r1					// dst0 = src + (256 - src.a) * dst0
	CompOver		r7, r1					// dst1 = src + (256 - src.a) * dst1
	CompOver		r8, r1					// dst2 = src + (256 - src.a) * dst2
	CompOver		r14, r1					// dst3 = src + (256 - src.a) * dst3
	stmia			r0!, {r4,r7,r8,r14}		// Store dst0..dst3

	subs			r2, r2, #4
	bcs				ccom_mask1_loop


ccom_tail:
	adds			r2, r2, #4
	beq				ccom_done

ccom_tail_loop:
	ldrb			r12, [r9], #1			// r12 = mask0

	cmp				r12, #0
	beq				1f						// Skip this sample if alpha == 0
	add				r12, r12, #1			// r12 = mask0 + 1
	Mask			r6, r11, r1, r12		// r6/r11 = masked src0
	ldr				r7, [r0]				// r7 = dst0
	CompOverMasked	r7, r6, r11, r7			// r7 = composited dst0
	str				r7, [r0]				// Store dst0
1:
	add				r0, r0, #4				// dst++

	subs			r2, r2, #1
	bne				ccom_tail_loop

ccom_done:
	pop				{r4 - r11, r14}
FUNC_EPILOGUE


//void CompOverConstMask_ARMv6(void* dst, const void* src, unsigned int mask, unsigned int len);
// r0 = dst
// r1 = src
// r2 = mask (1-256)
// r3 = len
FUNC_PROLOGUE_ARM CompOverConstMask_ARMv6
	// Do nothing if mask <= 1 (i.e. zero)
	cmp				r2, #1
	ble				cocm_exit

	// Plain composite over if mask >= 256
	cmp				r2, #256
	blt				cocm_normal
	mov				r2, r3					// r2 = len
	push			{lr}
	bl				CompOver_ARMv6
	pop				{lr}
	b				cocm_exit

cocm_normal:
	push			{r4 - r11, r14}

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	subs			r14, r3, #8				// len -= 8
	bcc				cocm_tail

cocm_main_loop:
	// Preload data
	pld				[r1, #32*4]
	pld				[r0, #32*4]

	ldmia			r1!, {r3, r4}			// Load src0..src1
	ldm				r0, {r8, r9}			// Load dst0..dst1
	Mask			r6, r7, r3, r2			// r6/r7 = masked src0
	CompOverMasked	r8, r6, r7, r8			// dst0 = src0 + (256 - src0.a) * dst0
	Mask			r6, r7, r4, r2			// r6/r7 = masked src1
	CompOverMasked	r9, r6, r7, r9			// dst1 = src1 + (256 - src1.a) * dst1
	stmia			r0!, {r8, r9}			// Store dst0..dst1

	ldmia			r1!, {r3, r4}			// Load src2..src3
	ldm				r0, {r8, r9}			// Load dst2..dst3
	Mask			r6, r7, r3, r2			// r6/r7 = masked src2
	CompOverMasked	r8, r6, r7, r8			// dst2 = src2 + (256 - src2.a) * dst2
	Mask			r6, r7, r4, r2			// r6/r7 = masked src3
	CompOverMasked	r9, r6, r7, r9			// dst3 = src3 + (256 - src3.a) * dst3
	stmia			r0!, {r8, r9}			// Store dst2..dst3

	ldmia			r1!, {r3, r4}			// Load src4..src5
	ldm				r0, {r8, r9}			// Load dst4..dst5
	Mask			r6, r7, r3, r2			// r6/r7 = masked src4
	CompOverMasked	r8, r6, r7, r8			// dst4 = src4 + (256 - src4.a) * dst4
	Mask			r6, r7, r4, r2			// r6/r7 = masked src5
	CompOverMasked	r9, r6, r7, r9			// dst5 = src5 + (256 - src5.a) * dst5
	stmia			r0!, {r8, r9}			// Store dst4..dst5

	ldmia			r1!, {r3, r4}			// Load src6..src7
	ldm				r0, {r8, r9}			// Load dst6..dst7
	Mask			r6, r7, r3, r2			// r6/r7 = masked src6
	CompOverMasked	r8, r6, r7, r8			// dst6 = src6 + (256 - src6.a) * dst6
	Mask			r6, r7, r4, r2			// r6/r7 = masked src7
	CompOverMasked	r9, r6, r7, r9			// dst7 = src7 + (256 - src7.a) * dst7
	stmia			r0!, {r8, r9}			// Store dst6..dst7

	subs			r14, r14, #8
	bcs				cocm_main_loop


cocm_tail:
	adds			r14, r14, #8
	beq				cocm_done

cocm_tail_loop:
	ldr				r3, [r1], #4			// r3 = src0
	Mask			r6, r7, r3, r2			// r6/r7 = masked src0
	ldr				r4, [r0]				// r4 = dst0
	CompOverMasked	r3, r6, r7, r4			// r4 = src0 + (256 - src0.a) * dst0
	str				r3, [r0], #4			// *dst++ = r4
	subs			r14, r14, #1
	bne				cocm_tail_loop


cocm_done:
	pop				{r4 - r11, r14}

cocm_exit:
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM
