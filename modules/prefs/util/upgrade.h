/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined PREFS_UPGRADE_H && defined UPGRADE_SUPPORT && defined PREFS_WRITE
#define PREFS_UPGRADE_H

class OpFileDescriptor;

/**
 * Helper class to migrate preferences from previous versions.
 * Please note that parts of the upgrade support resides inside the different
 * OpPrefsCollection subclasses, this code handles more complex upgrades.
 */
class PrefsUpgrade
{
public:
	/**
	 * Upgrade preferences from a previous version.
	 *
	 * @param reader The PrefsFile object used to get data.
	 * @param lastversion Version that upgrade was last run from.
	 * @return Upgraded version, or -1 on failure.
	 */
	static int Upgrade(PrefsFile *reader, int lastversion);

private:
#ifdef PREFS_UPGRADE_UAINI
	static void UpgradeUAINIL(OpFileDescriptor *);
#endif
#ifdef PREFS_UPGRADE_FROM_CORE1
	static void UpgradeCore1L(PrefsFile *);
	static int ReadMinutesHoursDaysValueL(PrefsFile *,
	                                      const char *, const char *,
	                                      const char *, const char *,
	                                      int);
#endif
#ifdef PREFS_UPGRADE_UNITE
	static void UpgradeUniteL(PrefsFile *);
#endif

#ifdef PREFS_UPGRADE_LIBSSL_VERSIONS
	static void UpgradeSSLVersionsL(PrefsFile *reader);
#endif

#ifdef URL_PER_SITE_PROXY_POLICY
	static void UpgradeNoProxyServersL();
#endif

	static void UpgradeDoubleNegativesL(PrefsFile *);

#ifdef SELFTEST
	friend class ST_prefsutilupgrade;
#endif
};

/** Current version code for preference upgrades.
 *
 * Defined version codes:
 *  - 0 = undefined previous version (core-1 and earlier).
 *  - 1 = unused
 *  - 2 = core-2 (smurf 1)
 *  - 3 = core-2.6 (default settings for Unite have changed)
 *  - 4 = ssl versions was previously wrongly written to disk.
 *  - 5 = convert No Proxy Servers into site preference(Enable Proxy)
 *  - 6 = rephrase certain negative and double-negative non-boolean prefs
 */
#define PREFSUPGRADE_CURRENT 6

#endif // PREFS_UPGRADE_H
