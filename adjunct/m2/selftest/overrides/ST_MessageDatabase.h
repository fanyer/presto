/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_MESSAGE_DATABASE
#define ST_MESSAGE_DATABASE

#ifdef SELFTEST

#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"

class ST_MessageDatabase : public MessageDatabase
{
public:
		ST_MessageDatabase() : m_all_messages_available(FALSE), m_message_id_available(0) {}
		virtual OP_STATUS AddMessage(StoreMessage& message) { return OpStatus::OK; }
		virtual OP_STATUS UpdateMessageMetaData(StoreMessage& message) { return OpStatus::OK; }
		virtual OP_STATUS UpdateMessageData(StoreMessage& message) { return OpStatus::OK; }
		virtual OP_STATUS RemoveMessage(message_gid_t message_id) { return OpStatus::OK; }
		virtual OP_STATUS GetMessage(message_gid_t message_id, BOOL full, StoreMessage& message) { return OpStatus::OK; }
		virtual OP_STATUS MessageSent(message_gid_t message_id) { return OpStatus::OK; }
		virtual OP_STATUS ArchiveMessage(message_gid_t message_id) { return OpStatus::OK; }
		virtual Index* GetIndex(index_gid_t index_id) { return NULL; }
		virtual OP_STATUS RemoveIndex(index_gid_t index_id)  { return OpStatus::OK; }
		virtual OP_STATUS RequestCommit() { return OpStatus::OK; }
		virtual OP_STATUS Commit() { return OpStatus::OK; }
		virtual OP_STATUS AddCommitListener(CommitListener* listener) { return OpStatus::OK; }
		virtual OP_STATUS RemoveCommitListener(CommitListener* listener) { return OpStatus::OK; }
		virtual void GroupSearchEngineFile(SearchGroupable &group_member) {}
		virtual BOOL HasFinishedLoading() { return m_all_messages_available; }
		virtual void OnMessageMadeAvailable(message_gid_t message_id, BOOL read) { m_message_id_available = message_id; }
		virtual void OnAllMessagesAvailable() { m_all_messages_available = TRUE;}
		virtual void OnMessageChanged(message_gid_t message_id) {}
		virtual void OnMessageBodyChanged(StoreMessage& message) {}
		virtual void OnMessageMarkedAsSent(message_gid_t message_id) {}
		virtual void OnMessageNeedsReindexing(message_gid_t message_id) {}
		virtual MessageChangeLock* CreateMessageChangeLock() { return 0; }
		virtual bool IsInitialized() { return true; }

		BOOL m_all_messages_available;
		UINT32 m_message_id_available;
};

#endif // SELFTEST

#endif // ST_MESSAGE_DATABASE
