/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULES_DATABASE_PREFSCOLLECTIONDATABASE_H_
#define _MODULES_DATABASE_PREFSCOLLECTIONDATABASE_H_

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/url/url_sn.h"

/** Global PrefsCollectionDatabase object (singleton). */
#define g_pcdatabase (g_opera->prefs_module.PrefsCollectionDatabase())

/**
  * Preferences related to the core database module implementation.
  */
class PrefsCollectionDatabase : public OpPrefsCollectionWithHostOverride
{
public:

	// Include enumerations describing the prefs in this collection
	#include "modules/database/src/prefscollectiondatabase_h.inl"

	/**
	  * Create method.
	  *
	  * This method preforms all actions required to construct the object.
	  * PrefsManager calls this method to initialize the object, and will
	  * not call the normal constructor directly.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  * @return Pointer to the created object.
	  */
	static PrefsCollectionDatabase *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionDatabase();

	// Read ------
	/**
	 * Read an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline int GetIntegerPref(integerpref which, const uni_char *host = NULL) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host);
	}

	/** @overload */
	inline int GetIntegerPref(integerpref which, const ServerName *host) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host ? host->UniName() : NULL);
	}

	/** @overload */
	inline int GetIntegerPref(integerpref which, const URL &host) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host);
	}

	/**
	 * Read a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param result Variable where the value will be stored.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline void GetStringPrefL(stringpref which, OpString &result, const uni_char *host = NULL) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/** @overload */
	inline void GetStringPrefL(stringpref which, OpString &result, const ServerName *host) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/** @overload */
	inline void GetStringPrefL(stringpref which, OpString &result, const URL &host) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/**
	 * Read a string preference. This method will return a pointer to internal
	 * data. The object is only guaranteed to live until the next call to any
	 * Write method.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline const OpStringC GetStringPref(stringpref which, const uni_char *host = NULL) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host));
	}

	/** @overload */
	inline const OpStringC GetStringPref(stringpref which, const ServerName *host) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host ? host->UniName() : NULL));
	}

	/** @overload */
	inline const OpStringC GetStringPref(stringpref which, const URL &host) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host));
	}

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Check if integer preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	inline BOOL IsPreferenceOverridden(integerpref which, const uni_char *host) const
	{
		return IsIntegerOverridden(int(which), host);
	}
	inline BOOL IsPreferenceOverridden(integerpref which, const URL &host) const
	{
		return IsIntegerOverridden(int(which), host);
	}

	/**
	 * Check if string preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	inline BOOL IsPreferenceOverridden(stringpref which, const uni_char *host) const
	{
		return IsStringOverridden(int(which), host);
	}
	inline BOOL IsPreferenceOverridden(stringpref which, const URL &host) const
	{
		return IsStringOverridden(int(which), host);
	}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host)
	{
		return OpPrefsCollectionWithHostOverride::GetPreferenceInternalL(
			section, key, target, defval, host,
			m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
	}
#endif


	// Defaults ------
	/**
	 * Get default value for an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	inline int GetDefaultIntegerPref(integerpref which) const
	{
		return m_integerprefdefault[which].defval;
	}

	/**
	 * Get default value for a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	inline const uni_char *GetDefaultStringPref(stringpref which) const
	{
		return m_stringprefdefault[which].defval;
	}

#ifdef PREFS_WRITE
	// Write ------
# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden integer preferences.
	  *
	  * @param host Host to write an override for
	  * @param which The preference to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS OverridePrefL(const uni_char *host, integerpref which,
	                               int value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePrefL(host, &m_integerprefdefault[which], int(which), value, from_user);
	}
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden string preferences.
	  *
	  * @param host Host to write an override for
	  * @param which The preference to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS OverridePrefL(const uni_char *host, stringpref which,
	                               const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePrefL(host, &m_stringprefdefault[which], int(which), value, from_user);
	}
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Remove overridden integer preference.
	  *
	  * @param host Host to remove an override from, or NULL to match all hosts.
	  * @param which The preference to override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return TRUE if an override was removed.
	  */
	inline BOOL RemoveOverrideL(const uni_char *host, integerpref which,
										   BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveIntegerOverrideL(host, &m_integerprefdefault[which], int(which), from_user);
	}
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Remove overridden string preference.
	  *
	  * @param host Host to remove an override from, or NULL to match all hosts.
	  * @param which The preference to override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return TRUE if an override was removed.
	  */
	inline BOOL RemoveOverrideL(const uni_char *host, stringpref which,
										   BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveStringOverrideL(host, &m_stringprefdefault[which], int(which), from_user);
	}
# endif // PREFS_HOSTOVERRIDE

	/** Write a string preference.
	  *
	  * @param which The preference to write.
	  * @param value Value for the write.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS WriteStringL(stringpref which, const OpStringC &value)
	{
		return OpPrefsCollection::WriteStringL(&m_stringprefdefault[which], int(which), value);
	}

	/** Write an integer preference.
	  *
	  * @param which The preference to write.
	  * @param value Value for the write.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS WriteIntegerL(integerpref which, int value)
	{
		return OpPrefsCollection::WriteIntegerL(&m_integerprefdefault[which], int(which), value);
	}

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
	{
		return OpPrefsCollection::WritePreferenceInternalL(
			section, key, value,
			m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
	}

#  ifdef PREFS_HOSTOVERRIDE
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePreferenceInternalL(
			host, section, key, value, from_user,
			m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
	}

	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveOverrideInternalL(
			host, section, key, from_user,
			m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
	}
#  endif
# endif
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	// Reset ------
	/** Reset a string preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	inline BOOL ResetStringL(stringpref which)
	{
		return OpPrefsCollection::ResetStringL(&m_stringprefdefault[which], int(which));
	}

	/** Reset an integer preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	inline BOOL ResetIntegerL(integerpref which)
	{
		return OpPrefsCollection::ResetIntegerL(&m_integerprefdefault[which], int(which));
	}
#endif // PREFS_WRITE

	// Fetch preferences from file ------
	virtual void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info);
#ifdef PREFS_HOSTOVERRIDE
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
#endif

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCDATABASE_NUMBEROFSTRINGPREFS + PCDATABASE_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
	}
#endif

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionDatabase(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(PersistentStorage, reader)
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCDATABASE_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCDATABASE_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue, OpString **outvalue, const uni_char *host);
#endif

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif
};

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#endif //_MODULES_DATABASE_PREFSCOLLECTIONDATABASE_H_
