/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM)

#include "vegafmtconvert_arm.h"

.arch armv6			// Support ARMv6 instructions
.syntax unified		// Allow both ARM and Thumb-2 instructions
.text


//============================================================================
// NOTE: The target format for all functions is BGRA8888, and all functions
// assume that we use little endian format.
//============================================================================

// ConvRGBA2 - Convert two pixels
//
// Parameters:
//  reg1 - source and destination register 1
//  reg2 - source and destination register 2
//
// Input:
//  r11 - must be set to 0x00ff00ff
//
// Output:
//  reg1 - result for sample 1
//  reg2 - result for sample 2
//
// Side effects:
//  r12, r14
.macro ConvRGBA2 reg1 reg2
	and				r12, \reg1, r11				// r12  = (..BB..RR)
	and				r14, \reg2, r11				// r14  = (..BB..RR)
	bic				\reg1, \reg1, r11			// reg1 = (AA..GG..)
	bic				\reg2, \reg2, r11			// reg2 = (AA..GG..)
	orr				\reg1, \reg1, r12, lsr #16	// reg1 = (AA..GGBB)
	orr				\reg2, \reg2, r14, lsr #16	// reg2 = (AA..GGBB)
	orr				\reg1, \reg1, r12, asl #16	// reg1 = (AARRGGBB)
	orr				\reg2, \reg2, r14, asl #16	// reg2 = (AARRGGBB)
.endm

// ConvABGR2 - Convert two pixels
//
// Parameters:
//  reg1 - source and destination register 1
//  reg2 - source and destination register 2
//
// Output:
//  reg1 - result for sample 1
//  reg2 - result for sample 2
//
// Side effects:
//  r12, r14
.macro ConvABGR2 reg1 reg2
	mov				r12, \reg1, asl #24			// r12 = (AA......)
	mov				r14, \reg2, asl #24			// r14 = (AA......)
	orr				\reg1, r12, \reg1, lsr #8	// reg1 = (AARRGGBB)
	orr				\reg2, r14, \reg2, lsr #8	// reg2 = (AARRGGBB)
.endm

// ConvBGRA4444
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvBGRA4444 reg
	and				r11,  \reg, #0x0000f000		// r11 = (....A...)
	and				r12,  \reg, #0x00000f00		// r12 = (.....R..)
	and				r14,  \reg, #0x000000f0		// r14 = (......G.)
	and				\reg, \reg, #0x0000000f		// reg = (.......B)
	orr				r12, r12, r11, lsl #4		// r12 = (...A.R..)
	orr				\reg, \reg, r14, lsl #4		// reg = (.....G.B)
	orr				\reg, \reg, r12, lsl #8		// reg = (.A.R.G.B)
	orr				\reg, \reg, \reg, lsl #4	// reg = (AARRGGBB)
.endm

// ConvBGRA4444x8 - convert eight 16-bit pixels into eight 32-bit pixels
//
// Input:
//  r3-r6 - src0..src7 (two pixels per register)
//
// Output:
//  r3-r10 - dst0..dst7 (one pixel per register)
//
// Side effects:
//  r2, r11, r12, r14
.macro ConvBGRA4444x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 33 CPU cycles on a dual pipelined architecture (i.e. roughly 4.1 cycles
	// per pixel).
	and				r10, r6, #0x000f0000	// dst7 = (...B....)
	and				r11, r6, #0x00f00000	// r11  = (..G.....)
	and				r12, r6, #0x0f000000	// r12  = (.R......)
	and				r14, r6, #0xf0000000	// r14  = (A.......)
	orr				r10, r11, r10, lsr #4	// dst7 = (..G.B...)
	and				r9,  r6, #0x0000f000	// dst6 = (....A...)
	orr				r14, r14, r12, lsr #4	// r14  = (A.R.....)
	and				r11, r6, #0x00000f00	// r11  = (.....R..)
	orr				r10, r14, r10, lsr #8	// dst7 = (A.R.G.B.)
	and				r12, r6, #0x000000f0	// r12  = (......G.)
	and				r14, r6, #0x0000000f	// r14  = (.......B)
	orr				r9, r11, r9, lsl #4		// dst6 = (...A.R..)
	orr				r10, r10, r10, lsr #4	// dst7 = (AARRGGBB)
	orr				r14, r14, r12, lsl #4	// r12  = (.....G.B)
	and				r8,  r5, #0x000f0000	// dst5 = (...B....)
	orr				r9, r14, r9, lsl #8		// dst6 = (.A.R.G.B)
	and				r11, r5, #0x00f00000	// r11  = (..G.....)
	and				r12, r5, #0x0f000000	// r12  = (.R......)
	and				r14, r5, #0xf0000000	// r14  = (A.......)
	orr				r9, r9, r9, lsl #4		// dst6 = (AARRGGBB)
	orr				r8, r11, r8, lsr #4		// dst5 = (..G.B...)
	and				r7,  r5, #0x0000f000	// dst4 = (....A...)
	orr				r14, r14, r12, lsr #4	// r14  = (A.R.....)
	and				r11, r5, #0x00000f00	// r11  = (.....R..)
	orr				r8, r14, r8, lsr #8		// dst5 = (A.R.G.B.)
	and				r12, r5, #0x000000f0	// r12  = (......G.)
	and				r14, r5, #0x0000000f	// r14  = (.......B)
	orr				r7, r11, r7, lsl #4		// dst4 = (...A.R..)
	orr				r8, r8, r8, lsr #4		// dst5 = (AARRGGBB)
	orr				r14, r14, r12, lsl #4	// r12  = (.....G.B)
	and				r6,  r4, #0x000f0000	// dst3 = (...B....)
	orr				r7, r14, r7, lsl #8		// dst4 = (.A.R.G.B)
	and				r11, r4, #0x00f00000	// r11  = (..G.....)
	and				r12, r4, #0x0f000000	// r12  = (.R......)
	and				r14, r4, #0xf0000000	// r14  = (A.......)
	orr				r7, r7, r7, lsl #4		// dst4 = (AARRGGBB)
	orr				r6, r11, r6, lsr #4		// dst3 = (..G.B...)
	and				r5,  r4, #0x0000f000	// dst2 = (....A...)
	orr				r14, r14, r12, lsr #4	// r14  = (A.R.....)
	and				r11, r4, #0x00000f00	// r11  = (.....R..)
	orr				r6, r14, r6, lsr #8		// dst3 = (A.R.G.B.)
	and				r12, r4, #0x000000f0	// r12  = (......G.)
	and				r14, r4, #0x0000000f	// r14  = (.......B)
	orr				r5, r11, r5, lsl #4		// dst2 = (...A.R..)
	orr				r6, r6, r6, lsr #4		// dst3 = (AARRGGBB)
	orr				r14, r14, r12, lsl #4	// r12  = (.....G.B)
	and				r4,  r3, #0x000f0000	// dst1 = (...B....)
	orr				r5, r14, r5, lsl #8		// dst2 = (.A.R.G.B)
	and				r11, r3, #0x00f00000	// r11  = (..G.....)
	and				r12, r3, #0x0f000000	// r12  = (.R......)
	and				r14, r3, #0xf0000000	// r14  = (A.......)
	orr				r5, r5, r5, lsl #4		// dst2 = (AARRGGBB)
	orr				r4, r11, r4, lsr #4		// dst1 = (..G.B...)
	and				r2,  r3, #0x0000f000	// r2   = (....A...)
	orr				r14, r14, r12, lsr #4	// r14  = (A.R.....)
	and				r11, r3, #0x00000f00	// r11  = (.....R..)
	orr				r4, r14, r4, lsr #8		// dst1 = (A.R.G.B.)
	and				r12, r3, #0x000000f0	// r12  = (......G.)
	and				r14, r3, #0x0000000f	// r14  = (.......B)
	orr				r2, r11, r2, lsl #4		// r2   = (...A.R..)
	orr				r4, r4, r4, lsr #4		// dst1 = (AARRGGBB)
	orr				r14, r14, r12, lsl #4	// r12  = (.....G.B)
	orr				r2, r14, r2, lsl #8		// r2   = (.A.R.G.B)
	orr				r3, r2, r2, lsl #4		// dst0 = (AARRGGBB)
.endm

// ConvRGBA4444
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvRGBA4444 reg
	rev16			\reg, \reg					// reg = (....GRAB)
	and				r11,  \reg, #0x000000f0		// r11 = (......A.)
	and				r12,  \reg, #0x0000f000		// r12 = (....G...)
	and				r14,  \reg, #0x00000f00		// r14 = (.....R..)
	and				\reg, \reg, #0x0000000f		// reg = (.......B)
	orr				r14, r14, r11, lsl #12		// r14 = (...A.R..)
	orr				\reg, \reg, r12, lsr #4		// reg = (.....G.B)
	orr				\reg, \reg, r14, lsl #8		// reg = (.A.R.G.B)
	orr				\reg, \reg, \reg, lsl #4	// reg = (AARRGGBB)
.endm

// ConvRGBA4444x8 - convert eight 16-bit pixels into eight 32-bit pixels
//
// Input:
//  r3-r6 - src0..src7 (two pixels per register)
//
// Output:
//  r3-r10 - dst0..dst7 (one pixel per register)
//
// Side effects:
//  r2, r11, r12, r14
.macro ConvRGBA4444x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 36 CPU cycles on a dual pipelined architecture (i.e. roughly 4.5 cycles
	// per pixel).
	and				r10, r6, #0x0f000000		// dst7 = (.B......)
	and				r11, r6, #0x000f0000		// r11  = (...R....)
	and				r12, r6, #0x00f00000		// r12  = (..G.....)
	and				r14, r6, #0xf0000000		// r14  = (A.......)
	orr				r10, r12, r10, lsr #12		// dst7 = (..G.B...)
	rev16			r6, r6						// r6   = (GRABGRAB)
	orr				r14, r14, r11, lsl #4		// r14  = (A.R.....)
	and				r9,  r6, #0x000000f0		// dst6 = (......A.)
	orr				r10, r14, r10, lsr #8		// dst7 = (A.R.G.B.)
	and				r11, r6, #0x0000f000		// r11  = (....G...)
	and				r12, r6, #0x00000f00		// r12  = (.....R..)
	and				r14, r6, #0x0000000f		// r14  = (.......B)
	orr				r10, r10, r10, lsr #4		// dst7 = (AARRGGBB)
	orr				r9, r12, r9, lsl #12		// dst6 = (...A.R..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....G.B)
	and				r8,  r5, #0x0f000000		// dst5 = (.B......)
	orr				r9, r14, r9, lsl #8			// dst6 = (.A.R.G.B)
	and				r11, r5, #0x000f0000		// r11  = (...R....)
	and				r12, r5, #0x00f00000		// r12  = (..G.....)
	and				r14, r5, #0xf0000000		// r14  = (A.......)
	orr				r9, r9, r9, lsl #4			// dst6 = (AARRGGBB)
	orr				r8, r12, r8, lsr #12		// dst5 = (..G.B...)
	rev16			r5, r5						// r5   = (GRABGRAB)
	orr				r14, r14, r11, lsl #4		// r14  = (A.R.....)
	and				r7,  r5, #0x000000f0		// dst4 = (......A.)
	orr				r8, r14, r8, lsr #8			// dst5 = (A.R.G.B.)
	and				r11, r5, #0x0000f000		// r11  = (....G...)
	and				r12, r5, #0x00000f00		// r12  = (.....R..)
	and				r14, r5, #0x0000000f		// r14  = (.......B)
	orr				r8, r8, r8, lsr #4			// dst5 = (AARRGGBB)
	orr				r7, r12, r7, lsl #12		// dst4 = (...A.R..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....G.B)
	and				r6,  r4, #0x0f000000		// dst3 = (.B......)
	and				r11, r4, #0x000f0000		// r11  = (...R....)
	orr				r7, r14, r7, lsl #8			// dst4 = (.A.R.G.B)
	and				r12, r4, #0x00f00000		// r12  = (..G.....)
	and				r14, r4, #0xf0000000		// r14  = (A.......)
	orr				r6, r12, r6, lsr #12		// dst3 = (..G.B...)
	orr				r7, r7, r7, lsl #4			// dst4 = (AARRGGBB)
	rev16			r4, r4						// r4   = (GRABGRAB)
	orr				r14, r14, r11, lsl #4		// r14  = (A.R.....)
	and				r5,  r4, #0x000000f0		// dst2 = (......A.)
	orr				r6, r14, r6, lsr #8			// dst3 = (A.R.G.B.)
	and				r11, r4, #0x0000f000		// r11  = (....G...)
	and				r12, r4, #0x00000f00		// r12  = (.....R..)
	and				r14, r4, #0x0000000f		// r14  = (.......B)
	orr				r6, r6, r6, lsr #4			// dst3 = (AARRGGBB)
	orr				r5, r12, r5, lsl #12		// dst2 = (...A.R..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....G.B)
	and				r4,  r3, #0x0f000000		// dst1 = (.B......)
	orr				r5, r14, r5, lsl #8			// dst2 = (.A.R.G.B)
	and				r11, r3, #0x000f0000		// r11  = (...R....)
	and				r12, r3, #0x00f00000		// r12  = (..G.....)
	and				r14, r3, #0xf0000000		// r14  = (A.......)
	rev16			r3, r3						// r3   = (GRABGRAB)
	orr				r5, r5, r5, lsl #4			// dst2 = (AARRGGBB)
	orr				r4, r12, r4, lsr #12		// dst1 = (..G.B...)
	orr				r14, r14, r11, lsl #4		// r14  = (A.R.....)
	and				r2,  r3, #0x000000f0		// r2   = (......A.)
	and				r11, r3, #0x0000f000		// r11  = (....G...)
	orr				r4, r14, r4, lsr #8			// dst1 = (A.R.G.B.)
	and				r12, r3, #0x00000f00		// r12  = (.....R..)
	and				r14, r3, #0x0000000f		// r14  = (.......B)
	orr				r2, r12, r2, lsl #12		// r2   = (...A.R..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....G.B)
	orr				r4, r4, r4, lsr #4			// dst1 = (AARRGGBB)
	orr				r2, r14, r2, lsl #8			// r2   = (.A.R.G.B)
	orr				r3, r2, r2, lsl #4			// dst0 = (AARRGGBB)
.endm

// ConvABGR4444
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvABGR4444 reg
	and				r11,  \reg, #0x000000f0		// r11 = (......B.)
	and				r12,  \reg, #0x0000f000		// r12 = (....R...)
	and				r14,  \reg, #0x00000f00		// r14 = (.....G..)
	and				\reg, \reg, #0x0000000f		// reg = (.......A)
	orr				r14, r14, r11, lsl #12		// r14 = (...B.G..)
	orr				\reg, \reg, r12, lsr #4		// reg = (.....R.A)
	orr				\reg, \reg, r14, lsl #8		// reg = (.B.G.R.A)
	rev				\reg, \reg					// reg = (.A.R.G.B)
	orr				\reg, \reg, \reg, lsl #4	// reg = (AARRGGBB)
.endm

// ConvABGR4444x8 - convert eight 16-bit pixels into eight 32-bit pixels
//
// Input:
//  r3-r6 - src0..src7 (two pixels per register)
//
// Output:
//  r3-r10 - dst0..dst7 (one pixel per register)
//
// Side effects:
//  r2, r11, r12, r14
.macro ConvABGR4444x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 39 CPU cycles on a dual pipelined architecture (i.e. roughly 4.9 cycles
	// per pixel).
	and				r10, r6, #0x00f00000		// dst7 = (..B.....)
	and				r11, r6, #0xf0000000		// r11  = (R.......)
	and				r12, r6, #0x0f000000		// r12  = (.G......)
	and				r14, r6, #0x000f0000		// r14  = (...A....)
	orr				r10, r12, r10, lsr #4		// dst7 = (.G.B....)
	orr				r14, r14, r11, lsr #20		// r14  = (...A.R..)
	and				r9,  r6, #0x000000f0		// dst6 = (......B.)
	and				r11, r6, #0x0000f000		// r11  = (....R...)
	mov				r10, r10, lsr #16			// dst7 = (.....G.B)
	and				r12, r6, #0x00000f00		// r12  = (.....G..)
	orr				r10, r10, r14, lsl #8		// dst7 = (.A.R.G.B)
	and				r14, r6, #0x0000000f		// r14  = (.......A)
	orr				r9, r12, r9, lsl #12		// dst6 = (...B.G..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....R.A)
	orr				r10, r10, r10, lsl #4		// dst7 = (AARRGGBB)
	and				r8,  r5, #0x00f00000		// dst5 = (..B.....)
	orr				r9, r14, r9, lsl #8			// dst6 = (.B.G.R.A)
	and				r11, r5, #0xf0000000		// r11  = (R.......)
	and				r12, r5, #0x0f000000		// r12  = (.G......)
	and				r14, r5, #0x000f0000		// r14  = (...A....)
	rev				r9, r9						// dst6 = (.A.R.G.B)
	orr				r8, r12, r8, lsr #4			// dst5 = (.G.B....)
	orr				r9, r9, r9, lsl #4			// dst6 = (AARRGGBB)
	orr				r14, r14, r11, lsr #20		// r14  = (...A.R..)
	mov				r8, r8, lsr #16				// dst5 = (.....G.B)
	and				r7,  r5, #0x000000f0		// dst4 = (......B.)
	orr				r8, r8, r14, lsl #8			// dst5 = (.A.R.G.B)
	and				r11, r5, #0x0000f000		// r11  = (....R...)
	and				r12, r5, #0x00000f00		// r12  = (.....G..)
	and				r14, r5, #0x0000000f		// r14  = (.......A)
	orr				r8, r8, r8, lsl #4			// dst5 = (AARRGGBB)
	orr				r7, r12, r7, lsl #12		// dst4 = (...B.G..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....R.A)
	and				r6,  r4, #0x00f00000		// dst3 = (..B.....)
	orr				r7, r14, r7, lsl #8			// dst4 = (.B.G.R.A)
	and				r11, r4, #0xf0000000		// r11  = (R.......)
	and				r12, r4, #0x0f000000		// r12  = (.G......)
	and				r14, r4, #0x000f0000		// r14  = (...A....)
	rev				r7, r7						// dst4 = (.A.R.G.B)
	orr				r6, r12, r6, lsr #4			// dst3 = (.G.B....)
	orr				r7, r7, r7, lsl #4			// dst4 = (AARRGGBB)
	orr				r14, r14, r11, lsr #20		// r14  = (...A.R..)
	mov				r6, r6, lsr #16				// dst3 = (.....G.B)
	and				r5,  r4, #0x000000f0		// dst2 = (......B.)
	orr				r6, r6, r14, lsl #8			// dst3 = (.A.R.G.B)
	and				r11, r4, #0x0000f000		// r11  = (....R...)
	and				r12, r4, #0x00000f00		// r12  = (.....G..)
	and				r14, r4, #0x0000000f		// r14  = (.......A)
	orr				r6, r6, r6, lsl #4			// dst3 = (AARRGGBB)
	orr				r5, r12, r5, lsl #12		// dst2 = (...B.G..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....R.A)
	and				r4,  r3, #0x00f00000		// dst1 = (..B.....)
	orr				r5, r14, r5, lsl #8			// dst2 = (.B.G.R.A)
	and				r11, r3, #0xf0000000		// r11  = (R.......)
	and				r12, r3, #0x0f000000		// r12  = (.G......)
	and				r14, r3, #0x000f0000		// r14  = (...A....)
	rev				r5, r5						// dst2 = (.A.R.G.B)
	orr				r4, r12, r4, lsr #4			// dst1 = (.G.B....)
	orr				r5, r5, r5, lsl #4			// dst2 = (AARRGGBB)
	orr				r14, r14, r11, lsr #20		// r14  = (...A.R..)
	mov				r4, r4, lsr #16				// dst1 = (.....G.B)
	and				r2,  r3, #0x000000f0		// r2   = (......B.)
	orr				r4, r4, r14, lsl #8			// dst1 = (.A.R.G.B)
	and				r11, r3, #0x0000f000		// r11  = (....R...)
	and				r12, r3, #0x00000f00		// r12  = (.....G..)
	and				r14, r3, #0x0000000f		// r14  = (.......A)
	orr				r4, r4, r4, lsl #4			// dst1 = (AARRGGBB)
	orr				r2, r12, r2, lsl #12		// r2   = (...B.G..)
	orr				r14, r14, r11, lsr #4		// r14  = (.....R.A)
	orr				r2, r14, r2, lsl #8			// r2   = (.B.G.R.A)
	rev				r2, r2						// r2   = (.A.R.G.B)
	orr				r3, r2, r2, lsl #4			// dst0 = (AARRGGBB)
.endm

// ConvARGB4444
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvARGB4444 reg
	and				r11,  \reg, #0x0000f000		// r11 = (....B...)
	and				r12,  \reg, #0x00000f00		// r12 = (.....G..)
	and				r14,  \reg, #0x000000f0		// r14 = (......R.)
	and				\reg, \reg, #0x0000000f		// reg = (.......A)
	orr				r12, r12, r11, lsl #4		// r12 = (...B.G..)
	orr				\reg, \reg, r14, lsl #4		// reg = (.....R.A)
	orr				\reg, \reg, r12, lsl #8		// reg = (.B.G.R.A)
	rev				\reg, \reg					// reg = (.A.R.G.B)
	orr				\reg, \reg, \reg, lsl #4	// reg = (AARRGGBB)
.endm

// ConvARGB4444x8 - convert eight 16-bit pixels into eight 32-bit pixels
//
// Input:
//  r3-r6 - src0..src7 (two pixels per register)
//
// Output:
//  r3-r10 - dst0..dst7 (one pixel per register)
//
// Side effects:
//  r2, r11, r12, r14
.macro ConvARGB4444x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 37 CPU cycles on a dual pipelined architecture (i.e. roughly 4.6 cycles
	// per pixel).
	and				r10, r6, #0xf0000000		// dst7 = (B.......)
	and				r11, r6, #0x0f000000		// r11  = (.G......)
	and				r12, r6, #0x000f0000		// r12  = (...A....)
	and				r14, r6, #0x00f00000		// r14  = (..R.....)
	orr				r10, r11, r10, lsr #12		// dst7 = (.G.B....)
	and				r9,  r6, #0x0000f000		// dst6 = (....B...)
	orr				r14, r14, r12, lsl #12		// r14  = (A.R.....)
	and				r11, r6, #0x00000f00		// r11  = (.....G..)
	orr				r10, r14, r10, lsr #12		// dst7 = (A.R.G.B.)
	and				r12, r6, #0x000000f0		// r12  = (......R.)
	and				r14, r6, #0x0000000f		// r14  = (.......A)
	orr				r9, r11, r9, lsl #4			// dst6 = (...B.G..)
	orr				r10, r10, r10, lsr #4		// dst7 = (AARRGGBB)
	orr				r14, r14, r12, lsl #4		// r14  = (.....R.A)
	orr				r9, r14, r9, lsl #8			// dst6 = (.B.G.R.A)
	and				r8,  r5, #0xf0000000		// dst5 = (B.......)
	and				r11, r5, #0x0f000000		// r11  = (.G......)
	and				r12, r5, #0x000f0000		// r12  = (...A....)
	rev				r9, r9						// dst6 = (.A.R.G.B)
	and				r14, r5, #0x00f00000		// r14  = (..R.....)
	orr				r9, r9, r9, lsl #4			// dst6 = (AARRGGBB)
	orr				r8, r11, r8, lsr #12		// dst5 = (.G.B....)
	orr				r14, r14, r12, lsl #12		// r14  = (A.R.....)
	and				r7,  r5, #0x0000f000		// dst4 = (....B...)
	orr				r8, r14, r8, lsr #12		// dst5 = (A.R.G.B.)
	and				r11, r5, #0x00000f00		// r11  = (.....G..)
	and				r12, r5, #0x000000f0		// r12  = (......R.)
	and				r14, r5, #0x0000000f		// r14  = (.......A)
	orr				r8, r8, r8, lsr #4			// dst5 = (AARRGGBB)
	orr				r7, r11, r7, lsl #4			// dst4 = (...B.G..)
	orr				r14, r14, r12, lsl #4		// r14  = (.....R.A)
	and				r6,  r4, #0xf0000000		// dst3 = (B.......)
	orr				r7, r14, r7, lsl #8			// dst4 = (.B.G.R.A)
	and				r11, r4, #0x0f000000		// r11  = (.G......)
	and				r12, r4, #0x000f0000		// r12  = (...A....)
	and				r14, r4, #0x00f00000		// r14  = (..R.....)
	rev				r7, r7						// dst4 = (.A.R.G.B)
	orr				r6, r11, r6, lsr #12		// dst3 = (.G.B....)
	orr				r7, r7, r7, lsl #4			// dst4 = (AARRGGBB)
	orr				r14, r14, r12, lsl #12		// r14  = (A.R.....)
	and				r5,  r4, #0x0000f000		// dst2 = (....B...)
	orr				r6, r14, r6, lsr #12		// dst3 = (A.R.G.B.)
	and				r11, r4, #0x00000f00		// r11  = (.....G..)
	and				r12, r4, #0x000000f0		// r12  = (......R.)
	and				r14, r4, #0x0000000f		// r14  = (.......A)
	orr				r6, r6, r6, lsr #4			// dst3 = (AARRGGBB)
	orr				r5, r11, r5, lsl #4			// dst2 = (...B.G..)
	orr				r14, r14, r12, lsl #4		// r14  = (.....R.A)
	and				r4,  r3, #0xf0000000		// dst1 = (B.......)
	and				r11, r3, #0x0f000000		// r11  = (.G......)
	orr				r5, r14, r5, lsl #8			// dst2 = (.B.G.R.A)
	and				r12, r3, #0x000f0000		// r12  = (...A....)
	and				r14, r3, #0x00f00000		// r14  = (..R.....)
	orr				r4, r11, r4, lsr #12		// dst1 = (.G.B....)
	rev				r5, r5						// dst2 = (.A.R.G.B)
	orr				r14, r14, r12, lsl #12		// r14  = (A.R.....)
	orr				r5, r5, r5, lsl #4			// dst2 = (AARRGGBB)
	and				r2,  r3, #0x0000f000		// r2   = (....B...)
	and				r11, r3, #0x00000f00		// r11  = (.....G..)
	and				r12, r3, #0x000000f0		// r12  = (......R.)
	orr				r4, r14, r4, lsr #12		// dst1 = (A.R.G.B.)
	and				r14, r3, #0x0000000f		// r14  = (.......A)
	orr				r2, r11, r2, lsl #4			// r2   = (...B.G..)
	orr				r14, r14, r12, lsl #4		// r14  = (.....R.A)
	orr				r2, r14, r2, lsl #8			// r2   = (.B.G.R.A)
	rev				r2, r2						// r2   = (.A.R.G.B)
	orr				r4, r4, r4, lsr #4			// dst1 = (AARRGGBB)
	orr				r3, r2, r2, lsl #4			// dst0 = (AARRGGBB)
.endm

// ConvRGB565
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvRGB565 reg
	and				r11, \reg, #0x0000f800		// r11 = (........ ........ rrrrr... ........)
	and				r12, \reg, #0x000007e0		// r12 = (........ ........ .....ggg ggg.....)
	and				r14, \reg, #0x0000001f		// r14 = (........ ........ ........ ...bbbbb)
	mov				\reg, #0xff000000			// reg = (11111111 ........ ........ ........)
	mov				r11, r11, lsr #8			// r11 = (........ ........ ........ rrrrr...)
	mov				r12, r12, lsr #3			// r12 = (........ ........ ........ gggggg..)
	mov				r14, r14, lsl #3			// r14 = (........ ........ ........ bbbbb...)
	orr				r11, r11, r11, lsr #5		// r11 = (........ ........ ........ rrrrrrrr)
	orr				r12, r12, r12, lsr #6		// r12 = (........ ........ ........ gggggggg)
	orr				r14, r14, r14, lsr #5		// r14 = (........ ........ ........ bbbbbbbb)
	orr				\reg, \reg, r11, lsl #16	// reg = (11111111 rrrrrrrr ........ ........)
	orr				\reg, \reg, r12, lsl #8		// reg = (11111111 rrrrrrrr gggggggg ........)
	orr				\reg, \reg, r14				// reg = (11111111 rrrrrrrr gggggggg bbbbbbbb)
.endm

// ConvBGR565
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r11, r12, r14
.macro ConvBGR565 reg
	and				r14, \reg, #0x0000001f		// r14 = (........ ........ ........ ...rrrrr)
	and				r12, \reg, #0x000007e0		// r12 = (........ ........ .....ggg ggg.....)
	and				r11, \reg, #0x0000f800		// r11 = (........ ........ bbbbb... ........)
	mov				\reg, #0xff000000			// reg = (11111111 ........ ........ ........)
	mov				r14, r14, lsl #3			// r14 = (........ ........ ........ rrrrr...)
	mov				r12, r12, lsr #3			// r12 = (........ ........ ........ gggggg..)
	mov				r11, r11, lsr #8			// r11 = (........ ........ ........ bbbbb...)
	orr				r14, r14, r14, lsr #5		// r14 = (........ ........ ........ rrrrrrrr)
	orr				r12, r12, r12, lsr #6		// r12 = (........ ........ ........ gggggggg)
	orr				r11, r11, r11, lsr #5		// r11 = (........ ........ ........ bbbbbbbb)
	orr				\reg, \reg, r14, lsl #16	// reg = (11111111 rrrrrrrr ........ ........)
	orr				\reg, \reg, r12, lsl #8		// reg = (11111111 rrrrrrrr gggggggg ........)
	orr				\reg, \reg, r11				// reg = (11111111 rrrrrrrr gggggggg bbbbbbbb)
.endm

// ConvRGB888
//
// Parameters:
//  dst - destination register
//  src - source memory pointer
//  num - number of pixel left to process
//
// Output:
//  dst - result
//  src - source memory pointer is incremented by 3
//  num - pixel count is decremented by 1 (num--)
//
// Side effects:
//  r11, r12, r14
//  status flags are updated by the num-- operation
.macro ConvRGB888 dst src num
	ldrb			r11, [\src]					// r11 = (......RR)
	ldrb			r12, [\src, #1]				// r12 = (......GG)
	mov				\dst, #0xff000000			// dst = (AA......)
	ldrb			r14, [\src, #2]				// r14 = (......BB)
	orr				\dst, \dst, r11, lsl #16	// dst = (AARR....)
	add				\src, \src, #3				// src += 3
	orr				\dst, \dst, r12, lsl #8		// dst = (AARRGG..)
	subs			\num, #1					// num--
	orr				\dst, \dst, r14				// dst = (AARRGGBB)
.endm

// ConvRGB888x8 - convert eight 24-bit pixels into eight 32-bit pixels
//
// Input:
//  r3 - (R1B0G0R0)
//  r4 - (G2R2B1G1)
//  r5 - (B3G3R3B2)
//  r6 - (R5B4G4R4)
//  r7 - (G6R6B5G5)
//  r8 - (B7G7R7B6)
//
// Output:
//  r5-r12 - dst0-dst7
//
// Side effects:
//  none
.macro ConvRGB888x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 13 CPU cycles on a dual pipelined architecture (i.e. roughly 1.6 cycles
	// per pixel).
	mov				r10, r6, lsr #16			// dst5 = (....RRxx)
	mov				r9, r6, lsl #8				// dst4 = (BBGGRR..)
	mov				r11, r7, lsr #8				// dst6 = (..GGRRxx)
	rev				r12, r8						// dst7 = (xxRRGGBB)
	orr				r10, r10, r7, lsl #16		// dst5 = (BBGGRRxx)
	orr				r11, r11, r8, lsl #24		// dst6 = (BBGGRRxx)
	rev				r8, r5						// dst3 = (xxRRGGBB)
	mov				r7, r5, lsl #24				// dst2 = (BB......)
	orr				r7, r7, r4, lsr #8			// dst2 = (BBGGRRxx)
	mov				r6, r4, lsl #16				// dst1 = (BBGG....)
	orr				r6, r6, r3, lsr #16			// dst1 = (BBGGRRxx)
	mov				r5, r3, lsl #8				// dst0 = (BBGGRR..)

	rev				r11, r11					// dst6 = (xxRRGGBB)
	rev				r10, r10					// dst5 = (xxRRGGBB)
	rev				r9, r9						// dst4 = (..RRGGBB)
	rev				r7, r7						// dst2 = (xxRRGGBB)
	rev				r6, r6						// dst1 = (xxRRGGBB)
	rev				r5, r5						// dst0 = (..RRGGBB)

	orr				r8, r8, #0xff000000			// dst3 = (AARRGGBB)
	orr				r7, r7, #0xff000000			// dst2 = (AARRGGBB)
	orr				r6, r6, #0xff000000			// dst1 = (AARRGGBB)
	orr				r5, r5, #0xff000000			// dst0 = (AARRGGBB)
	orr				r9, r9, #0xff000000			// dst4 = (AARRGGBB)
	orr				r10, r10, #0xff000000		// dst5 = (AARRGGBB)
	orr				r11, r11, #0xff000000		// dst6 = (AARRGGBB)
	orr				r12, r12, #0xff000000		// dst7 = (AARRGGBB)
.endm

// ConvBGR888
//
// Parameters:
//  dst - destination register
//  src - source memory pointer
//  num - number of pixel left to process
//
// Output:
//  dst - result
//  src - source memory pointer is incremented by 3
//  num - pixel count is decremented by 1 (num--)
//
// Side effects:
//  r11, r12, r14
//  status flags are updated by the num-- operation
.macro ConvBGR888 dst src num
	ldrb			r11, [\src, #2]				// r11 = (......RR)
	ldrb			r12, [\src, #1]				// r12 = (......GG)
	mov				\dst, #0xff000000			// dst = (AA......)
	ldrb			r14, [\src]					// r14 = (......BB)
	orr				\dst, \dst, r11, lsl #16	// dst = (AARR....)
	add				\src, \src, #3				// src += 3
	orr				\dst, \dst, r12, lsl #8		// dst = (AARRGG..)
	subs			\num, #1					// num--
	orr				\dst, \dst, r14				// dst = (AARRGGBB)
.endm

// ConvBGR888x8 - convert eight 24-bit pixels into eight 32-bit pixels
//
// Input:
//  r3 - (B1R0G0B0)
//  r4 - (G2B2R1G1)
//  r5 - (R3G3B3R2)
//  r6 - (B5R4G4B4)
//  r7 - (G6B6R5G5)
//  r8 - (R7G7B7R6)
//
// Output:
//  r5-r12 - dst0-dst7
//
// Side effects:
//  none
.macro ConvBGR888x8
	// This is an unrolled variant that has been optimized for minimal pipeline
	// stalls on an ARM Cortex-A8 machine. This routine should take about
	// 9 CPU cycles on a dual pipelined architecture (i.e. roughly 1 cycle
	// per pixel).
	orr				r9, r6, #0xff000000			// dst4 = (AARRGGBB)
	mov				r10, r6, lsr #24			// dst5 = (......BB)
	mov				r11, r7, lsr #16			// dst6 = (....GGBB)
	orr				r10, r10, r7, lsl #8		// dst5 = (xxRRGGBB)
	mov				r12, r8, lsr #8				// dst7 = (..RRGGBB)
	orr				r11, r11, r8, lsl #16		// dst6 = (xxRRGGBB)
	mov				r8, r5, lsr #8				// dst3 = (..RRGGBB)
	mov				r7, r4, lsr #16				// dst2 = (....GGBB)
	mov				r6, r4, lsl #8				// dst1 = (xxRRGG..)
	orr				r7, r7, r5, lsl #16			// dst2 = (xxRRGGBB)
	orr				r6, r6, r3, lsr #24			// dst1 = (xxRRGGBB)

	orr				r10, r10, #0xff000000		// dst5 = (AARRGGBB)
	orr				r11, r11, #0xff000000		// dst6 = (AARRGGBB)
	orr				r12, r12, #0xff000000		// dst7 = (AARRGGBB)
	orr				r5, r3, #0xff000000			// dst0 = (AARRGGBB)
	orr				r6, r6, #0xff000000			// dst1 = (AARRGGBB)
	orr				r7, r7, #0xff000000			// dst2 = (AARRGGBB)
	orr				r8, r8, #0xff000000			// dst3 = (AARRGGBB)
.endm

// void Convert_BGRA8888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
// NOTE: This is essentially a memcpy
FUNC_PROLOGUE_ARM Convert_BGRA8888_to_BGRA8888_ARMv6
	push			{r4-r9}

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

bgra_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	ldmia			r1!, {r3-r9,r12}			// Load src0..src7 (AARRGGBB)
	subs			r2, r2, #8
	stmia			r0!, {r3-r9,r12}			// Store to dst0..dst7 (AARRGGBB)
	bcs				bgra_main_loop

bgra_tail:
	adds			r2, r2, #8
	beq				bgra_done

bgra_tail_loop:
	ldr				r3, [r1], #4				// r3 = *src++ (AARRGGBB)
	subs			r2, #1						// num--
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				bgra_tail_loop

bgra_done:
	pop				{r4-r9}
FUNC_EPILOGUE


// void Convert_RGBA8888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_RGBA8888_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

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
	subs			r2, #1						// num--
	ConvRGBA		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				rgba_head_loop

rgba_aligned:
	subs			r2, r2, #8
	bcc				rgba_tail

	// Preload source data
	pld				[r1, #16*4]
rgba_main_loop:

	ldmia			r1!, {r3-r10}				// Load src0..src7 (AABBGGRR)
	subs			r2, r2, #8

	ConvRGBA2		r3, r4

	// Preload source data
	pld				[r1, #16*4]

	ConvRGBA2		r5, r6
	ConvRGBA2		r7, r8
	ConvRGBA2		r9, r10

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)
	bcs				rgba_main_loop

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
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_ABGR8888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ABGR8888_to_BGRA8888_ARMv6
	push			{r4-r10,r14}

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

	// Preload source data
	pld				[r1, #16*4]
abgr_main_loop:

	ldmia			r1!, {r3-r10}				// Load src0..src7 (RRGGBBAA)
	subs			r2, r2, #8

	ConvABGR2		r3, r4

	// Preload source data
	pld				[r1, #16*4]

	ConvABGR2		r5, r6
	ConvABGR2		r7, r8
	ConvABGR2		r9, r10

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)
	bcs				abgr_main_loop

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
	pop				{r4-r10,r14}
FUNC_EPILOGUE


// void Convert_ARGB8888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ARGB8888_to_BGRA8888_ARMv6
	push			{r4-r9}

	// Align s	ource to a cache line boundary
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

	// Preload source data
	pld				[r1, #16*4]
argb_main_loop:

	ldmia			r1!, {r3-r9,r12}			// Load src0..src7 (BBGGRRAA)
	subs			r2, r2, #8
	ConvARGB		r3
	ConvARGB		r4
	ConvARGB		r5

	// Preload source data
	pld				[r1, #16*4]

	ConvARGB		r6
	ConvARGB		r7
	ConvARGB		r8
	ConvARGB		r9
	ConvARGB		r12
	stmia			r0!, {r3-r9,r12}			// Store to dst0..dst7 (AARRGGBB)
	bcs				argb_main_loop

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
	pop				{r4-r9}
FUNC_EPILOGUE

// void Convert_BGRA4444_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_BGRA4444_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
bgra16_head_loop:
	beq				bgra16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				bgra16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (....ARGB)
	subs			r2, #1						// num--
	ConvBGRA4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				bgra16_head_loop

bgra16_aligned:
	subs			r2, r2, #8
	bcc				bgra16_tail

bgra16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	ldmia			r1!, {r3-r6}				// Load src0..src7 (ARGB,ARGB)

	// Convert 8 source pixels (r3-r6) to 8 destination pixels (r3-r10).
	// Note: r2 is clobbered by the conversion macro, so we save it on the stack
	// (which is actually a free operation in this case, since it's "hidden" by
	// other pending operations).
	push			{r2}
	ConvBGRA4444x8
	pop				{r2}

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				bgra16_main_loop

bgra16_tail:
	adds			r2, r2, #8
	beq				bgra16_done

bgra16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (....ARGB)
	subs			r2, #1						// num--
	ConvBGRA4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				bgra16_tail_loop

bgra16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_RGBA4444_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_RGBA4444_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
rgba16_head_loop:
	beq				rgba16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				rgba16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (....ABGR)
	subs			r2, #1						// num--
	ConvRGBA4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				rgba16_head_loop

rgba16_aligned:
	subs			r2, r2, #8
	bcc				rgba16_tail

rgba16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	ldmia			r1!, {r3-r6}				// Load src0..src7 (ABGR,ABGR)

	// Convert 8 source pixels (r3-r6) to 8 destination pixels (r3-r10).
	// Note: r2 is clobbered by the conversion macro, so we save it on the stack
	// (which is actually a free operation in this case, since it's "hidden" by
	// other pending operations).
	push			{r2}
	ConvRGBA4444x8
	pop				{r2}

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				rgba16_main_loop

rgba16_tail:
	adds			r2, r2, #8
	beq				rgba16_done

rgba16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (....ABGR)
	subs			r2, #1						// num--
	ConvRGBA4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				rgba16_tail_loop

rgba16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_ABGR4444_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ABGR4444_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
abgr16_head_loop:
	beq				abgr16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				abgr16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (....RGBA)
	subs			r2, #1						// num--
	ConvABGR4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				abgr16_head_loop

abgr16_aligned:
	subs			r2, r2, #8
	bcc				abgr16_tail

abgr16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	ldmia			r1!, {r3-r6}				// Load src0..src7 (RGBA,RGBA)

	// Convert 8 source pixels (r3-r6) to 8 destination pixels (r3-r10).
	// Note: r2 is clobbered by the conversion macro, so we save it on the stack
	// (which is actually a free operation in this case, since it's "hidden" by
	// other pending operations).
	push			{r2}
	ConvABGR4444x8
	pop				{r2}

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				abgr16_main_loop

abgr16_tail:
	adds			r2, r2, #8
	beq				abgr16_done

abgr16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (....RGBA)
	subs			r2, #1						// num--
	ConvABGR4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				abgr16_tail_loop

abgr16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_ARGB4444_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_ARGB4444_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
argb16_head_loop:
	beq				argb16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				argb16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (....BGRA)
	subs			r2, #1						// num--
	ConvARGB4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				argb16_head_loop

argb16_aligned:
	subs			r2, r2, #8
	bcc				argb16_tail

argb16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	ldmia			r1!, {r3-r6}				// Load src0..src7 (BGRA,BGRA)

	// Convert 8 source pixels (r3-r6) to 8 destination pixels (r3-r10).
	// Note: r2 is clobbered by the conversion macro, so we save it on the stack
	// (which is actually a free operation in this case, since it's "hidden" by
	// other pending operations).
	push			{r2}
	ConvARGB4444x8
	pop				{r2}

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				argb16_main_loop

argb16_tail:
	adds			r2, r2, #8
	beq				argb16_done

argb16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (....BGRA)
	subs			r2, #1						// num--
	ConvARGB4444	r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				argb16_tail_loop

argb16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_RGB565_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_RGB565_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
rgb16_head_loop:
	beq				rgb16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				rgb16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (................rrrrrggggggbbbbb)
	subs			r2, #1						// num--
	ConvRGB565		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				rgb16_head_loop

rgb16_aligned:
	subs			r2, r2, #8
	bcc				rgb16_tail

rgb16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Convert 8 source pixels to 8 destination pixels (r3-r10).
	ldrh			r3, [r1]
	ldrh			r4, [r1, #2]
	ConvRGB565		r3
	ldrh			r5, [r1, #4]
	ConvRGB565		r4
	ldrh			r6, [r1, #6]
	ConvRGB565		r5
	ldrh			r7, [r1, #8]
	ConvRGB565		r6
	ldrh			r8, [r1, #10]
	ConvRGB565		r7
	ldrh			r9, [r1, #12]
	ConvRGB565		r8
	ldrh			r10, [r1, #14]
	ConvRGB565		r9
	ConvRGB565		r10
	add				r1, r1, #16

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				rgb16_main_loop

rgb16_tail:
	adds			r2, r2, #8
	beq				rgb16_done

rgb16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (................rrrrrggggggbbbbb)
	subs			r2, #1						// num--
	ConvRGB565		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				rgb16_tail_loop

rgb16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE


// void Convert_BGR565_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_BGR565_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a cache line boundary
	cmp				r2, #0
bgr16_head_loop:
	beq				bgr16_done					// If cnt == 0...
	ands			r12, r1, #31				// Are we aligned yet?
	beq				bgr16_aligned
	ldrh			r3, [r1], #2				// r3 = *src++ (................bbbbbggggggrrrrr)
	subs			r2, #1						// num--
	ConvBGR565		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				bgr16_head_loop

bgr16_aligned:
	subs			r2, r2, #8
	bcc				bgr16_tail

bgr16_main_loop:
	// Preload source data
	pld				[r1, #16*4]

	// Convert 8 source pixels to 8 destination pixels (r3-r10).
	ldrh			r3, [r1]
	ldrh			r4, [r1, #2]
	ConvBGR565		r3
	ldrh			r5, [r1, #4]
	ConvBGR565		r4
	ldrh			r6, [r1, #6]
	ConvBGR565		r5
	ldrh			r7, [r1, #8]
	ConvBGR565		r6
	ldrh			r8, [r1, #10]
	ConvBGR565		r7
	ldrh			r9, [r1, #12]
	ConvBGR565		r8
	ldrh			r10, [r1, #14]
	ConvBGR565		r9
	ConvBGR565		r10
	add				r1, r1, #16

	stmia			r0!, {r3-r10}				// Store to dst0..dst7 (AARRGGBB)

	subs			r2, r2, #8
	bcs				bgr16_main_loop

bgr16_tail:
	adds			r2, r2, #8
	beq				bgr16_done

bgr16_tail_loop:
	ldrh			r3, [r1], #2				// r3 = *src++ (................bbbbbggggggrrrrr)
	subs			r2, #1						// num--
	ConvBGR565		r3
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				bgr16_tail_loop

bgr16_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE

// void Convert_RGB888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_RGB888_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a word boundary
	cmp				r2, #0
rgb24_head_loop:
	beq				rgb24_done					// If cnt == 0...
	ands			r12, r1, #3					// Are we aligned yet?
	beq				rgb24_aligned
	ConvRGB888		r3, r1, r2					// Convert one pixel from [r1] into r3, and decrement num (r2)
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				rgb24_head_loop

rgb24_aligned:
	subs			r2, r2, #8
	bcc				rgb24_tail

rgb24_main_loop:
	// Load src0..src7 (8*3 bytes = 6 words)
	ldmia			r1, {r3-r8}

	// Preload source data
	pld				[r1, #24*4]
	add				r1, r1, #8*3

	// Convert 8 source pixels to 8 destination pixels
	ConvRGB888x8

	// Store to dst0..dst7 (AARRGGBB)
	stmia			r0!, {r5-r12}

	subs			r2, r2, #8
	bcs				rgb24_main_loop

rgb24_tail:
	adds			r2, r2, #8
	beq				rgb24_done

rgb24_tail_loop:
	ConvRGB888		r3, r1, r2					// Convert one pixel from [r1] into r3, and decrement num (r2)
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				rgb24_tail_loop

rgb24_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE

// void Convert_BGR888_to_BGRA8888_ARMv6(void* dst, const void* src, unsigned num);
//  r0 = dst
//  r1 = src
//  r2 = num
FUNC_PROLOGUE_ARM Convert_BGR888_to_BGRA8888_ARMv6
	push			{r4-r11,r14}

	// Align source to a word boundary
	cmp				r2, #0
bgr24_head_loop:
	beq				bgr24_done					// If cnt == 0...
	ands			r12, r1, #3					// Are we aligned yet?
	beq				bgr24_aligned
	ConvBGR888		r3, r1, r2					// Convert one pixel from [r1] into r3, and decrement num (r2)
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	b				bgr24_head_loop

bgr24_aligned:
	subs			r2, r2, #8
	bcc				bgr24_tail

bgr24_main_loop:
	// Load src0..src7 (8*3 bytes = 6 words)
	ldmia			r1, {r3-r8}

	// Preload source data
	pld				[r1, #24*4]
	add				r1, r1, #8*3

	// Convert 8 source pixels to 8 destination pixels
	ConvBGR888x8

	// Store to dst0..dst7 (AARRGGBB)
	stmia			r0!, {r5-r12}

	subs			r2, r2, #8
	bcs				bgr24_main_loop

bgr24_tail:
	adds			r2, r2, #8
	beq				bgr24_done

bgr24_tail_loop:
	ConvBGR888		r3, r1, r2					// Convert one pixel from [r1] into r3, and decrement num (r2)
	str				r3, [r0], #4				// *dst++ = r3 (AARRGGBB)
	bne				bgr24_tail_loop

bgr24_done:
	pop				{r4-r11,r14}
FUNC_EPILOGUE

#endif // ARCHITECTURE_ARM
