// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#include "core/pch.h"

#include "platforms/mac/pi/desktop/MacSystemInputPI.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/UKeyTranslate.h"
#include "platforms/mac/util/macutils.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"

// Comment in to have printfs to check keytable of events
//#define _DEBUG_SCOPE_KEY_TABLE
// Comment in to have printfs to print menu debugging info
//#define _DEBUG_MAC_MENUS

// Modifier CGKeyCodes
#define kCGCommandKeyCode        55
#define kCGShiftKeyCode          56
#define kCGAltKeyCode            58
#define kCGControlKeyCode        59

#define kCGF1KeyCode			122
#define kCGF2KeyCode            120
#define kCGF3KeyCode             99
#define kCGF4KeyCode            118
#define kCGF5KeyCode             96
#define kCGF6KeyCode             97
#define kCGF7KeyCode             98
#define kCGF8KeyCode            100
#define kCGF9KeyCode            101
#define kCGF10KeyCode           109
#define kCGF11KeyCode           103
#define kCGF12KeyCode           111

#define kCGReturn				 36
#define kCGEnter				 76
#define kCGDeleteKeyCode        117
#define kCGTabKeyCode            48
#define kCGEscapeKeyCode         53
#define kCGCapsLockKeyCode       57
#define kCGNumLockKeyCode        71
#define kCGScrollLockKeyCode    107
#define kCGPauseKeyCode         113
#define kCGBackSpaceKeyCode      51
#define kCGInsertKeyCode        114

#define kCGUpKeyCode            126
#define kCGDownKeyCode          125
#define kCGLeftKeyCode          123
#define kCGRightKeyCode         124
#define kCGPageUpKeyCode        116
#define kCGPageDownKeyCode      121
#define kCGHomeKeyCode          115
#define kCGEndKeyCode           119

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SystemInputPI::Create(SystemInputPI** new_system_input)
{
	*new_system_input = OP_NEW(MacSystemInputPI, ());

	return *new_system_input ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

MacSystemInputPI::MacSystemInputPI() :
	m_menu_tracking(FALSE),
	m_sub_menu_next(FALSE),
	m_active_nspopupbutton(NULL)
{
	// Default it to the main menu since it's always showing
	SetActiveNSMenu(NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacSystemInputPI::Init()
{
	// Init the keycode table based on the current keyboard layout
	return InitKeyboardKeyCodeTable();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacSystemInputPI::InitKeyboardKeyCodeTable()
{
	// List the modifer combinations we want to try
	UInt32 modifier_key_states[] = {0, shiftKey, optionKey, controlKey, optionKey | shiftKey, optionKey | controlKey, controlKey | shiftKey, optionKey | shiftKey | controlKey};

	// Reset the table
	op_memset(&m_keycodetable.keycode, 0xFF, sizeof(m_keycodetable.keycode));
	op_memset(&m_keycodetable.modifiers, 0, sizeof(m_keycodetable.modifiers));
	
	// Loop all possible modifier combinations
	for (UInt32 i = 0; i < (sizeof(modifier_key_states) / sizeof(UInt32)); i++) 
	{
		UInt32 modifier_key_state = (modifier_key_states[i] >> 8) & 0xFF;
		
		// Loop for every possible key code
		for (UInt32 keyCode = 0; keyCode < 255; keyCode++) 
		{
			uni_char key = 0;
			
			// Get the char produced by the keycode
			if (OpStatus::IsSuccess(UKeyTranslate::GetUnicharFromVirtualKey(keyCode, key, modifier_key_state)))
			{
				// Set the keycode only once
				if (m_keycodetable.keycode[key] == 0xFFFF) 
				{
					// Set the key code
					m_keycodetable.keycode[key] = keyCode;
					
					// Save the modifier as an Opera Modifier
					if ((modifier_key_states[i] & cmdKey) == cmdKey)
						m_keycodetable.modifiers[key] |= SHIFTKEY_CTRL;
					if ((modifier_key_states[i] & shiftKey) == shiftKey)
						m_keycodetable.modifiers[key] |= SHIFTKEY_SHIFT;
					if ((modifier_key_states[i] & optionKey) == optionKey)
						m_keycodetable.modifiers[key] |= SHIFTKEY_ALT;
					if ((modifier_key_states[i] & controlKey) == controlKey)
						m_keycodetable.modifiers[key] |= SHIFTKEY_META;
				}
			}
		}
	}

#ifdef _DEBUG_SCOPE_KEY_TABLE
	// Debug code to print the entire table out
	printf("\n\n");
	for (UInt32 i = 0; i < keyCodeTableSize; i++)
	{
		if (m_keycodetable.keycode[i] != 0xFFFF) 
		{
			uni_char key_local[2];
			key_local[0] = i;
			key_local[1] = NULL;
			OpString key_string;
			key_string.Set(key_local);

			printf("Key Num: %u, Char: %s, KeyCode: %d, Modifiers: 0x%02x\n", i, key_string.UTF8AllocL(), m_keycodetable.keycode[i], m_keycodetable.modifiers[i]);
		}
	}
	printf("\n\n");
#endif // _DEBUG_SCOPE_KEY_TABLE

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier)
{
	// DSK-360264: During a Watir testrun the OS sometimes unfocuses the Opera window with some JRuby window.
	// Once the focus is taken away, we need two clicks instead of one to actually send an action to a dialog,
	// i.e. press a button, since we first have to give the focus to the dialog and only then can we click the button.
	// That breaks the Watir tests in a way that is hardly fixable from outside from the Opera binary, thus the change
	// below.
	ProcessSerialNumber psn;
	if (noErr == MacGetCurrentProcess(&psn))
		SetFrontProcess(&psn);

	// Create a CGPoint of where to click
	CGPoint pt = CGPointMake(x, y);

	// Create the mouse down and up events
	CGEventRef mouseDownEv = CGEventCreateMouseEvent(NULL, ConvertToMacButtonEvent(button, FALSE), pt, ConvertToMacButton(button));
	if (!mouseDownEv)
		return;
	CGEventRef mouseUpEv = CGEventCreateMouseEvent(NULL, ConvertToMacButtonEvent(button, TRUE), pt, ConvertToMacButton(button));
	if (!mouseUpEv)
	{
		CFRelease(mouseDownEv);
		return;
	}

	// Modify the events based on the modifier passed in
	// If you come to a conclusion that the modifiers should only be set when they are non-zero,
	// please consider DSK-361726.
	CGEventSetFlags(mouseDownEv, ConvertToMacModifiers(modifier));
	CGEventSetFlags(mouseUpEv, ConvertToMacModifiers(modifier));

	for (INT32 i = 0; i < num_clicks; i++)
	{
		// Set the click count
		CGEventSetIntegerValueField(mouseDownEv, kCGMouseEventClickState, num_clicks);
		CGEventSetIntegerValueField(mouseUpEv, kCGMouseEventClickState, num_clicks);
		
		// Post the events to the system
		CGEventPost(kCGHIDEventTap, mouseDownEv);
		CGEventPost(kCGHIDEventTap, mouseUpEv);
	}
	
	// Clean up
	CFRelease(mouseDownEv);
	CFRelease(mouseUpEv);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	PressMouseButton(x, y, button, modifier, FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	PressMouseButton(x, y, button, modifier, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	// Create a CGPoint of where to click
	CGPoint pt = CGPointMake(x, y);
	
	// Create the mouse move event
	//
	// Note: Originally I thought we would need to do kCGEventLeftMouseDragged etc events here so that 
	// the dragging would work in combination with the MouseUp/Down. But it turns out this isn't the case 
	// and it works bu setting the kCGEventMouseMoved event. Note I have to use the kCGEventMouseMoved
	// otherwise anyway because if the mouse isn't down the movement of the mouse isn't registed.
	// If in the future we want to bring back the kCGEventLeftMouseDragged etc events we have to keep this
	// restriction in mind and call the kCGEventMouseMoved event if the mouse isn't down. adamm 30-11-2010
	CGEventRef mouseEv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pt, ConvertToMacButton(button));
	if (!mouseEv)
		return;
	
	// Modify the events based on the modifier passed in
	if (modifier)
		CGEventSetFlags(mouseEv, ConvertToMacModifiers(modifier));
	
	// Set the click count (add if required)
	//CGEventSetIntegerValueField(mouseEv, kCGMouseEventClickState, 1);
	
	// Post the events to the system (or kCGSessionEventTap?)
	CGEventPost(kCGHIDEventTap, mouseEv);
	
	// Clean up
	CFRelease(mouseEv);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::PressMouseButton(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier, BOOL up)
{
	// Create a CGPoint of where to click
	CGPoint pt = CGPointMake(x, y);
	
	// Create the mouse down and up events
	CGEventRef mouseEv = CGEventCreateMouseEvent(NULL, ConvertToMacButtonEvent(button, up), pt, ConvertToMacButton(button));
	if (!mouseEv)
		return;
	
	// Modify the events based on the modifier passed in
	if (modifier)
		CGEventSetFlags(mouseEv, ConvertToMacModifiers(modifier));
	
	// Set the click count
	CGEventSetIntegerValueField(mouseEv, kCGMouseEventClickState, 1);
	
	// Post the events to the system
	CGEventPost(kCGHIDEventTap, mouseEv);
	
	// Clean up
	CFRelease(mouseEv);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	CGKeyCode key_code = GetKeyCode(key, op_key, modifier);

	KeyDownInternal(key_code, modifier);
	KeyUpInternal(key_code, modifier);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	CGKeyCode key_code = GetKeyCode(key, op_key, modifier);

	KeyDownInternal(key_code, modifier);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	CGKeyCode key_code = GetKeyCode(key, op_key, modifier);

	KeyUpInternal(key_code, modifier);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::PressMenu(const OpStringC& menu_text, bool popmenu)
{
	// Handle regular menus first
	if (!popmenu)
	{
		m_search_menu_item.Set(menu_text);

#ifdef _DEBUG_MAC_MENUS
		OpString8 opstr8;
		opstr8.SetUTF8FromUTF16(m_search_menu_item);
		printf("Searching for menu: %s\n", opstr8.CStr());
#endif // _DEBUG_MAC_MENUS
	
		// If no menu's are showing we go for the main menu bar
		if (!m_menu_tracking)
		{
			// Focus to menu and pull down the main menu, and shift to the "Opera" menu
			// This will fire the MenuItemHighlighted function below
			PressKey(0, OP_KEY_F2, SHIFTKEY_META);
			PressKey(0, OP_KEY_DOWN, SHIFTKEY_NONE);
			PressKey(0, OP_KEY_RIGHT, SHIFTKEY_NONE);
		}
		else if (m_first_sub_menu_item.HasContent())
		{
			// This is a submenu and you are searching for the first item
			if (!m_first_sub_menu_item.Compare(menu_text))
			{
				// DSK-361493: Reset the previous menu item in order to be able to click it again in
				// case that a menu option with the very same text was clicked last time.
				m_previous_menu_item.Empty();

				// Hit the menu item
				PressKey(0, OP_KEY_ENTER, SHIFTKEY_NONE);

				// Tell Watir we pressed the menu
				g_scope_manager->desktop_window_manager->MenuItemPressed(menu_text);
				
				// Clear the menu search text
				m_search_menu_item.Empty();
			}
			else {
				// Move to the next item
				PressKey(0, OP_KEY_DOWN, SHIFTKEY_NONE);
			}
		}
		else 
		{
			// DSK-361493: Reset the previous menu item in order to be able to click it again in
			// case that a menu option with the very same text was clicked last time.
			m_previous_menu_item.Empty();

			// Move to the next item in the menu
			PressKey(0, OP_KEY_DOWN, SHIFTKEY_NONE);
		}

		// Empty submenu item just in case something went crazy
		m_first_sub_menu_item.Empty();

	
		// Now callbacks take over (i.e. MenuItemHighlighted)
	}
	else {
		// This is a pop-menu on a drop down so we can set the
		// item directly with a call.
		if (m_active_nspopupbutton)
		{
			SelectPopMenuItemWithTitle(m_active_nspopupbutton, menu_text);
			
			// Tell Watir we pressed the pop-menu item
			g_scope_manager->desktop_window_manager->MenuItemPressed(menu_text, popmenu);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::MenuTracking(BOOL tracking)
{
	m_menu_tracking = tracking;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::SetActiveNSMenu(void *nsmenu)
{
	// If this is NULL then set back to the main menu
	if (!nsmenu)
		m_active_nsmenu = GetMainMenu();
	else
		m_active_nsmenu = nsmenu;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::MenuItemHighlighted(const OpStringC& menu_text, BOOL main_menu, BOOL is_submenu)
{
#ifdef _DEBUG_MAC_MENUS
	// Debugging menu events code
	OpString8 opstr8;
	opstr8.SetUTF8FromUTF16(menu_text);
	printf("Menu Item Title: %s, Is mainmenu: %d, is_submenu: %d\n", opstr8.CStr(), main_menu, is_submenu);
#endif // _DEBUG_MAC_MENUS

	// Save the first item of a submenu when it's opened and do nothing else.
	// The pressing will be handled by the next call
	if (m_sub_menu_next)
	{
		m_first_sub_menu_item.Set(menu_text);
		m_sub_menu_next = FALSE;
#ifdef _DEBUG_MAC_MENUS
		printf("sub menu saved!\n");
#endif // _DEBUG_MAC_MENUS
		return;
	}
	
	// Check if we got the same menu item in a row, this probably then the end of the menu
	if (m_previous_menu_item.HasContent() && !m_previous_menu_item.Compare(menu_text))
	{
		// Reset everything
		m_previous_menu_item.Empty();
		m_search_menu_item.Empty();
	}
	else
	{
		if (m_search_menu_item.HasContent())
		{
			if (!m_search_menu_item.Compare(menu_text))
			{
				// Clear the menu search text
				m_search_menu_item.Empty();
				
				// If this is the main menu do nothing
				if (!main_menu)
				{
					// For submenus we need to hit the right arrow to make them open
					if (is_submenu)
					{
						PressKey(0, OP_KEY_RIGHT, SHIFTKEY_NONE);
						m_sub_menu_next = TRUE;
#ifdef _DEBUG_MAC_MENUS
						printf("sub menu up next!\n");
#endif // _DEBUG_MAC_MENUS
					}
					else
					{
						// Hit the menu item
						PressKey(0, OP_KEY_ENTER, SHIFTKEY_NONE);
					}
				}
				
				// Tell Watir we pressed the menu
				g_scope_manager->desktop_window_manager->MenuItemPressed(menu_text);
				
			}
			else {
				// The main menu moves to the right and all other menus go down
				if (main_menu)
					PressKey(0, OP_KEY_RIGHT, SHIFTKEY_NONE);
				else
					PressKey(0, OP_KEY_DOWN, SHIFTKEY_NONE);
			}
		}

		m_previous_menu_item.Set(menu_text);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::KeyDownInternal(CGKeyCode key_code, ShiftKeyState modifier)
{
	// You need to send the event to the actual process of Opera otherwise
	// the modifiers stick and nothing works properly
	// Perhaps the reason for stuck modifiers is the "if (modifier)" line, see DSK-361726
	ProcessSerialNumber psn; 
	MacGetCurrentProcess(&psn);
	
	// Create the keydown event
	CGEventRef key_event_down = CGEventCreateKeyboardEvent(NULL, key_code, true);
	
	// Modify the events based on the modifier passed in
	if (modifier)
		CGEventSetFlags(key_event_down, ConvertToMacModifiers(modifier));
	
	// Run the keydown and cleanup
	CGEventPostToPSN(&psn, key_event_down);
	CFRelease(key_event_down);	
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacSystemInputPI::KeyUpInternal(CGKeyCode key_code, ShiftKeyState modifier)
{
	// You need to send the event to the actual process of Opera otherwise
	// the modifiers stick and nothing works properly
	// Perhaps the reason for stuck modifiers is the "if (modifier)" line, see DSK-361726
	ProcessSerialNumber psn; 
	MacGetCurrentProcess(&psn);
	
	// Create the keyup event
	CGEventRef key_event_up = CGEventCreateKeyboardEvent(NULL, key_code, false);
	
	// Modify the events based on the modifier passed in
	if (modifier)
		CGEventSetFlags(key_event_up, ConvertToMacModifiers(modifier));
	
	// Run the keyup and cleanup
	CGEventPostToPSN(&psn, key_event_up);
	CFRelease(key_event_up);	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGKeyCode MacSystemInputPI::GetKeyCode(uni_char key, OpVirtualKey op_key, ShiftKeyState &modifier)
{
	CGKeyCode key_code;
	
	// Check for a special Opera key
	if (op_key != OP_KEY_INVALID)
	{
		key_code = ConvertToMacKeyCode(op_key);
	}
	else
	{
		// If this is not an opkey then use the table to reverse lookup the
		// keycode based on the uni_char key
		key_code = m_keycodetable.keycode[key];
		
		// Add any modifiers that are required to make the char work
		modifier |= m_keycodetable.modifiers[key];
	}
	
	return key_code;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGEventType MacSystemInputPI::ConvertToMacButtonEvent(MouseButton button, BOOL up)
{
	switch (button) 
	{
		case MOUSE_BUTTON_1:
			if (up)
				return kCGEventLeftMouseUp;
			return kCGEventLeftMouseDown;
			
		case MOUSE_BUTTON_2:
			if (up)
				return kCGEventRightMouseUp;
			return kCGEventRightMouseDown;
			
		case MOUSE_BUTTON_3:
			if (up)
				return kCGEventOtherMouseUp;
			return kCGEventOtherMouseDown;
			
		default:
			OP_ASSERT(!"Unsupported mouse button event ");
			break;
	}
	
	return kCGEventNull;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGEventType MacSystemInputPI::ConvertToMacButtonDragEvent(MouseButton button)
{
	switch (button) 
	{
		case MOUSE_BUTTON_1:
			return kCGEventLeftMouseDragged;
			
		case MOUSE_BUTTON_2:
			return kCGEventRightMouseDragged;
			
		case MOUSE_BUTTON_3:
			return kCGEventOtherMouseDragged;
			
		default:
			OP_ASSERT(!"Unsupported mouse button drag event ");
			break;
	}
	
	return kCGEventNull;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGMouseButton MacSystemInputPI::ConvertToMacButton(MouseButton button)
{
	switch (button) 
	{
		case MOUSE_BUTTON_1:
			return kCGMouseButtonLeft;

		case MOUSE_BUTTON_2:
			return kCGMouseButtonRight;

		case MOUSE_BUTTON_3:
			return kCGMouseButtonCenter;

		default:
			OP_ASSERT(!"Unsupported mouse button ");
			break;
	}

	// Should assert so just click
	return kCGMouseButtonLeft;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGEventFlags MacSystemInputPI::ConvertToMacModifiers(ShiftKeyState modifier)
{
	CGEventFlags mac_modifiers = 0;
	
	if ((modifier & SHIFTKEY_CTRL) == SHIFTKEY_CTRL)
		mac_modifiers |= kCGEventFlagMaskCommand;
	if ((modifier & SHIFTKEY_SHIFT) == SHIFTKEY_SHIFT)
		mac_modifiers |= kCGEventFlagMaskShift;
	if ((modifier & SHIFTKEY_ALT) == SHIFTKEY_ALT)
		mac_modifiers |= kCGEventFlagMaskAlternate;
	if ((modifier & SHIFTKEY_META) == SHIFTKEY_META)
		mac_modifiers |= kCGEventFlagMaskControl;

	return mac_modifiers;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGKeyCode MacSystemInputPI::ConvertToMacKeyCode(const OpVirtualKey key)
{
	switch (key) 
	{
		case OP_KEY_CTRL:
			return kCGCommandKeyCode;
			
		case OP_KEY_SHIFT:
			return kCGShiftKeyCode;
			
		case OP_KEY_ALT:
			return kCGAltKeyCode;
			
		case OP_KEY_MAC_CTRL:
			return kCGControlKeyCode;
			
		case OP_KEY_F1:
			return kCGF1KeyCode;
			
		case OP_KEY_F2:
			return kCGF2KeyCode;
			
		case OP_KEY_F3:
			return kCGF3KeyCode;
			
		case OP_KEY_F4:
			return kCGF4KeyCode;
			
		case OP_KEY_F5:
			return kCGF5KeyCode;
			
		case OP_KEY_F6:
			return kCGF6KeyCode;
			
		case OP_KEY_F7:
			return kCGF7KeyCode;
			
		case OP_KEY_F8:
			return kCGF8KeyCode;
			
		case OP_KEY_F9:
			return kCGF9KeyCode;
			
		case OP_KEY_F10:
			return kCGF10KeyCode;
			
		case OP_KEY_F11:
			return kCGF11KeyCode;
			
		case OP_KEY_F12:
			return kCGF12KeyCode;
			
		case OP_KEY_ENTER:
			return kCGReturn;
			
		case OP_KEY_DELETE:
			return kCGDeleteKeyCode;
			
		case OP_KEY_TAB:
			return kCGTabKeyCode;

		case OP_KEY_ESCAPE:
			return kCGEscapeKeyCode;
			
		case OP_KEY_CAPS_LOCK:
			return kCGCapsLockKeyCode;
			
		case OP_KEY_NUM_LOCK:
			return kCGNumLockKeyCode;
			
		case OP_KEY_SCROLL_LOCK:
			return kCGScrollLockKeyCode;
			
		case OP_KEY_PAUSE:
			return kCGPauseKeyCode;
			
		case OP_KEY_BACKSPACE:
			return kCGBackSpaceKeyCode;
			
		case OP_KEY_INSERT:
			return kCGInsertKeyCode;
			
		case OP_KEY_UP:
			return kCGUpKeyCode;
			
		case OP_KEY_DOWN:
			return kCGDownKeyCode;
			
		case OP_KEY_LEFT:
			return kCGLeftKeyCode;
			
		case OP_KEY_RIGHT:
			return kCGRightKeyCode;
			
		case OP_KEY_PAGEUP:
			return kCGPageUpKeyCode;
			
		case OP_KEY_PAGEDOWN:
			return kCGPageDownKeyCode;
			
		case OP_KEY_HOME:
			return kCGHomeKeyCode;
			
		case OP_KEY_END:
			return kCGEndKeyCode;
			
		default:
			OP_ASSERT(!"Unsupported Opera opkey ");
			break;
	}
	
	return 0;
}


