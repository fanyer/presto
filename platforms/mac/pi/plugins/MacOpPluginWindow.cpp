/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "platforms/mac/pi/MacOpView.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/libgogi/mde.h"

#include "modules/pi/OpNS4PluginAdapter.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/plugins/MacOpPluginTranslator.h"
#ifndef NP_NO_CARBON
#include "platforms/mac/pi/CarbonMacOpPlatformEvent.h"
#endif // NP_NO_CARBON
#include "platforms/mac/pi/CocoaMacOpPlatformEvent.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"
#include "platforms/mac/src/generated/g_message_mac_messages.h"

#include "platforms/mac/pi/plugins/MacOpPluginImage.h"
#include "platforms/mac/util/OpenGLContextWrapper.h"

#include <sys/shm.h>

#ifdef NO_CARBON
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#endif

#ifdef USE_PLUGIN_EVENT_API
#include "modules/doc/frm_doc.h"
#include "modules/display/vis_dev.h"
#endif

#ifdef _MAC_DEBUG
#include "platforms/mac/debug/OnScreen.h"
#endif

// Set to turn on all the printfs to debug the PluginWrapper
// #define MAC_PLUGIN_WRAPPER_DEBUG

////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpPluginWindow::Create(OpPluginWindow** new_object, const OpRect& rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window)
{
	*new_object = OP_NEW(MacOpPluginWindow, ());

	RETURN_OOM_IF_NULL(*new_object);

	OP_STATUS status = static_cast<MacOpPluginWindow*>(*new_object)->Construct(rect, scale, parent, windowless, op_window);

	if (OpStatus::IsError(status))
		OP_DELETE(*new_object);

	return status;
}

#ifdef NS4P_COMPONENT_PLUGINS

MacOpPluginWindow::MacOpPluginWindow()
:
  m_parent_view(NULL),
  m_listener(NULL),
  m_blocked_input(FALSE)
, m_plugin_layer(NULL)
, m_x(0)
, m_y(0)
, m_width(0)
, m_height(0)
, m_title_height(0)
, m_win_height(0)
, m_use_gworld(TRUE)
, m_was_suppressed(FALSE)
, m_drawing_model_inited(false)
// Setting this to the new defaults of drawing/event model, 
// isn't strictly correct but the real values will be correct
// values will be copied over from the adapter
, m_drawing_model(NPDrawingModelCoreGraphics)
, m_event_model(NPEventModelCocoa)
, m_surface(NULL)
, m_surface_id(0)
, m_plugin_channel(NULL)
, m_desktop_window(NULL)
, m_desktop_window_this_tab(NULL)
{
	m_hw_accel = (g_opera->libvega_module.vega3dDevice && g_opera->libvega_module.rasterBackend == LibvegaModule::BACKEND_HW3D);
}

MacOpPluginWindow::~MacOpPluginWindow()
{
	if (m_desktop_window)
	{
		m_desktop_window->RemoveListener(this);
		m_desktop_window = NULL;
	}
	
	if (m_desktop_window_this_tab)
	{
		m_desktop_window_this_tab->RemoveListener(this);
		m_desktop_window_this_tab = NULL;
	}

	// Kill any existing surface
	if (m_surface)
		CFRelease(m_surface);
}

OP_STATUS MacOpPluginWindow::Construct(const OpRect &rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window)
{
	m_parent_view = parent;

	// Register as a listener of the top-level browser window, so that we may broadcast window
    // window movement to the plug-in and it may implement NPN_ConvertPoint() translations
    // involving screen space. See OnDesktopWindowMoved().
	if (DesktopOpWindow* dow = static_cast<DesktopOpWindow*>(op_window->GetRootWindow()))
	{
		m_desktop_window = dow->GetDesktopWindow();
		if (m_desktop_window)
			RETURN_IF_ERROR(m_desktop_window->AddListener(this));
	}

	// Set the desktop window listener for the desktop window for just this tab so we can 
	// track the parent changes
	MDE_OpWindow* win = static_cast<MDE_OpWindow*>(op_window);
	if (win)
	{
		MDE_OpView* view = static_cast<MDE_OpView*>(win->GetParentView());
		if (view)
		{
			m_desktop_window_this_tab = static_cast<DesktopOpWindow*>(view->GetParentWindow())->GetDesktopWindow();
			if (m_desktop_window_this_tab)
				RETURN_IF_ERROR(m_desktop_window_this_tab->AddListener(this));
		}
	}
	
	return OpStatus::OK;
}

void MacOpPluginWindow::Show()
{
	if (UsesPluginNativeWindow())
		ShowCALayer(TRUE);

	SendOpMacPluginVisibilityMessage(TRUE);
}

void MacOpPluginWindow::Hide()
{
	if (UsesPluginNativeWindow())
		ShowCALayer(FALSE);

	SendOpMacPluginVisibilityMessage(FALSE);
}

void MacOpPluginWindow::SetPos(int x, int y)
{
	m_x = x;
	m_y = y;

	// Update the position of the plugin
	SetLayerFrame();

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	fprintf(stderr, "MacOpPluginWindow::SetPos x:%d, y:%d\n", x, y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
}

void MacOpPluginWindow::SetSize(int w, int h)
{
	m_width = w;
	m_height = h;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "MacOpPluginWindow::SetSize w:%d, h:%d\n", w, h);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
}

void* MacOpPluginWindow::GetHandle()
{ 
    if (GetDrawingModel() == NPDrawingModelCoreAnimation || GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
        return (void *)m_surface_id;
    return this; 
}

BOOL MacOpPluginWindow::UsesPluginNativeWindow() const 
{ 
	if (!m_drawing_model_inited) 
		return FALSE; 

	return m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation; 
}

void MacOpPluginWindow::SetPluginObject(Plugin *plugin)
{
	m_plugin = plugin;
}

void MacOpPluginWindow::SetClipRegion(MDE_Region* rgn)
{
	MDE_RECT union_rect;
	
	if (rgn->num_rects)
	{
		union_rect = rgn->rects[0];
		for (int i = 1; i < rgn->num_rects; i++)
			union_rect = MDE_RectUnion(union_rect, rgn->rects[i]);
	
		m_painter_clip_rect.x = union_rect.x;
		m_painter_clip_rect.y = union_rect.y;
		m_painter_clip_rect.width = union_rect.w;
		m_painter_clip_rect.height = union_rect.h;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "MacOpPluginWindow::SetClipRegion x:%d, y:%d, width:%d, height:%d\n", union_rect.x, union_rect.y, union_rect.w, union_rect.h);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	}

	ShowCALayer(!!rgn->num_rects);
}

///////////////////////////////////////////////////
//
// Functions no longer in use
//
void MacOpPluginWindow::SaveState(OpPluginEventType event)
{
}

void MacOpPluginWindow::RestoreState(OpPluginEventType event)
{
}

OP_STATUS MacOpPluginWindow::CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS MacOpPluginWindow::CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS MacOpPluginWindow::CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS MacOpPluginWindow::CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}
//
///////////////////////////////////////////////////


OP_STATUS MacOpPluginWindow::SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent)
{
	npwin->type = NPWindowTypeDrawable;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	printf("SetupNPWindow: rect x:%d, y:%d, width:%d, height:%d, show:%d\n", rect.x, rect.y, rect.width, rect.height, show);
	printf("SetupNPWindow: npwin x:%d, y:%d, width:%d, height:%d\n", npwin->x, npwin->y, npwin->width, npwin->height);
	printf("SetupNPWindow: npwin->clipRect x:%d, y:%d, width:%d, height:%d\n", npwin->clipRect.left, npwin->clipRect.top, npwin->clipRect.right - npwin->clipRect.left, npwin->clipRect.bottom - npwin->clipRect.top);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	
#ifndef VEGA_USE_HW
	m_painter_clip_rect.x = npwin->clipRect.left;
	m_painter_clip_rect.y = npwin->clipRect.top;
	m_painter_clip_rect.width = npwin->clipRect.right - npwin->clipRect.left;
	m_painter_clip_rect.height = npwin->clipRect.bottom - npwin->clipRect.top;
#endif // !VEGA_USE_HW
	
	if (GetDrawingModel() == NPDrawingModelCoreAnimation || GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
	{
		if (show)
		{
			// We must initialise the plugin co-ord and the painter clip rect otherwise
			// the first frame of the plugin won't be collected on the first paint and 
			// some of the shim tests will fail
			m_x = npwin->x; 
			m_y = npwin->y; 

			m_painter_clip_rect.x = npwin->clipRect.left;
			m_painter_clip_rect.y = npwin->clipRect.top;
			m_painter_clip_rect.width = npwin->clipRect.right - npwin->clipRect.left;
			m_painter_clip_rect.height = npwin->clipRect.bottom - npwin->clipRect.top;

			npwin->clipRect.left = npwin->clipRect.top = 0;
			npwin->clipRect.right = m_width;
			npwin->clipRect.bottom = m_height;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			printf("SetupNPWindow: m_x:%d, m_y:%d\n", m_x, m_y);
			printf("SetupNPWindow: m_painter_clip_rect x:%d, y:%d, width:%d, height:%d\n", m_painter_clip_rect.x, m_painter_clip_rect.y, m_painter_clip_rect.width, m_painter_clip_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
        }
		
		// CoreAnimation needs to (re)create the IOSurface each time the plugin size changes
		BOOL resized = FALSE;
		if (m_plugin_rect.width != rect.width || m_plugin_rect.height != rect.height)
			resized = TRUE;

		if (!m_surface_id || resized)
		{
			m_plugin_rect = rect;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			printf("SetupNPWindow: m_plugin_rect x:%d, y:%d, width:%d, height:%d\n", m_plugin_rect.x, m_plugin_rect.y, m_plugin_rect.width, m_plugin_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

			RETURN_IF_ERROR(CreateIOSurface(rect));
		}

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
        printf(" -> SetupNPWindow: surface id:%d\n", m_surface_id);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		// Transport the IOSurface ID via the window param
		npwin->window = (void *)m_surface_id;

		if (show)
		{
			if (!m_plugin_layer)
				CreateCALayer();

#ifndef VEGA_USE_HW
			UpdateCALayer();
#endif
		}

		return OpStatus::OK;
	}

	return OpStatus::OK;
}

OP_STATUS MacOpPluginWindow::SetupNPPrint(NPPrint* npprint, const OpRect& rect)
{
	return OpStatus::ERR;
}

BOOL MacOpPluginWindow::SendEvent(OpPlatformEvent* event)
{
	return FALSE;
}

void MacOpPluginWindow::Detach()
{
	if (UsesDirectPaint())
		DestroyCALayer();
	
	m_parent_view = NULL;
}

void MacOpPluginWindow::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (active || desktop_window != m_desktop_window_this_tab || !(desktop_window->GetParentView() || desktop_window->GetParentWindow()))
		return;

	Hide();
}

void MacOpPluginWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (m_desktop_window == desktop_window)
	{
		m_desktop_window->RemoveListener(this);
		m_desktop_window = NULL;
	}

	if (desktop_window == m_desktop_window_this_tab)
	{
		m_desktop_window_this_tab->RemoveListener(this);
		m_desktop_window_this_tab = NULL;
	}
}

void MacOpPluginWindow::OnDesktopWindowMoved(DesktopWindow* desktop_window)
{
	if (desktop_window == m_desktop_window_this_tab)
		return;

	SendOpMacPluginDesktopWindowMovementMessage();
}

void MacOpPluginWindow::OnDesktopWindowParentChanged(DesktopWindow* desktop_window)
{
	if (desktop_window != m_desktop_window_this_tab)
		return;

	// Destroy the CALayer that is attached to the old parent
	DestroyCALayer();
	// Recreate the CALayer on the new parent window
	CreateCALayer();
}

void MacOpPluginWindow::SetPluginChannel(OpChannel *channel) 
{ 
	m_plugin_channel = channel; 
	// Set the initial window placement right away!
	SendOpMacPluginDesktopWindowMovementMessage();
}

void MacOpPluginWindow::SendOpMacPluginDesktopWindowMovementMessage()
{
	if (m_desktop_window && m_plugin_channel->IsConnected())
	{
		INT32 x, y;
		// Need the inner pos so that the titlebar is taken into account
		m_desktop_window->GetInnerPos(x, y);
		m_plugin_channel->SendMessage(OpMacPluginDesktopWindowMovementMessage::Create(x, y));
	}
}

void MacOpPluginWindow::SendOpMacPluginVisibilityMessage(BOOL visible)
{
	if (m_plugin_channel && m_plugin_channel->IsConnected())
		m_plugin_channel->SendMessage(OpMacPluginVisibilityMessage::Create(visible));
}

#else // NS4P_COMPONENT_PLUGINS

/* Device-independent bits found in event modifier flags */
enum {
	NSAlphaShiftKeyMask =	1 << 16,
	NSShiftKeyMask =		1 << 17,
	NSControlKeyMask =		1 << 18,
	NSAlternateKeyMask =	1 << 19,
	NSCommandKeyMask =		1 << 20,
	NSNumericPadKeyMask =	1 << 21,
	NSHelpKeyMask =			1 << 22,
	NSFunctionKeyMask =		1 << 23,
	NSDeviceIndependentModifierFlagsMask = 0xffff0000U
};

extern UniChar OpKey2UniKey(uni_char c);

NSUInteger CocoaKeyModifiers(ShiftKeyState shiftState)
{
	NSUInteger mods = 0;
	if (shiftState & SHIFTKEY_CTRL)
		mods |= NSCommandKeyMask;
	
	if (shiftState & SHIFTKEY_SHIFT)
		mods |= NSShiftKeyMask;
	
	if (shiftState & SHIFTKEY_ALT)
		mods |= NSAlternateKeyMask;
	
	return mods;
}

EventModifiers ClassicKeyModifiers(ShiftKeyState shiftState)
{
	EventModifiers mods = 0;
	if (shiftState & SHIFTKEY_CTRL)
		mods |= cmdKey;
	
	if (shiftState & SHIFTKEY_SHIFT)
		mods |= shiftKey;
	
	if (shiftState & SHIFTKEY_ALT)
		mods |= optionKey;
	
	return mods;
}

uni_char OperaToClassicCharCode(uni_char uc)
{
	switch(uc)
	{
		case OP_KEY_HOME:
			return kHomeCharCode;
		case OP_KEY_END:
			return kEndCharCode;
		case OP_KEY_PAGEUP:
			return kPageUpCharCode;
		case OP_KEY_PAGEDOWN:
			return kPageDownCharCode;
		case OP_KEY_UP:
			return kUpArrowCharCode;
		case OP_KEY_DOWN:
			return kDownArrowCharCode;
		case OP_KEY_LEFT:
			return kLeftArrowCharCode;
		case OP_KEY_RIGHT:
			return kRightArrowCharCode;
		case OP_KEY_ESCAPE:
			return kEscapeCharCode;
		case OP_KEY_DELETE:
			return kDeleteCharCode;
		case OP_KEY_BACKSPACE:
			return kBackspaceCharCode;
		case OP_KEY_TAB:
			return kTabCharCode;
		case OP_KEY_SPACE:
			return kSpaceCharCode;
		case OP_KEY_ENTER:
			return kReturnCharCode;
		default:
			return uc;
	}
}

uni_char ClassicToOperaCharCode(uni_char uc)
{
	switch(uc)
	{
		case kHomeCharCode:
			return OP_KEY_HOME;
		case kEndCharCode:
			return OP_KEY_END;
		case kPageUpCharCode:
			return OP_KEY_PAGEUP;
		case kPageDownCharCode:
			return OP_KEY_PAGEDOWN;
		case kUpArrowCharCode:
			return OP_KEY_UP;
		case kDownArrowCharCode:
			return OP_KEY_DOWN;
		case kLeftArrowCharCode:
			return OP_KEY_LEFT;
		case kRightArrowCharCode:
			return OP_KEY_RIGHT;
		case kEscapeCharCode:
			return OP_KEY_ESCAPE;
		case kDeleteCharCode:
			return OP_KEY_DELETE;
		case kBackspaceCharCode:
			return OP_KEY_BACKSPACE;
		case kTabCharCode:
			return OP_KEY_TAB;
		case kSpaceCharCode:
			return OP_KEY_SPACE;
		case kReturnCharCode:
		case kEnterCharCode:
			return OP_KEY_ENTER;
		default:
			return uc;
	}
}

MacOpPluginWindow::MacOpPluginWindow()
:
m_parent_view(NULL),
m_listener(NULL),
m_blocked_input(FALSE)
, m_plugin(NULL)
#ifndef NP_NO_QUICKDRAW
, m_np_port(NULL)
#endif
#ifndef SIXTY_FOUR_BIT
, m_key_handler(NULL)
#endif
, m_np_context(NULL)
, m_plugin_layer(NULL)
, m_x(0)
, m_y(0)
, m_width(0)
, m_height(0)
, m_last_width(0)
, m_last_height(0)
, m_title_height(0)
, m_win_height(0)
, m_qd_port(NULL)
, m_use_gworld(TRUE)
, m_was_suppressed(FALSE)
//, m_is_visible(TRUE)
, m_last_window_focus_sent(TRUE)
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
, m_timer(NULL)
, m_updated_text_input(false)
, m_desktop_window(NULL)
, m_desktop_window_this_tab(NULL)
{
}

MacOpPluginWindow::~MacOpPluginWindow()
{
	if (m_desktop_window)
	{
		m_desktop_window->RemoveListener(this);
		m_desktop_window = NULL;
	}
	
	if (m_desktop_window_this_tab)
	{
		m_desktop_window_this_tab->RemoveListener(this);
		m_desktop_window_this_tab = NULL;
	}

	if (m_np_context && m_np_context->context)
		CocoaVEGAWindow::ReleasePainter(m_np_context->context, m_provider);
    
	OP_DELETE(m_np_context);
    
#ifndef NP_NO_QUICKDRAW
	if (m_np_port)
	{
		if (m_np_port->port)
			DisposeGWorld(m_np_port->port);
        
		op_free(m_np_port);
	}
#endif
    
	delete m_timer;
}

OP_STATUS MacOpPluginWindow::Construct(const OpRect &rect, int scale, OpView* parent, BOOL transparent, OpWindow* op_window)
{
	// Register as a listener of the top-level browser window, so that we may broadcast window
    // window movement to the plug-in and it may implement NPN_ConvertPoint() translations
    // involving screen space. See OnDesktopWindowMoved().
	if (DesktopOpWindow* dow = static_cast<DesktopOpWindow*>(op_window->GetRootWindow()))
	{
		m_desktop_window = dow->GetDesktopWindow();
		if (m_desktop_window)
			RETURN_IF_ERROR(m_desktop_window->AddListener(this));
	}
    
	// Set the desktop window listener for the desktop window for just this tab so we can
	// track the parent changes
	MDE_OpWindow* win = static_cast<MDE_OpWindow*>(op_window);
	if (win)
	{
		MDE_OpView* view = static_cast<MDE_OpView*>(win->GetParentView());
		if (view)
		{
			m_desktop_window_this_tab = static_cast<DesktopOpWindow*>(view->GetParentWindow())->GetDesktopWindow();
			if (m_desktop_window_this_tab)
				RETURN_IF_ERROR(m_desktop_window_this_tab->AddListener(this));
		}
	}

    //#warning "MacOpPluginWindow::Construct: transparent parameter ignored"
    // FIXME: MacOpPluginWindow::Construct: transparent parameter ignored
	m_timer = new OpTimer();
	if(!m_timer)
		return OpStatus::ERR_NO_MEMORY;
	m_timer->SetTimerListener(this);
	m_timer->Start(20);
	m_parent_view = parent;

	return OpStatus::OK;
}

void MacOpPluginWindow::Show()
{
	if (UsesPluginNativeWindow())
		ShowCALayer(TRUE);
}

void MacOpPluginWindow::Hide()
{
	if (UsesPluginNativeWindow())
		ShowCALayer(FALSE);
}

void MacOpPluginWindow::SetPos(int x, int y)
{
	m_x = x;
	m_y = y;
    
	// Update the position of the plugin
	SetLayerFrame();
    
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "MacOpPluginWindow::SetPos x:%d, y:%d\n", x, y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
}

void MacOpPluginWindow::SetSize(int w, int h)
{
    if (w != m_width || h != m_height)
    {
        m_width = w;
        m_height = h;

        // Update the position of the plugin
        SetLayerFrame();
        SetPluginLayerFrame();
    }
    
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "MacOpPluginWindow::SetSize w:%d, h:%d\n", w, h);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
}

#ifndef NP_NO_QUICKDRAW
// Caller is responsible for deleting object
NP_Port *MacOpPluginWindow::GetNPPort()
{
	NP_Port *np_port = (NP_Port *) op_calloc(sizeof(NP_Port), 1);
	if(np_port)
	{
		np_port->portx = 0;
		np_port->porty = 0;
		np_port->port = NULL;
	}
	return np_port;
}
#endif

void* MacOpPluginWindow::GetHandle()
{
    return this;
}

BOOL MacOpPluginWindow::UsesPluginNativeWindow() const
{
	if (m_hw_accel)
		return TRUE;

	if (!m_drawing_model_inited)
		return FALSE;
    
	return m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation;
}

void MacOpPluginWindow::SetPluginObject(Plugin *plugin)
{
	m_plugin = plugin;
	const uni_char* name = m_plugin->GetName();
    
	if (name && (uni_strstr(name, UNI_L("RealPlayer")) || uni_strstr(name, UNI_L("Flash"))))
		m_use_gworld = FALSE;
    
	if (plugin)
		plugin->SetWindowless(TRUE);
}

void MacOpPluginWindow::SaveState(OpPluginEventType event)
{
	if (event == PLUGIN_PAINT_EVENT && m_np_context && m_np_context->context)
	{
		CGContextSaveGState(m_np_context->context);
        CGFloat scale = 1.0f;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
        PixelScaler scaler(GetPixelScalerScale());
        scale = CGFloat(scaler.GetScale()) / 100.0f;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		CGContextScaleCTM(m_np_context->context, scale, -scale);
		CocoaOpWindow* win = (CocoaOpWindow*)m_parent_view->GetRootWindow();
		if(win)
		{
			CocoaVEGAWindow* vw = win->GetCocoaVEGAWindow();
			if(vw)
			{
				INT32 h = m_win_height = CGImageGetHeight(vw->getImage());
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
                h = scaler.FromDevicePixel(m_win_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				CGContextTranslateCTM(m_np_context->context, 0.0, -h);
			}
		}
		CGContextTranslateCTM(m_np_context->context, m_x, m_y);
        
		OpPoint pt(0,0);
		pt = m_parent_view->ConvertToScreen(pt);
		OpWindow* root = m_parent_view->GetRootWindow();
		INT32 xpos,ypos;
		root->GetInnerPos(&xpos, &ypos);
        
		INT32 new_x = m_painter_clip_rect.x - m_x + (pt.x - xpos);
		INT32 new_y = m_painter_clip_rect.y - m_y + (pt.y - ypos);
        
		CGRect painter_clip = {{new_x, new_y}, {m_painter_clip_rect.width, m_painter_clip_rect.height}};
		CGContextClipToRect(m_np_context->context, painter_clip);
#ifndef NP_NO_QUICKDRAW
		if (m_np_context->window)
			SetPortWindowPort((WindowRef) m_np_context->window);
#endif
	}
#ifndef NP_NO_QUICKDRAW
	else if(m_np_port && m_np_port->port)
	{
		if(m_use_gworld)
		{
			if (event == PLUGIN_PAINT_EVENT)
			{
				SetGWorld(m_np_port->port, NULL);
			}
			else if(event == PLUGIN_MOUSE_DOWN_EVENT || event == PLUGIN_MOUSE_UP_EVENT || event == PLUGIN_MOUSE_MOVE_EVENT)
			{
				m_use_gworld = false;
				NPWindow *npwin =  m_plugin->GetNPWindow();
                OpPoint pt(0,0);
				SetupNPWindow(npwin, m_last_rect, m_last_rect, m_last_rect, pt, TRUE, FALSE);
				m_plugin->GetPluginfuncs()->setwindow(m_plugin->GetInstance(), m_plugin->GetNPWindow());
				m_use_gworld = true;
			}
		}
		else
		{
			if (m_plugin && m_plugin->GetLifeCycleState() != Plugin::FAILED_OR_TERMINATED)
			{
				CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
				NPWindow *npwin = m_plugin->GetNPWindow();
				if (npwin)
				{
					Rect old_clip = {npwin->clipRect.top, npwin->clipRect.left, npwin->clipRect.bottom, npwin->clipRect.right};
					Rect new_clip;
					BOOL suppressed;
					RgnHandle crgn = NewRgn();
					if (w)
					{
						WindowRef win = w->GetCarbonWindow();
						if (win)
						{
							SetPortWindowPort(win);
							// Bad me.
							MDE_Region &rgn = ((MDE_OpView*)m_parent_view)->GetMDEWidget()->m_region;
							RgnHandle crgn2 = NewRgn();
							for (int i = 0; i < rgn.num_rects; i++)
							{
								SetRectRgn(crgn2, rgn.rects[i].x,rgn.rects[i].y,rgn.rects[i].x+rgn.rects[i].w,rgn.rects[i].y+rgn.rects[i].h);
								UnionRgn(crgn2, crgn, crgn);
							}
							if (!EmptyRgn(crgn))
							{
								SetRectRgn(crgn2, m_x,m_y,m_x+m_width,m_y+m_height);
								SectRgn(crgn2, crgn, crgn);
							}
							GetRegionBounds(crgn, &new_clip);
							suppressed = EmptyRect(&new_clip);
							DisposeRgn(crgn2);
							if (!EqualRect(&old_clip, &new_clip) || suppressed != m_was_suppressed)
							{
								npwin->clipRect.top = new_clip.top;
								npwin->clipRect.left = new_clip.left;
								npwin->clipRect.bottom = new_clip.bottom;
								npwin->clipRect.right = new_clip.right;
								m_was_suppressed = suppressed;
								m_plugin->GetPluginfuncs()->setwindow(m_plugin->GetInstance(), m_plugin->GetNPWindow());
							}
						}
						DisposeRgn(crgn);
					}
				}
			}
			SetPort(m_np_port->port);
		}
	}
#endif // NP_NO_QUICKDRAW
}

void MacOpPluginWindow::RestoreState(OpPluginEventType event)
{
	RESET_OPENGL_CONTEXT

	if (event == PLUGIN_PAINT_EVENT && m_np_context && m_np_context->context)
	{
		CGContextRestoreGState(m_np_context->context);
		CGContextFlush(m_np_context->context);
	}
#ifndef NP_NO_QUICKDRAW
	else if(m_np_port && m_np_port->port && m_use_gworld)
	{
		if(event == PLUGIN_PAINT_EVENT)
		{
			CGRect clip = {{0, 0}, {m_width, m_height}};
            
			UINT32 w, h;
			m_parent_view->GetSize(&w, &h);
			CGRect clip2 = {{-m_plugin->GetWindowX(), -m_plugin->GetWindowY()}, {w, h}};
			clip = CGRectIntersection(clip, clip2);
            
			Rect window_src = {clip.origin.y, clip.origin.x, clip.origin.y + clip.size.height, clip.origin.x + clip.size.width};
			Rect window_dest = window_src;
			OffsetRect(&window_dest, m_x, m_y);
            
			PixMapHandle plugin_pixmap = GetGWorldPixMap(m_np_port->port);
			if (LockPixels(plugin_pixmap))
			{
				CocoaOpWindow* win = (CocoaOpWindow*)m_parent_view->GetRootWindow();
                
				CocoaVEGAWindow* vw = win->GetCocoaVEGAWindow();
                
				PixMapHandle shared_pixmap = GetGWorldPixMap(vw->getGWorld());
				if (LockPixels(shared_pixmap))
				{
					SetGWorld(vw->getGWorld(), NULL);
					CopyBits(reinterpret_cast<BitMap*>(*plugin_pixmap), reinterpret_cast<BitMap*>(*shared_pixmap), &window_src, &window_dest, srcCopy, NULL);
					UnlockPixels(shared_pixmap);
				}
				UnlockPixels(plugin_pixmap);
			}
		}
		else if(event == PLUGIN_MOUSE_DOWN_EVENT || event == PLUGIN_MOUSE_UP_EVENT || event == PLUGIN_MOUSE_MOVE_EVENT)
		{
			NPWindow *npwin =  m_plugin->GetNPWindow();
            OpPoint pt(0,0);
            SetupNPWindow(npwin, m_last_rect, m_last_rect, m_last_rect, pt, TRUE, FALSE);
			m_plugin->GetPluginfuncs()->setwindow(m_plugin->GetInstance(), m_plugin->GetNPWindow());
		}
	}
#endif // NP_NO_QUICKDRAW
}

extern uni_char g_sent_char;
extern unsigned short g_key_code;
extern CGFloat g_deltax;
extern CGFloat g_deltay;

OP_STATUS MacOpPluginWindow::CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers)
{
	if (GetEventModel() == NPEventModelCocoa)
	{
		switch(event_type)
		{
			case PLUGIN_MOUSE_DOWN_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseDown();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_UP_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseUp();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_ENTER_EVENT:
			case PLUGIN_MOUSE_MOVE_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseHover();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_LEAVE_EVENT:
				gMouseCursor->ReapplyCursor();
				break;
			default:
				if(IsBlocked())
					return OpStatus::ERR_NO_ACCESS;
				break;
		}
        
		Point location;
		location.h = point.x - m_plugin->GetWindowX();
		location.v = point.y - m_plugin->GetWindowY();
        
		NSUInteger mod = CocoaKeyModifiers(modifiers);
        
		if (button == 1)
			mod |= NSControlKeyMask;
        
		NPCocoaEventType what;
		CGFloat deltax = 0.0, deltay = 0.0;
        
		switch(event_type)
		{
			case PLUGIN_MOUSE_DOWN_EVENT:
			{
				what = NPCocoaEventMouseDown;
				if(button == 1)
				{
					g_input_manager->ResetInput();
					CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
					if(w)
					{
						w->GetScreen()->ReleaseMouseCapture();
					}
					// Plugin may eat the up event
					w->ResetRightMouseCount();
				}
				break;
			}
			case PLUGIN_MOUSE_UP_EVENT:
			{
				what = NPCocoaEventMouseUp;
				break;
			}
			case PLUGIN_MOUSE_MOVE_EVENT:
			{
				if (GetPressedMouseButtons())
					what = NPCocoaEventMouseDragged;
				else
					what = NPCocoaEventMouseMoved;
				break;
			}
			case PLUGIN_MOUSE_ENTER_EVENT:
			{
				what = NPCocoaEventMouseEntered;
				break;
			}
			case PLUGIN_MOUSE_LEAVE_EVENT:
			{
				what = NPCocoaEventMouseExited;
				break;
			}
			case PLUGIN_MOUSE_WHEELV_EVENT:
			{
				what = NPCocoaEventScrollWheel;
				deltay = -g_deltay;
				break;
			}
			case PLUGIN_MOUSE_WHEELH_EVENT:
			{
				what = NPCocoaEventScrollWheel;
				deltax = g_deltax;
				break;
			}
			default:
				return OpStatus::ERR_NOT_SUPPORTED;
		}
        
		MacOpPlatformEvent<NPCocoaEvent> *evt = OP_NEW(MacOpPlatformEvent<NPCocoaEvent>, ());
		RETURN_OOM_IF_NULL(evt);
        
		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
        
		event_record->type = what;
		event_record->version = 0;
		event_record->data.mouse.pluginX = location.h;
		event_record->data.mouse.pluginY = location.v;
		event_record->data.mouse.buttonNumber = button;
		event_record->data.mouse.modifierFlags = mod;
		event_record->data.mouse.clickCount = 1;
		event_record->data.mouse.deltaX = deltax;
		event_record->data.mouse.deltaY = deltay;
		event_record->data.mouse.deltaZ = 0;
		evt->m_events.Add(event_record);
		*event = evt;
        
		return OpStatus::OK;
        
	} else {
        
#ifndef NP_NO_CARBON
        
		switch(event_type)
		{
			case PLUGIN_MOUSE_DOWN_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseDown();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_UP_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseUp();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_ENTER_EVENT:
			case PLUGIN_MOUSE_MOVE_EVENT:
			{
				if(IsBlocked() && GetListener())
				{
					GetListener()->OnMouseHover();
					return OpStatus::ERR_NO_ACCESS;
				}
				break;
			}
			case PLUGIN_MOUSE_LEAVE_EVENT:
				break;
		}
        
		Point location;
		OpPoint p;
		p = m_plugin->GetDocument()->GetVisualDevice()->view->ConvertToScreen(point);
		location.h = p.x;
		location.v = p.y;
		EventModifiers mod = ClassicKeyModifiers(modifiers);
        
		if (button == 1)
			mod |= controlKey;
        
		UInt16 what;
        
		switch(event_type)
		{
			case PLUGIN_MOUSE_DOWN_EVENT:
			{
				what = mouseDown;
				if(button == 1)
				{
					g_input_manager->ResetInput();
					CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
					if(w)
					{
						w->GetScreen()->ReleaseMouseCapture();
					}
					// Plugin may eat the up event
					w->ResetRightMouseCount();
				}
				break;
			}
			case PLUGIN_MOUSE_UP_EVENT:
			{
				what = mouseUp;
				break;
			}
			case PLUGIN_MOUSE_MOVE_EVENT:
			{
				what = NPEventType_AdjustCursorEvent;
				break;
			}
			default:
				return OpStatus::ERR_NOT_SUPPORTED;
		}
        
		MacOpPlatformEvent<EventRecord> *evt = OP_NEW(MacOpPlatformEvent<EventRecord>, ());
		RETURN_OOM_IF_NULL(evt);
        
		EventRecord* event_record = OP_NEW(EventRecord, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->what = what;
		event_record->message = 0;
		event_record->where = location;
#if defined(_MAC_DEBUG) && 0
		switch(what) {
			case mouseDown:
				DecOnScreen(0, 8, kOnScreenGreen, kOnScreenBlack, location.h);
				DecOnScreen(48, 8, kOnScreenGreen, kOnScreenBlack, location.v);
				break;
			case mouseUp:
				DecOnScreen(0, 16, kOnScreenRed, kOnScreenBlack, location.h);
				DecOnScreen(48, 16, kOnScreenRed, kOnScreenBlack, location.v);
				break;
			default:
				DecOnScreen(0, 0, kOnScreenWhite, kOnScreenBlack, location.h);
				DecOnScreen(48, 0, kOnScreenWhite, kOnScreenBlack, location.v);
				break;
		}
#endif
		event_record->where.v -= m_title_height; //HACK!
		event_record->modifiers = mod;
		event_record->when = TickCount();
		evt->m_events.Add(event_record);
		*event = evt;
		return OpStatus::OK;
        
#else
        
		return OpStatus::ERR;
        
#endif
	}
}

OP_STATUS MacOpPluginWindow::CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect)
{
	if (GetEventModel() == NPEventModelCocoa)
	{
		if (UsesDirectPaint())
		{
			return OpStatus::ERR_NOT_SUPPORTED;
		}
		else
		{
			MacOpPlatformEvent<NPCocoaEvent> *evt = OP_NEW(MacOpPlatformEvent<NPCocoaEvent>, ());
			RETURN_OOM_IF_NULL(evt);
            
			NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
			if (!event_record)
			{
				OP_DELETE(evt);
				return OpStatus::ERR_NO_MEMORY;
			}
			event_record->type = NPCocoaEventDrawRect;
			event_record->version = 0;
            
			if(m_np_context)
			{
				event_record->data.draw.context = m_np_context->context;
				painter->GetClipRect(&m_painter_clip_rect);
			}
			else
			{
				event_record->data.draw.context = 0;
			}
            
			event_record->data.draw.x = rect.x;
			event_record->data.draw.y = rect.y;
			event_record->data.draw.width = rect.width;
			event_record->data.draw.height = rect.height;
			evt->m_events.Add(event_record);
			*event = evt;
			return OpStatus::OK;
		}
        
	} else {
        
#ifndef NP_NO_CARBON
        
		MacOpPlatformEvent<EventRecord> *evt = OP_NEW(MacOpPlatformEvent<EventRecord>, ());
		RETURN_OOM_IF_NULL(evt);
        
		Point mouse;
		GetGlobalMouse(&mouse);
		EventRecord* event_record = OP_NEW(EventRecord, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->what = updateEvt;
        
		if(m_np_context)
		{
			event_record->message = (unsigned long)m_np_context->context;
			painter->GetClipRect(&m_painter_clip_rect);
		}
		else
		{
			CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
			if(w)
			{
				//event->message = (unsigned long)w->GetCarbonWindow();
				event_record->message = 0;
			}
		}
        
		event_record->when = TickCount();
		event_record->where = mouse;
		event_record->where.v -= m_title_height; //HACK!
		event_record->modifiers = 0;
		if(m_parent_view->GetRootWindow()->IsActiveTopmostWindow())
			event_record->modifiers |= activeFlag;
		evt->m_events.Add(event_record);
		*event = evt;
		return OpStatus::OK;
        
#else
        
		return OpStatus::ERR;
        
#endif
        
	}
}

OP_STATUS MacOpPluginWindow::SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	printf("SetupNPWindow: rect x:%d, y:%d, width:%d, height:%d, show:%d\n", rect.x, rect.y, rect.width, rect.height, show);
	printf("SetupNPWindow: npwin x:%d, y:%d, width:%d, height:%d\n", npwin->x, npwin->y, npwin->width, npwin->height);
	printf("SetupNPWindow: npwin->clipRect x:%d, y:%d, width:%d, height:%d\n", npwin->clipRect.left, npwin->clipRect.top, npwin->clipRect.right - npwin->clipRect.left, npwin->clipRect.bottom - npwin->clipRect.top);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

#ifndef NP_NO_QUICKDRAW
	if (GetDrawingModel() == NPDrawingModelQuickDraw)
	{
		CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
        
		if(!m_np_port)
		{
			m_np_port = GetNPPort();
		}
		npwin->window = m_np_port;
        
		if(m_use_gworld)
		{
			npwin->x = 0;
			npwin->y = 0;
			m_np_port->portx = 0;
			m_np_port->porty = 0;
			m_np_port->port = NULL;
		}
		else
		{
			npwin->x = m_x;
			npwin->y = m_y;
			m_np_port->portx = -m_x;
			m_np_port->porty = -m_y;
			m_np_port->port = GetWindowPort(w->GetCarbonWindow());
		}
        
		if (m_use_gworld)
		{
			if (m_qd_port && m_last_rect.width == rect.width && m_last_rect.height == rect.height)
			{
				m_np_port->port = m_qd_port;
				return OpStatus::OK;
			}
            
			m_last_rect = rect;
            
			Rect win_bounds = {0, 0, m_height, m_width};
			NewGWorld(&m_qd_port, k32ARGBPixelFormat, &win_bounds, NULL, NULL, kNativeEndianPixMap);
            
			if (m_np_port->port)
				DisposeGWorld(m_np_port->port);
            
			m_np_port->port = m_qd_port;
		}
        
		return OpStatus::OK;
	}
#endif // NP_NO_QUICKDRAW

	m_x = npwin->x;
	m_y = npwin->y;

	m_painter_clip_rect.x = npwin->clipRect.left;
	m_painter_clip_rect.y = npwin->clipRect.top;
	m_painter_clip_rect.width = npwin->clipRect.right - npwin->clipRect.left;
	m_painter_clip_rect.height = npwin->clipRect.bottom - npwin->clipRect.top;

	if (GetDrawingModel() == NPDrawingModelCoreAnimation || GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
	{
		if (!m_plugin_layer)
		{
			void* value = NULL;
			NPError ret = m_plugin->GetPluginfuncs()->getvalue(m_plugin->GetInstance(), NPPVpluginCoreAnimationLayer, &value);
			if (value && ret == NPERR_NO_ERROR)
			{
				m_plugin_layer = (MacOpPluginLayer *)value;
                CreateCALayer();
			}
		}

		npwin->clipRect.left = npwin->clipRect.top = 0;
		npwin->clipRect.right = m_width;
		npwin->clipRect.bottom = m_height;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		printf("SetupNPWindow: m_x:%d, m_y:%d\n", m_x, m_y);
		printf("SetupNPWindow: m_painter_clip_rect x:%d, y:%d, width:%d, height:%d\n", m_painter_clip_rect.x, m_painter_clip_rect.y, m_painter_clip_rect.width, m_painter_clip_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		SetLayerFrame();

		return OpStatus::OK;
	}
    
	if (!m_np_context)
	{
		m_np_context = OP_NEW(NP_CGContext, ());
		RETURN_OOM_IF_NULL(m_np_context);
        
		m_np_context->context = NULL;
		m_np_context->window = NULL;
	}
    
	CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
	BOOL found = false;
	if (w)
	{
		INT32 dx = 0, dy = 0;
#ifndef NP_NO_QUICKDRAW
		if (GetEventModel() == NPEventModelCarbon)
			m_np_context->window = w->GetCarbonWindow();
#endif
		// HACK!
		// The carbon window spans the entire structure of the cocoa window.
		// Flash seems to expect that you can set the origin and call GlobalToLocal
		// but since the content rect of the carbon window is different from the
		// content rect of the cocoa window, this won't work
		// also see: http://gitorious.org/qtwebkit/qtwebkit/blobs/28142d89c5d30a8c0801446661c7f853ec6bf9b0/WebKit/mac/Plugins/WebNetscapePluginView.mm
        
		w->GetContentPos(&dx, &dy);
		m_title_height = dy;
		CocoaVEGAWindow* vw = w->GetCocoaVEGAWindow();
		if (vw)
		{
			CGDataProviderRef provider = vw->getProvider();
			if (provider && provider != m_provider)
			{
				if (m_np_context->context)
					CocoaVEGAWindow::ReleasePainter(m_np_context->context, m_provider);
				vw->CreatePainter(m_np_context->context, m_provider);
			}
			found = TRUE;
		}
	}
	if (!found && m_np_context->context)
	{
		OP_ASSERT(!"Missing plugin context!");
		CGContextRelease(m_np_context->context);
		m_np_context->context = NULL;
	}
    
	if (GetDrawingModel() == NPDrawingModelCoreGraphics)
	{
		npwin->window = m_np_context;
		if (show && !m_plugin_layer)
			CreateCALayer();
	}
    
	return OpStatus::OK;
}

OP_STATUS MacOpPluginWindow::SetupNPPrint(NPPrint* npprint, const OpRect& rect)
{
	return OpStatus::ERR;
}

BOOL MacOpPluginWindow::SendEvent(OpPlatformEvent* event)
{
	return FALSE;
}

void MacOpPluginWindow::Detach()
{
	if (m_timer)
		m_timer->Stop();

	if (UsesDirectPaint())
		DestroyCALayer();

	m_parent_view = NULL;
}

void MacOpPluginWindow::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (active || desktop_window != m_desktop_window_this_tab || !(desktop_window->GetParentView() || desktop_window->GetParentWindow()))
		return;
    
	Hide();
}

void MacOpPluginWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (m_desktop_window == desktop_window)
	{
		m_desktop_window->RemoveListener(this);
		m_desktop_window = NULL;
	}

	if (desktop_window == m_desktop_window_this_tab)
	{
		m_desktop_window_this_tab->RemoveListener(this);
		m_desktop_window_this_tab = NULL;
	}
}

void MacOpPluginWindow::OnDesktopWindowMoved(DesktopWindow* desktop_window)
{
	if (desktop_window == m_desktop_window_this_tab)
		return;
}

void MacOpPluginWindow::OnDesktopWindowParentChanged(DesktopWindow* desktop_window)
{
	if (desktop_window != m_desktop_window_this_tab)
		return;
    
	// Destroy the CALayer that is attached to the old parent
	DestroyCALayer();
	// Recreate the CALayer on the new parent window
	CreateCALayer();
}

BOOL MacOpPluginWindow::ShouldPluginBeVisible()
{
	if (m_parent_view)
	{
		MDE_Widget *w = ((MDE_OpView*)m_parent_view)->GetMDEWidget();
		if (w)
			return !!w->m_region.num_rects;
	}
    
	return FALSE;
}

void MacOpPluginWindow::OnTimeOut(OpTimer* timer)
{
	if (GetEventModel() == NPEventModelCocoa)
	{
		if (!ShouldPluginBeVisible())
		{
			if (m_last_window_focus_sent)
			{
				m_last_window_focus_sent = FALSE;
				NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
				if (event_record)
				{
					event_record->type = NPCocoaEventWindowFocusChanged;
					event_record->version = 0;
					event_record->data.focus.hasFocus = FALSE;
					MacOpPlatformEvent<NPCocoaEvent> evt;
					evt.m_events.Add(event_record);
					m_plugin->HandleEvent(&evt, PLUGIN_FOCUS_EVENT);
				}
			}
		}
		else
		{
			if (!m_last_window_focus_sent)
			{
				m_last_window_focus_sent = TRUE;
				NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
				if (event_record)
				{
					event_record->type = NPCocoaEventWindowFocusChanged;
					event_record->version = 0;
					event_record->data.focus.hasFocus = TRUE;
					MacOpPlatformEvent<NPCocoaEvent> evt;
					evt.m_events.Add(event_record);
					m_plugin->HandleEvent(&evt, PLUGIN_FOCUS_EVENT);
				}
			}
		}
		timer->Start(100);
	}
	else
	{
# ifndef NP_NO_CARBON
		EventRecord *event = new EventRecord;
		event->what = nullEvent;
		event->message = 0;
#  ifdef NO_CARBON
		event->modifiers = 0;
		CGEventFlags mods = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
		if (mods & kCGEventFlagMaskShift)
			event->modifiers |= shiftKey;
		if (mods & kCGEventFlagMaskAlternate)
			event->modifiers |= optionKey;
		if (mods & kCGEventFlagMaskCommand)
			event->modifiers |= cmdKey;
#   else
		event->modifiers = GetCurrentKeyModifiers();
#   endif
		GetGlobalMouse(&event->where);
		event->when = TickCount();
        
		event->where.v -= m_title_height; //HACK!
        
		if(m_np_port && m_np_port->port && m_use_gworld/* && QDIsPortBufferDirty(m_np_port->port)*/)
		{
			INT32 x,y;
            
			OpPoint pt(0,0);
			pt = m_parent_view->ConvertToScreen(pt);
			OpWindow* root = m_parent_view->GetRootWindow();
			INT32 xpos,ypos;
			root->GetInnerPos(&xpos, &ypos);
            
			x = m_x - (pt.x - xpos);
			y = m_y - (pt.y - ypos);
            
			m_parent_view->Invalidate(OpRect(x,y,m_width,m_height));
		}
        
		MacOpPlatformEvent<EventRecord> evt;
		evt.m_events.Add(event);
		if(m_plugin)
		{
			timer->Start(16);
			m_plugin->HandleEvent(&evt, PLUGIN_OTHER_EVENT, FALSE);
			// Dont reference members after HandleEvent, 'this' might be deleted.
		}
# endif
	}
}

#ifndef NP_NO_CARBON

OpKey::Code CarbonUniKey2OpKey(UniChar c);

OSStatus MacOpPluginWindow::CocoaCarbonIMEEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *inUserData)
{
	ByteCount bytes;
	GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, NULL, &bytes, NULL);
	int chars = (bytes+1)/2;
	UInt32 keycode;
	uni_char* data = new uni_char[chars+1];
	GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, bytes, NULL, data);
	EventRef macEvent = NULL;
	GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, sizeof(macEvent), NULL, &macEvent);
	GetEventParameter(macEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(keycode), NULL, &keycode);
	for (int i=0; i<chars; i++)
	{
		g_sent_char = ClassicToOperaCharCode(data[i]);
		g_key_code = keycode;
		OpString8 utf8key;
		if (data[i] >= 32 || data[i] == 3 || data[i] == 13)
			utf8key.SetUTF8FromUTF16(&data[i], 1);

		g_input_manager->InvokeKeyPressed(CarbonUniKey2OpKey((UniChar)data[i]), utf8key.CStr(), utf8key.Length(), SHIFTKEY_NONE, FALSE, TRUE);
	}
	return noErr;
}

ScriptLanguageRecord MacOpPluginWindow::s_slRec;
OSStatus MacOpPluginWindow::KeyEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *inUserData)
{
	ByteCount bytes;
	GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, NULL, &bytes, NULL);
	int chars = (bytes+1)/2;
	uni_char* data = new uni_char[chars+1];
	GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, bytes, NULL, data);
	GetEventParameter(inEvent, kEventParamTextInputSendSLRec, typeIntlWritingCode, NULL, sizeof(s_slRec), NULL, &s_slRec);
    
	for (int i=0; i<chars; i++)
	{
		uni_char key = ClassicToOperaCharCode(data[i]);
		g_input_manager->InvokeKeyPressed((OpVirtualKey)key, SHIFTKEY_NONE);
	}
	return noErr;
}
#endif


#endif // NS4P_COMPONENT_PLUGINS

#endif // _PLUGIN_SUPPORT_
