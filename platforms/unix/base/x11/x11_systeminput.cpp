/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** karie, Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_systeminput.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/posix/posix_native_util.h"
#include "modules/pi/OpDLL.h"

//#define DEBUG_SYSTEMINPUT
//#define DEBUG_EACH_KEY

OP_STATUS SystemInputPI::Create(SystemInputPI** new_system_input)
{
	*new_system_input = OP_NEW(X11SystemInput, ());
	if (!*new_system_input)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = ((X11SystemInput *)*new_system_input)->InitLibrary();
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*new_system_input);
		*new_system_input = 0;
	}

	return result;
}


X11SystemInput::X11SystemInput()
	:m_dll(0)
	,m_event_base(0)
	,m_error_base(0)
	,m_table_created(false)
{
}


const uni_char* X11SystemInput::GetLibraryName()
{
	return UNI_L("libXtst.so.6");
}

OP_STATUS X11SystemInput::Init()
{
	if (!m_table_created)
	{
		RETURN_IF_ERROR(CreateKeyCodeTableByEvents());
#ifdef DEBUG_SYSTEMINPUT
		PrintKeyMapping();
#endif
	}
	return OpStatus::OK;
}

OP_STATUS X11SystemInput::InitLibrary()
{
	RETURN_IF_ERROR(OpDLL::Create(&m_dll));
	RETURN_IF_ERROR(m_dll->Load(GetLibraryName()));
	if (OpStatus::IsError(g_x11->UnloadOnShutdown(m_dll))) // g_x11 takes over ownership of the library
	{
		OP_DELETE(m_dll);
		return OpStatus::ERR;
	}

	m_functions.xtestQueryExtension = (XtestFunctions::xtestQueryExtensionFunction)m_dll->GetSymbolAddress("XTestQueryExtension");
	m_functions.xtestFakeKeyEvent = (XtestFunctions::xtestFakeKeyEventFunction)m_dll->GetSymbolAddress("XTestFakeKeyEvent");
	m_functions.xtestFakeButtonEvent = (XtestFunctions::xtestFakeButtonEventFunction)m_dll->GetSymbolAddress("XTestFakeButtonEvent");
	m_functions.xtestFakeMotionEvent = (XtestFunctions::xtestFakeMotionEventFunction)m_dll->GetSymbolAddress("XTestFakeMotionEvent");

	if (!m_functions.xtestQueryExtension ||
		!m_functions.xtestFakeKeyEvent ||
		!m_functions.xtestFakeButtonEvent ||
		!m_functions.xtestFakeMotionEvent)
	{
		return OpStatus::ERR;
	}

	int major, minor;
	if (m_functions.xtestQueryExtension(g_x11->GetDisplay(), &m_event_base, &m_error_base, &major, &minor) == False)
		return OpStatus::ERR;

	return OpStatus::OK;
}



void X11SystemInput::Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier)
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = 0;
	int x11_button = OperaToX11Button(button);

	m_functions.xtestFakeMotionEvent(display, screen, x, y, CurrentTime);
	if (modifier)
		ModifierChanged(modifier, True);

	for (INT32 i=0; i<num_clicks; i++)
	{
		m_functions.xtestFakeButtonEvent(display, x11_button, True, CurrentTime);
		m_functions.xtestFakeButtonEvent(display, x11_button, False, CurrentTime);
	}

	if (modifier)
		ModifierChanged(modifier, False);
}

// FIXME: Modifier problem for mouse up og down

void  X11SystemInput::MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = 0;
	int x11_button = OperaToX11Button(button);

	m_functions.xtestFakeMotionEvent(display, screen, x, y, CurrentTime);

	if (modifier)
		ModifierChanged(modifier, True); // Modifier down

	m_functions.xtestFakeButtonEvent(display, x11_button, True, CurrentTime);
}


void  X11SystemInput::MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = 0;
	int x11_button = OperaToX11Button(button);

	// Move to location
	m_functions.xtestFakeMotionEvent(display, screen, x, y, CurrentTime);

	// MouseUp
	m_functions.xtestFakeButtonEvent(display, x11_button, False, CurrentTime);

	// Release modifier
	if (modifier)
		ModifierChanged(modifier, False);
}

void  X11SystemInput::MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier)
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = 0;

	m_functions.xtestFakeMotionEvent(display, screen, x, y, CurrentTime);
}


void X11SystemInput::PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier) 
{
	X11KeyState state(key, op_key, modifier, OpKey::LOCATION_STANDARD);

	SetKeyState(state);

	KeyDownInternal(state.GetKeyCode(), state.GetModifier());
	KeyUpInternal(state.GetKeyCode(), state.GetModifier());
}


void X11SystemInput::KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	X11KeyState state(key, op_key, modifier, OpKey::LOCATION_STANDARD);

	SetKeyState(state);

	KeyDownInternal(state.GetKeyCode(), state.GetModifier());
}

void X11SystemInput::KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier)
{
	X11KeyState state(key, op_key, modifier, OpKey::LOCATION_STANDARD);

	SetKeyState(state);

	KeyUpInternal(state.GetKeyCode(), state.GetModifier());
}


///////////////////////////////////////////////////////////////////////////////////////////

// - Helper functions


void X11SystemInput::SetKeyState(X11KeyState& state)
{
	if (state.GetVirtualKey() == OP_KEY_INVALID)
	{
		state.SetKeyCode(m_keycodetable.GetKeyCode(state.GetKey()));
		if (state.GetKeyCode())
		{
			state.SetModifier(X11StateToOperaModifier(m_keycodetable.GetState(state.GetKey())));
		}
	}
	else
	{
		state.SetKeyCode(VirtualKeyToKeyCode(state.GetVirtualKey(), state.GetLocation()));
	}
}



void X11SystemInput::ModifierChanged(ShiftKeyState modifier, X11Types::Bool is_press)
{
	X11Types::Display* display = g_x11->GetDisplay();

	if (modifier & SHIFTKEY_CTRL)
	{
		m_functions.xtestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Control_L), is_press, CurrentTime);
	}
	if (modifier & SHIFTKEY_SHIFT)
	{
		m_functions.xtestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Shift_L), is_press, CurrentTime);
	}
	if (modifier & SHIFTKEY_ALT)
	{
		m_functions.xtestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Alt_L), is_press, CurrentTime);
	}
	if (modifier & SHIFTKEY_META)
	{
		m_functions.xtestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Meta_L), is_press, CurrentTime);
	}
	if (modifier & SHIFTKEY_SUPER)
	{
		// SHIFTKEY_SUPER is used as a placeholder for AltGr as Opera has not defined state for this key
		m_functions.xtestFakeKeyEvent(display, XKeysymToKeycode(display, XK_ISO_Level3_Shift), is_press, CurrentTime);
	}
}


// Press modifier and key
void X11SystemInput::KeyDownInternal(int key_code, ShiftKeyState modifier)
{
#ifdef DEBUG_EACH_KEY
	OpString keytext;
	fprintf(stderr, "\n X11SystemInput::PressKey(%c), modifier = %x. KeySym(key) = %x\n", key, modifier, InputUtils::TranslateKeySym(key, keytext));
#endif

	X11Types::Display* display = g_x11->GetDisplay();

	ModifierChanged(modifier, True);

#ifdef DEBUG_EACH_KEY
	fprintf(stderr, " FakeKeyPress keycode %d (%x) Press\n", key_code, key_code);
#endif

	m_functions.xtestFakeKeyEvent(display, key_code, True, CurrentTime);
}

// Release key and modifier
void X11SystemInput::KeyUpInternal(int key_code, ShiftKeyState modifier)
{
#ifdef DEBUG_EACH_KEY
	OpString keytext;
	fprintf(stderr, "\n X11SystemInput::PressKey(%c), modifier = %x. KeySym(key) = %x\n", key, modifier, InputUtils::TranslateKeySym(key, keytext));
#endif

	X11Types::Display* display = g_x11->GetDisplay();

	m_functions.xtestFakeKeyEvent(display, key_code, False, CurrentTime);

#ifdef DEBUG_EACH_KEY
	fprintf(stderr, " FakeKeyPress keycode %d (%x) Release\n", key_code, key_code);
#endif
	ModifierChanged(modifier, False);
}


int X11SystemInput::VirtualKeyToKeyCode(OpVirtualKey virtual_key, OpKey::Location location)
{
	KeySym key_sym = InputUtils::VirtualKeyToKeySym(virtual_key, location);
	KeyCode key_code = XKeysymToKeycode(g_x11->GetDisplay(), key_sym);
	return key_code;
}


int X11SystemInput::X11StateToOperaModifier(int state)
{
	int operamodifier = 0;
	if (state & ShiftMask)
		operamodifier |= SHIFTKEY_SHIFT;
	if (state & ControlMask)
		operamodifier |= SHIFTKEY_CTRL;
	if (state & Mod1Mask)
		operamodifier |= SHIFTKEY_ALT;
	if (state & Mod2Mask)
		operamodifier |= SHIFTKEY_META;
	// SHIFTKEY_SUPER is used as a placeholder for AltGr as Opera has not defined state for this key
	if (state & Mod5Mask)
		operamodifier |= SHIFTKEY_SUPER;
	return operamodifier;
}


int X11SystemInput::OperaToX11Button(INT32 button)
{
	switch(button)
	{
	case MOUSE_BUTTON_1:
		return 1;
	case MOUSE_BUTTON_2:
		return 3;
	case MOUSE_BUTTON_3:
		return 2;
	default:
		OP_ASSERT(FALSE);
		return 0;
	}
}


OP_STATUS X11SystemInput::CreateKeyCodeTableByEvents()
{
	int keycode_low = 0;
	int keycode_high = 0;

	m_keycodetable.Reset();

	X11Types::Display* display = g_x11->GetDisplay();
	XDisplayKeycodes(display, &keycode_low, &keycode_high);

	XEvent event;
	event.xany.type = KeyPress;
	event.xany.serial = 3;
	event.xany.send_event = False;
	event.xany.display = display;
	event.xany.window = g_x11->GetAppWindow();
	event.xkey.subwindow = g_x11->GetAppWindow();
	event.xkey.root = RootWindowOfScreen(DefaultScreenOfDisplay(display));
	event.xkey.time = CurrentTime;
	event.xkey.x = event.xkey.y = event.xkey.x_root = event.xkey.y_root = 10;
	event.xkey.state = 0;
	event.xkey.same_screen = True;

	KeySym keysym = 0;
	char buf[100];

	UINT32 modifier_key_states[] = {0, ShiftMask, ControlMask, Mod1Mask, ShiftMask | Mod1Mask, Mod2Mask, ShiftMask | Mod2Mask, Mod4Mask, ShiftMask | Mod4Mask, ShiftMask | Mod5Mask, Mod5Mask};

	for (int i = keycode_low; i <= keycode_high; i++)
	{
		event.xkey.keycode = i;

		for (uint j = 0; j < ARRAY_SIZE(modifier_key_states); j++)
		{
			event.xkey.state = modifier_key_states[j];

			InputUtils::KeyContent content;
			InputUtils::TranslateKeyEvent(&event, content);
			if (content.GetUtf8Text().HasContent())
			{
				OpString text;
				RETURN_IF_ERROR(text.SetFromUTF8(content.GetUtf8Text().CStr()));
				if (text.Length() == 1)
				{
					int dummy;
					int key = Unicode::GetUnicodePoint(text.CStr(), text.Length(), dummy);

					// 0xFF means never set. Otherwise replace if we already have a state combination
					// of ControlMask as Opera does not accept the text part when receiving a <Ctrl>+<key> event
					if (m_keycodetable.keycode[key] == 0xFF || m_keycodetable.state[key] == ControlMask)
					{
						m_keycodetable.keycode[key] = event.xkey.keycode;
						m_keycodetable.state[key]   = event.xkey.state;
					}
				}
			}
		}
	}

	return OpStatus::OK;
}

void X11SystemInput::PrintKeyMapping()
{
	for (UINT32 i = 0; i < keyCodeTableSize; i++)
	{
		if (m_keycodetable.keycode[i] != 0xFF)
		{
			uni_char key_local[2];
			key_local[0] = i;
			key_local[1] = '\0';
			OpString key_string;
			key_string.Set(key_local);

			fprintf(stderr, "Key Num: %u, Char: %s, KeyCode: %d, Modifiers: 0x%02x\n", i, key_string.UTF8AllocL(), m_keycodetable.keycode[i], m_keycodetable.state[i]);
		}
	}
}


