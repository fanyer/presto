/**
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef X11API

#include "platforms/x11api/utils/OpKeyToX11Keysym.h"
#include "platforms/utilix/x11_all.h"

unsigned int OpKeyToX11Keysym(OpKey::Code key, OpKey::Location location, ShiftKeyState shiftstate, const uni_char *key_value)
{
    switch (key)
    {
#ifdef OP_KEY_CANCEL_ENABLED
    case OP_KEY_CANCEL:
	return XK_Cancel;
#endif // OP_KEY_CANCEL_ENABLED
#ifdef OP_KEY_BACKSPACE_ENABLED
    case OP_KEY_BACKSPACE:
	return XK_BackSpace;
#endif // OP_KEY_BACKSPACE_ENABLED
#ifdef OP_KEY_TAB_ENABLED
    case OP_KEY_TAB:
	return XK_Tab;
#endif // OP_KEY_TAB_ENABLED
#ifdef OP_KEY_CLEAR_ENABLED
    case OP_KEY_CLEAR:return XK_Clear;
#endif // OP_KEY_CLEAR_ENABLED
#ifdef OP_KEY_ENTER_ENABLED
    case OP_KEY_ENTER:return XK_Return;
#endif // OP_KEY_ENTER_ENABLED
#ifdef OP_KEY_SHIFT_ENABLED
    case OP_KEY_SHIFT:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Shift_R;
	else
	    return XK_Shift_L;
#endif // OP_KEY_SHIFT_ENABLED
#ifdef OP_KEY_CTRL_ENABLED
    case OP_KEY_CTRL:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Control_R;
	else
	    return XK_Control_L;
#endif // OP_KEY_CTRL_ENABLED
#ifdef OP_KEY_ALT_ENABLED
    case OP_KEY_ALT:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Alt_R;
	else
	    return XK_Alt_L;
#endif // OP_KEY_ALT_ENABLED
#ifdef OP_KEY_PAUSE_ENABLED
    case OP_KEY_PAUSE:
	return XK_Pause;
#endif // OP_KEY_PAUSE_ENABLED
#ifdef OP_KEY_CAPS_LOCK_ENABLED
    case OP_KEY_CAPS_LOCK:
	return XK_Caps_Lock;
#endif // OP_KEY_CAPS_LOCK_ENABLED
#ifdef OP_KEY_KANJI_ENABLED
    case OP_KEY_KANJI:
	return XK_Kanji;
#endif // OP_KEY_KANJI_ENABLED
#ifdef OP_KEY_FINAL_ENABLED
    case OP_KEY_FINAL:
	return XK_Cancel;
#endif // OP_KEY_FINAL_ENABLED
#ifdef OP_KEY_ESCAPE_ENABLED
    case OP_KEY_ESCAPE:
	return XK_Escape;
#endif // OP_KEY_ESCAPE_ENABLED
#ifdef OP_KEY_CONVERT_ENABLED
    case OP_KEY_CONVERT:
	return XK_Henkan_Mode;
#endif // OP_KEY_CONVERT_ENABLED
#ifdef OP_KEY_NONCONVERT_ENABLED
    case OP_KEY_NONCONVERT:
	return XK_Muhenkan;
#endif // OP_KEY_NONCONVERT_ENABLED
#ifdef OP_KEY_ACCEPT_ENABLED
    case OP_KEY_ACCEPT:
	return XK_Execute;
#endif // OP_KEY_ACCEPT_ENABLED
#ifdef OP_KEY_MODECHANGE_ENABLED
    case OP_KEY_MODECHANGE:
	return XK_Mode_switch;
#endif // OP_KEY_MODECHANGE_ENABLED
#ifdef OP_KEY_SPACE_ENABLED
    case OP_KEY_SPACE:
	return XK_space;
#endif // OP_KEY_SPACE_ENABLED
#ifdef OP_KEY_PAGEUP_ENABLED
    case OP_KEY_PAGEUP:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Page_Up;
	else
	    return XK_Page_Up;
#endif // OP_KEY_PAGEUP_ENABLED
#ifdef OP_KEY_PAGEDOWN_ENABLED
    case OP_KEY_PAGEDOWN:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Page_Down;
	else
	    return XK_Page_Down;
#endif // OP_KEY_PAGEDOWN_ENABLED
#ifdef OP_KEY_END_ENABLED
    case OP_KEY_END:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_End;
	else
	    return XK_End;
#endif // OP_KEY_END_ENABLED
#ifdef OP_KEY_HOME_ENABLED
    case OP_KEY_HOME:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Home;
	else
	    return XK_Home;
#endif // OP_KEY_HOME_ENABLED
#ifdef OP_KEY_LEFT_ENABLED
    case OP_KEY_LEFT:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Left;
	else
	    return XK_Left;
#endif // OP_KEY_LEFT_ENABLED
#ifdef OP_KEY_UP_ENABLED
    case OP_KEY_UP:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Up;
	else
	    return XK_Up;
#endif // OP_KEY_UP_ENABLED
#ifdef OP_KEY_RIGHT_ENABLED
    case OP_KEY_RIGHT:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Right;
	else
	    return XK_Right;
#endif // OP_KEY_RIGHT_ENABLED
#ifdef OP_KEY_DOWN_ENABLED
    case OP_KEY_DOWN:
	if (location == OpKey::LOCATION_NUMPAD)
	    return XK_KP_Down;
	else
	    return XK_Down;
#endif // OP_KEY_DOWN_ENABLED
#ifdef OP_KEY_SELECT_ENABLED
    case OP_KEY_SELECT:
	return XK_Select;
#endif // OP_KEY_SELECT_ENABLED
#ifdef OP_KEY_PRINT_ENABLED
    case OP_KEY_PRINT:
	return XK_Print;
#endif // OP_KEY_PRINT_ENABLED
#ifdef OP_KEY_EXECUTE_ENABLED
    case OP_KEY_EXECUTE:
	return XK_Execute;
#endif // OP_KEY_EXECUTE_ENABLED
#ifdef OP_KEY_PRINTSCREEN_ENABLED
    case OP_KEY_PRINTSCREEN:
	return XK_Print;
#endif // OP_KEY_PRINTSCREEN_ENABLED
#ifdef OP_KEY_INSERT_ENABLED
    case OP_KEY_INSERT:
	return XK_Insert;
#endif // OP_KEY_INSERT_ENABLED
#ifdef OP_KEY_DELETE_ENABLED
    case OP_KEY_DELETE:
	return XK_Delete;
#endif // OP_KEY_DELETE_ENABLED
#ifdef OP_KEY_HELP_ENABLED
    case OP_KEY_HELP:
	return XK_Help;
#endif // OP_KEY_HELP_ENABLED

#ifdef OP_KEY_WINDOW_ENABLED
    case OP_KEY_WINDOW:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Super_R;
	else
	    return XK_Super_L;
#endif // OP_KEY_WINDOW_ENABLED
#ifdef OP_KEY_MAC_CTRL_ENABLED
    case OP_KEY_MAC_CTRL:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Meta_R;
	else
	    return XK_Meta_L;
#endif // OP_KEY_MAC_CTRL_ENABLED
#ifdef OP_KEY_CONTEXT_MENU_ENABLED
    case OP_KEY_CONTEXT_MENU:
	return XK_Menu;
#endif // OP_KEY_CONTEXT_MENU_ENABLED
#ifdef OP_KEY_SLEEP_ENABLED
    case OP_KEY_SLEEP:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Hyper_R;
	else
	    return XK_Hyper_L;
#endif // OP_KEY_CONTEXT_MENU_ENABLED

#ifdef OP_KEY_NUMPAD0_ENABLED
    case OP_KEY_NUMPAD0:
	return XK_KP_0;
#endif // OP_KEY_NUMPAD0_ENABLED
#ifdef OP_KEY_NUMPAD1_ENABLED
    case OP_KEY_NUMPAD1:
	return XK_KP_1;
#endif // OP_KEY_NUMPAD1_ENABLED
#ifdef OP_KEY_NUMPAD2_ENABLED
    case OP_KEY_NUMPAD2:
	return XK_KP_2;
#endif // OP_KEY_NUMPAD2_ENABLED
#ifdef OP_KEY_NUMPAD3_ENABLED
    case OP_KEY_NUMPAD3:
	return XK_KP_3;
#endif // OP_KEY_NUMPAD3_ENABLED
#ifdef OP_KEY_NUMPAD4_ENABLED
    case OP_KEY_NUMPAD4:
	return XK_KP_4;
#endif // OP_KEY_NUMPAD4_ENABLED
#ifdef OP_KEY_NUMPAD5_ENABLED
    case OP_KEY_NUMPAD5:
	return XK_KP_5;
#endif // OP_KEY_NUMPAD5_ENABLED
#ifdef OP_KEY_NUMPAD6_ENABLED
    case OP_KEY_NUMPAD6:
	return XK_KP_6;
#endif // OP_KEY_NUMPAD6_ENABLED
#ifdef OP_KEY_NUMPAD7_ENABLED
    case OP_KEY_NUMPAD7:
	return XK_KP_7;
#endif // OP_KEY_NUMPAD7_ENABLED
#ifdef OP_KEY_NUMPAD8_ENABLED
    case OP_KEY_NUMPAD8:
	return XK_KP_8;
#endif // OP_KEY_NUMPAD8_ENABLED
#ifdef OP_KEY_NUMPAD9_ENABLED
    case OP_KEY_NUMPAD9:
	return XK_KP_9;
#endif // OP_KEY_NUMPAD9_ENABLED

#ifdef OP_KEY_MULTIPLY_ENABLED
    case OP_KEY_MULTIPLY:
	return XK_KP_Multiply;
#endif // OP_KEY_MULTIPLY_ENABLED
#ifdef OP_KEY_ADD_ENABLED
    case OP_KEY_ADD:
	return XK_KP_Add;
#endif // OP_KEY_ADD_ENABLED
#ifdef OP_KEY_SEPARATOR_ENABLED
    case OP_KEY_SEPARATOR:
	return XK_KP_Separator;
#endif // OP_KEY_SEPARATOR_ENABLED
#ifdef OP_KEY_SUBTRACT_ENABLED
    case OP_KEY_SUBTRACT:
	return XK_KP_Subtract;
#endif // OP_KEY_SUBTRACT_ENABLED
#ifdef OP_KEY_DECIMAL_ENABLED
    case OP_KEY_DECIMAL:
	return XK_KP_Decimal;
#endif // OP_KEY_DECIMAL_ENABLED
#ifdef OP_KEY_DIVIDE_ENABLED
    case OP_KEY_DIVIDE:
	return XK_KP_Divide;
#endif // OP_KEY_DIVIDE_ENABLED

#ifdef OP_KEY_F1_ENABLED
    case OP_KEY_F1:return XK_F1;
#endif // OP_KEY_F1_ENABLED
#ifdef OP_KEY_F2_ENABLED
    case OP_KEY_F2:return XK_F2;
#endif // OP_KEY_F2_ENABLED
#ifdef OP_KEY_F3_ENABLED
    case OP_KEY_F3:return XK_F3;
#endif // OP_KEY_F3_ENABLED
#ifdef OP_KEY_F4_ENABLED
    case OP_KEY_F4:return XK_F4;
#endif // OP_KEY_F4_ENABLED
#ifdef OP_KEY_F5_ENABLED
    case OP_KEY_F5:return XK_F5;
#endif // OP_KEY_F5_ENABLED
#ifdef OP_KEY_F6_ENABLED
    case OP_KEY_F6:return XK_F6;
#endif // OP_KEY_F6_ENABLED
#ifdef OP_KEY_F7_ENABLED
    case OP_KEY_F7:return XK_F7;
#endif // OP_KEY_F7_ENABLED
#ifdef OP_KEY_F8_ENABLED
    case OP_KEY_F8:return XK_F8;
#endif // OP_KEY_F8_ENABLED
#ifdef OP_KEY_F9_ENABLED
    case OP_KEY_F9:return XK_F9;
#endif // OP_KEY_F9_ENABLED
#ifdef OP_KEY_F10_ENABLED
    case OP_KEY_F10:return XK_F10;
#endif // OP_KEY_F10_ENABLED
#ifdef OP_KEY_F11_ENABLED
    case OP_KEY_F11:return XK_F11;
#endif // OP_KEY_F11_ENABLED
#ifdef OP_KEY_F12_ENABLED
    case OP_KEY_F12:return XK_F12;
#endif // OP_KEY_F12_ENABLED
#ifdef OP_KEY_F13_ENABLED
    case OP_KEY_F13:return XK_F13;
#endif // OP_KEY_F13_ENABLED
#ifdef OP_KEY_F14_ENABLED
    case OP_KEY_F14:return XK_F14;
#endif // OP_KEY_F14_ENABLED
#ifdef OP_KEY_F15_ENABLED
    case OP_KEY_F15:return XK_F15;
#endif // OP_KEY_F15_ENABLED
#ifdef OP_KEY_F16_ENABLED
    case OP_KEY_F16:return XK_F16;
#endif // OP_KEY_F16_ENABLED
#ifdef OP_KEY_F17_ENABLED
    case OP_KEY_F17:return XK_F17;
#endif // OP_KEY_F17_ENABLED
#ifdef OP_KEY_F18_ENABLED
    case OP_KEY_F18:return XK_F18;
#endif // OP_KEY_F18_ENABLED
#ifdef OP_KEY_F19_ENABLED
    case OP_KEY_F19:return XK_F19;
#endif // OP_KEY_F19_ENABLED
#ifdef OP_KEY_F20_ENABLED
    case OP_KEY_F20:return XK_F20;
#endif // OP_KEY_F20_ENABLED
#ifdef OP_KEY_F21_ENABLED
    case OP_KEY_F21:return XK_F21;
#endif // OP_KEY_F21_ENABLED
#ifdef OP_KEY_F22_ENABLED
    case OP_KEY_F22:return XK_F22;
#endif // OP_KEY_F22_ENABLED
#ifdef OP_KEY_F23_ENABLED
    case OP_KEY_F23:return XK_F23;
#endif // OP_KEY_F23_ENABLED
#ifdef OP_KEY_F24_ENABLED
    case OP_KEY_F24:return XK_F24;
#endif // OP_KEY_F24_ENABLED

#ifdef OP_KEY_NUM_LOCK_ENABLED
    case OP_KEY_NUM_LOCK:
	return XK_Num_Lock;
#endif // OP_KEY_NUM_LOCK_ENABLED
#ifdef OP_KEY_SCROLL_LOCK_ENABLED
    case OP_KEY_SCROLL_LOCK:
	return XK_Scroll_Lock;
#endif // OP_KEY_SCROLL_LOCK_ENABLED
#ifdef OP_KEY_META_ENABLED
    case OP_KEY_META:
	if (location == OpKey::LOCATION_RIGHT)
	    return XK_Meta_R;
	else
	    return XK_Meta_L;
#endif // OP_KEY_META_ENABLED
    }

    if (key >= OP_KEY_0 && key <= OP_KEY_9)
	return (XK_0 + (key - OP_KEY_0));
    else if (key >= OP_KEY_A && key <= OP_KEY_Z)
	if (shiftstate & SHIFTKEY_SHIFT)
	    return (XK_A + (key - OP_KEY_A));
	else
	    return (XK_a + (key - OP_KEY_A));

    if (!key_value)
    {
	OP_ASSERT(key != OP_KEY_PROCESSKEY && !"Unmapped function key");
	return XK_VoidSymbol;
    }

    if (key_value[0] < 0x100)
	/* Unicode and X11 agree down here. */
	return key_value[0];
    else
    {
	/* U+ABCDEF can be represented by keysym 0x01abcdef.
	   (http://www.cl.cam.ac.uk/~mgk25/unicode.html#x11) */
	int dummy;
	UnicodePoint cp = Unicode::GetUnicodePoint(key_value, 2, dummy);
	return (cp & 0xffffff) | 0x1000000;
    }
}

#endif // X11API
