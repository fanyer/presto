/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/prefsfile/impl/backend/prefsentryinternal.h"
#include "modules/util/OpHashTable.h"

/** First-phase constructor for the class. Cannot be called directly. */
PrefsEntry::PrefsEntry()
	: m_key(NULL)
#if !defined PREFSMAP_USE_CASE_SENSITIVE
	, m_lkey(NULL)
#endif
	, m_value(NULL)
{
}

PrefsEntryInternal::PrefsEntryInternal()
	: PrefsEntry()
{
}

/**
 * Second phase constructor for the class.
 * @param key The key name of the entry
 * @param value The initial value of the entry (may be NULL)
 * @return Status of the operation
 */
void PrefsEntry::ConstructL(const uni_char *key, const uni_char *value)
{
	size_t keylen = uni_strlen(key) + 1;
	size_t valuelen = value ? uni_strlen(value) + 1 : 0;
#if defined PREFSMAP_USE_CASE_SENSITIVE
	m_key = OP_NEWA_L(uni_char, keylen + valuelen);
	m_value = value ? m_key + keylen : NULL;
#else
	m_key = OP_NEWA_L(uni_char, keylen * 2 + valuelen);
	m_lkey = m_key + keylen;
	m_value = value ? m_lkey + keylen : NULL;
#endif

	uni_strcpy(m_key, key);

#if !defined PREFSMAP_USE_CASE_SENSITIVE
	// For speed optimization during compare, we keep a copy of the
	// section name in lowercase
	uni_strcpy(m_lkey, key);
	Unicode::Lower(m_lkey, TRUE);
#endif

	if (value)
	{
		uni_strcpy(m_value, value);
	}
}

PrefsEntry::~PrefsEntry()
{
	OP_DELETEA(m_key);
}


void PrefsEntryInternal::SetL(const uni_char *value)
{
	if (!value)
	{
		// A NULL value is different from an empty string.
		m_value = NULL;
		return;
	}

	size_t valuelen = uni_strlen(value);
	if (!m_value || valuelen > uni_strlen(m_value))
	{
		// Need to reallocate the entire key
		size_t keylen = uni_strlen(m_key) + 1;
#if defined PREFSMAP_USE_CASE_SENSITIVE
		uni_char *new_data = OP_NEWA_L(uni_char, keylen + valuelen + 1);
		m_value = new_data + keylen;
#else
		uni_char *new_data = OP_NEWA_L(uni_char, keylen * 2 + valuelen + 1);

		uni_strcpy(new_data + keylen, m_lkey);
		m_lkey = new_data + keylen;

		m_value = new_data + keylen * 2;
#endif
		uni_strcpy(new_data, m_key);

		// Delete the old data and point m_key to the new
		OP_DELETEA(m_key);
		m_key = new_data;
	}

	// Copy the new value (either to a newly allocated string, or to
	// overwrite the old). If there is room, we do not reallocate. Note
	// that we will forget about the extra size, so if it grows back
	// again, we will re-allocate.
	uni_strcpy(m_value, value);
}

#ifdef PREFSMAP_USE_HASH
void PrefsEntryInternal::OutOfHash(OpHashTable *hash)
{
	void *entry;
# if defined PREFSMAP_USE_CASE_SENSITIVE
	// Remove() does not return any OOM errors, so it should be safe
	// ignoring the status.
	OpStatus::Ignore(hash->Remove(&m_key, &entry));
# else
	OpStatus::Ignore(hash->Remove(&m_lkey, &entry));
# endif
	OP_ASSERT(entry == reinterpret_cast<void *>(this));
}
#endif

#ifdef PREFSMAP_USE_HASH
void PrefsEntryInternal::IntoHashL(OpHashTable *hash)
{
# if defined PREFSMAP_USE_CASE_SENSITIVE
	hash->AddL(&m_key, this);
# else
	hash->AddL(&m_lkey, this);
# endif
}
#endif
