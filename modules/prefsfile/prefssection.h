/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PREFSSECTION_H
#define PREFSSECTION_H

#include "modules/util/simset.h"
#include "modules/prefsfile/prefsentry.h"

class OpString_list;
class PrefsEntryInternal;
class OpHashTable;

/** A class that represents a section of entries.
  *
  * Used as a return type for PrefsFile::ReadSectionL(). Implemented
  * internally using a private class, this is the API provided to code
  * outside of the prefsfile module.
  */
class PrefsSection : public Link
{
public:
	/**
	 * The destructor for the class.
	 * Frees all associated data structures.
	 */
	virtual ~PrefsSection();

	/**
	 * Retrieve the name of the section
	 * @return the name of the section
	 */
	inline const uni_char *Name() const { return m_name; }

#ifdef PREFSFILE_RENAME_SECTION
	/**
	 * Sets the name of the section
	 */
	inline void SetNameL(const uni_char *name) {  AllocateNewNameL(name); }
#endif // PREFSFILE_RENAME_SECTION

	/**
	 * Return the number of keys in this section.
	 * @return the number of keys in the section
	 */
	inline int Number() const { return m_entries.Cardinal(); }

	/**
	 * Get the associated value of a key.
	 * @return the associated value of the key
	 * @param key the name of the key
	 */
	const uni_char *Get(const uni_char *key) const;

	/**
	 * Retrieve a entry with a given name.
	 * @return The found entry or NULL if not found
	 * @param key The key name of the entry
	 */
	PrefsEntry *FindEntry(const uni_char *key) const;

	/**
	 * Retrieve the list of entries in the section.
	 * @return The list of entries in the section.
	 */
	inline const PrefsEntry *Entries() const
	{ return static_cast<const PrefsEntry *>(m_entries.First()); }

#ifdef USE_UTIL_OPSTRING_LIST
	/**
	 * Retrieve a list of the keys in the section.
	 *
	 * @return The list of keys in the section. The pointer must be deleted
	 * by the caller.
	 */
	OpString_list *GetKeyListL() const;
#endif

	/**
	 * Copies all the content from the provided section to this section.
	 * If tries to copy a already existing key in the target section,
	 * it will be overwritten.
	 *
	 * @param from The section to copy.
	 * @return Status of the operation.
	 */
	void CopyKeysL(const PrefsSection *from);

	/**
	 * Check if the section contains any entries.
	 * @return true if empty
	 */
	inline BOOL IsEmpty() const { return m_entries.Empty(); }

	/**
	 * Return the next section in the list.
	 */
	inline const PrefsSection *Suc() const
	{ return static_cast<const PrefsSection *>(Link::Suc()); }

protected:
	uni_char *m_name;		///< The name of the section
#ifndef PREFSMAP_USE_CASE_SENSITIVE
	uni_char *m_lname;		///< Lowercase name of section; used for searches
#endif
	Head m_entries;			///< The list of entries
#ifdef PREFSMAP_USE_HASH
	OpHashTable *m_entries_hash;	///< Hash of the entries
#endif

	/* Hide constructor, this is an API class only. */
	PrefsSection();
	void ConstructL(const uni_char *);

	void DeleteEntry(PrefsEntryInternal *);
	void AllocateNewNameL(const uni_char *name);
};

#endif // PREFSSECTION_H
