/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscaler.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "platforms/mac/pi/plugins/MacOpPluginLayer.h"
#include "platforms/mac/pi/MacOpView.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/MacOpPrinterController.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/src/generated/g_message_mac_messages.h"
#include "platforms/mac/util/UKeyTranslate.h"
#include "platforms/mac/util/macutils_mincore.h"
#include "platforms/mac/model/BottomLineInput.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"

#define BOOL NSBOOL
#import <AppKit/NSEvent.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSWindow.h>
#import <QuartzCore/CATransaction.h>
#undef BOOL

// Set to turn on all the printfs to debug the PluginWrapper
// #define MAC_PLUGIN_WRAPPER_DEBUG

////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginWindow::CreateCALayer()
{
#ifdef NS4P_COMPONENT_PLUGINS
	// CA layer or our layer already created.
	if (m_plugin_layer)
		return OpStatus::ERR;
#endif // NS4P_COMPONENT_PLUGINS

	// Create the plugin layer
	MacOpPluginLayer *plugin_layer;
#ifndef NS4P_COMPONENT_PLUGINS
	if (GetDrawingModel() == NPDrawingModelCoreGraphics)
		plugin_layer = [[MacOpCGPluginLayer alloc] initWithPluginWindow:this];
	else
#endif // NS4P_COMPONENT_PLUGINS
	plugin_layer = [[MacOpPluginLayer alloc] init];

	[plugin_layer setAnchorPoint:CGPointMake(0, 0)];
    
#ifndef NS4P_COMPONENT_PLUGINS
    // Create a new layer because we can't trust the layer sent from the plugin
    // to position itself in *any* position except (0,0). So we make our own
    // layer that wrappers the plugin layer and we can position that

    // Set the anchor for the layer we got from the plugin
	[m_plugin_layer setAnchorPoint:CGPointMake(0, 0)];

    // Make the layer we got from the plugin as a sublayer of the positioning layer
	if ((GetDrawingModel() == NPDrawingModelCoreAnimation || GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation))
		[plugin_layer addSublayer:m_plugin_layer];

#endif // NS4P_COMPONENT_PLUGINS
	m_plugin_layer = plugin_layer;

	// Add the layer to the view
    [GetOperaNSView() addSublayer:m_plugin_layer];

	// Set the size of the plugin
	SetLayerFrame();

#ifdef NS4P_COMPONENT_PLUGINS
	// First time it's created so set the actual size, so it will paint correctly
	// on the very first display
	[m_plugin_layer setFrameFromDelayed];
#endif // NS4P_COMPONENT_PLUGINS

#ifndef NS4P_COMPONENT_PLUGINS
	SetPluginLayerFrame();
#endif // NS4P_COMPONENT_PLUGINS

#ifdef NS4P_COMPONENT_PLUGINS
	// Bind the IOSurface on the NSOpenGLView
	[(MacOpPluginLayer *)m_plugin_layer setSurfaceID:m_surface_id];
#endif // NS4P_COMPONENT_PLUGINS

	return OpStatus::OK;
}

void MacOpPluginWindow::DestroyCALayer()
{
	// Layer was not created.
	if (!m_plugin_layer)
		return;

	[m_plugin_layer removeFromSuperlayer];
	[m_plugin_layer release];

	m_plugin_layer = NULL;
}

void MacOpPluginWindow::ShowCALayer(BOOL show)
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
#ifdef NS4P_COMPONENT_PLUGINS
	printf("MacOpPluginWindow::ShowCALayer show:%d surface_id:%d\n", show, m_surface_id);
#else
	printf("MacOpPluginWindow::ShowCALayer show:%d\n", show);
#endif // NS4P_COMPONENT_PLUGINS
#endif // MAC_PLUGIN_WRAPPER_DEBUG	

	// Layer was not created
	if (!m_plugin_layer)
		return;

    [CATransaction begin];
	[CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
	[m_plugin_layer setHidden:!show];
    [CATransaction commit];
}

OP_STATUS MacOpPluginWindow::PaintDirectly(const OpRect& paintrect)
{
	if (UsesDirectPaint())
	{
		OpRect punch_rect(paintrect);
		OpPoint offset_point(-m_plugin->GetWindowX(), -m_plugin->GetWindowY());
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		PixelScaler scaler(GetPixelScalerScale());
		scaler.ToDevicePixel(&punch_rect);
		offset_point = scaler.ToDevicePixel(offset_point);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		punch_rect.OffsetBy(offset_point);

		// Make sure the layering of the plugin layers is correct
		[GetOperaNSView() updatePluginZPosition:m_plugin_layer];

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
        //printf("MacOpPluginWindow::PaintDirectly update_rect x:%d, y:%d\n", -m_plugin->GetWindowX(), -m_plugin->GetWindowY());
#endif // MAC_PLUGIN_WRAPPER_DEBUG

		UpdateCALayer(punch_rect);
	}
	return OpStatus::OK;
}

void MacOpPluginWindow::UpdateCALayer(const OpRect& update_rect)
{
    if (MacOpPrinterController::IsPrinting())
        return;
    
	// Layer was not created.
	if (!m_plugin_layer)
		return;

#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
	printf("MacOpPluginWindow::UpdateCALayer update_rect x:%d, y:%d, width:%d, height:%d\n", update_rect.x, update_rect.y, update_rect.width, update_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG	
	OpRect punch_rect(update_rect);

	OpPoint offset_point(m_x, m_y);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
    PixelScaler scaler(GetPixelScalerScale());
	offset_point = scaler.ToDevicePixel(offset_point);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	punch_rect.OffsetBy(offset_point);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
	printf("MacOpPluginWindow::UpdateCALayer punch_rect x:%d, y:%d, width:%d, height:%d\n", punch_rect.x, punch_rect.y, punch_rect.width, punch_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG	

	OpRect intersect_rect(m_x + m_painter_clip_rect.x, m_y + m_painter_clip_rect.y, m_painter_clip_rect.width, m_painter_clip_rect.height);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	scaler.ToDevicePixel(&intersect_rect);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	punch_rect.IntersectWith(intersect_rect);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG
	printf("MacOpPluginWindow::UpdateCALayer m_painter_clip_rect x:%d, y:%d, width:%d, height:%d\n", m_painter_clip_rect.x, m_painter_clip_rect.y, m_painter_clip_rect.width, m_painter_clip_rect.height);
	printf("MacOpPluginWindow::UpdateCALayer punch_rect x:%d, y:%d, width:%d, height:%d\n", punch_rect.x, punch_rect.y, punch_rect.width, punch_rect.height);
#endif // MAC_PLUGIN_WRAPPER_DEBUG

	[GetOperaNSView() screen]->MoveToBackground(punch_rect, m_transparent);

#ifdef NS4P_COMPONENT_PLUGINS
    // Adjust the position of the layer
	SetLayerFrame();

	// Update the surface if it has changed
	if (m_surface_id != [(MacOpPluginLayer*)m_plugin_layer getSurfaceID])
	{
		// Bind the IOSurface on the View
		[(MacOpPluginLayer*)m_plugin_layer setSurfaceID:m_surface_id];
	}
#endif // NS4P_COMPONENT_PLUGINS

#ifndef NS4P_COMPONENT_PLUGINS
	if (GetDrawingModel() == NPDrawingModelCoreGraphics)
		[m_plugin_layer setNeedsDisplay];
#endif
}

#ifdef NS4P_COMPONENT_PLUGINS

void MacOpPluginWindow::UpdatePluginView()
{
	if ((GetDrawingModel() == NPDrawingModelCoreAnimation || GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation))
	{
		// Tell the layer to update
		[m_plugin_layer setNeedsDisplay];
	}
	else if (m_parent_view && GetDrawingModel() == NPDrawingModelQuickDraw)
	{
		m_parent_view->Invalidate(OpRect(m_plugin->GetWindowX(), m_plugin->GetWindowY(),m_width,m_height));
	}
}

#endif // NS4P_COMPONENT_PLUGINS

#ifndef NS4P_COMPONENT_PLUGINS

extern NSUInteger CocoaKeyModifiers(ShiftKeyState shiftState);
extern uni_char OperaToClassicCharCode(uni_char uc);

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
			break;
	}
	return opKey;
}

void GetScreenMouseCoords(Point &globalMouse)
{
	NSPoint where = [NSEvent mouseLocation];
	globalMouse.h = where.x;
	globalMouse.v = [[NSScreen mainScreen] frame].size.height - where.y;
}

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

OP_STATUS MacOpPluginWindow::CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data,
                                            OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers)
{
	if (ExcludePluginKeyEvent(key, modifiers))
		return OpStatus::ERR;
	
	if (GetEventModel() == NPEventModelCocoa)
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
		fprintf(stderr, "CreateKeyEvent key:%d, key_value:%d, key_state:%d, modifier:%d\n", key, key_value, key_state, modifiers);
#endif
        // If the IME is up then don't send any plug-in key events
        if ([[BottomLineInput bottomLineInput] isVisible])
            return OpStatus::ERR_NOT_SUPPORTED;
        
		OpAutoPtr<MacOpPlatformEvent<NPCocoaEvent> > evt(OP_NEW(MacOpPlatformEvent<NPCocoaEvent>, ()));
		RETURN_OOM_IF_NULL(evt.get());

		NPCocoaEvent* event_record;
		if (key_state == PLUGIN_KEY_DOWN || key_state == PLUGIN_KEY_PRESSED)
		{
			Boolean wasDown = false;

			if (key_event_data)
			{
				uni_char sent_char = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_sent_char;
				unsigned short key_code = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_key_code;

//				if (key_state == PLUGIN_KEY_PRESSED)
				{
					for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
					{
						if (m_sent_keys.Get(i)->ch == sent_char)
							wasDown = true;
					}
					if (!wasDown)
						m_sent_keys.Add(new SentKey(sent_char, key_code));
				}

				if (key_state == PLUGIN_KEY_DOWN || (key_state == PLUGIN_KEY_PRESSED && wasDown))
				{
					NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
					RETURN_OOM_IF_NULL(event_record);
                    
					event_record->version = 0;
					event_record->data.key.keyCode = key_code;
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
						if (OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey(key_code, outUniChar)))
						{
							// Fix to convert arrow keys and such which some plugins read from this field i.e. DSK-369274
							outUniChar = CarbonUniKey2CocoaNSKey(outUniChar);
							event_record->data.key.charactersIgnoringModifiers = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&outUniChar length:1];
						}
						else
						{
							event_record->data.key.charactersIgnoringModifiers = event_record->data.key.characters;
						}
						event_record->data.key.isARepeat = wasDown;
					}
					MacOpPlatformEvent<NPCocoaEvent> seak_evt;
					seak_evt.m_events.Add(event_record);

#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
                   fprintf(stderr, " -> CreateKeyEvent wasDown:%d\n", wasDown);
#endif

//                    m_sent_keys.Add(new SentKey(sent_char, key_code));

                    // Send the event and check if the IME has started... if so don't send anything else
					int16 rtn_code = m_plugin->HandleEvent(&seak_evt, PLUGIN_KEY_EVENT);
                    if (rtn_code == kNPEventStartIME)
                    {
                        [[BottomLineInput bottomLineInput] shouldShow];
                        
                        *key_event = evt.release();
                        // This is cheating but since we handled the event in the Mac code we need to
                        // return ERR_NOT_SUPPORTED to which the core code will respond as if they
                        // handled it ;)
                        return OpStatus::ERR_NOT_SUPPORTED;
                    }
				}

				// If the plug-in doesn't support the new text input (via the IME in Opera) then we need to send
				// a NPCocoaEventTextInput with every key down/press
				if (!GetSupportsUpdatedCocoaTextInput() && !IsDeadKey(key) && !IsModifierUniKey(key) && !(modifiers & SHIFTKEY_CTRL))
				{
					event_record = OP_NEW(NPCocoaEvent, ());
					RETURN_OOM_IF_NULL(event_record);

					event_record->type = NPCocoaEventTextInput;
					event_record->version = 0;
					event_record->data.text.text = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&(sent_char) length:1];
					evt->m_events.Add(event_record);
				}
                else
                {
                    // We have to cheat again here, since we would have sent an event internally, and we don't want
                    // core to send another one and we don't want the function to fail at bottom so we can
                    // return ERR_NOT_SUPPORTED again and core will act like they handled it
                    *key_event = evt.release();
                    return OpStatus::ERR_NOT_SUPPORTED;
                }
			}
            else
            {
                // Core is as broken as hell and doesn't pass the OpPlatformKeyEventData when InvokeKeyPressed, for now I'll just ignore them
                // but this isn't a good solution. Core needs to pass the OpPlatformKeyEventData for KeyUp, KeyDown *and* KeyPressed!

                // Dodgey work around to keep things working for now
                *key_event = evt.release();
                return OpStatus::ERR_NOT_SUPPORTED;
            }
		} else {
            BOOL event_created = FALSE;

			if (key_event_data)
			{
				uni_char sent_char = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_sent_char;
				unsigned short key_code = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_key_code;
				
				for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
				{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
                    fprintf(stderr, " -> Checking key:%d of %d keys key_code:%d\n", m_sent_keys.Get(i)->key, m_sent_keys.GetCount(), key_code);
#endif
                    if (key_code == m_sent_keys.Get(i)->key)
                    {
                        // The new text input method never actually sends the key ups
                        // Added IsDeadKey even though it shouldn't be required according to the
                        // spec but some sites like redtube don't work otherwise see: DSK-363678
                        if (!GetSupportsUpdatedCocoaTextInput() || IsDeadKey(key) || IsModifierUniKey(key))
                        {
                            event_record = OP_NEW(NPCocoaEvent, ());
                            RETURN_OOM_IF_NULL(event_record);
                            
                            event_record->version = 0;
                            event_record->data.key.isARepeat = false;
                            event_record->data.key.keyCode = m_sent_keys.Get(i)->key;
                            event_record->data.key.modifierFlags = CocoaKeyModifiers(modifiers);
                            
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
                                if (OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey(key_code, outUniChar)))
                                    event_record->data.key.charactersIgnoringModifiers = (NPNSString*)[NSString stringWithCharacters:(const unichar *)&outUniChar length:1];
                                else
                                    event_record->data.key.charactersIgnoringModifiers = event_record->data.key.characters;
                            }
                            evt->m_events.Add(event_record);
#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
                            fprintf(stderr, " -> Delete key! key:%d\n", m_sent_keys.Get(i)->key);
#endif
                            m_sent_keys.Delete(i);
                            
                            event_created = TRUE;
                        }
                        else
                        {
#ifdef MAC_PLUGIN_WRAPPER_DEBUG	
                            fprintf(stderr, " -> Delete key! key:%d\n", m_sent_keys.Get(i)->key);
#endif
                            m_sent_keys.Delete(i);
                        }
						break;
                    }
				}
			}
            
            if (!event_created)
            {
                // Keyup has the same issue as key down with the new text input. We need to
                // tell core it's ERR_NOT_SUPPORTED so they don't try to handle no events
                *key_event = evt.release();
                return OpStatus::ERR_NOT_SUPPORTED;
            }
		}
        
		if (evt->GetEventDataCount() == 0)
		{
			return OpStatus::ERR;
		}
		*key_event = evt.release();
		return OpStatus::OK;
        
	} else {
        
#ifndef NP_NO_CARBON
        
		OpAutoPtr<MacOpPlatformEvent<EventRecord> > evt(OP_NEW(MacOpPlatformEvent<EventRecord>, ()));
		RETURN_OOM_IF_NULL(evt.get());
        
		Point mouse;
		GetScreenMouseCoords(mouse);
		mouse.v -= m_title_height;  //HACK!
		UInt32 when = TickCount();

		if (key_state == PLUGIN_KEY_DOWN || key_state == PLUGIN_KEY_PRESSED)
		{
			Boolean wasDown = false;
			if (key_event_data)
			{
				uni_char sent_char = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_sent_char;
				unsigned short key_code = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_key_code;
				for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
				{
					if (m_sent_keys.Get(i)->ch == sent_char)
						wasDown = true;
				}
                
				EventRecord* event_record = OP_NEW(EventRecord, ());
				RETURN_OOM_IF_NULL(event_record);
				event_record->what = wasDown ? autoKey : keyDown;
				event_record->message = (OperaToClassicCharCode(key)&0xFF) | key_code << 8;
				event_record->when = when;
				event_record->where = mouse;
				event_record->modifiers = CocoaKeyModifiers(modifiers);
				evt->m_events.Add(event_record);
                
				if (!wasDown)
					m_sent_keys.Add(new SentKey(sent_char, key_code));
			}
			else
			{
				static TextEncoding lastEncoding = -1;
				static TextEncoding utf16Encoding = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat);
				static TECObjectRef lastConverter = 0;
				UInt8 buf[4];
                
				if(utf16Encoding)
				{
					TextEncoding encode = 0;
					OSStatus err = ::UpgradeScriptInfoToTextEncoding(s_slRec.fScript, s_slRec.fLanguage, kTextRegionDontCare, NULL, &encode);
					if ((noErr == err) && ((!lastConverter) || (lastEncoding != encode)))
					{
						::TECDisposeConverter(lastConverter);
						lastConverter = 0;
						::TECCreateConverter(&lastConverter, utf16Encoding, encode);
						lastEncoding = encode;
					}
                    
					RETURN_OOM_IF_NULL(lastConverter);
                    
					ByteCount realSrcLen;
					ByteCount realDestLen;
					uni_char rc = OperaToClassicCharCode(key);
					err = ::TECConvertText(lastConverter, (ConstTextPtr)&rc, sizeof(rc), &realSrcLen, buf, sizeof(buf), &realDestLen);
                    
					// If we get here it is probably IME data. We post the events as keyboard events in the encoding of the selected layout.
					for (ByteCount i = 0; i < realDestLen; i++)
					{
						// The "characters" sent in these events have little meaning to anyone but the IME.
						// Several glyphs will be sent as 2 or more keyboard events.
						// The plugin will basically use similar code in reverse to obtain the unicode characters.
						for (int j = 0; j < 2; j++)
						{
							EventRecord* event_record = OP_NEW(EventRecord, ());
							RETURN_OOM_IF_NULL(event_record);
                            
							event_record->what = j ? keyUp : keyDown;
							event_record->message = buf[i];
							event_record->when = when;
							event_record->where = mouse;
							event_record->modifiers = CocoaKeyModifiers(modifiers);
							evt->m_events.Add(event_record);
						}
					}
				}
			}
		}
		else
		{
			if (key_event_data)
			{
				uni_char sent_char = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_sent_char;
				unsigned short key_code = ((MacOpPlatformKeyEventData*)key_event_data)->m_key_data.m_key_code;
				EventRecord* event_record = OP_NEW(EventRecord, ());
				for (unsigned int i = 0; i < m_sent_keys.GetCount(); i++)
				{
					if (key_code == m_sent_keys.Get(i)->key)
					{
						RETURN_OOM_IF_NULL(event_record);
						
						event_record->what = keyUp;
						event_record->message = (sent_char&0xFF) | m_sent_keys.Get(i)->key << 8;
						event_record->when = when;
						event_record->where = mouse;
						event_record->modifiers = CocoaKeyModifiers(modifiers);
						evt->m_events.Add(event_record);
						m_sent_keys.Delete(i);
						break;
					}
				}
			}
		}
        
		if (evt->GetEventDataCount() == 0)
		{
			return OpStatus::ERR;
		}
		*key_event = evt.release();
		return OpStatus::OK;
        
#else
        
		return OpStatus::ERR;
        
#endif
        
	}
}

OP_STATUS MacOpPluginWindow::CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in)
{
	if (GetEventModel() == NPEventModelCocoa)
	{
        // Let the bottom line input know this window has focus so the text input
        // is sent to the correct plugin!
        if (focus_in)
        {
            [[BottomLineInput bottomLineInput] setPluginWindow:this];
        }
        
		MacOpPlatformEvent<NPCocoaEvent> *evt = OP_NEW(MacOpPlatformEvent<NPCocoaEvent>, ());
		RETURN_OOM_IF_NULL(evt);
        
		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		if (!event_record)
		{
			OP_DELETE(evt);
			return OpStatus::ERR_NO_MEMORY;
		}
		event_record->type = NPCocoaEventFocusChanged;
		event_record->version = 0;
		event_record->data.focus.hasFocus = focus_in;
		evt->m_events.Add(event_record);
#ifndef SIXTY_FOUR_BIT
		if(focus_in)
		{
			static const EventTypeSpec keyEvents[] = {
				{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent }
			};
			InstallApplicationEventHandler(NewEventHandlerUPP(CocoaCarbonIMEEventHandler), GetEventTypeCount(keyEvents), keyEvents, this, &m_key_handler);
		}
		else
		{
			RemoveEventHandler(m_key_handler);
			// Workaround for DSK-306538:
			CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
			if(w)
			{
				w->ResetFirstResponder();
			}
		}
#endif
        
		*focus_event = evt;
		return OpStatus::OK;
        
	} else {
        
#ifndef NP_NO_CARBON
        
		MacOpPlatformEvent<EventRecord> *evt = OP_NEW(MacOpPlatformEvent<EventRecord>, ());
		RETURN_OOM_IF_NULL(evt);
        
		EventKind what;
        
		WindowRef window = ((CocoaOpWindow*)m_parent_view->GetRootWindow())->GetCarbonWindow();
		static const EventTypeSpec keyEvents[] = {
			{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent }
		};
        
		if(focus_in)
		{
			InstallWindowEventHandler(window, NewEventHandlerUPP(KeyEventHandler), GetEventTypeCount(keyEvents), keyEvents, this, &m_key_handler);
			what = NPEventType_GetFocusEvent;
		}
		else
		{
			RemoveEventHandler(m_key_handler);
			m_key_handler = NULL;
			what = NPEventType_LoseFocusEvent;
			// Workaround for DSK-306538:
			CocoaOpWindow* w = (CocoaOpWindow*)m_parent_view->GetRootWindow();
			if(w)
			{
				w->ResetFirstResponder();
			}
		}
		Point mouse;
		GetGlobalMouse(&mouse);
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
		event_record->where.v -= m_title_height; //HACK!
		event_record->modifiers = 0;
		evt->m_events.Add(event_record);
		*focus_event = evt;
		return OpStatus::OK;
        
#else
        
		return OpStatus::ERR;
        
#endif
        
	}
}

#endif // !NS4P_COMPONENT_PLUGINS

OP_STATUS MacOpPluginWindow::PopUpContextMenu(void* menu)
{
	NSEvent *event = (NSEvent*)CocoaOpWindow::GetLastMouseDownEvent();
	if (event)
	{
		[NSMenu popUpContextMenu:(NSMenu*)menu withEvent:event forView:[[event window] contentView]];
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS MacOpPluginWindow::ConvertPoint(double sourceX, double sourceY, int sourceSpace,
										  double* destX, double* destY, int destSpace)
{
#ifdef NS4P_COMPONENT_PLUGINS
	return OpStatus::ERR_NOT_SUPPORTED;
#else	// !NS4P_COMPONENT_PLUGINS
	if (!m_parent_view || !destX || !destY)
		return OpStatus::ERR_NULL_POINTER;

	CocoaOpWindow* cocoa_window = (CocoaOpWindow*)m_parent_view->GetRootWindow();
	NSWindow* window = cocoa_window->GetNativeWindow();
	NSPoint source_point = NSMakePoint(sourceX, sourceY);
	NSPoint dest_point;

	// Convert to screen coordinates.
	switch (static_cast<NPCoordinateSpace>(sourceSpace))
	{
		case NPCoordinateSpacePlugin:
		{
			/* FIXME: There must be cleaner way to do this but trying to unflip
			 view coordinates gave me a headache so I went crude way. Also it
			 might not be entirely correct (but works for Flash). Maybe if we
			 would override isFlipped method on our view then things would be
			 easier but I can't predict the consequences of doing that. */

			// Offset of window (bottom-left based).
			NSPoint screen_point = [window convertBaseToScreen:NSMakePoint(0, 0)];

			// Convert to top-left origin.
			screen_point.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - screen_point.y - [window frame].size.height;

			// Get offset of content relative to window. 
			INT32 content_x = 0;
			INT32 content_y = 0;
			cocoa_window->GetContentPos(&content_x, &content_y);

			// Add plugin and content offset. Plugin offset origin is "content".
			source_point.x += screen_point.x + content_x + m_x;
			source_point.y += screen_point.y + content_y + m_y;
			break;
		}

		case NPCoordinateSpaceScreen:
			break;

		default:
			return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Convert to target coordinates.
	switch (static_cast<NPCoordinateSpace>(destSpace))
	{
		case NPCoordinateSpacePlugin:
		{
			OperaNSView *view = (OperaNSView *)cocoa_window->GetContentView();
			dest_point.x = source_point.x - m_x - [[view window] frame].origin.x;
			dest_point.y = [[view window] frame].origin.y + [view frame].size.height - m_y - source_point.y;
		}
		break;

		case NPCoordinateSpaceFlippedScreen:
			dest_point = source_point;
			break;

		case NPCoordinateSpaceScreen:
			dest_point.x = source_point.x;
			dest_point.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - source_point.y;
			break;

		default:
			return OpStatus::ERR_NOT_SUPPORTED;
	}

	*destX = dest_point.x;
	*destY = dest_point.y;
	
	return OpStatus::OK;
#endif // NS4P_COMPONENT_PLUGINS
}

UINT MacOpPluginWindow::GetPressedMouseButtons()
{
	if (GetOSVersion() >= 0x1060)
		return (UINT)[NSEvent pressedMouseButtons];

	return 0;
}

#ifdef NS4P_COMPONENT_PLUGINS

UINT64 MacOpPluginWindow::GetWindowIdentifier()
{

	return 0;
}

OP_STATUS MacOpPluginWindow::CreateIOSurface(const OpRect& rect)
{
	// Kill any exisiting surface
	if (m_surface)
		CFRelease(m_surface);
	
    // init our texture and IOSurface
    NSMutableDictionary* surfaceAttributes = [[NSMutableDictionary alloc] init];
    [surfaceAttributes setObject:[NSNumber numberWithBool:YES] forKey: (NSString*)kIOSurfaceIsGlobal];
    [surfaceAttributes setObject:[NSNumber numberWithUnsignedInteger: (NSUInteger)rect.width] forKey:(NSString*)kIOSurfaceWidth];
    [surfaceAttributes setObject:[NSNumber numberWithUnsignedInteger: (NSUInteger)rect.height] forKey:(NSString*) kIOSurfaceHeight];
    [surfaceAttributes setObject:[NSNumber numberWithUnsignedInteger: (NSUInteger)4] forKey:(NSString*)kIOSurfaceBytesPerElement];
    
    m_surface = IOSurfaceCreate((CFDictionaryRef) surfaceAttributes);
    [surfaceAttributes release];

    m_surface_id = IOSurfaceGetID(m_surface);

    return OpStatus::OK;
}

#endif // NS4P_COMPONENT_PLUGINS

OperaNSView* MacOpPluginWindow::GetOperaNSView()
{
	return (OperaNSView *)((CocoaOpWindow*)m_parent_view->GetRootWindow())->GetContentView();
}

void MacOpPluginWindow::SetLayerFrame()
{
	// Layer was not created.
	if (!m_plugin_layer)
		return;

	if (!m_parent_view)
		return;

	CGPoint offset = [[m_plugin_layer superlayer] position];
    [m_plugin_layer setDelayedLayerFrame:CGRectMake(m_x-offset.x, [GetOperaNSView() frame].size.height-m_plugin_layer.frame.size.height-m_y-offset.y, m_width, m_height)];
}

#ifndef NS4P_COMPONENT_PLUGINS
void MacOpPluginWindow::SetPluginLayerFrame()
{
	// Layer was not created.
	if (!m_plugin_layer)
		return;

	[CATransaction begin];
	[CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
	// First time with this size, so it will paint correctly on the very first display
	[m_plugin_layer setFrameFromDelayed];
	// The plugin is always at 0,0
	[[[m_plugin_layer sublayers] objectAtIndex:0] setFrame:CGRectMake(0, 0, m_width, m_height)];
	[CATransaction commit];
}
#endif // NS4P_COMPONENT_PLUGINS

double MacOpPluginWindow::GetBackingScaleFactor()
{
    if (GetOSVersion() >= 0x1070)
        return [[GetOperaNSView() window] backingScaleFactor];
        
    return 1.0;
}

int MacOpPluginWindow::GetPixelScalerScale()
{
    return GetBackingScaleFactor() * 100;
}

#ifndef NS4P_COMPONENT_PLUGINS

void MacOpPluginWindow::SendIMEText(UniString &ime_text)
{
	NSString *text = nil;
    
	SetNSString((void **)&text, ime_text);
	if ([text length])
	{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG
		NSLog(@"MacOpPluginWindow::SendIMEText: %@", text);
#endif // MAC_PLUGIN_WRAPPER_DEBUG
        
		NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
		if (event_record)
		{
			event_record->type = NPCocoaEventTextInput;
			event_record->version = 0;
			event_record->data.text.text = (NPNSString*)text;
            
            MacOpPlatformEvent<NPCocoaEvent> seak_evt;
            seak_evt.m_events.Add(event_record);
            m_plugin->HandleEvent(&seak_evt, PLUGIN_KEY_EVENT);
		}
	}
}

#endif // !NS4P_COMPONENT_PLUGINS

/* static */ OP_STATUS
OpPlatformKeyEventData::Create(OpPlatformKeyEventData **key_event_data, void *data)
{
	MacKeyData *pMKD = reinterpret_cast<MacKeyData*>(data);
	MacOpPlatformKeyEventData *instance = OP_NEW(MacOpPlatformKeyEventData, (pMKD));
	if (!instance)
		return OpStatus::ERR_NO_MEMORY;
	*key_event_data = instance;
	return OpStatus::OK;
}

/* static */ void
OpPlatformKeyEventData::IncRef(OpPlatformKeyEventData *key_event_data)
{
	static_cast<MacOpPlatformKeyEventData *>(key_event_data)->m_ref_count++;
}

/* static */ void
OpPlatformKeyEventData::DecRef(OpPlatformKeyEventData *key_event_data)
{
	MacOpPlatformKeyEventData *event_data = static_cast<MacOpPlatformKeyEventData *>(key_event_data);
	if (event_data->m_ref_count-- == 1)
		OP_DELETE(event_data);
}	


