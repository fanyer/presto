/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_HANDY_H
#define MODULES_UTIL_HANDY_H

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif // ARRAY_SIZE

#if defined HAVE_TEMPLATE_ARG_DEDUCTION || defined HAVE_LOCAL_TEMPLATE_ARG_DEDUCTION
/**
 * Declare a function ARRAY_SIZE_HELPER that accepts a reference to an array
 * of @a ArraySize elements of type @a T, and returns a reference to an array
 * of @a ArraySize char elements.  Both @a ArraySize and @a T are
 * compiler-deduced, and the array element count is effectively "copied" from
 * function input to function output.  It is then finally extracted with the
 * ARRAY_SIZE macro.
 *
 * @note The function need not be defined.
 * @see ARRAY_SIZE
 * @see http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx
 */
template <typename T, size_t ArraySize>
char (&ARRAY_SIZE_HELPER(T (&array)[ArraySize]))[ArraySize];
#endif

#ifdef HAVE_TEMPLATE_ARG_DEDUCTION
/** ARRAY_SIZE(x) returns the number of elements in the array defined by
  * the non-local type x. */
#define ARRAY_SIZE(array) sizeof(ARRAY_SIZE_HELPER(array))
#else
/** ARRAY_SIZE(x) returns the number of elements in the array defined by
  * the non-local type x. */
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif // HAVE_TEMPLATE_ARG_DEDUCTION

#ifdef LOCAL_ARRAY_SIZE
#undef LOCAL_ARRAY_SIZE
#endif // LOCAL_ARRAY_SIZE

#ifdef HAVE_LOCAL_TEMPLATE_ARG_DEDUCTION
/** LOCAL_ARRAY_SIZE(x) returns the number of elements in the array defined by
  * the local type x. */
#define LOCAL_ARRAY_SIZE(array) sizeof(ARRAY_SIZE_HELPER(array))
#else
/** LOCAL_ARRAY_SIZE(x) returns the number of elements in the array defined by
  * the local type x. */
#define LOCAL_ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif // HAVE_LOCAL_TEMPLATE_ARG_DEDUCTION

BOOL UpdateOfflineModeL(BOOL toggle);
OP_BOOLEAN UpdateOfflineMode(BOOL toggle);

#endif // !MODULES_UTIL_HANDY_H
