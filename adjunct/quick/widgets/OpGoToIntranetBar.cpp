/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/OpGoToIntranetBar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"
#include "modules/history/direct_history.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"

#define GO_TO_PAGE_ID 1

// FIX: Should add generic toolbar type (or use OpInfobar?) that can automatically close when url is changed, by timeout etc.

DEFINE_CONSTRUCT(OpGoToIntranetBar)

OpGoToIntranetBar::OpGoToIntranetBar()
	: m_label(NULL)
	, m_show_time(0)
	, m_hide_when_url_change(FALSE)
	, m_save_typed_history(TRUE)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
}

OpGoToIntranetBar::~OpGoToIntranetBar()
{
}

void OpGoToIntranetBar::OnReadContent(PrefsSection *section)
{
	OpToolbar::OnReadContent(section);

	OpInputAction action(OpInputAction::ACTION_GO_TO_PAGE);
	if (OpButton *button = (OpButton*) GetWidgetByTypeAndIdAndAction(OpTypedObject::WIDGET_TYPE_BUTTON, 0, &action, FALSE))
	{
		button->SetID(GO_TO_PAGE_ID);
		button->SetButtonStyle(OpButton::STYLE_TEXT);
		button->SetButtonType(OpButton::TYPE_TOOLBAR);
		button->GetBorderSkin()->SetImage("Infobar Button Skin");
	}
}

void OpGoToIntranetBar::Show(const uni_char *address, BOOL save_typed_history)
{
	m_hide_when_url_change = FALSE;
	m_save_typed_history = save_typed_history;
	m_show_time = g_op_time_info->GetRuntimeMS();
	m_address.Set(address);

	OpString http_address;
	http_address.Append(UNI_L("http://"));
	http_address.Append(address);

	OpString opera_action_str;
	opera_action_str.Append(UNI_L("opera:/action/Go to page, &quot;"));
	opera_action_str.Append(http_address);
	opera_action_str.Append(UNI_L("&quot;"));

	OpString str;
	g_languageManager->GetString(Str::S_ADDRESS_FIELD_GO_TO_INTRANET_RICH, str);
	OpString info;
	info.AppendFormat(str.CStr(), opera_action_str.CStr(), http_address.CStr());

	if (OpRichTextLabel *label = (OpRichTextLabel*) GetWidgetByType(OpTypedObject::WIDGET_TYPE_RICHTEXT_LABEL, FALSE))
	{
		RETURN_VOID_IF_ERROR(label->SetText(info.CStr()));
	}

	
	if (OpButton *button = (OpButton*) GetWidgetById(GO_TO_PAGE_ID))
	{
		if (button->GetAction())
			button->GetAction()->SetActionDataString(http_address.CStr());
	}

	OpInfobar::Show();
}

void OpGoToIntranetBar::Hide(BOOL focus_page)
{
	if (GetAlignment() == ALIGNMENT_OFF)
		return;

	OpInfobar::Hide();

	if (focus_page)
		g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
}

void OpGoToIntranetBar::OnUrlChanged()
{
	// We want to hide it if the user go to a new page.
	// But the url will probably change at least once since
	// we showed the bar because we load the search in the background.
	if (m_hide_when_url_change && g_op_time_info->GetRuntimeMS() > m_show_time + 3000)
		Hide(FALSE);
	m_hide_when_url_change = TRUE;
}

void OpGoToIntranetBar::OnAlignmentChanged()
{
	if (GetAlignment() == ALIGNMENT_OFF)
	{
	}
}

BOOL OpGoToIntranetBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GO_TO_PAGE:
		{
			if (m_save_typed_history)
			{
				// Add the address to the typed history, so it will be quick completed next time it's typed.
				OpString intranet_address;
				intranet_address.Append(m_address.CStr());
				intranet_address.Append("/");
				if (OpStatus::IsSuccess(directHistory->Add(intranet_address.CStr(), DirectHistory::TEXT_TYPE, g_timecache->CurrentTime())))
					directHistory->Delete(m_address.CStr());

				g_searchEngineManager->AddDetectedIntranetHost(m_address);
			}
			action->SetActionData(1); // Pretend action is from the address bar itself
			// Fall through...
		}
		case OpInputAction::ACTION_CANCEL:
			Hide();
			break;
	};
	return OpToolbar::OnInputAction(action);
}
