/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: architecture identification.
 */
#ifndef POSIX_SYS_CHOICE_CHIPSET_H
#define POSIX_SYS_CHOICE_CHIPSET_H __FILE__
/** \file choice_chipset.h Disocver grungy details of architecture.
 *
 * Sets assorted \c SYSTEM_* defines related to the target architecture for
 * which we're compiling. Assumes that preprocessor symbols provided by the
 * compiler suffice.  If you need to over-ride any of these with a
 * \c \#undef/\#define in your \c system.h, please let the module owner know -
 * we may be able to work out a suitable symbol on which to do
 * <code>\#if</code>-ery.
 *
 * @note hardcore's operasetup.py uses its own preprocessor, which doesn't
 * define the symbols provided by the compiler.  If any feature, tweak or api
 * depends on any of the system settings defined here, the preprocessor symbols
 * that control these settings need to be exposed to hardcore's operasetup.py
 * script. Users of the make files in platforms/build can set
 * HARDCORE_CPP_COMPATIBILITY to YES: see
 * <a href="../../../build/documentation/project.html#HARDCORE_CPP_COMPATIBILITY">its
 * documentation</a>.
 */

/* Known 64-bit architectures: */
# if defined(__LP64__) || defined(_LP64)
#define SYSTEM_64BIT					YES
# else
#define SYSTEM_64BIT					NO
# endif

/* Architecture (for Carakan's JIT).
 *
 * Note: when compiling with gcc -Wundef, we get warnings (in every compilation
 * unit, since this file is part of the pch.h cascade) if we condition on an
 * undefined symbol, even though it's only going to be tested in #if-ery, where
 * it'll be treated as having value zero; hence the splits below with #ifndef
 * setting NO and its #else clause using a conditional.
 */
# ifndef __arm__
#define SYSTEM_ARCHITECTURE_ARM			NO
# else
#define SYSTEM_ARCHITECTURE_ARM			(__arm__ ? YES : NO)
# endif /* __arm__ */

# ifndef __mips__
#define SYSTEM_ARCHITECTURE_MIPS		NO
# else
#define SYSTEM_ARCHITECTURE_MIPS		(__mips__ ? YES : NO)
# endif /* __mips__ */

# ifndef __powerpc
#define SYSTEM_ARCHITECTURE_POWERPC		NO
# else
#define SYSTEM_ARCHITECTURE_POWERPC		(__powerpc ? YES : NO)
# endif /* __powerpc */

# ifndef __sh__
#define SYSTEM_ARCHITECTURE_SUPERH		NO
# else
#define SYSTEM_ARCHITECTURE_SUPERH		(__sh__ ? YES : NO)
# endif /* __sh__ */

# if defined(__i386) || defined (__x86_64)
#define SYSTEM_ARCHITECTURE_IA32		YES
# else
#define SYSTEM_ARCHITECTURE_IA32		NO
# endif /* i386 or x86_64 */

/* Floating point format: */
#define SYSTEM_IEEE_DENORMALS 			YES
#define SYSTEM_NAN_ARITHMETIC_OK		YES

#if defined(__mips__) || defined(__hppa__)
#define SYSTEM_QUIET_NAN_IS_ONE			NO
#else
#define SYSTEM_QUIET_NAN_IS_ONE			YES
#endif

/* Use platforms/build/query/nandiscern.c to find out what to set. */
#define SYSTEM_NAN_EQUALS_EVERYTHING	NO /* the common case */

# if defined(__arm__) || defined(__mips__) || \
	defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || defined(__sh__)
#define SYSTEM_RISC_ALIGNMENT			YES
# else
#define SYSTEM_RISC_ALIGNMENT			NO
# endif /* RISC-aligned architectures */
#endif /* POSIX_SYS_CHOICE_CHIPSET_H */
