/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef VISITEDSEARCH_H
#define VISITEDSEARCH_H

#include "modules/search_engine/Cursor.h"
#include "modules/search_engine/ResultBase.h"
#include "modules/search_engine/Vector.h"
#include "modules/search_engine/RankIndex.h"
#include "modules/hardcore/mh/mh.h"

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
#include "modules/search_engine/log/Log.h"
#endif
#ifdef SEARCH_ENGINE_PHRASESEARCH
#include "modules/search_engine/PhraseSearch.h"
#endif

#define RANK_TITLE  0.5F
#define RANK_H1     0.6F
#define RANK_H2     0.7F
#define RANK_H3     0.8F
#define RANK_H4     0.85F
#define RANK_H5     0.88F
#define RANK_H6     0.89F
#define RANK_I      0.9F
#define RANK_EM     0.91F
#define RANK_P      0.95F

class RecordHandleRec;
struct FileWord;

/**
 * @brief fulltext indexing/searching designed to hold URLs
 * @author Pavel Studeny <pavels@opera.com>
 */
class VisitedSearch : public MessageObject, private NonCopyable
{
public:
	/** handle to create a record for a URL */
	typedef RecordHandleRec *RecordHandle;

	VisitedSearch(void);

	~VisitedSearch(void)
	{
		OP_ASSERT(m_pending.GetCount() == 0);

		if (m_index.GetCount() != 0)
			OpStatus::Ignore(Close());
	}

	/**
	 * change the maximum size of the index;
	 * depending on the size and flush phase, the change may not come into effect immediately
	 * @param max_size new index size in MB
	 */
	void SetMaxSize(OpFileLength max_size);

	/**
	 * change the maximum size to contain approximately max_items.
	 * Could be analyzed later, because mapping to history items is not always 1:1 this could be improved.
	 * @param max_items maximal number of items the full index should roughly contain
	 */
	void SetMaxItems(int max_items);

#ifdef SELFTEST
	void SetSubindexSize(OpFileLength ssize) {m_subindex_size = ssize;}
#endif

	/**
	 * Open/create the index in the given directory.
	 * Must be called before using other methods except IsOpen.
	 * @param directory full path without terminating path separator
	 */
	CHECK_RESULT(OP_STATUS Open(const uni_char *directory));

	/**
	 * flush all cached data and closes all resources
	 * @param force_close close all resources even if all operations cannot be completed e.g. because out of disk space
	 * @return if force_close was set, returns error anyway, but the resources are released
	 */
	CHECK_RESULT(OP_STATUS Close(BOOL force_close = TRUE));

	BOOL IsOpen(void)
	{
		return m_index.GetCount() > 0;
	}

	/**
	 * erase all data
	 * @param reopen if FALSE, close the index when all data is cleared
	 */
	CHECK_RESULT(OP_STATUS Clear(BOOL reopen = TRUE));

/*** indexing part ***/

	/**
	 * Open a new record for writing. Not closing the record by CloseRecord or AbortRecord is evil.
	 * VisitedSearch must be opened before using this method.
	 * @param url address of the topmost document in the current window
	 * @param title the <title> tag from the header of the document
	 * @return 0 on out of memory or if indexing is disabled/uninitialized
	 */
	RecordHandle CreateRecord(const char *url, const uni_char *title = NULL);

	/**
	 * add the title of the document if it wasn't available at the time of CreateRecord
	 * @param handle handle created by CreateRecord
	 * @param title the <title> tag from the header  of the document
	 */
	CHECK_RESULT(OP_STATUS AddTitle(RecordHandle handle, const uni_char *title));

	/**
	 * add a block of text with the same ranking
	 * @param handle handle created by CreateRecord
	 * @param text plain text words
	 * @param ranking float value from an open interval of (0, 1), should be one of the RANK_XXX
	 * @param is_contination if TRUE, this text block should be considered a continuation of the previous
	 */
	CHECK_RESULT(OP_STATUS AddTextBlock(RecordHandle handle, const uni_char *text, float ranking, BOOL is_continuation = FALSE));

	/**
	 * make the data available for writing, no further changes will be possible
	 * @param handle handle created by CreateRecord
	 * @return aborts the record on error
	 */
	CHECK_RESULT(OP_STATUS CloseRecord(RecordHandle handle));

	/**
	 * cancel insertion of this document to the index, handle becomes invalid after this
	 * @param handle handle created by CreateRecord
	 */
	void AbortRecord(RecordHandle handle);

	/**
	 * attach a small picture of the document
	 * @param url URL of existing record created by CreateRecord
	 * @param picture data in a format known to the caller
	 * @param size size of the picture in bytes
	 */
	CHECK_RESULT(OP_STATUS AssociateThumbnail(const char *url, const void *picture, int size));

	/**
	 * associate a locally saved copy of the web page with the current handle
	 * @param url URL of existing record created by CreateRecord
	 * @param fname full path to a single file
	 */
	CHECK_RESULT(OP_STATUS AssociateFileName(const char *url, const uni_char *fname));

	/**
	 * begin transaction and prepare data from the cache to be written to disk;
	 * all Inserts or Deletes until Commit will not be included in this transaction;
	 * an error cancels the whole transaction and the data are returned back to cache
	 *
	 * @param max_ms maximum time to spend by PreFlush in miliseconds, 0 means unlimited
	 * @return OpBoolean::IS_TRUE if finished successfully, OpBoolean::IS_FALSE if time limit was reached (call PreFlush again)
	 * @see BlockStorage for more information about the transaction modes
	 */
	CHECK_RESULT(OP_BOOLEAN PreFlush(int max_ms = 0));

	/**
	 * write the data prepared by PreFlush;
	 * calling PreFlush before Flush is optional, but a delay (roughly 30s) between PreFlush and Flush
	 * reduces the time spent in operating system calls;
	 * an error cancels the whole transaction and the data are returned back to cache
	 *
	 * @param max_ms maximum time to spend by PreFlush in miliseconds, 0 means unlimited
	 * @return OpBoolean::IS_TRUE if finished successfully, OpBoolean::IS_FALSE if time limit was reached (call Flush again)
	 */
	CHECK_RESULT(OP_BOOLEAN Flush(int max_ms = 0));

	/**
	 * finish the transaction begun by PreFlush;
	 * calling Flush before Commit is optional, but a delay (roughly 30s) between Flush and Commit
	 * reduces the time spent in operating system calls
	 * @return not supposed to fail under normal circumstances if Flush was called and finished successfully
	 */
	CHECK_RESULT(OP_STATUS Commit(void));

/*** searching part ***/

	enum Sort
	{
		RankSort,  /**< sort the results by ranking, best ranking first */
		DateSort,  /**< sort the results by date, latest date first */
		Autocomplete /**< like RankSort, but no results for empty query */
	};

	/** one row of a result */
	struct Result
	{
		char *url;
		uni_char *title;
		unsigned char *thumbnail;
		int thumbnail_size;
		uni_char *filename;
		time_t visited;
		float ranking;

#ifndef SELFTEST
	protected:
#endif
		UINT32 id;  // for sorting purposes

		bool invalid;
		UINT16 prev_idx;
		UINT32 prev;
		UINT16 next_idx;
		UINT32 next;
		mutable uni_char *plaintext;
		mutable unsigned char *compressed_plaintext;
		mutable int compressed_plaintext_size;

		friend class RankIterator;
		friend class AllDocIterator;
		friend class TimeIterator;
		friend class VisitedSearch;
		friend class FastPrefixIterator;
	public:

		Result(void);

		CHECK_RESULT(OP_STATUS SetCompressedPlaintext(const unsigned char *buf, int size));
		uni_char *GetPlaintext() const;

		BOOL operator<(const Result &right) const
		{
			float s, a;

			s = ranking - right.ranking;
			a = (ranking + right.ranking) / 100000.0F;
			if (s < a && s > -a)
			{
				if (id == (UINT32)-1 && right.id == (UINT32)-1)
				{
					if (visited == right.visited)
						return op_strcmp(url, right.url) != 0;

					return visited > right.visited;
				}

				return id < right.id;
			}

			return s < 0.0;
		}

		CHECK_RESULT(static OP_STATUS ReadResult(Result &res, BlockStorage *metadata));

		static BOOL Later(const void *left, const void *right);
		static BOOL CompareId(const void *left, const void *right);
		static void DeleteResult(void *item);
		CHECK_RESULT(static OP_STATUS Assign(void *dst, const void *src));
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		static size_t EstimateMemoryUsed(const void *item);
#endif
	};

	struct SearchSpec
	{
		OpString query;
		Sort sort_by;
		int max_items;
		int max_chars;
		OpString start_tag;
		OpString end_tag;
		int prefix_ratio;
	};

	struct SearchResult : public NonCopyable
	{
		OpString8 url;
		OpString title;
		unsigned char *thumbnail;
		int thumbnail_size;
		OpString filename;
		OpString excerpt;
		time_t visited;
		float ranking;

		SearchResult() : thumbnail(NULL), thumbnail_size(0), visited(0), ranking(0) {}
		~SearchResult() { OP_DELETEA(thumbnail); }
		CHECK_RESULT(OP_STATUS CopyFrom(const Result & result, SearchSpec * search_spec));
	};

	/**
	 * lookup documents contatining all the given words. Full ranking
	 * @param text plain text words
	 * @param sort_by sort results by relevance or a date of viewing
	 * @param phrase_flags flags built up from PhraseMatcher::PhraseFlags, controlling what kind of phrase search is performed
	 * @return iterator to be deleted by a caller, NULL on error
	 */
#ifdef SEARCH_ENGINE_PHRASESEARCH
	SearchIterator<Result> *Search(const uni_char *text, Sort sort_by = RankSort, int phrase_flags = PhraseMatcher::AllPhrases);
#else
	SearchIterator<Result> *Search(const uni_char *text, Sort sort_by = RankSort, int phrase_flags = 0/*PhraseMatcher::NoPhrases*/);
#endif

	/**
	 * lookup documents contating all the words contained in text, last one as a prefix
	 * ranking or other sorting is not involved as a trade off for a fast response
	 * @param text searched phrase
	 * @param phrase_flags flags built up from PhraseMatcher::PhraseFlags, controlling what kind of
	 *        phrase search is performed. PhraseMatcher::PrefixSearch is implied.
	 * @return iterator to be deleted by a caller, NULL on error
	 */
	SearchIterator<Result> *FastPrefixSearch(const uni_char *text, int phrase_flags = 0/*PhraseMatcher::NoPhrases*/);

	/**
	 * disable the result from appearing in any further search results
	 */
	CHECK_RESULT(OP_STATUS InvalidateResult(const Result &row));

	/**
	 * disable the url from appearing in any further search results
	 */
	CHECK_RESULT(OP_STATUS InvalidateUrl(const char *url));
	CHECK_RESULT(OP_STATUS InvalidateUrl(const uni_char *url));

	/**
	 * find the indexed words for autocompletition
	 * @param prefix word prefix to search
	 * @param result resulting words, mustn't be NULL, the fields must be freed by caller
	 * @param result_size maximum number of results on input, number of results on output
	 */
	CHECK_RESULT(OP_STATUS WordSearch(const uni_char *prefix, uni_char **result, int *result_size));

/*** message part ***/
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * cancel the data being written by PreFlush/Flush/Commit
	 */
	void AbortFlush(void);

protected:
	enum Flags
	{
		PreFlushed =       4096,  /*< PreFlush had finished successfully before Flush */
		Flushed =          8192,  /*< Flush  had finished successfully before Commit */
		PreFlushing =     32768,  /*< time limit occured during PreFlush */
		UseNUR =          65536   /*< PreFlush hadn't been called before Flush, use alternative write strategy */
	};

	CHECK_RESULT(OP_STATUS IndexTextBlock(RecordHandle handle, const uni_char *text, float ranking, BOOL fine_segmenting = FALSE));
	CHECK_RESULT(OP_STATUS IndexURL(RecordHandle handle, const char *url, float ranking));

	CHECK_RESULT(OP_STATUS Insert(TVector<FileWord *> &cache, RecordHandle h, const uni_char *word, float rank));

	void CancelPreFlushItem(int i);
	CHECK_RESULT(OP_STATUS WriteMetadata(void));

	CHECK_RESULT(OP_STATUS FindLastIndex(UINT32 *prev, UINT16 *prev_idx, const char *url, RankIndex *current_index));
	CHECK_RESULT(OP_STATUS InsertHandle(RecordHandle handle));
	CHECK_RESULT(OP_STATUS CompressText(RecordHandle handle));

	CHECK_RESULT(OP_STATUS WipeData(BSCursor::RowID id, unsigned short r_index));

	void CheckMaxSize_RemoveOldIndexes();

    // Posts a message in a threadsafe manner when necessary
    void PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay=0);

	int m_flags;
	OpFileLength m_max_size;		//total size of document index, matched to History size in desktop browser.

	int m_pos;
	int m_flush_errors;

	TVector<RankIndex *> m_index;   //array of indexes, the newest index is at index 0.

	TVector<FileWord *> m_cache;    //word lists inserted but not yet written to disk.
	TVector<FileWord *> m_preflush; //word lists that are being written.

	TVector<RecordHandle> m_pending; //contains documents that are being parsed.
	TVector<RecordHandle> m_meta;	 //plain text, hash etc.
	TVector<RecordHandle> m_meta_pf; //meta data that is being written.

	OpFileLength m_subindex_size; //10 times less than m_max_size above.

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	OutputLogDevice *m_log;
#endif

	OP_STATUS AddWord(VisitedSearch::RecordHandle handle, const uni_char *text, float ranking);

private:
	CHECK_RESULT(OP_STATUS AbortPreFlush(OP_STATUS err));
	CHECK_RESULT(OP_STATUS AbortFlush(OP_STATUS err));

#ifdef SEARCH_ENGINE_PHRASESEARCH
	class VisitedSearchDocumentSource : public DocumentSource<Result>
	{
		virtual const uni_char *GetDocument(const Result &item)
		{
			return item.GetPlaintext();
		}
	};
	VisitedSearchDocumentSource m_doc_source;
#endif
};

#ifdef VPS_WRAPPER
class AsyncVisitedSearch
{
public:
	static AsyncVisitedSearch *Create(void);

	virtual ~AsyncVisitedSearch(void) {}

	virtual void SetMaxSize(OpFileLength max_size) = 0;

	virtual void SetMaxItems(int max_items) = 0;

	CHECK_RESULT(virtual OP_STATUS Open(const uni_char *directory)) = 0;

	CHECK_RESULT(virtual OP_STATUS Close(BOOL force_close = TRUE)) = 0;

	virtual BOOL IsOpen(void) = 0;

	CHECK_RESULT(virtual OP_STATUS Clear(BOOL reopen = TRUE)) = 0;

	virtual VisitedSearch::RecordHandle CreateRecord(const char *url, const uni_char *title = NULL) = 0;

	CHECK_RESULT(virtual OP_STATUS AddTitle(VisitedSearch::RecordHandle handle, const uni_char *title)) = 0;

	CHECK_RESULT(virtual OP_STATUS AddTextBlock(VisitedSearch::RecordHandle handle, const uni_char *text, float ranking, BOOL is_continuation = FALSE)) = 0;

	CHECK_RESULT(virtual OP_STATUS CloseRecord(VisitedSearch::RecordHandle handle)) = 0;

	virtual void AbortRecord(VisitedSearch::RecordHandle handle) = 0;

    CHECK_RESULT(virtual OP_STATUS Search(const uni_char *text,
                             VisitedSearch::Sort sort_by,
                             int max_items,
                             int excerpt_max_chars,
                             const OpStringC & excerpt_start_tag,
                             const OpStringC & excerpt_end_tag,
                             int excerpt_prefix_ratio,
                             MessageObject * callback)) = 0;

    virtual SearchIterator<VisitedSearch::Result> *Search(const uni_char *text,
                                                          VisitedSearch::Sort sort_by = VisitedSearch::RankSort,
#ifdef SEARCH_ENGINE_PHRASESEARCH
                                                          int phrase_flags = PhraseMatcher::AllPhrases
#else
                                                          int phrase_flags = 0/*PhraseMatcher::NoPhrases*/
#endif
														  ) = 0;

	CHECK_RESULT(virtual OP_STATUS InvalidateUrl(const char *url)) = 0;
	CHECK_RESULT(virtual OP_STATUS InvalidateUrl(const uni_char *url)) = 0;
};
#endif

#endif  // VISITEDSEARCH_H

