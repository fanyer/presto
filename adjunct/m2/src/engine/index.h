/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ---------------------------------------------------------------------------------

#ifndef INDEX_H
#define INDEX_H

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/m2/src/include/defs.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

class OpFile;
class Index;
class PrefsFile;
class IndexImage;

// ---------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------

/**
 * Keeps information about a Search.
 * The searching itself will typically be done by the Indexer.
 */
class IndexSearch
{
public:
	OpStringC GetSearchText() const { return m_search_text.Get(); }
	OP_STATUS SetSearchText(const OpStringC& text) { m_search_text.Set(text); return OpStatus::OK; }

	/**
	 * The start date should typically default to 3 months ago
	 * when the search is originally created.
	 */
	void SetStartDate(time_t date) { m_start_date = date; }

	UINT32 GetStartDate() const { return m_start_date; }

	/**
	 * The end date defaults to 0 -> unlimited.
	 */
	void SetEndDate(UINT32 date) { m_end_date = date; }
	UINT32 GetEndDate() const { return m_end_date; }

	/**
	 * Is the search text a regular expression?
	 */
	SearchTypes::Option GetOption() const { return (SearchTypes::Option)m_option.Get(); }
	void SetOption(SearchTypes::Option option) { m_option.Set(option); }

	SearchTypes::Operator GetOperator() const { return (SearchTypes::Operator)m_operator.Get(); };
	void SetOperator(SearchTypes::Operator op) { m_operator.Set(op); };

	/**
	 * Returns the current id in header cache that we compare against,
	 * 0 if finished. Permits to resume searching after a pause (by sending
	 * a messages through a MessageHandler so we don't monopolize the CPU.
	 */
	message_gid_t GetCurrentId() const { return m_current_id; }
	void SetCurrentId(message_gid_t id) { m_current_id = id; }

	/**
	 * Choose to search only headers or body
	 */
	SearchTypes::Field SearchBody() const  { return (SearchTypes::Field)m_search_body.Get(); }
	void SetSearchBody(SearchTypes::Field search_body) { m_search_body.Set(search_body); }

	IndexSearch();
	virtual ~IndexSearch() {};

	IndexSearch(const IndexSearch& copy_from) { Copy(copy_from); }
	IndexSearch& operator=(const IndexSearch& copy_from) { Copy(copy_from); return *this; }

	// properties
	OpProperty<OpString>	m_search_text;
	OpProperty<INT32>		m_option;
	OpProperty<INT32>		m_search_body;
	OpProperty<INT32>		m_operator;

private:
	void Copy(const IndexSearch& copy_from);

	UINT32					m_start_date;
	UINT32					m_end_date;
	index_gid_t				m_search_only_in; 
	message_gid_t			m_current_id;
};


/**
 * A Index is simply a list of mail indexes from the Storage.
 * Each Index is stored in a binary file, new messages are
 * appended to the file if they match the associated Search.
 */
class Index : public MessageObject
{
public:
	static const size_t RECENT_MESSAGES = 3; ///< Number of recent messages to remember

    class Observer
	{
		friend class Index;
		virtual OP_STATUS MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword) = 0;
		virtual OP_STATUS MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword) = 0;
		virtual OP_STATUS StatusChanged(Index* index) = 0;
		virtual OP_STATUS NameChanged(Index* index) = 0;
		virtual OP_STATUS VisibleChanged(Index* index) = 0;
		virtual OP_STATUS IconChanged(Index* index) = 0;
    public:
        virtual ~Observer() {}
	};

	OP_STATUS AddObserver(Observer* observer) { return m_observers.Add(observer); }
	OP_STATUS RemoveObserver(Observer* observer) { return m_observers.Remove(observer); }

	Index();
	~Index();

	/**
	 * @param justwrite If TRUE, don't prefetch ('throwaway' index)
	 * @param setting_keyword Whether we are adding the message because we're setting a keyword on a message
	 * @return OpStatus::OK if the index accepts the message
	 */
	OP_STATUS NewMessage(message_gid_t message, bool justwrite = false, bool setting_keyword = false);

	/**
	 * Remove a message from Index. Clears the field in the
	 * file, but file isn't resorted before needed. Sorting
	 * can be needed if a new message_gid_t is added that is
	 * lower than the current id.
	 * @param setting_keyword Whether we are removing the message because we're setting a keyword on a message
	 */
	OP_STATUS RemoveMessage(message_gid_t message, bool setting_keyword = false);

	/**
	 * Returns true if the Index has this message
	 * @param memory_only TRUE will only search in pre-fetched messages
	 */
	BOOL Contains(message_gid_t message) { return m_messages.Contains ( message ); }

	/**
	 * @return an iterator to loop through messages in this index
	 * NB: Caller becomes owner and is responsible for deletion
	 */
	INT32SetIterator GetIterator() { return INT32SetIterator(m_messages); }

	/**
	 * Number of messages in the Index.
	 */
	UINT32 MessageCount() { return m_messages.GetCount(); }

	/**
	 * Number of unread mails in index
	 */
	UINT32 UnreadCount();
	UINT32 ResetUnreadCount() { return m_unread = UINT_MAX; };
	UINT32 CommonCount(index_gid_t index_id);

	/**
	 * Is this message visible in the index?
	 * Returns TRUE if it's not in the index or if it's hidden
	 */
	bool MessageNotVisible ( message_gid_t message_id ) ;
	/**
	 * Is this message visible in the index according to ModelType, ModelAge, ModelFlags?
	 * May return FALSE if the message is not in the index
	 */
	bool MessageHidden(message_gid_t message_id) { return MessageHidden(message_id, (IndexTypes::Id)0); }
	bool MessageHidden(message_gid_t message_id, IndexTypes::Id ignore);

	/**
	 * Filter behaviour
	 */
	bool GetMarkMatchAsRead() { return GetIndexFlag(IndexTypes::INDEX_FLAGS_MARK_MATCH_AS_READ); }
	void SetMarkMatchAsRead(bool mark_match_as_read) {  SetIndexFlag(IndexTypes::INDEX_FLAGS_MARK_MATCH_AS_READ, mark_match_as_read); }

	time_t GetLastUpdateTime() { return m_last_update_time; }
	void SetLastUpdateTime(time_t last_update_time) { m_last_update_time = last_update_time; }

	time_t GetUpdateFrequency() { return m_update_frequency; }
	void SetUpdateFrequency(time_t update_frequency) { m_update_frequency = update_frequency; }

	bool GetUpdateNameManually() { return  m_update_name_manually; }
	void SetUpdateNameManually(bool update_manually) {  m_update_name_manually = update_manually; }

	/**
	 * Get images, used by the UI
	 */
	OP_STATUS GetImages(const char*& image, Image &bitmap_image);

	/** Get the Index Image
	*/
	IndexImage*	GetIndexImage() const { return m_image; }
	
	/** Set a custom IndexImage from a file
	* @param filename - pointing to an image file
	*/
	OP_STATUS	SetCustomImage(const OpStringC& filename);

	/** Set a skin image, used for the accessmodel and mail panel and saved to index.ini
	 * @param image
	 */
	OP_STATUS	SetSkinImage(const OpStringC8& image);

	/** Set a custom IndexImage from a base 64 encoded image
	* @param buffer - the image encoded in base 64
	*/
	OP_STATUS	SetCustomBase64Image(const OpStringC8& buffer);

	/**
	 * Visible in UI, used to not show newly entered Indexes when deleting messages.
	 */
	bool IsVisible() const { return GetIndexFlag(IndexTypes::INDEX_FLAGS_VISIBLE); }
	void SetVisible(bool visible);

	bool IsReadOnly() const { return m_is_readonly; }
	void SetReadOnly(bool readonly);

	void SetType(IndexTypes::Type type) { m_type = type;};
	IndexTypes::Type GetType() { return static_cast<IndexTypes::Type> (m_type); }

	/**
	 * The name is mainly for UI purposes
	 */
	const OpStringC16& GetName() const { return m_name.Get(); }
	OP_STATUS GetName(OpString &name);
	OP_STATUS SetName(const uni_char* name) { m_name.Set(name); return OpStatus::OK; }
	BOOL HasName() const { return m_name.Get().HasContent(); }

	/**
	 * To build up folder tree in UI:
	 */
	index_gid_t GetParentId() { return m_parent_id; };
	void SetParentId(index_gid_t parent_id);

	/**
	 * Contact address if it is a contact index
	 */

	OP_STATUS GetContactAddress(OpString& address);

	/**
	 * Search to match
	 */
	IndexSearch* GetSearch(UINT32 position = 0);
	UINT32		 GetSearchCount();
	OP_STATUS	 RemoveSearch(UINT32 position = 0);

	IndexSearch* GetM2Search(UINT32 position = 0) { return GetSearch(position); }
	IndexSearch* AddM2Search() { IndexSearch search; AddSearch(search); return GetM2Search(GetSearchCount()-1); };

	OP_STATUS	AddSearch(const IndexSearch &search);

	/**
	 * Returns a corresponding account Id for IMAP accounts
	 */
	UINT32 GetAccountId();
	void SetAccountId(INT32 id) { m_account_id = id; }

	/**
	 * Options for how to view an index in the UI
	 */
	void SetThreaded(bool threaded) { SetModelType(threaded? IndexTypes::MODEL_TYPE_THREADED: IndexTypes::MODEL_TYPE_FLAT, TRUE);}
	void SetModelType(IndexTypes::ModelType type, bool user_initiated = TRUE);
	void SetModelAge(IndexTypes::ModelAge age) { m_model_age = age; m_unread = UINT_MAX; }
	void SetModelFlags(INT32 flags) { m_model_flags = flags; m_unread = UINT_MAX; }
	void SetModelFlag(IndexTypes::ModelFlags flag, bool value) { m_model_flags = value ? m_model_flags | 1 << flag : m_model_flags & ~(m_model_flags & (1 << flag)); }
	void SetModelSort(int sort) { m_model_sort = static_cast<IndexTypes::ModelSort>(sort); }
	void SetModelGroup(int grouping) { m_model_grouping = static_cast<IndexTypes::GroupingMethods>(grouping); }
	void SetModelSortAscending(bool ascending) {  SetIndexFlag(IndexTypes::INDEX_FLAGS_ASCENDING, ascending); }
	void SetModelSelectedMessage(message_gid_t selected_message) { m_model_selected_message = selected_message; }
	void SetSpecialUseType(AccountTypes::FolderPathType type) { m_special_use_type = type; m_skin_image.Empty(); }

	void EnableModelFlag( IndexTypes::ModelFlags flag ) { m_model_flags |= (1<<flag); }
	void DisableModelFlag( IndexTypes::ModelFlags flag ) { m_model_flags = m_model_flags & ~(1<<flag); }
	IndexTypes::ModelType GetModelType() { return static_cast<IndexTypes::ModelType> (m_model_type); }
	IndexTypes::ModelAge GetModelAge() { return static_cast<IndexTypes::ModelAge> (m_model_age); }
	INT32 GetModelFlags() { return m_model_flags; }
	IndexTypes::ModelSort GetModelSort() { return static_cast<IndexTypes::ModelSort> (m_model_sort); }
	IndexTypes::GroupingMethods GetModelGrouping() { return static_cast<IndexTypes::GroupingMethods> (m_model_grouping); }
	bool GetModelSortAscending() { return  GetIndexFlag(IndexTypes::INDEX_FLAGS_ASCENDING); }
	message_gid_t GetModelSelectedMessage() { return m_model_selected_message; }
	AccountTypes::FolderPathType GetSpecialUseType() { return static_cast<AccountTypes::FolderPathType>(m_special_use_type); }

	bool GetIncludeSubfolders() { return  m_include_subfolders; };
	void SetIncludeSubfolders(bool subfolders) { m_include_subfolders = subfolders; };

	bool GetHideFromOther() { return GetIndexFlag(IndexTypes::INDEX_FLAGS_HIDE_FROM_OTHER); }
	void SetHideFromOther(bool hide_messages);

	bool GetOverrideDefaultSorting() { return (GetModelFlags() & (1 << IndexTypes::MODEL_FLAG_OVERRIDE_DEFAULTS_FOR_THIS_INDEX)) != FALSE; }
	void SetOverrideDefaultSorting(bool override_sorting) { SetModelFlag(IndexTypes::MODEL_FLAG_OVERRIDE_DEFAULTS_FOR_THIS_INDEX, override_sorting); }

	UINT32 GetAllIndexFlags() { return m_index_flags; }
	void SetAllIndexFlags(UINT32 index_flags) { m_index_flags = index_flags; }

	bool GetIndexFlag(IndexTypes::IndexFlags flag) const { return (m_index_flags & (1 << flag)) != FALSE; }
	void SetIndexFlag(IndexTypes::IndexFlags flag, bool value) { m_index_flags = value ? m_index_flags | 1 << flag : m_index_flags & ~(m_index_flags & (1 << flag));}

	/**
	 * Returns the unique id of the index.
	 * Some special ids are defined in Index::Id
	 *
	 */
	index_gid_t GetId() { return m_id; };

	void SetId(index_gid_t id) { m_id = id; }

	/**
	 * Typically used to clear the Clipboard index, removes all
	 * messages from an index.
	 */
	OP_STATUS Empty();


	/**
	 * Reads into memory for faster comparing of indexes when
	 * needed.
	 */
	OP_STATUS PreFetch();

	/**
	 * Fill the index with information previously stored in a binary tree
	 */
	void      SetPrefetched();

	/**
	 * Insert a message directly into index, only for use by store
	 */
	OP_STATUS InsertDirect(message_gid_t msg_id) { return m_messages.Add(msg_id); }

	/** Prefetch this index after 100 ms. Be careful to not use this on indexes that are used in MessageHidden or expect incorrect unread counts
	  */
	OP_STATUS DelayedPreFetch();

	bool IsPreFetched() { return m_is_prefetched; }

	/**
	 * Wether or not the access point is expanded in the Mail panel
	 */
	bool GetExpanded() { return GetIndexFlag(IndexTypes::INDEX_FLAGS_VIEW_EXPANDED); }
	void SetExpanded(bool view_expanded) { SetIndexFlag(IndexTypes::INDEX_FLAGS_VIEW_EXPANDED, view_expanded); }

	/**
	 * File used to store statistical information about
	 * e-mails contained in the folder
	 */
	void SetHasAutoFilter(bool filter) { filter ? SetAutoFilterFile() : RemoveAutoFilterFile(); }
	bool GetHasAutoFilter() { return GetAutoFilterFile() != NULL; }

	/**
	 * Get the keyword associated with this index
	 */
	const OpStringC8& GetKeyword() const { return m_keyword; }

	/**
	 * Get the default keyword associated with this index
	 * @param keyword Where to place the keyword
	 */
	const char* GetDefaultKeyword() const;

	/**
	 * Get a list of all messages in this index
	 * @param message_ids Where to place the messages
	 */
	OP_STATUS GetAllMessages(OpINT32Vector& message_ids);

	/** Get the number of new messages received in this index
	  * NB: This will reset the counter
	  */
	unsigned GetNewMessagesCount();

	/** Decrease the new messages count by a specified value, use e.g. when copying/moving messages
	  * @param decrease Amount with which count should be decreased
	  */
	OP_STATUS DecreaseNewMessageCount(unsigned decrease);

	/** Get recent messages
	  */
	message_gid_t GetNewMessageByIndex(unsigned index) const { return m_recent_messages[index]; }

	/** @return Whether this is a filter
	  */
	bool IsFilter() const;

	bool IsContact() const;

	/** @return Mirror index (archive) for this index
	  */
	index_gid_t GetMirrorId() const { return m_mirror_id; }

	/** Set a mirror index (archive) for this index
	  */
	void SetMirrorId(index_gid_t mirror_id) { m_mirror_id = mirror_id; }

	/** @return Search index id for this index (used when the actual search is in another index)
	  */
	index_gid_t GetSearchIndexId() const { return m_search_index_id; }

	/** Set a search index id for this index
	  */
	void SetSearchIndexId(index_gid_t search_index_id) { m_search_index_id = search_index_id; }

	
	/** Setup a new index that is to be a child of another index
	  * Will copy properties from parent where appropriate
	  * @param parent_index Index that is to be parent to this index
	  */
	OP_STATUS SetupWithParent(Index* parent_index);

	void SetWatched(bool watched);

	/** @return If this index should be followed
	*/
	bool IsWatched() const { return GetIndexFlag(IndexTypes::INDEX_FLAGS_WATCHED); };
	
	/** Set the index to a "watched index", and therefore also toggle visibility in the panel
	*/
	void ToggleWatched(bool watched);

	/** @return If this index should be ignored
	  */
	bool IsIgnored() const { return GetIndexFlag(IndexTypes::INDEX_FLAGS_IGNORED); };

	void SetIgnored(bool ignored) { SetIndexFlag(IndexTypes::INDEX_FLAGS_IGNORED, ignored); };

	/** @return Whether the searches or filter rules for this index should be executed on existing messages when changed
	  */
	bool ShouldSearchNewMessagesOnly() const { return GetIndexFlag(IndexTypes::INDEX_FLAGS_SERCH_NEW_MESSAGE_ONLY); }

	void SetSearchNewMessagesOnly(bool new_only) { SetIndexFlag(IndexTypes::INDEX_FLAGS_SERCH_NEW_MESSAGE_ONLY, new_only); }
	
	/** Set the index to be an ignored index, an ignored index marks all messages as read
	*/
	void ToggleIgnored(bool ignored) { SetIndexFlag(IndexTypes::INDEX_FLAGS_MARK_MATCH_AS_READ, ignored); SetIndexFlag(IndexTypes::INDEX_FLAGS_IGNORED, ignored); SetIndexFlag(IndexTypes::INDEX_FLAGS_VISIBLE, FALSE); };
	
	bool operator<(const Index& other_index) const { return m_id < other_index.m_id; }

	// From MessageObject:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

public:
	// properties
	OpProperty<OpString> m_name;				///< Typically used in UI, index.ini

private:
	friend class Indexer;
	friend class IndexGroup;
	friend class MessageEngine;

	void      NameChanged(const OpStringC& new_name);
	OP_STATUS IconChanged();
	OP_STATUS MessageAdded(message_gid_t message, BOOL setting_keyword = FALSE);
	OP_STATUS MessageRemoved(message_gid_t message, BOOL setting_keyword = FALSE);
	OP_STATUS StatusChanged();
	time_t    GetTimeByAge(IndexTypes::ModelAge age) const;

	//

	void IncUnread() { if (m_unread != UINT_MAX) { m_unread++; } }
	void DecUnread() { if (m_unread != UINT_MAX && m_unread != 0) { m_unread--; } }

	OP_STATUS SetKeyword(const OpStringC8& keyword) { return m_keyword.Set(keyword); }

	OpListeners<Observer> m_observers;

	//

    Index(const Index&);
    Index& operator =(const Index&);

	/**
	 * Get a unique name for this index (for saving in database)
	 */
	OP_STATUS GetUniqueName(OpString& name);

	/**
	  * Set whether this index should be saved to disk
	  */
	void      SetSaveToDisk(bool save_to_disk) { m_save_to_disk = save_to_disk; m_is_prefetched = FALSE; }
	bool	  GetSaveToDisk() const { return  m_save_to_disk; }

	bool	  GetSaveToIndexIni() const { return (GetSaveToDisk() || m_type == IndexTypes::UNIONGROUP_INDEX || m_type == IndexTypes::INTERSECTION_INDEX || m_id < IndexTypes::LAST_IMPORTANT); }

	OP_STATUS SetAutoFilterFile();
	PrefsFile* GetAutoFilterFile() { return m_auto_filter_file; }

	OP_STATUS RemoveAutoFilterFile();

	/**
	 * Write messages to disk at current file position or Sort() to allocate more room
	 */
	OP_STATUS WriteData(UINT32 data);

	PrefsFile* m_auto_filter_file;

	OpVector<IndexSearch> m_searches;	///< For search results indexes etc.
	OpString m_path;				///< Path in folder tree: "parent/inbox"
	index_gid_t m_id;
	index_gid_t m_parent_id;
	index_gid_t m_mirror_id;
	index_gid_t m_search_index_id;
	UINT32 m_account_id;

	//

	OpString8 m_keyword;			///< Have a special keyword assigned to the index, used for IMAP

	OpINT32Set m_messages;   ///< List of messages in this index
	UINT32 m_unread;
	int m_new_messages;
	message_gid_t m_recent_messages[RECENT_MESSAGES];

	UINT32 m_model_flags;
	message_gid_t m_model_selected_message;

	IndexImage* m_image;
	OpString8	m_skin_image;

	time_t m_accessed;
	time_t m_update_frequency;		///< How often to reload a RSS feed or similar
	time_t m_last_update_time;		///< Last time a RSS feed was reloaded	

	UINT32 m_index_flags;

	// have to use unsigned bitfields because: http://connect.microsoft.com/VisualStudio/feedback/details/100849/enum-bitfield-treated-incorrectly-as-integer
	unsigned m_type:4;				/* IndexTypes::Type */				
	unsigned m_model_type:3;		/* IndexTypes::ModelType */	
	unsigned m_model_age:3;			/* IndexTypes::ModelAge */
	unsigned m_special_use_type:3;	/* AccountTypes::FolderPathType */
	IndexTypes::ModelSort m_model_sort;
	IndexTypes::GroupingMethods m_model_grouping;	

	bool m_is_readonly:1; 
	bool m_is_prefetched:1;				///< Whether the index has fetch the message list from the indexer string table
	bool m_delayed_prefetch:1;
	bool m_include_subfolders:1;
	bool m_update_name_manually:1;
	bool m_save_to_disk:1;				///< Whether to save this index to disk (to the indexer string table and index.ini)

};


// ---------------------------------------------------------------------------------

#endif //M2_SUPPORT

#endif // INDEX_H
