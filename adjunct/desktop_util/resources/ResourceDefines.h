/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef RESOURCE_DEFINES_H
#define RESOURCE_DEFINES_H

#include "adjunct/desktop_util/version/operaversion.h"

// Singleton class to hold the runtype data

// Redefine for a more Opera like name
#define g_run_type (&StartupType::GetInstance())

class StartupType {
public:
	enum RunType
	{
		RUNTYPE_NORMAL = 0, // Normal running with a profile
		RUNTYPE_FIRST,		// First time running Opera after a major or a minor UPGRADE
		RUNTYPE_FIRSTCLEAN, // First running after install, clean profile
		RUNTYPE_FIRST_NEW_BUILD_NUMBER,// First time running Opera after only build number change
		RUNTYPE_NOT_SET
	};

	static StartupType& GetInstance();
	
	StartupType() : m_type(RUNTYPE_NOT_SET), m_added_custom_folder(false) {}
	
	RunType m_type;
	bool m_added_custom_folder; // true if custom folder was copied into profile (DSK-331317)
	OperaVersion		m_previous_version;
};

// Run type global define
enum RunType
{
	RUNTYPE_NORMAL = 0,
	RUNTYPE_FIRST,
	RUNTYPE_FIRSTCLEAN,
	RUNTYPE_FIRST_NEW_BUILD_NUMBER,
	RUNTYPE_NOT_SET
};


#define g_region_info (&RegionInfo::GetInstance())

/**
 * Information about country and region that this computer is located in.
 * Used to setup paths to default customization files (speed dials, bookmarks,
 * search engines), as a hint for speed dial extension suggestions, etc.
 * Country and region name are also stored in operaprefs.ini, but values here
 * may be different if that file is not writable (DSK-351573).
 */
class RegionInfo
{
public:
	static RegionInfo& GetInstance();

	RegionInfo() : m_changed(false), m_from_os(false) {}

	/**
	 * Country code in ISO 3166-1 alpha-2 format. Obtained from OS or from the autoupdate
	 * server (IP-based). Users can also override it with PrefsCollectionUI::CountryCode.
	 * Special value "undefined" means that country code could not be determined.
	 */
	OpString m_country;

	/**
	 * Region of the world. In most cases the same as m_country, but some countries are
	 * mapped to special region names via region.ini file.
	 */
	OpString m_region;

	/**
	 * Default language code for m_country. This should be a name of subfolder of locale folder.
	 * Can be empty if unknown or there is no single default language for given country. 
	 */
	OpString m_default_language;

	bool m_changed; //< true if region information changed since previous session (paths to customization files may be different)

	bool m_from_os; //< true if region information was obtained from OS in this session
};


// Add defines for common filenames etc in here

#define	DESKTOP_RES_OPERA_PREFS					UNI_L("operaprefs.ini")
#define	DESKTOP_RES_OPERA_PREFS_GLOBAL			UNI_L("operaprefs_default.ini")
#define	DESKTOP_RES_OPERA_PREFS_FIXED			UNI_L("operaprefs_fixed.ini")
#define	DESKTOP_RES_OPERA_PREFS_LOCALE			UNI_L("operaprefs_locale.ini")
#define DESKTOP_RES_OPERA_PREFS_REGION			UNI_L("operaprefs_region.ini")
#define DESKTOP_RES_OPERA_PREFS_DISABLED		UNI_L("operaprefs_disabled.ini")

#define	DESKTOP_RES_STD_SPEEDDIAL				UNI_L("standard_speeddial.ini")
#define	DESKTOP_RES_STD_ACCOUNTS				UNI_L("standard_accounts.ini")

#define	DESKTOP_RES_PACKAGE_BOOKMARK			UNI_L("bookmarks.adr")

#define	DESKTOP_RES_USERPREF_SPEEDDIAL			UNI_L("speeddial.ini")

#define DESKTOP_RES_DEFAULT_LANGUAGE			UNI_L("en.lng")

#define DESKTOP_RES_PACKAGE_BROWSERJS			UNI_L("browser.js")
#define DESKTOP_RES_BROWSERJS					UNI_L("browser.js")

#define DESKTOP_RES_PACKAGE_TURBOSETTINGS		UNI_L("turbosettings.xml")

#ifdef _MACINTOSH_
#define	DESKTOP_RES_OLD_OPERA_PREFS				UNI_L("Opera 9 Preferences")
#else
#define	DESKTOP_RES_OLD_OPERA_PREFS				UNI_L("opera6.ini")
#endif

#define DESKTOP_RES_REGION_CONFIG				UNI_L("region.ini")

#define DESKTOP_RES_AU_REGION					UNI_L("autoupdate_region.dat")

#endif // RESOURCE_UTILS_H
