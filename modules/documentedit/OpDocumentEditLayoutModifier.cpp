/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/scrollable.h"
#include "modules/layout/layout_workplace.h"

OpDocumentEditLayoutModifier::OpDocumentEditLayoutModifier(OpDocumentEdit* edit)
	: m_edit(edit)
	, m_helm(NULL)
{
	DEBUG_CHECKER_CONSTRUCTOR();
}

OpDocumentEditLayoutModifier::~OpDocumentEditLayoutModifier()
{
	DEBUG_CHECKER(TRUE);
}

void OpDocumentEditLayoutModifier::Paint(VisualDevice* vis_dev)
{
	DEBUG_CHECKER(TRUE);
	if (!m_helm)
		return;

	vis_dev->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED));
	vis_dev->RectangleOut(m_rect.x, m_rect.y, m_rect.width, m_rect.height);
	vis_dev->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED));
	vis_dev->RectangleOut(m_rect.x, m_rect.y, m_rect.width, m_rect.height, CSS_VALUE_dashed);
/*
	OpRect c[3];
	const int csz = 6;
	c[0].Set(m_rect.x + m_rect.width - csz, m_rect.y + m_rect.height/2 - csz, csz, csz);
	c[1].Set(m_rect.x + m_rect.width - csz, m_rect.y + m_rect.height - csz, csz, csz);
	c[2].Set(m_rect.x + m_rect.width/2 - csz, m_rect.y + m_rect.height - csz, csz, csz);
	for(int i = 0; i < 3; i++)
	{
		vis_dev->SetColor(OP_RGB(255, 255, 255));
		vis_dev->FillRect(c[i]);
		vis_dev->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED));
		vis_dev->RectangleOut(c[i].x, c[i].y, c[i].width, c[i].height);
	}*/
}

CursorType OpDocumentEditLayoutModifier::GetCursorType(HTML_Element* helm, int x, int y)
{
	DEBUG_CHECKER(TRUE);
	if (IsLayoutModifiable(helm))
		return CURSOR_DEFAULT_ARROW;

	if (helm->GetLayoutBox())
		if (ScrollableArea* sc = helm->GetLayoutBox()->GetScrollable())
			if (sc->IsOverScrollbar(OpPoint(x, y)))
				// We still want default cursor on the scrollbars.

				return CURSOR_AUTO;

	return CURSOR_TEXT;
}

BOOL OpDocumentEditLayoutModifier::IsLayoutModifiable(HTML_Element* helm)
{
	DEBUG_CHECKER(TRUE);
	Box* box = helm->GetLayoutBox();
	if (!box || !box->GetContent())
		return FALSE;
	if (helm->Type() == HE_DOC_ROOT || helm->Type() == HE_BODY)
		return FALSE;
	if (!(/*box->GetContent()->IsReplaced()*/ m_edit->IsReplacedElement(helm) ||
		box->IsAbsolutePositionedBox() || box->IsFixedPositionedBox() ||
		box->GetTableContent() || helm->Type() == HE_HR || helm->Type() == HE_BUTTON))
		return FALSE;
	HTML_Element* ec = m_edit->GetEditableContainer(helm);
	if (helm == ec)
		return FALSE;
	return ec ? TRUE : FALSE;
}

BOOL OpDocumentEditLayoutModifier::HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button)
{
	DEBUG_CHECKER(TRUE);
	if (event != ONMOUSEDOWN)
		return FALSE;

	Unactivate();

	if (!IsLayoutModifiable(helm))
		return FALSE;

	OldStyleTextSelectionPoint before, after;
	before.SetLogicalPosition(helm, 0);
	after.SetLogicalPosition(helm, 1);
	m_edit->m_selection.Select(&before, &after);

	m_helm = helm;

	UpdateRect();

	// Some other place where focus is set is not called when we handle mouseevent here.
	m_edit->SetFocus(FOCUS_REASON_MOUSE);

	return TRUE;
}

void OpDocumentEditLayoutModifier::UpdateRect()
{
	DEBUG_CHECKER(TRUE);
	AffinePos ctm;
	RECT tmprect;
	m_edit->GetContentRectOfElement(m_helm, ctm, tmprect, ENCLOSING_BOX);

	VisualDevice* vd = m_edit->GetDoc()->GetVisualDevice();

	vd->Update(m_rect.x, m_rect.y, m_rect.width, m_rect.height);

	m_rect = OpRect(&tmprect);
	ctm.Apply(m_rect);

	vd->Update(m_rect.x, m_rect.y, m_rect.width, m_rect.height);
}

void OpDocumentEditLayoutModifier::Delete()
{
	DEBUG_CHECKER(TRUE);
	if (!m_helm)
		return;
	HTML_Element* helm = m_helm;
	Unactivate();

	HTML_Element* new_caret_helm = NULL;
	int new_caret_ofs = 0;
	m_edit->GetNearestCaretPos(helm, &new_caret_helm, &new_caret_ofs, FALSE, FALSE);

	HTML_Element* containing_element = m_edit->GetDoc()->GetCaret()->GetContainingElementActual(helm);
	m_edit->BeginChange(containing_element);

	m_edit->m_caret.Set(NULL, 0);
	m_edit->DeleteElement(helm);
	m_edit->ReflowAndUpdate();

	if (new_caret_helm)
		m_edit->m_caret.Set(new_caret_helm, new_caret_ofs);
	else
	{
		/* Since no appropriate element was found for the caret, use the containing element as
		 * edit_root so an empty placeholder element can be inserted. */
		m_edit->m_caret.PlaceFirst(containing_element);
	}

	m_edit->EndChange(containing_element, TIDY_LEVEL_NORMAL);
}

void OpDocumentEditLayoutModifier::Unactivate()
{
	DEBUG_CHECKER(TRUE);
	if (!m_helm)
		return;
	m_helm = NULL;
	VisualDevice* vd = m_edit->GetDoc()->GetVisualDevice();
	vd->Update(m_rect.x, m_rect.y, m_rect.width, m_rect.height);
}

#endif // DOCUMENT_EDIT_SUPPORT
