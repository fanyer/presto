/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef TEXT_EDIT_ELEMENT_OF_INTEREST_H
#define TEXT_EDIT_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/complexform_element_of_interest.h"

class HTML_Element;

class TextEditElementOfInterest : public ComplexFormElementOfInterest
{
public:
	TextEditElementOfInterest(HTML_Element* html_element)
		: ComplexFormElementOfInterest(html_element) {}

protected:
	// Implementing OpWidgetListener:
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void OnChangeWhenLostFocus(OpWidget *widget);

private:
	virtual OP_STATUS MakeClone(BOOL expanded);

	// Implementing ElementOfInterest
	virtual BOOL DoIsSuitable(unsigned int max_elm_size);
};

#endif // TEXT_EDIT_ELEMENT_OF_INTEREST_H

