/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_BASE_OP_TYPES_H
#define MODULES_HARDCORE_BASE_OP_TYPES_H

/** Mouse button. */
enum MouseButton {
	MOUSE_BUTTON_1,	///< The left button
	MOUSE_BUTTON_2,	///< The right button
	MOUSE_BUTTON_3	///< The middle button
};

enum BOOL3
{
  // three state "boolean"
  MAYBE	= -1,
  NO = 0,
  YES = 1
};

#ifndef HAVE_RECT
typedef struct
{
    long left;
    long top;
    long right;
    long bottom;
} RECT;
#endif // !HAVE_RECT

#ifndef HAVE_POINT
typedef struct
{
    long x;
    long y;
} POINT;
#endif // !HAVE_POINT

#ifndef HAVE_COLORREF
typedef UINT32 COLORREF;
#endif // !HAVE_COLORREF

#ifndef HAVE_SIZE
typedef struct
{
    long cx;
    long cy;
} SIZE;
#endif // !HAVE_SIZE

/** 128-bit GUID (Globally Unique Identifier).
	The array is in network byte order (big-endian). */

typedef UINT8 OpGuid[16];

#ifndef HAVE_MAKELONG
#define MAKELONG(a, b)	((long) (((UINT16) (a)) | ((UINT32) ((UINT16) (b))) << 16))
#define HIWORD(l)		((UINT16) (((UINT32) (l) >> 16) & 0xFFFF))
#define LOWORD(l)		((UINT16) (l))
#endif // !HAVE_MAKELONG

#ifndef HAVE_RGB
#define RGB(r,g,b)		((COLORREF)(((BYTE)(r)|((UINT16)((BYTE)(g))<<8))|(((UINT32)(BYTE)(b))<<16)))

#define GetRValue(rgb)	((BYTE) (rgb))
#define GetGValue(rgb)	((BYTE) (((UINT16) (rgb)) >> 8))
#define GetBValue(rgb)	((BYTE) ((rgb) >> 16))
#endif // !HAVE_RGB

#define OP_RGBA(r,g,b,a)	(((COLORREF)(((BYTE)(r)|((UINT16)((BYTE)(g))<<8))|(((UINT32)(BYTE)(b))<<16)|(((UINT32)(BYTE)((a) >> 1 ))<<24))))
#define OP_RGB(r,g,b)		OP_RGBA(r, g, b, 255)

// VEGA switches COLORREF's R and B and uses a full byte instead of 7 bits for A. I. e. aBGR -> ARGB
#define OP_COLORREF2VEGA(rgba) ((UINT32) OP_GET_A_VALUE(rgba) << 24 | OP_GET_R_VALUE(rgba) << 16 | OP_GET_G_VALUE(rgba) << 8 | OP_GET_B_VALUE(rgba))

// Assumes that it's a colour value. Keywords can also be stored in COLORREF so that has to
// be checked before.
#define OP_GET_R_VALUE(rgb)	((BYTE) (rgb))
#define OP_GET_G_VALUE(rgb)	((BYTE) (((UINT16) (rgb)) >> 8))
#define OP_GET_B_VALUE(rgb)	((BYTE) ((rgb) >> 16))
// alpha stored in 7 bits near the top of the UINT32.
// Have to make sure 127 -> 255 and 0 -> 0 when we scale it up
// That is done by putting the high bit as the low bit instead of hardcoded 0 or 1.
#define OP_GET_A_VALUE(rgb)	((BYTE) ((((rgb) >> 23) & 0xfe) | (((rgb) >> 30) & 0x1)))

#define STRINGLENGTH(str) (sizeof(str)-1)

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif // ARRAY_SIZE

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))

#ifndef _UINT16
#define _UINT16
#endif // !_UINT16

#ifndef _UINT32
#define _UINT32
#endif // !_UINT32

#ifndef _INT16
#define _INT16
#endif // !_INT16

#ifndef _INT32
#define _INT32
#endif // !_INT32

#ifndef OP_MEMORY_VAR
# ifdef USE_DEBUGGING_OP_STATUS
#  define OP_MEMORY_VAR
# else
#  define OP_MEMORY_VAR volatile
# endif // USE_DEBUGGING_OP_STATUS
#endif // !OP_MEMORY_VAR

/**
 * Casts any integer to a pointer and vice-versa. Use with care, beware of pointer truncation.
 */
#define INT_TO_PTR(i)     reinterpret_cast<void *>(static_cast<UINTPTR>(i))
#define PTR_TO_UINTPTR(p) reinterpret_cast<UINTPTR>(p)
#define PTR_TO_INTPTR(p)  reinterpret_cast<INTPTR>(p)
#define PTR_TO_INTEGRAL(t,p) static_cast<t>(reinterpret_cast<UINTPTR>(p))
#define PTR_TO_INT(p)    PTR_TO_INTEGRAL(int,p)
#define PTR_TO_UINT(p)   PTR_TO_INTEGRAL(unsigned,p)
#define PTR_TO_LONG(p)   PTR_TO_INTEGRAL(long,p)
#define PTR_TO_ULONG(p)  PTR_TO_INTEGRAL(unsigned long,p)
#define PTR_TO_BOOL(p)   ((p) != NULL)
#ifdef HAVE_INT64
# define PTR_TO_INT64(p)  PTR_TO_INTEGRAL(INT64,p)
# define PTR_TO_UINT64(p) PTR_TO_INTEGRAL(UINT64,p)
#endif // HAVE_INT64

/** Number of digits needed to encode an unsigned integral value in decimal.
 *
 * Unsigned needs one less character than signed - the minus sign on a signed
 * value is always a possibility and the factor of two in possible magnitude
 * only affects the magnitude's length when the unsigned max value's first digit
 * is a 1 (which, in accordance with Benford's law, only happens log(2)/log(10)
 * of the time; about 30%); it works out being the sign that makes the bigger
 * difference.  So be sure to only use UINT_STRSIZE on unsigned values or types,
 * unless you're going to +1 for a sign anyway, e.g. because using the +
 * modifier in a format string.
 *
 * @note The biggest number of the expression 'what' is 2^(sizeof(what)*8)-1 =
 *  10^(sizeof(what)*8*log(2)/log(10))-1 so we need
 *  sizeof(what)*8*log(2)/log(10)+1 digits for a decimal encoding.
 *  and 53/22 is an upper bound for 8*log(2)/log(10).
 *
 * @param what An expression (usually variable) or type; only used as parameter
 * to sizeof(), so not evaluated at run-time.
 * @return An upper bound (as a compile-time integer constant) on the number of
 * characters in a decimal representation of a possible value of \p what.
 * Note that this does not include a trailing '\\0' terminator.
 */
#define UINT_STRSIZE(what) (1 + (53 * sizeof(what)) / 22)

/** Number of characters needed to encode a signed integral value in decimal.
 *
 * @param what An expression (usually variable) or type; only used as parameter
 * to sizeof(), so not evaluated at run-time.
 * @return An upper bound (as a compile-time integer constant) on the number of
 * characters (digits and optional sign) in a decimal representation of a
 * possible value of \p what.  Note that this does not include a trailing '\\0'
 * terminator.
 */
#define INT_STRSIZE(what)  (2 + (53 * sizeof(what)) / 22)

#endif // !MODULES_HARDCORE_BASE_OP_TYPES_H
