/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/models/DesktopGroupModelItem.h"

#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

void DesktopGroupModelItem::SetActiveDesktopWindow(DesktopWindow* window)
{
	if (m_active_desktop_window == window)
		return;

	m_active_desktop_window = window;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
		m_listeners.GetNext(iterator)->OnActiveDesktopWindowChanged(this, window);

	Change();
}

void DesktopGroupModelItem::UnsetActiveDesktopWindow(DesktopWindow* window)
{
	if (m_active_desktop_window != window)
		return;

	// If this was the active desktop window, choose a different one
	DesktopWindowCollectionItem* new_active = GetChildItem();
	if (new_active == &window->GetModelItem())
		new_active = new_active->GetSiblingItem();
	SetActiveDesktopWindow(new_active->GetDesktopWindow());
}

void DesktopGroupModelItem::SetCollapsed(bool collapsed)
{
	if (m_collapsed == collapsed)
		return;

	m_collapsed = collapsed;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
		m_listeners.GetNext(iterator)->OnCollapsedChanged(this, collapsed);
}

OP_STATUS DesktopGroupModelItem::GetItemData(ItemData* item_data)
{
	DesktopWindow* window = GetActiveDesktopWindow();
	if (!window)
		return OpStatus::OK;

	return window->GetItemData(item_data);
}

/* virtual */ void
DesktopGroupModelItem::OnRemoving()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
		m_listeners.GetNext(iterator)->OnGroupDestroyed(this);
}

/* virtual */void
DesktopGroupModelItem::OnAdded()
{
	DesktopWindowCollectionItem* child = GetChildItem();
	OpWorkspace* old_workspace = child ? child->GetDesktopWindow()->GetWorkspace() : NULL;
	OpWorkspace* new_workspace = GetToplevelWindow()->GetDesktopWindow()->GetWorkspace();

	if (old_workspace != new_workspace)
	{
		if (old_workspace)
			old_workspace->OnGroupRemoved(*this);
		new_workspace->OnGroupAdded(*this);
	}

}
