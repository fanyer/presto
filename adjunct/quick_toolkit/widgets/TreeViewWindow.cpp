/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/TreeViewWindow.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/widgets/WidgetContainer.h"


// --------------------------------------------------------------------------------
//
// class TreeViewWindow - a DesktopWindow subclass for implementing the dropdown window
//
// --------------------------------------------------------------------------------

/***********************************************************************************
**
**	TreeViewWindow
**
***********************************************************************************/
TreeViewWindow::TreeViewWindow()
	: m_dropdown(NULL),
	  m_tree_view(NULL),
	  m_tree_model(NULL),
	  m_items_deletable(FALSE),
	  m_blocked_parent(NULL)
{
}


/***********************************************************************************
 **
 ** ~TreeViewWindow
 **
 ***********************************************************************************/
TreeViewWindow::~TreeViewWindow()
{
	if( m_tree_view )
		m_tree_view->SetTreeModel(0);

	m_dropdown->OnWindowDestroyed();

	// ---------------------
	// Delete all deletable items and the model:
	// ---------------------
	Clear(TRUE);
}


/***********************************************************************************
 **
 ** Construct
 **
 ***********************************************************************************/
OP_STATUS TreeViewWindow::Construct(OpTreeViewDropdown* dropdown, const char *transp_window_skin)
{
	if(!dropdown)
		return OpStatus::ERR;

	m_dropdown = dropdown;

	DesktopWindow* parent_window = m_dropdown->GetParentDesktopWindow();

	OpWindow::Effect effect = transp_window_skin ? OpWindow::EFFECT_TRANSPARENT : OpWindow::EFFECT_NONE;

	RETURN_IF_ERROR(Init(OpWindow::STYLE_POPUP, parent_window, effect));

	WidgetContainer * widget_container = GetWidgetContainer();
	OpWidget        * root_widget      = widget_container->GetRoot();

	// ---------------------
	// We want the background erased:
	// ---------------------
	widget_container->SetEraseBg(TRUE);

	RETURN_IF_ERROR(OpTreeView::Construct(&m_tree_view));

	if (transp_window_skin)
	{
		// Remove skin from treeview and change window skin to transp_window_skin.
		m_tree_view->GetBorderSkin()->SetImage("");
		SetSkin(transp_window_skin, "Edit Skin");
		// Disable CSS border which WidgetContainer set to avoid painting the skin on the root.
		root_widget->SetHasCssBorder(FALSE);
	}

	m_tree_view->SetShowColumnHeaders(FALSE);

	// ---------------------
	// Use forced focus so that items look 'selected' in dropdown
	// ---------------------
	m_tree_view->SetForcedFocus(TRUE);

	// ---------------------
	// So that a mouse hover will select an item like in regular dropdowns
	// ---------------------
	m_tree_view->SetSelectOnHover(TRUE);

	// ---------------------
	// Don't allow wide icons (Bug 276205)
	// ---------------------
	m_tree_view->SetRestrictImageSize(TRUE);

	// ---------------------
	// Allow deselection only from the keyboard
	// ---------------------
	m_tree_view->SetDeselectableByMouse(FALSE);
	m_tree_view->SetDeselectableByKeyboard(TRUE);

	// ---------------------
	// Allow multi line items
	// ---------------------
	m_tree_view->SetAllowMultiLineItems(TRUE);

	// ---------------------
	// Force a background line
	// ---------------------
	m_tree_view->SetForceBackgroundLine(effect != OpWindow::EFFECT_TRANSPARENT);

	// ---------------------
	// Set the custom background line color
	// ---------------------
	m_tree_view->SetCustomBackgroundLineColor(OP_RGB(0xf5, 0xf5, 0xf5));

	root_widget->AddChild(m_tree_view);
	m_tree_view->SetListener(dropdown);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 ** Clear
 **
 ***********************************************************************************/
OP_STATUS TreeViewWindow::Clear(BOOL delete_model)
{
	if(m_tree_model)
	{
		if(m_items_deletable)
			m_tree_model->DeleteAll();
		else
			m_tree_model->RemoveAll();

		if(delete_model)
		{
			OP_DELETE(m_tree_model);
			m_tree_model = 0;
		}
	}

	if(m_dropdown)
		m_dropdown->OnClear();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 ** SetModel
 **
 ***********************************************************************************/
void TreeViewWindow::SetModel(GenericTreeModel * tree_model, BOOL items_deletable)
{
	m_tree_model = tree_model;
	m_tree_view->SetTreeModel(m_tree_model);

	// ---------------------
	// Set the column sizes depending on the number of columns
	// ---------------------
	switch(m_dropdown->GetMaxNumberOfColumns())
	{
		case 3:
			m_tree_view->SetColumnWeight(0, m_dropdown->GetColumnWeight(0));
			m_tree_view->SetColumnWeight(1, m_dropdown->GetColumnWeight(1));
			m_tree_view->SetColumnWeight(2, m_dropdown->GetColumnWeight(2));
			break;
		case 2:
			m_tree_view->SetColumnWeight(0, m_dropdown->GetColumnWeight(0));
			m_tree_view->SetColumnWeight(1, m_dropdown->GetColumnWeight(1));
			m_tree_view->SetColumnUserVisibility(2, FALSE);
			break;
		case 1:
		default:
			m_tree_view->SetColumnWeight(0, m_dropdown->GetColumnWeight(0));
			m_tree_view->SetColumnUserVisibility(1, FALSE);
			m_tree_view->SetColumnUserVisibility(2, FALSE);
			break;
	}

	// ---------------------
	// Set whether the items should be deletable by us in Clear
	// ---------------------
	m_items_deletable = items_deletable;
}

/***********************************************************************************
 **
 ** OnInputAction
 **
 ***********************************************************************************/
BOOL TreeViewWindow::OnInputAction(OpInputAction* action)
{
	return m_tree_view->OnInputAction(action);
}


/***********************************************************************************
 **
 ** GetSelectedItem
 **
 ***********************************************************************************/
OpTreeModelItem * TreeViewWindow::GetSelectedItem(int * position)
{
	if(position)
		*position = GetSelectedLine();

	return m_tree_view->GetSelectedItem();
}


/***********************************************************************************
 **
 ** GetSelectedLine
 **
 ***********************************************************************************/
int TreeViewWindow::GetSelectedLine()
{
	return m_tree_view->GetSelectedItemPos();
}

/***********************************************************************************
**
** SetSelectedLine
**
***********************************************************************************/
void TreeViewWindow::SetSelectedItem(OpTreeModelItem *item, BOOL selected)
{
	m_tree_view->SetSelectedItem(m_tree_view->GetItemByModelItem(item));
}

/***********************************************************************************
 **
 ** IsLastLineSelected
 **
 ***********************************************************************************/
BOOL TreeViewWindow::IsLastLineSelected()
{
	return m_tree_view->IsLastLineSelected();
}


/***********************************************************************************
 **
 ** IsFirstLineSelected
 **
 ***********************************************************************************/
BOOL TreeViewWindow::IsFirstLineSelected()
{
	return m_tree_view->IsFirstLineSelected();
}


/***********************************************************************************
 **
 ** UnSelectAndScroll
 **
 ***********************************************************************************/
void TreeViewWindow::UnSelectAndScroll()
{
	m_tree_view->ScrollToLine(0);
	m_tree_view->SetSelectedItem(-1);
}


#ifndef QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN
/***********************************************************************************
 **
 ** Create
 **
 ***********************************************************************************/
OP_STATUS OpTreeViewWindow::Create(OpTreeViewWindow **w,
								 OpTreeViewDropdown* dropdown,
								 const char *transp_window_skin)
{
	TreeViewWindow *win = OP_NEW(TreeViewWindow, ());
	if (win == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = win->Construct(dropdown, transp_window_skin);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(win);
		return status;
	}
	*w = win;
	return OpStatus::OK;
}
#endif // QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN


/***********************************************************************************
 **
 ** OnLayout
 **
 ***********************************************************************************/
void TreeViewWindow::OnLayout()
{
	UpdateMenu();
}


void TreeViewWindow::OnShow(BOOL show)
{
	if (show && m_blocked_parent == NULL)
	{
		DesktopWindow* parent = NULL;
		if (GetModelItem().GetParentItem() != NULL)
			parent = GetModelItem().GetParentItem()->GetDesktopWindow();

		if (parent != NULL && parent == g_application->GetPopupDesktopWindow())
		{
			// Block the parent window from closing while the drop down window is shown.
			m_blocked_parent = parent;
			m_blocked_parent->BlockWindowClosing();
		}
	}
	else if (!show && m_blocked_parent != NULL)
	{
		m_blocked_parent->UnblockWindowClosing();
		m_blocked_parent = NULL;
	}
}

void TreeViewWindow::OnClose(BOOL user_initiated)
{
	if (m_blocked_parent != NULL)
	{
		m_blocked_parent->UnblockWindowClosing();
		m_blocked_parent = NULL;
	}
}

/***********************************************************************************
 **
 ** CalculateRect
 **
 ***********************************************************************************/
OpRect TreeViewWindow::CalculateRect()
{
	OpRect edit_rect  = m_dropdown->GetRect();
	OpRect edit_rect_screen = m_dropdown->GetScreenRect();

 	INT32 num_lines   = MIN(m_dropdown->GetMaxLines(), m_tree_view->GetLineCount());
 	INT32 line_height = m_tree_view->GetLineHeight();

	INT32 width  = edit_rect.width;
	INT32 height = num_lines*line_height;

	// We need to add both the TreeViewWindow padding and the Treeview padding 
	OpRect r(0,0,width,height);
	m_tree_view->AddPadding(r);
	if( height > r.height)
		height += (height-r.height);

	INT32 left, top, bottom, right;
	GetSkinImage()->GetPadding(&left, &top, &right, &bottom);
	int padding_width = left + right;
	int padding_height = top + bottom;

	width += padding_width;
	height += padding_height;

	OpRect rect;

	OpRect sr = m_dropdown->GetScreenRect();

	// The dropdown must be clipped to the workspace. If the window horizontally spans two
	// monitors (we assume two monitors at the most) and the platform reports two different
	// areas, use the smallest height and the combined width of the two.
	OpPoint pos;
	OpScreenProperties p1, p2;
	pos = sr.TopLeft();
	g_op_screen_info->GetProperties(&p1, m_dropdown->GetWidgetContainer()->GetWindow(), &pos);
	pos = sr.TopRight();
	g_op_screen_info->GetProperties(&p2, m_dropdown->GetWidgetContainer()->GetWindow(), &pos);
	OpRect workspace_rect(p1.workspace_rect);
	if (!workspace_rect.Equals(p2.workspace_rect))
	{
		workspace_rect.height = min(p1.workspace_rect.height, p2.workspace_rect.height);
		workspace_rect.width  = p1.workspace_rect.width + p2.workspace_rect.width;
	}

	DesktopWindow* dw = g_application->GetActiveDesktopWindow(TRUE);
	if (dw && !SupportsExternalCompositing())
	{
		// Shaped dropdowns must be shown inside the parent window as well when compositing
		// is not available. Modify height so that it fits within the area of the window with
		// most space available

		OpRect dr;
		dw->GetInnerPos(dr.x, dr.y);
		dw->GetInnerSize((UINT32&)dr.width, (UINT32&)dr.height);
		dr.IntersectWith(workspace_rect);

		int space_above = sr.y - dr.y;
		int space_below = dr.height - (sr.y + sr.height - dr.y);
		if (space_below >= space_above)
		{
			if (height > space_below)
				height = space_below;
		}
		else
		{
			if (height > space_above)
				height = space_above;
		}
		rect = WidgetWindow::GetBestDropdownPosition(m_dropdown, width, height, TRUE, &dr);
	}
	else
	{
		rect = WidgetWindow::GetBestDropdownPosition(m_dropdown, width, height, TRUE, &workspace_rect);
	}

	GetSkinImage()->GetMargin(&left, &top, &right, &bottom);
	rect.x += left;
	rect.y = rect.y < edit_rect_screen.y ? rect.y - bottom : rect.y + top;

	return rect;
}


/***********************************************************************************
 **
 ** UpdateMenu
 **
 ***********************************************************************************/
void TreeViewWindow::UpdateMenu()
{
 	OpRect rect;
 	GetBounds(rect);

	INT32 left, top, bottom, right;
	GetSkinImage()->GetPadding(&left, &top, &right, &bottom);
	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;

	m_tree_view->SetRect(rect);
}


/***********************************************************************************
 **
 ** AddDesktopWindowListener
 **
 ***********************************************************************************/
OP_STATUS TreeViewWindow::AddDesktopWindowListener(DesktopWindowListener* listener)
{
	return AddListener(listener);
}


/***********************************************************************************
 **
 ** ClosePopup
 **
 ***********************************************************************************/
void TreeViewWindow::ClosePopup()
{
	Close(TRUE);
}


/***********************************************************************************
 **
 ** IsPopupVisible
 **
 ***********************************************************************************/
BOOL TreeViewWindow::IsPopupVisible()
{
	return IsShowed();
}


/***********************************************************************************
 **
 ** SetPopupOuterPos
 **
 ***********************************************************************************/
void TreeViewWindow::SetPopupOuterPos(INT32 x, INT32 y)
{
	SetOuterPos(x,y);
}


/***********************************************************************************
 **
 ** SetPopupOuterSize
 **
 ***********************************************************************************/
void TreeViewWindow::SetPopupOuterSize(UINT32 width, UINT32 height)
{
	SetOuterSize(width, height);
}


/***********************************************************************************
 **
 ** SetVisible
 **
 ***********************************************************************************/
void TreeViewWindow::SetVisible(BOOL vis, BOOL default_size)
{
	if (default_size)
	{
		OpRect size = CalculateRect();
		m_dropdown->AdjustSize(size);
		Show(vis, &size);
	}
	else
	{
		Show(vis);
	}
}

void TreeViewWindow::SetVisible(BOOL vis, OpRect rect)
{
	INT32 num_lines   = MIN(m_dropdown->GetMaxLines(), m_tree_view->GetLineCount());
	INT32 line_height = m_tree_view->GetLineHeight();

	INT32 width  = rect.width;
	INT32 height = num_lines * line_height;

	// We need to add both the TreeViewWindow padding and the Treeview padding 
	OpRect r(0, 0, width, height);
	m_tree_view->AddPadding(r);
	if (height > r.height)
		height += (height - r.height);

	INT32 left, top, bottom, right;
	GetSkinImage()->GetPadding(&left, &top, &right, &bottom);
	height += top + bottom;

	if (height > rect.height)
		rect.height = height;

	Show(vis, &rect);
}

/***********************************************************************************
 **
 ** SendInputAction
 **
 ***********************************************************************************/
BOOL TreeViewWindow::SendInputAction(OpInputAction* action)
{
	return OnInputAction(action);
}

void TreeViewWindow::SetTreeViewName(const char*  name) 
{
	m_tree_view->SetName(name);
}
