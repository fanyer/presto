//	___________________________________________________________________________
//	FILENAME	Exec.h
//	CREATED		Mai 13 1998 DG[50]
//
//	Running external programs from Opera (sync. and async)
//	___________________________________________________________________________


#ifndef _EXEC_H_INCLUDED
#define _EXEC_H_INCLUDED

#ifdef _DEBUG
	//#define DEBUG_EXEC			//	Comment out to remove xtra debug stuff
#endif
#ifdef DEBUG_EXEC
	void DEBUG_TestOpExecute();		//	Debug only -- bring up a dialog to test out the OpExecute function.
#endif

BOOL OpExecute
( 
	uni_char* szCommand,		//	Full command line
	HWND hWndOwner,				//	"Owner window" (when invoked from another modal dialog) or NULL (MainhWnd)
	BOOL bWait,					//	Wait for child to finnsish ?
	BOOL bAllowCancelWait,		//	Allow user to cancel waiting for child to finnish ?
	DWORD *pResult				//	OUT: result ( only valid on failure)
);

#endif
