/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/Column.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"

OpTreeView::Column::Column(OpTreeView* tree_view, INT32 index) :
    m_tree_view(tree_view),
    m_column_index(index),
    m_weight(100 << 8),
    m_fixed_width(0),
    m_width(0),
    m_has_dropdown(FALSE),
	m_image_fixed_width(-1),
	m_fixed_image_drawarea(FALSE),
    m_is_column_visible(TRUE),
    m_is_column_user_visible(TRUE),
    m_is_column_matchable(FALSE)
{
    SetButtonTypeAndStyle(OpButton::TYPE_HEADER, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
#ifdef _MACINTOSH_
	SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);
#endif
	SetEllipsis(ELLIPSIS_END);
    SetListener(tree_view);

};

void OpTreeView::Column::OnMouseMove(const OpPoint &point)
{
    if (m_tree_view->IsColumnSizing())
    {
	m_tree_view->DoColumnSizing(this, point);
    }

    OpButton::OnMouseMove(point);
}

void OpTreeView::Column::OnMouseLeave()
{
    if (m_tree_view->IsColumnSizing())
    {
	m_tree_view->EndColumnSizing();
    }

    OpButton::OnMouseLeave();
}

void OpTreeView::Column::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
    if (m_tree_view->IsColumnSizing())
    {
	m_tree_view->EndColumnSizing();
    }
    else if (button != MOUSE_BUTTON_1 || !m_tree_view->StartColumnSizing(this, point, FALSE))
    {
	OpButton::OnMouseDown(point, button, nclicks);
    }
}

void OpTreeView::Column::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
    if (m_tree_view->IsColumnSizing())
    {
	m_tree_view->EndColumnSizing();
    }
    else
    {
	OpButton::OnMouseUp(point, button, nclicks);

	if (button == MOUSE_BUTTON_2 && m_tree_view->HasName())
	{
	    g_application->GetMenuHandler()->ShowPopupMenu("Internal Column List", PopupPlacement::AnchorAtCursor());
	}
    }
}

void OpTreeView::Column::OnSetCursor(const OpPoint &point)
{
    if (m_tree_view->IsColumnSizing() || m_tree_view->StartColumnSizing(this, point, TRUE))
    {
	SetCursor(CURSOR_VERT_SPLITTER);
    }
    else
    {
	OpButton::OnSetCursor(point);
    }
}

