/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(KEYBOARD_SELECTION_SUPPORT) || defined(DOCUMENT_EDIT_SUPPORT)

#include "modules/doc/caret_painter.h"

#include "modules/pi/ui/OpUiInfo.h"
#include "modules/logdoc/selectionpoint.h"
#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/doc/caret_manager.h"
#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/display/vis_dev.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/box/box.h"

#define CARET_NORMAL_WIDTH 1
#define CARET_OVERSTRIKE_WIDTH 3

CaretPainter::CaretPainter(FramesDocument *frames_doc)
	: timer(NULL),
	  on(FALSE),
	  frames_doc(frames_doc),
	  x(0),
	  y(0),
	  paint_x(0),
	  wanted_x(0),
	  line_y(0),
	  width(CARET_NORMAL_WIDTH),
	  height(0),
	  line_height(0),
	  update_pos_lock(0),
	  update_pos_needed(FALSE),
	  update_wanted_x_needed(FALSE),
	  overstrike(FALSE)
#ifdef DRAG_SUPPORT
	  , drag_caret(FALSE)
#endif // DRAG_SUPPORT
{
}

void CaretPainter::BlinkNow()
{
#ifdef DRAG_SUPPORT
	if (!drag_caret)
#endif // DRAG_SUPPORT
	{
		on = !on;
		Invalidate();
	}
}

void CaretPainter::RestartBlink()
{
	StopBlink();

	on = TRUE;
	Invalidate();

#ifdef DRAG_SUPPORT
	if (drag_caret)
		return;
#endif // DRAG_SUPPORT

	if (!timer)
	{
		timer = OP_NEW(OpTimer, ());
		if (timer)
			timer->SetTimerListener(this);
	}
	if (timer)
		timer->Start(500);
}

void CaretPainter::StopBlink()
{
	if (timer)
		timer->Stop();

#ifdef DRAG_SUPPORT
	if (drag_caret)
		return;
#endif // DRAG_SUPPORT

	on = FALSE;
	Invalidate();
}

void CaretPainter::OnTimeOut(OpTimer *timer)
{
	timer->Start(500);
	BlinkNow();
}

void CaretPainter::Invalidate()
{
	VisualDevice* vd = frames_doc->GetVisualDevice();

	OpRect r(paint_x, y,  MAX(width, g_op_ui_info->GetCaretWidth()), height);

	transform_root.Apply(r);

	vd->Update(r.x, r.y, r.width, r.height);
}

BOOL CaretPainter::UpdatePos()
{
	HTML_Element *traverse_helm = NULL;
	int traverse_ofs = 0;
	BOOL changed = FALSE;

	CaretManager* caret = frames_doc->GetCaret();
	if (!caret->GetElement()
#ifdef DRAG_SUPPORT
	   // In case of the drag caret drag_caret_point may contain a proper element. Its validity is checked below.
	   && !drag_caret
#endif // DRAG_SUPPORT
	    )
		return FALSE;

	update_pos_needed = TRUE;

#ifdef DOCUMENT_EDIT_SUPPORT
	if (frames_doc->GetDocumentEdit())
		frames_doc->GetDocumentEdit()->ReflowAndUpdate();
	else
#endif // DOCUMENT_EDIT_SUPPORT
		frames_doc->Reflow(FALSE);

	if (update_pos_lock > 0 || frames_doc->GetLogicalDocument()->GetRoot()->IsDirty() || frames_doc->IsReflowing())
	{
		// Update is locked or the document is dirty and the caret will be update very soon (after the reflow)
		return FALSE;
	}

	Invalidate();

	// Find the current position of the caret

	HTML_Element* caret_element = caret->GetElement();
	int caret_offset = caret->GetOffset();

#ifdef DRAG_SUPPORT
	if (drag_caret)
	{
		TextSelection::ConvertPointToOldStyle(drag_caret_point, caret_element, caret_offset);
		if (!caret_element)
			return FALSE;
	}
#endif // DRAG_SUPPORT

	if (GetTraversalPos(caret_element, caret_offset, traverse_helm, traverse_ofs))
	{
		SelectionBoundaryPoint sel_point1 = TextSelection::ConvertFromOldStyle(traverse_helm, traverse_ofs);
		sel_point1.SetBindDirection(frames_doc->GetCaret()->GetIsLineEnd() ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);

		// Run a CalcSelPointPositionObject to get out position, line_height etc.
		CalcSelPointPositionObject selection_update(frames_doc, &sel_point1, FALSE);
		selection_update.Traverse(frames_doc->GetLogicalDocument()->GetRoot());

		const SelectionPointPosition& sel_point2 = selection_update.GetSelectionPointPosition();

		INT32 old_x = x, old_y = y;

		if (selection_update.HasSelPointPosition() && sel_point2.GetHeight())
		{
			x = sel_point2.GetPixelTranslationX() + sel_point2.GetCaretPixelOffset();
			y = sel_point2.GetPixelTranslationY();
			transform_root = sel_point2.GetTransformRoot();
			height = sel_point2.GetHeight();
			line_y = sel_point2.GetLineTranslationY();
			line_height = sel_point2.GetLineHeight();
#ifdef DOCUMENTEDIT_CHARACTER_WIDE_CARET
			width = sel_point2.GetCharacterWidth() < 2? 4 : sel_point2.GetCharacterWidth();
#endif //DOCUMENTEDIT_CHARACTER_WIDE_CARET

			if (traverse_helm->Type() != HE_TEXT && traverse_ofs == 1)
			{
				x += traverse_helm->GetLayoutBox()->GetWidth();
			}

			// Make sure the caret is painted within the document boundaries.
			// Except in the rare case of the document width not being calculated yet (caret would become invisible at x = -1)
			paint_x = x;
			if (frames_doc->Width())
				paint_x = MIN(paint_x, frames_doc->Width() - 1);

#ifdef DOCUMENT_EDIT_SUPPORT
			OpDocumentEdit* doc_edit = frames_doc->GetDocumentEdit();
			if (doc_edit && (old_x != x || old_y != y))
			{
				if (!doc_edit->m_readonly && !doc_edit->m_layout_modifier.IsActive() && doc_edit->m_caret.IsElementEditable(frames_doc->GetCaret()->GetElement()))
					doc_edit->m_caret.OnUpdatePosDone();
			}
#endif // DOCUMENT_EDIT_SUPPORT
		}
		else
		{
			// Not a valid position. (inside something collapsed or hidden)
			// Fallback on last position but change height to 2.
			height = 2;
		}
		changed = (old_x != x || old_y != y);
	}
	else // No valid position...
	{
		StopBlink();
		changed = TRUE;
	}

	Invalidate();

	update_pos_needed = FALSE;
	if (update_wanted_x_needed)
		UpdateWantedX();

	return changed;
}

void CaretPainter::LockUpdatePos(BOOL lock, BOOL process_update_pos)
{
	if (lock)
		update_pos_lock++;
	else
	{
		OP_ASSERT(update_pos_lock > 0);
		update_pos_lock--;
		if (update_pos_lock == 0 && update_pos_needed && process_update_pos)
			UpdatePos();
	}
}

void CaretPainter::Paint(VisualDevice* vis_dev)
{
	if (on)
	{
#ifdef DOCUMENT_EDIT_SUPPORT
		OpDocumentEdit* doc_edit = frames_doc->GetDocumentEdit();
		if (!frames_doc->GetKeyboardSelectionMode() && doc_edit)
		{
			if (!(!doc_edit->m_readonly && !doc_edit->m_layout_modifier.IsActive() && doc_edit->m_caret.IsElementEditable(frames_doc->GetCaret()->GetElement()) &&
			   (!doc_edit->m_selection.HasContent()))
#ifdef DRAG_SUPPORT
			    && !drag_caret
#endif // DRAG_SUPPORT
			)
				return;
		}
#endif // DOCUMENT_EDIT_SUPPORT
		vis_dev->PushCTM(transform_root);

		vis_dev->DrawCaret(OpRect(paint_x, y, (width == CARET_NORMAL_WIDTH ? g_op_ui_info->GetCaretWidth() : width), height));

#ifdef DEBUG_DOCUMENT_EDIT
		vis_dev->SetColor(OP_RGB(0, 255, 0));
		if (IsAtStartOrEndOfBlock(TRUE))
			vis_dev->FillRect(OpRect(paint_x, y, 1, height));
		if (IsAtStartOrEndOfBlock(FALSE))
			vis_dev->FillRect(OpRect(paint_x + 1, y, 1, height));
#endif

		vis_dev->PopCTM(transform_root);
	}
}

OpPoint CaretPainter::GetCaretPosInDocument() const
{
	OpPoint p;
	p.Set(x, y);
	transform_root.Apply(p);
	return p;
}

OpRect CaretPainter::GetCaretRectInDocument() const
{
	int padding = 4; // FIX: check current padding for body.
	OpRect r(x - padding, y - padding, width + padding*2, height + padding*2);
	transform_root.Apply(r);
	return r;
}

void CaretPainter::UpdateWantedX()
{
	if (update_pos_lock > 0 || frames_doc->GetLogicalDocument()->GetRoot()->IsDirty())
	{
		update_wanted_x_needed = TRUE;
		return;
	}
	wanted_x = x;
	update_wanted_x_needed = FALSE;
}

void CaretPainter::SetOverstrike(BOOL val)
{
	Invalidate();
	overstrike = val;
	width = val ? CARET_OVERSTRIKE_WIDTH : CARET_NORMAL_WIDTH;
	Invalidate();
}

#ifdef DRAG_SUPPORT
void CaretPainter::DragCaret(BOOL val)
{
	on = val;
	drag_caret = val;
	if (!val)
	{
		drag_caret_point.Reset();
		UpdatePos();
		RestartBlink();
	}
	else
		Invalidate();
}
#endif // DRAG_SUPPORT

BOOL CaretPainter::GetTraversalPos(HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs)
{
	if (!CaretManager::GetValidCaretPosFrom(frames_doc, helm, ofs, helm, ofs))
		return FALSE;

	if (helm->FirstChild() && helm->Type() != HE_TEXT && helm->Type() != HE_BR && !CaretManager::IsReplacedElement(helm, TRUE))
	{
		frames_doc->Reflow(FALSE);
		HTML_Element *tmp = ofs ? helm->LastLeaf() : helm->FirstChild();
		HTML_Element *stop = ofs ? helm : helm->NextSibling();
		for (; tmp && tmp != stop; tmp = ofs ? tmp->Prev() : tmp->Next())
			if (tmp->GetLayoutBox())
				break;
		if (tmp && tmp != stop)
			helm = tmp;
	}

	new_helm = helm;
	new_ofs = ofs;
	return TRUE;
}

void CaretPainter::ScrollIfNeeded()
{
	OpRect rect = GetCaretRectInDocument();
	CaretManager *caret = frames_doc->GetCaret();

	HTML_Element *caret_elm = caret->GetElement();
	HTML_Element *container = caret->GetCaretContainer(caret_elm);
	if (frames_doc->GetHtmlDocument())
		frames_doc->ScrollToRect(rect, SCROLL_ALIGN_NEAREST, FALSE, VIEWPORT_CHANGE_REASON_INPUT_ACTION, container ? caret_elm : NULL);
}

#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
