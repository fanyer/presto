/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** karie, Espen Sand
*/

#ifndef __X11_SYSTEMINPUT_H__
#define __X11_SYSTEMINPUT_H__

#include "modules/hardcore/keys/opkeys.h"
#include "adjunct/desktop_pi/system_input_pi.h"
#include "platforms/utilix/x11_all.h" // Needed to define Bool, otherwise x11types.h is sufficient

class OpDLL;

// // Ripped off mac. Size based on what a char can be which is a uni_char for now
#define keyCodeTableSize 0xFFFF

// Struct to hold the reverse lookup keycode and char
typedef struct
{
	void Reset()
	{
		op_memset(&keycode, 0xFF, sizeof(keycode));
		op_memset(&state, 0, sizeof(state));
	}

	int GetKeyCode(uni_char key)
	{
		if (keycode[key] == 0xff)
			return 0;
		return keycode[key];
	}

	int GetState(uni_char key)
	{
		return state[key];
	}

 	KeyCode keycode[keyCodeTableSize];
 	UINT16 state[keyCodeTableSize];
} KeyboardKeyCodeTable;


class X11SystemInput : public SystemInputPI
{

public:
	static OP_STATUS Create(SystemInputPI **new_system_input);

	// SystemInputPI API
	void Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier);	
	void PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);
	void KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);
	void KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);

	void MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);
	void MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);
	void MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);

	// Implementation of this is only neccessary if menu coordinates cannot be returned by the platform
	void PressMenu(const OpStringC& menu_text, bool popmenu) { }


	OP_STATUS Init();

	/**
	 * Build table mapping each character to keycode and modifiers
	 */
	OP_STATUS CreateKeyCodeTableByEvents();

	/**
	 * Initializes object
	 */
	X11SystemInput();

	/**
	 * Attempts a connection to test interface
	 *
	 * @return OpStatus::OK on success, otherwise an error code.
	 */
	OP_STATUS InitLibrary();

private:
	class X11KeyState
	{
	public:
		X11KeyState(uni_char key, OpVirtualKey virtual_key, ShiftKeyState shift_state, OpKey::Location location)
			:m_key_code(0), m_modifier(SHIFTKEY_NONE), m_key(key), m_virtual_key(virtual_key), m_shift_state(shift_state), m_location(location) {}

		uni_char GetKey() const { return m_key; }
		OpVirtualKey GetVirtualKey() const { return m_virtual_key; }
		ShiftKeyState GetShiftState() const { return m_shift_state; }
		OpKey::Location GetLocation() const { return m_location; }

		int GetKeyCode() const { return m_key_code; }
		ShiftKeyState GetModifier() const { return m_modifier | m_shift_state; }
		void SetKeyCode(int key_code) { m_key_code = key_code; }
		void SetModifier(ShiftKeyState modifier) { m_modifier = modifier; }

	private:
		int m_key_code;
		ShiftKeyState m_modifier;    // Shift key state generated locally
		uni_char m_key;
		OpVirtualKey m_virtual_key;
		ShiftKeyState m_shift_state; // Shift key state as given by Opera
		OpKey::Location m_location;
	};

	struct XtestFunctions
	{
		typedef int (*xtestQueryExtensionFunction)(X11Types::Display *display, int *event_base, int *error_base, int *major_version, int *minor_version);
		typedef void (*xtestFakeKeyEventFunction)(X11Types::Display *display, unsigned int keycode, X11Types::Bool is_press, unsigned long delay);
		typedef void (*xtestFakeButtonEventFunction)(X11Types::Display *display, unsigned int button, X11Types::Bool is_press, unsigned long delay);
		typedef void (*xtestFakeMotionEventFunction)(X11Types::Display *display, int screen_number, int x, int y, unsigned long delay);

		xtestQueryExtensionFunction xtestQueryExtension;
		xtestFakeKeyEventFunction xtestFakeKeyEvent;
		xtestFakeButtonEventFunction xtestFakeButtonEvent;
		xtestFakeMotionEventFunction xtestFakeMotionEvent;
	};

private:
	void SetKeyState(X11KeyState& state);

	/**
	 * Returns the library name used to load the test interface
	 */
	const uni_char* GetLibraryName();

	/**
	 * Press modifier and key
	 */
	void KeyDownInternal(int key_code, ShiftKeyState modifier);
	/**
	 * Release key and modifier
	 */
	void KeyUpInternal(int key_code, ShiftKeyState modifier);

	/**
	 * Press or release modifier
	 */
	void ModifierChanged(ShiftKeyState modifier, X11Types::Bool is_press);

	/**
	 * Returns the X11 key code of the Opera key symbol
	 *
	 * @param key Opera key symbol
	 *
	 * @return The key code
	 */
	int VirtualKeyToKeyCode(OpVirtualKey virtual_key, OpKey::Location location);

	/**
	 * Returns the X11 equivalent of the Opera mouse button symbol
	 *
	 * @param button Opera mouse button symbol
	 *
	 * @return The X11 mouse button
	 */
	int OperaToX11Button(INT32 button);

	/**
	 * Convert X11 state mask to opera modifiers
	 *
	 * @param state X11 state mask
	 *
	 * @return The opera modifiers
	 */
	int X11StateToOperaModifier(int state);

	/**
	 * Debug function
	 */
	void PrintKeyMapping();

private:
	OpDLL* m_dll;
	int m_event_base;
	int m_error_base;
	XtestFunctions m_functions;
	KeyboardKeyCodeTable m_keycodetable;
	bool m_table_created;
};
#endif
