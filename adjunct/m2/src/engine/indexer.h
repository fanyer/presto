/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ---------------------------------------------------------------------------------

#ifndef INDEXER_H
#define INDEXER_H

#ifdef M2_SUPPORT

// ---------------------------------------------------------------------------------

#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/index/indexcontainer.h"
#include "adjunct/m2/src/engine/index/indexgroup.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

#include "adjunct/m2/src/engine/selexicon.h"
#include "modules/search_engine/StringTable.h"

#include "modules/prefsfile/prefsfile.h"

// ---------------------------------------------------------------------------------

class Indexer : public MessageLoop::Target, Index::Observer, AccountListener
{
public:
	OP_STATUS AddIndexerListener(IndexerListener* listener) { return m_indexer_listeners.Add(listener); }
	OP_STATUS RemoveIndexerListener(IndexerListener* listener) { return m_indexer_listeners.RemoveByItem(listener); }

	OP_STATUS AddCategoryListener(CategoryListener* listener) { return m_category_listeners.Add(listener); }
	OP_STATUS RemoveCategoryListener(CategoryListener* listener) { return m_category_listeners.RemoveByItem(listener); }

	Indexer(MessageDatabase* message_database);
	virtual ~Indexer();

	/**
	 * To be called on exit, but before AccountManager is destroyed
	 */
	void PrepareToDie();

	/**
	 * Read ini file, create indexes, can possibly be done by the UI.
	 */
	OP_STATUS Init(OpStringC &indexer_filename, OpString8& status);

	/** 
	 */
	OP_STATUS ResetToDefaultIndexes() { RETURN_IF_ERROR(SetDefaultParentIds()); RETURN_IF_ERROR(AddDefaultCategories()); return SetPanelIndexesVisible(); }

	/**
	 * Get the list of categories visible in the mail panel
	 */
	OP_STATUS GetCategories(OpINT32Vector& categories);

	/**
	 * Update translated folder names
	 */
	OP_STATUS UpdateTranslatedStrings();

	/**
	 * Adds (and translates) all the basic M2 virtual folders (unread, account views etc.)
	 */
	OP_STATUS AddBasicFolders();

	/**
	 * Add a new index
	 *
	 * @param save Save the index to disk immediately - may choose to do so manually after calling index->SetFile().
	 */
	OP_STATUS NewIndex(Index* index, BOOL save = TRUE);	

	OP_STATUS RemoveIndex(Index* index, BOOL commit = TRUE);

	OP_STATUS UpdateIndex(index_gid_t id);

	/**
	 * Remove messages in this index from store and index
	 */
	OP_STATUS RemoveMessagesInIndex(Index* index);

	/**
	 * Get a pointer to an index in range [range_start, range_end)
	 *  Returns the index from the iterator (first index if iterator is -1) in the range, or NULL if
	 *  such an index can't be found
	 *
	 *  Ex. usage:
	 *   it = -1;
	 *   while (index = GetRange(FIRST_SEARCH, LAST_SEARCH, it))
	 *      // Do something with index
	 *
	 * @param range_start Start of the range to search
	 * @param range_end End of the range to search
	 * @param iterator a previously received iterator from this function, or -1 to start
	 * @return Index* the index if it could be found, NULL otherwise
	 */
	Index* GetRange(UINT32 range_start, UINT32 range_end, INT32& iterator) { return m_index_container.GetRange(range_start, range_end, iterator); }

	/** Get a range for the indexes for a specific account
	 *  NB: results still need to be checked with index->GetAccountId()!
	 * @param account The account to get a range for
	 * @param range_start Start of the range
	 * @param range_end End of the range
	 */
	void GetAccountRange(const Account* account, UINT32& range_start, UINT32& range_end);

	/**
	 * Get a pointer to an index
	 *
	 * @param index_id index_id of the index to be retrieved
	 * @return Index* the retrieved index
	 */
	inline Index* GetIndexById(index_gid_t index_id) { 
		Index* index = m_index_container.GetIndex(index_id); 
		return index ? index : (IsSpecialIndex(index_id) ? GetSpecialIndexById(index_id) : 0);
	}

	/**
	 * Special version to get the index of a given
	 * e-mail address
	 */
	Index* GetContactIndex(const OpStringC16& address, BOOL visible = TRUE);

	/**
	 * Special version to get the index of a contact with a given e-mail address,
	 * including indexes of other e-mail addresses from the same contact.
	 */
	Index* GetCombinedContactIndex(OpString& address);

	/**
	 * Hide a certain type of messages from the Unread access point
	 */
	OP_STATUS UpdateHideFromUnread();
	OP_STATUS UpdateHiddenViews();

	Index* GetSubscribedFolderIndex(const Account* account, const OpStringC& folder_path, char path_delimiter, const OpStringC& folder_name, BOOL create_if_missing, BOOL ask_before_creating);

	index_gid_t GetRSSAccountIndexId();

	UINT32 IndexCount() { return m_index_container.GetCount(); }

	/** Creates a new index with a search that it starts, can be given an existing index as well
	 * 
	 * @param search_text			the string to search for
	 * @param option				SearchTypes::Option used for the search (EXACT_PHRASE is default)
	 * @param field					SearchTypes::Field used for the search (ENTIRE_MESSAGE is default)
	 * @param start_date			Only search for messages after start_date (default is 0)
	 * @param end_date				Only search for messages before end_date (default is -1)
	 * @param id 					output, the id of the new index (only interesting if newly created)
	 * @param search_only_in		search only through the messages in a certain index
	 * @param existing_index		Index* that we should reuse an existing index
	 */
	OP_STATUS StartSearch(const OpStringC& search_text, SearchTypes::Option option, SearchTypes::Field field, UINT32 start_date, UINT32 end_date, index_gid_t& id, index_gid_t search_only_in = 0, Index* existing_index = NULL);

	/**
	 * Typically called from Engine to start a cycle of
	 * searching through mail.
	 */
	OP_STATUS StartSearch();

	/**
	 * Continues searches in all indexes with
	 * active searches.
	 */
	BOOL ContinueSearch();

	/**
	 * Creates an Index containing items that exist in both of the
	 * original Indexes.
	 *
	 * @param limit Max number of items to search through in each list.
	 */
	OP_STATUS IntersectionIndexes(Index &result, Index *first, Index *second, int limit);

	/**
	 * Creates an Index containing items that exist in any of the
	 * original Indexes.
	 *
	 * @param limit Max number of items to search through in each list.
	 */
	OP_STATUS UnionIndexes(Index &result, Index *first, Index *second, int limit);

	/**
	 * Creates an Index containing items from the first Index that are NOT
	 * contained in the second.
	 *
	 * @param limit Max number of items to search through in each list.
	 */
	OP_STATUS ComplementIndexes(Index &result, Index *first, Index *second, int limit);

	/**
	 * Mark message as read or unread, move from unread folder
	 */
	OP_STATUS MessageRead(message_gid_t id, BOOL read, BOOL new_message = FALSE);

	/**
	 * Move from spam folder and add sender/list-id to contact list
	 */
	OP_STATUS NotSpam(message_gid_t message, BOOL only_add_contacts, int parent_folder_id);
	OP_STATUS Spam(message_gid_t message);
	OP_STATUS SetSpamLevel(INT32 level) { TRAPD(err, m_prefs->WriteIntL(UNI_L("Spam Filter"), UNI_L("Start Score"), level)); return err; }
	INT32	  GetSpamLevel();

	/**
	 * Change the 'sent', 'spam' or 'trash' status of all messages in a specific index
	 * @param index Index to operate on
	 * @param AccountTypes::FolderPathType whether it's a type sent, spam or trash
	 * @param whether the messages should be marked or not as that special status
	 */
	OP_STATUS ChangeSpecialStatus(Index* index, AccountTypes::FolderPathType type, BOOL value);

	void SetCategoryOpen(index_gid_t index_id, BOOL open);
	BOOL GetCategoryOpen(index_gid_t index_id) const;

	void UpdateCategoryUnread(index_gid_t index_id);
	void SetCategoryUnread(index_gid_t index_id, UINT32 unread_messages);
	UINT32 GetCategoryUnread(index_gid_t index_id) const;

	void SetCategoryVisible(index_gid_t index_id, BOOL visible);
	BOOL GetCategoryVisible(index_gid_t index_id) const;

	OP_STATUS MoveCategory(index_gid_t id, UINT32 new_position);

	INT32 GetCategoryPosition(index_gid_t category) const;

	BOOL IsCategory(index_gid_t index_id) { return (IndexTypes::FIRST_CATEGORY <= index_id && index_id <= IndexTypes::LAST_CATEGORY) || (IndexTypes::FIRST_ACCOUNT <= index_id && index_id <= IndexTypes::LAST_ACCOUNT) ? TRUE : FALSE; }

	message_gid_t CurrentlyIndexing() { return m_currently_indexing; }

	/**
	 * Use this message to filter similar messages to the chosen view
	 */
	OP_STATUS UpdateAutoFilter(Message* message, Index* index, BOOL remove_message = FALSE);

	/**
	 * Automatically filter a message to index based on its content and
	 * the content of messages added to the autofilter of index
	 */
	OP_STATUS AutoFilter(Message* message, const OpStringC& body_text, Index* index, double& score);

	/**
	 * Delayed saving of indexes. Normal to use during download..
	 */
	OP_STATUS SaveRequest();

	/**
	 * Get the ID for a keyword (will create the ID if it doesn't exist yet)
	 * @param keyword Keyword to get ID for
	 * @return An ID if one could be found/created, or -1 on error
	 */
	int		  GetKeywordID(const OpStringC8& keyword);

	/**
	 * Commit pending changes to stable storage
	 */
	OP_STATUS CommitData();

	// From Target:
	OP_STATUS Receive(OpMessage message);

	// From Index::Observer
	OP_STATUS MessageAdded(Index* index, message_gid_t message_id, BOOL setting_keyword);
	OP_STATUS MessageRemoved(Index* index, message_gid_t message_id, BOOL setting_keyword);
	OP_STATUS StatusChanged(Index* index);
	OP_STATUS NameChanged(Index* index);
	OP_STATUS VisibleChanged(Index* index);
	OP_STATUS IconChanged(Index* index);

	// From AccountListener
	void OnAccountAdded(UINT16 account_id);
	void OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
	void OnAccountStatusChanged(UINT16 account_id) {}
	void OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
	void OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
	void OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
	void OnFolderLoadingCompleted(UINT16 account_id) {}

	BOOL HasTextLexicon() {return m_text_table != NULL;}
	OP_STATUS FindWords(const uni_char *prefix, uni_char **result, int *count) {return m_text_table ? m_text_table->WordSearch(prefix, result, count) : OpStatus::ERR_NULL_POINTER;}
	OP_STATUS AsyncFindPartialWords(const OpStringC& words, MH_PARAM_1 callback) {return m_text_table ? m_text_table->AsyncMultiSearch(words, callback): OpStatus::ERR_NULL_POINTER;}

	OP_STATUS AddIndexWord(uni_char *word, int id) {return m_index_table.Insert(word, id);}
	OP_STATUS RemoveIndexWord(uni_char *word, int id) {return m_index_table.Delete(word, id);}
	OP_STATUS FindIndexWord(const uni_char *word, OpINT32Set &result);

	enum LexiconIdSpace
	{
		// Only use the highest 3 bits for this
		MESSAGE_BODY			= 0,
		MESSAGE_SUBJECT			= 1 << 29,
		MESSAGE_FROM			= 2 << 29,
		MESSAGE_TO				= 3 << 29,
		MESSAGE_CC				= 4 << 29,
		MESSAGE_REPLYTO			= 5 << 29,
		MESSAGE_NEWSGROUPS		= 6 << 29,

		// place to index Organization etc.
		MESSAGE_OTHER_HEADERS	= 7 << 29,

		// magic values, not used in the indexer itself:
		MESSAGE_ALL_HEADERS		= 1,
		MESSAGE_ENTIRE_MESSAGE	= 2
	};

	OP_STATUS FindWords(const OpStringC& words, OpINTSortedVector& id_list, BOOL partial_match_allowed = FALSE, LexiconIdSpace id_space = MESSAGE_ENTIRE_MESSAGE);

	/**
	 * Saves list of all current indexes to file. Called by the
	 * destructor, which is why it doesn't return an error value,
	 * no matter what. Also used by engine when we update an index
	 * manually.
	 */
	void SaveAllToFile(BOOL commit = TRUE);

	/**
	 * Iterate through indexes and put message in indexes that accept it
	 * Also, creates Contact Index if it doesn't exist.
	 */
	OP_STATUS NewMessage(Message& message);
	OP_STATUS NewMessage(StoreMessage& message);

	void OnMessageBodyChanged(StoreMessage& message);

	void OnMessageMadeAvailable(message_gid_t message_id, BOOL read);

	void OnAllMessagesAvailable();
	/**
	 * Iterate through internal indexes and remove message from each
	 * in-memory index. Also, removes from Contact Index and possibly
	 * removes Contact index file.
	 * Will remove message from store if there's no index left that it's in.
	 * @param except_from Don't remove from this index
	 */
	OP_STATUS RemoveMessage(message_gid_t message_id, index_gid_t except_from = 0);

	/**
	 * Set keywords on a message
	 * @param id ID of the message the keywords should be set for
	 * @param keywords Keywords that this message should have (list of keyword ids)
	 */
	OP_STATUS SetKeywordsOnMessage(message_gid_t id, const OpINTSortedVector* keywords, bool new_message);

	/**
	 * Update the lexicon (search index) with the contents of this message.
	 * Use when a message has been changed that doesn't need any further processing
	 * (e.g. drafts)
	 * @param message_id Message to update
	 */
	OP_STATUS UpdateLexicon(message_gid_t message_id) { return m_text_table->InsertMessage(message_id); }

	/** Get a vector with children for a certain index
	  * @param index_id Which index to get the children for
	  * @param children Index ids of children
	  * @param only_direct_children Whether to only include direct
	  */
	OP_STATUS GetChildren(index_gid_t index_id, OpINT32Vector& children, BOOL only_direct_children = FALSE);

	/** Check if an index has children
	  * @param index_id Index to check
	  * @return Whether the index has children
	  */
	BOOL	  HasChildren(index_gid_t index_id);

	/** Get or create a feed folder with a specified name in as a subfolder of a certain index
	 * @param parent_index - the folder should be a child of this index
	 * @param name - the name of the folder we're looking for
	 * @result index or NULL
	 */
	Index*	  GetFeedFolderIndexByName(index_gid_t parent_id, const OpStringC& name);

	/**
	* Returns the thread index for a message (NULL if it doesn't exist)
	*/
	Index* GetThreadIndex(message_gid_t message_id);

	/**
	* Create a new index containing all messages in that thread, save_to_file needs to be set to FALSE for temporary indexes (ie. go to thread indexes)
	*/
	OP_STATUS CreateThreadIndex(const OpINTSet& thread_ids, Index*& index, BOOL save_to_file = TRUE);

	/**
	* Create a new index group that groups all child indexes with a UnionIndexGroup
	*/
	OP_STATUS CreateFolderGroupIndex(Index*& folder_index, index_gid_t parent_id);

	/**
	 * Call when parent for an index changes, to correct parent/child relations
	 * @param index_id ID of index of which parent changed
	 * @param old_parent_id
	 * @param new_parent_id
	 */
	OP_STATUS OnParentChanged(index_gid_t index_id, index_gid_t old_parent_id, index_gid_t new_parent_id);
	
	/** Get a list of headers needed for each new message
	  * @param headers Output: space-separated list of needed headers
	  */
	OP_STATUS GetNeededHeaders(OpString8& headers);

	/**
	  * Set a keyword for an index (empty for no keyword)
	  */
	OP_STATUS SetKeyword(Index* index, const OpStringC8& keyword, BOOL alert_listeners = FALSE);

	/**
	  * Add a message to the spam index without updating the spam autofilter
	  */
	OP_STATUS SilentMarkMessageAsSpam(message_gid_t message_id);

	/** 
	  * Set a search to search in another index, sets up an indexgroup with the correct indexes, etc
	  */
	OP_STATUS SetSearchInIndex(index_gid_t label_index_id, index_gid_t search_in_id);

	/** 
	  * Get the index that an label rule is searching in, by fetching it from the IntersectionIndex
	  */
	index_gid_t GetSearchInIndex(index_gid_t label_index_id);

	SELexicon* GetLexicon() { return m_text_table; }

	/**
	  * Add or remove a message to the index, which contains flagged messages
	  *
	  * @param message_id is a message id
	  * @param flagged If this flag is TRUE, add a message to the index; remove a message from the index in other case.
	  */
	OP_STATUS AddToPinBoard(message_gid_t message_id, BOOL flagged);

protected:
	friend class Store;
	friend class SEMessageDatabase;

	/**
	 * Same as NewMessage, but for reindexing a message when body is decoded or
	 * downloaded (like for IMAP/NNTP). (also reinvokes spam filter, filters, etc)
	 */
	OP_STATUS ChangedMessage(Message& message);

	/**
	 * Called when message has been confirmed sent.
	 */
	OP_STATUS MessageSent(message_gid_t message_id);

private:
	friend class GroupsModel;
	friend class MessageEngine;

	OP_STATUS NewMessage(Message& message, BOOL continue_search, UINT32 &searches_in_progress);

	/**
	 * Add message to the contacts found in header
	 *   Add contact if the contact doesn't exist yet and user has add contacts when sending set
	 */
	OP_STATUS AddMessageToContacts(Message *message, Header *header);

	/**
	 * Find needle in haystack, depending on what search option is set.
	 */
	BOOL Match(const OpStringC16& haystack, const OpStringC16& needle, SearchTypes::Option option);

	OP_STATUS GetListId(Message* message, BOOL& list_found, OpString& list_id, OpString& list_name);

	/**
	 * Checks the currently subscribed items in every NNTP account
	 * and makes sure there is a corresponding item in the indexer.
	 */
	OP_STATUS UpdateNewsGroupIndexList();

	/**
	 * Add a folder with known Id and name
	 */
	OP_STATUS AddFolderIndex(index_gid_t id, OpStringC folder_name, BOOL create_file);

	OP_STATUS DeserializeData();

	/**
	 * Create Inbox and Sent indexes for new POP accounts
	 */
	OP_STATUS AddPOPInboxAndSentIndexes(UINT16 account_id);

	/**
	 * Save index information to Index.ini
	 * @param commit TRUE to flush and commit PrefsFile object, very time-consuming on init/exit
	 */
	OP_STATUS SaveToFile(Index *index, UINT32 index_number, BOOL commit = TRUE);

	/**
	 * Add a message to it's thread index if it exists
	 */
	OP_STATUS AddToActiveThreads(Message& message);

	/** Update the index for a category (after creation of the group for this category)
	  */
	OP_STATUS UpdateCategory(index_gid_t index_id);
	
	/** Add a category, and notify listeners
	  */
	OP_STATUS AddCategory(index_gid_t index_id);
	
	/** Remove a category and notify listeners
	  */
	OP_STATUS RemoveCategory(index_gid_t index_id);

	OP_STATUS RemoveFromFile(UINT32 index_number);


	/** When removing an index, clean up this index from the child vectors
	 */
	OP_STATUS RemoveFromChildVectors(Index* index, BOOL commit);

	/**
	 * Move to the Spam folder if score is lower than 0.
	 * Start score can be tweaked in index.ini
	 * [Spam Filter]
	 * Start Score=30
	 */
	OP_STATUS SpamFilter(Message *message, BOOL& is_spam, OpString& body_text);

	/** Gets a generated index that is not in the normal containers */
	inline BOOL IsSpecialIndex(index_gid_t index_id) { return index_id == IndexTypes::UNREAD_UI || index_id == IndexTypes::HIDDEN || index_id >= IndexTypes::FIRST_CATEGORY; }
	Index* GetSpecialIndexById(index_gid_t index_id);

	static int KeywordCompare(const Index& index1, const Index& index2) { return index1.GetKeyword().Compare(index2.GetKeyword()); }
	
	/**
	 * Call when a new index is added, checks if headers are needed on new messages
	 * @param index non-NULL pointer to new index
	 */
	OP_STATUS HandleHeadersOnNewIndex(Index* index);

	// Helper functions
	static Store* GetStore();

	//
	OP_STATUS LoadIndexerData();
	OP_STATUS LoadBasicIndexData(const OpStringC8 current_index, index_gid_t& index_id, OpString& index_name, IndexTypes::Type& index_type, bool& goto_next_index);
	OP_STATUS LoadIndexFlags(const OpStringC8& current_index, UINT32& index_flags);
	OP_STATUS LoadIndexData(const OpStringC8& current_index, Index* index, INT32 index_flags, INT32 default_model_flags);
	OP_STATUS LoadSearchData(const OpStringC8& current_index, Index* search_index);
	OP_STATUS CreateFolderIndexGroup(Index*& index, index_gid_t index_id);
	OP_STATUS CreateIndexGroup(Index*& index, const OpStringC8& current_index, index_gid_t index_id, IndexTypes::Type index_type, bool& goto_next_index);
	OP_STATUS CreateIntersectionIndexGroup(Index*& index, Index*& search_index, index_gid_t index_id, index_gid_t search_id, index_gid_t search_only_in);
	OP_STATUS PrefetchImportantIndexes();
	// Mapping of indexes and their direct children.
	class IndexChilds
	{
	public:
		IndexChilds(index_gid_t index_id) : m_index_id(index_id) { }

		OP_STATUS AddChild(index_gid_t index_id) { return m_childs.Add(INT32(index_id)); }
		OP_STATUS RemoveChild(index_gid_t index_id) { return m_childs.RemoveByItem(index_id); }

		index_gid_t GetIndexId() const { return m_index_id; }
		index_gid_t GetChild(UINT idx) const { return index_gid_t(m_childs.Get(idx)); }
		UINT32 GetChildCount() const { return m_childs.GetCount(); }
		OpINT32Vector& GetChildren() { return m_childs; }

	private:
		IndexChilds(const IndexChilds& other);
		IndexChilds& operator=(const IndexChilds& other);

		index_gid_t m_index_id;
		OpINT32Vector m_childs;
	};

	class DeletedMessage : public Link
	{
	public:
		DeletedMessage(message_gid_t p_m2_id) : m2_id(p_m2_id) {}

		message_gid_t	m2_id;
	};

	struct Keyword
	{
		Keyword(unsigned p_id) : index(0), id(p_id) {}

		OpString8	keyword;
		index_gid_t index;
		unsigned	id;
	};

	struct MailCategory
	{
		MailCategory(index_gid_t id) : m_id(id), m_open(FALSE), m_visible(TRUE), m_unread_messages(0), m_index_group(NULL) {}
		~MailCategory() { OP_DELETE(m_index_group); }

		index_gid_t m_id;
		BOOL m_open;
		BOOL m_visible;
		UINT32 m_unread_messages;
		UnionIndexGroup* m_index_group;
	};

	MailCategory* GetCategory(index_gid_t category) const;

	/** Write a category to index.ini when a change has happened
	 *  @param category to write to file
	 *  @param idx - the index in m_categories (if known), otherwise it will search for it
	 *  @return OpStatus::ERR if category not found or OOM
	 */
	OP_STATUS WriteCategory(MailCategory* category, INT32 idx = -1);

	/**Sets the default parent ids on most indexes
	 *  Before the mail panel used index ranges to list items correctly, now it uses the parent id
	 */
	OP_STATUS SetDefaultParentIds();

	/**
	 * Clear and add the default list of categories visible in the mail panel
	 */
	OP_STATUS AddDefaultCategories();

	/** Adds the default label indexes like Important, Todo, etc
	 */
	OP_STATUS AddDefaultLabels();

	/** Makes all indexes in the mail panel visible
	  */
	OP_STATUS SetPanelIndexesVisible();
	
	/** Fixes problems we had in indexer version 11 and updates to the current version
	 */
	OP_STATUS UpgradeFromVersion11();

	// Members.

	PrefsFile *m_prefs;

	IndexContainer					  m_index_container;

	OpAutoINT32HashTable<IndexChilds> m_index_childs;
	OpString8HashTable<Keyword>		  m_keywords;
	OpAutoVector<Keyword>			  m_keywords_by_id;
	OpAutoString8HashTable<OpString8> m_needed_headers;

	OpVector<IndexerListener> m_indexer_listeners;;
	OpVector<CategoryListener> m_category_listeners;

	MessageLoop *m_loop;

	UnionIndexGroup* m_unread_group;
	UnionIndexGroup* m_hidden_group;

	OpVector<Index> m_important_indexes;
	OpVector<IndexGroup> m_pop_indexes; // inbox and sent indexgroups for all pop accounts
	OpVector<IndexGroup> m_folder_indexgroups; // feed and mailing list folders
	OpVector<MailCategory> m_categories; // all top level items in the panel have a category

	int m_index_count;
	int m_next_thread_id;
	int m_next_contact_id;
	int m_next_search_id;
	int m_next_folder_id;
	int m_next_newsgroup_id;
	int m_next_imap_id;
	int m_next_newsfeed_id;
	int m_next_archive_id;
	int m_next_folder_group_id;
	int m_verint;

	message_gid_t m_removing_message;		///< message being removed at this moment
	index_gid_t m_rss_account_index_id;		///< cache index id of the RSS account (if it exists) for fast lookup
	bool m_no_autofilter_update;			///< we don't want autofilters to be updated for the current message we are operating on

	BOOL m_save_requested;					///< we have requested a full save of index.ini, will be done after x seconds delay
	BOOL m_text_lexicon_flush_requested;	///< we have requested a flush of the indexing file
	BOOL m_index_lexicon_flush_requested;	///< we have requested a flush of the full text indexing file

	message_gid_t m_currently_indexing;		///< during first-time indexing, sets the message being indexed.

	SELexicon*		m_text_table;
	StringTable		m_index_table;
	MessageDatabase* m_message_database;

	BOOL m_debug_text_lexicon_enabled;
	BOOL m_debug_contact_list_enabled;
	BOOL m_debug_filters_enabled;
	BOOL m_debug_spam_filter_enabled;

	Index m_search_index;

	//long m_index_time;
};

/**
 * Yet another tokenizer class, tuned for full text indexing of messages
 * and learning filters.
 */
class TokenSet
{
	public:
		TokenSet(const uni_char* tokens);
		~TokenSet() { OP_DELETEA(m_tokens); }
		BOOL InSet(OpStringC& tokens);
		BOOL InSet(uni_char token) { return Search(token) >= 0; }
		INT32 Search(uni_char token) { return Search(token, 0, m_len, FALSE); }
	private:
		INT32 Search(uni_char token, UINT32 start, UINT32 end, BOOL insertion_place);
		uni_char* m_tokens;
		UINT32 m_len;
};

class MessageTokenizer
{
public:
	MessageTokenizer(OpStringC& original_string) : m_original_string(original_string), m_initialized(FALSE) { }
	OP_STATUS GetNextToken(OpString& next_token);
	static BOOL EndOfCJKWord(uni_char current_char, uni_char next_char);
private:
	OpStringC& m_original_string;
	int m_current_pos;
	int m_original_string_len;
	BOOL m_initialized;
	int m_current_block;

	static TokenSet m_spacers;
	static TokenSet m_blocklimits;
	static TokenSet m_remove_from_end_of_block;
};

// ---------------------------------------------------------------------------------

#endif //M2_SUPPORT

#endif // INDEXER_H
