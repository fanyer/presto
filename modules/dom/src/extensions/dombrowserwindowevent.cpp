/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowserwindowevent.h"

#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domtabapicache.h"

/* static */ OP_STATUS
DOM_BrowserWindowEvent::Make(DOM_BrowserWindowEvent*& new_evt, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);
	OP_ASSERT(extension_support);
	OP_ASSERT(window_id);
	return DOMSetObjectRuntime(new_evt = OP_NEW(DOM_BrowserWindowEvent, (extension_support, window_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_WINDOW_EVENT_PROTOTYPE), "BrowserWindowEvent");
}


/* virtual */
DOM_BrowserWindowEvent::~DOM_BrowserWindowEvent()
{
}

/* virtual */ void
DOM_BrowserWindowEvent::GCTrace()
{
	GCMark(m_window);
	DOM_Event::GCTrace();
}

DOM_BrowserWindow*
DOM_BrowserWindowEvent::GetBrowserWindow()
{
	if (!m_window)
	{
		DOM_BrowserWindow* wnd;
		RETURN_VALUE_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(wnd, m_extension_support, m_window_id, GetRuntime()), NULL);
		m_window = wnd;
	}
	return m_window;
}

/* virtual */ ES_GetState
DOM_BrowserWindowEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_browserWindow:
		if (value)
			DOMSetObject(value, GetBrowserWindow());
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
