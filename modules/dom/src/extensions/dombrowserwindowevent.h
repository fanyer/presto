/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EXTENSIONS_BROWSERWINDOWEVENT_H
#define DOM_EXTENSIONS_BROWSERWINDOWEVENT_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class DOM_BrowserWindow;

class DOM_BrowserWindowEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_BrowserWindowEvent*& new_evt, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId window_id, DOM_Runtime* runtime);
	virtual ~DOM_BrowserWindowEvent();


	virtual BOOL IsA(int type) { return type == DOM_TYPE_BROWSER_WINDOW_EVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

private:
	DOM_BrowserWindowEvent(DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId window_id)
		: m_window_id(window_id)
		, m_window(NULL)
		, m_extension_support(extension_support)
	{
	}

	DOM_BrowserWindow* GetBrowserWindow();

	OpTabAPIListener::TabAPIItemId m_window_id;
	DOM_BrowserWindow* m_window;

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_BROWSERWINDOWEVENT_H
