/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERTABGROUP_H
#define DOM_EXTENSIONS_BROWSERTABGROUP_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/dom/src/extensions/domtabapicachedobject.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class DOM_BrowserTabManager;

/** Implementation of BrowserTabGroup interface of Windows/Tabs API. */
class DOM_BrowserTabGroup
	: public DOM_TabApiCachedObject
{
public:
	static OP_STATUS Make(DOM_BrowserTabGroup*& new_obj, DOM_ExtensionSupport* extension, unsigned int tab_group_id, DOM_Runtime* origining_runtime);
	/**< Construct a new DOM_BrowserTabGroup object, representing and referring
	     to a group of tabs. The Windows + Tabs API gives access to these objects, providing
	     move/create/delete/update operations over them.

	     @param tab_group_id the unique ID assigned by the platform of the tab group.
	     @param window_id the unique ID that Core assigns to the Window
	            corresponding to this tab.
	     @return OpStatus::ERR_NO_MEMORY if a OOM condition hits;
	             OpStatus::OK if created OK.
	             No other error conditions */
	virtual ~DOM_BrowserTabGroup();

	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(focus);
	DOM_DECLARE_FUNCTION(update);

	enum {
		FUNCTIONS_close = 1,
		FUNCTIONS_focus,
		FUNCTIONS_update,
		FUNCTIONS_insert,
		FUNCTIONS_ARRAY_SIZE
	};

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState GetTabGroupInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_TAB_GROUP || DOM_Object::IsA(type); }

	OpTabAPIListener::TabAPIItemId GetTabGroupId() { return DOM_TabApiCachedObject::GetId(); }
	/**< Gets the unique ID for this tab group, supplied by the platform. */
private:
	DOM_BrowserTabGroup(DOM_ExtensionSupport* extension_support, unsigned int tab_group_id)
		: DOM_TabApiCachedObject(extension_support, tab_group_id)
		, m_tabs(NULL)
	{

	}

	DOM_BrowserTabManager* GetTabs();
	/**< Gets the tab manager for this tab group or constructs it if it hasn't been constructed yet.
	     It may OOM if the tab manager has not been constructed yet in which case it will return NULL. */

	DOM_BrowserTabManager* m_tabs;
	/**< Tab manager for this tab group. It is constructed when it is first needed.
	   Do not use this propertuy directly - use GetTabs() accessor. */
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERTABGROUP_H
