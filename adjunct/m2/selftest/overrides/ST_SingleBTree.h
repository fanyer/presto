/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_SINGLE_B_TREE_H
#define ST_SINGLE_B_TREE_H

#ifdef SELFTEST
# define BTreeDB ST_SingleBTree
# define BTreeDBCreate SingleBTreeWrapper

#include "modules/search_engine/SingleBTree.h"

template<class KEY> class ST_SingleBTree
{
public:
	virtual ~ST_SingleBTree() {}
	virtual OP_STATUS Close() = 0;
	virtual OP_STATUS Clear() = 0;
	virtual OP_STATUS Insert(const KEY &item, BOOL overwrite_existing = FALSE) = 0;
	virtual SearchIterator<KEY> *Search(const KEY &item, SearchOperator oper) = 0;
	virtual OP_BOOLEAN Delete(const KEY &item) = 0;
	virtual OP_STATUS Commit() = 0;
	virtual OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, long blocksize = 512, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER) = 0;
	virtual BlockStorage &GetGroupMember() = 0;
};

template<class KEY> class SingleBTreeWrapper : public ST_SingleBTree<KEY>
{
public:
	virtual OP_STATUS Close() { return m_tree.Close(); }
	virtual OP_STATUS Clear() { return m_tree.Clear(); }
	virtual OP_STATUS Insert(const KEY &item, BOOL overwrite_existing) { return m_tree.Insert(item, overwrite_existing); }
	virtual SearchIterator<KEY> *Search(const KEY &item, SearchOperator oper) { return m_tree.Search(item, oper); }
	virtual OP_BOOLEAN Delete(const KEY &item) { return m_tree.Delete(item); }
	virtual OP_STATUS Commit() { return m_tree.Commit(); }
	virtual OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, long blocksize, OpFileFolder folder) { return m_tree.Open(path, mode, blocksize, folder); }
	virtual BlockStorage &GetGroupMember() { return m_tree.GetGroupMember(); }

private:
	SingleBTree<KEY> m_tree;
};

#else
# define BTreeDB SingleBTree
# define BTreeDBCreate SingleBTree
#endif // SELFTEST

#endif // ST_SINGLE_B_TREE_H
