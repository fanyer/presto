/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** George Refseth, rfz
*/

#if defined PREFS_HAVE_DESKTOP_UI && !defined PC_UI_H
# define PC_UI_H

class PrefsFile;
class PrefsSectionInternal;

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/dochand/winman_constants.h"
#include "adjunct/desktop_util/prefs/DesktopPrefsTypes.h"

class OpRect;

/** Global PrefsCollectionUI object (singleton). */
#define g_pcui (g_opera->prefs_module.PrefsCollectionUI())

/**
  * Settings for the Quick user interface. Do not add preferences
  * referenced by core code here, unless that code is Quick-only.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionUI : public OpPrefsCollectionWithHostOverride
{
public:
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
	static PrefsCollectionUI *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionUI();

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI_h.inl"

	// Read ------
	/**
	 * Read an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 */
	inline int GetIntegerPref(integerpref which, const uni_char *host = NULL) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host);
	};

	/**
	 * Read a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param result Variable where the value will be stored.
	 */
	inline void GetStringPrefL(stringpref which, OpString &result, const uni_char *host = NULL) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/**
	 * Read a string preference. This method will return a pointer to internal
	 * data. The object is only guaranteed to live until the next call to any
	 * Write method.
	 *
	 * @param which Selects the preference to retrieve.
	 */
	inline const OpStringC GetStringPref(stringpref which, const uni_char *host = NULL) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host));
	}

	/** Get the size and state of a window. If the size and state is undefined
	  * (i.e not specified in the INI file), the size and state variables will
	  * not be touched.
	  * @param which The window type to get the size for.
	  * @param size Window position and size (output).
	  * @param state Minimize/maximize state (output).
	  * @return TRUE if any data was found. FALSE if no data was found, in
	  *         which case size and state was not touched.
	  */
	BOOL GetWindowInfo(const OpStringC &which, OpRect &size, WinSizeState &state);

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host)
	{
		return OpPrefsCollectionWithHostOverride::GetPreferenceInternalL(
			section, key, target, defval, host,
			m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
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

	/** Write an window size and state preference.
	  *
	  * @param which The window type.
	  * @param size Window position and size.
	  * @param state Minimize/maximize state.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS WriteWindowInfoL(const OpStringC &which, const OpRect &size, WinSizeState state);

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

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
	{
		return OpPrefsCollection::WritePreferenceInternalL(
			section, key, value,
			m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
	}

#  ifdef PREFS_HOSTOVERRIDE

	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePreferenceInternalL(
			host, section, key, value, from_user,
			m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
	}

	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveOverrideInternalL(
			host, section, key, from_user,
			m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
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
	void ReadCachedQuickSectionsL();
#ifdef PREFS_HOSTOVERRIDE
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
#endif // PREFS_HOSTOVERRIDE

	// Helpers ------
	/** Retrieve a value from the "Columns" section. */
	const uni_char *GetColumnSettings(const OpStringC &which);
	/** Retrieve a value from the "Matches" section. */
	const uni_char *GetMatchSettings(const OpStringC &which);

#ifdef PREFS_WRITE
	/** Write a value to the "Columns" section. */
	void WriteColumnSettingsL(const OpStringC &which, const uni_char *column_string);
	/** Write a value to the "Matches" section. */
	void WriteMatchSettingsL(const OpStringC &which, const uni_char *match_string);
#endif // PREFS_WRITE

	/** Clear all settings in the "Matches" section. */
	void ClearMatchSettingsL();

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCUI_NUMBEROFSTRINGPREFS + PCUI_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCUI_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCUI_NUMBEROFINTEGERPREFS);
	}
#endif

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionUI(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(UI, reader),
		  m_windowsizeprefs(NULL),
		  m_treeview_columns(NULL),
		  m_treeview_matches(NULL)
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCUI_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCUI_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host);
#endif

	PrefsSectionInternal *m_windowsizeprefs;
	PrefsSectionInternal *m_treeview_columns;
	PrefsSectionInternal *m_treeview_matches;

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif
};

#endif // PREFS_HAVE_DESKTOP_UI && !PC_UI_H
