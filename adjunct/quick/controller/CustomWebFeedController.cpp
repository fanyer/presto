/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/CustomWebFeedController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "modules/webfeeds/webfeeds_api.h"
#include "modules/widgets/OpListBox.h"

void CustomWebFeedController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Custom Web Feed Dialog"));

	m_widgets = m_dialog->GetWidgetCollection();

	unsigned reader_count = g_webfeeds_api->GetExternalReaderCount();
	for (unsigned index = 0; index<reader_count; index++)
	{
		unsigned id = g_webfeeds_api->GetExternalReaderIdByIndex(index);

		BOOL can_edit;
		if (OpStatus::IsSuccess(g_webfeeds_api->GetExternalReaderCanEdit(id, can_edit)) && can_edit)
		{
			ANCHORD(OpString, name);
			LEAVE_IF_ERROR(g_webfeeds_api->GetExternalReaderName(id, name));

			OpStackAutoPtr<Item> item(OP_NEW_L(Item, (id)));
			item->m_name.SetL(name);
			LEAVE_IF_ERROR(m_list.Add(item.get()));
			item.release();
		}
	}

	OpListBox* listbox = m_widgets->GetL<QuickListBox>("listbox_feed_list")->GetOpWidget();

	// For now we have to give the listbox a visual device, otherwise the list strings will
	// not get a width or height which breaks layout
	if (!listbox->GetVisualDevice())
		m_visdev_handler = OP_NEW_L(VisualDeviceHandler,(listbox));
	else
		OP_ASSERT("!Wrapper code for visual device can be removed");

	for (UINT32 i=0; i<m_list.GetCount(); i++)
		LEAVE_IF_ERROR(listbox->AddItem(m_list.Get(i)->m_name, -1, 0, reinterpret_cast<INTPTR>(m_list.Get(i))));

	listbox->SelectItem(0, TRUE);
}

BOOL CustomWebFeedController:: DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		{
			OpListBox* listbox = m_widgets->Get<QuickListBox>("listbox_feed_list")->GetOpWidget();
			return listbox->GetSelectedItem() == -1;
		}
	}

	return OkCancelDialogContext::DisablesAction(action);
}


BOOL CustomWebFeedController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
			return TRUE;
	}

	return OkCancelDialogContext::CanHandleAction(action);
}


OP_STATUS CustomWebFeedController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		{
			OpListBox* listbox = m_widgets->Get<QuickListBox>("listbox_feed_list")->GetOpWidget();
			INT32 index = listbox->GetSelectedItem();
			if (index >= 0 && index < listbox->CountItems())
			{
				Item* item = (Item*)listbox->GetItemUserData(index);
				if (item)
				{
					item->m_deleted = TRUE;
					listbox->RemoveItem(index);
					if (index >= listbox->CountItems())
						index = listbox->CountItems()-1;
					if (index >= 0)
						listbox->SelectItem(index, TRUE);
					listbox->InvalidateAll();
				}
			}
		}
		return OpStatus::OK;
	}
	return OkCancelDialogContext::HandleAction(action);
}


void CustomWebFeedController::OnOk()
{
	for (UINT32 i=0; i<m_list.GetCount(); i++)
	{
		Item* item = m_list.Get(i);
		if (item->m_deleted)
			g_webfeeds_api->DeleteExternalReader(item->m_id);
	}

	OkCancelDialogContext::OnOk();
}
