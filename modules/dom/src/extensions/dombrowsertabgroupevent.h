/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EXTENSIONS_BROWSERTABGROUPEVENT_H
#define DOM_EXTENSIONS_BROWSERTABGROUPEVENT_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class DOM_BrowserTabGroup;
class DOM_BrowserWindow;

class DOM_BrowserTabGroupEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_BrowserTabGroupEvent*& new_evt, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_group_id, DOM_Runtime* runtime);
	virtual ~DOM_BrowserTabGroupEvent();


	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_TAB_GROUP_EVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	void SetPreviousWindow(OpTabAPIListener::TabAPIItemId prev_browser_window_id, int prev_position);

private:
	DOM_BrowserTabGroupEvent(DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_group_id)
		: m_tab_group_id(tab_group_id)
		, m_tab_group(NULL)
		, m_prev_position(0)
		, m_prev_browser_window_id(0)
		, m_prev_browser_window(NULL)
		, m_extension_support(extension_support)
	{
	}

	DOM_BrowserTabGroup* GetTabGroup();
	DOM_BrowserWindow* GetPreviousBrowserWindow();

	OpTabAPIListener::TabAPIItemId m_tab_group_id;
	DOM_BrowserTabGroup* m_tab_group;

	unsigned int m_prev_position;
	OpTabAPIListener::TabAPIItemId m_prev_browser_window_id;
	DOM_BrowserWindow* m_prev_browser_window;

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERTABGROUPEVENT_H
