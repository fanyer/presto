/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "GoToPageDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/history/direct_history.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

// include the templates

/***********************************************************************************
**
**  GoToPageDialog
**
***********************************************************************************/

GoToPageDialog::GoToPageDialog(BOOL nick) 
	: m_is_nick(nick)
{
}

/***********************************************************************************
**
**  OnInit
**
***********************************************************************************/

void GoToPageDialog::OnInit()
{ 
	OpAddressDropDown* dropdown = (OpAddressDropDown*) GetWidgetByName("Address_field");
	OpEdit* nickedit = (OpEdit*) GetWidgetByName("Nick_field");
	if( dropdown && nickedit)
	{
		if(m_is_nick)
		{
			SetWidgetText("label_for_Address_field", Str::DI_IDM_NICKNAME_LABEL );

			dropdown->SetVisibility(FALSE);
	        nickedit->SetFocus(FOCUS_REASON_OTHER);

			// Catch Enter press in edit field.
			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO));
			nickedit->SetAction(action);
		}		
		else
		{
			nickedit->SetVisibility(FALSE);
			dropdown->SetInvokeActionOnClick(TRUE);
			dropdown->SetMaxNumberOfColumns(2);
			dropdown->SetTabMode(FALSE);
			dropdown->SetEnableDropdown(SupportsExternalCompositing());
			dropdown->SetText(directHistory->GetFirst());
		}
	}
}

/***********************************************************************************
**
**  OnOk
**
***********************************************************************************/

UINT32 GoToPageDialog::OnOk()
{
	OpString url;
	GetWidgetText("Address_field", url);

	GoToPage(url, 0);

	return 0;
}

/***********************************************************************************
**
**  OnInputAction
**
***********************************************************************************/

BOOL GoToPageDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GO_TO_PAGE:
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
		{
			OpString url;
			GetWidgetText("Address_field", url);

			CloseDialog(FALSE);

			GoToPage(url, action);

			return TRUE;
			break;
		}
		case OpInputAction::ACTION_GO:
		{
			OpEdit* widget = (OpEdit*) GetWidgetByName("Nick_field");
			if(widget)
			{
				OpString nick;
				GetWidgetText("Nick_field", nick);

				if(g_hotlist_manager->HasNickname(nick, NULL, TRUE, TRUE))
				{
					CloseDialog(FALSE);
					OpenNickname(nick,TRUE);
				}
			}
			return TRUE;
			break;
		}

		case OpInputAction::ACTION_PASTE_AND_GO:
		case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
		{
			OpString text;
			g_desktop_clipboard_manager->GetText(text);
			if (text.HasContent())
			{
				OpAddressDropDown* dropdown = (OpAddressDropDown*) GetWidgetByName("Address_field");
				if (dropdown)
					dropdown->EmulatePaste(text);

				// This is a bit tricky. Paste&Go has a default shortcut that includes Ctrl and Shift which
				// normally redirects a url to a new tab. That is why we test IsKeyboardInvoked() below. In
				// addition the 'NewWindow' pref tells us to open in a new tab/window no matter what (DSK-313944)
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
				{
					if (g_application->IsSDI())
						g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
					else
					{
						BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND;
						g_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
					}
				}
				else
				{
					BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND;
					g_application->GoToPage(text, bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				}
				CloseDialog(FALSE);
			}
			return TRUE;
			break;
		}

#ifdef _X11_SELECTION_POLICY_
		case OpInputAction::ACTION_PASTE_SELECTION_AND_GO:
		case OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND:
		{
			OpString text;
			g_desktop_clipboard_manager->GetText(text, true);
			if (text.HasContent())
			{

				OpAddressDropDown* dropdown = (OpAddressDropDown*) GetWidgetByName("Address_field");
				if (dropdown)
					dropdown->EmulatePaste(text);

				BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND;
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
				{
					if (g_application->IsSDI())
						g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
					else
						g_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				}
				else
					g_application->GoToPage(text.CStr(), bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
			}
			return TRUE;
		}
#endif
		case OpInputAction::ACTION_OPEN_URL_IN_CURRENT_PAGE:
		{
			// If Opera is configured not to reuse pages, it should be opened 
			// in a new page or new window instead of in the current page
			if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
			{
				if (g_application->IsSDI())
					g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, action->GetActionData(), action->GetActionDataString(), this);
				else	// tabbed or MDI
					g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE, action->GetActionData(), action->GetActionDataString(), this);
				CloseDialog(FALSE);
				return TRUE;
			}
		// fall through
		}
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW:
		{
			CloseDialog(FALSE);
			// Do not kill the action, underlying components will handled it (application),
			// only close the dialog because the user is done entering a url in the addressdropdown
			return FALSE;
		}
		
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**  OnChange
**
***********************************************************************************/

void GoToPageDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Nick_field"))
	{
		OpString nick;
		GetWidgetText("Nick_field", nick);

		if(g_hotlist_manager->HasNickname(nick, NULL, FALSE, TRUE))
		{
			CloseDialog(FALSE,FALSE,TRUE);
			OpenNickname(nick,FALSE);
		}
	}
}


BOOL GoToPageDialog::OpenNickname(const OpString& candidate, BOOL exact)
{
	OpString tmp;

	tmp.Set(candidate.CStr());
	tmp.Strip();

	if( tmp.HasContent() )
	{
		if( g_hotlist_manager->OpenByNickname(tmp, MAYBE, MAYBE, MAYBE, exact) )
		{
			return TRUE;
		}
	}

	return FALSE;
}


void GoToPageDialog::GoToPage(const OpString& candidate, OpInputAction* action)
{
	OpenURLSetting setting;
	setting.m_address.Set(candidate);
	setting.m_address.Strip();
	if (setting.m_address.HasContent())
	{
		setting.m_typed_address.Set(setting.m_address);
		setting.m_save_address_to_history = TRUE;
		setting.m_from_address_field = FALSE;
		setting.m_digits_are_speeddials = TRUE;
		setting.m_new_window    = MAYBE;
		setting.m_new_page      = MAYBE;
		setting.m_in_background = MAYBE;

		g_application->AdjustForAction(setting, action, g_application->GetActiveDocumentDesktopWindow());

		g_application->OpenURL(setting);
	}
}

