/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/widgets/OpTextCollection.h"
#include "modules/widgets/optextfragment.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/layout/bidi/characterdata.h"

#define TAB_SIZE 8 // IE and Mozilla use 8 in their textareas.
#define MIN_WRAPPED_LINES_TO_CACHE 1 // When a line has wrapped N times, the start for next line will be cached (optimization). N is this constant.

unsigned int FragmentExtentFromTo(OP_TEXT_FRAGMENT* frag,
                                  const uni_char* word,
                                  unsigned int from, unsigned int to,
                                  VisualDevice* vis_dev, int text_format);
void DrawFragment(VisualDevice* vis_dev, int x, int y,
	int sel_start, int sel_stop, int ime_start, int ime_stop,
	const uni_char* str, OP_TEXT_FRAGMENT* frag,
	UINT32 color, UINT32 fcol_sel, UINT32 bcol_sel, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type,
	int height, BOOL use_accurate_font_size = FALSE);
int OffsetToPosInFragment(int ofs, OP_TEXT_FRAGMENT* frag, const uni_char* word, VisualDevice* vis_dev, BOOL use_accurate_font_size = FALSE);
int PosToOffsetInFragment(int x_pos, OP_TEXT_FRAGMENT* frag, const uni_char* word, VisualDevice* vis_dev, BOOL use_accurate_font_size = FALSE);


#ifdef MULTILABEL_RICHTEXT_SUPPORT
static UINT32 styleForFragment(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	UINT32 i = 0;
	UINT32 style = OP_TC_STYLE_NORMAL;
	if (info == NULL || frag == NULL || info->styles == NULL)
		return style;

	OP_TCSTYLE *node = info->styles[i++];

	while (node->position <= (UINT32)frag->start && i < info->style_count)
	{
		style = node->style;
		node = info->styles[i++];
	}

	return style;
}

static const uni_char *linkForFragment(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	UINT32 i = 0;
	OP_TCSTYLE *foundNode = NULL;
	if (info == NULL || frag == NULL || info->styles == NULL)
		return NULL;

	OP_TCSTYLE *node = info->styles[i++];

	while (node->position <= (UINT32)frag->start && i < info->style_count)
	{
		foundNode = node;
		node = info->styles[i++];
	}

	if (foundNode && (foundNode->style & OP_TC_STYLE_LINK) == OP_TC_STYLE_LINK)
	{
		return foundNode->param;
	}

	return NULL;

}

static void styleMaskForFragment(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	if (info->styles == NULL)
		return;

	UINT32 style = styleForFragment(info, frag);

	// Set font weight (bold)
	info->vis_dev->SetFontWeight(((style & OP_TC_STYLE_BOLD) == OP_TC_STYLE_BOLD) ? 7 : 4);

	// Set italics
	info->vis_dev->SetFontStyle(((style & OP_TC_STYLE_ITALIC) == OP_TC_STYLE_ITALIC) ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL);

	// Headline (bigger font)
	int standard_font_size = info->vis_dev->GetFontSize();
	info->vis_dev->SetFontSize(((style & OP_TC_STYLE_HEADLINE) == OP_TC_STYLE_HEADLINE) ? ((int)(standard_font_size*1.5)) : standard_font_size);

	// Underline and link needs to be... underlined :P
	// This is done only while redrawing

}

/* Returns TRUE when any style given in bit mask styles is set on given text fragment, FALSE otherwise */
static BOOL hasFragmentAnyStyleSet(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, UINT32 styles)
{
	return (styleForFragment(info, frag) & styles) > 0;
}
#endif // MULTILABEL_RICHTEXT_SUPPORT

int GetTabWidth(int xpos, VisualDevice* vis_dev)
{
	INT32 avecharw = (INT32) vis_dev->GetFontAveCharWidth();
	OP_ASSERT(avecharw); // Something must be wrong with the current font if this is trigged!
	if (!avecharw)
		avecharw = 7;

	INT32 tabsize = avecharw * TAB_SIZE;
	INT32 tab_width = int(xpos / tabsize + 1) * tabsize - xpos;
	return tab_width;
}

// == class LayoutTraverser ========================================

void LayoutTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	block->fragments.ResolveOrder(info->rtl, first_fragment_idx, num_fragments);
	block->num_lines++;
}

// == class PaintTraverser ========================================

PaintTraverser::PaintTraverser(const OpRect& rect, UINT32 col, UINT32 fg_col, UINT32 bg_col, UINT32 link_col, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type, UINT32 base_ascent)
	: rect(rect)
	, col(col)
	, fg_col(fg_col)
	, bg_col(bg_col)
	, link_col(link_col)
	, selection_highlight_type(selection_highlight_type)
	, block_ofs(0)
	, base_ascent(base_ascent)
{
}

#ifdef MULTILABEL_RICHTEXT_SUPPORT
void PaintTraverser::SetStyle(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	styleMaskForFragment(info, frag);
}

BOOL PaintTraverser::IsLink(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	return hasFragmentAnyStyleSet(info, frag, OP_TC_STYLE_LINK);
}

void PaintTraverser::DrawUnderline(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, int x, int y)
{
	if (!hasFragmentAnyStyleSet(info, frag, OP_TC_STYLE_UNDERLINE | OP_TC_STYLE_LINK))
		return;

	int  underline_width = frag->wi.GetWidth();
	if (!hasFragmentAnyStyleSet(info, block->fragments.Get(frag->nextidx), OP_TC_STYLE_UNDERLINE | OP_TC_STYLE_LINK) &&
			frag->wi.HasTrailingWhitespace())
	{
		if (frag->wi.IsTabCharacter())
		{
			underline_width -= GetTabWidth(0, info->vis_dev);
		}
		else
		{
			underline_width -= info->vis_dev->GetFontStringWidth(UNI_L(" "), 1);
		}
	}
	info->vis_dev->DrawLine(OpPoint(x, y), underline_width, TRUE, 1);
}
#endif // MULTILABEL_RICHTEXT_SUPPORT

OP_TC_BT_STATUS PaintTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	if (collapsed)
		return OP_TC_BT_STATUS_CONTINUE;
	if (!info->wrap)
	{
		// Optimization
		if (translation_x > rect.x + rect.width)
			return OP_TC_BT_STATUS_STOP_ALL;
		if (translation_x + (int)frag->wi.GetWidth() < rect.x)
			return OP_TC_BT_STATUS_CONTINUE;
	}
	else
	{
		if (block->y + translation_y > rect.y + rect.height)
			return OP_TC_BT_STATUS_STOP_ALL;
		if (block->y + translation_y + (int) info->font_height < rect.y)
			return OP_TC_BT_STATUS_CONTINUE;
		if (block->line_info || block->num_lines == 1)
		{
			// We don't have to traverse the whole line because it's only 1 line or next start is cached in line_info
			if (translation_x > rect.x + rect.width)
				return OP_TC_BT_STATUS_STOP_LINE;
			if (translation_x + (int)frag->wi.GetWidth() < rect.x)
				return OP_TC_BT_STATUS_CONTINUE;
		}
	}

	info->vis_dev->SetFont(frag->wi.GetFontNumber());
	const int dy = base_ascent - info->vis_dev->GetFontAscent();

#ifdef MULTILABEL_RICHTEXT_SUPPORT
	SetStyle(info, frag);
#endif // MULTILABEL_RICHTEXT_SUPPORT

	int sel_start = 0;
	int sel_stop = 0;
	int ime_start = 0;
	int ime_stop = 0;

	OpTextCollection* tc = info->tc;
	if (info->show_selection && tc->HasSelectedText())
	{
		// Check if this block is in the selection
		if (block->y >= tc->sel_start.block->y && block->y <= tc->sel_stop.block->y)
		{
			sel_stop = block->text_len;
			if (block->y == tc->sel_start.block->y)
				sel_start = tc->sel_start.ofs;
			if (block->y == tc->sel_stop.block->y)
				sel_stop = tc->sel_stop.ofs;
		}
	}
#ifdef WIDGETS_IME_SUPPORT
	if (info->imestring && info->imestring->GetShowUnderline())
	{
		ime_start = info->ime_start_pos - block_ofs;
		ime_stop = ime_start + strlen_offset_half_newlines(info->imestring->Get());
	}
#endif

	UINT32 local_colour;

#ifdef COLOURED_MULTIEDIT_SUPPORT
	if (info->coloured && block->fragments.HasColours())
	{
		// Set the colour based on the tag type
		switch (block->fragments.GetTagType(frag))
		{
			case HTT_NONE:
				local_colour = TEXT_SYNTAX_COLOUR;
			break;

			case HTT_TAG_START:
			case HTT_TAG_END:
				local_colour = TAG_SYNTAX_COLOUR;
			break;

			case HTT_STYLE_OPEN_START:
			case HTT_STYLE_CLOSE_START:
			case HTT_STYLE_OPEN_END:
			case HTT_STYLE_CLOSE_END:
				local_colour = STYLE_SYNTAX_COLOUR;
			break;

			case HTT_SCRIPT_OPEN_START:
			case HTT_SCRIPT_CLOSE_START:
			case HTT_SCRIPT_OPEN_END:
			case HTT_SCRIPT_CLOSE_END:
				local_colour = SCRIPT_SYNTAX_COLOUR;
			break;

			case HTT_COMMENT_START:
			case HTT_COMMENT_END:
				local_colour = COMMENT_SYNTAX_COLOUR;
			break;

			default: // The default can reset the tag
				local_colour = TEXT_SYNTAX_COLOUR;
			break;
		}
	}
	else
# ifdef MULTILABEL_RICHTEXT_SUPPORT
		local_colour = (IsLink(info, frag)) ? link_col : col;
# else
		local_colour = col;
# endif // MULTILABEL_RICHTEXT_SUPPORT
#else // !COLOURED_MULTIEDIT_SUPPORT
# ifdef MULTILABEL_RICHTEXT_SUPPORT
	local_colour = (IsLink(info, frag)) ? link_col : col;
# else
	local_colour = col;
# endif // MULTILABEL_RICHTEXT_SUPPORT
#endif // !COLOURED_MULTIEDIT_SUPPORT

	int visible_line_height = MAX(info->tc->line_height, (int)info->font_height);
	DrawFragment(info->vis_dev, translation_x, block->y + translation_y + dy, sel_start, sel_stop, ime_start, ime_stop,
		block->text.CStr(), frag, local_colour, fg_col, bg_col, selection_highlight_type, visible_line_height, info->use_accurate_font_size);

#ifdef MULTILABEL_RICHTEXT_SUPPORT
	DrawUnderline(info, frag, translation_x, translation_y + visible_line_height - 1);
#endif // MULTILABEL_RICHTEXT_SUPPORT

	return OP_TC_BT_STATUS_CONTINUE;
}

void PaintTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	OpTextCollection* tc = info->tc;
	if (info->show_selection && tc->HasSelectedText())
	{
		// Check if this block is in the selection
		if (block->y >= tc->sel_start.block->y && block->y < tc->sel_stop.block->y)
		{
			// Check if it is the last line in the block
			if (first_fragment_idx + num_fragments == (int) block->fragments.GetNumFragments())
			{
				// Paint ending linebreaks selection
				int visible_line_height = MAX(info->tc->line_height, (int)info->font_height);
				info->vis_dev->SetColor(bg_col);
				info->vis_dev->FillRect(OpRect(translation_x, block->y + translation_y,
										info->vis_dev->GetFontAveCharWidth(), visible_line_height));
			}
		}
	}
}

// == class LineFinderTraverser ================================

LineFinderTraverser::LineFinderTraverser(OpTCOffset* ofs, BOOL snap_forward)
	: OffsetToPointTraverser(ofs, snap_forward)
	, line_nr(0)
	, line_first_fragment_idx(0)
	, line_num_fragments(0)
	, found_word(FALSE)
	, found_line(FALSE)
{
}

OP_TC_BT_STATUS LineFinderTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	if (found_line)
		return OP_TC_BT_STATUS_STOP_ALL;

	OP_TC_BT_STATUS status = OffsetToPointTraverser::HandleWord(info, frag, collapsed);
	if (status == OP_TC_BT_STATUS_STOP_ALL)
	{
		found_word = TRUE;
		return OP_TC_BT_STATUS_STOP_LINE;
	}

	return status;
}

void LineFinderTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	OffsetToPointTraverser::HandleLine(info, first_fragment_idx, num_fragments);
	line_first_fragment_idx = first_fragment_idx;
	line_num_fragments = num_fragments;
	line_nr++;
	if (found_word)
		found_line = TRUE;
}

// == class OffsetToPointTraverser ================================

OffsetToPointTraverser::OffsetToPointTraverser(OpTCOffset* ofs, BOOL snap_forward)
	: snap_forward(snap_forward)
	, ofs(ofs)
{
}

OP_TC_BT_STATUS OffsetToPointTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	int frag_end = frag->start + frag->wi.GetLength();
	if (!snap_forward || frag->start + (int) frag->wi.GetLength() == block->text_len)
		frag_end++;

	int frag_start = frag->start;
	if (snap_forward)
		frag_start--;

	if (ofs->ofs >= frag->start && ofs->ofs < frag_end)
	{
		// We have a candidate. Update point.
		int x_pos = 0;
		if (!collapsed) ///< collapsed fragments should return 0 (Soft hyphens)
			x_pos = OffsetToPosInFragment(ofs->ofs, frag, block->text.CStr() + frag->start, info->vis_dev, info->use_accurate_font_size);
		point.x = translation_x + x_pos;
		point.y = translation_y;

		// Only if we know we found the right fragment (depending on snap_forward), we will stop.
		if (ofs->ofs > frag_start)
			return OP_TC_BT_STATUS_STOP_ALL;
	}
	return OP_TC_BT_STATUS_CONTINUE;
}

void OffsetToPointTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	if (!num_fragments)
		point.x = translation_x;
}

// == class PointToOffsetTraverser ===================================

PointToOffsetTraverser::PointToOffsetTraverser(const OpPoint& point)
	: point(point)
	, snap_forward(FALSE)
	, line_first_frag(NULL)
	, line_last_frag(NULL)
{
}

OP_TC_BT_STATUS PointToOffsetTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	if (collapsed)
		// Never find collapsed fragments (collapsed soft hyphens)
		return OP_TC_BT_STATUS_CONTINUE;

	if (!line_first_frag)
		line_first_frag = frag;
	line_last_frag = frag;

	if (point.x >= translation_x && point.x < translation_x + (int)frag->wi.GetWidth() &&
		point.y >= translation_y && point.y < translation_y + info->tc->line_height)
	{
		ofs.block = block;
		ofs.ofs = PosToOffsetInFragment(point.x - translation_x, frag, block->text.CStr() + frag->start, info->vis_dev, info->use_accurate_font_size);
		snap_forward = (ofs.ofs == frag->start);
		return OP_TC_BT_STATUS_STOP_ALL;
	}
	return OP_TC_BT_STATUS_CONTINUE;
}

void PointToOffsetTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	if (point.y >= translation_y && point.y < translation_y + info->tc->line_height)
	{
		// This is the line, but no word was hit. Return first or last.
		ofs.block = block;
		ofs.ofs = 0;

		if (line_first_frag)
		{
			BOOL first = point.x < translation_x;
			OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_ofs;
			if (first)
			{
				vis_ofs.idx = first_fragment_idx;
				OP_TEXT_FRAGMENT* frag = block->fragments.Get(vis_ofs.idx);
				vis_ofs.offset = frag->start;
			}
			else
			{
				vis_ofs.idx = first_fragment_idx + num_fragments - 1;
				OP_TEXT_FRAGMENT* last_log_frag = block->fragments.Get(vis_ofs.idx);
				vis_ofs.offset = last_log_frag->start + last_log_frag->wi.GetLength();
			}

			OP_TEXT_FRAGMENT_LOGICAL_OFFSET log_ofs = block->fragments.VisualToLogicalOffset(vis_ofs);
			ofs.ofs = log_ofs.offset;
			snap_forward = log_ofs.snap_forward;
		}
	}
	line_first_frag = NULL;
	line_last_frag = NULL;
}

// == class GetTextTraverser ===================================

GetTextTraverser::GetTextTraverser(uni_char* buf, BOOL insert_linebreak, int max_len, int offset)
	: buf(buf), len(0), insert_linebreak(insert_linebreak), max_len(max_len), offset(offset)
{
}

OP_TC_BT_STATUS GetTextTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	int copylen = MIN(max_len - len, (int)frag->wi.GetLength() - offset);
	if (copylen <= 0)
		return OP_TC_BT_STATUS_STOP_ALL;

	if (buf)
		op_memcpy(&buf[len], &block->text.CStr()[frag->start + offset], copylen * sizeof(uni_char));
	len += copylen;
	return OP_TC_BT_STATUS_CONTINUE;
}

void GetTextTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	BOOL at_end_of_block = first_fragment_idx + num_fragments == (int) block->fragments.GetNumFragments();
	BOOL at_end_of_text = at_end_of_block && !block->Suc();
	if ((insert_linebreak || at_end_of_block) && !at_end_of_text)
	{
		int copylen = MIN(max_len - len, 2 - offset);
		if (copylen <= 0)
			return;

		if (buf)
			op_memcpy(&buf[len], UNI_L("\r\n") + offset, copylen * sizeof(uni_char));
		len += copylen;
	}
}

// == class SelectionRectListTraverser ===================================
extern void GetStartStopSelection(int *sel_start, int *sel_stop, const WordInfo& wi);

OP_TC_BT_STATUS SelectionTraverser::HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed)
{
	if (!OpStatus::IsSuccess(m_status))
		return OP_TC_BT_STATUS_STOP_ALL;

	if (collapsed)
		return OP_TC_BT_STATUS_CONTINUE;

	int sel_start = 0;
	int sel_stop = 0;

	const uni_char* word = block->text.CStr() + frag->start;

	OpTextCollection* tc = info->tc;
	if (tc->HasSelectedText())
	{
		// Check if this block is in the selection
		if (block->y >= tc->sel_start.block->y && block->y <= tc->sel_stop.block->y)
		{
			sel_stop = block->text_len;
			if (block->y == tc->sel_start.block->y)
				sel_start = tc->sel_start.ofs;
			if (block->y == tc->sel_stop.block->y)
				sel_stop = tc->sel_stop.ofs;
		}
	}

	if (sel_start != sel_stop)
	{
		sel_start -= frag->start;
		sel_stop -= frag->start;
		GetStartStopSelection(&sel_start, &sel_stop, frag->wi);
	}

	OpRect* sel_rect_pointer = NULL;
	int height = MAX(info->tc->line_height, (int)info->font_height);
	if (sel_start != sel_stop)
	{
		int start_x = OffsetToPosInFragment(sel_start + frag->start, frag, word, info->vis_dev, info->use_accurate_font_size);
		int stop_x = OffsetToPosInFragment(sel_stop + frag->start, frag, word, info->vis_dev, info->use_accurate_font_size);
		if (start_x > stop_x)
		{
			int tmp = start_x;
			start_x = stop_x;
			stop_x = tmp;
		}

		OpRect sel_rect(start_x + translation_x, block->y + translation_y , stop_x - start_x, height);
		sel_rect.IntersectWith(m_rect);
		if (sel_rect.IsEmpty())
			return OP_TC_BT_STATUS_CONTINUE;
		sel_rect.OffsetBy(m_offset);

		if (m_list)
		{
			sel_rect_pointer = OP_NEW(OpRect, (sel_rect));

			if (!sel_rect_pointer)
			{
				m_status = OpStatus::ERR_NO_MEMORY;
				return OP_TC_BT_STATUS_STOP_ALL;
			}
		}
		else
			sel_rect_pointer = &sel_rect;

		if (m_union_rect)
			m_union_rect->UnionWith(*sel_rect_pointer);

		if (m_list)
		{
			m_status = m_list->Add(sel_rect_pointer);
			if (OpStatus::IsError(m_status))
			{
				OP_DELETE(sel_rect_pointer);
				return OP_TC_BT_STATUS_STOP_ALL;
			}
		}
		else if (!m_union_rect)
		{
			if (sel_rect_pointer->Contains(m_point))
			{
				m_contains_point = TRUE;
				return OP_TC_BT_STATUS_STOP_ALL;
			}
		}
	}

	return OP_TC_BT_STATUS_CONTINUE;
}

void SelectionTraverser::HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments)
{
	if (!OpStatus::IsSuccess(m_status) || m_contains_point == TRUE)
		return;

	OpTextCollection* tc = info->tc;
	if (tc->HasSelectedText())
	{
		// Check if this block is in the selection
		if (block->y >= tc->sel_start.block->y && block->y < tc->sel_stop.block->y)
		{
			int visible_line_height = MAX(info->tc->line_height, (int)info->font_height);
			OpRect* sel_rect_pointer = NULL;
			OpRect sel_rect(translation_x, block->y + translation_y,
							info->vis_dev->GetFontAveCharWidth(), visible_line_height);

			sel_rect.IntersectWith(m_rect);
			if (sel_rect.IsEmpty())
				return;
			sel_rect.OffsetBy(m_offset);

			if (m_list)
			{
				sel_rect_pointer = OP_NEW(OpRect, (sel_rect));

				if (!sel_rect_pointer)
				{
					m_status = OpStatus::ERR_NO_MEMORY;
					return;
				}
			}
			else
				sel_rect_pointer = &sel_rect;

			if (m_union_rect)
				m_union_rect->UnionWith(*sel_rect_pointer);
			if (m_list)
			{
				m_status = m_list->Add(sel_rect_pointer);
				if (OpStatus::IsError(m_status))
					OP_DELETE(sel_rect_pointer);
			}
			else if (!m_union_rect)
				if (sel_rect_pointer->Contains(m_point))
					m_contains_point = TRUE;
		}
	}
}

// == class OpTCOffset ===================================

int OpTCOffset::GetGlobalOffset(OP_TCINFO* info) const
{
	if (block == NULL)
		return 0;

	int gofs = 0;
	OpTCBlock* tmp = info->tc->FirstBlock();
	while(tmp != block)
	{
		gofs += tmp->text_len + 2;
		tmp = (OpTCBlock*) tmp->Suc();
	}
	gofs += ofs;
	return gofs;
}

void OpTCOffset::SetGlobalOffset(OP_TCINFO* info, int newofs)
{
	int gofs = 0;
	Clear();
	OpTCBlock* tmp = info->tc->FirstBlock();
	while(tmp)
	{
		int gofs_next = gofs + tmp->text_len + 2;
		if (newofs < gofs_next)
		{
			block = tmp;
			ofs = MIN(newofs - gofs, tmp->text_len);
			break;
		}
		gofs = gofs_next;
		tmp = (OpTCBlock*) tmp->Suc();
	}
	if (!block && info->tc->LastBlock())
	{
		block = info->tc->LastBlock();
		ofs = info->tc->LastBlock()->text_len;
	}
}

int OpTCOffset::GetGlobalOffsetNormalized(OP_TCINFO* info) const
{
	if (block == NULL)
		return 0;

	int gofs = 0;
	OpTCBlock* tmp = info->tc->FirstBlock();
	const uni_char *str;
	while(tmp != block)
	{
		/* Compute the number of logical codepoints in this block,
		   normalizing line terminators. */
		str = tmp->text.CStr();
		for (unsigned i = 0, len = tmp->text.Length(); i < len; i++)
		{
			if (str[i] == '\r' && i < (len - 1) && str[i + 1] == '\n')
				i++;
			gofs++;
		}
		gofs += 1;
		tmp = (OpTCBlock*) tmp->Suc();
	}
	str = block->text.CStr();
	for (unsigned i = 0, len = MIN(ofs, block->text.Length()); i < len; i++)
	{
		if (str[i] == '\r' && i < (len - 1) && str[i + 1] == '\n')
			i++;
		gofs++;
	}
	return gofs;
}

void OpTCOffset::SetGlobalOffsetNormalized(OP_TCINFO* info, int newofs)
{
	int nofs = 0;
	Clear();
	OpTCBlock* tmp = info->tc->FirstBlock();
	while(tmp)
	{
		/* Compute the number of logical codepoints in this block,
		   normalizing line terminators. */
		const uni_char *str = tmp->text.CStr();
		int nofs_next = nofs + 1;
		for (unsigned i = 0, len = tmp->text.Length(); i < len; i++)
		{
			if (str[i] == '\r' && i < (len - 1) && str[i + 1] == '\n')
				i++;
			nofs_next++;
		}
		if (newofs < nofs_next)
		{
			block = tmp;
			/* The number of normalized characters into this block. */
			int chars = MIN(newofs - nofs, tmp->text_len);
			int new_ofs = 0;
			for (int i = 0; i < chars; i++)
			{
				if (str[i] == '\r' && i < (chars - 1) && str[i + 1] == '\n')
					i++, new_ofs++;
				new_ofs++;
			}
			ofs = new_ofs;
			break;
		}
		nofs = nofs_next;
		tmp = (OpTCBlock*) tmp->Suc();
	}
	if (!block && info->tc->LastBlock())
	{
		block = info->tc->LastBlock();
		ofs = info->tc->LastBlock()->text_len;
	}
}

OpPoint OpTCOffset::GetPoint(OP_TCINFO* info, BOOL snap_forward)
{
	if (!block)
		return OpPoint();

	OffsetToPointTraverser traverser(this, snap_forward);
	block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromOfs(info, ofs));

	traverser.point.y += block->y;
	return traverser.point;
}

/*void OpTCOffset::SetPoint(OP_TCINFO* info, const OpPoint& point)
{
	PointToOffsetTraverser traverser(point);
	block->Traverse(info, &traverser, TRUE, FALSE, block->GetStartLineFromY(info, point.y));
	block = traverser.ofs.block;
	ofs = traverser.ofs.ofs;
}*/

int OpTCOffset::IsRTL(OP_TCINFO* info, BOOL snap_forward)
{
	if (block)
	{
		for(UINT32 i = 0; i < block->fragments.GetNumFragments(); i++)
		{
			OP_TEXT_FRAGMENT* frag = block->fragments.GetByVisualOrder(i);

			int frag_end = frag->start + frag->wi.GetLength();
			if (!snap_forward || i == block->fragments.GetNumFragments() - 1)
				frag_end++;

			if (ofs >= frag->start && ofs < frag_end)
				return frag->packed.bidi == BIDI_FRAGMENT_MIRRORED;
		}
	}
	return info->rtl;
}

BOOL OpTCOffset::operator == (const OpTCOffset& other) const
{
	return (block == other.block && ofs == other.ofs);
}

// == class OpTCBlock ===================================

OpTCBlock::OpTCBlock()
	: y(0)
	, block_width(0)
	, block_height(0)
	, text_len(0)
	, line_info(NULL)
	, num_lines(0)
{
}

OpTCBlock::~OpTCBlock()
{
	Out();
	OP_DELETEA(line_info);
}

#ifdef MULTILABEL_RICHTEXT_SUPPORT
const uni_char* OpTCBlock::GetLinkForOffset(OP_TCINFO* info, int ofs)
{
	// Find proper fragment first
	for(unsigned int i = 0; i < fragments.GetNumFragments(); i++)
	{
		OP_TEXT_FRAGMENT* frag = fragments.Get(i);
		if (frag->start <= ofs && ofs <= frag->start + static_cast<int>(frag->wi.GetLength()))
		{
			return linkForFragment(info, frag);
		}
	}
	return NULL;
}

void OpTCBlock::SetStyle(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag)
{
	styleMaskForFragment(info, frag);
}
#endif // MULTILABEL_RICHTEXT_SUPPORT

OP_STATUS OpTCBlock::RemoveText(int pos, int len, OP_TCINFO* info)
{
	text.Delete(pos, len);
	text_len = text.Length();
	return UpdateAndLayout(info, TRUE);
}

OP_STATUS OpTCBlock::SetText(const uni_char* newtext, int len, OP_TCINFO* info)
{
	OP_STATUS status = text.Set(newtext, len);
	if(OpStatus::IsSuccess(status))
	{
		text_len = len;
		return UpdateAndLayout(info, TRUE);
	}
	else
	{
		text_len = text.Length();
		fragments.Clear();
	}
	return status;
}

OP_STATUS OpTCBlock::InsertTextInternal(int pos, const uni_char* newtext, int len, OP_TCINFO* info)
{
	OP_STATUS status = text.Insert(pos, newtext, len);
	text_len = text.Length();
	if(OpStatus::IsSuccess(status))
		return UpdateAndLayout(info, TRUE);
	else
		fragments.Clear();
	return status;
}

OP_STATUS OpTCBlock::InsertText(int pos, const uni_char* newtext, int len, OP_TCINFO* info)
{
	OpTCBlock* follow_block = this;
	int ofs = 0;
	while(TRUE)
	{
		int line_start = ofs;
		while(ofs < len && newtext[ofs] != '\r' && newtext[ofs] != '\n')
			ofs++;
		BOOL has_ending_break = (ofs != len);

		int line_len = ofs - line_start;
		if (ofs < len - 1 && newtext[ofs] == '\r' && newtext[ofs + 1] == '\n')
			ofs++;
		ofs++;

		BOOL first_line = (line_start == 0);
		BOOL last_line = (ofs >= len);

		if (first_line)
		{
			RETURN_IF_ERROR(InsertTextInternal(pos, &newtext[line_start], line_len, info));
			pos += line_len;

			if (has_ending_break)
				RETURN_IF_ERROR(Split(pos, info));
		}
		else
		{
			OpTCBlock* block = OP_NEW(OpTCBlock, ());
			if (!block)
				return OpStatus::ERR_NO_MEMORY;

			if (pos == 0 && follow_block == this)
				block->Precede(follow_block);
			else
				block->Follow(follow_block);
			follow_block = block;

			if (OpStatus::IsError(block->SetText(&newtext[line_start], line_len, info)))
				return OpStatus::ERR_NO_MEMORY;

			if (last_line && !has_ending_break)
				block->Merge(info);
		}

		if (last_line)
			break;
	}
	return OpStatus::OK;
}

OP_STATUS OpTCBlock::Split(int pos, OP_TCINFO* info)
{
	OpTCBlock* block = OP_NEW(OpTCBlock, ());
	if (!block)
		return OpStatus::ERR_NO_MEMORY;
	if (pos == 0)
	{
		block->Precede(this);
		return block->UpdateAndLayout(info, TRUE);
	}
	else
	{
		block->Follow(this);
		if (OpStatus::IsError(block->SetText(&text.CStr()[pos], text_len - pos, info)))
			return OpStatus::ERR_NO_MEMORY;
		RemoveText(pos, text_len - pos, info);
	}
	return OpStatus::OK;
}

OP_STATUS OpTCBlock::Merge(OP_TCINFO* info)
{
	OpTCBlock* suc = (OpTCBlock*) Suc();
	if (!suc)
		return OpStatus::OK;
	OP_STATUS status = text.Append(suc->text);
	text_len = text.Length();
	if(OpStatus::IsSuccess(status))
	{
		OP_DELETE(suc);
		return UpdateAndLayout(info, TRUE);
	}
	else
		fragments.Clear();
	return status;
}

OP_STATUS OpTCBlock::UpdateAndLayout(OP_TCINFO* info, BOOL propagate_height)
{
	OP_STATUS status = OpStatus::ERR;
#ifdef COLOURED_MULTIEDIT_SUPPORT
	int start_in_tag = HTT_NONE;

	OpTCBlock *pred_block = (OpTCBlock*)Pred();
	if (info->coloured && pred_block)
	{
		// Loop backwards until you find a line with some length!
		while (pred_block && !pred_block->text_len)
			pred_block = (OpTCBlock*)pred_block->Pred();

		if (pred_block)
		{
			if (pred_block->fragments.HasColours())
			{
				UINT32 tag_type = pred_block->fragments.GetTagType(pred_block->fragments.GetNumFragments() - 1);

				// Only set the start tag if it's not an end tag :)
				if (tag_type < HTT_TAG_SWITCH)
					start_in_tag = tag_type;
			}
		}
	}
	status = fragments.Update(text.CStr(), text_len, info->rtl, FALSE, TRUE, FALSE, info->doc, info->original_fontnumber, info->preferred_script, FALSE, info->coloured ? &start_in_tag : NULL);
#else
	status = fragments.Update(text.CStr(), text_len, info->rtl, FALSE, TRUE, FALSE, info->doc, info->original_fontnumber, info->preferred_script, FALSE);
#endif

	if (OpStatus::IsError(status))
	{
		OP_DELETEA(line_info);
		line_info = NULL;
		num_lines = 0;
		return status;
	}

	// IMPORTANT: don't return without calling EndAccurateFontSize
	if (info->use_accurate_font_size)
		info->vis_dev->BeginAccurateFontSize();

	// Update each fragments width
	unsigned int i;
	int used_width = 0;
	for(i = 0; i < fragments.GetNumFragments(); i++)
	{
		OP_TEXT_FRAGMENT* frag = fragments.Get(i);
		uni_char* word = text.CStr() + frag->start;
		info->vis_dev->SetFont(frag->wi.GetFontNumber());

#ifdef MULTILABEL_RICHTEXT_SUPPORT
		SetStyle(info, frag);
#endif // MULTILABEL_RICHTEXT_SUPPORT

		if (frag->wi.IsTabCharacter())
		{
			frag->wi.SetWidth(GetTabWidth(used_width, info->vis_dev));
			frag->wi.SetLength(1);
		}
		else
		{
			int text_format = 0;
			if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
				text_format |= TEXT_FORMAT_REVERSE_WORD;

			UINT32 word_width = FragmentExtentFromTo(frag, word, 0, frag->wi.GetLength(), info->vis_dev, text_format);

			while (word_width > WORD_INFO_WIDTH_MAX)
			{
				used_width += WORD_INFO_WIDTH_MAX;

				status = fragments.Split(info, text.CStr(), i, WORD_INFO_WIDTH_MAX, word_width);

				if (status == OpStatus::OK)
					i++;
				else
					break; // NB: it is not safe to return

				if (word_width <= WORD_INFO_WIDTH_MAX)
					frag = fragments.Get(i);
			}
			if (status == OpStatus::ERR_NO_MEMORY)
				break;
			else
				// If the word was too long, we should have managed to split it.
				OP_ASSERT(OpStatus::IsSuccess(status));

			OP_ASSERT(word_width <= WORD_INFO_WIDTH_MAX);
			frag->wi.SetWidth(MIN(word_width, WORD_INFO_WIDTH_MAX));
		}
		used_width += frag->wi.GetWidth();
	}

	if (info->use_accurate_font_size)
		info->vis_dev->EndAccurateFontSize();

	if (status == OpStatus::ERR_NO_MEMORY)
		return status; // Must be after EndAccurateFontSize() !

	// DEBUG
	/*for(i = 0; i < fragments.GetNumFragments(); i++)
	{
		OP_TEXT_FRAGMENT* frag = fragments.Get(i);
		const uni_char* word = &text.CStr()[frag->start];
		BYTE propstart = GetLineBreakingProperty(*word);
		BYTE propend = GetLineBreakingProperty(word[frag->wi.GetLength() - 1]);

		//	LINEBREAK_ALLOW_BEFORE=0x01,
		//	LINEBREAK_ALLOW_AFTER=0x02,
		//	LINEBREAK_PROHIBIT_BEFORE=0x04,
		//	LINEBREAK_PROHIBIT_AFTER=0x08,

		//	LINEBREAK_AGGRESSIVE_ALLOW_BEFORE=0x10,
		//	LINEBREAK_AGGRESSIVE_ALLOW_AFTER=0x20

		uni_char buf[2048]; // ARRAY OK 2009-01-27 emil
		uni_sprintf(buf, UNI_L("\"%s"), word);
		uni_sprintf(buf+frag->wi.GetLength()+1, UNI_L("\"   %d %d\n"), (int)propstart, (int)propend);
		OutputDebugString(buf);
	}*/

	Layout(info, used_width, propagate_height);
	return OpStatus::OK;
}

void OpTCBlock::Layout(OP_TCINFO* info, int virtual_width, BOOL propagate_height)
{
	fragments.ResetOrder();

	int old_block_width = block_width;

	// Optimization for very long lines if wrapping is off.
	block_width = virtual_width;

	OP_DELETEA(line_info);
	line_info = NULL;
	num_lines = 0;

	// Layout
	LayoutTraverser traverser;
	Traverse(info, &traverser, FALSE, TRUE, 0);

	// Update position
	UpdatePosition(info, traverser.block_width, traverser.block_height, propagate_height);

	// Update total_width
	int total_block_width = block_width;
	if (block_width > info->tc->max_block_width)
		info->tc->max_block_width = block_width;

	if (info->justify != JUSTIFY_LEFT)
		total_block_width = MAX(block_width, info->tc->visible_width);

	if (total_block_width > info->tc->total_width)
	{
		info->tc->total_width = total_block_width;
	}
	else if (block_width < old_block_width && old_block_width == info->tc->max_block_width)
	{
		// Reset total_width and check for the widest block.
		// Also update the max_block_width here
		info->tc->total_width = 0;
		info->tc->max_block_width = 0;
		OpTCBlock* block = info->tc->FirstBlock();
		while (block)
		{
			total_block_width = MAX(block->block_width, info->tc->visible_width);
			if (total_block_width > info->tc->total_width)
			{
				info->tc->total_width = total_block_width;
			}
			if (block->block_width > info->tc->max_block_width)
				info->tc->max_block_width = block->block_width;

			block = (OpTCBlock*) block->Suc();
		}
	}
}

void OpTCBlock::Traverse(OP_TCINFO* info, OpTCBlockTraverser* traverser, BOOL visual_order, BOOL layout, int line)
{
	traverser->block = this;
	traverser->translation_x = 0;
	traverser->translation_y = 0;

	int line_wrap_width = info->tc->visible_width;
	if (info->tc->m_layout_width > 0 && info->tc->m_layout_width < line_wrap_width)
		line_wrap_width = info->tc->m_layout_width;

	const uni_char* text = this->text.CStr();
	int i = 0, j, local_block_width = 0, current_line = 0;
	BOOL calculating_line_info = FALSE;

	if (!line_info && num_lines > MIN_WRAPPED_LINES_TO_CACHE)
	{
		// Create linecache
		line_info = OP_NEWA(LINE_CACHE_INFO, num_lines);
		if (line_info)
		{
			op_memset(line_info, 0, sizeof(LINE_CACHE_INFO) * num_lines);
			calculating_line_info = TRUE;
		}
	}
	else if (line_info && line)
	{
		// We have cached linestarts. Jump directly to line.
		traverser->translation_y += info->tc->line_height * line;
		OP_ASSERT(line < num_lines);
		if (line < num_lines)
		{
			i = line_info[line].start;
			current_line = line;
		}
	}

	do
	{
		// === Check how many fragments that fits on this line ===
		int line_width = 0;
		int first_frag_on_line = i;

		if (line_info && !calculating_line_info)
		{
			OP_ASSERT(current_line < num_lines); ///< PLEASE, send me stack and steps to reproduce this if you get it. (emil@opera.com)

			// Use line info cache for line_width and num_fragments on this line.
			line_width = line_info[current_line].width;

			// Get num fragments on this line from the start of the next line.
			if (current_line < num_lines - 1)
			{
				OP_ASSERT(line_info[current_line].start <= line_info[current_line + 1].start);
				i += line_info[current_line + 1].start - line_info[current_line].start;
			}
			else
			{
				OP_ASSERT(line_info[current_line].start <= (int) fragments.GetNumFragments());
				i += fragments.GetNumFragments() - line_info[current_line].start;
			}
		}
		else if (info->wrap && (num_lines != 1 || layout))
		{
			// Must go through this line and see how much will fit, and apply wordwrapping properties.
			int last_wrap_point = i;
			int last_wrap_point_line_width = 0;

			while (i < (int)fragments.GetNumFragments())
			{
				OP_TEXT_FRAGMENT* frag = fragments.Get(i);

				// Update the tabs width before use. We now know where it is placed on the line.
				if (frag->wi.IsTabCharacter())
				{
					info->vis_dev->SetFont(frag->wi.GetFontNumber());
#ifdef MULTILABEL_RICHTEXT_SUPPORT
					SetStyle(info, frag);
#endif // MULTILABEL_RICHTEXT_SUPPORT
					frag->wi.SetWidth(GetTabWidth(line_width, info->vis_dev));
				}

				int fragment_test_width = frag->wi.GetWidth();

				BOOL is_softhyphen = (text[frag->start] == 173);

				if (!frag->wi.HasTrailingWhitespace() && i < (int)fragments.GetNumFragments() - 1)
				{
					// The last whitespace before another bidicategory gets its own word (from GetNextTextFragment).
					// We must make sure that space is included with this word when checking for overflow.
					OP_TEXT_FRAGMENT* next_frag = fragments.Get(i + 1);
					if (uni_html_space(text[next_frag->start]) && next_frag->wi.HasTrailingWhitespace())
					{
						if (next_frag->wi.IsTabCharacter())
						{
							// We don't know yet where the next tab will end up, so we can't rely on its width in the wrap-test. Must use full tabwidth
							// to ensure we don't get different results next time we traverse this block (because the width will then be set).
							info->vis_dev->SetFont(next_frag->wi.GetFontNumber());
#ifdef MULTILABEL_RICHTEXT_SUPPORT
							SetStyle(info, frag);
#endif // MULTILABEL_RICHTEXT_SUPPORT
							fragment_test_width += GetTabWidth(0, info->vis_dev);
						}
						else
							fragment_test_width += next_frag->wi.GetWidth();
					}
				}

				// == Check if we are allowed to wrap here ============================================
#ifndef USE_UNICODE_LINEBREAK
				int current_num_frag_on_line = i - first_frag_on_line;
				if (current_num_frag_on_line > 0)
				{
					OP_TEXT_FRAGMENT* last_frag = fragments.Get(i - 1);

					const uni_char* last_word = &text[last_frag->start];
					BOOL prohibit_after_last = GetLineBreakingProperty(last_word[last_frag->wi.GetLength() - 1]) & LINEBREAK_PROHIBIT_AFTER;
					BOOL allow_after_last = GetLineBreakingProperty(last_word[last_frag->wi.GetLength() - 1]) & LINEBREAK_ALLOW_AFTER;
					BOOL prohibit_before_next = FALSE;
					BOOL allow_before_next = FALSE;
					BOOL before_css_punctuation = FALSE;
					if (i < (int)fragments.GetNumFragments())
					{
						OP_TEXT_FRAGMENT* frag = fragments.Get(i);
						const uni_char* word = &text[frag->start];
						prohibit_before_next = GetLineBreakingProperty(*word) & LINEBREAK_PROHIBIT_BEFORE;
						allow_before_next = GetLineBreakingProperty(*word) & LINEBREAK_ALLOW_BEFORE;

						// Note: Should uni_html_space be used below, or something else?
						if (!allow_after_last && uni_ispunct(text[last_frag->start]) && !uni_html_space(text[frag->start]))
							before_css_punctuation = TRUE;
					}
					// If not prohibited...
					if (!(prohibit_after_last || prohibit_before_next || is_softhyphen || before_css_punctuation))
					{
						// Only if it's really permitted...
						if (last_frag->wi.HasTrailingWhitespace() || allow_before_next || allow_after_last)
						{
							// We allow wrap here.
							last_wrap_point = i;
							last_wrap_point_line_width = line_width;
						}
					}
				}
#else
				int current_num_frag_on_line = i - first_frag_on_line;
				if (current_num_frag_on_line > 0)
				{
					if (frag->wi.CanLinebreakBefore())
					{
						// We allow wrap here.
						last_wrap_point = i;
						last_wrap_point_line_width = line_width;
					}
				}
#endif

				// == Check if we need to wrap =======================================================

				if (line_width + fragment_test_width > line_wrap_width)
				{
					if (info->aggressive_wrap ||
						(last_wrap_point > first_frag_on_line && i > first_frag_on_line))
					{
						if (info->aggressive_wrap && last_wrap_point == first_frag_on_line && frag->wi.GetLength())
						{
							OP_ASSERT(line_width <= line_wrap_width);
							OP_STATUS status = OpStatus::OK;
							int width_left = line_wrap_width - line_width;
							UINT32 next_width;
							if (!frag->wi.IsTabCharacter() &&
								frag->wi.GetWidth() > width_left &&
								(status = fragments.Split(info, text, i, width_left, next_width) == OpStatus::OK))
							{
								OP_TEXT_FRAGMENT* next_frag = fragments.Get(i + 1);
								next_frag->wi.SetWidth(MIN(next_width, WORD_INFO_WIDTH_MAX));
								frag = fragments.Get(i);
								last_wrap_point = i + 1;
								last_wrap_point_line_width = line_width + frag->wi.GetWidth();
							}
							else
							{
								if (i == first_frag_on_line)
								{
									last_wrap_point = i + 1;
									last_wrap_point_line_width = frag->wi.GetWidth();
									OP_ASSERT(line_wrap_width == 0 || status != OpStatus::OK ||
											  last_wrap_point_line_width <= line_wrap_width);
								}
								else
									last_wrap_point = i;

								if (OpStatus::IsError(status))
								{
									if (OpStatus::IsMemoryError(status))
										OP_ASSERT(!"OOM when trying to split text fragment");
									else
										OP_ASSERT(!"failed to split text fragment");
								}
							}
						}

						i = last_wrap_point;
						line_width = last_wrap_point_line_width;

						// Make sure that the ending linebreak or soft hyphen with no width is included.
						if (i == ((int) fragments.GetNumFragments()) - 1 && frag->wi.GetWidth() == 0)
							i++;
						break;
					}
				}

				// This fragment fits (or no allowed wrap-point has been found yet).
				if (!is_softhyphen)
					line_width += frag->wi.GetWidth();
				i++;
			}
		}
		else
		{
			// Optimization. block_width is calculated in UpdateAndLayout which can be reused when wrapping is off or there is only one line.
			line_width = block_width;
			i = fragments.GetNumFragments();
		}

		int num_frag_on_line = i - first_frag_on_line;

		// === Calculate justify offset ===
		INT32 ofs_x = 0;
		if (info->justify == JUSTIFY_CENTER)
			ofs_x = (line_wrap_width - line_width) / 2;
		else if (info->justify == JUSTIFY_RIGHT)
			ofs_x = line_wrap_width - line_width - 1;
		ofs_x = MAX(ofs_x, 0);
		traverser->translation_x = ofs_x;

		// === Traverse all fragments on the line ===
		for(j = 0; j < num_frag_on_line; j++)
		{
			OP_TEXT_FRAGMENT* frag = visual_order ? fragments.GetByVisualOrder(first_frag_on_line + j) : fragments.Get(first_frag_on_line + j);

			BOOL collapsed = FALSE;
			if (text[frag->start] == 173)
			{
				// Soft hyphens should collapse if not last on wrapped line.
				if (!(info->wrap && j == num_frag_on_line - 1 &&
					  first_frag_on_line + j != ((int) fragments.GetNumFragments()) - 1))
					collapsed = TRUE;
			}

			if (frag->wi.GetLength())
			{
				OP_TC_BT_STATUS status = traverser->HandleWord(info, frag, collapsed);

				// Must finish all lines if calculating_line_info is TRUE, so line_info cache is initialized properly.
				if (!calculating_line_info)
				{
					if (status == OP_TC_BT_STATUS_STOP_ALL)
						return;
					else if (status == OP_TC_BT_STATUS_STOP_LINE)
						break;
				}
			}

			if (!collapsed)
				traverser->translation_x += frag->wi.GetWidth();
		}

		if (line_info)
		{
			OP_ASSERT(current_line < num_lines); ///< PLEASE, send me stack and steps to reproduce this if you get it. (emil@opera.com)
			if (current_line < num_lines)
			{
				OP_ASSERT(calculating_line_info || line_info[current_line].width == line_width);
				OP_ASSERT(calculating_line_info || line_info[current_line].start == first_frag_on_line);
				if (calculating_line_info)
				{
					line_info[current_line].width = line_width;
					line_info[current_line].start = first_frag_on_line;
				}
				current_line++;
			}
			else
			{
				// Be failsafe, even if the 'impossible' happen.
				OP_ASSERT(FALSE);
				OP_DELETEA(line_info);
				line_info = NULL;
			}
		}

		traverser->HandleLine(info, first_frag_on_line, num_frag_on_line);

		if (traverser->translation_x - ofs_x > local_block_width)
			local_block_width = traverser->translation_x - ofs_x;

		traverser->translation_y += info->tc->line_height;

	} while (i < (int)fragments.GetNumFragments());

#ifdef _DEBUG
	// Check that the cached info is correct
	if (calculating_line_info && line_info)
	{
		OP_ASSERT(line_info[0].start == 0);
		for(i = 1; i < num_lines; i++)
		{
			OP_ASSERT(line_info[i - 1].start < line_info[i].start);
		}
	}
#endif

	if (layout)
	{
		traverser->block_width = local_block_width;
		traverser->block_height = traverser->translation_y;
	}
}

void OpTCBlock::UpdatePosition(OP_TCINFO* info, int new_w, int new_h, BOOL propagate_height)
{
	info->tc->InvalidateBlock(this);

	// Update position to just after the previous block.
	y = 0;
	OpTCBlock* pred = (OpTCBlock*) Pred();
	if (pred)
		y = pred->y + pred->block_height;

	block_width = new_w;
	block_height = new_h;

	info->tc->InvalidateBlock(this);

	if (propagate_height)
	{
		// FIX: invalidate slack after last block if shrunk.
		OpTCBlock* suc = (OpTCBlock*) Suc();

		if (suc && suc->y != y + block_height)
		{
			OpTCBlock* block = (OpTCBlock*) Suc();
			while (block)
			{
				block->UpdatePosition(info, block->block_width, block->block_height, FALSE);
				block = (OpTCBlock*) block->Suc();
			}
		}
	}

	// Update total_height
	info->tc->total_height = info->tc->LastBlock()->y + info->tc->LastBlock()->block_height;
	int visible_line_height = MAX(info->tc->line_height, (int) info->font_height);
	info->tc->total_height -= info->tc->line_height;
	info->tc->total_height += visible_line_height;
}

const uni_char* OpTCBlock::CStr(int ofs)
{
	if (!text.CStr())
		return UNI_L(""); // Instead of risk returning NULL
	return &text.CStr()[ofs];
}

int OpTCBlock::GetStartLineFromOfs(OP_TCINFO* info, int ofs)
{
	if (line_info)
	{
		for(int i = 1; i < num_lines; i++)
		{
			OP_TEXT_FRAGMENT* frag = fragments.Get(line_info[i].start);
			if (ofs <= frag->start)
			{
				return i - 1;
			}
		}
		return num_lines - 1;
	}
	return 0;
}

int OpTCBlock::GetStartLineFromY(OP_TCINFO* info, int y)
{
	if (info->tc->line_height == 0)
		return 0; // Just to be sure we don't divide by zero.
	int line = (y - this->y) / info->tc->line_height;
	return MAX(line, 0);
}
