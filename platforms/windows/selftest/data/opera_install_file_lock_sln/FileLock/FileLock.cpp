// FileLock.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <tchar.h>
#include <stdlib.h>

#include "Shellapi.h"

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{

	if (__argc != 2)
		return 1;

	LPWSTR* wide_commandline = NULL;
	int commandline_argument_number;
	wide_commandline = CommandLineToArgvW(GetCommandLineW(), &commandline_argument_number);

	HANDLE h = CreateFile(wide_commandline[1], GENERIC_ALL, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	FlushFileBuffers(h);

	DWORD e = GetLastError();

	Sleep(INFINITE);

	return 0;
}