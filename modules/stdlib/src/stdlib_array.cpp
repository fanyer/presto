/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_array.cpp Array functions */

#include "core/pch.h"

extern "C" {

#define ELEM(base,size,n)  ((void*)((char*)(base) + (size)*(n)))

#if !defined(HAVE_QSORT) || !defined(HAVE_QSORT_S)

/* IMPLEMENTATION NOTE: We do not want qsort to allocate memory, because
   happy-malloc gets confused when that happens.

   PERFORMANCE NOTE: The exchange has been optimized for sizes 4 (32-bit)
   and 8 (64-bit). Stdlib's op_memcpy() should be sufficiently optimized
   to handle other sizes appropriately. Since op_qsort ensures size
   to be > 0 we only do the size check at the end of the loop. */

static void exchcpy(void *base, size_t size, int fst, int snd)
{
	char *fstp = reinterpret_cast<char *>(ELEM(base,size,fst));
	char *sndp = reinterpret_cast<char *>(ELEM(base,size,snd));
	UINT32 buf[256 / sizeof(UINT32)]; /* ARRAY OK 2009-06-17 molsson */

	do
	{
		size_t k = MIN(size, sizeof(buf));
		op_memcpy( buf, fstp, k );
		op_memcpy( fstp, sndp, k );
		op_memcpy( sndp, buf, k );
		size -= k;
		fstp += k;
		sndp += k;
	} while (size > 0);
}

static void exchmov(void *base, size_t size, int fst, int snd)
{
	UINT32 *fstp = reinterpret_cast<UINT32 *>(ELEM(base,size,fst));
	UINT32 *sndp = reinterpret_cast<UINT32 *>(ELEM(base,size,snd));

	UINT32 copy = *fstp;
	*fstp = *sndp;
	*sndp = copy;

	if (size == 8)
	{
		copy = *(fstp + 1);
		*(fstp + 1) = *(sndp + 1);
		*(sndp + 1) = copy;
	}
}

#endif // HAVE_QSORT || HAVE_QSORT_S

#ifndef HAVE_QSORT

#define QSORT_COMP_FUNC_LEADING_ARGS
#define QSORT_COMP_FUNC_TRAILING_ARGS
#define QSORT_COMP_FUNC_CALL qsort_internal
static void QSORT_COMP_FUNC_CALL(void* base, int l, int r, size_t size, int (*compar)(const void*, const void*))
{
#include "modules/stdlib/src/stdlib_qsort_body.inc"
}
#undef  QSORT_COMP_FUNC_LEADING_ARGS
#undef  QSORT_COMP_FUNC_TRAILING_ARGS
#undef  QSORT_COMP_FUNC_CALL


void op_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*))
{
	if (size > 0)
		qsort_internal(base, 0, (int)nmemb-1, size, compar);
}

#endif // HAVE_QSORT

#ifndef HAVE_QSORT_S

#define QSORT_COMP_FUNC_LEADING_ARGS arg, 
#define QSORT_COMP_FUNC_TRAILING_ARGS , arg
#define QSORT_COMP_FUNC_CALL qsort_s_internal
static void QSORT_COMP_FUNC_CALL(void* base, int l, int r, size_t size, int (*compar)(void*, const void*, const void*), void* arg)
{
#include "modules/stdlib/src/stdlib_qsort_body.inc"
}
#undef  QSORT_COMP_FUNC_LEADING_ARGS
#undef  QSORT_COMP_FUNC_TRAILING_ARGS
#undef  QSORT_COMP_FUNC_CALL

void op_qsort_s(void* base, size_t nmemb, size_t size, int (*compar)(void*, const void*, const void*), void* arg)
{
	if (size > 0)
		qsort_s_internal(base, 0, (int)nmemb-1, size, compar, arg);
}

#endif // HAVE_QSORT_S

#ifndef HAVE_BSEARCH

void* op_bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*))
{
	/* Use int and not size_t for 'hi' and 'lo', since 'hi' may become -1 if 
	   the key is smaller than every element in the array */
	int lo = 0;
	int hi = (int)nmemb-1;

	while (hi >= lo)
	{
		int mid = (hi + lo) / 2;
		int res = compar(key, ELEM(base,size,mid));
		if (res < 0)
			hi = mid - 1;
		else if (res > 0)
			lo = mid + 1;
		else
			return ELEM(base,size,mid);
	}
	return NULL;
}

#endif // HAVE_BSEARCH

#ifdef SELFTEST

int stdlib_test_intcmp( const void* a, const void* b ) 
{
	return *(int*)a - *(int*)b;
}
int stdlib_test_intcmp_arg_unused( void* arg, const void* a, const void* b ) { return stdlib_test_intcmp(a, b); }

int stdlib_test_doublecmp( const void* a, const void* b ) 
{
	double ad = *(double*)a;
	double bd = *(double*)b;

	if (ad < bd)		return -1;
	else if (ad > bd)	return 1;
	else				return 0;
}
int stdlib_test_doublecmp_arg_unused( void* arg, const void* a, const void* b ) { return stdlib_test_doublecmp(a, b); }

int stdlib_test_charcmp( const void* a, const void* b ) 
{
	return *(char*)a - *(char*)b;
}
int stdlib_test_charcmp_arg_unused( void* arg, const void* a, const void* b ) { return stdlib_test_charcmp(a, b); }

int stdlib_test_intcmp_arg( void* arg, const void* a, const void* b )
{
	unsigned ai = *(unsigned*)a;
	unsigned bi = *(unsigned*)b;

	unsigned* data = (unsigned*)arg;
	return data[ai] - data[bi];
}

#endif

} // extern "C"
