/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM)

#include "vegasampler_arm.h"

.arch armv6			// Support ARMv6 instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text

// Lerp
//   out = c1 + (c2 - c1) * frac
//
// Parameters:
//   c1 - (must be r0 - r4, r8 - r9, r11 or r14)
//   c2 - (--"--)
//   frac - (--"--), 0 - 255
// Input
//   r10 - constant mask 0x00ff00ff
//
// Output
//   out - (must be r0 - r4, r8 - r9, r11 or r14, but NOT frac)
//
// Side effects:
//   r5, r6, r8
.macro Lerp out c1 c2 frac
	and				r5, r10, \c1, lsr #8	// r5 = c1.ag (..AA..GG)
	and				r8, r10, \c2, lsr #8	// r8 = c2.ag (..AA..GG)
	and				r6, r10, \c1			// r6 = c1.rb (..RR..BB)
	sub				r8, r8, r5				// r8 = c2.ag - c1.ag (..AA..GG)
	and				\out, r10, \c2			// out = c2.rb (..RR..BB)
	sub				\out, \out, r6			// out = c2.rb - c1.rb (..RR..BB)

	mul				r8, r8, \frac			// r8 = (c2.ag - c1.ag) * frac (AAaaGGgg)
	mul				\out, \out, \frac		// out = (c2.rb - c1.rb) * frac (RRrrBBbb)
	add				r8, r8, r5, asl #8		// r8 = (c2.ag - c1.ag) * frac + c1.ag
	add				\out, \out, r6, asl #8	// out = (c2.rb - c1.rb) * frac + c1.rb

	bic				r8, r8, r10
	and				\out, r10, \out, lsr #8
	orr				\out, r8, \out			// out = combined result
.endm


// void Sampler_LerpX_ARMv6(void* dst, const void* src, INT32 csx, INT32 cdx, unsigned int dst_len, unsigned int src_len);
//  r0 = dst
//  r1 = src
//  r2 = csx
//  r3 = cdx
//  [sp] = dst_len
//  [sp+4] = src_len
FUNC_PROLOGUE_ARM Sampler_LerpX_ARMv6
	push			{r4-r11,r14}

	ldr				r12, [sp, #36]			// r12 = dst_len
	cmp				r12, #0
	beq				slx_done
	ldr				r14, [sp, #40]
	sub				r14, r14, #1			// r14 = src_len - 1

	ldr				r4, [r1]				// r4 = src[0]
slx_headloop:							// while (dst_len && csx < 0)
	cmp				r2, #0
	bge				slx_headdone
	subs			r12, r12, #1			// dst_len--
	add				r2, r2, r3				// csx += cdx
	str				r4, [r0], #4			// *dst++ = src[0]
	bne				slx_headloop

	b				slx_done				// dst_len == 0, so done

slx_headdone:
	// Load up mask constants
	mov				r9, #0xff				// r9  = 0x000000ff
	orr				r10, r9, r9, asl #16	// r10 = 0x00ff00ff

slx_mainloop:
	cmp				r2, r14, asl #12		// csx >= VEGA_INTTOSAMPLE(src_len-1)?
	bge				slx_tail

	// Calculate source pointer (r5) and interpolation fraction (r11)
	mov				r5, r2, asr #12			// r5 = VEGA_SAMPLE_INT(csx)
	and				r11, r9, r2, lsr #4		// r11 = VEGA_SAMPLE_FRAC(csx)
	add				r5, r1, r5, asl #2		// r5 = &src[VEGA_SAMPLE_INT(csx)]

	subs			r12, r12, #1			// dst_len--
	add				r2, r2, r3				// csx += cdx

	// Load source samples
	ldm				r5, {r4, r7}			// r4 = c1 (AARRGGBB), r7 = c2 (AARRGGBB)

	// Interpolate
	Lerp			r4, r4, r7, r11			// r4 = c1 + (c2 - c1) * VEGA_SAMPLE_FRAC(csx)

	// Store destination
	str				r4, [r0], #4			// *dst++ = r4

	bne				slx_mainloop

slx_tail:
	cmp				r12, #0					// dst_len == 0?
	beq				slx_done

	ldr				r4, [r1, r14, asl #2]	// r4 = src[src_len - 1]
slx_tailloop:							// while (dst_len > 0)
	subs			r12, r12, #1			// dst_len--
	add				r2, r2, r3				// csx += cdx
	str				r4, [r0], #4			// *dst++ = src[src_len - 1]
	bne				slx_tailloop

slx_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Sampler_LerpY_ARMv6(UINT32* dst, const UINT32* src1, const UINT32* src2, INT32 frc_y, unsigned int len);
//  r0 = dst
//  r1 = src1
//  r2 = src2
//  r3 = frc_y (0 - 255)
//  [sp] = len
FUNC_PROLOGUE_ARM Sampler_LerpY_ARMv6
	push			{r4-r11,r14}

	// Load up mask constant
	mov				r10, #0xff
	orr				r10, r10, r10, asl #16	// r10 = 0x00ff00ff

	ldr				r14, [sp, #36]			// r14 = len

	// Align destination to a cache line boundary
	cmp				r14, #0
sly_headloop:
	beq				sly_done				// len == 0?
	ands			r12, r0, #15			// Are we aligned yet?
	beq				sly_aligned

	ldr				r4, [r1], #4			// r4 = c1 (AARRGGBB)
	ldr				r9, [r2], #4			// r9 = c2 (AARRGGBB)
	Lerp			r4, r4, r9, r3			// r4 = c1 + (c2 - c1) * frc_y
	str				r4, [r0], #4			// *dst++ = r4

	subs			r14, r14, #1			// len--
	b				sly_headloop

sly_aligned:
	subs			r14, r14, #4			// len -= 4
	bcc				sly_tail

sly_mainloop:
	// ---- PASS 1: 2 pixels ----

	// Load source 1
	ldmia			r1!, {r4, r7}			// r4 = c1, r7 = c3 (AARRGGBB)

	// Load source 2
	ldmia			r2!, {r9, r11}			// r9 = c2, r11 = c4 (AARRGGBB)

	// Interpolate
	Lerp			r4, r4, r9, r3			// r4 = c1 + (c2 - c1) * frc_y
	Lerp			r7, r7, r11, r3			// r7 = c3 + (c4 - c3) * frc_y

	// Store destination
	stmia			r0!, {r4, r7}

	// ---- PASS 2: 2 pixels ----

	// Load source 1
	ldmia			r1!, {r4, r7}			// r4 = c1, r7 = c3 (AARRGGBB)

	// Load source 2
	ldmia			r2!, {r9, r11}			// r9 = c6, r11 = c8 (AARRGGBB)

	// Interpolate
	Lerp			r4, r4, r9, r3			// r4 = c5 + (c6 - c5) * frc_y
	Lerp			r7, r7, r11, r3			// r7 = c7 + (c8 - c7) * frc_y

	// Store destination
	stmia			r0!, {r4, r7}

	subs			r14, r14, #4			// len -= 4
	bcs				sly_mainloop

sly_tail:
	adds			r14, r14, #4
	beq				sly_done

sly_tailloop:
	ldr				r4, [r1], #4			// r4 = c1 (AARRGGBB)
	ldr				r9, [r2], #4			// r9 = c2 (AARRGGBB)
	Lerp			r4, r4, r9, r3			// r4 = c1 + (c2 - c1) * frc_y
	str				r4, [r0], #4			// *dst++ = r4

	subs			r14, r14, #1			// len--
	bne				sly_tailloop

sly_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM
