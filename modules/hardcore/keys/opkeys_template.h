// <-*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*->
// <copyright header>

#ifndef OPKEY_AUTO_H
#define OPKEY_AUTO_H

#define YES 1
#define NO 0

#ifndef PRODUCT_OPKEY_FILE
#error "You must define PRODUCT_OPKEY_FILE to the opkey definition file for your platform/product. see hardcore/keys/README.txt for more information."
#endif // PRODUCT_OPKEY_FILE

#include PRODUCT_OPKEY_FILE

// <check defines>

#undef YES
#undef NO

typedef UINT8 ShiftKeyState;
enum
{
	SHIFTKEY_NONE = 0x00,
	SHIFTKEY_CTRL = 0x01,
	SHIFTKEY_SHIFT = 0x02,
	SHIFTKEY_ALT = 0x04,
	SHIFTKEY_META = 0x08,
	SHIFTKEY_SUPER = 0x10 ///< The Windows key under X11.
};

enum OpVirtualKey
{
	OP_KEY_FIRST = 0,
	OP_KEY_INVALID = OP_KEY_FIRST,
	/**< The OP_KEY_INVALID is used internally in Core to represent an
	     unknown key. No key input actions will contain its virtual key.
	     If a platform injects key input having its keycode, it will be
	     ignored. */

// <enum values>

	OP_KEY_FIRST_INTERNAL_EXTENDED = OP_KEY_FIRST + 0x3500,

#if defined _WML_SUPPORT_ && defined ACCESS_KEYS_SUPPORT
	OP_KEY_FIRST_DO_UNSPECIFIED,
	OP_KEY_LAST_DO_UNSPECIFIED = OP_KEY_FIRST_DO_UNSPECIFIED + 0x1ff,
#endif // _WML_SUPPORT_ && ACCESS_KEYS_SUPPORT

	OP_KEY_LAST_INTERNAL_EXTENDED
	/**< Last key of internally-allocated key ranges. These
	     ranges are kept for legacy and are not exposed via
	     the key system. */
};

/** Valid key predicate.
 *
 *  The input manager will verify that injected key events do
 *  use key codes that are valid and ignore attempts to
 *  inject keys outside the recognized ranges.
 *
 *  Four blocks of keys:
 *    - the predefined keys: [OP_KEY_FIRST, OP_KEY_FIRST_EXTENDED)
 *    - the extended keys:   [OP_KEY_FIRST_EXTENDED, OP_KEY_LAST_EXTENDED)
 *    - the internal keys:   [OP_KEY_FIRST_INTERNAL, OP_KEY_LAST_INTERNAL)
 *    - the pre-allocated, internal ranges:   [OP_KEY_FIRST_INTERNAL_EXTENDED, OP_KEY_LAST_INTERNAL_EXTENDED)
 *      (Voice, WML)
 */
#define IS_OP_KEY(key) \
  (key >= OP_KEY_FIRST && key < OP_KEY_FIRST_EXTENDED || \
   key >= OP_KEY_FIRST_EXTENDED && key < OP_KEY_LAST_EXTENDED || \
   key >= OP_KEY_FIRST_INTERNAL && key < OP_KEY_LAST_INTERNAL || \
   key >= OP_KEY_FIRST_INTERNAL_EXTENDED && key < OP_KEY_LAST_INTERNAL_EXTENDED)

#define IS_OP_KEY_MODIFIER(key) (OpKey::IsModifier(key))
#define IS_OP_KEY_FLIP(key) (OpKey::IsFlip(key))
#ifdef MOUSE_SUPPORT
#define IS_OP_KEY_MOUSE_BUTTON(key) (OpKey::IsMouseButton(key))
#else // MOUSE_SUPPORT
#define IS_OP_KEY_MOUSE_BUTTON(key) (FALSE)
#endif // MOUSE_SUPPORT
#define IS_OP_KEY_GESTURE(key) (OpKey::IsGesture(key))

/**
 * Opera virtual keys.
 *
 * The OpKey class defines a type (OpKey::Code) representing virtual keycodes,
 * along with operations for mapping from these unique key identifiers and
 * their key labels.
 *
 * The implementation of these operations is derived from the effective key
 * system for your build; see modules/hardcore/keys/README.txt.
 *
 * Mouse buttons, gestures are also handled by way of virtual keycodes.
 */
class OpKey
{
public:
	typedef OpVirtualKey Code;

	static const uni_char *ToString(OpKey::Code key);
	/**< Return the key name associated with 'code', or NULL
	     if unknown. */

	static OpKey::Code FromString(const uni_char *str);
	/**< For the given key name, return its key code. The key names
	     are exactly those defined by the key system configuration,
	     no default translation from some assumed keyboard configuration
	     to a key code.

	     If an unknown name, a key value of OP_KEY_FIRST is returned. */

	static OpKey::Code FromASCIIToKeyCode(char ch, ShiftKeyState &modifiers);
	/**< Map the US-ASCII character 'ch' to the virtual key and shift key state
	     required to produce it on a keyboard with a US layout.

	     If 'ch' is not in the recognized range, OP_KEY_FIRST is returned.
	     Only to be used in controlled circumstances when you do know that a
	     key is produced with a keyboard having a US keyboard layout. */

	static BOOL IsInValueSet(OpKey::Code key);
	/**< Returns TRUE if the virtual key's mapped key name (as specified
	     through the key system configuration files) is the key's
	     character value. e.g., for a key like OP_KEY_SHIFT, it would
	     be TRUE; for OP_KEY_A, it would be FALSE.

	     See hardcore/documentation/module.keys.txt for instructions on
	     how to map a key into the "value set". */

	static uni_char CharValue(OpKey::Code key);
	/**< Return the explicit character value mapped to the given key.
	     The character value is used when mapping a key's DOM event to
	     its corresponding 'charCode', and is normally derived from the
	     character value actually produced by the key. If the key
	     doesn't produce a character value, or some other value needs
	     to be reported, an explicit character value may be supplied
	     via the key system. That is the value returned by this
	     method.

	     If no explicit character value associated with the key, returns
	     0. */

	static BOOL IsModifier(OpKey::Code key);
	/**< Returns TRUE if the virtual key represents a modifier/shift key. */

	static BOOL IsFunctionKey(OpKey::Code key);
	/**< Returns TRUE if the virtual key represents a "function" key. The
	     notion of "function" key is wider than the row of keys found
	     above the standard area on a western keyboard; any key that
	     don't produce a character value (but whose primary purpose is
	     to act as a function key, to quote DOM Level3.) */

#ifdef MOUSE_SUPPORT
	static BOOL IsMouseButton(OpKey::Code key);
	/**< Returns TRUE if the virtual key represents a mouse button. */
#endif // MOUSE_SUPPORT

	static BOOL IsGesture(OpKey::Code key);
	/**< Returns TRUE if the virtual key represents a gesture. */

	static BOOL IsFlip(OpKey::Code key);
	/**< Returns TRUE if the virtual key is a flip key. */

	/** Enumeration giving some information about where
	    the key is located. DOM Level3 Events wants to know,
	    and is used to disambiguate keyboard keys with
	    same virtual key but appearing at multiple positions
	    (e.g., left and right Shift.) */
	enum Location
	{
		LOCATION_STANDARD = 0,
		LOCATION_LEFT,
		LOCATION_RIGHT,
		LOCATION_NUMPAD,
		LOCATION_MOBILE,
		LOCATION_JOYSTICK
	};

	static Location ToLocation(OpKey::Code key);
	/** Return where the virtual key is situated. If an unknown
	    key code, returns LOCATION_STANDARD. */
};

#endif // !OPKEY_AUTO_H
