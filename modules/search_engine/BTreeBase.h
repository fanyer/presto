/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef BTREEBASE_H
#define BTREEBASE_H


#include "modules/search_engine/TypeDescriptor.h"
#include "modules/search_engine/BSCache.h"
#include "modules/search_engine/ResultBase.h"

class PoolBase;

/**
 * @brief general type-less B-tree implementation as a base for a thin template TBTree
 * @author Pavel Studeny <pavels@opera.com>
 */
class BTreeBase
{
public:
	/**
	 * Used internally by BTree.
	 * One branch of the BTree containing at maximum 
	 * SIZE KEYs and SIZE + 1 POINTERs to child branches.
	 *
	 @verbatim
	       KEY1  KEY2  ... KEYmax
	      /     |     |   |       \
	 @endverbatim
     */
	struct BTreeBranch : public BSCache::Item
	{
		BTreeBase *owner;
		int size;
		BTreeBranch *parent;
		PoolBase *cache;
		char *data;

		BTreeBranch(PoolBase *cache, OpFileLength id, BTreeBranch *rbranch, unsigned short nur);
		virtual ~BTreeBranch();
		CHECK_RESULT(OP_STATUS Construct(BlockStorage *storage));

		CHECK_RESULT(virtual OP_STATUS Read(BlockStorage *storage));
		CHECK_RESULT(virtual OP_STATUS Flush(BlockStorage *storage));
		virtual void OnIdChange(DiskId new_id, DiskId old_id);
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		virtual size_t EstimateMemoryUsed() const;
#endif
		void CopyData(const BTreeBranch* src);

		inline char*       Data()               { return data; }
		inline const char* Data()         const { return data; }
		inline int         DataSize()     const { return sizeof(DiskId) + PtrItemSize() * size; }
		inline char*       PtrItem(int i)       { return data + sizeof(DiskId) + PtrItemSize() * i; }
		inline const char* PtrItem(int i) const { return data + sizeof(DiskId) + PtrItemSize() * i; }
		inline int         PtrItemSize()  const { return sizeof(DiskId) + ItemSize(); }
		inline char*       GetItem(int i)       { return PtrItem(i) + sizeof(DiskId); }
		inline const char* GetItem(int i) const { return PtrItem(i) + sizeof(DiskId); }
		inline int         ItemSize()     const ;
		inline DiskId&     Pointer(int i)       { return *reinterpret_cast<      DiskId *>(PtrItem(i)); }
		inline DiskId      Pointer(int i) const { return *reinterpret_cast<const DiskId *>(PtrItem(i)); }
		inline DiskId&     Rightmost()          { return *reinterpret_cast<      DiskId *>(data); }
		inline DiskId      Rightmost()    const { return *reinterpret_cast<const DiskId *>(data); }

	protected:
		friend class PoolBase;
	};

	/**
	 * delete all data; if KEY is a pointer, doesn't delete the pointer itself
	 */
	virtual ~BTreeBase(void);
	
	/**
	 * insert a single item into the BTree
	 * @param item data to insert
	 * @param overwrite_existing makes sense only if KEY contains some data not included in comparison
	 */
	CHECK_RESULT(OP_STATUS Insert(const void *item, BOOL overwrite_existing = FALSE));

	/**
	 * delete a single item from the BTree
	 * @param item data to delete
	 * @return OpBoolean::IS_FALSE if the item wasn't found
	 */
	CHECK_RESULT(OP_BOOLEAN Delete(const void *item));

	/**
	 * delete a range of items
	 * @param first item to delete from
	 * @param o1 operatorGE: delete including first, operatorGT: delete after first, other: delete from the beginning
	 * @param last item to delete until
	 * @param o2 operatorLE: delete including last, operatorLT: delete before last, other: delete until the end
	 */
	CHECK_RESULT(OP_STATUS Delete(const void *first, SearchOperator o1, const void *last, SearchOperator o2));

	/**
	 * search for the given item
	 * @param item contains an item to search for on input and the found item on output, if found
	 * @return OpBoolean::IS_TRUE if the item was found; OpStatus::OK if the item wasn't found - item contains the next item found; OpBoolean::IS_FALSE if BTree is empty or item wasn't found and there is no next item
	 */
	CHECK_RESULT(OP_BOOLEAN Search(void *item));

	/**
	 * look up the first item
	 * @param item contains the found item on output, if found
	 * @return OpBoolean::IS_TRUE if the item was found, OpBoolean::IS_FALSE if BTree is empty
	 */
	CHECK_RESULT(OP_BOOLEAN GetFirst(void *item));

	/**
	 * look up the last item
	 * @param item contains the found item on output, if found
	 * @return OpBoolean::IS_TRUE if the item was found, OpBoolean::IS_FALSE if BTree is empty
	 */
	CHECK_RESULT(OP_BOOLEAN GetLast(void *item));


	/**
	 * find all elements for which (element oper item) is TRUE
	 * @param item a key to compare
	 * @param oper operator to perform, one of operatorLT, operatorLE, operatorEQ, operatorGE, operatorGT
	 * @return NULL on error
	 */
	IteratorBase *Search(const void *item, SearchOperator oper);

	/**
	 * @return all items in the B-tree beginning with the first one
	 */
	IteratorBase *SearchFirst(void);

	/**
	 * @return all items in the B-tree beginning with the last one
	 */
	IteratorBase *SearchLast(void);


	/**
	 * @return YES if the B-tree is empty, NO if it contains at least 1 item and MAYBE on error
	 */
	BOOL3 Empty(void);

	/**
	 * @return position of the root branch in the PoolBase
	 */
	BSCache::Item::DiskId GetId(void);

	/**
	 * reset this instance to another root when transaction is being aborted
	 * @param new_root 0 to invalidate the written data
	 */
	void Renew(BSCache::Item::DiskId new_root = 0)
	{
		this->m_root = new_root;
	}

	/**
	 * empty the BTree
	 */
	CHECK_RESULT(OP_STATUS Clear(void));

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	size_t EstimateMemoryUsed() const;
#endif

	/**
	 * @brief methods to be used by TBTree::TResult
	 */
	class Result
	{
	public:
		CHECK_RESULT(OP_STATUS Init(BTreeBase *owner, BTreeBranch *current, int pos,
			SearchOperator oper, const void *key));

	protected:
		BOOL _Next(void);

		BOOL _Prev(void);

		int _Count(void) const;

		BOOL _End(void) const;

		BOOL _Beginning(void) const;

		const void *_Get(void);

		void Free(void);

		OP_STATUS m_last_error;
		BTreeBase *m_btree;
		BTreeBranch *m_current;
		int m_pos;
		SearchOperator m_oper;
		char *m_key_buf;
		LoopDetector<BSCache::Item::DiskId> m_loop_detector;
	};

	friend class Result;
	friend class PoolBase;
	friend struct BTreeBranch;
protected:

	/**
	 * create an empty BTree
	 */
	BTreeBase(const TypeDescriptor &allocator, PoolBase *cache, BSCache::Item::DiskId root);

	/**
	 * create a new result for the given key and operator
	 */
	virtual IteratorBase *NewResult(BTreeBranch *current, int pos, SearchOperator oper, const void *key) {return NULL;}

	/**
	 * create a new branch and increase its reference counter
	 * @param branch created branch
	 * @param parent parent of the new branch or 0 for root
	 */
	CHECK_RESULT(virtual OP_STATUS NewBranch(BTreeBranch **branch, BTreeBranch *parent));

	/**
	 * delete the branch and all its subbranches
	 */
	CHECK_RESULT(OP_STATUS DeleteBranch(BSCache::Item::DiskId branch));

	/**
	 * erase the branch and decrase its reference counter
	 */
	CHECK_RESULT(OP_STATUS DeleteLeaf(BTreeBranch *leaf));

	/**
	 * adjust root branch if empty
	 */
	CHECK_RESULT(OP_STATUS RebalanceRoot(void));

	/**
	 * change parent in case of splitting one branch
	 */
	void SetParent(BSCache::Item::DiskId branch, BTreeBranch *parent);

	/**
	 * load a new branch instead of the previous one
	 * @param rv loaded branch
	 * @param branch new branch to go
	 * @param previous a branch to release together with its parents or NULL if there is no such branch
	 */
	CHECK_RESULT(OP_STATUS Goto(BTreeBase::BTreeBranch **rv, BSCache::Item::DiskId branch, BTreeBase::BTreeBranch *previous));

	/**
	 * load a new branch instead of the previous one
	 * @param branch new branch to go or NULL to just release the previous
	 * @param previous a branch to release together with its parents or NULL if there is no such branch
	 */
	BTreeBranch *GotoB(BTreeBranch *branch, BTreeBranch *previous);

	/**
	 * set the current branch as modified; newly created branch is always modified
	 */
	CHECK_RESULT(OP_STATUS Modify(BTreeBranch *branch));

	/**
	 * @return POINTER which is guaranteed not to change
	 */
	BSCache::Item::DiskId SafePointer(BTreeBranch *branch);

	/**
	 * @return index of entry in branch >= item
	 */
//	int BinarySearch(BTreeBranch *branch, const void *item);

	/**
	 * search for the item in the whole BTree
	 * @param rbranch branch containing the KEY or where to insert it
	 * @param rpos position of the KEY in rbranch or position where to insert it
	 * @param item KEY to search
	 * @return IS_TRUE if found
	 */
	CHECK_RESULT(OP_BOOLEAN SearchItem(BTreeBranch **rbranch, int *rpos, const void *item));

	/**
	 * find the least item greater than the given one
	 * @return IS_FALSE on eond of data
	 */
	CHECK_RESULT(OP_BOOLEAN SearchNextItem(BTreeBranch **rbranch, int *rpos, const void *item));

	/**
	 * insert the KEY to the given branch and postion, doesn't release the branch
	 * @param branch relese the branch after return from this method by Goto(0, branch)
	 * @param pos position returned by SearchItem or BinarySearch
	 * @param item KEY to insert
	 * @param overflow new branch in case that insertion caused a full branch to overflow
	 */
	CHECK_RESULT(OP_STATUS InsertItem(BTreeBranch *branch, int pos, const void *item, BSCache::Item::DiskId overflow = 0));

	/**
	 * delete item from any branch
	 * @param branch branch containing the KEY to delete
	 * @param pos position of the KEY in the branch
	 */
	CHECK_RESULT(OP_STATUS DeleteItem(BTreeBranch *branch, int pos));

	/**
	 * delete item from a leaf or from a parent of merged branch - the KEY's POINTER is ignored
	 * @param branch branch containing the KEY to delete
	 * @param pos position of the KEY in the branch
	 */
	CHECK_RESULT(OP_STATUS RemoveItem(BTreeBranch *branch, int pos));

	/**
	 * balance the number of items between two neighbor branches
	 * @param l_child left branch
	 * @param parent parent branch
	 * @param g_child reight branch
	 */
	void Rotate(BTreeBranch *l_child, BTreeBranch *parent, BTreeBranch *g_child);

	/**
	 * join two branches into one, the non-destination child is deleted
	 * @param dst destination branch, it's supposed to be l_child or g_child
	 * @param l_child left branch
	 * @param parent parent branch
	 * @param g_child reight branch
	 */
	CHECK_RESULT(OP_STATUS Merge(BTreeBranch *dst, BTreeBranch *l_child, BTreeBranch *parent, BTreeBranch *g_child));

	/**
	 * @return TRUE if given branch has no children
	 */
	BOOL IsLeaf(BTreeBranch *branch) const {return branch->Rightmost() == 0;}

	/**
	 * @return TRUE if given branch has no parent
	 */
	BOOL IsRoot(BTreeBranch *branch) const {return branch->parent == NULL;}

	inline int BTreeSize() const;
	
#ifdef _DEBUG
	inline BOOL CheckIntegrity(const BTreeBranch *branch) const;
#endif

	TypeDescriptor m_allocator;
	BSCache::Item::DiskId m_root;
	PoolBase *m_cache;
	OpFileLength m_reserved_id;
};


/**
 * @brief cache holding several B-trees in a file
 * @author Pavel Studeny <pavels@opera.com>
 */
class PoolBase : public BSCache
{
public:
	PoolBase(int max_cache_branches, const TypeDescriptor &allocator);

	/**
	 * PoolBase must be opened before you call any other method
	 * @param path file storing the data; file is always created if it doesn't exist
	 * @param mode Read/ReadWrite mode
	 * @param blocksize one block consists of 12 B of internal BlockStorage data, 4 B rightmost pointer and the rest is divided into (sizeof(data) + 4 B pointer) chunks
	 * @param folder might be one of predefind folders
	 */
	CHECK_RESULT(OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode, int blocksize = 512, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/**
	 * flush all unsaved data, commit any pending transaction and close the file
	 */
	CHECK_RESULT(OP_STATUS Close(void));

	/**
	 * abort all write operations since the last Commit
	 */
	CHECK_RESULT(OP_STATUS Abort(void));

	/**
	 * write all modified data to disk
	 */
	CHECK_RESULT(OP_STATUS Commit(void));

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	virtual size_t EstimateMemoryUsed() const;
#endif

	friend class BTreeBase;
	friend struct BTreeBase::BTreeBranch;

protected:
	virtual BSCache::Item *NewMemoryItem(int id, Item *rbranch, int rnode, unsigned short nur);

	virtual BSCache::Item *NewDiskItem(OpFileLength id, unsigned short nur);

	BTreeBase::BTreeBranch *LoadFromCache(BSCache::Item::DiskId id);

	TypeDescriptor m_allocator;

public:
	/**
	 * @param root If the file holds only one BTree (e.g. a SingleBTree), you should specify
	 *   the root node (typically == 2) to enable more consistency checks. Otherwise use 0.
	 * @param thorough If TRUE, the BTree will be checked in depth for recursive sorting errors
	 * @return OpBoolean::IS_TRUE if the BTree is consistent
	 */
	CHECK_RESULT(OP_BOOLEAN CheckConsistency(UINT32 root = 0, BOOL thorough = TRUE));

#if defined(LINGOGI) && (defined(_DEBUG) || defined(SELFTEST) || defined(SEARCH_ENGINE_LOG))
	CHECK_RESULT(OP_BOOLEAN PrintBTree(UINT32 root = 0));
#endif
};

inline int BTreeBase::BTreeSize() const
{
	return (int)((m_cache->m_storage.GetBlockSize() - 12 - sizeof(BSCache::Item::DiskId)) / (m_cache->m_allocator.size + sizeof(BSCache::Item::DiskId)));
}

inline int BTreeBase::BTreeBranch::ItemSize() const
{
	return (int)(cache->m_allocator.size_align32);
}

#endif  // BTREEBASE_H

