/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/elementref.h"
#ifndef HTML5_STANDALONE
#include "modules/logdoc/htm_elm.h"
#endif

void ElementRef::SetElm(HTML5ELEMENT *elm)
{
	if (elm != m_elm)
	{
		if (m_elm)
			Unreference();

		if (elm)
		{
			if (elm->m_first_ref)
			{
				m_suc = elm->m_first_ref;
				elm->m_first_ref->m_pred = this;
			}
			elm->m_first_ref = this;
		}
		
		m_elm = elm;
	}
}

// Moved this here to avoid including htm_elm.h which lead
// to a circular inclusion problem.
void ElementRef::AdvanceFirstRef()
{
	m_elm->m_first_ref = m_suc;
}

void KeepAliveElementRef::DeleteElementIfNecessary(HTML_Element* elm)
{
	if (m_free_element_on_delete && elm)
	{
#ifndef HTML5_STANDALONE
		HTML_Element::DocumentContext ctx(static_cast<FramesDocument*>(NULL));
		if (elm->Clean(ctx))
			if (elm->OutSafe(ctx))
				elm->Free();
#else
		if (elm->Clean())
			elm->Free();
#endif // HTML5_STANDALONE
	}
}

void KeepAliveElementRef::Unreference()
{
	HTML5ELEMENT *elm = GetElm();

	UnlinkFromList();
	DeleteElementIfNecessary(elm);

	m_free_element_on_delete = FALSE; // we've now done that task, reset status
}

void KeepAliveElementRef::OnDelete(FramesDocument*)
{
	m_free_element_on_delete = TRUE;
}

void KeepAliveElementRef::OnInsert(FramesDocument*, FramesDocument*)
{
	// Someone else now owns the element, so don't free it!
	m_free_element_on_delete = FALSE;
}
