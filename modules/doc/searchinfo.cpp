/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL

#include "modules/doc/searchinfo.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"


SelectionElm::SelectionElm(FramesDocument *frm_doc, const SelectionBoundaryPoint *start, const SelectionBoundaryPoint *end)
{
	m_selection.SetNewSelection(frm_doc, start, end, FALSE, FALSE, TRUE);
}

SelectionElm::~SelectionElm()
{
}

OP_STATUS
SearchData::SetSearchText(const uni_char *txt)
{
	if (!m_text.IsEmpty())
	{
		if (m_text.Compare(txt) == 0)
			return OpStatus::OK;

		m_match_doc = NULL;
		m_new_search = TRUE;
	}

	return m_text.Set(txt);
}

const uni_char*
SearchData::GetSearchText()
{
	return m_text.CStr();
}

void
SearchData::SetMatchCase(BOOL val)
{
	if (m_match_case != val)
	{
		m_match_case = val;
		m_match_doc = NULL;
		m_new_search = TRUE;
	}
}

void
SearchData::SetMatchWords(BOOL val)
{
	if (m_match_words != val)
	{
		m_match_words = val;
		m_match_doc = NULL;
		m_new_search = TRUE;
	}
}

void
SearchData::SetLinksOnly(BOOL val)
{
	if (m_links_only != val)
	{
		m_links_only = val;
		m_match_doc = NULL;
		m_new_search = TRUE;
	}
}

# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
