/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef LAYOUT_CARET_SUPPORT

#include "modules/layout/wordinfoiterator.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/box/box.h"


WordInfoIterator::WordInfoIterator(FramesDocument* doc, HTML_Element* helm, SurroundingsInformation* surroundings) :
	m_doc(doc), m_helm(helm), m_surroundings(surroundings),
	m_word_idx(0), m_char_ofs(0), m_word_count(0), m_words(0), m_full_length(0),
	m_full_txt(NULL), m_visible_char_bits(NULL), m_un_col_count(0),
	m_first_un_col_ofs(-1), m_last_un_col_ofs(-1), m_has_uncollapsed_word(FALSE),
	m_pre_formatted(MAYBE)
{
}

WordInfoIterator::~WordInfoIterator()
{
	if (m_visible_char_bits != m_visible_char_stack_bits)
		OP_DELETEA(m_visible_char_bits);
}

OP_STATUS WordInfoIterator::Init()
{
	if (!m_doc || !m_surroundings || !m_helm || m_helm->Type() != HE_TEXT || (m_helm->GetTextContentLength() && !m_helm->TextContent()))
	{
		return OpStatus::ERR;
	}

	if (m_helm->GetLayoutBox() && m_helm->GetLayoutBox()->IsTextBox())
	{
		Text_Box *box = (Text_Box*)m_helm->GetLayoutBox();
		if (box->GetWords() && box->GetWordCount())
		{
			m_words = box->GetWords();
			m_word_count = box->GetWordCount();
		}
	}
	m_full_length = m_helm->GetTextContentLength();
	m_full_txt = m_helm->TextContent();
	OP_STATUS status = CalculateVisibilityBits();
	if (OpStatus::IsError(status))
	{
		m_words = NULL;
		m_word_count = 0;
	}
	return status;
}

int WordInfoIterator::ExtraLength(WordInfo *w)
{
	if (!w || static_cast<int>(w->GetStart() + w->GetLength()) >= m_full_length)
		return 0;
	if (w->IsTabCharacter())
		return 1;
	if (IsPreFormatted())
		return 0;
	return w->HasTrailingWhitespace() ? 1 : 0;
}

BOOL WordInfoIterator::ComputeIsInPreFormatted(HTML_Element *helm)
{
	Head prop_list;
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	LayoutProperties* lprops = LayoutProperties::CreateCascade(helm, prop_list, hld_profile);
	BOOL in_pre = lprops && (lprops->GetProps()->white_space == CSS_VALUE_pre || lprops->GetProps()->white_space == CSS_VALUE_pre_wrap);
	prop_list.Clear();
	return in_pre;
}

BOOL WordInfoIterator::IsPreFormatted()
{
	if (m_pre_formatted == MAYBE)
		m_pre_formatted = ComputeIsInPreFormatted(m_helm) ? YES : NO;
	return m_pre_formatted == YES;
}

BOOL WordInfoIterator::CollapsedToUnCollapsedOfs(int col_ofs, int &res_un_col_ofs)
{
	int i, count;
	if (col_ofs < 0 || col_ofs >= m_full_length || IsCharCollapsed(col_ofs))
		return FALSE;
	for (i = 0, count = 0; i < col_ofs; i++)
		if (!IsCharCollapsed(i))
			count++;
	res_un_col_ofs = count;
	return TRUE;
}

OP_STATUS WordInfoIterator::CalculateVisibilityBits()
{
	int i, j, bit;
	WordInfo *w;
	int alloc_units = m_full_length/WI_BPU + (m_full_length%WI_BPU != 0);
	m_un_col_count = 0;
	if (m_full_length > (int)WI_STACK_BITS)
	{
		m_visible_char_bits = OP_NEWA(UINT32, alloc_units);
		if (!m_visible_char_bits)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
		m_visible_char_bits = m_visible_char_stack_bits;
	op_memset(m_visible_char_bits, 0, alloc_units * sizeof(UINT32));
	if (!m_word_count)
		return OpStatus::OK;
	if (!m_full_length)
	{
		for (i = 0;i < m_word_count; i++)
			m_has_uncollapsed_word = m_has_uncollapsed_word || !GetWordAt(i)->IsCollapsed();
		return OpStatus::OK;
	}
	// Let's assume there are no zero-length non-collapsed words if m_full_length > 0...!?
	int first_with_len = -1, last_with_len = -1;
	int first_no_col = -1, after_no_col = -1;
	for (i = 0; i < m_word_count; i++)
	{
		w = GetWordAt(i);
		if (!w->IsCollapsed() && w->GetLength() + ExtraLength(w))
		{
			if (w->GetLength())
			{
				if (first_with_len < 0)
					first_with_len = i;
				last_with_len = i;
				after_no_col = -1;
			}
			else
			{
				if (first_no_col < 0 && first_with_len < 0)
					first_no_col = i;
				if (after_no_col < 0)
					after_no_col = i;
			}
		}
	}
	if (first_with_len < 0 && first_no_col < 0)
		return OpStatus::OK;
	m_has_uncollapsed_word = TRUE;
	int start_set = 0, stop_set = m_word_count - 1;
	if ((first_no_col >= 0 || after_no_col >= 0) && !IsPreFormatted())
	{
		if (first_no_col >= 0 && !m_surroundings->HasWsPreservingElmBeside(m_helm, TRUE))
		{
			if (first_with_len < 0) // we have only collapsing spaces
				return OpStatus::OK;
			start_set = first_with_len;
		}
		if (after_no_col >= 0 && !m_surroundings->HasWsPreservingElmBeside(m_helm, FALSE))
		{
			if (first_with_len < 0) // we have only collapsing spaces
				return OpStatus::OK;
			stop_set = last_with_len;
		}
	}
	OP_ASSERT(start_set >= 0 && stop_set >= 0 && start_set <= stop_set);
	for (i = start_set; i <= stop_set; i++)
	{
		w = GetWordAt(i);
		if (!w->IsCollapsed())
		{
			int len = w->GetLength() + ExtraLength(w);
			if (len && m_first_un_col_ofs < 0)
				m_first_un_col_ofs = w->GetStart();
			for (j = 0, bit = w->GetStart(); j < len; j++, bit++)
			{
				// This will lead to corrupted memory, this happened for instance in CORE-29493.
				OP_ASSERT(m_visible_char_bits != m_visible_char_stack_bits || bit / WI_BPU < WI_STACK_DWORDS);

				m_visible_char_bits[bit/WI_BPU] |= 1 << (bit%WI_BPU);
				m_un_col_count++;
				m_last_un_col_ofs = bit;
			}
		}
	}
	return OpStatus::OK;
}

BOOL WordInfoIterator::GetOfsSnappedToUnCollapsed(int ofs, int &res_ofs, BOOL snap_forward)
{
	if (!HasUnCollapsedChar() || (ofs < FirstUnCollapsedOfs() && !snap_forward) || (ofs > LastUnCollapsedOfs() && snap_forward))
		return FALSE;
	ofs = MIN(MAX(ofs, FirstUnCollapsedOfs()), LastUnCollapsedOfs());
	BOOL preformatted = IsPreFormatted();
	while ((!preformatted && IsCharCollapsed(ofs)) || IsCharInSurrogatePair(ofs))
		ofs += snap_forward ? 1 : -1;
	res_ofs = ofs;
	return TRUE;
}

BOOL WordInfoIterator::GetUnCollapsedOfsFrom(int ofs, int &res_ofs, BOOL forward)
{
	if (!GetOfsSnappedToUnCollapsed(ofs, ofs, forward))
		return FALSE;
	ofs += forward ? 1 : -1;
	if (!GetOfsSnappedToUnCollapsed(ofs, ofs, forward))
		return FALSE;
	res_ofs = ofs;
	return TRUE;
}

WordInfo* WordInfoIterator::GetWordAt(int index)
{
	if (m_words && index >= 0 && index < m_word_count)
		return &m_words[index];
	return NULL;
}

BOOL WordInfoIterator::IsWordCollapsed(int index)
{
	if (index >= 0 && index < m_word_count)
		return m_words[index].IsCollapsed();
	return TRUE;
}

#endif // LAYOUT_CARET_SUPPORT
