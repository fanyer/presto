/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERWINDOWS_H
#define DOM_EXTENSIONS_BROWSERWINDOWS_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"

/** Implementation of BrowserTabGroupManager interface of Windows/Tabs API. */
class DOM_BrowserWindowManager
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_BrowserWindowManager*& new_obj, DOM_ExtensionSupport* extension, DOM_Runtime* origining_runtime);

	virtual ~DOM_BrowserWindowManager();
	void BeforeDestroy();

	DOM_DECLARE_FUNCTION(getAll);
	DOM_DECLARE_FUNCTION(create);
	DOM_DECLARE_FUNCTION(getLastFocused);

	enum {
		FUNCTIONS_getAll = 1,
		FUNCTIONS_create,
		FUNCTIONS_getLastFocused,
		FUNCTIONS_getFocused,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	/* from DOM_Object */
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_WINDOW_MANAGER || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object* GetOwnerObject() { return this; }

private:
	DOM_BrowserWindowManager(DOM_ExtensionSupport* extension_support)
		: m_notifications(this)
		, m_has_registered_listener(FALSE)
		, m_extension_support(extension_support)
	{
	}

	OP_STATUS RegisterWindowListener();
	/**< Registers m_notifications as listener for Window/Tab events. */

	class ExtensionWindowNotifications
		: public OpTabAPIListener::ExtensionWindowTabActionListener
	{
	public:
		ExtensionWindowNotifications(DOM_BrowserWindowManager* owner)
			: m_owner(owner)
		{
		}

		virtual void OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ExtensionWindowTabActionListener::ActionData* data);
		virtual void OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ExtensionWindowTabActionListener::ActionData* data) { return; }
		virtual void OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ExtensionWindowTabActionListener::ActionData* data) { return; }
	private:
		DOM_BrowserWindowManager* m_owner;
	};
	friend class ExtensionWindowNotifications;

	ExtensionWindowNotifications m_notifications;
	/**< Listener for tab/window evens. */

	BOOL m_has_registered_listener;
	/**< Set to TRUE after successfully registering a tab/window event listener. */

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
#endif // DOM_EXTENSIONS_BROWSERWINDOWS_H
