/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/dialogs/KioskResetDialog.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

// For clear private data
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/TransfersPanel.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"

#include "adjunct/quick/models/DesktopHistoryModel.h"

#include "modules/history/direct_history.h"
#include "modules/url/uamanager/ua.h"

#include <ctype.h>

//#define RESET_USING_EXIT


BOOL KioskManager::m_mail_enabled = TRUE;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

KioskManager *KioskManager::GetInstance()
{
	static KioskManager instance;
	return &instance;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

KioskManager::KioskManager()
{
	m_packed.settings_checked = FALSE;
	m_ticks_since_last_activety = -1;
	m_timer = 0;
	m_dialog = 0;
	SetEnableFilter(TRUE);
}


KioskManager::~KioskManager()
{
	OP_DELETE(m_timer);
}


int KioskManager::ParseFlag(const char* arg, const char* next_arg )
{
	int flags_eaten = 0;

	if( !arg )
	{
		return flags_eaten;
	}

	int length = strlen(arg);
	if( length < 1 )
	{
		return flags_eaten;
	}

	// Convert to lowercase
	char* candidate = OP_NEWA(char, length+1);
	if( !candidate )
	{
		return flags_eaten;
	}
	for( int i=0; i<length; i++ )
	{
		candidate[i] = tolower(BYTE(arg[i]));
	}
	candidate[length] = 0;


	if( candidate[0] == 'k' )
	{
		if( strcmp(candidate, "k") == 0 ||
			strcmp(candidate, "kioskmode") == 0 )
		{
			m_packed.kiosk_mode = TRUE;
			m_packed.no_splash = TRUE;
			m_packed.no_change_fullscreen = TRUE;
			m_packed.no_change_buttons = TRUE;
			m_packed.no_sysmenu = TRUE;
		}
		else if( strcmp(candidate,"kioskbuttons") == 0 )
		{
			m_packed.kiosk_buttons = TRUE;
		}
		else if( strcmp(candidate,"kioskresetstation") == 0 )
		{
			m_packed.kiosk_resetstation = TRUE;
		}
		else if( strcmp(candidate,"kioskwindows") == 0 )
		{
			m_packed.kiosk_windows = TRUE;
		}
		else if( strcmp(candidate,"kiosknormalscreen") == 0 )
		{
			m_packed.kiosk_normalscreen = TRUE;
		}
		else if( strcmp(candidate,"kioskspeeddial") == 0 )
		{
			m_packed.kiosk_speeddial = TRUE;
		}
		else
		{
			OP_DELETEA(candidate);
			return flags_eaten;
		}
	}
	else if( candidate[0] == 'n' )
	{
		if( strcmp(candidate,"nochangebuttons") == 0 )
		{
			m_packed.no_change_buttons = TRUE;
		}
		else if( strcmp(candidate,"nochangefullscreen") == 0 )
		{
			m_packed.no_change_fullscreen = TRUE;
		}
		else if( strcmp(candidate,"nochangemenu") == 0 ||
				 strcmp(candidate,"nocontextmenu") == 0 )
		{
			m_packed.no_contextmenu = TRUE;
		}
		else if( strcmp(candidate,"nodownload") == 0 )
		{
			m_packed.no_download = TRUE;
		}
		else if( strcmp(candidate,"noexit") == 0 )
		{
			m_packed.no_exit = TRUE;
		}
		else if( strcmp(candidate,"nohotlist") == 0 || strcmp(candidate,"nopanel") == 0 )
		{
			m_packed.no_hotlist = TRUE;
		}
		else if( strcmp(candidate,"nokeys") == 0 )
		{
			m_packed.no_keys = TRUE;
		}
		else if( strcmp(candidate,"nomaillinks") == 0 )
		{
			m_packed.no_maillinks = TRUE;
			m_mail_enabled = FALSE;
		}
		else if( strcmp(candidate,"nomenu") == 0 )
		{
			m_packed.no_menu = TRUE;
		}
		else if( strcmp(candidate,"noprint") == 0 )
		{
			m_packed.no_print = TRUE;
		}
		else if( strcmp(candidate,"nosave") == 0 )
		{
			m_packed.no_save = TRUE;
		}
		else if( strcmp(candidate,"nosplash") == 0 )
		{
			m_packed.no_splash = TRUE;
		}
		else if( strcmp(candidate,"nosysmenu") == 0 )
		{
			m_packed.no_sysmenu = TRUE;
		}
		else
		{
			OP_DELETEA(candidate);
			return flags_eaten;
		}
	}
	else if( strcmp(candidate,"resetonexit") == 0 )
	{
		m_packed.reset_on_exit = TRUE;
	}
	else if( strcmp(candidate,"e") == 0 )
	{
		m_packed.no_splash = TRUE;
	}
	else if( strcmp(candidate,"useragent") == 0 )
	{
		if (next_arg)
		{
			m_useragent.Set(next_arg);
			flags_eaten++;
		}
	}
	else
	{
		OP_DELETEA(candidate);
		return flags_eaten;
	}

	OP_DELETEA(candidate);
	return ++flags_eaten;
}



// static
void KioskManager::PrintCommandlineOptions()
{
#if defined(UNIX) || defined(_MACINTOSH_)
	printf("Usage: %s [kiosk mode options]\n", "opera" );

	printf("\n");
	printf("  -kioskmode\n");
	printf("  -k\n");
	printf("  -kioskbuttons\n");
	printf("  -kioskresetstation\n");
	printf("  -kioskwindows\n");
	printf("  -kiosknormalscreen (overrides -nochangefullscreen)\n");
	printf("  -kioskspeeddial\n");
	printf("  -nochangebuttons\n");
	printf("  -nochangefullscreen\n");
	printf("  -nocontextmenu\n");
	printf("  -nodownload\n");
	printf("  -noexit\n");
	printf("  -nohotlist\n");
	printf("  -nopanel (same as -nohotlist)\n");
	printf("  -nokeys\n");
	printf("  -nomaillinks\n");
	printf("  -nomenu\n");
	printf("  -noprint\n");
	printf("  -nosave\n");
	printf("  -nosplash\n");
	//printf("  -nosysmenu\n"); ////
	printf("  -resetonexit\n");

	printf("\n");
	printf("Visit http://www.opera.com/support/mastering/kiosk/ for ");
	printf("more information.\n\n");
#endif
}


void KioskManager::Check()
{
	if( !m_packed.settings_checked )
	{
		m_packed.settings_checked = TRUE;

		if( !m_packed.kiosk_mode )
		{
			m_packed.kiosk_resetstation   = FALSE; // Only in kiosk mode
			m_packed.kiosk_normalscreen   = FALSE; // Only in kiosk mode
			m_packed.reset_on_exit        = FALSE; // Only in kiosk mode
			m_packed.kiosk_windows        = FALSE; // Only in kiosk mode
			m_packed.kiosk_buttons        = FALSE; // Only in kiosk mode
		}

		if( m_packed.kiosk_mode && m_packed.kiosk_normalscreen )
		{
			m_packed.no_change_fullscreen = FALSE; // It is allowed to change in this mode
		}

		if( m_packed.kiosk_resetstation )
		{
			if( !m_timer )
			{
				if (!(m_timer = OP_NEW(OpTimer, ())))
					return;
			}
			m_timer->SetTimerListener( this );
			m_timer->Start(1000);
		}

		// In kiosk mode turn off auto updates always!
		if (m_packed.kiosk_mode)
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::CheckForNewOpera, 0);
		}

		// Useragent string is only valid if all of the following are on
		// (nokeys, noexit, nomenu, nocontextmenu, nodownload, noprint, nosave, nomaillinks)
		if (!m_packed.no_keys || !m_packed.no_exit || !m_packed.no_menu || !m_packed.no_contextmenu ||
			!m_packed.no_download || !m_packed.no_print || !m_packed.no_save || !m_packed.no_maillinks)
		{
			m_useragent.Empty();
		}

		// If the useragent still has content then add a component to the useragent string
		if (m_useragent.HasContent())
		{
			// Turn on the preference to allow extra components regardless in this case
			g_pccore->WriteIntegerL(PrefsCollectionCore::AllowComponentsInUAStringComment, TRUE);
			g_uaManager->AddComponent(m_useragent.CStr());
		}
	}
}



void KioskManager::RegisterActivity()
{
	if( m_dialog )
	{
		m_dialog->CloseDialog(FALSE);
	}
	m_ticks_since_last_activety = 0;
}


void KioskManager::OnTimeOut(OpTimer* timer)
{
	if(m_packed.kiosk_resetstation)
	{
		// The m_ticks_since_last_activety != -1 will ensure that we
		// do not reset until after the first time RegisterActivity()
		// has been called.
		if( m_ticks_since_last_activety != -1 )
		{
			m_ticks_since_last_activety ++;

			int auto_home_time =
				g_pcui->GetIntegerPref(PrefsCollectionUI::SecondsBeforeAutoHome);

			if( auto_home_time > 0 )
			{
				// The '==' will ensure that we do not reset (after another reset)
				// until after a new RegisterActivity() has been received.

				if( m_ticks_since_last_activety == auto_home_time )
				{
					if( !m_dialog )
					{
						m_dialog = OP_NEW(KioskResetDialog, ());
						if (m_dialog)
							m_dialog->Init(g_application->GetActiveDesktopWindow());
					}
				}
			}
		}
		timer->Start(1000);
	}
}


void KioskManager::OnAutoReset()
{
#if defined(RESET_USING_EXIT)
	g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
#else

	if (GetEnabled())
	{
		if (GetResetOnExit())
		{
			// Clear settings. Will close all windows
			ClearPrivateData();
		}

		// Close all windows
		BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
		if (browser_window)
		{
			browser_window->GetWorkspace()->CloseAll();
		}
	}

#endif
}


void KioskManager::OnExit()
{
	if( m_packed.kiosk_mode && m_packed.reset_on_exit )
	{
		ClearPrivateData();
	}
}


BOOL KioskManager::ActionFilter(OpInputAction* action)
{
	if( !m_packed.enable_filter )
		return FALSE;

	if( GetNoKeys() )
	{
		if( !action->IsLowlevelAction() && action->IsKeyboardInvoked() )
		{
			if( !action->IsMoveAction() )
			{
				if( action->GetAction() != OpInputAction::ACTION_FOCUS_NEXT_WIDGET &&
					action->GetAction() != OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET &&
					action->GetAction() != OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET &&
					action->GetAction() != OpInputAction::ACTION_FOCUS_PREVIOUS_RADIO_WIDGET &&
					action->GetAction() != OpInputAction::ACTION_BACKSPACE &&
					action->GetAction() != OpInputAction::ACTION_DELETE &&
					action->GetAction() != OpInputAction::ACTION_CLOSE_DROPDOWN &&
					action->GetAction() != OpInputAction::ACTION_SCROLL &&
					action->GetAction() != OpInputAction::ACTION_SCROLL_LEFT &&
					action->GetAction() != OpInputAction::ACTION_SCROLL_RIGHT &&
					action->GetAction() != OpInputAction::ACTION_SCROLL_UP &&
					action->GetAction() != OpInputAction::ACTION_SCROLL_DOWN &&
					action->GetAction() != OpInputAction::ACTION_SHOW_EDIT_DROPDOWN &&
					action->GetAction() != OpInputAction::ACTION_CLICK_DEFAULT_BUTTON &&
					!(action->GetAction() == OpInputAction::ACTION_EXIT && action->GetActionData() == 0) // allow exit to go through, is password protected if /noExit
					)
				{
					//action->Dump("KioskManager::ActionFilter");
					return TRUE;
				}
			}
		}
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
		case OpInputAction::ACTION_NEW_BROWSER_WINDOW:
#ifdef SUPPORT_DATA_SYNC
		case OpInputAction::ACTION_SYNC_LOGIN:
		case OpInputAction::ACTION_SYNC_LOGOUT:
			action->SetEnabled(!GetEnabled());
			return GetEnabled();
			break;
#endif
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_FRAME_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_FRAME_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_DUPLICATE_PAGE:
		case OpInputAction::ACTION_CREATE_LINKED_WINDOW:
		case OpInputAction::ACTION_CLOSE_WINDOW:
			if( GetEnabled() && !GetKioskWindows() )
			{
				action->SetEnabled(FALSE);
				return TRUE;
			}
			return FALSE;
			break;

		// For now until we can be sure there will always be a valid page
		case OpInputAction::ACTION_NEW_PRIVATE_PAGE:
		case OpInputAction::ACTION_NEW_PAGE:
			if( GetEnabled() && !GetKioskWindows() )
			{
				action->SetEnabled(!g_application->GetActiveDocumentDesktopWindow());
				return g_application->GetActiveDocumentDesktopWindow() != 0;
			}
			return FALSE;
			break;

		case OpInputAction::ACTION_SHOW_PRINT_OPTIONS:
		case OpInputAction::ACTION_SHOW_PRINT_SETUP:
		case OpInputAction::ACTION_PRINT_DOCUMENT:
		case OpInputAction::ACTION_PRINT_PREVIEW:
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_AS_SCREEN:
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ONE_FRAME_PER_SHEET:
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ACTIVE_FRAME:
		case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
			action->SetEnabled(!GetNoPrint());
			return GetNoPrint();
			break;

		case OpInputAction::ACTION_MANAGE_BOOKMARKS:
		case OpInputAction::ACTION_MANAGE_CONTACTS:
		case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
		case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
		case OpInputAction::ACTION_ADD_LINK_TO_CONTACTS:
		case OpInputAction::ACTION_ADD_ALL_TO_BOOKMARKS:
		case OpInputAction::ACTION_ADD_FRAME_TO_BOOKMARKS:
		case OpInputAction::ACTION_OPEN_BOOKMARKS_FILE:
		case OpInputAction::ACTION_NEW_CONTACTS_FILE:
		case OpInputAction::ACTION_OPEN_CONTACTS_FILE:
		case OpInputAction::ACTION_IMPORT_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_CONTACTS:
		case OpInputAction::ACTION_IMPORT_NETSCAPE_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_EXPLORER_FAVORITES:
		case OpInputAction::ACTION_IMPORT_KONQUEROR_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_KDE1_BOOKMARKS:
		case OpInputAction::ACTION_ADD_PANEL:
		case OpInputAction::ACTION_FOCUS_PANEL:
		case OpInputAction::ACTION_HOTLIST_ALWAYS_ON_TOP:
		case OpInputAction::ACTION_ACTIVATE_HOTLIST_WINDOW:
			action->SetEnabled(!GetNoHotlist());
			return GetNoHotlist();
			break;

		case OpInputAction::ACTION_OPEN_LINK:
			// Used in bookmark menus
			if( action->GetActionData() == 1 || action->GetActionData() == -2 )
			{
				action->SetEnabled(!GetNoHotlist());
				return GetNoHotlist();
				break;
			}
			break;

		case OpInputAction::ACTION_SAVE_SELECTED_BOOKMARKS_AS:
		case OpInputAction::ACTION_SAVE_SELECTED_CONTACTS_AS:
		case OpInputAction::ACTION_EXPORT_BOOKMARKS:
		case OpInputAction::ACTION_EXPORT_BOOKMARKS_TO_HTML:
		case OpInputAction::ACTION_EXPORT_SELECTED_BOOKMARKS_TO_HTML:
		case OpInputAction::ACTION_EXPORT_CONTACTS:
			action->SetEnabled(!GetNoHotlist() && !GetNoSave());
			return GetNoHotlist() || GetNoSave();
			break;

		case OpInputAction::ACTION_SAVE_LINK:
		case OpInputAction::ACTION_SAVE_WINDOW_SETUP:
		case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
		case OpInputAction::ACTION_SAVE_FRAME_AS:
		case OpInputAction::ACTION_SAVE_IMAGE:
		case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
		case OpInputAction::ACTION_SAVE_DOCUMENT:
			action->SetEnabled(!GetNoSave());
			return GetNoSave();
			break;

#ifdef M2_SUPPORT
		case OpInputAction::ACTION_COMPOSE_MAIL:
		case OpInputAction::ACTION_COMPOSE_TO_CONTACT:
		case OpInputAction::ACTION_COMPOSE_CC_CONTACT:
		case OpInputAction::ACTION_COMPOSE_BCC_CONTACT:
		case OpInputAction::ACTION_SEND_ADDRESS_IN_MAIL:
		case OpInputAction::ACTION_SEND_TEXT_IN_MAIL:
		case OpInputAction::ACTION_SEND_DOCUMENT_ADDRESS_IN_MAIL:
		case OpInputAction::ACTION_SEND_FRAME_ADDRESS_IN_MAIL:
			action->SetEnabled(!GetNoMailLinks());
			return GetNoMailLinks();
			break;
#endif // M2_SUPPORT

		case OpInputAction::ACTION_ENABLE_MENU_BAR:
		case OpInputAction::ACTION_DISABLE_MENU_BAR:
			action->SetEnabled(!GetNoMenu());
			return GetNoMenu();
			break;

		case OpInputAction::ACTION_DOWNLOAD_URL:
		case OpInputAction::ACTION_DOWNLOAD_URL_AS:
		case OpInputAction::ACTION_RESUME_TRANSFER:
		case OpInputAction::ACTION_RESTART_TRANSFER:
		case OpInputAction::ACTION_STOP_TRANSFER:
			action->SetEnabled(!GetNoDownload());
			return GetNoDownload();
			break;

		case OpInputAction::ACTION_LOCK_TOOLBARS:
		case OpInputAction::ACTION_UNLOCK_TOOLBARS:
		case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
		case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
		case OpInputAction::ACTION_SET_BUTTON_STYLE:
		case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
		case OpInputAction::ACTION_REMOVE:
			action->SetEnabled(!GetNoChangeButtons());
			return GetNoChangeButtons();
			break;

		case OpInputAction::ACTION_ENTER_FULLSCREEN:
		case OpInputAction::ACTION_LEAVE_FULLSCREEN:
			action->SetEnabled(!GetNoChangeFullScreen());
			return GetNoChangeFullScreen();
			break;

// NOTE: ACTION_EXIT must be available so we can ask for password in Application
//		case OpInputAction::ACTION_EXIT:
//			action->SetEnabled(!GetNoExit());
//			return GetNoExit();
//			break;
	}

	return FALSE;
}


void KioskManager::ClearPrivateData()
{
	PrivacyManager::Flags flags;

	flags.SetAll();

	PrivacyManager::GetInstance()->ClearPrivateData(flags, FALSE);
}
