/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1999-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/utility/charsetnames.h"
#ifndef ENCODINGS_REGTEST
# include "modules/encodings/tablemanager/optablemanager.h"
# include "modules/util/str.h"
# include "modules/hardcore/mem/mem_man.h"
#endif

// ==== CONSTANTS ==========================================================

// Hackery to allow initialization of the static arrays of strings also for
// platforms that do not support static arrays of strings.
#ifdef HAS_COMPLEX_GLOBALS

# define CHARSET_ALIAS_DECLARE const char *const CharsetManager::m_charset_aliases[] =
# define CHARSET_ALIAS_INIT_START
# define CHARSET_ALIAS(x, y) x, y
# define CHARSET_ALIAS_INIT_END

# define MIB_CHARSET_DECLARE const CharsetManager::mibentry CharsetManager::m_mib[] =
# define MIB_CHARSET_INIT_START
# define MIB_CHARSET(x, y) { x, y }
# define MIB_CHARSET_INIT_END

#else

# define CHARSET_ALIAS_DECLARE BOOL CharsetManager::InitCharsetAliasesL()
# define CHARSET_ALIAS_INIT_START \
	int i = 0; \
	m_charset_aliases = new(ELeave) const char *[CHARSET_ALIASES_COUNT * 2];
# define CHARSET_ALIAS(x, y) m_charset_aliases[i ++] = x, \
                             m_charset_aliases[i ++] = y
# define CHARSET_ALIAS_INIT_END \
	; \
	OP_ASSERT(i == 2 * CHARSET_ALIASES_COUNT); \
	return TRUE;

# define MIB_CHARSET_DECLARE BOOL CharsetManager::InitMIBEntriesL()
# define MIB_CHARSET_INIT_START \
	int i = 0; \
	m_mib = new(ELeave) struct mibentry [MIB_ENTRIES_COUNT];
# define MIB_CHARSET(x, y) m_mib[i].mib = x, m_mib[i ++].charset = y
# define MIB_CHARSET_INIT_END \
	; \
	OP_ASSERT(i == MIB_ENTRIES_COUNT); \
	return TRUE;

#endif

// Character set names and their aliases.
#include "modules/encodings/utility/aliases.inl"

#ifdef ENCODINGS_HAVE_MIB_SUPPORT
// MIB identifiers and their mapping.
# include "modules/encodings/utility/mib.inl"
#endif

CharsetManager::CharsetManager()
#ifndef HAS_COMPLEX_GLOBALS
	: m_charset_aliases(NULL)
# ifdef ENCODINGS_HAVE_MIB_SUPPORT
	, m_mib(NULL)
# endif
#endif
{
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	m_all_supported_charsets = NULL;
#endif
}

CharsetManager::~CharsetManager()
{
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	// m_all_supported_charsets[0] points to start of the "charsets" table.
	if (m_all_supported_charsets && m_all_supported_charsets[0])
	{
		g_table_manager->Release(m_all_supported_charsets[0]);
		OP_DELETEA(m_all_supported_charsets);
	}
#endif

#ifndef HAS_COMPLEX_GLOBALS
	OP_DELETEA(m_charset_aliases);
	m_charset_aliases = NULL;

# ifdef ENCODINGS_HAVE_MIB_SUPPORT
	OP_DELETEA(m_mib);
	m_mib = NULL;
# endif
#endif
}

void CharsetManager::ConstructL()
{
#ifndef HAS_COMPLEX_GLOBALS
	InitCharsetAliasesL();
# ifdef ENCODINGS_HAVE_MIB_SUPPORT
	InitMIBEntriesL();
# endif
#endif

#ifndef ENCODINGS_REGTEST
	// Set up the first/null entry in table for GetCharsetID()
	OP_ASSERT(1 < ENCODINGS_CHARSET_CACHE_SIZE);
	CharsetInfo *ci = CharsetInfo::createConstantRecord(NULL);
	LEAVE_IF_NULL(ci);
	OP_STATUS rc = m_charset_info.Add(ci);
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(ci);
		LEAVE(rc);
	}
#endif // !ENCODINGS_REGTEST

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	// Load the "charsets" table and initialize m_all_supported_charsets

	long length;
	const char *charsets =
		reinterpret_cast<const char *>(g_table_manager->Get("charsets", length));
	if (charsets)
	{
		// This table contains one long string that is the names of the
		// 8-bit charsets (terminated with null-bytes) concatenated into
		// a long string. The table is allocated for the entire life-time
		// of this object, so all we need to do is to make a new array
		// holding pointers to the strings.

		// Count the number of charsets in the table
		OP_ASSERT(length > 0 && charsets[length - 1] == 0);
		int charset_count = 0;
		const char *pos = charsets;
		while (pos < charsets + length)
		{
			if (*pos == 0)
				charset_count++;
			pos++;
		}

		// Allocate and fill new table
		m_all_supported_charsets = OP_NEWA_L(const char*, charset_count + 1);
		m_all_supported_charsets[0] = charsets;
		int ix = charset_count ? 1 : 0;
		pos = charsets;
		while (pos + 1 < charsets + length)
		{
			if (*pos == 0)
			{
				m_all_supported_charsets[ix] = pos + 1;
				ix++;
			}
			pos++;
		}

		m_all_supported_charsets[ix] = NULL;

# ifndef ENCODINGS_REGTEST
		// Set up the table for GetCharsetID() from m_all_supported_charsets
		OP_ASSERT(charset_count < ENCODINGS_CHARSET_CACHE_SIZE - 1);
		for (int iy = 0; iy < charset_count; iy ++)
		{
			CharsetInfo *ci = CharsetInfo::createConstantRecord(m_all_supported_charsets[iy]);
			LEAVE_IF_NULL(ci);
			OP_STATUS rc = m_charset_info.Add(ci);
			if (OpStatus::IsError(rc))
			{
				OP_DELETE(ci);
				LEAVE(rc);
			}
		}
# endif // !ENCODINGS_REGTEST
	}
#else
# ifndef ENCODINGS_REGTEST
	// Set up the table for GetCharsetID() from m_charset_aliases
	for (unsigned int i = 1; i < CHARSET_ALIASES_COUNT * 2; i += 2)
	{
		// We have a lot of duplicates here, so check that we haven't
		// added this one already
		BOOL already_added = FALSE;
		for (unsigned int j = 0;
		     j < m_charset_info.GetCount() && !already_added; j ++)
		{
			const char *charset = m_charset_info.Get(j)->getCharset();
			if(charset)
				already_added =	!op_strcmp(charset, m_charset_aliases[i]);
		}

		if (!already_added)
		{
			OP_ASSERT(m_charset_info.GetCount() < ENCODINGS_CHARSET_CACHE_SIZE - 1);
			CharsetInfo *ci = CharsetInfo::createConstantRecord(m_charset_aliases[i]);
			LEAVE_IF_NULL(ci);
			OP_STATUS rc = m_charset_info.Add(ci);
			if (OpStatus::IsError(rc))
			{
				OP_DELETE(ci);
				LEAVE(rc);
			}
		}
	}
# endif // !ENCODINGS_REGTEST
#endif // ENCODINGS_HAVE_TABLE_DRIVEN
}

#ifndef ENCODINGS_REGTEST
/* static */ CharsetManager::CharsetInfo *
CharsetManager::CharsetInfo::createConstantRecord (const char *canonical_charset)
{
	return OP_NEW(CharsetInfo, (const_cast<char *>(canonical_charset), canonical_charset, REF_COUNT_CONSTANT));
}

/* static */ CharsetManager::CharsetInfo *
CharsetManager::CharsetInfo::createRegularRecord (const char *charset)
{
	char *charset_copy = NULL;
	if (charset) {
		// Ownership of charset_copy will be transferred to CharsetInfo
		// object, which will op_free() it in its destructor.
		charset_copy = op_strdup(charset);
		if (!charset_copy) return NULL;
	}

	const char *canonical_charset = NULL;
#ifdef ENC_CSETMAN_STORE_CANONICAL
	canonical_charset = g_charsetManager->GetCanonicalCharsetName(charset);
#endif

	return OP_NEW(CharsetInfo, (charset_copy, canonical_charset, 0));
}

OP_STATUS CharsetManager::GetCharsetID(const char *charset, unsigned short *charsetID)
{
	if (!charset)
	{
		*charsetID = 0;
		return OpStatus::ERR_NULL_POINTER;
	}

	// Go through the existing identifiers to see if we can find it.
	unsigned short known_charsets = static_cast<unsigned short>(m_charset_info.GetCount());
	unsigned short unreferenced_entry = ENCODINGS_CHARSET_CACHE_SIZE;
	for (unsigned short i = 1; i < known_charsets; i++)
	{
		CharsetInfo *ci = m_charset_info.Get(i);
		OP_ASSERT(ci);
		const char *cs = ci->getCharset();
		if (charset == cs ||
		    (charset && cs && op_strcmp(charset, cs) == 0))
		{
			*charsetID = i;
			return OpStatus::OK;
		}

		if (0 == ci->getRefCount() &&
		    unreferenced_entry == ENCODINGS_CHARSET_CACHE_SIZE)
		{
			// Remember the first unreferenced entry
			unreferenced_entry = i;
		}
	}

	// Not there, we need to allocate it.
	unsigned short insert_at = ENCODINGS_CHARSET_CACHE_SIZE;
	if (known_charsets < ENCODINGS_CHARSET_CACHE_SIZE)
	{
		// There is space in the list, so we add it to the end.
		insert_at = known_charsets;
	}
	else if (unreferenced_entry < ENCODINGS_CHARSET_CACHE_SIZE)
	{
		// There is no space in the list, but there is at least one
		// unreferenced entry that we can replace.
		insert_at = unreferenced_entry;
	}

	if (insert_at < ENCODINGS_CHARSET_CACHE_SIZE && *charset)
	{
		// There is space in the list.
		OP_ASSERT(charset && *charset); // Never insert empty entry

		// Create an entry and insert it into the list.
		CharsetInfo *ci = CharsetInfo::createRegularRecord(charset);
		if (!ci) return OpStatus::ERR_NO_MEMORY;

		if (insert_at == known_charsets)
		{
			// Add it to the end.
			OP_STATUS rc = m_charset_info.Add(ci);
			if (OpStatus::IsError(rc))
			{
				OP_DELETE(ci);
				return rc;
			}

			// The entry we added is now the last one.
			*charsetID = static_cast<unsigned short>(m_charset_info.GetCount() - 1);
		}
		else
		{
			// Replace an existing entry.
			CharsetInfo *old = m_charset_info.Get(insert_at);
			m_charset_info.Replace(insert_at, ci);
			OP_DELETE(old);

			*charsetID = insert_at;
		}
		return OpStatus::OK; // Successful insert
	}

	// The list is full and no unreferenced entry, try to find the canonical
	// identifier instead.
	const char *canonical_charset = GetCanonicalCharsetName(charset);
	OP_ASSERT(!canonical_charset || op_strcmp(charset, canonical_charset) != 0);
	OP_STATUS s = GetCharsetID(canonical_charset, charsetID);
	return s == OpStatus::ERR_NULL_POINTER ? OpStatus::ERR_OUT_OF_RANGE : s;
}

unsigned short CharsetManager::GetCharsetIDL(const char *charset)
{
	unsigned short charsetID = 0;
	OP_STATUS s = GetCharsetID(charset, &charsetID);

	if (s == OpStatus::ERR_NO_MEMORY) {
		LEAVE(s);
	}
	OP_ASSERT(OpStatus::IsSuccess(s) || charsetID == 0);
	return charsetID;
}

const char *CharsetManager::GetCharsetFromID(unsigned short id)
{
	CharsetInfo *ci = m_charset_info.Get(id);
	if (ci) return ci->getCharset();
	return NULL;
}

const char *CharsetManager::GetCanonicalCharsetFromID(unsigned short id)
{
	CharsetInfo *ci = m_charset_info.Get(id);
	if (ci) return ci->getCanonicalCharset();
	return NULL;
}

void CharsetManager::IncrementCharsetIDReference(unsigned short id)
{
	CharsetInfo *ci = m_charset_info.Get(id);
	if (ci) ci->incRefCount();
}

void CharsetManager::DecrementCharsetIDReference(unsigned short id)
{
	CharsetInfo *ci = m_charset_info.Get(id);
	if (ci) ci->decRefCount();
}
#endif // !ENCODINGS_REGTEST

// Enable the following to follow HTML5 as strictly as possible.
// This will make the us less successful at accepting weirdly spelled charsets.
// #define STRICT_HTML5

/**
 * Transforms a character set name into a name format suitable for use
 * with the GetCanonicalCharsetName method. Removes dashes, x- prefixes,
 * and other unnecessary characters and identifiers.
 *
 * We try to be lenient in the exact spelling of the given encoding
 * identifier. More specifically, we follow the rules laid out in
 * section 9.2.2.2 of HTML5:
 *
 * <quote src="http://www.w3.org/html/wg/html5/#character0" when="2009-12-21">
 * When comparing a string specifying a character encoding with the name or
 * alias of a character encoding to determine if they are equal, user agents
 * must remove any leading or trailing space characters in both names, and
 * then perform the comparison in an ASCII case-insensitive manner.
 * </quote>
 *
 * @param input Character set identifier
 * @param input_len Length of above character set identifier
 * @param output Output buffer
 * @param max_output_len Maximum length of output buffer (not counting final nul)
 * @return Number of characters written to name (not counting final nul)
 */
size_t CharsetManager::Canonize(const char *input, size_t input_len, char *output, size_t max_output_len)
{
	/** Lookup table for incoming characters
	 *
	 * Incoming characters >= 128 should be preserved verbatim
	 * For chars < 128, look up in this table:
	 *  - 0: discard character
	 *  - 1: preserve character verbatim
	 *  - 2: add 0x20 to character (i.e. lowercase it)
	 *  - 3: remove if nothing has been output yet, otherwise preserve
	 *  - 4: remove if nothing has been output yet, otherwise lowercase
	 */
	const UINT8 CanonMap[128] = {
		// ignore spaces and punctuation (but not control chars)
		// ensure letters are in lowercase
		// ignore leading 'x-'
		1, 1, 1, 1,  1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 1, 1, // 00 - 0f
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, // 10 - 1f
		3, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, /*3*/1, 1, 1, // 20 - 2f
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, // 30 - 3f (digits)
		1, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2, // 40 - 4f (uppercase chars)
		2, 2, 2, 2,  2, 2, 2, 2,  /*4*/2, 2, 2, 1,  1, 1, 1, 1, // 50 - 5f (uppercase chars)
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, // 60 - 6f (lowercase chars)
		1, 1, 1, 1,  1, 1, 1, 1,  /*3*/1, 1, 1, 1,  1, 1, 1, 1  // 70 - 7f (lowercase chars)
	};

	size_t i = 0; // index into input string
	size_t j = 0; // index into output string
	while (i < input_len && j < max_output_len) {
		UINT8 current = static_cast<UINT8>(input[i++]);
		switch (current < 128 ? CanonMap[current ] : 1) {
#ifndef STRICT_HTML5
		case 4: // ignore char if j == 0, otherwise lowercase char
			if (j) output[j++] = current + 0x20;
			break;
		case 3: // ignore char if j == 0, otherwise preserve char
			if (j) output[j++] = current;
			break;
#endif // STRICT_HTML5
		case 2: // lowercase char
			output[j++] = current + 0x20;
			break;
		case 1: // preserve char
			output[j++] = current;
			break;
		default: // ignore this char
			OP_ASSERT(current < 128 && CanonMap[current] == 0);
			break;
		}
	}
	output[j] = 0;

	return j;
}

int CharsetManager::CharsetCompare(const void *key, const void *entry)
{
	return op_strcmp(reinterpret_cast<const char *>(key),
	                 reinterpret_cast<char * const *>(entry)[0]);
}

const char *CharsetManager::GetCanonicalCharsetName(const char *charset, long charset_len /* = -1 */)
{
	if (!charset || !charset_len) return NULL; // Sanity check

	if (charset_len < 0) charset_len = op_strlen(charset);
	char canonized[65]; /* ARRAY OK 2009-03-02 johanh */
	size_t len = Canonize(charset, charset_len, canonized, sizeof canonized - 1);
	OP_ASSERT(len < sizeof canonized);

    const char **found = NULL;
	while ((found = reinterpret_cast<const char **>(op_bsearch(canonized, m_charset_aliases, CHARSET_ALIASES_COUNT, sizeof (char *) * 2, CharsetCompare))) == NULL)
	{
#ifndef STRICT_HTML5
		// Some mailer software seem to be using the Java charset constants
		// verbatim in mail, i.e "GB2312_CHARSET" instead of "GB2312". If the
		// above loop fails to find an alias, and the tag ends in "CHARSET",
		// delete it and redo the check.
		if (len > 8 && 0 == op_strncmp(&canonized[len - 8], "_charset", 8))
		{
			canonized[len - 8] = 0;
		}
		// While we're at it, try to remove any Unicode-version prefixes
		else if (len > 12 && 0 == op_strncmp(canonized, "unicode-", 8) &&
			     op_isdigit(static_cast<unsigned char>(canonized[8])) &&
				 canonized[9] == '-' &&
			     op_isdigit(static_cast<unsigned char>(canonized[10])) &&
				 canonized[11] == '-')
		{
			op_memmove(canonized, canonized + 12, len - 11);
		}
		else
#endif // STRICT_HTML5
			break;
	}
	if (found)
		return found[1];
	else
		// We don't know what this is
		return NULL;
}

#ifdef ENCODINGS_HAVE_MIB_SUPPORT
int CharsetManager::MIBCompare(const void *key, const void *entry)
{
	return *reinterpret_cast<const int *>(key) -
	       reinterpret_cast<const struct mibentry *>(entry)->mib;
}

const char *CharsetManager::GetCharsetNameFromMIBenum(int mibenum)
{
	struct mibentry *found =
		reinterpret_cast<struct mibentry *>
		(op_bsearch(&mibenum, m_mib, MIB_ENTRIES_COUNT, sizeof (struct mibentry),
		 MIBCompare));

	if (found)
		return found->charset;
	else
		return NULL;
}
#endif
