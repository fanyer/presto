/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2001-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * C++ supplement to malloc.c and core-2 global object hook-up.
 */

#define COMPILING_LEA_MALLOC
#include "core/pch_system_includes.h"

#ifdef HAVE_DL_MALLOC
#include "modules/lea_malloc/lea_malloc.h"
#endif // HAVE_DL_MALLOC
#include "modules/lea_malloc/malloc.h"

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE

extern "C" void *constrained_malloc(size_t nbytes);

// operator new(size_t nbytes)
void* constrained_new(size_t nbytes)
{
#ifdef VALGRIND
	return constrained_valgrind_new(nbytes);
#else
	return constrained_malloc(nbytes);
#endif
}

# ifndef _MSC_VER // c.f. #if in malloc.h on decls.
// operator new[]()
void* constrained_anew(size_t nbytes)
{
#ifdef VALGRIND
	return constrained_valgrind_anew(nbytes);
#else
	return constrained_malloc(nbytes);
#endif // VALGRIND
}
# endif // MSVC

# ifdef VALGRIND
/* Constrained-valgrind (a version of valgrind hacked to work with constrained
 * malloc) overrides these functions instead of constrained_new() and
 * constrained_anew().
 *
 * There are two reasons for doing it this way:
 *
 * The first reason is that these functions are declared extern "C", so the
 * function names are not mangled at the object level. It makes it easier to
 * override them since we don't have to deal with different name mangling
 * schemes used by the different versions of gcc.
 *
 * The second reason only applies for anew(). When the anew bug fix is used the
 * allocation isn't performed in constrained_anew(), but in anew_bug_stage1() or
 * anew_bug_stage2(). Using a special function for constrained array allocations
 * makes it possible for valgrind to know that an allocation is an array
 * allocation regardless of where it is performed. Overriding constrained_anew()
 * itself isn't an option since in that case the whole anew bug fix would have
 * to be implemented in valgrind.
 */

void* constrained_valgrind_new(size_t nbytes)
{
	return constrained_malloc(nbytes);
}

void* constrained_valgrind_anew(size_t nbytes)
{
	return constrained_malloc(nbytes);
}
# endif // VALGRIND

#endif // CONSTRAIN_OPERA_MEMORY_USAGE
