/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERWINDOW_H
#define DOM_EXTENSIONS_BROWSERWINDOW_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"
#include "modules/dom/src/extensions/domtabapicachedobject.h"

class DOM_BrowserTabManager;
class DOM_BrowserTabGroupManager;

/** Implementation of BrowserWindow interface of Windows/Tabs API. */
class DOM_BrowserWindow
	: public DOM_TabApiCachedObject
{
public:
	static OP_STATUS Make(DOM_BrowserWindow*& new_obj, DOM_ExtensionSupport* extension, OpTabAPIListener::TabAPIItemId win_id, DOM_Runtime* origining_runtime);

	virtual ~DOM_BrowserWindow();

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	ES_GetState GetWindowInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	/**< GetName helper for asynchronously obtaining window data using suspending calls. */

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_WINDOW || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(update);
	DOM_DECLARE_FUNCTION(focus);
	DOM_DECLARE_FUNCTION(insert);

	enum {
		FUNCTIONS_close = 1,
		FUNCTIONS_update,
		FUNCTIONS_focus,
		FUNCTIONS_insert,
		FUNCTIONS_ARRAY_SIZE
	};

	static int Update(DOM_BrowserWindow* window, ES_Object* properties, ES_Value* return_value, DOM_Runtime* origining_runtime);

	OpTabAPIListener::TabAPIItemId GetWindowId() { return DOM_TabApiCachedObject::GetId(); }
	/**< The unique ID for this browser group, supplied by the platform. */
private:
	friend class DOM_BrowserWindowManager;

	DOM_BrowserWindow(OpTabAPIListener::TabAPIItemId win_id, DOM_ExtensionSupport* extension_support)
		: DOM_TabApiCachedObject(extension_support, win_id)
		, m_tabs(NULL)
		, m_tab_groups(NULL)
	{
	}

	DOM_BrowserTabManager* GetTabs();
	/**< Gets the tab manager for this browser window or constructs it if it hasn't been constructed yet.
	     It may OOM if the tab manager has not been constructed yet in which case it will return NULL. */

	DOM_BrowserTabGroupManager* GetTabGroups();
	/**< Gets the tab group manager for this browser window or constructs it if it hasn't been constructed yet.
	     It may OOM if the tab group manager has not been constructed yet in which case it will return NULL. */
	void GetRectProperties(DOM_Runtime* origining_runtime, ES_Object* properties, OpRect& out_rect);

	DOM_BrowserTabManager* m_tabs;
	/**< Tab manager for this browser window. It is constructed when it is first needed.
	   Do not use this propertuy directly - use GetTabs() accessor. */

	DOM_BrowserTabGroupManager* m_tab_groups;
	/**< Tab group manager for this browser window. It is constructed when it is first needed.
	   Do not use this propertuy directly - use GetTabGroups() accessor. */
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERWINDOW_H
