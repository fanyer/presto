/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/logdoc_module.h"

#include "modules/logdoc/html5namemapper.h"

#include "modules/logdoc/src/html5/html5base.h"
#include "modules/logdoc/src/html5/elementnames.h"
#include "modules/logdoc/src/html5/attrnames.h"


HTML5NameMapper::~HTML5NameMapper()
{
	for (unsigned i = 0; i < HTML5_TAG_HASH_SIZE; i++)
	{
		HTML5HashEntry *next = m_tag_hash_map[i];
		while (next)
		{
			HTML5HashEntry *to_delete = next;
			next = next->m_next;
			OP_DELETE(to_delete);
		}
	}

	for (unsigned i = 0; i < HTML5_ATTR_HASH_SIZE; i++)
	{
		HTML5HashEntry *next = m_attr_hash_map[i];
		while (next)
		{
			HTML5HashEntry *to_delete = next;
			next = next->m_next;
			OP_DELETE(to_delete);
		}
	}
}

void HTML5NameMapper::InitL()
{
	// reset pointers
	for (unsigned i = 0; i < HTML5_TAG_HASH_SIZE; i++)
		m_tag_hash_map[i] = NULL;

	for (unsigned i = 0; i < HTML5_ATTR_HASH_SIZE; i++)
		m_attr_hash_map[i] = NULL;

	for (unsigned i = Markup::HTE_FIRST; i < Markup::HTE_LAST; i++)
		AddTagL(reinterpret_cast<const NameMetaData*>(&g_html5_tag_names[g_html5_tag_indices[i - Markup::HTE_FIRST]]), i);

	for (unsigned i = Markup::HA_FIRST; i < Markup::HA_LAST; i++)
		AddAttrL(reinterpret_cast<const NameMetaData*>(&g_html5_attr_names[g_html5_attr_indices[i - Markup::HA_FIRST]]), i);
}

Markup::Type HTML5NameMapper::GetTypeFromName(const uni_char *tag_name, unsigned tag_len, BOOL case_sensitive, Markup::Ns ns)
{
	unsigned hashval = HTML5_TAG_HASH_BASE;
	HashString(tag_name, tag_len, hashval);

	HTML5HashEntry *entry = m_tag_hash_map[hashval % HTML5_TAG_HASH_SIZE];
	if (entry)
	{
		if (EntryMatches(entry->m_key, tag_name, tag_len, case_sensitive, ns))
			return static_cast<Markup::Type>(entry->m_data);

		entry = entry->m_next;
		if (entry && EntryMatches(entry->m_key, tag_name, tag_len, case_sensitive, ns))
			return static_cast<Markup::Type>(entry->m_data);
	}

	return Markup::HTE_UNKNOWN;
}

Markup::Type HTML5NameMapper::GetTypeFromName(const uni_char *tag_name, BOOL case_sensitive, Markup::Ns ns)
{
	unsigned hashval = HTML5_TAG_HASH_BASE;
	HashString(tag_name, hashval);

	HTML5HashEntry *entry = m_tag_hash_map[hashval % HTML5_TAG_HASH_SIZE];
	if (entry)
	{
		if (EntryMatches(entry->m_key, tag_name, case_sensitive, ns))
			return static_cast<Markup::Type>(entry->m_data);

		entry = entry->m_next;
		if (entry && EntryMatches(entry->m_key, tag_name, case_sensitive, ns))
			return static_cast<Markup::Type>(entry->m_data);
	}

	return Markup::HTE_UNKNOWN;
}

Markup::Type HTML5NameMapper::GetTypeFromNameAmbiguous(const uni_char *tag_name, BOOL case_sensitive)
{
	unsigned hashval = HTML5_TAG_HASH_BASE;
	HashString(tag_name, hashval);

	HTML5HashEntry *entry = m_tag_hash_map[hashval % HTML5_TAG_HASH_SIZE];
	if (entry)
	{
		// First check if the entry has different cased versions of the string
		if (entry->m_key->m_offset != 0)
		{
			// If it does, check case-insensitively that it is in fact a matching entry
			if (uni_stri_eq(entry->m_key->m_name, tag_name))
				return Markup::HTE_UNKNOWN;
			// If not, this is the wrong hash entry, move to next entry.
		}
		else if (case_sensitive)
		{
			// only one string representation, and we care about character
			// case so check if it matches exactly.
			if (uni_strcmp(entry->m_key->m_name, tag_name) == 0)
				return static_cast<Markup::Type>(entry->m_data);
			// If not, this is the wrong hash entry, move to next entry.
		}
		else if (uni_stri_eq(entry->m_key->m_name, tag_name))
			// only one string representation, check if it matches insensitively
			return static_cast<Markup::Type>(entry->m_data);

		entry = entry->m_next;
		// Do the same for the next entry.
		if (entry)
		{
			if (entry->m_key->m_offset != 0)
			{
				if (uni_stri_eq(entry->m_key->m_name, tag_name))
					return Markup::HTE_UNKNOWN;
			}
			else if (case_sensitive)
			{
				if (uni_strcmp(entry->m_key->m_name, tag_name) == 0)
					return static_cast<Markup::Type>(entry->m_data);
			}
			else if (uni_stri_eq(entry->m_key->m_name, tag_name))
				return static_cast<Markup::Type>(entry->m_data);
		}
	}

	return Markup::HTE_UNKNOWN;
}

const uni_char* HTML5NameMapper::GetNameFromType(Markup::Type type, Markup::Ns ns, BOOL uppercase)
{
	if (!Markup::HasNameEntry(type))
		return NULL;

	if (uppercase)
		return g_html5_tag_names_upper[type - Markup::HTE_FIRST];
	else
	{
		unsigned name_idx = g_html5_tag_indices[type - Markup::HTE_FIRST];
		const NameMetaData *meta_data = reinterpret_cast<const NameMetaData*>(&g_html5_tag_names[name_idx]);
		const uni_char *tag_name = meta_data->m_name;
		if (meta_data->m_offset != 0 && ns != meta_data->m_ns)
			tag_name += meta_data->m_offset;

		return tag_name;
	}
}

Markup::AttrType HTML5NameMapper::GetAttrTypeFromName(const uni_char *attr_name, unsigned attr_len, BOOL case_sensitive, Markup::Ns ns)
{
	if (!attr_name)
		return Markup::HA_XML;

	unsigned hashval = HTML5_ATTR_HASH_BASE;
	HashString(attr_name, attr_len, hashval);

	HTML5HashEntry *entry = m_attr_hash_map[hashval % HTML5_ATTR_HASH_SIZE];
	if (entry)
	{
		if (EntryMatches(entry->m_key, attr_name, attr_len, case_sensitive, ns))
			return static_cast<Markup::AttrType>(entry->m_data);

		entry = entry->m_next;
		if (entry && EntryMatches(entry->m_key, attr_name, attr_len, case_sensitive, ns))
			return static_cast<Markup::AttrType>(entry->m_data);
	}

	return Markup::HA_XML;
}

Markup::AttrType HTML5NameMapper::GetAttrTypeFromName(const uni_char *attr_name, BOOL case_sensitive, Markup::Ns ns)
{
	if (!attr_name)
		return Markup::HA_XML;

	unsigned hashval = HTML5_ATTR_HASH_BASE;
	HashString(attr_name, hashval);

	HTML5HashEntry *entry = m_attr_hash_map[hashval % HTML5_ATTR_HASH_SIZE];
	if (entry)
	{
		if (EntryMatches(entry->m_key, attr_name, case_sensitive, ns))
			return static_cast<Markup::AttrType>(entry->m_data);

		entry = entry->m_next;
		if (entry && EntryMatches(entry->m_key, attr_name, case_sensitive, ns))
			return static_cast<Markup::AttrType>(entry->m_data);
	}

	return Markup::HA_XML;
}

const uni_char* HTML5NameMapper::GetAttrNameFromType(Markup::AttrType type, Markup::Ns ns, BOOL uppercase)
{
	if (!Markup::HasAttrNameEntry(type))
		return NULL;

	if (uppercase)
		return g_html5_attr_names_upper[type - Markup::HA_FIRST];
	else
	{
		unsigned name_idx = g_html5_attr_indices[type - Markup::HA_FIRST];
		const NameMetaData *meta_data = reinterpret_cast<const NameMetaData*>(&g_html5_attr_names[name_idx]);
		const uni_char *attr_name = meta_data->m_name;
		if (meta_data->m_offset != 0 && ns != meta_data->m_ns)
			attr_name += meta_data->m_offset;

		return attr_name;
	}
}

BOOL HTML5NameMapper::HasAmbiguousName(Markup::Type type)
{
	if (Markup::HasNameEntry(type))
	{
		unsigned name_idx = g_html5_tag_indices[type - Markup::HTE_FIRST];
		const NameMetaData *meta_data = reinterpret_cast<const NameMetaData*>(&g_html5_tag_names[name_idx]);
		if (meta_data->m_offset != 0)
			return TRUE;
	}

	return FALSE;
}

void HTML5NameMapper::HashString(const uni_char *name, unsigned &hashval)
{
	while (*name)
	{
		HTML5_HASH_FUNCTION(hashval, *name);
		name++;
	}
}

void HTML5NameMapper::HashString(const uni_char *name, unsigned name_len, unsigned &hashval)
{
	while (name_len && *name)
	{
		HTML5_HASH_FUNCTION(hashval, *name);
		name++;
		name_len--;
	}
}

void HTML5NameMapper::AddTagL(const NameMetaData *meta_data, unsigned code)
{
	unsigned position = HTML5_TAG_HASH_BASE;
	HashString(meta_data->m_name, position);
	position = position % HTML5_TAG_HASH_SIZE;

	if (m_tag_hash_map[position]) // collision
	{
		OP_ASSERT(!m_tag_hash_map[position]->m_next);
		m_tag_hash_map[position]->m_next = OP_NEW_L(HTML5HashEntry, (meta_data, code));
	}
	else
		m_tag_hash_map[position] = OP_NEW_L(HTML5HashEntry, (meta_data, code));
}

void HTML5NameMapper::AddAttrL(const NameMetaData *meta_data, unsigned code)
{
	unsigned position = HTML5_ATTR_HASH_BASE;
	HashString(meta_data->m_name, position);
	position = position % HTML5_ATTR_HASH_SIZE;

	if (m_attr_hash_map[position]) // collision
	{
		OP_ASSERT(!m_attr_hash_map[position]->m_next);
		m_attr_hash_map[position]->m_next = OP_NEW_L(HTML5HashEntry, (meta_data, code));
	}
	else
		m_attr_hash_map[position] = OP_NEW_L(HTML5HashEntry, (meta_data, code));
}

BOOL HTML5NameMapper::EntryMatches(const NameMetaData *data, const uni_char *name, unsigned name_len, BOOL case_sensitive, Markup::Ns ns)
{
	if (case_sensitive)
	{
		if (data->m_offset != 0 && data->m_ns != ns)
			return uni_strncmp(data->m_name + data->m_offset, name, name_len) == 0;
		return uni_strncmp(data->m_name, name, name_len) == 0;
	}
	return uni_strni_eq(data->m_name, name, name_len);
}

BOOL HTML5NameMapper::EntryMatches(const NameMetaData *data, const uni_char *name, BOOL case_sensitive, Markup::Ns ns)
{
	if (case_sensitive)
	{
		if (data->m_offset != 0 && ns != data->m_ns)
			return uni_strcmp(data->m_name + data->m_offset, name) == 0;
		return uni_strcmp(data->m_name, name) == 0;
	}
	return uni_stri_eq(data->m_name, name);
}
