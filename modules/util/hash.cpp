/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/hash.h"
#include "modules/unicode/unicode.h"

/* Hash: djb2 This algorithm was first reported by Dan
 * Bernstein many years ago in comp.lang.c.  It is easily
 * implemented and has relatively good statistical properties.
 */

template<class T> inline unsigned int internal_djb2hash(const T* s)
{
	unsigned int hashval = 5381;
	OP_ASSERT(static_cast<T>(-1) > 0 || !"Do not call with a signed type");
	while (*s)
	{
		hashval += (hashval << 5) + *(s ++); // hash*33 + c
	}
	return hashval;
}

template<class T> inline unsigned int internal_djb2hash(const T* s, size_t len)
{
	unsigned int hashval = 5381;
	OP_ASSERT(static_cast<T>(-1) > 0 || !"Do not call with a signed type");
	while (len-- != 0)
	{
		hashval += (hashval << 5) + *(s ++); // hash*33 + c
	}
	return hashval;
}

template<class T> inline unsigned int internal_djb2hash_nocase(const T* s)
{
	unsigned int hashval = 5381;
	OP_ASSERT(static_cast<T>(-1) > 0 || !"Do not call with a signed type");
	while (*s)
	{
		// Unicode::ToUpper() has special handling of ASCII, and is
		// inlined, so even if most input is pure ASCII, this should be
		// fairly cheap.
		hashval += (hashval << 5) + Unicode::ToUpper(*(s ++));
	}
	return hashval;
}

template<class T> inline unsigned int internal_djb2hash_nocase(const T* s, size_t len)
{
	unsigned int hashval = 5381;
	OP_ASSERT(static_cast<T>(-1) > 0 || !"Do not call with a signed type");
	while (len-- != 0)
	{
		// Unicode::ToUpper() has special handling of ASCII, and is
		// inlined, so even if most input is pure ASCII, this should be
		// fairly cheap.
		hashval += (hashval << 5) + Unicode::ToUpper(*(s ++));
	}
	return hashval;
}

/* Bodge round BREW tool-chain deficiencies.
 *
 * The ADS 1.2 compiler doesn't appear to define object code for methods of
 * templated classes unless they are referenced from the compilation unit which
 * defines the template class. So instead of making the templated versions
 * of the djb2hash functions above public, we inline them to a couple of
 * overloaded methods below.
 *
 * It gives us one bonus point by allowing us to cast all char strings over
 * to unsigned char.
 */
unsigned int djb2hash(const char* s)
{
	return internal_djb2hash(reinterpret_cast<const unsigned char *>(s));
}

unsigned int djb2hash(const uni_char* s)
{
	return internal_djb2hash(s);
}

unsigned int djb2hash_nocase(const char* s)
{
	return internal_djb2hash_nocase(reinterpret_cast<const unsigned char *>(s));
}

unsigned int djb2hash_nocase(const uni_char* s)
{
	return internal_djb2hash_nocase(s);
}

unsigned int djb2hash(const char* s, size_t len)
{
	return internal_djb2hash(reinterpret_cast<const unsigned char *>(s), len);
}

unsigned int djb2hash(const uni_char* s, size_t len)
{
	return internal_djb2hash(s, len);
}

unsigned int djb2hash_nocase(const char* s, size_t len)
{
	return internal_djb2hash_nocase(reinterpret_cast<const unsigned char *>(s), len);
}

unsigned int djb2hash_nocase(const uni_char* s, size_t len)
{
	return internal_djb2hash_nocase(s, len);
}
