/**
 * Copyright (C) 2011-12 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_UNIX_PLATFORM) && defined(NS4P_COMPONENT_PLUGINS)

#include "modules/hardcore/component/Messages.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/opdata/UniStringUTF8.h"
#include "platforms/x11api/pi/plugixparentwindowlistener.h"
#include "platforms/x11api/pi/x11_client.h"
#include "platforms/x11api/plugins/plugin_unified_window.h"
#include "platforms/x11api/plugins/unix_oppluginadapter.h"
#include "platforms/x11api/src/generated/g_message_x11api_messages.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"

#include <X11/Xregion.h>

OP_STATUS OpNS4PluginAdapter::Create(OpNS4PluginAdapter** new_object, OpComponentType)
{
	*new_object = NULL;

	OpAutoPtr<UnixOpPluginAdapter> adapter(OP_NEW(UnixOpPluginAdapter, ()));
	RETURN_OOM_IF_NULL(adapter.get());

	RETURN_IF_ERROR(adapter->Construct());
	*new_object = adapter.release();

	return OpStatus::OK;
}

/* static */
bool OpNS4PluginAdapter::IsBlacklistedFilename(UniString plugin_filepath)
{
	char plugin_realpath[_MAX_PATH+1]; // ARRAY OK 2012-05-03 molsson
	if (OpStatus::IsError(PosixFileUtil::RealPath(plugin_filepath.Data(true), plugin_realpath)))
		return true;
	OpString plugin_realpath_uni;
	if (OpStatus::IsError(PosixNativeUtil::FromNative(plugin_realpath, &plugin_realpath_uni)))
		return true;
	return !!g_pcapp->IsPluginToBeIgnored(plugin_realpath_uni.CStr());
}

BOOL OpNS4PluginAdapter::GetValueStatic(NPNVariable, void*, NPError*)
{
	return FALSE;
}

UnixOpPluginAdapter::UnixOpPluginAdapter()
	: m_channel(NULL)
	, m_parent_window(NULL)
	, m_parent_listener(NULL)
	, m_plugin_window(NULL) { }

UnixOpPluginAdapter::~UnixOpPluginAdapter()
{
	OP_DELETE(m_parent_listener);
	OP_DELETE(m_channel);
}

OP_STATUS UnixOpPluginAdapter::Construct()
{
	RETURN_OOM_IF_NULL(m_channel = g_opera->CreateChannel());
	RETURN_IF_ERROR(m_channel->AddMessageListener(this));
	return OpStatus::OK;
}

void UnixOpPluginAdapter::SetPluginParentWindow(OpWindow* parent_win)
{
	m_parent_window = parent_win;
	if (!m_parent_listener
	    && OpStatus::IsError(PlugixParentWindowListener::Create(&m_parent_listener, this)))
		return;
	m_parent_listener->SetParent(parent_win);
}

void UnixOpPluginAdapter::SetPluginWindow(OpPluginWindow* plug_win)
{
	m_plugin_window = static_cast<PluginUnifiedWindow*>(plug_win);
	m_plugin_window->SetAdapter(this);
}

void UnixOpPluginAdapter::SetVisualDevice(VisualDevice* vis_dev) { }
BOOL UnixOpPluginAdapter::GetValue(NPNVariable var, void* val, NPError* err) { return FALSE; }
BOOL UnixOpPluginAdapter::SetValue(NPPVariable var, void* val, NPError* err) { return FALSE; }

BOOL UnixOpPluginAdapter::ConvertPlatformRegionToRect(NPRegion region, NPRect& out_rect)
{
	if (region->numRects == 0)
		return FALSE;

	out_rect.top = region->rects[0].y1;
	out_rect.left = region->rects[0].x1;
	out_rect.bottom = region->rects[0].y2;
	out_rect.right = region->rects[0].x2;

	for (int i = 1; i < region->numRects; ++i)
	{
		if (region->rects[i].y1 < out_rect.top)
			out_rect.top = region->rects[i].y1;
		if (region->rects[i].x1 < out_rect.left)
			out_rect.left = region->rects[i].x1;
		if (region->rects[i].y2 < out_rect.bottom)
			out_rect.bottom = region->rects[i].y2;
		if (region->rects[i].x2 < out_rect.right)
			out_rect.right = region->rects[i].x2;
	}

	return TRUE;
}

OP_STATUS UnixOpPluginAdapter::SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent)
{
	npwin->window = NULL;
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginAdapter::SetupNPPrint(NPPrint* np_print, const OpRect& rect)
{
	// Copied and pasted from the old UnixPluginAdapter
	np_print->print.embedPrint.window.ws_info = 0;
	return OpStatus::OK;
}

void UnixOpPluginAdapter::SaveState(OpPluginEventType event_type) { }
void UnixOpPluginAdapter::RestoreState(OpPluginEventType event_type) { }

OpChannel* UnixOpPluginAdapter::GetChannel()
{
	return m_channel;
}

void UnixOpPluginAdapter::GetKeyEventPlatformData(uni_char key, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64& data1, UINT64& data2)
{
	X11Client::Instance()->GetKeyEventPlatformData(key, key_state == PLUGIN_KEY_DOWN || key_state == PLUGIN_KEY_PRESSED, modifiers, data1, data2);
}

OP_STATUS UnixOpPluginAdapter::OnBrowserWindowInformationMessage(OpBrowserWindowInformationMessage* message)
{
	OP_ASSERT(message != NULL);

	OpAutoPtr<OpBrowserWindowInformationResponseMessage> response (OpBrowserWindowInformationResponseMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	X11Types::Display* display = X11Client::Instance()->GetDisplay();
	UniString display_string;
	RETURN_IF_ERROR(UniString_UTF8::FromUTF8(display_string, DisplayString(display)));

	response->SetDisplayName(display_string);
	X11Types::Window toplevel_window = X11Client::Instance()->GetTopLevelWindow(m_parent_window);
	response->SetScreenNumber(X11Client::Instance()->GetScreenNumber(toplevel_window));

	/* Syncing is critical here, since we cannot give an XID to the other process
	 * unless we know for sure that it really exists inside the X server. */
	XSync(display, False);
	response->SetWindow(toplevel_window);

	return message->Reply(response.release());
}

OP_STATUS UnixOpPluginAdapter::OnParentChanged(UINT64 parent_window_handle)
{
	m_plugin_window->Reparent();

	OpPluginParentChangedMessage* msg = OpPluginParentChangedMessage::Create(parent_window_handle);
	RETURN_OOM_IF_NULL(msg);
	return m_channel->SendMessage(msg);
}

OP_STATUS UnixOpPluginAdapter::OnPluginGtkPlugAddedMessage(OpPluginGtkPlugAddedMessage* message)
{
	m_plugin_window->OnPluginWindowAdded(message->GetPlug(), true);
	return message->Reply(OpPluginErrorMessage::Create());
}

OP_STATUS UnixOpPluginAdapter::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS status = OpStatus::ERR;

	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
		case OpPluginErrorMessage::Type:
			break;
		case OpPeerDisconnectedMessage::Type:
			m_channel = NULL;
			status = OpStatus::OK;
			break;
		case OpBrowserWindowInformationMessage::Type:
			status = OnBrowserWindowInformationMessage(OpBrowserWindowInformationMessage::Cast(message));
			break;
		case OpPluginGtkPlugAddedMessage::Type:
			status = OnPluginGtkPlugAddedMessage(OpPluginGtkPlugAddedMessage::Cast(message));
			break;
		default:
			OP_ASSERT(!"UnixOpPluginAdapter::ProcessMessage: Received unknown/unhandled message");
			break;
	}

	return status;
}

#endif // X11API && NS4P_UNIX_PLATFORM && NS4P_COMPONENT_PLUGINS
