/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/m2_ui/controllers/AuthenticationFailedController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickExpand.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "modules/locale/locale-enum.h"


AuthenticationFailedController::AuthenticationFailedController(ProtocolBackend& backend, const OpStringC& server_message)
	: m_backend(backend)
	, m_server_message(server_message)
{
}


void AuthenticationFailedController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Mail Authentication Failed Dialog"));

	ANCHORD(OpString, title);
	Account* account = m_backend.GetAccountPtr();
	if (account->GetAccountName().HasContent()
			&& OpStatus::IsSuccess(StringUtils::GetFormattedLanguageString(
					title, Str::D_MAIL_AUTHENTICATION_FAILED_TITLE, account->GetAccountName().CStr())))
	{
		LEAVE_IF_ERROR(m_dialog->SetTitle(title));
	}

	return InitOptionsL();
}


void AuthenticationFailedController::InitOptionsL()
{
	const TypedObjectCollection* widgets = m_dialog->GetWidgetCollection();

	// Show server message, or hide label and expand details
	QuickMultilineLabel* label = widgets->GetL<QuickMultilineLabel>("Server_message_label");

	ANCHORD(OpString, message);
	if (m_server_message.HasContent()
			&& OpStatus::IsSuccess(StringUtils::GetFormattedLanguageString(
					message, Str::D_MAIL_AUTHENTICATION_SERVER_MSG, m_server_message.CStr())))
	{
		LEAVE_IF_ERROR(label->SetText(message));
	}
	else
	{
		label->Hide();

		QuickExpand* details = widgets->GetL<QuickExpand>("Expand_details");
		details->Expand();
	}

	// Bind username
	ANCHORD(OpString, username);
	LEAVE_IF_ERROR(m_backend.GetUsername(username));
	m_username.Set(username);
	LEAVE_IF_ERROR(GetBinder()->Connect("Username_edit", m_username));

	// Bind password
	LEAVE_IF_ERROR(GetBinder()->Connect("Password_edit", m_password));

	// Set up method dropdown
	QuickDropDown* dropdown = widgets->GetL<QuickDropDown>("Authentication_dropdown");
	LEAVE_IF_ERROR(m_backend.PopulateAuthenticationDropdown(dropdown->GetOpWidget()));
	m_method.Set(m_backend.GetAccountPtr()->GetIncomingAuthenticationMethod());
	LEAVE_IF_ERROR(GetBinder()->Connect(*dropdown, m_method));

	// Bind "remember password" checkbox
	m_remember_password.Set(true);
	LEAVE_IF_ERROR(GetBinder()->Connect("Remember_password", m_remember_password));
}


void AuthenticationFailedController::OnCancel()
{
	m_backend.CancelAuth();
}


void AuthenticationFailedController::OnOk()
{
	// Save data to account
	Account* account = m_backend.GetAccountPtr();

	if (account->IsIncomingBackend(&m_backend))
	{
		account->SetIncomingUsername(m_username.Get());
		account->SetIncomingAuthenticationMethod(AccountTypes::AuthenticationType(m_method.Get()));
		if (m_password.Get().HasContent())
			account->SetIncomingPassword(m_password.Get(), m_remember_password.Get());
	}
	else
	{
		account->SetOutgoingUsername(m_username.Get());
		account->SetOutgoingAuthenticationMethod(AccountTypes::AuthenticationType(m_method.Get()));
		if (m_password.Get().HasContent())
			account->SetOutgoingPassword(m_password.Get(), m_remember_password.Get());
	}

	// Force retry
	m_backend.RetryAuth();
}
