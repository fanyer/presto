/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef BUTTON_ELEMENT_OF_INTEREST_H
#define BUTTON_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/complexform_element_of_interest.h"

class ButtonElementOfInterest : public FormElementOfInterest
{
public:
	ButtonElementOfInterest(HTML_Element* html_element)
		: FormElementOfInterest(html_element) {}

	~ButtonElementOfInterest();

private:
	virtual OpWidget* GetWidgetFromHtmlElement();
};

#endif // BUTTON_ELEMENT_OF_INTEREST_H
