/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas
 */

#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryModel.h"
#include "modules/history/src/DelayedSave.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/opvector.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/history/src/OpSplayTree.h"
#include "modules/history/history_enums.h"
#include "modules/history/history_structs.h"

class HistoryKey;
class HistoryItem;
class HistoryPage;
class HistoryFolder;
class HistoryTimeFolder;
class HistoryPrefixFolder;
class PrefixMatchNode;
class HistorySiteFolder;
class HistorySiteSubFolder;

/**
 * @brief The model of the history of urls visited
 * @author Patricia Aas (psmaas)
 */
class HistoryModel :
	public OpHistoryModel,
	public DelayedSave
{
	friend class HistoryModule;

public:

	/**
	 *	Add listener - currently only supporting one listener
	 *
	 *	@param listener        - listener to be added
	 *	@return OpStatus::OK if the add was successful
	 */
	OP_STATUS AddListener(OpHistoryModel::Listener* listener) { m_listener = listener; return OpStatus::OK; }

	/**
	 *	Remove listener
	 *
	 *	@param listener        - listener to be removed
	 *		@return OpStatus::OK if the removal was successful
	 */
	OP_STATUS RemoveListener(OpHistoryModel::Listener* listener) { if(listener == m_listener) m_listener = 0; return OpStatus::OK; }

    /**
	 *	Deletes the model and all the items and trees
	 */
    virtual ~HistoryModel();

    /**
	 *	Get number of items - does not include folders
	 *
	 *	@return number of items in the vector
	 */
	INT32 GetCount(){ return m_modelVector->GetCount(); }

	/**
	 *	Get item at this position in the vector
	 *
	 *	@param index           - the index of the item to be removed
	 *	@return item at position index in the vector - if no such item will return 0
	 */
	HistoryPage* GetItemAtPosition(UINT32 index);

	/**
	 *	Returns the folder corresponding to this period
	 *
	 *	@param period         - the period indicating the folder
	 *	@return the time folder for the period
	 */
	HistoryTimeFolder* GetTimeFolder(TimePeriod period){ return m_timeFolderArray[period]; }

	/**
	 *
	 *
	 *	@param
	 *	@return
	 */
	OP_STATUS GetHistoryItems(OpVector<HistoryPage> &vector);

	/**
	 *
	 *
	 *	@param
	 *	@return
	 */
	OP_STATUS GetBookmarkItems(OpVector<HistoryPage> &vector);

	/**
	 *
	 *
	 *	@param
	 *	@return
	 */
	OP_STATUS GetInternalPages(OpVector<HistoryPage> &vector);

	/**
	 * Finds an item based on the url.
	 *
	 * @param url_name         - address to be searched for
	 * @param history_item     - reference to pointer to item - if found
	 *
	 * @return OpStatus:OK if item was found
	 */
	OP_STATUS GetItem(const OpStringC & address,
					  HistoryPage *& history_item);


	/**
	 * Determines if a url should be marked as visited
	 *
	 * @param url_name - address to be checked
	 *
	 * @return TRUE if it should be marked as visited
	 */
	BOOL IsVisited(const OpStringC& url_name);

	/**
	 * Set that the url was typed if it exists in history
	 * @param url to be marked
	 */
	void SetIsTyped(const OpStringC & url);

	/**
	 * Set that the url was not typed if it exists in history (this is the default anyway)
	 * @param url to be marked
	 */
	void ClearIsTyped(const OpStringC & url);


	/**
	 *	Adds an item to history - all listeners will be notified with a OnItemAdded
	 *
	 *	@param url_name         - URL to be inserted
	 *	@param title            - title of page
	 *	@param acc              - last accessed
	 *	@param save_session     - whether session should be saved
	 *	@param average_interval - average interval
	 *
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 *	Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *	      TRUE if this url can be inserted into history
	 *
	 *	If Add is called on an existing item the title will be changed to the title
	 *	supplied.
	 */
	OP_STATUS Add(const OpStringC& url_name,
				  const OpStringC& title,
				  time_t acc)
		{ return AddPage(url_name, title, acc); }

	OP_STATUS Add(const OpStringC8& address,
				  const OpStringC8& title,
				  time_t acc)
		{
			OpString address_str;
			RETURN_IF_ERROR(address_str.SetFromUTF8(address.CStr()));
			OpString title_str;
			RETURN_IF_ERROR(title_str.SetFromUTF8(title.CStr()));
			return Add(address_str, title_str, acc);
		}

	/**
	 * @param url_name - url of bookmark to be inserted
	 * @param title    - title or nick of bookmark to be inserted
	 * @param out_item - the history item of the added bookmark
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 * Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *       TRUE if this url can be inserted into history
	 *
	 * If AddBookmark is called on an existing item the title will be changed to the title
	 * supplied.
	 */
	 OP_STATUS AddBookmark(const OpStringC& url_name,
						   const OpStringC& title,
						   HistoryPage *& out_item);

	/**
	 * @param url_name - url of bookmark to be removed
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 * Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *       TRUE if this url can be inserted into history
	*/
	OP_STATUS RemoveBookmark(const OpStringC& url_name);

	/**
	 * Add an assosiation between a nick and a bookmark_item
	 *
	 * @param nick to be added
	 * @param bookmark_item it should be associated with
	 *
	 * @return OpStatus::OK if association was established
	 */
	OP_STATUS AddBookmarkNick(const OpStringC & nick, HistoryPage & bookmark_item);

	/**
	 * Gets the bookmark associated with the given nick if it exists
	 *
	 * @param nick to search for
	 * @param bookmark_item pointer to be set to the bookmark
	 *
	 * @return OpStatus::OK if the bookmark was found
	 */
	OP_STATUS GetBookmarkByNick(const OpStringC & nick, HistoryPage *& bookmark_item);

	/**
	 *	Deletes an item form history - all listeners will be notified with a OnItemRemoving
	 *
	 *	@param item            - item to be deleted
	 *	@param force           - if TRUE item will be deleted even if it is pinned
	 */
	void Delete(HistoryItem* item, BOOL force = FALSE);

    /**
	 *	Deletes all items in history
	 *
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    OP_STATUS DeleteAllItems();

	/**
	 *	@return address of first item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    OP_STATUS GetFirst(OpString & address);

	/**
	 *	@return address of next item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    OP_STATUS GetNext(OpString & address);

	/**
	 *	@return the title of the current item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    OP_STATUS GetTitle(OpString & title);

	/**
	 *	@return the accessed field of the current item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    time_t GetAccessed();

    /**
	 * Saves history to the global.dat file
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    OP_STATUS Save(BOOL force = FALSE) { return RequestSave(force); }

    /**
	 * Enable the save timer (on by default)
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	OP_STATUS EnableSaveTimer() { return EnableTimer(); }

    /**
	 * Disable the save timer (on by default) - no unforced saves will be performed
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	OP_STATUS DisableSaveTimer() { return DisableTimer(); }

	/**
	 *	Reads the history items from the history file global.dat
	 *
	 *	@param ifp             - the file to read from
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    OP_STATUS Read(OpFile *ifp);

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
	/**
	 *  Read the old vlink4.dat file to make sure visited links works on upgrade
	 */
	OP_STATUS ReadVlink();
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

    /**
	 *	Resize history to new size
	 *	@param m - the new maxsize
	 */
    void Size(INT32 m);

	/**
	 *	Returns an array consisting of all items stored matching the prefix in_MatchString.
	 *
	 *	@param in_MatchString  - the prefix that is to be searched for
	 *	@param out_vector      - the vector to fill with the matches
	 *	@param sorted          - specify the sorted type (defaults to BY_TIME)
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS GetItems(const OpStringC& in_MatchString,
							   OpVector<HistoryPage>& out_vector,
							   int max_number = HISTORY_DEFAULT_MAX_GET_ITEMS,
							   SortType sorted = BY_TIME);

	/**
	 *	Returns an array consisting of all items stored matching the prefix in_MatchString
	 *	The array of has size number of addresses * 3.
	 *
	 *	out_Items[i*3]     : The address
	 *	out_Items[i*3 + 1] : The title
	 *	out_Items[i*3 + 2] : Depricated
	 *
	 *	@param in_MatchString  - the prefix that is to be searched for
	 *	@param out_Items       - pointer to pointer that will be set to array of matches
	 *	@param out_ItemCount   - pointer to int that will be set to number of matches
	 *	@param sorted          - specify the sorted type (defaults to BY_TIME)
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 *	@deprecated Provided for backwards compatibility - use GetItems above instead.
	 */
    virtual OP_STATUS GetItems(const OpStringC& 	in_MatchString,
							   uni_char *** 	out_Items,
							   int* 			out_ItemCount,
							   SortType 		sorted = BY_TIME);

	/**
	 * Returns TRUE if this url has an acceptable form. That is if it will be inserted into
	 * history. This method is supplied to protect calls to Add/AddBookmark as these will
	 * produce an assertion if given an unacceptable url.
	 *
	 * @param url_name url to be tested
	 * @return returns TRUE if the url can be inserted into history
	 */
	BOOL IsAcceptable(const OpStringC& url_name);

	OP_STATUS MakeUnescaped(OpString& address);

	/**
	 * Returns the number of seconds since last midnight
	 */
	time_t SecondsSinceMidnight();

	/**
	 *
	 *
	 * @param url
	 * @param stripped
	 *
	 * @return
	 */
	BOOL IsLocalFileURL(const OpStringC & url, OpString & stripped);

private:

	// Item types
	// -----------------------------------
    enum HistoryItemType
	{
		HISTORY_ITEM_TYPE  = 0,
		BOOKMARK_ITEM_TYPE = 1,
		OPERA_PAGE_TYPE    = 2,
		NICK_ITEM_TYPE     = 3
    };


	/**
	 *	Create instance of History Model.
	 *
	 *	@param num             - the number of items allowed in history
	 *	@return  g_globalHistory or 0 if the model could not be created
	 */
    static HistoryModel* Create(INT32 num);

	static HistoryModel* _instance;

	/**
		Returns pointer to Core History Model instance - note: this function does not
		create an instance

		@return g_globalHistory;
	*/
	static OpHistoryModel* Instance(){ return globalHistory; }

    // Non-static Fields:
	// -----------------------------------
	OpHistoryModel::Listener* m_listener;
    UINT32    m_CurrentIndex; //Used by opera:history
    UINT32	  m_max;
	BOOL      m_model_alive;

    OpVector<HistoryPage>*    m_modelVector;
    OpVector<HistoryPage>*    m_bookmarkVector;
	OpVector<HistoryPage>*    m_operaPageVector;
	OpVector<HistoryPage>*    m_nickPageVector;
    HistoryTimeFolder    **   m_timeFolderArray;
	HistoryPrefixFolder  **   m_protocol_array;
    OpSplayTree<HistoryItem, HistoryPage>* m_strippedTree;
    OpSimpleSplayTree<PrefixMatchNode> * m_prefix_tree;

	// Timer fields:
	// -----------------------------------
	BOOL      m_midnight_timer_on;
	OpTimer * m_midnight_timer;

	// Tables:
	// -----------------------------------
	HistoryTimePeriod   * m_history_time_periods;
	HistoryPrefix       * m_history_prefixes;
	HistoryInternalPage * m_history_internal_pages;
	const int             m_size_history_time_periods;
	const int             m_size_history_prefixes;
	const int             m_size_history_internal_pages;

	// -----------------------------------
	// Listener management
	// -----------------------------------
	void BroadcastItemAdded(HistoryPage* item, BOOL save_session)
		{if(m_listener) m_listener->OnItemAdded(item, save_session);}

	void BroadcastItemRemoving(HistoryItem* item)
		{if(m_listener) m_listener->OnItemRemoving(item);}

	void BroadcastItemDeleting(HistoryItem* item)
		{if(m_listener) m_listener->OnItemDeleting(item);}

	void BroadcastItemMoving(HistoryItem* item)
		{if(m_listener) m_listener->OnItemMoving(item);}

	void BroadcastModelChanging()
		{if(m_listener) m_listener->OnModelChanging();}

	void BroadcastModelChanged()
		{if(m_listener) m_listener->OnModelChanged();}

	void BroadcastModelDeleted()
		{if(m_listener) m_listener->OnModelDeleted();}

	BOOL HasListener()
		{return m_listener != 0;}

	// -----------------------------------
	// Private constructor: Use the public static Create method
	// -----------------------------------
    HistoryModel(INT32 m,
				 OpVector<HistoryPage> *  modelVector,
				 OpVector<HistoryPage> *  bookmarkVector,
				 OpVector<HistoryPage> *  operaPageVector,
				 OpVector<HistoryPage> *  nickPageVector,
				 HistoryTimeFolder     ** timeFolderArray,
				 HistoryPrefixFolder   ** protocol_array,
				 OpSplayTree<HistoryItem, HistoryPage> * strippedTree,
				 OpSimpleSplayTree<PrefixMatchNode> * prefix_tree,
				 OpTimer               *  midnight_timer,
				 HistoryTimePeriod     *  history_time_periods,
				 HistoryPrefix         *  history_prefixes,
				 HistoryInternalPage   *  history_internal_pages,
				 int                      size_history_time_periods,
				 int                      size_history_prefixes,
				 int                      size_history_internal_pages);

	// -----------------------------------
	// Static initialisation methods used for allocating to the sigleton history model :
	// -----------------------------------

	/**
	   Set up the time folder array

	   @return OpStatus::ERR_NO_MEMORY if any of the allocations fail
	 */
	static OP_STATUS initFolderArray(HistoryTimeFolder *** time_folder_array,
									 HistoryTimePeriod * history_time_periods);

	/**
	   Set up the prefix array - including setting up the trees

	   @return OpStatus::ERR_NO_MEMORY if any of the allocations fail
	 */
	static OP_STATUS initPrefixes(OpSimpleSplayTree<PrefixMatchNode> ** prefix_tree,
								  HistoryPrefixFolder *** protocol_array,
								  HistoryPrefix * history_prefixes,
								  int size_history_prefixes);

	static OP_STATUS initPrefix(OpSimpleSplayTree<PrefixMatchNode> *& prefix_tree,
								HistoryPrefixFolder **& protocol_array,
								INT * top_level_protocols,
								INT index,
								HistoryPrefix * history_prefixes);

	/**
	   Allocate some of the memory needed in initPrefixArray - if an
	   allocation fails it deletes all the allocated memory.

	   @return OpStatus::ERR_NO_MEMORY if any of the allocations fail
	 */
	static OP_STATUS AllocatePrefixMemory(OpSimpleSplayTree<PrefixMatchNode> *& prefix_tree,
										  HistoryPrefixFolder **& protocol_array,
										  int size_history_prefixes);

	// -----------------------------------
	// Externally defined methods - setting up lookup-tables for history
	// -----------------------------------

	// MakeHistoryTimePeriods : modules/history/history_time_periods.h
	static OP_STATUS MakeHistoryTimePeriods(HistoryTimePeriod*& history_time_periods, int & size);

	// MakeHistoryPrefixes : modules/history/history_prefixes.h
	static OP_STATUS MakeHistoryPrefixes(HistoryPrefix*& history_prefixes, int & size);

	// MakeHistoryInternalPages : modules/history/history_internal_pages.h
	static OP_STATUS MakeHistoryInternalPages(HistoryInternalPage *& history_internal_pages, int & size);

	// -----------------------------------
	// Deletes all the memory referred to by the parameters
	// -----------------------------------

	static void CleanUp(OpSimpleSplayTree<PrefixMatchNode> *& loc_prefix_tree,
						HistoryPrefixFolder **& protocol_array,
						int size_history_prefixes);

	static void CleanUpMore(OpTimer *& midnight_timer,
							OpVector<HistoryPage>    *&  modelVector,
							OpVector<HistoryPage>    *&  bookmarkVector,
							OpVector<HistoryPage>    *&  operaPageVector,
							OpVector<HistoryPage>    *&  nickPageVector,
							HistoryTimeFolder       **& timeFolderArray,
							OpSplayTree<HistoryItem, HistoryPage> *& stripped_tree);

	static void CleanTimeFolders(HistoryTimeFolder **& timeFolderArray);

	static void CleanKey(HistoryKey * key);

	static void CleanTables(HistoryTimePeriod   *& history_time_periods,
							HistoryPrefix       *& history_prefixes,
							HistoryInternalPage *& history_internal_pages);

	// -----------------------------------
	// Member initialisation methods:
	// -----------------------------------

	OP_STATUS InitOperaPages();

	// -----------------------------------
	// Adding itmes:
	// -----------------------------------

#ifdef SELFTEST
public:
#endif // SELFTEST
	OP_STATUS AddPage(const OpStringC& url_name,
					  const OpStringC& title,
					  time_t acc,
					  BOOL save_session = TRUE,
					  time_t average_interval = -1);
#ifdef SELFTEST
private:
#endif // SELFTEST


	OP_STATUS AddHistoryItem(const OpStringC& url_name,
							 const OpStringC& title,
							 HistoryItemType type,
							 time_t acc = 0,
							 BOOL save_session = TRUE,
							 time_t average_interval = -1,
							 HistoryPage ** out_item = NULL,
							 BOOL prefer_exisiting_title = FALSE);

	HistorySiteFolder* AddServer(const OpStringC& url_name);

	OP_STATUS AddItem(HistoryPage* item,
					  HistorySiteFolder* site,
					  BOOL save_session,
					  HistoryItemType type);

	OP_STATUS AddItemSorted(HistoryPage* page);

	OP_STATUS AddOperaItem(const OpStringC& url_name,
						   const OpStringC& title);

	// -----------------------------------
	// Removing/Moving/Deleting itmes:
	// -----------------------------------

    void DeleteItem(HistoryPage* page, BOOL deleteParent = TRUE, BOOL literal_key = FALSE);
	BOOL RemoveItem(HistoryPage* item, BOOL literal_key = FALSE);
	void MoveItem(HistoryPage* item);

	BOOL RemoveBookmark(HistoryPage * bookmark_item);
	BOOL RemoveOperaPage(HistoryPage * opera_item);
	BOOL RemoveNick(HistoryPage * nick_item);

	void DeleteFolder(HistoryFolder* folder);
	BOOL RemoveFolder(HistoryFolder* item);
	void EmptyFolder(HistoryFolder* folder);
	void EmptySiteFolder(HistorySiteFolder* folder);
	void EmptySiteSubFolder(HistorySiteSubFolder* folder);
	void EmptyTimeFolder(HistoryTimeFolder* folder);
	void EmptyPrefixFolder(HistoryPrefixFolder* folder);

	// -----------------------------------
	// Find itmes:
	// -----------------------------------

	OP_STATUS FindInPrefixTrees(const OpStringC& in_MatchString,
								OpVector<HistoryItem> &pvector,
								OpVector<HistoryPage> &vector);

	OP_STATUS GetItemsVector(const OpStringC& in_MatchString, OpVector<HistoryPage>& out_vector);

	OP_STATUS CreateItemsArray(OpVector<HistoryPage>& in_vector, uni_char *** out_Items, int* out_ItemCount);

	INT32 BinaryTimeSearchPeriod(TimePeriod period);
	INT32 BinaryTimeSearch(time_t search_time);

	TimePeriod GetTimePeriod(time_t accessed);

	OP_STATUS GetItem(const OpStringC & address,
					  HistoryPage *& history_item,
					  BOOL literal_key);

	// -----------------------------------
	// Sorting functions:
	// -----------------------------------

	static BOOL PopularityGT(const HistoryPage& item1, const HistoryPage& item2);
	static BOOL AccessTimeGT(const HistoryPage& item1, const HistoryPage& item2);
	static BOOL AdjustedTimeGT(const HistoryPage& item1, const HistoryPage& item2);
	
	void SplitItems(OpVector<HistoryPage>& out_vector, int& split);
	void CutOffItems(OpVector<HistoryPage>& out_vector, int& split, unsigned int max_items);

	INT32 StripPrefix(const OpStringC& address, OpString& stripped, OP_STATUS& status);
	INT32 GetPrefix(const OpStringC& address, UINT32& prefix_len, OP_STATUS& status);

	OP_STATUS GetServerName(const OpStringC& address, OpString & stripped, bool key);
	OP_STATUS GetServerAddress(const OpStringC& address, OpString & stripped);

	// -----------------------------------
	// Debug functions:
	// -----------------------------------

#ifdef HISTORY_DEBUG
	void LogStats();
	void LogStatsWhenDeleted();
	int GetTotalPagesSize();
	int GetTotalKeySize();
	int GetTotalSiteFolderSize();
#endif // HISTORY_DEBUG

	// -----------------------------------
	// Implementing the DelayedSave interface
	virtual PrefsCollectionFiles::filepref GetFilePref() { return PrefsCollectionFiles::GlobalHistoryFile; }
    virtual OP_STATUS Write(OpFile *ofp);
	// -----------------------------------


	/**
	   Set the midnight timer. Each midnight the history will have to be moved so
	   that it corresponds to the correct folders.
	 */
	void SetMidnightTimeout();

	/**
	   Opens a file, but does not check if it is IsOpen
	   Can therefore be used safely inside of a RETURN_IF_ERROR and
	   later used to open a backup file.
	*/
	OP_STATUS OpenFile(OpFile *&fp,
					   const OpStringC &name,
					   OpFileFolder folder,
					   OpFileOpenMode mode);

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
	/**
	   Will open either vlink4.dat or vlink4.old in that order to support upgrades
	   from earlier versions not using history for visited links
	 */
	OP_STATUS OpenVlinkFile(OpFile *& fp,
							OpFileFolder folder);
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

	/**
	   Returns an array consisting of all items stored matching the prefix in_MatchString
	   sorted by popularity - most popular first

	   @param in_MatchString  - the prefix that is to be searched for
	   @param out_vector      - the matching items sorted by popularity
	   @param max_items       - Maximum number of items returned in out_vector
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	*/
	virtual OP_STATUS GetItemsByPopularity(const OpStringC& in_MatchString,
										   OpVector<HistoryPage>& out_vector,
										   int max_number);


	/**
	   Returns an array consisting of all items stored matching the prefix in_MatchString
	   sorted alphabetically

	   @param in_MatchString  - the prefix that is to be searched for
	   @param out_vector      - the matching items sorted alphabetically
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	*/
	virtual OP_STATUS GetItemsAlphabetically (const OpStringC& in_MatchString,
											  OpVector<HistoryPage>& out_vector,
											  int max_number);

	/**
	   Returns an array consisting of all items stored matching the prefix in_MatchString
	   sorted by the time it was accessed last

	   @param in_MatchString  - the prefix that is to be searched for
	   @param out_vector      - the matching items sorted by last accessed time
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	*/
	virtual OP_STATUS GetItemsByTime(const OpStringC& in_MatchString,
									 OpVector<HistoryPage>& out_vector,
									 int max_number);

	/**
	 * This timeout function is activated when the data in the list
	 * should be saved
	 *
	 * @param timer The timer that triggered the timeout
	 */
	virtual void OnTimeOut(OpTimer* timer);
};

#endif // HISTORY_SUPPORT
#endif // HISTORY_MODEL_H
