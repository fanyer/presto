/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/DropDownWindow.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpWindow.h"
#include "modules/display/fontdb.h"
#include "modules/display/vis_dev.h"
#include "modules/style/css.h"

////////////////////////////////////////////////////////////////////////////////
//
// If the platform is using a custum dropdown window then don't
// implement the default one
//
#ifndef WIDGETS_CUSTOM_DROPDOWN_WINDOW
OP_STATUS OpDropDownWindow::Create(OpDropDownWindow **w, OpDropDown* dropdown)
{
	DropDownWindow *win = OP_NEW(DropDownWindow, ());
	if (win == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = win->Construct(dropdown);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(win);
		win = NULL;
		return status;
	}
	*w = win;
	return OpStatus::OK;
}
#endif // WIDGETS_CUSTOM_DROPDOWN_WINDOW

/***********************************************************************************
**
**	DropDownWindow - a WidgetWindow subclass for implementing the dropdown window
**
***********************************************************************************/

DropDownWindow::DropDownWindow()
			: m_hot_tracking(FALSE),
			  m_dropdown(NULL),
			  m_listbox(NULL),
			  m_scale(0),
			  m_is_closed(FALSE)
{
}

OP_STATUS DropDownWindow::Construct(OpDropDown* dropdown)
{
	RETURN_IF_ERROR(Init(OpWindow::STYLE_POPUP, dropdown->GetParentOpWindow()));

	m_hot_tracking = FALSE;
	m_dropdown = dropdown;
	m_scale = m_dropdown->GetVisualDevice()->GetScale();

	RETURN_IF_ERROR(OpListBox::Construct(&m_listbox, FALSE, OpListBox::BORDER_STYLE_POPUP));

	m_listbox->SetRTL(dropdown->GetRTL());
	GetWidgetContainer()->GetRoot()->SetRTL(dropdown->GetRTL());

	GetWidgetContainer()->GetRoot()->GetVisualDevice()->SetScale(m_scale);
	GetWidgetContainer()->GetRoot()->AddChild(m_listbox);
	GetWidgetContainer()->SetEraseBg(TRUE);
	GetWidgetContainer()->GetRoot()->SetBackgroundColor(OP_RGB(255, 255, 255));

	m_listbox->SetListener(dropdown);
	m_listbox->hot_track = TRUE;

	if (m_dropdown->GetColor().use_default_foreground_color == FALSE)
		m_listbox->SetForegroundColor(dropdown->GetColor().foreground_color);
	if (m_dropdown->GetColor().use_default_background_color == FALSE)
		m_listbox->SetBackgroundColor(dropdown->GetColor().background_color);

	m_listbox->GetVisualDevice()->SetDocumentManager(m_dropdown->GetVisualDevice()->GetDocumentManager());

	m_listbox->SetFontInfo(	m_dropdown->font_info.font_info,
							m_dropdown->font_info.size,
							m_dropdown->font_info.italic,
							m_dropdown->font_info.weight,
							m_dropdown->font_info.justify,
							m_dropdown->font_info.char_spacing_extra);

	RETURN_IF_MEMORY_ERROR(m_listbox->ih.DuplicateOf(dropdown->ih));
	m_listbox->UpdateScrollbar();

	VisualDevice* vd = m_dropdown->GetVisualDevice();
	if (vd)
		vd->GetView()->AddScrollListener(this);
	return OpStatus::OK;
}

DropDownWindow::~DropDownWindow()
{
	if (m_dropdown) // check, in case Construct() failed early and Create() is deleting on its way to failure
	{
		m_dropdown->BlockPopupByMouse(); // For X11
		m_dropdown->SetBackOldItem();

		if (m_dropdown->GetListener())
			m_dropdown->GetListener()->OnDropdownMenu(m_dropdown, FALSE);

		m_dropdown->InvalidateAll();
		m_dropdown->m_dropdown_window = NULL;
	}

	CoreViewScrollListener::Out();
}

BOOL DropDownWindow::OnNeedPostClose()
{
	return TRUE;
}

void DropDownWindow::OnResize(UINT32 width, UINT32 height)
{
	WidgetWindow::OnResize(width, height);
	m_listbox->SetRect(OpRect(0, 0, unscaled_width, unscaled_height));
}

OpRect DropDownWindow::CalculateRect()
{
	VisualDevice* vis_dev = m_dropdown->GetVisualDevice();

	const UINT32 scrollbar_width = m_listbox->IsScrollable(TRUE) ? m_dropdown->GetInfo()->GetVerticalScrollbarWidth() : 0;
	INT32 listbox_width  = m_dropdown->ih.widest_item + scrollbar_width;
	INT32 listbox_height = m_dropdown->ih.GetTotalHeight();

	INT32 left, top, right, bottom;
	m_listbox->GetInfo()->GetBorders(m_listbox, left, top, right, bottom);
	listbox_width  += left + right;
	listbox_height += top  + bottom;

	unscaled_width  = MAX(m_dropdown->GetWidth(), listbox_width);
	unscaled_height = listbox_height;

#if defined(_MACINTOSH_)
	unscaled_width -= 2; // Needs this to align nicely with the textfield [ed 2004-01-07]
#endif

	// Scale width and height to screen coordinates
	INT32 scaled_width  = vis_dev->ScaleToScreen(unscaled_width);
	INT32 scaled_height = vis_dev->ScaleToScreen(unscaled_height);

	// Calculate a suitable maximum height in pixels
	INT32 dropdown_max_height = MIN(GetScreenHeight(), WIDGETS_POPUP_MAX_HEIGHT);

	if (scaled_height > dropdown_max_height)
	{
		unscaled_height = vis_dev->ScaleToDocRoundDown(dropdown_max_height);
		scaled_height = vis_dev->ScaleToScreen(unscaled_height);
	}

	// In order to fix CORE-20408, resize the window to only show as much of
	// the bottom and right borders that the borders become even. Hackish,
	// and might break alignment with the dropdown button by a pixels or
	// two, but this bug really does not seem to yield to an elegant fix.

	if (m_dropdown->GetVisualDevice()->GetScale() > 100)
	{
		// Desired border thickness
		INT32 left_bt = vis_dev->ScaleToScreen(left);
		INT32 top_bt  = vis_dev->ScaleToScreen(top);

		// The border thickness we would get on the right and bottom sides without this fix
		INT32 right_bt  = vis_dev->ScaleToScreen(unscaled_width) -
						  vis_dev->ScaleToScreen(unscaled_width - right);
		INT32 bottom_bt = vis_dev->ScaleToScreen(unscaled_height) -
						  vis_dev->ScaleToScreen(unscaled_height - bottom);

		// Compensate
		scaled_width  -= (right_bt  - left_bt);
		scaled_height -= (bottom_bt - top_bt);
	}

	return GetBestDropdownPosition(m_dropdown, scaled_width, scaled_height, FALSE);
}

UINT32 DropDownWindow::GetScreenHeight()
{
	VisualDevice* vis_dev = m_dropdown->GetVisualDevice();

	if(GetWidgetContainer()->GetWindow() != 0)
	{
		//in case where we dont have a document, but only a container
		//for example in gogi gui
		OpScreenProperties sp;
		g_op_screen_info->GetProperties(&sp, GetWidgetContainer()->GetWindow());
		return sp.height;
	} else {
		//in case we have a document
		//for example in webpage
		return vis_dev->GetScreenHeight();
	}
}

void DropDownWindow::UpdateMenu(BOOL items_changed)
{
	m_scale = m_dropdown->GetVisualDevice()->GetScale();
	GetWidgetContainer()->GetRoot()->GetVisualDevice()->SetScale(m_scale, FALSE);

	if (items_changed)
	{
		m_listbox->ih.DuplicateOf(m_dropdown->ih, FALSE, FALSE);
		m_listbox->UpdateScrollbar();
	}
	OpRect rect = CalculateRect();
	m_window->SetOuterPos(rect.x, rect.y);
	m_window->SetInnerSize(rect.width, rect.height);
	m_listbox->InvalidateAll();
}

void DropDownWindow::OnScroll(CoreView* view, INT32 dx, INT32 dy, SCROLL_REASON reason)
{
	UpdateMenu(FALSE);
}

void DropDownWindow::SetVisible(BOOL vis, BOOL default_size)
{
	if (default_size) {
		OpRect rect = CalculateRect();
		Show(vis, &rect);
	}
	else
		Show(vis);
}

void DropDownWindow::SelectItem(INT32 nr, BOOL selected)
{
	m_listbox->SelectItem(nr, selected);
}

INT32 DropDownWindow::GetFocusedItem()
{
	return m_listbox->ih.focused_item;
}

#ifndef MOUSELESS
void DropDownWindow::ResetMouseOverItem()
{
	m_listbox->ResetMouseOverItem();
}
#endif // MOUSELESS

BOOL DropDownWindow::HasHotTracking()
{
	return m_hot_tracking;
}

void DropDownWindow::SetHotTracking(BOOL hot_track)
{
	m_hot_tracking = hot_track;
}

BOOL DropDownWindow::IsListWidget(OpWidget* widget)
{
	return m_listbox == widget;
}

OpPoint DropDownWindow::GetMousePos()
{
	return m_listbox->GetMousePos();
}

#ifndef MOUSELESS
void DropDownWindow::OnMouseDown(OpPoint pt, MouseButton button, INT32 nclicks)
{
	m_listbox->OnMouseDown(pt, button, nclicks);
}

void DropDownWindow::OnMouseUp(OpPoint pt, MouseButton button, INT32 nclicks)
{
	m_listbox->OnMouseUp(pt, button, nclicks);
}

void DropDownWindow::OnMouseMove(OpPoint pt)
{
	m_listbox->OnMouseMove(pt);
}

BOOL DropDownWindow::OnMouseWheel(INT32 delta, BOOL vertical)
{
	return m_listbox->OnMouseWheel(delta, vertical);
}
#endif // MOUSELESS

void DropDownWindow::ScrollIfNeeded()
{
	m_listbox->ScrollIfNeeded();
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS DropDownWindow::GetAbsolutePositionOfList(OpRect &rect)
{
	return m_listbox->GetAbsolutePositionOfList(rect);
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

BOOL DropDownWindow::SendInputAction(OpInputAction* action)
{
	return m_listbox->OnInputAction(action);
}

void DropDownWindow::HighlightFocusedItem()
{
	m_listbox->HighlightFocusedItem();
}

