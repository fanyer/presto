/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "WindowsDesktopGlobalApplication.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/kioskmanager.h"
#ifdef _DDE_SUPPORT_
# include "platforms/windows/userdde.h"
#endif
#include "platforms/windows/windows_ui/menubar.h"
#include "platforms/windows/user.h"

#ifdef _DDE_SUPPORT_
extern DDEHandler* ddeHandler;
#endif

extern HANDLE g_no_dde_mutex;
extern OpString8 g_stopping_signature;
extern OUIMenuManager* g_windows_menu_manager;
extern NullAppMenuListener* g_null_menu_listener;

WindowsDesktopGlobalApplication::WindowsDesktopGlobalApplication() : m_taskbar(NULL)
{
}

WindowsDesktopGlobalApplication::~WindowsDesktopGlobalApplication()
{
}

void WindowsDesktopGlobalApplication::OnStart()
{
	if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall))
		m_taskbar = new WindowsOpTaskbar();
}

void WindowsDesktopGlobalApplication::ExitStarted()
{
	if(m_taskbar)
	{
		m_taskbar->OnExitStarted();
	}
	if(!g_no_dde_mutex)
	{
		// might be created by the message loop OnExitStarted() already
		g_no_dde_mutex = CreateMutexA(NULL, TRUE, g_stopping_signature.CStr());
	}
	delete ddeHandler;
	ddeHandler = NULL;
}

void WindowsDesktopGlobalApplication::Exit()
{
	delete m_taskbar;
	m_taskbar = NULL;

	PostMessage(g_main_hwnd, WM_CLOSE, 0, 0);
}

void WindowsDesktopGlobalApplication::OnHide()
{
	if(m_taskbar)
	{
		m_taskbar->AppHidden();
	}
}

void WindowsDesktopGlobalApplication::OnShow()
{
	if(m_taskbar)
	{
		m_taskbar->AppShown();
	}
}

// static
OP_STATUS DesktopGlobalApplication::Create(DesktopGlobalApplication** desktop_application)
{
    *desktop_application = OP_NEW(WindowsDesktopGlobalApplication, ());
    if (!*desktop_application)
        return OpStatus::ERR_NO_MEMORY;
    return OpStatus::OK;
}
  

// static  // OUIMenuManager / NullAppMenuListener
OP_STATUS DesktopPopupMenuHandler::Create(DesktopPopupMenuHandler** menu_handler)
{
    if(!KioskManager::GetInstance()->GetNoMenu())
    {
	    g_windows_menu_manager = OP_NEW(OUIMenuManager, ());
	    *menu_handler = g_windows_menu_manager;
        if (!*menu_handler)
            return OpStatus::ERR_NO_MEMORY;
        return OpStatus::OK;    
    }
    else
    {
	    g_null_menu_listener = OP_NEW(NullAppMenuListener, ());
	    *menu_handler = g_null_menu_listener;
        if (!*menu_handler)
            return OpStatus::ERR_NO_MEMORY;
        return OpStatus::OK;
     }
}

