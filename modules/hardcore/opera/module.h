/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_OPERA_MODULE_H
#define MODULES_HARDCORE_OPERA_MODULE_H

#include "modules/util/opfile/opfolder.h"
#include "modules/util/adt/opvector.h"
#include "modules/hardcore/opera/components.h"

class PrefsFile;

/**
 * OperaInitInfo shall be filled in by the platform
 * before starting Opera. All modules will see this'
 * information and can use it during the startup phase.
 */

class OperaInitInfo
{
private:
#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	/**
	 * Vector of paths to locale folders.
	 * Filled up using AddDefaultLocaleFolders().
	 * Retrieve using GetDefaultLocaleFolders().
	 * Get count using GetDefaultLocaleFoldersCount().
	 */
	OpAutoVector<OpString> default_locale_folders;
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

public:
#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	/**
	 * Adds paths to the locale folders in default_locale_folders.
	 *
	 * @note These paths do not necessarily have the same parent,
	 *  see default_locale_folders_parent.
	 *
	 * @note The locale folders are used e.g. by the spellchecker
	 *	module to search for available dictionaries. See
	 *	OpSpellCheckerManager::RefreshInstalledLanguages().
	 *
	 * @param folder Full path to one locale folder.
	 *
	 * @return Returns ERR_NO_MEMORY on OOM, ortherwise OK.
	 */
	OP_STATUS AddDefaultLocaleFolders(const OpStringC& folder)
		{
			if (folder.IsEmpty()) return OpStatus::OK;
			OpAutoPtr<OpString> s(OP_NEW(OpString, ()));
			RETURN_OOM_IF_NULL(s.get());
			RETURN_IF_ERROR(s->Set(folder));
			OP_STATUS status = default_locale_folders.Add(s.get());
			if (OpStatus::IsSuccess(status)) s.release();
			return status;
		}

	/**
	 * Retrieves one of the locale folders from default_locale_folders.
	 *
	 * @note These paths do not necessarily have the same parent,
	 *  see default_locale_folders_parent.
	 *
	 * @param index of the string path in the default_locale_folders vector.
	 *
	 * @return Return a constant reference to the path, or a NULL referenace.
	 */
	OpStringC& GetDefaultLocaleFolders(unsigned int index) const
		{ return *default_locale_folders.Get(index); }

	/**
	 * Retrieves the count of locale folders.
	 *
	 * @return Return the number of elements in default_locale_folders.
	 */
	unsigned int GetDefaultLocaleFoldersCount() const
		{ return default_locale_folders.GetCount(); }

# ifdef FOLDER_PARENT_SUPPORT
	/**
	 * All locale folders as specified in the vector
	 * default_locale_folders may have one common parent, which is
	 * specified by this attribute.
	 *
	 * @note Use OPFILE_ABSOLUTE_FOLDER if there is no common parent
	 *  folder and specify absolute paths in default_locale_folders.
	 */
	OpFileFolder default_locale_folders_parent;
# endif // FOLDER_PARENT_SUPPORT
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	/**
	 * Array of full paths to the default locations of folders.
	 *
	 * Note: OPFILE_ABSOLUTE_FOLDER and OPFILE_SERIALIZED_FOLDER
	 * shall not be set.
	 */
	OpString default_folders[OPFILE_FOLDER_COUNT];

#ifdef FOLDER_PARENT_SUPPORT
	/**
	 * Array of parents for the default folders.
	 *
	 * Note: OPFILE_ABSOLUTE_FOLDER and OPFILE_SERIALIZED_FOLDER
	 * shall not be set.
	 */
	OpFileFolder default_folder_parents[OPFILE_FOLDER_COUNT];
#endif // FOLDER_PARENT_SUPPORT

#ifdef PREFS_READ
	/**
	 * A prefs reader object which can be an instance
	 * of PrefsFile or PrefsNullReader for example.
	 */
	PrefsFile* prefs_reader;
#endif // PREFS_READ

#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE
	/**
	 * Default language file folder. This will be used by the preference
	 * system if the language file is not specified in the ini file.
	 */
	OpFileFolder def_lang_file_folder;
	/**
	 * Default language file name. This will be used by the preference
	 * system if the language file is not specified in the ini file.
	 */
	const uni_char *def_lang_file_name;
#endif // LANGUAGE_FILE_SUPPORT

#ifdef INIT_BLACKLIST
	/**
	 * Enabled by the tweak TWEAK_HC_INIT_BLACKLIST.
	 * Set this to decide dynamically if the black list
	 * is to be used or not. If the black list is enabled
	 * the InitL() function will not be run for the modules
	 * in the list.
	 */
	BOOL use_black_list;
#endif // INIT_BLACKLIST
};

/**
 * OperaModule is an interface which all modules may implement.
 * If a module named foo choose to implement this interface it will have
 * to define FOO_MODULE_REQUIRED in modules/foo/foo_module.h.
 *
 * Note: This interface does not have a virtual destructor because
 * it is not needed. Delete is never called on the base class but
 * on the subclasses only.
 */

class OperaModule
{
public:
	/**
	 * This destructor doesn't need to be virtual, but we don't like warnings;
	 * especially not thousands of them.
	 */
	virtual ~OperaModule() {}

	/**
	 * Initializes the module and allocate resources.
	 *
	 * The module should only initialize itself if it recognizes the component
	 * type as one in which it knows it belongs. In particular, in any unfamiliar
	 * component type, this method should perform no operations. The component
	 * type can be extracted from g_opera->GetComponentType().
	 *
	 * @param info Initialization information supplied by the platform.
	 */
	virtual void InitL(const OperaInitInfo& info) = 0;

	/**
	 * Destroys the module and release all resources.
	 */
	virtual void Destroy() = 0;

	/**
	 * Tells the module to try to free cached data.
	 *
	 * @param toplevel_context FALSE means we could potentially be inside a
	 *        new() call or such. TRUE means the stack is "safe".
	 * @return TRUE if an attempt to free data was made.
	 */
	virtual BOOL FreeCachedData(BOOL toplevel_context) { return FALSE; }
};

#endif // !MODULES_HARDCORE_OPERA_MODULE_H
