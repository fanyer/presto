/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/util/misc.h"

///////////////////////////////////////////
//                Create
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Create::OnFailed
 ***********************************************************************************/
OP_STATUS ImapCommands::Create::OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
{
	// Creating the folder failed, make sure it's removed again from all lists
	RETURN_IF_ERROR(protocol.GetBackend().RemoveFolder(m_mailbox));

	return ImapCommandItem::OnFailed(protocol, failed_msg);
}


///////////////////////////////////////////
//            DeleteMailbox
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** DeleteMailbox::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::DeleteMailbox::GetExpandedQueue(IMAP4& protocol)
{
	ImapCommandItem* delete_command = OP_NEW(ImapCommands::Delete, (m_mailbox));
	if (!delete_command)
		return NULL;

	if (m_mailbox->IsSubscribed())
	{
		ImapCommandItem* unsub = OP_NEW(ImapCommands::Unsubscribe, (m_mailbox));
		if (!unsub)
		{
			OP_DELETE(delete_command);
			return NULL;
		}

		delete_command->DependsOn(unsub, protocol);
		return unsub;
	}

	return delete_command;
}

///////////////////////////////////////////
//                Rename
///////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** Rename::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::Rename::OnSuccessfulComplete(IMAP4& protocol)
{
	if (!m_mailbox->IsSubscribed())
		return OpStatus::OK;
	
	// rename went fine, subscribe to the mailbox with the new name
	RETURN_IF_ERROR(ReSubscribeMailbox(m_mailbox, protocol));
	
	// and resubscribe to all the sub mailboxes 
	index_gid_t id = m_mailbox->GetIndex()->GetId();
	OpINT32Vector children; 
	if (OpStatus::IsSuccess(g_m2_engine->GetIndexer()->GetChildren(id, children)) && children.GetCount() > 0)
	{
		OpString replace_with;
		OpString8 new_name_unencoded, old_name_unencoded;
		// Get the new name of the parent folder
		RETURN_IF_ERROR(OpMisc::UnQuoteString(m_mailbox->GetNewName(), new_name_unencoded));
		RETURN_IF_ERROR(ImapFolder::DecodeMailboxName(new_name_unencoded, replace_with));

		for (UINT32 i = 0; i < children.GetCount(); i++)
		{
			ImapFolder* folder;
			OpString replace_in;
			OpString8 new_subfolder_name;

			if (OpStatus::IsSuccess(protocol.GetBackend().GetFolderByIndex(children.Get(i), folder)))
			{
				RETURN_IF_ERROR(replace_in.Set(folder->GetName16()));
				// set the new name on the folder
				RETURN_IF_ERROR(StringUtils::Replace(replace_in, m_mailbox->GetName16(), replace_with));
				RETURN_IF_ERROR(ImapFolder::EncodeMailboxName(replace_in, new_subfolder_name));
				RETURN_IF_ERROR(folder->SetNewName(new_subfolder_name));
				// and resubscribe to this folder (with the updated name)
				RETURN_IF_ERROR(ReSubscribeMailbox(folder, protocol));
			}
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Rename::ReSubscribeMailbox
 ***********************************************************************************/
OP_STATUS ImapCommands::Rename::ReSubscribeMailbox(ImapFolder* mailbox, IMAP4& protocol)
{
	ImapCommandItem* unsubscribe_to_old = OP_NEW(ImapCommands::UnsubscribeRenamed, (mailbox));
	ImapCommandItem* subscribe_to_new = OP_NEW(ImapCommands::SubscribeRenamed, (mailbox));
	
	if (!unsubscribe_to_old || !subscribe_to_new)
	{
		OP_DELETE(unsubscribe_to_old);
		OP_DELETE(subscribe_to_new);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	// subscribe first, in case it fails, we are still subscribed to the old one
	unsubscribe_to_old->DependsOn(subscribe_to_new, protocol);
	return AddDependency(subscribe_to_new, protocol);
}


///////////////////////////////////////////
//                Append
///////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** Append::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::Append::OnSuccessfulComplete(IMAP4& protocol)
{
	// If we don't have UIDPLUS (so we didn't get an APPENDUID), we immediately
	// synchronize the folder to get the just-appended message
	if (!(protocol.GetCapabilities() & ImapFlags::CAP_UIDPLUS))
		RETURN_IF_ERROR(protocol.GetBackend().SyncFolder(m_mailbox));

	if (m_mailbox->GetIndex())
		return m_mailbox->GetIndex()->DecreaseNewMessageCount(1);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Append::OnAppendUid
 ***********************************************************************************/
OP_STATUS ImapCommands::Append::OnAppendUid(IMAP4& protocol, unsigned uid_validity, unsigned uid)
{
	return protocol.GetBackend().OnMessageCopied(m_message_id, m_mailbox, uid, FALSE);
}


/***********************************************************************************
 **
 **
 ** Append::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Append::AppendCommand(OpString8& command, IMAP4& protocol)
{
	if (m_raw_message.IsEmpty())
	{
		Message message;

		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, m_message_id));
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageData(message));
		RETURN_IF_ERROR(message.GetRawMessage(m_raw_message));
	}

	if (!m_continuation)
		return command.AppendFormat("APPEND %s (\\Seen) {%u}", m_mailbox->GetQuotedName().CStr(), m_raw_message.Length());
	else
		return command.Append(m_raw_message);
}


///////////////////////////////////////////
//             AppendSent
///////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** AppendSent::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::AppendSent::OnSuccessfulComplete(IMAP4& protocol)
{
	// Remove this sent message, there's a new one waiting
	if (!m_got_uid)
		MessageEngine::GetInstance()->GetIndexer()->RemoveMessage(m_message_id);

	return Append::OnSuccessfulComplete(protocol);
}


/***********************************************************************************
 **
 **
 ** AppendSent::OnAppendUid
 ***********************************************************************************/
OP_STATUS ImapCommands::AppendSent::OnAppendUid(IMAP4& protocol, unsigned uid_validity, unsigned uid)
{
	if (OpStatus::IsSuccess(protocol.GetBackend().OnMessageCopied(m_message_id, m_mailbox, uid, TRUE)))
		m_got_uid = TRUE;

	return OpStatus::OK;
}


///////////////////////////////////////////
//                Select
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Select::PrepareToSend
 ***********************************************************************************/
OP_STATUS ImapCommands::Select::PrepareToSend(IMAP4& protocol)
{
	// Make sure that the connection knows which mailbox it's operating on
	m_mailbox->GetIndex();
	protocol.SetCurrentFolder(m_mailbox);

	// Check if this command is a full synchronization itself
	if ((protocol.GetCapabilities() & ImapFlags::CAP_QRESYNC) && m_mailbox->LastKnownModSeq())
		m_is_quick_resync = TRUE;

	// Do a full sync if necessary
	if (!m_is_quick_resync && m_mailbox->NeedsFullSync())
		return AddDependency(OP_NEW(ImapCommands::Sync, (m_mailbox)), protocol);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** Select::OnFailed
 ***********************************************************************************/
OP_STATUS ImapCommands::Select::OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
{
	if (m_is_quick_resync)
	{
		// maybe it's because of qresync, try again without
		protocol.SetCapabilities(protocol.GetCapabilities() & ~ImapFlags::CAP_QRESYNC);
		return protocol.InsertCommand(OP_NEW(ImapCommands::Select, (m_mailbox)), TRUE);
	}

	protocol.SetCurrentFolder(0); 
	return m_mailbox->MarkUnselectable(); 
}

/***********************************************************************************
 **
 **
 ** Select::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::Select::OnSuccessfulComplete(IMAP4& protocol)
{
	RETURN_IF_ERROR(m_mailbox->CreateIndexForFolder());

	protocol.SetCurrentFolderIsSelected();

	if (m_is_quick_resync)
		return m_mailbox->DoneFullSync();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** Select::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::StatusSync::OnSuccessfulComplete(IMAP4& protocol)
{
	RETURN_IF_ERROR(m_mailbox->CreateIndexForFolder());

	return OpStatus::OK;
}



/***********************************************************************************
 **
 **
 ** Select::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Select::AppendCommand(OpString8& command, IMAP4& protocol)
{
	if (m_is_quick_resync)
		return command.AppendFormat("SELECT %s (QRESYNC (%d %lld))", m_mailbox->GetQuotedName().CStr(), m_mailbox->GetUidValidity(), m_mailbox->LastKnownModSeq());
	else
		return command.AppendFormat("SELECT %s", m_mailbox->GetQuotedName().CStr());
}


///////////////////////////////////////////
//              Unselect
///////////////////////////////////////////

/***********************************************************************************
 ** Replaces the command with EXAMINE followed by CLOSE, which has the same effect
 ** as UNSELECT
 **
 ** Unselect::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::Unselect::GetExpandedQueue(IMAP4& protocol)
{
	ImapCommandItem* examine = OP_NEW(ImapCommands::Examine, (m_mailbox));
	ImapCommandItem* close   = OP_NEW(ImapCommands::Close, (m_mailbox));

	if (!examine || !close)
	{
		OP_DELETE(examine);
		OP_DELETE(close);
		return NULL;
	}

	close->DependsOn(examine, protocol);

	return examine;
}


///////////////////////////////////////////
//                Sync
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Sync::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::Sync::GetExpandedQueue(IMAP4& protocol)
{
	ImapCommandItem* sync_command = NULL;

	// Check if a full sync is needed
	if (m_mailbox->NeedsFullSync() && m_mailbox->GetExists() > 0)
	{
		// If possible, we use a quick resync, direct or via select; else, we sync new messages only or all messages depending on low bandwidth mode
		if ((protocol.GetCapabilities() & ImapFlags::CAP_QRESYNC) && m_mailbox->LastKnownModSeq())
		{
			if (protocol.GetCurrentFolder() == m_mailbox)
				sync_command = OP_NEW(ImapCommands::QuickResync, (m_mailbox));
			else
				sync_command = OP_NEW(ImapCommands::SelectSync, (m_mailbox));
		}
		else if (protocol.GetBackend().GetAccountPtr()->GetLowBandwidthMode())
			sync_command = OP_NEW(ImapCommands::LowBandwidthSync, (m_mailbox));
		else
			sync_command = OP_NEW(ImapCommands::FullSync, (m_mailbox));
	}
	else
	{
		// If we are already in the mailbox that needs to be synced, do a NOOP; select the mailbox if there's nothing selected right now; or just do a status otherwise
		if (protocol.GetCurrentFolder() == m_mailbox)
			sync_command = OP_NEW(ImapCommands::NoopSync, (m_mailbox));
		else if (!protocol.GetCurrentFolder())
			sync_command = OP_NEW(ImapCommands::SelectSync, (m_mailbox));
		else
			sync_command = OP_NEW(ImapCommands::StatusSync, (m_mailbox));
	}

	return sync_command;
}
