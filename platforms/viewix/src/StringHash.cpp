/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/StringHash.h"

/***********************************************************************************
 ** Calculates the hash value for a key.
 **
 ** @param the hash key.
 **
 ** @return the hash code.
 **
 ** Note: Implementation modified from
 ** UINT32 OpGenericStringHashTable::Hash(const void* key)
 ** in OpHashTable.
 ***********************************************************************************/
UINT32 StringHash::Hash(const void* key)
{
	const uni_char* str = static_cast<const uni_char*>(key);
	/* Hash: djb2 This algorithm was first reported by Dan
	 * Bernstein many years ago in comp.lang.c.  It is easily
	 * implemented and has relatively good statistical properties.
	 */
	UINT32 hashval = 5381;

	while (*str)
	{
		hashval = ((hashval << 5) + hashval) + *str++; // hash*33 + c
	}

	return hashval;
}

/***********************************************************************************
 ** Compares if to keys are equal.
 **
 ** @param the keys to be compared
 **
 ** @return true if keys are equal
 **
 ** Note: Implementation modified from
 ** UINT32 OpGenericStringHashTable::Hash(const void* key)
 ** in OpHashTable.
 ***********************************************************************************/
BOOL StringHash::KeysAreEqual(const void* key1,
						  const void* key2)
{
	const uni_char* string1 = (const uni_char*) key1;
	const uni_char* string2 = (const uni_char*) key2;

	return uni_str_eq(string1, string2);
}
