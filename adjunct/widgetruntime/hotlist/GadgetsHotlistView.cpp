/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/hotlist/GadgetsHotlistView.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "modules/util/opfile/opfolder.h"

const char* const GadgetsHotlistView::GADGET_MENU_NAME = "Widget Item Popup Menu";

OP_STATUS GadgetsHotlistView::Init()
{
	m_sort_column = NAME_COLUMN;

	m_controller = OP_NEW(GadgetsHotlistController, (*m_hotlist_view));
	RETURN_OOM_IF_NULL(m_controller.get());

	RETURN_IF_ERROR(InitModel());

	m_hotlist_view->SetTreeModel(m_gadgets_model.get(), m_sort_column, TRUE);
	m_hotlist_view->SetPaintBackgroundLine(FALSE);
	m_hotlist_view->SetHaveWeakAssociatedText(TRUE);
	m_hotlist_view->SetRestrictImageSize(FALSE);
	m_hotlist_view->SetShowThreadImage(FALSE);
	m_hotlist_view->SetAllowMultiLineIcons(TRUE);
	m_hotlist_view->SetColumnImageFixedDrawArea(ICON_COLUMN, 
			GADGET_HOTLIST_ICON_SIZE + 2);
	m_hotlist_view->SetColumnFixedWidth(ICON_COLUMN, 
			GADGET_HOTLIST_ICON_SIZE + 2);
	m_hotlist_view->SetColumnFixedWidth(SEPARATOR_COLUMN,
			SEPARATOR_COLUMN_WIDTH);
	m_hotlist_view->SetExtraLineHeight(2);
	m_hotlist_view->SetVerticalAlign(WIDGET_V_ALIGN_TOP);
	m_hotlist_view->SetMultiselectable(FALSE);

	SetPrefsmanFlags();

	return OpHotlistTreeView::Init();
}


OP_STATUS GadgetsHotlistView::InitModel()
{
	m_gadgets_model.reset(OP_NEW(GadgetsTreeModel, ()));
	RETURN_OOM_IF_NULL(m_gadgets_model.get());
	RETURN_IF_ERROR(m_gadgets_model->Init());

	return OpStatus::OK;
}


OP_STATUS GadgetsHotlistView::DeleteModel()
{
	m_hotlist_view->SetTreeModel(NULL, m_sort_column, TRUE);
	
	m_gadgets_model.reset();

	return OpStatus::OK;
}


void GadgetsHotlistView::OnDeleted()
{
	// As strange as it is, we have to delete the model before g_gadget_manager
	// is destroyed, or we crash on deleting OpGadgetClass objects.
	OpStatus::Ignore(DeleteModel());

	OpHotlistTreeView::OnDeleted();
}


void GadgetsHotlistView::OnShow(BOOL show)
{
	m_gadgets_model->SetActive(show);

	OpHotlistTreeView::OnShow(show);
}


BOOL GadgetsHotlistView::ShowContextMenu(const OpPoint& point, BOOL center,
		OpTreeView* view, BOOL use_keyboard_context)
{
	BOOL shown = FALSE;

	if (NULL == view)
	{
	}
	else if (NULL != view->GetSelectedItem())
	{
		// Inside the context of an installed gadget.
		const OpPoint translated_point = point + GetScreenRect().TopLeft();
		g_application->GetMenuHandler()->ShowPopupMenu(GADGET_MENU_NAME,
				PopupPlacement::AnchorAt(translated_point, center), 0, use_keyboard_context);
		shown = TRUE;
	}
	// TODO: Inside the context of an "unupgraded" gadget.

	return shown;
}


INT32 GadgetsHotlistView::GetRootID()
{
	return -1;
}


void GadgetsHotlistView::OnSetDetailed(BOOL detailed)
{
	// This method body intentionally left blank.
}


INT32 GadgetsHotlistView::OnDropItem(HotlistModelItem* hmi_target,
		DesktopDragObject* drag_object, INT32 i, DropType drop_type,
		DesktopDragObject::InsertType insert_type, INT32 *first_id, BOOL force_delete)
{
	// This method body intentionally left blank.
	return 0;
}


BOOL GadgetsHotlistView::OnInputAction(OpInputAction* action)
{
	return m_controller->HandleAction(*action);
}


void GadgetsHotlistView::OnMouseEvent(OpWidget* widget, INT32 pos, INT32 x,
		INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	// Context menu triggering more or less copied from OpHotlistTreeView.
	if (!down && 1 == nclicks && MOUSE_BUTTON_2 == button)
	{
		if (NULL != widget && WIDGET_TYPE_TREEVIEW == widget->GetType())
		{
			// Needed for split view layout
			x += widget->GetRect(FALSE).x;
			y += widget->GetRect(FALSE).y;
			ShowContextMenu(OpPoint(x,y), FALSE,
					static_cast<OpTreeView*>(widget), FALSE);
		}
		return;
	}

	if (MOUSE_BUTTON_1 == button && 2 == nclicks)
	{
		OpInputAction action(OpInputAction::ACTION_OPEN_WIDGET);
		g_input_manager->InvokeAction(&action, GetParentInputContext());
	}
}


#endif // WIDGET_RUNTIME_SUPPORT
