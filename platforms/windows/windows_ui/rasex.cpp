/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1999-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** CREATED			DG-170399
** DESCRIPTION		Allow Dynamic linking of RAS API functions
**
**					Because - If an application links statically to the RASAPI32 DLL,
**					the application will fail to load if Remote Access Service is not
**					installed.
*/

#include "core/pch.h"

#if defined(_RAS_SUPPORT_)

#include "rasex.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include <raserror.h>

static BOOL			s_fPrefsManagerAskCloseConnections = FALSE;

const int MAXConn = 16;

//	___________________________________________________________________________
//	OpRasEnableAskCloseConnections
//	
//	PrefsManager is void when the dialog is shown after leaving the main message
//	loop.  This function set's the static var s_fPrefsManagerAskCloseConnections
//	___________________________________________________________________________
//
void OpRasEnableAskCloseConnections( BOOL fEnable)
{
	s_fPrefsManagerAskCloseConnections = fEnable;
}



//	___________________________________________________________________________
//	OpRasAskCloseConnections
//
//	Display a dialog to allow closing all open dialup connections at exit.
//	If the return value is IDCANCEL Opera should not allow to exit.
//
//	Returns IDCANCEL or IDNO	if no close
//			IDYES				if the connections was closed
//			IDIGNORE			No connections was open
//	___________________________________________________________________________
//
//
LRESULT OpRasAskCloseConnections( HWND hWndParent, BOOL fAllowCancel)
{
	if( !s_fPrefsManagerAskCloseConnections)
		return IDIGNORE;

	LRESULT lRes;
	//
	//	Get all open connections.
	//
	DWORD dwSize = 0;
	DWORD dwConnections = 0;
	RASCONN rasconn[MAXConn];
	
	dwSize = sizeof( rasconn);
	rasconn[0].dwSize = sizeof(rasconn[0]);
	DWORD dwResult = RasEnumConnections( rasconn, &dwSize, &dwConnections);
    DWORD i;

	if( !dwConnections || dwResult != ERROR_SUCCESS  || !g_languageManager)
	{
		//	No connections was open or an error
		return IDIGNORE;	
	}

	//
	//	Ask the user whether he would close them or not
	//
	OpString szLangTitle;
	OpString szLangMsg;
	
	g_languageManager->GetString(Str::SI_IDSTR_ASKRASCLOSEALLCAPTION, szLangTitle);
	g_languageManager->GetString(Str::SI_IDSTR_ASKRASCLOSEALL, szLangMsg);

	uni_char szConnectionList [MAXConn * RAS_MaxEntryName] = UNI_L("");
	for( i=0; i<dwConnections; i++)
	{
		uni_strcat( szConnectionList, UNI_L("\""));
		uni_strcat( szConnectionList, rasconn[i].szEntryName);
		uni_strcat( szConnectionList, UNI_L("\""));
		
		if( i==dwConnections-1)
			uni_strcat( szConnectionList, UNI_L("."));
		else
			uni_strcat( szConnectionList, UNI_L(", "));
	}
	uni_strcat( szConnectionList, UNI_L("\n"));

	uni_char szMsg[ARRAY_SIZE(szConnectionList)+256];
	uni_sprintf(szMsg, szLangMsg.CStr(), szConnectionList);
	UINT type = fAllowCancel ? MB_YESNOCANCEL : MB_YESNO;
	lRes = MessageBox( hWndParent, szMsg, szLangTitle.CStr(), type|MB_ICONQUESTION);

	if( lRes != IDYES)
	{
		//	Do not close dun
		return lRes;
	}

	//
	//	Close all open connections.
	//
	for( i=0; i<dwConnections; i++)
	{
		HRASCONN hrasconn = rasconn[i].hrasconn;
		RasHangUp( hrasconn);

		RASCONNSTATUS status;
		status.dwSize = sizeof(status);

		DWORD dwStart = GetTickCount();
		DWORD dwTimeOut = dwStart + 5000;

		while( !RasGetConnectStatus( hrasconn, &status))
		{
			Sleep( 10);
			if( GetTickCount() > dwTimeOut)
			{
				//
				//	Timed out - unable to close all connections
				//	Has to load this manually - mess and prefsManager might be NULL
				//
				OpString sz;
				g_languageManager->GetString(Str::SI_IDSTR_ERR_RASCLOSEALLFAILED, sz);
				MessageBox( hWndParent, sz.CStr(), UNI_L("Opera"), MB_ICONERROR);

				return IDCANCEL;
			}
		}
	}
	
	return lRes;
}

#endif	// _RAS_SUPPORT_

