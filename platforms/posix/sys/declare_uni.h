/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, WCHAR_IS_UNICHAR, #ifndef symbol to be
 * defined.
 * Sort-order: alphabetic, ignoring any real_ prefix or _lowlevel mid-part.
 */
#ifndef POSIX_SYS_DECLARE_UNI_H
#define POSIX_SYS_DECLARE_UNI_H __FILE__
/** @file declare_uni.h Opera system-system configuration for unicode functions.
 *
 * These are POSIX extensions to libc, derived from string functions in those
 * specs in a fairly predictable way.  See also: posix_const.h for the related
 * functions that (as specified) violate const-ness; and posix_choice.h for an
 * explanation of why this won't work for most Unix systems and can't possibly
 * work unless WCHAR_IS_UNICHAR.
 *
 * The relevant POSIX documentation is at:
 * http://pubs.opengroup.org/onlinepubs/9699919799/idx/iw.html
 */

# ifdef WCHAR_IS_UNICHAR
#  if SYSTEM_UNI_SNPRINTF == YES && !defined(uni_snprintf)
#define uni_snprintf				snwprintf	/* varargs */
#  endif
#  if SYSTEM_UNI_SPRINTF == YES && !defined(uni_sprintf)
#define uni_sprintf					swprintf	/* varargs */
#  endif
#  if SYSTEM_UNI_SSCANF == YES && !defined(uni_sscanf)
#define uni_sscanf					swscanf		/* varargs */
#  endif
#  if SYSTEM_UNI_STRCAT == YES && !defined(uni_strcat)
#define uni_strcat(t, f)			wcscat(t, f)
#  endif
#  if SYSTEM_UNI_STRCMP == YES && !defined(real_uni_strcmp)
#define real_uni_strcmp(s, t)		wcscmp(s, t)
#  endif
#  if SYSTEM_UNI_STRCPY == YES && !defined(uni_strcpy)
#define uni_strcpy(t, f)			wcscpy(t, f)
#  endif
#  if SYSTEM_UNI_STRCSPN == YES && !defined(uni_strcspn)
#define uni_strcspn(s, c)			wcscspn(s, c)
#  endif
#  if SYSTEM_UNI_STRDUP == YES && !defined(uni_lowlevel_strdup)
#define uni_lowlevel_strdup(s)		wcsdup(s)
#  endif
#  if SYSTEM_UNI_STRICMP == YES && !defined(real_uni_stricmp)
#define real_uni_stricmp(s, t)		wcsicmp(s, t)
#  endif
#  if SYSTEM_UNI_STRLCAT == YES && !defined(uni_strlcat)
#define uni_strlcat(t, f, n)		wcslcat(t, f, n)
#  endif
#  if SYSTEM_UNI_STRLCPY == YES && !defined(uni_strlcpy)
#define uni_strlcpy(t, f, n)		wcslcpy(t, f, n)
#  endif
#  if SYSTEM_UNI_STRLEN == YES && !defined(uni_strlen)
#define uni_strlen(s)				wcslen(s)
#  endif
#  if SYSTEM_UNI_STRNCAT == YES && !defined(uni_strncat)
#define uni_strncat(t, f, n)		wcsncat(t, f, n)
#  endif
#  if SYSTEM_UNI_STRNCMP == YES && !defined(real_uni_strncmp)
#define real_uni_strncmp(s, t, n)	wcsncmp(s, t, n)
#  endif
#  if SYSTEM_UNI_STRNCPY == YES && !defined(uni_strncpy)
#define uni_strncpy(t, f, n)		wcsncpy(t, f, n)
#  endif
#  if SYSTEM_UNI_STRNICMP == YES && !defined(real_uni_strnicmp)
#define real_uni_strnicmp(s, t, n)	wcsnicmp(s, t, n)
#  endif
#  if SYSTEM_UNI_STRREV == YES && !defined(uni_strrev)
#define uni_strrev(s)				wcsrev(s)
#  endif
#  if SYSTEM_UNI_STRSPN == YES && !defined(uni_strspn)
#define uni_strspn(s, c)			wcsspn(s, c)
#  endif
#  if SYSTEM_UNI_STRTOK == YES && !defined(uni_strtok)
#define uni_strtok(s, c)			wcstok(s, c)
#  endif
#  if SYSTEM_UNI_VSNPRINTF == YES && !defined(uni_vsnprintf)
#define uni_vsnprintf(b, n, f, a)	vsnwprintf(b, n, f, a)
#  endif
#  if SYSTEM_UNI_VSPRINTF == YES && !defined(uni_vsprintf)
#define uni_vsprintf(b, f, a)		vswprintf(b, f, a)
#  endif
# endif /* WCHAR_IS_UNICHAR */
#endif /* POSIX_SYS_DECLARE_UNI_H */
