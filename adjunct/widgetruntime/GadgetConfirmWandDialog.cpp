/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetConfirmWandDialog.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/wand/wandmanager.h"

OP_STATUS GadgetConfirmWandDialog::Init(DesktopWindow* parent_window, WandInfo* info)
{
	m_info = info;
	Dialog::Init(parent_window);
	return OpStatus::OK;
}

void GadgetConfirmWandDialog::OnInit()
{
	
	OpString str;
		
	g_languageManager->GetString(Str::S_EDIT_WAND_CAPTION_STR, str);
	SetTitle(str.CStr());
								
	SetDefaultPushButton(2);
}

void GadgetConfirmWandDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	switch (index)
	{
		case 0:
			action = GetOkAction();
			g_languageManager->GetString(Str::D_WAND_SAVE, text);
			name.Set(WIDGET_NAME_BUTTON_OK);
			break;
				
		case 1:
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_NEVER));
			g_languageManager->GetString(Str::D_WAND_NEVER_SHORT, text);
			name.Set(WIDGET_NAME_BUTTON_NEVER);
			break;
				
		case 2:
			action = GetCancelAction();
			g_languageManager->GetString(Str::D_WAND_NOTNOW, text);
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
			break;
	}
}
	
UINT32 GadgetConfirmWandDialog::OnOk()
{
	if (!m_info)								// see bug 259732
		return 1;
				
	// No more dialog. Store directly
	m_info->ReportAction(WAND_STORE);
	
	return 1;
}
	
void GadgetConfirmWandDialog::OnCancel()
{
	if (m_info)								// see bug 259732
		m_info->ReportAction(WAND_DONT_STORE);
}
	
BOOL GadgetConfirmWandDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEVER:
			if(m_info)
			{
				m_info->ReportAction(WAND_NEVER_STORE_ON_THIS_PAGE);
				m_info = NULL;					// see bug 259732
			}
			CloseDialog(FALSE);
			return TRUE;
	}

	return Dialog::OnInputAction(action);
}

#endif // WIDGET_RUNTIME_SUPPORT
