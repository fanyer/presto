/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/dom/src/extensions/domtabapicachedobject.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dom/src/extensions/domextension_background.h"

/* static */ OP_STATUS
DOM_TabApiCache::Make(DOM_TabApiCache*& new_obj, DOM_ExtensionSupport* extension_support)
{
	OpAutoPtr<DOM_TabApiCache> obj(OP_NEW(DOM_TabApiCache, (extension_support)));
	RETURN_OOM_IF_NULL(obj.get());
	RETURN_IF_ERROR(obj->Construct());
	new_obj = obj.release();
	return OpStatus::OK;
}

DOM_TabApiCache::DOM_TabApiCache(DOM_ExtensionSupport* extension_support)
	: m_extension_support(extension_support)
{
}

OP_STATUS
DOM_TabApiCache::Construct()
{
	return g_windowCommanderManager->GetTabAPIListener()->OnRegisterWindowTabActionListener(this);
}

DOM_TabApiCache::~DOM_TabApiCache()
{
	g_windowCommanderManager->GetTabAPIListener()->OnUnRegisterWindowTabActionListener(this);
}

/* static */ OP_STATUS
DOM_TabApiCache::GetOrCreateTab(DOM_BrowserTab*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_id, unsigned long window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	OP_ASSERT(extension_support->GetBackground());
	OP_ASSERT(extension_support->GetBackground()->GetTabApiCache());

	DOM_TabApiCache* cache = extension_support->GetBackground()->GetTabApiCache();
	DOM_TabApiCachedObject* cached_obj;
	if (cache->m_cached_objects.GetData(tab_id, &cached_obj) == OpStatus::OK)
	{
		OP_ASSERT(cached_obj);
		OP_ASSERT(cached_obj->IsA(DOM_TYPE_BROWSER_TAB));
		OP_ASSERT(origining_runtime == cached_obj->GetRuntime());
		new_obj = static_cast<DOM_BrowserTab*>(cached_obj);
		return OpStatus::OK;
	}
	else
	{
		RETURN_IF_ERROR(DOM_BrowserTab::Make(new_obj, extension_support, tab_id, window_id, origining_runtime));
		return cache->m_cached_objects.Add(tab_id, new_obj);
	}
}

/* static */ OP_STATUS
DOM_TabApiCache::GetOrCreateTabGroup(DOM_BrowserTabGroup*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_group_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	OP_ASSERT(extension_support->GetBackground());
	OP_ASSERT(extension_support->GetBackground()->GetTabApiCache());

	DOM_TabApiCache* cache = extension_support->GetBackground()->GetTabApiCache();
	DOM_TabApiCachedObject* cached_obj;
	if (cache->m_cached_objects.GetData(tab_group_id, &cached_obj) == OpStatus::OK)
	{
		OP_ASSERT(cached_obj);
		OP_ASSERT(cached_obj->IsA(DOM_TYPE_BROWSER_TAB_GROUP));
		OP_ASSERT(origining_runtime == cached_obj->GetRuntime());
		new_obj = static_cast<DOM_BrowserTabGroup*>(cached_obj);
		return OpStatus::OK;
	}
	else
	{
		RETURN_IF_ERROR(DOM_BrowserTabGroup::Make(new_obj, extension_support, tab_group_id, origining_runtime));
		return cache->m_cached_objects.Add(tab_group_id, new_obj);
	}
}

/* static */ OP_STATUS
DOM_TabApiCache::GetOrCreateWindow(DOM_BrowserWindow*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId win_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	OP_ASSERT(extension_support->GetBackground());
	OP_ASSERT(extension_support->GetBackground()->GetTabApiCache());

	DOM_TabApiCache* cache = extension_support->GetBackground()->GetTabApiCache();
	DOM_TabApiCachedObject* cached_obj;
	if (cache->m_cached_objects.GetData(win_id, &cached_obj) == OpStatus::OK)
	{
		OP_ASSERT(cached_obj);
		OP_ASSERT(cached_obj->IsA(DOM_TYPE_BROWSER_WINDOW));
		OP_ASSERT(origining_runtime == cached_obj->GetRuntime());
		new_obj = static_cast<DOM_BrowserWindow*>(cached_obj);
		return OpStatus::OK;
	}
	else
	{
		RETURN_IF_ERROR(DOM_BrowserWindow::Make(new_obj, extension_support, win_id, origining_runtime));
		return cache->m_cached_objects.Add(win_id, new_obj);
	}
}

/* virtual */ void
DOM_TabApiCache::OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ActionData* data)
{
	if (action == ACTION_CLOSE)
	{
		DOM_TabApiCachedObject* unused;
		m_cached_objects.Remove(window_id, &unused);
	}
}

/* virtual */ void
DOM_TabApiCache::OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ActionData* data)
{
	if (action == ACTION_CLOSE)
	{
		DOM_TabApiCachedObject* unused;
		m_cached_objects.Remove(tab_id, &unused);
	}
}

/* virtual */ void
DOM_TabApiCache::OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ActionData* data)
{
	if (action == ACTION_CLOSE)
	{
		DOM_TabApiCachedObject* unused;
		OpStatus::Ignore(m_cached_objects.Remove(tab_group_id, &unused));
	}
}

void
DOM_TabApiCache::OnCachedObjectDestroyed(DOM_TabApiCachedObject* obj)
{
	DOM_TabApiCachedObject* unused;
	OpStatus::Ignore(m_cached_objects.Remove(obj->GetId(), &unused));
}

/* virtual */ void
DOM_TabApiCache::HandleKeyData(const void* key, void* data)
{
	DOM_TabApiCachedObject* cached_object = reinterpret_cast<DOM_TabApiCachedObject*>(data);
	if (cached_object->IsSignificant())
		DOM_Object::GCMark(cached_object);
}

void
DOM_TabApiCache::GCTrace()
{
	m_cached_objects.ForEach(this);
}

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
