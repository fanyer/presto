/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/text_edit_element_of_interest.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/element_expander_container.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"

void TextEditElementOfInterest::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpString text;
	if (OpStatus::IsSuccess(widget->GetText(text)))
		source->SetText(text);

	ComplexFormElementOfInterest::OnChange(widget,changed_by_mouse);
}

void TextEditElementOfInterest::OnChangeWhenLostFocus(OpWidget *widget)
{
	if (source->GetListener())
		source->GetListener()->OnChangeWhenLostFocus(source);
}

OP_STATUS TextEditElementOfInterest::MakeClone(BOOL expanded)
{
	if (!expanded)
	{
		OpWidget *old_widget = clone;
		OpWidget *root = widget_container->GetRoot();

		RETURN_IF_ERROR(source->CreateClone(&clone, root, WIDGETS_F_T_TEXT_INPUT_FONTSIZE, expanded));

		dest_rect = orig_rect;
		int minimum_size = (ElementExpander::GetFingertipPixelRadius(widget_container->GetWindow()) - LAYER_PADDING) * 2;
		dest_rect.height = MAX(minimum_size, orig_rect.height);
		dest_rect.width = MAX(minimum_size, orig_rect.width);

		minimum_size = GetElementExpander()->GetElementMinimumSize();
		if (dest_rect.width < minimum_size)
			dest_rect.width = minimum_size;
		if (dest_rect.height < minimum_size)
			dest_rect.height = minimum_size;

		widget_a_frag->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, dest_rect.width, dest_rect.height), 0);
		clone->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, dest_rect.width, dest_rect.height));
		clone->SetListener(this);

		OpPoint new_dest_rect_size = UpdateFragmentPositions(dest_rect.width, dest_rect.height);
		dest_rect.width = new_dest_rect_size.x;
		dest_rect.height = new_dest_rect_size.y;

		// Center new dest rect around orig rect.
		dest_rect.OffsetBy(-(dest_rect.width - orig_rect.width) / 2, -(dest_rect.height - orig_rect.height) / 2);

		if (old_widget)
		{
			old_widget->Remove();
			old_widget->Delete();
		}
		AdjustSize();
	}
	else
	{
		OpRect old_dest = dest_rect;
		OpRect visible_rect;
		int preferred_w = 0, preferred_h = 0;
		GetElementExpander()->GetAvailableRect(visible_rect);
		if (clone->GetType() == OpTypedObject::WIDGET_TYPE_EDIT)
		{
			dest_rect.x = visible_rect.x;
			preferred_w = visible_rect.width - LAYER_PADDING * 2; // Set to visible area's width
			preferred_h = clone->GetBounds().height;
		}
		else // Text area
		{
			dest_rect.x = visible_rect.x;
			dest_rect.y = visible_rect.y;
			preferred_w = visible_rect.width - LAYER_PADDING * 2;
			preferred_h = visible_rect.height- LAYER_PADDING * 2;
		}
		int minimum_size = (ElementExpander::GetFingertipPixelRadius(widget_container->GetWindow()) - LAYER_PADDING) * 2;
		preferred_h = MAX(minimum_size, preferred_h);
		preferred_w = MAX(minimum_size, preferred_w);

		widget_a_frag->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, preferred_w, preferred_h), 0);
		clone->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, preferred_w, preferred_h));

		OpPoint new_dest_rect_size;
		new_dest_rect_size = UpdateFragmentPositions(preferred_w, preferred_h);
		dest_rect.width = new_dest_rect_size.x + LAYER_PADDING * 2;
		dest_rect.height = new_dest_rect_size.y + LAYER_PADDING * 2;

		if (clone->GetType() == OpTypedObject::WIDGET_TYPE_EDIT)
			dest_rect.OffsetBy(0, -(dest_rect.height - old_dest.height) / 2);

		AdjustSize();
		ScheduleAnimation(old_dest, 1.0f, dest_rect, 1.0f, 200);
	}
	return OpStatus::OK;
}

BOOL TextEditElementOfInterest::DoIsSuitable(unsigned int max_elm_size)
{
	OpWidget* widget = GetHtmlElement()->GetFormObject()->GetWidget();
	if (widget)
	{
		OpTypedObject::Type type = widget->GetType();

		switch(type)
		{
		case OpTypedObject::WIDGET_TYPE_EDIT:
			return !((OpEdit*)widget)->IsReadOnly();
		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
			return !((OpMultilineEdit*)widget)->IsReadOnly();
		}
	}
	return TRUE;
}

#endif // NEARBY_ELEMENT_DETECTION
