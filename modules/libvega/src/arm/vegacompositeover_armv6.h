/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COMPOSITEOVER_ARMV6_H
#define COMPOSITEOVER_ARMV6_H

#include "vegacommon_arm.h"

#if defined(ARCHITECTURE_ARM)

// CompOver
//
// dst - src + (256 - src.a) * dst
//
// Parameters:
//  dst - (--"--)
//  src - (must be r0 - r4, r7 - r9 or r14)
//
// Input:
//  r10 - constant mask 0x00ff00ff
//
// Output
//  dst
//
// Side effects:
//  r5, r6, r11, r12
.macro CompOver dst src
	and				r6, \src, r10			// r6  = src.rb  (..RR..BB)
	bic				r11, \src, r10			// r11 = src.ag  (AA..GG..)
	bic				r12, \dst, r10			// r12 = dst.ag  (AA..GG..)
	mov				r5, \src, lsr #24
	and				\dst, \dst, r10			// dst = dst.rb  (..RR..BB)
	rsb				r5, r5, #256			// r5  = 256 - src.a
	mov				r6, r6, asl #8			// r6  = src.rb  (RR..BB..)
	mov				r12, r12, lsr #8		// r12 = dst.ag  (..AA..GG)

	mla				r6, \dst, r5, r6		// r6  = dst.rb * (256-src.a) + src.rb
	mla				r11, r12, r5, r11		// r11 = dst.ag * (256-src.a) + src.ag

	and				r6, r10, r6, lsr #8
	bic				r11, r11, r10
	orr				\dst, r6, r11			// dst = combined result
.endm

// Mask
//
// src * mask => mrb/mag
//
// Parameters:
//  mrb - result register for red and blue
//  mag - result register for alpha and green
//  src - source register (must be r0 - r9, r11, r12 or r14)
//  mask - (--"--)
//
// Input:
//  r10 - must be 0x00ff00ff
//
// Output
//  mrb - src.rb * mask (RRrrBBbb)
//  mag - src.ag * mask (AAaaGGgg)
//
// Side effects:
//   none
.macro Mask mrb mag src mask
	and				\mrb, r10, \src			// mrb = src.rb  (..RR..BB)
	and				\mag, r10, \src, lsr #8	// mag = src.ag  (..AA..GG)
	mul				\mrb, \mrb, \mask		// mrb = src.rb * mask
	mul				\mag, \mag, \mask		// mab = src.ag * mask
.endm

// CompOverMasked
//
// out = src + (256 - src.a) * dst,  where src is stored in r6/r11 as
// produced by the Mask macro
//
// Parameters:
//  out - result register
//  dst - destination color (must be r0 - r4, r7 - r9 or r14)
//  mrb - result register for red and blue
//  mag - result register for alpha and green
//
// Input:
//  r10 - must be 0x00ff00ff
//
// Output:
//   out - resulting pixel
//
// Side effects:
//   r5, r12, mrb, mag
.macro CompOverMasked out mrb mag dst
	and				r12, r10, \dst			// r12 = dst.rb  (..RR..BB)
	and				\out, r10, \dst, lsr #8	// out = dst.ag  (..AA..GG)
	mov				r5, \mag, lsr #24
	rsb				r5, r5, #256			// r5  = 256 - src.a

	// Note: Remove the following two lines for better performance and
	// accuracy, but it'll no longer match the C reference function.
	bic				\mrb, \mrb, r10			// mrb = src.rb  (RR..BB..)
	bic				\mag, \mag, r10			// mag = src.ag  (AA..GG..)

	mla				\mrb, r12, r5, \mrb		// mrb = dst.rb * (256-src.a) + src.rb
	mla				\mag, \out, r5, \mag	// mag = dst.ag * (256-src.a) + src.ag

	and				\mrb, r10, \mrb, lsr #8
	bic				\mag, \mag, r10
	orr				\out, \mrb, \mag		// out = combined result
.endm

#endif // ARCHITECTURE_ARM

#endif // COMPOSITEOVER_ARMV6_H
