/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef OPERA_CONSOLE

#include "modules/console/src/opconsoleprefshelper.h"
#include "modules/console/opconsoleengine.h"
#include "modules/console/console_module.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

void ConsoleModule::InitL(const OperaInitInfo &)
{
	m_engine = OP_NEW_L(class OpConsoleEngine, ());
	m_engine->ConstructL(g_pccore->GetIntegerPref(PrefsCollectionCore::MaxConsoleMessages));

#ifdef OPERA_CONSOLE_LOGFILE
	m_prefshelper = OP_NEW_L(OpConsolePrefsHelper, ());
	m_prefshelper->ConstructL();
#endif
}

void ConsoleModule::Destroy()
{
#ifdef OPERA_CONSOLE_LOGFILE
	OP_DELETE(m_prefshelper);
	m_prefshelper = NULL;
#endif

	OP_DELETE(m_engine);
	m_engine = NULL;
}

BOOL ConsoleModule::FreeCachedData(BOOL toplevel_context)
{
	if (toplevel_context)
	{
		// Remove all old error messages when running out of memory.
		m_engine->Clear();
		return TRUE;
	}

	return FALSE;
}

#endif // OPERA_CONSOLE
