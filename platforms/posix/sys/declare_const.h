/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, WCHAR_IS_UNICHAR, TYPE_SYSTEMATIC,
 * __cplusplus, #ifndef symbol to be defined.
 *
 * Sort-order: alphabetic, but the op_strto* sort after all others (because
 * their inline forms are more complex). */
#ifndef POSIX_SYS_DECLARE_CONST_H
#define POSIX_SYS_DECLARE_CONST_H __FILE__
/** @file declare_const.h Assorted string functions that violate constness.
 *
 * Some standard library string functions (memchr, strchr, strrchr, strstr,
 * strpbrk and extensions based on them) return a char* which is really an
 * offset from a const char* parameter.  This is done because C doesn't have
 * overloading: the function has to be able to accept a const char*, so the
 * relevant parameter must be declared as such; but, if it's given a plain
 * char*, the caller wants a plain char* back from it.  All of these functions
 * should have been defined to return an integer offset, which one then adds to
 * the parameter in question to get a suitably-typed pointer to a suitable
 * location (end of string on failure).  However, it's too late to fix that; but
 * we can work round it in C++ ...
 *
 * Likewise, the functions to parse a string as a number have a second parameter
 * which gets set to point to the end of the portion of the string read; again,
 * its type lacks const qualification, although the string into which it points
 * was const qualified.  Again, a pointer to integer offset would have been
 * better design, but we can code round it.  Worse, trying to pass const char **
 * to this non-const end parameter actually violates constness rules (hence the
 * need for the end variable in each const variant following).
 */

# if defined(TYPE_SYSTEMATIC) && defined(__cplusplus) /* TODO: sort this out */
#  if SYSTEM_GETENV == YES
inline const char* op_getenv(const char* name) { return getenv(name); }
#define op_getenv(name)			op_getenv(name)
#  endif

#  if SYSTEM_MEMCHR == YES
inline const void *op_memchr(const void *s, int c, size_t n) { return memchr(s,c,n); }
inline void *op_memchr(void *s, int c, size_t n) { return memchr(s,c,n); }
#define op_memchr(s, c, n)		op_memchr(s, c, n)
#  endif

#  if SYSTEM_UNI_MEMCHR == YES
inline const uni_char *uni_memchr(const uni_char *s, uni_char c, size_t n) { return wmemchr(s,c,n); }
inline uni_char *uni_memchr(uni_char *s, uni_char c, size_t n) { return wmemchr(s,c,n); }
#define uni_memchr(s, c, n)		uni_memchr(s, c, n)
#  endif

#  if SYSTEM_MEMMEM == YES
inline const void *op_memmem(const void *h, size_t hl, const void *n, size_t nl) { return memmem(h, hl, n, nl); }
inline void *op_memmem(void *h, size_t hl, const void *n, size_t nl) { return memmem(h, hl, n, nl); }
#define op_memmem(s, n, f, m)	op_memmem(s, n, f, m)
#  endif

#  if SYSTEM_STRCHR == YES
inline const char *op_strchr(const char *s, int c) { return strchr(s, c); }
inline char *op_strchr(char *s, int c) { return strchr(s, c); }
#define op_strchr(s, c)			op_strchr(s, c)
#  endif

#  if SYSTEM_STRPBRK == YES
inline const char *op_strpbrk(const char *t, const char *s) { return strpbrk(t, s); }
inline char *op_strpbrk(char *t, const char *s) { return strpbrk(t, s); }
#define op_strpbrk(t, s)		op_strpbrk(t, s)
#  endif

#  if SYSTEM_STRRCHR == YES
inline const char *op_strrchr(const char *s, int c) { return strrchr(s, c); }
inline char *op_strrchr(char *s, int c) { return strrchr(s, c); }
#define op_strrchr(s, c)		op_strrchr(s, c)
#  endif

#  if SYSTEM_STRSTR == YES
inline const char *op_strstr(const char *text, const char *sought) { return strstr(text, sought); }
inline char *op_strstr(char *text, const char *sought) { return strstr(text, sought); }
#define op_strstr(t, s)			op_strstr(t, s)
#  endif

#  ifdef WCHAR_IS_UNICHAR
#   if SYSTEM_UNI_STRCHR == YES && !defined(uni_strchr)
inline const uni_char *uni_strchr(const uni_char* s, uni_char c) { return wcschr(s, c); }
inline uni_char *uni_strchr(uni_char* s, uni_char c) { return wcschr(s, c); }
#define uni_strchr(s, c)		uni_strchr(s, c)
#   endif

/* Has no non-uni op_* variant: */
#   if SYSTEM_UNI_STRISTR == YES && !defined(real_uni_stristr)
inline const uni_char *real_uni_stristr(const uni_char *h, const uni_char *n) { return wcsistr(h, n); }
inline uni_char *real_uni_stristr(uni_char *h, const uni_char *n) { return wcsistr(h, n); }
#define real_uni_stristr(h, n)	real_uni_stristr(h, n)
#   endif

#   if SYSTEM_UNI_STRPBRK == YES && !defined(uni_strpbrk)
inline const uni_char* uni_strpbrk(const uni_char *t, const uni_char *s) { return wcspbrk(t, s); }
inline uni_char* uni_strpbrk(uni_char *t, const uni_char *s) { return wcspbrk(t, s); }
#define uni_strpbrk(t, s)		uni_strpbrk(t, s)
#   endif

#   if SYSTEM_UNI_STRRCHR == YES && !defined(uni_strrchr)
inline const uni_char *uni_strrchr(const uni_char *s, uni_char c) { return wcsrchr(s, c); }
inline uni_char *uni_strrchr(uni_char* s, uni_char c) { return wcsrchr(s, c); }
#define uni_strrchr(s, c)		uni_strrchr(s, c)
#   endif

#   if SYSTEM_UNI_STRSTR == YES && !defined(real_uni_strstr)
inline const uni_char* real_uni_strstr(const uni_char *h, const uni_char *n) { return wcsstr(h, n); }
inline uni_char* real_uni_strstr(uni_char *h, const uni_char *n) { return wcsstr(h, n); }
#define real_uni_strstr(h, n)	real_uni_strstr(h, n)
#   endif
#  endif /* WCHAR_IS_UNICHAR */

#  if SYSTEM_STRTOD == YES
inline double op_strtod(const char *n, const char **e)
{
	char *end = 0;
	double res = strtod(n, &end);
	if (e)
		*e = end;
	return res;
}
inline double op_strtod(char *n, char **e) { return strtod(n, e); }
#define op_strtod(s, e)			op_strtod(s, e)
#  endif /* SYSTEM_STRTOD */

#  if SYSTEM_STRTOL == YES
inline long op_strtol(const char *s, const char **e, int b)
{
	char *end = 0;
	long res = strtol(s, &end, b);
	if (e)
		*e = end;
	return res;
}
inline long op_strtol(char *s, char **e, int b) { return strtol(s, e, b); }
#define op_strtol(s, e, r)		op_strtol(s, e, r)
#  endif /* SYSTEM_STRTOL */

#  if SYSTEM_STRTOUL == YES
inline unsigned long op_strtoul(const char *s, const char **e, int b)
{
	char *end = 0;
	unsigned long res = strtoul(s, &end, b);
	if (e)
		*e = end;
	return res;
}
inline unsigned long op_strtoul(char *s, char **e, int b) { return strtoul(s, e, b); }
#define op_strtoul(s, e, r)		op_strtoul(s, e, r)
#  endif /* SYSTEM_STRTOUL */
# else /* TYPE_SYSTEMATIC && C++ */
#  if SYSTEM_GETENV == YES
#define op_getenv(name)			getenv(name)
#  endif
#  if SYSTEM_MEMCHR == YES
#define op_memchr(s, c, n)		memchr(s, c, n)
#  endif
#  if SYSTEM_UNI_MEMCHR == YES
#define uni_memchr(s, c, n)		wmemchr(s, c, n)
#  endif
#  if SYSTEM_MEMMEM == YES
#define op_memmem(s, n, f, m)	memmem(s, n, f, m)
#  endif
#  if SYSTEM_STRCHR == YES
#define op_strchr(s, c)			strchr(s, c)
#  endif
#  if SYSTEM_STRPBRK == YES
#define op_strpbrk(s, p)		strpbrk(s, p)
#  endif
#  if SYSTEM_STRRCHR == YES
#define op_strrchr(s, c)		strrchr(s, c)
#  endif
#  if SYSTEM_STRSTR == YES
#define op_strstr(s, f)			strstr(s, f)
#  endif
#  ifdef WCHAR_IS_UNICHAR
#   if SYSTEM_UNI_STRCHR == YES && !defined(uni_strchr)
#define uni_strchr(s, c)		wcschr(s, c)
#   endif
#   if SYSTEM_UNI_STRISTR == YES && !defined(real_uni_stristr)
#define real_uni_stristr(h, n)	wcsistr(h, n)
#   endif
#   if SYSTEM_UNI_STRPBRK == YES && !defined(uni_strpbrk)
#define uni_strpbrk(s, c)		wcspbrk(s, c)
#    endif
#   if SYSTEM_UNI_STRRCHR == YES && !defined(uni_strrchr)
#define uni_strrchr(s, c)		wcsrchr(s, c)
#   endif
#   if SYSTEM_UNI_STRSTR == YES && !defined(real_uni_strstr)
#define real_uni_strstr(h, n)	wcsstr(h, n)
#   endif
#  endif // WCHAR_IS_UNICHAR
#  if SYSTEM_STRTOD == YES
#define op_strtod(s, e)			strtod(s, e)
#  endif
#  if SYSTEM_STRTOL == YES
#define op_strtol(s, e, r)		strtol(s, e, r)
#  endif
#  if SYSTEM_STRTOUL == YES
#define op_strtoul(s, e, r)		strtoul(s, e, r)
#  endif
# endif /* TYPE_SYSTEMATIC && C++ */
#endif /* POSIX_SYS_DECLARE_CONST_H */
