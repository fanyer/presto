/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_EXTENSION_BACKGROUND_H
#define DOM_EXTENSIONS_EXTENSION_BACKGROUND_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/extensions/domextensionsupport.h"

class DOM_ExtensionContexts;
class DOM_BrowserTabManager;
class DOM_BrowserTabGroupManager;
class DOM_BrowserWindowManager;
class DOM_TabApiCache;

/**  This class implements opera.extension.* in the background page
 */
class DOM_ExtensionBackground
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_ExtensionBackground *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime);

	void BeforeDestroy();
	virtual ~DOM_ExtensionBackground();

	DOM_DECLARE_FUNCTION(broadcastMessage);
	DOM_DECLARE_FUNCTION(getFile);

	enum {
		FUNCTIONS_broadcastMessage = 1,
		FUNCTIONS_getFile,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	 // APIs injected conditionally - not in the prototype.
	DOM_DECLARE_FUNCTION(getScreenshot);

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_BACKGROUND || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }

	DOM_MessagePort *GetPort() { return m_port; }
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	DOM_TabApiCache* GetTabApiCache() { return m_tab_api_cache; }
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
	DOM_ExtensionContexts* GetContexts() { return m_contexts; }
	DOM_ExtensionSupport *GetExtensionSupport() { return m_extension_support; }

private:
	DOM_ExtensionBackground(DOM_ExtensionSupport *extension_support)
		: m_contexts(NULL)
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
		, m_tabs(NULL)
		, m_tab_groups(NULL)
		, m_windows(NULL)
		, m_tab_api_cache(NULL)
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
		, m_port(NULL)
		, m_onconnect_handler(NULL)
		, m_ondisconnect_handler(NULL)
		, m_onmessage_handler(NULL)
		, m_extension_support(extension_support)
	{
	}

	OP_STATUS Initialize(DOM_Runtime *runtime);

	DOM_ExtensionContexts *m_contexts;
	/**< The opera.contexts.* namespace object */
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	DOM_BrowserTabManager *m_tabs;
	/**< The opera.extensions.tabs object */

	DOM_BrowserTabGroupManager *m_tab_groups;
	/**< The opera.extensions.tabsGroups object */

	DOM_BrowserWindowManager *m_windows;
	/**< The opera.extensions.windows object */

	DOM_TabApiCache* m_tab_api_cache;
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
	DOM_MessagePort *m_port;
	/**< Where the extension background is listening (and registers) events. */

	/* .onXXX handlers for opera.extension.* */
	DOM_EventListener *m_onconnect_handler;
	DOM_EventListener *m_ondisconnect_handler;
	DOM_EventListener *m_onmessage_handler;

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_EXTENSION_BACKGROUND_H
