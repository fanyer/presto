/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"

#include "adjunct/desktop_scope/src/scope_widget_info.h"
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"
#include "modules/widgets/OpEdit.h"


OP_STATUS
DesktopOpDropDown::Construct(DesktopOpDropDown** obj, BOOL editable_text)
{
	*obj = OP_NEW(DesktopOpDropDown, (editable_text));
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


DesktopOpDropDown::DesktopOpDropDown(BOOL editable_text)
	: OpDropDown(editable_text), m_item_added_handler(NULL)
{
}

DesktopOpDropDown::~DesktopOpDropDown()
{
	OP_DELETE(m_item_added_handler);
}

OP_STATUS
DesktopOpDropDown::AddItem(const OpStringC & text, INT32 index, INT32 * got_index, INTPTR user_data, OpInputAction * action, const OpStringC8 & widget_image)
{
	INT32 got_index_internal = -1;
	RETURN_IF_ERROR(AddItem(text.CStr(), index, &got_index_internal, user_data));

	if (widget_image.HasContent())
	{
		ih.SetImage(got_index_internal, widget_image.CStr(), this);
	}
	if (got_index)
	{
		*got_index = got_index_internal;
	}

	if (action && got_index_internal != -1)
	{
		const OP_STATUS result = m_item_actions.Add(got_index_internal, action);
		if (OpStatus::IsError(result))
		{
			RemoveItem(got_index_internal);
			return result;
		}
	}

	return OpStatus::OK;
}

/*virtual*/ OP_STATUS
DesktopOpDropDown::AddItem(const uni_char* txt, INT32 index, INT32 *got_index, INTPTR user_data)
{
	VisualDeviceHandler vis_dev_handler(this);

	RETURN_IF_ERROR(OpDropDown::AddItem(txt, index, got_index, user_data));

	if (m_item_added_handler)
	{
		(*m_item_added_handler)(got_index ? *got_index : -1);
	}

	return OpStatus::OK;
}

void
DesktopOpDropDown::SetItemAddedHandler(OpDelegateBase<INT32>* handler)
{
	OP_DELETE(m_item_added_handler);
	m_item_added_handler = handler;
}

/*virtual*/ void
DesktopOpDropDown::InvokeAction(INT32 index)
{
	OpInputAction * action;
	OpStatus::Ignore(m_item_actions.GetData(index, &action));
	if (action)
	{
		g_input_manager->InvokeAction(action, GetParentInputContext());
	}
	else
	{
		OpDropDown::InvokeAction(index);
	}
}

void DesktopOpDropDown::OnUpdateActionState()
{
	if (GetAction())
		return OpDropDown::OnUpdateActionState();

	if (m_item_actions.GetCount() == 0)
		return;

	BOOL enabled = FALSE;	
	for (int i = 0; i < CountItems(); i++)
	{
		OpInputAction* action = NULL;
		OpStatus::Ignore(m_item_actions.GetData(i, &action));
		
		if (!action)
			continue;

		INT32 state = g_input_manager->GetActionState(action, GetParentInputContext());

		//Update the selection to match the action state, but not when the user is
		//selecting a new item in the dropdown window
		if (m_dropdown_window == NULL && state & OpInputAction::STATE_SELECTED)
		{
			SelectItem(i, TRUE);
		}

		if (!(state & OpInputAction::STATE_DISABLED) || state & OpInputAction::STATE_ENABLED || IsCustomizing())
			enabled = TRUE;
	}

	SetEnabled(enabled);
}
void DesktopOpDropDown::SetEditableText()
{
	 OpDropDown::SetEditableText(TRUE); 
	 edit->UpdateSkinPadding(); 
}

class DesktopOpDropDown::DropdownWidgetInfo : public OpScopeWidgetInfo
{
public:
	DropdownWidgetInfo(DesktopOpDropDown& dropdown) : m_dropdown(dropdown) {}
	virtual OP_STATUS AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible);
private:
	DesktopOpDropDown& m_dropdown;
};

OpScopeWidgetInfo* DesktopOpDropDown::CreateScopeWidgetInfo()
{
	return OP_NEW(DropdownWidgetInfo, (*this));
}

OP_STATUS DesktopOpDropDown::DropdownWidgetInfo::AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible)
{
	for (INT32 i = 0; i < m_dropdown.ih.CountItems(); i++)
	{
		OpStringItem* item = m_dropdown.ih.GetItemAtIndex(i);
		if (!item)
			continue;

		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickWidgetInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickWidgetInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR;

		// Set the info
		info.get()->SetType(OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QUICKWIDGETTYPE_DROPDOWNITEM);
		info.get()->SetText(m_dropdown.GetItemText(i));
		info.get()->SetVisible(m_dropdown.IsVisible() && (m_dropdown.IsDroppedDown() || item->IsSelected())); 

		// Do we need group or isSeparator property too?
		info.get()->SetEnabled(item->IsEnabled());
		
		// Set value based on Selected value 
		info.get()->SetValue(item->IsSelected());
		
		// Set the dropdown as the parent
		if (m_dropdown.GetName().HasContent())
		{
			OpString name;
			name.Set(m_dropdown.GetName());
			info.get()->SetParent(name);
		}
		
		OpRect rect;
		if (OpStatus::IsSuccess(m_dropdown.ih.GetAbsolutePositionOfItem(item, rect)))
		{
			info.get()->GetRectRef().SetX(rect.x);
			info.get()->GetRectRef().SetY(rect.y);
			info.get()->GetRectRef().SetWidth(rect.width);
			info.get()->GetRectRef().SetHeight(rect.height);
		}
		
		// Set the row and column
		info.get()->SetRow(i);
		info.get()->SetCol(0);
		
		RETURN_IF_ERROR(list.GetQuickwidgetListRef().Add(info.get()));
		info.release();

	}
	return OpStatus::OK;
}
