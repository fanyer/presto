/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpComposeEdit.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/pi/OpDragManager.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/widgets/OpEdit.h"

/***********************************************************************************
 **
 **	OpComposeEdit
 **
 ***********************************************************************************/

DEFINE_CONSTRUCT(OpComposeEdit)

OP_STATUS OpComposeEdit::Construct(OpComposeEdit** obj, const uni_char* menu)
{
	if (!(*obj = OP_NEW(OpComposeEdit, (menu))))
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;
}

void OpComposeEdit::SetDropdown(const uni_char* menu)
{
	m_dropdown_menu = menu;
}

BOOL OpComposeEdit::OnInputAction(OpInputAction* action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_EDIT_DROPDOWN:
		{
			OpInputAction new_action(OpInputAction::ACTION_SHOW_POPUP_MENU);
			new_action.SetActionDataString(m_dropdown_menu);
			new_action.SetActionMethod(action->GetActionMethod());

			return g_input_manager->InvokeAction(&new_action, this);
		}
	}

	return OpEdit::OnInputAction(action);
}

void OpComposeEdit::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	if(drag_object->GetSource() && drag_object->GetSource()->GetType() == WIDGET_TYPE_COMPOSE_EDIT)
	{
		OpEdit* edit = static_cast<OpEdit*>(drag_object->GetSource());
		if (edit && edit->IsEnabled())
		{
			OpInputAction action(OpInputAction::ACTION_DELETE);
			edit->EditAction(&action);
			edit->SelectNothing();
		}
	}
	OpString tmp;
	tmp.Set(DragDrop_Data_Utils::GetText(drag_object));
	const uni_char* text = tmp.CStr();
	while (text && (text[0] == ',' || text[0] == ';' || text[0] == ' '))
		++text;
	DragDrop_Data_Utils::SetText(drag_object, text);

    if (DragDrop_Data_Utils::GetText(drag_object) && GetTextLength() > 0)
	{
		OpString text;

		GetText(text);
		const uni_char* tmp = text.CStr() + text.Length() - 1;
		if (text.CStr() && *tmp != ',' && *tmp != ';' && *(tmp - 1) != ',' && *(tmp - 1) != ';')
			text.Append(UNI_L(", "));
		text.Append(DragDrop_Data_Utils::GetText(drag_object));

		SetText(text.CStr());

		SetFocus(FOCUS_REASON_OTHER);
	}
	else
	{
		OpEdit::OnDragDrop(drag_object, point);
	}
}
