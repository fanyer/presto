/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/QuickContextualUI.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/application/ClassicGlobalUiContext.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/util/OpLineParser.h"
#include "modules/util/opstring.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef MSWIN
#include "platforms/windows/win_handy.h"
#endif // MSWIN

static BOOL HasKeyword(const uni_char* token, const uni_char* key, char delimiter = '-')
{
	OpAutoVector<OpString> keywords;
	StringUtils::SplitString(keywords, token, delimiter);
	for (UINT32 i=0; i<keywords.GetCount(); i++)
	{
		keywords.Get(i)->Strip();
		if (keywords.Get(i)->CompareI(key) == 0)
			return TRUE;
	}
	return FALSE;
}

BOOL SkipEntry(OpLineParser& line, OpString& first_token)
{
	OpString token;
	BOOL ret = FALSE;

	while(TRUE)
	{
		line.GetNextToken(token);

		BOOL skip = TRUE, Not = TRUE; // Not is initialized later

		if( uni_stristr(token.CStr(), UNI_L("Platform")) != NULL )
		{
			Not = uni_stristr(token.CStr(), UNI_L("Platform !")) != NULL; 
			token.Delete(0, uni_strlen(UNI_L("Platform")) + (Not ? 2 : 1));
#if defined(_UNIX_DESKTOP_)
			if( HasKeyword(token.CStr(), UNI_L("Unix")) )
			{
				skip = FALSE;
			}
#elif defined(MSWIN)
			BOOL isMCE		= HasKeyword(token.CStr(), UNI_L("MCE"));
			BOOL isWindows	= HasKeyword(token.CStr(), UNI_L("Windows")) || HasKeyword(token.CStr(), UNI_L("Win2000"));
			BOOL win7		= HasKeyword(token.CStr(), UNI_L("Win7")) != NULL;

			if(isMCE && CommandLineManager::GetInstance()->GetArgument(CommandLineManager::MediaCenter))
			{
				skip = FALSE;
			}

			if (win7 && GetWinType() == WIN7)
			{
				skip = FALSE;
			}

			if(isWindows && IsSystemWin2000orXP())
			{
				skip = FALSE;
			}

#elif defined(_MACINTOSH_)
			if( HasKeyword(token.CStr(), UNI_L("Mac")) )
			{
				skip = FALSE;
			}
#endif
		}
		else if( uni_stristr(token.CStr(), UNI_L("Feature")) != NULL )
		{
			Not = uni_stristr(token.CStr(), UNI_L("Feature !")) != NULL; 
			token.Delete(0, uni_strlen(UNI_L("Feature")) + (Not ? 2 : 1));
#if defined(M2_SUPPORT)
			if( HasKeyword(token.CStr(), UNI_L("Mail")) )
			{
				if (g_application->HasMail())
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("Chat")) )
			{
				if (g_application->HasChat())
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("Feeds")) )
			{
				if (g_application->HasFeeds())
				{
					skip = FALSE;
				}
			}
#endif
			if( HasKeyword(token.CStr(), UNI_L("Tabbed")) )
			{
				if (!g_application->IsSDI())
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("SDI")) )
			{
				if (g_application->IsSDI())
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("MDI")) )
			{
				if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowWindowMenu)) // previously tagged as "MDI" in menu setups
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("Private")) )
			{
				BrowserDesktopWindow* win =	g_application->GetActiveBrowserDesktopWindow();
				if(win && win->PrivacyMode())
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("ExternalDownloadManager")) )
			{
				if (g_pcui->GetIntegerPref(PrefsCollectionUI::UseExternalDownloadManager))
				{
					skip = FALSE;
				}
			}
			if( HasKeyword(token.CStr(), UNI_L("Autoupdate")) )
			{
				skip = g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate);
			}
#ifdef WIDGET_RUNTIME_SUPPORT
			if (HasKeyword(token.CStr(), UNI_L("WidgetRuntime")))
			{
				skip = g_pcui->GetIntegerPref(PrefsCollectionUI::DisableWidgetRuntime);
			}
#endif // WIDGET_RUNTIME_SUPPORT
#ifdef WEBSERVER_SUPPORT
			if (HasKeyword(token.CStr(), UNI_L("Unite")))
			{
				skip = !g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite);
			}
#endif // WEBSERVER_SUPPORT
			if (HasKeyword(token.CStr(), UNI_L("WebHandler")) )
				skip = FALSE;
		}
		else if( uni_stristr(token.CStr(), UNI_L("Version")) != NULL )
		{
			// Here we need to do version testing later
			continue;
		}
		else if( uni_stristr(token.CStr(), UNI_L("Language")) != NULL )
		{
			Not = uni_stristr(token.CStr(), UNI_L("Language !")) != NULL; 
			token.Delete(0, uni_strlen(UNI_L("Language")) + (Not ? 2 : 1));

			skip = !HasKeyword(token.CStr(), g_languageManager->GetLanguage(), '|');
		}
		else
		{
			first_token.Set(token);
			break;
		}

		if (!Not == skip)
			ret = TRUE;	
	}

	return ret;
}
