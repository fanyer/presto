/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/hardcore/component/Messages.h"
#include "modules/ns4plugins/src/plugin.h"

#include "platforms/windows/pi/WindowsOpPluginAdapter.h"
#include "platforms/windows/pi/WindowsOpPluginWindow.h"

/* static */
OP_STATUS OpNS4PluginAdapter::Create(OpNS4PluginAdapter** new_object, OpComponentType component_type)
{
	WindowsOpNS4PluginAdapter* adapter = OP_NEW(WindowsOpNS4PluginAdapter, ());
	RETURN_OOM_IF_NULL(adapter);

	OP_STATUS ret = adapter->Construct();
	if (OpStatus::IsError(ret))
	{
		OP_DELETE(adapter);
		return ret;
	}

	*new_object = adapter;

	return OpStatus::OK;
}

#ifdef NS4P_COMPONENT_PLUGINS
/* static */
bool OpNS4PluginAdapter::IsBlacklistedFilename(UniString plugin_filepath)
{
	return false;
}
#endif // NS4P_COMPONENT_PLUGINS

/* static */
BOOL OpNS4PluginAdapter::GetValueStatic(NPNVariable, void*, NPError*)
{
	return FALSE;
}

OP_STATUS WindowsOpNS4PluginAdapter::Construct()
{
	RETURN_OOM_IF_NULL(m_channel = g_opera->CreateChannel());
	RETURN_IF_ERROR(m_channel->AddMessageListener(this));

	return OpStatus::OK;
}

OP_STATUS WindowsOpNS4PluginAdapter::OnWindowsPluginWindowInfoMessage(OpWindowsPluginWindowInfoMessage* message)
{
	if (!message)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpWindowsPluginWindowInfoResponseMessage> response (OpWindowsPluginWindowInfoResponseMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	response->SetWindow(reinterpret_cast<uintptr_t>(m_plugin_window->GetHandle()));

	return message->Reply(response.release());
}

OP_STATUS WindowsOpNS4PluginAdapter::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS status = OpStatus::ERR;

	switch (message->GetType())
	{
		case OpPeerDisconnectedMessage::Type:
			m_channel = NULL;
			status = OpStatus::OK;
			break;

		case OpWindowsPluginWindowInfoMessage::Type:
			status = OnWindowsPluginWindowInfoMessage(OpWindowsPluginWindowInfoMessage::Cast(message));
			break;

		default:
			OP_ASSERT(!"WindowsOpNS4PluginAdapter::ProcessMessage: Received unknown/unhandled message");
			break;
	}

	return status;
}

#ifndef NS4P_COMPONENT_PLUGINS
BOOL WindowsOpNS4PluginAdapter::GetValue(NPNVariable variable, void* value, NPError* result)
{
	*result = NPERR_NO_ERROR;
	switch (variable)
	{
		case NPNVnetscapeWindow:
			if (!m_plugin_window)
				return FALSE;
			*(static_cast<void**>(value)) = m_plugin_window->GetParentWindowHandle();
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

BOOL WindowsOpNS4PluginAdapter::ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect)
{
	RECT rect;
	if (GetRgnBox(static_cast<HRGN>(invalidRegion), &rect) == SIMPLEREGION)
	{
		invalidRect.top = static_cast<uint16>(rect.top);
		invalidRect.left = static_cast<uint16>(rect.left);
		invalidRect.bottom = static_cast<uint16>(rect.bottom);
		invalidRect.right = static_cast<uint16>(rect.right);
		return TRUE;
	}
	return FALSE;
}

OP_STATUS WindowsOpNS4PluginAdapter::SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent)
{
	OP_ASSERT(m_plugin_window);
	if (!m_plugin_window)
		return OpStatus::ERR;

	if (m_plugin_window->IsWindowless())
		npwin->window = m_plugin_window->GetRenderContext();
	else
		npwin->window = m_plugin_window->GetHandle();

	npwin->x = rect.x + view_offset.x;
	npwin->y = rect.y + view_offset.y;
	npwin->width = rect.width;
	npwin->height = rect.height;

	m_plugin_window->SetNPWindow(npwin);
	m_plugin_window->SetRect(rect);
	m_plugin_window->SetViewOffset(view_offset);
	m_plugin_window->SetViewRect(view_rect);
	m_plugin_window->SetTransparent(transparent);

	return OpStatus::OK;
}

OpWindowsPlatformEvent::~OpWindowsPlatformEvent()
{
	if (m_event.event == WM_WINDOWPOSCHANGED && m_event.lParam)
		OP_DELETE((WINDOWPOS*)m_event.lParam);
}
#endif // !NS4P_COMPONENT_PLUGINS
