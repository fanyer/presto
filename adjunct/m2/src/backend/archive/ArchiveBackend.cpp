/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/archive/ArchiveBackend.h"

#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"


/***********************************************************************************
 **
 **
 ** ArchiveBackend::ArchiveBackend
 ***********************************************************************************/
ArchiveBackend::ArchiveBackend(MessageDatabase& database)
  : ProtocolBackend(database)
{
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::Init
 ***********************************************************************************/
OP_STATUS ArchiveBackend::Init(Account* account)
{
	m_account = account;

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::GetType
 ***********************************************************************************/
AccountTypes::AccountType ArchiveBackend::GetType() const
{
	return AccountTypes::ARCHIVE;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::SettingsChanged
 ***********************************************************************************/
OP_STATUS ArchiveBackend::SettingsChanged(BOOL startup)
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::PrepareToDie
 ***********************************************************************************/
OP_STATUS ArchiveBackend::PrepareToDie()
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::GetAuthenticationSupported
 ***********************************************************************************/
UINT32 ArchiveBackend::GetAuthenticationSupported()
{
	return 0;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::GetIndexType
 ***********************************************************************************/
IndexTypes::Type ArchiveBackend::GetIndexType() const
{
	return IndexTypes::ARCHIVE_INDEX;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::GetIcon
 ***********************************************************************************/
const char* ArchiveBackend::GetIcon(BOOL progress_icon)
{
	return progress_icon ? NULL : "Account Archive";
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::FetchMessage
 ***********************************************************************************/
OP_STATUS ArchiveBackend::FetchMessage(message_index_t id, BOOL user_request, BOOL force_complete)
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::FetchMessages
 ***********************************************************************************/
OP_STATUS ArchiveBackend::FetchMessages(BOOL enable_signalling)
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::StopFetchingMessages
 ***********************************************************************************/
OP_STATUS ArchiveBackend::StopFetchingMessages()
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::Connect
 ***********************************************************************************/
OP_STATUS ArchiveBackend::Connect()
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::Disconnect
 ***********************************************************************************/
OP_STATUS ArchiveBackend::Disconnect()
{
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::InsertExternalMessage
 ***********************************************************************************/
OP_STATUS ArchiveBackend::InsertExternalMessage(Message& message)
{
	// TODO something?

	return OpStatus::OK;
}


/***********************************************************************************
 ** Moving messages is like copying, but removing the original message
 **
 ** ArchiveBackend::MoveMessages
 ***********************************************************************************/
OP_STATUS ArchiveBackend::MoveMessages(const OpINT32Vector& message_ids,
									   index_gid_t source_index_id,
									   index_gid_t destination_index_id)
{
	RETURN_IF_ERROR(CopyMessages(message_ids, source_index_id, destination_index_id));

	OpINT32Vector message_ids_copy;
	RETURN_IF_ERROR(message_ids_copy.DuplicateOf(message_ids));

	return MessageEngine::GetInstance()->RemoveMessages(message_ids_copy, TRUE);
}


/***********************************************************************************
 **
 **
 ** ArchiveBackend::CopyMessages
 ***********************************************************************************/
OP_STATUS ArchiveBackend::CopyMessages(const OpINT32Vector& message_ids,
									   index_gid_t source_index_id,
									   index_gid_t destination_index_id)
{
	// If it was just copied to the archive account, we archive the message as if
	// it was archived specifically. If not, we copy the message to the designated
	// index
	if (destination_index_id == (index_gid_t)(IndexTypes::FIRST_ACCOUNT + GetAccountPtr()->GetAccountId()))
	{
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			RETURN_IF_ERROR(ArchiveMessage(message_ids.Get(i)));
		}
	}
	else
	{
		Index* dest_index = MessageEngine::GetInstance()->GetIndexById(destination_index_id);
		if (!dest_index)
			return OpStatus::ERR;

		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			// Duplicate message
			Message message;

			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, message_ids.Get(i)));
			RETURN_IF_ERROR(GetAccountPtr()->InternalCopyMessage(message));

			// Add message to destination index
			RETURN_IF_ERROR(dest_index->NewMessage(message.GetId()));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Archiving a message sets up a structure similar to the structure in which
 ** the original message was found (i.e. account and folders if necessary)
 **
 ** This way, e.g. an archived IMAP message will mirror its original path on the
 ** IMAP server.
 **
 ** ArchiveBackend::ArchiveMessage
 ***********************************************************************************/
OP_STATUS ArchiveBackend::ArchiveMessage(message_gid_t message_id)
{
	// Find account for message
	UINT16 account_id = MessageEngine::GetInstance()->GetStore()->GetMessageAccountId(message_id);
	Account* account  = MessageEngine::GetInstance()->GetAccountById(account_id);

	if (!account)
		return OpStatus::ERR;

	// Find original index for message
	index_gid_t original_id = account->GetMessageOrigin(message_id);
	if (!original_id)
		original_id = IndexTypes::FIRST_ACCOUNT + account_id;

	// Find destination index for message
	Index* dest_index = MessageEngine::GetInstance()->GetIndexById(GetMirrorIndex(original_id));
	if (!dest_index)
		return OpStatus::ERR;

	// Duplicate message
	Message message;

	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, message_id));
	RETURN_IF_ERROR(GetAccountPtr()->InternalCopyMessage(message));

	// Add message to destination index
	return dest_index->NewMessage(message.GetId());
}


/***********************************************************************************
 ** Gets (creates if necessary) a mirror index for a specified index
 **
 ** ArchiveBackend::GetMirrorIndex
 ** @param orig_index_id ID of index to mirror
 ** @return ID of mirrored index, or 0 on error
 ***********************************************************************************/
index_gid_t ArchiveBackend::GetMirrorIndex(index_gid_t orig_index_id)
{
	// Find mirror in indexer
	Index* orig_index = MessageEngine::GetInstance()->GetIndexById(orig_index_id);
	if (!orig_index)
		return 0;

	if (orig_index->GetMirrorId())
		return orig_index->GetMirrorId();

	// No mirror exists yet, create a mirror

	// Determine parent index
	index_gid_t parent_index_id = GetMirrorIndex(orig_index->GetParentId());
	if (!parent_index_id)
		parent_index_id = IndexTypes::FIRST_ACCOUNT + GetAccountPtr()->GetAccountId();

	Index* parent_index = MessageEngine::GetInstance()->GetIndexById(parent_index_id);
	if (!parent_index)
		return 0;

	// Create new index, takes name from original index
	OpAutoPtr<Index> new_index (OP_NEW(Index, ()));
	if (!new_index.get())
		return 0;

	OpString orig_name;
	RETURN_VALUE_IF_ERROR(orig_index->GetName(orig_name),0);
	RETURN_VALUE_IF_ERROR(new_index->SetName(orig_name.CStr()),0);

	// Setup and save the new index
	RETURN_VALUE_IF_ERROR(new_index->SetupWithParent(parent_index),0);
	new_index->SetType(IndexTypes::ARCHIVE_INDEX);
	RETURN_VALUE_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->NewIndex(new_index.get()),0);

	orig_index->SetMirrorId(new_index->GetId());

	return new_index.release()->GetId();
}


#endif // M2_SUPPORT
