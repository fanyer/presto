/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/imap-folder.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"
#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/qp.h"

#include "modules/encodings/decoders/utf7-decoder.h"
#include "modules/encodings/encoders/utf7-encoder.h"


/***********************************************************************************
 **
 **
 ** ImapFolder::ImapFolder
 **
 ***********************************************************************************/
ImapFolder::ImapFolder(ImapBackend& backend, unsigned exists, unsigned uid_next, unsigned uid_validity, UINT64 last_known_mod_seq, index_gid_t index_id)
  : m_scheduled_for_sync(0)
  , m_exists(exists)
  , m_uid_next(uid_next)
  , m_uid_validity(uid_validity)
  , m_last_known_mod_seq(last_known_mod_seq)
  , m_next_mod_seq_after_sync(0)
  , m_unseen(0)
  , m_recent(0)
  , m_delimiter('/')
  , m_settable_flags(0)
  , m_permanent_flags(0)
  , m_readonly(FALSE)
  , m_last_full_sync(0)
  , m_index_id(index_id)
  , m_subscribed(index_id > 0)
  , m_selectable(TRUE)
  , m_backend(backend)
{
}


/***********************************************************************************
 **
 **
 ** ImapFolder::Create
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::Create(ImapFolder*& folder, ImapBackend& backend, const OpStringC8& name, char delimiter, int flags, unsigned exists, unsigned uid_next, unsigned uid_validity, UINT64 last_known_mod_seq)
{
	folder = 0;

	OpAutoPtr<ImapFolder> new_folder (OP_NEW(ImapFolder, (backend, exists, uid_next, uid_validity, last_known_mod_seq, 0)));
	if (!new_folder.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_folder->SetName(name, delimiter));
	RETURN_IF_ERROR(backend.AddFolder(new_folder.get()));

	new_folder->SetFlags(flags, FALSE);
	if (!name.CompareI("INBOX"))
		new_folder->m_subscribed = TRUE;

	folder = new_folder.release();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::Create
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::Create(ImapFolder*& folder, ImapBackend& backend, const OpStringC& name, char delimiter, unsigned exists, unsigned uid_next, unsigned uid_validity, UINT64 last_known_mod_seq)
{
	folder = 0;

	OpAutoPtr<ImapFolder> new_folder (OP_NEW(ImapFolder, (backend, exists, uid_next, uid_validity, last_known_mod_seq, 0)));
	if (!new_folder.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_folder->SetName(name, delimiter));
	RETURN_IF_ERROR(backend.AddFolder(new_folder.get()));

	if (!name.CompareI("INBOX"))
		new_folder->m_subscribed = TRUE;

	folder = new_folder.release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** To be called when this folder is opened and server signals expunged message
 **
 ** ImapFolder::Expunge
 ** @param expunged_message_id The id of the expunged message as signaled by server
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::Expunge(int expunged_message_id)
{
	// Check if we have a UID map
	if (!m_backend.ShouldMaintainUIDMap(this))
		return OpStatus::OK;

	message_gid_t message_id = GetMessageByServerID(expunged_message_id);

	// Get and remove message from local storage
	if (message_id > 0)
		RETURN_IF_ERROR(MessageEngine::GetInstance()->OnDeleted(message_id, TRUE));

	// Remove message from general UID index
	RETURN_IF_ERROR(m_backend.GetUIDManager().RemoveUID(m_index_id, m_uid_map.GetByIndex(expunged_message_id - 1)));

	// Remove message from this mailbox's UID map
	RETURN_IF_ERROR(m_uid_map.RemoveByIndex(expunged_message_id - 1, 1));
	m_exists--;

	return OpStatus::OK;
}


/***********************************************************************************
 ** To be called when this folder is opened and server signals vanished messages
 **
 ** ImapFolder::Vanished
 **  @param vanished_uids UIDs of vanished messages
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::Vanished(const OpINT32Vector& vanished_uids)
{
	for (unsigned i = 0; i < vanished_uids.GetCount(); i++)
	{
		RETURN_IF_ERROR(m_backend.GetUIDManager().RemoveMessagesByUID(m_index_id, vanished_uids.Get(i), vanished_uids.Get(i) + 1));
		m_exists--;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetMessageUID
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetMessageUID(unsigned server_id, unsigned uid)
{
	// Check if we need this information
	if (!m_backend.ShouldMaintainUIDMap(this))
		return OpStatus::OK;

	// server_id is one-based, we are zero-based
	server_id--;

	int current_index = m_uid_map.Find(uid);

	if (current_index < 0)
	{
		// UID not found, needs to be added
		RETURN_IF_ERROR(m_uid_map.Insert(uid));
		current_index = m_uid_map.Find(uid);
	}

	if (current_index < 0 || server_id > (unsigned)current_index)
	{
		// Something went wrong; we are fetching these in order, and the server ids
		// are supposed to be sequential, therefore this is impossible.
		OP_ASSERT(!"Parsing error or server barfed");

		// We now know nothing about all values in the map higher than current_index, so remove them
		RETURN_IF_ERROR(m_uid_map.RemoveByIndex(current_index, m_uid_map.GetCount() - current_index));

		return OpStatus::OK;
	}

	// Remove messages with index i where server_id <= i < current_index
	// since they are no longer available on the server

	// Remove from UID map
	if (current_index - server_id > 0)
		RETURN_IF_ERROR(m_uid_map.RemoveByIndex(server_id, current_index - server_id));

	current_index = server_id;

	// Remove messages with a uid between m_uid_map[current_index - 1] and uid from local storage
	int from = current_index > 0 && current_index < (int)m_uid_map.GetCount() ? m_uid_map.GetByIndex(current_index - 1) + 1 : 1;
	RETURN_IF_ERROR(m_backend.GetUIDManager().RemoveMessagesByUID(m_index_id, from, uid));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::LastUIDReceived
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::LastUIDReceived()
{
	// Check if we have a UID map
	if (!m_backend.ShouldMaintainUIDMap(this))
		return OpStatus::OK;

	// remove messages by uid at the end [m_uid_map[m_uid_map.GetCount() - 1]+1 .. inf)
	int from = m_uid_map.GetCount() ? m_uid_map.GetByIndex(m_uid_map.GetCount() - 1) + 1 : 1;

	if (!m_index_id)
		GetIndex();

	return m_backend.GetUIDManager().RemoveMessagesByUID(m_index_id, from, 0);
}


/***********************************************************************************
 ** Add a UID to this folder, means that this UID has been fetched
 **
 ** ImapFolder::AddUID
 ** @param uid UID that is fetched
 ** @param m2_id M2 message id of the fetched message
 ***********************************************************************************/
OP_STATUS ImapFolder::AddUID(unsigned uid, message_gid_t m2_id)
{
	if (!m_index_id)
		return OpStatus::ERR;

	return m_backend.GetUIDManager().AddUID(m_index_id, uid, m2_id);
}


/***********************************************************************************
 **
 **
 ** ImapFolder::GetMessageByServerID
 **
 ***********************************************************************************/
message_gid_t ImapFolder::GetMessageByServerID(int server_id)
{
	// Check if we have a UID map
	if (!m_backend.ShouldMaintainUIDMap(this))
		return 0;

	// server_id is one-based, we are zero-based
	server_id--;

	if (server_id >= (int)m_uid_map.GetCount())
		return 0;

	return GetMessageByUID(m_uid_map.GetByIndex(server_id));
}


/***********************************************************************************
 **
 **
 ** ImapFolder::GetMessageByUID
 **
 ***********************************************************************************/
message_gid_t ImapFolder::GetMessageByUID(unsigned uid)
{
	return m_backend.GetUIDManager().GetByUID(m_index_id, uid);
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetSubscribed
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetSubscribed(BOOL subscribed, BOOL sync_now)
{
	m_subscribed = subscribed;

	if (!sync_now)
		return OpStatus::OK;

	if (subscribed)
	{
		// Synchronize this folder, mark as selectable (might discover that it's not selectable later)
		m_selectable = TRUE;
		RETURN_IF_ERROR(m_backend.SyncFolder(this));
	}
	else
	{
		// We don't select unsubscribed folders
		RETURN_IF_ERROR(MarkUnselectable(FALSE, TRUE));
	}

	// Save changes
	RETURN_IF_ERROR(SaveToFile());

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::MarkUnselectable
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::MarkUnselectable(BOOL force, BOOL remove_messages)
{
	if (!m_selectable && !force)
		return OpStatus::OK;

	// Reset the folder
	if (remove_messages)
		RETURN_IF_ERROR(ResetFolder());

	// Set selectable property
	m_selectable = FALSE;

	// Reset the index
	Index* index = GetIndex();
	if (index && remove_messages)
	{
		index_gid_t parent_index = index->GetParentId();

		if (!MessageEngine::GetInstance()->GetIndexer()->HasChildren(m_index_id))
		{
			// If this index has no children, remove the index completely
			OpStatus::Ignore(MessageEngine::GetInstance()->GetIndexer()->RemoveIndex(index));
			m_index_id = 0;

			// Do actions on parents if necessary (will remove the parent index if that's now possible)
			ImapFolder* parent;
			if (OpStatus::IsSuccess(m_backend.GetFolderByIndex(parent_index, parent)) &&
						 (!parent->IsSelectable() || !parent->IsSubscribed()))
			{
				RETURN_IF_ERROR(parent->MarkUnselectable(TRUE, remove_messages));
			}
		}
		else
		{
			index->SetReadOnly(TRUE);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetExists
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetExists(unsigned exists, BOOL do_sync)
{
	if (exists == 0)
	{
		// There are now no messages left. Clear all messages
		m_uid_map.Clear();
		RETURN_IF_ERROR(LastUIDReceived());

		if (m_next_mod_seq_after_sync > 0)
		{
			m_last_known_mod_seq = m_next_mod_seq_after_sync;
			m_next_mod_seq_after_sync = 0;
		}
	}

	if (exists != m_exists)
	{
		if (exists < m_exists && exists > 0)
		{
			// Messages have been removed, and we weren't notified

			// Truncate the UID map
			if (static_cast<unsigned int>(m_uid_map.GetCount()) > exists)
				m_uid_map.RemoveByIndex(exists, m_uid_map.GetCount() - exists);

			if (do_sync)
			{
				// We do a full synchronisation to see which messages were removed
				m_last_full_sync = 0;
				RETURN_IF_ERROR(m_backend.SyncFolder(this));
			}
		}
		else if (exists > m_exists && do_sync)
		{
			// There are new messages in this folder
			if (!(NeedsFullSync() && IsScheduledForSync()) && m_backend.SelectedByConnection(this))
			{
				// If this folder is currently selected, fetch messages immediately
				RETURN_IF_ERROR(m_backend.GetConnection(this).FetchMessagesByID(this, m_exists + 1, exists));
			}
			else
			{
				// Needs to synchronize
				m_last_full_sync = 0;
				RETURN_IF_ERROR(m_backend.SyncFolder(this));
			}
		}

		m_exists = exists;

		RETURN_IF_ERROR(SaveToFile());
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetUidNext
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetUidNext(unsigned uid_next)
{
	if (m_uid_next > 0 && uid_next != m_uid_next && !m_backend.SelectedByConnection(this))
	{
		// This mailbox has new messages and is not currently selected, we'll need to sync
		RequestFullSync();
		RETURN_IF_ERROR(m_backend.SyncFolder(this));
	}

	if (m_uid_next != uid_next)
	{
		m_uid_next = uid_next;
		RETURN_IF_ERROR(SaveToFile());
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetUidValidity
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetUidValidity(unsigned uid_validity)
{
	if (m_uid_validity != uid_validity)
	{
		if (m_uid_validity > 0)
		{
			// UID validity has changed - forget all we know about this folder
			RETURN_IF_ERROR(ResetFolder());
			RETURN_IF_ERROR(m_backend.SyncFolder(this));
		}

		m_uid_validity = uid_validity;
		RETURN_IF_ERROR(SaveToFile(TRUE));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::UpdateModSeq
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::UpdateModSeq(UINT64 mod_seq, bool from_fetch_response)
{
	// if received in a fetch response, only use it if it's higher and a folder is selected (and therefore synced)
	// if not from fetch response, update it if the folder is selected
	// otherwise update it after next sync and set it as the next_mod_seq

	if (from_fetch_response)
	{
		// only update from FETCH when we're not doing a full sync
		if (mod_seq > m_last_known_mod_seq && m_last_known_mod_seq >= m_next_mod_seq_after_sync && m_backend.GetConnection(this).HasSelectedFolder())
		{
			m_last_known_mod_seq = mod_seq;
			return SaveToFile();
		}
		
		return OpStatus::OK;
	}
	else if (m_backend.GetConnection(this).HasSelectedFolder() && m_backend.GetConnection(this).GetCurrentFolder() == this)
	{
		m_last_known_mod_seq = mod_seq;
		return SaveToFile();
	}
	else if (mod_seq == 0 || mod_seq != m_last_known_mod_seq)
	{
		if ((mod_seq == 0 && m_last_known_mod_seq > 0) ||
			(mod_seq > m_last_known_mod_seq && !m_backend.SelectedByConnection(this)))
		{
			// If we are resetting the mod seq, and the folder previously had a mod seq, resync
			// If the folder got a higher mod seq without being selected, we need to sync
			RequestFullSync();
			m_backend.SyncFolder(this);
		}
		m_next_mod_seq_after_sync = mod_seq;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get the index for this folder, or NULL if not available
 **
 ** ImapFolder::GetIndex
 **
 ***********************************************************************************/
Index* ImapFolder::GetIndex()
{
	if (!MessageEngine::GetInstance()->GetIndexer())
		return NULL;

	if (!m_index_id)
	{
		Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(
				m_backend.GetAccountPtr(), GetName16(), GetDelimiter(), GetDisplayName(), FALSE, FALSE);
		if (index)
		{
			m_index_id = index->GetId();
			index->SetReadOnly(!m_selectable);
		}

		return index;
	}
	else
	{
		return MessageEngine::GetInstance()->GetIndexer()->GetIndexById(m_index_id);
	}
}


/***********************************************************************************
 ** Create an index for this folder if it did not already exist
 **
 ** ImapFolder::CreateIndexForFolder
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::CreateIndexForFolder()
{
	if (GetIndex())
		return OpStatus::OK;

	if (MessageEngine::GetInstance()->GetIndexer())
	{
		Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(
				m_backend.GetAccountPtr(), GetName16(), GetDelimiter(), GetDisplayName(), TRUE, FALSE);

		if (!index)
			return OpStatus::ERR;

		m_index_id = index->GetId();
		
		ImapFolder* all_mail_folder = m_backend.GetFolderByType(AccountTypes::FOLDER_ALLMAIL);
		if (all_mail_folder && all_mail_folder != this)
		{
			// we have a new IMAP folder for a gmail account => hide from other views by default
			index->SetHideFromOther(true);
		}

		// Set read-only property for this folder and its parents
		while (index &&
			   IndexTypes::FIRST_IMAP <= index->GetId() &&
			   index->GetId()         <  IndexTypes::LAST_IMAP)
		{
			ImapFolder* folder;
			if (OpStatus::IsSuccess(m_backend.GetFolderByIndex(index->GetId(), folder)))
				index->SetReadOnly(!folder->IsSelectable());

			index = MessageEngine::GetInstance()->GetIndexById(index->GetParentId());
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ImapFolder::GetDisplayName
 **
 ***********************************************************************************/
const uni_char* ImapFolder::GetDisplayName() const
{
	int pos = m_name16.FindLastOf(m_delimiter);

	if (pos == KNotFound)
		return m_name16.CStr();
	else
		return (m_name16.CStr() + pos + 1);
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetNewName
 **
 ***********************************************************************************/

OP_STATUS ImapFolder::SetNewName(const OpStringC8& new_name)
{
	return OpMisc::QuoteString(new_name, m_quoted_new_name);
}


/***********************************************************************************
 **
 **
 ** ImapFolder::SetFlags
 **
 ***********************************************************************************/
void ImapFolder::SetFlags(int flags, BOOL permanent)
{
	if (permanent)
		m_permanent_flags = flags;
	else
		m_settable_flags = flags;
}


/***********************************************************************************
 ** Set the list flags (from LSUB or LIST response) for this folder
 **
 ** ImapFolder::SetListFlags
 **
 ***********************************************************************************/
void ImapFolder::SetListFlags(int flags)
{
	if (flags & ImapFlags::LFLAG_NOSELECT)
		MarkUnselectable();

	if (!m_subscribed)
		return;
	
	// Set special folder if flag is set
	if (flags & ImapFlags::LFLAG_SENT && !m_backend.GetFolderByType(AccountTypes::FOLDER_SENT))
	{
		OpStatus::Ignore(m_backend.SetFolderPath(AccountTypes::FOLDER_SENT, m_name16));
	}
	else if (flags & ImapFlags::LFLAG_SPAM && !m_backend.GetFolderByType(AccountTypes::FOLDER_SPAM))
	{
		OpStatus::Ignore(m_backend.SetFolderPath(AccountTypes::FOLDER_SPAM, m_name16));
	}
	else if (flags & ImapFlags::LFLAG_TRASH && !m_backend.GetFolderByType(AccountTypes::FOLDER_TRASH))
	{
		OpStatus::Ignore(m_backend.SetFolderPath(AccountTypes::FOLDER_TRASH, m_name16));
	}
	else if (flags & ImapFlags::LFLAG_ALLMAIL)
	{
		OpStatus::Ignore(m_backend.SetAllMailFolder(this));
	}
}


/***********************************************************************************
 **
 **
 ** ImapFolder::GetInternetLocation
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::GetInternetLocation(unsigned uid, OpString8& internet_location) const
{
	internet_location.Empty();
	return internet_location.AppendFormat(m_internet_location_format.CStr(), uid);
}

/***********************************************************************************
 **
 **
 ** ImapFolder::DoneFullSync
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::DoneFullSync()
{ 
	if (m_next_mod_seq_after_sync > 0) 
		m_last_known_mod_seq = m_next_mod_seq_after_sync; 
	
	m_last_full_sync = g_timecache->CurrentTime(); 
	RETURN_IF_ERROR(SaveToFile());
	
	return LastUIDReceived(); 
}

/***********************************************************************************
 **
 **
 ** ImapFolder::ResetFolder
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::ResetFolder()
{
	if (m_exists == 0 && m_uid_map.GetCount() == 0)
		return OpStatus::OK; // Nothing to reset

	// Clear all messages
	m_uid_map.Clear();
	RETURN_IF_ERROR(LastUIDReceived());

	m_exists = 0;
	m_uid_next = 0;
	m_uid_validity = 0;
	m_unseen = 0;
	m_recent = 0;
	m_last_full_sync = 0;
	m_scheduled_for_sync = 0;
	m_last_known_mod_seq = 0;
	m_next_mod_seq_after_sync = 0;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Saves data about this folder to the IMAP account configuration file
 **
 ** ImapFolder::SaveToFile
 ** @param commit Whether to commit data to disk
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SaveToFile(BOOL commit)
{
	OpString   folder_section;
	PrefsFile* prefs_file = m_backend.GetCurrentPrefsFile();

	if (!prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(folder_section.AppendFormat(UNI_L("Folder=%s"), m_name16.CStr()));

	// Create string for 64-bit mod seq
	OpString last_known_mod_seq;
	RETURN_IF_ERROR(last_known_mod_seq.AppendFormat(UNI_L("%lld"), m_last_known_mod_seq));

	// Save folder properties
	// TODO Namespace
	RETURN_IF_LEAVE(prefs_file->WriteStringL(folder_section, UNI_L("Folder"),				m_name16			); 
					prefs_file->WriteIntL   (folder_section, UNI_L("Separator"),			m_delimiter			); 
					prefs_file->WriteStringL(folder_section, UNI_L("Namespace"),			UNI_L("")			); 
					prefs_file->WriteIntL   (folder_section, UNI_L("UID Validity"),			m_uid_validity		); 
					prefs_file->WriteIntL   (folder_section, UNI_L("UID Next"),				m_uid_next			); 
					prefs_file->WriteIntL   (folder_section, UNI_L("Exists"),				m_exists			); 
					prefs_file->WriteStringL(folder_section, UNI_L("Last Known Mod Seq"),   last_known_mod_seq	);
				   );

	// commit if necessary
	if (commit)
		RETURN_IF_LEAVE(prefs_file->CommitL());

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets the highest UID known in this folder
 **
 ** ImapFolder::GetHighestUid
 **
 ***********************************************************************************/
unsigned ImapFolder::GetHighestUid()
{
	return m_backend.GetUIDManager().GetHighestUID(m_index_id);
}


/***********************************************************************************
 ** Reads UIDs of all messages contained in this folder and adds them to
 ** the UID manager. Use when updating from UID-manager-less account
 **
 ** ImapFolder::InsertUIDs
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::InsertUIDs()
{
	Index*			index       = GetIndex();
	Store*			store       = MessageEngine::GetInstance()->GetStore();
	ImapUidManager& uid_manager = m_backend.GetUIDManager();
	OpString8		alt_format;

	if (!index || m_internet_location_format.Length() < 4)
		return OpStatus::ERR;

	// Prepare a quoted internet location, was used in Merlin for some folders
	RETURN_IF_ERROR(alt_format.Set(m_internet_location_format));
	RETURN_IF_ERROR(alt_format.Insert(0, "\""));
	RETURN_IF_ERROR(alt_format.Insert(alt_format.Length() - 3, "\""));

	RETURN_IF_ERROR(index->PreFetch());

	// Do conversion
	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		message_gid_t message_id = it.GetData();
		int           uid;
		OpString8	  internet_location;

		if (OpStatus::IsError(store->GetMessageInternetLocation(message_id, internet_location)) ||
			internet_location.IsEmpty())
			continue;

		if (op_sscanf(internet_location.CStr(), m_internet_location_format.CStr(), &uid) > 0)
		{
			// Add to UID manager
			OpStatus::Ignore(uid_manager.AddUID(m_index_id, uid, message_id));
		}
		else if (op_sscanf(internet_location.CStr(), alt_format.CStr(), &uid) > 0)
		{
			// Old format internet location, update the format first
			Message message;

			RETURN_IF_ERROR(store->GetMessage(message, message_id));
			RETURN_IF_ERROR(GetInternetLocation(uid, internet_location));
			RETURN_IF_ERROR(message.SetInternetLocation(internet_location));
			RETURN_IF_ERROR(store->UpdateMessage(message));

			// Add to UID manager
			OpStatus::Ignore(uid_manager.AddUID(m_index_id, uid, message_id));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Reads the uids in 'container' and converts them to M2 ids
 **
 ** ImapFolder::ConvertUIDToM2Id
 ** @param container Vector containing uids, will contain M2 ids if successful
 ***********************************************************************************/
OP_STATUS ImapFolder::ConvertUIDToM2Id(OpINT32Vector& container)
{
	ImapUidManager& uid_manager = m_backend.GetUIDManager();

	// Make sure we have an index
	GetIndex();

	// Replace all ids in the container
	for (unsigned i = 0; i < container.GetCount(); i++)
	{
		message_gid_t m2_id = uid_manager.GetByUID(m_index_id, container.Get(i));

		RETURN_IF_ERROR(container.Replace(i, m2_id));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Convert a UTF16 string to a modified UTF-7 format compatible with IMAP
 ** Details in RFC3501 5.1.3
 **
 ** ImapFolder::EncodeMailboxName
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::EncodeMailboxName(const OpStringC16& source, OpString8& target)
{
	if (source.IsEmpty())
		return OpStatus::OK; // empty string

	static    UTF16toUTF7Converter converter(UTF16toUTF7Converter::IMAP);
	int       src_len      = source.Length();
	int       max_dest_len = (src_len * 4) + 1;
	int       bytes_read;
	OpString8 dest;
	char*     dest_ptr = dest.Reserve(max_dest_len);

	if (!dest_ptr)
		return OpStatus::ERR_NO_MEMORY;

	// Encode
	int written = converter.Convert(source.CStr(), UNICODE_SIZE(src_len), dest_ptr, max_dest_len, &bytes_read);

	// Check if we got all
	if (written < 0 || bytes_read < UNICODE_SIZE(src_len))
		return OpStatus::ERR;

	// terminate string
	dest_ptr[written] = '\0';

	return target.Set(dest_ptr);
}


/***********************************************************************************
 ** Set names in various formats. Supply either name8 or name16, not both!
 **
 ** ImapFolder::SetName
 ** @param name8     8-bit version of the folder name
 ** @param name16    16-bit version of the folder name
 ** @param delimiter Folder delimiter character
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::SetName(const OpStringC8& name8, const OpStringC16& name16, char delimiter)
{
	if (name8.HasContent())
	{
		RETURN_IF_ERROR(m_name.Set(name8));
		RETURN_IF_ERROR(DecodeMailboxName(m_name, m_name16));
	}
	else
	{
		RETURN_IF_ERROR(m_name16.Set(name16));
		RETURN_IF_ERROR(EncodeMailboxName(m_name16, m_name));
	}
	RETURN_IF_ERROR(OpMisc::QuoteString(m_name, m_quoted_name));

	RETURN_IF_ERROR(m_internet_location_format.Set(m_name));
	RETURN_IF_ERROR(m_internet_location_format.Append(":%d"));

	m_delimiter = delimiter;

	return OpStatus::OK;
}

/***********************************************************************************
 ** Apply the new name on the ImapFolder
 **
 ** ImapFolder::ApplyNewName
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::ApplyNewName()
{
	OpString8	new_name8;
	OpString	new_name16;
	RETURN_IF_ERROR(OpMisc::UnQuoteString(m_quoted_new_name, new_name8));
	RETURN_IF_ERROR(DecodeMailboxName(new_name8, new_name16));
	
	// ImapBackend::RemoveFolderFromIMAPBackend and GetIndex() use m_name_16, so we have to change it before we set the new name
	RETURN_IF_ERROR(m_backend.RemoveFolderFromIMAPBackend(this));
	Index* index = GetIndex();
	RETURN_IF_ERROR(index->GetSearch(0)->SetSearchText(new_name16));
	
	// Update the internetlocation for all messages in the IMAP folder (slow... should probably be async)
	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		Message message;
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, it.GetData()));
		OpString8 internet_location;
		message.GetInternetLocation(internet_location);
		unsigned uid = atoi(internet_location.CStr()+m_name.Length()+1);
		RETURN_IF_ERROR(internet_location.Set(new_name8.CStr()));
		RETURN_IF_ERROR(internet_location.AppendFormat(":%d", uid));
		RETURN_IF_ERROR(message.SetInternetLocation(internet_location));
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->UpdateMessage(message));
	}

	// apply the new name (since we have all versions of the new name already, we might as well set them directly)
	RETURN_IF_ERROR(m_name.Set(new_name8));
	RETURN_IF_ERROR(m_name16.Set(new_name16));
	RETURN_IF_ERROR(m_quoted_name.Set(m_quoted_new_name));
	m_quoted_new_name.Empty();

	RETURN_IF_ERROR(m_internet_location_format.Set(m_name));
	RETURN_IF_ERROR(m_internet_location_format.Append(":%d"));

	// add the new folder name
	return m_backend.AddFolder(this);
}

/***********************************************************************************
 ** Convert a modified UTF-7 string from IMAP to a UTF16 string
 ** Details in RFC3501 5.1.3
 **
 ** ImapFolder::DecodeMailboxName
 **
 ***********************************************************************************/
OP_STATUS ImapFolder::DecodeMailboxName(const OpStringC8& source, OpString16& target)
{
	return OpMisc::DecodeMailboxOrKeyword(source, target);
}

#endif //M2_SUPPORT
