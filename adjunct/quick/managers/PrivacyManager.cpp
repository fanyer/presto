// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"


#include "adjunct/quick/managers/PrivacyManager.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/search/search_field_history.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/TransfersPanel.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/history/direct_history.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/thumbnails/thumbnailmanager.h"

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/accountmgr.h"
#endif //M2_SUPPORT

#if defined(_MACINTOSH_)
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/embrowser/EmBrowser_mac.h"
#endif

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
#include "modules/database/ps_commander.h"
#endif // DATABASE_MODULE_MANAGER_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

PrivacyManager *PrivacyManager::GetInstance()
{
	static PrivacyManager manager;
	return &manager;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

PrivacyManager::PrivacyManager() :
	m_turbo_not_available_in_private_tab_notified(FALSE),
	m_sync_password_manager_deletion(FALSE)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

PrivacyManager::~PrivacyManager()
{
	// Always kill the private folders on shutdown regardless
	DeletePrivateFolders();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::AddPrivateFolder(const uni_char *folder)
{
	OpString *new_folder = OP_NEW(OpString, ());
	if (new_folder)
	{
		if (OpStatus::IsError(new_folder->Set(folder))
			|| OpStatus::IsError(m_private_folders.Add(new_folder)))
		{
			OP_DELETE(new_folder);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL PrivacyManager::GetDeleteState(int flag)
{
	int flags = g_pcui->GetIntegerPref(PrefsCollectionUI::ClearPrivateDataDialog_CheckFlags);
	return flags & (1 << flag);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


void PrivacyManager::ClearPrivateData(Flags flags, BOOL write_to_prefs, BrowserDesktopWindow* browser_window)
{
	if (write_to_prefs)
	{
		TRAPD(rc,g_pcui->WriteIntegerL(PrefsCollectionUI::ClearPrivateDataDialog_CheckFlags, flags.Get()));
		TRAP(rc,g_prefsManager->CommitL());
	}

	ClearPrivateData(flags, browser_window);

	if (write_to_prefs)
		g_application->SettingsChanged(SETTINGS_DELETE_PRIVATE_DATA);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::ClearPrivateData(Flags flags, BrowserDesktopWindow* browser_window)
{
	BOOL clear_global_history			= flags.IsSet(GLOBAL_HISTORY);
	BOOL clear_typed_history			= flags.IsSet(TYPED_HISTORY);
	BOOL clear_mail_passwords			= flags.IsSet(MAIL_PASSWORDS);
	BOOL clear_download_history			= flags.IsSet(DOWNLOAD_HISTORY);
	BOOL clear_wand_passwords_sync		= flags.IsSet(WAND_PASSWORDS_SYNC);
	BOOL clear_wand_passwords_dont_sync	= flags.IsSet(WAND_PASSWORDS_DONT_SYNC);
	BOOL clear_bookmark_visited_time	= flags.IsSet(BOOKMARK_VISITED_TIME);
	BOOL close_all_windows				= flags.IsSet(ALL_WINDOWS);
	BOOL clear_searchfield_history		= flags.IsSet(SEARCHFIELD_HISTORY);
	BOOL clear_plugin_data				= flags.IsSet(PLUGIN_DATA);
	BOOL clear_geoloc_data				= flags.IsSet(GEOLOC_DATA);
	BOOL clear_camera_permissions		= flags.IsSet(CAMERA_PERMISSIONS);

	if (!browser_window)
	{
		browser_window = g_application->GetActiveBrowserDesktopWindow();
	}

	if (close_all_windows)
	{
		OpVector<DesktopWindow> browser_windows;
		if (OpStatus::IsSuccess(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, browser_windows)))
		{
			for(UINT32 i = 0; i < browser_windows.GetCount(); ++i)
 			{
				// Tabs from active browser window should be closed at the end
 				if (browser_windows.Get(i) != browser_window)
 				{ 
					// Close the window itself, not only the tabs (fixing DSK-346047)
					browser_windows.Get(i)->Close(TRUE);
 				}
			}
		}
		if (browser_window)
		{
			browser_window->GetWorkspace()->CloseAll(TRUE /*immediately*/);
		}

		// delete only the autosave session file
		if (g_session_manager)
		{
			OP_MEMORY_VAR OP_STATUS err;

			TRAP(err, g_session_manager->ScanSessionFolderL());
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ? Fails when unable to create folder.

			for (OP_MEMORY_VAR UINT32 i = 0; i < g_session_manager->GetSessionCount(); i++)
			{
				const uni_char* OP_MEMORY_VAR name = 0;

				TRAPD(err, name = g_session_manager->GetSessionNameL(i).CStr());
				// FIXME: ignoring error code

				if (uni_stricmp(name, UNI_L("autosave")) == 0)
				{
					TRAP(err, g_session_manager->DeleteSessionL(i));
					// FIXME: ignoring error code
				}
			}
		}
		// delete the backupfile for the autosave session
		OpFile autosave_backup;
		OP_STATUS err = autosave_backup.Construct(UNI_L("autosave.win.bak"), OPFILE_SESSION_FOLDER);
		BOOL exists;
		if (OpStatus::IsSuccess(err) && OpStatus::IsSuccess(autosave_backup.Exists(exists)) && exists)
		{
			OpStatus::Ignore(autosave_backup.Delete());
		}
	}

	int wic_flags = 0;
	if (flags.IsSet(CACHE))
	{
		// clean temporary downloads directory
		FileUtils::DeleteFolderFiles(OPFILE_TEMPDOWNLOAD_FOLDER);
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_IMAGE_CACHE;
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_DISK_CACHE;
	}
	if (clear_plugin_data)
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_PLUGIN_DATA;
	if (clear_geoloc_data)
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_GEOLOCATION_PERMISSIONS;
	if (clear_camera_permissions)
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_CAMERA_PERMISSIONS;
	if (flags.IsSet(VISITED_LINKS))
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_VISITED_LINKS;
	if (flags.IsSet(ALL_COOKIES))
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_ALL_COOKIES;
	if (flags.IsSet(TEMPORARY_COOKIES))
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_SESSION_COOKIES;
	if (flags.IsSet(DOCUMENTS_WITH_PASSWORD))
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_SENSITIVE_DATA;
	if (flags.IsSet(WEBSTORAGE_DATA))
	{
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_APPCACHE;
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_WEBDATABASES;
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_WEBSTORAGE;
	}
	if (flags.IsSet(EXTENSION_DATA))
		wic_flags |=  OpWindowCommanderManager::WIC_OPERA_CLEAR_EXTENSION_STORAGE;

	g_windowCommanderManager->ClearPrivateData(wic_flags);

	if (clear_bookmark_visited_time)
	{
		g_favicon_manager->EraseDataFiles();
		g_desktop_bookmark_manager->ClearSensitiveSettings(DesktopBookmarkManager::VisitedTime, TRUE /* because icons are gone */);
	}

	DesktopHistoryModel* OP_MEMORY_VAR history_model = DesktopHistoryModel::GetInstance();


	if (clear_global_history)
	{
		// Removes closed windows (ie, a window with many tabs) in trashcan menu
		g_session_manager->EmptyClosedSessions();

		if (browser_window)
		{
			// Removes closed tabs and blocked popups in trashcan menu
			browser_window->EmptyClosedTabsAndPopups();
		}

		if(history_model)
		{
			history_model->DeleteAllItems(); // Empties file on disk as well

#ifdef MSWIN
			g_main_message_handler->PostMessage(MSG_WIN_RECREATE_JUMPLIST, 0, 0);
#endif //MSWIN
		}
	}

	if (clear_typed_history)
	{
		directHistory->DeleteAllItems(); // Empties file on disk as well

		// Clear out the matches section of the opera6.ini
		TRAPD(err, g_pcui->ClearMatchSettingsL());
	}

	if (clear_download_history && NULL != g_desktop_transfer_manager)
	{
		TransferItemContainer* itemcontainer = (TransferItemContainer*)g_desktop_transfer_manager->GetItemByPosition(0);
		INT32 runningitems = 0;
		while(itemcontainer)
		{
			OpTransferListener::TransferStatus running_status = itemcontainer->GetStatus();

			TransferItem* titem = itemcontainer->GetAssociatedItem();
			if(titem && !(running_status == OpTransferListener::TRANSFER_PROGRESS) )
			{
				((TransferManager*)g_transferManager)->ReleaseTransferItem((OpTransferItem*)titem);
				g_desktop_transfer_manager->OnTransferItemRemoved((OpTransferItem*)titem);
			}
			else
			{
				// keep running downloads ref. bug 130082
				runningitems++;
			}
			itemcontainer = (TransferItemContainer*)g_desktop_transfer_manager->GetItemByPosition(runningitems);
		}
#ifdef _BITTORRENT_SUPPORT_
		// clear the metadata directory for bittorrent
		OpString tmp_storage;
		const OpStringC btdir(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_BITTORRENT_METADATA_FOLDER, tmp_storage));
		OpFile file;

		if(OpStatus::IsSuccess(file.Construct(btdir.CStr())))
		{
			OpStatus::Ignore(file.Delete());
		}

#endif // _BITTORRENT_SUPPORT_
		if(runningitems != 0)
		{
			SimpleDialogController* controller = OP_NEW(SimpleDialogController,
				(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_WARNING,WINDOW_NAME_HAD_TRANSFERS_RUNNING,
				 Str::D_CLEAR_PRIVDATA_HAD_TRANSFERSRUNNING_MESSAGE, Str::D_CLEAR_PRIVDATA_HAD_TRANSFERSRUNNING_TITLE));
			
			OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, browser_window));
		}
	}

#ifdef WAND_SUPPORT
# ifdef M2_SUPPORT
	if (clear_mail_passwords)
	{
		g_wand_manager->DeleteLoginsWithIdPrefix(WAND_OPERA_MAIL);
		if (g_m2_engine)
			g_m2_engine->GetAccountManager()->ClearAccountPasswords();
	}
# endif //M2_SUPPORT

	if (clear_wand_passwords_sync)
		g_wand_manager->ClearAll(FALSE, TRUE, FALSE, FALSE);
	else if (clear_wand_passwords_dont_sync)
	{
		g_wand_manager->ClearAll(FALSE, FALSE, TRUE, TRUE);

		// Notify desktop sync manager that passwords synchronization
		// is now turned off (but only if syncing is turned on).

		if (g_sync_manager->IsLinkEnabled())
			g_sync_manager->SetSupportsType(SyncManager::SYNC_PASSWORD_MANAGER, FALSE);
	}
#endif // WAND_SUPPORT

	if (clear_searchfield_history)
	{
		g_search_field_history->DeleteAllItems();
		OpStatus::Ignore(g_search_field_history->Write());
	}

#ifdef OPERA_CONSOLE
	// Always close the console
	if (NULL != g_message_console_manager)
	{
		g_message_console_manager->CloseDialog();
	}
#endif // OPERA_CONSOLE

#ifdef _MACINTOSH_
	if (close_all_windows)
	{
		g_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE);
	}
#endif // _MACINTOSH_
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::DeletePrivateFolders()
{
	// Loop the folders and delete them all!
	for (UINT32 i = 0; i < m_private_folders.GetCount(); i++)
	{
		OpFile folder;
		OpString *s = m_private_folders.Get(i);

		if (s)
		{
			folder.Construct(s->CStr());
			// Delete the folder and this is recursive
			folder.Delete();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS PrivacyManager::GetPassword(const OpStringC8 &protocol,
									  const OpStringC &server,
									  const OpStringC &username,
									  PrivacyManagerCallback* callback)
{
	OpString id;

	if (username.IsEmpty())
		return OpStatus::ERR;

	// Build the Wand ID
	RETURN_IF_ERROR(MakeWandID(protocol, server, id));

	int index = 0;
	WandLogin* info = g_wand_manager->FindLogin(id.CStr(), index);

	while (info && info->username.Compare(username))
	{
		index++;
		info = g_wand_manager->FindLogin(id.CStr(), index);
	}

	if (!info)
		return OpStatus::ERR;

	// We have found the the login data
	// Decrypt the password, listen for replies
	CallerInfo *caller = OP_NEW(CallerInfo, (callback));
	if (!caller)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = m_callers.Add(caller);
	if (OpStatus::IsError(status) || OpStatus::IsError(status = g_wand_manager->GetLoginPasswordWithoutWindow(info, caller)))
		OP_DELETE(caller);

	return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS PrivacyManager::SetPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, const OpStringC &password)
{
	OpString id;

	if (username.IsEmpty())
		return OpStatus::ERR;

	// Build the Wand ID
	RETURN_IF_ERROR(MakeWandID(protocol, server, id));

	// Store the password
	return g_wand_manager->StoreLoginWithoutWindow(id.CStr(), username.CStr(), password.CStr());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS PrivacyManager::DeletePassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username)
{
	if (username.IsEmpty())
		return OpStatus::OK;

	OpString id;

	// Build the Wand ID
	RETURN_IF_ERROR(MakeWandID(protocol, server, id));

	// Delete the wand info
	g_wand_manager->DeleteLogin(id.CStr(), username.CStr());

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS PrivacyManager::RemoveCallback(PrivacyManagerCallback* callback)
{
	for (unsigned i = 0; i < m_callers.GetCount(); i++)
	{
		if (callback == m_callers.Get(i)->GetCallback())
			m_callers.Get(i)->ResetCallback();
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL PrivacyManager::IsCallback(PrivacyManagerCallback* callback)
{
	unsigned i = 0;

	while (i < m_callers.GetCount() && callback != m_callers.Get(i)->GetCallback())
		i++;

	return i < m_callers.GetCount();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS PrivacyManager::MakeWandID(const OpStringC8 &protocol, const OpStringC &server, OpString &id)
{
	// First just set the id to be the protocol
	RETURN_IF_ERROR(id.Set(protocol));

	// If there is a server build the id from it
	if (server.HasContent())
		RETURN_IF_ERROR(id.AppendFormat(UNI_L("/%s"), server.CStr()));

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::RemoveCallerInfo(CallerInfo* caller_info)
{
	m_callers.RemoveByItem(caller_info);
	OP_DELETE(caller_info);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::CallerInfo::OnPasswordRetrieved(const uni_char* password)
{
	if (m_callback)
		m_callback->OnPasswordRetrieved(password);

	PrivacyManager::GetInstance()->RemoveCallerInfo(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrivacyManager::CallerInfo::OnPasswordRetrievalFailed()
{
	if (m_callback)
		m_callback->OnPasswordFailed();

	PrivacyManager::GetInstance()->RemoveCallerInfo(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
OP_STATUS PrivacyManager::SetPrivateWindowSaveFolderLocation(OpStringC16 string)
{
	RETURN_IF_ERROR(m_private_window_save_loc.Set(string));
	int len = m_private_window_save_loc.Length();
	int pos = m_private_window_save_loc.FindLastOf(UNI_L(PATHSEPCHAR));
	if (pos != len-1)
	{
		RETURN_IF_ERROR(m_private_window_save_loc.Append(PATHSEP));
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uni_char * PrivacyManager::GetPrivateWindowSaveFolderLocation()
{
	if (m_private_window_save_loc.IsEmpty())
	{
		OpString tmp_storage;
		m_private_window_save_loc.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage));
	}
	return m_private_window_save_loc.CStr();
}
