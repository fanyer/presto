/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/ColumnListAccessor.h"
#include "modules/widgets/OpScrollbar.h"



class TreeModelGroupingWrapper: public OpTreeModel::TreeModelGrouping
{
public:
								TreeModelGroupingWrapper(OpTreeModel::TreeModelGrouping* grouping, TreeViewModel* model) : m_grouping(grouping), m_model(model) {}
			
	virtual INT32				GetGroupForItem(const OpTreeModelItem* item) const { return m_grouping->GetGroupForItem(static_cast<const TreeViewModelItem*>(item)->GetModelItem()); }
	virtual INT32				GetGroupCount() const { return m_grouping->GetGroupCount(); }
	virtual OpTreeModelItem*	GetGroupHeader(UINT32 group_idx) const { return m_model->GetItemByModelItem(m_grouping->GetGroupHeader(group_idx)); }
	virtual BOOL				IsVisibleHeader(const OpTreeModelItem* header) const { return m_grouping->IsVisibleHeader(static_cast<const TreeViewModelItem*>(header)->GetModelItem());};
	virtual BOOL				IsHeader(const OpTreeModelItem* header) const { return static_cast<const TreeViewModelItem*>(header)->IsHeader(); }
	virtual BOOL				HasGrouping() const { return m_grouping->HasGrouping();}
protected:
	OpTreeModel::TreeModelGrouping* m_grouping;
	TreeViewModel* m_model;
};

/***********************************************************************************
**
**	TreeViewModel::~TreeViewModel
**
***********************************************************************************/
TreeViewModel::~TreeViewModel()
{
	if (m_model)
		m_model->RemoveListener(this);

	Clear();
	if (GetTreeModelGrouping())
	{
		OP_DELETE(GetTreeModelGrouping());
		SetTreeModelGrouping(NULL);
	}
}

/***********************************************************************************
**
**	TreeViewModel::SetModel
**
***********************************************************************************/
void TreeViewModel::SetModel(OpTreeModel* model, OpTreeModel::TreeModelGrouping* grouping)
{
	ModelLock lock(this);

	if (grouping)
	{
		OP_DELETE(GetTreeModelGrouping());
		SetTreeModelGrouping(OP_NEW(TreeModelGroupingWrapper, (grouping, this)));
	}

	if (m_model)
	{
		m_model->RemoveListener(this);
	}

	Clear();

	m_model = model;

	if (!m_model)
		return;

	m_model->AddListener(this);

	Init();
}


/***********************************************************************************
**
**	TreeViewModel::Resort
**
***********************************************************************************/
void TreeViewModel::Resort()
{
	if (!m_model)
		return;

	ModelLock lock(this);

	if (GetSortListener())
	{
		Sort();
	}
	else
	{
		m_resorting = TRUE;

		RemoveAll();

		INT32 count = m_model->GetItemCount();

		for (INT32 i = 0; i < count; i++)
		{
			void *item = NULL;

			m_hash_table.GetData(m_model->GetItemByPosition(i), &item);

			OP_ASSERT(item);

			INT32 parent = m_model->GetItemParent(i);

			AddLast((TreeViewModelItem*)item, parent);
		}

		m_resorting = FALSE;
	}
}

/***********************************************************************************
**
**	TreeViewModel::Regroup
**
***********************************************************************************/
void TreeViewModel::Regroup()
{
	if (!m_model || !GetTreeModelGrouping())
		return;
	ModelLock lock(this);
	RegroupItems();
	return;
}

/***********************************************************************************
**
**	TreeViewModel::OnItemAdded
**
***********************************************************************************/
void TreeViewModel::OnItemAdded(OpTreeModel* tree_model, INT32 index)
{
	TreeViewModelItem* item = OP_NEW(TreeViewModelItem, (m_model->GetItemByPosition(index)));
    if (!item) return;
	if (GetSortListener())
	{
		INT32 parent = GetIndexByModelIndex(m_model->GetItemParent(index));
		if (parent == -1 && GetTreeModelGrouping() && GetTreeModelGrouping()->HasGrouping())
		{
			parent = static_cast<TreeViewModelItem*>(GetTreeModelGrouping()->GetGroupHeader(GetTreeModelGrouping()->GetGroupForItem(item)))->GetIndex();
		}

		AddSorted(item, parent);
	}
	else
	{
		INT32 parent = m_model->GetItemParent(index);

		if (index < GetCount() && GetParentIndex(index) == parent)
		{
			InsertBefore(item, index);
		}
		else
		{
			AddLast(item, parent);
		}
	}
}

/***********************************************************************************
**
**	TreeViewModel::OnSubtreeRemoving
**
***********************************************************************************/
void TreeViewModel::OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size)
{
	Delete(GetIndexByModelIndex(index), subtree_size > 0);
	RegroupItem(GetIndexByModelIndex(parent_index));
}

/***********************************************************************************
**
**	TreeViewModel::OnItemChanged
**
***********************************************************************************/
void TreeViewModel::OnItemChanged(OpTreeModel* tree_model, INT32 index, BOOL sort)
{
	if (index == -1)
	{
		ChangeAll();
	}
	else
	{
		Change(GetIndexByModelIndex(index), sort);
	}
}

/***********************************************************************************
**
**	TreeViewModel::OnTreeChanging
**
***********************************************************************************/
void TreeViewModel::OnTreeChanging(OpTreeModel* tree_model)
{
	BeginChange();
	DeleteAll();
	// EndChange will be called in OnTreeChanged
}

/***********************************************************************************
**
**	TreeViewModel::OnTreeChanged
**
***********************************************************************************/
void TreeViewModel::OnTreeChanged(OpTreeModel* tree_model)
{
	Init();
	// BeginChange was called in OnTreeChanging
	EndChange();
}

/***********************************************************************************
**
**	TreeViewModel::OnTreeDeleted
**
***********************************************************************************/
void TreeViewModel::OnTreeDeleted(OpTreeModel* tree_model)
{
	m_model = NULL;
	Clear();
}

/***********************************************************************************
**
**	TreeViewModel::Init
**
***********************************************************************************/
void TreeViewModel::Init()
{
	INT32 count = m_model->GetItemCount();

	//SetInitialSize(count);

	for (INT32 i = 0; i < count; i++)
	{
		TreeViewModelItem* item = OP_NEW(TreeViewModelItem, (m_model->GetItemByPosition(i)));
        if (!item) break;

		INT32 parent = m_model->GetItemParent(i);

		AddLast(item, parent);
	}

	if (GetTreeModelGrouping())
	{
		Regroup();
	}

	if (GetSortListener())
		Sort();
}

/***********************************************************************************
**
**	TreeViewModel::Clear
**
***********************************************************************************/
void TreeViewModel::Clear()
{
	m_hash_table.RemoveAll();
	DeleteAll();
}

/***********************************************************************************
**
**	TreeViewModel::GetIndexByModelIndex
**
***********************************************************************************/
INT32 TreeViewModel::GetIndexByModelIndex(INT32 model_index)
{
	if (!m_model)
		return -1;

	return GetIndexByModelItem(m_model->GetItemByPosition(model_index));
}

/***********************************************************************************
**
**	TreeViewModel::GetIndexByModelItem
**
***********************************************************************************/
INT32 TreeViewModel::GetIndexByModelItem(OpTreeModelItem* model_item)
{
	TreeViewModelItem* item = GetItemByModelItem(model_item);

	return item ? item->GetIndex() : -1;
}

/***********************************************************************************
**
**	TreeViewModel::GetItemByModelItem
**
***********************************************************************************/
TreeViewModelItem* TreeViewModel::GetItemByModelItem(OpTreeModelItem* model_item)
{
	if (!model_item)
		return NULL;

	void *item = NULL;

	m_hash_table.GetData(model_item, &item);

	OP_ASSERT(item);

	return (TreeViewModelItem*)item;
}

/***********************************************************************************
**
**	TreeViewModel::GetModelIndexByIndex
**
***********************************************************************************/
INT32 TreeViewModel::GetModelIndexByIndex(INT32 index)
{
	if (!GetSortListener())
		return index;

	TreeViewModelItem* item = GetItemByIndex(index);

	if (!item)
		return -1;

	INT32 count = m_model->GetItemCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpTreeModelItem* compare_item = m_model->GetItemByPosition(i);

		if (item->GetModelItem() == compare_item)
			return i;
	}

	return -1;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void TreeViewModel::SetView(OpTreeView* view)
{
	m_view = view;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS TreeViewModel::AccessibilityClicked()
{
	return m_view->AccessibilityClicked();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TreeViewModel::AccessibilityChangeSelection(Accessibility::SelectionSet flags, OpAccessibleItem* child)
{
	int i;
	int count = GetCount();
	for (i = 0; i < count; i++)
	{
/*		TreeViewModelItem * item = GetItemByIndex(i);
		if ((flags == kSelectionSelectAll) || (item->GetOrCreateAccessibleObject() == child))
		{
			m_view->SetSelected(i, flags != kSelectionSetRemove);
			if (flags == kSelectionSetSelect)
				SetFocusToItem(i);
		}
		else if ((flags == kSelectionSetSelect) || (flags == kSelectionDeselectAll))
			m_view->SetSelected(i, FALSE);
		m_view->InvalidateAll();*/
	}
//	SelectItem(pos, FALSE, control, shift, TRUE);
	return TRUE;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void TreeViewModel::SetFocusToItem(int index)
{
	AccessibilitySetFocus();
	m_view->SetSelected(index, TRUE);
//	m_view->SelectItem(index);

/*	int i;
	int count = GetCount();
	for (i = 0; i < count; i++)
	{
		TreeViewModelItem * item = GetItemByIndex(i);
		if (i == index)
			item->m_flags |= OpTreeModelItem::FLAG_FOCUSED;
		else if (item->m_flags & OpTreeModelItem::FLAG_FOCUSED)
			item->m_flags &= ~OpTreeModelItem::FLAG_FOCUSED;
	}
	if (m_view)
		m_view->InvalidateAll();*/
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TreeViewModel::AccessibilitySetFocus()
{
	return m_view->AccessibilitySetFocus();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS TreeViewModel::AccessibilityGetText(OpString& str)
{
	return OpStatus::ERR;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS TreeViewModel::AccessibilityGetDescription(OpString& str)
{
	if (m_model)
		return m_model->GetTypeString(str);
	return OpStatus::ERR;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS TreeViewModel::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	OpRect off_rect;

	rect = m_view->GetVisibleRect();
	m_view->AccessibilityGetAbsolutePosition(off_rect);
	rect.x += off_rect.x;
	rect.y += off_rect.y;
	if (m_view->m_horizontal_scrollbar)
		rect.x -= m_view->m_horizontal_scrollbar->GetValue();
	if (m_view->m_vertical_scrollbar)
		rect.y -= m_view->m_vertical_scrollbar->GetValue();
	if (rect.height < m_view->GetLineCount() * m_view->m_line_height)
		rect.height = m_view->GetLineCount() * m_view->m_line_height;
	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Accessibility::ElementKind TreeViewModel::AccessibilityGetRole() const
{
	if (m_view->IsFlat())
	{
		return Accessibility::kElementKindList;
	}
	return Accessibility::kElementKindOutlineList;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Accessibility::State TreeViewModel::GetUnfilteredState() const
{
	if(m_view)
	{
		return m_view->GetUnfilteredState();
	}
	else
	{
		return Accessibility::kAccessibilityStateNone;
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Accessibility::State TreeViewModel::AccessibilityGetState()
{
	Accessibility::State state = GetUnfilteredState();
	if (m_view && m_view->GetSelectedItem())
		state &= ~Accessibility::kAccessibilityStateFocused;
	return state;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int TreeViewModel::GetAccessibleChildrenCount()
{
	return m_view->GetLineCount()+1;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* TreeViewModel::GetAccessibleParent()
{
	return m_view;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* TreeViewModel::GetAccessibleChild(int i)
{
	if ((i >= 0) && (i < m_view->GetLineCount()))
	{
		TreeViewModelItem * item = GetItemByIndex(m_view->GetItemByLine(i));
		if (item)
		{
			item->SetViewModel(this);
			return item;
		}
	}
	if (i == m_view->GetLineCount())
	{
		m_view->CreateColumnListAccessorIfNeeded();
		return m_view->m_column_list_accessor;
	}
	return NULL;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int TreeViewModel::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int n = m_view->GetLineCount();
	if (child == m_view->m_column_list_accessor)
		return n;

	for (int i = 0; i < n; i++)
	{
		if (GetItemByIndex(m_view->GetItemByLine(i)) == child)
			return i;
	}

	return Accessibility::NoSuchChild;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* TreeViewModel::GetAccessibleChildOrSelfAt(int x, int y)
{
	if (m_view)
	{
		int i = m_view->GetItemAt(x, y);
		if ((i >= 0) && (i < GetCount()))
		{
			TreeViewModelItem * item = GetItemByIndex(i);
			if (item)
			{
				item->SetViewModel(this);
				return item;
			}
		}
		m_view->CreateColumnListAccessorIfNeeded();
		if (m_view->m_column_list_accessor->GetAccessibleChildOrSelfAt(x,y))
			return m_view->m_column_list_accessor;
	}
	return this;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* TreeViewModel::GetAccessibleFocusedChildOrSelf()
{
	if (m_view)
	{
		TreeViewModelItem * item = GetItemByIndex(m_view->GetSelectedItemPos());
		if (item && item->IsFocused())
		{
			item->SetViewModel(this);
			return item;
		}
		m_view->CreateColumnListAccessorIfNeeded();
		if (m_view->m_column_list_accessor->GetAccessibleFocusedChildOrSelf())
			return m_view->m_column_list_accessor;
	}
	return this;
}

/***********************************************************************************
**
**	RegroupItem
**
***********************************************************************************/
void TreeViewModel::RegroupItem(INT32 pos)
{
	TreeViewModelItem* item = GetItemByIndex(pos);

	if (!GetTreeModelGrouping() || !GetTreeModelGrouping()->HasGrouping() || !item)
		return;

	while(item->GetParentItem() && !item->GetParentItem()->IsHeader())
		item = item->GetParentItem();

	TreeViewModelItem* parent = static_cast<TreeViewModelItem*>(GetTreeModelGrouping()->GetGroupHeader(GetTreeModelGrouping()->GetGroupForItem(item)));
	
	// Don't regroup in some cases, to avoid "thread jumping" (DSK-350587) '
	// eg when a thread must move to another group because an item in the thread was removed (painful to understand what's happening for the user)
	// the thread will be in the correct group when the whole tree is regrouped
	if (parent && parent->GetID() < item->GetParentItem()->GetID())
		return;

	if(parent && parent != item->GetParentItem())
	{
		if (!parent->IsOpen() && (IsEmptyGroup(parent) || m_view->GetSelectedItemPos() == pos))
		{
			m_view->OpenItemRecursively(parent->GetIndex(), TRUE, FALSE);
		}
		Move(item->GetIndex(), parent->GetIndex(), -1);

		if (GetSortListener())
			ResortItem(item->GetIndex());
	}
}

/***********************************************************************************
**
**	IsEmptyGroup
**
***********************************************************************************/
BOOL TreeViewModel::IsEmptyGroup(TreeViewModelItem* item)
{
	if (!item || !item->IsHeader())
	{
		return FALSE;
	}

	BOOL ret = item->GetSubtreeSize() == 0 ? TRUE : FALSE;
	if (!ret && item == GetTreeModelGrouping()->GetGroupHeader(GetTreeModelGrouping()->GetGroupCount() - 1))
	{
		ret = TRUE;
		for (int id = item->GetIndex() +1, count = item->GetIndex() + item->GetSubtreeSize() + 1; id < count; id++)
		{
			TreeViewModelItem* item = GetItemByIndex(id);
			if (!item->IsHeader())
			{
				ret = FALSE;
				break;
			}
		}
	}

	return ret;
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
