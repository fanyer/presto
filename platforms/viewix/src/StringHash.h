/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_STRING_HASH_H__
#define __FILEHANDLER_STRING_HASH_H__

class StringHash : public OpHashFunctions
{
public:
	~StringHash() {}

	/**
	 * Calculates the hash value for a key.
	 * the hash key.
	 */
	UINT32	Hash(const void* key);

	/**
	 * Compares if to keys are equal.
	 */
	BOOL	KeysAreEqual(const void* key1, const void* key2);
};

#endif //__FILEHANDLER_STRING_HASH_H__
