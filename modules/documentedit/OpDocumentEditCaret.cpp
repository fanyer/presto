/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/layout_workplace.h"

#ifdef DOCUMENT_EDIT_SUPPORT

OpDocumentEditCaret::OpDocumentEditCaret(OpDocumentEdit *edit)
	: m_edit(edit)
	  // m_transform_root has suitable default constructor
	, m_parent_candidate(NULL)
	, m_remove_when_move_elm(NULL)
	, m_remove_when_move_id(0)
	, m_real_caret_elm_off(0)
{
}

CaretManager* OpDocumentEditCaret::GetCaretManager()
{
	return m_edit->m_doc->GetCaret();
}

HTML_Element* OpDocumentEditCaret::GetElement()
{
	HTML_Element* elm = GetCaretManager()->GetElement();

#ifdef DRAG_SUPPORT
	CaretPainter* painter = m_edit->GetDoc()->GetCaretPainter();
	if (painter->IsDragCaret())
	{
		int offset;
		TextSelection::ConvertPointToOldStyle(painter->GetDragCaretPoint(), elm, offset);
	}
#endif // DRAG_SUPPORT

	return IsElementEditable(elm) ? elm : NULL;
}

int OpDocumentEditCaret::GetOffset()
{
	int offset = GetCaretManager()->GetOffset();

#ifdef DRAG_SUPPORT
	CaretPainter* painter = m_edit->GetDoc()->GetCaretPainter();
	if (painter->IsDragCaret())
	{
		HTML_Element* elm;
		TextSelection::ConvertPointToOldStyle(painter->GetDragCaretPoint(), elm, offset);
	}
#endif // DRAG_SUPPORT

	return offset;
}

HTML_Element *GetAncestorOfType(HTML_Element *helm, HTML_ElementType type);

void OpDocumentEditCaret::PlaceFirst(HTML_Element* edit_root, BOOL create_line_if_empty/*=TRUE*/)
{
	DEBUG_CHECKER(TRUE);

	if (!edit_root && m_parent_candidate) // Use m_parent_candidate if there is one (contentEditable is active on some element)
		edit_root = m_parent_candidate;
	else if (!edit_root && IsElementEditable(m_edit->GetBody())) // No root specified. Use body if it's editable (designMode is active).
		edit_root = m_edit->GetBody();

	if (edit_root)
	{
		if (edit_root->IsDirty() || !edit_root->GetLayoutBox())
			return; // Can't place carets without a complete layout structure
		HTML_Element* editable_elm = m_edit->FindEditableElement(edit_root, TRUE, FALSE, TRUE);
		if (editable_elm && edit_root->IsAncestorOf(editable_elm))
		{
			Set(editable_elm, 0);
		}
		else if (create_line_if_empty)
		{
			BOOL has_body_ancestor = !!GetAncestorOfType(edit_root, HE_BODY);

			// Locking to avoid recursive calls to this method when setting cursor
			LockUpdatePos(TRUE, FALSE);
			HTML_Element *first_text_elm = CreateTemporaryCaretHelm(edit_root);
			LockUpdatePos(FALSE, FALSE);

			if(!first_text_elm)
				return; // TODO: fixmed OOM

			Set(first_text_elm,0);

			if(!has_body_ancestor)
				SetRemoveWhenMoveIfUntouched();
		}
	}
}

OP_STATUS OpDocumentEditCaret::Init(BOOL create_line_if_empty, HTML_Element* edit_root, BOOL only_create_line_if_body)
{
	DEBUG_CHECKER(TRUE);

	if (!edit_root && m_parent_candidate) // Use m_parent_candidate if there is one (contentEditable is active on some element)
		edit_root = m_parent_candidate;
	else if (!edit_root && IsElementEditable(m_edit->GetBody())) // No root specified. Use body if it's editable (designMode is active).
		edit_root = m_edit->GetBody();

	if (!edit_root)
		return OpStatus::OK; // We can't initialize now. Wait for later.

	OP_ASSERT(IsElementEditable(edit_root));

	// Can't place carets in invisible elements since we need layout info to do it
	if (edit_root->IsDirty() || !edit_root->GetLayoutBox())
		return OpStatus::OK;

	PlaceFirst(edit_root, create_line_if_empty);

	OP_ASSERT(!edit_root->IsMatchingType(HE_HTML, NS_HTML));
	if (create_line_if_empty && !GetElement())
	{
		BOOL has_body_ancestor = !!GetAncestorOfType(edit_root, HE_BODY);
		if(has_body_ancestor || !only_create_line_if_body)
		{
			HTML_Element *first_text_elm = CreateTemporaryCaretHelm(edit_root);
			if(!first_text_elm)
				return OpStatus::ERR_NO_MEMORY;
			Set(first_text_elm,0);

			if(!has_body_ancestor)
				SetRemoveWhenMoveIfUntouched();
		}
	}

	if (IsValid())
		m_edit->GetDoc()->GetCaretPainter()->Invalidate();

	m_edit->GetDoc()->GetCaretPainter()->Reset();

	m_edit->GetDoc()->GetCaretPainter()->UpdatePos();
	UpdateWantedX();

	return OpStatus::OK;
}

BOOL OpDocumentEditCaret::IsValid()
{
	DEBUG_CHECKER(TRUE);
	return m_edit->GetDoc()->GetCaretPainter()->GetHeight() != 2;
}

#ifdef DOCUMENTEDIT_SPATIAL_FRAMES_FOCUS

FramesDocElm *FindFrame(int x, int y, FramesDocument* doc, FramesDocElm* frm)
{
	if (frm && (x < frm->GetAbsX() ||
				y < frm->GetAbsY() ||
				x > frm->GetAbsX() + frm->GetWidth() ||
				y > frm->GetAbsY() + frm->GetHeight()))
		return NULL;

	if (doc)
	{
		FramesDocElm *child = doc->GetFrmDocRoot();
		while(child)
		{
			FramesDocElm *hit = FindFrame(x, y, (FramesDocument*)child->GetCurrentDoc(), child);
			if (hit)
				return hit;
			child = child->Suc();
		}
		child = doc->GetIFrmRoot();
		while(child)
		{
			FramesDocElm *hit = FindFrame(x, y, (FramesDocument*)child->GetCurrentDoc(), child);
			if (hit)
				return hit;
			child = child->Suc();
		}
	}
	if (frm)
	{
		FramesDocElm *child = frm->FirstChild();
		while(child)
		{
			FramesDocElm *hit = FindFrame(x, y, (FramesDocument*)child->GetCurrentDoc(), child);
			if (hit)
				return hit;
			child = child->Suc();
		}
	}

	return frm;
}

#endif

void OpDocumentEditCaret::MoveSpatial(BOOL forward)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element* old_helm = GetElement();
	int old_ofs = GetOffset();
	int old_x = m_edit->GetDoc()->GetCaretPainter()->GetX();
	int old_y = m_edit->GetDoc()->GetCaretPainter()->GetY();
	int old_line_y = m_edit->GetDoc()->GetCaretPainter()->GetLineY();
	int old_line_height = m_edit->GetDoc()->GetCaretPainter()->GetLineHeight();

	Move(forward, FALSE);

	HTML_Element* moved_helm = GetElement();
	int moved_ofs = GetOffset();

	if (m_edit->GetDoc()->GetCaretPainter()->GetLineY() != old_line_y)
	{
		Set(old_helm, old_ofs);

		AffinePos old_ctm, ctm;
		RECT old_rect, rect;
		FramesDocument* doc = m_edit->GetDoc();
		m_edit->GetContentRectOfElement(m_edit->GetDoc()->GetCaret()->GetContainingElement(old_helm), old_ctm, old_rect);

		const int max_distance = 100;
		int delta = forward ? 3 : -3;
		int distance = delta;
		while (op_abs(distance) < max_distance)
		{
			if (GetCaretManager()->Place(old_ctm, old_x + distance, old_y + 1, old_x + distance, old_y + old_line_height - 1, TRUE, TRUE))
				if (m_edit->GetDoc()->GetCaretPainter()->GetX() != old_x)
					if (m_edit->GetDoc()->GetCaret()->GetContainingElement(GetElement()) != m_edit->GetDoc()->GetCaret()->GetContainingElement(old_helm))
					{
						m_edit->GetContentRectOfElement(m_edit->GetDoc()->GetCaret()->GetContainingElement(GetElement()), ctm, rect);
						if (forward && rect.left > old_rect.right || ctm != old_ctm)
							break;
						if (!forward && rect.right < old_rect.left || ctm != old_ctm)
							break;
					}
			distance += delta;
		}
		if (op_abs(distance) >= max_distance && doc->GetParentDoc())
		{
#ifdef DOCUMENTEDIT_SPATIAL_FRAMES_FOCUS
			// Search for another frame
			int width = doc->GetDocManager()->GetFrame()->GetWidth();
			if ((forward && old_x + distance > width) ||
				(!forward && old_x + distance < 0))
			{
				FramesDocElm* frm = doc->GetDocManager()->GetFrame();
				if (frm)
				{
					int abs_x = old_x + distance + frm->GetAbsX() - doc->GetVisualDevice()->GetRenderingViewX();
					int abs_y = old_y + frm->GetAbsY() - doc->GetVisualDevice()->GetRenderingViewY();
					FramesDocument* top_doc = frm->GetTopFramesDoc();
					FramesDocElm* top_frm = top_doc->GetDocManager()->GetFrame();
					FramesDocElm* targetframe = FindFrame(abs_x, abs_y, top_doc, top_frm);

					if (targetframe)
					{
						FramesDocument* frm_doc = (FramesDocument*) targetframe->GetCurrentDoc();
						if (frm_doc && frm_doc->GetDocumentEdit())
						{
							// FIX: correct coordinates!!
							int x = 10;
							int y = 10;
							GetCaretManager()->Place(x, y, x, y, TRUE, TRUE);
							frm_doc->GetDocumentEdit()->SetFocus(FOCUS_REASON_OTHER);
						}
					}
				}
			}
#endif
			Place(moved_helm, moved_ofs);
			//UpdatePos();
		}
	}
}

void OpDocumentEditCaret::MoveSpatialVertical(BOOL down)
{
	DEBUG_CHECKER(TRUE);
#ifdef DOCUMENTEDIT_SPATIAL_FRAMES_FOCUS
	// Search for another frame
	FramesDocument* doc = m_edit->GetDoc();
	FramesDocElm* frm = doc->GetDocManager()->GetFrame();
	if (frm)
	{
		int height = frm->GetHeight();

		int abs_x = frm->GetAbsX() - doc->GetVisualDevice()->GetRenderingViewX();
		int abs_y = frm->GetAbsY() - doc->GetVisualDevice()->GetRenderingViewY() + (down ? height + 10 : -10);
		FramesDocument* top_doc = frm->GetTopFramesDoc();
		FramesDocElm* top_frm = top_doc->GetDocManager()->GetFrame();
		FramesDocElm* targetframe = FindFrame(abs_x, abs_y, top_doc, top_frm);

		if (targetframe)
		{
			FramesDocument* frm_doc = (FramesDocument*) targetframe->GetCurrentDoc();
			if (frm_doc && frm_doc->GetDocumentEdit())
			{
				// FIX: correct coordinates!!
				int x = 10;
				int y = 10;
				GetCaretManager()->Place(x, y, x, y, FALSE, TRUE);
				frm_doc->GetDocumentEdit()->SetFocus(FOCUS_REASON_OTHER);
			}
		}
	}
#endif
}

void OpDocumentEditCaret::SetRemoveWhenMoveIfUntouched()
{
	DEBUG_CHECKER(TRUE);
	if(!m_edit->m_undo_stack.GetBeginCount())
	{
		m_remove_when_move_elm = GetElement();
		m_remove_when_move_id = m_edit->m_undo_stack.GetBeginCountCalls();
	}
}

void OpDocumentEditCaret::DeleteRemoveWhenMoveIfUntouched(HTML_Element *new_helm)
{
	DEBUG_CHECKER(TRUE);
	if(m_remove_when_move_elm &&
		new_helm != m_remove_when_move_elm &&
		m_remove_when_move_id == m_edit->m_undo_stack.GetBeginCountCalls() &&
		!m_remove_when_move_elm->GetTextContentLength() &&
		!m_edit->m_undo_stack.GetBeginCount()
		)
	{
		HTML_Element *to_delete = m_remove_when_move_elm;
		HTML_Element *parent = to_delete->ParentActual();
		m_remove_when_move_elm = NULL;
		m_edit->BeginChange(parent, CHANGE_FLAGS_ALLOW_APPEND | CHANGE_FLAGS_USER_INVISIBLE);
		m_edit->DeleteElement(to_delete);
		m_edit->EndChange(parent,parent,parent,FALSE,TIDY_LEVEL_MINIMAL);
	}
	if(m_remove_when_move_elm != new_helm)
		m_remove_when_move_elm = NULL;
}

BOOL OpDocumentEditCaret::IsAtRemoveWhenMoveIfUntounced()
{
	DEBUG_CHECKER(TRUE);
	return m_remove_when_move_elm &&
		GetElement() == m_remove_when_move_elm &&
		m_edit->m_undo_stack.GetBeginCountCalls() == m_remove_when_move_id;
}

void OpDocumentEditCaret::CleanTemporaryCaretTextElement(BOOL new_is_created)
{
	HTML_Element* current_temp_elm = m_temp_caret_text_elm.GetElm();
	if (current_temp_elm && (new_is_created || GetElement() != current_temp_elm))
	{
		m_temp_caret_text_elm.Reset();
		if (current_temp_elm->GetTextContentLength() == 0) // Not modified. May be removed
		{
			current_temp_elm->OutSafe(m_edit->GetDoc());
			m_edit->DeleteElement(current_temp_elm);
		}
	}
}

HTML_Element* OpDocumentEditCaret::ConvertTemporaryCaretTextElementToBR()
{
	HTML_Element* current_temp_elm = m_temp_caret_text_elm.GetElm();
	enum
	{
		INSERT_AFTER,
		INSERT_BEFORE,
		INSERT_UNDER
	} insert_type;

	HTML_Element* orientation_point;
	if (current_temp_elm)
	{
		if (orientation_point = current_temp_elm->Suc())
		{
			insert_type = INSERT_BEFORE;
		}
		else if (orientation_point = current_temp_elm->Pred())
		{
			insert_type = INSERT_AFTER;
		}
		else
		{
			orientation_point = current_temp_elm->Parent();
			insert_type = INSERT_UNDER;
		}

		if (current_temp_elm->GetTextContentLength() == 0) // Not modified. May be removed
		{
			HTML_Element* br_elm = m_edit->NewElement(Markup::HTE_BR, TRUE);
			if (br_elm && orientation_point)
			{
				if (insert_type == INSERT_AFTER)
					br_elm->FollowSafe(m_edit->GetDoc(), orientation_point);
				else if (insert_type == INSERT_BEFORE)
					br_elm->PrecedeSafe(m_edit->GetDoc(), orientation_point);
				else
					br_elm->UnderSafe(m_edit->GetDoc(), orientation_point);

				m_temp_caret_text_elm.Reset();
				current_temp_elm->OutSafe(m_edit->GetDoc());
				m_edit->DeleteElement(current_temp_elm);
				Set(br_elm, 0);
			}

			return br_elm;
		}
	}

	return m_temp_caret_text_elm.GetElm();
}

BOOL OpDocumentEditCaret::IsUnmodifiedTemporaryCaretTextElement()
{
	return GetElement() && GetElement() == m_temp_caret_text_elm.GetElm() && GetElement()->GetTextContentLength() == 0;
}


HTML_Element* OpDocumentEditCaret::CreateTemporaryCaretHelm(HTML_Element *parent, HTML_Element *after_me, BOOL text)
{
	DEBUG_CHECKER(TRUE);
	if(!parent && !after_me)
	{
		OP_ASSERT(FALSE);
		return NULL;
	}
	if(!parent)
		parent = after_me->Parent();
	if(!parent || !m_edit->m_undo_stack.IsValidAsChangeElm(parent))
		return NULL;

	if (after_me && (after_me == m_temp_caret_text_elm.GetElm() ||
		(after_me->IsMatchingType(Markup::HTE_BR, NS_HTML) && m_edit->GetInsertedAutomatically(after_me))))
		// No need to create another temp. elm. It would be even wrong if we did.
		return after_me;

	// If we're already on a temporary text-element that should be removed when caret moves if logtree
	// is untouched, then we want to keep it "untouched" even after adding new element
	BOOL is_at_untouched_remove = IsAtRemoveWhenMoveIfUntounced();
	HTML_Element *new_helm;
	if (!text)
		new_helm = m_edit->NewElement(HE_BR, TRUE);
	else
	{
		CleanTemporaryCaretTextElement(TRUE);
		new_helm = m_edit->NewTextElement(UNI_L(""), 0);
		m_temp_caret_text_elm.SetElm(new_helm);
	}

	if(!new_helm)
	{
		m_edit->ReportOOM();
		return NULL;
	}
	if(OpStatus::IsError(m_edit->BeginChange(parent, CHANGE_FLAGS_ALLOW_APPEND | CHANGE_FLAGS_USER_INVISIBLE)))
	{
		m_edit->DeleteElement(new_helm);
		return NULL;
	}
	if(after_me)
		new_helm->FollowSafe(m_edit->GetDoc(),after_me);
	else
	{
		if(parent->FirstChild())
			new_helm->PrecedeSafe(m_edit->GetDoc(),parent->FirstChild());
		else
			new_helm->UnderSafe(m_edit->GetDoc(),parent);
	}
	new_helm->MarkExtraDirty(m_edit->GetDoc());
	m_edit->EndChange(parent,new_helm,new_helm,FALSE,TIDY_LEVEL_MINIMAL);
	if(is_at_untouched_remove)
		SetRemoveWhenMoveIfUntouched();
	return new_helm;
}


void OpDocumentEditCaret::MakeRoomAndMoveCaret(HTML_Element* new_helm, int new_ofs, BOOL forward)
{
	if(m_edit->MakeSureHasValidEdgeCaretPos(GetElement(), NULL, forward))
	{
		if (m_edit->GetOneStepBeside(forward, GetElement(), GetOffset(), new_helm, new_ofs))
		{
			GetCaretManager()->Place(new_helm, new_ofs, FALSE, TRUE, TRUE);
			SetRemoveWhenMoveIfUntouched();
		}
	}
}

BOOL OpDocumentEditCaret::IsAtStartOrEndOfElement(BOOL start)
{
	DEBUG_CHECKER(TRUE);
	if (!GetElement() || !GetElement()->GetLayoutBox())
		return TRUE;
	if (!GetElement()->GetLayoutBox()->IsTextBox())
		return (start && GetOffset() == 0) || (!start && GetOffset() == 1);

	OpDocumentEditWordIterator iter(GetElement(),m_edit);
	if(OpStatus::IsError(iter.GetStatus()) || !iter.HasUnCollapsedChar())
		return TRUE;

	int dummy;
	return !iter.GetValidCaretOfsFrom(GetOffset(),dummy,!start);
}

BOOL OpDocumentEditCaret::IsAtStartOrEndOfBlock(BOOL start, BOOL *is_at_edge_to_child_container)
{
	if (is_at_edge_to_child_container)
		*is_at_edge_to_child_container = FALSE;

	DEBUG_CHECKER(TRUE);
	if (!GetElement())
		return TRUE;

	HTML_Element* nearest_helm = NULL;
	int nearest_ofs;

	if(!m_edit->GetOneStepBeside(!start,GetElement(),GetOffset(),nearest_helm,nearest_ofs))
		return TRUE;

	if (!m_edit->IsFriends(GetElement(), nearest_helm, FALSE, TRUE, TRUE))
	{
		if (GetElement()->Parent()->IsAncestorOf(nearest_helm))
		{
			if (is_at_edge_to_child_container)
				*is_at_edge_to_child_container = TRUE;
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

void OpDocumentEditCaret::Place(const SelectionBoundaryPoint& point)
{
	DEBUG_CHECKER(TRUE);
	HTML_Element *point_helm = point.GetElement();
	if (m_edit->IsElementValidForCaret(point_helm,TRUE,FALSE,TRUE))
	{
		int ofs = point.GetElementCharacterOffset();
		Place(point_helm, ofs, FALSE, FALSE);
	}
	else
	{
		// Convert the parent element and child offset to element and offset 0/1
		int ofs;
		HTML_Element* tmp;
		TextSelection::ConvertPointToOldStyle(point, tmp, ofs);
		if (tmp)
			Place(tmp, ofs);
		else
		{
			/* Looks like the selection point is an element a caret normally can not be placed in.
			 * However if it's not a standalone element, init the caret in it to allow placing
			 * the caret in the empty selection. 
			 */
			if (!m_edit->IsStandaloneElement(point_helm))
				Init(TRUE, point_helm);
		}
	}
	UpdateWantedX();
}

BOOL OpDocumentEditCaret::FixAndCheckCaretMovement(HTML_Element*& helm, int& ofs, BOOL allow_snap, BOOL keep_within_current_context)
{
	if(allow_snap)
	{
		HTML_Element *valid_helm = NULL;
		m_edit->GetValidCaretPosFrom(helm,ofs,valid_helm,ofs);
		if(valid_helm != helm)
		{
			if(helm->Type() == HE_TEXT)
			{
				if(!valid_helm || !m_edit->IsFriends(helm,valid_helm,TRUE,TRUE))
				{
					m_edit->SetElementText(helm,UNI_L(""));
					valid_helm = helm;
					ofs = 0;
				}
			}
		}
		if (!valid_helm)
		{
			StopBlink();
			return FALSE;
		}
		helm = valid_helm;
		if(helm->Type() == HE_TEXT && helm->GetTextContentLength())
		{
			OpDocumentEditWordIterator iter(helm,m_edit);
			if(OpStatus::IsSuccess(iter.GetStatus()) && !iter.UnCollapsedCount())
			{
				m_edit->SetElementText(helm,UNI_L(""));
				ofs = 0;
			}
		}
	}

	if (!IsElementEditable(helm))
		return FALSE;

	if (keep_within_current_context && !m_edit->m_body_is_root && GetElement())
	{
		// Check if the new place is still in the same contentEditable
		HTML_Element* ec1 = m_edit->GetEditableContainer(GetElement());
		HTML_Element* ec2 = m_edit->GetEditableContainer(helm);
		if (ec1 != ec2)
			return FALSE;
	}

	return TRUE;
}

BOOL OpDocumentEditCaret::IsElementEditable(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	if (!helm || !(helm->IsIncludedActual() || helm->GetInserted() == HE_INSERTED_BY_IME))
		return FALSE;
	HTML_Element* body = m_edit->GetBody();
	return (m_edit->m_body_is_root || helm->IsContentEditable(TRUE)) && body && body->IsAncestorOf(helm);
}

void OpDocumentEditCaret::Place(INT32 x, INT32 y, BOOL remember_x, BOOL enter_all, BOOL search_whole_viewport)
{
	AffinePos ctm; ctm.SetTranslation(0, 0);
	m_edit->CheckLogTreeChanged();
	if (GetCaretManager()->PlaceAtCoordinates(ctm, x, y, remember_x, enter_all, search_whole_viewport))
	{
		m_edit->ClearPendingStyles();
		m_edit->m_layout_modifier.Unactivate();
	}
}

void OpDocumentEditCaret::Place(OpWindowCommander::CaretMovementDirection place)
{
	m_edit->CheckLogTreeChanged();
	HTML_Element *containing_element = m_edit->GetDoc()->GetCaret()->GetContainingElement(GetElement());
	if (place == OpWindowCommander::CARET_DOCUMENT_HOME || place == OpWindowCommander::CARET_DOCUMENT_END)
	{
		// For start and end, we want to use the editable container.
		containing_element = m_edit->GetEditableContainer(containing_element);
	}
	if (!containing_element)
		return;
	GetCaretManager()->MoveCaret(place, containing_element, m_edit->GetEditableContainer(GetElement()));
	m_edit->ClearPendingStyles();
	m_edit->m_layout_modifier.Unactivate();
}

void OpDocumentEditCaret::Set(HTML_Element* helm, int ofs, BOOL prefer_first, BOOL remember_x /* = TRUE*/)
{
	DEBUG_CHECKER(TRUE);
//	OP_ASSERT(!helm || helm->Type() == HE_TEXT || (helm->Type() == HE_BR && ofs == 0));
//	OP_ASSERT(!helm || m_edit->GetRoot()->IsAncestorOf(helm));
#ifdef _DOCEDIT_DEBUG
	if (!helm)
	{
		int stop = 0; (void) stop;
	}
#endif
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	m_edit->OnBeforeNewCaretPos(helm, ofs);
#endif
	if (helm)
	{
		if (ofs == 1 && helm->Type() != HE_TEXT && !helm->IsMatchingType(HE_BR, NS_HTML))
		{
			// The old style method of saying "after helm". Translate...
			if (HTML_Element* parent = helm->ParentActual())
			{
				int helm_offset = 0;
				HTML_Element* pred = helm->PredActual();
				while (pred)
				{
					helm_offset++;
					pred = pred->PredActual();
				}
				helm = parent;
				ofs = helm_offset + 1;
			}
		}

		SelectionBoundaryPoint caret_point(helm, ofs);
		caret_point.SetBindDirection(prefer_first ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);
#ifdef DRAG_SUPPORT
		CaretPainter* caret_painter = m_edit->GetDoc()->GetCaretPainter();
		if (caret_painter->IsDragCaret())
			caret_painter->SetDragCaretPoint(caret_point);
		else
#endif // DRAG_SUPPORT
			m_edit->GetDoc()->SetSelection(&caret_point, &caret_point, FALSE, !remember_x);
	}
#ifdef DRAG_SUPPORT
	else
	{
		CaretPainter* caret_painter = m_edit->GetDoc()->GetCaretPainter();
		if (caret_painter->IsDragCaret())
			caret_painter->GetDragCaretPoint().Reset();
	}
#endif // DRAG_SUPPORT

	HTML_Element* temp_caret_elm = m_temp_caret_text_elm.GetElm();
	if (temp_caret_elm && temp_caret_elm != helm)
	{
		// It would be better to remove it complately but 'no, can't do sir. Someone might be using the old caret's HTML_Element pointer.'
		m_temp_caret_text_elm.Reset();
		StoreRealCaretPlacement(NULL, 0);
	}

	m_parent_candidate = NULL;
}

void OpDocumentEditCaret::StoreRealCaretPlacement(HTML_Element* helm, int ofs)
{
	m_real_caret_elm.SetElm(helm);
	m_real_caret_elm_off = ofs;
}

void OpDocumentEditCaret::Set(HTML_Element* helm, int ofs, int x, int y, int height)
{
	DEBUG_CHECKER(TRUE);
	Set(helm , ofs);
	RestartBlink();

	m_edit->GetDoc()->GetCaretPainter()->Invalidate();
	m_edit->GetDoc()->GetCaretPainter()->SetX(x);
	m_edit->GetDoc()->GetCaretPainter()->SetPaintX(x);
	m_edit->GetDoc()->GetCaretPainter()->SetY(y);
	m_edit->GetDoc()->GetCaretPainter()->SetLineY(y);
	m_edit->GetDoc()->GetCaretPainter()->SetHeight(height);
	m_edit->GetDoc()->GetCaretPainter()->SetLineHeight(height);
	m_edit->GetDoc()->GetCaretPainter()->SetWantedX(x);
	m_edit->GetDoc()->GetCaretPainter()->Invalidate();
}

void OpDocumentEditCaret::StickToPreceding()
{
	DEBUG_CHECKER(TRUE);
	if (GetElement()->Type() == HE_TEXT && GetElement()->GetTextContentLength() == 0)
		return; // Stay inside dummyelements
	if (GetOffset() == 0)
	{
		HTML_Element* tmp = m_edit->FindElementBeforeOfType(GetElement(), HE_TEXT, TRUE);
		if (tmp)
		{
			if (m_edit->IsFriends(tmp, GetElement()))
			{
				// Check if it is possible to place in this element.
				int new_ofs;
				OpDocumentEditWordIterator iter(tmp,m_edit);
				if(OpStatus::IsSuccess(iter.GetStatus()) && iter.GetLastValidCaretOfs(new_ofs))
					Place(tmp, new_ofs);
			}
		}
	}
}

void OpDocumentEditCaret::StickToDummy()
{
	DEBUG_CHECKER(TRUE);
	if (GetElement()->Type() == HE_TEXT && GetElement()->GetTextContentLength() == GetOffset())
	{
		HTML_Element* tmp = m_edit->FindElementAfterOfType(GetElement(), HE_TEXT, TRUE);
		if (tmp && tmp->GetTextContentLength() == 0)
		{
			if (m_edit->IsFriends(tmp, GetElement()))
			{
				Place(tmp, 0);
			}
		}
	}
}

void OpDocumentEditCaret::UpdateWantedX()
{
	m_edit->GetDoc()->GetCaretPainter()->UpdateWantedX();
}

void OpDocumentEditCaret::SetOverstrike(BOOL overstrike)
{
	m_edit->GetDoc()->GetCaretPainter()->SetOverstrike(overstrike);
}

void OpDocumentEditCaret::BlinkNow()
{
	m_edit->GetDoc()->GetCaretPainter()->BlinkNow();
}

void OpDocumentEditCaret::StopBlink()
{
	m_edit->GetDoc()->GetCaretPainter()->StopBlink();
}

void OpDocumentEditCaret::LockUpdatePos(BOOL lock, BOOL process_update_pos)
{
	m_edit->GetDoc()->GetCaretPainter()->LockUpdatePos(lock, process_update_pos);
}

BOOL OpDocumentEditCaret::UpdatePos(BOOL prefer_first, BOOL create_line_if_empty)
{
	if (!GetCaretManager()->GetElement())
	{
		PlaceFirst(m_parent_candidate, create_line_if_empty);
		if (!GetElement())
			return FALSE;
	}

	BOOL changed = m_edit->GetDoc()->GetCaretPainter()->UpdatePos();
	return changed;
}

void OpDocumentEditCaret::OnUpdatePosDone()
{
	if (m_edit->IsFocused())
	{
		VisualDevice* vis_dev = m_edit->GetDoc()->GetVisualDevice();
		OpRect rect(m_edit->GetDoc()->GetCaretPainter()->GetX() - vis_dev->GetRenderingViewX(), m_edit->GetDoc()->GetCaretPainter()->GetY() - vis_dev->GetRenderingViewY(), m_edit->GetDoc()->GetCaretPainter()->GetWidth(), m_edit->GetDoc()->GetCaretPainter()->GetHeight());
		rect = vis_dev->ScaleToScreen(rect);
		rect = vis_dev->OffsetToContainer(rect);
		vis_dev->GetOpView()->OnHighlightRectChanged(rect);
		vis_dev->GetOpView()->OnCaretPosChanged(rect);
	}

	OpDocumentEditListener* listener = m_edit->GetListener();
	if (listener)
		listener->OnCaretMoved();
}

OpRect OpDocumentEditCaret::GetCaretRectInDocument() const
{
	return m_edit->GetDoc()->GetCaretPainter()->GetCaretRectInDocument();
}


void OpDocumentEditCaret::RestartBlink()
{
	StopBlink();

	if (!m_edit->IsFocused())
		return;

	m_edit->GetDoc()->GetCaretPainter()->RestartBlink();
}

#endif // DOCUMENT_EDIT_SUPPORT
