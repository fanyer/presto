/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPMESSAGE_LOOP_H
#define WINDOWS_OPMESSAGE_LOOP_H

#ifndef NS4P_COMPONENT_PLUGINS

# include "platforms/windows/pi/WindowsOpMessageLoopLegacy.h"

#else // NS4P_COMPONENT_PLUGINS

#include "adjunct/quick/managers/sleepmanager.h"

#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
/** Maximum number of unprocessed WM_KEYDOWN entries recorded. */
#define MAX_KEYDOWN_QUEUE_LENGTH 128

/** Maximum number of non-standard virtual key => WM_CHAR character value
 *  mappings recorded. */
#define MAX_KEYUP_TABLE_LENGTH 32


#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/utils/sync_primitives.h"

class WindowsOpMessageLoop
	: public WindowsOpComponentPlatform
	, public StayAwakeListener
	, private WindowsOpComponentPlatform::WMListener
{
public:
						WindowsOpMessageLoop();
						~WindowsOpMessageLoop();

	OP_STATUS			Init();
	OP_STATUS			Run() {return Run(NULL);}

	OpKey::Code			ConvertKey_VK_To_OP(UINT32 vk_key);
	BOOL				TranslateOperaMessage(MSG *pMsg, BOOL &called_translate);

	inline void			SSLSeedFromMessageLoop(const MSG *msg);

	OP_STATUS			Run(WindowsOpWindow* wait_for_this_dialog_to_close);
	void				EndDialog(WindowsOpWindow* window) {m_dialog_hash_table.Remove(window);}

	BOOL				OnAppCommand(UINT32 lParam);

	void				DispatchAllPostedMessagesNow();

	// implementing OpComponentPlatform
	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type);
	virtual OP_STATUS RequestPeerThread(int& peer, OpMessageAddress requester, OpComponentType type) { return OpStatus::ERR_NOT_SUPPORTED; }

	//implementing StayAwakeListener
	virtual void OnStayAwakeStatusChanged(BOOL stay_awake);

private:
	// Internal class.
	class AppCommandInfo
	{
	public:
		AppCommandInfo(OpInputAction::Action action, OpInputContext* input_context, INTPTR action_data)
		:	m_action(action),
			m_action_data(action_data),
			m_input_context(input_context)
		{
		}

		OpInputAction::Action m_action;
		OpInputContext* m_input_context;
		INTPTR m_action_data;
	};

	//Used to store information about processes started by RequestPeer
	struct ProcessDetails
	{
		~ProcessDetails()
		{
			if (!pid)
				UnregisterWait(process_wait);
			else
				UnregisterWaitEx(process_wait, INVALID_HANDLE_VALUE);
			CloseHandle(process_handle);
		}

		HANDLE process_handle;
		INT32 pid;
		HANDLE process_wait;
		WindowsOpMessageLoop* message_loop;
	};

	// Methods.
	static VOID CALLBACK ReportDeadPeer(PVOID process_details, BOOLEAN timeout);

	virtual void HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	OP_STATUS ProcessMessage(MSG &msg);
	OP_STATUS DoLoopContent(BOOL ran_core);


	// Members.
	OpCriticalSection				m_known_processes_list_access;
	/* List of initialized ProcessDetails structures. Used to make sure we
	   don't leak them on exit. */
	OpVector<ProcessDetails>		m_known_processes_list;

	BOOL							m_destructing;

	OpPointerSet<WindowsOpWindow>	m_dialog_hash_table;
	WindowsOpWindow*				m_wait_for_this_dialog_to_close;
	BOOL							m_use_RTL_shortcut;

	UINT 				GetRShiftScancode() { return m_rshift_scancode; }

	/** Determine if the virtual_key and modifiers will produce a character value.
	 * If TRUE, the reporting of a keydown must wait until the translation
	 * of the key has occurred. If FALSE, the key does not produce a character
	 * value and its keydown event may be reported right away. */
	BOOL 				HasCharacterValue(OpKey::Code virtual_key, ShiftKeyState shiftkeys, WPARAM wParam, LPARAM lParam);

	/** Insert a mapping from a WM_KEYDOWN's wParam to the wParam
	 * that its corresponding WM_CHAR produced. If a mapping already
	 * exists, it's value will be overwritten/updated. */
	void 				AddKeyValue(WPARAM wParamDown, WPARAM wParamChar);

	/** Return the character value that virtual_key has been
	 * recorded as producing. Returns TRUE if found, and wParamChar
	 * is updated with its value. FALSE if no mapping known. */
	BOOL 				LookupKeyCharValue(WPARAM virtual_key, WPARAM &wParamChar);

	/** Clear out any queued character values kept. */
	void 				ClearKeyValues();

	AppCommandInfo*					m_pending_app_command;

	/** Scancode for the right Shift key; used to disambiguate location of
		the VK_SHIFT virtual key. */
	UINT 							m_rshift_scancode;

	/** Associating the WPARAM from a WM_KEYDOWN with the translated character
	 * it produces in a WM_CHAR.
	 *
	 * Used when processing WM_KEYUP, having to affix the character value that
	 * its key produces/d. @see m_keyvalue_table. */
	class KeyValuePair
	{
	public:
		/** The WPARAM of the WM_KEYDOWN that initiated the key event. */
		WPARAM keydown_wParam;

		/** The WPARAM that the WM_CHAR reported for the WM_KEYDOWN
		 * corresponding to keydown_wParam. */
		WPARAM char_wParam;
	};

	/** The delivery of keyup events must include the character value
	 * of the virtual key. The WM_KEYUP only supplies the virtual key,
	 * hence a table is kept of virtual keys and the character values
	 * they produced on WM_CHAR. If a valid match is found for the virtual
	 * key, that entry's character will be included. If not, no character
	 * value will be included. For function and modifier keys that do
	 * not produce character values, that is expected.
	 *
	 * To handle this, keep a mapping table for all standard VKs together
	 * with a fixed size one for non-standard ones that somehow are generated
	 * (its size controlled by MAX_KEYUP_TABLE LENGTH.) Should the user
	 * be able to hold down more non-standard keys than that without
	 * first releasing any, keyup events for these won't have correct
	 * character value information attached. Given that it is unclear
	 * if any virtual key map into this range, the risk of information
	 * loss is on the low side. KeyValuePair encodes the association
	 * for these non-standard keys. */
	WPARAM*							m_keyvalue_vk_table;
	KeyValuePair*					m_keyvalue_table;

	/** Index of next available slot in the m_keyvalue_table table. */
	unsigned int 					m_keyvalue_table_index;
};

extern WindowsOpMessageLoop* g_windows_message_loop;

#endif // NS4P_COMPONENT_PLUGINS
#endif // WINDOWS_OPMESSAGE_LOOP_H
