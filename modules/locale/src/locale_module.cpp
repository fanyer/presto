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

#include "modules/locale/locale_module.h"
#include "modules/locale/oplanguagemanager.h"
#ifdef LANGUAGE_FILE_SUPPORT
# include "modules/locale/src/opprefsfilelanguagemanager.h"
#endif
#ifdef LOCALE_BINARY_LANGUAGE_FILE
# include "modules/locale/src/opbinaryfilelanguagemanager.h"
#endif
#ifdef USE_DUMMY_LANGUAGEMANAGER
# include "modules/locale/src/opdummylanguagemanager.h"
#endif
#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE
# include "modules/prefs/prefsmanager/collections/pc_files.h"
#endif

void LocaleModule::InitL(const OperaInitInfo &)
{
	// Initialize the localization support. If the implementation we are
	// using is one of our own, we do the initialization here. If not,
	// we let the platform layer set things up for us manually.

#ifndef LOCALE_SELFCONTAINED
	// We need to have a language manager at this point. If your platform
	// is using its own implementation, you will need to call
	// g_opera->locale_module.SetLanguageManager() before calling
	// g_opera->InitL().
	OP_ASSERT(m_language_manager);

#elif defined LANGUAGE_FILE_SUPPORT
	// Create the language manager and have it load the language file.
	OpStackAutoPtr<OpPrefsFileLanguageManager>
		new_language_manager(OP_NEW_L(OpPrefsFileLanguageManager, ()));
	new_language_manager->LoadL();
# ifdef LANGUAGEMANAGER_CAN_RELOAD
	g_pcfiles->RegisterFilesListenerL(new_language_manager.get());
# endif

	m_language_manager = new_language_manager.release();

#elif defined LOCALE_BINARY_LANGUAGE_FILE
	// Create the language manager and have it load the language file.
	OpStackAutoPtr<OpFile> language_file(OP_NEW_L(OpFile, ()));
	g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, *language_file.get());
	OpStackAutoPtr<OpBinaryFileLanguageManager>
		new_language_manager(OP_NEW_L(OpBinaryFileLanguageManager, ()));
	new_language_manager->LoadTranslationL(language_file.get());

	m_language_manager = new_language_manager.release();

#elif defined USE_DUMMY_LANGUAGEMANAGER
	// Use a dummy language manager that will just return empty strings for
	// everything.

	m_language_manager = OP_NEW_L(OpDummyLanguageManager, ());
#endif
}

void LocaleModule::Destroy()
{
#ifdef LANGUAGEMANAGER_CAN_RELOAD
	if (g_pcfiles)
		g_pcfiles->UnregisterFilesListener(static_cast<OpPrefsFileLanguageManager *>(m_language_manager));
#endif

	delete m_language_manager;
	m_language_manager = NULL;
}
