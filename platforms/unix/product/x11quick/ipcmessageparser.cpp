/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "ipcmessageparser.h"
#include "modules/pi/OpWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "platforms/quix/commandline/quix_commandlineargumenthandler.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"
#include "platforms/unix/base/x11/x11utils.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#ifdef _DEBUG
# define DEBUG_IPC_PARSING
#endif



namespace IPCMessageParser
{
	void RaiseWindow(DesktopWindow* dw)
	{
		dw->Activate(TRUE);
		dw->GetOpWindow()->Raise();
	}

	void LowerWindow(DesktopWindow* dw)
	{
		dw->Lower();
	}

#if defined(DEBUG_IPC_PARSING)
	void DumpParsedData(const ParsedData& pd)
	{
		OpString8 tmp;
		tmp.Set(pd.address);
		printf("address:       %s\n", tmp.CStr());
		tmp.Set(pd.window_name);
		printf("window name:   %s\n", tmp.CStr());
		printf("rect:          %d %d %d %d\n", pd.window_rect.x, pd.window_rect.y, pd.window_rect.width, pd.window_rect.height);
		printf("new window:    %d\n", pd.new_window);
		printf("new tab:       %d\n", pd.new_tab);
		printf("privacy mode:  %d\n", pd.privacy_mode);
		printf("in background: %d\n", pd.in_background);
		printf("noraise:       %d\n", pd.noraise);
	}
#endif

	BOOL ParseCommand(BrowserDesktopWindow* bw, const OpString& cmd)
	{
#if defined(DEBUG_IPC_PARSING)
		OpString8 tmp;
		tmp.Set(cmd);
		printf("target: %p, cmd: %s\n", bw, tmp.CStr());
#endif

		if (!bw)
		{
			return FALSE;
		}

		ParsedData pd;

		if (cmd.Find("openURL") == 0)
		{
			if (ParseOpenUrlCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif

				if (pd.address.IsEmpty())
				{
					RaiseWindow(bw);
					g_input_manager->InvokeAction(OpInputAction::ACTION_GO_TO_PAGE);
				}
				else
				{
					/* Not all combinations of values in pd are
					 * sensible.  And only some combinations have
					 * actually been tested.  In particular, setting
					 * both new_page and new_window to TRUE may not
					 * work as expected.  And the same for setting
					 * in_background to TRUE but new_page to FALSE.
					 */

					// Set up the url opening parameters
					OpenURLSetting url_setting;
					url_setting.m_address.Set(pd.address);
					url_setting.m_new_page = MAYBE;
					url_setting.m_new_window = MAYBE;
					url_setting.m_in_background = MAYBE;
					url_setting.m_ignore_modifier_keys = TRUE;
					url_setting.m_is_privacy_mode = pd.privacy_mode;
					url_setting.m_is_remote_command = TRUE;

					if (pd.new_window)
					{
						url_setting.m_new_page = NO;
						url_setting.m_new_window = YES;
						url_setting.m_in_background = NO;
					}

					if (pd.new_tab)
						url_setting.m_new_page = YES;

					if (pd.in_background)
						url_setting.m_in_background = YES;

					if (!pd.new_window)
					{
						url_setting.m_target_window = bw;
#if 0
						/* This code sets url_setting.m_src_window.
						 * It used to happen as a side effect of
						 * calling g_application->GoToPage(), and I
						 * copied it in here when I rewrote the code
						 * to use the single-parameter OpenURL()
						 * instead.
						 *
						 * It is probably not needed.  Most likely it
						 * does more harm than good.  I expect
						 * m_src_window indicates which window
						 * initiated the loading.  It seems obvious to
						 * me that when loading is initiated by an
						 * external application, no opera window
						 * should be set as opener.
						 *
						 * I chose to ifdef out the code rather than
						 * delete it in order to more easily test if
						 * removing this code is the cause of any
						 * strange regression we might run into.
						 */
						BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
						if (browser)
						{
							DesktopWindow *active_win = browser->GetActiveDesktopWindow();
							if (active_win)
							{
								OpWindowCommander *win_com = active_win->GetWindowCommander();
								if (win_com)
								{
									// This sets setting.m_src_window to win_com's window
									WindowCommanderProxy::InitOpenURLSetting(url_setting, win_com);
								}
							}
						}
#endif
					}

					// Ok, URL opening parameters are set up, now do the work

					if (!pd.noraise && !pd.new_window)
						RaiseWindow(bw);

					if (pd.new_window)
					{
						// I suspect this block is here just to be able to set the geometry.
						Application::StartupSetting setting;
						setting.open_in_background = FALSE;
						setting.open_in_new_page = FALSE;
						setting.open_in_new_window = TRUE;
						setting.fullscreen = FALSE;
						setting.iconic = FALSE;
						setting.geometry = pd.window_rect;
						g_application->SetStartupSetting(setting);
					}
					g_application->OpenURL(url_setting);
				}
			}
		}
		else if (cmd.Find("newInstance") == 0)
		{
			if (ParseDestinationCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif
				if (pd.in_background)
				{
					if (!pd.noraise)
						RaiseWindow(bw);
					g_application->CreateDocumentDesktopWindow(bw->GetWorkspace(), NULL, 0, 0,  TRUE, FALSE, TRUE, FALSE,  NULL, pd.privacy_mode);
				}
				else if (pd.new_tab)
				{
					if (!pd.noraise)
						RaiseWindow(bw);
					g_application->CreateDocumentDesktopWindow(bw->GetWorkspace(), NULL, 0, 0,  TRUE, FALSE, FALSE, FALSE,  NULL, pd.privacy_mode);
				}
				else if (pd.new_window)
				{
					Application::StartupSetting setting;
					setting.open_in_background = FALSE;
					setting.open_in_new_page = FALSE;
					setting.open_in_new_window = TRUE;
					setting.fullscreen = FALSE;
					setting.iconic = FALSE;
					setting.geometry = pd.window_rect;
					g_application->SetStartupSetting(setting);
					g_application->GetBrowserDesktopWindow(TRUE,FALSE,TRUE);
				}
				else
				{
					if (!pd.noraise)
						RaiseWindow(bw);
					g_application->CreateDocumentDesktopWindow(bw->GetWorkspace(), NULL, 0, 0,  TRUE, FALSE, FALSE, FALSE,  NULL, pd.privacy_mode);
				}

			}
		}
		else if (cmd.Find("openM2") == 0)
		{
			if (ParseDestinationCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif
				if (pd.new_window)
				{
					g_application->GetBrowserDesktopWindow(TRUE,FALSE,FALSE);
				}
				else if (pd.new_tab || pd.in_background)
				{
					return TRUE;
				}
				else
				{
					if (!pd.noraise)
						RaiseWindow(bw);
				}
				g_input_manager->InvokeAction(OpInputAction::ACTION_READ_MAIL);
			}
		}
		else if (cmd.Find("openComposer") == 0)
		{
			if (ParseDestinationCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif

				if (pd.new_window)
				{
					g_application->GetBrowserDesktopWindow(TRUE,FALSE,FALSE);
				}
				else if (pd.new_tab || pd.in_background)
				{
					return TRUE;
				}
				else
				{
					if (!pd.noraise)
						RaiseWindow(bw);
				}
				g_input_manager->InvokeAction(OpInputAction::ACTION_COMPOSE_MAIL);
			}
		}
		else if (cmd.Find("openFile") == 0)
		{
			if (ParseDestinationCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif
				if (pd.new_window)
				{
					g_application->GetBrowserDesktopWindow(TRUE,FALSE,FALSE);
				}
				else if (pd.new_tab)
				{
					if (!pd.noraise)
						RaiseWindow(bw);
					g_application->GetBrowserDesktopWindow(FALSE,FALSE,TRUE);
				}
				else if (pd.in_background)
				{
					return TRUE;
				}
				else
				{
					if (!pd.noraise)
						RaiseWindow(bw);
				}
				g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_DOCUMENT);
			}
		}
		else if (cmd.Find("addBookmark") == 0)
		{
			if (ParseOpenUrlCommand(cmd, pd))
			{
#if defined(DEBUG_IPC_PARSING)
				DumpParsedData(pd);
#endif

				RaiseWindow(bw);
				/*
				HotlistManager::ItemData item_data;

				INT32 id = -1;
				BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();
				if (bw)
				{
					id = bw->GetSelectedHotlistItemId(OpTypedObject::PANEL_TYPE_BOOKMARKS);
				}

				item_data.url.Set( address );
				item_data.name.Set( item_data.url.CStr() );
				g_hotlist_manager->NewBookmark( item_data, id, NULL, TRUE );
				*/
			}
		}
		else if (cmd.Find("version") == 0)
		{
			X11Globals::ShowOperaVersion(TRUE, TRUE, bw);
		}
		else
		{
			return ParseWindowCommand(bw,cmd);
		}
		return TRUE;
	}

	BOOL ParseWindowCommand(DesktopWindow* dw, const OpString& cmd)
	{
#if defined(DEBUG_IPC_PARSING)
		OpString8 tmp;
		tmp.Set(cmd);
		printf("window target: %p, cmd: %s\n", dw, tmp.CStr());
#endif
		if (!dw)
		{
			return FALSE;
		}

		if (cmd.Compare("raise()") == 0)
		{
			int p1 = cmd.FindFirstOf('(');
			int p2 = cmd.FindLastOf(')');
			if (p1 >= 0 && p1 < p2)
			{
				RaiseWindow(dw);
			}
		}
		else if (cmd.Compare("lower()") == 0 )
		{
			int p1 = cmd.FindFirstOf('(');
			int p2 = cmd.FindLastOf(')');
			if (p1 >= 0 && p1 < p2)
			{
				LowerWindow(dw);
			}
		}
		return TRUE;
	}

	BOOL ParseDestinationCommand(const OpString& cmd, ParsedData& pd)
	{
		int p1 = cmd.FindFirstOf('(');
		int p2 = cmd.FindLastOf(')');
		if (p1 == KNotFound || p2 == KNotFound || p1 > p2)
		{
			return FALSE;
		}

		OpString argument;
		argument.Set(cmd.SubString(p1+1), p2-p1-1);
		if (argument.IsEmpty())
		{
			return TRUE;
		}

		BOOL ok = FALSE;

		const char* commands[] = { "new-window", "new-page", "new-tab", "new-private-tab", "background-page", "background-tab", 0 };
		for (int i=0; commands[i]; i++)
		{
			int p4 = argument.Find(commands[i]);
			if (p4 != KNotFound)
			{
				pd.new_window    = i==0;
				pd.new_tab       = i==1 || i==2 || i==3 || i==4 || i==5;
				pd.privacy_mode  = i==3;
				pd.in_background = i==4 || i==5;

				if (i == 1 || i == 4)
					printf("opera: 'new-page' and 'background-page' commands are deprecated and will stop working in a future version of opera.  Use 'new-tab' or 'background-tab' instead.\n");

				if (pd.new_window)
				{
					int p5 = argument.Find(UNI_L(",N="), p4);
					if (p5 != KNotFound)
					{
						int p7 = argument.FindFirstOf(UNI_L(","), p5+3);
						if( p7 == KNotFound)
							p7 = argument.Length();

						pd.window_name.Set(argument.SubString(p5+3),p7-p5-3);
						pd.window_name.Strip();
					}
					int p6 = argument.Find(UNI_L(",G="), p4);
					if (p6 != KNotFound)
					{
						int p7 = argument.FindFirstOf(UNI_L(","), p6+3);
						if (p7 == KNotFound)
							p7 = argument.Length();

						OpString8 tmp;
						tmp.Set(argument.SubString(p6+3),p7-p6-3);
						pd.window_rect = X11Utils::ParseGeometry(tmp);
					}
				}

				ok = TRUE;
				break;
			}
		}

		int p8 = argument.Find(UNI_L(",noraise"));
		if (p8 != KNotFound)
		{
			pd.noraise = TRUE;
			ok = TRUE;
		}

		return ok;
	}


	BOOL ParseOpenUrlCommand(const OpString& cmd, ParsedData& pd)
	{
		int p1 = cmd.FindFirstOf('(');
		int p2 = cmd.FindLastOf(')');
		if (p1 == KNotFound && p2 == KNotFound && p1 > p2)
		{
			return FALSE;
		}

		OpString argument;
		argument.Set(cmd.SubString(p1+1), p2-p1-1);
		if( argument.IsEmpty() )
		{
			return TRUE;
		}

		// Format openURL([url][,destnation][,N=name][,G=geometry][,noraise])
		// Name and geometry have only meaning with new-window

		int p4 = KNotFound;
		const char* commands[] = { "new-window", "new-page", "new-tab", "new-private-tab", "background-page", "background-tab", 0 };
		for (int i=0; commands[i]; i++)
		{
			p4 = argument.Find(commands[i]);
			if (p4 != KNotFound)
			{
				pd.new_window    = i==0;
				pd.new_tab       = i==1 || i==2 || i==3 || i==4 || i==5;
				pd.privacy_mode  = i==3;
				pd.in_background = i==4 || i==5;

				if (i == 1 || i == 4)
					printf("opera: 'new-page' and 'background-page' commands are deprecated and will stop working in a future version of opera.  Use 'new-tab' or 'background-tab' instead.\n");

				if (pd.new_window)
				{
					int p5 = argument.Find(UNI_L(",N="), p4);
					if (p5 != KNotFound)
					{
						int p7 = argument.FindFirstOf(UNI_L(","), p5+3);
						if( p7 == KNotFound)
							p7 = argument.Length();

						pd.window_name.Set(argument.SubString(p5+3),p7-p5-3);
						pd.window_name.Strip();
					}
					int p6 = argument.Find(UNI_L(",G="), p4);
					if (p6 != KNotFound)
					{
						int p7 = argument.FindFirstOf(UNI_L(","), p6+3);
						if( p7 == KNotFound)
							p7 = argument.Length();

						OpString8 tmp;
						tmp.Set(argument.SubString(p6+3),p7-p6-3);
						pd.window_rect = X11Utils::ParseGeometry(tmp);
					}
				}
				break; // We are done
			}
		}

		int p8 = argument.Find(UNI_L(",noraise"));
		if (p8 != KNotFound)
		{
			pd.noraise = TRUE;
			if (p4 == KNotFound)
				p4 = p8;
		}

		if (p4 != KNotFound)
		{
			OpString tmp;
			tmp.Set(argument.SubString(0),p4);
			
			int p5 = tmp.FindLastOf(',');
			if (p5 == KNotFound)
			{
				p5 = p4;
			}
			
			pd.address.Set(tmp.SubString(0),p5);
			pd.address.Strip();
		}
		else
		{
			pd.address.Set(argument);
		}

		// resolve local files and convert from locale encoding
		QuixCommandLineArgumentHandler::DecodeURL(pd.address);

		return TRUE;
	}
}
