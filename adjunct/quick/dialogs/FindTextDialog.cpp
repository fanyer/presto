/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"
#include "FindTextDialog.h"
#include "adjunct/quick/managers/FindTextManager.h"

FindTextDialog::FindTextDialog(const FindTextManager* manager)
{
	m_manager = const_cast<FindTextManager*>(manager);
}


void FindTextDialog::OnInit()
{
	OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName("Find_what_edit"));
	if (edit)
	{
		edit->GetForegroundSkin()->SetImage("Search Web");
		edit->SetText(m_manager->GetSearchText().CStr());
	}

	OpCheckBox* check = static_cast<OpCheckBox*>(GetWidgetByName("Match_whole_word_only_checkbox"));
	if (check)
	{
		check->SetValue((INT32)m_manager->GetWholeWordFlag());
	}

	check = static_cast<OpCheckBox*>(GetWidgetByName("Match_case_checkbox"));
	if (check)
	{
		check->SetValue((INT32)m_manager->GetMatchCaseFlag());
	}

	BOOL forward = m_manager->GetForwardFlag();

	OpRadioButton* radio = static_cast<OpRadioButton*>(GetWidgetByName("Search_up_radio"));
	if (radio)
	{
		radio->SetValue(!forward);
	}

	radio = static_cast<OpRadioButton*>(GetWidgetByName("Search_down_radio"));
	if (radio)
	{
		radio->SetValue(forward);
	}

	OpButton* find_button = static_cast<OpButton*>(GetWidgetByName("Find_next_button"));
	if (find_button)
	{
		find_button->SetDefaultLook();
	}
}


void FindTextDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Find_next_button"))
	{
		BOOL whole_word = FALSE;
		BOOL match_case = FALSE;
		BOOL forward = TRUE;

		OpCheckBox* check = static_cast<OpCheckBox*>(GetWidgetByName("Match_whole_word_only_checkbox"));
		if (check)
		{
			whole_word = (BOOL)check->GetValue();
		}

		check = static_cast<OpCheckBox*>(GetWidgetByName("Match_case_checkbox"));
		if (check)
		{
			match_case = (BOOL)check->GetValue();
		}

		OpRadioButton* radio = static_cast<OpRadioButton*>(GetWidgetByName("Search_up_radio"));
		if (radio)
		{
			forward = !(BOOL)radio->GetValue();
		}

		m_manager->SetFlags(forward, match_case, whole_word);

		OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName("Find_what_edit"));
		if (edit)
		{
			int text_len = edit->GetTextLength();
			if (text_len > 0)
			{
				uni_char* text = OP_NEWA(uni_char, text_len + 1);
				if (NULL == text)
				{
					return;
				}

				edit->GetText(text, text_len, 0);
				m_manager->SetSearchText(text);

				OP_DELETEA(text);

				m_manager->Search();
			}
			else
			{
				m_manager->SetSearchText(UNI_L(""));
			}
		}
	}
}


void FindTextDialog::OnClose(BOOL user_initiated)
{
	m_manager->OnDialogKilled();
	Dialog::OnClose(user_initiated);
}
