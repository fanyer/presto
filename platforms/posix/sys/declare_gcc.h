/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, #ifndef symbol to be defined.
 */
#ifndef POSIX_SYS_DECLARE_GCC_H
#define POSIX_SYS_DECLARE_GCC_H __FILE__

# if SYSTEM_FORCE_INLINE == YES
#define op_force_inline inline __attribute__((always_inline))
# endif

# if SYSTEM_INT64 == YES
#  ifndef INT64_MIN
#define INT64_MIN ((INT64)(((UINT64)1)<<63))
#  endif
#  ifndef INT64_MAX
#define INT64_MAX ((INT64)(~(((UINT64)1)<<63)))
#  endif
# endif /* SYSTEM_INT64 */

# if SYSTEM_UINT64 == YES
#  ifndef UINT64_MAX /* if __STDC_LIMIT_MACROS didn't get it for us from <limits.h> */
#define UINT64_MAX (~((UINT64)0))
#  endif /* needed in adjunct/m2/ by p2p/dl-base.h */
# endif /* SYSTEM_UINT64 */

# if SYSTEM_INT64_LITERAL == YES
#  ifndef OP_INT64
#define OP_INT64(x) x ## LL
#  endif
# endif // SYSTEM_INT64_LITERAL

# if SYSTEM_UINT64_LITERAL == YES
#  ifndef OP_UINT64
#define OP_UINT64(x) x ## ULL
#  endif
# endif // SYSTEM_INT64_LITERAL

#endif /* POSIX_SYS_DECLARE_GCC_H */
