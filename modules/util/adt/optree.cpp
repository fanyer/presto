/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/adt/optree.h"

/***********************************************************************************
**
**	OpGenericTree
**
***********************************************************************************/

OpGenericTree::OpGenericTree()
{
	m_tree_nodes.SetAllocationStepSize(1000);
	m_total_sub_tree_sizes = 0;
}

OpGenericTree::~OpGenericTree()
{
	Clear();
}

/***********************************************************************************
**
**	Clear
**
***********************************************************************************/

void OpGenericTree::Clear()
{
	m_tree_nodes.DeleteAll();
}

/***********************************************************************************
**
**	Sort
**
***********************************************************************************/

OP_STATUS OpGenericTree::Sort(compare_function_t compare_function)
{
	// Method used for sorting, keeping nlogn normal case for quicksort
	// 1. create and array for quicksorting
	// 2. create an array for holding resulting tree order
	// 3. sort children of parent x
	// 4. loop: add nth starting at 0 item after sort into result array
	//    and goto 3 for this child
	// 5. rebuild tree from result array

	OP_STATUS status = OpStatus::OK;
	
	INT32 count = GetCount();
	
	if (count == 0)
		{
			return status;
		}

	INT32*		quicksort_array		= OP_NEWA(INT32, count);
	if (!quicksort_array)
	{
			return OpStatus::ERR_NO_MEMORY;
	}
	INT32*		parent_array		= OP_NEWA(INT32, count);
	if (!parent_array)
	{
		OP_DELETEA(quicksort_array);
		return OpStatus::ERR_NO_MEMORY;
	}
	TreeNode**	result_array		= OP_NEWA(TreeNode*, count);
	if (!result_array)
	{
		OP_DELETEA(quicksort_array);
		OP_DELETEA(parent_array);
		return OpStatus::ERR_NO_MEMORY;
	}
	INT32		current_quicksort_array_pos	= 0;
	INT32		current_result_array_pos	= 0;
	
	SortChildren(compare_function, -1, -1, quicksort_array, parent_array, result_array, current_quicksort_array_pos, current_result_array_pos);

	// rebuild tree

	for (INT32 i = 0; i < count; i++)
	{
		result_array[i]->m_parent_index = parent_array[i];
		m_tree_nodes.Replace(i, result_array[i]);
	}

	// free buffers

	OP_DELETEA(result_array);
	OP_DELETEA(parent_array);
	OP_DELETEA(quicksort_array);

	return status;
}

/***********************************************************************************
**
**	SortChildren
**
***********************************************************************************/

void OpGenericTree::SortChildren(compare_function_t compare_function, INT32 old_parent_index, INT32 new_parent_index, INT32* quicksort_array, INT32* parent_array, TreeNode** result_array, INT32& current_quicksort_array_pos, INT32& current_result_array_pos)
{
	INT32 child = GetChild(old_parent_index);

	if (child == -1)
	{
		return;
	}

	// fill quick sort array

	INT32 quicksort_array_pos = current_quicksort_array_pos;
	INT32 count = 0;

	while (child != -1)
	{
		quicksort_array[current_quicksort_array_pos] = child;
		current_quicksort_array_pos++;
		count++;
		child = GetSibling(child);
	}

	// quicksort the array

	op_qsort(quicksort_array + quicksort_array_pos, count, sizeof(INT32), compare_function);

	// append result to result array while recursivly sorting grandchildren

	for (INT32 i = 0; i < count; i++)
	{
		INT32 result_index = quicksort_array[quicksort_array_pos + i];

		TreeNode* result_node = m_tree_nodes.Get(result_index);

		result_array[current_result_array_pos] = result_node;
		parent_array[current_result_array_pos] = new_parent_index;

		current_result_array_pos++;
		SortChildren(compare_function, result_index, current_result_array_pos - 1, quicksort_array, parent_array, result_array, current_quicksort_array_pos, current_result_array_pos);
	}
}

/***********************************************************************************
**
**	UpdateNodes
**
***********************************************************************************/

void OpGenericTree::UpdateNodes(INT32 index_changed_from, INT32 index, INT32 parent_index, INT32 count)
{
	while (parent_index != -1)
	{
		TreeNode* parent = m_tree_nodes.Get(parent_index);
		parent->m_sub_tree_size += count;
		parent_index = parent->m_parent_index;
	}

	if (m_total_sub_tree_sizes > 0)
	{
		INT32 total_count = GetCount();

		while (index < total_count)
		{
			TreeNode* node = m_tree_nodes.Get(index);

			if (node->m_parent_index >= index_changed_from)
			{
				node->m_parent_index += count;
			}

			index++;
		}
	}
}

/***********************************************************************************
**
**	Insert
**
***********************************************************************************/

OP_STATUS OpGenericTree::Insert(INT32 parent_index, INT32 index, void* item)
{
	// allocate new node

	TreeNode* node = OP_NEW(TreeNode, ());

	if (node == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	node->m_item = item;
	node->m_parent_index = parent_index;
	node->m_sub_tree_size = 0;

	// insert new node at index

	OP_STATUS status = m_tree_nodes.Insert(index, node);

	if (status != OpStatus::OK)
	{
		OP_DELETE(node);
		return status;
	}

	// update total sub tree sizes

	if (parent_index != -1)
	{
		m_total_sub_tree_sizes++;
	}

	// update parents and nodes below

	UpdateNodes(index, index + 1, node->m_parent_index, 1);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	AddFirst
**
***********************************************************************************/

OP_STATUS OpGenericTree::AddFirst(INT32 parent_index, void* item, INT32* got_index)
{
	OP_ASSERT(parent_index < GetCount());

	if (got_index)
	{
		*got_index = parent_index + 1;
	}

	return Insert(parent_index, parent_index + 1, item);
}

/***********************************************************************************
**
**	AddLast
**
***********************************************************************************/

OP_STATUS OpGenericTree::AddLast(INT32 parent_index, void* item, INT32* got_index)
{
	OP_ASSERT(parent_index < GetCount());

	INT32 index;

	if (parent_index == -1)
	{
		index = GetCount();
	}
	else
	{
		TreeNode* parent = m_tree_nodes.Get(parent_index);

		index = parent_index + parent->m_sub_tree_size + 1;
	}

	if (got_index)
	{
		*got_index = index;
	}

	return Insert(parent_index, index, item);
}

/***********************************************************************************
**
**	InsertBefore
**
***********************************************************************************/

OP_STATUS OpGenericTree::InsertBefore(INT32 sibling_index, void* item, INT32* got_index)
{
	OP_ASSERT(sibling_index < GetCount());

	TreeNode* sibling = m_tree_nodes.Get(sibling_index);

	if (got_index)
	{
		*got_index = sibling_index;
	}

	return Insert(sibling->m_parent_index, sibling_index, item);
}

/***********************************************************************************
**
**	InsertAfter
**
***********************************************************************************/

OP_STATUS OpGenericTree::InsertAfter(INT32 sibling_index, void* item, INT32* got_index)
{
	OP_ASSERT(sibling_index < GetCount());

	TreeNode* sibling = m_tree_nodes.Get(sibling_index);

	if (got_index)
	{
		*got_index = sibling_index + sibling->m_sub_tree_size + 1;
	}

	return Insert(sibling->m_parent_index, sibling_index + sibling->m_sub_tree_size + 1, item);
}

/***********************************************************************************
**
**	Remove
**
***********************************************************************************/

void* OpGenericTree::Remove(INT32 index, BOOL all_children)
{
	OP_ASSERT(index < GetCount());

	TreeNode* node = m_tree_nodes.Get(index);

	void* item = node->m_item;

	INT32 count = 1;

	if (all_children)
	{
		m_total_sub_tree_sizes -= node->m_sub_tree_size;

		count += node->m_sub_tree_size;
	}
	else
	{
		// reparent children

		INT32 child = GetChild(index);

		while (child != -1)
		{
			INT32 sibling = GetSibling(child);

			m_tree_nodes.Get(child)->m_parent_index = node->m_parent_index;

			child = sibling;
		}
	}

	// update total sub tree sizes

	if (node->m_parent_index != -1)
	{
		m_total_sub_tree_sizes--;
	}

	// update parents and nodes below

	UpdateNodes(index, index + count, node->m_parent_index, -count);

	// iterate through from index and all of subtree and delete
	// all TreeNodes without removing them, and then remove
	// all at once int the end.

	for (INT32 i = 0; i < count; i++)
	{
		OP_DELETE(m_tree_nodes.Get(index + i));
	}

	m_tree_nodes.Remove(index, count);

	return item;
}

/***********************************************************************************
**
**	Find
**
***********************************************************************************/

INT32 OpGenericTree::Find(void* item) const
{
	if (!item)
		return -1;

	UINT32 count = GetCount();

	for (UINT32 index = 0; index < count; index++)
	{
		if (Get(index) == item)
		{
			return index;
		}
	}

	return -1;
}

/***********************************************************************************
**
**	Get
**
***********************************************************************************/

void* OpGenericTree::Get(INT32 index) const
{
	OP_ASSERT(index < GetCount());

	TreeNode* node = m_tree_nodes.Get(index);

	return node->m_item;
}

/***********************************************************************************
**
**	GetParent
**
***********************************************************************************/

INT32 OpGenericTree::GetParent(INT32 index) const
{
	OP_ASSERT(index < GetCount());

	TreeNode* node = m_tree_nodes.Get(index);

	return node->m_parent_index;
}

/***********************************************************************************
**
**	GetChild
**
***********************************************************************************/

INT32 OpGenericTree::GetChild(INT32 index) const
{
	OP_ASSERT(index < GetCount());

	if (index == -1)
	{
		if (GetCount() == 0)
		{
			return -1;
		}
	}
	else
	{
		TreeNode* node = m_tree_nodes.Get(index);

		if (node->m_sub_tree_size == 0)
		{
			return -1;
		}
	}

	return index + 1;
}

/***********************************************************************************
**
**	GetSibling
**
***********************************************************************************/

INT32 OpGenericTree::GetSibling(INT32 index) const
{
	OP_ASSERT(index < GetCount());

	TreeNode* node = m_tree_nodes.Get(index);

	index += node->m_sub_tree_size + 1;

	if (index == GetCount())
	{
		return -1;
	}

	TreeNode* sibling = m_tree_nodes.Get(index);

	if (sibling->m_parent_index != node->m_parent_index)
	{
		return -1;
	}

	return index;
}

/***********************************************************************************
**
**	GetSubtreeSize
**
***********************************************************************************/

INT32 OpGenericTree::GetSubtreeSize(INT32 index) const
{
	OP_ASSERT(index < GetCount());

	if (index == -1)
	{
		return m_tree_nodes.GetCount();
	}

	return m_tree_nodes.Get(index)->m_sub_tree_size;
}
