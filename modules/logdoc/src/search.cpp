/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifndef HAS_NO_SEARCHTEXT
#ifdef SEARCH_MATCHES_ALL

#include "modules/logdoc/src/search.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/layout/box/box.h"
#include "modules/util/OpHashTable.h"
#include "modules/doc/searchinfo.h"
#include "modules/doc/html_doc.h"
#include "modules/svg/SVGManager.h"

OP_STATUS SearchHelper::Init()
{
	const uni_char *search_text = m_data->GetSearchText();
	int len = uni_strlen(search_text) + 1;

	m_matches = OP_NEWA(PartialMatch, len);
	if (!m_matches)
		return OpStatus::ERR_NO_MEMORY;

	while (*search_text && uni_isspace(*search_text))
	{
		m_leading_spaces++;
		search_text++;
	}

	return OpStatus::OK;
}

void SearchHelper::RemovePartialMatch(unsigned int idx)
{
	unsigned int j = idx;
	for (; m_matches[j + 1].GetCharsDone(); j++)
		m_matches[j] = m_matches[j + 1];

	OP_ASSERT(j <= uni_strlen(m_data->GetSearchText()));

	m_matches[j].Reset();
}

BOOL IncludeFormInSearch(HTML_Element *elm)
{
	NS_Type ns = elm->GetNsType();
	if (ns == NS_HTML)
	{
		switch(elm->Type())
		{
		case HE_INPUT:
			switch (elm->GetInputType())
			{
			case INPUT_NOT_AN_INPUT_OBJECT:
			case INPUT_CHECKBOX:
			case INPUT_RADIO:
			case INPUT_HIDDEN:
			case INPUT_IMAGE:
			case INPUT_PASSWORD:
			case INPUT_FILE:
				return FALSE;
			}
			return TRUE;

		case HE_BUTTON:
		case HE_TEXTAREA:
		case HE_OUTPUT:
			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS SearchHelper::SearchElement(HTML_Element *elm)
{
	if (elm->Type() == HE_TEXT)
		return SearchText(elm->Content(), elm);
	else if (IncludeFormInSearch(elm))
	{
		FormValue *form_val = elm->GetFormValue();
		if (form_val)
		{
			OpString text;
			RETURN_IF_MEMORY_ERROR(form_val->GetValueAsText(elm, text));
			if (!text.IsEmpty())
				return SearchText(text.CStr(), elm);
		}
	}
// TODO: to fix bug 253779, complete this code
//	else if (!images_are_shown && elm->HasAttr(ATTR_ALT))
//		return SearchText(elm->GetStringAttr(ATTR_ALT), elm);

	return OpStatus::OK;
}

OP_STATUS SearchHelper::SearchText(const uni_char *content, HTML_Element *txt_elm)
{
	if (!content)
		return OpStatus::OK;

	const uni_char *text = m_data->GetSearchText();
	int start_word_idx = 0;
	unsigned int match_i = 0;

	while (!match_i || m_matches[match_i - 1].GetCharsDone())
	{
		int i = start_word_idx;
		int characters_done = m_matches[match_i].GetCharsDone();

		BOOL finished = FALSE;
		BOOL force_match_continue = FALSE;
		int start_char_offset = 0;

		while (content[i] && text[characters_done])
		{
			// Check if the search text matches the content...
			if ((!m_data->GetMatchWords() // ...if we are not matching whole words
					|| characters_done // ...or we have already started a matching word
					|| uni_isspace(content[i]) // ...or the content could match a space
					|| (i == 0 && m_last_word_start == NULL) // ...or we are starting a new word
					|| (m_data->GetMatchWords() && (i == 0 || uni_isspace(content[i - 1]) || uni_ispunct(content[i - 1]))) // or we are matching whole words and the char before was a space or a punctation
				)
				&& // ...and do the actual matching. We are matching if...
				// ...content is NBSP and we search for space
				((text[characters_done] == ' ' && uni_isspace(content[i]))
				  // ...we match case sensistive and the characters are equal
					|| (m_data->GetMatchCase() && text[characters_done] == content[i])
					// ...we are case insensitive and they match folded
					|| (!m_data->GetMatchCase() && op_tolower(text[characters_done]) == op_tolower(content[i]))
				))
			{
				if (!characters_done)
				{
					start_char_offset = i;
					if (m_data->GetMatchWords())
						m_last_word_start = content + i;
				}

				characters_done++;
			}
			else if (characters_done)
			{
				if (content[i] == 0x00ad /* soft hyphen */)
					// Ignore embedded soft hyphens.
					;
				else if (!uni_isspace(content[i])
						 || text[characters_done] == ' '
						 || text[characters_done - 1] != ' ')
				{
					// If we have several whitespace after each other
					// we should collapse them and they should match
					// a single space in the search expression.
					if (characters_done <= i
						&& (m_matches[match_i].GetElm() == txt_elm || m_matches[match_i].GetElm() == NULL))
					{
						// This match started in this element
						// Restart with character number two in the match.

						i = start_char_offset + 1;
						characters_done = 0;
						continue;
					}
					else
					{
						// This match started in another element
						// Remove it and proceed with next partial match.

						RemovePartialMatch(match_i);

						force_match_continue = TRUE;
						break;
					}
				}
				else if (uni_isspace(content[i]) && text[characters_done - 1] == ' ')
				{
					// If latest match was a whitespace and current
					// character is a whitespace then match it.

					// if the matched space is a leading space we need to
					// adjust the start point of the next search.
					if (characters_done <= m_leading_spaces)
					{
						// the match started in a previous element and
						// we can advance the match one position so we
						// drop this match and proceed with the next
						if (characters_done > i)
						{
							RemovePartialMatch(match_i);
							force_match_continue = TRUE;
							break;
						}

						start_char_offset++;
					}
				}
			}

			i++;
		}

		if (force_match_continue)
			continue;

		// We have matched the whole search string
		if (!text[characters_done])
		{
			if (m_data->GetMatchWords())
			{
				if (i == 0)
				{
					if (m_last_word_start == NULL
						|| uni_isspace(content[0])
						|| Unicode::IsCSSPunctuation(content[0]))
					{
						finished = TRUE;
					}
					else
					{
						OP_ASSERT(text[0]);
						OP_ASSERT(m_matches[match_i].GetElm());
						OP_ASSERT(m_matches[match_i].GetElm() != txt_elm);

						// This match started in another word.
						// Remove it and proceed with next partial match.

						RemovePartialMatch(match_i);
						if (m_end_pending_word_finished.GetElm())
							m_end_pending_word_finished.Reset();

						continue;
					}
				}
				// if we stopped at the end of this element we need to
				// check if the word ends here or continues in the next
				// element.
				else if (content[i] == '\0')
				{
					BOOL should_activate = m_next_is_active
						&& (txt_elm != m_active || m_active_offset <= start_char_offset);

					m_end_pending_word_finished.Set(txt_elm, i, characters_done, should_activate);
				}
				else if (!uni_isspace(content[i]) && !uni_ispunct(content[i]))
				{
					if (characters_done <= i
						&& (m_matches[match_i].GetElm() == txt_elm || m_matches[match_i].GetElm() == NULL))
					{
						// This match started in this word.
						// Restart with character number two in the match.

						start_word_idx = start_char_offset + 1;
						OP_ASSERT(m_matches[match_i].GetCharsDone() == 0);
					}
					else
					{
						// This match started in another word.
						// Remove it and proceed with next partial match.

						RemovePartialMatch(match_i);
					}

					if (m_end_pending_word_finished.GetElm())
						m_end_pending_word_finished.Reset();
					continue;
				}
				else
					finished = TRUE;
			}
			else
				finished = TRUE;
		}

		if (characters_done && !m_matches[match_i].GetElm())
		{
			BOOL should_activate = m_next_is_active && (txt_elm != m_active || m_active_offset <= start_char_offset);

			// Set start of partial match
			m_matches[match_i].Set(txt_elm, start_char_offset, characters_done, should_activate);

			if (m_matches[match_i].GetElm() == txt_elm)
				// Next check need to start at the character following start of this match.
				start_word_idx = start_char_offset + 1;
		}

		if (finished)
		{
			OP_ASSERT(match_i == 0);

			PartialMatch end_match;
			// Set end of match
			if (m_end_pending_word_finished.GetElm())
			{
				end_match = m_end_pending_word_finished;
				m_end_pending_word_finished.Reset();
			}
			else
				end_match.Set(txt_elm, i, characters_done, FALSE);

			RETURN_IF_MEMORY_ERROR(AddSearchHit(&m_matches[0], &end_match));

			RemovePartialMatch(0);

			finished = FALSE;
			characters_done = 0;
			continue;
		}

		m_matches[match_i].SetCharsDone(characters_done);

		match_i++;
	}

	return OpStatus::OK;
}

OP_STATUS
SearchHelper::EnterOrLeaveElement(HTML_Element *elm, BOOL entering)
{
	if (m_data->GetLinksOnly())
	{
		if (entering && elm->GetAnchor_HRef(m_logdoc->GetFramesDocument()))
			m_current_link = elm;
		else if (!entering && elm == m_current_link)
			m_current_link = NULL;
	}

	if (elm->GetNsType() == NS_HTML)
	{
		if ((elm->Type() == HE_BR && entering)
			|| !IsHtmlInlineElm(elm->Type(), m_logdoc->GetHLDocProfile())
			|| elm->IsFormElement())
		{
			// we have a matched word that needs terminating punctuation
			if (m_end_pending_word_finished.GetElm())
			{
				RETURN_IF_MEMORY_ERROR(AddSearchHit(&m_matches[0], &m_end_pending_word_finished));

				m_end_pending_word_finished.Reset();
				RemovePartialMatch(0);
			}

			// Reset all partial matches
			for (int i = 0; m_matches[i].GetCharsDone();)
			{
				// still valid after linebreak if next character is space
				// BR, can match space
				if (elm->Type() == HE_BR
					&& uni_isspace(*(m_data->GetSearchText() + m_matches[i].GetCharsDone())))
				{
					m_matches[i].SetCharsDone(m_matches[i].GetCharsDone() + 1);
					i++;
					continue;
				}
				else
					RemovePartialMatch(i);
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS SearchHelper::FindAllMatches(HTML_Element *current_highlight_element, int current_offset)
{
	HTML_Element *iter = m_logdoc->GetRoot();

	if (!iter || !iter->GetLayoutBox() || !m_logdoc->GetFramesDocument())
		return OpStatus::OK;

	// Loop through all elements and find matching text
	BOOL children_done = FALSE;

	m_last_word_start = NULL;
	m_active_offset = current_offset;
	m_active = NULL;

	while (iter)
	{
		// Enter the element
		if (!iter->GetIsFirstLetterPseudoElement())
		{
			if (!children_done)
			{
				if (current_highlight_element == iter)
				{
					m_next_is_active = TRUE;
					m_active = current_highlight_element;
					current_highlight_element = NULL; // reset so we don't match again
				}

				EnterOrLeaveElement(iter, TRUE);

				// Traverse content
				if (iter->FirstChild())
				{
					iter = iter->FirstChild();
					continue;
				}
			}

			// Process the element if it seems to be one that is displayed to the user. We
			// ignore search hits in obviously invisible elements.
			if ((iter->GetLayoutBox() 
#ifdef SVG_SUPPORT
				|| (iter->Type() == HE_TEXT && iter->Parent() && iter->Parent()->GetNsType() == NS_SVG 
					&& g_svg_manager->IsVisible(iter->Parent()))
#endif // SVG_SUPPORT
				)
				&& (!m_data->GetLinksOnly() || m_current_link))
			{
				RETURN_IF_MEMORY_ERROR(SearchElement(iter));
			}
		}

		// Leave the element
		EnterOrLeaveElement(iter, FALSE);

		HTML_Element *candidate = iter->Suc();
		if (!candidate)
		{
			candidate = iter->Parent();
			children_done = TRUE;
		}
		else
			children_done = FALSE;

		iter = candidate;
	}

	m_active = NULL; // just to make sure we don't have refrences to deleted elements

	return OpStatus::OK;
}

OP_STATUS
SearchHelper::AddSearchHit(const PartialMatch *start, const PartialMatch *end)
{
	SelectionBoundaryPoint start_point;
	SelectionBoundaryPoint end_point;
	int start_offset = start->GetOffset();
	int end_offset = end->GetOffset();
	HTML_Element *start_elm = start->GetElm();
	HTML_Element *end_elm = end->GetElm();

	// This isn't correct conversion from PartialMatch to SelectionBoundaryPoint
	// (PartialMatch treats form elements in a special way, SelectionBoundaryPoint
	// does not). HTML_Document::UpdateSearchHit deals with it but not all 
	// selection code does.
	// See CORE-47127
	start_point.SetLogicalPosition(start_elm, start_offset);
	end_point.SetLogicalPosition(end_elm, end_offset);

	BOOL activate = FALSE;
	if (m_next_is_active && start->GetShouldActivate())
	{
		m_next_is_active = FALSE;
		activate = TRUE;
	}

	return m_logdoc->GetFramesDocument()->GetHtmlDocument()->AddSearchHit(&start_point, &end_point, activate);
}

#endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
