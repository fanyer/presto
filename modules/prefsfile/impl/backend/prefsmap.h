/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSMAP_H
#define PREFSMAP_H

#ifdef PREFSMAP_SECTION_HASH
# include "modules/util/OpHashTable.h"
#endif
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h"
#include "modules/util/OpHashTable.h"

#if defined PREFSMAP_SECTION_HASH && !defined PREFS_SECTIONS_BEFORE_HASH
/**
 * Threshold of number of sections required before we waste time, space
 * and effort building a hash table.
 */
# define PREFS_SECTIONS_BEFORE_HASH 6
#endif // PREFSMAP_SECTION_HASH && !PREFS_SECTIONS_BEFORE_HASH

/**
 * @file prefsmap.h
 * Classes representing the structure of a preferences file.
 */

class PrefsEntry;
class PrefsEntryInternal;

/**
 * A representation of the logical structure of the contents in a preference
 * file. This class represents a set of entries, providing a mapping from
 * key to value, divided into sections in a set of preferences. Implemented
 * both as linked lists and as a hash table on most platforms because the
 * ordering needs to be kept whilst keeping low access times.
 */
class PrefsMap
{
public:

	/**
	 * A constructor for the class.
	 */
	PrefsMap();

	/**
	 * The destructor for the class. Frees all data by calling FreeAll()
	 */
	~PrefsMap();

	/**
	 * Frees all data associated with the map.
	 */
	void FreeAll();
	  
	/**
	 * Retrieve a section with a specific name.
	 * @return The requested section or NULL if not found
	 * @param name The name of the section
	 */
	PrefsSectionInternal *FindSection(const uni_char *name) const;

	/**
	 * Retrieve a section with a specific name. If the section
	 * isn't found, create a new section.
	 * @return The requested section.
	 * @param name The name of the section.
	 */
	PrefsSectionInternal *GetSectionL(const uni_char *name);

	/**
	 * Retrieves the value of a key in a specific section.
	 * @return The corresponding value to the key
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 */
	const uni_char *Get(const uni_char *section, const uni_char *key);

	/**
	 * Set the value of a key.
	 * @param section The section that contains the key
	 * @param key     The key in section
	 * @param value   The value to set
	 */
	void SetL(const uni_char *section, const uni_char *key, const uni_char *value);

	/**
	 * Check if contains any keys.
	 * @return true if empty
	 */
	inline BOOL IsEmpty() const { return m_sections.Empty(); }

	/**
	 * Retrieves the list of sections.
	 * @return The list of sections
	 */
	const PrefsSectionInternal *Sections() const
	{ return FirstSection(); }

	/**
	 * Deletes all keys that also is in the provided map
	 * if identical is true, only remove if the values of
	 * the keys are the same.
	 * @param map The map to compare
	 * @param identical Require the same key values
	 */
	void DeleteDuplicates(const PrefsMap *map, BOOL identical);

	/**
	 * Moves all the content from the provided map to this map.
	 * @param map The map to incorporate
	 */
	void IncorporateL(PrefsMap *map);

	/**
	 * Deletes the specified entry.
	 * @param section the section
	 * @param key the key
	 * @return whether the key was found and deleted
	 */
	BOOL DeleteEntry(const uni_char *section, const uni_char *key);

	/**
	 * Deletes a section from the list.
	 * @param section The section to be deleted
	 * @return whether the key was found and deleted
	 */
	BOOL DeleteSection(const uni_char *section);

#ifdef PREFSFILE_RENAME_SECTION
	/**
	 * Renames a section
	 *
	 * @param old_section_name The old section name
	 * @param new_section_name The new section name
	 * @return whether the section was found and renamed
	 */
	BOOL RenameSectionL(const uni_char *old_section_name, const uni_char *new_section_name);
#endif // PREFSFILE_RENAME_SECTION

#ifdef PREFS_HAS_PREFSFILE
	/**
	 * Clears all entries in a section.
	 * @param section The section to be cleared
	 * @return whether the section was found and cleared
	 */
	BOOL ClearSection(const uni_char *section);
#endif

private:
	/**
	 * Retrieve the first section in the list.
	 * @return The first section in the list
	 */
	PrefsSectionInternal *FirstSection() const
	{ return static_cast<PrefsSectionInternal *>(m_sections.First()); }

	/**
	 * Deletes a section from the list.
	 * @param section The section to be deleted
	 */
	void DeleteSection(PrefsSectionInternal *section);

	/**
	 * The list of sections
	 */
	Head m_sections;

	/**
	 * We keep an internal state to make it convenient and fast
	 * adding multiple keys to the same section.
	 */
	PrefsSectionInternal *m_current_section;

#ifdef PREFSMAP_SECTION_HASH
	/**
	 * We count sections to know when a hash table might be useful.
	 */
	unsigned int m_section_count;

	/**
	 * A pointer to a section hash. When the number of sections is low,
	 * this will be NULL and unused. When the number exceeds the threshold
	 * defined by PREFS_SECTIONS_BEFORE_HASH, GetSectionL() will create
	 * the hash table.
	 */
	OpStringHashTable<PrefsSectionInternal> *m_section_hash;

	/**
	 * Builds the section hash table.
	 */
	void BuildSectionHashL();

	/**
	 * Helper that keeps the hash updated when adding sections.
	 */
	void AddToSectionHashL(PrefsSectionInternal *section);

	/**
	 * Helper that keeps the hash updated when deleting sections.
	 */
	void RemoveFromSectionHash(PrefsSectionInternal *section);
#endif // PREFSMAP_SECTION_HASH
};

#endif
