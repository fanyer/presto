/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _LIE_HASHTABLE_
#define _LIE_HASHTABLE_

#include "modules/util/simset.h"
#include "modules/util/OpHashTable.h"
#include "modules/doc/loadinlineelm.h"

/**
 * LoadInlineElmHashTable is an insertion ordered map from URL_ID to
 * multiple LoadInlineElms.
 *
 * <P>Internally, each OpHashTable entry refers to
 * a LoadInlineElmHashEntry, which in turn refers to a list of all
 * LoadInlineElms that a single URL_ID maps to (it might be several, after
 * redirection). LoadInlineElmHashEntries are additionally chained in
 * insertion order, so that iteration starts with the least recently
 * used <I>list</I> of LoadInlineElms.
 *
 * <P>After inserting LoadInlineElms 1,2,3 and 4, where 1 and 4 has been
 * redirected to the same URL_ID, the structure might look like this
 * (OpHashTable also has an internal structure, not included here):
 *
 * <pre>
 * LoadInlineElmHashTable:
 *
 *   OpHashTable   m_insertion_ordered_entries
 *    +-------+            |
 *    |       |            |
 *    +-------+            V
 *    |       |    LoadInlineElmHashEntry    LoadInlineElm
 *    +-------+        +-------+               +-------+
 *    |   *----------->| m_list--------------->|   2   |
 *    +-------+        +-------+               +-------+
 *    |       |            |
 *    +-------+            |
 *    |       |            |
 *    +-------+            |
 *    |       |            V
 *    +-------+        +-------+               +-------+
 *    |   *----------->| m_list--------------->|   3   |
 *    +-------+        +-------+               +-------+
 *    |       |            |
 *    +-------+            |
 *    |       |            V
 *    +-------+        +-------+   +-------+   +-------+
 *    |   *----------->| m_list--->|   1   |-->|   4   |
 *    +-------+        +-------+   +-------+   +-------+
 *    |       |
 *    +-------+
 * </pre>
 */
class LoadInlineElmHashTable
	: private OpHashTable
{
	friend class LoadInlineElmHashIterator;
public:
	LoadInlineElmHashTable();

	OP_STATUS Add(LoadInlineElm* lie);
	OP_STATUS Remove(LoadInlineElm* lie);
	OP_STATUS GetData(URL_ID url_id, Head** lie) const;
	OP_STATUS UrlMoved(URL_ID old_url_id, URL_ID new_url_id);
	OP_STATUS MoveLast(LoadInlineElm* lie);
	BOOL Contains(LoadInlineElm* lie) const;
	BOOL Empty() const;

	void DeleteAll();
	virtual void Delete(void* data);
private:
	Head m_insertion_ordered_entries;
};

/**
 * LoadInlineElmHashEntry is an entry in the underlying OpHashTable
 * of LoadInlineElmHashTable. Each entry is linked in insertion
 * order, and contains a list of all LoadInlineElms that a single
 * URL_ID maps to.
 */
class LoadInlineElmHashEntry
	: public Link
{
	LoadInlineElmHashEntry(void* hash_key) : m_hash_key(hash_key) {}
	friend class LoadInlineElmHashTable;
	friend class LoadInlineElmHashIterator;
private:
	Head m_list;
	void* m_hash_key; // Only needed by the fallback code in LoadInlineElmHashTable::Remove
};

/**
 * Insertion ordered iterator for LoadInlineElmHashTable.
 *
 * <P>Iteration traverses each m_list of each LoadInlineElmHashEntry taken from
 * m_insertion_ordered_entries of a LoadInlineElmHashTable. The iterator will
 * continue to work even if the current element is removed from the underlying
 * hash table.
 */
class LoadInlineElmHashIterator
{
public:
	LoadInlineElmHashIterator(const LoadInlineElmHashTable& hash_table);
	LoadInlineElm* First();
	LoadInlineElm* Next();
private:
	const Head* m_entries;
	LoadInlineElmHashEntry* m_next_entry;
	LoadInlineElm* m_next;
};

#endif /* _LIE_HASHTABLE_ */
