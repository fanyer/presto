/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef UNIX_ACCESSIBILITY_EXTENSION_H
#define UNIX_ACCESSIBILITY_EXTENSION_H

#include "modules/accessibility/AccessibilityManager.h"
#include "modules/accessibility/opaccessibleitem.h"

/**
 * @brief The unix OpAccessibilityAdapter
 * @author Patricia Aas
 */

class UnixAccessibilityExtension :
	public OpAccessibilityAdapter
{
public:
	UnixAccessibilityExtension(OpAccessibleItem* accessible_item) :
		m_item(accessible_item) {}

	// Implementing OpAccessibilityAdapter interface

	virtual ~UnixAccessibilityExtension() {}
	virtual void AccessibleItemRemoved() {}
	virtual OpAccessibleItem* GetAccessibleItem() { return m_item; }

private:

	OpAccessibleItem* m_item;
};

#endif // UNIX_ACCESSIBILITY_EXTENSION_H
