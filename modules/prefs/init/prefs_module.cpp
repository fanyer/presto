/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/prefs/prefs_module.h"
#include "modules/prefs/init/delayed_init.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/util/upgrade.h"

#include "modules/display/color.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpSystemInfo.h"

PrefsModule::PrefsModule()
	: 
#ifdef PREFS_DELAYED_INIT_NEEDED
	m_delayed_init(NULL),
#endif
	m_prefs_manager(NULL)
	, m_pcapp(NULL)
	, m_pccore(NULL)
	, m_pcdisplay(NULL)
	, m_pcdoc(NULL)
	, m_pcparsing(NULL)
	, m_pcfiles(NULL)
	, m_pcfontscolors(NULL)
	, m_pcjs(NULL)
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	, m_pcdatabase(NULL)
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef M2_SUPPORT
	, m_pcm2(NULL)
#endif
#ifdef PREFS_HAVE_MAC
	, m_pcmacos(NULL)
#endif
#ifdef PREFS_HAVE_MSWIN
	, m_pcmswin(NULL)
#endif
#ifdef PREFS_HAVE_COREGOGI
	, m_pccoregogi(NULL)
#endif
	, m_pcnet(NULL)
#ifdef _PRINT_SUPPORT_
	, m_pcprint(NULL)
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	, m_pctools(NULL)
#endif
#if defined GEOLOCATION_SUPPORT
	, m_pcgeolocation(NULL)
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	, m_pcui(NULL)
#endif
#ifdef PREFS_HAVE_UNIX
	, m_pcunix(NULL)
#endif
#ifdef WEBSERVER_SUPPORT
	, m_pcwebserver(NULL)
#endif
#ifdef SUPPORT_DATA_SYNC
	, m_pcsync(NULL)
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	, m_pcopera_account(NULL)
#endif
#ifdef DOM_JIL_API_SUPPORT
	, m_pcjil(NULL)
#endif
	, m_collections(NULL)
{
}

void PrefsModule::InitL(const OperaInitInfo &info)
{
	PrefsInitInfo prefsinfo(&info);
	LEAVE_IF_FATAL(g_op_system_info->GetUserLanguages(&prefsinfo.m_user_languages));
	OP_ASSERT(!prefsinfo.m_user_languages.IsEmpty());
	m_collections = OP_NEW_L(Head, ());

#ifdef PREFS_READ
	m_prefs_manager = OP_NEW_L(class PrefsManager, (info.prefs_reader));
#else
	m_prefs_manager = OP_NEW_L(class PrefsManager, (NULL));
#endif
	m_prefs_manager->ConstructL();
	OP_ASSERT(!m_collections->Empty()); // Initialization failed, miserably
	m_prefs_manager->ReadAllPrefsL(&prefsinfo);

#if defined UPGRADE_SUPPORT && defined PREFS_WRITE
	// Check if an upgrade is needed
	int lastseen = m_pccore->GetIntegerPref(PrefsCollectionCore::PreferenceUpgrade);
	if (lastseen < PREFSUPGRADE_CURRENT)
	{
		int newseen = PrefsUpgrade::Upgrade(info.prefs_reader, lastseen);
		if (newseen > lastseen)
		{
			m_pccore->WriteIntegerL(PrefsCollectionCore::PreferenceUpgrade, newseen);
			m_prefs_manager->CommitL();
		}
	}
#endif

	// ColorManager now need to read the color settings
	OP_ASSERT(colorManager); // Need to have display module initialized
	colorManager->ReadColors();

	// StyleManager now need to read the font settings
	OP_ASSERT(styleManager); // Need to have display module initialized
	styleManager->OnPrefsInitL();

	// Initialize MemoryManager cache sizes
	g_memory_manager->ApplyRamCacheSettings();

#ifdef PREFS_READ
	// Flush the ini file, we're done with it now
	info.prefs_reader->Flush();
#endif

#ifdef PREFS_DELAYED_INIT_NEEDED
	// Set up delayed initialization. Destroys itself when done.
	m_delayed_init = OP_NEW_L(PrefsDelayedInit, ());
	m_delayed_init->StartL();
#endif
}

void PrefsModule::Destroy()
{
#ifdef PREFS_DELAYED_INIT_NEEDED
	OP_DELETE(m_delayed_init); // Should usually be dead already.
	m_delayed_init = NULL;
#endif

	OP_DELETE(m_prefs_manager);
	m_prefs_manager = NULL;

	// Make sure everything was cleaned up properly by the PrefsManager
	// destructor.
	OP_ASSERT(!m_pcapp);
	OP_ASSERT(!m_pccore);
	OP_ASSERT(!m_pcdisplay);
	OP_ASSERT(!m_pcdoc);
	OP_ASSERT(!m_pcparsing);
	OP_ASSERT(!m_pcfiles);
	OP_ASSERT(!m_pcfontscolors);
#if defined GEOLOCATION_SUPPORT
	OP_ASSERT(!m_pcgeolocation);
#endif
	OP_ASSERT(!m_pcjs);
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	OP_ASSERT(!m_pcdatabase);
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef M2_SUPPORT
	OP_ASSERT(!m_pcm2);
#endif
#ifdef PREFS_HAVE_MAC
	OP_ASSERT(!m_pcmacos);
#endif
#ifdef PREFS_HAVE_MSWIN
	OP_ASSERT(!m_pcmswin);
#endif
#ifdef PREFS_HAVE_COREGOGI
	OP_ASSERT(!m_pccoregogi);
#endif
	OP_ASSERT(!m_pcnet);
#ifdef _PRINT_SUPPORT_
	OP_ASSERT(!m_pcprint);
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	OP_ASSERT(!m_pctools);
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	OP_ASSERT(!m_pcui);
#endif
#ifdef PREFS_HAVE_UNIX
	OP_ASSERT(!m_pcunix);
#endif
#ifdef WEBSERVER_SUPPORT
	OP_ASSERT(!m_pcwebserver);
#endif
#ifdef SUPPORT_DATA_SYNC
	OP_ASSERT(!m_pcsync);
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	OP_ASSERT(!m_pcopera_account);
#endif

	OP_ASSERT(m_collections == 0 || m_collections->Empty());
	OP_DELETE(m_collections);
	m_collections = NULL;
}

OpPrefsCollection *PrefsModule::GetFirstCollection()
{
	return static_cast<OpPrefsCollection *>(m_collections->First());
}
