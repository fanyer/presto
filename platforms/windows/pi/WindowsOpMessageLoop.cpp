/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifndef NS4P_COMPONENT_PLUGINS

#include "platforms/windows/pi/WindowsOpMessageLoopLegacy.cpp"

#else // NS4P_COMPONENT_PLUGINS

#include "platforms/windows/pi/WindowsOpMessageLoop.h"

#include "adjunct/quick/managers/kioskmanager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#include "modules/ns4plugins/src/plugin.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/pi/WindowsOpDragManager.h"
#include "platforms/windows/pi/WindowsOpPluginAdapter.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/IntMouse.h"

#define WM_APPCOMMAND					  0x0319
#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_MASK  0xF000
#define GET_APPCOMMAND_LPARAM(lParam)	((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define GET_DEVICE_LPARAM(lParam)		((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))

WindowsOpMessageLoop* g_windows_message_loop = NULL;

/***********************************************************************************
**
**	WindowsOpMessageLoop
**
***********************************************************************************/

WindowsOpMessageLoop::WindowsOpMessageLoop()
	: m_pending_app_command(NULL)
	, m_use_RTL_shortcut(FALSE)
	, m_wait_for_this_dialog_to_close(NULL)
	, m_destructing(FALSE)
	/* Support for "new" key events. */
	, m_rshift_scancode(0)
	, m_keyvalue_vk_table(NULL)
	, m_keyvalue_table(NULL)
	, m_keyvalue_table_index(0)
{
	g_sleep_manager->SetStayAwakeListener(this);

	this->SetWMListener(WM_DEAD_COMPONENT_MANAGER, this);
	m_rshift_scancode = MapVirtualKeyEx(VK_RSHIFT, MAPVK_VK_TO_VSC, GetKeyboardLayout(0));
}

/***********************************************************************************
**
**	~WindowsOpMessageLoop
**
***********************************************************************************/

WindowsOpMessageLoop::~WindowsOpMessageLoop()
{
	OP_DELETE(m_pending_app_command);

	g_sleep_manager->SetStayAwakeListener(NULL);

	m_known_processes_list_access.Enter();
	m_destructing = TRUE;
	m_known_processes_list_access.Leave();
	// If this asserts and we are left with stray plugin wrapper processes
	// after that then consider signaling shared memory handle maybe.
	OP_ASSERT(m_known_processes_list.GetCount() == 0);
	m_known_processes_list.DeleteAll();

	this->RemoveWMListener(WM_DEAD_COMPONENT_MANAGER);

	OP_DELETEA(m_keyvalue_vk_table);
	OP_DELETEA(m_keyvalue_table);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::Init()
{
	RETURN_IF_ERROR(WindowsOpComponentPlatform::Init());

	{
		OP_PROFILE_METHOD("Got keyboard layout list");

		//We need this to know if we want to support the RTL shortcuts(Ctrl+Left/Right Shift)
		UINT n_lang = GetKeyboardLayoutList(0, NULL);
		HKL *langs = new HKL[n_lang];
		GetKeyboardLayoutList(n_lang, langs);

		for (unsigned i = 0; i < n_lang; i++)
		{
			DWORD lcid = MAKELCID(langs[i], SORT_DEFAULT);
			LOCALESIGNATURE localesig;

			if (GetLocaleInfoW(lcid, LOCALE_FONTSIGNATURE, (LPWSTR)&localesig, (sizeof(localesig)/sizeof(WCHAR))) &&
				(localesig.lsUsb[3] & 0x08000000))
			{
				m_use_RTL_shortcut = TRUE;
				break;
			}
		}

		delete[] langs;
	}


	m_keyvalue_vk_table = OP_NEWA(WPARAM, 0x100);
	RETURN_OOM_IF_NULL(m_keyvalue_vk_table);
	op_memset(m_keyvalue_vk_table, 0, 0x100 * sizeof(WPARAM));

	m_keyvalue_table = OP_NEWA(KeyValuePair, MAX_KEYUP_TABLE_LENGTH);
	RETURN_OOM_IF_NULL(m_keyvalue_table);
	m_keyvalue_table_index = 0;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Run
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::Run(WindowsOpWindow* wait_for_this_dialog_to_close)
{
	if (wait_for_this_dialog_to_close)
	{
		m_dialog_hash_table.Add(wait_for_this_dialog_to_close);
	}

	WindowsOpWindow* old_wait_for_this_dialog_to_close = m_wait_for_this_dialog_to_close;
	m_wait_for_this_dialog_to_close = wait_for_this_dialog_to_close;

	OP_STATUS ret = WindowsOpComponentPlatform::Run();

	m_wait_for_this_dialog_to_close = old_wait_for_this_dialog_to_close;
	m_exit_loop = FALSE;

	return ret;
}

OP_STATUS WindowsOpMessageLoop::ProcessMessage(MSG &msg)
{
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	SSLSeedFromMessageLoop(&msg);
#endif

	//If we need to give a specific treatment to some messages, add a switch here andmake the code below be the default case.
	BOOL called_translate;
	if (!TranslateOperaMessage(&msg, called_translate))
	{
		// Nothing ate the message, so translate and dispatch it

		if (!called_translate)
			TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpMessageLoop::DoLoopContent(BOOL ran_core)
{
	if ((m_wait_for_this_dialog_to_close && !m_dialog_hash_table.Contains(m_wait_for_this_dialog_to_close)))
		m_exit_loop = TRUE;

	if (m_pending_app_command != NULL && m_event_flags != PROCESS_IPC_MESSAGES)
	{
		//Don't process the message again if we happen to reenter.
		AppCommandInfo* pending_app_command = m_pending_app_command;
		m_pending_app_command = NULL;

		g_input_manager->InvokeAction(pending_app_command->m_action, pending_app_command->m_action_data, NULL, pending_app_command->m_input_context);

		OP_DELETE(pending_app_command);
	}

	return OpStatus::OK;
}

void WindowsOpMessageLoop::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OP_ASSERT (uMsg == WM_DEAD_COMPONENT_MANAGER);
	OP_ASSERT (IsInitialProcess());

	INT32 pid = wParam;

	if (WindowsSharedMemoryManager::GetInstance())
		WindowsSharedMemoryManager::GetInstance()->CloseMemory(OpMessageAddress(pid, 0, 0));

	g_component_manager->PeerGone(pid);
}

/***********************************************************************************
**
**	RequestPeer
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type)
{
	OP_ASSERT (IsInitialProcess());

	if (!IsInitialProcess())
		return OpStatus::ERR;

	if (!m_shared_mem)
		return g_component_manager->HandlePeerRequest(requester, type);

	switch (type)
	{
		case COMPONENT_TEST:
		case COMPONENT_PLUGIN:
		case COMPONENT_PLUGIN_WIN32:
		{
			OpFile executable;
			if (type == COMPONENT_PLUGIN)
				executable.Construct(UNI_L("opera_plugin_wrapper.exe"), OPFILE_PLUGINWRAPPER_FOLDER);
			else if (type == COMPONENT_PLUGIN_WIN32)
				executable.Construct(UNI_L("opera_plugin_wrapper_32.exe"), OPFILE_PLUGINWRAPPER_FOLDER);
			else
				executable.Construct(UNI_L("opera.exe"), OPFILE_RESOURCES_FOLDER);

			BOOL exists;
			RETURN_IF_ERROR(executable.Exists(exists));
			if (!exists)
				return OpStatus::ERR;

			OpString log_folder;
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CRASHLOG_FOLDER, log_folder));
			if (log_folder[log_folder.Length() - 1] == PATHSEPCHAR)
				log_folder[log_folder.Length() - 1] = 0;

			UniString params;
			params.AppendFormat(UNI_L("\"%s\" -newprocess \"%d %d %d %d %d\" -logfolder \"%s\""),
			                    executable.GetFullPath(), GetCurrentProcessId(), type, requester.cm, requester.co, requester.ch, log_folder.CStr());

			OpAutoArray<uni_char> params_ptr = OP_NEWA(uni_char, params.Length()+1);
			params.CopyInto(params_ptr.get(), params.Length());
			params_ptr[params.Length()] = 0;

			STARTUPINFO startup_info = {0};
			startup_info.cb = sizeof(startup_info);

			PROCESS_INFORMATION pi;

			if (!CreateProcess(NULL, params_ptr.get(), NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &pi))
				return OpStatus::ERR;

			peer = pi.dwProcessId;

			CloseHandle(pi.hThread);

			ProcessDetails* details = OP_NEW(ProcessDetails, ());
			RETURN_OOM_IF_NULL(details);
			details->pid = pi.dwProcessId;
			details->process_handle = pi.hProcess;
			details->message_loop = this;

			m_known_processes_list_access.Enter();
			OpStatus::Ignore(m_known_processes_list.Add(details));
			m_known_processes_list_access.Leave();

			//In the unlikely event where this fails, we'll only be unable to know if the process crash, so let's ignore the error.
			RegisterWaitForSingleObject(&(details->process_wait), pi.hProcess, ReportDeadPeer, details, INFINITE, WT_EXECUTEONLYONCE);

			return OpStatus::OK;
		}

		default:
			return g_component_manager->HandlePeerRequest(requester, type);
	}
}

/*static*/
VOID WindowsOpMessageLoop::ReportDeadPeer(PVOID process_details, BOOLEAN timeout)
{
	ProcessDetails* details = static_cast<ProcessDetails*>(process_details);

	::SendNotifyMessage(details->message_loop->GetMessageWindow(), WM_DEAD_COMPONENT_MANAGER, details->pid, 0);

	OpCriticalSection &sync_section = details->message_loop->m_known_processes_list_access;

	sync_section.Enter();
	if (!details->message_loop->m_destructing)
	{
		details->pid = 0; //To make the destructor of ProcessDetails use the non-waiting UnregisterWait. (prevents deadlocks)
		OpStatus::Ignore(details->message_loop->m_known_processes_list.Delete(details));
	}
	sync_section.Leave();
}

/***********************************************************************************
**
**	OnStayAwakeStatusChanged
**
***********************************************************************************/

void WindowsOpMessageLoop::OnStayAwakeStatusChanged(BOOL stay_awake)
{
	SetThreadExecutionState(stay_awake ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED : ES_CONTINUOUS);
}

void WindowsOpMessageLoop::DispatchAllPostedMessagesNow()
{
	while (g_component_manager->RunSlice() != 0);
}

#endif // NS4P_COMPONENT_PLUGINS

/** Bits 16-23 contains the key scancode. */
static inline unsigned char
GetKeyScancode(LPARAM lParam)
{
	return (lParam >> 16) & 0xff;
}

/** Returns TRUE if the LPARAM is for a repeating keydown. */
static inline BOOL
IsKeyRepeat(LPARAM lParam)
{
	return (HIWORD(lParam) & KF_REPEAT) == KF_REPEAT;
}

static OpKey::Location
GetKeyLocation(WPARAM vk, LPARAM lParam)
{
	switch (vk)
	{
	case VK_SHIFT:
		{
			unsigned scancode = (lParam & 0xFF0000) >> 16;
			if (scancode == MapVirtualKey(VK_LSHIFT, 0))
				return OpKey::LOCATION_LEFT;
			else if (scancode == MapVirtualKey(VK_RSHIFT, 0))
				return OpKey::LOCATION_RIGHT;
		}
		break;
	case VK_MENU:
		if (GetKeyState(VK_LMENU) & 0x8000)
			return OpKey::LOCATION_LEFT;
		else if (GetKeyState(VK_RMENU) & 0x8000)
			return OpKey::LOCATION_RIGHT;
		else if (lParam & (0x1 << 24))
			return OpKey::LOCATION_RIGHT;
		else
			return OpKey::LOCATION_LEFT;
		break;
	case VK_CONTROL:
		if (GetKeyState(VK_LCONTROL) & 0x8000)
			return OpKey::LOCATION_LEFT;
		else if (GetKeyState(VK_RCONTROL) & 0x8000)
			return OpKey::LOCATION_RIGHT;
		else if (lParam & (0x1 << 24))
			return OpKey::LOCATION_RIGHT;
		else
			return OpKey::LOCATION_LEFT;
		break;
	case VK_RETURN:
		if (lParam & (0x1 << 24))
			return OpKey::LOCATION_NUMPAD;
		break;
	case VK_CLEAR:
		return OpKey::LOCATION_NUMPAD;
	}

	return OpKey::LOCATION_STANDARD;
}


/***********************************************************************************
**
**	Key and character translation.
**
***********************************************************************************/

void
WindowsOpMessageLoop::AddKeyValue(WPARAM wParamDown, WPARAM wParamChar)
{
	if (wParamDown <= 0xff)
	{
		m_keyvalue_vk_table[wParamDown] = wParamChar;
		return;
	}
	unsigned int last_available = UINT_MAX;
	for (unsigned int i = 0; i < m_keyvalue_table_index; i++)
	{
		WPARAM keydown_wParam = m_keyvalue_table[i].keydown_wParam;
		if (keydown_wParam == wParamDown)
		{
			m_keyvalue_table[i].char_wParam = wParamChar;
			return;
		}
		else if (!keydown_wParam)
			last_available = i;
	}

	OP_ASSERT(m_keyvalue_table_index < MAX_KEYUP_TABLE_LENGTH || !"Exhausted key value table");
	if (last_available != UINT_MAX)
	{
		m_keyvalue_table[last_available].keydown_wParam = wParamDown;
		m_keyvalue_table[last_available].char_wParam = wParamChar;
	}
	else if (m_keyvalue_table_index < MAX_KEYUP_TABLE_LENGTH)
	{
		m_keyvalue_table[m_keyvalue_table_index].keydown_wParam = wParamDown;
		m_keyvalue_table[m_keyvalue_table_index].char_wParam = wParamChar;
		m_keyvalue_table_index++;
	}
}

BOOL
WindowsOpMessageLoop::LookupKeyCharValue(WPARAM virtual_key, WPARAM &wParamChar)
{
	if (virtual_key <= 0xff)
	{
		wParamChar = m_keyvalue_vk_table[virtual_key];
		return TRUE;
	}
	for (unsigned int i = 0; i < m_keyvalue_table_index; i++)
	{
		if (m_keyvalue_table[i].keydown_wParam == virtual_key)
		{
			wParamChar = m_keyvalue_table[i].char_wParam;
			m_keyvalue_table[i].keydown_wParam = 0;
			return TRUE;
		}
	}
	return FALSE;
}

void
WindowsOpMessageLoop::ClearKeyValues()
{
	m_keyvalue_table_index = 0;
}

BOOL
WindowsOpMessageLoop::HasCharacterValue(OpKey::Code vkey, ShiftKeyState shiftkeys, WPARAM wParam, LPARAM lParam)
{
	if (vkey == OP_KEY_BACKSPACE || vkey == OP_KEY_ESCAPE)
		return TRUE;
	else if ((shiftkeys & (SHIFTKEY_CTRL | SHIFTKEY_ALT)) == (SHIFTKEY_CTRL | SHIFTKEY_ALT))
	{
		/* If Ctrl-Alt is active, three kinds of keys possible:
		 *  - a composition dead key.
		 *  - a key not producing any character value. (Alt-Ctrl-G on a US keyboard.)
		 *  - a normal key producing a character value (Alt-Ctrl-E on a Norwegian keyboard.)
		 *
		 * The middle option doesn't produce a value, the other two do (the first one
		 * only upon the next key pressed.) Single out the middle one by doing the
		 * key translation & checking if the outcome is empty. If so, the handling
		 * of its keydown must generate an event right away (i.e., there won't be a
		 * follow-up WM_CHAR message that will handle it.)
		 */
		BYTE key_state[256];
		if (GetKeyboardState(key_state))
		{
			WCHAR key_value[5];
			int length = ToUnicodeEx(wParam, lParam, key_state, key_value, 4, 0, GetKeyboardLayout(0));

			/* If this was a dead key, ToUnicodeEx() will have side-effected
			 * the keyboard buffer. The state of that buffer must be left
			 * undisturbed for subsequent key input, so repeat the call to
			 * ToUnicodeEx() to undo the side-effect the dead key had.
			 * [Non-stateful APIs are to be preferred on the whole, but have
			 * no option but to use ToUnicodeEx() here.]
			 *
			 * This is unnecessary if the key produced no translation to start with. */
			if (length != 0)
				ToUnicodeEx(wParam, lParam, key_state, key_value, 4, 0, GetKeyboardLayout(0));

			return length != 0;
		}
	}
	else if (shiftkeys == SHIFTKEY_CTRL)
	{
		if (vkey >= OP_KEY_A && vkey <= OP_KEY_Z)
			return TRUE;
		else if (vkey == OP_KEY_OEM_4 || vkey == OP_KEY_OEM_6 || vkey == OP_KEY_OEM_5)
			return TRUE;
	}
	else if ((shiftkeys & (SHIFTKEY_CTRL | SHIFTKEY_SHIFT)) == (SHIFTKEY_CTRL | SHIFTKEY_SHIFT))
	{
		if (vkey >= OP_KEY_A && vkey <= OP_KEY_Z)
			return TRUE;
		else if (vkey == OP_KEY_2 || vkey == OP_KEY_6 || vkey == OP_KEY_OEM_MINUS || vkey == OP_KEY_OEM_2 || vkey == OP_KEY_OEM_102)
			return TRUE;
	}
	else if (!(OpKey::IsFunctionKey(vkey) || OpKey::IsModifier(vkey)))
		return TRUE;

	return FALSE;
}

//#pragma PRAGMA_OPTIMIZE_SPEED

extern BOOL IsPluginWnd(HWND hWnd, BOOL fTestParents);

extern int IntelliMouse_GetLinesToScroll(HWND hwnd, short zDelta, BOOL* scroll_as_page);

#if 0
#ifdef _DEBUG
static void
OutputKeyMessage(MSG *pMsg)
{
		OpString debug;
		switch (pMsg->message)
		{
			case WM_KEYDOWN:
				debug.Set(UNI_L("WM_KEYDOWN: "));
				break;
			case WM_KEYUP:
				debug.Set(UNI_L("WM_KEYUP: "));
				break;
			case WM_CHAR:
				debug.Set(UNI_L("WM_CHAR: "));
				break;
			case WM_DEADCHAR:
				debug.Set(UNI_L("WM_DEADCHAR: "));
				break;
			case WM_SYSKEYDOWN:
				debug.Set(UNI_L("WM_SYSKEYDOWN: "));
				break;
			case WM_SYSKEYUP:
				debug.Set(UNI_L("WM_SYSKEYUP: "));
				break;
			case WM_SYSCHAR:
				debug.Set(UNI_L("WM_SYSCHAR: "));
				break;
			case WM_SYSDEADCHAR:
				debug.Set(UNI_L("WM_SYSDEADCHAR: "));
				break;
		}
		debug.AppendFormat(UNI_L("%d\n"), pMsg->wParam);
		OutputDebugString(debug.CStr());
}
#endif // _DEBUG
#endif // 0

/***********************************************************************************
**
**	TranslateOperaMessage
**
***********************************************************************************/

BOOL WindowsOpMessageLoop::TranslateOperaMessage(MSG *pMsg, BOOL &called_translate)
{
	called_translate = FALSE;
#ifndef NS4P_COMPONENT_PLUGINS
	if (pMsg->message == WM_KEYUP || pMsg->message == WM_KEYDOWN)
		if (WindowsOpPluginWindow::HandleFocusedPluginWindow(pMsg))
			return TRUE;
#endif // !NS4P_COMPONENT_PLUGINS

	// Send keyboard input to OpInputManager.
	// InvokeKeyDown() when a key is pressed down, and on repeat.
	// InvokeKeyUp() when a key is released
	// InvokeKeyPressed() when a key is pressed down, with repeating
	// For more documentation: http://wiki/developerwiki/Platform/windows/KeyboardHandling

	static OpKey::Location s_new_rtl_direction = OpKey::LOCATION_STANDARD;

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		ShiftKeyState shift_keys = g_op_system_info->GetShiftKeyState();

		// The inputmethod hacks requires the current state so we cannot use what g_op_system_info returns.
		ShiftKeyState async_shift_keys = SHIFTKEY_NONE;
		if(HIBYTE(GetAsyncKeyState(VK_SHIFT)))
			async_shift_keys |= SHIFTKEY_SHIFT;
		if(HIBYTE(GetAsyncKeyState(VK_MENU)))
			async_shift_keys |= SHIFTKEY_ALT;
		if(HIBYTE(GetAsyncKeyState(VK_CONTROL)))
			async_shift_keys |= SHIFTKEY_CTRL;

		OpKey::Code key = ConvertKey_VK_To_OP(pMsg->wParam);

#if 0
#ifdef _DEBUG
		OutputKeyMessage(pMsg);
#endif // _DEBUG
#endif
		BOOL ignore = FALSE;

#ifdef WIDGETS_IME_SUPPORT
		// ===== Some Input Method hacks =============================

		static BOOL ctrl_backspace_undo = FALSE;

		// IMEs in WinME send a keydown with char 229 when ctrl+backspace is pressed. We must ignore this event since we don't want it.
		if (pMsg->message == WM_KEYDOWN && (async_shift_keys & SHIFTKEY_CTRL) && key == 229)
			ignore = TRUE;

		extern BOOL compose_started;
		/* NOTE: do want keydown + up delivered during IME composition (but not
		   keypresses.) */
		if (pMsg->message == WM_KEYDOWN && key == VK_BACK && (async_shift_keys & SHIFTKEY_CTRL) && g_input_manager)
		{
			// Japanese IMEs allow the user to ctrl+backspace to start composing the previous submitted string.
			// Windows sends backspaces for each character that should be removed in the previous string. (The IME somehow caches that)
			// This hack bypasses the inputmanager because it screws up the order of IME messages and keymessages.
			//
			// I haven't found any documented way to know if this ctrl+backspace will start to undo the IME.
			// These 2 ways work with Microsofts IME and ATOK.
			//
			// emil@opera.com
			MSG peekmsg;
			BOOL waiting_compose_start2 = ::PeekMessage(&peekmsg, NULL, WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION, PM_NOREMOVE|PM_NOYIELD);
			BOOL waiting_compose_start1 = ::PeekMessage(&peekmsg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE|PM_NOYIELD);
			if((waiting_compose_start1 && peekmsg.wParam == VK_BACK) || waiting_compose_start2)
			{
				ctrl_backspace_undo = TRUE;
				OpInputContext* ic = g_input_manager->GetKeyboardInputContext();
				if (ic && ic->GetInputContextName())
					if (op_strcmp(ic->GetInputContextName(), "Edit Widget") == 0)
					{
						OpInputAction action(OpInputAction::ACTION_BACKSPACE);
						ic->OnInputAction(&action);
					}
				ignore = TRUE;
			}
			else if (ctrl_backspace_undo)
			{
				OpInputContext* ic = g_input_manager->GetKeyboardInputContext();
				if (ic && ic->GetInputContextName())
					if (op_strcmp(ic->GetInputContextName(), "Edit Widget") == 0)
					{
						OpInputAction action(OpInputAction::ACTION_BACKSPACE);
						ic->OnInputAction(&action);
					}
				ignore = TRUE;
			}
		}

		if (pMsg->message == WM_KEYDOWN && key == VK_BACK && !(async_shift_keys & SHIFTKEY_CTRL))
			ctrl_backspace_undo = FALSE;

#endif  // WIDGETS_IME_SUPPORT
		// ===========================================================

		char *key_value = NULL;
		int key_value_length = 0;
		OpKey::Location location = GetKeyLocation(pMsg->wParam, pMsg->lParam);
		OpStringS8 str;

		if (pMsg->message == WM_KEYDOWN && g_drag_manager && g_drag_manager->IsDragging())
		{
			WindowsOpWindow *window = WindowsOpWindow::FindWindowUnderCursor();
			if(window && window->m_mde_screen)
			{
				//Esc stops dragging
				if(key == OP_KEY_ESCAPE)
				{
					window->m_mde_screen->TrigDragCancel(g_op_system_info->GetShiftKeyState());
				}
				else
				{
					// if a modifier has changed, core needs to know about it 
					// during drag so the cursor can be updated
					window->m_mde_screen->TrigDragUpdate();
				}
			}
		}

		//We ignore the input if it's not for any of our windows
		ignore = ignore || (!WindowsOpWindow::GetWindowFromHWND(pMsg->hwnd)
#ifdef EMBROWSER_SUPPORT
//							&& IsPluginWnd(pMsg->hwnd, TRUE)
							&& !WindowsOpWindow::GetAdoptedWindowFromHWND(pMsg->hwnd)
#endif // EMBROWSER_SUPPORT
						);

		OpPlatformKeyEventData *key_event_data = NULL;

		if (!ignore && g_input_manager)
		{
			//Happens when a key is pressed or held pressed down. Invoke keydown for both.
			if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
			{
				//Attempt to get the corresponding WM_(SYS)(DEAD)CHAR
				called_translate = TRUE;
				MSG translated_message;
				if (TranslateMessage(pMsg) &&
					(PeekMessage(&translated_message, pMsg->hwnd, WM_CHAR, WM_DEADCHAR, PM_REMOVE) ||
					PeekMessage(&translated_message, pMsg->hwnd, WM_SYSCHAR, WM_SYSDEADCHAR, PM_REMOVE)) &&
					//Skip control char generated by ctrl+symbol key combination. It's very unlikely for them to be used
					//for anything useful and they screw shortcut handling.
					((shift_keys & (SHIFTKEY_CTRL | SHIFTKEY_ALT)) != SHIFTKEY_CTRL || translated_message.wParam <= 0x1A))
				{

					//Once we enter here, we are going to handle one or more WM_(SYS)(DEAD)CHAR instead. We dispatch the
					//previous message and make sure to dispatch the translated message if relevant, then always return TRUE
					DispatchMessage(pMsg);

					do
					{
						uni_char value_buffer[2] = {0};
						value_buffer[0] = static_cast<uni_char>(translated_message.wParam);

						char simple_value;
						OpString8 str;

						if (key == OP_KEY_BACKSPACE || key == OP_KEY_ESCAPE)
							key_value_length = 0;
						else if (translated_message.wParam < 0x80)
						{
							simple_value = static_cast<char>(translated_message.wParam);
							key_value = &simple_value;
							key_value_length = 1;
						}
						else if (OpStatus::IsError(str.SetUTF8FromUTF16(value_buffer)))
						{
							DispatchMessage(&translated_message);
							return TRUE;
						}
						else
						{
							key_value = str.CStr();
							key_value_length = str.Length();
						}
						if (compose_started)
							key = OP_KEY_PROCESSKEY;

						HWND old_focus = GetFocus();
						BOOL handled = FALSE;

						if (OpStatus::IsError(OpPlatformKeyEventData::Create(&key_event_data, pMsg)))
						{
							DispatchMessage(&translated_message);
							return TRUE;
						}

						handled = g_input_manager->InvokeKeyDown(key, key_value, key_value_length, key_event_data, shift_keys, (translated_message.lParam & 0xffff) > 1, location, TRUE);
						if (translated_message.message != WM_DEADCHAR && translated_message.message != WM_SYSDEADCHAR)
							handled = g_input_manager->InvokeKeyPressed(key, key_value, key_value_length, shift_keys, (translated_message.lParam & 0xffff) > 1, TRUE);

						OpPlatformKeyEventData::DecRef(key_event_data);

						AddKeyValue(pMsg->wParam, translated_message.wParam);

						//We do not dispatch if a shortcut was handled or if the focus was changed
						ShiftKeyState ctrl_alt_state = shift_keys & (SHIFTKEY_ALT | SHIFTKEY_CTRL);
						if (((ctrl_alt_state != SHIFTKEY_ALT && ctrl_alt_state != SHIFTKEY_CTRL) || !handled) && old_focus == GetFocus())
							DispatchMessage(&translated_message);

						//Always dispatch WM_SYS_(DEAD)CHAR messages.
						//This is because core tends to be overeager in returning TRUE from InvokeKeyDown/InvokeKeyPressed
						else if (translated_message.message == WM_SYSCHAR || translated_message.message == WM_SYSDEADCHAR)
							DispatchMessage(&translated_message);
					}
					while (PeekMessage(&translated_message, pMsg->hwnd, WM_CHAR, WM_DEADCHAR, PM_REMOVE) ||
						PeekMessage(&translated_message, pMsg->hwnd, WM_SYSCHAR, WM_SYSDEADCHAR, PM_REMOVE));

					return TRUE;
				}
				//(julienp) This is to support the (apparently standard) behavior on windows that
				//Pressing Ctrl+ one of the shift keys, then releasing any of them switches to
				//RTL or LTR (depeding on which shift key)
				//When pressing Shift while Ctrl is being held, we record that we should be ready
				//to change direction (and which direction). If any other key should be held down
				//after that, we cancel this.
				s_new_rtl_direction = OpKey::LOCATION_STANDARD;
				if (m_use_RTL_shortcut && key == OP_KEY_SHIFT && async_shift_keys == (SHIFTKEY_SHIFT | SHIFTKEY_CTRL))
				{
					s_new_rtl_direction = location;
				}

				key_value = NULL;
				key_value_length = 0;
				OpString8 str;
				if (!OpKey::IsFunctionKey(key) && !OpKey::IsModifier(key) && (key < OP_KEY_A || key > OP_KEY_Z) &&
					(key < OP_KEY_NUMPAD0 || key > OP_KEY_NUMPAD9 || shift_keys != SHIFTKEY_ALT))
				{
					uni_char converted[4];
					byte vk_state[256];
					op_memset(vk_state, 0, sizeof(vk_state));
					vk_state[key] = 0x80;
					if (shift_keys & SHIFTKEY_SHIFT)
						vk_state[VK_SHIFT] = 0x80;
					if (key_value_length = ToUnicode(key, 0, vk_state, converted, 4, 0))
					{
						str.SetUTF8FromUTF16(converted, key_value_length);
						key_value = str.CStr();
					}
				}


				if (OpStatus::IsError(OpPlatformKeyEventData::Create(&key_event_data, pMsg)))
					return FALSE;

				g_input_manager->InvokeKeyDown(key, key_value, key_value_length, key_event_data, shift_keys, IsKeyRepeat(pMsg->lParam), location, TRUE);
				BOOL handled = g_input_manager->InvokeKeyPressed(key, key_value, key_value_length, shift_keys, IsKeyRepeat(pMsg->lParam), TRUE);

				OpPlatformKeyEventData::DecRef(key_event_data);

				//To prevent activating the window menu, return true if the F10 key was handled.
				if (handled && key == OP_KEY_F10)
					return TRUE;
			}

			//WM_CHAR that isn't associated to any WM_KEYDOWN (which can happen with e.g. Alt+numbers combinations)
			else if (pMsg->message == WM_CHAR || pMsg->message == WM_DEADCHAR)
			{
				key = OP_KEY_UNICODE;
				location = OpKey::LOCATION_STANDARD;

				uni_char value_buffer[2] = {0};
				value_buffer[0] = static_cast<uni_char>(pMsg->wParam);

				char simple_value;
				OpString8 str;

				if (pMsg->wParam < 0x80)
				{
					simple_value = static_cast<char>(pMsg->wParam);
					key_value = &simple_value;
					key_value_length = 1;
				}
				else if (OpStatus::IsError(str.SetUTF8FromUTF16(value_buffer)))
					return FALSE;
				else
				{
					key_value = str.CStr();
					key_value_length = str.Length();
				}

				if (compose_started)
					key = OP_KEY_PROCESSKEY;

				HWND old_focus = GetFocus();
				BOOL handled = FALSE;

				if (pMsg->message == WM_DEADCHAR)
					handled = g_input_manager->InvokeKeyDown(key, key_value, key_value_length, NULL, shift_keys, (pMsg->lParam & 0xffff) > 1, location, TRUE);
				else
					handled = g_input_manager->InvokeKeyPressed(key, key_value, key_value_length, shift_keys, (pMsg->lParam & 0xffff) > 1, TRUE);

				//We return true if a shortcut was handled or if the focus was changed
				ShiftKeyState ctrl_alt_state = shift_keys & (SHIFTKEY_ALT | SHIFTKEY_CTRL);
				if (((ctrl_alt_state == SHIFTKEY_ALT || ctrl_alt_state == SHIFTKEY_CTRL) && handled) || old_focus == GetFocus())
					return TRUE;
			}

			//Happens when a key is released, we invoke key up
			if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
			{
				WPARAM last_char_wParam = 0;
				if (HasCharacterValue(key, shift_keys, pMsg->wParam, pMsg->lParam))
					LookupKeyCharValue(pMsg->wParam, last_char_wParam);

				uni_char value_buffer[2] = {0};
				value_buffer[0] = static_cast<uni_char>(last_char_wParam);

				char simple_value;
				OpString8 str;

				if (last_char_wParam == 0)
				{
					key_value = NULL;
					key_value_length = 0;
				}
				else if (last_char_wParam < 0x80)
				{
					simple_value = static_cast<char>(last_char_wParam);
					key_value = &simple_value;
					key_value_length = 1;
				}
				else if (OpStatus::IsError(str.SetUTF8FromUTF16(value_buffer)))
					return FALSE;
				else
				{
					key_value = str.CStr();
					key_value_length = str.Length();
				}

				if (OpStatus::IsError(OpPlatformKeyEventData::Create(&key_event_data, pMsg)))
					return FALSE;

				g_input_manager->InvokeKeyUp(key, key_value, key_value_length, key_event_data, shift_keys, FALSE, location, TRUE);

				OpPlatformKeyEventData::DecRef(key_event_data);

				//Second part of the RTL support mentioned in WMSYSKEYDOWN
				//When releasing Ctrl or shift, if we recorded earlier that we are preparing a
				//direction change, do it. Then make sure we unregister the fact that we are ready.
				if (s_new_rtl_direction != OpKey::LOCATION_STANDARD && (key == OP_KEY_SHIFT || key == OP_KEY_CTRL))
				{
					if (s_new_rtl_direction == OpKey::LOCATION_LEFT)
						g_input_manager->InvokeAction(OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR);
					else
						g_input_manager->InvokeAction(OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL);
				}
				s_new_rtl_direction = OpKey::LOCATION_STANDARD;
			}
#if defined(MDE_DEBUG_INFO)
			// Enable/disable debugging of invalidation rectangles.
			else if (key == OP_KEY_F12 && shift_keys == (SHIFTKEY_SHIFT | SHIFTKEY_CTRL))
			{
				BrowserDesktopWindow *bw = g_application->GetActiveBrowserDesktopWindow();
				if (bw && bw->GetOpWindow())
				{
					MDE_Screen *screen = ((MDE_OpWindow*)bw->GetOpWindow())->GetMDEWidget()->m_screen;
					int flags = screen->m_debug_flags;
					if (screen->m_debug_flags & MDE_DEBUG_INVALIDATE_RECT)
						flags &= ~MDE_DEBUG_INVALIDATE_RECT;
					else
						flags |= MDE_DEBUG_INVALIDATE_RECT;
					screen->SetDebugInfo(flags);
				}
			}
#endif // defined(MDE_DEBUG_INFO)

		}
	}

	// close popup if clicking outside it
	if (g_input_manager && WindowsOpWindow::s_popup_window && (pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_NCLBUTTONDOWN || pMsg->message == WM_ACTIVATE))
	{
		if (WindowsOpWindow::s_popup_window->m_hwnd != pMsg->hwnd)
		{
			HWND hWndNextWindow = NULL;
			if (pMsg->message == WM_ACTIVATE)
				hWndNextWindow = LOWORD(pMsg->wParam) == WA_INACTIVE ? (HWND)pMsg->lParam : pMsg->hwnd;
			WindowsOpWindow::ClosePopup(hWndNextWindow);
			g_input_manager->ResetInput();
		}
	}

	if(pMsg->message == g_IntelliMouse_MsgMouseWheel)
	{
		IntelliMouse_TranslateWheelMessage(pMsg);
	}

	if (pMsg->message == WM_MOUSEWHEEL)
	{
		short	zDelta	= (short) HIWORD(pMsg->wParam);   //	Wheel rotation

		if (g_input_manager && g_input_manager->IsKeyDown(g_input_manager->GetGestureButton()))
		{
			g_input_manager->SetFlipping(TRUE);
			g_input_manager->InvokeAction(zDelta >= 0 ? OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE :
														OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE);
			return TRUE;
		}
	}

	// handle app command
	if (pMsg->message == WM_APPCOMMAND)
	{
		if (OnAppCommand(pMsg->lParam))
			return TRUE;
	}

	// hide task manager dialog if needed
	if (KioskManager::GetInstance()->GetNoExit())
	{
		HWND task_manager = FindWindow(UNI_L("#32770"), NULL);
		if (task_manager)
		{
			// check that the title matches "Windows Task Manager", or otherwise verify again
			// since "iTunes Helper" and a few other apps are known to use "#32770" as class name too.
			OpString window_title;
			const int TITLE_LEN = 21;

			window_title.Reserve(TITLE_LEN);
			GetWindowText(task_manager, window_title.CStr(), TITLE_LEN);

			if (window_title.Compare("Windows Task Manager") == 0)
			{
				ShowWindow(task_manager, SW_MINIMIZE);
				EnableWindow(task_manager, FALSE);
			}
		}
	}

	return FALSE;
}

/***********************************************************************************
**
**	SSLSeedFromMessageLoop
**
***********************************************************************************/

void WindowsOpMessageLoop::SSLSeedFromMessageLoop(const MSG *msg)
{
	extern void SSL_Process_Feeder();

	UINT message = msg->message;
	if (SSL_RND_feeder_len)
	{
		if (message == WM_MOUSEMOVE || message == WM_KEYDOWN)
		{
			if(SSL_RND_feeder_pos +2 > SSL_RND_feeder_len)
				SSL_Process_Feeder();

			if (message == WM_MOUSEMOVE)
			{
				SSL_RND_feeder_data[SSL_RND_feeder_pos++] = msg->lParam;
			}
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = GetTickCount();
		}
		// solve bug #298947
		else if(SSL_RND_feeder_pos == 0)
		{
			// would be nice to use rand_s here, but that depends on the RtlGenRandom API,
			// which is only available in Windows XP and later, and will simply crash the process
			// if it can't import that (thanks MS!)
			// no srand(time) in this fallback, it's seeded in other places already, e.g. OperaEntry(),
			// and seeding with current time here when we already used GetTickCount() is not clever
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = (unsigned)rand() << 17 ^ rand() << 7 ^ rand();
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = GetTickCount();
		}
	}
}

/***********************************************************************************
**
**	OnAppCommand
**
***********************************************************************************/

#define APPCOMMAND_BROWSER_BACKWARD       1
#define APPCOMMAND_BROWSER_FORWARD        2
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_BROWSER_STOP           4
#define APPCOMMAND_BROWSER_SEARCH         5
#define APPCOMMAND_BROWSER_FAVORITES      6
#define APPCOMMAND_BROWSER_HOME           7
#define APPCOMMAND_VOLUME_MUTE            8
#define APPCOMMAND_VOLUME_DOWN            9
#define APPCOMMAND_VOLUME_UP              10
#define APPCOMMAND_MEDIA_NEXTTRACK        11
#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
#define APPCOMMAND_MEDIA_STOP             13
#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
#define APPCOMMAND_LAUNCH_MAIL            15
#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
#define APPCOMMAND_LAUNCH_APP1            17
#define APPCOMMAND_LAUNCH_APP2            18
#define APPCOMMAND_BASS_DOWN              19
#define APPCOMMAND_BASS_BOOST             20
#define APPCOMMAND_BASS_UP                21
#define APPCOMMAND_TREBLE_DOWN            22
#define APPCOMMAND_TREBLE_UP              23
#define APPCOMMAND_MICROPHONE_VOLUME_MUTE 24
#define APPCOMMAND_MICROPHONE_VOLUME_DOWN 25
#define APPCOMMAND_MICROPHONE_VOLUME_UP   26
#define APPCOMMAND_HELP                   27
#define APPCOMMAND_FIND                   28
#define APPCOMMAND_NEW                    29
#define APPCOMMAND_OPEN                   30
#define APPCOMMAND_CLOSE                  31
#define APPCOMMAND_SAVE                   32
#define APPCOMMAND_PRINT                  33
#define APPCOMMAND_UNDO                   34
#define APPCOMMAND_REDO                   35
#define APPCOMMAND_COPY                   36
#define APPCOMMAND_CUT                    37
#define APPCOMMAND_PASTE                  38
#define APPCOMMAND_REPLY_TO_MAIL          39
#define APPCOMMAND_FORWARD_MAIL           40
#define APPCOMMAND_SEND_MAIL              41
#define APPCOMMAND_SPELL_CHECK            42
#define APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE    43
#define APPCOMMAND_MIC_ON_OFF_TOGGLE      44
#define APPCOMMAND_CORRECTION_LIST        45
#define APPCOMMAND_MEDIA_PLAY             46
#define APPCOMMAND_MEDIA_PAUSE            47
#define APPCOMMAND_MEDIA_RECORD           48
#define APPCOMMAND_MEDIA_FAST_FORWARD     49
#define APPCOMMAND_MEDIA_REWIND           50
#define APPCOMMAND_MEDIA_CHANNEL_UP       51
#define APPCOMMAND_MEDIA_CHANNEL_DOWN     52

BOOL WindowsOpMessageLoop::OnAppCommand(UINT32 lParam)
{
	INT32 command		= GET_APPCOMMAND_LPARAM(lParam);
	BOOL mouse			= GET_DEVICE_LPARAM(lParam) & FAPPCOMMAND_MOUSE;

	OpInputAction::Action action = OpInputAction::ACTION_UNKNOWN;
	INTPTR action_data = 0;

	// try pause first for this special double app command

	switch (command)
	{
		case APPCOMMAND_BROWSER_BACKWARD:		action = OpInputAction::ACTION_BACK; break;
		case APPCOMMAND_BROWSER_FORWARD:		action = OpInputAction::ACTION_FORWARD; break;
		case APPCOMMAND_BROWSER_FAVORITES:		action = OpInputAction::ACTION_ACTIVATE_HOTLIST_WINDOW; break;
		case APPCOMMAND_BROWSER_HOME:			action = OpInputAction::ACTION_GO_TO_HOMEPAGE; break;
		case APPCOMMAND_BROWSER_REFRESH:		action = OpInputAction::ACTION_RELOAD; break;
		case APPCOMMAND_BROWSER_STOP:			action = OpInputAction::ACTION_STOP; break;
		case APPCOMMAND_BROWSER_SEARCH:			action = OpInputAction::ACTION_FOCUS_SEARCH_FIELD; break;
		case APPCOMMAND_COPY:					action = OpInputAction::ACTION_COPY; break;
		case APPCOMMAND_FIND:					action = OpInputAction::ACTION_FIND; break;
		case APPCOMMAND_UNDO:					action = OpInputAction::ACTION_UNDO; break;
		case APPCOMMAND_REDO:					action = OpInputAction::ACTION_REDO; break;
		case APPCOMMAND_CUT:					action = OpInputAction::ACTION_CUT; break;
		case APPCOMMAND_PASTE:					action = OpInputAction::ACTION_PASTE; break;
		case APPCOMMAND_HELP:					action = OpInputAction::ACTION_SHOW_HELP; break;
		case APPCOMMAND_NEW:					action = OpInputAction::ACTION_NEW_PAGE; break;
		case APPCOMMAND_OPEN:					action = OpInputAction::ACTION_OPEN_DOCUMENT; break;
		case APPCOMMAND_CLOSE:					action = OpInputAction::ACTION_CLOSE_PAGE; break;
		case APPCOMMAND_PRINT:					action = OpInputAction::ACTION_PRINT_DOCUMENT; break;
		case APPCOMMAND_SAVE:					action = OpInputAction::ACTION_SAVE_DOCUMENT_AS; break;

#ifdef HAS_REMOTE_CONTROL

		case APPCOMMAND_VOLUME_MUTE:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_MUTE, 0);
			}
			break;

		case APPCOMMAND_MEDIA_NEXTTRACK:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_NEXT, 0);
			}
			break;

		case APPCOMMAND_MEDIA_PREVIOUSTRACK:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_PREVIOUS, 0);
			}
			break;

		case APPCOMMAND_MEDIA_STOP:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_STOP, 0);
			}
			break;

		case APPCOMMAND_MEDIA_PLAY:
		case APPCOMMAND_MEDIA_PLAY_PAUSE:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_PLAY, 0);
			}
			break;

		case APPCOMMAND_MEDIA_PAUSE:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_PAUSE, 0);
			}
			break;

		case APPCOMMAND_MEDIA_FAST_FORWARD:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_FASTFORWARD, 0);
			}
			break;

		case APPCOMMAND_MEDIA_REWIND:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_REWIND, 0);
			}
			break;

		case APPCOMMAND_MEDIA_RECORD:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_RECORD, 0);
			}
			break;

		case APPCOMMAND_MEDIA_CHANNEL_UP:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_CHANNELUP, 0);
			}
			break;


		case APPCOMMAND_MEDIA_CHANNEL_DOWN:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_CHANNELDOWN, 0);
			}
			break;

		case APPCOMMAND_VOLUME_UP:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_VOLUMEUP, 0);
			}
			break;

		case APPCOMMAND_VOLUME_DOWN:
			{
				g_input_manager->InvokeKeyPressed(OP_KEY_VOLUMEDOWN, 0);
			}
			break;
#endif // HAS_REMOTE_CONTROL

#ifdef M2_SUPPORT
		case APPCOMMAND_LAUNCH_MAIL:			action = OpInputAction::ACTION_READ_MAIL; break;
		case APPCOMMAND_REPLY_TO_MAIL:			action = OpInputAction::ACTION_REPLY; break;
		case APPCOMMAND_FORWARD_MAIL:			action = OpInputAction::ACTION_FORWARD_MAIL; break;
		case APPCOMMAND_SEND_MAIL:				action = OpInputAction::ACTION_SEND_MAIL; break;
#endif
	}

	if (action != OpInputAction::ACTION_UNKNOWN)
	{
		// Performing this action immediately might lead to Opera hanging / crashing (see bug #193727).
		// Could be because we potentially end up here through DispatchMessage() in Run(), and when the
		// plugin tries to destroy itself it does things that it shouldn't do at the wrong time, or
		// something. Either way, we solve that by remembering the app command to invoke, and then
		// invoke it at the end of Run() (when we're well out of DispatchMessage()).
		OP_ASSERT(m_pending_app_command == NULL);
		m_pending_app_command = OP_NEW(AppCommandInfo, (action, mouse ? g_input_manager->GetMouseInputContext() : g_input_manager->GetKeyboardInputContext(), action_data));

		if (m_pending_app_command == NULL)
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
**
**	ConvertKey_VK_To_OP
**
***********************************************************************************/

OpKey::Code WindowsOpMessageLoop::ConvertKey_VK_To_OP(UINT32 vk_key)
{
	/* The Windows virtual key mapping is now equal to the keycodes
	   in OpKey. Consistent values across platforms..and a simpler
	   mapping function for this particular one. */
	return static_cast<OpKey::Code>(vk_key);
}

