/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "platforms/windows/pi/WindowsOpTaskbar.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "platforms/windows/windows_ui/OpShObjIdl.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#ifdef WIDGET_RUNTIME_SUPPORT
# include "adjunct/widgetruntime/GadgetStartup.h"
#endif // WIDGET_RUNTIME_SUPPORT
#include "adjunct/quick_toolkit/widgets/OpNotifier.h"

#include "platforms/windows/WindowsDesktopGlobalApplication.h"
#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/utils/win_icon.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "adjunct/desktop_util/widget_utils/LogoThumbnailClipper.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include <Psapi.h>
#include <process.h>

extern HINSTANCE hInst;

extern void Win32DebugLogging(const uni_char *x, ...);

HRESULT ActivateTaskbarThumbnail(HWND hWnd);
HRESULT CreateTaskbarThumbnail(HWND hWnd);
HRESULT RemoveTaskbarThumbnail(HWND hWnd);

extern BOOL IsSystemWin7();
extern WinType GetWinType();

#ifndef NOTIFYICONDATAA_V3_SIZE
#define NOTIFYICONDATAA_V3_SIZE FIELD_OFFSET(NOTIFYICONDATAA, hBalloonIcon)
#define NOTIFYICONDATAW_V3_SIZE FIELD_OFFSET(NOTIFYICONDATAW, hBalloonIcon)
#endif // NOTIFYICONDATAA_V3_SIZE

#ifndef NIIF_LARGE_ICON
#define NIIF_LARGE_ICON	0x00000020
#endif // NIIF_LARGE_ICON

#define DELAY_CREATE_JUMPLIST	(3 * 1000)	// delay 3 seconds before re-creating the jumplist
#define DELAY_UPDATE_PROGRESS	(1 * 300)	// delay 300ms seconds before updating the progress

//julienp
//We are using the default OpNotifier, so I'm declaring this here to avoid creating a new file.
//This seemed to be the best fitting place. If you feel it should move, please feel free to
//propose a better place.
OP_STATUS DesktopNotifier::Create(DesktopNotifier** new_notifier)
{
	*new_notifier = new OpNotifier;
	return *new_notifier ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#ifdef M2_SUPPORT
bool DesktopNotifier::UsesCombinedMailAndFeedNotifications()
{
	return true;
}
#endif // M2_SUPPORT

WindowsOpTaskbar::WindowsOpTaskbar():
	m_trayicon_inited(FALSE),
	m_unread_chat(FALSE),
	m_unread_mail(FALSE),
	m_unite_attention(FALSE)
{
	g_pcui->RegisterListenerL(this);

#ifdef WEBSERVER_SUPPORT
	g_pcwebserver->RegisterListenerL(this);
	g_main_message_handler->SetCallBack(this,MSG_QUICK_UNITE_ATTENTION,0);

	// if widget_runtime is on and widget in standalone mode, g_webserver_manager is not initialized
	// something to be investigated and changed later
	if (g_webserver_manager && g_webserver_manager->IsFeatureEnabled())
	{
		OpStatus::Ignore(InitTrayIcon());
	}
#endif // WEBSERVER_SUPPORT

	if (g_application->ShowM2())
	{
		MessageEngine *engine = MessageEngine::GetInstance();
		engine->GetMasterProgress().AddNotificationListener(this);
		engine->AddAccountListener(this);
#ifdef IRC_SUPPORT
		engine->AddChatListener(this);
#endif //IRC_SUPPORT

		OpStatus::Ignore(InitTrayIcon());
	}

	OpStatus::Ignore(InitWindows7TaskbarIntegration());
}

WindowsOpTaskbar::~WindowsOpTaskbar()
{
	g_main_message_handler->UnsetCallBacks(this);
	g_pcui->UnregisterListener(this);

	ShutdownWindows7TaskbarIntegration();

#ifdef WEBSERVER_SUPPORT
	g_pcwebserver->UnregisterListener(this);
#endif // WEBSERVER_SUPPORT

	if (g_application->ShowM2())
	{
		MessageEngine *engine = MessageEngine::GetInstance();
		engine->GetMasterProgress().RemoveNotificationListener(this);
		engine->RemoveAccountListener(this);
#ifdef IRC_SUPPORT
		engine->RemoveChatListener(this);
#endif //IRC_SUPPORT
	}

	OpStatus::Ignore(UninitTrayIcon(TRUE));
}
void WindowsOpTaskbar::ShutdownWindows7TaskbarIntegration()
{
	if (NULL != m_windows7_taskbar_integration.get())
	{
		m_windows7_taskbar_integration->Shutdown();
		m_windows7_taskbar_integration.reset(NULL);
	}
}

OP_STATUS WindowsOpTaskbar::InitWindows7TaskbarIntegration()
{
	// Only integrate with the Windows 7 taskbar if we're the browser (and not
	// some other Opera product).
	// FIXME: Make Windows 7 integration for "other products".
	BOOL is_browser = TRUE;
#ifdef WIDGET_RUNTIME_SUPPORT
	is_browser = GadgetStartup::IsBrowserStartupRequested();
#endif // WIDGET_RUNTIME_SUPPORT
	if (is_browser)
	{
		m_windows7_taskbar_integration =
			OP_NEW(WindowsSevenTaskbarIntegration, ());
		if (NULL != m_windows7_taskbar_integration.get())
		{
			return m_windows7_taskbar_integration->Init();
		}
	}
	return OpStatus::OK;
}

void WindowsOpTaskbar::NeedNewMessagesNotification(Account* account, unsigned count)
{
	if (m_active_mail_windows.GetCount() == 0
		&& account->GetIncomingProtocol() != AccountTypes::RSS)
	{
		m_unread_mail = TRUE;
		Update(NIM_MODIFY);
	}
}

void WindowsOpTaskbar::OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active)
{
	if (active)
	{
		m_unread_mail = FALSE;
		Update(NIM_MODIFY);

		if (m_active_mail_windows.Find(mail_window) == -1)
			m_active_mail_windows.Add(mail_window);
	}
	else
		m_active_mail_windows.RemoveByItem(mail_window);
}

void WindowsOpTaskbar::OnAccountAdded(UINT16 account_id)
{
	OpStatus::Ignore(InitTrayIcon());
}

void WindowsOpTaskbar::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	OpStatus::Ignore(UninitTrayIcon());
}

void WindowsOpTaskbar::AppHidden()
{
	OpStatus::Ignore(InitTrayIcon(TRUE));
}

void WindowsOpTaskbar::AppShown()
{
	OpStatus::Ignore(UninitTrayIcon());
}

#ifdef IRC_SUPPORT
void WindowsOpTaskbar::OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread)
{
	m_unread_chat = unread > 0;
	Update(NIM_MODIFY);
}
#endif //IRC_SUPPORT

OP_STATUS WindowsOpTaskbar::InitTrayIcon(BOOL apphidden /* = FALSE */)
{
	if (m_trayicon_inited)
		return OpStatus::OK;

	if (!apphidden && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowTrayIcon) == 0)
		return OpStatus::ERR;

	if (!(apphidden
		|| g_application->ShowM2() && g_m2_engine->GetAccountManager()->GetAccountCount() > 0
#ifdef WEBSERVER_SUPPORT
		|| g_pcwebserver && g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable)
#endif
		))
		return OpStatus::ERR;

	WNDCLASSEX wnd_class;
	uni_char *name = UNI_L("OpTaskbar");

	// Invisible window class..
	wnd_class.cbSize = sizeof(wnd_class);
	wnd_class.style = CS_DBLCLKS;
	wnd_class.lpfnWndProc = (WNDPROC)WndProc;
	wnd_class.cbClsExtra = 0;
	wnd_class.cbWndExtra = 0;
	wnd_class.hInstance = hInst;
	wnd_class.hIcon = NULL;
	wnd_class.hIconSm = NULL;
	wnd_class.hCursor = (HICON) LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
	wnd_class.hbrBackground = NULL;
	wnd_class.lpszMenuName  = NULL;
	wnd_class.lpszClassName = name;
	RegisterClassEx(&wnd_class);

	// Create invisible window (no WS_VISIBLE).
	m_hwnd = CreateWindowEx(0, name, name, WS_POPUP, CW_USEDEFAULT, 0,
		CW_USEDEFAULT, 0, NULL, NULL, hInst, NULL);

	m_trayicon_inited = TRUE;

	RETURN_IF_ERROR(Update(NIM_ADD));

	return OpStatus::OK;
}

OP_STATUS WindowsOpTaskbar::UninitTrayIcon(BOOL force /* = FALSE */)
{
	if (!m_trayicon_inited)
		return OpStatus::OK;

	if (!force && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowTrayIcon) == 1)
		return OpStatus::OK;

	NOTIFYICONDATAA tnid;

	tnid.cbSize = sizeof(NOTIFYICONDATAA);
	tnid.hWnd = m_hwnd;
	tnid.uID = 1;
	tnid.uFlags = 0;

	Shell_NotifyIconA(NIM_DELETE, &tnid);
	DestroyWindow(m_hwnd);

	m_hwnd = NULL;

	m_trayicon_inited = FALSE;
	return OpStatus::OK;
}

#ifdef SELFTEST
OP_STATUS WindowsOpTaskbar::Notify(OpString8& message)
{
	if(!m_trayicon_inited)
	{
		RETURN_IF_ERROR(InitTrayIcon(TRUE));
	}
	NOTIFYICONDATAA tnid;

	ZeroMemory(&tnid, sizeof(tnid)); 

	// see http://msdn.microsoft.com/en-us/library/bb773352(VS.85).aspx and compatibility
	DWORD structure_size = sizeof(NOTIFYICONDATAA_V1_SIZE);

	WinType wintype = GetWinType();

	switch(wintype)
	{
	case WIN2K:
		structure_size = sizeof(NOTIFYICONDATAA_V2_SIZE);
		break;

	case WINXP:
		structure_size = sizeof(NOTIFYICONDATAA_V2_SIZE) + sizeof(HICON);	// HICON is the last member on Vista
		break;

		// everything from Vista and up
	default:
		structure_size = sizeof(NOTIFYICONDATAA);
		break;
	}
	tnid.cbSize = structure_size;
	tnid.hWnd = m_hwnd;
	tnid.uID = 1;
	tnid.uFlags = NIF_MESSAGE | NIF_INFO;
	tnid.uCallbackMessage = WM_OP_TASKBAR;
	tnid.uTimeout = 15000;
	tnid.dwInfoFlags = NIIF_INFO;

	lstrcpynA(tnid.szInfoTitle, "Opera", sizeof(tnid.szInfoTitle));
	lstrcpynA(tnid.szInfo, message.CStr(), sizeof(tnid.szInfo));

	Shell_NotifyIconA(NIM_MODIFY, &tnid);

	return OpStatus::OK;
}
#endif // SELFTEST

OP_STATUS WindowsOpTaskbar::Update(DWORD message)
{
	// we want the Windows 7 icon even if the tray icon is disabled
	if(IsSystemWin7() && message != NIM_ADD && NULL != m_windows7_taskbar_integration.get())
	{
		if (m_unread_mail && m_unread_chat)
		{
			m_windows7_taskbar_integration->SetNotifyIcon(UNI_L("ZCHATMAIL"));
		}
		else if(m_unread_mail)
		{
			m_windows7_taskbar_integration->SetNotifyIcon(UNI_L("ZMAILUNREAD"));
		}
		else if (m_unread_chat)
		{
			m_windows7_taskbar_integration->SetNotifyIcon(UNI_L("ZCHATATTENTION"));
		}
		else if (m_unite_attention)
		{
			m_windows7_taskbar_integration->SetNotifyIcon(UNI_L("ZUNITEDEFAULT"));
		}
		else
		{
			m_windows7_taskbar_integration->SetNotifyIcon(NULL);
		}
	}

	if(m_trayicon_inited)
	{
		HICON icon;

		int color = IsSystemWinXP() ? LR_DEFAULTCOLOR : LR_VGACOLOR;

		if(m_unread_mail && m_unread_chat)
		{
			icon = (HICON) LoadImage(hInst, UNI_L("ZMAILCHAT"), IMAGE_ICON, 16, 16, color | LR_SHARED);
		}
		else if(m_unread_mail)
		{
			icon = (HICON) LoadImage(hInst, g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA?UNI_L("ZMAIL"):UNI_L("ZMAILNEXT"), IMAGE_ICON, 16, 16, color | LR_SHARED);
		}
		else if (m_unread_chat)
		{
			icon = (HICON) LoadImage(hInst, g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA?UNI_L("ZIMNEW"):UNI_L("ZIMNEWNEXT"), IMAGE_ICON, 16, 16, color | LR_SHARED);
		}
		else if (m_unite_attention)
		{
			icon = (HICON) LoadImage(hInst, g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA?UNI_L("ZUNITE"):UNI_L("ZUNITENEXT"), IMAGE_ICON, 16, 16, color | LR_SHARED);
		}
		else
		{
			uni_char* icon_name;
			switch (g_desktop_product->GetProductType())
			{
				case PRODUCT_TYPE_OPERA_NEXT:
					icon_name = UNI_L("OPERA1NEXT");
					break;
				case PRODUCT_TYPE_OPERA_LABS:
					icon_name = UNI_L("OPERA2LABS");
					break;
				default:
					icon_name = UNI_L("OPERA");
					break;
			}

			icon = (HICON) LoadImage(hInst, icon_name, IMAGE_ICON, 16, 16, color | LR_SHARED);
		}
		NOTIFYICONDATAA tnid;

		tnid.cbSize = sizeof(NOTIFYICONDATAA);
		tnid.hWnd = m_hwnd;
		tnid.uID = 1;
		tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		tnid.uCallbackMessage = WM_OP_TASKBAR;
		tnid.hIcon = icon;
		lstrcpynA(tnid.szTip, "Opera", sizeof(tnid.szTip));

		Shell_NotifyIconA(message, &tnid);

		if (icon)
			DestroyIcon(icon);
	}
	return OpStatus::OK;
}

LONG APIENTRY WindowsOpTaskbar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UINT id;
	UINT mouse_msg;
	static UINT s_taskbarrestart;

	id = (UINT) wParam;
	mouse_msg = (UINT) lParam;

	if(msg == WM_CREATE)
	{
		s_taskbarrestart = RegisterWindowMessage(UNI_L("TaskbarCreated"));
	}
	else
	{
		if(msg == s_taskbarrestart)
		{
			// re-enable the tasktray icon
			WindowsOpTaskbar *taskbar = static_cast<WindowsDesktopGlobalApplication *>(g_desktop_global_application)->GetTaskbar();
			if(taskbar)
				taskbar->Update(NIM_ADD);
		}
	}

	if (msg != WM_OP_TASKBAR || !g_application)
	{
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	if (mouse_msg ==  WM_RBUTTONUP)
	{
		/*DWORD position = GetMessagePos();*/

		SetForegroundWindow(hwnd); // MS BUG Q135788

		g_application->GetMenuHandler()->ShowPopupMenu("Tray Popup Menu", PopupPlacement::AnchorAtCursor());

		PostMessage(hwnd, WM_NULL, 0, 0); // MS BUG Q135788
	}
	else if (mouse_msg == WM_LBUTTONDBLCLK)
	{
		SetForegroundWindow(hwnd);
		g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_OPERA);
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

#ifdef WEBSERVER_SUPPORT
void WindowsOpTaskbar::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_QUICK_UNITE_ATTENTION)
	{
		if(par2 > 0)
		{
			if(!m_unite_attention)
			{
				m_unite_attention = TRUE;
				Update(NIM_MODIFY);
			}
		}
		else
			if(m_unite_attention)
			{
				m_unite_attention = FALSE;
				Update(NIM_MODIFY);
			}
	}
}
#endif // WEBSERVER_SUPPORT
#endif // M2_SUPPORT

void WindowsOpTaskbar::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::UI && PrefsCollectionUI::ShowTrayIcon == PrefsCollectionUI::integerpref(pref)
#ifdef WEBSERVER_SUPPORT
		|| id == OpPrefsCollection::Webserver && PrefsCollectionWebserver::WebserverEnable == PrefsCollectionWebserver::integerpref(pref)
#endif // WEBSERVER_SUPPORT
		)
	{
		if(newvalue)
			InitTrayIcon();
		else
			UninitTrayIcon(TRUE);
	}
}

void WindowsOpTaskbar::OnExitStarted()
{
	ShutdownWindows7TaskbarIntegration();
}

WindowsSevenTaskbarIntegration::WindowsSevenTaskbarIntegration() :
	m_has_active_transfers(FALSE),
	m_posted_progress_update(FALSE),
	m_jumplist_created(FALSE),
	m_pTaskbarList(NULL),
	m_event_quit(NULL)
{
}

WindowsSevenTaskbarIntegration::~WindowsSevenTaskbarIntegration()
{
	// signal thread to quit
	if(m_event_quit)
	{
		WaitForThreadToQuit();

		CloseHandle(m_event_quit);
		m_event_quit = NULL;
	}
	if(g_main_message_handler)
	{
		g_main_message_handler->UnsetCallBacks(this);
	}
	if(m_pTaskbarList)
	{
		m_pTaskbarList->Release();
	}

	UINT32 cnt;
	for(cnt = m_workspaces.GetCount(); cnt; cnt--)
	{
		OpWorkspace *workspace = m_workspaces.Get(cnt-1);

		OnWorkspaceDeleted(workspace);
		workspace->RemoveListener(this);
	}
	for(cnt = m_speeddials.GetCount(); cnt; cnt--)
	{
		m_speeddials.Get(cnt-1)->RemoveListener(*this);
	}

	g_speeddial_manager->RemoveListener(*this);
	g_desktop_transfer_manager->RemoveTransferListener(this);
}

OP_STATUS WindowsSevenTaskbarIntegration::Init()
{
	if (!g_desktop_transfer_manager || !g_speeddial_manager)
	{
		// means desktop init was prematurely aborted
		return OpStatus::ERR;
	}
	g_desktop_transfer_manager->AddTransferListener(this);
	RETURN_IF_ERROR(g_speeddial_manager->AddListener(*this));

	g_application->AddBrowserDesktopWindowListener(this);

	g_main_message_handler->SetCallBack(this, MSG_WIN_RECREATE_JUMPLIST, 0);
	g_main_message_handler->SetCallBack(this, MSG_WIN_UPDATE_PROGRESS, 0);

	// not critical if this fails
	OpStatus::Ignore(ClearJumplistIconCache());

	return OpStatus::OK;
}

void WindowsSevenTaskbarIntegration::Shutdown()
{
	OP_ASSERT(g_application);

	if(g_application)
	{
		g_application->RemoveBrowserDesktopWindowListener(this);

		HRESULT hr = CreateJumpList(FALSE);
		OP_ASSERT(SUCCEEDED(hr));
		UNREFERENCED_PARAMETER(hr);	// silence warning
	}
}

void WindowsSevenTaskbarIntegration::OnSpeedDialAdded(const DesktopSpeedDial &sd)
{
	ScheduleJumplistUpdate();
	if (OpStatus::IsSuccess(m_speeddials.Add(const_cast<DesktopSpeedDial *>(&sd))))
		sd.AddListener(*this);
}
void WindowsSevenTaskbarIntegration::OnSpeedDialRemoving(const DesktopSpeedDial &sd)
{
	sd.RemoveListener(*this);
	m_speeddials.RemoveByItem(const_cast<DesktopSpeedDial *>(&sd));
	ScheduleJumplistUpdate();
}
void WindowsSevenTaskbarIntegration::OnSpeedDialReplaced(const DesktopSpeedDial &old_sd, const DesktopSpeedDial &new_sd)
{
	old_sd.RemoveListener(*this);
	m_speeddials.RemoveByItem(const_cast<DesktopSpeedDial *>(&old_sd));

	if (OpStatus::IsSuccess(m_speeddials.Add(const_cast<DesktopSpeedDial *>(&new_sd))))
		new_sd.AddListener(*this);

	ScheduleJumplistUpdate();
}
void WindowsSevenTaskbarIntegration::OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry)
{
	ScheduleJumplistUpdate();
}
void WindowsSevenTaskbarIntegration::OnSpeedDialUIChanged(const DesktopSpeedDial &sd)
{
	ScheduleJumplistUpdate();
}
void WindowsSevenTaskbarIntegration::OnSpeedDialExpired()
{
	ScheduleJumplistUpdate();
}

void WindowsSevenTaskbarIntegration::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_WIN_RECREATE_JUMPLIST)
	{
		HRESULT hr = CreateJumpList(TRUE);
		OP_ASSERT(SUCCEEDED(hr));
		UNREFERENCED_PARAMETER(hr);	// silence warning
	}
	else if(msg == MSG_WIN_UPDATE_PROGRESS)
	{
		UpdateProgress();
	}
}

/**************************************************************************
*
* Windows 7+ taskbar thumbnails code below
*
**************************************************************************/

HWND GetParentHWNDFromDesktopWindow(DesktopWindow* desktop_window)
{
	BrowserDesktopWindow *browser_window = reinterpret_cast<BrowserDesktopWindow *>(desktop_window->GetParentDesktopWindow());
	if(browser_window)
	{
		WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
		OP_ASSERT(browser_op_window);
		if(browser_op_window)
		{
			return browser_op_window->m_hwnd;
		}
	}
	return NULL;
}

void WindowsSevenTaskbarIntegration::OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if(!IsSystemWin7())
	{
		return;
	}
	if(desktop_window)
	{
		OpStatus::Ignore(desktop_window->AddListener(this));

		WindowsOpWindow *window = static_cast<WindowsOpWindow *>(desktop_window->GetOpWindow());
		if(window)
		{
			window->SetTaskbarMessageListener(this);

			HWND wnd = window->GetTaskbarProxyWindow();

			if(wnd)
			{
				/* HRESULT hr = */ CreateTaskbarThumbnail(wnd, GetParentHWNDFromDesktopWindow(desktop_window));
//				OP_ASSERT(SUCCEEDED(hr));
#ifdef _DEBUG
//				Win32DebugLogging(UNI_L("Win7: Window added, HWND: 0x08%lx, parent: 0x%08lx hr: 0x%08lx\n"), wnd, window->GetParentHWND(), hr);
#endif
			}
		}
	}
}

void WindowsSevenTaskbarIntegration::OnWorkspaceDeleted(OpWorkspace* workspace)
{
	// On exit, the OnDesktopWindowRemoved() is not called.  This one is always called, so remove ourself as listener here

	if(!IsSystemWin7())
	{
		return;
	}
	INT32 cnt;

	for(cnt = 0; cnt < workspace->GetDesktopWindowCount(); cnt++)
	{
		DesktopWindow *desktop_window = workspace->GetDesktopWindowFromStack(cnt);
		if(desktop_window)
		{
			desktop_window->RemoveListener(this);
		}
	}
}

void WindowsSevenTaskbarIntegration::OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if(desktop_window)
	{
		desktop_window->RemoveListener(this);
	}
	if(desktop_window && m_pTaskbarList)
	{
		WindowsOpWindow *window = static_cast<WindowsOpWindow *>(desktop_window->GetOpWindow());
		if(window)
		{
			HWND wnd = window->GetTaskbarProxyWindow(FALSE);	// FALSE means don't create it if it's missing
			if(wnd)
			{
				HRESULT hr = RemoveTaskbarThumbnail(wnd);
				OP_ASSERT(SUCCEEDED(hr));
				UNREFERENCED_PARAMETER(hr);	// silence warning
#ifdef _DEBUG
//				Win32DebugLogging(UNI_L("Win7: Window removed, HWND: 0x%08lx, hr: 0x%08lx\n"), wnd, hr);
#endif
			}
			window->SetTaskbarMessageListener(NULL);
		}
	}
}

void WindowsSevenTaskbarIntegration::OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate)
{
	WindowsOpWindow *window = static_cast<WindowsOpWindow *>(desktop_window->GetOpWindow());
	if(window && m_pTaskbarList)
	{
		HWND wnd = window->GetTaskbarProxyWindow();
		if(wnd)
		{
			HRESULT hr = ActivateTaskbarThumbnail(wnd, GetParentHWNDFromDesktopWindow(desktop_window));
			OP_ASSERT(SUCCEEDED(hr));
			UNREFERENCED_PARAMETER(hr);	// silence warning
#ifdef _DEBUG
//			Win32DebugLogging(UNI_L("Win7: Window activated, HWND: 0x%08lx, parent: 0x%08lx hr: 0x%08lx\n"), wnd, window->GetParentHWND(), hr);
#endif
		}
	}
}

void WindowsSevenTaskbarIntegration::OnDesktopWindowContentChanged(DesktopWindow* desktop_window)
{
	WindowsOpWindow *window = static_cast<WindowsOpWindow *>(desktop_window->GetOpWindow());
	if(window && m_pTaskbarList && !window->HasDirtyThumbnail())	// we're already marked as dirty, no need to do it again
	{
		HWND wnd = window->GetTaskbarProxyWindow();
		if(wnd)
		{
			window->SetDirtyThumbnail(TRUE);

			OPDwmInvalidateIconicBitmaps(wnd);
#ifdef _DEBUG
//			Win32DebugLogging(UNI_L("Win7: Window changed, HWND: 0x%08lx, hr: 0x%08lx\n"), wnd, hr);
#endif
		}
	}
}

void WindowsSevenTaskbarIntegration::OnDesktopWindowOrderChanged(OpWorkspace* workspace)
{
	if(!m_pTaskbarList)
	{
		return;
	}

	OpVector<DesktopWindow> windows;
	OpStatus::Ignore(workspace->GetDesktopWindows(windows));

	// re-arrange all windows from the end to the top
	WindowsOpWindow* previous = NULL;
	for (INT32 pos = windows.GetCount() - 1; pos >= 0; pos--)
	{
		DesktopWindow* desktop_window = windows.Get(pos);
		WindowsOpWindow* window = static_cast<WindowsOpWindow*>(desktop_window->GetOpWindow());
		HWND wnd = window ? window->GetTaskbarProxyWindow() : NULL;

		if(!wnd)
			return;

		HWND wnd_after = previous ? previous->GetTaskbarProxyWindow() : NULL;
		HRESULT hr = m_pTaskbarList->SetTabOrder(wnd, wnd_after);
		OP_ASSERT(SUCCEEDED(hr));
		UNREFERENCED_PARAMETER(hr);	// silence warning
		previous = window;
	}
}

void WindowsSevenTaskbarIntegration::OnBrowserDesktopWindowCreated(BrowserDesktopWindow *window)
{
	if(!IsSystemWin7())
	{
		return;
	}
	if(!m_jumplist_created)
	{
		m_jumplist_created = TRUE;
		ScheduleJumplistUpdate();
	}
	OpWorkspace *workspace = window->GetWorkspace();
	if(workspace)
	{
		workspace->AddListener(this);
		m_workspaces.Add(workspace);
	}
#ifdef _DEBUG
//	Win32DebugLogging(UNI_L("Win7: Browser window created, HWND: 0x%08lx\n"), reinterpret_cast<WindowsOpWindow *>(window)->m_hwnd);
#endif
}

void WindowsSevenTaskbarIntegration::OnBrowserDesktopWindowDeleting(BrowserDesktopWindow *window)
{
	if(!IsSystemWin7())
	{
		return;
	}
	OpWorkspace *workspace = window->GetWorkspace();
	if(workspace)
	{
		OnWorkspaceDeleted(workspace);

		workspace->RemoveListener(this);
		m_workspaces.RemoveByItem(workspace);
	}
}

void WindowsSevenTaskbarIntegration::OnMessageSendIconicThumbnail(WindowsOpWindow *window, UINT32 width, UINT32 height)
{
	DesktopWindow *desktop_window = window->GetDesktopWindow();
	if(desktop_window)
	{
		Image image;

		image = desktop_window->GetThumbnailImage(width, height, TRUE);
		if(!image.IsEmpty())
		{
			OpBitmap *bitmap = image.GetBitmap(NULL);
			if(bitmap)
			{
				if(desktop_window->HasFixedThumbnailImage())
				{
					OpRect src_rect(0, 0, image.Width(), image.Height());
					OpRect dst_rect(0, 0, width, height);
					OpRect windows_rect(0, 0, width, height);	// won't be modified

					SmartCropScaling crop_scaling;

					crop_scaling.SetOriginalSize(width, height);
					crop_scaling.GetCroppedRects(src_rect, dst_rect);

					OpBitmap *bitmap_p = NULL;
					if (OpStatus::IsError(OpBitmap::Create(&bitmap_p, width, height, FALSE, TRUE, 0, 0, TRUE)))
					{
						image.ReleaseBitmap();
						return;
					}
					OpAutoPtr<OpBitmap> bitmap_tmp(bitmap_p);

					OpPainter *painter = bitmap_tmp->GetPainter();
					if(painter)
					{
						painter->ClearRect(windows_rect);
						painter->DrawBitmapScaled(bitmap, src_rect, dst_rect);

						bitmap_tmp->ReleasePainter();

						HBITMAP bm = CreateHBITMAPFromOpBitmap(bitmap_p);
						if(bm)
						{
							HRESULT hr = OPDwmSetIconicThumbnail(window->GetTaskbarProxyWindow(), bm, 0 /* DWM_SIT_DISPLAYFRAME */);
							OP_ASSERT(SUCCEEDED(hr));
							UNREFERENCED_PARAMETER(hr);	// silence warning

							DeleteObject(bm);
						}
					}
				}
				else
				{
					HBITMAP bm = CreateHBITMAPFromOpBitmap(bitmap);
					if(bm)
					{
						HRESULT hr = OPDwmSetIconicThumbnail(window->GetTaskbarProxyWindow(), bm, 0 /* DWM_SIT_DISPLAYFRAME */);
						OP_ASSERT(SUCCEEDED(hr));
						UNREFERENCED_PARAMETER(hr);	// silence warning

						DeleteObject(bm);
					}
				}
				image.ReleaseBitmap();

				window->SetDirtyThumbnail(FALSE);
			}
		}
	}
}

void WindowsSevenTaskbarIntegration::OnMessageSendIconicLivePreviewBitmap(WindowsOpWindow *window, UINT32 width, UINT32 height)
{
	DesktopWindow *desktop_window = window->GetDesktopWindow();
	if(desktop_window && m_pTaskbarList)
	{
		Image image = desktop_window->GetSnapshotImage();
		if(!image.IsEmpty())
		{
			OpBitmap *bitmap = image.GetBitmap(NULL);
			if(bitmap)
			{
				HBITMAP bm = CreateHBITMAPFromOpBitmap(bitmap);
				if(bm)
				{
					POINT pt;
					int x, y;

					window->GetDocumentWidgetCoordinates(x, y);

					pt.x = x;
					pt.y = y;
#ifdef _DEBUG
//					Win32DebugLogging(UNI_L("Win7: LivePreview, x: %ld, y: %ld\n"), pt.x, pt.y);
#endif

					HRESULT hr = OPDwmSetIconicLivePreviewBitmap(window->GetTaskbarProxyWindow(), bm, &pt, 0);
					OP_ASSERT(SUCCEEDED(hr));
					UNREFERENCED_PARAMETER(hr);	// silence warning

					DeleteObject(bm);
				}
				image.ReleaseBitmap();
			}
		}
	}
}

void WindowsSevenTaskbarIntegration::OnMessageActivateWindow(WindowsOpWindow *window, BOOL activate)
{
	if(!IsSystemWin7())
	{
		return;
	}
	DesktopWindow *desktop_window = window->GetDesktopWindow();
	if(desktop_window)
	{
		desktop_window->Activate(activate);
	}
}

void WindowsSevenTaskbarIntegration::OnMessageCloseWindow(WindowsOpWindow *window)
{
	if(!IsSystemWin7())
	{
		return;
	}
	window->SetTaskbarMessageListener(NULL);

	DesktopWindow *desktop_window = window->GetDesktopWindow();
	if(desktop_window && desktop_window->IsClosableByUser())
	{
		// we need to de-register it as a tab in Windows 7 as the proxy window will already be gone when Close() is called
		HWND wnd = window->GetTaskbarProxyWindow();
		if(wnd)
		{
			HRESULT hr = RemoveTaskbarThumbnail(wnd);
			OP_ASSERT(SUCCEEDED(hr));
			UNREFERENCED_PARAMETER(hr);	// silence warning
#ifdef _DEBUG
//				Win32DebugLogging(UNI_L("Win7: Window removed, HWND: 0x%08lx, hr: 0x%08lx\n"), wnd, hr);
#endif
		}
		desktop_window->Close(FALSE, TRUE);
	}
}

BOOL WindowsSevenTaskbarIntegration::CanCloseWindow(WindowsOpWindow *window)
{
	DesktopWindow *desktop_window = window->GetDesktopWindow();

	return desktop_window && desktop_window->IsClosableByUser();
}

HRESULT WindowsSevenTaskbarIntegration::CreateTaskbarThumbnail(HWND hWnd, HWND parent_hwnd)
{
	HRESULT hr;

	if(!m_pTaskbarList)
	{
		hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITaskbarList3), (LPVOID *)&m_pTaskbarList);
		if (FAILED(hr))
		{
			return hr;
		}
		hr = m_pTaskbarList->HrInit();
		if (FAILED(hr))
		{
			return hr;
		}
	}
	hr = m_pTaskbarList->RegisterTab(hWnd, parent_hwnd);
	if (SUCCEEDED(hr))
	{
		hr = m_pTaskbarList->SetTabOrder(hWnd, NULL);
	}
	return hr;
}

HRESULT WindowsSevenTaskbarIntegration::ActivateTaskbarThumbnail(HWND hWnd, HWND parent_hwnd)
{
	HRESULT hr = S_FALSE;

	if(m_pTaskbarList)
	{
#if _MSC_VER < 1600
		hr = m_pTaskbarList->SetTabActive(hWnd, parent_hwnd, (TBATFLAG)0);
#else
		hr = m_pTaskbarList->SetTabActive(hWnd, parent_hwnd, 0);
#endif
	}
	return hr;
}

HRESULT WindowsSevenTaskbarIntegration::RemoveTaskbarThumbnail(HWND hWnd)
{
	HRESULT hr = S_FALSE;

	if(m_pTaskbarList)
	{
		hr = m_pTaskbarList->DeleteTab(hWnd);
		if(SUCCEEDED(hr))
		{
			hr = m_pTaskbarList->UnregisterTab(hWnd);
		}
	}
	return hr;
}

/**************************************************************************
*
* Windows 7+ taskbar progress bar code below
*
**************************************************************************/
void WindowsSevenTaskbarIntegration::OnTransferProgress(TransferItemContainer* transferItem, OpTransferListener::TransferStatus status)
{
	switch(status)
	{
	case OpTransferListener::TRANSFER_PROGRESS:
	case OpTransferListener::TRANSFER_DONE:
	case OpTransferListener::TRANSFER_ABORTED:
	case OpTransferListener::TRANSFER_SHARING_FILES:
		{
			if(!m_posted_progress_update)
			{
				m_posted_progress_update = TRUE;
				g_main_message_handler->PostDelayedMessage(MSG_WIN_UPDATE_PROGRESS, 0, 0,  DELAY_UPDATE_PROGRESS);
			}
		}
		break;
	}
}

void WindowsSevenTaskbarIntegration::UpdateProgress()
{
	HRESULT hr;

	if(!IsSystemWin7())
	{
		return;
	}
	if(!m_pTaskbarList)
	{
		hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITaskbarList3), (LPVOID *)&m_pTaskbarList);
		if (FAILED(hr))
		{
			OP_ASSERT(FALSE);
			return;
		}
		hr = m_pTaskbarList->HrInit();
		if (FAILED(hr))
		{
			OP_ASSERT(FALSE);
			return;
		}
	}
	m_posted_progress_update = FALSE;

	UINT32 remaining_transfers = 0;
	OpFileLength downloaded_size;
	OpFileLength total_sizes;

	g_desktop_transfer_manager->GetFilesSizeInformation(downloaded_size, total_sizes, &remaining_transfers);

	m_has_active_transfers = remaining_transfers != 0;

	// It doesn't really matter which window we use for this, so we'll just pick the first one
	OpWorkspace *workspace = m_workspaces.Get(0);
	DesktopWindow* window = workspace ? workspace->GetActiveDesktopWindow() : NULL;
	HWND hwnd = window ? GetParentHWNDFromDesktopWindow(window) : NULL;

	OP_ASSERT(hwnd && m_pTaskbarList);

	if(hwnd && m_pTaskbarList)
	{
		if(m_has_active_transfers)
		{
			m_pTaskbarList->SetProgressValue(hwnd, downloaded_size, total_sizes);
		}
		else
		{
			m_pTaskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
		}
	}
}

/**************************************************************************
*
* Windows 7+ taskbar overlay icon code below
*
**************************************************************************/
HRESULT WindowsSevenTaskbarIntegration::SetNotifyIcon(const uni_char* icon)
{
	HRESULT hr = S_OK;

	if(!m_pTaskbarList)
	{
		hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITaskbarList3), (LPVOID *)&m_pTaskbarList);
		if (FAILED(hr))
		{
			return hr;
		}
		hr = m_pTaskbarList->HrInit();
		if (FAILED(hr))
		{
			return hr;
		}
	}
	HWND hwnd = NULL;

	BrowserDesktopWindow *browser_window = g_application->GetActiveBrowserDesktopWindow();
	if(browser_window)
	{
		WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
		OP_ASSERT(browser_op_window);
		if(browser_op_window)
		{
			hwnd = browser_op_window->m_hwnd;
		}
	}

	if(m_pTaskbarList)
	{
		HICON win7_icon = NULL;
		if (icon)
			win7_icon = (HICON) LoadImage(hInst, icon, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED);

		hr = m_pTaskbarList->SetOverlayIcon(hwnd, win7_icon, NULL);
		OP_ASSERT(SUCCEEDED(hr));
		
		if (win7_icon)
			DestroyIcon(win7_icon);
	}
	return hr;
}

/**************************************************************************
*
* Windows 7+ Jumplist code below
*
**************************************************************************/

OP_STATUS WindowsSevenTaskbarIntegration::ClearJumplistIconCache()
{
	// If this assert goes off, core is not initialized. 
	OP_ASSERT(g_opera && g_folder_manager);

	// Delete all .ico files within the cache directory
	OpFolderLister *folder_lister = OpFile::GetFolderLister(OPFILE_JUMPLIST_CACHE_FOLDER, UNI_L("*.ico"));
	if (!folder_lister)
		return OpStatus::ERR_NO_MEMORY;

	while (folder_lister->Next())
	{
		const uni_char* file_path = folder_lister->GetFullPath();
		if (file_path && !folder_lister->IsFolder())
		{
			OpFile file;

			RETURN_IF_ERROR(file.Construct(file_path));

			file.Delete(FALSE);
		}
	}
	OP_DELETE(folder_lister);

	return OpStatus::OK;
}

void WindowsSevenTaskbarIntegration::ScheduleJumplistUpdate()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_WIN_RECREATE_JUMPLIST, 0, 0);
	g_main_message_handler->PostDelayedMessage(MSG_WIN_RECREATE_JUMPLIST, 0, 0,  DELAY_CREATE_JUMPLIST);
}

/* static */
HRESULT WindowsSevenTaskbarIntegration::CreateShellLinkObject(OpComPtr<IShellLink>& link, JumpListInformationBase* information, const uni_char *url, const uni_char *title, const uni_char *arguments, const uni_char *icon_path)
{
	HRESULT hr = S_OK;

	// An IShellLink object is almost the same as an application shortcut it requires
	// the absolute path to an application, an argument string, and a title string.

	hr = link.CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	// Set the application path.
	// Exit if we fail as it doesn't make any sense to add a shortcut we can't execute
	hr = link->SetPath(information->m_full_program_path);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	hr = link->SetShowCmd(SW_SHOWNORMAL);
	OP_ASSERT(SUCCEEDED(hr));

	// TODO:  Should we add other switches that might have been used too?

	// arguments contains both the comment line switches and the url
	hr = link->SetArguments(arguments);
	if (FAILED(hr))
	{
		return hr;
	}
	// Attach the optional icon path to this IShellLink object.
	if(icon_path)
	{
		link->SetIconLocation(icon_path, 0);
	}

	OpComPtr<IPropertyStore> prop_store;

	hr = link.QueryInterface(&prop_store.p);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	PROPVARIANT pvt;

	pvt.vt = VT_LPWSTR;
	pvt.pwszVal = (LPWSTR)title;

	hr = prop_store->SetValue(PKEY_Title, pvt);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	hr = prop_store->Commit();
	OP_ASSERT(SUCCEEDED(hr));

	return hr;
}

OP_STATUS WindowsSevenTaskbarIntegration::BuildFrequentCategoryList(JumpListInformationFrequent* information)
{
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	OpVector<HistoryModelPage> toplist;

	if(history_model)
	{
		history_model->GetTopItems(toplist, 5);
	}
	OpString address;
	OpString title;
	const uni_char *arguments = UNI_L("-newtab");

	INT32 count = toplist.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		HistoryModelPage* item = toplist.Get(i);

		item->GetAddress(address);
		item->GetTitle(title);

		OpString filename;
		OpString tmpurl;

		if(arguments)
		{
			tmpurl.Set(arguments);
		}
		if(address.HasContent())
		{
			tmpurl.Append(UNI_L(" "));
			tmpurl.AppendFormat(UNI_L("\"%s\""), address.CStr());

			RETURN_IF_ERROR(GenerateICOFavicon(address.CStr(), filename));
			if(filename.IsEmpty())
			{
				// if not, use the standard icon in the Opera executable
				filename.Set(information->m_full_program_path);
			}
		}
		RETURN_IF_ERROR(information->AddEntry(i, title.CStr(), address.CStr(), tmpurl.CStr(), filename.CStr()));
	}
	return OpStatus::OK;
}

OP_STATUS WindowsSevenTaskbarIntegration::BuildSpeedDialCategoryList(JumpListInformationSpeedDial* information)
{
	if(g_speeddial_manager)
	{
		uni_char *arguments = UNI_L("-newtab");
		UINT32 i;

		// iterate through all speed dials
		for(i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
		{
			const DesktopSpeedDial *sd = g_speeddial_manager->GetSpeedDial(i);
			if(sd && !sd->IsEmpty())
			{
				OpString filename;
				OpString tmpurl;

				if(arguments)
				{
					tmpurl.Set(arguments);
				}
				if(sd->GetURL().HasContent())
				{
					tmpurl.Append(UNI_L(" "));
					tmpurl.AppendFormat(UNI_L("\"%s\""), sd->GetURL().CStr());

					RETURN_IF_ERROR(GenerateICOFavicon(sd->GetURL().CStr(), filename));
					if(filename.IsEmpty())
					{
						// if not, use the standard icon in the Opera executable
						filename.Set(information->m_full_program_path);
					}
				}
				RETURN_IF_ERROR(information->AddEntry(i, sd->GetTitle().CStr(), sd->GetDisplayURL().CStr(), tmpurl.CStr(), filename.CStr()));
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS WindowsSevenTaskbarIntegration::GenerateICOFavicon(OpStringC url, OpString& filename)
{
	// If this assert goes off, core is not initialized. 
	OP_ASSERT(g_opera && g_folder_manager && g_favicon_manager);

	// try for a favicon for the url first
	Image image = g_favicon_manager->Get(url);
	if(!image.IsEmpty())
	{
		OpString path;

		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_JUMPLIST_CACHE_FOLDER, path));

		// generate a unique filename based on a guid
		OpString8 hash;
		OpString8 url_seed;

		RETURN_IF_ERROR(url_seed.SetUTF8FromUTF16(url.CStr()));
		RETURN_IF_ERROR(StringUtils::CalculateMD5Checksum(url_seed, hash));

		OpString hash16;
		RETURN_IF_ERROR(hash16.Set(hash.CStr()));
		RETURN_IF_ERROR(path.AppendFormat(UNI_L("%s.ico"), hash16.CStr()));

		RETURN_IF_ERROR(IconUtils::WriteIcoFile(image, path));

		RETURN_IF_ERROR(filename.Set(path));
	}
	return OpStatus::OK;
}

OP_STATUS WindowsSevenTaskbarIntegration::BuildTasksCategoryList(JumpListInformationTasks* information)
{
	uni_char *arguments = NULL;
	OpString title, filename;

	filename.Set(information->m_full_program_path);

	arguments = UNI_L("-newtab");

	g_languageManager->GetString(Str::MI_IDM_MENU_PAGEBAR_NEW_PAGE, title);

	RETURN_IF_ERROR(information->AddEntry(0, title.CStr(), NULL, arguments, filename.CStr()));

	arguments = UNI_L("-newprivatetab");

	g_languageManager->GetString(Str::MI_IDM_MENU_NEW_PRIVATE_PAGE, title);

	RETURN_IF_ERROR(information->AddEntry(1, title.CStr(), NULL, arguments, filename.CStr()));

	return OpStatus::OK;
}

HRESULT WindowsSevenTaskbarIntegration::AddFrequentCategoryToList(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved)
{
	BOOL items_added = FALSE;

	// We once add the given items to this collection object and add this collection to the JumpList.
	OpComPtr<IObjectCollection> collection;
	HRESULT hr = collection.CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER);
	if (FAILED(hr))
	{
		return hr;
	}
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	OpVector<HistoryModelPage> toplist;

	if(history_model)
	{
		history_model->GetTopItems(toplist, 5);
	}

	OpString address;
	OpString title;
	uni_char *arguments = UNI_L("-newtab");

	INT32 count = toplist.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		HistoryModelPage* item = toplist.Get(i);

		item->GetAddress(address);
		item->GetTitle(title);

		OpComPtr<IShellLink> link;

		hr = CreateShellLinkObject(link, &information, address.CStr(), title.CStr(), arguments);
		OP_ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return hr;
		}
		// Add this IShellLink object to the given collection.
		hr = collection->AddObject(link);
		OP_ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return hr;
		}
		items_added = TRUE;
	}
	//	hr = dest_list->AppendKnownCategory(KDC_FREQUENT);
	//	OP_ASSERT(SUCCEEDED(hr));

	if(items_added)
	{
		OpComPtr<IObjectArray> object_array;

		hr = collection.QueryInterface(&object_array.p);
		OP_ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return hr;
		}
		hr = dest_list->AppendCategory(UNI_L("Frequent"), object_array);
		OP_ASSERT(SUCCEEDED(hr));
	}
	return hr;
}

HRESULT WindowsSevenTaskbarIntegration::AddSpeedDialCategoryToList(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved)
{
	BOOL items_added = FALSE;

	// We once add the given items to this collection object and add this collection to the JumpList.
	OpComPtr<IObjectCollection> collection;
	HRESULT hr = collection.CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER);
	if (FAILED(hr))
	{
		return hr;
	}
	if(g_speeddial_manager)
	{
		uni_char *arguments = UNI_L("-newtab");
		UINT32 i;

		// iterate through all speed dials
		for(i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
		{
			const DesktopSpeedDial *sd = g_speeddial_manager->GetSpeedDial(i);
			if(sd && !sd->IsEmpty())
			{
				OpComPtr<IShellLink> link;

				hr = CreateShellLinkObject(link, &information, sd->GetURL().CStr(), sd->GetTitle().CStr(), arguments);
				OP_ASSERT(SUCCEEDED(hr));
				if (FAILED(hr))
				{
					return hr;
				}
				// Add this IShellLink object to the given collection.
				hr = collection->AddObject(link);
				OP_ASSERT(SUCCEEDED(hr));
				if (FAILED(hr))
				{
					return hr;
				}
				items_added = TRUE;
			}
		}
	}
	if(items_added)
	{
		OpComPtr<IObjectArray> object_array;

		hr = collection.QueryInterface(&object_array.p);
		OP_ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return hr;
		}
		OpString title;

		g_languageManager->GetString(Str::S_SPEED_DIAL, title);

		hr = dest_list->AppendCategory(title.CStr(), object_array);
		OP_ASSERT(SUCCEEDED(hr));
	}
	return hr;
}

HRESULT WindowsSevenTaskbarIntegration::AddTasks(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved)
{
	// We once add the given items to this collection object and add this collection to the JumpList.
	OpComPtr<IObjectCollection> collection;
	HRESULT hr = collection.CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER);
	if (FAILED(hr))
	{
		return hr;
	}
	uni_char *arguments = NULL;
	OpString title;

	arguments = UNI_L("-newtab");

	g_languageManager->GetString(Str::MI_IDM_MENU_PAGEBAR_NEW_PAGE, title);

	OpComPtr<IShellLink> link;

	hr = CreateShellLinkObject(link, &information, NULL, title.CStr(), arguments);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	// Add this IShellLink object to the given collection.
	hr = collection->AddObject(link);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	arguments = UNI_L("-newprivatetab");

	g_languageManager->GetString(Str::MI_IDM_MENU_NEW_PRIVATE_PAGE, title);

	OpComPtr<IShellLink> link2;

	hr = CreateShellLinkObject(link2, &information, NULL, title.CStr(), arguments);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	// Add this IShellLink object to the given collection.
	hr = collection->AddObject(link2);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	OpComPtr<IObjectArray> object_array;

	hr = collection.QueryInterface(&object_array.p);
	OP_ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}
	hr = dest_list->AddUserTasks(object_array);
	OP_ASSERT(SUCCEEDED(hr));

	return hr;
}

#define MAX_JUMPLIST_TYPES	3

HRESULT WindowsSevenTaskbarIntegration::CreateJumpList(BOOL async)
{
	HRESULT hr = S_OK;

	if(!IsSystemWin7())
	{
		return S_OK;
	}
	OpAutoPtr<JumpListThreadData> thread_data(OP_NEW(JumpListThreadData, ()));
	if(!thread_data.get())
	{
		return GetLastError();
	}
	// create an array of the various jumplist types we use
	OpAutoArray<JumpListInformationBase *> jumplists(OP_NEWA(JumpListInformationBase *, MAX_JUMPLIST_TYPES + 1)); // array is NULL terminated
	if(!jumplists.get())
	{
		return GetLastError();
	}
	OpString title;

	JumpListInformationBase** jumplists_ptr = jumplists.get();

	g_languageManager->GetString(Str::S_JUMPLIST_FREQUENT, title);

	*jumplists_ptr = OP_NEW(JumpListInformationFrequent, (title.CStr()));
	if(!*jumplists_ptr)
	{
		return GetLastError();
	}
	BuildFrequentCategoryList(static_cast<JumpListInformationFrequent *>(*jumplists_ptr++));

	g_languageManager->GetString(Str::S_SPEED_DIAL, title);

	*jumplists_ptr = OP_NEW(JumpListInformationSpeedDial, (title.CStr()));
	if(!*jumplists_ptr)
	{
		return GetLastError();
	}
	BuildSpeedDialCategoryList(static_cast<JumpListInformationSpeedDial *>(*jumplists_ptr++));

	*jumplists_ptr = OP_NEW(JumpListInformationTasks, (NULL));
	if(!*jumplists_ptr)
	{
		return GetLastError();
	}
	BuildTasksCategoryList(static_cast<JumpListInformationTasks *>(*jumplists_ptr++));

	*jumplists_ptr = NULL;	// terminate

	if(!m_event_quit)
	{
		m_event_quit = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	else
	{
		// Set event to non-signaled
		ResetEvent(m_event_quit);
	}
	thread_data.get()->event_quit = m_event_quit;
	thread_data.get()->jumplists = jumplists.release();

	// this is the async code path - spawns a thread to update the list. Don't change this to use _beginthreadex.
	uintptr_t thread = _beginthread(&JumplistThreadFunc, 0, (void *)thread_data.get());
	if(!thread)
	{
		return GetLastError();
	}
	thread_data.release();
	if(async == FALSE)
	{
		// this is supposed to be a synchronous operation, wait for the thread to do its thing
		WaitForThreadToQuit();
	}
	return hr;
}

// Determines if the provided IShellItem is listed in the array of items that the user has removed
BOOL _IsItemInArray(const uni_char* path, IObjectArray *poaRemoved)
{
	BOOL ret = FALSE;
	UINT items;
	if (SUCCEEDED(poaRemoved->GetCount(&items)))
	{
		IShellLink *psiCompare;
		for (UINT i = 0; !ret && i < items; i++)
		{
			if (SUCCEEDED(poaRemoved->GetAt(i, IID_PPV_ARGS(&psiCompare))))
			{
				uni_char removedpath[MAX_PATH];

				ret = psiCompare->GetArguments(removedpath, MAX_PATH);

				ret = !uni_stricmp(path, removedpath);

				psiCompare->Release();
			}
		}
	}
	return ret;
}
void WindowsSevenTaskbarIntegration::WaitForThreadToQuit()
{
	if(m_event_quit)
	{
		// wait for thread to signal it will quit
		if(WAIT_OBJECT_0 != (WaitForSingleObject(m_event_quit, 1000*15)))	// wait max 15 seconds
		{
			OP_ASSERT(FALSE);
		}
		CloseHandle(m_event_quit);
		m_event_quit = NULL;
	}
}

// this is the thread used to create the jumplist
void __cdecl WindowsSevenTaskbarIntegration::JumplistThreadFunc( void* pArguments )
{
	JumpListThreadData *thread_data = reinterpret_cast<JumpListThreadData *>(pArguments);
	JumpListInformationBase** jumplists_ptr = thread_data->jumplists;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	OpComPtr<ICustomDestinationList> dest_list;

	HRESULT hr = dest_list.CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER);
	if (SUCCEEDED(hr))
	{
		UINT uMaxSlots;
		OpComPtr<IObjectArray> poaRemoved;

		hr = dest_list->BeginList(&uMaxSlots, __uuidof(IObjectArray), reinterpret_cast<void **>(&poaRemoved));
		if (SUCCEEDED(hr))
		{
			if(uMaxSlots < 2)
			{
				// ensure it's not below 2, although I don't think it can be.  
				uMaxSlots = 15;
			}
			uMaxSlots -= 2;	// make room for the tasks

			BOOL items_added = FALSE;

			while(*jumplists_ptr)
			{
				// We once add the given items to this collection object and add this collection to the JumpList.
				OpComPtr<IObjectCollection> collection;
				HRESULT hr = collection.CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER);
				if (SUCCEEDED(hr))
				{
					JumpListInformationBase *list = *jumplists_ptr;

					UINT32 offset = 0;

					JumpListInformationBase::URLEntry *entry = list->GetEntry(offset++);
					while(entry)
					{
						OpComPtr<IShellLink> link;

						hr = WindowsSevenTaskbarIntegration::CreateShellLinkObject(link, list, entry->address, entry->title, entry->arguments, entry->icon_path);
						OP_ASSERT(SUCCEEDED(hr));
						if (FAILED(hr))
						{
							break;
						}
						// don't add it if it's in the removed list as adding the category will fail then
						if(!_IsItemInArray(entry->arguments, poaRemoved))
						{
							// Add this IShellLink object to the given collection.
							hr = collection->AddObject(link);
							OP_ASSERT(SUCCEEDED(hr));
							if (FAILED(hr))
							{
								break;
							}
							items_added = TRUE;
						}
						link.Release();

						entry = list->GetEntry(offset++);
					}
					if(items_added)
					{
						OpComPtr<IObjectArray> object_array;

						hr = collection.QueryInterface(&object_array.p);
						OP_ASSERT(SUCCEEDED(hr));
						if (SUCCEEDED(hr))
						{
							if((*jumplists_ptr)->GetType() == JUMPLIST_TYPE_TASKS)
							{
								hr = dest_list->AddUserTasks(object_array);
								OP_ASSERT(SUCCEEDED(hr));
							}
							else if((*jumplists_ptr)->m_header_title)
							{
								UINT count;
								hr = object_array->GetCount(&count);
								if (count > 0)
									hr = dest_list->AppendCategory((*jumplists_ptr)->m_header_title, object_array);

								// This might fail with E_ACCESSDENIED if the user has turned off tracking of last used documents.
								OP_ASSERT(SUCCEEDED(hr) || hr == E_ACCESSDENIED);
							}
						}
					}
					jumplists_ptr++;
					collection.Release();
				}
			}
			hr = dest_list->CommitList();

			OP_ASSERT(SUCCEEDED(hr));
		}
	}
	CoUninitialize();

	jumplists_ptr = thread_data->jumplists;
	while(*jumplists_ptr)
	{
		OP_DELETE(*jumplists_ptr);
		jumplists_ptr++;
	}
	OP_DELETEA(thread_data->jumplists);

	// signal main thread that we're done
	if(thread_data->event_quit)
	{
		SetEvent(thread_data->event_quit);
	}
	OP_DELETE(thread_data);
}
