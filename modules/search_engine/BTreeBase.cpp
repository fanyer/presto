/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE

#include "modules/search_engine/BTreeBase.h"

#ifdef _DEBUG
#define CHECK_INTEGRITY(branch) OP_ASSERT(CheckIntegrity(branch))
#else
#define CHECK_INTEGRITY(branch)
#endif

static int BinarySearch(const BTreeBase::BTreeBranch *branch, const void *item, const TypeDescriptor &m_allocator)
{
	int n2, start, end;

	start = 0;
	end = branch->size;

	while (end > start)
	{
		n2 = (end - start) / 2;
		if (m_allocator.Compare(branch->GetItem(start + n2), item))
			start = start + n2 + 1;
		else
			end = start + n2;
	}

	return start;
}

BTreeBase::BTreeBranch::BTreeBranch(PoolBase *cache, OpFileLength id, BTreeBranch *rbranch, unsigned short nur)
{
	parent = rbranch;
	size = 0;

	disk_id = (BSCache::Item::DiskId)id;
	modified = disk_id < 0;
	NUR_mark = nur;
	owner = NULL;
	this->cache = cache;
}

BTreeBase::BTreeBranch::~BTreeBranch() {
	OP_DELETEA(data);
}

OP_STATUS BTreeBase::BTreeBranch::Construct(BlockStorage *storage) {
	RETURN_OOM_IF_NULL(data = OP_NEWA(char, storage->GetBlockSize() - 12));
	Rightmost() = 0;
	return OpStatus::OK;
}

OP_STATUS BTreeBase::BTreeBranch::Read(BlockStorage *storage)
{
	int data_size;
	OpFileLength pos = ((OpFileLength)disk_id) * storage->GetBlockSize();

	data_size = storage->DataLength(pos);
	if (data_size > storage->GetBlockSize() - 12 || data_size < (int)sizeof(BSCache::Item::DiskId))
	{
		OP_ASSERT(0);  // invalid data
		return OpStatus::ERR_PARSING_FAILED;
	}

	if (!storage->Read(this->Data(), data_size, pos))
		return OpStatus::ERR_NO_ACCESS;

	this->size = (data_size - sizeof(BSCache::Item::DiskId)) / PtrItemSize();

	return OpStatus::OK;
}

OP_STATUS BTreeBase::BTreeBranch::Flush(BlockStorage *storage)
{
	OpFileLength did;
	BSCache::Item::DiskId old_id;

	OP_ASSERT(this->DataSize() <= storage->GetBlockSize() - 12);

	if (this->disk_id > 0 && !this->id_reserved)  // update
	{
		if (!storage->Update(this->Data(), this->DataSize(),
			((OpFileLength)disk_id) * storage->GetBlockSize()))
			return OpStatus::ERR_NO_DISK;
	}
	else {  // create new
		old_id = this->disk_id;

		if (this->id_reserved)
		{
			if ((did = storage->Write(this->Data(), this->DataSize(),
				((OpFileLength)this->disk_id) * storage->GetBlockSize()) / storage->GetBlockSize()) == 0)

				return OpStatus::ERR_NO_DISK;

			this->id_reserved = FALSE;
		}
		else {
			if ((did = storage->Write(this->Data(), this->DataSize()) / storage->GetBlockSize()) == 0)

				return OpStatus::ERR_NO_DISK;
		}

		this->disk_id = (BSCache::Item::DiskId)did;

		OnIdChange(disk_id, old_id);

	}

	this->modified = FALSE;

	return OpStatus::OK;
}

void BTreeBase::BTreeBranch::OnIdChange(DiskId new_id, DiskId old_id)
{
	int pos;

	if (this->parent == NULL && old_id < 0 && (this->owner->m_root == 0 || this->owner->m_root == old_id))
		this->owner->m_root = new_id;

	if (this->parent != NULL && old_id != new_id)
	{
		if (this->size == 0)
			pos = this->parent->size; // This should *only* happen if btree-size==3 and insert-pos==3
		else
			pos = BinarySearch(this->parent, this->GetItem(0), this->cache->m_allocator);
		if (pos < this->parent->size)
		{
			if (this->parent->Pointer(pos) == old_id)
				this->parent->Pointer(pos) = new_id;
		}
		else {
			if (this->parent->Rightmost() == old_id)
				this->parent->Rightmost() = new_id;
		}
	}
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t BTreeBase::BTreeBranch::EstimateMemoryUsed() const
{
	return Item::EstimateMemoryUsed() +
		sizeof(owner) +
		sizeof(size) +
		sizeof(parent) +
		sizeof(cache) +
		sizeof(data) +
		cache->m_storage.GetBlockSize() - 12;
}
#endif

void BTreeBase::BTreeBranch::CopyData(const BTreeBase::BTreeBranch* src)
{
	op_memcpy(Data(), src->Data(), src->DataSize());
	size = src->size;
}

inline void CopyPtrItems(BTreeBase::BTreeBranch* dstBranch, int dstPos,
						 const BTreeBase::BTreeBranch* srcBranch, int srcPos, int n)
{
	op_memmove(dstBranch->PtrItem(dstPos), srcBranch->PtrItem(srcPos), n * srcBranch->PtrItemSize());
}

inline void CopyItem(BTreeBase::BTreeBranch* dstBranch, int dstPos,
					 const BTreeBase::BTreeBranch* srcBranch, int srcPos)
{
	op_memcpy(dstBranch->GetItem(dstPos), srcBranch->GetItem(srcPos), srcBranch->ItemSize());
}

inline void CopyItem(void* dst, const BTreeBase::BTreeBranch* srcBranch, int srcPos)
{
	op_memcpy(dst, srcBranch->GetItem(srcPos), srcBranch->ItemSize());
}

inline void CopyItem(BTreeBase::BTreeBranch* dstBranch, int dstPos, const void* src)
{
	op_memcpy(dstBranch->GetItem(dstPos), src, dstBranch->ItemSize());
}

BTreeBase::BTreeBase(const TypeDescriptor &allocator, PoolBase *cache, BSCache::Item::DiskId root) : m_allocator(allocator)
{
	m_root = root;
	m_cache = cache;
	m_reserved_id = 0;
}

BTreeBase::~BTreeBase(void)
{
	OP_ASSERT(this->m_root >= 0);  // closing this tree without knowing it's ID
//	m_cache->Flush();
}

BSCache::Item::DiskId BTreeBase::GetId(void)
{
	if (this->m_root > 0)
		return this->m_root;

	if (this->m_root < 0)
	{
		BTreeBase::BTreeBranch *root;
		BSCache::Item::DiskId rv;

#ifdef _DEBUG
		OP_ASSERT(OpStatus::IsSuccess(Goto(&root, this->m_root, NULL))); // cannot fail since root already is in memory
#else
		OpStatus::Ignore(Goto(&root, this->m_root, NULL));
#endif

		rv = SafePointer(root);
		GotoB(NULL, root);

		return rv;
	}

	if (m_reserved_id != 0)
		return (BSCache::Item::DiskId)(m_reserved_id / m_cache->m_storage.GetBlockSize());

	return (BSCache::Item::DiskId)(m_reserved_id = m_cache->m_storage.Reserve()) / m_cache->m_storage.GetBlockSize();
}


OP_STATUS BTreeBase::Insert(const void *item, BOOL overwrite_existing)
{
	BTreeBranch *current;
	int pos;
	OP_BOOLEAN rv;

	OP_ASSERT(item != NULL);

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if (this->m_root == 0)
	{
		RETURN_IF_ERROR(NewBranch(&current, NULL));
		RETURN_IF_ERROR(Modify(current));
		++(current->size);
		CopyItem(current, 0, item);
		current->Pointer(0) = 0;
		GotoB(NULL, current);
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(rv = SearchItem(&current, &pos, item));
	if (rv == OpBoolean::IS_TRUE)
	{
		if (overwrite_existing)
		{
			RETURN_IF_ERROR(Modify(current));
			CopyItem(current, pos, item);
			CHECK_INTEGRITY(current);
		}
		GotoB(NULL, current);
		return OpStatus::OK;
	}

	rv = InsertItem(current, pos, item);

	GotoB(NULL, current);
	return rv;
}

OP_BOOLEAN BTreeBase::Delete(const void *item)
{
	BTreeBranch *current;
	int pos;
	OP_BOOLEAN rv;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if (this->m_root == 0)
		return OpBoolean::IS_FALSE;

	if ((rv = SearchItem(&current, &pos, item)) != OpBoolean::IS_TRUE)
	{
		GotoB(NULL, current);
		return rv;
	}

	if (current == NULL)
		return OpStatus::ERR;

	if (OpStatus::IsError(rv = DeleteItem(current, pos)))
	{
		GotoB(NULL, current);
		return rv;
	}

	GotoB(NULL, current);

	RETURN_IF_ERROR(RebalanceRoot());

	return OpBoolean::IS_TRUE;
}

OP_STATUS BTreeBase::Delete(const void *first, SearchOperator o1, const void *last, SearchOperator o2)
{
	OpString next_item;
	BTreeBranch *current, *next;
	BSCache::Item::DiskId ptr;
	int pos, npos;
	OP_BOOLEAN rv;
	BOOL eod;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if (this->m_root == 0)
		return OpStatus::OK;

	RETURN_OOM_IF_NULL(next_item.Reserve((int)m_allocator.size));

	// find the position to delete from
	switch (o1)
	{
	case operatorGE:
		if (OpStatus::IsError(rv = SearchItem(&current, &pos, first)))
		{
			GotoB(NULL, current);
			return rv;
		}
		if (rv == OpBoolean::IS_FALSE && pos >= current->size)
		{
			OP_ASSERT(IsLeaf(current));
			GotoB(NULL, current);
			return OpStatus::OK;
		}
		break;

	case operatorGT:
		if (OpStatus::IsError(rv = SearchItem(&current, &pos, first)))
		{
			GotoB(NULL, current);
			return rv;
		}

		if (rv == OpBoolean::IS_FALSE && pos >= current->size)
		{
			OP_ASSERT(IsLeaf(current));
			GotoB(NULL, current);
			return OpStatus::OK;
		}

		if (rv == OpBoolean::IS_TRUE)
		{
			if (OpStatus::IsError(rv = SearchNextItem(&current, &pos, current->GetItem(pos))))
			{
				GotoB(NULL, current);
				return rv;
			}
			if (rv == OpBoolean::IS_FALSE)
			{
				GotoB(NULL, current);
				return OpStatus::OK;
			}
		}
		break;
	default:
		current = NULL;
		ptr = m_root;

		LoopDetector<BSCache::Item::DiskId> loopDetector;
		do {
			RETURN_IF_ERROR(loopDetector.CheckNext(ptr));
			RETURN_IF_ERROR(Goto(&current, ptr, current));

			ptr = current->Pointer(0);
		} while (!IsLeaf(current));

		if (current->size == 0)  // empty
		{
			GotoB(NULL, current);
			return OpStatus::OK;
		}

		pos = 0;
	}

	if (current == NULL)
		return OpStatus::ERR;

	// delete the items
	while ((o2 != operatorLT && o2 != operatorLE) ||
		(o2 == operatorLE && !m_allocator.Compare(last, current->GetItem(pos))) ||
		m_allocator.Compare(current->GetItem(pos), last))
	{
		next = GotoB(current, NULL);
		npos = pos;

		if (OpStatus::IsError(rv = SearchNextItem(&next, &npos, current->GetItem(pos))))
		{
			GotoB(NULL, next);
			GotoB(NULL, current);
			return rv;
		}

		eod = rv == OpBoolean::IS_FALSE;

		if (!eod)
			CopyItem(next_item.DataPtr(), next, npos);

		GotoB(NULL, next);

		rv = DeleteItem(current, pos);

		GotoB(NULL, current);
		current = NULL;

		if (OpStatus::IsError(rv))
			return rv;

		RETURN_IF_ERROR(RebalanceRoot());

		if (eod)
			break;

		RETURN_IF_ERROR(SearchItem(&current, &pos, next_item.DataPtr()));
	}

	if (current != NULL)
		GotoB(NULL, current);

	return OpStatus::OK;
}

OP_BOOLEAN BTreeBase::Search(void *item)
{
	BTreeBranch *current;
	int pos;
	OP_BOOLEAN rv;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if (this->m_root == 0)
		return OpBoolean::IS_FALSE;

	RETURN_IF_ERROR(rv = SearchItem(&current, &pos, item));

	// cannot be empty, since it was checked above
	while (pos >= current->size && current->parent != NULL)
	{
		CopyItem(item, current, 0);
		current = GotoB(current->parent, current);
		pos = BinarySearch(current, item, m_allocator);
	}

	if (pos >= current->size)
	{
		GotoB(NULL, current);
		return OpBoolean::IS_FALSE;
	}

	CopyItem(item, current, pos);
	GotoB(NULL, current);

	return rv == OpBoolean::IS_TRUE ? rv : OpStatus::OK;
}

OP_BOOLEAN BTreeBase::GetFirst(void *item)
{
	BSCache::Item::DiskId ptr;
	BTreeBranch *current;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if ((ptr = this->m_root) == 0)
		return OpBoolean::IS_FALSE;  // empty

	current = NULL;

	LoopDetector<BSCache::Item::DiskId> loopDetector;
	do {
		RETURN_IF_ERROR(loopDetector.CheckNext(ptr));
		RETURN_IF_ERROR(Goto(&current, ptr, current));

		ptr = current->Pointer(0);
	} while (!IsLeaf(current));

	CopyItem(item, current, 0);
	GotoB(NULL, current);

	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN BTreeBase::GetLast(void *item)
{
	BSCache::Item::DiskId ptr;
	BTreeBranch *current;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if ((ptr = this->m_root) == 0)
		return OpBoolean::IS_FALSE;  // empty

	current = NULL;

	LoopDetector<BSCache::Item::DiskId> loopDetector;
	do {
		RETURN_IF_ERROR(loopDetector.CheckNext(ptr));
		RETURN_IF_ERROR(Goto(&current, ptr, current));

		ptr = current->Rightmost();
	} while (!IsLeaf(current));

	CopyItem(item, current, current->size - 1);
	GotoB(NULL, current);

	return OpBoolean::IS_TRUE;
}


IteratorBase *BTreeBase::Search(const void *item, SearchOperator oper)
{
	BSCache::Item::DiskId ptr;
	BTreeBranch *current;
	int pos;
	BOOL matched = FALSE;
	IteratorBase *rv;
	OP_BOOLEAN err;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if ((ptr = this->m_root) == 0)
		return NewResult(NULL, 0, oper, item);

	current = NULL;

	if (oper == operatorLT || oper == operatorLE)
	{
		LoopDetector<BSCache::Item::DiskId> loopDetector;
		do {
			RETURN_VALUE_IF_ERROR(loopDetector.CheckNext(ptr), NULL);
			RETURN_VALUE_IF_ERROR(Goto(&current, ptr, current), NULL);

			ptr = current->Pointer(0);
		} while (!IsLeaf(current));

		pos = 0;

		if ((oper == operatorLT && m_allocator.Compare(item, current->GetItem(0))) ||
			(oper == operatorLE && !m_allocator.Compare(current->GetItem(0), item)))
		{
			GotoB(NULL, current);
			current = NULL;
			pos = -1;
		}
	}
	else {
		err = SearchItem(&current, &pos, item);
		RETURN_VALUE_IF_ERROR(err, NULL);
		matched = err == OpBoolean::IS_TRUE;
	}

	if ((rv = NewResult(current, pos, oper, item)) == NULL)
	{
		GotoB(NULL, current);
		return NULL;
	}

	if (oper == operatorGT && matched && rv->Count() != 0)
	{
		if (!rv->Next())
		{
			if (OpStatus::IsError(rv->Error()))
			{
				OP_DELETE(rv);
				return NULL;
			}
		}
	}

	if (rv->End() && !rv->Empty())
	{
		OP_DELETE(rv);
		return NewResult(NULL, 0, oper, item);
	}

	return rv;
}

IteratorBase *BTreeBase::SearchFirst(void)
{
	BSCache::Item::DiskId ptr;
	BTreeBase::BTreeBranch *current;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if ((ptr = this->m_root) == 0)
		return NewResult(NULL, 0, operatorEQ, NULL);

	current = NULL;

	LoopDetector<BSCache::Item::DiskId> loopDetector;
	do {
		RETURN_VALUE_IF_ERROR(loopDetector.CheckNext(ptr), NULL);
		RETURN_VALUE_IF_ERROR(Goto(&current, ptr, current), NULL);

		ptr = current->Pointer(0);
	} while (!IsLeaf(current));

	return NewResult(current, 0, operatorGE, current->GetItem(0));
}

IteratorBase *BTreeBase::SearchLast(void)
{
	BSCache::Item::DiskId ptr;
	BTreeBase::BTreeBranch *current;

	m_cache->m_NUR_mark = (m_cache->m_NUR_mark + 1) & NUR_MASK;

	if ((ptr = this->m_root) == 0)
		return NewResult(NULL, 0, operatorEQ, NULL);

	current = NULL;

	LoopDetector<BSCache::Item::DiskId> loopDetector;
	do {
		RETURN_VALUE_IF_ERROR(loopDetector.CheckNext(ptr), NULL);
		RETURN_VALUE_IF_ERROR(Goto(&current, ptr, current), NULL);

		ptr = current->Rightmost();
	} while (!IsLeaf(current));

	return NewResult(current, current->size - 1, operatorLE, current->GetItem(current->size - 1));
}


BOOL3 BTreeBase::Empty(void)
{
	BTreeBase::BTreeBranch *current;
	BOOL empty;

	if (this->m_root == 0)
		return YES;

	RETURN_VALUE_IF_ERROR(Goto(&current, this->m_root, NULL), MAYBE);

	empty = current->size == 0;

	GotoB(NULL, current);

	return empty ? YES : NO;
}

OP_STATUS BTreeBase::Clear(void)
{
	if (this->m_root != 0)
	{
		RETURN_IF_ERROR(DeleteBranch(this->m_root));
	}

	m_root = 0;

	return OpStatus::OK;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t BTreeBase::EstimateMemoryUsed() const
{
	return
		sizeof(m_allocator) +
		sizeof(m_root) +
		sizeof(m_cache) +
		sizeof(m_reserved_id);
}
#endif


OP_STATUS BTreeBase::NewBranch(BTreeBase::BTreeBranch **branch, BTreeBase::BTreeBranch *parent)  // parent == NULL ... creating root
{
	BSCache::Item *bi;

	OP_ASSERT(parent == NULL || parent->reference_count > 0);

	if (!m_cache->GetStorage()->InTransaction())
	{
		RETURN_IF_ERROR(m_cache->GetStorage()->BeginTransaction(m_cache->m_flush_mode == BSCache::JournalOnly));
	}

	RETURN_IF_ERROR(m_cache->Create(&bi, parent, 0));
	*branch = (BTreeBase::BTreeBranch *)bi;
	(*branch)->modified = TRUE;
	(*branch)->owner = this;

	if (this->m_root == 0)
	{
		if (m_reserved_id != 0)
		{
			(*branch)->disk_id = (BSCache::Item::DiskId)(m_reserved_id / m_cache->m_storage.GetBlockSize());
			(*branch)->id_reserved = TRUE;
			(*branch)->journalled = TRUE;
			++m_cache->m_journal_count;
			m_reserved_id = 0;
		}

		this->m_root = (*branch)->disk_id;
	}


	while (parent != NULL)
	{
		m_cache->Load(&bi, parent);  // will always succeed, since parent is in memory
		parent = ((BTreeBase::BTreeBranch *)bi)->parent;
	}

	return OpStatus::OK;
}

OP_STATUS BTreeBase::DeleteBranch(BSCache::Item::DiskId branch)
{
	int i;
	BTreeBranch *rb;

	RETURN_IF_ERROR(Goto(&rb, branch, NULL));

	if (!IsLeaf(rb))
	{
		for (i = 0; i < rb->size; ++i)
		{
			RETURN_IF_ERROR(DeleteBranch(rb->Pointer(i)));
		}
		RETURN_IF_ERROR(DeleteBranch(rb->Rightmost()));
	}

	return DeleteLeaf(rb);
}

OP_STATUS BTreeBase::DeleteLeaf(BTreeBase::BTreeBranch *leaf)
{
	if (!m_cache->GetStorage()->InTransaction())
	{
		RETURN_IF_ERROR(m_cache->GetStorage()->BeginTransaction(m_cache->m_flush_mode == BSCache::JournalOnly));
	}

//	if (leaf->id_reserved)
//		m_cache->m_storage.Write(leaf, 0, ((OpFileLength)leaf->disk_id) * m_cache->m_storage.GetBlockSize());

	m_cache->Unlink(leaf);

//	if (IsRoot(leaf))
//		m_cache->Flush();
// I'm not sure if I should have just deleted these 4 lines, but the Write is invalid at this point and Flush shouldn't be called (perhaps with JournalAll)
	GotoB(NULL, leaf);

	return OpStatus::OK;
}

OP_STATUS BTreeBase::RebalanceRoot(void)
{
	BTreeBase::BTreeBranch *root, *current;
	int pos;
	OP_STATUS rv;

	RETURN_IF_ERROR(Goto(&root, this->m_root, NULL));

	if (!IsLeaf(root))
	{
		if (root->size <= 0)
		{
			if (OpStatus::IsError(rv = Goto(&current, root->Rightmost(), NULL)))
			{
				GotoB(NULL, root);
				return rv;  // root is empty, but that isn't fatal
			}

			RETURN_IF_ERROR(Modify(root));
			root->CopyData(current);

			for (pos = 0; pos < root->size; ++pos)
				SetParent(root->Pointer(pos), root);
			SetParent(root->Rightmost(), root);

			CHECK_INTEGRITY(root);

			RETURN_IF_ERROR(DeleteLeaf(current));
		}

		GotoB(NULL, root);
	}
	else {
		if (root->size <= 0)
		{
			RETURN_IF_ERROR(DeleteLeaf(root));
			m_root = 0;
		}
		else
			GotoB(NULL, root);
	}
	
	return OpStatus::OK;
}

void BTreeBase::SetParent(BSCache::Item::DiskId branch, BTreeBase::BTreeBranch *parent)
{
	BTreeBase::BTreeBranch *tmp, *tmp_parent;
	BTreeBase::BTreeBranch *old_parent;
	BTreeBase::BTreeBranch *new_parent;
	BSCache::Item *bi;
	int i;

	if (branch == 0)
		return;

	if ((tmp = m_cache->LoadFromCache(branch)) == NULL)
		return;

	old_parent = tmp->parent;
	new_parent = parent;

	if (old_parent != new_parent)
	{
		// new branch is inserted between referenced child branch and its parent - reference it too
		if (parent != NULL && tmp->reference_count > 1)
		{
			OP_ASSERT(tmp->parent == NULL || tmp->parent->reference_count > 0);
			do {
				m_cache->Load(&bi, parent);
				parent = ((BTreeBase::BTreeBranch *)bi)->parent;
			} while (parent != NULL);

			if (!IsRoot(old_parent))
			{
				for (i = 2; i < tmp->reference_count; ++i)
				{
					parent = new_parent;
					do {
						m_cache->Load(&bi, parent);
						parent = ((BTreeBase::BTreeBranch *)bi)->parent;
					} while (parent != NULL);
				}
			}
			else {
				for (i = 2; i < tmp->reference_count; ++i)
					m_cache->Load(&bi, new_parent);
			}
		}

		tmp->parent = new_parent;

		if (old_parent != NULL && tmp->reference_count > 1)
		{
			if (!IsRoot(old_parent))  // if IsRoot, we have inserted a new branch in between
			{
				for (i = 2; i < tmp->reference_count; ++i)
				{
					parent = old_parent;
					do {
						tmp_parent = parent->parent;
						m_cache->Release(parent);
						parent = tmp_parent;
					} while (parent != NULL);
				}
			}

			do {
				parent = old_parent->parent;
				m_cache->Release(old_parent);
				old_parent = parent;
			} while (old_parent != NULL);
		}
	}

	m_cache->Release(tmp);
}

OP_STATUS BTreeBase::Goto(BTreeBase::BTreeBranch **rv, BSCache::Item::DiskId branch, BTreeBase::BTreeBranch *previous)
{
	BTreeBase::BTreeBranch *tmp, *parent;
	BSCache::Item *bi;
	OP_STATUS err;

	if (branch == 0)
	{
		OP_ASSERT(0);
		GotoB(NULL, previous);
		return OpStatus::ERR_PARSING_FAILED;
	}

	if (OpStatus::IsError(err = m_cache->Load(&bi, branch)))
	{
		GotoB(NULL, previous);
		return err;
	}
	*rv = (BTreeBase::BTreeBranch *)bi;
	(*rv)->owner = this;

	if (previous != NULL && branch != this->m_root)
		parent = previous;
	else
		parent = (*rv)->parent;

	tmp = parent;
	while (tmp != NULL)
	{
		if (tmp == *rv) // Found *rv while following ->parent, avoid creating loop
		{
			OP_ASSERT(0);
			GotoB(NULL, previous);
			return OpStatus::ERR_PARSING_FAILED;
		}
		m_cache->Load(&bi, tmp);
		tmp = ((BTreeBase::BTreeBranch *)bi)->parent;
	}
	
	(*rv)->parent = parent;

	if (previous != NULL) {
		do {
			tmp = previous->parent;
			m_cache->Release(previous);
			previous = tmp;
		} while (previous != NULL);
	}

	return OpStatus::OK;
}

BTreeBase::BTreeBranch *BTreeBase::GotoB(BTreeBase::BTreeBranch *branch, BTreeBase::BTreeBranch *previous)
{
	BTreeBase::BTreeBranch *b, *tmp;
	BSCache::Item *bi;

	OP_ASSERT(branch == NULL || branch->disk_id == this->m_root || branch->parent != NULL);

	b = NULL;
	if (branch != NULL)
	{
		m_cache->Load(&bi, branch);
		b = (BTreeBase::BTreeBranch *)bi;
		tmp = b->parent;
		while (tmp != NULL)
		{
			m_cache->Load(&bi, tmp);
			tmp = ((BTreeBase::BTreeBranch *)bi)->parent;
		}
	}

	
	if (previous != NULL) {
		do {
			tmp = previous->parent;
			m_cache->Release(previous);
			previous = tmp;
		} while (previous != NULL);
	}

	return b;
}

OP_STATUS BTreeBase::Modify(BTreeBase::BTreeBranch *branch)
{
	if (!m_cache->GetStorage()->InTransaction())
		RETURN_IF_ERROR(m_cache->GetStorage()->BeginTransaction(m_cache->m_flush_mode == BSCache::JournalOnly));

	branch->modified = TRUE;
	return OpStatus::OK;
}

BSCache::Item::DiskId BTreeBase::SafePointer(BTreeBase::BTreeBranch *branch)
{
	if (branch->disk_id < 0)
	{
		BSCache::Item::DiskId new_id;
		new_id = (BSCache::Item::DiskId)(m_cache->m_storage.Reserve() / m_cache->m_storage.GetBlockSize());
		if (new_id > 0)
		{
			branch->id_reserved = TRUE;
			branch->journalled = TRUE;
			++m_cache->m_journal_count;
			branch->modified = TRUE;

			branch->OnIdChange(new_id, branch->disk_id);

			branch->disk_id = new_id;
		}
	}

	return branch->disk_id;
}

OP_BOOLEAN BTreeBase::SearchItem(BTreeBranch **rbranch, int *rpos, const void *item)
{
	BSCache::Item::DiskId ptr;
	BTreeBranch *current;
	int pos;

	*rbranch = NULL;
	*rpos = -1;

	// root is assumed to be valid at this point
	OP_ASSERT(this->m_root != 0);
	if ((ptr = this->m_root) == 0)
		return OpBoolean::IS_FALSE;

	current = NULL;
	LoopDetector<BSCache::Item::DiskId> loopDetector;
	do {
		RETURN_IF_ERROR(loopDetector.CheckNext(ptr));
		RETURN_IF_ERROR(Goto(&current, ptr, current));

		pos = BinarySearch(current, item, m_allocator);

		if (pos >= current->size)
		{
			ptr = current->Rightmost();
			continue;
		}

		// current->GetItem(pos) is >= item here
		if (!m_allocator.Compare(item, current->GetItem(pos))) // item isn't < current->GetItem(pos) ... must be ==
		{
			*rbranch = current;
			*rpos = pos;
			return OpBoolean::IS_TRUE;
		}

		ptr = current->Pointer(pos);
	} while (ptr != 0);

	*rbranch = current;
	*rpos = pos;

	return OpBoolean::IS_FALSE;
}

// modified BTreeBase::Result::_Next()
OP_BOOLEAN BTreeBase::SearchNextItem(BTreeBranch **rbranch, int *rpos, const void *item)
{
	OpString8 key_buf;
	BSCache::Item::DiskId tmp_ptr;

	RETURN_OOM_IF_NULL(key_buf.Reserve((int)m_allocator.size));

	++*rpos;

	// go down
	LoopDetector<BSCache::Item::DiskId> loopDetector;
	while (!IsLeaf(*rbranch) && *rpos <= (*rbranch)->size)
	{
		tmp_ptr = *rpos == (*rbranch)->size ? (*rbranch)->Rightmost() : (*rbranch)->Pointer(*rpos);

		RETURN_IF_ERROR(loopDetector.CheckNext(tmp_ptr));
		RETURN_IF_ERROR(Goto(rbranch, tmp_ptr, *rbranch));

		*rpos = 0;
	}

	// cannot go down - go to the first valid uppper position
	while (*rpos >= (*rbranch)->size)
	{
		if (IsRoot(*rbranch))
			return OpBoolean::IS_FALSE;

		tmp_ptr = SafePointer(*rbranch);
		CopyItem(key_buf.DataPtr(), *rbranch, (*rbranch)->size - 1);

		if ((*rbranch = GotoB((*rbranch)->parent, *rbranch)) == NULL)
			return OpStatus::ERR;

		if ((*rbranch)->Rightmost() == tmp_ptr)
			*rpos = (*rbranch)->size;
		else
			*rpos = BinarySearch(*rbranch, key_buf.DataPtr(), m_allocator);
	}

	return OpBoolean::IS_TRUE;
}

OP_STATUS BTreeBase::InsertItem(BTreeBranch *branch, int pos, const void *item, BSCache::Item::DiskId overflow)
{
	int btree_size = BTreeSize();

	if (branch->size >= btree_size)
	{
		BTreeBranch *n, *n2;
		OP_STATUS rv;
		int i;

		if (branch->disk_id == m_root)
		{
			RETURN_IF_ERROR(NewBranch(&n, branch));
			if (OpStatus::IsError(rv = NewBranch(&n2, branch)))
			{
				OpStatus::Ignore(DeleteLeaf(n));
				return rv;
			}

			if (pos == (btree_size + 1) / 2)
			{
				n->size = (btree_size - 1) / 2 + 1;
				CopyPtrItems(n, 0, branch, 0, n->size);
				n->Rightmost() = branch->Pointer(n->size);
				
				n2->size = branch->size - n->size;
				CopyPtrItems(n2, 0, branch, n->size, n2->size);
				n2->Pointer(0) = overflow;
				n2->Rightmost() = branch->Rightmost();

				branch->size = 1;
				CopyItem(branch, 0, item);
				branch->Pointer(0) = SafePointer(n);
				branch->Rightmost() = SafePointer(n2);

				RETURN_IF_ERROR(Modify(n));
				RETURN_IF_ERROR(Modify(n2));
				RETURN_IF_ERROR(Modify(branch));

				if (!IsLeaf(n))
				{
					for (i = 0; i < n->size; ++i)
						SetParent(n->Pointer(i), n);
					SetParent(n->Rightmost(), n);

					for (i = 0; i < n2->size; ++i)
						SetParent(n2->Pointer(i), n2);
					SetParent(n2->Rightmost(), n2);
				}

				CHECK_INTEGRITY(n);
				CHECK_INTEGRITY(n2);
				CHECK_INTEGRITY(branch);

				GotoB(NULL, n);
				GotoB(NULL, n2);

				return OpStatus::OK;
			}

			n->size = (btree_size - 1) / 2 + 1 - (pos <= btree_size / 2);
			CopyPtrItems(n, 0, branch, 0, n->size);
			n->Rightmost() = branch->Pointer(n->size);
				
			n2->size = branch->size - n->size - 1;
			CopyPtrItems(n2, 0, branch, branch->size - n2->size, n2->size);
			n2->Rightmost() = branch->Rightmost();

			branch->size = 1;
			CopyItem(branch, 0, branch, n->size);
			branch->Pointer(0) = SafePointer(n);
			branch->Rightmost() = SafePointer(n2);

			RETURN_IF_ERROR(Modify(n));
			RETURN_IF_ERROR(Modify(n2));
			RETURN_IF_ERROR(Modify(branch));

			if (pos <= btree_size / 2)
			{
				RETURN_IF_ERROR(InsertItem(n, pos, item, overflow));
			}
			else {
				RETURN_IF_ERROR(InsertItem(n2, pos - n->size - 1, item, overflow));
			}

			if (!IsLeaf(n))
			{
				for (i = 0; i < n->size; ++i)
					SetParent(n->Pointer(i), n);
				SetParent(n->Rightmost(), n);

				for (i = 0; i < n2->size; ++i)
					SetParent(n2->Pointer(i), n2);
				SetParent(n2->Rightmost(), n2);
			}

			CHECK_INTEGRITY(n);
			CHECK_INTEGRITY(n2);
			CHECK_INTEGRITY(branch);

			GotoB(NULL, n);
			GotoB(NULL, n2);

			return OpStatus::OK;
		}  // branch->disk_id == this->m_root

		RETURN_IF_ERROR(NewBranch(&n2, branch->parent));

		n = GotoB(branch->parent, NULL);

		n2->size = btree_size / 2 - (pos > (btree_size + 1) / 2);
		CopyPtrItems(n2, 0, branch, branch->size - n2->size, n2->size);
		n2->Rightmost() = branch->Rightmost();

		if (pos == (btree_size + 1) / 2)  // new item is in the middle
		{
			branch->Rightmost() = n2->Pointer(0);
			n2->Pointer(0) = overflow;
			branch->size -= n2->size;

			RETURN_IF_ERROR(Modify(branch));

			rv = InsertItem(n, BinarySearch(n, item, m_allocator), item, SafePointer(n2));

			GotoB(NULL, n);
			if (OpStatus::IsError(rv))
			{
				OpStatus::Ignore(DeleteLeaf(n2));
				return rv;
			}
		}
		else {
			rv = InsertItem(n, BinarySearch(n, branch->GetItem(branch->size - n2->size - 1), m_allocator),
				branch->GetItem(branch->size - n2->size - 1), SafePointer(n2));

			GotoB(NULL, n);
			if (OpStatus::IsError(rv))
			{
				OpStatus::Ignore(DeleteLeaf(n2));
				return rv;
			}

			branch->Rightmost() = branch->Pointer(branch->size - n2->size - 1);
			branch->size -= n2->size + 1;

			RETURN_IF_ERROR(Modify(branch));

			if (pos < (btree_size + 1) / 2)  // new item is on left
			{
				RETURN_IF_ERROR(InsertItem(branch, pos, item, overflow));
			}
			else {
				RETURN_IF_ERROR(InsertItem(n2, pos - branch->size - 1, item, overflow));
			}
		}

		if (!IsLeaf(n2))
		{
			for (i = 0; i < n2->size; ++i)
				SetParent(n2->Pointer(i), n2);
			SetParent(n2->Rightmost(), n2);
		}

		RETURN_IF_ERROR(Modify(n2));

		CHECK_INTEGRITY(n2);
		CHECK_INTEGRITY(branch);

		GotoB(NULL, n2);

		return OpStatus::OK;
	}

	if (pos >= branch->size)
	{
#ifdef _DEBUG
		if (branch->size > 0)
		{
			OP_ASSERT(m_allocator.Compare(branch->GetItem(branch->size - 1), item));
		}
#endif
		CopyItem(branch, branch->size, item);
		branch->Pointer(branch->size) = branch->Rightmost();
		branch->Rightmost() = overflow;
	}
	else {
#ifdef _DEBUG
		if (pos > 0)
		{
			OP_ASSERT(m_allocator.Compare(branch->GetItem(pos - 1), item));
		}
		OP_ASSERT(m_allocator.Compare(item, branch->GetItem(pos)));
#endif
		CopyPtrItems(branch, pos + 2, branch, pos + 1, branch->size - pos - 1);

		CopyItem(branch, pos + 1, branch, pos);
		CopyItem(branch, pos, item);
		branch->Pointer(pos + 1) = overflow;
	}

	if (!IsLeaf(branch))
	{
		OP_ASSERT(overflow != 0);
		SetParent(overflow, branch);
	}
#ifdef _DEBUG
	else {
		OP_ASSERT(overflow == 0);
	}
#endif

	++(branch->size);
	RETURN_IF_ERROR(Modify(branch));

	CHECK_INTEGRITY(branch);

	return OpStatus::OK;
}  // InsertItem

OP_STATUS BTreeBase::DeleteItem(BTreeBranch *branch, int pos)
{
	BTreeBranch *n;
	OP_STATUS rv;

	if (!IsLeaf(branch))
	{
		RETURN_IF_ERROR(Goto(&n, pos >= branch->size - 1 ? branch->Rightmost() : branch->Pointer(pos + 1), branch));

		LoopDetector<BSCache::Item::DiskId> loopDetector;
		while (!IsLeaf(n))
		{
			BSCache::Item::DiskId ptr = n->Pointer(0);
			RETURN_IF_ERROR(loopDetector.CheckNext(ptr));
			RETURN_IF_ERROR(Goto(&n, ptr, n));
		}

		RETURN_IF_ERROR(Modify(branch));

		CopyItem(branch, pos, n, 0);

		rv = DeleteItem(n, 0);

		CHECK_INTEGRITY(branch);

		GotoB(branch, n);

		return rv;
	}

	return RemoveItem(branch, pos);
}

OP_STATUS BTreeBase::RemoveItem(BTreeBranch *branch, int pos)
{
	BTreeBranch *parent = NULL, *n2 = NULL, *lc = NULL, *rc = NULL;
	int i;
	OP_STATUS rv;
	int btree_size = BTreeSize();

	if (branch->size <= btree_size / 2 && !IsRoot(branch))
	{
		parent = GotoB(branch->parent, NULL);

		i = BinarySearch(parent, branch->GetItem(0), m_allocator);
		if (i < parent->size && !m_allocator.Compare(branch->GetItem(0), parent->GetItem(i)))
			++i;

		--i;
		if (i < 0)
			i = 1;  // take right neighbor for the leftmost

		// if root->size == 1, i can be >= size
		if (OpStatus::IsError(rv = Goto(&n2, i >= parent->size ? parent->Rightmost() : parent->Pointer(i), parent)))
		{
			GotoB(NULL, parent);
			return rv;
		}
		parent = GotoB(branch->parent, NULL);

		OP_ASSERT(n2->disk_id != branch->disk_id);

		if (m_allocator.Compare(n2->GetItem(0), branch->GetItem(0)))
		{
			lc = n2;
			rc = branch;
		}
		else {
			lc = branch;
			rc = n2;
		}
	}

	RETURN_IF_ERROR(Modify(branch));

	OP_ASSERT(pos < branch->size);
	if (pos < branch->size)
	{
		m_allocator.Destruct(branch->GetItem(pos));
		CopyPtrItems(branch, pos, branch, pos + 1, branch->size - pos - 1);
	}
	--branch->size;

	if (branch->size >= btree_size / 2 || IsRoot(branch))
	{
		CHECK_INTEGRITY(branch);

		return OpStatus::OK;  // parent and n2 aren't referenced here
	}
	// branch is too empty here

	if (n2->disk_id == branch->disk_id)
	{
		// This causes an infinite loop lower down
		OP_ASSERT(!"The B-tree is in an illegal state. Contact search_engine module owner if reproducible");
		return OpStatus::ERR_PARSING_FAILED;
	}

	RETURN_IF_ERROR(Modify(n2));
	RETURN_IF_ERROR(Modify(parent));

	if (n2->size + branch->size >= btree_size)
	{
		Rotate(lc, parent, rc);

		OP_ASSERT(branch->size >= btree_size / 2);
		OP_ASSERT(n2->size >= btree_size / 2);
		OP_ASSERT(IsRoot(parent) || parent->size >= btree_size / 2);

		CHECK_INTEGRITY(branch);
		CHECK_INTEGRITY(parent);
		CHECK_INTEGRITY(n2);

		GotoB(NULL, parent);
		GotoB(NULL, n2);

		return OpStatus::OK;
	}

	if (OpStatus::IsError(rv = Merge(branch, lc, parent, rc)))
	{
		CHECK_INTEGRITY(branch);
		CHECK_INTEGRITY(parent);
		CHECK_INTEGRITY(n2);

		GotoB(NULL, n2);
		GotoB(NULL, parent);
		return rv;
	}

	// n2 is deleted here

	CHECK_INTEGRITY(branch);
	CHECK_INTEGRITY(parent);

	OP_ASSERT(branch->size >= btree_size / 2);

	GotoB(NULL, parent);

	return OpStatus::OK;
}

void BTreeBase::Rotate(BTreeBranch *l_child, BTreeBranch *parent, BTreeBranch *g_child)
{
	int size, shift;
	int pos;
	int i;

	OP_ASSERT(l_child->modified);
	OP_ASSERT(parent->modified);
	OP_ASSERT(g_child->modified);

	if (g_child->size == l_child->size)
	{
		OP_ASSERT(0);  // Rotate is not supposed to be called in this case
		return;
	}

	pos = BinarySearch(parent, l_child->GetItem(0), m_allocator);

	if (g_child->size < l_child->size)
	{
		size = (l_child->size + g_child->size) / 2;
		shift = size - g_child->size - 1;

		CopyPtrItems(g_child, shift + 1, g_child, 0, g_child->size);

		CopyItem(g_child, shift, parent, pos);
		g_child->Pointer(shift) = l_child->Rightmost();
		SetParent(g_child->Pointer(shift), g_child);

		for (i = 0; i < shift; ++i)
		{
			CopyPtrItems(g_child, i, l_child, l_child->size - shift + i, 1);
			SetParent(g_child->Pointer(i), g_child);
		}
		CopyItem(parent, pos, l_child, l_child->size - (shift + 1));
		l_child->Rightmost() = l_child->Pointer(l_child->size - (shift + 1));

		l_child->size -= shift + 1;
		g_child->size = size;
	}
	else {
		size = (l_child->size + g_child->size + 1) / 2;
		shift = size - l_child->size - 1;

		CopyItem(l_child, l_child->size, parent, pos);
		l_child->Pointer(l_child->size++) = l_child->Rightmost();

		for (i = 0; i < shift; ++i)
		{
			CopyPtrItems(l_child, l_child->size, g_child, i, 1);
			SetParent(l_child->Pointer(l_child->size++), l_child);
		}
		l_child->Rightmost() = g_child->Pointer(shift);
		SetParent(l_child->Rightmost(), l_child);

		CopyItem(parent, pos, g_child, shift);

		g_child->size -= shift + 1;

		CopyPtrItems(g_child, 0, g_child, shift + 1, g_child->size);
	}
}

OP_STATUS BTreeBase::Merge(BTreeBranch *dst, BTreeBranch *l_child, BTreeBranch *parent, BTreeBranch *g_child)
{
	int pos;
	int i;

	OP_ASSERT(dst->modified);
	OP_ASSERT(l_child->modified);
	OP_ASSERT(parent->modified);
	OP_ASSERT(g_child->modified);

	pos = BinarySearch(parent, l_child->GetItem(0), m_allocator);

	CopyItem(l_child, l_child->size, parent, pos);
	l_child->Pointer(l_child->size++) = l_child->Rightmost();

	for (i = 0; i < g_child->size; ++i)
		CopyPtrItems(l_child, l_child->size++, g_child, i, 1);
	l_child->Rightmost() = g_child->Rightmost();

	if (dst != l_child)
	{
		dst->CopyData(l_child);

		RETURN_IF_ERROR(DeleteLeaf(l_child));

		if (dst != g_child)
			RETURN_IF_ERROR(DeleteLeaf(g_child));
	}
	else
		RETURN_IF_ERROR(DeleteLeaf(g_child));

	for (i = 0; i < dst->size; ++i)
		SetParent(dst->Pointer(i), dst);
	SetParent(dst->Rightmost(), dst);

	if (pos == parent->size - 1)
		parent->Rightmost() = dst->disk_id;
	else parent->Pointer(pos + 1) = dst->disk_id;

	OP_STATUS status = RemoveItem(parent, pos);

	return status;
}

#ifdef _DEBUG
BOOL BTreeBase::CheckIntegrity(const BTreeBranch *branch) const
{
	int i;
	const BTreeBranch *parent, *child;

	if (branch->size <= 1)
		return TRUE;

	if (branch->parent != NULL && branch->size + 1 < BTreeSize() / 2)
		return FALSE;

	for (i = 1; i < branch->size; ++i)
	{
		if (!m_allocator.Compare(branch->GetItem(i - 1), branch->GetItem(i)))
			return FALSE;
	}
	
	if (!branch->modified) // This should almost never happen. If it does, the same contents should be on disk
	{
		int block_len, block_size, item_size, num_items;
		char *block;
		OpAutoArray<char> block_a;
		int item;
		OpFileLength pos;

		if (branch->disk_id < 0)
			return FALSE;
		item_size = (int)m_allocator.size_align32 + 4;
		block_size = m_cache->m_storage.GetBlockSize();
		pos = (OpFileLength)branch->disk_id * block_size;
		block_len = m_cache->m_storage.DataLength(pos);
		num_items = (block_len - 4) / item_size;
		if (branch->size != num_items)
			return FALSE;
		block_a.reset(block = OP_NEWA(char, block_size));
		if (block != NULL) // We may be running with "random OOM"
		{
			if (!m_cache->m_storage.Read(block, block_len, pos))
				return FALSE;
	
			if (branch->Rightmost() != *(BSCache::Item::DiskId *)(block))
				return FALSE;
			for (item = 0; item < num_items; ++item)
			{
				if (branch->Pointer(item) != *(BSCache::Item::DiskId *)(block + 4 + item * item_size))
					return FALSE;
				if (m_allocator.Compare(block + 8 + item * item_size, branch->GetItem(item)) ||
					m_allocator.Compare(branch->GetItem(item), block + 8 + item * item_size))
					return FALSE;
			}
		}
	}

	child = branch;

	while (child->parent != NULL)
	{
		parent = child->parent;
		while (parent != NULL)
		{
			i = BinarySearch(parent, child->GetItem(0), m_allocator);
			if (i > parent->size)
			{
				OP_ASSERT(0);
				return FALSE;
			}

			if (child->parent == parent && child->disk_id > 0 &&
				((i == parent->size && parent->Rightmost() != child->disk_id) ||
				 (i <  parent->size && parent->Pointer(i)  != child->disk_id)))
				return FALSE;
			if (i < parent->size && !m_allocator.Compare(child->GetItem(child->size - 1), parent->GetItem(i)))
				return FALSE;
			if (i > 0 && !m_allocator.Compare(parent->GetItem(i - 1), child->GetItem(0)))
				return FALSE;

			parent = parent->parent;
		}

		child = child->parent;
	}

	return TRUE;
}
#endif

PoolBase::PoolBase(int max_cache_branches, const TypeDescriptor &allocator) : BSCache(max_cache_branches), m_allocator(allocator)
{
}

BTreeBase::BTreeBranch *PoolBase::LoadFromCache(BSCache::Item::DiskId id)
{
	BSCache::Item *tmp;

	for (tmp = m_head; tmp != NULL; tmp = tmp->previous)
		if (tmp->disk_id == id)
		{
			Load(&tmp, tmp);
			return (BTreeBase::BTreeBranch *)tmp;
		}

	return NULL;
}

OP_STATUS PoolBase::Open(const uni_char* path, BlockStorage::OpenMode mode, int blocksize, OpFileFolder folder)
{
	RETURN_IF_ERROR(m_storage.Open(path, mode, blocksize, 0, folder));

	if (m_storage.GetBlockSize() != blocksize)
	{
		m_storage.Close();
		return OpStatus::ERR_PARSING_FAILED;
	}	

	return OpStatus::OK;
}

OP_STATUS PoolBase::Close(void)
{
	RETURN_IF_ERROR(Flush(ReleaseAll));

	ClearCache();

	if (m_storage.InTransaction())
	{
		RETURN_IF_ERROR(m_storage.Commit());
	}
	m_storage.Close();

	return OpStatus::OK;
}


OP_STATUS PoolBase::Abort(void)
{
	ClearCache();
	if (m_storage.InTransaction())
		return m_storage.Rollback();
	return OpStatus::OK;
}

OP_STATUS PoolBase::Commit(void)
{
	RETURN_IF_ERROR(Flush());

	if (!m_storage.InTransaction())
		return OpStatus::OK;

	return m_storage.Commit();
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t PoolBase::EstimateMemoryUsed() const
{
	return BSCache::EstimateMemoryUsed() +
		sizeof(m_allocator);
}
#endif


OP_STATUS BTreeBase::Result::Init(BTreeBase *owner, BTreeBase::BTreeBranch *current, int pos,
	SearchOperator oper, const void *key)
{
	RETURN_OOM_IF_NULL(m_key_buf = OP_NEWA(char, owner->m_allocator.size * 2));

	m_last_error = OpStatus::OK;
	m_btree = owner;
	m_current = current;
	m_pos = pos;
	m_oper = oper;
	if (key != NULL)
		RETURN_IF_ERROR(owner->m_allocator.Assign(m_key_buf, key));
	else
		op_memset(m_key_buf, 0, owner->m_allocator.size);
	m_loop_detector.Reset();

	return OpStatus::OK;
}

void BTreeBase::Result::Free(void)
{
	if (m_current != NULL)
	{
		m_btree->GotoB(NULL, m_current);
		m_current = NULL;
	}

	if (m_key_buf != NULL)
	{
		OP_DELETEA(m_key_buf);
		m_key_buf = NULL;
	}
}

BOOL BTreeBase::Result::_Next(void)
{
	BSCache::Item::DiskId tmp_ptr, next_ptr;

	if (_End())
	{
		m_last_error = OpStatus::ERR_OUT_OF_RANGE;
		return FALSE;
	}
	if (_Beginning() && m_last_error == OpStatus::ERR_OUT_OF_RANGE)
			m_last_error = OpStatus::OK;

	++m_pos;

	// go down
	while (!m_btree->IsLeaf(m_current) && m_pos <= m_current->size)
	{
		next_ptr = m_pos == m_current->size ? m_current->Rightmost() : m_current->Pointer(m_pos);

		RETURN_VALUE_IF_ERROR(m_last_error = m_loop_detector.CheckNext(next_ptr), FALSE);
		RETURN_VALUE_IF_ERROR(m_last_error = m_btree->Goto(&m_current, next_ptr, m_current), FALSE);

		m_pos = 0;
	}

	// cannot go down - go to the first valid upper position
	while (m_pos >= m_current->size)
	{
		if (m_btree->IsRoot(m_current))
			return FALSE;

		tmp_ptr = m_btree->SafePointer(m_current);
		RETURN_VALUE_IF_ERROR(m_last_error = m_btree->m_allocator.Assign(m_key_buf + m_btree->m_allocator.size, m_current->GetItem(m_current->size - 1)), FALSE);

		if ((m_current = m_btree->GotoB(m_current->parent, m_current)) == NULL)
		{
			m_last_error = OpStatus::ERR;
			return FALSE;
		}

		if (m_current->Rightmost() == tmp_ptr)
			m_pos = m_current->size;
		else
			m_pos = BinarySearch(m_current, m_key_buf + m_btree->m_allocator.size, m_btree->m_allocator);
	}

	return !_End();
}

BOOL BTreeBase::Result::_Prev(void)
{
	BSCache::Item::DiskId next_ptr;

	m_loop_detector.Reset(); // Cannot do global loop detection when iterating back and forth

	if (_Beginning())
	{
		m_last_error = OpStatus::ERR_OUT_OF_RANGE;
		return FALSE;
	}
	if (_End() && m_last_error == OpStatus::ERR_OUT_OF_RANGE)
			m_last_error = OpStatus::OK;

	--m_pos;

	// go down
	LoopDetector<BSCache::Item::DiskId> loopDetector;
	while (!m_btree->IsLeaf(m_current) && m_pos >= 0)
	{
		next_ptr = m_pos + 1 >= m_current->size ? m_current->Rightmost() : m_current->Pointer(m_pos + 1);

		RETURN_VALUE_IF_ERROR(m_last_error = loopDetector.CheckNext(next_ptr), FALSE);
		RETURN_VALUE_IF_ERROR(m_last_error = m_btree->Goto(&m_current, next_ptr, m_current), FALSE);

		m_pos = m_current->size - 1;
	}

	// cannot go down - go to the first valid uppper position
	while (m_pos < 0)
	{
		if (m_btree->IsRoot(m_current))
			return FALSE;

		RETURN_VALUE_IF_ERROR(m_btree->m_allocator.Assign(m_key_buf + m_btree->m_allocator.size, m_current->GetItem(0)), FALSE);

		if ((m_current = m_btree->GotoB(m_current->parent, m_current)) == NULL)
		{
			m_last_error = OpStatus::ERR;
			return FALSE;
		}

		m_pos = BinarySearch(m_current, m_key_buf + m_btree->m_allocator.size, m_btree->m_allocator) - 1;
	}

	return !_Beginning();
}

int BTreeBase::Result::_Count(void) const
{
	if (m_current == NULL || m_current->size == 0)
		return 0;

	if (m_oper == operatorEQ)
	{
		if (m_pos > m_current->size || m_pos < 0)  // used to be somewhere else, but somebody called Next() or Prev()
			return 1;

		return m_btree->m_allocator.Compare(m_key_buf, m_current->GetItem(m_pos)) ? 1 : 0;
	}

	return -1;
}

BOOL BTreeBase::Result::_End(void) const
{
	if (_Count() == 0 || m_pos >= m_current->size)
		return TRUE;

	if (m_pos < 0)  // !Empty && Beginning
		return FALSE;

	if (m_oper == operatorLT && !m_btree->m_allocator.Compare(m_current->GetItem(m_pos), m_key_buf))
		return TRUE;

	if (m_oper == operatorLE && m_btree->m_allocator.Compare(m_key_buf, m_current->GetItem(m_pos)))
		return TRUE;

	return FALSE;
}

BOOL BTreeBase::Result::_Beginning(void) const
{
	if (_Count() == 0 || m_pos < 0)
		return TRUE;

	if (m_pos >= m_current->size)  // !Empty && End
		return FALSE;

	if (m_oper == operatorGT && !m_btree->m_allocator.Compare(m_key_buf, m_current->GetItem(m_pos)))
		return TRUE;

	if (m_oper == operatorGE && m_btree->m_allocator.Compare(m_current->GetItem(m_pos), m_key_buf))
		return TRUE;

	return FALSE;
}

const void *BTreeBase::Result::_Get(void)
{
	OP_ASSERT(!_End());
	return m_current->GetItem(m_pos);
}


class BlockReader : public BlockStorage
{
public:
	CHECK_RESULT(OP_STATUS ReadPtr(OpFileLength pos, OpFileLength *ptr))
	{
		*ptr = 0;
		RETURN_IF_ERROR(m_file->SetFilePos(pos));
		return ReadOFL(m_file, ptr);
	}

	CHECK_RESULT(OP_STATUS ReadDeletedBitField(void *bf, OpFileLength pos))
	{
		RETURN_IF_ERROR(m_file->SetFilePos(pos));
		return BlockStorage::ReadFully(m_file, bf, m_blocksize);
	}
};

OP_BOOLEAN PoolBase::CheckConsistency(UINT32 root, BOOL thorough)
{
	int block_len, block_size, item_size, num_items, num_items2;
	char *block, *block2, *deleted, *refs;
	OpAutoArray<char> block_a, block2_a, deleted_a, refs_a;
	int item;
	OpFileLength next, fsize, pos;
	UINT32 ptr, ptr2, rightmost, num_blocks, b, b2;

	block_size = m_storage.GetBlockSize();
	fsize = m_storage.GetFileSize();
	num_blocks = (UINT32)(fsize/block_size);
	item_size = (int)m_allocator.size_align32 + 4;

	block_a  .reset(block   = OP_NEWA(char, block_size));
	block2_a .reset(block2  = OP_NEWA(char, block_size));
	deleted_a.reset(deleted = OP_NEWA(char, (num_blocks + 7) / 8));
	refs_a   .reset(refs    = OP_NEWA(char, (num_blocks + 7) / 8));
	RETURN_OOM_IF_NULL(block);
	RETURN_OOM_IF_NULL(block2);
	RETURN_OOM_IF_NULL(deleted);
	RETURN_OOM_IF_NULL(refs);

	op_memset(deleted, 0, (num_blocks + 7) / 8);
	op_memset(refs,    0, (num_blocks + 7) / 8);

	RETURN_IF_ERROR(((BlockReader *)&m_storage)->ReadPtr(0, &next));
	if (next >= fsize)
		return OpBoolean::IS_FALSE;  // corrupted file

	for (b = 1; b < num_blocks; b += 8 * block_size + 1)
	{
		pos = (OpFileLength)b * block_size;
		RETURN_IF_ERROR(((BlockReader *)&m_storage)->ReadDeletedBitField(block, pos));
		for (b2 = b+1; b2 < b+1+8*block_size && b2 < num_blocks; ++b2)
			if (block[(b2 - b - 1) / 8] & (1 << ((b2 - b - 1) % 8)))
				deleted[b2 / 8] |= (1 << (b2 % 8));
		deleted[b / 8] |= (1 << (b % 8));
	}
	op_memcpy(refs, deleted, (num_blocks + 7) / 8);

	for (b = 2; b < num_blocks; ++b)
	{
		pos = (OpFileLength)b * block_size;

		if (deleted[b / 8] & (1 << (b % 8)))
		{
			if (m_storage.IsStartBlock(pos))
				return OpBoolean::IS_FALSE;
			continue;
		}

		if (m_storage.IsStartBlocksSupported() && !m_storage.IsStartBlock(pos))
			return OpBoolean::IS_FALSE;
		block_len = m_storage.DataLength(pos);
		if (block_len == 0 && m_head != NULL)
			continue;  // empty btree might be tolerated for reserved pages during a transaction
		if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
			return OpBoolean::IS_FALSE;

		num_items = (block_len - 4) / item_size;
		
		if (root != 0 && b != root && num_items + 1 < ((block_size - 12 - 4) / item_size) / 2)
			return OpBoolean::IS_FALSE;

		if (!m_storage.Read(block, block_len, pos))
			return OpStatus::ERR_NO_ACCESS;

		rightmost = *(UINT32 *)(block);

		for (item = 0; item <= num_items; ++item)
		{
			ptr = item==num_items ? rightmost : *(UINT32 *)(block + 4 + item * item_size);
			if (   ((rightmost == 0) != (ptr == 0)) // ptr_error
				|| (ptr >= num_blocks || (ptr - 1) % (8 * block_size + 1) == 0 || ptr == b) // ptr_bad_error
				|| (ptr && (deleted[ptr / 8] & (1 << (ptr % 8))) != 0) // ptr_deleted_error
				|| (item > 0 && item < num_items &&
					!m_allocator.Compare(block + 8 + (item - 1) * item_size, block + 8 + item * item_size))) // sort_error
				return OpBoolean::IS_FALSE;
			if (ptr) {
				if ((refs[ptr / 8] & (1 << (ptr % 8))) != 0) // ref_error
					return OpBoolean::IS_FALSE;
				refs[ptr / 8] |= (1 << (ptr % 8));
			}
			if (ptr && thorough)
			{
				pos = (OpFileLength)ptr * block_size;
				block_len = m_storage.DataLength(pos);
				if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
					return OpBoolean::IS_FALSE;
				num_items2 = (block_len - 4) / item_size;
				if (!m_storage.Read(block2, block_len, pos))
					return OpStatus::ERR_NO_ACCESS;
				if (   (item > 0         && !m_allocator.Compare(block + 8 + (item - 1) * item_size, block2 + 8))
					|| (item < num_items && !m_allocator.Compare(block2 + 8 + (num_items2 - 1) * item_size, block + 8 + item * item_size)))
					return OpBoolean::IS_FALSE;
				if (item > 0) {
					ptr2 = ptr;
					LoopDetector<UINT32> loopDetector;
					while (ptr2)
					{
						pos = (OpFileLength)ptr2 * block_size;
						block_len = m_storage.DataLength(pos);
						if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
							return OpBoolean::IS_FALSE;
						if (!m_storage.Read(block2, block_len, pos))
							return OpStatus::ERR_NO_ACCESS;
						ptr2 = *(UINT32 *)(block2 + 4);
						if (ptr2 &&
							((ptr2 - 1) % (8 * block_size + 1) == 0 ||
							 (deleted[ptr2 / 8] & (1 << (ptr2 % 8))) != 0))
							return OpBoolean::IS_FALSE;
						if (!m_allocator.Compare(block + 8 + (item - 1) * item_size, block2 + 8))
							return OpBoolean::IS_FALSE;
						if (OpStatus::IsError(loopDetector.CheckNext(ptr2)))
							return OpBoolean::IS_FALSE;
					}
				}
				if (item < num_items)
				{
					ptr2 = ptr;
					LoopDetector<UINT32> loopDetector;
					while (ptr2)
					{
						pos = (OpFileLength)ptr2 * block_size;
						block_len = m_storage.DataLength(pos);
						if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
							return OpBoolean::IS_FALSE;
						num_items2 = (block_len - 4) / item_size;
						if (!m_storage.Read(block2, block_len, pos))
							return OpStatus::ERR_NO_ACCESS;
						ptr2 = *(UINT32 *)(block2);
						if (ptr2 &&
							((ptr2 - 1) % (8 * block_size + 1) == 0 ||
							 (deleted[ptr2 / 8] & (1 << (ptr2 % 8))) != 0))
							return OpBoolean::IS_FALSE;
						if (!m_allocator.Compare(block2 + 8 + (num_items2 - 1) * item_size, block + 8 + item * item_size))
							return OpBoolean::IS_FALSE;
						if (OpStatus::IsError(loopDetector.CheckNext(ptr2)))
							return OpBoolean::IS_FALSE;
					}
				}
			}			
		}
	}
	if (root != 0)
	{
		// Check that all branches are referenced, deleted or a deleted-bit-field
		for (b = 2; b < num_blocks; ++b)
			if (b != root && (refs[b / 8] & (1 << (b % 8))) == 0)
				return OpBoolean::IS_FALSE;
	}

	return OpBoolean::IS_TRUE;
}

#if defined(LINGOGI) && (defined(_DEBUG) || defined(SELFTEST) || defined(SEARCH_ENGINE_LOG))
OP_BOOLEAN PoolBase::PrintBTree(UINT32 root)
{
	int block_len, block_size, item_size, num_items, num_items2;
	char *block, *block2, *deleted, *refs, *referr;
	OpAutoArray<char> block_a, block2_a, deleted_a, refs_a, referr_a;
	int item;
	int ptr_errors, sort_errors, size_errors, ref_errors;
	BOOL ptr_error, ptr_bad_error, ptr_deleted_error, ptr_sort_error, ptr_sort_recursive_error, sort_error, size_error, ref_error;
	OpFileLength next, fsize, pos;
	UINT32 ptr, ptr2, rightmost, num_blocks, b, b2;

	ptr_errors = 0;
	sort_errors = 0;
	size_errors = 0;
	ref_errors = 0;

	block_size = m_storage.GetBlockSize();
	fsize = m_storage.GetFileSize();
	num_blocks = (UINT32)(fsize/block_size);
	item_size = (int)m_allocator.size_align32 + 4;

	block_a  .reset(block   = OP_NEWA(char, block_size));
	block2_a .reset(block2  = OP_NEWA(char, block_size));
	deleted_a.reset(deleted = OP_NEWA(char, (num_blocks + 7) / 8));
	refs_a   .reset(refs    = OP_NEWA(char, (num_blocks + 7) / 8));
	referr_a .reset(referr  = OP_NEWA(char, (num_blocks + 7) / 8));
	RETURN_OOM_IF_NULL(block);
	RETURN_OOM_IF_NULL(block2);
	RETURN_OOM_IF_NULL(deleted);
	RETURN_OOM_IF_NULL(refs);
	RETURN_OOM_IF_NULL(referr);

	op_memset(deleted, 0, (num_blocks + 7) / 8);
	op_memset(refs,    0, (num_blocks + 7) / 8);
	op_memset(referr,  0, (num_blocks + 7) / 8);

	RETURN_IF_ERROR(((BlockReader *)&m_storage)->ReadPtr(0, &next));
	if (next >= fsize)
		return OpBoolean::IS_FALSE;  // corrupted file

	printf("\n<table border=0>\n");
	printf("<tr><td colspan=7>Legend:</td></tr>\n");
	printf("<tr><td><b>000:</b></td><td></td><td></td><td colspan=7>Branch number</td></tr>\n");
	printf("<tr><td><b>000:</b></td><td colspan=2>Free</td><td colspan=7>Free branch</td></tr>\n");
	printf("<tr><td bgcolor=#FF40FF><b>000:</b></td><td></td><td></td><td colspan=7>Bad block size</td></tr>\n");
	printf("<tr><td bgcolor=#FFC0FF><b>000:</b></td><td></td><td></td><td colspan=7>Bad number of items in branch</td></tr>\n");
	printf("<tr><td bgcolor=#808080><b>000:</b></td><td></td><td></td><td colspan=7>Unreferenced (leaked) branch</td></tr>\n");
	printf("<tr><td></td><td><b>000</b></td><td></td><td colspan=7>Pointer to child branch</td></tr>\n");
	printf("<tr><td></td><td><b>---</b></td><td></td><td colspan=7>Leaf</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#FF40FF><b>000</b></td><td></td><td colspan=7>Illegal pointer value</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#808080><b>000</b></td><td></td><td colspan=7>Pointer to free branch</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#FF8080><b>000</b></td><td></td><td colspan=7>Pointer used more than once</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#FFC080><b>000</b></td><td></td><td colspan=7>Child branch sort error</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#FFC0FF><b>000</b></td><td></td><td colspan=7>Recursive child branch sort error</td></tr>\n");
	printf("<tr><td></td><td bgcolor=#FFFF80><b>000</b></td><td></td><td colspan=7>Mixed leaf/nonleaf pointers</td></tr>\n");
	printf("<tr><td></td><td></td><td><b>0000</b></td><td colspan=7>Data</td></tr>\n");
	printf("<tr><td></td><td></td><td bgcolor=#FFC080><b>0000</b></td><td colspan=7>Data sort error</td></tr>\n");
	printf("<tr></tr>\n");
	printf("<tr><td colspan=7>Btree:</td></tr>\n");

	for (b = 1; b < num_blocks; b += 8 * block_size + 1)
	{
		pos = (OpFileLength)b * block_size;
		RETURN_IF_ERROR(((BlockReader *)&m_storage)->ReadDeletedBitField(block, pos));
		for (b2 = b+1; b2 < b+1+8*block_size && b2 < num_blocks; ++b2)
			if (block[(b2 - b - 1) / 8] & (1 << ((b2 - b - 1) % 8)))
				deleted[b2 / 8] |= (1 << (b2 % 8));
	}
	op_memcpy(refs, deleted, (num_blocks + 7) / 8);

	for (b = 2; b < num_blocks; ++b)
	{
		if ((b - 1) % (8 * block_size + 1) == 0)
			continue;

		if (deleted[b / 8] & (1 << (b % 8)))
			continue;

		pos = (OpFileLength)b * block_size;
		block_len = m_storage.DataLength(pos);
		if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
			continue;

		num_items = (block_len - 4) / item_size;

		if (!m_storage.Read(block, block_len, pos))
			return OpStatus::ERR_NO_ACCESS;

		rightmost = *(UINT32 *)(block);
		for (item = 0; item <= num_items; ++item)
		{
			ptr = item==num_items ? rightmost : *(UINT32 *)(block + 4 + item * item_size);
			if (ptr != 0 && ptr < num_blocks) {
				if ((refs[ptr / 8] & (1 << (ptr % 8))) != 0)
					referr[ptr / 8] |= (1 << (ptr % 8));
				refs[ptr / 8] |= (1 << (ptr % 8));
			}
		}
	}

	for (b = 2; b < num_blocks; ++b)
	{
		if ((b - 1) % (8 * block_size + 1) == 0)
			continue;

		if (deleted[b / 8] & (1 << (b % 8)))
		{
			printf("<tr bgcolor=#%s><td><b><a id=\"%03x\" href=\"#r%03x\">%03x:</a></b></td><td colspan=5>Free</td></tr>\n", b&1 ? "FFFFFF" : "F0F0FF", b, b, b);
			continue;
		}

		pos = (OpFileLength)b * block_size;
		block_len = m_storage.DataLength(pos);
		if (block_len == 0 && m_head != NULL)
		{
			printf("<tr bgcolor=#%s><td><b><a id=\"%03x\" href=\"#r%03x\">%03x:</a></b></td></tr>\n", b&1 ? "FFFFFF" : "F0F0FF", b, b, b);
			continue;  // empty btree might be tolerated for reserved pages during a transaction
		}
		if (block_len > block_size - 12 || block_len < 4 + item_size || (block_len - 4) % item_size != 0)
		{
			++size_errors;
			printf("<tr bgcolor=#%s><td bgcolor=#FF40FF><b><a id=\"%03x\" href=\"#r%03x\">%03x:</a></b></td><td colspan=5>size = (%d-4)/%d</td></tr>\n", b&1 ? "FFFFFF" : "F0F0FF", b, b, b, block_len, item_size);
			continue;
		}

		num_items = (block_len - 4) / item_size;
		
		size_error = root != 0 && b != root && num_items + 1 < ((block_size - 12 - 4) / item_size) / 2;
		if (size_error)
			++size_errors;

		ref_error = root != 0 && b != root && (refs[b / 8] & (1 << (b % 8))) == 0;
		if (ref_error)
			++ref_errors;

		printf("<tr bgcolor=#%s><td%s><b><a id=\"%03x\" href=\"#r%03x\">%03x:</a></b></td>",
				b&1 ? "FFFFFF" : "F0F0FF",
				ref_error ? " bgcolor=#808080" :
				size_error ? " bgcolor=#FFC0FF" : "",
				b, b, b);

		if (!m_storage.Read(block, block_len, pos))
			return OpStatus::ERR_NO_ACCESS;

		rightmost = *(UINT32 *)(block);

		for (item = 0; item <= num_items; ++item)
		{
			ptr = item==num_items ? rightmost : *(UINT32 *)(block + 4 + item * item_size);
			ptr_error = (rightmost == 0) != (ptr == 0);
			ptr_bad_error = ptr >= num_blocks || (ptr - 1) % (8 * block_size + 1) == 0 || ptr == b;
			ptr_deleted_error = ptr && !ptr_bad_error && (deleted[ptr / 8] & (1 << (ptr % 8))) != 0;
			sort_error = item > 0 && item < num_items &&
				!m_allocator.Compare(block + 8 + (item - 1) * item_size, block + 8 + item * item_size);
			ref_error = ptr && !ptr_bad_error && (referr[ptr / 8] & (1 << (ptr % 8))) != 0;
			ptr_sort_error = FALSE;
			ptr_sort_recursive_error = FALSE;
			if (ptr && !ptr_bad_error && !ptr_deleted_error && !ref_error && !ptr_error)
			{
				pos = (OpFileLength)ptr * block_size;
				block_len = m_storage.DataLength(pos);
				if (block_len <= block_size - 12 && block_len >= 4 + item_size && (block_len - 4) % item_size == 0)
				{
					num_items2 = (block_len - 4) / item_size;
					if (!m_storage.Read(block2, block_len, pos))
						return OpStatus::ERR_NO_ACCESS;
					ptr_sort_error = (item > 0 && !m_allocator.Compare(block + 8 + (item - 1) * item_size, block2 + 8))
						||   (item < num_items && !m_allocator.Compare(block2 + 8 + (num_items2 - 1) * item_size, block + 8 + item * item_size));
				}
				if (item > 0 && !ptr_sort_error) {
					BOOL read_ok;
					ptr2 = ptr;
					LoopDetector<INT32> loopDetector;
					while (ptr2 && !ptr_sort_recursive_error)
					{
						UINT32 prev_ptr2 = ptr2;
						pos = (OpFileLength)ptr2 * block_size;
						block_len = m_storage.DataLength(pos);
						ptr2 = 0;
						read_ok = FALSE;
						if (block_len <= block_size - 12 && block_len >= 4 + item_size && (block_len - 4) % item_size == 0)
						{
							if (!m_storage.Read(block2, block_len, pos))
								return OpStatus::ERR_NO_ACCESS;
							read_ok = TRUE;
							ptr2 = *(UINT32 *)(block2 + 4);
							if (ptr2 &&
								(ptr2 == prev_ptr2 || ptr2 >= num_blocks || (ptr2 - 1) % (8 * block_size + 1) == 0 ||
								 (deleted[ptr2 / 8] & (1 << (ptr2 % 8))) != 0))
							{
								ptr2 = 0;
								read_ok = FALSE;
							}
						}
						ptr_sort_recursive_error = read_ok && !m_allocator.Compare(block + 8 + (item - 1) * item_size, block2 + 8);
						if (OpStatus::IsError(loopDetector.CheckNext(ptr2)))
							break;
					}
				}
				if (item < num_items && !ptr_sort_error && !ptr_sort_recursive_error)
				{
					BOOL read_ok;
					ptr2 = ptr;
					LoopDetector<INT32> loopDetector;
					while (ptr2 && !ptr_sort_recursive_error)
					{
						UINT32 prev_ptr2 = ptr2;
						pos = (OpFileLength)ptr2 * block_size;
						block_len = m_storage.DataLength(pos);
						ptr2 = 0;
						read_ok = FALSE;
						if (block_len <= block_size - 12 && block_len >= 4 + item_size && (block_len - 4) % item_size == 0)
						{
							num_items2 = (block_len - 4) / item_size;
							if (!m_storage.Read(block2, block_len, pos))
								return OpStatus::ERR_NO_ACCESS;
							read_ok = TRUE;
							ptr2 = *(UINT32 *)(block2);
							if (ptr2 &&
								(ptr2 == prev_ptr2 || ptr2 >= num_blocks || (ptr2 - 1) % (8 * block_size + 1) == 0 ||
								 (deleted[ptr2 / 8] & (1 << (ptr2 % 8))) != 0))
							{
								ptr2 = 0;
								read_ok = FALSE;
							}
						}
						ptr_sort_recursive_error = read_ok && !m_allocator.Compare(block2 + 8 + (num_items2 - 1) * item_size, block + 8 + item * item_size);
						if (OpStatus::IsError(loopDetector.CheckNext(ptr2)))
							break;
					}
				}
			}			
			if (ptr)
				printf("<td%s><b><a id=\"r%03x\" href=\"#%03x\">%03x</a></b></td>",
					   ptr_bad_error            ? " bgcolor=#FF40FF" :
					   ptr_deleted_error        ? " bgcolor=#808080" :
					   ref_error                ? " bgcolor=#FF8080" :
					   ptr_error                ? " bgcolor=#FFFF80" :
					   ptr_sort_error           ? " bgcolor=#FFC080" :
					   ptr_sort_recursive_error ? " bgcolor=#FFC0FF" : "", ptr, ptr, ptr);
			else
				printf("<td%s><b>---</b></td>",
					   ptr_error                ? " bgcolor=#FFFF80" : "");
			if (item < num_items) {
				UINT32 data = *(UINT32 *)(block + 8 + item * item_size) >> 16;
				printf("<td%s>%04x</td>",
					   sort_error               ? " bgcolor=#FFC080" : "", data);
			}
			if (ptr_error || ptr_bad_error || ptr_deleted_error)
				++ptr_errors;
			if (sort_error)
				++sort_errors;
			if (ptr_sort_error || ptr_sort_recursive_error)
				++sort_errors;
			if (ref_error)
				++ref_errors;
		}

		printf("</tr>\n");
	}
	printf("</table>\n");

	return (ptr_errors + sort_errors + size_errors + ref_errors == 0) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}
#endif // defined(LINGOGI) && (defined(_DEBUG) || defined(SELFTEST) || defined(SEARCH_ENGINE_LOG))

BSCache::Item *PoolBase::NewMemoryItem(int id, Item *rbranch, int rnode, unsigned short nur)
{
	BTreeBase::BTreeBranch* branch = OP_NEW(BTreeBase::BTreeBranch, (this, -id, (BTreeBase::BTreeBranch *)rbranch, nur));
	if (branch != NULL && OpStatus::IsError(branch->Construct(&m_storage)))
	{
		OP_DELETE(branch);
		branch = NULL;
	}
	return branch;
}

BSCache::Item *PoolBase::NewDiskItem(OpFileLength id, unsigned short nur)
{
	BTreeBase::BTreeBranch* branch = OP_NEW(BTreeBase::BTreeBranch, (this, id, NULL, nur));
	if (branch != NULL && OpStatus::IsError(branch->Construct(&m_storage)))
	{
		OP_DELETE(branch);
		branch = NULL;
	}
	return branch;
}

#undef CHECK_INTEGRITY

#endif  // SEARCH_ENGINE

