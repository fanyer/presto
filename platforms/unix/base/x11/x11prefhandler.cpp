/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

/* This is code that changes various settings in the X11 specific code.
   This is where 3 different isolated parts of the code meet - Qt, X11 and
   core. Needless to say, this leads to an extremely polluted namespace.
   That's why it's a good idea to have a separate compile unit for this.
 */

#include "x11_globals.h"
#undef CursorShape
#undef CurrentTime

#include "modules/prefs/prefsmanager/collections/pc_unix.h"

// This function must not be called before g_x11 is created!
void UpdateX11FromPrefs()
{
	g_x11->SetForcedDpi(g_pcunix->GetIntegerPref(PrefsCollectionUnix::ForceDPI));
}
