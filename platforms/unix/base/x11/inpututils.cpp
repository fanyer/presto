/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "inpututils.h"

#include "modules/unicode/unicode.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/inputmanager/inputmanager.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"

#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/utilix/x11_all.h"
#include "x11_globals.h"
#include "x11_windowwidget.h"

#include <X11/XF86keysym.h>

#include <time.h>
#include <sys/time.h>

namespace InputUtils
{
	struct KeyValue
	{
	public:
		KeyValue() : keysym(None) {}
		void Set(KeySym ks, const OpString& txt)
		{
			keysym = ks;
			text.Set(txt);
		}

		KeySym keysym;
		OpString text;
	};

	OpVirtualKey OMenuHotKey = OP_KEY_ALT;
	BOOL can_show_O_menu = FALSE;
	BOOL mouse_navigation_active = FALSE;

	unsigned int modifier_mask = 0;
	int button_flags = 0;

	int clickbutton = 0;
	X11Types::Window clickwindow = None;
	int clickcount = 0;
	double lastclicktime = 0;
	OpPoint lastclickpos;
	OpString8 keybuf;
	KeyValue last_keypress;


	/** maximum number of milliseconds between each mouse click to be
		considered as a double-click, triple-click etc. */
	const double multiclick_treshold = 500;

	bool TranslateKeyEvent(XEvent* event, KeyContent& content)
	{
		content.Reset();

		if (event->type != KeyPress && event->type != KeyRelease)
			return false;

		OpString text;
		KeySym keysym = NoSymbol;
		GetKey(event, keysym, text);

		OpKey::Location location = OpKey::LOCATION_STANDARD;
		OpVirtualKey virtual_key = OP_KEY_INVALID;

		bool allow_keycode_mapping = true;

		// The keysym to virtual key part should match the reverse operation we do in
		// VirtualKeyToKeySym. Any updates here must be done there as well.

		// 1. Non-character keysyms
		switch (keysym)
 		{
			case XK_BackSpace:    virtual_key = OP_KEY_BACKSPACE; break;
			case XK_Tab:          virtual_key = OP_KEY_TAB; break;
			case XK_ISO_Left_Tab: virtual_key = OP_KEY_TAB; break;
 			case XK_Return:       virtual_key = OP_KEY_ENTER; break;
 			case XK_KP_Enter:     virtual_key = OP_KEY_ENTER; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_space:        virtual_key = OP_KEY_SPACE; break;
 			case XK_Escape:       virtual_key = OP_KEY_ESCAPE; break;
 			case XK_Delete:       virtual_key = OP_KEY_DELETE; break;
 			case XK_KP_Delete:    virtual_key = OP_KEY_DELETE; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Home:         virtual_key = OP_KEY_HOME; break;
 			case XK_KP_Home:      virtual_key = OP_KEY_HOME; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Left:         virtual_key = OP_KEY_LEFT; break;
 			case XK_KP_Left:      virtual_key = OP_KEY_LEFT; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Up:           virtual_key = OP_KEY_UP; break;
 			case XK_KP_Up:        virtual_key = OP_KEY_UP; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Right:        virtual_key = OP_KEY_RIGHT; break;
 			case XK_KP_Right:     virtual_key = OP_KEY_RIGHT; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Down:         virtual_key = OP_KEY_DOWN; break;
 			case XK_KP_Down:      virtual_key = OP_KEY_DOWN; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Page_Up:      virtual_key = OP_KEY_PAGEUP; break;
 			case XK_KP_Page_Up:   virtual_key = OP_KEY_PAGEUP; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Page_Down:    virtual_key = OP_KEY_PAGEDOWN; break;
 			case XK_KP_Page_Down: virtual_key = OP_KEY_PAGEDOWN; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_End:          virtual_key = OP_KEY_END; break;
 			case XK_KP_End:       virtual_key = OP_KEY_END; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Insert:       virtual_key = OP_KEY_INSERT; break;
			case XK_KP_Insert:    virtual_key = OP_KEY_INSERT; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_KP_Multiply:  virtual_key = OP_KEY_MULTIPLY; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_KP_Add:       virtual_key = OP_KEY_ADD; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_KP_Subtract:  virtual_key = OP_KEY_SUBTRACT; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_KP_Divide:    virtual_key = OP_KEY_DIVIDE; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_KP_Separator: virtual_key = OP_KEY_SEPARATOR; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Menu:         virtual_key = OP_KEY_CONTEXT_MENU; break;
 			case XK_Cancel:       virtual_key = OP_KEY_CANCEL; break;
 			case XK_Break:        /* fallthrough */
 			case XK_Pause:        virtual_key = OP_KEY_PAUSE; break;
 			case XK_Caps_Lock:    virtual_key = OP_KEY_CAPS_LOCK; break;
 			case XK_Mode_switch:  virtual_key = OP_KEY_MODECHANGE; break;
 			case XK_Select:       virtual_key = OP_KEY_SELECT; break;
 			case XK_Execute:      virtual_key = OP_KEY_EXECUTE; break;
 			case XK_Sys_Req:      /* fallthrough */
 			case XK_Print:        virtual_key = OP_KEY_PRINTSCREEN; break;
 			case XK_Help:         virtual_key = OP_KEY_HELP; break;
 			case XK_KP_Decimal:   virtual_key = OP_KEY_DECIMAL; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Num_Lock:     virtual_key = OP_KEY_NUM_LOCK; location = OpKey::LOCATION_NUMPAD; break;
 			case XK_Scroll_Lock:  virtual_key = OP_KEY_SCROLL_LOCK; break;
 			case XK_Shift_L:      virtual_key = OP_KEY_SHIFT; location = OpKey::LOCATION_LEFT; break;
			case XK_Shift_R:      virtual_key = OP_KEY_SHIFT; location = OpKey::LOCATION_RIGHT; break;
			case XK_Control_L:    virtual_key = OP_KEY_CTRL; location = OpKey::LOCATION_LEFT; break;
			case XK_Control_R:    virtual_key = OP_KEY_CTRL; location = OpKey::LOCATION_RIGHT; break;
			case XK_Meta_L:       virtual_key = OP_KEY_META; location = OpKey::LOCATION_LEFT; break;
			case XK_Meta_R:       virtual_key = OP_KEY_META; location = OpKey::LOCATION_RIGHT; break;
			case XK_Alt_L:        virtual_key = OP_KEY_ALT; location = OpKey::LOCATION_LEFT; break;
			case XK_Alt_R:        virtual_key = OP_KEY_ALT; location = OpKey::LOCATION_RIGHT; break;
			case XK_Super_L:      virtual_key = OP_KEY_WINDOW; location = OpKey::LOCATION_LEFT; break;
			case XK_Super_R:      virtual_key = OP_KEY_WINDOW; location = OpKey::LOCATION_RIGHT; break;
			case XK_ISO_Level3_Shift: virtual_key = OP_KEY_INVALID; location = OpKey::LOCATION_RIGHT; allow_keycode_mapping = false; break;
			default:
				if (keysym >= XK_F1 && keysym <= XK_F24)
					virtual_key = static_cast<OpVirtualKey>(OP_KEY_F1 + (keysym - XK_F1));
				else if (keysym >= XK_F25 && keysym <= XK_F35)
					virtual_key = static_cast<OpVirtualKey>(OP_KEY_F25 + (keysym - XK_F25));
				else if (keysym >= XK_KP_0 && keysym <= XK_KP_9)
				{
					virtual_key = static_cast<OpVirtualKey>(OP_KEY_0 + (keysym - XK_KP_0));
					location = OpKey::LOCATION_NUMPAD;
				}
		}

		// 2. Numbers (0-9), letters (a-z) and plus, minus, period and comma characters
		for (int i=0; virtual_key == OP_KEY_INVALID && i<4; i++)
		{
			keysym = XKeycodeToKeysym(g_x11->GetDisplay(), event->xkey.keycode, i);
			switch (keysym)
			{
				case XK_plus:   virtual_key = OP_KEY_ADD; break;
				case XK_minus:  virtual_key = OP_KEY_SUBTRACT; break;
				case XK_comma:  virtual_key = OP_KEY_COMMA; break;
				case XK_period: virtual_key = OP_KEY_PERIOD; break;
				default:
					if (keysym >= XK_0 && keysym <= XK_9)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_0 + (keysym - XK_0));
					else if (keysym >= XK_a && keysym <= XK_z)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_A + (keysym - XK_a));
			}
		}

		// 3. The rest.
		// We use the raw keycodes. This means that we will produce the same virtual key
		// (OP_KEY_nnn) for a given key on the keyboard regardless of layout or language.
		// Allows scripts to work with non-western keyboards
		// Same order as in /usr/share/X11/xkb/keycodes/evdev

		if (virtual_key == OP_KEY_INVALID && allow_keycode_mapping)
		{
			switch (event->xkey.keycode)
			{
				case 94: /* '>' */ virtual_key = OP_KEY_OEM_102; break;

				case 49: virtual_key = OP_KEY_OEM_3; break;

				// 1 to 0 handled in default section

				case 20: virtual_key = OP_KEY_SUBTRACT; break;
				case 21: virtual_key = OP_KEY_ADD; break;
				case 22: virtual_key = OP_KEY_BACKSPACE; break;

				case 23: virtual_key = OP_KEY_TAB; break;
				case 24: virtual_key = OP_KEY_Q; break;
				case 25: virtual_key = OP_KEY_W; break;
				case 26: virtual_key = OP_KEY_E; break;
				case 27: virtual_key = OP_KEY_R; break;
				case 28: virtual_key = OP_KEY_T; break;
				case 29: virtual_key = OP_KEY_Y; break;
				case 30: virtual_key = OP_KEY_U; break;
				case 31: virtual_key = OP_KEY_I; break;
				case 32: virtual_key = OP_KEY_O; break;
				case 33: virtual_key = OP_KEY_P; break;
				case 34: virtual_key = OP_KEY_OPEN_BRACKET; break;
				case 35: virtual_key = OP_KEY_CLOSE_BRACKET; break;
				case 51: virtual_key = OP_KEY_OPEN_BACKSLASH; break; // Same as OP_KEY_OEM5
				case 36: virtual_key = OP_KEY_ENTER; break;

				case 66: virtual_key = OP_KEY_CAPS_LOCK; break;
				case 38: virtual_key = OP_KEY_A; break;
				case 39: virtual_key = OP_KEY_S; break;
				case 40: virtual_key = OP_KEY_D; break;
				case 41: virtual_key = OP_KEY_F; break;
				case 42: virtual_key = OP_KEY_G; break;
				case 43: virtual_key = OP_KEY_H; break;
				case 44: virtual_key = OP_KEY_J; break;
				case 45: virtual_key = OP_KEY_K; break;
				case 46: virtual_key = OP_KEY_L; break;
				case 47: virtual_key = OP_KEY_SEMICOLON; break;
				case 48: virtual_key = OP_KEY_QUOTE; break;

				case 50: virtual_key = OP_KEY_SHIFT; location = OpKey::LOCATION_LEFT; break;
				case 52: virtual_key = OP_KEY_Z; break;
				case 53: virtual_key = OP_KEY_X; break;
				case 54: virtual_key = OP_KEY_C; break;
				case 55: virtual_key = OP_KEY_V; break;
				case 56: virtual_key = OP_KEY_B; break;
				case 57: virtual_key = OP_KEY_N; break;
				case 58: virtual_key = OP_KEY_M; break;
				case 59: virtual_key = OP_KEY_COMMA; break;
				case 60: virtual_key = OP_KEY_PERIOD; break;
				case 61: virtual_key = OP_KEY_SLASH; break;
				case 62: virtual_key = OP_KEY_SHIFT; location = OpKey::LOCATION_RIGHT; break;

				case 64: virtual_key = OP_KEY_ALT; location = OpKey::LOCATION_LEFT; break;
				case 37: virtual_key = OP_KEY_CTRL; location = OpKey::LOCATION_LEFT; break;
				case 65: virtual_key = OP_KEY_SPACE; break;
				case 105: virtual_key = OP_KEY_CTRL; location = OpKey::LOCATION_RIGHT; break;
				case 108: virtual_key = OP_KEY_ALT; location = OpKey::LOCATION_RIGHT; break;
				case 133: virtual_key = OP_KEY_WINDOW; location = OpKey::LOCATION_LEFT; break;
				case 134: virtual_key = OP_KEY_WINDOW; location = OpKey::LOCATION_RIGHT; break;
				case 135: virtual_key = OP_KEY_CONTEXT_MENU; break;

				case 9: virtual_key = OP_KEY_ESCAPE; break;

				// F1 to F12 handled in default section

				case 107: virtual_key = OP_KEY_PRINTSCREEN; break;
				case 78: virtual_key = OP_KEY_SCROLL_LOCK; break;
				case 127: virtual_key = OP_KEY_PAUSE; break;

				case 118: virtual_key = OP_KEY_INSERT; break;
				case 110: virtual_key = OP_KEY_HOME; break;
				case 112: virtual_key = OP_KEY_PAGEUP; break;
				case 119: virtual_key = OP_KEY_DELETE; break;
				case 115: virtual_key = OP_KEY_END; break;
				case 117: virtual_key = OP_KEY_PAGEDOWN; break;

				case 111: virtual_key = OP_KEY_UP; break;
				case 113: virtual_key = OP_KEY_LEFT; break;
				case 116: virtual_key = OP_KEY_DOWN; break;
				case 114: virtual_key = OP_KEY_RIGHT; break;

				// We map to virtual keys as if numlock is active for the num pad
				case 77: virtual_key = OP_KEY_NUM_LOCK; location = OpKey::LOCATION_NUMPAD; break;
				case 106: virtual_key = OP_KEY_DIVIDE; location = OpKey::LOCATION_NUMPAD; break;
				case 63: virtual_key = OP_KEY_MULTIPLY; location = OpKey::LOCATION_NUMPAD; break;
				case 82: virtual_key = OP_KEY_SUBTRACT; location = OpKey::LOCATION_NUMPAD; break;
				case 79: virtual_key = OP_KEY_NUMPAD7; location = OpKey::LOCATION_NUMPAD; break;
				case 80: virtual_key = OP_KEY_NUMPAD8; location = OpKey::LOCATION_NUMPAD; break;
				case 81: virtual_key = OP_KEY_NUMPAD9; location = OpKey::LOCATION_NUMPAD; break;
				case 86: virtual_key = OP_KEY_ADD; location = OpKey::LOCATION_NUMPAD; break;
				case 83: virtual_key = OP_KEY_NUMPAD4; location = OpKey::LOCATION_NUMPAD; break;
				case 84: virtual_key = OP_KEY_NUMPAD5; location = OpKey::LOCATION_NUMPAD; break;
				case 85: virtual_key = OP_KEY_NUMPAD6; location = OpKey::LOCATION_NUMPAD; break;
				case 87: virtual_key = OP_KEY_NUMPAD1; location = OpKey::LOCATION_NUMPAD; break;
				case 88: virtual_key = OP_KEY_NUMPAD2; location = OpKey::LOCATION_NUMPAD; break;
				case 89: virtual_key = OP_KEY_NUMPAD3; location = OpKey::LOCATION_NUMPAD; break;
				case 104: virtual_key = OP_KEY_ENTER; location = OpKey::LOCATION_NUMPAD; break;
				case 90: virtual_key = OP_KEY_NUMPAD0; location = OpKey::LOCATION_NUMPAD; break;
				case 91: virtual_key = OP_KEY_DECIMAL; location = OpKey::LOCATION_NUMPAD; break;
				case 125: /* '=' */ virtual_key = OP_KEY_INVALID; location = OpKey::LOCATION_NUMPAD; break;

				// F13 to F24 handled in default section

				// Japanese
				case 101: virtual_key = OP_KEY_HK_TOGGLE   /* HKTG*/; break;
				case 100: virtual_key = OP_KEY_CONVERT     /* HENK*/; break;
				case 102: virtual_key = OP_KEY_NONCONVERT  /* MUHE*/; break;
				case 98:  virtual_key = OP_KEY_KATAKANA    /* KATA*/; break;
				case 99:  virtual_key = OP_KEY_HIRAGANA    /* HIRA*/; break;

				// JK keys we do not map to internal key symbols
				case 97:  /* AB11 - Japanese*/
				case 132: /* AE13 - Japanese*/
				case 103: /* JPCM - Japanese*/
				case 130: /* HNGL - Korean*/
				case 131: /* HJCV - Korean*/
					virtual_key = OP_KEY_INVALID; break;

				// Solaris compatibility
				case 121: virtual_key = OP_KEY_XF86XK_AUDIO_MUTE; break;
				case 122: virtual_key = OP_KEY_XF86XK_AUDIO_LOWER_VOLUME; break;
				case 123: virtual_key = OP_KEY_XF86XK_AUDIO_RAISE_VOLUME; break;
				case 124: virtual_key = OP_KEY_XF86XK_POWER_DOWN; break;
	    		case 136: virtual_key = OP_KEY_XF86XK_STOP; break;
				case 137: virtual_key = OP_KEY_REDO; break;
				case 138: virtual_key = OP_KEY_PROPERITES; break;
				case 139: virtual_key = OP_KEY_UNDO; break;
				case 140: virtual_key = OP_KEY_FRONT; break;
				case 141: virtual_key = OP_KEY_XF86XK_COPY; break;
				case 142: virtual_key = OP_KEY_XF86XK_OPEN; break;
				case 143: virtual_key = OP_KEY_XF86XK_PASTE; break;
				case 144: virtual_key = OP_KEY_XF86XK_SEARCH; break;
				case 145: virtual_key = OP_KEY_XF86XK_CUT; break;
				case 146: virtual_key = OP_KEY_HELP; break;

				// Internet keys
				case 109: virtual_key = OP_KEY_INVALID /* KEY_LINEFEED*/; break;
				case 126: virtual_key = OP_KEY_INVALID /* KEY_KPPLUSMINUS*/; location = OpKey::LOCATION_NUMPAD; break;
				case 128: virtual_key = OP_KEY_XF86XK_LAUNCH_A; break;
				case 129: virtual_key = OP_KEY_DECIMAL; location = OpKey::LOCATION_NUMPAD; break;
				case 147: virtual_key = OP_KEY_XF86XK_MENU_KB; break;
				case 148: virtual_key = OP_KEY_XF86XK_CALCULATOR; break;
				case 150: virtual_key = OP_KEY_SLEEP; break;
				case 151: virtual_key = OP_KEY_XF86XK_WAKE_UP; break;
				case 152: virtual_key = OP_KEY_XF86XK_EXPLORER; break;
				case 153: virtual_key = OP_KEY_XF86XK_SEND; break;
				case 155: virtual_key = OP_KEY_XF86XK_XFER; break;
				case 156: virtual_key = OP_KEY_XF86XK_LAUNCH1; break;
				case 157: virtual_key = OP_KEY_XF86XK_LAUNCH2; break;
				case 158: virtual_key = OP_KEY_XF86XK_WWW; break;
				case 159: virtual_key = OP_KEY_XF86XK_DOS; break;
				case 160: virtual_key = OP_KEY_XF86XK_SCREEN_SAVER; break;
				case 162: virtual_key = OP_KEY_XF86XK_ROTATE_WINDOWS; break;
				case 163: virtual_key = OP_KEY_XF86XK_MAIL; break;
				case 164: virtual_key = OP_KEY_XF86XK_FAVORITES; break;
				case 165: virtual_key = OP_KEY_XF86XK_MY_COMPUTER; break;
				case 166: virtual_key = OP_KEY_XF86XK_BACK; break;
				case 167: virtual_key = OP_KEY_XF86XK_FORWARD; break;
				case 168: /* fallthrough */
				case 169: /* fallthrough */
				case 170: virtual_key = OP_KEY_XF86XK_EJECT; break;
				case 171: virtual_key = OP_KEY_XF86XK_AUDIO_NEXT; break;
				case 172: virtual_key = OP_KEY_XF86XK_AUDIO_PLAY; break;
				case 173: virtual_key = OP_KEY_XF86XK_AUDIO_PREV; break;
				case 174: virtual_key = OP_KEY_XF86XK_AUDIO_STOP; break;
				case 175: virtual_key = OP_KEY_XF86XK_AUDIO_RECORD; break;
				case 176: virtual_key = OP_KEY_XF86XK_AUDIO_REWIND; break;
				case 177: virtual_key = OP_KEY_XF86XK_PHONE; break;
				case 179: virtual_key = OP_KEY_XF86XK_TOOLS; break;
				case 180: virtual_key = OP_KEY_XF86XK_HOME_PAGE; break;
				case 181: virtual_key = OP_KEY_XF86XK_RELOAD; break;
				case 182: virtual_key = OP_KEY_XF86XK_CLOSE; break;
				case 185: virtual_key = OP_KEY_XF86XK_SCROLL_UP; break;
				case 186: virtual_key = OP_KEY_XF86XK_SCROLL_DOWN; break;
				case 187: virtual_key = OP_KEY_INVALID; location = OpKey::LOCATION_NUMPAD; break; // '(' on numpad
				case 188: virtual_key = OP_KEY_INVALID; location = OpKey::LOCATION_NUMPAD; break;// ')' on numpad
				case 189: virtual_key = OP_KEY_XF86XK_NEW; break;
				case 190: virtual_key = OP_KEY_REDO; break;
				case 208: virtual_key = OP_KEY_XF86XK_AUDIO_PLAY; break;
				case 209: virtual_key = OP_KEY_XF86XK_AUDIO_PAUSE; break;
				case 210: virtual_key = OP_KEY_XF86XK_LAUNCH3; break;
				case 211: virtual_key = OP_KEY_XF86XK_LAUNCH4; break;
				case 212: virtual_key = OP_KEY_XF86XK_LAUNCH_B; break;
				case 213: virtual_key = OP_KEY_XF86XK_SUSPEND; break;
				case 214: virtual_key = OP_KEY_XF86XK_CLOSE; break;
				case 215: virtual_key = OP_KEY_XF86XK_AUDIO_PLAY; break;
				case 216: virtual_key = OP_KEY_XF86XK_AUDIO_FORWARD; break;
				case 218: virtual_key = OP_KEY_PRINT; break;
				case 220: virtual_key = OP_KEY_XF86XK_WEB_CAM; break;
				case 223: virtual_key = OP_KEY_XF86XK_MAIL; break;
				case 224: virtual_key = OP_KEY_XF86XK_MESSENGER; break;
				case 225: virtual_key = OP_KEY_XF86XK_SEARCH; break;
				case 226: virtual_key = OP_KEY_XF86XK_GO; break;
				case 227: virtual_key = OP_KEY_XF86XK_FINANCE; break;
				case 228: virtual_key = OP_KEY_XF86XK_GAME; break;
				case 229: virtual_key = OP_KEY_XF86XK_SHOP; break;
				case 231: virtual_key = OP_KEY_CANCEL; break;
				case 232: virtual_key = OP_KEY_XF86XK_MON_BRIGHTNESS_DOWN; break;
				case 233: virtual_key = OP_KEY_XF86XK_MON_BRIGHTNESS_UP; break;
				case 234: virtual_key = OP_KEY_XF86XK_AUDIO_MEDIA; break;
				case 235: virtual_key = OP_KEY_XF86XK_DISPLAY; break;
				case 236: virtual_key = OP_KEY_XF86XK_KBD_LIGHT_ON_OFF; break;
				case 237: virtual_key = OP_KEY_XF86XK_KBD_BRIGHTNESS_UP; break;
				case 238: virtual_key = OP_KEY_XF86XK_KBD_BRIGHTNESS_DOWN; break;
				case 239: virtual_key = OP_KEY_XF86XK_SEND; break;
				case 240: virtual_key = OP_KEY_XF86XK_REPLY; break;
				case 241: virtual_key = OP_KEY_XF86XK_MAIL_FORWARD; break;
				case 242: virtual_key = OP_KEY_XF86XK_SAVE; break;
				case 243: virtual_key = OP_KEY_XF86XK_DOCUMENTS; break;
				case 244: virtual_key = OP_KEY_XF86XK_BATTERY; break;
				case 245: virtual_key = OP_KEY_XF86XK_BLUETOOTH; break;
				case 246: virtual_key = OP_KEY_XF86XK_WLAN; break;
				case 247: virtual_key = OP_KEY_XF86XK_UWB; break;

				// Keys we do not map to internal key symbols
				case 120: /* KEY_MACRO*/
				case 149: /* KEY_SETUP*/
				case 161: /* KEY_DIRECTION*/
				case 178: /* KEY_ISO*/
				case 183: /* KEY_MOVE*/
				case 184: /* KEY_EDIT*/
				case 217: /* KEY_BASSBOOST*/
				case 219: /* KEY_HP*/
				case 221: /* KEY_SOUND*/
				case 222: /* KEY_QUESTION*/
				case 230: /* KEY_ALTERASE*/
				case 248: /* KEY_UNKNOWN*/
				case 249: /* KEY_VIDEO_NEXT*/
				case 250: /* KEY_VIDEO_PREV*/
				case 251: /* KEY_BRIGHTNESS_CYCLE*/
				case 252: /* KEY_BRIGHTNESS_ZERO*/
				case 253: /* KEY_DISPLAY_OFF*/
					virtual_key = OP_KEY_INVALID; break;

				default:
					// OP_KEY_1 to OP_KEY_9
					if (event->xkey.keycode >= 11 && event->xkey.keycode <= 18)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_1 + (event->xkey.keycode - 11));
					// OP_KEY_0 is not in the same order as the key codes
					else if (event->xkey.keycode == 19)
						virtual_key = OP_KEY_0;
					// OP_KEY_F1 to OP_KEY_F10
					else if (event->xkey.keycode >= 67 && event->xkey.keycode <= 76)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_F1 + (event->xkey.keycode - 67));
					// OP_KEY_F11 to OP_KEY_F12
					else if (event->xkey.keycode >= 95 && event->xkey.keycode <= 96)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_F11 + (event->xkey.keycode - 95));
					// OP_KEY_F13 to OP_KEY_F24
					else if (event->xkey.keycode >= 191 && event->xkey.keycode <= 202)
						virtual_key = static_cast<OpVirtualKey>(OP_KEY_F13 + (event->xkey.keycode - 191));
					else
						virtual_key = OP_KEY_INVALID;
			}
		}

		if (text.HasContent())
		{
			if (virtual_key == OP_KEY_INVALID)
				virtual_key = OP_KEY_UNICODE;
			RETURN_VALUE_IF_ERROR(content.SetText(text), false);
		}
		if (virtual_key == OP_KEY_INVALID)
		{
			if (allow_keycode_mapping)
				return false;
			else
				return true;
		}

		content.SetVirtualKey(virtual_key, location);
		return true;
	}

	KeySym VirtualKeyToKeySym(OpVirtualKey virtual_key, OpKey::Location location)
	{
		// The virtual key -> keysym to mapping here should be a complete reverse of the
		// keysym -> virtual key conversion in TranslateKeyEvent for the locations we decide
		// to support in watir (we only support OpKey::LOCATION_STANDARD now)

		// Left to implement whenever needed
		OP_ASSERT(location == OpKey::LOCATION_STANDARD);

		switch (virtual_key)
		{
			case OP_KEY_BACKSPACE: return XK_BackSpace;
			case OP_KEY_TAB: return XK_Tab;
			case OP_KEY_LEFT_TAB: return XK_ISO_Left_Tab;
			case OP_KEY_ENTER: return XK_Return;
			case OP_KEY_SPACE: return XK_space;
			case OP_KEY_ESCAPE: return XK_Escape;
			case OP_KEY_DELETE: return XK_Delete;
			case OP_KEY_HOME: return XK_Home;
			case OP_KEY_LEFT: return XK_Left;
			case OP_KEY_UP: return XK_Up;
			case OP_KEY_RIGHT: return XK_Right;
			case OP_KEY_DOWN: return XK_Down;
			case OP_KEY_PAGEUP: return XK_Page_Up;
			case OP_KEY_PAGEDOWN: return XK_Page_Down;
			case OP_KEY_END: return XK_End;
			case OP_KEY_INSERT: return XK_Insert;
			case OP_KEY_CONTEXT_MENU: return XK_Menu;
			case OP_KEY_XF86XK_STOP: return XK_Cancel;
			case OP_KEY_CANCEL: return XK_Cancel;
			case OP_KEY_PAUSE: return XK_Pause;
			case OP_KEY_CAPS_LOCK: return XK_Caps_Lock;
			case OP_KEY_MODECHANGE: return XK_Mode_switch;
			case OP_KEY_SELECT: return XK_Select;
			case OP_KEY_EXECUTE: return XK_Execute;
			case OP_KEY_PRINTSCREEN: return XK_Print;
			case OP_KEY_HELP: return XK_Help;
			case OP_KEY_SCROLL_LOCK: return XK_Scroll_Lock;
			case OP_KEY_SHIFT: return XK_Shift_L;
			case OP_KEY_CTRL: return XK_Control_L;
			case OP_KEY_ALT: return XK_Alt_L;
			case OP_KEY_F1: return XK_F1;
			case OP_KEY_F2: return XK_F2;
			case OP_KEY_F3: return XK_F3;
			case OP_KEY_F4: return XK_F4;
			case OP_KEY_F5: return XK_F5;
			case OP_KEY_F6: return XK_F6;
			case OP_KEY_F7: return XK_F7;
			case OP_KEY_F8: return XK_F8;
			case OP_KEY_F9: return XK_F9;
			case OP_KEY_F10: return XK_F10;
			case OP_KEY_F11: return XK_F11;
			case OP_KEY_F12: return XK_F12;
			case OP_KEY_F13: return XK_F13;
			case OP_KEY_F14: return XK_F14;
			case OP_KEY_F15: return XK_F15;
			case OP_KEY_F16: return XK_F16;
			case OP_KEY_F17: return XK_F17;
			case OP_KEY_F18: return XK_F18;
			case OP_KEY_F19: return XK_F19;
			case OP_KEY_F20: return XK_F20;
			case OP_KEY_F21: return XK_F21;
			case OP_KEY_F22: return XK_F22;
			case OP_KEY_F23: return XK_F23;
			case OP_KEY_F24: return XK_F24;
			case OP_KEY_F25: return XK_F25;
			case OP_KEY_F26: return XK_F26;
			case OP_KEY_F27: return XK_F27;
			case OP_KEY_F28: return XK_F28;
			case OP_KEY_F29: return XK_F29;
			case OP_KEY_F30: return XK_F30;
			case OP_KEY_F31: return XK_F31;
			case OP_KEY_F32: return XK_F32;
			case OP_KEY_F33: return XK_F33;
			case OP_KEY_F34: return XK_F34;
			case OP_KEY_F35: return XK_F35;
			case OP_KEY_0: return XK_0;
			case OP_KEY_1: return XK_1;
			case OP_KEY_2: return XK_2;
			case OP_KEY_3: return XK_3;
			case OP_KEY_4: return XK_4;
			case OP_KEY_5: return XK_5;
			case OP_KEY_6: return XK_6;
			case OP_KEY_7: return XK_7;
			case OP_KEY_8: return XK_8;
			case OP_KEY_9: return XK_9;
			default:
				if (virtual_key >= OP_KEY_0 && virtual_key <= OP_KEY_9)
					return XK_0 + (virtual_key-OP_KEY_0);
				else if (virtual_key >= OP_KEY_A && virtual_key <= OP_KEY_Z)
					return XK_A + (virtual_key-OP_KEY_A);
				else if (virtual_key >= 0x20 && virtual_key <= 0x7e || virtual_key >= 0xa0 && virtual_key < 0xff)
				{
					// For ASCII / Latin1
					return virtual_key;
				}
		}
		return NoSymbol;
	}




	MouseButton X11ToOpButton(int xbutton)
	{
		if (xbutton == 1)
			return MOUSE_BUTTON_1;
		else if (xbutton == 2)
			return MOUSE_BUTTON_3;
		else if (xbutton == 3)
			return MOUSE_BUTTON_2;

		OP_ASSERT(FALSE);
		return MOUSE_BUTTON_1;
	}

	OpVirtualKey TranslateModifierState(unsigned int state)
	{
		state &= (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
		switch(state)
		{
		case ShiftMask:
			return OP_KEY_SHIFT;
		case ControlMask:
			return OP_KEY_CTRL;
		case Mod1Mask:
			return OP_KEY_ALT;
		default:
			return OP_KEY_INVALID;
		}
	}

	void DumpOperaKey( BOOL keypress, int operakey, int modifiers )
	{
		char buf[100];
		buf[0]=0;
		if (modifiers&SHIFTKEY_CTRL)
			op_strcat(buf, "ctrl ");
		if (modifiers&SHIFTKEY_SHIFT)
			op_strcat(buf, "shift ");
		if (modifiers&SHIFTKEY_ALT)
			op_strcat(buf, "alt ");
		if (buf[0]==0)
			op_strcat(buf, "none");
		printf("%s (local) ", keypress ? "Key press  " : "Key release");
		printf("key: %c  modifiers: %s\n", operakey, buf);
	}

	bool IsOMenuToggleKey(XEvent* e)
	{
		if (e->type == KeyPress || e->type == KeyRelease)
		{
			KeyContent content;
			if (!TranslateKeyEvent(e,content))
				return false;

			// State keys we want to test for during menu activation with Alt button. Mod4Mask is the windows key
			unsigned int state = e->xkey.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

			if (e->type == KeyPress)
				return state == 0 && content.GetVirtualKey() == OMenuHotKey;
			else
				return TranslateModifierState(state) == OMenuHotKey && content.GetVirtualKey() == OMenuHotKey;
		}

		return false;
	}

	bool HandleEvent(XEvent *e)
	{
		switch (e->type)
		{
			case MotionNotify:
			{
				g_input_manager->SetMousePosition(OpPoint(e->xmotion.x_root,e->xmotion.y_root));
				return false;
			}
			break;

			case ButtonPress:
			case ButtonRelease:
			{
				can_show_O_menu = FALSE;

				int button = e->xbutton.button;
				if (button >= 1 && button <= 9)
				{
					OpVirtualKey buttonkey;
					if (button == 2)
						buttonkey = OP_KEY_MOUSE_BUTTON_3;
					else if (button == 3)
						buttonkey = OP_KEY_MOUSE_BUTTON_2;
					else
						buttonkey = static_cast<OpVirtualKey>(OP_KEY_MOUSE_BUTTON_1 + button - 1);

					int modifiers = GetOpModifierFlags();

					if ((button == Button4 || button == Button5) && (e->xbutton.state & Button3Mask) )
					{
						if (e->type == ButtonPress)
						{
							// We have to block mouse release if we navigate without the list (DSK-300205)
							mouse_navigation_active = g_pcui->GetIntegerPref(PrefsCollectionUI::WindowCycleType) == WINDOW_CYCLE_TYPE_MDI;

							if (button == Button4)
								g_input_manager->InvokeAction(OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE);
							else
								g_input_manager->InvokeAction(OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE);
						}
						return TRUE;
					}
					if (e->type == ButtonRelease && button == Button3)
					{
						if (mouse_navigation_active)
						{
							mouse_navigation_active = FALSE;
							g_input_manager->ResetInput();
							return TRUE;
						}
						else if (g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_CYCLER, 1)) // DSK-322175
						{
							g_input_manager->ResetInput();
							return TRUE;
						}
					}

					if (e->type == ButtonPress)
					{
						return g_input_manager->InvokeKeyDown(buttonkey,modifiers);
					}
					else
					{
						return g_input_manager->InvokeKeyUp(buttonkey,modifiers);
					}
				}
				return false;
			}
			break;

			case KeyPress:
			case KeyRelease:
			{
				int modifiers = GetOpModifierFlags();

				KeyContent content;
				if (!TranslateKeyEvent(e,content))
				{
					can_show_O_menu = FALSE;
					return true;
				}

				if (e->type == KeyPress)
				{
					if (content.GetVirtualKey() == OP_KEY_ALT || content.GetVirtualKey() == OP_KEY_CTRL)
					{
						// Close tab group when a pager activates (DSK-318871)
						// Slight side effect, closes normal tooltips as well
						g_application->SetToolTipListener(NULL, OP_KEY_ESCAPE);
					}
				}

				if (X11Widget::GetGrabbedWidget() && content.GetVirtualKey() == OP_KEY_ALT)
				{
					// Close dropdowns on Alt-down. Allows direct Alt+TAB navigation in WM from a state where
					// we show a grabbed dropdown in opera. Popup menus are closed in PopupMenu::OnInputEvent()
					g_input_manager->InvokeKeyDown(OP_KEY_ESCAPE, 0);
					g_input_manager->InvokeKeyPressed(OP_KEY_ESCAPE, 0);
					g_input_manager->InvokeKeyUp(OP_KEY_ESCAPE, 0);
					return true;
				}

				// O-menu activation. The O-menu is turned off in PopupMenu::OnInputEvent()
				if (e->type == KeyPress)
				{
					can_show_O_menu = IsOMenuToggleKey(e) && !g_drag_manager->IsDragging();
					if (can_show_O_menu)
					{
						can_show_O_menu = !X11Widget::GetGrabbedWidget();
						if (can_show_O_menu && X11WindowWidget::GetActiveWindowWidget())
						{
							can_show_O_menu = !X11WindowWidget::GetActiveWindowWidget()->IsDialog();
						}
					}
				}
				else if (can_show_O_menu)
				{
					can_show_O_menu = FALSE;
					if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu) && IsOMenuToggleKey(e))
					{
						OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_MAIN_MENU));
						if (action)
						{
							// Use the presence of a shortcut as a flag that we do not want hardcoded key activation. DSK-316375
							OpString shortcut;
							g_input_manager->GetShortcutStringFromAction(action, shortcut, 0);
							if (shortcut.HasContent())
								OP_DELETE(action);
							else
								g_input_manager->InvokeAction(action);
						}
					}
					return true; // Eat event regardless of menubar visibility for consistent behavior
				}

				if (g_startup_settings.debug_keyboard)
					DumpOperaKey(e->type == KeyPress, content.GetVirtualKey(), modifiers);

				// Interface with input manager
				OpPlatformKeyEventData* event_data = NULL;
				OpStatus::Ignore(OpPlatformKeyEventData::Create(&event_data, &e->xkey));
				if (e->type == KeyPress)
				{
					// We only send a value and value length if the key event produce a character.
					// We assume that pressing a key with ALT down will produce a character if the
					// same event without the Mod1 mask set (no ALT) produce something else. Otherwise
					// do not set the value and value length. Edit fields will consume Alt+P when value
					// is passed, but that is also a global shortcut (DSK-372584).

					OpStringC8 value;
					// We should not send along a key value if only control is pressed, but the shortcut
					// parser core uses accepts shortcuts like ". ctrl" without modifying the character into
					// a virtual key ("period" or "oemperiod" for '.')
#if 0
					if ((GetOpModifierFlags() & SHIFTKEY_CTRL) == SHIFTKEY_CTRL)
						; // Do not pass value if ctrl is pressed
					else
#endif
					if ((GetOpModifierFlags() & SHIFTKEY_ALT) == 0)
						value = content.GetUtf8Text();
					else if (content.GetUtf8Text().HasContent())
					{
						unsigned int state = e->xkey.state;
						e->xkey.state &= ~Mod1Mask;

						KeyContent content_no_alt;
						if (TranslateKeyEvent(e,content_no_alt))
						{
							if (content_no_alt.GetUtf8Text().Compare(content.GetUtf8Text()) != 0)
								value = content.GetUtf8Text();
						}
						e->xkey.state = state;
					}

					g_input_manager->InvokeKeyDown(content.GetVirtualKey(), value.CStr(), value.Length(), event_data, GetOpModifierFlags(), FALSE, content.GetLocation(), TRUE);
					g_input_manager->InvokeKeyPressed(content.GetVirtualKey(), value.CStr(), value.Length(), GetOpModifierFlags(), FALSE, TRUE);
				}
				else
				{
					BOOL ok = g_input_manager->InvokeKeyUp(content.GetVirtualKey(), content.GetUtf8Text().CStr(), content.GetUtf8Text().Length(), event_data, GetOpModifierFlags(), FALSE, content.GetLocation(), TRUE);
					if (!ok && content.GetVirtualKey() == OP_KEY_CTRL)
					{
						// Workaround for bug DSK-296599. Happens because external programs may
						// eat first single CTRL down. CTRL up arrives fine, but
						// OpInputManager::InvokeKeyUp() will fail because it can not remove
						// the key value from its table of down-keys since it is not there
						g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_CYCLER, 1);
					}
				}
				if (event_data)
					OpPlatformKeyEventData::DecRef(event_data);
			}
			break;
		}

		return false;
	}

	
	void GetKey(XEvent* event, KeySym& keysym, OpString& text)
	{
		keysym = NoSymbol;
		text.Set(""); // Just empty string so not release allocated memory

		if (event->type == KeyPress)
		{
			last_keypress.Set(None, text);

			// TODO. We really want the nearest toplevel of the window the
			// event is delivered to, but it really makes no sense as long
			// as the input manager is not tied to to a window.
			X11Widget* widget = X11WindowWidget::GetActiveWindowWidget();
			if (widget)
				widget = widget->GetNearestTopLevel();

			if (widget && widget->GetKeyPress(event, keysym, text))
			{
				last_keypress.Set(keysym, text);
				return;
			}

			// Fallback to allow navigation. Text input in non-latin1 environments will generally fail
			if (keybuf.Reserve(100))
			{
				int length = XLookupString(&event->xkey, keybuf.CStr(), keybuf.Capacity(), &keysym, 0);
				text.Set(keybuf.CStr(), length);
				last_keypress.Set(keysym, text);
			}
		}
		else if (event->type == KeyRelease)
		{
			// Get the keysym on release. We need it to determine if a modifier key
			// has been released. It would be good if we could convert the keysym to
			// a unicode value as well. The returned string from XLookupString can not
			// be used for that since it is latin1 biased (XmbLookupString, XwcLookupString
			// and Xutf8LookupString do not work on a key release).

			if (keybuf.Reserve(100))
			{
				XLookupString(&event->xkey, keybuf.CStr(), keybuf.Capacity(), &keysym, 0);

				// Use saved state to improve behavior for non latin1 environments
				if (keysym == last_keypress.keysym)
					text.Set(last_keypress.text);
			}
		}
	}


	void HandleFocusChange(XEvent* event)
	{
		// Bug #DSK-288162 If we press the left mouse button while holding ALT down
		// most window managers will eat it without notifying Opera. We will receive 
		// a focus out though.
	    can_show_O_menu = FALSE;
	}


	void SetX11ModifierMask(XEvent* event)
	{
		switch(event->type)
		{
			case MotionNotify:
				modifier_mask = event->xmotion.state;
				break;
			case ButtonPress:			
			case ButtonRelease:
				modifier_mask = event->xbutton.state;
				break;
			case KeyPress:
			{
				modifier_mask = event->xkey.state;
				KeySym ks = XLookupKeysym(&event->xkey, 0);
				if (ks == XK_Shift_L || ks == XK_Shift_R)
					modifier_mask |= ShiftMask;
				else if (ks == XK_Control_L || ks == XK_Control_R)
					modifier_mask |= ControlMask;
				else if (ks == XK_Alt_L || ks == XK_Alt_R )
					modifier_mask |= Mod1Mask;
				break;
			}
			case KeyRelease:
			{
				modifier_mask = event->xkey.state;
				KeySym ks = XLookupKeysym(&event->xkey, 0);
				if (ks == XK_Shift_L || ks == XK_Shift_R)
					modifier_mask &= ~ShiftMask;
				else if (ks == XK_Control_L || ks == XK_Control_R)
					modifier_mask &= ~ControlMask;
				else if (ks == XK_Alt_L || ks == XK_Alt_R )
					modifier_mask &= ~Mod1Mask;
				break;
			}
		}
	}

	unsigned int GetX11ModifierMask()
	{
		return modifier_mask;
	}

	ShiftKeyState GetOpModifierFlags()
	{
		ShiftKeyState shift_state = 0;
		if (modifier_mask & ShiftMask)
			shift_state |= SHIFTKEY_SHIFT;
		if (modifier_mask & ControlMask)
			shift_state |= SHIFTKEY_CTRL;
		if (modifier_mask & Mod1Mask)
			shift_state |= SHIFTKEY_ALT;
		return shift_state;
	}


	void SetOpButtonFlags(XEvent* event)
	{
		switch(event->type)
		{
			case MotionNotify:
				// The button flags can be incorrectly set if we cancel
				// a modal window by clicking outside the opera window.
				// We will only catch the mouse down event
				if( button_flags )
				{
					unsigned int mask = GetX11ModifierMask();
					if( !(mask&Button1Mask) )
						button_flags &= ~(1 << X11ToOpButton(1));
					if( !(mask&Button2Mask) )
						button_flags &= ~(1 << X11ToOpButton(2));
					if( !(mask&Button3Mask) )
						button_flags &= ~(1 << X11ToOpButton(3));
				}
				break;

			case ButtonPress:
			{
				unsigned int button = event->xbutton.button;
				if (button >= 1 && button <= 3)
					button_flags |= 1 << X11ToOpButton(button);
				break;
			}

			case ButtonRelease:
			{
				unsigned int button = event->xbutton.button;
				if (button >= 1 && button <= 3)
					button_flags &= ~(1 << X11ToOpButton(button));
				break;
			}
		}
	}

	int GetOpButtonFlags()
	{
		return button_flags;
	}

	void UpdateClickCount(XEvent* event)
	{
		if (event->type == ButtonPress)
		{
			int button = event->xbutton.button;
			double now = g_op_time_info->GetRuntimeMS();
			bool cancel = clickwindow != event->xbutton.window || clickbutton != button || 
				now - lastclicktime > multiclick_treshold;
			
			if (!cancel)
			{
				int diffx = lastclickpos.x - event->xbutton.x;
				int diffy = lastclickpos.y - event->xbutton.y;
				cancel = diffx*diffx + diffy*diffy > 25;
			}

			if (cancel)
			{
				clickbutton = button;
				clickcount = 0;
				clickwindow = event->xbutton.window;
			}
			clickcount ++;
			lastclicktime = now;
			lastclickpos.x = event->xbutton.x;
			lastclickpos.y = event->xbutton.y;
		}
	}

	bool IsOpButtonPressed(MouseButton button, bool exact)
	{
		if (exact)
			return GetOpButtonFlags() == (1 << button);
		else
			return !!(GetOpButtonFlags() & (1 << button));
	}

	int GetClickCount()
	{
		return clickcount;
	}


	int GetScreenUnderPointer()
	{
		X11Types::Display* display = g_x11->GetDisplay();
		int num_screens = ScreenCount(display);

		X11Types::Window win;
		unsigned int mask;
		int pos;

		for (int i = 0; i < num_screens; i++)
		{
			if (XQueryPointer(display, RootWindow(display, i), &win, &win, &pos, &pos, &pos, &pos, &mask) == True)
			{
				return i;
			}
		}

		return -1;
	}

	BOOL GetGlobalPointerPos(OpPoint& position)
	{
		X11Types::Display* display = g_x11->GetDisplay();
		int num_screens = ScreenCount(display);

		X11Types::Window win;
		unsigned int mask;
		int pos, x, y;

		for (int i = 0; i < num_screens; i++)
		{
			if (XQueryPointer(display, RootWindow(display, i), &win, &win, &x, &y, &pos, &pos, &mask) == True)
			{
				position.x = x;
				position.y = y;
				return TRUE;
			}		
		}
		return FALSE;
	}

	void DumpKeyEvent(XEvent* e)
	{
		XKeyEvent* ke = &e->xkey;

		OpInputContext* context = g_input_manager->GetKeyboardInputContext();
		const char* context_name = context ? context->GetInputContextName() : "None";

		char buffer[101];
		buffer[0]=0;

		KeySym keysym;
		XLookupString(ke, buffer, 100, &keysym, 0);

		if (e->type == KeyPress)
		{
			printf("Key press   (x11)   window=%08x, state=%08x, keycode=%08x, keysym=%d,'%s', context: %s\n",
			   (unsigned int)ke->window, ke->state, ke->keycode, (unsigned int)keysym, buffer, context_name);
		}
		else
		{
			printf("Key release (x11)   window=%08x, state=%08x, keycode=%08x, keysym=%d,'%s', context: %s\n",
			   (unsigned int)ke->window, ke->state, ke->keycode, (unsigned int)keysym, buffer, context_name);
		}
	}

	void DumpButtonEvent(XEvent* e)
	{
		XButtonEvent* be = &e->xbutton;

		OpInputContext* context = g_input_manager->GetKeyboardInputContext();
		const char* context_name = context ? context->GetInputContextName() : "None";

		if (e->type == ButtonPress)
		{
			printf("Button press : state=%08x, button=%d, context: %s\n",
				   be->state, be->button, context_name );
		}
		else
		{
			printf("Button release : state=%08x, button=%d, context: %s\n",
				   be->state, be->button, context_name );
		}
	}

}
