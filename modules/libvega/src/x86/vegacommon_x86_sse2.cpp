/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)

#include "modules/libvega/src/x86/vegacommon_x86.h"

// Compiler incompatibilities to keep in mind:
// ==========================================
// * __m128i is implemented in a compiler specific way, initialization can't
//   be done in a cross platform way. Use _mm_set* to initialize them.
// * Use _mm_set* sparingly, it can generate unpredictable code if the compiler
//   is confused about data alignment.
// * MSVC can't take more than 3 __m128i arguments to a function or you'll get
//   "error C2719: 'x': format parameter with __declspec(align('16')) won't be
//   aligned"
//
// General advice:
// ==============
// * Always benchmark changes, it's not always predictable which implementation
//   method is faster.
// * Examine the generated assembler (when compiling with full optimizations
//   enabled). Compilers can be both unexpectedly good or bad at certain things
//   and results will always vary between different vendors.

VEGAXmmConstants g_xmm_constants;

void VEGAXmmConstants::Init()
{
	// Note that the initialization can't be put in a constructor since it's
	// potentially using CPU architecture specific instructions that may not
	// be available.
	_1						= _mm_set1_epi16(1);
	_256					= _mm_set1_epi16(256);
	broadcast_word			= _mm_set_epi8(1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0);
	broadcast_b2w_lo		= _mm_set_epi8(3, 4, 3, 4, 3, 4, 3, 4, 3, 0, 3, 0, 3, 0, 3, 0);
	broadcast_b2w_hi		= _mm_set_epi8(11, 12, 11, 12, 11, 12, 11, 12, 11, 8, 11, 8, 11, 8, 11, 8);
	broadcast_alpha			= _mm_set_epi8(5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3);
	broadcast_word_alpha	= _mm_set_epi8(5, 14, 5, 14, 5, 14, 5, 14, 5, 6, 5, 6, 5, 6, 5, 6);
	broadcast_src			= _mm_set_epi8(5, 3, 5, 2, 5, 1, 5, 0, 5, 3, 5, 2, 5, 1, 5, 0);
	broadcast_mask_lo		= _mm_set_epi8(5, 1, 5, 1, 5, 1, 5, 1, 5, 0, 5, 0, 5, 0, 5, 0);
	broadcast_mask_hi		= _mm_set_epi8(5, 3, 5, 3, 5, 3, 5, 3, 5, 2, 5, 2, 5, 2, 5, 2);
	bgra_pattern			= _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
	rgba_pattern			= _mm_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
	abgr_pattern			= _mm_set_epi8(12, 15, 14, 13, 8, 11, 10, 9, 4, 7, 6, 5, 0, 3, 2, 1);
	argb_pattern			= _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
}

extern "C" void VEGAInitXmmConstants(void)
{
	g_xmm_constants.Init();
}

#endif // defined(VEGA_USE_ASM) && defined(ARCHITECTURE_IA32)
