/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/finger_touch/form_element_of_interest.h"
#include "modules/widgets/finger_touch/dropdown_element_of_interest.h"
#include "modules/widgets/OpDropDown.h"

void DropDownElementOfInterest::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	FormElementOfInterest::OnMouseEvent(widget, pos, x, y, button, down, nclicks);

	if (!down)
	{
		// According to specifications, when clicking in item,
		// this item should selected and list should be closed just after.
		g_input_manager->InvokeAction(OpInputAction::ACTION_UNFOCUS_FORM);
	}
}

void DropDownElementOfInterest::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpDropDown *src_dropdown = (OpDropDown *) source;
	int selected_item;
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_LISTBOX)
		selected_item = ((OpListBox *) widget)->GetSelectedItem();
	else
		selected_item = ((OpDropDown *) widget)->GetSelectedItem();
	src_dropdown->SetFocus(FOCUS_REASON_MOUSE);
	src_dropdown->SelectItem(selected_item, TRUE);
	src_dropdown->InvalidateAll();

	clone->SetFocus(FOCUS_REASON_MOUSE);

	ComplexFormElementOfInterest::OnChange(widget, changed_by_mouse);
}

int DropDownElementOfInterest::GetPreferredNumberOfRows(BOOL expanded)  const
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
