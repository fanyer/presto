/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_TABAPICACHE_H
#define DOM_EXTENSIONS_TABAPICACHE_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class DOM_Runtime;
class DOM_TabApiCachedObject;
class DOM_BrowserTab;
class DOM_BrowserTabGroup;
class DOM_BrowserWindow;

class DOM_TabApiCache
	: OpTabAPIListener::ExtensionWindowTabActionListener
	, public OpHashTableForEachListener
{
	friend class DOM_TabApiCachedObject;
public:

	~DOM_TabApiCache();

	static OP_STATUS Make(DOM_TabApiCache*& new_obj, DOM_ExtensionSupport* extension_support);

	static OP_STATUS GetOrCreateTab(DOM_BrowserTab*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_id, unsigned long window_id, DOM_Runtime* origining_runtime);
	static OP_STATUS GetOrCreateTabGroup(DOM_BrowserTabGroup*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_group_id, DOM_Runtime* origining_runtime);
	static OP_STATUS GetOrCreateWindow(DOM_BrowserWindow*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId win_id, DOM_Runtime* origining_runtime);

	virtual void OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ActionData* data);
	virtual void OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ActionData* data);
	virtual void OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ActionData* data);

	void GCTrace();
	// OpHashTableForEachListener implementation.
	virtual void HandleKeyData(const void* key, void* data);

private:
	DOM_TabApiCache(DOM_ExtensionSupport* extension_support);
	OP_STATUS Construct();

	void OnCachedObjectDestroyed(DOM_TabApiCachedObject* object);
	/**< Called by DOM_TabApiCachedObject destructor. */
	OpINT32HashTable<DOM_TabApiCachedObject> m_cached_objects;
	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
#endif // DOM_EXTENSIONS_TABAPICACHE_H
