/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: gcc/glibc version.
 * Sort order: alphabetic among the simple, ad hoc for the conditional.
 */
#ifndef POSIX_SYS_CHOICE_GCC_H
#define POSIX_SYS_CHOICE_GCC_H __FILE__
/** \file choice_gcc.h system config relevant to gcc and glibc
 *
 * This file enables some SYSTEM_* defines that gcc and glibc ensure we do have
 * available.  It also ensures __GLIBC_PREREQ is defined, even when it's an
 * anachronism, and implements work-arounds for some known historical bugs.
 */

#define SYSTEM_CONSTANT_DATA_IS_EXECUTABLE	YES
#define SYSTEM_FORCE_INLINE					YES
#define SYSTEM_INT64						YES
#define SYSTEM_INT64_LITERAL				YES
#define SYSTEM_LOCAL_TYPE_AS_TEMPLATE_ARG	NO
#define SYSTEM_LONGLONG						YES
#define SYSTEM_MEMMEM						POSIX_SYS_USE_NATIVE
#define SYSTEM_NAMESPACE					YES
#define SYSTEM_NEWA_CHECKS_OVERFLOW			NO
#define SYSTEM_TEMPLATE_ARG_DEDUCTION		YES
#define SYSTEM_UINT64						YES
#define SYSTEM_UINT64_LITERAL				YES

# ifdef __GLIBC__ /* known glibc bugs: see relevant man pages */
#  ifdef __GNU_LIBRARY__
#   if __GNU_LIBRARY__ < 5 /* __GLIBC__+5 == __GNU_LIBRARY__ when > 5. */
#undef __GLIBC_PREREQ
#define __GLIBC_PREREQ(maj, min) 0
#   endif /* __GNU_LIBRARY__ was 1 in late libc 5, undefined early */
#  endif

/* There's a known memmem bug (aside from the one worked round here) in glibc <
 * 5.1; unfortunately, GNU libc < 5.1 didn't define even __GNU_LIBRARY__, let
 * alone __GLIBC__, so we can't usefully test for it.  Fortunately, it's now an
 * antique which we can generally expect not to be using ! */
#  if !__GLIBC_PREREQ(2, 2)
/* Mishanded empty "needle" */
#define memmem(h, hl, n, nl) ((nl) > 0 ? memmem(h, hl, n, nl) : (h))
#  endif

#  if !__GLIBC_PREREQ(2, 1)
/* glibc <= 2.0.6: wrong return when insufficient space. */
#undef SYSTEM_SNPRINTF
#undef SYSTEM_VSNPRINTF
#define SYSTEM_SNPRINTF						NO
#define SYSTEM_VSNPRINTF					NO
#  endif /* glibc < 2.1 */

#  ifndef va_copy /* <stdarg.h> C99 but not C89. */
/* Some systems may have __va_copy but not va_copy ... and this seemed to help
 * with bug DSK-164341. */
#   ifdef __va_copy
#define va_copy __va_copy
#   endif
#  endif /* va_copy */
# endif /* glibc bugs */
#endif /* POSIX_SYS_CHOICE_GCC_H */
