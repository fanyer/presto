//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	FILENAME		rasex.h
//	CREATED			DG-170399 
//	
//	DESCRIPTION		Allow Dynamic linking of RAS API functions
//
//					Because - If an application links statically to the RASAPI32 DLL, 
//					the application will fail to load if Remote Access Service is not 
//					installed. 
//				
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯


#ifndef _RASEX_H_INCLUDED__133614
#define _RASEX_H_INCLUDED__133614


#if defined(_RAS_SUPPORT_)

#include "core/pch.h"

#include <ras.h>

//
//	OpRasAskCloseConnections
//
/**	Display a dialog to allow closing all open dialup connections at exit.
 *	If the return value is IDCANCEL Opera should not allow to exit.
 *
 *	Returns IDCANCEL or IDNO	if no close
 *			IDYES				if the connections was closed
 *			IDIGNORE			No connections was open
 */
LRESULT OpRasAskCloseConnections
( 
	HWND hWndParent, 
	BOOL fAllowCancel
);

//
//	OpRasEnableAskCloseConnections
//
/**	Enable/disable to dialog when closing Opera after prefsManager is gone.
 */
void OpRasEnableAskCloseConnections( BOOL fEnable);

#endif	

#endif	//	_RASEX_H_INCLUDED__133614 undefined
