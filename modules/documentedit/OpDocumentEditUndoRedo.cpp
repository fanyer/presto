/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/util/simset.h"
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/documentedit/OpDocumentEditUndoRedo.h"
#include "modules/dom/domutils.h"

#ifdef DOCUMENT_EDIT_SUPPORT

// #define DOCEDIT_PRINT_LOGS

// == OpDocumentEditUndoRedoAutoGroup ==================================================

OpDocumentEditUndoRedoAutoGroup::OpDocumentEditUndoRedoAutoGroup(OpDocumentEditUndoRedoStack* stack)
	: stack(stack)
{
	stack->BeginGroup();
}

OpDocumentEditUndoRedoAutoGroup::~OpDocumentEditUndoRedoAutoGroup()
{
	stack->EndGroup();
}

// == OpDocumentEditDisableUndoRegistrationAuto ==================================================

OpDocumentEditDisableUndoRegistrationAuto::OpDocumentEditDisableUndoRegistrationAuto(OpDocumentEditUndoRedoStack* stack)
	: stack(stack)
{
	OP_ASSERT(stack);
	stack->DisableChangeRegistration();
}

OpDocumentEditDisableUndoRegistrationAuto::~OpDocumentEditDisableUndoRegistrationAuto()
{
	stack->EnableChangeRegistration();
}

// == OpDocumentEditUndoRedoEvent ==================================================

OpDocumentEditUndoRedoEvent::OpDocumentEditUndoRedoEvent(
	OpDocumentEdit *edit
	)
	: caret_elm_nr_before(0)
	, caret_ofs_before(0)
	, caret_elm_nr_after(0)
	, caret_ofs_after(0)
	, elm_nr(0)
	, changed_elm_nr_before(-1)
	, changed_elm_nr_after(-1)
	, bytes_used(0)
	, element_changed(NULL)
	, tree_inserter(this)
	, type(UNDO_REDO_TYPE_UNKNOWN)
	, user_invisible(FALSE)
	, is_ended(FALSE)
	, m_edit(edit)
	, stage(UNDO)
	, runtime(NULL)
{
	DEBUG_CHECKER_CONSTRUCTOR();
}

OpDocumentEditUndoRedoEvent::~OpDocumentEditUndoRedoEvent()
{
	DEBUG_CHECKER(TRUE);
	while(OpDocumentEditUndoRedoEvent* merged = static_cast<OpDocumentEditUndoRedoEvent*>(First()))
		OP_DELETE(merged);

	Out();
	// No clone in below cases
	if ((type != UNDO_REDO_ELEMENT_INSERT || stage != UNDO) &&
		(type != UNDO_REDO_ELEMENT_REMOVE || stage != REDO) &&
		type != UNDO_REDO_CARET_PLACEMENT_BEGIN &&
		type != UNDO_REDO_CARET_PLACEMENT_END)
	{
		if (element_changed)
		{
			Unprotect(element_changed);
			OpDocumentEdit::DeleteElement(element_changed, m_edit, FALSE);
		}
	}
}

OP_STATUS OpDocumentEditUndoRedoEvent::Protect(HTML_Element* element)
{
	DEBUG_CHECKER(TRUE);
	DOM_Object *node;
	DOM_Environment *environment = m_edit->GetDoc()->GetDOMEnvironment();
	if (!environment)
		return OpStatus::OK;

	RETURN_IF_ERROR(environment->ConstructNode(node, element));

	runtime = environment->GetRuntime();
	return runtime->Protect(DOM_Utils::GetES_Object(node)) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

void OpDocumentEditUndoRedoEvent::Unprotect(HTML_Element* element)
{
	DEBUG_CHECKER(TRUE);
	if (runtime)
		if (DOM_Object *node = element->GetESElement())
			runtime->Unprotect(DOM_Utils::GetES_Object(node));
}

#if defined(DOCEDIT_PRINT_LOGS) && defined(_DOCEDIT_DEBUG)
#define DOCEDIT_DBG(x) dbg_printf x
#else
#define DOCEDIT_DBG(x)
#endif

OP_STATUS OpDocumentEditUndoRedoEvent::BeginChange(HTML_Element* containing_elm, HTML_Element* element_changed, int type)
{
	DEBUG_CHECKER(TRUE);

	this->type = type;

	DOCEDIT_DBG(("EVENT %d, ELM %d\n", type, element_changed ? element_changed->Type() : -1));
	if (type != UNDO_REDO_CARET_PLACEMENT_BEGIN && type != UNDO_REDO_CARET_PLACEMENT_END)
	{
		if (type != UNDO_REDO_ELEMENT_INSERT)
		{
			changed_elm_nr_before = GetNumberOfElement(element_changed);
			DOCEDIT_DBG(("BEFORE %d, ", changed_elm_nr_before));
			this->element_changed = CreateClone(element_changed, TRUE);
			RETURN_IF_ERROR(Protect(this->element_changed));
			if(!this->element_changed)
				return OpStatus::ERR_NO_MEMORY;

			tree_inserter.FindPlaceMarker(element_changed);
		}
		else
		{
			this->element_changed = element_changed;
			changed_elm_nr_before = -1;
		}
	}
	else
	{
		elm_nr = GetNumberOfElement(containing_elm);
		caret_elm_nr_before = GetNumberOfElement(m_edit->m_caret.GetElement());
		caret_ofs_before = m_edit->m_caret.GetOffset();
	}

	UpdateBytesUsed();

	return OpStatus::OK;
}

OP_STATUS OpDocumentEditUndoRedoEvent::EndChange(HTML_Element* containing_elm)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(!is_ended);
	if (type != UNDO_REDO_CARET_PLACEMENT_BEGIN && type != UNDO_REDO_CARET_PLACEMENT_END)
	{
		if (type == UNDO_REDO_ELEMENT_INSERT)
			changed_elm_nr_after = GetNumberOfElement(element_changed);
		else
		{
			changed_elm_nr_after = (type == UNDO_REDO_ELEMENT_REMOVE) ? -1 : changed_elm_nr_before;
		}
	}
	else
	{
		caret_elm_nr_after = GetNumberOfElement(m_edit->m_caret.GetElement());
		caret_ofs_after = m_edit->m_caret.GetOffset();
	}

	DOCEDIT_DBG(("END %d\n", changed_elm_nr_after));
	is_ended = TRUE;

	return OpStatus::OK;
}

OP_STATUS OpDocumentEditUndoRedoEvent::Clone(HTML_Element* target, HTML_Element* source, BOOL deep)
{
	DEBUG_CHECKER(TRUE);
	FramesDocument* fdoc = m_edit->GetDoc();
	HLDocProfile *hld_profile = fdoc->GetHLDocProfile();
	if (deep)
	{
		// Delete children
		while(target->FirstChild())
			m_edit->DeleteElement(target->FirstChild(), FALSE);

		// Clone with children
		RETURN_IF_ERROR(target->DeepClone(hld_profile, source));
		target->MarkExtraDirty(fdoc);
		target->Parent()->MarkDirty(fdoc, FALSE, TRUE);
	}
	else
	{
		// Create new clone
		HTML_Element* copy = m_edit->NewCopyOfElement(source);
		if (!copy)
			return OpStatus::ERR_NO_MEMORY;
		copy->PrecedeSafe(fdoc, target);
		copy->MarkExtraDirty(fdoc);
		copy->SetEndTagFound();
		// Move children to the new clone and remove the source.
		while(target->FirstChild())
		{
			HTML_Element* move = target->FirstChild();
			move->OutSafe(fdoc, FALSE);
			move->UnderSafe(fdoc, copy);
			move->MarkExtraDirty(fdoc);
		}
		if (copy->Type() == HE_BODY)
			hld_profile->SetBodyElm(copy);
		m_edit->DeleteElement(target, FALSE);

		m_edit->GetBody()->MarkDirty(fdoc, FALSE, TRUE);
	}
	return OpStatus::OK;
}

void OpDocumentEditUndoRedoEvent::Action(int type)
{
	switch (type)
	{
		case UNDO_REDO_ELEMENT_INSERT:
		{
			if (element_changed)
			{
				Unprotect(element_changed);
				/* If a stage is UNDO this means it's in fact REMOVE event so nr before is only valid for it.
				 * For INSERT REDO nr after is only valid
				 */
				INT32 pos = stage == UNDO ? changed_elm_nr_before : changed_elm_nr_after;
				DOCEDIT_DBG(("INSERT, ELM %d, POS %d, ", element_changed->Type(), pos));
				tree_inserter.Insert(element_changed, m_edit->GetDoc(), pos);
#ifdef _DOCEDIT_DEBUG
				INT32 after = GetNumberOfElement(element_changed);
				DOCEDIT_DBG(("AFTER %d\n", after));
				OP_ASSERT((changed_elm_nr_before == after && stage == UNDO) || (changed_elm_nr_after == after && stage == REDO));
#endif // _DOCEDIT_DEBUG
			}
			break;
		}
		case UNDO_REDO_ELEMENT_REMOVE:
		{
			INT32 elm_nr = (stage == UNDO) ? changed_elm_nr_after : changed_elm_nr_before;
			HTML_Element* elem_to_remove = GetElementOfNumber(elm_nr);
			if (elem_to_remove)
			{
				DOCEDIT_DBG(("REMOVE, ELM %d, POS %d\n", elem_to_remove->Type(), elm_nr));
				tree_inserter.FindPlaceMarker(elem_to_remove);
				element_changed = CreateClone(elem_to_remove, TRUE);
				if (!element_changed)
					m_edit->ReportOOM();
				Protect(element_changed);
				m_edit->DeleteElement(elem_to_remove);
			}
			else
				element_changed = NULL;
			break;
		}
		case UNDO_REDO_ELEMENT_CHANGE:
		case UNDO_REDO_ELEMENT_REVERT_CHANGE:
		{
			OP_ASSERT(changed_elm_nr_before == changed_elm_nr_after);
			HTML_Element* changed = GetElementOfNumber(changed_elm_nr_before);
			if (changed)
			{
				OP_ASSERT(changed->Type() == element_changed->Type());
				DOCEDIT_DBG(("CHANGE, ELM %d, POS %d, ", changed->Type(), changed_elm_nr_before));
				HTML_Element* to_remove = changed;
				changed = CreateClone(changed, TRUE);
				if (!changed)
					m_edit->ReportOOM();
				Protect(changed);
				m_edit->DeleteElement(to_remove, FALSE);
				Unprotect(element_changed);
				tree_inserter.Insert(element_changed, m_edit->GetDoc(), changed_elm_nr_before);
#ifdef _DOCEDIT_DEBUG
				INT32 after = GetNumberOfElement(element_changed);
				DOCEDIT_DBG(("AFTER %d\n", after));
				OP_ASSERT(changed_elm_nr_before == after);
#endif // _DOCEDIT_DEBUG
			}
			element_changed = changed;
			break;
		}
		case UNDO_REDO_CARET_PLACEMENT_BEGIN:
		case UNDO_REDO_CARET_PLACEMENT_END:
			DOCEDIT_DBG(("CARET\n")); 
			break;
		default:
			OP_ASSERT(!"Unknown event type");
			return;
	}
}

void OpDocumentEditUndoRedoEvent::Undo()
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(is_ended);
	OpDocumentEditUndoRedoEvent* merged = static_cast<OpDocumentEditUndoRedoEvent*>(Last());
	while (merged)
	{
		OpDocumentEditUndoRedoEvent* pred_merged = static_cast<OpDocumentEditUndoRedoEvent*>(merged->Pred());
		merged->Undo();
		merged = pred_merged;
	}

	Action(~type);
	stage = REDO;

	if (type == UNDO_REDO_CARET_PLACEMENT_BEGIN)
	{
		m_edit->ReflowAndUpdate();
		HTML_Element* new_caret_elm = GetElementOfNumber(caret_elm_nr_before);
		if (new_caret_elm)
			m_edit->m_caret.Place(new_caret_elm, caret_ofs_before);
	}
}

void OpDocumentEditUndoRedoEvent::Redo()
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(is_ended);
	Action(type);
	stage = UNDO;

	if (type == UNDO_REDO_CARET_PLACEMENT_END)
	{
		m_edit->ReflowAndUpdate();
		HTML_Element* new_caret_elm = GetElementOfNumber(caret_elm_nr_after);
		if (new_caret_elm)
			m_edit->m_caret.Place(new_caret_elm, caret_ofs_after);
	}

	OpDocumentEditUndoRedoEvent* merged = static_cast<OpDocumentEditUndoRedoEvent*>(First());
	while (merged)
	{
		OpDocumentEditUndoRedoEvent* suc_merged = static_cast<OpDocumentEditUndoRedoEvent*>(merged->Suc());
		merged->Redo();
		merged = suc_merged;
	}
}

BOOL OpDocumentEditUndoRedoEvent::IsAppending(OpDocumentEditUndoRedoEvent* previous_event)
{
	DEBUG_CHECKER(TRUE);
	if (previous_event->IsInsertEvent() && IsInsertEvent())
	{
		// Adding after the previous event
		if (elm_nr == previous_event->elm_nr &&
			caret_elm_nr_before == previous_event->caret_elm_nr_after &&
			caret_ofs_before == previous_event->caret_ofs_after)
			return TRUE;
	}
	if (!previous_event->IsInsertEvent() && !IsInsertEvent())
	{
		// Deleting after previous event
		if (elm_nr == previous_event->elm_nr &&
			caret_elm_nr_after == previous_event->caret_elm_nr_after &&
			caret_ofs_after == previous_event->caret_ofs_after)
			return TRUE;
		// Deleting before previous event
		if (elm_nr == previous_event->elm_nr &&
			caret_elm_nr_before == previous_event->caret_elm_nr_after &&
			caret_ofs_before == previous_event->caret_ofs_after)
			return TRUE;
	}
	return FALSE;
}

BOOL OpDocumentEditUndoRedoEvent::IsInsertEvent()
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(type == UNDO_REDO_CARET_PLACEMENT_BEGIN || type == UNDO_REDO_CARET_PLACEMENT_END);
	return	caret_elm_nr_before < caret_elm_nr_after ||
			(caret_elm_nr_before == caret_elm_nr_after && caret_ofs_before < caret_ofs_after);
}

void OpDocumentEditUndoRedoEvent::UpdateBytesUsed()
{
	DEBUG_CHECKER(TRUE);
	// Not completly accurate. The elements may have stuff allocated (attributes and properties)

	bytes_used = sizeof(OpDocumentEditUndoRedoEvent);

	// No clone in below cases so no additional memory used for HTML_Element
	if ((type != UNDO_REDO_ELEMENT_INSERT || stage != UNDO) &&
		(type != UNDO_REDO_ELEMENT_REMOVE || stage != REDO) &&
		type != UNDO_REDO_CARET_PLACEMENT_BEGIN &&
		type != UNDO_REDO_CARET_PLACEMENT_END)
	{
		HTML_Element* tmp = element_changed;
		while(tmp)
		{
			if (tmp->IsIncludedActual() || tmp->GetInserted() == HE_INSERTED_BY_IME)
			{
				bytes_used += sizeof(HTML_Element);
				if (tmp->Type() == HE_TEXT)
					bytes_used += tmp->GetTextContentLength() * sizeof(uni_char);
			}
			tmp = (HTML_Element*) tmp->Next();
		}
	}
}

HTML_Element* OpDocumentEditUndoRedoEvent::GetElementOfNumber(INT32 nr)
{
	DEBUG_CHECKER(TRUE);
	if(nr < 0)
		return NULL;
	HTML_Element* tmp = m_edit->GetRoot();
	for(int i = 0; i < nr && tmp;)
	{
		tmp = (HTML_Element*) tmp->Next();
		if (tmp && (tmp->IsIncludedActual() || tmp->GetInserted() == HE_INSERTED_BY_IME))
			++i;
	}
	OP_ASSERT(tmp); // This should really not happen!
	return tmp;
}

INT32 OpDocumentEditUndoRedoEvent::GetNumberOfElement(HTML_Element* elm)
{
	DEBUG_CHECKER(TRUE);
	if(!elm)
		return -1;
	INT32 nr = 0;
	HTML_Element* tmp = m_edit->GetRoot();
	while (tmp && tmp != elm)
	{
		tmp = (HTML_Element*) tmp->Next();
		if (tmp && (tmp->IsIncludedActual() || tmp->GetInserted() == HE_INSERTED_BY_IME))
			++nr;
	}
	OP_ASSERT(tmp); // elm is not part of document tree!
	return nr;
}

HTML_Element* OpDocumentEditUndoRedoEvent::CreateClone(HTML_Element* elm, BOOL deep)
{
	DEBUG_CHECKER(TRUE);
	HLDocProfile *hld_profile = m_edit->GetDoc()->GetHLDocProfile();

	HTML_Element* copy = m_edit->NewCopyOfElement(elm);
	if (!copy)
		return NULL;
	if (deep && OpStatus::IsError(copy->DeepClone(hld_profile, elm)))
	{
		m_edit->DeleteElement(copy);
		return NULL;
	}
	return copy;
}


BOOL OpDocumentEditUndoRedoEvent::UndoRedoTreeInserter::FindSucMark(HTML_Element* elm)
{
	HTML_Element* mark_elm;

	OP_ASSERT(type == ELEMENT_MARK_NONE);

	if (elm->Suc())
	{
		mark_elm = elm->Suc();

		while (mark_elm && (!mark_elm->IsIncludedActual() && mark_elm->GetInserted() != HE_INSERTED_BY_IME))
			mark_elm = mark_elm->Suc();

		if (mark_elm)
		{
			mark = evt->GetNumberOfElement(mark_elm);
			DOCEDIT_DBG(("MARK %d, MARK_TYPE SUC, MARK_ELM_TYPE %d\n", mark, mark_elm->Type()));
			type = ELEMENT_MARK_SUC;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL OpDocumentEditUndoRedoEvent::UndoRedoTreeInserter::FindPredMark(HTML_Element* elm)
{
	HTML_Element* mark_elm;

	OP_ASSERT(type == ELEMENT_MARK_NONE);

	if (elm->Pred())
	{
		mark_elm = elm->Pred();

		while (mark_elm && (!mark_elm->IsIncludedActual() && mark_elm->GetInserted() != HE_INSERTED_BY_IME))
			mark_elm = mark_elm->Pred();

		if (mark_elm)
		{
			mark = evt->GetNumberOfElement(mark_elm);
			DOCEDIT_DBG(("MARK %d, MARK_TYPE PRED, MARK_ELM_TYPE %d\n", mark, mark_elm->Type()));
			type = ELEMENT_MARK_PRED;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL OpDocumentEditUndoRedoEvent::UndoRedoTreeInserter::FindParentMark(HTML_Element* elm)
{
	HTML_Element* mark_elm;

	OP_ASSERT(type == ELEMENT_MARK_NONE);

	if (elm->Parent())
	{
		mark_elm = elm->Parent();

		while (mark_elm && (!mark_elm->IsIncludedActual() && mark_elm->GetInserted() != HE_INSERTED_BY_IME))
			mark_elm = mark_elm->Parent();

		if (mark_elm)
		{
			// If the element is the last or the only one child MARK_PARENT marker is good enough for us. However if there are also some other children of mark_elm
			// we need to find proper position where to put this element under the parent
			if (mark_elm->LastChildActual() != elm)
			{
				HTML_Element* iter = mark_elm->FirstChild();
				while (iter)
				{
					if (iter->IsAncestorOf(elm))
					{
						if (FindSucMark(iter) || FindPredMark(iter))
							return TRUE;
					}
					iter = iter->Suc();
				}
			}

			mark = evt->GetNumberOfElement(mark_elm);
			DOCEDIT_DBG(("MARK %d, MARK_TYPE PARENT, MARK_ELM_TYPE %d\n", mark, mark_elm->Type()));
			type = ELEMENT_MARK_PARENT;

			return TRUE;
		}
	}

	return FALSE;
}

void OpDocumentEditUndoRedoEvent::UndoRedoTreeInserter::FindPlaceMarker(HTML_Element* elm)
{
	OP_ASSERT(elm);

	type = ELEMENT_MARK_NONE;

#ifdef DEBUG_ENABLE_OPASSERT
	BOOL found =
#endif // DEBUG_ENABLE_OPASSERT
	FindSucMark(elm) || FindPredMark(elm) || FindParentMark(elm);
	OP_ASSERT(found);
}

void OpDocumentEditUndoRedoEvent::UndoRedoTreeInserter::Insert(HTML_Element* element, const HTML_Element::DocumentContext& context, INT32 position, BOOL mark_dirty /* = TRUE */)
{
	OP_ASSERT(element);
	OP_ASSERT(type == ELEMENT_MARK_PRED || type == ELEMENT_MARK_SUC || type == ELEMENT_MARK_PARENT);
	OP_ASSERT(mark != -1);
	OP_ASSERT(evt->type == UNDO_REDO_ELEMENT_CHANGE ||
			 (evt->type == UNDO_REDO_ELEMENT_INSERT && evt->stage == OpDocumentEditUndoRedoEvent::REDO) ||
			 (evt->type == UNDO_REDO_ELEMENT_REMOVE && evt->stage == OpDocumentEditUndoRedoEvent::UNDO));

	INT32 pos = mark;
	BOOL retry_using_pos = FALSE;
	/* For below operations REMOVE occurs at some point, so when the mark is SUCCESSOR
	 * a position must be adjusted (decreased) because it was remembered BEFORE removing
	 * the element from the tree. The element and its children must be taken into account.
	 */
	if ((evt->type == UNDO_REDO_ELEMENT_CHANGE ||
		(evt->type == UNDO_REDO_ELEMENT_INSERT && evt->stage == OpDocumentEditUndoRedoEvent::REDO) ||
		(evt->type == UNDO_REDO_ELEMENT_REMOVE && evt->stage == OpDocumentEditUndoRedoEvent::UNDO)) &&
		type == ELEMENT_MARK_SUC)
	{
		--pos;
		HTML_Element* iter = element->FirstChild();
		HTML_Element* stop = static_cast<HTML_Element*>(element->NextSibling());

		while (iter && iter != stop)
		{
			if (iter->IsIncludedActual() || iter->GetInserted() == HE_INSERTED_BY_IME)
				--pos;
			iter = iter->Next();
		}
	}
	do
	{
		HTML_Element* elm = evt->GetElementOfNumber(pos);
		if (elm)
		{
			switch (type)
			{
				case ELEMENT_MARK_PRED:
				{
					element->FollowSafe(context, elm, mark_dirty);
					break;
				}

				case ELEMENT_MARK_SUC:
				{
					element->PrecedeSafe(context, elm, mark_dirty);
					break;
				}

				case ELEMENT_MARK_PARENT:
				{
					element->UnderSafe(context, elm, mark_dirty);
					break;
				}
			}
		}
		
		if (evt->GetNumberOfElement(element) != position && !retry_using_pos)
		{
			// Something went wrong and the element is inserted in the wrong place. Try again using the position instead of the mark this time
			element->OutSafe(context, FALSE);
			pos = position;
			if (type == ELEMENT_MARK_PRED)
				--pos;
		}
		else
			break;

		retry_using_pos = !retry_using_pos;
	} while(retry_using_pos);
}

/* static */
UINT32 OpDocumentEditUndoRedoEvent::AllBytesUsed(OpDocumentEditUndoRedoEvent *evt)
{
	UINT32 count = evt->BytesUsed();
	OpDocumentEditUndoRedoEvent *iter = static_cast<OpDocumentEditUndoRedoEvent*>(evt->First());
	while (iter)
	{
		count += iter->BytesUsed();
		iter = static_cast<OpDocumentEditUndoRedoEvent*>(iter->Suc());
	}

	return count;
}

/* static */
void OpDocumentEditUndoRedoEvent::UpdateAllBytesUsed(OpDocumentEditUndoRedoEvent *evt)
{
	evt->UpdateBytesUsed();
	OpDocumentEditUndoRedoEvent *iter = static_cast<OpDocumentEditUndoRedoEvent*>(evt->First());
	while (iter)
	{
		iter->UpdateBytesUsed();
		iter = static_cast<OpDocumentEditUndoRedoEvent*>(iter->Suc());
	}
}

// == OpDocumentEditUndoRedoStack ==================================================

OpDocumentEditUndoRedoStack::OpDocumentEditUndoRedoStack(OpDocumentEdit* edit)
	: m_mem_used(0)
	, m_edit(edit)
	, m_current_event(NULL)
	, m_begin_count(0)
	, m_begin_count_calls(0)
	, m_group_begin_count(0)
	, m_group_event_count(0)
	, m_flags(CHANGE_FLAGS_NONE)
	, m_disabled_count(0)
{
	DEBUG_CHECKER_CONSTRUCTOR();
	m_edit->AddInternalEventListener(this);
}

OpDocumentEditUndoRedoStack::~OpDocumentEditUndoRedoStack()
{
	DEBUG_CHECKER(FALSE);
	OP_ASSERT(m_begin_count == 0);
	OP_ASSERT(m_group_begin_count == 0);
	Clear();
	m_edit->RemoveInternalEventListener(this);
}

void OpDocumentEditUndoRedoStack::Clear(BOOL clear_undos, BOOL clear_redos)
{
	DEBUG_CHECKER(FALSE);
	OpDocumentEditUndoRedoEvent* event;

	// Clear undos
	if (clear_undos)
	{
		event = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.First());
		while(event)
		{
			OpDocumentEditUndoRedoEvent* next_event = static_cast<OpDocumentEditUndoRedoEvent*>(event->Suc());
			m_mem_used -= OpDocumentEditUndoRedoEvent::AllBytesUsed(event);
			OP_DELETE(event);
			event = next_event;
		}
	}
	// Clear redos
	if (clear_redos)
	{
		event = static_cast<OpDocumentEditUndoRedoEvent*>(m_redos.First());
		while(event)
		{
			OpDocumentEditUndoRedoEvent* next_event = static_cast<OpDocumentEditUndoRedoEvent*>(event->Suc());
			m_mem_used -= OpDocumentEditUndoRedoEvent::AllBytesUsed(event);
			OP_DELETE(event);
			event = next_event;
		}
	}
}

OP_STATUS OpDocumentEditUndoRedoStack::BeginChange(HTML_Element* containing_elm, int flags)
{
	if (m_disabled_count)
		return OpStatus::OK;

	DEBUG_CHECKER(FALSE);
	m_begin_count++;
	m_begin_count_calls++;
	if (m_begin_count > 1)
	{
		// Must be same containing element or child if nesting calls to BeginChange
		OP_ASSERT(containing_elm == m_current_containing_elm || m_current_containing_elm->IsAncestorOf(containing_elm));
		return OpStatus::OK;
	}
	OP_ASSERT(containing_elm->IsIncludedActual() || containing_elm->GetInserted() == HE_INSERTED_BY_IME);
	OP_ASSERT(!m_current_event);
	OP_ASSERT(containing_elm == m_edit->GetRoot() || m_edit->GetRoot()->IsAncestorOf(containing_elm));
	m_current_containing_elm = containing_elm;

	Clear(FALSE, TRUE);

	m_flags = flags;

	// Remember start caret placement when change(s) start.
	RETURN_IF_MEMORY_ERROR(BeginChange(m_current_containing_elm, NULL, UNDO_REDO_CARET_PLACEMENT_BEGIN, m_flags));
	RETURN_IF_MEMORY_ERROR(EndLastChange(m_current_containing_elm));

	return OpStatus::OK;
}

void OpDocumentEditUndoRedoStack::OnElementOut(HTML_Element *elm)
{
	if (m_begin_count > 0 && (elm->IsIncludedActual() || elm->GetInserted() == HE_INSERTED_BY_IME) && m_edit->GetRoot()->IsAncestorOf(elm))
	{
		if (OpStatus::IsMemoryError(BeginChange(m_current_containing_elm, elm, UNDO_REDO_ELEMENT_REMOVE, m_flags)))
			m_edit->ReportOOM();
		if (OpStatus::IsMemoryError(EndLastChange(m_current_containing_elm)))
			m_edit->ReportOOM();
	}
}

void OpDocumentEditUndoRedoStack::OnElementInserted(HTML_Element* elm)
{
	if (m_begin_count > 0 && (elm->IsIncludedActual() || elm->GetInserted() == HE_INSERTED_BY_IME) && m_edit->GetRoot()->IsAncestorOf(elm))
	{
		if (OpStatus::IsMemoryError(BeginChange(m_current_containing_elm, elm, UNDO_REDO_ELEMENT_INSERT, m_flags)))
			m_edit->ReportOOM();
		if (OpStatus::IsMemoryError(EndLastChange(m_current_containing_elm)))
			m_edit->ReportOOM();
	}
}

void OpDocumentEditUndoRedoStack::OnElementChange(HTML_Element *elm)
{
	if (m_begin_count > 0 && (elm->IsIncludedActual() || elm->GetInserted() == HE_INSERTED_BY_IME) && m_edit->GetRoot()->IsAncestorOf(elm))
	{
		if (OpStatus::IsMemoryError(BeginChange(m_current_containing_elm, elm, UNDO_REDO_ELEMENT_CHANGE, m_flags)))
			m_edit->ReportOOM();
	}
}

void OpDocumentEditUndoRedoStack::OnElementChanged(HTML_Element *elm)
{
	if (m_begin_count > 0 && (elm->IsIncludedActual() || elm->GetInserted() == HE_INSERTED_BY_IME) && m_edit->GetRoot()->IsAncestorOf(elm))
	{
		if (OpStatus::IsMemoryError(EndLastChange(m_current_containing_elm)))
			m_edit->ReportOOM();
	}
}

OP_STATUS OpDocumentEditUndoRedoStack::BeginChange(HTML_Element* containing_elm, HTML_Element* element_changed, int type, int flags)
{
	if (m_disabled_count)
		return OpStatus::OK;

	m_current_event = OP_NEW(OpDocumentEditUndoRedoEvent, (m_edit));

	if (m_current_event == NULL || OpStatus::IsError(m_current_event->BeginChange(containing_elm, element_changed, type)))
	{
		AbortChange();
		Clear();
		return OpStatus::ERR_NO_MEMORY;
	}

	m_mem_used += m_current_event->BytesUsed();

	if (m_group_begin_count)
	{
		m_group_event_count++;
	}

	OpDocumentEditUndoRedoEvent* previous_event = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.Last());
	if (previous_event && previous_event->Type() == UNDO_REDO_CARET_PLACEMENT_BEGIN)
	{
		// All events between CARET_PLACEMENT_BEGIN - CARET_PLACEMENT_END should be treated as one.
		if (!previous_event->Empty())
		{

			if (static_cast<OpDocumentEditUndoRedoEvent*>(previous_event->Last())->Type() == UNDO_REDO_CARET_PLACEMENT_END)
			{
				if (m_current_event->Type() == UNDO_REDO_CARET_PLACEMENT_BEGIN &&
					flags & CHANGE_FLAGS_ALLOW_APPEND &&
					m_current_event->IsAppending(static_cast<OpDocumentEditUndoRedoEvent*>(previous_event->Last())) &&
					// Do not append user visible change to the user invisible one as it may become invisible too
					(!previous_event->IsUserInvisible() || flags & CHANGE_FLAGS_USER_INVISIBLE)
				   )
				{
					if (m_current_event->MergeTo(previous_event))
						m_current_event = NULL;
				}
			}
			else
			{
				if (m_current_event->MergeTo(previous_event))
					m_current_event = NULL;
			}
		}
		else
		{
			if (m_current_event->MergeTo(previous_event))
				m_current_event = NULL;
		}
	}

	if (m_current_event)
	{
		if (flags & CHANGE_FLAGS_USER_INVISIBLE)
		{
			m_current_event->SetUserInvisible();
			if (m_current_event->MergeTo(previous_event))
					m_current_event = NULL;
		}
		else if (m_group_begin_count)
		{
			if (m_group_event_count > 1)
				if (m_current_event->MergeTo(static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.Last())))
					m_current_event = NULL;
		}
	}

	if (m_current_event)
	{
		m_current_event->Into(&m_undos);
		m_current_event = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS OpDocumentEditUndoRedoStack::EndLastChange(HTML_Element* containing_elm)
{
	if (m_disabled_count)
		return OpStatus::OK;

	OpDocumentEditUndoRedoEvent* last = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.Last());
	if(!last)
	{
		OP_ASSERT(FALSE);
		Clear();
		return OpStatus::ERR;
	}

	if (!last->Empty())
		last = static_cast<OpDocumentEditUndoRedoEvent*>(last->Last());

	if (OpStatus::IsError(last->EndChange(containing_elm)))
	{
		Clear();
		m_current_event = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS OpDocumentEditUndoRedoStack::EndChange(HTML_Element* containing_elm)
{
	if (m_disabled_count)
		return OpStatus::OK;

	DEBUG_CHECKER(FALSE);
	OP_ASSERT(m_begin_count > 0);
	m_begin_count--;

	// Delete old events if the MAX_MEM_PER_UNDOREDOSTACK has been reached.
	// But always keep at least 1 eventgroup left.
	while (m_mem_used > MAX_MEM_PER_UNDOREDOSTACK && m_undos.Cardinal() > 1)
	{
		OpDocumentEditUndoRedoEvent* event = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.First());
		m_mem_used -= OpDocumentEditUndoRedoEvent::AllBytesUsed(event);
		OP_DELETE(event);
	}

	// Remember end caret placement when change(s) end.
	RETURN_IF_MEMORY_ERROR(BeginChange(m_current_containing_elm, NULL, UNDO_REDO_CARET_PLACEMENT_END, m_flags));
	RETURN_IF_MEMORY_ERROR(EndLastChange(m_current_containing_elm));

	return OpStatus::OK;
}

void OpDocumentEditUndoRedoStack::AbortChange()
{
	if (m_disabled_count)
		return;

	DEBUG_CHECKER(FALSE);
	OP_ASSERT(m_begin_count > 0);
	m_begin_count--;
	OP_DELETE(m_current_event);
	m_current_event = NULL;
	Clear();
}

void OpDocumentEditUndoRedoStack::BeginGroup()
{

	DEBUG_CHECKER(FALSE);
	if (m_group_begin_count == 0)
		m_group_event_count = 0;
	m_group_begin_count++;
}

void OpDocumentEditUndoRedoStack::EndGroup()
{
	DEBUG_CHECKER(FALSE);
	m_group_begin_count--;
}

void OpDocumentEditUndoRedoStack::Undo()
{
	DEBUG_CHECKER(FALSE);
	OpDocumentEditUndoRedoEvent* evt = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.Last());
	if (evt && !evt->IsUserInvisible())
	{
		OP_ASSERT(evt->Type() == UNDO_REDO_CARET_PLACEMENT_BEGIN);
		OP_ASSERT(evt->Last() && static_cast<OpDocumentEditUndoRedoEvent*>(evt->Last())->Type() == UNDO_REDO_CARET_PLACEMENT_END);
		// As depending on the event type and current stage a clone may or may not be stored - update mem used
		m_mem_used -= OpDocumentEditUndoRedoEvent::AllBytesUsed(evt);
		evt->Undo();
		evt->Out();
		evt->IntoStart(&m_redos);
		OpDocumentEditUndoRedoEvent::UpdateAllBytesUsed(evt);
		m_mem_used += OpDocumentEditUndoRedoEvent::AllBytesUsed(evt);
	}
}

void OpDocumentEditUndoRedoStack::Redo()
{
	DEBUG_CHECKER(FALSE);
	OpDocumentEditUndoRedoEvent* evt = static_cast<OpDocumentEditUndoRedoEvent*>(m_redos.First());
	OP_ASSERT(!evt->IsUserInvisible());
	if (evt)
	{
		OP_ASSERT(evt->Type() == UNDO_REDO_CARET_PLACEMENT_BEGIN);
		OP_ASSERT(evt->Last() && static_cast<OpDocumentEditUndoRedoEvent*>(evt->Last())->Type() == UNDO_REDO_CARET_PLACEMENT_END);
		// As depending on the event type and current stage a clone may or may not be stored - update mem used
		m_mem_used -= OpDocumentEditUndoRedoEvent::AllBytesUsed(evt);
		evt->Redo();
		evt->Out();
		evt->Into(&m_undos);
		OpDocumentEditUndoRedoEvent::UpdateAllBytesUsed(evt);
		m_mem_used += OpDocumentEditUndoRedoEvent::AllBytesUsed(evt);
	}
}

void OpDocumentEditUndoRedoStack::MergeLastChanges()
{
	OpDocumentEditUndoRedoEvent* last_event = static_cast<OpDocumentEditUndoRedoEvent*>(m_undos.Last());
	if (last_event)
	{
		OpDocumentEditUndoRedoEvent* merge_to = static_cast<OpDocumentEditUndoRedoEvent*>(last_event->Pred());
		if (merge_to)
		{
			last_event->Out();
			last_event->MergeTo(merge_to);
		}
	}
}

#endif // DOCUMENT_EDIT_SUPPORT
