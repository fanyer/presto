/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef LOC_LNGFILELANGMAN
# include "modules/locale/src/opprefsfilelanguagemanager.h"
# include "modules/locale/locale-enum.h"
# include "modules/prefsfile/prefsfile.h"
# include "modules/prefsfile/prefssection.h"
# include "modules/prefsfile/prefsentry.h"
# include "modules/util/opfile/opfile.h"

OpPrefsFileLanguageManager::OpPrefsFileLanguageManager()
	: m_stringdb(NULL)
	, m_number_of_strings(0)
{
}

OpPrefsFileLanguageManager::~OpPrefsFileLanguageManager()
{
	delete[] m_stringdb;
}

int OpPrefsFileLanguageManager::GetString(Str::LocaleString num, UniString &s)
{
	// Find the proper entry
	stringdb_s target;
	if (0 != (target.id = static_cast<int>(num)))
	{
		stringdb_s *entry =
			reinterpret_cast<stringdb_s *>
			(op_bsearch(&target, m_stringdb, m_number_of_strings,
			            sizeof (stringdb_s), entrycmp));

		if (entry)
		{
			s = UniString(m_strings, entry->s.ofs, (entry + 1)->s.ofs - entry->s.ofs);
			return OpStatus::OK;
		}
	}

	s.Clear();

#ifdef LOCALE_STRING_NOT_FOUND
	if (0 != target.id && Str::S_STRING_NOT_FOUND != target.id)
	{
		UniString placeholder;
		const uni_char *placeholder_str = NULL;
		if (OpStatus::IsSuccess(GetString(Str::S_STRING_NOT_FOUND, placeholder)))
			placeholder_str = placeholder.Data(true);
		if (!placeholder_str || !*placeholder_str)
			placeholder_str = UNI_L("String not found (%d)");

		if (OpStatus::IsSuccess(s.AppendFormat(placeholder_str, target.id)))
			return s.Length();
	}
#endif

	return OpStatus::OK;
}

#ifdef LANGUAGE_FILE_SUPPORT
void OpPrefsFileLanguageManager::LoadL()
{
	// Must be done after PrefsModule::InitL().
	OP_ASSERT(g_pcfiles);

# ifdef PREFSFILE_CASCADE
	// Set up the fallback language, we read this as the global file to handle
	// partially translated language files.
	OpStackAutoPtr<OpFile> fallbacklanguage(OP_NEW_L(OpFile, ()));
	OpFileFolder def_lang_file_folder;
	const uni_char *def_lang_file_name;
	g_pcfiles->GetDefaultFilePref(PrefsCollectionFiles::LanguageFile, &def_lang_file_folder, &def_lang_file_name);

	LEAVE_IF_ERROR(fallbacklanguage->Construct(def_lang_file_name,
	               def_lang_file_folder));

	BOOL fallback_language_file_exists;
	LEAVE_IF_ERROR(fallbacklanguage->Exists(fallback_language_file_exists));
# endif

	// The standard language is read from the preferences as usual.
	OpStackAutoPtr<OpFile> language(OP_NEW_L(OpFile, ()));
	g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, *language.get());

	BOOL language_file_exists;
	LEAVE_IF_ERROR(language->Exists(language_file_exists));

	// Set up the PrefsFile object with both files, unless they point to
	// the same file.
	OpStackAutoPtr<PrefsFile> languageFile(OP_NEW_L(PrefsFile, (PREFS_LNG)));
	languageFile->ConstructL();
	if (language_file_exists)
	{
		languageFile->SetFileL(language.get());
	}

# ifdef PREFSFILE_CASCADE
	if (fallback_language_file_exists)
	{
		if (!language_file_exists)
		{
			languageFile->SetFileL(fallbacklanguage.get());
		}
		else if (uni_strcmp(fallbacklanguage->GetSerializedName(), language->GetSerializedName()) != 0)
		{
			languageFile->SetGlobalFileL(fallbacklanguage.get());
		}
	}
# endif

	// Now load the translation; LoadTranslationL() assumes ownership
	LoadTranslationL(languageFile.release());
}
#endif // LANGUAGE_FILE_SUPPORT

void OpPrefsFileLanguageManager::LoadTranslationL(PrefsFile *file)
{
	// file cannot be NULL
	OP_ASSERT(file);

#ifndef LANGUAGEMANAGER_CAN_RELOAD
	// Cannot be called twice
	OP_ASSERT(!m_stringdb);
#endif

	// All errors are fatal here
	OpStackAutoPtr<PrefsFile> lngfile(file);
	LEAVE_IF_ERROR(lngfile->LoadAllL());

	// Get static data
	OpString new_language; ANCHOR(OpString, new_language);
	/* Default to "und" if we fail to get language (cf. RFC 5646) */
	lngfile->ReadStringL("Info", "Language", new_language, UNI_L("und"));
	unsigned int new_db = lngfile->ReadIntL("Info", "DB.version");
	switch (lngfile->ReadIntL("Info", "Direction"))
	{
	case 0:
	default:
		m_direction = LTR;
		break;

	case 1:
		m_direction = RTL;
	}

	// Move everything over to a more compact internal storage
	OpStackAutoPtr<PrefsSection> translation(lngfile->ReadSectionL(UNI_L("Translation")));

#ifdef LOCALE_SUPPORT_EXTRASECTION
	// Merge the platform overrides
	OpStackAutoPtr<PrefsSection> extrastrings(lngfile->ReadSectionL(LOCALE_EXTRASECTION));
	if (extrastrings.get())
	{
		translation->CopyKeysL(extrastrings.get());
	}
#endif

	// We don't need the language file object any more
	delete lngfile.release();

	int new_number_of_strings = translation->Number();
	OpStackAutoPtr<stringdb_s> new_stringdb(OP_NEWA_L(stringdb_s, (new_number_of_strings + 1)));

	size_t stringbufsize = 0;

	// Calculate the total string length; we need to loop through first
	// anyway to get all the id numbers so that we can sort the list
	const PrefsEntry *entry_p = translation->Entries();
	stringdb_s *current = new_stringdb.get();
	while (entry_p)
	{
		// Read the id number
		current->id = uni_strtoul(entry_p->Key(), NULL, 10);

		// Read the string and remember for the next iteration
		current->s.string = entry_p->Value();
		if (current->id && current->s.string)
		{
			// Valid entry
			size_t len = uni_strlen(current->s.string);
			stringbufsize += len;
			++ current;
		}
		else
		{
			// Malformed entry, skip it 
			-- new_number_of_strings;
		}

		// Step to next entry
		entry_p = entry_p->Suc();
	}
	stringdb_s *sentinel = current;

	// Sort the ids
	op_qsort(new_stringdb.get(), new_number_of_strings, sizeof (stringdb_s), entrycmp);

	// Run through the strings again, this time copying and replacing the
	// string pointers with offsets into the UniString.
	UniString new_strings;
	uni_char *current_pos = new_strings.GetAppendPtr(stringbufsize);
	if (!current_pos)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	current = new_stringdb.get();
	size_t current_ofs = 0;
	while (current < sentinel)
	{
		// Get the string
		size_t len = uni_strlen(current->s.string);
		op_memcpy(current_pos, current->s.string, len * sizeof (uni_char));
		current->s.ofs = current_ofs;
		current_ofs += len;
		current_pos += len;

		// Step to next entry
		++ current;
	}

	// Sentinel
	sentinel->s.ofs = current_ofs;

#ifdef LANGUAGEMANAGER_CAN_RELOAD
	// Clean-up if we're called twice
	delete[] m_stringdb;
#endif

	m_language.TakeOver(new_language);
	m_number_of_strings = new_number_of_strings;
	m_stringdb = new_stringdb.release();
	m_strings = new_strings;
	m_db = new_db;
}

#ifdef LANGUAGEMANAGER_CAN_RELOAD
void OpPrefsFileLanguageManager::FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile* /*newvalue*/)
{
	if (PrefsCollectionFiles::LanguageFile == pref)
	{
		LoadL();
	}
}
#endif

int OpPrefsFileLanguageManager::entrycmp(const void *p1, const void *p2)
{
	int id1 = reinterpret_cast<const stringdb_s *>(p1)->id;
	int id2 = reinterpret_cast<const stringdb_s *>(p2)->id;
	if (id1 < id2)
	{
		return -1;
	}
	else if (id1 > id2)
	{
		return 1;
	}

	return 0;
}

#endif // LOC_LNGFILELANGMAN
