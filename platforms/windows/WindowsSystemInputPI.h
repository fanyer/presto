// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef WINDOWS_SYSTEM_INPUT_PI_H
#define WINDOWS_SYSTEM_INPUT_PI_H

#include "adjunct/desktop_pi/system_input_pi.h"

class WindowsSystemInputPI : public SystemInputPI
{
public:
	WindowsSystemInputPI();
	
	/**
	 * Call anything you need to init for system input
	 */
	virtual OP_STATUS Init();

	/*
	 * Performs a mouse click at the given screen
	 * coords. 
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to click
	 * @param num_clicks	Number of clicks on the button
	 * @param modifier		List of modifiers to have down during the mouse click
	 *
	 * @return void
	 */
	virtual void Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier);
	
	/*
	 * Performs a mouse down at the given screen coords. 
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to click down
	 * @param modifier		List of modifiers to have down before the mouse click
	 *
	 * @return void
	 */
	virtual void MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);
	
	/*
	 * Performs a mouse up at the given screen coords. 
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to release
	 * @param modifier		List of modifiers to be released after the mouse up
	 *
	 * @return void
	 */
	virtual void MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);
	
	/*
	 * Moves the mouse to the cordinates given. Will also hold down mouse button and 
	 * modifiers during the move if specified
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to hold down during the move
	 * @param modifier		List of modifiers to have down during the mouse move
	 *
	 * @return void
	 */
	virtual void MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier);
	
	/**
	 * Performs a key press (including modifiers if required)
	 *
	 * @param key			Key to press if op_key is OP_KEY_INVALID
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to have down during the keypress
	 *
	 * @return void
	 */
	virtual void PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);

	/**
	 * Performs a key down (including modifiers if required)
	 *
	 * @param key			Key to press if op_key is OP_KEY_INVALID
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to have down before the keypress
	 *
	 * @return void
	 */
	virtual void KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);
	
	/**
	 * Performs a key up (including modifiers if required)
	 *
	 * @param key			Key to press if op_key is OP_KEY_INVALID
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to release after the keypress
	 *
	 * @return void
	 */
	virtual void KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier);

	/**
	 * Used if you cannot get the coordinates of the menuitems to click them (Needed for mac).
	 */
	virtual void PressMenu(const OpStringC &, bool popmenu) { }

private:
	// Functions to convert to Windows defines from Opera
	DWORD ConvertToWinButtonInput(MouseButton button, BOOL up);
	WORD  ConvertToWinKey(ShiftKeyState modifier);

	// Internal mouse functions
	void  PressMouseButton(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier, BOOL up);
	
	// Internal keyup/down functions
	SHORT GetKeyCode(uni_char key, OpVirtualKey op_key, ShiftKeyState &modifier);
	void  KeyDownInternal(SHORT key_code, ShiftKeyState modifier);
	void  KeyUpInternal(SHORT key_code, ShiftKeyState modifier);

	// Sends all of the modifiers held down
	void SendModifiers(ShiftKeyState modifier, BOOL up);
	// Sends a single modifier held down
	void SendModifier(ShiftKeyState modifier, BOOL up);
};

#endif // WINDOWS_SYSTEM_INPUT_PI_H
