// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef MAC_SYSTEM_INPUT_PI_H
#define MAC_SYSTEM_INPUT_PI_H

#include "adjunct/desktop_pi/system_input_pi.h"

// Size based on what a char can be which is a uni_char for now
#define keyCodeTableSize 0xFFFF

// Struct to hold the reverse lookup keycode and char
typedef struct 
{
	UINT16 keycode[keyCodeTableSize];
	UINT16 modifiers[keyCodeTableSize];
} KeyboardKeyCodeTable;


class MacSystemInputPI : public SystemInputPI
{
public:
	MacSystemInputPI();
	
	/**
	 * Call anything you need to init for system input
	 */
	virtual OP_STATUS Init();
	
	/*
	 * Performs a mouse click at the given screen coords. 
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
	 * Presses a menu item based on text. 
	 *
	 * Note: Only required if menu's can't be located by co-ordinates
	 *
	 * If used, this function needs to call MenuItemPressed to inform
	 * the web-driver that the menu item was pressed
	 *
	 * @param menu_text		Text on the menu to press
	 * @param popmenu		Set to true for pop-menus like on drop downs on Mac
	 *
	 * @return void
	 */
	virtual void PressMenu(const OpStringC& menu_text, bool popmenu);
	
	/**
	 * Called by the platform when a menu item is highlighted. Currently used
	 * my Mac to navigate and test what the current menu item is and hit enter
	 * when the correct menu item is found.
	 *
	 * If used, this function needs to call MenuItemPressed to inform
	 * the web-driver that the menu item was pressed
	 *
	 * Note: Only required if menu's can't be located by co-ordinates
	 *
	 * @param menu_text		Text on the menu to press
	 * @param main_menu		TRUE if this is on the main menu
	 * @param is_submenu	TRUE if this is a submenu item
	 *
	 * @return void
	 */
	virtual void MenuItemHighlighted(const OpStringC& menu_text, BOOL main_menu, BOOL is_submenu);

	/**
	 * Called each time a menu is show or hidden.
	 *
	 * @param tracking	TRUE when a menu is up or FALSE when no menus are shown
	 *
	 * @return void
	 */
	virtual void MenuTracking(BOOL tracking);
	
	/**
	 * Sets the currently active menu
	 *
	 * @param nsmenu	menu that is becoming active
	 *
	 * @return void
	 */
	virtual void SetActiveNSMenu(void *nsmenu);
	virtual void *GetActiveNSMenu() { return m_active_nsmenu; }

	/**
	 * Sets the currently active pop-menu
	 *
	 * @param nspopupbutton	Pop-menu that is becoming active
	 *
	 * @return void
	 */
	virtual void SetActiveNSPopUpButton(void *nspopupbutton) { m_active_nspopupbutton = nspopupbutton; }
	
private:
	// Sends all the modifiers up or down
	void			SendModifiers(ShiftKeyState modifier, BOOL up);
	// Send a single modifier
	void			SendModifier(ShiftKeyState modifier, BOOL up);

	
	// Conversion functions to Mac system defines
	CGMouseButton	ConvertToMacButton(MouseButton button);
	CGEventType		ConvertToMacButtonEvent(MouseButton button, BOOL up);
	CGEventType		ConvertToMacButtonDragEvent(MouseButton button);
	CGEventFlags	ConvertToMacModifiers(ShiftKeyState modifier);
	CGKeyCode		ConvertToMacModifierKeyCode(ShiftKeyState modifier);
	CGKeyCode		ConvertToMacKeyCode(const OpVirtualKey key);

	// Mouse internal functions
	void			PressMouseButton(INT32 x, INT32 y, MouseButton button, ShiftKeyState modifier, BOOL up);
	
	// Get the keycode from key and adjust modifier as required
	CGKeyCode		GetKeyCode(uni_char key, OpVirtualKey op_key, ShiftKeyState &modifier);
	void			KeyDownInternal(CGKeyCode key_code, ShiftKeyState modifier);
	void			KeyUpInternal(CGKeyCode key_code, ShiftKeyState modifier);

	// Init a reverse lookup table to get the keycode from the character
	OP_STATUS		InitKeyboardKeyCodeTable();

	KeyboardKeyCodeTable m_keycodetable;
	
	OpString	m_search_menu_item;			// Menu item to search for
	OpString	m_previous_menu_item;		// Holds the text of the previous menu item to check if we make it to the end of a menu
	OpString	m_first_sub_menu_item;		// First item in a submenu when it is opened
	BOOL		m_menu_tracking;			// TRUE when a menu is shown otherwise FALSE
	BOOL		m_sub_menu_next;			// Flag to trap the first item in a sub menu when it's highlighted
	void		*m_active_nsmenu;			// NSMenu pointer to the currently active menu
	void		*m_active_nspopupbutton;	// NSPopUpButton pointer to the currently active pop-menu (i.e. drop down menu)
};

#endif // MAC_SYSTEM_INPUT_PI_H
