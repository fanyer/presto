/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PC_APP_H
#define PC_APP_H

#include "modules/prefs/prefsmanager/opprefscollection_default.h"
#include "modules/viewers/viewers.h"

class PrefsSection;
class PrefsEntry;

/** Global PrefsCollectionApp object (singleton). */
#define g_pcapp (g_opera->prefs_module.PrefsCollectionApp())

/**
  * Preferences for external applications and plug-ins.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionApp : public OpPrefsCollectionDefault
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
	static PrefsCollectionApp *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionApp();

#include "modules/prefs/prefsmanager/collections/pc_app_h.inl"

	// Read ------
	/**
	 * Read an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * retrieve the default context.
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
	 * retrieve the default context.
	 */
	inline void GetStringPrefL(stringpref which, OpString &result) const
	{
		result.SetL(GetStringPref(which));
	}

	/**
	 * Read a string preference. Use of this method is slighlty deprecated,
	 * as it will return a pointer to internal data. Use it where performance
	 * demands it. The object is only guaranteed to live until the next call
	 * to a Write method.
	 *
	 * @param which Selects the preference to retrieve.
	 */
	inline const OpStringC GetStringPref(stringpref which) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionDefault::GetStringPref(int(which)));
	}

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Check whether a given plugin is on the ignore list.
	 *
	 * @param plugname Name of plugin
	 * @return TRUE if the plugin should be ignored
	 */
	BOOL IsPluginToBeIgnored(const uni_char *plugname);

# if defined PREFS_IGNOREPLUGIN_CONFIGURE && defined PREFS_WRITE
	/**
	 * Add or remove a plugin in the ignore list.
	 *
	 * @param plugname Name of plugin
	 * @param ignore TRUE if the plugin should be ignored.
	 */
	void WritePluginToBeIgnoredL(const uni_char *plugfilename, BOOL ignore);
# endif
#endif

#ifdef DYNAMIC_VIEWERS
	/**
	  * Read viewer types defined in the fixed global, user and global
	  * preferences files. A type can only be returned once, and that is
	  * from the highest ranking file it is found in.
	  *
	  * Please note that reading this section may require reading back the
	  * file into the cache, so it may leave if you are low on memory. Also,
	  * make sure you read the section through to the end, as it will clean
	  * up its internal state only when it is ready.
	  *
	  * @param key      Buffer receiving key name
	  * @param value    Buffer receiving value data
	  * @param errval	Returned error code, if any
	  * @param restart  TRUE to restart reading, FALSE to continue.
	  * @return TRUE if another value exists, FALSE if we are done reading or
	  *         if an error occured.
	  */
	BOOL ReadViewerTypesL(
					OpString &key,
					OpString &value,
					OP_STATUS &errval,
					BOOL restart = FALSE);

	/**
	  * Read extended data associated with this viewer.
	  * @param key    Key to lookup.
	  * @param buf    String that receives extended data.
	  * @return TRUE if a value was read.
	  */
	BOOL ReadViewerExtensionL(const OpStringC &key, OpString &buf);
#endif // DYNAMIC_VIEWERS

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *)
	{
		return OpPrefsCollectionDefault::GetPreferenceInternalL(
			section, key, target, defval,
			m_stringprefdefault, PCAPP_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCAPP_NUMBEROFINTEGERPREFS);
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
	const uni_char *GetDefaultStringPref(stringpref which) const;

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
			m_stringprefdefault, PCAPP_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCAPP_NUMBEROFINTEGERPREFS);
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
		return PCAPP_NUMBEROFSTRINGPREFS + PCAPP_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCAPP_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCAPP_NUMBEROFINTEGERPREFS);
	}
#endif

#if defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
	/**
	 * Deletes Viewer sections in the main prefs file (since viewers were moved to their own file)
	 */
	void DeleteViewerSectionsL();
#endif // defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionApp(PrefsFile *reader)
		: OpPrefsCollectionDefault(App, reader)
#if defined _PLUGIN_SUPPORT_ && defined PREFS_HAS_PREFSFILE && defined PREFS_READ
# ifdef PREFS_IGNOREPLUGIN_CONFIGURE
		  , m_ignoreini(NULL)
# else
		  , m_IgnorePlugins(NULL)
# endif
#endif
#ifdef DYNAMIC_VIEWERS
		  , m_viewersection(NULL)
		  , m_viewerextensionsection(NULL)
		  , m_viewer_current(NULL)
#endif
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCAPP_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCAPP_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host);
#endif

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
	virtual const uni_char *GetDefaultStringInternal(int, const struct stringprefdefault *);
#endif

#if defined _PLUGIN_SUPPORT_ && defined PREFS_HAS_PREFSFILE && defined PREFS_READ
	void ReadPluginsToBeIgnoredL();
# ifdef PREFS_IGNOREPLUGIN_CONFIGURE
	class PrefsFile *m_ignoreini;			///< File for plugins to be ignored
# else
	PrefsSection *m_IgnorePlugins;			///< Plugins that should be ignored
# endif
#endif
#ifdef DYNAMIC_VIEWERS
	PrefsSection *m_viewersection;			///< State for ReadViewerTypesL
	PrefsSection *m_viewerextensionsection;	///< State for ReadViewerTypesL
	const PrefsEntry *m_viewer_current;		///< State for ReadViewerTypesL
#endif // DYNAMIC_VIEWERS

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif
};

#endif // PC_APP_H
