/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#include "core/pch.h"

#include "OperaDll.h"

#include "adjunct/quick/managers/LaunchManager.h"
#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/updater/auupdater.h"
#endif

#include "platforms/crashlog/gpu_info.h"
#include "platforms/windows/user.h"
#include "platforms/windows/windows_ui/menubar.h"
#include "platforms/windows/CustomWindowMessages.h"

extern HINSTANCE	hInst;	// Handle to instance.
extern OpString	g_spawnPath;
extern OpVector<OpString>* g_uninstall_files;

extern GpuInfo* g_gpu_info;
int WINAPI OperaEntry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow);

/***********************************************************************************
**
**	DllMain
**
***********************************************************************************/

BOOL APIENTRY DllMain(   HINSTANCE hinstDLL,  // handle to DLL module
						 DWORD fdwReason,     // reason for calling function
						 LPVOID lpvReserved   // reserved
					 )
{

	hInst = hinstDLL;

	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		return WindowsTLS::Init();

	case DLL_THREAD_ATTACH:
		WindowsTLS::AttachThread();
		break;

	case DLL_THREAD_DETACH:
		WindowsTLS::DetachThread();
		break;

	case DLL_PROCESS_DETACH:
		WindowsTLS::Destroy();
		break;
	}
 
	return TRUE;
}

//This is the procedure that is run when Opera is used via the EmBrowser API
LRESULT BLD_CALLBACK DLLWndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch(message)
	{
	case WM_SETTINGCHANGE:
		extern void OnSettingsChanged();
		OnSettingsChanged();
		return 0;
		
    case WM_DRAWITEM:
        return ucWMDrawItem(hWnd,message,wParam,lParam);
		
    case WM_MEASUREITEM:
        return ucMeasureItem(hWnd,message,wParam,lParam);
		
    case WM_MENUCHAR:
        return g_windows_menu_manager->OnMenuChar(LOWORD(wParam), (HMENU) lParam);
    case WM_COMMAND:
		return g_windows_menu_manager && g_windows_menu_manager->OnMenuCommand(LOWORD(wParam));
    case WM_TIMER:
		{
			return ucTimer(hWnd,message,wParam,lParam);
		}
		
	case WM_KEYDOWN:
		{
			g_input_manager->InvokeKeyPressed(static_cast<OpKey::Code>(wParam), 0);
		}
		
		break;
		
		
	default:
		{ 
			if (hWnd == g_main_hwnd && g_main_message_handler && (message >= WM_USER)) // The (message >= WM_USER) test checks if this is a message posted by opera.
			{
				switch (message)
				{
					case WM_WINSOCK_ACCEPT_CONNECTION:
					{
						OpMessage opr_msg = WINSOCK_ACCEPT_CONNECTION;
						g_main_message_handler->PostDelayedMessage(opr_msg, wParam, lParam, 0);
						break;
					}
					case WM_WINSOCK_CONNECT_READY:
					{
						OpMessage opr_msg = WINSOCK_CONNECT_READY;
						g_main_message_handler->PostDelayedMessage(opr_msg, wParam, lParam, 0);
						break;
					}
					case WM_WINSOCK_DATA_READY:
					{
						OpMessage opr_msg = WINSOCK_DATA_READY;
						g_main_message_handler->PostDelayedMessage(opr_msg, wParam, lParam, 0);
						break;
					}
					case WM_WINSOCK_HOSTINFO_READY:
					{
						OpMessage opr_msg = WINSOCK_HOSTINFO_READY;
						g_main_message_handler->PostDelayedMessage(opr_msg, wParam, lParam, 0);
						break;
					}
					default:
					{
						OP_ASSERT(0);
						// Note by mariusab 20051222:
						// If this hits, a message in user space has been posted that is not properly handled when it returns.
						// The PostDelayedMessage callback requires OpMessage-messages, and these don't have set values. 
						// Mapping between windows user space messages and OpMessages must be done explicitly.
						break;
					}
				}
				return FALSE;
			}
			else
			{
				return DefWindowProc(hWnd, message,wParam,lParam);
			}
		}
	}
	return FALSE;
}

/***********************************************************************************
**
**	exported OpStart
**
***********************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport)
int OpStart(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{

	//Note: hInstance is the callers instance, our hInst is initialized in DllMain 

	return OperaEntry(hInst, hPrevInstance, lpCmdLine, nCmdShow);
}

/***********************************************************************************
**
**	exported OpSetSpawnPath
**
***********************************************************************************/
__declspec(dllexport)
int OpSetSpawnPath(uni_char* path)
{
	if(g_spawnPath.Set(path) == OpStatus::OK)
	{
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************************
**
**	exported OpSetLaunchMan
**
***********************************************************************************/
__declspec(dllexport)
int OpSetLaunchMan(void* launch_man)
{
#ifdef AUTO_UPDATE_SUPPORT
	LaunchManager::SetInstance((LaunchManager *)launch_man);
#endif // AUTO_UPDATE_SUPPORT
	return 0;
}

/***********************************************************************************
**
**	exported OpGetNextUninstallFile
**
***********************************************************************************/
__declspec(dllexport)
uni_char* OpGetNextUninstallFile()
{
	static OpString* last_uninstall_file = NULL;

	OP_DELETE(last_uninstall_file);
	last_uninstall_file = NULL;

	if (g_uninstall_files && g_uninstall_files->GetCount() == 0)
	{
		OP_DELETE(g_uninstall_files);
		g_uninstall_files = NULL;
	}

	if (g_uninstall_files == NULL)
		return NULL;

	last_uninstall_file = g_uninstall_files->Remove(0);

	return last_uninstall_file->CStr();
}

/***********************************************************************************
**
**  exported OpWaitFileWasPresent
**
***********************************************************************************/
__declspec(dllexport)
void OpWaitFileWasPresent()
{
#ifdef AUTO_UPDATE_SUPPORT
	AUUpdater::SetWaitFileWasPresent();
#endif // AUTO_UPDATE_SUPPORT
}

/***********************************************************************************
**
**  exported OpSetGpuInfo
**
***********************************************************************************/
__declspec(dllexport)
void OpSetGpuInfo(GpuInfo* gpu_info)
{
	g_gpu_info = gpu_info;
}

#ifdef __cplusplus
}
#endif
