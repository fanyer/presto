// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#include "adjunct/quick/managers/FeatureManager.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

FeatureManager::FeatureManager()
{

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

FeatureManager::~FeatureManager()
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL FeatureManager::IsEnabled(Names feature_name)
{
	// Check the feature
	switch (feature_name)
	{
#ifdef WEBSERVER_SUPPORT
		case OperaAccount:
		{
			// This is set to used when you have logged on once.
			return g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::OperaAccountUsed);

		}
#endif // WEBSERVER_SUPPORT
		case Sync:
		{
#if 0       // Take in with the new module release (prefs)
			return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncUsed);
#else
			OpString old_username;

			// Grab the old username as we only want to show the button if there is one
			// We also need to be logged on but you will have this prefs entry if you are logged
			// on so the logged on check is not required
			TRAPD(err, g_pcopera_account->GetStringPrefL(PrefsCollectionOperaAccount::Username, old_username));

			if (old_username.HasContent())
				return TRUE;
#endif

		}
		break;

	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

