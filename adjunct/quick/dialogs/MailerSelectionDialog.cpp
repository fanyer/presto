/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Adam Minchinton
 */
#include "core/pch.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/MailerSelectionDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpDropDown.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

MailerSelectionDialog::MailerSelectionDialog()
  : m_selection(NO_MAILER)
  , m_webmail_id(0)
  , m_force_background(FALSE)
  , m_new_window(FALSE)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MailerSelectionDialog::Init(DesktopWindow * parent_window, const MailTo& mailto, BOOL force_background, BOOL new_window, const OpStringC* attachment)
{
	RETURN_IF_ERROR(m_mailto.Init(mailto.GetRawMailTo().CStr()));
	m_force_background = force_background;
	m_new_window = new_window;
	if (attachment)
	{
		RETURN_IF_ERROR(m_attachment.Set(attachment->CStr()));
	}
	return Dialog::Init(parent_window);
}
//////////////////////////////////////////////////////////////////////////////////////////////////

void MailerSelectionDialog::OnInit()
{
	OpWidget* widget;

	// Set the default application and the default mailer
	widget = GetWidgetByName("Radio_default_application");
	if (widget)
	{
		widget->SetValue(TRUE);
	}

	// Fill the web service selection dropdown
	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Dropdown_webmail_service"));
	if (dropdown)
	{
		for (unsigned i = 0; i < WebmailManager::GetInstance()->GetCount(); i++)
		{
			unsigned id = WebmailManager::GetInstance()->GetIdByIndex(i);
			OpString name;

			if (OpStatus::IsSuccess(WebmailManager::GetInstance()->GetName(id, name)))
			{
				INT32 got_index;
				dropdown->AddItem(name.CStr(), -1,&got_index, id);
				// Set the favicon based on the URL
				OpString url;
				if (OpStatus::IsSuccess(WebmailManager::GetInstance()->GetURL(id,url)))
				{
					OpWidgetImage widget_image;
					widget_image.SetRestrictImageSize(TRUE);
					Image img = g_favicon_manager->Get(url.CStr());
					if (img.IsEmpty())
						widget_image.SetImage("Mail Unread");
					else
						widget_image.SetBitmapImage(img);
					dropdown->ih.SetImage(got_index, &widget_image, widget);
				}
			}

		}
		// Default to remember setting
		SetDoNotShowDialogAgain(TRUE);
		// Web service dropdown is by default disabled
		dropdown->SetEnabled(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Str::LocaleString MailerSelectionDialog::GetDoNotShowAgainTextID()
{
	return Str::D_REMEMBER_MY_PREF_CHOICE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MailerSelectionDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Radio_use_webmail"))
	{
		SetWidgetEnabled("Dropdown_webmail_service", widget->GetValue());
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

UINT32 MailerSelectionDialog::OnOk()
{
	OpWidget* widget;

	// Grab the mailer selected
	widget = GetWidgetByName("Radio_default_application");
	if (widget && widget->GetValue())
		m_selection = SYSTEM_MAILER;

	widget = GetWidgetByName("Radio_use_Opera");
	if (widget && widget->GetValue())
		m_selection = INTERNAL_MAILER;

	widget = GetWidgetByName("Radio_use_webmail");
	if (widget && widget->GetValue())
		m_selection = WEBMAIL_MAILER;

	if (m_selection == WEBMAIL_MAILER)
	{
		OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Dropdown_webmail_service"));

		if (dropdown && dropdown->GetSelectedItem() != -1)
			m_webmail_id = dropdown->GetItemUserData(dropdown->GetSelectedItem());
	}

	// If the do not show again box is check set the preference to System, this should mean
	// that the box never shows again as the preference will now be changed
	if (IsDoNotShowDialogAgainChecked() && m_selection != INTERNAL_MAILER)
	{
		// The trusted protocol list must be in sync with the other prefs. settings
		TrustedProtocolData data;
		int index = g_pcdoc->GetTrustedProtocolInfo(UNI_L("mailto"), data);
		if (index >= 0)
		{
			if (m_selection == WEBMAIL_MAILER)
				data.viewer_mode = UseWebService;
			else if(m_selection == INTERNAL_MAILER)
				data.viewer_mode = UseInternalApplication;
			else
				data.viewer_mode = UseDefaultApplication;
			data.user_defined = TRUE;
			data.flags = TrustedProtocolData::TP_ViewerMode | TrustedProtocolData::TP_UserDefined;

			// Core does not know about preinstalled platform web handlers.
			if (data.viewer_mode == UseWebService)
			{
				WebmailManager::HandlerSource source;
				OP_STATUS rc = WebmailManager::GetInstance()->GetSource(m_webmail_id, source);
				if (OpStatus::IsSuccess(rc) && source == WebmailManager::HANDLER_PREINSTALLED)
				{
					data.viewer_mode = UseCustomApplication;
				}
			}

			TRAPD(rc, g_pcdoc->SetTrustedProtocolInfoL(index, data));
			if (OpStatus::IsSuccess(rc))
				TRAP(rc, g_pcdoc->WriteTrustedProtocolsL(g_pcdoc->GetNumberOfTrustedProtocols()));
		}

		if (m_selection == SYSTEM_MAILER)
		{
			TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_SYSTEM));
		}
		else if (m_selection == WEBMAIL_MAILER)
		{
			TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_WEBMAIL));
			TRAP(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::WebmailService, m_webmail_id));
		}
		TRAPD(rc, g_prefsManager->CommitL());
	}

	MAILHANDLER handler;
	switch (m_selection)
	{
#ifdef M2_SUPPORT
		case INTERNAL_MAILER:
		{
			handler = MAILHANDLER_OPERA;
			if (!g_application->ShowM2())
				return 0;

			NewAccountWizard* wizard = OP_NEW(NewAccountWizard, ());
			if (wizard)
			{
				wizard->Init(AccountTypes::UNDEFINED, GetParentDesktopWindow());
				wizard->SetMailToInfo(m_mailto, m_force_background, m_new_window, &m_attachment);
			}
			return 0;
		}
#endif // M2_SUPPORT
		case SYSTEM_MAILER:
			handler = MAILHANDLER_SYSTEM;
			break;
		case WEBMAIL_MAILER:
			handler    = MAILHANDLER_WEBMAIL;
			break;
		case NO_MAILER:
			// No mailer selected, don't do anything
		default:
			return 0;
	}

	MailCompose::InternalComposeMail(m_mailto, handler, m_webmail_id, m_force_background, m_new_window, &m_attachment);
	return 0;
}

