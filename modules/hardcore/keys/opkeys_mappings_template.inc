// <copyright header>

#include "modules/hardcore/keys/opkeys.h"

/* static */ const uni_char*
OpKey::ToString(OpKey::Code key)
{
	switch (key)
	{
// <key to string>
	}
	return NULL;
}

/* static */ OpKey::Code
OpKey::FromString(const uni_char* str)
{
// <string to key>

	return OP_KEY_FIRST;
}

/* static */ BOOL
OpKey::IsInValueSet(OpKey::Code key)
{
	switch (key)
	{
// <has no key value>
		return FALSE;
	default:
		return TRUE;
	}
}

/* static */ uni_char
OpKey::CharValue(OpKey::Code key)
{
	switch (key)
	{
// <with char value>
	}
	return 0;
}

/* static */ BOOL
OpKey::IsModifier(OpKey::Code key)
{
	switch (key)
	{
// <is modifier cases>
		return TRUE;

	default:
		return FALSE;
	}
}

/* static */ BOOL
OpKey::IsFunctionKey(OpKey::Code key)
{
	switch (key)
	{
// <is function key>
	    return TRUE;
	}
	return FALSE;
}

/* static */ OpKey::Location
OpKey::ToLocation(OpKey::Code key)
{
	switch (key)
	{
// <location cases>

	}
	return LOCATION_STANDARD;
}

#ifdef MOUSE_SUPPORT

/* static */ BOOL
OpKey::IsMouseButton(OpKey::Code key)
{
	switch (key)
	{
// <is mouse button cases>
		return TRUE;

	default:
		return FALSE;
	}
}

#endif // MOUSE_SUPPORT

/* static */ BOOL
OpKey::IsGesture(OpKey::Code key)
{
	switch (key)
	{
// <is gesture cases>
		return TRUE;

	default:
		return FALSE;
	}
}

/* static */ BOOL
OpKey::IsFlip(OpKey::Code key)
{
	switch (key)
	{
// <is flip cases>
		return TRUE;

	default:
		return FALSE;
	}
}

/* static */ OpKey::Code
OpKey::FromASCIIToKeyCode(char ch, ShiftKeyState &modifiers)
{
    modifiers = SHIFTKEY_NONE;
    if (ch >= 'a' && ch <= 'z')
        return static_cast<OpKey::Code>(OP_KEY_A + (ch - 'a'));
    else if (ch >= '0' && ch <= '9')
        return static_cast<OpKey::Code>(OP_KEY_0 + (ch - '0'));
    else if (ch >= 'A' && ch <= 'Z')
    {
        modifiers |= SHIFTKEY_SHIFT;
        return static_cast<OpKey::Code>(OP_KEY_A + (ch - 'A'));
    }
    else
        /* US-ASCII means US keyboard layout. */
        switch (ch)
        {
#if OP_KEY_BACKSPACE_ENABLED
        case 8: return OP_KEY_BACKSPACE;
#endif // OP_KEY_BACKSPACE_ENABLED
#if OP_KEY_TAB_ENABLED
        case '\t': return OP_KEY_TAB;
#endif // OP_KEY_TAB_ENABLED
#if OP_KEY_ENTER_ENABLED
        case '\r':
        case '\n': return OP_KEY_ENTER;
#endif // OP_KEY_ENTER_ENABLED
#if OP_KEY_ESCAPE_ENABLED
        case 27: return OP_KEY_ESCAPE;
#endif // OP_KEY_ESCAPE_ENABLED
#if OP_KEY_SPACE_ENABLED
        case ' ': return OP_KEY_SPACE;
#endif // OP_KEY_SPACE_ENABLED
        case '!':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_1;
#ifdef OP_KEY_OEM_7_ENABLED
        case '"':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_7;
#endif // OP_KEY_OEM_7_ENABLED
        case '#':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_3;
        case '$':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_4;
        case '%':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_5;
        case '&':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_7;
#if OP_KEY_OEM_7_ENABLED
        case '\'':
            return OP_KEY_OEM_7;
#endif // OP_KEY_OEM_7_ENABLED
        case '(':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_9;
        case ')':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_0;
        case '*':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_8;
#if OP_KEY_OEM_PLUS_ENABLED
        case '+':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_PLUS;
#endif // OP_KEY_OEM_PLUS_ENABLED
#if OP_KEY_OEM_COMMA_ENABLED
        case ',':
            return OP_KEY_OEM_COMMA;
#endif // OP_KEY_OEM_COMMA_ENABLED
#if OP_KEY_OEM_MINUS_ENABLED
        case '-':
            return OP_KEY_OEM_MINUS;
#endif // OP_KEY_OEM_MINUS_ENABLED
#if OP_KEY_OEM_PERIOD_ENABLED
        case '.':
            return OP_KEY_OEM_PERIOD;
#endif // OP_KEY_OEM_PERIOD_ENABLED
#if OP_KEY_OEM_2_ENABLED
        case '/':
            return OP_KEY_OEM_2;
#endif // OP_KEY_OEM_2_ENABLED
#if OP_KEY_OEM_1_ENABLED
        case ':':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_1;
#endif // OP_KEY_OEM_1_ENABLED
#if OP_KEY_OEM_1_ENABLED
        case ';':
            return OP_KEY_OEM_1;
#endif // OP_KEY_OEM_1_ENABLED
#if OP_KEY_OEM_COMMA_ENABLED
        case '<':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_COMMA;
#endif // OP_KEY_OEM_COMMA_ENABLED
#if OP_KEY_OEM_PLUS_ENABLED
        case '=':
            return OP_KEY_OEM_PLUS;
#endif // OP_KEY_OEM_PLUS_ENABLED
#if OP_KEY_OEM_PERIOD_ENABLED
        case '>':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_PERIOD;
#endif // OP_KEY_OEM_PERIOD_ENABLED
#if OP_KEY_OEM_2_ENABLED
        case '?':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_2;
#endif // OP_KEY_OEM_2_ENABLED
        case '@':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_2;
#if OP_KEY_OEM_4_ENABLED
        case '[':
            return OP_KEY_OEM_4;
#endif // OP_KEY_OEM_4_ENABLED
#if OP_KEY_OEM_5_ENABLED
        case '\\':
            return OP_KEY_OEM_5;
#endif // OP_KEY_OEM_5_ENABLED
#if OP_KEY_OEM_6_ENABLED
        case ']':
            return OP_KEY_OEM_6;
#endif // OP_KEY_OEM_6_ENABLED
        case '^':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_6;
#if OP_KEY_OEM_MINUS_ENABLED
        case '_':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_MINUS;
#endif // OP_KEY_OEM_MINUS_ENABLED
#if OP_KEY_OEM_3_ENABLED
        case '`':
            return OP_KEY_OEM_3;
#endif // OP_KEY_OEM_3_ENABLED
#if OP_KEY_OEM_4_ENABLED
        case '{':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_4;
#endif // OP_KEY_OEM_4_ENABLED
#if OP_KEY_OEM_5_ENABLED
        case '|':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_5;
#endif // OP_KEY_OEM_5_ENABLED
#if OP_KEY_OEM_6_ENABLED
        case '}':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_6;
#endif // OP_KEY_OEM_6_ENABLED
#if OP_KEY_OEM_3_ENABLED
        case '~':
            modifiers |= SHIFTKEY_SHIFT;
            return OP_KEY_OEM_3;
#endif // OP_KEY_OEM_3_ENABLED
#if OP_KEY_DELETE_ENABLED
        case 0x7f:
            return OP_KEY_DELETE;
#endif // OP_KEY_DELETE_ENABLED
        }
    return OP_KEY_FIRST;
}
