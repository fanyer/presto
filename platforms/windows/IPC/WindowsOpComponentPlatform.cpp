/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/opdata/UniString.h"
#include "modules/inputmanager/inputcontext.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include "platforms/windows/PluginWrapperMessageLoop.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows_common/pi_impl/WindowsOpPluginTranslator.h"

const uni_char* WindowsOpComponentPlatform::s_message_window_class_name = UNI_L("OpMessageOnlyWindow");

WindowsOpComponentPlatform::WindowsOpComponentPlatform()
	: m_shared_mem(NULL)
	, m_exit_loop(FALSE)
	, m_event_flags(PROCESS_ANY)
#ifdef NS4P_COMPONENT_PLUGINS
	, m_timer(this)
#endif // NS4P_COMPONENT_PLUGINS
	, m_ran_core(FALSE)
	, m_must_run_core(FALSE)
	, m_initial_process(NULL)
	, m_initial_process_wait(NULL)
	, m_sm_read_ready_wait(NULL)
	, m_message_window(NULL)
	, m_loop_return_value(OpStatus::OK)
{
	m_is_initial_process = g_component_manager->GetAddress().cm == 0;
	if (m_is_initial_process)
		WindowsSharedMemoryManager::SetCM0PID(GetCurrentProcessId());

	if (WindowsSharedMemoryManager::GetInstance())
		m_shared_mem = WindowsSharedMemoryManager::GetInstance()->GetMemory();

	op_memset(m_WM_listeners, 0, sizeof(m_WM_listeners));
}

WindowsOpComponentPlatform::~WindowsOpComponentPlatform()
{
	if (!m_is_initial_process)
	{
		UnregisterWait(m_initial_process_wait);
		CloseHandle(m_initial_process);
	}

	UnregisterWait(m_sm_read_ready_wait);

	if (m_message_window)
		DestroyWindow(m_message_window);
}

/*static*/
OP_STATUS WindowsOpComponentPlatform::RegisterWNDCLASS()
{
	WNDCLASS window_class = {};
	window_class.lpfnWndProc	= MessageWindowProc;
	window_class.lpszClassName	= s_message_window_class_name;

	if (!RegisterClass(&window_class))
		return OpStatus::ERR;

	return OpStatus::OK;
}

/*static*/
void WindowsOpComponentPlatform::UnregisterWNDCLASS()
{
	UnregisterClass(s_message_window_class_name, NULL);
}


/*static*/
double WindowsOpComponentPlatform::GetRuntimeMSImpl()
{
	static DWORD runtimems_last = timeGetTime();
	static double runtimems_wrap_compensation = 0;

	DWORD tick = timeGetTime();

	// Must check if we're wrapping
	if(tick < runtimems_last)
	{
		// wrap
		runtimems_wrap_compensation += (DWORD)-1;
	}
	runtimems_last = tick;

	return tick + runtimems_wrap_compensation; 
}

OP_STATUS WindowsOpComponentPlatform::SendMessage(OpTypedMessage* message)
{
	if (!WindowsSharedMemoryManager::GetInstance())
		return OpStatus::ERR_NO_MEMORY;

	WindowsSharedMemory* sm;
	RETURN_IF_ERROR(WindowsSharedMemoryManager::GetInstance()->OpenMemory(sm, message->GetDst()));

	return sm->WriteMessage(message);
}

#ifdef NS4P_COMPONENT_PLUGINS
/*static*/
OP_STATUS WindowsOpComponentPlatform::ProcessCommandLineRequest(WindowsOpComponentPlatform *&component_platform, const UniString& command_line)
{
	OpAutoPtr< OtlCountedList<UniString> > parts(command_line.Split(' '));

	//Make sure we'll be able to successfully call PopFirst 5 times (otherwise, command line was incorrect).
	if (parts->Length() != 5)
		return OpStatus::ERR;

	OpComponentType component_type;
	OpMessageAddress requester;

	int cm_0_pid;

	RETURN_IF_ERROR(parts->PopFirst().ToInt(&cm_0_pid));
	RETURN_IF_ERROR(parts->PopFirst().ToInt((int*)&component_type));
	RETURN_IF_ERROR(parts->PopFirst().ToInt(&requester.cm));
	RETURN_IF_ERROR(parts->PopFirst().ToInt(&requester.co));
	RETURN_IF_ERROR(parts->PopFirst().ToInt(&requester.ch));

	WindowsSharedMemoryManager::SetCM0PID(cm_0_pid);

	OpComponentManager* manager;
	RETURN_IF_ERROR(OpComponentManager::Create(&manager, GetCurrentProcessId()));

	g_component_manager = manager;

	OpAutoPtr<OpComponentManager> manager_autoptr(manager);

	OpAutoPtr<WindowsOpComponentPlatform> platform;

	switch (component_type)
	{
		case COMPONENT_PLUGIN:
		case COMPONENT_PLUGIN_WIN32:
#ifdef PLUGIN_WRAPPER
			platform.reset(OP_NEW(PluginWrapperMessageLoop, ()));
#endif //PLUGIN_WRAPPER
			break;

		case COMPONENT_LAST:
		default:
			platform.reset(OP_NEW(WindowsOpComponentPlatform, ()));
	}

	if (!platform.get())
		return OpStatus::ERR_NO_MEMORY;

	manager_autoptr.release()->SetPlatform(platform.get());

	RETURN_IF_ERROR(g_component_manager->HandlePeerRequest(requester, component_type));

	component_platform = platform.release();
	return OpStatus::OK;
}
#endif // NS4P_COMPONENT_PLUGINS

OP_STATUS WindowsOpComponentPlatform::Init()
{
	if (!m_shared_mem)
		return OpStatus::ERR;

	if ((m_message_window = CreateWindow(s_message_window_class_name, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL)) == NULL)
		return OpStatus::ERR;

	if (!m_is_initial_process)
	{
		m_initial_process = OpenProcess(SYNCHRONIZE, FALSE, WindowsSharedMemoryManager::GetCM0PID());
		if (!m_initial_process)
			return OpStatus::ERR;
		RegisterWaitForSingleObject(&m_initial_process_wait, m_initial_process, ReportDeadCM0, m_message_window, INFINITE, WT_EXECUTEONLYONCE);
	}

	RegisterWaitForSingleObject(&m_sm_read_ready_wait, m_shared_mem->GetNotEmptyEvent(), ReportReadReady, m_message_window, INFINITE, 0);

	SetWindowLongPtr(m_message_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	return OpStatus::OK;
}

#ifdef NS4P_COMPONENT_PLUGINS
OP_STATUS WindowsOpComponentPlatform::Run()
{
	return RunLoop(FALSE);
}

OP_STATUS WindowsOpComponentPlatform::RunLoop(BOOL once)
{
	if (m_exit_loop)
		return m_loop_return_value;

	m_loop_return_value = OpStatus::OK;

	OP_ASSERT (GetWindowThreadProcessId(m_message_window, NULL) == GetCurrentThreadId());
	do
	{
		m_ran_core = FALSE;

		DWORD ret;
		DWORD wake_mask = QS_ALLINPUT;
		if (m_event_flags == PROCESS_IPC_MESSAGES)
			wake_mask = QS_SENDMESSAGE;
		ret = MsgWaitForMultipleObjects(0, NULL, FALSE, INFINITE, wake_mask);

		if (ret == WAIT_FAILED)
			return OpStatus::ERR;

		else if (ret == WAIT_OBJECT_0)
		{
			MSG msg;
			UINT remove_msg = PM_REMOVE;
			if (m_event_flags == PROCESS_IPC_MESSAGES)
				remove_msg |= PM_QS_SENDMESSAGE;
			while(PeekMessage(&msg, NULL, 0, 0, remove_msg))
			{
				switch (msg.message)
				{
					case WM_QUIT:
						PostQuitMessage(msg.wParam);
						return OpStatus::OK;

					case WM_DELAYED_FOCUS_CHANGE:
						reinterpret_cast<OpInputContext*>(msg.wParam)->SetFocus(FOCUS_REASON_OTHER);
				}

				RETURN_IF_ERROR(ProcessMessage(msg));
			}
		}

		//julienp: If the timer fires about at the time we reach this, we might
		//be in a situation where we send WM_RUNSLICE twice, which means core can rarely
		//run twice in a row, which is pretty harmless and shouldn't impact performance.
		//So, I'm deliberately not protecting m_must_run_core and related code with a
		//critical section to avoid the (very small) cost in time involved.
		if (m_must_run_core)
		{
			m_must_run_core = FALSE;
			m_timer.Stop();
			//This will call the window procedure directly
			::SendMessage(m_message_window, WM_RUN_SLICE, 0, 0);
		}

		RETURN_IF_ERROR(DoLoopContent(m_ran_core));
	}
	while(!once && g_component_manager->GetComponentCount() && !m_exit_loop);

	return m_loop_return_value;
}

void WindowsOpComponentPlatform::RequestRunSlice(unsigned int limit)
{
	m_timer.Stop();
	if (limit == UINT_MAX)
		return;

	if (limit == 0)
	{
		//If possible, try to run core before the timeout occurs.
		//Disabled for plugin wrapper as it interferes with flash.
		//TODO: find a better fix for flash.
		m_must_run_core = TRUE;

		//Timers don't work with a timeout of 0
		limit = 1;
	}

	m_timer.Start(limit);
}

void WindowsOpComponentPlatform::OnTimeOut(class WindowsTimer* timer)
{
	OP_ASSERT(timer == &m_timer);

	m_must_run_core = FALSE;

	if (m_event_flags == PROCESS_IPC_MESSAGES)
	{
		//Using SendMessage here instead of PostMessage becaus if RunLoop is
		//called with PROCESS_IPC_MESSAGE, only sent messages are pumped anyway.
		::SendNotifyMessage(m_message_window, WM_RUN_SLICE, 0, 0);
	}
	else
		::PostMessage(m_message_window, WM_RUN_SLICE, 0, 0);
}

OP_STATUS WindowsOpComponentPlatform::ProcessEvents(unsigned int timeout, EventFlags flags)
{
	EventFlags event_flags = m_event_flags;
	m_event_flags = flags;
	RequestRunSlice(timeout);
	OP_STATUS ret = RunLoop(TRUE);
	m_event_flags = event_flags;
	return ret;
}
#endif // NS4P_COMPONENT_PLUGINS

/*static*/
VOID CALLBACK WindowsOpComponentPlatform::ReportReadReady(PVOID message_window, BOOLEAN timeout)
{
	HWND window = reinterpret_cast<HWND>(message_window);
	::SendMessage(window, WM_SM_READ_READY, 0, 0);
}

/*static*/
VOID CALLBACK WindowsOpComponentPlatform::ReportDeadCM0(PVOID message_window, BOOLEAN timeout)
{
	HWND window = reinterpret_cast<HWND>(message_window);
	SendNotifyMessage(window, WM_DEAD_CM0, 0, 0);
}

OP_STATUS WindowsOpComponentPlatform::SetWMListener(UINT message, WMListener* listener)
{
	if (message < FIRST_PSEUDO_THREAD_MESSAGE || message >= LAST_PSEUDO_THREAD_MESSAGE)
		return OpStatus::ERR;

	m_WM_listeners[message-FIRST_PSEUDO_THREAD_MESSAGE] = listener;

	return OpStatus::OK;
}

OP_STATUS WindowsOpComponentPlatform::RemoveWMListener(UINT message)
{
	if (message < FIRST_PSEUDO_THREAD_MESSAGE || message >= LAST_PSEUDO_THREAD_MESSAGE)
		return OpStatus::ERR;

	m_WM_listeners[message-FIRST_PSEUDO_THREAD_MESSAGE] = NULL;

	return OpStatus::OK;
}

/*static*/
LRESULT WindowsOpComponentPlatform::MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef NS4P_COMPONENT_PLUGINS
	static BOOL s_in_run_slice;
#endif // NS4P_COMPONENT_PLUGINS
	WindowsOpComponentPlatform* component_platform = reinterpret_cast<WindowsOpComponentPlatform*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (component_platform)
	{
#ifdef NS4P_COMPONENT_PLUGINS
		if (uMsg == WM_RUN_SLICE || uMsg == WM_FORCED_RUN_SLICE)
		{
			//If the call to RunSlice triggers the creation of a new message loop on the stack,
			//we need to make sure that a WM_RUNSLICE message will be sent to it at some point.

			if (uMsg == WM_FORCED_RUN_SLICE && !s_in_run_slice)
				return 0;

			::PostMessage(hwnd, WM_FORCED_RUN_SLICE, 0, 0);

			if (component_platform->m_event_flags != PROCESS_IPC_MESSAGES)
			{
				s_in_run_slice = TRUE;
				DWORD timeout = g_component_manager->RunSlice();
				s_in_run_slice = FALSE;

				component_platform->RequestRunSlice(timeout);
			}
			component_platform->m_ran_core = TRUE;
		}
#endif // NS4P_COMPONENT_PLUGINS

		if (uMsg == WM_DEAD_CM0)
		{
			g_component_manager->PeerGone(0);
			component_platform->m_exit_loop = TRUE;
			component_platform->m_loop_return_value = OpStatus::ERR;
		}

		if (uMsg == WM_SM_READ_READY)
		{
			component_platform->m_shared_mem->ReadMessages();
		}

		if (uMsg >= FIRST_PSEUDO_THREAD_MESSAGE && uMsg < LAST_PSEUDO_THREAD_MESSAGE)
		{
			WMListener* listener = component_platform->m_WM_listeners[uMsg-FIRST_PSEUDO_THREAD_MESSAGE];
			if (listener)
				listener->HandleMessage(uMsg, wParam, lParam);
		}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
