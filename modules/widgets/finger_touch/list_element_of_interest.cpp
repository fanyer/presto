/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/doc/frm_doc.h"

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/finger_touch/form_element_of_interest.h"
#include "modules/widgets/finger_touch/list_element_of_interest.h"
#include "modules/widgets/OpListBox.h"

void ListElementOfInterest::OnMouseEvent(OpWidget *widget, INT32 index, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	HTML_Element *option_helm = GetHtmlElement()->FirstChildActual();
	int idx = 0;

	// search for the clicked option html element
	while (option_helm)
	{
		if (option_helm->Type() == HE_OPTION)
		{
			if (idx == index)
				break;
			++idx;
		}
		else if (option_helm->Type() == HE_OPTGROUP)
		{
			option_helm = option_helm->FirstChildActual();
			continue;
		}

		if (option_helm->SucActual())
			option_helm = option_helm->SucActual();
		else if (option_helm->ParentActual()->Type() == HE_OPTGROUP)
			option_helm = option_helm->ParentActual()->SucActual();
		else
			option_helm = NULL;
	}

	if (option_helm)
	{
		OpPoint origin = GetElementExpander()->GetOrigin();
		OpRect widget_doc_rect = widget->GetDocumentRect(TRUE);

		INT32 offset_x = origin.x - widget_doc_rect.x;
		INT32 offset_y = origin.y - widget_doc_rect.y;
		INT32 doc_x = offset_x + widget_doc_rect.x;
		INT32 doc_y = offset_y + widget_doc_rect.y;

		DOM_EventType event;

#ifdef MOUSELESS // We need to send ONCLICK ourselves
		event = ONCLICK;
#else
		event = down ? ONMOUSEDOWN : ONMOUSEUP; // ONMOUSEUP also triggers onclick
#endif // MOUSELESS

		int sequence_count_and_button_or_delta = MAKE_SEQUENCE_COUNT_AND_BUTTON(nclicks, button);

		FramesDocument *doc = GetElementExpander()->GetDocument();

		OP_STATUS status = doc->HandleMouseEvent(event,NULL, option_helm, NULL, offset_x, offset_y, doc_x, doc_y, SHIFTKEY_NONE, sequence_count_and_button_or_delta);

		if(OpStatus::IsError(status))
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}

	if (!down)
	{
		OpListBox * list_box = static_cast<OpListBox*>(source);

		if (list_box && !list_box->IsMultiple())
		{
			// According to specifications, when clicking in item,
			// this item should selected and list should be closed just after.
			g_input_manager->InvokeAction(OpInputAction::ACTION_UNFOCUS_FORM);
		}
	}
}

void ListElementOfInterest::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpListBox *clone_listbox = (OpListBox *) widget;
	OpListBox *src_listbox = (OpListBox *) source;
	if (src_listbox->IsMultiple())
	{
		int clone_count = clone_listbox->CountItems();
		int src_count = src_listbox->CountItems();
		for(int i = 0; i < clone_count && i < src_count; i++)
		{
			if (src_listbox->IsSelected(i) != clone_listbox->IsSelected(i))
				src_listbox->SelectItem(i, clone_listbox->IsSelected(i));
		}
	}
	else
		src_listbox->SelectItem(clone_listbox->GetSelectedItem(), TRUE);
	src_listbox->SetFocus(FOCUS_REASON_MOUSE);
	src_listbox->ScrollIfNeeded();
	src_listbox->InvalidateAll();

	widget->SetFocus(FOCUS_REASON_MOUSE);

	ComplexFormElementOfInterest::OnChange(widget,changed_by_mouse);
}

int ListElementOfInterest::GetPreferredNumberOfRows(BOOL expanded) const
{
	int rows = ((OpListBox*)clone)->CountItems();
	rows = MAX(rows, 1);

	if (!expanded)
	{
		rows = MIN(FormElementOfInterest::GetPreferredNumberOfRows(expanded), rows);
	}

	return rows;
}

#endif // NEARBY_ELEMENT_DETECTION
