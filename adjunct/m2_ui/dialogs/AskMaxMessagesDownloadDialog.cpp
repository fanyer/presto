/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "AskMaxMessagesDownloadDialog.h"
#include "adjunct/quick/Application.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

AskMaxMessagesDownloadDialog::AskMaxMessagesDownloadDialog(int& current_max_messages, BOOL& dont_ask_again, int available_messages, const OpStringC8& group, BOOL& ok)
  : m_max_messages(current_max_messages)
  , m_available_messages(available_messages)
  , m_ask_again(dont_ask_again)
  , m_ok(ok)
{
	OpStatus::Ignore(m_group.Set(group));
}

void AskMaxMessagesDownloadDialog::OnInit()
{
	OpString format, question, max_messages;

	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_NEWS_HOW_MANY_NEW_MESSAGES, format));

	question.AppendFormat(format.CStr(), m_available_messages, m_group.CStr());

	OpMultilineEdit* question_edit = (OpMultilineEdit*)GetWidgetByName("Question");
	if (question_edit)
	{
		question_edit->SetLabelMode();
		question_edit->SetText(question.CStr());
	}

	OpNumberEdit* limited_messages = (OpNumberEdit*)GetWidgetByName("NumberEdit_messages");
	if (limited_messages)
	{
		if (m_max_messages != 0)
		{
			max_messages.AppendFormat(UNI_L("%d"), min(m_max_messages, m_available_messages));
			limited_messages->SetText(max_messages.CStr());
		}
		else
		{
			limited_messages->SetText(UNI_L("250"));
		}
		limited_messages->SetMinValue(0);
		limited_messages->SetMaxValue(m_available_messages);
	}

	SetWidgetValue("Radio_limited_messages", m_max_messages != 0);
	SetWidgetValue("Radio_all_messages", m_max_messages == 0);
}

UINT32 AskMaxMessagesDownloadDialog::OnOk()
{
	OpNumberEdit* limited_messages = (OpNumberEdit*)GetWidgetByName("NumberEdit_messages");
	if (limited_messages)
		limited_messages->GetIntValue(m_max_messages);
	if (m_max_messages < 0)
		m_max_messages = 0;

	BOOL fetch_all = GetWidgetValue("Radio_all_messages");
	if (fetch_all)
		m_max_messages = 0;

	m_ask_again = !GetWidgetValue("Dont_ask_checkbox");

	m_ok = TRUE;

	return 0;
}

void AskMaxMessagesDownloadDialog::OnCancel()
{
	m_ok = FALSE;
}

#endif // M2_SUPPORT
