/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/encoders/encoder-utility.h"

#ifndef ENCODINGS_REGTEST
# include "modules/encodings/utility/charsetnames.h"
# include "modules/encodings/encoders/outputconverter.h"
# include "modules/encodings/encoders/utf7-encoder.h"
# include "modules/encodings/encoders/utf8-encoder.h"
# include "modules/encodings/encoders/utf16-encoder.h"
# include "modules/encodings/encoders/iso-8859-1-encoder.h"
# ifdef ENCODINGS_HAVE_TABLE_DRIVEN
#  include "modules/encodings/encoders/big5hkscs-encoder.h"
#  include "modules/encodings/encoders/dbcs-encoder.h"
#  include "modules/encodings/encoders/euc-tw-encoder.h"
#  include "modules/encodings/encoders/gb18030-encoder.h"
#  include "modules/encodings/encoders/hz-encoder.h"
#  include "modules/encodings/encoders/iso-2022-cn-encoder.h"
#  include "modules/encodings/encoders/jis-encoder.h"
#  include "modules/encodings/encoders/sbcs-encoder.h"
# endif
# include "modules/util/handy.h" // ARRAY_SIZE
#endif

#ifndef ENCODINGS_REGTEST
/* static */
OP_STATUS OutputConverter::CreateCharConverter(const uni_char *in_charset,
                                               OutputConverter **obj,
                                               BOOL iscanon,
                                               BOOL allownuls)
{
	char tmp[64]; /* ARRAY OK 2009-01-27 johanh - boundary checked */

	uni_cstrlcpy(tmp, in_charset, ARRAY_SIZE(tmp));
	return CreateCharConverter(tmp, obj, iscanon, allownuls);
}

/* static */
OP_STATUS OutputConverter::CreateCharConverterFromID(unsigned short id,
	                                             OutputConverter **obj,
	                                             BOOL allownuls)
{
	const char *canonical = g_charsetManager->GetCanonicalCharsetFromID(id);
	if (!canonical)
		return OpStatus::ERR_OUT_OF_RANGE;

	return CreateCharConverter(canonical, obj, TRUE, allownuls);
}
#endif // !ENCODINGS_REGTEST

#if !defined ENCODINGS_REGTEST && defined ENCODINGS_HAVE_TABLE_DRIVEN
/* static */
OP_STATUS OutputConverter::CreateCharConverter(const char *in_charset,
                                               OutputConverter **obj,
                                               BOOL iscanon,
                                               BOOL allownuls)
{
	if (!obj)
		return OpStatus::ERR_NULL_POINTER;

	// Bug#193696: Make sure the output pointer is NULL if this call fails.
	*obj = NULL;

	const char *realname = iscanon ? in_charset : g_charsetManager->GetCanonicalCharsetName(in_charset);
	if (realname == NULL)
		return OpStatus::ERR_OUT_OF_RANGE;

	// NOTE: UTF-16 is not exported unless explicitly requested,
	// because of the possible problems with embedded nul octets.

	if (strni_eq(realname, "ISO-8859-1", 11))
	{
		*obj = OP_NEW(UTF16toISOLatin1Converter, (FALSE));
	}
	else if (strni_eq(realname, "US-ASCII", 9))
	{
		*obj = OP_NEW(UTF16toISOLatin1Converter, (TRUE));
	}
	else if (allownuls && strni_eq(realname, "UTF-16", 6) /* also matches 16le and 16be */)
	{
		// If we allow outputting UTF-16 (which most likely will cause nul
		// octets to appear in the stream), make sure it is always converted
		// to local byte order, and that a BOM is inserted.
		*obj = OP_NEW(UTF16toUTF16OutConverter, ());
	}
	else if (strni_eq(realname, "UTF-8", 6) ||
	         strni_eq(realname, "UTF-16", 6) /* also matches 16le and 16be */)
	{
		*obj = OP_NEW(UTF16toUTF8Converter, ());
	}
# ifdef ENCODINGS_HAVE_JAPANESE
	else if (strni_eq(realname, "ISO-2022-JP", 12))
	{
		*obj = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::ISO_2022_JP));
	}
	else if (strni_eq(realname, "ISO-2022-JP-1", 14))
	{
		*obj = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::ISO_2022_JP_1));
	}
	else if (strni_eq(realname, "EUC-JP", 7))
	{
		*obj = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::EUC_JP));
	}
	else if (strni_eq(realname, "SHIFT_JIS", 10))
	{
		*obj = OP_NEW(UTF16toJISConverter, (UTF16toJISConverter::SHIFT_JIS));
	}
# endif // ENCODINGS_HAVE_JAPANESE
# ifdef ENCODINGS_HAVE_CHINESE
	else if (strni_eq(realname, "BIG5", 5))
	{
		*obj = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::BIG5));
	}
	else if (strni_eq(realname, "BIG5-HKSCS", 11))
	{
		*obj = OP_NEW(UTF16toBig5HKSCSConverter, ());
	}
	else if (strni_eq(realname, "GBK", 6)/* ||
	         strni_eq(realname, "GB2312", 7)*/ /* (remapped using charset_aliases) */)
	{
		*obj = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::GBK));
	}
	else if (strni_eq(realname, "GB18030", 7))
	{
		*obj = OP_NEW(UTF16toGB18030Converter, ());
	}
	else if (strni_eq(realname, "EUC-TW", 7))
	{
		*obj = OP_NEW(UTF16toEUCTWConverter, ());
	}
	else if (strni_eq(realname, "HZ-GB-2312", 11))
	{
		*obj = OP_NEW(UTF16toDBCS7bitConverter, (UTF16toDBCS7bitConverter::HZGB2312));
	}
	else if (strni_eq(realname, "ISO-2022-CN", 12))
	{
		*obj = OP_NEW(UTF16toISO2022CNConverter, ());
	}
# endif // ENCODINGS_HAVE_CHINESE
# ifdef ENCODINGS_HAVE_KOREAN
	else if (strni_eq(realname, "EUC-KR", 7))
	{
		*obj = OP_NEW(UTF16toDBCSConverter, (UTF16toDBCSConverter::EUCKR));
	}
	else if (strni_eq(realname, "ISO-2022-KR", 12))
	{
		*obj = OP_NEW(UTF16toDBCS7bitConverter, (UTF16toDBCS7bitConverter::ISO2022KR));
	}
# endif // ENCODINGS_HAVE_KOREAN
# ifdef ENCODINGS_HAVE_UTF7
	else if (strni_eq(realname, "UTF-7", 6))
	{
		*obj = OP_NEW(UTF16toUTF7Converter, (UTF16toUTF7Converter::STANDARD));
	}
# endif // ENCODINGS_HAVE_UTF7
	else if ((strni_eq(realname, "ISO-8859-8-I", 13) ||
	          strni_eq(realname, "ISO-8859-8", 11)))
	{
		*obj = OP_NEW(UTF16toSBCSConverter, ("iso-8859-8-i", "iso-8859-8-rev"));
	}
	else if ((strni_eq(realname, "ISO-8859-6-I", 13) ||
	          strni_eq(realname, "ISO-8859-6", 11)))
	{
		*obj = OP_NEW(UTF16toSBCSConverter, ("iso-8859-6-i", "iso-8859-6-rev"));
	}
	else if (strni_eq(realname, "X-USER-DEFINED", 14))
	{
		*obj = OP_NEW(UTF16toISOLatin1Converter, (FALSE, TRUE));
	}
	else
	{
		// Check if we have a table for a single-byte character set
		char revtablename[32]; /* ARRAY OK 2009-01-27 johanh - boundary checked */
		size_t namelen = op_strlen(realname);
		if (namelen > ARRAY_SIZE(revtablename) - 5)
		{
			// This should never occur, since we don't have names that are
			// this long. But better safe than sorry.
			namelen = ARRAY_SIZE(revtablename) - 5;
		}
		op_memcpy(revtablename, realname, namelen);
		op_strcpy(revtablename + namelen, "-rev");

		if (g_table_manager && g_table_manager->Has(revtablename))
		{
			*obj = OP_NEW(UTF16toSBCSConverter, (realname, revtablename));
		}
		else
		{
			// We know this encoding, but cannot create an encoder for it.
			// This usually means that someone has stolen our data tables,
			// or that it has been disabled using the feature system.
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	if (!*obj) return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = (*obj)->Construct();
	if (!OpStatus::IsSuccess(ret)) {
		OP_DELETE(*obj);
		*obj = NULL;
	}
	return ret;
}
#endif // !ENCODINGS_REGTEST && !ENCODINGS_HAVE_TABLE_DRIVEN

const char *OutputConverter::GetCharacterSet()
{
	return "utf-16";
}

#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
OutputConverter::OutputConverter()
# ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	: m_entity_encoding(FALSE)
# endif
{
# ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	// Clear the list of invalid characters
	op_memset(m_invalid_characters, 0, sizeof m_invalid_characters);
# endif
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
	m_invalid_high_surrogate = 0;
# endif
}
#endif // ENCODINGS_HAVE_SPECIFY_INVALID || ENCODINGS_HAVE_ENTITY_ENCODING

#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
/**
 * Handle an unrepresentable input character. If entity encoding is enabled,
 * we convert it into an entity for the output buffer. If not, we store the
 * substitute string ("?" if unspecified) and record the fact.
 *
 * @param ch The character that cannot be represented
 * @param input_offset The offset into the current input buffer where ch occurs
 * @param dest Destination buffer pointer (advanced on success)
 * @param written The number of bytes written to buffer (increased on success)
 * @param maxlen Maximum number of bytes in buffer (including the count from
 *               the written parameter)
 * @param substitute Nul terminated string to use instead of '?' if entity
 *                   encoding is disabled
 * @return TRUE if the unrepresentable character was handled
 */
BOOL OutputConverter::CannotRepresent(const uni_char ch, int input_offset,
                                      char **dest, int *written, int maxlen,
                                      const char *substitute)
{
	const char *output; // String to output
	int output_len; // Length of string to output
	char entity[16]; /* ARRAY OK 2009-03-02 johanh */ // Buffer for entity string
	BOOL use_entity = FALSE;

	UINT32 ucs4char = ch;
	if (m_invalid_high_surrogate || Unicode::IsLowSurrogate(ch))
	{
		ucs4char = Unicode::DecodeSurrogate(m_invalid_high_surrogate, ch);
	}

	if (m_entity_encoding && !Unicode::IsSurrogate(ucs4char))
	{
		// Create an entity reference
		// uni_char is [0,4294967295], so max output length is 14 characters
		output_len = op_snprintf(entity, ARRAY_SIZE(entity), "&#%u;",
				static_cast<unsigned int>(ucs4char));
		output = entity;
		m_invalid_high_surrogate = 0;
		use_entity = TRUE;
	}
	else
	{
		// Store the substitute
		if (substitute)
		{
			output = substitute;
			output_len = op_strlen(substitute);
		}
		else
		{
			output = "?";
			output_len = 1;
		}
	}

	if (Unicode::IsHighSurrogate(ch))
	{
#ifndef ENCODINGS_HAVE_SPECIFY_INVALID
		m_invalid_high_surrogate = ch;
#endif
	}
	else
	{
		// Make sure there is space
		if (*written + output_len > maxlen)
			return FALSE;

		// Copy
		while (*output)
		{
			**dest = *(output ++);
			(*written) ++;
			(*dest) ++;
		}
	}

	if (!use_entity)
	{
		// Record the fact that we couldn't store this character
		CannotRepresent(ch, input_offset);
	}
	return TRUE;
}
#endif // ENCODINGS_HAVE_ENTITY_ENCODING

#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
/**
 * Indicate that the current input character cannot be represented in
 * the target encoding.
 *
 * @param ch The character that cannot be represented
 * @param input_offset The offset into the current input buffer where ch occurs
 */
void OutputConverter::CannotRepresent(const uni_char ch, int input_offset)
{
	if (Unicode::IsSurrogate(ch))
	{
		if (Unicode::IsHighSurrogate(ch))
		{
			m_invalid_high_surrogate = ch;
			return; // Don't count the cached character
		}
		else if (Unicode::IsLowSurrogate(ch))
		{
			if (m_invalid_high_surrogate)
			{
				// Add both high and low surrogate as invalid characters
				if (m_num_invalid < static_cast<int>(ARRAY_SIZE(m_invalid_characters) - 2))
				{
					m_invalid_characters[m_num_invalid++] = m_invalid_high_surrogate;
					m_invalid_characters[m_num_invalid] = ch;
				}
				input_offset -= 1; // invalid starts at high surrogate (=previous position)
			}
		}
	}
	else if (m_num_invalid < static_cast<int>(ARRAY_SIZE(m_invalid_characters) - 1))
	{
		m_invalid_characters[m_num_invalid] = ch;
	}

	m_invalid_high_surrogate = 0;
	++m_num_invalid;
	if (m_first_invalid_offset == -1)
		m_first_invalid_offset = m_num_converted + input_offset;
}
#elif defined ENCODINGS_HAVE_ENTITY_ENCODING
void OutputConverter::CannotRepresent(const uni_char ch, int input_offset)
{
	if (Unicode::IsHighSurrogate(ch))
	{
		m_invalid_high_surrogate = ch;
	}
	++m_num_invalid;
	if (m_first_invalid_offset == -1)
		m_first_invalid_offset = m_num_converted + input_offset;
}
#endif

void OutputConverter::Reset()
{
	this->CharConverter::Reset();
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	m_entity_encoding = FALSE;
#endif
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	// Clear the list of invalid characters
	op_memset(m_invalid_characters, 0, sizeof m_invalid_characters);
	m_invalid_high_surrogate = 0;
#endif
}
