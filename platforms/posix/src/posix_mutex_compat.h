/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef POSIX_MUTEX_COMPAT_H
#define POSIX_MUTEX_COMPAT_H

# ifdef __hpux__ // HP's almost-POSIX threads:
#define pthread_mutexattr_init(a) pthread_mutexattr_create(a)
#define pthread_mutexattr_settype(a, t) pthread_mutexattr_setkind_np(a, t)
#define pthread_mutex_init(m, a) pthread_mutex_init(m, *a)
#define pthread_mutexattr_destroy(a) pthread_mutexattr_delete(a)
#  ifdef _DEBUG // independent of OP_ASSERT
#define PTHREAD_MUTEX_NORMAL MUTEX_ERRORCHECK_NP
// Both names are guesses [Eddy/2007/Mar/29, with no HP-UX handy]
#  else
#define PTHREAD_MUTEX_NORMAL MUTEX_NORMAL_NP
#  endif
#define PTHREAD_MUTEX_RECURSIVE MUTEX_RECURSIVE_NP
# else
#  ifdef _DEBUG // independent of OP_ASSERT
#undef PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_NORMAL PTHREAD_MUTEX_ERRORCHECK
#  endif
# endif // __hpux__

#endif // POSIX_MUTEX_COMPAT_H
