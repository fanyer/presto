/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "platforms/mac/pi/plugins/MacOpPluginAdapter.h"
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "modules/ns4plugins/src/proxyoppluginwindow.h"
#include "platforms/mac/util/macutils.h"

#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"
#include "platforms/mac/src/generated/g_message_mac_messages.h"

extern bool gHandlingContextMenu;
//////////////////////////////////////////////////////////////////////

OP_STATUS OpNS4PluginAdapter::Create(OpNS4PluginAdapter** new_object, OpComponentType component_type)
{
	*new_object = OP_NEW(MacOpPluginAdapter, (
#ifdef NS4P_COMPONENT_PLUGINS
                                              component_type
#endif // NS4P_COMPONENT_PLUGINS
                                              ));
	return *new_object ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

BOOL OpNS4PluginAdapter::GetValueStatic(NPNVariable variable, void* value, NPError* result)
{
	return FALSE;
}

#ifdef NS4P_COMPONENT_PLUGINS

/* static */
bool OpNS4PluginAdapter::IsBlacklistedFilename(UniString plugin_filepath) { return false; }

MacOpPluginAdapter::MacOpPluginAdapter(OpComponentType component_type)
:
m_plugin_window(NULL)
, m_plugin_channel(NULL)
{
	// DO NOT use the NP_NO_QUICKDRAW AND NP_NO_CARBON defines anywhere on the Opera
	// side because they are useless. The plugin can still support these features even
	// if Opera doesn't. We need to have code paths in Opera that will support all
	// Plugin types
#ifdef SIXTY_FOUR_BIT
	// Set the default values based on the component type
	if (component_type == COMPONENT_PLUGIN)
	{
		m_drawing_model = NPDrawingModelCoreGraphics;
		m_event_model = NPEventModelCocoa;
	}
	else if (component_type == COMPONENT_PLUGIN_MAC_32)
	{
		m_drawing_model = NPDrawingModelQuickDraw;
		m_event_model = NPEventModelCarbon;
	}
#else
	m_drawing_model = NPDrawingModelQuickDraw;
	m_event_model = NPEventModelCarbon;
#endif // SIXTY_FOUR_BIT
	
}

MacOpPluginAdapter::~MacOpPluginAdapter()
{
	OP_DELETE(m_plugin_channel);
}


#else // NS4P_COMPONENT_PLUGINS


MacOpPluginAdapter::MacOpPluginAdapter()
:
m_plugin_window(NULL)
# ifdef NP_NO_QUICKDRAW
, m_drawing_model(NPDrawingModelCoreGraphics)
# else
, m_drawing_model(NPDrawingModelQuickDraw)
# endif
# ifdef NP_NO_CARBON
, m_event_model(NPEventModelCocoa)
# else
, m_event_model(NPEventModelCarbon)
# endif
, m_updated_text_input(false)
, m_transparent(false)
{
}

MacOpPluginAdapter::~MacOpPluginAdapter()
{
}

#endif // NS4P_COMPONENT_PLUGINS

void MacOpPluginAdapter::SetPluginWindow(OpPluginWindow* plugin_window)
{
#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW
	m_plugin_window = static_cast<MacOpPluginWindow *>(static_cast<ProxyOpPluginWindow *>(plugin_window)->GetInternalPluginWindow());
#else
	m_plugin_window = static_cast<MacOpPluginWindow *>(plugin_window);
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW
	
	if (m_plugin_window)
	{
		m_plugin_window->SetDrawingModel(m_drawing_model);
		m_plugin_window->SetEventModel(m_event_model);
		m_plugin_window->SetSupportsUpdatedCocoaTextInput(m_updated_text_input);
		m_plugin_window->SetTransparent(m_transparent);
#ifdef NS4P_COMPONENT_PLUGINS
		m_plugin_window->SetPluginChannel(m_plugin_channel);
#endif // NS4P_COMPONENT_PLUGINS
	}
}

BOOL MacOpPluginAdapter::GetValue(NPNVariable variable, void* value, NPError* result)
{
#ifdef NS4P_COMPONENT_PLUGINS
	// This is no longer used
	return FALSE;
#else // NS4P_COMPONENT_PLUGINS
    switch (variable)
	{
        case 1001: // NPNVcontentsScaleFactor
			*(static_cast<double *>(value)) = m_plugin_window ? m_plugin_window->GetBackingScaleFactor() : 1.0;
			break;
            
# ifndef NP_NO_QUICKDRAW
		case NPNVsupportsQuickDrawBool:
			*(static_cast<NPBool *>(value)) = FALSE;
			break;
# endif
        // Cocoa Event model
		case NPNVsupportsCocoaBool:
        
        // Main drawing mode CoreGraphics (old)
		case NPNVsupportsCoreGraphicsBool:
			*(static_cast<NPBool *>(value)) = TRUE;
			break;

        // Main drawing mode CoreAnimation (new)
		case NPNVsupportsCoreAnimationBool:
			*(static_cast<NPBool *>(value)) = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DisableCoreAnimation) ? FALSE : TRUE;
			break;

        // InvalidatingCoreAnimation was added for Out of Process Plugins
        // In Process Plugins do not need the Invalidating mode!
        case NPNVsupportsInvalidatingCoreAnimationBool:
			*(static_cast<NPBool *>(value)) = FALSE;
			break;

        // Special mode for CoreAnimation when we can composite them, which we can
		case NPNVsupportsCompositingCoreAnimationPluginsBool:
			*(static_cast<NPBool *>(value)) = TRUE;
			break;
            
		case NPNVsupportsUpdatedCocoaTextInputBool:
			// Not ideal, but if the plugin asks if we support NPNVsupportsUpdatedCocoaTextInputBool
			// the we'll send it events according to the updated specs
			m_updated_text_input = true;
			*(static_cast<NPBool *>(value)) = TRUE;
			break;
            
		default:
			return FALSE;
	}
    
	return TRUE;
#endif // NS4P_COMPONENT_PLUGINS
}

BOOL MacOpPluginAdapter::SetValue(NPPVariable variable, void* value, NPError* result)
{
	switch (variable)
	{
		case NPPVpluginDrawingModel:
		{
			INTPTR val = (INTPTR)value;
			if (
				val != NPDrawingModelQuickDraw &&
				val != NPDrawingModelCoreGraphics
				&& val != NPDrawingModelCoreAnimation
#ifdef NS4P_COMPONENT_PLUGINS
				&& val != NPDrawingModelInvalidatingCoreAnimation
#endif // NS4P_COMPONENT_PLUGINS
				)
			{
				*result = NPERR_GENERIC_ERROR;
			}
			else
			{
				*result = NPERR_NO_ERROR;
				SetDrawingModel((NPDrawingModel)val);
				return FALSE;	// Currently plugin module still needs to handle this
			}
		}
		break;
			
		case NPPVpluginEventModel:
		{
			INTPTR val = (INTPTR)value;
			if (
				val != NPEventModelCarbon &&
				val != NPEventModelCocoa)
			{
				*result = NPERR_GENERIC_ERROR;
			}
			else
			{
				*result = NPERR_NO_ERROR;
				SetEventModel((NPEventModel)val);
			}
		}
		break;
			
		case NPPVpluginTransparentBool:
		{
			SetTransparent((bool)value);
			return FALSE;	// Plugin module needs to handle this too
		}
		break;

		default:
			return FALSE;
	}
	return TRUE;
}

OP_STATUS MacOpPluginAdapter::SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent)
{ 
	return m_plugin_window ? m_plugin_window->SetupNPWindow(npwin, rect, paint_rect, view_rect, view_offset, show, transparent) : OpStatus::ERR;
}

OP_STATUS MacOpPluginAdapter::SetupNPPrint(NPPrint* npprint, const OpRect& rect) 
{ 
	return m_plugin_window ? m_plugin_window->SetupNPPrint(npprint, rect) : OpStatus::ERR; 
}

void MacOpPluginAdapter::SaveState(OpPluginEventType event_type) 
{ 
	if (m_plugin_window) 
		m_plugin_window->SaveState(event_type); 
}

void MacOpPluginAdapter::RestoreState(OpPluginEventType event_type) 
{ 
	if (m_plugin_window) 
		m_plugin_window->RestoreState(event_type); 
}

///////////////////////////////////////////////////
//
// Functions no longer in use
//
BOOL MacOpPluginAdapter::ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect)
{
#ifdef NS4P_COMPONENT_PLUGINS
	return FALSE;
#else // NS4P_COMPONENT_PLUGINS
#ifndef NP_NO_QUICKDRAW
	if (m_plugin_window->GetDrawingModel() == NPDrawingModelQuickDraw)
	{
		Rect r;
		GetRegionBounds((RgnHandle)invalidRegion, &r);
		invalidRect.left = r.left;
		invalidRect.top = r.top;
		invalidRect.bottom = r.bottom;
		invalidRect.right = r.right;
		return TRUE;
	}
#endif
	CGRect bounds = CGPathGetBoundingBox((CGPathRef)invalidRegion);
	invalidRect.left = bounds.origin.x;
	invalidRect.top = bounds.origin.y;
	invalidRect.right = bounds.origin.x+bounds.size.width;
	invalidRect.bottom = bounds.origin.y+bounds.size.height;
	return TRUE;
#endif // NS4P_COMPONENT_PLUGINS
}
//
///////////////////////////////////////////////////

#ifdef NS4P_COMPONENT_PLUGINS

OpChannel* MacOpPluginAdapter::GetChannel()
{
	if (!m_plugin_channel)
	{
		// FIXME: You might want to disable the plug-in if this OOMs.
		m_plugin_channel = g_opera->CreateChannel();
		m_plugin_channel->AddMessageListener(this);
	}
	
	return m_plugin_channel;
}

OP_STATUS MacOpPluginAdapter::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
			if (m_plugin_window)
				// Send initial top-level window coordinates.
				m_plugin_window->OnDesktopWindowMoved(NULL);
			break;
			
		case OpPeerDisconnectedMessage::Type:
			m_plugin_channel = NULL;
			break;
			
		case OpMacPluginUpdateViewMessage::Type:
			if (m_plugin_window)
				m_plugin_window->UpdatePluginView();
			break;
			
		case OpMacPluginFullscreenMessage::Type:
		{
			SystemUIMode outMode;
			SystemUIOptions  outOptions;
			GetSystemUIMode(&outMode, &outOptions);
			
			ProcessSerialNumber psn;
			
			if (OpMacPluginFullscreenMessage::Cast(message)->GetFullscreen())
			{
				// Hide menu and dock
				SetSystemUIMode(kUIModeAllHidden, outOptions);
				
				// Set the plugin as the front process
				psn.highLongOfPSN = OpMacPluginFullscreenMessage::Cast(message)->GetHighLongOfPSN();
				psn.lowLongOfPSN = OpMacPluginFullscreenMessage::Cast(message)->GetLowLongOfPSN();
				SetFrontProcess(&psn);
			}
			else
			{
				// Set Opera as the front process
				GetCurrentProcess(&psn);
				SetFrontProcess(&psn);
				
				// Show menu and dock
				SetSystemUIMode(kUIModeNormal, outOptions);
			}
		}
			break;
			
		case OpMacPluginWindowShownMessage::Type:
		{
			ProcessSerialNumber psn;
			
			if (OpMacPluginWindowShownMessage::Cast(message)->GetShow())
			{
				// Set the plugin as the front process
				psn.highLongOfPSN = OpMacPluginWindowShownMessage::Cast(message)->GetHighLongOfPSN();
				psn.lowLongOfPSN = OpMacPluginWindowShownMessage::Cast(message)->GetLowLongOfPSN();
				SetFrontProcess(&psn);
			}
			else
			{
				// Set Opera as the front process
				GetCurrentProcess(&psn);
				SetFrontProcess(&psn);
			}
		}
			break;
			
		case OpMacPluginInfoMessage::Type:
			return message->Reply(OpMacPluginInfoResponseMessage::Create(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DisableCoreAnimation) ? TRUE : FALSE,
																		 CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DisableInvalidatingCoreAnimation) ? TRUE : FALSE));

		case OpMacPluginCursorShownMessage::Type:
		{
			if (OpMacPluginCursorShownMessage::Cast(message)->GetShow())
			{
				CGDisplayShowCursor(CGMainDisplayID());
			}
			else
			{
				CGDisplayHideCursor(CGMainDisplayID());
			}
		}
			break;

		case OpMacPluginContextMenuShownMessage::Type:
		{
			gHandlingContextMenu = OpMacPluginContextMenuShownMessage::Cast(message)->GetShow();
		}
			break;
	}
	
	return OpStatus::OK;
}

#endif // NS4P_COMPONENT_PLUGINS

void MacOpPluginAdapter::SetDrawingModel(NPDrawingModel drawing_model) 
{ 
	m_drawing_model = drawing_model; 
	if (m_plugin_window)
		m_plugin_window->SetDrawingModel(m_drawing_model); 
}

void MacOpPluginAdapter::SetEventModel(NPEventModel event_model) 
{ 
	m_event_model = event_model; 
	if (m_plugin_window) 
		m_plugin_window->SetEventModel(m_event_model);
}

void MacOpPluginAdapter::SetSupportsUpdatedCocoaTextInput(bool updated_text_input) 
{ 
	m_updated_text_input = updated_text_input; 
	if (m_plugin_window) 
		m_plugin_window->SetSupportsUpdatedCocoaTextInput(m_updated_text_input);
}

void MacOpPluginAdapter::SetTransparent(bool transparent)
{
	m_transparent = transparent;
	if (m_plugin_window)
		m_plugin_window->SetTransparent(m_transparent);
}

#endif // _PLUGIN_SUPPORT_

