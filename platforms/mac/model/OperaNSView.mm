/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/model/OperaWindowDelegate.h"
#include "platforms/mac/model/VegaLayer.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/MacOpView.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaOpAccessibilityAdapter.h"
#include "platforms/mac/pi/CocoaOpDragManager.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "platforms/mac/pi/MacOpPrinterController.h"
#include "platforms/mac/pi/plugins/MacOpPluginLayer.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/UKeyTranslate.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/OpenGLContextWrapper.h"
#include "platforms/mac/util/CocoaPasteboardUtils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_pi/DesktopOpUiInfo.h"
#include "platforms/mac/model/BottomLineInput.h"
#include "platforms/mac/util/macutils_mincore.h"
#include "platforms/mac/File/FileUtils_Mac.h"

#undef YES
#undef NO

#include "adjunct/quick/WindowCommanderProxy.h"

#include "modules/libgogi/pi_impl/mde_widget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpDragManager.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/inputmanager/inputaction.h"

#ifdef USE_GRADIENT_SKIN
#include "modules/skin/OpSkinManager.h"
#endif //USE_GRADIENT_SKIN

#define BOOL NSBOOL
#import <AppKit/NSApplication.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSCursor.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSAttributedString.h>
#import <objc/objc-class.h>
#import <objc/runtime.h>
#undef BOOL
#define EVENT_OPPOINT_X [theEvent locationInWindow].x+[[self window] frame].origin.x
#define EVENT_OPPOINT_Y [[[NSScreen screens] objectAtIndex:0] frame].size.height-([theEvent locationInWindow].y+[[self window] frame].origin.y

extern "C"
{
	extern void _objc_flush_caches(Class);
}

extern OP_STATUS AddTemporaryException(const OpString* exception);

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_6

enum {
	NSEventSwipeTrackingLockDirection      = 0x1 << 0, // Clamp gestureAmount to 0 if the user starts to swipe in the opposite direction than they started.
	NSEventSwipeTrackingClampGestureAmount = 0x1 << 1  // Don't allow gestureAmount to go beyond +/-1.0
};
typedef NSUInteger NSEventSwipeTrackingOptions;

enum {
	NSEventPhaseNone        = 0, // event not associated with a phase.
	NSEventPhaseBegan       = 0x1 << 0,
	NSEventPhaseStationary  = 0x1 << 1,
	NSEventPhaseChanged     = 0x1 << 2,
	NSEventPhaseEnded       = 0x1 << 3,
	NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

@interface NSView (LION_ONLY)
- (void)viewDidChangeBackingProperties;
@end
@interface NSEvent (LION_ONLY)
- (NSEventPhase)momentumPhase;
@end
#endif

/*
 *  Summary:
 *    Virtual keycodes
 *
 *  Discussion:
 *    These constants are the virtual keycodes defined originally in
 *    Inside Mac Volume V, pg. V-191. They identify physical keys on a
 *    keyboard. Those constants with "ANSI" in the name are labeled
 *    according to the key position on an ANSI-standard US keyboard.
 *    For example, kVK_ANSI_A indicates the virtual keycode for the key
 *    with the letter 'A' in the US keyboard layout. Other keyboard
 *    layouts may have the 'A' key label on a different physical key;
 *    in this case, pressing 'A' will generate a different virtual
 *    keycode.
 */
enum {
	kVK_ANSI_1                    = 0x12,
	kVK_ANSI_2                    = 0x13,
	kVK_ANSI_3                    = 0x14,
	kVK_ANSI_4                    = 0x15,
	kVK_ANSI_6                    = 0x16,
	kVK_ANSI_5                    = 0x17,
	kVK_ANSI_9                    = 0x19,
	kVK_ANSI_7                    = 0x1A,
	kVK_ANSI_8                    = 0x1C,
	kVK_ANSI_0                    = 0x1D,

	kVK_ANSI_KeypadDecimal        = 0x41,
	kVK_ANSI_KeypadMultiply       = 0x43,
	kVK_ANSI_KeypadPlus           = 0x45,
	kVK_ANSI_KeypadClear          = 0x47,
	kVK_ANSI_KeypadDivide         = 0x4B,
	kVK_ANSI_KeypadEnter          = 0x4C,
	kVK_ANSI_KeypadMinus          = 0x4E,
	kVK_ANSI_KeypadEquals         = 0x51,
	kVK_ANSI_Keypad0              = 0x52,
	kVK_ANSI_Keypad1              = 0x53,
	kVK_ANSI_Keypad2              = 0x54,
	kVK_ANSI_Keypad3              = 0x55,
	kVK_ANSI_Keypad4              = 0x56,
	kVK_ANSI_Keypad5              = 0x57,
	kVK_ANSI_Keypad6              = 0x58,
	kVK_ANSI_Keypad7              = 0x59,
	kVK_ANSI_Keypad8              = 0x5B,
	kVK_ANSI_Keypad9              = 0x5C,

	kVK_Command                   = 0x37
};
enum {
	kVK_Command_Right             = 0x36  // not defined anywhere(?)
};


static action_handler s_action_handler;
action_handler *g_delayed_action_handler;

@interface NSEvent (DeviceDelta)
// These exist for 10.7, but we use the 10.6 SDK
- (NSBOOL)hasPreciseScrollingDeltas;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (NSEventPhase)phase;
+ (NSBOOL)isSwipeTrackingFromScrollEventsEnabled;
- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options dampenAmountThresholdMin:(CGFloat)minDampenThreshold max:(CGFloat)maxDampenThreshold usingHandler:(void (^)(CGFloat gestureAmount, NSEventPhase phase, NSBOOL isComplete, NSBOOL *stop))trackingHandler;
@end

ShiftKeyState g_nonshift_modifiers;
OpKey::Code g_virtual_key_code;
uni_char g_sent_char;
unsigned short g_key_code;
short g_magic_mouse_momentum = -1;
CGFloat g_deltax;
CGFloat g_deltay;
CGFloat g_prevAmount = 0;

ShiftKeyState ConvertToOperaModifiers(unsigned int modifiers, bool ignoreAltCtrl = false)
{
	ShiftKeyState operaModifiers = SHIFTKEY_NONE;

	if (modifiers & NSShiftKeyMask)
	{
		operaModifiers |= SHIFTKEY_SHIFT;
	}
	if (modifiers & NSCommandKeyMask)
	{
		operaModifiers |= SHIFTKEY_CTRL;
	}
	if (!ignoreAltCtrl)
	{
		if (modifiers & NSAlternateKeyMask)
		{
			operaModifiers |= SHIFTKEY_ALT;
		}
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
		if (modifiers & NSControlKeyMask)
		{
			operaModifiers |= SHIFTKEY_META;
		}
#endif // _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
	}
	return operaModifiers;
}

OpKey::Code UniKey2OpKey(unichar c, unsigned short vk)
{
	OpKey::Code opKey = (OpKey::Code)c;
	switch (vk)
	{
		case kVK_ANSI_1:
			return OP_KEY_1;
		case kVK_ANSI_2:
			return OP_KEY_2;
		case kVK_ANSI_3:
			return OP_KEY_3;
		case kVK_ANSI_4:
			return OP_KEY_4;
		case kVK_ANSI_6:
			return OP_KEY_6;
		case kVK_ANSI_5:
			return OP_KEY_5;
		case kVK_ANSI_9:
			return OP_KEY_9;
		case kVK_ANSI_7:
			return OP_KEY_7;
		case kVK_ANSI_8:
			return OP_KEY_8;
		case kVK_ANSI_0:
			return OP_KEY_0;
	}
	switch (c)
	{
		case NSUpArrowFunctionKey:
			opKey = OP_KEY_UP;
			break;
		case NSDownArrowFunctionKey:
			opKey = OP_KEY_DOWN;
			break;
		case NSLeftArrowFunctionKey:
			opKey = OP_KEY_LEFT;
			break;
		case NSRightArrowFunctionKey:
			opKey = OP_KEY_RIGHT;
			break;
		case NSDeleteFunctionKey:
			opKey = OP_KEY_DELETE;
			break;
		case NSHomeFunctionKey:
			opKey = OP_KEY_HOME;
			break;
		case NSEndFunctionKey:
			opKey = OP_KEY_END;
			break;
		case NSPageUpFunctionKey:
			opKey = OP_KEY_PAGEUP;
			break;
		case NSPageDownFunctionKey:
			opKey = OP_KEY_PAGEDOWN;
			break;
		case 27:
			opKey = OP_KEY_ESCAPE;
			break;
		case 127:
			opKey = OP_KEY_BACKSPACE;
			break;
		case 9:
		case 25:
			opKey = OP_KEY_TAB;
			break;
		case 32:
			opKey = OP_KEY_SPACE;
			break;
		case 3:
		case 13:
			opKey = OP_KEY_ENTER;
			break;
		case NSF1FunctionKey:
			opKey = OP_KEY_F1;
			break;
		case NSF2FunctionKey:
			opKey = OP_KEY_F2;
			break;
		case NSF3FunctionKey:
			opKey = OP_KEY_F3;
			break;
		case NSF4FunctionKey:
			opKey = OP_KEY_F4;
			break;
		case NSF5FunctionKey:
			opKey = OP_KEY_F5;
			break;
		case NSF6FunctionKey:
			opKey = OP_KEY_F6;
			break;
		case NSF7FunctionKey:
			opKey = OP_KEY_F7;
			break;
		case NSF8FunctionKey:
			opKey = OP_KEY_F8;
			break;
		case NSF9FunctionKey:
			opKey = OP_KEY_F9;
			break;
		case NSF10FunctionKey:
			opKey = OP_KEY_F10;
			break;
		case NSF11FunctionKey:
			opKey = OP_KEY_F11;
			break;
		case NSF12FunctionKey:
			opKey = OP_KEY_F12;
			break;
		case NSF13FunctionKey:
			opKey = OP_KEY_F13;
			break;
		case NSF14FunctionKey:
			opKey = OP_KEY_F14;
			break;
		case NSF15FunctionKey:
			opKey = OP_KEY_F15;
			break;
		case NSF16FunctionKey:
			opKey = OP_KEY_F16;
			break;
		case '\'':
			opKey = OP_KEY_QUOTE;
			break;
		case '\\':
			opKey = OP_KEY_OPEN_BACKSLASH;
			break;
		case '/':
			opKey = (vk == kVK_ANSI_KeypadDivide ? OP_KEY_DIVIDE : OP_KEY_SLASH);
			break;
		case '*':
			opKey = (vk == kVK_ANSI_KeypadMultiply ? OP_KEY_MULTIPLY : (OpKey::Code)'*');
			break;
		case '+':
			opKey = (vk == kVK_ANSI_KeypadPlus ? OP_KEY_ADD : OP_KEY_PLUS);
			break;
		case '-':
			opKey = (vk == kVK_ANSI_KeypadMinus ? OP_KEY_SUBTRACT : OP_KEY_MINUS);
			break;
		case ';':
			opKey = OP_KEY_SEMICOLON;
			break;
		case '.':
			opKey = OP_KEY_PERIOD;
			break;
		case ',':
			opKey = OP_KEY_COMMA;
			break;
		case '[':
			opKey = OP_KEY_OPEN_BRACKET;
			break;
		case ']':
			opKey = OP_KEY_CLOSE_BRACKET;
			break;
		case '_': // conflicts with OP_KEY_SLEEP
		case '`': // conflicts with OP_KEY_NUMPAD0
		case '{': // conflicts with OP_KEY_F12
		case '|': // conflicts with OP_KEY_F13
		case '}': // conflicts with OP_KEY_F14
		case '~': // conflicts with OP_KEY_F15
			opKey = OP_KEY_UNICODE;
			break;
		default:
			if (c >= '0' && c <= '9' && vk >= kVK_ANSI_Keypad0 && vk <= kVK_ANSI_Keypad9)
				opKey = (OpKey::Code)(c-'0'+OP_KEY_NUMPAD0);
			else if (c >= 'a' && c <= 'z')
				opKey = (OpKey::Code)uni_toupper(c);
			else if (c > 'z')
				opKey = OP_KEY_UNICODE;
			break;
	}
	return opKey;
}

OpKey::Code CarbonUniKey2OpKey(UniChar c)
{
	OpKey::Code opKey = (OpKey::Code)c;
	switch (c)
	{
		case kHomeCharCode:
			opKey = OP_KEY_HOME;
			break;
		case kEndCharCode:
			opKey = OP_KEY_END;
			break;
		case kPageUpCharCode:
			opKey = OP_KEY_PAGEUP;
			break;
		case kPageDownCharCode:
			opKey = OP_KEY_PAGEDOWN;
			break;
		case kUpArrowCharCode:
			opKey = OP_KEY_UP;
			break;
		case kDownArrowCharCode:
			opKey = OP_KEY_DOWN;
			break;
		case kLeftArrowCharCode:
			opKey = OP_KEY_LEFT;
			break;
		case kRightArrowCharCode:
			opKey = OP_KEY_RIGHT;
			break;
		case kEscapeCharCode:
			opKey = OP_KEY_ESCAPE;
			break;
		case kDeleteCharCode:
			opKey = OP_KEY_DELETE;
			break;
		case kBackspaceCharCode:
			opKey = OP_KEY_BACKSPACE;
			break;
		case kTabCharCode:
			opKey = OP_KEY_TAB;
			break;
		case kSpaceCharCode:
			opKey = OP_KEY_SPACE;
			break;
		case kEnterCharCode:
		case kReturnCharCode:
			opKey = OP_KEY_ENTER;
			break;
		case '\'':
			opKey = OP_KEY_QUOTE;
			break;
		case '\\':
			opKey = OP_KEY_OPEN_BACKSLASH;
			break;
		case '/':
			opKey = OP_KEY_SLASH;
			break;
		case '+':
			opKey = OP_KEY_PLUS;
			break;
		case '-':
			opKey = OP_KEY_MINUS;
			break;
		case ';':
			opKey = OP_KEY_SEMICOLON;
			break;
		case '.':
			opKey = OP_KEY_PERIOD;
			break;
		case ',':
			opKey = OP_KEY_COMMA;
			break;
		case '[':
			opKey = OP_KEY_OPEN_BRACKET;
			break;
		case ']':
			opKey = OP_KEY_CLOSE_BRACKET;
			break;
		case '_': // conflicts with OP_KEY_SLEEP
		case '`': // conflicts with OP_KEY_NUMPAD0
		case '{': // conflicts with OP_KEY_F12
		case '|': // conflicts with OP_KEY_F13
		case '}': // conflicts with OP_KEY_F14
		case '~': // conflicts with OP_KEY_F15
			opKey = OP_KEY_UNICODE;
			break;
		default:
			opKey = (OpKey::Code)uni_toupper(c);
			if (!IS_OP_KEY(opKey))
				opKey = OP_KEY_UNICODE;
			break;
	}
	return opKey;
}

unichar UniKey2Char(unichar c, bool ctrl = false)
{
	switch (c)
	{
		case NSUpArrowFunctionKey:
		case NSDownArrowFunctionKey:
		case NSLeftArrowFunctionKey:
		case NSRightArrowFunctionKey:
		case NSDeleteFunctionKey:
		case NSHomeFunctionKey:
		case NSEndFunctionKey:
		case NSPageUpFunctionKey:
		case NSPageDownFunctionKey:
		case 27:
		case 127:
		case NSF1FunctionKey:
		case NSF2FunctionKey:
		case NSF3FunctionKey:
		case NSF4FunctionKey:
		case NSF5FunctionKey:
		case NSF6FunctionKey:
		case NSF7FunctionKey:
		case NSF8FunctionKey:
		case NSF9FunctionKey:
		case NSF10FunctionKey:
		case NSF11FunctionKey:
		case NSF12FunctionKey:
		case NSF13FunctionKey:
		case NSF14FunctionKey:
		case NSF15FunctionKey:
		case NSF16FunctionKey:
            return 0;
		case 3: // Enter
			return (ctrl ? 3 : 13); // Return
		default:;
	}
	return c;
}

BOOL ShouldHideMousePointer(OpKey::Code c, ShiftKeyState s)
{
	switch (c) {
		case OP_KEY_ENTER:
		case OP_KEY_ESCAPE:
			return NO;

		case OP_KEY_TAB:
			switch (s) {
				case SHIFTKEY_ALT:
					return NO;
				default:
					break;
			}
			return YES;
		default:
			break;
	}
	return YES;
}


class SetEventShiftKeyState
{
public:
	SetEventShiftKeyState(ShiftKeyState shift_keys);
	~SetEventShiftKeyState();
};

SetEventShiftKeyState::SetEventShiftKeyState(ShiftKeyState shift_keys)
{
	((MacOpSystemInfo*)g_op_system_info)->SetEventShiftKeyState(TRUE, shift_keys);
}

SetEventShiftKeyState::~SetEventShiftKeyState()
{
	((MacOpSystemInfo*)g_op_system_info)->SetEventShiftKeyState(FALSE, 0);
}

// Gradient shading structure
struct ShadingInfo
{
	int components;
	UINT32 cmin;
	UINT32 cmax;
	float offset_t;
};

// Gradient shading callback function This function wants RGBA
static void CalculateShadingValues(void *info, const CGFloat *in, CGFloat *out)
{
	float v;
	size_t k, components;
	float cmin[4];
	float cmax[4];

	ShadingInfo *shading_info = (ShadingInfo *)info;

	cmin[0] = OP_GET_R_VALUE(shading_info->cmin)/255.0f;
	cmin[1] = OP_GET_G_VALUE(shading_info->cmin)/255.0f;
	cmin[2] = OP_GET_B_VALUE(shading_info->cmin)/255.0f;
	cmin[3] = OP_GET_A_VALUE(shading_info->cmin)/255.0f;

	cmax[0] = OP_GET_R_VALUE(shading_info->cmax)/255.0f;
	cmax[1] = OP_GET_G_VALUE(shading_info->cmax)/255.0f;
	cmax[2] = OP_GET_B_VALUE(shading_info->cmax)/255.0f;
	cmax[3] = OP_GET_A_VALUE(shading_info->cmax)/255.0f;

	components = shading_info->components;

	float offs = shading_info->offset_t;

	v = fabsf((1-*in) - offs);
	for (k = 0; k < components; k++)
	{
		*out++ = cmax[k] - ((cmax[k] - cmin[k]) * v);
	}
}

MacOpView* GetOpViewAt(MDE_Screen* screen, int x, int y, bool include_children)
{
	MDE_View* v = screen->GetViewAt(x, y, true);
	while (v)
	{
		if (v->IsType("MDE_Widget") && ((MDE_Widget*)v)->GetOpView())
		{
			return (MacOpView*)((MDE_Widget*)v)->GetOpView();
		}
		v = v->m_parent;
	}
	return NULL;
}

OpDragListener* FindDragListener(MDE_Screen* screen, int x, int y)
{
	MacOpView* view = GetOpViewAt(screen, x, y, true);
	if (view) {
		return view->GetDragListener();
	}
	return NULL;
}

@implementation OperaNSView

- (void)setFrameSize:(NSSize)newSize
{
	[super setFrameSize:newSize];

	[CATransaction begin];
	[CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];

	[_content_layer setFrame:CGRectMake(0,0, newSize.width, newSize.height)];
	[_background_layer setFrame:CGRectMake(0,0, newSize.width, newSize.height)];
	[_comp_layer setFrame:CGRectMake(0,0, newSize.width, newSize.height)];

	INT32 top		= [((OperaNSWindow*)[self window]) getUnifiedToolbarHeight] - MAC_TITLE_BAR_HEIGHT + _top_padding;
	INT32 left		= [((OperaNSWindow*)[self window]) unifiedToolbarLeft] + _left_padding;
	INT32 right		= [((OperaNSWindow*)[self window]) unifiedToolbarRight] + _right_padding;
	INT32 bottom	= [((OperaNSWindow*)[self window]) unifiedToolbarBottom] + _bottom_padding;
	[_plugin_clip_layer setFrame:CGRectMake(left, bottom, newSize.width - left - right, newSize.height - top - bottom)];

	// Refresh the CALayers
	[_content_layer setNeedsDisplay];
	[_background_layer setNeedsDisplay];
	[_plugin_clip_layer setNeedsDisplay];

    [CATransaction commit];
}

- (void)setPaddingLeft:(INT32)left_padding andTop:(INT32)top_padding andRight:(INT32)right_padding andBottom:(INT32)bottom_padding
{
	_left_padding = left_padding;
	_top_padding = top_padding;
	_right_padding = right_padding;
	_bottom_padding = bottom_padding;
}

- (id)initWithFrame:(NSRect)frameRect operaStyle:(OpWindow::Style)opera_style unifiedToolbar:(BOOL)unified_toolbar
{
	self = [super initWithFrame:frameRect];
	if (!self)
		return nil;

	_comp_layer = [[CALayer alloc] init];
	[self setLayer:_comp_layer];
	[self setWantsLayer:YES];

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)
	VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;

	if (device && g_opera->libvega_module.rasterBackend == LibvegaModule::BACKEND_HW3D)
	{
		_content_layer = [[VegaNSOpenGLLayer alloc] init];
		_background_layer = [[VegaNSOpenGLLayer alloc] init];
	}
	else
#endif
	{
		_content_layer = [[VegaSoftwareLayer alloc] init];
#ifdef VEGALAYER_DISPLAY_WORKAROUND
		NSMutableDictionary *contentLayerActions = [[[NSMutableDictionary alloc] initWithObjectsAndKeys:[NSNull null], @"contents", nil] autorelease];
		_content_layer.actions = contentLayerActions;
#endif
		_background_layer = [[VegaSoftwareLayer alloc] init];
	}

	[_comp_layer addSublayer:_content_layer];
	[_comp_layer setAnchorPoint:CGPointMake(0, 0)];
	[_comp_layer setFrame:CGRectMake(0, 0, frameRect.size.width, frameRect.size.height)];

	[_content_layer setZPosition:2];
	[_content_layer setAnchorPoint:CGPointMake(0, 0)];

	_plugin_clip_layer = [[CALayer alloc] init];
	[_comp_layer addSublayer:_plugin_clip_layer];
	[_plugin_clip_layer setZPosition:1];
	[_plugin_clip_layer setAnchorPoint:CGPointMake(0, 0)];
	_plugin_clip_layer.masksToBounds = YES;

	[_comp_layer addSublayer:_background_layer];
	[_background_layer setZPosition:0];
	[_background_layer setAnchorPoint:CGPointMake(0, 0)];
	[_background_layer setBackground:TRUE];

	_screen = 0;
	trackingRect = [self addTrackingRect:[self visibleRect] owner:self userData:nil assumeInside:NO];
	_opera_style = opera_style;
	_unified_toolbar = unified_toolbar;
	_imstring = NULL;
	_widgetinfo = OP_NEW(IM_WIDGETINFO, ());
	_fake_right_mouse = FALSE;
	_cycling = FALSE;
	_privacy_mode = FALSE;
	_accessible_item = nil;
	_scale_adjustments = 0;
	_prev_scale = 100;
	_performing_gesture = FALSE;
	_mouse_down_x = 0;
	_mouse_down_y = 0;
	_can_be_dragged = FALSE;
	_has_been_dropped = FALSE;
	_plugin_z_order = 1;
	_left_padding = 0;
	_top_padding = 0;
	_right_padding = 0;
	_bottom_padding = 0;
	_momentum_active = FALSE;
	id items = [[NSMutableArray alloc] init];
	[items addObject:NSStringPboardType];
	[items addObject:NSFilenamesPboardType];
	[items addObject:NSURLPboardType];
	[items addObject:NSFilesPromisePboardType];
	[items addObject:kOperaDragType];
	[items addObject:NSHTMLPboardType];
	if (GetOSVersion() >= 0x1060)
		[items addObject:NSPasteboardTypeHTML];
	[self registerForDraggedTypes:items];
	[items release];
	if (g_magic_mouse_momentum == -1) {
		// DSK-270588 Scrolls too fast and jumpy with momentum
		// prefs for com.apple.driver.AppleBluetoothMultitouch.mouse
		// seems to be missing when no magic mouse has been installed.
		g_magic_mouse_momentum = 0;
		CFPropertyListRef momentum = CFPreferencesCopyAppValue(CFSTR("MouseMomentumScroll"), CFSTR("com.apple.driver.AppleBluetoothMultitouch.mouse"));
		if (momentum)
		{
			int momentum_int = 0;
			if (CFGetTypeID(momentum) == CFBooleanGetTypeID())
				g_magic_mouse_momentum = CFBooleanGetValue((CFBooleanRef)momentum);
			else if (CFGetTypeID(momentum) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef)momentum, kCFNumberIntType, &momentum_int))
				g_magic_mouse_momentum = !!momentum_int;
			CFRelease(momentum);
		}
	}
	for (int buttonNumber = FIRST_OTHER_MOUSE_BUTTON; buttonNumber < NUM_MOUSE_BUTTONS; buttonNumber++)
	{
		_mouseButtonIsDown[buttonNumber] = FALSE;
	}
	return self;
}

-(void)setFrame:(NSRect)frameRect
{
	[super setFrame:frameRect];
	[_comp_layer setFrame:CGRectMake(frameRect.origin.x, frameRect.origin.y, frameRect.size.width, frameRect.size.height)];
}

-(void)dealloc
{
	OP_DELETE(_widgetinfo);
	[_comp_layer release];
	[_content_layer release];
	[_background_layer release];
	[_plugin_clip_layer release];
	[super dealloc];
}

-(void)initDrawFrame
{
	// This will replace the drawRect function in the superview with our own version called drawFrame.
	// Doing this will enable us to control how the frame (i.e. chrome) looks and simulate a Unified toolbar

	// Get the class of the superview where we will insert the new fuction and override the existing
	id frame_class = [[self superview] class];

	// You only want to insert the new function into the class ONCE, so first check if this has been done
	Method already_swapped = class_getInstanceMethod(frame_class, @selector(drawRectOriginal:));
	if (already_swapped)
		return;

	// Get the new drawFrame method from this class
	Method m0 = class_getInstanceMethod([self class], @selector(drawFrame:));

	// Add this new method to the frame class but call it drawRectOriginal: (Don't worry we are doing an exchange below)
	class_addMethod(frame_class, @selector(drawRectOriginal:), method_getImplementation(m0), method_getTypeEncoding(m0));

	// Flush the method cache
	_objc_flush_caches(frame_class);

	// Get the original drawRect: from the frame class and the new method drawRectOriginal: so we can exchange them
	Method m1 = class_getInstanceMethod(frame_class, @selector(drawRect:));
	Method m2 = class_getInstanceMethod(frame_class, @selector(drawRectOriginal:));

	// This is the trick, swap the new and old methods! This will mean our new method will
	// be called everytime drawRect: is called (i.e. totally taking over the drawing)
	IMP imp = method_getImplementation(m1);
	method_setImplementation(m1, method_getImplementation(m2));
	method_setImplementation(m2, imp);
}

- (BOOL)isUnifiedToolbar
{
	return _unified_toolbar;
}

- (void)resetRightMouseCount
{
	_right_mouse_count = 0;
}

- (void)setPrivacyMode:(BOOL)privacyMode
{
	_privacy_mode = privacyMode;
}

- (BOOL)isPrivacyMode
{
	return _privacy_mode;
}

- (void)setAlpha:(float)alpha
{
	DesktopWindow *desktop_window = g_application->GetActiveDesktopWindow(FALSE);
	if (desktop_window)
	{
		CocoaOpWindow *cocoa_window = (CocoaOpWindow*)desktop_window->GetParentWindow();
		if (cocoa_window)
		{
			if (alpha > 1.0f)
				alpha = 1.0f;
			if (alpha < 0.1f)
				alpha = 0.1f;
			CocoaOpWindow::s_alpha = alpha;
			cocoa_window->SetAlpha(CocoaOpWindow::s_alpha);
		}
	}
}

- (void)setAccessibleItem:(OpAccessibleItem*)item
{
	_accessible_item = item;
}

- (CocoaMDEScreen*)screen
{
	return _screen;
}

- (void)setScreen:(CocoaMDEScreen*)screen
{
	_screen = screen;
	[_content_layer setScreen:screen];
	[_background_layer setScreen:screen];

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	[self updateContentsScales];	// DO NOT REMOVE: Might seem like an overhead but is needed on an actual retina machine
#endif
}

- (void)abandonInput
{
	if (g_im_listener && g_im_listener->IsIMEActive())
		g_im_listener->OnStopComposing(TRUE);
	[[NSInputManager currentInputManager] markedTextAbandoned:self];
}

- (BOOL)fake_right_mouse
{
	return _fake_right_mouse;
}

- (void)invalidateAll
{
	if (_screen)
	{
		_screen->Invalidate(MDE_MakeRect(0, 0, [self frame].size.width, [self frame].size.height), TRUE);
	}
}

- (void)drawFrame:(NSRect)rect
{
	RESET_OPENGL_CONTEXT
	
    if (MacOpPrinterController::IsPrinting())
        return;
    
	// This drawFrame isn't really in this class, but takes over for drawing of the frame.
	// This allows us to simulate the look of the Unified toolbar.

	// Draw a clear rect so the titlebar is "erased"
	if ([[self window] respondsToSelector:@selector(isInvisibleTitlebar)] && [(OperaNSWindow*)[self window] isInvisibleTitlebar])
	{
		[[NSColor clearColor] set];
		NSRectFill(rect);

		return;
	}

	// If this isn't a unified toolbar then draw the the normal frame!
	if (![[[self window] contentView] respondsToSelector:@selector(isUnifiedToolbar)] || ![[[self window] contentView] isUnifiedToolbar])
	{
		[self drawRectOriginal:rect];
		return;
	}

	// Draw the Unified toolbar look
	CGContextRef	ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
	CGRect			bounds;
	INT32			gradient_height = [((OperaNSWindow*)[self window]) getUnifiedToolbarHeight];
	CGImageRef		persona_image = [((OperaNSWindow*)[self window]) getPersonaCGImageForWidth:[[self window] frame].size.width andHeight:[[self window] frame].size.height];

	INT32			unified_left = [((OperaNSWindow*)[self window]) unifiedToolbarLeft];
	INT32			unified_right = [((OperaNSWindow*)[self window]) unifiedToolbarRight];
	INT32			unified_bottom = [((OperaNSWindow*)[self window]) unifiedToolbarBottom];

	COLORREF		min_color, max_color, min_border_color, border_color, bottom_border_color, text_color;
	BOOL			gradient_colours_set = FALSE, text_colour_set = FALSE;
	NSColor			*title_colour;

	// No need to draw frame if update area does not intersect with window chrome.
	if(rect.origin.y + rect.size.height < [self frame].size.height - gradient_height &&
		rect.origin.y > unified_bottom &&
		rect.origin.x > unified_left &&
		rect.origin.x + rect.size.width < [self frame].size.width - unified_right)
	{
		return;
	}

	if(rect.size.height == MAC_TITLE_BAR_HEIGHT)
	{
		[self setNeedsDisplayInRect:NSMakeRect(0, [self frame].size.height - gradient_height, [self frame].size.width, gradient_height)];
	}
	bounds.origin.x = 0;
	bounds.origin.y = [self frame].size.height - gradient_height;
	bounds.size.width = [self frame].size.width;
	bounds.size.height = gradient_height;

#ifdef USE_GRADIENT_SKIN
	OpSkinElement *skin_elm = NULL;

	if ([[[self window] contentView] respondsToSelector:@selector(isPrivacyMode)] && [[[self window] contentView] isPrivacyMode])
		skin_elm = g_skin_manager->GetSkinElement("Privacy Gradient Skin");
	else
		skin_elm = g_skin_manager->GetSkinElement("Normal Gradient Skin");

	// If the skin element was found start loading from it!
	if (skin_elm)
	{
		// Try and load the gradient colours from the skin
		if (OpStatus::IsSuccess(skin_elm->GetGradientColors(&min_color, &max_color, &min_border_color, &border_color, &bottom_border_color, [[self window] isMainWindow] ? 0 : SKINSTATE_DISABLED)))
			gradient_colours_set = TRUE;

		// Try and load the title text colour from the skin
		if (OpStatus::IsSuccess(skin_elm->GetTextColor(&text_color, [[self window] isMainWindow] ? 0 : SKINSTATE_DISABLED)))
			text_colour_set = TRUE;
	}
#endif // USE_GRADIENT_SKIN

	// If the gradient colours were not set from the skin use the hard coded defaults
	// Colours here are RGA like they are in the Skin
	if (!gradient_colours_set)
	{
		if ([[self window] isMainWindow])
		{
			min_color			= OP_RGB(0x96, 0x96, 0x96);
			max_color			= OP_RGB(0xc5, 0xc5, 0xc5);
			min_border_color	= OP_RGB(0xdc, 0xdc, 0xdc);
			border_color		= OP_RGB(0x40, 0x40, 0x40);
			bottom_border_color = OP_RGB(0xc7, 0xc7, 0xc7);
		}
		else
		{
			min_color			= OP_RGB(0xcf, 0xcf, 0xcf);
			max_color			= OP_RGB(0xe9, 0xe9, 0xe9);
			min_border_color	= OP_RGB(0xf1, 0xf1, 0xf1);
			border_color		= OP_RGB(0x87, 0x87, 0x87);
			bottom_border_color = OP_RGB(0xe6, 0xe6, 0xe6);
		}
	}

	// If the text colours were not set from the skin use the hard coded defaults
	if (!text_colour_set)
	{
		if ([[self window] isMainWindow])
		{
			text_color		= OP_RGB(0x00, 0x00, 0x00);
			title_colour	= [NSColor windowFrameTextColor];
		}
		else
		{
			text_color		= OP_RGB(0x60, 0x60, 0x60);
			title_colour	= [NSColor disabledControlTextColor];
		}
	}
	// Clear the background
	CGContextClearRect(ctx, CGRectMake(0, [[[self window] contentView] frame].size.height, [self frame].size.width, [self frame].size.height-[[[self window] contentView] frame].size.height));

	// Clip the rounded corners
	INT32 corner_radius = 4;

	// Create a clipping path that creates rounded corners on the top of the window
	CGMutablePathRef thePath = CGPathCreateMutable();
	CGPathMoveToPoint(thePath, NULL, bounds.origin.x, bounds.origin.y);
	CGPathAddArc(thePath, NULL, bounds.origin.x + corner_radius, bounds.origin.y + bounds.size.height - corner_radius, corner_radius, M_PI, M_PI/2., true);
	CGPathAddArc(thePath, NULL, bounds.origin.x + bounds.size.width - corner_radius, bounds.origin.y + bounds.size.height - corner_radius, corner_radius, M_PI/2., 0.0, true);
	CGPathAddLineToPoint(thePath, NULL, bounds.origin.x + bounds.size.width, bounds.origin.y);
	CGPathCloseSubpath(thePath);

	// This is to fix the status bar if it's transparent by filling the entire window
	CGContextSetRGBFillColor(ctx, OP_GET_R_VALUE(min_color)/255.0, OP_GET_G_VALUE(min_color)/255.0, OP_GET_B_VALUE(min_color)/255.0, 1.0);
	CGContextFillRect(ctx, CGRectMake(0, 0, [self frame].size.width, unified_bottom));
	CGContextFillRect(ctx, CGRectMake(0, 0, unified_left, [self frame].size.height));
	CGContextFillRect(ctx, CGRectMake([self frame].size.width - unified_right, 0, unified_right, [self frame].size.height));

	// Clip the path
	CGContextSaveGState(ctx);
	CGContextAddPath(ctx, thePath);
	CGContextClip(ctx);

	if(persona_image)
	{
		// draw the persona image
		CGRect img_rect = CGRectMake(0, 0, [self frame].size.width, [self frame].size.height);
		CGImageRef crop_image = CGImageCreateWithImageInRect(persona_image, img_rect);

		// Make a nice white background to draw on
		CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 1.0);
		CGContextFillRect(ctx, CGRectMake(0, [[[self window] contentView] frame].size.height, [self frame].size.width, [self frame].size.height-[[[self window] contentView] frame].size.height));

		// draw the persona image
		CGContextDrawImage(ctx, CGRectMake(0, [self frame].size.height-CGImageGetHeight(crop_image), CGImageGetWidth(crop_image), CGImageGetHeight(crop_image)), crop_image);
		CGImageRelease(crop_image);

		// Draw the whiteish line across the top of the window
		CGContextSetRGBFillColor(ctx, 1.0, OP_GET_G_VALUE(min_border_color)/255.0, OP_GET_B_VALUE(min_border_color)/255.0, 0.4);
		CGContextFillRect(ctx, CGRectMake(0, [self frame].size.height - 1, [self frame].size.width, 1));

		// paint the overlay for the inactive window, if needed
		if (!([[self window] isMainWindow]))
		{
			UINT32 color = g_desktop_op_ui_info->GetPersonaDimColor();
			CGContextSetRGBFillColor(ctx, (color&0xFF)/255.0, ((color>>8)&0xFF)/255.0, ((color>>16)&0xFF)/255.0, ((color>>24)&0xFF)/255.0);
			CGContextFillRect(ctx, CGRectMake(0, 1, [self frame].size.width, [self frame].size.height - 1));
		}
		CGContextRestoreGState(ctx);
	}
	else
	{
		// no persona image, draw gradient instead
		CGPoint startPoint, endPoint;

		// Calculate the start and end points of the gradient. We need to allow a single pixel at the top and
		// the bottom for the whiteish and blackish lines draw on either side
		startPoint = CGPointMake(bounds.origin.x + bounds.size.width/2, bounds.origin.y + 1);
		endPoint = CGPointMake(bounds.origin.x + bounds.size.width/2, bounds.origin.y + bounds.size.height - 1);

		CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

		static const CGFunctionCallbacks callbacks = { 0, &CalculateShadingValues, NULL };

		size_t components = 1 + CGColorSpaceGetNumberOfComponents(colorspace);

		// Setup the shading info for this gradient
		ShadingInfo shading_info;
		shading_info.components = components;
		shading_info.cmin = min_color;
		shading_info.cmax = max_color;
		shading_info.offset_t = 0.0f;

		CGFunctionRef myShadingFunction = CGFunctionCreate((void *)&shading_info, 1, NULL, components, NULL, &callbacks);

		CGShadingRef shading = CGShadingCreateAxial(colorspace, startPoint, endPoint, myShadingFunction, false, false);

		// Draw the gradient
		CGContextDrawShading(ctx, shading);

		// Draw the whiteish line across the top of the gradient
		CGContextSetRGBFillColor(ctx, OP_GET_R_VALUE(min_border_color)/255.0, OP_GET_G_VALUE(min_border_color)/255.0, OP_GET_B_VALUE(min_border_color)/255.0, 1.0);
		CGContextFillRect(ctx, CGRectMake(0, [self frame].size.height - 1, [self frame].size.width, 1));

		CGContextRestoreGState(ctx);
		CGColorSpaceRelease(colorspace);
		CGShadingRelease(shading);
		CGFunctionRelease(myShadingFunction);
	}

	// Make sure that the content area is actually visible. (i.e. the window hasn't been made really really skinny)
	if ([self frame].size.width > unified_left+unified_right && [self frame].size.height > gradient_height+unified_bottom)
	{
		CGContextSetRGBFillColor(ctx, OP_GET_R_VALUE(border_color)/255.0, OP_GET_G_VALUE(border_color)/255.0, OP_GET_B_VALUE(border_color)/255.0, 1.0);
		// Draw the blackish line across the bottom of the gradient
		CGContextFillRect(ctx, CGRectMake(unified_left, [self frame].size.height-gradient_height, [self frame].size.width-unified_left-unified_right, 1));
		// Draw the blackish line across the top of the bottom bar
		CGContextFillRect(ctx, CGRectMake(unified_left, unified_bottom-1, [self frame].size.width-unified_left-unified_right, 1));

		// If there is a status bar at the bottom we need to draw the highlight line between the status bar and the content view
		if (unified_bottom && !unified_left && !unified_right)
		{
			CGContextSetRGBFillColor(ctx, OP_GET_R_VALUE(bottom_border_color)/255.0, OP_GET_G_VALUE(bottom_border_color)/255.0, OP_GET_B_VALUE(bottom_border_color)/255.0, 1.0);
			CGContextFillRect(ctx, CGRectMake(0, unified_bottom-2, [self frame].size.width, 1));
		}
	}

	// Check if the secret method exists
	if ([self respondsToSelector: @selector(_drawTitleStringIn:withColor:)])
	{
		// Invoke the "secret" draw title method (No you won't find it in any headers, just trust us it's there)
		NSRect title_rect = {{0,[self frame].size.height-22},{bounds.size.width,22}};
		[self _drawTitleStringIn: title_rect withColor: title_colour];
	}
	else
	{
		// Draw the window title by hand. Dodgy.
		HIRect hibounds = {{0,[self frame].size.height-22},{bounds.size.width,22}};
		CGContextSetRGBFillColor(ctx, OP_GET_R_VALUE(text_color)/255.0, OP_GET_G_VALUE(text_color)/255.0, OP_GET_B_VALUE(text_color)/255.0, 1.0);
		HIThemeTextInfo info = {kHIThemeTextInfoVersionZero,
								kThemeStateActive,
								kThemeSystemFont,
								kHIThemeTextHorizontalFlushCenter,
								kHIThemeTextVerticalFlushCenter,
								(1 << 2),
								kHIThemeTextTruncationEnd,
								1, false};
		HIThemeDrawTextBox((CFStringRef)[[self window] title], &hibounds, &info, ctx, kHIThemeOrientationInverted);
	}

	// Clean up
	CGPathRelease(thePath);
}

- (NSBOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
	if (_opera_style == OpWindow::STYLE_GADGET || _opera_style == OpWindow::STYLE_HIDDEN_GADGET || [((OperaNSWindow*)[self window]) isInvisibleTitlebar])
		return YES;

	return [super acceptsFirstMouse:theEvent];
}

- (NSBOOL)acceptsFirstResponder
{
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return NO;
	else
		return YES;
}

- (void)releaseMouseCapture
{
	if (_screen && _screen->m_captured_input)
	{
		_screen->m_captured_input->OnMouseLeave();
		_screen->ReleaseMouseCapture();
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT

	// We should no longer be dragging at this point, but if the last TrigDragEnd was not handled, we may still be.
	if (g_cocoa_drag_mgr->IsDragging())
		g_cocoa_drag_mgr->StopDrag(FALSE);

	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (_imstring && _imstring->GetCandidateLength() > 0) {
		[[NSInputManager currentInputManager] markedTextAbandoned:self];
		g_im_listener->OnStopComposing(FALSE);
	}
	if (CocoaOpWindow::CloseCurrentPopupIfNot([self window]))
		return;

	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	OP_ASSERT(!_fake_right_mouse);
	_fake_right_mouse = !!([theEvent modifierFlags] & NSControlKeyMask);
	if (_fake_right_mouse)
	{
		++_right_mouse_count;
		if (_right_mouse_count > 1)
			return;
	}
	_handling_mouse_down = TRUE;
	g_input_manager->SetMousePosition(pt);
	if (!g_input_manager->InvokeKeyDown(_fake_right_mouse?OP_KEY_MOUSE_BUTTON_2:OP_KEY_MOUSE_BUTTON_1, NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE))
	{
		if (_screen) {
			_mouse_down_x = [theEvent locationInWindow].x;
			_mouse_down_y = [self frame].size.height-[theEvent locationInWindow].y;
			g_delayed_action_handler = &s_action_handler;
			_screen->TrigMouseDown(_mouse_down_x, _mouse_down_y, _fake_right_mouse?2:1, [theEvent clickCount], ConvertToOperaModifiers([theEvent modifierFlags]));
			if (g_delayed_action_handler && g_delayed_action_handler->action)
			{
				g_input_manager->InvokeAction(g_delayed_action_handler->action, g_delayed_action_handler->context);
				// some actions are invoked later, so they need to check for existance
				// action handler here too
				if (g_delayed_action_handler)
					g_delayed_action_handler->action = NULL;
			}
			g_delayed_action_handler = NULL;
		}
		if (!_handling_mouse_down) {
			g_input_manager->InvokeKeyUp(_fake_right_mouse?OP_KEY_MOUSE_BUTTON_2:OP_KEY_MOUSE_BUTTON_1, NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
			if (_screen) {
				_screen->TrigMouseUp([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, _fake_right_mouse?2:1, ConvertToOperaModifiers([theEvent modifierFlags]));
			}
		}
	}
	else
	{
		[self releaseMouseCapture];
	}
	_handling_mouse_down = FALSE;
	_can_be_dragged = !_fake_right_mouse;
	CocoaOpWindow::SetLastMouseDown(pt.x, pt.y, theEvent);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (_imstring && _imstring->GetCandidateLength() > 0) {
		[[NSInputManager currentInputManager] markedTextAbandoned:self];
		g_im_listener->OnStopComposing(FALSE);
		if (_widgetinfo->widget_rect.Contains(pt)) // Right-click inside the widget does not invalidate
			[self invalidateAll];
	}
	if (CocoaOpWindow::CloseCurrentPopupIfNot([self window]))
		return;

	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	++_right_mouse_count;
	if (_right_mouse_count > 1)
		return;
	CocoaOpWindow::SetLastMouseDown(pt.x, pt.y, theEvent);
	g_input_manager->SetMousePosition(pt);
	if (!g_input_manager->InvokeKeyDown(OP_KEY_MOUSE_BUTTON_2, NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE))
	{
		if (_screen) {
			_screen->TrigMouseDown([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, 2, [theEvent clickCount], ConvertToOperaModifiers([theEvent modifierFlags]));
		}
	}
	else
	{
		[self releaseMouseCapture];
	}
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (_imstring && _imstring->GetCandidateLength() > 0) {
		[[NSInputManager currentInputManager] markedTextAbandoned:self];
		g_im_listener->OnStopComposing(FALSE);
	}
	if (CocoaOpWindow::CloseCurrentPopupIfNot([self window]))
		return;

	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	int buttonNumber = [theEvent buttonNumber];
	if (buttonNumber < NUM_MOUSE_BUTTONS) {
		_mouseButtonIsDown[buttonNumber] = TRUE;
		g_input_manager->SetMousePosition(pt);
		g_input_manager->InvokeKeyDown((OpKey::Code)(OP_KEY_MOUSE_BUTTON_1 + buttonNumber), NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
		if (_screen) {
			_screen->TrigMouseDown([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, buttonNumber + 1, [theEvent clickCount], ConvertToOperaModifiers([theEvent modifierFlags]));
		}
	}
}

- (void)mouseUp:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	ShiftKeyState modifiers = ConvertToOperaModifiers([theEvent modifierFlags]);
	SetEventShiftKeyState sesks(modifiers);
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (_handling_mouse_down) {
		_handling_mouse_down = FALSE;
		return;
	}
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	if (_fake_right_mouse)
	{
		--_right_mouse_count;
		if (_right_mouse_count >= 1) {
			_fake_right_mouse = FALSE;
			return;
		}
	}
	g_input_manager->SetMousePosition(pt);
	BOOL key_handled = g_input_manager->InvokeKeyUp(_fake_right_mouse?OP_KEY_MOUSE_BUTTON_2:OP_KEY_MOUSE_BUTTON_1, NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
	if (_screen) {
		if (!key_handled)
		{
			g_delayed_action_handler = &s_action_handler;
			_screen->TrigMouseUp([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, _fake_right_mouse?2:1, ConvertToOperaModifiers([theEvent modifierFlags]));
			if (g_delayed_action_handler && g_delayed_action_handler->action)
			{
				g_input_manager->InvokeAction(g_delayed_action_handler->action, g_delayed_action_handler->context);
				// some actions are invoked later, so they need to check for existance
				// action handler here too
				if (g_delayed_action_handler)
					g_delayed_action_handler->action = NULL;
			}
			g_delayed_action_handler = NULL;
		}
		else
		{
			[self releaseMouseCapture];
		}
	}
	_fake_right_mouse = FALSE;
	_can_be_dragged = FALSE;
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	--_right_mouse_count;
	if (_right_mouse_count >= 1)
		return;
	g_input_manager->SetMousePosition(pt);
	BOOL key_handled = g_input_manager->InvokeKeyUp(OP_KEY_MOUSE_BUTTON_2, NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
	if (_screen) {
		if (!_cycling)
		{
			if (!key_handled)
			{
				_screen->TrigMouseUp([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, 2, ConvertToOperaModifiers([theEvent modifierFlags]));
			}
			else
			{
				[self releaseMouseCapture];
			}

		}
		else
		{
			_screen->ReleaseMouseCapture();
			_cycling = FALSE;
		}
	}
	_cycling = FALSE;
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	int buttonNumber = [theEvent buttonNumber];
	if (buttonNumber < NUM_MOUSE_BUTTONS) {
		_mouseButtonIsDown[buttonNumber] = FALSE;
		g_input_manager->SetMousePosition(pt);
		g_input_manager->InvokeKeyUp((OpKey::Code)(OP_KEY_MOUSE_BUTTON_1 + buttonNumber), NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
		if (_screen) {
			_screen->TrigMouseUp([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, buttonNumber + 1, ConvertToOperaModifiers([theEvent modifierFlags]));
		}
	}
}

- (void)releaseOtherMouseButtons:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	for (int buttonNumber = FIRST_OTHER_MOUSE_BUTTON; buttonNumber < NUM_MOUSE_BUTTONS; buttonNumber++)
	{
		if (_mouseButtonIsDown[buttonNumber])
		{
			_mouseButtonIsDown[buttonNumber] = FALSE;
			g_input_manager->SetMousePosition(pt);
			g_input_manager->InvokeKeyUp((OpKey::Code)(OP_KEY_MOUSE_BUTTON_1 + buttonNumber), NULL, NULL, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
			if (_screen) {
				_screen->TrigMouseUp([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, buttonNumber + 1, ConvertToOperaModifiers([theEvent modifierFlags]));
			}
		}
	}
}

- (void)mouseMoved:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	NSPoint event_location;
	event_location = [theEvent locationInWindow];
	if (event_location.y < 0.0)
		event_location.y += [[[self window] screen] frame].size.height;	// Workaround for DSK-299654

	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(event_location.x+[[self window] frame].origin.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height-(event_location.y+[[self window] frame].origin.y));
	if (!CocoaOpWindow::ShouldPassMouseMovedEvents([self window], pt.x, pt.y))
		return;

	g_cocoa_drag_mgr->SetDragView(self);

	g_input_manager->SetMousePosition(pt);
	if (_screen && _opera_style != OpWindow::STYLE_TOOLTIP) {
		_screen->TrigMouseMove(event_location.x, [self frame].size.height-event_location.y, ConvertToOperaModifiers([theEvent modifierFlags]));
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	g_cocoa_drag_mgr->SetDragView(self);
	g_input_manager->SetMousePosition(pt, _fake_right_mouse ? SHIFTKEY_META : 0);

	if (_screen) {

		ShiftKeyState modifiers = 0;
		int current_x = [theEvent locationInWindow].x;
		int current_y = [self frame].size.height - [theEvent locationInWindow].y;

		if (!g_cocoa_drag_mgr->IsDragging() && _can_be_dragged && _right_mouse_count == 0)
		{
			int drag_threshold = 4;
			if (abs(current_x - _mouse_down_x) >= drag_threshold || abs(current_y - _mouse_down_y) >= drag_threshold)
			{
				_can_be_dragged = FALSE;
				_has_been_dropped = FALSE;
				_initial_drag_view = GetOpViewAt(_screen, _mouse_down_x, _mouse_down_y, true);
				_screen->TrigDragStart(_mouse_down_x, _mouse_down_y, modifiers, current_x, current_y);
			}
		}

		_screen->TrigMouseMove(current_x, current_y, ConvertToOperaModifiers([theEvent modifierFlags]));
	}
}

typedef CGFloat (*scrollingDeltaXProc)(id obj, SEL op);
typedef CGFloat (*scrollingDeltaYProc)(id obj, SEL op);

// Return TRUE if document can NOT be scrolled in the specified direction
// That means we are free to perform a swipe in that direction instead.
BOOL cantScrollInDirection(OpInputAction::Action action)
{
	OpInputAction scroll(action);
	OpInputAction getState(&scroll, OpInputAction::ACTION_GET_ACTION_STATE);
	OpInputContext * context = g_input_manager->GetKeyboardInputContext();
	if (!context) {
		context = g_application->GetInputContext();
	}
	g_input_manager->InvokeAction(&getState, context);
	return (scroll.GetActionState() & OpInputAction::STATE_DISABLED);
}

- (void)scrollWheel:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	// Bugfix: Sending shift causes VisDevListeners to fail
//	UpdateModifiers mods(ConvertToOperaModifiers([theEvent modifierFlags]) & ~SHIFTKEY_SHIFT);
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseMovedEvents([self window], pt.x, pt.y))
		return;

	if (_screen) {
		g_deltax = -[theEvent deltaX], g_deltay = -[theEvent deltaY];
		if ((g_deltax || g_deltay) && g_input_manager && g_input_manager->IsKeyDown(g_input_manager->GetGestureButton()))
		{
			_cycling = TRUE;
			g_input_manager->SetFlipping(TRUE);
			g_input_manager->InvokeAction(g_deltay <=0 ? OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE : OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE);
		}
		else
		{
			CGFloat lineHeightPixels = 32;	// We have no actual line height, but 32 seems to match Safari.
			CGFloat oldStepSize = 4;		// DISPLAY_SCROLLWHEEL_STEPSIZE was previously 4, now it's 1, so this gets us back to the old value.
			CGFloat swipeGestureSpeed = 400;
			CGFloat swipeThreshold = 0.1;	// Don't accept swipes shorter than this

			if (GetOSVersion() >= 0x1070)
			{
				// When hasPreciseScrollingDeltas returns NO, multiply the value returned by scrollingDeltaY by the line or row height.
				// Otherwise scroll by the returned amount (in points).
				if ([theEvent hasPreciseScrollingDeltas] == NO)
				{
					// The minimum value returned is 0.100006 and x32 that is 3.200192, which is rounded up to 4 pixel steps like before.
					g_deltax = -[theEvent scrollingDeltaX] * lineHeightPixels;
					g_deltay = -[theEvent scrollingDeltaY] * lineHeightPixels;
				}
				else
				{
					// Docs say that the returned value is in points, but here we assume that points == pixels.
					g_deltax = -[theEvent scrollingDeltaX];
					g_deltay = -[theEvent scrollingDeltaY];

					if (fabs(g_deltax) > fabs(g_deltay) && [NSEvent isSwipeTrackingFromScrollEventsEnabled] && [theEvent phase] != NSEventPhaseNone && _first_gesture_event)
					{
						_first_gesture_event = FALSE;
						OpInputAction::Action scrollAction = (g_deltax < 0) ? OpInputAction::ACTION_SCROLL_LEFT : OpInputAction::ACTION_SCROLL_RIGHT;
						BOOL cantScroll    = cantScrollInDirection(scrollAction);
						BOOL canSwipeLeft  = (cantScroll && scrollAction == OpInputAction::ACTION_SCROLL_LEFT);
						BOOL canSwipeRight = (cantScroll && scrollAction == OpInputAction::ACTION_SCROLL_RIGHT);

						if (cantScroll)
						{
							[theEvent trackSwipeEventWithOptions:0 dampenAmountThresholdMin:-1 max:1 usingHandler:^(CGFloat gestureAmount, NSEventPhase phase, NSBOOL isComplete, NSBOOL *stop) {
								switch (phase)
								{
									case NSEventPhaseBegan:
										g_prevAmount = gestureAmount;
										break;

									case NSEventPhaseChanged:
									{
										CGFloat delta = -(gestureAmount - g_prevAmount);
										g_prevAmount = gestureAmount;
										if (delta != 0)
										{
											delta *= swipeGestureSpeed;
											_screen->TrigMouseWheel([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, delta>0?ceil(delta):floor(delta), false, ConvertToOperaModifiers([theEvent modifierFlags]));
										}
										break;
									}

									case NSEventPhaseEnded:
										if (gestureAmount > swipeThreshold && canSwipeLeft && cantScrollInDirection(scrollAction))
											g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_LEFT, NULL, SHIFTKEY_NONE, FALSE, TRUE);
										else if (gestureAmount < -swipeThreshold && canSwipeRight && cantScrollInDirection(scrollAction))
											g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_RIGHT, NULL, SHIFTKEY_NONE, FALSE, TRUE);
										return;
								}
							}];
						}
					}
				}

				switch ([theEvent momentumPhase])
				{
				case NSEventPhaseBegan:
					_momentum_active = TRUE;
					break;
				case NSEventPhaseChanged:
					if (!_momentum_active)
					{
						g_deltax = 0;
						g_deltay = 0;
					}
					break;
				default:
					_momentum_active = FALSE;
					break;
				}
			}
			else
			{
				// A regular mouse event has no deviceDeltaY, so use the normal deltaY
				if ([theEvent subtype] == NSMouseEventSubtype)
				{
					// The minimum value returned is 0.100006 and x32 that is 3.200192, which is rounded up to 4 pixel steps like before.
					g_deltax *= lineHeightPixels;
					g_deltay *= lineHeightPixels;
				}
				else
				{
					// Note: deviceDeltaY is private, so check specifically that it exists
					if ([theEvent respondsToSelector:@selector(deviceDeltaY)])
					{
						// Docs say that the returned value is in points, but here we assume that points == pixels.
						g_deltax = -((scrollingDeltaXProc)objc_msgSend)(theEvent, @selector(deviceDeltaX));
						g_deltay = -((scrollingDeltaYProc)objc_msgSend)(theEvent, @selector(deviceDeltaY));
					}
					else
					{
						// In case the selector isn't available, just use deltaY directly, scaled by the step size from previous Opera version.
						g_deltax *= oldStepSize;
						g_deltay *= oldStepSize;
					}
				}
			}

			if (g_deltax)
				_screen->TrigMouseWheel([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, g_deltax>0?ceil(g_deltax):floor(g_deltax), false, ConvertToOperaModifiers([theEvent modifierFlags]));
			if (g_deltay)
				_screen->TrigMouseWheel([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, g_deltay>0?ceil(g_deltay):floor(g_deltay), true, ConvertToOperaModifiers([theEvent modifierFlags]));
		}
		g_component_manager->RunSlice();
	}
}

- (void)stopScrollMomentum
{
	_momentum_active = FALSE;
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

	g_input_manager->SetMousePosition(pt);
	if (_screen) {
		_screen->TrigMouseMove([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, ConvertToOperaModifiers([theEvent modifierFlags]));
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	if (!CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;

}

- (void)mouseEntered:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (![[NSApplication sharedApplication] isActive] || !CocoaOpWindow::ShouldPassMouseMovedEvents([self window], pt.x, pt.y))
		return;

	g_input_manager->SetMousePosition(pt);
	if (_screen) {
		_screen->TrigMouseMove([theEvent locationInWindow].x, [self frame].size.height-[theEvent locationInWindow].y, ConvertToOperaModifiers([theEvent modifierFlags]));
	}
}

- (void)mouseExited:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	OpPoint pt(EVENT_OPPOINT_X, EVENT_OPPOINT_Y));
	if (![[NSApplication sharedApplication] isActive] || !CocoaOpWindow::ShouldPassMouseEvents([self window]))
		return;
	if (gMouseCursor)
		gMouseCursor->SetCursor(CURSOR_DEFAULT_ARROW);
	if (g_input_manager)
		g_input_manager->SetMousePosition(pt);
}

#define FAKE_SHIFTKEY_CAPS_LOCK 0x40
#if defined(OP_KEY_CONTEXT_MENU_ENABLED) && !defined(OP_KEY_MAC_CTRL_RIGHT_ENABLED)
// Temporary workaround: OP_KEY_CONTEXT_MENU cannot actually be disabled, so we reuse it.
#define OP_KEY_MAC_CTRL_RIGHT OP_KEY_CONTEXT_MENU
#define OP_KEY_MAC_CTRL_RIGHT_ENABLED 1
#endif

- (void)flagsChanged:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	MacKeyData key_data;
	MacOpPlatformKeyEventData *key_event_data = NULL;
	key_data.m_key_code = [theEvent keyCode];
	key_data.m_sent_char = 0;
	
	ShiftKeyState shift_keys = ConvertToOperaModifiers([theEvent modifierFlags]);
	static ShiftKeyState s_old_shift_keys = 0;
	if ([theEvent modifierFlags] & NSAlphaShiftKeyMask) {
		// Not really a proper Opera modifier, but used here for convenience
		shift_keys |= FAKE_SHIFTKEY_CAPS_LOCK;
	}

	// Handle the modifier keys and send the correct event when they are pressed
	ShiftKeyState state_change = shift_keys ^ s_old_shift_keys;
	if (state_change & SHIFTKEY_CTRL)
	{
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		OpVirtualKey key = OP_KEY_MAC_CTRL;
#if defined(OP_KEY_MAC_CTRL_LEFT_ENABLED) && defined(OP_KEY_MAC_CTRL_RIGHT_ENABLED)
		unsigned short kc = [theEvent keyCode];
		if (kc == kVK_Command_Right)
			key = OP_KEY_MAC_CTRL_RIGHT;
		else if (kc == kVK_Command)
			key = OP_KEY_MAC_CTRL_LEFT;

#endif
		if (shift_keys & SHIFTKEY_CTRL)
			g_input_manager->InvokeKeyDown(key, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		else
			g_input_manager->InvokeKeyUp(key, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
	}
	if (state_change & SHIFTKEY_SHIFT)
	{
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		if (shift_keys & SHIFTKEY_SHIFT)
			g_input_manager->InvokeKeyDown(OP_KEY_SHIFT, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		else
			g_input_manager->InvokeKeyUp(OP_KEY_SHIFT, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
	}
	if (state_change & SHIFTKEY_ALT)
	{
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		if (shift_keys & SHIFTKEY_ALT)
			g_input_manager->InvokeKeyDown(OP_KEY_ALT, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		else
			g_input_manager->InvokeKeyUp(OP_KEY_ALT, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
	}
	if (state_change & SHIFTKEY_META)
	{
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		if (shift_keys & SHIFTKEY_META)
			g_input_manager->InvokeKeyDown(OP_KEY_CTRL, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		else
			g_input_manager->InvokeKeyUp(OP_KEY_CTRL, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
	}
	if (state_change & FAKE_SHIFTKEY_CAPS_LOCK)
	{
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		if (shift_keys & FAKE_SHIFTKEY_CAPS_LOCK)
			g_input_manager->InvokeKeyDown(OP_KEY_CAPS_LOCK, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		else
			g_input_manager->InvokeKeyUp(OP_KEY_CAPS_LOCK, NULL, 0, key_event_data, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
	}
	s_old_shift_keys = shift_keys;
}

#if defined(OP_KEY_CONTEXT_MENU_ENABLED) && defined(OP_KEY_MAC_CTRL_RIGHT_ENABLED)
#undef OP_KEY_MAC_CTRL_RIGHT
#undef OP_KEY_MAC_CTRL_RIGHT_ENABLED
#endif // OP_KEY_CONTEXT_MENU_ENABLED

- (void)keyDown:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	[NSCursor setHiddenUntilMouseMoves:TRUE];

	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	NSString *str = [theEvent characters];
	int len = [str length];
	NSString *cim_str = [theEvent charactersIgnoringModifiers];
	int len2 = [cim_str length];

	if (len && len2)
	{
        g_sent_char = UniKey2Char([str characterAtIndex:0], [theEvent modifierFlags] & NSControlKeyMask);
		g_virtual_key_code = UniKey2OpKey([cim_str characterAtIndex:0], [theEvent keyCode]);
		g_nonshift_modifiers = ConvertToOperaModifiers([theEvent modifierFlags]);
		if (!ShouldHideMousePointer(g_virtual_key_code, g_nonshift_modifiers))
			[NSCursor setHiddenUntilMouseMoves:FALSE];
		g_key_code = [theEvent keyCode];
		_ime_hide_enter = NO;
		if (g_virtual_key_code == OP_KEY_ENTER && g_im_listener && g_im_listener->IsIMEActive())
			_ime_hide_enter = YES;
		if (_ime_hide_enter == NO)
		{
			MacKeyData key_data;
			MacOpPlatformKeyEventData *key_event_data = NULL;
			key_data.m_key_code = g_key_code;
			key_data.m_sent_char = g_sent_char;
			OpString8 utf8key;
			if (g_sent_char)
				utf8key.SetUTF8FromUTF16(&g_sent_char, 1);
			OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
			g_input_manager->InvokeKeyDown(g_virtual_key_code, utf8key.CStr(), utf8key.Length(), key_event_data, g_nonshift_modifiers, FALSE, OpKey::LOCATION_STANDARD, TRUE);
			OpPlatformKeyEventData::DecRef(key_event_data);
		}
	}
	[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
}

- (void)keyUp:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	if (_ime_hide_enter == YES)
		return;
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	NSString *str = [theEvent characters];
	int len = [str length];
	NSString *cim_str = [theEvent charactersIgnoringModifiers];
	int len2 = [cim_str length];

	if (len && len2 && ![self hasMarkedText])
	{
        g_sent_char = UniKey2Char([str characterAtIndex:0], [theEvent modifierFlags] & NSControlKeyMask);
		g_virtual_key_code = UniKey2OpKey([cim_str characterAtIndex:0], [theEvent keyCode]);
		g_key_code = [theEvent keyCode];
		MacKeyData key_data;
		MacOpPlatformKeyEventData *key_event_data = NULL;
		key_data.m_key_code = g_key_code;
		key_data.m_sent_char = g_sent_char;
		OpString8 utf8key;
		if (g_sent_char)
			utf8key.SetUTF8FromUTF16(&g_sent_char, 1);
		OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
		g_input_manager->InvokeKeyUp(g_virtual_key_code, utf8key.CStr(), utf8key.Length(), key_event_data, ConvertToOperaModifiers([theEvent modifierFlags]), FALSE, OpKey::LOCATION_STANDARD, TRUE);
		OpPlatformKeyEventData::DecRef(key_event_data);
        g_virtual_key_code = OP_KEY_INVALID;
        g_sent_char = 0;
	}
}

- (NSBOOL)performKeyEquivalent:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	if (![[theEvent characters] length])
		return FALSE;

	[NSCursor setHiddenUntilMouseMoves:TRUE];

	// Intercept the key events and send them directly to the bottom line input window if it is showing
	// so that the navigation in the composing pop-up actually works properly
	if ([[BottomLineInput bottomLineInput] isVisible])
	{
		[[BottomLineInput bottomLineInput] performKeyEquivalent:theEvent];
		return NO;
	}

	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));

	// Give input method chance to handle event.
	if (g_im_listener && g_im_listener->IsIMEActive())
	{
		_ime_sel_changed = NO;
		[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
		if (_ime_sel_changed)
			return YES;
	}

	OpInputContext *input_context = g_input_manager->GetKeyboardInputContext();

	static bool s_avoid_sending_key_to_the_menu = false;

	// makes the menu blink
	BOOL handled = NO;
	if (![theEvent isARepeat] && !s_avoid_sending_key_to_the_menu)
	{
		s_avoid_sending_key_to_the_menu = true;
		handled = [[NSApp mainMenu] performKeyEquivalent:theEvent];
		s_avoid_sending_key_to_the_menu = false;
	}

	// Fix for DSK-313248: Blocking dialogs are not able to handle menus
	// Remove this code when all blocking dialogs are eradicated from Opera
	if (handled && [(OperaNSWindow*)[self window] getOperaStyle] == OpWindow::STYLE_BLOCKING_DIALOG)
		handled = NO;

	// Try to match with an Opera shortcut
	BOOL match = FALSE;

	for (;input_context && !match; input_context = input_context->GetParentInputContext())
	{
		ShortcutContext* shortcut_context = g_input_manager->GetShortcutContextFromActionMethodAndName(OpInputAction::METHOD_KEYBOARD, input_context->GetInputContextName());

		if (!shortcut_context)
		{
			continue;
		}

		if(shortcut_context)
		{
			INT32 count = shortcut_context->GetShortcutActionCount();

			Shortcut shortcut;
			uni_char c = [[theEvent characters] characterAtIndex:0];
			if ((!uni_isascii(c) || c<32) && [[theEvent charactersIgnoringModifiers] length] > 0)
				c = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];
			shortcut.Set(UniKey2OpKey(c, [theEvent keyCode]), ConvertToOperaModifiers([theEvent modifierFlags]), [[theEvent characters] characterAtIndex:0]);

			for (INT32 shortcut_action_pos = 0; shortcut_action_pos < count; shortcut_action_pos++)
			{
				ShortcutAction* shortcut_action = shortcut_context->GetShortcutActionFromIndex(shortcut_action_pos);

				if (shortcut_action->GetShortcutCount() != 1)
					continue;

				// Check if the current key sequence is a possible match for this short cut. Skip otherwise.
				if (shortcut_action->GetShortcutByIndex(0)->Equals(shortcut))
				{
					match = TRUE;
					break;
				}
			}
		}
	}

	short carbonMods = 0;
	if ([theEvent modifierFlags] & NSShiftKeyMask)
		carbonMods |= shiftKey;
	if ([theEvent modifierFlags] & NSAlternateKeyMask)
		carbonMods |= optionKey;
	// Do not set other modifer flags. If you set the cmd key, you get the same result as [[theEvent characters] characterAtIndex:0]

	if ([theEvent type] == NSKeyDown)
	{
		g_sent_char = UniKey2Char([[theEvent characters] characterAtIndex:0], [theEvent modifierFlags] & NSControlKeyMask);
		uni_char key = 0;
		NSString *cim_str = [theEvent charactersIgnoringModifiers];
		int len = [cim_str length];
		uni_char cim = len?[cim_str characterAtIndex:0]:0;
		uni_char c = [[theEvent characters] characterAtIndex:0];

		g_nonshift_modifiers = ConvertToOperaModifiers([theEvent modifierFlags]);

		// There is some heuristic magic going on here, strap yourself in
		// First, if modifiers do not seem to affect the letter (say, tab,
		// F2, backspace, down arrow or a numpad key), always leave the modifiers in.
		// If this is a modified alpha character send all the modifiers and the uppercase letter (cim).
		// If this is a special character, such as { or #, send the special character but no
		// modifiers except ctrl, since the character already shows the effect of the other modifiers.
		if (!uni_isalpha(cim) && !uni_isalpha(c) && [theEvent modifierFlags] & NSCommandKeyMask &&
			OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey([theEvent keyCode], key, carbonMods>>8)))
		{
			g_virtual_key_code = CarbonUniKey2OpKey(key);
			// Cannot compare the OpKey::Codes as they may well end up being OP_KEY_UNICODE for 2 different reasons.
			// On the other hand, key and [[theEvent characters] characterAtIndex:0] are not really directly comparable
			// as many identical keys (tab, arrows, insert, F3) will have different numbers in Carbon and Cocoa.
			// This test ties to determine if modifiers have a net effect.
			// Could be simplified if UKeyTranslate were to return NS key numbers.
			unichar orig = [[theEvent characters] characterAtIndex:0];
			if (key >= 32 && key != kDeleteCharCode && (key != orig || key != cim))
			{
				g_nonshift_modifiers &= ~(SHIFTKEY_ALT|SHIFTKEY_SHIFT);
				g_sent_char = key;
			}
		}
		else
		{
			if ([theEvent modifierFlags] & NSCommandKeyMask && OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey([theEvent keyCode], key, cmdKey>>8)))
				g_virtual_key_code = CarbonUniKey2OpKey(key);
			else
				g_virtual_key_code = UniKey2OpKey((uni_isascii(c) && c>=32 || !cim)?c:cim, [theEvent keyCode]);
		}

		// Prepare the fallback in case the original cannot be matched.
		// For instance, { would have the same OpKey::Code as F12 (123)
		// Only set it for printables.
		// Do not test on g_virtual_key_code==OP_KEY_UNICODE, we need it to be sent for things like hyphen (minus),
		// square brackets and backslash, which are not translated properly from the standard_keyboard.ini.
		// You can add the test once ShortcutAction::AddShortcutsFromString handles all relevant keys.
		OpString8 utf8key;
		if (g_sent_char && (!uni_isalpha(g_sent_char) || !([theEvent modifierFlags] & NSCommandKeyMask)))
			utf8key.SetUTF8FromUTF16(&g_sent_char, 1);

		g_key_code = [theEvent keyCode];
		
		if (g_nonshift_modifiers == SHIFTKEY_CTRL)
			[NSCursor setHiddenUntilMouseMoves:FALSE];
		
		if (!handled)
		{
			MacKeyData key_data;
			MacOpPlatformKeyEventData *key_event_data = NULL;
			key_data.m_key_code = g_key_code;
			key_data.m_sent_char = [[theEvent characters] characterAtIndex:0];
			OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
			g_input_manager->InvokeKeyDown(g_virtual_key_code, NULL, 0, key_event_data, g_nonshift_modifiers, FALSE, OpKey::LOCATION_STANDARD, TRUE);
			// Need to set handled for things like fn+@
			// Need to not set handled for things like ^F2 (focus menu).
			handled |= (g_input_manager->InvokeKeyPressed(g_virtual_key_code, utf8key.CStr(), utf8key.Length(), g_nonshift_modifiers, FALSE, TRUE) && ([theEvent modifierFlags] & (NSFunctionKeyMask|NSCommandKeyMask|NSControlKeyMask)) == NSFunctionKeyMask);
			OpPlatformKeyEventData::DecRef(key_event_data);
		}
	}
	else if ([theEvent type] == NSKeyUp)
	{
		if (!handled)
		{
			MacKeyData key_data;
			MacOpPlatformKeyEventData *key_event_data = NULL;
			key_data.m_key_code = g_key_code;
			key_data.m_sent_char = g_sent_char;
			OpPlatformKeyEventData::Create((OpPlatformKeyEventData**)&key_event_data, (void*)&key_data);
			g_input_manager->InvokeKeyUp(g_virtual_key_code, NULL, 0, key_event_data, g_nonshift_modifiers, FALSE, OpKey::LOCATION_STANDARD, TRUE);
			OpPlatformKeyEventData::DecRef(key_event_data);
		}
	}

	// Return false to have the system shortcuts processed by NSApp if Opera isn't going to handle the shortcut.
	return (handled || s_avoid_sending_key_to_the_menu) ? YES : match;
}

- (BOOL)tryToOpenWebloc:(const uni_char*)urlString view:(OpView*)opView
{
	const int webloc_len = 7; // ".webloc"
	int length = uni_strlen(urlString);
	if (length > webloc_len && uni_stricmp(urlString + length - webloc_len, UNI_L(".webloc")) == 0)
	{
		NSString *inFile = [NSString stringWithCharacters:(const unichar*)urlString length:uni_strlen(urlString)];
		NSDictionary* plist = [[NSDictionary alloc] initWithContentsOfFile:inFile];
		if (plist)
		{
			NSURL* nsurl = [NSURL URLWithString:[plist objectForKey:@"URL"]];
			[plist release];
			if (nsurl)
			{
				NSString* ns_path = [nsurl absoluteString];
				OpString op_path;
				if (ns_path && op_path.Reserve([ns_path length] + 1))
				{
					[ns_path getCharacters:(unichar*)op_path.CStr() range:NSMakeRange(0, [ns_path length])];
					op_path.CStr()[[ns_path length]] = '\0';
					[self openUrl:op_path.CStr() view:opView newTab:TRUE];
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

- (void)openUrl:(const uni_char*)urlString view:(OpView*)opView newTab:(BOOL)openInNewTab
{
	if ([self tryToOpenWebloc:urlString view:opView])
		return;

	// Open URL normally
	DesktopOpWindow* root_win = (DesktopOpWindow*)opView->GetRootWindow();
	if (root_win)
	{
		DesktopWindow* desk_win = root_win->GetDesktopWindow();
		if (desk_win)
		{
			OpenURLSetting setting;
			BOOL open_in_background = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenDraggedLinkInBackground);
			WindowCommanderProxy::InitOpenURLSetting(setting, urlString, FALSE, open_in_background, FALSE, desk_win, -1, FALSE);
			setting.m_new_page = openInNewTab ? YES : NO;
			g_application->OpenURL(setting);
		}
	}
}

- (void)addSublayer:(CALayer *)layer
{
	[_plugin_clip_layer addSublayer:layer];
}

- (void)setValidateStarted:(BOOL)validateStarted
{
	_validate_started = validateStarted;
}

// This is called for each plugin layer after Validate and drawing begins.
// Each layer is assigned a sequential Z position so that they are
// layered correctly on top of each other
- (void)updatePluginZPosition:(CALayer *)layer
{
	// Check if this is the first call after Validate is called
	// If so reset the Z position of the layer
	if (_validate_started)
	{
		_validate_started = FALSE;
		_plugin_z_order = 1;
	}

    [CATransaction begin];
	[CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
	[layer setZPosition:_plugin_z_order];
    [CATransaction commit];	// Assign this layer the current Z position and increment
	_plugin_z_order++;
}

- (void)setNeedsDisplayForLayers:(BOOL)needsDisplay
{
	if (needsDisplay)
	{
		[_content_layer setNeedsDisplay];
		[_background_layer setNeedsDisplay];
	}
    [CATransaction begin];
	// Disable implicit animations.
    [CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
    for (MacOpPluginLayer *plugin_layer in _plugin_clip_layer.sublayers)
    {
        [plugin_layer setFrameFromDelayed];
    }
    [CATransaction commit];
}

- (void)beginGestureWithEvent:(NSEvent *)theEvent
{
	_performing_gesture = TRUE;
	_first_gesture_event = TRUE;
}

- (void)endGestureWithEvent:(NSEvent *)theEvent
{
	_performing_gesture = FALSE;
	_first_gesture_event = FALSE;
}

- (void)swipeWithEvent:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	if ([theEvent deltaX] > 0.0)
		g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_LEFT, NULL, SHIFTKEY_NONE, FALSE, TRUE);
	else if ([theEvent deltaX] < 0.0)
		g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_RIGHT, NULL, SHIFTKEY_NONE, FALSE, TRUE);
	else
	{
		if ([theEvent deltaY] > 0.0)
			g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_UP, NULL, SHIFTKEY_NONE, FALSE, TRUE);
		else if ([theEvent deltaY] < 0.0)
			g_input_manager->InvokeKeyPressed(OP_KEY_SWIPE_DOWN, NULL, SHIFTKEY_NONE, FALSE, TRUE);
	}
}

- (void)rotateWithEvent:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	ShiftKeyState shift_keys = ConvertToOperaModifiers([theEvent modifierFlags]);
	SetEventShiftKeyState sesks(shift_keys);

	if ((shift_keys & (SHIFTKEY_CTRL | SHIFTKEY_ALT | SHIFTKEY_SHIFT)) == (SHIFTKEY_CTRL | SHIFTKEY_ALT | SHIFTKEY_SHIFT))
		[self setAlpha:(CocoaOpWindow::s_alpha - ([theEvent rotation] / 100.0))];
}

- (void)magnifyWithEvent:(NSEvent *)theEvent
{
	RESET_OPENGL_CONTEXT
	
	SetEventShiftKeyState sesks(ConvertToOperaModifiers([theEvent modifierFlags]));
	float mag = [theEvent deltaZ];

	int adj = sqrt(fabs(mag));
	// Avoid small movements
	if (adj < 1)
		return;

	_scale_adjustments += adj;

	DesktopWindow *desktop_window = g_application->GetActiveDesktopWindow(FALSE);
	if (desktop_window && desktop_window->GetWindowCommander())
	{
		INT32 curr_scale = desktop_window->GetWindowCommander()->GetScale();

//		printf("Delta: %f, Scale: %d, Curr Scale %d, Scale Adjustments:%d, PrevScale:%d\n", mag, adj, curr_scale, _scale_adjustments, _prev_scale);

		// This trys to make the pinch "snap" to 100%
		if (curr_scale == 100 && _scale_adjustments < 15)
		{
//			printf("100 stuck\n");
			return;
		}
		else if ((_prev_scale < 100 && curr_scale > 100) || (_prev_scale > 100 && curr_scale < 100))
		{
//			printf("100 set\n");
			desktop_window->GetWindowCommander()->SetScale(100);
		}
		else
		{
//			printf("Normal Adjustment\n");
			if ([theEvent deltaZ] > 0.0)
				g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_IN, adj);
			else
				g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_OUT, adj);
		}

		_prev_scale = curr_scale;
		_scale_adjustments = 0;
	}
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
- (void)updateContentsScales
{
	if (GetOSVersion() >= 0x1070)
	{
		if (_screen)
		{
			float pixelscale = float(_screen->GetPixelScale()) / 100.0f;
			[_background_layer setContentsScale:pixelscale];
			[_comp_layer setContentsScale:pixelscale];
			[_content_layer setContentsScale:pixelscale];
			_screen->UpdatePainterScaleFactor();
		}
	}
}

- (void)viewDidChangeBackingProperties
{
	[super viewDidChangeBackingProperties];

	[self updateContentsScales];
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#pragma mark <NSTextInput> protocol

- (void) insertText:(id)aString
{
	NSString *string = NULL;
	if ([aString isKindOfClass:[NSAttributedString class]])
		string = [aString string];
	else if ([aString isKindOfClass:[NSString class]])
		string = aString;

	if ([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible])
	{
		[[BottomLineInput bottomLineInput] sendText:string];
		[[BottomLineInput bottomLineInput] show:FALSE];
	}

	if (string)
	{
		int len = [string length];
		if (g_im_listener && g_im_listener->IsIMEActive())
		{
			uni_char* str = new uni_char[[string length]+1];
			[string getCharacters:(unichar*)str range:NSMakeRange(0, [string length])];
			_imstring->Set(str, [string length]);
			_imstring->SetCandidate(0, [string length]);
			_imstring->SetCaretPos([string length]);
			delete [] str;
			g_im_listener->OnCompose();
			g_im_listener->OnCommitResult();
			g_im_listener->OnStopComposing(FALSE);
			return;
		}
		if (len)
		{
			g_input_manager->InvokeKeyPressed(g_virtual_key_code, [string UTF8String], strlen([string UTF8String]), g_nonshift_modifiers, FALSE, TRUE);
		}
	}
}

- (void) doCommandBySelector:(SEL)aSelector
{
	if (g_im_listener && g_im_listener->IsIMEActive())
		g_im_listener->OnStopComposing(TRUE);

	OpString8 utf8key;
	if (g_sent_char)
		utf8key.SetUTF8FromUTF16(&g_sent_char, 1);
	g_input_manager->InvokeKeyPressed(g_virtual_key_code, utf8key.CStr(), utf8key.Length(), g_nonshift_modifiers, FALSE, TRUE);
}

- (void) setMarkedText:(id)aString selectedRange:(NSRange)selRange
{
	NSString *string = NULL;
	if ([aString isKindOfClass:[NSAttributedString class]])
		string = [aString string];
	else if ([aString isKindOfClass:[NSString class]])
		string = aString;

	if (([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible]) && ([string length] == 0))
	{
		// Unset itself as the OperaNSView to avoid infinte loop
		[[BottomLineInput bottomLineInput] setOperaNSView:nil];
		[[BottomLineInput bottomLineInput] show:FALSE];
	}
	else
	{
		// Only should the bottom line input after the setMarkedText call to avoid blinking of the field
		if ([[BottomLineInput bottomLineInput] shouldBeShown])
		{
			[[BottomLineInput bottomLineInput] show:TRUE];
			[[BottomLineInput bottomLineInput] setOperaNSView:self];
			[[BottomLineInput bottomLineInput] setMarkedText:aString selectedRange:selRange];
			return;
		}
		else if ([[BottomLineInput bottomLineInput] isVisible])
		{
			[[BottomLineInput bottomLineInput] setMarkedText:aString selectedRange:selRange];
			return;
		}
	}

	if(g_im_listener && string) {
		if (!_imstring) {
			_imstring = OP_NEW(OpInputMethodString, ());
			_imstring->SetShowCaret(false);
		}
		uni_char* str = new uni_char[[string length]+1];
		[string getCharacters:(unichar*)str range:NSMakeRange(0, [string length])];
		_imstring->Set(str, [string length]);
		if (selRange.length)
			_imstring->SetCandidate(selRange.location, selRange.length);
		else
			_imstring->SetCandidate(0, [string length]);
		delete [] str;
		if (g_im_listener->IsIMEActive())
			g_im_listener->OnStopComposing(TRUE);
		if ([string length]) {
			g_im_listener->OnStartComposing(_imstring, IM_COMPOSE_NEW);
			*_widgetinfo = g_im_listener->OnCompose();
		}
		_ime_sel_changed = YES;
	}
}

- (void) unmarkText
{
	if(g_im_listener)
		g_im_listener->OnStopComposing(TRUE);
}

- (NSBOOL) hasMarkedText
{
	return (g_im_listener && g_im_listener->IsIMEActive() && _imstring && (_imstring->GetCandidateLength() > 0));
}

- (NSInteger) conversationIdentifier
{
	return (NSInteger)self;
}

- (NSAttributedString *) attributedSubstringFromRange:(NSRange)theRange
{
	const uni_char* s = _imstring ? _imstring->Get() : NULL;
	if (g_im_listener && g_im_listener->IsIMEActive() && s) {
		NSString *str = [[NSString alloc] initWithCharacters:(const unichar*)s+theRange.location length:theRange.length];
		return [[NSAttributedString alloc] initWithString:[str autorelease]];
	}
	return nil;
}

- (NSRange) markedRange
{
	if ([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible])
		return [[BottomLineInput bottomLineInput] getMarkedRange];
	else if (_imstring)
		return NSMakeRange(_imstring->GetCandidatePos(), _imstring->GetCandidateLength());
	return NSMakeRange(0, 0);
}

- (NSRange) selectedRange
{
	return NSMakeRange(0, 0);
}

- (NSRect) firstRectForCharacterRange:(NSRange)theRange
{
	if ([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible])
	{
		// The values used here to place the window were derived through testing.
		// This was the best numbers that worked when using two monitors
		return NSMakeRect([[BottomLineInput bottomLineInput] frame].origin.x + 50 + (10 * theRange.location), [[BottomLineInput bottomLineInput] frame].origin.y + 10, 14, 14);
	}

	return NSMakeRect(_widgetinfo->rect.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height-_widgetinfo->rect.y-_widgetinfo->rect.height, _widgetinfo->rect.width, _widgetinfo->rect.height);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
// FIXME: IMPLEMENTME
	return 0;
}

- (NSArray*) validAttributesForMarkedText
{
	return nil;
}

@end

// Invoked when the dragged image enters destination bounds or frame; delegate returns dragging operation to perform.
@implementation OperaNSView(NSDraggingDestination)
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	NSPoint where = [sender draggingLocation];
	where = [self convertPoint:where fromView:nil];
	where.y = [self frame].size.height - where.y;
	MacOpView* view = GetOpViewAt(_screen, where.x, where.y, true);
	RETURN_VALUE_IF_NULL(view, NSDragOperationNone);

	OpDragListener* listener = view->GetDragListener();
	RETURN_VALUE_IF_NULL(listener, NSDragOperationNone);

	OpPoint pt;
	pt = view->ConvertToScreen(pt);
	pt.x = where.x - pt.x + [[self window] frame].origin.x;
	pt.y = where.y - pt.y + [[[NSScreen screens] objectAtIndex:0] frame].size.height-[[self window] frame].origin.y-[self frame].size.height;

	DesktopDragObject* drag_object = NULL;
	id pboard = [sender draggingPasteboard];
	if (g_cocoa_drag_mgr->IsDragging()) // already dragging an object
	{
		drag_object = (DesktopDragObject*)g_cocoa_drag_mgr->GetDragObject();
		RETURN_VALUE_IF_NULL(drag_object, NSDragOperationNone);

		_screen->TrigDragMove(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_last_drag = pt;

		DropType drop = drag_object->GetVisualDropType();
		if (drop == DROP_COPY)
			return NSDragOperationCopy;
		if (drop == DROP_MOVE)
			return NSDragOperationGeneric;
		return NSDragOperationNone;
	}
	else if ([[pboard types] containsObject:NSFilenamesPboardType]) // dragging file(s)
	{
		drag_object = OP_NEW(DesktopDragObject, (OpTypedObject::UNKNOWN_TYPE)); // hack
		NSEnumerator* files = [[pboard propertyListForType:NSFilenamesPboardType] objectEnumerator];
		NSString* file;
		while ((file = [files nextObject]) != nil)
		{
			OpString fpath;
			int len = [file length];
			if (fpath.Reserve(len+1))
			{
				[file getCharacters:(unichar*)fpath.CStr()];
				fpath.CStr()[len] = 0;
				drag_object->SetData(fpath.CStr(), UNI_L("text/x-opera-files"), TRUE, FALSE);
			}
		}
		drag_object->SetDropType(DROP_COPY);
		drag_object->SetVisualDropType(DROP_COPY);
		drag_object->SetEffectsAllowed(DROP_COPY);
		g_cocoa_drag_mgr->SetDragObject(drag_object);
		_screen->TrigDragEnter(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_has_been_dropped = FALSE;
		return NSDragOperationGeneric;
	}
	else if ([[pboard types] containsObject:NSURLPboardType]) // dragging URL(s) - like dragging an image from Safari into Opera
	{
		drag_object = OP_NEW(DesktopDragObject, (OpTypedObject::UNKNOWN_TYPE)); // hack
		NSURL* url = [NSURL URLFromPasteboard:pboard];
		OpString urlstr;
		int len = [[url absoluteString] length];
		urlstr.Reserve(len+1);
		[[url absoluteString] getCharacters:(unichar*)urlstr.CStr()];
		urlstr[len] = 0;
		drag_object->SetURL(urlstr.CStr());
		if ([[pboard types] containsObject:NSStringPboardType]) {
			NSData * data = [pboard dataForType:NSStringPboardType];
			urlstr.SetFromUTF8((char*)[data bytes], [data length]);
			DragDrop_Data_Utils::SetText(drag_object, urlstr.CStr());
		}
		drag_object->SetDropType(DROP_COPY);
		drag_object->SetVisualDropType(DROP_COPY);
		drag_object->SetEffectsAllowed(DROP_COPY);
		g_cocoa_drag_mgr->SetDragObject(drag_object);
		_screen->TrigDragEnter(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_has_been_dropped = FALSE;
		return NSDragOperationGeneric;
	}
	else if ([[pboard types] containsObject:NSStringPboardType]) // dragging a string
	{
		drag_object = OP_NEW(DesktopDragObject, (OpTypedObject::DRAG_TYPE_TEXT));
		NSData * data = [pboard dataForType:NSStringPboardType];
		OpString text;
		text.SetFromUTF8((char*)[data bytes], [data length]);
		drag_object->SetDropType(DROP_COPY);
		drag_object->SetVisualDropType(DROP_COPY);
		drag_object->SetEffectsAllowed(DROP_COPY);
		DragDrop_Data_Utils::SetText(drag_object, text.CStr());
		g_cocoa_drag_mgr->SetDragObject(drag_object);
		_screen->TrigDragEnter(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_has_been_dropped = FALSE;
		return NSDragOperationCopy;
	}

	return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
	NSPoint where = [sender draggingLocation];
	where = [self convertPoint:where fromView:nil];
	where.y = [self frame].size.height - where.y;

	MacOpView* view = GetOpViewAt(_screen, where.x, where.y, true);
	RETURN_VALUE_IF_NULL(view, NSDragOperationNone);

	OpDragListener* listener = view->GetDragListener();
	RETURN_VALUE_IF_NULL(listener, NSDragOperationNone);

	OpPoint pt;
	pt = view->ConvertToScreen(pt);
	pt.x = where.x - pt.x + [[self window] frame].origin.x;
	pt.y = where.y - pt.y + [[[NSScreen screens] objectAtIndex:0] frame].size.height-[[self window] frame].origin.y-[self frame].size.height;

	DesktopDragObject* drag_object = (DesktopDragObject*)g_cocoa_drag_mgr->GetDragObject();
	RETURN_VALUE_IF_NULL(drag_object, NSDragOperationNone);

	if (pt.x != _last_drag.x || pt.y != _last_drag.y)
	{
		_screen->TrigDragMove(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_last_drag = pt;
	}

	DropType drop = drag_object->GetVisualDropType();
	if (drop == DROP_COPY)
		return NSDragOperationCopy;
	if (drop == DROP_MOVE)
		return NSDragOperationGeneric;
	if (drop == DROP_LINK)
		return NSDragOperationLink;

	// Allow external files and URL links being dropped
	if (drag_object->GetType() == OpTypedObject::UNKNOWN_TYPE)
	{
		if (DragDrop_Data_Utils::HasFiles(drag_object))
			return NSDragOperationCopy;
		if (DragDrop_Data_Utils::HasURL(drag_object))
			return NSDragOperationLink;
	}

	if (DragDrop_Data_Utils::HasURL(drag_object))
	{
		// Allow dropping bookmarks on the page
		if (drag_object->GetType() == OpTypedObject::DRAG_TYPE_BOOKMARK && DragDrop_Data_Utils::HasURL(drag_object) && !view->GetParentWindow())
			return NSDragOperationLink;

		// Allow URL links from Opera being dropped on another view
		if (drag_object->GetType() == OpTypedObject::DRAG_TYPE_LINK && (view != _initial_drag_view || g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag) & DRAG_SAMEWINDOW))
			return NSDragOperationLink;
	}

	return NSDragOperationNone;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
	NSPoint where = [sender draggingLocation];
	where = [self convertPoint:where fromView:nil];
	where.y = [self frame].size.height - where.y;

	if (!NSPointInRect(where, [self bounds]))
		_screen->TrigDragLeave(g_op_system_info->GetShiftKeyState());
}

- (void)draggingEnded:(id <NSDraggingInfo>)sender
{
	if (!g_cocoa_drag_mgr->IsDragging() || _has_been_dropped)
		return;

	NSPoint where = [sender draggingLocation];
	where = [self convertPoint:where fromView:nil];
	where.y = [self frame].size.height - where.y;

	if (NSPointInRect(where, [self bounds]))
	{
		_screen->TrigDragDrop(where.x, where.y, g_op_system_info->GetShiftKeyState());
		_has_been_dropped = TRUE;
	}
	else
		_screen->TrigDragEnd(where.x, where.y, g_op_system_info->GetShiftKeyState());
}

- (NSBOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	return YES;
}

// Invoked after the released image has been removed from the screen, signaling the receiver to import the pasteboard data.
- (NSBOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	if (!g_application->IsBrowserStarted())
		return NO;

	NSPoint where = [sender draggingLocation];
	where = [self convertPoint:where fromView:nil];
	where.y = [self frame].size.height - where.y;
	MacOpView* view = GetOpViewAt(_screen, where.x, where.y, true);
	RETURN_VALUE_IF_NULL(view, NO);

	OpDragListener* listener = view->GetDragListener();
	RETURN_VALUE_IF_NULL(listener, NO);

	OpPoint pt;
	pt = view->ConvertToScreen(pt);
	pt.x = where.x - pt.x + [[self window] frame].origin.x;
	pt.y = where.y - pt.y + [[[NSScreen screens] objectAtIndex:0] frame].size.height-[[self window] frame].origin.y-[self frame].size.height;

	DesktopDragObject* drag_object = (DesktopDragObject*)g_cocoa_drag_mgr->GetDragObject();
	RETURN_VALUE_IF_NULL(drag_object, NO);

	if (drag_object->GetType() == OpTypedObject::UNKNOWN_TYPE)
	{
		DropType drop = drag_object->GetVisualDropType();
		if (drop == DROP_COPY)
		{
			// Calling UNKNOWN TYPE drop
			_screen->TrigDragDrop(where.x, where.y, g_op_system_info->GetShiftKeyState());
			_has_been_dropped = TRUE;
		}
		else if (drag_object->GetURL() && uni_strlen(drag_object->GetURL()))
		{
			OpString tempException;
			tempException.Set(drag_object->GetURL());
			AddTemporaryException(&tempException);
			[self openUrl:drag_object->GetURL() view:view newTab:TRUE];
			g_cocoa_drag_mgr->StopDrag(FALSE);
		}
		else
		{
			const OpVector<OpString>* drop_names = &drag_object->GetURLs();

			// Files will override URLs
			OpAutoVector<OpString> file_names;
			if (DragDrop_Data_Utils::HasFiles(drag_object))
			{
				OpDragDataIterator& iter = drag_object->GetDataIterator();
				if (iter.First())
				{
					do
					{
						if (iter.IsFileData())
						{
							OpString *file_name = OP_NEW(OpString, ());
							if (file_name)
							{
								//some apps when drag files into Opera creates paths like /Volumes/disk/xxx which later could cause crashes, DSK-372892
								FSRef fsref;
								OpString normalized_path;
								if (OpFileUtils::ConvertUniPathToFSRef(iter.GetFileData()->GetFullPath(), fsref) == true)
								{
									if (!OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &normalized_path)))
										normalized_path.Set(iter.GetFileData()->GetFullPath());
								}
								else
									normalized_path.Set(iter.GetFileData()->GetFullPath());

								file_name->Set(normalized_path.CStr());

								AddTemporaryException(file_name);
								if ([self tryToOpenWebloc:file_name->CStr() view:view])
									OP_DELETE(file_name);
								else if (OpStatus::IsError(file_names.Add(file_name)))
									OP_DELETE(file_name);
							}
						}
					} while (iter.Next());
				}
				drop_names = &file_names;
			}

			while (view)
			{
				// This is a bit ugly. First, while all other drags are sent to the view, files are sent to the window.
				// Second, there are MDE_OpWindows that do not have a proper DesktopWindowListener, so we need to traverse past the wrong ones,
				// Thirdly, [[[self window] delegate] getListener]->OnDropFiles will just blanket open in a new tab, rather than do the right thing for the active tab.
				if (view->GetParentWindow())
				{
					if (((DesktopOpWindow*)view->GetParentWindow())->GetDesktopWindow())
					{
						((CocoaOpWindow*)view->GetParentWindow())->GetWindowListener()->OnDropFiles(*drop_names);
						break;
					}
					else
						view = (MacOpView*)view->GetParentWindow()->GetParentView();
				}
				else
					view = (MacOpView*)view->GetParentView();
			}
		}
	}
	else
	{
		BOOL isNoDropUrl = !drag_object->IsDropAvailable() && DragDrop_Data_Utils::HasURL(drag_object);

		// Open internal URLs (links) dropped on another page (view)
		BOOL isInternalLink = (isNoDropUrl
							&& drag_object->GetType() == OpTypedObject::DRAG_TYPE_LINK
							&& (view != _initial_drag_view || g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag) & DRAG_SAMEWINDOW));

		// Open bookmarks dropped on the page
		BOOL isBookmark = (drag_object->GetDropType() == DROP_NOT_AVAILABLE
						&& drag_object->GetType() == OpTypedObject::DRAG_TYPE_BOOKMARK
						&& DragDrop_Data_Utils::HasURL(drag_object));

		if (isInternalLink || isBookmark)
		{
			TempBuffer buffer;
			RETURN_VALUE_IF_ERROR(DragDrop_Data_Utils::GetURL(drag_object, &buffer, TRUE), NO);
			uni_char* url = buffer.GetStorage();
			RETURN_VALUE_IF_NULL(url, NO);
			[self openUrl:url view:view newTab:isInternalLink];
			g_cocoa_drag_mgr->StopDrag(FALSE);
		}
		else
		{
			// Calling drop for a known data type
			_screen->TrigDragDrop(where.x, where.y, g_op_system_info->GetShiftKeyState());
			_has_been_dropped = TRUE;
		}
	}

	return YES;
}

// Invoked when the dragging operation is complete, signaling the receiver to perform any necessary clean-up.
- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
	((MacOpSystemInfo*)g_op_system_info)->SetScreenReaderRunning(TRUE);
	id result = nil;
	if (_accessible_item && ([attribute isEqualToString:NSAccessibilityChildrenAttribute] || [attribute isEqualToString:NSAccessibilityVisibleChildrenAttribute]))
	{
		CocoaOpAccessibilityAdapter* access = static_cast<CocoaOpAccessibilityAdapter*>(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(_accessible_item));
		id access_obj = access ? access->GetAccessibleObject() : nil;
		return access_obj ? [NSArray arrayWithObject:access_obj] : nil;
	}
	if ([super respondsToSelector:@selector(accessibilityAttributeValue:)])
		result = [super accessibilityAttributeValue:attribute];
	return result;
}

- (id)accessibilityHitTest:(NSPoint)point
{
	((MacOpSystemInfo*)g_op_system_info)->SetScreenReaderRunning(TRUE);
	if (_accessible_item) {
		CocoaOpAccessibilityAdapter* access = static_cast<CocoaOpAccessibilityAdapter*>(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(_accessible_item));
		return [access->GetAccessibleObject() accessibilityHitTest:point];
	}
	return nil;
}

- (id)accessibilityFocusedUIElement
{
	((MacOpSystemInfo*)g_op_system_info)->SetScreenReaderRunning(TRUE);
	if (_accessible_item) {
		CocoaOpAccessibilityAdapter* access = static_cast<CocoaOpAccessibilityAdapter*>(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(_accessible_item));
		return [access->GetAccessibleObject() accessibilityFocusedUIElement];
	}
	return nil;
}

@end

@implementation OperaNSView(NSDraggingSource)
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
	OpString url_str;
	NSString *str = [dropDestination path];
	if(str)
	{
		int len = [str length];
		if(url_str.Reserve(len + 1))
		{
			[str getCharacters:(unichar*)url_str.CStr()];
			url_str[len] = 0;
			OpString full_path;
			AddTemporaryException(&url_str);
			if (g_cocoa_drag_mgr->SavePromisedFileInFolder(url_str, full_path))
			{
				NSString *s = [NSString stringWithCharacters:(const unichar*)full_path.CStr() length:full_path.Length()];
				return [NSArray arrayWithObject:s];
			}
		}
	}
	return NULL;
}

// consider using this for positioning the dropped file
//- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)viewLocation operation:(NSDragOperation)operation
//{
//	printf("ENDED AT %g, %g\n", viewLocation.x, viewLocation.y);
//}
@end

@implementation OperaNSView(NSDrag)
- (void)dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag
{
	NSImage *image = (NSImage*)g_cocoa_drag_mgr->GetNSDragImage();
	if(!image)
	{
		image = anImage;
	}
//	if(OpStatus::IsSuccess(g_cocoa_drag_mgr->PlaceItemsOnPasteboard(pboard)))
	{
		[super dragImage:image at:viewLocation offset:initialOffset event:event pasteboard:pboard source:sourceObj slideBack:slideFlag];
	}
}
@end
