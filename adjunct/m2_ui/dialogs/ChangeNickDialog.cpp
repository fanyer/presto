/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)
//# include "modules/prefs/storage/pfmap.h"

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "ChangeNickDialog.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/locale/oplanguagemanager.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/m2/src/engine/engine.h"

/***********************************************************************************
**
**	ChangeNickDialog
**
***********************************************************************************/

ChangeNickDialog::ChangeNickDialog(ChangeType change_type, UINT16 account_id, const OpStringC& old_nick)
:	m_change_type(change_type),
	m_account_id(account_id)
{
	m_old_nick.Set(old_nick);
}

void ChangeNickDialog::OnInit()
{
	if (m_change_type == CHANGE_NICK_IN_USE)
	{
		OpString formatstring;
		g_languageManager->GetString(Str::S_NICK_TAKEN, formatstring);
		OpString label;
		label.AppendFormat(formatstring.CStr(), m_old_nick.CStr());
		SetWidgetText("label_for_Nick_edit", label.CStr());
	}

	SetWidgetText("Nick_edit", m_old_nick.CStr());
}

UINT32 ChangeNickDialog::OnOk()
{
	OpString nick;
	GetWidgetText("Nick_edit", nick);

	g_m2_engine->ReportChangeNickDialogResult(m_account_id,nick);
	return 0;
}

void ChangeNickDialog::OnCancel()
{
	//abort
}

/***********************************************************************************
**
**	RoomPasswordDialog
**
***********************************************************************************/

RoomPasswordDialog::RoomPasswordDialog(UINT16 account_id, const OpStringC& room)
:	m_account_id(account_id)
{
	m_room.Set(room);
}

void RoomPasswordDialog::OnInit()
{
	OpWidget* edit = GetWidgetByName("Password_edit");

    if (edit)
    {
      ((OpEdit*)edit)->SetPasswordMode(TRUE);
    }

	OpString formatstring;
	g_languageManager->GetString(Str::S_IRC_ROOM_PROTECTED, formatstring);
	OpString label;
	label.AppendFormat(formatstring.CStr(), m_room.CStr());

	SetWidgetText("label_for_Password_edit", label.CStr());
}

UINT32 RoomPasswordDialog::OnOk()
{
	OpString password;

	GetWidgetText("Password_edit", password);

	g_m2_engine->ReportRoomPasswordResult(m_account_id, m_room, password);

	return 0;
}

void RoomPasswordDialog::OnCancel()
{
	ChatDesktopWindow* window = ChatDesktopWindow::FindChatRoom(m_account_id, m_room, TRUE);

	if (window)
	{
		window->Close();
	}
}

#endif //M2_SUPPORT && IRC_SUPPORT
