/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 *
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/OpNewPageButton.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/dragdrop/dragdrop_manager.h"

DEFINE_CONSTRUCT(OpNewPageButton)

/***********************************************************************************
 ** Overridden version of OnContextMenu
 **
 **	OpNewPageButton::OnContextMenu
 **
 ***********************************************************************************/
BOOL OpNewPageButton::OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	g_application->GetMenuHandler()->ShowPopupMenu("Pagebar Plus Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return TRUE;
}

OpNewPageButton::OpNewPageButton()
{
	OpString title;
	g_languageManager->GetString(Str::SI_NEW_PAGE_BUTTON_TEXT, title);
	SetText(title.CStr());

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_NEW_PAGE));
	if (!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	action->SetActionData(1);
	OpInputAction* action2 = OP_NEW(OpInputAction, (OpInputAction::ACTION_NEW_PRIVATE_PAGE));
	if (!action2)
	{
		OP_DELETE(action);
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	action2->SetActionData(1);

	action->SetNextInputAction(action2);
	action->SetActionOperator(OpInputAction::OPERATOR_OR);

	SetAction(action);
}

void OpNewPageButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}

/***********************************************************************************
**  OpStateButton::OnDragStart
************************************************************************************/
void OpNewPageButton::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_NEW_PAGE_BUTTON, point.x, point.y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}
