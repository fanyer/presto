/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "modules/pi/OpPainter.h"

#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"

#include "platforms/x11api/plugins/plugin_unified_vega.h"
#include "platforms/x11api/plugins/unix_oppluginadapter.h"
#include "platforms/utilix/x11_all.h"
#include "platforms/x11api/plugins/unix_oppluginimage.h"
#include "platforms/x11api/pi/x11_client.h"

OP_STATUS OpPluginWindow::Create(OpPluginWindow** newObj, const OpRect &rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window)
{
	PluginUnifiedWindow *win = PluginUnifiedWindow::Create(rect, scale, parent, windowless, op_window);

	if (!win)
	{
		*newObj = 0;
		return OpStatus::ERR_NO_MEMORY;
	}

	*newObj = win;
	return OpStatus::OK;
}

PluginUnifiedWindow* PluginUnifiedWindow::Create(const OpRect& rect, int scale, OpView* parent_view, const bool windowless, OpWindow* parent_window)
{
	if (windowless)
	{
		OP_ASSERT(parent_window || !"PluginUnifiedWindow::Create: Cannot create a plug-in window outside browser window");
		return OP_NEW(PluginUnifiedWindow, (rect, scale, parent_view, parent_window));
	}

	PluginUnifiedVega* p = OP_NEW(PluginUnifiedVega, (rect, scale, parent_view, parent_window));
	if (!p || OpStatus::IsError(p->Init(X11Client::Instance()->GetDisplay())))
	{
		OP_DELETE(p);
		return NULL;
	}
	return p;
}

unsigned int PluginUnifiedWindow::CheckPaintEvent()
{
	return 1;
}

OP_STATUS PluginUnifiedWindow::EnableDirectPaint(const OpRect&)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

void* PluginUnifiedWindow::GetHandle()
{
	return 0;
}

unsigned int PluginUnifiedWindow::GetWindowHandle()
{
	return 0;
}

void PluginUnifiedWindow::SetAdapter(UnixOpPluginAdapter* adapter)
{
	m_adapter = adapter;
	adapter->SetPluginParentWindow(m_parent_window);
}

void PluginUnifiedWindow::SetPos(const int x, const int y)
{
	m_plugin_rect.x = x;
	m_plugin_rect.y = y;
}

void PluginUnifiedWindow::SetSize(const int w, const int h)
{
	m_plugin_rect.width  = w;
	m_plugin_rect.height = h;
}

BOOL PluginUnifiedWindow::SendEvent(OpPlatformEvent* event)
{
	OP_ASSERT(!"PluginUnifiedWindow::SendEvent: Shouldn't ever be used in a windowless instance");
	return OpStatus::ERR;
}

OP_STATUS PluginUnifiedWindow::FocusEvent(OpPlatformEvent**, const bool)
{
	OP_ASSERT(!"PluginUnifiedWindow::FocusEvent: Shouldn't ever be used in a windowless instance");
	return OpStatus::ERR;
}

OP_STATUS PluginUnifiedWindow::KeyEvent(OpPlatformEvent**, OpKey::Code, const uni_char*, const bool, OpKey::Location location, const ShiftKeyState)
{
	OP_ASSERT(!"PluginUnifiedWindow::KeyEvent: Shouldn't ever be used in a windowless instance");
	return OpStatus::ERR;
}

OP_STATUS PluginUnifiedWindow::MouseEvent(OpPlatformEvent**, const OpPluginEventType, const OpPoint&, const int, const ShiftKeyState)
{
	OP_ASSERT(!"PluginUnifiedWindow::MouseEvent: Deprecated. See UnixOpPluginTranslator::CreateMouseEvent.");
	return OpStatus::ERR;
}

OP_STATUS PluginUnifiedWindow::WindowMovedEvent(OpPlatformEvent** event)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

BOOL PluginUnifiedWindow::UsesDirectPaint() const
{
	return false;
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
