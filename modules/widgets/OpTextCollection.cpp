/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/widgets/OpEditCommon.h"
#include "modules/widgets/OpTextCollection.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/optextfragment.h"
#include "modules/unicode/unicode_stringiterator.h"

OpTextCollection::OpTextCollection(OpTCListener* listener)
	: caret_snap_forward(TRUE)
	, line_height(0)
	, visible_width(0)
	, visible_height(0)
	, m_layout_width(-1)
	, total_width(0)
	, max_block_width(0)
	, total_height(0)
	, total_width_old(0)
	, total_height_old(0)
	, autodetect_direction(FALSE)
	, listener(listener)
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	, m_spell_change_count(0)
	, m_delayed_spell(FALSE)
	, m_has_spellchecked(FALSE)
	, m_has_pending_spellcheck(FALSE)
	, m_iterators(NULL)
	, m_spell_session(NULL)
	, m_pending_spell_first(NULL)
	, m_pending_spell_last(NULL)
	, m_by_user(FALSE)
#endif // INTERNAL_SPELLCHECK_SUPPORT
{
}

OpTextCollection::~OpTextCollection()
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
#endif // INTERNAL_SPELLCHECK_SUPPORT
	blocks.Clear();
}

OpTCBlock* OpTextCollection::FindBlockAtPoint(const OpPoint& point)
{
	OpTCBlock* block = FirstBlock();
	while (block)
	{
		if (point.y >= block->y && point.y < block->y + block->block_height)
			return block;
		block = (OpTCBlock*) block->Suc();
	}
	return NULL;
}

OpTCOffset OpTextCollection::PointToOffset(const OpPoint& cpoint, BOOL* snap_forward, BOOL bounds_check_cpoint_y/* = TRUE*/)
{
	OpPoint point = cpoint;
	if (bounds_check_cpoint_y && LastBlock())
	{
		point.y = MIN(point.y, LastBlock()->y + LastBlock()->block_height - 1);
		point.y = MAX(point.y, FirstBlock()->y);
	}

	OpTCBlock* block = FindBlockAtPoint(point);
	if (!block)
		return OpTCOffset();

	OP_TCINFO* info = listener->TCGetInfo();
	PointToOffsetTraverser traverser(OpPoint(point.x, point.y - block->y));
	block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromY(info, point.y));

	if (!traverser.ofs.block)
	{
		// Should not happen but sometimes the layoutengine gives us a different fontsize when layouting than painting
		// which results in the traversecode not finding the correct block.
		traverser.ofs.block = block;
		traverser.ofs.ofs = block->text_len;
	}

	if (snap_forward)
		*snap_forward = traverser.snap_forward;
	return traverser.ofs;
}

void OpTextCollection::OnFocus(BOOL focus)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(!focus && m_delayed_spell && m_spell_session && caret.block)
	{
		m_delayed_spell = FALSE;
		OnBlocksInvalidated(caret.block, caret.block);
		RunPendingSpellcheck();
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

BOOL OpTextCollection::SetCaretPos(const OpPoint& point, BOOL bounds_check_point_y/* = TRUE*/)
{
	OpTCOffset new_ofs = PointToOffset(point, &caret_snap_forward, bounds_check_point_y);
	if (!new_ofs.block)
		return FALSE;

	SetCaret(new_ofs);
	return TRUE;
}

void OpTextCollection::SelectAll()
{
	OP_TCINFO* info = listener->TCGetInfo();
	if (!info->selectable || FirstBlock() == NULL)
		return;

	sel_start.block = FirstBlock();
	sel_start.ofs = 0;
	sel_stop.block = LastBlock();
	sel_stop.ofs = sel_stop.block ? sel_stop.block->text.Length() : 0;
	InvalidateBlocks(sel_start.block, sel_stop.block);

	SetCaret(sel_stop);

	listener->TCInvalidateAll();
}

void OpTextCollection::SelectNothing(BOOL invalidate)
{
	if (invalidate)
		InvalidateBlocks(sel_start.block, sel_stop.block);
	sel_start.Clear();
	sel_stop.Clear();
}

BOOL OpTextCollection::SetVisibleSize(INT32 width, INT32 height)
{
	INT32 old_visible_width = visible_width;
	visible_width = MAX(width, 0);
	visible_height = MAX(height, 0);
	return visible_width != old_visible_width;
}

BOOL OpTextCollection::SetLayoutWidth(INT32 width)
{
	INT32 old = m_layout_width;
	m_layout_width = width;
	return old != width;
}

OP_STATUS OpTextCollection::SetText(const uni_char* text, INT32 text_len, BOOL use_undo_stack)
{
	InvalidateBlocks(FirstBlock(), LastBlock());
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OnBlocksInvalidated(FirstBlock(), LastBlock());
	OP_ASSERT(!m_iterators || !m_iterators->word_iterator.Active());
	OP_ASSERT(!m_has_pending_spellcheck || (!m_pending_spell_first && !m_pending_spell_last));
#endif // INTERNAL_SPELLCHECK_SUPPORT

	// First, clear everything.
	blocks.Clear();
	caret.Clear();
	sel_start.Clear();
	sel_stop.Clear();
	total_width = 0;
	max_block_width = 0;
	total_height = 0;

	if(text_len > 0)
	{
		RETURN_IF_ERROR(InsertText(text, text_len, use_undo_stack));
	}
	else
	{
		BeginChange();
		OpTCBlock* block = OP_NEW(OpTCBlock, ());

		if (!block)
			return OpStatus::ERR_NO_MEMORY;
		block->Into(&blocks);
		// Make sure stuff are updated by calling UpdateAndLayout.
		// (Like f.ex. total_width which might be nonzero even with empty text if JUSTIFY_RIGHT)
		block->UpdateAndLayout(listener->TCGetInfo(), TRUE);
		EndChange();

		SetCaretOfsGlobal(0);
	}

	return OpStatus::OK;
}

INT32 OpTextCollection::GetTextLength(BOOL insert_linebreak) const
{
	INT32 text_len = 0;
	OP_TCINFO* info = listener->TCGetInfo();
	OpTCBlock* block = FirstBlock();
	while (block)
	{
		GetTextTraverser traverser(NULL, insert_linebreak, 100000000, 0);
		block->Traverse(info, &traverser, FALSE, FALSE, 0);
		text_len += traverser.len;
		block = (OpTCBlock*) block->Suc();
	}
	return text_len;
}

INT32 OpTextCollection::GetText(uni_char* buf, INT32 max_len, INT32 offset, BOOL insert_linebreak) const
{
	INT32 text_len = 0;
	OP_TCINFO* info = listener->TCGetInfo();
	OpTCBlock* block = FirstBlock();
	while (block && max_len > 0)
	{
		GetTextTraverser traverser(&buf[text_len], insert_linebreak, max_len, offset);
		block->Traverse(info, &traverser, FALSE, FALSE, 0);
		text_len += traverser.len;
		max_len -= traverser.len;
		block = (OpTCBlock*) block->Suc();
	}
	return text_len;
}

// utility - compares str1 to str2, increases str2, and updates max_len as necessary
static inline
INT32 str_comp(UINT32& len, INT32& max_len, const uni_char* str1, const uni_char*& str2)
{
	if (max_len > 0)
	{
		if (len > (UINT32)max_len)
			len = max_len;
		max_len -= len;
	}
	const INT32 r = uni_strncmp(str1, str2, len);
	str2 += len;
	return r;
}
INT32 OpTextCollection::CompareText(const uni_char* str, INT32 max_len/* = -1*/, UINT32 offset/* = 0*/) const
{
	// early bail
	if (!max_len)
		return 0;
	// signal difference - not exactly compatible with uni_strcmp since that would crash
	if (!str)
	{
		OP_ASSERT(!"CompareText shouldn't be called with str == NULL");
		return 1;
	}

	// compare one block at the time - no need to create a traversal object
	for (OpTCBlock* block = FirstBlock(); block; block = (OpTCBlock*)block->Suc())
	{
		// should be -1 or maximum number of characters left to read, never 0
		OP_ASSERT(max_len);

		UINT32 len = block->text_len;

		// this block falls completely before starting point
		if (offset >= len)
		{
			offset -= len;

			// this is not the last block, which means implicit linebreak
			if (block->Suc())
			{
				if (offset >= 2)
					offset -= 2;
				else
				{
					// compare to linebreak
					const uni_char* br = UNI_L("\r\n");
					br += offset;
					UINT32 len = 2 - offset;
					offset = 0;
					INT32 r = str_comp(len, max_len, br, str);
					if (r || !max_len)
						return r;
				}
			}
			continue;
		}

		UINT32 s_offs = 0;
		// this block falls partly before starting point
		if (offset)
		{
			OP_ASSERT(offset < len);
			len -= offset;
			s_offs += offset;
			offset = 0;
		}

		int r = str_comp(len, max_len, block->text.CStr() + s_offs, str);
		if (r || !max_len)
			return r;

		// this is not the last block, which means implicit linebreak
		if (block->Suc())
		{
			// compare to linebreak
			const uni_char* br = UNI_L("\r\n");
			UINT32 len = 2;
			int r = str_comp(len, max_len, br, str);
			if (r || !max_len)
				return r;
		}
	}
	return offset ? 0 : -*str;
}

// == Updating/Invalidating =============================================

void OpTextCollection::BeginChange(BOOL invalidate_spell_data)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(invalidate_spell_data)
	{
		if(!m_spell_change_count)
		{ // The blocks where the change will take place should be spellchecked (after returning to the message loop)
			if(HasSelectedText())
				OnBlocksInvalidated(sel_start.block, sel_stop.block);
			else if (caret.block)
				OnBlocksInvalidated(caret.block, caret.block);
		}
		m_spell_change_count++;
	}

#endif // INTERNAL_SPELLCHECK_SUPPORT
	total_width_old = total_width;
	total_height_old = total_height;
}

void OpTextCollection::EndChange(BOOL spell_data_invalidated)
{
	// Update totalheight and scrollbars
/*	total_height = 0;
	if (LastBlock())
		total_height = LastBlock()->y + LastBlock()->block_height;*/

	if (total_width != total_width_old ||
		total_height != total_height_old)
	{
		listener->TCOnContentResized();
		// Updating cached width and height so that
		// nested Begin-Begin-End-<changed size>-End works
		total_width_old = total_width;
		total_height_old = total_height;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (autodetect_direction)
	{
		OpTCBlock* block = FirstBlock();
		BOOL detected = FALSE;

		if (block)
		{
			for (const uni_char* c = block->CStr(); !detected && c && *c; c++)
			{
				switch (Unicode::GetBidiCategory(*c))
				{
					case BIDI_R:
					case BIDI_AL:
						listener->TCOnRTLDetected(TRUE);
						detected = TRUE;
						break;
					case BIDI_L:
						listener->TCOnRTLDetected(FALSE);
						detected = TRUE;
						break;
				}
			}
		}
	}
#endif // SUPPORT_TEXT_DIRECTION
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(spell_data_invalidated)
	{
		m_spell_change_count--;
		if(!m_spell_change_count)
			RunPendingSpellcheck(); // Will post a message to the spellchecker to spellcheck the blocks that has been changed
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpTextCollection::InvalidateBlock(OpTCBlock* block)
{
	OP_TCINFO* info = listener->TCGetInfo();
	int block_height = block->block_height;
	int visible_line_height = MAX(info->tc->line_height, (int)info->font_height);
	block_height -= line_height;
	block_height += visible_line_height;
	listener->TCInvalidate(OpRect(0, block->y, MAX(total_width, visible_width), block_height));
}

void OpTextCollection::InvalidateBlocks(OpTCBlock* first, OpTCBlock* last)
{
	while(first)
	{
		InvalidateBlock(first);
		if (first == last)
			break;
		first = (OpTCBlock*) first->Suc();
	}
}

void OpTextCollection::InvalidateCaret()
{
	OP_TCINFO* info = listener->TCGetInfo();
	int visible_line_height = MAX(info->tc->line_height, (int)info->font_height);
	listener->TCInvalidate(OpRect(caret_pos.x, caret_pos.y, MAX(3, g_op_ui_info->GetCaretWidth()), visible_line_height));
}

BOOL OpTextCollection::HasSelectedText()
{
	return sel_start.block && !(sel_start == sel_stop);
}

int GetTextBetween(OpTCOffset start, OpTCOffset stop, uni_char* buf)
{
	if (!start.block)
		return 0;

	if (start.block == stop.block)
	{
		int len = stop.ofs - start.ofs;
		if (buf)
			op_memcpy(buf, start.block->CStr(start.ofs), len * sizeof(uni_char));
		return len;
	}

	// First part of first block
	INT32 sel_len = start.block->text_len - start.ofs;
	if (buf)
		op_memcpy(buf, start.block->CStr(start.ofs), sel_len * sizeof(uni_char));
	if (buf)
		op_memcpy(&buf[sel_len], UNI_L("\r\n"), 2 * sizeof(uni_char));
	sel_len += 2;

	// All blocks between
	OpTCBlock* block = (OpTCBlock*) start.block->Suc();
	while (block != stop.block)
	{
		if (buf)
			op_memcpy(&buf[sel_len], block->CStr(), block->text_len * sizeof(uni_char));
		sel_len += block->text_len;
		if (buf)
			op_memcpy(&buf[sel_len], UNI_L("\r\n"), 2 * sizeof(uni_char));
		sel_len += 2;
		block = (OpTCBlock*) block->Suc();
	}

	// Last block
	int len = stop.ofs;
	if (buf)
		op_memcpy(&buf[sel_len], block->CStr(), len * sizeof(uni_char));
	sel_len += len;

	return sel_len;
}

INT32 OpTextCollection::GetSelectionTextLen()
{
	return GetTextBetween(sel_start, sel_stop, NULL);
}

uni_char* OpTextCollection::GetSelectionText()
{
	if (!sel_start.block)
		return 0;
	int len = GetSelectionTextLen();
	uni_char* text = OP_NEWA(uni_char, len + 1);
	if (text)
	{
		GetTextBetween(sel_start, sel_stop, text);
		text[len] = 0;
	}
	return text;
}


#ifdef WIDGETS_UNDO_REDO_SUPPORT
OP_STATUS OpTextCollection::SaveSelectionOnUndoStack()
{
	uni_char* text = GetSelectionText();
	if (text == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Add the string to UndoRedoStack.
	OP_TCINFO* info = listener->TCGetInfo();
	INT32 s1 = sel_start.GetGlobalOffset(info);
	INT32 s2 = sel_stop.GetGlobalOffset(info);
	OP_STATUS ret_val = undo_stack.SubmitRemove(s1, s1, s2, text);

	OP_DELETEA(text);
	return ret_val;
}
#endif // WIDGETS_UNDO_REDO_SUPPORT

OP_STATUS OpTextCollection::RemoveSelection(BOOL use_undo_stack, BOOL reformat_block)
{
	if (!HasSelectedText())
		return OpStatus::OK; // Nothing selected, return.

	listener->TCOnContentChanging();

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	// Save the content in the selection.
	if (use_undo_stack)
	{
		RETURN_IF_ERROR(SaveSelectionOnUndoStack());
	}
#endif // WIDGETS_UNDO_REDO_SUPPORT

	OP_TCINFO* info = listener->TCGetInfo();

	INT32 old_bottom = LastBlock()->y + LastBlock()->block_height;

	BeginChange(TRUE);
	OP_STATUS status;

	// Remove
	if (sel_start.block == sel_stop.block)
	{
		// Remove within the same block
		status = sel_start.block->RemoveText(sel_start.ofs, sel_stop.ofs - sel_start.ofs, info);
	}
	else
	{
		// Remove all block between
		OpTCBlock* block = (OpTCBlock*) sel_start.block->Suc();
		while (block != sel_stop.block)
		{
			OpTCBlock* suc = (OpTCBlock*) block->Suc();
			InvalidateBlock(block);
			OP_DELETE(block);
			block = suc;
		}

		// Remove last part
		sel_stop.block->RemoveText(0, sel_stop.ofs, info);

		// Remove first part
		sel_start.block->RemoveText(sel_start.ofs, sel_start.block->text_len - sel_start.ofs, info);

		status = sel_start.block->Merge(info);
	}

	if(OpStatus::IsError(status))
	{
		EndChange(TRUE);
		return status;
	}

	SetCaret(sel_start);
	SelectNothing(FALSE);

	// If we shrink, Update empty area below bottom.
	INT32 bottom = 0;
	if (LastBlock())
		bottom = LastBlock()->y + LastBlock()->block_height;
	if (bottom < old_bottom)
		listener->TCInvalidate(OpRect(0, bottom, visible_width, old_bottom - bottom));
	EndChange(TRUE);
	return OpStatus::OK;
}

void OpTextCollection::UpdateCaretPos()
{
	OP_TCINFO* info = listener->TCGetInfo();
	if (!info->vis_dev)
		return;

	InvalidateCaret();

	caret_pos = caret.GetPoint(info, caret_snap_forward);
	caret_pos.x = MIN(caret_pos.x, total_width - 1);
	caret_pos.x = MAX(caret_pos.x, 0);

	InvalidateCaret();
}

void OpTextCollection::SetCaret(OpTCOffset ofs)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OnCaretChange(caret, ofs);
#endif
	caret = ofs;
	UpdateCaretPos();
}

void OpTextCollection::SetCaretOfs(int ofs)
{
	OP_ASSERT(ofs >= 0 && ofs <= caret.block->text_len);
	SetCaret(OpTCOffset(caret.block, ofs));
}

void OpTextCollection::SetCaretOfsGlobal(int global_ofs)
{
	OpTCOffset new_caret = caret;
	new_caret.SetGlobalOffset(listener->TCGetInfo(), MAX(global_ofs, 0));
	SetCaret(new_caret);
}

void OpTextCollection::SetCaretOfsGlobalNormalized(int global_ofs)
{
	OpTCOffset new_caret = caret;
	new_caret.SetGlobalOffsetNormalized(listener->TCGetInfo(), MAX(global_ofs, 0));
	SetCaret(new_caret);
}

void OpTextCollection::MoveToNextWord(BOOL right, BOOL visual_order)
{
	if (!caret.block)
		return;
	caret_snap_forward = !right;

	INT32 step = right ? 1 : -1;
	step = SeekWord(caret.block->CStr(), caret.ofs, step, caret.block->text_len);

	if (step)
		SetCaretOfs(caret.ofs + step);
	else
		MoveCaret(right, visual_order);
}

void OpTextCollection::MoveToStartOrEndOfWord(BOOL right, BOOL visual_order)
{
	if (!caret.block)
		return;

	caret_snap_forward = !right;
	INT32 new_pos;

	if (right)
		new_pos = NextCharRegion(caret.block->CStr(), caret.ofs, caret.block->text_len);
	else
		new_pos = PrevCharRegion(caret.block->CStr(), caret.ofs, TRUE);

	if (new_pos != caret.ofs)
		SetCaretOfs(new_pos);
}

void OpTextCollection::MoveToStartOrEndOfLine(BOOL start, BOOL visual_order)
{
	if (!caret.block)
		return;

	OP_TCINFO* info = listener->TCGetInfo();
	LineFinderTraverser traverser(&caret, caret_snap_forward);
	caret.block->Traverse(info, &traverser, visual_order, FALSE, 0);

	OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_ofs;
	vis_ofs.offset = 0;
	if (start)
	{
		vis_ofs.idx = traverser.line_first_fragment_idx;
		if (traverser.line_num_fragments)
			vis_ofs.offset = caret.block->fragments.Get(traverser.line_first_fragment_idx)->start;
	}
	else
	{
		vis_ofs.idx = traverser.line_first_fragment_idx + traverser.line_num_fragments - 1;
		if (traverser.line_num_fragments)
		{
			vis_ofs.offset = caret.block->fragments.Get(traverser.line_first_fragment_idx)->start;
			for(int i = 0; i < traverser.line_num_fragments; i++)
			{
				OP_TEXT_FRAGMENT* frag = caret.block->fragments.GetByVisualOrder(traverser.line_first_fragment_idx + i);
				vis_ofs.offset += frag->wi.GetLength();
			}
		}
	}

	int new_ofs;
	if (visual_order)
	{
		OP_TEXT_FRAGMENT_LOGICAL_OFFSET log_ofs = caret.block->fragments.VisualToLogicalOffset(vis_ofs);
		new_ofs = log_ofs.offset;
		caret_snap_forward = log_ofs.snap_forward;
	}
	else
	{
		// vis_ofs is not really a visual offset. The code above that calculates vis_ofs works for both
		// logical and visual position. Just shouldn't convert it with VisualToLogicalOffset.
		new_ofs = vis_ofs.offset;
		caret_snap_forward = start;
	}
	SetCaret(OpTCOffset(caret.block, new_ofs));
}

void OpTextCollection::MoveCaret(BOOL forward, BOOL visual_order)
{
	if (!caret.block)
		return;

	OP_TCINFO* info = listener->TCGetInfo();
	if (!visual_order)
	{
		// Logical movement is simple. Just use the global offset.

		int gofs = caret.GetGlobalOffset(info);

		UnicodeStringIterator iter(caret.block->text.CStr(), caret.ofs, caret.block->text_len);
		if (forward)
		{
			if (iter.IsAtEnd())
				gofs += 2;
			else
			{

#ifdef SUPPORT_UNICODE_NORMALIZE
				iter.NextBaseCharacter();
#else
				iter.Next();
#endif

				gofs += iter.Index() - caret.ofs;
			}
		}
		else
		{
			if (iter.IsAtBeginning())
				gofs -= 2;
			else
			{

#ifdef SUPPORT_UNICODE_NORMALIZE
				iter.PreviousBaseCharacter();
#else
				iter.Previous();
#endif

				gofs += iter.Index() - caret.ofs;
			}
		}
		caret_snap_forward = !forward;
		SetCaretOfsGlobal(gofs);
		return;
	}

	// Visual movement has to be done with a visual offset to handle RTL/LTR correctly.

	OP_TEXT_FRAGMENT_LOGICAL_OFFSET log_ofs;
	log_ofs.offset = caret.ofs;
	log_ofs.snap_forward = caret_snap_forward;
	BOOL wrapped = FALSE;

	// Check if we wrap from this block to another.

	OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_ofs = caret.block->fragments.LogicalToVisualOffset(log_ofs);

	UnicodeStringIterator iter(caret.block->text.CStr(), vis_ofs.offset, caret.block->text_len);
	if (forward)
	{
		if (iter.IsAtEnd())
			wrapped = TRUE;
		else
		{

#ifdef SUPPORT_UNICODE_NORMALIZE
			iter.NextBaseCharacter();
#else
			iter.Next();
#endif

			vis_ofs.offset = iter.Index();
		}
	}
	else
	{
		if (iter.IsAtBeginning())
			wrapped = TRUE;
		else
		{

#ifdef SUPPORT_UNICODE_NORMALIZE
			iter.PreviousBaseCharacter();
#else
			iter.Previous();
#endif

			vis_ofs.offset = iter.Index();
		}
	}

	// We might still have wrapped to a new line within the same block.
	// Find that out by calculating the new offsets position. (Simple fix)
	if (!wrapped)
	{
		log_ofs = caret.block->fragments.VisualToLogicalOffset(vis_ofs);
		OpTCOffset tmp_ofs(caret.block, log_ofs.offset);
		OpPoint potential_caret_pos = tmp_ofs.GetPoint(info, log_ofs.snap_forward);
		if (potential_caret_pos.y != caret_pos.y)
			wrapped = TRUE;
	}

	// If we wrapped to a new line, we have to base the new position on the visual start or stop of that line.
	if (wrapped)
	{
		OpPoint old_caret_pos = caret_pos;

		// Move to next or previous line. RTL widgets does the opposite.
		if (forward != info->rtl)
			SetCaretPos(OpPoint(caret_pos.x, caret_pos.y + line_height));
		else
			SetCaretPos(OpPoint(caret_pos.x, caret_pos.y - line_height));

		// Move it to the first or last visual pos if it was moved
		if (caret_pos.y != old_caret_pos.y)
		{
			MoveToStartOrEndOfLine(forward, visual_order);
		}
		return;
	}

	log_ofs = caret.block->fragments.VisualToLogicalOffset(vis_ofs);

	caret_snap_forward = log_ofs.snap_forward;
	SetCaretOfs(log_ofs.offset);
}

void OpTextCollection::SelectToCaret(INT32 old_caret_pos, INT32 new_caret_pos)
{
	if (old_caret_pos == new_caret_pos)
		return;

	int s1 = old_caret_pos;
	int s2 = new_caret_pos;
	if (HasSelectedText())
	{
		OP_TCINFO* info = listener->TCGetInfo();
		s1 = sel_start.GetGlobalOffset(info);
		s2 = sel_stop.GetGlobalOffset(info);
		if (s1 == old_caret_pos)
			s1 = new_caret_pos;
		else
			s2 = new_caret_pos;
	}
	SetSelection(s1, s2, FALSE);
}

void OpTextCollection::SetSelection(OpPoint from, OpPoint to)
{
	OP_TCINFO* info = listener->TCGetInfo();
	if (!info->selectable)
		return;

	OpTCOffset s1 = PointToOffset(from);
	OpTCOffset s2 = PointToOffset(to);
	if (s1.block && s2.block)
		SetSelection(s1, s2);
}

void OpTextCollection::SetSelection(OpTCOffset ofs1, OpTCOffset ofs2)
{
	OP_TCINFO* info = listener->TCGetInfo();
	if (!info->selectable)
		return;

	int s1 = ofs1.GetGlobalOffset(info);
	int s2 = ofs2.GetGlobalOffset(info);
	SetSelection(s1, s2, FALSE);
}

void OpTextCollection::SetSelection(INT32 char_start, INT32 char_stop, BOOL check_boundary, BOOL line_normalized)
{
	OP_TCINFO* info = listener->TCGetInfo();
	if (!info->selectable)
		return;

	BOOL old_selection = HasSelectedText();
	OpTCOffset old_start = sel_start;
	OpTCOffset old_stop = sel_stop;

	if (char_stop < char_start)
	{
		INT32 tmp = char_start;
		char_start = char_stop;
		char_stop = tmp;
	}

	if (char_start == 0 && char_stop == 0)
		check_boundary = FALSE; // Optimization

	int text_len = check_boundary ? GetTextLength(FALSE) : 0;

	char_start = MAX(0, char_start);
	if (check_boundary)
		char_start = MIN(text_len, char_start);

	char_stop = MAX(char_start, char_stop);
	if (check_boundary)
		char_stop = MIN(char_stop, text_len);

	if (char_stop == char_start)
	{
		SelectNothing(TRUE);
		return;
	}

	if (!line_normalized)
	{
		sel_start.SetGlobalOffset(info, char_start);
		sel_stop.SetGlobalOffset(info, char_stop);
	}
	else
	{
		sel_start.SetGlobalOffsetNormalized(info, char_start);
		sel_stop.SetGlobalOffsetNormalized(info, char_stop);
	}
	if (sel_start.block && sel_stop.block)
	{
		if (old_selection)
		{
			InvalidateDiff(&old_start, &sel_start);
			InvalidateDiff(&old_stop, &sel_stop);
		}
		else
			InvalidateDiff(&sel_start, &sel_stop);
	}
	else
		SelectNothing(TRUE);
}

void OpTextCollection::InvalidateDiff(OpTCOffset* ofs1, OpTCOffset* ofs2)
{
	if (*ofs1 == *ofs2)
		return;

	OP_TCINFO* info = listener->TCGetInfo();
	int visible_line_height = MAX(line_height, (int)info->font_height);

	int line1 = ofs1->block->GetStartLineFromOfs(info, ofs1->ofs);
	int line2 = ofs2->block->GetStartLineFromOfs(info, ofs2->ofs);

	int y1 = ofs1->block->y + line1 * line_height;
	int y2 = ofs2->block->y + line2 * line_height;

	if (!ofs1->block->line_info)
		InvalidateBlock(ofs1->block);
	if (!ofs2->block->line_info)
		InvalidateBlock(ofs2->block);

	if (y1 < y2)
		listener->TCInvalidate(OpRect(0, y1, MAX(total_width, visible_width), y2 - y1 + visible_line_height));
	else
		listener->TCInvalidate(OpRect(0, y2, MAX(total_width, visible_width), y1 - y2 + visible_line_height));
}

OP_STATUS OpTextCollection::Reformat(BOOL update_fragment_list)
{
	total_width = 0;
	max_block_width = 0;
	OP_TCINFO* info = listener->TCGetInfo();
	OpTCBlock* block = FirstBlock();
	while (block)
	{
		if (update_fragment_list)
			block->UpdateAndLayout(info, FALSE);
		else
			block->Layout(info, block->block_width, FALSE);
		block = (OpTCBlock*) block->Suc();
	}
	UpdateCaretPos();
	return OpStatus::OK;
}

OP_STATUS OpTextCollection::InsertText(const uni_char* text, INT32 text_len, BOOL use_undo_stack, BOOL can_append_undo)
{
	if (!text)
		return OpStatus::OK;

	OP_TCINFO* info = listener->TCGetInfo();

#ifdef WIDGETS_LIMIT_TEXTAREA_SIZE
	INT32 max_bytes = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaximumBytesTextarea) / sizeof(uni_char);
	INT32 current_len = GetTextLength(FALSE);
	if (text_len + current_len > max_bytes)
		text_len = max_bytes - current_len;
#endif

	if (text_len == 0)
	{
#if defined WIDGETS_UNDO_REDO_SUPPORT && defined WIDGETS_IME_SUPPORT
		if (info->imestring && use_undo_stack)
			// Must create a event in the undostack since inputmethods will Undo one next OnCompose.
			return undo_stack.SubmitEmpty();
#endif //defined WIDGETS_UNDO_REDO_SUPPORT && defined WIDGETS_IME_SUPPORT

		return OpStatus::OK;
	}

	// We treat all breaks as \r\n internally (for offsets and when the text is returned), so if we have \r breaks,
	// we have to make a temporary string with \r\n breaks so undostack and caretpos won't get unsynchronized with the text.
	int i, extra_length = 0;
	for(i = 0; i < text_len; i++)
	{
		if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r'))
			extra_length++;
	}
	OpString tmp_string; //< Dummy string that holds the temporary buffer.
	if (extra_length)
	{
		uni_char *buf = tmp_string.Reserve(text_len + extra_length);
		if (!buf)
			return OpStatus::ERR_NO_MEMORY;
		int j = 0;
		for(i = 0; i < text_len; i++)
		{
			if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r'))
				buf[j++] = '\r';
			buf[j++] = text[i];
		}
		OP_ASSERT(j == text_len + extra_length);

		// Use temporary data
		text = buf;
		text_len += extra_length;
	}

	listener->TCOnContentChanging();

	BOOL delay_spell = FALSE;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	UINT32 spell_old_frag_count = 0;
	// Check if we should delay spellchecking, for not marking the word currently being written
	if(m_spell_session && caret.block && !HasSelectedText() && use_undo_stack && text_len == 1 && !m_spell_session->IsWordSeparator(*text))
	{
		BOOL after_blocked = caret.ofs < caret.block->text_len && !m_spell_session->IsWordSeparator(caret.block->text.CStr()[caret.ofs]);
		if(!after_blocked)
		{
			BOOL before_blocked = FALSE;
			if(!m_delayed_spell)
				before_blocked = caret.ofs && !m_spell_session->IsWordSeparator(caret.block->text.CStr()[caret.ofs-1]);
			if(!before_blocked)
			{
				spell_old_frag_count = caret.block->fragments.GetNumFragments();
				delay_spell = TRUE;
				m_delayed_spell = TRUE;
			}
		}
	}
#endif
	BeginChange(!delay_spell);

	// Remove any text that is marked.
	OP_STATUS ret = RemoveSelection(use_undo_stack, TRUE);
	if (OpStatus::IsError(ret))
	{
		EndChange(!delay_spell);
		return ret;
	}

	if (!FirstBlock())
	{
		OpTCBlock* block = OP_NEW(OpTCBlock, ());
		if (!block)
		{
			EndChange(!delay_spell);
			return OpStatus::ERR_NO_MEMORY;
		}
		block->Into(&blocks);
	}
	if (!caret.block)
		SetCaret(OpTCOffset(FirstBlock(), 0));

	INT32 old_caret_pos = caret.GetGlobalOffset(info);

	OP_STATUS ret_val = caret.block->InsertText(caret.ofs, text, text_len, info);
	if (OpStatus::IsError(ret_val))
	{
		EndChange(!delay_spell);
		return ret_val;
	}

#if defined(INTERNAL_SPELLCHECK_SUPPORT)
	// possibly move the spell-flag for the fragments in front of the word beeing written, if a new fragment has been created
	if(spell_old_frag_count && caret.block && caret.block->fragments.GetNumFragments() > spell_old_frag_count)
		caret.block->fragments.MoveSpellFlagsForwardAfter(caret.ofs, caret.block->fragments.GetNumFragments() - spell_old_frag_count);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	SetCaretOfsGlobal(old_caret_pos + text_len);

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	// Remember to put things into the undo stack. It is the position of the inserted text.
	if (use_undo_stack)
	{
		ret_val = undo_stack.SubmitInsert(old_caret_pos, text, !can_append_undo, text_len);
	}
#endif // WIDGETS_UNDO_REDO_SUPPORT

	EndChange(!delay_spell);
	return ret_val;
}

void OpTextCollection::Paint(UINT32 color, UINT32 fg_col, UINT32 bg_col, UINT32 link_col, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type, const OpRect& rect, bool paint_styled)
{
	OP_TCINFO* info = listener->TCGetInfo();
	VisualDevice* vis_dev = info->vis_dev;
	vis_dev->BeginClipping(rect);
	vis_dev->SetColor32(color);
	vis_dev->SetFont(info->original_fontnumber);
	const UINT32 base_ascent = vis_dev->GetFontAscent();

	// Draw content
	PaintTraverser traverser(rect, color, fg_col, bg_col, link_col, selection_highlight_type, base_ascent);

	int text_ofs = 0;
	OpTCBlock* block = FirstBlock();
	while (block)
	{
		if (block->y > rect.y + rect.height)
			break;
		traverser.block_ofs = text_ofs;
		if (block->y + block->block_height > rect.y)
		{
			block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromY(info, rect.y));
		}
		text_ofs += block->text_len + 2;
		block = (OpTCBlock*) block->Suc();
	}

	vis_dev->EndClipping();

	// Draw caret
#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
	if (!paint_styled || !HasSelectedText())
#endif
		if (info->show_caret)
		{
			INT32 height = info->font_height;
			vis_dev->DrawCaret(OpRect(caret_pos.x, caret_pos.y, info->overstrike ? 3:g_op_ui_info->GetCaretWidth(), height));
		}
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT

OP_STATUS OpTextCollection::RunPendingSpellcheck()
{
	if(!m_spell_session || !m_has_pending_spellcheck || m_iterators->word_iterator.Active() || m_spell_change_count)
		return OpStatus::OK;
	m_has_pending_spellcheck = FALSE;
	if(!blocks.First())
		return OpStatus::OK;
	if(!m_pending_spell_first)
		m_pending_spell_first = (OpTCBlock*)blocks.First();
	if(!m_pending_spell_last)
		m_pending_spell_last = (OpTCBlock*)blocks.Last();
	m_iterators->word_iterator.SetRange(m_pending_spell_first, m_pending_spell_last);
	m_pending_spell_first = NULL;
	m_pending_spell_last = NULL;
	if(!m_iterators->word_iterator.Active())
		return OpStatus::OK;

	OP_STATUS status;
	if(OpStatus::IsError(status = m_spell_session->CheckText(&m_iterators->word_iterator, FALSE)))
		DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
	return OpStatus::OK;
}

void OpTextCollection::OnSessionReady(OpSpellCheckerSession *session)
{
	if(!blocks.First())
		return;
	OnBlocksInvalidated((OpTCBlock*)blocks.First(), (OpTCBlock*)blocks.Last());
	RunPendingSpellcheck();
}

void OpTextCollection::OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string)
{
	DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
}

void OpTextCollection::OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements)
{
	m_has_spellchecked = TRUE;
	TextCollectionWordIterator *w = (TextCollectionWordIterator*)word;
	if (w->WordInvalid())
		return;
	MarkMisspellingInBlock(TRUE, w->CurrentStart().block, w->CurrentStart().ofs, w->CurrentStop().ofs);
}

void OpTextCollection::OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word)
{
	TextCollectionWordIterator *w = (TextCollectionWordIterator*)word;
	if (w->WordInvalid())
		return;
	MarkMisspellingInBlock(FALSE, w->CurrentStart().block, w->CurrentStart().ofs, w->CurrentStop().ofs);
}

void OpTextCollection::OnCheckingComplete(OpSpellCheckerSession *session)
{
	RunPendingSpellcheck();
}

BOOL OpTextCollection::SetSpellCheckLanguage(const uni_char *lang)
{
	return OpStatus::IsSuccess(EnableSpellcheckInternal(TRUE /*by_user*/, lang));
}

void OpTextCollection::DisableSpellcheck()
{
	DisableSpellcheckInternal(TRUE /*by_user*/, TRUE /*force*/);
}
OP_STATUS OpTextCollection::EnableSpellcheckInternal(BOOL by_user, const uni_char* lang)
{
	if(!by_user && m_by_user)
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	if(!m_spell_session)
	{
		if(!m_iterators)
			m_iterators = OP_NEW(TextCollectionWordIteratorSet, (this));

		/* Creating the spellchecker session eventually leads to OnSessionReady() being called, from there everything is set up and the checking is started */
		status = m_iterators ? g_internal_spellcheck->CreateSession(lang, this, m_spell_session, FALSE) : OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsError(status))
		{
			OP_ASSERT(m_spell_session == NULL);
			by_user = FALSE; // Remaining disabled is not what the user wanted.
		}
	}

	m_by_user = by_user;

	return status;
}

void OpTextCollection::DisableSpellcheckInternal(BOOL by_user, BOOL force)
{
	if(!force && !by_user && m_by_user)
		return;

	if(m_spell_session || by_user)
		m_by_user = by_user;

	UINT32 i;
	OP_DELETE(m_spell_session);
	m_spell_session = NULL;
	OP_DELETE(m_iterators);
	m_iterators = NULL;
	if(m_has_spellchecked)
	{
		OpTCBlock *block = (OpTCBlock*)blocks.First();
		while(block)
		{
			for(i=0;i<block->fragments.GetNumFragments();i++)
				block->fragments.Get(i)->wi.SetIsMisspelling(FALSE);
			block = (OpTCBlock*)block->Suc();
		}
		listener->TCInvalidateAll();
	}
	m_has_spellchecked = FALSE;
}

OpSpellCheckerWordIterator *OpTextCollection::GetAllContentIterator()
{
	if(!blocks.First())
		return NULL;
	m_iterators->background_updater.SetRange((OpTCBlock*)blocks.First(), (OpTCBlock*)blocks.Last());
	return m_iterators->background_updater.Active() ? &m_iterators->background_updater : NULL;
}

OP_STATUS OpTextCollection::ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *str)
{
	TextCollectionWordIterator *word = (TextCollectionWordIterator*)word_iterator;
	OP_ASSERT(!word->WordInvalid());
	sel_start = word->CurrentStart();
	sel_stop = word->CurrentStop();
	SetCaret(sel_start);

	OP_STATUS rc = InsertText(str, uni_strlen(str));
	if (listener && OpStatus::IsSuccess(rc))
	{
		listener->TCOnTextCorrected();
	}
	return rc;
}

void OpTextCollection::OnBlocksInvalidated(OpTCBlock *start, OpTCBlock *stop)
{
	m_delayed_spell = FALSE;
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY)
		return;
	m_iterators->replace_word.OnBlocksInvalidated(start, stop);
	m_iterators->background_updater.OnBlocksInvalidated(start, stop);
	m_iterators->word_iterator.OnBlocksInvalidated(start, stop);
	if(start && stop)
	{
		if(!m_pending_spell_first || start == m_pending_spell_first || start->Precedes(m_pending_spell_first))
			m_pending_spell_first = (OpTCBlock*)start->Pred();
		if(!m_pending_spell_last || stop == m_pending_spell_last || m_pending_spell_last->Precedes(stop))
			m_pending_spell_last = (OpTCBlock*)stop->Suc();
	}
	else
	{
		m_pending_spell_first = NULL;
		m_pending_spell_last = NULL;
	}
	m_has_pending_spellcheck = TRUE;
}

void OpTextCollection::OnCaretChange(OpTCOffset old_caret, OpTCOffset new_caret)
{
	int i;
	if(!m_delayed_spell || !m_spell_session)
		return;
	if(!old_caret.block || !new_caret.block || old_caret.block != new_caret.block)
	{
		m_delayed_spell = FALSE;
		if(old_caret.block)
		{
			OnBlocksInvalidated(old_caret.block, old_caret.block);
			RunPendingSpellcheck();
		}
		return;
	}
	int start = MAX(MIN(old_caret.ofs, new_caret.ofs), 0);
	int stop = MIN(MAX(old_caret.ofs, new_caret.ofs), old_caret.block->text_len);
	for(i = start; i < stop && !m_spell_session->IsWordSeparator(old_caret.block->text.CStr()[i]); i++)
		;
	if(i == stop)
		return;
	m_delayed_spell = FALSE;
	OnBlocksInvalidated(old_caret.block, old_caret.block);
	RunPendingSpellcheck();
}

void OpTextCollection::MarkMisspellingInBlock(BOOL misspelled, OpTCBlock *block, int start_ofs, int stop_ofs)
{
	UINT32 i;
	if(!block || start_ofs == stop_ofs)
		return;
	BOOL repaint = FALSE;
	for(i=0;i<block->fragments.GetNumFragments();i++)
	{
		OP_TEXT_FRAGMENT *fragment = block->fragments.Get(i);
		if(fragment->start >= start_ofs && fragment->start < stop_ofs)
		{
			repaint = repaint || !fragment->wi.IsMisspelling() != !misspelled;
			fragment->wi.SetIsMisspelling(misspelled);
		}
	}
	if(repaint)
		InvalidateBlock(block);
}

OP_STATUS OpTextCollection::CreateSpellUISessionInternal(const OpPoint &point, int &spell_session_id)
{
	spell_session_id = 0;
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY)
		return OpStatus::OK;

	// Run pending spellchecking first to avoid that the replace_word iterator is reseted in case the popup menu causes us to loose focus.
	if(m_delayed_spell && caret.block)
	{
		m_delayed_spell = FALSE;
		OnBlocksInvalidated(caret.block, caret.block);
		RunPendingSpellcheck();
	}

	OpTCOffset location = PointToOffset(point);
	if(!location.block)
		return OpStatus::OK;
	location.ofs = MAX(location.ofs-1,0);
	if(!location.block
		|| !location.block->text_len
		|| location.ofs > location.block->text_len
		|| m_spell_session->IsWordSeparator(location.block->text.CStr()[location.ofs]))
	{
		return OpStatus::OK;
	}
	m_iterators->replace_word.SetAtWord(location);
	if(!m_iterators->replace_word.Active())
		return OpStatus::OK;

	OpSpellCheckerSession temp_session(g_internal_spellcheck, m_spell_session->GetLanguage(), g_spell_ui_data, TRUE, m_spell_session->GetIgnoreWords());
	if(temp_session.CanSpellCheck(m_iterators->replace_word.GetCurrentWord()))
	{
		spell_session_id = g_spell_ui_data->CreateIdFor(this);
		RETURN_IF_ERROR(temp_session.CheckText(&m_iterators->replace_word, TRUE));
	}
	return OpStatus::OK;
}

OP_STATUS OpTextCollection::CreateSpellUISession(const OpPoint &point, int &spell_session_id)
{
	OP_STATUS status = CreateSpellUISessionInternal(point, spell_session_id);
	if(OpStatus::IsError(status))
	{
		spell_session_id = 0;
		g_spell_ui_data->InvalidateData();
		return status;
	}
	if(!spell_session_id && (g_internal_spellcheck->HasInstalledLanguages() || m_spell_session))
		spell_session_id = g_spell_ui_data->CreateIdFor(this);
	return OpStatus::OK;
}

//====================================TextCollectionWordIterator====================================

void TextCollectionWordIterator::Reset()
{
	m_active = FALSE;
	m_single_word = FALSE;
	m_current_start.Clear();
	m_current_stop.Clear();
	m_first = NULL;
	m_last = NULL;
	m_word_invalid = TRUE;
	if(m_current_string.CStr())
		m_current_string.CStr()[0] = '\0';
	if(m_used_by_session)
	{
		OpSpellCheckerSession *session = m_collection->GetSpellCheckerSession();
		if(session)
			session->StopCurrentSpellChecking();
	}
}

void TextCollectionWordIterator::SetRange(OpTCBlock *first, OpTCBlock *last, BOOL goto_first_word)
{
	Reset();
	if(!first || !last)
		return;
	m_current_start.block = first;
	m_current_start.ofs = 0;
	m_current_stop.block = first;
	m_current_stop.ofs = 0;
	m_first = first;
	m_last = last;
	m_active = TRUE;
	if(goto_first_word)
		ContinueIteration();
}

void TextCollectionWordIterator::SetAtWord(OpTCOffset location)
{
	Reset();
	OpSpellCheckerSession *session = m_collection->GetSpellCheckerSession();
	if(!session || !location.block)
		return;
	const uni_char *text = location.block->text.CStr();
	int ofs = MIN(location.ofs, location.block->text_len-1);
	if(!text || ofs < 0 || session->IsWordSeparator(text[ofs]))
		return;
	SetRange(location.block, location.block, FALSE);
	while(ofs && !session->IsWordSeparator(text[ofs-1]))
		ofs--;
	m_current_start.ofs = ofs;
	m_current_stop.ofs = ofs;
	m_active = TRUE;
	ContinueIteration();
	if(m_active)
		m_single_word = TRUE;
}

void MoveOutOfRange(OpTCOffset &location, OpTCBlock *start, OpTCBlock *stop)
{
	OpTCBlock *tmp = start;
	for(;;)
	{
		if(tmp == location.block)
			break;
		if(tmp == stop)
			return;
		tmp = (OpTCBlock*)tmp->Suc();
	}
	if(start->Pred())
	{
		location.block = (OpTCBlock*)(start->Pred());
		location.ofs = location.block->text_len;
	}
	else
	{
		location.block = (OpTCBlock*)(stop->Suc());
		location.ofs = 0;
	}
}

void MoveOutOfRange(OpTCBlock *&block, OpTCBlock *start, OpTCBlock *stop)
{
	OpTCOffset location(block,0);
	MoveOutOfRange(location, start, stop);
	block = location.block;
}

void TextCollectionWordIterator::OnBlocksInvalidated(OpTCBlock *start, OpTCBlock *stop)
{
	OP_ASSERT(start && stop);

	if(!m_active || !start || !stop)
		return;

	if(m_single_word)
	{
		OpTCBlock *tmp, *end = (OpTCBlock*)stop->Suc();
		for(tmp = start; tmp != end; tmp = (OpTCBlock*)tmp->Suc())
		{
			if(tmp == m_current_start.block)
			{
				Reset();
				return;
			}
		}
		return;
	}
	OP_ASSERT(m_current_start.block == m_current_stop.block);
	OpTCBlock* old_current_start_block = m_current_start.block;
	MoveOutOfRange(m_current_start, start, stop);
	MoveOutOfRange(m_current_stop, start, stop);
	MoveOutOfRange(m_first, start, stop);
	MoveOutOfRange(m_last, start, stop);
	m_word_invalid = m_current_start.block != old_current_start_block;
	if(!m_current_start.block)
		Reset(); // The range covered all blocks. No point in continuing.
}

BOOL TextCollectionWordIterator::ContinueIteration()
{
	if(!m_active || m_single_word)
		return FALSE;
	OpSpellCheckerSession *session = m_collection->GetSpellCheckerSession();
	OpTCBlock *block = m_current_stop.block;
	int ofs = m_current_stop.ofs;
	for(;;)
	{
		int first_clean = ofs;
		while(ofs < block->text_len && session->IsWordSeparator(block->text.CStr()[ofs]))
			ofs++;
		m_collection->MarkMisspellingInBlock(FALSE, block, first_clean, ofs);
		if(ofs < block->text_len)
		{
			m_current_start.block = block;
			m_current_start.ofs = ofs;
			break;
		}
		if(block == m_last || !block->Suc())
		{
			Reset();
			return FALSE;
		}
		block = (OpTCBlock*)block->Suc();
		ofs = 0;
	}
	do
	{
		ofs++;
	} while(ofs < block->text_len && !session->IsWordSeparator(block->text.CStr()[ofs]));
	m_current_stop.block = block;
	m_current_stop.ofs = ofs;
	m_word_invalid = FALSE;
	OP_STATUS status = m_current_string.Set(block->text.CStr() + m_current_start.ofs, m_current_stop.ofs - m_current_start.ofs);
	if(OpStatus::IsError(status))
		Reset();
	return OpStatus::IsSuccess(status);
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

BOOL OpTextCollection::HasBeenSplit()
{
	for (OpTCBlock* b = FirstBlock(); b; b = (OpTCBlock*)b->Suc())
		if (b->fragments.HasBeenSplit())
			return TRUE;
	return FALSE;
}

OP_STATUS OpTextCollection::GetSelectionRects(OpVector<OpRect>* rects, OpRect& union_rect, const OpPoint& offset, const OpRect& edit_rect)
{
	OP_TCINFO* info = listener->TCGetInfo();
	SelectionTraverser traverser(rects, &union_rect, offset, edit_rect);

	int text_ofs = 0;
	OpTCBlock* block = sel_start.block;
	while (block && OpStatus::IsSuccess(traverser.m_status))
	{
		if (block->y > traverser.m_rect.y + traverser.m_rect.height)
			break;
		if (block->y + block->block_height > traverser.m_rect.y)
		{
			block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromY(info, traverser.m_rect.y));
		}
		text_ofs += block->text_len + 2;
		if (block == sel_stop.block)
			break;
		block = (OpTCBlock*) block->Suc();
	}

	return traverser.m_status;
}

BOOL OpTextCollection::IsWithinSelection(const OpPoint& point, const OpPoint& offset, const OpRect& edit_rect)
{
	OP_TCINFO* info = listener->TCGetInfo();
	SelectionTraverser traverser(point, offset, edit_rect);

	if (OpTCBlock* block = FindBlockAtPoint(point - offset))
		block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromY(info, traverser.m_rect.y));

	return traverser.m_contains_point;
}
