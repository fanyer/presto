/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibilityManager.h"
#include "modules/accessibility/opaccessibleitem.h"

AccessibilityManager* AccessibilityManager::GetInstance()
{
	static AccessibilityManager instance;
	return &instance;
}

OP_STATUS AccessibilityManager::SendEvent(OpAccessibleItem *sender, Accessibility::Event evt)
{
	if (!sender->AccessibilityIsReady())
		return OpStatus::ERR;

	RETURN_IF_ERROR(OpAccessibilityAdapter::SendEvent(sender, evt));

	if (!m_item_adapters.Contains(sender))
	{
		// Create a placeholder, so future calls to GetAdapterForAccessibleItem, even with
		// create_new == FALSE can be mapped up. This avoids allocating memory if there aren't
		// any a11y clients listening.
		m_item_adapters.Add(sender, NULL);
	}

	return OpStatus::OK;
}

void AccessibilityManager::AccessibleItemRemoved(const OpAccessibleItem *accessible_item)
{
	OpAccessibilityAdapter* adapter = NULL;
	RETURN_VOID_IF_ERROR(m_item_adapters.Remove(accessible_item, &adapter));

	if (adapter != NULL)
		adapter->AccessibleItemRemoved();
}

OpAccessibilityAdapter* AccessibilityManager::GetAdapterForAccessibleItem(OpAccessibleItem *accessible_item, BOOL create_new)
{
	OpAccessibilityAdapter* adapter;
	if (!m_item_adapters.Contains(accessible_item))
	{
		if (!create_new)
			return NULL;

		if (OpStatus::IsError(OpAccessibilityAdapter::Create(&adapter, accessible_item)))
			return NULL;

		if (OpStatus::IsError(m_item_adapters.Add(accessible_item, adapter)))
		{
			delete adapter;
			return NULL;
		}
		return adapter;
	}
	
	RETURN_VALUE_IF_ERROR(m_item_adapters.GetData(accessible_item, &adapter), NULL);
	if (adapter == NULL)
	{
		RETURN_VALUE_IF_ERROR(m_item_adapters.Remove(accessible_item, &adapter), NULL);

		if (OpStatus::IsError(OpAccessibilityAdapter::Create(&adapter, accessible_item)))
			return NULL;

		if (OpStatus::IsError(m_item_adapters.Add(accessible_item, adapter)))
		{
			delete adapter;
			return NULL;
		}
	}

	return adapter;

}


#endif //ACCESSIBILITY_EXTENSION_SUPPORT
