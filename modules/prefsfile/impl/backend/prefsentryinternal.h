/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSENTRYINTERNAL_H
#define PREFSENTRYINTERNAL_H

#include "modules/prefsfile/prefssection.h" // To get PREFSMAP_USE_HASH
#include "modules/prefsfile/prefsentry.h"

/**
 * Implementation of the PrefsEntry class. Represents an entry in a set of
 * preferences.
 *
 * This API is internal to the prefsfile module, but is also referenced
 * by the prefs module.
 */
class PrefsEntryInternal : public PrefsEntry
{
public:
	/**
	 * A constructor for the class.
	 */
	PrefsEntryInternal();

	/**
	 * Second phase constructor for the class.
	 * @param name The key name of the entry
	 * @param value The value of the entry. Default to NULL
	 * @return Status of the operation
	 */
	inline void ConstructL(const uni_char *name, const uni_char *value = NULL)
	{ PrefsEntry::ConstructL(name, value); }

	/**
	 * Set the associated value.
	 *
	 * @param value The new value
	 * @return Status of the operation
	 */
	void SetL(const uni_char *value);

#ifndef PREFSMAP_USE_HASH
	/**
	 * Compares the name of the keys. The key name must be converted to
	 * lower case, unless PREFSMAP_USE_CASE_SENSITIVE is defined.
	 *
	 * @return true if it is the same key
	 * @param lowercase_key The name of the key you are searching for,
	 *                      in lowercase
	 */
	inline BOOL IsSameAs(const uni_char *lowercase_key) const
	{
		if ((m_key == NULL) || (lowercase_key == NULL)) return FALSE;
# ifdef PREFSMAP_USE_CASE_SENSITIVE
		return (uni_strcmp(m_key, lowercase_key) == 0);
# else
		return (uni_strcmp(m_lkey, lowercase_key) == 0);
# endif
	}
#endif

#ifdef PREFSMAP_USE_HASH
	/**
	 * Enter this entry into the specified hash table.
	 */
	void IntoHashL(OpHashTable *);

	/**
	 * Remove this entry from the specified hash table.
	 */
	void OutOfHash(OpHashTable *);
#endif

	/**
	 * Return the next entry in the list.
	 */
	inline PrefsEntryInternal *Suc() const
	{ return static_cast<PrefsEntryInternal *>(Link::Suc()); }
};

#endif
