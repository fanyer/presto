/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_integer.cpp Integer operations and conversions */

#include "core/pch.h"

extern "C" {


#if !defined(HAVE_STRTOL) || !defined(HAVE_STRTOUL) || defined(STDLIB_64BIT_STRING_CONVERSIONS)

#define STRTOX_NAME op_strtox
#define STRTOX_CHAR_TYPE char
#include "modules/stdlib/src/stdlib_integer_strtol.inl"
#undef STRTOX_NAME
#undef STRTOX_CHAR_TYPE

#endif // !defined(HAVE_STRTOL) || !defined(HAVE_STRTOUL) || defined(STDLIB_64BIT_STRING_CONVERSIONS)

#ifndef HAVE_STRTOL

long op_strtol(const char* nptr, char** endptr, int radix)
{
	int signed_multiplier;
	long value = (long)op_strtox(nptr, endptr, radix, NULL, &signed_multiplier, LONG_MIN, LONG_MAX);
	return value * (long)signed_multiplier;
}

#endif // HAVE_STRTOL

#ifndef HAVE_STRTOUL

unsigned long op_strtoul(const char* nptr, char** endptr, int base)
{
	return (unsigned long)op_strtox(nptr, endptr, base, NULL, NULL, 0, ULONG_MAX);
}

#endif // HAVE_STRTOUL

#ifdef STDLIB_64BIT_STRING_CONVERSIONS

INT64 op_strtoi64(const char* nptr, char** endptr, int base)
{
	int   signed_multiplier;
	INT64 value = op_strtox(nptr, endptr, base, NULL, &signed_multiplier, INT64_MIN, INT64_MAX);
	return value * (INT64)signed_multiplier;
}

UINT64 op_strtoui64(const char* nptr, char** endptr, int base)
{
	return (UINT64)op_strtox(nptr, endptr, base, NULL, NULL, 0, UINT64_MAX);
}

#endif // STDLIB_64BIT_STRING_CONVERSIONS

#ifndef HAVE_ATOI

int op_atoi(const char* nptr)
{
	long answer = op_strtol(nptr, NULL, 10);
#ifdef SIXTY_FOUR_BIT
	if (answer >= INT_MAX)
		answer = INT_MAX;
	else if (answer <= INT_MIN)
		answer = INT_MIN;
#else // SIXTY_FOUR_BIT
	OP_STATIC_ASSERT(sizeof(int) == sizeof(long));
#endif // SIXTY_FOUR_BIT
	return (int)answer;
}

#endif // HAVE_ATOI

#ifndef HAVE_ITOA

/* itoa is not in Standard C.

   We use this definition: http://www.cplusplus.com/ref/cstdlib/itoa.html

   <blockquote>
     char * itoa( int value, char * buffer, int radix );

     Convert integer to string.
       Converts an integer value to a null-terminated string using the specified
	   radix and stores the result in the given buffer.  If radix is 10 and value
	   is negative the string is preceded by the minus sign (-).  With any other
	   radix, value is always considered unsigned.  buffer should be large enough
	   to contain any possible value: (sizeof(int)*8+1) for radix=2, i.e. 17 bytes
	   in 16-bits platforms and 33 in 32-bits platforms.
   </blockquote>
*/
char* op_itoa(int value, char* string, int radix)
{
	unsigned int uvalue;
	char *s = string;
	char *t;

	OP_ASSERT(radix >= 2 && radix <= 36);
	if (radix < 2 || radix > 36)
	{
#ifdef HAVE_ERRNO
		errno = EINVAL;
#endif
		return string;
	}

	if (value < 0 && radix == 10)
	{
		*s++ = '-';
		uvalue = (unsigned int)-value;
	}
	else
		uvalue = value;

	t = s;

	while (uvalue > 0)
	{
		*s++ = "0123456789abcdefghijklmnopqrstuvwxyz"[uvalue % radix];
		uvalue = uvalue / radix;
	}

	if (s == t)
		*s++ = '0';

	*s = 0;

	--s;
	while (s > t)
	{
		char c = *s;
		*s = *t;
		*t = c;
		--s;
		++t;
	}

	return string;
}

#endif // HAVE_ITOA

#ifndef HAVE_LTOA

/* ltoa is not in Standard C.

   We use the same definition as for itoa, above. */

char* op_ltoa(long value, char* string, int radix)
{
	unsigned long uvalue;
	char *s = string;
	char *t;

	OP_ASSERT(radix >= 2 && radix <= 36);
	if (radix < 2 || radix > 36)
	{
#ifdef HAVE_ERRNO
		errno = EINVAL;
#endif
		return string;
	}

	if (value < 0 && radix == 10)
	{
		*s++ = '-';
		uvalue = (unsigned long)-value;
	}
	else
		uvalue = value;

	t = s;

	while (uvalue > 0)
	{
		*s++ = "0123456789abcdefghijklmnopqrstuvwxyz"[uvalue % radix];
		uvalue = uvalue / radix;
	}

	if (s == t)
		*s++ = '0';

	*s = 0;

	--s;
	while (s > t)
	{
		char c = *s;
		*s = *t;
		*t = c;
		--s;
		++t;
	}

	return string;
}

#endif // HAVE_LTOA

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/* i64toa is not in Standard C.

   We use the same definition as for itoa, above. */

char* op_i64toa(INT64 value, char* string, int radix)
{
	UINT64 uvalue;
	char *s = string;
	char *t;

	OP_ASSERT(radix >= 2 && radix <= 36);
	if (radix < 2 || radix > 36)
	{
#ifdef HAVE_ERRNO
		errno = EINVAL;
#endif
		return string;
	}

	if (value < 0 && radix == 10)
	{
		*s++ = '-';
		uvalue = (UINT64)-value;
	}
	else
		uvalue = value;

	t = s;

	while (uvalue > 0)
	{
		*s++ = "0123456789abcdefghijklmnopqrstuvwxyz"[uvalue % radix];
		uvalue = uvalue / radix;
	}

	if (s == t)
		*s++ = '0';

	*s = 0;

	--s;
	while (s > t)
	{
		char c = *s;
		*s = *t;
		*t = c;
		--s;
		++t;
	}

	return string;
}

#endif // STDLIB_64BIT_STRING_CONVERSIONS

#if !defined HAVE_RAND && !defined MERSENNE_TWISTER_RNG

/* Multiply two 32-bit unsigned numbers and produce a 64-bit unsigned
 * result.  This is more general than we need for random.
 */
void stdlib_umul_32x32_to_64( unsigned int a, unsigned int b, unsigned int *hi, unsigned int *lo )
{
	unsigned int a1, a2, b1, b2, r1, r2, r3, r4, carry, x, y;

	a1 = a & 0xFFFF;
	a2 = a >> 16;
	b1 = b & 0xFFFF;
	b2 = b >> 16;

	/* a*b = (a2*e+a1) * (b2*e+b1) = (a2*b2*e^2 + a1*b2*e + a2*b1*e + a1*b1) */

	r1 = a1 * b1;
	r2 = a1 * b2;
	r3 = a2 * b1;
	r4 = a2 * b2;

	carry = 0;
	x = r1 + (r2 << 16);
	if (x < r1) carry++;
	y = x + (r3 << 16);
	if (y < x) carry++;
	*lo = y;
	*hi = r4 + (r2 >> 16) + (r3 >> 16) + carry;
}

/* See David G Carta, "Two Fast Implementations of the 'Minimal Standard'
   Random Number Generator", Communications of the ACM 33, 1 (January 1990),
   p 87-88.  */

void stdlib_srand_init(UINT32 seed, int *stdlib_random_state_p)
{
	if (seed == 0)
		seed = 40;
	*stdlib_random_state_p = (seed & 0x7FFFFFFFU);
}

void stdlib_srand(UINT32 seed)
{
	stdlib_srand_init(seed, &g_opera->stdlib_module.m_random_state);
}

UINT32 stdlib_rand()
{
	const unsigned int a = 16807;			// 0x41a7: representable in 15 bits
	unsigned int result, p, q = g_opera->stdlib_module.m_random_state;

	/* Compute (p,q) = q*a, where q is a 31-bit register. */

	stdlib_umul_32x32_to_64(q, a, &p, &q);

	p = (p << 1) | ((q >> 31) & 1);
	q = (q & 0x7FFFFFFFU);

	int trial = (int)p + (int)q;
	if (trial > 0)
		result = trial;
	else
		result = trial+1;

	g_opera->stdlib_module.m_random_state = q;

	return result;
}

#endif // !defined HAVE_RAND && !defined MERSENNE_TWISTER_RNG

#ifndef HAVE_ABS

/* C code needs direct access to this, can't use op_abs */
int stdlib_abs(int n)
{
    return op_abs(n);
}

#endif // HAVE_ABS

} // extern "C"

uni_char *uni_ultoa(unsigned long value, uni_char *str, int radix)
{
	uni_char *ret = str, *t = str;

	if (value == 0)
	{
		*str++ = '0';
		*str = 0;
		return ret;
	}

	OP_ASSERT(radix >= 2 && radix <= 36);
	if (radix < 2 || radix > 36)
	{
#ifdef HAVE_ERRNO
		errno = EINVAL;
#endif
		return str;
	}

	while (value != 0)
	{
		*str++ = "0123456789abcdefghijklmnopqrstuvwxyz"[value % radix];
		value = value / radix;
	}
	*str = 0;

	--str;
	while (str > t)
	{
		uni_char c = *str;
		*str = *t;
		*t = c;
		--str;
		++t;
	}

	return ret;
}

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
/* ui64toa is not in Standard C.

   We use the same definition as for ultoa, above. */

uni_char *uni_ui64toa(UINT64 value, uni_char *str, int radix)
{
	uni_char *ret = str, *t = str;

	if (value == 0)
	{
		*str++ = '0';
		*str = 0;
		return ret;
	}

	OP_ASSERT(radix >= 2 && radix <= 36);
	if (radix < 2 || radix > 36)
	{
#ifdef HAVE_ERRNO
		errno = EINVAL;
#endif
		return str;
	}

	while (value != 0)
	{
		*str++ = "0123456789abcdefghijklmnopqrstuvwxyz"[value % radix];
		value = value / radix;
	}
	*str = 0;

	--str;
	while (str > t)
	{
		uni_char c = *str;
		*str = *t;
		*t = c;
		--str;
		++t;
	}

	return ret;
}
#endif

#ifndef HAVE_UNI_LTOA
uni_char *uni_ltoa( long value, uni_char *str, int radix )
{
	uni_char *ret = str;

	if (value < 0)
	{
		value = -value;
		*str++ = '-';
	}
	uni_ultoa( (unsigned long)value, str, radix );

	return ret;
}
#endif

uni_char* uni_ltoan(long value, uni_char* str, int radix, int max)
{
	uni_char buf[sizeof(long)*8 + 2]; // ARRAY OK 2008-09-18 mortenro
	uni_ltoa(value, buf, radix);
	uni_strncpy(str, buf, max);
	str[max - 1] = 0;
	return str;
}

#define STRTOX_NAME uni_strtox
#define STRTOX_CHAR_TYPE uni_char
#include "modules/stdlib/src/stdlib_integer_strtol.inl"
#undef STRTOX_NAME
#undef STRTOX_CHAR_TYPE

long uni_strtol(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow)
{
	int signed_multiplier;
	long value = (long)uni_strtox(nptr, endptr, base, overflow, &signed_multiplier, LONG_MIN, LONG_MAX);
	return value * (long)signed_multiplier;
}

unsigned long uni_strtoul(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow)
{
	return (unsigned long)uni_strtox(nptr, endptr, base, overflow, NULL, 0, ULONG_MAX);
}

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
INT64 uni_strtoi64(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow)
{
	int signed_multiplier;
	INT64 value = uni_strtox(nptr, endptr, base, overflow, &signed_multiplier, INT64_MIN, INT64_MAX);
	return value * (INT64)signed_multiplier;
}

UINT64 uni_strtoui64(const uni_char* nptr, uni_char** endptr, int base, BOOL* overflow)
{
	return (UINT64)uni_strtox(nptr, endptr, base, overflow, NULL, 0, UINT64_MAX);
}
#endif // STDLIB_64BIT_STRING_CONVERSIONS

int uni_atoi(const uni_char * str)
{
	return uni_strtol( str, NULL, 10, NULL );
}

#ifndef HAVE_UNI_ITOA
uni_char *uni_itoa(int i, uni_char * buffer, int radix)
{
	op_itoa(i, (char*)buffer, radix);
	make_doublebyte_in_place(buffer, op_strlen((char*)buffer));
	return buffer;
}
#endif

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
uni_char *uni_i64toa(INT64 i, uni_char* buffer, int radix)
{
	op_i64toa(i, (char*)buffer, radix);
	make_doublebyte_in_place(buffer, op_strlen((char*)buffer));
	return buffer;
}
#endif // STDLIB_64BIT_STRING_CONVERSIONS

#ifndef HAVE_NTOHS
UINT16 op_ntohs(UINT16 netshort)
{
# ifdef OPERA_BIG_ENDIAN
	return netshort;
# else
	return (netshort & 0xffu) << 8 | (netshort & 0xff00u) >> 8;
# endif
}
#endif

#ifndef HAVE_NTOHL
UINT32 op_ntohl(UINT32 netlong)
{
# ifdef OPERA_BIG_ENDIAN
	return netlong;
# else
	UINT32 rotate16 = (netlong << 16) | (netlong >> 16);
	const UINT32 mask = 0xff00ff;
	return ((rotate16 >> 8) & mask) | ((rotate16 & mask) << 8);
# endif
}
#endif

#ifndef HAVE_HTONS
UINT16 op_htons(UINT16 hostshort)
{
# ifdef OPERA_BIG_ENDIAN
	return hostshort;
# else
	return (hostshort & 0xffu) << 8 | (hostshort & 0xff00u) >> 8;
# endif
}
#endif

#ifndef HAVE_HTONL
UINT32 op_htonl(UINT32 hostlong)
{
# ifdef OPERA_BIG_ENDIAN
	return hostlong;
# else
	UINT32 rotate16 = (hostlong << 16) | (hostlong >> 16);
	const UINT32 mask = 0xff00ff;
	return ((rotate16 >> 8) & mask) | ((rotate16 & mask) << 8);
# endif
}
#endif
