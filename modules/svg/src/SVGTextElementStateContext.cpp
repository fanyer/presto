/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/unicode/unicode_bidiutils.h"
#include "modules/layout/box/box.h"

void SVGTextNode::AddInvalidState(SVGElementInvalidState state)
{
	if (state >= INVALID_STRUCTURE)
	{
		m_textcache.RemoveCachedInfo();
	}
}

void SVGTRefElement::AddInvalidState(SVGElementInvalidState state)
{
	if (state >= INVALID_STRUCTURE)
	{
		m_textcache.RemoveCachedInfo();
	}
}

void SVGSwitchElement::AddInvalidState(SVGElementInvalidState state)
{
	SVGContainer::AddInvalidState(state);

	// A structural change in a <switch> means we need to re-evaluate
	// its children, so upgrade invalid state to INVALID_ADDED.
	if (state >= INVALID_STRUCTURE)
		UpgradeInvalidState(INVALID_ADDED);
}

#ifdef SUPPORT_TEXT_DIRECTION
#ifdef _DEBUG
void SVGTextRootContainer::PrintDebugInfo()
{
	OP_NEW_DBG("SVGTextRootContainer", "svg_bidi");
	OP_DBG(("bidisegments count: %d bidicalc: %p", m_bidi_segments.Cardinal(), m_bidi_calculation));
}
#endif // _DEBUG

OP_STATUS SVGTextRootContainer::ResolveBidiSegments(WordInfo* wordinfo_array, int wordinfo_array_len, SVGTextFragment*& tflist)
{
	OP_NEW_DBG("SVGTextRootContainer::ResolveBidiSegment", "svg_bidi");

	int bidi_segment_count = wordinfo_array_len;
	tflist = OP_NEWA(SVGTextFragment, bidi_segment_count);
	if (!tflist)
		return OpStatus::ERR_NO_MEMORY;

	ParagraphBidiSegment* segment = (ParagraphBidiSegment*) m_bidi_segments.First();

	int i, num_levels = 0;
	for (i = 0; i < bidi_segment_count; i++)
	{
		tflist[i].idx.wordinfo = i;
		tflist[i].idx.next = i;

		if(segment && segment->Suc() && (int)wordinfo_array[i].GetStart() >= ((ParagraphBidiSegment*)segment->Suc())->virtual_position)
		{
			segment = (ParagraphBidiSegment*)segment->Suc();
		}

		if (segment)
		{
			tflist[i].packed.level = segment->level;
			tflist[i].packed.bidi = segment->level % 2 == 1 ? BIDI_FRAGMENT_MIRRORED : BIDI_FRAGMENT_NORMAL;
		}
		else
		{
			// Crashfix. We get no segments in some cases (when they are not needed?)
			tflist[i].packed.level = 0;
			tflist[i].packed.bidi = BIDI_FRAGMENT_NORMAL;
		}
	}

	m_bidi_segments.Clear();

#ifdef _DEBUG
	for (int i = 0;i < bidi_segment_count;i++)
		OP_DBG(("Before reorder: i: %d start: %d length: %d, left_to_right: %d nextidx: %d",i, wordinfo_array[tflist[i].idx.wordinfo].GetStart(), wordinfo_array[tflist[i].idx.wordinfo].GetLength() + (wordinfo_array[tflist[i].idx.wordinfo].HasTrailingWhitespace() ? 1 : 0), 
			   tflist[i].packed.bidi == BIDI_FRAGMENT_MIRRORED ? 0 : 1, tflist[i].idx.next));
#endif // _DEBUG

	for(i = 0; i < bidi_segment_count; i++)
		num_levels = MAX(num_levels, tflist[i].packed.level);

	// Turn around all fragments for each level
	for(int k = num_levels; k >= 1; k--)
	{
		for(int frag = 0; frag < bidi_segment_count; )
		{
			int i = frag;
			if (tflist[i].packed.level >= k)
			{
				int count = 0;
				while(i + count < bidi_segment_count && tflist[i + count].packed.level >= k)
					count++;
				for(int j = 0; j < count/2; j++)
				{
					int idx1 = i + j;
					int idx2 = i - j + count - 1;
					int tmp = tflist[idx1].idx.next;
					tflist[idx1].idx.next = tflist[idx2].idx.next;
					tflist[idx2].idx.next = tmp;
				}
				frag += count;
			}
			else
				frag++;
		}
	}

#ifdef _DEBUG
	for (int i = 0;i < bidi_segment_count;i++)
		OP_DBG(("i: %d start: %d length: %d, left_to_right: %d nextidx: %d",i, wordinfo_array[tflist[i].idx.wordinfo].GetStart(), wordinfo_array[tflist[i].idx.wordinfo].GetLength() + (wordinfo_array[tflist[i].idx.wordinfo].HasTrailingWhitespace() ? 1 : 0), 
			   tflist[i].packed.bidi == BIDI_FRAGMENT_MIRRORED ? 0 : 1, tflist[i].idx.next));
#endif // _DEBUG

	return OpStatus::OK;
}

/** Push new inline bidi properties onto stack. */

// ed: these should maybe go in SVGTextContainer instead, since direction applies to 'text content elements'
BOOL SVGTextRootContainer::PushBidiProperties(const HTMLayoutProperties* props)
{
	OP_NEW_DBG("PushBidiProperties", "svg_bidi");
	OP_DBG(("PushBidi: %s bidi-override: %d",
			props->direction == CSS_VALUE_ltr ? "LTR" : "RTL",
			props->unicode_bidi == CSS_VALUE_bidi_override ? 1 : 0
			));

	if (!m_bidi_calculation)
		//if (!ReconstructBidiStatus(info))
		if (!InitBidiCalculation(props))
			return FALSE;

	if (m_bidi_calculation->PushLevel(props->direction, props->unicode_bidi == CSS_VALUE_bidi_override) == OpStatus::ERR_NO_MEMORY)
		return FALSE;

	return TRUE;
}

/** Pop inline bidi properties from stack. */

BOOL SVGTextRootContainer::PopBidiProperties()
{
	OP_NEW_DBG("PopBidiProperties", "svg_bidi");

	if (m_bidi_calculation)
	{
		OP_DBG(("Popped"));
		if (m_bidi_calculation->PopLevel() == OpStatus::ERR_NO_MEMORY)
			return FALSE;
	}

	return TRUE;
}

#endif // SUPPORT_TEXT_DIRECTION

#if defined(SVG_SUPPORT_EDITABLE) || defined(SUPPORT_TEXT_DIRECTION)
SVGTextRootContainer::~SVGTextRootContainer()
{
#ifdef SVG_SUPPORT_EDITABLE
	OP_DELETE(m_edit_context);
#endif // SVG_SUPPORT_EDITABLE
#ifdef SUPPORT_TEXT_DIRECTION
	OP_DELETE(m_bidi_calculation);
#endif // SUPPORT_TEXT_DIRECTION
}
#endif // SVG_SUPPORT_EDITABLE || SUPPORT_TEXT_DIRECTION

#ifdef SVG_SUPPORT_EDITABLE
SVGEditable* SVGTextRootContainer::GetEditable(BOOL construct)
{
	if (!m_edit_context && construct)
		OpStatus::Ignore(SVGEditable::Construct(&m_edit_context, GetHtmlElement()));

	return m_edit_context;
}

BOOL SVGTextRootContainer::IsEditing()
{
	return m_edit_context && m_edit_context->IsFocused();
}

BOOL SVGTextRootContainer::IsCaretVisible()
{
	return m_edit_context && m_edit_context->m_caret.m_on;
}
#endif // SVG_SUPPORT_EDITABLE

#ifdef SUPPORT_TEXT_DIRECTION

/** Initialise bidi calculation. */

BOOL SVGTextRootContainer::InitBidiCalculation(const HTMLayoutProperties* props)
{
	m_bidi_segments.Clear();

	OP_ASSERT(!m_bidi_calculation);
	m_bidi_calculation = OP_NEW(BidiCalculation, ());

	if (!m_bidi_calculation)
		return FALSE;

	m_bidi_calculation->Reset();
	m_bidi_calculation->SetSegments(&m_bidi_segments);

	OP_NEW_DBG("InitBidiCalculation", "svg_bidi");
	OP_DBG(("SetParagraphLevel: %s bidi-override: %d",
			!props || props->direction == CSS_VALUE_ltr ? "LTR" : "RTL",
			props && props->unicode_bidi == CSS_VALUE_bidi_override ? 1 : 0
			));

	if (props)
		m_bidi_calculation->SetParagraphLevel((props->direction == CSS_VALUE_rtl) ? BIDI_R : BIDI_L,
														  props->unicode_bidi == CSS_VALUE_bidi_override);
	else
		m_bidi_calculation->SetParagraphLevel(BIDI_L, FALSE);

	return TRUE;
}

void SVGTextRootContainer::ResetBidi()
{
	m_bidi_segments.Clear();
	OP_DELETE(m_bidi_calculation);
	m_bidi_calculation = NULL;
}
#endif // SUPPORT_TEXT_DIRECTION

/** Allocate word_info_array, and update word_count. Precondition: empty array. */

BOOL SVGTextCache::AllocateWordInfoArray(unsigned int new_word_count)
{
	OP_ASSERT(!word_info_array && !packed.word_count);

	word_info_array = OP_NEWA(WordInfo, new_word_count);

	if (!word_info_array && new_word_count)
		return FALSE;

	packed.word_count = new_word_count;

	REPORT_MEMMAN_INC(packed.word_count * sizeof(WordInfo));

	return TRUE;
}

/** Delete word_info_array, and update word_count. */

void SVGTextCache::DeleteWordInfoArray()
{
	OP_NEW_DBG("DeleteWordInfoArray", "svg_textcache");
	OP_DBG(("Deleted wordinfoarray: %p tflist: %p",
			word_info_array,
			text_fragment_list));
	OP_DELETEA(word_info_array);
	word_info_array = NULL;
	OP_DELETEA(text_fragment_list);
	text_fragment_list = NULL;

	if (packed.word_count)
	{
		REPORT_MEMMAN_DEC(packed.word_count * sizeof(WordInfo));

		packed.word_count = 0;
	}
}

/** Remove cached info */

BOOL SVGTextCache::RemoveCachedInfo()
{
	OP_NEW_DBG("RemoveCachedInfo", "svg_textcache");
	OP_DBG(("Removed cached info."));
	packed.remove_word_info = 1;

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	DeleteWordInfoArray();
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	return TRUE;
}

#endif // SVG_SUPPORT
