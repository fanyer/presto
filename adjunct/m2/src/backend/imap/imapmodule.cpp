/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/imapmodule.h"

#include "adjunct/m2/src/backend/imap/commands/AuthenticationCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MiscCommands.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/util/misc.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/opstrlst.h"

namespace ImapConstants
{
	const long ConnectRetryDelay = 5000; // Delay when reconnecting after unexpectedly disconnected
	const int  ImapTagLength     = 4;    // Length of tags used when sending commands
	const int  ConfigVersion     = 4;    // Version of configuration file
};
using namespace ImapConstants;


/***********************************************************************************
 **
 **
 ** ImapBackend::ImapBackend
 **
 ***********************************************************************************/
ImapBackend::ImapBackend(MessageDatabase& database)
  : ProtocolBackend(database)
  , m_default_connection(this, 1)
  , m_extra_connection(this, 2)
  , m_selected_folder(NULL)
  , m_uid_manager(GetMessageDatabase())
  , m_prefsfile(NULL)
  , m_connect_retries(0)
  , m_connected_on_sleep(false)
  , m_tag_seed(0)
  , m_options(0)
{
}


/***********************************************************************************
 **
 **
 ** ImapBackend::~ImapBackend
 **
 ***********************************************************************************/
ImapBackend::~ImapBackend()
{
	g_main_message_handler->UnsetCallBacks(this);
	OpStatus::Ignore(g_sleep_manager->RemoveSleepListener(this));
	OpStatus::Ignore(PrepareToDie());
}


/***********************************************************************************
 **
 **
 ** ImapBackend::Init
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::Init(Account* account)
{
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	m_account = account;

	RETURN_IF_ERROR(m_uid_manager.Init(account->GetAccountId()));

	RETURN_IF_ERROR(m_default_connection.Init());
	RETURN_IF_ERROR(m_extra_connection.Init());

	RETURN_IF_ERROR(ReadConfiguration());

	RETURN_IF_ERROR(g_sleep_manager->AddSleepListener(this));
	g_main_message_handler->SetCallBack(this, MSG_M2_CONNECT, (MH_PARAM_1)this);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::PrepareToDie
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::PrepareToDie()
{
	OpStatus::Ignore(m_default_connection.Disconnect("End of connection"));
	OpStatus::Ignore(m_extra_connection.Disconnect("End of connection"));

	OpStatus::Ignore(WriteConfiguration(TRUE));

	m_special_folders.RemoveAll();

	// Delete the prefs file
	if (m_prefsfile)
	{
		MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(m_prefsfile);
		m_prefsfile = NULL;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::FetchMessage
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::FetchMessage(message_index_t id, BOOL user_request, BOOL force_complete)
{
	ImapFolder* folder;
	unsigned uid;

	// Get folder and uid for this message
	RETURN_IF_ERROR(GetFolderAndUID(id, folder, uid));

	// Schedule message for fetching
	if (force_complete)
		RETURN_IF_ERROR(m_scheduled_messages.Insert(id));

	return FetchMessageByUID(folder, uid, force_complete, TRUE);
}

/***********************************************************************************
**
**
** ImapBackend::MessageReceived
**
***********************************************************************************/
void ImapBackend::MessageReceived(message_gid_t id)
{
	m_scheduled_messages.Remove(id);
}


/***********************************************************************************
**
**
** ImapBackend::IsScheduledForFetch
**
***********************************************************************************/
BOOL ImapBackend::IsScheduledForFetch(message_gid_t id) const
{
	return m_scheduled_messages.Contains(id);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::FetchMessageByUID
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::FetchMessageByUID(ImapFolder* folder, unsigned uid, BOOL force_full_message, BOOL high_priority)
{
	if (!folder)
		return OpStatus::ERR_NULL_POINTER;

	return GetConnection(folder).FetchMessageByUID(folder, uid, TRUE, force_full_message, high_priority);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::FetchMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::FetchMessages(BOOL enable_signalling)
{
	m_options &= ~ImapFlags::OPTION_LOCKED;

	// We force the default connection, but not the extra connection, since that might not have any folders yet
	RETURN_IF_ERROR(m_default_connection.FetchNewMessages(TRUE));
	return m_extra_connection.FetchNewMessages(FALSE);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::SelectFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::SelectFolder(UINT32 index_id)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByIndex(index_id, folder));

	if (folder->IsSelectable() && folder->IsSubscribed())
	{
		m_selected_folder = folder;

		if (GetConnection(folder).GetCurrentFolder() != folder)
			return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Select, (folder)));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RefreshFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RefreshFolder(UINT32 index_id)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByIndex(index_id, folder));

    if (!folder)
        return OpStatus::ERR;

    folder->RequestFullSync();
    return SyncFolder(folder);
}

/***********************************************************************************
 **
 **
 ** ImapBackend::RefreshAll
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RefreshAll()
{
	m_options &= ~ImapFlags::OPTION_LOCKED;

	RETURN_IF_ERROR(m_default_connection.RefreshAll());
	return m_extra_connection.RefreshAll();
}

/***********************************************************************************
 **
 **
 ** ImapBackend::StopFetchingMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::StopFetchingMessages()
{
	// Disconnect if busy
	if (IsBusy())
		return Disconnect("Disconnect requested by user");

	return OpStatus::OK;
}


/***********************************************************************************
 ** A keyword has been added to a message
 **
 ** ImapBackend::KeywordAdded
 ** @param message_id Message to which a keyword has been added
 ** @param keyword Keyword that was added to the message
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::KeywordAdded(message_gid_t message_id, const OpStringC8& keyword)
{
	ImapFolder* folder;
	unsigned uid;
	int silent = m_account->IsOfflineLogBusy() ? 0 : ImapCommands::Store::SILENT;

	if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
	{
		return GetConnection(folder).InsertCommand(
					OP_NEW(ImapCommands::Store, (folder, uid, uid,
					ImapCommands::USE_UID | ImapCommands::Store::ADD_FLAGS | silent | ImapCommands::Store::KEYWORDS, keyword)));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** A keyword has been removed from a message
 **
 ** ImapBackend::KeywordRemoved
 ** @param message_id Message from which a keyword has been removed
 ** @param keyword Keyword that was removed from the message
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword)
{
	ImapFolder* folder;
	unsigned uid;
	int silent = m_account->IsOfflineLogBusy() ? 0 : ImapCommands::Store::SILENT;

	if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
	{
		return GetConnection(folder).InsertCommand(
							OP_NEW(ImapCommands::Store, (folder, uid, uid,
							ImapCommands::USE_UID | ImapCommands::Store::REMOVE_FLAGS | silent | ImapCommands::Store::KEYWORDS, keyword)));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets the index where a message was originally stored (override if backend uses its own folders)
 **
 ** ImapBackend::GetMessageOrigin
 ** @param message_id A message to check
 ** @return Index where message was originally stored, or 0 if unknown
 ***********************************************************************************/
index_gid_t ImapBackend::GetMessageOrigin(message_gid_t message_id)
{
	ImapFolder* folder;
	unsigned uid;

	if (OpStatus::IsError(GetFolderAndUID(message_id, folder, uid)))
		return 0;

	return folder->GetIndex() ? folder->GetIndex()->GetId() : 0;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::Connect
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::Connect()
{
	return m_default_connection.Connect();
}


/***********************************************************************************
 **
 **
 ** ImapBackend::Disconnect
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::Disconnect(const OpStringC8& why)
{
	// Disconnect open connections
	RETURN_IF_ERROR(m_default_connection.Disconnect(why));
	RETURN_IF_ERROR(m_extra_connection.Disconnect(why));
	m_scheduled_messages.Clear();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RequestDisconnect
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RequestDisconnect()
{
	// Disconnect open connections (delayed, async)
	RETURN_IF_ERROR(m_default_connection.RequestDisconnect());
	RETURN_IF_ERROR(m_extra_connection.RequestDisconnect());

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetSubscribedFolderCount
 **
 ***********************************************************************************/
UINT32 ImapBackend::GetSubscribedFolderCount()
{
	unsigned		count = 0;

	// Check all folders in the list and count the subscribed ones
	for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
	{
		if (it.GetData() && it.GetData()->IsSubscribed())
			count++;
	}

	return count;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetSubscribedFolderName
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetSubscribedFolderName(UINT32 index, OpString& name)
{
	unsigned					count = 0;
	ImapFolder*					folder = NULL;

	// Check all folders in the list and count the subscribed ones, stop when index folders counted
	for (StringHashIterator<ImapFolder> it (m_folders); it && count <= index; it++)
	{
		folder = it.GetData();

		if (folder && folder->IsSubscribed())
			count++;
	}

	// Check if we found the folder
	if (count <= index)
		return OpStatus::ERR;

	// Set name
	RETURN_IF_ERROR(name.Set(folder->GetName16()));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::AddSubscribedFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::AddSubscribedFolder(OpString& path)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByName(path, folder));

	// Subscribe to the folder on server
	return m_default_connection.InsertCommand(OP_NEW(ImapCommands::Subscribe, (folder)));
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RemoveSubscribedFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveSubscribedFolder(UINT32 index)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByIndex(index, folder));

	if (folder != GetFolderByType(AccountTypes::FOLDER_INBOX))
		return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Unsubscribe, (folder)));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::RemoveSubscribedFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveSubscribedFolder(const OpStringC& path)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByName(path, folder));

	if (folder != GetFolderByType(AccountTypes::FOLDER_INBOX))
		return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Unsubscribe, (folder)));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetAllFolders
 **
 ***********************************************************************************/
void ImapBackend::GetAllFolders()
{
	// Check all folders in the list
	for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
	{
		ImapFolder* folder = it.GetData();

		if (folder)
		{
			INT32 subscribed = folder->IsSubscribed() ? 1 : 0;
			GetAccountPtr()->OnFolderAdded(folder->GetName16(), folder->GetName16(), folder->IsSubscribed(), subscribed, folder->IsSelectable() && folder != GetFolderByType(AccountTypes::FOLDER_INBOX));
		}
	}

	OpStatus::Ignore(m_default_connection.InsertCommand(OP_NEW(ImapCommands::List, ())));
}


/***********************************************************************************
 **
 **
 ** ImapBackend::OnMessageSent
 **
 ***********************************************************************************/
void ImapBackend::OnMessageSent(message_gid_t message_id)
{
	// Append this message to the 'sent' folder, if it exists
	ImapFolder* sent_folder;
	if (OpStatus::IsSuccess(m_special_folders.GetData(AccountTypes::FOLDER_SENT, &sent_folder)) && sent_folder)
	{
		// Append message
		GetConnection(sent_folder).InsertCommand(OP_NEW(ImapCommands::AppendSent, (sent_folder, message_id)));
	}
}


/***********************************************************************************
 **
 **
 ** ImapBackend::CreateFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::CreateFolder(OpString& completeFolderPath, BOOL subscribed)
{
	OpString8 path8;
	ImapFolder* folder;

	// Need an 8-bit string of the path
	RETURN_IF_ERROR(ImapFolder::EncodeMailboxName(completeFolderPath, path8));

	// Create the folder in memory
	// TODO Handle delimiters - depends on getting information on parent folder
	RETURN_IF_ERROR(UpdateFolder(path8, 0, 0, FALSE, TRUE));
	RETURN_IF_ERROR(GetFolderByName(completeFolderPath, folder));

	// Create the folder on the server
	return m_default_connection.InsertCommand(OP_NEW(ImapCommands::Create, (folder)));
}


/***********************************************************************************
 **
 **
 ** ImapBackend::DeleteFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::DeleteFolder(OpString& completeFolderPath)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(GetFolderByName(completeFolderPath, folder));

	// Remove folder from server (only selectable folders, see bug #324176)
	if (folder->IsSelectable())
		return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::DeleteMailbox, (folder)));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RenameFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RenameFolder(OpString& oldCompleteFolderPath, OpString& newCompleteFolderPath)
{
	ImapFolder* folder;
	OpString8 newFolderName8;

	RETURN_IF_ERROR(ImapFolder::EncodeMailboxName(newCompleteFolderPath, newFolderName8));

	RETURN_IF_ERROR(GetFolderByName(oldCompleteFolderPath, folder));
	RETURN_IF_ERROR(folder->SetNewName(newFolderName8));

	return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Rename, (folder)));
}


/***********************************************************************************
 ** Check if a backend already has received a certain message
 **
 ** ImapBackend::HasExternalMessage
 ** @param message The message to check for
 ** @return TRUE if the backend has the message, FALSE if we don't know
 ***********************************************************************************/
BOOL ImapBackend::HasExternalMessage(Message& message)
{
	ImapFolder* folder;
	unsigned uid;

	if (OpStatus::IsError(GetFolderAndUID(message.GetInternetLocation(), folder, uid)))
		return FALSE;

	return folder->GetIndex() && m_uid_manager.GetByUID(folder->GetIndex()->GetId(), uid);
}


/***********************************************************************************
 ** Do all actions necessary to insert an 'external' message (e.g. from import) into
 ** this backend
 **
 ** ImapBackend::InsertExternalMessage
 ** @param message Message to insert
 ***********************************************************************************/
OP_STATUS ImapBackend::InsertExternalMessage(Message& message)
{
	ImapFolder* folder;
	unsigned uid;

	// Check folder and uid in this message - return if it doesn't exist
	RETURN_IF_ERROR(GetFolderAndUID(message.GetInternetLocation(), folder, uid));

	// Return error if we already have this message
	if (folder->GetIndex() && m_uid_manager.GetByUID(folder->GetIndex()->GetId(), uid))
		return OpStatus::ERR;

	// Add message to UID index for folder
	RETURN_IF_ERROR(folder->AddUID(uid, message.GetId()));

	// Add message to IMAP folder index if possible
	if (folder->GetIndex())
		RETURN_IF_ERROR(folder->GetIndex()->NewMessage(message.GetId()));

	return OpStatus::OK;
}


/***********************************************************************************
 **If your backend can hold commands, write all commands that it's currently holding
 ** into the offline log. This will be called when the backend is destroyed, makes it
 ** possible to resume after startup
 **
 ** ImapBackend::InsertExternalMessage
 ** @param offline_log Log to write to
 ***********************************************************************************/
OP_STATUS ImapBackend::WriteToOfflineLog(OfflineLog& offline_log)
{
	// Write logs for both connections
	RETURN_IF_ERROR(m_default_connection.WriteToOfflineLog(offline_log));
	RETURN_IF_ERROR(m_extra_connection.WriteToOfflineLog(offline_log));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::InsertMessage
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::InsertMessage(message_gid_t message_id, index_gid_t destination_index)
{
	ImapFolder* folder;

	// Append message to destination_path
	RETURN_IF_ERROR(GetFolderByIndex(destination_index, folder));

	return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Append, (folder, message_id)));
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RemoveMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently)
{
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		ImapFolder* folder;
		unsigned uid;
		message_gid_t message_id = message_ids.Get(i);

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		{
			if (permanently)
			{
				RETURN_IF_ERROR(GetConnection(folder).InsertCommand(
					OP_NEW(ImapCommands::MessageDelete, (folder, uid, uid, GetFolderByType(AccountTypes::FOLDER_TRASH)))));

				if (folder->GetIndex())
					RETURN_IF_ERROR(m_uid_manager.RemoveUID(folder->GetIndexId(), uid));
			}
			else
			{
				ImapCommands::MessageMarkAsDeleted* move_to_trash = OP_NEW(ImapCommands::MessageMarkAsDeleted, (folder, GetFolderByType(AccountTypes::FOLDER_TRASH), uid, uid));
				RETURN_OOM_IF_NULL(move_to_trash);
				RETURN_IF_ERROR(GetConnection(folder).InsertCommand(move_to_trash));
			}
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::RemoveMessage
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveMessage(const OpStringC8& internet_location)
{
	ImapFolder* folder;
	unsigned uid;

	RETURN_IF_ERROR(GetFolderAndUID(internet_location, folder, uid));

	return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::MessageDelete, (folder, uid, uid)));
}


/***********************************************************************************
 **
 **
 ** ImapBackend::MoveMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	ImapFolder *folder_from, *folder_to;

	// Get folders
	RETURN_IF_ERROR(GetFolderByIndex(destination_index_id, folder_to));
	if (OpStatus::IsError(GetFolderByIndex(source_index_id, folder_from)))
		folder_from = NULL;

	// Check if we can do it in one command with a range (only if dragging from the same IMAP folder)
	if (!folder_from)
	{
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			ImapFolder* folder;
			unsigned uid;
			message_gid_t message_id = message_ids.Get(i);
			if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
			{
				RETURN_IF_ERROR(GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Move, (folder, folder_to, uid, uid))));
			}
			else
			{
				RETURN_IF_ERROR(GetConnection(folder_to).InsertCommand(OP_NEW(ImapCommands::Append, (folder_to, message_id))));
			}
		}
		return OpStatus::OK;
	}

	// Local move, use MOVE
	OpAutoPtr<ImapCommands::Move> command (OP_NEW(ImapCommands::Move, (folder_from, folder_to, 0, 0)));
	if (!command.get())
		return OpStatus::ERR_NO_MEMORY;

	// Get UIDs for these messages
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		ImapFolder* folder;
		unsigned uid;
		message_gid_t message_id = message_ids.Get(i);

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
			RETURN_IF_ERROR(command->ExtendSequenceByRange(uid, uid));
	}

	// Execute command
	return GetConnection(folder_from).InsertCommand(command.release());
}


/***********************************************************************************
 **
 **
 ** ImapBackend::CopyMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	ImapFolder *folder_from, *folder_to;

	// Get folders
	RETURN_IF_ERROR(GetFolderByIndex(destination_index_id, folder_to));
	if (OpStatus::IsError(GetFolderByIndex(source_index_id, folder_from)))
		folder_from = NULL;

	// Check if we are doing a 'local' copy
	if (folder_from)
	{
		// Local copy, use COPY
		OpAutoPtr<ImapCommands::Copy> command (OP_NEW(ImapCommands::Copy, (folder_from, folder_to, 0, 0)));
		if (!command.get())
			return OpStatus::ERR_NO_MEMORY;

		// Get UIDs for these messages
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			ImapFolder* folder;
			unsigned uid;
			message_gid_t message_id = message_ids.Get(i);

			if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
				RETURN_IF_ERROR(command->ExtendSequenceByRange(uid, uid));
		}

		// Execute command
		return GetConnection(folder_from).InsertCommand(command.release());
	}
	else
	{
		// Check all messages, they might be from this account
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			unsigned uid;

			if (OpStatus::IsSuccess(GetFolderAndUID(message_ids.Get(i), folder_from, uid)))
				RETURN_IF_ERROR(GetConnection(folder_from).InsertCommand(OP_NEW(ImapCommands::Copy, (folder_from, folder_to, uid, uid))));
			else
				RETURN_IF_ERROR(GetConnection(folder_to).InsertCommand(OP_NEW(ImapCommands::Append, (folder_to, message_ids.Get(i)))));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::ReadMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::ReadMessages(const OpINT32Vector& message_ids, BOOL read)
{
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		ImapFolder* folder;
		unsigned uid;
		int silent = m_account->IsOfflineLogBusy() ? 0 : ImapCommands::Store::SILENT;
		message_gid_t message_id = message_ids.Get(i);

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
			RETURN_IF_ERROR(GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Store, (folder, uid, uid,
				ImapCommands::USE_UID | (read ? ImapCommands::Store::ADD_FLAGS : ImapCommands::Store::REMOVE_FLAGS) | silent | ImapCommands::Store::FLAG_SEEN))));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::FlaggedMessages
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::FlaggedMessage(message_gid_t message_id, BOOL flagged)
{
	ImapFolder* folder;
	unsigned uid;
	int silent = m_account->IsOfflineLogBusy() ? 0 : ImapCommands::Store::SILENT;

	if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		RETURN_IF_ERROR(GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Store, (folder, uid, uid,
			ImapCommands::USE_UID | (flagged ? ImapCommands::Store::ADD_FLAGS : ImapCommands::Store::REMOVE_FLAGS) | silent | ImapCommands::Store::FLAG_FLAGGED))));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::ReplyToMessage
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::ReplyToMessage(message_gid_t message_id)
{
	ImapFolder* folder;
	unsigned uid;
	int silent = m_account->IsOfflineLogBusy() ? 0 : ImapCommands::Store::SILENT;

	if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		return GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Store, (folder, uid, uid,
			ImapCommands::USE_UID | ImapCommands::Store::ADD_FLAGS | silent | ImapCommands::Store::FLAG_ANSWERED)));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::EmptyTrash
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::EmptyTrash(BOOL done_removing_messages)
{
	if (done_removing_messages)
		return OpStatus::OK;

	Index* trash = MessageEngine::GetInstance()->GetIndexById(IndexTypes::TRASH);
	if (!trash)
		return OpStatus::ERR_NULL_POINTER;

	OpPointerSet<ImapFolder> folders;
	ImapFolder* trash_folder;

	if (OpStatus::IsSuccess(m_special_folders.GetData(AccountTypes::FOLDER_TRASH, &trash_folder)) && (trash_folder->GetFetched() > 0))
	{
		// Mark all messages in the trash folder as \deleted
		ImapCommands::Store *mark_as_deleted = OP_NEW(ImapCommands::Store, (trash_folder, 1, 0,
				ImapCommands::Store::ADD_FLAGS | ImapCommands::Store::SILENT | ImapCommands::Store::FLAG_DELETED));

		RETURN_OOM_IF_NULL(mark_as_deleted);
		RETURN_IF_ERROR(GetConnection(trash_folder).InsertCommand(mark_as_deleted));
	}

	for (INT32SetIterator it(trash->GetIterator()); it; it++)
	{
		message_gid_t message_id = it.GetData();
		ImapFolder*   folder;
		unsigned	  uid;

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		{
			// Remove from UID index, we no longer think we have this message
			if (folder->GetIndex())
				RETURN_IF_ERROR(m_uid_manager.RemoveUID(folder->GetIndex()->GetId(), uid));

			// Add an EXPUNGE command, if we don't have one yet for this folder
			if (!folders.Contains(folder))
			{
				RETURN_IF_ERROR(GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::Expunge, (folder))));
				RETURN_IF_ERROR(folders.Add(folder));
			}
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::MessagesMovedFromTrash
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::MessagesMovedFromTrash(const OpINT32Vector& message_ids)
{
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		ImapFolder* folder;
		unsigned uid;
		message_gid_t message_id = message_ids.Get(i);

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		{
			ImapFolder* restore_folder = folder == GetFolderByType(AccountTypes::FOLDER_TRASH)? GetRestoreFolder() : NULL;

			ImapCommands::MessageUndelete *undelete = OP_NEW(ImapCommands::MessageUndelete, 
				(folder, restore_folder, uid, uid));

			RETURN_OOM_IF_NULL(undelete);

			RETURN_IF_ERROR(GetConnection(folder).InsertCommand(undelete));
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::MarkMessagesAsSpam
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL imap_move)
{
	ImapFolder* spam_folder = NULL;
	OpStatus::Ignore(m_special_folders.GetData(AccountTypes::FOLDER_SPAM, &spam_folder));

	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		ImapFolder* folder, *to_folder = NULL;
		unsigned uid;
		message_gid_t message_id = message_ids.Get(i);

		if (OpStatus::IsSuccess(GetFolderAndUID(message_id, folder, uid)))
		{
			if (is_spam)
			{
				to_folder = spam_folder;
			}
			else if (folder == spam_folder)
			{
				to_folder = GetFolderByType(AccountTypes::FOLDER_INBOX);
			}

			RETURN_IF_ERROR(GetConnection(folder).InsertCommand(OP_NEW(ImapCommands::MessageMarkAsSpam, (folder, imap_move? to_folder : NULL, is_spam, uid, uid))));
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderPath
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderPath(AccountTypes::FolderPathType type, OpString& folder_path) const
{
	folder_path.Empty();

	switch (type)
	{
		case AccountTypes::FOLDER_INBOX:
		{
			return folder_path.Set("INBOX");
		}
		case AccountTypes::FOLDER_ROOT:
		{
			OpString8 unquoted_string;
			RETURN_IF_ERROR(OpMisc::UnQuoteString(m_folder_prefix, unquoted_string));
			return ImapFolder::DecodeMailboxName(unquoted_string, folder_path);
		}
		case AccountTypes::FOLDER_SENT:
		case AccountTypes::FOLDER_TRASH:
		case AccountTypes::FOLDER_SPAM:
		case AccountTypes::FOLDER_ALLMAIL:
		{
			ImapFolder* folder;
			if (OpStatus::IsSuccess(m_special_folders.GetData(type, &folder)))
			{
				return folder_path.Set(folder->GetName16().CStr());
			}
		}
		default:
		{
			return OpStatus::OK;
		}
	}
}


/***********************************************************************************
 **
 **
 ** ImapBackend::SetFolderPath
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::SetFolderPath(AccountTypes::FolderPathType type, const OpStringC& folder_path)
{
	switch (type)
	{
		case AccountTypes::FOLDER_ROOT:
		{
			OpString8 encoded_path;
			RETURN_IF_ERROR(ImapFolder::EncodeMailboxName(folder_path, encoded_path));
			RETURN_IF_ERROR(OpMisc::QuoteString(encoded_path, m_folder_prefix));
			break;
		}
		case AccountTypes::FOLDER_SPAM:
		case AccountTypes::FOLDER_TRASH:
		case AccountTypes::FOLDER_SENT:
		{
			ImapFolder* new_folder = NULL;
			ImapFolder* old_folder = NULL;
			OpStatus::Ignore(m_special_folders.GetData(type, &old_folder));

			Indexer*    indexer = MessageEngine::GetInstance()->GetIndexer();

			if (OpStatus::IsError(GetFolderByName(folder_path, new_folder)) && folder_path.HasContent())
			{
				OpString8 encoded_path;
				RETURN_IF_ERROR(ImapFolder::EncodeMailboxName(folder_path, encoded_path));
				OpStatus::Ignore(UpdateFolder(encoded_path, '/', 0, TRUE));
				if (OpStatus::IsSuccess(GetFolderByName(folder_path.CStr(), new_folder)))
					OpStatus::Ignore(new_folder->CreateIndexForFolder());
			}
			
			// Check if it was changed
			if (new_folder != old_folder)
			{
				// Remove the special status from items in the previous special folder
				if (old_folder)
				{
					RETURN_IF_ERROR(m_special_folders.Remove(type, &old_folder));
					RETURN_IF_ERROR(indexer->ChangeSpecialStatus(old_folder->GetIndex(), type, FALSE));
				}

				if (new_folder)
				{
					
					// Set the special status to items in the new special folder
					RETURN_IF_ERROR(m_special_folders.Add(type, new_folder));
	
					if (!new_folder->GetIndex() && new_folder->IsSubscribed())
					{
						RETURN_IF_ERROR(new_folder->CreateIndexForFolder());
					}
					RETURN_IF_ERROR(indexer->ChangeSpecialStatus(new_folder->GetIndex(), type, TRUE));
				}
			}
			break;
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Setup a new account
 **
 ** ImapBackend::OnAccountAdded
 **
 ***********************************************************************************/
void ImapBackend::OnAccountAdded()
{
	// Don't do anything if we're offline
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return;

	// Make sure there is an inbox
	ImapFolder* inbox_folder = NULL;

	if (OpStatus::IsError(GetFolderByName(UNI_L("INBOX"), inbox_folder)))
	{
		OpStatus::Ignore(UpdateFolder("INBOX", '/', 0, TRUE));
		if (OpStatus::IsSuccess(GetFolderByName(UNI_L("INBOX"), inbox_folder)))
			OpStatus::Ignore(inbox_folder->CreateIndexForFolder());
	}
}


/***********************************************************************************
 ** Someone changed settings for this account
 **
 ** ImapBackend::SettingsChanged
 ** @param startup Whether this was during the startup phase
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::SettingsChanged(BOOL startup)
{
	// We do nothing during startup
	if (startup)
		return OpStatus::OK;

	// Save changes to disk
	RETURN_IF_ERROR(WriteConfiguration(TRUE));

	// Disconnect, the changes might need a reconnect
	return Disconnect();
}


/***********************************************************************************
 **
 **
 ** ImapBackend::SyncFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::SyncFolder(ImapFolder* folder)
{
	return GetConnection(folder).SyncFolder(folder);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::UpdateFolder
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::UpdateFolder(const OpStringC8& name, char delimiter, int list_flags, BOOL is_lsub, BOOL no_sync)
{
	ImapFolder* folder;

	if (OpStatus::IsError(GetFolderByName(name, folder)))
	{
		// Folder doesn't exist yet, we'll have to create it
		RETURN_IF_ERROR(ImapFolder::Create(folder, *this, name, delimiter, 0));

		GetAccountPtr()->OnFolderAdded(folder->GetName16(), folder->GetName16(), is_lsub, is_lsub, folder->IsSelectable() && folder != GetFolderByType(AccountTypes::FOLDER_INBOX));
	}

	if (is_lsub && !folder->IsSubscribed())
		folder->SetSubscribed(TRUE);

	folder->SetDelimiter(delimiter);
	folder->SetListFlags(list_flags);

	if (!is_lsub)
	{
		// This folder has been confirmed to exist
		RETURN_IF_ERROR(m_confirmed_folders.Insert(folder));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove a folder from all possible local storage - M2 will forget about this folder
 **
 ** ImapBackend::RemoveFolder
 ** @param folder Folder to remove
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveFolder(ImapFolder* folder)
{
	ImapFolder* removed_folder;

	// Notify that folder has been removed
	m_account->OnFolderRemoved(folder->GetName16());

	// This can't be the current folder
	if (m_selected_folder == folder)
		m_selected_folder = NULL;

	for (INT32 i = AccountTypes::FOLDER_INBOX; i <= AccountTypes::FOLDER_ALLMAIL; i++)
	{
		ImapFolder* to_delete;
		if (OpStatus::IsSuccess(m_special_folders.GetData(i, &to_delete)) && to_delete == folder)
		{
			RETURN_IF_ERROR(m_special_folders.Remove(i, &to_delete));
			break;
		}

	}
	// Remove folder from all indexes
	RETURN_IF_ERROR(folder->MarkUnselectable());

	m_default_connection.RemoveFolder(folder);
	m_extra_connection.RemoveFolder(folder);

	RETURN_IF_ERROR(m_folders.Remove(folder->GetName16().CStr(), &removed_folder));
	OP_ASSERT(removed_folder == folder);
	// Delete folder pointer
	OP_DELETE(removed_folder);

	return OpStatus::OK;
}

/***********************************************************************************
 ** Remove a folder from all possible local storage - M2 will forget about this folder
 **
 ** ImapBackend::RemoveFolder
 ** @param folder Folder to remove
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::RemoveFolderFromIMAPBackend(ImapFolder* folder)
{
	ImapFolder* removed_folder;
	RETURN_IF_ERROR(m_folders.Remove(folder->GetName16().CStr(), &removed_folder));
	OP_ASSERT(removed_folder == folder);
	return OpStatus::OK;
}


/***********************************************************************************
 ** The connection should call this function when unexpectantly disconnected
 **
 ** ImapBackend::OnUnexpectedDisconnect
 ** @param connection The connection that was disconnected
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::OnUnexpectedDisconnect(IMAP4& connection)
{
	if (&connection == &m_extra_connection && !connection.IsAuthenticated())
	{
		// The extra connection was disconnected before it was authenticated,
		// which probably means we can't use two connections.
		// Let it lay low; transfer all its folders and queue to the default connection
		RETURN_IF_ERROR(m_default_connection.TakeOverCompletely(m_extra_connection));
		m_options |= ImapFlags::OPTION_EXTRA_CONNECTION_FAILS;
	}
	else
	{
		if (m_connect_retries < MaxConnectRetries)
		{
			RETURN_IF_ERROR(Disconnect("The default connection was unexpectedly interrupted. Reconnect all"));

			// Try connecting again
			g_main_message_handler->PostMessage(MSG_M2_CONNECT, (MH_PARAM_1)this, 0, ConnectRetryDelay);

			m_connect_retries++;
		}
		else
		{
			RETURN_IF_ERROR(Disconnect("The default connection was unexpectedly interrupted. Reconnect failed. Disconnect all"));

			m_connect_retries = 0;

			OpString errormsg;
			g_languageManager->GetString(Str::D_MAIL_IMAP_DISCONNECTED, errormsg);
			OnError(errormsg);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** If folder is selected by one of the connections, return that connection
 **
 ** ImapBackend::SelectedByConnection\
 ** @param folder The folder to check
 ** @return Connection that has folder selected, or NULL if none
 **
 ***********************************************************************************/
IMAP4* ImapBackend::SelectedByConnection(ImapFolder* folder)
{
	if (m_default_connection.IsConnected() && m_default_connection.GetCurrentFolder() == folder)
		return &m_default_connection;
	if (m_extra_connection.IsConnected() && m_extra_connection.GetCurrentFolder() == folder)
		return &m_extra_connection;

	return NULL;
}


/***********************************************************************************
 ** Generate a new tag for a command sent over this account
 **
 ** ImapBackend::GenerateNewTag
 ** @param new_tag Where to save the tag
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GenerateNewTag(OpString8& new_tag)
{
	char* new_tag_str = new_tag.Reserve(ImapTagLength);

	if (!new_tag_str)
		return OpStatus::ERR_NO_MEMORY;

	new_tag_str[ImapTagLength] = '\0';

	//For now, the format is ImapTagLength character BASE36, which can hold 36^4 = 1.679.616 different tags when it is 4.
	//A tag is defined as "1*<any ASTRING-CHAR except '+'>", so feel free to change it if 36^4 is insufficient
	unsigned int temp_tag_seed = m_tag_seed;
	for (int i = ImapTagLength - 1; i >= 0; i--)
	{
		new_tag_str[i] = '0'+(temp_tag_seed%36);
		if (new_tag_str[i] > '9')
			new_tag_str[i] += ('A'-'9'-1);

		temp_tag_seed /= 36;
	}

	m_tag_seed++;
	return OpStatus::OK;
}


/***********************************************************************************
 ** Add a folder to this IMAP account's internal list of folders
 **
 ** ImapBackend::AddFolder
 ** @param folder The folder to add
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::AddFolder(ImapFolder* folder)
{
	RETURN_IF_ERROR(m_folders.Add(folder->GetName16().CStr(), folder));

	// Set the special INBOX folder
	if (!folder->GetName().CompareI("INBOX"))
	{
		RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_INBOX, folder));
		RETURN_IF_ERROR(m_default_connection.AddFolder(folder));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove all namespace data that we have
 **
 ** IMAP4::ResetNamespaces
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::ResetNamespaces()
{
	m_namespaces[ImapNamespace::PERSONAL].DeleteAll();
	m_namespaces[ImapNamespace::OTHER].DeleteAll();
	m_namespaces[ImapNamespace::SHARED].DeleteAll();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add namespace for a specific namespace type
 **
 ** IMAP4::AddNamespace
 ** @param type Namespace type
 ** @param prefix Namespace prefix
 ** @param delimiter Delimiter used for this namespace
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::AddNamespace(ImapNamespace::NamespaceType type, const OpStringC8& prefix, char delimiter)
{
	OpAutoPtr<ImapNamespace> namespace_desc (OP_NEW(ImapNamespace, ()));
	if (!namespace_desc.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(namespace_desc->SetNamespace(prefix, delimiter));
	RETURN_IF_ERROR(m_namespaces[type].Add(namespace_desc.get()));
	namespace_desc.release();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetConnection
 **
 ***********************************************************************************/
IMAP4& ImapBackend::GetConnection(ImapFolder* folder)
{
	// First see if any of the connections claim this folder as their own
	if (m_default_connection.HasMailbox(folder))
		return m_default_connection;
	if (m_extra_connection.HasMailbox(folder))
		return m_extra_connection;

	// The default connection is only for the \AllMail folder or if that is not present the INBOX folder
	ImapFolder* allmail_folder = GetFolderByType(AccountTypes::FOLDER_ALLMAIL);

	// Try to use the extra connection if it's available for other folders
	if (IsExtraAvailable() && ((allmail_folder && allmail_folder != folder) || 
							   (!allmail_folder && folder != GetFolderByType(AccountTypes::FOLDER_INBOX))))
	{
		OpStatus::Ignore(m_extra_connection.AddFolder(folder));
		return m_extra_connection;
	}
	else
	{
		OpStatus::Ignore(m_default_connection.AddFolder(folder));
		return m_default_connection;
	}
}


/***********************************************************************************
 ** Check whether a UID map should be maintained for a certain folder
 **
 ** ImapBackend::ShouldMaintainUIDMap
 ** @param folder Folder to check
 ***********************************************************************************/
BOOL ImapBackend::ShouldMaintainUIDMap(ImapFolder* folder)
{
	// We don't maintain UID maps in low bandwidth mode
	if (GetAccountPtr()->GetLowBandwidthMode())
		return FALSE;

	// We don't maintain UID maps when we can use QRESYNC
	if (GetConnection(folder).GetCapabilities() & ImapFlags::CAP_QRESYNC)
		return FALSE;

	return TRUE;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetRestoreFolder
 **
 ***********************************************************************************/
ImapFolder* ImapBackend::GetRestoreFolder()
{
	// if there is an AllMail folder, move undeleted there, otherwise move to INBOX
	ImapFolder* restore_folder;
	if (OpStatus::IsError(m_special_folders.GetData(AccountTypes::FOLDER_ALLMAIL, &restore_folder)))
	{
		if (OpStatus::IsError(m_special_folders.GetData(AccountTypes::FOLDER_INBOX, &restore_folder)))
			return NULL;
	}
	return restore_folder;
}

/***********************************************************************************
 ** @return configuration file for this account
 **
 ** ImapBackend::GetCurrentPrefsFile
 **
 ***********************************************************************************/
PrefsFile* ImapBackend::GetCurrentPrefsFile()
{
	if (!m_prefsfile)
		m_prefsfile = GetPrefsFile();

	return m_prefsfile;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderByType
 **
 ***********************************************************************************/
ImapFolder*	ImapBackend::GetFolderByType(AccountTypes::FolderPathType type) 
{ 
	ImapFolder* folder; 
	if (OpStatus::IsSuccess(m_special_folders.GetData(type, &folder)))
		return folder; 
	else 
		return NULL; 
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderByName
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderByName(const OpStringC8& name, ImapFolder*& folder) const
{
	OpString16 tmp_string;

	RETURN_IF_ERROR(ImapFolder::DecodeMailboxName(name, tmp_string));

	return GetFolderByName(tmp_string, folder);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderByName
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderByName(const OpStringC& name, ImapFolder*& folder) const
{
	folder = NULL;

	if (name.HasContent())
		return m_folders.GetData(name.CStr(), &folder);
	else
		return OpStatus::ERR;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderByIndex
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderByIndex(index_gid_t index_id, ImapFolder*& folder) const
{
	Index*	 requested_index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(index_id);

	if (!requested_index)
		return OpStatus::ERR_NULL_POINTER;

	IndexSearch* search = requested_index->GetSearch();
	if (!search)
		return OpStatus::ERR_NULL_POINTER;

	// Retrieve folder by folder name
	RETURN_IF_ERROR(GetFolderByName(search->GetSearchText(), folder));
	if (folder->GetIndex() == requested_index)
		return OpStatus::OK;
	else 
		return OpStatus::ERR;
}


/***********************************************************************************
 ** Call when a message has been copied to a different mailbox and received
 ** a new UID (to ensure appearance in correct indexes)
 **
 ** ImapBackend::OnMessageCopied
 ** @param source_message Message that was copied
 ** @param dest_folder Folder that message was copied to
 ** @param dest_uid UID that message received in dest_folder
 ***********************************************************************************/
OP_STATUS ImapBackend::OnMessageCopied(message_gid_t source_message, ImapFolder* dest_folder, unsigned dest_uid, BOOL replace)
{
	if (!dest_folder->GetIndex())
		return OpStatus::OK;

	Message   message;
	OpString8 internet_location;

	// Get details for message
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, source_message));

	// Set correct internet location for message
	RETURN_IF_ERROR(dest_folder->GetInternetLocation(dest_uid, internet_location));
	RETURN_IF_ERROR(message.SetInternetLocation(internet_location));

	// If we are replacing, just update the message; else create a new message
	if (replace)
	{
		// Simply replace the information in the message
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->UpdateMessage(message));
	}
	else
	{
		// Copy message
		GetAccountPtr()->InternalCopyMessage(message);
	}

	// Add UID to UID manager
	RETURN_IF_ERROR(dest_folder->AddUID(dest_uid, message.GetId()));

	// Put in IMAP folder
	RETURN_IF_ERROR(dest_folder->GetIndex()->NewMessage(message.GetId()));

	// Let IMAP folder know there's an extra item
	RETURN_IF_ERROR(dest_folder->SetExists(dest_folder->GetExists() + 1, FALSE));

	// Request a full synchronization (since list of UIDs/server ids needs to be updated)
	dest_folder->RequestFullSync();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove folders that are not in the confirmed list
 **
 ** ImapBackend::ProcessConfirmedFolderList
 ***********************************************************************************/
OP_STATUS ImapBackend::ProcessConfirmedFolderList()
{
	OpVector<ImapFolder> todelete;

	// Iterate through the folders in our list, see if they exist
	for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
	{
		ImapFolder* folder = it.GetData();

		if (folder != GetFolderByType(AccountTypes::FOLDER_INBOX) && !folder->IsSubscribed() && m_confirmed_folders.Find(folder) == -1)
		{
			// Folder hasn't been confirmed, remove the folder
			// NOTE: it's unsafe to remove from a hash while
			// iterating (CORE-38422) so build a list instead
			RETURN_IF_ERROR(todelete.Add(folder));
		}
	}

	// Remove unconfirmed folders
	for (UINT32 i = 0; i < todelete.GetCount(); i++)
	{
		RETURN_IF_ERROR(RemoveFolder(todelete.Get(i)));
	}

	m_confirmed_folders.Clear();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add a progress group item
 **
 ** ImapBackend::NewProgress
 ***********************************************************************************/
void ImapBackend::NewProgress(ProgressInfo::ProgressAction group, int total_increase)
{
	m_progress_groups[group].user_count += 1;
	IncreaseProgressTotal(group, total_increase);
}


/***********************************************************************************
 ** Increase progress total without changing users
 **
 ** ImapBackend::NewProgress
 ***********************************************************************************/
void ImapBackend::IncreaseProgressTotal(ProgressInfo::ProgressAction group, int total_increase)
{
	m_progress_groups[group].total += total_increase;
}


/***********************************************************************************
 ** Update a progress group
 **
 ** ImapBackend::UpdateProgressGroup
 ***********************************************************************************/
void ImapBackend::UpdateProgressGroup(ProgressInfo::ProgressAction group, int done_increase, BOOL propagate)
{
	m_progress_groups[group].done += done_increase;

	// Update current progress if necessary
	if (propagate)
	{
		m_progress.SetCurrentAction(group, m_progress_groups[group].total);
		m_progress.SetCurrentActionProgress(m_progress_groups[group].done, m_progress_groups[group].total);
	}
}


/***********************************************************************************
 ** Remove a progress group item
 **
 ** ImapBackend::ProgressDone
 ***********************************************************************************/
void ImapBackend::ProgressDone(ProgressInfo::ProgressAction group)
{
	if (m_progress_groups[group].user_count == 0)
		return;

	m_progress_groups[group].user_count--;

	// Check if this was the last user
	if (m_progress_groups[group].user_count == 0)
	{
		m_progress_groups[group].done = 0;
		m_progress_groups[group].total = 0;

		switch (group)
		{
			// If we were fetching headers/messages, display a notification
			case ProgressInfo::FETCHING_MESSAGES:
			case ProgressInfo::FETCHING_HEADERS:
				MessageEngine::GetInstance()->GetMasterProgress().NotifyReceived(GetAccountPtr());
				break;
		}
	}
}


/***********************************************************************************
 ** Reset
 **
 ** ImapBackend::ResetProgress
 ***********************************************************************************/
void ImapBackend::ResetProgress()
{
	m_progress.EndCurrentAction(!m_default_connection.IsConnected() && !m_extra_connection.IsConnected());

	if (!m_default_connection.IsBusy() && !m_extra_connection.IsBusy())
	{
		op_memset(m_progress_groups, 0, sizeof(m_progress_groups));
	}
}


/***********************************************************************************
 ** Called when system goes into sleep mode
 **
 ** ImapBackend::OnSleep
 ***********************************************************************************/
void ImapBackend::OnSleep()
{
	if (m_default_connection.IsConnected() || m_extra_connection.IsConnected())
	{
		m_connected_on_sleep = true;
		OpStatus::Ignore(Disconnect("Standby mode"));
	}
}


/***********************************************************************************
 ** Called when system comes out of sleep mode
 **
 ** ImapBackend::OnWakeUp
 ***********************************************************************************/
void ImapBackend::OnWakeUp()
{
	if (m_connected_on_sleep)
	{
		m_connected_on_sleep = false;
		OpStatus::Ignore(FetchMessages(FALSE));
	}
}


/***********************************************************************************
 **
 **
 ** ImapBackend::HandleCallback
 ***********************************************************************************/
void ImapBackend::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_M2_CONNECT)
		Connect();
}

/***********************************************************************************
 **
 **
 ** ImapBackend::SetAllMailFolder
 ***********************************************************************************/
OP_STATUS ImapBackend::SetAllMailFolder(ImapFolder* allmail_folder)
{
	// maybe we have done this already?
	if (GetFolderByType(AccountTypes::FOLDER_ALLMAIL) == allmail_folder || !allmail_folder->IsSubscribed())
		return OpStatus::OK;

	// hide all other IMAP folders from the generic views
	for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
	{
		ImapFolder* folder = it.GetData();
		if (folder && folder != allmail_folder && folder != GetFolderByType(AccountTypes::FOLDER_SPAM) && folder->IsSubscribed())
		{
			if (!folder->GetIndex())
				RETURN_IF_ERROR(folder->CreateIndexForFolder());

			folder->GetIndex()->SetHideFromOther(TRUE);
			RETURN_IF_ERROR(MessageEngine::GetInstance()->UpdateIndex(folder->GetIndexId()));
		}
	}
	
	// set special attributes to the all mail folder
	if (allmail_folder->GetIndex())
		allmail_folder->GetIndex()->SetHideFromOther(FALSE);
	ImapFolder* old_inbox;
	if (OpStatus::IsSuccess(m_special_folders.GetData(AccountTypes::FOLDER_INBOX, &old_inbox)))
	{
		if (IsExtraAvailable())
		{
			RETURN_IF_ERROR(m_extra_connection.AddFolder(old_inbox));
			m_default_connection.RemoveFolder(old_inbox);
		}
	}
	

	// the all mail folder is specific to gmail and we know it has server spam filtering
	m_account->SetServerHasSpamFilter(TRUE);

	RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_ALLMAIL, allmail_folder));
	RETURN_IF_ERROR(m_default_connection.AddFolder(allmail_folder));
	m_extra_connection.RemoveFolder(allmail_folder);
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderAndUID
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderAndUID(const OpStringC8& internet_location, ImapFolder*& folder, unsigned& uid) const
{
	OpString8 folder_name;

	// Find the delimiter - format: FOLDERNAME:UID
	int pos = internet_location.FindLastOf(':');
	if (pos == KNotFound)
		return OpStatus::ERR;

	// Split out the parts
	RETURN_IF_ERROR(folder_name.Set(internet_location.CStr(), pos));
	uid = op_atoi(internet_location.CStr() + pos + 1);

	if (uid == 0)
		return OpStatus::ERR;

	return GetFolderByName(folder_name, folder);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::GetFolderAndUID
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::GetFolderAndUID(message_gid_t message_id, ImapFolder*& folder, unsigned& uid) const
{
	if (MessageEngine::GetInstance()->GetStore()->GetMessageAccountId(message_id) != m_account->GetAccountId())
		return OpStatus::ERR;
	
	// Get internet location for this message
	OpString8 internet_location;
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageInternetLocation(message_id, internet_location));

	return ImapBackend::GetFolderAndUID(internet_location, folder, uid);
}


/***********************************************************************************
 **
 **
 ** ImapBackend::ReadConfiguration
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::ReadConfiguration()
{
	// Check for prefs file
	if (!m_prefsfile)
		m_prefsfile = GetPrefsFile();

	// Read Configuration
	OpStringC8 settings_section("Settings");
	OpString  sent_folder, spam_folder, trash_folder, all_mail_folder;
	OpString  folder_prefix;
	BOOL 	  force_single_connection = FALSE, disable_qresync_support = FALSE, disable_uidplus_support = FALSE, disable_special_use_support = FALSE, disable_compress_support = FALSE;

	// Load file
	RETURN_IF_LEAVE(
		m_prefsfile->LoadAllL(); 
		m_old_version = m_prefsfile->ReadIntL(settings_section, "Version", ConfigVersion));

	// Only version 2 or higher is supported
	if (m_old_version >= 2 && m_old_version <= ConfigVersion)
	{
		// Read generic settings
		RETURN_IF_LEAVE(
			force_single_connection	= m_prefsfile->ReadIntL(settings_section, "Force Single Connection", 	  m_account->GetForceSingleConnection()); 
			m_prefsfile->ReadStringL(settings_section, "Sent", sent_folder, UNI_L("")); 
			m_prefsfile->ReadStringL(settings_section, "Spam", spam_folder, UNI_L("")); 
			m_prefsfile->ReadStringL(settings_section, "Trash", trash_folder, UNI_L("")); 
			m_prefsfile->ReadStringL(settings_section, "All Mail", all_mail_folder, UNI_L("")); 
			m_prefsfile->ReadStringL(settings_section, "Root", folder_prefix, UNI_L(""));
			disable_qresync_support = m_prefsfile->ReadIntL(settings_section, "Disable QRESYNC", !m_account->IsQRESYNCEnabled()); 
			disable_uidplus_support = m_prefsfile->ReadIntL(settings_section, "Disable UIDPLUS", FALSE);
			disable_special_use_support = m_prefsfile->ReadIntL(settings_section, "Disable SPECIAL-USE", FALSE);
			disable_compress_support = m_prefsfile->ReadIntL(settings_section, "Disable COMPRESS", FALSE);
			);

		// Set folder prefix
		RETURN_IF_ERROR(SetFolderPath(AccountTypes::FOLDER_ROOT, folder_prefix));

		// IMAP connection flags
		if (force_single_connection)
			m_options |= ImapFlags::OPTION_FORCE_SINGLE_CONNECTION;
		if (!m_account->GetImapIdle())
			m_options |= ImapFlags::OPTION_DISABLE_IDLE;
		if (!m_account->GetImapBodyStructure())
			m_options |= ImapFlags::OPTION_DISABLE_BODYSTRUCTURE;
		if (disable_qresync_support)
			m_options |= ImapFlags::OPTION_DISABLE_QRESYNC;
		if (disable_uidplus_support)
			m_options |= ImapFlags::OPTION_DISABLE_UIDPLUS;
		if (disable_special_use_support)
			m_options |= ImapFlags::OPTION_DISABLE_SPECIAL_USE;
		if (disable_compress_support)
			m_options |= ImapFlags::OPTION_DISABLE_COMPRESS;

		// TODO Read namespaces

		// Read folder sections
		OpString_list section_list;
		RETURN_IF_LEAVE(m_prefsfile->ReadAllSectionsL(section_list));

		for (unsigned long i = 0; i < section_list.Count(); i++)
		{
			if (!section_list[i].Compare("Folder", 6))
			{
				OpString  name, name_space, mod_seq;
				char      delimiter;
				unsigned       uid_validity, uid_next, exists;
				long long last_known_mod_seq = 0;

				RETURN_IF_LEAVE(
										 m_prefsfile->ReadStringL(section_list[i], UNI_L("Namespace"), name_space,	UNI_L("")); 
										 m_prefsfile->ReadStringL(section_list[i], UNI_L("Folder"), name, 			UNI_L("")); 
										 m_prefsfile->ReadStringL(section_list[i], UNI_L("Last Known Mod Seq"), mod_seq, UNI_L("0")); 
					delimiter		   = m_prefsfile->ReadIntL   (section_list[i], UNI_L("Separator"), 				'/'); 
					uid_validity	   = m_prefsfile->ReadIntL   (section_list[i], UNI_L("UID Validity"), 			0); 
					uid_next		   = m_prefsfile->ReadIntL   (section_list[i], UNI_L("UID Next"),				0); 
					exists			   = m_prefsfile->ReadIntL   (section_list[i], UNI_L("Exists"), 				INT_MAX); 
							   );
				
				// Read 64-bit mod sequence
				if (mod_seq.CStr())
					uni_sscanf(mod_seq.CStr(), UNI_L("%Ld"), &last_known_mod_seq);

				if (name.HasContent())
				{
					ImapFolder* new_folder;

					if (m_old_version == 2 && name_space.HasContent() && name.Compare("INBOX"))
					{
						// Old versions had namespace and folders confused. Fuse them together, except INBOX
						if (name_space[name_space.Length() - 1] != delimiter)
							RETURN_IF_ERROR(name_space.Append(&delimiter, 1));
						RETURN_IF_ERROR(name.Insert(0, name_space));
					}

					// Create folder
					if (OpStatus::IsSuccess(ImapFolder::Create(new_folder, *this, name, delimiter, exists, uid_next, uid_validity, last_known_mod_seq)))
					{
						// Make sure one of the connections handles the folder
						GetConnection(new_folder);
						// Check if this is the 'sent' folder
						if (name.Compare(sent_folder) == 0)
							RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_SENT, new_folder));
						else if (name.Compare(trash_folder) == 0)
							RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_TRASH, new_folder));
						else if (name.Compare(spam_folder) == 0)
							RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_SPAM, new_folder));
						else if (name.Compare(all_mail_folder) == 0)
						{
							ImapFolder* old_inbox;
							if (OpStatus::IsSuccess(m_special_folders.GetData(AccountTypes::FOLDER_INBOX, &old_inbox)))
							{
								if (OpStatus::IsSuccess(m_extra_connection.AddFolder(old_inbox)))
									m_default_connection.RemoveFolder(old_inbox);
							}
							RETURN_IF_ERROR(new_folder->SetSubscribed(TRUE, FALSE));
							RETURN_IF_ERROR(m_special_folders.Add(AccountTypes::FOLDER_ALLMAIL, new_folder));
							RETURN_IF_ERROR(m_default_connection.AddFolder(new_folder));
							m_extra_connection.RemoveFolder(new_folder);
						}
					}
				}
			}
		}

		// upgrade the sent folder index
		if (m_old_version <= 4 && GetFolderByType(AccountTypes::FOLDER_SENT))
		{
			ImapFolder* sent_folder = GetFolderByType(AccountTypes::FOLDER_SENT);
			if (sent_folder && sent_folder->GetIndex())
				sent_folder->GetIndex()->SetSpecialUseType(AccountTypes::FOLDER_SENT);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapBackend::WriteConfiguration
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::WriteConfiguration(BOOL commit)
{
	// Check for prefs file
	if (!m_prefsfile)
		return OpStatus::ERR_NULL_POINTER;

	// Convert folder prefix
	OpString folder_prefix;
	RETURN_IF_ERROR(GetFolderPath(AccountTypes::FOLDER_ROOT, folder_prefix));

	ImapFolder *spam_folder, *trash_folder, *allmail_folder, *sent_folder;
	spam_folder = GetFolderByType(AccountTypes::FOLDER_SPAM);
	trash_folder = GetFolderByType(AccountTypes::FOLDER_TRASH);
	allmail_folder = GetFolderByType(AccountTypes::FOLDER_ALLMAIL);
	sent_folder = GetFolderByType(AccountTypes::FOLDER_SENT);

	// Generic settings
	RETURN_IF_LEAVE(
		m_prefsfile->DeleteAllSectionsL();
		m_prefsfile->WriteIntL("Settings", "Version", ConfigVersion);
		m_prefsfile->WriteIntL("Settings", "Force Single Connection",
							   (m_options & ImapFlags::OPTION_FORCE_SINGLE_CONNECTION) ? TRUE : FALSE);
		m_prefsfile->WriteStringL("Settings", "Root", folder_prefix);
		m_prefsfile->WriteStringL("Settings", "Spam", spam_folder ? spam_folder->GetName16().CStr() : UNI_L(""));
		m_prefsfile->WriteStringL("Settings", "Trash", trash_folder ? trash_folder->GetName16().CStr() : UNI_L(""));
		m_prefsfile->WriteStringL("Settings", "All Mail", allmail_folder ? allmail_folder->GetName16().CStr() : UNI_L("")); 
		m_prefsfile->WriteStringL("Settings", "Sent", sent_folder ? sent_folder->GetName16().CStr() : UNI_L(""));
		m_prefsfile->WriteIntL("Settings", "Disable QRESYNC",
							   (m_options & ImapFlags::OPTION_DISABLE_QRESYNC) ? TRUE : FALSE);
		m_prefsfile->WriteIntL("Settings", "Disable UIDPLUS",
							   (m_options & ImapFlags::OPTION_DISABLE_UIDPLUS) ? TRUE : FALSE);
		m_prefsfile->WriteIntL("Settings", "Disable SPECIAL-USE",
							   (m_options & ImapFlags::OPTION_DISABLE_SPECIAL_USE) ? TRUE : FALSE);
		m_prefsfile->WriteIntL("Settings", "Disable COMPRESS",
							   (m_options & ImapFlags::OPTION_DISABLE_COMPRESS) ? TRUE : FALSE));

	// TODO Namespaces

	// Folders
	for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
	{
		// Save folder, actual commit follows later
		if (it.GetData())
			RETURN_IF_ERROR(it.GetData()->SaveToFile());
	}

	if (commit)
		RETURN_IF_LEAVE(m_prefsfile->CommitL());

	return OpStatus::OK;
}


/***********************************************************************************
 ** Do updates associated with old configuration files
 **
 ** ImapBackend::UpdateStore
 **
 ***********************************************************************************/
OP_STATUS ImapBackend::UpdateStore(int old_version)
{
	if (m_old_version == 2)
	{
		// Check all folders in the list
		for (StringHashIterator<ImapFolder> it (m_folders); it; it++)
		{
			ImapFolder* folder  = it.GetData();
			Index* folder_index = folder->GetIndex();

			if (!folder_index)
				continue;

			// Update UID index
			RETURN_IF_ERROR(folder->InsertUIDs());

			// Update folder indexes (nested folders) if we have a delimiter
			if (!folder->GetDelimiter())
			{
				// Sometimes the delimiter in merlin is broken; try the INBOX delimiter instead
				ImapFolder* inbox_folder = GetFolderByType(AccountTypes::FOLDER_INBOX);
				if (inbox_folder && inbox_folder->GetDelimiter())
					folder->SetDelimiter(inbox_folder->GetDelimiter());
				else
					continue;
			}

			int pos = folder->GetName16().FindLastOf(folder->GetDelimiter());

			if (pos == KNotFound)
				continue;

			OpString parent_path, parent_name;
			Index* parent_index;

			// Determine parent path and name of folder
			RETURN_IF_ERROR(parent_path.Set(folder->GetName16().CStr(), pos));
			pos = parent_path.FindLastOf(folder->GetDelimiter());
			RETURN_IF_ERROR(parent_name.Set(pos == KNotFound ? parent_path.CStr() : parent_path.CStr() + pos + 1));

			parent_index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(
							m_account, parent_path, folder->GetDelimiter(), parent_name, TRUE, FALSE);

			if (parent_index && folder_index)
			{
				folder_index->SetParentId(parent_index->GetId());
				RETURN_IF_ERROR(folder_index->SetName(folder->GetDisplayName()));
			}
		}
	}

	return OpStatus::OK;
}


#endif //M2_SUPPORT
