/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/BrowserAuthenticationListener.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"

BrowserAuthenticationListener::~BrowserAuthenticationListener()
{
	// Remove dialog listeners
	for (unsigned i = 0; i < m_open_dialogs.GetCount(); i++)
	{
		m_open_dialogs.Get(i)->SetDialogListener(0);
	}
}

void BrowserAuthenticationListener::OnAuthenticationRequired(OpAuthenticationCallback* callback)
{
	DesktopWindow* dw = g_application->GetActiveDesktopWindow();
	OnAuthenticationRequired(callback, NULL, dw);
	return;
}

void BrowserAuthenticationListener::OnAuthenticationRequired(OpAuthenticationCallback* callback, OpWindowCommander *commander, DesktopWindow* parent_window)
{
	// Check if there already exists an authentication request with same authentication
	for (unsigned i = 0; i < m_open_dialogs.GetCount(); i++)
	{
		PasswordDialog* dialog_current = m_open_dialogs.Get(i);
		OpAuthenticationCallback* callback_current = dialog_current->GetCallback();

		// if same authentication don't launch another password dialog (on the same page)
		if (callback && callback->IsSameAuth(callback_current) && dialog_current->GetParentDesktopWindow() == parent_window)
			return;
	}

	PasswordDialog *dialog = OP_NEW(PasswordDialog, (callback, commander));
	if (!dialog)
	{
		callback->CancelAuthentication();
		return; //FALSE;
	}

	RETURN_VOID_IF_ERROR(dialog->Init(parent_window));

	if (OpStatus::IsError(m_open_dialogs.Add(dialog)))
	{
		dialog->CloseDialog(TRUE, TRUE);
		return;
	}

	dialog->SetDialogListener(this);

	// Inform OpPage about pending authentication 
	if (parent_window && parent_window->GetBrowserView())
	{	
		OpPage *page = parent_window->GetBrowserView()->GetOpPage();
		if (page)
			page->SetAuthIsPending(true);
	
		// Update document icon
		parent_window->GetBrowserView()->UpdateWindowImage(TRUE);
		parent_window->SetWidgetImage(parent_window->GetBrowserView()->GetFavIcon(FALSE));
	}

	return;
}

void BrowserAuthenticationListener::OnOk(Dialog* dialog, UINT32 result)
{
	// Close all dialogs with same authentication id
	PasswordDialog* password_dialog = static_cast<PasswordDialog*>(dialog);
	ClosePasswordDialogs(password_dialog->GetAuthID(), password_dialog);
}

void BrowserAuthenticationListener::OnClose(Dialog* dialog)
{
	m_open_dialogs.RemoveByItem(static_cast<PasswordDialog*>(dialog));
	
	// Inform OpPage about nonpending authentication 
	DesktopWindow *dw = dialog->GetParentDesktopWindow();
	if (dw && dw->GetBrowserView())
	{	
		OpPage *page = dw->GetBrowserView()->GetOpPage();
		if (page)
			page->SetAuthIsPending(false);
	}
}

void BrowserAuthenticationListener::ClosePasswordDialogs(const URL_ID authid, PasswordDialog* request_from)
{
	OpVector<PasswordDialog> still_opened_dialogs;

	// will close all passworddialogs with the supplied id, except the one that request was from
	for (unsigned i = 0; i < m_open_dialogs.GetCount(); i++)
	{
		PasswordDialog* dialog = m_open_dialogs.Get(i);
		if (dialog->GetAuthID() == authid && dialog != request_from)
		{
			dialog->CloseDialog(FALSE);
		}
		else 
		{
			still_opened_dialogs.Add(dialog);
		}
	}

	// This code updates m_open_dialogs list to contain only opened dialogs.
	// Relying on 'OnClose()' to call 'm_open_dialogs.RemoveByItem()' is not enough,
	// because some events (like OnAuthenticationRequired) may be called before 'OnClose()' gets called,
	// and we may want to access the good version of m_open_dialogs.
	m_open_dialogs.Swap(still_opened_dialogs);
}
