/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/models/DesktopWindowCollectionItem.h"

#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

DesktopWindowCollectionItem* DesktopWindowCollectionItem::GetToplevelWindow()
{
	if (!GetModel())
		return NULL;

	DesktopWindowCollectionItem* toplevel = this;
	while (toplevel->GetParentItem())
		toplevel = toplevel->GetParentItem();

	return toplevel;
}

OP_STATUS DesktopWindowCollectionItem::GetDescendants(OpVector<DesktopWindow>& descendants, Type type)
{
	if (!GetModel())
		return OpStatus::OK;

	for (INT32 i = GetIndex() + 1; i <= GetIndex() + GetSubtreeSize(); i++)
	{
		DesktopWindow* window = GetModel()->GetItemByIndex(i)->GetDesktopWindow();
		if (window && (type == WINDOW_TYPE_UNKNOWN || type == window->GetType()))
			RETURN_IF_ERROR(descendants.Add(window));
	}

	return OpStatus::OK;
}

UINT32 DesktopWindowCollectionItem::CountDescendants(Type type)
{
	if (!GetModel())
		return 0;

	UINT32 count = 0;
	for (INT32 i = GetIndex() + 1; i <= GetIndex() + GetSubtreeSize(); i++)
	{
		count += GetModel()->GetItemByIndex(i)->GetType() == type;
	}

	return count;
}

bool DesktopWindowCollectionItem::IsAncestorOf(const DesktopWindowCollectionItem& other)
{
	for (DesktopWindowCollectionItem* parent = other.GetParentItem(); parent != NULL;
			parent = parent->GetParentItem())
	{
		if (parent == this)
			return true;
	}
	return false;
}
