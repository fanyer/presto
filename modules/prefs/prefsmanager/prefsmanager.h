/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

/**
 * @file prefsmanager.h
 * Declarations for preference manager framework.
 * Please see the generic documentation below for information about this
 * framework.
 */

#ifndef PREFSMANAGER_H
#define PREFSMANAGER_H

#include "modules/prefs/prefsapitypes.h"
#include "modules/prefsfile/prefsfile.h"

class OpPrefsCollection;
class OpString_list;
class PrefsSectionInternal;

/** Global PrefsManager object (singleton). */
#define g_prefsManager (g_opera->prefs_module.PrefsManager())

/** The preference manager framework contains the framework for handling
  * user customisable settings. The PrefsManager framework collaborates
  * with the OpPrefsCollection subclasses to handle the actual preferences.
  * They use the PrefsFile interface to actually read and write the
  * preferences. How the preferences actually are stored in the file is up
  * to the implementation of PrefsFile, PrefsManager and its partner
  * classes do not care. See the documentation for the
  * <a href="http://wiki.oslo.opera.com/developerwiki/index.php/Prefsfile_module">prefsfile module</a>
  * for more information about that.
  *
  * PrefsManager itself does not store any preferences.
  *
  * @author Peter Karlsson
  * @see OpPrefsCollection
  */
class PrefsManager
{
public:
	// -- Get meta data --------------------------------------------------
#ifdef PREFS_READ
	/** Retrieve the reader object.
	  * This will return the reader object associated with the PrefsManager.
	  * Since outside code is not allowed to tinker with it directly, a
	  * const pointer is returned.
	  */
	inline const PrefsFile *GetReader() const { return m_reader; }
#endif

	/** Check whether the prefs manager has been initialized. */
	inline BOOL IsInitialized() { return m_initialized; }


	// -- String-based preferences API -----------------------------------
#ifdef PREFS_HAVE_STRING_API
	/** Read a preference using the INI name.
	  *
	  * Using this API, you can read any preference using the name in the
	  * INI file. Since this involves a great number of string comparisons,
	  * it should only be used in conjunction with APIs exposed to external
	  * customers. Only known preferences can be read.
	  *
	  * @param section  INI section name.
	  * @param key      INI key name.
	  * @param target   A place to store the value in.
	  * @param defval   Get default value instead of current value.
	  * @param host     Host for overridden setting, NULL to normal setting.
	  * @return         TRUE if the preference was found and could be read.
	  */
	BOOL GetPreferenceL(const char *section, const char *key, OpString &target,
	                    BOOL defval = FALSE, const uni_char *host = NULL);
#endif

#if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE
	/** Write a preference using the INI name.
	  *
	  * Using this API, you can write any preference using the name in the
	  * INI file. Since this involves a great number of string comparisons,
	  * it should only be used in conjunction with APIs exposed to external
	  * customers. Only known preferences can be written.
	  *
	  * If "value" contains non-numeric data and the setting you are trying
	  * to write is an integer preference, this function will leave.
	  *
	  * @param section  INI section name.
	  * @param key      INI key name.
	  * @param value    Value to write.
	  * @return         TRUE if the preference was found and was writable.
	  */
	BOOL WritePreferenceL(const char *section, const char *key, const OpStringC value);

# ifdef PREFS_HOSTOVERRIDE
	/** Write a site specific preference using the INI name.
	  *
	  * Using this API, you can write any preference using the name in the
	  * INI file. This involves a great number of string comparisons,
	  * and should be used with care. Only known preferences can be written.
	  *
	  * If "value" contains non-numeric data and the setting you are trying
	  * to write is an integer preference, this function will leave.
	  *
	  * @param host     host name.
	  * @param section  INI section name.
	  * @param key      INI key name.
	  * @param value    Value to write.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return         TRUE if the preference was found and was writable.
	  */
	BOOL OverridePreferenceL(const uni_char *host, const char *section,
	                         const char *key, const OpStringC value,
	                         BOOL from_user);

	/** Remove a specific override for a subset of hosts. This will delete
	 * the override from the INI file and from memory. The changes will
	 * not be written to disk until CommitL is called.
	 *
	 * @param host Host to delete overrides for (or NULL to match all hosts.)
	 * @param from_user TRUE if the preference to be deleted was entered by
	 *                  the user.
	 * @return TRUE if an override was removed.
	 */
	BOOL RemoveOverrideL(const uni_char *host, const char *section,
						 const char *identifier, BOOL from_user);
# endif
#endif


	// -- Enumerator API -------------------------------------------------
#ifdef PREFS_ENUMERATE
	/** Retrieve a list of (almost) all available preferences in this
	  * Opera build, and their current values. The values are the same
	  * as would be returned when you do GetPreferenceL() on the setting.
	  *
	  * Only known preferences are supported, and only those that are
	  * known at compile time (not dynamic preferences). Only preferences
	  * in the main preferences file will be exposed.
	  *
	  * There is no support for host overrides in this API.
	  *
	  * The returned array must be deleted by the caller.
	  *
	  * @param sort   Whether to sort the list of settings alphabetically.
	  * @param length Number of entries in the list (output).
	  * @return       A list of available settings.
	  */
	struct prefssetting *GetPreferencesL(BOOL sort, unsigned int &length) const;
#endif


	// -- Host overrides -------------------------------------------------
#ifdef PREFS_HOSTOVERRIDE
# ifdef PREFS_WRITE
	/** Remove all overrides for a named host. This will delete all the
	  * overrides from the INI file and from the memory. The changes will
	  * not actually be written to disk until CommitL is called.
	  *
	  * @param host Host to delete overrides for.
	  * @param from_user TRUE if the preference to be deleted was entered by
	  *                  the user.
	  * @return FALSE if there was no override set for this host, or if we
	  * were not allowed to delete it.
	  */
	BOOL RemoveOverridesL(const uni_char *host, BOOL from_user);

	/**
	 * Remove all overrides for all hosts. CommitL is called after
	 * each host has been processed.
	 *
	 * @param from_user TRUE if the preference to be deleted was entered by the user.
	 * @return FALSE if there no overrides were set, or if we were not allowed
	 * to delete them.
	 */
	BOOL RemoveOverridesAllHostsL(BOOL from_user);
# endif

	/** Check if there are overrides available for a named host.
	  *
	  * @param host Host to check for overrides.
	  * @param exact Whether to perform an exact match, or if cascaded
	  * overrides should be allowed.
	  * @return Override status.
	  */
	HostOverrideStatus IsHostOverridden(const uni_char *host, BOOL exact = TRUE);

	/** Count the global number of overrides for a specific host.
	 *
	 * @param host Host to count overrides for.
	 * @return Number of overrides.
	 */
	size_t HostOverrideCount(const uni_char *host);

	/** Enable or disable an overridden host. The name matching is always done
	  * exact.
	  *
	  * @param host Host to check for overrides.
	  * @param active Whether to activate the host.
	  * @return TRUE if a matching host was found.
	  */
	BOOL SetHostOverrideActiveL(const uni_char *host, BOOL active = TRUE);

# ifdef PREFS_GETOVERRIDDENHOSTS
	/** Retrieve a list of overridden hosts. The returned value is a
	  * OpString_list listing the host names that have override data.
	  * The list will contain both enabled and disabled overrides, the
	  * status should always be checked via IsHostOverridden.
	  *
	  * If there are no overridden hosts, this function will return
	  * a NULL pointer.
	  *
	  * @return List of overridden hosts. The pointer must be deleted
	  * by the caller.
	  */
	OpString_list *GetOverriddenHostsL();
# endif

# ifdef PREFS_WRITE
	/** Only to be called from OpPrefsCollectionWithHostOverride */
	void OverrideHostAddedL(const uni_char *);
# endif
#endif // PREFS_HOSTOVERRIDE


	// -- Storage --------------------------------------------------------
	/** Writes all (changed) preferences to the user preferences file.
	  * If PREFS_SOFT_COMMIT is set, the commit will probably not make
	  * changes to the actual file on disk.
	  *
	  * Call this method after changing a batch of preferences where
	  * the user expects the changes to be stored, for instance when
	  * pressing OK in the preferences dialogue.
	  */
	inline void CommitL()
	{
#if defined PREFS_READ && defined PREFS_WRITE
		m_reader->CommitL();
# ifdef PREFS_HOSTOVERRIDE
		if (m_overridesreader)
		{
			m_overridesreader->CommitL();
		}
# endif
#endif
	}

    /** Writes all (changed) preferences to the user preferences file.
	  * This version will set the forced flag to TRUE, and will save
	  * changes to disk even if PREFS_SOFT_COMMIT is set.
	  *
	  * For normal use, you should call CommitL(). Only call this at
	  * critical points in the program, for instance just before
	  * shutting down, or when changing critical data.
	  */
	inline void CommitForceL()
	{
#if defined PREFS_READ && defined PREFS_WRITE
		m_reader->CommitL(TRUE);
# ifdef PREFS_HOSTOVERRIDE
		if (m_overridesreader)
			m_overridesreader->CommitL(TRUE);
# endif
#endif
	}


	// -- Construction and destruction (module internal use only) --------
	/** First-phase constructor.
	  *
	  * @param reader Pointer to a fully configured PrefsFile object to
	  * use. Ownership of this object will be transferred to PrefsManager.
	  * This pointer may not be NULL unless FEATURE_PREFS_READ is disabled,
	  * in which case the value is ignored altogether.
	  */
	explicit PrefsManager(PrefsFile *reader);

	/** Second-phase constructor.
	  * This will initialise PrefsManager and create all the associated
	  * preference collections.
	  */
	void ConstructL();

	/** Read all preferences.
	  * This is called initially, after the object is fully configured,
	  * and before anything is read from any of the collections.
	  *
	  * Please note that the collections may not be used before this
	  * has been called.
	  *
	  * Precondition: OpFolderManager must be set up properly with the paths
	  * that are not configurable by the preferences.
	  *
	  * @param info Initialization data.
	  */
	void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info);

	/** Destructor.
	  *
	  * Please note that you MUST perform a commit before deleting the
	  * object, if you do not all outstanding changes will be lost.
	  */
	~PrefsManager();

private:
	BOOL m_initialized;					///< Flag that we are fully initialized
#ifdef PREFS_READ
	PrefsFile *m_reader;			///< Associated reader object
#endif
#ifdef PREFS_HOSTOVERRIDE
	PrefsSectionInternal *m_overrides;	///< Cached copy of the host override section
# ifdef PREFS_READ
	PrefsFile *m_overridesreader;	///< Associated reader object for overrides
# endif
#endif

#ifdef PREFS_ENUMERATE
	static int GetPreferencesSort(const void *, const void *);
#endif
};
#endif
