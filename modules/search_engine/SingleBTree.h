/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef SINGLEBTREE_H
#define SINGLEBTREE_H

#include "modules/search_engine/BTree.h"

#define SBTREE_MAX_CACHE_BRANCHES 50

/**
 * @brief BTree stored in a file.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * SingleBTree is a BTree stored on disk.
 * Unlike DiskBTree, you can use it directly, since there is only one BTree in the file.
 */
template <typename KEY> class SingleBTree :
	public TBTree<KEY>,
	public TPool<KEY>
{
public:
	SingleBTree(int max_cache_branches = SBTREE_MAX_CACHE_BRANCHES) : TBTree<KEY>(this, 0), TPool<KEY>(max_cache_branches) {}
	SingleBTree(TypeDescriptor::ComparePtr compare, int max_cache_branches = SBTREE_MAX_CACHE_BRANCHES) :
		TBTree<KEY>(this, 0, compare), TPool<KEY>(compare, max_cache_branches) {}

	/**
	 * SingleBTree must be opened before you call any other method 
	 * @param path file storing the data; file is always created if it doesn't exist
	 * @param mode Read/ReadWrite mode
	 * @param blocksize one block consists of 12 B of internal BlockStorage data, 4 B rightmost pointer and the rest is divided into (sizeof(data) + 4 B pointer) chunks
	 * @param folder might be one of predefind folders
	 */
	CHECK_RESULT(OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, int blocksize = 512, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER))
	{
		RETURN_IF_ERROR((TPool<KEY>::Open(path, mode, blocksize, folder)));

		if (this->m_storage.GetFileSize() > (OpFileLength)this->m_storage.GetBlockSize() * 2)
			this->m_root = 2;

		return OpStatus::OK;
	}

	/** Recovery function for corrupted SingleBTree
	 * SingleBTree must be closed before you call this function
	 * @param path file name of the original SingleBTree
	 * @param blocksize blocksize of original SingleBTree
	 * @param folder might be one of predefind folders
	 */
	CHECK_RESULT(OP_STATUS Recover(const uni_char* path, int blocksize = 512, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER))
	{
		OpString temp_filename;
		RETURN_IF_ERROR(temp_filename.AppendFormat(UNI_L("%s.temp"),path));

		SingleBTree<KEY> temp_tree;
		
		// Open new btree
		RETURN_IF_ERROR(temp_tree.Open(temp_filename.CStr(), BlockStorage::OpenReadWrite, blocksize, folder));

		// Open old btree
		RETURN_IF_ERROR(Open(path, BlockStorage::OpenRead, blocksize, folder));

		OpAutoPtr<SearchIterator<KEY> > tree_iterator(this->SearchFirst());
		if (!tree_iterator.get())
			return OpStatus::ERR_NO_MEMORY;

		if (!tree_iterator->Empty())
		{
			// loop through the old tree and add stuff as we find them
			do
			{
				RETURN_IF_ERROR(temp_tree.Insert(KEY(tree_iterator->Get())));
			}
			while (tree_iterator->Next());
		}
		// open iterators need to be closed before closing the tree
		tree_iterator.reset();

		RETURN_IF_ERROR(this->Close());
		RETURN_IF_ERROR(temp_tree.Close());

		// Delete the old tree and move new tree to the correct location 
		RETURN_IF_ERROR(BlockStorage::DeleteFile(path, folder));
		return BlockStorage::RenameFile(temp_filename.CStr(), path, folder);
	}


protected:
	CHECK_RESULT(virtual OP_STATUS NewBranch(BTreeBase::BTreeBranch **branch, BTreeBase::BTreeBranch *parent))
	{
		if (this->m_root == 0 && this->m_head != NULL)
			RETURN_IF_ERROR(this->Flush());

		RETURN_IF_ERROR(TBTree<KEY>::NewBranch(branch, parent));

		if (this->m_root < 0 && this->m_root == (*branch)->disk_id)
			TBTree<KEY>::SafePointer(*branch);  // SafePointer will set this->m_root

		return OpStatus::OK;
	}
};

#endif  // SINGLEBTREE_H

