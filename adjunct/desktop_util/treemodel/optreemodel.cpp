/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "optreemodel.h"

#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/base/op_types.h"
#include "modules/util/excepts.h"
#include "modules/util/opstring.h"
#include "modules/pi/OpSystemInfo.h"

/***********************************************************************************
**
**	OpTreeModel::RemoveListener
**
***********************************************************************************/

OP_STATUS OpTreeModel::RemoveListener(Listener* listener)
{
	INT32 pos = m_listeners.Find(listener);

	if (pos != -1)
	{
		m_listeners.Remove(pos);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastItemAdded
**
***********************************************************************************/

void OpTreeModel::BroadcastItemAdded(INT32 index)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnItemAdded(this, index);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastItemChanged
**
***********************************************************************************/

void OpTreeModel::BroadcastItemChanged(INT32 index, BOOL sort)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnItemChanged(this, index, sort);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastSubtreeRemoving
**
***********************************************************************************/

void OpTreeModel::BroadcastSubtreeRemoving(INT32 parent, INT32 index, INT32 subtreesize)
{
	for (INT32 i = 0; i <= subtreesize; i++)
	{
		for (UINT32 j = 0; j < m_listeners.GetCount(); j++)
		{
			m_listeners.Get(j)->OnItemRemoving(this, index + i);
		}
	}

	for (UINT32 j = 0; j < m_listeners.GetCount(); j++)
	{
		m_listeners.Get(j)->OnSubtreeRemoving(this, parent, index, subtreesize);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastSubtreeRemoved
**
***********************************************************************************/

void OpTreeModel::BroadcastSubtreeRemoved(INT32 parent, INT32 index, INT32 subtreesize)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnSubtreeRemoved(this, parent, index, subtreesize);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastTreeChanging
**
***********************************************************************************/

void OpTreeModel::BroadcastTreeChanging()
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnTreeChanging(this);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastTreeChanged
**
***********************************************************************************/

void OpTreeModel::BroadcastTreeChanged()
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnTreeChanged(this);
	}
}

/***********************************************************************************
**
**	OpTreeModel::BroadcastTreeDeleted
**
***********************************************************************************/

void OpTreeModel::BroadcastTreeDeleted()
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnTreeDeleted(this);
	}
}

/***********************************************************************************
**
**	OpTreeModel::GetItemByID
**
***********************************************************************************/

void OpTreeModel::GetItemByID(INT32 id, OpTreeModelItem*& item)
{
	for (INT32 i = 0; i < GetItemCount(); i++)
	{
		item = GetItemByPosition(i);

		if (item->GetID() == id)
		{
			return;
		}
	}

	item = NULL;
}

/***********************************************************************************
**
**	OpTreeModel::GetItemByIDAndType
**
***********************************************************************************/

void OpTreeModel::GetItemByIDAndType(INT32 id, OpTypedObject::Type type, OpTreeModelItem*& item)
{
	for (INT32 i = 0; i < GetItemCount(); i++)
	{
		item = GetItemByPosition(i);

		if (item->GetID() == id && item->GetType() == type)
		{
			return;
		}
	}

	item = NULL;
}

/***********************************************************************************
**
**	GenericTreeModel::~GenericTreeModel
**
***********************************************************************************/

GenericTreeModel::~GenericTreeModel()
{
	BroadcastTreeDeleted();
	m_items.DeleteAll();
}

/***********************************************************************************
**
**	GenericTreeModel::GetIndexByItem
**
***********************************************************************************/

INT32 GenericTreeModel::GetIndexByItem(GenericTreeModelItem* item)
{
	if (!item || !item->IsInModel(this))
		return -1;

	INT32 count = GetCount();
	INT32 last_pos_downwards = item->GetLastIndex();

	if (last_pos_downwards >= count)
		last_pos_downwards = count - 1;

	INT32 last_pos_upwards = last_pos_downwards + 1;

	while (last_pos_downwards >= 0 || last_pos_upwards < count)
	{
		if (last_pos_downwards >= 0)
		{
			GenericTreeModelItem* item_at_downwards_pos = GetGenericItemByIndex(last_pos_downwards);

			if (item_at_downwards_pos == item)
				return last_pos_downwards;

			last_pos_downwards--;
		}

		if (last_pos_upwards < count)
		{
			GenericTreeModelItem* item_at_upwards_pos = GetGenericItemByIndex(last_pos_upwards);

			if (item_at_upwards_pos == item)
				return last_pos_upwards;

			last_pos_upwards++;
		}
	}

	return -1;
}

/***********************************************************************************
**
**	GenericTreeModel::GetSiblingIndex
**
***********************************************************************************/

INT32 GenericTreeModel::GetSiblingIndex(INT32 index)
{
	GenericTreeModelItem* item = GetGenericItemByIndex(index);

	if (!item)
		return -1;

	index += item->GetSubtreeSize() + 1;

	GenericTreeModelItem* sibling = GetGenericItemByIndex(index);

	return sibling && sibling->GetGenericParentItem() == item->GetGenericParentItem() ? index : -1;
}

/***********************************************************************************
**
**	GenericTreeModel::GetPreviousIndex
**
***********************************************************************************/

INT32 GenericTreeModel::GetPreviousIndex(INT32 index)
{
	if (index <= 0) return -1;

	GenericTreeModelItem* item = GetGenericItemByIndex(index);

	if (!item)      return -1;

	INT32 prev_index           = index - 1;
	GenericTreeModelItem* prev = GetGenericItemByIndex(prev_index);

	while (prev)
	{
		OP_ASSERT(index >= 0);

		// If item is child of prev item, stop search
		if (prev_index + prev->GetSubtreeSize() >= index)
		{
			return -1;
		}

		if (prev->GetSiblingIndex() == index)
		{
			 break;
		}

		prev_index -= 1;

		if (prev_index < 0) return -1;

		prev = GetGenericItemByIndex(prev_index);
	}

	GenericTreeModelItem* previous = GetGenericItemByIndex(prev_index);

	return previous && previous->GetGenericParentItem() == item->GetGenericParentItem() ? prev_index : -1;
}

/***********************************************************************************
**
**	GenericTreeModel::GetLastChildIndex
**
***********************************************************************************/

INT32 GenericTreeModel::GetLastChildIndex(INT32 index)
{
	INT32 child = GetChildIndex(index);

	for (INT32 next_child = GetSiblingIndex(child); next_child != -1; next_child = GetSiblingIndex(child))
	{
		child = next_child;
	}

	return child;
}

/***********************************************************************************
**
**	GenericTreeModel::GetPreviousLeafIndex
**
***********************************************************************************/

INT32 GenericTreeModel::GetPreviousLeafIndex(INT32 index)
{
	if (GetSubtreeSize(index) == 0)
	{
		// this is a leaf itself. Go up
		while (GetPreviousIndex(index) == -1 && index != -1)
			index = GetParentIndex(index);
		if (index == -1)
			return -1;
		
		index = GetPreviousIndex(index);
	}

	// Go down to the leaf
	return index + GetSubtreeSize(index);
}

/***********************************************************************************
**
**	GenericTreeModel::GetNextLeafIndex
**
***********************************************************************************/

INT32 GenericTreeModel::GetNextLeafIndex(INT32 index)
{
	if (GetSubtreeSize(index) == 0)
	{
		// this is a leaf itself. Go up
		while (GetSiblingIndex(index) == -1 && index != -1)
			index = GetParentIndex(index);
		if (index == -1)
			return -1;
		
		index = GetSiblingIndex(index);
	}

	// Go down to the leaf
	while (GetChildIndex(index) != -1)
		index = GetChildIndex(index);

	return index;
}

/***********************************************************************************
**
**	GenericTreeModel::GetChildCount
**
***********************************************************************************/

INT32 GenericTreeModel::GetChildCount(INT32 index)
{
	INT32 count = 0;
	for (INT32 child = GetChildIndex(index); child != -1; child = GetSiblingIndex(child))
	{
		count++;
	}
	return count;
}

/***********************************************************************************
**
**	GenericTreeModel::GetSubtreeSize
**
***********************************************************************************/

INT32 GenericTreeModel::GetSubtreeSize(INT32 index)
{
	if (index == -1)
		return GetCount();

	GenericTreeModelItem* item = GetGenericItemByIndex(index);

	return item ? item->GetSubtreeSize() : 0;
}

/***********************************************************************************
**
**	GenericTreeModel::UpdateSubtreeSizes
**
***********************************************************************************/

void GenericTreeModel::UpdateSubtreeSizes(GenericTreeModelItem* parent, INT32 count)
{
	for (; parent; parent = parent->GetGenericParentItem())
	{
		parent->AddSubtreeSize(count);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::Insert
**
***********************************************************************************/

INT32 GenericTreeModel::Insert(GenericTreeModelItem* item, GenericTreeModelItem* parent, INT32 index)
{
	OP_ASSERT(!item->IsInModel());
	OP_ASSERT(index <= GetCount());

	if (m_items.Insert(index, item) != OpStatus::OK)
		return -1;

	item->SetGenericModel(this);
	item->SetGenericParentItem(parent);
	item->SetLastIndex(index);
	item->SetSubtreeSize(0);

	UpdateSubtreeSizes(parent, 1);
	m_id_to_item.Add(item->GetID(), item);
	if (parent)
		m_is_flat = FALSE;
	item->OnAdded();

	if (SetChangeType(TREE_CHANGED))
	{
		BroadcastItemAdded(index);
	}

	return index;
}

/***********************************************************************************
**
**	GenericTreeModel::ResortItem
**
***********************************************************************************/

INT32 GenericTreeModel::ResortItem(INT32 index)
{
	OP_ASSERT(m_sort_listener);

	if (!m_sort_listener)
		return -1;

	INT32 sibling = -1;

	// Binary search until correct sibling is found

	INT32 tree_size = GetSubtreeSize(index) + 1;
	INT32 parent = GetParentIndex(index);
	INT32 parent_tree_size = GetSubtreeSize(parent);
	INT32 lowest = 0;
	INT32 highest = parent_tree_size - tree_size;
	GenericTreeModelItem* item = GetGenericItemByIndex(index);

	while (lowest < highest)
	{
		INT32 current_position = (lowest + highest) / 2;
		INT32 pos_to_compare_to = parent + current_position + 1;

		if (pos_to_compare_to >= index)
			pos_to_compare_to += tree_size;

		// convert any grandchildren up to same level we're inserting at
		INT32 parent_of_pos_to_compare_to	= GetParentIndex(pos_to_compare_to);

		while (parent_of_pos_to_compare_to != parent)
		{
			pos_to_compare_to = parent_of_pos_to_compare_to;
			parent_of_pos_to_compare_to = GetParentIndex(pos_to_compare_to);
		}

		INT32 comparison = m_sort_listener->OnCompareItems(this, item, GetGenericItemByIndex(pos_to_compare_to));

		if (comparison > 0)
		{
			lowest = current_position + 1;
		}
		else
		{
			sibling = pos_to_compare_to;
			highest = current_position;
		}
	}

	INT32 newpos = (sibling != -1 ? sibling : parent + 1 + parent_tree_size);
	if (newpos == index)
		return newpos;

	ModelLock lock(this);

	SetChangeType(TREE_CHANGED);

	GenericTreeModelItem** items = OP_NEWA(GenericTreeModelItem*, tree_size);
	if (!items)
		return -1;

	for (INT32 i = 0; i < tree_size; i++)
		items[i] = m_items.Get(index + i);

	m_items.Remove(index, tree_size);

	if (newpos > index)
		newpos -= tree_size;

	for (INT32 i = tree_size - 1; i >= 0; i--)
		m_items.Insert(newpos, items[i]);

	OP_DELETEA(items);

	return newpos;
}

/***********************************************************************************
**
**	GenericTreeModel::Sort
**
***********************************************************************************/

OP_STATUS GenericTreeModel::Sort()
{
	OP_ASSERT(m_sort_listener);

	if (!m_sort_listener)
		return OpStatus::ERR;

	INT32 count = GetCount();

	if (count == 0)
	{
		return OpStatus::OK;
	}

	GenericTreeModelItem**	result_array		= OP_NEWA(GenericTreeModelItem*, count);
	INT32*					sort_array		= OP_NEWA(INT32, count * 2);

	if (!sort_array || !result_array)
	{
		OP_DELETEA(result_array);
		OP_DELETEA(sort_array);
		return OpStatus::ERR_NO_MEMORY;
	}

	BOOL broadcast = SetChangeType(TREE_CHANGED);

	if (broadcast)
	{
		BroadcastTreeChanging();
	}

	INT32		current_sort_array_pos	= 0;
	INT32		current_result_array_pos	= 0;

	SortChildren(-1, sort_array, sort_array + count, result_array, current_sort_array_pos, current_result_array_pos);

	// rebuild tree

	for (INT32 i = 0; i < count; i++)
	{
		GenericTreeModelItem* item = result_array[i];
		item->SetLastIndex(i);
		m_items.Replace(i, item);
	}

	// free buffers

	OP_DELETEA(result_array);
	OP_DELETEA(sort_array);

	if (broadcast)
	{
		BroadcastTreeChanged();
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SortChildren
**
***********************************************************************************/

void GenericTreeModel::SortChildren(INT32 parent_index, INT32* sort_array, INT32* temp_array, GenericTreeModelItem** result_array, INT32& current_sort_array_pos, INT32& current_result_array_pos)
{
	INT32 child = GetChildIndex(parent_index);

	if (child == -1)
	{
		return;
	}

	// fill sort array

	INT32 sort_array_pos = current_sort_array_pos;
	INT32 count = 0;

	while (child != -1)
	{
		sort_array[current_sort_array_pos] = child;
		current_sort_array_pos++;
		count++;
		child = GetSiblingIndex(child);
	}

	// sort the array
	m_comparison_counter = 0;

	//QuickSort(sort_array + sort_array_pos, count);
	//HeapSort(sort_array + sort_array_pos, count);
	MergeSort(sort_array + sort_array_pos, temp_array, 0, count - 1);

	// append result to result array while recursivly sorting grandchildren

	for (INT32 i = 0; i < count; i++)
	{
		INT32 result_index = sort_array[sort_array_pos + i];
		GenericTreeModelItem* result_item = GetGenericItemByIndex(result_index);
		result_array[current_result_array_pos] = result_item;
		current_result_array_pos++;
		SortChildren(result_index, sort_array, temp_array, result_array, current_sort_array_pos, current_result_array_pos);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::MergeSort
**
***********************************************************************************/

void GenericTreeModel::MergeSort(INT32* array, INT32* temp_array, INT32 left, INT32 right)
{
	if (left < right)
	{
		INT32 center = (left + right) / 2;
		MergeSort(array, temp_array, left, center);
		MergeSort(array, temp_array, center + 1, right);
		Merge(array, temp_array, left, center + 1, right);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::Merge
**
***********************************************************************************/

void GenericTreeModel::Merge(INT32* array, INT32* temp_array, INT32 left, INT32 right, INT32 right_end)
{
	INT32 left_end = right - 1;
	INT32 temp_pos = left;
	INT32 count = right_end - left + 1;

	while (left <= left_end && right <= right_end)
	{
		temp_array[temp_pos++] = Compare(array[left], array[right]) <= 0 ? array[left++] : array[right++];
	}

	while (left <= left_end)
	{
		temp_array[temp_pos++] = array[left++];
	}

	while (right <= right_end)
	{
		temp_array[temp_pos++] = array[right++];
	}

	for (INT32 i = 0; i < count; i++, right_end--)
	{
		array[right_end] = temp_array[right_end];
	}
}

/***********************************************************************************
**
**	GenericTreeModel::QuickSort
**
***********************************************************************************/

void GenericTreeModel::QuickSort(INT32* array, INT32 count)
{
	s_sort_model = this;
	op_qsort(array, count, sizeof(INT32), StaticCompareFunction);
}

/***********************************************************************************
**
**	GenericTreeModel::HeapSort
**
***********************************************************************************/

void GenericTreeModel::HeapSort(INT32* array, INT32 count)
{
	INT32 index;

	for (index = count / 2; index >= 0; index--)
	{
		HeapTraverse(array, index, count);
	}
	for (index = count - 1; index > 0; index--)
	{
		INT32 tmp = *array;
		*array = array[index];
		array[index] = tmp;
		HeapTraverse(array, 0, index);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::HeapTraverse
**
***********************************************************************************/

void GenericTreeModel::HeapTraverse(INT32* array, INT32 index, INT32 count)
{
	INT32 child;
	INT32 tmp;

	for (tmp = array[index]; (child = 2 * index + 1) < count; index = child)
	{
		if (child != count - 1 && Compare(array[child], array[child + 1]) < 0)
		{
			child++;
		}
		if (Compare(tmp, array[child]) < 0)
		{
			array[index] = array[child];
		}
		else
		{
			break;
		}
	}

	array[index] = tmp;
}

/***********************************************************************************
**
**	GenericTreeModel::Compare
**
***********************************************************************************/

INT32 GenericTreeModel::Compare(INT32 index1, INT32 index2)
{
	m_comparison_counter++;
	return m_sort_listener->OnCompareItems(this, GetGenericItemByIndex(index1), GetGenericItemByIndex(index2));
}

/***********************************************************************************
**
**	StaticCompareFunction
**
***********************************************************************************/

GenericTreeModel* GenericTreeModel::s_sort_model = NULL;

int GenericTreeModel::StaticCompareFunction(const void* p0, const void* p1)
{
	return s_sort_model->Compare(*((INT32*)p0), *((INT32*)p1));
}

/***********************************************************************************
**
**	GenericTreeModel::Change
**
***********************************************************************************/

void GenericTreeModel::Change(INT32 index, BOOL sorting_might_have_changed)
{
	if (IsItem(index) && SetChangeType(ITEMS_CHANGED))
	{
		BroadcastItemChanged(index, sorting_might_have_changed);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::ChangeAll
**
***********************************************************************************/

void GenericTreeModel::ChangeAll()
{
	if (SetChangeType(ITEMS_CHANGED))
	{
		BroadcastItemChanged(-1);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::Remove
**
***********************************************************************************/

void GenericTreeModel::Remove(INT32 index, BOOL all_children, BOOL delete_item)
{
	INT32 subtreesize = all_children ? GetSubtreeSize(index) : 0;

	BOOL broadcast = SetChangeType(TREE_CHANGED);

	GenericTreeModelItem* parent = GetGenericParentItem(index);

	if (broadcast)
	{
		BroadcastSubtreeRemoving(parent ? parent->GetIndex() : -1, index, subtreesize);
	}

	if (!all_children)
	{
		INT32 child = GetChildIndex(index);

		while (child != -1)
		{
			INT32 sibling = GetSiblingIndex(child);

			GetGenericItemByIndex(child)->SetGenericParentItem(parent);

			child = sibling;
		}
	}

	INT32 count = subtreesize + 1;

	Removing(index, count);
	UpdateSubtreeSizes(parent, -count);
	OpTreeModelItem* data;
	for(UINT32 i = index, count_item = index + count; i < count_item; i++)
		m_id_to_item.Remove(m_items.Get(i)->GetID(), &data);

	if (delete_item)
	{
		m_items.Delete(index, count);
	}
	else
	{
		m_items.Remove(index, count);
	}

	if (broadcast)
	{
		BroadcastSubtreeRemoved(parent ? parent->GetIndex() : -1, index, subtreesize);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::Removing
**
***********************************************************************************/

void GenericTreeModel::Removing(INT32 index, INT32 count)
{
	for (INT32 i = 0; i < count; i++)
	{
		GenericTreeModelItem* item = GetGenericItemByIndex(index + i);
		item->OnRemoving();
		item->SetGenericModel(NULL);
		item->SetGenericParentItem(NULL);
		item->SetLastIndex(0);
		item->SetSubtreeSize(0);
		item->OnRemoved();
	}
}

/***********************************************************************************
**
**	GenericTreeModel::RemoveAll
**
***********************************************************************************/

void GenericTreeModel::RemoveAll()
{
	if (!GetCount())
		return;

	BOOL broadcast = SetChangeType(TREE_CHANGED);

	if (broadcast)
	{
		BroadcastTreeChanging();
	}

	Removing(0, GetCount());
	m_items.Clear();
	m_id_to_item.RemoveAll();

	if (broadcast)
	{
		BroadcastTreeChanged();
	}
}

/***********************************************************************************
**
**	GenericTreeModel::Remove
**
***********************************************************************************/
GenericTreeModelItem* GenericTreeModel::Remove(INT32 index, BOOL all_children)
{
	GenericTreeModelItem* removed_item = 0;
	if (IsItem(index))
	{
		removed_item = GetGenericItemByIndex(index);
		Remove(index, all_children, FALSE);
	}
	return removed_item;
}

/***********************************************************************************
**
**	GenericTreeModel::DeleteAll
**
***********************************************************************************/

void GenericTreeModel::DeleteAll()
{
	if (!GetCount())
		return;

	BOOL broadcast = SetChangeType(TREE_CHANGED);

	if (broadcast)
	{
		BroadcastTreeChanging();
	}

	Removing(0, GetCount());
	m_items.DeleteAll();
	m_id_to_item.RemoveAll();

	if (broadcast)
	{
		BroadcastTreeChanged();
	}
}

void GenericTreeModel::Move(INT32 index, INT32 parent_index, INT32 previous_index)
{
	if (index == -1 || index == previous_index)
	{
		return;
	}

	INT32 subtree_size = GetSubtreeSize(index);
	INT32 count = subtree_size + 1;
	
	GenericTreeModelItem* item			= GetGenericItemByIndex(index);

	GenericTreeModelItem* parent		= item->GetGenericParentItem();
	GenericTreeModelItem* new_parent	= GetGenericItemByIndex(parent_index);
	GenericTreeModelItem* new_previous	= GetGenericItemByIndex(previous_index);

	OP_ASSERT(!new_parent || !new_previous || new_parent == new_previous->GetGenericParentItem());

	if(new_previous && !new_parent)
		new_parent = new_previous->GetGenericParentItem();

	BOOL broadcast = SetChangeType(TREE_CHANGED);

	// 1. Save item and subtree
	OpAutoArray<GenericTreeModelItem*> to_be_moved (OP_NEWA(GenericTreeModelItem*, count));
	if (!to_be_moved.get())
		return;

	for(INT32 j = 0; j < count; j++)
	{
		to_be_moved[j] = m_items.Get(index + j);
	}

	// 2. Remove the subtree
	if (broadcast)
		BroadcastSubtreeRemoving(parent ? parent->GetIndex() : -1, index, subtree_size);

	if(parent)
		UpdateSubtreeSizes(parent, -count);
	m_items.Remove(index,count);
	
	if (broadcast)
		BroadcastSubtreeRemoved(parent ? parent->GetIndex() : -1, index, subtree_size);

	// 3. Insert it in proper position,update parent of item if needed
	
	// NOTICE: index of new_previous and new_parent may have changed! dont use previous_index and parent_index here
	INT32 insert_pos = new_previous ? 
		new_previous->GetIndex()  + new_previous->GetSubtreeSize() + 1
		: new_parent ? new_parent->GetIndex() + 1 : 0;
	for(INT32 i=0; i<count; i++)
	{
		m_items.Insert(insert_pos + i, to_be_moved[i]);
	}
	item->SetGenericParentItem(new_parent);
	if(new_parent)
		UpdateSubtreeSizes(new_parent, count);

	if(broadcast)
	{
		for(INT32 i=0; i<count; i++)
			BroadcastItemAdded(insert_pos + i);
	}
}

/***********************************************************************************
**
**	GenericTreeModel::SetChangeType
**
***********************************************************************************/

BOOL GenericTreeModel::SetChangeType(ChangeType type)
{
	if (m_change_counter == 0)
		return TRUE;

	if (type > m_change_type)
	{
		m_change_type = type;

		if (m_change_type == TREE_CHANGED)
		{
			BroadcastTreeChanging();
		}
	}

	return FALSE;
}

/***********************************************************************************
**
**	GenericTreeModel::EndChange
**
***********************************************************************************/

void GenericTreeModel::EndChange()
{
	OP_ASSERT(m_change_counter > 0);

	if (m_change_counter > 0)
	{
		m_change_counter--;

		if (m_change_counter == 0)
		{
			if (m_change_type == ITEMS_CHANGED)
			{
				BroadcastItemChanged(-1);
			}
			else if (m_change_type == TREE_CHANGED)
			{
				BroadcastTreeChanged();
			}

			m_change_type = NO_CHANGES;
		}
	}
}

/***********************************************************************************
**
**	GenericTreeModel::GetGenericItemByIndex
**
***********************************************************************************/

GenericTreeModelItem* GenericTreeModel::GetGenericItemByIndex(INT32 index)
{
	GenericTreeModelItem* item = m_items.Get(index);

	if (item)
	{
		item->SetLastIndex(index);
	}

	return item;
};

/***********************************************************************************
**
**	GenericTreeModel::GetGenericItemByID
**
***********************************************************************************/

GenericTreeModelItem* GenericTreeModel::GetGenericItemByID(INT32 id)
{
	OpTreeModelItem* item;
	GetItemByID(id, item);
	return item ? (GenericTreeModelItem*) item : NULL;
}

/***********************************************************************************
**
**	GenericTreeModel::GetGenericItemByIDAndType
**
***********************************************************************************/

GenericTreeModelItem* GenericTreeModel::GetGenericItemByIDAndType(INT32 id, OpTypedObject::Type type)
{
	OpTreeModelItem* item;
	GetItemByIDAndType(id, type, item);
	return item ? (GenericTreeModelItem*) item : NULL;
}

/***********************************************************************************
**
**	GenericTreeModel::GetGenericParentItem
**
***********************************************************************************/

GenericTreeModelItem* GenericTreeModel::GetGenericParentItem(INT32 index)
{
	GenericTreeModelItem* item = GetGenericItemByIndex(index);
	return item ? item->GetGenericParentItem() : NULL;
}

/***********************************************************************************
**
**	GenericTreeModel::RegroupItems
**
***********************************************************************************/

OP_STATUS GenericTreeModel::RegroupItems()
{
	if (!GetTreeModelGrouping() || GetCount() == 0)
	{
		return OpStatus::OK;
	}

	BOOL broadcast = SetChangeType(TREE_CHANGED);
	if (!GetTreeModelGrouping()->HasGrouping())
	{
		if (broadcast)
		{
			BroadcastTreeChanging();
		}

		RemoveGroups();
		
		if (broadcast)
		{
			BroadcastTreeChanged();
		}
		return OpStatus::OK;
	}

	OpAutoArray<OpVector<GenericTreeModelItem> > result (OP_NEWA(OpVector<GenericTreeModelItem>, GetTreeModelGrouping()->GetGroupCount()));

	RETURN_OOM_IF_NULL(result.get());

	for (int i = 0, count = GetTreeModelGrouping()->GetGroupCount(); i < count; i++)
	{
		GenericTreeModelItem* header = static_cast<GenericTreeModelItem*>(GetTreeModelGrouping()->GetGroupHeader(i));
		header->SetGenericParentItem(NULL);
		header->SetSubtreeSize(0);
		if (OpStatus::IsError(result[i].Add(header)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (broadcast)
	{
		BroadcastTreeChanging();
	}

	RemoveGroups();

	INT32 last_group = GetTreeModelGrouping()->GetGroupCount() - 1;
	for (INT32 i = 0; i < GetCount(); i++)
	{
		GenericTreeModelItem* item = m_items.Get(i);
		int group = GetTreeModelGrouping()->GetGroupForItem(item);
		if (group > -1)
		{
			for (UINT32 j = i, count = i + item->GetSubtreeSize()+1; j < count; j++)
			{
				if (OpStatus::IsError(result[group].Add(m_items.Get(j))))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			item->SetGenericParentItem(result[group].Get(0));
			i += item->GetSubtreeSize();
		} 
		else if (!GetTreeModelGrouping()->IsVisibleHeader(item))
		{
			if (OpStatus::IsError(result[last_group].Add(item)))
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			item->SetGenericParentItem(result[last_group].Get(0));
			item->SetSubtreeSize(0);
		}
	}

	// rebuild tree
	UINT32 pos = 0;
	for (UINT32 i = 0, count = GetTreeModelGrouping()->GetGroupCount(); i < count; i++)
	{
		GenericTreeModelItem* parent_item = result[i].Get(0);
		for (UINT32 j = 0, count = result[i].GetCount(); j < count; j++)
		{
			GenericTreeModelItem* item = result[i].Get(j);
			item->SetLastIndex(pos);
			m_items.Replace(pos++, item);
			if (parent_item != item)
			{
				parent_item->AddSubtreeSize(1);			
			}
		}
	}

	if (broadcast)
	{
		BroadcastTreeChanged();
	}
	
	return OpStatus::OK;
	
}

/***********************************************************************************
**
**	GenericTreeModel::RemoveGroups()
**
***********************************************************************************/

OP_STATUS GenericTreeModel::RemoveGroups()
{
	if (!GetTreeModelGrouping() || GetCount() == 0)
	{
		return OpStatus::OK;
	}

	for (INT32 i = 0; i < GetCount(); i++)
	{
		GenericTreeModelItem* item = m_items.Get(i);
		if(item->GetGenericParentItem() && GetTreeModelGrouping()->IsHeader(item->GetGenericParentItem()))
		{
			item->SetGenericParentItem(NULL);
			i += item->GetSubtreeSize();
		}
		else if (GetTreeModelGrouping()->IsHeader(item))
		{
			item->SetSubtreeSize(0);
		}
	}
	return OpStatus::OK;
}
