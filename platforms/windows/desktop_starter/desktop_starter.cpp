
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "desktop_starter.h"

#include "platforms/windows/windows_ui/res/#BuildNr.rci"

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/updater/auupdater.h"
#include "adjunct/autoupdate/updater/pi/auinstaller.h"
#include "adjunct/autoupdate/updater/pi/aufileutils.h"
#endif
#include "adjunct/quick/managers/LaunchManager.h"
#include "platforms/crashlog/crashlog.h"
#include "platforms/windows/utils/shared.h"
#include "platforms/windows/desktop_starter/dep.h"
#include "platforms/windows/desktop_starter/win_file_utils.h"


BOOL GetIntegrityLevel(DWORD process_id,DWORD& integrity_level)
{
	OSVERSIONINFOA osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	if (!GetVersionExA(&osvi) || osvi.dwMajorVersion < 6) // Older than Vista.
		return FALSE;

	integrity_level = 0;
	
	HANDLE hProcess = 0;
	HANDLE hToken = 0;
	DWORD size = 0;
	TOKEN_MANDATORY_LABEL* label = 0;

	do
	{
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id);
		if (!hProcess)
			break;

		if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
			break;
					
		if (!GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)TokenIntegrityLevel, NULL, 0, &size) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			break;
							
		label = (TOKEN_MANDATORY_LABEL*) malloc(size);
		if (!label)
			break;

		if (!GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)TokenIntegrityLevel, label, size, &size) || !IsValidSid(label->Label.Sid))
			break;

		integrity_level = *(GetSidSubAuthority(label->Label.Sid, *(GetSidSubAuthorityCount(label->Label.Sid))-1));
	} while (0);

	if (label)
		free(label);
	if (hToken)
		CloseHandle(hToken);
	if (hProcess)
		CloseHandle(hProcess);

	return (integrity_level != 0);
}

void RestartWithMediumIntegrityLevel()
{
	HANDLE hProcess = 0;
	HANDLE hToken = 0;
	HANDLE hDupToken = 0;
	PROCESS_INFORMATION proc_info = {0};
	STARTUPINFOW startup_info = {0};

	HWND hWnd_progman = ::FindWindowA("Progman", NULL);
	if (!hWnd_progman) // This should only happen if Explorer has crashed
		return;

	DWORD explorer_pid = 0;
	GetWindowThreadProcessId(hWnd_progman, &explorer_pid);
	if (!explorer_pid)
		return;

	//If explorer has a high integrity level, it means UAC is turned off, so no need to restart.
	DWORD integrity_level;
	if (!GetIntegrityLevel(explorer_pid, integrity_level) || integrity_level > SECURITY_MANDATORY_MEDIUM_RID)
		return;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, explorer_pid);
	if (!hProcess)
		return;

	do
	{
		if (!OpenProcessToken(hProcess, TOKEN_DUPLICATE, &hToken))
			break;

		if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS | TOKEN_ADJUST_SESSIONID, 0, SecurityImpersonation, TokenPrimary, &hDupToken))
			break;

		if (!OPCreateProcessWithTokenW(hDupToken, 0, NULL, GetCommandLine(), 0, 0, 0, &startup_info, &proc_info))
			break;

		ExitProcess(0);
	} while (0);

	if (hToken)
		CloseHandle(hToken);
	if (hDupToken)
		CloseHandle(hDupToken);
	if (hProcess)
		CloseHandle(hProcess);

	return;
}

UINT_PTR simple_hextoint(char *str)
{
	if (!str)
		return 0;

	UINT_PTR retval = 0;
	unsigned char c;
	while ((c = *str) != 0)
	{
		// Make alphabetical characters lowercase (does not affect digits).
		c |= 0x20;

		// If we have a digit || letter (c-('a'-'0'-0xA)) && letter is not bigger than f.
		if ((c -= '0') <= 9 || ((c -= 0x27) >= 10 && c <= 15))
			retval = retval << 4 | c; // Multiply by 16 and add the value of the current digit.
		else
			break; // Break on other characters or space.

		str++;
	}
	return retval;
}


char* GetNextWord(char* str)
{
	while (*str && *str != ' ')
		++str;

	while (*str && *str == ' ')
		++str;

	return str;
}
#ifdef WIDGET_RUNTIME_SUPPORT

const uni_char DEFAULT_DLL_PATH[] = UNI_L("Opera.dll");
const uni_char BROWSER_EXE_NAME[]=  UNI_L("opera.exe");
const uni_char KNOWN_BROWSER_EXE_NAME[2][20] = { {UNI_L("opera.exe")},{UNI_L("operaupgrader.exe")}};
const uni_char GADGET_INSTALL_CONF_NAME[] = UNI_L("install.conf");

BOOL IsBrowserExePath(const uni_char* path)
{	 
	const uni_char *file_name = uni_strrchr(path, '\\');
	file_name = (NULL != file_name) ? (file_name + 1) : (path);
	const uni_char *file_name_backup = file_name;

	// inlined uni_stricmp
	const uni_char* browser_name;
	for (UINT32 i = 0; i < ARRAY_SIZE(KNOWN_BROWSER_EXE_NAME); i++)
	{
		browser_name = KNOWN_BROWSER_EXE_NAME[i];
		while (*browser_name)
		{
			if ((*browser_name | 0x20) != (*file_name | 0x20))
			{
				break;
			}

			++file_name;
			++browser_name;
		}
		if (!*file_name)
			return TRUE;

		file_name = file_name_backup;
	}

	return FALSE;
}


BOOL GetOperaPathsFromGadget(uni_char* exe_path, uni_char* dll_path)
{
	uni_char *exe_name = uni_strrchr(exe_path, '\\');
	exe_name = (NULL != exe_name) ? (exe_name + 1) : (exe_path);
	*exe_name = 0;

	uni_char config_path[MAX_PATH];
	uni_strncpy(config_path, exe_path,
			MAX_PATH - ARRAY_SIZE(GADGET_INSTALL_CONF_NAME));
	uni_strcat(config_path, GADGET_INSTALL_CONF_NAME);

	char* file_data = NULL;
	size_t file_size = size_t(-1);
	if (WinFileUtils::Read(config_path, &file_data, file_size) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	// Start with a 2-byte offset: skip the BOM.
	const uni_char* dir_path = (const uni_char*)(file_data + 2);
	const uni_char* dir_path_end = uni_strchr(dir_path, '\n');

	// Limit the directory path length so that the entire path fits in MAX_PATH.
	const size_t max_dir_path_length = MAX_PATH
			- MAX(ARRAY_SIZE(BROWSER_EXE_NAME), ARRAY_SIZE(DEFAULT_DLL_PATH));
	const size_t dir_path_length =
			MIN(dir_path_end - dir_path, max_dir_path_length);

	uni_strncpy(exe_path, dir_path, dir_path_length);
	uni_strcpy(exe_path + dir_path_length, BROWSER_EXE_NAME);

	uni_strncpy(dll_path, dir_path, dir_path_length);
	uni_strcpy(dll_path + dir_path_length, DEFAULT_DLL_PATH);

	delete [] file_data;
	return TRUE;
}


/**
 * Initializes the paths to the Opera executable and main DLL.
 *
 * @param exe_path Receives the path to the executable to be set as the spawner.
 * 		The buffer must be large enough to accommodate any path name (MAX_PATH
 * 		or more).
 * @param dll_path Receives the path to Opera.dll.
 * 		The buffer must be large enough to accommodate any path name (MAX_PATH
 * 		or more).
 * @return @c TRUE on success
 */
BOOL InitOperaPaths(uni_char* exe_path, uni_char* dll_path)
{
	// Set up the defaults.

	uni_strcpy(dll_path, DEFAULT_DLL_PATH);

	const DWORD exe_path_length = GetModuleFileName(0, exe_path, MAX_PATH);
	if (!exe_path_length || exe_path_length == MAX_PATH)
	{
		return FALSE;
	}

	// If we're the browser, we're done.
	if (IsBrowserExePath(exe_path))
	{
		return TRUE;
	}
	
	return GetOperaPathsFromGadget(exe_path, dll_path);
}
#endif // WIDGET_RUNTIME_SUPPORT

/**
 * This function is called for each toplevel window in system after the call to EnumWindows().
 * The idea is to find the AutoUpdateDialog window and bring user's attention to it, 
 * taking the window to front of the other windows and/or flashing it.
 * Since we can't really know the ID of the process that opened the dialog, and since we can't
 * rely on translated strings to compare the window title, we find any window that has the
 * OperaWindowClass class and doesn't have the maximize window title button.
 *
 * This function returns TRUE if EnumWindows() should call it again for the next window in the system,
 * FALSE otherwise.
 */
BOOL CALLBACK FindAutoUpdaterDialog(HWND hwnd, LPARAM)
{
	char clsName[32];
	GetClassNameA(hwnd, clsName, sizeof(clsName));
	if(strcmp(clsName, "OperaWindowClass") == 0)
	{
		WINDOWINFO info;
		info.cbSize = sizeof(info);
		GetWindowInfo(hwnd, &info);

		// This is some other Opera window, keep looking
		if (info.dwStyle & WS_MAXIMIZEBOX)
			return TRUE;

		// We have found the dialog, hopefully, now bring it to front or at least get user's attention
		FlashWindow(hwnd, TRUE);
		SetActiveWindow(hwnd);
		SetForegroundWindow(hwnd);
		// We're done.
		return FALSE;
	}

	// Keep looking
	return TRUE;
}


int g_null_mem = 0;

// DSK-365815
// The symptom of this bug is that all registers are messed up, including the one used to compare against null.
// When this occurs, the following if-test will be true, and the forced crash will tell us WTF happened

#define CHECK_NULL_REGISTER 	if (g_null_mem) forced_crash();

#ifndef _WIN64
__declspec(naked)
#endif
void forced_crash()
{
	// push some API pointers to the stack, this way enough code bytes will appear in the crash log memory dump
	// to be able to tell whether some broken API hook causes this
#ifndef _WIN64
	_asm mov esi, DeleteFile
	_asm add esi, 15
	_asm push esi
	_asm add esi,32
	_asm push esi
	_asm mov esi, RemoveDirectory
	_asm add esi, 15
	_asm push esi
	_asm add esi,32
	_asm push esi
#endif
	*(char *)0 = 0;
}

HINSTANCE	hInst;
HWND		g_main_hwnd = NULL;
///////////////////////////////////////////////////////////

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	OSVERSIONINFOA osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	if (!GetVersionExA(&osvi) || osvi.dwMajorVersion < 6)
	{
		// Not needed on Vista and higher, where the NXCOMPAT exe flag takes care of this
		SetEnableDEP();
	}

	g_CmdLine = lpCmdLine;

	if (str_begins_with(lpCmdLine, "-unelevate"))
	{
		DWORD integrity_level;
		if (GetIntegrityLevel(GetCurrentProcessId(), integrity_level) && integrity_level > SECURITY_MANDATORY_MEDIUM_RID)
			RestartWithMediumIntegrityLevel();
	}
	                                                                        // How the commandline might look like, with "-write_crashlog <pid> <g_gpu_info address>":
	char* char_ptr = str_begins_with(lpCmdLine, "-write_crashlog");         // lpCmdLine = "-write_crashlog 4204 4884672"
	if (char_ptr)
	{
		bool no_restart = *char_ptr == '!';                                 // char_ptr = " 4204 4884672"
		DWORD process_id = *char_ptr ? simple_hextoint(++char_ptr) : 0;     // char_ptr = "4204 4884672"
		char_ptr = GetNextWord(char_ptr);                                   // char_ptr = "4884672"
		GpuInfo* gpu_info = (GpuInfo*)simple_hextoint(char_ptr);

		char filename[MAX_PATH] = {};
		UINT len_path = WriteCrashlog(process_id, gpu_info, NULL, filename);


		if (no_restart)
		{
			static char crash_msg[] = "Opera crashed while trying to show the crash dialogue for a previous crash.\nA crash log was created here:\n%s\nPlease send us this log manually.";
			char *msg_buf = new char[len_path + ARRAY_SIZE(crash_msg)-2];
			if (msg_buf)
			{
				wsprintfA(msg_buf, crash_msg, filename);
				MessageBoxA(0, msg_buf, "Opera Crash Logging", 0);
				delete[] msg_buf;
			}
		}
		else
		{
			lpCmdLine = GetNextWord(char_ptr);

			BOOL is_installer = (*lpCmdLine == '/' || *lpCmdLine == '-') && str_begins_with(lpCmdLine+1, "install");

			// Remove trailing slash if there is one.
			char *path_end = filename + len_path - 1;
			if (len_path && *path_end == '\\')
			{
					*path_end = 0;
					--len_path;
			}

			char* cmd_line = NULL;

			if (len_path)
			{
				static char format_string[] = "-crashlog \"%s\"";

				UINT cmdline_len = op_strlen(lpCmdLine);
				if (cmdline_len)
					cmdline_len++;

				len_path += cmdline_len + ARRAY_SIZE(format_string)-2;

				cmd_line = new char[len_path];
				if (!cmd_line)
					return 0;

				char *dest = cmd_line + wsprintfA(cmd_line, format_string, filename);
				if (*lpCmdLine)
				{
					*dest++ = ' ';
					op_strcpy(dest, lpCmdLine);
				}

				lpCmdLine = cmd_line;
			}

			if (GetModuleFileNameA(NULL, filename, MAX_PATH))
			{
				SHELLEXECUTEINFOA info = {};

				info.cbSize = sizeof(info);
				if (is_installer)
					info.fMask = SEE_MASK_NOCLOSEPROCESS;
				info.lpFile = filename;
				info.lpParameters = lpCmdLine;
				info.nShow = SW_NORMAL;

				// start a new Opera instance, which will present the crashlog upload dialogue and then continue
				if (ShellExecuteExA(&info) && info.hProcess)
				{
					//if the installer crashed, we should wait until the user is done viewing the crashlog dialog,
					//since closing this will cause the package to be cleaned up.
					WaitForSingleObject(info.hProcess, INFINITE);
					CloseHandle(info.hProcess);
				}
			}

			delete[] cmd_line;
		}

		// quit this instance, which will automatically terminate the crashed instance as well
		return 0;
	}

	SetUnhandledExceptionFilter(ExceptionFilter);

	hInst = hInstance;

	PF_OperaDesktopStart	OpStartDesktopInstance;
	PF_OperaSetSpawnerPath	OpSetSpawnerPath;
	PF_OperaSetLaunchMan	OpSetLaunchMan;
	PF_OperaGetNextUninstallFile OpGetNextUninstallFile;
	PF_OpWaitFileWasPresent	OpWaitFileWasPresent;
	PF_OpSetGpuInfo OpSetGpuInfo;

#ifdef AUTO_UPDATE_SUPPORT
	// DSK-345746 - terminate right away if the updater is already running.
	// Also, if there is the AutoUpdaterDialog waiting for user's input, bring it to user's attention.

	// DSK-345746: If there is an OperaUpgrader.exe running and there is NO mutex allowing Opera.exe to start,
	// then refuse to start and try to catch the user's attention with the OperaUpgrader.exe window.
	if (WindowsUtils::GetOperaProcess(true) != 0)
	{
		AUInstaller* au_installer = AUInstaller::Create();
		if (!au_installer)
			return 1;
		bool not_allowed_to_start = false;

		if (au_installer && !au_installer->IsUpgraderMutexPresent())
			not_allowed_to_start = true;

		delete au_installer;

		if (not_allowed_to_start)
		{
			// Play an exclamation system sound
			MessageBeep(MB_ICONEXCLAMATION);
			// Start looking for the AutoUpdaterDialog, if any
			EnumWindows(FindAutoUpdaterDialog, 0);
			// Exit now, don't bother to start another copy of self while upgrading
			return 0;
		}
	}

	// If the same opera instance is already running, then there is no reason to 
	// run the update check again, because that might start the installer, if the update is ready.
	if (!str_begins_with(lpCmdLine, "/ReinstallBrowser") && !str_begins_with(lpCmdLine, "/uninstall") && !str_begins_with(lpCmdLine, "/addfirewallexception") && !WindowsUtils::GetOperaProcess())
	{
		AUUpdater au_updater;
		if (!au_updater.Run())
			return 0;
	}

	AUFileUtils* au_fileutils = AUFileUtils::Create();
	if (!au_fileutils)
		return 1;

	BOOL is_updater;
	if (au_fileutils->IsUpdater(is_updater) == AUFileUtils::ERR)
		is_updater = TRUE;

	delete au_fileutils;

	//if we are running the updater, we should stop here regardless, otherwise,
	//we risk to start a proper opera instance from %TEMP%
	if (is_updater)
		return 0;

#endif


#ifdef WIDGET_RUNTIME_SUPPORT
	uni_char exe_path[MAX_PATH];
	uni_char dll_path[MAX_PATH];
	if (!InitOperaPaths(exe_path, dll_path))
	{
		MessageBoxA(NULL, "Failed to start application.", "Opera Error",
				MB_OK | MB_ICONERROR);
		return 0;
	}

	LPITEMIDLIST pidl;
	if (SHGetSpecialFolderLocation(0, CSIDL_DESKTOPDIRECTORY, &pidl) == NOERROR)
 	{
		uni_char mg_ini_path[MAX_PATH];
		if (SHGetPathFromIDList(pidl, mg_ini_path))
		{
			uni_char *mg_ini_path_end = mg_ini_path;
			while (*++mg_ini_path_end);
			UINT maxlen_fname = MAX_PATH - (mg_ini_path_end-mg_ini_path);

			static uni_char ini_files[] = UNI_L("\\MemGuard*.ini");
			if (ARRAY_SIZE(ini_files) <= maxlen_fname)
			{
				memcpy(mg_ini_path_end++, ini_files, sizeof(ini_files));
				WIN32_FIND_DATA fd;
				HANDLE fhandle = FindFirstFile(mg_ini_path, &fd);
				if (fhandle != INVALID_HANDLE_VALUE)
				{
					int choice = 0;
					do
					{
						UINT fname_len = uni_strlen(fd.cFileName);
						if (fname_len > maxlen_fname)
							continue;

						memcpy(mg_ini_path_end, fd.cFileName, UNICODE_SIZE(fname_len+1));
						HANDLE hnd = CreateFile(mg_ini_path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
						if (hnd == INVALID_HANDLE_VALUE)
							continue;

						DWORD bytes_read;
						char *mg_ini = new char[fd.nFileSizeLow+1];
						if (!mg_ini)
							continue;

						OpAutoArray<char> ap_mg_ini(mg_ini);
						mg_ini[fd.nFileSizeLow] = 0;
						BOOL read_ok = ReadFile(hnd, mg_ini, fd.nFileSizeLow, &bytes_read, 0);
						CloseHandle(hnd);
						if (!read_ok)
							continue;
#ifdef _WIN64
						char *args = str_begins_with(mg_ini, VER_NUM_STR "." VER_BUILD_NUMBER_STR ".x64");
#else
						char *args = str_begins_with(mg_ini, VER_NUM_STR "." VER_BUILD_NUMBER_STR);
						if (args && *args > ' ')
							continue;
#endif
						if (!args)
						{
							args = str_begins_with(mg_ini, "all");
							if (!args)
								continue;
						}

						if (*args == '*')
							args++;
						else
						{
							if (str_begins_with(lpCmdLine, "/install") ||
								str_begins_with(lpCmdLine, "/uninstall") ||
								str_begins_with(lpCmdLine, "-crashlog"))
							{
								break;
							}
						}

						if (*args == 'q')
						{
							choice = IDYES;
							args++;
						}

						if (!choice)
							choice = MessageBoxA(0, "Press Yes to run Opera in memory guarding mode\n"
								"to improve the information gathered when a crash is logged.\n"
								"This causes Opera to consume much more memory and run slower.\n"
								"Press No to start normally.\n"
								"Press Cancel to delete the file MemGuard.ini from your desktop\n"
								"to disable this message in the future.", "Opera MemGuard", MB_YESNOCANCEL);
							
						if (choice == IDYES)
						{
							if (!InitializeMemGuard(args))
							{
								DestroyMemGuard();
								MessageBoxA(0, "Couldn't initialize MemGuard, address space exhausted!", "Opera Error", MB_OK);
								break;
							}
							continue;
						}
						else if (choice == IDCANCEL)
						{
							DeleteFile(mg_ini_path);
						}
						break;
					} while (FindNextFile(fhandle, &fd));
					FindClose(fhandle);
				}
			}
		}
		CoTaskMemFree(pidl);
	}

	if (!str_begins_with(lpCmdLine, "-newprocess "))
		WinFileUtils::PreLoadDll(dll_path, 0);
	HINSTANCE operaLibrary = LoadLibrary(dll_path);
#else
	HINSTANCE operaLibrary = LoadLibraryA("Opera.dll");
#endif // WIDGET_RUNTIME_SUPPORT

	int retval = 0;

	if (operaLibrary)
	{
		OpStartDesktopInstance = (PF_OperaDesktopStart)GetProcAddress(operaLibrary, "OpStart");
		OpSetSpawnerPath = (PF_OperaSetSpawnerPath)GetProcAddress(operaLibrary, "OpSetSpawnPath");		
		OpSetLaunchMan = (PF_OperaSetLaunchMan)GetProcAddress(operaLibrary, "OpSetLaunchMan");		
		OpGetNextUninstallFile = (PF_OperaGetNextUninstallFile)GetProcAddress(operaLibrary, "OpGetNextUninstallFile");
		OpWaitFileWasPresent = (PF_OpWaitFileWasPresent)GetProcAddress(operaLibrary, "OpWaitFileWasPresent");
		OpSetGpuInfo = (PF_OpSetGpuInfo)GetProcAddress(operaLibrary, "OpSetGpuInfo");

		if (OpStartDesktopInstance && OpSetSpawnerPath && OpSetLaunchMan && OpGetNextUninstallFile && OpWaitFileWasPresent && OpSetGpuInfo &&
			InstallMemGuardHooks(operaLibrary))
		{
#ifdef WIDGET_RUNTIME_SUPPORT
			OpSetSpawnerPath(exe_path);
#else
			wchar_t modulefilename[MAX_PATH];		/* ARRAY OK 2005-08-10 andre */
	
			GetModuleFileName(0,modulefilename,MAX_PATH);
			OpSetSpawnerPath(modulefilename);
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef AUTO_UPDATE_SUPPORT
			if (OpSetLaunchMan)
			{
				OpSetLaunchMan(g_launch_manager);
			}
			if (OpWaitFileWasPresent && AUUpdater::WaitFileWasPresent())
			{
				OpWaitFileWasPresent();
			}
#endif // AUTO_UPDATE_SUPPORT

			OpSetGpuInfo(&g_gpu_info);

			CHECK_NULL_REGISTER;
			retval = OpStartDesktopInstance(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
			CHECK_NULL_REGISTER;

			//If the dll reports there are files left to uninstall (There will always be some when trying to uninstall)
			uni_char* uninstall_file;
			if ((uninstall_file = OpGetNextUninstallFile()) != NULL)
			{
				//remove the files we can. Nevermind if it fails at this point.
				//Note the Opera.exe and Opera.dll will fail regardless since we are running.
				do
				{
					CHECK_NULL_REGISTER;
					DeleteFile(uninstall_file);
					CHECK_NULL_REGISTER;

					uni_char* sep;
					//Try removing hierarchies of empty folder.
					//It's ok to do it this way, because the Opera folder won't be empty at this point (Opera.exe is there).
					do
					{
						sep = uni_strrchr(uninstall_file, '\\');
						if (!sep) break;

						*sep = 0;
						CHECK_NULL_REGISTER;
					}
					while (RemoveDirectory(uninstall_file));
					CHECK_NULL_REGISTER;
				}
				while ((uninstall_file = OpGetNextUninstallFile()) != NULL);

				//Since we are uninstalling, we shall remove the dll and the executable as well.

				//This is one of the possible ways to make a self-deleting process.
				//We create a batch file to finish the cleanup, since those are able to delete themselves.
				//
				//~julienp

				uni_char self_destruct_bat_path[MAX_PATH];
				uni_strncpy(self_destruct_bat_path, exe_path, MAX_PATH);
				uni_strncpy(uni_strrchr(self_destruct_bat_path, '\\') + 1, UNI_L("k.bat"), 6);

				HANDLE self_destruct_bat;

				do
				{
					self_destruct_bat = CreateFile(self_destruct_bat_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if (self_destruct_bat == INVALID_HANDLE_VALUE)
						break;

					char buffer[4*MAX_PATH + 200];
					uni_char dll_full_path[MAX_PATH];
					uni_strncpy(dll_full_path, exe_path, MAX_PATH);
					uni_strcpy(uni_strrchr(dll_full_path, '\\') + 1, DEFAULT_DLL_PATH);

					sprintf(buffer, ":Repeat1\r\ndel \"%S\"\r\nif exist \"%S\" goto Repeat1\r\n:Repeat2\r\ndel \"%S\"\r\nif exist \"%S\" goto Repeat2\r\ndel %%0\r\n", exe_path, exe_path, dll_full_path, dll_full_path);
					DWORD len = op_strlen(buffer);
					DWORD written;

					BOOL success = WriteFile(self_destruct_bat, buffer, len, &written, NULL) && written == len ;
					CloseHandle(self_destruct_bat);
					if (!success) break;

					SHELLEXECUTEINFO info;
					memset(&info, 0, sizeof(info));

					info.cbSize = sizeof(info);
					info.fMask = SEE_MASK_NOASYNC;
					info.lpFile = self_destruct_bat_path;
					info.lpVerb = UNI_L("open");
					info.nShow = SW_HIDE;

					ShellExecuteEx(&info);
				}
				while (FALSE);
			}
		}
		else
		{
			WindowsUtils::ShowError("Opera failed to start because: \n%s");
		}
#ifdef AUTO_UPDATE_SUPPORT
		uni_char* update_path = g_launch_manager->GetUpdatePath();
#endif
		FreeLibrary(operaLibrary);

		DestroyMemGuard();

#ifdef AUTO_UPDATE_SUPPORT
		if (update_path)
		{
			g_launch_manager->LaunchOnExitApplication(update_path);
			delete [] update_path;
		}
#endif		
	}
	else
	{
		WindowsUtils::ShowError("Failed to load Opera.DLL because: \n%s");
	}

	if (g_crash_thread_id != 0)
	{
		// another thread crashed - suspend current thread, otherwise CRT might
		// terminate current process before crash logging subprocess attaches
		// itself as a debugger (DSK-362561)
		SuspendThread(GetCurrentThread());
	}

	return retval;
}
