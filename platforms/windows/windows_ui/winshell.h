//  ___________________________________________________________________________
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//  FILENAME    WinShell.h
//
//  CREATED     DG-191298 
//
//  DESCRIPION  Win95/NT4.0 ++ shell interface functions 
//              I.e. stuff using the IShellFolder object, etc.
//
//              All of this stuff is Win32
//  ___________________________________________________________________________
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?

#ifdef WIN32

#ifndef _WIN_SHELL_H_INCLUDED
#define _WIN_SHELL_H_INCLUDED

#include <shlobj.h>
#include "OpShObjIdl.h"

BOOL GetShellProtocolHandler
(
	IN	const uni_char*	pszProtocol,			///<	IN	e.g "mailto"	(an opt. trailing colon is ok)
	OUT	uni_char *	pszAppDescription,			///<	OUT	e.g "Eudora"	(not always awailable)
	IN	int			nMaxAppDescriptionLen,		///<	IN	max strlen() for 'pszAppDescription'
	OUT	uni_char *	pszOpenCommand,				///<	OUT	value of HKEY_CLASSES_ROOT/shell/open/command
	IN	int			nMaxOpenCommandLen			///<	IN	max strlen() for 'pszOpenCommand'
);

BOOL GetShellFileHandler
(
	const uni_char*		file_name,			///<	IN	e.g "C:\metallica - sad but true.mp3"
	OUT	OpString&		app_name				///<	OUT	e.g "C:\Program Files\winamp.exe
);

LPITEMIDLIST    GetPidlFromPath     ( HWND hWndOwner, IShellFolder *pIShellFolder, LPCTSTR tchPath);
ULONG           GetRefCount         ( IUnknown * pIUnknown);

/*BOOL ExecShellContextMenuForFile
( 
    HWND		hWndParent,             ///< Parent window  (the shell might need a parent to dislplay dialogs etc)
    const uni_char*	szPath,				///< Full path to file
    HMENU		hMenuShellContext,      ///< The menu where the shell will add its items. This menu is either the same or an submenu of 'hMenuToShow')
    BOOL		fEnableShellItems,      ///< Enable/disable shell menu items ?
    int			nFirstPos,              ///< Shell menu items are added to 'hmenuShellContext' above, starting with this position on the menu
    HMENU		hMenuToShow,            ///< The menu that will be displayed with this function
    int			idMin, int idMax,       ///< Allowed range for shell menu items ID's
    int			x, int y,               ///< Location for menu
    int			*pCommandSelected,      ///< OPT OUT -- Menu item selected for items not belonging to the shell.
	BOOL		fDisableDelete=FALSE	///< Disable those known menu items that would cause the file to be deleted
);

BOOL WinShell_InvokeFileDefaultContextMenu
(
    HWND			hWndParent,             ///< Parent window  (the shell might need a parent to dislplay dialogs etc)
    const uni_char	*szPath,                ///< Full path to file
    BOOL			fEnableItems,			///< Enable/disable shell menu items ?
	BOOL			fDisableDelete,			///< Disable those known menu items that would cause the file to be deleted
    POINT			ptWhere					///< Location for menu
);*/

HRESULT GetShortcutTarget
( 
    HWND    hWndParent,             ///< The shell might want to display some messages
    LPCTSTR  pszShortcutFile,       ///< Path to shortcutfile
    BOOL    fResolve,               ///< Resolve broken shortcuts ?  (I.e Windows is search for missing ...)
    LPTSTR   pszPath,               ///< OUT: Resolved path to the file pointed to by the shortcut
    int     cbPath,                 ///< Size in bytes of 'pszPath'
    LPTSTR   pszDescription,        ///< OUT: Filedescription (use NULL if not needed)
    int     cbDescription           ///< Size in bytes of 'szDescription'
);

DWORD GetLongPathNameOp
( 
	LPCTSTR pszShortPath_,			///< Inp. string
	LPTSTR pszLongPath_,			///< Out string
	DWORD cbBuffer_					///< Size of out string
);

/*BOOL WinShell_InvokeFilePropertiesDialog
( 
	HWND			hWndParent,		///< Parent window
	const uni_char *szFileName		///< File name to invoke context menu for
);*/
OP_STATUS GetFileDescription(const uni_char* file, OpString* filedescription, OpString* productName = NULL);
OP_STATUS GetAllShellFileHandlers (const uni_char* file_name, OpVector<OpString>& app_name_list, OpString* excluded_app);
OP_STATUS GetURLHandlers (OpVector<OpString>& app_name_list);
OP_STATUS GetFileHandlerInfo(const uni_char* handler, OpString* filedescription, OpBitmap** icon, INT32 icon_size);
//Callback for EnumResourceNames (used in GetFileHandlerIcon)
BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
OP_STATUS GetFirstIconFromBinary(const uni_char* binary, OpBitmap** icon, INT32 icon_size);
OP_STATUS GetShellDefaultIcon (const uni_char* file_name, OpBitmap** icon, INT32 icon_size);
OP_STATUS GetMainShellFileHandler (const OpString& extension, OpString& app_command);
//helper functions for GetAllShellFileHandlers
OP_STATUS AddAppToList (OpString *app_command, OpVector<OpString>& app_name_list, OpString *excluded_app);
OP_STATUS ExtractOpenWithData(HKEY root_key, const uni_char* key_name, BOOL from_value_datas, BOOL from_key_names, BOOL as_prog_id, OpVector<OpString>& app_name_list, OpString *excluded_app);
OP_STATUS GetAppCommand(const OpString& app_name, OpString& app_command);
OP_STATUS GetShellCommand(const OpString& key, OpString& app_command);
void ParseShellCommand(OpString& app_command);

#endif  //  this file
#endif  //  win32


