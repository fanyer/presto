/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_Tree.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"


QuickBinder::QuickTreeViewModelBinding::QuickTreeViewModelBinding()
	: m_tree_view(NULL), m_property(NULL)
{
}

QuickBinder::QuickTreeViewModelBinding::~QuickTreeViewModelBinding()
{
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::QuickTreeViewModelBinding::Init(
		QuickTreeView& tree_view, OpProperty<OpTreeModel*>& property)
{
	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &QuickTreeViewModelBinding::OnPropertyChanged)));

	m_property = &property;
	m_tree_view = &tree_view;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

const QuickWidget* QuickBinder::QuickTreeViewModelBinding::GetBoundWidget()
		const
{
	return m_tree_view;
}

void QuickBinder::QuickTreeViewModelBinding::OnPropertyChanged(
		OpTreeModel* new_value)
{
	const INT32 sort_column = m_tree_view->GetOpWidget()->GetSortByColumn();
	const BOOL sort_ascending = m_tree_view->GetOpWidget()->GetSortAscending();
	m_tree_view->GetOpWidget()->SetTreeModel(new_value, sort_column, sort_ascending);
}




QuickBinder::QuickTreeViewSelectionBinding::QuickTreeViewSelectionBinding()
	: m_tree_view(NULL), m_property(NULL)
{
}

QuickBinder::QuickTreeViewSelectionBinding::~QuickTreeViewSelectionBinding()
{
	if (m_tree_view != NULL)
	{
		m_tree_view->RemoveOpWidgetListener(*this);
	}
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::QuickTreeViewSelectionBinding::Init(
		QuickTreeView& tree_view, OpProperty<INT32>& property)
{
	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &QuickTreeViewSelectionBinding::OnPropertyChanged)));
	m_property = &property;

	RETURN_IF_ERROR(tree_view.AddOpWidgetListener(*this));
	m_tree_view = &tree_view;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

const QuickWidget* QuickBinder::QuickTreeViewSelectionBinding::GetBoundWidget()
		const
{
	return m_tree_view;
}

void QuickBinder::QuickTreeViewSelectionBinding::OnPropertyChanged(
		INT32 new_value)
{
	const INT32 view_index =
			m_tree_view->GetOpWidget()->GetItemByModelPos(new_value);
	m_tree_view->GetOpWidget()->SetSelectedItem(view_index);
}

void QuickBinder::QuickTreeViewSelectionBinding::OnChange(OpWidget* widget,
		BOOL by_mouse)
{
	m_property->Set(m_tree_view->GetOpWidget()->GetSelectedItemModelPos());
}

void QuickBinder::QuickTreeViewSelectionBinding::OnSortingChanged(
		OpWidget* widget)
{
	// View sorting change must not affect which model item is selected.
	OnPropertyChanged(m_property->Get());
}



QuickBinder::QuickTreeViewMultiSelectionBinding::QuickTreeViewMultiSelectionBinding()
	: m_tree_view(NULL), m_property(NULL)
{
}

QuickBinder::QuickTreeViewMultiSelectionBinding::~QuickTreeViewMultiSelectionBinding()
{
	if (m_tree_view != NULL)
	{
		m_tree_view->RemoveOpWidgetListener(*this);
	}
}

OP_STATUS QuickBinder::QuickTreeViewMultiSelectionBinding::Init(
		QuickTreeView& tree_view, OpProperty<OpINT32Vector>& property)
{
	RETURN_IF_ERROR(tree_view.AddOpWidgetListener(*this));
	m_tree_view = &tree_view;

	m_property = &property;

	return OpStatus::OK;
}

const QuickWidget*
		QuickBinder::QuickTreeViewMultiSelectionBinding::GetBoundWidget() const
{
	return m_tree_view;
}

void QuickBinder::QuickTreeViewMultiSelectionBinding::OnChange(OpWidget* widget,
		BOOL by_mouse)
{
	OpINT32Vector indices;
	m_tree_view->GetOpWidget()->GetSelectedItems(indices, FALSE);
	m_property->Set(indices);
}
