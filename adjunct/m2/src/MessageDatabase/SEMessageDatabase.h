/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef SE_MESSAGE_DATABASE_H
#define SE_MESSAGE_DATABASE_H

#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/oplisteners.h"

class MessageStore;
class Indexer;
class SELexicon;
class MessageEngine;
class MessageHandler;

/** @brief A message database implemented with search_engine
  */
class SEMessageDatabase
  : public MessageDatabase
  , public MessageObject
{
public:
	/** Constructor
	  * @param message_handler Message handler to use for asynchronous callbacks
	  */
	SEMessageDatabase(MessageHandler& message_handler);

	/** Destructor
	  */
	virtual ~SEMessageDatabase();

	/** Initialize the store - call before calling any other functions and check return value
	  * @param store Backend store to use
	  * @param indexer Backend indexer to use
	  * @param engine MessageEngine to use
	  * @param lexicon Backend lexicon to use
	  */
	virtual OP_STATUS Init(MessageStore* store, Indexer* indexer, MessageEngine* engine, SELexicon* lexicon);

	// From MessageDatabase
	virtual OP_STATUS AddMessage(StoreMessage& message);
	virtual OP_STATUS UpdateMessageMetaData(StoreMessage& message);
	virtual OP_STATUS UpdateMessageData(StoreMessage& message);
	virtual OP_STATUS RemoveMessage(message_gid_t message_id);
	virtual OP_STATUS GetMessage(message_gid_t message_id, BOOL full, StoreMessage& message);
	virtual OP_STATUS MessageSent(message_gid_t message_id) { return OpStatus::OK; }
	virtual OP_STATUS ArchiveMessage(message_gid_t message_id) { return OpStatus::OK; }
	virtual BOOL HasFinishedLoading();

	virtual Index* GetIndex(index_gid_t index_id);
	virtual OP_STATUS RemoveIndex(index_gid_t index_id);

	virtual OP_STATUS RequestCommit();
	virtual OP_STATUS Commit();
	virtual OP_STATUS AddCommitListener(CommitListener* listener);
	virtual OP_STATUS RemoveCommitListener(CommitListener* listener);
	
	/** Group the various search_engine files (ie. SingleBTree, StringTable) so they are commited together
	 */
	virtual void GroupSearchEngineFile(SearchGroupable &group_member);

	// Functions for communicating between Store, Engine and Indexer
	virtual void OnMessageMadeAvailable(message_gid_t message_id, BOOL read);
	virtual void OnAllMessagesAvailable();
	virtual void OnMessageChanged(message_gid_t message_id);
	virtual void OnMessageBodyChanged(StoreMessage& message);
	virtual void OnMessageMarkedAsSent(message_gid_t message_id);
	virtual void OnMessageNeedsReindexing(message_gid_t message_id);

	virtual bool IsInitialized() { return m_store && m_indexer && m_lexicon && m_engine; }

	virtual MessageChangeLock* CreateMessageChangeLock();

	// From MessageObject
	virtual void	  HandleCallback(OpMessage message, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	static const long CommitDelay = 5000; ///< Delay for flushing database to stable storage after changes

	MessageStore*	m_store;
	Indexer*		m_indexer;
	SELexicon*		m_lexicon;
	MessageHandler& m_message_handler;
	MessageEngine*	m_engine;
	SearchGroupable* m_grouped_search_engine_files;

	OpListeners<CommitListener> m_commit_listeners;
	BOOL m_commit_requested;
};

#endif // SE_MESSAGE_DATABASE_H
