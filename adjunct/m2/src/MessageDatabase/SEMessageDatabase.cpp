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

#ifdef M2_SUPPORT

#include "adjunct/m2/src/MessageDatabase/SEMessageDatabase.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/MessageDatabase/CommitListener.h"
#include "adjunct/m2/src/MessageDatabase/MessageStore.h"
#include "adjunct/m2/src/engine/selexicon.h"

#include "modules/search_engine/BlockStorage.h"


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::SEMessageDatabase
 ***********************************************************************************/
SEMessageDatabase::SEMessageDatabase(MessageHandler& message_handler)
  : m_store(0)
  , m_indexer(0)
  , m_lexicon(0)
  , m_message_handler(message_handler)
  , m_engine(0)
  , m_grouped_search_engine_files(NULL)
  , m_commit_requested(FALSE)
{
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::~SEMessageDatabase
 ***********************************************************************************/
SEMessageDatabase::~SEMessageDatabase()
{
	m_message_handler.UnsetCallBacks(this);
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::Init
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::Init(MessageStore* store, Indexer* indexer, MessageEngine* engine, SELexicon* lexicon)
{
	RETURN_IF_ERROR(m_message_handler.SetCallBack(this, MSG_M2_ALL_MESSAGES_AVAILABLE, reinterpret_cast<MH_PARAM_1>(this)));
	RETURN_IF_ERROR(m_message_handler.SetCallBack(this, MSG_M2_MESSAGE_DELETED_FROM_LEXICON, reinterpret_cast<MH_PARAM_1>(this)));
	RETURN_IF_ERROR(m_message_handler.SetCallBack(this, MSG_M2_COMMIT_DATA, reinterpret_cast<MH_PARAM_1>(this)));

	m_store   = store;
	m_indexer = indexer;
	m_lexicon = lexicon;
	m_engine  = engine;

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::AddMessage
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::AddMessage(StoreMessage& message)
{
	message_gid_t new_id;

	// Insert message into store, indexer and lexicon
	RETURN_IF_ERROR(m_store->AddMessage(new_id, message));

	OP_STATUS ret;
	// Index message
	if (OpStatus::IsError(ret = m_indexer->NewMessage(message)))
	{
		// we should never have message in the store if they couldn't be added in the indexes
		// TODO make error handler for SEMessageDatabase
		// OnError(Str::S_MESSAGE_INDEXING_FAILED);
		// remove it from all indexes and from store
		OpStatus::Ignore(RemoveMessage(new_id));
		
		return ret;
	}

	Index* index   = m_indexer->GetIndexById(IndexTypes::SPAM);
	BOOL   is_spam = index && index->Contains(new_id);

	// If the message was spam, insert only headers
	if (!m_store->GetMessageFlag(new_id, StoreMessage::IS_WAITING_FOR_INDEXING))
	{
		RETURN_IF_ERROR(m_store->SetMessageFlag(new_id, StoreMessage::IS_WAITING_FOR_INDEXING, TRUE));
		RETURN_IF_ERROR(m_lexicon->InsertMessage(new_id, is_spam));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::UpdateMessage
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::UpdateMessageMetaData(StoreMessage& message)
{
	return m_store->UpdateMessage(message);
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::UpdateMessageData
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::UpdateMessageData(StoreMessage& message)
{
	RETURN_IF_ERROR(m_store->SetRawMessage(message));

	// TODO: This should probably delete a message and reinsert it, not just NewMessage
	// for both indexer and lexicon
	RETURN_IF_ERROR(m_indexer->NewMessage(message));

	Index* spam_index = m_indexer->GetIndexById(IndexTypes::SPAM);
	BOOL   is_spam    = spam_index && spam_index->Contains(message.GetId());

	RETURN_IF_ERROR(m_store->SetMessageFlag(message.GetId(), StoreMessage::IS_WAITING_FOR_INDEXING, TRUE));
	RETURN_IF_ERROR(m_lexicon->InsertMessage(message.GetId(), is_spam));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::RemoveMessage
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::RemoveMessage(message_gid_t message_id)
{
	// when removing permanently we want to mark it as removed first (in case it crashes during removing from indexes)
	// if it crashes, it will either have the mark as removed flag or not be removed from indexes

	// if marking as removed fails, it's probably because it's a ghost, and we want to continue and remove from indexes anyway
	OpStatus::Ignore(m_store->MarkAsRemoved(message_id)); 
	RETURN_IF_ERROR(m_indexer->RemoveMessage(message_id));
	RETURN_IF_ERROR(m_lexicon->RemoveMessage(message_id, reinterpret_cast<MH_PARAM_1>(this)));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::GetMessage
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::GetMessage(message_gid_t message_id, BOOL full, StoreMessage& message)
{
	RETURN_IF_ERROR(m_store->GetMessage(message, message_id));

	if (full)
		RETURN_IF_ERROR(m_store->GetMessageData(message));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::HasFinishedLoading
 ***********************************************************************************/
BOOL SEMessageDatabase::HasFinishedLoading()
{
	return m_store ? m_store->HasFinishedLoading() : FALSE;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::GetIndex
 ***********************************************************************************/
Index* SEMessageDatabase::GetIndex(index_gid_t index_id)
{
	return m_indexer->GetIndexById(index_id);
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::RemoveIndex
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::RemoveIndex(index_gid_t index_id)
{
	Index* index = GetIndex(index_id);

	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	return m_indexer->RemoveIndex(index);
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::RequestCommit
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::RequestCommit()
{
	if (m_commit_requested)
		return OpStatus::OK;

	m_message_handler.PostMessage(MSG_M2_COMMIT_DATA, reinterpret_cast<MH_PARAM_1>(this), 0, CommitDelay);
	m_commit_requested = TRUE;

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::Commit
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::Commit()
{
	RETURN_IF_ERROR(m_store->CommitData());
	RETURN_IF_ERROR(m_indexer->CommitData());
	
	if (!m_grouped_search_engine_files->IsFullyCommitted())
		return OpStatus::ERR;

	for (OpListenersIterator iterator(m_commit_listeners); m_commit_listeners.HasNext(iterator);)
	{
		m_commit_listeners.GetNext(iterator)->OnCommitted();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::AddCommitListener
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::AddCommitListener(CommitListener* listener)
{
	return m_commit_listeners.Add(listener);
}


/***********************************************************************************
 **
 **
 ** SEMessageDatabase::RemoveCommitListener
 ***********************************************************************************/
OP_STATUS SEMessageDatabase::RemoveCommitListener(CommitListener* listener)
{
	return m_commit_listeners.Remove(listener);
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::GroupSearchEngineFile
 ***********************************************************************************/
void SEMessageDatabase::GroupSearchEngineFile(SearchGroupable& group_member)
{
	if (m_grouped_search_engine_files)
		m_grouped_search_engine_files->GroupWith(group_member);
	else
		m_grouped_search_engine_files = &group_member;
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::HandleCallback
 ***********************************************************************************/
void SEMessageDatabase::HandleCallback(OpMessage message, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (message)
	{
		case MSG_M2_COMMIT_DATA:
			m_commit_requested = FALSE;
			if (OpStatus::IsError(Commit()))
			{
				OpStatus::Ignore(RequestCommit());
			}
			break;
		case MSG_M2_MESSAGE_DELETED_FROM_LEXICON:
			OpStatus::Ignore(m_store->RemoveMessage(par2));
			break;
		case MSG_M2_ALL_MESSAGES_AVAILABLE:
			m_engine->OnAllMessagesAvailable();
			m_indexer->OnAllMessagesAvailable();
			OpStatus::Ignore(m_indexer->StartSearch()); // resume searches that weren't finished
			break;
	}
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnAllMessagesAvailable
 ***********************************************************************************/
void SEMessageDatabase::OnAllMessagesAvailable()
{
	m_message_handler.PostMessage(MSG_M2_ALL_MESSAGES_AVAILABLE, reinterpret_cast<MH_PARAM_1>(this), 0);
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnMessageChanged
 ***********************************************************************************/
void SEMessageDatabase::OnMessageChanged(message_gid_t message_id)
{
	m_engine->OnMessageChanged(message_id);
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnMessageBodyChanged
 ***********************************************************************************/
void SEMessageDatabase::OnMessageBodyChanged(StoreMessage& message)
{
	m_indexer->OnMessageBodyChanged(message);
	m_engine->OnMessageBodyChanged(message);

	OpStatus::Ignore(m_store->SetMessageFlag(message.GetId(), StoreMessage::IS_WAITING_FOR_INDEXING, TRUE));
	OpStatus::Ignore(m_lexicon->InsertMessage(message.GetId()));
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnMessageMarkedAsSent
 ***********************************************************************************/
void SEMessageDatabase::OnMessageMarkedAsSent(message_gid_t message_id)
{
	m_indexer->MessageSent(message_id);
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnMessageNeedsReindexing
 ***********************************************************************************/
void SEMessageDatabase::OnMessageNeedsReindexing(message_gid_t message_id)
{
	OpStatus::Ignore(m_lexicon->InsertMessage(message_id));
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::OnMessageMadeAvailable
 ***********************************************************************************/
void SEMessageDatabase::OnMessageMadeAvailable(message_gid_t message_id, BOOL read)
{
	// add to indexes first
	m_indexer->OnMessageMadeAvailable(message_id, read);
	m_engine->OnMessageMadeAvailable(message_id, read);
}

/***********************************************************************************
 **
 **
 ** SEMessageDatabase::CreateMessageChangeLock
 ***********************************************************************************/
MessageDatabase::MessageChangeLock* SEMessageDatabase::CreateMessageChangeLock()
{
	return OP_NEW(MessageEngine::MultipleMessageChanger, ());
}

#endif // M2_SUPPORT
