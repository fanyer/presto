/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Patricia Aas (psmaas)
 */
#ifndef VISITED_SEARCH_THREAD_H
#define VISITED_SEARCH_THREAD_H

#include "adjunct/desktop_util/thread/DesktopThread.h"
#include "adjunct/desktop_util/thread/DesktopMutex.h"
#include "modules/search_engine/VisitedSearch.h"
#include "modules/search_engine/VSUtil.h"
#include "modules/hardcore/mh/messobj.h"

/**
 * @brief A thread to run the VisitedSearch in
 * @author Patricia Aas
 *
 * This is a coordination class to provide threadsafe access to the
 * VisitedSearch object in the search_engine module.
 *
 * It provides two interfaces, one to the mainthread through the public
 * functions and a set of private functions that are to be called by the thread
 * function HandleMessage.  In addition two these two sets of functions it has a
 * set of private functions for internal synchronization.
 *
 * Main Thread interface           : all public functions
 *
 * Visited Search Thread interface : HandleMessage and all private Handle* functions
 *
 * Synchronization interface       : AddSearch, RemoveSearch and ContinueSearch
 *
 * The main flow is from the mainthread through the public functions which will
 * post messages in a threadsafe manner to the DesktopThread baseclass, these
 * will be picked off the queue by the thread which will call the virtual
 * HandleMessage - implemented here in the subclass.
 *
 * HandleMessage will call a set of Handle* functions to perform the various
 * tasks - since these are called from HandleMessage they will run in the
 * thread.
 */
class VisitedSearchThread :
    public AsyncVisitedSearch,
    public MessageObject,
    private DesktopThread
{
public:

	/** Constructor
     */
    VisitedSearchThread();

	/** Destructor
	  */
	virtual ~VisitedSearchThread() {}

	/**
	 * change the maximum size of the index;
	 * depending on the size and flush phase, the change may not come into effect immediatelly
	 * @param max_size new index size in MB
	 */
	virtual void SetMaxSize(OpFileLength max_size);

	/**
	 * change the maximum size to contain aproximatelly max_items
	 * @param max_items maximal number of items the full index should roughly contain
	 */
	virtual void SetMaxItems(int max_items);

    /**
	 * Open/create the index in teh given directory.
	 * Must be called before using other methods except IsOpen.
	 * @param directory full path without terminating path separator
	 */
	virtual OP_STATUS Open(const uni_char *directory);

	/**
	 * flush all cached data and closes all resources
	 * @param force_close close all resources even if all operations cannot be completed e.g. because out of disk space
	 * @return if force_close was set, returns error anyway, but the resources are released
	 */
	virtual OP_STATUS Close(BOOL force_close = TRUE);

    /**
     *
     */
	virtual BOOL IsOpen();

	/**
	 * erase all data
	 * @param reopen if FALSE, close the index when all data is cleared
	 */
	virtual OP_STATUS Clear(BOOL reopen = TRUE);

	/**
	 * Open a new record for writing. Not closing the record by CloseRecord or AbortRecord is evil.
	 * VisitedSearch must be opened before using this method.
	 * @param url address of the topmost document in the current window
	 * @param title the <title> tag from the header of the document
	 * @return 0 on out of memory or if indexing is disabled/uninitialized
	 */
	virtual VisitedSearch::RecordHandle CreateRecord(const char *url,
                                                     const uni_char *title = NULL);

	/**
	 * add the title of the document if it wasn't available at the time of CreateRecord
	 * @param handle handle created by CreateRecord
	 * @param title the <title> tag from the header  of the document
	 */
	virtual OP_STATUS AddTitle(VisitedSearch::RecordHandle handle,
                               const uni_char *title);

	/**
	 * add a block of text with the same ranking
	 * @param handle handle created by CreateRecord
	 * @param text plain text words
	 * @param ranking float value from an open interval of (0, 1), should be one of the RANK_XXX
	 */
	virtual OP_STATUS AddTextBlock(VisitedSearch::RecordHandle handle,
                                   const uni_char *text,
                                   float ranking,
								   BOOL is_continuation = FALSE);

	/**
	 * make the data available for writing, no further changes will be possible
	 * @param handle handle created by CreateRecord
	 * @return aborts the record on error
	 */
	virtual OP_STATUS CloseRecord(VisitedSearch::RecordHandle handle);

	/**
	 * cancel insertion of this document to the index, handle becomes invalid after this
	 * @param handle handle created by CreateRecord
	 */
	virtual void AbortRecord(VisitedSearch::RecordHandle handle);

    /**
	 * lookup documents contatining all the given words
	 * @param text plain text words
	 * @param sort_by sort results by relevance or a date of viewing
	 * @return
	 */
    virtual OP_STATUS Search(const uni_char *text,
                             VisitedSearch::Sort sort_by,
                             int max_items,
                             int excerpt_max_chars,
                             const OpStringC & excerpt_start_tag,
                             const OpStringC & excerpt_end_tag,
                             int excerpt_prefix_ratio,
                             MessageObject * callback);

    /**
     * NOT RECOMMENDED : Synchronous search function
     * @return a search iterator with the results of the search
     */
    virtual SearchIterator<VisitedSearch::Result> *Search(const uni_char *text,
                                                          VisitedSearch::Sort sort_by = VisitedSearch::RankSort,
														  int phrase_flags = PhraseMatcher::AllPhrases);

	
	/**
	 * disable the url from appearing in any further search results
	 */
	virtual OP_STATUS InvalidateUrl(const char *url);
	virtual OP_STATUS InvalidateUrl(const uni_char *url);

    /**
     *
     */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

    struct TextRecord
    {
        VisitedSearch::RecordHandle handle;
        OpString text;
        float ranking;
		BOOL is_continuation;

        TextRecord(VisitedSearch::RecordHandle p_handle,
                   const uni_char * p_text,
                   float p_ranking,
				   BOOL p_is_continuation)
            : handle(p_handle), ranking(p_ranking), is_continuation(p_is_continuation)
            { OpStatus::Ignore(text.Set(p_text)); }
    };

	struct NewRecord
	{
		VisitedSearch::RecordHandle handle;
		OpString8 url;
		OpString title;
	
		NewRecord(VisitedSearch::RecordHandle p_handle,
				  const char* p_url,
				  const uni_char* p_title)
			: handle(p_handle)
			{ OpStatus::Ignore(url.Set(p_url)); OpStatus::Ignore(title.Set(p_title)); }
	};

    struct SearchCounter
    {
        int counter;

        SearchCounter() : counter(0) {}
    };

	OP_STATUS StartThread();

	void StopThread();

    OP_STATUS AddSearch(MessageObject * callback);

    OP_STATUS RemoveSearch(MessageObject * callback);

    BOOL ContinueSearch(MessageObject * callback);

	void HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void HandleCreateRecord(NewRecord * new_record);

    void HandleAddTitle(VisitedSearch::RecordHandle handle,
                        OpString * title);

    void HandleAddTextBlock(TextRecord * text_record);

    void HandleCloseRecord(VisitedSearch::RecordHandle handle);

    void HandleAbortRecord(VisitedSearch::RecordHandle handle);

    void HandleSearch(VisitedSearch::SearchSpec * search_spec,
                      MessageObject * callback);

    void HandleInvalidateUrl(char *url);

	VisitedSearch::RecordHandle GetRealHandle(VisitedSearch::RecordHandle handle);

    VisitedSearch      m_search;
    DesktopMutex       m_search_mutex;
    BOOL               m_is_open;
    OpPointerHashTable<MessageObject, SearchCounter> m_current_searches;
    DesktopMutex       m_table_mutex; ///< Mutex to protect the hashtable m_current_searches
	VisitedSearch::RecordHandle m_next_free_handle;
	OpPointerHashTable<RecordHandleRec, RecordHandleRec> m_record_map;
	BOOL			   m_initialized;
};

#endif // VISITED_SEARCH_THREAD_H
