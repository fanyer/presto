/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/jsdialogs.h"

const uni_char* FormatJSDialogTitle(DesktopWindow* parent_window)
{
#ifdef EMBROWSER_SUPPORT
	if (g_application->IsEmBrowser() && parent_window->GetTitle())
		return (uni_char*)parent_window->GetTitle();
#endif // EMBROWSER_SUPPORT
	return UNI_L("JavaScript");
}

/***********************************************************************************
**
**	JSAlertDialog
**
***********************************************************************************/

void JSAlertDialog::Init(DesktopWindow* parent_window,	const uni_char* message, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view)
{
	m_callback = callback;
	m_window_commander = commander;

	// If we have kiosk mode on then don't allow the disabling of scripts on the page
	if (KioskManager::GetInstance()->GetEnabled())
		m_show_disable_checkbox = FALSE;

	SimpleDialog::Init(WINDOW_NAME_JS_ALERT, FormatJSDialogTitle(parent_window), message, parent_window, TYPE_OK, IMAGE_INFO, 0, 0, 0, 0, 0, parent_browser_view);
	FocusButton(0); // focus OK button
}

void JSAlertDialog::OnClose(BOOL user_initiated)
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_callback->AbortScript();
		m_window_commander->SetScriptingDisabled(TRUE);
	}

	m_callback->OnAlertDismissed();

	SimpleDialog::OnClose(user_initiated);
}

/***********************************************************************************
**
**	JSConfirmDialog
**
***********************************************************************************/

void JSConfirmDialog::Init(DesktopWindow* parent_window, const uni_char* message, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view)
{
	m_callback = callback;
	m_window_commander = commander;

	// If we have kiosk mode on then don't allow the disabling of scripts on the page
	if (KioskManager::GetInstance()->GetEnabled())
		m_show_disable_checkbox = FALSE;

	SimpleDialog::Init(WINDOW_NAME_JS_CONFIRM, FormatJSDialogTitle(parent_window), message, parent_window, TYPE_OK_CANCEL, IMAGE_QUESTION, 0, 0, 0, 0, 0, parent_browser_view);
	FocusButton(0); // focus OK button
}

UINT32 JSConfirmDialog::OnOk()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_callback->AbortScript();
		m_window_commander->SetScriptingDisabled(TRUE);
	}

	m_callback->OnConfirmDismissed(TRUE);
	return 1;
}

void JSConfirmDialog::OnCancel()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_callback->AbortScript();
		m_window_commander->SetScriptingDisabled(TRUE);
	}

	m_callback->OnConfirmDismissed(FALSE);
}

/***********************************************************************************
**
**	JSPromptDialog
**
***********************************************************************************/

void JSPromptDialog::Init(DesktopWindow* parent_window, const uni_char* message, const uni_char* default_value, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view)
{
	m_message = message;
	m_default_value = default_value;
	m_callback = callback;
	m_window_commander = commander;

	// If we have kiosk mode on then don't allow the disabling of scripts on the page
	if (KioskManager::GetInstance()->GetEnabled())
		m_show_disable_checkbox = FALSE;

	Dialog::Init(parent_window, 0, 0, parent_browser_view);
}

void JSPromptDialog::OnInit()
{
	OpMultilineEdit* label = (OpMultilineEdit*)GetWidgetByName("Description_label");

	if(label)
	{
		label->SetLabelMode();
		label->SetText(m_message);
	}

	if (m_default_value)
	{
		SetWidgetText("Text_edit", m_default_value);
	}

	SetTitle(FormatJSDialogTitle(GetParentDesktopWindow()));
}

void JSPromptDialog::OnReset()
{
	OpMultilineEdit* edit = (OpMultilineEdit*) GetWidgetByName("Description_label");

	int move_y = 0;

	if (edit)
	{
		INT32 left, top, right, bottom;
		edit->GetPadding(&left, &top, &right, &bottom);
		int width = edit->GetMaxBlockWidth() + left + right;
		int height = edit->GetContentHeight() + top + bottom;

		move_y = min (height - edit->GetHeight(), 300);

		edit->SetSize(width, height);
	}

	OpWidget* input = GetWidgetByName("Text_edit");

	if (input)
	{
		OpRect rect = input->GetRect();
		rect.y += move_y;
		input->SetRect(rect, FALSE, FALSE);
	}
}

UINT32 JSPromptDialog::OnOk()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_callback->AbortScript();
		m_window_commander->SetScriptingDisabled(TRUE);
	}

	OpString value;
	GetWidgetText("Text_edit", value);
	m_callback->OnPromptDismissed(TRUE, value.CStr());
	return 1;
}

void JSPromptDialog::OnCancel()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_callback->AbortScript();
		m_window_commander->SetScriptingDisabled(TRUE);
	}

	m_callback->OnPromptDismissed(FALSE, NULL);
}

