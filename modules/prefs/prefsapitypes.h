/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file prefsapitypes.h
 * Types used in the preference module API.
 *
 * Various typedef, enum and #define data types used in the API exposed by
 * the preferences code.
 */

#ifndef PREFSAPITYPES_H
#define PREFSAPITYPES_H

#ifdef PREFS_HOSTOVERRIDE
/** Status values used for PrefsManager::IsHostOverridden(). Can be treated as a
  * boolean value if you just want to know if there are overrides or not.
  */
enum HostOverrideStatus
{
	HostNotOverridden = 0,			///< No overrides for host.
	HostOverrideActive,				///< User-entered overrides for host, active.
	HostOverrideDisabled			///< User-entered overrides for host, disabled.
# ifdef PREFSFILE_WRITE_GLOBAL
	,HostOverrideDownloadedActive	///< Downloaded overrides for host, active.
	,HostOverrideDownloadedDisabled	///< Downloaded overrides for host, disabled.
# endif
};
#endif

#ifdef PREFS_ENUMERATE
/** Return datatype for PrefsManager::GetPreferencesL() */
struct prefssetting
{
	const char *section;		///< Name of preference section.
	const char *key;			///< Name of preference key.
	OpString value;				///< Current value.
	enum prefssettingtypes
	{
		string, integer, boolean, file, requiredfile, directory, color
	} type;						///< Type of setting.
# ifdef PREFSFILE_CASCADE
	BOOL enabled;				///< Setting not overridden in global ini.
# endif
};
#endif

#endif // PREFSAPITYPES_H
