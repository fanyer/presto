/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// File name:          winutils.cpp
// Contents:           Functions working with windows
 
#include "core/pch.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/winutils.h"
#include "modules/util/opfile/opfile.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/widgets/WidgetContainer.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"

#ifdef QUICK
# include "adjunct/quick/dialogs/HomepageDialog.h"
# include "adjunct/quick/Application.h"
#endif

#include "modules/util/str.h"

OP_STATUS GetWindowHome(Window* window, BOOL always, BOOL check_if_expired)
{
	OpString temp;
	OpStringC home_url;

	BOOL homeable = window && window->GetState() == NOT_BUSY && window->HasFeature(WIN_FEATURE_HOMEABLE);

	if (homeable)
	{
		RETURN_IF_MEMORY_ERROR(window->GetHomePage(temp));
		home_url = temp.CStr();
	}

	if (!IsStr(home_url.CStr()))
	{
		home_url = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);
	}

	if (homeable && !always && IsStr(home_url.CStr()))
	{
		URL url = window->GetCurrentURL();
		OpString s;
		OP_STATUS ret = url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, 0, s);
		if (OpStatus::IsSuccess(ret) && s.Compare(home_url) == 0)
		{
			return OpStatus::OK;
		}
	}

    if (IsStr(home_url.CStr()))
    {
        if (!homeable)
        {
            BOOL3 open_in_new_window = MAYBE;
            BOOL3 open_in_background = MAYBE;
            window = g_windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);
        }
		if (window)
			RETURN_IF_ERROR(window->OpenURL(home_url.CStr(), check_if_expired, TRUE));
    }
    else
    {
#ifdef QUICK
		HomepageDialog* dialog = OP_NEW(HomepageDialog, ());
		RETURN_OOM_IF_NULL(dialog);
		dialog->Init(g_application->GetActiveDesktopWindow(), window);
#else // QUICK
		return OpStatus::ERR;
#endif // QUICK
    }

	return OpStatus::OK;
}

#ifdef _ISHORTCUT_SUPPORT
OP_STATUS ReadParseFollowInternetShortcutL(uni_char* filename, Window* curWin)
{
# ifndef PREFS_HAS_INI
#  error "INI support must be enabled"
# endif

	OpStackAutoPtr<PrefsFile> pfIS(OP_NEW_L(PrefsFile, (PREFS_INI)));
	OpFile file; ANCHOR(OpFile, file);
	LEAVE_IF_ERROR(file.Construct(filename));
	pfIS->SetFileL(&file);

	pfIS->ConstructL();
	OpStatus::Ignore(pfIS->LoadAllL()); // safe (if loading failes, IsSection will also fail)
	if(pfIS->IsSection("InternetShortcut") && curWin)
	{
		OpStringC url;
		url=pfIS->ReadStringL(UNI_L("InternetShortcut"), UNI_L("URL"));
		return curWin->OpenURL(url.CStr());
	}
	return OpStatus::ERR;
}
#endif // _ISHORTCUT_SUPPORT
