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

#include "platforms/windows/WindowsSystemInputPI.h"

// Shift keys returned from VkKeyScanEx
#define VKKEYSCAN_SHIFT		0x0100
#define VKKEYSCAN_CTRL		0x0200
#define VKKEYSCAN_MENU		0x0400

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SystemInputPI::Create(SystemInputPI** new_system_input)
{
	*new_system_input = OP_NEW(WindowsSystemInputPI, ());
	
	return *new_system_input ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

WindowsSystemInputPI::WindowsSystemInputPI()
{
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WindowsSystemInputPI::Init()
{
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier)
{
	// Move the mouse
	SetCursorPos(x,y);

	// Create the button down input
	INPUT mouse_down; 
	::ZeroMemory(&mouse_down, sizeof(INPUT));
	mouse_down.type			= INPUT_MOUSE;
	mouse_down.mi.dwFlags	= ConvertToWinButtonInput(button, FALSE);
	mouse_down.mi.dx		= x;
	mouse_down.mi.dy		= y;

	// Create the button up input
	INPUT mouse_up; 
	::ZeroMemory(&mouse_up, sizeof(INPUT));
	mouse_up.type		= INPUT_MOUSE;
	mouse_up.mi.dwFlags	= ConvertToWinButtonInput(button, TRUE);
	mouse_up.mi.dx		= x;
	mouse_up.mi.dy		= y;

	// Press down the modifiers
	SendModifiers(modifier, FALSE);

	// Post the events to the system
	for (INT32 i = 0; i < num_clicks; i++)
	{
		::SendInput(1, &mouse_down, sizeof(INPUT));
		::SendInput(1, &mouse_up, sizeof(INPUT));
	}

	// Release the modifiers
	SendModifiers(modifier, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	PressMouseButton(x, y, button, modifier, FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	PressMouseButton(x, y, button, modifier, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	// Move the mouse
	SetCursorPos(x,y);

	// No need to handle button or modifiers
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::PressMouseButton(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier, BOOL up)
{
	// Move the mouse
	SetCursorPos(x,y);

	// Create the button down input
	INPUT mouse_down; 
	::ZeroMemory(&mouse_down, sizeof(INPUT));
	mouse_down.type			= INPUT_MOUSE;
	mouse_down.mi.dwFlags	= ConvertToWinButtonInput(button, up);
	mouse_down.mi.dx		= x;
	mouse_down.mi.dy		= y;

	// Press down the modifiers
	if (!up)
		SendModifiers(modifier, FALSE);

	// Post the events to the system
	::SendInput(1, &mouse_down, sizeof(INPUT));

	// Release the modifiers
	if (up)
		SendModifiers(modifier, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	SHORT key_code = GetKeyCode(key, op_key, modifier);

	KeyDownInternal(key_code, modifier);
	KeyUpInternal(key_code, modifier);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	KeyDownInternal(GetKeyCode(key, op_key, modifier), modifier);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	KeyUpInternal(GetKeyCode(key, op_key, modifier), modifier);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::KeyDownInternal(SHORT key_code, ShiftKeyState modifier)
{
	INPUT key_down; 
	::ZeroMemory(&key_down, sizeof(INPUT));
	key_down.type		= INPUT_KEYBOARD;
	key_down.ki.wVk		= key_code;

	// Press down the modifiers
	SendModifiers(modifier, FALSE);

	// Press the key
	::SendInput(1, &key_down, sizeof(INPUT));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::KeyUpInternal(SHORT key_code, ShiftKeyState modifier)
{
	INPUT key_up; 
	::ZeroMemory(&key_up, sizeof(INPUT));
	key_up.type			= INPUT_KEYBOARD;
	key_up.ki.wVk		= key_code;
	key_up.ki.dwFlags	= KEYEVENTF_KEYUP;

	// Release the key
	::SendInput(1, &key_up, sizeof(INPUT));

	// Release the modifiers
	SendModifiers(modifier, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SHORT WindowsSystemInputPI::GetKeyCode(uni_char key, OpVirtualKey op_key, ShiftKeyState &modifier)
{
	if (op_key != OP_KEY_INVALID)
	{
		// The Windows virtual key mapping is now equal to the keycodes in OpKey
		return static_cast<OpKey::Code>(op_key);
	}

	HKL keyboard_layout = GetKeyboardLayout(0);

	// Get the keycode based on the keyboard layout and the key passed in
	SHORT key_code = VkKeyScanEx(key, keyboard_layout);

	// Get the modifier from the hibyte
	if ((key_code & VKKEYSCAN_CTRL) == VKKEYSCAN_CTRL)
		modifier |= SHIFTKEY_CTRL;
	if ((key_code & VKKEYSCAN_SHIFT) == VKKEYSCAN_SHIFT)
		modifier |= SHIFTKEY_SHIFT;
	if ((key_code & VKKEYSCAN_MENU) == VKKEYSCAN_MENU)
		modifier |= SHIFTKEY_ALT;
		
	// Remove any modifiers from the keycode
	key_code &= 0xFF;

	return key_code;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD WindowsSystemInputPI::ConvertToWinButtonInput(MouseButton button, BOOL up)
{
	switch (button) 
	{
		case MOUSE_BUTTON_1:
			if (up)
				return MOUSEEVENTF_LEFTUP;
			return MOUSEEVENTF_LEFTDOWN;
			
		case MOUSE_BUTTON_2:
			if (up)
				return MOUSEEVENTF_RIGHTUP;
			return MOUSEEVENTF_RIGHTDOWN;
			
		case MOUSE_BUTTON_3:
			if (up)
				return MOUSEEVENTF_MIDDLEUP;
			return MOUSEEVENTF_MIDDLEDOWN;
			
		default:
			OP_ASSERT(!"Unsupported mouse button event ");
			break;
	}
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::SendModifiers(ShiftKeyState modifier, BOOL up)
{
	if ((modifier & SHIFTKEY_CTRL) == SHIFTKEY_CTRL)
		SendModifier(SHIFTKEY_CTRL, up);
	if ((modifier & SHIFTKEY_SHIFT) == SHIFTKEY_SHIFT)
		SendModifier(SHIFTKEY_SHIFT, up);
	if ((modifier & SHIFTKEY_ALT) == SHIFTKEY_ALT)
		SendModifier(SHIFTKEY_ALT, up);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowsSystemInputPI::SendModifier(ShiftKeyState modifier, BOOL up)
{
	INPUT key; 
	::ZeroMemory(&key, sizeof(INPUT));
	key.type			= INPUT_KEYBOARD;
	key.ki.wVk			= ConvertToWinKey(modifier);
	if (up)
		key.ki.dwFlags	= KEYEVENTF_KEYUP;

	::SendInput(1, &key, sizeof(INPUT));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

WORD WindowsSystemInputPI::ConvertToWinKey(ShiftKeyState modifier)
{
	// There can only be one modifier at a time passed to this function
	switch (modifier)
	{
		case SHIFTKEY_CTRL:
			return VK_CONTROL;
		
		case SHIFTKEY_SHIFT:
			return VK_SHIFT;

		case SHIFTKEY_ALT:
			return VK_MENU;

		default:
			OP_ASSERT(!"Unsupported shift key event ");
			break;
	}

	return 0;
}
