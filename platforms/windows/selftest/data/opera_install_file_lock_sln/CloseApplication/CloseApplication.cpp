// close-application.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>

#include "Shellapi.h"

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

// Posts a message to a window with the pid wmp->process_id
// lParam should be of type WindowMessageParam
BOOL CALLBACK MessageEnum(HWND hwnd, LPARAM lParam)
{

	MessageParam* mp = (MessageParam*)lParam;
	DWORD process_id;

    GetWindowThreadProcessId(hwnd, &process_id);

    if(process_id == mp->process_id)
	{
		if (mp->MessageType == MessageParam::POST)
		{
			mp->answer = PostMessage(hwnd, mp->window_message, mp->wparam, mp->lparam);
			WCHAR buffer[200] = {0};
			wsprintf(buffer, L"Post message returned %d\n", mp->answer);
			OutputDebugString(buffer);
		}
		else if (mp->MessageType == MessageParam::SEND)
		{
			mp->answer = SendMessage(hwnd, mp->window_message, mp->wparam, mp->lparam);
		}
		else
		{
			mp->answer = 0;
			return FALSE;
		}
	}

    return TRUE;
}

// Terminates an app, by first sending WM_CLOSE to all it's windows,
// and then let Windows Terminate the app if it is still running.
BOOL WINAPI TerminateApp(DWORD pid, DWORD timeout)
{

    HANDLE process;
    BOOL status = TRUE;

    // If we can't open the process with PROCESS_TERMINATE rights,
    // then we give up immediately.
    process = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, pid);
	if (!process)
		return FALSE;

	MessageParam mp;
	mp.MessageType = MessageParam::POST;
	mp.process_id = pid;
	mp.window_message = WM_CLOSE;
	mp.lparam = NULL;
	mp.wparam = NULL;

    // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
    // matches your process's.
    EnumWindows((WNDENUMPROC)MessageEnum, (LPARAM)&mp);

    // Wait on the handle. If it signals, great. If it times out,
    // then you kill it.
    if(WaitForSingleObject(process, timeout) != WAIT_OBJECT_0)
	{
        status = (TerminateProcess(process, 0) ? TRUE : FALSE);
	}
    else
        status = TRUE;

    CloseHandle(process);

    return status;
}

int _tmain(int argc, _TCHAR* argv[])
{

	if (__argc != 2)
		return 1;

	LPWSTR* wide_commandline = NULL;
	int commandline_argument_number;
	wide_commandline = CommandLineToArgvW(GetCommandLineW(), &commandline_argument_number);

	//if (!iswdigit(wide_commandline[1]))
	//	return 1;

	int pid = _wtoi(wide_commandline[1]);

	TerminateApp(pid, 1500);

	return 0;
}

