/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpFileChooserEdit.h"
#include "modules/widgets/finger_touch/filechooser_element_of_interest.h"
#include "modules/widgets/OpFileChooserEdit.h"

void FileChooserElementOfInterest::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	FormElementOfInterest::OnMouseEvent(widget,pos,x,y,button,down,nclicks);

	// Check if it the event came from an OpButton inside an OpFileChooserEdit
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
	{
		/*
			Turn the parent clone widget into a full-featured form widget here.

			This has to be done here since clone widgets should not be
			form widgets, otherwise mouse events would fall through the
			original widgets.  If this is not performed at all, the
			OpFileChooserEdit widget will ignore any meaningful event as
			neither a form object nor a window commander instance have
			been assigned to the class instance in the first place.
		*/

		OpWidget* parent = widget->GetParent();

		parent->SetFormObject(source->GetFormObject());
		parent->GetVisualDevice()->SetDocumentManager(GetElementExpander()->GetDocument()->GetVisualDevice()->GetDocumentManager());
	}
}

void FileChooserElementOfInterest::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpString text;
	if (OpStatus::IsSuccess(widget->GetText(text)))
	{
		source->SetText(text.CStr());
		static_cast<OpFileChooserEdit*>(source)->OnChange(widget, changed_by_mouse);
	}
}

void FileChooserElementOfInterest::DoActivate()
{
	ScheduleAnimation(dest_rect, 1.0f, dest_rect, 0.0f, 0);

	// make OnClick from callback since WindowListener::OnActivate deletes this
	DelayOnClick();

	clone->SetFocus(FOCUS_REASON_MOUSE);
}

void FileChooserElementOfInterest::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_FINGERTOUCH_DELAYED_CLICK:
	{
		g_main_message_handler->UnsetCallBack(this, msg);
		static_cast<OpFileChooserEdit*>(source)->OnClick();
		break;
	}
	default:
		FormElementOfInterest::HandleCallback(msg, par1, par2);
		break;
	}
}

OP_STATUS FileChooserElementOfInterest::MakeClone(BOOL expanded)
{
	OP_STATUS status = FormElementOfInterest::MakeClone(expanded);

	if(status == OpStatus::OK)
	{
		OpFileChooserEdit* filechooser = static_cast<OpFileChooserEdit*>(clone);

		// File choosers are compound widgets, listen to subwidgets events also
		filechooser->GetButton()->SetListener(this);
		filechooser->GetEdit()->SetListener(this);
	}

	return 	status;
}

#endif // NEARBY_ELEMENT_DETECTION
