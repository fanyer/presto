/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef BTREE_H
#define BTREE_H


#include "modules/search_engine/BTreeBase.h"
#include "modules/search_engine/ResultBase.h"

/**
 * @brief B-tree - easy searching and indexing data of the same size on disk.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * This is a thin template based on BTreeBase. Methods Empty, GetId and Clear don't need to be specialized.
 */
template <typename KEY> class TBTree : public BTreeBase
{
public:
	/** constructor is not supposed to be called directly, use TPool::GetTree/CreateTree instead */
	TBTree(PoolBase *cache, BSCache::Item::DiskId root) : BTreeBase(DefDescriptor<KEY>(), cache, root) {}

	/** constructor is not supposed to be called directly, use TPool::GetTree/CreateTree instead */
	TBTree(PoolBase *cache, BSCache::Item::DiskId root, TypeDescriptor::ComparePtr compare) :
		BTreeBase(TypeDescriptor(sizeof(KEY), &DefDescriptor<KEY>::Assign, compare, &DefDescriptor<KEY>::Destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
			, &DefDescriptor<KEY>::EstimateMemoryUsed
#endif			
			), cache, root) {}

	/**
	 * insert a single item into the BTree
	 * @param item KEY to insert
	 * @param overwrite_existing makes sense only if KEY contains some data not included in comparison
	 */
	CHECK_RESULT(OP_STATUS Insert(const KEY &item, BOOL overwrite_existing = FALSE)) {return BTreeBase::Insert(&item, overwrite_existing);}

	CHECK_RESULT(OP_STATUS Insert(SearchIterator<KEY> *iterator, BOOL overwrite_existing = FALSE))
	{
		// this is a temporary stuff, will be optimized
		if (iterator->Empty())
			return OpStatus::OK;

		do {
			RETURN_IF_ERROR(Insert(iterator->Get(), overwrite_existing));
		} while (iterator->Next());

		return OpStatus::OK;
	}


	/**
	 * delete a single item from the BTree
	 * @param item KEY to delete
	 * @return OpBoolean::IS_FALSE if the item wasn't found
	 */
	CHECK_RESULT(OP_BOOLEAN Delete(const KEY &item)) {return BTreeBase::Delete(&item);}

	/**
	 * delete a range of items
	 * @param iterator MUST NOT be obtained from searching from the same B-tree
	 */
	CHECK_RESULT(OP_STATUS Delete(SearchIterator<KEY> *iterator))
	{
		// this is a temporary stuff, will be optimized
		if (iterator->Empty())
			return OpStatus::OK;

		do {
			RETURN_IF_ERROR(Delete(iterator->Get()));
		} while (iterator->Next());

		return OpStatus::OK;
	}

	/**
	 * delete a range of items
	 * @param first item to delete from
	 * @param o1 operatorGE: delete including first, operatorGT: delete after first, other: delete from the beginning
	 * @param last item to delete until
	 * @param o2 operatorLE: delete including last, operatorLT: delete before last, other: delete until the end
	 */
	CHECK_RESULT(OP_STATUS Delete(const KEY &first, SearchOperator o1, const KEY &last, SearchOperator o2)) {return BTreeBase::Delete(&first, o1, &last, o2);}

	/**
	 * search for the given item
	 * @param item contains an item to search for on input and the found item on output, if found
	 * @return OpBoolean::IS_FALSE if BTree is empty, OpStatus::OK if the item wasn't found and OpBoolean::IS_TRUE if the item was found
	 */
	CHECK_RESULT(OP_BOOLEAN Search(KEY &item)) {return BTreeBase::Search(&item);}

	/**
	 * look up the first item
	 * @param item contains the found item on output, if found
	 * @return OpBoolean::IS_TRUE if the item was found, OpBoolean::IS_FALSE if BTree is empty
	 */
	CHECK_RESULT(OP_BOOLEAN GetFirst(KEY &item)) {return BTreeBase::GetFirst(&item);}

	/**
	 * look up the last item
	 * @param item contains the found item on output, if found
	 * @return OpBoolean::IS_TRUE if the item was found, OpBoolean::IS_FALSE if BTree is empty
	 */
	CHECK_RESULT(OP_BOOLEAN GetLast(KEY &item)) {return BTreeBase::GetLast(&item);}


	/**
	 * find all elements for which (element oper item) is TRUE
	 * @param item a key to compare
	 * @param oper operator to perform, one of operatorLT, operatorLE, operatorEQ, operatorGE, operatorGT
	 * @return NULL on error
	 */
	SearchIterator<KEY> *Search(const KEY &item, SearchOperator oper) {return (SearchIterator<KEY> *)BTreeBase::Search(&item, oper);}
	
	/**
	 * @return all items in the B-tree beginning with the first one
	 */
	SearchIterator<KEY> *SearchFirst(void) {return (SearchIterator<KEY> *)BTreeBase::SearchFirst();}

	/**
	 * @return all items in the B-tree beginning with the last one
	 */
	SearchIterator<KEY> *SearchLast(void) {return (SearchIterator<KEY> *)BTreeBase::SearchLast();}

protected:
	class TResult : public SearchIterator<KEY>, public BTreeBase::Result
	{
	public:
		TResult() {}

		~TResult(void) {Free(); OP_DELETE(this->m_btree);}

		virtual BOOL Next(void) {return _Next();}

		virtual BOOL Prev(void) {return _Prev();}

		CHECK_RESULT(virtual OP_STATUS Error(void) const) {return m_last_error;}

		virtual int Count(void) const {return _Count();}

		virtual BOOL End(void) const {return _End();}

		virtual BOOL Beginning(void) const {return _Beginning();}

		virtual const KEY &Get(void) {return *(KEY *)_Get();}
	};

	virtual IteratorBase *NewResult(BTreeBase::BTreeBranch *current, int pos, SearchOperator oper, const void *key)
	{
		BTreeBase *bt;
		TResult *rv;

		if ((bt = OP_NEW(TBTree<KEY>, (*this))) == NULL)
			return NULL;

		if ((rv = OP_NEW(TResult, ())) == NULL)
		{
			OP_DELETE(bt);
			return NULL;
		}

		if (OpStatus::IsError(rv->Init(bt, current, pos, oper, key)))
		{
			OP_DELETE(bt);
			OP_DELETE(rv);
			return NULL;
		}

		return rv;
	}
};


/**
 * @brief cache holding several B-trees in a file
 * @author Pavel Studeny <pavels@opera.com>
 *
 * This is a thin template based on BTreeBase. Methods Open, Close, Abort and Commit don't need to be specialized.
 */
template <typename KEY> class TPool : public PoolBase
{
public:
	/**
	 * @param max_cache_branches max. number of branches kept in memory, it is a memory/performance trade-off
	 */
	TPool(int max_cache_branches = BTREE_MAX_CACHE_BRANCHES) : PoolBase(max_cache_branches, DefDescriptor<KEY>()) {}

	/**
	 * @param compare comparison of the items kept in the B-trees; assignment and destruction are not expected to be needed here
	 * @param max_cache_branches max. number of branches kept in memory, it is a memory/performance trade-off
	 */
	TPool(TypeDescriptor::ComparePtr compare, int max_cache_branches = BTREE_MAX_CACHE_BRANCHES) :
		PoolBase(max_cache_branches, TypeDescriptor(sizeof(KEY), DefDescriptor<KEY>::Assign, compare, DefDescriptor<KEY>::Destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
			, DefDescriptor<KEY>::EstimateMemoryUsed
#endif
			)) {}

	/**
	 * @param root valid root position returned by DiskBTree::GetId()
	 * @return valid DiskBTree or NULL on out of memory
	 */
	TBTree<KEY> *GetTree(BSCache::Item::DiskId root)
	{
		return OP_NEW(TBTree<KEY>, (this, root, m_allocator.Compare));
	}

	/**
	 * @return a new empty DiskBTree
	 */
	TBTree<KEY> *CreateTree(void)
	{
		return OP_NEW(TBTree<KEY>, (this, 0, m_allocator.Compare));
	}
};

#endif  // BTREE_H

