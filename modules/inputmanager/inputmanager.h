/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef IM_OP_INPUTMANAGER_H
#define IM_OP_INPUTMANAGER_H

#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager_constants.h"
#include "modules/util/OpTypedObject.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"

#include "modules/pi/OpKeys.h"

class PrefsFile;
class PrefsSection;
class OpInputContext;
class ShortcutContext;
class OpFileDescriptor;

/** The global OpInputManager object. */
#define g_input_manager g_opera->inputmanager_module.m_input_manager
#define MAX_SHORTCUT_SEQUENCE_LENGTH (9)

#if !defined(IM_UI_HANDLES_SHORTCUTS)
/**
 * This listener is informed about a key press event that was sent to
 * the inputmanager which was not handled by anything in opera.
 *
 * Since keystrokes are handled asynchronously, the listener is not
 * called from inside InvokeKey(), but shortly after.
 *
 * To install a instance of this class, call
 * OpInputManager::SetExternalInputListener().
 */
class ExternalInputListener
{
public:
	virtual ~ExternalInputListener() {}
	virtual void OnKeyPressed(OpKey::Code key, ShiftKeyState shift_keys) = 0;
};
#endif // !IM_UI_HANDLES_SHORTCUTS

/**
 * A Shortcut represents a single keystroke. Either identified by a
 * a virtual key code and modifiers, or the character value produced
 * by a key. A character value is invariant across keyboard layouts
 * and keys producing an equal value (e.g., 5.)
 *
 * Shortcuts are put in sequence by ShortcutAction.
 */
class Shortcut
{
public:
	Shortcut()
		: m_key(OP_KEY_INVALID),
		  m_modifiers(SHIFTKEY_NONE),
		  m_key_value('\0')
	{
	}

	/**
	 * Is the shortcut equal to the given (key, modifiers, value) triple?
	 * If 'key' is something other than OP_KEY_INVALID, the equality is tested
	 * over the (key, modifiers) pair. Otherwise, 'value' is used.
	 *
	 * @param key The virtual key code.
	 * @param modifiers The shift,ctrl,.. modifier state.
	 * @param key_value The character value produced by the key.
	 * @returns TRUE if equal.
	 */
	BOOL Equals(OpKey::Code key, ShiftKeyState modifiers, uni_char key_value) const
	{
		if (m_key_value != 0 && m_key_value == key_value)
		{
			if (uni_isdigit(key_value))
				return m_modifiers == modifiers;
			OP_ASSERT(!uni_isalpha(key_value));
			return !((m_modifiers^modifiers) & ~SHIFTKEY_SHIFT);
		}
		else
			return (m_key != OP_KEY_INVALID && m_key == key && m_modifiers == modifiers);
	}

	/**
	 * Shortcut equality, @see Equals() for rules used.
	 */
	BOOL Equals(Shortcut& shortcut) const
	{
		return Equals(shortcut.m_key, shortcut.m_modifiers, shortcut.m_key_value);
	}

	OpKey::Code	GetKey() const {return m_key;}
	ShiftKeyState GetShiftKeys() const {return m_modifiers;}

	void Set(OpKey::Code key, ShiftKeyState modifiers, uni_char key_value)
	{
		m_key = key;
		m_modifiers = modifiers;
		m_key_value = key_value;
	}

	/**
	 * Get a string representation of the Shortcut.
	 *
	 * @param [out]result A string representation of the representatin is appended
	 *        to result.
	 */
	void GetShortcutStringL(OpString& result);

	/**
	 * Get a string representation of the Shortcut.
	 *
	 * @param [out]result A string representation of the representatin is appended
	 *        to result.
	 */
	OP_STATUS GetShortcutString(OpString& result)
	{
		TRAPD(status, GetShortcutStringL(result)); return status;
	}

private:
	OpKey::Code m_key;
	ShiftKeyState m_modifiers;
	uni_char m_key_value;
};

/** An action that triggers if a sequence of Shortcuts matches.
 */
class ShortcutAction
{
public:
	ShortcutAction()
	{
	}

	~ShortcutAction()
	{
		OP_DELETE(m_input_action);
	}

	/**
	 * Construct the ShortcutAction object. This method must always be called when ShortcutAction objects are
	 * created.
	 *
	 * @param method Type of shortcut.. mouse, keyboard etc..
	 * @param shortcut_string A string representation of a shortcut.
	 * @param input_action Action invoked by the shortcut. Ownership is transferred to the ShortcutAction object.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Construct(OpInputAction::ActionMethod method, const uni_char* shortcut_string, OpInputAction* input_action);

	OpInputAction* GetAction() const {return m_input_action;}

	Shortcut* GetShortcutByIndex(INT32 index) const {return m_shortcut_list.Get(index);}
	UINT32 GetShortcutCount() const {return m_shortcut_list.GetCount();}

	/**
	 * Check if a shortcut sequence partially matches this shortcut action.
	 *
	 * @param shortcuts The shortcuts sequence.
	 * @param shortcut_count Number of shortcuts in the sequence.
	 */
	BOOL MatchesKeySequence(Shortcut* shortcuts, unsigned shortcut_count) const;

private:

	/**
	 * Add shortcuts from given string, one per key mapped to.
	 *
	 * @param shortcut_string A string representation of the shortcut.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddShortcutsFromString(uni_char* shortcut_string);

	OP_STATUS AddShortcut(OpKey::Code key, ShiftKeyState modifiers, uni_char key_value);

	OpAutoVector<Shortcut>	m_shortcut_list;
	OpInputAction* m_input_action;
};

class ShortcutContext
	: public OpHashFunctions
{
public:
	ShortcutContext(OpInputAction::ActionMethod method);
	virtual ~ShortcutContext() {}

	OP_STATUS Construct(const char* context_name, PrefsSection* section);

	OP_STATUS AddShortcuts(const char* context_name, PrefsSection * section);

	const char* GetName() const {return m_context_name.CStr();}

	ShortcutAction* GetShortcutActionFromAction(OpInputAction* action);
	ShortcutAction* GetShortcutActionFromIndex(INT32 index) const {return m_shortcut_actions_list.Get(index);}
	INT32 GetShortcutActionCount() const {return m_shortcut_actions_list.GetCount();}

	virtual UINT32 Hash(const void* key);
	virtual BOOL KeysAreEqual(const void* key1, const void* key2);

private:
	void AddShortcutsL(const char* context_name, PrefsSection * section);

	OpHashTable m_shortcut_actions_hashtable;
	OpAutoVector<ShortcutAction> m_shortcut_actions_list;
	OpString8 m_context_name;
	OpInputAction::ActionMethod m_action_method;
};

class ShortcutContextList
{
public:

	ShortcutContextList(OpInputAction::ActionMethod method) : m_action_method(method) {}

	ShortcutContext *GetShortcutContextFromName(const char* context_name, PrefsSection* special_section = NULL);

	void Flush();

private:

	OpString8HashTable<ShortcutContext>	m_shortcut_context_hashtable;
	OpAutoVector<ShortcutContext> m_shortcut_context_list;
	OpInputAction::ActionMethod m_action_method;
};

#ifdef MOUSE_SUPPORT
/** Class used to identify mouse gestures from mouse movement */
class MouseGesture
{
public:
	/**
	 * Calculate mouse gesture.
	 *
	 * @param delta_x Horizontal mouse movement.
	 * @param delta_y Vertical mouse movement.
	 * @param threshold Gesture triggering threshold (minimum distance.)
	 * @return Virtual keycode of the mouse gesture. If the vector length
	 *         is below the threshold, OP_KEY_INVALID is returned.
	 */
	static OpKey::Code CalculateMouseGesture(int delta_x, int delta_y, int threshold);

#if 0
	// Reference implementation
	static OpKey::Code CalculateMouseGestureRef(int deltax, int deltay, int threshold);
#endif

private:
	static OpKey::Code NextGesture(OpKey::Code gesture, BOOL diagonal = FALSE);
};

/**
 * This listener gets informed about performed mouse gestures. Used by
 * the UI to be able to visualize gesture information.
 */
class MouseGestureListener
{
public:
	virtual ~MouseGestureListener() {}

	/**
	 * Called when the mouse gesture UI should be shown. It is up to the
	 * UI to decide whether to display anything.
	 */
	virtual void OnGestureShow() {}

	/** Called when moving to a new gesture region. */
	virtual void OnGestureMove(int dir) {}

	/**
	 * Called when gesture has ended, and UI should display selected gesture
	 * or fade out.
	 */
	virtual void OnFadeOutGestureUI(int selected_gesture_idx) {}

	/** Called when we need to immediately clear all mouse gesture UI. */
	virtual void OnKillGestureUI() {}
};

#define NUM_GESTURE_DIRECTIONS  (4)

/** Class used to identify mouse gestures from mouse movement */
class MouseGestureManager
	: public MessageObject
{
public:
	MouseGestureManager();
	virtual ~MouseGestureManager();

	/** Tells you where this gesture was started. */
	OpPoint GetGestureOrigin() const { return m_gesture_origin; }

	void BeginPotentialGesture(OpPoint origin, OpInputContext* input_context);
	void UpdateMouseGesture(int dir);
	void EndGesture(BOOL gesture_was_cancelled = FALSE);
	void CancelGesture()  { EndGesture(TRUE); }

	void SetListener (MouseGestureListener* listener) { m_listener = listener; }

	void ShowUIAfterDelay();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

//private: // Probably you shouldn't be messing with this, but the UI code will want to.

	struct GestureNode
	{
		int addr[MAX_SHORTCUT_SEQUENCE_LENGTH];
		int dirs[NUM_GESTURE_DIRECTIONS];
		OpInputAction* action;

		GestureNode()
			: action(NULL)
		{
			int i;
			for (i = 0; i < MAX_SHORTCUT_SEQUENCE_LENGTH; i++) addr[i] = -1;
			for (i = 0; i < NUM_GESTURE_DIRECTIONS      ; i++) dirs[i] = -1;
		}
	};

	void InitGestureTree(OpInputContext* input_context);

	void AddToTree(GestureNode* node, ShortcutAction* sa, OpInputAction* ia);

	BOOL m_callback_set;
	BOOL m_ui_shown;

	OpPoint m_gesture_origin;

	int m_active_idx;

	int m_instantiation_id; // changes every time you select a different potential gesture

	OpAutoVector<GestureNode> m_gesture_tree;

	MouseGestureListener* m_listener;
};
#endif // MOUSE_SUPPORT


/***********************************************************************************
**
**	OpInputManager
**
***********************************************************************************/

class OpInputManager
	: public MessageObject
{
	friend class OpInputAction;
public:
	OpInputManager();
	virtual	~OpInputManager();

	OP_STATUS Construct();

	/** Flush cached data. Use this when input files has changed */
	void Flush();

	/* Set/get/lock current keyboard input context */

	void UpdateRecentKeyboardChildInputContext(OpInputContext* input_context);
	void SetKeyboardInputContext(OpInputContext* input_context, FOCUS_REASON reason);
	void RestoreKeyboardInputContext(OpInputContext* input_context, OpInputContext* fallback_input_context, FOCUS_REASON reason = FOCUS_REASON_OTHER);

	OpInputContext* GetKeyboardInputContext() const {return m_keyboard_input_context;}
	OpInputContext*	GetOldKeyboardInputContext() const {return m_old_keyboard_input_context;}
	void LockKeyboardInputContext(BOOL lock) {m_lock_keyboard_input_context = lock;}

#if !defined(IM_UI_HANDLES_SHORTCUTS)
	void SetExternalInputListener(ExternalInputListener* external_input_listener) { m_external_input_listener = external_input_listener; }
#endif // !IM_UI_HANDLES_SHORTCUTS

	/** Tell input manager that an input context isn't valid at the moment. This must be done if the input context
	 * at some point has been used by input manager. The method does three actions:
	 * - clears the references from ancestors to the input contexts in the subtree of the one being released
	 * - releases keyboard input context from the subtree (unless keep_keyboard_focus is TRUE)
	 * - clears the current mouse input context if it is the one being released.
	 * @param input_context the input_context to release
	 * @param reason the reason of the release
	 * @param keep_keyboard_focus TRUE will skip the release of keyboard input context from the subtree of input_context.
	 */
	void ReleaseInputContext(OpInputContext* input_context, FOCUS_REASON reason = FOCUS_REASON_RELEASE, BOOL keep_keyboard_focus = FALSE);

	/**
	 * Invoke an action directly to an input context.
	 *
	 * @param action The action object to invoke. Note this object will be copied.
	 * @param first Start the invokation on this context.
	 * @param last End the invokation on this context
	 * @param send_prefilter_action
	 * @param action_method
	 *
	 * @return TRUE if the action has been handled, FALSE otherwise.
	 */
	BOOL InvokeAction(OpInputAction* action, OpInputContext* first = NULL, OpInputContext* last = NULL, BOOL send_prefilter_action = TRUE, OpInputAction::ActionMethod action_method = OpInputAction::METHOD_OTHER);

	/** @see InvokeAction */
	BOOL InvokeAction(OpInputAction::Action action, INTPTR action_data = 0, const uni_char* action_data_string = NULL, OpInputContext* first = NULL, OpInputContext* last = NULL, BOOL send_prefilter_action = TRUE, OpInputAction::ActionMethod action_method = OpInputAction::METHOD_OTHER);
	BOOL InvokeAction(OpInputAction::Action action, INTPTR action_data, const uni_char* action_data_string, const uni_char* action_data_string_param, OpInputContext* first = NULL, OpInputContext* last = NULL, BOOL send_prefilter_action = TRUE, OpInputAction::ActionMethod action_method = OpInputAction::METHOD_OTHER);

	/* Methods for informing the input manager of key/mouse presses.

	   - InvokeKeyDown is called when a key is pressed down, and any repeated
	     down events that might be reported if the key is held down.
	   - InvokeKeyPressed is called for each repeated "virtual" keypress
	     from a character producing key.
	   - InvokeKeyUp should only be called once when a key is released.

	   That is, for "normal" keys the pattern will be: (DOWN, PRESS)+ UP.
	   For function keys: DOWN+ UP.

	   Note that mouse buttons are treated as keys, except that InvokeKeyPressed
	   makes no sense for those and should not be called. (OP_KEY_MOUSE_BUTTON_1 etc)
	*/

	/**
	 * Issue a key depressing action, identified by the virtual key pressed
	 * along with the character value, if any, it produces.
	 * @param key The virtual key pressed.
	 * @param value The character value produced by the key.
	 *        Can be NULL; UTF-8 encoded.
	 * @param value_length The length of value.
	 * @param event_data Extra platform event data associated
	 *        with this key event.
	 * @param shift_keys The state of the modifier keys.
	 * @param repeat TRUE if this is a repeated keydown.
	 * @param location where on the keyboard (or other input devices)
	 *        the key press emanated. (to, e.g., distinguish between
	 *        left and right shifts.)
	 * @param send_prefilter_action If TRUE, first process the action
	 *        up the stack of input contexts before processing the action
	 *        as usual.
	 * @return TRUE if the text input was handled, FALSE otherwise.
	 */
	BOOL InvokeKeyDown(OpKey::Code key, const char *value, unsigned value_length, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action);
	BOOL InvokeKeyDown(OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action);
	BOOL InvokeKeyDown(OpKey::Code key, ShiftKeyState shift_keys);

	/**
	 * Issue a key release action, identified by the virtual key pressed
	 * along with the character value, if any, it produces.
	 * @param key The virtual key pressed.
	 * @param value The character value produced by the key.
	 *        Can be NULL; UTF-8 encoded.
	 * @param value_length The length of value.
	 * @param event_data Extra platform event data associated
	 *        with this key event.
	 * @param shift_keys The state of the modifier keys.
	 * @param repeat TRUE if this is a repeated keydown.
	 * @param location where on the keyboard (or other input devices)
	 *        the key press emanated. (to, e.g., distinguish between
	 *        left and right shifts.)
	 * @param send_prefilter_action If TRUE, first process the action
	 *        up the stack of input contexts before processing the action
	 *        as usual.
	 * @return TRUE if the text input was handled, FALSE otherwise.
	 */
	BOOL InvokeKeyUp(OpKey::Code key, const char *value, unsigned value_length, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action);
	BOOL InvokeKeyUp(OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action);
	BOOL InvokeKeyUp(OpKey::Code key, ShiftKeyState shift_keys);

	/**
	 * Issue a keypress action, identified by the virtual key pressed
	 * along with the character value, if any, it produces.
	 * @param key The virtual key pressed.
	 * @param value The character value produced by the key.
	 *        Can be NULL; UTF-8 encoded.
	 * @param value_length The length of value.
	 * @param event_data Extra platform event data associated
	 *        with this key event.
	 * @param shift_keys The state of the modifier keys.
	 * @param repeat TRUE if this is a repeated keydown.
	 * @param location where on the keyboard (or other input devices)
	 *        the key press emanated. (to, e.g., distinguish between
	 *        left and right shifts.)
	 * @param send_prefilter_action If TRUE, first process the action
	 *        up the stack of input contexts before processing the action
	 *        as usual.
	 * @return TRUE if the text input was handled, FALSE otherwise.
	 */
	BOOL InvokeKeyPressed(OpKey::Code key, const char *value, unsigned value_length, ShiftKeyState shift_keys, BOOL repeat, BOOL send_prefilter_action);
	BOOL InvokeKeyPressed(OpKey::Code key, const uni_char *value, ShiftKeyState shift_keys, BOOL repeat, BOOL send_prefilter_action);
	BOOL InvokeKeyPressed(OpKey::Code key, ShiftKeyState shift_keys);

	/* Based on InvokeKeyDown/Up calls, input manager knows what keys are down. Feel free to query. */
	BOOL IsKeyDown(OpKey::Code key) {return m_key_down_hashtable.Contains(key);}

	/* Is it to be considered a mouse key */
	BOOL IsMouseKey(OpKey::Code key) {return GetActionMethodFromKey(key) == OpInputAction::METHOD_MOUSE;}

	/* Get input context to use based on key */
	OpInputContext* GetInputContextFromKey(OpKey::Code key);

	/* Get action method based on key*/
	OpInputAction::ActionMethod	GetActionMethodFromKey(OpKey::Code key);

	/* Get shortcut context list based on action method */
	ShortcutContextList* GetShortcutContextListFromActionMethod(OpInputAction::ActionMethod method);

	/* Get shortcut context based on action method */
	ShortcutContext* GetShortcutContextFromActionMethodAndName(OpInputAction::ActionMethod method, const char* context_name);

	/**
	 * ResetInput will set back all keys to "up" state, and stop any
	 * tracking of shortcut combos or mouse gestures etc. It is meant to be
	 * called when the host app is deactivated since it then most likely
	 * will stop getting input messages, so to avoid getting things out of
	 * sync, it is best to just reset the states.
	 *
	 * Quick already calls this when top level windows are deactivated, but
	 * non-Quick versions might need to call this manually.
	 */
	void ResetInput();

	/**
	 * Given an action and input context (or NULL for current keyboard
	 * context), input manager can return what keyboard shortcut would
	 * trigger that action. Used to show those shortcuts in menus and
	 * button tooltips etc.
	 *
	 * @param action Generate a string of the shortcut(s) to this action.
	 * @param [out]string The resulting string.
	 * @param input_context Generate a string for shortcuts to the action
	 *        in this context.
	 * @return OpStatus::OK, OpStatus::ERR_NULL_POINTER or
	 *         OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetShortcutStringFromAction(OpInputAction* action, OpString& string, OpInputContext* input_context = NULL);

	/* Query the state of an action from a given input context (or NULL for current keyboard context) */

	INT32 GetActionState(OpInputAction* action, OpInputContext* input_context = NULL);

	/* Search for someone that is willing to identify itself as of type "type" in a given input context path */

	OpTypedObject* GetTypedObject(OpTypedObject::Type type, OpInputContext* input_context = NULL);

	/* Add/remove something implementing OpInputState to input manager's list of input state objects.
	   they will be called (OnUpdate) every time they ought to re-query their action state */

	void AddInputState(OpInputState* state);
	void RemoveInputState(OpInputState* state);

	/* Invoke an OnUpdate on all input states. This can potentially be a heavy call, so know when to call this or not */
	void UpdateAllInputStates(BOOL full_update = FALSE);

	/* Make sure input states have been updated */

	void SyncInputStates();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef MOUSE_SUPPORT
	/* Set/get/lock current mouse input context */

	void SetMouseInputContext(OpInputContext* input_context);
	OpInputContext* GetMouseInputContext() const {return m_mouse_input_context;}

	void LockMouseInputContext(BOOL lock) {m_lock_mouse_input_context = lock;}
	BOOL IsLockMouseInputContext() const { return m_lock_mouse_input_context; }

	/**
	 * Set the current mouse cursor position (screen coordinates).
	 * OpInputManager relies on having the correct position at all times,
	 * since all mouse invoked actions get the position passed along,
	 * and it is also how mouse gestures are recognized.
	 *
	 * In other words, when the platform code gets "mouse move" messages,
	 * it should call SetMousePosition().
	 *
	 * Also, when a platform typically gets a "left mouse button down"
	 * event together with the mouse coordinates, it should then call
	 * SetMousePosition first(), and then do a
	 * InvokeKeyDown(OP_KEY_MOUSE_BUTTON_1).
	 *
	 * If mouse gestures are enabled, then the implementation of this
	 * method detects mouse gestures. When a gesture is identified it
	 * calls InvokeKeyPressed() with the current shift-key state as
	 * returned by OpSystemInfo::GetShiftKeyState(). The mask
	 * ignore_modifiers can be used to mask away some of the shift key
	 * states. If the value is 0, the full shift-key states is used.
	 *
	 * @param new_mouse_position
	 *   New mouse position (screen co-ordinates).
	 * @param ignore_modifiers
	 *   Shift-key states to ignore for gesture processing.
	 */
	void SetMousePosition(const OpPoint& new_mouse_position, INT32 ignore_modifiers = SHIFTKEY_NONE);
	/** Get the current mouse cursor position (screen coordinates). */
	OpPoint GetMousePosition() const {return m_mouse_position;}

	/** Are we gesure recognizing? */
	BOOL IsGestureRecognizing() {return IsKeyDown(GetGestureButton()) && !IsFlipping();}

	/** Get gesture button */
	OpKey::Code GetGestureButton() const {return OP_KEY_MOUSE_BUTTON_2;}

	/** Checks if a key up will trigger a gesture action (so that other actions can be prevented). */
	BOOL WillKeyUpTriggerGestureAction(OpKey::Code key) const { return key == GetGestureButton() && m_loaded_gesture_action != NULL; }

	/** Set flipping */
	void SetFlipping(BOOL flipping) {m_is_flipping = flipping;}

	/** Are we button flipping? */
	BOOL IsFlipping() const {return m_is_flipping;}

	/** Get flip buttons */
	void GetFlipButtons(OpKey::Code &back_button, OpKey::Code &forward_button) const;

	/** Set a combination of TOUCH_INPUT_STATE flags. It can be used by the platform to track
	  * global states on active touch input such as mouse move or similar.
	  */
	void SetTouchInputState(UINT32 touch_state) { m_touch_state = touch_state; }

	/** Get the TOUCH_INPUT_STATE flags currently active */
	UINT32 GetTouchInputState() { return m_touch_state; }

	/**	Temporarily disables mouse gestures,
		or re-enables them after a call to disable them.
		(This is "temporary", as opposed to the "permanent" setting
		in the user preferences.) */
	void TemporarilyDisableMouseGestures(BOOL are_we_disabling) { m_mouse_gestures_temporarily_disabled = are_we_disabling; }
#endif // MOUSE_SUPPORT

	void SetBlockInputRecording( BOOL state ) { m_block_input_recording = state; }

	PrefsSection* GetPrefsSectionFromName(const uni_char* section_name);
#ifdef SPECIAL_INPUT_SECTIONS
	OP_STATUS AddSpecialKeyboardSection(const char* section_name, PrefsSection* section) { return m_special_inputsections.Add(section_name, section);}
#endif // SPECIAL_INPUT_SECTIONS

	OP_STATUS GetActionFromString(const char* string, OpInputAction::Action* result);

	static OP_STATUS OpKeyToLanguageString(OpKey::Code key, OpString& string);

#ifdef MOUSE_SUPPORT
	MouseGestureManager* SetMouseGestureListener (MouseGestureListener* listener) { m_gesture_manager.SetListener(listener); return &m_gesture_manager; }
#endif

private:

	class OpStringToActionHashTable
		: private OpGenericString8HashTable
	{
		union HashItem
		{
			void* value;
			OpInputAction::Action action;
		};

	public:
		OP_STATUS Add(const char* key, OpInputAction::Action action);
		OP_STATUS GetAction(const char* key, OpInputAction::Action* action);
		INT32 GetCount() {return OpGenericString8HashTable::GetCount();}
	};

	// FIXME: Platform-specific code. Should be rewritten/removed

	/**
	 * Tell the application some activity has occured.
	 */
	void RegisterActivity();

	/**
	 * Should mouse gestures be invoked?
	 *
	 * @return TRUE is gestures should be invoked, FALSE otherwise.
	 */
#ifdef MOUSE_SUPPORT
	BOOL ShouldInvokeGestures();
#endif

	// End platform-specific.

	void BroadcastInputContextChanged(BOOL keyboard, OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason = FOCUS_REASON_OTHER) const;

	BOOL InvokeActionInternal(OpInputAction* action, OpInputContext* first = NULL, OpInputContext* last = NULL, BOOL send_prefilter_action = TRUE);

	OpStringToActionHashTable m_action_hashtable;

	// list of input states

	Head m_input_states;
	Head m_full_update_input_states;
	BOOL m_full_update;
	BOOL m_update_message_pending;

	// keyboard stuff

	OpInputContext* m_keyboard_input_context;
	OpInputContext* m_old_keyboard_input_context;
	BOOL m_lock_keyboard_input_context;
	Shortcut m_key_sequence[MAX_SHORTCUT_SEQUENCE_LENGTH + 1];
	UINT32 m_key_sequence_id;
	OpINT32Set m_key_down_hashtable;
	ShortcutContextList m_keyboard_shortcut_context_list;
	BOOL m_block_input_recording;

	OpInputContext* m_action_first_input_context;
#if !defined(IM_UI_HANDLES_SHORTCUTS)
	ExternalInputListener* m_external_input_listener;
#endif // !IM_UI_HANDLES_SHORTCUTS

		// mouse stuff

#ifdef MOUSE_SUPPORT
	OpInputContext* m_mouse_input_context;
	BOOL m_lock_mouse_input_context;
	BOOL m_is_flipping;
	OpPoint m_mouse_position;
	OpPoint m_first_gesture_mouse_position;
	OpPoint m_last_gesture_mouse_position;
	INT32 m_last_gesture;
	OpInputAction* m_loaded_gesture_action;
	BOOL m_gesture_recording_was_attempted;
	ShortcutContextList m_mouse_shortcut_context_list;
	MouseGestureManager	m_gesture_manager;
	BOOL m_mouse_gestures_temporarily_disabled;
#endif // MOUSE_SUPPORT

	friend class MouseGestureManager;

#ifdef SPECIAL_INPUT_SECTIONS
	OpAutoString8HashTable<PrefsSection> m_special_inputsections;
#endif // SPECIAL_INPUT_SECTIONS

	UINT32	m_touch_state;		// a combination of TOUCH_INPUT_STATE flags
};

#endif // !IM_OP_INPUTMANAGER_H
