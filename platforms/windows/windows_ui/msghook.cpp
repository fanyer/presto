/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** CREATED			DG-271198
** DESCRIPTION		Win message hooks
*/

#include "core/pch.h"

#include "platforms/windows/windows_ui/msghook.h"

#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"

#include "platforms/windows/win_handy.h"
#include "platforms/windows/CustomWindowMessages.h"
#ifndef NS4P_COMPONENT_PLUGINS
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#endif // !NS4P_COMPONENT_PLUGINS
#include "platforms/windows/pi/WindowsOpPluginWindow.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/windows_ui/menubar.h"

#include <xmmintrin.h>

BOOL g_is_showing_menu  = FALSE;

extern BOOL IsPluginWnd(HWND hWnd, BOOL fTestParents);
extern BOOL HandleMouseEvents(HWND hWnd, UINT message, const OpPoint& point);

static HWND _hWndBelowCursor = NULL;

static LRESULT CALLBACK _HookProc_Mouse			( int nCode, WPARAM wParam, LPARAM lParam );
static LRESULT CALLBACK _HookProc_WndProc		( int nCode, WPARAM wParam, LPARAM lParam );
static LRESULT CALLBACK _HookProc_CBT			( int nCode, WPARAM wParam, LPARAM lParam );
static LRESULT CALLBACK _HookProc_GetMessage	( int nCode, WPARAM wParam, LPARAM lParam );

extern UINT	idMsgLostCursor;

LRESULT CALLBACK _HookProc_LowLevelKeyboard		( int nCode, WPARAM wParam, LPARAM lParam );
static HHOOK _hHookLLKeyboard = 0;

static HHOOK _hHookMouse = 0;
static HHOOK _hHookMessageProc = 0;
static HHOOK _hHookWndProc = 0;
static HHOOK _hHookCBT = 0;
static HHOOK _hHookGetMessage = 0;

struct HOOKMAP
{
	HHOOK		*pHook;
	int			kindOfHook;
	HOOKPROC	pHookProc;
}
static _hookMap[] = 
{
#ifdef _MSGHOOK_MOUSE
	{ &_hHookMouse,			WH_MOUSE,		_HookProc_Mouse			},
#endif

	{ &_hHookLLKeyboard,   WH_KEYBOARD_LL, _HookProc_LowLevelKeyboard},

#ifdef _MSGHOOK_CALLWNDPROC
	{ &_hHookWndProc,		WH_CALLWNDPROC,	_HookProc_WndProc		},
#endif

#ifdef _MSGHOOK_CBT
	{ &_hHookCBT,			WH_CBT,			_HookProc_CBT			},
#endif

#ifdef _MSGHOOK_GETMESSAGE
	{ &_hHookGetMessage,	WH_GETMESSAGE,	_HookProc_GetMessage	},
#endif
};

#ifndef NS4P_COMPONENT_PLUGINS
DelayedFlashMessageHandler*	g_delayed_flash_message_handler = NULL;
#endif // !NS4P_COMPONENT_PLUGINS

#if _MSC_VER < VS2012
extern "C" BOOL __sse2_available;
#endif

//	___________________________________________________________________________
//	InstallMouseHook
//	Attach the mouse hook to the calling thread (WIN32) or the current task-
//	handle on WIN16
//	___________________________________________________________________________
//  
BOOL InstallMSWinMsgHooks()
{
#ifndef NS4P_COMPONENT_PLUGINS
	if (!(g_delayed_flash_message_handler = OP_NEW(DelayedFlashMessageHandler, ())))
		return FALSE;
#endif // !NS4P_COMPONENT_PLUGINS

	for (int i=0; i<ARRAY_SIZE(_hookMap); i++)
	{
		HHOOK &hook = *_hookMap[i].pHook;

		//	Dont hook twice
		if( !hook)
		{
			//
			//	Install hook        
			//
			if(_hookMap[i].kindOfHook == WH_KEYBOARD_LL && KioskManager::GetInstance()->GetEnabled()) 
			{
				hook = SetWindowsHookEx( _hookMap[i].kindOfHook, _hookMap[i].pHookProc, hInst, 0);
			}
			else
			{
				hook = SetWindowsHookEx( _hookMap[i].kindOfHook, _hookMap[i].pHookProc, 0, GetCurrentThreadId());
			}
		}
	}
	return TRUE;
}



//	___________________________________________________________________________
//	RemoveMSWinMsgHooks()
//	___________________________________________________________________________
//  
void RemoveMSWinMsgHooks()
{
	for (int i=0; i<ARRAY_SIZE(_hookMap); i++)
	{
		HHOOK &hook = *_hookMap[i].pHook;
		if( hook)
			UnhookWindowsHookEx(hook);
		hook = NULL;
	}
}

//****************************************************************************************
 
LRESULT CALLBACK _HookProc_LowLevelKeyboard( int nCode, WPARAM wParam, LPARAM lParam )
{
	BOOL fEatKeystroke = FALSE;
	static bool s_l_ctrl_pressed = FALSE;
	static bool s_r_ctrl_pressed = FALSE;
	
	if (nCode == HC_ACTION)
	{
		switch (wParam)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			{
				PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT) lParam;
				
				// julienp:	Without the additionnal checks s_l_ctrl_pressed and s_r_ctrl_pressed,
				//          CTRL+ESC still slip through if keys are pressed repeatedly and quickly,
				//			_when Opera has focus_ (see bug 182226).
				//			It seems like when Opera has focus, something like this happens:

				//          +-------------------------- time ----------------------------->
				//			|  Ctrl pressed -> Hook for Ctrl        -> Ctrl gets /registered/ for GetKeyState (too late)
				//    Events|  delta -> Esc Pressed -> hook for Esc (doesn't know about Ctrl)
				//          V

				//			If Opera is not the window with focus, it looks like that order
				//          is changed (Ctrl gets registered before switching to the application
				//          , maybe)
				//
				//			It might be that the check of GetKeyState isn't needed anymore but keeping
				//          it to be on the safe side.

				if (p->vkCode == VK_LCONTROL && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
					s_l_ctrl_pressed = TRUE;
				if (p->vkCode == VK_LCONTROL && (wParam == WM_KEYUP || wParam == WM_SYSKEYUP))
					s_l_ctrl_pressed = FALSE;
				if (p->vkCode == VK_RCONTROL && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
					s_r_ctrl_pressed = TRUE;
				if (p->vkCode == VK_RCONTROL && (wParam == WM_KEYUP || wParam == WM_SYSKEYUP))
					s_r_ctrl_pressed = FALSE;

				BOOL ctrl_pressed = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);

				fEatKeystroke =
					(

					((p->vkCode == VK_SPACE) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||                       // ALT+SPACE
					((p->vkCode == VK_TAB) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||                         // ALT+TAB
                    ((p->vkCode == VK_ESCAPE) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||                      // ALT+ESC			
                    ((p->vkCode == VK_ESCAPE) && (ctrl_pressed || s_l_ctrl_pressed || s_r_ctrl_pressed)) ||              // CTRL+ESC
                    ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) ||                                   // WINDOWSKEYS
					((p->vkCode == VK_SUBTRACT) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||                    // ALT+'-'
					((p->vkCode == VK_OEM_MINUS ) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||                    // ALT+'-'
					(KioskManager::GetInstance()->GetKioskWindows() && (p->vkCode == VK_F4) && (ctrl_pressed || s_l_ctrl_pressed || s_r_ctrl_pressed)) ||
					(p->vkCode == VK_APPS) ||                                                               // APPLICATIONS KEY
					(p->vkCode == VK_SNAPSHOT) ||                                                           // PRINTSCREEN
					((p->vkCode == VK_LMENU) && ((p->flags & LLKHF_ALTDOWN) != 0))	// ALT key altogether
					);
			}
			break;
		default:
			;
		}
	}

	return(fEatKeystroke ? 1 : CallNextHookEx(_hHookLLKeyboard, nCode, wParam, lParam));
}


//	___________________________________________________________________________
//	_HookProc_Mouse
//	___________________________________________________________________________
//  
static LRESULT CALLBACK _HookProc_Mouse
(  
	int		nCode,      // hook code
	WPARAM	wParam,		// message identifier  
	LPARAM	lParam		// Pointer to a MOUSEHOOKSTRUCT structure. 
)
{
	MOUSEHOOKSTRUCT * pInfo = (MOUSEHOOKSTRUCT*)lParam;
	HWND hWnd = pInfo->hwnd;
	
	//	If nCode is less than zero, the hook procedure must pass the message to 
	//	the CallNextHookEx function without further processing and should return 
	//	the value returned by CallNextHookEx. 

	if (nCode < 0)
		goto LABEL_Done;

	if (nCode == HC_ACTION
		&& (wParam == WM_RBUTTONDOWN
		|| wParam == WM_RBUTTONDBLCLK
		|| wParam == WM_RBUTTONUP))
	{
		// don't allow this in kiosk mode
		if (KioskManager::GetInstance()->GetNoContextMenu())
		{
			return TRUE;
		}
	}
	if (nCode == HC_ACTION
		&& !g_is_showing_menu
		&& ((wParam >= WM_NCMOUSEMOVE && wParam <= WM_NCMBUTTONDBLCLK) || (wParam >= WM_MOUSEMOVE && wParam <= WM_MBUTTONDBLCLK))
		&& !WindowsOpWindow::GetWindowFromHWND(hWnd)
		&& HandleMouseEvents(hWnd, wParam, OpPoint(pInfo->pt.x, pInfo->pt.y)))
	{
		return TRUE;
	}

	switch( wParam)
	{
		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		{
			HWND hWndPrev = _hWndBelowCursor;
					
			if( nCode != HC_NOREMOVE)
			{
				// Tooltip windows should not recieve any mouse events and
				// should not "disturb" hovered window or capture in any way.
				WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
				if (window && window->GetStyle() == OpWindow::STYLE_TOOLTIP)
					break;

				if( hWndPrev != hWnd)
				{
					if( ISWINDOW(hWndPrev))
						SendMessage( hWndPrev, idMsgLostCursor, 0, 0);
					
					_hWndBelowCursor = hWnd;
				}
			}
			break;
		}    

	}

LABEL_Done:
	return CallNextHookEx( _hHookMouse, nCode, wParam, lParam);
}


//	___________________________________________________________________________
//	CheckForValidOperaWindowBelowCursor
//
//	Called from ucTimer.
//	Used reset _hWndBelowCursor when the cursor moves outside the Opera workspace.
//	___________________________________________________________________________
//  
void CheckForValidOperaWindowBelowCursor()
{
	if (!_hWndBelowCursor)
		return;

	if (_hWndBelowCursor && !ISWINDOW(_hWndBelowCursor))
	{
		_hWndBelowCursor = NULL;
		return;
	}

	if (IsOperaWindow(GetCapture()))
	{
		return;
	}

	BOOL fOperaHasCursor = FALSE;
	POINT pt;

	if (GetCursorPos( &pt))
	{
		HWND hWndBelowCursor = WindowFromPoint( pt);
		if (hWndBelowCursor)
		{
			fOperaHasCursor =IsOperaWindow( hWndBelowCursor);
		//	if (!fOperaHasCursor) DEBUG_Beep();
		}	
	}

	if (!fOperaHasCursor)
	{
		//	Notify window losing cursor
		PostMessage( _hWndBelowCursor, idMsgLostCursor, 0, 0);
		_hWndBelowCursor = NULL;
	};
}

extern OUIMenuManager*				g_windows_menu_manager;

//	___________________________________________________________________________
//	_HookProc_WndProc
//	MH_CALLWNDPROC
//
//	Monitors messages BEFORE the system sends them to the destination window 
//	procedure.
//	___________________________________________________________________________
//  
#ifdef _MSGHOOK_CALLWNDPROC

static LRESULT CALLBACK _HookProc_WndProc( int nCode, WPARAM key, LPARAM lParam )
{
	CWPSTRUCT *pInfo = (CWPSTRUCT*)lParam;

	if (nCode == HC_ACTION && pInfo)
	{
		static int g_DEP_enabled = 0;

		if (!g_DEP_enabled)
		{
			g_DEP_enabled = -1;
			if (IsProcessorFeaturePresent(PF_NX_ENABLED))
				g_DEP_enabled = 1;
		}
		if (g_DEP_enabled > 0)
		{
			UINT wndproc = GetWindowLongW(pInfo->hwnd, GWLP_WNDPROC);

			if ((wndproc >> 16) == 0xFFFF)	// not a real address, but a WindowProc unicode conversion handle
				wndproc = GetWindowLongA(pInfo->hwnd, GWLP_WNDPROC);

			if (wndproc)
			{
				static UINT g_known_WindowProcs[64];

				for (UINT i=0; i<ARRAY_SIZE(g_known_WindowProcs) && g_known_WindowProcs[i] != wndproc; i++)
				{
					if (!g_known_WindowProcs[i])
					{
						UINT DLL_location = 0;

						if (!IsBadReadPtr((LPVOID)wndproc, 0xA5) &&
							*(UINT *)(wndproc + 0xA1) == 0xD6FF5350 &&
							*(WORD *)(wndproc + 0x10) == 0x358B)
						{
							// special hack for SKCHUI.DLL version 1.0.1038.0
							DWORD *thread_list = **(DWORD ***)(wndproc + 0x12);
							DWORD thread_id = GetCurrentThreadId();
							while (thread_list)
							{
								if (thread_list[1] == thread_id)
								{
									DLL_location = wndproc;
									wndproc = thread_list[0] + 8;
									break;
								}
								else
									thread_list = (DWORD *)thread_list[2];
							}
						}

						g_known_WindowProcs[i] = wndproc;

						MEMORY_BASIC_INFORMATION mem_inf;
						VirtualQuery((LPVOID)wndproc, &mem_inf, sizeof(mem_inf));
						if (!(mem_inf.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY	)))
						{
							DWORD old_protect;
							if (VirtualProtect((LPVOID)wndproc, 32, PAGE_EXECUTE_READWRITE, &old_protect) && !DLL_location)
							{
								if (*(unsigned char*)wndproc == 0xB9)
									wndproc += 5;
								else if (*(UINT *)wndproc == 0x042444C7)
									wndproc += 8;
								if (*(unsigned char*)wndproc == 0xE9)
									DLL_location = *(UINT *)(wndproc+1) + wndproc+5;
							}
							PostMessage(g_main_hwnd, WM_BAD_DLL, 0, DLL_location);
						}
						break;
					}
				}
			}
		}

		switch (pInfo->message)
		{
			case WM_INITMENUPOPUP:
				if(g_windows_menu_manager)
				{
					g_windows_menu_manager->OnInitPopupMenu(pInfo->hwnd, (HMENU)pInfo->wParam, LOWORD(pInfo->lParam));
				}
				break; 

			case WM_ENTERMENULOOP:
			{
				g_is_showing_menu = TRUE;
				break;
			}

			case WM_MENUSELECT:
			case WM_EXITMENULOOP:
			{
				g_input_manager->ResetInput();
				ReleaseCapture();
				g_is_showing_menu = FALSE;
				break;
			}

			case WM_ACTIVATE:
			{
				if (g_windows_menu_manager)
					g_windows_menu_manager->CancelAltKey();
				break;
			}

			case WM_KILLFOCUS:
			{
				// If the focus goes to a plugin, set the input context to be
				// the view closest to the plugin.
				if (pInfo->wParam && IsPluginWnd(reinterpret_cast<HWND>(pInfo->wParam), TRUE))
				{
					HWND hwnd = reinterpret_cast<HWND>(pInfo->wParam);
					WindowsOpPluginWindow* pw = WindowsOpPluginWindow::GetPluginWindowFromHWND(hwnd);
					WindowsOpView* view = pw ? static_cast<WindowsOpView*>(pw->GetParentView()) : NULL;

					while (view)
					{
						if (view->GetParentInputContext() && view->GetParentInputContext()->GetParentInputContext())
						{
							// This one is really dirty, only let plugin get keyboard input
							// and focus when clicking on it.
							static short primary_button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
							if ((GetAsyncKeyState(primary_button) & 0x8000))
								PostThreadMessage(GetCurrentThreadId(), WM_DELAYED_FOCUS_CHANGE, reinterpret_cast<WPARAM>(view->GetParentInputContext()->GetParentInputContext()), NULL);
								
							else
							{
								if (view->GetParentView() && view->GetNativeWindow())
									SetFocus(view->GetNativeWindow()->m_hwnd);
							}
							break;
						}

						view = static_cast<WindowsOpView*>(view->GetParentView());
					}
				}
				break;
			}
		}
	}

	return CallNextHookEx( _hHookWndProc, nCode, key, lParam);
}
#endif //	_MSGHOOK_CALLWNDPROC


//	_HookProc_GetMessage
//	MH_GETMESSAGE
//	___________________________________________________________________________

#ifdef _MSGHOOK_GETMESSAGE

#ifndef NS4P_COMPONENT_PLUGINS
DelayedFlashMessageHandler::DelayedFlashMessageHandler()
{
	g_main_message_handler->SetCallBack(this, DELAYED_FLASH_MESSAGE, (MH_PARAM_1)this);
	m_last_time = 0;
}

DelayedFlashMessageHandler::~DelayedFlashMessageHandler()
{
	g_main_message_handler->UnsetCallBacks(this);
	MSG *wmsg;
	while ((wmsg = m_messages.Get(0)))
	{
		HandleCallback(MSG_NO_MESSAGE, 0, (MH_PARAM_2)wmsg);
	}
}

BOOL DelayedFlashMessageHandler::PostFlashMessage(MSG *msg)
{
	UINT32 last_time = m_last_time;
	m_last_time = timeGetTime();

	if (m_last_time - last_time < 5 || m_messages.GetCount() > 0 || g_in_synchronous_loop)
	{
		MSG *flash_msg = OP_NEW(MSG, (*msg));
		if (flash_msg)
		{
			m_messages.Add(flash_msg);
			g_main_message_handler->PostMessage(DELAYED_FLASH_MESSAGE, (MH_PARAM_1)this, (MH_PARAM_2)flash_msg, 1);
			return TRUE;
		}
	}
	return FALSE;
}

void DelayedFlashMessageHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (!g_in_synchronous_loop)
	{
		DispatchMessage((MSG *)par2);
		m_messages.Delete((MSG *)par2);
	}
	else
	{
		g_main_message_handler->PostMessage(DELAYED_FLASH_MESSAGE, par1, par2, 1);
	}
}
#endif // !NS4P_COMPONENT_PLUGINS

LRESULT CALLBACK _HookProc_GetMessage(int code, WPARAM wParam, LPARAM lParam)
{
#ifndef NS4P_COMPONENT_PLUGINS
	MSG* msg = (MSG*) lParam;

	if (code == HC_ACTION && msg && wParam == PM_REMOVE && g_windows_message_loop)
	{
		// message 0x401, wParam==1, is posted by Flash every time its "do the next thing" timer
		// runs out. Once dispatched to the Flash plugin window procedure, it will e.g. paint the
		// next frame. With several heavy Flash animations running, this can slow things down
		// to a crawl, so we delay that message if it comes more than once per millisecond

		if (msg->message == 0x401 && msg->wParam == 1
			&& !g_is_showing_menu) // added to fix bug 192167 (johan)
		{
			if (g_windows_message_loop->GetIsExiting() // Just drop completely when exiting. Possibly fixes DSK-372317.
				|| g_delayed_flash_message_handler->PostFlashMessage(msg))
				msg->wParam = 0xDEADBEEF;
		}

		g_windows_message_loop->PostMessageIfNeeded();
	}
#endif // !NS4P_COMPONENT_PLUGINS

	LRESULT res = CallNextHookEx(_hHookGetMessage, code, wParam, lParam);

	// Workaround for crashes caused by the buggy keyboard hooks in
	// AllChars v3.6.2 and MemoClip v1.35, buggy plugins, printer drivers etc.
	// Their Delphi library functions mess up the FPU control word,
	// sooner or later causing a crash in the Ecmascript engine.
	// The master bug for these is #109433.

	// This ASM code to reset the CW is a highly efficient,
	// but compiler-dependant replacement for
	// _control87(_CW_DEFAULT, ~0);
#ifndef _WIN64
// _asm is only supported on Win32
	_asm {
		push 27fh
		fldcw [esp]
		pop edx
	}
	if (__sse2_available)
		_mm_setcsr(0x1F80);
#endif // !_WIN64
	return res;
}

#endif //	_MSGHOOK_GETMESSAGE

//	___________________________________________________________________________
//	��������������������������������������������������������������������������?
//	_HookProc_CVT
//	MH_CBT
//
//	The system calls this function before activating, creating, destroying, 
//	minimizing, maximizing, moving, or sizing a window; before completing a 
//	system command; before removing a mouse or keyboard event from the system 
//	message queue; before setting the keyboard focus; or before synchronizing 
//	with the system message queue. A computer-based training (CBT) application 
//	uses this hook procedure to receive useful notifications from the system.
//	___________________________________________________________________________
//  
#ifdef _MSGHOOK_CBT
static LRESULT CALLBACK _HookProc_CBT( int nCode, WPARAM wParam, LPARAM lParam )
{

	if (nCode < 0)
		goto LABEL_Done;

	switch (nCode)
	{

		case HCBT_ACTIVATE:
			//	The system is about to activate a window. 
			break;

		case HCBT_CLICKSKIPPED:
			//	The system has removed a mouse message from the system message queue. 
			//	Upon receiving this hook code, a CBT application must install a WH_JOURNALPLAYBACK 
			//	hook procedure in response to the mouse message. 
			break;

		case HCBT_CREATEWND:
			//	A window is about to be created. The system calls the hook procedure before sending 
			//	the WM_CREATE orWM_NCCREATE message to the window. If the hook procedure returns a 
			//	nonzero value, the system destroys the window; theCreateWindow function returns NULL, 
			//	but theWM_DESTROY message is not sent to the window. If the hook procedure returns 
			//	zero, the window is created normally. At the time of the HCBT_CREATEWND notification, 
			//	the window has been created, but its final size and position may not have been 
			//	determined and its parent window may not have been established. It is possible to 
			//	send messages to the newly created window, although it has not yet received WM_NCCREATE 
			//	or WM_CREATE messages. It is also possible to change the position in the Z order of 
			//	the newly created window by modifying the hwndInsertAfter member of the CBT_CREATEWND 
			//	structure. 

			// Disable window shadow for OpenGL backend on XP
			if (GetWinType() <= WINXP &&
				g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
				g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::OpenGL)
			{
				uni_char className[32];
				if (GetClassName((HWND)wParam, className, 32) && uni_strcmp(className, UNI_L("SysShadow")) == 0)
					return 1;
			}
			break;

		case HCBT_DESTROYWND:
			//	A window is about to be destroyed. 
			break;

		case HCBT_KEYSKIPPED:
			//	The system has removed a keyboard message from the system message queue. Upon 
			//	receiving this hook code, a CBT application must install a WH_JOURNALPLAYBACK_hook 
			//	hook procedure in response to the keyboard message. 
			break;

		case HCBT_MINMAX:
			//	A window is about to be minimized or maximized. 
			break;

		case HCBT_MOVESIZE:
			//	A window is about to be moved or sized. 
			break;

		case HCBT_QS:
			//	The system has retrieved a WM_QUEUESYNC message from the system message queue. 
			break;

		case HCBT_SETFOCUS:
			//	A window is about to receive the keyboard focus. 
			break;

		case HCBT_SYSCOMMAND:
			//	A system command is about to be carried out. This allows a CBT application to 
			//	prevent task switching by means of hot keys. 
			break;
	}

LABEL_Done:
	return CallNextHookEx( _hHookCBT, nCode, wParam, lParam);
}
#endif // _MSGHOOK_CBT
