/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM_NEON)

#include "vegafmtconvert_arm.h"

.arch armv7-a		// Support ARMv7 instructions
.fpu neon			// Support NEON instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text

// void Convert_BGRA8888_to_BGRA8888_NEON(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
// NOTE: This is essentially a memcpy
FUNC_PROLOGUE_ARM Convert_BGRA8888_to_BGRA8888_NEON
	// Align source to a cache line boundary
	cmp				r2, #0
bgra_head_loop:
	beq				bgra_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				bgra_aligned
	ldr				r3, [r1], #4				// r3 = *src++ (AARRGGBB)
	subs			r2, #1						// num--
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				bgra_head_loop

bgra_aligned:
	subs			r2, r2, #8
	bcc				bgra_tail

bgra_inner_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Load up source de-interleaved, one color channel per register.
	// Then increment the pointer.
	vld4.u8			{d0 - d3}, [r1,:256]!

	// Conversion would happen here, but this function is rather point-less
	// since there is no conversion, view it as a reference implementation if
	// you will...

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d0 - d3}, [r0]!

	subs			r2, r2, #8
	bcs				bgra_inner_loop

bgra_tail:
	adds			r2, r2, #8
	beq				bgra_done

bgra_tail_loop:
	ldr				r3, [r1], #4				// r3 = *src++ (AARRGGBB)
	subs			r2, #1						// num--
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				bgra_tail_loop

bgra_done:
FUNC_EPILOGUE


// void Convert_RGBA8888_to_BGRA8888_NEON(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_RGBA8888_to_BGRA8888_NEON
	push			{r11}

	// Load constant mask
	mov				r11, #0xff
	orr				r11, r11, r11, asl #16		// r11 = 0x00ff00ff

	// Align source to a cache line boundary
	cmp				r2, #0
rgba_head_loop:
	beq				rgba_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				rgba_aligned
	ldr				r3, [r1], #4				// r3 = *src++ (AABBGGRR)
	ConvRGBA		r3
	subs			r2, #1						// num--
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				rgba_head_loop

rgba_aligned:
	subs			r2, r2, #8
	bcc				rgba_tail

rgba_inner_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Load up source de-interleaved, one color channel per register.
	// Then increment the pointer.
	vld4.u8			{d0 - d3}, [r1,:256]!

	// Swap the Red and Blue channels/registers.
	vswp.u8			d0, d2						// RGBA to BGRA.

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d0 - d3}, [r0]!

	subs			r2, r2, #8
	bcs				rgba_inner_loop

rgba_tail:
	adds			r2, r2, #8
	beq				rgba_done

rgba_tail_loop:
	ldr				r3, [r1], #4				// r3 = *src++ (AABBGGRR)
	subs			r2, #1						// num--
	ConvRGBA		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				rgba_tail_loop

rgba_done:
	pop				{r11}
FUNC_EPILOGUE


// void Convert_ABGR8888_to_BGRA8888_NEON(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ABGR8888_to_BGRA8888_NEON
	vpush			{d4}

	// Align source to a cache line boundary
	cmp				r2, #0
abgr_head_loop:
	beq				abgr_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				abgr_aligned
	ldr				r3, [r1], #4				// r3 = *src++ (RRGGBBAA)
	subs			r2, #1						// num--
	ConvABGR		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				abgr_head_loop

abgr_aligned:
	subs			r2, r2, #8
	bcc				abgr_tail

abgr_inner_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Load up source de-interleaved, one color channel per register.
	// Then increment the pointer.
	vld4.u8			{d0 - d3}, [r1,:256]!

	// Rotate the registers, convert from ABGR to BGRA.
	vmov			d4, d0

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d1 - d4}, [r0]!

	subs			r2, r2, #8
	bcs				abgr_inner_loop

abgr_tail:
	adds			r2, r2, #8
	beq				abgr_done

abgr_tail_loop:
	ldr				r3, [r1], #4				// r3 = *src++ (RRGGBBAA)
	subs			r2, #1						// num--
	ConvABGR		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				abgr_tail_loop

abgr_done:
	vpop			{d4}
FUNC_EPILOGUE


// void Convert_ARGB8888_to_BGRA8888_NEON(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ARGB8888_to_BGRA8888_NEON
	// Align source to a cache line boundary
	cmp				r2, #0
argb_head_loop:
	beq				argb_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				argb_aligned
	ldr				r3, [r1], #4				// r3 = *src++ (BBGGRRAA)
	subs			r2, #1						// num--
	ConvARGB		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				argb_head_loop

argb_aligned:
	subs			r2, r2, #8
	bcc				argb_tail

argb_inner_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Load up source de-interleaved, one color channel per register.
	// Then increment the pointer.
	vld4.u8			{d0 - d3}, [r1,:256]!

	// Rotate the registers, convert from ARGB to BGRA.
	vswp.u8			d0, d3	// ARGB -> BRGA
	vswp.u8			d1, d2	// BRGA -> BGRA

	// Store the result to destination interleaved, then increment
	// the pointer.
	vst4.u8			{d0 - d3}, [r0]!

	subs			r2, r2, #8
	bcs				argb_inner_loop

argb_tail:
	adds			r2, r2, #8
	beq				argb_done

argb_tail_loop:
	ldr				r3, [r1], #4				// r3 = *src++ (BBGGRRAA)
	subs			r2, #1						// num--
	ConvARGB		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				argb_tail_loop

argb_done:
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM_NEON
