/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/display/coreview/coreview.h"
#include "modules/logdoc/src/textdata.h"

#ifdef DOCUMENT_EDIT_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
#ifndef DOCUMENTEDIT_DISABLE_IME_SUPPORT

IM_WIDGETINFO OpDocumentEdit::GetIMInfo()
{
	VisualDevice* vd = m_doc->GetVisualDevice();
	IM_WIDGETINFO info;
	info.font = NULL;
	info.rect = OpRect(m_doc->GetCaretPainter()->GetX() - vd->GetRenderingViewX(), m_doc->GetCaretPainter()->GetY() - vd->GetRenderingViewY(), 0, m_doc->GetCaretPainter()->GetHeight());
	// Scale
	info.rect = vd->ScaleToScreen(info.rect);
	// Make relative to screen
	OpPoint screenpos(0, 0);
	screenpos = vd->GetView()->ConvertToScreen(screenpos);
	info.rect.OffsetBy(screenpos);
	info.is_multiline = FALSE;
	return info;
}

HTML_Element* OpDocumentEdit::BuildInputMethodStringElement()
{
	// Create a span with red bottomborder
	HTML_Element* ime_string_elm = NewElement(HE_SPAN);
	HTML_Element* text_elm = NewTextElement(m_imstring->Get(), uni_strlen(m_imstring->Get()));

	if (!ime_string_elm || !text_elm)
	{
		DeleteElement(ime_string_elm);
		DeleteElement(text_elm);
		return NULL;
	}

	ime_string_elm->SetInserted(HE_INSERTED_BY_IME);
	ime_string_elm->SetIMEStyling(1); // Will get the underlining property
	text_elm->UnderSafe(m_doc, ime_string_elm);

	if (m_imstring->GetCandidateLength())
	{
		SplitElement(text_elm, m_imstring->GetCandidatePos() + m_imstring->GetCandidateLength());
		BOOL split = SplitElement(text_elm, m_imstring->GetCandidatePos());

		HTML_Element* candidate_span = NewElement(HE_SPAN);
		if (!candidate_span)
		{
			DeleteElement(ime_string_elm);
			DeleteElement(text_elm);
			return NULL;
		}

		candidate_span->SetInserted(HE_INSERTED_BY_IME);
		candidate_span->SetIMEStyling(2); // Will get the candidate styling

		HTML_Element* candidate_element = ime_string_elm->FirstChild();
		if (split)
			candidate_element = candidate_element->Suc();

		candidate_span->PrecedeSafe(m_doc, candidate_element);
		candidate_element->OutSafe(m_doc, FALSE);
		candidate_element->UnderSafe(m_doc, candidate_span);
	}
	return ime_string_elm;
}

static HTML_Element* GetImeStringTextElement(HTML_Element *root, HTML_Element **ime_elm)
{
	HTML_Element *elm = root;
	HTML_Element *stop = root->NextSibling();
	while (elm != stop && elm->GetInserted() != HE_INSERTED_BY_IME)
		elm = elm->Next();
	if (elm && elm->GetInserted() == HE_INSERTED_BY_IME)
	{
		if (ime_elm)
			*ime_elm = elm;
		elm = elm->FirstChildActual();
		if (elm && elm->Type() == HE_TEXT)
			return elm;
	}
	return NULL;
}

void
OpDocumentEdit::EmptyInputMethodStringElement(BOOL remove)
{
	if (m_ime_string_elm)
	{
		OP_ASSERT(m_caret.GetElement());

		HTML_Element *ime_elm;
		HTML_Element *elm = GetImeStringTextElement(m_ime_string_elm, &ime_elm);
		if (elm)
		{
			elm->SetText(m_doc, UNI_L(""), 0, HTML_Element::MODIFICATION_DELETING);
			if (elm == m_caret.GetElement())
				m_caret.Place(elm, 0);
			if (remove)
			{
				elm->Out();
				if (ime_elm->Suc())
					elm->Precede(ime_elm->Suc());
				else
					elm->Under(ime_elm->Parent());
				ime_elm->Remove(m_doc);
				if (ime_elm->Clean())
					ime_elm->Free(m_doc);
			}
		}

		ReflowAndUpdate();

		OP_ASSERT(m_caret.GetElement() || m_caret.m_parent_candidate);
		if (!m_caret.GetElement())
			m_caret.Init(TRUE);
	}
}

IM_WIDGETINFO OpDocumentEdit::OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose)
{
	OP_ASSERT(!m_ime_string_elm);
	if (m_readonly)
		return GetIMInfo();
	im_waiting_first_compose = TRUE;
	m_imstring = imstring;
	return GetIMInfo();
}

IM_WIDGETINFO OpDocumentEdit::OnCompose()
{
	OP_ASSERT(m_imstring);
	if (m_imstring == NULL)
		return GetIMInfo();

	CheckLogTreeChanged();

	BOOL already_inserted = !!m_ime_string_elm;

	EmptyInputMethodStringElement();

	if (already_inserted)
	{
		HTML_Element *elm = GetImeStringTextElement(m_ime_string_elm, NULL);
		if (elm)
			elm->SetText(m_doc, m_imstring->Get(), uni_strlen(m_imstring->Get()));
	}
	else
		m_ime_string_elm = BuildInputMethodStringElement();

	if (!m_ime_string_elm)
	{
		ReportOOM();
		m_imstring = NULL;
		return GetIMInfo();
	}
	
	// Do not include this deletion action in the autogroup,or it will be undo in a following OnCompose message
	if(im_waiting_first_compose)
	{
		if (m_selection.HasContent())
			m_selection.RemoveContent();
		im_waiting_first_compose = FALSE;
	}

	OpDocumentEditDisableUndoRegistrationAuto disable(&m_undo_stack);
	
	// Since we are using up the pending styles in InsertElement, we must clone them and add them again to make
	// the next OnCompose use the styles again.
	Head pending_styles;
	if (OpStatus::IsMemoryError(ClonePendingStyles(pending_styles)))
		ReportOOM();


	m_caret.LockUpdatePos(TRUE);

	// Mark clean. Workaround for the fact that dirty elements that is inserted doesn't (and won't) mark parents dirty.
	m_ime_string_elm->MarkClean();

	HTML_Element* outmost_style;
	InsertPendingStyles(&outmost_style);
	if (!already_inserted)
	{
		InsertElement(m_ime_string_elm);
		if (m_ime_string_elm->Parent() == NULL)
		{
			m_ime_string_elm->Remove(m_doc);
			if (m_ime_string_elm->Clean())
				m_ime_string_elm->Free(m_doc);
			m_ime_string_elm = NULL;
		}
	}

	m_caret.Place(FindElementAfterOfType(m_ime_string_elm, HE_TEXT, FALSE), m_imstring->GetCaretPos());

	if (outmost_style)
		m_ime_string_elm = outmost_style;

	m_caret.LockUpdatePos(FALSE);
	ScrollIfNeeded();

	AddPendingStyles(pending_styles);

	return GetIMInfo();
}

IM_WIDGETINFO OpDocumentEdit::GetWidgetInfo()
{
	return GetIMInfo();
}

void OpDocumentEdit::OnCommitResult()
{
	if (m_imstring == NULL)
		return;

	CheckLogTreeChanged();

	EmptyInputMethodStringElement(TRUE);

	OP_STATUS status = InsertText(m_imstring->Get(), uni_strlen(m_imstring->Get()));
	if (OpStatus::IsMemoryError(status))
		ReportOOM();

	m_ime_string_elm = NULL;
	im_waiting_first_compose = FALSE;
}

void OpDocumentEdit::OnStopComposing(BOOL canceled)
{
	OP_ASSERT(m_imstring);
	if (m_imstring == NULL)
		return;

	CheckLogTreeChanged();

	if (IsImComposing())
	{
		if(canceled)
			EmptyInputMethodStringElement(TRUE);
		else
			OnCommitResult();
	}
	m_ime_string_elm = NULL;

	m_undo_stack.Clear(FALSE, TRUE);

	im_waiting_first_compose = FALSE;
	m_imstring = NULL;
}

#endif // DOCUMENTEDIT_DISABLE_IME_SUPPORT

#ifdef IME_RECONVERT_SUPPORT
void OpDocumentEdit::OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop)
{
	// get selection
	SelectionBoundaryPoint anchor,focus;
	this->GetSelection(anchor,focus);

	HTML_Element *start = anchor.GetElement()
		,*stop = focus.GetElement();

	int start_ofs = anchor.GetElementCharacterOffset(),
		stop_ofs = focus.GetElementCharacterOffset();

	// make sure start precedes stop
	if ( stop->Precedes(start) )
	{
		HTML_Element* tmp = start;
		start = stop;
		stop = tmp;

		int itmp = start_ofs;
		start_ofs = stop_ofs;
		stop_ofs = itmp;
	}

	// move to first text element
	HTML_Element* iter = start;
	while (!iter->IsText() && iter != stop)
	{
		iter = iter->Suc();
	}

	// set str to the whole element.
	if ( iter->IsText() )
	{
		str = iter->GetTextData()->GetText();
		if ( iter == start )
			sel_start = start_ofs;
		else
			sel_start = 0;

		if (iter == stop)
			sel_stop = stop_ofs;
		else
			sel_stop = iter->GetTextData()->GetTextLen();
	}
}

void OpDocumentEdit::OnReconvertRange(int sel_start, int sel_stop)
{
	// get selection
	SelectionBoundaryPoint anchor,focus;
	this->GetSelection(anchor,focus);

	HTML_Element *start = anchor.GetElement()
		,*stop = focus.GetElement();

	int start_ofs = anchor.GetElementCharacterOffset(),
		stop_ofs = focus.GetElementCharacterOffset();

	// make sure start precedes stop
	if ( stop->Precedes(start) )
	{
		HTML_Element* tmp = start;
		start = stop;
		stop = tmp;

		int itmp = start_ofs;
		start_ofs = stop_ofs;
		stop_ofs = itmp;
	}

	// move to first text element
	HTML_Element* iter = start;
	while (!iter->IsText() && iter != stop)
	{
		iter = iter->Suc();
	}

	OP_ASSERT(iter->IsText());

	// set selection and caret
	OldStyleTextSelectionPoint txt_sel_start,txt_sel_stop;
	txt_sel_start.SetLogicalPosition(iter,sel_start);
	txt_sel_stop.SetLogicalPosition(iter,sel_stop);

	m_selection.Select(&txt_sel_stop,&txt_sel_start);
}
#endif // IME_RECONVERT_SUPPORT
#endif // WIDGETS_IME_SUPPORT
#endif // DOCUMENT_EDIT_SUPPORT
