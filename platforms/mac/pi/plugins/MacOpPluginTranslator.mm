/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "platforms/mac/pi/plugins/MacOpPluginTranslator.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "platforms/mac/pi/plugins/MacOpPluginImage.h"
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "platforms/mac/pi/CocoaMacOpPlatformEvent.h"
#ifndef NP_NO_CARBON
#include "platforms/mac/pi/CarbonMacOpPlatformEvent.h"
#endif // NP_NO_CARBON
#include <sys/shm.h>
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/browser_functions.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/UKeyTranslate.h"
#include "platforms/mac/util/macutils_mincore.h"

#ifndef NP_NO_QUICKDRAW
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#endif // NP_NO_QUICKDRAW

#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"

#define BOOL NSBOOL
#import <AppKit/NSMenu.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSColor.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CARenderer.h>
#import <QuartzCore/CATransaction.h>
#import <OpenGL/CGLMacro.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSOpenGLView.h>
#undef BOOL

// Set to turn on all the printfs to debug the PluginWrapper
//#define MAC_PLUGIN_WRAPPER_DEBUG

// This is needed for QuickTime using QuickDraw. Once 10.5 is gone
// get rid of this hack
#define MAC_PLUGIN_QUICKTIME_QUICKDRAW_HACK

#if defined(_PLUGIN_SUPPORT_)

@interface OperaPluginMenuDelegate : NSObject <NSMenuDelegate>
{
	OpChannel *m_adapter_channel;
}

- (id)initWithChannel:(OpChannel *)channel;
- (void)menuWillOpen:(NSMenu*)menu;
- (void)menuDidClose:(NSMenu*)menu;

@end

@implementation OperaPluginMenuDelegate

- (id)initWithChannel:(OpChannel*)channel
{
	[super init];
	m_adapter_channel = channel;
	return self;
}

- (void)menuWillOpen:(NSMenu*)menu
{
	// Tell Opera the menu is about to show
	if (m_adapter_channel)
		m_adapter_channel->SendMessage(OpMacPluginContextMenuShownMessage::Create(TRUE));
}

- (void)menuDidClose:(NSMenu*)menu
{
	// Tell Opera the menu is about to close
	if (m_adapter_channel)
		m_adapter_channel->SendMessage(OpMacPluginContextMenuShownMessage::Create(FALSE));
}

@end

//////////////////////////////////////////////////////////////////////

@interface FullscreenTimer : NSObject {
	NSTimer *timer;
    MacOpPluginTranslator *mac_translator;
} 

- (id)init:(MacOpPluginTranslator *)macTranslator;
- (void)timeout:(NSTimer*)theTimer;

- (void)start;
- (void)stop;
@end

//////////////////////////////////////////////////////////////////////

@implementation FullscreenTimer

- (id)init:(MacOpPluginTranslator *)macTranslator
{
    mac_translator = macTranslator;
	timer = nil;
	[self start];
	return self;
}

- (void)timeout:(NSTimer*)theTimer
{
	// Check the fullscreen is set correctly
    mac_translator->SetFullscreenMode();
}

- (void)start
{
	if (timer == nil)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		NSLog(@" ---> Start Timer (FullscreenTimer)");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		// Every second should be enough
		timer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(timeout:) userInfo:nil repeats:YES];
	}
}

- (void)stop
{
	if (timer != nil)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		NSLog(@" ---> Stop Timer (FullscreenTimer)");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		[timer invalidate];
		timer = nil;
	}
}

@end

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

@interface PluginDrawingTimer : NSObject {
	NSTimer *timer;
    MacOpPluginTranslator *mac_translator;
} 

- (id)init:(MacOpPluginTranslator *)macTranslator;
- (void)timeout:(NSTimer*)theTimer;

- (void)start;
- (void)stop;
@end

//////////////////////////////////////////////////////////////////////

@implementation PluginDrawingTimer

- (id)init:(MacOpPluginTranslator *)macTranslator
{
    mac_translator = macTranslator;
	timer = nil;
	[self start];
	return self;
}

- (void)timeout:(NSTimer*)theTimer
{
	// Pumps frames for CoreAnimation
    mac_translator->DrawInBuffer();
}

- (void)start
{
	if (timer == nil)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		NSLog(@" ---> Start Timer (PluginDrawingTimer)");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		// Pump the timer every 1/60th of a second
		timer = [NSTimer scheduledTimerWithTimeInterval:0.016 target:self selector:@selector(timeout:) userInfo:nil repeats:YES];
	}
}

- (void)stop
{
	if (timer != nil)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		NSLog(@" ---> Stop Timer (PluginDrawingTimer) surface id:%d", mac_translator->GetSurfaceID());
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		[timer invalidate];
		timer = nil;
	}
}

@end

//////////////////////////////////////////////////////////////////////

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

uni_char CarbonUniKey2CocoaNSKey(UniChar c)
{
	uni_char opKey = c;
	switch (c)
	{
		case kHomeCharCode:
			opKey = NSHomeFunctionKey;
			break;
		case kEndCharCode:
			opKey = NSEndFunctionKey;
			break;
		case kPageUpCharCode:
			opKey = NSPageUpFunctionKey;
			break;
		case kPageDownCharCode:
			opKey = NSPageDownFunctionKey;
			break;
		case kUpArrowCharCode:
			opKey = NSUpArrowFunctionKey;
			break;
		case kDownArrowCharCode:
			opKey = NSDownArrowFunctionKey;
			break;
		case kLeftArrowCharCode:
			opKey = NSLeftArrowFunctionKey;
			break;
		case kRightArrowCharCode:
			opKey = NSRightArrowFunctionKey;
			break;
		case kDeleteCharCode:
			opKey = NSDeleteFunctionKey;
			break;
		default:
			opKey = c;
			break;
	}
	return opKey;
}

#ifndef NP_NO_CARBON

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

void GetSceenMouseCoords(Point* globalMouse)
{
	NSPoint where = [NSEvent mouseLocation];
	globalMouse->h = where.x;
	globalMouse->v = [[NSScreen mainScreen] frame].size.height - where.y;
}
#endif // NP_NO_CARBON

//////////////////////////////////////////////////////////////////////

OP_STATUS OpPluginTranslator::Create(OpPluginTranslator** translator, const OpMessageAddress& instance, const OpMessageAddress* adapter, const NPP npp)
{
	*translator = OP_NEW(MacOpPluginTranslator, (instance, adapter, npp));
	if (*translator == NULL)
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

MacOpPluginTranslator::MacOpPluginTranslator(const OpMessageAddress& instance, const OpMessageAddress* adapter, const NPP npp) :
	m_instance(instance)
# ifdef NP_NO_QUICKDRAW
	, m_drawing_model(NPDrawingModelCoreGraphics)
# else
	, m_drawing_model(NPDrawingModelQuickDraw)
# endif // NP_NO_QUICKDRAW
# ifdef NP_NO_CARBON
	, m_event_model(NPEventModelCocoa)
# else
	, m_event_model(NPEventModelCarbon)
# endif // NP_NO_CARBON
# ifndef NP_NO_QUICKDRAW
	, m_qd_port(NULL)
# endif // NP_NO_QUICKDRAW
	, m_resized(false)
	, m_npp(npp)
    , m_view(NULL)
#ifndef NP_NO_QUICKDRAW
	, m_np_port(NULL)
#endif
	, m_np_cgcontext(NULL)
    , m_cgl_ctx(NULL)
    , m_surface(NULL)
	, m_surface_id(0)
    , m_renderer(NULL)
    , m_layer(NULL)
    , m_texture(0)
	, m_fbo(0)
	, m_render_buffer(0)
	, m_plugin_drawing_timer(NULL)
	, m_fullscreen_timer(NULL)
	, m_adapter_channel(NULL)
	, m_adapter(*adapter)
	, m_fullscreen(false)
	, m_got_focus(false)
	, m_updated_text_input(false)
	, m_plugin_menu_delegate(NULL)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	fprintf(stderr, "MacOpPluginTranslator::MacOpPluginTranslator %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	// Store the name of the plugin as a flag to make tests quicker.
	// This is only for hacks so we don't care if it fails
	UniString plugin_name;
	m_plugin_name = None;
	if (OpStatus::IsSuccess(static_cast<PluginComponentInstance*>(m_npp->ndata)->GetComponent()->GetLibrary()->GetName(&plugin_name)))
	{
		if (plugin_name.StartsWith(UNI_L("QuickTime"), 9))
			m_plugin_name = Quicktime;
		else if (plugin_name.StartsWith(UNI_L("Flip4Mac"), 8))
			m_plugin_name = Flip4Mac;
	}
}

//////////////////////////////////////////////////////////////////////

MacOpPluginTranslator::~MacOpPluginTranslator() 
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	fprintf(stderr, "MacOpPluginTranslator::~MacOpPluginTranslator %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	// Delete the menu delegate
	if (m_plugin_menu_delegate)
	{
		[(OperaPluginMenuDelegate *)m_plugin_menu_delegate release];
	}

	// Kill the fullscreen time
	if (m_fullscreen_timer)
	{
		[(FullscreenTimer *)m_fullscreen_timer stop];
		[(FullscreenTimer *)m_fullscreen_timer release];
	}
	
	// Kill the core animation timer if it's running
	if (m_plugin_drawing_timer)
	{
		[(PluginDrawingTimer *)m_plugin_drawing_timer stop];
		[(PluginDrawingTimer *)m_plugin_drawing_timer release];
	}
	
	// Clean up the context if created
	if (m_np_cgcontext)
	{
		CGContextRelease(m_np_cgcontext->context);
		OP_DELETE(m_np_cgcontext);
	}
	
	m_mac_shm.Detach();
	
	DeleteSurface();
	
	// Release the CARenderer
	if (m_renderer)
		CFRelease(m_renderer);

#ifndef NP_NO_QUICKDRAW
	if (m_np_port)
	{
		if (m_np_port->port) {
			DisposeGWorld(m_np_port->port);
			SetGWorld(NULL, NULL);
		}
		op_free(m_np_port);
	}
#endif
	
	OP_DELETE(m_adapter_channel);
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::FinalizeOpPluginImage(OpPluginImageID image, const NPWindow& np_window)
{
	if (m_drawing_model == NPDrawingModelCoreGraphics) 
	{
		// Get the context from the NPWindow
		NP_CGContext *np_cgcontext = (NP_CGContext*)np_window.window;
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//		fprintf(stderr, "FinalizeOpPluginImage NPDrawingModelCoreGraphics\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		if (np_cgcontext && np_cgcontext->context)
		{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//			fprintf(stderr, "FinalizeOpPluginImage NPDrawingModelCoreGraphics %p\n", np_cgcontext->context);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			CGContextRestoreGState(np_cgcontext->context);
		}
		m_resized = false;

		return OpStatus::OK;
	}
	
#ifndef NP_NO_QUICKDRAW
	if (m_drawing_model != NPDrawingModelQuickDraw)
		return OpStatus::OK;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//	fprintf(stderr, "MacOpPluginTranslator::FinalizeOpPluginImage\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	PixMapHandle pm = GetGWorldPixMap(m_qd_port);
	LockPixels(pm);
	
	Ptr base = GetPixBaseAddr(pm);
	
	if (!m_mac_shm.IsAttached())
		RETURN_IF_ERROR(m_mac_shm.Attach(image));
	
	for (int i=0;i<(*pm)->bounds.bottom;i++)
		memcpy(((unsigned char*)m_mac_shm.GetBuffer())+i*(np_window.width*4), base+i*((*pm)->rowBytes&0x3fff), np_window.width*4);
	UnlockPixels(pm); 

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//	fprintf(stderr, " -> MacOpPluginTranslator::FinalizeOpPluginImage done\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	return OpStatus::OK;
#else
	return OpStatus::OK;
#endif // NP_NO_QUICKDRAW
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::CreateAdapterChannel()
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	fprintf(stderr, "MacOpPluginTranslator::CreateAdapterChannel %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	if (!m_adapter_channel)
	{
		/* Create a local channel end-point for two-way communication with the MacOpPluginAdapter. */
		RETURN_OOM_IF_NULL(m_adapter_channel = g_component->CreateChannel(m_adapter));
		RETURN_IF_ERROR(m_adapter_channel->AddMessageListener(this));
		RETURN_IF_ERROR(m_adapter_channel->SendMessage(OpPeerConnectedMessage::Create()));
	}
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type)
{
	RETURN_IF_ERROR(CreateAdapterChannel());
	
	// Start the fullscreen timer when the UpdateNPWindow is called for the first time
	if (!m_fullscreen_timer)
		m_fullscreen_timer = [(FullscreenTimer*)[FullscreenTimer alloc] init:this];

#ifndef NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelQuickDraw)
	{
		if (!m_np_port)
		{
			m_np_port = (NP_Port *)op_calloc(sizeof(NP_Port), 1);
			if(!m_np_port)
				return OpStatus::ERR_NO_MEMORY;
		}

        out_npwin.x						= 0;
        out_npwin.y						= 0;
        out_npwin.width                 = rect.width;
        out_npwin.height                = rect.height;
        out_npwin.clipRect.left         = 0;
        out_npwin.clipRect.top          = 0;
        out_npwin.clipRect.right        = clip_rect.width;
        out_npwin.clipRect.bottom       = clip_rect.height;
        m_np_port->portx = 0;
        m_np_port->porty = 0;
        m_np_port->port = NULL;

		out_npwin.window = m_np_port;
		out_npwin.type = NPWindowTypeDrawable;

		// When the plugin changes size we need to create a new GWorld
		if (!m_qd_port || m_plugin_rect.width != rect.width || m_plugin_rect.height != rect.height) 
		{
			if (m_qd_port)
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "m_qd_port: %p\n", m_qd_port);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
				DisposeGWorld(m_qd_port);
				m_qd_port = NULL;
			}
			Rect gworld_bounds = {0,0,rect.height,rect.width};
			OSStatus err;
			if (noErr != (err = NewGWorld(&m_qd_port,
										  k32ARGBPixelFormat,
										  &gworld_bounds,
										  NULL,					// can be NULL 
										  NULL,					// can be NULL 
										  kNativeEndianPixMap)))	
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "Err: %d, bounds:{%hi,%hi,%hi,%hi}\n", err, gworld_bounds.top, gworld_bounds.left, gworld_bounds.bottom, gworld_bounds.right);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
				return OpStatus::ERR_NO_MEMORY;
			}
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			else
				fprintf(stderr, "bounds:{%hi,%hi,%hi,%hi}\n", gworld_bounds.top, gworld_bounds.left, gworld_bounds.bottom, gworld_bounds.right);
			fprintf(stderr, "m_qd_port1: %p\n", m_qd_port);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			m_np_port->port = m_qd_port;

			m_resized = true;
		}

		// Save the size of the plugin 
		m_plugin_rect = rect;

		// Timer is always needed for Carbon events and QuickDraw is always Carbon Event model based
		if (!m_plugin_drawing_timer)
			m_plugin_drawing_timer = [(PluginDrawingTimer *)[PluginDrawingTimer alloc] init:this];

		return OpStatus::OK;
	}
#endif // NP_NO_QUICKDRAW

	if (m_drawing_model == NPDrawingModelCoreGraphics || m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation)
	{
		out_npwin.x                     = 0;
		out_npwin.y                     = 0;
#ifndef NP_NO_CARBON
		if (m_event_model == NPEventModelCarbon)
		{
			// Fix to make co-ords work correctly for Silverlight
			out_npwin.x                 = m_window_position.x + rect.x;
			out_npwin.y                 = m_window_position.y + rect.y;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: Adjusted rect %u x:%d, y:%d\n", getpid(), out_npwin.x, out_npwin.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		}
#endif // NP_NO_CARBON
        out_npwin.width                 = rect.width;
        out_npwin.height                = rect.height;
        out_npwin.clipRect.left         = 0;
        out_npwin.clipRect.top          = 0;
        out_npwin.clipRect.right        = clip_rect.width;
        out_npwin.clipRect.bottom       = clip_rect.height;
        out_npwin.type                  = type;
		
        // Setup the NP_CGContext and timer (NPDrawingModelCoreGraphics only)
        if (m_drawing_model == NPDrawingModelCoreGraphics)
		{
			if (!m_np_cgcontext)
			{
				m_np_cgcontext = OP_NEW(NP_CGContext, ());
				RETURN_OOM_IF_NULL(m_np_cgcontext);
				m_np_cgcontext->context = NULL;
				m_np_cgcontext->window = NULL;
			}
            out_npwin.window = (void *)m_np_cgcontext;

			if (!m_np_cgcontext->context || m_plugin_rect.width != rect.width || m_plugin_rect.height != rect.height) 
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "UpdateNPWindow: NPDrawingModelCoreGraphics resize %u x:%d, y:%d, width: %d, height: %d\n", getpid(), rect.x, rect.y, rect.width, rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
				m_resized = true;
			}

#ifndef NP_NO_CARBON
			// Timer is always needed for Carbon events
			if (!m_plugin_drawing_timer && m_event_model == NPEventModelCarbon)
				m_plugin_drawing_timer = [(PluginDrawingTimer *)[PluginDrawingTimer alloc] init:this];
#endif // NP_NO_CARBON
		}
	}

	// Save the size of the plugin 
	m_plugin_rect = rect;

	if (m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "UpdateNPWindow: NPDrawingModelCoreAnimation %u width: %d, height: %d, model: %d\n", getpid(), rect.width, rect.height, m_drawing_model);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		// Some documentation on how all this is works
		// http://developer.apple.com/library/mac/#documentation/graphicsimaging/Conceptual/OpenGL-MacProgGuide/opengl_offscreen/opengl_offscreen.html
		if (!m_layer)
		{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: Get CALayer 1\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

			void* value = NULL;
			NPError ret = static_cast<PluginComponentInstance*>(m_npp->ndata)->GetValue(NPPVpluginCoreAnimationLayer, &value);
			if (value && ret == NPERR_NO_ERROR)
				m_layer = (CALayer *)value;
			if (!m_layer)
				return OpStatus::ERR;

			// Create the context we will use for this layer
			CGLPixelFormatAttribute attributes[] = {
				kCGLPFANoRecovery,
				kCGLPFAAccelerated,
				kCGLPFAPBuffer,
				kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
				(CGLPixelFormatAttribute)0
			};
			
			GLint screen;
			CGLPixelFormatObj format;
			if (CGLChoosePixelFormat(attributes, &format, &screen) != kCGLNoError) 
				return OpStatus::ERR;
			
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: Get CALayer 2\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			
			if (CGLCreateContext(format, NULL, &m_cgl_ctx) != kCGLNoError) 
				return OpStatus::ERR;
			CGLDestroyPixelFormat(format);
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: Get CALayer 3\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			
			m_renderer = [[CARenderer rendererWithCGLContext:m_cgl_ctx options:nil] retain];
			if (m_renderer == nil) 
				return OpStatus::ERR;
			((CARenderer *)m_renderer).layer = (CALayer *)m_layer;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: Get CALayer 4\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		}

		// If we get here and there is no gl context it's very very bad
		if (!m_cgl_ctx)
			return OpStatus::ERR;
		
		// Get the surface that is passed via the NPWindow object
		IOSurfaceID surface_id = (IOSurfaceID)(UINT64)out_npwin.window;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "UpdateNPWindow: surface id: %d\n", surface_id);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		// Reset out_npwin.window to NULL to avoid crashes since some plugins 
		// expect this when running CoreAnimation (e.g. Quicktime)
		out_npwin.window = NULL;
		
		// The surface ID has changed then we need to bind to the new surface
		if (m_surface_id != surface_id)
		{
			IOSurfaceRef surface = IOSurfaceLookup(surface_id);

			// Check that the new surface is valid, there is no point doing anything if it isn't
			if (!surface)
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "UpdateNPWindow: Invalid surface\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG	
				return OpStatus::ERR;
			}

			// Set gl context local variable for use in the macros
			CGLContextObj cgl_ctx = m_cgl_ctx;

			// Kill the old surface
			if (m_surface_id)
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "UpdateNPWindow: Delete surface\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
				DeleteSurface();
			}
			
			// Lookup the surface that was created in the browser
            m_surface = surface;

			// Save the surface id
			m_surface_id = surface_id;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: 1\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

			// Setup the size of the CALayer
			[CATransaction begin];
			[CATransaction setValue:[NSNumber numberWithInt:0.0f] forKey:kCATransactionAnimationDuration];
			[(CALayer *)m_layer setFrame:CGRectMake(0, 0, rect.width, rect.height)];
			[CATransaction commit];

			// Set the size of the CARenderer
			((CARenderer *)m_renderer).bounds = CGRectMake(0, 0, rect.width, rect.height);		

			// Set the viewport
			glViewport(0, 0, rect.width, rect.height);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, rect.width, 0, rect.height, -1, 1);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: 2 SurfaceID: %d, IOSurfaceRef: %p, width: %d, height: %d\n", m_surface_id, m_surface, (int)IOSurfaceGetWidth(m_surface), (int)IOSurfaceGetHeight(m_surface));
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			
			// Generate the texture object.
			glGenTextures(1, &m_texture);
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: 3 texture: %d\n", m_texture);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_texture);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			// FBO Generation
			{
				glGenFramebuffersEXT(1, &m_fbo);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
				glGenRenderbuffersEXT(1, &m_render_buffer);
			}
			
			// FBO update and bind to texture
			{
                glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_render_buffer);
                glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                                         GL_DEPTH24_STENCIL8_EXT,
                                         rect.width,
                                         rect.height);
                
                glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
                
                // Update the texture
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_texture);
            }
            
			// Connect the surface and context (texture)
			CGLError cglerr = CGLTexImageIOSurface2D(m_cgl_ctx, GL_TEXTURE_RECTANGLE_ARB,
													 GL_RGBA, rect.width, rect.height,
													 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 
													 m_surface, 0);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: 4 error:%d\n", cglerr);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			if (cglerr != kCGLNoError)
				return OpStatus::ERR;
            
            // FBO work
            {
                GLenum status;
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                          GL_COLOR_ATTACHMENT0_EXT,
                                          GL_TEXTURE_RECTANGLE_ARB,
                                          m_texture,
                                          0);
                status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
				fprintf(stderr, "UpdateNPWindow: 4a status:%d\n", status);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
                if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
                    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                                 GL_DEPTH_ATTACHMENT_EXT,
                                                 GL_RENDERBUFFER_EXT,
                                                 m_render_buffer);
                    status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
					fprintf(stderr, "UpdateNPWindow: 4b status:%d\n", status);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
                }

                if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
                    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                                 GL_STENCIL_ATTACHMENT,
                                                 GL_RENDERBUFFER_EXT,
                                                 m_render_buffer);
                    status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
					fprintf(stderr, "UpdateNPWindow: 4c status:%d\n", status);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
                }
				
				if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
					return OpStatus::ERR;
            }
			
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "UpdateNPWindow: 5, drawing model:%d\n", m_drawing_model);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
            
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Start the timer to make the CoreAnimation code run
			// Hack for flip for Mac so that it gets some extra invalidates and paints 
			// correctly when it is first shown. Fixes DSK-356634
			if (!m_plugin_drawing_timer && (m_drawing_model == NPDrawingModelCoreAnimation || m_plugin_name == Flip4Mac))
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
				fprintf(stderr, "UpdateNPWindow: 6 (PluginDrawingTimer started)\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG
				m_plugin_drawing_timer = [(PluginDrawingTimer *)[PluginDrawingTimer alloc] init:this];
			}
		}
		
		// Always draw a frame after setting everything up
		DrawInBuffer();
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::CreateFocusEvent(OpPlatformEvent** out_event, bool got_focus)
{
	m_got_focus = got_focus;
	
	if (m_event_model == NPEventModelCocoa)
	{
		// As we gain focus we might be dropping out of fullsceen. If the plugin doesn't paint
		// very often the Invalidate won't be called so we should flip out of
		// fulscreen mode here and get the dock and menu back up
		if (m_fullscreen && m_got_focus)
		{
			m_fullscreen = false;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
			fprintf(stderr, " -> ** Exited Fullscreen CreateFocusEvent!\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG

			// Hack for Flash which doesn't drop out of this mode correctly
			SetSystemUIMode(kUIModeNormal, kUIOptionAutoShowMenuBar);

			SendFullscreenMessage(m_fullscreen);
		}

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "Cocoa CreateFocusEvent got_focus:%d\n", got_focus);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		CocoaMacOpPlatformEvent *evt = OP_NEW(CocoaMacOpPlatformEvent, ());
		RETURN_OOM_IF_NULL(evt);
		
		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->type = NPCocoaEventFocusChanged;
		event_record->version = 0;
		
		event_record->data.focus.hasFocus = got_focus;
		evt->m_events.Add(event_record);
		
		*out_event = evt;
		return OpStatus::OK;
		
	} 
	else 
	{
#ifndef NP_NO_CARBON
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "Carbon CreateFocusEvent got_focus:%d\n", got_focus);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		CarbonMacOpPlatformEvent *evt = OP_NEW(CarbonMacOpPlatformEvent, ());
		RETURN_OOM_IF_NULL(evt);
		
		EventKind what;
		
		if (got_focus)
		{
			what = NPEventType_GetFocusEvent;
		}
		else
		{
			what = NPEventType_LoseFocusEvent;
		}

		Point mouse;
		GetSceenMouseCoords(&mouse);

		EventRecord* event_record = OP_NEW(EventRecord, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->what = what;
		event_record->message = 0;
		event_record->when = TickCount();
		event_record->where = mouse;
		event_record->modifiers = 0;
		
		evt->m_events.Add(event_record);
		
		*out_event = evt;
		return OpStatus::OK;
#else
		return OpStatus::ERR;
#endif
	}
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

// Used to check for commands NOT to be passed to the plugin at all.
bool ExcludePluginKeyEvent(OpKey::Code key, ShiftKeyState modifiers)
{
	NSUInteger modifierFlags = CocoaKeyModifiers(modifiers);
	
	// Check for just the Cmd key
	if ((modifierFlags & (NSCommandKeyMask|NSShiftKeyMask|NSAlternateKeyMask)) == NSCommandKeyMask && !IsModifierUniKey(key))
	{
		// We'll support x,c,v,z,a for now
		switch (key)
		{
			case OP_KEY_X:
			case OP_KEY_C:
			case OP_KEY_V:
			case OP_KEY_Z:
			case OP_KEY_A:
				return false;
			default:
				return true;
		}
	}
	return false;
}

OP_STATUS MacOpPluginTranslator::CreateKeyEventSequence(OtlList<OpPlatformEvent*>& out_events, OpKey::Code key, uni_char key_value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 keycode, UINT64 sent_char)
{
	if (ExcludePluginKeyEvent(key, modifiers))
		return OpStatus::ERR;

	if (m_event_model == NPEventModelCocoa)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
		//fprintf(stderr, "CreateKeyEventSequence key:%d, key_state:%d, modifier:%d\n", key, key_state, modifiers);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
		if (key_state == PLUGIN_KEY_DOWN || key_state == PLUGIN_KEY_PRESSED)
		{
			Boolean wasDown = false;

			if (key_state == PLUGIN_KEY_PRESSED)
			{
				for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
				{
					if (m_sent_keys.Get(i)->ch == sent_char)
						wasDown = true;
				}
				if (!wasDown)
					m_sent_keys.Add(new SentKey(sent_char, keycode));
			}

			if (key_state == PLUGIN_KEY_DOWN || (key_state == PLUGIN_KEY_PRESSED && wasDown))
			{
				CocoaMacOpPlatformEvent *evt = OP_NEW(CocoaMacOpPlatformEvent, ());
				RETURN_OOM_IF_NULL(evt);
				NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
				if (!event_record)
				{
					OP_DELETE(evt);
					return OpStatus::ERR_NO_MEMORY;
				}
				
				event_record->version = 0;
				event_record->data.key.keyCode = keycode;
				event_record->data.key.modifierFlags = CocoaKeyModifiers(modifiers);

				if (IsModifierUniKey(key))
				{
					event_record->type = NPCocoaEventFlagsChanged;
					event_record->data.key.characters = NULL;
					event_record->data.key.charactersIgnoringModifiers = NULL;
					event_record->data.key.isARepeat = false;
				}
				else
				{
					event_record->type = NPCocoaEventKeyDown;
					event_record->data.key.characters = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&sent_char length:1];
					uni_char outUniChar;
					if (OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey(keycode, outUniChar))) {
						// Fix to convert arrow keys and such which some plugins read from this field i.e. DSK-369274
						outUniChar = CarbonUniKey2CocoaNSKey(outUniChar);
						event_record->data.key.charactersIgnoringModifiers = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&outUniChar length:1];
					}
					else
						event_record->data.key.charactersIgnoringModifiers = event_record->data.key.characters;
					event_record->data.key.isARepeat = wasDown;
				}
				static_cast<CocoaMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
				
				out_events.Append(evt);
				
				// If the plug-in doesn't support the new text input (via the IME in Opera) then we need to send
				// a NPCocoaEventTextInput with every key down/press
				if (!m_updated_text_input && !IsDeadKey(key) && !IsModifierUniKey(key) && !(modifiers & SHIFTKEY_CTRL))
				{
					evt = OP_NEW(CocoaMacOpPlatformEvent, ());
					RETURN_OOM_IF_NULL(evt);
					event_record = OP_NEW(NPCocoaEvent, ());
					if (!event_record)
					{
						OP_DELETE(evt);
						return OpStatus::ERR_NO_MEMORY;
					}

					event_record->type = NPCocoaEventTextInput;
					event_record->version = 0;
					event_record->data.text.text = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&sent_char length:1];
					static_cast<CocoaMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
					out_events.Append(evt);
				}
			}
		} 
		else 
		{
			for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
			{
				if (m_sent_keys.Get(i)->ch == sent_char)
				{
					// The new text input method never actually sends the key ups
					// Added IsDeadKey even though it shouldn't be required according to the
					// spec but some sites like redtube don't work otherwise see: DSK-363678
					if (!m_updated_text_input || IsDeadKey(key))
					{
						CocoaMacOpPlatformEvent *evt = OP_NEW(CocoaMacOpPlatformEvent, ());
						RETURN_OOM_IF_NULL(evt);
						NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
						if (!event_record)
						{
							OP_DELETE(evt);
							return OpStatus::ERR_NO_MEMORY;
						}

						event_record->version = 0;
						event_record->data.key.isARepeat = false;
						event_record->data.key.keyCode = m_sent_keys.Get(i)->key;
						event_record->data.key.modifierFlags = CocoaKeyModifiers(modifiers);;

						if (IsModifierUniKey(key))
						{
							event_record->type = NPCocoaEventFlagsChanged;
							event_record->data.key.characters = NULL;
							event_record->data.key.charactersIgnoringModifiers = NULL;
						}
						else
						{
							event_record->type = NPCocoaEventKeyUp;
							event_record->data.key.characters = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&sent_char length:1];
							uni_char outUniChar;
							if (OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey(keycode, outUniChar)))
								event_record->data.key.charactersIgnoringModifiers = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&outUniChar length:1];
							else
								event_record->data.key.charactersIgnoringModifiers = event_record->data.key.characters;
						}

						static_cast<CocoaMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
						out_events.Append(evt);
					}
					m_sent_keys.Delete(i);
					break;
				}
			}
		}
		
		return OpStatus::OK;
		
	}
	else
	{
		
#ifndef NP_NO_CARBON
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "Carbon CreateKeyEventSequence key:%d, key_state:%d, modifier:%d, keycode:%d, sent_char:%d\n", key, key_state, modifiers, keycode, sent_char);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		Point mouse;
		GetSceenMouseCoords(&mouse);
		UInt32 when = TickCount();

		if (key_state == PLUGIN_KEY_DOWN || key_state == PLUGIN_KEY_PRESSED)
		{
			Boolean wasDown = false;

			for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
			{
				if (m_sent_keys.Get(i)->ch == sent_char)
					wasDown = true;
			}
			
			CarbonMacOpPlatformEvent *evt = OP_NEW(CarbonMacOpPlatformEvent, ());
			RETURN_OOM_IF_NULL(evt);
			
			EventRecord* event_record = OP_NEW(EventRecord, ());
			if (!event_record)
			{
				OP_DELETE(evt);
				return OpStatus::ERR_NO_MEMORY;
			}
			
			event_record->what = wasDown ? autoKey : keyDown;
			event_record->message = (sent_char&0xFF) | keycode << 8;
			event_record->when = when;
			event_record->where = mouse;
			event_record->modifiers = ClassicKeyModifiers(modifiers);
			
			static_cast<CarbonMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
			
			out_events.Append(evt);
			
			if (!wasDown)
				m_sent_keys.Add(new SentKey(sent_char, keycode));
		}
		else
		{
			for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
			{
				if (m_sent_keys.Get(i)->ch == sent_char)
				{
					CarbonMacOpPlatformEvent *evt = OP_NEW(CarbonMacOpPlatformEvent, ());
					RETURN_OOM_IF_NULL(evt);

					EventRecord* event_record = OP_NEW(EventRecord, ());
					if (!event_record)
					{
						OP_DELETE(evt);
						return OpStatus::ERR_NO_MEMORY;
					}
					
					event_record->what = keyUp;
					event_record->message = (sent_char&0xFF) | m_sent_keys.Get(i)->key << 8;
					event_record->when = when;
					event_record->where = mouse;
					event_record->modifiers = ClassicKeyModifiers(modifiers);

					static_cast<CarbonMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
					out_events.Append(evt);

					m_sent_keys.Delete(i);
					break;
				}
			}
		}
		
		if (out_events.Count() == 0)
		{
			return OpStatus::ERR;
		}

		return OpStatus::OK;
#else
		return OpStatus::ERR;
#endif
	}
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::CreateMouseEvent(OpPlatformEvent** out_event,
								   const OpPluginEventType event_type,
								   const OpPoint& point,
								   const int button,
								   const ShiftKeyState modifiers)
{
	if (m_event_model == NPEventModelCocoa)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//		fprintf(stderr, "Cocoa CreateMouseEvent x:%d, y:%d, button:%d\n", point.x, point.y, button);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		Point location;
		location.h = point.x;
		location.v = point.y;
		
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
				break;
			}
			case PLUGIN_MOUSE_UP_EVENT:
			{
				what = NPCocoaEventMouseUp;
				break;
			}
			case PLUGIN_MOUSE_MOVE_EVENT:
			{
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
				deltay = -button;
				break;
			}
			case PLUGIN_MOUSE_WHEELH_EVENT:
			{
				what = NPCocoaEventScrollWheel;
				deltax = button;
				break;
			}
			default:
				return OpStatus::ERR_NOT_SUPPORTED;
		}
		
		CocoaMacOpPlatformEvent *evt = OP_NEW(CocoaMacOpPlatformEvent, ());
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
		*out_event = evt;
		
		return OpStatus::OK;
		
	}
#ifndef NP_NO_CARBON
	else if (m_event_model == NPEventModelCarbon)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
//		fprintf(stderr, "Carbon CreateMouseEvent x:%d, y:%d\n", point.x, point.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG

#ifdef MAC_PLUGIN_QUICKTIME_QUICKDRAW_HACK		
		// Special hack to QuickTime under QuickDraw, to transform the left mouse click in a key event 
		// of pressing the spacebar. This will allow the video to be started and stopped
		if (event_type == PLUGIN_MOUSE_DOWN_EVENT) 
		{
			if (m_plugin_name == Quicktime)
			{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//				fprintf(stderr, "Carbon CreateMouseEvent (for QuickTime) x:%d, y:%d\n", point.x, point.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

				Point mouse;
				GetSceenMouseCoords(&mouse);
				UInt32 when = TickCount();

				CarbonMacOpPlatformEvent *evt = OP_NEW(CarbonMacOpPlatformEvent, ());
				RETURN_OOM_IF_NULL(evt);

				EventRecord* event_record = OP_NEW(EventRecord, ());
				if (!event_record)
				{
					OP_DELETE(evt);
					return OpStatus::ERR_NO_MEMORY;
				}

				event_record->what = keyDown;
				event_record->message = 12576; // Space
				event_record->when = when;
				event_record->where = mouse;
				event_record->modifiers = 0;

				evt->m_events.Add(event_record);
				*out_event = evt;

				return OpStatus::OK;
			}
		}
#endif // MAC_PLUGIN_QUICKTIME_QUICKDRAW_HACK		
		
		/*
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
				s_over_plugin++;
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
				s_over_plugin--;
				break;
		}*/
		
		// Convert to screen co-ords for Carbon events
		Point location;
		location.h = m_window_position.x + m_plugin_rect.x + point.x;
		location.v = m_window_position.y + m_plugin_rect.y + point.y;

		EventModifiers mod = ClassicKeyModifiers(modifiers);
		
		if (button == 1)
			mod |= controlKey;
		
		UInt16 what;
		
		switch(event_type)
		{
			case PLUGIN_MOUSE_DOWN_EVENT:
			{
				what = mouseDown;
/*				if(button == 1)
				{
					g_input_manager->ResetInput();
					CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
					if(w)
					{
						w->GetScreen()->ReleaseMouseCapture();
					}
					// Plugin may eat the up event
					w->ResetRightMouseCount();
				}*/
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

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "Carbon CreateMouseEvent x:%d, y:%d location h:%d, v:%d win_pos x:%d, y:%d plugin_pos x:%d, y:%d\n", point.x, point.y, location.h, location.v, m_window_position.x, m_window_position.y, m_plugin_rect.x, m_plugin_rect.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		CarbonMacOpPlatformEvent *evt = OP_NEW(CarbonMacOpPlatformEvent, ());
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
		event_record->modifiers = mod;
		event_record->when = TickCount();
		evt->m_events.Add(event_record);
		*out_event = evt;
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, " -> Carbon MouseEvent what:%d\n", event_record->what);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		
		return OpStatus::OK;
	}
#endif // !NP_NO_CARBON
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::CreatePaintEvent(OpPlatformEvent** event, OpPluginImageID dest, OpPluginImageID bg, const OpRect& paint_rect, NPWindow* npwin, bool* npwindow_was_modified)
{
    if (m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation)
    {
        return OpStatus::ERR_NOT_SUPPORTED;
    }

	MacOpPlatformEvent *evt = NULL;
	NP_CGContext *np_cgcontext = NULL;

	// Create the event, the shared memory is inside it
	if (m_event_model == NPEventModelCocoa)
		evt = OP_NEW(CocoaMacOpPlatformEvent, ());
#ifndef NP_NO_QUICKDRAW
	else
		evt = OP_NEW(CarbonMacOpPlatformEvent, ());
#endif // NP_NO_QUICKDRAW
	RETURN_OOM_IF_NULL(evt);
	
	if (m_drawing_model == NPDrawingModelCoreGraphics) 
	{
		// Get the context from the NPWindow
		np_cgcontext = (NP_CGContext*)npwin->window;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		if (np_cgcontext && np_cgcontext->context)
			fprintf(stderr, "CreatePaintEvent NPDrawingModelCoreGraphics %p Shared Memory:%p\n", np_cgcontext->context, m_mac_shm.GetBuffer());
		else
			fprintf(stderr, "CreatePaintEvent NPDrawingModelCoreGraphics\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		// Silverlight 5 RC hack-o-rama as it doesn't return the CGContext when we ask it for it, so 
		// we'll just flip in the last one we trusted rather than trust it.
		if (!np_cgcontext)
		{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "CreatePaintEvent NPDrawingModelCoreGraphics -> broken cgcontext\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			np_cgcontext = m_np_cgcontext;
		}
		else
		{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "CreatePaintEvent NPDrawingModelCoreGraphics -> using cgcontext:%p\n", np_cgcontext->context);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		}
		
		if (!np_cgcontext->context || m_resized)
		{
			// Release the previous context
			CGContextRelease(np_cgcontext->context);
			
			// Reattach to the shared memory
			m_mac_shm.Detach();
			m_mac_shm.Attach(dest);
			
			// Create the new CGBitmap using the shared memory
			CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
			CGBitmapInfo alpha = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
			np_cgcontext->context = CGBitmapContextCreate(m_mac_shm.GetBuffer(), npwin->width, npwin->height, 8, npwin->width*4, colorSpace, alpha);

			CGColorSpaceRelease(colorSpace);
			
			// Translate the coords into the Mac system
			CGContextTranslateCTM(np_cgcontext->context, 0.0, npwin->height);
			CGContextScaleCTM(np_cgcontext->context, 1.0, -1.0);
			
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "CreatePaintEvent NPDrawingModelCoreGraphics -> resized cgcontext:%p cgsize:%lu shmemsize:%d\n", np_cgcontext->context, CGBitmapContextGetBytesPerRow(np_cgcontext->context)*CGBitmapContextGetHeight(np_cgcontext->context), m_mac_shm.GetSize());
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			
			*npwindow_was_modified = true;
			m_resized = false;
		}

		// Clear the shared memory on each frame to avoid transparent areas bleeding through
		m_mac_shm.Clear();

		CGContextSaveGState(np_cgcontext->context);
		
		// Update the currently saved context
		m_np_cgcontext = np_cgcontext;
	}
#ifndef NP_NO_QUICKDRAW
    else if (m_drawing_model == NPDrawingModelQuickDraw)
    {
		// On size changes reassign the qd port and tell the caller we resized
		if (m_resized)
		{
			NP_Port *np_port = (NP_Port *)npwin->window;
			np_port->port = m_qd_port;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "CreatePaintEvent NPDrawingModelQuickDraw -> resized\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

			*npwindow_was_modified = true;
			m_resized = false;
		}

		SetGWorld(m_qd_port, NULL);
    }
#endif
	
	// Create the event to send
    if (m_event_model == NPEventModelCocoa)
    {
        NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
        if (!event_record)
        {
            OP_DELETE(evt);
            return OpStatus::ERR_NO_MEMORY;
        }
        event_record->type = NPCocoaEventDrawRect;
        event_record->version = 0;
        
        if(np_cgcontext)
            event_record->data.draw.context = np_cgcontext->context;
        else
            event_record->data.draw.context = 0;
        
        event_record->data.draw.x = paint_rect.x;
        event_record->data.draw.y = paint_rect.y;
        event_record->data.draw.width = paint_rect.width;
        event_record->data.draw.height = paint_rect.height;
        static_cast<CocoaMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
        
        *event = evt;
        
        return OpStatus::OK;
    }
#ifndef NP_NO_CARBON
    else if (m_event_model == NPEventModelCarbon)
    {
		Point mouse;
		GetSceenMouseCoords(&mouse);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//		fprintf(stderr, "Carbon CreatePaintEvent mouse h:%d, v:%d\n", mouse.h, mouse.v);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		EventRecord* event_record = OP_NEW(EventRecord, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->what = updateEvt;
		
		if(np_cgcontext)
			event_record->message = (unsigned long)np_cgcontext->context;
		else
			event_record->message = 0;
		
		event_record->when = TickCount();
		event_record->where = mouse;
		event_record->modifiers = 0;
		static_cast<CarbonMacOpPlatformEvent*>(evt)->m_events.Add(event_record);
		*event = evt;

		return OpStatus::OK;
    }
#endif // NP_NO_CARBON
	
	return OpStatus::ERR_NOT_SUPPORTED;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::CreateWindowPosChangedEvent(OpPlatformEvent** out_event)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

//////////////////////////////////////////////////////////////////////

bool MacOpPluginTranslator::GetValue(NPNVariable variable, void* value, NPError* result)
{
	*result = NPERR_NO_ERROR;

	switch (variable)
	{
# ifndef NP_NO_QUICKDRAW
		case NPNVsupportsQuickDrawBool:
			*(static_cast<NPBool*>(value)) = false;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "MacOpPluginTranslator::GetValue QuickDraw enabled:1 %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			break;
# endif
		case NPNVsupportsInvalidatingCoreAnimationBool:
		case NPNVsupportsCoreAnimationBool: 
		{
			// Default it to on
			bool	ca_active = true;
			bool	ica_active = true;
			SInt32	version = 0;

			// No CoreAnimation on anything less than Snow Leopard (10.6)
			Gestalt(gestaltSystemVersion, &version);
			if (version >= 0x1060)
			{
				// Called before the NPUpdateWindow so create this if it's not there already
				OpStatus::Ignore(CreateAdapterChannel());

				// Check in Opera if the -disableca command line was used
				if (m_adapter_channel)
				{
					OpSyncMessenger sync(g_component->CreateChannel(m_adapter_channel->GetDestination()));
					if (OpStatus::IsSuccess(sync.SendMessage(OpMacPluginInfoMessage::Create())))
					{
						OpMacPluginInfoResponseMessage *response = OpMacPluginInfoResponseMessage::Cast(sync.GetResponse());

						if (response && response->HasDisabledCA() && response->GetDisabledCA())
							ca_active = false;
						if (response && response->HasDisabledICA() && response->GetDisabledICA())
							ica_active = false;
					}
				}
			}
			else
			{
				ca_active = false;
				ica_active = false;
			}
			
			*(static_cast<NPBool*>(value)) = variable == NPNVsupportsCoreAnimationBool ? ca_active : ica_active;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "MacOpPluginTranslator::GetValue CoreAnimation enabled:%d %p\n", ca_active, this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			break;
		}
		case NPNVsupportsCoreGraphicsBool:
			*(static_cast<NPBool*>(value)) = true;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
			fprintf(stderr, "MacOpPluginTranslator::GetValue CoreGraphics enabled:1 %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
			break;
		case NPNVsupportsCocoaBool:
			*(static_cast<NPBool*>(value)) = true;
			break;

		case NPNVsupportsUpdatedCocoaTextInputBool:
			// Not ideal, but if the plugin asks if we support NPNVsupportsUpdatedCocoaTextInputBool
			// the we'll send it events according to the updated specs
			m_updated_text_input = true;
			*(static_cast<NPBool*>(value)) = true;
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
			fprintf(stderr, "MacOpPluginTranslator::GetValue NPNVsupportsUpdatedCocoaTextInputBool %p\n", this);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
			break;
			
		case NPNVsupportsCompositingCoreAnimationPluginsBool:
			*(static_cast<NPBool*>(value)) = true;
			break;
			
		default:
			return false;
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////

bool MacOpPluginTranslator::SetValue(NPPVariable variable, void* value, NPError* result)
{
	switch (variable)
	{
		case NPPVpluginDrawingModel:
		{
			INTPTR val = (INTPTR)value;
			if (
# ifndef NP_NO_QUICKDRAW
				val != NPDrawingModelQuickDraw &&
# endif // !NP_NO_QUICKDRAW
				val != NPDrawingModelCoreGraphics
				&& val != NPDrawingModelCoreAnimation
				&& val != NPDrawingModelInvalidatingCoreAnimation)
			{
				*result = NPERR_GENERIC_ERROR;
			}
			else
			{
				*result = NPERR_NO_ERROR;
				m_drawing_model = (NPDrawingModel)val;
				return FALSE;	// Set FALSE so this setting is passed back to the MacOpPluginWindow in Opera
			}
		}
			break;
			
		case NPPVpluginEventModel:
		{
			INTPTR val = (INTPTR)value;
			if (
#ifndef NP_NO_CARBON
				val != NPEventModelCarbon &&
#endif
				val != NPEventModelCocoa)
			{
				*result = NPERR_GENERIC_ERROR;
			}
			else
			{
				*result = NPERR_NO_ERROR;
				m_event_model = (NPEventModel)val;
				return FALSE;	// Set FALSE so this setting is passed back to the MacOpPluginWindow in Opera
			}
		}
			break;
			
		default:
			return false;
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////

bool MacOpPluginTranslator::Invalidate(NPRect* rect)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//	NSLog(@"MacOpPluginTranslator::Invalidate top:%u, left:%u, bottom:%u, right:%u menuBarVisible:%d", rect->top, rect->left, rect->bottom, rect->right, [NSMenu menuBarVisible]);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	
    switch (m_drawing_model)
	{
		case NPDrawingModelCoreAnimation:
			return true;
			
		case NPDrawingModelInvalidatingCoreAnimation:
			DrawInBuffer();
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////

bool OpPluginTranslator::GetGlobalValue(NPNVariable variable, void* ret_value, NPError* result)
{
	return false;
}

//////////////////////////////////////////////////////////////////////

bool MacOpPluginTranslator::ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "ConvertPlatformRegionToRect\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
#ifndef NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelQuickDraw)
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
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::ConvertPoint(const double src_x, const double src_y, const int src_space,
											  double* dst_x, double* dst_y, const int dst_space)
{
	if (!dst_x || !dst_y)
		return OpStatus::ERR_NULL_POINTER;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	if (src_x != 0.0 && src_y != 0.0) // Reduce spam
		fprintf(stderr, "ConvertPoint x:%lf, y:%lf, sourceSpace:%d, destSpace:%d Plugin Pos x:%d, y%d\n", src_x, src_y, src_space, dst_space, m_plugin_rect.x, m_plugin_rect.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	NSPoint source_point;
	NSPoint dest_point;
	
	// Convert to screen coordinates.
	switch (src_space)
	{
		case NPCoordinateSpacePlugin: // 1
		{
			source_point.x = (double)m_window_position.x + (double)m_plugin_rect.x + src_x;
			source_point.y = [[NSScreen mainScreen] frame].size.height - (double)m_window_position.y - (double)m_plugin_rect.y - src_y;
			break;
		}
			
		case NPCoordinateSpaceWindow: // 2
			source_point.x = (double)m_window_position.x + src_x;
			source_point.y = (double)m_window_position.y + src_y;
			break;
			
		case NPCoordinateSpaceScreen: // 4
			source_point.x = src_x;
			source_point.y = src_y;
			break;
			
		default:
			OP_ASSERT(!"Implement me.");
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	
	// Convert to target coordinates.
	switch (dst_space)
	{
		case NPCoordinateSpacePlugin: // 1
		{
			dest_point.x = source_point.x - (double)m_window_position.x - (double)m_plugin_rect.x;
			dest_point.y = [[NSScreen mainScreen] frame].size.height - (double)m_window_position.y - (double)m_plugin_rect.y - source_point.y;
			break;
		}
			
		case NPCoordinateSpaceFlippedScreen: // 5
			dest_point.x = source_point.x;
			dest_point.y = [[NSScreen mainScreen] frame].size.height - source_point.y;
			break;
			
		case NPCoordinateSpaceScreen: // 4
			dest_point.x = source_point.x;
			dest_point.y = source_point.y;
			break;
			
		default:
			OP_ASSERT(!"Implement me.");
			return OpStatus::ERR_NOT_SUPPORTED;
	}

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	if (src_x != 0.0 && src_y != 0.0) // Reduce spam
		fprintf(stderr, " -> ConvertPoint Source x:%lf, y:%lf, Dest x:%lf, y:%lf\n", (double)source_point.x, (double)source_point.y, (double)dest_point.x, (double)dest_point.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	*dst_x = dest_point.x;
	*dst_y = dest_point.y;
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::PopUpContextMenu(NPMenu* menu)
{
	NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(m_window_position.x + m_plugin_rect.x, [[NSScreen mainScreen] frame].size.height - (m_window_position.y + m_plugin_rect.y + m_plugin_rect.height), m_plugin_rect.width, m_plugin_rect.height) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
	[window setBackgroundColor: [NSColor clearColor]];
    [window setAlphaValue:0.999];
	[window setOpaque:NO];
	[window makeKeyAndOrderFront:nil];
	[window display];

	NSPoint point = [window convertScreenToBase:[NSEvent mouseLocation]];
	NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown
										location:point
								   modifierFlags:0 
									   timestamp:[NSDate timeIntervalSinceReferenceDate]
									windowNumber:[window windowNumber]
										 context:[NSGraphicsContext currentContext]
									 eventNumber:0
									  clickCount:1 
										pressure:0.0];
	if (event)
	{
		// Create the delegate if it doesn't exist
		if (!m_plugin_menu_delegate)
			m_plugin_menu_delegate = [[OperaPluginMenuDelegate alloc] initWithChannel:m_adapter_channel];

		// If there is no delegate now something is seriously wrong so don't do anything else
		if (m_plugin_menu_delegate)
		{
			[(NSMenu*)menu setDelegate:(OperaPluginMenuDelegate*)m_plugin_menu_delegate];
			[NSMenu popUpContextMenu:(NSMenu*)menu withEvent:event forView:[window contentView]];
			[window close];

			return OpStatus::OK;
		}
	}
	[window close];
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::SendWindowFocusEvent(BOOL focus)
{
	/* Once core inherits MDENativeWindow, this can be turned into an API method. */
	if (m_event_model == NPEventModelCocoa)
	{
		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		RETURN_OOM_IF_NULL(event_record);

		event_record->type = NPCocoaEventWindowFocusChanged;
		event_record->version = 0;
		event_record->data.focus.hasFocus = focus;

		static_cast<PluginComponentInstance*>(m_npp->ndata)->HandleEvent(event_record);
	}

	return OpStatus::ERR_NOT_SUPPORTED;
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::DrawInBuffer()
{
#ifndef NP_NO_CARBON
	if (m_event_model == NPEventModelCarbon)
	{
		if (m_npp->ndata)
		{
			// Send the nullEvent to the plugin to force it to pump all events
			EventRecord* event_record = OP_NEW(EventRecord, ());
			if (!event_record)
				return;

			Point mouse;
			GetSceenMouseCoords(&mouse);

			event_record->where = mouse;
			event_record->what = nullEvent;
			event_record->message = 0;
			event_record->when = TickCount();
			static_cast<PluginComponentInstance*>(m_npp->ndata)->HandleEvent(event_record);

			// Make the plugin draw in the browser process
			if (m_adapter_channel)
				m_adapter_channel->SendMessage(OpMacPluginUpdateViewMessage::Create());    
		}
		return;
	}
#endif // NP_NO_CARBON
	if (m_drawing_model == NPDrawingModelCoreAnimation || m_drawing_model == NPDrawingModelInvalidatingCoreAnimation)
	{
		if (!m_renderer)
			return;

		// Get the rect
		CGRect layerRect = [(CALayer *)m_layer bounds];

		// Set the GL context to the surface
		CGLContextObj cgl_ctx = m_cgl_ctx;

		// Clear the surface
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Render the CARenderer
		[(CARenderer *)m_renderer beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];
		[(CARenderer *)m_renderer addUpdateRect:layerRect];
		[(CARenderer *)m_renderer render];
		[(CARenderer *)m_renderer endFrame];

		// Swap the buffers 
		glFlush();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

		// Make the plugin draw in the browser process
		if (m_adapter_channel)
			m_adapter_channel->SendMessage(OpMacPluginUpdateViewMessage::Create());    
	}

}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::DeleteSurface()
{
	if (m_surface)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, "MacOpPluginTranslator::DeleteSurface surface id:%d\n", m_surface_id);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

		CFRelease(m_surface);
		m_surface = NULL;
	}
	
	if (m_cgl_ctx)
	{
		CGLContextObj cgl_ctx = m_cgl_ctx;

		// Delete the old render buffers
		if (m_render_buffer)
		{
			glDeleteRenderbuffersEXT(1, &m_render_buffer);
			m_render_buffer = 0;
		}
		
		// Delete the FBO
		if (m_fbo)
		{
			glDeleteFramebuffersEXT(1, &m_fbo);
			m_fbo = 0;
		}
		
		// Delete the old texture
		if (m_texture)
		{
			glDeleteTextures(1, &m_texture);
			m_texture = 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::SetFullscreenMode()
{
	SystemUIMode outMode;
	SystemUIOptions  outOptions;
	GetSystemUIMode(&outMode, &outOptions);
	
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//	fprintf(stderr, " -> Fullscreen outMode:%d, m_fullscreen:%d, m_got_focus:%d\n", outMode, m_fullscreen, m_got_focus);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	if (outMode == kUIModeAllHidden && m_fullscreen == false && m_got_focus)
	{
		m_fullscreen = true;
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, " -> Entered Fullscreen!\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		
		SendFullscreenMessage(m_fullscreen);
	}
	else if (outMode == kUIModeNormal && m_fullscreen == true && m_got_focus)
	{
		m_fullscreen = false;
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		fprintf(stderr, " -> ** Exited Fullscreen DrawInBuffer!\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
		
		SendFullscreenMessage(m_fullscreen);
	}
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::SendFullscreenMessage(bool fullscreen)
{
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
	
	if (m_adapter_channel)
		m_adapter_channel->SendMessage(OpMacPluginFullscreenMessage::Create(fullscreen, psn.highLongOfPSN, psn.lowLongOfPSN));    
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::SendBrowserWindowShown(bool show)
{
    ProcessSerialNumber psn;
	
	if (GetCurrentProcess(&psn) == noErr)
	{
		if (m_adapter_channel)
			m_adapter_channel->SendMessage(OpMacPluginWindowShownMessage::Create(show, psn.highLongOfPSN, psn.lowLongOfPSN));    
	}
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginTranslator::SendBrowserCursorShown(bool show)
{
	if (m_adapter_channel)
		m_adapter_channel->SendMessage(OpMacPluginCursorShownMessage::Create(show));    
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginTranslator::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpPeerDisconnectedMessage::Type:
			break;

		case OpMacPluginVisibilityMessage::Type:
			OnVisibilityMessage(OpMacPluginVisibilityMessage::Cast(message));
			break;

		case OpMacPluginDesktopWindowMovementMessage::Type:
			OnDesktopWindowMovementMessage(OpMacPluginDesktopWindowMovementMessage::Cast(message));
			break;

		case OpMacPluginIMETextMessage::Type:
			OnIMETextMessage(OpMacPluginIMETextMessage::Cast(message));
			break;
	}

	return OpStatus::OK;
}

void MacOpPluginTranslator::OnVisibilityMessage(OpMacPluginVisibilityMessage* message)
{
	if (m_plugin_drawing_timer)
	{
		if (message->GetVisible())
			[(PluginDrawingTimer *)m_plugin_drawing_timer start];
		else
			[(PluginDrawingTimer *)m_plugin_drawing_timer stop];
	}
	else if (message->GetVisible())
	{
		// Required for drawing models that don't have a timer (i.e. NPDrawingModelInvalidatingCoreAnimation)
		// so that the plugin is draw when it becomes visible
		DrawInBuffer();
	}

	OpStatus::Ignore(SendWindowFocusEvent(message->GetVisible()));
}

void MacOpPluginTranslator::OnDesktopWindowMovementMessage(OpMacPluginDesktopWindowMovementMessage* message)
{
	m_window_position.x = message->GetX();
	m_window_position.y = message->GetY();
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	fprintf(stderr, "OpMacPluginDesktopWindowMovementMessage x:%d, y:%d\n", m_window_position.x, m_window_position.y);
#endif // MAC_PLUGIN_WRAPPER_DEBUG

#ifndef NP_NO_CARBON
	if (m_event_model == NPEventModelCarbon)
	{
		OpRect rect(m_window_position.x, m_window_position.y, m_plugin_rect.width, m_plugin_rect.height);
		static_cast<PluginComponentInstance*>(m_npp->ndata)->SetWindow(rect, rect);
	}
#endif // NP_NO_CARBON
}

void MacOpPluginTranslator::OnIMETextMessage(OpMacPluginIMETextMessage* message)
{
	NSString *text = nil;

	SetNSString((void **)&text, message->GetText());
	if ([text length])
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
		NSLog(@"MacOpPluginTranslator::OnIMETextMessage: %@", text);
#endif // MAC_PLUGIN_WRAPPER_DEBUG

		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		if (event_record)
		{
			event_record->type = NPCocoaEventTextInput;
			event_record->version = 0;
			event_record->data.text.text = (NPNSString*)text;

			static_cast<PluginComponentInstance*>(m_npp->ndata)->HandleEvent(event_record);
		}
	}
}

MacOpPluginTranslator *MacOpPluginTranslator::GetActivePluginTranslator()
{
	PluginComponent *plugin_component = PluginComponent::FindPluginComponent();
	if (plugin_component)
	{
		OtlSet<PluginComponentInstance*>::ConstIterator it = plugin_component->GetInstances().Begin();
		if (it != plugin_component->GetInstances().End())
			return static_cast<MacOpPluginTranslator*>((*it)->GetTranslator());
	}

	return NULL;
}

#endif // NS4P_COMPONENT_PLUGINS

#endif // defined(_PLUGIN_SUPPORT_)

