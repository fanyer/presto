/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOC_SEARCH_H
#define LOGDOC_SEARCH_H

#ifndef HAS_NO_SEARCHTEXT
#ifdef SEARCH_MATCHES_ALL

#include "modules/util/simset.h"

class SearchData;
class HTML_Element;
class LogicalDocument;

/** Class to hold partial matches during text search.
 *
 * A match point is denoted by an element + offset pair.
 * If the element is a text node, the offset is the character
 * position within the text content.
 * If the element is a form element, the offset is the character
 * position within the text representation of the form element's
 * value (@See FormValue::GetValueAsText()).
 */
class PartialMatch
{
public:

	PartialMatch() : m_elm(NULL), m_offset(0), m_chars_done(0) {}

	HTML_Element*	GetElm() const { return m_elm; }
	int				GetOffset() const { return m_offset; }
	int				GetCharsDone() const { return m_chars_done; }
	void			SetCharsDone(unsigned int done) { m_chars_done = done; }
	BOOL			GetShouldActivate() const { return m_activate; }

	void			Set(HTML_Element *start, int offset, int chars_done, BOOL activate)
		{ m_elm = start; m_offset = offset; m_chars_done = chars_done; m_activate = activate; }
	void			Reset() { m_elm = NULL; m_offset = 0; m_chars_done = 0; m_activate = FALSE; }

private:

	HTML_Element*	m_elm; ///< Element where the match is
	int				m_offset; ///< Offset into the text of the element
	int				m_chars_done; ///< How many characters matched before we stopped
	BOOL			m_activate; ///< TRUE if the search hit should be activated
};

/// Helper class for text searches in pages
class SearchHelper
{
public:

	/**
	 * Constructor.
	 * @param data SearchData describing the what to search for
	 *  and how.
	 * @param logdoc The logical document in which to search
	 */
	SearchHelper(SearchData *data, LogicalDocument *logdoc)
		: m_logdoc(logdoc)
		, m_data(data)
		, m_matches(NULL)
		, m_current_link(NULL)
		, m_last_word_start(NULL)
		, m_active_offset(0)
		, m_leading_spaces(0)
		, m_active(NULL)
		, m_next_is_active(FALSE) {}

	~SearchHelper() { OP_DELETEA(m_matches); }

	/// Secondary constructor. Call this before any searches.
	OP_STATUS	Init();
	/** Call this to start a search for the text in SearchData
     * @param current_highlight_element The currently highlighted
	 *  element. The first hit after this element will be set as the
	 *  active one.
	 */
	OP_STATUS	FindAllMatches(HTML_Element *current_highlight_element, int current_offset);

private:
	LogicalDocument*	m_logdoc; ///< The logical document to search
	SearchData*			m_data; ///< Description of how and what to search for
	PartialMatch*		m_matches; ///< Array of unfinished, partial matches
	/** When matching for a whole word and a word was found at the end of a
	 * block of text we need to check the next element to find out if the
	 * word ended there or continues in the next one. The start of the matched
	 * word is pointed to by this variable */
	PartialMatch		m_end_pending_word_finished;
	/// NULL if we are not in a link otherwise points to the link we are in
	HTML_Element*		m_current_link;
	/// Start of the word we are currently matching
	const uni_char*		m_last_word_start;
	int					m_active_offset; ///< The char offset into the currently selected element
	int					m_leading_spaces; ///< The number of leading spaces in the search phrase
	HTML_Element*		m_active; ///< The currently selected element
	BOOL				m_next_is_active; ///< The next match should be marked as active

	/**
	 * To remove one of the partial matches because we find out that it didn't
	 * match anything after all.
	 * @param idx Index into the partial match table of the match
	 */
	void		RemovePartialMatch(unsigned int idx);

	/**
	 * Search an element for matching text. Should only be called for elements
	 * with meaningful text representation like text or form elements.
	 * @param elm The text element to search in
	 */
	OP_STATUS	SearchElement(HTML_Element *elm);

	/**
	 * Will search the string for matching text.
	 * @param content The text contents of the element
	 * @param txt_elm The element which the text belongs to
	 */
	OP_STATUS	SearchText(const uni_char *content, HTML_Element *txt_elm);
	/**
	 * Called when going into or leaving an element during search traversal
	 * @param elm The element we enter or leave
	 * @entering Set to TRUE if we are going into otherwise FALSE
	 */
	OP_STATUS	EnterOrLeaveElement(HTML_Element *elm, BOOL entering);
	/**
	 * Called when we find a match to add it to the list of hits
	 */
	OP_STATUS	AddSearchHit(const PartialMatch *start, const PartialMatch *end);
};

#endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

#endif // LOGDOC_SEARCH_H
