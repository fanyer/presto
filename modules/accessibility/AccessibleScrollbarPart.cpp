/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibleScrollbarPart.h"

AccessibleScrollbarPart::AccessibleScrollbarPart(AccessibleTextObjectListener* parent, PartCode part)
	: AccessibleTextObject(parent, part)
{
}

Accessibility::ElementKind AccessibleScrollbarPart::AccessibilityGetRole() const
{
	switch (GetIndex())
	{
		case kPartArrowUp:
			return Accessibility::kElementKindScrollbarPartArrowUp;
		case kPartArrowDown:
			return Accessibility::kElementKindScrollbarPartArrowDown;
		case kPartPageUp:
			return Accessibility::kElementKindScrollbarPartPageUp;
		case kPartPageDown:
			return Accessibility::kElementKindScrollbarPartPageDown;
		case kPartKnob:
			return Accessibility::kElementKindScrollbarPartKnob;
		default:
			OP_ASSERT(0);
			return Accessibility::kElementKindUnknown;
	}
}

OP_STATUS AccessibleScrollbarPart::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	return m_parent->GetAbsolutePositionOfObject(GetIndex(), rect);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
