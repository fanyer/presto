/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Code in this file has been copied from adjunct/quick/managers/DesktopExtensionsManager.cpp
 * For history prior to 20th Dec 2010, see there...
 *
 */
#include "core/pch.h"

#ifdef EXTENSION_SUPPORT

#include "adjunct/quick/extensions/TabAPIListener.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/debug/debug.h"

/* static */
BOOL TabAPIListener::CheckFilter(DesktopWindowCollectionItem* item, Filter filter)
{
	switch (filter)
	{
	case ALL:
		return TRUE;
	case ONLY_TOP_LEVEL:
		return !item->GetParentItem();
	case ONLY_TAB_GROUPS:
		return item->GetType() == OpTypedObject::WINDOW_TYPE_GROUP;
	case ONLY_TABS:
		return !item->IsContainer();
	default:
		OP_ASSERT(FALSE);
		return FALSE;
	}
}

/* static */
DesktopWindowCollectionItem* TabAPIListener::FindNextItem(DesktopWindowCollectionItem* cur, DesktopWindowCollectionItem* root, Filter filter)
{
	DesktopWindowCollection& windows = g_application->GetDesktopWindowCollection();
	if (!cur)
	{
		if (root)
			cur = root->GetChildItem();
		else
			cur = windows.GetFirstToplevel();

		if (CheckFilter(cur, filter))
			return cur;
	}
	if (!cur)
		return NULL;

	int end_index;
	if (!root || root->GetSiblingIndex() < 0)
		end_index = g_application->GetDesktopWindowCollection().GetCount();
	else
		end_index = root->GetSiblingIndex();

	OP_ASSERT(!root || (cur->GetIndex() > root->GetIndex() && cur->GetIndex() < end_index)); // check if cur is a child of root

	for (int i = cur->GetIndex() + 1; i < end_index; ++i)
	{
		DesktopWindowCollectionItem* next = windows.GetItemByIndex(i);
		if (CheckFilter(next, filter))
			return next;
	}
	return NULL;
}

/* static */
bool TabAPIListener::FindInsertPosition(DesktopWindowCollectionItem*& parent, int insert_before_id, DesktopWindowCollectionItem*& insert_after, bool opening_new_tab)
{
	OP_ASSERT(parent);

	if (!insert_before_id)
	{
		if (opening_new_tab && g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent) != FALSE)
		{
			// Let's check the parent of the active tab. If parent and actives_parent
			// are the same object, there is no philosophy here. If, however, they are
			// not the same, and:
			//
			// 1. actives_parent is a group, we should go inside this group and place
			//    new tab next to the active
			// 2. parent is a group, then extension author wrote something like
			//    "g.tabs.create()" and it's obvious he wanted
			//    the tab to appear in the group "g" and not outside of it...
			//
			// That is:
			//    if SAME or parent is not GROUP:
			//        if actives_parent is GROUP: <MOVE-TO-AP> -- this is needed for the "parent is not GROUP" part of the OR
			//                                                 -- in case of "SAME" part, it's... the same, so moving is a nop
			//        <use-active-as-after>
			DesktopWindowCollectionItem* toplevel = parent->GetToplevelWindow();
			DesktopWindow* top_window = toplevel ? toplevel->GetDesktopWindow() : NULL;

			if (top_window && top_window->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
			{
				BrowserDesktopWindow* browser_window = static_cast<BrowserDesktopWindow*>(top_window);
				DesktopWindow* active_tab_window = browser_window->GetActiveDesktopWindow(); // any window, including all the panels
				DesktopWindowCollectionItem* active = active_tab_window ? &active_tab_window->GetModelItem() : NULL;
				DesktopWindowCollectionItem* actives_parent = active ? active->GetParentItem() : NULL;
				if (actives_parent)
				{
					if (actives_parent->GetID() == parent->GetID() ||
						parent->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
					{
						if (actives_parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
						{
							parent = actives_parent;
						}
						insert_after = active;
						return true;
					}
				}
			}
			//otherwise, fall through to selecting the last item in the parent
		}

		insert_after = parent->GetLastChildItem();
		return true;
	}

	DesktopWindowCollectionItem* prev = NULL;
	DesktopWindowCollectionItem* current = parent->GetChildItem();
	while (current)
	{
		if (current->GetID() == insert_before_id)
		{
			insert_after = prev;
			return true;
		}

		prev = current;
		current = current->GetSiblingItem();
	};

	return false;
}

/* static */
int TabAPIListener::GetItemPosition(DesktopWindowCollectionItem* item)
{
	OP_ASSERT(item);
	int position = 0;

	while (item = item->GetPreviousItem()) position++;
	return position;
}

/////////////

TabAPIListener::~TabAPIListener()
{
	g_application->GetDesktopWindowCollection().RemoveListener(this);
	g_application->RemoveBrowserDesktopWindowListener(this);
	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS TabAPIListener::Init()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_TABAPI_BROWSER_CLOSE, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_TABAPI_GROUP_CLOSE, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_TABAPI_TAB_ACTION, m_dualton_id));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_TABAPI_GROUP_UPDATE, 0));

	RETURN_IF_ERROR(g_application->AddBrowserDesktopWindowListener(this));
	return g_application->GetDesktopWindowCollection().AddListener(this);
}

/*static*/
DesktopWindow* TabAPIListener::GetToplevelWindowByID(INT32 id)
{
	if (!g_application)
		return NULL;

	if (id == 0)
		return g_application->GetActiveBrowserDesktopWindow();

	DesktopWindow* desktopwindow = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(id);
	if (desktopwindow && desktopwindow->GetModelItem().GetParentItem() == NULL)
		return desktopwindow;
	return NULL; // not a top level window id
}

/*static*/
DesktopWindow* TabAPIListener::FindTabWindowByID(INT32 tab_id)
{
	DesktopWindowCollectionItem* cur_item = g_application->GetDesktopWindowCollection().GetItemByID(tab_id);
	if (cur_item)
		return cur_item->GetDesktopWindow();
	return NULL;
}

/*static*/
void TabAPIListener::MoveWindowToTheNearestScreen(DesktopWindow* browser)
{
	OpRect orig;
	browser->GetRect(orig);

	OpRect wnd = orig;
	browser->FitRectToScreen(wnd);

	if (!wnd.Equals(orig))
	{
		browser->SetOuterPos(wnd.x, wnd.y);
		browser->SetOuterSize(wnd.width, wnd.height);
	}
}

/*static*/
OP_STATUS TabAPIListener::GetTabInfo(OpTabAPIListener::ExtensionTabInfo& info, DesktopWindow* tab)
{
	RETURN_VALUE_IF_NULL(tab, OpStatus::ERR);
	if (tab->IsClosing())
		return OpStatus::ERR; // to properly report tab.closed with non-immediate tab.close();

	info.position = GetItemPosition(&tab->GetModelItem());
	DesktopWindow* browser = tab->GetToplevelDesktopWindow();
	OpWindowCommander* wc = tab->GetWindowCommander();

	info.id = tab->GetID();
	info.window_id = wc ? wc->GetWindowId() : 0;
	info.title = tab->GetTitle();
	info.is_locked =   !!tab->IsLockedByUser();
	info.is_selected = !!tab->IsActive();
	info.is_private =  !!tab->PrivacyMode();

	// If we were asked to create this window by Win/Tabs API,
	// we were asked for good old non-private mode. And then
	// we stored that value. And then core code looked into the
	// properties and MAYBE, just MAYBE set the PrivacyMode to
	// TRUE. Now, let's try to synchronize...
	if (wc)
	{
		bool commanderPrivacyMode = !!wc->GetPrivacyMode();
		if (info.is_private != commanderPrivacyMode)
		{
			info.is_private = commanderPrivacyMode;
			tab->SetPrivacyMode(commanderPrivacyMode);
		}
	}

	if (browser)
	{
		info.browser_window_id = browser->GetID();
	}

	OP_ASSERT(tab->GetModelItem().GetParentItem());
	if (tab->GetModelItem().GetParentItem()->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
	{
		DesktopGroupModelItem* group = static_cast<DesktopGroupModelItem*>(tab->GetModelItem().GetParentItem());
		info.tab_group_id = group->GetID();
	}
	else
		info.tab_group_id = 0;

	return OpStatus::OK;
}

/* virtual */
void TabAPIListener::OnQueryTab(
	TabAPIItemId target_tab_id,
	ExtensionWindowTabNotifications* callbacks)
{
	ExtensionTabInfo info = {};
	DesktopWindow* tab = FindTabWindowByID(target_tab_id);
	OP_STATUS ret = GetTabInfo(info, tab);
	if (OpStatus::IsError(ret))
		callbacks->NotifyQueryTab(ret, NULL);
	else
		callbacks->NotifyQueryTab(OpStatus::OK, &info);
}

/* virtual */
void TabAPIListener::OnQueryTab(
	OpWindowCommander* target_window,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	ExtensionTabInfo info = {};
	DesktopWindow* tab = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(target_window);
	OP_STATUS ret = GetTabInfo(info, tab);
	if (OpStatus::IsError(ret))
		callbacks->NotifyQueryTab(ret, NULL);
	else
		callbacks->NotifyQueryTab(OpStatus::OK, &info);
}

/* virtual */
void TabAPIListener::OnQueryAllBrowserWindows(
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	TabAPIItemIdCollection info;

	if (!g_application)
	{
		callbacks->NotifyQueryAllBrowserWindows(OpStatus::ERR, NULL);
		return;
	}

	OP_STATUS err = OpStatus::OK;
	for (DesktopWindowCollectionItem* item = g_application->GetDesktopWindowCollection().GetFirstToplevel();
		item != NULL;
		item = item->GetSiblingItem())
	{
		DesktopWindow* dsk = item->GetDesktopWindow();
		OP_ASSERT(dsk);
		if (dsk->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
			continue;
		if (OpStatus::IsError(err = info.Add(dsk->GetID())))
			break;
	}

	callbacks->NotifyQueryAllBrowserWindows(err, &info);
}

/* virtual */
void TabAPIListener::OnQueryAllTabGroups(
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	TabAPIItemIdCollection info;

	if (!g_application)
	{
		callbacks->NotifyQueryAllTabGroups(OpStatus::ERR, NULL);
		return;
	}

	OP_STATUS err = OpStatus::OK;
	for (DesktopWindowCollectionItem* iterator = FindNextItem(NULL, NULL, ONLY_TAB_GROUPS);
		iterator != NULL && OpStatus::IsSuccess(err);
		iterator = FindNextItem(iterator, NULL, ONLY_TAB_GROUPS))
	{
		err = info.Add(iterator->GetID());
	}

	callbacks->NotifyQueryAllTabGroups(err, &info);
}

/* virtual */
void TabAPIListener::OnQueryTabGroup(
	TabAPIItemId tab_group_id,
	BOOL include_contents,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	ExtensionTabGroupInfo info = {};
	info.id = tab_group_id;

	DesktopWindowCollectionItem* item = g_application->GetDesktopWindowCollection().GetItemByID(tab_group_id);
	if (!item || item->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		callbacks->NotifyQueryTabGroup(OpStatus::ERR, NULL, NULL, NULL);
		return;
	}

	DesktopGroupModelItem* group = static_cast<DesktopGroupModelItem*>(item);
	DesktopWindow* active_wnd = group->GetActiveDesktopWindow();
	DesktopWindowCollectionItem* parent_window = group->GetToplevelWindow();

	info.browser_window_id = parent_window->GetID();
	info.selected_tab_id = active_wnd ? active_wnd->GetID() : 0;
	info.selected_tab_window_id = active_wnd && active_wnd->GetWindowCommander() ? active_wnd->GetWindowCommander()->GetWindowId() : 0;
	info.position = GetItemPosition(group);
	info.is_selected = FALSE;
	info.is_collapsed = group->IsCollapsed();

	TabAPIItemIdCollection tabs;
	TabAPIItemIdCollection tab_windows;
	OP_STATUS err = OpStatus::OK;
	if (include_contents)
	{
		for (DesktopWindowCollectionItem* iterator = FindNextItem(NULL, group, ONLY_TABS);
			 iterator != NULL && OpStatus::IsSuccess(err);
			 iterator = FindNextItem(iterator, group, ONLY_TABS))
		{
			OP_ASSERT(iterator->GetDesktopWindow());

			DesktopWindow* desktopwindow = iterator->GetDesktopWindow();
			if (desktopwindow->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT || desktopwindow->IsClosing())
				continue;

			err = tabs.Add(iterator->GetDesktopWindow()->GetID());
			if (OpStatus::IsSuccess(err))
				err = tab_windows.Add(iterator->GetDesktopWindow()->GetWindowCommander() ?
				                      iterator->GetDesktopWindow()->GetWindowCommander()->GetWindowId() :
				                      0);
		}
	}

	callbacks->NotifyQueryTabGroup(err, &info, &tabs, &tab_windows);
}

/* virtual */
void TabAPIListener::OnQueryAllTabs(
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	TabAPIItemIdCollection tabs;
	TabAPIItemIdCollection tab_windows;

	OP_STATUS err = OpStatus::OK;
	DesktopWindowCollectionItem* iterator = NULL;
	while (iterator = FindNextItem(iterator, NULL, ONLY_TABS))
	{
		DesktopWindow* desktopwindow = iterator->GetDesktopWindow();
		OP_ASSERT(desktopwindow);

		if (desktopwindow->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT || desktopwindow->IsClosing())
			continue;

		err = tabs.Add(desktopwindow->GetID());

		if (OpStatus::IsSuccess(err))
		{
			OpWindowCommander* wc = desktopwindow->GetWindowCommander();
			err = tab_windows.Add(wc ? wc->GetWindowId() : 0);
		}

		if (OpStatus::IsError(err))
			break;
	}

	callbacks->NotifyQueryAllTabs(err, &tabs, &tab_windows);
}


/* virtual */
void TabAPIListener::OnQueryBrowserWindow(
	TabAPIItemId window_id,
	BOOL include_contents,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	DesktopWindow* desktopwindow = GetToplevelWindowByID(window_id);

	if (!desktopwindow || desktopwindow->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER || desktopwindow->IsClosing())
	{
		callbacks->NotifyQueryBrowserWindow(OpStatus::ERR, NULL, NULL, NULL, NULL);
		return;
	}

	ExtensionBrowserWindowInfo info;
	OpRect pos;
	desktopwindow->GetRect(pos);
	DesktopWindow* top_level_parent = desktopwindow->GetToplevelDesktopWindow();
	OP_ASSERT(top_level_parent);

	info.id         = desktopwindow->GetID();
	info.parent_id  = top_level_parent->GetID();

	// Window deactivation is asynchronous on platform level - because of that we
	// sometimes get false positives during IsActive() call. On the other hand,
	// TabAPI expects false negatives in such situations, that's why we compare
	// with g_application->GetActiveBrowserDesktopWindow().
	// Fixes window.is_focused related tests in DSK-357081.
	info.has_focus  = top_level_parent->IsActive() && top_level_parent == g_application->GetActiveBrowserDesktopWindow();

	info.top        = pos.y;
	info.left       = pos.x;
	info.width      = pos.width;
	info.height     = pos.height;
	//info.type     = desktopwindow->GetType();
	info.is_private = !!desktopwindow->PrivacyMode();
	if (DesktopWindow* dsk_wnd = desktopwindow->GetWorkspace() ? desktopwindow->GetWorkspace()->GetActiveDesktopWindow() : NULL)
	{
		info.selected_tab_id        = dsk_wnd->GetID();
		info.selected_tab_window_id = dsk_wnd->GetWindowCommander() ? dsk_wnd->GetWindowCommander()->GetWindowId(): 0;
	}
	else
	{
		info.selected_tab_id        = 0;
		info.selected_tab_window_id = 0;
	}

	TabAPIItemIdCollection groups;
	TabAPIItemIdCollection tabs;
	TabWindowIdCollection tab_windows;
	OP_STATUS err = OpStatus::OK;
	if (include_contents && desktopwindow->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		for (DesktopWindowCollectionItem* iterator = desktopwindow->GetModelItem().GetChildItem();
			iterator != NULL;
			iterator = FindNextItem(iterator, &desktopwindow->GetModelItem(), ALL))
		{
			if (iterator->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			{
				err = groups.Add(iterator->GetID());
				if (OpStatus::IsError(err))
					break;
			}
			if (!iterator->IsContainer())
			{
				DesktopWindow* dsk_wnd = iterator->GetDesktopWindow();
				if (!dsk_wnd) continue;

				if (dsk_wnd->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT || dsk_wnd->IsClosing())
					continue;

				if (OpStatus::IsSuccess(err = tabs.Add(dsk_wnd->GetID())))
				{
					err = tab_windows.Add(dsk_wnd->GetWindowCommander()? dsk_wnd->GetWindowCommander()->GetWindowId() : 0);
					if (OpStatus::IsError(err))
						break;
				}
			}
		}
	}
	callbacks->NotifyQueryBrowserWindow(err, &info, &groups, &tabs, &tab_windows);
	return;
}

/* virtual */
void TabAPIListener::OnRequestBrowserWindowMoveResize(
	TabAPIItemId window_id,
	const ExtensionWindowMoveResize &window_info,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	DesktopWindow* desktopwindow = GetToplevelWindowByID(window_id);
	if (!desktopwindow || desktopwindow->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER || desktopwindow->IsClosing())
	{
		callbacks->NotifyBrowserWindowMoveResize(OpStatus::ERR);
		return;
	}

	if (window_info.window_left >= 0 || window_info.window_top >= 0 || window_info.window_height >= 0 || window_info.window_width >= 0)
		desktopwindow->Restore();

	if (window_info.window_height >= 0 || window_info.window_width >= 0)
	{
		UINT32 width, height;
		desktopwindow->GetInnerSize(width, height);
		if (window_info.window_width  >= 0) width  = static_cast<UINT32>(window_info.window_width);
		if (window_info.window_height >= 0) height = static_cast<UINT32>(window_info.window_height);
#ifdef _UNIX_DESKTOP_
		// SetOuterSize is used because of DSK-325679.
		desktopwindow->SetOuterSize(width, height);
#else
		desktopwindow->SetInnerSize(width, height);
#endif
		MoveWindowToTheNearestScreen(desktopwindow);
	}

	if (window_info.window_left >= 0 || window_info.window_top >= 0)
	{
		INT32 left, top;
		desktopwindow->GetOuterPos(left, top);
		if (window_info.window_left >= 0) left = window_info.window_left;
		if (window_info.window_top  >= 0) top  = window_info.window_top;
		desktopwindow->SetOuterPos(left, top);
	}
	if (window_info.maximize)
		desktopwindow->Maximize();

	OP_ASSERT(callbacks);
	callbacks->NotifyBrowserWindowMoveResize(OpStatus::OK);
}

/* virtual */
void TabAPIListener::OnRequestBrowserWindowClose(
	TabAPIItemId window_id,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	DesktopWindow* desktopwindow = GetToplevelWindowByID(window_id);
	if (!desktopwindow || desktopwindow->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER || desktopwindow->IsClosing())
	{
		callbacks->NotifyBrowserWindowClose(OpStatus::ERR);
		return;
	}
	desktopwindow->Close();
	callbacks->NotifyBrowserWindowClose(OpStatus::OK);
}

/* virtual */
void TabAPIListener::OnRequestTabClose(
		TabAPIItemId tab_id,
		ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	DesktopWindow* desktopwindow = FindTabWindowByID(tab_id);
	if (!desktopwindow || desktopwindow->IsClosing())
	{
		callbacks->NotifyTabClose(OpStatus::ERR);
		return;
	}

	if (desktopwindow->IsClosableByUser())
		desktopwindow->Close();

	callbacks->NotifyTabClose(OpStatus::OK);
}

/* static */
DesktopWindowCollectionItem* TabAPIListener::GetOrCreateInsertTarget(
		const WindowTabInsertTarget& data, DesktopWindowCollectionItem* item,
		TabAPIItemId& dst_group_id, TabAPIItemId& dst_window_id, BOOL open_background)
{
	dst_group_id = data.dst_group_id;
	dst_window_id = data.dst_window_id;

	switch (data.type)
	{

	case WindowTabInsertTarget::NEW_BROWSER_WINDOW:
		{
			BrowserDesktopWindow* browser = NULL;
			BrowserDesktopWindow* old_active = g_application->GetActiveBrowserDesktopWindow();

			RETURN_VALUE_IF_ERROR(BrowserDesktopWindow::Construct(
						&browser, FALSE, data.window_create.is_private), NULL);

			OpRect rect;

			rect.x = MapDimension(data.window_create.left);
			rect.y = MapDimension(data.window_create.top);
			rect.width = MapDimension(data.window_create.width);
			rect.height = MapDimension(data.window_create.height);

			browser->Show(
					TRUE,               // show
					&rect,              // position and size
					OpWindow::RESTORED, // preferred state
					open_background,    // behind (unreliable)
					TRUE,               // force passed position and size
					FALSE               // inner coordinates
					);

			if (open_background && old_active)
			{
				// Receiving focus in newly created window may be forced by platform.
				// On Mac, it looks like forcing activate is enough to get expected behaviour.
				// On Unix, newly created browser window will receive FocusIn events right
				// after mapping to screen, so we need to override those.
				// Fixes creation of unfocused window related tests in DSK-357081.
#if defined(_MACINTOSH_)
				old_active->Activate();
#elif defined(_UNIX_DESKTOP_)
				OpInputAction* delayed_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_ACTIVATE_WINDOW));
				if (!delayed_action)
					return NULL;
				delayed_action->SetActionData(old_active->GetID());
				g_main_message_handler->PostMessage(MSG_QUICK_DELAYED_ACTION, reinterpret_cast<MH_PARAM_1>(delayed_action), 0);
#endif
			}

			MoveWindowToTheNearestScreen(browser);

			// We can't pass maximized state to Show(), because mix of policies
			// and platform ifdefs will prevent it from working as expected.
			if (data.window_create.maximized)
				browser->Maximize();

			dst_group_id = 0;
			dst_window_id = browser->GetID();

			OP_ASSERT(open_background != browser->IsActive());
			OP_ASSERT(open_background == old_active->IsActive());
			OP_ASSERT(open_background != (g_application->GetActiveBrowserDesktopWindow() == browser));
			OP_ASSERT(open_background == (g_application->GetActiveBrowserDesktopWindow() == old_active));

			return &browser->GetModelItem();
		}

	case WindowTabInsertTarget::EXISTING_BROWSER_WINDOW:
		{
			DesktopWindowCollectionItem* browser = GetContainerItem(data.dst_window_id, data.before_item_id);
			if (browser && browser->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
				return browser;
			if (browser && data.dst_window_id == 0 && browser->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
				return browser;
			return NULL;
		}
		break;
	case WindowTabInsertTarget::NEW_TAB_GROUP:
		{
			OP_ASSERT(data.dst_window_id != TabAPIItemId(-1));

			DesktopWindowCollectionItem* top_level = GetContainerItem(data.dst_window_id, data.before_item_id);
			if (!top_level || top_level->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
				return NULL;

			DesktopWindow* top_level_wnd = top_level->GetDesktopWindow();
			DesktopWindowCollectionItem* insert_after = NULL;
			if (!FindInsertPosition(top_level, data.before_item_id, insert_after, false))
				return NULL;

			DesktopGroupModelItem* tab_group = NULL;
			if (item)
			{
				top_level->GetModel()->ReorderByItem(*item, top_level, insert_after);

				tab_group = top_level->GetModel()->CreateGroup(
					*item,
					data.tab_group_create.collapsed != FALSE);
			}
			else
			{
				tab_group = top_level->GetModel()->CreateGroupAfter(
					*top_level, insert_after,
					data.tab_group_create.collapsed != FALSE);
			}

			if (!tab_group)
				return NULL;

			dst_group_id = tab_group->GetID();
			dst_window_id = top_level_wnd->GetID();

			return tab_group;
		}
	case WindowTabInsertTarget::EXISTING_TAB_GROUP:
		{
			if (data.dst_group_id)
			{
				DesktopWindowCollectionItem* group = g_application->GetDesktopWindowCollection().GetItemByID(data.dst_group_id);

				if (group && group->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
					return group;

				return NULL;
			}
			else
			{
				// If you are getting this assert, you most probably didn't return the dst_group_id
				// back to DOM (DOM win/tab API updates this id every time they could get it from us).
				//
				// If, however, you are sure this is not due to not reporting the group id back,
				// then your task is to implement the idea of the "default" group

				OP_ASSERT(!"Group id lost");
				return NULL;
			}
		}
	case WindowTabInsertTarget::CURRENT_CONTAINER:
		{
			if (item)
			{
				return item->GetParentItem();
			}
			else
			{
				return GetContainerItem(0, data.before_item_id);
			}
		}
	}
	OP_ASSERT(FALSE);
	return NULL;
}

DesktopWindowCollectionItem* TabAPIListener::GetContainerItem(TabAPIItemId window_id, TabAPIItemId helper_id)
{
	if (!window_id)
	{
		if (helper_id)
		{
			DesktopWindowCollectionItem* tab = g_application->GetDesktopWindowCollection().GetItemByID(helper_id);
			if (tab && tab->GetParentItem())
				return tab->GetParentItem();
		}

		DesktopWindow* win = g_application->GetActiveBrowserDesktopWindow();
		return win ? &win->GetModelItem() : NULL;
	}

	DesktopWindow* win = GetToplevelWindowByID(window_id);
	return win ? &win->GetModelItem() : NULL;
}

OpTabAPIListener::TabAPIItemId TabAPIListener::CreateNewTab(OpWindowCommander* wc, const OpUiWindowListener::CreateUiWindowArgs &args)
{
	OP_ASSERT(args.type == OpUiWindowListener::WINDOWTYPE_REGULAR);

	WindowTabInsertTarget default_insert_target;
	const WindowTabInsertTarget* insert_target = args.insert_target;

	if (!insert_target)
	{
		insert_target = &default_insert_target;

		default_insert_target.type = WindowTabInsertTarget::EXISTING_BROWSER_WINDOW;
		default_insert_target.before_item_id = 0;
		default_insert_target.dst_group_id = 0;
		default_insert_target.dst_window_id = 0;
	}

	TabAPIItemId dummy = 0;
	DesktopWindowCollectionItem* parent = GetOrCreateInsertTarget(*insert_target, NULL, dummy, dummy, args.regular.open_background);

	int insert_before_id = insert_target->before_item_id;
	if (insert_target->type == WindowTabInsertTarget::NEW_BROWSER_WINDOW ||
		insert_target->type == WindowTabInsertTarget::NEW_TAB_GROUP)
	{
		insert_before_id = 0;
	}

	DocumentDesktopWindow* page = OpenNewTab(wc, parent, insert_before_id, args);

	return page ? page->GetID() : 0;
}

DocumentDesktopWindow* TabAPIListener::OpenNewTab(OpWindowCommander* wc, DesktopWindowCollectionItem* parent, int insert_before_id, const OpUiWindowListener::CreateUiWindowArgs &args)
{
	DesktopWindow* target = parent ? parent->GetToplevelWindow()->GetDesktopWindow() : NULL;
	if (!target)
		return NULL;

	DesktopWindowCollectionItem* insert_after = NULL;
	DesktopWindowCollectionItem* old_parent = parent; //if insert_before_id is 0, parent might change from browser window to one of it's groups.
	if (!FindInsertPosition(parent, insert_before_id, insert_after, true))
		return NULL;

	if (parent != old_parent)
	{
		target = parent ? parent->GetToplevelWindow()->GetDesktopWindow() : NULL;
		if (!target)
			return NULL;
	}

	OpWorkspace* workspace = target->GetWorkspace();

	OpenURLSetting settings;
	settings.m_src_commander = args.opener;

	if(target->PrivacyMode())
		settings.m_is_privacy_mode = TRUE;

	if(args.regular.open_background)
		settings.m_in_background = YES;

	DocumentDesktopWindow* page = g_application->CreateDocumentDesktopWindow(
		workspace, wc,
		0, 0,
		TRUE,
		args.regular.open_background ? TRUE : args.regular.focus_document,
		args.regular.open_background,
		TRUE, &settings, target->PrivacyMode());

	if (!page)
		return NULL;

	m_supress_this_moving = &page->GetModelItem();

	parent->GetModel()->ReorderByItem(page->GetModelItem(), parent, insert_after);

	if (args.regular.focus_document)
	{
		page->Activate();
	}

	if (args.locked)
	{
		page->SetLockedByUser(TRUE);
		PagebarButton* btn = page->GetPagebarButton();
		if (btn && btn->GetPagebar())
			btn->GetPagebar()->OnLockedByUser(btn, TRUE);
	}
	m_supress_this_moving = NULL;

	return page;
}

/* static */
OP_STATUS TabAPIListener::MoveItem(DesktopWindowCollectionItem* src, const WindowTabInsertTarget& insert_target, TabAPIItemId& dst_group_id, TabAPIItemId& dst_window_id)
{
	OP_ASSERT(src);
	RETURN_VALUE_IF_NULL(g_application, OpStatus::ERR_NOT_SUPPORTED);

	DesktopWindowCollectionItem* parent = GetOrCreateInsertTarget(insert_target, src, dst_group_id, dst_window_id, TRUE);

	RETURN_VALUE_IF_NULL(parent, OpStatus::ERR);

	int before_item_id = insert_target.before_item_id;
	switch (insert_target.type)
	{
		case WindowTabInsertTarget::NEW_BROWSER_WINDOW:
		case WindowTabInsertTarget::NEW_TAB_GROUP:
			before_item_id = 0;
	}

	DesktopWindowCollectionItem* insert_after = NULL;
	if (!FindInsertPosition(parent, before_item_id, insert_after, false))
		return OpStatus::ERR;

	if (src->GetType() == OpTypedObject::WINDOW_TYPE_GROUP &&
		parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
	{
		return OpStatus::ERR;
	}

	parent->GetModel()->ReorderByItem(*src, parent, insert_after);

	if (parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP && insert_target.type == WindowTabInsertTarget::NEW_TAB_GROUP)
	{
		static_cast<DesktopGroupModelItem*>(parent)->SetActiveDesktopWindow(src->GetDesktopWindow());
	}
	return OpStatus::OK;
}

/* virtual */
void TabAPIListener::OnRequestTabMove(
	TabAPIItemId tab_id,
	const WindowTabInsertTarget& insert_target,
	ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	if (!g_application)
	{
		callbacks->NotifyTabMove(OpStatus::ERR_NOT_SUPPORTED, 0, 0);
		return;
	}

	DesktopWindow* src_win = FindTabWindowByID(tab_id);
	if (!src_win)
	{
		callbacks->NotifyTabMove(OpStatus::ERR, 0, 0);
		return;
	}

	TabAPIItemId dst_group_id = 0;
	TabAPIItemId dst_window_id = 0;
	OP_STATUS error = MoveItem(&src_win->GetModelItem(), insert_target, dst_group_id, dst_window_id);
	callbacks->NotifyTabMove(error, dst_window_id, dst_group_id);
}

/* virtual */ void
TabAPIListener::OnRequestTabGroupMove(
		TabAPIItemId tab_group_id,
		const WindowTabInsertTarget& insert_target,
		ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	if (!g_application)
	{
		callbacks->NotifyTabMove(OpStatus::ERR_NOT_SUPPORTED, 0, 0);
		return;
	}

	DesktopWindowCollectionItem* src = g_application->GetDesktopWindowCollection().GetItemByID(tab_group_id);
	if (!src || src->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		callbacks->NotifyTabMove(OpStatus::ERR, 0, 0);
		return;
	}

	TabAPIItemId dst_group_id = 0;
	TabAPIItemId dst_window_id = 0;
	OP_STATUS error = MoveItem(src, insert_target, dst_group_id, dst_window_id);
	callbacks->NotifyTabMove(error, dst_window_id, dst_group_id);
}

void TabAPIListener::OnRequestTabGroupClose(
		TabAPIItemId tab_group_id,
		ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	if (!g_application)
	{
		callbacks->NotifyTabGroupClose(OpStatus::ERR);
		return;
	}

	DesktopWindowCollectionItem* group_item = g_application->GetDesktopWindowCollection().GetItemByID(tab_group_id);
	if (!group_item || group_item->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		callbacks->NotifyTabGroupClose(OpStatus::ERR);
		return;
	}

	group_item->GetModel()->DestroyGroup(group_item);

	callbacks->NotifyTabGroupClose(OpStatus::OK);
}


/* virtual */
void TabAPIListener::OnRequestTabGroupUpdate(
		TabAPIItemId tab_group_id,
		int collapsed,
		ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	if (!g_application)
	{
		callbacks->NotifyTabGroupUpdate(OpStatus::ERR);
		return;
	}

	DesktopWindowCollectionItem* group = g_application->GetDesktopWindowCollection().GetItemByID(tab_group_id);
	if (!group || group->GetType() != OpTypedObject::WINDOW_TYPE_GROUP)
	{
		callbacks->NotifyTabGroupUpdate(OpStatus::ERR);
		return;
	}
	static_cast<DesktopGroupModelItem*>(group)->SetCollapsed(!!collapsed);

	callbacks->NotifyTabGroupUpdate(OpStatus::OK);
}

/* virtual */ void TabAPIListener::OnRequestTabUpdate(
		TabAPIItemId tab_id,
		BOOL set_focus,
		int set_pinned,
		const uni_char* set_url,
		const uni_char* set_title,
		ExtensionWindowTabNotifications* callbacks)
{
	OP_ASSERT(callbacks);

	if (!g_application)
	{
		callbacks->NotifyTabUpdate(OpStatus::ERR);
		return;
	}
	DesktopWindow* tab = FindTabWindowByID(tab_id);
	if (!tab || tab->IsClosing())
	{
		callbacks->NotifyTabUpdate(OpStatus::ERR);
		return;
	}

	if (set_focus)
	{
		// If we have false positive active state on toplevel window then let's clear
		// active flags - in result activation of child window will propagate to toplevel
		// window properly. Fixes window.focus() related tests in DSK-357081.
		DesktopWindow* toplevel = tab->GetToplevelDesktopWindow();
		BrowserDesktopWindow* active = g_application->GetActiveBrowserDesktopWindow();
		if (toplevel && toplevel->IsActive() && toplevel != active)
		{
			if (active)
				active->Activate(FALSE);
			toplevel->Activate(FALSE);
		}

		tab->Activate();

		if (toplevel)
			toplevel->GetOpWindow()->SetFocus(TRUE);
	}

	if (set_pinned >= 0)
	{
		tab->SetLockedByUser(set_pinned > 0);
		tab->UpdateUserLockOnPagebar();
	}

	OpWindowCommander* wc = NULL;
	if (set_url || set_title)
	{
		wc = tab->GetWindowCommander();
		if (!wc)
		{
			callbacks->NotifyTabUpdate(OpStatus::ERR);
			return;
		}
	}

	if (set_url)
		wc->OpenURL(set_url, FALSE);

	if (set_title)
	{
		OpString title_str;
		OP_STATUS ret = title_str.Set(set_title);

		if (OpStatus::IsError(ret))
		{
			callbacks->NotifyTabUpdate(ret);
			return;
		}

		wc->SetWindowTitle(title_str, TRUE);
	}

	callbacks->NotifyTabUpdate(OpStatus::OK);
}

//
// UIListener implementation
//

void TabAPIListener::WinTabReport(const char* type, ExtensionWindowTabActionListener::ActionType action, TabAPIItemId window_id, TabAPIItemId group_id, TabAPIItemId tab_id)
{
	// This function produces a lot of output. A lot. You do not even need this most of the time in debug, unless you specifically debug order of events. Otherwise, no, not so much.
#if 0
	const char* s_action = "<unknown>";
	switch (action)
	{
	case ExtensionWindowTabActionListener::ACTION_CREATE:          s_action = "CREATE"; break;
	case ExtensionWindowTabActionListener::ACTION_CLOSE:           s_action = "CLOSE"; break;
	case ExtensionWindowTabActionListener::ACTION_FOCUS:           s_action = "FOCUS"; break;
	case ExtensionWindowTabActionListener::ACTION_BLUR:            s_action = "BLUR"; break;
	case ExtensionWindowTabActionListener::ACTION_MOVE:            s_action = "MOVE"; break;
	case ExtensionWindowTabActionListener::ACTION_MOVE_FROM_GROUP: s_action = "MOVE_FROM_GROUP"; break;
	case ExtensionWindowTabActionListener::ACTION_UPDATE:          s_action = "UPDATE"; break;
	}

	// We could restore window_id based on tab and we could restore group_id from the tab as well
	DesktopWindowCollectionItem* tab = ((group_id == -1) || (window_id == -1)) && (tab_id != -1)
		? g_application->GetDesktopWindowCollection().GetItemByID(tab_id)
		: NULL;

	// If we don't have a tab, but we have the group, we can still use it to restore the window_id
	DesktopWindowCollectionItem* group = (!tab && (group_id != -1) && (window_id == -1))
		? g_application->GetDesktopWindowCollection().GetItemByID(group_id)
		: NULL;

	// If the group is missing, maybe the tab is in one; if yes, store group's ID
	if (group_id == -1 && tab)
	{
		DesktopWindowCollectionItem* parent = tab->GetParentItem();
		if (parent && parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			group_id = parent->GetID();
	}

	// If the window is missing we can use the tab (falling back to group) to restore it
	if (window_id == -1 && (tab || group))
	{
		DesktopWindowCollectionItem* window = (tab ? tab : group)->GetToplevelWindow();
		if (window)
			window_id = window->GetID();
	}

	int index = (window_id != -1 ? 4 : 0) | (group_id != -1 ? 2 : 0) | (tab_id != -1 ? 1 : 0);

	OpString8 output;
	switch(index)
	{
	case 0: output.AppendFormat("%s-%s", type, s_action); break;
	case 1: output.AppendFormat("%s-%s (tab: %d)", type, s_action, tab_id); break;
	case 2: output.AppendFormat("%s-%s (group: %d)", type, s_action, group_id); break;
	case 3: output.AppendFormat("%s-%s (group: %d, tab: %d)", type, s_action, group_id, tab_id); break;
	case 4: output.AppendFormat("%s-%s (window: %d)", type, s_action, window_id); break;
	case 5: output.AppendFormat("%s-%s (window: %d, tab: %d)", type, s_action, window_id, tab_id); break;
	case 6: output.AppendFormat("%s-%s (window: %d, group: %d)", type, s_action, window_id, group_id); break;
	case 7: output.AppendFormat("%s-%s (window: %d, group: %d, tab: %d)", type, s_action, window_id, group_id, tab_id); break;
	}

	OP_NEW_DBG(":Action", "WinTab");
	OP_DBG(("") << output.CStr());
#endif
}

void TabAPIListener::GenerateWindowAction(
	ExtensionWindowTabActionListener::ActionType action,
	TabAPIItemId window_id,
	ExtensionWindowTabActionListener::ActionData* data)
{
	WinTabReport("WND", action, window_id, (TabAPIItemId)-1, (TabAPIItemId)-1);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnBrowserWindowAction(action, window_id, data);
	}
}

void TabAPIListener::GenerateTabAction(
	ExtensionWindowTabActionListener::ActionType action,
	TabAPIItemId tab_id, OpWindowCommander* tab_window_commander,
	ExtensionWindowTabActionListener::ActionData* data)
{
	WinTabReport("TAB", action, (TabAPIItemId)-1, (TabAPIItemId)-1, tab_id);

	TabAPIItemId window_id = tab_window_commander ? tab_window_commander->GetWindowId() : (TabAPIItemId)0;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnTabAction(action, tab_id, window_id, data);
	}
}

void TabAPIListener::GenerateTabActionDelayed(
	ExtensionWindowTabActionListener::ActionType action,
	TabAPIItemId tab_id, OpWindowCommander* tab_window_commander,
	const ExtensionWindowTabActionListener::ActionData& data)
{
	OpAutoPtr<TabActionPostData> ptr(OP_NEW(TabActionPostData, (action, tab_id, tab_window_commander, data)));
	if (g_main_message_handler->PostMessage(MSG_TABAPI_TAB_ACTION, m_dualton_id, reinterpret_cast<MH_PARAM_2>(ptr.get())))
	{
		ptr.release();
	}
}

void TabAPIListener::GenerateTabGroupAction(
	ExtensionWindowTabActionListener::ActionType action,
	TabAPIItemId tab_group_id,
	ExtensionWindowTabActionListener::ActionData* data)
{
	WinTabReport("GRP", action, (TabAPIItemId)-1, tab_group_id, (TabAPIItemId)-1);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnTabGroupAction(action, tab_group_id, data);
	}
}

void TabAPIListener::OnBrowserDesktopWindowDeleting(BrowserDesktopWindow *window)
{
	g_main_message_handler->PostMessage(MSG_TABAPI_BROWSER_CLOSE, window->GetID(), 0);
}

void TabAPIListener::OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate)
{
	if (!desktop_window || desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	GenerateTabAction(
		activate ?
			ExtensionWindowTabActionListener::ACTION_FOCUS :
			ExtensionWindowTabActionListener::ACTION_BLUR,
		desktop_window->GetID(), desktop_window->GetWindowCommander());
}

/* virtual */
void TabAPIListener::OnDesktopWindowAdded(DesktopWindow* window)
{
	if (!window || window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	DesktopWindow* parent = window->GetParentDesktopWindow();
	if (parent && parent->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
		GenerateTabAction(ExtensionWindowTabActionListener::ACTION_CREATE, window->GetID(), window->GetWindowCommander());
}

/* virtual */
void TabAPIListener::OnDesktopWindowRemoved(DesktopWindow* window)
{
	if (!window || window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	GenerateTabAction(ExtensionWindowTabActionListener::ACTION_CLOSE, window->GetID(), window->GetWindowCommander());
}

/* virtual */
void TabAPIListener::OnCollectionItemMoved(DesktopWindowCollectionItem* item,
		                                   DesktopWindowCollectionItem* old_parent,
		                                   DesktopWindowCollectionItem* old_previous)
{
	if (m_supress_this_moving == item)
		return;
	if (item->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
	{
		OP_ASSERT(old_parent);
		ExtensionWindowTabActionListener::ActionData data;
		data.move_data.previous_parent_id = old_parent->GetID();
		data.move_data.position =  old_previous ? GetItemPosition(old_previous) + 1 : 0;
		GenerateTabGroupAction(ExtensionWindowTabActionListener::ACTION_MOVE,         // Event type
		                       item->GetID(),                                         // tab id
		                       &data);                                                // action data

		for (DesktopWindowCollectionItem* child = item->GetChildItem(); child; child = child->GetSiblingItem())
		{
			// this will potentially generate
			data.move_data.previous_parent_id = item->GetID();
			data.move_data.position = GetItemPosition(child);
			GenerateTabAction(ExtensionWindowTabActionListener::ACTION_MOVE,         // Event type
			                  child->GetID(),                                        // tab id
			                  child->GetDesktopWindow()->GetWindowCommander(),       // tab window commander
			                  &data);                                                // action data

		}
	}
	else
	{
		if (item->IsContainer())
			return; // Not a tab - ignore it

		OP_ASSERT(item->GetDesktopWindow());
		if (old_parent)
		{
			ExtensionWindowTabActionListener::ActionData data;
			data.move_data.previous_parent_id = old_parent->GetID();
			data.move_data.position =  old_previous ? GetItemPosition(old_previous) + 1 : 0;
			if(old_parent->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			{
				GenerateTabActionDelayed(ExtensionWindowTabActionListener::ACTION_MOVE_FROM_GROUP, // Event type
				                  item->GetID(),                                         // tab id
				                  item->GetDesktopWindow()->GetWindowCommander(),        // tab window commander
				                  data);                                                 // action data
			}
			else
			{
				GenerateTabActionDelayed(ExtensionWindowTabActionListener::ACTION_MOVE,  // Event type
				                  item->GetID(),                                         // tab id
				                  item->GetDesktopWindow()->GetWindowCommander(),        // tab window commander
				                  data);                                                 // action data
			}
		}
	}

}



/* virtual */
void TabAPIListener::OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	GenerateTabGroupAction(ExtensionWindowTabActionListener::ACTION_CREATE,        // Event type
		                    group.GetID());                                        // tab id
}

/* virtual */
void TabAPIListener::OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	g_main_message_handler->PostMessage(MSG_TABAPI_GROUP_CLOSE, group.GetID(), 0);
}

/* virtual */
void TabAPIListener::OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	g_main_message_handler->PostMessage(MSG_TABAPI_GROUP_UPDATE, group.GetID(), 0);
}

/* virtual */
OP_STATUS TabAPIListener::OnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *callbacks)
{
	return m_listeners.Add(callbacks);
}

/* virtual */
OP_STATUS TabAPIListener::OnUnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *callbacks)
{
	return m_listeners.Remove(callbacks);
}

/* virtual */
void TabAPIListener::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// The events comming from Quick are not in the order expected by the Win/Tab API DOM
	// Most notably, TAB-CLOSE messages can come after GRP-CLOSE or WND-CLOSE, or TAB-MOVE
	// and GRP-UPDATE come before GRP-CREATE. It was argued this would be problematic for
	// the extension authors and therefore these messages are introduced.
	//
	// The WND-CLOSE and GRP-CLOSE are delayed until all TAB-CLOSE are delivered by placing
	// the browser window and group into the message handler queue and wait for their turn
	// to be picked up from there. Same goes for waiting with TAB-MOVE and GRP-UPDATE, until
	// GRP-CREATE is bubbled into DOM.
	switch(msg)
	{
	case MSG_TABAPI_GROUP_CLOSE:
		GenerateTabGroupAction(ExtensionWindowTabActionListener::ACTION_CLOSE, par1);
		break;

	case MSG_TABAPI_BROWSER_CLOSE:
		GenerateWindowAction(ExtensionWindowTabActionListener::ACTION_CLOSE, par1);
		break;

	case MSG_TABAPI_TAB_ACTION:
		{
			TabActionPostData* ptr = reinterpret_cast<TabActionPostData*>(par2);
			if (ptr)
			{
				GenerateTabAction(ptr->action, ptr->tab_id, ptr->tab_window_commander, &ptr->data);
				OP_DELETE(ptr);
			}

			break;
		}

	case MSG_TABAPI_GROUP_UPDATE:
		GenerateTabGroupAction(ExtensionWindowTabActionListener::ACTION_UPDATE, par1);
		break;
	}
}

#endif // EXTENSION_SUPPORT
