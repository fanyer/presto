/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW

#include "modules/ns4plugins/src/pluginnativewindow.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/ns4plugins/src/plugin.h"

////////////////////////////////////////////////////////////////////////

PluginNativeWindow::PluginNativeWindow(const MDE_RECT &rect, OpPluginWindow *plugin_window, Plugin *plugin)
	: MDENativeWindow(rect)
	, m_plugin_window(plugin_window)
	, m_plugin(plugin)
	, m_send_onmouseover_on_next_mousemove(true)
{
}

void PluginNativeWindow::MoveWindow(int x, int y)
{
	m_plugin_window->SetPos(x, y);
}

void PluginNativeWindow::ResizeWindow(int w, int h)
{
	m_plugin_window->SetSize(w, h);
}

void PluginNativeWindow::Attach()
{
	if (!m_parent)
	{
		static_cast<MDE_OpView*>(m_plugin_window->GetParentView())->GetMDEWidget()->AddChild(this);
		SetTransparent(true);
		SetFullyTransparent(true);
	}
}

void PluginNativeWindow::Detach()
{
	if (m_parent)
	{
		m_parent->RemoveChild(this);
	}
}

void PluginNativeWindow::ShowWindow(BOOL show)
{
	if (show)
	{
		Attach();
		m_plugin_window->Show();
	}
	else
	{
		Detach();
		m_plugin_window->Hide();
	}
}

void PluginNativeWindow::SetClipRegion(MDE_Region* rgn)
{
	m_plugin_window->SetClipRegion(rgn);
}

#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW
