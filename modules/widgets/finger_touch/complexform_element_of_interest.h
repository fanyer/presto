/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COMPLEXFORM_ELEMENT_OF_INTEREST_H
#define COMPLEXFORM_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/form_element_of_interest.h"

class ComplexFormElementOfInterest : public FormElementOfInterest
{
public:
	ComplexFormElementOfInterest(HTML_Element* html_element)
		: FormElementOfInterest(html_element) {}

	// Override ElementOfInterest
	virtual BOOL DoHasEditMode() const;
	virtual void DoActivate();

	// From OpWidgetListener
	virtual BOOL OnMouseEventConsumable(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
};

#endif // COMPLEXFORM_ELEMENT_OF_INTEREST_H
