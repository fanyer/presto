/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERTABGROUPS_H
#define DOM_EXTENSIONS_BROWSERTABGROUPS_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domasynccallback.h"

/** Implementation of BrowserTabGroupManager interface of Windows/Tabs API. */
class DOM_BrowserTabGroupManager
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_BrowserTabGroupManager*& new_obj, DOM_ExtensionSupport* extension, unsigned int window_id, DOM_Runtime* origining_runtime);

	virtual ~DOM_BrowserTabGroupManager();

	DOM_DECLARE_FUNCTION(create);
	DOM_DECLARE_FUNCTION(getAll);

	enum {
		FUNCTIONS_create = 1,
		FUNCTIONS_getAll,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener= 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	/* from DOM_Object */
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_TAB_GROUP_MANAGER || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object* GetOwnerObject() { return this; }

	OpTabAPIListener::TabAPIItemId GetWindowId() { return m_window_id; }

	void BeforeDestroy();
	/**< Called by DOM_EnvirnomentImpl before it is destroyed. */
private:
	DOM_BrowserTabGroupManager(DOM_ExtensionSupport* extension_support, unsigned int window_id)
		: m_notifications(this)
		, m_has_registered_listener(FALSE)
		, m_window_id(window_id)
		, m_extension_support(extension_support)
	{
	}

	BOOL IsGlobalTabGroupsCollection() { return m_window_id == 0; }
	BOOL IsWindowTabGroupsCollection() { return m_window_id != 0; }

	OP_STATUS RegisterTabsListener();
	/**< Registers m_notifications as listener for Window/Tab events. */

	class ExtensionTabGroupNotifications
		: public OpTabAPIListener::ExtensionWindowTabActionListener
	{
	public:
		ExtensionTabGroupNotifications(DOM_BrowserTabGroupManager* owner)
			: m_owner(owner)
		{
		}

		virtual void OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ExtensionWindowTabActionListener::ActionData* data) { return; }
		virtual void OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ExtensionWindowTabActionListener::ActionData* data) { return; }
		virtual void OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ExtensionWindowTabActionListener::ActionData* data);

	private:
		DOM_BrowserTabGroupManager* m_owner;
	};
	friend class ExtensionTabGroupNotifications;

	ExtensionTabGroupNotifications m_notifications;
	/**< Listener for tab/window evens. */

	BOOL m_has_registered_listener;
	/**< Set to TRUE after successfully registering a tab/window event listener. */

	OpTabAPIListener::TabAPIItemId m_window_id;
	/**< The unique ID of a browser window if this is a tab group manager of a BrowserWindow
	      or 0 if this is a global tab group manager. */

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERTABGROUPS_H
