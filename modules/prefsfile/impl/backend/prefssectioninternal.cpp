/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h"
#include "modules/prefsfile/impl/backend/prefsentryinternal.h"
#include "modules/prefsfile/impl/backend/prefshashfunctions.h"
#include "modules/util/opstrlst.h"

/** First-phase constructor for the class. Cannot be called directly. */
PrefsSection::PrefsSection()
	: m_name(NULL)
#ifndef PREFSMAP_USE_CASE_SENSITIVE
	, m_lname(NULL)
#endif
#ifdef PREFSMAP_USE_HASH
	, m_entries_hash(NULL)
#endif
{
}

PrefsSectionInternal::PrefsSectionInternal()
	: PrefsSection()
{
}

/**
 * Second phase constructor for the class.
 * @param name The name of the new section.
 */
void PrefsSection::ConstructL(const uni_char *name)
{
	if (name)
	{
		AllocateNewNameL(name);
	}

#ifdef PREFSMAP_USE_HASH
	m_entries_hash = OP_NEW_L(OpHashTable, (g_opera->prefsfile_module.GetHashFunctions()));
#endif
}

/**
 * The destructor for the class.
 * Frees all associated data structures.
 */
PrefsSection::~PrefsSection()
{
#ifdef PREFSMAP_USE_HASH
	OP_DELETE(m_entries_hash);
#endif
	m_entries.Clear();
	OP_DELETEA(m_name);
}

/**
 * (Re-)Allocate the memory used to hold the name
 */
void PrefsSection::AllocateNewNameL(const uni_char *name)
{
	OP_ASSERT(name != NULL);

	OP_DELETEA(m_name);
	m_name = NULL;

	size_t namelen = uni_strlen(name) + 1;
#ifdef PREFSMAP_USE_CASE_SENSITIVE
	m_name = OP_NEWA_L(uni_char, namelen);
	uni_strcpy(m_name, name);
#else
	m_name = OP_NEWA_L(uni_char, namelen * 2);
	uni_strcpy(m_name, name);

	// For speed optimization during compare, we keep a copy of the
	// section name in lowercase
	m_lname = m_name + namelen;
	uni_strcpy(m_lname, name);
	Unicode::Lower(m_lname, TRUE);
#endif
}

/**
 * Removes and deletes the provided entry.
 * @param entry the entry to delete
 */
void PrefsSection::DeleteEntry(PrefsEntryInternal *entry)
{
	entry->Out();
#ifdef PREFSMAP_USE_HASH
	entry->OutOfHash(m_entries_hash);
#endif
	OP_DELETE(entry);
}

BOOL PrefsSectionInternal::IsSameAs(const uni_char *name_lowercase) const
{
#ifdef PREFSMAP_USE_CASE_SENSITIVE
	return uni_strcmp(m_name, name_lowercase) == 0;
#else
	return uni_strcmp(m_lname, name_lowercase) == 0;
#endif
}

BOOL PrefsSectionInternal::DeleteEntry(const uni_char *key)
{
	PrefsEntryInternal *entry = FindEntry(key);
	if (entry != NULL)
	{
		PrefsSection::DeleteEntry(entry);
		return TRUE;
	}
	return FALSE;
}

void PrefsSectionInternal::Clear()
{
#ifdef PREFSMAP_USE_HASH
	m_entries_hash->RemoveAll();
#endif
	m_entries.Clear();
}

void PrefsSectionInternal::SetL(const uni_char *key, const uni_char *value)
{
	PrefsEntryInternal *entry = FindEntry(key);
	if (entry)
	{
		entry->SetL(value);
	}
	else
	{
		NewEntryL(key, value);
	}
}

const uni_char *PrefsSection::Get(const uni_char *key) const
{
	PrefsEntry *entry = FindEntry(key);
	if (entry == NULL) return NULL;
	return entry->Get();
}

PrefsEntry *PrefsSection::FindEntry(const uni_char *key) const
{
	if (!key) return NULL;

	PrefsEntryInternal *entry = static_cast<PrefsEntryInternal *>(m_entries.First());
	if (!entry)
	{
		// Make sure we have at least one entry to compare with.
		return NULL;
	}

#if defined PREFSMAP_USE_CASE_SENSITIVE
	// Case insensitivity handled in hashing functions, or there's no
	// case insensitivity at all
	const uni_char *lowercase_key = key;
#else
	// Global variable m_temp_buffer is used for performance gains
	// because heap allocation is slow and the needed buffer is too
	// large for the stack. I do not dare to use the TempBufUni here,
	// since it might already be in use. This buffer is allocated in
	// the global PrefsfileModule object.
	uni_char *lowercase_key = g_opera->prefsfile_module.m_temp_buffer;
	uni_strlcpy(lowercase_key, key, PREFSFILE_MODULE_TMPBUF_LEN);
	Unicode::Lower(lowercase_key, TRUE);
#endif

#ifdef PREFSMAP_USE_HASH
	// Look up the entry in the hash table
	void *hash_lookup_entry; // to avoid "dereferencing type-punned pointer will break strict-aliasing rules"
	if (OpStatus::IsSuccess(m_entries_hash->GetData(static_cast<void *>(&lowercase_key),
	                                                &hash_lookup_entry)))
	{
		return reinterpret_cast<PrefsEntry *>(hash_lookup_entry);
	}
#else
	// Search for the entry in the linked list
	do
	{
		if (entry->IsSameAs(lowercase_key))
		{
			return entry;
		}
		entry = entry->Suc();
	} while (entry != NULL);
#endif // PREFSMAP_USE_HASH

	// None found
	return NULL;
}

#ifdef USE_UTIL_OPSTRING_LIST
OpString_list *PrefsSection::GetKeyListL() const
{
	int numentries = Number();

	if (numentries)
	{
		// Allocate room for all the keys
		OpStackAutoPtr<OpString_list> keys(OP_NEW_L(OpString_list, ()));
		LEAVE_IF_ERROR(keys->Resize(numentries));

		// Copy
		const PrefsEntry *entry = Entries();
		for (int i = 0; entry && i < numentries; ++ i)
		{
			keys->Item(i).SetL(entry->Key());
			entry = entry->Suc();
		}

		return keys.release();
	}

	return NULL;
}
#endif

PrefsEntryInternal *PrefsSectionInternal::NewEntryL(const uni_char *key, const uni_char *value)
{
	OpStackAutoPtr<PrefsEntryInternal> entry(OP_NEW_L(PrefsEntryInternal, ()));
	entry->ConstructL(key, value);

	PrefsEntryInternal *safeptr = entry.release(); // It's safe now
	safeptr->Into(&m_entries);
#ifdef PREFSMAP_USE_HASH
	// If this fails, the key will be in the linked list anyway.
	safeptr->IntoHashL(m_entries_hash);
#endif
	return safeptr;
}

void PrefsSectionInternal::DeleteDuplicates(const PrefsSectionInternal *section, BOOL identical)
{
	// Remove settings that already are in the provided section.
	// If identical is true, only settings with equal value
	// are removed, others are kept.
	PrefsEntryInternal *entry = static_cast<PrefsEntryInternal *>(m_entries.First());
	while (entry != NULL)
	{
		PrefsEntryInternal *next = entry->Suc();
		const PrefsEntryInternal *other = section->FindEntry(entry->Key());
		if (other != NULL)
		{
			if (!identical ||
			    (entry->Value() == NULL && other->Value() == NULL) ||
				((entry->Value() != NULL && other->Value() != NULL) &&
			     (uni_strcmp(entry->Value(), other->Value()) == 0)))
			{
				PrefsSection::DeleteEntry(entry);
			}
		}
		entry = next;
	}
}

void PrefsSectionInternal::IncorporateL(PrefsSectionInternal *section)
{
	PrefsEntryInternal *source = static_cast<PrefsEntryInternal *>(section->m_entries.First());

	while (source != NULL)
	{
		PrefsEntryInternal *next = source->Suc();

		// If the target key exists, we must first delete it
		PrefsEntryInternal *target = FindEntry(source->Key());
		if (target)
		{
			target->Out();
#ifdef PREFSMAP_USE_HASH
			target->OutOfHash(m_entries_hash);
#endif
			OP_DELETE(target);
		}

		// Move the source key from the old section to the new.
		// First try to add it to the hash table. If that fails,
		// we leave the key where it was. A subsequent call to
		// this function will add it again. The only thing we
		// lose is the old key, but we're supposed to do that anyway.
#ifdef PREFSMAP_USE_HASH
		source->IntoHashL(m_entries_hash); // now in both hash tables
#endif

		// Okay, we were able to add it. Now remove from the source
		// section.
		source->Out();

#ifdef PREFSMAP_USE_HASH
		// Remove the source from its hash table. Doing it for each
		// entry is slow, but since IntoHashL() above may fail, not
		// doing it would leave the store in an inconsitent state.
		source->OutOfHash(section->m_entries_hash);
#endif

		// Add it to our list.
		source->Into(&m_entries);

		source = next;
	}
}

void PrefsSection::CopyKeysL(const PrefsSection *from)
{
	const PrefsEntryInternal *other = static_cast<PrefsEntryInternal *>(from->m_entries.First());
	while (other != NULL)
	{
		static_cast<PrefsSectionInternal *>(this)->SetL(other->Key(), other->Value());
		other = other->Suc();
	}
}
