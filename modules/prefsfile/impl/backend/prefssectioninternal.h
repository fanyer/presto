/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSSECTIONINTERNAL_H
#define PREFSSECTIONINTERNAL_H

#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/impl/backend/prefsentryinternal.h"

class PrefsEntryInternal;

/**
 * Implementation of the PrefsSection class. Represents a section of
 * entries.
 *
 * This API is internal to the prefsfile module, but is also referenced
 * by the prefs module.
 */
class PrefsSectionInternal : public PrefsSection
{
public:
	/**
	 * A constructor for the class.
	 */
	PrefsSectionInternal();

	/**
	 * Second phase constructor for the class.
	 * @param name The name of the new section
	 */
	inline void ConstructL(const uni_char *name)
	{ PrefsSection::ConstructL(name); }

	/**
	 * Set the associated value of a key.
	 * @param key the name of the key
	 * @param value the value of the key
	 * @return Status of the operation
	 */
	void SetL(const uni_char *key, const uni_char *value);

	/**
	 * Compares the names of the sections. The section must be converted
	 * to lower case, unless PREFSMAP_USE_CASE_SENSITIVE is defined.
	 *
	 * @return true if the names is equal
	 * @param name_lowercase The name of the section you are searching for,
	 *                       in lowercase
	 */
	BOOL IsSameAs(const uni_char *name_lowercase) const;

	/**
	 * Retrieve a entry with a given name.
	 * @return The found entry or NULL if not found
	 * @param key The key name of the entry
	 */
	inline PrefsEntryInternal *FindEntry(const uni_char *key) const
	{
		return static_cast<PrefsEntryInternal *>(PrefsSection::FindEntry(key));
	}

	/**
	 * Creates a new entry.
	 *
	 * @param key The name of the entry
	 * @param value The initial value of the entry
	 * @return The newly created entry
	 */
	PrefsEntryInternal *NewEntryL(const uni_char *key, const uni_char *value);

	/**
	 * Deletes the specified entry.
	 * @param key the key
	 * @return whether the key was found and deleted
	 */
	BOOL DeleteEntry(const uni_char *key);

	/**
	 * Clears all entries from this section. This is faster than calling
	 * DeleteEntry() on each individual key.
	 */
	void Clear();

	/**
	 * Deletes all entries that also is in the provided section
	 * if identical is true, only remove if the values of
	 * the keys are the same.
	 * @param section The section to compare with
	 * @param identical Require the same key values
	 */
	void DeleteDuplicates(const PrefsSectionInternal *section, BOOL identical);

	/**
	 * Moves all the content from the provided section to this section.
	 * @param section The section to incorporate
	 */
	void IncorporateL(PrefsSectionInternal *section);

#if defined PREFSMAP_SECTION_HASH || defined PREFS_READ_ALL_SECTIONS
	/**
	 * Retrieve the hashable name of the section. This name can always
	 * be used for direct string comparisons, no matter whether we are
	 * using case sensitive mode or not.
	 * @return the lowecase name of the section
	 */
	inline const uni_char *HashableName() const
	{
#ifndef PREFSMAP_USE_CASE_SENSITIVE
		return m_lname;
#else
		return m_name;
#endif
	}
#endif

	/**
	 * Return the next section in the list.
	 */
	inline PrefsSectionInternal *Suc()
	{ return static_cast<PrefsSectionInternal *>(Link::Suc()); }

	/** @overload */
	inline const PrefsSectionInternal *Suc() const
	{ return static_cast<const PrefsSectionInternal *>(Link::Suc()); }

private:
};

#endif
