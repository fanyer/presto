/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5NAMEMAPPER_H
#define HTML5NAMEMAPPER_H

#include "modules/logdoc/markup.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"
#include "modules/logdoc/src/html5/elementhashbase.h"
#include "modules/logdoc/src/html5/attrhashbase.h"

class HTML5TokenBuffer;

/**
 * This class is used to map strings to element and attribute codes and back
 * again.
 */
class HTML5NameMapper
{
public:
	~HTML5NameMapper();

	/// Secondary constructor.
	void		InitL();

	/**
	 * Gets the element code from a null terminated string.
	 * @param[in] tag_name Null terminated string with the tag name.
	 * @param[in] case_sensitive If TRUE, the name must match the canonical name exactly.
	 * @param[in] ns The namespace the tag should be interpreted in.
	 * @returns The type matching the name, or HTE_UNKNOWN if no match is found.
	 */
	Markup::Type	GetTypeFromName(const uni_char *tag_name, BOOL case_sensitive, Markup::Ns ns);
	/**
	 * Gets the element code from a string.
	 * @param[in] tag_name String with the tag name.
	 * @param[in] tag_len The length, in uni_chars, of the name
	 * @param[in] case_sensitive If TRUE, the name must match the canonical name exactly.
	 * @param[in] ns The namespace the tag should be interpreted in.
	 * @returns The type matching the name, or HTE_UNKNOWN if no match is found.
	 */
	Markup::Type	GetTypeFromName(const uni_char *tag_name, unsigned tag_len, BOOL case_sensitive, Markup::Ns ns);
	/**
	 * Gets the element code from a null terminated string. If the string
	 * has different casing in different namespaces, it returns HTE_UNKNOWN.
	 * Used for getting tag types in CSS when any namespace can be used.
	 * @param[in] tag_name Null terminated string with the tag name.
	 * @param[in] case_sensitive If TRUE, the name must match the canonical name exactly.
	 * @returns The type matching the name, or HTE_UNKNOWN if no match is found or ambiguous.
	 */
	Markup::Type	GetTypeFromNameAmbiguous(const uni_char *tag_name, BOOL case_sensitive);
	/**
	 * Gets the element code from a token buffer used by the HTML parser.
	 * Always case insensitive and always treated as HTML.
	 * @param[in] ns The namespace the tag should be interpreted in.
	 * @returns The type matching the name, or HTE_UNKNOWN if no match is found.
	 */
	Markup::Type	GetTypeFromTokenBuffer(const HTML5TokenBuffer *tag_buffer)
	{
		HTML5HashEntry *entry = m_tag_hash_map[tag_buffer->GetHashValue() % HTML5_TAG_HASH_SIZE];
		if (entry)
		{
			if (uni_stri_eq(entry->m_key->m_name, tag_buffer->GetBuffer()))
				return static_cast<Markup::Type>(entry->m_data);

			entry = entry->m_next;
			if (entry && uni_stri_eq(entry->m_key->m_name, tag_buffer->GetBuffer()))
				return static_cast<Markup::Type>(entry->m_data);
		}

		return Markup::HTE_UNKNOWN;
	}
	/**
	 * Returns the canonical name for the element code given.
	 * @param[in] type The type to find the name for.
	 * @param[in] ns The namespace the type belongs to.
	 * @param[in] uppercase If TRUE, an uppercased version of the name is returned.
	 * @returns A string containing the name of the element. The string is owned
	 *          by the name-mapper.
	 */
	const uni_char*		GetNameFromType(Markup::Type type, Markup::Ns ns, BOOL uppercase);

	/**
	 * Gets the attribute code from a null terminated string.
	 * @param[in] attr_name Null terminated string with the attribute name.
	 * @param[in] case_sensitive If TRUE, the name must match the canonical name exactly.
	 * @param[in] ns The namespace the attribute should be interpreted in.
	 * @returns The type matching the name, or HA_XML if no match is found.
	 */
	Markup::AttrType	GetAttrTypeFromName(const uni_char *attr_name, BOOL case_sensitive, Markup::Ns ns);
	/**
	 * Gets the attribute code from a string.
	 * @param[in] attr_name String with the attribute name.
	 * @param[in] attr_len The length, in uni_chars, of the name
	 * @param[in] case_sensitive If TRUE, the name must match the canonical name exactly.
	 * @param[in] ns The namespace the attribute should be interpreted in.
	 * @returns The type matching the name, or HA_XML if no match is found.
	 */
	Markup::AttrType	GetAttrTypeFromName(const uni_char *attr_name, unsigned attr_len, BOOL case_sensitive, Markup::Ns ns);
	/**
	 * Gets the attribute code from a token buffer used by the HTML parser.
	 * Always case insensitive and always treated as HTML.
	 * @param[in] ns The namespace the attribute should be interpreted in.
	 * @returns The type matching the name, or HA_XML if no match is found.
	 */
	Markup::AttrType	GetAttrTypeFromTokenBuffer(const HTML5TokenBuffer *attr_buffer)
	{
		HTML5HashEntry *entry = m_attr_hash_map[attr_buffer->GetHashValue() % HTML5_ATTR_HASH_SIZE];
		if (entry)
		{
			if (uni_stri_eq(entry->m_key->m_name, attr_buffer->GetBuffer()))
				return static_cast<Markup::AttrType>(entry->m_data);

			entry = entry->m_next;
			if (entry && uni_stri_eq(entry->m_key->m_name, attr_buffer->GetBuffer()))
				return static_cast<Markup::AttrType>(entry->m_data);
		}

		return Markup::HA_XML;
	}
	/**
	 * Returns the canonical name for the attribute code given.
	 * @param[in] type The type to find the name for.
	 * @param[in] ns The namespace the type belongs to.
	 * @param[in] uppercase If TRUE, an uppercased version of the name is returned.
	 * @returns A string containing the name of the attribute. The string is owned
	 *          by the name-mapper.
	 */
	const uni_char*		GetAttrNameFromType(Markup::AttrType type, Markup::Ns ns, BOOL uppercase);

	/**
	 * Used to find out if a given type can have different character casing 
	 * depending on the namespace.
	 * @param[in] type The element type.
	 * @returns TRUE if the type has string representations with different case.
	 */
	BOOL				HasAmbiguousName(Markup::Type type);

private:

	/**
	 * Helper structure used to extract meta data for an entry in the name tables.
	 * Format:
	 * offset,   ns,   mixed-case-string,  flattened-string
	 * For all lowercase names, the format would be something like this:
	 *      0,    0,               'div',
	 * For mixed case strings, something like this:
	 *     14,  SVG,     'foreignObject', 'foreignobject'
	 * If m_offset is non-null, it means there is a case flattened version of the string
	 * that should be used if the namespace does not match the one found in m_ns.
	 */
	struct NameMetaData
	{
		/// The offset from m_name where the case flattened version of the string starts
		char		m_offset;
		/// The namespace where the mixed case string should be used.
		char		m_ns;
		/// The start of the name string.
		uni_char	m_name[1]; /* ARRAY OK 2011-09-08 danielsp */
	};

	class HTML5HashEntry;
	friend class HTML5HashEntry;

	/**
	 * Used to store the name strings in the element and attribute hash tables
	 */
	class HTML5HashEntry
	{
	public:
		HTML5HashEntry(const NameMetaData *key, unsigned data) : m_key(key), m_data(data), m_next(NULL) {}
		const NameMetaData*	m_key; /// The meta data containing among others the name string
		unsigned			m_data; /// The element code matching the name
		HTML5HashEntry*		m_next; /// In case of a collision, this points to the next entry matching the hash value
	};

	/// Hash table for the element names
	HTML5HashEntry*		m_tag_hash_map[HTML5_TAG_HASH_SIZE];
	/// Hash table for the attribute names
	HTML5HashEntry*		m_attr_hash_map[HTML5_ATTR_HASH_SIZE];

	/**
	 * Calculate the hash value for a null terminated string.
	 * @param[in] name String to get hash value for.
	 * @param[out] hashval Contains the calculated hash value after the call.
	 */
	void			HashString(const uni_char *name, unsigned &hashval);
	/**
	 * Calculate the hash value for a string with length.
	 * @param[in] name String to get hash value for.
	 * @param[in] name_len Length of name in uni_chars.
	 * @param[out] hashval Contains the calculated hash value after the call.
	 */
	void			HashString(const uni_char *name, unsigned name_len, unsigned &hashval);

	/// Used to add element names to the element type hash table during initialization.
	void			AddTagL(const NameMetaData *meta_data, unsigned code);
	/// Used to add attribute names to the attribute type hash table during initialization.
	void			AddAttrL(const NameMetaData *meta_data, unsigned code);

	/// Helper function to match a string with length to a hash table entry
	BOOL			EntryMatches(const NameMetaData *data, const uni_char *name, unsigned name_len, BOOL case_sensitive, Markup::Ns ns);
	/// Helper function to match a null terminated string to a hash table entry
	BOOL			EntryMatches(const NameMetaData *data, const uni_char *name, BOOL case_sensitive, Markup::Ns ns);
	/// Helper function to match a null terminated string to a hash table entry
	BOOL			EntryMatches(const NameMetaData *data, const uni_char *name);
};

#ifdef HTML5_STANDALONE
extern HTML5NameMapper* g_html5_name_mapper;
#endif

#endif // HTML5NAMEMAPPER_H
