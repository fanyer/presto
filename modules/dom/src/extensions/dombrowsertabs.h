/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERTABS_H
#define DOM_EXTENSIONS_BROWSERTABS_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/src/extensions/domextensionsupport.h"

#include "modules/dom/src/domasynccallback.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/windowcommander/OpTabAPIListener.h"

/** Implementation of BrowserTabManager interface of Windows/Tabs API. */
class DOM_BrowserTabManager
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	static OP_STATUS MakeGlobalTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension, DOM_Runtime* origining_runtime);
	static OP_STATUS MakeWindowTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension, DOM_Runtime* origining_runtime, OpTabAPIListener::TabAPIItemId window_id);
	static OP_STATUS MakeTabGroupTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension, DOM_Runtime* origining_runtime, OpTabAPIListener::TabAPIItemId tab_group_id);

	virtual ~DOM_BrowserTabManager();

	void BeforeDestroy();
	/**< Called by DOM_EnvirnomentImpl before it is destroyed. */

	/* from DOM_Object */
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual BOOL TypeofYieldsObject() { return TRUE; }
	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_TAB_MANAGER || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object* GetOwnerObject() { return this; }

	DOM_DECLARE_FUNCTION(create);
	DOM_DECLARE_FUNCTION(getAll);
	DOM_DECLARE_FUNCTION(getSelected);
	DOM_DECLARE_FUNCTION(close);

	enum {
		FUNCTIONS_create = 1,
		FUNCTIONS_getAll,
		FUNCTIONS_getSelected,
		FUNCTIONS_getFocused,
		FUNCTIONS_close,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener= 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	DOM_BrowserTabManager(DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId window_id, OpTabAPIListener::TabAPIItemId tab_group_id)
		: m_notifications(this)
		, m_has_registered_listener(FALSE)
		, m_window_id(window_id)
		, m_tab_group_id(tab_group_id)
		, m_extension_support(extension_support)
	{
		OP_ASSERT(m_window_id == 0 || m_tab_group_id == 0);
	}

	OP_STATUS RegisterTabsListener();
	/**< Registers m_notifications as listener for Window/Tab events. */

	class ExtensionTabNotifications
		: public OpTabAPIListener::ExtensionWindowTabActionListener
	{
	public:
		ExtensionTabNotifications(DOM_BrowserTabManager* owner)
			: m_owner(owner)
		{
		}

		virtual void OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ActionData* data) { return; }
		virtual void OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ActionData* data);
		virtual void OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ActionData* data) { return; }
	private:
		DOM_BrowserTabManager* m_owner;
	};
	friend class ExtensionTabNotifications;

	ExtensionTabNotifications m_notifications;
	/**< Listener for tab/window evens. */

	BOOL m_has_registered_listener;
	/**< Set to TRUE after successfully registering a tab/window event listener. */

	BOOL IsGlobalTabCollection() { return m_window_id == 0 && m_tab_group_id == 0; }
	BOOL IsWindowTabCollection() { return m_window_id != 0; }
	BOOL IsTabGroupTabCollection() { return m_window_id == 0 && m_tab_group_id != 0; }

	OpTabAPIListener::TabAPIItemId m_window_id;
	/**< The unique ID of a browser window if this is a tab manager of a BrowserWindow
	      or 0 if this is a global tab manager or BrowserTabGroup tab manager. */

	OpTabAPIListener::TabAPIItemId m_tab_group_id;
	/**< The unique ID of a browser tab group if this is a tab manager of a BrowserTabGroup
	      or 0 if this is a global tab manager or BrowserWindow tab manager. */

	int MakeTabArray(ES_Value* return_value, const OpTabAPIListener::TabAPIItemIdCollection* tabs, const OpTabAPIListener::TabWindowIdCollection* tab_windows, DOM_Runtime* origining_runtime);

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERTABS_H
