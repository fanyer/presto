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

#ifdef _DOCEDIT_DEBUG

#include "modules/documentedit/OpDocumentEditUndoRedo.h"

void OpDocumentEditDebugChecker::UpdateCallerInfo(void *call_addr)
{
	// To be implemented...
}

OpDocumentEditDebugChecker::OpDocumentEditDebugChecker(OpDocumentEdit *edit, OpDocumentEditDebugCheckerObject *obj, BOOL check_begin_count, BOOL static_context)
{
#define __CALL_ADDRESS__ ((void**)(&edit))[-1]
	m_static_context = static_context;
	if(!static_context)
	{
		m_edit = edit;
		m_obj = obj;
		if(!m_edit || !obj)
		{
			OP_ASSERT(FALSE);
			m_check_begin_count = 0;
			return;
		}
		m_check_begin_count = obj->CheckBeginCount() && check_begin_count;
		if(m_check_begin_count)
			m_begin_count = edit->m_undo_stack.GetBeginCount();
	}
	UpdateCallerInfo(__CALL_ADDRESS__);
}
OpDocumentEditDebugChecker::~OpDocumentEditDebugChecker()
{
	if(!m_static_context)
	{
		if(m_check_begin_count)
			OP_ASSERT(m_edit->m_undo_stack.GetBeginCount() == m_begin_count);
	}
}

#endif // _DOCEDIT_DEBUG

Text_Box *GetTextBox(HTML_Element *helm)
{
	if(!helm || helm->Type() != HE_TEXT || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox())
		return NULL;
	return (Text_Box*)helm->GetLayoutBox();
}

/** Returns TRUE if helm is a non-collapsed empty text-element. */
BOOL IsNonCollapsedEmptyTextElm(HTML_Element *helm)
{
	DEBUG_CHECKER_STATIC();
	Text_Box *box;
	if(!helm || helm->Type() != HE_TEXT || helm->GetTextContentLength() || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox())
		return FALSE;
	box = (Text_Box*)helm->GetLayoutBox();
	return box->GetWordCount() == 1 && box->GetWords() && !box->GetWords()->IsCollapsed();
}

OpDocumentEditAutoLink *OpDocumentEditAutoLink::GetLink(void *object, Head *head)
{
	if(!head)
		return NULL;

	OpDocumentEditAutoLink *link = (OpDocumentEditAutoLink*)head->First();
	while(link)
	{
		if(link->GetObject() == object)
			return link;
		link = (OpDocumentEditAutoLink*)link->Suc();
	}
	return NULL;
}

OpDocumentEditRangeKeeper::OpDocumentEditRangeKeeper(HTML_Element *container, HTML_Element *start_elm, HTML_Element *stop_elm, OpDocumentEdit *edit) : OpDocumentEditInternalEventListener()
{
	DEBUG_CHECKER_CONSTRUCTOR();
	m_pending_start_elm = m_pending_stop_elm = NULL;
	if(!edit || !container || !start_elm || !stop_elm || !container->IsAncestorOf(start_elm) || !container->IsAncestorOf(stop_elm))
	{
		OP_ASSERT(FALSE);
		MakeInvalid();
		return;
	}
	m_edit = edit;
	m_container = container;
	m_start_elm = start_elm;
	m_stop_elm = stop_elm;
	m_edit->AddInternalEventListener(this);
}

HTML_Element *OpDocumentEditRangeKeeper::AdjustInDirection(HTML_Element *helm, BOOL forward, BOOL only_allow_in_direction)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *org_helm = helm;
	if(forward)
	{
		HTML_Element *after_container = (HTML_Element*)m_container->NextSibling();
		helm = (HTML_Element*)helm->NextSibling();
		while(helm && helm != after_container && !helm->IsIncludedActual())
			helm = helm->Next();
		if(helm == after_container)
			helm = NULL;
	}
	else
	{
		helm = helm->Prev();
		while(helm && helm != m_container && !helm->IsIncludedActual())
			helm = helm->Prev();
	}
	return helm ? helm : (only_allow_in_direction ? NULL : AdjustInDirection(org_helm,!forward,TRUE));
}

void OpDocumentEditRangeKeeper::OnElementInserted(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	BOOL inserted_start_stop = FALSE;
	if(!helm || !IsValid())
		return;

	if(m_pending_start_elm && helm->IsAncestorOf(m_pending_start_elm) && m_container->IsAncestorOf(m_pending_start_elm))
	{
		inserted_start_stop = TRUE;
		m_start_elm = m_pending_start_elm;
		m_pending_start_elm = NULL;
	}
	if(m_pending_stop_elm && helm->IsAncestorOf(m_pending_stop_elm) && m_container->IsAncestorOf(m_pending_stop_elm))
	{
		inserted_start_stop = TRUE;
		m_stop_elm = m_pending_stop_elm;
		m_pending_stop_elm = NULL;
	}
	if(inserted_start_stop && m_stop_elm->Precedes(m_start_elm))
	{
		HTML_Element *tmp = m_start_elm;
		m_start_elm = m_stop_elm;
		m_stop_elm = tmp;
	}
}

void OpDocumentEditRangeKeeper::OnElementOut(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || !IsValid())
		return;

	BOOL is_start_ancestor = helm->IsAncestorOf(m_start_elm);
	BOOL is_stop_ancestor = helm->IsAncestorOf(m_stop_elm);
	BOOL is_container_ancestor = helm->IsAncestorOf(m_container);
	if((!is_start_ancestor && !is_stop_ancestor) || is_container_ancestor)
		return;

	if(is_start_ancestor)
	{
		if(!m_pending_start_elm)
			m_pending_start_elm = m_start_elm;
		m_start_elm = AdjustInDirection(helm,FALSE);
	}
	if(is_stop_ancestor)
	{
		if(!m_pending_stop_elm)
			m_pending_stop_elm = m_stop_elm;
		m_stop_elm = AdjustInDirection(helm,TRUE);
	}
}

void OpDocumentEditRangeKeeper::OnElementDeleted(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || !IsValid())
		return;

	if(helm->IsAncestorOf(m_container))
	{
		MakeInvalid();
		return;
	}
	if(helm->IsAncestorOf(m_start_elm))
	{
		OP_ASSERT(FALSE); // We seems to have missed OnElementOut!
		m_start_elm = m_container;
	}
	if(helm->IsAncestorOf(m_stop_elm))
	{
		OP_ASSERT(FALSE); // We seems to have missed OnElementOut!
		m_stop_elm = m_container;
	}

	if(m_pending_start_elm && helm->IsAncestorOf(m_pending_start_elm))
		m_pending_start_elm = NULL;
	if(m_pending_stop_elm && helm->IsAncestorOf(m_pending_stop_elm))
		m_pending_stop_elm = NULL;
}


OpDocumentEditWsPreserverContainer::OpDocumentEditWsPreserverContainer(OpDocumentEditWsPreserver *preserver, OpDocumentEdit *edit)
{
	m_preserver = preserver;
	m_edit = edit;
	m_edit->AddInternalEventListener(this);
}

void OpDocumentEditWsPreserverContainer::OnElementDeleted(HTML_Element *helm)
{
	if (helm == m_preserver->GetStartElement())
		m_preserver->SetStartElement(NULL);
	if (helm == m_preserver->GetStopElement())
		m_preserver->SetStopElement(NULL);
}

OpDocumentEditWsPreserver::OpDocumentEditWsPreserver(HTML_Element *start_elm, HTML_Element *stop_elm, int start_ofs, int stop_ofs, OpDocumentEdit *edit)
{
	DEBUG_CHECKER_CONSTRUCTOR();
	m_edit = edit;
	if(edit)
		SetRemoveRange(start_elm,stop_elm,start_ofs,stop_ofs);
	else
		ClearRange();
}

BOOL OpDocumentEditWsPreserver::SetRemoveRange(HTML_Element *start_elm, HTML_Element *stop_elm, int start_ofs, int stop_ofs, BOOL check_start)
{
	DEBUG_CHECKER(TRUE);
	if(!m_edit)
		return FALSE;

	if(!check_start)
		start_elm = NULL;
	start_ofs--;
	if(start_elm && (start_ofs < 0 || start_elm->Type() != HE_TEXT))
	{
		for(start_elm = start_elm->PrevActual(); start_elm; start_elm = start_elm->PrevActual())
		{
			if(start_elm->Type() == HE_TEXT && start_elm->GetTextContentLength())
			{
				start_ofs = start_elm->GetTextContentLength()-1;
				break;
			}
		}
	}
	if(stop_elm && (stop_ofs >= stop_elm->GetTextContentLength() || stop_elm->Type() != HE_TEXT))
	{
		stop_ofs = 0;
		for(stop_elm = stop_elm->NextActual(); stop_elm; stop_elm = stop_elm->NextActual())
			if(stop_elm->Type() == HE_TEXT && stop_elm->GetTextContentLength())
				break;
	}

	if(start_elm && (!start_elm->TextContent() || !uni_collapsing_sp(start_elm->TextContent()[start_ofs])))
		start_elm = NULL;
	if(stop_elm && (!stop_elm->TextContent() || !uni_collapsing_sp(stop_elm->TextContent()[stop_ofs])))
		stop_elm = NULL;

	BOOL check_stop_elm = stop_elm && start_elm != stop_elm;
	OP_STATUS status = OpStatus::OK;
	if(start_elm)
	{
		OpDocumentEditWordIterator iter(start_elm,m_edit);
		status = iter.GetStatus();
		m_start_was_collapsed = iter.IsCharCollapsed(start_ofs);
		if(start_elm == stop_elm)
			m_stop_was_collapsed = iter.IsCharCollapsed(stop_ofs);
	}
	if(OpStatus::IsSuccess(status) && check_stop_elm)
	{
		OpDocumentEditWordIterator iter(stop_elm,m_edit);
		status = iter.GetStatus();
		m_stop_was_collapsed = iter.IsCharCollapsed(stop_ofs);
	}

	m_start_elm = start_elm;
	m_stop_elm = stop_elm;
	m_start_ofs = start_ofs;
	if(stop_elm)
		m_stop_ofs = stop_elm->GetTextContentLength() - stop_ofs;

	if(OpStatus::IsError(status))
		m_start_elm = m_stop_elm = NULL;

	BOOL preserving = m_start_elm || m_stop_elm;
	if(preserving)
		m_edit->AddWsPreservingOperation(this);
	return preserving;
}

BOOL OpDocumentEditWsPreserver::GetCollapsedCountFrom(HTML_Element *helm, int ofs, int &found_count)
{
	DEBUG_CHECKER(TRUE);
	int i;
	found_count = 0;
	if(!helm || helm->Type() != HE_TEXT)
		return FALSE;
	int txt_len = helm->GetTextContentLength();
	if(!txt_len)
		return TRUE;
	OpDocumentEditWordIterator iter(helm,m_edit);
	if(OpStatus::IsError(iter.GetStatus()))
		return FALSE;
	for(i = ofs; i < txt_len && iter.IsCharCollapsed(i); i++, found_count++) {}
	return i >= txt_len;
}

HTML_Element *OpDocumentEditWsPreserver::GetNextFriendlyTextHelm(HTML_Element *helm)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *after = helm->NextActual();
	while(after)
	{
		if(after->Type() == HE_TEXT)
			return after;
		if(!m_edit->IsFriends(helm,after))
			return NULL;
		after = after->NextActual();
	}
	return NULL;
}

BOOL OpDocumentEditWsPreserver::DeleteCollapsedFrom(HTML_Element *helm, int ofs, BOOL insert_nbsp_first)
{
	DEBUG_CHECKER(TRUE);
	if(!helm || !m_edit)
		return FALSE;

	OpString str;
	HTML_Element *curr_helm = helm;
	int curr_ofs = ofs + 1;
	int count = 0, current_empty = 0;
	while(GetCollapsedCountFrom(curr_helm,curr_ofs,current_empty))
	{
		curr_helm = GetNextFriendlyTextHelm(curr_helm);
		count += current_empty;
		curr_ofs = 0;
	}
	count += current_empty;
	curr_helm = helm;
	curr_ofs = ofs;
	BOOL dont_set = FALSE;
	if(insert_nbsp_first)
	{
		if(OpStatus::IsError(str.Set(curr_helm->TextContent())))
		{
			m_edit->ReportOOM();
			return FALSE;
		}
		str.CStr()[curr_ofs++] = 0xA0;
		dont_set = TRUE;
	}
	else
		count++; // delete ofs too...
	BOOL has_changed = FALSE;
	for(;;)
	{
		if(!curr_helm)
		{
			OP_ASSERT(FALSE);
			return has_changed;
		}
		int to_delete = MIN(curr_helm->GetTextContentLength() - curr_ofs, count);
		if(curr_helm->GetTextContentLength())
		{
			if(!dont_set)
			{
				if(OpStatus::IsError(str.Set(curr_helm->TextContent())))
				{
					m_edit->ReportOOM();
					return has_changed;
				}
			}
			str.Delete(curr_ofs,to_delete);
			m_edit->SetElementText(curr_helm,str.CStr());
			has_changed = TRUE;
		}
		dont_set = FALSE;
		curr_ofs = 0;
		count -= to_delete;
		if(count)
			curr_helm = GetNextFriendlyTextHelm(curr_helm);
		else
			break;
	}
	return has_changed;
}

BOOL OpDocumentEditWsPreserver::WsPreserve()
{
	DEBUG_CHECKER(TRUE);
	if(!m_edit)
		return FALSE;
	m_edit->RemoveWsPreservingOperation(this);

	BOOL preserved = FALSE;
	BOOL check_start = m_start_elm && m_start_elm->Type() == HE_TEXT && m_start_elm->GetTextContentLength() > m_start_ofs;
	BOOL check_stop = m_stop_elm && m_stop_elm->Type() == HE_TEXT && m_stop_elm->GetTextContentLength() >= m_stop_ofs;

	if(check_start)
	{
		OpDocumentEditWordIterator iter(m_start_elm,m_edit);
		if(OpStatus::IsError(iter.GetStatus()))
			return FALSE;
		if(iter.IsCharCollapsed(m_start_ofs) != m_start_was_collapsed)
		{
			OpString str;
			if (OpStatus::IsError(str.Set(m_start_elm->TextContent())))
			{
				m_edit->ReportOOM();
				return FALSE;
			}
			OP_ASSERT(m_start_elm->GetTextContentLength() == str.Length());
			// FIXME: We should also check the last previously collapsed char behind start to see if it has
			// became uncollapsed, then m_start_elm will still be collapsed however.
			if(m_start_was_collapsed)
				str.Delete(m_start_ofs,1);
			else
				str.CStr()[m_start_ofs] = 0xA0;
			m_edit->SetElementText(m_start_elm,str.CStr());
			preserved = TRUE;
		}
	}
	if(check_stop)
	{
		OpDocumentEditWordIterator iter(m_stop_elm,m_edit);
		if(OpStatus::IsError(iter.GetStatus()))
			return preserved;
		int ofs = m_stop_elm->GetTextContentLength() - m_stop_ofs;
		if(iter.IsCharCollapsed(ofs) != m_stop_was_collapsed)
		{
			preserved = DeleteCollapsedFrom(m_stop_elm,ofs,!m_stop_was_collapsed);
		}
	}

	return preserved;
}

OpDocumentEditWordIterator::~OpDocumentEditWordIterator()
{
	DEBUG_CHECKER(TRUE);
}

OpDocumentEditWordIterator::OpDocumentEditWordIterator(HTML_Element* helm, OpDocumentEdit *edit) :
		WordInfoIterator(edit ? edit->GetDoc() : NULL, helm, &m_surround_checker),
		m_surround_checker(edit),
		m_status(OpStatus::OK),
		m_edit(edit),
		m_is_valid_for_caret(MAYBE)
{
	DEBUG_CHECKER_CONSTRUCTOR();
	if(edit->GetRoot()->IsDirty())
		edit->ReflowAndUpdate();

	m_status = WordInfoIterator::Init();
	if (OpStatus::IsError(m_status))
		return;
}

/* virtual */
BOOL DocEditWordIteratorSurroundChecker::HasWsPreservingElmBeside(HTML_Element* helm, BOOL before)
{
	int i;
	HTML_Element *next = helm;
	if(!next)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	for(;;)
	{
		next = before ? next->Prev() : next->Next();
		if (!next)
			return FALSE;
		if(before && !m_edit->IsFriends(next, helm, TRUE, TRUE, FALSE))
			return FALSE;
		if(!before && !m_edit->IsFriends(helm, next, TRUE, TRUE, TRUE))
			return FALSE;
		if(!next->GetLayoutBox())
			continue; // or return FALSE???
		if(m_edit->IsReplacedElement(next) /*|| next->Type() == HE_BR*/)
			return TRUE;
		if(next->Type() != HE_TEXT || !next->GetLayoutBox()->IsTextBox())
			continue;
		Text_Box *next_box = (Text_Box*)next->GetLayoutBox();
		WordInfo *words = next_box->GetWords();
		int count = next_box->GetWordCount();
		if(!words || !count)
			continue;
		if(before && words[count-1].HasTrailingWhitespace())
			return FALSE;
		for (i = 0; i < count; i++)
			if (words[i].GetLength() && !words[i].IsCollapsed())
				return TRUE;
	}
}

BOOL OpDocumentEditWordIterator::IsValidForCaret(BOOL valid_if_possible)
{
	DEBUG_CHECKER(TRUE);
	if(m_is_valid_for_caret != MAYBE)
		return m_is_valid_for_caret == YES;
	/*
	  samuelp rocked :) if i understand this code correctly the
	  intention here is to assign correct state to
	  m_is_valid_for_caret and return the corresponding result using
	  only one statement.
	 */
	if(HasUnCollapsedChar())
		return (m_is_valid_for_caret = YES) == YES;
	if(!HasUnCollapsedWord())
		return (m_is_valid_for_caret = NO) != NO;
	if(valid_if_possible)
		return (m_is_valid_for_caret = YES) == YES;

	HTML_Element *current_caret = m_edit->m_caret.GetElement();
	if(current_caret && current_caret->Type() == HE_TEXT && !current_caret->GetTextContentLength())
	{ // don't set m_is_valid_for_caret yet...
		if(current_caret == GetElement())
			return TRUE;
		if(!m_edit->IsBeforeOutElm(current_caret) && m_edit->IsFriends(current_caret,GetElement(),TRUE,FALSE,FALSE))
			return FALSE;
	}
	HTML_Element *tmp;
	for(tmp = GetElement()->NextActual(); tmp; tmp = tmp->NextActual())
	{
		if(!m_edit->IsBeforeOutElm(tmp))
		{
			if(tmp->Type() == HE_BR || !m_edit->IsFriends(tmp,GetElement(),TRUE,FALSE,FALSE))
				break;
			if(tmp->Type() == HE_TEXT)
			{
				if(IsNonCollapsedEmptyTextElm(tmp))
				{
					if(GetFullLength())
						return (m_is_valid_for_caret = NO) != NO;
				}
				else
				{
					OpDocumentEditWordIterator iter(tmp,m_edit);
					if(iter.HasUnCollapsedChar())
						return (m_is_valid_for_caret = NO) != NO;
				}
			}
		}
	}
	for(tmp = GetElement()->PrevActual(); tmp; tmp = tmp->PrevActual())
	{
		if(!m_edit->IsBeforeOutElm(tmp))
		{
			if(tmp->Type() == HE_BR || !m_edit->IsFriends(GetElement(),tmp,TRUE,FALSE,FALSE))
				break;
			if(tmp->Type() == HE_TEXT)
			{
				if(IsNonCollapsedEmptyTextElm(tmp))
					return (m_is_valid_for_caret = NO) != NO;
				else
				{
					OpDocumentEditWordIterator iter(tmp,m_edit);
					if(iter.HasUnCollapsedWord() && !(!iter.HasUnCollapsedChar() && !GetFullLength() && iter.GetFullLength()))
						return (m_is_valid_for_caret = NO) != NO;
				}
			}
		}
	}
	return (m_is_valid_for_caret = YES) == YES;
}

BOOL OpDocumentEditWordIterator::GetFirstValidCaretOfs(int &res_ofs)
{
	DEBUG_CHECKER(TRUE);
	res_ofs = 0;
	if(!IsValidForCaret())
		return FALSE;
	if(HasUnCollapsedChar())
		res_ofs = FirstUnCollapsedOfs();
	return TRUE;
}

BOOL OpDocumentEditWordIterator::GetLastValidCaretOfs(int &res_ofs)
{
	DEBUG_CHECKER(TRUE);
	res_ofs = 0;
	if(!IsValidForCaret())
		return FALSE;
	if(HasUnCollapsedChar())
		res_ofs = LastUnCollapsedOfs() + 1;
	return TRUE;
}

BOOL OpDocumentEditWordIterator::GetValidCaretOfsFrom(int ofs, int &res_ofs, BOOL forward)
{
	DEBUG_CHECKER(TRUE);
	if(!IsValidForCaret())
		return FALSE;
	if(!HasUnCollapsedChar())
	{
		if((forward && ofs >= 0) || (!forward && ofs <= 0))
			return FALSE;
		res_ofs = 0;
		return TRUE;
	}
#ifdef DEBUG_ENABLE_OPASSERT
	BOOL success = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	if(forward)
	{
		if(ofs > LastUnCollapsedOfs())
			return FALSE;
#ifdef DEBUG_ENABLE_OPASSERT
		success =
#endif // DEBUG_ENABLE_OPASSERT
			GetOfsSnappedToUnCollapsed(ofs,res_ofs,TRUE);
		OP_ASSERT(success);
		if(res_ofs == LastUnCollapsedOfs())
			res_ofs++;
		else
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetNextUnCollapsedOfs(res_ofs,res_ofs);
	}
	else
	{
		if(ofs <= FirstUnCollapsedOfs())
			return FALSE;
		if(ofs > LastUnCollapsedOfs())
		{
			res_ofs = LastUnCollapsedOfs();
			return TRUE;
		}
		if(IsCharCollapsed(ofs) && !IsPreFormatted())
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetOfsSnappedToUnCollapsed(ofs,res_ofs,FALSE);
		else
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetPrevUnCollapsedOfs(ofs,res_ofs);
	}
	OP_ASSERT(success);
	return TRUE;
}

BOOL OpDocumentEditWordIterator::SnapToValidCaretOfs(int ofs, int &res_ofs)
{

	DEBUG_CHECKER(TRUE);
	res_ofs = 0;
	if(!IsValidForCaret())
		return FALSE;
	if(!HasUnCollapsedChar())
		return TRUE;
	if(ofs > LastUnCollapsedOfs())
		res_ofs = LastUnCollapsedOfs() + 1;
	else
		GetOfsSnappedToUnCollapsed(ofs,res_ofs,TRUE);
	return TRUE;
}

BOOL OpDocumentEditWordIterator::GetCaretOfsWithoutCollapsed(int ofs, int &res_ofs)
{
	DEBUG_CHECKER(TRUE);
	res_ofs = 0;
	if(!IsValidForCaret())
		return FALSE;
	if(!HasUnCollapsedChar())
		return TRUE;
	if(ofs >= LastUnCollapsedOfs())
		res_ofs = UnCollapsedCount();
	else
	{
		SnapToValidCaretOfs(ofs,ofs);
		CollapsedToUnCollapsedOfs(ofs,res_ofs);
	}
	return TRUE;
}

OP_STATUS NonActualElementPosition::Construct(HTML_Element* element, BOOL search_reference_forward /* FALSE */)
{
	OP_ASSERT(m_actual_reference == NULL);
	OP_ASSERT(m_offset == 0);
	if (!element)
		return OpStatus::OK;

	while (element && !element->IsIncludedActual())
	{
		element = search_reference_forward ? element->Next() : element->Prev();
		++m_offset;
	}
	if (!element)
	{
		m_offset = 0;
		return OpStatus::ERR;
	}

	m_actual_reference = element;
	return OpStatus::OK;
}

HTML_Element* NonActualElementPosition::Get() const
{
	HTML_Element* result = m_actual_reference;
	for (unsigned int i = 0; result && i < m_offset; ++i)
		result = m_forward ? result->Prev() : result->Next();

	return result;
}


#endif // DOCUMENT_EDIT_SUPPORT
