/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author George Refseth (rfz)
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick/dialogs/MailerSelectionDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/security_manager/include/security_manager.h"

/***********************************************************************************
**
**	MailCompose::ComposeMail
**
***********************************************************************************/
void MailCompose::ComposeMail(OpINT32Vector& id_list)
{
	OpString rfc822_address;

	if (!g_hotlist_manager->GetRFC822FormattedNameAndAddress(id_list, rfc822_address, TRUE))
		return;

	OpString empty;
	MailTo mailto;
	mailto.Init(rfc822_address, empty, empty, empty, empty);
	ComposeMail(mailto);
}

/***********************************************************************************
 **
 ** MailCompose::ComposeMail
 **
 ***********************************************************************************/
void MailCompose::ComposeMail(const MailTo& mailto, BOOL force_in_opera, BOOL force_background, BOOL new_window, const OpStringC* attachment, ComposeDesktopWindow** compose_window)
{
	MAILHANDLER handler    = force_in_opera ? MAILHANDLER_OPERA : static_cast<MAILHANDLER>(g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler));
	unsigned    webmail_id = g_pcui->GetIntegerPref(PrefsCollectionUI::WebmailService);

	// Check if we should show the mailer selection dialog
	if (handler == MAILHANDLER_OPERA && !g_application->HasMail())
	{
		MailerSelectionDialog* dialog = OP_NEW(MailerSelectionDialog, ());
		DesktopWindow* parent = g_application->GetActiveMailDesktopWindow();
		if( !parent )
			parent = g_application->GetActiveBrowserDesktopWindow();

		if (OpStatus::IsError(dialog->Init(parent, mailto, force_background, new_window, attachment)))
			OP_DELETE(dialog);
		else
			return;
	}

	InternalComposeMail(mailto, handler, webmail_id, force_background, new_window, attachment, compose_window);
}

/***********************************************************************************
 **
 **
 ** MailCompose::InternalComposeMail
 ***********************************************************************************/
void MailCompose::InternalComposeMail(const MailTo& mailto, MAILHANDLER handler, unsigned int webmail_id, BOOL force_background, BOOL new_window, const OpStringC* attachment, ComposeDesktopWindow** compose_window)
{
	if (handler == MAILHANDLER_WEBMAIL)
	{
		// Test for valid ID and, provided this is a custom web handler, that the protocol is not blocked
		WebmailManager::HandlerSource source;
		OP_STATUS rc = WebmailManager::GetInstance()->GetSource(webmail_id, source);
		if (OpStatus::IsSuccess(rc) && source == WebmailManager::HANDLER_CUSTOM)
		{
			URL url;
			BOOL allowed;
			OpSecurityContext source(url);
			OpSecurityContext target(url);
			target.AddText(UNI_L("mailto"));
			rc = g_secman_instance->CheckSecurity(OpSecurityManager::WEB_HANDLER_REGISTRATION, source, target, allowed);
			if (OpStatus::IsError(rc) || !allowed)
				rc = OpStatus::ERR;
		}
		if (OpStatus::IsError(rc))
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_OPERA);
			ComposeMail(mailto, FALSE, force_background, new_window, attachment);
			return;
		}

		OpString8 target_url8;
		if (OpStatus::IsError(WebmailManager::GetInstance()->GetTargetURL(webmail_id, mailto.GetRawMailTo(), target_url8)))
			return;

		OpString target_url;
		target_url.SetFromUTF8(target_url8.CStr());

		g_application->GoToPage(target_url, TRUE);
	}
	else if (handler == MAILHANDLER_OPERA && !g_application->HasMail())
	{
	}
	else if( KioskManager::GetInstance()->GetNoMailLinks() )
	{
	}
	else if (handler != MAILHANDLER_OPERA && handler != MAILHANDLER_WEBMAIL)
	{
		OpString raw;
		raw.Set(mailto.GetRawMailTo().CStr());
		g_desktop_op_system_info->ComposeExternalMail(mailto.GetTo().CStr(), mailto.GetCc().CStr(), mailto.GetBcc().CStr(),
													  mailto.GetSubject().CStr(), mailto.GetBody().CStr(), raw.CStr(), _MAILHANDLER_FIRST_ENUM);
	}
#ifdef M2_SUPPORT
	else if (handler == MAILHANDLER_OPERA)
	{
		ComposeDesktopWindow* window;
		if (OpStatus::IsSuccess(ComposeDesktopWindow::Construct(&window, new_window? g_application->GetBrowserDesktopWindow( TRUE, force_background, FALSE)->GetWorkspace():
			g_application->IsSDI() ? g_application->GetBrowserDesktopWindow(TRUE)->GetWorkspace() : (g_application->GetActiveBrowserDesktopWindow() ? g_application->GetActiveBrowserDesktopWindow()->GetWorkspace() : NULL))))
		{
			window->InitMessage(MessageTypes::NEW);
			// This can be triggered from the systray, even when Opera is minimized, so we need to activate the browserview first
			if (window->GetParentDesktopWindow())
				window->GetParentDesktopWindow()->Activate();

			if (mailto.GetTo().HasContent()) window->SetHeaderValue(Header::TO, &mailto.GetTo());
			if (mailto.GetCc().HasContent()) window->SetHeaderValue(Header::CC, &mailto.GetCc());
			if (mailto.GetBcc().HasContent()) window->SetHeaderValue(Header::BCC, &mailto.GetBcc());
			if (mailto.GetSubject().HasContent()) window->SetHeaderValue(Header::SUBJECT, &mailto.GetSubject());
			if (mailto.GetBody().HasContent()) window->SetMessageBody(mailto.GetBody());
			window->Show(TRUE, NULL, OpWindow::RESTORED, force_background);
			if (attachment && attachment->HasContent())
				window->AddAttachment(attachment->CStr());
		}
		if (compose_window)
		{
			*compose_window = window;
		}
	}
#endif //M2_SUPPORT
}


