/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// ConvRGBA - Convert a pixel in place from RGBA to BGRA.
//
// Parameters:
//  reg - source and destination register
//
// Input:
//  r11 - must be set to 0x00ff00ff
//
// Output:
//  reg - result
//
// Side effects:
//  r12
//
// Note:
//  The byte order in memory when represented as 32 bit register will be reversed
//  because of endianness.
.macro ConvRGBA reg
	and				r12, \reg, r11				// r12 = (..BB..RR)
	bic				\reg, \reg, r11				// reg = (AA..GG..)
	orr				\reg, \reg, r12, lsr #16	// reg = (AA..GGBB)
	orr				\reg, \reg, r12, asl #16	// reg = (AARRGGBB)
.endm

// ConvABGR - Convert a pixel in place from ABGR to BGRA.
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//  r12
//
// Note:
//  The byte order in memory when represented as 32 bit register will be reversed
//  because of endianness.
.macro ConvABGR reg
	mov				r12, \reg, asl #24			// r12 = (AA......)
	orr				\reg, r12, \reg, lsr #8		// reg = (AARRGGBB)
.endm

// ConvARGB - Convert a pixel in place from ARGB to BGRA.
//
// Parameters:
//  reg - source and destination register
//
// Output:
//  reg - result
//
// Side effects:
//   none
//
// Note:
//  The byte order in memory when represented as 32 bit register will be reversed
//  because of endianness.
.macro ConvARGB reg
	rev				\reg, \reg					// reg = (AARRGGBB)
.endm

