/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2001-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Petter Nilsen <pettern@opera.com>
*/
#ifndef _WIN_HASHTABLES_INCLUDED_
#define _WIN_HASHTABLES_INCLUDED_

#include "modules/util/OpHashTable.h"

/**************************************************************************
*
* This is a variation of OpPointerHashTable but doesn't assume everything
* can be used as a pointer (which won't work with HWND).
*
**************************************************************************/

template <class KeyType, class DataType>
class OpWindowsPointerHashTable : private OpHashTable
{
public:
	OpWindowsPointerHashTable() : OpHashTable() {}

	OpWindowsPointerHashTable(OpHashFunctions* hashFunctions, BOOL useChainedHashing = TRUE) :
		OpHashTable(hashFunctions,useChainedHashing)
		{}

	OP_STATUS Add(const KeyType key, DataType data) { return OpHashTable::Add((const void *)(key), (void *)(data)); }
	OP_STATUS Remove(const KeyType key, DataType* data) { return OpHashTable::Remove((const void *)(key), (void **)(data)); }

	OP_STATUS GetData(const KeyType key, DataType* data) const { return OpHashTable::GetData((const void *)(key), (void **)(data)); }
	BOOL Contains(const KeyType key) const { return OpHashTable::Contains((const void *)(key)); }

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach(void (*function)(const void *, void *)) { OpHashTable::ForEach(function); }
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) { OpHashTable::ForEach(function); }

	void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	virtual void Delete(void* data) { delete (DataType)(data); }

	INT32 GetCount() const { return OpHashTable::GetCount(); }

	void SetMinimumCount(UINT32 minimum_count){ OpHashTable::SetMinimumCount(minimum_count); }

	void SetHashFunctions(OpHashFunctions* hashFunctions) {OpHashTable::SetHashFunctions(hashFunctions);}
};

#endif // _WIN_HASHTABLES_INCLUDED_
