/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "modules/widgets/WidgetContainer.h"


void DesktopWindowCollection::RemoveDesktopWindow(DesktopWindow* desktop_window)
{
	if(!desktop_window)
		return;

	// Stop listening to it
	StopListening(desktop_window);

	// Broadcast the removal to the listeners
	BroadcastDesktopWindowRemoved(desktop_window);

	// Check up on group after removal
	DesktopWindowCollectionItem* parent = desktop_window->GetModelItem().GetParentItem();
	if (parent && parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
		RemovingFromGroup(*static_cast<DesktopGroupModelItem*>(parent), desktop_window);
}


void DesktopWindowCollection::RemovingFromGroup(DesktopGroupModelItem& group, DesktopWindow* desktop_window)
{
	if (group.GetChildCount() <= 2)
		return UnGroup(&group);

	group.UnsetActiveDesktopWindow(desktop_window);
}


OP_STATUS DesktopWindowCollection::AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window)
{
	if(!desktop_window)
		return OpStatus::ERR;

	StartListening(desktop_window);

	if (desktop_window->GetParentWorkspace())
	{
		DesktopWindowCollectionItem* parent = desktop_window->GetParentWorkspace()->GetModelItem();
		DesktopWindowCollectionItem *dwi_new = &desktop_window->GetModelItem();
		DesktopWindowCollectionItem* dwi = 0;

		if( g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent) && g_application->IsBrowserStarted() )
		{
			DesktopWindow* dw = desktop_window->GetParentWorkspace()->GetActiveDesktopWindow();
			if (dw && dw->GetParentGroup())
				dwi = dw->GetParentGroup();
			else if (dw)
				dwi = &dw->GetModelItem();
		}

		if( dwi )
		{
			dwi->InsertSiblingAfter(dwi_new);
		}
		else if (parent)
		{
			parent->AddChildLast(dwi_new);
		}
		else
		{
			AddLast(dwi_new);
		}
	}
	else if (parent_window)
	{
		parent_window->GetModelItem().AddChildLast(&desktop_window->GetModelItem());
	}
	else
	{
		AddLast(&desktop_window->GetModelItem());
	}

	BroadcastDesktopWindowAdded(desktop_window);

	return OpStatus::OK;
}


void DesktopWindowCollection::ReorderByItem(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem* parent, DesktopWindowCollectionItem* after)
{
	if (&item == parent || (item.GetParentItem() == parent && (&item == after || item.GetPreviousItem() == after)))
		return;

	INT32 parent_index = parent ? parent->GetIndex() : -1;
	INT32 after_index = after ? after->GetIndex() : -1;

	// Make sure there are no separate Remove / Add listener calls, we just want to know it changed
	ModelLock lock(this);

	DesktopWindowCollectionItem* old_previous = item.GetPreviousItem();
	DesktopWindowCollectionItem* old_parent = item.GetParentItem();
	if (old_parent && old_parent != parent && old_parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP && item.GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		RemovingFromGroup(*static_cast<DesktopGroupModelItem*>(old_parent), item.GetDesktopWindow());

		old_previous = NULL;
		old_parent = NULL;
	}

	Move(item.GetIndex(), parent_index, after_index);
	item.OnAdded();
	BroadcastCollectionItemMoved(&item, old_parent, old_previous);
}


void DesktopWindowCollection::ReorderByPos(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem* parent, UINT32 pos)
{
	if (pos == 0)
		return ReorderByItem(item, parent, NULL);

	DesktopWindowCollectionItem* after = parent ? parent->GetChildItem() : GetChildItem(-1);
	for (UINT32 count = 1; count < pos && after && after->GetSiblingItem(); count++)
	{
		after = after->GetSiblingItem();
	}

	return ReorderByItem(item, parent, after);
}


DesktopGroupModelItem* DesktopWindowCollection::CreateGroup(DesktopWindowCollectionItem* parent, DesktopWindowCollectionItem* main, DesktopWindowCollectionItem* second, bool collapsed)
{
	OP_ASSERT(parent || main);
	ModelLock lock(this);

	OpAutoPtr<DesktopGroupModelItem> group (OP_NEW(DesktopGroupModelItem, ()));

	if (main)
	{
		if (!group.get() || main->InsertSiblingAfter(group.get()) < 0)
			return NULL;
	}
	else if (parent)
	{
		if (!group.get() || parent->AddChildFirst(group.get()) < 0)
			return NULL;
	}
	else
		return NULL;

	group->SetCollapsed(collapsed);

	if (!parent && main)
	{
		ReorderByItem(*main, group.get(), NULL);
		if (main && second)
		{
			// Let items in the group take the same order as they had before
			DesktopWindowCollectionItem* after = main->GetIndex() < second->GetIndex() ? main : NULL;
			ReorderByItem(*second, group.get(), after);
		}

		group->SetActiveDesktopWindow(second ? second->GetDesktopWindow() : main->GetDesktopWindow());
	}

	DesktopWindowCollectionItem* base = parent ? parent : main;
	if (base->GetDesktopWindow() && base->GetDesktopWindow()->GetWorkspace())
		base->GetDesktopWindow()->GetWorkspace()->OnGroupCreated(*group);

	return group.release();
}


void DesktopWindowCollection::DestroyGroup(DesktopWindowCollectionItem* group)
{
	ModelLock lock(this);

	for (DesktopWindowCollectionItem* item = group->GetChildItem(); item; item = item->GetSiblingItem())
	{
		DesktopWindow* window = item->GetDesktopWindow();
		if (window && window->IsClosableByUser())
			window->Close(FALSE, TRUE, FALSE);
	}

	UnGroup(group);
}


OP_STATUS DesktopWindowCollection::MergeInto(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem& into, DesktopWindow* new_active)
{
	DesktopGroupModelItem* group;

	if (item.GetType() != OpTypedObject::WINDOW_TYPE_GROUP && into.GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		group = CreateGroup(into, item);
		RETURN_OOM_IF_NULL(group);
	}
	else
	{
		// Decide which to merge into which
		group = static_cast<DesktopGroupModelItem*>(into.GetType() == OpTypedObject::WINDOW_TYPE_GROUP ? &into : &item);
		DesktopWindowCollectionItem& tomerge = group == &into ? item : into;

		ReorderByItem(tomerge, group, group->GetLastChildItem());

		// Check if we just merged two groups
		if (tomerge.GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			UnGroup(&tomerge);
	}

	group->SetActiveDesktopWindow(new_active);
	return OpStatus::OK;
}


void DesktopWindowCollection::OnDragWindows(DesktopDragObject* drag_object, DesktopWindowCollectionItem* target)
{
	// No dragging to the stand alone dragonfly window or outside of containers
	if (!target || !target->GetParentItem() || target->GetType() == OpTypedObject::WINDOW_TYPE_DEVTOOLS ||
		(target->GetDesktopWindow() && target->GetDesktopWindow()->IsDevToolsOnly()))
		return drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);

	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		DesktopWindowCollectionItem* item = GetItemByID(drag_object->GetID(i));

		// Browser windows can't be dragged, can't drag into itself
		if (!item || item->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER || item == target || item == target->GetParentItem())
			return drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
	}

	DesktopDragObject::InsertType insert_type = drag_object->GetSuggestedInsertType();

	if (insert_type == DesktopDragObject::INSERT_INTO && !target->AcceptsInsertInto())
		insert_type = DesktopDragObject::INSERT_BEFORE;

	drag_object->SetInsertType(insert_type);
	drag_object->SetDesktopDropType(DROP_MOVE);
}


void DesktopWindowCollection::OnDropWindows(DesktopDragObject* drag_object, DesktopWindowCollectionItem* target)
{
	if (!target)
		return;

	DesktopWindowCollectionItem* parent = NULL;
	DesktopWindowCollectionItem* after = NULL;
	DesktopWindowCollectionItem* toplevel = target->GetToplevelWindow();

	switch (drag_object->GetInsertType())
	{
		case DesktopDragObject::INSERT_INTO:
			if (target->IsContainer())
			{
				parent = target;
				after = target->GetLastChildItem();
			}
			else
			{
				after = target;
			}
			break;
		case DesktopDragObject::INSERT_BETWEEN:
			after = target;
			break;
		case DesktopDragObject::INSERT_AFTER:
			parent = target->GetParentItem();
			after = target;
			break;
		case DesktopDragObject::INSERT_BEFORE:
			parent = target->GetParentItem();
			after = target->GetPreviousItem();
			break;
	}

	ModelLock lock(this); // Protect multiple changes

	PrepareToplevelMove(drag_object, toplevel);

	for (INT32 i = drag_object->GetIDCount() - 1; i >= 0; i--)
	{
		DesktopWindowCollectionItem* item = GetItemByID(drag_object->GetID(i));
		if (!item || item == parent || item == target)
			continue;

		// Now do the real move
		if (drag_object->GetInsertType() == DesktopDragObject::INSERT_INTO && !parent)
		{
			// Don't allow creating a group from a parent item or inside another group
			if (target->GetParentItem() != item && target->GetParentItem()->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
				parent = CreateGroup(*target, *item);
		}
		else if (!item->IsAncestorOf(*parent))
		{
			ReorderByItem(*item, parent, after);
		}

		// Dissolve group if it has become unnecessary
		if (parent && parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP &&
			item->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			UnGroup(item);
	}

	// Delayed activation in the page bar causes the tab and the page to be out of sync when we drop
	// into or form a new group. After the drop we shall always see the page we moved so we activate it.
	// This is general behavior (not restricted to the page bar) regardless of where a drop occurs.
	if (drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW && drag_object->GetIDCount() == 1)
	{
		DesktopWindow* desktop_window = GetDesktopWindowByID(drag_object->GetID(0));
		if (desktop_window)
			desktop_window->Activate(TRUE, FALSE); // Never activate the parent (browser window). A drop shall not do that
	}
}


void DesktopWindowCollection::PrepareToplevelMove(DesktopDragObject* drag_object, DesktopWindowCollectionItem* toplevel)
{
	for (INT32 i = drag_object->GetIDCount() - 1; i >= 0; i--)
	{
		DesktopWindowCollectionItem* item = GetItemByID(drag_object->GetID(i));
		if (!item || item->GetToplevelWindow() == toplevel)
			continue;

		if (item->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
		{
			// Disband group and add members instead
			for (DesktopWindowCollectionItem* child = item->GetChildItem(); child; )
			{
				drag_object->AddID(child->GetID());

				DesktopWindowCollectionItem* reorder = child;
				child = child->GetSiblingItem();
				ReorderByItem(*reorder, toplevel, NULL);
			}
		}
		else
		{
			// Execute toplevel move
			ReorderByItem(*item, toplevel, NULL);
		}
	}
}


void DesktopWindowCollection::OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url)
{
	// Broadcast the change to the listeners
	BroadcastDocumentWindowUrlAltered(document_window, url);
}


void DesktopWindowCollection::OnDocumentChanging(DocumentDesktopWindow* document_window)
{
	// Broadcast the change to the listeners
	BroadcastDocumentWindowUrlAltered(document_window, UNI_L(""));
}


void DesktopWindowCollection::BroadcastDesktopWindowAdded(DesktopWindow* window)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		DesktopWindowCollection::Listener* item = m_listeners.GetNext(iterator);
		item->OnDesktopWindowAdded(window);
	}
}


void DesktopWindowCollection::BroadcastDesktopWindowRemoved(DesktopWindow* window)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		DesktopWindowCollection::Listener* item = m_listeners.GetNext(iterator);
		item->OnDesktopWindowRemoved(window);
	}
}


void DesktopWindowCollection::BroadcastCollectionItemMoved(DesktopWindowCollectionItem* collection_item, DesktopWindowCollectionItem* old_parent, DesktopWindowCollectionItem* old_previous)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		DesktopWindowCollection::Listener* item = m_listeners.GetNext(iterator);
		item->OnCollectionItemMoved(collection_item, old_parent, old_previous);
	}
}

void DesktopWindowCollection::BroadcastDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,
																const uni_char* url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		DesktopWindowCollection::Listener* item = m_listeners.GetNext(iterator);
		item->OnDocumentWindowUrlAltered(document_window, url);
	}
}


void DesktopWindowCollection::StartListening(DesktopWindow* window)
{
	switch(window->GetType())
	{
	case OpTypedObject::WINDOW_TYPE_DOCUMENT:
		DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(window);
		document->AddDocumentWindowListener(this);
		break;
	}
}


void DesktopWindowCollection::StopListening(DesktopWindow* window)
{
	switch(window->GetType())
	{
	case OpTypedObject::WINDOW_TYPE_DOCUMENT:
		DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(window);
		document->RemoveDocumentWindowListener(this);
		break;
	}
}


void DesktopWindowCollection::CloseAll()
{
	for (UINT32 i = 0; i < GetCount(); i++)
		GetItemByIndex(i)->GetDesktopWindow()->Close(FALSE, TRUE, FALSE);
}


DesktopWindow* DesktopWindowCollection::GetCurrentInputWindow()
{
	OpInputContext* context = g_input_manager->GetKeyboardInputContext();

	// Find the closest input context that is a desktop window (has a window or dialog type)
	while (context &&
		   !(context->GetType() >= OpTypedObject::WINDOW_TYPE_UNKNOWN && context->GetType() < OpTypedObject::WINDOW_TYPE_LAST) &&
		   !(context->GetType() >= OpTypedObject::DIALOG_TYPE && context->GetType() < OpTypedObject::DIALOG_TYPE_LAST))
	{
		context = context->GetParentInputContext();
	}

	if (!context)
		return NULL;

	return static_cast<DesktopWindow*>(context);
}


DesktopWindow* DesktopWindowCollection::GetDesktopWindowByID(INT32 id)
{
	DesktopWindowCollectionItem* item = GetItemByID(id);

	return item ? item->GetDesktopWindow() : NULL;
}


DesktopWindow* DesktopWindowCollection::GetDesktopWindowByUrl(const uni_char* search_url)
{
	if(!search_url)
		return NULL;

	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* window = GetItemByIndex(i)->GetDesktopWindow();
		if (!window || window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
			continue;
		URL url = WindowCommanderProxy::GetMovedToURL(window->GetWindowCommander());
		OpString url_str;
		url.GetAttribute(URL::KUniName, url_str);

		if (url_str.CompareI(search_url) == 0)
			return window;
	}

	return NULL;
}


DesktopWindow* DesktopWindowCollection::GetDesktopWindowByType(OpTypedObject::Type type)
{
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		if (GetItemByIndex(i)->GetType() == type)
			return GetItemByIndex(i)->GetDesktopWindow();
	}

	return NULL;
}


void DesktopWindowCollection::CloseDesktopWindowsByType(OpTypedObject::Type type)
{
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* window = GetItemByIndex(i)->GetDesktopWindow();
		if (window && window->GetType() == type)
			window->Close(TRUE);
	}
}


OpWorkspace* DesktopWindowCollection::GetWorkspace()
{
	// We will only attempt to put in a tab if there is only 1 desktop window open, otherwise
	// we risk adding the tab to a browser window that could be hiddden from view.
	// If there is more than one then we will just use the default and open a new window

	DesktopWindow* browserwindow = NULL;
	for (UINT32 i = 0; i < GetCount(); i += GetSubtreeSize(i) + 1)
	{
		if (GetItemByIndex(i)->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
		{
			if (browserwindow)
				return NULL;
			browserwindow = GetItemByIndex(i)->GetDesktopWindow();
		}
	}

	return browserwindow->GetWorkspace();
}


OP_STATUS DesktopWindowCollection::GetDesktopWindows(OpTypedObject::Type type, OpVector<DesktopWindow>& windows)
{
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* window = GetItemByIndex(i)->GetDesktopWindow();
		if (window && (type == OpTypedObject::WINDOW_TYPE_UNKNOWN || window->GetType() == type))
			RETURN_IF_ERROR(windows.Add(window));
	}

	return OpStatus::OK;
}


UINT32 DesktopWindowCollection::GetCount(OpTypedObject::Type type)
{
	UINT32 count = 0;
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		if (GetItemByIndex(i)->GetType() == type &&
				(!GetItemByIndex(i)->GetDesktopWindow() || !GetItemByIndex(i)->GetDesktopWindow()->IsClosing()))
			count++;
	}

	return count;
}


void DesktopWindowCollection::GentleClose()
{
	// close all windows... don't close those with parents since they will be closed by parent
	UINT32 curr_win = 0;

	while (GetCount())
	{
        DesktopWindow* desktop_window = GetItemByIndex(curr_win)->GetDesktopWindow();
		OP_ASSERT(desktop_window && "Top-level window should always be a desktop window");

		if (!desktop_window->GetParentDesktopWindow())
		{
			desktop_window->Close(TRUE);
			curr_win = 0;
		}
		else
		{
			// This was added because on Mac once the tabs are shifted between windows
			// it seems as if the list is changed and the main window is not grabbed first
			// therefore we get stuck in an infinite loop. Now we will step though the list
			// and attempt to get the main window (i.e. !desktop_window->GetParentDesktopWindow())
			curr_win++;

			// This should NEVER happen but I'll add it to avoid infinite loops
			if (curr_win > GetCount())
				break;
		}
	}
}


void DesktopWindowCollection::MinimizeAll()
{
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* window = GetItemByIndex(i)->GetDesktopWindow();

		if (!window || window->GetParentWorkspace())
			continue;

		window->GetOpWindow()->Minimize();
	}
}


void DesktopWindowCollection::SetWindowsVisible(bool visible)
{
	//show/hide all opera windows
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* window = GetItemByIndex(i)->GetDesktopWindow();

		if (!window || window->GetParentWorkspace() || !window->RespectsGlobalVisibilityChange(visible))
			continue;

		window->GetOpWindow()->Show(visible);
	}
}


DesktopWindow*	DesktopWindowCollection::GetDesktopWindowFromOpWindow(OpWindow* window)
{
	for (UINT32 i = 0; i < GetCount(); i++)
	{
		DesktopWindow* desktop_win = GetItemByIndex(i)->GetDesktopWindow();
		if (!desktop_win)
			continue;

		if (desktop_win->GetOpWindow() == window)
			return desktop_win;

		OpBrowserView* browser = desktop_win->GetBrowserView();

		if (browser && browser->GetOpWindow() == window)
			return desktop_win;
	}

	return NULL;
}


DocumentDesktopWindow* DesktopWindowCollection::GetDocumentWindowFromCommander(OpWindowCommander* commander)
{
	if (commander)
	{
		DesktopWindow* win = GetDesktopWindowFromOpWindow(commander->GetOpWindow());
		if (win && win->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
			return (DocumentDesktopWindow*)win;
	}
	return NULL;
}


OP_STATUS DesktopWindowCollection::GetColumnData(ColumnData* column_data)
{
	if (column_data->column == 0)
		return g_languageManager->GetString(Str::S_WINDOW_HEADER, column_data->text);

	column_data->text.Empty();
	return OpStatus::OK;
}
