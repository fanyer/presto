/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, lea_malloc config.
 */
#ifndef POSIX_SYS_DECLARE_LIBC_H
#define POSIX_SYS_DECLARE_LIBC_H __FILE__
/** @file declare_libc.h Opera system-system configuration for C standard library support.
 *
 * This simply maps everything in ANSI C's standard library to the relevant op_*
 * define.  It is to be expected that it can be generally used by most platforms
 * in practice, due to the ubiquity of ANSI C support.
 */

# if SYSTEM_ABS == YES
#define op_abs(n)					abs(n)
# endif
# if SYSTEM_ACOS == YES
#define op_acos(x)					acos(x)
# endif
# if SYSTEM_ASIN == YES
#define op_asin(x)					asin(x)
# endif
# if SYSTEM_ATAN == YES
#define op_atan(x)					atan(x)
# endif
# if SYSTEM_ATAN2 == YES
#define op_atan2(y, x)				atan2(y, x)
# endif
# if SYSTEM_ATOI == YES
#define op_atoi(s)					atoi(s)
# endif
# if SYSTEM_BSEARCH == YES
#define op_bsearch(k, b, n, s, c)	bsearch(k, b, n, s, c)
# endif
# if SYSTEM_CEIL == YES
#define op_ceil(x)					ceil(x)
# endif
# if SYSTEM_COS == YES
#define op_cos(x)					cos(x)
# endif
# if SYSTEM_EXP == YES
#define op_exp(x)					exp(x)
# endif
# if SYSTEM_FABS == YES
#define op_fabs(x)					fabs(x)
# endif
# if SYSTEM_FLOOR == YES
#define op_floor(x)					floor(x)
# endif
# if SYSTEM_FMOD == YES
#define op_fmod(n, d)				fmod(n, d)
# endif
# if SYSTEM_FREXP == YES
#define op_frexp(x, r)				frexp(x, r)
# endif
# if SYSTEM_GMTIME == YES
#define op_gmtime(c)				gmtime(c)
# endif
# if SYSTEM_LDEXP == YES
#define op_ldexp(s, e)				ldexp(s, e)
# endif
/* no op_localeconv ! */
# if SYSTEM_LOCALTIME == YES
#define op_localtime(c)				localtime(c)
# endif
# if SYSTEM_LOG == YES
#define op_log(x)					log(x)
# endif
# if SYSTEM_MALLOC == YES
#  if defined(HAVE_DL_MALLOC) && defined(USE_DL_PREFIX)
#define op_lowlevel_malloc(n)		dlmalloc(n)
#define op_lowlevel_free(p)			dlfree(p)
#define op_lowlevel_calloc(n, s)	dlcalloc(n, s)
#define op_lowlevel_realloc(p, n)	dlrealloc(p, n)
#  else
#define op_lowlevel_malloc(n)		malloc(n)
#define op_lowlevel_free(p)			free(p)
#define op_lowlevel_calloc(n, s)	calloc(n, s)
#define op_lowlevel_realloc(p, n)	realloc(p, n)
#  endif /* Doug Lea's malloc, with prefix */
# endif /* SYSTEM_MALLOC */
# if SYSTEM_MEMCMP == YES
#define op_memcmp(s, t, n)			memcmp(s, t, n)
# endif
# if SYSTEM_MEMCPY == YES
#define op_memcpy(t, f, n)			memcpy(t, f, n)
# endif
# if SYSTEM_MEMMOVE == YES
#define op_memmove(t, f, n)			memmove(t, f, n)
# endif
# if SYSTEM_MEMSET == YES
#define op_memset(s, c, n)			memset(s, c, n)
# endif
# if SYSTEM_MKTIME == YES
#define op_mktime(t)				mktime(t)
# endif
# if SYSTEM_MODF == YES
#define op_modf(x, r)				modf(x, r)
# endif
# if SYSTEM_POW == YES
#define op_pow(a, b)				pow(a, b)
# endif /* SYSTEM_POW */
# if SYSTEM_QSORT == YES
#define op_qsort(b, n, s, c)		qsort(b, n, s, c)
# endif
# if SYSTEM_QSORT_S == YES
#  if defined(__FreeBSD__)
/* FreeBSD's qsort_r() uses a different argument order than the
 * MS-style qsort_s().  We need to transpose the last two
 * arguments: */
#define op_qsort_s(b, n, s, c, d) qsort_r(b, n, s, d, c)
#  else /* qsort_r() was added to glibc ni version 2.8. */
/* We use the MS-style qsort_s() singature, as it's more widely copied.  Since
 * glibc chose differently, we have to do a bit of work to re-pack for it; its
 * comparison function takes the context pointer last, rather than first.
 * For glibc older than 2.8, there is no known way to support op_qsort_s().
 * Patches welcome.
 */
struct posix2MS_qsort_wrapper {
	void * context;
	int (*cmp)(void*, const void*, const void*); /* The MS-style signature */
};
/* The glibc-style signature: */
static inline int posix2MS_qsort_unwrap(const void* one, const void* two, void* data)
{
	struct posix2MS_qsort_wrapper *ctx = (struct posix2MS_qsort_wrapper *)data;
	return ctx->cmp(ctx->context, one, two);
}
static inline void op_qsort_s(void *base, size_t count, size_t each,
					   int (*compar)(void *, const void *, const void *),
					   void *data)
{
	struct posix2MS_qsort_wrapper ctx = { data, compar };
	qsort_r(base, count, each, posix2MS_qsort_unwrap, &ctx);
}
#  endif /* FreeBSD, glibc-2.8 */
# endif /* SYSTEM_QSORT_S */
# if SYSTEM_RAND == YES
#define op_rand						rand
#define op_srand(s)					srand(s)
/* Consider using lrand48(void) in place of rand, seeded by srand48(long); their
 * RAND_MAX is 2**31. */
# endif
# if SYSTEM_SETJMP == YES
#define op_setjmp(e)				setjmp(e)
#define op_longjmp(e, v)			longjmp(e, v)
# endif
# if SYSTEM_SETLOCALE == YES
#define op_setlocale(c, l)			setlocale(c, l)
# endif
# if SYSTEM_SIN == YES
#define op_sin(x)					sin(x)
# endif
# if SYSTEM_SNPRINTF == YES
#define op_snprintf					snprintf /* varargs */
# endif
# if SYSTEM_SPRINTF == YES
#define op_sprintf					sprintf /* varargs */
# endif
# if SYSTEM_SQRT == YES
#define op_sqrt(x)					sqrt(x)
# endif
# if SYSTEM_SSCANF == YES
#define op_sscanf					sscanf	/* varargs */
# endif
# if SYSTEM_STRCAT == YES
#define op_strcat(t, f)				strcat(t, f)
# endif
# if SYSTEM_STRCMP == YES
#define op_strcmp					strcmp /* at least some code wants its address ! */
# endif
# if SYSTEM_STRCPY == YES
#define op_strcpy(t, f)				strcpy(t, f)
# endif
# if SYSTEM_STRCSPN == YES
#define op_strcspn(s, p)			strcspn(s, p)
# endif
# if SYSTEM_STRLEN == YES
#define op_strlen(s)				strlen(s)
# endif
# if SYSTEM_STRNCAT == YES
#define op_strncat(t, f, n)			strncat(t, f, n)
# endif
# if SYSTEM_STRNCMP == YES
#define op_strncmp(s, t, n)			strncmp(s, t, n)
# endif
# if SYSTEM_STRNCPY == YES
#define op_strncpy(t, f, n)			strncpy(t, f, n)
# endif
# if SYSTEM_STRSPN == YES
#define op_strspn(s, f)				strspn(s, f)
# endif
# if SYSTEM_TAN == YES
#define op_tan(x)					tan(x)
# endif
# if SYSTEM_TIME == YES
#define op_time(t)					time(t)
# endif
# if SYSTEM_VSNPRINTF == YES
#define op_vsnprintf(b, n, f, a)	vsnprintf(b, n, f, a)
# endif
# if SYSTEM_VSPRINTF == YES
#define op_vsprintf(b, f, a)		vsprintf(b, f, a)
# endif
# if SYSTEM_VSSCANF == YES
#define op_vsscanf(b, f, a)			vsscanf(b, f, a)
# endif
#endif /* POSIX_SYS_DECLARE_LIBC_H */
