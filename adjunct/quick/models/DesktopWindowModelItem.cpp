/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/models/DesktopWindowModelItem.h"

#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

bool DesktopWindowModelItem::AcceptsInsertInto()
{
	// Accepts items if not yet in a group
	return !GetParentItem() || GetParentItem()->GetType() != WINDOW_TYPE_GROUP;
}

INT32 DesktopWindowModelItem::GetID()
{
	return m_desktop_window->GetID();
}

OpTypedObject::Type DesktopWindowModelItem::GetType()
{
	return m_desktop_window->GetType();
}

OP_STATUS DesktopWindowModelItem::GetItemData(ItemData* item_data)
{
	return m_desktop_window->GetItemData(item_data);
}

void DesktopWindowModelItem::OnAdded()
{
	// Reset workspace parent in case it changed
	m_desktop_window->ResetParentWorkspace();

	// Check for active group membership
	DesktopGroupModelItem* group = m_desktop_window->GetParentGroup();
	if (group && m_desktop_window->IsActive())
		group->SetActiveDesktopWindow(m_desktop_window);
}
