// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef SYSTEM_INPUT_PI_H
#define SYSTEM_INPUT_PI_H

#include "modules/hardcore/keys/opkeys.h"

/*
 * SystemInputPI is and interface to allow input at the system level
 * of the operating system. This will allow us to send real mouse or
 * keyboard clicks and presses to Opera.
 *
 * The primary use of this PI is in conjunction with the desktop_scope
 * functionality which when tied to OperaWatir allow us to automate
 * UI testing via scripts. In general this PI should not be used for
 * anything except automated UI testing.
 *
 */
class SystemInputPI
{
public:
	static	OP_STATUS Create(SystemInputPI **new_system_input);

	virtual ~SystemInputPI() {}
	
	/**
	 * Call anything you need to init for system input
	 */
	virtual OP_STATUS Init() = 0;

	/*
	 * Performs a mouse click at the given screen
	 * coords. 
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to click
	 * @param num_clicks	Number of clicks on the button
	 * @param modifier		List of modifiers to have down during the mouse click(s)
	 
	 *
	 * @return void
	 */
	virtual void Click(INT32 x, INT32 y, MouseButton button, INT32 num_clicks, ShiftKeyState modifier) = 0;

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
	virtual void MouseDown(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier) = 0;
	
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
	virtual void MouseUp(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier) = 0;
	
	/*
	 * Moves the mouse to the cordinates given. Will also hold down mouse button and 
	 * modifiers during the move if specified
	 *
	 * Note: The button and modifier are only required if your system needs them to
	 *       construct the moving/dragging event. Normally they can be ignored.
	 *
	 * @param x				x coordinate in screen coordinated
	 * @param y				y coordinate in screen coordinated
	 * @param button		Button to hold down during the move
	 * @param modifier		List of modifiers to have down during the mouse move
	 *
	 * @return void
	 */
	virtual void MouseMove(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier) = 0;
	
	/**
	 * Performs a key press (including modifiers if required)
	 *
	 * @param key			Key to press if op_key is OP_KEY_INVALID
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to have down during the keypress
	 *
	 * @return void
	 */
	virtual void PressKey(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier) = 0;


	/**
	 * Performs a key down (including modifiers if required)
	 *
	 * @param key			Key to press 
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to have down before the keypress
	 *
	 * @return void
	 */
	virtual void KeyDown(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier) = 0;
	
	/**
	 * Performs a key up (including modifiers if required)
	 *
	 * @param key			Key to press
	 * @param op_key        OP_KEY to press if key not set
	 * @param modifier		List of modifiers to release after the keypress
	 *
	 * @return void
	 */
	virtual void KeyUp(uni_char key, OpVirtualKey op_key, ShiftKeyState modifier) = 0;

	/**
	 * Presses a menu item based on text. (Scope)
	 *
	 * Note: Only required if menu's can't be located by co-ordinates
	 *
	 * If used, this function needs to call MenuItemPressed to inform
	 * the webdriver that the menu item was pressed
	 *
	 * @param menu_text		Text on the menu to press
	 * @param popupmenu		Set to true for pop-menus like on drop downs on Mac
	 *
	 * @return void
	 */
	virtual void PressMenu(const OpStringC& menu_text, bool popupmenu) = 0;
};

#endif // SYSTEM_INPUT_PI_H
