/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/ColumnListAccessor.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/Column.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
Accessibility::State ColumnListAccessor::AccessibilityGetState()
{
	return m_parent->AccessibilityGetState();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS ColumnListAccessor::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	OpRect parent_rect;
	m_parent->AccessibilityGetAbsolutePosition(parent_rect);
	rect.x=parent_rect.x;
	rect.y=parent_rect.y;
	rect.width=parent_rect.width;
	rect.height= m_parent->m_column_headers_height;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
int	ColumnListAccessor::GetAccessibleChildrenCount()
{
	int n = m_parent->m_columns.GetCount();
	int children = 0;
	for (int i = 0; i < n; i++)
	{
		OpTreeView::Column* col = m_parent->m_columns.Get(i);
		if (col->IsColumnVisible())
			children++;
	}

	return children;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* ColumnListAccessor::GetAccessibleChild(int child_nr)
{
	int n = m_parent->m_columns.GetCount();
	int children = -1;
	OpTreeView::Column* col = NULL;
	for (int i = 0; i < n && children < child_nr; i++)
	{
		col = m_parent->m_columns.Get(i);
		if (col->IsColumnVisible())
		{
			children++;
		}
	}

	if (children == child_nr)
		return col;
	else
		return NULL;
}

int ColumnListAccessor::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = m_parent->m_columns.Find(static_cast<OpTreeView::Column*>(child));
	if (index == -1)
		return Accessibility::NoSuchChild;
	return index;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* ColumnListAccessor::GetAccessibleFocusedChildOrSelf()
{
	int n = m_parent->m_columns.GetCount();
	for (int i = 0; i < n; i++)
	{
		OpTreeView::Column* col = m_parent->m_columns.Get(i);
		OpAccessibleItem* child;
		if (col->IsColumnVisible() && (child = col->GetAccessibleFocusedChildOrSelf()) != NULL)
			return child;
	}

	return NULL;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* ColumnListAccessor::GetAccessibleChildOrSelfAt(int x, int y)
{
	int n = m_parent->m_columns.GetCount();
	for (int i = 0; i < n; i++)
	{
		OpTreeView::Column* col = m_parent->m_columns.Get(i);
		OpAccessibleItem* child;
		if (col->IsColumnVisible() && (child = col->GetAccessibleChildOrSelfAt(x,y)) != NULL)
			return child;
	}

	return NULL;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
