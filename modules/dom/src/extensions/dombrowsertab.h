/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_BROWSERTAB_H
#define DOM_EXTENSIONS_BROWSERTAB_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/extensions/domtabapicachedobject.h"
#include "modules/windowcommander/OpTabAPIListener.h"

/** Implementation of BrowserTab interface of Windows/Tabs API. */
class DOM_BrowserTab
	: public DOM_TabApiCachedObject
{
public:
	static OP_STATUS Make(DOM_BrowserTab *&new_obj, DOM_ExtensionSupport *extension, unsigned int tab_id, unsigned long window_id, DOM_Runtime *origining_runtime);
	/**< Construct a new DOM_BrowserTab object, representing and referring
	     to a "tab'ed" window, possibly residing within a toplevel window container.
	     The Windows + Tabs API gives access to these objects, providing
	     move/create/delete/update operations over them.

	     @param tab_id the unique ID assigned by the platform of the tabbed window.
	     @param window_id the unique ID that Core assigns to the Window
	            corresponding to this tab. May be 0 if this tab doesn't hava a
				core Window assigned.
	     @return OpStatus::ERR_NO_MEMORY if a OOM condition hits;
	             OpStatus::OK if created OK.
	             No other error conditions */

	virtual ~DOM_BrowserTab();

	DOM_DECLARE_FUNCTION(postMessage);
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(update);
	DOM_DECLARE_FUNCTION(focus);
	DOM_DECLARE_FUNCTION(refresh);

	enum {
		FUNCTIONS_postMessage = 1,
		FUNCTIONS_close,
		FUNCTIONS_update,
		FUNCTIONS_focus,
		FUNCTIONS_refresh,
		FUNCTIONS_ARRAY_SIZE
	};

	 // APIs injected conditionally - not in the prototype.
	DOM_DECLARE_FUNCTION(getScreenshot);

	/** GetName helper for asynchronously obtaining tab data using suspending calls. */
	ES_GetState GetTabInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_TAB || DOM_Object::IsA(type); }

	unsigned int GetWindowId() { return m_window_id; }
	/**< Gets the unique core Window ID for this tab. */

	Window* GetTabWindow();
	/**< Gets the core Window for this tab or NULL if the id is not valid. */

	unsigned int GetTabId() { return DOM_TabApiCachedObject::GetId(); }
	/**< Gets the unique ID for this tab, supplied by the platform. */
private:
	friend class DOM_BrowserTabManager;

	DOM_BrowserTab(DOM_ExtensionSupport* extension_support, unsigned int window_id, OpTabAPIListener::TabAPIItemId tab_id)
		: DOM_TabApiCachedObject(extension_support, tab_id)
		, m_window_id(window_id)
		, m_port(NULL)
	{
	}

	void InjectOptionalAPIL();

	unsigned int m_window_id;
	/**< The unique ID for the corresponding Core Window. Keeping a direct reference isn't safe. */

	DOM_MessagePort *m_port;
	/** The entangled port for communicating with the extension operating 'in' the tabbed window. */

	DOM_MessagePort* GetPort();
	/**< Gets the port which can be used to send/receive messages from the tabs UserJS.
	     If the port has not been yet constructed then it will be constructed.
	     If any error occurrs then NULL is returned. */

	BOOL IsPrivateDataAllowed();
	/**< Checks if the runtime has access to private tabs. */
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERTAB_H
