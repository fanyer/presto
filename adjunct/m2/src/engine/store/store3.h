// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef ENGINE_STORE3_H
#define ENGINE_STORE3_H

#define Store3 Store
#define Store2 Store

#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/desktop_util/async_queue/AsyncQueue.h"
#include "adjunct/desktop_util/thread/DesktopMutex.h"
#ifdef SELFTEST
#include "adjunct/m2/selftest/overrides/ST_BlockStorage.h"
#include "adjunct/m2/selftest/overrides/ST_Cursor.h"
#include "adjunct/m2/selftest/overrides/ST_SingleBTree.h"
#else
typedef class BlockStorage CursorDBBackend;
typedef class BlockStorage CursorDBBackendCreate;
typedef class BSCursor CursorDB;
typedef class BSCursor CursorDBCreate;
#define BTreeDB SingleBTree
#define BTreeDBCreate SingleBTree
#endif
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/store/mboxstoremanager.h"
#include "adjunct/m2/src/engine/store/storecache.h"
#include "adjunct/m2/src/engine/store/duplicatetable.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/m2/src/MessageDatabase/MessageStore.h"
#include "adjunct/m2/src/glue/mh.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/search_engine/Cursor.h"
#include "modules/search_engine/SingleBTree.h"
#include "modules/util/adt/oplisteners.h"

class Index;
class MessageDatabase;
class ProgressInfo;

class Store3 : public MessageStore
{
public:
	enum SortType
	{
		SORT_BY_ID,
		SORT_BY_MESSAGE_ID_HASH,
		SORT_BY_SENT_DATE,
		SORT_BY_THREADED_SENT_DATE,
		SORT_BY_THREADED_SENT_DATE_DESCENDING,
		SORT_BY_SIZE,
		SORT_BY_FLAGS,
		SORT_BY_STATUS,
		SORT_BY_ATTACHMENT,
		SORT_BY_MBOX_POS,
		SORT_BY_LABEL,
		SORT_BY_ACCOUNT_ID,
		SORT_BY_PARENT,
		SORT_BY_PREV_FROM,
		SORT_BY_PREV_TO,
		SORT_BY_PREV_SUBJECT
	};

	struct MessageIdKey
	{
		MessageIdKey() {}
		MessageIdKey(unsigned message_id_hash, message_gid_t m2_id) { data[0] = message_id_hash; data[1] = m2_id; }

		BOOL operator<(const MessageIdKey& other) const
		{
			return data[0] < other.data[0] ||
					(data[0] == other.data[0] && data[1] < other.data[1]);
		}

		unsigned      GetMessageIdHash() const { return data[0]; }
		message_gid_t GetM2Id()          const { return data[1]; }

		UINT32 data[2];
	};

	/** Constructor
	  */
	Store3(MessageDatabase* message_database,
		   CursorDB* cursor_db = OP_NEW(CursorDBCreate, (TRUE)),
		   CursorDBBackend* backend = OP_NEW(CursorDBBackendCreate, ()),
		   BTreeDB<MessageIdKey>* index = OP_NEW(BTreeDBCreate<MessageIdKey>, ()));

	/** Destructor
	  */
	virtual ~Store3();

	/** Call this before calling the destructor
	  */
	void PrepareToDie();

	/** Initialize the store - call before calling any other functions and check return value
	  * @param store_path Path to the store directory, leave empty for default
	  */
	OP_STATUS Init(OpString& error_message, const OpStringC& store_path = UNI_L(""));

	/** Reinitialize the store (start from scratch)
	  */
	OP_STATUS ReInit();

	/**
	 * Adds a (new) message to the Store and Index. Drafts are stored with draft = TRUE
	 * and all updates on drafts must be with SetRawMessage(, draft = TRUE);
	 * When a draft has been sent, a RemoveMessage(id) should be called before
	 * a AddMessage(, draft = FALSE) is done.
	 *
	 * If a message with this id already exists, it will be replaced (including
	 * the 'received' date of that message!)
	 */
	OP_STATUS AddMessage(message_gid_t& id, StoreMessage& message, BOOL draft, BOOL headers_only);

	// From MessageStore
	virtual OP_STATUS AddMessage(message_gid_t& id, StoreMessage& message) { return AddMessage(id, message, FALSE, !message.GetRawBody()); }

	/**
	 * Returns highest unique ID given to a message in Store
	 */
	message_gid_t GetLastId();

	/**
	 * Finds the timestamp for a given 'age' before now
	 */
	static time_t GetTimeByAge(IndexTypes::ModelAge age);

	/**
	 * Returns the Id of the parent (by threading) of given child Id
	 */
	message_gid_t GetParentId(message_gid_t child) { return GetFromCache(child).parent_id; }

	/**
	 
	 * Returns the vector of message ids of all the replies to a specified message (all the direct children) 
	 * @param message_id Message to find the children for
	 * @param children_ids All messages that are children of the given message
	 */
	OP_STATUS GetChildrenIds(message_gid_t message_id, OpINTSet& children_ids);

	/**
	 * Returns a set of all messages in the thread that a specified message is part of
	 * @param message_id Message to find a thread for
	 * @param thread_ids All messages in the thread that contains message_id (includes message_id itself)
	 */
	OP_STATUS GetThreadIds(message_gid_t message_id, OpINTSet& thread_ids);

	/**
	 * Compares two messages according to sort_by, typically for display purposes
	 */
	int CompareMessages(message_gid_t one, message_gid_t two, SortType sort_by);

	/**
	 * Updates the message meta-info on disk with status information from the message
	 * in memory.
	 *
	 * @param draft Used for outgoing messages, going from drafts to outbox to sent and needing frequent updates
	 */
	OP_STATUS UpdateMessage(StoreMessage& message, BOOL draft);
	virtual OP_STATUS UpdateMessage(StoreMessage& message) { return UpdateMessage(message, FALSE); }

	/**
	 * Called when message has been confirmed sent.
	 */
	OP_STATUS MessageSent(message_gid_t id);

	/**
	 * Reads in a Message from disk or cache, but only fills in
	 * essential headers. For full body, call GetMessageData() later.
	 */
	virtual OP_STATUS GetMessage(StoreMessage& message, message_gid_t id) { return GetMessage(message, id, FALSE); }

	/** Reads in a message from cache, and only fills in the headers needed for the indexmodel
	  */ 
	virtual OP_STATUS GetMessageMinimalHeaders(StoreMessage& message, message_gid_t id) { return GetMessage(message, id, FALSE, TRUE); }

	// Setting and getting of message flags and labels
	virtual OP_STATUS SetMessageFlag(message_gid_t id, StoreMessage::Flags flag, BOOL value);
	OP_STATUS SetMessageFlags(message_gid_t id, INT64 flags);

	UINT32	  GetMessageSize(message_gid_t id) { return GetFromCache(id).size; }
	UINT64	  GetMessageFlags(message_gid_t id) { return GetFromCache(id).flags; }
	BOOL	  GetMessageFlag(message_gid_t id, StoreMessage::Flags flag) { return (GetMessageFlags(id) >> flag) & static_cast<UINT64>(1); }
	virtual	  UINT16 GetMessageAccountId(message_gid_t id);
	time_t	  GetMessageDate(message_gid_t id) { return GetFromCache(id).sent_date; }
	message_gid_t GetMessageByMessageId(const OpStringC8& message_id);
	OP_STATUS GetMessageInternetLocation(message_gid_t id, OpString8& internet_location);
	OP_STATUS GetMessageMessageId(message_gid_t id, OpString8& message_id);
	BOOL	  GetMessageHasBody(message_gid_t id) { BOOL temp = FALSE; HasMessageDownloadedBody(id, temp); return temp; }

	/** Get whether there are more messages with the same Message-Id as this message or not
	  */
	bool GetMessageHasDuplicates(message_gid_t id) { return GetFromCache(id).master_id != 0; }

	/** Get a list of m2 ids of messages with the same Message-Id as the one requested
	  */
	const DuplicateMessage* GetMessageDuplicates(message_gid_t id) { return m_duplicate_table.GetDuplicates(GetFromCache(id).master_id); }

	/**
	 * Physically deletes the directory containing messages for account_id (use only if account_id is removed)
	 *   Remove messages in this directory before running this function!
	 */
	OP_STATUS RemoveAccountDirectory(UINT16 account_id) { /* TODO implement */ return OpStatus::OK; }

	/**
	 * Retrieves full message content from the local disk cache if
	 * available.
	 */
	OP_STATUS GetMessageData(StoreMessage& message);

	/**
	 * Retrieves full draft message content from the local disk based on the message id.
	 */
	OP_STATUS GetDraftData(StoreMessage& message, message_gid_t id);

	/**
	 * Checks if a message has a body
	 */
	OP_STATUS HasMessageDownloadedBody(message_gid_t id, BOOL& has_downloaded_body);

	/**
	 * Stores local copy of message data after retrieval of body
	 * from server.
	 */
	OP_STATUS SetRawMessage(StoreMessage& message);

	/**
	  * Checks if a message is currently available from the store
	  */
	BOOL	  MessageAvailable(message_gid_t id);

	/**
	 * Change the storage type for all messages of a certain account
	 */
	OP_STATUS ChangeStorageType(UINT16 account_id, int to_type);

	/**
	 * Mark a message as removed
	 */
	OP_STATUS MarkAsRemoved(message_gid_t id) { return SetMessageFlag(id, StoreMessage::PERMANENTLY_REMOVED, TRUE); }

	/** Request a commit of the store and associated databases
	  */
	OP_STATUS RequestCommit();

	/** Set progress keeper when loading data
	  */
	void	  SetProgressKeeper(ProgressInfo* progress) { m_progress = progress; }

	/** Commit data to disk
	  */
	OP_STATUS CommitData();

	/**
	 * Deletes a message permanently from the message base, to be called by SELexicon
	 */
	OP_STATUS RemoveMessage(message_gid_t id);

	BOOL	  HasFinishedLoading() { return m_reader_queue.Empty(); }

	BOOL	  IsNextM2IDReady()	   { return m_next_m2_id_ready; }

	// From MessageStore
	virtual OpFileLength ReadData(OpFileLength startpos, int blockcount);

	void	  SetReadOnly() { m_readonly = TRUE; }

	/** Remove all instances of Re: FWD: [BTS] [Nonsense] in subjects and return the base subject
	  */
	static OpStringC StripSubject(const OpStringC& subject);

	// Fields used in database - if this is changed, change InitBackend() as well!
	// NB: CHANGING THIS WILL BREAK COMPATIBILITY
	enum StoreField
	{
		STORE_RECV_DATE,
		STORE_SENT_DATE,
		STORE_SIZE,
		STORE_FLAGS,
		STORE_LABEL,
		STORE_ACCOUNT_ID,
		STORE_PARENT,
		STORE_M2_ID,
		STORE_MBX_TYPE,
		STORE_MBX_DATA,
		STORE_FLAGS2,
		STORE_RESERV,
		STORE_FROM,
		STORE_TO,
		STORE_SUBJECT,
		STORE_MESSAGE_ID,
		STORE_INTERNET_LOCATION
	};
protected:
	OP_STATUS GetMessage(StoreMessage& message, message_gid_t id, BOOL import, BOOL minimal_headers = FALSE);

	CursorDB*				  m_maildb;
	CursorDBBackend*		  m_maildb_backend;
	message_gid_t			  m_current_id;			///< m_maildb currently points to this M2 ID

private:
	friend class StoreUpdater; // Must be able to change internal store structures
	friend class SELexicon;    // Only class able to remove messages from store and reindex
	friend class MailRecovery;

	class DeletedMessage : public Link
	{
	public:
		DeletedMessage(message_gid_t p_m2_id) : m2_id(p_m2_id) {}

		message_gid_t   m2_id;
	};

	OP_STATUS RemoveMessages(const OpINT32Vector& id_vector);

	/**
	 * Reindex all messages in lexicon. Needs to be called before store is loaded
	 */
	void	  Reindex() { m_needs_reindexing = TRUE; }

	void	  MakeMessageAvailable(message_gid_t message_id);
	OP_STATUS RemoveRawMessage(message_gid_t id);
	OP_STATUS UpdateThreads(StoreMessage& message);
	int		  CompareStrings(const OpStringC& field1, const OpStringC& field2, StoreField compare, bool strip_subject);
	OpStringC StripAddress(const OpStringC& address);
	OP_STATUS GotoMessage(message_gid_t id);
	OP_STATUS CreateM2ID(StoreMessage& message);
	int		  CompareMessagesWithSortCache(message_gid_t one, message_gid_t two, SortType sort_by);
	int		  CompareFlagRange(int flags1, int flags2, int range_start, int range_end);
	OP_STATUS UpdateCache(message_gid_t id);
	void      RemoveFromCache(message_gid_t id);
	const StoreItem& GetFromCache(message_gid_t id);
	OP_STATUS AddToDuplicateTable(StoreItem &item);
	void	  RemoveFromDuplicateTable(StoreItem& cache_item);

	void	  AlertMessageChanged(message_gid_t id);
	OP_STATUS InitBackend(OpString& error_message, const OpStringC& storage_path);
	OP_STATUS InitMailbase(OpString& error_message, const OpStringC& storage_path);
	OP_STATUS InitMessageIdIndex(OpString& error_message, const OpStringC& storage_path);
#ifdef M2_KESTREL_BETA_COMPATIBILITY
	OP_STATUS InitM2IdIndex(const OpStringC& storage_path);
#endif // M2_KESTREL_BETA_COMPATIBILITY
	OP_STATUS InitCaches();

	UINT64 ReadFlags();
	OP_STATUS WriteFlags(UINT64 flags);

	OP_STATUS CleanUpDeletedMessages();
	OP_STATUS WriteNextM2ID();

	MboxStoreManager		  m_mbox_store_manager;
	MessageDatabase*		  m_message_database;
	ProgressInfo*			  m_progress;

	BTreeDB<MessageIdKey>*	  m_message_id_index;

	BOOL					  m_readonly;			///< Store is readonly, used for import (reading external store)
	BOOL					  m_next_m2_id_ready;	///< Store can start adding messages as soon as the next m2 id is ready
	BOOL					  m_needs_reindexing;	///< Store needs to reindex lexicon
	message_gid_t			  m_next_m2_id;			///< highest m2_id used + 1, stored in the user headers of the m_maildb_backend
	DesktopMutex			  m_mutex;				///<
	AsyncQueue				  m_reader_queue;
	StoreCache				  m_cache;
	DuplicateTable			  m_duplicate_table;	///< keeps track of the duplicate messages, to deal with sorting and hiding duplicates from a view
	Head					  m_deleted_messages;     ///< Messages that should be deleted from store on next commit

};

#endif // ENGINE_STORE3_H
