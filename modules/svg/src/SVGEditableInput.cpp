/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_EDITABLE

#include "modules/pi/OpClipboard.h"

#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGTextSelection.h"

void SVGEditable::EditAction(OpInputAction* action)
{
	SVGCaretPoint old_caret_point = m_caret.m_point;

	switch (action->GetAction())
	{
#if 0
	case OpInputAction::ACTION_TOGGLE_STYLE_BOLD:
		m_caret.ToggleBold();
		break;

	case OpInputAction::ACTION_TOGGLE_STYLE_ITALIC:
		m_caret.ToggleItalic();
		break;

	case OpInputAction::ACTION_TOGGLE_STYLE_UNDERLINE:
		m_caret.ToggleUnderline();
		break;

	case OpInputAction::ACTION_TOGGLE_OVERSTRIKE:
		m_caret.ToggleOverstrike();
		break;
#endif

	case OpInputAction::ACTION_RANGE_GO_TO_START:
	case OpInputAction::ACTION_GO_TO_START:
		m_caret.Place(SVGEditableCaret::PLACE_START);
		break;

	case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
	case OpInputAction::ACTION_GO_TO_LINE_START:
		m_caret.Place(SVGEditableCaret::PLACE_LINESTART);
		break;

	case OpInputAction::ACTION_RANGE_GO_TO_END:
	case OpInputAction::ACTION_GO_TO_END:
		m_caret.Place(SVGEditableCaret::PLACE_END);
		break;

	case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
	case OpInputAction::ACTION_GO_TO_LINE_END:
		m_caret.Place(SVGEditableCaret::PLACE_LINEEND);
		break;

	case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
	case OpInputAction::ACTION_NEXT_CHARACTER:
		m_caret.Move(TRUE);
		break;
	case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
	case OpInputAction::ACTION_PREVIOUS_CHARACTER:
		m_caret.Move(FALSE);
		break;

	case OpInputAction::ACTION_RANGE_NEXT_WORD:
	case OpInputAction::ACTION_NEXT_WORD:
		m_caret.MoveWord(TRUE);
		break;
	case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
	case OpInputAction::ACTION_PREVIOUS_WORD:
		m_caret.MoveWord(FALSE);
		break;

	case OpInputAction::ACTION_NEXT_LINE_SPATIAL:
	case OpInputAction::ACTION_RANGE_NEXT_LINE:
	case OpInputAction::ACTION_NEXT_LINE:
		m_caret.Place(SVGEditableCaret::PLACE_LINENEXT);
		break;
	case OpInputAction::ACTION_PREVIOUS_LINE_SPATIAL:
	case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
	case OpInputAction::ACTION_PREVIOUS_LINE:
		m_caret.Place(SVGEditableCaret::PLACE_LINEPREVIOUS);
		break;

	case OpInputAction::ACTION_DELETE_WORD:
	case OpInputAction::ACTION_BACKSPACE_WORD:
	case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
	case OpInputAction::ACTION_DELETE:
	case OpInputAction::ACTION_BACKSPACE:
		{
			m_caret.LockUpdatePos(TRUE);
#ifdef SVG_SUPPORT_TEXTSELECTION
			SVGTextSelection* selection = GetTextSelection();
			if (selection->IsEmpty())
#endif // SVG_SUPPORT_TEXTSELECTION
			{
				switch(action->GetAction())
				{
				case OpInputAction::ACTION_DELETE_WORD:
					m_caret.Move(TRUE);
					break;
				case OpInputAction::ACTION_BACKSPACE_WORD:
					m_caret.Move(FALSE);
					break;
				case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
					m_caret.Place(SVGEditableCaret::PLACE_LINEEND);
					break;
				case OpInputAction::ACTION_DELETE:
					m_caret.Move(TRUE, TRUE /* Stop on first 'tbreak' */);
					break;
				case OpInputAction::ACTION_BACKSPACE:
					m_caret.Move(FALSE);
					break;
				};

				SVGCaretPoint start_cp = old_caret_point;
				SVGCaretPoint stop_cp = m_caret.m_point;

				if (start_cp.elm != stop_cp.elm)
				{
					if (stop_cp.elm->Precedes(start_cp.elm))
					{
						start_cp = m_caret.m_point;
						stop_cp = old_caret_point;
					}
				}
				else
				{
					start_cp.ofs = MIN(old_caret_point.ofs, m_caret.m_point.ofs);
					stop_cp.ofs = MAX(old_caret_point.ofs, m_caret.m_point.ofs);
				}

				RemoveContentCaret(start_cp, stop_cp);
			}
#ifdef SVG_SUPPORT_TEXTSELECTION
			else
			{
				SVGCaretPoint start_cp(selection->GetLogicalStartPoint());
				SVGCaretPoint stop_cp(selection->GetLogicalEndPoint());

				RemoveContentCaret(start_cp, stop_cp);

				selection->ClearSelection();
			}
#endif // SVG_SUPPORT_TEXTSELECTION

			m_caret.LockUpdatePos(FALSE);
			if (m_listener)
				m_listener->OnTextChanged();
		}
		break;

	case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
// 			if (m_readonly)
// 				break;
			OpKey::Code key = action->GetActionKeyCode();

			if (m_caret.m_point.elm->Parent()->GetIsPseudoElement())
				// Don't let us do something in pseudoelements.
				break;
#if 0
			if (!m_caret.IsElementEditable(m_caret.m_helm))
				// Don't let us do something in elements with contentEditable false.
				break;
#endif // 0

#ifdef SVG_SUPPORT_TEXTSELECTION
			SVGTextSelection* selection = GetTextSelection();
			if (!selection->IsEmpty())
			{
				SVGCaretPoint start_cp(selection->GetLogicalStartPoint());
				SVGCaretPoint stop_cp(selection->GetLogicalEndPoint());

				RemoveContentCaret(start_cp, stop_cp);

				selection->ClearSelection();
			}
#endif // SVG_SUPPORT_TEXTSELECTION

			if (key == OP_KEY_ENTER && IsMultiLine())
			{
				InsertBreak();
			}
			else
			{
				m_caret.LockUpdatePos(TRUE);
#if 0
				if (m_caret.m_overstrike//  && !m_selection.HasContent()
					)
				{
					m_caret.Move(TRUE);
					m_caret.Set(old_caret_helm, old_caret_ofs);
				}
#endif

				if (key == OP_KEY_ENTER)
				{
					uni_char instr[2] = { ' ', 0 };
					InsertText(instr, 1, TRUE);
				}
				else if (action->GetKeyValue())
					InsertText(action->GetKeyValue(), uni_strlen(action->GetKeyValue()), TRUE);

				m_caret.LockUpdatePos(FALSE);
			}
			if (m_listener)
				m_listener->OnTextChanged();
			break;
		}
	}

#ifdef SVG_SUPPORT_TEXTSELECTION
	if (action->IsRangeAction())
		SelectToCaret(old_caret_point);
	else if (action->IsMoveAction())
		SelectNothing();
#endif // SVG_SUPPORT_TEXTSELECTION

	ScrollIfNeeded();
}

BOOL SVGEditable::OnInputAction(OpInputAction* action)
{
	if (!GetDocument())
		return FALSE;

	switch (action->GetAction())
	{
	case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();
			if (child_action->IsKeyboardInvoked())
			{
				DOM_EventType key_event = DOM_EVENT_NONE;
				switch (child_action->GetAction())
				{
				case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
					key_event = ONKEYDOWN;
					break;
				case OpInputAction::ACTION_LOWLEVEL_KEY_UP:
					key_event = ONKEYUP;
					break;
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
					key_event = ONKEYPRESS;
					break;
				}
				if (key_event != DOM_EVENT_NONE)
				{
					OP_BOOLEAN result = FramesDocument::SendDocumentKeyEvent(GetDocument(), NULL, key_event, child_action->GetActionKeyCode(), child_action->GetKeyValue(), child_action->GetPlatformKeyEventData(), child_action->GetShiftKeys(), child_action->GetKeyRepeat(), child_action->GetKeyLocation(), 0);
					if (OpStatus::IsMemoryError(result))
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					return (result == OpBoolean::IS_TRUE);
				}
			}
			return FALSE;
		}
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_DELETE:
				case OpInputAction::ACTION_CLEAR:
					child_action->SetEnabled(TRUE); //GetTextLength() > 0);
					return TRUE;
#ifdef USE_OP_CLIPBOARD

				case OpInputAction::ACTION_CUT:
				case OpInputAction::ACTION_COPY:
					child_action->SetEnabled(g_clipboard_manager->ForceEnablingClipboardAction(child_action->GetAction(), GetDocument(), GetEditRoot()) ||
					                         HasSelectedText());
					return TRUE;
				case OpInputAction::ACTION_COPY_TO_NOTE:
					child_action->SetEnabled(HasSelectedText());
					return TRUE;

				case OpInputAction::ACTION_PASTE:
					child_action->SetEnabled(g_clipboard_manager->ForceEnablingClipboardAction(OpInputAction::ACTION_PASTE, GetDocument(), GetEditRoot()) ||
					                        g_clipboard_manager->HasText());
					return TRUE;
				case OpInputAction::ACTION_PASTE_AND_GO:
					child_action->SetEnabled(g_clipboard_manager->HasText());
					return TRUE;
#endif // USE_OP_CLIPBOARD
#ifndef HAS_NOTEXTSELECTION
				case OpInputAction::ACTION_SELECT_ALL:
					child_action->SetEnabled(TRUE); //GetTextLength() > 0);
					return TRUE;
#endif // !HAS_NOTEXTSELECTION

				case OpInputAction::ACTION_INSERT:
					child_action->SetEnabled(TRUE);
					return TRUE;
			}
			return FALSE;
		}
		break;
	case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			OpKey::Code key = action->GetActionKeyCode();
			const uni_char *key_value = action->GetKeyValue();
			BOOL is_key_wanted;

			switch (key)
			{
#ifdef OP_KEY_ENTER_ENABLED
			case OP_KEY_ENTER:
				is_key_wanted = TRUE;
				break;
#endif // OP_KEY_ENTER_ENABLED
#ifdef OP_KEY_TAB_ENABLED
			case OP_KEY_TAB:
				is_key_wanted = m_wants_tab && action->GetShiftKeys() == SHIFTKEY_NONE;
				break;
#endif // OP_KEY_TAB_ENABLED
			default:
				is_key_wanted = key_value != NULL && key_value[0] >= 32 && key_value[0] != 0x7f;
			}
			if (m_caret.m_point.IsValid() && is_key_wanted)
			{
				EditAction(action);
				return TRUE;
			}

			return FALSE;
		}

	case OpInputAction::ACTION_GO_TO_START:
	case OpInputAction::ACTION_GO_TO_END:
	case OpInputAction::ACTION_GO_TO_LINE_START:
	case OpInputAction::ACTION_GO_TO_LINE_END:
	case OpInputAction::ACTION_PAGE_UP:
	case OpInputAction::ACTION_PAGE_DOWN:
	case OpInputAction::ACTION_NEXT_CHARACTER:
	case OpInputAction::ACTION_PREVIOUS_CHARACTER:
	case OpInputAction::ACTION_NEXT_CHARACTER_SPATIAL:
	case OpInputAction::ACTION_PREVIOUS_CHARACTER_SPATIAL:
	case OpInputAction::ACTION_NEXT_WORD:
	case OpInputAction::ACTION_PREVIOUS_WORD:
	case OpInputAction::ACTION_NEXT_LINE:
	case OpInputAction::ACTION_PREVIOUS_LINE:
	case OpInputAction::ACTION_NEXT_LINE_SPATIAL:
	case OpInputAction::ACTION_PREVIOUS_LINE_SPATIAL:
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
	case OpInputAction::ACTION_DELETE:
	case OpInputAction::ACTION_DELETE_WORD:
	case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
	case OpInputAction::ACTION_BACKSPACE:
	case OpInputAction::ACTION_BACKSPACE_WORD:
#ifdef ACTION_TOGGLE_STYLE_BOLD_ENABLED
	case OpInputAction::ACTION_TOGGLE_STYLE_BOLD:
#endif // ACTION_TOGGLE_STYLE_BOLD_ENABLED
#ifdef ACTION_TOGGLE_STYLE_ITALIC_ENABLED
	case OpInputAction::ACTION_TOGGLE_STYLE_ITALIC:
#endif // ACTION_TOGGLE_STYLE_ITALIC_ENABLED
#ifdef ACTION_TOGGLE_STYLE_UNDERLINE_ENABLED
	case OpInputAction::ACTION_TOGGLE_STYLE_UNDERLINE:
#endif // ACTION_TOGGLE_STYLE_UNDERLINE_ENABLED
		{
			if (!m_caret.m_point.IsValid())
			{
				OP_ASSERT(!"If it is because we haven't initiated m_caret it is OK, otherwise not");
				return FALSE;
			}

			EditAction(action);
			return TRUE;
		}
#if 0
	case OpInputAction::ACTION_TOGGLE_OVERSTRIKE:
		{
			if (!m_caret.m_point.IsValid())
			{
				OP_ASSERT(!"If it is because we haven't initiated m_caret it is OK, otherwise not");
				return FALSE;
			}

			m_caret.ToggleOverstrike();
			return TRUE;
		}
#endif
#ifdef USE_OP_CLIPBOARD
	case OpInputAction::ACTION_CUT:
#ifdef SVG_SUPPORT_TEXTSELECTION
		{
			Cut();
		}
#endif // SVG_SUPPORT_TEXTSELECTION
		break;
	case OpInputAction::ACTION_COPY:
#ifdef SVG_SUPPORT_TEXTSELECTION
		Copy();
#endif // SVG_SUPPORT_TEXTSELECTION
		break;
	case OpInputAction::ACTION_PASTE:
		Paste();
		break;
	case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
		{
			if (!m_caret.m_point.IsValid())
			{
				OP_ASSERT(!"If it is because we haven't initiated m_caret it is OK, otherwise not");
				return FALSE;
			}

# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
# endif // _X11_SELECTION_POLICY_
			if( g_clipboard_manager->HasText() )
			{
				if( action->GetActionData() == 1 )
				{
					Clear();
				}
				Paste();
			}
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
# endif // _X11_SELECTION_POLICY_
			return TRUE;
		}
#endif // USE_OP_CLIPBOARD

	case OpInputAction::ACTION_SELECT_ALL:
#ifdef SVG_SUPPORT_TEXTSELECTION
		{
			SVGTextSelection* selection = GetTextSelection();
			selection->SelectAll(m_root);
		}
#endif // SVG_SUPPORT_TEXTSELECTION
		return TRUE;

	case OpInputAction::ACTION_CLEAR:
		Clear();
		return TRUE;

	case OpInputAction::ACTION_INSERT:
		if (!m_caret.m_point.IsValid())
		{
			OP_ASSERT(!"If it is because we haven't initiated m_caret it is OK, otherwise not");
			return FALSE;
		}

		InsertText(action->GetActionDataString(), uni_strlen(action->GetActionDataString()));
		return TRUE;

	case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
	case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
		if (m_caret.m_point.IsValid())
		{
			// Handle this?
		}
		return FALSE;
	}
	return FALSE;
}

void SVGEditable::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OnFocus(TRUE, reason);
}

void SVGEditable::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OnFocus(FALSE, reason);
}

#endif // SVG_SUPPORT_EDITABLE
#endif // SVG_SUPPORT
