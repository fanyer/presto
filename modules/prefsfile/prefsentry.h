/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PREFSENTRY_H
#define PREFSENTRY_H

#include "modules/util/simset.h"

/**
  * A class that represents an entry in a set of preferences.
  *
  * Used as a return type for PrefsSection::FindEntry() and
  * PrefsSection::Entries(). Implemented internally using a private class,
  * this is the API provided to code outside the prefsfile module.
  */
class PrefsEntry : public Link
{
public:
	/**
	 * The destructor for the class.
	 * Frees all associated data structures.
	 */
	virtual ~PrefsEntry();

	/**
	 * Retrieve the associated value.
	 * @return The value string
	 */
	inline const uni_char *Get()    const { return Value(); }

	/**
	 * Return the key name.
	 * @return The key name
	 */
	inline const uni_char *Key()    const { return m_key;   }

	/**
	 * Return the associated value.
	 * @return the value string
	 */
	inline const uni_char *Value() const { return m_value; } 

	/**
	 * Return the next entry in the list.
	 */
	inline const PrefsEntry *Suc() const
	{ return static_cast<const PrefsEntry *>(Link::Suc()); }

protected:
	uni_char *m_key;		///< The key name.
#if !defined PREFSMAP_USE_CASE_SENSITIVE
	uni_char *m_lkey;		///< Lowercase name of key; used for searches.
#endif
	uni_char *m_value;		///< The value string

	/* Hide constructor, this is an API class only. */
	PrefsEntry();
	void ConstructL(const uni_char *, const uni_char *);
};

#endif // PREFSENTRY_H
