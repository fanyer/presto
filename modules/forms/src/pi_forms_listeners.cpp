/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/forms/pi_forms_listeners.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpFileChooserEdit.h"
#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/piforms.h"
#include "modules/display/coreview/coreview.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/webforms2support.h"
#include "modules/locale/locale-enum.h"
#ifdef _WML_SUPPORT_
# include "modules/locale/oplanguagemanager.h"
# include "modules/forms/src/validationerrorwindow.h"
#endif // _WML_SUPPORT_

// == Misc functions used in this file ============================================

static void AutoCorrectURLFieldIfNeeded(FramesDocument* doc, HTML_Element* helm)
{
	// FIXME: Reuse auto correction code used in the desktop address bar?
	FormValue* form_value = helm->GetFormValue();
	if (!form_value)
		return;

	FormValidator validator(doc);
	if (validator.ValidateSingleControl(helm).HasError(VALIDATE_ERROR_TYPE_MISMATCH))
	{
		OpString uri;
		if (OpStatus::IsSuccess(form_value->GetValueAsText(helm, uri)) &&
		    OpStatus::IsSuccess(uri.Insert(0, "http://")))
		{
			// If the corrected value is valid, use it
			if (validator.ValidateSingleControlAgainstValue(helm, uri.CStr()).IsOk())
			{
				form_value->SetValueFromText(helm, uri.CStr());
				OP_STATUS status = FormValueListener::HandleValueChanged(doc, helm, TRUE, TRUE, NULL);
				if(OpStatus::IsMemoryError(status))
				{
					doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				}
			}
		}
	}
}

// == WidgetListener =================================================

/* virtual */
void WidgetListener::OnGeneratedClick(OpWidget *widget, UINT32 id /* = 0 */)
{
	HandleGeneratedClick(widget, NULL);
}

void WidgetListener::HandleGeneratedClick(OpWidget* widget, ES_Thread* thread)
{
	// Then nobody has sent the ONCLICK event, so we have to do it. This is not right since
	// if the onclick is cancelled, we shouldn't have done the click but it's too late
	// to cancel the action already.

	// The radiobutton/checkbox state is already saved in OnClick()
	FramesDocument* doc = doc_man->GetCurrentDoc();
	if (helm->IsDisabled(doc))
		return;

	OpPoint doc_pos = widget->GetPosInDocument().GetTranslation();
	OP_STATUS status = doc->HandleMouseEvent(ONCLICK,
					NULL, helm, NULL,
					0, 0,       // x and y relative to the element
					doc_pos.x, doc_pos.y, // x and y relative to the document
					0, // keystate
					MAKE_SEQUENCE_COUNT_AND_BUTTON(1, MOUSE_BUTTON_1),
					thread);
	if(OpStatus::IsMemoryError(status))
	{
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

void WidgetListener::OnClick(OpWidget *widget, UINT32 id /* = 0 */)
{
	if (helm->IsMatchingType(HE_INPUT, NS_HTML) &&
		(helm->GetInputType() == INPUT_CHECKBOX || helm->GetInputType() == INPUT_RADIO))
	{
		FormValueRadioCheck* form_value = FormValueRadioCheck::GetAs(helm->GetFormValue());
		OP_ASSERT(form_value);
		// We will only get a click event if the widget was enabled and changed its value, so restore the old value, save it and then revert the change.
		if (helm->GetInputType() == INPUT_CHECKBOX)
		{
			BOOL after_click_value = !!widget->GetValue();
			form_value->SetIsChecked(helm, !after_click_value, NULL, FALSE);
			form_value->SaveStateBeforeOnClick(helm);
			form_value->SetIsChecked(helm, after_click_value, NULL, FALSE);
		}
		else
		{
			// Radio buttons will change when this function
			// has ended in RadioButtonListener::OnClick
			form_value->SaveStateBeforeOnClick(helm);
		}
	}

	// Some pseudo classes might have changed (:default, :changed for instance)
	FramesDocument* doc = doc_man->GetCurrentDoc();
	doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
}

void WidgetListener::OnChange(OpWidget *object, BOOL changed_by_mouse)
{
	FramesDocument* doc = doc_man->GetCurrentDoc();
	FormValue* value = helm->GetFormValue();
	if (value->GetType() == FormValue::VALUE_RADIOCHECK)
	{
		// This is handled in the ONCLICK handler
		// in HTML_Element::HandleEvent to get event order correct.
		//
		// Special handling needed for mouse interactions that travel
		// outside the 'onclick radius' only to return and finish up inside
		// the button. These must also trigger an ONCLICK from the
		// mouseup. Do this by reverting the state kept on the document
		// that records status of the mouse interaction.
		// HTML_Element::HandleEvent() will take notice.
		OpButton *button = static_cast<OpButton *>(object);
		if (button->IsInside() && button->GetValue() && !doc->GetMouseUpMayBeClick())
			doc->ResetMouseUpMayBeClick();
		return;
	}

	BOOL is_textedit = helm->Type() == HE_INPUT && FormObject::IsEditFieldType(helm->GetInputType()) || helm->Type() == HE_TEXTAREA;

	// The document (FormValue) must get to know the changes the user just did
	if (helm->Type() == HE_SELECT)
	{
		FormValueList::GetAs(helm->GetFormValue())->SyncWithFormObject(helm);
	}
	else if (helm->Type() == HE_INPUT && helm->GetInputType() == INPUT_FILE && !helm->GetFormValue()->IsValueExternally())
	{
		/* The form element has been unexternalized while the user was choosing
		   a file. Propagate the now-current value to the form value. */
		OpFileChooserEdit *file_chooser = static_cast<OpFileChooserEdit *>(object);
		OpString value;
		if (OpStatus::IsMemoryError(file_chooser->GetText(value)) ||
		    OpStatus::IsMemoryError(helm->GetFormValue()->SetValueFromText(helm, value.CStr())))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}

	// Send events and such
	OP_STATUS status = FormValueListener::HandleValueChanged(doc, helm, !is_textedit, TRUE, NULL);
	if(OpStatus::IsMemoryError(status))
	{
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

void WidgetListener::OnChangeWhenLostFocus(OpWidget *widget)
{
	FramesDocument* doc = doc_man->GetCurrentDoc();
	if (doc == NULL)
		return;

#ifdef DEBUG_ENABLE_OPASSERT
	InputType input_type;
	BOOL is_textedit = helm->Type() == HE_INPUT &&
		(input_type = helm->GetInputType(),
		 input_type == INPUT_TEXT ||
		 input_type == INPUT_PASSWORD ||
		 input_type == INPUT_URI ||
		 input_type == INPUT_WEEK ||
		 input_type == INPUT_EMAIL ||
		 input_type == INPUT_NUMBER ||
		 input_type == INPUT_MONTH ||
		 input_type == INPUT_TEL ||
		 input_type == INPUT_SEARCH) ||
		helm->Type() == HE_TEXTAREA;
	OP_ASSERT(is_textedit);
#endif // DEBUG_ENABLE_OPASSERT

	// Since this is a text field, we have already done everything required except for
	// sending the ONCHANGE event, in the OnChange listener

#ifdef _WML_SUPPORT_
	// When the user leaves the field (thus the check for send_onchange_event)
	// we check validity
	if (doc->GetHLDocProfile() && doc->GetHLDocProfile()->HasWmlContent() &&
		!FormManager::ValidateWmlInputData(doc, helm, TRUE))
	{
		OpString error_message;
		OP_STATUS status = g_languageManager->GetString(Str::SI_WML_INPUT_FORMAT_ERROR, error_message);
		if (OpStatus::IsSuccess(status) && error_message.CStr())
		{
			ValidationErrorWindow::Display(helm, error_message.CStr());
		}
		else if (OpStatus::IsMemoryError(status))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}
#endif // _WML_SUPPORT_
	if (helm->GetInputType() == INPUT_URI)
	{
		// Auto correct it a little if we can
		AutoCorrectURLFieldIfNeeded(doc, helm);
	}

	ShiftKeyState keystate = 0;
	OP_STATUS status = doc->HandleEvent(ONCHANGE, NULL, helm, keystate);
	if (OpStatus::IsMemoryError(status))
	{
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

void WidgetListener::OnSelect(OpWidget *object)
{
	FramesDocument* doc = doc_man->GetCurrentDoc();
	HTML_Document* hdoc = doc->GetHtmlDocument();
	if (hdoc) // obml2d documents have no html_document for instance
	{
		hdoc->SetElementWithSelection(helm);

		OP_STATUS status = doc->HandleEvent(ONSELECT, NULL, helm, SHIFTKEY_NONE);
		if(OpStatus::IsMemoryError(status))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}
}

void WidgetListener::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	FramesDocument* doc = doc_man->GetCurrentDoc();
	HTML_Document* hdoc = doc->GetHtmlDocument();
	if (hdoc) // obml2d documents have no html_document for instance
	{
		OP_STATUS status = doc->HandleEvent(ONSCROLL, NULL, helm, SHIFTKEY_NONE);
		if(OpStatus::IsMemoryError(status))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}
}

/**
 * Resize the specified HTML_Element.
 *
 * This function resizes the the specified HTML_Element by modifying
 * its inline style.
 *
 * @param hldoc The HLDocProfile associated with the HTML_Element.
 * @param elm The HTML_Element to resize.
 * @param w The new width of the HTML_Element (in pixels).
 * @param h The new height of the HTML_Element (in pixels).
 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY.
 */
static OP_STATUS
ResizeElement(HLDocProfile* hldoc, HTML_Element *elm, INT32 w, INT32 h)
{
	const float wf = static_cast<float>(w);
	const float hf = static_cast<float>(h);

	if (w != OpWidgetListener::NO_RESIZE)
		RETURN_IF_ERROR(elm->SetStyleDecl(CSS_PROPERTY_width, wf, CSS_PX));
	if (h != OpWidgetListener::NO_RESIZE)
		RETURN_IF_ERROR(elm->SetStyleDecl(CSS_PROPERTY_height, hf, CSS_PX));

	return elm->ApplyStyle(hldoc);
}

void WidgetListener::OnResizeRequest(OpWidget *widget, INT32 width, INT32 height)
{
	const INT32 no_resize = OpWidgetListener::NO_RESIZE;

	FramesDocument* doc = doc_man->GetCurrentDoc();
	HLDocProfile* hldoc = doc ? doc->GetHLDocProfile() : NULL;

	if (!hldoc)
		return;

	if (!widget->IsBorderBox())
	{
		struct { short left, top, right, bottom; } brd;
		struct { INT32 left, top, right, bottom; } pad;

		widget->GetBorders(brd.left, brd.top, brd.right, brd.bottom);
		widget->GetPadding(&pad.left, &pad.top, &pad.right, &pad.bottom);

		const INT32 adjust_w = (brd.left + brd.right) + (pad.left + pad.right);
		const INT32 adjust_h = (brd.top + brd.bottom) + (pad.top + pad.bottom);

		width -= (width == no_resize) ? 0 : adjust_w;
		height -= (height == no_resize) ? 0 : adjust_h;
	}

	if (OpStatus::IsMemoryError(ResizeElement(hldoc, helm, width, height)))
		doc_man->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

// == RadioButtonListener =================================================

RadioButtonListener::RadioButtonListener(DocumentManager* doc_man, HTML_Element* helm)
	: WidgetListener(doc_man, helm)
{
}

void RadioButtonListener::OnClick(OpWidget *object, UINT32 id)
{
	WidgetListener::OnClick(object);

	helm->GetFormObject()->CheckRadio(helm, object);
	// There will be a ONCLICK event, and the ONCHANGE will be sent in
	// HTML_Element::HandleEvent when that click event arrives. All to
	// get the order of the events correct.
}

// == SelectionObjectListener =================================================

SelectionObjectListener::SelectionObjectListener(DocumentManager* doc_man, HTML_Element* helm)
	: WidgetListener(doc_man, helm)
{
}

void SelectionObjectListener::OnMouseEvent(OpWidget *widget, INT32 index,
										   INT32 widget_x, INT32 widget_y,
										   MouseButton button, BOOL down,
										   UINT8 nclicks)
{
	FramesDocument* doc = doc_man->GetCurrentDoc();
	OP_ASSERT(doc);

	if (index == -1)
	{
		// This was an event on the select itself, rather than on any option.
		return;
	}

	if (static_cast<SelectionObject*>(helm->GetFormObject())->IsListbox())
		return; // DOM events are sent the ordinary way through doc/logdoc

	HTML_Element *he = helm->FirstChildActual();
	int idx = 0;

	while (he)
	{
		if (he->Type() == HE_OPTION)
		{
			if (idx == index)
			{
#ifdef MOUSELESS // We need to send ONCLICK ourselves
				if (!down) // doesn't matter if we send on down or up since they're not for real
					return;

				OpPoint doc_pt = widget->GetPosInDocument().GetTranslation();

				INT32 doc_x = doc_pt.x + widget_x;
				INT32 doc_y = doc_pt.y + widget_y;

				const DOM_EventType mouse_event = ONCLICK;
#else
				INT32 doc_x, doc_y;
				doc->GetVisualDevice()->view->GetMousePos(&doc_x, &doc_y);

				DOM_EventType mouse_event;
				if (down)
				{
					mouse_event = ONMOUSEDOWN;
				}
				else
				{
					// This also makes ONCLICK happen
					mouse_event = ONMOUSEUP;
				}
#endif // MOUSELESS

				int sequence_count_and_button_or_delta = MAKE_SEQUENCE_COUNT_AND_BUTTON(nclicks, button);

				OP_STATUS status = doc->HandleMouseEvent(mouse_event, NULL, he, NULL, widget_x, widget_y, doc_x, doc_y, SHIFTKEY_NONE, sequence_count_and_button_or_delta);
				if (OpStatus::IsMemoryError(status))
				{
					doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				}

				return;
			}

			++idx;
		}
		else if (he->Type() == HE_OPTGROUP)
		{
			he = he->FirstChildActual();
			continue;
		}

		if (he->SucActual())
			he = he->SucActual();
		else if (he->ParentActual()->Type() == HE_OPTGROUP)
			he = he->ParentActual()->SucActual();
		else
			he = NULL;
	}
}
