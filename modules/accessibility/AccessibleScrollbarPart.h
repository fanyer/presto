/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBLE_SCROLLBAR_PART_H
#define ACCESSIBLE_SCROLLBAR_PART_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibleTextObject.h"

/** A lightweight class for sub-parts of a scrollbar.
  * The scrollbar class needs to provide a AccessibleTextObjectListener,
  * that the part can call to get information on where it is,
  * as well as notify when it gets hit.
  */

class AccessibleScrollbarPart : public AccessibleTextObject
{
public:
	typedef enum {
		kPartArrowUp = 0,
		kPartArrowDown,
		kPartPageUp,
		kPartPageDown,
		kPartKnob
	} PartCode;
	AccessibleScrollbarPart(AccessibleTextObjectListener* parent, PartCode part);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBLE_SCROLLBAR_PART_H
