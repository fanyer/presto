/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/element_expander_container.h"
#include "modules/widgets/WidgetContainer.h"

BOOL ElementExpanderContainer::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_UNFOCUS_FORM:
		if (element_expander->GetState() == EE_EDITMODE)
			element_expander->Hide(TRUE);
		return TRUE;
	}

	return WidgetContainer::OnInputAction(action);
}

#endif // NEARBY_ELEMENT_DETECTION
