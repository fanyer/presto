/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* Notes on register usage (AAPCS & iOS ABI):
 *
 * General purpose registers:
 *  r0-r3             Arguments/scratch registers (no need to preserve)
 *  r4-r11,r14        Free to use (need to be preserved)
 *  r12               Scratch register (no need to preserve)
 *
 * Special uses of registers:
 *  r7                Stack frame pointer (for debugging/profiling)
 *  r9                Must not be used on iOS 2.x (OS reserved register)
 *  r14               Link register (LR), holding call return address
 *
 * VFP/NEON registers:
 *  d0-d7 (q0-q3)     Scratch registers (no need to preserve)
 *  d8-d15 (q4-q7)    Free to use (need to be preserved)
 *  d16-d31 (q8-q15)  Scratch registers (no need to preserve)
 */

#ifndef VEGACOMMON_ARM_H
#define VEGACOMMON_ARM_H

#include "modules/libvega/src/arm/vegaconfig_arm.h"

#if defined(ARCHITECTURE_ARM)

// Note that only 4 arguments are allowed. For more, set up a stack frame after the 4th.
.macro FUNC_PROLOGUE_ARM func_name
	.align 6						// Align the code to a 64 (2^6) byte boundary.
	.arm							// Use ARM instruction set.
#ifdef __ELF__
	.globl \func_name				// extern "C" name.
	.hidden \func_name				// don't export from .so files.
\func_name\():						// Function address.
#else
	.globl _\func_name				// extern "C" name.
_\func_name\():						// Function address.
#endif
.endm

.macro FUNC_EPILOGUE
	bx			lr					// Return to caller.
.endm

#endif // ARCHITECTURE_ARM

#endif // VEGACOMMON_ARM_H
