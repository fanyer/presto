/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef PHRASESEARCH_H
#define PHRASESEARCH_H

#include "modules/search_engine/ResultBase.h"
#include "modules/search_engine/Vector.h"
#include "modules/search_engine/WordSegmenter.h"

/**
 * @brief The PhraseMatcher extracts phrase elements from a text query and can
 * check if a document matches.
 * 
 * Unless the FullSearch flag is set,
 * parts of the text query that is not phrase related is ignored and not
 * searched for in the document. The main usage is to post-process search
 * results where all whole words are already accounted for and present in the
 * results. If a query does not contain any phrase elements, all documents
 * will match. Phrases match case-insensitively. 
 * 
 * @par
 * Definitions:
 * @li A word is defined as a substring that would be returned if the query
 *     string was parsed by the WordSegmenter. This also includes single
 *     CJK characters that are not separated by space.
 * @li Only space, '+' and double quotes have special meaning in phrase
 *     search. All whitespace and '+' is treated as space.
 * 
 * @par
 * Phrase elements in a search query is defined as follows:
 * @li More than one word in double quotes is a phrase element if the
 *     QuotedPhrases flag is set.
 * @li A sub-string containing non-word characters other than space, '+' and
 *     double quotes is a phrase element if the PunctuationPhrases flag is set.
 * @li A sub-string consisting of only word characters that contains more
 *     than one word is a phrase element if the CJKPhrases flag is set.
 *     
 * @par
 * Examples of quoted phrase elements:
 * @li "to be, or not to be"
 * @li "to be"
 * 
 * @par
 * Examples of "punctuation" phrase elements:
 * @li be,
 * @li [BTS]
 * @li opera.com
 * 
 * @par
 * Examples of CJK phrase elements
 * @li ABC (where A, B and C are CJK characters)
 * 
 * @par
 * Examples of non-phrase elements:
 * @li to be or not to be
 * @li "to"
 * @li to+be+" " ('+' is considered a space (esp. in forms))
 * 
 * @par
 * A query with phrase content in double quotes match if all the phrase
 * elements within the quotes are located consecutively in the document,
 * separated by only non-word characters. That is, a phrase like
 * "to be or not to be" will match a document containing
 * "to be, or not to be".
 */
class PhraseMatcher
{
public:
	enum PhraseFlags
	{
		NoPhrases          = 0, ///< Don't do phrase filtering
		CJKPhrases         = 1, ///< Match phrases consisting of multiple consecutive CJK characters.
		QuotedPhrases      = 2, ///< Match quoted phrases. This also implies CJKPhrases within the quotes.
		PunctuationPhrases = 4, ///< Match phrases built up by a combination of word and non-word characters. This also implies CJKPhrases.
		AllPhrases         = 7, ///< Match all types of phrases
		PrefixSearch       = 8, ///< If set, do not require that the found phrase ends on a word boundary
		FullSearch         =16, ///< If set, search for all words, not only phrase content. (Do not assume that all single words have already been found. Useful with AppendHighlight)
		DontCopyInputString=32  /**< The query string set with Init will be used directly, not copied.
									 The string must not be deleted before the PhraseMatcher is.
									 Note that this flag prevents preprocessing of the input string (e.g. to remove &shy; characters) */
	};

	PhraseMatcher();

	virtual ~PhraseMatcher();

	/**
	 * initialize with a search query
	 * @param query the query used for searching, presumably containing phrases
	 * @param phrase_flags flags built up from PhraseFlags, controlling which phrases are matched
	 * @return OK if there were no errors
	 */
	CHECK_RESULT(OP_STATUS Init(const uni_char *query, int phrase_flags));

	/**
	 * @return TRUE if no phrase elements were found in the query
	 */
	BOOL Empty() const { return m_phrases.GetCount() == 0; }

    /**
     * Search the haystack for the all the words in the query
     * @param haystack to search
     * @return TRUE if all the phrases are present in the haystack
     */
    BOOL Matches(const uni_char *haystack) const;

	/**
	 * append src to dst with searched words marked by start_tag and end_tag
	 * @param dst output with tagged words
	 * @param src plaintext excerpt, parser treats XML tags as regular words
	 * @param max_chars maximum number of plaintext characters which should appear in the output
	 * @param start_tag tag to prefix at the beginning of a searched word found in the plaintext; may be NULL
	 * @param end_tag tag to prefix at the end of a searched word found in the plaintext; may be NULL
     * @param prefix_ratio to use when generating the context before the first matched word, if less than or
     *        equal to zero the beginning of src will be used as the beginning of the excerpt 
	 */
    CHECK_RESULT(OP_STATUS AppendHighlight(OpString &dst,
										   const uni_char *src,
										   int max_chars,
										   const OpStringC &start_tag,
										   const OpStringC &end_tag,
										   int prefix_ratio = 15));

#ifdef SELFTEST
    uni_char *GetPhrases() const;
#endif

protected:
	struct Word : public WordSegmenter::Word
	{
		const uni_char *found_pos; ///< Pointer to where in the haystack the word was found
		int found_len;             ///< Length of the occurrence that was found
        BOOL is_last;              ///< TRUE if this was the last, possibly unfinished word and we are doing prefix search

		Word() : WordSegmenter::Word(), found_pos(NULL), found_len(0), is_last(FALSE) {}
	};

	static BOOL IsDoubleQuote(UnicodePoint c);
	BOOL TreatAsSpace(UnicodePoint c) const;
	static BOOL FindPhrase(TVector<Word> *phrase, const uni_char *haystack);
	static BOOL FindWord(Word &word, const uni_char *s, const uni_char *haystack, BOOL first_word);

	static int CopyText(uni_char *&dst_ptr, const uni_char *from, const uni_char *end);
    static int CopyResult(uni_char *&dst_ptr, Word &result, const OpStringC &start_tag, const OpStringC &end_tag);
    static const uni_char *FindEnd(const uni_char *start, int length);
    static const uni_char *FindStart(const uni_char *text, const uni_char *start, int length, int prefix_ratio);

	const uni_char *m_query;
	TVector<TVector<Word> *> m_phrases;
	int m_phrase_flags;
};

/**
 * @brief Used by PhraseFilter to get the document corresponding to a search
 * result to be able to look for the actual phrase.
 */
template <typename T> class DocumentSource
{
public:
	virtual ~DocumentSource() {}

	/**
	 * Get the document associated with a search result.
	 *
	 * Since phrase filtering in worst case can take a long time, it might be
	 * necessary to abort the search (e.g. in search-as-you-type, if another letter
	 * has been written to the search query). To effect an abort, the DocumentSource
	 * may return NULL on all subsequent calls. Since NULL documents do not match,
	 * no further results will be added, and the filtering will be quickly finished.
	 *
	 * If several "records" are concatenated into one document, but phrases should
	 * not match across record boundaries (e.g. message subject + message body), the
	 * records may be separated using the form-feed character ('\f').
	 *
	 * @param item A search result, potentially matching
	 * @return the document associated with item, or NULL to abort the search
	 */
	virtual const uni_char *GetDocument(const T &item) = 0;
};

/**
 * @brief A DocumentSource that automatically deletes the acquired documents.
 */
template <typename T> class AutoDeleteDocumentSource : public DocumentSource<T>
{
public:
	AutoDeleteDocumentSource() : m_doc(NULL) {}

	virtual ~AutoDeleteDocumentSource()
	{
		OP_DELETEA(m_doc);
	}

	virtual const uni_char *GetDocument(const T &item)
	{
		OP_DELETEA(m_doc);
		m_doc = AcquireDocument(item);
		return m_doc;
	}

protected:
	/**
	 * Get the document associated with a search result
	 * @param item A search result
	 * @return a document associated with item. Will be freed by the
	 *   destructor of this class using OP_DELETEA().
	 */
	virtual uni_char *AcquireDocument(const T &item) = 0;

	uni_char *m_doc;
};


/**
 * @brief PhraseFilter is a SearchFilter to be used with FilterIterator.
 *
 * It uses a PhraseMatcher to match documents retrieved from a DocumentSource.
 */
template <typename T> class PhraseFilter : public SearchFilter<T>
{
public:
	/**
	 * Construct a PhraseFilter
	 * @param query the query used for searching, presumably containing phrases
	 * @param doc_source The document source used to match phrases
	 * @param phrase_flags flags built up from PhraseMatcher::PhraseFlags, controlling what kind of phrase search is performed
	 */
	PhraseFilter(const uni_char *query, DocumentSource<T> &doc_source, int phrase_flags)
		: m_doc_source(doc_source)
	{
		m_status = OpStatus::OK;
		m_matcher = OP_NEW(PhraseMatcher, ());
		if (m_matcher == NULL)
		{
			m_status = OpStatus::ERR_NO_MEMORY;
		}
		else if (OpStatus::IsError(m_status = m_matcher->Init(query, phrase_flags)) || m_matcher->Empty())
		{
			OP_DELETE(m_matcher);
			m_matcher = NULL;
		}
		m_first_time = TRUE;
	}

	virtual ~PhraseFilter()
	{
		OP_DELETE(m_matcher);
	}
	
	/**
	 * @return TRUE if no phrases were found in the query
	 */
	virtual BOOL Empty() const { return m_matcher == NULL; }

	/**
	 * @param item A search result
	 * @return TRUE if the document associated with the search result matches the phrase
	 */
	virtual BOOL Matches(const T &item) const
	{
		if (!m_first_time)
			m_status = OpStatus::OK;
		m_first_time = FALSE;
		return m_matcher == NULL || m_matcher->Matches(m_doc_source.GetDocument(item));
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const) { return m_status; }

protected:
	mutable OP_STATUS m_status;
	mutable BOOL m_first_time;
	PhraseMatcher *m_matcher;
	DocumentSource<T> &m_doc_source;
};


#endif // PHRASESEARCH_H
