/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author: Erman Doser (ermand)
 */

#include "core/pch.h"

#include "adjunct/quick/controller/ClearPrivateDataController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/sync/view/SyncPasswordDeletionWarningDialog.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

ClearPrivateDataController::ClearPrivateDataController()
	: m_sync_passwords_deletion(SyncPasswordDeletionWarningDialog::DONT_DELETE_ANYTHING)
	, m_disable_action_handling(false)
{
}

void ClearPrivateDataController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Clear Private Data Dialog"));
	LEAVE_IF_ERROR(InitOptions());
}

OP_STATUS ClearPrivateDataController::InitOptions()
{
	m_flags.Set(g_pcui->GetIntegerPref(PrefsCollectionUI::ClearPrivateDataDialog_CheckFlags));
	if (m_flags.Get() == 0)
	{
		// reset, the dialog should do *something*
		// setting defaults here, since only this dialog will use the setting
		m_flags.SetAll();
		m_flags.Set(PrivacyManager::MAIL_PASSWORDS, FALSE);
		m_flags.Set(PrivacyManager::WAND_PASSWORDS_SYNC, FALSE);
		m_flags.Set(PrivacyManager::WAND_PASSWORDS_DONT_SYNC, FALSE);
		m_flags.Set(PrivacyManager::EXTENSION_DATA, FALSE);
	}

	RETURN_IF_ERROR(GetBinder()->Connect("Delete_all_cookies", m_del_all_cookies));
	RETURN_IF_ERROR(GetBinder()->Connect("Delete_temporary_cookies", m_del_tmp_cookies));
	RETURN_IF_ERROR(GetBinder()->Connect("Delete_passworddocs_and_data", m_del_pwddocs_data));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_cache", m_clear_cache));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_plugin_data", m_clear_plugin_data));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_geoloc_data", m_clear_geoloc_data));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_camera_permissions", m_clear_camera_permissions));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_visited_pages", m_clear_visited_pages));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_transferred_files_history", m_clear_trnsf_files_hist));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_email_account_passwords", m_clear_email_acc_pwds));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_wand_passwords", m_clear_wand_pwds));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_bookmark_visited_time", m_clear_bkmrk_visited_time));
	RETURN_IF_ERROR(GetBinder()->Connect("Close_all_windows", m_close_all_windows));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_webstorage", m_clear_web_storage));
	RETURN_IF_ERROR(GetBinder()->Connect("Clear_extension_data", m_clear_extension_data));

	m_del_all_cookies.Set(m_flags.IsSet(PrivacyManager::ALL_COOKIES));
	m_del_tmp_cookies.Set(m_flags.IsSet(PrivacyManager::TEMPORARY_COOKIES));
	m_del_pwddocs_data.Set(m_flags.IsSet(PrivacyManager::DOCUMENTS_WITH_PASSWORD));
	m_clear_cache.Set(m_flags.IsSet(PrivacyManager::CACHE));
	m_clear_plugin_data.Set(m_flags.IsSet(PrivacyManager::PLUGIN_DATA));
	m_clear_geoloc_data.Set(m_flags.IsSet(PrivacyManager::GEOLOC_DATA));
	m_clear_camera_permissions.Set(m_flags.IsSet(PrivacyManager::CAMERA_PERMISSIONS));
	m_clear_visited_pages.Set(m_flags.IsSet(PrivacyManager::GLOBAL_HISTORY));
	m_clear_trnsf_files_hist.Set(m_flags.IsSet(PrivacyManager::DOWNLOAD_HISTORY));
	m_clear_email_acc_pwds.Set(m_flags.IsSet(PrivacyManager::MAIL_PASSWORDS));
	m_clear_wand_pwds.Set(m_flags.IsSet(PrivacyManager::WAND_PASSWORDS_SYNC)
			|| m_flags.IsSet(PrivacyManager::WAND_PASSWORDS_DONT_SYNC));
	m_clear_bkmrk_visited_time.Set(m_flags.IsSet(PrivacyManager::BOOKMARK_VISITED_TIME));
	m_close_all_windows.Set(m_flags.IsSet(PrivacyManager::ALL_WINDOWS));
	m_clear_web_storage.Set(m_flags.IsSet(PrivacyManager::WEBSTORAGE_DATA));
	m_clear_extension_data.Set(m_flags.IsSet(PrivacyManager::EXTENSION_DATA));

	return OpStatus::OK;
}

void ClearPrivateDataController::ClearPrivateData()
{
	// delete the stored user email anyway, there's no separate checkbox for it
	TRAPD(err, g_pcui->ResetStringL(PrefsCollectionUI::UserEmail));

	if (!g_privacy_manager)
		return;

	m_flags.Reset();

	m_flags.Set(PrivacyManager::TEMPORARY_COOKIES, m_del_tmp_cookies.Get());
	m_flags.Set(PrivacyManager::DOCUMENTS_WITH_PASSWORD, m_del_pwddocs_data.Get());
	m_flags.Set(PrivacyManager::ALL_COOKIES, m_del_all_cookies.Get());
	m_flags.Set(PrivacyManager::WAND_PASSWORDS_SYNC,
			m_sync_passwords_deletion == SyncPasswordDeletionWarningDialog::DELETE_ON_ALL_COMPUTERS);
	m_flags.Set(PrivacyManager::WAND_PASSWORDS_DONT_SYNC,
			m_sync_passwords_deletion == SyncPasswordDeletionWarningDialog::DELETE_ONLY_ON_THIS_COMPUTER);
	// Normal History, empties with History.
	m_flags.Set(PrivacyManager::GLOBAL_HISTORY, m_clear_visited_pages.Get());
	// Typed History, empties with History.
	m_flags.Set(PrivacyManager::TYPED_HISTORY, m_clear_visited_pages.Get());
	// Visited Links, empties with History.
	m_flags.Set(PrivacyManager::VISITED_LINKS, m_clear_visited_pages.Get());
	// Previous searches in the search field.
	m_flags.Set(PrivacyManager::SEARCHFIELD_HISTORY, m_clear_visited_pages.Get());
	m_flags.Set(PrivacyManager::MAIL_PASSWORDS, m_clear_email_acc_pwds.Get());
	m_flags.Set(PrivacyManager::CACHE, m_clear_cache.Get());
	m_flags.Set(PrivacyManager::PLUGIN_DATA, m_clear_plugin_data.Get());
	m_flags.Set(PrivacyManager::GEOLOC_DATA, m_clear_geoloc_data.Get());
	m_flags.Set(PrivacyManager::CAMERA_PERMISSIONS, m_clear_camera_permissions.Get());
	m_flags.Set(PrivacyManager::DOWNLOAD_HISTORY, m_clear_trnsf_files_hist.Get());
	m_flags.Set(PrivacyManager::BOOKMARK_VISITED_TIME, m_clear_bkmrk_visited_time.Get());
	m_flags.Set(PrivacyManager::ALL_WINDOWS, m_close_all_windows.Get());
	m_flags.Set(PrivacyManager::WEBSTORAGE_DATA, m_clear_web_storage.Get());
	m_flags.Set(PrivacyManager::EXTENSION_DATA, m_clear_extension_data.Get());

	DesktopWindow* toplevel_window = m_dialog->GetDesktopWindow()->GetToplevelDesktopWindow();
	BrowserDesktopWindow* browser_window = NULL;
	if (toplevel_window->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
		browser_window = static_cast<BrowserDesktopWindow*>(toplevel_window);

	// Prevent window closing and action handling while deleting private data.
	// The process is not guaranteed to complete synchronously.
	m_disable_action_handling = true;
	m_dialog->BlockClosing(true);

	g_privacy_manager->ClearPrivateData(m_flags, TRUE, browser_window);

	m_dialog->BlockClosing(false);
	m_disable_action_handling = false;
}

BOOL ClearPrivateDataController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			return m_disable_action_handling;
	}

	return OkCancelDialogContext::DisablesAction(action);
}

OP_STATUS ClearPrivateDataController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			if (g_sync_manager->SupportsType(SyncManager::SYNC_PASSWORD_MANAGER)
					&& g_sync_manager->IsFeatureEnabled() && m_clear_wand_pwds.Get())
			{
				// Passwords synchronization is on therefore we should ask
				// the user whether he wants to synchronize passwords deletion.

				SyncPasswordDeletionWarningDialog* dialog = OP_NEW(SyncPasswordDeletionWarningDialog, ());
				RETURN_OOM_IF_NULL(dialog);
				RETURN_IF_ERROR(dialog->Init(m_dialog->GetDesktopWindow()));
				dialog->SetDialogListener(this);
				return OpStatus::OK;
			}
			else
		    {
				if (m_clear_wand_pwds.Get())
				{
					m_sync_passwords_deletion = SyncPasswordDeletionWarningDialog::DELETE_ONLY_ON_THIS_COMPUTER;
				}
				ClearPrivateData();
			}
		break;
	}

	return OkCancelDialogContext::HandleAction(action);
}

void ClearPrivateDataController::OnOk(Dialog* dialog, UINT32 result)
{
	m_sync_passwords_deletion = result;
	ClearPrivateData();

	g_input_manager->InvokeAction(OpInputAction::ACTION_OK, 0, NULL, this);
}
