/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_OPFILE_OPFOLDER_H
#define MODULES_UTIL_OPFILE_OPFOLDER_H

enum OpFileFolder
{
	OPFILE_ABSOLUTE_FOLDER,		///< Special folder to use when the filename is an absolute path.
	OPFILE_SERIALIZED_FOLDER,	///< Special folder to use when serializing a file name.
	OPFILE_TEMP_FOLDER,			///< Read-Write: Location of temporary files (periodically deleted files).
	OPFILE_RESOURCES_FOLDER,	///< Read-Only: Location of installed resources files for Opera (aka "read directory").
								///< /usr/share/opera/ on Linux.
	OPFILE_INI_FOLDER,			///< Read-Only: Location of read-only default INI files for Opera
								///< (dialog.ini, keyboard.ini, standard_menu.ini etc)
	OPFILE_STYLE_FOLDER,		///< Read-Only: Location of style sheets for internal documents, WML, mail messages, etc.
	OPFILE_USERSTYLE_FOLDER,	///< Read-Only: Location of the default user style sheets.
#ifdef PREFS_USE_CSS_FOLDER_SCAN
	OPFILE_USERPREFSSTYLE_FOLDER,///< Read-Write: Location of the user style sheets copied to user profile.
#endif // PREFS_USE_CSS_FOLDER_SCAN
#ifdef SESSION_SUPPORT
	OPFILE_SESSION_FOLDER,		///< Read-Write: Location of Opera sessions.
#endif // SESSION_SUPPORT
	OPFILE_HOME_FOLDER,			///< Read-Write: Location of user profile (aka "home folder" or "write directory").
#ifdef UTIL_HAVE_LOCAL_HOME_FOLDER
	OPFILE_LOCAL_HOME_FOLDER,	///< Read-Write: Location of user profile (aka "home folder" or "write directory") that is not part of the roaming profile.
#endif // UTIL_HAVE_LOCAL_HOME_FOLDER
	OPFILE_USERPREFS_FOLDER,	///< Read-Write: Location of the user preferences (INI) file.
								///< Often the same as OPFILE_HOME_FOLDER.
#ifdef PREFSFILE_CASCADE
	OPFILE_GLOBALPREFS_FOLDER,	///< Read-Only: Location of the global default INI file.
	OPFILE_FIXEDPREFS_FOLDER,	///< Read-Only: Location of the fixed default INI file.
#endif
	OPFILE_CACHE_FOLDER,		///< Read-Write: Location of the cache.
	OPFILE_COOKIE_FOLDER,           ///< Read-Write: Location of cookie jar files.

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	OPFILE_OCACHE_FOLDER,		///< Read-Write: Location of the operator or help cache.
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_OPERATOR_CACHE_MANAGEMENT
	OPFILE_LANGUAGE_FOLDER,		///< Read-Only: Location of localized files for the current language,
								///< such as the language file or localized search.ini.
#ifdef OPFILE_HAVE_GENERIC_LANGUAGE_FOLDER
	OPFILE_GENERIC_LANGUAGE_FOLDER, ///< Read-Only: Location of generic versions of localized files,
									///< e.g English language file or search.ini.
#endif
	OPFILE_IMAGE_FOLDER,		///< Read-Only: Location of "internal images", for instance: folder icons, file icons, etc.
								///< FIXME: This folder should be removed (internal images are now referenced from CSS only)
#ifdef OPFILE_HAVE_SCRIPT_FOLDER
	OPFILE_SCRIPT_FOLDER,		///< Read-Only: Location of internal scripts, used in opera generated pages.
#endif
#ifdef UTIL_VPS_FOLDER
	OPFILE_VPS_FOLDER,          ///< Read-Write: Location of visited pages index files.
#endif // UTIL_VPS_FOLDER
#ifdef _FILE_UPLOAD_SUPPORT_
	OPFILE_UPLOAD_FOLDER,		///< Read-Write: Preferred location of uploads.
#endif // _FILE_UPLOAD_SUPPORT_

#ifdef SKIN_SUPPORT
	OPFILE_SKIN_FOLDER,			///< Read-Write: Location of downloaded skins.
	OPFILE_DEFAULT_SKIN_FOLDER,	///< Read-Only: Location of default skins.
#endif // SKIN_SUPPORT

#ifdef WEBSERVER_SUPPORT
	OPFILE_WEBSERVER_FOLDER,	///< Read-Write: Web server content, and location of files posted to web server.
	OPFILE_UNITE_PACKAGE_FOLDER,///< Read-Only: Location of package containing unite services to be installed on first run.
#endif // WEBSERVER_SUPPORT

#ifdef _FILE_UPLOAD_SUPPORT_
	OPFILE_OPEN_FOLDER,			///< Read-Write: Remembered location for File - Open.
	OPFILE_SAVE_FOLDER,			///< Read-Write: Remembered lcoation for File - Save.
#endif // _FILE_UPLOAD_SUPPORT_

#ifdef M2_SUPPORT
	OPFILE_MAIL_FOLDER,			///< Read-Write: Location of user mail.
#endif // M2_SUPPORT

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	OPFILE_WEBFEEDS_FOLDER,		///< Read-Write: Location of webfeeds store
#elif defined WEBFEEDS_BACKEND_SUPPORT
	OPFILE_WEBFEEDS_FOLDER,		///< Read-Write: Location of webfeeds store
#endif // WEBFEEDS_BACKEND_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
	OPFILE_JSPLUGIN_FOLDER,		///< Read-Write: Location of JS plugins.
#endif // JS_PLUGIN_SUPPORT

#ifdef PREFS_FILE_DOWNLOAD
    OPFILE_PREFSLOAD_FOLDER,	///< Read-Write: Location of downloaded prefs files.
#endif // PREFS_FILE_DOWNLOAD

#ifdef GADGET_SUPPORT
	OPFILE_GADGET_FOLDER,			///< Read-Write: Location of downloaded widgets.
	OPFILE_GADGET_PACKAGE_FOLDER,	///< Read-Only: Location of package containing widgets to be installed on first run.
#endif // GADGET_SUPPORT

#ifndef NO_EXTERNAL_APPLICATIONS
	OPFILE_TEMPDOWNLOAD_FOLDER,	///< Read-Write: Location of temporary downloaded files
#endif // !NO_EXTERNAL_APPLICATIONS

#ifdef _BITTORRENT_SUPPORT_
	OPFILE_BITTORRENT_METADATA_FOLDER,	///< Read-Write: Location of bittorrent metadata files ("bt_metadata" is the suggested name)
#endif // _BITTORRENT_SUPPORT_

	OPFILE_SECURE_FOLDER,	///< Read-Write: Location of secure informations (e.g. wand data)

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
	OPFILE_TRUSTED_CERTS_FOLDER,       ///< Read-Only: Location of   trusted CA certs, used by libcrypto.
	OPFILE_UNTRUSTED_CERTS_FOLDER,     ///< Read-Only: Location of untrusted CA certs, used by libcrypto.
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
#ifdef SELFTEST
	OPFILE_SELFTEST_DATA_FOLDER,	///< Read-Only: Location of files used by selftests
#endif // SELFTEST

#ifdef UTIL_HAVE_DOWNLOAD_FOLDER
	OPFILE_DOWNLOAD_FOLDER,
#endif // UTIL_HAVE_DOWNLOAD_FOLDER

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OPFILE_DICTIONARY_FOLDER,	///< Location of Hunspell dictionaries
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef CORE_THUMBNAIL_SUPPORT
	OPFILE_THUMBNAIL_FOLDER,	///< Read-Write: Location of the thumbnail cache
#endif // CORE_THUMBNAIL_SUPPORT

#ifdef UTIL_HAVE_UI_INI_FOLDER
	OPFILE_UI_INI_FOLDER,				// Read-Only: folder for all ui files if using a seperate folder from OPFILE_INI_FOLDER
#endif // UTIL_HAVE_UI_INI_FOLDER

#ifdef UTIL_HAVE_CUSTOM_FOLDERS
	OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER,		///< Read-Only: Location of installed custom resources files for Opera (aka "read directory").
	OPFILE_INI_CUSTOM_PACKAGE_FOLDER,			///< Read-Only: Location of read-only custom default files for Opera except ui files
	OPFILE_LOCALE_CUSTOM_PACKAGE_FOLDER,		///< Read-Only: Location of root folder for custom language files
	OPFILE_UI_INI_CUSTOM_PACKAGE_FOLDER,		///< Read-Only: folder for all custom ui files if using a seperate folder from OPFILE_INI_FOLDER

	OPFILE_RESOURCES_CUSTOM_FOLDER,		///< Read-Only: Location of installed custom resources files for Opera (aka "read directory").
	OPFILE_UI_INI_CUSTOM_FOLDER,		///< Read-Only: folder for all custom ui files if using a seperate folder from OPFILE_INI_FOLDER
	OPFILE_INI_CUSTOM_FOLDER,			///< Read-Only: Custom folder in the prefs for the default files (e.g. search.ini, feedreaders.ini)
#endif // UTIL_HAVE_CUSTOM_FOLDERS

#ifdef UTIL_HAVE_SKIN_CUSTOM_FOLDERS
	OPFILE_SKIN_CUSTOM_PACKAGE_FOLDER,			///< Read-Only: Location of custom default skins.
	OPFILE_SKIN_CUSTOM_FOLDER,			///< Read-Only: Location of custom default skins.
#endif // UTIL_HAVE_SKIN_CUSTOM_FOLDERS


#ifdef MEDIA_BACKEND_GSTREAMER
	OPFILE_GSTREAMER_FOLDER, ///< Read-Only: Location of GStreamer library/plugins.
#endif // MEDIA_BACKEND_GSTREAMER

#ifdef NS4P_COMPONENT_PLUGINS
	OPFILE_PLUGINWRAPPER_FOLDER, ///< Read-Only: Location of Plugin Wrapper binaries.
#endif // NS4P_COMPONENT_PLUGINS

#ifdef DOM_JIL_API_SUPPORT
	OPFILE_DEVICE_SETTINGS_FOLDER, ///< Read-Only: Location of the device settings file.
#endif // DOM_JIL_API_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	OPFILE_APP_CACHE_FOLDER, ///< Read-Write: Location of the application cache.
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEB_HANDLERS_SUPPORT
	OPFILE_HANDLERS_FOLDER, ///< Read-Write: Location of the protocol handlers white or black list.
#endif // WEB_HANDLERS_SUPPORT

	OPFILE_VIDEO_FOLDER,	///< Read-Only: Location of videobackend library

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	OPFILE_PERSISTENT_STORAGE_FOLDER, ///< Read-Write: Location of the pstorage folder, the folder used
	                                  ///< for the persistent storage APIs (Web Storage, Web SQL Databases and Indexed DB).
                                      ///< This folder should be the same as OPFILE_HOME_FOLDER,
                                      ///< unless the platform has specific reasons to place it elsewhere.
                                      ///< This applies only to the main browsing context.
                                      ///< Other contexts (app cache, extensions, widgets, etc...)
                                      ///< have the data stored inside their respective profile folders.
#endif // DATABASE_MODULE_MANAGER_SUPPORT

	// Include product specific folders.
#ifdef PRODUCT_OPFOLDER_FILE
#include PRODUCT_OPFOLDER_FILE
#endif // PRODUCT_OPFOLDER_FILE

	OPFILE_FOLDER_COUNT
#ifdef DYNAMIC_FOLDER_SUPPORT
	/* The compiler is allowed to chose, for each enum separately, any integral
	 * type that suffices to represent that enum; which means it might chose a
	 * type with only just enough bits (six, at present; unlikely, but if we add
	 * enough dynamic folders, we might wrap a signed 8-bit type) to represent
	 * all of the values it knows about - but dynamic folder support presumes
	 * that we can add as many members to this type as we like, which might wrap
	 * the type used for implementation.  So be sure to include a member which
	 * is big enough to ensure we have a type big enough for our needs. */
	// Someone needs to decide how big we want this: perhaps a tweak ?
	, OPFILE_FOLDER_BIG_ENOUGH = 127 // Using 127 here makes no difference.
#endif
};

#if defined(DYNAMIC_FOLDER_SUPPORT) || defined(FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT)
#include "modules/util/adt/opvector.h"
#endif // DYNAMIC_FOLDER_SUPPORT || FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

class OpFolderManager
{
public:
	/**
	 * Second phase constructor.
	 */
	void InitL();

	/**
	 * Sets the path for a OpFileFolder.
	 * A PATHSEPCHAR will be appened if it does not already end on one.
	 *
	 * If FOLDER_PARENT_SUPPORT is defined, the parent parameter
	 * means the folder will be treated as a child to parent and
	 * will follow when the parent is moved. OPFILE_ABSOLUTE_FOLDER
	 * as a parent means no parent specified.
	 * If parent != OPFILE_ABSOLUTE_FOLDER, path should be relative to the parent,
	 * not an absolute path.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS SetFolderPath(OpFileFolder folder, const uni_char* path, OpFileFolder parent=OPFILE_ABSOLUTE_FOLDER);

	/**
	 * True precisely if the input is a real OpFileFolder value.
	 */
	BOOL IsValid(OpFileFolder folder)
	{
		/* The enum above may be implemented as an unsigned type, since it has
		 * no negative members - in which case, we get compiler warnings (about
		 * redundant comparison) if we test it >= 0 - but it could equally be
		 * implemented as a signed integral type ... so we need a type-agnostic
		 * way to test that it's >= 0.  Annoyingly, (ANSI X3.159-1989, 3.2.1.2)
		 * casting unsigned to signed is implementation-defined (whereas the
		 * reverse is well-defined); so (int)folder >= 0 isn't robust.  We can,
		 * however, do things the hard way: work out what bit would be the sign
		 * bit if folder were of a signed type; and test if that is set. */
		const unsigned int sign = 1u << (sizeof(OpFileFolder) * CHAR_BIT - 1);
		return !(sign & static_cast<unsigned int>(folder)) && CountPaths() > static_cast<UINT32>(folder);
	}

	/**
	 * Returns the path for a OpFileFolder in 'ret_path'.
	 *
	 * If this method fails (returns an non-success status code), the
	 * content of 'ret_path' is undefined.
	 */
	OP_STATUS GetFolderPath(OpFileFolder folder, OpString & ret_path);

	/**
	 * This method is unsafe and should preferably not be used at all.
	 * It is likely to become deprecated in the not-so-distant future.
	 * Every use of this method indicates a potential bug in opera and
	 * should be fixed.  However, it is significantly safer than the
	 * old, single-parameter GetFolderPath() method that it mimics.
	 *
	 * It is intended to be used to simplify porting from the old,
	 * completely unsafe single-parameter GetFolderPath() in cases
	 * where the code didn't handle errors at all.  This method is
	 * designed to behave as similarly as possible to the old method.
	 * The only difference between this method and the one it replaces
	 * is that the caller controls the lifetime of the returned path
	 * (by keeping 'storage' alive).
	 *
	 * (The other advantage of this method is that it is easy to find
	 * uses of it with grep.)
	 *
	 * The data pointed to by the return value is only valid as long
	 * as 'storage' is left untouched (i.e. the return value may point
	 * at some data inside 'storage').
	 *
	 * The content of 'storage' should always be considered undefined
	 * after having been passed to this method.
	 *
	 * This method returns 0 if there is an error.  (Although it could
	 * possibly return 0 also when there is no error).
	 */
	const uni_char * GetFolderPathIgnoreErrors(OpFileFolder folder, OpString & storage);

#ifdef DYNAMIC_FOLDER_SUPPORT
	/**
	 * Adds a new dynamic folder. The result value is not in the
	 * OpFileFolder-enum but can still be used to represent folders
	 * just like the static ones.
	 *
	 * @param parent Base directory for the new folder.
	 *               This can be a static or dynamic folder.
	 * @param path Path which is appended on the folder path of
	 *             the parent folder.
	 * @param result A new OpFileFolder value.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddFolder(OpFileFolder parent, const uni_char* path, OpFileFolder* result);
#endif // DYNAMIC_FOLDER_SUPPORT

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	/**
	 * Returns the number of locale folders that were added via
	 * AddLocaleFolder().
	 * @see GetLocaleFolder()
	 */
	UINT32 GetLocaleFoldersCount() const { return m_locale_folders.GetCount(); }

	/**
	 * Returns the enum of the locale folder with the specified
	 * index. The returned value can e.g. be used as an argument for
	 * GetFolderPath().
	 * @param index is the index of the locale folder to return. The
	 *  index must be smaller than GetLocaleFoldersCount().
	 * @see AddLocaleFolder()
	 */
	OpFileFolder GetLocaleFolder(UINT32 index) const
	{
		OP_ASSERT(index < GetLocaleFoldersCount());
		return static_cast<OpFileFolder>(m_locale_folders.Get(index));
	}

	/** Look up name of locale folder at specified index.
	 *
	 * @param index The index of the locale folder to return; must be smaller
	 *  than GetLocaleFoldersCount().
	 * @param[out] ret_path Will be set to the indicated folder.
	 * @see AddLocaleFolder()
	 */
	OP_STATUS GetLocaleFolderPath(UINT32 index, OpString & ret_path)
	{
		OP_ASSERT(index < GetLocaleFoldersCount());
		return GetFolderPath(GetLocaleFolder(index), ret_path);
	}

	/**
	 * Call this method to add the specified path to the list of
	 * locale folders.
	 * @param parent is the base directory for the new folder. This
	 *  can be a static or dynamic folder. Use OPFILE_ABSOLUTE_FOLDER
	 *  if you specify an absolute path.
	 * @param path is the path (relative to the specified parent
	 *  folder) of the locale path to add.
	 * @return OpStatus::OK on success, an error status if the
	 *  specified path could not be added.
	 *
	 * @note The locale folders are used e.g. by the spellchecker
	 *  module to search for available dictionaries. See
	 *  OpSpellCheckerManager::RefreshInstalledLanguages().
	 *
	 * @note This method does not return the folder-id of the added
	 *  path, but it is possible to get that id on calling
	 *  GetLocaleFolder().
	 *
	 * Example to add the directories \c "/usr/share/locale" and \c
	 * "/usr/share/locale/dictionaries", \c "/usr/share/locale/foo" to
	 * the lost of locale folders:
	 * \code
	 * OpFolderManager* folder_manager = ...;
	 * RETURN_IF_ERROR(folder_manager->AddLocaleFolder(OPFILE_ABSOLUTE_FOLDER, UNI_L("/usr/share/locale")));
	 * OpFileFolder parent_folder = folder_manager->GetLocaleFolder(folder_manager->GetLocaleFoldersCount()-1);
	 * RETURN_IF_ERROR(folder_manager->AddLocaleFolder(parent_folder, UNI_L("dictionaries")));
	 * RETURN_IF_ERROR(folder_manager->AddLocaleFolder(parent_folder, UNI_L("foo")));
	 * \endcode
	 *
	 * @see GetLocaleFoldersCount()
	 * @see GetLocaleFolder()
	 */
	OP_STATUS AddLocaleFolder(OpFileFolder parent, const uni_char* path);
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

private:

#ifdef DYNAMIC_FOLDER_SUPPORT
	OpAutoVector<OpString> m_paths;
	UINT32 CountPaths() const { return m_paths.GetCount(); }
	OpString* GetPath(OpFileFolder folder) { return m_paths.Get(folder); }
#else // DYNAMIC_FOLDER_SUPPORT
	OpString m_paths[OPFILE_FOLDER_COUNT];
	UINT32 CountPaths() const { return OPFILE_FOLDER_COUNT; }
	OpString* GetPath(OpFileFolder folder) { return &m_paths[folder]; }
#endif // DYNAMIC_FOLDER_SUPPORT

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	OpINT32Vector m_locale_folders;
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

#ifdef FOLDER_PARENT_SUPPORT
# ifdef DYNAMIC_FOLDER_SUPPORT
	OpINT32Vector m_parents;
	OpFileFolder GetParent(OpFileFolder folder) const { return static_cast<OpFileFolder>(m_parents.Get(folder)); }
	OP_STATUS SetParent(OpFileFolder folder, OpFileFolder parent) { return m_parents.Replace(folder, parent); }
# else // DYNAMIC_FOLDER_SUPPORT
	OpFileFolder m_parents[OPFILE_FOLDER_COUNT];
	OpFileFolder GetParent(OpFileFolder folder) const { return m_parents[folder]; }
	OP_STATUS SetParent(OpFileFolder folder, OpFileFolder parent) { m_parents[folder] = parent; return OpStatus::OK; }
# endif // DYNAMIC_FOLDER_SUPPORT
#endif // FOLDER_PARENT_SUPPORT
};

#endif // !MODULES_UTIL_OPFILE_OPFOLDER_H
