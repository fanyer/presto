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

#ifdef PREFS_DOWNLOAD
#include "modules/prefsloader/prefsloader_module.h"
#include "modules/prefsloader/prefsloadmanager.h"

PrefsloaderModule::PrefsloaderModule()
	: m_prefs_load_manager(NULL)
{
}

void PrefsloaderModule::InitL(const OperaInitInfo &/*info*/)
{
	// Create the PrefsLoadManager
	m_prefs_load_manager = OP_NEW_L(class PrefsLoadManager, ());
}

void PrefsloaderModule::Destroy()
{
	if (m_prefs_load_manager)
	{
		// Clear any open connections
		m_prefs_load_manager->Cleanup();

		// Clean up
		OP_DELETE(m_prefs_load_manager);
		m_prefs_load_manager = NULL;
	}
}
#endif
