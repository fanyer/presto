/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/webforms2support.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/piforms.h"
#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#ifdef _AUTO_WIN_RELOAD_SUPPORT_
# include "modules/dochand/win.h" // For Window and AutoWinReloadParam
#endif // _AUTO_WIN_RELOAD_SUPPORT_
#include "modules/ecmascript_utils/essched.h"
#include "modules/dom/domenvironment.h"

/* static */
OP_STATUS
FormValueListener::HandleValueChanged(FramesDocument* doc, HTML_Element* helm,
									  BOOL send_onchange_event,
									  BOOL has_user_origin,
									  ES_Thread* thread)
{
	OP_ASSERT(doc);
	OP_ASSERT(helm);
	OP_ASSERT(helm->GetNsType() == NS_HTML);

#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	// Disable autoreload in this window when we have changed a textbox.
	if (has_user_origin)
	{
		HTML_ElementType type = helm->Type();
		if (type == HE_TEXTAREA || type == HE_INPUT)
		{
			// Textareas don't have INPUT_RADIO or INPUT_CHECKBOX input types
			InputType inp_type = helm->GetInputType();
			Window* window = doc->GetWindow();
			if (window &&
				inp_type != INPUT_RADIO && inp_type != INPUT_CHECKBOX)
			{
				AutoWinReloadParam* autoparams = window->GetUserAutoReload();
				if (autoparams->GetOptIsEnabled())
				{
					autoparams->Enable(FALSE);
				}
			}
		}
	}
#endif // _AUTO_WIN_RELOAD_SUPPORT_

	FormValue* form_value = helm->GetFormValue();
	if (has_user_origin)
	{
		if (helm->Type() == HE_TEXTAREA)
		{
			FormObject* form_obj = helm->GetFormObject();
			TextAreaObject* textarea = static_cast<TextAreaObject*>(form_obj);
			// It can have disappeared if this was the last that
			// happened before it was removed, for instance if it was
			// removed when the focus was in it so a
			// OnChangeWhenLostFocus was triggered
			if (textarea)
			{
				textarea->SetIsUserModified();
			}
		}

		// After someone has started to write/meddle with a form we
		// shouldn't move the document or any focus
		doc->SetObeyNextAutofocus(FALSE);

		form_value->SetIsChangedFromOriginalByUser(helm);

		if (helm->GetInputType() == INPUT_RADIO)
		{
			FormValueRadioCheck::GetAs(form_value)->SetWasChangedExplicitly(doc, helm);
		}
		if (helm->GetInputType() == INPUT_CHECKBOX)
		{
			// The checkbox is invoked so indeterminate should now be FALSE.
			// No need to apply property changes since it's done below
			helm->SetIndeterminate(doc, FALSE, FALSE);
		}
	}

	form_value->SetIsChangedFromOriginal(helm);

	// This might have changed the CSS3 UI Pseudo classes
	if (HTML_Element *root = doc->GetDocRoot())
	{
		if (root->IsAncestorOf(helm))
		{
			doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
	}

	ShiftKeyState keystate = 0;
	if (has_user_origin)
	{
		OP_ASSERT(doc->GetVisualDevice()->view); // Since it's user_origin
		keystate = doc->GetVisualDevice()->view->GetShiftKeys();
		RETURN_IF_ERROR(doc->HandleEvent(ONINPUT, NULL, helm, keystate, 0, thread));
	}

	if (send_onchange_event)
	{
		// Invalidate the collection cache before firing onchange.
		if (helm->IsMatchingType(HE_SELECT, NS_HTML))
			if (DOM_Environment *environment = doc->GetDOMEnvironment())
				environment->ElementCollectionStatusChanged(helm, DOM_Environment::COLLECTION_SELECTED_OPTIONS);

		RETURN_IF_ERROR(doc->HandleEvent(ONCHANGE, NULL, helm, keystate, 0, thread));
	}
	return OpStatus::OK;
}
