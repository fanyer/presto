/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"
#include "x11_global_desktop_application.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/LaunchManager.h"

#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_colorpickermanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_ipcmanager.h"
#include "platforms/unix/base/x11/x11_opclipboard.h"
#include "platforms/unix/base/x11/x11_opdragmanager.h"
#include "platforms/unix/base/x11/x11_dropmanager.h"
#include "platforms/unix/base/x11/x11_optaskbar.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_sessionmanager.h"
#include "platforms/unix/base/x11/x11_settingsclient.h"
#include "platforms/unix/base/x11/x11_systemtrayicon.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/product/x11quick/panningmanager.h"
#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "platforms/x11api/utils/unix_process.h"
#include "platforms/posix/posix_native_util.h"
#endif // WIDGET_RUNTIME_SUPPORT

void SetCrashlogRestart(BOOL state); // quick.cpp - Improve this after Alexey's package task


X11DesktopGlobalApplication::X11DesktopGlobalApplication()
	:m_tray_icon(0)
	,m_taskbar(0)
	,m_running(FALSE)
{
}


X11DesktopGlobalApplication::~X11DesktopGlobalApplication()
{
}

void X11DesktopGlobalApplication::OnStart()
{
	// We can live without the session manager. Do not return if it fails
	X11SessionManager::Create();
	X11SessionManager::AddX11SessionManagerListener(this);
	RETURN_VOID_IF_ERROR(X11IPCManager::Create());
	RETURN_VOID_IF_ERROR(PanningManager::Create());
	RETURN_VOID_IF_ERROR(X11DropManager::Create());
	

	BOOL show_tray_icon = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowTrayIcon) 
#ifdef WIDGET_RUNTIME_SUPPORT
		&& GadgetStartup::IsBrowserStartupRequested();
#endif
	;

	if (show_tray_icon)
	{
		RETURN_VOID_IF_ERROR(X11OpTaskbar::Create(&m_taskbar));
		RETURN_VOID_IF_ERROR(X11SystemTrayIcon::Create(&m_tray_icon));
		m_taskbar->AddX11OpTaskbarListener(m_tray_icon);
		m_taskbar->AddX11OpTaskbarListener(this);
	}

	g_application->AddSettingsListener(this);
	UpdateColorScheme();

#ifdef WIDGET_RUNTIME_SUPPORT
	if (GadgetStartup::IsGadgetStartupRequested())
	{
		OpString conf_xml_path;
		RETURN_VOID_IF_ERROR(GadgetStartup::GetRequestedGadgetFilePath(conf_xml_path));
		
		OpString8 gadget_name;
		RETURN_VOID_IF_ERROR(GadgetUtils::GetGadgetName(conf_xml_path, gadget_name));
		RETURN_VOID_IF_ERROR(UnixProcess::SetCallingProcessName(gadget_name)); 
	}
#endif // WIDGET_RUNTIME_SUPPORT

	g_xsettings_client->UpdateState();

	m_running = TRUE;
}


void X11DesktopGlobalApplication::ExitStarted()
{
	m_running = FALSE;

	g_x11->GetWidgetList()->OnApplicationExit();

	X11OpClipboard::s_x11_op_clipboard->OnExit();

	X11SessionManager::Destroy();

	X11IPCManager::Destroy();

	PanningManager::Destroy();

	X11DropManager::Destroy();

	if (m_taskbar)
	{
		m_taskbar->RemoveX11OpTaskbarListener(this);
		if (m_tray_icon)
		{
			m_taskbar->RemoveX11OpTaskbarListener(m_tray_icon);
		}
	}

	OP_DELETE(m_taskbar);
	m_taskbar = 0;

	OP_DELETE(m_tray_icon);
	m_tray_icon = 0;

	g_application->RemoveSettingsListener(this);
}



BOOL X11DesktopGlobalApplication::OnConfirmExit(BOOL exit)
{
	X11SessionManager* sm = X11SessionManager::Self();
	if (sm && sm->IsShutdown())
	{
		switch(exit)
		{
		case FALSE:
			sm->Cancel();
			break;

		case TRUE:
			sm->Accept();
			return TRUE;
			break;
		}
	}

	return FALSE;
}


void X11DesktopGlobalApplication::Exit()
{
	LaunchManager::Destroy();
	g_toolkit_library->OnOperaShutdown();
	X11OpMessageLoop::Self()->StopAll();
}


BOOL X11DesktopGlobalApplication::HandleEvent(XEvent* e)
{
	if (m_running)
	{
		if (g_x11->HandleEvent(e))
			return TRUE;
		if (PopupMenu::FilterEvent(e) )
			return TRUE;
		if (m_tray_icon && m_tray_icon->HandleSystemEvent(e))
			return TRUE;
		if (g_drop_manager->HandleEvent(e))
			return TRUE;
		if (g_x11_op_dragmanager->HandleEvent(e))
			return TRUE;
		if (g_panning_manager->HandleEvent(e))
			return TRUE;
		if (g_ipc_manager->HandleEvent(e))
			return TRUE;
		if (X11OpClipboard::s_x11_op_clipboard->HandleEvent(e))
			return TRUE;
		if (g_color_picker_manager && g_color_picker_manager->HandleEvent(e))
			return TRUE;
		if (g_xsettings_client->HandleEvent(e))
			return TRUE;
	}

	switch( e->type )
	{
		case LeaveNotify:
			// Never show a tooltip if mouse is outside an opera window

			// We test for NotifyNonlinear to prevent hiding a tooltip that opens 
			// under the mouse (that generates a leave event to the window under the tooltip)
			// 2 We test for NotifyGrab to let popup based tooltips (group thumbnails) stay open
			if (e->xcrossing.detail != NotifyNonlinear && e->xcrossing.mode != NotifyGrab)
				g_application->SetToolTipListener(0);
		break;

		case ClientMessage:
		{
			if (e->xclient.format == 32 && e->xclient.message_type == ATOMIZE(WM_PROTOCOLS)) 
			{
				X11Types::Atom atom = e->xclient.data.l[0];
				if (atom == ATOMIZE(_NET_WM_PING))
				{
					X11Types::Window root = RootWindowOfScreen(DefaultScreenOfDisplay(e->xclient.display));
					if (e->xclient.window != root) 
					{
						e->xclient.window = root;
						XSendEvent(e->xclient.display, e->xclient.window,False, SubstructureNotifyMask|SubstructureRedirectMask, e);
					}
					return TRUE;
				}
			}
			break;
		}

		case FocusOut:
			g_application->SetToolTipListener(NULL); // hide tooltip on focus out (see DSK-372345)
			break;

		case KeyPress:
			g_application->SetToolTipListener(NULL); // hide tooltip on keypress (see DSK-372345)
			// no break

		case KeyRelease:
		{
			for (OpListenersIterator iterator(m_global_keyevent_listener); m_global_keyevent_listener.HasNext(iterator);)
			{
				bool eaten = m_global_keyevent_listener.GetNext(iterator)->OnGlobalKeyEvent(e);
				if( eaten )
				{
					return TRUE;
				}
			}
			break;
		}
	}

	return FALSE;
}


BOOL X11DesktopGlobalApplication::BlockInputEvent(XEvent* event, X11Widget* widget)
{
	switch (event->type)
	{
		case MotionNotify:
		case ButtonPress:
		case ButtonRelease:
		case KeyPress:
		case KeyRelease:
		{
			// Filter for events when we have a modal widget of some sort
			X11Widget* modal_widget = X11Widget::GetModalWidget();
			if (modal_widget)
			{
				if (!widget)
					widget = g_x11->GetWidgetList()->GetGrabbedWidget();
				if (!widget)
					widget = g_x11->GetWidgetList()->FindWidget(event->xany.window);
				if (widget)
				{
					bool popup = false;
					X11Widget* p = widget;
					while (p)
					{
						if (p->GetWindowFlags() & X11Widget::POPUP)
						{
							popup = true;
							break;
						}
						p = p->Parent();
					}
					
					if (!popup)
					{
						// Give event to widget if it is a child of the modal widget
						if (!modal_widget->Contains(widget))
						{
							// The widget is not a child of the modal widget. Give the
							// event to it if it belongs to another toplevel window group

							// Note: We have no support for application modality so far.
							// If needed, we just have to add a flag in the X11Widget
							// class and test for it on the 'modal_widget' here.
							
							if (widget->GetTopLevel() == modal_widget->GetTopLevel())
							{
								// Hook for special cases. All widgets will by default not
								// ignore modality so this test will match for those

								if (!widget->IgnoreModality(event))
								{
									return TRUE;
								}
							}
						}
					}
				}
			}			
		}
		break;
	}

	return FALSE;
}


void X11DesktopGlobalApplication::OnUnattendedChatCount( OpWindow* op_window, UINT32 count )
{
	X11Widget* widget = ((X11OpWindow*)op_window)->GetOuterWidget();
	if( widget )
	{
		while( widget->Parent() )
			widget = widget->Parent();
	}
	if( widget )
	{
		widget->SetWindowUrgency(count > 0);
	}
}

void X11DesktopGlobalApplication::OnSaveYourself(X11SessionManager& sm)
{
	if (sm.IsShutdown())
	{
		SetCrashlogRestart(FALSE);

		INT32 exit_strategy = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowExitDialog);
		if (exit_strategy == ExitStrategyConfirm)
		{
			BOOL can_ask_user = sm.AllowUserInteraction(X11SessionManager::InteractNormal);
			if (can_ask_user)
			{
				// Exit(FALSE) will show the exit dialog. The outcome from that dialog
				// gets trapped in X11DesktopGlobalApplication::SessionManagerShutdown()
				// which will notifty the system session manager with Accept()/Cancel()
				// On Accept() the system session manager will call 
				// X11SessionManager::OnDie() where we call Exit(TRUE)
				g_application->Exit(FALSE);
				return;
			}
		}
		sm.Accept();
	}
	else
	{
		g_session_auto_save_manager->SaveCurrentSession(NULL, FALSE, TRUE, FALSE);
		// Accept() in this case will not trigger X11SessionManager::OnDie() because the 
		// system session manager knows IsShutdown() is FALSE
		sm.Accept();
	}
}


void X11DesktopGlobalApplication::OnHide() 
{
	g_x11->GetWidgetList()->OnApplicationShow(false);
}


void X11DesktopGlobalApplication::OnShow() 
{
	g_x11->GetWidgetList()->OnApplicationShow(true);
}


void X11DesktopGlobalApplication::OnShutdownCancelled()
{
	SetCrashlogRestart(TRUE);
}



void X11DesktopGlobalApplication::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_COLOR_SCHEME) || settings->IsChanged(SETTINGS_CUSTOMIZE_END_CANCEL))
		UpdateColorScheme();
}


void X11DesktopGlobalApplication::UpdateColorScheme()
{
	uint32_t colorize_color = 0;
	uint32_t mode = g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode, (const uni_char*)0);
	switch(mode)
	{
		case 1:
		break;

		case 2:
		{
			uint32_t c  = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_SKIN, (const uni_char*)0);
			colorize_color = 0x7F000000 | ((c & 0xFF) << 16) | (c & 0xFF00) | ((c & 0xFF0000) >> 16);
		}
		break;

		default:
			mode = 0;
		break;
	}
	g_toolkit_library->SetColorScheme(mode, colorize_color);

}
