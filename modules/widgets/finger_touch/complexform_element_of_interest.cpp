/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/complexform_element_of_interest.h"

BOOL ComplexFormElementOfInterest::DoHasEditMode() const
{
	return TRUE;
}

void ComplexFormElementOfInterest::DoActivate()
{
	source->SetFocus(FOCUS_REASON_MOUSE);
	MakeClone(TRUE);
	clone->SetFocus(FOCUS_REASON_MOUSE);
}


BOOL ComplexFormElementOfInterest::OnMouseEventConsumable(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 &&
		GetElementExpander()->GetState() == EE_EXPANDED)
	{
		// Activate edit-mode-enabled eois if expanded and eat the mouse event
		if (!down)
			GetElementExpander()->Activate(this);
		return TRUE;
	}
	else if (GetElementExpander()->GetState() != EE_EDITMODE)
	{
		// Eat events when not in edit mode for edit-mode-enabled eois
		return TRUE;
	}
	return FALSE;
}

#endif // NEARBY_ELEMENT_DETECTION
