// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "modules/hardcore/timer/optimer.h"

#ifndef PROCESSITEM_H
#define PROCESSITEM_H

class HandleInfo;
class ProcessModel;

static const int ENUMERATE_PROCESS_LIST_NOW				=     0;
static const int ENUMERATE_PROCESS_LIST_TIMEOUT			=  1500;
static const int TERMINATE_PROCESS_GRACEFULLY_TIMEOUT	= 10000;

/*
 *	Class ProcessItem
 *
 *	- Represents a process and it's "properties" on the system.
 *	- Togheter with ProcessModel, it can be used to present a set of processes to the user in a TreeView.
 *	- Has the possibility to terminate other processes, with gracefull handling for windowed Windows processes 
 *		(might fail if user does not have enough permissions).
 *	- To get debug info in the output window, set the "Processes" key in the debug.txt file.
 *
 *	  NOTE:	There is process helper functions declared at the bottom, 
 *			which are used by ProcessItem/ProcessModel, and other parts of the Windows code.
 */

class ProcessItem : public TreeModelItem<ProcessItem, ProcessModel>, public OpTimerListener
{
	public:

		ProcessItem(DWORD process_id);
		ProcessItem(){ }
		~ProcessItem();

		OP_STATUS OnInit(const ProcessItem& process);

		// Override baseclass methods
		virtual OP_STATUS	GetItemData(ItemData* item_data);
		virtual Type		GetType()	{ return OpTypedObject::UNKNOWN_TYPE; }
		virtual INT32		GetID()		{ return m_process_id; }
			
		// Helper methods 
		OP_STATUS			SetProcessPath(uni_char* process_path);
		OP_STATUS			SetProcessTitle(uni_char* process_title) { return m_process_title.Set(process_title); }
		OP_STATUS			FindProcessTitle();
		OP_STATUS			LoadProcessIcon();

		DWORD				GetProcessID()	  const		{ return m_process_id; }
		const uni_char*		GetProcessPath() const		{ return m_process_path.CStr(); }
		const uni_char*		GetProcessTitle() const		{ return m_process_title.CStr(); }
		
		void				TerminateProcessGracefully();
		void				TerminateProcess();

		// OpTimerlistener implementation
		virtual void		OnTimeOut(OpTimer* timer);

	private:

		OP_STATUS			GetWindowList(OpAutoVector<HWND>& window_list);

		DWORD				m_process_id;
		OpString			m_process_path;
		OpString			m_process_title;
		Image				m_iconbitmap;

		OpTimer*			m_terminate_timer;			// The timer that will terminate the app, if it does not want to end it's session.
};

namespace ProcessTools
{
	//
	//	Struct to fill out when sending/posting/.. messages
	//	to other process (used for example to terminate other processes).
	//
	struct MessageParam
	{
		enum { POST, SEND };

		DWORD		MessageType;
		DWORD		process_id;
		DWORD		window_message;
		LRESULT		answer;
		WPARAM		wparam;
		LPARAM		lparam;
	};

	//
	//	Struct used as parameter when terminating applications async (threaded).
	//
	struct TerminateParam
	{
		DWORD		process_id;
		DWORD		timeout;
	};

	//
	// Sends WM_CLOSE to every window, then let Windows terminate
	// the application if the app refuses to quit that way.
	//
	OP_STATUS WINAPI TerminateApp(DWORD pid, DWORD timeout = 0);
	void TerminateAppThreaded(PVOID pParam);

	//
	// Sends WM_QUERYENDSESSION and WM_ENDSESSION to applications
	// to make them terminate.
	//
	void WINAPI EndAppSession(DWORD pid, DWORD timeout = 0);
	void EndAppSessionThreaded(PVOID pParam);
}
#endif // PROCESSITEM_H