/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFS_MODULE_H
#define PREFS_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/prefs/init/delayed_init.h"

/** PrefsModule must be created on startup */
#define PREFS_MODULE_REQUIRED

/* Avoid platform-specific ifdefs by using them once to define which
 * collections to include.
 */
#ifdef MSWIN
# define PREFS_HAVE_MSWIN
#endif
#ifdef _MACINTOSH_
# define PREFS_HAVE_MAC
#endif
#ifdef CORE_GOGI
# define PREFS_HAVE_COREGOGI
#endif
#ifdef UNIX
# define PREFS_HAVE_UNIX
#endif

class PrefsModule : public OperaModule
{
public:
	PrefsModule();

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();

	// Access methods
	// - Preference Manager framework
	inline class PrefsManager *PrefsManager()
	{ return m_prefs_manager; }

	// - Preference collections
	inline class PrefsCollectionApp *PrefsCollectionApp()
	{ return m_pcapp; }
	inline class PrefsCollectionCore *PrefsCollectionCore()
	{ return m_pccore; }
	inline class PrefsCollectionDisplay *PrefsCollectionDisplay()
	{ return m_pcdisplay; }
	inline class PrefsCollectionDoc *PrefsCollectionDoc()
	{ return m_pcdoc; }
	inline class PrefsCollectionParsing *PrefsCollectionParsing()
	{ return m_pcparsing; }
	inline class PrefsCollectionFiles *PrefsCollectionFiles()
	{ return m_pcfiles; }
	inline class PrefsCollectionFontsAndColors *PrefsCollectionFontsAndColors()
	{ return m_pcfontscolors; }
	inline class PrefsCollectionJS *PrefsCollectionJS()
	{ return m_pcjs; }
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	inline class PrefsCollectionDatabase *PrefsCollectionDatabase()
	{ return m_pcdatabase; }
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef M2_SUPPORT
	inline class PrefsCollectionM2 *PrefsCollectionM2()
	{ return m_pcm2; }
#endif
#ifdef PREFS_HAVE_MAC
	inline class PrefsCollectionMacOS *PrefsCollectionMacOS()
	{ return m_pcmacos; }
#endif
#ifdef PREFS_HAVE_MSWIN
	inline class PrefsCollectionMSWIN *PrefsCollectionMSWIN()
	{ return m_pcmswin; }
#endif
#ifdef PREFS_HAVE_COREGOGI
	inline class PrefsCollectionCoregogi *PrefsCollectionCoregogi()
	{ return m_pccoregogi; }
#endif
	inline class PrefsCollectionNetwork *PrefsCollectionNetwork()
	{ return m_pcnet; }
#ifdef _PRINT_SUPPORT_
	inline class PrefsCollectionPrint *PrefsCollectionPrint()
	{ return m_pcprint; }
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	inline class PrefsCollectionTools *PrefsCollectionTools()
	{ return m_pctools; }
#endif
#if defined GEOLOCATION_SUPPORT
	inline class PrefsCollectionGeolocation *PrefsCollectionGeolocation()
	{ return m_pcgeolocation; }
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	inline class PrefsCollectionUI *PrefsCollectionUI()
	{ return m_pcui; }
#endif
#ifdef PREFS_HAVE_UNIX
	inline class PrefsCollectionUnix *PrefsCollectionUnix()
	{ return m_pcunix; }
#endif
#ifdef WEBSERVER_SUPPORT
	inline class PrefsCollectionWebserver *PrefsCollectionWebserver()
	{ return m_pcwebserver; }
#endif
#ifdef SUPPORT_DATA_SYNC
	inline class PrefsCollectionSync *PrefsCollectionSync()
	{ return m_pcsync; }
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	inline class PrefsCollectionOperaAccount *PrefsCollectionOperaAccount()
	{ return m_pcopera_account; }
#endif
#ifdef DOM_JIL_API_SUPPORT
	inline class PrefsCollectionJIL *PrefsCollectionJIL()
	{ return m_pcjil; }
#endif
	class OpPrefsCollection *GetFirstCollection();

#ifdef PREFS_DELAYED_INIT_NEEDED
	// - Utility objects
	class PrefsDelayedInit *m_delayed_init;
#endif

	// Internal data structures
	struct PrefsInitInfo
	{
		const OperaInitInfo *m_init;
		OpString m_user_languages;

		PrefsInitInfo(const OperaInitInfo *init) : m_init(init) {}
	};

private:
	class PrefsManager *m_prefs_manager;

	class PrefsCollectionApp *m_pcapp;
	class PrefsCollectionCore *m_pccore;
	class PrefsCollectionDisplay *m_pcdisplay;
	class PrefsCollectionDoc *m_pcdoc;
	class PrefsCollectionParsing *m_pcparsing;
	class PrefsCollectionFiles *m_pcfiles;
	class PrefsCollectionFontsAndColors *m_pcfontscolors;
	class PrefsCollectionJS *m_pcjs;
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	class PrefsCollectionDatabase *m_pcdatabase;
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef M2_SUPPORT
	class PrefsCollectionM2 *m_pcm2;
#endif
#ifdef PREFS_HAVE_MAC
	class PrefsCollectionMacOS *m_pcmacos;
#endif
#ifdef PREFS_HAVE_MSWIN
	class PrefsCollectionMSWIN *m_pcmswin;
#endif
#ifdef PREFS_HAVE_COREGOGI
	class PrefsCollectionCoregogi *m_pccoregogi;
#endif
	class PrefsCollectionNetwork *m_pcnet;
#ifdef _PRINT_SUPPORT_
	class PrefsCollectionPrint *m_pcprint;
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	class PrefsCollectionTools *m_pctools;
#endif
#if defined GEOLOCATION_SUPPORT
	class PrefsCollectionGeolocation *m_pcgeolocation;
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	class PrefsCollectionUI *m_pcui;
#endif
#ifdef PREFS_HAVE_UNIX
	class PrefsCollectionUnix *m_pcunix;
#endif
#ifdef WEBSERVER_SUPPORT
	class PrefsCollectionWebserver *m_pcwebserver;
#endif
#ifdef SUPPORT_DATA_SYNC
	class PrefsCollectionSync *m_pcsync;
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	class PrefsCollectionOperaAccount *m_pcopera_account;
#endif
#ifdef DOM_JIL_API_SUPPORT
	class PrefsCollectionJIL *m_pcjil;
#endif

	Head *m_collections; ///< List of preference collections

	// Friend declare so that these can configure themselves:
	friend class PrefsCollectionApp;
	friend class PrefsCollectionCore;
	friend class PrefsCollectionDisplay;
	friend class PrefsCollectionDoc;
	friend class PrefsCollectionParsing;
	friend class PrefsCollectionFiles;
	friend class PrefsCollectionFontsAndColors;
	friend class PrefsCollectionJS;
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	friend class PrefsCollectionDatabase;
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef M2_SUPPORT
	friend class PrefsCollectionM2;
#endif
#ifdef PREFS_HAVE_MAC
	friend class PrefsCollectionMacOS;
#endif
#ifdef PREFS_HAVE_MSWIN
	friend class PrefsCollectionMSWIN;
#endif
#ifdef PREFS_HAVE_COREGOGI
	friend class PrefsCollectionCoregogi;
#endif
	friend class PrefsCollectionNetwork;
#ifdef _PRINT_SUPPORT_
	friend class PrefsCollectionPrint;
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	friend class PrefsCollectionTools;
#endif
#if defined GEOLOCATION_SUPPORT
	friend class PrefsCollectionGeolocation;
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	friend class PrefsCollectionUI;
#endif
#ifdef PREFS_HAVE_UNIX
	friend class PrefsCollectionUnix;
#endif
#ifdef WEBSERVER_SUPPORT
	friend class PrefsCollectionWebserver;
#endif
#ifdef SUPPORT_DATA_SYNC
	friend class PrefsCollectionSync;
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	friend class PrefsCollectionOperaAccount;
#endif
#ifdef DOM_JIL_API_SUPPORT
	friend class PrefsCollectionJIL;
#endif
	// Friend declare to access m_collections:
	friend class PrefsManager;

	// Friend declare to access itself:
	friend class PrefsDelayedInit;

	// Friend declare to hack around non-HAS_COMPLEX_GLOBALS limitations
	friend class OpPrefsCollection;
};

#endif // !PREFS_MODULE_H
