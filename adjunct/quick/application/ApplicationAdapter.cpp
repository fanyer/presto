/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick/application/ApplicationAdapter.h"

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"


ApplicationAdapter::ApplicationAdapter(NonBrowserApplication* adaptee)
   	: m_application(adaptee), m_is_showing_opera(TRUE)
{
}

ApplicationAdapter::~ApplicationAdapter()
{
	OP_DELETE(m_application);
	m_application = NULL;
}

BOOL ApplicationAdapter::OpenURL(const OpStringC &address,
		BOOL3, BOOL3, BOOL3, URL_CONTEXT_ID, BOOL, BOOL, BOOL)
{
	return m_application->OpenURL(address);
}

BOOL ApplicationAdapter::OpenURL(OpenURLSetting& setting)
{
	return m_application->OpenURL(setting.m_address);
}

BOOL ApplicationAdapter::ShowOpera(BOOL show, BOOL force_activate)
{
	if (show && m_is_showing_opera && force_activate)
	{
		return g_input_manager->InvokeAction(OpInputAction::ACTION_ACTIVATE_WINDOW);
	}

	if (show == m_is_showing_opera)
		return FALSE;

	// If there is no system tray available, we can't hide, since
	// the user wouldn't be able to open it again. Minimize instead.
	if (!show && !g_desktop_op_system_info->HasSystemTray())
		m_window_collection.MinimizeAll();
	else
		m_window_collection.SetWindowsVisible(show != FALSE);

	m_is_showing_opera = show;

	if (show)
		g_desktop_global_application->OnShow();
	else
		g_desktop_global_application->OnHide();

	return FALSE;
}
