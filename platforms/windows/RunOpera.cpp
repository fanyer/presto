/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core\pch.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "modules/formats/uri_escape.h"

extern KeywordIndex commandline_arglist[];	//declared in opera.cpp

extern BOOL InitRunOperaDDE(HWND hWnd, const char *ServiceNameRunOpera, const char *servername);
extern void CleanUpRunOperaDDE();    
extern long RunOperaDDEActivate(long win_id);
extern long RunOperaDDEGetURL(const char *url, const char *savename, long win_id, long flags, const char *form_post_data, const char *form_post_mime_type, BOOL want_progress, BOOL open_new_window, BOOL open_private_tab);

char *TestServiceName;          

int RunOpera(OpVector<OpString>* commandlineurls, OpString8* signature)
{

	TestServiceName = new char[signature->Length()+1];
	strcpy(TestServiceName, signature->CStr());

	unsigned int i;
	InitRunOperaDDE(NULL, TestServiceName, "Opera");
	if (commandlineurls->GetCount() > 0)
	{
		for(i=0; i < commandlineurls->GetCount(); i++)
		{
			OpString utf8_url_16, url_encoded;
			
			// Convert the url to utf8
			OpString8 utf8_url_8;
			utf8_url_8.SetUTF8FromUTF16(commandlineurls->Get(i)->CStr());
			utf8_url_16.Set(utf8_url_8.CStr());

			// Encode the unicode characters
			INT32 escape = UriEscape::CountEscapes(utf8_url_16.CStr(), UriEscape::Range_80_ff);
			url_encoded.Reserve(utf8_url_16.Length() + escape * 2 + 1);
			INT32 read;
			UriEscape::Escape(url_encoded.CStr(), url_encoded.Capacity(), utf8_url_16.CStr(), utf8_url_16.Length(), UriEscape::Range_80_ff, &read);
			url_encoded.CStr()[read + escape * 2] = 0;

			// Convert to ansi safely
			OpString8 url8;
			url8.Set(url_encoded.CStr());
			
			RunOperaDDEGetURL(url8.CStr(), NULL, 0x00000000, 0, NULL, NULL, TRUE, CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewWindow) != NULL, CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewPrivateTab) != NULL);
		}
	}
	else
	{
//		RunOperaDDEGetURL("opera:blank", NULL, 0x00000000, 0, NULL, NULL, TRUE, (BOOL)CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewWindow), (BOOL)CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewPrivateTab));
		RunOperaDDEGetURL("", NULL, 0x00000000, 0, NULL, NULL, TRUE, CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewWindow) != NULL, CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewPrivateTab) != NULL);
	}

	RunOperaDDEActivate(0xFFFFFFFF);
	CleanUpRunOperaDDE();

	
	delete [] TestServiceName;

    return 0;
}

    
char RunOperaServer[40];           
HSZ hszServiceRunOpera;            
char ServiceNameRunOpera[200];
FARPROC lpfnDdeProcRunOpera ;
DWORD   dwDdeInstanceRunOpera ;              

HSZ 	hszActivateTopicRunOpera;
HSZ		hszOpenURLTopicRunOpera;
HDDEDATA hDdeDataRunOpera;

HDDEDATA CALLBACK DDECallbackFunc ( UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2,
									HDDEDATA hdata, DWORD dwData1, DWORD dwData2);


BOOL InitRunOperaDDE(HWND hWnd, const char *SrvcName, const char *servername)
{                                                 
	if (!SrvcName || !strlen(SrvcName) || strlen(SrvcName) > 100 ||
		!servername || !strlen(servername) || strlen(servername) > 39)
		return FALSE;             
		
	strcpy(ServiceNameRunOpera, SrvcName);
    strcpy(RunOperaServer, servername);                                               

    DdeInitialize ((LPDWORD) &dwDdeInstanceRunOpera, (PFNCALLBACK) DDECallbackFunc, APPCMD_CLIENTONLY, 0L) ;
    hszServiceRunOpera = DdeCreateStringHandleA(dwDdeInstanceRunOpera, ServiceNameRunOpera, CP_WINANSI) ;
    DdeNameService (dwDdeInstanceRunOpera, hszServiceRunOpera, NULL, DNS_REGISTER) ;                           
    
 	hszActivateTopicRunOpera = DdeCreateStringHandleA (dwDdeInstanceRunOpera, "WWW_Activate", CP_WINANSI);
	hszOpenURLTopicRunOpera = DdeCreateStringHandleA (dwDdeInstanceRunOpera, "WWW_OpenURL", CP_WINANSI) ;
	
	return TRUE;
}

void CleanUpRunOperaDDE()    
{
    DdeFreeStringHandle (dwDdeInstanceRunOpera, hszActivateTopicRunOpera) ;
    DdeFreeStringHandle (dwDdeInstanceRunOpera, hszOpenURLTopicRunOpera) ;
	DdeFreeStringHandle (dwDdeInstanceRunOpera, hszServiceRunOpera);

    DdeUninitialize (dwDdeInstanceRunOpera) ;
}


HDDEDATA CALLBACK DDECallbackFunc ( UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2,
									HDDEDATA hdata, DWORD dwData1, DWORD dwData2) 
{
	return ((HDDEDATA)NULL);
}

static HCONV    hConvRunOpera ;  
DWORD dwDataLongRunOpera ;
LPBYTE      lpDataRunOpera ;

BOOL PerformRunOperaDDEOperation(const char *topic, const char *item, char *buf)
{                                                         
	BOOL retval = FALSE;		                  
	buf[0] = 0;
	
    HSZ hszService = DdeCreateStringHandleA (dwDdeInstanceRunOpera, RunOperaServer, CP_WINANSI) ;
    HSZ hszTopic = DdeCreateStringHandleA (dwDdeInstanceRunOpera, topic, CP_WINANSI) ;
    hConvRunOpera = DdeConnect (dwDdeInstanceRunOpera, hszService, hszTopic, NULL) ;
    if (hConvRunOpera != 0L)  // if linked, request data 
    {
		INT32 len = strlen(item) + 1;

		hDdeDataRunOpera = DdeClientTransaction ((LPBYTE)item, len, hConvRunOpera, NULL, CF_TEXT, XTYP_EXECUTE, TIMEOUT_ASYNC, &dwDataLongRunOpera) ;

        if (hDdeDataRunOpera)
        {
	       	lpDataRunOpera = DdeAccessData (hDdeDataRunOpera, &dwDataLongRunOpera) ;            
	        if (dwDataLongRunOpera < 256 && lpDataRunOpera)
	        {            	
	            memcpy(buf, (char *)lpDataRunOpera, dwDataLongRunOpera);
	            retval = TRUE;
	        }
    	    DdeFreeDataHandle (hDdeDataRunOpera) ;
    	}                   
	}
	else
	{	
		DWORD lastDDEerror = DdeGetLastError(dwDdeInstanceRunOpera);
		switch(lastDDEerror)
		{
		case DMLERR_NO_CONV_ESTABLISHED:
			{
				// could not connect with this name, most probable cause is that another user is using Opera with this profile
				static BOOL has_shown_warning = FALSE;
				if(!has_shown_warning)
				{
					MessageBoxA(NULL, "Another user is running this copy of Opera.\n\nYou should install Opera with individual profiles to allow multiple users to run the same copy independently.", "Opera", MB_OK | MB_ICONERROR);
					has_shown_warning = TRUE;
				}
			}
			break;
		default:
			;
		}
	}
    DdeFreeStringHandle (dwDdeInstanceRunOpera, hszService) ;
    DdeFreeStringHandle (dwDdeInstanceRunOpera, hszTopic) ;     
	DdeDisconnect(hConvRunOpera);

    return retval;
}


long RunOperaDDEActivate(long win_id)
{
	char buf[260];
	char tmp[15];
	sprintf(tmp, "%ld", win_id);
	if (PerformRunOperaDDEOperation("WWW_Activate", tmp, buf))
    	return buf[0] + 256 * (buf[1] + 256 * (buf[2] + 256 * buf[3]));
    else
    	return 0;	
}


long RunOperaDDEGetURL(const char *url, const char *savename, long win_id, long flags, const char *form_post_data, const char *form_post_mime_type, BOOL want_progress, BOOL open_new_window, BOOL open_private_tab)
{
	char buf[260];
	char tmp[4000];
	snprintf(tmp, 4000, "\"%s\",%s,%s,%s,%ld,%ld,%s,%s,%s", url, open_new_window ? "1" : "0", open_private_tab ? "1" : "0", (savename) ? savename : "", win_id, flags, 
		(form_post_data) ? form_post_data : "", (form_post_mime_type) ? form_post_mime_type : "", 
		(want_progress) ? ServiceNameRunOpera : "");

	if (PerformRunOperaDDEOperation("WWW_OpenURL", tmp, buf))
    	return buf[0] + 256 * (buf[1] + 256 * (buf[2] + 256 * buf[3]));
    else
    	return 0;	
}

