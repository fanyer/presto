/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/BrowserUiWindowListener.h"

#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/widgets/OpPagebar.h"

#include "adjunct/quick/extensions/TabAPIListener.h"

OP_STATUS BrowserUiWindowListener::CreateUiWindow(OpWindowCommander* windowCommander,
											OpWindowCommander* opener,
											UINT32 width,
											UINT32 height,
											UINT32 flags)
{
	// Fix for DSK-360921 - Small windows can be used to trick users with opera:config
	// To prevent small popup windows, window size cannot be below minimum dimensions.
	// DSK-362958 - When width and height are set to 0 then tab will be maximized later.
	if (!(width == 0 && height == 0))
	{
		width = MAX(width, DocumentDesktopWindowConsts::MINIMUM_WINDOW_WIDTH);
		height = MAX(height, DocumentDesktopWindowConsts::MINIMUM_WINDOW_HEIGHT);
	}

	// Fix for bug 172168, small popup windows will not have a readable
	// address field, so we prefer to turn off the address bar and instead
	// rely on the servername button that opens the address bar when clicked
	
	BOOL toolbars = ((flags & CREATEFLAG_TOOLBARS) == CREATEFLAG_TOOLBARS);
	
	if (width && width < 400)
	{
		toolbars = FALSE;
	}
	OpenURLSetting settings;

	// we only use the src window for this call
	settings.m_src_commander = opener;

	if(opener && opener->GetPrivacyMode())
		settings.m_is_privacy_mode = TRUE;

	CreateToplevelWindow(windowCommander, settings, width, height, toolbars, ((flags & CREATEFLAG_FOCUS_DOCUMENT) == CREATEFLAG_FOCUS_DOCUMENT), 
																			 ((flags & CREATEFLAG_OPEN_BACKGROUND) == CREATEFLAG_OPEN_BACKGROUND));

	g_session_auto_save_manager->SaveCurrentSession();

	return OpStatus::OK;
}

void BrowserUiWindowListener::CreateToplevelWindow(OpWindowCommander* windowCommander, OpenURLSetting & settings, UINT32 width, UINT32 height, BOOL toolbars, BOOL focus_document, BOOL open_background)
{
	if ((g_application->IsSDI() ||
		static_cast<DesktopOpSystemInfo*>(g_op_system_info)->IsForceTabMode())
		&& !toolbars)
	{
		g_application->CreateDocumentDesktopWindow(NULL, windowCommander, width, height, toolbars, focus_document, open_background, TRUE, &settings, settings.m_is_privacy_mode);
	}
	else
	{
		BOOL ignore_shiftkeys = TRUE;
		g_application->GetBrowserDesktopWindow(
				g_application->IsSDI() || settings.m_new_window == YES,
				open_background, ignore_shiftkeys, NULL, windowCommander, width, height,
				toolbars, focus_document, ignore_shiftkeys, TRUE, &settings,
				settings.m_is_privacy_mode);
	}
}

OP_STATUS BrowserUiWindowListener::CreateUiWindow(OpWindowCommander* commander,
		const CreateUiWindowArgs& args)
{
	OP_ASSERT(commander != NULL);
	if (commander == NULL)
	{
		return OpStatus::ERR;
	}

	switch (args.type)
	{
		case WINDOWTYPE_EXTENSION:
			return g_desktop_extensions_manager->CreateExtensionWindow(commander, args);

		case WINDOWTYPE_REGULAR:
		{
			OpTabAPIListener::TabAPIItemId tab_id = 
				g_desktop_extensions_manager->GetTabAPIListener()->CreateNewTab(commander, args);
			if (tab_id == 0)
				return OpStatus::ERR;

			break;
		}

		default:
			OP_ASSERT(!"Unexpected window type");
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void BrowserUiWindowListener::CloseUiWindow(OpWindowCommander* windowCommander)
{
	DesktopWindowCollection &c = g_application->GetDesktopWindowCollection();
	DesktopWindow *w = c.GetDesktopWindowFromOpWindow(windowCommander->GetOpWindow());
	if (w && !w->IsClosing())
		w->Close(FALSE); // We should post a close here, see DSK-345129
}

