/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FILECHOOSER_ELEMENT_OF_INTEREST_H
#define FILECHOOSER_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/form_element_of_interest.h"

class FileChooserElementOfInterest : public FormElementOfInterest
{
public:
	FileChooserElementOfInterest(HTML_Element* html_element)
		: FormElementOfInterest(html_element) {}

protected:
	// Implementing OpWidgetListener:
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);

	// for delayed OnClick
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	// Override FormElementOfInterest
	virtual void DoActivate();
	virtual OP_STATUS MakeClone(BOOL expanded);
};

#endif // FILECHOOSER_ELEMENT_OF_INTEREST_H
