/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PC_FILES_H
#define PC_FILES_H

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/util/opfile/opfile.h"
#include "modules/url/url_sn.h"

#ifdef LOCAL_CSS_FILES_SUPPORT
# include "modules/util/adt/opvector.h"
#endif

class OverrideHostForPrefsCollectionFiles;
class PrefsCollectionFilesListener;

/** Global PrefsCollectionFiles object (singleton). */
#define g_pcfiles (g_opera->prefs_module.PrefsCollectionFiles())

/**
  * Preferences for file names.
  *
  * <h3>Directory paths</h3>
  *
  * The INI file can specify a number of file and directory names, and
  * itself needs a number of files and directories to read the preferences
  * properly. OpFolderManager is initialized with the default values by
  * the global initialization system, and then PrefsCollectionFiles will
  * read all the directories and update OpFolderManager accordingly. Since
  * PrefsCollectionFiles is initialized first, all collections can access
  * anything that require paths to be fully configured.
  *
  * Please note, however, that PrefsCollectionFiles will not itself retain
  * the directory paths read from the INI file, but relies on
  * OpFolderManager doing so. It is possible, however, to use
  * PrefsCollectionFiles to update the user-configurable paths from inside
  * the program. The API will take care of updating OpFolderManager when
  * that you do so.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionFiles : public OpPrefsCollectionWithHostOverride
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
	static PrefsCollectionFiles *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionFiles();

#include "modules/prefs/prefsmanager/collections/pc_files_h.inl"

	// Read ------
	/**
	  * Read a file preference.
	  *
	  * @param which Selects the file to retrieve.
	  * @param target Variable where the value will be stored.
	  * @param host Host context to retrieve the preference for. NULL will
	  * retrieve the default context.
	  */
	void GetFileL(filepref which, OpFile &target, const uni_char *host = NULL) const;

	/** @overload */
	inline void GetFileL(filepref which, OpFile &target, const ServerName *host) const
	{
		GetFileL(which, target, host ? host->UniName() : NULL);
	}

	/**
	 * Read a file preference. This method will return a pointer to internal
	 * data. The object is only guaranteed to live until the next call to any
	 * Write method.
	 *
	 * @param which Selects the file to retrieve.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline const OpFile *GetFile(filepref which, const uni_char *host = NULL) const
	{
		return GetFileInternal(which, host, NULL);
	}

	/** @overload */
	inline const OpFile *GetFile(filepref which, const ServerName *host) const
	{
		return GetFile(which, host ? host->UniName() : NULL);
	}

#ifdef _LOCALHOST_SUPPORT_
	/**
	  * Read a file preference as a URL. This method will return a URL string
	  * for the requested file preference.
	  *
	  * @param which Selects the file to retrieve.
	  * @param target Variable where the URL will be stored.
	  * @param host Host context to retrieve the preference for. NULL will
	  * retrieve the default context.
	  */
	void GetFileURLL(filepref which, OpString *target, const uni_char *host = NULL) const;

	/** @overload */
	inline void GetFileURLL(filepref which, OpString *target, const ServerName *host) const
	{
		GetFileURLL(which, target, host ? host->UniName() : NULL);
	}

	/** @overload */
	void GetFileURLL(filepref which, OpString *target, const URL &host) const;
#endif // _LOCALHOST_SUPPORT_

#ifdef LOCAL_CSS_FILES_SUPPORT
	/** Get number of local user style sheets. */
	inline unsigned int GetLocalCSSCount()
	{ return m_localCSSFileList.GetCount(); }

	/** Get name of a local user style sheet.
	  *
	  * @param index Number of the local user style sheet to retrieve.
	  * @return The name of the file, or NULL if not available.
	  */
	const uni_char *GetLocalCSSName(unsigned int index);

	/** Check if a local user style sheet is active.
	  *
	  * @param index Number of the local user style sheet to retrieve.
	  * @return TRUE if the file is active.
	  */
	BOOL IsLocalCSSActive(unsigned int index);

	/** Retrieve a local user style sheet object.
	  *
	  * @param index Number of the local user style sheet to retrieve.
	  * @return The file object, or NULL if not available.
	  */
	OpFile *GetLocalCSSFile(unsigned int index);
#endif // LOCAL_CSS_FILES_SUPPORT

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Check if file preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsPreferenceOverridden(filepref which, const uni_char *host) const;
	BOOL IsPreferenceOverridden(filepref which, const URL &host) const;
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host);
#endif

	// Defaults ------
	/**
	 * Get default value for a file preference.
	 *
	 * @param which Selects the file to retrieve.
	 * @param folder (Output) Receives the default folder for the file.
	 * @param filename (Output) Receives the default filename for the file.
	 */
	void GetDefaultFilePref(filepref which, OpFileFolder *folder,
	                        const uni_char **filename);

	/**
	 * Get default value for a directory preference.
	 *
	 * @param which Selects the directory to retrieve.
	 * @param parent[out]
	 *   Receives the parent folder for the file, or OPFILE_ABSOLUTE_FOLDER
	 *   if it as absolute path.
	 * @param foldername[out] Receives the default folder name for the file.
	 */
	void GetDefaultDirectoryPref(OpFileFolder which, OpFileFolder *parent,
	                             const uni_char **foldername);

	/**
	 * Get default value for a directory preference as a full path.
	 *
	 * @param which Selects the directory to retrieve.
	 * @param folderpath[out] Receives the default folder full path for the file.
	 */
	void GetDefaultDirectoryPrefL(OpFileFolder which, OpString &folderpath);

#ifdef PREFS_WRITE
	// Write ------
# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden file preferences.
	  *
	  * @param host Host to write an override for.
	  * @param which The preference to override.
	  * @param source The file object for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS OverridePrefL(const uni_char *host, filepref which,
	                        const OpFile *source, BOOL from_user);
# endif // PREFS_HOSTOVERRIDE

	/** Write a file preference.
	  *
	  * @param which The file preference to write.
	  * @param source The file object to write.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS WriteFilePrefL(filepref which, const OpFile *source);

	/** Write a directory to the preferences. This will update the list of
	  * folders in OpFolderManager.
	  *
	  * Please note that to retrieve the folders, you need to use
	  * OpFolderManager's interface.
	  *
	  * @param which The directory to change.
	  * @param path The new directory for this one.
	  */
	void WriteDirectoryL(OpFileFolder which, const uni_char *path);

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value);
#  ifdef PREFS_HOSTOVERRIDE
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user);
	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user);
#  endif
# endif
#endif // PREFS_WRITE

#ifdef LOCAL_CSS_FILES_SUPPORT
	/** Write activation status for a local user style sheet.
	  *
	  * @param index Number of the local user style sheet to update.
	  * @param active Whether to activate or not.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	OP_STATUS WriteLocalCSSActiveL(unsigned int index, BOOL active);
#endif // LOCAL_CSS_FILES_SUPPORT

#ifdef PREFS_WRITE
	// Reset ------
	/** Reset a file preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	BOOL ResetFileL(filepref which);

# ifdef PREFS_READ
	/** Reset a directory preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	BOOL ResetDirectoryL(OpFileFolder which);
# endif // PREFS_READ
#endif // PREFS_WRITE

	// Fetch preferences from file ------
	virtual void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info);
#ifdef PREFS_HOSTOVERRIDE
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
#endif // PREFS_HOSTOVERRIDE

	// Update ------
#if defined PREFS_READ && defined PREFS_WRITE
	/**
	 * Update file names that are relative to the path
	 */
	void SetStorageFilenamesL(OpFileFolder folder);
#endif

	// Listeners ------
	/** Register a listener. The listener will be informed every time a
	  * file preference is changed in this collection.
	  *
	  * Will leave if the registration fails for any reason.
	  * If the listener is already registered, nothing will happen.
	  *
	  * @param listener The listener object to register.
	  */
	void RegisterFilesListenerL(PrefsCollectionFilesListener *listener);

	/** Unregister a listener. The listener will no longer be informed of
	  * changes.
	  *
	  * Will leave if the unregistration fails for any reason.
	  * If the listener is not registered, nothing will happen.
	  *
	  * @param listener The listener object to unregister.
	  */
	void UnregisterFilesListener(PrefsCollectionFilesListener *listener);

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	virtual unsigned int GetNumberOfPreferences() const;
	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const;
#endif

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionFiles(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(Files, reader),
		  m_files(NULL)
#ifdef PREFS_HAVE_STRING_API
		, m_defaultdirectories(NULL)
# ifdef FOLDER_PARENT_SUPPORT
		, m_defaultparentdirectories(NULL)
# endif
#endif
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitFiles();
			InitFolders();
#endif
		}

	OpFile **m_files;

	PREFS_STATIC struct fileprefdefault
	{
#ifdef PREFS_READ
		IniSection section;			///< Section of INI file
		const char *key;			///< Key in INI file
#endif
		OpFileFolder folder;        ///< Default folder for file
		const uni_char *filename;	///< Default file name
#ifdef PREFS_READ
		BOOL default_if_not_found;	///< Use the default file if the specified file is not found
#endif
	} m_fileprefdefault[PCFILES_NUMBEROFFILEPREFS + 1];
#ifdef PREFS_COVERAGE
	mutable int m_fileprefused[PCFILES_NUMBEROFFILEPREFS];
#endif

#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE
	OpFileFolder m_default_lang_file_folder;
	OpString m_default_lang_file_name;
#endif

#ifdef PREFS_READ
	PREFS_STATIC struct directorykey
	{
		OpFileFolder folder;
		OpPrefsCollection::IniSection section;
		const char *key;
	} m_directorykeys[PCFILES_NUMBEROFFOLDERPREFS + 1];
#endif

#ifdef PREFS_HAVE_STRING_API
	OpString *m_defaultdirectories;
# ifdef FOLDER_PARENT_SUPPORT
	OpFileFolder *m_defaultparentdirectories;
# endif
#endif

	/* FIXME: This is used by GetDefaultDirectoryPref() for the
	 * returned string of that method.  So the path returned from that
	 * method has a lifetime only until the next call to the same
	 * method.  This should really be fixed.
	 */
	OpString m_GetDefaultDirectoryPref_static_storage;

#ifdef PREFS_HOSTOVERRIDE
	virtual OpOverrideHost *CreateOverrideHostObjectL(const uni_char *, BOOL);
#endif // PREFS_HOSTOVERRIDE

	OpFile *ReadFileL(filepref);

#ifdef PREFS_VALIDATE
	// These inherited interfaces are not used.
	virtual void CheckConditionsL(int, int *, const uni_char *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); }
	virtual BOOL CheckConditionsL(int, const OpStringC &, OpString **, const uni_char *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); return FALSE; }
	void RegisterListenerL(OpPrefsListener *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); }
	void UnregisterListener(OpPrefsListener *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); }
#endif

#if defined PREFS_HOSTOVERRIDE && defined PREFS_READ
	// This is abstract in superclass, so a dummy implementation is required
	BOOL ReadAllOverridesL(OverrideHost *, PrefsSection *)
	{ OP_ASSERT(!"Invalid call"); return FALSE; };
#endif // PREFS_HOSTOVERRIDE

	/** Internal helper for GetFile */
	const OpFile *GetFileInternal(filepref which, const uni_char *host, BOOL *overridden) const;

#ifdef PREFS_HAVE_STRING_API
	/** Internal helper for GetPreferenceL */
	void GetFilePathStringL(OpString *, OpFileFolder, const uni_char *);
#endif

	/** Container element used in the list of listeners. */
	class FilesListenerContainer : public Link
	{
	public:
		/** Create a container.
		  * @param listener The object to encapsulate.
		  */
		FilesListenerContainer(PrefsCollectionFilesListener *listener)
			: Link(), m_listener(listener) {};
		/** Return the encapsulated pointer as the link id. */
		virtual const char* LinkId() { return reinterpret_cast<const char *>(m_listener); }
		/** Retrieve the encapsulated pointer. */
		inline PrefsCollectionFilesListener *GetListener() { return m_listener; }

	private:
		PrefsCollectionFilesListener *m_listener; ///< The encapsulated object
	};

	/** Helper function to find a OpPrefsListener in the list of listeners.
	  *
	  * @param which The OpPrefsListener to look for.
	  * @return The requested object, NULL if not found.
	  */
	FilesListenerContainer *GetFilesContainerFor(const PrefsCollectionFilesListener *which);

	/** Linked list of registered listeners, contains ListenerContainer
	  * objects. */
	Head m_fileslisteners;

#ifdef PREFS_WRITE
	/** Broadcast a changed file preference to all listeners.
	  *
	  * @param pref     Identity of the preference.
	  */
	virtual void BroadcastChangeL(filepref pref);
	/** Broadcast a changed directory preference to all listeners.
	  *
	  * @param pref       Identity of the preference.
	  * @param directory  Value of the preference.
	  */
	virtual void BroadcastDirectoryChangeL(OpFileFolder pref, OpFile *directory);
#endif // PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
	virtual void BroadcastOverride(const uni_char *hostname);
#endif

#ifdef LOCAL_CSS_FILES_SUPPORT
	void BroadcastLocalCSSChanged();
#endif

#ifdef LOCAL_CSS_FILES_SUPPORT
# ifdef PREFS_READ
	void ReadLocalCSSFileInfoL();
# endif

	/** Structure for local user style sheet information. */
	struct CSSFile
	{
		OpString name;	///< Name of local user style sheet.
		int nameid;		///< Name of local user style sheet (localized).
		BOOL active;	///< Whether the local user style sheet is active.
		OpFile file;	///< Local user style sheet object.

		CSSFile() : nameid(0), active(FALSE) {}
	};

	OpVector<CSSFile> m_localCSSFileList;	///< List of all local user style sheets
#endif

	// Debug code ------
#ifdef PREFS_COVERAGE
	virtual void CoverageReport(
		const struct stringprefdefault *, int,
		const struct integerprefdefault *, int);
#endif

	// Friends -----
	// Uses fileprefdefault:
	friend class OverrideHostForPrefsCollectionFiles;

#ifndef HAS_COMPLEX_GLOBALS
	void InitFiles();
	void InitFolders();
#endif

	/**
	 * Find a key in m_directorykeys field by the directory ID.
	 * @param which Directory preference ID.
	 * @return directorykey or NULL if not found
	 */
	const struct directorykey *FindDirectoryKey(OpFileFolder which) const;

#ifdef PREFS_VALIDATE
	/**
	 * Check post-read and pre-write conditions for a directory value.
	 * This gets called by ReadAllPrefsL after every read, and before every
	 * WriteDirectoryL call. If the condition fails, a new replacement value
	 * will be created and written to the outvalue pointer (a new OpString
	 * will be created which must be deleted by the caller). Will only leave
	 * if a fatal error occurs. If FALSE is returned, i.e no change was
	 * necessary, outvalue is undefined.
	 *
	 * @param which Directory preference ID.
	 * @param invalue Reference to the value to be checked.
	 * @param outvalue Cleaned-up value if change was needed.
	 * @return TRUE if a change was required.
	 */
	virtual BOOL CheckDirectoryConditionsL(OpFileFolder which, const OpStringC &invalue, OpString **outvalue);

#ifdef PREFS_CHECK_TEMPDOWNLOAD_FOLDER
	/**
	 * Verifies correctness of the OPFILE_TEMPDOWNLOAD_FOLDER.
	 * This folder path must contain a "temporary_downloads" subdirectory in the end.
	 * @see CheckDirectoryConditionsL
	 * @param invalue Reference to the value to be checked.
	 * @param outvalue Cleaned-up value if change was needed.
	 * @return TRUE if a change was required.
	 */
	BOOL CheckTempDownloadDirectoryConditionsL(const OpStringC &invalue, OpString **outvalue);
#endif // PREFS_CHECK_TEMPDOWNLOAD_FOLDER
#endif // PREFS_VALIDATE

};

/**
  * Abstract interface implemented by all classes that wish to listen to
  * changes in PrefsCollectionFiles.
  *
  * To do that, make your class inherit this one, and call
  * PrefsCollectionFiles::RegisterFileListenerL(this)
  */
class PrefsCollectionFilesListener
{
public:
	/** Standard destructor. Needs to be virtual due to inheritance. */
	virtual ~PrefsCollectionFilesListener() {}

	/** Signals a change in a file preference.
	  *
	  * @param pref     Identity of the preference.
	  * @param newvalue The new value of the preference.
	  */
	virtual void FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue) {};

	/** Signals a change in a directory preference.
	  *
	  * @param pref     Identity of the preference.
	  * @param newvalue The new value of the preference.
	  */
	virtual void DirectoryChangedL(OpFileFolder pref, const OpFile *newvalue) {};

#ifdef PREFS_HOSTOVERRIDE
	/** Signals the addition, change or removal of a host override.
	  *
	  * @param hostname The affected host name.
	  */
	virtual void FileHostOverrideChanged(const uni_char *hostname) {}
#endif

#ifdef LOCAL_CSS_FILES_SUPPORT
	/** Signals the addition, change or removal of a local user style sheet. */
	virtual void LocalCSSChanged() {}
#endif
};

#endif // PC_FILES_H
