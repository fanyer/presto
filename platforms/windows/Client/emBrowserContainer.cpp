/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** emBrowserContainer.cpp : Defines the entry point for the application.
*/

/*  This is a simple example on how to use the Opera EmBrowser API
 *
 *  History
 *  ===================================================================
 *  Date : 090403 - Initial - André Schultz <andre@opera.com>
 *  Date : 100303 - Fix for keyboard hooks being processed - André Schultz <andre@opera.com>
 *  Date : 101703 - Milestone 1 together with build 3258 - André Schultz <andre@opera.com>
 *
 *
 *
 *
 *
 *
*/

#include <stdio.h>
#include <wchar.h>


#include "emBrowserContainer.h"
#include "embrowsertestsuite.h"

#include "commctrl.h" 

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;											// current instance
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];
TCHAR szPopupWindowClass[MAX_LOADSTRING];

EmBrowserInitLibraryProc gInitLibraryProc;
EmBrowserInitParameterBlock gInitParamBlock;
EmBrowserProcessOperaMessageProc gProcessOperaMessage;
EmBrowserRef gEmBrowser = NULL;  // our first browser widget
EmBrowserRef gSecondBrowser = NULL;  // our second browser widget
EmBrowserRef gTestWidget = NULL;  // pointer to browser widget used for testing
HINSTANCE gOperaLib = NULL;
HWND gMainWin = NULL;
HWND gToolbar = NULL;
EmBrowserStatus gFuncStat = emBrowserGenericErr;


EmBrowserStatus InitBrowser(HWND hwnd);
EmBrowserStatus CloseBrowser();
EmBrowserStatus ShowCookies();
EmBrowserStatus ShowPageCookies();
EmBrowserStatus RemoveAllCookies();
EmBrowserStatus RemovePageCookies();
EmBrowserStatus SetOneCookie();
EmBrowserStatus ShowHistory();
EmBrowserStatus GoBack();
EmBrowserStatus GoForward();
EmBrowserStatus RemoveCurrent();
EmBrowserStatus RemoveFirst();
EmBrowserStatus RemoveLast();
EmBrowserStatus MoveCurrentBack();
EmBrowserStatus MoveCurrentForward();
EmBrowserStatus ShowThumbnail();
EmBrowserStatus ReparentBrowserWidget(HWND newparent);
EmBrowserStatus SetLanguageFile();
EmBrowserStatus RemoveAllHistoryItems();
EmBrowserStatus SetVisLinks(BOOL visible);
EmBrowserStatus TestJavascript();
EmBrowserStatus TestJavascript2();
EmBrowserStatus TestSuite();
EmBrowserStatus ToggleSSR();
EmBrowserStatus ToggleFitToWidth();

void ShowLastError();

#ifdef _DEBUG
    FILE * dumpf = NULL;
#endif

void thumbnailCallback (IN void *clientData, IN EmThumbnailStatus status, IN EmThumbnailRef);

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_EMBROWSERCONTAINER, szWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDC_EMBROWSERPOPUP,szPopupWindowClass, MAX_LOADSTRING);

	//Register windowsclasses
	RegisterMainClass(hInstance);
	RegisterPopupClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_EMBROWSERCONTAINER);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		//
		if ((gProcessOperaMessage && !gProcessOperaMessage(&msg))/* && !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)*/) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}



//
//  FUNCTION: RegisterMainClass()
//
//  PURPOSE: Registers the window class.
//
ATOM RegisterMainClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_EMBROWSERCONTAINER);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCSTR)IDC_EMBROWSERCONTAINER;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

//
//  FUNCTION: RegisterPopupClass()
//
//  PURPOSE: Registers the window class.
//
ATOM RegisterPopupClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)PopupWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_EMBROWSERCONTAINER);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szPopupWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

TBBUTTON tbButton[] =           // Array defining the toolbar buttons 
{     
	{0,		IDM_BACK,   TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},      
	{1,		IDM_FORWARD,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},      
	{2,		IDM_STOP,   TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},      
	{3,		IDM_RELOAD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},     
	{4,		IDM_GOTO,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0}
};

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   gMainWin = hWnd;

   gToolbar = CreateToolbarEx(gMainWin, WS_CHILD | WS_MAXIMIZE | WS_DLGFRAME | WS_VISIBLE, IDR_TOOLBAR1, 5, hInstance, IDR_TOOLBAR1, tbButton, sizeof(tbButton)/sizeof(TBBUTTON), 18, 16, 18, 16, sizeof(TBBUTTON)); 

   if (!gToolbar)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDM_REMOVEPAGECOOKIES:
				   RemovePageCookies();
				   break;
				case IDM_REMOVEALLCOOKIES:
				   RemoveAllCookies();
				   break;
				case IDM_SHOWPAGECOOKIES:
				   ShowPageCookies();
				   break;
				case IDM_SHOWCOOKIES:
				   ShowCookies();
				   break;
				case IDM_SETCOOKIE:
				   DialogBox(hInst, (LPCTSTR)IDD_SETCOOKIEBOX, hWnd, (DLGPROC)SetCookie);
				   break;
				case IDM_SETCOOKIEVAL:
				   SetOneCookie();
				   break;
				case IDM_BACK:
				   GoBack();
				   break;
				case IDM_FORWARD:
				   GoForward();
				   break;
				case IDM_REMOVECURRENT:
				   RemoveCurrent();
				   break;
				case IDM_REMOVEFIRST:
				   RemoveFirst();
				   break;
				case IDM_REMOVELAST:
				   RemoveLast();
				   break;
				case IDM_REMOVEALL:
				   RemoveAllHistoryItems();
				   break;
				case IDM_MOVECURRENTBACK:
				   MoveCurrentBack();
				   break;
				case IDM_MOVECURRENTFORWARD:
				   MoveCurrentForward();
				   break;
				case IDM_HISTORY:
				   ShowHistory();
				   break;
				case IDM_ABOUT:
				   DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDM_GOTO:
				   DialogBox(hInst, (LPCTSTR)IDD_GOTOBOX, hWnd, (DLGPROC)Goto);
				   break;
				case IDM_RELOAD:
				   gInitParamBlock.browserMethods->handleMessage(gEmBrowser, emBrowser_msg_reload);
				   break;
				case IDM_STOP:
				   gInitParamBlock.browserMethods->handleMessage(gEmBrowser, emBrowser_msg_stop_loading);
				   break;

				/*
				case IDM_STYLESHEET:
                    {
                        int ac = gInitParamBlock.browserMethods->getAltStylesheetCount(gEmBrowser);
                        if(ac > 0)
                        {
                            unsigned short tit[128];
                            gInitParamBlock.browserMethods->getAltStylesheetTitle(gEmBrowser, ac-1, 128, tit, NULL);
    					    gInitParamBlock.browserMethods->enableAltStylesheet(gEmBrowser, tit);

                        }
                    }
				   break;
*/
				case IDM_THUMBNAIL:
                    ShowThumbnail();
				   break;
				case IDM_PRINT:
				   gInitParamBlock.browserMethods->handleMessage(gEmBrowser, emBrowser_msg_print);
				   break;
				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				case IDM_NEWWIN:
					ReparentBrowserWidget(NULL);
					break;
				case IDM_SETLANG:
					SetLanguageFile();
					break;
				case IDM_DISABLEVISLINKS:
					SetVisLinks(FALSE);
					break;
				case IDM_ENABLEVISLINKS:
					SetVisLinks(TRUE);
					break;
				case IDM_TESTJAVASCRIPT:
					TestJavascript();
					break;
				case IDM_PAGEINTRUDER:
					TestJavascript2();
					break;
				case IDM_TESTRUN:
                    TestSuite();
					break;
				case IDM_TOGGLE_SSR:
					ToggleSSR();
					break;
				case IDM_TOGGLE_FTW:
					ToggleFitToWidth();
					break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code here...

			EndPaint(hWnd, &ps);
			break;
		case WM_CREATE:
			if(!gOperaLib)
			{
				if(InitBrowser(hWnd) != emBrowserNoErr)
				{
					PostQuitMessage(0);
				}
			}
			break;
		case WM_SIZE:
			if (gEmBrowser)
			{
				EmBrowserRect bounds;
			
				RECT containerRect;
				RECT toolbarRect;

				GetClientRect(hWnd, &containerRect);
				
				GetClientRect(gToolbar, &toolbarRect);

				bounds.left = containerRect.left;
				bounds.top = containerRect.top+toolbarRect.bottom+1;	//size of toolbar
				bounds.right = containerRect.right;
				bounds.bottom = containerRect.bottom;

				gInitParamBlock.browserMethods->setLocation(gEmBrowser, &bounds, NULL);
			}
			break;
		case WM_ACTIVATE:
			{
				BOOL fActive = LOWORD(wParam) == WA_ACTIVE;
				if(gEmBrowser && gInitParamBlock.browserMethods)
				{
					gInitParamBlock.browserMethods->setFocus(gEmBrowser, fActive);
				}
			}
			break;
		case WM_NCDESTROY:
			CloseBrowser();
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}

// Mesage handler for goto url box.
LRESULT CALLBACK Goto(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_ACTIVATE:
			{
				if(LOWORD(wParam) == WA_ACTIVE)
				{
					SetFocus(GetDlgItem(hDlg, IDC_URL));
				}
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				if(LOWORD(wParam) == IDOK)
				{
					EmBrowserString destURL = new EmBrowserChar[512];
					char* destURLA = new char[512];

					GetDlgItemText(hDlg, IDC_URL, destURLA, 512);
					
					MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, destURLA, -1, destURL, 511);				
					
					if (gInitParamBlock.browserMethods->openURL)
					{		
						gInitParamBlock.browserMethods->openURL(gEmBrowser, (EmBrowserString)destURL, emBrowserURLOptionDefault);
					}

					delete [] destURLA;
					delete [] destURL;
				}

				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}

// Mesage handler for set cookie box.
LRESULT CALLBACK SetCookie(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_ACTIVATE:
			{
				if(LOWORD(wParam) == WA_ACTIVE)
				{
					SetFocus(GetDlgItem(hDlg, IDC_COOKIEREQUEST));
				}
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				if(LOWORD(wParam) == IDOK)
				{
					EmBrowserString request = new EmBrowserChar[512];
					char* requestA = new char[512];

					GetDlgItemText(hDlg, IDC_COOKIEREQUEST, requestA, 512);

   					MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, requestA, -1, request, 511);				

					
					if (gInitParamBlock.browserMethods->setCookieRequest)
					{		        
                        EmDocumentRef document;
                        EmBrowserChar url_str[128];
                        EmBrowserString domain;
                        EmBrowserString path;
                        gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);
                        gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
                        if(wcsncmp(url_str,L"http://",7) == 0)
						{
							domain = url_str + 7;  
							if(domain) path = wcschr(domain,'/');
							if(path)
							{
								*path = 0;
								path += 1;
                                EmBrowserString endp = wcsrchr(path,'/');
                                if(endp) *(endp+1) = 0;
							}
							gInitParamBlock.browserMethods->setCookieRequest(domain, path, (EmBrowserString)request);
						}
					}		
				}

				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}

// Mesage handler for popup windows
LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_ACTIVATE:
		{
			BOOL fActive = LOWORD(wParam) == WA_ACTIVE;
			if(gSecondBrowser && gInitParamBlock.browserMethods)
			{
				gInitParamBlock.browserMethods->setFocus(gSecondBrowser, fActive);
			}
		}
		break;
	case WM_SIZE:
		if (gSecondBrowser)
		{
			EmBrowserRect bounds;
			
			RECT containerRect;
			
			GetClientRect(hWnd, &containerRect);
			
			bounds.left = containerRect.left;
			bounds.top = containerRect.top;
			bounds.right = containerRect.right;
			bounds.bottom = containerRect.bottom;
			
			gInitParamBlock.browserMethods->setLocation(gSecondBrowser, &bounds, NULL);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...

		EndPaint(hWnd, &ps);
		break;
		
	case WM_DESTROY:
		{
			//set the mainwindow as parent again
			EmBrowserRect bounds;
			
			RECT containerRect;
			
			GetClientRect(gMainWin, &containerRect);
			
			bounds.left = containerRect.left;
			bounds.top = containerRect.top;
			bounds.right = containerRect.right;
			bounds.bottom = containerRect.bottom;
			
			gInitParamBlock.browserMethods->setLocation(gEmBrowser, &bounds, (void*)gMainWin);
		}
		break;
	case WM_NCDESTROY:
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		
	}
    return FALSE;
}


//
//  FUNCTION: InitBrowser(HWND parent)
//
//  PURPOSE:  Initializes EmBrowser object
//
//  parent	- handle to parent window
//

EmBrowserStatus InitBrowser(HWND parent)
{
	unsigned long maxdirlen = MAX_PATH;
	char* operadir = new char[MAX_PATH];
	*operadir = 0;
#ifndef _DEBUG
	if(GetOperaDirectory(operadir, maxdirlen))
	{
		char* operalib = new char[MAX_PATH];
		_snprintf(operalib, MAX_PATH, "%s%copera.dll", operadir, PATHSEPCHAR);
		gOperaLib = LoadLibrary(operalib);
		delete [] operalib;
	}
	else	//try current directory
#endif //!_DEBUG
    {
        gOperaLib = LoadLibrary("opera.dll");
	}

	delete [] operadir;
	
#ifdef _DEBUG
//    if(!dumpf) dumpf = fopen("emdump.txt","w");
#endif

	//clear the EmBrowserInitParameterBlock 
	memset(&gInitParamBlock, 0, sizeof(gInitParamBlock));

	if(gOperaLib)
	{
		gInitLibraryProc = (EmBrowserInitLibraryProc)GetProcAddress(gOperaLib, "WidgetInitLibrary");

		gProcessOperaMessage = (EmBrowserProcessOperaMessageProc)GetProcAddress(gOperaLib, "TranslateOperaMessage");

		if(gInitLibraryProc)
		{

			//Initialize callbacks before the library is initialized
			gInitParamBlock.notifyCallbacks = new EmBrowserAppNotification;
			gInitParamBlock.notifyCallbacks->notification = BrowserNotification;
			gInitParamBlock.notifyCallbacks->beforeNavigation = BeforeNavigation;
			gInitParamBlock.notifyCallbacks->afterNavigation = AfterNavigation;
			gInitParamBlock.notifyCallbacks->redirNavigation = RedirNavigation;
			gInitParamBlock.notifyCallbacks->allowLocalFileURL = AllowLocalFileURL;
			gInitParamBlock.notifyCallbacks->handleProtocol = NULL;
			gInitParamBlock.notifyCallbacks->newBrowser = NewBrowser;
			gInitParamBlock.notifyCallbacks->posChange = NULL;
			gInitParamBlock.notifyCallbacks->sizeChange = NULL;
			gInitParamBlock.notifyCallbacks->javascript  = NULL;

			EmVendorBrowserInitParameterBlock vendordata;

			vendordata.majorVersion = 2;
			vendordata.minorVersion = 0;
			strcpy(vendordata.registrationCode, "somevalidcodesuppliedbyOpera");

			gInitParamBlock.vendorData = &vendordata;
			
			gInitParamBlock.initParams = emBrowserInitDefault; // emBrowserInitNoContextMenu | emBrowserInitDisableHotclickMenu;

			//Initialize the library, this will also set up all function pointers

//			gInitParamBlock.writeLocation = (void*)L"C:\\embrowser";

			EmBrowserStatus libStatus = gInitLibraryProc(&gInitParamBlock);

			if(libStatus == emBrowserNoErr)
			{
				// Add custom user agent comment to distinguish us from regular Opera
				if (gInitParamBlock.browserMethods->addUAComponent)
				{
					gInitParamBlock.browserMethods->addUAComponent(L"EmBrowser TestApp");
				}

				EmBrowserRect bounds;
				EmBrowserString destURL = L"http://www.opera.com";
			
				RECT containerRect;

				GetClientRect(parent, &containerRect);
					
				bounds.left = containerRect.left;
				bounds.top = containerRect.top;
				bounds.right = containerRect.right;
				bounds.bottom = containerRect.bottom;

				if (gInitParamBlock.browserMethods->openURL)
				{		
					EmBrowserStatus ret = gInitParamBlock.createInstance(parent, emBrowserOptionDisableAppkeys|emBrowserOptionDisableTooltips|emBrowserOptionNoContextMenu|emBrowserOptionDisableDownloadDialog|emBrowserOptionDisableUrlErrorMessage/*|emBrowserOptionDisableAllDrags*//*emBrowserOptionUseDefaults*/, &gEmBrowser);

					gInitParamBlock.browserMethods->setLocation(gEmBrowser, &bounds, NULL);				                  
					gInitParamBlock.browserMethods->setVisible(gEmBrowser, true);
					gFuncStat = gInitParamBlock.browserMethods->openURL(gEmBrowser, destURL, emBrowserURLOptionDefault);

					ret = gInitParamBlock.browserMethods->usePluginPath(gEmBrowser, L"C:/Programfiler/Opera/Program/Plugins/");
					if (ret != emBrowserNoErr)
						return ret;
				}	
            }
			return emBrowserNoErr;
		}
		else
		{
			ShowLastError();
			return emBrowserGenericErr;
		}
	}

	ShowLastError();
	return emBrowserGenericErr;
}

//
//  FUNCTION: CloseBrowser()
//
//  PURPOSE:  Terminates EmBrowser object
//
//

EmBrowserStatus CloseBrowser()
{
	if(gFuncStat == emBrowserNoErr)
	{
		gInitParamBlock.shutdown();
	}
	FreeLibrary(gOperaLib);

	gProcessOperaMessage = NULL;

#ifdef _DEBUG
    if(dumpf)
		fclose(dumpf);
#endif
	return emBrowserNoErr;
}

//
//  FUNCTION: ShowCookies()
//
//  PURPOSE:  Display all registred cookies in a message box
//            Demonstrates the use of getCookie 
//

EmBrowserStatus ShowCookies()
{
    if ( 
        gInitParamBlock.browserMethods->getRootDoc        &&
        gInitParamBlock.browserMethods->getCookieCount    &&
        gInitParamBlock.browserMethods->getCookieName     &&
        gInitParamBlock.browserMethods->getCookieValue    &&
        gInitParamBlock.browserMethods->getCookieExpiry   &&
        gInitParamBlock.browserMethods->getCookiePath     &&
        gInitParamBlock.browserMethods->getCookieDomain   &&
        gInitParamBlock.browserMethods->getCookieSecurity &&
        gInitParamBlock.browserMethods->getCookieList
        )
	{
        long siz = gInitParamBlock.browserMethods->getCookieCount(NULL,NULL);
        EmCookieRef * cookieArray = new EmCookieRef [siz];
		gInitParamBlock.browserMethods->getCookieList(NULL,NULL, cookieArray);

        int i;

        unsigned short * mess;
        unsigned short str[128];
        long str_size;
        BOOL security;
        time_t expiry;

        mess = (unsigned short *)malloc((128 * 2 * siz) + 90 + 600 /*name+url*/);
        mess[0] = '\0';

        if(siz == 0)
            wcscat(mess,L"No cookies found on the page \n");  
        else if(siz < 0)
            wcscat(mess,L"Error occured while searching for cookies \n");  
        else
            for(i=0;i<siz;i++)
            {
                gInitParamBlock.browserMethods->getCookieDomain(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,str);}
                gInitParamBlock.browserMethods->getCookiePath(cookieArray[i], 128, str, &str_size);
                if(str_size) wcscat(mess,str);
                wcscat(mess,L" :: ");

                gInitParamBlock.browserMethods->getCookieName(cookieArray[i], 128, str, NULL);
                wcscat(mess,str);wcscat(mess,L" = ");
                gInitParamBlock.browserMethods->getCookieValue(cookieArray[i], 128, str, NULL);
                wcscat(mess,str);
                expiry = (time_t)gInitParamBlock.browserMethods->getCookieExpiry(cookieArray[i]);
                if(expiry) { wcscat(mess,L"; expires = "); wcscat(mess,_wctime( &expiry )); }
                gInitParamBlock.browserMethods->getCookieReceivedDomain(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,L"; domain = "); wcscat(mess,str);}
                gInitParamBlock.browserMethods->getCookieReceivedPath(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,L"; path = "); wcscat(mess,str);}
                security = (BOOL)gInitParamBlock.browserMethods->getCookieSecurity(cookieArray[i]);
                if(security) {wcscat(mess,L"; secure ");}
                wcscat(mess,L" \n");  
            }

        MessageBoxW(gMainWin,mess,L"Cookies",MB_OK);

        free(mess);
	}		


	return emBrowserNoErr;
}


//
//  FUNCTION: ShowPageCookies()
//
//  PURPOSE:  Display cookies in the current page in a message box
//            Demonstrates the use of getCookie 
//

EmBrowserStatus ShowPageCookies()
{
    if ( 
        gInitParamBlock.browserMethods->getRootDoc        &&
        gInitParamBlock.browserMethods->getCookieCount    &&
        gInitParamBlock.browserMethods->getCookieName     &&
        gInitParamBlock.browserMethods->getCookieValue    &&
        gInitParamBlock.browserMethods->getCookieExpiry   &&
        gInitParamBlock.browserMethods->getCookiePath     &&
        gInitParamBlock.browserMethods->getCookieDomain   &&
        gInitParamBlock.browserMethods->getCookieSecurity &&
        gInitParamBlock.browserMethods->getCookieList
        )
	{
        EmDocumentRef document;
        EmBrowserChar url_str[128];
		EmBrowserChar title_str[128];
		long title_len = 0;
        EmBrowserString domain = NULL;
        EmBrowserString path = NULL;
        gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);
        gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
        gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
		gInitParamBlock.browserMethods->getText(gEmBrowser, emBrowserTitleText, 128, title_str, &title_len);

		long siz = 0;
        if(wcsncmp(url_str,L"http://",7) == 0)
		{
			domain = url_str + 7;  
			if(domain) path = wcschr(domain,'/');
			if(path)
			{
				*path = 0;
				path += 1;
			}
        
			siz = gInitParamBlock.browserMethods->getCookieCount(domain,path);
		}
        EmCookieRef * cookieArray = new EmCookieRef [siz];
		gInitParamBlock.browserMethods->getCookieList(domain,path, cookieArray);

        int i;

        unsigned short * mess;
        unsigned short str[128];
        long str_size;
        BOOL security;
        time_t expiry;

        mess = (unsigned short *)malloc((128 * 2 * siz) + 90 + 600 /*name+url*/);
        mess[0] = '\0';

		if (title_len)
		{
			wcscat(mess,title_str);
			wcscat(mess,L"\n");
		}
		if (url_str && url_str[0])
		{
			wcscat(mess,url_str);
			wcscat(mess,L"\n");
		}

        if(siz == 0)
            wcscat(mess,L"No cookies found on the page \n");  
        else if(siz < 0)
            wcscat(mess,L"Error occured while searching for cookies \n");  
        else
            for(i=0;i<siz;i++)
            {
                gInitParamBlock.browserMethods->getCookieDomain(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,str);}
                gInitParamBlock.browserMethods->getCookiePath(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,str);}
                wcscat(mess,L" :: ");

                gInitParamBlock.browserMethods->getCookieName(cookieArray[i], 128, str, NULL);
                wcscat(mess,str);wcscat(mess,L" = ");
                gInitParamBlock.browserMethods->getCookieValue(cookieArray[i], 128, str, NULL);
                wcscat(mess,str);
                expiry = (time_t)gInitParamBlock.browserMethods->getCookieExpiry(cookieArray[i]);
                if(expiry) { wcscat(mess,L"; expires = "); wcscat(mess,_wctime( &expiry )); }
                gInitParamBlock.browserMethods->getCookieReceivedDomain(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,L"; domain = "); wcscat(mess,str);}
                gInitParamBlock.browserMethods->getCookieReceivedPath(cookieArray[i], 128, str, &str_size);
                if(str_size) {wcscat(mess,L"; path = "); wcscat(mess,str);}
                security = (BOOL)gInitParamBlock.browserMethods->getCookieSecurity(cookieArray[i]);
                if(security) {wcscat(mess,L"; secure ");}
                wcscat(mess,L" \n");  
            }

        MessageBoxW(gMainWin,mess,L"Cookies",MB_OK);

        free(mess);
	}		


	return emBrowserNoErr;
}


//
//  FUNCTION: RemoveAllCookies()
//
//  PURPOSE:  Remove all registred cookies in Opera
//            Demonstrates the use of removeCookies 
//

EmBrowserStatus RemoveAllCookies()
{
    if ( 
        gInitParamBlock.browserMethods->removeCookies
        )
	{
		EmBrowserStatus ret = gInitParamBlock.browserMethods->removeCookies(NULL, NULL, NULL);
        if(ret < 0)
        {
            unsigned short * mess;

            mess = (unsigned short *)malloc(64 * 2);
            mess[0] = '\0';

            if(ret == emBrowserOutOfMemoryErr)
                wcscat(mess,L"Out of memory while removing cookies \n");  
            else
                wcscat(mess,L"Unspecified error occured while removing cookies \n");  

            MessageBoxW(gMainWin,mess,L"Cookies",MB_OK);

            free(mess);
            return emBrowserNoErr;
        }

	}		


	return emBrowserNoErr;
}


//
//  FUNCTION: RemovePageCookies()
//
//  PURPOSE:  Remove cookies valid in the current page.
//            Demonstrates the use of removeCookies
//

EmBrowserStatus RemovePageCookies()
{
    if ( 
        gInitParamBlock.browserMethods->getRootDoc        &&
        gInitParamBlock.browserMethods->removeCookies     &&
        gInitParamBlock.browserMethods->getURL   
        )
	{
        EmDocumentRef document;
        EmBrowserChar url_str[128];
        EmBrowserString domain = NULL;
        EmBrowserString path   = NULL;
        EmBrowserString endp   = NULL;
        gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);
        gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
        if(wcsncmp(url_str,L"http://",7) == 0)
		{
            domain = url_str + 7;  
			if(domain) path = wcschr(domain,'/');
			if(path)
			{
				*path = 0;
				path += 1;
				endp = wcsrchr(path,'/');
			}
			if(endp) *endp = 0;
        
			gInitParamBlock.browserMethods->removeCookies(domain,path, NULL);
		}
    }
    return emBrowserNoErr;
}


//
//  FUNCTION: SetOneCookie()
//
//  PURPOSE:  Set the value of the first cookie to "Hello world"
//            Demonstrates the use of setCookieValue 
//

EmBrowserStatus SetOneCookie()
{
    if ( 
        gInitParamBlock.browserMethods->getRootDoc        &&
        gInitParamBlock.browserMethods->getCookieCount    &&
        gInitParamBlock.browserMethods->setCookieValue    &&
        gInitParamBlock.browserMethods->getCookieList
        )
	{
        EmDocumentRef document;
        EmBrowserChar url_str[128];
        EmBrowserString domain = NULL;
        EmBrowserString path = NULL;
        gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);
        gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
        if(wcsncmp(url_str,L"http://",7) == 0)
            domain = url_str + 7;  
        if(domain) path = wcschr(domain,'/');
        if(path)
        {
            *path = 0;
            path += 1;
        }
        
        long siz = gInitParamBlock.browserMethods->getCookieCount(domain,path);

        if(siz <= 0)
        {
            unsigned short * mess;

            mess = (unsigned short *)malloc((128 * 2 * siz) + 90);
            mess[0] = '\0';

            if(siz == 0)
                wcscat(mess,L"No cookies found on the page \n");  
            else 
                wcscat(mess,L"Error occured while searching for cookies \n");  

            MessageBoxW(gMainWin,mess,L"Cookies",MB_OK);

            free(mess);
            return emBrowserNoErr;
        }

        EmCookieRef * cookieArray = new EmCookieRef [siz];
		gInitParamBlock.browserMethods->getCookieList(domain,path, cookieArray);

        gInitParamBlock.browserMethods->setCookieValue(domain,path, cookieArray[0], L"Hello world");
	}		


	return emBrowserNoErr;
}


//
//  FUNCTION: ShowHistory()
//
//  PURPOSE:  Display current history in a message box
//            Demonstrates the use of getHistoryList, getHistoryItemTitle etc
//
//            historyArray will contain references to the history items.
//            From each item the title is extracted. The title is preceded by a '>'
//            to mark the current document.

EmBrowserStatus ShowHistory()
{
    if (gInitParamBlock.browserMethods->getHistoryList && 
        gInitParamBlock.browserMethods->getHistorySize &&
        gInitParamBlock.browserMethods->getHistoryItemTitle &&
        gInitParamBlock.browserMethods->getHistoryIndex
        )
	{	
        long siz = gInitParamBlock.browserMethods->getHistorySize(gEmBrowser);
        long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
        EmHistoryRef * historyArray = new EmHistoryRef [siz];
		gInitParamBlock.browserMethods->getHistoryList(gEmBrowser, historyArray);

        int i;

        unsigned short * mess;
        unsigned short title[128];

        mess = (unsigned short *)malloc((128 * 2 * siz) + 2);
        mess[0] = '\0';

        for(i=0;i<siz;i++)
        {
            if(ind == i)
                wcscat(mess,L"> ");
            else
                wcscat(mess,L". ");
            gInitParamBlock.browserMethods->getHistoryItemTitle(historyArray[i], 128, title, NULL);
            wcscat(mess,title);wcscat(mess,L" \n");
        }
        MessageBoxW(gMainWin,mess,L"History",MB_OK);

		free(mess);
	}		

	return emBrowserNoErr;
}


//
//  FUNCTION: GoBack()
//
//  PURPOSE:  Move to the previously displayed document, as recorded in the history list.
//            Demonstrates the use of getHistoryIndex & setHistoryIndex 

EmBrowserStatus GoBack()
{
    if (
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->setHistoryIndex
        )
	{	
        long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
        gInitParamBlock.browserMethods->setHistoryIndex(gEmBrowser, ind - 1);
	}		

	return emBrowserNoErr;
}

//
//  FUNCTION: GoForward()
//
//  PURPOSE:  Move to the next displayed document, as recorded in the history list.
//            (Assumes that a backward navigation already is done.)
//            Demonstrates the use of getHistoryIndex & setHistoryIndex 

EmBrowserStatus GoForward()
{
    if (
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->setHistoryIndex
        )
	{	
        long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
        gInitParamBlock.browserMethods->setHistoryIndex(gEmBrowser, ind + 1);
	}		

	return emBrowserNoErr;
}


//
//  FUNCTION: RemoveCurrent()
//
//  PURPOSE:  Remove the current document in the history list.
//            Demonstrates the use of removeHistoryItem 

EmBrowserStatus RemoveCurrent()
{
    if (
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->removeHistoryItem
        )
	{	
        long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
        gInitParamBlock.browserMethods->removeHistoryItem(gEmBrowser, ind );
	}		

	return emBrowserNoErr;
}

//
//  FUNCTION: RemoveFirst()
//
//  PURPOSE:  Remove the first document in the history list.
//            Demonstrates the use of removeHistoryItem 

EmBrowserStatus RemoveFirst()
{
    if (
        gInitParamBlock.browserMethods->removeHistoryItem
        )
	{	
        gInitParamBlock.browserMethods->removeHistoryItem(gEmBrowser, 0 );
	}		

	return emBrowserNoErr;
}


//
//  FUNCTION: RemoveLast()
//
//  PURPOSE:  Remove the last document in the history list.
//            Demonstrates the use of removeHistoryItem 

EmBrowserStatus RemoveLast()
{
    if (
        gInitParamBlock.browserMethods->getHistorySize &&
        gInitParamBlock.browserMethods->removeHistoryItem
        )
	{	
        long siz = gInitParamBlock.browserMethods->getHistorySize(gEmBrowser);
        gInitParamBlock.browserMethods->removeHistoryItem(gEmBrowser, siz - 1 );
	}		

	return emBrowserNoErr;
}


//
//  FUNCTION: MoveCurrentBack()
//
//  PURPOSE:  Move the current page one step backwards in history list.
//            Demonstrates the use of insertHistoryItem 
//            1. Remove the current page.
//               The document is not destroyed and can still be referred through the historyArray
//            2. Insert the page in a new position.
//            3. Set the ny current position in the history list. 
 
EmBrowserStatus MoveCurrentBack()
{
    if (
        gInitParamBlock.browserMethods->getHistorySize &&
        gInitParamBlock.browserMethods->getHistoryList &&
        gInitParamBlock.browserMethods->removeHistoryItem &&
        gInitParamBlock.browserMethods->insertHistoryItem &&
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->setHistoryIndex
        )
	{	
        long siz = gInitParamBlock.browserMethods->getHistorySize(gEmBrowser);
		if (siz > 1)
		{
			EmHistoryRef * historyArray = new EmHistoryRef [siz];
			long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
//			EmDocument currentItem = gInitParamBlock.browserMethods->getHistoryItem(gEmBrowser, ind);
			gInitParamBlock.browserMethods->getHistoryList(gEmBrowser, historyArray);
			EmHistoryRef currentItem = historyArray[ind];

			gInitParamBlock.browserMethods->removeHistoryItem(gEmBrowser, ind );
			gInitParamBlock.browserMethods->insertHistoryItem(gEmBrowser, ind - 1, currentItem);
			gInitParamBlock.browserMethods->setHistoryIndex(gEmBrowser, ind - 1);
		}
	}		

	return emBrowserNoErr;
}


//
//  FUNCTION: MoveCurrentForward()
//
//  PURPOSE:  Move the current page one step forward in history list.
//            Demonstrates the use of insertHistoryItem 
//            1. Remove the current page.
//               The document is not destroyed and can still be referred through the historyArray
//            2. Insert the page in a new position.
//            3. Set the ny current position in the history list. 
 
EmBrowserStatus MoveCurrentForward()
{
    if (
        gInitParamBlock.browserMethods->getHistorySize &&
        gInitParamBlock.browserMethods->getHistoryList &&
        gInitParamBlock.browserMethods->removeHistoryItem &&
        gInitParamBlock.browserMethods->insertHistoryItem &&
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->setHistoryIndex
        )
	{	
        long siz = gInitParamBlock.browserMethods->getHistorySize(gEmBrowser);
		if (siz > 1)
		{
			EmHistoryRef * historyArray = new EmHistoryRef [siz];
			long ind = gInitParamBlock.browserMethods->getHistoryIndex(gEmBrowser);
//          EmDocument currentItem = gInitParamBlock.browserMethods->getHistoryItem(gEmBrowser, ind);
			gInitParamBlock.browserMethods->getHistoryList(gEmBrowser, historyArray);
			EmHistoryRef currentItem = historyArray[ind];

			gInitParamBlock.browserMethods->removeHistoryItem(gEmBrowser, ind );
			gInitParamBlock.browserMethods->insertHistoryItem(gEmBrowser, ind + 1, currentItem);
			gInitParamBlock.browserMethods->setHistoryIndex(gEmBrowser, ind + 1);
		}
	}		

	return emBrowserNoErr;
}

//
//  FUNCTION: RemoveAllHistoryItems()
//
//  PURPOSE:  Remove all documents in the history list.
//            Demonstrates the use of removeAllHistoryItems 

EmBrowserStatus RemoveAllHistoryItems()
{
    if (
        gInitParamBlock.browserMethods->getHistoryIndex &&
        gInitParamBlock.browserMethods->removeAllHistoryItems
        )
	{	
        gInitParamBlock.browserMethods->removeAllHistoryItems(gEmBrowser);
	}		

	return emBrowserNoErr;
}

//
//  FUNCTION: ShowThumbnail()
//
//  PURPOSE:  Create a thumbnail of the current page and display it on the screen.
//            Demonstrates the use of thumbnailRequest 

 
EmBrowserStatus ShowThumbnail()
{
    if (
        gInitParamBlock.browserMethods->thumbnailRequest &&
        gInitParamBlock.browserMethods->getRootDoc)
    {
		EmDocumentRef document;
        unsigned short url[128];

        gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);
        gInitParamBlock.browserMethods->getURL(document,128,url,NULL);
        gInitParamBlock.browserMethods->thumbnailRequest(url,1000,800,20,thumbnailCallback,NULL); 
    }
	return emBrowserNoErr;
}

void JSEvaluateCodeCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSEvaluateCodeCallback2(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSEvaluateCodeCallback3(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
void JSGetPropertyCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value);
EmJavascriptStatus JSMethodCallBack(void *inData, EmDocumentRef inDoc, EmJavascriptObjectRef inObject,
                      unsigned argumentsCount, EmJavascriptValue *argumentsArray,
                      EmJavascriptValue *returnValue);

//
//  FUNCTION: BrowserNotification(EmBrowserRef inst, EmBrowserAppMessage msg, long value)
//
//  PURPOSE:  Callbacks from notifications from EmBrowser object
//
//	inst		- pointer to browser instance
//	msg			- notification message
//	value		- notification value, see "embrowser.h" for documentation on type

void BrowserNotification(EmBrowserRef inst, EmBrowserAppMessage msg, long value)
{

	switch(msg)
	{
	case emBrowser_app_msg_request_focus:
		{
			if(gEmBrowser && gInitParamBlock.browserMethods)
			{
				gInitParamBlock.browserMethods->setFocus(inst, TRUE);
			}			
		}
	break;
	case emBrowser_app_msg_title_changed:
		{
			if(value)
			{
				SetWindowTextW(gMainWin, (EmBrowserString)value);
			}
		}
	break;
	case emBrowser_app_msg_request_close:
		{
			// rough handling to test functionality
			HWND handle = (HWND)gInitParamBlock.browserMethods->getWindowhandle(inst);
			while (GetParent(handle))
			{
				handle = GetParent(handle);
			}
			gInitParamBlock.destroyInstance(inst);
			DestroyWindow(handle);
			DWORD error = GetLastError();
		}
	break;
	case emBrowser_app_msg_page_busy_changed:
		if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusLoading)
		{
            /* Notification debug */
	        char* message = new char[45];
            sprintf(message, "File download started \n\n");
//            MessageBox( NULL, message, "Download", MB_OK | MB_ICONERROR );

#ifdef _DEBUG
            if (dumpf) fprintf(dumpf,message);
#endif
        }
		else if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusLoaded)
		{
			EmDocumentRef document; 
			EmBrowserStatus status = gInitParamBlock.browserMethods->getRootDoc(inst, &document);
			if (status == emBrowserNoErr)
			{
				/* Example use of Javascript API.  See callbacks below. */

				/* Get the object "window.location". */
				gInitParamBlock.browserMethods->jsEvaluateCode(document, L"window.location", JSEvaluateCodeCallback, (void *) document);


				/* Check if this is our test page */
				if(inst == gTestWidget)
				{
    				gInitParamBlock.browserMethods->jsEvaluateCode(document, L"myMethod();", NULL, (void *) document);
    				gInitParamBlock.browserMethods->jsEvaluateCode(document, L"myObject", JSEvaluateCodeCallback2, (void *) document);
					gTestWidget = NULL;
				}

				// Create javascript function
				gInitParamBlock.browserMethods->jsEvaluateCode(document, L"dings", JSEvaluateCodeCallback3, (void *) document);

				/* Notification debug */
#ifdef _DEBUG
				if(dumpf) fprintf(dumpf, "File download ended \n\n");
#endif
			}
        }
		else if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusFailed)
		{
            /* Notification debug */
#ifdef _DEBUG
            if(dumpf) fprintf(dumpf, "Loading failed \n\n");
#endif
        }
		else if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusCreated)
		{
			EmDocumentRef document;
			gInitParamBlock.browserMethods->getRootDoc(inst, &document);
			if (document)
			{
				EmBrowserChar url_str[128];
				gInitParamBlock.browserMethods->getURL(document,128,url_str,NULL);
			}
		}
	break;
	case emBrowser_app_msg_phase_changed:
		if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusUploading)
		{
#ifdef _DEBUG
            if(dumpf) fprintf(dumpf, "File upload started \n\n");
#endif
        }
        else if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusLoading)
		{
#ifdef _DEBUG
            if(dumpf) fprintf(dumpf, "File upload ended \n\n");
#endif
        }
    break;
	case emBrowser_app_msg_progress_changed:
		if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusUploading)
        {
            long rem, tot;
            gInitParamBlock.browserMethods->uploadProgress(inst,&rem,&tot);
#ifdef _DEBUG
            if(dumpf) fprintf(dumpf, "File uploading. File size : %d bytes. Uploaded %d bytes. \n\n", tot,rem);
#endif
        }
        else if(gInitParamBlock.browserMethods->pageBusy(inst) == emLoadStatusLoading)
        {
            long rem, tot;
            gInitParamBlock.browserMethods->downloadProgress(inst,&rem,&tot);
#ifdef _DEBUG
            if(dumpf) fprintf(dumpf, "File downloading. File size : %d bytes. Downloaded %d bytes. \n\n", tot,rem);
#endif
        }
    break;
    default:
		;
	}

}

//
//  FUNCTION: BeforeNavigation(EmBrowserRef inst, const EmBrowserString destURL, EmBrowserString newURL)
//
//  PURPOSE:  Callback from EmBrowser object before loading the destination URL
//
//	inst			- pointer to browser instance
//	destURL			- original destination URL
//	newURL			- the new URL overridden by the host application. Returned as a null-terminated string
//
//  Note that return value = zero means no loading, return value = non-zero means loading of newURL (if any) or destURL

long BeforeNavigation(IN EmBrowserRef inst, IN const EmBrowserString destURL, OUT EmBrowserString newURL)
{
	return 1; // just go ahead. remove during debugging
    int length = strlen("http://www.test.com/");

	if (newURL)
	{
		int dochange = 0; // change while debugging/testing

		if (dochange && destURL && (wcsncmp((EmBrowserString)destURL, L"http://www.opera.com/", 21) == 0))
		{
			memcpy(newURL, L"http://www.test.com/", (length + 1) * 2);
		}
		else
			memcpy(newURL, L"", 2);

		int doload = 1; // change while debugging/testing
		if (doload)
			return 1;
		else
			return 0;
	}
	return 0;
}

//
//  FUNCTION: AfterNavigation(EmBrowserRef inst, EmDocumentRef doc)
//
//  PURPOSE:  Callback from EmBrowser object after navigation
//
//	inst	- pointer to browser instance
//	doc		- document being left (unloaded)

long AfterNavigation(EmBrowserRef inst, EmDocumentRef doc)
{
	unsigned short str[128];
	gInitParamBlock.browserMethods->getURL(doc,128,str,NULL);

	return 0;
}

//
//  FUNCTION: RedirNavigation(EmBrowserRef inst, const EmBrowserString destURL, const EmBrowserString redirectURL)
//
//  PURPOSE:  Callback from EmBrowser object with redirection information
//
//	inst		- pointer to browser instance
//	destURL		- destination URL
//	redirectURL	- redirection URL

void RedirNavigation(IN EmBrowserRef inst, IN const EmBrowserString destURL, IN const EmBrowserString redirectURL)
{
	char dest[_MAX_PATH] = {'\0'};
	char redir[_MAX_PATH] = {'\0'};

	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, destURL, -1, dest, _MAX_PATH, NULL, NULL);
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, redirectURL, -1, redir, _MAX_PATH, NULL, NULL);

#ifdef _DEBUG
    if(dumpf) fprintf(dumpf, "Navigation to destURL %s redirected to %s\n\n", dest, redir);
#endif //_DEBUG

	return;
}

//
//  FUNCTION: AllowLocalFileURL(EmBrowserRef inst, const EmBrowserString docURL, const EmBrowserString localFileURL)
//
//  PURPOSE:  Callback from EmBrowser object with loading local file information
//
//	inst		- pointer to browser instance
//	docURL		- document URL
//	localFileURL- local file URL to be loaded in document
//
//  Note that return value = zero means no loading, return value = non-zero means loading of localFileURL

long AllowLocalFileURL(IN EmBrowserRef inst, IN const EmBrowserString docURL, IN const EmBrowserString localFileURL)
{
	char doc[_MAX_PATH] = {'\0'};
	char local[_MAX_PATH] = {'\0'};

	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, docURL, -1, doc, _MAX_PATH, NULL, NULL);
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, localFileURL, -1, local, _MAX_PATH, NULL, NULL);

	int allow = 0; // change while debugging/testing
	if (allow)
		return 1;
	else
		return 0; // default value
}

//
//  FUNCTION: NewBrowser(OPT IN EmBrowserRef caller, IN EmBrowserOptions browserOptions, IN EmBrowserWindowOptions windowOptions, IN EmBrowserRect *bounds, IN const EmBrowserString destURL, OUT EmBrowserRef* result)
//
//  PURPOSE:  Callback from EmBrowser object requesting new browser window (target="_blank")
//
//  ATTENTION: Reusing gSecondBrowser every time for simplicity, a real implementation would create a new EmBrowserRef

long NewBrowser(OPT IN EmBrowserRef caller, IN EmBrowserOptions browserOptions, IN EmBrowserWindowOptions windowOptions, IN EmBrowserRect *bounds, IN const EmBrowserString destURL, OUT EmBrowserRef* result)
{
	long width = bounds->right - bounds->left;
	long height = bounds->bottom - bounds->top;

	EmBrowserRect newbounds;
	newbounds.bottom = bounds->bottom;
	newbounds.top = bounds->top;
	newbounds.left = bounds->left;
	newbounds.right = bounds->right;

	if (width == 0)
	{
		width = 200;
		newbounds.right = newbounds.left + width;
	}
	if (height == 0)
	{
		height = 200;
		newbounds.bottom = newbounds.top + height;
	}

	HWND newparent = CreateWindow(szPopupWindowClass, "New Widget Parent", WS_POPUPWINDOW | WS_THICKFRAME | WS_CAPTION | WS_CLIPCHILDREN, 
			                     CW_USEDEFAULT, 0, width, height, NULL, NULL, hInst, NULL);
	
	if (!newparent)
	{
		return 0;
	}

	if (!gSecondBrowser)
	{
		EmBrowserStatus ret = gInitParamBlock.createInstance(newparent, emBrowserOptionDisableAppkeys|emBrowserOptionDisableTooltips|emBrowserOptionNoContextMenu|emBrowserOptionDisableDownloadDialog/*|emBrowserOptionDisableAllDrags*//*emBrowserOptionUseDefaults*/, &gSecondBrowser);
		EmBrowserStatus retval = gInitParamBlock.browserMethods->setLocation(gSecondBrowser, &newbounds, (void*)newparent);
		gInitParamBlock.browserMethods->setVisible(gSecondBrowser, true);
	}

	if (gInitParamBlock.browserMethods->setLocation)
	{		
		EmBrowserStatus retval = gInitParamBlock.browserMethods->setLocation(gSecondBrowser, &newbounds, (void*)newparent);

		ShowWindow(newparent, SW_RESTORE);
		UpdateWindow(newparent);

		if (gInitParamBlock.browserMethods->openURL)
		{		
			gInitParamBlock.browserMethods->openURL(gSecondBrowser, (EmBrowserString)destURL, emBrowserURLOptionDefault);
		}

		*result = gSecondBrowser;
		return 1;
	}		

	return 0; // non-zero if success
}


//
//  FUNCTION: GetOperaDirectory(char* result, unsigned long maxresult)
//
//  PURPOSE:  Try to find the directory where Opera is installed, if fails NULL is returned
//			  Read value from : "HKEY_CURRENT_USER\Software\Opera Software\Last Directory3"
//            if this exists and opera.dll is present we're set.
//
//	result		- resulting directory buffer
//	maxresult	- max length of directory buffer

BOOL GetOperaDirectory(char* result, unsigned long maxresult)
{
	HKEY	hKey = NULL;
	DWORD	dwRes = ERROR_SUCCESS;
	
	dwRes = RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Opera Software", NULL, KEY_READ, &hKey);
	
	if (dwRes == ERROR_SUCCESS)
	{
		DWORD dwType;
		if (RegQueryValueEx( hKey, "Last Directory3", NULL, &dwType, (LPBYTE)result, &maxresult) == ERROR_SUCCESS)
		{
			if (dwType != REG_SZ && dwType != REG_EXPAND_SZ)
			{
				dwRes = E_FAIL;
			}
		}
		RegCloseKey( hKey);
	}

	if(dwRes == E_FAIL || strlen(result) == 0)
	{
		return FALSE;
	}

	return TRUE;
}

//
//  FUNCTION: ShowLastError()
//
//  PURPOSE:  Show a messagebox with error text based on last error
//

void ShowLastError()
{
	LPVOID lpMsgBuf;
	DWORD messSize = FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	char* message = new char[messSize+45];

	sprintf(message, "Failed to initialize EmBrowser because: \n\n%s", (LPCTSTR)lpMsgBuf);

	MessageBox( NULL, message, "Error", MB_OK | MB_ICONERROR );
	LocalFree( lpMsgBuf );
	delete [] message;
}

//
//  FUNCTION: ReparentBrowserWidget()
//
//  PURPOSE:  Example on reparenting, moving widget over to a new window
//
//	newparent - new parent window

EmBrowserStatus ReparentBrowserWidget(HWND newparent)
{
	
	if(newparent == NULL)
	{
		newparent = CreateWindow(szPopupWindowClass, "New Widget Parent", WS_POPUPWINDOW | WS_THICKFRAME | WS_CAPTION | WS_CLIPCHILDREN, 
			                     CW_USEDEFAULT, 0, 200, 200, NULL, NULL, hInst, NULL);
	}
	
	if (!newparent)
	{
		return emBrowserGenericErr;
	}

	EmBrowserRect bounds;

	RECT containerRect;

	GetClientRect(newparent, &containerRect);
		
	bounds.left = containerRect.left;
	bounds.top = containerRect.top;
	bounds.right = containerRect.right;
	bounds.bottom = containerRect.bottom;

	if (gInitParamBlock.browserMethods->setLocation)
	{		
		EmBrowserStatus retval = gInitParamBlock.browserMethods->setLocation(gEmBrowser, &bounds, (void*)newparent);
		ShowWindow(newparent, SW_RESTORE);
		UpdateWindow(newparent);
		return retval;
	}		

	return emBrowserGenericErr;
}

struct ClientDataStruct
{
	EmDocumentRef document;
	EmJavascriptObjectRef location;
};

//
//  FUNCTION: JSEvaluateCodeCallback()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode.
//

void JSEvaluateCodeCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		ClientDataStruct *cds = new ClientDataStruct;
		cds->document = (EmDocumentRef) clientData;
		cds->location = value->value.object;
		gInitParamBlock.browserMethods->jsProtectObject(cds->location);

		/* Get the property "href" from the location object. */
		gInitParamBlock.browserMethods->jsGetProperty(cds->document, cds->location, L"href", JSGetPropertyCallback, cds);
	}
}

//
//  FUNCTION: JSGetPropertyCodeCallback()
//
//  PURPOSE:  Example callback for calls to JSGetProperty.
//

void JSGetPropertyCallback(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	ClientDataStruct *cds = (ClientDataStruct *) clientData;

	if(status == emJavascriptNoErr && value->type == emJavascriptString)
	{
		/* If the property "href" begins with "http://www.microsoft.com/" ... */
		if (wcslen(value->value.string) >= wcslen(L"http://www.microsoft.com/") &&
		    wcsncmp(L"http://www.microsoft.com/", value->value.string, wcslen(L"http://www.microsoft.com/")) == 0)
		{
			EmJavascriptValue new_value;
			new_value.type = emJavascriptString;
			new_value.value.string = L"http://www.opera.com/";

			/* ... assign the new value "http://www.opera.com/" to it instead. */
			gInitParamBlock.browserMethods->jsSetProperty(cds->document, cds->location, L"href", &new_value, NULL, NULL);
		}
	}
	
	gInitParamBlock.browserMethods->jsUnprotectObject(cds->location);
	delete cds;
}

//
//  FUNCTION: JSEvaluateCodeCallback2()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

void JSEvaluateCodeCallback2(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		/* Call 'method' on the returned object */
      
		gInitParamBlock.browserMethods->jsCallMethod((EmDocumentRef) clientData, value->value.object, L"method", 0, NULL, NULL, NULL);
	}
}


//
//  FUNCTION: JSEvaluateCodeCallback3()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

void JSEvaluateCodeCallback3(void *clientData, EmJavascriptStatus status, EmJavascriptValue *value)
{
	if(status == emJavascriptNoErr && value->type == emJavascriptObject)
	{
		if (gInitParamBlock.browserMethods->jsCreateMethod && gInitParamBlock.browserMethods->jsSetProperty)
		{
            EmJavascriptObjectRef method;
            EmJavascriptValue object;
            gInitParamBlock.browserMethods->jsCreateMethod((EmDocumentRef) clientData, &method, JSMethodCallBack, NULL );
            // Give the returned window object a property called 'eksos' that has the value of our function
            object.type = emJavascriptObject;
            object.value.object = method;
       		gInitParamBlock.browserMethods->jsSetProperty((EmDocumentRef) clientData, value->value.object, L"method", (EmJavascriptValue *)&object, NULL, NULL);
            object.type = emJavascriptString;
            object.value.string = L"abrakadabra";
       		gInitParamBlock.browserMethods->jsSetProperty((EmDocumentRef) clientData, value->value.object, L"str", &object, NULL, NULL);
        }
	}
}



//
//  FUNCTION: JSMethodCallBack()
//
//  PURPOSE:  Example callback for calls to JSEvaluateCode. Call methods on a object
//

EmJavascriptStatus JSMethodCallBack(void *inData, EmDocumentRef inDoc, EmJavascriptObjectRef inObject,
                      unsigned argumentsCount, EmJavascriptValue *argumentsArray,
                      EmJavascriptValue *returnValue)
{
    unsigned i;

    for(i=0; i<argumentsCount; i++)
    {
        EmJavascriptValue a = argumentsArray[i];

        if(a.type == emJavascriptString)
            MessageBoxW(gMainWin,a.value.string,L"Message from page",MB_OK);

    }
	return emBrowserNoErr;
}


//
//  FUNCTION: thumbnailCallback()
//
//  PURPOSE:  Example callback for calls to thumbnailRequest.
//

void thumbnailCallback (IN void *imageData, IN EmThumbnailStatus status, IN EmThumbnailRef ref)
{
    static int i = 0;
    int w = 200;
    int h = 160;
    int x = (i%6)*(w+10)+10;
    int y = (i/6)*(h+10)+10; i++;

    HDC dest_hdc = GetWindowDC(NULL); //gMainWin);
    HDC src_hdc  = CreateCompatibleDC(NULL);

    HBITMAP src_bitmap = (HBITMAP)imageData;

    void * old_one = SelectObject(src_hdc,src_bitmap);

    BOOL success = BitBlt(dest_hdc,x,y,w,h,src_hdc,0,0,SRCCOPY);

    DeleteDC(src_hdc);

	gInitParamBlock.browserMethods->thumbnailKill(ref);

}

//
//  FUNCTION: SetLanguageFile()
//
//  PURPOSE:  Example and test for EmBrowserSetUILanguage
//

EmBrowserStatus SetLanguageFile()
{
	char* languagefileA = new char[512];

	languagefileA[0] = 0;

	OPENFILENAME ofn;
	memset( &ofn, 0, sizeof(ofn));
	ofn.lStructSize			= sizeof(ofn);
	ofn.hwndOwner			= gMainWin;
	ofn.hInstance			= NULL;
	ofn.lpstrFilter			= "*.lng\0*.*\0\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= languagefileA; 
    ofn.nMaxFile			= 511; 
    ofn.lpstrFileTitle		= NULL;
    ofn.nMaxFileTitle		= 0;
    ofn.lpstrInitialDir		= NULL;
    ofn.lpstrTitle			= NULL;
    
	ofn.Flags				= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
	ofn.nFileOffset			= 0; 
    ofn.nFileExtension		= 0; 
    ofn.lpstrDefExt			= 0; 
    ofn.lCustData			= 0; 
    ofn.lpfnHook			= 0; 
    ofn.lpTemplateName		= 0; 
	int m_rows = 1;

	EmBrowserStatus status = emBrowserGenericErr;

	if(	GetOpenFileName( &ofn) )
	{
		//convert from multi to wide
		EmBrowserString languagefile = new EmBrowserChar[512];
	
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, languagefileA, -1, languagefile, 511);

		status = gInitParamBlock.browserMethods->setUILanguage(languagefile);

		delete [] languagefile;
	}

	delete [] languagefileA;

	return status;
}

//
//  FUNCTION: SetVisLinks()
//
//  PURPOSE:  Enable or disable the visibility of visited links. 
//			  Demonstrates "Start/Stop the history recording system from adding new URLs to its list"
//

EmBrowserStatus SetVisLinks(BOOL visible)
{
	if (gInitParamBlock.browserMethods->setVisLinks)
	{	
		gInitParamBlock.browserMethods->setVisLinks(visible);
	}		
	return emBrowserNoErr;
}

//
//  FUNCTION: GetWindowHandle()
//
//  PURPOSE:  Return the window handle to a browser instance. 
//			  Demonstrates calling convention
//

HWND GetWindowHandle(EmBrowserRef inst)
{
	if (gInitParamBlock.browserMethods->getWindowhandle)
	{
		return *((HWND*)(gInitParamBlock.browserMethods->getWindowhandle(gEmBrowser)));
	}
	return NULL;
}
 
//
//  FUNCTION: TestJavasript()
//
//  PURPOSE:  Open a page containing a javascript function that will be called when the page is loaded.
//            Demonstrates the use of EvaluateCode
//

EmBrowserStatus TestJavascript()
{
	if (gInitParamBlock.browserMethods->openURL)
	{	
		char modulefilename[MAX_PATH];
		unsigned short modulefilenameW[MAX_PATH];
		unsigned short filename[MAX_PATH];

		GetModuleFileName(gOperaLib, modulefilename, MAX_PATH);

		char* pos = strrchr(modulefilename, '\\');

		if(pos)
		{
			*pos = 0;
		}

        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, modulefilename, -1, modulefilenameW, MAX_PATH);

		wsprintfW(filename, L"file://localhost/%s/script_test.html", modulefilenameW);
		
        // Correct url argument when using this
		gInitParamBlock.browserMethods->openURL(gEmBrowser, (EmBrowserString)filename, emBrowserURLOptionDefault);
        gTestWidget = gEmBrowser;
    }
	return emBrowserNoErr;
}

//
//  FUNCTION: TestJavasript2()
//
//  PURPOSE:  Scan through a page, insert new element in ordered lists. Replace all images.
//            Demonstrates the use of EvaluateCode
//

EmBrowserStatus TestJavascript2()
{
	EmBrowserString code =
  	L"{ var taglist = document.getElementsByTagName('ol'); for(i=0;i<taglist.length;i++) { taglist[i].type = 'I'; taglist[i].insertBefore( document.createElement('li'), taglist[i].firstChild).appendChild(document.createTextNode('Opera, always on top')); } }";
	EmBrowserString code2 =
  	L"{ var taglist = document.getElementsByTagName('img'); for(i=0;i<taglist.length;i++) { taglist[i].src = 'http://www.opera.com/graphics/logos/opera.png'; } }";

    EmDocumentRef document;
    gInitParamBlock.browserMethods->getRootDoc(gEmBrowser, &document);

	gInitParamBlock.browserMethods->jsEvaluateCode(document, code, NULL, NULL);
	gInitParamBlock.browserMethods->jsEvaluateCode(document, code2, NULL, NULL);


	return emBrowserNoErr;
}




//
//  FUNCTION: TestSuite
//
//  PURPOSE:  Run testsuite.
//

EmBrowserStatus TestSuite()
{
	EmBrowserStatus ret = emBrowserNoErr;
    if (gInitParamBlock.browserMethods->openURL)
    {
        char modulefilename[MAX_PATH];
        char openfile[MAX_PATH];
        char openpath[MAX_PATH];
        unsigned short openpathW[MAX_PATH];
        GetModuleFileName(gOperaLib, modulefilename, MAX_PATH);
        char * pos = strrchr(modulefilename, '\\');
        if(pos)
        {
            *pos = 0;
        }
        sprintf(openfile,"%s/emtestlog.html",modulefilename);

		if (ret = InitTestSuite(openfile, gInitParamBlock.browserMethods, gEmBrowser) != emBrowserNoErr)
			return ret;
        if (ret = TestSuitePart(1) != emBrowserNoErr)
			return ret;
	    if (ret = TestSuitePart(2) != emBrowserNoErr)
			return ret;
		if (ret = ExitTestSuite() != emBrowserNoErr)
			return ret;
			
		sprintf(openpath,"file://localhost/%s",openfile);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, openpath, -1, openpathW, 511);
        ret = gInitParamBlock.browserMethods->openURL(gEmBrowser, (EmBrowserString)openpathW, emBrowserURLOptionDefault);
    }
    
    return ret;
}

//
//  FUNCTION: ToggleSSR
//
//  PURPOSE:  Example of toggling of small screen rendering
//

EmBrowserStatus ToggleSSR()
{
	EmBrowserStatus ret = emBrowserNoErr;
    if (gInitParamBlock.browserMethods->toggleSSRmode)
	{
		ret = gInitParamBlock.browserMethods->toggleSSRmode(gEmBrowser);
	}
	return ret;
}

//
//  FUNCTION: ToggleFitToWidth
//
//  PURPOSE:  Example of toggling of fit to width rendering
//

EmBrowserStatus ToggleFitToWidth()
{
	EmBrowserStatus ret = emBrowserNoErr;
    if (gInitParamBlock.browserMethods->toggleFitToWidth)
	{
		ret = gInitParamBlock.browserMethods->toggleFitToWidth(gEmBrowser);
	}
	return ret;
}