/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/prefsfile/impl/backend/prefsmap.h"

PrefsMap::PrefsMap()
	: m_current_section(NULL)
#ifdef PREFSMAP_SECTION_HASH
	, m_section_count(0)
	, m_section_hash(NULL)
#endif
{
}

PrefsMap::~PrefsMap()
{
	FreeAll();
}

void PrefsMap::FreeAll()
{
	m_sections.Clear();
#ifdef PREFSMAP_SECTION_HASH
	OP_DELETE(m_section_hash);
	m_section_hash = NULL;
	m_section_count = 0;
#endif
	m_current_section = NULL;
}

PrefsSectionInternal *PrefsMap::FindSection(const uni_char *name) const
{
	if (!name) return NULL;

	PrefsSectionInternal *section = FirstSection();
	if (!section)
	{
		// Make sure we have at least one section to compare with
		return NULL;
	}

#ifdef PREFSMAP_USE_CASE_SENSITIVE
# define lowercase_name name
#else
	// Global variable m_temp_buffer is used for performance gains
	// because heap allocation is slow and the needed buffer is too
	// large for the stack. I do not dare to use the TempBufUni here,
	// since it might already be in use. This buffer is allocated in
	// the global PrefsfileModule object. Convert section name to
	// lowercase and use that in repeated comparisons below.
	uni_char *lowercase_name = g_opera->prefsfile_module.m_temp_buffer;
	uni_strlcpy(lowercase_name, name, PREFSFILE_MODULE_TMPBUF_LEN);
	Unicode::Lower(lowercase_name, TRUE);
#endif

	// Check current section
	if (m_current_section)
	{
		if (m_current_section->IsSameAs(lowercase_name))
		{
			return m_current_section;
		}
	}

#ifdef PREFSMAP_SECTION_HASH
	if (m_section_hash)
	{
		// Use the hash table, if available
		OP_STATUS rc = m_section_hash->GetData(lowercase_name, &section);
		return OpStatus::IsSuccess(rc) ? section : NULL;
	}
#endif // PREFSMAP_SECTION_HASH

	// Traverse section list
	do
	{
		if (section->IsSameAs(lowercase_name))
		{
			return section;
		}
		section = section->Suc();
	} while (section != NULL);

	// None found
	return NULL;

#ifdef PREFSMAP_USE_CASE_SENSITIVE
# undef lowercase_name
#endif
}

PrefsSectionInternal *PrefsMap::GetSectionL(const uni_char *name)
{
	PrefsSectionInternal *section = FindSection(name);
	if (section == NULL)
	{
		// Create the section and add it to the linked list
		OpStackAutoPtr<PrefsSectionInternal> new_section(OP_NEW_L(PrefsSectionInternal, ()));
		new_section->ConstructL(name);
		new_section->Into(&m_sections);

		// Since m_sections now contains the pointer, we must release it from
		// the auto pointer.
		section = new_section.release();

#ifdef PREFSMAP_SECTION_HASH
		// Add the newly created section to the hash table. Please note that
		// if this fails, our linked list will contain something the hash table
		// doesn't. This should not cause problems since the next GetSectionL()
		// will add one that is hashed. The written ini file may look odd,
		// though, with an empty section before the real one.
		++ m_section_count;
		AddToSectionHashL(section);
#endif
	}

	return section;
}

void PrefsMap::SetL(const uni_char *section, const uni_char *key, const uni_char *value)
{
	m_current_section = GetSectionL(section);
	m_current_section->SetL(key, value);
}

const uni_char *PrefsMap::Get(const uni_char *section, const uni_char *key)
{
	m_current_section = FindSection(section);
	if (m_current_section)
	{
		return m_current_section->Get(key);
	}
	return NULL;
}

BOOL PrefsMap::DeleteEntry(const uni_char *section, const uni_char *key)
{
	PrefsSectionInternal *s = FindSection(section);
	if (s != NULL)
	{
		return s->DeleteEntry(key);
	}
	return FALSE;
}

BOOL PrefsMap::DeleteSection(const uni_char *section)
{
	PrefsSectionInternal *s = FindSection(section);
	if (s != NULL)
	{
		DeleteSection(s);
		return TRUE;
	}
	return FALSE;
}

void PrefsMap::DeleteSection(PrefsSectionInternal *section)
{
	section->Out();
	if (m_current_section == section) m_current_section = NULL;

#ifdef PREFSMAP_SECTION_HASH
	OP_ASSERT(m_section_count != 0);
	-- m_section_count;
	RemoveFromSectionHash(section);
#endif

	OP_DELETE(section);
}

#ifdef PREFSFILE_RENAME_SECTION

BOOL PrefsMap::RenameSectionL(const uni_char *old_section_name, const uni_char *new_section_name)
{
	OP_ASSERT(old_section_name != NULL && new_section_name != NULL);

	PrefsSectionInternal *section = FindSection(old_section_name);
	if (section == NULL)
	{
		return FALSE;
	}
#ifdef PREFSMAP_SECTION_HASH
	// remove it from the hash table with the old name
	RemoveFromSectionHash(section);
#endif // PREFSMAP_SECTION_HASH

	// rename it
	OP_STATUS rc;

	TRAP(rc, section->SetNameL(new_section_name));
	if(OpStatus::IsError(rc))
	{
		section->Out();
		if (m_current_section == section) m_current_section = NULL;
		OP_DELETE(section);
		LEAVE(rc);
	}

#ifdef PREFSMAP_SECTION_HASH
	// add it with the new name
	AddToSectionHashL(section);
#endif // PREFSMAP_SECTION_HASH
	return TRUE;
}
#endif // PREFSFILE_RENAME_SECTION

#ifdef PREFS_HAS_PREFSFILE
BOOL PrefsMap::ClearSection(const uni_char *section)
{
	PrefsSectionInternal *s = FindSection(section);

	if (s != NULL)
	{
		s->Clear();
		return TRUE;
	}
	return FALSE;
}
#endif

// remove settings that already are in the provided map
// if identical is true, only settings with equal value
// are removed, other are kept
// empty sections also get removed

void PrefsMap::DeleteDuplicates(const PrefsMap *map, BOOL identical)
{
	PrefsSectionInternal *section = FirstSection();
	while (section != NULL)
	{
		PrefsSectionInternal *next = section->Suc();
		const PrefsSectionInternal *other = map->FindSection(section->Name());
		if (other != NULL)
		{
			section->DeleteDuplicates(other, identical);
			if (section->IsEmpty())
			{
				DeleteSection(section);
			}
		}
		section = next;
	}
}

// moves everything from the provided map
// to this map

void PrefsMap::IncorporateL(PrefsMap *from)
{
	PrefsSectionInternal *source = from->FirstSection();

	while (source != NULL)
	{
		PrefsSectionInternal *next = source->Suc();

		PrefsSectionInternal *target = FindSection(source->Name());

		// If the target does not exist, we move the entire section,
		// otherwise we copy key by key.
		if (target)
		{
			target->IncorporateL(source);

			// If copying succeeded, kill source. If we didn't we
			// didn't copy everything, but that's ok.
			source->Out();
		}
		else
		{
			// First try to add it to the hash table. If that fails, we
			// leave everything as is.
#ifdef PREFSMAP_SECTION_HASH
			AddToSectionHashL(source); // now in both hash tables
#endif

			// Okay, we were able to add it. Now remove it from the source
			// map. We do RemoveFromSectionHash() outside the if.
			source->Out();

			// And add it to our.
			source->Into(&m_sections);
#ifdef PREFSMAP_SECTION_HASH
			++ m_section_count;
#endif
		}

#ifdef PREFSMAP_SECTION_HASH
		// Do some housekeeping.
		from->RemoveFromSectionHash(source);
		-- from->m_section_count;
#endif

		// If we copied the section, delete the source.
		if (target)
		{
			OP_DELETE(source);
		}

		source = next;
	}
}

#ifdef PREFSMAP_SECTION_HASH
void PrefsMap::BuildSectionHashL()
{
	OP_ASSERT(!m_section_hash);
	// The created hash table is always case sensitive, since we add pointers
	// to the lowercase version of the section name when we're operating in
	// case insensitive mode.
	m_section_hash = OP_NEW(OpStringHashTable<PrefsSectionInternal>, (TRUE));
	if (!m_section_hash)
	{
		// Just ignore that we couldn't create the table. We can do just
		// as well without it for now, and we will try again next time we
		// add a section anyway.
		return;
	}

	// Now add all our sections to the freshly created hash.
	PrefsSectionInternal *section = FirstSection();
	OP_ASSERT(section); // We should only be called if we have sections
	while (section)
	{
		m_section_hash->AddL(section->HashableName(), section);
		section = section->Suc();
	}
}

void PrefsMap::AddToSectionHashL(PrefsSectionInternal *section)
{
	OP_ASSERT(section);

	if (m_section_hash)
	{
		// Add to the hash if it's already created.
		m_section_hash->AddL(section->HashableName(), section);
	}
	else if (m_section_count > PREFS_SECTIONS_BEFORE_HASH)
	{
		// Create the hash if we've reached our threshold, this will
		// include the section we just created since it is already
		// added to the linked list.
		BuildSectionHashL();

		// This function may be called before section is added to the
		// m_sections list (e.g. in IncorporateL(PrefsMap *from)); so
		// check if section is in m_section_hash, if not add it:
		if (m_section_hash && !m_section_hash->Contains(section->HashableName()))
			m_section_hash->AddL(section->HashableName(), section);
	}
}

void PrefsMap::RemoveFromSectionHash(PrefsSectionInternal *section)
{
	if (m_section_hash)
	{
		PrefsSectionInternal *section_in_hash;

		// We can ignore the result value; if we get an out of memory because
		// the hash table could not be shrunk, we still have the old index.
		OpStatus::Ignore(m_section_hash->Remove(section->HashableName(),
		                                        &section_in_hash));

		OP_ASSERT(section_in_hash == section);
	}
}
#endif // PREFSMAP_SECTION_HASH
