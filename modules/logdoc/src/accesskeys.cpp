#ifdef ACCESS_KEYS_SUPPORT

#define AK2OK_NAME_LEN 79

CONST_ARRAY(g_AK2OK_names, char*)
	CONST_ENTRY("accept"),
	CONST_ENTRY("accesskey"),
	CONST_ENTRY("address"),
	CONST_ENTRY("alt"),
	CONST_ENTRY("brk"),
	CONST_ENTRY("bs"),
	CONST_ENTRY("calendar"),
	CONST_ENTRY("caps"),
	CONST_ENTRY("clr"),
	CONST_ENTRY("cmd"),
	CONST_ENTRY("copy"),
	CONST_ENTRY("ctrl"),
	CONST_ENTRY("cut"),
	CONST_ENTRY("del"),
	CONST_ENTRY("delete"),
	CONST_ENTRY("down"),
	CONST_ENTRY("end"),
	CONST_ENTRY("enter"),
	CONST_ENTRY("esc"),
	CONST_ENTRY("f1"),
	CONST_ENTRY("f10"),
	CONST_ENTRY("f11"),
	CONST_ENTRY("f12"),
	CONST_ENTRY("f13"),
	CONST_ENTRY("f14"),
	CONST_ENTRY("f15"),
	CONST_ENTRY("f2"),
	CONST_ENTRY("f3"),
	CONST_ENTRY("f4"),
	CONST_ENTRY("f5"),
	CONST_ENTRY("f6"),
	CONST_ENTRY("f7"),
	CONST_ENTRY("f8"),
	CONST_ENTRY("f9"),
	CONST_ENTRY("fcn"),
	CONST_ENTRY("fn"),
	CONST_ENTRY("help"),
	CONST_ENTRY("home"),
	CONST_ENTRY("ins"),
	CONST_ENTRY("left"),
	CONST_ENTRY("lopt"),
	CONST_ENTRY("lwin"),
	CONST_ENTRY("mail"),
	CONST_ENTRY("memo"),
	CONST_ENTRY("menu"),
	CONST_ENTRY("meta"),
	CONST_ENTRY("namemenu"),
	CONST_ENTRY("numlock"),
	CONST_ENTRY("opt"),
	CONST_ENTRY("options"),
	CONST_ENTRY("paste"),
	CONST_ENTRY("pause"),
	CONST_ENTRY("pgdn"),
	CONST_ENTRY("pgup"),
	CONST_ENTRY("phone-accept"),
	CONST_ENTRY("phone-end"),
	CONST_ENTRY("phone-send"),
	CONST_ENTRY("prev"),
	CONST_ENTRY("prtsc"),
	CONST_ENTRY("pwr"),
	CONST_ENTRY("rcl"),
	CONST_ENTRY("rwin"),
	CONST_ENTRY("reset"),
	CONST_ENTRY("return"),
	CONST_ENTRY("right"),
	CONST_ENTRY("ropt"),
	CONST_ENTRY("scrlock"),
	CONST_ENTRY("shift"),
	CONST_ENTRY("snd"),
	CONST_ENTRY("space"),
	CONST_ENTRY("sto"),
	CONST_ENTRY("sysrq"),
	CONST_ENTRY("tab"),
	CONST_ENTRY("todo"),
	CONST_ENTRY("undo"),
	CONST_ENTRY("up"),
	CONST_ENTRY("volumedown"),
	CONST_ENTRY("volumeup"),
	CONST_ENTRY("win")
CONST_END(g_AK2OK_names)

#ifdef PLATFORM_ACCESSKEYS_FILE
# include PLATFORM_ACCESSKEYS_FILE
#else // PLATFORM_ACCESSKEYS_FILE

const OpKey::Code g_AK2OK_token[] =
{
	OP_KEY_FIRST, // "accept"
	OP_KEY_FIRST, // "accesskey"
	OP_KEY_FIRST, // "address"
# ifdef OP_KEY_ALT_ENABLED
	OP_KEY_ALT, // "alt"
# else // OP_KEY_ALT_ENABLED
	OP_KEY_FIRST, // "alt"
# endif // OP_KEY_ALT_ENABLED
	OP_KEY_FIRST, // "brk"
# ifdef OP_KEY_BACKSPACE_ENABLED
	OP_KEY_BACKSPACE, // "bs"
# else // OP_KEY_BACKSPACE_ENABLED
	OP_KEY_FIRST, // "bs"
# endif // OP_KEY_BACKSPACE_ENABLED
	OP_KEY_FIRST, // "calendar"
# ifdef OP_KEY_CAPS_LOCK_ENABLED
	OP_KEY_CAPS_LOCK, // "caps"
# else // OP_KEY_CAPS_LOCK_ENABLED
	OP_KEY_FIRST, // "caps"
# endif // OP_KEY_CAPS_LOCK_ENABLED
	OP_KEY_FIRST, // "clr"
	OP_KEY_FIRST, // "cmd"
	OP_KEY_FIRST, // "copy"
# ifdef OP_KEY_CTRL_ENABLED
	OP_KEY_CTRL, // "ctrl"
# else // OP_KEY_CTRL_ENABLED
	OP_KEY_FIRST, // "ctrl"
# endif // OP_KEY_CTRL_ENABLED
	OP_KEY_FIRST, // "cut"
# ifdef OP_KEY_DEL_ENABLED
	OP_KEY_DEL, // "del"
# else // OP_KEY_DEL_ENABLED
	OP_KEY_FIRST, // "del"
# endif // OP_KEY_DEL_ENABLED
	OP_KEY_FIRST, // "delete"
# ifdef OP_KEY_DOWN_ENABLED
	OP_KEY_DOWN, // "down"
# else // OP_KEY_DOWN_ENABLED
	OP_KEY_FIRST, // "down"
# endif // OP_KEY_DOWN_ENABLED
# ifdef OP_KEY_END_ENABLED
	OP_KEY_END, // "end"
# else // OP_KEY_END_ENABLED
	OP_KEY_FIRST, // "end"
# endif // OP_KEY_END_ENABLED
# ifdef OP_KEY_ENTER_ENABLED
	OP_KEY_ENTER, // "enter"
# else // OP_KEY_ENTER_ENABLED
	OP_KEY_FIRST, // "enter"
# endif // OP_KEY_ENTER_ENABLED
# ifdef OP_KEY_ESCAPE_ENABLED
	OP_KEY_ESCAPE, // "esc"
# else // OP_KEY_ESCAPE_ENABLED
	OP_KEY_FIRST, // "esc"
# endif // OP_KEY_ESCAPE_ENABLED
# ifdef OP_KEY_F1_ENABLED
	OP_KEY_F1, // "f1"
# else // OP_KEY_F1_ENABLED
	OP_KEY_FIRST, // "f1"
# endif // OP_KEY_F1_ENABLED
# ifdef OP_KEY_F10_ENABLED
	OP_KEY_F10, // "f10"
# else // OP_KEY_F10_ENABLED
	OP_KEY_FIRST, // "f10"
# endif // OP_KEY_F10_ENABLED
# ifdef OP_KEY_F11_ENABLED
	OP_KEY_F11, // "f11"
# else // OP_KEY_F11_ENABLED
	OP_KEY_FIRST, // "f11"
# endif // OP_KEY_F11_ENABLED
# ifdef OP_KEY_F12_ENABLED
	OP_KEY_F12, // "f12"
# else // OP_KEY_F12_ENABLED
	OP_KEY_FIRST, // "f12"
# endif // OP_KEY_F12_ENABLED
# ifdef OP_KEY_F13_ENABLED
	OP_KEY_F13, // "f13"
# else // OP_KEY_F13_ENABLED
	OP_KEY_FIRST, // "f13"
# endif // OP_KEY_F13_ENABLED
# ifdef OP_KEY_F14_ENABLED
	OP_KEY_F14, // "f14"
# else // OP_KEY_F14_ENABLED
	OP_KEY_FIRST, // "f14"
# endif // OP_KEY_F14_ENABLED
# ifdef OP_KEY_F15_ENABLED
	OP_KEY_F15, // "f15"
# else // OP_KEY_F15_ENABLED
	OP_KEY_FIRST, // "f15"
# endif // OP_KEY_F15_ENABLED
# ifdef OP_KEY_F2_ENABLED
	OP_KEY_F2, // "f2"
# else // OP_KEY_F2_ENABLED
	OP_KEY_FIRST, // "f2"
# endif // OP_KEY_F2_ENABLED
# ifdef OP_KEY_F3_ENABLED
	OP_KEY_F3, // "f3"
# else // OP_KEY_F3_ENABLED
	OP_KEY_FIRST, // "f3"
# endif // OP_KEY_F3_ENABLED
# ifdef OP_KEY_F4_ENABLED
	OP_KEY_F4, // "f4"
# else // OP_KEY_F4_ENABLED
	OP_KEY_FIRST, // "f4"
# endif // OP_KEY_F4_ENABLED
# ifdef OP_KEY_F5_ENABLED
	OP_KEY_F5, // "f5"
# else // OP_KEY_F5_ENABLED
	OP_KEY_FIRST, // "f5"
# endif // OP_KEY_F5_ENABLED
# ifdef OP_KEY_F6_ENABLED
	OP_KEY_F6, // "f6"
# else // OP_KEY_F6_ENABLED
	OP_KEY_FIRST, // "f6"
# endif // OP_KEY_F6_ENABLED
# ifdef OP_KEY_F7_ENABLED
	OP_KEY_F7, // "f7"
# else // OP_KEY_F7_ENABLED
	OP_KEY_FIRST, // "f7"
# endif // OP_KEY_F7_ENABLED
# ifdef OP_KEY_F8_ENABLED
	OP_KEY_F8, // "f8"
# else // OP_KEY_F8_ENABLED
	OP_KEY_FIRST, // "f8"
# endif // OP_KEY_F8_ENABLED
# ifdef OP_KEY_F9_ENABLED
	OP_KEY_F9, // "f9"
# else // OP_KEY_F9_ENABLED
	OP_KEY_FIRST, // "f9"
# endif // OP_KEY_F9_ENABLED
	OP_KEY_FIRST, // "fcn"
	OP_KEY_FIRST, // "fn"
	OP_KEY_FIRST, // "help"
# ifdef OP_KEY_HOME_ENABLED
	OP_KEY_HOME, // "home"
# else // OP_KEY_HOME_ENABLED
	OP_KEY_FIRST, // "home"
# endif // OP_KEY_HOME_ENABLED
# ifdef OP_KEY_INS_ENABLED
	OP_KEY_INS, // "ins"
# else // OP_KEY_INS_ENABLED
	OP_KEY_FIRST, // "ins"
# endif // OP_KEY_INS_ENABLED
# ifdef OP_KEY_LEFT_ENABLED
	OP_KEY_LEFT, // "left"
# else // OP_KEY_LEFT_ENABLED
	OP_KEY_FIRST, // "left"
# endif // OP_KEY_LEFT_ENABLED
	OP_KEY_FIRST, // "lopt"
	OP_KEY_FIRST, // "lwin"
	OP_KEY_FIRST, // "mail"
	OP_KEY_FIRST, // "memo"
	OP_KEY_FIRST, // "menu"
	OP_KEY_META,  // "meta"
	OP_KEY_FIRST, // "namemenu"
# ifdef OP_KEY_NUMLOCK_ENABLED
	OP_KEY_NUMLOCK, // "numlock"
# else // OP_KEY_NUMLOCK_ENABLED
	OP_KEY_FIRST, // "numlock"
# endif // OP_KEY_NUMLOCK_ENABLED
	OP_KEY_FIRST, // "opt"
	OP_KEY_FIRST, // "options"
	OP_KEY_FIRST, // "paste"
# ifdef OP_KEY_PAUSE_ENABLED
	OP_KEY_PAUSE, // "pause"
# else // OP_KEY_PAUSE_ENABLED
	OP_KEY_FIRST, // "pause"
# endif // OP_KEY_PAUSE_ENABLED
# ifdef OP_KEY_PAGEDOWN_ENABLED
	OP_KEY_PAGEDOWN, // "pgdn"
# else // OP_KEY_PAGEDOWN_ENABLED
	OP_KEY_FIRST, // "pgdn"
# endif // OP_KEY_PAGEDOWN_ENABLED
# ifdef OP_KEY_PAGEUP_ENABLED
	OP_KEY_PAGEUP, // "pgup"
# else // OP_KEY_PAGEUP_ENABLED
	OP_KEY_FIRST, // "pgup"
# endif // OP_KEY_PAGEUP_ENABLED
	OP_KEY_FIRST, // "phone-accept"
	OP_KEY_FIRST, // "phone-end"
	OP_KEY_FIRST, // "phone-send"
	OP_KEY_FIRST, // "prev"
# ifdef OP_KEY_PRINTSCREEN_ENABLED
	OP_KEY_PRINTSCREEN, // "prtsc"
# else // OP_KEY_PRINTSCREEN_ENABLED
	OP_KEY_FIRST, // "prtsc"
# endif // OP_KEY_PRINTSCREEN_ENABLED
	OP_KEY_FIRST, // "pwr"
	OP_KEY_FIRST, // "rcl"
	OP_KEY_FIRST, // "rwin"
	OP_KEY_FIRST, // "reset"
# ifdef OP_KEY_RETURN_ENABLED
	OP_KEY_RETURN, // "return"
# else // OP_KEY_RETURN_ENABLED
	OP_KEY_FIRST, // "return"
# endif // OP_KEY_RETURN_ENABLED
# ifdef OP_KEY_RIGHT_ENABLED
	OP_KEY_RIGHT, // "right"
# else // OP_KEY_RIGHT_ENABLED
	OP_KEY_FIRST, // "right"
# endif // OP_KEY_RIGHT_ENABLED
	OP_KEY_FIRST, // "ropt"
# ifdef OP_KEY_SCROLL_LOCK_ENABLED
	OP_KEY_SCROLL_LOCK, // "scrlock"
# else // OP_KEY_SCROLL_LOCK_ENABLED
	OP_KEY_FIRST, // "scrlock"
# endif // OP_KEY_SCROLL_LOCK_ENABLED
# ifdef OP_KEY_SHIFT_ENABLED
	OP_KEY_SHIFT, // "shift"
# else // OP_KEY_SHIFT_ENABLED
	OP_KEY_FIRST, // "shift"
# endif // OP_KEY_SHIFT_ENABLED
	OP_KEY_FIRST, // "snd"
# ifdef OP_KEY_SPACE_ENABLED
	OP_KEY_SPACE, // "space"
# else // OP_KEY_SPACE_ENABLED
	OP_KEY_FIRST, // "space"
# endif // OP_KEY_SPACE_ENABLED
	OP_KEY_FIRST, // "sto"
	OP_KEY_FIRST, // "sysrq"
# ifdef OP_KEY_TAB_ENABLED
	OP_KEY_TAB, // "tab"
# else // OP_KEY_TAB_ENABLED
	OP_KEY_FIRST, // "tab"
# endif // OP_KEY_TAB_ENABLED
	OP_KEY_FIRST, // "todo"
	OP_KEY_FIRST, // "undo"
# ifdef OP_KEY_UP_ENABLED
	OP_KEY_UP, // "up"
# else // OP_KEY_UP_ENABLED
	OP_KEY_FIRST, // "up"
# endif // OP_KEY_UP_ENABLED
	OP_KEY_FIRST, // "volumedown"
	OP_KEY_FIRST, // "volumeup"
	OP_KEY_FIRST // "win"
};

#endif // PLATFORM_ACCESSKEYS_FILE

OpKey::Code
AccesskeyToOpKey(const uni_char *string)
{
	int lo = 0, hi = AK2OK_NAME_LEN - 1;
	const char *const *names = g_AK2OK_names;

	while (hi >= lo)
	{
		int index = (lo + hi) >> 1;

		const char *n1 = names[index];
		const uni_char *n2 = string;

		while (*n1 && *n2 && *n1 == *n2)
			++n1, ++n2;

		int r = *n1 - *n2;
		if (r == 0)
			return g_AK2OK_token[index];
		else if (r < 0)
			lo = index + 1;
		else
			hi = index - 1;
	}

	return OP_KEY_FIRST;
}

#endif // ACCESS_KEYS_SUPPORT
