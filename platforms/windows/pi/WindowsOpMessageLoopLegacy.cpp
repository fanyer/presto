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

#include "platforms/windows/pi/WindowsOpMessageLoopLegacy.h"

#include "adjunct/quick/managers/kioskmanager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#include "modules/ns4plugins/src/plugin.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "platforms/windows/user.h"
#include "platforms/windows/pi/WindowsOpPluginWindow.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/IntMouse.h"

/*Supported and defined in Windows Vista SDK*/
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif //WM_MOUSEHWHEEL

#define WM_APPCOMMAND					  0x0319
#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_MASK  0xF000
#define GET_APPCOMMAND_LPARAM(lParam)	((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define GET_DEVICE_LPARAM(lParam)		((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))

HWND WindowsOpMessageLoop::s_hwnd = NULL;
WindowsOpMessageLoop* g_windows_message_loop = NULL;

#ifdef _FALLBACK_SHORTCUT_SUPPORT_
static int g_win32_unshifted_key = 0;
int GetNonShiftedKey(int shifted_key, int state) {return g_win32_unshifted_key;}
#endif //_FALLBACK_SHORTCUT_SUPPORT_

WindowsOpMessageLoop::WindowsOpMessageLoop() :
	m_core_has_message_waiting(FALSE),
	m_message_posted(FALSE),
	m_stop_after_timeout(0),
	m_pending_app_command(NULL),
	m_timeout(FALSE),
	m_use_RTL_shortcut(FALSE),
	/* Support for "new" key events. */
	m_rshift_scancode(0),
	m_keyvalue_vk_table(NULL),
	m_keyvalue_table(NULL),
	m_keyvalue_table_index(0),
	m_timer_queue(NULL),
	m_timer_queue_timer(NULL),
	m_msg_delay_handler_registered(FALSE),
	m_is_processing_silverlight_message(0),
	m_exiting(FALSE),
	m_exiting_timestamp(0),
	m_timer(this)
{
	InitializeCriticalSection(&m_post_lock);
	g_sleep_manager->SetStayAwakeListener(this);
}

/***********************************************************************************
**
**	~WindowsOpMessageLoop
**
***********************************************************************************/

WindowsOpMessageLoop::~WindowsOpMessageLoop()
{
	OP_DELETE(m_pending_app_command);

	if (s_hwnd)
	{
		::DestroyWindow(s_hwnd);
		s_hwnd = NULL;
		Unregister();
	}

	DeleteCriticalSection(&m_post_lock);

	g_sleep_manager->SetStayAwakeListener(NULL);

	OP_DELETEA(m_keyvalue_vk_table);
	OP_DELETEA(m_keyvalue_table);

}

/***********************************************************************************
**
**	Unregister
**
***********************************************************************************/


void WindowsOpMessageLoop::Unregister()
{
	::UnregisterClass(UNI_L("OPMESSAGEWINDOW"), NULL);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::Init()
{
	RETURN_IF_ERROR(WindowsOpComponentPlatform::Init());

	if (s_hwnd)
	{
		// there can only exist one WindowsOpMessageLoop
		// (this could easily be changed, but then some code
		// must be changed)

		return OpStatus::ERR;
	}

	WNDCLASS wndClass;
	wndClass.style			= 0;
	wndClass.lpfnWndProc	= (WNDPROC)WndProc;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= 0;
	wndClass.hInstance		= NULL;
	wndClass.hIcon			= 0;
	wndClass.hCursor		= NULL;
    wndClass.hbrBackground	= NULL;
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= UNI_L("OPMESSAGEWINDOW");

	{
		OP_PROFILE_METHOD("Registered message loop class");

		::RegisterClass(&wndClass);
	}
	{
		OP_PROFILE_METHOD("Created message loop window");

		s_hwnd = ::CreateWindow(wndClass.lpszClassName,
							NULL,				//  window caption
							WS_POPUP,			//  style
							0, 0, 0, 0,			//  pos, size
							NULL,				//  parent handle
							NULL,				//  menu or child ID
							NULL,				//  instance
							NULL);				//  additional info

		if (s_hwnd == 0)
		{
			Unregister();
			return OpStatus::ERR_NO_MEMORY;
		}
		::SetWindowLongPtr(s_hwnd, GWLP_USERDATA, (LONG_PTR)this);
	}
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

void WindowsOpMessageLoop::OnTimeOut(WindowsTimer*)
{
	if (!m_message_posted)
	{
		m_message_posted = TRUE;
		::PostMessage(s_hwnd, WM_OPERA_DISPATCH_POSTED_MESSAGE, 0, 0);
	}
}

/***********************************************************************************
**
**	WndProc - WM_TIMER only arrives on Win9x, other platforms gets their timers in the callback
**			above
**
***********************************************************************************/

LRESULT CALLBACK WindowsOpMessageLoop::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_OPERA_DISPATCH_POSTED_MESSAGE:
		case WM_TIMER:
		{
			WindowsOpMessageLoop* msgloop = (WindowsOpMessageLoop*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			if (msgloop)
			{
				msgloop->DispatchPostedMessage();
			}
			return 0;
		}
		default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
}

void WindowsOpMessageLoop::StopTimer()
{
	m_timer.Stop();
}

void WindowsOpMessageLoop::StartTimer(int delay)
{
	m_timer.Start(delay);
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
	m_timeout = FALSE;

	BOOL ret;
	MSG msg;

	while ( (ret = ::GetMessage(&msg, 0, 0, 0)) != 0)
	{
		// Forcefully break out if m_exiting has been set to TRUE(PostQuitMessage 
		// has already been called) for more than 3 seconds and there is no pending 
		// core message to process. Note core timer doesn't count as a message.(DSK-336892)
		if ( ret == -1 || GetIsExiting() && !m_core_has_message_waiting && GetTickCount() - m_exiting_timestamp > 3000/*ms*/)
			break;

		if (msg.message == 0x401 && msg.wParam == 0xDEADBEEF)
		{
			// Flash message which was delayed and will be reposted - discard
			continue;
		}


#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
		SSLSeedFromMessageLoop(&msg);
#endif

		switch (msg.message)
		{
			case WM_TIMER:
			case 0x401:
			case 0x402:
			case 0x403:
			case 0x404:
			case 0x405:
			case 0x406:
			case 0x407:
			case 0x408:
			case 0x409:
			case 0x40A:
			case 0x40B:
			case 0x40C:
			case 0x40D:
			case 0x40E:
			case 0x40F:
			case 0x410:
			case 0x411:
			{
				char class_name[16];
				if (GetClassNameA(msg.hwnd, class_name, 16) == ARRAY_SIZE("XCPTimerClass")-1 &&
					!op_strcmp(class_name, "XCPTimerClass"))
				{
					// this is a message for the Silverlight plugin
					// it crashes on nested messages, see bug DSK-243354
					if (m_is_processing_silverlight_message || m_silverlight_messages.GetCount())
					{
						MSG *silverlight_msg = new MSG(msg);
						if (silverlight_msg)
						{
							m_silverlight_messages.Add(silverlight_msg);
							if (!m_msg_delay_handler_registered)
							{
								// can't do this in constructor, the message loop is initialised before g_main_message_handler
								g_main_message_handler->SetCallBack(this, DELAYED_FLASH_MESSAGE, (MH_PARAM_1)this);
								m_msg_delay_handler_registered = TRUE;
							}
							g_main_message_handler->PostMessage(DELAYED_FLASH_MESSAGE, (MH_PARAM_1)this, (MH_PARAM_2)silverlight_msg, 1);
						}
					}
					else
					{
						m_is_processing_silverlight_message++;
						BOOL flag = g_pluginscriptdata->AllowNestedMessageLoop();
						g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);
						DispatchMessage(&msg);
						g_pluginscriptdata->SetAllowNestedMessageLoop(flag);
						m_is_processing_silverlight_message--;
					}
					break;
				}
				//fallthrough
			}
			default:
			{
				BOOL called_translate;
				if (!TranslateOperaMessage(&msg, called_translate))
				{
					// Nothing ate the message, so translate and dispatch it

					if (!called_translate)
						TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}

		if (!GetIsExiting())
		{
			PostMessageIfNeeded();
		}
		if ((m_stop_after_timeout && m_timeout) ||
			(wait_for_this_dialog_to_close && !m_dialog_hash_table.Contains(wait_for_this_dialog_to_close)))
		{
//			OutputDebugStringW(UNI_L("breaking out of main loop\n"));
			break;
		}
		if (m_pending_app_command)
		{
			g_input_manager->InvokeAction(m_pending_app_command->m_action, m_pending_app_command->m_action_data, NULL, m_pending_app_command->m_input_context);

			OP_DELETE(m_pending_app_command);
			m_pending_app_command = NULL;
		}
	}
	m_timeout = FALSE;

	return OpStatus::OK;
}

void WindowsOpMessageLoop::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == DELAYED_FLASH_MESSAGE)
	{
		if (!m_is_processing_silverlight_message)
		{
			m_is_processing_silverlight_message++;
			MSG *msg = m_silverlight_messages.Remove(0);
			BOOL flag = g_pluginscriptdata->AllowNestedMessageLoop();
			g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);
			DispatchMessage(msg);
			g_pluginscriptdata->SetAllowNestedMessageLoop(flag);
			delete msg;
			m_is_processing_silverlight_message--;
		}
		else
			g_main_message_handler->PostMessage(DELAYED_FLASH_MESSAGE, par1, par2, 1);
	}
}

/***********************************************************************************
**
**	PostMessageIfNeeded
**
***********************************************************************************/

void WindowsOpMessageLoop::PostMessageIfNeeded()
{
	EnterCriticalSection(&m_post_lock);
	PostMessageIfNeeded_Internal();
	LeaveCriticalSection(&m_post_lock);
}

// the internal version should only be called inside a protected critical section
void WindowsOpMessageLoop::PostMessageIfNeeded_Internal()
{
	MSG msg;

	if (m_core_has_message_waiting && !m_message_posted 
		// Fix for DSK-328543
		// Post a dispatch message when needed, but handle system message first 
		// so even when opera is busy we keep the UI responsive
		&& !::PeekMessage(&msg, NULL, 0, WM_OPERA_DISPATCH_POSTED_MESSAGE, PM_NOREMOVE))
	{
#ifdef _DEBUG
		BOOL res =
#endif
		::PostMessage(s_hwnd, WM_OPERA_DISPATCH_POSTED_MESSAGE, 0, 0);

		OP_ASSERT(res);

		m_message_posted = TRUE;
	}
}

/***********************************************************************************
**
**	OnReschedulingNeeded
**
***********************************************************************************/

void WindowsOpMessageLoop::RequestRunSlice(unsigned int limit)
{
	EnterCriticalSection(&m_post_lock);
	OnReschedulingNeeded_Internal(limit);
	LeaveCriticalSection(&m_post_lock);
}

// the internal version should only be called inside a protected critical section
void WindowsOpMessageLoop::OnReschedulingNeeded_Internal(int delay)
{
	StopTimer();
	m_core_has_message_waiting = (delay == 0);

	if (m_core_has_message_waiting)
	{
		PostMessageIfNeeded_Internal();
	}
	else if (delay > 0)
	{
		StartTimer(delay);
	}
}

/***********************************************************************************
**
**	RequestPeer
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type)
{
	OP_ASSERT (IsInitialProcess());

	return g_component_manager->HandlePeerRequest(requester, type);
}
 
/***********************************************************************************
**
**	ProcessEvents
**
***********************************************************************************/

OP_STATUS WindowsOpMessageLoop::ProcessEvents(unsigned int next_timeout, EventFlags flags)
{
	OnReschedulingNeeded_Internal(next_timeout);

	m_stop_after_timeout ++;
	OP_STATUS ret = Run(NULL);
	m_stop_after_timeout --;
	return ret;
}

/***********************************************************************************
**
**  OnStayAwakeStatusChanged
**
***********************************************************************************/

void WindowsOpMessageLoop::OnStayAwakeStatusChanged(BOOL stay_awake)
{
	SetThreadExecutionState(stay_awake ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED : ES_CONTINUOUS);
}

/***********************************************************************************
**
**	DispatchPostedMessage
**
***********************************************************************************/

void WindowsOpMessageLoop::DispatchPostedMessage()
{
	EnterCriticalSection(&m_post_lock);
	m_message_posted = FALSE;

	if (m_stop_after_timeout)
		m_timeout = TRUE;
	else if (g_component_manager)
	{
		// Quick fix for DSK-306701 HTML5 video, animated gif animations and downloads stop playing when context menu is open
		// When openning a menu in g_opera->Run() it initiates another message loop and
		// won't return as long as the menu is open. In this case we don't get the chance
		// to post the dispatch message thus all activity is halted. A simple solution is
		// to set a 50ms timer in advance to make sure we always have a pending timer to 
		// reenable the halted message loop. The downside is we may at most have 20 useless
		// timer event every second, which should be trivial. A alternate solution is to 
		// only setup a timer when we know we're going into an embedded message loop, but
		// then the fix will be scattered to several places.
		OnReschedulingNeeded_Internal(50);

		int delay;
		delay = g_component_manager->RunSlice();

		OnReschedulingNeeded_Internal(delay);
	}

	LeaveCriticalSection(&m_post_lock);
}

void WindowsOpMessageLoop::DispatchAllPostedMessagesNow()
{
	// assume there's pending core message(since we don't know)
	m_core_has_message_waiting = TRUE;
	while (m_core_has_message_waiting)
	{
		DispatchPostedMessage();
	}
}

void WindowsOpMessageLoop::SetIsExiting()
{
	m_exiting = TRUE;
	m_exiting_timestamp = GetTickCount();
}

#endif // !NS4P_COMPONENT_PLUGINS
