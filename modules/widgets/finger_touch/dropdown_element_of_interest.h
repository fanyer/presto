/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DROPDOWN_ELEMENT_OF_INTEREST_H
#define DROPDOWN_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/complexform_element_of_interest.h"

class DropDownElementOfInterest : public ComplexFormElementOfInterest
{
public:
	DropDownElementOfInterest(HTML_Element* html_element)
		: ComplexFormElementOfInterest(html_element) {}

	int GetPreferredNumberOfRows(BOOL expanded) const;

	// Implementing OpWidgetListener:
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
};

#endif // DROPDOWN_ELEMENT_OF_INTEREST_H
