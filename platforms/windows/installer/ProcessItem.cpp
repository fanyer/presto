// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "core/pch.h"

#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/ProcessItem.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows_common/utils/fileinfo.h"

#include <process.h>

using namespace ProcessTools;

ProcessItem::ProcessItem(DWORD process_id) : m_process_id(process_id)
											,m_terminate_timer(NULL)
{

}

ProcessItem::~ProcessItem()
{
	if (m_terminate_timer)
	{
		m_terminate_timer->Stop();
		OP_DELETE(m_terminate_timer);
	}
}

OP_STATUS ProcessItem::OnInit(const ProcessItem& process)
{
	RETURN_IF_ERROR(m_process_path.Set(process.GetProcessPath()));
	OP_ASSERT(m_process_path.HasContent());

	m_process_id = process.GetProcessID();
	RETURN_IF_ERROR(m_process_title.Set(process.GetProcessTitle()));
	return LoadProcessIcon();
}

OP_STATUS ProcessItem::SetProcessPath(uni_char* process_path)
{
	return m_process_path.Set(process_path);
}

OP_STATUS ProcessItem::LoadProcessIcon()
{
	OpBitmap* iconbitmap = NULL;
	
	RETURN_IF_ERROR(GetFirstIconFromBinary(m_process_path.CStr(), &iconbitmap, 16));

	if(!iconbitmap)
		return OpStatus::ERR;
	else
		m_iconbitmap = imgManager->GetImage(iconbitmap);

	return OpStatus::OK;
}

OP_STATUS ProcessItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == COLUMN_QUERY)
	{
		if (item_data->column_query_data.column == 0)
		{
			item_data->column_query_data.column_text->Set(m_process_title.CStr()); // Window title
			if (!m_iconbitmap.IsEmpty()) 	// Get an icon
			{
				item_data->column_bitmap = m_iconbitmap;
			}
		}
	}
	return OpStatus::OK;
}


OP_STATUS ProcessItem::FindProcessTitle()
{
	if (m_process_path.IsEmpty())
		return OpStatus::ERR;

	UniString result;

	WindowsCommonUtils::FileInformation fileinfo;
	if (OpStatus::IsSuccess(fileinfo.Construct(m_process_path)))
	{
		// Try product name first
		if (OpStatus::IsError(fileinfo.GetInfoItem(UNI_L("ProductName"), result)))
			// Try file description next
			OpStatus::Ignore(fileinfo.GetInfoItem(UNI_L("FileDescription"), result));
	}

	// Fall back to file name
	if (result.IsEmpty())
	{
		int pos = m_process_path.FindLastOf(UNI_L('\\'));
		if (pos != KNotFound)
		{
			OP_STATUS aErr;
			result.SetCopyData(m_process_path.SubString(pos+1, KAll, &aErr));
			RETURN_IF_ERROR(aErr);
		}
	}

	const uni_char* result_str;
	RETURN_IF_ERROR(result.CreatePtr(&result_str, TRUE));		//TODO: Use UniString to OpString conversion function when available

	return (!result.IsEmpty()) ? m_process_title.Set(result_str) : OpStatus::ERR;
}

struct GetWindowParam
{
	DWORD process_id;
	OpVector<HWND>*	window_list;
};

// lParam should be of type WindowMessageParam
BOOL CALLBACK GetWindowEnum(HWND hwnd, LPARAM lParam)
{
	GetWindowParam* gwp = (GetWindowParam*)lParam;
	DWORD process_id = gwp->process_id;
	OpVector<HWND>* window_list = gwp->window_list;

	DWORD window_pid;
    GetWindowThreadProcessId(hwnd, &window_pid);

    if(process_id == window_pid)
	{
		HWND* win_handle = OP_NEW(HWND, );
		*win_handle = hwnd;
		window_list->Add(win_handle);
	}

    return TRUE;
}

OP_STATUS ProcessItem::GetWindowList(OpAutoVector<HWND>& window_list)
{
	GetWindowParam gwp;
	gwp.process_id = m_process_id;
	gwp.window_list = &window_list;
	EnumWindows((WNDENUMPROC)GetWindowEnum, (LPARAM)&gwp);

	return OpStatus::OK;
}

void ProcessItem::TerminateProcessGracefully()
{
	OpAutoVector<HWND> window_list;
	GetWindowList(window_list);

	if (window_list.GetCount() > 0)
	{
		TerminateParam* param = OP_NEW(TerminateParam, ());
		param->process_id = m_process_id;

		_beginthread(EndAppSessionThreaded, 0, param);

		m_terminate_timer = OP_NEW(OpTimer, ());
		m_terminate_timer->SetTimerListener(this);
		m_terminate_timer->Start(TERMINATE_PROCESS_GRACEFULLY_TIMEOUT);
	}
	else
		TerminateProcess();
}

void ProcessItem::TerminateProcess()
{
	OpAutoVector<HWND> window_list;
	GetWindowList(window_list);

	OP_NEW_DBG("ProcessItem", "Processes");		

	OP_DBG(("Process %d has %d windows.", m_process_id, window_list.GetCount()));

	if (window_list.GetCount() > 0)
	{
		TerminateParam* param = OP_NEW(TerminateParam, ());
		param->process_id = m_process_id;
		param->timeout = TERMINATE_PROCESS_GRACEFULLY_TIMEOUT;

		_beginthread(TerminateAppThreaded, 0, param);
	}
	else
	{
	    HANDLE process = ::OpenProcess(PROCESS_TERMINATE, FALSE, m_process_id);
		if (process)
		{
			OP_DBG(("Terminating process %d with no windows.", m_process_id));
			::TerminateProcess(process, 0);
		}
		CloseHandle(process);
	}
}

void ProcessItem::OnTimeOut(OpTimer* timer)
{
	if (timer == m_terminate_timer)
	{
		m_terminate_timer->Stop();
		TerminateProcess();
	}
}

// Posts a message to a window with the pid wmp->process_id
// lParam should be of type WindowMessageParam
BOOL CALLBACK MessageEnum(HWND hwnd, LPARAM lParam)
{
	OP_NEW_DBG("MessageEnum", "Processes");

	MessageParam* mp = (MessageParam*)lParam;
	DWORD process_id;

    GetWindowThreadProcessId(hwnd, &process_id);

    if(process_id == mp->process_id)
	{
		if (mp->MessageType == MessageParam::POST)
		{
			mp->answer = PostMessage(hwnd, mp->window_message, mp->wparam, mp->lparam);
			OP_DBG(("Post message returned %d", mp->answer));
		}
		else if (mp->MessageType == MessageParam::SEND)
		{
			mp->answer = SendMessage(hwnd, mp->window_message, mp->wparam, mp->lparam);
			OP_DBG(("Send message returned %d", mp->answer));
		}
		else
		{
			OP_ASSERT(!"This message type is either invalid, or not been added yet.");
			mp->answer = 0;
			return FALSE;
		}
	}

    return TRUE;
}

// Terminates an app, by first sending WM_CLOSE to all it's windows,
// and then let Windows Terminate the app if it is still running.
OP_STATUS WINAPI ProcessTools::TerminateApp(DWORD pid, DWORD timeout)
{
	OP_NEW_DBG("TerminateApp", "Processes");

    HANDLE process;
    OP_STATUS status = OpStatus::OK;

    // If we can't open the process with PROCESS_TERMINATE rights,
    // then we give up immediately.
    process = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, pid);
	RETURN_OOM_IF_NULL(process);

	MessageParam mp;
	mp.MessageType = MessageParam::POST;
	mp.process_id = pid;
	mp.window_message = WM_CLOSE;
	mp.lparam = NULL;
	mp.wparam = NULL;

	OP_DBG(("Sending WM_CLOSE to %d", pid));

    // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
    // matches your process's.
    EnumWindows((WNDENUMPROC)MessageEnum, (LPARAM)&mp);

    // Wait on the handle. If it signals, great. If it times out,
    // then you kill it.
    if(WaitForSingleObject(process, timeout) != WAIT_OBJECT_0)
	{
		OP_DBG(("Let Windows Terminate %d", pid));
        status = (TerminateProcess(process, 0) ? OpStatus::OK : OpStatus::ERR);
	}
    else
        status = OpStatus::OK;

    CloseHandle(process);

    return status;
}

// Function with special signature, to be started
// threaded by the use of _beginthread
void ProcessTools::TerminateAppThreaded(PVOID pParam)
{
	OP_NEW_DBG("TerminateAppThreaded", "Processes");

	TerminateParam* param = reinterpret_cast<TerminateParam*>(pParam);

	OP_DBG(("Terminate process %d", param->process_id));

	OpStatus::Ignore(TerminateApp(param->process_id, param->timeout));

	OP_DELETE(param);
}

// Tries to end an application by first sending WM_QUERYENDSESSION
// Then moving on to send WM_ENDSESSION to all windows of the "pid" application.
void WINAPI ProcessTools::EndAppSession(DWORD pid, DWORD timeout)
{
	OP_NEW_DBG("EndAppSession", "Processes");

	MessageParam mp;
	mp.MessageType = MessageParam::SEND;
	mp.process_id = pid;
	mp.window_message = WM_QUERYENDSESSION;
	mp.wparam = NULL;
	mp.lparam = NULL;

	OP_DBG(("Sending WM_QUERYENDSESSION to pid %u", pid));

    // Enumerates the windows, and sends the window message
    EnumWindows((WNDENUMPROC)MessageEnum, (LPARAM)&mp);

	mp.MessageType = MessageParam::SEND;
	mp.process_id = pid;
	mp.window_message = WM_ENDSESSION;
	mp.wparam = (WPARAM)TRUE;
	mp.lparam = (LPARAM)ENDSESSION_CLOSEAPP;

	OP_DBG(("Sending WM_ENDSESSION to pid %u", pid));

	 // Enumerates the windows, and sends the window message
	EnumWindows((WNDENUMPROC)MessageEnum, (LPARAM)&mp);
}

// Function with special signature, to be started
// threaded by the use of _beginthread
void ProcessTools::EndAppSessionThreaded(PVOID pParam)
{
	OP_NEW_DBG("EndAppSessionThreaded", "Processes");

	TerminateParam* param = reinterpret_cast<TerminateParam*>(pParam);

	OP_DBG(("End session for process %d", param->process_id));

	EndAppSession(param->process_id);

	OP_DELETE(param);
}
