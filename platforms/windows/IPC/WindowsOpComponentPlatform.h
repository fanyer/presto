/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OP_COMPONENT_PLATFORM_H
#define WINDOWS_OP_COMPONENT_PLATFORM_H

#include "modules/hardcore/component/OpComponentPlatform.h"
#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/IPC/SharedMemory.h"
#include "platforms/windows/WindowsTimer.h"

class WindowsOpPluginTranslator;

class WindowsOpComponentPlatform
	: public OpComponentPlatform
#ifdef NS4P_COMPONENT_PLUGINS
	, WindowsTimerListener
#endif // NS4P_COMPONENT_PLUGINS
{
public:
	WindowsOpComponentPlatform();
	virtual ~WindowsOpComponentPlatform();

	BOOL IsInitialProcess() {return m_is_initial_process;}

	/*Implementation of OpComponentPlatform*/
#ifdef NS4P_COMPONENT_PLUGINS
	virtual void RequestRunSlice(unsigned int limit);
	virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags);
#endif // NS4P_COMPONENT_PLUGINS

	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS RequestPeerThread(int& peer, OpMessageAddress requester, OpComponentType type) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS SendMessage(OpTypedMessage* message);

	virtual void OnComponentCreated(OpMessageAddress address) {}
	virtual void OnComponentDestroyed(OpMessageAddress address) {}

	virtual OP_STATUS Init();

#ifdef NS4P_COMPONENT_PLUGINS
	virtual OP_STATUS Run();

	/*Implementation of WindowsTimerListener*/
	void OnTimeOut(class WindowsTimer* timer);
#endif // NS4P_COMPONENT_PLUGINS

	class WMListener
	{
	public:
		WMListener() {}
		virtual ~WMListener() {}

		virtual void HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
	};

	OP_STATUS SetWMListener(UINT message, WMListener* listener);
	OP_STATUS RemoveWMListener(UINT message);
	HWND GetMessageWindow() {return m_message_window;}

	static double GetRuntimeMSImpl();

	//Since we may have several concurent ComponentPlatform objects when we start having separate threads,
	//The window class (un)registration needs to be requested from outisde.
	static OP_STATUS RegisterWNDCLASS();
	static void UnregisterWNDCLASS();

#ifdef NS4P_COMPONENT_PLUGINS
	static OP_STATUS ProcessCommandLineRequest(WindowsOpComponentPlatform *&component_platform, const UniString &command_line);
#endif // NS4P_COMPONENT_PLUGINS

protected:
	WindowsSharedMemory* m_shared_mem;	//A pointer to the shared memory owned by this process

	BOOL m_exit_loop;					//Subclasses can set this if the message loop should be interrupted

	EventFlags m_event_flags;			//event_flags passed to the current ProcessEvents call or PROCESS_ANY if we aren't in a call to ProcessEvents

	/** Passes a message retrieved using PeekMessage for processing by a subclass.
	  *
	  * @param msg The message retrieved
	  */
	virtual OP_STATUS ProcessMessage(MSG &msg) {return OpStatus::OK;}

	/** Lets the subclass perform its own tasks in the message loop's body
	  *
	  * @param ran_core Whether core was run since the last call to DoLoopContent
	  */
	virtual OP_STATUS DoLoopContent(BOOL ran_core) {return OpStatus::OK;}

private:
#ifdef NS4P_COMPONENT_PLUGINS
	WindowsTimer m_timer;				//Timer, used to trigger core runs at the time core wants it.
#endif // NS4P_COMPONENT_PLUGINS
	BOOL m_ran_core;					//Whether core ran since the last call to
	BOOL m_is_initial_process;			//TRUE if this process has the main component manager.

	BOOL m_must_run_core;				//TRUE if RequestRunSlice was called with a timeout of 0, in which case we should try running core ASAP.

	HANDLE m_initial_process;			//HANDLE to the process which has the initial component manager.
	HANDLE m_initial_process_wait;		//Wait handle on the initial process, returned by RegisterWaitForSingleObject.
	//Callback procedure called when the initial process dies.
	static VOID CALLBACK ReportDeadCM0(PVOID message_window, BOOLEAN timeout);

	HANDLE m_sm_read_ready_wait;		//Wait handle on our shared mem's read event, returned by RegisterWaitForSingleObject.
	//Callback procedure called when our shared memory's read event is set
	static VOID CALLBACK ReportReadReady(PVOID message_window, BOOLEAN timeout);

	/* Message only window used to allow running core, regardless of whether our message loop
	   or a message loop created by the OS is active*/
	HWND m_message_window;
	static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static const uni_char* s_message_window_class_name;

	WMListener* m_WM_listeners[LAST_PSEUDO_THREAD_MESSAGE - FIRST_PSEUDO_THREAD_MESSAGE];

#ifdef NS4P_COMPONENT_PLUGINS
	/* Actual implementation of Run, called by ProcessEvents also. */
	OP_STATUS RunLoop(BOOL once);
#endif // NS4P_COMPONENT_PLUGINS

	OP_STATUS m_loop_return_value; //Status which will be returned at the end of the loop.
};

#endif //WINDOWS_OP_COMPONENT_PLATFORM_H
