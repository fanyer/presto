/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/autoupdate/version_checker.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/quick/dialogs/crashloggingdialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#ifdef WIDGET_RUNTIME_SUPPORT
# include "adjunct/widgetruntime/GadgetStartup.h"
#endif // WIDGET_RUNTIME_SUPPORT
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/gpu/gpubenchmark.h"

#include "modules/dochand/win.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpThreadTools.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefsfile.h"
#ifdef SUPPORT_PROBETOOLS
# include "modules/probetools/probepoints.h"
#endif // SUPPORT_PROBETOOLS
#ifdef SELFTEST
# include "modules/selftest/optestsuite.h"
#endif
#include "modules/util/filefun.h"
#ifdef ENCRYPT_FILE_HACK
# include "modules/util/opcrypt.h"
#endif //ENCRYPT_FILE_HACK
#include "modules/util/opfile/opfile.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/user.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/userdde.h"
#include "platforms/windows/utils/authorization.h"
#include "platforms/windows/utils/OperaProcessesList.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/WindowsDesktopGlobalApplication.h"
#include "platforms/windows/pi/WindowsOpLocale.h"
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#if defined(_PLUGIN_SUPPORT_)
# include "platforms/windows/pi/WindowsOpPluginWindow.h"
#endif // _PLUGIN_SUPPORT_
#ifndef NS4P_COMPONENT_PLUGINS
# include "platforms/windows/windows_ui/msghook.h"
#endif // ! NS4P_COMPONENT_PLUGINS
#include "platforms/windows/pi/WindowsOpScreenInfo.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/pi/WindowsOpTimeInfo.h"
#include "platforms/windows/pi/WindowsOpThreadTools.h"

#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"

#include "platforms/windows/Installer/OperaInstaller.h"
#include "platforms/windows/network/WindowsSocket2.h"
#include "platforms/windows/network/WindowsHostResolver2.h"
#ifdef _INTELLIMOUSE_SUPPORT_
#include "platforms/windows/windows_ui/intmouse.h"
#endif // _INTELLIMOUSE_SUPPORT_
#include "platforms/windows/windows_ui/menubar.h"
#include "platforms/windows/windows_ui/msghook.h"
#include "platforms/windows/windows_ui/oui.h"
#include "platforms/windows/windows_ui/printwin.h"
#include "platforms/windows/windows_ui/rasex.h"
#include "platforms/windows/windows_ui/Registry.h"

#include "platforms/windows/Windows3dDevice.h"
#include "platforms/windows/user_fun.h"

#include "platforms/windows/windows_ui/autoupdatereadycontroller.h"
#include "platforms/windows/windows_ui/res/resource.h"

// *************************************************************
//					GLOBAL VARIABLES
// *************************************************************
static  UINT	MSG_READ_PROXY_SETTINGS_FROM_REGISTRY = 0;

extern void							OnDestroyMainWindow();

#ifndef NS4P_COMPONENT_PLUGINS
extern DelayedFlashMessageHandler* g_delayed_flash_message_handler;
#endif // !NS4P_COMPONENT_PLUGINS

CoreComponent*	g_opera = NULL;
OpThreadTools*	g_thread_tools = NULL;

OpString8		g_starting_signature;
OpString8		g_stopping_signature;
HANDLE			g_no_dde_mutex;
HINSTANCE		hInst				= 0;	// Handle to instance.
HWND			g_main_hwnd			= 0;	// Handle to main window.
int				DefShowWindow		= 0;	// How to show main window
OpString		g_spawnPath;
UINT			g_codepage = GetACP();
BOOL			g_far_eastern_win = ((932 == g_codepage) || (936 == g_codepage) || (949 == g_codepage) || (950 == g_codepage));

BOOL	BLDMainRegClass(HINSTANCE hInstance, BOOL use_dll_main_window_proc);
HWND	BLDCreateWindow(HINSTANCE hInstance);
LRESULT BLD_CALLBACK BLDMainWndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);

#ifdef EMBROWSER_SUPPORT
extern	long gVendorDataID;
#endif // EMBROWSER_SUPPORT

extern	int RunOpera(OpVector<OpString>* commandlineurls, OpString8* signature);
extern OpVector<OpFile>* g_delete_profile_files;

static UINT _RegisterWindowMessage( const char* szMessage)
{
	UINT idMsg = RegisterWindowMessageA( szMessage);
// 	OP_RELEASE_ASSERT( idMsg);
	return idMsg;
}

UINT	idMsgLostCursor			= _RegisterWindowMessage("opera_idMsgLostCursor");			//	duplicated in sfttree.cpp

class WindowsVersionCheckerListener : public VersionCheckerListener
{
public:
	virtual void VersionCheckFinished()
	{
		PostMessage(g_main_hwnd, WM_OPERA_CREATED, 0, 0);
	}
} g_windowsVersionCheckerListener;

VersionChecker *g_version_checker;

/*
For Opera's use of the Win32 heap, it makes a lot of sense to use the low-fragmentation heap in Windows
as it will be much more optimized for many smaller allocations. 
See http://msdn.microsoft.com/en-us/library/aa366750(VS.85).aspx for more details.

- pettern, 2008-12-08
*/
BOOL SetLowFragmentationHeap()
{
	HANDLE heap = GetProcessHeap();
	if(heap)
	{
		ULONG HeapFragValue = 2; // enable the low-fragmentation heap

		// not critical if it fails
		HeapSetInformation(heap, HeapCompatibilityInformation, &HeapFragValue, sizeof(HeapFragValue));

		// set LFH for the CRT heap too
		HeapSetInformation((PVOID)_get_heap_handle(), HeapCompatibilityInformation, &HeapFragValue, sizeof(HeapFragValue));

		return TRUE;
	}

	return FALSE;
}

BOOL SetScreenResolution(int new_screen_width, int new_screen_height, int& old_screen_width, int& old_screen_height)
{
	// set the wanted screen resolution
	DEVMODE devmode;
	
	ZeroMemory(&devmode, sizeof(DEVMODE));
	devmode.dmSize = sizeof(DEVMODE);
	
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode))
	{
		old_screen_width = devmode.dmPelsWidth;
		old_screen_height = devmode.dmPelsHeight;

		devmode.dmPelsWidth = new_screen_width;
		devmode.dmPelsHeight = new_screen_height;

		devmode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
		
		LONG ret = ChangeDisplaySettings(&devmode, 0);
		
		OP_ASSERT(DISP_CHANGE_SUCCESSFUL == ret);

		if (ret == DISP_CHANGE_SUCCESSFUL)
		{
			return TRUE;
		}
	}	
	return FALSE;
}

#ifdef WIDGET_RUNTIME_SUPPORT 
/*
* variable used to track another instance of the opera(widget) process
* used in WinMain to pass information to OpEnumProc
*/
DWORD checked_process_id = 0;

/*
* Call back function for enumarating windows.
* Usedm in case when user want to start another instance of the same widget
* to send to another instance of widget (its window) 
*information to activate and show up on desktop
*/
BOOL CALLBACK OpEnumProc(HWND hWnd, LPARAM lParam)
{
	DWORD dwProcessId;
	DWORD process_id = GetWindowThreadProcessId(hWnd,&dwProcessId);
	
	if (checked_process_id != 0 && (dwProcessId == checked_process_id || checked_process_id == process_id))
	{
		SetForegroundWindow(hWnd);
		// if window is found and messages have been send, we can stop enumerating windows
		return FALSE;
	}
    return TRUE;
}
    
#endif //WIDGET_RUNTIME_SUPPORT 


// Message handler for launch error dialog box.
INT_PTR CALLBACK LaunchErrorWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
		case WM_INITDIALOG:
		{
			HWND hwndProgress = GetDlgItem(hDlg, IDC_PROGRESS1);
			
			if (!hwndProgress)
				return (INT_PTR)TRUE;

			if (GetWinType() < WINXP)
			{
				ShowWindow(hwndProgress,SW_HIDE);
			}
			else
			{
				DWORD dwStyle = GetWindowLong(hwndProgress, GWL_STYLE);
				SetWindowLong(hwndProgress, GWL_STYLE, dwStyle|PBS_MARQUEE);
				SendMessage(hwndProgress,(UINT) PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30);
				SetTimer(hDlg, NULL, 500, NULL);
			}
			return (INT_PTR)TRUE;
		}

		case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK)
				TerminateCurrentHandles();
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;

		case WM_TIMER:
		{
			HANDLE test_dde_mutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, g_stopping_signature.CStr());
			if (!test_dde_mutex)
				break;

			switch (WaitForSingleObject(test_dde_mutex, 1))
			{
				case WAIT_OBJECT_0:
				case WAIT_ABANDONED:
				case WAIT_FAILED: 
					KillTimer(hDlg, NULL);
					EndDialog(hDlg, LOWORD(IDOK));
					break;

				case WAIT_TIMEOUT:
					//continue waiting
					break;
			}
			ReleaseMutex(test_dde_mutex);
			CloseHandle(test_dde_mutex);
			return (INT_PTR)TRUE;
		}
	}
	return (INT_PTR)FALSE;
}


//	___________________________________________________________________________
//	WinMain
//	___________________________________________________________________________
//
int WINAPI OperaEntry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
	SetLowFragmentationHeap();

//Uncomment to set up a console for the process (useful for printf debugging when no debugger is hooked up or when the debugger's output window is inadequate
/*
	AllocConsole();
	freopen("CONOUT$", "wb", stdout);
*/

	if (OpStatus::IsError(WindowsOpComponentPlatform::RegisterWNDCLASS()))
		return 0;

	//
	//	Parse command line - do it here as it's needed for the command line profiling
	//

	LPWSTR command_line = GetCommandLineW();

	if (uni_strncmp(command_line, "-newprocess ", 12) == 0)
	{
#ifdef NS4P_COMPONENT_PLUGINS
		WindowsOpComponentPlatform* platform;
		UniString command_line_string;
		command_line_string.SetCopyData(command_line + 12);
		if (OpStatus::IsSuccess(WindowsOpComponentPlatform::ProcessCommandLineRequest(platform, command_line_string)))
		{
			if (OpStatus::IsError(platform->Init()))
			{
				OP_DELETE(platform);
				return 0;
			}
			platform->Run();
			return 0;
		}
#else
		OP_ASSERT(!"-newprocess not supported without component plugins!");
#endif // !NS4P_COMPONENT_PLUGINS
	}

	CommandLineManager::GetInstance()->Init();

	// process the command line
	LPWSTR* wide_argv = NULL;
	int commandline_argument_number;
	wide_argv = CommandLineToArgvW(command_line, &commandline_argument_number);
	OP_ASSERT (commandline_argument_number == __argc);

	CommandLineManager::GetInstance()->ParseArguments(__argc, __argv, wide_argv);
	LocalFree(wide_argv);

#if defined(_DEBUG) && defined(MEMORY_ELECTRIC_FENCE)
	CommandLineArgument* electric_fence = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::Fence);
	if(electric_fence)
	{
		OpString8 fence;

		OpStatus::Ignore(fence.Set(electric_fence->m_string_value));

		// we want electric fence protection, see tweaks.h for details
		OpMemoryInit(0, FALSE, fence.CStr());
	}
#endif // WINDOWS_ELECTRIC_EFENCE_CLASSES

	OP_PROFILE_INIT();

//set signature
#if defined(EMBROWSER_SUPPORT)
	gVendorDataID = 'OPRA';
#endif //EMBROWSER_SUPPORT

	{
		OP_PROFILE_METHOD("Create core");

		// Create the main component manager
		OP_ASSERT(!g_component_manager);
		OpComponent* singleton;

		RETURN_VALUE_IF_ERROR(OpComponentManager::Create(&g_component_manager), -1);
		if (!g_component_manager || OpStatus::IsError(g_component_manager->CreateComponent(singleton, COMPONENT_SINGLETON)))
		{
			OP_DELETE(g_component_manager);
			return -1;
		}
		g_opera = static_cast<CoreComponent*>(singleton);
	}
	{
		OP_PROFILE_METHOD("Initialized message loop");

		g_windows_message_loop = new WindowsOpMessageLoop();
		if (!g_windows_message_loop || OpStatus::IsError(g_windows_message_loop->Init()))
			return 0;
	}

	g_component_manager->SetPlatform(g_windows_message_loop);
	OperaProcessesList::GetInstance();
	OP_ASSERT(g_opera_processes_list);

	if (!MainThreadSender::Construct())
		return -1;
	RETURN_VALUE_IF_ERROR(OpThreadTools::Create(&g_thread_tools), -1);

#ifdef SUPPORT_PROBETOOLS
	OpProfile::Init1();
#endif

#ifdef _ACTIVATE_TIMING_
	__DO_INIT_TIMING();
#endif // ACTIVATE_TIMING

	OleInitialize(NULL);

	srand( (unsigned)time( NULL ) );

    hInst = hInstance;          //  Saves the current instance
	DefShowWindow = nCmdShow;

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIGiveFolderWriteAccess) && CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIGiveWriteAccessTo))
	{
		OperaInstaller::GiveFolderWriteAccess();
		return 0;
	}

#ifdef _DEBUG
	// Init debug class
	CommandLineArgument* debug_settings = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DebugSettings);
	uni_char* debug_result = NULL;
	if (debug_settings && debug_settings->m_string_value.HasContent())
	{
		OpString8 debugfile;
		debugfile.Set(debug_settings->m_string_value);
		debug_result = Debug::InitSettings(debugfile);
	}
	else
	{
		char cur_dir[1024] = {0};
		_getcwd(cur_dir, 1024);
		char* root_checkout_folder = op_strrchr(cur_dir, '\\');
		op_strcpy(root_checkout_folder, "\\platforms\\windows\\desktop_starter\\debug.txt");
		debug_result = Debug::InitSettings(cur_dir);
	}

	if (debug_result != NULL)
	{
		OutputDebugString(UNI_L("Warning: Error initializing Debug class (Can't find the debug.txt file?"));
		delete [] debug_result;
	}
#endif // _DEBUG

	// Preferences pre-setup -----
	{
		OP_PROFILE_METHOD("Preferences pre-setup");

		if (OpStatus::IsError(g_desktop_bootstrap->Init()))
		{
			if (g_desktop_bootstrap->GetInitInfo())
			{
				MessageBoxA(NULL,
					"Opera has failed to access or upgrade your profile."
					" This may have occurred because your computer has "
					"insufficient resources available or because some "
					"files are locked by other applications. You may "
					"have to restart your computer before Opera will "
					"start again.",
					"Startup error", MB_OK);
			}
			return 0;
		}

	}

	//Run the gpu benchmark test
	CommandLineArgument* gpu_test = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::GPUTest);
	if (gpu_test)
	{
		// disable crash logging
		SetUnhandledExceptionFilter(NULL);
		SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);

		TRAPD(init_error, g_opera->InitL(*g_desktop_bootstrap->GetInitInfo()));
		if (OpStatus::IsError(init_error))
			return 0;

		if (OpStatus::IsSuccess(TestGPU(gpu_test->m_int_value)))
		{
			VEGABlocklistDevice::DataProvider* provider = g_vegaGlobals.vega3dDevice->CreateDataProvider();
			if (provider)
			{
				OpString section;
				if (OpStatus::IsSuccess(GetBackendIdentifier(section, gpu_test->m_int_value, provider)))
					WriteBackendStatus(section, true);
				OP_DELETE(provider);
			}
		}
		g_opera->Destroy();
		return 0;
	}

	OpString8 signature;

	//Note, all mutexes created here will die when this thread dies
	OP_STATUS prefsresult = MakeOperaIniFileCheckSum(
			((OpFile*)((PrefsFile*)g_desktop_bootstrap->GetInitInfo()->prefs_reader)
			 		->GetFile())->GetFullPath(),
			&signature);

	g_starting_signature.Set(signature);
	g_starting_signature.Append("_starting");

	g_stopping_signature.Set(signature);
	g_stopping_signature.Append("_stopping");

	HANDLE mutex = 0;

	if(OpStatus::IsSuccess(prefsresult))
	{
		mutex = CreateMutexA(NULL, TRUE, signature.CStr());
	}
	else
	{
		return 0;
	}
	// we need to give the user feedback based on what the user tried to do
	// Scenarios:
	// no commandline arguments: Open new window in existing Opera instance, we need to create a service based on checksum created over
	// if commandline arguments: Check for link and call RunOpera
	if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall) || CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIUninstall))
		{
			//If we are installing, we don't care about the mutex
			goto continue_normally;
		}

		CommandLineArgument *crash_log = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog);
		if (crash_log)
		{
			// Wait for the crashed instance to die and release the mutex
			DWORD result = WaitForSingleObject(mutex, 10000);
			if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED)
				goto continue_normally;
		}

		{
			HANDLE test_dde_mutex;
			//If the other instance is still initializing, the DDE is unreachable.
			//We should just stop there and then.
			test_dde_mutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, g_starting_signature.CStr());
			if (test_dde_mutex)
			{
				CloseHandle(test_dde_mutex);
				goto emergency_return;
			}

			//If the other instance is closing, the DDE is unreachable.
			//We can safely start a new instance once the previous one is done closing.
			test_dde_mutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, g_stopping_signature.CStr());

			if (test_dde_mutex)
			{
				while (TRUE)
				{
					switch (WaitForSingleObject(test_dde_mutex, 500))
					{
						case WAIT_OBJECT_0:
						case WAIT_ABANDONED:
							//The other instance terminated itself. We can finish opening
							ReleaseMutex(test_dde_mutex);
							CloseHandle(test_dde_mutex);
							goto continue_normally;

						case WAIT_TIMEOUT:
						{
							//Something abnormal is happening to the other instance. Better stop here.
							int result = DialogBox(hInst, MAKEINTRESOURCE(IDD_KILLDLG) , NULL, (DLGPROC)LaunchErrorWndProc);//LPTSTR(UNI_L("KILLD"))

							switch(result)
							{
								case -1:
								case IDCANCEL:
									ReleaseMutex(test_dde_mutex);
									CloseHandle(test_dde_mutex);
									goto emergency_return;

								case IDYES:
									ReleaseMutex(test_dde_mutex);
									CloseHandle(test_dde_mutex);
									goto continue_normally;
							}
				
							break;
						}

						case WAIT_FAILED:
							ReleaseMutex(test_dde_mutex);	
							CloseHandle(test_dde_mutex);
							goto emergency_return;
							break;
					}
				}
			}

			{
				OP_PROFILE_METHOD("Enumerating possible existing windows");

				DWORD process;
				if(process = WindowsUtils::GetOperaProcess())
				{
#ifdef WIDGET_RUNTIME_SUPPORT // fix for: Bring gadget window to front (DSK-259182).
					if (GadgetStartup::IsGadgetStartupRequested())
					{
						checked_process_id = process;
						EnumWindows(&OpEnumProc,0);
					}
					else
#endif //WIDGET_RUNTIME_SUPPORT
					{
						AllowSetForegroundWindow(process);
					}
				}
			}
#ifdef WIDGET_RUNTIME_SUPPORT
			if (!GadgetStartup::IsGadgetStartupRequested())
#endif // WIDGET_RUNTIME_SUPPORT
				// this will try to open the commandline pages in a previously running "Opera" instance.
				RunOpera(CommandLineManager::GetInstance()->GetUrls(), &signature);
		}
		if(mutex)	
		{		
			CloseHandle(mutex);
		}

emergency_return:
		delete g_memory_manager;

		OleUninitialize();

		delete g_windows_message_loop;

		return 0;
	}

continue_normally:
	//Indicates that we are starting and won't respond to DDE before some time
	g_no_dde_mutex = CreateMutexA(NULL, TRUE, g_starting_signature.CStr());

    if (!hPrevInstance)         //  Is there an other instance of the task
	{
		OUIWindow::StaticRegister(hInst);

		//Note: BLDMainRegClass is also called from DllMain, see OperaDll.cpp
		BLDMainRegClass(hInstance, FALSE);
	
#ifdef _PLUGIN_SUPPORT_
        WindowsOpPluginWindow::PluginRegClassDef(hInstance);
#endif // _PLUGIN_SUPPORT_
	}
#ifdef _NO_HELP_
	bHelpSupport = FALSE;
#endif

	//HKEY_CURRENT_USER\Software\Opera Software
	OpString8 last_command_line8;
	OpStatus::Ignore(last_command_line8.AppendFormat("%s %s",
				__argv[0], lpCmdLine));
	OpString last_command_line;
	OpStatus::Ignore(last_command_line.Set(last_command_line8));
	OpRegWriteStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"),
			UNI_L("Last CommandLine v2"), last_command_line.CStr());

	SetErrorMode(SEM_NOOPENFILEERRORBOX);

	{
		OP_PROFILE_MSG("Initializing core starting");
		OP_PROFILE_METHOD("Init core completed");

		TRAPD(init_error, g_opera->InitL(*g_desktop_bootstrap->GetInitInfo()));

		if (OpStatus::IsError(init_error))
		{
			MSG msg;
			const char* msg_err;
			char err_output[80];

			PeekMessage(&msg, g_main_hwnd, WM_RUN_SLICE, WM_RUN_SLICE, PM_REMOVE);
			PeekMessage(&msg, g_main_hwnd, WM_TIMER, WM_TIMER, PM_REMOVE);

			if (OpStatus::IsMemoryError(init_error))
			{
				msg_err = "Cannot initialize Opera due to lack of memory: module %d";
			}
			else
				msg_err = "Error initializing Opera: module %d (%s)";

			sprintf(err_output, msg_err, g_opera->failed_module_index, g_opera->module_names[g_opera->failed_module_index]);

			MessageBoxA(NULL, err_output, "Opera", MB_OK | MB_ICONERROR);

			g_opera->Destroy();
			mswin_clear_locale_cache();

			goto emergency_return;
		}
	}
#ifdef SELFTEST
	TestSuite::main( &__argc, __argv );
#endif

#ifdef _DDE_SUPPORT_
	if (g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::DDEEnabled))
	{
		ddeHandler = new DDEHandler();
		if (!ddeHandler)
			return 0;
		ddeHandler->InitDDE();
	}
#endif

	OUIWindow::StaticRegister(hInst);

#ifdef _INTELLIMOUSE_SUPPORT_
	IntelliMouse_Init();			
#endif

	// g_main_hwnd could also have been created in DllMain, see operadll.cpp
	if(!g_main_hwnd)
	{
		g_main_hwnd = BLDCreateWindow(hInstance);
	}

    if (!g_main_hwnd)
	{
        return 0;
	}

	OpStringC soundfile = g_pcui->GetStringPref(PrefsCollectionUI::StartSound);
	OpStatus::Ignore(SoundUtils::SoundIt(soundfile, FALSE));

	int old_screen_width = 0;
	int old_screen_height = 0;

	CommandLineArgument* new_screen_width = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ScreenWidth);
	CommandLineArgument* new_screen_height = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ScreenHeight);

	if (new_screen_width && new_screen_height && new_screen_width->m_int_value && new_screen_height->m_int_value)
	{
		SetScreenResolution(new_screen_width->m_int_value, new_screen_height->m_int_value, old_screen_width, old_screen_height);
	}

	if (KioskManager::GetInstance()->GetEnabled())
	{
		//kill off Ctrl+Alt+Del on Win9x systems, this will also disable screensavers!
		ULONG pOld;
		SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, &pOld, 0);
	}

	{
		OP_PROFILE_METHOD("Initialized hooks");
	
		InstallMSWinMsgHooks();
	}

	//This is a hack made to check if there is a newer update on the server before autoupdating
	//It will eventually have to be replaced by a cross-platform solution
	//This will post a WM_OPERA_CREATED message if CheckUpdateVersion is successful
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIAutoupdate) == NULL ||
		(g_version_checker = OP_NEW(VersionChecker, ())) == NULL ||
		OpStatus::IsError(g_version_checker->Init(&g_windowsVersionCheckerListener)) ||
		OpStatus::IsError(g_version_checker->CheckRemoteVersion()))
	{
	//	PLEASE NOTE:  This has to be THE LAST STATEMENT GENERATING MESSAGES above the message loop.
		PostMessage(g_main_hwnd, WM_OPERA_CREATED, 0, 0);
	}
	
	// Disable shell tray, must be reenabled on exit
	if (KioskManager::GetInstance()->GetNoExit())
	{
		HWND shell_traywnd = FindWindowA("Shell_traywnd", NULL);
		if (shell_traywnd)
		{
			EnableWindow(shell_traywnd, FALSE);
		}
	}

	int delay_shutdown = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DelayShutdown)?CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DelayShutdown)->m_int_value : 0;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	//	ENTER MESSAGE LOOP
	//

	//The message loop is about to be started and we will respond to DDE messages -> clear the mutex
	ReleaseMutex(g_no_dde_mutex);
	CloseHandle(g_no_dde_mutex);
	g_no_dde_mutex = NULL;

	HANDLE evt = OpenEvent(EVENT_MODIFY_STATE, FALSE, UNI_L("Opera Main Window Event"));
	if(evt)
	{
		// this event is used for the external QA tool "TimeOperaStartup.exe" that measures the startup time of Opera. If the event exists, Opera
		// was started from the tool
		SetEvent(evt);
		CloseHandle(evt);
	}

	g_windows_message_loop->Run();

	//Indicates that the message loop was stopped and we will not receive DDE messages anymore. We will soon be stopped.
	g_no_dde_mutex = CreateMutexA(NULL, TRUE, g_stopping_signature.CStr());
	if(!g_no_dde_mutex)
	{
		// might be created by the message loop ExitStarted() already
		g_no_dde_mutex = CreateMutexA(NULL, TRUE, g_stopping_signature.CStr());
	}

#ifdef _DDE_SUPPORT_
	delete ddeHandler;
	ddeHandler = NULL;
#endif

	// Reenabled shell tray on exit
	if (KioskManager::GetInstance()->GetNoExit())
	{
		HWND shell_traywnd = FindWindowA("Shell_traywnd", NULL);
		if (shell_traywnd)
		{
			EnableWindow(shell_traywnd, TRUE);
		}
	}

	if (delay_shutdown)
		Sleep(delay_shutdown * 1000);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	//	CLEAN UP AND EXIT
	//
	if (old_screen_width && old_screen_height)
	{
		int dummy;
		SetScreenResolution(old_screen_width, old_screen_height, dummy, dummy);
	}

	OleUninitialize();

#ifdef _ACTIVATE_TIMING_
	__DO_FLUSH_TIMING();
#endif // _ACTIVATE_TIMING_

	if(KioskManager::GetInstance()->GetResetOnExit())
	{
		ResetStation();         //clear personaldata
	}

#ifndef NS4P_COMPONENT_PLUGINS
	OP_DELETE(g_delayed_flash_message_handler);
#endif // !NS4P_COMPONENT_PLUGINS

	// It is not safe to delete g_thread_tools yet, because some worker threads (e.g. host resolvers)
	// may be inside OpMainThread::PostMessage.
	// Just seal the object and it will be deleted after g_opera and network managers are shut down.
	MainThreadSender::GetInstance()->Finalize();

	{
		OP_PROFILE_MSG("Destroying core starting");
		OP_PROFILE_METHOD("Destroying core completed");

		g_opera->Destroy();
	}

	OperaInstaller::DeleteProfileEffective();

#ifdef _DEBUG
	Debug::Free();
#endif // _DEBUG
	if (g_opera_processes_list)
		g_opera_processes_list->Destroy();
	OP_DELETE(g_component_manager);
	g_component_manager = NULL;
	g_opera = NULL;

	delete g_windows_message_loop;
	g_windows_message_loop = NULL;

	WindowsOpComponentPlatform::UnregisterWNDCLASS();

	delete WindowsSocket2::m_manager;
	g_windows_host_resolver_manager->ShutDown();
	mswin_clear_locale_cache();

	// All worker threads that may use g_thread_tools should be terminated by now.
	OP_DELETE(g_thread_tools);
	g_thread_tools = NULL;
	MainThreadSender::Destruct();

	//Another instance can start after this happened
	ReleaseMutex(mutex);
	CloseHandle(mutex);
	ReleaseMutex(g_no_dde_mutex);
	CloseHandle(g_no_dde_mutex);
	g_no_dde_mutex = NULL;

#ifdef OPMEMORY_EXECUTABLE_SEGMENT

	extern HANDLE g_executableMemoryHeap;

	// heap created for carakan in WindowsOpMemory.cpp
	if(g_executableMemoryHeap)
	{
		HeapDestroy(g_executableMemoryHeap);
		g_executableMemoryHeap = NULL;
	}
#endif // OPMEMORY_EXECUTABLE_SEGMENT

	OleUninitialize();

	OP_PROFILE_EXIT();

	return 0;
}


// *************************************************************
//            UpdateProxyFromSystemL
// 
// Update proxy settings from OS if SlipStream accelerator is
// running on the computer. See www.slipstreamdata.com
// 
// *************************************************************

static BOOL can_update_proxy_from_system = TRUE;

void ResetProxySettingsL()
{
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UseHTTPProxy);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UseHTTPSProxy);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UseFTPProxy);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UseGopherProxy);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::UseWAISProxy);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig);
	g_pcnet->EnableAutomaticProxy(TRUE);
#endif

	g_pcnet->ResetStringL(PrefsCollectionNetwork::HTTPProxy);
	g_pcnet->ResetStringL(PrefsCollectionNetwork::HTTPSProxy);
	g_pcnet->ResetStringL(PrefsCollectionNetwork::FTPProxy);
	g_pcnet->ResetStringL(PrefsCollectionNetwork::GopherProxy);
	g_pcnet->ResetStringL(PrefsCollectionNetwork::WAISProxy);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	g_pcnet->ResetStringL(PrefsCollectionNetwork::AutomaticProxyConfigURL);
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::EnableHTTP11ForProxy);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::HasUseProxyLocalNames);

	g_pcnet->ResetStringL(PrefsCollectionNetwork::NoProxyServers);
	g_pcnet->ResetIntegerL(PrefsCollectionNetwork::HasNoProxyServers);
}

BOOL UpdateProxyFromSystemL(BOOL first_time)
{
	BOOL ret = FALSE;

	BOOL use_proxy_server_http = FALSE;
	OpString http_proxy_server_from_system;

	g_op_system_info->GetProxySettingsL(UNI_L("http"), use_proxy_server_http, http_proxy_server_from_system);

	OpString http_proxy_server;
	TRAPD(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HTTPProxy, http_proxy_server));
	
	if (first_time)
	{
		if (http_proxy_server.HasContent() && http_proxy_server_from_system.CompareI(http_proxy_server) != 0)
		{
			can_update_proxy_from_system = FALSE;
		}

		HANDLE connected_event = OpenEvent(EVENT_ALL_ACCESS, FALSE,UNI_L("SlipStreamAcceleratorStatus"));
		
		if(connected_event != NULL)
		{
			if (WaitForSingleObject(connected_event, 0) == WAIT_OBJECT_0)
			{
				if (can_update_proxy_from_system)
				{
					ResetProxySettingsL();
				}
				ret = TRUE;
			}
			CloseHandle(connected_event);
		}
	}
	else
	{
		if (http_proxy_server.HasContent() && http_proxy_server_from_system.HasContent() && 
			http_proxy_server_from_system.CompareI(http_proxy_server) == 0)
		{
			can_update_proxy_from_system = TRUE;
		}

		if (can_update_proxy_from_system)
		{
			ResetProxySettingsL();
			g_application->SettingsChanged(SETTINGS_PROXY);
		}
	}
	return ret;
}


// *************************************************************
//            INIT & EXIT FOR MAIN WINDOW
// *************************************************************
LRESULT BLD_CALLBACK DLLWndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);

BOOL BLDMainRegClass(HINSTANCE hInstance, BOOL use_dll_main_window_proc)
{
    WNDCLASS    WndClass;

    WndClass.style         = CS_DBLCLKS;  //CS_OWNDC;

	if(use_dll_main_window_proc)
	{
		WndClass.lpfnWndProc   = DLLWndProc;		
	}
	else
	{
		WndClass.lpfnWndProc   = BLDMainWndProc;
	}
    
	WndClass.cbClsExtra    = 0;
    WndClass.cbWndExtra    = 0;
    WndClass.hInstance     = hInstance;
    WndClass.hIcon         = LoadIcon(hInstance, UNI_L("OPERA"));
    WndClass.hCursor       = LoadCursor(NULL,IDC_ARROW);
    WndClass.hbrBackground = NULL;
    WndClass.lpszMenuName  = 0;
    WndClass.lpszClassName = UNI_L("Opera Main Window");

    return RegisterClass(&WndClass);
}

/*
   The "Application Shutdown Changes in Windows Vista"
   <http://msdn.microsoft.com/en-us/library/ms700677(v=vs.85).aspx> describes the need
   to both negotiate longer blocking timeouts for WM_QUERYENDSESSION/WM_ENDSESSION
   as well as actually shutdown inside the WM_ENDSESSION time frame.
  
   The XP SP3 in itself does not have the API for that negotation, but it will grant
   the timeouts if there is _any_ top-level window. For that we can use the window we
   would use on Vista for negotiating the block. Hence, the negotiation code is relaxed
   to make sure we have a window not only on Vista and later, but also on older systems.
 */

class WindowsShutdownBlockReason
{
	HWND m_shutdown;
	ATOM m_class;
public:
	WindowsShutdownBlockReason()
		: m_shutdown(NULL)
		, m_class(0)
	{
	}

	void Init();
	void Negotiate();
	static void TerminateOpera();
};

void WindowsShutdownBlockReason::Init()
{
	WNDCLASS WndClass;
	memset(&WndClass, 0, sizeof(WndClass));

	WndClass.style         = CS_DBLCLKS;
	WndClass.lpfnWndProc   = DefWindowProc;		
	WndClass.hInstance     = hInst;
	WndClass.hIcon         = LoadIcon(hInst, UNI_L("OPERA"));
	WndClass.lpszClassName = UNI_L("Opera Shutdown Window");

	m_class = RegisterClass(&WndClass);

	OP_ASSERT(m_class != 0);
}

void WindowsShutdownBlockReason::Negotiate()
{
	// For Windows Vista+, we need a hidden window and the ShutdownBlockReasonCreate
	// For Windows XP, we need only the window

	OpString message;
	const uni_char* title = NULL;

	{
		OpString format;
		OpStatus::Ignore(g_languageManager->GetString(Str::S_BREAM_IS_EXITING, format));
		OpStatus::Ignore(message.AppendFormat(format.CStr(), UNI_L("Opera")));
	}
	if (g_application)
	{
		BrowserDesktopWindow* desktop = g_application->GetActiveBrowserDesktopWindow();
		if (desktop)
		{
			title = desktop->GetTitle();
		}
	}
	if (!title) title = UNI_L("Opera");

	const uni_char* msg = message.HasContent() ? message.CStr() : UNI_L("Opera");
	if (!m_shutdown && m_class)
		m_shutdown = CreateWindowEx(0, MAKEINTATOM(m_class), title, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);

	if (IsWindow(m_shutdown))
		OPShutdownBlockReasonCreate(m_shutdown, msg);
}

void WindowsShutdownBlockReason::TerminateOpera()
{
	// The only time we are guaranteed we get for cleanup as of Vista/Se7en
	// is during this function (during WM_ENDSESSION to be exact). So, we must
	// initialize exiting, get all the messages out of the way and finally
	// destroy the rest of the Core.
	g_desktop_global_application->Exit();
	g_windows_message_loop->Run();
	g_opera->Destroy();

	// if someone else canceled the shutdown, we are left
	// with half-destroyed core, and we will either get
	// stuck in the outer Run, or get crashed in OperaEntry
	// due to the g_opera->Destroy above
	TerminateProcess(GetCurrentProcess(), 0);
}

static WindowsShutdownBlockReason s_windows_shutdown_block_reason;

// *************************************************************
//            WINDOW PROCEDURE FOR MAIN WINDOW
// *************************************************************

LRESULT BLD_CALLBACK BLDMainWndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    LRESULT        lReturn;

    lReturn     = 0;

    switch (message)
    {
    case WM_CREATE:
		{
			g_main_hwnd = hWnd;

			if (OnCreateMainWindow())
			{
				// success
				s_windows_shutdown_block_reason.Init();
				return 0;
			}

			// failure
			return -1;
		}

    case WM_DESTROY:
		{
			//kill off Ctrl+Alt+Del on Win9x systems, this will also disable screensavers!
			ULONG pOld;
			SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, &pOld, 0);

			OnDestroyMainWindow();

			return 0;
		}

    case WM_CLOSE:
        return ucClose(hWnd, TRUE);

	case WM_SETTINGCHANGE:
		extern void OnSettingsChanged();
		OnSettingsChanged();
		//kindof hack
		//when an international setting is changed, this makes our
		//cached version of setlocale update its cache (see system.h)
		if (!wParam && lParam && !uni_strncmp(UNI_L("intl"),(LPCTSTR)lParam,5))
		{
			op_setlocale(LC_ALL, "reset_cache");
			((WindowsOpLocale*)g_oplocale)->InitLocale();
		}
		return 0;

    case WM_DRAWITEM:
        return ucWMDrawItem(hWnd,message,wParam,lParam);

    case WM_MEASUREITEM:
        return ucMeasureItem(hWnd,message,wParam,lParam);

	// All these were put here because I don't yet get messages
	// hooks when submenus are opened.

	// These fire when submenus of context menus open and close
	// but also in other cases:
	//
	case WM_INITMENUPOPUP:
		// Get this event for popup menus and their submenus
		// Not for main menus and their submenus
		// Want this call to happen for submenus of popup menus
		if (wParam)
		{
			g_windows_menu_manager->OnMenuShown(wParam, LOWORD(lParam)); // Pos (lParam) is including separators
			return 0;
		}
	case WM_UNINITMENUPOPUP:
		g_windows_menu_manager->OnMenuClosed(wParam);
		return 0;
	case WM_MENUCHAR:
        return g_windows_menu_manager->OnMenuChar(LOWORD(wParam), (HMENU) lParam);

    case WM_TIMER:
	     return ucTimer(hWnd,message,wParam,lParam);

	case WM_TIMECHANGE:
		// Force the std library to update the timezone, which it normally only does once
		((WindowsOpTimeInfo *)g_op_time_info)->OnSettingsChanged();
		return 0;

	case WM_SYSCOLORCHANGE:
		break;

	case WM_POWERBROADCAST:
		if (wParam == PBT_APMSUSPEND)
		{
			g_sleep_manager->Sleep();
		}
		else if (wParam == PBT_APMRESUMESUSPEND)
		{
			g_sleep_manager->WakeUp();
		}
		break;

    case WM_QUERYENDSESSION:
		s_windows_shutdown_block_reason.Negotiate();
		return TRUE; // Success

    case WM_ENDSESSION:
    	if (wParam)
		{
			s_windows_shutdown_block_reason.TerminateOpera();
			return 0; // Handled
		}
		return 1; // Not handled

	case WM_OPERA_DELAYED_CLOSE_WINDOW:
		{
#ifdef _PRINT_SUPPORT_
			((OpBrowserView*)((Window*)lParam)->GetWindowCommander()->GetPrintingListener())->Delete();
#endif // _PRINT_SUPPORT_
			break;
		}

	case WM_OPERA_CREATED:	
		{
			BOOL go_on = FALSE;		// Will restart opera if go_on is TRUE
			BOOL minimal = FALSE;	// Will restart without HW accel if minimal is TRUE

			// whether a crash log upload is intended
			CommandLineArgument *crash_log = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog);

			if (crash_log)
			{
				if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCrashLogUploadDlg))
				{
					// show crash report dialog
					CrashLoggingDialog* dialog = OP_NEW(CrashLoggingDialog, (crash_log->m_string_value, go_on, minimal));
					int show_main = DefShowWindow;	
					DefShowWindow = 0;
					dialog->Init(NULL);
					DefShowWindow = show_main;
				}

				g_desktop_global_application->Exit();

				if (go_on)
				{
					OpString opera_path;
					OpString opera_to_execute;

					if (OpStatus::IsError(GetExePath(opera_path)))
						break;
					if (OpStatus::IsError(opera_to_execute.AppendFormat(UNI_L("\"%s\""), opera_path.CStr())))
						break;

					const uni_char* parameters = minimal ? UNI_L("/nohwaccel /nosession") : NULL;

					ShellExecute(NULL, NULL, opera_to_execute.CStr(), parameters, NULL, SW_SHOW);
				}

				break;
			}

			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ReinstallBrowser))
			{
				OperaInstaller installer;
				if (installer.AssociationsAPIReady())
					installer.SetDefaultBrowserAndAssociations(TRUE);
				g_desktop_global_application->Exit();
			}

			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ReinstallMailer))
			{
				OperaInstaller installer;
				if (installer.AssociationsAPIReady())
				{
					installer.SetDefaultMailerAndAssociations(TRUE);
				}
				g_desktop_global_application->Exit();
			}

			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ReinstallNewsReader))
			{
				OperaInstaller installer;
				if (installer.AssociationsAPIReady())
					installer.SetNewsAssociations(TRUE);
				g_desktop_global_application->Exit();
			}

			//This is a hack made to allow the user to confirm that they want to install a new update
			//It will eventually have to be replaced by a cross-platform solution.
			OpAutoPtr<OperaInstaller> installer(OP_NEW(OperaInstaller, ()));
			RETURN_VALUE_IF_NULL(installer.get(), -1); // OOM
			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIAutoupdate))
			{
				VersionChecker::VCStatus status = VersionChecker::VCSInitFailed;

				// It is possible that we find ourselves here while the g_version_checker is NULL.
				if (g_version_checker)
					status = g_version_checker->GetStatus();

				OP_DELETE(g_version_checker);
				OP_ASSERT(status != VersionChecker::VCSNotInitialized); // It should have been initialized before running the message loop
				OP_ASSERT(status != VersionChecker::VCSCheckNotPerformed); // ... and if it was initialized, the version check should also be done
				BOOL install_update = FALSE;

				// Ask the user if (s)he wants to install update only if the version check failed,
				// if the update ready to be downloaded is exactly the same the one we already
				// have, or the update we have is not older than VersionChecker::MaxDownloadedPackageAgeDays days.
				if (status == VersionChecker::VCSSameUpdateAvailable ||
					status == VersionChecker::VCSInitFailed ||
					status == VersionChecker::VCSCheckVersionFailed ||
					status == VersionChecker::VCSCheckNotNeeded)
				{
					// OK, we want to use the update we have got downloaded.

					// Check if the autoupdate.txt file tells us to disable to AutoUpdateReadyController dialog,
					// that is if the "show_information" flag is set to 0.
					BOOL show_information = TRUE;
					AUDataFileReader dataFileReader;
					if (OpStatus::IsSuccess(dataFileReader.Init()) && OpStatus::IsSuccess(dataFileReader.Load()))
						show_information = dataFileReader.ShowInformation();

					if (!show_information)
					{
						// Yup, the flag was set to 0. That is because the user clicked "Install now" in the UpdateAvailableDialog dialog.
						// Therefore:
						// 1. Install the update without displaying the AutoUpdateReadyController dialog;
						install_update = TRUE;

						// 2. Set the flag back to 1, in case that for any reason we will try to install the update again the next time
						// Opera is started (i.e. user clicked No in the UAC dialog?).
						AUDataFile dataFile;
						if (OpStatus::IsSuccess(dataFile.Init()) && OpStatus::IsSuccess(dataFile.LoadValuesFromExistingFile())) {
							dataFile.SetShowInformation(TRUE);
							// Not much to do with the error anyway.
							OpStatus::Ignore(dataFile.Write());
						}
					}
					else
					{
						// Show the AutoUpdateReadyController dialog.
						// Check if we need the UAC shield in it.
						BOOL needs_elevation = FALSE;
						if (!WindowsUtils::IsPrivilegedUser(FALSE))
						{
							OperaInstaller::Settings settings = installer->GetSettings();
							if (settings.all_users)
								needs_elevation = TRUE;
							else if (OpStatus::IsError(installer->PathRequireElevation(needs_elevation)))
								needs_elevation = TRUE;
						}

						AutoUpdateReadyController* controller = OP_NEW(AutoUpdateReadyController, (needs_elevation));
						if (controller)
						{
							SimpleDialogController::DialogResultCode res;
							controller->SetBlocking(res);
							OP_STATUS stat = ShowDialog(controller,g_global_ui_context,NULL);

							if (OpStatus::IsSuccess(stat) && res == SimpleDialogController::DIALOG_RESULT_OK)
								install_update = TRUE;
						}
					}
				}

				if (!install_update)
				{ // The user didn't agree to install the update
					g_desktop_global_application->Exit();
					break;
				}
			}

			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall) || CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIUninstall))
			{
				OperaInstaller* installer_ptr = installer.release();

				OP_STATUS err;
				if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISilent))
					err = installer_ptr->Run();
				else
					err = installer_ptr->RunWizard();

				if (OpStatus::IsError(err))
				{
					OP_DELETE(installer_ptr);
					g_desktop_global_application->Exit();
				}
				
				break;
			}
			OP_PROFILE_MSG("Opera message loop running");

			g_main_message_handler->PostDelayedMessage(MSG_QUICK_APPLICATION_START, 0, 0, 0);

			return 0;
	}

	case WM_NCDESTROY:
	{
	    RemoveMSWinMsgHooks();
		return 0;
	}

    case WM_COMMAND:
		return g_windows_menu_manager && g_windows_menu_manager->OnMenuCommand(LOWORD(wParam));

#ifdef WIN32		// only makes sense on 32bit
	case WM_BAD_DLL:
	{
		uni_char module_name[MAX_PATH];
		UINT DLL_base = lParam & ~0xFFFF;
		UINT retries = DLL_base ? 0x400 : 0;
		while (retries && !GetModuleFileName((HMODULE)DLL_base, module_name, MAX_PATH))
		{
			DLL_base -= 0x10000;
			retries--;
		}
		if (!retries)
			uni_strcpy(module_name, UNI_L("unknown"));

		OpString dlg_title, dlg_text;
		g_languageManager->GetString(Str::D_BAD_DLL_WARNING_TITLE, dlg_title);
		g_languageManager->GetString(Str::D_BAD_DLL_WARNING_TEXT, dlg_text);
		dlg_text.Append(module_name);
		
		SimpleDialog* dialog = new SimpleDialog;
		if (dialog)
			dialog->Init(WINDOW_NAME_BAD_DLL, dlg_title, dlg_text, 0, Dialog::TYPE_OK);

		break;
	}
#endif // WIN32

    default:
		
		if (message && message == MSG_READ_PROXY_SETTINGS_FROM_REGISTRY)
		{
			//OP_ASSERT(0); // We got message from proxy software (SlipStream)
			TRAPD(err, UpdateProxyFromSystemL(FALSE));
			return 0;
		}

    	if (hWnd == g_main_hwnd && g_main_message_handler && (message >= WM_USER) && (message != idMsgLostCursor) ) // The (message >= WM_USER) test checks if this is a message posted by opera.
																												// idMsgLostCursor is handled explicitly as it is defined run-time.
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
				case 49378:
				case 49364:
				case 49363:
				{
					// This is the message posted every time an explorer window is opened. We don't intend
					// To do anything with the message, so we just ignore it.
					break;
				}
				default:
				{
					//OP_ASSERT(0);
					// Note by mariusab 20051222:
					// If this hits, a message in user space has been posted that is not properly handled when it returns.
					// The PostDelayedMessage callback requires OpMessage-messages, and these don't have set values. 
					// Mapping between windows user space messages and OpMessages must be done explicitly.
					break;
				}
			}
    		return 0;
    	}
    	else
		{
        	return DefWindowProc(hWnd, message,wParam,lParam);
		}

	}

    return FALSE;
}

//  Creates windows at Initialization
HWND BLDCreateWindow(HINSTANCE hInstance)
{
	HWND	hWnd;				// window handle
	int		coordinate[4];		// Coordinates of main window
	DWORD	WndStyle;			// Style for main window
	DWORD	WndStyleEx;			// Extended Style for main window
	PSTR	pInfo;				// Points to Info sent with WM_CREATE
	
	//  Initial value of main window parameters
	WndStyle	= WS_CAPTION | WS_OVERLAPPED | WS_CLIPCHILDREN;
	WndStyleEx	= 0;
	pInfo		= NULL;
	coordinate[0]=CW_USEDEFAULT;
	coordinate[1]=0;
	coordinate[2]=CW_USEDEFAULT;
	coordinate[3]=0;

	hWnd = CreateWindowExA(WndStyleEx, //  window Extended style
			"Opera Main Window",	//  WndClass registered earlier
			"Opera",			//  window title in title bar
			WndStyle,			//  window style
			coordinate[0],		//  x position
			coordinate[1],		//  y position
			coordinate[2],		//  width
			coordinate[3],		//  height
			0,					//  parent handle
			0,					//  menu or child ID
			hInst,				//  instance
			(LPSTR)pInfo);		//  additional info

	return hWnd;
}

/***********************************************************************************
**
**	HandleOldBrowserWin32Actions
**
***********************************************************************************/

BOOL HandleOldBrowserWin32Actions(OpInputAction* action, Window* window)
{
	HWND hwnd = NULL;

	DesktopWindow* desktop_window = g_application->GetDesktopWindowFromAction(action);

	if (desktop_window)
	{
		hwnd = ((WindowsOpWindow*) desktop_window->GetWindow())->m_hwnd;
	}
	if (!hwnd && window)
	{
		WindowsOpWindow *src_win = (WindowsOpWindow *)window->GetOpWindow();
		if(src_win)
		{
			hwnd = src_win->m_hwnd;
		}
		hwnd = GetParent(hwnd);
	}
	if (!hwnd && g_application->GetActiveDesktopWindow())
		hwnd = ((WindowsOpWindow*)g_application->GetActiveDesktopWindow()->GetWindow())->m_hwnd;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PRINT_DOCUMENT:
		{
			if (window)
			{
#ifdef _PRINT_SUPPORT_
				cdPrint(window, FALSE);
#endif // _PRINT_SUPPORT_
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_PRINT_SETUP:
		{
#ifdef _PRINT_SUPPORT_
	        cdPrintSetup(hwnd);
#endif // _PRINT_SUPPORT_
			return TRUE;
		}
	}

	return FALSE;
}

