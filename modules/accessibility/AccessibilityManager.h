/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPACCESSIBILITYMANAGER_H
#define OPACCESSIBILITYMANAGER_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/pi/OpAccessibilityAdapter.h"
#include "modules/accessibility/accessibilityenums.h"
class OpAccessibleItem;

class AccessibilityManager
{
public:

	static AccessibilityManager* GetInstance();

	OP_STATUS SendEvent(OpAccessibleItem* sender, Accessibility::Event evt);
	OpAccessibilityAdapter* GetAdapterForAccessibleItem(OpAccessibleItem* accessible_item, BOOL create_new = TRUE);
	void AccessibleItemRemoved (const OpAccessibleItem* accessible_item);


private:
	AccessibilityManager() {}
	~AccessibilityManager() {}

	AccessibilityManager(const AccessibilityManager&);
	AccessibilityManager& operator=(const AccessibilityManager&);

	OpPointerHashTable<const OpAccessibleItem, OpAccessibilityAdapter> m_item_adapters;
};

#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#endif //OPACCESSIBILITYMANAGER_H
