/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/extensions/dombrowsertabgroupevent.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domtabapicache.h"

/* static */ OP_STATUS
DOM_BrowserTabGroupEvent::Make(DOM_BrowserTabGroupEvent*& new_evt, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_group_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);
	OP_ASSERT(extension_support);
	OP_ASSERT(tab_group_id);
	return DOMSetObjectRuntime(new_evt = OP_NEW(DOM_BrowserTabGroupEvent, (extension_support, tab_group_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_GROUP_EVENT_PROTOTYPE), "BrowserTabGroupEvent");
}

/* virtual */
DOM_BrowserTabGroupEvent::~DOM_BrowserTabGroupEvent()
{
}

/* virtual */ void
DOM_BrowserTabGroupEvent::GCTrace()
{
	GCMark(m_tab_group);
	GCMark(m_prev_browser_window);
	DOM_Event::GCTrace();
}

DOM_BrowserTabGroup*
DOM_BrowserTabGroupEvent::GetTabGroup()
{
	if (!m_tab_group)
	{
		DOM_BrowserTabGroup* group;
		RETURN_VALUE_IF_ERROR(DOM_TabApiCache::GetOrCreateTabGroup(group, m_extension_support, m_tab_group_id, GetRuntime()), NULL);
		m_tab_group = group;
	}
	return m_tab_group;
}

DOM_BrowserWindow*
DOM_BrowserTabGroupEvent::GetPreviousBrowserWindow()
{
	if (!m_prev_browser_window && m_prev_browser_window_id != 0)
	{
		DOM_BrowserWindow* prev_window;
		RETURN_VALUE_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(prev_window, m_extension_support, m_prev_browser_window_id, GetRuntime()), NULL);
		m_prev_browser_window = prev_window;
	}
	return m_prev_browser_window;
}

/* virtual */ ES_GetState
DOM_BrowserTabGroupEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tabGroup:
		if (value)
			DOMSetObject(value, GetTabGroup());
		return GET_SUCCESS;
	case OP_ATOM_prevWindow:
		if (value)
		{
			if (m_prev_browser_window_id != 0)
				DOMSetObject(value, GetPreviousBrowserWindow());
			else
				DOMSetNull(value);

		}
		return GET_SUCCESS;
	case OP_ATOM_prevPosition:
		if (value)
			DOMSetNumber(value, m_prev_position);
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BrowserTabGroupEvent::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tabGroup:
	case OP_ATOM_prevWindow:
	case OP_ATOM_prevPosition:
		return PUT_SUCCESS; // Read-only.
	}
	return DOM_Event::PutName(property_name, value, origining_runtime);
}

void
DOM_BrowserTabGroupEvent::SetPreviousWindow(OpTabAPIListener::TabAPIItemId prev_browser_window_id, int prev_position)
{
	m_prev_browser_window_id = prev_browser_window_id;
	m_prev_position = prev_position;
}

#include "modules/dom/src/domglobaldata.h"

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
