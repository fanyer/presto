/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/prefs/prefsmanager/prefsnotifier.h"

#ifdef PREFS_NOTIFIER
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#ifdef DYNAMIC_PROXY_UPDATE
void PrefsNotifier::OnProxyChangedL()
{
# ifdef PREFS_READ
	// Load the preference file, since we need to check with it to see
	// if the user has overridden the default settings.
	PrefsFile *reader = const_cast<PrefsFile *>(g_prefsManager->GetReader());
	reader->LoadAllL();
# endif

	// Read proxy changes. Will broadcast changes if needed.
	g_pcnet->ProxyChangedL();

# ifdef PREFS_READ
	// Unload when we are done.
	reader->Flush();
# endif
}
#endif

#ifdef PREFS_HAVE_REREAD_FONTS
void PrefsNotifier::OnFontChangedL()
{
# ifdef PREFS_READ
	// Load the preference file, since we need to check with it to see
	// if the user has overridden the default settings.
	PrefsFile *reader = const_cast<PrefsFile *>(g_prefsManager->GetReader());
	reader->LoadAllL();
# endif

	// Read proxy changes. Will broadcast changes if needed.
	g_pcfontscolors->FontChangedL();

# ifdef PREFS_READ
	// Unload when we are done.
	reader->Flush();
# endif
}
#endif

#ifdef PREFS_HAVE_NOTIFIER_ON_COLOR_CHANGED
void PrefsNotifier::OnColorChangedL()
{
# ifdef PREFS_READ
	// Load the preference file, since we need to check with it to see
	// if the user has overridden the default settings.
	PrefsFile *reader = const_cast<PrefsFile *>(g_prefsManager->GetReader());
	reader->LoadAllL();
# endif

	// Read color changes. Will broadcast changes if needed.
	g_pcfontscolors->ColorChangedL();

# ifdef PREFS_READ
	// Unload when we are done.
	reader->Flush();
# endif
}
#endif

#endif // PREFS_NOTIFIER
