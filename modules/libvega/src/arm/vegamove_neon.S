/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM_NEON)

.arch armv7-a		// Support ARMv7 instructions
.fpu neon			// Support NEON instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text


// void MoveFrom_NEON(void* dst, const void* src, unsigned int len);
//  r0 = dst
//  r1 = src
//  r2 = len
FUNC_PROLOGUE_ARM MoveFrom_NEON
	// Calculate movement delta
	sub			r3, r0, r1
	mov			r3, r3, asr #2			// r3 = delta = dst - src

	// Nothing to do if src == dst
	cmp			r0, r1
	beq			mf_done

	// Is the data non-overlapping?
	cmp			r3, #0					// delta <= 0 ?
	ble			mf_no_overlap
	cmp			r3, r2					// delta >= len ?
	bge			mf_no_overlap
	b			mf_overlap				// We must handle overlap :(


	//------------------------------------------------------------------------
	// We don't have to care about overlapping data. Hence we can use a fast
	// copy operation. NOTE: This is essentially memcpy.
	//------------------------------------------------------------------------

mf_no_overlap:
	// Early pre-load - try to avoid being stalled in the start-up
	pld         [r1]
	pld         [r1, #16*4]
	pld         [r1, #32*4]

	// Fall back to simple word-by-word copy for short runs.
	// NOTE: If len < 8, we MUST do this (since the alignment logic does
	// not handle cases where num_unaligned > len), but we MUST NOT use
	// the fallback if len > 15 (since it does not handle runs longer than
	// that).
	cmp			r2, #15
	bls			mf_copy_tail_loop

	// Align destination to a cache line boundary
	and			r12, r0, #31
	mov			r12, r12, lsr #2		// r12 = (src & 31) / 4
	rsb			r12, r12, #8			// r12 = num_unaligned (1-8)

	// Unrolled head-loop
	adr			r3, 1f
	ldr			pc, [r3, r12, lsl #2]
1:	.word		mf_copy_aligned			// (r12 == 0, will never happen)
	.word		1f
	.word		2f
	.word		3f
	.word		4f
	.word		5f
	.word		6f
	.word		7f
	.word		mf_copy_aligned			// (r12 == 8 => already aligned)
7:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
6:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
5:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
4:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
3:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
2:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
1:	ldr			r3, [r1], #4			// r3 = *src++
	sub			r2, r2, r12				// len -= num_unaligned
	str			r3, [r0], #4			// *dst++ = r3

mf_copy_aligned:
	subs		r2, r2, #16
	bcc			mf_copy_tail

	// Pre-load even more cache lines
	pld         [r1, #16*4]
	pld         [r1, #32*4]
	pld         [r1, #48*4]

mf_copy_inner_loop:
	vld1.8		{d0 - d3}, [r1]!		// Load 16 source pixels
	vld1.8		{d4 - d7}, [r1]!
	pld			[r1, #64*4]				// Pre-load source data
	subs        r2, r2, #16				// len -= 16
	vst1.8		{d0 - d3}, [r0,:128]!	// Store 16 destination pixels
	vst1.8		{d4 - d7}, [r0,:128]!
	bcs         mf_copy_inner_loop


mf_copy_tail:
	add			r2, r2, #16

mf_copy_tail_loop:
	// Unrolled tail-loop (r2 = 0..15)
	adr			r3, 1f
	ldr			pc, [r3, r2, lsl #2]
1:	.word		mf_done					// r2 == 0 => we're done
	.word		1f
	.word		2f
	.word		3f
	.word		4f
	.word		5f
	.word		6f
	.word		7f
	.word		8f
	.word		9f
	.word		10f
	.word		11f
	.word		12f
	.word		13f
	.word		14f
	.word		15f
15:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
14:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
13:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
12:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
11:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
10:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
9:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
8:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
7:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
6:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
5:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
4:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
3:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
2:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
1:	ldr			r3, [r1], #4			// r3 = *src++
	str			r3, [r0], #4			// *dst++ = r3
	b			mf_done



	//------------------------------------------------------------------------
	// Handle overlapping data.
	//------------------------------------------------------------------------

mf_overlap:
	// If delta >= 5, use the default backwards loop
	cmp			r3, #5
	bge			mf_backwards


	//------------------------------------------------------------------------
	// Forward overlapping operation, when we have overlapping data where
	// src and dst are close enough to use the CPU registers as an
	// intermediate buffer.
	//------------------------------------------------------------------------

	// Handle tail first (avoid overlap problems)
	push		{r0, r1, r4}
	sub			r12, r2, #1
	add			r1, r1, r12, lsl #2		// src += len - 1
	add			r0, r0, r12, lsl #2		// dst += len - 1
	ands		r4, r2, #15				// r4 = len & 15 (i.e. tail length)
	beq			mf_fw_tail_done
mf_fw_tail_loop:
	ldr			r12, [r1], #-4			// r12 = *src--
	str			r12, [r0], #-4			// *dst-- = r12
	subs		r4, r4, #1
	bne			mf_fw_tail_loop

mf_fw_tail_done:
	pop			{r0, r1, r4}
	movs		r2, r2, lsr #4			// loopCount = len / 16
	beq			mf_done

	// Select move loop based on movement delta
	// switch(delta) {...}
	adr			r12, 1f
	ldr			pc, [r12, r3, lsl #2]
1:	.word		mf_done					// delta == 0 (never used)
	.word		mf_overlap1				// delta == 1
	.word		mf_overlap2				// delta == 2
	.word		mf_overlap3				// delta == 3
	.word		mf_overlap4				// delta == 4


	// The overlap distance is exactly 1
mf_overlap1:
	push		{r4 - r10}
	mov			r0, r1					// Align src and dst
	ldr			r12, [r1]				// src15' = first pixel
mf_overlap1_loop:
	pld			[r1, #32*4]				// Preload source data

	ldmia		r1!, {r3 - r10}			// Load src0..src7
	str			r12, [r0], #4			// Store src15'
	mov			r12, r10				// src7' = src7
	stmia		r0!, {r3 - r9}			// Store src0..src6

	ldmia		r1!, {r3 - r10}			// Load src8..src15
	str			r12, [r0], #4			// Store src7'
	mov			r12, r10				// src15' = src15
	stmia		r0!, {r3 - r9}			// Store src8..src14

	subs		r2, r2, #1				// loopCount--
	bne			mf_overlap1_loop

	str			r12, [r0], #4			// Store src15'

	pop			{r4 - r10}
	b			mf_done


	// The overlap distance is exactly 2
mf_overlap2:
	push		{r4 - r11}
	mov			r0, r1					// Align src and dst
	ldm			r1, {r11, r12}			// src14'..src15' = first two pixels
mf_overlap2_loop:
	pld			[r1, #32*4]				// Preload source data

	ldmia		r1!, {r3 - r10}			// Load src0..src7
	stmia		r0!, {r11, r12}			// Store src14'..src15'
	mov			r11, r9					// src6' = src6
	mov			r12, r10				// src7' = src7
	stmia		r0!, {r3 - r8}			// Store src0..src5

	ldmia		r1!, {r3 - r10}			// Load src8..src15
	stmia		r0!, {r11, r12}			// Store src6'..src7'
	mov			r11, r9					// src14' = src14
	mov			r12, r10				// src15' = src15
	stmia		r0!, {r3 - r8}			// Store src8..src13

	subs		r2, r2, #1				// loopCount--
	bne			mf_overlap2_loop

	stmia		r0!, {r11, r12}			// Store src14'..src15'

	pop			{r4 - r11}
	b			mf_done


	// The overlap distance is exactly 3
mf_overlap3:
	push		{r4 - r11, r14}
	mov			r0, r1					// Align src and dst
	ldm			r1, {r11, r12, r14}		// src13'..src15' = first three pixels
mf_overlap3_loop:
	pld			[r1, #32*4]				// Preload source data

	ldmia		r1!, {r3 - r10}			// Load src0..src7
	stmia		r0!, {r11, r12, r14}	// Store src13'..src15'
	mov			r11, r8					// src5' = src5
	mov			r12, r9					// src6' = src6
	mov			r14, r10				// src7' = src7
	stmia		r0!, {r3 - r7}			// Store src0..src4

	ldmia		r1!, {r3 - r10}			// Load src8..src15
	stmia		r0!, {r11, r12, r14}	// Store src5'..src7'
	mov			r11, r8					// src13' = src13
	mov			r12, r9					// src14' = src14
	mov			r14, r10				// src15' = src15
	stmia		r0!, {r3 - r7}			// Store src8..src12

	subs		r2, r2, #1				// loopCount--
	bne			mf_overlap3_loop

	stmia		r0!, {r11, r12, r14}	// Store src13'..src15'

	pop			{r4 - r11, r14}
	b			mf_done


	// The overlap distance is exactly 4
mf_overlap4:
	push		{r4 - r11, r14}
	mov			r0, r1					// Align src and dst
	ldm			r1, {r1,r11,r12,r14}	// src12'..src15' = first four pixels
mf_overlap4_loop:
	pld			[r0, #32*4]				// Preload source data

	ldm			r0, {r3 - r10}			// Load src0..src7
	stmia		r0!, {r1,r11,r12,r14}	// Store src12'..src15'
	mov			r1, r7					// src4' = src4
	mov			r11, r8					// src5' = src5
	mov			r12, r9					// src6' = src6
	mov			r14, r10				// src7' = src7
	stmia		r0!, {r3 - r6}			// Store src0..src3

	ldm			r0, {r3 - r10}			// Load src8..src15
	stmia		r0!, {r1,r11,r12,r14}	// Store src5'..src7'
	mov			r1, r7					// src12' = src12
	mov			r11, r8					// src13' = src13
	mov			r12, r9					// src14' = src14
	mov			r14, r10				// src15' = src15
	stmia		r0!, {r3 - r6}			// Store src8..src11

	subs		r2, r2, #1				// loopCount--
	bne			mf_overlap4_loop

	stmia		r0!, {r1,r11,r12,r14}	// Store src12'..src15'
	sub			r1, r0, #4*4

	pop			{r4 - r11, r14}
	b			mf_done


	//------------------------------------------------------------------------
	// Generic backwards operation, when we have overlapping data and we can't
	// use one of the fast-cases.
	//------------------------------------------------------------------------

mf_backwards:
	// Prepare for backwards looping
	sub			r12, r2, #1
	add			r1, r1, r12, lsl #2		// src += len - 1
	add			r0, r0, r12, lsl #2		// dst += len - 1

	// Prepare for unrolled operation
	subs		r2, r2, #16
	bcc			mf_bw_tail

	// Adjust src/dst pointers
	sub			r1, r1, #7*4			// src -= 7
	sub			r0, r0, #7*4			// dst -= 7


mf_bw_main_loop:
	pld			[r1, #-64*4]
	sub			r12, r1, #8*4
	vld1.8		{d0 - d3}, [r1]			// Load src0..src7
	sub			r1, r1, #16*4
	vld1.8		{d4 - d7}, [r12]		// Load src8..src15
	subs        r2, r2, #16
	sub			r12, r0, #8*4
	vst1.8		{d0 - d3}, [r0]			// Store src0..src7
	sub			r0, r0, #16*4
	vst1.8		{d4 - d7}, [r12]		// Store src8..src15
	bcs         mf_bw_main_loop


	// Adjust src/dst pointers
	add			r1, r1, #7*4			// src += 7
	add			r0, r0, #7*4			// dst += 7

mf_bw_tail:
	adds		r2, r2, #16
	beq			mf_done

mf_bw_tail_loop:
	ldr			r12, [r1], #-4			// r12 = *src--
	str			r12, [r0], #-4			// *dst-- = r12
	subs		r2, r2, #1
	bne			mf_bw_tail_loop

mf_done:
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM_NEON
