/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas
 */

#ifndef OP_HISTORY_MODEL_H
#define OP_HISTORY_MODEL_H

#ifdef HISTORY_SUPPORT

#include "modules/util/opfile/opfile.h"
#include "modules/history/history_enums.h"

#include "modules/util/adt/opvector.h"

class HistoryKey;
class HistoryItem;
class HistoryPage;
class HistoryFolder;
class HistoryTimeFolder;
class HistoryPrefixFolder;
class PrefixMatchNode;
class HistorySiteFolder;
class HistorySiteSubFolder;

#define HISTORY_DEFAULT_MAX_GET_ITEMS 200

/**
 * @brief The model of the history of urls visited
 * @author Patricia Aas (psmaas)
 */
class OpHistoryModel
{
	friend class HistoryModule;

public:

	//---------------------------------------------------------------------------
	// Listener interface:
	//---------------------------------------------------------------------------
	class Listener
	{
	public:

		virtual ~Listener() {}

		/**
		 *  Sent after item is added
		 *
		 *  @param item that is being inserted
		 *  @param save_session TRUE if the session should be saved
		 */
		virtual	void OnItemAdded(HistoryPage* item,
								 BOOL save_session) = 0;

		/**
		 *	Sent before item is removed from history, the object may
		 *	still persist if it is marked as a bookmark for example.
		 *
		 *	@param item that is being removed
		 */
		virtual	void OnItemRemoving(HistoryItem* item) = 0;

		/**
		 *	Sent before item is deleted
		 *
		 *	@param item that is being removed
		 */
		virtual	void OnItemDeleting(HistoryItem* item) = 0;

		/**
		 *	Sent before item is moved - note that item is not being deleted and an
		 *	OnItemAdded will occur soon
		 *
		 *	@param item that is being moved
		 */
		virtual	void OnItemMoving(HistoryItem* item) = 0;

		/**
		 *	Sent when whole model is changing, and listener should prepare to rebuild
		 *	from scratch.
		 */
		virtual void OnModelChanging() = 0;

		/**
		 *	Sent when whole model is changed, and listener should rebuild from scratch.
		 */
		virtual void OnModelChanged() = 0;

		/**
		 *	Sent when whole model is being deleted
		 */
		virtual void OnModelDeleted() = 0;
	};

	/**
	 *	Adds a listener - currently only supporting one listener
	 *
	 *	@param listener to be added
	 *	@return OpStatus::OK if the add was successful
	 */
	virtual OP_STATUS AddListener(Listener* listener) = 0;

	/**
	 *	Removes a listener
	 *
	 *	@param listener to be removed
	 *	@return OpStatus::OK if the removal was successful
	 */
	virtual OP_STATUS RemoveListener(Listener* listener) = 0;

    /**
	 *	Deletes the model
	 */
    virtual ~OpHistoryModel() {}

    /**
	 *	Get number of items - does not include folders
	 *
	 *	@return number of history items in the model
	 */
	virtual INT32 GetCount() = 0;

	/**
	 *	Get item at this position in the vector
	 *
	 *	@param index of the item to be removed
	 *	@return item at position index in the vector - if no such item will return 0
	 */
	virtual HistoryPage* GetItemAtPosition(UINT32 index) = 0;

	/**
	 *	Returns the folder corresponding to this period
	 *
	 *	@param period indicating the folder
	 *	@return the time folder for the period
	 */
	virtual HistoryTimeFolder* GetTimeFolder(TimePeriod period) = 0;

	/**
	 *  Fills the vector with all the history items currently in
	 *  the model sorting they alphabetically.
	 *
	 *	@param vector to be filled
	 *	@return OpStatus:OK if successful
	 */
	virtual OP_STATUS GetHistoryItems(OpVector<HistoryPage>& vector) = 0;

	/**
	 *  Fills the vector with all the bookmark items currently in
	 *  the model sorting they alphabetically.
	 *
	 *	@param vector to be filled
	 *	@return OpStatus:OK if successful
	 */
	virtual OP_STATUS GetBookmarkItems(OpVector<HistoryPage>& vector) = 0;

	/**
	 *  Fills the vector with all the internal page items
	 *
	 *	@param vector to be filled
	 *	@return OpStatus:OK if successful
	 */
	virtual OP_STATUS GetInternalPages(OpVector<HistoryPage>& vector) = 0;

	/**
	 * Finds an item based on the address.
	 *
	 * @param address to be searched for
	 * @param history_item that represents that address
	 *
	 * @return OpStatus:OK if item was found
	 */
	virtual OP_STATUS GetItem(const OpStringC& address,
							  HistoryPage*& history_item) = 0;


	/**
	 * Determines if a address should be marked as visited
	 *
	 * @param address to be checked
	 *
	 * @return TRUE if it should be marked as visited
	 */
	virtual BOOL IsVisited(const OpStringC& address) = 0;

	/**
	 *	Adds an item to history - all listeners will be notified with a OnItemAdded
	 *
	 *	@param address - ADDRESS to be inserted
	 *	@param title   - title of page
	 *	@param acc     - last accessed
	 *
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 *	Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *	      TRUE if this address can be inserted into history
	 *
	 *	If Add is called on an existing item the title will be changed to the title
	 *	supplied.
	 */
	 virtual OP_STATUS Add(const OpStringC& address,
						   const OpStringC& title,
						   time_t acc) = 0;

	 virtual OP_STATUS Add(const OpStringC8& address,
						   const OpStringC8& title,
						   time_t acc) = 0;

	/**
	 * @param address - address of bookmark to be inserted
	 * @param title    - title or nick of bookmark to be inserted
	 * @param out_item - the history item of the added bookmark
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 * Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *       TRUE if this address can be inserted into history
	 *
	 * If AddBookmark is called on an existing item the title will be changed to the title
	 * supplied.
	 */
	 virtual OP_STATUS AddBookmark(const OpStringC& address,
								   const OpStringC& title,
								   HistoryPage*& out_item) = 0;

	/**
	 * @param address - address of bookmark to be removed
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 * Note: Please use IsAcceptable to protect this call - IsAcceptable will return
	 *       TRUE if this address can be inserted into history
	*/
	virtual OP_STATUS RemoveBookmark(const OpStringC& address) = 0;

	/**
	 * Add an assosiation between a nick and a bookmark_item
	 *
	 * @param nick to be added
	 * @param bookmark_item it should be associated with
	 *
	 * @return OpStatus::OK if association was established
	 */
	virtual OP_STATUS AddBookmarkNick(const OpStringC& nick,
									  HistoryPage& bookmark_item) = 0;

	/**
	 * Gets the bookmark associated with the given nick if it exists
	 *
	 * @param nick to search for
	 * @param bookmark_item pointer to be set to the bookmark
	 *
	 * @return OpStatus::OK if the bookmark was found
	 */
	virtual OP_STATUS GetBookmarkByNick(const OpStringC& nick,
										HistoryPage*& bookmark_item) = 0;

	/**
	 *	Deletes an item form history - all listeners will be notified with a OnItemRemoving
	 *
	 *	@param item            - item to be deleted
	 *	@param force           - if TRUE item will be deleted even if it is pinned
	 */
	virtual void Delete(HistoryItem* item,
						BOOL force = FALSE) = 0;

    /**
	 *	Deletes all items in history
	 *
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS DeleteAllItems() = 0;

	/**
	 *  Iteration API
	 *
	 *	@return address of first item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    virtual OP_STATUS GetFirst(OpString& address) = 0;

	/**
	 *  Iteration API
	 *
	 *	@return address of next item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    virtual OP_STATUS GetNext(OpString& address) = 0;

	/**
	 *  Iteration API
	 *
	 *	@return the title of the current item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    virtual OP_STATUS GetTitle(OpString& title) = 0;

	/**
	 *  Iteration API
	 *
	 *	@return the accessed field of the current item in history
	 *
	 *	@deprecated Provided for backwards compatibility
	 */
    virtual time_t GetAccessed() = 0;

    /**
	 * Saves history to the global.dat file
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS Save(BOOL force = FALSE) = 0;

    /**
	 * Enable the save timer (on by default)
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	virtual OP_STATUS EnableSaveTimer() = 0;

    /**
	 * Disable the save timer (on by default) - no unforced saves will be performed
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	virtual OP_STATUS DisableSaveTimer() = 0;

	/**
	 *	Reads the history items from the history file global.dat
	 *
	 *	@param ifp             - the file to read from
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS Read(OpFile* ifp) = 0;

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
	/**
	 *  Read the old vlink4.dat file to make sure visited links works on upgrade
	 */
	virtual OP_STATUS ReadVlink() = 0;
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

    /**
	 *	Resize history to new size
	 *	@param m - the new maxsize
	 */
    virtual void Size(INT32 m) = 0;

	// Sorting for GetItems
	// -----------------------------------
    enum SortType
	{
		ALPHABETICALLY = 0,
		BY_POPULARITY = 1,
		BY_TIME = 2
    };

	/**
	 *	Returns an array consisting of all items stored matching the prefix prefix.
	 *
	 *	@param prefix that is to be searched for
	 *	@param out_vector to fill with the matches
	 *	@param sort_type - specify the sorted type (defaults to BY_TIME)
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS GetItems(const OpStringC& prefix,
							   OpVector<HistoryPage>& out_vector,
							   int max_number = HISTORY_DEFAULT_MAX_GET_ITEMS,
							   SortType sort_type = BY_TIME) = 0;

	/**
	 *	Returns an array consisting of all items stored matching the prefix prefix
	 *	The array of has size number of addresses * 3.
	 *
	 *	out_Items[i*3]     : The address
	 *	out_Items[i*3 + 1] : The title
	 *	out_Items[i*3 + 2] : Depricated
	 *
	 *	@param prefix that is to be searched for
	 *	@param out_Items will be set to an array with the matches if no error occurs
	 *	@param out_ItemCount will be set to number of matches if no error occurs
	 *	@param sort_type - specify the sorted type (defaults to BY_TIME)
	 *	@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 *
	 *	@deprecated Provided for backwards compatibility - use GetItems above instead.
	 */
    virtual OP_STATUS GetItems(const OpStringC& prefix,
							   uni_char*** out_Items,
							   int* out_ItemCount,
							   SortType sort_type = BY_TIME) = 0;

	/**
	 * Returns TRUE if this url has an acceptable form. That is if it will be inserted into
	 * history. This method is supplied to protect calls to Add/AddBookmark as these will
	 * produce an assertion if given an unacceptable address.
	 *
	 * @param address to be tested
	 * @return returns TRUE if the address can be inserted into history
	 */
	virtual BOOL IsAcceptable(const OpStringC& address) = 0;

	/**
	 * 
	 *
	 * @param address
	 * @param stripped
	 *
	 * @return
	 */
	virtual BOOL IsLocalFileURL(const OpStringC& address,
								OpString& stripped) = 0;

	/**
	 * The address is stored as an escaped string. If you want to show it
	 * as an unescaped string to make it as readable as possible, this
	 * will change the string to unescaped.
	 *
	 * @param address to be transformed
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	virtual OP_STATUS MakeUnescaped(OpString& address) = 0;

	/**
	 * Returns the number of seconds since last midnight
	 */
	virtual time_t SecondsSinceMidnight() = 0;

#ifdef SELFTEST
	virtual OP_STATUS AddPage(const OpStringC& url_name,
							  const OpStringC& title,
							  time_t acc,
							  BOOL save_session = TRUE,
							  time_t average_interval = -1) = 0;
#endif // SELFTEST

private:

	virtual OP_STATUS InitOperaPages() = 0;
};

typedef OpHistoryModel CoreHistoryModel;   // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // OP_HISTORY_MODEL_H
