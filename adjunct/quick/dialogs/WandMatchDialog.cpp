/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
 
 													 
/***********************************************************************************
**
**	WandMatchDialog
**
***********************************************************************************/
#include "core/pch.h"
#include "adjunct/quick/dialogs/WandMatchDialog.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/wand/wandmanager.h"
#include "modules/pi/OpWindow.h"
#include "modules/widgets/OpListBox.h"
#include "modules/dochand/win.h"

WandMatchDialog::~WandMatchDialog()
{
	// m_info will call delete on it self
	if (m_info)
	{
		m_info->Select(GetWidgetValue("Match_list"));
		m_info = NULL;
	}
}

OP_STATUS WandMatchDialog::Init(DesktopWindow* parent_window, WandMatchInfo* info)
{
	OP_STATUS initiated = OpStatus::OK;
	m_info = info;
	
	if (parent_window->GetType() == OpTypedObject::WINDOW_TYPE_GADGET)
	{
		initiated = Dialog::Init(parent_window);
	}
	else
	{
	 //	OpWindow *window = info->GetOpWindowCommander()->GetWindow()->GetOpWindow();
		//OpPageView *pw = ((DesktopOpWindow*)window)->GetPageView();
		OpWindowCommander* win_comm = info->GetOpWindowCommander();
		OpWindow *window = win_comm->GetOpWindow();
		OpBrowserView* pw = ((DesktopOpWindow*)window )->GetBrowserView();

		if (!pw)
		{
			 return OpStatus::ERR_NULL_POINTER;
		}	 
		parent_window = pw->GetParentDesktopWindow();
		initiated = Dialog::Init(parent_window, 0, 0, pw);
	}
		
	

	OpString wand;
	g_languageManager->GetString(Str::S_EDIT_WAND_CAPTION_STR, wand);

	SetTitle(wand.CStr());
	UpdateList();
	return initiated;
}

void WandMatchDialog::OnCancel()
{
	if (m_info)
	{
		m_info->Cancel();
		m_info = NULL;
	}
}


BOOL WandMatchDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		if (m_info)
		{
			int index = GetWidgetValue("Match_list");
			OpListBox* listbox = (OpListBox*) GetWidgetByName("Match_list");

			m_info->Delete(index);
			{
				listbox->RemoveItem(index);
				listbox->Invalidate(listbox->GetBounds());
			}

			if (m_info->GetMatchCount() == 0)
			{
				m_info->Cancel();
				m_info = NULL;
				CloseDialog(FALSE);
			}
			return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}

void WandMatchDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (nclicks == 2 && widget == GetWidgetByName("Match_list"))
	{
		CloseDialog(FALSE, FALSE, TRUE);
	}
}
void WandMatchDialog::UpdateList()
{
	OpListBox* listbox = (OpListBox*) GetWidgetByName("Match_list");
	while(listbox->CountItems())
		listbox->RemoveItem(0);
	for(int i = 0; i < m_info->GetMatchCount(); i++)
		listbox->AddItem(m_info->GetMatchString(i));
	listbox->SelectItem(0, TRUE);
}



