/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/hardcore/mh/constant.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/win.h"
#include "modules/pi/OpClipboard.h"
#include "modules/util/tempbuf.h"
#include "modules/layout/box/box.h"
#include "modules/layout/layout_workplace.h"
#include "modules/display/coreview/coreview.h"
#include "modules/style/css_property_list.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/logdoc_util.h" // for GetFirstFontNumber
#include "modules/logdoc/selection.h"
#include "modules/logdoc/html5parser.h"

// == Some local util stuff ==================================

//#define DEBUG_DOCUMENT_EDIT
#ifdef DEBUG_DOCUMENT_EDIT
# define DUMPDEBUGTREE m_doc->GetLogicalDocument()->GetRoot()->DumpDebugTree();
# define DUMPDEBUGTREE_ELM(elm) elm->DumpDebugTree();
#else
# define DUMPDEBUGTREE
# define DUMPDEBUGTREEE_ELM(elm)
#endif

BOOL IsDummyElement(HTML_Element* helm)
{
	DEBUG_CHECKER_STATIC();
	return (helm->Type() == HE_TEXT && helm->GetTextContentLength() == 1 && helm->TextContent()[0] == document_edit_dummy_str[0]);
}

BOOL HasAttribute(HTML_Element* helm, short attr)
{
	DEBUG_CHECKER_STATIC();
	if (attr == ATTR_NULL)
		return TRUE;
	return helm->HasAttr(attr);
}

BOOL ContainsLinebreak(const uni_char* text, int len)
{
	DEBUG_CHECKER_STATIC();
	for(int i = 0; i < len; i++)
		if (text[i] == '\r' || text[i] == '\n')
			return TRUE;
	return FALSE;
}

void Replace(OpString& str, const char* what, const char* with)
{
	DEBUG_CHECKER_STATIC();
	int pos;
	while((pos = str.FindI(what)) != KNotFound)
	{
		str.Delete(pos, op_strlen(what));
		str.Insert(pos, with);
	}
}

BOOL IsHeader(HTML_Element* helm)
{
	DEBUG_CHECKER_STATIC();
	switch(helm->Type())
	{
	case HE_H1:
	case HE_H2:
	case HE_H3:
	case HE_H4:
	case HE_H5:
	case HE_H6:
		return TRUE;
	}
	return FALSE;
}

BOOL IsStyleElementType(HTML_ElementType type)
{
	DEBUG_CHECKER_STATIC();
	switch(type)
	{
	case HE_FONT:
	case HE_BIG:
	case HE_SMALL:
	case HE_STRONG:
	case HE_B:
	case HE_EM:
	case HE_U:
	case HE_STRIKE:
	case HE_SUB:
	case HE_SUP:
	case HE_SPAN:
		return TRUE;
	}
	return FALSE;
}

BOOL ElementHandlesAlignAttribute(HTML_Element *helm)
{
	DEBUG_CHECKER_STATIC();
	return TRUE;
}

BOOL ChildrenShouldInheritAlignment(HTML_Element *helm)
{
	DEBUG_CHECKER_STATIC();
	switch(helm->Type())
	{
		case HE_TABLE:
			return FALSE;
	}
	return TRUE;
}

BOOL ElementHandlesMarginStyle(HTML_Element *helm)
{
	DEBUG_CHECKER_STATIC();
	switch(helm->Type())
	{
		case HE_TR:
		case HE_TD:
		case HE_TH:
		case HE_TBODY:
		case HE_THEAD:
		case HE_TFOOT:
			return FALSE;
	}
	return TRUE;
}

HTML_Element *GetMatchingParent(BOOL match_value, HTML_Element *helm, BOOL (*func)(HTML_Element*,void*), void *func_arg)
{
	DEBUG_CHECKER_STATIC();
	helm = helm->ParentActual();
	while(helm && helm->Type() != HE_BODY)
	{
		if(func(helm,func_arg) == match_value)
			return helm;
		helm = helm->ParentActual();
	}
	return NULL;
}

// The following are functions that might be used as arguments to OpDocumentEdit::GetSelectionMatchesFunction...

BOOL IsNonLeftAligned(HTML_Element *helm, void *dont_care_about_me)
{
	DEBUG_CHECKER_STATIC();
	return helm->GetAttr(ATTR_ALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_left) != (void*)CSS_VALUE_left;
}

BOOL IsAlignedAs(HTML_Element *helm, void *CSS_align_value)
{
	DEBUG_CHECKER_STATIC();
	return helm->GetAttr(ATTR_ALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_left) == CSS_align_value;
}

BOOL StaticIsMatchingType(HTML_Element *helm, void *elm_types)
{
	DEBUG_CHECKER_STATIC();
	HTML_ElementType *types = (HTML_ElementType*)elm_types;
	while(*types != HE_UNKNOWN)
	{
		if(helm->Type() == *types)
			return TRUE;
		types++;
	}
	return FALSE;
}

// End of functions for GetSelectionMatchesFunction.............................

// The following are functions that might be used as arguments to OpDocumentEdit::ApplyFunctionBetween...

OP_STATUS SetNewHref(HTML_Element *helm, void *arg, BOOL &ret)
{
	DEBUG_CHECKER_STATIC();
	OP_STATUS status;
	if(!helm || !arg)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	ret = FALSE;
	if(helm->Type() != HE_A)
		return OpStatus::OK;

	void **args = (void**)arg;
	OpDocumentEdit *edit = (OpDocumentEdit*)args[0];
	const uni_char *value = (const uni_char*)args[1];
	edit->OnElementChange(helm);
	status = helm->SetAttribute(edit->GetDoc(), ATTR_HREF, NULL, NS_IDX_DEFAULT, value, uni_strlen(value), NULL, FALSE);
	edit->OnElementChanged(helm);
	ret = OpStatus::IsSuccess(status);
	return status;
}

// End of functions for ApplyFunctionBetween.............................

/** Returns FALSE if helm is a TEXT element that is non-empty but only contains chars <= 32,
    or <=31 if whitespace_is_valid==TRUE. */
BOOL IsElmValidInListBlock(HTML_Element *helm, BOOL whitespace_is_valid = FALSE)
{
	DEBUG_CHECKER_STATIC();
	if(!helm)
		return FALSE;
	int max_invalid = whitespace_is_valid ? 31 : 32;
	if(helm->Type() == HE_TEXT && helm->GetTextContentLength())
	{
		const uni_char *txt = helm->TextContent();
		while(*txt && *txt <= max_invalid)
			txt++;
		if(!*txt)
			return FALSE;
	}
	return TRUE;
}

BOOL IsValidListBlock(SiblingBlock block, BOOL whitespace_is_valid = FALSE)
{
	DEBUG_CHECKER_STATIC();
	if(block.IsEmpty())
		return FALSE;
	HTML_Element *helm = block.start;
	while(TRUE)
	{
		if(IsElmValidInListBlock(helm,whitespace_is_valid))
			return TRUE;
		if(helm == block.stop)
			break;
		helm = helm->Suc();
		OP_ASSERT(helm);
	}
	return FALSE;
}

HTML_Element *GetAncestorOfType(HTML_Element *helm, HTML_ElementType type)
{
	DEBUG_CHECKER_STATIC();
	while(helm && helm->Type() != type)
		helm = helm->ParentActual();
	return helm;
}

HTML_Element *GetParentListElm(HTML_Element *elm)
{
	DEBUG_CHECKER_STATIC();
	while(elm)
	{
		if(elm->Type() == HE_OL || elm->Type() == HE_UL)
			return elm;
		elm = elm->Parent();
	}
	return NULL;
}

HTML_Element *GetRootList(HTML_Element *elm)
{
	DEBUG_CHECKER_STATIC();
	HTML_Element *root_list = NULL;
	OP_ASSERT(elm);
	if(!elm)
		return NULL;
	elm = elm->Parent();
	while(elm && elm->Type() != HE_BODY)
	{
		if(elm->Type() == HE_OL || elm->Type() == HE_UL)
			root_list = elm;
		elm = elm->Parent();
	}
	return root_list;
}

INT32 GetListNestling(HTML_Element *elm)
{
	DEBUG_CHECKER_STATIC();
	INT32 nestling = 0;
	OP_ASSERT(elm);
	if(!elm)
		return 0;
	elm = elm->Parent();
	while(elm && elm->Type() != HE_BODY)
	{
		if(elm->Type() == HE_OL || elm->Type() == HE_UL)
			nestling++;
		elm = elm->Parent();
	}
	return nestling;
}

void SetOrdering(SELECTION_ORDERING &ordering, HTML_ElementType type)
{
	DEBUG_CHECKER_STATIC();
	OP_ASSERT(type == HE_OL || type == HE_UL);
	if(ordering == UNKNOWN)
		ordering = type == HE_OL ? ORDERED : UN_ORDERED;
	else if((type == HE_OL) ^ (ordering == ORDERED))
		ordering = SPLIT_ORDER;
}

BOOL HasActualBeside(HTML_Element *root, HTML_Element *helm, BOOL check_left)
{
	DEBUG_CHECKER_STATIC();
	OP_ASSERT(root && helm && helm != root && root->IsAncestorOf(helm) && root->IsIncludedActual());
	if(check_left)
	{
		HTML_Element *actual = helm->PrevActual();
		while(actual != root)
		{
			if(!actual->IsAncestorOf(helm))
				return TRUE;
			actual = actual->PrevActual();
		}
	}
	else
		return root->IsAncestorOf(helm->NextActual());
	return FALSE;
}

BOOL CharMightCollapse(uni_char ch)
{
	DEBUG_CHECKER_STATIC();
	return uni_collapsing_sp(ch) || ch == '\n' || ch == '\r' || ch == '\t';
}

// == OpDocumentEdit ===========================================

OpDocumentEdit::OpDocumentEdit()
	: m_doc(NULL)
	, m_caret(this)
	, m_selection(this)
	, m_undo_stack(this)
	, m_layout_modifier(this)
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	, m_spell_session(NULL)
	, m_has_spellchecked(FALSE)
	, m_word_iterator(this,TRUE,TRUE)
	, m_replace_word(this,FALSE,FALSE)
	, m_background_updater(this,FALSE,FALSE)
	, m_pending_spell_first(NULL)
	, m_pending_spell_last(NULL)
	, m_delay_misspell_word_info(NULL)
	, m_doc_has_changed(FALSE)
	, m_last_helm_spelled(NULL)
	, m_enable_spellcheck_later(FALSE)
	, m_blocking_spellcheck(FALSE)
	, m_by_user(FALSE)
#endif // INTERNAL_SPELLCHECK_SUPPORT
	, m_pending_styles_lock(0)
	, m_begin_count(0)
	, m_logtree_changed(FALSE)
	, m_usecss(FALSE)
	, m_readonly(FALSE)
	, m_wants_tab(FALSE)
	, m_body_is_root(TRUE)
	, m_autodetect_direction(FALSE)
	, m_blockquote_split(FALSE)
	, m_plain_text_mode(FALSE)
#ifdef WIDGETS_IME_SUPPORT
# ifndef DOCUMENTEDIT_DISABLE_IME_SUPPORT
	, m_imstring(NULL)
	, m_ime_string_elm(NULL)
	, im_waiting_first_compose(FALSE)
# endif
#endif
	, m_content_pending_helm(NULL)
	, m_listener(NULL)
#ifdef _DOCEDIT_DEBUG
	, m_random_seed_initialized(FALSE)
	, m_edit(this)
#endif // _DOCEDIT_DEBUG
	, m_recreate_caret(FALSE)
	, m_paragraph_element_type(HE_P)
	, m_is_focusing(FALSE)
{
	DEBUG_CHECKER_CONSTRUCTOR();
}

OP_STATUS OpDocumentEdit::Construct(OpDocumentEdit** obj, FramesDocument* doc, BOOL designmode)
{
	DEBUG_CHECKER_STATIC();
	*obj = OP_NEW(OpDocumentEdit, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->Init(doc, designmode)))
	{
		OP_DELETE(*obj);
		*obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpDocumentEdit::~OpDocumentEdit()
{
	DEBUG_CHECKER(TRUE);
	if (g_input_manager)
	{
		// OpInputContext::~OpInputContext does this too, but then it's to late for our virtual OnKeyboardInputLost to be called.
		g_input_manager->ReleaseInputContext(this);
	}

	// Clear unused temporary elements that might have been inserted along the way.
	m_caret.DeleteRemoveWhenMoveIfUntouched(NULL);

	ClearPendingStyles();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	g_main_message_handler->UnsetCallBacks(this);

#ifdef USE_OP_CLIPBOARD
	g_clipboard_manager->UnregisterListener(this);
#endif // USE_OP_CLIPBOARD
}

OP_STATUS OpDocumentEdit::Init(FramesDocument* doc, BOOL designmode)
{
	if (!doc || !doc->GetLogicalDocument())
		return OpStatus::ERR;

	DEBUG_CHECKER(TRUE);
	m_doc = doc;
	m_body_is_root = designmode;

	SetParentInputContext(m_doc->GetVisualDevice());

	m_undo_stack.Clear();

	if (!doc->IsReflowing())
	{
		ReflowAndUpdate();

		Begin();
		RETURN_IF_ERROR(CollapseWhitespace());
		End();

		if (designmode && m_doc->GetVisualDevice()->IsFocused())
			SetFocus(FOCUS_REASON_OTHER);
	}
	return designmode ? m_caret.Init(FALSE /* !doc->IsReflowing() */) : OpStatus::OK;
}

void OpDocumentEdit::InitEditableRoot(HTML_Element* root)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* body = GetBody();

	if (!root || !body)
		return;

	if (root->IsAncestorOf(body))
		root = body;

	if (!body->IsAncestorOf(root))
		return;

	m_body_is_root = FALSE;
}

void OpDocumentEdit::UninitEditableRoot(HTML_Element* root)
{
	DEBUG_CHECKER(TRUE);
	if (root->IsAncestorOf(m_caret.GetElement()))
	{
		ReleaseFocus();
		m_caret.Set(NULL, 0);
	}
}

void OpDocumentEdit::FocusEditableRoot(HTML_Element* helm, FOCUS_REASON reason)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* ec = GetFocusableEditableContainer(helm);
	if (ec && !ec->GetDisabled())
	{
		if (ec->Type() != HE_BODY)
		{
			// If we want to focus a editable container, place the caret in it, and if there is no editable
			// childelement, we must create it.
			HTML_Element* ee = FindEditableElement(ec, TRUE, FALSE, TRUE);
			if (!ee || !ec->IsAncestorOf(ee) || !m_caret.GetElement() || !ec->IsAncestorOf(m_caret.GetElement()))
				m_caret.Init(TRUE, ec);
		}
		m_caret.RestartBlink();
		SetFocus(reason);
		if (reason == FOCUS_REASON_MOUSE)
			g_input_manager->SetMouseInputContext(this);
	}
}

OP_STATUS OpDocumentEdit::Clear()
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* root_elm = GetBody();
	while(root_elm && root_elm->FirstChild())
		DeleteElement(root_elm->FirstChild());

	ReflowAndUpdate();

	m_undo_stack.Clear();

	return m_caret.Init(TRUE);
}

OP_STATUS OpDocumentEdit::CollapseWhitespace()
{
	DEBUG_CHECKER(TRUE);
	if (!GetBody())
		return OpStatus::OK;
	// Convert linebreaks in text into whitespace (as the layout does).
	// Otherwise they will be treated as <br> when when copied and pasted again and we'll get
	// a lot of new lines where we should not.
	// IE and Mozilla also does this when a document is set to designmode.

	if (m_doc->IsReflowing())
		return OpStatus::OK;

	ReflowAndUpdate();
	return OpStatus::OK;
}

void OpDocumentEdit::Paint(VisualDevice* vis_dev)
{
	DEBUG_CHECKER(TRUE);
	m_layout_modifier.Paint(vis_dev);
}

BOOL OpDocumentEdit::HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button)
{
	DEBUG_CHECKER(TRUE);
	BOOL handled = m_layout_modifier.HandleMouseEvent(helm, event, x, y, button);

	if (!handled && event == ONMOUSEDOWN)
	{
		FocusEditableRoot(helm, FOCUS_REASON_MOUSE);

#if defined(_X11_SELECTION_POLICY_)
		if (button == MOUSE_BUTTON_3 && GetEditableContainer(helm))
		{
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			if (g_clipboard_manager->HasText())
			{
				g_clipboard_manager->Paste(this, m_doc, m_caret.GetElement());
			}
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
			handled = TRUE; // Must set to TRUE, otherwise we get bug #217331
		}
#endif
	}

	return handled;
}

BOOL OpDocumentEdit::IsLayoutModifiable(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	return m_layout_modifier.IsLayoutModifiable(helm);
}

CursorType OpDocumentEdit::GetCursorType(HTML_Element* helm, int x, int y)
{
	DEBUG_CHECKER(TRUE);
	if (!m_body_is_root && !m_caret.IsElementEditable(helm))
		return CURSOR_AUTO;
	return m_layout_modifier.GetCursorType(helm, x, y);
}

#ifdef SUPPORT_TEXT_DIRECTION

BOOL OpDocumentEdit::SetRTL(bool is_rtl)
{
	if (GetRTL() == (BOOL)is_rtl)
		return FALSE;

	DEBUG_CHECKER(TRUE);

	HTML_Element* helm = GetEditableContainer(m_caret.GetElement());
	if (helm)
	{
		BeginChange(helm);
		OnElementChange(helm);
		helm->SetAttribute(m_doc, ATTR_DIR, NULL, NS_IDX_DEFAULT, is_rtl ? UNI_L("rtl") : UNI_L("ltr"), 3, NULL, FALSE);
		OnElementChanged(helm);
		EndChange(helm);

		if (m_listener)
			m_listener->OnTextDirectionChanged(is_rtl);

		return TRUE;
	}
	return FALSE;
}

#endif // SUPPORT_TEXT_DIRECTION

void OpDocumentEdit::AutodetectDirection()
{
#ifdef DOCUMENTEDIT_AUTODETECT_DIRECTION
	if (!m_autodetect_direction)
		return;
	HTML_Element *body = GetBody();
	if (body)
	{
		HTML_Element *text = FindElementAfterOfType(body, HE_TEXT, TRUE);
		BOOL detected = FALSE;
		if (text)
		{
			for (const uni_char* c = text->TextContent(); !detected && c && *c; c++)
			{
				switch (Unicode::GetBidiCategory(*c))
				{
					case BIDI_R:
					case BIDI_AL:
						if (SetRTL(TRUE))
							m_undo_stack.MergeLastChanges();
						detected = TRUE;
						break;
					case BIDI_L:
						if (SetRTL(FALSE))
							m_undo_stack.MergeLastChanges();
						detected = TRUE;
						break;
				}
			}
		}
	}
#endif // DOCUMENTEDIT_AUTODETECT_DIRECTION
}

void OpDocumentEdit::Copy(BOOL cut)
{
	DEBUG_CHECKER(TRUE);
#ifdef USE_OP_CLIPBOARD
	HTML_Element* target_elm = m_layout_modifier.IsActive() ? m_layout_modifier.m_helm : (m_selection.HasContent() ? m_selection.GetStartElement() : NULL);

	if (cut)
	{
		REPORT_AND_RETURN_IF_ERROR(g_clipboard_manager->Cut(this, GetDoc()->GetWindow()->GetUrlContextId(), m_doc, target_elm))
	}
	else
	{
		REPORT_AND_RETURN_IF_ERROR(g_clipboard_manager->Copy(this, GetDoc()->GetWindow()->GetUrlContextId(), m_doc, target_elm))
	}
#endif // USE_OP_CLIPBOARD
}

void OpDocumentEdit::Paste()
{
#ifdef USE_OP_CLIPBOARD
	HTML_Element* target = m_selection.HasContent() ? m_selection.GetStartElement() : m_caret.GetElement();
	g_clipboard_manager->Paste(this, m_doc, target);
#endif // USE_OP_CLIPBOARD
}

#ifdef USE_OP_CLIPBOARD
void OpDocumentEdit::OnCopy(OpClipboard* clipboard)
{
	DEBUG_CHECKER(TRUE);
#ifdef USE_OP_CLIPBOARD
# ifdef CLIPBOARD_HTML_SUPPORT
	if (m_layout_modifier.IsActive())
	{
#  ifdef _X11_SELECTION_POLICY_
		if (!g_clipboard_manager->GetMouseSelectionMode())
#  endif // _X11_SELECTION_POLICY_
		{
			OpString htmltext;
			REPORT_AND_RETURN_IF_ERROR(GetTextHTMLFromElement(htmltext, m_layout_modifier.m_helm, TRUE));
			if (htmltext.Length())
			{
				REPORT_AND_RETURN_IF_ERROR(clipboard->PlaceTextHTML(htmltext.CStr(), UNI_L(""), GetDoc()->GetWindow()->GetUrlContextId()))
			}
		}
		return;
	}
# endif

	if (!m_selection.HasContent())
		return;

	OpString text;
	REPORT_AND_RETURN_IF_ERROR(m_selection.GetText(text));
	if (!text.Length())
		return;

# ifdef CLIPBOARD_HTML_SUPPORT
#  ifdef _X11_SELECTION_POLICY_
	if (!g_clipboard_manager->GetMouseSelectionMode())
#  else // _X11_SELECTION_POLICY_
	if (TRUE)
#  endif // _X11_SELECTION_POLICY_
	{
		OpString htmltext;
		REPORT_AND_RETURN_IF_ERROR(m_selection.GetTextHTML(htmltext));
		if (htmltext.Length())
		{
			REPORT_AND_RETURN_IF_ERROR(clipboard->PlaceTextHTML(htmltext.CStr(), text.CStr(), GetDoc()->GetWindow()->GetUrlContextId()))
		}
	}
	else
		REPORT_AND_RETURN_IF_ERROR(clipboard->PlaceText(text.CStr(), GetDoc()->GetWindow()->GetUrlContextId()))
# else
	REPORT_AND_RETURN_IF_ERROR(clipboard->PlaceText(text.CStr(), GetDoc()->GetWindow()->GetUrlContextId()))
# endif
#endif // USE_OP_CLIPBOARD
}

void OpDocumentEdit::OnPaste(OpClipboard* clipboard)
{
	if(m_readonly)
		return;

	DEBUG_CHECKER(TRUE);
	OpString text;
	BOOL take_text = TRUE;
# ifdef CLIPBOARD_HTML_SUPPORT
	if (!m_plain_text_mode
#  ifdef _X11_SELECTION_POLICY_
		&& !clipboard->GetMouseSelectionMode()
#  endif // _X11_SELECTION_POLICY_
		&& clipboard->HasTextHTML())
	{
		REPORT_AND_RETURN_IF_ERROR((clipboard->GetTextHTML(text)));
		REPORT_AND_RETURN_IF_ERROR(InsertTextHTML(text.CStr(), text.Length(), NULL, NULL, NULL));
		take_text = FALSE;
	}
# endif

	if (take_text)
	{
		if (clipboard->HasText())
		{
			REPORT_AND_RETURN_IF_ERROR((clipboard->GetText(text)));
			REPORT_AND_RETURN_IF_ERROR(InsertText(text.CStr(), text.Length()));
		}
		else
			return;
	}

	ScrollIfNeeded();
	m_caret.UpdateWantedX();
}
#endif // USE_OP_CLIPBOARD

void OpDocumentEdit::Undo()
{
	DEBUG_CHECKER(TRUE);
	if (m_undo_stack.CanUndo())
	{
		Begin();

		if (m_selection.HasContent())
			m_selection.SelectNothing();
		m_undo_stack.Undo();

		End();
		ScrollIfNeeded();
		m_caret.UpdateWantedX();
	}
}

void OpDocumentEdit::Redo()
{
	DEBUG_CHECKER(TRUE);
	if (m_undo_stack.CanRedo())
	{
		Begin();

		if (m_selection.HasContent())
			m_selection.SelectNothing();
		m_undo_stack.Redo();

		End();
		ScrollIfNeeded();
		m_caret.UpdateWantedX();
	}
}

OP_STATUS OpDocumentEdit::SetText(const uni_char* text)
{
	DEBUG_CHECKER(TRUE);
	Begin();
	RETURN_IF_ERROR(Clear());
	OP_STATUS status = InsertText(text, uni_strlen(text));
	m_undo_stack.Clear();
	End();
	return status;
}

OP_STATUS OpDocumentEdit::SetTextHTML(const uni_char* text)
{
	DEBUG_CHECKER(TRUE);
	Begin();
	RETURN_IF_ERROR(Clear());
	OP_STATUS status = InsertTextHTML(text, uni_strlen(text), NULL, NULL, NULL, TIDY_LEVEL_AGGRESSIVE);
	m_undo_stack.Clear();
	End();
	return status;
}

OP_STATUS OpDocumentEdit::GetText(OpString& text, BOOL block_quotes_as_text)
{
	DEBUG_CHECKER(TRUE);

	if (!GetBody())
		return text.Set(UNI_L(""));

	TextSelection text_selection;
	text_selection.SetNewSelection(GetDoc(), GetBody(), FALSE, FALSE, TRUE);

	int len = text_selection.GetSelectionAsText(GetDoc(), NULL, 0, TRUE, block_quotes_as_text);

	if (text.Reserve(len + 1) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	text_selection.GetSelectionAsText(GetDoc(), text.CStr(), len + 1, TRUE, block_quotes_as_text);

	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::GetTextHTML(OpString& text)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* body = GetBody();

	if (body)
		return GetTextHTMLFromElement(text, body, FALSE);
	else
		return OpStatus::OK;
}

void OpDocumentEdit::OnDOMChangedSelection()
{
	DEBUG_CHECKER(TRUE);
	if (TextSelection* sel = m_doc->GetTextSelection())
	{
		SelectionBoundaryPoint focus_point = *sel->GetFocusPoint();
		HTML_Element* focus_elm = focus_point.GetElement();
		HTML_Element* edit_root = GetEditableContainer(focus_elm);
		if (focus_elm && edit_root && sel->IsEmpty())
		{
			// Focus point inside the editable block so we must make sure to make it suitable for a caret.
			HTML_Element* caret_elm = focus_elm;
			HTML_Element* elm_to_put_caret_after = NULL;
			int caret_offset = focus_point.GetOffset(); // offset here means a number of a child element.
			m_caret.StoreRealCaretPlacement(caret_elm, caret_offset);
			BOOL force_br = FALSE;

			if (!caret_elm->IsText())
			{
				caret_elm = focus_elm->FirstChildActualStyle();
				force_br = !caret_elm;
				while (caret_offset > 0)
				{
					elm_to_put_caret_after = caret_elm;
					caret_elm = caret_elm->SucActualStyle();
					--caret_offset;
				}

				force_br = force_br || (caret_elm && !caret_elm->FirstChildActualStyle());
			}

			if ((!caret_elm || !IsElementValidForCaret(caret_elm, TRUE, FALSE, TRUE)) && !IsStandaloneElement(focus_elm))
			{
				/* Looks like the selection point is an element a caret normally can not be placed in.
				 * However if it's not a standalone element, init the caret in it to allow placing
				 * the caret in the empty selection.
				 */
				caret_elm = m_caret.CreateTemporaryCaretHelm(focus_elm, elm_to_put_caret_after, !IsNoTextContainer(focus_elm) && !force_br);
				if (!caret_elm)
				{
					m_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					return;
				}
				caret_offset = 0;
			}
			else if (!caret_elm && elm_to_put_caret_after)
			{
				caret_elm = elm_to_put_caret_after;
				caret_offset = caret_elm->IsText() ? caret_elm->GetTextContentLength() : 0;
			}

			m_caret.Set(caret_elm, caret_offset);
			if (IsFocused())
			{
				if (caret_elm)
					m_doc->GetCaretPainter()->RestartBlink();
				else
					m_doc->GetCaretPainter()->StopBlink();
			}

			m_caret.CleanTemporaryCaretTextElement(FALSE);
		}
	}
}

/* static */
SelectionBoundaryPoint OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(const OldStyleTextSelectionPoint& point)
{
	HTML_Element* elm = point.GetElement();
	int offset = point.GetOffset();
	if (!elm)
		return SelectionBoundaryPoint();

	// We have a slight incompatibility between the caret representation here and the selection
	// implementation so we need to do some conversion. Will be removed when (if?) the caret
	// is moved into the normal selection.
	SelectionBoundaryPoint return_value = TextSelection::ConvertFromOldStyle(elm, offset);
	return_value.SetBindDirection(point.GetBindDirection());
	return return_value;
}

BOOL OpDocumentEdit::GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus)
{
	DEBUG_CHECKER(TRUE);
	if (m_recreate_caret && !m_caret.GetElement() && !m_caret.m_parent_candidate)
	{
		ReflowAndUpdate();
		CheckLogTreeChanged(FALSE);
		m_caret.Init(TRUE, NULL);
	}

	m_recreate_caret = FALSE;

	if (m_caret.GetElement())
	{
		OldStyleTextSelectionPoint caret_point;
		caret_point.SetLogicalPosition(m_caret.GetElement(), m_caret.GetOffset());
		SelectionBoundaryPoint dom_compatible_caret_point = OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(caret_point);
		if (m_selection.HasContent())
		{
			TextSelection* text_selection = m_doc->GetTextSelection();
			anchor = *text_selection->GetAnchorPoint();
			focus = *text_selection->GetFocusPoint();
		}
		else if (m_layout_modifier.IsActive())
		{
			anchor = OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(GetTextSelectionPoint(m_layout_modifier.m_helm, 0));
			focus = anchor;
		}
		else
		{
			anchor = dom_compatible_caret_point;
			focus = dom_compatible_caret_point;
		}
		return TRUE;
	}
	else if (m_caret.m_parent_candidate)
	{
		anchor = OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(GetTextSelectionPoint(m_caret.m_parent_candidate, 0));
		focus = anchor;
	}
	return FALSE;
}

void OpDocumentEdit::ClearPendingStyles()
{
	DEBUG_CHECKER(TRUE);
	if(m_pending_styles_lock)
		return;

	OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
	while (pending_style)
	{
		OP_DOCUMENT_EDIT_PENDING_STYLES* suc = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
		DeleteElement(pending_style->helm);
		pending_style->Out();
		OP_DELETE(pending_style);
		pending_style = suc;
	}
}

OP_STATUS OpDocumentEdit::InsertStyleElementLayoutModified(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *target = m_layout_modifier.m_helm;

	DUMPDEBUGTREE

	helm->PrecedeSafe(m_doc, target);
	target->OutSafe(m_doc, FALSE);
	target->UnderSafe(m_doc, helm);

	helm->MarkExtraDirty(m_doc);
	target->MarkExtraDirty(m_doc);

	DUMPDEBUGTREE

	ReflowAndUpdate();
	m_caret.UpdatePos();

	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::InsertStyleElementSingle(HTML_Element* helm, HTML_ElementType exclude_type, BOOL allow_nestling)
{
	DEBUG_CHECKER(TRUE);
	// Get text as html, remove it and add it again. This way, we will get the selected text under its own set of
	// styletags. The we can easily run through them and remove the tags of type type and move the children up one level.

	SelectionState state = GetSelectionState(FALSE, FALSE, TRUE, TRUE);
	if(!state.IsValid())
	{
		OP_ASSERT(FALSE); // This function should ONLY be called when editable content is selected!
		DeleteElement(helm);
		return OpStatus::ERR;
	}
	OP_STATUS status = OpStatus::OK;
	OpString text;
	RETURN_IF_ERROR(m_selection.GetTextHTML(text,FALSE));

	HTML_Element *shared_containing_elm = GetSharedContainingElement(state.editable_start_elm,state.editable_stop_elm);
	m_selection.SelectNothing();
	m_caret.LockUpdatePos(TRUE);
	m_selection.RemoveContent(FALSE, state.editable_start_elm, state.editable_stop_elm, state.editable_start_ofs, state.editable_stop_ofs);

	// Make sure caret is inside shared_containing_elm in order to not create problems in InserTextHTML
	while(!shared_containing_elm->IsAncestorOf(m_caret.GetElement()))
	{
		if(shared_containing_elm->Precedes(m_caret.GetElement()))
			m_caret.Move(FALSE,FALSE);
		else
			m_caret.Move(TRUE,FALSE);
	}

	DUMPDEBUGTREE

	HTML_Element *result_start = NULL, *result_stop = NULL;
	status = InsertTextHTML(text, text.Length(), &result_start, &result_stop, NULL, TIDY_LEVEL_AGGRESSIVE);

	if (!result_start)
	{
		// InsertTextHTML Did not produce anything. Use the caretelement as referenceelement.
		result_start = m_caret.GetElement();
		result_stop = m_caret.GetElement();
	}

	HTML_Element *start_sel_element = (HTML_Element*)result_start->FirstLeaf();
	DUMPDEBUGTREE

	m_caret.LockUpdatePos(FALSE,FALSE);
	m_caret.Place(m_caret.GetElement(),m_caret.GetOffset());
	RETURN_IF_ERROR(status);

	helm->PrecedeSafe(m_doc, result_start);
	helm->MarkExtraDirty(m_doc);

	// mark result_start->result_stop extra dirty???
	MoveSiblingBlock(SiblingBlock(result_start,result_stop),helm);

	HTML_Element* new_caret_elm = FindEditableElement(start_sel_element, TRUE, TRUE, FALSE);

	Tidy(result_start, result_stop, TRUE);

	ReflowAndUpdate();
	m_caret.UpdatePos();

	OldStyleTextSelectionPoint s1 = GetTextSelectionPoint(new_caret_elm, 0);
	OldStyleTextSelectionPoint s2 = GetTextSelectionPoint(m_caret.GetElement(), m_caret.GetOffset());
	m_selection.Select(&s1, &s2);

	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::InsertStyleElementMultiple(HTML_Element* helm, HTML_ElementType exclude_type, BOOL allow_nestling)
{
	DEBUG_CHECKER(TRUE);
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
	{
		OP_ASSERT(FALSE); // This function should ONLY be called when editable content is selected!
		DeleteElement(helm);
		return OpStatus::ERR;
	}

	HTML_Element* start_elm = selection_state.editable_start_elm;
	HTML_Element* stop_elm = selection_state.editable_stop_elm;
	int start_ofs = selection_state.editable_start_ofs;
	int stop_ofs = selection_state.editable_stop_ofs;
	BOOL single_element = (start_elm == stop_elm);

	// Split start and stop so we can insert the style on only the selected parts.
	SplitElement(stop_elm, stop_ofs);
	BOOL split1 = SplitElement(start_elm, start_ofs);
	if (split1)
		start_elm = start_elm->NextActual();
	if (single_element)
		stop_elm = start_elm;

	OP_STATUS status = OpStatus::OK;

	// Run through the selection and insert several instances of helm where it is needed.

	HTML_Element* tmp = start_elm;
	while(tmp)
	{
		// FIX: insert fewer elements by setting it outside friendly elements.
		if ((tmp->Type() == HE_TEXT || tmp->Type() == HE_BR || (tmp->Type() == HE_IMG && helm->Type() == HE_A)) && !(exclude_type && GetHasStyleInternal(tmp,exclude_type,ATTR_NULL)))
		{
			HTML_Element* new_style = NewCopyOfElement(helm,TRUE);
			if (!new_style)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}
			new_style->PrecedeSafe(m_doc, tmp);
			tmp->OutSafe(m_doc, FALSE);
			tmp->UnderSafe(m_doc, new_style);
			new_style->MarkExtraDirty(m_doc);
		}
		if (tmp == stop_elm)
			break;
		tmp = tmp->NextActual();
	}
	DeleteElement(helm);

	DUMPDEBUGTREE

	RestoreSelectionState(selection_state);

	return status;
}

OP_STATUS OpDocumentEdit::InsertStyleElement(HTML_Element* helm, HTML_ElementType exclude_type, BOOL allow_nestling, HTML_ElementType must_be_below_this_type)
{
	DEBUG_CHECKER(TRUE);
	SelectionState state = GetSelectionState(FALSE);
	if (state.HasEditableContent())
	{
		// Run through the selection and insert several instances of helm where it is needed.
		HTML_Element* shared_containing_elm = GetSharedContainingElement(state.editable_start_elm, state.editable_stop_elm);
		BeginChange(shared_containing_elm);

		HTML_Element* start_elm = state.editable_start_elm;
		HTML_Element* stop_elm = state.editable_stop_elm;
		BOOL need_multiple = (exclude_type && GetHasStyle(exclude_type)) ||
			(must_be_below_this_type && GetHasStyle(must_be_below_this_type)) ||
			!IsFriends(start_elm, stop_elm, TRUE, TRUE, TRUE);

		if (need_multiple || start_elm == stop_elm)
			InsertStyleElementMultiple(helm,exclude_type,allow_nestling);
		else
			InsertStyleElementSingle(helm,exclude_type,allow_nestling);

		EndChange(shared_containing_elm);
	}
	else if (m_layout_modifier.IsActive())
	{
		HTML_Element *parent = m_layout_modifier.m_helm->ParentActual();

		RETURN_IF_MEMORY_ERROR(BeginChange(parent));
		OP_STATUS status = InsertStyleElementLayoutModified(helm);
		RETURN_IF_MEMORY_ERROR(EndChange(parent));

		return status;
	}
	else
	{
		if(exclude_type && GetHasPendingStyle(exclude_type))
		{
			DeleteElement(helm);
			return OpStatus::OK;
		}

		// No selection. Insert helm at the caretposition with a empty textelement.
/*		HTML_Element *new_content = NewTextElement(document_edit_dummy_str, 1);
		if (!new_content)
			return OpStatus::ERR_NO_MEMORY;
		new_content->UnderSafe(helm);

		InsertElement(helm);

		m_caret.Place(new_content, 0);*/

		// No selection. Add the style to the pending list so it will be added when text is added (if the caret isn't
		// moved away from the current spot).
		// First check if it's already in the pending list and in that case remove it.
		OP_DOCUMENT_EDIT_PENDING_STYLES *pending_style;
		if(!allow_nestling)
		{
			pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
			while (pending_style)
			{
				if (IsMatchingStyle(pending_style->helm, helm->Type(), ATTR_NULL))
				{
					DeleteElement(pending_style->helm);
					pending_style->Out();
					OP_DELETE(pending_style);
					DeleteElement(helm);
					return OpStatus::OK;
				}
				pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
			}
		}

		pending_style = OP_NEW(OP_DOCUMENT_EDIT_PENDING_STYLES, ());
		if (!pending_style)
			return OpStatus::ERR_NO_MEMORY;
		pending_style->helm = helm;
		if(must_be_below_this_type)
			pending_style->Into(&m_pending_styles);
		else
			pending_style->IntoStart(&m_pending_styles);
	}
	return OpStatus::OK;
}

void OpDocumentEdit::InsertStyle(HTML_ElementType type, HTML_ElementType exclude_type, BOOL allow_nestling, HTML_ElementType must_be_below_this_type)
{
	DEBUG_CHECKER(TRUE);
	if(exclude_type && GetHasStyle(exclude_type,ATTR_NULL,TRUE))
		return; // Everything should be excluded!

	HTML_Element *new_elm = NewElement(type);
	if (!new_elm)
	{
		ReportOOM();
		return;
	}
	REPORT_AND_RETURN_IF_ERROR(InsertStyleElement(new_elm,exclude_type,allow_nestling,must_be_below_this_type));
}

void OpDocumentEdit::InsertFont(short attr, void* val)
{
	DEBUG_CHECKER(TRUE);
	OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

	if (GetHasStyle(HE_FONT, attr))
		RemoveStyle(HE_FONT, attr);

	HTML_Element *new_elm = NewElement(HE_FONT);
	if (!new_elm)
	{
		ReportOOM();
		return;
	}

	if (attr == ATTR_FACE)
	{
		void *value = NULL;
		ItemType item_type;
		BOOL need_free;
		BOOL is_event;
		HtmlAttrEntry hae[2];
		hae[0].attr = ATTR_FACE;
		hae[0].value = static_cast<uni_char*>(val);
		hae[0].value_len = uni_strlen(hae[0].value);
		hae[1].attr = ATTR_NULL;

		if (OpStatus::IsMemoryError(new_elm->ConstructAttrVal(m_doc->GetHLDocProfile(), &hae[0], FALSE, value, item_type, need_free, is_event, hae, NULL)))
		{
			ReportOOM();
			return;
		}

		new_elm->SetAttr(ATTR_FACE, item_type, value, need_free, NS_IDX_DEFAULT, FALSE, FALSE, FALSE, is_event);
	}
	else
		new_elm->SetAttr(attr, ITEM_TYPE_NUM, val);

	REPORT_AND_RETURN_IF_ERROR(InsertStyleElement(new_elm));
}

void OpDocumentEdit::InsertFontColor(UINT32 color)
{
	DEBUG_CHECKER(TRUE);
	InsertFont(ATTR_COLOR, reinterpret_cast<void*>(color));
}

void OpDocumentEdit::InsertFontFace(const uni_char* fontface)
{
	DEBUG_CHECKER(TRUE);
	InsertFont(ATTR_FACE, (void*)fontface);
}

void OpDocumentEdit::InsertFontSize(UINT32 size)
{
	DEBUG_CHECKER(TRUE);
	InsertFont(ATTR_SIZE, reinterpret_cast<void*>(size));
}

BOOL OpDocumentEdit::IsMatchingStyle(HTML_Element* helm, HTML_ElementType type, short attr)
{
	DEBUG_CHECKER(TRUE);
	if (type == HE_ANY && IsFriendlyElement(helm) /*&& HasAttribute(helm, attr)*/ && !IsStandaloneElement(helm))
		return TRUE;
	return (helm->Type() == type && HasAttribute(helm, attr));
}

BOOL OpDocumentEdit::IsAnyOfTypes(HTML_Element *helm, HTML_ElementType *types, short attr)
{
	DEBUG_CHECKER(TRUE);
	while(*types != HE_UNKNOWN)
	{
		if(IsMatchingStyle(helm,*types,attr))
			return TRUE;
		types++;
	}
	return FALSE;
}

HTML_Element *OpDocumentEdit::GetRootOfTypes(HTML_Element *helm, HTML_ElementType *types, short attr)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *root = NULL;
	while(helm)
	{
		if(IsAnyOfTypes(helm,types,attr))
			root = helm;
		helm = helm->ParentActual();
	}
	return root;
}

HTML_Element* OpDocumentEdit::ExtractTreeBeside(HTML_Element *root, HTML_Element *helm, BOOL extract_left)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *tmp = helm;
	OP_ASSERT(root->IsAncestorOf(helm));
	while(tmp != root)
	{
		if(extract_left ? tmp->Pred() : tmp->Suc())
			break;
		tmp = tmp->Parent();
	}
	if(tmp == root)
	{
		OP_ASSERT(FALSE);
		return NULL;
	}
	HTML_Element *new_tree = NULL;
	while(tmp != root)
	{
		HTML_Element *parent = tmp->Parent();
		HTML_Element *top = NewCopyOfElement(parent,TRUE);
		if(!top)
		{
			while(new_tree && new_tree->FirstLeaf())
				DeleteElement((HTML_Element*)new_tree->FirstLeaf());
			ReportOOM();
			return NULL;
		}
		if(extract_left)
		{
			while(parent->FirstChild() != tmp)
			{
				helm = parent->FirstChild();
				helm->OutSafe(m_doc, FALSE);
				helm->UnderSafe(m_doc,top);
			}
		}
		else
		{
			while(tmp->Suc())
			{
				helm = tmp->Suc();
				helm->OutSafe(m_doc, FALSE);
				helm->UnderSafe(m_doc,top);
			}
		}
		if(new_tree)
			new_tree->UnderSafe(m_doc,top);
		new_tree = top;
		tmp = parent;
	}
	return new_tree;
}

OP_STATUS OpDocumentEdit::ExtractElementsTo(HTML_Element *start_elm, HTML_Element *stop_elm, HTML_Element *old_root, HTML_Element *new_root, HTML_Element *after_me)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(new_root);
	if(!start_elm && !stop_elm)
	{
		if(!old_root)
		{
			OP_ASSERT(FALSE);
			return OpStatus::ERR;
		}
		SiblingBlock block(old_root->FirstChildActual(),old_root->LastChildActual());
		if(!block.IsEmpty())
			MoveSiblingBlock(block,new_root,after_me);
		return OpStatus::OK;
	}
	if(!start_elm || !stop_elm)
	{
		HTML_Element *dummy_elm;
		if(!start_elm)
			GetBlockStartStopInternal(&start_elm,&dummy_elm,stop_elm);
		else
			GetBlockStartStopInternal(&dummy_elm,&stop_elm,start_elm);
		OP_ASSERT(start_elm && stop_elm);
	}
	if(!old_root)
	{
		old_root = GetSharedContainingElement(start_elm,stop_elm);
		OP_ASSERT(old_root);
	}
	if(!old_root->IsAncestorOf(start_elm) || !old_root->IsAncestorOf(stop_elm))
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	while(start_elm->Parent() != old_root)
	{
		HTML_Element *parent = start_elm->Parent();
		if(parent->FirstChild() != start_elm)
		{
			HTML_Element *new_parent = NewCopyOfElement(parent,TRUE);
			if(!new_parent)
			{
				ReportOOM();
				return OpStatus::ERR_NO_MEMORY;
			}
			new_parent->PrecedeSafe(m_doc,parent);
			SiblingBlock block(parent->FirstChildActual(),start_elm->PredActual());
			MoveSiblingBlock(block,new_parent);
		}
		start_elm = parent;
	}
	while(stop_elm->Parent() != old_root)
	{
		HTML_Element *parent = stop_elm->Parent();
		if(parent->LastChild() != stop_elm)
		{
			HTML_Element *new_parent = NewCopyOfElement(parent,TRUE);
			if(!new_parent)
			{
				ReportOOM();
				return OpStatus::ERR_NO_MEMORY;
			}
			new_parent->FollowSafe(m_doc,parent);
			SiblingBlock block(stop_elm->SucActual(),parent->LastChildActual());
			MoveSiblingBlock(block,new_parent);
		}
		stop_elm = parent;
	}
	SiblingBlock block(start_elm,stop_elm);
	MoveSiblingBlock(block,new_root,after_me);
	return OpStatus::OK;
}

void OpDocumentEdit::RemoveStyleElementMultiple(HTML_ElementType type, short attr, BOOL just_one_level)
{
	DEBUG_CHECKER(TRUE);
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
		return;

	HTML_Element* start_elm = selection_state.editable_start_elm;
	HTML_Element* stop_elm = selection_state.editable_stop_elm;
	int start_ofs = selection_state.editable_start_ofs;
	int stop_ofs = selection_state.editable_stop_ofs;
	OP_ASSERT(start_elm != stop_elm);

	HTML_Element* shared_containing_elm = GetSharedContainingElement(start_elm, stop_elm);

	BeginChange(shared_containing_elm);

	// Possibly split start- and stop-elements in order to only remove style from selected text
	SplitElement(stop_elm, stop_ofs);
	BOOL split1 = SplitElement(start_elm, start_ofs);
	if(split1)
	{
		// If split1 is true, the element was split, and this is safe:
		// coverity[returned_null: FALSE]
		start_elm = start_elm->SucActual();
	}

	HTML_Element *tmp = start_elm;
	while(tmp->Parent() != shared_containing_elm)
		tmp = tmp->Parent();

	BOOL start_elm_deleted = FALSE; // It MIGHT happen that start_elm is a style element that is deleted
	while(tmp)
	{
		HTML_Element* tmp_next = (HTML_Element*) tmp->Next();
		BOOL is_start_elm_ancestor = !start_elm_deleted && tmp->IsAncestorOf(start_elm);
		if(IsMatchingStyle(tmp, type, attr) && tmp->IsIncludedActual() &&
			(start_elm_deleted || is_start_elm_ancestor || !tmp->Precedes(start_elm)))
		{
			// We might need to split the tree to the left of start_elm or to the right of
			// stop_elm in order not remove style from text before/after selection
			if(is_start_elm_ancestor && start_elm->FirstLeaf() != tmp->FirstLeaf() &&
				HasActualBeside(tmp,start_elm,TRUE))
			{
				HTML_Element *left_tree = ExtractTreeBeside(tmp,start_elm,TRUE);
				if(!left_tree)
					return;
				left_tree->PrecedeSafe(m_doc,tmp);
			}
			BOOL is_stop_elm_ancestor = tmp->IsAncestorOf(stop_elm);
			if(is_stop_elm_ancestor && stop_elm->LastLeaf() != tmp->LastLeaf() &&
				HasActualBeside(tmp,stop_elm,FALSE))
			{
				HTML_Element *right_tree = ExtractTreeBeside(tmp,stop_elm,FALSE);
				if(!right_tree)
					return;
				right_tree->FollowSafe(m_doc,tmp);
			}

			// Move children of the deleted style up one level...
			SiblingBlock children = SiblingBlock(tmp->FirstChildActual(),tmp->LastChildActual());
			MoveSiblingBlock(children,tmp->Parent(),tmp->Pred());

			DeleteElement(tmp);
			if(start_elm == tmp)
				start_elm_deleted = TRUE;
			if(just_one_level && is_stop_elm_ancestor)
				break;
			if(just_one_level)
			{
				if(!children.IsEmpty())
				{
					// Don't search the children of the deleted style when we're just removing one level
					tmp_next = (HTML_Element*)children.stop->NextSibling();
					if(!tmp_next)
					{
						OP_ASSERT(FALSE);
						break;
					}
				}
			}
			else if(!children.IsEmpty())
				tmp_next = children.start;
		}
		if(tmp == stop_elm)
			break;
		tmp = tmp_next;
	}

	shared_containing_elm->MarkExtraDirty(m_doc);

	RestoreSelectionState(selection_state);
	EndChange(shared_containing_elm);
}

void OpDocumentEdit::RemoveStyleElementSingle(HTML_ElementType type, short attr, BOOL just_one_level)
{
	DEBUG_CHECKER(TRUE);
	SelectionState state = GetSelectionState(FALSE, FALSE, TRUE, TRUE);
	if(!state.IsValid())
		return;

	HTML_Element* shared_containing_elm = GetSharedContainingElement(state.editable_start_elm, state.editable_stop_elm);
	OP_STATUS status = BeginChange(shared_containing_elm);
	REPORT_AND_RETURN_IF_ERROR(status);

	// Remove the content and insert it again. This is to have all selected text undependent of the
	// surrounding text so we then can remove the elements of type from that section.

	// Create a dummy element after the branch we are removing so the caret will jump to it if everything in this
	// branch is removed in RemoveSelection. (Must do this to get the text back at the correct place.)

	HTML_Element* dummy_elm = NewTextElement(document_edit_dummy_str, 1);
	HTML_Element* dummy_tmp = shared_containing_elm->LastChildActual();
	while(dummy_tmp && !dummy_tmp->IsAncestorOf(state.editable_stop_elm))
	{
		dummy_tmp = dummy_tmp->PredActual();
	}
	if(!dummy_tmp || !dummy_elm)
	{
		OP_ASSERT(FALSE);
		DeleteElement(dummy_elm);
		AbortChange();
		return;
	}
	dummy_elm->FollowSafe(m_doc, dummy_tmp);

	OpString text;
	status = m_selection.GetTextHTML(text,FALSE);
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			ReportOOM();
		AbortChange();
		return;
	}
	m_selection.SelectNothing();
	m_caret.LockUpdatePos(TRUE);
	m_selection.RemoveContent(FALSE, state.editable_start_elm, state.editable_stop_elm, state.editable_start_ofs, state.editable_stop_ofs);

	DUMPDEBUGTREE

	HTML_Element *result_start = NULL, *result_stop = NULL;
	status = InsertTextHTML(text, text.Length(), &result_start, &result_stop, NULL);
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			ReportOOM();
		AbortChange();
		return;
	}

	m_caret.LockUpdatePos(FALSE,FALSE);
	m_caret.Place(m_caret.GetElement(),m_caret.GetOffset());

	if (result_stop)
	{
		HTML_Element* last_leaf = result_stop->LastLeafActual();
		if (last_leaf)
			result_stop = last_leaf;
	}

	HTML_Element *start_sel_element = result_start ? result_start->FirstLeafActual() : NULL;
	DUMPDEBUGTREE

	HTML_Element* tmp = result_start;
	// Remove all elements of type type. If there is children, move them up to the parent.
	while(tmp)
	{
		HTML_Element* tmp_next = (HTML_Element*) tmp->NextActual();
		if (IsMatchingStyle(tmp, type, attr))
		{
			DUMPDEBUGTREE

			if (result_start == tmp)
				result_start = FindEditableElement(tmp, TRUE, FALSE, FALSE);
			if (result_stop == tmp)
			{
				//result_stop = FindElementBeforeOfType(tmp, HE_TEXT);
				OP_ASSERT(0); // Can this still happen?
			}

			SiblingBlock children = SiblingBlock(tmp->FirstChildActual(),tmp->LastChildActual());
			BOOL do_break = just_one_level && tmp->IsAncestorOf(result_stop);
			MoveSiblingBlock(children,tmp->ParentActual(),tmp->PredActual());
			DeleteElement(tmp);
			if(do_break)
				break;
			if(just_one_level && !children.IsEmpty())
				tmp_next = (HTML_Element*)children.stop->NextSiblingActual();

			DUMPDEBUGTREE
		}
		// Check if we've reached the end of the elements we should move, including some ugly
		// hack just in case we passed result_stop without noticing (FIXME)
		if (tmp == result_stop || (tmp_next && result_stop && !tmp_next->Precedes(result_stop)))
			break;
		tmp = tmp_next;
	}

	ReflowAndUpdate();

	HTML_Element* new_caret_elm = start_sel_element;
	int new_caret_ofs = 0;
	if (GetNearestCaretPos(start_sel_element, &new_caret_elm, &new_caret_ofs, TRUE, TRUE) ||
		GetNearestCaretPos(start_sel_element, &new_caret_elm, &new_caret_ofs, FALSE, TRUE))
	{
		OldStyleTextSelectionPoint s1 = GetTextSelectionPoint(new_caret_elm, new_caret_ofs);
		OldStyleTextSelectionPoint s2 = GetTextSelectionPoint(m_caret.GetElement(), m_caret.GetOffset());
		m_selection.Select(&s1, &s2);
	}
	m_caret.UpdatePos();

	status = EndChange(shared_containing_elm, TIDY_LEVEL_AGGRESSIVE);
}

void OpDocumentEdit::RemoveStyle(HTML_ElementType type, short attr, BOOL just_one_level)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(GetHasStyle(type, attr));

	SelectionState state = GetSelectionState(FALSE, FALSE, TRUE, TRUE);
	if (state.HasEditableContent())
	{
		BOOL need_multiple = !IsFriends(state.editable_start_elm, state.editable_stop_elm, TRUE, TRUE, TRUE);

		if (need_multiple)
			RemoveStyleElementMultiple(type, attr, just_one_level);
		else
			RemoveStyleElementSingle(type, attr, just_one_level);
	}
	else
	{

		m_selection.SelectNothing();
		OP_DOCUMENT_EDIT_PENDING_STYLES *pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
		while (pending_style)
		{
			OP_DOCUMENT_EDIT_PENDING_STYLES *suc_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
			if (IsMatchingStyle(pending_style->helm, type, ATTR_NULL))
			{
				DeleteElement(pending_style->helm);
				pending_style->Out();
				OP_DELETE(pending_style);
				if(just_one_level)
					return;
			}
			pending_style = suc_style;
		}

		if(!m_caret.GetElement() || !GetHasStyle(type, attr, FALSE, FALSE))
			return;

		HTML_Element* containing_elm = m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());

		BOOL in_dummy = FALSE;
		if (m_caret.GetElement()->Type() == HE_TEXT && m_caret.GetElement()->GetTextContentLength() == 0)
			// Simple way to destroy dummy (in tidy) if there is one.
			in_dummy = TRUE;

		// Create a temporary copy of the current branch up to
		// containing_elm, excluding the style we are removing.
		HTML_Element* tmp = NewTextElement(document_edit_dummy_str, 1);
		if (!tmp)
		{
			ReportOOM();
			return;
		}
		HTML_Element* parent = m_caret.GetElement()->ParentActual();

		// If just_one_level==TRUE -> after not copying one element with matching style,
		// copy the rest even though they are matching.
		BOOL omit_matching = TRUE;

		while (parent != containing_elm)
		{
			if (!omit_matching || !IsMatchingStyle(parent, type, attr))
			{
				HTML_Element* new_tmp = NewCopyOfElement(parent,TRUE);
				if (new_tmp)
					tmp->UnderSafe(m_doc, new_tmp);
				else
				{
					ReportOOM();
					break;
				}
				tmp = new_tmp;
			}
			else if(just_one_level)
				omit_matching = FALSE;
			parent = parent->ParentActual();
		}
		OpString text;
		GetTextHTMLFromElement(text, tmp, TRUE);
		DeleteElement(tmp);

		// Insert it with containing_elm as split_root
		InsertTextHTML(text, text.Length(), NULL, NULL, containing_elm, in_dummy ? TIDY_LEVEL_AGGRESSIVE : TIDY_LEVEL_NORMAL);
	}
}

BOOL OpDocumentEdit::GetHasStyleInternal(HTML_Element* helm, HTML_ElementType type, short attr)
{
	DEBUG_CHECKER(TRUE);
	return GetStyleElementInternal(helm,type,attr) != NULL;
}

HTML_Element *OpDocumentEdit::GetStyleElementInternal(HTML_Element* helm, HTML_ElementType type, short attr)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return NULL;
	HTML_Element* tmp = helm->Parent();
	while(tmp && tmp->Type() != HE_BODY)
	{
		if (IsMatchingStyle(tmp, type, attr))
			return tmp;
		tmp = tmp->ParentActual();
	}
	return NULL;
}

BOOL OpDocumentEdit::GetHasBlockTypesInternal(HTML_Element *helm, HTML_ElementType *types, short attr)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* tmp = helm ? helm->Parent() : NULL;
	while(tmp && tmp->Type() != HE_BODY)
	{
		if(IsAnyOfTypes(tmp,types,attr))
			return TRUE;
		tmp = tmp->ParentActual();
	}
	return FALSE;
}

HTML_Element* OpDocumentEdit::GetTopMostParentOfType(HTML_Element *helm, HTML_ElementType type)
{
	HTML_Element* candidate = NULL;
	while (helm)
	{
		if (helm->IsMatchingType(type, NS_HTML))
			candidate = helm;
		helm = helm->ParentActual();
	}
	return candidate;
}

BOOL OpDocumentEdit::GetHasPendingStyle(HTML_ElementType type, short attr)
{
	DEBUG_CHECKER(TRUE);
	OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
	while (pending_style)
	{
		if(IsMatchingStyle(pending_style->helm,type,attr))
			return TRUE;
		pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
	}
	return FALSE;
}

BOOL OpDocumentEdit::GetHasStyle(HTML_ElementType type, short attr, BOOL all_must_have_style, BOOL include_pending_styles)
{
	DEBUG_CHECKER(TRUE);
	SelectionState state = GetSelectionState(FALSE);
	if (state.HasEditableContent())
	{
		// Check if some elements inside the selection has type.
		HTML_Element* start_elm = state.editable_start_elm;
		HTML_Element* stop_elm = state.editable_stop_elm;
		if(start_elm != stop_elm)
		{
			HTML_Element* tmp = start_elm->NextActual();
			while(tmp != stop_elm)
			{
				if (IsMatchingStyle(tmp, type, attr))
				{
					if(!all_must_have_style)
						return TRUE;
					else // This might not be enough, because all TEXT/BR elements must be under the style
					{
						if(tmp->IsAncestorOf(stop_elm))
							break;
						tmp = (HTML_Element*)tmp->NextSiblingActual();
						continue;
					}
				}
				else if(all_must_have_style && (tmp->Type() == HE_TEXT || tmp->Type() == HE_BR))
				{
					HTML_Element *style_elm = GetStyleElementInternal(tmp,type,attr);
					if(!style_elm)
						return FALSE;
					if(style_elm->IsAncestorOf(stop_elm))
						break;
					tmp = (HTML_Element*)style_elm->NextSiblingActual();
					continue;
				}
				tmp = (HTML_Element*) tmp->NextActual();
			}
		}
		// Check if the start or stop is under a element of type type.
		// Don't care about the start if the selectionpoint is at the end of it.
		if (GetHasStyleInternal(start_elm, type, attr))
		{
			if(!all_must_have_style)
				return TRUE;
		}
		else if(all_must_have_style)
			return FALSE;
		return GetHasStyleInternal(stop_elm, type, attr);
	}
	else
	{
		return (include_pending_styles && GetHasPendingStyle(type,attr)) || GetHasStyleInternal(m_caret.GetElement(), type, attr);
	}
}

short OpDocumentEdit::GetFontSize()
{
	DEBUG_CHECKER(TRUE);
//	if (!m_selection.HasContent())
	{
		OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
		while (pending_style)
		{
			if (pending_style->helm->IsMatchingType(HE_FONT, NS_HTML) && pending_style->helm->HasAttr(ATTR_SIZE))
				return static_cast<short>(pending_style->helm->GetNumAttr(ATTR_SIZE));
			pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
		}

		HTML_Element* tmp = m_caret.GetElement();
		while(tmp)
		{
			if (tmp->GetFontSizeDefined())
				return (short) tmp->GetFontSize();
			tmp = tmp->Parent();
		}
	}

	return 3; // The default
}

const uni_char* OpDocumentEdit::GetFontFace()
{
	DEBUG_CHECKER(TRUE);
//	if (!m_selection.HasContent())
	{
		OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
		while (pending_style)
		{
			if (pending_style->helm->IsMatchingType(HE_FONT, NS_HTML) && pending_style->helm->HasAttr(ATTR_FACE))
				return pending_style->helm->GetAttrValue(ATTR_FACE, NS_IDX_HTML);
			pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
		}

		HTML_Element* tmp = m_caret.GetElement();
		while(tmp)
		{
			if (tmp->Type() == HE_FONT && HasAttribute(tmp, ATTR_FACE))
			{
				return tmp->GetAttrValue(ATTR_FACE, NS_IDX_HTML);
			}
			tmp = tmp->Parent();
		}
	}
	const uni_char *const result = UNI_L(""); // ADS 1.2 can't cope with returning it directly !
	return result;
}

static OP_STATUS FormatDeclaration(TempBuffer *tmp_buffer, CSS_decl *decl)
{
	TRAPD(status, CSS::FormatDeclarationL(tmp_buffer, decl, FALSE, CSS_FORMAT_COMPUTED_STYLE));
	return status;
}

OP_STATUS OpDocumentEdit::GetFontFace(TempBuffer &output)
{
	OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
	while (pending_style)
	{
		if (pending_style->helm->IsMatchingType(HE_FONT, NS_HTML) && pending_style->helm->HasAttr(ATTR_FACE))
			return output.Append(pending_style->helm->GetAttrValue(ATTR_FACE, NS_IDX_HTML));
		pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) pending_style->Suc();
	}

	HTML_Element* tmp = m_caret.GetElement();
	while(tmp)
	{
		// Handle font att
		if (tmp->Type() == HE_FONT && HasAttribute(tmp, ATTR_FACE))
			return output.Append(tmp->GetAttrValue(ATTR_FACE, NS_IDX_HTML));

		// Handle inline style
		CSS_property_list* prop_list = tmp->GetCSS_Style();
		CSS_decl *decl = prop_list ? prop_list->GetFirstDecl() : NULL;
		while (decl)
		{
			if (decl->GetProperty() == CSS_PROPERTY_font_family)
			{
				OP_STATUS status = FormatDeclaration(&output, decl);
				if (OpStatus::IsSuccess(status))
					return OpStatus::OK;
			}
			decl = decl->Suc();
		}
		tmp = tmp->ParentActual();
	}
	return OpStatus::OK;
}

void OpDocumentEdit::GetBlockStartStopInternal(HTML_Element** start, HTML_Element** stop, HTML_Element* from)
{
	DEBUG_CHECKER(TRUE);
	if(!from)
	{
		OP_ASSERT(FALSE);
		start = stop = NULL;
		return;
	}
	HTML_Element* tmp = from;
	while(tmp->PrevActual() && IsFriends(tmp->PrevActual(), from, TRUE, TRUE))
		tmp = tmp->PrevActual();
	*start = tmp;

	tmp = from;
	while(tmp->NextActual() && IsFriends(from, tmp->NextActual(), TRUE, TRUE))
		tmp = tmp->NextActual();

	// Include the br at the end of the block.
	if (tmp->NextActual() && IsEndingBr(tmp->NextActual()))
		tmp = tmp->NextActual();
	*stop = tmp;
}

void OpDocumentEdit::GetBlockStartStop(HTML_Element** start, HTML_Element** stop)
{
	DEBUG_CHECKER(TRUE);
	ReflowAndUpdate();
	SelectionState state = GetSelectionState(FALSE);
	if (state.HasEditableContent())
	{
		OP_ASSERT(state.editable_start_elm == state.editable_stop_elm || state.editable_start_elm->Precedes(state.editable_stop_elm));
		HTML_Element *start1, *stop1, *start2, *stop2;
		GetBlockStartStopInternal(&start1, &stop1, state.editable_start_elm);
		GetBlockStartStopInternal(&start2, &stop2, state.editable_stop_elm);
		*start = start1;
		*stop = stop2;
	}
	else
	{
		GetBlockStartStopInternal(start, stop, m_caret.GetElement());
	}
}

BOOL OpDocumentEdit::HasBlockElementChildren(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *child = helm->FirstChildActual();
	while(child)
	{
		if(IsBlockElement(child))
			return TRUE;
		child = child->SucActual();
	}
	return FALSE;
}

BOOL OpDocumentEdit::GetEditableSubrange(HTML_Element *&start_elm, HTML_Element *&stop_elm, int &start_ofs, int &stop_ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!start_elm || !stop_elm)
		return FALSE;
	HTML_Element *new_start_elm = start_elm, *new_stop_elm = stop_elm;
	int new_start_ofs = start_ofs, new_stop_ofs = stop_ofs;

	if(new_start_elm->Type() == HE_BR)
		new_start_ofs = 0;
	if(new_stop_elm->Type() == HE_BR)
		new_stop_ofs = 0;
	int valid_ofs = 0;
	BOOL has_valid_ofs = GetLastValidCaretOfs(new_start_elm,valid_ofs);
	if(!has_valid_ofs || (start_ofs >= valid_ofs && new_start_elm->Type() != HE_BR))
	{
		if(new_start_elm == new_stop_elm)
			return FALSE;
		do
		{
			new_start_elm = new_start_elm->Next();
		} while(new_start_elm && !IsElementValidForCaret(new_start_elm) && new_start_elm != new_stop_elm);

		if(!new_start_elm || !IsElementValidForCaret(new_start_elm))
			return FALSE;
		new_start_ofs = 0;
	}
	BOOL is_before_stop = TRUE;
	if(IsElementValidForCaret(new_stop_elm))
	{
		GetValidCaretPosFrom(new_stop_elm,0,new_stop_elm,valid_ofs);
		is_before_stop = new_stop_ofs <= valid_ofs && new_stop_elm->Type() != HE_BR;
	}
	if(is_before_stop)
	{
		if(new_start_elm == new_stop_elm)
			return FALSE;
		do
		{
			new_stop_elm = new_stop_elm->Prev();
		} while(new_stop_elm && !IsElementValidForCaret(new_stop_elm) && new_start_elm != new_stop_elm);

		if(!new_stop_elm) // otherwise new_stop_elm is valid for caret (because new_start_elm is valid)
			return FALSE;
		if(new_stop_elm->Type() == HE_TEXT)
			new_stop_ofs = new_stop_elm->GetTextContentLength();
		else
			GetLastValidCaretOfs(new_stop_elm,new_stop_ofs);
	}
	start_elm = new_start_elm;
	stop_elm = new_stop_elm;
	start_ofs = new_start_ofs;
	stop_ofs = new_stop_ofs;
	return TRUE;
}

BOOL OpDocumentEdit::ActualizeSelectionState(SelectionState &state)
{
	HTML_Element *tmp;
	BOOL adjusted = FALSE;
	if(!state.HasContent())
	{
		OP_ASSERT(FALSE); // This function should only be used when state has content!
		return FALSE;
	}
	if(!state.start_elm->IsIncludedActual())
	{
		adjusted = TRUE;
		if(!state.start_ofs && state.start_elm->PrevActual() &&
			state.start_elm->PrevActual()->IsAncestorOf(state.start_elm))
		{
			state.start_elm = state.start_elm->PrevActual();
		}
		else
		{
			if(state.start_ofs)
			{
				if(state.start_elm->IsAncestorOf(state.stop_elm))
					return FALSE;
				state.start_elm = (HTML_Element*)state.start_elm->NextSibling();
			}
			else
			{
				state.start_elm = state.start_elm->Next();
			}
			while(state.start_elm)
			{
				if(state.start_elm->GetInserted() <= HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
					break;
				if(state.start_elm == state.stop_elm)
					return FALSE;
				state.start_elm = state.start_elm->Next();
			}
			if(!state.start_elm)
				return FALSE;
			state.start_ofs = 0;
		}
	}

	if(!state.stop_elm->IsIncludedActual())
	{
		adjusted = TRUE;
		if(state.stop_ofs)
		{
			tmp = state.stop_elm->LastLeaf();
			if(!tmp)
				tmp = state.stop_elm;
			if(state.stop_elm->ParentActual() && !state.stop_elm->ParentActual()->IsAncestorOf(tmp->NextActual()))
			{
				state.stop_elm = state.stop_elm->ParentActual();
				goto stop_found;
			}
			for(tmp = state.stop_elm->LastLeaf(); tmp && tmp != state.stop_elm ; tmp = tmp->Prev())
			{
				if(tmp->IsIncludedActual())
				{
					state.stop_elm = tmp;
					goto stop_found;
				}
			}
		}
		for(tmp = state.stop_elm; tmp; tmp = tmp->Prev())
		{
			if(!tmp->IsAncestorOf(state.stop_elm) && tmp->GetInserted() <= HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
			{
				state.stop_elm = tmp;
				goto stop_found;
			}
		}

		stop_found:

		state.stop_ofs = state.stop_elm->Type() == HE_TEXT ? state.stop_elm->GetTextContentLength() : 1;
	}

	if(adjusted)
	{
		OP_ASSERT(!(state.stop_elm->Precedes(state.start_elm) || (state.start_elm == state.stop_elm && state.start_ofs > state.stop_ofs)));
		OP_ASSERT(state.start_elm->IsIncludedActual() && state.stop_elm->IsIncludedActual());
		return state.HasContent();
	}
	return TRUE;
}

SelectionState OpDocumentEdit::GetSelectionState(BOOL de_select, BOOL de_caret, BOOL require_editable, BOOL actual_only)
{
	DEBUG_CHECKER(TRUE);

	// Assume that we never want to reset the caret and return no selection state.
	// I have no idea why this function was once changed to do the opposite (unintentional?).
	// We still let GetValidCaretPosFrom change it if it wants though.

	SelectionState state;
	state.caret_elm = m_caret.GetElement();
	state.caret_ofs = m_caret.GetOffset();
	if(m_selection.HasContent())
	{
		state.start_elm = m_selection.GetStartElement(actual_only);
		state.stop_elm = m_selection.GetStopElement(actual_only);
		state.start_ofs = m_selection.GetStartOfs();
		state.stop_ofs = m_selection.GetStopOfs();

		TextSelection::ConvertPointToOldStyle(SelectionBoundaryPoint(state.start_elm, state.start_ofs), state.start_elm, state.start_ofs);
		TextSelection::ConvertPointToOldStyle(SelectionBoundaryPoint(state.stop_elm, state.stop_ofs), state.stop_elm, state.stop_ofs);

		if(!m_caret.IsElementEditable(state.start_elm) || !m_caret.IsElementEditable(state.stop_elm))
			return SelectionState();
		//if(ActualizeSelectionState(state))
		{
			state.editable_start_elm = state.start_elm;
			state.editable_stop_elm = state.stop_elm;
			state.editable_start_ofs = state.start_ofs;
			state.editable_stop_ofs = state.stop_ofs;
			if(!GetEditableSubrange(state.editable_start_elm, state.editable_stop_elm, state.editable_start_ofs, state.editable_stop_ofs))
			{
				state.editable_start_elm = state.editable_stop_elm = NULL;
			}
			if(state.editable_start_elm || !require_editable)
			{
				if(de_select)
				{
					m_selection.SelectNothing();
					state.removed_selection = TRUE;
				}
				if(de_caret && state.caret_elm)
				{
					m_caret.Set(NULL,0);
					state.removed_caret = TRUE;
				}
				return state;
			}
		}
	}
	if(!state.caret_elm)
		return SelectionState();

	state.start_elm = state.stop_elm = state.caret_elm;
	state.start_ofs = state.stop_ofs = state.caret_ofs;

	state.editable_start_elm = state.editable_stop_elm = state.caret_elm;
	state.editable_start_ofs = state.editable_stop_ofs = state.caret_ofs;

	state.caret_at_end = m_selection.IsEndPointFocusPoint();

	if(de_caret)
	{
		m_caret.Set(NULL,0);
		state.removed_caret = TRUE;
	}
	return state;
}

void OpDocumentEdit::RestoreSelectionState(SelectionState state)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(state.start_elm->IsIncludedActual());
	OP_ASSERT(state.stop_elm->IsIncludedActual());
	ReflowAndUpdate();
	if(state.removed_selection)
	{
		AdjustTextOfs(state.start_elm,state.start_ofs);
		AdjustTextOfs(state.stop_elm,state.stop_ofs);
		OldStyleTextSelectionPoint s1 = GetTextSelectionPoint(state.start_elm, state.start_ofs);
		OldStyleTextSelectionPoint s2 = GetTextSelectionPoint(state.stop_elm, state.stop_ofs);
		m_selection.Select(&s1, &s2, state.caret_at_end);
	}
	else if(state.caret_elm)
	{
		AdjustTextOfs(state.caret_elm,state.caret_ofs);

		HTML_Element *old_caret_elm = m_caret.GetElement();
		int old_caret_offset = m_caret.GetOffset();

		OldStyleTextSelectionPoint s1 = GetTextSelectionPoint(state.caret_elm, state.caret_ofs);
		OldStyleTextSelectionPoint s2 = s1;
		m_selection.Select(&s1, &s2, state.caret_at_end);

		// The caret could be set outside the editable element.
		if (old_caret_elm && !m_caret.GetElement())
		{
			SelectionBoundaryPoint s1(old_caret_elm, old_caret_offset);
			SelectionBoundaryPoint s2 = s1;
			m_doc->SetSelection(&s1, &s2, state.caret_at_end);
		}

	}
}

BOOL OpDocumentEdit::AdjustTextOfs(HTML_Element *&helm, int &ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || helm->Type() != HE_TEXT || helm->GetTextContentLength() > ofs)
		return FALSE;
	HTML_Element *next_helm = NULL;
	if(helm->GetTextContentLength() == ofs)
	{
		next_helm = FindEditableElement(helm,TRUE,FALSE,FALSE);
		if(next_helm && next_helm->Type() == HE_TEXT && IsFriends(helm,next_helm))
		{
			helm = next_helm;
			ofs = 0;
			return TRUE;
		}
		return FALSE;
	}
	int count = helm->GetTextContentLength();
	next_helm = helm;
	HTML_Element *last_helm = helm;
	int last_ofs = helm->GetTextContentLength();
	for(;;)
	{
		next_helm = FindEditableElement(next_helm,TRUE,FALSE,TRUE);
		if(!next_helm)
			break;
		if(next_helm->Type() == HE_TEXT)
		{
			int len = next_helm->GetTextContentLength();
			if(count+len >= ofs)
			{
				helm = next_helm;
				ofs = ofs - count;
				return TRUE;
			}
			count += len;
			last_helm = next_helm;
			last_ofs = len;
		}
	}
	if(last_helm != helm) // ok... the desired offset cant be found in the document, set helm+ofs to end of last text-element
	{
		helm = last_helm;
		ofs = last_ofs;
		return TRUE;
	}
	return FALSE;
}

void OpDocumentEdit::RemoveBlockTypes(HTML_ElementType *types, short attr, BOOL just_one_level, HTML_ElementType exclude_at_bottom_type)
{
	DEBUG_CHECKER(TRUE);
	if(!GetHasBlockTypes(types,attr))
		return;
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
		return;
	HTML_Element *start_elm = selection_state.editable_start_elm;
	HTML_Element *stop_elm = selection_state.editable_stop_elm;

	HTML_Element *dummy_elm;
	GetBlockStartStopInternal(&start_elm,&dummy_elm,start_elm);
	GetBlockStartStopInternal(&dummy_elm,&stop_elm,stop_elm);

	HTML_Element *tmp = GetSharedContainingElement(start_elm,stop_elm);
	if(!tmp)
	{
		OP_ASSERT(FALSE);
		return;
	}
	HTML_Element *shared_containing_elm = tmp;

	/* Make sure shared_containing_elm is outside of any elements that should be removed,
	 * but don't search past the contentEditable element (if there is one). */
	while(tmp && tmp->Type() != HE_BODY && !tmp->IsContentEditable(FALSE /*inherit*/))
	{
		if(IsAnyOfTypes(tmp,types,attr))
			shared_containing_elm = tmp->ParentActual();
		tmp = tmp->ParentActual();
	}
	OP_STATUS status = BeginChange(shared_containing_elm);
	REPORT_AND_RETURN_IF_ERROR(status);

	tmp = start_elm;
	while(tmp->Parent() != shared_containing_elm)
		tmp = tmp->Parent();

	BOOL start_elm_deleted = FALSE; // It MIGHT happen that start_elm is a block element that is deleted
	while(tmp)
	{
		HTML_Element* tmp_next = (HTML_Element*) tmp->Next();
		BOOL is_start_elm_ancestor = !start_elm_deleted && tmp->IsAncestorOf(start_elm);
		if(IsAnyOfTypes(tmp, types, attr) && !tmp->IsContentEditable(FALSE /*inherit*/) && tmp->IsIncludedActual() &&
			(start_elm_deleted || is_start_elm_ancestor || !tmp->Precedes(start_elm)) &&
			!(tmp->Type() == exclude_at_bottom_type && !HasBlockElementChildren(tmp)))
		{
			if(is_start_elm_ancestor && start_elm->FirstLeaf() != tmp->FirstLeaf() &&
				HasActualBeside(tmp,start_elm,TRUE))
			{
				HTML_Element *left_tree = ExtractTreeBeside(tmp,start_elm,TRUE);
				if(!left_tree)
				{
					AbortChange();
					return;
				}
				left_tree->PrecedeSafe(m_doc,tmp);
			}
			BOOL is_stop_elm_ancestor = tmp->IsAncestorOf(stop_elm);
			if(is_stop_elm_ancestor && stop_elm->LastLeaf() != tmp->LastLeaf() &&
				HasActualBeside(tmp,stop_elm,FALSE))
			{
				HTML_Element *right_tree = ExtractTreeBeside(tmp,stop_elm,FALSE);
				if(!right_tree)
				{
					AbortChange();
					return;
				}
				right_tree->FollowSafe(m_doc,tmp);
			}

			SiblingBlock children = SiblingBlock(tmp->FirstChildActual(),tmp->LastChildActual());
			MoveSiblingBlock(children,tmp->Parent(),tmp->Pred());
			DeleteElement(tmp);
			if(tmp == start_elm)
				start_elm_deleted = TRUE;
			if(just_one_level && is_stop_elm_ancestor)
				break;
			if(just_one_level)
			{
				if(!children.IsEmpty())
				{
					tmp_next = (HTML_Element*)children.stop->NextSibling();
					if(!tmp_next)
					{
						OP_ASSERT(FALSE);
						break;
					}
				}
			}
			else if(!children.IsEmpty())
				tmp_next = children.start;
		}
		if(tmp == stop_elm)
			break;
		tmp = tmp_next;
	}

	shared_containing_elm->MarkExtraDirty(m_doc);
	RestoreSelectionState(selection_state);
	EndChange(shared_containing_elm);
}

OP_STATUS OpDocumentEdit::InsertBlockElementMultiple(HTML_Element *helm, BOOL exclude_lists, BOOL only_over_cursor, BOOL dont_insert_where_already_exists, HTML_ElementType *will_be_removed_types)
{
	DEBUG_CHECKER(TRUE);
	OP_STATUS status = OpStatus::OK;
	SelectionState selection_state = GetSelectionState();
	if(!selection_state.IsValid())
	{
		if(!only_over_cursor)
			DeleteElement(helm);
		return OpStatus::ERR;
	}

	HTML_Element *start_elm = NULL, *stop_elm = NULL;
	if(only_over_cursor)
	{
		HTML_Element* real_elm = m_caret.m_real_caret_elm.GetElm();
		if (m_caret.IsUnmodifiedTemporaryCaretTextElement() && real_elm)
		{
			m_caret.CleanTemporaryCaretTextElement(TRUE);
			SelectionBoundaryPoint sel_point(real_elm, m_caret.m_real_caret_elm_off);
			m_caret.Place(sel_point);
		}

		start_elm = m_caret.GetElement();
		stop_elm = start_elm;
		if(!start_elm)
			return OpStatus::ERR;
	}
	else
	{
		start_elm = selection_state.editable_start_elm;
		stop_elm = selection_state.editable_stop_elm;
	}

	HTML_Element *dummy_elm;
	GetBlockStartStopInternal(&start_elm,&dummy_elm,start_elm);
	GetBlockStartStopInternal(&dummy_elm,&stop_elm,stop_elm);

	HTML_Element *shared_containing_elm = GetSharedContainingElement(start_elm,stop_elm);
	BeginChange(shared_containing_elm);

	// Run through the selection and insert several instances of helm where it is needed.

	HTML_Element* tmp = start_elm;
	while(tmp)
	{
		HTML_Element *tmp_next = tmp->NextActual();
		if(only_over_cursor || ((tmp->Type() == HE_TEXT || tmp->Type() == HE_BR) && !(exclude_lists && GetParentListElm(tmp))))
		{
			HTML_Element *left_elm, *right_elm;
			GetBlockStartStopInternal(&left_elm,&right_elm,tmp);
			left_elm = (HTML_Element*)left_elm->FirstLeaf(); // should not be necessary...
			right_elm = (HTML_Element*)right_elm->LastLeaf(); // should not be necessary...
			HTML_Element *left_under_block = left_elm;
			HTML_Element *right_under_block = right_elm;
			while(!IsBlockElement(left_under_block->Parent()))
				left_under_block = left_under_block->Parent();
			while(!IsBlockElement(right_under_block->Parent()))
				right_under_block = right_under_block->Parent();
			if(left_under_block->Parent() != right_under_block->Parent())
			{
				OP_ASSERT(FALSE);
				status = OpStatus::ERR;
				break;
			}

			if(dont_insert_where_already_exists)
			{
				HTML_Element *current_block_parent = left_under_block->Parent();
				BOOL dont_insert = FALSE;
				while(current_block_parent)
				{
					BOOL cover_start_stop = current_block_parent->FirstLeaf() == left_elm && current_block_parent->LastLeaf() == right_elm;
					if(current_block_parent->Type() == helm->Type())
					{
						dont_insert = cover_start_stop;
						break;
					}
					if(!will_be_removed_types || !IsAnyOfTypes(current_block_parent, will_be_removed_types))
						break;
					if(!cover_start_stop)
						break;
					current_block_parent = current_block_parent->Parent();
				}
				if(dont_insert)
				{
					if(current_block_parent->IsAncestorOf(stop_elm))
						break;
					tmp = (HTML_Element*)current_block_parent->NextSibling();
					continue;
				}
			}
			// Due to the possible presence of BR element, we might need split the tree in order
			// to insert blocks over BR terminated lines.
			if(left_under_block->FirstLeaf() != left_elm)
			{
				HTML_Element *left_tree = ExtractTreeBeside(left_under_block,left_elm,TRUE);
				if(!left_tree)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				left_tree->PrecedeSafe(m_doc,left_under_block);
			}
			if(right_under_block->LastLeaf() != right_elm)
			{
				HTML_Element *right_tree = ExtractTreeBeside(right_under_block,right_elm,FALSE);
				if(!right_tree)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				right_tree->FollowSafe(m_doc,right_under_block);
			}
			SiblingBlock block_childs = SiblingBlock(left_under_block,right_under_block);

			// Don't insert if all children are text-elements with just "\n\r" etc,
			// then the possible tree-split above was unnecessary, but no harm done.
			if(IsValidListBlock(block_childs))
			{
				HTML_Element* new_block = only_over_cursor ? helm : NewCopyOfElement(helm,TRUE);
				if (!new_block)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				new_block->PrecedeSafe(m_doc, block_childs.start);
				// Don't access block_childs.stop after MoveSiblingBlock because it may be already gone if it's not "actual".
				tmp_next = block_childs.stop->NextSiblingActual();
				MoveSiblingBlock(block_childs,new_block);
				new_block->MarkExtraDirty(m_doc);
				if(only_over_cursor || new_block->IsAncestorOf(stop_elm))
					break;
			}
		}
		if (tmp == stop_elm)
			break;
		tmp = tmp_next;
	}

	if(!only_over_cursor)
		DeleteElement(helm);
	DUMPDEBUGTREE

	RestoreSelectionState(selection_state);
	if(OpStatus::IsSuccess(status))
		EndChange(shared_containing_elm);
	else
		AbortChange();

	return status;
}

BOOL OpDocumentEdit::GetHasBlockTypes(HTML_ElementType *types, short attr)
{
	DEBUG_CHECKER(TRUE);
	SelectionState state = GetSelectionState(FALSE);
	if (state.HasEditableContent())
	{
		// Check if some elements inside the selection has any of types.
		HTML_Element* start_elm = state.editable_start_elm;
		HTML_Element* stop_elm = state.editable_stop_elm;
		HTML_Element* tmp = start_elm;
		while(tmp != stop_elm)
		{
			if(IsAnyOfTypes(tmp,types,attr))
				return TRUE;
			tmp = (HTML_Element*) tmp->NextActual();
		}
		// Check if the start or stop is under a element of any of types
		if (GetHasBlockTypesInternal(start_elm, types, attr) || GetHasBlockTypesInternal(stop_elm, types, attr))
			return TRUE;
	}
	else
	{
		return GetHasBlockTypesInternal(m_caret.GetElement(), types, attr);
	}
	return FALSE;
}

BOOL OpDocumentEdit::GetHasBlockType(HTML_ElementType type, short attr)
{
	DEBUG_CHECKER(TRUE);
	HTML_ElementType types[] = {type,HE_UNKNOWN};
	return GetHasBlockTypes(types,attr);
}

BOOL OpDocumentEdit::GetSelectionMatchesFunction(BOOL (*func)(HTML_Element*,void*), void *func_arg,
	BOOL (*must_match)(HTML_Element*,void*), void *must_match_arg,
	BOOL all_must_match, BOOL all_ancestors_must_match)
{
	DEBUG_CHECKER(TRUE);
	BOOL found = FALSE;
	if(all_ancestors_must_match && (!all_must_match || must_match))
	{
		OP_ASSERT(FALSE);
		all_must_match = TRUE;
		must_match = NULL;
	}
	SelectionState state = GetSelectionState(FALSE);
	if (state.HasEditableContent())
	{
		HTML_Element* start_elm = state.editable_start_elm;
		HTML_Element* stop_elm = state.editable_stop_elm;
		if(start_elm != stop_elm)
		{
			HTML_Element* tmp = start_elm->NextActual();
			while(tmp != stop_elm)
			{
				if (func(tmp,func_arg))
				{
					if(!all_must_match)
						return TRUE;
					found = TRUE;
					if(!all_ancestors_must_match)
					{
						if(tmp->IsAncestorOf(stop_elm))
							break;
						tmp = (HTML_Element*)tmp->NextSiblingActual();
						continue;
					}
				}
				else if(all_must_match && (!must_match || must_match(tmp,must_match_arg)))
				{
					if(all_ancestors_must_match)
						return FALSE;
					HTML_Element *match_elm = GetMatchingParent(TRUE,tmp,func,func_arg);
					if(!match_elm)
						return FALSE;
					if(match_elm->IsAncestorOf(stop_elm))
						break;
					tmp = (HTML_Element*)match_elm->NextSiblingActual();
					continue;
				}
				tmp = (HTML_Element*) tmp->NextActual();
			}
		}
		// Check start_elm + stop_elm
		BOOL start_matches = func(start_elm,func_arg);
		BOOL stop_matches = func(stop_elm,func_arg);
		if(all_ancestors_must_match) // all_must_match == TRUE
			return start_matches && stop_matches && !GetMatchingParent(FALSE,start_elm,func,func_arg) && !GetMatchingParent(FALSE,stop_elm,func,func_arg);
		start_matches = start_matches || GetMatchingParent(TRUE,start_elm,func,func_arg);
		stop_matches = stop_matches || GetMatchingParent(TRUE,stop_elm,func,func_arg);
		if(all_must_match)
		{
			if(start_matches && stop_matches)
				return TRUE;
			if(start_matches || stop_matches || found)
			{
				if(!start_matches && must_match && !must_match(start_elm,must_match_arg))
					start_matches = !GetMatchingParent(TRUE,start_elm,must_match,must_match_arg);
				if(!stop_matches && must_match && !must_match(stop_elm,must_match_arg))
					stop_matches = !GetMatchingParent(TRUE,stop_elm,must_match,must_match_arg);
				return start_matches && stop_matches;
			}
			return FALSE;
		}
		return start_matches || stop_matches; // found == FALSE
	}
	else
	{
		HTML_Element *helm = m_layout_modifier.IsActive() ? m_layout_modifier.m_helm : m_caret.GetElement();
		if(helm)
		{
			BOOL matches = func(helm,func_arg);
			if(all_ancestors_must_match)
				return matches && !GetMatchingParent(FALSE,helm,func,func_arg);
			return matches || GetMatchingParent(TRUE,helm,func,func_arg);
		}
	}
	return FALSE;
}

OP_STATUS OpDocumentEdit::ApplyFunctionBetween(HTML_Element *start_elm, HTML_Element *stop_elm, OP_STATUS (*func)(HTML_Element*,void*,BOOL&), void *func_arg, int *call_count, int *true_count, BOOL only_actual)
{
	DEBUG_CHECKER(TRUE);
	if(call_count)
		*call_count = 0;
	if(true_count)
		*true_count = 0;
	if((!start_elm && !stop_elm) || !func)
	{
		OP_ASSERT(FALSE);
		return OpStatus::OK;
	}
	HTML_Element *container = NULL;
	if(!start_elm || !stop_elm)
	{
		if((container = GetEditableContainer(start_elm ? start_elm : stop_elm)) == NULL)
		{
			// This might happen when there's no <body> and e.g. <html> is contentEditable.
			OP_ASSERT(!"Editing when there's no <body>?");
			return OpStatus::OK;
		}
		if(start_elm)
			stop_elm = container->LastLeaf() ? container->LastLeaf() : container;
		else
			start_elm = container;
	}
	if(!container)
	{
		if((container = GetEditableContainer(start_elm)) == NULL)
		{
			// This might happen when there's no <body> and e.g. <html> is contentEditable.
			OP_ASSERT(!"Editing when there's no <body>?");
			return OpStatus::OK;
		}
		if(!container->IsAncestorOf(stop_elm))
		{
			OP_ASSERT(FALSE);
			stop_elm = container->LastLeaf() ? container->LastLeaf() : container;
		}
	}

	HTML_Element *helm = start_elm;
	BOOL going_up = FALSE;
	for(;;)
	{
		if(!helm)
		{
			OP_ASSERT(FALSE);
			return OpStatus::OK;
		}
		if(!only_actual || helm->IsIncludedActual())
		{
			BOOL ret = FALSE;
			if(call_count)
				(*call_count)++;
			RETURN_IF_ERROR(func(helm,func_arg,ret));
			if(ret && true_count)
				(*true_count)++;
		}
		if(helm == stop_elm)
		{
			if(going_up)
				break;
			going_up = TRUE;
			helm = start_elm;
			if(start_elm == container)
				break;
			stop_elm = container;
		}
		helm = going_up ? helm->Parent() : helm->Next();
	}
	return OpStatus::OK;
}

SiblingBlock OpDocumentEdit::GetBlockSiblings(HTML_Element *helm, BOOL stop_at_br, HTML_Element *stop_pred, HTML_Element *stop_suc)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || IsBlockElement(helm) || helm == stop_pred || helm == stop_suc)
		return SiblingBlock(helm,helm);

	SiblingBlock block;
	HTML_Element *tmp;
	tmp = helm;
	while(tmp->Pred() && !IsBlockElement(tmp->Pred())
		&& !(stop_at_br && tmp->Pred()->Type() == HE_BR) && tmp->Pred() != stop_pred)
	{
		tmp = tmp->Pred();
	}
	block.start = tmp;
	tmp = helm;
	while(tmp->Suc() && !IsBlockElement(tmp->Suc())
		&& !(stop_at_br && tmp->Type() == HE_BR) && tmp->Suc() != stop_suc)
	{
		tmp = tmp->Suc();
	}
	block.stop = tmp;
	return block;
}

void OpDocumentEdit::AdjustStartStopToBlocks(HTML_Element *&start_elm, HTML_Element *&stop_elm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *tmp;
	OP_ASSERT(start_elm && stop_elm && (start_elm == stop_elm || start_elm->Precedes(stop_elm)));
	if(start_elm && !IsBlockElement(start_elm))
	{
		tmp = m_doc->GetCaret()->GetContainingElementActual(start_elm);
		OP_ASSERT(GetBody() && GetBody()->IsAncestorOf(tmp));
		if(tmp != GetBody())
			start_elm = tmp;
		else
		{
			while(start_elm->ParentActual() != tmp)
				start_elm = start_elm->ParentActual();
			while(start_elm->PredActual() && !IsBlockElement(start_elm->PredActual()))
				start_elm = start_elm->PredActual();
		}
	}
	if(stop_elm && !IsBlockElement(stop_elm))
	{
		tmp = m_doc->GetCaret()->GetContainingElementActual(stop_elm);
		OP_ASSERT(GetBody() && GetBody()->IsAncestorOf(tmp));
		if(tmp != GetBody())
			stop_elm = tmp;
		else
		{
			while(stop_elm->ParentActual() != tmp)
				stop_elm = stop_elm->ParentActual();
			while(stop_elm->SucActual() && !IsBlockElement(stop_elm->SucActual()))
				stop_elm = stop_elm->SucActual();
		}
	}
}

void OpDocumentEdit::AdjustStartStopToActual(HTML_Element *&start_elm, HTML_Element *&stop_elm)
{
	DEBUG_CHECKER(TRUE);
	BOOL non_actual = FALSE;
	if(!start_elm->IsIncludedActual())
	{
		non_actual = TRUE;
		if(start_elm == stop_elm)
		{
			start_elm = start_elm->FirstLeafActual();
			stop_elm = stop_elm->LastLeafActual();
			return;
		}
		start_elm = start_elm->NextActual();
		if(start_elm && start_elm->FirstLeafActual())
			start_elm = start_elm->FirstLeafActual();
	}
	if(!stop_elm->IsIncludedActual())
	{
		non_actual = TRUE;
		stop_elm = stop_elm->PrevSiblingActual();
		if(stop_elm && stop_elm->LastLeafActual())
			stop_elm = stop_elm->LastLeafActual();
	}
	if(non_actual)
	{
		if(!start_elm || !stop_elm || (start_elm != stop_elm && !start_elm->Precedes(stop_elm)))
		{
			start_elm = stop_elm = NULL;
			return;
		}
	}
}

void OpDocumentEdit::AdjustStartStopNotBreakTypes(HTML_Element *&start_elm, HTML_Element *&stop_elm, HTML_ElementType *types)
{
	DEBUG_CHECKER(TRUE);
	if(!start_elm || !stop_elm)
	{
		OP_ASSERT(FALSE);
		return;
	}
	OP_ASSERT(start_elm == stop_elm || start_elm->Precedes(stop_elm));

	AdjustStartStopToActual(start_elm,stop_elm);
	if(!start_elm)
		return;

	HTML_Element *start_break = GetRootOfTypes(start_elm,types);
	HTML_Element *stop_break = GetRootOfTypes(stop_elm,types);
	if(!start_break && !stop_break)
		return;

	BOOL select_nothing = FALSE;
	if(start_break == stop_break)
		select_nothing = TRUE;
	else if(start_break && stop_break)
	{
		HTML_Element *next_sibling = start_break->NextSiblingActual();
		if(!next_sibling || next_sibling == stop_break || next_sibling->IsAncestorOf(stop_break))
			select_nothing = TRUE;
	}
	if(select_nothing)
	{
		start_elm = stop_elm = NULL;
		return;
	}
	if(start_break && !start_break->IsAncestorOf(start_elm->PrevSiblingActual()))
		start_break = NULL;
	if(stop_break && !stop_break->IsAncestorOf(stop_elm->NextSiblingActual()))
		stop_break = NULL;
	if(!start_break && !stop_break)
		return;
	if(start_break)
	{
		start_elm = start_break->NextSiblingActual();
		if(start_elm && start_elm->FirstLeafActual())
			start_elm = start_elm->FirstLeafActual();
	}
	if(stop_break)
	{
		stop_elm = stop_break->PrevSiblingActual();
		if(stop_elm && stop_elm->LastLeafActual())
			stop_elm = stop_elm->LastLeafActual();
	}
}

HTML_Element *OpDocumentEdit::AdjustStartStopCloserRoot(HTML_Element *&start_elm, HTML_Element *&stop_elm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *before_start = start_elm->PrevSiblingActual();
	HTML_Element *after_stop = stop_elm->NextSiblingActual();
	HTML_Element *shared_containing_elm = GetSharedContainingElement(start_elm,stop_elm);

	// Let's set shared_containing_elm+start_elm+stop_elm as close to the root as possible
	// If all cells in a table are selected (and nothing around) we want to change the
	// cell-content and not the table itself
	HTML_Element *body = GetBody();
	while(shared_containing_elm != body &&
		!shared_containing_elm->IsAncestorOf(before_start) &&
		!shared_containing_elm->IsAncestorOf(after_stop) &&
		shared_containing_elm->ParentActual() &&
		shared_containing_elm->ParentActual()->Type() != HE_TABLE
		)
	{
		shared_containing_elm = shared_containing_elm->ParentActual();
	}
	while(start_elm->ParentActual() != shared_containing_elm &&
		!start_elm->ParentActual()->IsAncestorOf(before_start))
	{
		start_elm = start_elm->ParentActual();
	}
	while(stop_elm->ParentActual() != shared_containing_elm &&
		!stop_elm->ParentActual()->IsAncestorOf(after_stop))
	{
		stop_elm = stop_elm->ParentActual();
	}
	return shared_containing_elm;
}

void OpDocumentEdit::JustifySelection(CSSValue align)
{
	DEBUG_CHECKER(TRUE);
	SelectionState selection_state = GetSelectionState();
	if(!selection_state.IsValid())
		return;

	OP_STATUS status = OpStatus::OK;
	HTML_Element *start_elm = selection_state.editable_start_elm;
	HTML_Element *stop_elm = selection_state.editable_stop_elm;
	AdjustStartStopToBlocks(start_elm,stop_elm);
	HTML_Element *shared_containing_elm = AdjustStartStopCloserRoot(start_elm,stop_elm);
	BeginChange(shared_containing_elm);

	// is_removing_attr will be != NULL while removing ATTR_ALIGN from children where we've
	// set alignment on an ancestor where ChildrenShouldInheritAlignment(ancestor) == TRUE.
	BOOL is_removing_attr = FALSE;
	HTML_Element *next_to_set = NULL; // When we should stop removing
	HTML_Element *tmp = start_elm;
	HTML_Element *after_justify = stop_elm->NextSiblingActual();
	if(!GetBody()->IsAncestorOf(after_justify))
		after_justify = NULL; // after_justify was probably HEAD or something...
	while(tmp != after_justify && tmp)
	{
		if(is_removing_attr && tmp == next_to_set)
			is_removing_attr = FALSE;
		if(is_removing_attr)
		{
			if(tmp->HasAttr(ATTR_ALIGN))
			{
				OnElementChange(tmp);
				tmp->RemoveAttribute(ATTR_ALIGN);
				OnElementChanged(tmp);
			}
			if(!ChildrenShouldInheritAlignment(tmp))
			{
				tmp = tmp->NextSiblingActual();
				continue;
			}
		}
		else
		{
			// Place DIV outside inline elements, we won't hit after_justify as it has been set above to
			// not be an inline element Suc() another inline element.
			if(!IsBlockElement(tmp))
			{
				if(!(tmp->Type() == HE_TEXT && tmp->GetTextContentLength() && IsWhiteSpaceOnly(tmp->TextContent())))
				{
					// Add also "non-actual" elements under DIV
					SiblingBlock under_div = GetBlockSiblings(tmp);
					HTML_Element* helm = NewElement(HE_DIV);
					if(!helm || OpStatus::IsError(status = SetAlignAttrFromCSSValue(helm,align)))
						break;
					helm->PrecedeSafe(m_doc,under_div.start);
					// If under_div.stop is not actual it may disappear in MoveSiblingBlock.
					tmp = under_div.stop->NextSiblingActual();
					MoveSiblingBlock(under_div,helm);
					continue;
				}
			}
			else if(!tmp->IsAncestorOf(after_justify) && ElementHandlesAlignAttribute(tmp))
			{
				SetAlignAttrFromCSSValue(tmp,align);
				tmp->MarkDirty(m_doc);
				if(ChildrenShouldInheritAlignment(tmp)) // Let's remove alignment from the children
				{
					next_to_set = tmp->NextSiblingActual();
					is_removing_attr = TRUE;
				}
				else
				{
					tmp = tmp->NextSiblingActual();
					continue;
				}
			}
		}
		tmp = tmp->NextActual();
	}

	RestoreSelectionState(selection_state);
	if(OpStatus::IsError(status))
		AbortChange();
	else
		EndChange(shared_containing_elm,TIDY_LEVEL_MINIMAL);
}

OP_STATUS OpDocumentEdit::IndentSelection(int px)
{
	DEBUG_CHECKER(TRUE);
	OP_STATUS status = OpStatus::OK;
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
		return status;

	BOOL indent = px > 0;
	HTML_Element *start_elm = selection_state.editable_start_elm;
	HTML_Element *stop_elm = selection_state.editable_stop_elm;
	HTML_ElementType li_types[] = {HE_OL, HE_UL, HE_UNKNOWN};
	AdjustStartStopNotBreakTypes(start_elm,stop_elm,li_types);
	if(!start_elm || !stop_elm)
	{
		RestoreSelectionState(selection_state);
		return status;
	}
	AdjustStartStopToBlocks(start_elm,stop_elm);
	HTML_Element *shared_containing_elm = AdjustStartStopCloserRoot(start_elm,stop_elm);
	BeginChange(shared_containing_elm);

	// is_removing_attr will be != NULL while removing ATTR_ALIGN from children where we've
	// set alignment on an ancestor where ChildrenShouldInheritAlignment(ancestor) == TRUE.
	HTML_Element *tmp = start_elm;
	HTML_Element *after_indent = stop_elm->NextSiblingActual();
	if(!GetBody()->IsAncestorOf(after_indent))
		after_indent = NULL; // after_indent was probably HEAD or something...
	while(tmp && tmp != after_indent && OpStatus::IsSuccess(status))
	{
		// Place DIV outside inline elements, we won't hit after_indent as it has been set in AdjustStartStopToBlocks
		// not be an inline element Suc() another inline element.
		if(!IsBlockElement(tmp))
		{
			if(indent && !(tmp->Type() == HE_TEXT && tmp->GetTextContentLength() && IsWhiteSpaceOnly(tmp->TextContent())))
			{
				// Add also "non-actual" elements under DIV
				SiblingBlock under_div = GetBlockSiblings(tmp);
				HTML_Element* helm = NewElement(HE_DIV);
				if(!helm)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				helm->PrecedeSafe(m_doc,under_div.start);
				// If under_div.stop is not actual it may disappear in MoveSiblingBlock.
				tmp = under_div.stop->NextSiblingActual();
				MoveSiblingBlock(under_div,helm);
				status = SetCSSIndent(helm,px);
				continue;
			}
		}
		else if(!tmp->IsAncestorOf(after_indent) && ElementHandlesMarginStyle(tmp))
		{
			if(IsAnyOfTypes(tmp,li_types))
			{
				tmp = tmp->NextSiblingActual();
				continue;
			}
			int before_indent;
			if(OpStatus::IsError(status = GetCSSIndent(tmp,before_indent)))
				break;
			if(indent || before_indent)
			{
				if(OpStatus::IsError(status = SetCSSIndent(tmp,MAX(before_indent+px,0))))
					break;
				tmp->MarkDirty(m_doc);
				tmp = tmp->NextSiblingActual();
				continue;
			}
		}
		tmp = tmp->NextActual();
	}

	RestoreSelectionState(selection_state);
	if(OpStatus::IsError(status))
	{
		if(OpStatus::IsMemoryError(status))
			ReportOOM();
		AbortChange();
	}
	else
		EndChange(shared_containing_elm,TIDY_LEVEL_MINIMAL);

	return status;
}

OP_STATUS OpDocumentEdit::GetCSSIndent(HTML_Element *helm, int &result_px)
{
	DEBUG_CHECKER(TRUE);
	//result_px = 0;
	//CSS_property_list *props = helm->GetCSS_Style();
	//if(!props)
	//	return OpStatus::OK;
	//
	//CSS_decl *p = props->GetFirstDecl();
	//for(p = props->GetFirstDecl(); p && p->GetProperty() != CSS_PROPERTY_margin_left; p = p->Suc());
	//if(p)
	//	result_px = p->GetNumberValue(0);
	//return OpStatus::OK;

	result_px = 0;
	Head prop_list;
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	LayoutProperties* lprops = LayoutProperties::CreateCascade(helm, prop_list, hld_profile);
	if(lprops)
	{
		result_px = MAX(lprops->GetProps()->margin_left,0);
	}
	prop_list.Clear();
	return OpStatus::OK;
}

static OP_STATUS AddDecl(CSS_property_list *props, int px)
{
	TRAPD(status, props->AddDeclL(CSS_PROPERTY_margin_left, (float)px, CSS_PX, TRUE, CSS_decl::ORIGIN_USER));
	return status;
}

OP_STATUS OpDocumentEdit::SetCSSIndent(HTML_Element *helm, int px)
{
	DEBUG_CHECKER(TRUE);
	OP_STATUS status = OpStatus::OK;
	OnElementChange(helm);
	CSS_property_list *props = helm->GetCSS_Style();
	if(!props)
	{
		props = OP_NEW(CSS_property_list, ());
		if (!props)
			return OpStatus::ERR_NO_MEMORY;
		props->Ref();
		if(OpStatus::IsError(status = helm->SetCSS_Style(props)))
		{
			props->Unref();
			return status;
		}
	}
	CSS_decl *p;
	if((p = props->RemoveDecl(CSS_PROPERTY_margin_left)) != NULL)
		p->Unref();
	status = AddDecl(props, px);
	if(OpStatus::IsSuccess(status))
		status = m_doc->GetHLDocProfile()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, 0, FALSE);
	OnElementChanged(helm);
	return status;
}

OP_STATUS OpDocumentEdit::RemoveInlinePropertyBetween(HTML_Element *start_elm, HTML_Element *stop_elm, int prop, BOOL only_block_elements)
{
	DEBUG_CHECKER(TRUE);
	OP_STATUS status = OpStatus::OK;
	HTML_Element *shared_containing_elm = NULL;
	if(!start_elm || !stop_elm)
		return status;
	HTML_Element *tmp = start_elm;
	while(tmp)
	{
		CSS_property_list *props;
		if((!only_block_elements || IsBlockElement(tmp)) &&
			tmp->IsIncludedActual() &&
			(props = tmp->GetCSS_Style()))
		{
			CSS_decl *p;
			for(p = props->GetFirstDecl(); p && p->GetProperty() != prop; p = p->Suc()) {}
			if(p)
			{
				if(!shared_containing_elm)
				{
					shared_containing_elm = GetSharedContainingElement(start_elm,stop_elm);
					BeginChange(shared_containing_elm);
				}
				OnElementChange(tmp);
				if((p = props->RemoveDecl(prop)) != NULL)
					p->Unref();
				status = m_doc->GetHLDocProfile()->GetLayoutWorkplace()->ApplyPropertyChanges(tmp, 0, FALSE);
				OnElementChanged(tmp);
			}
		}
		if(tmp == stop_elm)
			break;
		tmp = tmp->Next();
	}
	if(shared_containing_elm)
		EndChange(shared_containing_elm);
	return status;
}

OP_STATUS OpDocumentEdit::SetAlignAttrFromCSSValue(HTML_Element *helm, CSSValue value)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(helm && !helm->IsText());
	const uni_char *align =
		value == CSS_VALUE_center ? UNI_L("center") :
		value == CSS_VALUE_left ? UNI_L("left") :
		value == CSS_VALUE_right ? UNI_L("right") : UNI_L("justify");
	OnElementChange(helm);
	OP_STATUS status = helm->SetAttribute(m_doc->GetLogicalDocument(), ATTR_XML, UNI_L("align"), NS_IDX_DEFAULT, align, uni_strlen(align), NULL, TRUE);
	OnElementChanged(helm);
	return status;
}

void OpDocumentEdit::CleanAroundBlock(SiblingBlock block)
{
	DEBUG_CHECKER(TRUE);
	if(block.IsEmpty())
		return;
	SiblingBlock remaining;
	HTML_Element *parent = block.start->Parent();
	if(parent->Type() == HE_OL || parent->Type() == HE_UL || parent->Type() == HE_LI)
	{
		if(block.start->Pred())
		{
			remaining = SiblingBlock(parent->FirstChild(),block.start->Pred());
			CleanUnnecessaryElementsInBlock(&remaining);
		}
		if(block.stop->Suc())
		{
			remaining = SiblingBlock(block.stop->Suc(),parent->LastChild());
			CleanUnnecessaryElementsInBlock(&remaining);
		}
	}
}

OP_STATUS OpDocumentEdit::SplitListAroundAndMoveBlock(SiblingBlock block, HTML_Element *new_parent,
	HTML_Element *after_me, HTML_Element *&split_parent, HTML_Element *&split_pred)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(!block.IsEmpty() && block.start->Parent() == block.stop->Parent());

	HTML_Element *first_pred,*first_suc,*second_pred,*second_suc;
	HTML_Element *parent = block.start->ParentActual();
	HTML_Element *parent_parent = parent->ParentActual();
	HTML_Element *new_li = NULL;

	// Create new LI element right away to simplify things a bit,
	// this element might be unnecessary and will in that case be removed below
	if(parent_parent->Type() == HE_LI)
	{
		new_li = NEW_HTML_Element();
		if(new_li == NULL)
			return OpStatus::ERR_NO_MEMORY;
		if(OpStatus::IsError(new_li->Construct(m_doc->GetHLDocProfile(),parent_parent,TRUE/*???*/)))
		{
			DeleteElement(new_li);
			return OpStatus::ERR_NO_MEMORY;
		}
		new_li->SetInserted(HE_INSERTED_BY_DOM);
		new_li->SetEndTagFound(TRUE);
		new_li->RemoveAttribute(ATTR_ID);
	}

	CleanAroundBlock(block);
	first_pred = block.start->Pred();
	first_suc = block.stop->Suc();
	MoveSiblingBlock(block,new_parent,after_me);

	if(!first_pred && !first_suc) //remove list
	{
		second_pred = parent->Pred();
		if (second_pred && second_pred->GetIsListMarkerPseudoElement())
			second_pred = NULL;
		second_suc = parent->Suc();
		DeleteElement(parent);
	}
	else if(first_pred && first_suc) //split list
	{
		HTML_Element *new_list = NEW_HTML_Element();
		second_pred = parent;
		second_suc = new_list;
		if(new_list == NULL)
		{
			DeleteElement(new_li);
			return OpStatus::ERR_NO_MEMORY;
		}
		if(OpStatus::IsError(new_list->Construct(m_doc->GetHLDocProfile(),parent,TRUE/*???*/)))
		{
			DeleteElement(new_li);
			DeleteElement(new_list);
			return OpStatus::ERR_NO_MEMORY;
		}
		new_list->SetInserted(HE_INSERTED_BY_DOM);
		new_list->SetEndTagFound(TRUE);
		new_list->RemoveAttribute(ATTR_ID);
		SiblingBlock cont_block = SiblingBlock(first_suc,parent->LastChild());
		new_list->FollowSafe(m_doc,parent);
		MoveSiblingBlock(cont_block,new_list);
	}
	else if(first_suc) //keep lower range of list
	{
		second_pred = parent->Pred();
		if (second_pred && second_pred->GetIsListMarkerPseudoElement())
			second_pred = NULL;
		second_suc = parent;
	}
	else // keep higher range of list
	{
		second_pred = parent;
		second_suc = parent->Suc();
	}

	if(parent_parent->Type() != HE_LI)
	{
		split_parent = parent_parent;
		split_pred = second_pred;
		return OpStatus::OK;
	}

	// If the list (probably) we moved block from was inside a LI element, this element
	// should be splitted as well.
	parent = parent_parent;
	parent_parent = parent->Parent();
	SiblingBlock remaining_over;
	if (second_pred)
	{
		HTML_Element* start = parent->FirstChild();
		if (start && start->GetIsListMarkerPseudoElement())
			start = start->Suc();
		remaining_over = SiblingBlock(start, second_pred);
	}

	SiblingBlock remaining_under;
	if (second_suc)
	{
		HTML_Element* end = parent->LastChild();
		if (end && end->GetIsListMarkerPseudoElement())
			end = NULL;
		remaining_under = SiblingBlock(second_suc, end);
	}

	CleanUnnecessaryElementsInBlock(&remaining_over);
	CleanUnnecessaryElementsInBlock(&remaining_under);

	split_parent = parent_parent;
	if(remaining_over.IsEmpty() && remaining_under.IsEmpty())
	{
		split_pred = parent->Pred();
		DeleteElement(parent);
		DeleteElement(new_li);
	}
	else if(!remaining_over.IsEmpty() && !remaining_under.IsEmpty())
	{
		split_pred = parent;
		new_li->FollowSafe(m_doc,parent);
		MoveSiblingBlock(remaining_under,new_li);
	}
	else if(remaining_over.IsEmpty())
	{
		split_pred = parent->Pred();
		DeleteElement(new_li);
	}
	else
	{
		split_pred = parent;
		DeleteElement(new_li);
	}

	return OpStatus::OK;
}

SiblingBlock OpDocumentEdit::GetSucListBlock(HTML_Element *root, SiblingBlock block)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(block.start->Parent() == block.stop->Parent());
	BOOL return_always = block.start->Parent() == root
		|| block.start->Parent()->Type() == HE_OL
		|| block.start->Parent()->Type() == HE_UL;

	HTML_Element *helm = block.stop->Suc();
	while(helm)
	{
		HTML_Element *next_elm = helm->FirstChild();
		if(next_elm)
		{
			BOOL go_down = TRUE;
			while(next_elm != helm)
			{
				if(go_down)
				{
					if(next_elm->FirstChild())
						next_elm = next_elm->FirstChild();
					else
						go_down = FALSE;
				}
				else
				{
					if(next_elm->Parent()->Type() == HE_OL || next_elm->Parent()->Type() == HE_UL)
					{
						block = GetBlockSiblings(next_elm);
						OP_ASSERT(block.start == next_elm);
						if(IsValidListBlock(block))
							return block;
					}
					if(next_elm->Suc())
					{
						next_elm = next_elm->Suc();
						go_down = TRUE;
					}
					else
						next_elm = next_elm->Parent();
				}
			}
		}
		if(return_always)
		{
			block = GetBlockSiblings(helm);
			OP_ASSERT(block.start == helm);
			if(IsValidListBlock(block))
				return block;
		}
		block = GetBlockSiblings(helm);
		helm = block.stop->Suc();
	}

	return SiblingBlock();
}

SiblingBlock OpDocumentEdit::GetNextListBlock(HTML_Element *root, SiblingBlock block)
{
	DEBUG_CHECKER(TRUE);
	SiblingBlock next_block;
	HTML_Element *helm = block.start;
	SiblingBlock suc_list_block;
	while(helm != GetBody() && (suc_list_block = GetSucListBlock(root,block)).IsEmpty())
	{
		helm = helm->Parent();
		if(!helm || helm == root)
			return SiblingBlock();
		block = GetBlockSiblings(helm);
	}
	if(helm == GetBody())
		return SiblingBlock();

	OP_ASSERT(block.start->Parent() == block.stop->Parent());

	return suc_list_block;
}

void OpDocumentEdit::MoveSiblingBlock(SiblingBlock block, HTML_Element *new_parent, HTML_Element *after_me)
{
	DEBUG_CHECKER(TRUE);
	if(block.IsEmpty())
		return;
	OP_ASSERT(new_parent);
	OP_ASSERT(!after_me || after_me->Parent() == new_parent);
	HTML_Element *next,*tmp = block.start;
	HTML_Element *caret_helm = m_caret.GetElement();
	int caret_ofs = m_caret.GetOffset();
	if(!after_me)
	{
		next = tmp->SucActual();
		tmp->OutSafe(m_doc, FALSE);
		if(new_parent->FirstChildActual())
			tmp->PrecedeSafe(m_doc,new_parent->FirstChildActual());
		else
			tmp->UnderSafe(m_doc,new_parent);
		after_me = tmp;
		if(tmp == block.stop)
			tmp = NULL;
		else
			tmp = next;
	}

	while(tmp)
	{
		next = tmp->SucActual();
		tmp->OutSafe(m_doc, FALSE);
		tmp->FollowSafe(m_doc,after_me);
		if(tmp == block.stop)
			break;
		after_me = tmp;
		tmp = next;
	}

	// If the caret moved during the operation, move it back.
	if(caret_helm && (caret_helm != m_caret.GetElement() || caret_ofs != m_caret.GetOffset()))
	{
		m_caret.Set(caret_helm,caret_ofs);
		m_caret.UpdatePos();
	}
}

void OpDocumentEdit::CleanUnnecessaryElementsInBlock(SiblingBlock *block, BOOL save_at_least_one)
{
	DEBUG_CHECKER(TRUE);
	if(block->IsEmpty() || (save_at_least_one && block->start == block->stop))
		return;
	HTML_Element *last_ok_helm = NULL;
	HTML_Element *end_helm = block->stop->Suc();
	for(HTML_Element *helm = block->start; helm != end_helm;)
	{
		OP_ASSERT(helm);
		if(!IsElmValidInListBlock(helm) && (!save_at_least_one || (helm != block->stop || last_ok_helm)))
		{
			if(helm == block->start)
				block->start = helm == block->stop ? NULL : helm->Suc();
			HTML_Element *next = helm->Suc();
			if(helm == m_caret_elm_copy)
				m_caret_elm_copy = NULL;
			if(helm == m_start_sel_copy.GetElement() || helm == m_stop_sel_copy.GetElement())
			{
				m_start_sel_copy = OldStyleTextSelectionPoint();
				m_stop_sel_copy = OldStyleTextSelectionPoint();
			}
			DeleteElement(helm);
			helm = next;
			continue;
		}
		last_ok_helm = helm;
		helm = helm->Suc();
	}
	OP_ASSERT(!save_at_least_one || last_ok_helm);
	block->stop = last_ok_helm;
}

OP_STATUS OpDocumentEdit::InsertAndOrToggleListOrdering(HTML_Element *shared_elm,
	SiblingBlock start_block, SiblingBlock stop_block, BOOL ordered)
{
	DEBUG_CHECKER(TRUE);
	INT32 nestling = GetListNestling(start_block.start);
	SiblingBlock block = start_block;
	HTML_Element *current_list = NULL;

	// Check if the list containing start_block has the same list-type as ordered specifies.
	// Then will all other items be added to this list - and no new list will be created.
	if(nestling)
	{
		HTML_Element *tmp_list = GetParentListElm(start_block.start);
		if(!((tmp_list->Type() == HE_OL) ^ ordered))
		{
			OP_ASSERT(block != stop_block);
			current_list = tmp_list;
			block = GetNextListBlock(shared_elm,block);
			nestling = GetListNestling(block.start);
			OP_ASSERT(!block.IsEmpty());
		}
	}

	while(TRUE)
	{
		OP_ASSERT(!block.IsEmpty());
		if(block.IsEmpty())
			return OpStatus::ERR;
		BOOL is_last_block = block == stop_block;

		HTML_Element *parent_list = nestling ? GetParentListElm(block.start) : NULL;
		if(!parent_list) // block is not in any list
		{
			OP_ASSERT(block.start->Parent() == shared_elm);
			HTML_Element *old_suc = block.stop->Suc();
			HTML_Element *new_current_list = NULL;

			// Replace BR with empty string
			if(block.start == block.stop && block.start->Type() == HE_BR)
			{
				HTML_Element* dummy_elm = NewTextElement(document_edit_dummy_str, 1);
				if(!dummy_elm)
					return OpStatus::ERR_NO_MEMORY;
				dummy_elm->FollowSafe(m_doc,block.start);
				if(m_caret_elm_copy == block.start)
					m_caret_elm_copy = dummy_elm;

				//FIXME: start and stop selection points should be updated to the new element, but the following lines of code doesn't work...

				//if(m_start_sel_copy.GetElement() == block.start)
				//	m_start_sel_copy.SetTextElement(dummy_elm);
				//if(m_stop_sel_copy.GetElement() == block.start)
				//	m_stop_sel_copy.SetTextElement(dummy_elm);

				if(m_start_sel_copy.GetElement() == block.start || m_stop_sel_copy.GetElement() == block.start)
				{
					m_start_sel_copy = OldStyleTextSelectionPoint();
					m_stop_sel_copy = OldStyleTextSelectionPoint();
				}

				DeleteElement(block.start);
				block = SiblingBlock(dummy_elm,dummy_elm);
			}
			else // Discard and remove this block if it doesn't contain any "necessary" elements
			{
				if(is_last_block)
				{
					CleanUnnecessaryElementsInBlock(&block);
					if(block.IsEmpty())
						break;
				}
				else
				{
					SiblingBlock next_block = GetNextListBlock(shared_elm,block);
					CleanUnnecessaryElementsInBlock(&block);
					if(block.IsEmpty())
					{
						block = next_block;
						nestling = GetListNestling(block.start);
						continue;
					}
				}
			}

			// Create new HE_LI and put block under
			BOOL transform_p_to_li = block.start->Type() == m_paragraph_element_type;
			BOOL already_li = block.start->Type() == HE_LI;
			HTML_Element *list_helm = already_li ? block.start :
				(transform_p_to_li ? NEW_HTML_Element() : NewElement(HE_LI));
			if(!list_helm && !already_li)
				return OpStatus::ERR_NO_MEMORY;
			if(!current_list)
			{
				new_current_list = NewElement(ordered ? HE_OL : HE_UL);
				if(!new_current_list)
				{
					if(!already_li)
						DeleteElement(list_helm);
					return OpStatus::ERR_NO_MEMORY;
				}

				current_list = new_current_list;
				if(old_suc)
					current_list->PrecedeSafe(m_doc,old_suc);
				else
					current_list->UnderSafe(m_doc,shared_elm);
			}

			if(transform_p_to_li) // "Transform" P to LI
			{
				OP_ASSERT(block.start == block.stop);
				HTML_Element *p_elm = block.start;
				if(OpStatus::IsError(list_helm->Construct(m_doc->GetHLDocProfile(),p_elm)))
				{
					DeleteElement(list_helm);
					if(new_current_list)
						DeleteElement(new_current_list);
					return OpStatus::ERR_NO_MEMORY;
				}
				list_helm->SetType(HE_LI);
				list_helm->SetInserted(HE_INSERTED_BY_DOM);
				list_helm->SetEndTagFound(TRUE);
				list_helm->RemoveAttribute(ATTR_ID);
				list_helm->UnderSafe(m_doc,current_list);
				SiblingBlock copy_block = SiblingBlock(p_elm->FirstChild(), p_elm->LastChild());
				CleanUnnecessaryElementsInBlock(&copy_block);
				if(!copy_block.IsEmpty())
					MoveSiblingBlock(copy_block, list_helm);
				else // Create "dummy-element" and put it into the new LI element.
				{
					HTML_Element* dummy_elm = NewTextElement(document_edit_dummy_str, 1);
					if(!dummy_elm)
					{
						if(new_current_list)
							DeleteElement(new_current_list);
						return OpStatus::ERR_NO_MEMORY;
					}
					dummy_elm->UnderSafe(m_doc,list_helm);
				}
				if(m_start_sel_copy.GetElement() == p_elm || m_stop_sel_copy.GetElement() == p_elm)
				{
					m_start_sel_copy = OldStyleTextSelectionPoint();
					m_stop_sel_copy = OldStyleTextSelectionPoint();
				}
				DeleteElement(p_elm);
				// Update block so that next call to GetNextListBlock will not end up in disaster...
				block = SiblingBlock(list_helm,list_helm);
			}
			else if(!already_li)// Just move the block to the new LI element
			{
				CleanUnnecessaryElementsInBlock(&block,TRUE);
				list_helm->UnderSafe(m_doc,current_list);
				MoveSiblingBlock(block, list_helm);
			}
			else
			{
				list_helm->OutSafe(m_doc, FALSE);
				list_helm->UnderSafe(m_doc,current_list);
			}
		}
		else if(parent_list != current_list) // Move element already in list to current_list
		{
			OP_ASSERT(block.start->Parent() == shared_elm || block.start->Parent() == parent_list);
			HTML_Element *split_parent, *split_pred;

			if(current_list)
			{
				if(block.start->Parent()->IsAncestorOf(current_list))
				{
					MoveSiblingBlock(block,current_list,current_list->LastChild());
				}
				else
				{
					HTML_Element *after_me = current_list->LastChild();
					for(HTML_Element *tmp = block.start; tmp; tmp = tmp->Parent())
					{
						if(tmp->Parent() == current_list)
						{
							after_me = tmp->Pred();
							break;
						}
					}
					RETURN_IF_ERROR(SplitListAroundAndMoveBlock(block,current_list,after_me,
						split_parent,split_pred));

					// Clear lists and list elements that have now possibly become empty
					HTML_Element *parent = split_parent;
					while(parent->Type() == HE_OL || parent->Type() == HE_UL || parent->Type() == HE_LI)
					{
						SiblingBlock remaining = SiblingBlock(parent->FirstChild(),parent->LastChild());
						CleanUnnecessaryElementsInBlock(&remaining);
						if(!remaining.IsEmpty())
							break;
						HTML_Element *new_parent = parent->Parent();
						if(m_start_sel_copy.GetElement() == parent || m_stop_sel_copy.GetElement() == parent)
						{
							m_start_sel_copy = OldStyleTextSelectionPoint();
							m_stop_sel_copy = OldStyleTextSelectionPoint();
						}
						DeleteElement(parent);
						parent = new_parent;
					}
				}
			}
			else // Create new current_list
			{
				current_list = NewElement(ordered ? HE_OL : HE_UL);
				if(!current_list)
					return OpStatus::ERR_NO_MEMORY;

				// Create a new list element for inserting current_list in if split_parent
				// is a list, if not - this element is unnecessary and will be removed below
				// but it's created in advance for simplicity.
				HTML_Element *new_li = NewElement(HE_LI);
				if(new_li == NULL)
				{
					DeleteElement(current_list);
					return OpStatus::ERR_NO_MEMORY;
				}

				OP_STATUS status = SplitListAroundAndMoveBlock(block,current_list,NULL,
					split_parent,split_pred);
				if(OpStatus::IsError(status))
				{
					DeleteElement(current_list);
					DeleteElement(new_li);
					return status;
				}

				// The new list should be encapsulated inside the list element created above.
				if(split_parent->Type() == HE_OL || split_parent->Type() == HE_UL)
				{
					current_list->UnderSafe(m_doc,new_li);
					if(split_pred)
						new_li->FollowSafe(m_doc,split_pred);
					else if(split_parent->FirstChild())
						new_li->PrecedeSafe(m_doc,split_parent->FirstChild());
					else
						new_li->UnderSafe(m_doc,split_parent);
				}
				else
				{
					if(split_pred)
						current_list->FollowSafe(m_doc,split_pred);
					else if(split_parent->FirstChild())
						current_list->PrecedeSafe(m_doc,split_parent->FirstChild());
					else
						current_list->UnderSafe(m_doc,split_parent);
					DeleteElement(new_li);
				}
			}
		}
		// else... helm is already in current_list

		if(is_last_block)
			break;
		block = GetNextListBlock(shared_elm,block);
		nestling = GetListNestling(block.start);
	}
	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::IncreaseListNestling()
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *start_elm,*stop_elm;
	OP_STATUS status = OpStatus::OK;
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
		return status;

	start_elm = selection_state.editable_start_elm;
	stop_elm = selection_state.editable_stop_elm;

	HTML_Element *shared_containing_elm = GetSharedContainingElement(start_elm,stop_elm);
	if(!shared_containing_elm)
		return status;
	HTML_Element* tmp = GetRootList(shared_containing_elm);
	if(tmp)
		shared_containing_elm = tmp;

	SiblingBlock start_block,stop_block;

	// Move start_elm and stop_elm up in the tree to the nearest child under a list or under shared_containing_elm.
	while(start_elm && start_elm->Parent() && start_elm->Parent() != shared_containing_elm && start_elm->Parent()->Type() != HE_OL && start_elm->Parent()->Type() != HE_UL)
		start_elm = start_elm->Parent();
	start_block = GetBlockSiblings(start_elm);
	while(stop_elm && stop_elm->Parent() && stop_elm->Parent() != shared_containing_elm && stop_elm->Parent()->Type() != HE_OL && stop_elm->Parent()->Type() != HE_UL)
		stop_elm = stop_elm->Parent();
	stop_block = GetBlockSiblings(stop_elm);

	HTML_Element *change_elm = ((!shared_containing_elm->IsIncludedActual()) ? shared_containing_elm->ParentActual() : shared_containing_elm);
	BeginChange(change_elm);

	SiblingBlock block = start_block;
	while(!block.IsEmpty())
	{
		HTML_Element *parent = block.start->Parent();

		OP_ASSERT(block.start->IsIncludedActual());
		OP_ASSERT(block.stop->IsIncludedActual());

		// We're only intrested in items that are already in a list
		if (parent->Type() == HE_OL || parent->Type() == HE_UL)
		{
			HTML_Element *pred = block.start->Pred();
			HTML_Element *suc = block.stop->Suc();
			HTML_Element *list = NULL;
			if(pred && pred->Type() == parent->Type()) // move element(s) to list above
			{
				list = pred;
				MoveSiblingBlock(block,pred,pred->LastChild());
			}
			else if(suc && suc->Type() == parent->Type()) // move element(s) to list below
				MoveSiblingBlock(block,suc,NULL);
			else // Let's create a new more deeply nestled list
			{
				list = NewCopyOfElement(parent,TRUE);
				if(!list)
				{
					status = OpStatus::ERR_NO_MEMORY;
					ReportOOM();
					break;
				}
				if(suc)
					list->PrecedeSafe(m_doc,suc);
				else
					list->UnderSafe(m_doc,parent);
				MoveSiblingBlock(block,list,NULL);
			}
			// If true -> we moved the element(s) to the list above and can now concat this list with list below
			if(suc && list && suc->Type() == list->Type())
			{
				MoveSiblingBlock(SiblingBlock(suc->FirstChild(),suc->LastChild()),list,list->LastChild());
				DeleteElement(suc);
			}
			parent->MarkExtraDirty(m_doc);
		}
		if (block == stop_block)
			break;
		block = GetNextListBlock(shared_containing_elm,block);
	}

	RestoreSelectionState(selection_state);
	if(OpStatus::IsSuccess(status))
		EndChange(change_elm);
	else
		AbortChange();

	return status;
}

OP_STATUS OpDocumentEdit::DecreaseListNestling()
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *start_elm,*stop_elm;
	OP_STATUS status = OpStatus::OK;
	SelectionState selection_state = GetSelectionState(TRUE, FALSE, TRUE, TRUE);
	if(!selection_state.IsValid())
		return status;

	start_elm = selection_state.editable_start_elm;
	stop_elm = selection_state.editable_stop_elm;

	HTML_Element *shared_containing_elm = GetSharedContainingElement(start_elm,stop_elm);
	if(!shared_containing_elm)
		return status;
	HTML_Element* tmp = GetRootList(shared_containing_elm);
	if(tmp)
		shared_containing_elm = tmp;
	HTML_Element *change_elm;
	HTML_Element *iter = shared_containing_elm;
	// Find the shared container being outside of any list.
	do
	{
		change_elm = iter;
		iter = iter->Parent();
	}
	while (change_elm->IsMatchingType(Markup::HTE_UL, NS_HTML) || change_elm->IsMatchingType(Markup::HTE_OL, NS_HTML) || (change_elm->IsMatchingType(Markup::HTE_LI, NS_HTML)));

	change_elm = ((!change_elm->IsIncludedActual()) ? change_elm->ParentActual() : change_elm);

	SiblingBlock start_block,stop_block;
	while(start_elm && start_elm->Parent() && start_elm->Parent() != shared_containing_elm && start_elm->Parent()->Type() != HE_OL && start_elm->Parent()->Type() != HE_UL)
		start_elm = start_elm->Parent();
	start_block = GetBlockSiblings(start_elm);
	while(stop_elm && stop_elm->Parent() && stop_elm->Parent() != shared_containing_elm && stop_elm->Parent()->Type() != HE_OL && stop_elm->Parent()->Type() != HE_UL)
		stop_elm = stop_elm->Parent();
	stop_block = GetBlockSiblings(stop_elm);

	BeginChange(change_elm);

	status = DecreaseMaxListNestling(shared_containing_elm,start_block,stop_block,0);
	change_elm->MarkExtraDirty(m_doc);

	RestoreSelectionState(selection_state);
	if(OpStatus::IsSuccess(status))
		EndChange(change_elm);
	else
		AbortChange();

	return status;
}

OP_STATUS OpDocumentEdit::DecreaseMaxListNestling(HTML_Element *shared_elm, SiblingBlock start_block, SiblingBlock stop_block, INT32 max_nestling)
{
	DEBUG_CHECKER(TRUE);
	INT32 nestling = GetListNestling(start_block.start);
	SiblingBlock block = start_block;
	while(TRUE)
	{
		BOOL is_last_block = block == stop_block;
		BOOL next_block_already_found = FALSE;
		OP_ASSERT(!block.IsEmpty());
		if(block.IsEmpty())
			return OpStatus::ERR;

		// Only perform this operation on items that are maximally nestled.
		if(nestling && nestling >= max_nestling)
		{
			OP_ASSERT(block.start->Parent() == shared_elm || block.start->Parent()->Type() == HE_OL || block.start->Parent()->Type() == HE_UL);
			HTML_Element *split_parent, *split_pred;

			HTML_Element *container = NewElement(m_paragraph_element_type); // just a temporary element...
			if(container == NULL)
				return OpStatus::ERR_NO_MEMORY;
			container->FollowSafe(m_doc, shared_elm); // Make the container part of the tree.
			OP_STATUS status = SplitListAroundAndMoveBlock(block,container,NULL,split_parent,split_pred);
			if(OpStatus::IsError(status))
			{
				DeleteElement(container);
				return status;
			}
			NonActualElementPosition split_pred_info;
			RETURN_IF_ERROR(split_pred_info.Construct(split_pred));

			// If the new parent is not a list and the block is a list-item (LI),
			// then convert it to a paragraph element.
			if(block.start->Type() == HE_LI && split_parent->Type() != HE_OL && split_parent->Type() != HE_UL)
			{
				OP_ASSERT(block.start == block.stop);
				HTML_Element *li_elm = block.start;
				HTML_Element *first_child_actual = li_elm->FirstChildActual();
				HTML_Element *last_child_actual = li_elm->LastChildActual();

				SiblingBlock child_block = SiblingBlock(first_child_actual, last_child_actual);
				CleanUnnecessaryElementsInBlock(&child_block);
				if(!child_block.IsEmpty())
				{
					HTML_Element *p_elm;
					BOOL move_from_li_to_p = FALSE;
					// LI just contains one paragraph element, don't create any new
					if(first_child_actual->Type() == m_paragraph_element_type && first_child_actual == last_child_actual)
					{
						p_elm = first_child_actual;
						p_elm->OutSafe(m_doc, FALSE);
					}
					else
					{
						p_elm = NEW_HTML_Element();
						if(p_elm == NULL || OpStatus::IsError(p_elm->Construct(m_doc->GetHLDocProfile(),li_elm)))
						{
							DeleteElement(p_elm);
							DeleteElement(container);
							return OpStatus::ERR_NO_MEMORY;
						}
						p_elm->SetType(m_paragraph_element_type);
						p_elm->SetInserted(HE_INSERTED_BY_DOM);
						p_elm->SetEndTagFound(TRUE);
						p_elm->RemoveAttribute(ATTR_ID);

						move_from_li_to_p = TRUE;
					}
					split_pred = split_pred_info.Get();
					if(split_pred)
						p_elm->FollowSafe(m_doc,split_pred);
					else if(split_parent->FirstChild())
						p_elm->PrecedeSafe(m_doc,split_parent->FirstChild());
					else
						p_elm->UnderSafe(m_doc,split_parent);

					// Move children of the LI element to the new paragraph element.
					if (move_from_li_to_p)
						MoveSiblingBlock(SiblingBlock(first_child_actual,last_child_actual), p_elm);

					// Update block so that next call to GetNextListBlock will not end up in disaster...
					block = SiblingBlock(p_elm,p_elm);

					if (m_start_sel_copy.GetElement() == li_elm)
						m_start_sel_copy = GetTextSelectionPoint(p_elm, m_start_sel_copy.GetElementCharacterOffset());
					if (m_stop_sel_copy.GetElement() == li_elm)
						m_stop_sel_copy = GetTextSelectionPoint(p_elm, m_stop_sel_copy.GetElementCharacterOffset());
				}
				else // if LI is empty -> just remove it
				{
					if(!is_last_block)
					{
						// "hack" for finding next block...
						split_pred = split_pred_info.Get();
						if(split_pred)
							container->FollowSafe(m_doc,split_pred);
						else if(split_parent->FirstChild())
							container->PrecedeSafe(m_doc,split_parent->FirstChild());
						else
							container->UnderSafe(m_doc,split_parent);
						block = GetNextListBlock(shared_elm,block);
						next_block_already_found = TRUE;
					}

					if (m_start_sel_copy.GetElement() == li_elm)
					{
						if (li_elm->Precedes(m_stop_sel_copy.GetElement()))
							m_start_sel_copy = GetTextSelectionPoint(li_elm->NextActual(), 0);
						else
						{
							m_start_sel_copy = OldStyleTextSelectionPoint();
							m_stop_sel_copy = OldStyleTextSelectionPoint();
						}
					}
					else if (m_stop_sel_copy.GetElement() == li_elm)
					{
						if (li_elm->Precedes(m_start_sel_copy.GetElement()))
						{
							HTML_Element *prev = li_elm->PrevActual();
							m_stop_sel_copy = GetTextSelectionPoint(prev, prev->Type() == HE_TEXT ? prev->GetTextContentLength() : 1);
						}
						else
						{
							m_start_sel_copy = OldStyleTextSelectionPoint();
							m_stop_sel_copy = OldStyleTextSelectionPoint();
						}
					}
				}
				DeleteElement(li_elm); // delete LI
			}
			else
			{
				// No conversion needed, just copy the block to the new parent.
				split_pred = split_pred_info.Get();
				MoveSiblingBlock(SiblingBlock(container->FirstChild(),container->LastChild()), split_parent, split_pred);
			}

			DeleteElement(container);
		}

		if(is_last_block)
			break;
		if(!next_block_already_found)
			block = GetNextListBlock(shared_elm,block);
		nestling = GetListNestling(block.start);
	}
	return OpStatus::OK;
}

void OpDocumentEdit::ToggleInsertList(BOOL ordered)
{
	DEBUG_CHECKER(TRUE);
	m_undo_stack.BeginGroup();
	HTML_ElementType block_quote[] = {HE_BLOCKQUOTE,HE_UNKNOWN};
	RemoveBlockTypes(block_quote);

	OP_STATUS status;
	SiblingBlock start, stop, block;
	INT32 nestling, max_nestling, min_nestling;
	SELECTION_ORDERING ordering = UNKNOWN;
	BOOL had_content = m_selection.HasContent();
	HTML_Element *old_caret = m_caret.GetElement();

	m_caret_elm_copy = old_caret;
	if(had_content)
	{
		start = GetBlockSiblings(m_selection.GetStartElement(TRUE));
		stop = GetBlockSiblings(m_selection.GetStopElement(TRUE));
		m_start_sel_copy = GetTextSelectionPoint(m_selection.GetStartElement(TRUE), m_selection.GetStartOfs());
		m_stop_sel_copy = GetTextSelectionPoint(m_selection.GetStopElement(TRUE), m_selection.GetStopOfs());
	}
	else
	{
		m_start_sel_copy = OldStyleTextSelectionPoint();
		m_stop_sel_copy = OldStyleTextSelectionPoint();
		start = GetBlockSiblings(m_caret.GetElement());
		stop = start;
	}

	if(!(start.start && start.stop && stop.start && stop.stop))
	{
		m_undo_stack.EndGroup();
		return;
	}

	HTML_Element* shared_containing_elm = GetSharedContainingElement(start.start, stop.stop);

	//FIXME: Should support making several lists in several table-cells
	HTML_ElementType avoid_containers[] = { HE_TABLE, HE_TBODY, HE_TR, HE_UNKNOWN };
	if(IsAnyOfTypes(shared_containing_elm,avoid_containers))
	{
		m_undo_stack.EndGroup();
		return;
	}

	if(had_content)
		m_selection.SelectNothing();
	HTML_Element *helm;

	// Adjust shared_containing_elm up to the nearest enclosing OL, UL or BODY element
	// or to the closest parent of a block-element that is a shared containing element.
	if(shared_containing_elm->Type() == m_paragraph_element_type)
		shared_containing_elm = shared_containing_elm->Parent();
	for(helm = shared_containing_elm; helm; helm = helm->ParentActual())
	{
		if(helm->Type() == HE_OL || helm->Type() == HE_UL /*|| helm->Type() == HE_BODY*/)
		{
			shared_containing_elm = helm;
			break;
		}
	}

	// Adjust start and stop up in the tree so that they are placed immediately under
	// shared_containing_elm or the nearest enclosing list element.
	if(start.start != shared_containing_elm)
	{
		if(start == stop)
		{
			while(TRUE)
			{
				HTML_Element *parent = start.start->Parent();
				if(parent == shared_containing_elm || parent->Type() == HE_OL || parent->Type() == HE_UL)
					break;
				start = GetBlockSiblings(parent);
				stop = start;
			}
		}
		else
		{
			while(TRUE)
			{
				HTML_Element *parent = start.start->Parent();
				if(parent == shared_containing_elm || parent->Type() == HE_OL || parent->Type() == HE_UL
					/*|| parent->IsAncestorOf(stop.start)*/)
				{
					break;
				}
				start = GetBlockSiblings(parent);
			}
			while(TRUE)
			{
				HTML_Element *parent = stop.start->Parent();
				if(parent == shared_containing_elm || parent->Type() == HE_OL || parent->Type() == HE_UL
					/*|| parent->IsAncestorOf(start.stop)*/)
				{
					break;
				}
				stop = GetBlockSiblings(parent);
			}
		}
	}

	// Not supposed to happen... but take care of this anyway in order to prevent crashes.
	if(!IsValidListBlock(start) || !IsValidListBlock(stop))
	{
		m_undo_stack.EndGroup();
		return;
	}

	nestling = GetListNestling(start.start);
	max_nestling = nestling;
	min_nestling = nestling;
	BOOL different_root_lists = FALSE;
	if(nestling)
		SetOrdering(ordering,GetParentListElm(start.start)->Type());

	// Iterate through (none, some or all of) the list elements in order to determine if items
	// should be inserted in a new/existing list by InsertAndOrToggleListOrdering - or if the
	// maximum list-nestling should be decreased by DecreaseMaxListNestling.
	//
	// InsertAndOrToggleListOrdering should be used if:
	//
	// 1. There are some items that are not in any list.
	// 2. All items are in lists but the list type of the most deeply nestled lists (OL/UL) they
	//    participate in is not coherent.
	// 3. All items are in lists but they are contained in different root-lists (the lists in the
	//    top of the list hierarchy)
	//
	// Otherwise should DecreaseMaxListNestling be used, the maximum list nestling is also found
	// here.
	block = start;
	HTML_Element *root_list = GetRootList(block.start);
	while(block != stop && nestling && !different_root_lists)
	{
		block = GetNextListBlock(shared_containing_elm,block);
		OP_ASSERT(!block.IsEmpty());
		if(block.IsEmpty())
		{
			m_undo_stack.EndGroup();
			return;
		}
		nestling = GetListNestling(block.start);
		max_nestling = MAX(max_nestling,nestling);
		min_nestling = MIN(min_nestling,nestling);
		if(nestling)
		{
			HTML_Element *tmp_root = GetRootList(block.start);
			if(tmp_root != root_list)
				different_root_lists = TRUE;
			SetOrdering(ordering,GetParentListElm(block.start)->Type());
		}
	}
	BOOL insert = min_nestling == 0 || (ordering != ORDERED && ordered) ||
		(ordering != UN_ORDERED && !ordered) || different_root_lists;

	// The root-element for where the changes will apply is set so it's not inside any list
	// because we might "shuffle" things around a bit later.
	HTML_Element *change_elm = shared_containing_elm;
	for(helm = change_elm; helm; helm = helm->Parent())
	{
		if(helm->Type() == HE_OL || helm->Type() == HE_UL || helm->Type() == HE_LI)
			change_elm = helm->Parent();
	}
	if(!change_elm->IsIncludedActual())
		change_elm = change_elm->ParentActual();

	// Set shared_containing_elm to it's parent if it's a list because the list might be splitted.
	if(shared_containing_elm->Type() == HE_OL || shared_containing_elm->Type() == HE_UL)
		shared_containing_elm = shared_containing_elm->Parent();

	BeginChange(change_elm);

	if(insert)
	{
		RemoveInlinePropertyBetween(start.start,stop.stop,CSS_PROPERTY_margin_left,TRUE);
		LockPendingStyles(TRUE);
		status = InsertAndOrToggleListOrdering(shared_containing_elm,start,stop,ordered);
		LockPendingStyles(FALSE);
	}
	else
		status = DecreaseMaxListNestling(shared_containing_elm,start,stop,max_nestling);
	if(OpStatus::IsError(status))
	{
		AbortChange();
		if(OpStatus::IsMemoryError(status))
			ReportOOM();
		m_undo_stack.EndGroup();
		return;
	}

	change_elm->MarkExtraDirty(m_doc); // Seems necessary, don't know why because UnderSafe and other "Safe" functions are used for manipulating the tree.
	if(m_start_sel_copy.GetElement() || old_caret != m_caret_elm_copy)
	{
		ReflowAndUpdate();
		if(m_start_sel_copy.GetElement()) // restore selection
			m_selection.Select(&m_start_sel_copy,&m_stop_sel_copy);
		if(old_caret != m_caret_elm_copy) // change caret element if the old element has been deleted.
		{
			LockPendingStyles(TRUE);
			m_caret.Set(m_caret_elm_copy, 0);
			if(m_caret_elm_copy)
			{
				m_caret.GetElement()->MarkExtraDirty(m_doc);
				m_caret.UpdatePos();
			}
			LockPendingStyles(FALSE);
		}
	}

	LockPendingStyles(TRUE);
	EndChange(change_elm,TIDY_LEVEL_MINIMAL);
	LockPendingStyles(FALSE);
	m_undo_stack.EndGroup();
}

OP_STATUS OpDocumentEdit::CreateStyleTreeCopyOf(HTML_Element *src_helm, HTML_Element *src_containing_elm, HTML_Element *&new_top_helm, HTML_Element *&new_bottom_helm, BOOL put_empty_string_at_bottom, BOOL make_empty_dummy)
{
	if (!src_helm)
		return OpStatus::ERR;
	DEBUG_CHECKER(TRUE);
	new_top_helm = NULL;
	new_bottom_helm = NULL;
	OP_ASSERT(src_helm);
	if(!src_containing_elm)
		src_containing_elm = m_doc->GetCaret()->GetContainingElementActual(src_helm);
	OP_ASSERT(src_containing_elm->IsAncestorOf(src_helm));
	if(put_empty_string_at_bottom)
	{
		HTML_Element* new_text_helm = NULL;
		if(make_empty_dummy)
			new_text_helm = NewElement(HE_BR, TRUE);
		else
			new_text_helm = NewTextElement(UNI_L(""), 0);
		if(!new_text_helm)
			return OpStatus::ERR_NO_MEMORY;
		new_top_helm = new_text_helm;
		new_bottom_helm = new_text_helm;
	}
	while(src_helm != src_containing_elm)
	{
		if(IsStyleElementType(src_helm->Type()))
		{
			HTML_Element *new_helm = NewCopyOfElement(src_helm,TRUE);
			if(!new_helm)
			{
				while(new_bottom_helm)
				{
					HTML_Element *tmp = new_bottom_helm->Parent();
					DeleteElement(new_bottom_helm);
					new_bottom_helm = tmp;
				}
				return OpStatus::ERR_NO_MEMORY;
			}
			if(!new_top_helm)
			{
				new_top_helm = new_helm;
				new_bottom_helm = new_helm;
			}
			else
			{
				new_top_helm->UnderSafe(m_doc,new_helm);
				new_top_helm = new_helm;
			}
		}
		src_helm = src_helm->ParentActual();
	}
	return OpStatus::OK;
}

void OpDocumentEdit::InsertBreak(BOOL break_list, BOOL new_paragraph)
{
	DEBUG_CHECKER(TRUE);
	ReflowAndUpdate();

	if (m_selection.HasContent())
	{
		m_selection.RemoveContent();
		m_selection.SelectNothing();
	}

	if(m_caret.GetElement() && m_layout_modifier.IsActive() && m_layout_modifier.m_helm == m_caret.GetElement())
		m_layout_modifier.Delete();

	HTML_Element* real_elm = m_caret.m_real_caret_elm.GetElm();
	if (m_caret.IsUnmodifiedTemporaryCaretTextElement() && real_elm)
	{
		m_caret.CleanTemporaryCaretTextElement(TRUE);
		SelectionBoundaryPoint sel_point(real_elm, m_caret.m_real_caret_elm_off);
		m_caret.Place(sel_point);
	}

	OP_ASSERT(m_caret.GetElement());
	if (!m_caret.GetElement())
		return; // Sanitycheck. Should not happen but sometimes *everything* was removed above.

	HTML_Element* containing_elm = m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());
	HTML_Element* tmp = containing_elm;
	if(!containing_elm)
		return;

	HTML_Element* top_editable_parent = GetTopEditableParent(containing_elm);

	// The containing element might be inside a list-element, then we want to break that list-element
	while(tmp && top_editable_parent && tmp != top_editable_parent->Parent())
	{
		if (tmp->IsMatchingType(HE_LI, NS_HTML))
		{
			containing_elm = tmp;
			break;
		}
		tmp = tmp->ParentActual();
	}
	if (containing_elm->Type() == HE_LI && break_list && !containing_elm->IsContentEditable(FALSE))
	{
		int dummy;
		HTML_Element *editable = NULL;
		GetOneStepBeside(FALSE,m_caret.GetElement(),m_caret.GetOffset(),editable,dummy);
		BOOL at_start = !containing_elm->IsAncestorOf(editable);
		GetOneStepBeside(TRUE,m_caret.GetElement(),m_caret.GetOffset(),editable,dummy);
		BOOL at_end = !containing_elm->IsAncestorOf(editable);

		if (at_start && at_end)
		{
			// Was breaking a empty HE_LI so we will remove the HE_LI and make a empty HE_P instead (or HE_LI if we're breaking a nestled list).

			HTML_Element* parent = containing_elm->ParentActual();
			void *align = containing_elm->GetAttr(ATTR_ALIGN,ITEM_TYPE_NUM,NULL);
			if(!align)
				align = parent->GetAttr(ATTR_ALIGN,ITEM_TYPE_NUM,NULL);

			// If we end up in another list we should create a HE_LI
			BOOL create_li = FALSE;
			if (parent->Type() == HE_OL || parent->Type() == HE_UL)
			{
				parent = parent->ParentActual();
				create_li = parent->Type() == HE_OL || parent->Type() == HE_UL;
			}

			OP_STATUS status = BeginChange(parent);

			OpString tmp;
			const uni_char *tag = create_li ? UNI_L("li") : HTM_Lex::GetTagString(m_paragraph_element_type);
			ReportIfOOM(tmp.AppendFormat(UNI_L("<%s>%s</%s>"), tag, document_edit_dummy_str, tag));

			HTML_Element *result_start = NULL, *result_stop = NULL;
			HTML_Element *top_helm, *bottom_helm;

			// First, let's copy the style-elements over the caret so we keep the style in the new HE_P/HE_LI.
			if (OpStatus::IsSuccess(status = CreateStyleTreeCopyOf(m_caret.GetElement(),containing_elm,
					top_helm,bottom_helm,FALSE,FALSE)))
			{
				// Split the list and insert the new element between the two parts.
				status = InsertTextHTML(tmp, uni_strlen(tmp),&result_start, &result_stop, parent);
				if(OpStatus::IsSuccess(status) && result_start)
				{
					if(result_start == result_stop && result_start->FirstChild() == result_stop->LastChild() && result_start->FirstChild())
					{
						HTML_Element *empty_str = result_start->FirstChild();
						empty_str->SetText(m_doc,document_edit_dummy_str,1);
						if(top_helm) // Insert the style-elements under the new P/LI element and above the dummy text element
						{
							empty_str->OutSafe(m_doc, FALSE);
							top_helm->UnderSafe(m_doc,result_start);
							empty_str->UnderSafe(m_doc,bottom_helm);
							m_caret.Place(empty_str,0);
						}
						if(align && !result_start->IsText())
							SetAlignAttrFromCSSValue(result_start,(CSSValue)(INTPTR)align);
					}
					else //FIXME: If some elements where inserted due to CSS in InserTextHTML, we currently just skip the style-copying.
						DeleteElement(top_helm);
				}
				else
				{
					if(OpStatus::IsSuccess(status))
						status = OpStatus::ERR;
					DeleteElement(top_helm);
				}
				parent->MarkExtraDirty(m_doc);
			}
			if(OpStatus::IsSuccess(status))
			{
				// If the lower part of the splitted list is empty - let's remove it.
				HTML_Element *after = result_stop->SucActual();
				if(after && after->IsAncestorOf(after->NextActual()))
					after = after->NextActual();
				HTML_Element *end_change = after ? after : result_stop;
				status = EndChange(parent,result_start,end_change,TRUE,TIDY_LEVEL_AGGRESSIVE);
			}
			else
				AbortChange();
		}
		else if (at_end)
		{
			// Was at the end of last element, or of element before ending BR
			// Add a new LI following the current one

			HTML_Element* parent = containing_elm->Parent();
			parent->Parent()->MarkDirty(m_doc, FALSE, TRUE); // Only for the repaint

			BeginChange(parent);

			HTML_Element* new_li = NewElement(HE_LI);
			void *align = containing_elm->GetAttr(ATTR_ALIGN,ITEM_TYPE_NUM,NULL);

			HTML_Element *top_helm,*bottom_helm;
			if(new_li && OpStatus::IsSuccess(CreateStyleTreeCopyOf(m_caret.GetElement(),containing_elm,top_helm,bottom_helm,TRUE,FALSE)))
			{
				if(align)
					new_li->SetAttr(ATTR_ALIGN,ITEM_TYPE_NUM,align);
				top_helm->UnderSafe(m_doc, new_li);
				new_li->FollowSafe(m_doc, containing_elm);
				top_helm->MarkExtraDirty(m_doc);
				m_caret.Place(bottom_helm, 0);
			}
			EndChange(parent, TIDY_LEVEL_NORMAL);
		}
		else
		{
			m_undo_stack.BeginGroup();

			// Insert a new HE_LI at the caretposition

			containing_elm->MarkExtraDirty(m_doc);
			void *align = containing_elm->GetAttr(ATTR_ALIGN,ITEM_TYPE_NUM,NULL);
			BOOL insert_before = (m_caret.GetOffset() == 0);

			OpString tmp;
			ReportIfOOM(tmp.AppendFormat(UNI_L("<li>%s</li>"), document_edit_dummy_str));

			HTML_Element* result_start;
			OP_STATUS status = InsertTextHTML(tmp, tmp.Length(), &result_start, NULL, containing_elm->Parent());
			if (OpStatus::IsMemoryError(status))
				ReportOOM();

			if (result_start)
			{
				if (result_start->IsText())
				{
					// Probably parsed into a <plaintext> node or something
					// similar. Undo what we can.
					m_undo_stack.EndGroup();
					Undo();
					m_undo_stack.Clear(FALSE, TRUE);
					return;
				}
				else
				{
					if (align)
						SetAlignAttrFromCSSValue(result_start, (CSSValue)(INTPTR)align);

					HTML_Element* helm = result_start;
					m_caret.Place(helm->NextActual(), 0); // place at dummy
					helm->NextActual()->MarkExtraDirty(m_doc);
				}
			}

			if (!insert_before)
			{
				OpInputAction action(OpInputAction::ACTION_DELETE);
				EditAction(&action);
			}

			m_undo_stack.EndGroup();
		}
	}
#ifdef DOCUMENTEDIT_SPLIT_BLOCKQUOTE
	else if (m_blockquote_split && GetTopMostParentOfType(containing_elm, HE_BLOCKQUOTE))
	{
		containing_elm = GetTopMostParentOfType(containing_elm, HE_BLOCKQUOTE);
		HTML_Element* split_root = m_doc->GetCaret()->GetContainingElementActual(containing_elm);

		OpString tmp_str;
		const uni_char *tag = HTM_Lex::GetTagString(m_paragraph_element_type);
		ReportIfOOM(tmp_str.AppendFormat(UNI_L("<%s><br></%s>"), tag, tag));

		HTML_Element *result_start = NULL, *result_stop = NULL;
		InsertTextHTML(tmp_str, tmp_str.Length(), &result_start, &result_stop, split_root);

		// Make sure we set the InsertedAutomatically flag so the br element can be removed automatically too.
		if (result_start && result_start->FirstChild() && result_start->FirstChild()->Type() == HE_BR)
			SetInsertedAutomatically(result_start->FirstChild(), TRUE);
	}
#endif // DOCUMENTEDIT_SPLIT_BLOCKQUOTE
#ifdef DOCUMENT_EDIT_USE_PARAGRAPH_BREAK
	else if (new_paragraph)
	{
		// In IE, a PRE element is split, like DIV and P instead of inserting <br> (which is only done when hitting shift+enter)
		// Just disabling the following check will make that happen in Opera to, but i'm not sure what's the expected/best behaviour.
		if(IsInPreFormatted(containing_elm))
		{
			InsertBreak(break_list, FALSE);
			return;
		}

		OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

		BOOL is_at_start_after_child_container;
		BOOL is_at_end_before_child_container;
		BOOL is_at_start = m_caret.IsAtStartOrEndOfBlock(TRUE, &is_at_start_after_child_container);
		BOOL is_at_end = m_caret.IsAtStartOrEndOfBlock(FALSE, &is_at_end_before_child_container);

		HTML_Element* split_root = m_doc->GetCaret()->GetContainingElementActual(containing_elm);
		HTML_Element* current_p_elm = NULL;

		// Check if it's a blocklevel element we should split, or create new paragraph under it using InsertBlockElementMultiple

		if (!containing_elm->IsContentEditable() &&
			// Theese elements will be split, instead of getting new P blocks inside.
			(containing_elm->Type() == HE_P ||
			containing_elm->Type() == HE_DIV ||
			containing_elm->Type() == HE_PRE ||
			IsHeader(containing_elm)
			// ...More?
			))
		{
			current_p_elm = containing_elm;
		}
		else
		{
			//OP_STATUS status = BeginChange(containing_elm);

			// Insert new paragraph element over this block
			current_p_elm = NewElement(m_paragraph_element_type);
			DoceditRef holder;
			holder.SetElm(current_p_elm);
			if (!current_p_elm || OpStatus::IsError(InsertBlockElementMultiple(current_p_elm,FALSE,TRUE)))//InsertOverBlock(current_p_elm, current_p_elm);
			{
				ReportOOM();
				DeleteElement(current_p_elm);
				return;
			}

			//status = EndChange(containing_elm, TIDY_LEVEL_NORMAL);
			split_root = containing_elm;
		}

		// Spec says that at ending position of a header, we should follow with a new p tag.
		BOOL force_p = FALSE;
		if (is_at_end && IsHeader(containing_elm))
			force_p = TRUE;

		OpString tmp_str;
		const uni_char *tag = HTM_Lex::GetTagString(m_paragraph_element_type);
		ReportIfOOM(tmp_str.AppendFormat(UNI_L("<%s><br></%s>"), tag, tag));

		if (!force_p)
		{
			// Quick and dirty way to insert a new copy of what we already have, with the same styles as we already have.
			// First extract the html as string from a copy of the element, and use that for insertion later.
			HTML_Element* tmp_element = NewCopyOfElement(current_p_elm);
			if (tmp_element)
			{
				// Remove the ID attribute
				tmp_element->RemoveAttribute(ATTR_ID);

				// Insert child text node containing dumme element for caret.
				HTML_Element *top_helm, *bottom_helm;

				if (!containing_elm->IsAncestorOf(m_caret.GetElement()))
					containing_elm = m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());

				if (OpStatus::IsSuccess(CreateStyleTreeCopyOf(m_caret.GetElement(), containing_elm,
					top_helm,bottom_helm,TRUE,TRUE)))
				{
					top_helm->Under(tmp_element);
					GetTextHTMLFromElement(tmp_str, tmp_element, TRUE);
				}
			}
			DeleteElement(tmp_element);
		}

		// Insert a new paragraph. Either inside and split, or before or after.

		if (!is_at_start && !is_at_end)
		{
			HTML_Element *result_start = NULL, *result_stop = NULL;
			InsertTextHTML(tmp_str, tmp_str.Length(), &result_start, &result_stop, split_root);

			// Make sure we set the InsertedAutomatically flag so the br element can be removed automatically too.
			if (result_start && result_start->FirstChild() && result_start->FirstChild()->Type() == HE_BR)
				SetInsertedAutomatically(result_start->FirstChild(), TRUE);

			if (IsHeader(containing_elm))
			{
				// Backspace so the new p block of this line is removed and next line moved up untouched (we just want to split headers)
				OpInputAction action(OpInputAction::ACTION_BACKSPACE);
				EditAction(&action);
				m_caret.Move(TRUE, FALSE);
			}
			else if (!is_at_end_before_child_container && !is_at_start_after_child_container)
			{
				// Delete so next block is moved up to this line
				OpInputAction action(OpInputAction::ACTION_DELETE);
				EditAction(&action);
			}
		}
		else
		{
			m_caret.Set(current_p_elm, is_at_start ? 0 : 1);

			HTML_Element *result_start = NULL, *result_stop = NULL;
			InsertTextHTML(tmp_str, tmp_str.Length(), &result_start, &result_stop, split_root);

			// Make sure we set the InsertedAutomatically flag so the br element can be removed automatically too.
			if (result_start && result_start->FirstChild() && result_start->FirstChild()->Type() == HE_BR)
				SetInsertedAutomatically(result_start->FirstChild(), TRUE);

			if (is_at_start)
				m_caret.Move(TRUE, FALSE);
			else
				m_caret.SetToValidPos();
		}
	}
#endif // DOCUMENT_EDIT_USE_PARAGRAPH_BREAK
	else
	{
		// Make sure we set the InsertedAutomatically flag to FALSE if we already have a BR element so the br element isn't removed in the process.
		if (m_caret.GetElement()->Type() == HE_BR)
			SetInsertedAutomatically(m_caret.GetElement(), FALSE);

		// If there is no ending BR and the caret is at the last element, we have to insert 2 BR. (One to end the
		// current line and one to create a new line).
		HTML_Element* containing_element = containing_elm;

		BOOL add_ending_enter = FALSE;
		if(m_caret.GetElement() == containing_element->LastLeaf() && m_caret.GetElement()->Type() != HE_BR)
		{
			int last_ofs = 0;
			GetLastValidCaretOfs(m_caret.GetElement(),last_ofs);
			if(m_caret.GetOffset() >= last_ofs)
				add_ending_enter = TRUE;
		}

		HTML_Element *new_elm = NewElement(HE_BR);
		if (!new_elm)
		{
			ReportOOM();
			return;
		}

		DUMPDEBUGTREE

		HTML_Element* sibling_element = NULL;
		BOOL is_at_start = m_caret.IsAtStartOrEndOfBlock(TRUE);
		BOOL is_at_end = m_caret.IsAtStartOrEndOfBlock(FALSE);
		if(is_at_end && is_at_start)
			add_ending_enter = FALSE;

		if (is_at_end || is_at_start)
		{
#ifndef DOCUMENT_EDIT_USE_PARAGRAPH_BREAK
			// Let shift+break always insert <br> (even in headers) when we have paragraphbreak as default
			if (IsHeader(containing_element))
				sibling_element = containing_element;
			else
#endif
			sibling_element = m_caret.GetElement()->ParentActual();
			while (sibling_element && sibling_element->Type() != HE_A)
				sibling_element = sibling_element->ParentActual();
		}

		BOOL finished = FALSE;
		if (sibling_element && m_undo_stack.IsValidAsChangeElm(sibling_element->ParentActual()))
		{
			// Create a new line after the header, instead of inside it.
			HTML_Element* parent = sibling_element->ParentActual();
			BeginChange(parent);

			if (is_at_end)
				new_elm->FollowSafe(m_doc, sibling_element);
			else
				new_elm->PrecedeSafe(m_doc, sibling_element);
			if(!m_caret.IsElementEditable(new_elm))
			{
				new_elm->OutSafe(m_doc, FALSE);
				AbortChange();
			}
			else
			{
				m_caret.Set(new_elm, 0);

				if (is_at_end && IsFriendlyElement(sibling_element))
					InsertBreak(break_list);

				EndChange(parent);
				finished = TRUE;
			}
		}
		if(!finished)
		{
			HTML_Element* parent = m_caret.GetElement()->ParentActual();
			BeginChange(parent);

			InsertElement(new_elm);

			if (add_ending_enter)
			{
				m_caret.Set(new_elm, 0);
				InsertBreak(break_list);
			}

			EndChange(parent);
		}

		DUMPDEBUGTREE

		m_caret.RestartBlink();
	}
}

OP_STATUS OpDocumentEdit::ClonePendingStyles(Head &head)
{
	OP_DOCUMENT_EDIT_PENDING_STYLES *pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First();
	while (pending_style)
	{
		OP_DOCUMENT_EDIT_PENDING_STYLES *clone_style = OP_NEW(OP_DOCUMENT_EDIT_PENDING_STYLES, ());
		HTML_Element *clone_helm = NewElement(pending_style->helm->Type());
		if (clone_style && clone_helm)
		{
			clone_style->helm = clone_helm;
			clone_style->Into(&head);
		}
		else
		{
			OP_DELETE(clone_style);
			OP_DELETE(clone_helm);
			return OpStatus::ERR_NO_MEMORY;
		}
		pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES *) pending_style->Suc();
	}
	return OpStatus::OK;
}

void OpDocumentEdit::AddPendingStyles(Head &head)
{
	OP_DOCUMENT_EDIT_PENDING_STYLES *pending_style;
	while ((pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) head.First()) != 0)
	{
		pending_style->Out();
		pending_style->Into(&m_pending_styles);
	}
}

OP_STATUS OpDocumentEdit::InsertPendingStyles(HTML_Element** outmost_element)
{
	if (m_pending_styles.First())
	{
		// New text element under the new styles in m_pending_styles
		HTML_Element *new_text_content = NewTextElement(UNI_L(""), 0);
		if(!new_text_content)
			return OpStatus::ERR_NO_MEMORY;

		// So the tidy operation knows we are adding content soon.
		m_content_pending_helm = new_text_content;

		// We have pending style elements. Insert them all under each other, with the caret at the bottom.
		// Then proceed with the textinsertion.
		m_caret.LockUpdatePos(TRUE);

		OP_DOCUMENT_EDIT_PENDING_STYLES* pending_style;
		Head temp_list;

		while ((pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) m_pending_styles.First()) != 0)
		{
			// Move to a temporary list. m_pending_styles will be cleared in InsertElement.
			pending_style->Out();
			pending_style->Into(&temp_list);
		}

		HTML_Element *top_style = ((OP_DOCUMENT_EDIT_PENDING_STYLES*)(temp_list.First()))->helm;
		HTML_Element *bottom_style = top_style;

		if (outmost_element != NULL)
			*outmost_element = top_style;

		pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*)temp_list.First();
		pending_style->Out();
		OP_DELETE(pending_style);
		while ((pending_style = (OP_DOCUMENT_EDIT_PENDING_STYLES*) temp_list.First()) != 0)
		{
			pending_style->helm->UnderSafe(m_doc,bottom_style);
			bottom_style = pending_style->helm;
			pending_style->Out();
			OP_DELETE(pending_style);
		}
		new_text_content->UnderSafe(m_doc,bottom_style);
		InsertElement(top_style);
		m_caret.Set(new_text_content, 0);
		m_caret.LockUpdatePos(FALSE);

		// We can now unset m_content_pending_helm since we will add the text now.
		m_content_pending_helm = NULL;
	}
	else if (outmost_element != NULL)
		*outmost_element = NULL;

	return OpStatus::OK;
}

void OpDocumentEdit::InsertElement(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* parent = m_caret.m_parent_candidate;
	if (!parent)
	{
		parent = m_caret.GetElement();
		if (parent && IsStandaloneElement(parent))
			parent = parent->ParentActual();

		if (!parent)
			return;
	}

	BeginChange(parent);

	if (m_caret.m_parent_candidate)
	{
		if (IsStandaloneElement(m_caret.m_parent_candidate))
			helm->FollowSafe(m_doc, m_caret.m_parent_candidate);
		else
			helm->UnderSafe(m_doc, m_caret.m_parent_candidate);
	}
	else
	{
		// If the caret is inside a textelement we have to split the textelement in 2.
		SplitElement(m_caret.GetElement(), m_caret.GetOffset());

		// Insert helm
		if (IsStandaloneElement(m_caret.GetElement()))
		{
			if (m_caret.GetOffset() == 0)
				helm->PrecedeSafe(m_doc, m_caret.GetElement());
			else
				helm->FollowSafe(m_doc, m_caret.GetElement());
		}
		else
		{
			int offset = m_caret.GetOffset();
			HTML_Element* follower = parent->FirstChildActual();
			while (offset > 0 && follower)
			{
				follower = follower->SucActual();
				offset--;
			}
			if (follower)
				helm->PrecedeSafe(m_doc, follower);
			else
				helm->UnderSafe(m_doc, parent);
		}
	}

	ReflowAndUpdate();

	//int old_caret_ofs = m_caret.GetOffset();
	HTML_Element* old_caret_elm = m_caret.GetElement();

	// Insertion of the new element happened after the caret so we'll need to move the caret.
	// We can't do it with Move if the caret is currently at a <br> since Move() will
	// sometimes move the caret too far in that case. Otherwise we'll use Move().
	if (!(m_caret.GetOffset() == 0 && helm->Type() == HE_BR))
	{
		m_caret.Move(TRUE, FALSE);
		// Make sure we didn't moved outside the parent (WHY?????????????)
		//if (!parent->IsAncestorOf(m_caret.GetElement()) || m_caret.GetElement() == old_caret_elm && m_caret.GetOffset() == old_caret_ofs)
		//{
		//	m_caret.Set(old_caret_elm, old_caret_ofs);
		//	m_caret.Move(FALSE,TRUE);
		//	if (!parent->IsAncestorOf(m_caret.GetElement()))
		//		m_caret.Set(old_caret_elm, old_caret_ofs);
		//}
	}
	else if (helm->IsMatchingType(HE_BR, NS_HTML) && helm->NextActual())
	{
		// Instead of moving the caret, just put it where we want it. There is a risk that it will end up at an
		// invisible position but this is less of a problem than that the caret doesn't move at all.
		OldStyleTextSelectionPoint point(helm, 1);
		m_selection.Select(&point, &point, TRUE); // Put the caret after the newly inserted break.
	}

	// If the old caretelement was a empty textelement, we can now delete it.
	if (old_caret_elm != m_caret.GetElement() && old_caret_elm->Type() == HE_TEXT && old_caret_elm->GetTextContentLength() == 0)
		DeleteElement(old_caret_elm);

	EndChange(parent);
}

OP_STATUS OpDocumentEdit::InsertTextWithLinebreaks(const uni_char* text, INT32 len, BOOL allow_append)
{
	DEBUG_CHECKER(TRUE);
	OP_STATUS status = OpStatus::OK;

	if(!m_caret.GetElement())
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

	HTML_Element* parent = m_caret.GetElement()->ParentActual();

	if (!(IsDummyElement(m_caret.GetElement()) || m_caret.GetElement()->GetTextContentLength() == 0))
		// Cause the current textelement to be split up, so our new elements and linebreaks can follow.
		status = InsertTextHTML(document_edit_dummy_str, 1, NULL, NULL, parent);

	status = BeginChange(parent);

	HTML_Element *follow_elm = m_caret.GetElement();

	int ofs = 0;
	while(TRUE)
	{
		int line_start = ofs;
		while(ofs < len && text[ofs] != '\r' && text[ofs] != '\n')
			ofs++;
		BOOL first_line = (line_start == 0);
		BOOL last_line = (ofs >= len);

		int line_len = ofs - line_start;
		if (ofs < len - 1 && text[ofs] == '\r' && text[ofs + 1] == '\n')
			ofs++;
		ofs++;

		if (line_len > 0)
		{
			// Insert text element.
			if (first_line && m_caret.GetElement() && m_caret.GetElement()->Type() == HE_TEXT)
			{
				SetElementText(m_caret.GetElement(), &text[line_start], line_len);
				follow_elm = m_caret.GetElement();
			}
			else
			{
				HTML_Element *text_elm = NewTextElement(&text[line_start], line_len);
				if (!text_elm)
					break;

				/* If the caret is to the left of a non-text element then the
				 * text we're inserting should precede said element. */
				if (first_line && m_caret.GetOffset() == 0)
					text_elm->PrecedeSafe(m_doc, follow_elm);
				else
					text_elm->FollowSafe(m_doc, follow_elm);

				follow_elm = text_elm;
			}
		}
		if (last_line)
			break;

		// Insert linebreak
		HTML_Element *br_elm = NewElement(HE_BR);
		if (!br_elm)
			break;
		br_elm->FollowSafe(m_doc, follow_elm);
		follow_elm = br_elm;
	}

	if (follow_elm->IsMatchingType(HE_BR, NS_HTML))
	{
		OldStyleTextSelectionPoint point(follow_elm, 1);
		m_selection.Select(&point, &point, TRUE); // Put the caret after the newly inserted break.
	}
	else
		m_caret.Place(follow_elm, follow_elm->GetTextContentLength(), FALSE, FALSE);
	m_caret.UpdateWantedX();

	status = EndChange(parent);

	return status;
}

BOOL OpDocumentEdit::IsBeforeCollapsedOrEdge(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *before = helm;
	do
	{
		before = before->PrevActual();
		if (!before || !IsFriends(helm, before, TRUE, TRUE) || IsCollapsed(before))
			return TRUE;
	} while(before->Type() != HE_TEXT || before->GetTextContentLength() == 0);
	if(CharMightCollapse(before->TextContent()[before->GetTextContentLength()-1]))
		return TRUE;
	return FALSE;
}

BOOL OpDocumentEdit::IsAfterCollapsedOrEdge(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *after = helm;
	do
	{
		after = after->NextActual();
		if (!after || !IsFriends(helm, after, TRUE, TRUE) || IsCollapsed(after))
			return TRUE;
	} while(after->Type() != HE_TEXT || after->GetTextContentLength() == 0);
	if(CharMightCollapse(after->TextContent()[0]))
		return TRUE;
	return FALSE;
}

BOOL OpDocumentEdit::RemoveNBSPIfPossible(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || helm->Type() != HE_TEXT || !helm->GetTextContentLength())
		return FALSE;

	OpString str;
	if(OpStatus::IsError(str.Set(helm->TextContent())))
	{
		ReportOOM();
		return FALSE;
	}
	int i;
	int len = helm->GetTextContentLength();
	BOOL changed = FALSE;
	if (len > 0 && !IsInPreFormatted(helm))
	{
		// We might need a nbsp at the beginning or end
		if (uni_collapsing_sp(str.CStr()[0]))
		{
			str.CStr()[0] = 0xA0;
			changed = TRUE;
		}
		if (uni_collapsing_sp(str.CStr()[len - 1]))
		{
			str.CStr()[len - 1] = 0xA0;
			changed = TRUE;
		}
	}
	for(i=0;i<len;i++)
	{
		if(str.CStr()[i] != 0xA0)
			continue;
		if((!i && IsBeforeCollapsedOrEdge(helm)) || (i == len-1 && IsAfterCollapsedOrEdge(helm)))
			continue;
		if((!i || !uni_collapsing_sp(str.CStr()[i-1])) && !(i < len-1 && uni_collapsing_sp(str.CStr()[i+1])))
		{
			str.CStr()[i] = ' ';
			changed = TRUE;
		}
	}
	if(changed)
		SetElementText(helm,str.CStr());
	return changed;
}

/*void UnCollapseLastCollapsing(uni_char *buf)
{
	while(uni_collapsing_sp(buf[1])) buf++;
	*buf = 0xA0;
}*/

BOOL OpDocumentEdit::IsInPreFormatted(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return FALSE;

	Head prop_list;
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	LayoutProperties* lprops = LayoutProperties::CreateCascade(helm, prop_list, hld_profile);
	BOOL in_pre = lprops && (lprops->GetProps()->white_space == CSS_VALUE_pre || lprops->GetProps()->white_space == CSS_VALUE_pre_wrap);
	prop_list.Clear();
	return in_pre;
}

OP_STATUS OpDocumentEdit::InsertText(const uni_char *text, INT32 len, BOOL allow_append, BOOL delete_selection)
{
	DEBUG_CHECKER(TRUE);
	if(len <= 0)
		return OpStatus::OK;

	OP_STATUS status;
	if(!m_caret.GetElement())
	{
		LockPendingStyles(TRUE);
		m_caret.Init(TRUE);
		LockPendingStyles(FALSE);
		if(!m_caret.GetElement())
		{
			OP_ASSERT(FALSE);
			return OpStatus::ERR;
		}
	}
	OpDocumentEditWsPreserver preserver(this);
	OpDocumentEditWsPreserverContainer preserver_container(&preserver, this);
	OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

	m_caret.LockUpdatePos(TRUE);
	BOOL had_selection = m_selection.HasContent();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	SpellWordInfoObject old_spell_word_info;
	if(m_spell_session && !had_selection)
		old_spell_word_info.Set(m_caret.GetElement());
#endif
	if (delete_selection)
		DeleteSelectedContent();

	if (ContainsLinebreak(text, len))
	{
		status = InsertTextWithLinebreaks(text, len, allow_append);
		m_caret.LockUpdatePos(FALSE);
		return status;
	}

	if (m_caret.GetElement() && m_caret.GetElement()->Type() == HE_TEXT && m_caret.GetElement()->ParentActual() && m_caret.GetElement()->ParentActual()->Type() == HE_A)
	{
		// If we stand at the end of a link, we should insert the text after the link instead of inside it.
		// Otherwise it would be very hard for the user to continue writing after making a link.
		int last_valid_ofs = 0;
		if (GetLastValidCaretOfs(m_caret.GetElement(), last_valid_ofs) && m_caret.GetOffset() == last_valid_ofs)
		{
			// Check if we have text following the link. If we do, we do nothing.
			HTML_Element *containing_element = m_doc->GetCaret()->GetContainingElement(m_caret.GetElement());
			HTML_Element *next_helm = NULL;
			int next_ofs = 0;
			BOOL last = GetBestCaretPosFrom(m_caret.GetElement(), next_helm, next_ofs);
			if (!last || !containing_element->IsAncestorOf(next_helm) || next_helm->Type() == HE_BR ||
				// If the best pos precedes the caret, the following position wasn't as good so that must also mean we're at 'the end' of something.
				next_helm->Precedes(m_caret.GetElement()))
			{
				HTML_Element *candidate = m_caret.GetElement();
				HTML_Element *containing_element = m_doc->GetCaret()->GetContainingElement(m_caret.GetElement());
				while (candidate && candidate->ParentActual() != containing_element && candidate->ParentActual() && candidate->ParentActual()->LastChild() == candidate)
					candidate = candidate->ParentActual();
				if (candidate && candidate != m_caret.GetElement())
					m_caret.Set(candidate, 1);
			}
		}
	}

	int flags = (allow_append) ? CHANGE_FLAGS_ALLOW_APPEND : CHANGE_FLAGS_NONE;

	status = InsertPendingStyles();
	if (OpStatus::IsError(status))
	{
		m_caret.LockUpdatePos(FALSE);
		return status;
	}

	if (m_caret.m_parent_candidate)
	{
		// We have a parent candidate so try to reinitialize the caret there.
		// This might happen if something tried to place the caret in a empty element (via DOM).
		// Init call will create a textelement if needed.
		m_caret.Init(TRUE, m_caret.m_parent_candidate);
	}

	if (!m_caret.GetElement())
		return OpStatus::ERR;

	if (m_caret.GetElement()->Type() != HE_TEXT)
	{
		// We are trying to insert text in something else than a textelement. Create a empty textelement first.

		HTML_Element* root = m_caret.GetElement()->ParentActual();
		if (!root) // Trying to insert something in the very root.
			return OpStatus::ERR;
		BeginChange(root, flags);

		HTML_Element* new_elm = NewTextElement(document_edit_dummy_str, 1);
		if (!new_elm)
		{
			AbortChange();
			m_caret.LockUpdatePos(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}

		// So the tidy operation knows we are adding content soon.
		m_content_pending_helm = new_elm;

		if (m_caret.IsElementEditable(root))
		{
			if (m_caret.GetOffset() == 1)
				new_elm->FollowSafe(m_doc, m_caret.GetElement());
			else
				new_elm->PrecedeSafe(m_doc, m_caret.GetElement());
		}
		else
			new_elm->UnderSafe(m_doc, m_caret.GetElement());
		new_elm->MarkExtraDirty(m_doc);
		m_caret.Set(new_elm, 0);

		EndChange(root);
	}
	else
	{
		// Since the text insertion is not a deep change, it won't tidy away automatic BR.
		// If we have one, and need to tidy it away, we will do a deep change here to remove it.
		HTML_Element *containing_element = m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());
		if (containing_element && HasAutoInsertedBR(containing_element) && !NeedAutoInsertedBR(containing_element))
		{
			// So the tidy operation knows we are adding content soon.
			m_content_pending_helm = m_caret.GetElement();

			BeginChange(containing_element, flags);
			EndChange(containing_element);
		}
	}

	HTML_Element* root = m_caret.GetElement();
	status = BeginChange(root, flags);

	// We can now unset m_content_pending_helm since we will add the text now.
	m_content_pending_helm = NULL;

	if (IsDummyElement(m_caret.GetElement()))
	{
		// This was a dummyelement. Delete the content before adding the real content.
		SetElementText(m_caret.GetElement(), UNI_L(""));
	}

	// Take out the text from the element, add the text we should insert and set it back to the element.

	HTML_Element *he = m_caret.GetElement();
	int pos;
	OpString str;
	if (OpStatus::IsError(str.Set(he->TextContent())))
		goto error;

	pos = MIN(str.Length(), m_caret.GetOffset());
	if(!had_selection)
		preserver.SetRemoveRange(he,he,pos,pos);
	status = str.Insert(pos, text, len);
	if (OpStatus::IsError(status))
		goto error;

	if(!IsInPreFormatted(he))
	{
		// If we are not in pre-formatted layout, we should convert tabs and starting and ending whitespace into nonbreaking space.
		for(int i = 0; i < len; i++)
		{
			if (str.CStr()[pos + i] == '\t')
			{
				str.CStr()[pos + i] = 0xA0;
				str.Insert(pos + i, "\xA0\xA0\xA0", 3);
				len += 3;
				i += 3;
			}
		}
		if(uni_collapsing_sp(str.CStr()[pos]) || uni_collapsing_sp(str.CStr()[pos+len-1]))
		{
			if(uni_collapsing_sp(str.CStr()[pos]))
				str.CStr()[pos] = 0xA0;
			if(uni_collapsing_sp(str.CStr()[pos+len-1]))
				str.CStr()[pos+len-1] = 0xA0;
		}
	}

	SetElementText(he, str);

	if(!had_selection)
		preserver.WsPreserve();
	RemoveNBSPIfPossible(he);

	// Update

	m_caret.LockUpdatePos(FALSE);
	m_caret.Place(m_caret.GetElement(), m_caret.GetOffset() + len, FALSE, TRUE);
	m_caret.UpdateWantedX();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DoSpellWordInfoUpdate(&old_spell_word_info);
#endif // INTERNAL_SPELLCHECK_SUPPORT
	return EndChange(root);

error:
	AbortChange();
	m_caret.LockUpdatePos(FALSE);
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpDocumentEdit::InsertPlainText(const uni_char *text, INT32 len)
{
	DEBUG_CHECKER(TRUE);

	DeleteSelectedContent();

	if(len <= 0)
		return OpStatus::OK;

	if(!m_caret.GetElement())
	{
		// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
		return OpStatus::ERR;
	}

	m_caret.LockUpdatePos(TRUE);
	OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

	if (m_caret.m_parent_candidate && m_caret.IsElementEditable(m_caret.m_parent_candidate))
	{
		// We have a parent candidate so try to reinitialize the caret there.
		// This might happen if something tried to place the caret in a empty element (via DOM).
		// Init call will create a textelement if needed.
		m_caret.Init(TRUE, m_caret.m_parent_candidate);
	}

	OP_STATUS status;
	if (m_caret.GetElement()->Type() != HE_TEXT)
	{

		// We are trying to insert text in something else than a textelement. Create a empty textelement first.
		HTML_Element* root = m_caret.GetElement()->ParentActual();
		status = BeginChange(root);

		HTML_Element* new_elm = NewTextElement(document_edit_dummy_str, 1);
		if (!new_elm)
		{
			AbortChange();
			m_caret.LockUpdatePos(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (m_caret.IsElementEditable(root))
		{
			if (m_caret.GetOffset() == 1)
				new_elm->FollowSafe(m_doc, m_caret.GetElement());
			else
				new_elm->PrecedeSafe(m_doc, m_caret.GetElement());
		}
		else
			new_elm->UnderSafe(m_doc, m_caret.GetElement());
		new_elm->MarkExtraDirty(m_doc);
		m_caret.Set(new_elm, 0);

		status = EndChange(root);
	}

	HTML_Element* root = m_caret.GetElement();
	status = BeginChange(root);

	if (IsDummyElement(m_caret.GetElement()))
	{
		// This was a dummyelement. Delete the content before adding the real content.
		SetElementText(m_caret.GetElement(), UNI_L(""));
	}

	// Take out the text from the element, add the text we should insert and set it back to the element.
	HTML_Element *he = m_caret.GetElement();
	int pos;
	OpString str;
	if (OpStatus::IsError(str.Set(he->TextContent())))
		goto error;

	pos = MIN(str.Length(), m_caret.GetOffset());
	status = str.Insert(pos, text, len);
	if (OpStatus::IsError(status))
		goto error;

	SetElementText(he, str);

	m_caret.LockUpdatePos(FALSE);
	m_caret.Place(m_caret.GetElement(), m_caret.GetOffset() + len, TRUE, FALSE);
	m_caret.UpdateWantedX();

	return EndChange(root);

error:
	AbortChange();
	m_caret.LockUpdatePos(FALSE);
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpDocumentEdit::InsertTextHTML(const uni_char* text, INT32 len, HTML_Element** result_start, HTML_Element** result_stop, HTML_Element* split_root, TIDY_LEVEL tidy_level, BOOL delete_selection)
{
	DEBUG_CHECKER(TRUE);

	OpDocumentEditUndoRedoAutoGroup autogroup(&m_undo_stack);

	if (delete_selection)
		DeleteSelectedContent();

	if (!text || !len)
		return OpStatus::OK;

	if(!m_caret.GetElement())
	{
		// Code is unfortunately dependant on m_caret.m_helm. Will be fixed in the next redesign/rewrite.
		return OpStatus::ERR;
	}

	// Must copy text so we are sure it's not a const string. (SetInnerHTML can do nasty things with the data).
	OpString temp_text;
	RETURN_IF_ERROR(temp_text.Set(text, len));

	if (!temp_text.Length())
		return OpStatus::OK;

	// Create temp root for the parsed text. then add all content after the splitted caretelement.
	HTML_Element* tmproot = NewElement(HE_DOC_ROOT);
	if (!tmproot)
		return OpStatus::ERR_NO_MEMORY;

	// We'll use the parent of the caret as context
	OP_STATUS status = tmproot->SetInnerHTML(m_doc, temp_text, temp_text.Length(), m_caret.GetElement()->ParentActual());
	if (OpStatus::IsError(status))
	{
		DeleteElement(tmproot);
		return status;
	}

#ifdef DEBUG_DOCUMENT_EDIT
	DUMPDEBUGTREE_ELM(tmproot);
#endif // DEBUG_DOCUMENT_EDIT

	Tidy(tmproot, tmproot, FALSE, tidy_level, TRUE);

	// If there was a body, use the body as root.
	HTML_Element* resultroot = FindElementAfterOfType(tmproot, HE_BODY);
	if (!resultroot)
		resultroot = tmproot;

	// Find out where we are going to place the caret when it's done. (The last editable element)
	HTML_Element* last_text_helm;
	for(last_text_helm = resultroot->LastLeafActual();
		last_text_helm && !IsElementValidForCaret(last_text_helm, FALSE, TRUE);
		last_text_helm = last_text_helm->PrevActual()) {}

	if (m_caret.m_parent_candidate)
	{
		// We have a parent candidate so try to reinitialize the caret there.
		// This might happen if something tried to place the caret in a empty element (via DOM).
		// Init call will create a textelement if needed.
		m_caret.Init(TRUE, m_caret.m_parent_candidate);
	}

	// FIX: See if it only contains friendly elements. In that case we always want to use containing element as root
	// even if split_root is something else.

	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	HTML_Element* current_root_helm = split_root ? split_root : m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());

/*	BOOL friendly = TRUE;
	HTML_Element* tmp = resultroot->FirstChild();
	while(tmp && resultroot->IsAnchestorOf(tmp))
	{
		if (tmp->IsContainingElement())
		{
			friendly = FALSE;
			break;
		}
		tmp = tmp->Next();
	}
	if (friendly)
		current_root_helm = GetContainingElementActual(m_caret.GetElement());*/

	// Insert it

	status = BeginChange(current_root_helm); // FIXME: OOM

	OP_ASSERT(m_caret.GetElement() || !"If it happens, you probably have a bug. Modifications to the tree of elements above should not change the caret");
	if (m_caret.GetElement()) ///< Check should not be needed but better safe than sorry
		SplitElement(m_caret.GetElement(), m_caret.GetOffset());

	HTML_Element* current_top_elm = m_caret.GetElement();
	while(current_top_elm && current_top_elm->ParentActual() != current_root_helm)
		current_top_elm = current_top_elm->ParentActual();

	OP_ASSERT(current_top_elm);
	if (!current_top_elm)
	{
		DeleteElement(tmproot);
		EndChange(current_root_helm);
		return OpStatus::ERR;
	}

	if (result_start)
		*result_start = resultroot->FirstChild();
	if (result_stop)
		*result_stop = resultroot->LastChild();

	DUMPDEBUGTREE

	// Add all parseresult after current_top_elm (or before if caret is at toplevel at position 0).
	HTML_Element* tmp = resultroot->FirstChild();
	if (!tmp)
	{
		DeleteElement(tmproot);
		return EndChange(current_root_helm, tidy_level);
	}

	tmp->OutSafe(static_cast<FramesDocument*>(NULL), FALSE);
	if (current_top_elm == m_caret.GetElement() && m_caret.GetOffset() == 0)
		tmp->PrecedeSafe(m_doc, current_top_elm);
	else if (current_top_elm->Type() == HE_TEXT || current_top_elm->Type() == HE_BR)
		tmp->FollowSafe(m_doc, current_top_elm);
	else
	{
//		tmp->UnderSafe(current_top_elm);
		tmp->FollowSafe(m_doc, current_top_elm);
	}
	tmp->MarkExtraDirty(m_doc);

//	HTML_Element* first_inserted_elm = tmp;
	HTML_Element* following_elm = tmp;
	while(resultroot->FirstChild())
	{
		tmp = resultroot->FirstChild();
		tmp->OutSafe(static_cast<FramesDocument*>(NULL), FALSE);
		tmp->FollowSafe(m_doc, following_elm);
		tmp->MarkExtraDirty(m_doc);
		following_elm = tmp;
	}

	DUMPDEBUGTREE

	if (m_caret.GetElement() && current_top_elm != m_caret.GetElement())
	{
		// We splitted a styled textelement.
		// Make a copy of the treebranch that contains the splitted text. Remove the last part in the original and the
		// first part of the copy. Then add the copy after the inserted text.

		HTML_Element* copytree = NewCopyOfElement(current_top_elm);
		if (!copytree || OpStatus::IsError(copytree->DeepClone(hld_profile, current_top_elm)))
		{
			AbortChange();
			DeleteElement(tmproot);
			DeleteElement(copytree);
			return OpStatus::ERR_NO_MEMORY;
		}

		HTML_Element* first_elm_after_split = ((m_caret.GetOffset() == 0 && m_caret.GetElement()->IsIncludedActual()) ? m_caret.GetElement() : m_caret.GetElement()->NextSiblingActual());
		HTML_Element* first_elm_after_split_in_copy = NULL;

		// Find the first_elm_after_split_in_copy
		tmp = current_top_elm;
		first_elm_after_split_in_copy = copytree;
//			while(tmp != first_elm_after_split && current_top_elm->IsAncestorOf(tmp))
		while(tmp != first_elm_after_split && first_elm_after_split_in_copy)
		{
			tmp = (HTML_Element*) tmp->NextActual();
			first_elm_after_split_in_copy = (HTML_Element*) first_elm_after_split_in_copy->NextActual();
		}

		// Remove everything after caretpos in the original tree.
		tmp = first_elm_after_split;
		HTML_Element *prev_first_elm_after_split = first_elm_after_split->PrevSiblingActual();
		while(tmp && current_top_elm->IsAncestorOf(tmp))//tmp->ParentActual() != current_root_helm)
		{
			HTML_Element* next_elm = (HTML_Element*) tmp->NextActual();
			if (!tmp->IsAncestorOf(prev_first_elm_after_split))
			{
				next_elm = tmp->NextSiblingActual();
				DeleteElement(tmp);
				if(next_elm == current_root_helm || !current_root_helm->IsAncestorOf(next_elm))
					break;
			}
			tmp = next_elm;
		}

		tmp = copytree->FirstChild();
		while(tmp && tmp != first_elm_after_split_in_copy)
		{
			HTML_Element* next_elm = (HTML_Element*) tmp->NextActual();
			if (!tmp->IsAncestorOf(first_elm_after_split_in_copy))
			{
				next_elm = tmp->NextSiblingActual();
				DeleteElement(tmp);
			}
			tmp = next_elm;
		}
/*#ifdef DEBUG_DOCUMENT_EDIT
		copytree->DumpDebugTree();
#endif*/
		// Insert copytree after the new inserted text.
		if(copytree->FirstChild())
		{
			copytree->FollowSafe(m_doc, following_elm);
			copytree->MarkExtraDirty(m_doc);
		}
		else
			DeleteElement(copytree);
	}

	current_root_helm->MarkDirty(m_doc, FALSE, TRUE);

	// Find out where to put the caret.
	/*HTML_Element* last_inserted_helm = following_elm->LastLeaf();
	HTML_Element* last_text_helm = FindEditableElement(first_inserted_elm, TRUE, TRUE, FALSE);
	while(last_text_helm && last_text_helm->Precedes(last_inserted_helm))
	{
		HTML_Element* next = FindEditableElement(last_text_helm, TRUE, FALSE, FALSE);
		if (next && !last_inserted_helm->Precedes(next))
			last_text_helm = next;
		else
			break;
	}
	if (last_inserted_helm->Precedes(last_text_helm))
		last_text_helm = NULL;

	if (last_text_helm)
		m_caret.Place(last_text_helm, last_text_helm->GetTextContentLength());
	else if (following_elm->SucActual())
	{
		last_text_helm = following_elm->SucActual();
		m_caret.Place(last_text_helm, 0);
	}
	else
	{
		last_text_helm = following_elm->LastLeaf();
		m_caret.Place(last_text_helm, last_text_helm->GetTextContentLength());
	}*/

	// Place the caret
	if (last_text_helm)
	{
		int text_helm_ofs = 0;
		if (IsEndingBr(last_text_helm) && last_text_helm->NextActual())
		{
			// Don't place the caret on a ending br (it isn't layouted anyway)
			// Try put it after it.
			HTML_Element *next_actual = last_text_helm->NextActual();
			if (next_actual->Type() == HE_TEXT || next_actual->Type() == HE_BR)
				last_text_helm = next_actual;

			GetFirstValidCaretOfs(last_text_helm, text_helm_ofs);
		}
		else
			GetLastValidCaretOfs(last_text_helm, text_helm_ofs);
		m_caret.Place(last_text_helm, text_helm_ofs);
	}
	else if (!m_caret.GetElement())
	{
		// The caret element was removed above. Create a dummyelement so the undostack won't freak out.
		HTML_Element *text_elm = NewTextElement(document_edit_dummy_str, 1);
		text_elm->FollowSafe(m_doc, current_top_elm);
		text_elm->MarkExtraDirty(m_doc);
		m_caret.Place(text_elm, 0);
	}

	DeleteElement(tmproot);

	return EndChange(current_root_helm, tidy_level);
}

void OpDocumentEdit::DeleteSelectedContent(BOOL aggressive)
{
	DEBUG_CHECKER(TRUE);
	if (m_layout_modifier.IsActive())
	{
		m_layout_modifier.Delete();
	}
	if (m_selection.HasContent())
	{
		m_selection.RemoveContent(aggressive);
		m_selection.SelectNothing();
	}
}

void OpDocumentEdit::Begin()
{
	DEBUG_CHECKER(TRUE);
	m_caret.LockUpdatePos(TRUE);
	m_begin_count++;
}

void OpDocumentEdit::End()
{
	DEBUG_CHECKER(TRUE);
	m_begin_count--;
	m_caret.LockUpdatePos(FALSE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(!m_begin_count)
		RunPendingSpellCheck();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpDocumentEdit::Abort()
{
	DEBUG_CHECKER(TRUE);
	m_begin_count--;
	m_caret.LockUpdatePos(FALSE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(!m_begin_count)
		RunPendingSpellCheck();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

OP_STATUS OpDocumentEdit::BeginChange(HTML_Element* containing_elm, int flags)
{
	DEBUG_CHECKER(FALSE);
	Begin();
	return m_undo_stack.BeginChange(containing_elm, flags);
}

OP_STATUS OpDocumentEdit::EndChange(HTML_Element* containing_elm, TIDY_LEVEL tidy_level)
{
	DEBUG_CHECKER(FALSE);
	return EndChange(containing_elm,containing_elm,containing_elm,FALSE,tidy_level);
}

OP_STATUS OpDocumentEdit::EndChange(HTML_Element* containing_elm, HTML_Element *start_elm, HTML_Element *stop_elm, BOOL include_start_stop, TIDY_LEVEL tidy_level)
{
	DEBUG_CHECKER(FALSE);
	Tidy(start_elm, stop_elm, include_start_stop, tidy_level, FALSE, containing_elm);

	ReflowAndUpdate();

	OP_STATUS ret = m_undo_stack.EndChange(containing_elm);

	End();

	// Something changed, so we will have to call the direction autodetection code now, if we are the last EndChange.
	if (m_begin_count == 0)
		AutodetectDirection();

	DUMPDEBUGTREE
	return ret;
}

void OpDocumentEdit::AbortChange()
{

	DEBUG_CHECKER(FALSE);
	m_undo_stack.AbortChange();
	Abort();
}

void OpDocumentEdit::ReflowAndUpdate()
{
	DEBUG_CHECKER(TRUE);
	if (!GetRoot() || GetRoot()->IsDirty())
		if (!m_doc->IsReflowing() && !m_doc->IsUndisplaying())
			m_doc->Reflow(FALSE, TRUE);
}

void OpDocumentEdit::OnScaleChanged()
{
	DEBUG_CHECKER(TRUE);
	m_caret.UpdatePos();
	m_caret.UpdateWantedX();
	ScrollIfNeeded();
}

void OpDocumentEdit::OnLayoutMoved()
{
	DEBUG_CHECKER(TRUE);
	m_caret.UpdatePos();
	if (m_layout_modifier.IsActive())
		m_layout_modifier.UpdateRect();
}

void OpDocumentEdit::ReportOOM()
{
	DEBUG_CHECKER(TRUE);
	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void OpDocumentEdit::ReportIfOOM(OP_STATUS status)
{
	DEBUG_CHECKER(TRUE);
	if (OpStatus::IsMemoryError(status))
		ReportOOM();
}

void OpDocumentEdit::ScrollIfNeeded()
{
	DEBUG_CHECKER(TRUE);
	if (!m_caret.IsValid()) // Avoid scrolling if the caret isn't valid (not visible, probably no layoutbox)
		return;
	ReflowAndUpdate();
	OpRect rect = m_caret.GetCaretRectInDocument();

	HTML_Element* ec = GetEditableContainer(m_caret.GetElement());
	if(m_doc->GetHtmlDocument())
		// We might need to scroll a scrollable container and update caretpos again.
		m_doc->ScrollToRect(rect, SCROLL_ALIGN_NEAREST, FALSE, VIEWPORT_CHANGE_REASON_DOCUMENTEDIT, ec ? m_caret.GetElement() : NULL);
}

void OpDocumentEdit::OnReflow()
{
	DEBUG_CHECKER(TRUE);
	TextSelection* sel = m_doc->GetTextSelection();
	// Initialize the caret only when there's no selection or a selection is not being updated.
	// If there's a selection we may be reflowing to find elements for its end points (because the caret == a selection).
	if(!m_caret.GetElement() && (!sel || !sel->IsBeingUpdated()))
		m_caret.Init(FALSE);

	if (m_layout_modifier.IsActive())
		m_layout_modifier.UpdateRect();
}

void OpDocumentEdit::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	DEBUG_CHECKER(TRUE);
	if (focus && !m_caret.GetElement())
		m_caret.Init(TRUE, NULL, TRUE);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_enable_spellcheck_later = focus && !m_spell_session && !m_by_user;
#endif // INTERNAL_SPELLCHECK_SUPPORT
	if (focus)
	{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		if(m_enable_spellcheck_later && g_internal_spellcheck->SpellcheckEnabledByDefault())
		{
			OpString pref_lang;
			BOOL has_pl = GetPreferredLanguage(pref_lang);
			EnableSpellcheckInternal(FALSE /*by_user*/, has_pl ? pref_lang.CStr() : NULL);
			m_enable_spellcheck_later = false;
		}
#endif // INTERNAL_SPELLCHECK_SUPPORT

		if (m_doc->GetHtmlDocument())
		{
			HTML_Element* ec = GetEditableContainer(m_caret.GetElement());
			if (ec)
				m_doc->GetHtmlDocument()->SetFocusedElement(ec, FALSE);
		}

		m_caret.RestartBlink();
	}
	else
	{
		m_caret.StopBlink();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		if(m_delay_misspell_word_info)
		{
			m_delay_misspell_word_info = NULL;
			RepaintElement(m_caret.GetElement());
		}
#endif // INTERNAL_SPELLCHECK_SUPPORT
	}

#if defined(WIDGETS_IME_SUPPORT) && !defined(DOCUMENTEDIT_DISABLE_IME_SUPPORT)
	VisualDevice* vd = m_doc->GetVisualDevice();
	if (vd->GetView())
	{
		vd->GetView()->GetOpView()->SetInputMethodMode(focus ? OpView::IME_MODE_TEXT : OpView::IME_MODE_UNKNOWN, OpView::IME_CONTEXT_DEFAULT, NULL);
		if (!focus)
			vd->GetView()->GetOpView()->AbortInputMethodComposing();
	}
#endif // WIDGETS_IME_SUPPORT && !DOCUMENTEDIT_DISABLE_IME_SUPPORT
}

void OpDocumentEdit::CheckLogTreeChanged(BOOL caused_by_user)
{
	DEBUG_CHECKER(TRUE);
	if (m_logtree_changed)
	{
		Begin();
		m_undo_stack.Clear();
		if (!m_caret.GetElement())
			m_caret.Init(FALSE /* TRUE */);
		m_logtree_changed = FALSE;

		CollapseWhitespace();
		End();
	}
	else if (!m_caret.GetElement())
	{
		if (caused_by_user)
			m_undo_stack.Clear();
		m_caret.Init(FALSE /* caused_by_user */);
	}
}

void OpDocumentEdit::OnElementInserted(HTML_Element* elm)
{
	DEBUG_CHECKER(TRUE);
	OpDocumentEditInternalEventListener *listener = (OpDocumentEditInternalEventListener*)m_internal_event_listeners.First();
	while(listener)
	{
		OpDocumentEditInternalEventListener *next_listener = (OpDocumentEditInternalEventListener *)(listener->Suc());
		listener->OnElementInserted(elm);
		listener = next_listener;
	}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spell_session)
		SpellInvalidateAround(elm);
	else if ((m_enable_spellcheck_later || (!m_by_user && elm->SpellcheckEnabledByAttr() == HTML_Element::SPC_ENABLE)) && g_internal_spellcheck->SpellcheckEnabledByDefault())
	{
		OpString pref_lang;
		BOOL has_pl = GetPreferredLanguage(pref_lang);
		EnableSpellcheckInternal(FALSE /*by_user*/, has_pl ? pref_lang.CStr() : NULL);
		m_enable_spellcheck_later = FALSE;
	}

#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (m_begin_count == 0) // Deleted from outside documenteditor (by script).
		m_logtree_changed = TRUE;
}

void OpDocumentEdit::OnElementChange(HTML_Element* elm)
{
	OpDocumentEditInternalEventListener *listener = (OpDocumentEditInternalEventListener*)m_internal_event_listeners.First();
	while(listener)
	{
		OpDocumentEditInternalEventListener *next_listener = (OpDocumentEditInternalEventListener *)(listener->Suc());
		listener->OnElementChange(elm);
		listener = next_listener;
	}
}

void OpDocumentEdit::OnElementChanged(HTML_Element* elm)
{
	OpDocumentEditInternalEventListener *listener = (OpDocumentEditInternalEventListener*)m_internal_event_listeners.First();
	while(listener)
	{
		OpDocumentEditInternalEventListener *next_listener = (OpDocumentEditInternalEventListener *)(listener->Suc());
		listener->OnElementChanged(elm);
		listener = next_listener;
	}
}

void OpDocumentEdit::OnBeforeElementOut(HTML_Element* elm)
{
	DEBUG_CHECKER(TRUE);

	OpDocumentEditAutoLink auto_link(elm, &m_before_out_elements);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_doc_has_changed = TRUE;
	if(m_spell_session)
		SpellInvalidateAround(elm, TRUE);

	if(elm->IsAncestorOf(m_last_helm_spelled))
		m_last_helm_spelled = NULL;

	OP_ASSERT(!elm->IsAncestorOf(m_pending_spell_first) && !elm->IsAncestorOf(m_pending_spell_last));
#endif // INTERNAL_SPLLCHECK_SUPPORT

	m_caret.m_remove_when_move_elm = NULL;

	OpDocumentEditInternalEventListener *listener = (OpDocumentEditInternalEventListener*)m_internal_event_listeners.First();
	while(listener)
	{
		OpDocumentEditInternalEventListener *next_listener = (OpDocumentEditInternalEventListener *)(listener->Suc());
		listener->OnElementOut(elm);
		listener = next_listener;
	}
}

BOOL OpDocumentEdit::IsBeforeOutElm(HTML_Element *elm)
{
	OpDocumentEditAutoLink *link;
	for(link = (OpDocumentEditAutoLink*)m_before_out_elements.First(); link; link = (OpDocumentEditAutoLink*)link->Suc())
		if(link->GetObject() && ((HTML_Element*)(link->GetObject()))->IsAncestorOf(elm))
			return TRUE;

	return FALSE;
}

void OpDocumentEdit::OnElementRemoved(HTML_Element* elm)
{
	DEBUG_CHECKER(TRUE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OP_ASSERT(!elm->IsAncestorOf(m_pending_spell_first) && !elm->IsAncestorOf(m_pending_spell_last));
#endif // INTERNAL_SPELLCHECK_SUPPORT
	m_caret.m_remove_when_move_elm = NULL;
	if (m_begin_count == 0) // Deleted from outside documenteditor (by script).
	{
		if (elm == m_caret.GetElement() || elm->IsAncestorOf(m_caret.GetElement()))
		{	// Dont let the caret point to the removed element.
			HTML_Element *body = GetBody();
			if(!body || elm->IsAncestorOf(body))
				m_caret.Set(NULL,0);
			else
				m_caret.Init(FALSE);
		}

		m_logtree_changed = TRUE;
	}

#ifdef WIDGETS_IME_SUPPORT
 #ifndef DOCUMENTEDIT_DISABLE_IME_SUPPORT
	if (m_ime_string_elm == elm)
		m_ime_string_elm = NULL;
 #endif // !DOCUMENTEDIT_DISABLE_IME_SUPPORT
#endif // WIDGETS_IME_SUPPORT

	if (elm->IsAncestorOf(m_caret.m_parent_candidate))
		m_caret.m_parent_candidate = NULL;
	if (elm->IsAncestorOf(m_layout_modifier.m_helm))
		m_layout_modifier.Unactivate();

	m_recreate_caret = !m_caret.GetElement() && !m_caret.m_parent_candidate;

	if (!m_caret.GetElement())
		PostRecreateCaretMessage();
}

void OpDocumentEdit::OnElementDeleted(HTML_Element* elm)
{
	DEBUG_CHECKER(TRUE);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OP_ASSERT(!elm->IsAncestorOf(m_pending_spell_first) && !elm->IsAncestorOf(m_pending_spell_last));
	// There seems to be many situations where the above asserts trig and we crash later.
	// NULL'ing the pointers here for now so we at least doesn't crash.
	if (elm->IsAncestorOf(m_pending_spell_first) || elm->IsAncestorOf(m_pending_spell_last))
	{
		m_pending_spell_first = m_pending_spell_last = NULL;
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT
	OpDocumentEditInternalEventListener *listener = (OpDocumentEditInternalEventListener*)m_internal_event_listeners.First();
	while(listener)
	{
		OpDocumentEditInternalEventListener *next_listener = (OpDocumentEditInternalEventListener *)(listener->Suc());
		listener->OnElementDeleted(elm);
		listener = next_listener;
	}

	OP_ASSERT(elm != m_content_pending_helm);
	if (elm == m_content_pending_helm)
		m_content_pending_helm = NULL;

	m_caret.m_remove_when_move_elm = NULL;
	if (elm->IsAncestorOf(m_caret.GetElement()))
		OnBeforeElementOut(elm);
	if (elm->IsAncestorOf(m_caret.m_parent_candidate))
		m_caret.m_parent_candidate = NULL;
	if (elm->IsAncestorOf(m_layout_modifier.m_helm))
		m_layout_modifier.Unactivate();

	if (!GetRoot()->IsAncestorOf(elm))
		return;

	if (m_begin_count == 0) // Deleted from outside documenteditor (by script).
		m_logtree_changed = TRUE;
}

void OpDocumentEdit::OnTextConvertedToTextGroup(HTML_Element* elm)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	// The spell checker points to text elements, not to textgroups. If a text element
	// changes we need to modify the spell checker as well.

	if (m_pending_spell_first == elm)
		m_pending_spell_first = elm->FirstChild();

	if (m_pending_spell_last == elm)
		m_pending_spell_last = elm->FirstChild();

	m_word_iterator.OnTextConvertedToTextGroup(elm);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpDocumentEdit::OnTextChange(HTML_Element *elm)
{
	OnElementChange(elm);
}

void OpDocumentEdit::OnTextChanged(HTML_Element *elm)
{
	OnElementChanged(elm);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_doc_has_changed = TRUE;
	if(elm->Type() == HE_TEXT && m_spell_session && !IsInBeforeElementOut())
		SpellInvalidateAround(elm);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpDocumentEdit::OnTextElmGetsLayoutWords(HTML_Element *elm)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_doc_has_changed = TRUE;
	if(elm->Type() == HE_TEXT && m_spell_session && !IsInBeforeElementOut())
		SpellInvalidateAround(elm);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

BOOL OpDocumentEdit::LineIsEmpty(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
	{
		OP_ASSERT(FALSE);
		return TRUE;
	}
	if((helm->GetTextContentLength() && !IsCollapsed(helm)) || (IsStandaloneElement(helm) && helm->Type() != HE_BR && helm->Type() != HE_TEXT))
		return FALSE;
	HTML_Element *tmp = helm->PrevActual();
	while(tmp)
	{
		if(!IsFriends(tmp,helm,TRUE,TRUE,FALSE))
			break;
		if(IsStandaloneElement(tmp) || !IsCollapsed(tmp))
			return FALSE;
		tmp = tmp->PrevActual();
	}
	tmp = helm->NextActual();
	while(tmp)
	{
		if(!IsFriends(helm,tmp,TRUE,TRUE,FALSE))
			break;
		if(IsStandaloneElement(tmp) || !IsCollapsed(tmp))
			return FALSE;
		tmp = tmp->NextActual();
	}
	return TRUE;
}

BOOL OpDocumentEdit::NeedAutoInsertedBR(HTML_Element *containing_element, BOOL ignore_current_auto_inserted_br)
{
	if (containing_element->Type() == HE_LI)
		return FALSE; ///< Not good, but layout collapse list items if we put a br in them! (oddly enough!)

	HTML_Element *tmp = containing_element->FirstChildActual();
	while (tmp && containing_element->IsAncestorOf(tmp))
	{
		if (ignore_current_auto_inserted_br && tmp->Type() == HE_BR && GetInsertedAutomatically(tmp))
			; // continue
		else if (tmp->Type() == HE_TEXT && tmp == m_content_pending_helm)
			return FALSE;
		else if (tmp->Type() == HE_TEXT && !IsCollapsed(tmp) && tmp->GetTextContentLength())
			return FALSE;
		else if (IsStandaloneElement(tmp) && tmp->Type() != HE_TEXT)
			return FALSE;
		else if (IsBlockElement(tmp))
			return FALSE;
		tmp = tmp->NextActual();
	}
	return TRUE;
}

BOOL OpDocumentEdit::HasAutoInsertedBR(HTML_Element *containing_element)
{
	HTML_Element *tmp = containing_element->FirstChildActual();
	while (tmp && containing_element->IsAncestorOf(tmp))
	{
		if (tmp->Type() == HE_BR && GetInsertedAutomatically(tmp))
			return TRUE;
		tmp = tmp->NextActual();
	}
	return FALSE;
}

BOOL OpDocumentEdit::WillEndLineCollapseIfWs(uni_char *buf, HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	if(buf && *buf)
	{
		while(uni_collapsing_sp(*++buf)) {}
		if(*buf)
			return FALSE;
	}
	HTML_Element *after = helm;
	do
	{
		after = after->NextActual();
		if (!after || !IsFriends(helm, after))
			return TRUE;
	} while(after->Type() != HE_TEXT || after->GetTextContentLength() == 0 || IsCollapsed(after));
	return FALSE;
}

BOOL OpDocumentEdit::IsCollapsed(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	if (!helm->GetLayoutBox())
		return TRUE;

	if(helm->Type() == HE_TEXT)
	{
		OpDocumentEditWordIterator iter(helm,this);
		if(OpStatus::IsSuccess(iter.GetStatus()))
			return iter.IsCollapsed();
	}
	return FALSE;
}

HTML_Element *OpDocumentEdit::GetEdgeHelmMightWsCollapse(HTML_Element *helm, BOOL forward)
{
	DEBUG_CHECKER(TRUE);
	int idx;
	helm = forward ? helm->NextActual() : helm->PrevActual();
	while(helm && (helm->Type() != HE_TEXT || !helm->GetTextContentLength()))
		helm = forward ? helm->NextActual() : helm->PrevActual();

	OpDocumentEditWordIterator iter(helm,this);
	if(OpStatus::IsError(iter.GetStatus()))
		return NULL;

	idx = forward ? 0 : helm->GetTextContentLength()-1;

	return uni_collapsing_sp(helm->TextContent()[idx]) && !iter.IsCharCollapsed(idx) ? helm : NULL;
}

HTML_Element* OpDocumentEdit::FindEditableElement(HTML_Element* from_helm, BOOL forward, BOOL include_current, BOOL require_box, BOOL include_ending_br, HTML_Element* helm_to_remove)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *editable_helm = NULL;
	HTML_Element *helm = from_helm;
	HTML_Element *body = GetBody();
	if (!body || !helm)
		return NULL;
	if (!include_current)
	{
		helm = (forward ? helm->NextActual() : helm->PrevActual());
		if (helm_to_remove && helm_to_remove->IsAncestorOf(helm))
			helm = (forward ? helm->NextActual() : helm->PrevActual());
	}

	// Exit if helm is outside of body, but not if helm is (temporarily) detached from the tree.
	if (!body->IsAncestorOf(helm) && GetRoot()->IsAncestorOf(helm))
		return NULL;

	while(helm && helm != body)
	{
		if (IsElementValidForCaret(helm, TRUE, include_ending_br, FALSE, helm_to_remove))
		{
			//if (require_box && !helm->GetLayoutBox() && helm->IsDirty())
			//FIX: Reflow here so we don't have to do it always from other places.

			if (!require_box || helm->GetLayoutBox())
			{
				editable_helm = helm;
				break;
			}
		}
		helm = (forward ? helm->NextActual() : helm->PrevActual());
		if (helm_to_remove && helm_to_remove->IsAncestorOf(helm))
			helm = (forward ? helm->NextActual() : helm->PrevActual());
	}
	return editable_helm;
}

BOOL OpDocumentEdit::IsEndingBr(HTML_Element* helm, HTML_Element* helm_to_remove)
{
	DEBUG_CHECKER(TRUE);
	if (helm->Type() != HE_BR)
		return FALSE;
	// Handles the case when the BR is ending but not in the same branch.
	// <I>gggggggggggggg</I><BR>
	HTML_Element* prev_helm = FindEditableElement(helm, FALSE, FALSE, TRUE, TRUE, helm_to_remove);
	return (prev_helm && prev_helm->Type() != HE_BR && IsFriends(prev_helm, helm, TRUE, TRUE, TRUE));
}

BOOL OpDocumentEdit::GetNearestCaretPos(HTML_Element* from_helm, HTML_Element** nearest_helm, int* nearest_ofs, BOOL forward, BOOL include_current, HTML_Element* helm_to_remove)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* helm = FindEditableElement(from_helm, forward, include_current, TRUE, FALSE, helm_to_remove);
	if (!helm)
		return FALSE;

	if (forward)
	{
		*nearest_ofs = 0;
	}
	else
	{
		if(helm->Type() == HE_TEXT)
			*nearest_ofs = helm->GetTextContentLength();
		else
			*nearest_ofs = helm->Type() == HE_BR ? 0 : 1;
	}
	*nearest_helm = helm;
	return TRUE;
}

BOOL OpDocumentEdit::GetBestCaretPosFrom(HTML_Element *helm, HTML_Element *&new_helm, int &new_ofs, BOOL3 prefer_first, BOOL must_be_friends, HTML_Element* helm_to_remove)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *prev_helm = NULL, *next_helm = NULL;
	int prev_ofs = 0, next_ofs = 0;
	GetNearestCaretPos(helm,&prev_helm,&prev_ofs,FALSE,FALSE, helm_to_remove);
	if(must_be_friends && !IsFriends(prev_helm,helm,TRUE,TRUE))
	{
		prev_helm = NULL;
		prev_ofs = 0;
	}
	GetNearestCaretPos(helm->NextSiblingActual(),&next_helm,&next_ofs,TRUE,TRUE, helm_to_remove);
	if(must_be_friends && !IsFriends(helm,next_helm,TRUE,TRUE))
	{
		next_helm = NULL;
		next_ofs = 0;
	}
	if(!prev_helm || !next_helm)
	{
		new_helm = prev_helm ? prev_helm : next_helm;
		new_ofs = prev_ofs | next_ofs;
		return !!new_helm;
	}
	BOOL pick_first = IsFriends(prev_helm,helm,TRUE,TRUE,TRUE);
	if(prefer_first == NO)
		pick_first = pick_first && !IsFriends(helm,next_helm,TRUE,TRUE);
	if(pick_first)
	{
		new_helm = prev_helm;
		new_ofs = prev_ofs;
	}
	else
	{
		new_helm = next_helm;
		new_ofs = next_ofs;
	}
	return TRUE;
}

BOOL OpDocumentEdit::GetValidCaretPosFrom(HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return FALSE;

	if(IsElementValidForCaret(helm))
	{
		new_helm = helm;
		if(helm->Type() == HE_TEXT)
		{
			OpDocumentEditWordIterator iter(helm,this);
			iter.SnapToValidCaretOfs(ofs,new_ofs);
		}
		else if(helm->Type() == HE_BR)
			new_ofs = 0;
		else
			new_ofs = MAX(MIN(ofs,1),0);
		return TRUE;
	}
	HTML_Element *candidate_helm = NULL;
	int candidate_ofs = 0;
	BOOL found = FALSE;

	if(helm->FirstChildActual())
	{
		BOOL beginning = ofs <= 0;
		if(beginning)
		{
			if(GetNearestCaretPos(helm,&candidate_helm,&candidate_ofs,TRUE,FALSE))
				found = helm->IsAncestorOf(candidate_helm);
		}
		else
		{
			if(GetNearestCaretPos(helm->LastLeafActual(),&candidate_helm,&candidate_ofs,FALSE,TRUE))
				found = helm->IsAncestorOf(candidate_helm);
		}
	}
	if(!found && !GetBestCaretPosFrom(helm,candidate_helm,candidate_ofs))
	{
		candidate_helm = NULL;
		candidate_ofs = 0;
	}
	else
		found = TRUE;
	new_helm = candidate_helm;
	new_ofs = candidate_ofs;
	return found;
}

BOOL OpDocumentEdit::GetLastValidCaretOfs(HTML_Element *helm, int &ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return FALSE;
	if(helm->Type() == HE_TEXT)
	{
		OpDocumentEditWordIterator iter(helm,this);
		if(OpStatus::IsError(iter.GetStatus()) || !iter.GetLastValidCaretOfs(ofs))
			return FALSE;
	}
	else
		ofs = 1;
	return TRUE;
}

BOOL OpDocumentEdit::GetFirstValidCaretOfs(HTML_Element *helm, int &ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return FALSE;
	if(helm->Type() == HE_TEXT)
	{
		OpDocumentEditWordIterator iter(helm, this);
		if(OpStatus::IsError(iter.GetStatus()) || !iter.GetFirstValidCaretOfs(ofs))
			return FALSE;
	}
	else
		ofs = 0;
	return TRUE;
}

HTML_Element* OpDocumentEdit::FindElementBeforeOfType(HTML_Element* helm, HTML_ElementType type, BOOL require_box)
{
	DEBUG_CHECKER(TRUE);
	// FIX: reflow if helm doesn't have a layoutbox and is dirty.
	helm = helm ? helm->PrevActual() : NULL;
	while(helm && (helm->Type() != type || (require_box && !helm->GetLayoutBox())))
		helm = helm->PrevActual();
	return helm;
}

HTML_Element* OpDocumentEdit::FindElementAfterOfType(HTML_Element* helm, HTML_ElementType type, BOOL require_box)
{
	DEBUG_CHECKER(TRUE);
	// FIX: reflow if helm doesn't have a layoutbox and is dirty.
	helm = helm ? helm->NextActual() : NULL;
	while(helm && (helm->Type() != type || (require_box && !helm->GetLayoutBox())))
		helm = helm->NextActual();
	return helm;
}

HTML_Element* OpDocumentEdit::MakeSureHasValidEdgeCaretPos(HTML_Element *helm, HTML_Element *container, BOOL at_end)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *new_helm = NULL, *existing_helm = NULL;
	if(!helm && !container)
		return NULL;
	if(helm && container && !container->IsAncestorOf(helm))
	{
		OP_ASSERT(FALSE);
		return NULL;
	}
	else if(helm && !container)
	{
		container = GetEditableContainer(helm);
		if(!container)
			return NULL;
	}
	if(IsStandaloneElement(container))
		return NULL;
	if(container->FirstChildActual())
	{
		if(at_end)
			existing_helm = FindEditableElement(container->LastLeafActual(),FALSE,TRUE,TRUE);
		else
			existing_helm = FindEditableElement(container,TRUE,FALSE,TRUE);
		if(!container->IsAncestorOf(existing_helm))
			existing_helm = NULL;
	}
	else
		existing_helm = NULL;
	if(!existing_helm)
	{
		new_helm = NewTextElement(UNI_L(""),0);
		if(!new_helm)
		{
			ReportOOM();
			return NULL;
		}
		new_helm->UnderSafe(m_doc,container);
		return new_helm;
	}

	HTML_Element *before_blocker = NULL, *after_blocker = NULL;
	HTML_Element *stop = container->NextSiblingActual();
	HTML_ElementType blocker_types[] = {HE_TABLE, HE_HR, HE_UNKNOWN};
	helm = container->FirstChild();
	BOOL found_existing = FALSE;
	while(helm && helm != stop)
	{
		if(helm == existing_helm)
			found_existing = TRUE;
		else if(IsAnyOfTypes(helm,blocker_types))
		{
			if(!found_existing)
			{
				before_blocker = helm;
				if(helm->IsAncestorOf(existing_helm))
					after_blocker = helm;
			}
			else if(!after_blocker || after_blocker->IsAncestorOf(helm))
				after_blocker = helm;
		}
		helm = helm->NextActual();
	}
	if((after_blocker && at_end) || (before_blocker && !at_end))
	{
		if(at_end)
			new_helm = m_caret.CreateTemporaryCaretHelm(after_blocker->Parent(),after_blocker);
		else
			new_helm = m_caret.CreateTemporaryCaretHelm(before_blocker->Parent(),before_blocker->Pred());
	}

	// Add empty text element to the right or left of a non-editable element
	// inside an editable element to be able to place the caret there.
	if (!new_helm && at_end && !m_caret.IsElementEditable(container->LastChildActual()))
	{
		new_helm = NewTextElement(document_edit_dummy_str, 1);
		BeginChange(container);
		new_helm->UnderSafe(m_doc, container);
		EndChange(container, TIDY_LEVEL_NONE);
	}
	else if (!new_helm && !at_end && !m_caret.IsElementEditable(container->FirstChildActual()))
	{
		new_helm = NewTextElement(document_edit_dummy_str, 1);
		BeginChange(container);
		new_helm->PrecedeSafe(m_doc, container->FirstChild());
		EndChange(container, TIDY_LEVEL_NONE);
	}

	return new_helm;
}

HTML_Element* OpDocumentEdit::NewTextElement(const uni_char* text, int len)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *new_elm = NEW_HTML_Element();
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	if (!new_elm)
		return NULL;
	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, text, len)))
	{
		DeleteElement(new_elm);
		return NULL;
	}
	return new_elm;
}

HTML_Element* OpDocumentEdit::NewElement(HTML_ElementType type, BOOL set_automatic_flag)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *new_elm = NEW_HTML_Element();
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	if (!new_elm)
		return NULL;
	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, NS_IDX_HTML, type, NULL, HE_INSERTED_BY_DOM))
		|| (set_automatic_flag && OpStatus::IsError(SetInsertedAutomatically(new_elm, TRUE)))
		)
	{
		DeleteElement(new_elm);
		return NULL;
	}
	new_elm->SetEndTagFound();
	return new_elm;
}

HTML_Element* OpDocumentEdit::NewCopyOfElement(HTML_Element* helm, BOOL remove_id)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(helm->IsIncludedActual() || helm->GetInserted() == HE_INSERTED_BY_IME);
	HTML_Element *new_elm = NEW_HTML_Element();
	HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
	if (!new_elm)
		return NULL;
	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, helm)))
	{
		DeleteElement(new_elm);
		return NULL;
	}
	new_elm->SetEndTagFound();
	if(remove_id)
		new_elm->RemoveAttribute(ATTR_ID);
	return new_elm;
}

void OpDocumentEdit::SetElementText(HTML_Element* helm, const uni_char* text, int len)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(helm->Type() == HE_TEXT);
	if (len == -1)
		len = uni_strlen(text);
	if (OpStatus::IsError(helm->SetText(m_doc, text, len)))
		ReportOOM();
	helm->RemoveCachedTextInfo(m_doc);
}

OP_STATUS OpDocumentEdit::SetInsertedAutomatically(HTML_Element* helm, BOOL inserted_automatically)
{
	return SetSpecialAttr(helm, ATTR_DOCEDIT_AUTO_INSERTED, inserted_automatically);
}

BOOL OpDocumentEdit::GetInsertedAutomatically(HTML_Element* helm)
{
	return GetSpecialAttr(helm, ATTR_DOCEDIT_AUTO_INSERTED);
}

OP_STATUS OpDocumentEdit::SetSpecialAttr(HTML_Element* helm, Markup::AttrType attr, BOOL value)
{
	DEBUG_CHECKER(TRUE);
	return helm->SetSpecialAttr(attr, ITEM_TYPE_NUM, (void*)(INTPTR)value, FALSE, SpecialNs::NS_DOCEDIT) == -1
			? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

BOOL OpDocumentEdit::GetSpecialAttr(HTML_Element* helm, Markup::AttrType attr)
{
	return !!helm->GetSpecialNumAttr(attr, SpecialNs::NS_DOCEDIT);
}

BOOL OpDocumentEdit::IsCharCollapsed(HTML_Element *helm, int ofs)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(helm && helm->Type() == HE_TEXT && ofs >= 0 && ofs < helm->GetTextContentLength());
	OpDocumentEditWordIterator iter(helm,this);
	if(OpStatus::IsError(iter.GetStatus()))
		return FALSE;
	return iter.IsCharCollapsed(ofs);
}

BOOL OpDocumentEdit::DeleteTextInElement(HTML_Element* helm, INT32 start_ofs, INT32 len)
{
	DEBUG_CHECKER(TRUE);
	if (len == 0 || helm->Type() != HE_TEXT)
		return FALSE;

	OpString str;
	if (OpStatus::IsError(str.Set(helm->TextContent())))
		ReportOOM();

	OP_ASSERT(start_ofs >= 0);
	start_ofs = MAX(start_ofs, 0);
	OP_ASSERT(len <= helm->GetTextContentLength());
	len = MIN(len, helm->GetTextContentLength());

	str.Delete(start_ofs, len);

	SetElementText(helm, str);
	return TRUE;
}

BOOL OpDocumentEdit::SplitElement(HTML_Element* helm, INT32 ofs)
{
	DEBUG_CHECKER(TRUE);
	if (helm->Type() != HE_TEXT)
		return FALSE;

	if (ofs != 0 && ofs != helm->GetTextContentLength())
	{
		// Create second part element
		const uni_char* second_part = helm->TextContent() + ofs;
		HTML_Element* new_elm = NewTextElement(second_part, uni_strlen(second_part));
		if (!new_elm)
		{
			ReportOOM();
			return FALSE;
		}
		else
		{
			// Delete second part from the first element.
			DeleteTextInElement(helm, ofs, helm->GetTextContentLength() - ofs);
			// Insert second part
			new_elm->FollowSafe(m_doc, helm);
			new_elm->MarkExtraDirty(m_doc);
		}
		return TRUE;
	}
	return FALSE;
}

/*
FIX:

HTMLArea.isBlockElement = function(el) {
	var blockTags = " body form textarea fieldset ul ol dl li div " +
		"p h1 h2 h3 h4 h5 h6 quote pre table thead " +
		"tbody tfoot tr td iframe address ";
	return (blockTags.indexOf(" " + el.tagName.toLowerCase() + " ") != -1);
};

HTMLArea.needsClosingTag = function(el) {
	var closingTags = " script style div span tr td tbody table em strong font a ";
	return (closingTags.indexOf(" " + el.tagName.toLowerCase() + " ") != -1);
};
*/

void OpDocumentEdit::AddWsPreservingOperation(OpDocumentEditWsPreserver *preserver)
{
	DEBUG_CHECKER(TRUE);
	if(!preserver)
		return;
	preserver->Out();
	preserver->Into(&m_ws_preservers);
}

void OpDocumentEdit::RemoveWsPreservingOperation(OpDocumentEditWsPreserver *preserver)
{
	DEBUG_CHECKER(TRUE);
	if(preserver)
		preserver->Out();
}

BOOL OpDocumentEdit::IsInWsPreservingOperation(HTML_Element *helm, BOOL3 was_collapsed)
{
	DEBUG_CHECKER(TRUE);
	if(!helm)
		return FALSE;
	OpDocumentEditWsPreserver *p = (OpDocumentEditWsPreserver*)m_ws_preservers.First();
	while(p)
	{
		if(p->GetStartElement() == helm)
			return was_collapsed == MAYBE || p->WasStartElementCollapsed() == (was_collapsed == YES);
		if(p->GetStopElement() == helm)
			return was_collapsed == MAYBE || p->WasStopElementCollapsed() == (was_collapsed == YES);
		p = (OpDocumentEditWsPreserver*)p->Suc();
	}
	return FALSE;
}

BOOL OpDocumentEdit::IsElementValidForCaret(HTML_Element *helm, BOOL is_in_tree, BOOL ending_br_is_ok, BOOL valid_if_possible, HTML_Element* helm_to_remove)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || !helm->IsIncludedActual() || (is_in_tree && !m_caret.IsElementEditable(helm)))
		return FALSE;
	HTML_Element *parent = helm->Parent();
	for( ; parent && !IsReplacedElement(parent); parent = parent->Parent()) {}
	if(parent)
		return FALSE;
	if(helm->Type() == HE_BR)
		return ending_br_is_ok || !IsEndingBr(helm, helm_to_remove);//return TRUE;
	if(helm->Type() == HE_TEXT)
	{
		if(!is_in_tree)
			return TRUE;
		if(IsInWsPreservingOperation(helm,NO)) // This element might for the moment not be valid, but WILL be later
			return TRUE;
		OpDocumentEditWordIterator iter(helm,this);
		return OpStatus::IsSuccess(iter.GetStatus()) && iter.IsValidForCaret(valid_if_possible);
	}
	if(IsReplacedElement(helm))
		return TRUE;

	return FALSE;
}

BOOL OpDocumentEdit::IsFriendlyElement(HTML_Element* helm, BOOL include_replaced, BOOL br_is_friendly, BOOL non_editable_is_friendly)
{
	DEBUG_CHECKER(TRUE);
 	if (!m_caret.IsElementEditable(helm) && !non_editable_is_friendly)
		return FALSE;
	if (!include_replaced && IsStandaloneElement(helm) && helm->Type() != HE_TEXT && helm->Type() != HE_BR)
		return FALSE;
	if (helm->Type() == HE_BR && !br_is_friendly)
		return FALSE;
	return !IsBlockElement(helm);
}

BOOL OpDocumentEdit::IsFriends(HTML_Element* helm_a, HTML_Element* helm_b, BOOL include_helm_b, BOOL include_replaced, BOOL br_is_friendly)
{
	DEBUG_CHECKER(TRUE);
	if(!helm_a || !helm_b)
		return FALSE;

	HTML_Element* unfriendly_parent_a = helm_a->Parent();//Actual();
	HTML_Element* unfriendly_parent_b = helm_b->Parent();//Actual();

	BOOL a_editable = m_caret.IsElementEditable(helm_a);
	BOOL b_editable = m_caret.IsElementEditable(helm_b);

	while(unfriendly_parent_a && IsFriendlyElement(unfriendly_parent_a, include_replaced, br_is_friendly, a_editable))
		unfriendly_parent_a = unfriendly_parent_a->Parent();//Actual();
	while(unfriendly_parent_b && IsFriendlyElement(unfriendly_parent_b, include_replaced, br_is_friendly, b_editable))
		unfriendly_parent_b = unfriendly_parent_b->Parent();//Actual();

	if (unfriendly_parent_a != unfriendly_parent_b)
		return FALSE;

	BOOL friendly = TRUE;

	if (helm_b->Precedes(helm_a))
	{
		HTML_Element* tmp = helm_a;
		helm_a = helm_b;
		helm_b = tmp;
	}

	// Check if there is a unfriendly element between. If there is, return FALSE.
	HTML_Element* tmp = (HTML_Element*) helm_a;
	while(tmp)
	{
		if (tmp == helm_b && !include_helm_b)
			break;
		if (!IsFriendlyElement(tmp, include_replaced, br_is_friendly) /*&& !tmp->GetIsPseudoElement()*/)
		{
			friendly = FALSE;
			break;
		}
		if (tmp == helm_b)
			break;
		tmp = (HTML_Element*) tmp->Next();
	}

	return friendly;
}

BOOL OpDocumentEdit::GetContentRectOfElement(HTML_Element *helm, AffinePos &ctm, RECT &rect, BoxRectType type)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || !helm->GetLayoutBox())
		return FALSE;
	return helm->GetLayoutBox()->GetRect(m_doc, type, ctm, rect);
}

BOOL OpDocumentEdit::KeepWhenTidy(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	switch(helm->Type())
	{
	case HE_TR:
	case HE_TD:
	case HE_TH:
	case HE_TBODY:
	case HE_THEAD:
	case HE_TFOOT:
	case HE_COLGROUP:
		return TRUE;
	};
	if (helm->IsContentEditable())
		return TRUE;
	if (helm->GetFirstReferenceOfType(ElementRef::DOCEDITELM))
		return TRUE;
	return FALSE;
}

BOOL OpDocumentEdit::KeepWhenTidyIfEmpty(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	switch(helm->Type())
	{
	case HE_IFRAME:
	case HE_TEXTAREA:
		return TRUE;
	};
	if (helm->IsContentEditable())
		return TRUE;
	if (helm->GetFirstReferenceOfType(ElementRef::DOCEDITELM))
		return TRUE;
	return FALSE;
}

BOOL OpDocumentEdit::IsNoTextContainer(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	switch(helm->Type())
	{
	case HE_TR:
	case HE_TH:
	case HE_TBODY:
	case HE_THEAD:
	case HE_TFOOT:
	case HE_COLGROUP:
	case HE_IFRAME:
		return TRUE;
	};
	if (helm->GetInserted() == HE_INSERTED_BY_LAYOUT)
		return TRUE;
	return FALSE;
}

BOOL OpDocumentEdit::IsEnclosedBy(HTML_Element* helm, HTML_Element* start, HTML_Element* stop)
{
	DEBUG_CHECKER(TRUE);
	if (start->Precedes(helm))
	{
		if (!helm->IsAncestorOf(stop))
			return TRUE;
	}
	return FALSE;
}

BOOL OpDocumentEdit::ContainsTextBetween(HTML_Element* start, HTML_Element* stop)
{
	DEBUG_CHECKER(TRUE);
	start = (HTML_Element*) start->Next();
	while(start != stop)
	{
		if (start->Type() == HE_TEXT)
			return TRUE;
		start = (HTML_Element*) start->Next();
	}
	return FALSE;
}

OldStyleTextSelectionPoint OpDocumentEdit::GetTextSelectionPoint(HTML_Element* helm, int character_offset, BOOL prefer_first)
{
	DEBUG_CHECKER(TRUE);
	OldStyleTextSelectionPoint sel_point;

	if(!helm)
		return sel_point;

	if(helm->Type() != HE_TEXT)
		character_offset = helm->Type() == HE_BR ? 0 : MAX(MIN(character_offset,1),0);
	else
	{
		OpDocumentEditWordIterator iter(helm,this);
		if(OpStatus::IsError(iter.GetStatus()) || !iter.HasUnCollapsedChar())
			character_offset = 0;
		else
			iter.SnapToValidCaretOfs(character_offset,character_offset);
	}

	sel_point.SetLogicalPosition(helm,character_offset);
	sel_point.SetBindDirection(prefer_first ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);

	return sel_point;
}

OP_STATUS OpDocumentEdit::GetTextHTMLFromNamedElement(OpString& text, const uni_char* element_name, BOOL include_helm)
{
	if (m_doc->GetLogicalDocument())
	{
		HTML_Element* helm = m_doc->GetLogicalDocument()->GetNamedHE(element_name);
		if (helm)
			return GetTextHTMLFromElement(text, helm, include_helm);
	}

	return OpStatus::ERR;
}

BOOL OpDocumentEdit::DeleteNamedElement(const uni_char* element_name)
{
	if (m_doc->GetLogicalDocument())
	{
		HTML_Element* helm = m_doc->GetLogicalDocument()->GetNamedHE(element_name);
		if (helm)
		{
			DeleteElement(helm);
			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS OpDocumentEdit::GetTextHTMLFromElement(OpString& text, HTML_Element* helm, BOOL include_helm)
{
	DEBUG_CHECKER(TRUE);
	TempBuffer tmp_buf;

	tmp_buf.SetExpansionPolicy( TempBuffer::AGGRESSIVE );
	tmp_buf.SetCachedLengthPolicy( TempBuffer::TRUSTED );

	OP_STATUS oom_stat = HTML5Parser::SerializeTree(helm, &tmp_buf, HTML5Parser::SerializeTreeOptions().IncludeRoot(include_helm));

	if (OpStatus::IsSuccess(oom_stat))
		oom_stat = text.Set(tmp_buf.GetStorage());

	return oom_stat;
}

void OpDocumentEdit::DeleteElement(HTML_Element* helm, OpDocumentEdit* edit, BOOL check_caret)
{
#ifdef _DOCEDIT_DEBUG
	OpDocumentEditDebugChecker __docedit_debug_checker__dbg__ = OpDocumentEditDebugChecker(edit,edit,TRUE,TRUE);
#endif
	if (helm)
	{
		FramesDocument* fdoc = NULL;

		if (edit && edit->GetRoot()->IsAncestorOf(helm))
			fdoc = edit->GetDoc();

		if (fdoc && helm->Parent())
		{
			helm->Parent()->MarkDirty(fdoc, FALSE, TRUE);

			HTML_Document *html_doc = fdoc->GetHtmlDocument();
			if (html_doc && (helm == html_doc->GetHoverHTMLElement() || helm->IsAncestorOf(html_doc->GetHoverHTMLElement())))
				// Avoid crash when lowlevelapplypseudoclasses traverse the hoverelement when it is not in the document anymore.
				html_doc->SetHoverHTMLElement(NULL);
		}

		helm->Remove(fdoc, TRUE);
		if (helm->Clean(fdoc))
			helm->Free(fdoc);
	}
}

HTML_Element*
OpDocumentEdit::GetTopEditableParent(HTML_Element* element)
{
	if (m_body_is_root)
		return GetBody();

	HTML_Element* top_editable_parent = NULL;
	while (element)
	{
		BOOL3 is_content_editable = element->GetContentEditableValue();
		if (is_content_editable == YES)
			top_editable_parent = element;
		else if (is_content_editable == NO)
			return top_editable_parent;

		element = element->ParentActual();
	}
	return top_editable_parent;
}

HTML_Element* OpDocumentEdit::GetSharedContainingElement(HTML_Element* elm1, HTML_Element* elm2)
{
	DEBUG_CHECKER_STATIC();
	HTML_Element* containing_elm1 = m_doc->GetCaret()->GetContainingElementActual(elm1);
	HTML_Element* containing_elm2 = m_doc->GetCaret()->GetContainingElementActual(elm2);

	if (!containing_elm1)
		containing_elm1 = GetEditableContainer(elm1);
	if (!containing_elm2)
		containing_elm2 = GetEditableContainer(elm2);

	if (containing_elm1 == containing_elm2)
		return containing_elm1;

	while(!containing_elm1->IsAncestorOf(containing_elm2))
	{
		HTML_Element *containing_elm = m_doc->GetCaret()->GetContainingElementActual(containing_elm1);
		if (containing_elm == containing_elm1)
			containing_elm1 = containing_elm1->ParentActual();
		else
			containing_elm1 = containing_elm;
	}

	return containing_elm1;
}

HTML_Element* OpDocumentEdit::GetRoot()
{
	DEBUG_CHECKER(TRUE);
	return m_doc->GetLogicalDocument() ? m_doc->GetLogicalDocument()->GetRoot() : NULL;
}

HTML_Element* OpDocumentEdit::GetBody()
{
	DEBUG_CHECKER(TRUE);
	return FindElementAfterOfType(GetRoot(), HE_BODY);
}

HTML_Element* OpDocumentEdit::GetEditableContainer(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *body = GetBody();
	if (m_body_is_root)
	{
		if(body && body->IsAncestorOf(helm))
			return body;
	}
	HTML_Element *tmp = helm, *container = NULL;
	if (!tmp || !body)
		return NULL;

	if (!body->IsAncestorOf(tmp))
		return NULL;

	while (tmp)
	{
		if (tmp->IsContentEditable())
			container = tmp;
		tmp = tmp->ParentActual();
	}
	if (container->IsAncestorOf(body))
		container = body;

	OP_ASSERT(!container || body->IsAncestorOf(container));
	return container;
}

HTML_Element* OpDocumentEdit::GetFocusableEditableContainer(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *body = GetBody();
	if (m_body_is_root)
	{
		if(body && body->IsAncestorOf(helm))
			return body;
	}

	HTML_Element *tmp = helm, *container = NULL;
	if (!tmp || !body)
		return NULL;

	if (!body->IsAncestorOf(tmp))
		tmp = body;

	while (tmp)
	{
		if (tmp->IsContentEditable())
			container = tmp;
		tmp = tmp->ParentActual();
	}

	if (container->IsAncestorOf(body))
		container = body;

	return container;
}

HTML_Element* OpDocumentEdit::GetHrContainer(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *container = GetEditableContainer(helm);
	if(!container)
		return NULL;
	while(helm != container)
	{
		switch(helm->Type())
		{
			case HE_TD:
				return helm;
		}
		helm = helm->ParentActual();
	}
	return helm;
}

void OpDocumentEdit::Tidy(HTML_Element* start_elm, HTML_Element* stop_elm, BOOL include_start_stop, TIDY_LEVEL tidy_level, BOOL keep_dummy, HTML_Element *shared_containing_elm)
{
	DEBUG_CHECKER(TRUE);
#ifdef DEBUG_DOCUMENT_EDIT
	if (start_elm == stop_elm && !m_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(start_elm))
		DUMPDEBUGTREE_ELM(start_elm)
	else
		DUMPDEBUGTREE
#endif

	if (tidy_level == TIDY_LEVEL_NONE)
		return;

	HTML_Element *old_caret = m_caret.GetElement();
	BOOL is_in_document = GetRoot()->IsAncestorOf(start_elm);

	if (is_in_document)
		ReflowAndUpdate();

	m_undo_stack.SetFlags(CHANGE_FLAGS_USER_INVISIBLE);

	HTML_Element* tmp;
	if (include_start_stop)
		tmp = start_elm;
	else
	{
		tmp = start_elm->Next();
		if (tmp && tmp->GetIsListMarkerPseudoElement())
			/* List marker pseudo element should be treated as integral
			   part of list item element and so should be skipped when
			   cleaning up. Also it is illegal to remove list marker element
			   without marking list item element extra dirty. */
			tmp = static_cast<HTML_Element*>(tmp->NextSibling());
	}

	while(tmp)
	{
		if (!include_start_stop && tmp == stop_elm)
			break;
		if (stop_elm->Precedes(tmp) && !stop_elm->IsAncestorOf(tmp))
			break;

		HTML_Element* next_elm = tmp->Next();

		if (next_elm && next_elm->GetIsListMarkerPseudoElement())
			/* List marker pseudo element should be treated as integral
			   part of list item element. */
			next_elm = static_cast<HTML_Element*>(next_elm->NextSibling());

		// If it is a empty textelement we will delete it, but we must also continue with its parents since
		// they may become empty.

		if (tidy_level != TIDY_LEVEL_MINIMAL)
		{
			HTML_Element* tmpremove = tmp;
			while((((!tmpremove->FirstChild() || (tmpremove->FirstChild()->GetIsListMarkerPseudoElement() && !tmpremove->FirstChild()->Suc()))
					&& !IsStandaloneElement(tmpremove) && !KeepWhenTidyIfEmpty(tmpremove)) ||
					// Remove inserted elements so that generated branches won't be duplicated.
					(tmpremove->GetInserted() == HE_INSERTED_BY_LAYOUT && tmpremove->Parent() && tmpremove->Parent()->GetIsPseudoElement()) ||
					 // Remove empty textelements that has suc or pred. Other must be keept as dummy for caret placement.
					(tmpremove->Type() == HE_TEXT && tmpremove->GetTextContentLength() == 0 &&
					(tidy_level == TIDY_LEVEL_AGGRESSIVE || (tmpremove->Suc() && tmpremove->Suc()->Type() == HE_TEXT) || (tmpremove->Pred() && tmpremove->Pred()->Type() == HE_TEXT)))))
			{
				if(tmpremove == shared_containing_elm)
				{
					next_elm = NULL; // No next element, we're finished!
					break;
				}
				if (KeepWhenTidy(tmpremove))
				{
					// We should not remove this element.
					if (!IsNoTextContainer(tmpremove))
					{
						// Create a empty textelement for the caret.
						HTML_Element *new_content = NewTextElement(UNI_L(""), 0); // FIXME: OOM
						new_content->UnderSafe(m_doc, tmpremove);
					}
					break;
				}

				HTML_Element* tmpremove_parent = tmpremove->ParentActual();
				if (tmpremove->IsAncestorOf(next_elm))
				{
					HTML_Element* potential_next_elm = tmpremove_parent->Next();
					if (shared_containing_elm->IsAncestorOf(potential_next_elm))
						next_elm = potential_next_elm;
					else
						next_elm = NULL;
				}

				DeleteElement(tmpremove);
				tmp = NULL;

				if (tmpremove == stop_elm)
					stop_elm = NULL;

				if (tmpremove == old_caret)
				{
					old_caret = NULL;
					m_caret.Set(NULL, 0);
				}

				// Abort if we are outside our permitted range (between start_elm and stop_elm)
				if (tmpremove == start_elm || (tmpremove_parent == start_elm && !include_start_stop) ||
					(tmpremove_parent != start_elm && tmpremove_parent->IsAncestorOf(start_elm)))
				{
					if (start_elm == tmpremove)
					{
						start_elm = next_elm ? next_elm : tmpremove_parent;
						if(!shared_containing_elm)
							break;
					}
					else
						break;
				}

				tmpremove = tmpremove_parent;
			}
		}

		if (tmp && IsDummyElement(tmp) && !keep_dummy)
		{
			SetElementText(tmp, UNI_L(""));
		}
		// Remove automatically inserted BR elements. (But not if it's needed!)
		else if (tmp && tmp->Type() == HE_BR &&
				GetInsertedAutomatically(tmp) &&
				m_doc->GetCaret()->GetContainingElementActual(tmp) &&
				!NeedAutoInsertedBR(m_doc->GetCaret()->GetContainingElementActual(tmp), TRUE))
		{
			if (tmp == stop_elm)
				stop_elm = NULL;
			if (tmp != start_elm)
			{
				DeleteElement(tmp);
				tmp = NULL;
			}
		}

		if (!stop_elm)
			break;

		tmp = next_elm;
	}

	// If empty there is nothing left, we have to create a empty textelement for the caret.
	if(old_caret && m_caret.GetElement() != old_caret)
	{
		if ((!m_caret.GetElement() || !IsElementValidForCaret(m_caret.GetElement(), TRUE, TRUE) || (!include_start_stop && !start_elm->FirstChild())) && !IsStandaloneElement(start_elm) && !IsNoTextContainer(start_elm))
		{
			// Try to find a valid element first, since we don't want empty textnodes we don't need.
			// Unfortunately FindEditableElement can't be used since it doesn't accept empty textnodes.
			//HTML_Element* caret_candidate = FindEditableElement(start_elm, TRUE, TRUE, FALSE, TRUE, FALSE);
			HTML_Element* caret_candidate = NULL;
			if (stop_elm)
			{
				caret_candidate = FindElementAfterOfType(start_elm, HE_TEXT);
				if (!caret_candidate || (stop_elm->Precedes(caret_candidate) && !stop_elm->IsAncestorOf(caret_candidate)))
					caret_candidate = FindElementAfterOfType(start_elm, HE_BR);
				if (!caret_candidate || (stop_elm->Precedes(caret_candidate) && !stop_elm->IsAncestorOf(caret_candidate)))
					caret_candidate = NULL;
			}
			if (!caret_candidate)
			{
				caret_candidate = NewTextElement(UNI_L(""), 0);
				if (caret_candidate)
					caret_candidate->UnderSafe(m_doc, start_elm);
			}
			if (caret_candidate && is_in_document)
				m_caret.Place(caret_candidate, 0);
		}
		if (!m_caret.GetElement())
			m_caret.PlaceFirst();
	}

#ifdef DEBUG_DOCUMENT_EDIT
	if (start_elm == stop_elm && !m_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(start_elm))
		DUMPDEBUGTREE_ELM(start_elm)
	else
		DUMPDEBUGTREE
#endif
}

void OpDocumentEdit::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	g_main_message_handler->UnsetCallBack(this, msg, par1);

	switch (msg)
	{
	case MSG_DOCEDIT_RECREATE_CARET:
		if (!m_caret.GetElement())
			m_caret.Init(TRUE, 0, FALSE);
		break;
	};
}
OP_STATUS OpDocumentEdit::PostRecreateCaretMessage()
{
	const unsigned long delay = 10;
	if (!g_main_message_handler->HasCallBack(this, MSG_DOCEDIT_RECREATE_CARET, (INTPTR) this))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_DOCEDIT_RECREATE_CARET, (INTPTR) this));
		RETURN_IF_ERROR(g_main_message_handler->PostDelayedMessage(MSG_DOCEDIT_RECREATE_CARET, (MH_PARAM_1) this, 0, delay));
	}
	return OpStatus::OK;
}

#endif // DOCUMENT_EDIT_SUPPORT
