/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined PC_MSWIN_H && defined PREFS_HAVE_MSWIN
#define PC_MSWIN_H

#include "modules/prefs/prefsmanager/opprefscollection_default.h"

/** Global PrefsCollectionMSWIN object (singleton). */
#define g_pcmswin (g_opera->prefs_module.PrefsCollectionMSWIN())

/**
  * Preferences for the Windows platform.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionMSWIN : public OpPrefsCollectionDefault
{
public:
	virtual ~PrefsCollectionMSWIN();

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
	static PrefsCollectionMSWIN *CreateL(PrefsFile *reader);

#include "platforms/windows/prefs/PrefsCollectionMSWIN_h.inl"

	// Read ------
	/**
	 * Read an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 */
	inline int GetIntegerPref(integerpref which) const
	{
		return OpPrefsCollectionDefault::GetIntegerPref(int(which));
	};

	/**
	 * Read a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param result Variable where the value will be stored.
	 */
	inline void GetStringPrefL(stringpref which, OpString &result) const
	{
		result.SetL(GetStringPref(which));
	}

	/**
	 * Read a string preference. This method will return a pointer to internal
	 * data. The object is only guaranteed to live until the next call to any
	 * Write method.
	 *
	 * @param which Selects the preference to retrieve.
	 */
	inline const OpStringC GetStringPref(stringpref which) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionDefault::GetStringPref(int(which)));
	}

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host)
	{
		return OpPrefsCollectionDefault::GetPreferenceInternalL(
			section, key, target, defval,
			m_stringprefdefault, PCMSWIN_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCMSWIN_NUMBEROFINTEGERPREFS);
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

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
	{
		return OpPrefsCollection::WritePreferenceInternalL(
			section, key, value,
			m_stringprefdefault, PCMSWIN_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCMSWIN_NUMBEROFINTEGERPREFS);
	}
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

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCMSWIN_NUMBEROFSTRINGPREFS + PCMSWIN_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCMSWIN_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCMSWIN_NUMBEROFINTEGERPREFS);
	}
#endif

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionMSWIN(PrefsFile *reader)
		: OpPrefsCollectionDefault(MSWin, reader)
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCMSWIN_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCMSWIN_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host);
#endif

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif
};

#endif // !PC_MSWIN_H && PREFS_HAVE_MSWIN
