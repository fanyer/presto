/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/doc/html_doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/pi/OpClipboard.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/winman.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/scrollable.h"
#include "modules/util/str.h"
#include "modules/dom/domeventtypes.h"

extern HTML_Element *GetParentListElm(HTML_Element *elm);

void OpDocumentEdit::EditAction(OpInputAction* action)
{
	DEBUG_CHECKER(TRUE);
	SelectionBoundaryPoint anchor, focus;
	if (m_doc->GetHtmlDocument())
		m_doc->GetHtmlDocument()->GetSelection(anchor, focus);
	HTML_Element* old_caret_helm = m_caret.GetElement();
	INT32 old_caret_ofs = m_caret.GetOffset();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	SpellWordInfoObject old_info;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEXT_CHARACTER_SPATIAL:
			m_caret.MoveSpatial(TRUE);
			break;
		case OpInputAction::ACTION_PREVIOUS_CHARACTER_SPATIAL:
			m_caret.MoveSpatial(FALSE);
			break;

		case OpInputAction::ACTION_RANGE_GO_TO_START:
		case OpInputAction::ACTION_GO_TO_START:
			m_caret.Place(OpWindowCommander::CARET_DOCUMENT_HOME);
			break;

		case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
		case OpInputAction::ACTION_GO_TO_LINE_START:
			m_caret.Place(GetRTL() ? OpWindowCommander::CARET_LINE_END : OpWindowCommander::CARET_LINE_HOME);
			break;

		case OpInputAction::ACTION_RANGE_GO_TO_END:
		case OpInputAction::ACTION_GO_TO_END:
			m_caret.Place(OpWindowCommander::CARET_DOCUMENT_END);
			break;

		case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
		case OpInputAction::ACTION_GO_TO_LINE_END:
			m_caret.Place(GetRTL() ? OpWindowCommander::CARET_LINE_HOME : OpWindowCommander::CARET_LINE_END);
			break;

		case OpInputAction::ACTION_RANGE_PAGE_UP:
		case OpInputAction::ACTION_PAGE_UP:
			m_caret.Place(OpWindowCommander::CARET_PAGEUP);
			break;
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
		case OpInputAction::ACTION_PAGE_DOWN:
			m_caret.Place(OpWindowCommander::CARET_PAGEDOWN);
			break;

		case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
		case OpInputAction::ACTION_NEXT_CHARACTER:
			if (m_selection.HasContent() && !action->IsRangeAction())
			{
				TextSelection* text_selection = m_doc->GetTextSelection();
				m_caret.Place(text_selection->GetEndSelectionPoint());
			}
			else
			{
				// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
				BOOL forward = TRUE;
				if (/*action->IsRangeAction() && */GetRTL())
					forward = !forward;
				m_caret.Move(forward, FALSE/*, !action->IsRangeAction()*/);
			}
			break;
		case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_PREVIOUS_CHARACTER:
			if (m_selection.HasContent() && !action->IsRangeAction())
			{
				TextSelection* text_selection = m_doc->GetTextSelection();
				m_caret.Place(text_selection->GetStartSelectionPoint());
			}
			else
			{
				// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
				BOOL forward = FALSE;
				if (/*action->IsRangeAction() && */GetRTL())
					forward = !forward;
				m_caret.Move(forward, FALSE/*, !action->IsRangeAction()*/);
			}
			break;

		case OpInputAction::ACTION_RANGE_NEXT_WORD:
		case OpInputAction::ACTION_NEXT_WORD:
			m_caret.Move(TRUE, TRUE);
			break;
		case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
		case OpInputAction::ACTION_PREVIOUS_WORD:
			m_caret.Move(FALSE, TRUE);
			break;

		case OpInputAction::ACTION_NEXT_LINE_SPATIAL:
		case OpInputAction::ACTION_RANGE_NEXT_LINE:
		case OpInputAction::ACTION_NEXT_LINE:
			m_caret.Place(OpWindowCommander::CARET_DOWN);
			break;
		case OpInputAction::ACTION_PREVIOUS_LINE_SPATIAL:
		case OpInputAction::ACTION_RANGE_PREVIOUS_LINE:
		case OpInputAction::ACTION_PREVIOUS_LINE:
			m_caret.Place(OpWindowCommander::CARET_UP);
			break;

		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_BACKSPACE:
			if (m_readonly)
				break;
			if (m_layout_modifier.IsActive())
			{
				m_layout_modifier.Delete();
				break;
			}
			m_caret.LockUpdatePos(TRUE);
			if (!m_selection.HasContent())
			{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				old_info.Set(m_caret.GetElement());
#endif // INTERNAL_SPELLCHECK_SUPPORT
				m_caret.SetNotRemoveWhenMoveIfUntouched();
				switch(action->GetAction())
				{
				case OpInputAction::ACTION_DELETE_WORD:
					m_caret.Move(TRUE, TRUE);
					break;
				case OpInputAction::ACTION_BACKSPACE_WORD:
					m_caret.Move(FALSE, TRUE);
					break;
				case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
					m_caret.Place(OpWindowCommander::CARET_LINE_END);
					break;
				case OpInputAction::ACTION_DELETE:
					m_caret.Move(TRUE, FALSE);
					break;
				case OpInputAction::ACTION_BACKSPACE:
					m_caret.Move(FALSE, FALSE);
					break;
				}
				HTML_Element* anchor_elm;
				int anchor_offset;
				TextSelection::ConvertPointToOldStyle(anchor, anchor_elm, anchor_offset);
				m_selection.SelectToCaret(anchor_elm, anchor_offset);
			}
			if (m_selection.HasContent())
			{
				m_selection.RemoveContent();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				DoSpellWordInfoUpdate(&old_info);
#endif // INTERNAL_SPELLCHECK_SUPPORT
			}
			else
			{
				HTML_Element* list = GetParentListElm(m_caret.GetElement());
				if (list && action->GetAction() == OpInputAction::ACTION_BACKSPACE && (m_body_is_root || list->IsContentEditable(TRUE)))
				{
					HTML_Element* new_caret_helm = NULL;
					int new_caret_ofs = 0;
					GetNearestCaretPos(list, &new_caret_helm, &new_caret_ofs, FALSE, FALSE);

					HTML_Element* root = m_doc->GetCaret()->GetContainingElementActual(list);
					BeginChange(root, CHANGE_FLAGS_ALLOW_APPEND);

					DeleteElement(list);
					ReflowAndUpdate();
					if (new_caret_helm)
						m_caret.Place(new_caret_helm, new_caret_ofs);
					else
						m_caret.PlaceFirst(root);
					EndChange(root);
				}
			}
			m_caret.LockUpdatePos(FALSE);
			if (m_listener)
				m_listener->OnTextChanged();
			break;

		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
			{
				if (m_caret.GetElement()->Parent()->GetIsPseudoElement())
					// Don't let us do something in pseudoelements.
					break;
				if (!m_caret.IsElementEditable(m_caret.GetElement()))
					// Don't let us do something in elements with contentEditable false.
					break;

				// Extract the text of the current element
				OpString string;
				int text_len = m_caret.GetElement()->GetTextContentLength() + 1;
				if (!string.Reserve(text_len))
					break;
				if (OpStatus::IsError(m_caret.GetElement()->GetTextContent(string.DataPtr(), text_len)))
					break;
				int hex_start = 0;

				// Find the hex string
				if (UnicodePoint charcode = ConvertHexToUnicode(0, m_caret.GetOffset(), hex_start, string.CStr()))
				{
					// Remove the hex string
					m_undo_stack.BeginGroup();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
					old_info.Set(m_caret.GetElement());
#endif // INTERNAL_SPELLCHECK_SUPPORT
					m_caret.SetNotRemoveWhenMoveIfUntouched();
					SelectionBoundaryPoint hex_start_point(old_caret_helm, hex_start);
					HTML_Element* hex_start_elm;
					int hex_start_offset;
					TextSelection::ConvertPointToOldStyle(hex_start_point, hex_start_elm, hex_start_offset);
					m_selection.SelectToCaret(hex_start_elm, hex_start_offset);
//					m_caret.Set(old_caret_helm, old_caret_ofs);
					m_selection.RemoveContent();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
					DoSpellWordInfoUpdate(&old_info);
#endif // INTERNAL_SPELLCHECK_SUPPORT

					// Insert the new text
					uni_char instr[3] = { 0, 0, 0 }; /* ARRAY OK 2011-11-07 peter */
					int len = Unicode::WriteUnicodePoint(instr, charcode);
					m_caret.LockUpdatePos(TRUE);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
					BOOL was_delayed_misspell = FALSE;
#endif // INTERNAL_SPELLCHECK_SUPPORT
					BOOL has_content = m_selection.HasContent();
					if (GetDoc()->GetCaretPainter()->GetOverstrike() && !has_content)
					{
						m_caret.Move(TRUE, FALSE);
						if (IsFriends(old_caret_helm, m_caret.GetElement()))
							m_selection.SelectToCaret(old_caret_helm, old_caret_ofs);
						m_caret.Set(old_caret_helm, old_caret_ofs);
					}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
					else if(!has_content && !m_layout_modifier.IsActive() && m_delay_misspell_word_info)
						was_delayed_misspell = TRUE;
#endif // INTERNAL_SPELLCHECK_SUPPORT

					InsertText(instr, len, TRUE);
					m_undo_stack.EndGroup();

					m_caret.LockUpdatePos(FALSE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
					if(m_spell_session)
						PossiblyDelayMisspell(was_delayed_misspell);
					m_doc_has_changed = FALSE;
#endif // INTERNAL_SPELLCHECK_SUPPORT

					if (m_listener)
						m_listener->OnTextChanged();
				}

				break;
			}

		case OpInputAction::ACTION_TOGGLE_STYLE_BOLD:
			execCommand(OP_DOCUMENT_EDIT_COMMAND_BOLD);
			if (m_listener)
				m_listener->OnTextChanged();
			break;
		case OpInputAction::ACTION_TOGGLE_STYLE_ITALIC:
			execCommand(OP_DOCUMENT_EDIT_COMMAND_ITALIC);
			if (m_listener)
				m_listener->OnTextChanged();
			break;
		case OpInputAction::ACTION_TOGGLE_STYLE_UNDERLINE:
			execCommand(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE);
			if (m_listener)
				m_listener->OnTextChanged();
			break;
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			if (m_readonly)
				break;
			OpKey::Code key = action->GetActionKeyCode();

			if (!m_caret.GetElement())
				// Nowhere to put the input.
				break;
			if (m_caret.GetElement()->Parent()->GetIsPseudoElement())
				// Don't let us do something in pseudoelements.
				break;
			if (!m_caret.IsElementEditable(m_caret.GetElement()))
				// Don't let us do something in elements with contentEditable false.
				break;

			if (key == OP_KEY_ENTER)
			{
				BOOL new_paragraph = action->GetShiftKeys() & SHIFTKEY_SHIFT ? FALSE : TRUE;
				BOOL break_list = TRUE;
#ifdef DOCUMENT_EDIT_USE_PARAGRAPH_BREAK
				break_list = new_paragraph;
#endif // DOCUMENT_EDIT_USE_PARAGRAPH_BREAK
				InsertBreak(break_list, new_paragraph);
			}
			else if (key == OP_KEY_TAB)
			{
				// FIX: if we don't use wrap_pre, we can't do this! we have to insert nbspaces then.
				uni_char instr[2] = { 9, 0 };
				InsertText(instr, 1, TRUE);
			}
			else
			{
				m_caret.LockUpdatePos(TRUE);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
				BOOL was_delayed_misspell = FALSE;
#endif // INTERNAL_SPELLCHECK_SUPPORT
				BOOL has_content = m_selection.HasContent();
				if (GetDoc()->GetCaretPainter()->GetOverstrike() && !has_content)
				{
					m_caret.Move(TRUE, FALSE);
					if (IsFriends(old_caret_helm, m_caret.GetElement()))
						m_selection.SelectToCaret(old_caret_helm, old_caret_ofs);
					m_caret.Set(old_caret_helm, old_caret_ofs);
				}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				else if(!has_content && !m_layout_modifier.IsActive() && m_delay_misspell_word_info)
					was_delayed_misspell = TRUE;
#endif // INTERNAL_SPELLCHECK_SUPPORT

				const uni_char *key_value = action->GetKeyValue();
				if (key_value)
					InsertText(key_value, uni_strlen(key_value), TRUE);

				m_caret.LockUpdatePos(FALSE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				if(m_spell_session)
					PossiblyDelayMisspell(was_delayed_misspell);
				m_doc_has_changed = FALSE;
#endif // INTERNAL_SPELLCHECK_SUPPORT
			}
			if (m_listener)
				m_listener->OnTextChanged();
			break;
		}
	}

	if (action->IsRangeAction() && anchor.GetElement())
	{
		HTML_Element* anchor_elm;
		int anchor_offset;
		TextSelection::ConvertPointToOldStyle(anchor, anchor_elm, anchor_offset);
		m_selection.SelectToCaret(anchor_elm, anchor_offset);
	}
	else if (action->IsMoveAction())
		m_selection.SelectNothing();

	BOOL IsUpDownAction(int action); // widgets/OpMultiEdit.cpp
	if (!IsUpDownAction(action->GetAction()))
		m_caret.UpdateWantedX();

	ScrollIfNeeded();
}

BOOL OpDocumentEdit::OnInputAction(OpInputAction* action)
{
	DEBUG_CHECKER(TRUE);
	if (!GetDoc())
		return FALSE;

	CheckLogTreeChanged(TRUE);

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
					OP_BOOLEAN result = FramesDocument::SendDocumentKeyEvent(GetDoc(), NULL, key_event, child_action->GetActionKeyCode(), child_action->GetKeyValue(), child_action->GetPlatformKeyEventData(), child_action->GetShiftKeys(), child_action->GetKeyRepeat(), child_action->GetKeyLocation(), static_cast<unsigned>(DOM_KEYEVENT_DOCUMENTEDIT));
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
			OpInputAction::Action action_type = child_action->GetAction();
#ifdef USE_OP_CLIPBOARD
			BOOL force_enabling = FALSE;
			if (action_type == OpInputAction::ACTION_CUT ||
			    action_type == OpInputAction::ACTION_COPY ||
			    action_type == OpInputAction::ACTION_PASTE)
			{
				HTML_Element* target = m_layout_modifier.IsActive() ? m_layout_modifier.m_helm : (m_selection.HasContent() ? m_selection.GetStartElement() : m_caret.GetElement());
				force_enabling = g_clipboard_manager->ForceEnablingClipboardAction(action_type, m_doc, target);
			}
#endif // USE_OP_CLIPBOARD
			switch (action_type)
			{
				case OpInputAction::ACTION_UNDO:
					child_action->SetEnabled(!m_readonly && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_UNDO));
					return TRUE;

				case OpInputAction::ACTION_REDO:
					child_action->SetEnabled(!m_readonly && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_REDO));
					return TRUE;

				case OpInputAction::ACTION_TOGGLE_STYLE_BOLD:
					child_action->SetEnabled(!m_readonly && !m_plain_text_mode && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_BOLD));
					return TRUE;
				case OpInputAction::ACTION_TOGGLE_STYLE_ITALIC:
					child_action->SetEnabled(!m_readonly && !m_plain_text_mode && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_ITALIC));
					return TRUE;
				case OpInputAction::ACTION_TOGGLE_STYLE_UNDERLINE:
					child_action->SetEnabled(!m_readonly && !m_plain_text_mode && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE));
					return TRUE;

#ifdef USE_OP_CLIPBOARD
				case OpInputAction::ACTION_CUT:
					child_action->SetEnabled(force_enabling || (!m_readonly && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_CUT)));
					return TRUE;
				case OpInputAction::ACTION_COPY:
					child_action->SetEnabled(force_enabling || queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_COPY));
					return TRUE;
				case OpInputAction::ACTION_PASTE:
					child_action->SetEnabled(force_enabling || (!m_readonly && queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_PASTE)));
					return TRUE;
#endif // USE_OP_CLIPBOARD
			}
			return FALSE;
		}
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			OpKey::Code key = action->GetActionKeyCode();
#if defined(_DOCEDIT_DEBUG) && defined(USE_OP_CLIPBOARD)
			if (key == OP_KEY_F10)
			{
				m_doc->GetLogicalDocument()->GetRoot()->DumpDebugTree();
				return TRUE;
			}
			if (key == OP_KEY_F11)
			{
				OpString text;
				GetTextHTML(text);

				OP_NEW_DBG("OpDocumentEdit::OnInputAction()", "docedit");
				OP_DBG_LEN(text.CStr(), text.Length());

				return TRUE;
			}
			if (key == OP_KEY_F9)
			{
				const uni_char* tmp = UNI_L("<table border=1><tr><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>&nbsp;</td><td>&nbsp;</td></tr></table>");
				InsertTextHTML(tmp, uni_strlen(tmp));
				return TRUE;
			}
#endif // _DOCEDIT_DEBUG && USE_OP_CLIPBOARD */

			ShiftKeyState modifiers = action->GetShiftKeys();
			const uni_char *key_value = action->GetKeyValue();
			BOOL is_key_wanted;
			switch (key)
			{
#ifdef OP_KEY_ENTER_ENABLED
			case OP_KEY_ENTER:
				is_key_wanted = (modifiers & (SHIFTKEY_CTRL | SHIFTKEY_ALT | SHIFTKEY_META)) == 0;
				break;
#endif // OP_KEY_ENTER_ENABLED
#ifdef OP_KEY_TAB_ENABLED
			case OP_KEY_TAB:
				is_key_wanted = m_wants_tab && modifiers == SHIFTKEY_NONE;
				break;
#endif // OP_KEY_TAB_ENABLED
			default:
				/* A keypress involving CTRL is ignored, but not if it involves
				   ALT (or META.) Some key mappings use those modifier combinations
				   for dead/accented keys (e.g., Ctrl-Alt-Z on a Polish keyboard.) */
				if ((modifiers & SHIFTKEY_CTRL) != 0 && (modifiers & (SHIFTKEY_ALT | SHIFTKEY_META)) == 0)
					is_key_wanted = FALSE;
				else
					is_key_wanted = key_value != NULL && key_value[0] >= 32 && key_value[0] != 0x7f;
			}

			if (m_caret.GetElement() && is_key_wanted)
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
		case OpInputAction::ACTION_TOGGLE_STYLE_BOLD:
		case OpInputAction::ACTION_TOGGLE_STYLE_ITALIC:
		case OpInputAction::ACTION_TOGGLE_STYLE_UNDERLINE:
		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		{
			if (!m_caret.GetElement())
			{
				// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
				return FALSE;
			}

			EditAction(action);
			return TRUE;
		}

		case OpInputAction::ACTION_TOGGLE_OVERSTRIKE:
		{
			if (!m_caret.GetElement())
			{
				// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
				return FALSE;
			}

			m_caret.SetOverstrike(!GetDoc()->GetCaretPainter()->GetOverstrike());
			return TRUE;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
			if (!m_caret.GetElement())
			{
				// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
				return FALSE;
			}

			SetRTL(action->GetAction() == OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL);
			return TRUE;
#endif // SUPPORT_TEXT_DIRECTION

		case OpInputAction::ACTION_UNDO:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_UNDO));
		case OpInputAction::ACTION_REDO:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_REDO));

#ifdef USE_OP_CLIPBOARD
		case OpInputAction::ACTION_CUT:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_CUT));
		case OpInputAction::ACTION_COPY:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_COPY));
		case OpInputAction::ACTION_PASTE:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_PASTE));
		case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
		{
			if (!m_caret.GetElement())
			{
				// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
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
				HTML_Element* target = m_selection.HasContent() ? m_selection.GetStartElement() : m_caret.GetElement();
				g_clipboard_manager->Paste(this, m_doc, target);
			}
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
# endif // _X11_SELECTION_POLICY_
			return TRUE;
		}
#endif // USE_OP_CLIPBOARD

		case OpInputAction::ACTION_SELECT_ALL:
			return OpStatus::IsSuccess(execCommand(OP_DOCUMENT_EDIT_COMMAND_SELECTALL));

#ifdef DEBUG_DOCUMENT_EDIT
		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_FORCE_RELOAD:
#endif
		case OpInputAction::ACTION_CLEAR:
			Clear();
			return TRUE;

		case OpInputAction::ACTION_INSERT:
			if (!m_caret.GetElement())
			{
				// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
				return FALSE;
			}

			InsertText(action->GetActionDataString(), uni_strlen(action->GetActionDataString()));
			return TRUE;

		case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
			if (m_caret.GetElement())
			{
				HTML_Element *ec = GetEditableContainer(m_caret.GetElement());
				if (ec && ec->Type() == HE_BODY)
				{
					FramesDocElm *fd_elm = m_doc->GetDocManager()->GetFrame();
					if (fd_elm && fd_elm->IsInlineFrame())
					{
						// This is a IFRAME so we should focus the parentdocument and call this action on it, so
						// focus will be moved within the parentdocument.
						fd_elm->GetCurrentDoc()->GetParentDoc()->GetVisualDevice()->SetFocus(FOCUS_REASON_KEYBOARD);
						OP_BOOLEAN res = fd_elm->GetCurrentDoc()->GetParentDoc()->OnInputAction(action);
						return res == OpBoolean::IS_TRUE;
					}
				}
			}
			return FALSE;
		case OpInputAction::ACTION_SCROLL_UP:
		case OpInputAction::ACTION_SCROLL_DOWN:
		case OpInputAction::ACTION_SCROLL_LEFT:
		case OpInputAction::ACTION_SCROLL_RIGHT:
			if (m_caret.GetElement())
			{
				// Redirect theese actions to a scrollablecontainer so selectionscroll works.
				// The ScrollableContainer is not a parent or child of the DocumentEdits inputcontext.
				HTML_Element *ec = GetEditableContainer(m_caret.GetElement());
				if (ec && ec->GetLayoutBox())
				{
					ScrollableArea* sc = ec->GetLayoutBox()->GetScrollable();
					if (sc)
						return sc->TriggerScrollInputAction(action);
				}
			}
			break;
	}
	return FALSE;
}

BOOL OpDocumentEdit::GetBoundingRect(OpRect &rect)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *ec = GetEditableContainer(m_caret.GetElement());
	if (ec && ec->GetLayoutBox())
	{
		if (OpInputContext* input_context = ec->GetLayoutBox()->GetScrollable())
			return input_context->GetBoundingRect(rect);
	}
	return FALSE;
}

void OpDocumentEdit::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	DEBUG_CHECKER(TRUE);
	OnFocus(TRUE, reason);
}

void OpDocumentEdit::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	DEBUG_CHECKER(TRUE);
	OnFocus(FALSE, reason);
}

#endif // DOCUMENT_EDIT_SUPPORT
