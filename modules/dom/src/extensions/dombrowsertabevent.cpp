/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowsertabevent.h"

#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domtabapicache.h"

/* static */ OP_STATUS
DOM_BrowserTabEvent::Make(DOM_BrowserTabEvent*& new_evt, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId tab_id, unsigned int window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);
	OP_ASSERT(extension_support);
	OP_ASSERT(tab_id);
	return DOMSetObjectRuntime(new_evt = OP_NEW(DOM_BrowserTabEvent, (extension_support, tab_id, window_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_EVENT_PROTOTYPE), "BrowserTabEvent");
}

/* virtual */
DOM_BrowserTabEvent::~DOM_BrowserTabEvent()
{
}

/* virtual */ void
DOM_BrowserTabEvent::GCTrace()
{
	GCMark(m_tab);
	GCMark(m_prev_browser_window);
	GCMark(m_prev_tab_group);
	DOM_Event::GCTrace();
}

DOM_BrowserTab*
DOM_BrowserTabEvent::GetTab()
{
	if (!m_tab)
	{
		DOM_BrowserTab* tab;
		RETURN_VALUE_IF_ERROR(DOM_TabApiCache::GetOrCreateTab(tab, m_extension_support, m_tab_id, m_window_id, GetRuntime()), NULL);
		m_tab = tab;
	}
	return m_tab;
}

DOM_BrowserTabGroup*
DOM_BrowserTabEvent::GetPreviousTabGroup()
{
	if (!m_prev_tab_group && m_prev_tab_group_id != 0)
	{
		DOM_BrowserTabGroup* prev_tab_group;
		RETURN_VALUE_IF_ERROR(DOM_TabApiCache::GetOrCreateTabGroup(prev_tab_group, m_extension_support, m_prev_tab_group_id, GetRuntime()), NULL);
		m_prev_tab_group = prev_tab_group;
	}
	return m_prev_tab_group;
}

DOM_BrowserWindow*
DOM_BrowserTabEvent::GetPreviousBrowserWindow()
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
DOM_BrowserTabEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tab:
		if (value)
			DOMSetObject(value, GetTab());
		return GET_SUCCESS;
	case OP_ATOM_prevTabGroup:
		if (value)
			DOMSetObject(value, GetPreviousTabGroup());
		return GET_SUCCESS;
	case OP_ATOM_prevWindow:
		if (value)
		{
			if (m_prev_browser_window_id != 0)
				DOMSetObject(value, GetPreviousBrowserWindow());
			else if (GetPreviousTabGroup())
				return GetPreviousTabGroup()->GetName(OP_ATOM_browserWindow, value, origining_runtime);
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

/* virtual */ ES_GetState
DOM_BrowserTabEvent::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_prevWindow:
		OP_ASSERT(GetPreviousTabGroup());
		return GetPreviousTabGroup()->GetNameRestart(OP_ATOM_browserWindow, value, origining_runtime, restart_object);

	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

void
DOM_BrowserTabEvent::SetPreviousTabGroup(OpTabAPIListener::TabAPIItemId prev_tab_group_id, int prev_position)
{
	m_prev_tab_group_id = prev_tab_group_id;
	m_prev_position = prev_position;
}

void
DOM_BrowserTabEvent::SetPreviousWindow(OpTabAPIListener::TabAPIItemId prev_browser_window_id, int prev_position)
{
	m_prev_browser_window_id = prev_browser_window_id;
	m_prev_position = prev_position;
}

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
