/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef HASH_H
#define HASH_H

#include "modules/util/OpHashTable.h"
#include "adjunct/desktop_util/string/hash-literal.inc"

namespace Hash
{
	/** Create a 32-bit hash for a string
	  * @param string String to hash
	  * @return 32-bit hash calculated using djbhash
	  */
	inline unsigned String(const char* string) { return OpGenericString8HashTable::HashString(string, TRUE); }
	inline unsigned String(const uni_char* string) { return OpGenericStringHashTable::HashString(string, TRUE); }

	/** Create a 32-bit hash for a string of known length
	  * @param string String to hash
	  * @param length Amount of characters in string to consider in hashing (first length characters)
	  * @return 32-bit hash calculated using djbhash
	  */
	inline unsigned String(const char* string, unsigned length) { return OpGenericString8HashTable::HashString(string, length, TRUE); }
	inline unsigned String(const uni_char* string, unsigned length) { return OpGenericStringHashTable::HashString(string, length, TRUE); }
};

/** Create a 32-bit hash for a string literal
  * @note this macro will calculate a hash value at compile-time. Only use it for string literals.
  * @note HASH_LITERAL(x) == Hash::String(x)
  * @example unsigned hash = HASH_LITERAL("example");
  *
  * @param string String to hash
  * @return 32-bit hash calculated using djbhash
  */
#define HASH_LITERAL(string) DJBHASH_LITERAL(string)

#endif // HASH_H
