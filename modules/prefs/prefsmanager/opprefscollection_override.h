/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef OPPREFSCOLLECTION_OVERRIDE_H
#define OPPREFSCOLLECTION_OVERRIDE_H

#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/url/url2.h" // for URL

class PrefsSection;
#ifdef PREFS_HOSTOVERRIDE
class OpOverrideHost;
class OverrideHost;
#endif

/**
 * Preference collection with host override support.
 *
 * This implementation of the OpPrefsCollection class has support for
 * overriding individual preferences based on host name. For preferences
 * that are not overridden, the standard value will be returned as normal.
 *
 * OpPrefsCollectionWithHostOverride holds a list of OverrideHost objects,
 * each object listing overrides for a particular host. Each OverrideHost
 * holds a list of HostOverrideInt and HostOverrideString objects, which
 * contain a single preference to be overridden. When a preference is
 * looked up from OpPrefsCollectionWithHostOverride the list of override
 * hosts is automatically searched (unless a NULL host is sent in).
 *
 * Changes in overrides are not relayed to the listeners.
 */
class OpPrefsCollectionWithHostOverride : public OpPrefsCollection
#ifdef PREFS_HOSTOVERRIDE
	, protected OpOverrideHostContainer
#endif
{
public:
	/** Destructor. */
	virtual ~OpPrefsCollectionWithHostOverride() {}

#ifdef PREFS_HOSTOVERRIDE
	/** Read overridden settings from the PrefsFile to internal storage.
	  * Interface implemented by sub-classes to perform the actual call to
	  * ReadOverridesInternalL(). Used by PrefsManager, called only once
	  * some time after creating the object and before the first call to
	  * any other use, once per host.
	  *
	  * @param host Host data is to be read for.
	  * @param section The preference section with overrides for the host.
	  * @param active Whether the overrides for this host are active or not.
	  * @param from_user Whether the overrides are entered by the user.
	  */
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section,
		BOOL active, BOOL from_user) = 0;

# ifdef PREFS_WRITE
	/** Remove all overrides for a named host. Used by PrefsManager, called
	  * if PrefsManager is requested to remove a host.
	  *
	  * @param host Host to delete overrides for.
	  */
	virtual void RemoveOverridesL(const uni_char *host);
# endif // PREFS_WRITE

	// Implementation of OpPrefsCollection interfaces ---
	virtual void SetOverrideReader(PrefsFile *reader);
	virtual HostOverrideStatus IsHostOverridden(const uni_char *host, BOOL exact = TRUE);
	size_t HostOverrideCount(const uni_char *host) const;

	/** Enable or disable an overridden host. The name matching is always done
	  * exact.
	  *
	  * @param host Host to check for overrides.
	  * @param active Whether to activate the host.
	  * @return TRUE if a matching host was found.
	  */
	virtual BOOL SetHostOverrideActive(const uni_char *host, BOOL active = TRUE);
#endif // PREFS_HOSTOVERRIDE

protected:
	/** Single-phase constructor.
	  *
	  * Inherited classes may have two-phase constructors if they wish.
	  *
	  * @param identity Identity of this collection (communicated by subclass).
	  * @param reader Pointer to the PrefsFile object.
	  */
	OpPrefsCollectionWithHostOverride(enum Collections identity, PrefsFile *reader)
		: OpPrefsCollection(identity, reader)
#if defined PREFS_HOSTOVERRIDE && defined PREFS_READ
		, m_overridesreader(NULL)
#endif
		{};

#ifdef PREFS_HOSTOVERRIDE
	/** Broadcast added, changed or removed host overrides to all listeners.
	  *
	  * @param hostname The host name for which the override was changed.
	  */
	virtual void BroadcastOverride(const uni_char *hostname);
#endif // PREFS_HOSTOVERRIDE

	/** Interface used by sub-classes to read integer preferences. */
	int GetIntegerPref(int which, const uni_char *host = NULL, BOOL *overridden = NULL) const;

	/** Interface used by sub-classes to read integer preferences. */
	int GetIntegerPref(int which, const uni_char *host, BOOL exact_match, BOOL *overridden = NULL) const;

	/** Interface used by sub-classes to read string preferences. */
	const OpStringC GetStringPref(int which, const uni_char *host = NULL, BOOL *overridden = NULL) const;

	/** Interface used by sub-classes to read integer preferences. */
	int GetIntegerPref(int which, const URL &host, BOOL *overridden = NULL) const;

	/** Interface used by sub-classes to read string preferences. */
	const OpStringC GetStringPref(int which, const URL &host, BOOL *overridden = NULL) const;

#ifdef PREFS_HOSTOVERRIDE
# ifdef PREFS_WRITE
	/** Interface used by sub-classes to write overridden integer preferences.
	  * Overriding a preference automatically enables the override host.
	  *
	  * @param host Host to write an override for
	  * @param pref Pointer to the entry in the integerprefdefault structure
	  *             for this preference. Used to get the strings to write to
	  *             to the INI file.
	  * @param which Index for this preference (integer version of the
	  *              enumeration used in OpPrefsCollection). Used for lookup.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS OverridePrefL(
		const uni_char *host,
		const struct integerprefdefault *pref,
		int which, int value, BOOL from_user);

	/** Interface used by sub-classes to write overridden string preferences.
	  * Overriding a preference automatically enables the override host.
	  *
	  * @param host Host to write an override for
	  * @param pref Pointer to the entry in the stringprefdefault structure
	  *             for this preference. Used to get the strings to write to
	  *             to the INI file.
	  * @param which Index for this preference (integer version of the
	  *              enumeration used in OpPrefsCollection). Used for lookup.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS OverridePrefL(
		const uni_char *host,
		const struct stringprefdefault *pref,
		int which, const OpStringC &value,
		BOOL from_user);

#  ifdef PREFS_HAVE_STRING_API
	/** Override a site specific preference using the INI name. Should only
	  * be used from PrefsManager.
	  */
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section,
	                                 const char *key, const OpStringC &value,
	                                 BOOL from_user) = 0;

	/** Override a site specific preference using the INI name. Should only
	 * be used from PrefsManager.
	 */
	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section,
								 const char *key, BOOL from_user) = 0;
#  endif // PREFS_HAVE_STRING_API

	/** Remove an existing integer override for a named host.
	 *
	 * @param host Host to delete overrides for, or NULL to match all hosts.
	 * @param pref Pointer to the entry in the integerprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param from_user TRUE if the preference is entered by the user.
	 * @return TRUE if an override was found and removed.
	 */
	virtual BOOL RemoveIntegerOverrideL(const uni_char *host,
										const struct integerprefdefault *pref,
										int which,
										BOOL from_user);

	/** Remove an existing string override for a named host.
	 *
	 * @param host Host to delete overrides for, or NULL to match all hosts.
	 * @param pref Pointer to the entry in the stringprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param from_user TRUE if the preference is entered by the user.
	 * @return TRUE if an override was found and removed.
	 */
	virtual BOOL RemoveStringOverrideL(const uni_char *host,
									   const struct stringprefdefault *pref,
									   int which,
									   BOOL from_user);
# endif // PREFS_WRITE

	/** Create an OpOverrideHostObject suitable for this collection. Usually, this
	  * creates the regular OverrideHost kind, but it is may be overridden for
	  * special collections.
	  *
	  * @param host Host name to create an object for.
	  * @param from_user Whether the overrides are entered by the user.
	  * @return The created object.
	  */
	virtual OpOverrideHost *CreateOverrideHostObjectL(const uni_char *host, BOOL from_user);

	/**
	 * Check if integer preference is overridden for this host (internal).
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsIntegerOverridden(int which, const uni_char *host) const;
	BOOL IsIntegerOverridden(int which, const URL &host) const;

	/**
	 * Check if string preference is overridden for this host (internal).
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsStringOverridden(int which, const uni_char *host) const;
	BOOL IsStringOverridden(int which, const URL &host) const;

# if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE
	BOOL OverridePreferenceInternalL(
		const uni_char *host, IniSection section, const char *key,
		const OpStringC &value, BOOL from_user,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers);

	BOOL RemoveOverrideInternalL(
		const uni_char *host, IniSection section, const char *key, BOOL from_user,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers);
# endif // PREFS_HAVE_STRING_API and PREFS_WRITE
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	BOOL GetPreferenceInternalL(IniSection section, const char *key,
		OpString &target, BOOL defval, const uni_char *host,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers);
#endif

#ifdef PREFS_HOSTOVERRIDE
	/** Read all overrides for this domain from the preferences. Called from
	  * ReadAllOverridesL() in the sub-classes.
	  *
	  * @param host Host data is to be read for.
	  * @param section The prefs section with the data.
	  * @param active Whether the overrides for this host are active or not.
	  * @param from_user Whether the overrides are entered by the user.
	  * @param intprefs Pointer to the integerprefdefault structure.
	  * @param stringprefs Pointer to the stringprefdefault structure.
	  * @return TRUE if any overrides where found.
	  */
	void ReadOverridesInternalL(
		const uni_char *host,
		PrefsSection *section,
		BOOL active, BOOL from_user,
		const struct OpPrefsCollection::integerprefdefault *intprefs,
		const struct OpPrefsCollection::stringprefdefault *stringprefs);
#endif

#if defined(PREFS_HOSTOVERRIDE) && defined(PREFS_READ)
	/**
	 * Pointer to the preferences file object that override data is read
	 * from and written to. This object is owned by PrefsManager and is
	 * set when the overrides are first loaded.
	 */
	PrefsFile *m_overridesreader;
#endif // PREFS_HOSTOVERRIDE and PREFS_READ
};

#endif // OPPREFSCOLLECTION_OVERRIDE_H
