
#if !defined(AFX_EMBROWSERCONTAINER_H__0CF38CD7_ECDD_4407_81A9_099362DEC3B8__INCLUDED_)
#define AFX_EMBROWSERCONTAINER_H__0CF38CD7_ECDD_4407_81A9_099362DEC3B8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

# define PATHSEPCHAR	'\\'

// Windows Header Files:
#include <windows.h>
#include <commdlg.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "adjunct/embrowser/embrowser.h"

#include "resource.h"

void				ShowLastError();
BOOL				GetOperaDirectory(char* result, unsigned long maxresult);
void				BrowserNotification(EmBrowserRef inst, EmBrowserAppMessage msg, long value);
long				AfterNavigation(EmBrowserRef inst, EmDocumentRef doc);
long				BeforeNavigation(IN EmBrowserRef inst, IN const EmBrowserString destURL, OUT EmBrowserString newURL);
void				RedirNavigation(IN EmBrowserRef inst, IN const EmBrowserString destURL, IN const EmBrowserString redirectURL);
long				AllowLocalFileURL(IN EmBrowserRef inst, IN const EmBrowserString docURL, IN const EmBrowserString localFileURL);
long				NewBrowser(OPT IN EmBrowserRef caller, IN EmBrowserOptions browserOptions, IN EmBrowserWindowOptions windowOptions, IN EmBrowserRect *bounds, IN const EmBrowserString destURL, OUT EmBrowserRef* result);
EmBrowserStatus		InitBrowser(HWND hwnd);
EmBrowserStatus		CloseBrowser();
EmBrowserStatus		ShowHistory();
BOOL				InitInstance(HINSTANCE, int);
ATOM				RegisterMainClass(HINSTANCE hInstance);
ATOM				RegisterPopupClass(HINSTANCE hInstance);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	Goto(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	SetCookie(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	PopupWndProc(HWND, UINT, WPARAM, LPARAM);

struct EmVendorBrowserInitParameterBlock  
{
	long 				majorVersion;
	long 				minorVersion;
	char				registrationCode[64];
};

#endif // !defined(AFX_EMBROWSERCONTAINER_H__0CF38CD7_ECDD_4407_81A9_099362DEC3B8__INCLUDED_)
