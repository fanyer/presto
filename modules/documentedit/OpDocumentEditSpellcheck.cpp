/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(INTERNAL_SPELLCHECK_SUPPORT)

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/layout/layout_workplace.h"

void OpDocumentEdit::DisableSpellcheck()
{
	DisableSpellcheckInternal(TRUE /*by_user*/, TRUE /*force*/);
}

OpSpellCheckerWordIterator* OpDocumentEdit::GetAllContentIterator()
{
	if(m_background_updater.Active())
		return NULL;

	HTML_Element *first = GetBody(), *last = NULL;
	if(first && first->Type() == HE_BODY)
		last = first->LastLeafActual();
	if(!first || !last)
		return NULL;
	m_background_updater.SetRange(first, last);
	if(!m_background_updater.Active())
		return NULL;
	return &m_background_updater;
}

BOOL OpDocumentEdit::SetSpellCheckLanguage(const uni_char *lang)
{
	return OpStatus::IsSuccess(EnableSpellcheckInternal(TRUE /*by_user*/, lang));
}

BOOL OpDocumentEdit::HandleSpellCheckCommand(BOOL showui, const uni_char *value)
{
	int spell_session_id = 0;
	CreateSpellUISession(NULL, spell_session_id);
	OpSpellUiSessionImpl session(spell_session_id);
	return session.SetLanguage(value);
}

void OpDocumentEdit::OnSessionReady(OpSpellCheckerSession *session)
{
	OP_ASSERT(session == m_spell_session);
	HTML_Element *first = GetBody(), *last = NULL;
	if(first && first->Type() == HE_BODY)
		last = first->LastLeafActual();
	if(!first || !last)
		return;
	SpellCheckRange(first, last);
}

void OpDocumentEdit::OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string)
{
	OP_ASSERT(session == m_spell_session);
	DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
}

void OpDocumentEdit::DisableSpellcheckInternal(BOOL by_user, BOOL force)
{
	if(!force && !by_user && m_by_user)
		return;

	if(m_spell_session || by_user)
		m_by_user = by_user;

	if(!m_spell_session)
		return;

	m_word_iterator.Reset();
	m_replace_word.Reset();
	m_background_updater.Reset();
	OP_DELETE(m_spell_session);
	m_spell_session = NULL;
	if(m_has_spellchecked)
	{
		HTML_Element *root = GetRoot();
		HTML_Element *helm = root;
		while(helm)
		{
			if(helm->Type() == HE_TEXT && helm->GetLayoutBox() && helm->GetLayoutBox()->IsTextBox())
			{
				int i;
				Text_Box *box = (Text_Box*)helm->GetLayoutBox();
				WordInfo *words = box->GetWords();
				for(i=0;i<box->GetWordCount();i++)
					words[i].SetIsMisspelling(FALSE);
			}
			helm = helm->NextActual();
		}
		if(root)
			RepaintElement(root);
	}
	m_has_spellchecked = FALSE;
	m_last_helm_spelled = NULL;
}

OP_STATUS OpDocumentEdit::EnableSpellcheckInternal(BOOL by_user, const uni_char* lang)
{
	if(!by_user && m_by_user)
		return OpStatus::OK;

	OP_ASSERT(!m_spell_session);
	OP_STATUS status = g_internal_spellcheck->CreateSession(lang, this, m_spell_session, m_blocking_spellcheck);
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(m_spell_session == NULL);
		by_user = FALSE; // Remaining disabled is not what the user wanted.
	}
	m_by_user = by_user;

	return status;
}


OP_STATUS OpDocumentEdit::SpellCheckRange(HTML_Element *first, HTML_Element *last)
{
	OP_ASSERT(!m_word_iterator.Active() && m_spell_session);
	OP_STATUS status = OpStatus::OK;
	m_word_iterator.SetRange(first, last);
	if(!m_word_iterator.Active())
		return status;
	if(OpStatus::IsError(status = m_spell_session->CheckText(&m_word_iterator, FALSE)))
		DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/);
	return status;
}

void OpDocumentEdit::RepaintElement(HTML_Element *helm)
{
	if(!helm || !helm->GetLayoutBox() || !m_doc || !m_doc->GetVisualDevice() || m_doc->IsReflowing() || m_doc->IsUndisplaying())
		return;
	if (m_doc->GetLogicalDocument() &&
	    m_doc->GetLogicalDocument()->GetRoot() &&
	    m_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(helm))
	{
		RECT box_rect;
		helm->GetLayoutBox()->GetRect(m_doc,CONTENT_BOX,box_rect);
		OpRect update_rect = OpRect(box_rect.left, box_rect.top, box_rect.right-box_rect.left, box_rect.bottom-box_rect.top);
		m_doc->GetVisualDevice()->Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);
	}
}

OP_STATUS OpDocumentEdit::MarkMisspellingInTextElement(BOOL misspelled, HTML_WordIterator *word, HTML_Element *helm, int start_ofs, int stop_ofs)
{
	if(!helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox())
		return OpStatus::OK;
	Text_Box *box = (Text_Box*)helm->GetLayoutBox();
	if(!box->GetWords() || !box->GetWordCount())
		return OpStatus::OK;
	int i;
	WordInfo *words = box->GetWords();
	BOOL update = FALSE;
	BOOL spell_iterator = word == &m_word_iterator;
	int start_idx = 0;
	if(spell_iterator)
	{
		if(helm != m_last_helm_spelled)
		{
			if(m_last_helm_spelled)
				RepaintElement(m_last_helm_spelled);
			m_last_helm_spelled = helm;		
			m_last_helm_spelled_needs_update = FALSE;
			m_next_wi_index_to_spell = 0;
		}
		start_idx = m_next_wi_index_to_spell;
	}
	int word_count = box->GetWordCount();
	for(i=start_idx;i<word_count;i++)
	{
		if((int) words[i].GetStart() >= start_ofs)
		{
			while(i < box->GetWordCount() && words[i].GetStart() < (UINT16)stop_ofs)
			{
				if(!words[i].IsMisspelling() != !misspelled)
				{
					update = TRUE;
					words[i].SetIsMisspelling(misspelled);
				}
				i++;
			}
			break;
		}
	}
	if(spell_iterator)
	{
		m_next_wi_index_to_spell = i;
		m_last_helm_spelled_needs_update = m_last_helm_spelled_needs_update || update;
	}
	else
		RepaintElement(helm);
	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::MarkNodesMisspelled(BOOL misspelled, HTML_WordIterator *word)
{
	HTML_Element *start_elm = word->CurrentStartHelm();
	HTML_Element *stop_elm = word->CurrentStopHelm();
	int start_ofs = word->CurrentStartOfs();
	int stop_ofs = word->CurrentStopOfs();
	if(!start_elm)
		return OpStatus::OK;

	HTML_Element *helm = start_elm;
	while(helm)
	{
		if(helm->Type() == HE_TEXT && helm->GetTextContentLength())
		{
			int _stop_ofs = helm == stop_elm ? stop_ofs : helm->GetTextContentLength();
			RETURN_IF_ERROR(MarkMisspellingInTextElement(misspelled, word, helm, start_ofs, _stop_ofs));
		}
		start_ofs = 0;
		if(helm == stop_elm)
			break;
		helm = helm->NextActual();
	}
	return OpStatus::OK;
}

void OpDocumentEdit::OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word)
{
	HTML_WordIterator *w = (HTML_WordIterator*)word;
	MarkNodesMisspelled(FALSE, w);
}

void OpDocumentEdit::OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements)
{
	m_has_spellchecked = TRUE;
	HTML_WordIterator *w = (HTML_WordIterator*)word;
	MarkNodesMisspelled(TRUE, w);
}

void OpDocumentEdit::RunPendingSpellCheck()
{
	if(!m_spell_session || !m_pending_spell_first || m_word_iterator.Active())
		return;

	HTML_Element *first = m_pending_spell_first, *last = m_pending_spell_last;
	m_pending_spell_first = NULL;
	m_pending_spell_last = NULL;
	SpellCheckRange(first, last);
}

void OpDocumentEdit::OnSpellcheckerStopped()
{
	if(m_last_helm_spelled && m_last_helm_spelled_needs_update && m_doc)
		RepaintElement(m_last_helm_spelled);
	m_last_helm_spelled = NULL;
}

void OpDocumentEdit::OnCheckingComplete(OpSpellCheckerSession *session)
{
	if(m_last_helm_spelled && m_last_helm_spelled_needs_update)
		RepaintElement(m_last_helm_spelled);
	m_last_helm_spelled = NULL;
	OP_ASSERT(!m_word_iterator.Active());
	RunPendingSpellCheck();
}

void OpDocumentEdit::OnCheckingTakesABreak(OpSpellCheckerSession *session)
{
	if(m_last_helm_spelled && m_last_helm_spelled_needs_update)
		RepaintElement(m_last_helm_spelled);
	m_last_helm_spelled = NULL;
}

void MoveSpellRangeOutOf(HTML_Element *parent, HTML_Element *&first, HTML_Element *&last)
{
	if(!first)
		return;
	OP_ASSERT(last);
	BOOL first_ancestor = parent->IsAncestorOf(first);
	BOOL last_ancestor = parent->IsAncestorOf(last);
	if(!first_ancestor && !last_ancestor)
		return;
	if(first_ancestor && last_ancestor)
	{
		first = NULL;
		last = NULL;
		return;
	}
	if(first_ancestor)
	{
		first = (HTML_Element*)parent->NextSibling();
		while(first && first != last && !first->IsIncludedActual())
			first = first->Next();
	}
	else // last_ancestor
	{
		last = parent->Prev();
		while(last && last != first && !last->IsIncludedActual())
			last = last->Prev();
	}
	if(!first || !last)
	{
		OP_ASSERT(FALSE);
		first = NULL;
		last = NULL;
	}
}

void OpDocumentEdit::SpellInvalidateAround(HTML_Element *helm, BOOL must_be_outside_helm)
{
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY ||
		!GetRoot() || !GetRoot()->IsAncestorOf(helm))
	{
		return;
	}
	int i, len;
	const uni_char *content;
	HTML_Element *first = NULL, *last = NULL;
	HTML_Element *tmp = (HTML_Element*)helm->PrevSibling();
	while(tmp)
	{
		if(!IsFriendlyElement(tmp))
			break;
		if(tmp->IsIncludedActual() && tmp->Type() == HE_TEXT && 
			tmp->TextContent() && tmp->GetTextContentLength())
		{
			len = tmp->GetTextContentLength();
			content = tmp->TextContent();
			for(i=len-1;i >= 0 && !m_spell_session->IsWordSeparator(content[i]) ;i--) {}
			if(i != len-1) // tmp need spellcheck
				first = tmp;
			if(i >= 0)
				break;
			first = tmp;
		}
		tmp = tmp->LastChild() ? tmp->LastChild() : (HTML_Element*)tmp->PrevSibling();
	}
	tmp = (HTML_Element*)helm->NextSibling();
	while(tmp)
	{
		if(!IsFriendlyElement(tmp))
			break;
		if(tmp->IsIncludedActual() && tmp->Type() == HE_TEXT && 
			tmp->TextContent() && tmp->GetTextContentLength())
		{
			len = tmp->GetTextContentLength();
			content = tmp->TextContent();
			for(i=0;i < len && !m_spell_session->IsWordSeparator(content[i]) ;i++) {}
			if(i) // tmp need spellcheck
				last = tmp;
			if(i < len)
				break;
			last = tmp;
		}
		tmp = tmp->Next();
	}
	if(must_be_outside_helm)
	{
		MoveSpellRangeOutOf(helm, m_pending_spell_first, m_pending_spell_last);
		if(!first && !last)
			return;
		if(!first)
		{
			first = (HTML_Element*)helm->NextSibling();
			while(first && !first->IsIncludedActual())
				first = first->Next();
			if(!first)
			{
				OP_ASSERT(FALSE);
				return;
			}
		}
		if(!last)
		{
			last = helm->Prev();
			while(last && !last->IsIncludedActual())
				last = last->Prev();
			if(!last)
			{
				OP_ASSERT(FALSE);
				return;
			}
		}
	}
	else
	{
		if(!first)
		{
			first = helm;
			HTML_Element *stop = last ? last : (HTML_Element*)helm->NextSibling();
			while(first && first != stop && !first->IsIncludedActual())
				first = first->Next();
			if(first == stop && (stop != last || !last))
				return;
		}
		OP_ASSERT(first && first->IsIncludedActual());
		if(!last)
		{
			last = helm->LastLeaf();
			while(last && !last->IsIncludedActual())
				last = last->Prev();
			if(!last)
			{
				OP_ASSERT(FALSE);
				return;
			}
		}
	}
	if(!m_word_iterator.Active() && !m_begin_count)
	{
		SpellCheckRange(first, last);
		return;
	}
	else if (m_word_iterator.Active() && m_word_iterator.IsInRange(helm))
	{
		m_word_iterator.RestartFromBeginning();
		return;
	}
	if(!m_pending_spell_first)
	{
		m_pending_spell_first = first;
		m_pending_spell_last = last;
		return;
	}
	if(first->Precedes(m_pending_spell_first) || helm->IsAncestorOf(m_pending_spell_first))
		m_pending_spell_first = first;
	if(m_pending_spell_last->Precedes(last) || helm->IsAncestorOf(m_pending_spell_last))
		m_pending_spell_last = last;
}

OP_STATUS OpDocumentEdit::CreateSpellUISessionInternal(IntersectionObject *intersection, int &spell_session_id)
{
	spell_session_id = 0;
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY || !intersection)
		return OpStatus::OK;
	HTML_Element *helm = intersection->GetInnerBox() ? intersection->GetInnerBox()->GetHtmlElement() : NULL;
	if(!helm || helm->Type() != HE_TEXT || !intersection->GetWord())
		return OpStatus::OK;
	UINTPTR _ofs = (UINTPTR)(intersection->GetWord() - helm->TextContent());
	if(_ofs >= (UINTPTR)helm->GetTextContentLength())
		return OpStatus::OK;
	UINT32 ofs = (UINT32)_ofs;
	if(!helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox() || !helm->IsIncludedActual())
		return OpStatus::OK;
	if(m_spell_session->IsWordSeparator(helm->TextContent()[ofs]))
		return OpStatus::OK;
	m_replace_word.SetAtWord(helm, ofs);
	if(!*m_replace_word.GetCurrentWord())
		return OpStatus::OK;
	
	OpSpellCheckerSession temp_session(g_internal_spellcheck, m_spell_session->GetLanguage(), g_spell_ui_data, TRUE, m_spell_session->GetIgnoreWords());
	if(temp_session.CanSpellCheck(m_replace_word.GetCurrentWord()))
	{
		spell_session_id = g_spell_ui_data->CreateIdFor(this);
		RETURN_IF_ERROR(temp_session.CheckText(&m_replace_word, TRUE));
	}
	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::CreateSpellUISession(IntersectionObject *intersection, int &spell_session_id)
{
	OP_STATUS status = CreateSpellUISessionInternal(intersection, spell_session_id);
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

OP_STATUS OpDocumentEdit::ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *new_word)
{
	HTML_WordIterator *word = (HTML_WordIterator*)word_iterator;
	HTML_Element *start_helm = word->CurrentStartHelm();
	HTML_Element *stop_helm = word->CurrentStopHelm();
	int start_ofs = word->CurrentStartOfs();
	int stop_ofs = word->CurrentStopOfs();
	if(!GetValidCaretPosFrom(start_helm, start_ofs, start_helm, start_ofs))
		return OpStatus::OK;
	if(!GetValidCaretPosFrom(stop_helm, stop_ofs, stop_helm, stop_ofs))
		return OpStatus::OK;
	OldStyleTextSelectionPoint start;
	OldStyleTextSelectionPoint stop;
	start.SetLogicalPosition(start_helm, start_ofs);
	stop.SetLogicalPosition(stop_helm, stop_ofs);
	
	m_selection.Select(&start, &stop);
	RETURN_IF_ERROR(InsertText(new_word, uni_strlen(new_word)));
	return OpStatus::OK;
}

void OpDocumentEdit::OnBeforeNewCaretPos(HTML_Element *helm, int ofs)
{
	if(!m_spell_session || !m_delay_misspell_word_info)
		return;
	Text_Box *box = GetTextBox(helm);
	if(m_doc_has_changed || !box)
	{
		m_delay_misspell_word_info = NULL;
		RepaintElement(m_caret.GetElement());
		return;
	}
	WordInfo *w = box->GetWords();
	WordInfo *end = w + box->GetWordCount();
	for(; w != end; w++)
	{
		if(static_cast<int>(w->GetStart()) <= ofs && static_cast<int>(w->GetStart()) + static_cast<int>(w->GetLength()) >= ofs)
			break;
	}
	if(w == m_delay_misspell_word_info)
		return;
	
	m_delay_misspell_word_info = NULL;
	RepaintElement(m_caret.GetElement());
}

void OpDocumentEdit::DoSpellWordInfoUpdate(SpellWordInfoObject *old_info)
{
	if(!m_spell_session || !old_info->IsValid() || old_info->helm != m_caret.GetElement())
		return;
	Text_Box *box = GetTextBox(m_caret.GetElement());
	if(!box || box->GetWordCount() == old_info->word_count || m_caret.GetOffset() >= m_caret.GetElement()->GetTextContentLength())
		return;
	int i,j;
	WordInfo *wi = box->GetWords();
	int word_count = box->GetWordCount();
	int steps = word_count - old_info->word_count;
	if(!wi || !word_count)
		return;
	if(steps > 0)
	{
		for(i = word_count-1; i >= 0 ; i--)
		{
			if(i < steps || wi[i].GetStart() <= (UINT32)m_caret.GetOffset())
			{
				for(j = steps; i >= 0 && j > 0; j--, i--)
					wi[i].SetIsMisspelling(FALSE);
				break;
			}
			wi[i].SetIsMisspelling(wi[i-steps].IsMisspelling());
		}
	}
	else // steps < 0
	{
		for(i = 0; i < word_count && wi[i].GetStart() <= (UINT32)m_caret.GetOffset(); i++) {}
		for(;i - steps < word_count;i++)
			wi[i].SetIsMisspelling(wi[i-steps].IsMisspelling());
		wi[word_count-1].SetIsMisspelling(old_info->last_word_misspelled);
	}
}

void OpDocumentEdit::PossiblyDelayMisspell(BOOL was_delayed_misspell)
{
	HTML_Element *helm = m_caret.GetElement();
	int ofs = m_caret.GetOffset();
	if(!m_spell_session || !helm || helm->Type() != HE_TEXT || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox())
		return;

	Text_Box *box = (Text_Box*)helm->GetLayoutBox();
	WordInfo *w = box->GetWords();
	WordInfo *end = w + box->GetWordCount();
	for(; w != end; w++)
	{
		if(static_cast<int>(w->GetStart() + w->GetLength()) == ofs)
			break;
	}
	if(w == end || !w->GetLength() || !was_delayed_misspell && w->GetLength() != 1)
		return;
	const uni_char *txt = helm->TextContent();
	int txt_len = helm->GetTextContentLength();
	unsigned int i;
	for(i=0;i<w->GetLength();i++)
	{
		if(m_spell_session->IsWordSeparator(txt[w->GetStart()+i]))
			return;
	}
	BOOL start_ok = FALSE;
	if(w->GetStart())
	{
		if(m_spell_session->IsWordSeparator(txt[w->GetStart()-1]))
			start_ok = TRUE;
		else
			return;
	}

	if(static_cast<int>(w->GetStart()+w->GetLength()) < txt_len)
	{
		if(m_spell_session->IsWordSeparator(txt[w->GetStart()+w->GetLength()]))
		{
			if(start_ok)
			{
				m_delay_misspell_word_info = w;
				return;
			}
		}
		else
			return;
	}
	HTML_Element *tmp;
	if(!start_ok)
	{
		tmp = helm->Prev();
		while(tmp && IsFriendlyElement(tmp))
		{
			if(tmp->Type() == HE_TEXT && tmp->GetTextContentLength() && tmp->TextContent())
			{
				if(m_spell_session->IsWordSeparator(tmp->TextContent()[tmp->GetTextContentLength()-1]))
					break;
				else
					return;
			}
			tmp = tmp->Prev();
		}
	}
	else // start_ok
	{
		HTML_Element *container = m_doc->GetCaret()->GetContainingElement(helm);
		HTML_Element *stop_elm = container ? (HTML_Element*)container->NextSibling() : NULL;
		tmp = helm->Next();
		while(tmp && tmp != stop_elm && IsFriendlyElement(tmp))
		{
			if(tmp->Type() == HE_TEXT && tmp->GetTextContentLength() && tmp->TextContent())
			{
				if(m_spell_session->IsWordSeparator(tmp->TextContent()[0]))
					break;
				else
					return;
			}
			tmp = tmp->Next();
		}

	}
	m_delay_misspell_word_info = w;
}

//===================================HTML_WordIterator===================================

void HTML_WordIterator::Reset(BOOL stop_session)
{
	m_active = FALSE;
	m_been_used = FALSE;
	m_single_word = FALSE;
	m_current_start = NULL;
	m_current_stop = NULL;
	m_editable_container = NULL;
	m_after_editable_container = NULL;
	m_current_start_ofs = 0;
	m_current_stop_ofs = 0;
	m_first = NULL;
	m_last = NULL;
	if(m_current_string.CStr())
		m_current_string.Set(UNI_L(""));
	if(stop_session && m_used_by_session)
	{
		OpSpellCheckerSession *session = m_edit->GetSpellCheckerSession();
		if(session)
		{
			m_edit->OnSpellcheckerStopped();
			session->StopCurrentSpellChecking();
		}
	}
}

HTML_WordIterator::HTML_WordIterator(OpDocumentEdit *edit, BOOL used_by_session, BOOL mark_separators) : OpDocumentEditInternalEventListener()
{
	m_edit = edit;
	m_used_by_session = used_by_session;
	m_mark_separators = mark_separators;
	Reset();
}

HTML_WordIterator::~HTML_WordIterator()
{
}

void HTML_WordIterator::SetRange(HTML_Element *first, HTML_Element *last)
{
	Reset();
	m_editable_container = m_edit->GetEditableContainer(first);
	if(!m_editable_container)
	{
		if(!m_edit->GetDoc() || m_edit->GetDoc()->GetDesignMode())
		{
			OP_ASSERT(FALSE);
			return;
		}
		while(first && first != last && !first->IsContentEditable(FALSE))
			first = first->NextActual();
		if(!first || first == last)
			return;
		m_editable_container = first;
	}
	m_after_editable_container = m_editable_container->NextSiblingActual();
	m_edit->AddInternalEventListener(this);
	m_first = first;
	m_last = last;
	m_active = TRUE;
	m_current_start = m_first;
	m_current_stop = m_first;
	ContinueIteration();	
}

void HTML_WordIterator::SetAtWord(HTML_Element *helm, int ofs)
{
	Reset();
	HTML_Element *editable_container = m_edit->GetEditableContainer(helm);
	if(!editable_container)
		return;
	OpSpellCheckerSession *session = m_edit->GetSpellCheckerSession();
	if(session->IsWordSeparator(helm->TextContent()[ofs]))
		return;

	HTML_Element *tmp = helm;
	unsigned tmp_ofs = ofs;

	HTML_Element* stop = m_edit->GetDoc()->GetCaret()->GetContainingElement(helm);
	if (!stop || stop->IsAncestorOf(editable_container))
		stop = editable_container;
	while(tmp)
	{
		const uni_char* txt=tmp->Content();
		while(tmp_ofs > 0 && !session->IsWordSeparator(txt[tmp_ofs-1]))
			tmp_ofs--;
		if(tmp_ofs != tmp->ContentLength())
		{
			helm = tmp;
			ofs = tmp_ofs;
		}
		if(tmp_ofs || tmp==stop)
			break;

		do
		{
			tmp = tmp->PrevActual();
			if(!tmp || !m_edit->IsFriendlyElement(tmp))
			{
				tmp = NULL;
				break;
			}
		} while(!tmp->ContentLength() && tmp!=stop);
		if(tmp)
			tmp_ofs = tmp->ContentLength();
	}
	if (helm->SpellcheckEnabledByAttr() < 0)
		return;

	m_editable_container = editable_container;
	m_after_editable_container = m_editable_container->NextSiblingActual();
	m_edit->AddInternalEventListener(this);
	m_active = TRUE;
	m_current_start = helm;
	m_current_stop = helm;
	m_current_start_ofs = ofs;
	m_current_stop_ofs = ofs;
	m_first = helm;
	m_last = m_editable_container->LastLeafActual(); // or maybe it could be NULL...
	ContinueIteration();
	if(m_active)
		m_single_word = TRUE;
}

BOOL HTML_WordIterator::FindStartOfWord()
{
	OpSpellCheckerSession *session = m_edit->m_spell_session;
	HTML_Element *helm = m_current_stop;
	int ofs = m_current_stop_ofs;

	m_current_start = m_current_stop;
	m_current_start_ofs = m_current_stop_ofs;

	OP_ASSERT(helm != m_after_editable_container);

	BOOL start_found = FALSE;
	while(helm)
	{

		//FIXME: Account for first letter pseudo element?
		const uni_char* txt = helm->Content();
		int txt_len = helm->ContentLength();
		OP_ASSERT(ofs <= txt_len);
		for(;ofs<txt_len && session->IsWordSeparator(txt[ofs]);ofs++) {}
		m_current_stop = helm;
		m_current_stop_ofs = ofs;
		if(ofs != txt_len)
		{
			start_found = TRUE;
			break;
		}
		if (helm == m_last)
			break;

		ofs = 0;
		helm = helm->NextActual();
		if(helm == m_after_editable_container)
		{
			if(!m_edit->GetDoc() || m_edit->GetDoc()->GetDesignMode())
				break;

			while(helm && helm != m_last && !helm->IsContentEditable(FALSE))
				helm = helm->NextActual();
			if(!helm || !helm->IsContentEditable(FALSE))
				break;

			m_editable_container = helm;
			m_after_editable_container = helm->NextSiblingActual();
		}
	}

	if(m_mark_separators)
		m_edit->MarkNodesMisspelled(FALSE, this);
	m_current_start = m_current_stop;
	m_current_start_ofs = m_current_stop_ofs;
	return start_found;
}

BOOL HTML_WordIterator::ReadWord(OpString* toString)
{
	int i;

	OpSpellCheckerSession *session = m_edit->m_spell_session;

	HTML_Element *helm = m_current_stop;
	int ofs = m_current_stop_ofs;

	HTML_Element *stop = m_edit->GetDoc()->GetCaret()->GetContainingElement(helm);
	if(stop && !stop->IsAncestorOf(m_after_editable_container))
		stop = stop->NextSiblingActual();
	else
		stop = m_after_editable_container;

	while(helm)
	{
		const uni_char* txt = helm->Content();
		int txt_len = helm->ContentLength();
		for(i = ofs; i<txt_len && !session->IsWordSeparator(txt[i]); i++) {}
		if(toString && OpStatus::IsError(toString->Append(txt+ofs, i-ofs)))
		{
			m_current_stop = helm;
			m_current_stop_ofs = i;
			if(m_mark_separators)
				m_edit->MarkNodesMisspelled(FALSE, this);
			Reset(!m_been_used);
			return FALSE;
		}
		ofs = i;
		if(ofs != txt_len)
			break; // we found a word separator and are finished
		if(helm == m_last)
			break;

		HTML_Element *next = helm->NextActual();

		// Skip past elements with no text in them.
		while(next && next != stop)
		{
			if(next->ContentLength())
				break;

			if(next == m_last || !m_edit->IsFriendlyElement(next))
			{
				next = stop;
				break;
			}
			next = next->NextActual();
		}
		if(next == stop)
			break;
		helm = next;
		ofs = 0;
	}
	m_current_stop = helm;
	m_current_stop_ofs = ofs;
	if(!toString && m_mark_separators)
		m_edit->MarkNodesMisspelled(FALSE, this);
	return TRUE;
}


BOOL HTML_WordIterator::ContinueIteration()
{
	if(!m_active || m_single_word)
		return FALSE;

	m_current_string.Empty();

	BOOL word_found = FALSE;
	while (!word_found)
	{
		if(!FindStartOfWord())
			break;

		word_found = m_current_stop->SpellcheckEnabledByAttr() > 0;

		if(!word_found)
			ReadWord(NULL);
	}

	if (!word_found)
	{
		Reset(!m_been_used);
		return FALSE;
	}

	if (!ReadWord(&m_current_string))
		return FALSE;

	OP_ASSERT(!m_current_string.IsEmpty());
	return TRUE;
}

void HTML_WordIterator::RestartFromBeginning()
{
	RestartFrom(m_first);
}

void HTML_WordIterator::RestartFrom(HTML_Element *helm)
{
	if(m_single_word)
	{ // the single word we where supposed to hold got messed up...
		Reset();
		return;
	}
	//FIXME: What if helm is in the middle of a word.
	m_current_start = helm;
	m_current_stop = helm;
	m_current_start_ofs = 0;
	m_current_stop_ofs = 0;
	if(m_current_string.CStr())
		m_current_string.Set(UNI_L(""));
}

void HTML_WordIterator::OnElementOut(HTML_Element *helm)
{
	if(!m_active)
		return;
	BOOL first_ancestor = helm->IsAncestorOf(m_first);
	BOOL last_ancestor = helm->IsAncestorOf(m_last);
	if(helm->IsAncestorOf(m_editable_container) || first_ancestor && last_ancestor)
	{
		Reset();
		return;
	}
	if(helm->IsAncestorOf(m_after_editable_container))
		m_after_editable_container = helm->NextSiblingActual();
	if(first_ancestor)
		m_first = helm->NextSiblingActual();
	if(last_ancestor)
		m_last = helm->PrevActual();
	if(!m_first || !m_last)
	{
		OP_ASSERT(!"should not be possible");
		Reset();
		return;
	}
	if(helm->IsAncestorOf(m_current_start))
	{
		if(first_ancestor)
			RestartFrom(m_first);
		else
		{
			helm = helm->PrevActual();
			if(!helm)
			{
				OP_ASSERT(!"should not be possible");
				Reset();
				return;
			}
			RestartFrom(helm);
		}
		return;
	}
	BOOL restart_from_start = helm->IsAncestorOf(m_current_stop);
	if(!restart_from_start)
	{
		HTML_Element *tmp = m_current_start;
		while(tmp && tmp != m_current_stop)
		{
			if(tmp == helm)
			{
				restart_from_start = TRUE;
				break;
			}
			tmp = tmp->NextActual();
		}
		if(!tmp)
		{
			OP_ASSERT(FALSE);
			Reset();
			return;
		}
	}
	if(restart_from_start)
		RestartFrom(m_current_start);
}

void HTML_WordIterator::OnElementInserted(HTML_Element *helm)
{
	if(!m_active)
		return;
	if(m_last->IsAncestorOf(helm))
	{
		m_last = m_last->LastLeaf();
		while(m_last != m_last && (m_last->Type() != HE_TEXT || !m_last->IsIncludedActual()))
			m_last = m_last->Prev();
	}
}

void HTML_WordIterator::OnTextConvertedToTextGroup(HTML_Element* elm)
{
	// Our pointers are to text element, not to textgroups. If a text element
	// changes we need to modify the pointers as well.
	if (m_current_start == elm)
		m_current_start = elm->FirstChild();

	if (m_current_stop == elm)
		m_current_stop = elm->FirstChild();

	if (m_first == elm)
		m_first = elm->FirstChild();

	if (m_last == elm)
		m_last = elm->FirstChild();
}

BOOL HTML_WordIterator::IsInRange(HTML_Element* elm)
{
	if (!m_last || !m_first)
		return FALSE;

	HTML_Element* iter = m_first;
	while (iter && iter != m_last->NextActual())
	{
		if (iter == elm)
			return TRUE;

		iter = iter->NextActual();
	}

	return FALSE;
}

#endif // DOCUMENT_EDIT_SUPPORT && INTERNAL_SPELLCHECK_SUPPORT
