/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/AddActionButtonController.h"

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

#include "modules/locale/oplanguagemanager.h"

AddActionButtonController::AddActionButtonController(OpToolbar * toolbar, BOOL is_button, OpInputAction* input_action, int pos, BOOL force)
	: SimpleDialogController(TYPE_OK_CANCEL,IMAGE_QUESTION,"Add New Button Dialog"),
	  m_pos(pos),
	  m_force(force),
	  m_toolbar(toolbar),
	  m_input_action(input_action),
	  m_is_button(is_button)
{}

OP_STATUS AddActionButtonController::SetData(const uni_char* action_url, const uni_char* action_title)
{
	RETURN_IF_ERROR(m_actionurl.Set(action_url));
	RETURN_IF_ERROR(m_actiontitle.Set(action_title));
	return OpStatus::OK;
}

void AddActionButtonController::InitL()
{
	SimpleDialogController::InitL();

	ANCHORD(OpString, title);
	g_languageManager->GetStringL(Str::S_ADD_NEW_BUTTON_CAPTION, title);
	LEAVE_IF_ERROR(SetTitle(title));

	ANCHORD(OpString, format);
	ANCHORD(OpString, message);

	if (m_actionurl.FindI(UNI_L("Execute program")) != -1)
	{
		LEAVE_IF_ERROR(SetIcon(IMAGE_WARNING));

		g_languageManager->GetStringL(Str::S_ADD_NEW_BUTTON_WITH_EXECUTE_ACTION, format);
	}
	else
		g_languageManager->GetStringL(Str::S_ADD_NEW_BUTTON_WITH_ACTION, format);

	LEAVE_IF_ERROR(message.AppendFormat(format.CStr(), m_actionurl.CStr()));
	LEAVE_IF_ERROR(SetMessage(message));
}

void AddActionButtonController::OnOk()
{
	m_toolbar->AddButtonFromURL2(m_is_button, m_input_action, m_actionurl.CStr(), m_actiontitle.CStr(), m_pos, m_force);
}
