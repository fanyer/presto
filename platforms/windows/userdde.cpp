/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include <ddeml.h>
#include <dde.h>

#ifdef _DDE_SUPPORT_

#include "userdde.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"	// for EscapeURLIfNeeded
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#ifdef WIDGET_RUNTIME_SUPPORT
#	include "adjunct/widgetruntime/GadgetUtils.h"
#endif // WIDGET_RUNTIME_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/util/str.h"

#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/installer/OperaInstaller.h"

#ifdef _DDE_SUPPORT_
extern DDEHandler* ddeHandler;
#endif

DWORD	dwDdeInstance;

DDEHandler::DDEHandler() : 
m_process(FALSE),
busy(TRUE),
m_willdie(FALSE)
{
	phsz[0] = &hszSystemTopic;
	phsz[1] = &hszOpenURLTopic;
	phsz[2] = &hszQueryVersionTopic;
	phsz[3] = &hszActivateTopic;
	phsz[4] = &hszAlertTopic;
	phsz[5] = &hszBeginProgressTopic;
	phsz[6] = &hszCancelProgressTopic;
	phsz[7] = &hszEndProgressTopic;
	phsz[8] = &hszExitTopic;
	phsz[9] = &hszGetWindowInfoTopic;
	phsz[10] = &hszListWindowsTopic;
	phsz[11] = &hszMakingProgressTopic;		
	phsz[12] = &hszParseAnchorTopic;
	phsz[13] = &hszQueryURLFileTopic;
	phsz[14] = &hszQueryViewerTopic;
	phsz[15] = &hszRegisterProtocolTopic;
	phsz[16] = &hszRegisterURLEchoTopic;
	phsz[17] = &hszRegisterViewerTopic;
	phsz[18] = &hszRegisterWindowChangeTopic;
	phsz[19] = &hszSetProgressRangeTopic;
	phsz[20] = &hszShowFileTopic;
	phsz[21] = &hszUnRegisterProtocolTopic;
	phsz[22] = &hszUnRegisterURLEchoTopic;
	phsz[23] = &hszUnRegisterViewerTopic;
	phsz[24] = &hszUnRegisterWindowChangeTopic;
	phsz[25] = &hszURLEchoTopic;
	phsz[26] = &hszVersionTopic;
	phsz[27] = &hszViewDocFileTopic;
	phsz[28] = &hszWindowChangeTopic;
}

BOOL DDEHandler::RestartDDE()
{
	ExitDDE();
	InitDDE();
	return TRUE;
}

void DDEHandler::InitDDE()
{
	iCodePage = CP_WINUNICODE;

	lpfnDdeProc = MakeProcInstance ((FARPROC) DdeCallback, (HINSTANCE) GetWindowWord (g_main_hwnd, GWW_HINSTANCE));

	UINT res;

	busy = TRUE;

	OperaInstaller installer;
	BOOL registerIllegalServices = installer.AssociationsAPIReady() &&
		(installer.HasAssociation(OperaInstaller::HTML)?1:0) +
		(installer.HasAssociation(OperaInstaller::HTM)?1:0) +
		(installer.HasAssociation(OperaInstaller::HTTP)?1:0) +
		(installer.HasAssociation(OperaInstaller::HTTPS)?1:0) > 1;

	res = DDEInitNT(registerIllegalServices);

	busy = FALSE;

	return;
}

UINT DDEHandler::DDEInitNT(BOOL registerIllegalServices)
{
	HSZ	hszService;
	UINT res = DdeInitialize((LPDWORD) &dwDdeInstance, (PFNCALLBACK) lpfnDdeProc, APPCLASS_STANDARD|/*MF_SENDMSGS|MF_POSTMSGS|*/CBF_FAIL_SELFCONNECTIONS/*|APPCMD_FILTERINITS*/, 0L);

	if(res != DMLERR_NO_ERROR)
	{
		return res;
	}

	hszService = DdeCreateStringHandle(dwDdeInstance, UNI_L("Opera"), iCodePage);
	DdeNameService (dwDdeInstance, hszService, NULL, DNS_REGISTER);
	DdeFreeStringHandle (dwDdeInstance, hszService);

	if(registerIllegalServices)
	{
//***************************************************************************************
	//We should not respond to these
	//but since there are a lot of programs not calling Opera, they are needed

		hszService = DdeCreateStringHandle(dwDdeInstance, UNI_L("Netscape"), iCodePage);
		DdeNameService (dwDdeInstance, hszService, NULL, DNS_REGISTER);
		DdeFreeStringHandle (dwDdeInstance, hszService);
		hszService = DdeCreateStringHandle(dwDdeInstance, UNI_L("IExplore"), iCodePage);
		DdeNameService (dwDdeInstance, hszService, NULL, DNS_REGISTER);
		DdeFreeStringHandle (dwDdeInstance, hszService);

//***************************************************************************************
	}

	for (int i=0; i<ARRAY_SIZE(phsz); i++)
		*phsz[i] = DdeCreateStringHandle(dwDdeInstance, DDE_service_names[i], iCodePage);

	return res;
}

void DDEHandler::ExitDDE()
{
	busy = TRUE;

	m_delayed_executes.DeleteAll();

	for (int i=0; i<ARRAY_SIZE(phsz); i++)
		DdeFreeStringHandle (dwDdeInstance, *phsz[i]);

	DdeNameService (dwDdeInstance, NULL, NULL, DNS_UNREGISTER);

	DdeUninitialize (dwDdeInstance);
	dwDdeInstance = NULL;
}

#endif

extern "C" HDDEDATA  EXPENTRY DdeCallback(
		UINT wType,		// transaction type
		UINT wFmt,		// clipboard data format
		HCONV hConv,	// handle to the conversation
		HSZ hsz1,		// handle to a string
		HSZ hsz2,		// handle to a string
		HDDEDATA hData,	// handle to global memory object
		DWORD dwData1,	// transaction-specific data
		DWORD dwData2)	// transaction-specific data
{
#ifndef _DDE_SUPPORT_
	return 0;
#else

	if (!ddeHandler || 
		!g_pcmswin ||
		!g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::DDEEnabled) ||
		ddeHandler->Busy())
		return (HDDEDATA)DDE_FBUSY;

	switch (wType)
	{
	case XTYP_CONNECT:	// check if right topic and service
		{
/*			uni_char szBuf[512];

			DdeQueryString(dwDdeInstance, hsz2, szBuf,	ARRAY_SIZE(szBuf), iCodePage);*/

			return (HDDEDATA) ddeHandler->CheckConnect(hsz1);
		}
		break;

	case XTYP_WILDCONNECT:	// client asking for all
					// supported services and topics
		{
			return (HDDEDATA)0;
		}

	case XTYP_REQUEST:		// client requesting data
		{
			if(!ddeHandler->IsStarted() || wFmt != CF_TEXT) // don't support anything else that CF_TEXT
			{
				return (HDDEDATA)0;
			}
			return ddeHandler->HandleRequest(hsz1, hsz2, hData);
		}

	case XTYP_POKE:
		{
			if(!ddeHandler->IsStarted())
			{
				return (HDDEDATA)0;
			}
			return ddeHandler->HandlePoke(hsz1, hsz2, hData);
		}
	case XTYP_EXECUTE:
		if (DdeCmpStringHandles (ddeHandler->hszSystemTopic, hsz1) == 0 ||
			DdeCmpStringHandles (ddeHandler->hszOpenURLTopic, hsz1) == 0)
		{
			int size = 4096;

			LPBYTE szBuf = new BYTE[size];
			HDDEDATA retval = (HDDEDATA)0;
			if (szBuf)
			{
				DdeGetData(hData, szBuf, size-1, 0);
				szBuf[size-1]=0;

				if (*szBuf)
				{


				// Check for misconfigured registry
				// Is this a DDE request to handle something that we don't handle ?

					uni_char *szProtocol = ((uni_char*)szBuf) + uni_strspn( (uni_char*)szBuf, UNI_L("\""));
					if (DocumentManager::IsTrustedExternalURLProtocol(szProtocol))
						retval = (HDDEDATA)DDE_FNOTPROCESSED;	// Anything better ???
					else
					{
						if(!ddeHandler->IsStarted())
						{
							if (!ddeHandler->WillDie() && OpStatus::IsSuccess(ddeHandler->AddDelayedExecution((uni_char*)szBuf)))
								return (HDDEDATA)DDE_FACK;			// The execution was accepted. We just wait until it can be done
							else
								retval = (HDDEDATA)DDE_FNOTPROCESSED;	// Anything better ???
						}
						else
						{
							retval = ddeHandler->ExecuteCommand((uni_char*)szBuf);
						}
					}
				}
				delete[] szBuf;
			}
			return retval;
		}
		break;

	default:
		break;
	}

	return (HDDEDATA)0;
#endif
}


#ifdef _DDE_SUPPORT_

BOOL DDEHandler::CheckConnect(HSZ hsz1)
{

#ifdef WIDGET_RUNTIME_SUPPORT
	// Ignore the OpenURL topic if we are a gadget or the gadget installer.
	//
	// Short story:  This is required for the "Access external link via widget"
	// feature to work if Opera is the default browser.
	//
	// Long story:  The standard way to open a URL with the default browser on
	// Windows is via `ShellExecute()'. [1]  When Opera registers itself as the
	// default browser, it fills the "\open\ddeexec" registry keys for the
	// relevant protocols, which causes `ShellExecute()' to try DDE first. [2]
	// The end result is that the currently executing gadget or the gadget
	// installer may be told, by means of DDE, to open a new URL, which
	// obviously is not what we want.
	//
	// [1] See platforms/windows/WindowsOpGadgetUtils.cpp
	// [2] See http://support.microsoft.com/kb/224816/en-us
	//
	// TODO:  Make it more efficient, e.g., by establishing a set of ignored
	//        topics upon `DDEHandler' creation / initialization.

	if (NULL != g_application)
	{	
		if (!g_application->IsBrowserStarted()
				&& 0 == DdeCmpStringHandles(hszOpenURLTopic, hsz1))
		{
			return false;
		}
	}
#endif // WIDGET_RUNTIME_SUPPORT


	for (int i=0; i<ARRAY_SIZE(phsz); i++)
		if (DdeCmpStringHandles(*phsz[i], hsz1) == 0)
			return true;
	
	return false;
}

HDDEDATA DDEHandler::HandleRequest(HSZ hsz1, HSZ hsz2, HDDEDATA hData)
{
	uni_char szBuf[512];

	if (DdeCmpStringHandles (hszVersionTopic, hsz1) == 0)
		return HandleDDEVersion();

	if (DdeCmpStringHandles (hszQueryVersionTopic, hsz1) == 0)
		return HandleDDEQueryVersion();

	if (DdeCmpStringHandles (hszListWindowsTopic, hsz1) == 0)
		return HandleDDEListWindows();
	
	if (!DdeQueryString(dwDdeInstance, hsz2, szBuf, ARRAY_SIZE(szBuf), iCodePage))
		return NULL;
	
	if (DdeCmpStringHandles (hszSystemTopic, hsz1) == 0)
	{
		return HandleDDESystem(szBuf);
	}
	else if (DdeCmpStringHandles (hszActivateTopic, hsz1) == 0)
	{
		return HandleDDEActivate(szBuf);
	}
	else if (DdeCmpStringHandles (hszGetWindowInfoTopic, hsz1) == 0)
	{
		return HandleDDEGetWindowInfo(szBuf);
	}
	else if (DdeCmpStringHandles (hszOpenURLTopic, hsz1) == 0)
	{
		return HandleDDEOpenURL(szBuf);
	}
	else if (DdeCmpStringHandles (hszParseAnchorTopic, hsz1) == 0)
	{
		return HandleDDEParseAnchor(szBuf);
	}
	else if (DdeCmpStringHandles (hszQueryURLFileTopic, hsz1) == 0)
	{
		return HandleDDEQueryURLFile(szBuf);
	}
	else if (DdeCmpStringHandles (hszQueryViewerTopic, hsz1) == 0)
	{
		return HandleDDEQueryViewer(szBuf);
	}
	else if (DdeCmpStringHandles (hszRegisterProtocolTopic, hsz1) == 0)
	{
		return HandleDDERegisterProtocol(szBuf);
	}
	else if (DdeCmpStringHandles (hszRegisterViewerTopic, hsz1) == 0)
	{
		return HandleDDERegisterViewer(szBuf);
	}
	else if (DdeCmpStringHandles (hszRegisterWindowChangeTopic, hsz1) == 0)
	{
		return HandleDDERegisterWindowChange(szBuf);
	}
	else if (DdeCmpStringHandles (hszShowFileTopic, hsz1) == 0)
	{
		return HandleDDEShowFile(szBuf);
	}
	else if (DdeCmpStringHandles (hszUnRegisterProtocolTopic, hsz1) == 0)
	{
		return HandleDDEUnRegisterProtocol(szBuf);
	}
	else if (DdeCmpStringHandles (hszUnRegisterViewerTopic, hsz1) == 0)
	{
		return HandleDDEUnRegisterViewer(szBuf);
	}
	else if (DdeCmpStringHandles (hszUnRegisterWindowChangeTopic, hsz1) == 0)
	{
		return HandleDDEUnRegisterWindowChange(szBuf);
	}

	return NULL;
}

HDDEDATA DDEHandler::HandlePoke(HSZ hsz1, HSZ hsz2, HDDEDATA hData)
{
	uni_char szBuf[512];

	if (DdeCmpStringHandles (hszExitTopic, hsz1) == 0)
	{
		HandleDDEExit();
		return (HDDEDATA) DDE_FACK;
	}

	if (!DdeQueryString(dwDdeInstance, hsz2, szBuf, ARRAY_SIZE(szBuf), iCodePage))
		return NULL;
	
	if (DdeCmpStringHandles (hszCancelProgressTopic, hsz1) == 0)
		return HandleDDECancelProgress(szBuf);

	if (DdeCmpStringHandles (hszRegisterURLEchoTopic, hsz1) == 0)
		return HandleDDERegisterURLEcho(szBuf);

	if (DdeCmpStringHandles (hszUnRegisterURLEchoTopic, hsz1) == 0)
		return HandleDDEUnRegisterURLEcho(szBuf);

	if (DdeCmpStringHandles (hszWindowChangeTopic, hsz1) == 0)
		return HandleDDEWindowChange(szBuf);

	return NULL;
}

HDDEDATA DDEHandler::HandleDDEActivate(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *tmp;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	DWORD window_id = (tmp) ? (DWORD)DDEStrToULong(tmp) : 0;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);

	Window* window = NULL;
	BOOL tile_now = TRUE;
	DWORD wid = 0;

	if (window_id == 0xFFFFFFFF)
		window = g_application->GetActiveWindow();
	else
		window = windowManager->GetADDEIdWindow(window_id, tile_now);

	if (window)
	{
		window->Raise();

		wid = window->Id();
	}
	else if (window_id == 0xFFFFFFFF)
		wid = window_id;

	hDdeData = DdeCreateDataHandle (dwDdeInstance,
			(LPBYTE)&wid, sizeof(wid) + 1, 0, hszActivateTopic, CF_TEXT, NULL);
	return hDdeData;
}

/*
HDDEDATA DDEHandler::HandleDDECancelProgress(uni_char *str)
{
	char *tmp;
	short pos = 0;
	BOOL done = FALSE;
	tmp = MyStrTok(str, ",", pos, done);
	DWORD trans_id = (tmp) ? DDEStrToULong(tmp) : 0;
	if (trans_id)
	{
		//HWND h = windowManager->GetWindowTransID(trans_id);
		//CancelLoad(h);
	}
}*/

HDDEDATA DDEHandler::HandleDDEGetWindowInfo(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *tmp;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	DWORD window_id = (tmp) ? (DWORD)DDEStrToULong(tmp) : 0;

	Window* window = NULL;

	if (window_id == 0xFFFFFFFF || !window_id)
		window = g_application->GetActiveWindow();
	else
		window = windowManager->GetWindow(window_id);

	if (window)
	{
		FramesDocument *doc = window->GetCurrentDoc();

		if (doc)
		{
			URL url = doc->GetURL();
			const uni_char *url_name = url.GetUniName(TRUE, PASSWORD_SHOW);
			const uni_char *doc_title = window->GetWindowTitle();

			const uni_char* title = doc_title;

			if (url_name && *url_name)
			{
				uni_char* dataString = NULL;
				int dLen = 0;
				if (title && *title)
				{
					dataString = new uni_char [(uni_strlen(url_name) + uni_strlen(title) + 30)];
					dLen = uni_sprintf(dataString, UNI_L("\"%s\",\"Opera - [%s]\""), url_name, title);
				}
				else
				{
					dataString = new uni_char [(uni_strlen(url_name)+ 5)];
					dLen = uni_sprintf(dataString, UNI_L("\"%s\""), url_name);
				}

				HGLOBAL hglbWindowText = GlobalAlloc(GMEM_DDESHARE, dLen + 1);

				char* strCopy = (char*)GlobalLock(hglbWindowText);

				WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, (LPCWSTR)dataString, dLen + 1,
									(LPSTR)strCopy, dLen + 1, NULL, NULL);

				hDdeData = DdeCreateDataHandle (dwDdeInstance,
					(LPBYTE)strCopy, dLen+1, 0, hszGetWindowInfoTopic, CF_TEXT, NULL);

				GlobalUnlock(hglbWindowText);
				GlobalFree(hglbWindowText);
				delete [] dataString;

				return hDdeData;
			}
		}
	}
	return 0;
}

HDDEDATA DDEHandler::HandleDDEListWindows()
{
	DWORD wids[255];
	int num = 0;

	for (Window *window = windowManager->FirstWindow(); window && num < 255; window = window->Suc())
		if(window->IsNormalWindow()) // Only list browser windows to other apps, right?
			wids[num++] = window->Id();

	wids[num] = 0;

	if (num)
	{
		hDdeData = DdeCreateDataHandle (dwDdeInstance,
		(LPBYTE)&wids, 4 * (num+1) + 1, 0, hszListWindowsTopic, CF_TEXT, NULL);

		return hDdeData;
	}
	else
		return 0;
}

HDDEDATA DDEHandler::HandleDDEOpenURL(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *tmp;
	uni_char *quotedurl;

	if(*str == UNI_L('\"'))
	{
		str++;
		quotedurl =MyUniStrTok(str, UNI_L("\""), pos, done);
		tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	}
	else
		quotedurl =MyUniStrTok(str, UNI_L(","), pos, done);

	OpString escapedurl;

	escapedurl.Set(quotedurl);
	if(OpStatus::IsSuccess(EscapeURLIfNeeded(escapedurl)))
	{
		quotedurl = escapedurl.CStr();
	}

//	MyUniStrTok(str, UNI_L(","), pos, done);
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	DWORD new_window = (tmp) ? DDEStrToULong(tmp) : 0;

	tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	DWORD new_private_tab = (tmp) ? DDEStrToULong(tmp) : 0;

	tmp = MyUniStrTok(str, UNI_L(","), pos, done);

	// the following are ignored
//	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// Flags ignored
//	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// FormData ignored
//	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// MIMEType ignored
//	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// ProgressApp ignored
//	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// ResultApp ignored

	DesktopWindow* window = NULL;
	DWORD wid = 0;

	if(NULL == quotedurl)
	{
		return 0;
	}

	if (!g_desktop_gadget_manager->OpenIfGadgetFolder(OpStringC(str)/*, TRUE*/))	// set to TRUE for standalone gadget
	{
		if(g_application)
		{
			// open_url requests that start with "? " are search queries (syntax used in windows Vista)
			if (!uni_strncmp(quotedurl, UNI_L("? "), 2))
			{
				//create a search template with the default search provider (index 0)
				int search_id = g_searchEngineManager->SearchIndexToID(0);	

				if (!search_id)
				{						
					OP_ASSERT(!"Could not create a search template: are there any search engines installed?");
					return 0;
				}

				// perform the search in the window and with the created search template
				SearchEngineManager::SearchSetting settings;
				RETURN_VALUE_IF_ERROR(settings.m_keyword.Set(&quotedurl[2]), 0);
				settings.m_target_window = g_application->GetActiveDesktopWindow();
				settings.m_search_template = g_searchEngineManager->SearchFromID(search_id);
				settings.m_search_issuer = SearchEngineManager::SEARCH_REQ_ISSUER_OTHERS;
				g_searchEngineManager->DoSearch(settings);
			}
			else
			{
				OpenURLSetting setting;

				setting.m_address.Set(quotedurl);
				if(!setting.m_address.HasContent())
				{
					setting.m_force_new_tab = TRUE;
				}
				setting.m_new_window = new_window || g_application->IsSDI() ? YES : NO;
				setting.m_is_privacy_mode = new_private_tab ? YES : NO;
				setting.m_new_page = YES;
				setting.m_is_remote_command = TRUE;

				if(g_application->OpenURL(setting))
				{
					// setting.m_target_window now contains the window that got created
					window = setting.m_target_window;
				}
			}
		}
		if (window)
		{
			BrowserDesktopWindow *bw = reinterpret_cast<BrowserDesktopWindow *>(window->GetParentDesktopWindow());
			if(bw)
			{
				WindowsOpWindow *op_window = (WindowsOpWindow *)bw->GetOpWindow();
				OP_ASSERT(op_window && op_window->m_hwnd);
				if(op_window && op_window->m_hwnd)
				{
					SetForegroundWindow(op_window->m_hwnd);
				}
			}
			wid = window->GetWindowCommander()->GetWindowId();
		}
	}

	hDdeData = DdeCreateDataHandle (dwDdeInstance, (LPBYTE)&wid, sizeof(wid) + 1, 0, hszOpenURLTopic, CF_TEXT, NULL);

	return hDdeData;
}

HDDEDATA DDEHandler::HandleDDEParseAnchor(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *absurl = MyUniStrTok(str, UNI_L(","), pos, done);
	uni_char *relurl = MyUniStrTok(str, UNI_L(","), pos, done);

	if (*absurl && *relurl)
	{
		uni_char *tmp = new uni_char[uni_strlen(absurl) + uni_strlen(relurl) + 11];

		uni_strcpy(tmp, UNI_L("Parsed URL"));

		if (*tmp)
		{
			hDdeData = DdeCreateDataHandle (dwDdeInstance,
				(LPBYTE)tmp, uni_strlen(tmp) + 1, 0, hszParseAnchorTopic, CF_TEXT, NULL);
			return hDdeData;

		}

		delete [] tmp;
	}
	return 0;
}

HDDEDATA DDEHandler::HandleDDEQueryURLFile(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *filename = MyUniStrTok(str, UNI_L(","), pos, done);

	if (*filename)
	{
		uni_char *tmp = new uni_char[5000];

		uni_sprintf(tmp, UNI_L("Associated URL"));

		if (*tmp)
		{
			hDdeData = DdeCreateDataHandle (dwDdeInstance,
				(LPBYTE)tmp, uni_strlen(tmp) + 1, 0, hszQueryURLFileTopic, CF_TEXT, NULL);
			return hDdeData;
		}

		delete [] tmp;
	}
	return 0;
}

/*HDDEDATA DDEHandler::HandleDDERegisterProtocol(uni_char *str)
{
	short pos = 0;
	BOOL done = FALSE;
	uni_char *newserv = MyUniStrTok(str, UNI_L(","), pos, done);
	uni_char *protocol = MyUniStrTok(str, UNI_L(","), pos, done);

	// nothing here

	return NULL;
}

HDDEDATA DDEHandler::HandleDDERegisterURLEcho(uni_char *str)
{
	short pos = 0;
	BOOL done = FALSE;
	uni_char *echoserver = MyUniStrTok(str, UNI_L(","), pos, done);

	// nothing here

	return NULL;
}

HDDEDATA DDEHandler::HandleDDEUnRegisterURLEcho(uni_char *str)
{
	short pos = 0;
	BOOL done = FALSE;
	uni_char *echoserver = MyUniStrTok(str, UNI_L(","), pos, done);

	// nothing here

	return NULL;
}*/

HDDEDATA DDEHandler::HandleDDEVersion()
{
	DWORD version = 0x00010000;
	hDdeData = DdeCreateDataHandle (dwDdeInstance,
			(LPBYTE)&version, sizeof(version) + 1, 0, hszVersionTopic, CF_TEXT, NULL);
	return hDdeData;
}


HDDEDATA DDEHandler::HandleDDEWindowChange(uni_char *str)
{
	int pos = 0;
	BOOL done = FALSE;
	uni_char *tmp;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);
	DWORD window_id = (tmp) ? DDEStrToULong(tmp) : 0;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// Flags
	DWORD flags = (tmp) ? DDEStrToULong(tmp) : 0;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// X
	long x = (tmp) ? DDEStrToULong(tmp) : -1000;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// Y
	long y = (tmp) ? DDEStrToULong(tmp) : -1000;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// Width
	long width = (tmp) ? DDEStrToULong(tmp) : -1000;
	tmp = MyUniStrTok(str, UNI_L(","), pos, done);		// Height
	long height = (tmp) ? DDEStrToULong(tmp) : -1000;

	if (x < -1000) x = -999;
	if (x > 10000) x = 10000;
	if (y < -1000) y = -999;
	if (y > 10000) y = 10000;
	if (width < -1000) width = -999;
	if (width > 10000) width = 10000;
	if (height < -1000) height = -999;
	if (height > 10000) height = 10000;

	HWND hWnd = 0;
	BOOL tile_now = TRUE;

	if (window_id == 0x00010000)
		hWnd = g_main_hwnd;
	else
	{
		Window* window = windowManager->GetADDEIdWindow(window_id, tile_now);

		if (window)
		{
			WindowsOpWindow *win = (WindowsOpWindow *)window->GetOpWindow();
			if(win)
			{
				hWnd = win->m_hwnd;
			}
		}
	}

	if (hWnd)
	{
		SetActiveWindow(hWnd);

		switch (flags)
		{
			case 0x2L:
				// maximize
				break;

			case 0x4L:
				// normal
				break;

			case 0x8L:
				// minimize
				break;

			case 0x10L:
				SendMessage(hWnd, WM_CLOSE, 0, 0L);
				break;

			case 0x1L:
			default:
			{
				RECT rect;
				GetWindowRect(hWnd, &rect);
				if (x != -1000) rect.left = (int)x;
				if (y != -1000) rect.top = (int)y;
				if (width != -1000) rect.right = rect.left + (int)width;
				if (height != -1000) rect.bottom = rect.top + (int)height;

				POINT p1, p2;
				p1.x = rect.left;
				p1.y = rect.top;
				p2.x = rect.right - rect.left;
				p2.y = rect.bottom - rect.top;

				if (GetParent(hWnd))
					ScreenToClient(GetParent(hWnd), &p1);

				SetWindowPos(hWnd, NULL, p1.x, p1.y, p2.x, p2.y, SWP_NOZORDER);
				break;
			}
		}
	}

	return NULL;
}

DWORD DDEStrToULong(uni_char *str)
{
	uni_char *tmp = uni_stripdup (str);
	if (tmp)
	{
		uni_char *tmp2 = tmp;
		if (tmp[0] == UNI_L('\"'))
			tmp++;
		DWORD window_id = (tmp) ? (DWORD)uni_strtoul(tmp, NULL, 0) : 0;
		delete [] tmp2;
		return window_id;
	}
	return 0;
}


HDDEDATA CALLBACK DDECallbackDummy(UINT wType,
								UINT wFmt,
								HCONV hConv,
								HSZ hsz1,
								HSZ hsz2,
								HDDEDATA hDDEData,
								DWORD dwData1,
								DWORD dwData2)
{
	switch (wType) {
	case 0: //add styff here when needed
	default:
		return NULL;
		break;
	}
}


BOOL DDEHandler::SendExecCmd(uni_char* lpszCmd, uni_char* appName, uni_char* topic)
{
	DWORD dwDDEInst = 0;
	UINT ui;
	HSZ hszServicename;
	HSZ hszTopic;
	HCONV hConv;
	HDDEDATA hExecData;

	//
	// Initialize DDEML.
	//

	ui = DdeInitialize(&dwDDEInst,
						(PFNCALLBACK)DDECallbackDummy,
						CBF_FAIL_ALLSVRXACTIONS,
						(DWORD)0l);

	if (ui != DMLERR_NO_ERROR) {
		return FALSE;
	}

	hszServicename = DdeCreateStringHandle(dwDDEInst, appName, iCodePage);

	hszTopic = DdeCreateStringHandle(dwDDEInst, topic, iCodePage);

	hConv = DdeConnect(dwDDEInst, hszServicename, hszTopic, NULL);

	//
	// Free the HSZ now.
	//

	DdeFreeStringHandle(dwDDEInst, hszServicename);
	DdeFreeStringHandle(dwDDEInst, hszTopic);

	if (!hConv) {
		return FALSE;
	}

	//
	// Create a data handle for the execute string.
	//


	DWORD dLen = (uni_strlen(lpszCmd)*sizeof(uni_char))+1;

	hExecData = DdeCreateDataHandle(dwDDEInst,
									(LPBYTE)lpszCmd,
									dLen,
									0,
									NULL,
									0,
									0);

	//
	// Send the execute request.
	//



	hExecData = DdeClientTransaction((LPBYTE)hExecData,
									ULONG_MAX,
									hConv,
									NULL,
									CF_TEXT,
									XTYP_EXECUTE,
									TIMEOUT_ASYNC , // ms timeout
									NULL);


	BOOL ret;
	if(!hExecData)
		ret = FALSE;
	else ret = TRUE;

	//
	// Done with the conversation now.
	//

	DdeDisconnect(hConv);

	//
	// Done with DDEML.
	//

	DdeUninitialize(dwDDEInst);

	return ret;
}

void DDEHandler::Start()
{
	m_process=TRUE;

	int n = m_delayed_executes.GetCount();
	
	for (int i = 0; i < n; i++)
		ExecuteCommand(m_delayed_executes.Get(i));

	m_delayed_executes.DeleteAll();
}

#endif
