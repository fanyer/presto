/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Golczynski (tgolczynski@opera.com)
 */
#include "core/pch.h"

#ifdef OPERA_MAPI

#include <windows.h>
#include <MAPI.h>

#include "platforms/windows/mapi/OperaMapiUtils.h"
#include "adjunct/m2/src/generated/g_proto_m2_mapimessage.h"
#include "adjunct/m2/src/generated/g_proto_descriptor_m2_mapimessage.h"
#include "adjunct/m2/src/generated/g_message_m2_mapimessage.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"

CoreComponent* g_opera = NULL;
OpThreadTools* g_thread_tools = NULL;
HINSTANCE hInst = NULL;

////////////////////////////
////    MAPISendMail    ////
////////////////////////////

ULONG FAR PASCAL MAPISendMail(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved)
{
	OperaMapiUtils mapi_utils;
	RETURN_VALUE_IF_ERROR(OpComponentManager::Create(&g_component_manager), MAPI_E_FAILURE);
	RETURN_VALUE_IF_ERROR(mapi_utils.Init(), MAPI_E_FAILURE);

	OpCreateNewMailMessage* desc;
	RETURN_VALUE_IF_ERROR(mapi_utils.CreateNewMailDescription(&desc, lpMessage->lpszSubject, lpMessage->lpszNoteText, lpMessage->lpszDateReceived), MAPI_E_FAILURE);
	OpAutoPtr<OpCreateNewMailMessage> message(desc);

	lpMapiRecipDesc recipients = lpMessage->lpRecips;
	for (ULONG i = 0; i < lpMessage->nRecipCount; i++)
	{
		lpMapiRecipDesc recip = recipients + i;
		RETURN_VALUE_IF_ERROR(mapi_utils.AddRecipient(desc, recip->ulRecipClass, recip->lpszName, recip->lpszAddress), MAPI_E_FAILURE);
	}
	
	lpMapiFileDesc files = lpMessage->lpFiles;
	for (ULONG i = 0; i < lpMessage->nFileCount; i++)
	{
		lpMapiFileDesc file = files + i;
		RETURN_VALUE_IF_ERROR(mapi_utils.AttachFile(desc, file->lpszPathName, file->lpszFileName), MAPI_E_FAILURE);
	}

	RETURN_VALUE_IF_ERROR(mapi_utils.SendMessageToOpera(message.release()), MAPI_E_FAILURE);
	OP_DELETE(g_component_manager);
	g_component_manager = NULL;
	return SUCCESS_SUCCESS;
}

/////////////////////////////////
////    MAPISendDocuments    ////
/////////////////////////////////

ULONG FAR PASCAL MAPISendDocuments(ULONG_PTR ulUIParam, LPSTR lpszDelimChar, LPSTR lpszFilePaths, LPSTR lpszFileNames, ULONG ulReserved)
{
	OperaMapiUtils mapi_utils;
	RETURN_VALUE_IF_ERROR(OpComponentManager::Create(&g_component_manager), MAPI_E_FAILURE);
	RETURN_VALUE_IF_ERROR(mapi_utils.Init(), MAPI_E_FAILURE);

	OpString paths;
	OpString names;
	OpString delimiter;

	
	RETURN_VALUE_IF_ERROR(mapi_utils.ConvertToUTF16(lpszDelimChar, delimiter), MAPI_E_FAILURE);
	RETURN_VALUE_IF_ERROR(mapi_utils.ConvertToUTF16(lpszFilePaths, paths), MAPI_E_FAILURE);
	RETURN_VALUE_IF_ERROR(mapi_utils.ConvertToUTF16(lpszFileNames, names), MAPI_E_FAILURE);

	OpCreateNewMailMessage* desc;
	RETURN_VALUE_IF_ERROR(mapi_utils.CreateNewMailDescription(&desc), MAPI_E_FAILURE);
	OpAutoPtr<OpCreateNewMailMessage> message(desc);
	
	RETURN_VALUE_IF_ERROR(mapi_utils.AttachFiles(desc, paths, names, delimiter[0]), MAPI_E_FAILURE);
	RETURN_VALUE_IF_ERROR(mapi_utils.SendMessageToOpera(message.release()), MAPI_E_FAILURE);
	OP_DELETE(g_component_manager);
	g_component_manager = NULL;
	return SUCCESS_SUCCESS;
}

////////////////////////////////////
////    Unsuported functions    ////
////////////////////////////////////
ULONG FAR PASCAL MAPIAddress(LHANDLE lhSession, ULONG_PTR ulUIParam, LPSTR lpszCaption, ULONG nEditFields, LPSTR lpszLabels,
						 ULONG nRecips, lpMapiRecipDesc lpRecips, FLAGS flFlags, ULONG ulReserved, LPULONG lpnNewRecips, lpMapiRecipDesc *lppNewRecips)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIDeleteMail(LHANDLE lhSession, ULONG_PTR ulUIParam, LPSTR lpszMessageID, FLAGS flFlags, ULONG ulReserved)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIDetails(LHANDLE lhSession, ULONG_PTR ulUIParam, lpMapiRecipDesc lpRecip, FLAGS flFlags, ULONG ulReserved)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIFindNext(LHANDLE lhSession, ULONG_PTR ulUIParam, LPSTR lpszMessageType, LPSTR lpszSeedMessageID, FLAGS flFlags, ULONG ulReserved, LPSTR lpszMessageID)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIFreeBuffer(LPVOID pv)
{
	return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPILogoff(LHANDLE lhSession, ULONG_PTR ulUIParam, FLAGS flFlags, ULONG ulReserved)
{
	return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPILogon(ULONG_PTR ulUIParam, LPSTR lpszProfileName, LPSTR lpszPassword, FLAGS flFlags, ULONG ulReserved, LPLHANDLE lplhSession)
{
	*lplhSession = reinterpret_cast<LHANDLE>(hInst);
	return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPIReadMail(LHANDLE lhSession, ULONG_PTR ulUIParam, LPSTR lpszMessageID, FLAGS flFlags, ULONG ulReserved, lpMapiMessage *lppMessage)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIResolveName(LHANDLE lhSession, ULONG_PTR ulUIParam, LPSTR lpszName, FLAGS flFlags, ULONG ulReserved, lpMapiRecipDesc *lppRecip)
{
	return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPISaveMail(LHANDLE lhSession, ULONG_PTR ulUIParam, lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved, LPSTR lpszMessageID)
{
	return MAPI_E_NOT_SUPPORTED;
}

BOOL APIENTRY DllMain (HINSTANCE hinstDLL, DWORD fdwReason,  LPVOID lpvReserved)
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
		hInst = NULL;
		WindowsTLS::Destroy();
		break;
	}
	return TRUE;
}

#endif
