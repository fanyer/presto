/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SEARCHINFO_H
#define SEARCHINFO_H

#include "modules/logdoc/selection.h"
#include "modules/logdoc/selectionpoint.h"
#include "modules/util/simset.h"


class FramesDocument;
class HTML_Element;


class SelectionElm : public ListElement<SelectionElm>
{
private:

	TextSelection	m_selection;

public:

					SelectionElm(FramesDocument *frm_doc,
						const SelectionBoundaryPoint *start,
						const SelectionBoundaryPoint *end);
					~SelectionElm();

	TextSelection*	GetSelection() { return &m_selection; }
};

/**
 * Object used for searching.
 *
 * @author stighal (I think)
 */
class SearchData
{
public:
	/**
	 * Creates an object that can be used to search a document. See
	 * HTML_Document::FindAllMatches() and
	 * HTML_Document::HighlightNextMatch().
	 *
	 * @param[in] match_case If TRUE requires matching case, if FALSE
	 * the search is case insensitive.
	 *
	 * @param[in] match_words Match whole words if TRUE, match partial
	 * words as well.
	 *
	 * @param[in] links_only If TRUE only text inside links will be
	 * considered. FALSE (the default) searches all text.
	 *
	 * @param[in] forward If TRUE searches forward, if FALSE searches
	 * backwards from the current search hit.
	 *
	 * @param[in] wrap If TRUE, the search will restart from the
	 * beginning/end of the document in case there was no hit from the
	 * first search and the result of a search from that position will
	 * be returned.
	 */
	SearchData(BOOL match_case, BOOL match_words, BOOL links_only, BOOL forward, BOOL wrap)
		: m_match_doc(NULL),
		m_new_search(TRUE),
		m_match_case(match_case),
		m_match_words(match_words),
		m_links_only(links_only),
		m_forward(forward),
		m_wrap(wrap) {}

	FramesDocument*	GetMatchingDoc() { return m_match_doc; }
	void		SetMatchingDoc(FramesDocument* doc) { m_match_doc = doc; }

	OP_STATUS		SetSearchText(const uni_char *txt);
	const uni_char*	GetSearchText();

	BOOL		IsNewSearch() { return m_new_search; }
	void		SetIsNewSearch(BOOL val) { m_new_search = val; }

	BOOL		GetMatchCase() { return m_match_case; }
	BOOL		GetMatchWords() { return m_match_words; }
	BOOL		GetLinksOnly() { return m_links_only; }
	BOOL		GetForward() { return m_forward; }
	BOOL		GetWrap() { return m_wrap; }

	void		SetMatchCase(BOOL val);
	void		SetMatchWords(BOOL val);
	void		SetLinksOnly(BOOL val);
	void		SetForward(BOOL val) { m_forward = val; }
	void		SetWrap(BOOL val) { m_wrap = val; }

private:
	FramesDocument*	m_match_doc;
	OpString		m_text;
	BOOL			m_new_search;
	BOOL			m_match_case;
	BOOL			m_match_words;
	BOOL			m_links_only;
	BOOL			m_forward;
	BOOL			m_wrap;
};

#endif // SEARCHINFO_H
