/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_memory.cpp Memory functions.

    PERFORMANCE NOTE: Some obvious optimizations have been done, but
	these are mostly correct but naive implementations.  If the
	system has character read and write instructions and a reasonable
	memory hierarchy, this is good enough.  On systems that don't, it
	might be faster to use word accesses with byte extraction and
	insertions.  Some optimizations should be considered here.  */

#include "core/pch.h"

extern "C" {

#ifndef HAVE_MEMCMP

int op_memcmp(const void* a, const void* b, size_t n)
{
    const unsigned char *ca = reinterpret_cast<const unsigned char *>(a);
    const unsigned char *cb = reinterpret_cast<const unsigned char *>(b);
    while (n > 0 && *ca == *cb)
    {
        ca++;
        cb++;
        --n;
    }
    return (n == 0) ? 0 : *ca - *cb;
}

#endif // HAVE_MEMCMP

// Determine type used for fast copy in memcpy and memmove
#if !defined HAVE_MEMCPY || !defined HAVE_MEMMOVE
# ifdef SIXTY_FOUR_BIT
#  define MEMCPY_TYPE INT64
# else
#  define MEMCPY_TYPE UINT32
# endif
#endif

#ifndef HAVE_MEMCPY

void* op_memcpy(void *dest, const void *src, size_t n)
{
	if (n > 0)
	{
		unsigned char *d       = reinterpret_cast<unsigned char *>(dest);
		const unsigned char *s = reinterpret_cast<const unsigned char *>(src);

		// If source and destination are both aligned, do fast
		// copying
		if ((reinterpret_cast<UINTPTR>(dest) % sizeof(MEMCPY_TYPE)) == 0 &&
		    (reinterpret_cast<UINTPTR>(src) % sizeof(MEMCPY_TYPE)) == 0)
		{
			while (n >= sizeof(MEMCPY_TYPE))
			{
				*reinterpret_cast<MEMCPY_TYPE *>(d) =
					*reinterpret_cast<const MEMCPY_TYPE *>(s);
				d += sizeof(MEMCPY_TYPE);
				s += sizeof(MEMCPY_TYPE);
				n -= sizeof(MEMCPY_TYPE);
			}
			if (!n)
				return dest;
		}

		// Copy remaining or unaligned data byte-by-byte
		switch (n & 7)
		{
		case 0:	do {	*d++ = *s++;
		case 7:			*d++ = *s++;
		case 6:			*d++ = *s++;
		case 5:			*d++ = *s++;
		case 4:			*d++ = *s++;
		case 3:			*d++ = *s++;
		case 2:			*d++ = *s++;
		case 1:			*d++ = *s++;
				} while (n > 8 && (n -= 8));
		}
	}
    return dest;
}

#endif // HAVE_MEMCPY

#ifndef HAVE_MEMMOVE

void* op_memmove(void *dest, const void *src, size_t n)
{
	if (n > 0)
	{
		unsigned char *d       = reinterpret_cast<unsigned char *>(dest);
		const unsigned char *s = reinterpret_cast<const unsigned char *>(src);

		if (d <= s || d >= s+n)
		{
			// If source and destination are both aligned, do fast
			// copying
			if ((reinterpret_cast<UINTPTR>(dest) % sizeof(MEMCPY_TYPE)) == 0 &&
				(reinterpret_cast<UINTPTR>(src) % sizeof(MEMCPY_TYPE)) == 0)
			{
				while (n >= sizeof(MEMCPY_TYPE))
				{
					*reinterpret_cast<MEMCPY_TYPE *>(d) =
						*reinterpret_cast<const MEMCPY_TYPE *>(s);
					d += sizeof(MEMCPY_TYPE);
					s += sizeof(MEMCPY_TYPE);
					n -= sizeof(MEMCPY_TYPE);
				}
				if (!n)
					return dest;
			}

			switch (n & 7)
			{
			case 0:	do {	*d++ = *s++;
			case 7:			*d++ = *s++;
			case 6:			*d++ = *s++;
			case 5:			*d++ = *s++;
			case 4:			*d++ = *s++;
			case 3:			*d++ = *s++;
			case 2:			*d++ = *s++;
			case 1:			*d++ = *s++;
					} while (n > 8 && (n -= 8));
			}
		}
		else
		{
			d += n;
			s += n;

			// If source and destination are both aligned, do fast
			// copying
			if ((reinterpret_cast<UINTPTR>(d) % sizeof(MEMCPY_TYPE)) == 0 &&
				(reinterpret_cast<UINTPTR>(s) % sizeof(MEMCPY_TYPE)) == 0)
			{
				while (n >= sizeof(MEMCPY_TYPE))
				{
					d -= sizeof(MEMCPY_TYPE);
					s -= sizeof(MEMCPY_TYPE);
					*reinterpret_cast<MEMCPY_TYPE *>(d) =
						*reinterpret_cast<const MEMCPY_TYPE *>(s);
					n -= sizeof(MEMCPY_TYPE);
				}
				if (!n)
					return dest;
			}

			switch (n & 7)
			{
			case 0:	do {	*--d = *--s;
			case 7:			*--d = *--s;
			case 6:			*--d = *--s;
			case 5:			*--d = *--s;
			case 4:			*--d = *--s;
			case 3:			*--d = *--s;
			case 2:			*--d = *--s;
			case 1:			*--d = *--s;
					} while (n > 8 && (n -= 8));
			}
		}
	}
    return dest;
}

#endif // HAVE_MEMMOVE

#ifndef HAVE_MEMSET

void* op_memset(void *s, int c, size_t n)
{
	unsigned char *cs = reinterpret_cast<unsigned char *>(s);
	unsigned char b   = static_cast<unsigned char>(c & 255);

	// If destination is aligned, do fast filling
	if ((reinterpret_cast<UINTPTR>(s) % sizeof(UINT32)) == 0 &&
		n > 8)
	{
		UINT32 b32 = b | (b << 8) | (b << 16) | (b << 24);

		while (n >= sizeof(UINT32))
		{
			*reinterpret_cast<UINT32 *>(cs) = b32;
			cs += sizeof(UINT32);
			n  -= sizeof(UINT32);
		}
	}

	if (n > 0)
	{
		switch (n & 7)
		{
		case 0:	do {	*cs++ = b;
		case 7:			*cs++ = b;
		case 6:			*cs++ = b;
		case 5:			*cs++ = b;
		case 4:			*cs++ = b;
		case 3:			*cs++ = b;
		case 2:			*cs++ = b;
		case 1:			*cs++ = b;
				} while (n > 8 && (n -= 8));
		}
	}

    return s;
}

#endif // HAVE_MEMSET

#ifndef HAVE_MEMCHR

void* op_memchr(const void *buf, int c, size_t n)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(buf);
    unsigned char b = static_cast<unsigned int>(c & 255);
    while (n > 0 && *p != b)
    {
       ++p;
       --n;
    }
    if (n == 0)
        return NULL;
    else
        return const_cast<unsigned char *>(p);
}

#endif // HAVE_MEMCHR

#ifndef HAVE_MEMMEM

void* op_memmem(const void *_haystack, size_t hl, const void *_needle, size_t nl)
{
	if (!nl)
		return const_cast<void *>(_haystack);
	if (!hl)
		return NULL;

	size_t no = 0;
	const unsigned char *haystack = reinterpret_cast<const unsigned char *>(_haystack);
	const unsigned char *needle   = reinterpret_cast<const unsigned char*>(_needle);

	while (hl-- >= nl - no)
	{
		if (*haystack ++ == needle[no])
		{
			no ++;
			if (no == nl)
				return const_cast<unsigned char *>(haystack - no);
		}
		else if (no > 0)
		{
			/* backtrack */
			haystack -= no;
			hl += no;
			no = 0;
		}
	}
	return NULL;
}

#endif // HAVE_MEMMEM

#ifndef HAVE_UNI_MEMCHR

uni_char* uni_memchr(const uni_char *buf, uni_char c, size_t n)
{
    while (n > 0 && *buf != c)
    {
       ++buf;
       --n;
    }
    if (n == 0)
        return NULL;
    else
        return const_cast<uni_char *>(buf);
}

#endif // HAVE_UNI_MEMCHR

} // extern "C"
