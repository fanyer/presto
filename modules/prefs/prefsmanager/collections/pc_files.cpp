/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/style/cssmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/handy.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/url2.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "modules/prefs/prefsmanager/prefsinternal.h"
#include "modules/locale/locale-enum.h"

#if defined PREFS_USE_CSS_FOLDER_SCAN && defined PREFS_USE_USERSTYLE_INI
# error "You cannot combine PREFS_USE_CSS_FOLDER_SCAN and PREFS_USE_USERSTYLE_INI"
#endif
#if defined PREFS_HAVE_HOTLIST && defined PREFS_HAVE_BOOKMARKS_FILE
# error "You cannot combine API_PRF_FILE_HOTLIST and API_PRF_BOOKMARKS_FILE"
#endif

#include "modules/prefs/prefsmanager/collections/pc_files_c.inl"

// OverrideHostForPrefsCollectionFiles implementation ---------------------

#ifdef PREFS_HOSTOVERRIDE
/** Class describing a particular host that has overridden file preferences.
  */
class OverrideHostForPrefsCollectionFiles : public OpOverrideHost
{
public:
	/** First-phase constructor. */
	OverrideHostForPrefsCollectionFiles()
		: m_fileoverrides(NULL) {}
	/** Second-phase constructor.
	  *
	  * @param host Host for which this override is valid.
	  * @param from_user Whether override was specified by the user.
	  */
	virtual void ConstructL(const uni_char *host, BOOL from_user);

	virtual ~OverrideHostForPrefsCollectionFiles();

# ifdef PREFS_WRITE
	/** Write a new override for this host.
	  *
	  * @param reader Reader to write the information to.
	  * @param pref Pointer to the entry in the fileprefdefault structure
	  *             for this preference. Used to get the strings to write to
	  *             to the INI file.
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in PrefsCollectionFiles). Used for lookup.
	  * @param from_user Whether the override comes from the user or not. If
	  *                  it doesn't, and there is one that does, the current
	  *                  overridden value is not changed.
	  * @param value Value for the override.
	  */
	OP_STATUS WriteOverrideL(
		PrefsFile *reader,
		const struct PrefsCollectionFiles::fileprefdefault *pref,
		int prefidx,
		const OpFile *value,
		BOOL from_user);
#endif

	/** Read an overridden file value, if one exists.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in PrefsCollectionFiles). Used for
	  *                lookup.
	  * @return A file if there was an override, or NULL.
	  */
	const OpFile *GetOverridden(int prefidx) const;

protected:
	OpFile **m_fileoverrides;		///< List of file overrides for this domain

	inline void SetFileEntry(int i, OpFile *file)
	{
		OP_DELETE(m_fileoverrides[i]);
		m_fileoverrides[i] = file;
	}

	friend class PrefsCollectionFiles;
};

void OverrideHostForPrefsCollectionFiles::ConstructL(const uni_char *host, BOOL from_user)
{
	OpOverrideHost::ConstructL(host, from_user);
	m_fileoverrides = OP_NEWA_L(OpFile*, PCFILES_NUMBEROFFILEPREFS);

	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; i ++)
	{
		m_fileoverrides[i] = NULL;
	}
}

OverrideHostForPrefsCollectionFiles::~OverrideHostForPrefsCollectionFiles()
{
	if (m_fileoverrides != NULL)
	{
		for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; i ++)
		{
			OP_DELETE(m_fileoverrides[i]);
		}

		OP_DELETEA(m_fileoverrides);
	}
}

# ifdef PREFS_WRITE
OP_STATUS OverrideHostForPrefsCollectionFiles::WriteOverrideL(
	PrefsFile *reader,
	const struct PrefsCollectionFiles::fileprefdefault *pref, int prefidx,
	const OpFile *value,
	BOOL from_user)
{
	if (reader->AllowedToChangeL(OpPrefsCollection::SectionNumberToString(pref->section), pref->key))
	{
		OpString key; ANCHOR(OpString, key);
		key.Reserve(128); // should be enough to not have to expand again
		key.SetL(OpPrefsCollection::SectionNumberToString(pref->section));
		key.AppendL("|");
		key.AppendL(pref->key);

		OP_STATUS rc;
		BOOL overwrite = from_user;
#ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
#else
		overwrite = TRUE;
#endif
		{
			rc = reader->WriteStringL(m_host, key, value->GetSerializedName());
		}
#ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = reader->WriteStringGlobalL(m_host, key, value->GetSerializedName());
			// Check if we should overwrite the current value (i.e it is
			// not overwritten internally).
			overwrite =
				reader->ReadStringL(m_host, key).Compare(value->GetSerializedName()) == 0;
		}
#endif

		// Create a new override
		if (overwrite && OpStatus::IsSuccess(rc))
		{
			// Create file override entry
			SetFileEntry(prefidx, static_cast<OpFile *>(value->CreateCopy()));
			LEAVE_IF_NULL(m_fileoverrides[prefidx]);
		}

		return rc;
	}
	else
	{
		return OpStatus::ERR_NO_ACCESS;
	}
}
# endif // PREFS_WRITE

const OpFile *OverrideHostForPrefsCollectionFiles::GetOverridden(int prefidx) const
{
	if (m_fileoverrides != NULL)
	{
		// An overridden file is defined
		return m_fileoverrides[prefidx];
	}

	return NULL;
}
#endif // PREFS_HOSTOVERRIDE

// PrefsCollectionFiles implementation ------------------------------------

PrefsCollectionFiles *PrefsCollectionFiles::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcfiles)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcfiles = OP_NEW_L(PrefsCollectionFiles, (reader));
	return g_opera->prefs_module.m_pcfiles;
}

void PrefsCollectionFiles::ReadAllPrefsL(PrefsModule::PrefsInitInfo *info)
{
	OP_ASSERT(!m_files); // Make sure we are never called twice

#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE
	m_default_lang_file_folder = info->m_init->def_lang_file_folder;
	m_default_lang_file_name.SetL(info->m_init->def_lang_file_name);
#endif

#ifdef PREFS_READ
	// Read directories
# ifdef PREFS_HAVE_STRING_API
	OpString *fld_p = m_defaultdirectories =
		OP_NEWA_L(OpString, PCFILES_NUMBEROFFOLDERPREFS);
#  ifdef FOLDER_PARENT_SUPPORT
	OpFileFolder *par_p = m_defaultparentdirectories =
		OP_NEWA_L(OpFileFolder, PCFILES_NUMBEROFFOLDERPREFS);
#  endif
# endif
	for (const directorykey *p = m_directorykeys; p < m_directorykeys + PCFILES_NUMBEROFFOLDERPREFS; ++ p)
	{
# ifdef PREFS_HAVE_STRING_API
		// Remember the default
		//(fld_p ++)->SetL(g_folder_manager->GetFolderPath(p->folder));
		(fld_p ++)->SetL(info->m_init->default_folders[p->folder]);
#  ifdef FOLDER_PARENT_SUPPORT
		*(par_p ++) = info->m_init->default_folder_parents[p->folder];
#  endif
# endif

		ANCHORD(OpString, directory);
		directory.ReserveL(_MAX_PATH);

		m_reader->ReadStringL(m_sections[p->section], p->key, directory, NULL);

# ifdef PREFS_VALIDATE
		OpString *newvalue;
		if (CheckDirectoryConditionsL(p->folder, directory, &newvalue))
		{
			// Move new value over to the current string object
			directory.TakeOver(*newvalue);
			OP_DELETE(newvalue);
		}
# endif // PREFS_VALIDATE

		if (!directory.IsEmpty())
		{
			// Allow relative paths by using OpFile's serialization support
			OpFile file; ANCHOR(OpFile, file);
			LEAVE_IF_ERROR(file.Construct(directory.CStr(), OPFILE_SERIALIZED_FOLDER));
			LEAVE_IF_ERROR(g_folder_manager->SetFolderPath(p->folder, file.GetFullPath()));
		}
	}
#endif

	// Read files
	m_files = OP_NEWA_L(OpFile *, PCFILES_NUMBEROFFILEPREFS);
	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; i ++)
	{
		m_files[i] = NULL;
	}
	for (int j = 0; j < PCFILES_NUMBEROFFILEPREFS; j ++)
	{
		m_files[j] = ReadFileL(static_cast<filepref>(j));
	}

#ifdef PREFS_COVERAGE
	op_memset(m_fileprefused, 0, sizeof (int) * PCFILES_NUMBEROFFILEPREFS);
#endif

	// Read style sheet list
#if defined PREFS_READ && defined LOCAL_CSS_FILES_SUPPORT
	ReadLocalCSSFileInfoL();
#endif
}

PrefsCollectionFiles::~PrefsCollectionFiles()
{
#ifdef PREFS_COVERAGE
	CoverageReport(NULL, 0, NULL, 0);
#endif

	if (m_files)
	{
		for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; i ++)
		{
			OP_DELETE(m_files[i]);
		}
	}
	OP_DELETEA(m_files);

#ifdef LOCAL_CSS_FILES_SUPPORT
	m_localCSSFileList.DeleteAll();
#endif // LOCAL_CSS_FILES_SUPPORT

#ifdef PREFS_HAVE_STRING_API
	OP_DELETEA(m_defaultdirectories);
# ifdef FOLDER_PARENT_SUPPORT
	OP_DELETEA(m_defaultparentdirectories);
# endif
#endif

	g_opera->prefs_module.m_pcfiles = NULL;
}

OpFile *PrefsCollectionFiles::ReadFileL(filepref i)
{
	// Setup the default file object
	OpStackAutoPtr<OpFile> defaultfile(OP_NEW_L(OpFile, ()));
	OpFileFolder def_folder;
	const uni_char *def_filename;
	GetDefaultFilePref(i, &def_folder, &def_filename);
	LEAVE_IF_ERROR(defaultfile->Construct(def_filename, def_folder));

#ifdef PREFS_READ
	const fileprefdefault *pref = &m_fileprefdefault[static_cast<int>(i)];

	// Read file
	OpStackAutoPtr<OpFile> newfile(OP_NEW_L(OpFile, ()));

	OpString name; ANCHOR(OpString, name);
	name.ReserveL(_MAX_PATH);
	m_reader->ReadStringL(m_sections[pref->section], pref->key,
	                         name, defaultfile->GetSerializedName());
	LEAVE_IF_ERROR(newfile->Construct(name.CStr(), OPFILE_SERIALIZED_FOLDER));

	// If file does not exist, revert to default for some files (even if that
	// one does not exist either!)
	BOOL exists;
	LEAVE_IF_ERROR(newfile->Exists(exists));
	if (!pref->default_if_not_found || exists)
	{
		return newfile.release();
	}
#endif

	return defaultfile.release();
}

void PrefsCollectionFiles::GetDefaultFilePref(
	filepref which, OpFileFolder *folder, const uni_char **filename)
{
	// Some file defaults must be determined at run-time
#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE
	if (which == LanguageFile)
	{
		*folder   = m_default_lang_file_folder;
		*filename = m_default_lang_file_name.CStr();
	}
	else
#endif
	{
		*folder   = m_fileprefdefault[which].folder;
		*filename = m_fileprefdefault[which].filename;
	}
}

void PrefsCollectionFiles::GetDefaultDirectoryPref(OpFileFolder which, OpFileFolder *parent,
	                             const uni_char **foldername)
{
	*parent = OPFILE_ABSOLUTE_FOLDER;

#ifdef PREFS_HAVE_STRING_API
	for (int i = 0; i < static_cast<int>(PCFILES_NUMBEROFFOLDERPREFS); ++ i)
	{
		if (m_directorykeys[i].folder == which)
		{
# ifdef FOLDER_PARENT_SUPPORT
			*parent = m_defaultparentdirectories[i];
# endif
			*foldername = m_defaultdirectories[i].CStr();
			return;
		}
	}
#endif

	// Some directories are not configurable through the preferences system,
	// so we assume they are not changed at run-time.
	// FIXME: Get folder parent
	// FIXME: Returning pointer with uncertain lifetime
	OP_STATUS err = g_folder_manager->GetFolderPath(which, m_GetDefaultDirectoryPref_static_storage);
	if (OpStatus::IsError(err))
	{
		// I believe this is what the old code would do when GetFolderPath failed.
		*foldername = 0;
	}
	else
		*foldername = m_GetDefaultDirectoryPref_static_storage.CStr();
}

void PrefsCollectionFiles::GetDefaultDirectoryPrefL(OpFileFolder which, OpString &folderpath)
{
#ifndef FOLDER_PARENT_SUPPORT
# ifdef PREFS_HAVE_STRING_API
	for (int i = 0; i < static_cast<int>(PCFILES_NUMBEROFFOLDERPREFS); ++ i)
	{
		if (m_directorykeys[i].folder == which)
		{
			folderpath.SetL(m_defaultdirectories[i].CStr());
			return;
		}
	}
# endif

	// Some directories are not configurable through the preferences system,
	// so we assume they are not changed at run-time.
	// FIXME: Get folder parent
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(which, folderpath));
#else
	OpFileFolder parent = which;
	const uni_char* name;
	folderpath.Empty();

	// Iterate through the folder's parents, inserting each parent's folder
	// path at the beginning of the path string until we reach the top.
	while (parent != OPFILE_ABSOLUTE_FOLDER)
	{
		GetDefaultDirectoryPref(parent, &parent, &name);
		if (name[uni_strlen(name) - 1] != PATHSEPCHAR)
		{
			folderpath.InsertL(0, PATHSEP);
		}
		LEAVE_IF_ERROR(folderpath.Insert(0, name));
	}
#endif
}

#if defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
OP_STATUS PrefsCollectionFiles::OverridePrefL(const uni_char *host,
                                              filepref which,
											  const OpFile *source,
											  BOOL from_user)
{
#ifdef PREFS_COVERAGE
	++ m_fileprefused[which];
#endif

	OverrideHostForPrefsCollectionFiles *override =
		static_cast<OverrideHostForPrefsCollectionFiles *>
		(FindOrCreateOverrideHostL(host, from_user));

	OP_STATUS rc =
		override->WriteOverrideL(m_overridesreader, &m_fileprefdefault[which], which,
		                         source, from_user);

	if (OpStatus::IsSuccess(rc))
	{
# ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
# endif
		{
			rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
		}
# ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
		}
# endif
	}


	if (OpStatus::IsSuccess(rc))
	{
		BroadcastOverride(host);
	}
	return rc;
}
#endif // PREFS_HOSTOVERRIDE

void PrefsCollectionFiles::GetFileL(filepref which, OpFile &target, const uni_char *host) const
{
	LEAVE_IF_ERROR(target.Copy(GetFile(which, host)));
}

const OpFile *PrefsCollectionFiles::GetFileInternal(
	filepref which, const uni_char *host, BOOL *overridden) const
{
#ifdef PREFS_COVERAGE
	++ m_fileprefused[which];
#endif

#ifdef PREFS_HOSTOVERRIDE
	if (host)
	{
		OverrideHostForPrefsCollectionFiles *override =
			static_cast<OverrideHostForPrefsCollectionFiles *>(FindOverrideHost(host));

		if (override)
		{
			// Found a matching override
			const OpFile *f;
			if ((f = override->GetOverridden(int(which))) != NULL)
			{
				// Value is overridden
				if (overridden)
					*overridden = TRUE;

				return f;
			}
		}
	}
#endif // PREFS_HOSTOVERRIDE

	if (overridden)
		*overridden = FALSE;

	return m_files[int(which)];
}

#ifdef _LOCALHOST_SUPPORT_
void PrefsCollectionFiles::GetFileURLL(filepref which, OpString *target, const uni_char *host) const
{
	const uni_char* path = GetFile(which, host)->GetFullPath();
	uni_char* escaped_path = OP_NEWA_L(uni_char, uni_strlen(path)*3 + 1);
	ANCHOR_ARRAY(uni_char, escaped_path);
	UriEscape::Escape(escaped_path , path, UriEscape::Filename);
	g_url_api->ResolveUrlNameL(escaped_path, *target);
}

void PrefsCollectionFiles::GetFileURLL(filepref which, OpString *target, const URL &host) const
{
	GetFileURLL(which, target, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif

#ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFiles::IsPreferenceOverridden(filepref which, const uni_char *host) const
{
	BOOL is_overridden;
	GetFileInternal(which, host, &is_overridden);
	return is_overridden;
}

BOOL PrefsCollectionFiles::IsPreferenceOverridden(filepref which, const URL &host) const
{
	return IsPreferenceOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif

#ifdef PREFS_HAVE_STRING_API
void PrefsCollectionFiles::GetFilePathStringL(OpString *s, OpFileFolder folder, const uni_char *filename)
{
	OpStackAutoPtr<OpFile> file(OP_NEW_L(OpFile, ()));
	LEAVE_IF_ERROR(file->Construct(filename, folder));
	s->SetL(file->GetFullPath());
}
#endif

#ifdef PREFS_HAVE_STRING_API
BOOL PrefsCollectionFiles::GetPreferenceL(IniSection section, const char *key,
	OpString &target, BOOL defval, const uni_char *host)
{
	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; ++ i)
	{
		if (m_fileprefdefault[i].section == section && 0 == op_strcmp(m_fileprefdefault[i].key, key))
		{
#ifdef PREFS_COVERAGE
			++ m_fileprefused[i];
#endif

			if (defval)
			{
				OpFileFolder def_folder;
				const uni_char *def_filename;
				GetDefaultFilePref(filepref(i), &def_folder, &def_filename);
				GetFilePathStringL(&target, def_folder, def_filename);
			}
			else
			{
				const OpFile *file = GetFile(filepref(i), host);
				if (file)
				{
					target.SetL(file->GetFullPath());
				}
				else
				{
					target.Empty();
				}
			}
			return TRUE;
		}
	}

	for (size_t j = 0; j < PCFILES_NUMBEROFFOLDERPREFS; ++ j)
	{
		if (m_directorykeys[j].section == section && 0 == op_strcmp(m_directorykeys[j].key, key))
		{
			if (defval)
			{
#ifdef FOLDER_PARENT_SUPPORT
				OpFileFolder parent = m_defaultparentdirectories[j];
				const uni_char *folder = m_defaultdirectories[j].CStr();
				GetFilePathStringL(&target, parent, folder);
				if (target[target.Length() - 1] != PATHSEPCHAR)
				{
					target.AppendL(PATHSEP);
				}
#else
				target.SetL(m_defaultdirectories[j]);
#endif
			}
			else
			{
				LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(m_directorykeys[j].folder, target));
			}
			return TRUE;
		}
	}

#if defined LOCAL_CSS_FILES_SUPPORT && !defined PREFS_USE_CSS_FOLDER_SCAN
	if (SLocalCSSFiles == section && !defval)
	{
		const char *space = op_strchr(key, ' ');
		if (space)
		{
			unsigned long index = op_strtoul(space + 1, NULL, 10);
			if (index > 0 && index <= GetLocalCSSCount())
			{
				-- index; // OpVector is zero-based

				if (0 == op_strncmp(key, "File ", 5))
				{
					target.SetL(GetLocalCSSFile(index)->GetFullPath());
					return TRUE;
				}
				else if (0 == op_strncmp(key, "Name ", 5))
				{
					target.SetL(GetLocalCSSName(index));
					return TRUE;
				}
				else if (0 == op_strncmp(key, "Active ", 7))
				{
					if (IsLocalCSSActive(index))
					{
						target.SetL("1");
					}
					else
					{
						target.SetL("0");
					}
					return TRUE;
				}
			}
		}
	}
#endif // LOCAL_CSS_FILES_SUPPORT

	return FALSE;
}
#endif

#ifdef PREFS_WRITE
OP_STATUS PrefsCollectionFiles::WriteFilePrefL(
	PrefsCollectionFiles::filepref which, const OpFile *source)
{
# ifdef PREFS_COVERAGE
	++ m_fileprefused[which];
# endif

	if (uni_strcmp(source->GetSerializedName(), m_files[which]->GetSerializedName()) == 0)
		return OpStatus::OK;

# ifdef PREFS_READ
	OP_STATUS rc = m_reader->WriteStringL(m_sections[m_fileprefdefault[which].section],
	                                      m_fileprefdefault[which].key,
	                                      source->GetSerializedName());
# else
	const OP_STATUS rc = OpStatus::OK;
# endif

	if (OpStatus::IsSuccess(rc))
	{
		// Update internal storage
		OpFile *newfile = static_cast<OpFile *>(source->CreateCopy());
		LEAVE_IF_NULL(newfile);
		OP_DELETE(m_files[which]);
		m_files[which] = newfile;

		BroadcastChangeL(which);
	}

	return rc;
}

const PrefsCollectionFiles::directorykey *PrefsCollectionFiles::FindDirectoryKey(OpFileFolder which) const
{
	const directorykey *p = NULL;
	for (p = m_directorykeys; p < m_directorykeys + PCFILES_NUMBEROFFOLDERPREFS; ++ p)
		if (which == p->folder)
			return p;
	return NULL;
}

void PrefsCollectionFiles::WriteDirectoryL(OpFileFolder which, const uni_char *path)
{
#ifdef PREFS_VALIDATE
	ANCHORD(OpString, path_str);
	path_str.Set(path);
	OpString *changed_value;
	if (!CheckDirectoryConditionsL(which, path_str, &changed_value))
		changed_value = NULL;
	ANCHOR_PTR(OpString, changed_value);
	path = (changed_value != NULL) ? changed_value->CStr() : path;
#endif // PREFS_VALIDATE

	// Use OpFile's serialization support to write this with proper support for relative paths.
	ANCHORD(OpFile, file);
	LEAVE_IF_ERROR(file.Construct(path, OPFILE_SERIALIZED_FOLDER));

#ifdef PREFS_VALIDATE
	ANCHOR_PTR_RELEASE(changed_value);
	OP_DELETE(changed_value);
#endif // PREFS_VALIDATE

	// Update the folder manager. It will leave if the property is not found.
	LEAVE_IF_ERROR(g_folder_manager->SetFolderPath(which, file.GetFullPath()));

#ifdef PREFS_READ
	// Update the INI file (if we have one)
	const directorykey *p = FindDirectoryKey(which);

	// The requested folder is not configurable at run-time
	if (p == NULL)
		LEAVE(OpStatus::ERR_NOT_SUPPORTED);

	// Found what we are looking for.
	m_reader->WriteStringL(m_sections[p->section], p->key, file.GetSerializedName());

	// Update all file names which are relative to this path (if any)
	SetStorageFilenamesL(which);
#endif // PREFS_READ

	BroadcastDirectoryChangeL(which, &file);

}

BOOL PrefsCollectionFiles::ResetFileL(filepref which)
{
# ifdef PREFS_COVERAGE
	++ m_fileprefused[which];
# endif


# ifdef PREFS_READ
	BOOL is_deleted =
		m_reader->DeleteKeyL(m_sections[m_fileprefdefault[which].section],
		                     m_fileprefdefault[which].key);
# else
	const BOOL is_deleted = TRUE;
# endif
	if (is_deleted)
	{
		OpFileFolder folder;
		const uni_char *filename;
		GetDefaultFilePref(which, &folder, &filename);

		if (uni_strcmp(filename, m_files[which]->GetSerializedName()) != 0)
		{
			// Update internal storage
			OpStackAutoPtr<OpFile> newfile(OP_NEW_L(OpFile, ()));
			LEAVE_IF_ERROR(newfile->Construct(filename, folder));
			OP_DELETE(m_files[which]);
			m_files[which] = newfile.release();

			BroadcastChangeL(which);
		}
	}

	return is_deleted;
}

# ifdef PREFS_READ
BOOL PrefsCollectionFiles::ResetDirectoryL(OpFileFolder which)
{
	const directorykey *p = NULL;
	for (p = m_directorykeys; p < m_directorykeys + PCFILES_NUMBEROFFOLDERPREFS; ++ p)
	{
		if (which == p->folder)
		{
			BOOL is_deleted = m_reader->DeleteKeyL(m_sections[p->section], p->key);
			if (is_deleted)
			{
				OpFileFolder parent;
				const uni_char *folder;
				GetDefaultDirectoryPref(which, &parent, &folder);
#   ifdef FOLDER_PARENT_SUPPORT
				LEAVE_IF_ERROR(g_folder_manager->SetFolderPath(which, folder, parent));
#   else
				LEAVE_IF_ERROR(g_folder_manager->SetFolderPath(which, folder));
#   endif
				// Update all file names which are relative to this path (if any)
				SetStorageFilenamesL(which);
			}
			return is_deleted;
		}
	}

	// The requested folder is not configurable at run-time
	LEAVE(OpStatus::ERR_NOT_SUPPORTED);
	return FALSE;
}
# endif // PREFS_READ

# ifdef PREFS_HAVE_STRING_API
BOOL PrefsCollectionFiles::WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
{
	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; ++ i)
	{
		if (m_fileprefdefault[i].section == section && 0 == STRINGAPI_STRCMP(m_fileprefdefault[i].key, key))
		{
#  ifdef PREFS_COVERAGE
			++ m_fileprefused[i];
#  endif
			OpStackAutoPtr<OpFile> new_file(OP_NEW_L(OpFile, ()));
			LEAVE_IF_ERROR(new_file->Construct(value.CStr()));
			return OpStatus::IsSuccess(WriteFilePrefL(filepref(i), new_file.get()));
		}
	}

	for (size_t j = 0; j < PCFILES_NUMBEROFFOLDERPREFS; ++ j)
	{
		if (m_directorykeys[j].section == section && 0 == STRINGAPI_STRCMP(m_directorykeys[j].key, key))
		{
			if (value.HasContent())
				WriteDirectoryL(m_directorykeys[j].folder, value.CStr());
			else
				ResetDirectoryL(m_directorykeys[j].folder);
			return TRUE;
		}
	}

#  if defined LOCAL_CSS_FILES_SUPPORT && !defined PREFS_USE_CSS_FOLDER_SCAN
	if (SLocalCSSFiles == section)
	{
		// TODO: Can only change "Active" for now.
		if (0 == op_strncmp(key, "Active ", 7))
		{
			unsigned long index = op_strtoul(key + 7, NULL, 10);
			if (index > 0 && index <= GetLocalCSSCount())
			{
				// API doc says we do a LEAVE if "value" does not contain
				// a proper integer.
				uni_char *endptr;
				if (value.IsEmpty())
				{
					LEAVE(OpStatus::ERR_OUT_OF_RANGE);
				}
				long intval = uni_strtol(value.CStr(), &endptr, 0);
				if (*endptr)
				{
					LEAVE(OpStatus::ERR_OUT_OF_RANGE);
				}
				return OpStatus::IsSuccess(WriteLocalCSSActiveL(index - 1, intval != 0));
			}
		}
	}
#  endif // LOCAL_CSS_FILES_SUPPORT

	return FALSE;
}

#  ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFiles::OverridePreferenceL(
	const uni_char *host, IniSection section, const char *key,
	const OpStringC &value, BOOL from_user)
{
	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; ++ i)
	{
		if (m_fileprefdefault[i].section == section && 0 == op_strcmp(m_fileprefdefault[i].key, key))
		{
#   ifdef PREFS_COVERAGE
			++ m_fileprefused[i];
#   endif
			OpStackAutoPtr<OpFile> new_file(OP_NEW_L(OpFile, ()));
			LEAVE_IF_ERROR(new_file->Construct(value.CStr()));
			return OpStatus::IsSuccess(OverridePrefL(host, filepref(i), new_file.get(), from_user));
		}
	}

	return FALSE;
}

BOOL PrefsCollectionFiles::RemoveOverrideL(
	const uni_char *host, IniSection section, const char *key, BOOL from_user)
{
	// Not implemented.

	return FALSE;
}
#  endif // PREFS_HOSTOVERRIDE
# endif // !PREFS_HAVE_STRING_API

#endif // PREFS_WRITE

#if defined PREFS_READ && defined PREFS_WRITE
void PrefsCollectionFiles::SetStorageFilenamesL(OpFileFolder folder)
{
	// Update all the settings for files in the specified folder, unless they
	// are explicitly configured in the INI file.

	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; i ++)
	{
		if (folder == m_fileprefdefault[i].folder)
		{
			// Read new file object and replace the old one
			OpFile *newfile = ReadFileL(filepref(i));
			OP_DELETE(m_files[i]);
			m_files[i] = newfile;
		}
	}
}
#endif // PREFS_READ && PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
// Override the implementation in OpPrefsCollectionWithHostOverride so that we
// can use our extended version which supports file properties
OpOverrideHost *PrefsCollectionFiles::CreateOverrideHostObjectL(
	const uni_char *host, BOOL from_user)
{
	OpStackAutoPtr<OverrideHostForPrefsCollectionFiles>
		newobj(OP_NEW_L(OverrideHostForPrefsCollectionFiles, ()));
	newobj->ConstructL(host, from_user);
# ifdef PREFS_WRITE
	if (g_prefsManager->IsInitialized())
	{
		g_prefsManager->OverrideHostAddedL(host);
	}
# endif
	return newobj.release();
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionFiles::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	OpString overridekey; ANCHOR(OpString, overridekey);
	OverrideHostForPrefsCollectionFiles *override = NULL;

	// The entries for a host are stored in a section which has the
	// same name as the host itself.
	const PrefsEntry *overrideentry = section ? section->Entries() : NULL;
	while (overrideentry)
	{
		OpStringC key(overrideentry->Key());

		// The override is stored as Section|Key=Value, so we need to split
		// it up to lookup in the default values. We assume that no section
		// or key can be longer than 64 characters.
#define MAXLEN 64
		int sectionlen = key.FindFirstOf('|');
		int keylen = key.Length() - sectionlen - 1;
		if (sectionlen != KNotFound && sectionlen <= MAXLEN - 1 && keylen <= MAXLEN - 1)
		{
			char realsection[MAXLEN], realkey[MAXLEN]; /* ARRAY OK 2007-01-26 peter */
			uni_cstrlcpy(realsection, key.CStr(), sectionlen + 1);
			uni_cstrlcpy(realkey, key.CStr() + sectionlen + 1, MAXLEN);

			if (m_reader->AllowedToChangeL(realsection, realkey))
			{
				OpPrefsCollection::IniSection section_id =
					OpPrefsCollection::SectionStringToNumber(realsection);

				// Locate the preference and insert its value if we were allowed
				// to change it and it is ours.
				for (int idx = 0; idx < PCFILES_NUMBEROFFILEPREFS; idx ++)
				{
					if (m_fileprefdefault[idx].section == section_id &&
						!OVERRIDE_STRCMP(m_fileprefdefault[idx].key, realkey))
					{
						if (!override)
						{
							override =
								static_cast<OverrideHostForPrefsCollectionFiles *>
								(FindOrCreateOverrideHostL(host, from_user));
						}

						// Create file override entry
						OpStackAutoPtr<OpFile> overridefile(OP_NEW_L(OpFile, ()));
						LEAVE_IF_ERROR(overridefile->Construct(
							overrideentry->Value(), OPFILE_SERIALIZED_FOLDER));
						override->SetFileEntry(idx, overridefile.release());

						break;
					}
				}
			}
		}

		overrideentry = overrideentry->Suc();
	}

	if (override)
	{
		override->SetActive(active);
	}
}
#endif // PREFS_HOSTOVERRIDE

void PrefsCollectionFiles::RegisterFilesListenerL(PrefsCollectionFilesListener *listener)
{
	// Mirrors the code in OpPrefsCollection; make into template?
	// Don't do anything if we already have it registered
	if (NULL != GetFilesContainerFor(listener)) return;

	// Encapsulate the OpPrefsListener in a ListenerContainer and put it into
	// our list.
	FilesListenerContainer *new_container =
		OP_NEW_L(FilesListenerContainer, (listener));
	new_container->IntoStart(&m_fileslisteners);
}

void PrefsCollectionFiles::UnregisterFilesListener(PrefsCollectionFilesListener *listener)
{
	// Mirrors the code in OpPrefsCollection; make into template?
	// Find the Listener object corresponding to this OpPrefsListener
	FilesListenerContainer *container = GetFilesContainerFor(listener);

	if (NULL == container)
	{
		// Don't do anything if we weren't registered (warn in debug builds)
		OP_ASSERT(!"Unregistering unregistered listener");
		return;
	}

	container->Out();
	OP_DELETE(container);
}

PrefsCollectionFiles::FilesListenerContainer *
	PrefsCollectionFiles::GetFilesContainerFor(const PrefsCollectionFilesListener *which)
{
	Link *p = m_fileslisteners.First();
	while (p && p->LinkId() != reinterpret_cast<const char *>(which))
	{
		p = p->Suc();
	}

	// Either the found object, or NULL
	return static_cast<FilesListenerContainer *>(p);
}

#ifdef PREFS_WRITE
void PrefsCollectionFiles::BroadcastChangeL(filepref pref)
{
	FilesListenerContainer *p =
		static_cast<FilesListenerContainer *>(m_fileslisteners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->FileChangedL(pref, m_files[pref]);

		p = static_cast<FilesListenerContainer *>(p->Suc());
	}
}
void PrefsCollectionFiles::BroadcastDirectoryChangeL(OpFileFolder pref, OpFile *directory)
{
	FilesListenerContainer *p =
		static_cast<FilesListenerContainer *>(m_fileslisteners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->DirectoryChangedL(pref, directory);

		p = static_cast<FilesListenerContainer *>(p->Suc());
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionFiles::BroadcastOverride(const uni_char *host)
{
	FilesListenerContainer *p =
		static_cast<FilesListenerContainer *>(m_fileslisteners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->FileHostOverrideChanged(host);

		p = static_cast<FilesListenerContainer *>(p->Suc());
	}
}
#endif

#ifdef LOCAL_CSS_FILES_SUPPORT
void PrefsCollectionFiles::BroadcastLocalCSSChanged()
{
	FilesListenerContainer *p = static_cast<FilesListenerContainer *>(m_fileslisteners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->LocalCSSChanged();

		p = static_cast<FilesListenerContainer *>(p->Suc());
	}
}
#endif

#if defined LOCAL_CSS_FILES_SUPPORT && defined PREFS_READ
void PrefsCollectionFiles::ReadLocalCSSFileInfoL()
{
# ifdef PREFS_USE_CSS_FOLDER_SCAN
	// Auto-discovery of local CSS files.
	OpStackAutoPtr<OpFile> target_directory(OP_NEW_L(OpFile, ()));

	// Test if the Styles folder exists under the user preferences.
	// If not we will copy everything from the Styles/user folder in the
	// package to that location. This will be enhanced later to probably
	// use version checking with the css file once all platforms use this
	// code.
	OP_STATUS rc;
	BOOL have_copied_files = FALSE;
	if (OpStatus::IsSuccess(rc = target_directory->Construct(UNI_L(""),
	                        OPFILE_USERPREFSSTYLE_FOLDER)))
	{
		BOOL exists;
		if (OpStatus::IsSuccess(rc = target_directory->Exists(exists)) && !exists)
		{
			// CreateDirectory(target_directory->GetFullPath(), NULL);

			OpStackAutoPtr<OpFolderLister>
				dir(OpFile::GetFolderLister(OPFILE_USERSTYLE_FOLDER,
				                            UNI_L("*.css")));

			while (dir.get() && dir->Next())
			{
				OpStackAutoPtr<OpFile>
					src_file(OP_NEW_L(OpFile, ())),
					dest_file(OP_NEW_L(OpFile, ()));
				if (OpStatus::IsSuccess(rc = src_file->Construct(dir->GetFullPath())) &&
					OpStatus::IsSuccess(rc = dest_file->Construct(dir->GetFileName(),
										OPFILE_USERPREFSSTYLE_FOLDER)))
				{
					OP_STATUS rc = dest_file->CopyContents(src_file.get(), FALSE);
					if (OpStatus::IsMemoryError(rc))
					{
						LEAVE(rc);
						have_copied_files = TRUE;
					}
				}
			}
		}
		else
		{
			// We already have copied files.
			have_copied_files = TRUE;
		}
	}
	if (OpStatus::IsMemoryError(rc))
	{
		LEAVE(rc);
	}

	// Scan through all the files in the user directory
	OpStackAutoPtr<OpFolderLister>
		dir(OpFile::GetFolderLister(have_copied_files ? OPFILE_USERPREFSSTYLE_FOLDER : OPFILE_USERSTYLE_FOLDER,
		                            UNI_L("*.css")));

	while (dir.get() && dir->Next())
	{
		// Read the name of the CSS file from the Name: tag at the top of
		// the file.
		OpString line; ANCHOR(OpString, line);

		OpStackAutoPtr<CSSFile> css_file(OP_NEW_L(CSSFile, ()));
		LEAVE_IF_ERROR(css_file->file.Construct(dir->GetFullPath()));
		if (OpStatus::IsSuccess(css_file->file.Open(OPFILE_READ | OPFILE_SHAREDENYWRITE)))
		{
			for (int i = 0; i < 6; ++ i)
			{
				if (OpStatus::IsError(css_file->file.ReadUTF8Line(line)))
				{
					break;
				}

				int index = line.Find("Name:");
				if (index != KNotFound)
				{
					// Strip final whitespace and comment delimeters
					uni_char *p = line.CStr() + index + 5;
					uni_char *strend = line.CStr() + line.Length() - 1;
					while (uni_isspace(*strend) || '/' == *strend)
					{
						while (uni_isspace(*strend))
						{
							*(strend --) = 0;
						}
						if (strend > p && '/' == *strend && '*' == *(strend - 1))
						{
							*(strend - 1) = 0;
							strend -= 2;
						}
					}

					// If the name is numerical, use it as a string id.
					// Otherwise use the name string verbatim.
					int nameid = 0;
					if (uni_sscanf(p, UNI_L("%d%*c"), &nameid) != 1 ||
					    nameid == 0)
					{
						css_file->name.SetL(p);
						css_file->name.Strip(TRUE, TRUE);
					}
					else if (nameid != 0)
					{
						css_file->nameid = Str::LocaleString(nameid);
					}
					break;
				}
			}
			css_file->file.Close();

			// If no name or number was found, set default the name to the
			// filename itself.
			if (css_file->name.IsEmpty() &&
				Str::LocaleString(css_file->nameid) == Str::NOT_A_STRING)
			{
				css_file->name.SetL(dir->GetFileName());
			}

			// Use the file name as a key for the active flag
			css_file->active =
				m_reader->ReadBoolL(UNI_L("Local CSS Files"),
									dir->GetFileName());

			LEAVE_IF_ERROR(m_localCSSFileList.Add(css_file.get()));
			css_file.release(); // Now owned by m_localCSSFileList
		}
	}
# else
#  ifdef PREFS_USE_USERSTYLE_INI
	// Unix desktop releases read and write from a userstyle.ini in the
	// home folder.
	OpStackAutoPtr<OpFile> user_style_file(OP_NEW_L(OpFile, ()));
	GetFileL(UserStyleIniFile, *user_style_file.get());

	BOOL exists;
	LEAVE_IF_ERROR(!user_style_file->Exists(exists));
	if (!exists)
	{
		// Try to copy from shared (install) directory to personal directory
		// (assumes that user_style_file points to the personal directory)
		OpFile file; ANCHOR(OpFile, file);
		if (OpStatus::IsSuccess(file.Construct(m_fileprefdefault[UserStyleIniFile].filename,
		                                       OPFILE_USERSTYLE_FOLDER)))
		{
			OP_STATUS rc = user_style_file->CopyContents(&file, FALSE);
			if (OpStatus::IsMemoryError(rc))
			{
				LEAVE(rc);
			}
		}
	}

	OpStackAutoPtr<PrefsFile> userstyle_ini(OP_NEW_L(PrefsFile, (PREFS_STD)));
	userstyle_ini->ConstructL();
	userstyle_ini->SetFileL(user_style_file.get());

	userstyle_ini->LoadAllL();
	PrefsFile *reader = userstyle_ini.get();
#  else // PREFS_USE_USERSTYLE_INI
	// Everyone else uses the standard reader
	PrefsFile *reader = m_reader;
#  endif

	const char *section = m_sections[SLocalCSSFiles];

	// Iterate over the local CSS file section until we run out of data
	BOOL found;
	unsigned int index = 1;
	char filekey[32], namekey[32], tnamekey[32], activekey[32]; /* ARRAY OK 2007-01-26 peter */
	do
	{
		op_snprintf(filekey, ARRAY_SIZE(filekey), "File %u", index);
		found = reader->IsKey(section, filekey);
		if (found)
		{
			// Read file
			OpStackAutoPtr<CSSFile> css_file(OP_NEW_L(CSSFile, ()));
			OpString name; ANCHOR(OpString, name);
			name.ReserveL(_MAX_PATH);
			reader->ReadStringL(section, filekey, name);
			LEAVE_IF_ERROR(css_file->file.Construct(name.CStr(), OPFILE_SERIALIZED_FOLDER));
			BOOL exists;
			LEAVE_IF_ERROR(css_file->file.Exists(exists));

#  ifdef PREFS_USE_USERSTYLE_INI
			if (!exists)
			{
				// Installed file contains filenames without paths.
				OpFile shared_file; ANCHOR(OpFile, shared_file);
				LEAVE_IF_ERROR(shared_file.Construct(name, OPFILE_USERSTYLE_FOLDER));
				LEAVE_IF_ERROR(shared_file.Exists(exists));
				if (exists)
				{
					// Write full path to local ini file.
					userstyle_ini->WriteStringL(section, filekey, shared_file.GetSerializedName());
				}
			}
#  endif

			if (exists)
			{
				// Only add the file if it actually exists.

				op_snprintf(tnamekey, ARRAY_SIZE(tnamekey), "Translated name %u", index);
				op_snprintf(namekey, ARRAY_SIZE(namekey), "Name %u", index);
				op_snprintf(activekey, ARRAY_SIZE(activekey), "Active %u", index);

				// Read name to display in UI, prefer the string name, if
				// defined.
				reader->ReadStringL(section, namekey, css_file->name, NULL);
				if (css_file->name.IsEmpty())
				{
					css_file->nameid = reader->ReadIntL(section, tnamekey, Str::NOT_A_STRING);
					if (Str::LocaleString(css_file->nameid) == Str::NOT_A_STRING)
					{
						// Fall back to the file name.
						css_file->name.SetL(css_file->file.GetName());
					}
				}
				else
				{
					css_file->nameid = Str::NOT_A_STRING;
				}

				css_file->active = reader->ReadBoolL(section, activekey);
				LEAVE_IF_ERROR(m_localCSSFileList.Add(css_file.get()));
				css_file.release(); // Now owned by m_localCSSFileList
			}
		}
		++ index;
	} while (found);

#  ifdef PREFS_USE_USERSTYLE_INI
	userstyle_ini->CommitL();
#  endif
# endif // PREFS_USE_CSS_FOLDER_SCAN
}
#endif // LOCAL_CSS_FILES_SUPPORT && PREFS_READ

#ifdef LOCAL_CSS_FILES_SUPPORT
const uni_char *PrefsCollectionFiles::GetLocalCSSName(unsigned int index)
{
	CSSFile *css = m_localCSSFileList.Get(index);
	if (css)
	{
		if (css->name.IsEmpty())
		{
			// Look up language string
			if (OpStatus::IsError(g_languageManager->GetString(Str::LocaleString(css->nameid), css->name)))
			{
				// Failed string lookup, use the file name.
				return css->file.GetName();
			}
		}
		return css->name.CStr();
	}

	return NULL;
}

BOOL PrefsCollectionFiles::IsLocalCSSActive(unsigned int index)
{
	const CSSFile *css = m_localCSSFileList.Get(index);
	if (css)
	{
		return css->active;
	}

	return FALSE;
}

OpFile *PrefsCollectionFiles::GetLocalCSSFile(unsigned int index)
{
	CSSFile *css = m_localCSSFileList.Get(index);
	if (css)
	{
		return &css->file;
	}

	return NULL;
}

OP_STATUS PrefsCollectionFiles::WriteLocalCSSActiveL(unsigned int index, BOOL active)
{
#ifdef PREFS_READ
	CSSFile *css = m_localCSSFileList.Get(index);
	if (!css)
	{
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}
	css->active = active;

	OP_STATUS rc = OpStatus::OK;

# ifdef PREFS_WRITE
#  ifdef PREFS_USE_CSS_FOLDER_SCAN
	m_reader->WriteIntL(UNI_L("Local CSS Files"), css->file.GetName(), active);
#  else
	char key[32]; /* ARRAY OK 2009-02-26 adame */
	op_snprintf(key, ARRAY_SIZE(key), "Active %u", index + 1);

#   ifdef PREFS_USE_USERSTYLE_INI
	// Unix desktop releases read and write from a userstyle.ini in the
	// home folder.
	OpStackAutoPtr<OpFile> user_style_file(OP_NEW_L(OpFile, ()));
	GetFileL(UserStyleIniFile, *user_style_file.get());

	OpStackAutoPtr<PrefsFile> reader(OP_NEW_L(PrefsFile, (PREFS_STD)));
	reader->ConstructL();
	reader->SetFileL(user_style_file.get());
	reader->LoadAllL();
#   else // PREFS_USE_USERSTYLE_INI
	// Everyone else uses the standard reader
	PrefsFile *reader = m_reader;
#   endif

	rc = reader->WriteIntL(m_sections[SLocalCSSFiles], key, active);

#   ifdef PREFS_USE_USERSTYLE_INI
	reader->CommitL();
#   endif
#  endif // !PREFS_USE_CSS_FOLDER_SCAN
# endif // PREFS_WRITE

	// Broadcast the change
	BroadcastLocalCSSChanged();
	return rc;
#else
	// FIXME: Update the internal list here
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}
#endif // LOCAL_CSS_FILES_SUPPORT

#ifdef PREFS_COVERAGE
void PrefsCollectionFiles::CoverageReport(const struct stringprefdefault *, int,
                                          const struct integerprefdefault *, int)
{
	OpPrefsCollection::CoverageReport(NULL, 0, NULL, 0);

	BOOL all = TRUE;
	CoverageReportPrint("* File preferences:\n");
	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; ++ i)
	{
		if (!m_fileprefused[i])
		{
			CoverageReportPrint("  \"");
			CoverageReportPrint(m_sections[m_fileprefdefault[i].section]);
			CoverageReportPrint("\".\"");
			CoverageReportPrint(m_fileprefdefault[i].key);
			CoverageReportPrint("\" is unreferenced\n");
			all = FALSE;
		}
	}
	if (all)
	{
		CoverageReportPrint("  All file preferences referenced\n");
	}
}
#endif

#ifdef PREFS_ENUMERATE
unsigned int PrefsCollectionFiles::GetNumberOfPreferences() const
{
	return static_cast<unsigned int>(PCFILES_NUMBEROFFILEPREFS
	       + PCFILES_NUMBEROFFOLDERPREFS);
}

unsigned int PrefsCollectionFiles::GetPreferencesL(struct prefssetting *settings) const
{
	// Copy file values
	const fileprefdefault *files = m_fileprefdefault;

	for (int i = 0; i < PCFILES_NUMBEROFFILEPREFS; ++ i)
	{
		settings->section = m_sections[files->section];
		settings->key     = files->key;
		settings->value.SetL(m_files[i]->GetFullPath());
		settings->type    = files->default_if_not_found ? prefssetting::requiredfile : prefssetting::file;

		++ settings;
		++ files;
	}

	// Copy directory values
	const directorykey *directories = m_directorykeys;

	OpFolderManager *fldr = g_folder_manager;
	for (size_t j = 0; j < PCFILES_NUMBEROFFOLDERPREFS; ++ j)
	{
		settings->section = m_sections[directories->section];
		settings->key     = directories->key;
		LEAVE_IF_ERROR(fldr->GetFolderPath(directories->folder, settings->value));
		settings->type    = prefssetting::directory;

		++ settings;
		++ directories;
	}

	return static_cast<unsigned int>(PCFILES_NUMBEROFFILEPREFS
	       + PCFILES_NUMBEROFFOLDERPREFS);
}
#endif

#ifdef PREFS_VALIDATE

#ifdef PREFS_CHECK_TEMPDOWNLOAD_FOLDER
static BOOL StringEndsWith(const uni_char *text, const uni_char *postfix)
{
	if ((text == NULL) || (postfix == NULL))
		return FALSE;
	size_t postfix_len = uni_strlen(postfix);
	// everything ends with an empty string
	if (postfix_len == 0)
		return TRUE;
	size_t text_len = uni_strlen(text);
	if (postfix_len > text_len)
		return FALSE;
	return uni_strncmp(postfix, text + text_len - postfix_len, postfix_len) == 0;
}

BOOL PrefsCollectionFiles::CheckTempDownloadDirectoryConditionsL(const OpStringC &invalue, OpString **outvalue)
{
	// sometimes (in installer on linux and mac) the setting is empty initially
	// in this case we should not try to touch and change it
	if (invalue.IsEmpty())
		return FALSE;

	BOOL is_changed = FALSE;
	OpStackAutoPtr<OpString> tempdownload_folder(OP_NEW_L(OpString, ()));
	tempdownload_folder->SetL(invalue);

	// remove trailing slashes
	while (StringEndsWith(tempdownload_folder->CStr(), UNI_L(PATHSEP)))
	{
		tempdownload_folder->Delete(tempdownload_folder->Length() - uni_strlen(UNI_L(PATHSEP)));
		is_changed = TRUE;
	}

	// verify that this folder path must contain a "temporary_downloads" subdirectory in the end
	const uni_char *sub_folder_name = UNI_L("temporary_downloads");
	if (!StringEndsWith(tempdownload_folder->CStr(), sub_folder_name))
	{
		tempdownload_folder->AppendL(PATHSEP);
		tempdownload_folder->AppendL(sub_folder_name);
		is_changed = TRUE;
	}

	*outvalue = tempdownload_folder.release();
	return is_changed;
}
#endif // PREFS_CHECK_TEMPDOWNLOAD_FOLDER

BOOL PrefsCollectionFiles::CheckDirectoryConditionsL(OpFileFolder which, const OpStringC &invalue, OpString **outvalue)
{
	switch (which)
	{
#ifdef PREFS_CHECK_TEMPDOWNLOAD_FOLDER
	case OPFILE_TEMPDOWNLOAD_FOLDER:
		return CheckTempDownloadDirectoryConditionsL(invalue, outvalue);
#endif // PREFS_CHECK_TEMPDOWNLOAD_FOLDER
	default:
		return FALSE;
	}
}

#endif // PREFS_VALIDATE
