/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/doc/html_doc.h"
#include "modules/layout/box/box.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/logdoc/selection.h"

#ifdef DOCUMENT_EDIT_SUPPORT

HTML_Element* GetPrevSiblingActual(HTML_Element *elm)
{
	for (const HTML_Element *leaf = elm; leaf; leaf = leaf->ParentActual() )
    {
        HTML_Element *candidate = leaf->PredActual();
		if (candidate)
			return candidate;
    }

	return NULL;
}

OpDocumentEditSelection::OpDocumentEditSelection(OpDocumentEdit *edit)
	: m_edit(edit)
{
	DEBUG_CHECKER_CONSTRUCTOR();
}

void OpDocumentEditSelection::Select(OldStyleTextSelectionPoint *startpoint, OldStyleTextSelectionPoint *endpoint, BOOL place_caret_at_endpoint)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(startpoint && startpoint->GetElement());
	OP_ASSERT(endpoint && endpoint->GetElement());
	HTML_Document *hdoc = m_edit->GetDoc()->GetHtmlDocument();
	OldStyleTextSelectionPoint *start, *end;
	if (startpoint->Precedes(*endpoint))
	{
		start = startpoint;
		end = endpoint;
	}
	else
	{
		start = endpoint;
		end = startpoint;
		place_caret_at_endpoint = !place_caret_at_endpoint;
	}

	if(hdoc)
	{
		SelectionBoundaryPoint fixed_start = OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(*start);
		SelectionBoundaryPoint fixed_end = OldStyleTextSelectionPoint::ConvertFromOldStyleSelectionPoint(*end);
		hdoc->SetSelection(&fixed_start, &fixed_end, place_caret_at_endpoint);
	}
}

void OpDocumentEditSelection::SelectToCaret(HTML_Element* from_helm, INT32 from_ofs)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(from_helm);
	OldStyleTextSelectionPoint sel_point1, sel_point2;

	if(!m_edit->m_doc->GetCaret()->GetElement())
	{
		OP_ASSERT(FALSE);
		return;
	}

	sel_point1 = m_edit->GetTextSelectionPoint(from_helm, from_ofs);
	sel_point2 = m_edit->GetTextSelectionPoint(m_edit->m_doc->GetCaret()->GetElement(), m_edit->m_doc->GetCaret()->GetOffset());

	Select(&sel_point1, &sel_point2, TRUE);
}

void OpDocumentEditSelection::SelectAll()
{
	DEBUG_CHECKER(TRUE);
	// Select all either in the document or in the focused fragment
	m_edit->ReflowAndUpdate();

	HTML_Document *hdoc = m_edit->GetDoc()->GetHtmlDocument();

	if(hdoc)
	{
		HTML_Element* ec = m_edit->GetEditableContainer(m_edit->m_caret.GetElement());
		BOOL is_focused = m_edit->IsFocused(FALSE) || m_edit->IsFocused(TRUE);
		hdoc->SelectAll(TRUE, is_focused ? ec : NULL);
	}
}

void OpDocumentEditSelection::SelectNothing()
{
	DEBUG_CHECKER(TRUE);
	// Collapse the selection around the caret.
	if (TextSelection* sel = m_edit->GetDoc()->GetTextSelection())
	{
		if (!sel->IsEmpty())
		{
			SelectionBoundaryPoint focus_point = *sel->GetFocusPoint();
			m_edit->GetDoc()->SetSelection(&focus_point, &focus_point);
		}
	}
}

BOOL OpDocumentEditSelection::HasContent()
{
	DEBUG_CHECKER(TRUE);
	return m_edit->GetDoc()->HasSelectedText();
}

void OpDocumentEditSelection::RemoveContent(BOOL aggressive)
{
	DEBUG_CHECKER(TRUE);
	SelectionState sel_state = m_edit->GetSelectionState(TRUE,FALSE,FALSE);
	if(!sel_state.HasContent() || !m_edit->ActualizeSelectionState(sel_state))
		return;
	
	RemoveContent(aggressive, sel_state.start_elm, sel_state.stop_elm, sel_state.start_ofs, sel_state.stop_ofs);
	//if (!HasContent())
	//	return;

	//HTML_Element* start_elm = GetStartElement();
	//HTML_Element* stop_elm = GetStopElement();
	//INT32 start_ofs = GetStartOfs();
	//INT32 stop_ofs = GetStopOfs();

	//m_edit->GetDoc()->GetHtmlDocument()->ClearSelection(FALSE); // FIX: Unneccesary selection update is done in here.

	//RemoveContent(aggressive, start_elm, stop_elm, start_ofs, stop_ofs);
}

BOOL OpDocumentEditSelection::GetRemoveStopElm(BOOL aggressive, HTML_Element *start_elm, int start_ofs, HTML_Element *stop_elm, int stop_ofs)
{
	DEBUG_CHECKER(TRUE);
	if(!start_elm || !stop_elm || start_elm == stop_elm)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	
	// remove empty text-element
	if(stop_elm->Type() == HE_TEXT)
		return stop_elm->GetTextContentLength() == 0;
	
	// remove elements if stop_ofs > 0
	if(stop_elm->Type() != HE_BR)
		return !!stop_ofs;
	
	// if !aggressive we want to remove the same content that GetTextHtml returns, which includes this BR
	if(!aggressive)
		return TRUE;
	
	if(start_elm->Type() == HE_BR)
		return FALSE;
	
	return !m_edit->IsEndingBr(stop_elm);

}

void OpDocumentEditSelection::RemoveContent(BOOL aggressive, HTML_Element* start_elm, HTML_Element* stop_elm, INT32 start_ofs, INT32 stop_ofs, BOOL change_caret_pos)
{
	DEBUG_CHECKER(TRUE);
	int nearest_ofs = 0;
	HTML_Element* nearest_helm = NULL;
	HTML_Element* body = m_edit->GetBody();
	FramesDocument* fdoc = m_edit->GetDoc();
	// FIX: will crash when start or stop returns something bad. (When selection goes first or last and there is no word is one example)	OP_ASSERT(start_elm == stop_elm || start_elm->Precedes(stop_elm));

	if(start_elm != stop_elm && (start_elm->IsAncestorOf(stop_elm) || stop_elm->IsAncestorOf(start_elm)) || !body)
	{
		OP_ASSERT(start_elm->IsAncestorOf(stop_elm));
		OP_ASSERT(stop_elm->IsAncestorOf(start_elm));
		OP_ASSERT(body);
		return;
	}

	HTML_Element* shared_containing_elm = m_edit->GetSharedContainingElement(start_elm, stop_elm);
	OP_ASSERT(body->IsAncestorOf(shared_containing_elm));

	// Make sure that shared_containing_elm doesn't contain ALL content that should be removed,
	// beacuse shared_containing_elm might then itself be a candidate for removal by Tidy()
	// if aggressive==TRUE, and this would otherwise be prevented.
	if(aggressive && !start_ofs && stop_ofs == stop_elm->GetTextContentLength())
	{
		DocTree *prev = start_elm->PrevSibling();
		DocTree *next = stop_elm->NextSibling();
		while(shared_containing_elm != body &&
			(!prev || !shared_containing_elm->IsAncestorOf(prev)) &&
			(!next || !shared_containing_elm->IsAncestorOf(next)))
		{
			shared_containing_elm = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(shared_containing_elm);
		}
	}

	m_edit->BeginChange(shared_containing_elm, CHANGE_FLAGS_ALLOW_APPEND);

	if (change_caret_pos)
		m_edit->m_caret.Set(NULL,0);
	OpDocumentEditWsPreserver preserver(start_elm, stop_elm, start_ofs, stop_ofs, m_edit);
	OpDocumentEditWsPreserverContainer preserver_container(&preserver, m_edit);
	OpDocumentEditRangeKeeper range_keeper(shared_containing_elm, start_elm, stop_elm, m_edit);

	if (start_elm == stop_elm)
	{
		m_edit->DeleteTextInElement(start_elm, start_ofs, stop_ofs - start_ofs);
	}
	else
	{
		HTML_Element* start_container = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(start_elm);
		HTML_Element* stop_container = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(stop_elm);
		OP_ASSERT(shared_containing_elm->IsAncestorOf(start_container));
		OP_ASSERT(shared_containing_elm->IsAncestorOf(stop_container));

		// Crop first element
		m_edit->DeleteTextInElement(start_elm, start_ofs, start_elm->GetTextContentLength() - start_ofs);
		// Remove elements between
		HTML_Element* tmp = (HTML_Element*)(start_elm->NextSiblingActual());
		BOOL moved_stop_elm = FALSE;
		while(tmp != stop_elm)
		{
			HTML_Element* next_elm = (HTML_Element *) tmp->NextActual();

			if (m_edit->IsStandaloneElement(tmp) && !tmp->FirstChild())
			{
				// Remove all standalone elements. Eventual empty parents will be removed in Tidy if they should.
				m_edit->DeleteElement(tmp);
			}
			else if (!m_edit->KeepWhenTidy(tmp) && m_edit->IsEnclosedBy(tmp, start_elm, stop_elm))
			{
				// If a non-standalone element is completly enclosed by selection, we can remove it.
				next_elm = (HTML_Element*) tmp->LastLeaf()->NextActual();
				m_edit->DeleteElement(tmp);
			}
			else if (!m_edit->KeepWhenTidy(tmp) && !m_edit->IsFriendlyElement(tmp) && tmp->Type() != HE_TABLE)
			{
				// User tries to delete at the end of a container, reaching out of that container and into next or parent.
				// We will move move up its content and delete the container.
				// Remove elements under tmp that is previous stop_elm
				HTML_Element *tmp_remove = stop_elm->Prev();
				while(tmp_remove && tmp->IsAncestorOf(tmp_remove) && tmp_remove != tmp)
				{
					HTML_Element *tmp_prev = tmp_remove->PrevActual();
					if(!tmp_remove->IsAncestorOf(stop_elm) && !m_edit->KeepWhenTidy(tmp_remove))
						m_edit->DeleteElement(tmp_remove);
					tmp_remove = tmp_prev;
				}
				
				if(!stop_container->IsAncestorOf(start_container))
				{
					HTML_Element *follow_me = start_elm;
					while(follow_me->Parent() != start_container)
						follow_me = follow_me->Parent();

					while(HTML_Element* move = stop_container->LastChild())
					{
						move->OutSafe(fdoc, FALSE);
						move->FollowSafe(fdoc, follow_me);
						move->MarkExtraDirty(fdoc);
					}
					m_edit->GetBody()->MarkDirty(fdoc, FALSE, TRUE);
					m_edit->DeleteElement(stop_container);
					moved_stop_elm = TRUE;
				}
				next_elm = stop_elm;
			}

			tmp = next_elm;
		}
		// Crop last element
		m_edit->DeleteTextInElement(stop_elm, 0, stop_ofs);
		
		// Possibly move stop_elm with "friends" down in the tree to after start_elm
		if(!moved_stop_elm && stop_container != start_container && 
			stop_container->IsAncestorOf(start_container) && m_edit->IsFriendlyElement(stop_elm,TRUE))
		{
			m_edit->ExtractElementsTo(stop_elm,NULL,stop_container,start_container,start_container->LastChild());
		}
		
		// Tidy up all garbage between start and stop
		m_edit->Tidy(start_elm, stop_elm, FALSE);

		if (GetRemoveStopElm(aggressive,start_elm,start_ofs,stop_elm,stop_ofs))
		{
			m_edit->DeleteElement(stop_elm);
			stop_elm = NULL;
		}
	}

	// Deal with deletion of the start_elm

	BOOL must_keep_start = FALSE;
	if(start_elm->Type() != HE_TEXT && start_elm->Type() != HE_BR && !start_ofs)
	{
		if(!m_edit->GetBestCaretPosFrom(start_elm,nearest_helm,nearest_ofs,MAYBE,TRUE))
		{
			nearest_helm = m_edit->NewTextElement(UNI_L(""),0);
			nearest_ofs = 0;
			if(!nearest_helm)
				m_edit->ReportOOM();
			else
				nearest_helm->PrecedeSafe(fdoc,start_elm);
		}
		if(nearest_helm)
		{
			m_edit->DeleteElement(start_elm);
			if (start_elm == stop_elm)
			{
				stop_elm = nearest_helm;
				stop_ofs = nearest_ofs;
			}
			start_elm = nearest_helm;
			start_ofs = nearest_ofs;
			must_keep_start = m_edit->IsReplacedElement(start_elm);
		}
	}

	// Make sure start_elm is not deleted if its containing element doesn't contain any editable element
	// and if it wasn't empty from the beginning.
	if (start_elm->Type() == HE_TEXT && start_elm->GetTextContentLength() == 0)
	{
		HTML_Element *start_containing_elm = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(start_elm);
		if (m_edit->NeedAutoInsertedBR(start_containing_elm))
		{
			// We need to add a BR to not collapse the layout.
			HTML_Element *newbr = m_edit->NewElement(HE_BR, TRUE);
			if (newbr)
			{
				newbr->FollowSafe(fdoc, start_elm);
				nearest_ofs = 0;
				nearest_helm = newbr;
			}
		}
		else if(!m_edit->GetBestCaretPosFrom(start_elm,nearest_helm,nearest_ofs,MAYBE,TRUE))
			m_edit->SetElementText(start_elm, document_edit_dummy_str);
	}

	if (start_elm->Type() == HE_TEXT && start_elm->GetTextContentLength() == 0 && start_elm->Parent()->FirstChild() == start_elm->Parent()->LastChild())
	{
		start_ofs = 0;
	}
	else if (!must_keep_start && start_elm->GetTextContentLength() == 0 && aggressive && !(start_elm->Type() != HE_TEXT && start_ofs))
	{
		if (!nearest_helm && stop_elm && stop_elm->Type() == HE_BR && stop_ofs == 0)
		{
			// If we have a linebreak as stop element, we can place the caret there when we are done
			// instead of creating empty text node.
			nearest_ofs = 0;
			nearest_helm = stop_elm;
		}
		else if (!nearest_helm)
		{
			nearest_ofs = 0;
			nearest_helm = m_edit->NewTextElement(UNI_L(""),0);
			if(!nearest_helm)
				m_edit->ReportOOM();
			else
				nearest_helm->PrecedeSafe(fdoc,start_elm);				
		}
		if (nearest_helm)
		{
			HTML_Element* tmp = start_elm;
			start_elm = nearest_helm;
			start_ofs = nearest_ofs;

			// Delete the element and it's parents if they become empty.

			HTML_Element* parent = tmp->Parent();
			m_edit->DeleteElement(tmp);
			if(tmp == stop_elm)
				stop_elm = NULL;

			while(parent && !parent->FirstChild() && parent != shared_containing_elm && !m_edit->KeepWhenTidy(parent))
			{
				tmp = parent;
				parent = parent->Parent();

				m_edit->DeleteElement(tmp);
				if(tmp == stop_elm)
					stop_elm = NULL;
			}
		}
	}

	if (start_elm->Type() == HE_TEXT && start_elm->GetTextContentLength() == 0 && start_elm->ParentActual()->IsContentEditable())
		m_edit->SetElementText(start_elm, document_edit_dummy_str);

	// Update caret, just use Set so we won't remove collapsed "garbage" that might be valid after preserver.WsPreserve()
	if (change_caret_pos)
		m_edit->m_caret.Set(start_elm, start_ofs);

	if(aggressive)
	{
		preserver.WsPreserve();
		m_edit->RemoveNBSPIfPossible(start_elm);
		m_edit->RemoveNBSPIfPossible(stop_elm);
	}

	if(!range_keeper.IsValid() || range_keeper.GetContainer() != shared_containing_elm)
	{
		OP_ASSERT(FALSE); // shared_containing_elm has probably been deleted!
		m_edit->AbortChange();
	}
	else
	{
		TIDY_LEVEL tidy_level = aggressive ? TIDY_LEVEL_AGGRESSIVE : TIDY_LEVEL_MINIMAL;
		HTML_Element *t_start = range_keeper.GetStartElement();
		HTML_Element *t_stop = range_keeper.GetStopElement();
		m_edit->EndChange(shared_containing_elm, t_start, t_stop, TRUE, tidy_level);
	}
	
	// Now it's ok to finally use Place, see above.
	if (change_caret_pos)
		m_edit->m_caret.Place(m_edit->m_caret.GetElement(), m_edit->m_caret.GetOffset(), FALSE, aggressive);
}

OP_STATUS OpDocumentEditSelection::GetText(OpString& text)
{
	DEBUG_CHECKER(TRUE);
	int len = m_edit->GetDoc()->GetTextSelection()->GetSelectionAsText(m_edit->GetDoc(), NULL, 0, FALSE);
	if (text.Reserve(len + 1) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	m_edit->GetDoc()->GetTextSelection()->GetSelectionAsText(m_edit->GetDoc(), text.CStr(), len + 1, FALSE);

	return OpStatus::OK;
}

OP_STATUS OpDocumentEditSelection::GetTextHTML(OpString& text, BOOL include_style_containers)
{
	DEBUG_CHECKER(TRUE);
	SelectionState state = m_edit->GetSelectionState(FALSE);
	if(!state.IsValid())
	{
		return text.Set(UNI_L(""));
	}
	return GetTextHTML(text, state.editable_start_elm, state.editable_stop_elm, state.editable_start_ofs, state.editable_stop_ofs,include_style_containers);
}

OP_STATUS OpDocumentEditSelection::GetTextHTML(OpString& text, HTML_Element* start_elm, HTML_Element* stop_elm, INT32 start_ofs, INT32 stop_ofs, BOOL include_style_containers)
{
	// Create a temporary elementtree with the selected text and call HTML5Parser::SerializeTreeL() to
	// convert it into htmltext. This should maby be done on a better way leter (without creating the extra elementtree).

	DEBUG_CHECKER(TRUE);
	FramesDocument *fdoc = m_edit->GetDoc();
	HLDocProfile *hld_profile = fdoc->GetHLDocProfile();
	HTML_Element* sel_start_elm = start_elm;
	HTML_Element* sel_stop_elm = stop_elm;

	if (start_elm == stop_elm && start_ofs == stop_ofs)
		return text.Set(UNI_L(""));

	HTML_Element* rootdiv_helm = m_edit->GetSharedContainingElement(start_elm, stop_elm);

	if(include_style_containers)
	{
		// Include UR, OL and PRE tags in the copied text to.
		// Note: Removed PRE for now since we doesn't handle copy/paste of style elements (an other) properly. Elements are duplicated when that happens.
		while(rootdiv_helm->Type() == HE_UL || rootdiv_helm->Type() == HE_OL /*|| rootdiv_helm->Type() == HE_PRE*/)
			rootdiv_helm = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(rootdiv_helm);
	}

	// Get the toplevel of start and stop elements

	while(start_elm->ParentActual() != rootdiv_helm)
		start_elm = start_elm->ParentActual();
	while(stop_elm->ParentActual() != rootdiv_helm)
		stop_elm = stop_elm->ParentActual();

	// Create a temporary root and DeepClone everything inside start_elm and stop_elm.

	// FIX: use HTML_Element::DOMCloneElement ?
	HTML_Element* tmproot = m_edit->NewCopyOfElement(rootdiv_helm);
	if (!tmproot)
		return OpStatus::ERR_NO_MEMORY;

	HTML_Element* tmp = start_elm;
	while(tmp != stop_elm->Suc())
	{
		HTML_Element* new_elm = m_edit->NewCopyOfElement(tmp);
		if (!new_elm || OpStatus::IsError(new_elm->DeepClone(hld_profile, tmp)))
		{
			m_edit->DeleteElement(new_elm);
			m_edit->DeleteElement(tmproot);
			return OpStatus::ERR_NO_MEMORY;
		}
		new_elm->UnderSafe(fdoc, tmproot);

		tmp = tmp->Suc();
	}

	// Find the elements in the temporary tree that is the copies of selection start and stop in the original tree.

	HTML_Element* original_tree_elm = start_elm;
	// If tree was empty, we have already returned, so this cannot be null:
	// coverity[returned_null: FALSE]
	HTML_Element* temp_tree_elm = tmproot->FirstChildActual();
	while(original_tree_elm != sel_start_elm)
	{
		original_tree_elm = (HTML_Element*) original_tree_elm->NextActual();
		temp_tree_elm = (HTML_Element*) temp_tree_elm->NextActual();
	}
	HTML_Element* temp_start = temp_tree_elm;

	original_tree_elm = stop_elm;
	temp_tree_elm = tmproot->LastChild();
	while(original_tree_elm != sel_stop_elm)
	{
		original_tree_elm = (HTML_Element*) original_tree_elm->NextActual();
		temp_tree_elm = (HTML_Element*) temp_tree_elm->NextActual();
	}
	HTML_Element* temp_stop = temp_tree_elm;

	OP_ASSERT(temp_start->Type() == sel_start_elm->Type());
	OP_ASSERT(temp_stop->Type() == sel_stop_elm->Type());
	OP_ASSERT(temp_start->Type() != HE_TEXT || uni_strcmp(temp_start->TextContent(), sel_start_elm->TextContent()) == 0);
	OP_ASSERT(temp_stop->Type() != HE_TEXT || uni_strcmp(temp_stop->TextContent(), sel_stop_elm->TextContent()) == 0);

	// Delete the unselected textparts in the temporary tree

	tmp = tmproot->FirstChild();
	while(tmp != temp_start)
	{
		HTML_Element* next_elm = (HTML_Element*) tmp->Next();
		if (!tmp->IsAncestorOf(temp_start))
		{
			next_elm = (HTML_Element*)tmp->NextSibling();
			m_edit->DeleteElement(tmp);
		}
		tmp = next_elm;
	}

	tmp = (HTML_Element*) temp_stop->Next();
	while(tmp)
	{
		HTML_Element* next_elm = (HTML_Element*) tmp->NextSibling();
		m_edit->DeleteElement(tmp);
		tmp = next_elm;
	}

	if (temp_stop->Type() == HE_BR && stop_ofs == 0 && temp_start != temp_stop)
	{
		m_edit->DeleteElement(temp_stop);
		temp_stop = NULL;
	}
	else
		m_edit->DeleteTextInElement(temp_stop, stop_ofs, temp_stop->GetTextContentLength() - stop_ofs);
	m_edit->DeleteTextInElement(temp_start, 0, start_ofs);

	// Remove all nontexttags that are now empty (B, FONT, I etc..)
	m_edit->Tidy(tmproot, tmproot, FALSE, TIDY_LEVEL_AGGRESSIVE);

	// Convert the tmproot into a HTML string

	OP_STATUS oom_stat = m_edit->GetTextHTMLFromElement(text, tmproot, FALSE);

	m_edit->DeleteElement(tmproot);

	return oom_stat;
}

HTML_Element* OpDocumentEditSelection::GetStartElement(BOOL only_actual_element)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(HasContent());
	HTML_Element* start_elm;
	int start_offset;
	HTML_Element* end_elm;
	int end_offset;
	TextSelection::ConvertPointToOldStyle(m_edit->GetDoc()->GetTextSelection()->GetStartSelectionPoint(), start_elm, start_offset);
	TextSelection::ConvertPointToOldStyle(m_edit->GetDoc()->GetTextSelection()->GetEndSelectionPoint(), end_elm, end_offset);

	OldStyleTextSelectionPoint start_point(start_elm, start_offset);
	OldStyleTextSelectionPoint end_point(end_elm, end_offset);

	if (end_point.Precedes(start_point))
	{
		start_elm = end_elm;
		start_offset = end_offset;
	}

	if (start_elm->GetIsPseudoElement() || (only_actual_element && !start_elm->IsIncludedActual()))
		start_elm = start_elm->ParentActual();
	else if (start_elm->Parent()->GetIsPseudoElement())
	{
		HTML_Element *candidate = start_elm->ParentActual()->FirstChildActual();
		if (!candidate)
		{
			start_elm = GetPrevSiblingActual(start_elm->ParentActual());
			while (!candidate && start_elm)
			{
				candidate = start_elm->LastChildActual();
				if (!candidate)
					start_elm = GetPrevSiblingActual(start_elm);
			}
		}

		return candidate;
	}

	return start_elm;
}

HTML_Element* OpDocumentEditSelection::GetStopElement(BOOL only_actual_element)
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(HasContent());
	HTML_Element* start_elm;
	int start_offset;
	HTML_Element* end_elm;
	int end_offset;
	TextSelection::ConvertPointToOldStyle(m_edit->GetDoc()->GetTextSelection()->GetStartSelectionPoint(), start_elm, start_offset);
	TextSelection::ConvertPointToOldStyle(m_edit->GetDoc()->GetTextSelection()->GetEndSelectionPoint(), end_elm, end_offset);

	OldStyleTextSelectionPoint start_point(start_elm, start_offset);
	OldStyleTextSelectionPoint end_point(end_elm, end_offset);

	if (end_point.Precedes(start_point))
	{
		end_elm = start_elm;
		end_offset = start_offset;
	}

	if (end_elm->GetIsPseudoElement() || (only_actual_element && !end_elm->IsIncludedActual()))
		end_elm = end_elm->ParentActual();
	else if (end_elm->Parent()->GetIsPseudoElement())
	{
		HTML_Element *candidate = end_elm->ParentActual()->LastChildActual();
		if (!candidate)
		{
			end_elm = end_elm->ParentActual()->NextSiblingActual();
			while (!candidate && end_elm)
			{
				candidate = end_elm->FirstChildActual();
				if (!candidate)
					end_elm = end_elm->NextSiblingActual();
			}
		}

		return candidate;
	}

	return end_elm;
}

INT32 OpDocumentEditSelection::GetStartOfs()
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(HasContent());
	TextSelection* ts = m_edit->GetDoc()->GetTextSelection();
	HTML_Element *helm;
	int offset;
	TextSelection::ConvertPointToOldStyle(ts->GetStartSelectionPoint(), helm, offset);
	if (GetStartElement() != helm)
		return 0;

	return offset;
}

INT32 OpDocumentEditSelection::GetStopOfs()
{
	DEBUG_CHECKER(TRUE);
	OP_ASSERT(HasContent());
	TextSelection* ts = m_edit->GetDoc()->GetTextSelection();
	HTML_Element *helm;
	int offset;
	TextSelection::ConvertPointToOldStyle(ts->GetEndSelectionPoint(), helm, offset);
	HTML_Element *stop_elm = GetStopElement();
	if (stop_elm && stop_elm != helm)
		return stop_elm->GetTextContentLength();

	return offset;
}

BOOL OpDocumentEditSelection::IsEndPointFocusPoint()
{
	DEBUG_CHECKER(TRUE);
	TextSelection* ts = m_edit->GetDoc()->GetTextSelection();
	return ts && ts->GetFocusPoint() == &ts->GetEndSelectionPoint();
}

#endif // DOCUMENT_EDIT_SUPPORT
