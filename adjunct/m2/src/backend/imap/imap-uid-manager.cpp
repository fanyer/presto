/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/imap-uid-manager.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"

/***********************************************************************************
 ** Constructor
 **
 ** ImapUidManager::ImapUidManager
 **
 ***********************************************************************************/
ImapUidManager::ImapUidManager(MessageDatabase& message_database)
  : m_message_database(message_database)
{
	// Ideally we want to group this file, but we can't because of backend initialization timing
	// m_message_database.GroupSearchEngineFile(m_tree.GetGroupMember());
}


/***********************************************************************************
 ** Destructor
 **
 ** ImapUidManager::~ImapUidManager
 **
 ***********************************************************************************/
ImapUidManager::~ImapUidManager()
{
	OpStatus::Ignore(m_tree.Close());
	m_message_database.RemoveCommitListener(this);
}


/***********************************************************************************
 ** Initialization function, run before using any other function
 **
 ** ImapUidManager::Init
 ** @param account_id Account ID of the account this UID manager will manage
 **
 ***********************************************************************************/
OP_STATUS ImapUidManager::Init(int account_id)
{
	OpString filename;

	// Construct path of btree file
	RETURN_IF_ERROR(MailFiles::GetIMAPUIDFilename(account_id,filename));

	// Open btree file
	RETURN_IF_ERROR(m_tree.Open(filename.CStr(), BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE));
	
	// Add this as a commit listener so that commits are in sync with store
	return m_message_database.AddCommitListener(this);
}


/***********************************************************************************
 ** Get a message by its UID
 **
 ** ImapUidManager::GetByUID
 ** @param folder_index_id Index number of the folder this message resides in
 ** @param server_uid UID of this message on the server
 ** @return M2 message ID of the message, or 0 if not found
 **
 ***********************************************************************************/
message_gid_t ImapUidManager::GetByUID(index_gid_t folder_index_id, unsigned uid)
{
	OP_ASSERT(folder_index_id != 0);

	ImapUidKey key(folder_index_id, uid, 0);

	if (m_tree.Search(key) == OpBoolean::IS_TRUE)
		return key.GetMessageId();
	else
		return 0;
}


/***********************************************************************************
 ** Add a server UID or replace information
 **
 ** ImapUidManager::AddUID
 ** @param folder_index_id Index number of the folder this message resides in
 ** @param uid UID of message to add
 ** @param m2_message_id M2 message ID of the message
 **
 ***********************************************************************************/
OP_STATUS ImapUidManager::AddUID(index_gid_t folder_index_id, unsigned uid, message_gid_t m2_message_id)
{
	OP_ASSERT(folder_index_id != 0);

	ImapUidKey key(folder_index_id, uid, m2_message_id);

	RETURN_IF_ERROR(m_tree.Insert(key, TRUE));

	return m_message_database.RequestCommit();
}


/***********************************************************************************
 ** Remove a server UID
 **
 ** ImapUidManager::RemoveUID
 ** @param folder_index_id Index number of the folder this message resides in
 ** @param uid UID of message to remove
 **
 ***********************************************************************************/
OP_STATUS ImapUidManager::RemoveUID(index_gid_t folder_index_id, unsigned uid)
{
	OP_ASSERT(folder_index_id != 0);

	ImapUidKey key(folder_index_id, uid, 0);

	RETURN_IF_ERROR(m_tree.Delete(key));

	return m_message_database.RequestCommit();
}


/***********************************************************************************
 ** Remove messages with UID [from .. to) from M2s message store and the UID manager
 **
 ** ImapUidManager::RemoveMessagesByUID
 ** @param folder_index_id Index number of the folder this message resides in
 ** @param from Remove messages with a UID at least this value
 ** @param to Remove messages with a UID less than this value, or 0 for no limit
 **
 ***********************************************************************************/
OP_STATUS ImapUidManager::RemoveMessagesByUID(index_gid_t folder_index_id, unsigned from, unsigned to)
{
	OP_ASSERT(folder_index_id != 0);

	if (to > 0 && from >= to)
		return OpStatus::OK; // Empty set

	// Prepare search
	ImapUidKey from_key(folder_index_id, from, 0);
	ImapUidKey to_key  (folder_index_id + (to == 0 ? 1 : 0), to, 0);
	SearchIterator<ImapUidKey> *iterator = m_tree.Search(from_key, operatorGE);

	if (!iterator)
		return OpStatus::ERR_NO_MEMORY;

	// Check if there were any results
	if (!iterator->Empty() && iterator->Get() < to_key)
	{
		// Remove results from store
		do
		{
			OpStatus::Ignore(m_message_database.RemoveMessage(iterator->Get().GetMessageId()));
		} while (iterator->Next() && iterator->Get() < to_key);

		// Always close iterator before doing changes
		OP_DELETE(iterator);

		// Remove results from UID index
		RETURN_IF_ERROR(m_tree.Delete(from_key, operatorGE, to_key, operatorLT));
		RETURN_IF_ERROR(m_message_database.RequestCommit());
	}
	else
		OP_DELETE(iterator);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets the highest UID in the database for a certain folder
 **
 ** ImapUidManager::GetHighestUID
 ** @param folder_index_id Index number of the folder to check
 ** @return Highest UID in this folder
 **
 ***********************************************************************************/
unsigned ImapUidManager::GetHighestUID(index_gid_t folder_index_id)
{
	OP_ASSERT(folder_index_id != 0);

	// Prepare search
	ImapUidKey from_key(folder_index_id, 0, 0);
	ImapUidKey to_key  (folder_index_id + 1, 0, 0);
	OpAutoPtr< SearchIterator<ImapUidKey> > iterator (m_tree.Search(from_key, operatorGT));

	if (!iterator.get())
		return 0;

	// Check if there were any results
	if (!iterator->Empty() && iterator->Get() < to_key)
	{
		// iterate until highest one
		while (iterator->Next() && iterator->Get() < to_key) {}

		// Go back one
		if (iterator->Prev())
			return iterator->Get().GetUID();
	}

	// No UIDs for this folder
	return 0;
}

void ImapUidManager::OnCommitted()
{
	if (OpStatus::IsError(m_tree.Commit()))
		m_message_database.RequestCommit();
}


#endif //M2_SUPPORT
