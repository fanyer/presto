/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/logdoc/htm_elm.h"
#include "modules/widgets/finger_touch/form_element_of_interest.h"
#include "modules/widgets/finger_touch/button_element_of_interest.h"
#include "modules/widgets/OpButton.h"

ButtonElementOfInterest::~ButtonElementOfInterest()
{
	// for buttons, unlike the rest of form elements, we have to create our own button widget,
	// so delete it when we're done.
	if (source)
		source->Delete();
}

OpWidget* ButtonElementOfInterest::GetWidgetFromHtmlElement()
{
	HTML_Element *he = GetHtmlElement();
	OpButton* button;

	OP_STATUS status = OpButton::Construct(&button);

	if(OpStatus::IsError(status))
		return NULL;

	// copy the button html element text into the widget object
	// TODO: images in buttons
	OpString text;
	int text_len = he->GetTextContentLength();
	if (text_len > 0)
	{
		uni_char* text_p = text.Reserve(text_len+1); // add one for the NULL
		if (!text_p)
			return NULL;
		he->GetTextContent(text_p, text_len+1);
	}
	else
		text.Empty();

	button->SetText(text.CStr());

	return button;
}

#endif // NEARBY_ELEMENT_DETECTION
