/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef PREFS_HAS_PREFSFILE

#include "modules/util/opfile/opfile.h"
#include "modules/prefsfile/accessors/prefsaccessor.h"
#include "modules/prefsfile/accessors/ini.h"
#include "modules/prefsfile/accessors/xmlaccessor.h"
#include "modules/prefsfile/accessors/lng.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/impl/backend/prefsmap.h"

#include "modules/debug/debug.h"

#ifdef PREFS_READ_ALL_SECTIONS
# include "modules/util/OpHashTable.h"
# include "modules/util/opstrlst.h"
#endif

#undef RETURN_OPSTRINGC
#ifdef CAN_RETURN_IMPLICIT_OBJECTS
# define RETURN_OPSTRINGC(s) { return s; }
#else
# define RETURN_OPSTRINGC(s) { OpStringC tmp(s); return tmp; }
#endif

/*
 * PrefsFile
 *
 */

PrefsFile::PrefsFile(PrefsType type
#ifdef PREFSFILE_CASCADE
					 , int numglobal, int numfixed
#endif
					 )
	: m_prefsAccessor(NULL)
	, m_cacheMap(NULL)
	, m_localMap(NULL)
#ifdef PREFSFILE_CASCADE
	, m_globalMaps(NULL)
# ifdef PREFSFILE_WRITE_GLOBAL
	, m_globalCacheMaps(NULL)
# endif
	, m_fixedMaps(NULL)
#endif // PREFSFILE_CASCADE
	, m_file(NULL)
#ifdef PREFSFILE_CASCADE
	, m_numglobalfiles(numglobal)
	, m_globalfiles(NULL)
	, m_numfixedfiles(numfixed)
	, m_fixedfiles(NULL)
#endif // PREFSFILE_CASCADE
	, m_hasdeletedkeys(FALSE)
#if defined PREFSFILE_CASCADE && defined PREFSFILE_WRITE_GLOBAL
	, m_hasdeletedglobalkeys(NULL)
#endif
	, m_writable(FALSE)
	, m_localloaded(FALSE)
#ifdef PREFSFILE_CASCADE
	, m_globalsloaded(NULL)
	, m_fixedsloaded(NULL)
#endif // PREFSFILE_CASCADE
	, m_type(type)
{
}

#ifdef PREFSFILE_EXT_ACCESSOR
PrefsFile::PrefsFile(PrefsAccessor *accessor
#ifdef PREFSFILE_CASCADE
					 , int numglobal, int numfixed
#endif
					 )
	: m_prefsAccessor(accessor)
	, m_cacheMap(NULL)
	, m_localMap(NULL)
#ifdef PREFSFILE_CASCADE
	, m_globalMaps(NULL)
# ifdef PREFSFILE_WRITE_GLOBAL
	, m_globalCacheMaps(NULL)
# endif
	, m_fixedMaps(NULL)
#endif // PREFSFILE_CASCADE
	, m_file(NULL)
#ifdef PREFSFILE_CASCADE
	, m_numglobalfiles(numglobal)
	, m_globalfiles(NULL)
	, m_numfixedfiles(numfixed)
	, m_fixedfiles(NULL)
#endif // PREFSFILE_CASCADE
	, m_hasdeletedkeys(FALSE)
#if defined PREFSFILE_CASCADE && defined PREFSFILE_WRITE_GLOBAL
	, m_hasdeletedglobalkeys(NULL)
#endif
	, m_writable(FALSE)
	, m_localloaded(FALSE)
#ifdef PREFSFILE_CASCADE
	, m_globalsloaded(NULL)
	, m_fixedsloaded(NULL)
#endif // PREFSFILE_CASCADE
	, m_type(PREFS_STD)
{
}
#endif

void PrefsFile::ConstructL()
{
#ifdef PREFSFILE_EXT_ACCESSOR
	if (!m_prefsAccessor)
#endif
	{
		// select your preferred file type (used for both local and global files)
		switch (m_type)
		{
#ifdef PREFS_HAS_INI
			case PREFS_INI:
				m_prefsAccessor  = OP_NEW_L(IniAccessor, (FALSE));
				break;

# ifdef PREFS_HAS_INI_ESCAPED
			case PREFS_INI_ESCAPED:
				m_prefsAccessor  = OP_NEW_L(IniAccessor, (TRUE));
				break;
# endif

# ifdef PREFS_HAS_LNG
			case PREFS_LNG:
				m_prefsAccessor  = OP_NEW_L(LangAccessor, ());
				break;
# endif
#endif // PREFS_HAS_INI

#ifdef PREFS_HAS_XML
			case PREFS_XML:
				m_prefsAccessor  = OP_NEW_L(XmlAccessor, ());
				break;
#endif

#ifdef PREFS_HAS_NULL
			case PREFS_NULL:
				break;
#endif // PREFS_HAS_NULL

			default:
				OP_ASSERT(!"Unknown preference file format requested");
				LEAVE(OpStatus::ERR_OUT_OF_RANGE);
				break;
		}
	}

	m_cacheMap		= OP_NEW_L(PrefsMap, ());
	m_localMap		= OP_NEW_L(PrefsMap, ());
#ifdef PREFSFILE_CASCADE
	OP_ASSERT(m_numglobalfiles >= 0 || !"Bogus number of files in constructor");
	if (m_numglobalfiles >= 1)
	{
		m_globalMaps = OP_NEWA_L(PrefsMap, m_numglobalfiles);
# ifdef PREFSFILE_WRITE_GLOBAL
		m_globalCacheMaps = OP_NEWA_L(PrefsMap, m_numglobalfiles);
		m_hasdeletedglobalkeys = OP_NEWA_L(BOOL, m_numglobalfiles);
# endif
		m_globalsloaded = OP_NEWA_L(BOOL, m_numglobalfiles);
		m_globalfiles = OP_NEWA_L(OpFileDescriptor *, m_numglobalfiles);

		for (int i = 0; i < m_numglobalfiles; ++ i)
		{
			m_globalfiles[i] = NULL;
			m_globalsloaded[i] = FALSE;
# ifdef PREFSFILE_WRITE_GLOBAL
			m_hasdeletedglobalkeys[i] = FALSE;
# endif
		}
	}
	OP_ASSERT(m_numfixedfiles >= 0 || !"Bogus number of files in constructor");
	if (m_numfixedfiles >= 1)
	{
		m_fixedMaps = OP_NEWA_L(PrefsMap, m_numfixedfiles);
		m_fixedsloaded = OP_NEWA_L(BOOL, m_numfixedfiles);
		m_fixedfiles = OP_NEWA_L(OpFileDescriptor *, m_numfixedfiles);

		for (int i = 0; i < m_numfixedfiles; ++ i)
		{
			m_fixedfiles[i] = NULL;
			m_fixedsloaded[i] = FALSE;
		}
	}
#endif // PREFSFILE_CASCADE
}

PrefsFile::~PrefsFile()
{
	// If these asserts hit you, you forgot to call CommitL(). In some
	// cases, you might need to delete the PrefsFile object prematurely
	// due to OOM or OOD conditions. Please note that if this assert
	// hits you, you HAVE lost all your preference changes.
	OP_ASSERT(!m_cacheMap || m_cacheMap->IsEmpty() || !"CommitL() not called or call failed");
	OP_ASSERT(!m_hasdeletedkeys || !"CommitL() not called or call failed");
	// ---

	// Now clean up. Any changes not committed are lost.
	OP_DELETE(m_prefsAccessor);
	OP_DELETE(m_cacheMap);
	OP_DELETE(m_localMap);
#ifdef PREFSFILE_CASCADE
	OP_DELETEA(m_globalMaps);
# ifdef PREFSFILE_WRITE_GLOBAL
	OP_DELETEA(m_globalCacheMaps);
	OP_DELETEA(m_hasdeletedglobalkeys);
# endif
	OP_DELETEA(m_globalsloaded);
	OP_DELETEA(m_fixedMaps);
	OP_DELETEA(m_fixedsloaded);
#endif // PREFSFILE_CASCADE

	OP_DELETE(m_file);
#ifdef PREFSFILE_CASCADE
	if (m_globalfiles)
	{
		for (int i = 0; i < m_numglobalfiles; ++ i)
		{
			OP_DELETE(m_globalfiles[i]);
		}
		OP_DELETEA(m_globalfiles);
	}
	if (m_fixedfiles)
	{
		for (int j = 0; j < m_numfixedfiles; ++ j)
		{
			OP_DELETE(m_fixedfiles[j]);
		}
		OP_DELETEA(m_fixedfiles);
	}
#endif // PREFSFILE_CASCADE
}

#ifdef PREFSFILE_RESET
void PrefsFile::Reset()
{
# if defined PREFSFILE_CASCADE && defined PREFSFILE_WRITE_GLOBAL
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		m_hasdeletedglobalkeys[i] = FALSE;
	};
# endif
	m_hasdeletedkeys = FALSE;
	Flush();
	m_cacheMap->FreeAll();
# ifdef PREFSFILE_WRITE_GLOBAL
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		m_globalCacheMaps[i].FreeAll();
	}
# endif
}
#endif // PREFSFILE_RESET

OP_STATUS PrefsFile::LoadFileL(OpFileDescriptor *file, PrefsMap *map)
{
#if defined(PREFS_HAS_NULL)
	if (!m_prefsAccessor) return OpStatus::OK;
#endif

	// only load if the corresponding map
	// in memory is empty, else assume that
	// we are up to date

	// If you crash here with this==NULL, you forgot to call
	// PrefsFile::ConstructL().
	if (!map->IsEmpty())
	{
		return OpStatus::OK;
	}

	return m_prefsAccessor->LoadL(file, map);
}

OP_STATUS PrefsFile::LoadLocalL()
{
	OP_STATUS rc = OpStatus::OK;
	if (!m_localloaded)
	{
		if (m_file)
		{
			OpStatus::Ignore(rc);
			rc = LoadFileL(m_file, m_localMap);
		}
		m_localloaded = TRUE;
	}
	return rc;
}

#ifdef PREFSFILE_CASCADE
void PrefsFile::LoadGlobalL(int num)
{
	OP_ASSERT(num >= 0 && num < m_numglobalfiles && "num is out of range");
	if (num >= 0 && num < m_numglobalfiles && !m_globalsloaded[num])
	{
		// It doesn't matter if the file is found or not.
		// The out-of-memory error is handled via leave.
		if (m_globalfiles[num])
			OpStatus::Ignore(LoadFileL(m_globalfiles[num], &m_globalMaps[num]));
		m_globalsloaded[num] = TRUE;
	}
}

void PrefsFile::LoadFixedL(int num)
{
	OP_ASSERT(num >= 0 && num < m_numfixedfiles && "num is out of range");
	if (num >= 0 && num < m_numfixedfiles && !m_fixedsloaded[num])
	{
		// It doesn't matter if the file is found or not.
		// The out-of-memory error is handled via leave.
		if (m_fixedfiles[num])
			OpStatus::Ignore(LoadFileL(m_fixedfiles[num], &m_fixedMaps[num]));
		m_fixedsloaded[num] = TRUE;
	}
}
#endif // PREFSFILE_CASCADE

OP_STATUS PrefsFile::LoadAllL()
{

	/*

	  1) read and parse global settings file to global map
	  2) read and parse local settings file to local map
	  3) read and parse fixed settings file to fixed map

	*/

	OP_STATUS rc = OpStatus::OK;
#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numfixedfiles; ++ i)
	{
		if (!m_fixedsloaded[i])
		{
			LoadFixedL(i);
		}
	}

	for (int j = 0; j < m_numglobalfiles; ++ j)
	{
		if (!m_globalsloaded[j])
		{
			LoadGlobalL(j);
		}
	}
#endif // PREFSFILE_CASCADE

	if (!m_localloaded)
	{
		OpStatus::Ignore(rc);
		rc = LoadLocalL();
		if (OpStatus::ERR_NO_ACCESS == rc)
		{
			/* The file is not readable by us, or is not a file at all.
			   We can survive that, we just cannot write to the file. */
			m_writable = FALSE;
			rc = OpStatus::OK;
		}
	}

	return rc;
}

void PrefsFile::Flush()
{
	/*

	  1) free global map
	  2) free local map

	*/

	// we don't free the fixed map (or should we?)

#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
#ifdef PREFSFILE_WRITE_GLOBAL
		// cannot unload global map if we have deleted keys
		if (!m_hasdeletedglobalkeys[i])
#endif
		{
			m_globalMaps[i].FreeAll();
			m_globalsloaded[i] = FALSE;
		}
	}
#endif // PREFSFILE_CASCADE

	// cannot unload local map if we have deleted keys
	if (!m_hasdeletedkeys)
	{
		m_localMap->FreeAll();
		m_localloaded = FALSE;
	}
}

void PrefsFile::CommitL(BOOL force, BOOL flush)
{
#ifdef PREFSFILE_WRITE

#if defined(PREFS_HAS_NULL)
	if (!m_prefsAccessor) return;
#endif

#ifdef PREFSFILE_WRITE_GLOBAL
	/*

	  01) if global cache map is empty, do not do anything

	  02) load and parse global settings file to global map

	  03) for each key in global cache map:
	      if key in global cache map = key in global map
		     -> remove from global cache map

	  04) if global cache map is empty, do not do anything

	  05) merge global cache map to global map

	  06) write global map to global settings file

	  07) free global cache map
	*/

	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		if (m_globalfiles[i] && (!m_globalCacheMaps[i].IsEmpty() || m_hasdeletedglobalkeys[i]))
		{
			// Load the global file
			if (!m_globalsloaded[i])
			{
				OP_ASSERT(!m_hasdeletedglobalkeys[i]);
				LoadGlobalL(i);
			}

			// Remove everything that's already in the file
			m_globalCacheMaps[i].DeleteDuplicates(&m_globalMaps[i], TRUE);

			// something else to store?
			if (!m_globalCacheMaps[i].IsEmpty() || m_hasdeletedglobalkeys[i])
			{
				m_globalMaps[i].IncorporateL(&m_globalCacheMaps[i]);
				OpStatus::Ignore(m_prefsAccessor->StoreL(m_globalfiles[i], &m_globalMaps[i]));

				m_globalCacheMaps[i].FreeAll();
			}

			m_hasdeletedglobalkeys[i] = FALSE;
		}
	}
#endif

	if (!m_writable) return;

	/*

	  01) if cache map is empty flush and return without error

	  02) load and parse fixed settings file to fixed map

	  03) for each key in cache map
	      if key in cache map also in fixed map -> remove from cache map

	  04) load and parse local settings file to local map

	  05) for each key in cache map:
	      if key in cache map = key in local map  -> remove from cache map

	  06) if cache map is empty - return without error

	  07) load and parse global settings file to global map

	  08) for each key in cache map:
	      if key in cache map = key in global map -> remove from local map and cache map

	  09) if cache map is empty - flush and return without error

	  10) merge cache map to local map

	  11) write local map to local settings file

	  12) call flush

	  13) free cache map

	*/

	// nothing to store?
	if (m_cacheMap->IsEmpty() && !m_hasdeletedkeys)
	{
		if (flush) Flush();
		return;
	}

#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numfixedfiles; ++ i)
	{
		// load the file with the fixed settings
		if (!m_fixedsloaded[i])
		{
			LoadFixedL(i);
		}

		// remove all attempts to change fixed settings
		m_cacheMap->DeleteDuplicates(&m_fixedMaps[i], FALSE);
	}

	// nothing else to store?
	if (m_cacheMap->IsEmpty() && !m_hasdeletedkeys)
	{
		if (flush) Flush();
		return;
	}
#endif // PREFSFILE_CASCADE

	// load the file with the local settings
	if (!m_localloaded)
	{
		OP_ASSERT(!m_hasdeletedkeys);
		OpStatus::Ignore(LoadLocalL());
	}

	// remove everything that's already in the file
	m_cacheMap->DeleteDuplicates(m_localMap, TRUE);

	// nothing else to store?
	if (m_cacheMap->IsEmpty() && !m_hasdeletedkeys)
	{
		if (flush) Flush();
		return;
	}

#ifdef PREFS_SOFT_COMMIT
	// Platforms that use disks that aren't very well re-writable
	// will check the force flag, and only write the file when it
	// is TRUE (on program exit). This may, however, hog memory if
	// a lot of changes have been made, as the cache map will be
	// kept in memory.
	if (!force)
	{
		if (flush && !m_hasdeletedkeys
# if defined PREFSFILE_CASCADE && defined PREFSFILE_WRITE_GLOBAL
		    && !m_hasdeletedglobalkeys
# endif
			)
		{
			Flush();
		}
		return;
	}
#endif

	/*

	  not implemented:

	  // load the file with the global settings
	  LoadGlobal();
	  m_cacheMap->RemoveDuplicates();

	  take care of this case:

	  global: fruit = apple
	  local:  fruit = orange
	  cache:  fruit = apple

	  the settings in both the local file and the cache
	  can be removed (because the value was reset)

	*/

	/*

	  not implemented?

	  global: fruit = apple
	  local:
	  cache:  fruit = apple

	  we shouldn't write anything to the local file. just remove from the cache.

	*/

#ifdef PREFSFILE_CASCADE
	// remove everything in the local file that's already in the global file
	for (int j = 0; j < m_numglobalfiles; ++ j)
	{
		m_localMap->DeleteDuplicates(&m_globalMaps[j], TRUE);
	}
#endif // PREFSFILE_CASCADE

	m_localMap->IncorporateL(m_cacheMap);

	OpStatus::Ignore(m_prefsAccessor->StoreL(m_file, m_localMap));

	m_hasdeletedkeys = FALSE;
	if (flush) Flush();

	m_cacheMap->FreeAll();

#endif // PREFSFILE_WRITE
}

void PrefsFile::SetFileL(const OpFileDescriptor *file)
{
	OP_DELETE(m_file); m_file = NULL;
	if (!file)
		LEAVE(OpStatus::ERR_NULL_POINTER);
	m_file = file->CreateCopy();
	LEAVE_IF_NULL(m_file);
	m_writable = m_file->IsWritable();
}

OpFileDescriptor *PrefsFile::GetFile()
{
	return m_file;
}

const OpFileDescriptor *PrefsFile::GetFile() const
{
	return m_file;
}

#ifdef PREFSFILE_CASCADE
void PrefsFile::SetGlobalFileL(const OpFileDescriptor *file, int priority)
{
	if (priority >= 0 && priority < m_numglobalfiles)
	{
		OP_DELETE(m_globalfiles[priority]); m_globalfiles[priority] = NULL;
		m_globalfiles[priority] = file->CreateCopy();
		LEAVE_IF_NULL(m_globalfiles[priority]);
	}
	else
	{
		OP_ASSERT(!"priority out of range");
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}
}

void PrefsFile::SetGlobalFixedFileL(const OpFileDescriptor *file, int priority)
{
	if (priority >= 0 && priority < m_numfixedfiles)
	{
		OP_DELETE(m_fixedfiles[priority]); m_fixedfiles[priority] = NULL;
		m_fixedfiles[priority] = file->CreateCopy();
		LEAVE_IF_NULL(m_fixedfiles[priority]);
	}
	else
	{
		OP_ASSERT(!"priority out of range");
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}
}

#ifdef PREFSFILE_GETFIXEDFILE
OpFileDescriptor *PrefsFile::GetGlobalFixedFile(int priority)
{
	OP_ASSERT(priority >= 0 && priority < m_numfixedfiles && "priority out of range");
	return (priority >= 0 && priority < m_numfixedfiles) ? m_fixedfiles[priority] : NULL;
}
#endif

#ifdef PREFSFILE_GETGLOBALFILE
OpFileDescriptor *PrefsFile::GetGlobalFile(int priority)
{
	OP_ASSERT(priority >= 0 && priority < m_numglobalfiles && "priority out of range");
	return (priority >= 0 && priority < m_numglobalfiles) ? m_globalfiles[priority] : NULL;
}
#endif
#endif // PREFSFILE_CASCADE

BOOL PrefsFile::DeleteKeyL(const uni_char *section, const uni_char *key)
{
	// Need to load files first
	OpStatus::Ignore(LoadAllL());

#ifdef PREFSFILE_CASCADE
	// no use trying to remove anything from
	// the global/fixed files
	if (!AllowedToChangeL(section, key))
	{
		return FALSE; // We're not allowed to do that
	}
#endif // PREFSFILE_CASCADE

	BOOL rc = FALSE;
	if (m_cacheMap->DeleteEntry(section, key))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	if (m_localMap->DeleteEntry(section, key))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	return rc;
}

#ifdef PREFSFILE_WRITE_GLOBAL
BOOL PrefsFile::DeleteKeyGlobalL(const uni_char *section, const uni_char *key, int priority)
{
	// Need to load files first
	LoadGlobalL(priority);

	BOOL rc = FALSE;
	if (m_globalCacheMaps[priority].DeleteEntry(section, key))
	{
		rc = m_hasdeletedglobalkeys[priority] = TRUE;
	}
	if (m_globalMaps[priority].DeleteEntry(section, key))
	{
		rc = m_hasdeletedglobalkeys[priority] = TRUE;
	}

	return rc;
}
#endif

BOOL PrefsFile::DeleteSectionL(const uni_char *section)
{
	// Need to load files first
	OpStatus::Ignore(LoadAllL());

	// no use trying to remove anything from
	// the global/fixed files
	BOOL rc = FALSE;
	if (m_cacheMap->DeleteSection(section))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	if (m_localMap->DeleteSection(section))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	return rc;
}

BOOL PrefsFile::DeleteSectionL(const char *section8)
{
	return DeleteSectionL(GetTempStringL(section8).CStr());
}

#ifdef PREFSFILE_RENAME_SECTION
BOOL PrefsFile::RenameSectionL(const uni_char *old_section_name, const uni_char *new_section_name)
{
	OP_ASSERT(old_section_name != NULL && new_section_name != NULL);

	// no use trying to remove anything from
	// the global/fixed files
	BOOL rc = FALSE;
	if (m_cacheMap->RenameSectionL(old_section_name, new_section_name))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	if (m_localMap->RenameSectionL(old_section_name, new_section_name))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	return rc;
}

BOOL PrefsFile::RenameSectionL(const char *old_section_name8, const char *new_section_name8)
{
	OP_ASSERT(old_section_name8 != NULL && new_section_name8 != NULL);

	const uni_char* tmp_new_section_name = NULL;
	OpStringC tmp_old_section_name(GetTempStringsL(old_section_name8, new_section_name8, tmp_new_section_name));

	return RenameSectionL(tmp_old_section_name.CStr(), tmp_new_section_name);
}
#endif // PREFSFILE_RENAME_SECTION

#ifdef PREFSFILE_WRITE_GLOBAL
BOOL PrefsFile::DeleteSectionGlobalL(const uni_char *section, int priority)
{
	// Need to load files first
	LoadGlobalL(priority);

	BOOL rc = FALSE;
	if (m_globalCacheMaps[priority].DeleteSection(section))
	{
		rc = m_hasdeletedglobalkeys[priority] = TRUE;
	}
	if (m_globalMaps[priority].DeleteSection(section))
	{
		rc = m_hasdeletedglobalkeys[priority] = TRUE;
	}
	return rc;
}
#endif

BOOL PrefsFile::ClearSectionL(const uni_char *section)
{
	// Need to load files first
	OpStatus::Ignore(LoadAllL());

	BOOL rc = FALSE;
	if (m_cacheMap->ClearSection(section))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	if (m_localMap->ClearSection(section))
	{
		rc = m_hasdeletedkeys = TRUE;
	}
	return rc;
}

BOOL PrefsFile::ClearSectionL(const char *section8)
{
	return ClearSectionL(GetTempStringL(section8).CStr());
}

#ifdef PREFS_DELETE_ALL_SECTIONS
void PrefsFile::DeleteAllSectionsL()
{
	// Clear the local map, but pretend we have it loaded, thus faking
	// that an empty file has been loaded.
	m_localMap->FreeAll();
	m_localloaded = TRUE;

	// Clear any unwritten changes.
	m_cacheMap->FreeAll();

	m_hasdeletedkeys = TRUE;
}
#endif

OP_STATUS PrefsFile::ReadValueL(const uni_char *section, const uni_char *key, const uni_char **value)
{

	/*

	  "generic read"

	  1) if requested key is in fixed map, return value
	  2) if requested key in cache map, return value
	  3) load and parse local settings file to local map
	  4) if requested key in local map, return value
	  5) if requested key in global cache map, return value
	  6) load and parse global settings file to global map
	  7) if requested key in global map, return value
	  8) error, default

	*/

	const uni_char *result = NULL;

#ifdef PREFSFILE_CASCADE
	// first, check the fixed settings
	for (int i = 0; i < m_numfixedfiles; ++ i)
	{
		if (m_fixedfiles[i])
		{
			result = m_fixedMaps[i].Get(section, key);
			if (result != NULL)
			{
				*value = result;
				return OpStatus::OK;
			}
		}
	}
#endif // PREFSFILE_CASCADE

	// check the cache
	result = m_cacheMap->Get(section, key);
	if (result != NULL)
	{
		*value = result;
		return OpStatus::OK;
	}

	// check the local file
	if (!m_localloaded)
	{
		LoadLocalL();
	}
	if (!m_localloaded)
	{
		*value = NULL;
		return OpStatus::OK;
	}

	result = m_localMap->Get(section, key);
	if (result != NULL)
	{
		*value = result;
		return OpStatus::OK;
	}

#ifdef PREFSFILE_CASCADE
	// check the global file
	for (int j = 0; j < m_numglobalfiles; ++ j)
	{
		if (m_globalfiles[j])
		{
#ifdef PREFSFILE_WRITE_GLOBAL
			result = m_globalCacheMaps[j].Get(section, key);
			if (result != NULL)
			{
				*value = result;
				return OpStatus::OK;
			}
#endif // PREFSFILE_WRITE_GLOBAL

			if (!m_globalsloaded[j])
			{
				LoadGlobalL(j);
			}
			if (m_globalsloaded[j])
			{
				*value = m_globalMaps[j].Get(section, key);
				if (*value)
				{
					return OpStatus::OK;
				}
			}
		}
	}
#endif // PREFSFILE_CASCADE

	*value = NULL;
	return OpStatus::OK;
}

#ifdef PREFSFILE_WRITE
OP_STATUS PrefsFile::WriteStringL(const OpStringC &section, const OpStringC &key, const OpStringC &value)
{
#if defined(PREFS_HAS_NULL)
	// If we don't have a preferences file, just ignore data
	if (!m_prefsAccessor) return OpStatus::OK;
#endif

	// if the key is in the fixed settings, the user are not
	// allowed to write that setting.

#ifdef PREFSFILE_CASCADE
	if (!AllowedToChangeL(section, key))
		return OpStatus::ERR_NO_ACCESS;
#endif // PREFSFILE_CASCADE

	m_cacheMap->SetL(section.CStr(), key.CStr(), value.CStr());
	return OpStatus::OK;
}

OP_STATUS PrefsFile::WriteIntL(const OpStringC &section, const OpStringC &key, int value)
{
	// We need log(256)/log(10) * sizeof(value) characters to store
	// the number, adding one for the rounding, one for the sign
	// and one for the terminator. 53/22 is a good approximation
	// of this.
	// Note that the multiplication happens before the integer division
	// so the concrete buflen for 32/64-bit ints are 12 and 22 respectively.

	static const int buflen = 3 + sizeof(value) * 53 / 22;

	uni_char buf[buflen]; /* ARRAY OK 2009-06-16 molsson */
	uni_ltoa(value, buf, 10);

	return WriteStringL(section, key, buf);
}
#endif // PREFSFILE_WRITE

#ifdef PREFSFILE_CASCADE
BOOL PrefsFile::AllowedToChangeL(const OpStringC &section, const OpStringC &key)
{
# ifdef PREFSFILE_HONOR_READONLY
	if (!m_writable)
		return FALSE;
# endif

	if (AllowedToChangeL(section))
		return TRUE;

	for (int i = 0; i < m_numfixedfiles; ++ i)
	{
		if (m_fixedMaps[i].Get(section.CStr(), key.CStr()))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL PrefsFile::AllowedToChangeL(const OpStringC8 &section, const OpStringC8 &key)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return AllowedToChangeL(sec, k);
}

BOOL PrefsFile::AllowedToChangeL(const OpStringC &section)
{
# ifdef PREFSFILE_HONOR_READONLY
	if (!m_writable)
		return FALSE;
# endif

	for (int i = 0; i < m_numfixedfiles; ++ i)
	{
		LoadFixedL(i);

		if (m_fixedMaps[i].FindSection(section.CStr()))
		{
			return FALSE;
		}
	}

	return TRUE;
}
#endif // PREFSFILE_CASCADE

int PrefsFile::ReadIntL(const OpStringC &section, const OpStringC &key,
	                    int dfval)
{
	const uni_char *result;
	// The below call is OOM safe since the OOM result is handled using
	// the leave mechanism. The return value from ReadValueL can be safely
	// ignored at this point (it might indicate a file not found).
	OpStatus::Ignore(ReadValueL(section.CStr(), key.CStr(), &result));

	if (result == NULL)
	{
		return dfval;
	}

	return uni_strtol(result, NULL, 0);
}

#ifdef PREFSFILE_WRITE_GLOBAL
OP_STATUS PrefsFile::WriteIntGlobalL(const OpStringC &section, const OpStringC &key, int value, int priority)
{
	// We need log(256)/log(10) * sizeof(value) characters to store
	// the number, adding one for the rounding, one for the sign
	// and one for the terminator. 53/22 is a good approximation
	// of this.
	// Note that the multiplication happens before the integer division
	// so the concrete buflen for 32/64-bit ints are 12 and 22 respectively.

	static const int buflen = 3 + sizeof(value) * 53 / 22;

	uni_char buf[buflen]; /* ARRAY OK 2009-06-16 molsson */
	uni_ltoa(value, buf, 10);
	return WriteStringGlobalL(section, key, buf, priority);
}

OP_STATUS PrefsFile::WriteStringGlobalL(const OpStringC &section, const OpStringC &key, const OpStringC &value, int priority)
{
	// Check that the priority is in range
	if (priority < 0 || priority > m_numglobalfiles)
	{
		OP_ASSERT(!"priority out of range");
		return OpStatus::ERR_OUT_OF_RANGE;
	}

#if defined(PREFS_HAS_NULL)
	// If we don't have a preferences file, just ignore data
	if (!m_prefsAccessor) return OpStatus::OK;
#endif

	// If we don't have a global file, return an error
	if (!m_globalfiles[priority]) return OpStatus::ERR_NULL_POINTER;

	m_globalCacheMaps[priority].SetL(section.CStr(), key.CStr(), value.CStr());
	return OpStatus::OK;

}
#endif // PREFSFILE_WRITE_GLOBAL

OpStringC PrefsFile::ReadStringL(const OpStringC &section,
							     const OpStringC &key,
							     const OpStringC &dfval)
{
	const uni_char *val;
	OpStatus::Ignore(ReadValueL(section.CStr(), key.CStr(), &val));

	if (val)
		RETURN_OPSTRINGC(val);
	RETURN_OPSTRINGC(dfval);
}

void PrefsFile::ReadStringL(const OpStringC &section,
							const OpStringC &key,
							OpString &result,
							const OpStringC &dfval)
{
	result.SetL(ReadStringL(section, key, dfval));
}

PrefsSectionInternal *PrefsFile::ReadSectionInternalL(const OpStringC &section)
{
	if (section.IsEmpty())
	{
		return NULL;
	}

	OpStackAutoPtr<PrefsSectionInternal> result(OP_NEW_L(PrefsSectionInternal, ()));
	result->ConstructL(NULL); // don't care about name

	PrefsSectionInternal *s;

#ifdef PREFSFILE_CASCADE
	// Must read the global files in reverse order to preserve priority
	for (int i = m_numglobalfiles - 1; i >= 0; -- i)
	{
		s = m_globalMaps[i].FindSection(section.CStr());
		if (s != NULL)
		{
			result->CopyKeysL(s);
		}
	}
#endif // PREFSFILE_CASCADE

	s = m_localMap->FindSection(section.CStr());
	if (s != NULL)
	{
		result->CopyKeysL(s);
	}

	s = m_cacheMap->FindSection(section.CStr());
	if (s != NULL)
	{
		result->CopyKeysL(s);
	}

#ifdef PREFSFILE_CASCADE
	// Must read the fixed files in reverse order to preserve priority
	for (int j = m_numfixedfiles - 1; j >= 0; -- j)
	{
		s = m_fixedMaps[j].FindSection(section.CStr());
		if (s != NULL)
		{
			result->CopyKeysL(s);
		}
	}
#endif // PREFSFILE_CASCADE

	return result.release();
}

#ifdef PREFS_READ_ALL_SECTIONS
unsigned int PrefsFile::ReadAllSectionsHelperL(PrefsMap *map, OpGenericStringHashTable *hash)
{
	unsigned int count = 0;
	const PrefsSectionInternal *section = map->Sections();
	while (section)
	{
		void *dummy;
		if (OpStatus::IsError(hash->GetData(section->HashableName(), &dummy)))
		{
			// Not in hash, add it
			hash->AddL(section->HashableName(), (void *) section->Name());
			++ count;
		}
		section = section->Suc();
	}

	return count;
}

void PrefsFile::ReadAllSectionsL(OpString_list &output)
{
	// Add the sections in all maps to a hash table
	OpStackAutoPtr<OpGenericStringHashTable> hash(OP_NEW_L(OpGenericStringHashTable, (TRUE)));

	unsigned int count = 0;
#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		count += ReadAllSectionsHelperL(&m_globalMaps[i], hash.get());
	}
	for (int j = 0; j < m_numfixedfiles; ++ j)
	{
		count += ReadAllSectionsHelperL(&m_fixedMaps[j], hash.get());
	}
#endif
	count += ReadAllSectionsHelperL(m_localMap, hash.get());
	count += ReadAllSectionsHelperL(m_cacheMap, hash.get());

	// Iterate over the hash table and add everything to our string list
	LEAVE_IF_ERROR(output.Resize(count));

	if (count)
	{
		unsigned int ix = 0;
		OpStackAutoPtr<OpHashIterator> iter(hash->GetIterator());
		LEAVE_IF_NULL(iter.get());
		OP_STATUS rc = iter->First();
		while (OpStatus::IsSuccess(rc) && ix < count)
		{
			output[ix ++].SetL(static_cast<uni_char *>(iter->GetData()));
			rc = iter->Next();
		}
		if (ix != count)
		{
			LEAVE(OpStatus::ERR);
		}
	}
}
#endif // PREFS_READ_ALL_SECTIONS

BOOL PrefsFile::IsSection(const uni_char *section)
{
	if (m_cacheMap->FindSection(section)
		|| m_localMap->FindSection(section))
	{
		return TRUE;
	}

#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		if (m_globalMaps[i].FindSection(section)
# ifdef PREFSFILE_WRITE_GLOBAL
			|| m_globalCacheMaps[i].FindSection(section)
# endif
			)
		{
			return TRUE;
		}
	}
	for (int j = 0; j < m_numfixedfiles; ++ j)
	{
		if (m_fixedMaps[j].FindSection(section))
		{
			return TRUE;
		}
	}
#endif // PREFSFILE_CASCADE

	return FALSE;
}

BOOL PrefsFile::IsSection(const char *section8)
{
	return IsSection(GetTempStringL(section8).CStr());
}

#ifdef PREFSFILE_WRITE_GLOBAL
BOOL PrefsFile::IsSectionLocal(const uni_char *section)
{
	return m_cacheMap->FindSection(section)
		|| m_localMap->FindSection(section);
}
#endif

BOOL PrefsFile::IsKey(const uni_char *section, const uni_char *key)
{
#if defined(PREFS_HAS_NULL)
	// If we don't have an accessor, we have no keys
	if (!m_prefsAccessor) return FALSE;
#endif

	PrefsSection *p;

	if ((p = m_cacheMap->FindSection(section)))
	{
		if (p->FindEntry(key))
			return TRUE;
	}

	if ((p = m_localMap->FindSection(section)))
	{
		if (p->FindEntry(key))
			return TRUE;
	}

#ifdef PREFSFILE_CASCADE
	for (int i = 0; i < m_numglobalfiles; ++ i)
	{
		if ((p = m_globalMaps[i].FindSection(section)))
		{
			if (p->FindEntry(key))
				return TRUE;
		}

# ifdef PREFSFILE_WRITE_GLOBAL
		if ((p = m_globalCacheMaps[i].FindSection(section)))
		{
			if (p->FindEntry(key))
				return TRUE;
		}
# endif
	}

	for (int j = 0; j < m_numfixedfiles; ++ j)
	{
		if ((p = m_fixedMaps[j].FindSection(section)))
		{
			if (p->FindEntry(key))
				return TRUE;
		}
	}
#endif // PREFSFILE_CASCADE

	return FALSE;
}

BOOL PrefsFile::IsKey(const char *section, const char *key)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return IsKey(sec.CStr(), k);
}

#ifdef PREFSFILE_WRITE_GLOBAL
BOOL PrefsFile::IsKeyLocal(const uni_char *section, const uni_char *key)
{
	PrefsSection *p;
	if ((p = m_cacheMap->FindSection(section)))
	{
		if (p->FindEntry(key))
			return TRUE;
	}

	if ((p = m_localMap->FindSection(section)))
	{
		if (p->FindEntry(key))
			return TRUE;
	}

	return FALSE;
}

BOOL PrefsFile::IsKeyLocal(const char *section, const char *key)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return IsKeyLocal(sec.CStr(), k);
}
#endif

OpStringC PrefsFile::GetTempStringsL(const OpStringC8 &sec, const OpStringC8 &key, const uni_char*& tmp_key)
{
	// This code uses m_tmp_buf to store a string with an embeeded NULL. Must be careful
	// to not trigger any code that assumes the string is NULL terminated. Such as
	// string expansion triggered through AppendL.
	m_tmp_buf.Clear();
	int sec_len = sec.Length() + 1;
	int key_len = key.Length() + 1;
	m_tmp_buf.ExpandL(sec_len+key_len);
	m_tmp_buf.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	m_tmp_buf.AppendL(sec.CStr());
	m_tmp_buf.AppendL(static_cast<uni_char>('\0')); // Will do cached_length++.
	m_tmp_buf.AppendL(key.CStr());
	tmp_key = m_tmp_buf.GetStorage()+sec_len;
	RETURN_OPSTRINGC(m_tmp_buf.GetStorage());
}

OpStringC PrefsFile::GetTempStringL(const char *sec)
{
	m_tmp_buf.Clear();
	m_tmp_buf.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	m_tmp_buf.AppendL(sec);
	RETURN_OPSTRINGC(m_tmp_buf.GetStorage());
}

int PrefsFile::ReadIntL(const OpStringC8 &section, const OpStringC8 &key, int dfval)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return ReadIntL(sec, k, dfval);
}

OpStringC PrefsFile::ReadStringL(const OpStringC8 &section, const OpStringC8 &key, const OpStringC &dfval)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	RETURN_OPSTRINGC(ReadStringL(sec, k, dfval));
}

void PrefsFile::ReadStringL(const OpStringC8 &section, const OpStringC8 &key, OpString &result, const OpStringC &dfval)
{
	result.SetL(ReadStringL(section,key,dfval));
}

#ifdef PREFSFILE_WRITE
OP_STATUS PrefsFile::WriteStringL(const OpStringC8 &section, const OpStringC8 &key, const OpStringC &value)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return WriteStringL(sec, k, value);
}

OP_STATUS PrefsFile::WriteIntL(const OpStringC8 &section, const OpStringC8 &key, int value)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return WriteIntL(sec, k, value);
}
#endif // PREFSFILE_WRITE

#ifdef PREFSFILE_WRITE_GLOBAL
OP_STATUS PrefsFile::WriteStringGlobalL(const OpStringC8 &section, const OpStringC8 &key, const OpStringC &value, int priority)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return WriteStringGlobalL(sec, k, value, priority);
}

OP_STATUS PrefsFile::WriteIntGlobalL(const OpStringC8 &section, const OpStringC8 &key, int value, int priority)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return WriteIntGlobalL(sec, k, value, priority);
}
#endif // PREFSFILE_WRITE_GLOBAL

BOOL PrefsFile::DeleteKeyL(const char *section, const char *key)
{
	const uni_char *k;
	OpStringC sec(GetTempStringsL(section, key, k));
	return DeleteKeyL(sec.CStr(), k);
}

#ifdef PREFSFILE_IMPORT
void PrefsFile::ImportL(PrefsFile *source_in)
{
	// Take over the pointer, making sure it gets autodestructed when
	// function exits
	OpStackAutoPtr<PrefsFile> source(source_in);

	// Check that the source is properly committed
	if (source->m_hasdeletedkeys ||
		!source->m_cacheMap->IsEmpty())
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	// Import (destroys the source map)
	m_cacheMap->IncorporateL(source->m_localMap);
}
#endif

PrefsSection *PrefsFile::ReadSectionL(const OpStringC &section)
{
	return ReadSectionInternalL(section);
}

PrefsSection *PrefsFile::ReadSectionL(const OpStringC8 &section8)
{
	return ReadSectionL(GetTempStringL(section8.CStr()));
}
#endif // PREFS_HAS_PREFSFILE
