/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW

#include "modules/ns4plugins/src/proxyoppluginwindow.h"
#include "modules/ns4plugins/src/pluginnativewindow.h"
#include "modules/libgogi/mde.h"

OP_STATUS ProxyOpPluginWindow::Create(OpPluginWindow** new_object, const OpRect& rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window, Plugin* plugin)
{
	OpAutoPtr<ProxyOpPluginWindow> plugin_window (OP_NEW(ProxyOpPluginWindow, ()));
	RETURN_OOM_IF_NULL(plugin_window.get());

	RETURN_IF_ERROR(OpPluginWindow::Create(&plugin_window->m_plugin_window, rect, scale, parent, windowless, op_window));

	MDE_RECT mde_rect = {rect.x, rect.y, rect.width, rect.height};
	plugin_window->m_plugin_native_window = OP_NEW(PluginNativeWindow, (mde_rect,  plugin_window->m_plugin_window, plugin));
	RETURN_OOM_IF_NULL(plugin_window->m_plugin_native_window);

	*new_object = plugin_window.release();

	return OpStatus::OK;
}

ProxyOpPluginWindow::ProxyOpPluginWindow() :
	m_plugin_window(NULL),
	m_plugin_native_window(NULL)
{
}

ProxyOpPluginWindow::~ProxyOpPluginWindow()
{
	OP_DELETE(m_plugin_window);
	OP_DELETE(m_plugin_native_window);
}

void ProxyOpPluginWindow::SetListener(OpPluginWindowListener *listener)
{
	m_plugin_window->SetListener(listener);
}

void ProxyOpPluginWindow::Show()
{
	m_plugin_window->Show();
	if (UsesPluginNativeWindow())
	{
		// Attach the native window
		m_plugin_native_window->Attach();
		m_plugin_native_window->Show(TRUE);
	}
}

void ProxyOpPluginWindow::Hide()
{
	m_plugin_window->Hide();
	if (UsesPluginNativeWindow())
	{
		// Detach the native window
		m_plugin_native_window->Detach();
		m_plugin_native_window->Show(FALSE);
	}
}

void ProxyOpPluginWindow::SetPos(int x, int y)
{
	if (!UsesPluginNativeWindow())
	{
		m_plugin_window->SetPos(x, y);
		m_plugin_native_window->Move(x, y);
	}
}

void ProxyOpPluginWindow::SetSize(int w, int h)
{
	m_plugin_window->SetSize(w, h);
	m_plugin_native_window->Resize(w, h);
}

void *ProxyOpPluginWindow::GetHandle()
{
	return m_plugin_window->GetHandle();
}

void ProxyOpPluginWindow::BlockMouseInput(BOOL block)
{
	m_plugin_window->BlockMouseInput(block);
}

BOOL ProxyOpPluginWindow::IsBlocked()
{
	return m_plugin_window->IsBlocked();
}

OP_STATUS ProxyOpPluginWindow::CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers)
{
	return m_plugin_window->CreateMouseEvent(event, event_type, point, button, modifiers);
}

OP_STATUS ProxyOpPluginWindow::CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect)
{
	return m_plugin_window->CreatePaintEvent(event, painter, rect);
}

OP_STATUS ProxyOpPluginWindow::CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers)
{
	return m_plugin_window->CreateKeyEvent(key_event, key, key_value, key_event_data, key_state, location, modifiers);
}

OP_STATUS ProxyOpPluginWindow::CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in)
{
	return m_plugin_window->CreateFocusEvent(focus_event, focus_in);
}

OP_STATUS ProxyOpPluginWindow::CreateWindowPosChangedEvent(OpPlatformEvent** event)
{
	return m_plugin_window->CreateWindowPosChangedEvent(event);
}

OP_STATUS ProxyOpPluginWindow::ConvertPoint(double source_x, double source_y, int source_space, double* dest_x, double* dest_y, int dest_space)
{
	return m_plugin_window->ConvertPoint(source_x, source_y, source_space, dest_x, dest_y, dest_space);
}

OP_STATUS ProxyOpPluginWindow::PopUpContextMenu(void* menu)
{
	return m_plugin_window->PopUpContextMenu(menu);
}

BOOL ProxyOpPluginWindow::SendEvent(OpPlatformEvent* event)
{
	return m_plugin_window->SendEvent(event);
}

BOOL ProxyOpPluginWindow::UsesDirectPaint() const
{
	return m_plugin_window->UsesDirectPaint();
}

OP_STATUS ProxyOpPluginWindow::PaintDirectly(const OpRect& rect)
{
	return m_plugin_window->PaintDirectly(rect);
}

#ifdef NS4P_SILVERLIGHT_WORKAROUND
void ProxyOpPluginWindow::InvalidateWindowed(const OpRect& rect)
{
	m_plugin_window->InvalidateWindowed(rect);
}
#endif // NS4P_SILVERLIGHT_WORKAROUND

void ProxyOpPluginWindow::SetPluginObject(Plugin* plugin)
{
	m_plugin_window->SetPluginObject(plugin);
}

void ProxyOpPluginWindow::Detach()
{
	m_plugin_native_window->Detach();
	m_plugin_window->Detach();
}

unsigned int ProxyOpPluginWindow::CheckPaintEvent()
{
	return m_plugin_window->CheckPaintEvent();
}

BOOL ProxyOpPluginWindow::UsesPluginNativeWindow() const
{
	return m_plugin_window->UsesPluginNativeWindow();
}

OpView *ProxyOpPluginWindow::GetParentView()
{
	return m_plugin_window->GetParentView();
}

void ProxyOpPluginWindow::SetClipRegion(MDE_Region *rgn)
{
	m_plugin_window->SetClipRegion(rgn);
}

#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW
