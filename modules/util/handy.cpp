/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/handy.h"
#include "modules/util/str.h"
#include "modules/url/url2.h"
#include "modules/dochand/winman.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#if defined WEBSERVER_SUPPORT && defined QUICK
#include "adjunct/quick/managers/OperaAccountManager.h"
#endif // WEBSERVER_SUPPORT && QUICK

//	UpdateOfflineMode
//
//	Update/toggle offline mode and reflect the new/current setting into the
//	main menu.  Returns the new offline mode.
OP_BOOLEAN UpdateOfflineMode(BOOL toggle)
{
	OP_MEMORY_VAR BOOL offline_mode = FALSE;
	if (g_pcnet)
	{
		offline_mode = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode);
		if (toggle)
		{
			offline_mode = !offline_mode;

#ifndef PREFS_NO_WRITE
			RETURN_IF_LEAVE(g_pcnet->WriteIntegerL(PrefsCollectionNetwork::OfflineMode, offline_mode));
#endif // !PREFS_NO_WRITE

			if (offline_mode)
			{
				g_url_api->CloseAllConnections();
			}

#ifdef QUICK
			g_desktop_account_manager->SetIsOffline(offline_mode);
#endif // QUICK

			RETURN_IF_ERROR(g_windowManager->OnlineModeChanged());
		}
	}

	return offline_mode ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

BOOL UpdateOfflineModeL(BOOL toggle)
{
	OP_BOOLEAN ret_val = UpdateOfflineMode(toggle);
	LEAVE_IF_ERROR(ret_val);

	return ret_val == OpBoolean::IS_TRUE;
}
