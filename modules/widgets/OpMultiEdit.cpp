/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpEditCommon.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpTextCollection.h"
#include "modules/widgets/OpResizeCorner.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpClipboard.h"
#include "modules/pi/OpWindow.h"
#include "modules/layout/bidi/characterdata.h"
#include "modules/doc/html_doc.h"
#include "modules/forms/piforms.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/unicode/unicode_stringiterator.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpFont.h"
#include "modules/doc/frm_doc.h"
#include "modules/style/css.h"
#include "modules/display/fontdb.h"
#include "modules/display/styl_man.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/util/str.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/dialogs/SimpleDialog.h"
# include "adjunct/quick/panels/NotesPanel.h"
#endif //QUICK

#ifdef GRAB_AND_SCROLL
# include "modules/display/VisDevListeners.h"
#endif

#ifdef _MACINTOSH_
#include "platforms/mac/pi/CocoaOpWindow.h"
#endif

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/pi/OpDragObject.h"
# include "modules/dragdrop/dragdrop_data_utils.h"
#endif // DRAG_SUPPORT

#define REPORT_IF_OOM(status)	if (OpStatus::IsMemoryError(status)) \
								{ \
									ReportOOM(); \
								}

#ifdef WIDGETS_IME_SUPPORT
BOOL SpawnIME(VisualDevice* vis_dev, const uni_char* istyle, OpView::IME_MODE mode, OpView::IME_CONTEXT context);
OpView::IME_CONTEXT GetIMEContext(FormObject* form_obj);
#endif // WIDGETS_IME_SUPPORT

// Scrollbarlistener

void OpMultilineEdit::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	if (FormObject* form_obj = widget->GetFormObject(TRUE))
		form_obj->UpdatePosition();

	INT32 dx = 0, dy = 0;
	OpScrollbar* scrollbar = (OpScrollbar*) widget;
	if (scrollbar->IsHorizontal())
		dx = new_val - old_val;
	else
		dy = new_val - old_val;

	OpRect rect = GetDocumentRect(TRUE);
	OpWidgetInfo* info = widget->GetInfo();
	info->AddBorder(this, &rect);
	rect.x += GetPaddingLeft();
	rect.y += GetPaddingTop();
	rect.width = GetVisibleWidth();
	rect.height = GetVisibleHeight();

	if (LeftHandedUI() && y_scroll->IsVisible())
		rect.x += y_scroll->GetWidth();

	VisualDevice* vis_dev = scrollbar->GetVisualDevice();

	if (IsForm() == FALSE)
	{
		vis_dev->ScrollRect(rect, -dx, -dy);
		return;
	}

	OP_ASSERT(IsForm());
	if(listener)
		listener->OnScroll(this, old_val, new_val, caused_by_input);

	if (caused_by_input && HasCssBackgroundImage() == FALSE && !(LeftHandedUI() && corner->IsVisible()))
	{
		OpRect clip_rect;
		if (GetIntersectingCliprect(clip_rect))
		{
			rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
			rect.IntersectWith(clip_rect);
			vis_dev->ScrollRect(rect, -dx, -dy);
			return;
		}
	}

	vis_dev->Update(rect.x, rect.y, rect.width, rect.height);
}

// == OpMultilineEdit ===========================================================

DEFINE_CONSTRUCT(OpMultilineEdit)

OpMultilineEdit::OpMultilineEdit()
	: m_ghost_text(0)
	, caret_wanted_x(0)
	, caret_blink(1)
	, is_selecting(0)
	, selection_highlight_type(VD_TEXT_HIGHLIGHT_TYPE_SELECTION)
	, multi_edit(NULL)
	, x_scroll(NULL)
	, y_scroll(NULL)
	, corner(NULL)
	, alt_charcode(0)
	, is_changing_properties(0)
	, needs_reformat(0)
	, scrollbar_status_x(SCROLLBAR_STATUS_AUTO)
	, scrollbar_status_y(SCROLLBAR_STATUS_ON)
#ifdef WIDGETS_IME_SUPPORT
	, im_pos(0)
	, imstring(NULL)
#endif
#ifdef IME_SEND_KEY_EVENTS
	, m_fake_keys(0)
#endif // IME_SEND_KEY_EVENTS
	, m_packed_init(0)
{
	m_packed.is_wrapping = TRUE;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_packed.never_had_focus = TRUE;
	m_packed.enable_spellcheck_later = TRUE;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	SetTabStop(TRUE);

	multi_edit = OP_NEW(OpTextCollection, (this));
	if (multi_edit == NULL)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	OP_STATUS status1 = OpScrollbar::Construct(&x_scroll, TRUE);
	OP_STATUS status2 = OpScrollbar::Construct(&y_scroll, FALSE);
	corner = OP_NEW(OpWidgetResizeCorner, (this));

	if (OpStatus::IsError(status1) || OpStatus::IsError(status2) || !corner)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	x_scroll->SetVisibility(FALSE);
	y_scroll->SetVisibility(FALSE);

	corner->SetVisibility(IsResizable());

	x_scroll->SetListener(this);
	y_scroll->SetListener(this);

	AddChild(corner, TRUE);
	AddChild(x_scroll, TRUE);
	AddChild(y_scroll, TRUE);

#ifdef SKIN_SUPPORT
	GetBorderSkin()->SetImage("MultilineEdit Skin");
	SetSkinned(TRUE);
#endif

#ifdef WIDGETS_IME_SUPPORT
	SetOnMoveWanted(TRUE);
#endif

#ifdef IME_SEND_KEY_EVENTS
	previous_ime_len = 0;
#endif // IME_SEND_KEY_EVENTS

	SetPadding(1, 1, 1, 1);
}

OpMultilineEdit::~OpMultilineEdit()
{
}

void OpMultilineEdit::OnShow(BOOL show)
{
	EnableCaretBlinking(show);
}

void OpMultilineEdit::EnableCaretBlinking(BOOL enable)
{
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	StopTimer();

	// we don't need any caret if it's read only or not focused
	if(enable && !m_packed.is_readonly && GetFocused() == this)
		StartTimer(GetInfo()->GetCaretBlinkDelay());
	else
		caret_blink = 1;	// avoid a visible non-blinking caret

#endif // !WIDGETS_DISABLE_CURSOR_BLINKING
}

void OpMultilineEdit::OnRemoving()
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring && vis_dev)
		// This should teoretically not be needed since OpWidget::OnKeyboardInputLost will do it. But will keep it to be extra safe.
		vis_dev->view->GetOpView()->AbortInputMethodComposing();
#endif // WIDGETS_IME_SUPPORT
}

void OpMultilineEdit::OnDeleted()
{
	OP_DELETE(multi_edit);
	op_free(m_ghost_text);
	m_ghost_text = 0;
	m_packed.ghost_mode = FALSE;
}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpMultilineEdit::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;
	OpMultilineEdit *widget;

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&widget));
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

void OpMultilineEdit::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, GetType(), w, h, cols, rows);
}

INT32 OpMultilineEdit::GetTextLength(BOOL insert_linebreak)
{
	return m_packed.ghost_mode ? 0 : multi_edit->GetTextLength(insert_linebreak);
}

void OpMultilineEdit::GetText(uni_char *buf, INT32 buflen, INT32 offset, BOOL insert_linebreak)
{
	if (m_packed.ghost_mode)
	{
		*buf = 0;
		return;
	}

	multi_edit->GetText(buf, buflen, offset, insert_linebreak);
}

OP_STATUS OpMultilineEdit::GetText(OpString &str, BOOL insert_linebreak)
{
	if (m_packed.ghost_mode)
	{
		str.Empty();
		return OpStatus::OK;
	}

	INT32 len = GetTextLength(insert_linebreak);
	str.Empty();
	uni_char* buf = str.Reserve(len + 1);
	RETURN_OOM_IF_NULL(buf);
	GetText(buf, len, 0, insert_linebreak);
	buf[len] = 0;
	return OpStatus::OK;
}

BOOL OpMultilineEdit::IsEmpty()
{
	if (!multi_edit->FirstBlock())
		return TRUE;
	return multi_edit->FirstBlock() == multi_edit->LastBlock() && !multi_edit->FirstBlock()->text.Length();
}

void OpMultilineEdit::OnResize(INT32* new_w, INT32* new_h)
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
		vis_dev->GetOpView()->SetInputMoved();
#endif // WIDGETS_IME_SUPPORT

	if (UpdateScrollbars())
	{
		UpdateFont();
		ReformatNeeded(FALSE);
	}

	multi_edit->UpdateCaretPos();
}

void OpMultilineEdit::OnMove()
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
		vis_dev->GetOpView()->SetInputMoved();
#endif // WIDGETS_IME_SUPPORT
}

OP_STATUS OpMultilineEdit::SetTextInternal(const uni_char* text, BOOL use_undo_stack)
{
	INT32 text_len;
	if (text == NULL)
	{
		text = UNI_L("");
		text_len = 0;
	}
	else
		text_len = uni_strlen(text);

	UpdateFont();
	is_selecting = 0;

	// reset ghost mode flag - fixes CORE-23010
	m_packed.ghost_mode = FALSE;

	// first call to OpMultilineEdit::SetText needs to result in a call
	// to OpTextCollection::SetText so that everything's initialized
	// properly (see CORE-24067, CORE-28277, CORE-30459)
	if (multi_edit->FirstBlock())
	{
		// early bail when contents haven't changed - CORE-24067
		if (*text ? !CompareText(text) : IsEmpty())
			return OpStatus::OK;

		OP_ASSERT(text_len != 0 || !IsEmpty());
	}

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	//don't use undo bufer if user has not input anything
	if (use_undo_stack && !HasReceivedUserInput())
		use_undo_stack = FALSE;

	if (use_undo_stack && !IsEmpty())
	{
		//store the whole text in a replace event
		//these steps are a bit expensive, so only do them if
		//we really need to handle undo/redo, hence previous check

		//1st get a copy of all text
		INT32 current_text_length = GetTextLength(FALSE);
		TempBuffer current_text;
		RETURN_IF_ERROR(current_text.Expand(current_text_length));

		current_text_length = multi_edit->GetText(current_text.GetStorage(), current_text_length, 0, FALSE);

		OP_TCINFO* info = multi_edit->listener->TCGetInfo();
		INT32 s_start = multi_edit->sel_start.GetGlobalOffset(info);
		INT32 s_stop = multi_edit->sel_stop.GetGlobalOffset(info);
		INT32 caret = multi_edit->caret.GetGlobalOffset(info);

		//2nd save the text on the undo history
		if (text_len > 0)
		{
			RETURN_IF_ERROR(multi_edit->undo_stack.SubmitReplace(MAX(caret, text_len), s_start, s_stop,
					current_text.GetStorage(), current_text_length, text, text_len));
		}
		else
		{
			RETURN_IF_ERROR(multi_edit->undo_stack.SubmitRemove(caret, 0, current_text_length, current_text.GetStorage()));
		}
		//text was saved, don't save again
		use_undo_stack = FALSE;
	}
	else if (!use_undo_stack)
	{
		multi_edit->undo_stack.Clear();
	}
#endif

	OP_STATUS status = multi_edit->SetText(text, text_len, use_undo_stack);

#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
	{
		imstring->Set(text, uni_strlen(text));
		vis_dev->GetOpView()->SetInputTextChanged();
	}
#endif // WIDGETS_IME_SUPPORT
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
#endif

	return status;
}

OP_STATUS OpMultilineEdit::InsertText(const uni_char* text, BOOL use_undo_stack)
{
	if (!text)
		return OpStatus::OK;

	if (m_packed.ghost_mode)
	{
		OP_STATUS status = SetText(0);
		// this shouldn't fail. or can it?
		OpStatus::Ignore(status);
		OP_ASSERT(OpStatus::IsSuccess(status));
		OP_ASSERT(!m_packed.ghost_mode);
	}

	UpdateFont();
	return multi_edit->InsertText(text, uni_strlen(text), use_undo_stack);
}

void OpMultilineEdit::SetReadOnly(BOOL readonly)
{
	m_packed.is_readonly = !!readonly;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckIfNotUsable();
#endif // INTERNAL_SPELLCHECK_SUPPORT

	EnableCaretBlinking(!readonly);
}

void OpMultilineEdit::SetWrapping(BOOL status)
{
	if (m_packed.is_wrapping == !!status)
		return;

	m_packed.is_wrapping = !!status;

	UpdateScrollbars();
	ReformatNeeded(FALSE);
}
void OpMultilineEdit::SetAggressiveWrapping(BOOL break_word)
{
	if (break_word == GetAggressiveWrapping())
		return;

	if (m_packed.is_wrapping)
		m_overflow_wrap = break_word ? CSS_VALUE_break_word : CSS_VALUE_normal;
	UpdateScrollbars();
	ReformatNeeded(TRUE);
}
BOOL OpMultilineEdit::GetAggressiveWrapping()
{
	return m_packed.is_wrapping && m_overflow_wrap == CSS_VALUE_break_word;
}

void OpMultilineEdit::SelectText()
{
	if (m_packed.ghost_mode)
		return;

	UpdateFont();
	multi_edit->SelectAll();
	selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;

#ifdef RANGESELECT_FROM_EDGE
	m_packed.determine_select_direction = TRUE;
#endif // RANGESELECT_FROM_EDGE
}

void OpMultilineEdit::SetFlatMode()
{
	SetHasCssBorder(TRUE); // So we get rid of the border
	SetReadOnly(TRUE);
	SetTabStop(FALSE);
	m_packed.flatmode = TRUE;
	SetScrollbarStatus(SCROLLBAR_STATUS_OFF);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckIfNotUsable();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpMultilineEdit::SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status)
{
	if (scrollbar_status_x != status || scrollbar_status_y != status)
	{
		scrollbar_status_x = status;
		scrollbar_status_y = (status == SCROLLBAR_STATUS_AUTO ? SCROLLBAR_STATUS_ON : status);
		if (UpdateScrollbars())
			ReformatNeeded(FALSE);
	}
}

void OpMultilineEdit::SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status_x, WIDGET_SCROLLBAR_STATUS status_y)
{
	if (scrollbar_status_x != status_x || scrollbar_status_y != status_y)
	{
		scrollbar_status_x = status_x;
		scrollbar_status_y = status_y;
		if (UpdateScrollbars())
			ReformatNeeded(FALSE);
	}
}

void OpMultilineEdit::SetLabelMode(BOOL allow_selection)
{
	SetFlatMode();
	m_packed.is_label_mode = !allow_selection;
}

#ifndef HAS_NO_SEARCHTEXT

BOOL GetMatchedText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words,
					int &start_idx, int end_idx, int elm_len, const uni_char* ctxt)
{
	if (forward)
	{
		if (end_idx >= 0)
		{
			for (int i=start_idx; i<=end_idx; i++)
				if ((match_case && uni_strncmp(ctxt+i, txt, len) == 0) ||
					(!match_case && uni_strnicmp(ctxt+i, txt, len) == 0))
				{
					if (words)
					{
						if ((i == 0 || *(ctxt+i-1) == ' ') &&
							(i == end_idx || *(ctxt+i+len) == ' ' || *(ctxt+i+len) == '.' || *(ctxt+i+len) == ',' || *(ctxt+i+len) == '?' || *(ctxt+i+len) == '!' || *(ctxt+i+len) == ':'))
						{
							start_idx = i;
							return TRUE;
						}
					}
					else
					{
						start_idx = i;
						return TRUE;
					}
				}
		}
	}
	else
	{
		if (end_idx > start_idx)
			end_idx = start_idx;
		if (end_idx >= 0)
		{
			for (int i=end_idx; i>=0; i--)
				if ((match_case && uni_strncmp(ctxt+i, txt, len) == 0) ||
					(!match_case && uni_strnicmp(ctxt+i, txt, len) == 0))
				{
					if (words)
					{
						if ((i == 0 || *(ctxt+i-1) == ' ') &&
							(i == elm_len - len || *(ctxt+i+len) == ' ' || *(ctxt+i+len) == '.' || *(ctxt+i+len) == ',' || *(ctxt+i+len) == '?' || *(ctxt+i+len) == '!' || *(ctxt+i+len) == ':'))
						{
							start_idx = i;
							return TRUE;
						}
					}
					else
					{
						start_idx = i;
						return TRUE;
					}
				}
		}
	}
	return FALSE;
}

BOOL OpMultilineEdit::SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, SEARCH_TYPE type, BOOL select_match, BOOL scroll_to_selected_match, BOOL wrap, BOOL next)
{
	if( !txt )
	{
		return FALSE;
	}

	UpdateFont();

	int search_text_len = uni_strlen(txt);
	OpTCBlock* start_block = multi_edit->caret.block;
	int start_ofs = multi_edit->caret.ofs;

	if (type == SEARCH_FROM_BEGINNING)
	{
		start_block = multi_edit->FirstBlock();
		start_ofs = 0;
	}
	else if (type == SEARCH_FROM_END)
	{
		start_block = multi_edit->LastBlock();
		start_ofs = start_block->text.Length();
	}
	else if (!next && forward)
	{
		// We should be able to find the same match again. Move start back to selection start (which would be the previous match).
		if (multi_edit->sel_start.block)
		{
			start_block = multi_edit->sel_start.block;
			start_ofs = multi_edit->sel_start.ofs;
		}
	}

	int ofs = start_ofs;
	OpTCBlock* block = start_block;
	while(block)
	{
		int block_len = block->text.Length();
		int end_ofs = block_len - len;

		BOOL found = GetMatchedText(txt, len, forward, match_case, words,
									ofs, end_ofs, block_len, block->CStr());
		if (found)
		{
			if (!forward && block == start_block && ofs == start_ofs - search_text_len && next)
			{
				// We found the same, we need to decrease idx and try again.
				ofs -= search_text_len;
			}
			else
			{
				if (select_match)
				{
					OpTCOffset tmp;
					tmp.block = block;
					tmp.ofs = ofs;
					INT32 glob_pos = tmp.GetGlobalOffset(TCGetInfo());

					SelectSearchHit(glob_pos, glob_pos + search_text_len, TRUE);
					if (scroll_to_selected_match)
						ScrollIfNeeded(TRUE);

				}
				return TRUE;
			}
		}
		else if (forward)
		{
			block = (OpTCBlock*)block->Suc();
			ofs = 0;
		}
		else if (!forward)
		{
			block = (OpTCBlock*)block->Pred();
			if (block)
				ofs = block->text.Length();
		}
	}
	return FALSE;
}

#endif // !HAS_NO_SEARCHTEXT

void OpMultilineEdit::SelectNothing()
{
	multi_edit->BeginChange();
	multi_edit->SelectNothing(TRUE);
	multi_edit->EndChange();
	selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
}

/* virtual */
void OpMultilineEdit::GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL line_normalized)
{
	if (multi_edit->HasSelectedText())
	{
		OP_TCINFO* info = TCGetInfo();
		if (!line_normalized)
		{
			start_ofs = multi_edit->sel_start.GetGlobalOffset(info);
			stop_ofs = multi_edit->sel_stop.GetGlobalOffset(info);
		}
		else
		{
			start_ofs = multi_edit->sel_start.GetGlobalOffsetNormalized(info);
			stop_ofs = multi_edit->sel_stop.GetGlobalOffsetNormalized(info);
		}

#ifdef RANGESELECT_FROM_EDGE
		if (m_packed.determine_select_direction)
			direction = SELECTION_DIRECTION_NONE;
		else
#endif // RANGESELECT_FROM_EDGE
		{
			INT32 caret_ofs;
			if (!line_normalized)
				caret_ofs = multi_edit->caret.GetGlobalOffset(info);
			else
				caret_ofs = multi_edit->caret.GetGlobalOffsetNormalized(info);

			if (caret_ofs == start_ofs)
				direction = SELECTION_DIRECTION_BACKWARD;
			else
				direction = SELECTION_DIRECTION_FORWARD;
		}
	}
	else
	{
		start_ofs = stop_ofs = 0;
		direction = SELECTION_DIRECTION_DEFAULT;
	}
}

/* virtual */
void OpMultilineEdit::SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction, BOOL line_normalized)
{
	if (m_packed.ghost_mode)
		return;

	BeforeAction();
	if (stop_ofs < start_ofs)
		stop_ofs = start_ofs;

	multi_edit->SetSelection(start_ofs, stop_ofs, TRUE, line_normalized);

#ifdef RANGESELECT_FROM_EDGE
	if (direction == SELECTION_DIRECTION_NONE)
		m_packed.determine_select_direction = TRUE;
	else
		m_packed.determine_select_direction = FALSE;
#endif // RANGESELECT_FROM_EDGE
	if (direction == SELECTION_DIRECTION_BACKWARD)
	{
		if (!line_normalized)
			multi_edit->SetCaretOfsGlobal(start_ofs);
		else
			multi_edit->SetCaretOfsGlobalNormalized(start_ofs);
	}
	else
	{
		if (!line_normalized)
			 multi_edit->SetCaretOfsGlobal(stop_ofs);
		else
			 multi_edit->SetCaretOfsGlobalNormalized(stop_ofs);
	}

	AfterAction();
	selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
}

void OpMultilineEdit::SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit)
{
	SetSelection(start_ofs, stop_ofs);
	selection_highlight_type = is_active_hit
		? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT
		: VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT;
}

BOOL OpMultilineEdit::IsSearchHitSelected()
{
	return (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
		|| (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
}

BOOL OpMultilineEdit::IsActiveSearchHitSelected()
{
	return (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
}

INT32 OpMultilineEdit::GetCaretOffset()
{
	return multi_edit->caret.GetGlobalOffset(TCGetInfo());
}

void OpMultilineEdit::SetCaretOffset(INT32 caret_ofs)
{
	multi_edit->SetCaretOfsGlobal(caret_ofs);
}

INT32 OpMultilineEdit::GetCaretOffsetNormalized()
{
	return multi_edit->caret.GetGlobalOffsetNormalized(TCGetInfo());
}

void OpMultilineEdit::SetCaretOffsetNormalized(INT32 caret_ofs)
{
	multi_edit->SetCaretOfsGlobalNormalized(caret_ofs);
}

void OpMultilineEdit::ReportCaretPosition()
{
		INT32 ofs_x, ofs_y;
		GetLeftTopOffset(ofs_x, ofs_y);

		int font_height = vis_dev->GetFontAscent() + vis_dev->GetFontDescent();
		int visible_line_height = MAX(multi_edit->line_height, font_height);

		GenerateHighlightRectChanged(OpRect(ofs_x + multi_edit->caret_pos.x,
											ofs_y + multi_edit->caret_pos.y,
											1,
											visible_line_height), TRUE);
}

void OpMultilineEdit::SetScrollbarColors(const ScrollbarColors &colors)
{
	x_scroll->scrollbar_colors = colors;
	y_scroll->scrollbar_colors = colors;
	corner->SetScrollbarColors(&colors);
}

BOOL IsUpDownAction(int action)
{
	switch(action)
	{
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_START:
		case OpInputAction::ACTION_RANGE_GO_TO_END:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_GO_TO_START:
		case OpInputAction::ACTION_GO_TO_END:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PAGE_UP:
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
		case OpInputAction::ACTION_RANGE_NEXT_LINE:
		case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PAGE_UP:
		case OpInputAction::ACTION_PAGE_DOWN:
		case OpInputAction::ACTION_NEXT_LINE:
		case OpInputAction::ACTION_PREVIOUS_LINE:
			return TRUE;
	}
	return FALSE;
}

void OpMultilineEdit::EditAction(OpInputAction* action)
{
	BOOL action_treated = FALSE;
	if (m_packed.is_readonly) // scroll, but don't move the caret
	{
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_RANGE_GO_TO_START:
			case OpInputAction::ACTION_GO_TO_START:
			case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
			case OpInputAction::ACTION_GO_TO_LINE_START:
				y_scroll->SetValue(y_scroll->limit_min, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_GO_TO_END:
			case OpInputAction::ACTION_GO_TO_END:
			case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
			case OpInputAction::ACTION_GO_TO_LINE_END:
				y_scroll->SetValue(y_scroll->limit_max, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_PAGE_UP:
			case OpInputAction::ACTION_PAGE_UP:
				OnScrollAction(-multi_edit->visible_height, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_PAGE_DOWN:
			case OpInputAction::ACTION_PAGE_DOWN:
				OnScrollAction(multi_edit->visible_height, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
			case OpInputAction::ACTION_PREVIOUS_LINE:
				OnScrollAction(-multi_edit->line_height, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_NEXT_LINE:
			case OpInputAction::ACTION_NEXT_LINE:
				OnScrollAction(multi_edit->line_height, TRUE);
				action_treated = TRUE;
				break;

			case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
			case OpInputAction::ACTION_NEXT_CHARACTER:
			case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
			case OpInputAction::ACTION_PREVIOUS_CHARACTER:
			case OpInputAction::ACTION_RANGE_NEXT_WORD:
			case OpInputAction::ACTION_NEXT_WORD:
			case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
			case OpInputAction::ACTION_PREVIOUS_WORD:
				action_treated = TRUE; // do nothing
				break;
		}
		if (action_treated == TRUE) // there might be some action to be treated later on
			return;
	}

	BOOL moving_caret = action->IsMoveAction();
	/* FALSE if non-caret action didn't alter content. */
	BOOL local_input_changed = !action->IsRangeAction();

	// allow readonly widgets to receive the navigation keys
	if (m_packed.is_readonly && !moving_caret)
		return;

	OpPoint old_caret_pos = multi_edit->caret_pos;

	UpdateFont();

	OP_TCINFO* info = TCGetInfo();

#ifdef RANGESELECT_FROM_EDGE
	if (m_packed.determine_select_direction && action->GetAction() == OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER)
		multi_edit->SetCaret(multi_edit->sel_start);
#endif // RANGESELECT_FROM_EDGE

	INT32 old_caret_pos_global = 0;
	if (action->IsRangeAction())
		old_caret_pos_global = multi_edit->caret.GetGlobalOffset(info);

	if (moving_caret)
		multi_edit->InvalidateCaret();

	if (!moving_caret)
		multi_edit->BeginChange();

#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
	BOOL handled = FALSE;
	if (!action->IsRangeAction() && multi_edit->HasSelectedText())
	{
		OpPoint pos_sel_start = multi_edit->sel_start.GetPoint(info, TRUE);
		OpPoint pos_sel_stop = multi_edit->sel_stop.GetPoint(info, FALSE);
		BOOL start_stop_at_same_line = (pos_sel_start.y == pos_sel_stop.y);
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_NEXT_CHARACTER:
#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
			case OpInputAction::ACTION_NEXT_LINE:
#endif
				multi_edit->SetCaret((pos_sel_start.x > pos_sel_stop.x && start_stop_at_same_line) ? multi_edit->sel_start : multi_edit->sel_stop);
				handled = TRUE;
				break;
			case OpInputAction::ACTION_PREVIOUS_CHARACTER:
#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
			case OpInputAction::ACTION_PREVIOUS_LINE:
#endif
				multi_edit->SetCaret((pos_sel_start.x > pos_sel_stop.x && start_stop_at_same_line) ? multi_edit->sel_stop : multi_edit->sel_start);
				handled = TRUE;
				break;
		}
	}
	if (!handled)
#endif

	switch (action->GetAction())
	{
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_START:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_GO_TO_START:
			multi_edit->SetCaretPos(OpPoint(0, 0));
			// fall through...
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_GO_TO_LINE_START:
			multi_edit->MoveToStartOrEndOfLine(TRUE, FALSE);
			break;

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_END:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_GO_TO_END:
			multi_edit->SetCaretPos(OpPoint(multi_edit->total_width, multi_edit->total_height));
			// fall through...
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_GO_TO_LINE_END:
			multi_edit->MoveToStartOrEndOfLine(FALSE, FALSE);
			break;

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PAGE_UP:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PAGE_UP:
			multi_edit->SetCaretPos(OpPoint(caret_wanted_x, multi_edit->caret_pos.y - multi_edit->visible_height));
			break;
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PAGE_DOWN:
			multi_edit->SetCaretPos(OpPoint(caret_wanted_x, multi_edit->caret_pos.y + multi_edit->visible_height));
			break;

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_NEXT_CHARACTER:
			{
			// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
			BOOL forward = TRUE;
			if (action->IsRangeAction() && GetRTL())
				forward = !forward;
			multi_edit->MoveCaret(forward, !action->IsRangeAction());
			}
			break;
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PREVIOUS_CHARACTER:
			{
			// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
			BOOL forward = FALSE;
			if (action->IsRangeAction() && GetRTL())
				forward = !forward;
			multi_edit->MoveCaret(forward, !action->IsRangeAction());
			}
			break;

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_NEXT_WORD:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_NEXT_WORD:
			multi_edit->MoveToNextWord(TRUE, !action->IsRangeAction());
			break;
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PREVIOUS_WORD:
			multi_edit->MoveToNextWord(FALSE, !action->IsRangeAction());
			break;

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_NEXT_LINE:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_NEXT_LINE:
			if (!multi_edit->SetCaretPos(OpPoint(caret_wanted_x, multi_edit->caret_pos.y + multi_edit->line_height), FALSE))
			{
				multi_edit->MoveToStartOrEndOfLine(FALSE, FALSE);
				caret_wanted_x = multi_edit->caret_pos.x;
			}
			break;
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PREVIOUS_LINE:
			if (!multi_edit->SetCaretPos(OpPoint(caret_wanted_x, multi_edit->caret_pos.y - multi_edit->line_height), FALSE))
			{
				multi_edit->MoveToStartOrEndOfLine(TRUE, FALSE);
				caret_wanted_x = multi_edit->caret_pos.x;
			}
			break;

		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
			if (!multi_edit->HasSelectedText())
			{
				INT32 old_caret_pos_global = multi_edit->caret.GetGlobalOffset(info);

				if (action->GetAction() == OpInputAction::ACTION_DELETE_WORD)
					multi_edit->MoveToNextWord(TRUE, FALSE);
				else if( action->GetAction() == OpInputAction::ACTION_DELETE_TO_END_OF_LINE)
					multi_edit->MoveToStartOrEndOfLine(FALSE, FALSE);
				else
					multi_edit->MoveToNextWord(FALSE, FALSE);

				INT32 caret_pos_global = multi_edit->caret.GetGlobalOffset(info);
				local_input_changed = old_caret_pos_global != caret_pos_global;
				if (local_input_changed)
					multi_edit->SelectToCaret(old_caret_pos_global, caret_pos_global);
			}
			if (!multi_edit->HasSelectedText())
				break;

		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_BACKSPACE:
			if (!multi_edit->HasSelectedText())
			{
				INT32 old_caret_pos_global = multi_edit->caret.GetGlobalOffset(info);
				multi_edit->MoveCaret(action->GetAction() == OpInputAction::ACTION_DELETE ? TRUE : FALSE, FALSE);
				multi_edit->SelectToCaret(old_caret_pos_global, multi_edit->caret.GetGlobalOffset(info));
				local_input_changed = multi_edit->caret.GetGlobalOffset(info) != old_caret_pos_global;
			}
			if (multi_edit->HasSelectedText())
				multi_edit->RemoveSelection(TRUE, TRUE);
			break;

		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		{
			int hex_start, hex_end, hex_pos;

			if (multi_edit->HasSelectedText() && multi_edit->sel_start.block == multi_edit->caret.block && multi_edit->sel_stop.block == multi_edit->caret.block)
			{
				hex_end = multi_edit->sel_stop.ofs;
				hex_start = multi_edit->sel_start.ofs;
			}
			else
			{
				hex_end = multi_edit->caret.ofs;
				hex_start = 0;
			}
			if (UnicodePoint charcode = ConvertHexToUnicode(hex_start, hex_end, hex_pos, multi_edit->caret.block->CStr()))
			{
				OpTCOffset ofs1, ofs2;
				ofs1.block = multi_edit->caret.block;
				ofs1.ofs = hex_pos;
				ofs2.block = multi_edit->caret.block;
				ofs2.ofs = hex_end;
				multi_edit->SetSelection(ofs1, ofs2);
				uni_char instr[3] = { 0, 0, 0 }; /* ARRAY OK 2011-11-07 peter */
				Unicode::WriteUnicodePoint(instr, charcode);
				REPORT_IF_OOM(multi_edit->InsertText(instr, uni_strlen(instr)));
			}
			break;
		}
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
#ifdef WIDGETS_IME_SUPPORT
			OP_ASSERT(imstring == NULL); // Key should not be sent if inputmethod is active.
#endif
			OpKey::Code key = action->GetActionKeyCode();

#ifdef OP_KEY_ENTER_ENABLED
			if (key == OP_KEY_ENTER)
			{
				uni_char instr[2] = { 13, 10 };
				REPORT_IF_OOM(multi_edit->InsertText(instr, 2));
			}
			else
#endif // OP_KEY_ENTER_ENABLED
			{
				const uni_char *instr;
				const uni_char *tab = UNI_L("\t");

				if (key == OP_KEY_TAB)
					instr = tab;
				else
				{
					instr = action->GetKeyValue();
					const OpString& block_text = multi_edit->caret.block->text;

					if (m_packed.is_overstrike &&
						!multi_edit->HasSelectedText() &&
						multi_edit->caret.ofs < block_text.Length())
					{
						UnicodeStringIterator iter(block_text.DataPtr(),
								                   multi_edit->caret.ofs,
										           block_text.Length());

#ifdef SUPPORT_UNICODE_NORMALIZE
						iter.NextBaseCharacter();
#else
						iter.Next();
#endif

						int consumed = iter.Index() - multi_edit->caret.ofs;

						INT32 caret_pos_global = multi_edit->caret.GetGlobalOffset(info);
						multi_edit->SetSelection(caret_pos_global, caret_pos_global + consumed);
					}
				}
				REPORT_IF_OOM(multi_edit->InsertText(instr, instr ? uni_strlen(instr) : 0));
			}
			break;
		}
	}

#ifdef HAS_NOTEXTSELECTION
	(void)old_caret_pos_global;
#else
	if (action->IsRangeAction())
	{
		multi_edit->SelectToCaret(old_caret_pos_global, multi_edit->caret.GetGlobalOffset(info));
		if (listener && HasSelectedText())
			listener->OnSelect(this);
		m_packed.determine_select_direction = FALSE;
	}
	else
#endif // !HAS_NOTEXTSELECTION
	if (moving_caret)
	{
		multi_edit->SelectNothing(TRUE);
		multi_edit->InvalidateCaret();
		m_packed.determine_select_direction = FALSE;
	}

	if (!moving_caret)
		multi_edit->EndChange();

	if (!IsUpDownAction(action->GetAction()) ||
		action->GetAction() == OpInputAction::ACTION_GO_TO_START ||
		action->GetAction() == OpInputAction::ACTION_GO_TO_END)
	{
		caret_wanted_x = multi_edit->caret_pos.x;
	}

	ScrollIfNeeded(TRUE);

	if (!moving_caret && local_input_changed)
	{
		SetReceivedUserInput();
		m_packed.is_changed = TRUE;
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
#endif
	// Reset cursor blink
#if defined(_MACINTOSH_)
	if (multi_edit->HasSelectedText())
		caret_blink = 1;
	else
#endif
	caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	EnableCaretBlinking(TRUE);
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

	if (multi_edit->caret_pos.x != old_caret_pos.x || multi_edit->caret_pos.y != old_caret_pos.y)
		ReportCaretPosition();

	// If the user has changed the text, invoke the listener

	if (!moving_caret && local_input_changed && listener)
		listener->OnChange(this); // Invoke!
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpMultilineEdit::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
#ifdef PAN_SUPPORT
#ifdef ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_COMPOSITE_PAN:
#endif // ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_PAN_X:
		case OpInputAction::ACTION_PAN_Y:
			return OpWidget::OnInputAction(action);
			break;
#endif // PAN_SUPPORT

		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OpInputAction::Action action_type = child_action->GetAction();
#ifdef USE_OP_CLIPBOARD
			BOOL force_enabling = FALSE;
			if (action_type == OpInputAction::ACTION_COPY ||
			    action_type == OpInputAction::ACTION_CUT ||
			    action_type == OpInputAction::ACTION_PASTE)
				if (FormObject* form_obj = GetFormObject())
					force_enabling = g_clipboard_manager->ForceEnablingClipboardAction(action_type, form_obj->GetDocument(), form_obj->GetHTML_Element());
#endif // USE_OP_CLIPBOARD
			switch (action_type)
			{
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				case OpInputAction::ACTION_UNDO:
					child_action->SetEnabled(!m_packed.is_readonly && (IsUndoAvailable() || HasReceivedUserInput()));
					return TRUE;

				case OpInputAction::ACTION_REDO:
					child_action->SetEnabled(!m_packed.is_readonly && (IsRedoAvailable() || HasReceivedUserInput()));
					return TRUE;
#endif // WIDGETS_UNDO_REDO_SUPPORT

				case OpInputAction::ACTION_DELETE:
				case OpInputAction::ACTION_CLEAR:
					child_action->SetEnabled(!m_packed.is_readonly && GetTextLength(FALSE) > 0);
					return TRUE;
#ifdef USE_OP_CLIPBOARD
				case OpInputAction::ACTION_CUT:
					child_action->SetEnabled( force_enabling || (!m_packed.is_readonly && HasSelectedText()) );
					return TRUE;

				case OpInputAction::ACTION_COPY:
					child_action->SetEnabled( force_enabling || HasSelectedText() );
					return TRUE;

				case OpInputAction::ACTION_COPY_TO_NOTE:
					child_action->SetEnabled( HasSelectedText() );
					return TRUE;

				case OpInputAction::ACTION_COPY_LABEL_TEXT:
					child_action->SetEnabled( TRUE );
					return TRUE;

				case OpInputAction::ACTION_PASTE:
					child_action->SetEnabled( force_enabling || (!m_packed.is_readonly && g_clipboard_manager->HasText()) );
					return TRUE;
#endif // USE_OP_CLIPBOARD
#ifndef HAS_NOTEXTSELECTION
				case OpInputAction::ACTION_SELECT_ALL:
					child_action->SetEnabled(GetTextLength(FALSE) > 0);
					return TRUE;
				case OpInputAction::ACTION_DESELECT_ALL:
					child_action->SetEnabled(HasSelectedText());
					return TRUE;
#endif // !HAS_NOTEXTSELECTION

				case OpInputAction::ACTION_INSERT:
					child_action->SetEnabled(!m_packed.is_readonly);
					return TRUE;
			}
			return FALSE;
		}

#ifdef QUICK
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		{
			if (action->IsKeyboardInvoked())
			{
				OpRect rect = GetRect(TRUE);
				OpPoint point = vis_dev->GetView()->ConvertToScreen(OpPoint(rect.x, rect.y));
				OpPoint cp = GetCaretCoordinates();
				action->SetActionPosition(OpRect(point.x + cp.x, point.y + cp.y, 1, GetLineHeight()));
			}
			return g_input_manager->InvokeAction(action, GetParentInputContext());
		}
		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			OpPoint pt = GetCaretCoordinates();
			OpRect rect(pt.x, pt.y, 1, GetLineHeight());
			OnContextMenu(rect.BottomLeft(), &rect, action->IsKeyboardInvoked());
			return TRUE;
		}
#endif
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			OpKey::Code key = action->GetActionKeyCode();
			ShiftKeyState modifiers = action->GetShiftKeys();
			BOOL is_key_wanted;
			switch (key)
			{
#ifdef OP_KEY_ENTER_ENABLED
			case OP_KEY_ENTER:
				is_key_wanted = (action->GetShiftKeys() & (SHIFTKEY_CTRL | SHIFTKEY_ALT | SHIFTKEY_META)) == 0;
				break;
#endif // OP_KEY_ENTER_ENABLED
#ifdef OP_KEY_TAB_ENABLED
			case OP_KEY_TAB:
				is_key_wanted = m_packed.wants_tab && modifiers == SHIFTKEY_NONE;
				break;
#endif // OP_KEY_TAB_ENABLED
			default:
			{
				const uni_char *key_value = action->GetKeyValue();
#ifdef MSWIN
				/* A keypress involving CTRL or ALT is ignored, but not if it involves
				   both. Some key mappings use those modifier combinations
				   for dead/accented keys (e.g., Ctrl-Alt-Z on a Polish keyboard.) */
				ShiftKeyState ctrl_alt = modifiers & (SHIFTKEY_CTRL | SHIFTKEY_ALT);
				if (ctrl_alt == SHIFTKEY_ALT || ctrl_alt == SHIFTKEY_CTRL)
#else //MSWIN
				/* A keypress involving CTRL is ignored, but not if it involves
				   ALT (or META.) Some key mappings use those modifier combinations
				   for dead/accented keys (e.g., Ctrl-Alt-Z on a Polish keyboard.) */
				if ((modifiers & SHIFTKEY_CTRL) != 0 && (modifiers & (SHIFTKEY_ALT | SHIFTKEY_META)) == 0)
#endif //MSWIN
					is_key_wanted = FALSE;
				else
					is_key_wanted = key_value != NULL && key_value[0] >= 32 && key_value[0] != 0x7f;
			}
			}

			if (is_key_wanted)
			{
				if (m_packed.is_readonly && !IsForm())
				{
					// Some keypresses like ENTER must be propagated to parent
					// For now I limit this to non-form widgets
					if( !IsUpDownAction(action->GetAction()))
					{
						return FALSE;
					}
				}

				if (alt_charcode > 255)
				{
					OP_ASSERT(alt_charcode < 0xffff);
					uni_char str[2]; /* ARRAY OK 2011-12-01 sof */
					str[0] = static_cast<uni_char>(alt_charcode);
					str[1] = 0;
					action->SetActionKeyValue(str);
				}
				alt_charcode = 0;

#ifdef IME_SEND_KEY_EVENTS
				if (m_fake_keys == 0)
					EditAction(action);
				else
					m_fake_keys--;
#else
				EditAction(action);
#endif // IME_SEND_KEY_EVENTS
				Sync();

				return TRUE;
			}
			return FALSE;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
			if (IsReadOnly())
				return FALSE;
			SetJustify(action->GetAction() == OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL ? JUSTIFY_RIGHT : JUSTIFY_LEFT, TRUE);
			SetRTL(action->GetAction() == OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL);
			return TRUE;
#endif // SUPPORT_TEXT_DIRECTION

#ifdef OP_KEY_ALT_ENABLED
		case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
		{
			OpKey::Code key = action->GetActionKeyCode();
			if (key == OP_KEY_ALT)
				alt_charcode = 0;		// reset if previous value was stuck because Windoze didn't issue WM_CHAR
			else if ((action->GetShiftKeys() & SHIFTKEY_ALT) && key >= OP_KEY_0 && key <= OP_KEY_9)
				alt_charcode = alt_charcode * 10 + key - OP_KEY_0;

			return FALSE;
		}
#endif //  OP_KEY_ALT_ENABLED

		case OpInputAction::ACTION_TOGGLE_OVERSTRIKE:
		{
			m_packed.is_overstrike = !m_packed.is_overstrike;
			return TRUE;
		}

		case OpInputAction::ACTION_NEXT_LINE:
		case OpInputAction::ACTION_PREVIOUS_LINE:
#if defined(ESCAPE_FOCUS_AT_TOP_BOTTOM) || defined(WIDGETS_UP_DOWN_MOVES_TO_START_END)
			if (action->GetAction() == OpInputAction::ACTION_PREVIOUS_LINE && CaretOnFirstInputLine())
				return FALSE;
			if (action->GetAction() == OpInputAction::ACTION_NEXT_LINE && CaretOnLastInputLine())
				return FALSE;
#endif
			// fall through...
		case OpInputAction::ACTION_GO_TO_START:
		case OpInputAction::ACTION_GO_TO_END:
		case OpInputAction::ACTION_GO_TO_LINE_START:
		case OpInputAction::ACTION_GO_TO_LINE_END:
		case OpInputAction::ACTION_PAGE_UP:
		case OpInputAction::ACTION_PAGE_DOWN:
		case OpInputAction::ACTION_NEXT_CHARACTER:
		case OpInputAction::ACTION_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_NEXT_WORD:
		case OpInputAction::ACTION_PREVIOUS_WORD:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_START:
		case OpInputAction::ACTION_RANGE_GO_TO_END:
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
		case OpInputAction::ACTION_RANGE_PAGE_UP:
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
		case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
		case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_RANGE_NEXT_WORD:
		case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
		case OpInputAction::ACTION_RANGE_NEXT_LINE:
		case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
		case OpInputAction::ACTION_BACKSPACE:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		{
			EditAction(action);
			Sync();

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
			if( action->IsRangeAction() )
			{
				if( HasSelectedText() )
				{
					g_clipboard_manager->SetMouseSelectionMode(TRUE);
					Copy();
					g_clipboard_manager->SetMouseSelectionMode(FALSE);
				}
			}
#endif // _X11_SELECTION_POLICY_ && USE_OP_CLIPBOARD
			return TRUE;
		}

#ifdef WIDGETS_UNDO_REDO_SUPPORT
		case OpInputAction::ACTION_UNDO:
			if (m_packed.is_readonly ||
				(!IsUndoAvailable() && !HasReceivedUserInput()))
				return FALSE;
			Undo();
			return TRUE;

		case OpInputAction::ACTION_REDO:
			if (m_packed.is_readonly ||
				(!IsRedoAvailable() && !HasReceivedUserInput()))
				return FALSE;
			Redo();
			return TRUE;
#endif // WIDGETS_UNDO_REDO_SUPPORT

#ifdef USE_OP_CLIPBOARD
		case OpInputAction::ACTION_CUT:
			Cut();
			return TRUE;

		case OpInputAction::ACTION_COPY:
			Copy();
			return TRUE;

		case OpInputAction::ACTION_COPY_TO_NOTE:
			Copy(TRUE);
			return TRUE;

		case OpInputAction::ACTION_COPY_LABEL_TEXT:
			Copy();
			return TRUE;

		case OpInputAction::ACTION_PASTE:
			g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			return TRUE;

		case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
		{
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
# endif // _X11_SELECTION_POLICY_
			if( g_clipboard_manager->HasText() )
			{
				if( action->GetActionData() == 1 )
				{
					Clear();
				}
				g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			}
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
# endif // _X11_SELECTION_POLICY_
			return TRUE;
		}
#endif // USE_OP_CLIPBOARD

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_SELECT_ALL:
		{
			SelectText();
			if (listener)
				listener->OnSelect(this);
			return TRUE;
		}

		case OpInputAction::ACTION_DESELECT_ALL:
		{
			if (HasSelectedText())
			{
				SelectNothing();
				return TRUE;
			}
			return FALSE;
		}
#endif // !HAS_NOTEXTSELECTION

		case OpInputAction::ACTION_CLEAR:
			Clear();
			return TRUE;

		case OpInputAction::ACTION_INSERT:
			if (!IsReadOnly())
				InsertText(action->GetActionDataString());
			return TRUE;

		case OpInputAction::ACTION_LEFT_ADJUST_TEXT:
			if( IsForm() )
			{
				SetJustify(JUSTIFY_LEFT, TRUE);
				return TRUE;
			}
			else
				return FALSE;

		case OpInputAction::ACTION_RIGHT_ADJUST_TEXT:
			if( IsForm() )
			{
				SetJustify(JUSTIFY_RIGHT, TRUE);
				return TRUE;
			}
			else
				return FALSE;

#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_UNFOCUS_FORM:
			return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // _SPAT_NAV_SUPPORT_
	}

	return FALSE;
}

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
OpPoint OpMultilineEdit::GetCaretCoordinates()
{
	int x = 0, y = 0;
	GetLeftTopOffset(x, y);
	OpPoint internal_caret_pos = GetCaretPos();
	return OpPoint(internal_caret_pos.x + x,
					internal_caret_pos.y + y);
}
#endif // OP_KEY_CONTEXT_MENU_ENABLED

BOOL OpMultilineEdit::CaretOnFirstInputLine() const
{
	if (!multi_edit->FirstBlock())
		return TRUE;
	return multi_edit->caret_pos.y == multi_edit->FirstBlock()->y;
}

BOOL OpMultilineEdit::CaretOnLastInputLine() const
{
	if (!multi_edit->LastBlock())
		return TRUE;
	return multi_edit->caret_pos.y == multi_edit->LastBlock()->y + multi_edit->LastBlock()->block_height - multi_edit->line_height;
}

INT32 OpMultilineEdit::GetCharacterOfs(OpRect rect, OpPoint pos)
{
	UpdateFont();
	return multi_edit->PointToOffset(pos).GetGlobalOffset(TCGetInfo());
}

void OpMultilineEdit::BeforeAction()
{
	UpdateFont();
	multi_edit->BeginChange();
	multi_edit->InvalidateCaret();
}

void OpMultilineEdit::AfterAction()
{
//	multi_edit->MoveElements();
	multi_edit->InvalidateCaret();
	multi_edit->EndChange();
	caret_wanted_x = multi_edit->caret_pos.x;
}

#ifdef WIDGETS_UNDO_REDO_SUPPORT
void OpMultilineEdit::Undo(BOOL ime_undo, BOOL scroll_if_needed)
{
	BeforeAction();
	BOOL local_input_changed = FALSE;
	if (!m_packed.is_readonly && multi_edit->undo_stack.CanUndo())
	{
		UndoRedoEvent* event = multi_edit->undo_stack.Undo();
		UndoRedoEvent* previous_event = multi_edit->undo_stack.PeekUndo();

		OP_ASSERT(event && event->GetType() != UndoRedoEvent::EV_TYPE_REPLACE);

		if (previous_event != NULL && previous_event->GetType() == UndoRedoEvent::EV_TYPE_REPLACE)
		{
			local_input_changed = TRUE;
			OP_ASSERT(event->GetType() == UndoRedoEvent::EV_TYPE_INSERT);
			//need to undo the previous_event as well
			event = multi_edit->undo_stack.Undo();

			multi_edit->SelectNothing(TRUE);
			REPORT_IF_OOM(multi_edit->SetText(event->str, event->str_length, FALSE));
			multi_edit->SetSelection(event->sel_start, event->sel_stop);
			multi_edit->SetCaretOfsGlobal(event->caret_pos);
		}
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_INSERT) // remove inserted text
		{
			local_input_changed = TRUE;
			multi_edit->SetSelection(event->caret_pos, event->caret_pos + event->str_length);
			REPORT_IF_OOM(multi_edit->RemoveSelection(FALSE, TRUE));
			multi_edit->SetCaretOfsGlobal(event->caret_pos);
		}
#ifdef WIDGETS_IME_SUPPORT
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_EMPTY)
		{
			// nothing
		}
#endif // WIDGETS_IME_SUPPORT
		else// if (event->GetType() == UndoRedoEvent::EV_TYPE_REMOVE) // insert removed text
		{
			local_input_changed = TRUE;
			multi_edit->SelectNothing(TRUE);
			multi_edit->SetCaretOfsGlobal(event->sel_start);
			REPORT_IF_OOM(multi_edit->InsertText(event->str, event->str_length, FALSE));
			multi_edit->SetSelection(event->sel_start, event->sel_stop);
			multi_edit->SetCaretOfsGlobal(event->caret_pos);
		}
	}
	AfterAction();

	if (scroll_if_needed)
		ScrollIfNeeded();

	if (!ime_undo && local_input_changed && listener)
		listener->OnChange(this, FALSE);
}

void OpMultilineEdit::Redo()
{
	BeforeAction();
	BOOL local_input_changed = FALSE;
	if (!m_packed.is_readonly && multi_edit->undo_stack.CanRedo())
	{
		local_input_changed = TRUE;
		UndoRedoEvent* event = multi_edit->undo_stack.Redo();

		if (event->GetType() == UndoRedoEvent::EV_TYPE_REPLACE)
		{
			OP_ASSERT(multi_edit->undo_stack.PeekRedo() && multi_edit->undo_stack.PeekRedo()->GetType() == UndoRedoEvent::EV_TYPE_INSERT);

			//need to redo the next_event as well
			event = multi_edit->undo_stack.Redo();

			REPORT_IF_OOM(multi_edit->SetText(event->str, event->str_length, FALSE));
			multi_edit->SetSelection(event->sel_start, event->sel_stop);
			multi_edit->SetCaretOfsGlobal(event->caret_pos);
		}
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_INSERT) // icnsert text again
		{
			multi_edit->SelectNothing(TRUE);
			multi_edit->SetCaretOfsGlobal(event->caret_pos);
			REPORT_IF_OOM(multi_edit->InsertText(event->str, event->str_length, FALSE));
		}
		else// if (event->GetType() == UndoRedoEvent::EV_TYPE_REMOVE) // remove text again
		{
			multi_edit->SetSelection(event->sel_start, event->sel_stop);
			REPORT_IF_OOM(multi_edit->RemoveSelection(FALSE, TRUE));
			multi_edit->SetCaretOfsGlobal(event->sel_start);
		}
		multi_edit->SelectNothing(TRUE);
	}
	AfterAction();
	ScrollIfNeeded();

	if (listener && local_input_changed)
		listener->OnChange(this, FALSE);
}
#endif // WIDGETS_UNDO_REDO_SUPPORT

#ifdef USE_OP_CLIPBOARD
#ifdef WIDGETS_REMOVE_CARRIAGE_RETURN

static uni_char* RemoveCarriageReturn(uni_char* text)
{
	if( text )
	{
		int length = uni_strlen(text);
		uni_char* copy = OP_NEWA(uni_char, length+1);
		if( copy )
		{
			uni_char* p = copy;
			for( int i=0; i<length; i++ )
			{
				if( text[i] != '\r' )
				{
					*p =text[i];
					p++;
				}
			}
			*p = 0;
			OP_DELETEA(text);
			text = copy;
		}
	}

	return text;
}

#endif // WIDGETS_REMOVE_CARRIAGE_RETURN
#endif // USE_OP_CLIPBOARD

void OpMultilineEdit::Copy(BOOL to_note)
{
#ifdef QUICK
	if (to_note)
	{
		if (multi_edit->HasSelectedText())
		{
			uni_char* text = multi_edit->GetSelectionText();
			if (text == NULL)
				ReportOOM();
			else
			{
#ifdef WIDGETS_REMOVE_CARRIAGE_RETURN
				text = RemoveCarriageReturn(text);
#endif
				// less code here.. redirect to static helper function ASAP
				NotesPanel::NewNote(text);
			}
		}
	}
	else
#endif
	{
#ifdef USE_OP_CLIPBOARD
		FormObject* fo = GetFormObject();
		REPORT_IF_OOM(g_clipboard_manager->Copy(this, fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0, fo ? fo->GetDocument() : NULL, fo ? fo->GetHTML_Element() : NULL));
#endif // USE_OP_CLIPBOARD
	}
}

void OpMultilineEdit::Cut()
{
#ifdef USE_OP_CLIPBOARD
	FormObject* fo = GetFormObject();
	REPORT_IF_OOM(g_clipboard_manager->Cut(this, fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0, fo ? fo->GetDocument() : NULL, fo ? fo->GetHTML_Element() : NULL));
#endif // USE_OP_CLIPBOARD
}

#ifdef USE_OP_CLIPBOARD
void OpMultilineEdit::PlaceSelectionInClipboard(OpClipboard* clipboard, BOOL remove)
{
	if (remove)
	{
		if (m_packed.is_readonly)
			return;
		BeforeAction();
	}

	if (multi_edit->HasSelectedText())
	{
		UpdateFont();
		if (m_packed.is_label_mode)
		{
			OpString text;
			GetText(text);

#ifdef WIDGETS_REMOVE_CARRIAGE_RETURN
			if( text.CStr() )
			{
				int new_len = text.Length()+1;
				uni_char* tmp = OP_NEWA(uni_char, new_len);
				if( tmp )
				{
					uni_strcpy( tmp, text.CStr() );
					tmp = RemoveCarriageReturn(tmp);
					if (OpStatus::IsError(text.Set(tmp))) ReportOOM();
					OP_DELETEA(tmp);
				}
			}
#endif

			if( text.CStr() )
			{
				FormObject* fo = GetFormObject();
				REPORT_IF_OOM(clipboard->PlaceText(text.CStr(), fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0));
			}
		}
		else
		{
			uni_char* text = multi_edit->GetSelectionText();
			if (text == NULL)
				ReportOOM();
			else
			{
#ifdef WIDGETS_REMOVE_CARRIAGE_RETURN
				text = RemoveCarriageReturn(text);
#endif
				FormObject* fo = GetFormObject();
				REPORT_IF_OOM(clipboard->PlaceText(text, fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0));
				OP_DELETEA(text);
			}
		}

		if (remove)
		{
			OpInputAction action(OpInputAction::ACTION_DELETE);
			EditAction(&action);
		}
	}

	if (remove)
	{
		AfterAction();

		if (listener && !IsForm())
			listener->OnChange(this, FALSE);
	}
}

void OpMultilineEdit::OnCut(OpClipboard* clipboard)
{
	PlaceSelectionInClipboard(clipboard, TRUE);
}

void OpMultilineEdit::OnCopy(OpClipboard* clipboard)
{
	PlaceSelectionInClipboard(clipboard, FALSE);
}

void OpMultilineEdit::OnPaste(OpClipboard* clipboard)
{
	if (m_packed.is_readonly || !clipboard->HasText())
		return;

	BeforeAction();
	OpString text;
	OP_STATUS status = clipboard->GetText(text);
	if (OpStatus::IsSuccess(status))
	{
		REPORT_IF_OOM(multi_edit->InsertText(text.CStr(), text.Length()));
		ScrollIfNeeded();
		InvalidateAll();
	}
	else
		ReportOOM();
	AfterAction();
	SetReceivedUserInput();

	if (listener)
		listener->OnChange(this, FALSE);
}
#endif // USE_OP_CLIPBOARD

#if defined _TRIPLE_CLICK_ONE_LINE_

void OpMultilineEdit::SelectOneLine()
{
	if (!m_packed.is_label_mode)
	{
		SelectNothing();

		OP_TCINFO* info = TCGetInfo();
		multi_edit->SetCaretPos(OpPoint(0, multi_edit->caret_pos.y));
		INT32 old_caret_pos_global = multi_edit->caret.GetGlobalOffset(info);
		multi_edit->SetCaretPos(OpPoint(multi_edit->total_width, multi_edit->caret_pos.y));
		multi_edit->SelectToCaret(old_caret_pos_global, multi_edit->caret.GetGlobalOffset(info));
	}
}

#endif

void OpMultilineEdit::SelectAll()
{
	if (!m_packed.is_label_mode)
	{
		SelectText();
		if (listener)
			listener->OnSelect(this);

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
		if( HasSelectedText() )
		{
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			Copy();
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
		}
#endif // _X11_SELECTION_POLICY_ && USE_OP_CLIPBOARD

	}
}


void OpMultilineEdit::Clear()
{
	if (m_packed.is_readonly)
		return;

	// We need a multi_edit->Clear() with undo

	BOOL local_input_changed = !IsEmpty();

	BeforeAction();
	SelectText();
	OpInputAction action(OpInputAction::ACTION_DELETE);
	EditAction(&action);
	AfterAction();

	if (listener && local_input_changed && !IsForm())
		listener->OnChange(this, FALSE);
}

void OpMultilineEdit::BeginChangeProperties()
{
	if (is_changing_properties == 0)
		needs_reformat = 0;

	is_changing_properties++;
}

void OpMultilineEdit::EndChangeProperties()
{
	is_changing_properties--;

	if (is_changing_properties == 0 && needs_reformat)
	{
		BOOL update_fragment_list = (needs_reformat & 2) ? TRUE : FALSE;

		// scale has changed, need to re-measure text with new font
		// size
		if (vis_dev && vis_dev->GetScale() != m_vd_scale)
		{
			update_fragment_list = TRUE;
			m_vd_scale = vis_dev->GetScale();
		}

		ReformatNeeded(update_fragment_list);
	}
}

void OpMultilineEdit::ReformatNeeded(BOOL update_fragment_list)
{
	if (is_changing_properties)
		needs_reformat |= update_fragment_list ? 2 : 1;
	else
	{
		if (OpStatus::IsError(Reformat(!!update_fragment_list)))
			ReportOOM();

		if (UpdateScrollbars())
		{
			if (OpStatus::IsError(Reformat(update_fragment_list && GetAggressiveWrapping())))
				ReportOOM();
#ifdef DEBUG_ENABLE_OPASSERT
			const BOOL r =
#endif // DEBUG_ENABLE_OPASSERT
				UpdateScrollbars(TRUE);
			OP_ASSERT(!r);
		}
	}
}

OP_STATUS OpMultilineEdit::Reformat(BOOL update_fragment_list)
{
	RETURN_IF_ERROR(multi_edit->Reformat(update_fragment_list || m_packed.fragments_need_update));
	m_packed.fragments_need_update = false;
	return OpStatus::OK;
}

void OpMultilineEdit::OnJustifyChanged()
{
	InvalidateAll();
}

void OpMultilineEdit::OnFontChanged()
{
	m_packed.fragments_need_update = true;
	if (vis_dev)
	{
		// Make sure line_height is updated in UpdateFont
		m_packed.line_height_set = FALSE;

		if (rect.width)
			ReformatNeeded(TRUE);
	}
}

void OpMultilineEdit::OnDirectionChanged()
{
	SetJustify(GetRTL() ? JUSTIFY_RIGHT : JUSTIFY_LEFT, FALSE);
	if (vis_dev)
	{
		multi_edit->UpdateCaretPos();
		caret_wanted_x = multi_edit->caret_pos.x;
		UpdateFont();
		if (OpStatus::IsError(Reformat(TRUE)))
			ReportOOM();
	}
	UpdateScrollbars();
	InvalidateAll();
}

void OpMultilineEdit::OnResizabilityChanged()
{
	UpdateScrollbars();
}

void OpMultilineEdit::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	widget_painter->DrawMultiEdit(GetBounds());
}

void OpMultilineEdit::OnTimer()
{
	// Scroll
#ifndef MOUSELESS
	if (is_selecting)
	{
		OpPoint mp = GetMousePos();
		if (mp.x < 0)
			x_scroll->SetValue(x_scroll->GetValue() + mp.x);
		if (mp.x > multi_edit->visible_width)
			x_scroll->SetValue(x_scroll->GetValue() + mp.x - multi_edit->visible_width);
		if (mp.y < 0)
			y_scroll->SetValue(y_scroll->GetValue() + mp.y);
		if (mp.y > multi_edit->visible_height)
			y_scroll->SetValue(y_scroll->GetValue() + mp.y - multi_edit->visible_height);
	}
	else
#endif // !MOUSELESS
#if defined(_MACINTOSH_) || defined (WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP)
	if (!multi_edit->HasSelectedText())
#endif
	{
		caret_blink = !caret_blink;
		multi_edit->InvalidateCaret();
	}
}

BOOL OpMultilineEdit::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return (vertical ? (OpScrollbar*)y_scroll : (OpScrollbar*)x_scroll)->OnScroll(delta,vertical, smooth);
}

BOOL OpMultilineEdit::SetScrollValue(INT32 value, BOOL vertical, BOOL caused_by_input)
{
	OpScrollbar* to_scroll = vertical ? (OpScrollbar*)y_scroll : (OpScrollbar*)x_scroll;
	INT32 value_before = to_scroll->GetValue();
	to_scroll->SetValue(value, caused_by_input);
	return value_before != to_scroll->GetValue();
}

#ifndef MOUSELESS

BOOL OpMultilineEdit::OnMouseWheel(INT32 delta, BOOL vertical)
{
	return vertical ?
		((OpScrollbar*)y_scroll)->OnMouseWheel(delta, vertical) :
		((OpScrollbar*)x_scroll)->OnMouseWheel(delta, vertical);
}

#endif // !MOUSELESS

#ifdef WIDGETS_IME_SUPPORT
BOOL OpMultilineEdit::IMEHandleFocusEventInt(BOOL focus, FOCUS_REASON reason)
{
# ifdef WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	if (!(focus && (reason == FOCUS_REASON_ACTIVATE || reason == FOCUS_REASON_RESTORE)))
# endif // WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	{
		if (!SpawnIME(vis_dev, im_style.CStr(), (focus && !m_packed.is_readonly) ? OpView::IME_MODE_TEXT : OpView::IME_MODE_UNKNOWN, GetIMEContext(GetFormObject())))
			return FALSE;
	}

	return TRUE;
}
#endif // WIDGETS_IME_SUPPORT

void OpMultilineEdit::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus && m_packed.ghost_mode)
	{
		OP_STATUS status = SetText(0);
		// this shouldn't fail. or can it?

		OpStatus::Ignore(status);
		OP_ASSERT(OpStatus::IsSuccess(status));
		OP_ASSERT(!m_packed.ghost_mode);
	}

#ifdef _MACINTOSH_
	// this is to draw the focusborder
	Invalidate(GetBounds().InsetBy(-2,-2), FALSE, TRUE);
#endif
#ifdef WIDGETS_IME_SUPPORT
	m_suppress_ime = FALSE;

	if (focus)
	{
		if (FormObject* fo = GetFormObject())
		{
			if (reason != FOCUS_REASON_ACTIVATE && fo->GetHTML_Element()->HasEventHandler(fo->GetDocument(), ONFOCUS) && !m_packed.focus_comes_from_scrollbar)
			{
				m_suppress_ime = TRUE;
				m_suppressed_ime_reason = reason;
			}

			if (!fo->IsDisplayed())
				SetRect(OpRect(0, 0, GetWidth(), GetHeight()), FALSE);
		}
	}

	m_packed.focus_comes_from_scrollbar = FALSE;
	if (!m_suppress_ime && !m_suppress_ime_mouse_down)
		if (!IMEHandleFocusEventInt(focus, reason))
			return;

	m_suppress_ime_mouse_down = FALSE;
#endif // WIDGETS_IME_SUPPORT

	multi_edit->OnFocus(focus);

	if (focus)
	{
		caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		EnableCaretBlinking(TRUE);
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

		m_packed.is_changed = FALSE;

		ReportCaretPosition();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
		m_packed.enable_spellcheck_later = m_packed.enable_spellcheck_later && g_internal_spellcheck->SpellcheckEnabledByDefault();
		if (m_packed.never_had_focus && m_packed.enable_spellcheck_later && CanUseSpellcheck())
		{
			EnableSpellcheck();
		}
		m_packed.never_had_focus = FALSE;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	}
	else
	{
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		EnableCaretBlinking(FALSE);
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING
		caret_blink = 1;
		multi_edit->InvalidateCaret();
		multi_edit->BeginChange();
		if (multi_edit->HasSelectedText())
			multi_edit->InvalidateBlocks(multi_edit->sel_start.block, multi_edit->sel_stop.block);
		multi_edit->EndChange();

		// If the user has changed the text, invoke the listener
		if (m_packed.is_changed && listener)
			listener->OnChangeWhenLostFocus(this); // Invoke!

		// set ghost text
		if (IsEmpty())
		{
			// FIXME:OOM!
			OP_STATUS status = SetText(m_ghost_text);
			if (OpStatus::IsSuccess(status))
				m_packed.ghost_mode = TRUE;
		}
	}
}

#ifndef MOUSELESS

void OpMultilineEdit::OnMouseDown(const OpPoint &cpoint, MouseButton button, UINT8 nclicks)
{
#ifdef DISPLAY_NO_MULTICLICK_LOOP
	nclicks = nclicks <= 3 ? nclicks : 3;
#else
	nclicks = (nclicks-1)%3 + 1;
#endif

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_SUPPRESS_ON_MOUSE_DOWN)
	SuppressIMEOnMouseDown();
#endif

	if (listener && listener->OnMouseEventConsumable(this, -1, cpoint.x, cpoint.y, button, TRUE, nclicks))
		return;
	if (listener)
		listener->OnMouseEvent(this, -1, cpoint.x, cpoint.y, button, TRUE, nclicks);

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
	if( button == MOUSE_BUTTON_3)
	{
		if (!IsInWebForm())
		{
			SetFocus(FOCUS_REASON_MOUSE);
		}

		UpdateFont();

		OpPoint point = TranslatePoint(cpoint);
		multi_edit->SelectNothing(TRUE);
		multi_edit->SetCaretPos(point);
		caret_wanted_x = multi_edit->caret_pos.x;
		caret_blink = 0;

		g_clipboard_manager->SetMouseSelectionMode(TRUE);
		if( g_clipboard_manager->HasText() )
		{
			SelectNothing();
			g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
		}
		else
		{
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
			if( g_clipboard_manager->HasText() )
			{
				SelectNothing();
				g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			}
		}

		return;
	}
#endif

	if (button != MOUSE_BUTTON_1)
		return;

	if (!IsInWebForm())
		SetFocus(FOCUS_REASON_MOUSE);

#ifdef WIDGETS_IME_SUPPORT
	if (m_packed.im_is_composing) // We shall not be able to place the caret when we are composing with IME
		return;
#endif

	OpRect inner_rect = GetBounds();
	GetInfo()->AddBorder(this, &inner_rect);
	if (!inner_rect.Contains(cpoint))
		return; // We clicked the border

	UpdateFont();

	multi_edit->BeginChange();

	OpPoint point = TranslatePoint(cpoint);

	sel_start_point = point;

	BOOL shift = (vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT) != 0;

	if (nclicks == 3)
	{
# if defined _TRIPLE_CLICK_ONE_LINE_
		SelectOneLine();
		is_selecting = m_packed.is_label_mode ? 0 : 3;
# else
		SelectAll();
# endif
		is_selecting = 3;
	}
	else if (nclicks == 1 && shift) // Select from current caretpos to new caretpos
	{
		OP_TCINFO* info = TCGetInfo();
		INT32 old_pos_glob;

		if (!HasSelectedText())
			old_pos_glob = multi_edit->caret.GetGlobalOffset(info);
		else if (multi_edit->caret == multi_edit->sel_start)
			old_pos_glob = multi_edit->sel_start.GetGlobalOffset(info);
		else
			old_pos_glob = multi_edit->sel_stop.GetGlobalOffset(info);

		multi_edit->SetCaretPos(point);
		INT32 new_pos_glob = multi_edit->caret.GetGlobalOffset(info);
		multi_edit->SelectToCaret(old_pos_glob, new_pos_glob);
	}
	else if (!IsDraggable(cpoint))
	{
		multi_edit->SelectNothing(TRUE);
		multi_edit->SetCaretPos(point);
		if (nclicks == 2) // Select a word
		{
			is_selecting = m_packed.is_label_mode ? 0 : 2;

			multi_edit->MoveToStartOrEndOfWord(FALSE, FALSE);
			int start_ofs = multi_edit->caret.GetGlobalOffset(TCGetInfo());
			multi_edit->MoveToStartOrEndOfWord(TRUE, FALSE);
			int stop_ofs = multi_edit->caret.GetGlobalOffset(TCGetInfo());
			multi_edit->SetSelection(start_ofs, stop_ofs);
		}
		else
			is_selecting = m_packed.is_label_mode ? 0 : 1;
	}
	caret_wanted_x = multi_edit->caret_pos.x;
	caret_blink = 0;

	multi_edit->EndChange();

	ReportCaretPosition();

#ifdef PAN_SUPPORT
	if (vis_dev->GetPanState() != NO)
		is_selecting = 0;
#elif defined GRAB_AND_SCROLL
	if (MouseListener::GetGrabScrollState() != NO)
		is_selecting = 0;
#endif
}



void OpMultilineEdit::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener && listener->OnMouseEventConsumable(this, -1, point.x, point.y, button, FALSE, nclicks))
		return;

	OpRect rect(0, 0, multi_edit->visible_width, multi_edit->visible_height);
	if (rect.Contains(point) && listener)
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, nclicks);
	GetInfo()->AddBorder(this, &rect);

	if (button == MOUSE_BUTTON_1)
	{
		BOOL shift = (vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT) != 0;
#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
		if( is_selecting && HasSelectedText() )
		{
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			Copy();
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
		}
#endif
		if (is_selecting && listener && HasSelectedText())
		{
			listener->OnSelect(this);
			m_packed.determine_select_direction = TRUE;
		}
		else if (!is_selecting && HasSelectedText() && !shift
#ifdef DRAG_SUPPORT
			&& !g_drag_manager->IsDragging() && GetClickableBounds().Contains(point)
#endif // DRAG_SUPPORT
			)
		{
			UpdateFont();
			multi_edit->SetCaretPos(TranslatePoint(point));
			SelectNothing();
		}

#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		EnableCaretBlinking(TRUE);
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING
	}

	is_selecting = 0;

	if (GetBounds().Contains(point) && listener && !IsDead())
		listener->OnClick(this); // Invoke!

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT)
	if (NULL == imstring)
	{
		SpawnIME(vis_dev, im_style.CStr(), (!m_packed.is_readonly) ? OpView::IME_MODE_TEXT : OpView::IME_MODE_UNKNOWN, GetIMEContext(GetFormObject()));
	}
#endif // defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT)
}

BOOL OpMultilineEdit::OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (listener)
		listener->OnContextMenu(this, -1, menu_point, avoid_rect, keyboard_invoked);

	return TRUE;
}

void OpMultilineEdit::OnMouseMove(const OpPoint &cpoint)
{
	if (is_selecting == 1 || is_selecting == 2)
	{
		UpdateFont();
		OpPoint point = TranslatePoint(cpoint);

#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		// See if we need to scroll
		OpRect rect(0, 0, multi_edit->visible_width, multi_edit->visible_height);
		if (!rect.Contains(OpPoint(point.x - GetXScroll(), point.y - GetYScroll())))
		{
			OnTimer();
			StartTimer(GetInfo()->GetScrollDelay(FALSE, FALSE));
		}
		else
		{
			EnableCaretBlinking(TRUE);
		}
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

		// Select
		multi_edit->BeginChange();
		if (is_selecting == 2) // Word-by-word selecting
		{
			OP_TCINFO* info = TCGetInfo();

			int start_pos_glob = multi_edit->PointToOffset(sel_start_point).GetGlobalOffset(info);
			int stop_pos_glob = multi_edit->PointToOffset(point).GetGlobalOffset(info);
			BOOL forward = stop_pos_glob > start_pos_glob;

			multi_edit->SetCaretOfsGlobal(start_pos_glob);
			multi_edit->MoveToStartOrEndOfWord(!forward, FALSE);
			start_pos_glob = multi_edit->caret.GetGlobalOffset(info);

			multi_edit->SetCaretOfsGlobal(stop_pos_glob);
			multi_edit->MoveToStartOrEndOfWord(forward, FALSE);
			stop_pos_glob = multi_edit->caret.GetGlobalOffset(info);

			multi_edit->SetSelection(start_pos_glob, stop_pos_glob, FALSE);
		}
		else
		{
			multi_edit->SetCaretPos(point);
			multi_edit->SetSelection(sel_start_point, point);
		}
		multi_edit->EndChange();
	}
}

#endif // !MOUSELESS

void OpMultilineEdit::OutputText(UINT32 color)
{
	UpdateFont();

	INT32 ofs_x, ofs_y;
	GetLeftTopOffset(ofs_x, ofs_y);

	BOOL use_focused_colors = IsFocused()
		|| (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
	BOOL use_search_hit_colors = (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
		|| (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);

	OP_SYSTEM_COLOR fg_col_id = OpWidgetString::GetSelectionTextColor(use_search_hit_colors, use_focused_colors);
	OP_SYSTEM_COLOR bg_col_id = OpWidgetString::GetSelectionBackgroundColor(use_search_hit_colors, use_focused_colors);

	UINT32 fg_col = GetInfo()->GetSystemColor(fg_col_id);
	UINT32 bg_col = GetInfo()->GetSystemColor(bg_col_id);

	vis_dev->Translate(ofs_x, ofs_y);

	this->PaintMultiEdit(color, fg_col, bg_col,
		OpRect(GetXScroll(), GetYScroll(), multi_edit->visible_width, multi_edit->visible_height));

	vis_dev->Translate(-ofs_x, -ofs_y);
}

void OpMultilineEdit::PaintMultiEdit(UINT32 text_color, UINT32 selection_text_color,
	UINT32 background_selection_color, const OpRect& rect)
{
	multi_edit->Paint(text_color, selection_text_color, background_selection_color, 0, selection_highlight_type, rect);
}


#ifndef MOUSELESS

void OpMultilineEdit::OnSetCursor(const OpPoint &point)
{
	if (m_packed.is_label_mode)
	{
		SetCursor(CURSOR_DEFAULT_ARROW);
	}
	else
	{
		OpRect inner_rect = GetBounds();
		GetInfo()->AddBorder(this, &inner_rect);

		if (!inner_rect.Contains(point)) // We are over the border
			OpWidget::OnSetCursor(point);
		else
		{
#ifdef WIDGETS_DRAGNDROP_TEXT
			if (!is_selecting && IsDraggable(point))
			{
				OpWidget::OnSetCursor(point);
				return;
			}
#endif
			SetCursor(CURSOR_TEXT);
		}
	}
}

#endif // !MOUSELESS

void OpMultilineEdit::GetLeftTopOffset(int &x, int &y)
{
	OpRect border_rect(0, 0, 0, 0);
	GetInfo()->AddBorder(this, &border_rect);
	x = border_rect.x - GetXScroll() + GetPaddingLeft();
	y = border_rect.y - GetYScroll() + GetPaddingTop();
	if(LeftHandedUI() && y_scroll->IsVisible())
		x += y_scroll->GetWidth();

	WIDGET_V_ALIGN v_align = GetVerticalAlign();
	if (v_align == WIDGET_V_ALIGN_MIDDLE && multi_edit->total_height < multi_edit->visible_height)
		y += (multi_edit->visible_height - multi_edit->total_height) / 2;
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT

BOOL OpMultilineEdit::CanUseSpellcheck()
{
	return !m_packed.flatmode && !m_packed.is_readonly && IsEnabled() && !IsDead() && !m_packed.is_label_mode;
}

BOOL OpMultilineEdit::SpellcheckByUser()
{
	return multi_edit->SpellcheckByUser();
}

void OpMultilineEdit::EnableSpellcheck()
{
	m_packed.enable_spellcheck_later = !CanUseSpellcheck() || !g_internal_spellcheck->SpellcheckEnabledByDefault();

	if (!m_packed.enable_spellcheck_later && !multi_edit->GetSpellCheckerSession())
		m_packed.enable_spellcheck_later = !!OpStatus::IsError(multi_edit->EnableSpellcheckInternal(FALSE /*by_user*/, NULL /*lang*/));
}

void OpMultilineEdit::DisableSpellcheck(BOOL force)
{
	multi_edit->DisableSpellcheckInternal(FALSE /*by_user*/, force);
	m_packed.enable_spellcheck_later = FALSE;
}


int OpMultilineEdit::CreateSpellSessionId(OpPoint* point /* = NULL */)
{
	int spell_session_id = 0;
	if(multi_edit && CanUseSpellcheck())
	{
		OpPoint p;
		if (point)
		{
			p = TranslatePoint(*point);
		}
		else
		{
			// Caret position
			// FIXME
		}
		multi_edit->CreateSpellUISession(p, spell_session_id);
	}
	return spell_session_id;
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WIDGETS_IME_SUPPORT

int offset_half_newlines(const uni_char *str, int pos)
{
	int diff = 0;
	for(int i = 1; i < pos; i++)
	{
		if (str[i - 1] != '\r' && str[i] == '\n')
			diff++;
	}
	return pos + diff;
}

int strlen_offset_half_newlines(const uni_char *str)
{
	return offset_half_newlines(str, uni_strlen(str));
}

IM_WIDGETINFO OpMultilineEdit::GetIMInfo()
{
	if (GetFormObject())
		GetFormObject()->UpdatePosition();

	IM_WIDGETINFO info;
	INT32 ofs_x, ofs_y;
	GetLeftTopOffset(ofs_x, ofs_y);

	//INT32 height = multi_edit->caret_link ? multi_edit->caret_link->GetHeight() : multi_edit->line_height;
	INT32 height = multi_edit->line_height;
	info.rect.Set(multi_edit->caret_pos.x + ofs_x,
				multi_edit->caret_pos.y + ofs_y,
				0, height);
	info.rect.OffsetBy(GetDocumentRect(TRUE).TopLeft());
	info.rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
	info.rect = GetVisualDevice()->ScaleToScreen(info.rect);

	OpPoint screenpos(0, 0);
	screenpos = GetVisualDevice()->GetView()->ConvertToScreen(screenpos);
	info.rect.OffsetBy(screenpos);

	info.widget_rect = GetDocumentRect(TRUE);
	info.widget_rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
	info.widget_rect = GetVisualDevice()->ScaleToScreen(info.widget_rect);
	info.widget_rect.OffsetBy(screenpos);

	info.font = NULL;
	info.has_font_info = TRUE;
	info.font_height = font_info.size;
	if (font_info.italic) info.font_style |= WIDGET_FONT_STYLE_ITALIC;
	if (font_info.weight >= 6) info.font_style |= WIDGET_FONT_STYLE_BOLD; // See OpFontManager::CreateFont() in OpFont.h for the explanation
	if (font_info.font_info->Monospace()) info.font_style |= WIDGET_FONT_STYLE_MONOSPACE;
	info.is_multiline = TRUE;
	return info;
}

IM_WIDGETINFO OpMultilineEdit::OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose)
{
	if (multi_edit->HasSelectedText())
		im_pos = multi_edit->sel_start.GetGlobalOffset(TCGetInfo());
	else
		im_pos = multi_edit->caret.GetGlobalOffset(TCGetInfo());
	if (compose == IM_COMPOSE_WORD)
	{
		OP_ASSERT(!"Implement!");
	}
	else if (compose == IM_COMPOSE_ALL)
	{
		OpString text;
		GetText(text, FALSE);
		imstring->Set(text, text.Length());
		im_pos = 0; // The whole string is edited.
	}

	imstring->SetCaretPos(multi_edit->caret.GetGlobalOffset(TCGetInfo()) - im_pos);
#ifdef IME_SEND_KEY_EVENTS
	previous_ime_len = uni_strlen(imstring->Get());
#endif // IME_SEND_KEY_EVENTS

	im_compose_method = compose;
	this->imstring = (OpInputMethodString*) imstring;
	InvalidateAll();
	return GetIMInfo();
}

IM_WIDGETINFO OpMultilineEdit::GetWidgetInfo()
{
	return GetIMInfo();
}

IM_WIDGETINFO OpMultilineEdit::OnCompose()
{
	if (imstring == NULL)
	{
		IM_WIDGETINFO info;
		info.font = NULL;
		info.is_multiline = TRUE;
		return info;
	}

	UpdateFont();

	multi_edit->BeginChange();
	if (m_packed.im_is_composing)
		Undo(TRUE, FALSE);
	m_packed.im_is_composing = TRUE;
	m_packed.is_changed = TRUE;

	if (im_compose_method == IM_COMPOSE_ALL)
	{
		OpInputMethodString *real_imstring = imstring;
		imstring = NULL;
		SetText(UNI_L(""));
		imstring = real_imstring;
	}

	int ime_caret_pos = offset_half_newlines(imstring->Get(), imstring->GetCaretPos());
	int ime_cand_pos = offset_half_newlines(imstring->Get(), imstring->GetCandidatePos());
	int ime_cand_length = offset_half_newlines(imstring->Get(), imstring->GetCandidatePos() + imstring->GetCandidateLength()) - ime_cand_pos;

#ifdef WIDGETS_LIMIT_TEXTAREA_SIZE
	INT32 max_bytes = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaximumBytesTextarea) / sizeof(uni_char);
	int space_left = max_bytes - GetTextLength(FALSE);
	if (space_left < 0)
		space_left = 0;
	int ime_string_len = MIN(uni_strlen(imstring->Get()), space_left);
	ime_caret_pos = MIN(ime_caret_pos, ime_string_len);
	ime_cand_pos = MIN(ime_cand_pos, ime_string_len);
	ime_cand_length = MIN(ime_cand_length, ime_cand_pos + ime_string_len);
	OpString tmp;
	tmp.Append(imstring->Get(), ime_string_len);
	imstring->Set(tmp, ime_string_len);
	imstring->SetCaretPos(ime_caret_pos);
	imstring->SetCandidate(ime_cand_pos, ime_cand_length);
#elif defined(IME_SEND_KEY_EVENTS)
	int ime_string_len = uni_strlen(imstring->Get());
#endif // WIDGETS_LIMIT_TEXTAREA_SIZE
#ifdef IME_SEND_KEY_EVENTS
	const uni_char* str = imstring->Get();
	BOOL ends_with_enter = FALSE; // enter is actually \r\n, so we need to send OP_KEY_ENTER
	if (ime_string_len > 1 && str[ime_string_len - 1] == '\n')
	{
		ends_with_enter = TRUE;
	}
#endif // IME_SEND_KEY_EVENTS

	REPORT_IF_OOM(multi_edit->InsertText(imstring->Get(), uni_strlen(imstring->Get()), TRUE, FALSE)); // FALSE because we must NOT let the undostack append this event.
	OP_ASSERT((size_t)imstring->GetCaretPos() <= uni_strlen(imstring->Get()));
	multi_edit->SetCaretOfsGlobal(im_pos + ime_caret_pos);
	multi_edit->EndChange();

	if (imstring->GetCandidateLength())
		multi_edit->SetSelection(im_pos + ime_cand_pos, im_pos + ime_cand_pos + ime_cand_length);

	// Let the form object know that its value has changed.
	if (listener)
		listener->OnChange(this, FALSE);

#ifdef IME_SEND_KEY_EVENTS
	if (ime_string_len != previous_ime_len)
	{
		uni_char key;
		if (previous_ime_len > ime_string_len)
			key = OP_KEY_BACKSPACE;
		else if (ends_with_enter)
			key = OP_KEY_ENTER;
		else
			key = str[ime_string_len - 1];

		m_fake_keys++;
		g_input_manager->InvokeKeyDown(OpKey::FromString(&key), 0);
		g_input_manager->InvokeKeyPressed(OpKey::FromString(&key), 0);
		g_input_manager->InvokeKeyUp(OpKey::FromString(&key), 0);
	}
	previous_ime_len = ime_string_len;
#endif // IME_SEND_KEY_EVENTS

	ScrollIfNeeded(TRUE);
	return GetIMInfo();
}

void OpMultilineEdit::OnCommitResult()
{
	if (imstring == NULL)
		return;

	OnCompose();
	multi_edit->SetCaretOfsGlobal(im_pos + strlen_offset_half_newlines(imstring->Get()));
	im_pos = multi_edit->caret.GetGlobalOffset(TCGetInfo());
	m_packed.im_is_composing = FALSE;

	SetReceivedUserInput();

	if (listener)
		listener->OnChange(this, FALSE);
}

void OpMultilineEdit::OnStopComposing(BOOL canceled)
{
	if (imstring == NULL)
		return;

	if (canceled && m_packed.im_is_composing)
	{
		Undo(TRUE);
		multi_edit->SetCaretOfsGlobal(im_pos);
	}

	if (!canceled)
		/** Note: We used the 'real' selection for candidate highlight so we need to reset it.
			  This isn't needed in OpEdit because it doesn't touch the 'real' selection. */
		multi_edit->SelectNothing(TRUE);

	m_packed.im_is_composing = FALSE;
	imstring = NULL;
	caret_wanted_x = multi_edit->caret_pos.x;
	InvalidateAll();

	if (!canceled)
		SetReceivedUserInput();
}

OP_STATUS OpMultilineEdit::SetIMStyle(const uni_char* style)
{
	if (im_style.Compare(style) != 0)
	{
		RETURN_IF_ERROR(im_style.Set(style));
		if (IsFocused())
		{
			// Set the current wanted input method mode.
			OpView::IME_MODE mode = m_packed.is_readonly ? OpView::IME_MODE_UNKNOWN : OpView::IME_MODE_TEXT;
			vis_dev->GetView()->GetOpView()->SetInputMethodMode(mode, GetIMEContext(GetFormObject()), im_style);
		}
	}
	return OpStatus::OK;
}

#ifdef IME_RECONVERT_SUPPORT
void OpMultilineEdit::OnPrepareReconvert(const uni_char*& str, int& psel_start, int& psel_stop)
{
	if (multi_edit->HasSelectedText())// set str to the first block of selection and (psel_start,psel_stop) to the selection
	{
		str = multi_edit->sel_start.block->CStr();
		psel_start = multi_edit->sel_start.ofs;
		if (multi_edit->sel_start.block == multi_edit->sel_stop.block)
		{
			psel_stop = multi_edit->sel_stop.ofs;
		}
		else
		{
			psel_stop = multi_edit->sel_start.block->text_len;
		}
	}
	else // no selection exists, set str to caret.block and both psel_start and psel_stop to caret.ofs
	{
		str = multi_edit->caret.block->CStr();
		psel_stop = psel_start = multi_edit->caret.ofs;
	}
}

void OpMultilineEdit::OnReconvertRange(int psel_start, int psel_stop)
{
	// psel_start and psel_stop are in the same block, set [psel_start,psel_stop) as selection

	OpTCOffset new_start,new_stop;

	// this ifelse is just to determine the block where the selection is in
	if (multi_edit->HasSelectedText())
	{
		OP_ASSERT(multi_edit->sel_start.ofs >= psel_start);

		new_start.block = multi_edit->sel_start.block;
		new_start.ofs = psel_start;

		new_stop.block = multi_edit->sel_start.block;
		new_stop.ofs = psel_stop;
	}
	else
	{
		new_start.block = multi_edit->caret.block;
		new_start.ofs = psel_start;

		new_stop.block = multi_edit->caret.block;
		new_stop.ofs = psel_stop;
	}

	multi_edit->SetCaret(new_start);
	multi_edit->SetSelection(new_start,new_stop);
}
#endif // IME_RECONVERT_SUPPORT
#endif // WIDGETS_IME_SUPPORT

INT32 OpMultilineEdit::GetPreferedHeight()
{
	OpRect rect;
	GetInfo()->AddBorder(this, &rect);
	return GetContentHeight() +
			GetPaddingTop() +
			GetPaddingBottom() - rect.height;
}

void OpMultilineEdit::SetLineHeight(int line_height)
{
	if (line_height != multi_edit->line_height)
	{
		int old = multi_edit->line_height;
		multi_edit->line_height = line_height;
		m_packed.line_height_set = TRUE;

		// Update layout with new lineheight
		if (old && vis_dev)
			ReformatNeeded(FALSE);
	}
}

INT32 OpMultilineEdit::GetVisibleLineHeight()
{
	int font_height = vis_dev->GetFontAscent() + vis_dev->GetFontDescent();
	return MAX(multi_edit->line_height, font_height);
}

BOOL OpMultilineEdit::UpdateScrollbars(BOOL keep_vertical/* = FALSE*/)
{
	OpRect border_rect(0, 0, rect.width, rect.height);
	GetInfo()->AddBorder(this, &border_rect);

	INT32 visible_w = border_rect.width - GetPaddingLeft() - GetPaddingRight();
	INT32 visible_h = border_rect.height - GetPaddingTop() - GetPaddingBottom();
	INT32 scrsizew = GetInfo()->GetVerticalScrollbarWidth();
	INT32 scrsizeh = GetInfo()->GetHorizontalScrollbarHeight();

	INT32 available_visible_w = visible_w;
	INT32 available_visible_h = visible_h;

	BOOL x_on = FALSE;
	BOOL y_on = FALSE;
	BOOL left_handed = LeftHandedUI();

	if (scrollbar_status_x == SCROLLBAR_STATUS_ON)
	{
		x_on = TRUE;
		visible_h -= scrsizeh;
	}
	if (scrollbar_status_y == SCROLLBAR_STATUS_ON || (keep_vertical && y_scroll->IsVisible()))
	{
		y_on = TRUE;
		visible_w -= scrsizew;
	}

	while (TRUE)
	{
		if (scrollbar_status_y == SCROLLBAR_STATUS_AUTO && !y_on)
		{
			if (multi_edit->total_height > visible_h &&
				multi_edit->line_height + scrsizew <= available_visible_w)
			{
				y_on = TRUE;
				visible_w -= scrsizew;
			}
		}
		if (scrollbar_status_x == SCROLLBAR_STATUS_AUTO && !x_on)
		{
			if (multi_edit->total_width > /*visible_w */available_visible_w &&
				multi_edit->line_height + scrsizeh <= available_visible_h)
			{
				x_on = TRUE;
				visible_h -= scrsizeh;
				continue;
			}
		}
		break;
	}

	if (x_on || y_on)
	{
		BOOL c_on = IsResizable();
		const OpRect &b = border_rect;

		OpRect xrect(b.x, b.y + b.height - scrsizeh, b.width - (int(y_on || c_on) * scrsizew) , scrsizeh);
		OpRect yrect(b.x + b.width - scrsizew, b.y, scrsizew, b.height - (int(x_on || c_on) * scrsizeh));

		if (left_handed)
		{
			yrect.x = b.x;
			yrect.height = b.height - (int(x_on) * scrsizeh);

			if (y_on)
			{
				xrect.x += scrsizew;
				xrect.width = b.width - (scrsizew * (1 + (int)c_on));
			}
		}

		y_scroll->SetRect(yrect);
		x_scroll->SetRect(xrect);

		if (GetParentOpWindow())
		{
#if defined(_MACINTOSH_)
			CocoaOpWindow* cocoa_window = (CocoaOpWindow*) GetParentOpWindow()->GetRootWindow();
			OpPoint deltas;
			if(cocoa_window && GetVisualDevice() && y_scroll->GetVisualDevice() && x_scroll->GetVisualDevice() && !GetVisualDevice()->IsPrinter())
			{
				deltas = cocoa_window->IntersectsGrowRegion(y_scroll->GetScreenRect());
				yrect.height -= deltas.y;
				y_scroll->SetRect(yrect);

				deltas = cocoa_window->IntersectsGrowRegion(x_scroll->GetScreenRect());
				xrect.width -= deltas.x;
				x_scroll->SetRect(xrect);
			}
#endif // _MACINTOSH_
		}
	}

	x_scroll->SetVisibility(x_on);
	y_scroll->SetVisibility(y_on);
	corner->SetVisibility(IsResizable() || (x_on && y_on));
	corner->SetHasScrollbars(x_on, LeftHandedUI() ? FALSE : y_on);

	BOOL need_reformat = (multi_edit->SetVisibleSize(visible_w, visible_h) && m_packed.is_wrapping);
	OP_ASSERT(!(keep_vertical && need_reformat));

	// when we limit the width of paragraphs it makes sense to do the
	// same for multiedits.
	INT32 layout_w = -1;
	FramesDocument* frm_doc = NULL;
	if (vis_dev && vis_dev->GetDocumentManager())
	{
		frm_doc = vis_dev->GetDocumentManager()->GetCurrentDoc();
		if (frm_doc)
			layout_w = frm_doc->GetMaxParagraphWidth();
	}
	need_reformat |= (multi_edit->SetLayoutWidth(layout_w) && m_packed.is_wrapping);

	x_scroll->SetEnabled(OpWidget::IsEnabled());
	y_scroll->SetEnabled(OpWidget::IsEnabled());

	y_scroll->SetSteps(multi_edit->line_height, multi_edit->visible_height);
	x_scroll->SetSteps(multi_edit->line_height, multi_edit->visible_width);

	y_scroll->SetLimit(0, multi_edit->total_height - multi_edit->visible_height, multi_edit->visible_height);
	x_scroll->SetLimit(0, multi_edit->total_width - multi_edit->visible_width, multi_edit->visible_width);

	INT32 csizew = 0, csizeh = 0;
	corner->GetPreferedSize(&csizew, &csizeh, 0, 0);

	corner->InvalidateAll();
	INT32 cx = border_rect.x + border_rect.width - csizew;
	INT32 cy = border_rect.y + border_rect.height - csizeh;
	corner->SetRect(OpRect(cx, cy, csizew, csizeh));

	return need_reformat;
}

INT32 OpMultilineEdit::GetXScroll()
{
	return x_scroll->GetValue();
}

INT32 OpMultilineEdit::GetYScroll()
{
	return y_scroll->GetValue();
}

OpRect OpMultilineEdit::GetVisibleRect()
{
	OpRect rect = GetBounds();
	GetInfo()->AddBorder(this, &rect);
	if (y_scroll->IsVisible())
	{
		if(LeftHandedUI())
			rect.x += GetInfo()->GetVerticalScrollbarWidth();
		rect.width -= GetInfo()->GetVerticalScrollbarWidth();
	}
	if (x_scroll->IsVisible())
		rect.height -= GetInfo()->GetHorizontalScrollbarHeight();
	return rect;
}

void OpMultilineEdit::UpdateFont()
{
	VisualDevice* vis_dev = GetVisualDevice();
	if (!vis_dev)
		return;

	vis_dev->SetFont(font_info.font_info->GetFontNumber());
	vis_dev->SetFontSize(font_info.size);
	vis_dev->SetFontWeight(font_info.weight);
	vis_dev->SetFontStyle(font_info.italic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL);
	vis_dev->SetCharSpacingExtra(font_info.char_spacing_extra);

	if (!m_packed.line_height_set)
	{
		multi_edit->line_height = vis_dev->GetLineHeight();
		m_packed.line_height_set = TRUE;
	}
}

OpPoint OpMultilineEdit::TranslatePoint(const OpPoint &point)
{
	int x, y;
	GetLeftTopOffset(x, y);
	return OpPoint(point.x - x, point.y - y);
}

BOOL OpMultilineEdit::IsScrollable(BOOL vertical)
{
	if( vertical )
		return ((OpScrollbar*)y_scroll)->CanScroll(ARROW_UP) || ((OpScrollbar*)y_scroll)->CanScroll(ARROW_DOWN);
	else
		return ((OpScrollbar*)x_scroll)->CanScroll(ARROW_LEFT) || ((OpScrollbar*)x_scroll)->CanScroll(ARROW_RIGHT);
}

void OpMultilineEdit::ScrollIfNeeded(BOOL include_document, BOOL smooth_scroll)
{
	// Scroll the content in the widget if needed

	if (!multi_edit->caret.block)
		return;
	INT32 xval = x_scroll->GetValue();
	INT32 yval = y_scroll->GetValue();
	INT32 caret_x = multi_edit->caret_pos.x;
	INT32 caret_y = multi_edit->caret_pos.y;
	INT32 caret_w = 1; // So we can scroll in a segment of text (IME candidate)

	int font_height = vis_dev->GetFontAscent() + vis_dev->GetFontDescent();
	int visible_line_height = MAX(multi_edit->line_height, font_height);

#ifdef WIDGETS_IME_SUPPORT
	if (imstring && imstring->GetCandidateLength())
	{
		OP_TCINFO* info = TCGetInfo();
		OpTCOffset start, stop;
		start.SetGlobalOffset(info, im_pos + imstring->GetCandidatePos());
		stop.SetGlobalOffset(info, im_pos + imstring->GetCandidatePos() + imstring->GetCandidateLength());
		OpPoint c_pos_start = start.GetPoint(info, TRUE);
		OpPoint c_pos_stop = stop.GetPoint(info, FALSE);
		caret_x = c_pos_start.x;
		caret_y = c_pos_start.y;
		if (c_pos_start.y == c_pos_stop.y)
			caret_w = c_pos_stop.x - c_pos_start.x;
		visible_line_height = MAX(visible_line_height, (c_pos_stop.y + visible_line_height) - c_pos_start.y);
	}
#endif

	if (caret_x + caret_w - xval > multi_edit->visible_width)
		xval = (caret_x + caret_w) - multi_edit->visible_width + 50;
	if (caret_x - xval < 0)
		xval = xval + (caret_x - xval);
	if (multi_edit->total_width < multi_edit->visible_width)
		xval = 0;
	x_scroll->SetValue(xval, TRUE, TRUE, smooth_scroll);

	if (caret_y - yval + visible_line_height > multi_edit->visible_height)
		yval = (caret_y + visible_line_height) - multi_edit->visible_height;
	if (caret_y - yval < 0)
		yval = yval + (caret_y - yval);
	if (multi_edit->total_height < multi_edit->visible_height)
		yval = 0;
	y_scroll->SetValue(yval, TRUE, TRUE, smooth_scroll);

	// Scroll the document if needed

	if (include_document && IsForm() && vis_dev->GetDocumentManager())
	{
		FramesDocument* doc = vis_dev->GetDocumentManager()->GetCurrentDoc();
		if (doc && !doc->IsReflowing())
		{
			OpRect caret_rect(caret_x - x_scroll->GetValue(),
							  caret_y - y_scroll->GetValue(),
							  1, visible_line_height);
			document_ctm.Apply(caret_rect);

			HTML_Element* html_element = GetFormObject() ? GetFormObject()->GetHTML_Element() : 0;
			doc->ScrollToRect(caret_rect, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS, html_element);
		}
	}
}

void OpMultilineEdit::TCOnContentChanging()
{
	m_packed.is_changed = TRUE;
}

void OpMultilineEdit::TCOnContentResized()
{
	if (UpdateScrollbars())
		ReformatNeeded(multi_edit->HasBeenSplit());
}

void OpMultilineEdit::TCInvalidate(const OpRect& crect)
{
	INT32 ofs_x, ofs_y;
	GetLeftTopOffset(ofs_x, ofs_y);

	OpRect rect = crect;
	rect.x += ofs_x;
	rect.y += ofs_y;
	Invalidate(rect);
}

void OpMultilineEdit::TCInvalidateAll()
{
	InvalidateAll();
}

OP_TCINFO* OpMultilineEdit::TCGetInfo()
{
	UpdateFont();
	OP_TCINFO* info = g_opera->widgets_module.tcinfo;
	info->tc = multi_edit;
	info->vis_dev = GetVisualDevice();
	info->wrap = m_packed.is_wrapping;
	info->aggressive_wrap = GetAggressiveWrapping();
	OP_ASSERT(info->wrap || !info->aggressive_wrap);
	info->rtl = GetRTL();
	info->readonly = m_packed.is_readonly;
	info->overstrike = m_packed.is_overstrike;
	info->selectable = !m_packed.is_label_mode;
	info->show_selection = !(m_packed.flatmode && !IsFocused()); // Don't show selection on unfocused flat widget
	info->show_caret = (
#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
	                    !HasSelectedText() &&
#endif
	                    !caret_blink && !info->readonly
	                    ) || m_packed.show_drag_caret;

#ifdef WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	// Never show caret if widgets keep focus after IME commit
	info->show_caret = FALSE;
#endif
	//info->font_height = info->vis_dev->GetFontHeight();
	info->font_height = info->vis_dev ? info->vis_dev->GetFontAscent() + info->vis_dev->GetFontDescent() : 0;
#ifdef COLOURED_MULTIEDIT_SUPPORT
	info->coloured = m_packed.coloured;
#endif
#ifdef WIDGETS_IME_SUPPORT
	info->ime_start_pos = im_pos;
	info->imestring = imstring;
#endif
	info->justify = font_info.justify;
	info->doc = NULL;
	info->original_fontnumber = font_info.font_info->GetFontNumber();
	info->preferred_script = WritingSystem::Unknown;
#ifdef FONTSWITCHING
	if (vis_dev && vis_dev->GetDocumentManager())
	{
		info->doc = vis_dev->GetDocumentManager()->GetCurrentDoc();
		if (info->doc && info->doc->GetHLDocProfile())
			info->preferred_script = info->doc->GetHLDocProfile()->GetPreferredScript();
	}
	// script takes preference over document codepage
	if (m_script != WritingSystem::Unknown)
		info->preferred_script = m_script;
#endif
#ifdef WIDGETS_IME_SUPPORT
	if (info->show_caret && imstring)
		info->show_caret = imstring->GetShowCaret();
#endif

#ifdef MULTILABEL_RICHTEXT_SUPPORT
	info->styles = NULL;
	info->style_count = 0;
#endif
	// let text areas relayout on zoom change also when true zoom is
	// enabled, since it won't affect page layout
	info->use_accurate_font_size = TRUE;

	return info;
}

void OpMultilineEdit::TCOnRTLDetected(BOOL is_rtl)
{
	SetRTL(is_rtl);
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
void OpMultilineEdit::TCOnTextCorrected()
{
	if (listener)
		listener->OnChange(this);
}
#endif

BOOL OpMultilineEdit::IsDraggable(const OpPoint& point)
{
#ifdef WIDGETS_DRAGNDROP_TEXT
	if (HasSelectedText())
	{
		UpdateFont();
		return IsWithinSelection(point.x, point.y);
	}
#endif
	return FALSE;
}

/***********************************************************************************
**
**	SetGhostText
**
***********************************************************************************/

OP_STATUS OpMultilineEdit::SetGhostText(const uni_char* ghost_text)
{
	op_free(m_ghost_text);
	m_ghost_text = uni_strdup(ghost_text ? ghost_text : UNI_L(""));
	RETURN_OOM_IF_NULL(m_ghost_text);
	OP_STATUS status = OpStatus::OK;
	// if conditions are met, set ghost text directly
	if (!IsFocused() && (IsEmpty() || m_packed.ghost_mode))
	{
		status = SetText(m_ghost_text);
		if (OpStatus::IsSuccess(status))
			m_packed.ghost_mode = TRUE;
	}
	return status;
}

void OpMultilineEdit::OnScaleChanged()
{
	// need to re-measure text when zoom level changes
	REPORT_IF_OOM(Reformat(TRUE));
}

#ifdef DRAG_SUPPORT

void OpMultilineEdit::OnDragStart(const OpPoint& cpoint)
{
#ifdef WIDGETS_DRAGNDROP_TEXT
	if (!is_selecting && IsDraggable(cpoint)
#ifdef DRAG_SUPPORT
		&& !g_drag_manager->IsDragging()
#endif // DRAG_SUPPORT
		)
	{
		uni_char* text = multi_edit->GetSelectionText();
		if (!text)
			return;

		OpDragObject* drag_object = NULL;
		RETURN_VOID_IF_ERROR(OpDragObject::Create(drag_object, OpTypedObject::DRAG_TYPE_TEXT));
		OP_ASSERT(drag_object);
		drag_object->SetDropType(DROP_COPY);
		drag_object->SetVisualDropType(DROP_COPY);
		drag_object->SetEffectsAllowed(DROP_COPY | DROP_MOVE);
		DragDrop_Data_Utils::SetText(drag_object, text);
		drag_object->SetSource(this);
		g_drag_manager->StartDrag(drag_object, GetWidgetContainer(), TRUE);

		OP_DELETEA(text);
	}
#endif // WIDGETS_DRAGNDROP_TEXT
}


/***********************************************************************************
**
**	OnDragMove/OnDragDrop
**
***********************************************************************************/


void OpMultilineEdit::OnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
	UpdateFont();
	if (m_packed.is_readonly)
	{
		drag_object->SetDropType(DROP_NOT_AVAILABLE);
		drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
	}
	else if (DragDrop_Data_Utils::HasText(drag_object) || DragDrop_Data_Utils::HasURL(drag_object))
	{
		DropType drop_type = drag_object->GetDropType();
		BOOL suggested_drop_type = drag_object->GetSuggestedDropType() != DROP_UNINITIALIZED;
		if (!suggested_drop_type) // Modify only when the suggested drop type is not used.
		{
			// Default to move withing the same widget.
			if (drag_object->GetSource() == this)
			{
				drag_object->SetDropType(DROP_MOVE);
				drag_object->SetVisualDropType(DROP_MOVE);
			}
			else if (drop_type == DROP_NOT_AVAILABLE || drop_type == DROP_UNINITIALIZED)
			{
				drag_object->SetDropType(DROP_COPY);
				drag_object->SetVisualDropType(DROP_COPY);
			}
		}

		if (drag_object->GetDropType() == DROP_NOT_AVAILABLE)
		{
			OnDragLeave(drag_object);
			return;
		}

		OpPoint new_caret_pos = TranslatePoint(point);
		if( !new_caret_pos.Equals(multi_edit->caret_pos) )
		{
			multi_edit->SetCaretPos(new_caret_pos);

			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			int caret_offset = GetCaretOffset();
			BOOL caret_in_selection = HasSelectedText() && caret_offset >= s1 && caret_offset <= s2;
			m_packed.show_drag_caret = caret_in_selection ? false : true;
			InvalidateAll();
		}
	}
	else
	{
		drag_object->SetDropType(DROP_NOT_AVAILABLE);
		drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
	}
}

void OpMultilineEdit::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	BOOL changed = FALSE;
	m_packed.show_drag_caret = FALSE;

	if (drag_object->GetDropType() == DROP_NOT_AVAILABLE)
	{
		OnDragLeave(drag_object);
		return;
	}

	const uni_char* dragged_text = DragDrop_Data_Utils::GetText(drag_object);
	TempBuffer buff;
	OpStatus::Ignore(DragDrop_Data_Utils::GetURL(drag_object, &buff));
	const uni_char* url = buff.GetStorage();
	if (dragged_text && drag_object->GetType() != OpTypedObject::DRAG_TYPE_WINDOW)
	{
		if( !*dragged_text )
		{
			drag_object->SetDropType(DROP_NOT_AVAILABLE);
			drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
			return;
		}

#ifdef WIDGETS_DRAGNDROP_TEXT
		if (drag_object->GetSource() == this && HasSelectedText())
		{
			UpdateFont();
			int insert_caret_pos = GetCaretOffset();
			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			if (insert_caret_pos >= s1 && insert_caret_pos <= s2)
				return;

			if (drag_object->GetDropType() == DROP_MOVE)
			{
				if (insert_caret_pos > s2)
					insert_caret_pos -= s2 - s1;
				OpInputAction action(OpInputAction::ACTION_DELETE);
				EditAction(&action);
			}

			SetCaretOffset(insert_caret_pos);
		}

		SelectNothing();
#endif // WIDGETS_DRAGNDROP_TEXT

		InsertText(dragged_text);
		changed = TRUE;
	}
	else if (url)
	{
		if( !*url )
		{
			drag_object->SetDropType(DROP_NOT_AVAILABLE);
			drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
			return;
		}

		InsertText(url);
		changed = TRUE;
	}

	if (changed && listener && !IsForm())
	{
		listener->OnChange(this, TRUE);
	}

	SetFocus(FOCUS_REASON_MOUSE);

	// The OnFocus have reset m_packed.is_changed, and we don't want to move it to before InsertText so we have to set it here.
	if (changed && IsForm())
		m_packed.is_changed = TRUE;
}


void OpMultilineEdit::OnDragLeave(OpDragObject* drag_object)
{
	if (m_packed.show_drag_caret)
	{
		m_packed.show_drag_caret = FALSE;
		InvalidateAll();
	}
	drag_object->SetDropType(DROP_NOT_AVAILABLE);
	drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
	SetCursor(CURSOR_NO_DROP);
}

#endif // DRAG_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OP_STATUS OpMultilineEdit::AccessibilitySetSelectedTextRange(int start, int end)
{
	SetSelection(start, end);
	return OpStatus::OK;
}

OP_STATUS OpMultilineEdit::AccessibilityGetSelectedTextRange(int &start, int &end)
{
	OpWidget::GetSelection(start, end);
	if (((start == 0) && (end == 0)))
		start = end = GetCaretOffset();
	return OpStatus::OK;
}

Accessibility::State OpMultilineEdit::AccessibilityGetState()
{
	Accessibility::State state = OpWidget::AccessibilityGetState();
	if (m_packed.is_readonly)
		state |= Accessibility::kAccessibilityStateReadOnly;
	return state;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

void OpMultilineEdit::PrepareOffsetAndArea(OpPoint& offset, OpRect& area, BOOL visible_part_only)
{
	GetLeftTopOffset(offset.x, offset.y);
	if (visible_part_only)
	{
		area.x = GetXScroll();
		area.y = GetYScroll();
		area.width = multi_edit->visible_width;
		area.height = multi_edit->visible_height;
	}
	else
	{
		area.x = 0;
		area.y = 0;
		area.width = multi_edit->total_width;
		area.height = multi_edit->total_height;
	}
}

OP_STATUS OpMultilineEdit::GetSelectionRects(OpVector<OpRect>* list, OpRect& union_rect, BOOL visible_part_only)
{
	OP_ASSERT(list);
	OpRect inner_rect;
	OpPoint offset;
	PrepareOffsetAndArea(offset, inner_rect, visible_part_only);

	return multi_edit->GetSelectionRects(list, union_rect, offset, inner_rect);
}

BOOL OpMultilineEdit::IsWithinSelection(int x, int y)
{
	if (!HasSelectedText())
		return FALSE;

	OpRect inner_rect;
	OpPoint offset;
	PrepareOffsetAndArea(offset, inner_rect, TRUE);
	offset.x += GetXScroll();
	offset.y += GetYScroll();

	OpPoint point(x + GetXScroll(), y + GetYScroll());
	return multi_edit->IsWithinSelection(point, offset, inner_rect);
}

#ifdef MULTILABEL_RICHTEXT_SUPPORT
DEFINE_CONSTRUCT(OpRichtextEdit)

OpRichtextEdit::OpRichtextEdit()
: OpMultilineEdit()
, m_styles_array(NULL)
, m_styles_array_size(0)
, m_pointed_link(NULL)
{
	m_link_col = OP_RGB(0x00, 0x00, 0xFF);
}

OpRichtextEdit::~OpRichtextEdit()
{
	ClearAllStyles();
}

XMLTokenHandler::Result OpRichtextEdit::HandleToken(XMLToken &token)
{
	StringAttribute				     *currAttr   = NULL;
	StringAttribute					 *parentAttr = NULL;
	const uni_char				     *tag_name   = NULL;
	UINT32							  tag_len    = 0;
	XMLToken::Type					  t			 = token.GetType();
	XMLToken::Attribute				 *attr		 = NULL;
	OP_STATUS						  status;

	if (m_attribute_stack.GetCount() > 0)
	{
		currAttr = m_attribute_stack.Get(m_attribute_stack.GetCount() - 1);
	}

	switch (t)
	{
		case XMLToken::TYPE_Text:
		{
			if (currAttr)
			{
				currAttr->range_length += token.GetLiteralLength();
			}
			status = m_current_string_value.Append(token.GetLiteralSimpleValue(), token.GetLiteralLength());
			if (OpStatus::IsMemoryError(status))
				return XMLTokenHandler::RESULT_OOM;
			else if (OpStatus::IsError(status))
				return XMLTokenHandler::RESULT_ERROR;
			break;
		}
		case XMLToken::TYPE_STag:
		{
			// Create string attribute and set it properly
			tag_name = token.GetName().GetLocalPart();
			tag_len  = token.GetName().GetLocalPartLength();
			currAttr = OP_NEW(StringAttribute, ());
			if (currAttr == NULL)
				return XMLTokenHandler::RESULT_OOM;

			OpAutoPtr<StringAttribute> currAttr_ap(currAttr);

			currAttr->range_start = m_current_string_value.Length();
			currAttr->range_length = 0;
			if (uni_strncmp(tag_name, UNI_L("i"), tag_len) == 0)
			{
				currAttr->attribute_type = StringAttribute::TYPE_ITALIC;
			}
			else if (uni_strncmp(tag_name, UNI_L("b"), tag_len) == 0)
			{
				currAttr->attribute_type = StringAttribute::TYPE_BOLD;
			}
			else if (uni_strncmp(tag_name, UNI_L("u"), tag_len) == 0)
			{
				currAttr->attribute_type = StringAttribute::TYPE_UNDERLINE;
			}
			else if (uni_strncmp(tag_name, UNI_L("a"), tag_len) == 0)
			{
				currAttr->attribute_type = StringAttribute::TYPE_LINK;
				attr = token.GetAttribute(UNI_L("href"));
				if (attr != NULL)
				{
					status  = currAttr->link_value.Set(attr->GetValue(), attr->GetValueLength());
					if (OpStatus::IsMemoryError(status))
						return XMLTokenHandler::RESULT_OOM;
					else if (OpStatus::IsError(status))
						return XMLTokenHandler::RESULT_ERROR;
				}
			}
			else if (uni_strncmp(tag_name, UNI_L("font"), tag_len) == 0)
			{
				currAttr->attribute_type = StringAttribute::TYPE_UNKNOWN;
				attr = token.GetAttribute(UNI_L("style"));
				if (attr != NULL)
				{
					if (uni_strncmp(attr->GetValue(), UNI_L("bold"), attr->GetValueLength()) == 0)
					{
						currAttr->attribute_type = StringAttribute::TYPE_BOLD;
					}
					else if (uni_strncmp(attr->GetValue(), UNI_L("large"), attr->GetValueLength()) == 0)
					{
						currAttr->attribute_type = StringAttribute::TYPE_HEADLINE;
					}
					else if (uni_strncmp(attr->GetValue(), UNI_L("italic"), attr->GetValueLength()) == 0)
					{
						currAttr->attribute_type = StringAttribute::TYPE_ITALIC;
					}
					else if (uni_strncmp(attr->GetValue(), UNI_L("headline"), attr->GetValueLength()) == 0)
					{
						currAttr->attribute_type = StringAttribute::TYPE_HEADLINE;
					}
					else if (uni_strncmp(attr->GetValue(), UNI_L("underline"), attr->GetValueLength()) == 0)
					{
						currAttr->attribute_type = StringAttribute::TYPE_UNDERLINE;
					}
				}
			}
			else
			{
				currAttr->attribute_type = StringAttribute::TYPE_UNKNOWN;
			}

			// Now put it on the stack
			if (OpStatus::IsSuccess(status = m_attribute_stack.Add(currAttr)))
				currAttr_ap.release();
			if (OpStatus::IsMemoryError(status))
				return XMLTokenHandler::RESULT_OOM;
			else if (OpStatus::IsError(status))
				return XMLTokenHandler::RESULT_ERROR;
			break;
		}
		case XMLToken::TYPE_ETag:
		{
			// First of all - if there's another attribute on the stack - we need to add our length to their length first
			if (m_attribute_stack.GetCount() > 1)
			{
				parentAttr = m_attribute_stack.Get(m_attribute_stack.GetCount() - 2);
				parentAttr->range_length += currAttr->range_length;
			}

			// Now - pop attribute from the stack and if it's not the unknown - add it to the list
			if (currAttr->attribute_type != StringAttribute::TYPE_UNKNOWN)
			{
				status = m_attribute_list.Add(currAttr);
				if (OpStatus::IsMemoryError(status))
					return XMLTokenHandler::RESULT_OOM;
				else if (OpStatus::IsError(status))
					return XMLTokenHandler::RESULT_ERROR;

				m_attribute_stack.Remove(m_attribute_stack.GetCount() - 1, 1);
			}
			else
			{
				m_attribute_stack.Delete(m_attribute_stack.GetCount() - 1, 1);
			}

			break;
		}
		default:
			break;
	}

	return XMLTokenHandler::RESULT_OK;
}

void OpRichtextEdit::ParsingStopped(XMLParser *parser)
{
	XMLTokenHandler::ParsingStopped(parser);
}

void OpRichtextEdit::ClearAllStyles()
{
	if (m_styles_array != NULL)
	{
		for(UINT32 i = 0; i < m_styles_array_size; i++)
		{
			op_free(m_styles_array[i]);
		}
		op_free(m_styles_array);
		m_styles_array = NULL;
		m_styles_array_size = 0;
	}

	m_links.DeleteAll();
}

OP_STATUS OpRichtextEdit::SetTextStyle(UINT32 start, UINT32 end, UINT32 style, const uni_char* param, int param_len)
{
	// Check if we're not going out of bounds
	if (start >= end || end > (UINT32)multi_edit->GetTextLength(FALSE))
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	if (m_styles_array == NULL)
	{
		// Create styles array with two elements
		m_styles_array = (OP_TCSTYLE **)op_malloc(sizeof(OP_TCSTYLE*)*2);
		if (m_styles_array == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		// Allocate node for start...
		m_styles_array[0] = (OP_TCSTYLE *)op_malloc(sizeof(OP_TCSTYLE));
		if (m_styles_array[0] == NULL)
		{
			op_free(m_styles_array);
			m_styles_array = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		m_styles_array[0]->position = start;
		m_styles_array[0]->style = style;
		m_styles_array[0]->param = (uni_char *)param;
		m_styles_array[0]->param_len = param_len;
		//...and end
		m_styles_array[1] = (OP_TCSTYLE*)op_malloc(sizeof(OP_TCSTYLE));
		if (m_styles_array[1] == NULL)
		{
			op_free(m_styles_array[0]);
			op_free(m_styles_array);
			m_styles_array = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		m_styles_array[1]->position = end;
		m_styles_array[1]->style = OP_TC_STYLE_NORMAL;
		m_styles_array[1]->param = (uni_char *)param;
		m_styles_array[1]->param_len = param_len;

		m_styles_array_size = 2;
	}
	else
	{
		OP_TCSTYLE *node = NULL;
		OP_TCSTYLE *node2 = NULL;
		UINT32 curpos = 0;
		UINT32 curpos_new = 0;
		UINT32 previous_style = OP_TC_STYLE_NORMAL;

		// Allocate additional two places for start and end (possibly we won't need it but...)
		OP_TCSTYLE **new_style_array = (OP_TCSTYLE**)op_malloc(sizeof(OP_TCSTYLE*)*(m_styles_array_size+2));
		if (new_style_array == NULL)
			return OpStatus::ERR_NO_MEMORY;

		// 1. First - scroll through stylesets that were till that moment
		node = m_styles_array[curpos++];

		while(node->position < start && curpos < m_styles_array_size)
		{
			new_style_array[curpos_new++] = node;
			previous_style = node->style;
			node = m_styles_array[curpos++];
		}

		//
		// 2. It's possible that we are at the end of the list
		//
		if (node->position < start)
		{
			new_style_array[curpos_new++] = node;
			previous_style = node->style;
			node = NULL;
		}

		//
		// 3. Now - if node points to the same starting place - modify it
		//
		if (node != NULL && node->position == start)
		{
			previous_style = node->style;
			node->style |= style;
			node->param = (uni_char *)param;
			node->param_len = param_len;
			new_style_array[curpos_new++] = node;
			node = NULL;
		}

		//
		// 4. If it's a bit "later" position - add new node before it with new style
		//
		else
		{
			node2 = node;

			node = (OP_TCSTYLE*)op_malloc(sizeof(OP_TCSTYLE));
			if (node == NULL)
			{
				op_free(new_style_array);
				return OpStatus::ERR_NO_MEMORY;
			}

			node->position = start;
			node->style = previous_style | style;
			node->param = (uni_char *)param;
			node->param_len = param_len;
			new_style_array[curpos_new++] = node;
		}

		//
		// 5. Now "scroll" to the position when we should put "end" sign,
		//    keeping in mind that we should update "previous_style" in the meantime.

		while(node2 != NULL && node2->position < end && curpos < m_styles_array_size)
		{
			new_style_array[curpos_new++] = node2;
			node2 = m_styles_array[curpos++];
			previous_style = node2->style;
		}

		//
		// 6. Again - it's possible that we are at the end of the list
		//
		if (node2 != NULL && node2->position < end)
		{
			new_style_array[curpos_new++] = node2;
			node2 = NULL;
		}

		//
		// 7. If node points to the same starting place - modify it
		//
		if (node2 != NULL && node2->position == end)
		{
			previous_style = node2->style;
			node2->style |= style;
			node2->param = (uni_char *)param;
			node2->param_len = param_len;
			new_style_array[curpos_new++] = node2;
			node2 = NULL;
		}
		//
		// 8. Create a new one before the chosen one otherwise
		//
		else
		{
			new_style_array[curpos_new+1] = node2;

			node2 = (OP_TCSTYLE*)op_malloc(sizeof(OP_TCSTYLE)*2);
			if (node2 == NULL)
			{
				op_free(node);
				op_free(new_style_array);
				return OpStatus::ERR_NO_MEMORY;
			}

			node2->position = end;
			node2->style = previous_style;
			node2->param = (uni_char *)param;
			node2->param_len = param_len;

			new_style_array[curpos_new++] = node2;

			if (new_style_array[curpos_new] != NULL) curpos_new++;
		}

		//
		// 9. Now copy all the remaining styles
		//
		OP_TCSTYLE *node3 = NULL;
		while(curpos < m_styles_array_size)
		{
			node3 = m_styles_array[curpos++];
			new_style_array[curpos_new++] = node3;
		}

		if (node)
			m_styles_array_size++;
		if (node2)
			m_styles_array_size++;

		OP_ASSERT(m_styles_array_size == curpos_new);
		op_free(m_styles_array);
		m_styles_array = new_style_array;
	}

	return OpStatus::OK;
}

OP_STATUS OpRichtextEdit::SetText(const uni_char* text)
{
	OpString current_text;
	XMLParser *p;
	URL blankURL;
	XMLParser::Configuration pConfig;
	OpString error_reason;
	INT32 text_len;
	OP_STATUS status;

	if (text == NULL)
	{
		text = UNI_L("");
		text_len = 0;
	}
	else
		text_len = uni_strlen(text);

	if (!m_raw_text_input.Compare(text))
		return OpStatus::OK;

	ClearAllStyles();

	UpdateFont();
	is_selecting = 0;

	// reset ghost mode flag - fixes CORE-23010
	m_packed.ghost_mode = FALSE;

	// It's time to parse. We need to parse the text first
	pConfig.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
	RETURN_IF_ERROR(XMLParser::Make(p, NULL, static_cast<MessageHandler *>(NULL), static_cast<XMLTokenHandler *>(this), blankURL));
	p->SetConfiguration(pConfig);

	status = p->Parse(text, uni_strlen(text), FALSE);

	OP_DELETE(p);

	if (OpStatus::IsSuccess(status))
	{
		status = multi_edit->SetText(m_current_string_value.CStr(), m_current_string_value.Length(), FALSE);
		if (OpStatus::IsSuccess(status))
		{
			UINT32 style = OP_TC_STYLE_NORMAL;
			StringAttribute *attribute = NULL;

			BeginChangeProperties();
			for (int i = m_attribute_list.GetCount() - 1; i >= 0 ; i--)
			{
				attribute = m_attribute_list.Get(i);
				switch(attribute->attribute_type)
				{
					case StringAttribute::TYPE_BOLD:       style = OP_TC_STYLE_BOLD; break;
					case StringAttribute::TYPE_ITALIC:     style = OP_TC_STYLE_ITALIC; break;
					case StringAttribute::TYPE_UNDERLINE:  style = OP_TC_STYLE_UNDERLINE; break;
					case StringAttribute::TYPE_HEADLINE:   style = OP_TC_STYLE_HEADLINE; break;
					case StringAttribute::TYPE_LINK:	   style = OP_TC_STYLE_LINK; break;
					default:							   style = OP_TC_STYLE_NORMAL; break;
				}

				if (style != OP_TC_STYLE_NORMAL)
				{
					if (style == OP_TC_STYLE_LINK)
					{
						OpString *link = OP_NEW(OpString, ());
						if (!link)
							status = OpStatus::ERR_NO_MEMORY;
						else
						{
							link->TakeOver(attribute->link_value);
							if (OpStatus::IsSuccess(status = m_links.Add(link)))
								status = SetTextStyle(attribute->range_start, attribute->range_start + attribute->range_length, style, link->CStr(), link->Length());
							else
								OP_DELETE(link);
						}
					}
					else
					{
						status = SetTextStyle(attribute->range_start, attribute->range_start + attribute->range_length, style);
					}
				}

				if (OpStatus::IsError(status))
					break;
			}

			ReformatNeeded(TRUE);
			EndChangeProperties();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
#endif
		}
	}

	// Reset parser variables
	m_current_string_value.Empty();
	m_attribute_list.DeleteAll();
	m_attribute_stack.DeleteAll();

	if (OpStatus::IsSuccess(status))
	{
		status = m_raw_text_input.Set(text);
	}

	return status;
}

#ifndef MOUSELESS
void OpRichtextEdit::OnSetCursor(const OpPoint &point)
{
	// This part is for styled widget only
	if (m_styles_array != NULL)
	{
		OpTCOffset offset = multi_edit->PointToOffset(TranslatePoint(point));
		m_pointed_link = (uni_char *)offset.block->GetLinkForOffset(TCGetInfo(), offset.ofs);
		if (NULL != m_pointed_link)
		{
			SetCursor(CURSOR_CUR_POINTER);
		}
		else
		{
			OpMultilineEdit::OnSetCursor(point);
		}
	}
	else
	{
		OpMultilineEdit::OnSetCursor(point);
	}
}
#endif //!MOUSELESS

void OpRichtextEdit::OutputText(UINT32 color)
{
	UpdateFont();

	INT32 ofs_x, ofs_y;
	GetLeftTopOffset(ofs_x, ofs_y);

	UINT32 fg_col, bg_col;
	if (IsFocused())
	{
		fg_col = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
		bg_col = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
	}
	else
	{
		fg_col = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS);
		bg_col = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS);
	}

	vis_dev->Translate(ofs_x, ofs_y);

	multi_edit->Paint(color, fg_col, bg_col, m_link_col, VD_TEXT_HIGHLIGHT_TYPE_SELECTION, OpRect(GetXScroll(), GetYScroll(), multi_edit->visible_width, multi_edit->visible_height));

	vis_dev->Translate(-ofs_x, -ofs_y);
}

OP_STATUS OpRichtextEdit::GetLinkPositions(OpVector<OpRect>& rects)
{

    rects.DeleteAll();

    for (UINT32 i = 0; i < m_styles_array_size; i++)
    {
        if ((m_styles_array[i]->style & OP_TC_STYLE_LINK) == OP_TC_STYLE_LINK)
        {
            multi_edit->SetCaretOfs(m_styles_array[i]->position);
            OpAutoPtr<OpRect> rect(OP_NEW(OpRect, ()));
            if (rect.get() == NULL)
                return OpStatus::ERR;

            rect->x = multi_edit->caret_pos.x;
            rect->y = multi_edit->caret_pos.y;

            // We don't need any more of this. This is only for watir
            rect->width = 10;
            rect->height = 10;

            RETURN_IF_ERROR(rects.Add(rect.get()));

            rect.release();
        }
    }

    return OpStatus::OK;
}

OP_TCINFO* OpRichtextEdit::TCGetInfo()
{
	OP_TCINFO* info = OpMultilineEdit::TCGetInfo();

	if (info)
	{
		info->styles = m_styles_array;
		info->style_count = m_styles_array_size;
	}
	return info;
}
#endif // MULTILABEL_RICHTEXT_SUPPORT
