/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPMESSAGE_LOOP_LEGACY_H
#define WINDOWS_OPMESSAGE_LOOP_LEGACY_H
#ifndef NS4P_COMPONENT_PLUGINS

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/quick/managers/sleepmanager.h"

#include "modules/hardcore/component/OpComponentPlatform.h"

#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/WindowsTimer.h"

/** Maximum number of unprocessed WM_KEYDOWN entries recorded. */
#define MAX_KEYDOWN_QUEUE_LENGTH 128

/** Maximum number of non-standard virtual key => WM_CHAR character value
 *  mappings recorded. */
#define MAX_KEYUP_TABLE_LENGTH 32

typedef WAITORTIMERCALLBACKFUNC WAITORTIMERCALLBACK ;

class WindowsOpMessageLoop
	: public WindowsOpComponentPlatform
	, public StayAwakeListener
	, public WindowsTimerListener
	, private MessageObject
{
	public:

							WindowsOpMessageLoop();
							~WindowsOpMessageLoop();

		OP_STATUS           Init();
		OP_STATUS           Run() {return Run(NULL);}

		void                PostMessageIfNeeded();

		OpKey::Code			ConvertKey_VK_To_OP(UINT32 vk_key);
		BOOL				TranslateOperaMessage(MSG *pMsg, BOOL &called_translate);

		inline void         SSLSeedFromMessageLoop(const MSG *msg);

		OP_STATUS           Run(WindowsOpWindow* wait_for_this_dialog_to_close);
		void                EndDialog(WindowsOpWindow* window) {m_dialog_hash_table.Remove(window);}
		BOOL                GetIsExiting() const { return m_exiting; }
		void                SetIsExiting();

		BOOL                OnAppCommand(UINT32 lParam);

		void                DispatchAllPostedMessagesNow();

		virtual void        HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		// implementing OpComponentPlatform
		virtual void RequestRunSlice(unsigned int limit);
		virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type);
		virtual OP_STATUS SendMessage(OpTypedMessage* message) { return OpStatus::ERR_NOT_SUPPORTED; }
		virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags = PROCESS_ANY);

		// implementing StayAwakeListener
		virtual void OnStayAwakeStatusChanged(BOOL stay_awake);

		// implementing WindowsTimerListener
		// NOTE this will be run in a different thread.
		virtual void OnTimeOut(WindowsTimer* timer);

	private:
		// Internal class.
		class AppCommandInfo
		{
		public:
			AppCommandInfo(OpInputAction::Action action, OpInputContext* input_context, INTPTR action_data)
			:   m_action(action),
				m_action_data(action_data),
				m_input_context(input_context)
			{
			}

			OpInputAction::Action m_action;
			OpInputContext* m_input_context;
			INTPTR m_action_data;
		};

		// Methods.
		void                PostMessageIfNeeded_Internal();
		void                OnReschedulingNeeded_Internal(int next_event);

		void                StopTimer();
		void                StartTimer(int delay);
		void                DispatchPostedMessage();
		void                Unregister();
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

		static void         InitSpecialTimerFunctions();

		// Members.
		OpPointerSet<WindowsOpWindow>   m_dialog_hash_table;
		BOOL                            m_message_posted;
		BOOL                            m_core_has_message_waiting;
		BOOL                            m_timeout;
		int                             m_stop_after_timeout;

		CRITICAL_SECTION                m_post_lock;

		static HWND                     s_hwnd;

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

		HANDLE                          m_timer_queue;          // 2000 and higher offers timer queues
		HANDLE                          m_timer_queue_timer;

		BOOL                            m_msg_delay_handler_registered;
		OpAutoVector<MSG>               m_silverlight_messages;
		UINT                            m_is_processing_silverlight_message;

		BOOL                            m_exiting;
		DWORD                           m_exiting_timestamp;
		WindowsTimer                    m_timer;
};

extern WindowsOpMessageLoop* g_windows_message_loop;

#endif // !NS4P_COMPONENT_PLUGINS
#endif // WINDOWS_OPMESSAGE_LOOP_LEGACY_H
