/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/button_element_of_interest.h"
#include "modules/widgets/finger_touch/dropdown_element_of_interest.h"
#include "modules/widgets/finger_touch/form_element_of_interest.h"
#include "modules/widgets/finger_touch/filechooser_element_of_interest.h"
#include "modules/widgets/finger_touch/list_element_of_interest.h"
#include "modules/widgets/finger_touch/text_edit_element_of_interest.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/finger_touch/eoi_fragments.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"
#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/element_expander_container.h"

/* static */
FormElementOfInterest* FormElementOfInterest::Create(HTML_Element* html_element)
{
	FormElementOfInterest * form_eoi = NULL;

	FormObject * form_object = html_element->GetFormObject();

	// FIXME: switch for element type or widget type, not both
	if (!form_object)
	{
		OP_ASSERT(html_element->Type() == HE_BUTTON);

		if (html_element->Type() != HE_BUTTON)
			return NULL;

		return OP_NEW(ButtonElementOfInterest, (html_element));
	}

	switch (form_object->GetWidget()->GetType())
	{
	case OpTypedObject::WIDGET_TYPE_LISTBOX:
		form_eoi = OP_NEW(ListElementOfInterest, (html_element));
		break;

	case OpTypedObject::WIDGET_TYPE_DROPDOWN:
		form_eoi = OP_NEW(DropDownElementOfInterest, (html_element));
		break;

	case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
	case OpTypedObject::WIDGET_TYPE_EDIT:
		form_eoi = OP_NEW(TextEditElementOfInterest, (html_element));
		break;


	case OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT:
		form_eoi = OP_NEW(FileChooserElementOfInterest, (html_element));
		break;

	default:
		form_eoi = OP_NEW(FormElementOfInterest, (html_element));
	}

	return form_eoi;
}

void FormElementOfInterest::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (source->GetListener())
		source->GetListener()->OnMouseEvent(source, pos, x, y, button, down, nclicks);
}

void FormElementOfInterest::OnPaint(OpWidget *widget, const OpRect &paint_rect)
{
	OpWidget *root = widget_container->GetRoot();
	if (widget == root)
		PaintFragments(paint_rect, widget->GetVisualDevice()); // FIXME: OOM
}

void FormElementOfInterest::OnClick(OpWidget *widget, UINT32 id)
{
	GetElementExpander()->Activate(this);
}

void FormElementOfInterest::DelayOnClick(void* arg/* = 0*/)
{
	MH_PARAM_1 par1 = reinterpret_cast<MH_PARAM_1>(this);
	MH_PARAM_2 par2 = reinterpret_cast<MH_PARAM_2>(arg);
	const OpMessage msg = MSG_FINGERTOUCH_DELAYED_CLICK;
	if (OpStatus::IsMemoryError(g_main_message_handler->SetCallBack(this, msg, par1)))
	{
		source->ReportOOM();
		return;
	}
	OP_ASSERT(g_main_message_handler->HasCallBack(this, msg, par1));
	g_main_message_handler->RemoveDelayedMessage(msg, par1, par2);
	g_main_message_handler->PostDelayedMessage(msg, par1, par2, 0);
}

void FormElementOfInterest::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_EXPAND_WIDGET_MESSAGE)
	{
		if (g_widget_globals->captured_widget != clone)
			MakeClone(TRUE);
	}
}

void FormElementOfInterest::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (source->GetListener())
		source->GetListener()->OnChange(source, changed_by_mouse);
}


void FormElementOfInterest::DoActivate()
{
	((OpButton*)source)->Click();
	((OpButton*)clone)->SetValue(source->GetValue());
	source->SetFocus(FOCUS_REASON_MOUSE);

	OpRect bigger_dest(dest_rect.x - dest_rect.width * 2, dest_rect.y - dest_rect.height * 2, dest_rect.width *4 , dest_rect.height * 4);
	ScheduleAnimation(dest_rect, 1.0f, bigger_dest, 0.0f, 400);

}

OP_STATUS FormElementOfInterest::InitContent()
{
	source = GetWidgetFromHtmlElement();
	if (!source)
		return OpStatus::ERR;

	widget_a_frag = OP_NEW(WidgetAnchorFragment, (source->GetFormObject()));
	if (!widget_a_frag)
		return OpStatus::ERR_NO_MEMORY;
	widget_a_frag->IntoStart(&region);

	RETURN_IF_ERROR(MakeClone(FALSE));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_EXPAND_WIDGET_MESSAGE, (MH_PARAM_1)this));
	return OpStatus::OK;
}

FormElementOfInterest::~FormElementOfInterest()
{
	g_main_message_handler->UnsetCallBacks(this);
}

OpWidget* FormElementOfInterest::GetWidgetFromHtmlElement()
{
	OP_ASSERT(GetHtmlElement()->GetFormObject());
	OP_ASSERT(GetHtmlElement()->GetFormObject()->GetWidget());

	if (!GetHtmlElement()->GetFormObject() || !GetHtmlElement()->GetFormObject()->GetWidget())
		return NULL;

	return GetHtmlElement()->GetFormObject()->GetWidget();
}

OpPoint FormElementOfInterest::UpdateFragmentPositions(int preferred_w, int preferred_h)
{
	// Calculate new dest_rect
	int total_w = 0;
	int total_h = 0;
	int x = LAYER_PADDING;
	OpWidget *root = widget_container->GetRoot();
	for (AnchorFragment* fragment = (AnchorFragment*) region.First(); fragment; fragment = fragment->Suc())
	{
		TextAnchorFragment *text_anchor_frag = fragment->GetTextAnchorFragment();
		if (text_anchor_frag)
		{
			OP_STATUS err = text_anchor_frag->UpdateWidgetString(root, GetMaxWidth() - preferred_w, FALSE);
			OP_ASSERT(OpStatus::IsSuccess(err));
			if (OpStatus::IsError(err))
				return OpPoint(0, 0); // Bad error handling.
		}

		int frag_w = fragment->GetWidth();
		int frag_h = fragment->GetHeight();
		fragment->SetPos(x, (preferred_h + LAYER_PADDING*2 - fragment->GetHeight()) / 2);

		total_w += frag_w;
		total_h = MAX(total_h, frag_h);
		x += frag_w;
	}

	return OpPoint(total_w, total_h);
}

int FormElementOfInterest::GetPreferredNumberOfRows(BOOL expanded) const
{
	return 5;
}

OP_STATUS FormElementOfInterest::MakeClone(BOOL expanded)
{
	OpRect anim_start_rect;
	float  anim_start_opacity = 0.0f;
	if (expanded)
	{
		if (GetElementExpander()->GetState() == EE_EXPANDED)
		{
			anim_start_rect = dest_rect;
			anim_start_opacity = 1.0f;
		}
		else
		{
			anim_start_rect = orig_rect;
		}
	}

	OpWidget *old_widget = clone;
	OpWidget *root = widget_container->GetRoot();

	RETURN_IF_ERROR(source->CreateClone(&clone, root, WIDGETS_F_T_FONTSIZE, expanded));

	if (old_widget)
	{
		old_widget->Remove();
		old_widget->Delete();
	}

	INT32 cols = 20;
	INT32 rows = GetPreferredNumberOfRows(expanded);

	int minimum_size = (ElementExpander::GetFingertipPixelRadius(widget_container->GetWindow()) - LAYER_PADDING) * 2;
	int preferred_w = MAX(minimum_size, source->GetBounds().width);
	int preferred_h = MAX(MAX(minimum_size, source->GetBounds().height), dest_rect.height);
	minimum_size = GetElementExpander()->GetElementMinimumSize();
	if (preferred_w < minimum_size)
		preferred_w = minimum_size;
	if (preferred_h < minimum_size)
		preferred_h = minimum_size;

	clone->GetVisualDevice()->SetFont(font);
	clone->GetPreferedSize(&preferred_w, &preferred_h, cols, rows);
	clone->SetListener(this);

	OpRect visible_rect;
	GetElementExpander()->GetAvailableRect(visible_rect);

	// Limit size to screensize
	preferred_w = MIN(preferred_w, visible_rect.width - LAYER_PADDING * 2);
	preferred_h = MIN(preferred_h, visible_rect.height - LAYER_PADDING * 2);

	widget_a_frag->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, preferred_w, preferred_h), 0);

	// Calculate new dest_rect
	int total_w = 0;
	int total_h = preferred_h;
	int x = LAYER_PADDING;

	for (AnchorFragment* fragment = (AnchorFragment*) region.First(); fragment; fragment = fragment->Suc())
	{
		int y = preferred_h + LAYER_PADDING * 2 - fragment->GetHeight();
		TextAnchorFragment *text_anchor_frag = fragment->GetTextAnchorFragment();
		if (text_anchor_frag)
		{
			RETURN_IF_ERROR(text_anchor_frag->UpdateWidgetString(root, GetMaxWidth() - preferred_w, FALSE));
		}

		int frag_w = fragment->GetWidth();
		int frag_h = fragment->GetHeight();

		// Checkbox and radiobutton captions should not be too close to the image.
		if (text_anchor_frag)
		{
			switch (clone->GetType())
			{
				case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
				case OpTypedObject::WIDGET_TYPE_CHECKBOX:
					// TODO: Adapt this to RTL languages!
					y = (frag_h + LAYER_BG_PADDING * 2 - font.GetHeight() + (preferred_h - frag_h)) / 2;
					x += 5;
					frag_w += 5;
					break;
			}
		}

		fragment->SetPos(x, y);

		total_w += frag_w;
		total_h = MAX(total_h, frag_h);
		x += frag_w;
    }

	int y = LAYER_PADDING;
	switch (clone->GetType())
	{
		case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
		case OpTypedObject::WIDGET_TYPE_CHECKBOX:
			y = (total_h+2*LAYER_PADDING-preferred_h)/2;
			break;
	}

	clone->SetRect(OpRect(LAYER_PADDING, y, preferred_w, preferred_h));

	dest_rect.width = total_w;
	dest_rect.height = total_h;

	if (expanded)
	{
		dest_rect.width += LAYER_PADDING * 2;
		dest_rect.height += LAYER_PADDING * 2;
		ElementExpanderImpl::SmartPositionRectInRect(dest_rect, visible_rect,
													 ElementExpanderImpl::EDGE_NORTH,
													 ElementExpanderImpl::EDGE_WEST);
		AdjustSize();
		ScheduleAnimation(anim_start_rect, anim_start_opacity, dest_rect, 1.0f, 400);
	}
	else
	{
		AdjustSize();
	}

	return OpStatus::OK;
}

void FormElementOfInterest::OnEnd()
{
	// Release focus now since virtual listener functions will be deinitialized during destruction so things wouldn't be called correctly then.
	if (clone)
		clone->ReleaseFocus();
}

#endif // NEARBY_ELEMENT_DETECTION
