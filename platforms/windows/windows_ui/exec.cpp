//	___________________________________________________________________________
//	FILENAME	Exec.cpp
//	CREATED		Mai 13 1998 DG[50]
//
//	Running external programs from Opera (sync. and async)
//	___________________________________________________________________________
#include "core/pch.h"

#include "Exec.h"

#undef PostMessage
#define PostMessage PostMessageU

// Get rid of the _export warning for WIN32
#ifndef _export
	#define _export
#endif	

//	Internal use only (pass information to the dialog proc)
struct OPEXEC_LPARAM		
{
	uni_char *szCommand;
	BOOL	bAllowCancelWait;	
	BOOL	bWait;					//	Wait for child to finnsish ?
	DWORD	error;
};

//	Internal forwarders
static BOOL _ExecuteNow (uni_char *szCommand, BOOL bCloseHandles, HANDLE *hProcessChild, DWORD*pError);
static BOOL IsAppRunning( HANDLE hApp);
LRESULT CALLBACK SpawnChildDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void WaitForAppToFinish( OPEXEC_LPARAM execParam);

//	___________________________________________________________________________
//	FUNCTION OpExecute
//	
//	Execute an external program.  Returns TRUE on success.  
//	
//	Param 'pResult' will on failure contain:
//		WIN 16		- ERROR_BAD_FORMAT, ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND
//				      or 0 (out of system resources)
//		WIN 32		- The result from GetLastError()
//		WIN 16/32	- IDCANCEL the user pressed the cancel button (child may 
//					  still be running)
//
//		Use NULL if no result code is needed.
//	___________________________________________________________________________
//

BOOL OpExecute
( 
	uni_char* szCommand,	//	Full command line
	HWND hWndOwner,			//	"Owner window" (when invoked from another modal dialog)
	BOOL bWait,				//	Wait for child to finish ?
	BOOL bAllowCancelWait,	//	Allow user to cancel waiting for child to finish ?
	DWORD *pResult			//	OUT: result ( only valid on failure)
)
{   
	static BOOL bInside = FALSE;

	if( bInside)
	{
		if( pResult)
			*pResult = 0;
		return FALSE;
	}

	bInside = TRUE;

	BOOL bRetVal = FALSE;
	OPEXEC_LPARAM execParam;
	execParam.bAllowCancelWait = bAllowCancelWait;
	execParam.szCommand = szCommand;


	//	Param sanity check
	if( !IsStr( szCommand))
		return FALSE;

	HANDLE hDummy;
	bRetVal = _ExecuteNow( szCommand, TRUE, &hDummy, pResult);

#ifdef DEBUG_EXEC
	MessageBox( NULL, "Done with function Execute()...", "DEBUG_EXEC", MB_OK);
#endif
	
	bInside = FALSE;
	return bRetVal;
}                          



//	___________________________________________________________________________
//	FUNCTION _ExecuteNow
//	___________________________________________________________________________
//

static BOOL _ExecuteNow
(	uni_char	*szCommand,			//	Command
	BOOL		bCloseHandles,		//	WIN32 only 
	HANDLE		*hProcessChild,		//	OUT:
	DWORD		*pError				//	OUT: error ( only valid on failure)
)
{
	BOOL bRetVal = FALSE;

	PROCESS_INFORMATION processInfo;
	STARTUPINFO startupInfo;

	memset( &startupInfo, 0, sizeof( startupInfo));
	startupInfo.cb = sizeof( startupInfo);


	BOOL fOk = CreateProcess(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
	if( fOk)
	{
		bRetVal = TRUE;

		CloseHandle( processInfo.hThread);
		if( hProcessChild)
			*hProcessChild = processInfo.hProcess;

		if( bCloseHandles)
		{
			CloseHandle( processInfo.hProcess);
		}
	}
	else
	{
		if( pError)
			*pError = GetLastError();
	}

	return bRetVal;
}	
