/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/plugin_window_tracker.h"

PluginWindowTracker* PluginWindowTracker::s_instance = 0;

/***********************************************************************************
 ** Get manager
 **
 ** PluginWindowTracker::GetInstance()
 **
 ***********************************************************************************/
PluginWindowTracker& PluginWindowTracker::GetInstance()
{
	if(!s_instance)
	{
		OP_NEW_DBG("PluginWindowTracker::GetInstance()", "plugin_window_tracker");
		s_instance = OP_NEW(PluginWindowTracker, ());
		if (s_instance && OpStatus::IsError(s_instance->m_x11_callbacks.Construct()))
			DeleteInstance();
	}

	return *s_instance;
}

void PluginWindowTracker::DeleteInstance()
{
	OP_NEW_DBG("PluginWindowTracker::DeleteInstance()", "plugin_window_tracker");
	OP_DELETE(s_instance);
	s_instance = 0;
}

PluginWindowTracker::~PluginWindowTracker()
{
	OP_NEW_DBG("PluginWindowTracker::~PluginWindowTracker()", "plugin_window_tracker");
#ifdef DEBUG
	OpHashIterator* window_iterator = m_plugin_windows.GetIterator();
	if (window_iterator && OpStatus::IsSuccess(window_iterator->First()))
	{
		unsigned int count=0;
		do {
			OP_DBG(("%d: window: %p; plugin_window: %p", ++count, window_iterator->GetKey(), window_iterator->GetData()));
		} while (OpStatus::IsSuccess(window_iterator->Next()));
	}
	OP_DELETE(window_iterator);
#endif // DEBUG
}

OP_STATUS PluginWindowTracker::AddPluginWindow(unsigned int window, PluginUnifiedWindow* plugin_window)
{
	OP_NEW_DBG("PluginWindowTracker::AddPluginWindow()", "plugin_window_tracker");
	OP_DBG(("window: %#8x; plugin_window: %p", window, plugin_window));
	return m_plugin_windows.Add(window, plugin_window);
}

OP_STATUS PluginWindowTracker::RemovePluginWindow(unsigned int window)
{
	OP_NEW_DBG("PluginWindowTracker::RemovePluginWindow()", "plugin_window_tracker");
	PluginUnifiedWindow* plugin_window = 0;
	OP_STATUS status = m_plugin_windows.Remove(window, &plugin_window);
	OP_DBG(("window: %#8x; plugin_window: %p", window, plugin_window));
	return status;
}

void PluginWindowTracker::ReparentedPluginWindow(unsigned int child_window, unsigned int parent_window)
{
	OP_NEW_DBG("PluginWindowTracker::ReparentedPluginWindow()", "plugin_window_tracker");
	OP_DBG(("child: %#8x; parent: %#8x", child_window, parent_window));

	PluginUnifiedWindow* plugin_window = GetPluginWindow(parent_window);
	if(plugin_window)
	{
		plugin_window->OnPluginWindowAdded(child_window, FALSE);
	}
}

PluginUnifiedWindow* PluginWindowTracker::GetPluginWindow(unsigned int window)
{
	PluginUnifiedWindow* plugin_window = 0;
	OpStatus::Ignore(m_plugin_windows.GetData(window, &plugin_window));
	return plugin_window;
}

OP_STATUS PluginWindowTracker::registerX11EventCallback(uint32_t win, bool (*cb)(XEvent *))
{
	return m_x11_callbacks.addCallback(win, cb);
}

void PluginWindowTracker::unregisterX11EventCallback(uint32_t win, bool (*cb)(XEvent *))
{
	m_x11_callbacks.removeCallback(win, cb);
}

bool PluginWindowTracker::HandleEvent(XEvent * event)
{
	return m_x11_callbacks.callCallbacks(event);
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
