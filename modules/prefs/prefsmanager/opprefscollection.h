/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef OPPREFSCOLLECTION_H
#define OPPREFSCOLLECTION_H

#include "modules/prefs/prefs_module.h"
#include "modules/prefs/prefsapitypes.h"
#include "modules/prefs/prefsmanager/prefsinternal.h"
#include "modules/util/simset.h"

class OpPrefsListener;
class PrefsSection;

/** Abstract interface describing a collection of preferences.
  *
  * Most preferences are handled through the generic integer, string and file
  * interfaces, which are defined in the subclasses inheriting this class.
  * Due to a deficiency in the C++ programming language, the generic interface
  * cannot be declared in OpPrefsCollection, but must be defined in its
  * subclasses. When creating a new collection, please make sure you stick
  * to the conventions when it comes to the interface.
  *
  * Some special settings (for instance "externally enumerated" settings) may
  * have their own interfaces.
  *
  * Non cross-platform settings (especially file names stored as strings for
  * platforms which do not wish to use OpFile, especially not in its UI) should
  * be broken out into a platform-specific collection.
  *
  * All actual preferences are declared in the subclasses header file and the
  * corresponding source file. To add one, you need to add a value to the
  * appropriate enum (stringpref or integerpref) in the header file
  * and the default value to the stringprefdefault or integerprefdefault
  * array in the implementation file. To retrieve the values use
  * GetStringPref()/GetStringPrefL() and GetIntegerPref(). To store values use
  * WriteStringL() and WriteIntegerL().
  *
  * A generic preference can only store an integer or a string value. To
  * perform any special pre-write/post-read checks, you must add code to
  * do this in CheckConditionsL().
  *
  * <h3>State saving and return values</h3>
  *
  * Because of the possibility of non-overridable settings (/etc/opera6rc.fixed
  * or \\WINDOWS\\SYSTEM\\opera6.ini etc.) the Write methods do check the
  * return value from the reader object and will only update the internal
  * state if they return success (i.e value is not fixed). If the caller also
  * saves state, it should check the return value as well. The value
  * OpStatus::ERR_NO_ACCESS is used to indicate that it was not allowed to
  * change the settings. Fatal errors (out of memory and similar) are handled
  * through LEAVE.
  *
  * <h3>Implementing non-generic interfaces</h3>
  *
  * Please use the generic interfaces as much as possible. This information
  * is for when the cases when it is allowable to use non-generic interfaces:
  *
  * The Write methods should always take a const OpStringC reference
  * (if you have a char pointer, take a const char pointer). It needs to be
  * constant for GCC to be able to convert a UNI_L() construct to an OpStringC
  * implicitly, and an OpStringC so that we can pass in an OpStringS or an
  * OpString without any type conversion or de-referencing.
  *
  * Your string Read method MUST be of the L kind, and thus should use
  * PrefsFile::ReadStringL. Any allocation done here should be done with
  * new(ELeave).
  *
  * Please note that you should NOT use m_reader->ReadString or
  * m_reader->ReadInt in methods called from outside ReadAllPrefsL, since
  * this will cause a reload of all the INI files if they have been flushed.
  * Please separate the ReadInt/ReadString calls out to Read methods, and
  * access the local storage in a Get method.
  *
  * Please see the standard readers and writers for reference implementations.
  * If anything is unclear, please contact the owner of this module.
  *  
  * @author Peter Karlsson
  */
class OpPrefsCollection : public Link
{
public:
	/**
	  * A list of identifiers of existing collections. Whenever a new
	  * collection is sub-classed from this class, it needs to allocate
	  * a new entry in this enumeration list.
	  */
	enum Collections
	{
		// Core collections ---
		Core,		///< Various core preferences
		Network,	///< Network related preferences
		Display,	///< Display related preferences
		Doc,		///< Document related preferences
		FontsAndColors,///< Font and color related preferences
		Parsing,	///< Parsing related preferences
		Print,		///< Printing related preferences
		App,		///< External application related preferences
		Files,		///< File name preferences
		JS,			///< Javascript related preferences
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
		PersistentStorage,
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSERVER_SUPPORT
		Webserver,	///< Webserver related preferences
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
		Tools,		///< Developer tools related preferences
#endif
#if defined GEOLOCATION_SUPPORT
		Geolocation,///< Settings for the geolocation module
#endif
#ifdef SUPPORT_DATA_SYNC
		Sync,		///< Opera Sync related preferences
#endif

		// Adjunct collections ---
#ifdef PREFS_HAVE_DESKTOP_UI
		UI,			///< Quick UI related preferences
#endif
#ifdef M2_SUPPORT
		M2,			///< M2 preferences
#endif

		// Platform collections ---
#ifdef PREFS_HAVE_MSWIN
		MSWin,		///< MS Windows specific preferences
#endif
#ifdef PREFS_HAVE_UNIX
		Unix,		///< Unix specific preferences
#endif
#ifdef PREFS_HAVE_MAC
		MacOS,		///< MacOS specific preferences
#endif
#ifdef PREFS_HAVE_COREGOGI
		Coregogi,	///< Coregogi specific preferences
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
		OperaAccount,			///< Opera Account preferences
#endif
#ifdef DOM_JIL_API_SUPPORT
		JIL,		///< JIL API preferences
#endif	
		// Sentinel
		None		///< Sentinel value, unused
	};

	/** Destructor. */
	virtual ~OpPrefsCollection();

	/** Retrieve the identity of this collection. */
	enum Collections Identity() { return m_identity; }

	/** Register a listener. The listener will be informed every time a
	  * preference is changed in this collection.
	  *
	  * Will leave if the registration fails for any reason.
	  * If the listener is already registered, nothing will happen.
	  *
	  * @param listener The listener object to register.
	  */
	void RegisterListenerL(OpPrefsListener *listener);

	/** Register a listener. The listener will be informed every time a
	  * preference is changed in this collection.
	  *
	  * If the listener is already registered, nothing will happen.
	  *
	  * @param listener The listener object to register.
	  * @return OpStatus::ERR_NO_MEMORY if OOM encountered,
	  * OpStatus::OK otherwise.
	  */
	OP_STATUS RegisterListener(OpPrefsListener *listener);

	/** Unregister a listener. The listener will no longer be informed of
	  * changes.
	  *
	  * If the listener is not registered, nothing will happen.
	  *
	  * @param listener The listener object to unregister.
	  */
	void UnregisterListener(OpPrefsListener *listener);

	/** Read all settings from the PrefsFile to internal storage.
	  * Used by PrefsManager, called only once some time after creating
	  * the object and before the first call to any other use.
	  *
	  * @param info The initialization data provided by the platform
	  */
	virtual void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info) = 0;

#ifdef PREFS_HOSTOVERRIDE
	/** Set reader object to use when accessing host overrides.
	  *
	  * @param reader Reader for host overrides.
	  */
	virtual void SetOverrideReader(PrefsFile *reader) = 0;

	/** Read overridden settings from the PrefsFile to internal storage.
	  * Used by PrefsManager, called only once some time after creating
	  * the object and before the first call to any other use. Does nothing
	  * for this class, but is implemented in OpPrefsCollectionWithHostOverride
	  *
	  * @param host Host to read overrides for.
	  * @param section The host override section for this host.
	  * @param active Whether the overrides are active.
	  * @param from_user Whether the overrides are entered by the user.
	  */
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section,
		BOOL active, BOOL from_user) = 0;

# ifdef PREFS_WRITE
	/** Remove all overrides for a named host. Used by PrefsManager, called
	  * if PrefsManager is requested to remove a host. Does nothing for this
	  * class, but is implemented in OpPrefsCollectionWithHostOverride
	  *
	  * @param host Host to delete overrides for.
	  */
	virtual void RemoveOverridesL(const uni_char *host) = 0;
# endif

	/** Check if there are overrides available for this host. Please note
	  * that it may return HostNotOverridden if the host is overridden only
	  * for other OpPrefsCollection objects, or if the collection does not
	  * support host overrides.
	  *
	  * @param host Host to check for overrides.
	  * @param exact Whether to perform an exact match, or if cascaded
	  * overrides should be allowed.
	  * @return If there are overrides available for this host.
	  */
	virtual HostOverrideStatus IsHostOverridden(const uni_char *host, BOOL exact = TRUE) = 0;

	/** Return number of preferences overridden for host.
	 */
	virtual size_t HostOverrideCount(const uni_char *host) const { return 0; }

	/** Enable or disable an overridden host. The name matching is always done
	  * exact.
	  *
	  * @param host Host to check for overrides.
	  * @param status Whether to activate the host.
	  * @return TRUE if a matching host was found.
	  */
	virtual BOOL SetHostOverrideActive(const uni_char *host, BOOL status = TRUE) = 0;
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_ENUMERATE
	/** Retrieve the number of settings available from this collection.
	  * This must match the number of settings read by GetPreferencesL(),
	  * and may be less than the actual number of preferences held by the
	  * collection.
	  *
	  * The value may be volatile, and may change if any other method is
	  * called before GetPreferencesL().
	  *
	  * @return Number of preferences to be returned by GetPreferencesL().
	  */
	virtual unsigned int GetNumberOfPreferences() const = 0;

	/** Retrieve the list of available preferences from the collection. The
	  * number of preferences copied are the same as returned from
	  * GetNumberOfPreferences(), and the passed array must have at least
	  * that number of elements available.
	  *
	  * @param settings Array to write settings to.
	  * @return Number of preferences returned.
	  */
	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const = 0;
#endif

#ifdef PREFS_READ
	/** Sections named in the INI file. To avoid wasting space with
	  * duplicated strings for compilers that do not optimise them, the
	  * section names are only stored in this class. The enumeration
	  * name must be "S" + the name of the section without spaces.
	  */
	enum IniSection
	{
		SUserPrefs = 0,
		SParsing,
		SNetwork,
		SCache,
		SDiskCache,
		SExtensions,
#if defined PREFS_HAVE_MAIL_APP || defined PREFS_HAVE_NEWS_APP || defined M2_SUPPORT
		SMail,
#endif
#ifdef PREFS_HAVE_NEWS_APP
		SNews,
#endif
#if defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
		SFileTypesSectionInfo,
#endif
		SCSSGenericFontFamily,
		SLink,
		SVisitedLink,
		SSystem,
		SMultimedia,
		SUserDisplayMode,
		SAuthorDisplayMode,
		SRM1,
		SRM2,
		SRM3,
		SRM4,
#ifdef TV_RENDERING
		SRM5,
#endif
		SHandheld,
		SSpecial,
		SISP,
#ifdef PREFS_HAVE_DESKTOP_UI
		SHotListWindow,
#endif
		SProxy,
		SPerformance,
#ifdef _SSL_USE_SMARTCARD_
		SSmartCards,
#endif
		SUserAgent,
		SSecurityPrefs,
#ifdef _PRINT_SUPPORT_
		SPrinter,
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
		SBrand,
#endif
#ifdef PREFS_HAVE_WORKSPACE
		SWorkspace,
#endif
#ifdef PREFS_HAVE_FILESELECTOR
		SFileSelector,
#endif
		SPersonalInfo,
#if defined PREFS_HAVE_DESKTOP_UI || defined PREFS_DOWNLOAD
		SInstall,
#endif
#if defined PREFS_HAVE_DESKTOP_UI || defined PREFS_HAVE_UNIX_PLUGIN
		SState,
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
		STransferWindow,
		SClearPrivateDataDialog,
		SInterfaceColors,
		SSounds,
#endif
#ifdef _FILE_UPLOAD_SUPPORT_
		SSavedSettings,
#endif
#ifndef _NO_HELP_
		SHelpWindow,
#endif
#ifdef PREFS_HAVE_HOTLIST_EXTRA
		SMailBox,
#endif
#ifdef GADGET_SUPPORT
		SWidgets,
#endif
#if defined LOCAL_CSS_FILES_SUPPORT && !defined PREFS_USE_CSS_FOLDER_SCAN
		SLocalCSSFiles,
#endif
#ifdef WEBSERVER_SUPPORT
		SWebServer,
#endif
#ifdef SVG_SUPPORT
		SSVG,
#endif
#ifdef _BITTORRENT_SUPPORT_
		SBitTorrent,
#endif
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		SOEM,
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
		SDeveloperTools,
#endif
#if defined GEOLOCATION_SUPPORT
		SGeolocation,
#endif
#if defined DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
		SOrientation,
#endif
#ifdef SUPPORT_DATA_SYNC
		SOperaSync,
		SOperaSyncServer,
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
		SOperaAccount,
#endif
#ifdef AUTO_UPDATE_SUPPORT
		SAutoUpdate,
#endif
		SColors,
		SFonts,
#if defined(PREFS_HAVE_APP_INFO)
		SAppInfo,
#endif
#if defined CLIENTSIDE_STORAGE_SUPPORT || defined DATABASE_STORAGE_SUPPORT
		SPersistentStorage,
#endif
#ifdef DOM_JIL_API_SUPPORT
		SJILAPI,
#endif
#ifdef PREFS_HAVE_HBBTV
		SHbbTV,
#endif
#ifdef UPGRADE_SUPPORT
		STrustedProtocolsSectionInfo,
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
		SCollectionView,
#endif
		SNone						///< Sentinel

	};
#define OPPREFSCOLLECTION_NUMBEROFSECTIONS (static_cast<int>(OpPrefsCollection::SNone))

	/** Convert a section string to an internal numerical id.
	  * @param section Name of the section.
	  * @return Internal numerical id for the section.
	  */
	static enum IniSection SectionStringToNumber(const char *section);


# ifdef HAS_COMPLEX_GLOBALS
	/** Convert a section id to a string.
	  * @param section Number of the section.
	  * @return String title for the section.
	  */
	static inline const char *SectionNumberToString(IniSection section)
	{ return m_sections[int(section)]; }
# else
	/** Convert a section id to a string.
	  * @param section Number of the section.
	  * @return String title for the section.
	  */
	static const char *SectionNumberToString(IniSection section);
# endif
#endif

protected:
	/** Single-phase constructor.
	  *
	  * Inherited classes may have two-phase constructors if they wish.
	  *
	  * @param identity Identity of this collection (communicated by subclass).
	  * @param reader Pointer to the PrefsFile object.
	  */
	OpPrefsCollection(enum Collections identity, PrefsFile *reader);

#ifdef PREFS_WRITE
	/** Broadcast a changed integer preference to all listeners.
	  *
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void BroadcastChange(int pref, int newvalue);

	/** Broadcast a changed string preference to all listeners.
	  *
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void BroadcastChange(int pref, const OpStringC &newvalue);
#endif // PREFS_WRITE

	/** Identity of this collection. */
	enum Collections m_identity;

	/** Container element used in the list of listeners. */
	class ListenerContainer : public Link
	{
	public:
		/** Create a container.
		  * @param listener The object to encapsulate.
		  */
		ListenerContainer(OpPrefsListener *listener)
			: Link(), m_listener(listener) {};
		/** Return the encapsulated pointer as the link id. */
		virtual const char* LinkId() { return reinterpret_cast<const char *>(m_listener); }
		/** Retrieve the encapsulated pointer. */
		inline OpPrefsListener *GetListener() { return m_listener; }

	private:
		OpPrefsListener *m_listener; ///< The encapsulated object
	};

	/** Linked list of registered listeners, contains ListenerContainer
	  * objects. */
	Head m_listeners;

#ifdef PREFS_READ
	/**
	  * Pointer to the preferences file object that data is read from and
	  * written to. This object is owned by PrefsManager and is set when
	  * the object is constructed.
	  */
	PrefsFile *m_reader;
#endif

#ifdef PREFS_READ
	/** Sections named in the INI file. To avoid wasting space with
	  * duplicated strings for compilers that do not optimise them, the
	  * section names are only stored in this class.
	  */
	PREFS_STATIC_CONST char * PREFS_CONST m_sections[OPPREFSCOLLECTION_NUMBEROFSECTIONS];
#endif

	/**
	  * Structure used in sub-classes to list string settings to be read from
	  * file.
	  */
	struct stringprefdefault
	{
#ifdef PREFS_READ
		enum IniSection section;	///< Section of INI file
		const char *key;			///< Key in INI file
#endif
		const uni_char *defval;		///< Default value
	};

	/**
	  * Structure used in sub-classes to list integer settings to be read from
	  * file.
	  */
	struct integerprefdefault
	{
#ifdef PREFS_READ
		enum IniSection section;	///< Section of INI file
		const char *key;			///< Key in INI file
#endif
		int defval;					///< Default value
#ifdef PREFS_ENUMERATE
		enum prefssetting::prefssettingtypes type;///< Type of preference (must be integral)
#endif
	};

#ifdef PREFS_HAVE_STRING_API
	/** Read a preference using the INI name. Should only be used from
	  * PrefsManager.
	  */
	virtual BOOL GetPreferenceL(IniSection section, const char *key,
		OpString &target, BOOL defval, const uni_char *host) = 0;
#endif

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
	/** Helper for GetPreferenceL(), GetPreferenceInternalL() and
	  * ResetStringL(). Should be overridden by collections with defaults
	  * that cannot be determined at run-time, and which do not override
	  * GetPreferenceL() or GetStringL().
	  *
	  * @param which Index of the string preference.
	  * @param pref Entry for the string preference.
	  */
	virtual const uni_char *GetDefaultStringInternal(int which, const struct stringprefdefault *pref);

	/** Helper for GetPreferenceL(), GetPreferenceInternalL() and
	  * ResetIntegerL(). Should be overridden by collections with defaults
	  * that cannot be determined at run-time, and which do not override
	  * GetPreferenceL() or GetIntegerL().
	  *
	  * @param which Index of the integer preference.
	  * @param pref Entry for the integer preference.
	  */
	virtual int GetDefaultIntegerInternal(int which, const struct integerprefdefault *pref);
#endif

#if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE
	/** Write a preference using the INI name. Should only be used from
	  * PrefsManager.
	  */
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value) = 0;
	BOOL WritePreferenceInternalL(IniSection section, const char *key, const OpStringC &value,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers);

# ifdef PREFS_HOSTOVERRIDE
	/** Override a preference using the INI name. Should only be used from
	  * PrefsManager.
	  */
	virtual BOOL OverridePreferenceL(const uni_char *, IniSection, const char *, const OpStringC &, BOOL) = 0;

	/** Remove a preference using the INI name. Should only be used from
	 * PrefsManager.
	 */
	virtual BOOL RemoveOverrideL(const uni_char *, IniSection, const char *, BOOL) = 0;
# endif // PREFS_HOSTOVERRIDE
#endif // PREFS_HAVE_STRING_API and PREFS_WRITE

#ifdef PREFS_ENUMERATE
	/** Enumerate available available preferences, internal helper.
	  */
	unsigned int GetPreferencesInternalL(struct prefssetting *settings,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers) const;
#endif

#ifdef PREFS_WRITE
	/** Interfaced used by sub-classes to write integer preferences. */
	OP_STATUS WriteIntegerL(const struct integerprefdefault *pref,
	                        int which, int value);

	/** Interfaced used by sub-classes to write string preferences. */
	OP_STATUS WriteStringL(const struct stringprefdefault *pref,
	                       int which, const OpStringC &value);

	/** Interface used by sub-classes to reset string preferences. */
	BOOL ResetStringL(const struct stringprefdefault *pref, int which);

	/** Interfaced used by sub-classes to reset string preferences. */
	BOOL ResetIntegerL(const struct integerprefdefault *pref, int which);
#endif // PREFS_WRITE

	/** Interface used by sub-classes to fetch preferences from the reader. */
	void ReadAllPrefsL(const struct stringprefdefault *strings, int numstrings, 
	                   const struct integerprefdefault *integers, int numintegers);

#ifdef PREFS_VALIDATE
	/** Check post-read and pre-write conditions for an integer value.
	  * This gets called by ReadAllPrefsL after every read, and before every
	  * WriteInt call. If the condition fails, the value pointed to will be
	  * changed. Will only leave if a fatal error occurs.
	  *
	  * @param which Index to the preference, converted from the enumeration
	  *              defined in the collection.
	  * @param value Pointer to the value to be checked.
	  * @param host  Host for which the preference is being set. NULL if the
	  *              global setting is being set.
	  */
	virtual void CheckConditionsL(int which, int *value, const uni_char *host) = 0;

	/** Check post-read and pre-write conditions for a string value.
	  * This gets called by ReadAllPrefsL after every read, and before every
	  * WriteString call. If the condition fails, a new replacement value
	  * will be created and written to the outvalue pointer (a new OpString
	  * will be created which must be deleted by the caller). Will only leave
	  * if a fatal error occurs. If FALSE is returned, i.e no change was
	  * necessary, outvalue is undefined.
	  *
	  * @param which Index to the preference, converted from the enumeration
	  *              defined in the collection.
	  * @param invalue Reference to the value to be checked.
	  * @param outvalue Cleaned-up value if change was needed.
	  * @param host Host for which the preference is being set. NULL if the
	  *             global setting is being set.
	  * @return TRUE if a change was required.
	  */
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host) = 0;
#endif // PREFS_VALIDATE

#ifdef PREFS_HAVE_STRING_API
	int GetIntegerPrefIndex(IniSection, const char *,
	                        const struct integerprefdefault *, int);
	int GetStringPrefIndex(IniSection, const char *,
	                       const struct stringprefdefault *, int);
#endif

#ifdef PREFS_READ
# ifndef HAS_COMPLEX_GLOBALS
	inline const char *SectionNumberToStringInternal(IniSection section)
	{
		// No static data, so look up in this instance.
		return m_sections[static_cast<int>(section)];
	}
# else
	inline const char *SectionNumberToStringInternal(IniSection section)
	{
		// We have static data, so map back to the static implementation.
		return SectionNumberToString(section);
	}
# endif
#endif

	// Debug code ------
#ifdef PREFS_COVERAGE
	virtual void CoverageReport(const struct stringprefdefault *, int,
	                            const struct integerprefdefault *, int);
	static void CoverageReportPrint(const char *);
#endif

	// Storage ------
	OpString *m_stringprefs;	///< All string preferences
	int *m_intprefs;			///< All integer preferences
#ifdef PREFS_COVERAGE
	int *m_stringprefused, *m_intprefused;
#endif

private:
	/** Helper function to find a OpPrefsListener in the list of listeners.
	  *
	  * @param which The OpPrefsListener to look for.
	  * @return The requested object, NULL if not found.
	  */
	ListenerContainer *GetContainerFor(const OpPrefsListener *which);

#ifdef PREFS_READ
# ifndef HAS_COMPLEX_GLOBALS
	void InitSections();
# endif
#endif

	// Sorry about these:
#ifdef PREFS_HOSTOVERRIDE
	friend class OverrideHost; // Accesses CheckConditionsL()
	friend class OpOverrideHostContainer; // Uses {integer,string}prefdefault struct.
	friend class OverrideHostForPrefsCollectionFontsAndColors; // Uses stringprefdefault struct
#endif
#ifdef PREFS_HAVE_STRING_API
	friend class PrefsManager; // Accesses GetPreferenceL() &co
#endif
};

#endif // OPPREFSCOLLECTION_H
