/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/src/manifest_parser_impl.h"

#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/auth/auth_digest.h"

/**
 * @section constant definitions
 * @{
 */

#define TAB static_cast<uni_char> ('\t')
#define SPACE static_cast<uni_char> (' ')
#define COLUMN static_cast<uni_char> (':')
#define SIGN static_cast<uni_char> ('#')
#define LF static_cast<uni_char> ('\x0A')
#define CR static_cast<uni_char> ('\x0D')
#define WILDCARD static_cast<uni_char> ('*')

#define MANIFEST_MAGIC_SIGN static_cast<const uni_char*const> (UNI_L ("CACHE MANIFEST"))
#define SECTION_CACHE static_cast<const uni_char*const> (UNI_L ("CACHE"))
#define SECTION_CACHE_LENGTH static_cast<const unsigned> (5) // CACHE
#define SECTION_NETWORK static_cast<const uni_char*const> (UNI_L ("NETWORK"))
#define SECTION_NETWORK_LENGTH static_cast<const unsigned> (7) // NETWORK
#define SECTION_FALLBACK static_cast<const uni_char*const> (UNI_L ("FALLBACK"))
#define SECTION_FALLBACK_LENGTH static_cast<const unsigned> (8) // FALLBACK
// manifest magic sign length: used for chopping magic sign
#define MANIFEST_MAGIC_SIGN_LENGTH static_cast<const unsigned>(14) // `CACHE MANIFEST' -> 14 characters
#define WRONG_POS static_cast<const int> (-1)

class StringUtility
{
public:

	/**
	 * Find first position of the character
	 *
	 * The implementation is independent to the `\0' symbol of the string,
	 * hence the solution may be used for buffer data.
	 *
	 * @param uni_str string to analyze
	 * @param str_size size of the string
	 * @param chr   character to analyze
	 *
	 * @return position of the character of KNotFound if the character was not find in the string
	 */
	static inline
	unsigned FindFirstOf(const uni_char* const uni_str, unsigned str_size, uni_char chr)
	{
		OP_ASSERT(uni_str != NULL);
		if (!uni_str)
			return static_cast<unsigned> (KNotFound);

		unsigned ix = 0;
		while (ix < str_size && uni_str[ix] != chr)
		{
			ix++; // it may be optimized in one line of while, but it is not so readable though
		}

		return ix == str_size ? static_cast<unsigned> (KNotFound) : ix;
	}

	/**
	 * Find Line Position
	 *
	 * The function checks new line delimiters (either CR (Carriage Return) or LF (Line Feed))
	 * and returns a new line position. The function tries to find the nearest new line delimiter.
	 *
	 * TODO: add examples in the documentation
	 *
	 * @param start_line_pos position in the string from which a new line has to be find
	 *
	 * @return position of when a  new line has been find
	 * @note If neither CR nor LF may not be found, then KNotFound is returned.
	 */
	static inline
	unsigned FindNewLinePosition(const uni_char* const uni_str, unsigned str_size, unsigned start_line_pos)
	{
		OP_ASSERT (static_cast<unsigned>(KNotFound) != start_line_pos);
		OP_ASSERT(uni_str != NULL);

		if (!uni_str)
			return static_cast<unsigned> (KNotFound);

		// find the nearest new line delimiter (!)
		const unsigned lf_line_pos = FindFirstOf(uni_str + start_line_pos, str_size - start_line_pos, LF);
		const unsigned cr_line_pos = FindFirstOf(uni_str + start_line_pos, str_size - start_line_pos, CR);

		// note: KNotFound == -1, hence in order to select the minimum value but not KNotFound,
		// static cast to unsigned is used. When the int is casted to unsigned, then
		// value becomes positive and in case of KNotFound the value is the biggest integer value
		const unsigned line_pos_offset = MIN (lf_line_pos, cr_line_pos);

		return static_cast<unsigned> (KNotFound) == line_pos_offset ? line_pos_offset : start_line_pos
			+ line_pos_offset;
	}

	/**
	 * Check is a character valid white space
	 *
	 * According specification, only space and tab are considered as valid white space characters
	 * Note: a std function for white space validation has not been used because it may not comply with HTML 5 spec.,
	 * so exact validation has been created instead.
	 *
	 * @param chr character to check
	 * @return TRUE if a character is a white space, FALSE - in other case
	 */
	static inline BOOL CheckWhiteLine(uni_char chr)
	{
		return SPACE == chr || TAB == chr;
	}

	/**
	 * Truncate Leading White Spaces
	 *
	 * The function finds where in the string (limited by start_pos and end_pos) there are starting and ending positions
	 * of tokens but without leading and trailing white spaces.
	 *
	 * @param str - string to check
	 * @param start_pos reference to the starting point of the string
	 *  Note: after function invocation, a new position may be assigned - this is the starting position of the tokens
	 *      without white space
	 * @param end_pos - ending point of the string
	 */
	static inline
	void TruncateLeadingWhiteSpaces(const uni_char* const uni_str, unsigned& start_pos, unsigned end_pos)
	{
		OP_ASSERT(start_pos <= end_pos); // pre check

		OP_ASSERT(uni_str != NULL);

		// Find a first character that is not either space or tab
		while (start_pos < end_pos && CheckWhiteLine(uni_str[start_pos]))
		{
			start_pos++;
		}

		OP_ASSERT(start_pos <= end_pos); // post check
	}

	/**
	 * Truncate Trailing White Spaces
	 *
	 * The function truncates trailing white spaces
	 *
	 * @param uni_str   input string
	 * @param start_pos  position from which the string has to be processed
	 * @param end_pos position to which the string has to be processed
	 *
	 */
	static inline
	void TruncateTrailingWhiteSpaces(const uni_char* const uni_str, unsigned start_pos, unsigned& end_pos)
	{
		OP_ASSERT (start_pos <= end_pos); // pre check

		while (start_pos < end_pos && CheckWhiteLine(uni_str[end_pos - 1]))
		{
			end_pos--;
		}

		OP_ASSERT (start_pos <= end_pos); // post check
	}

	/**
	 * Read Token
	 *
	 * Read token from a string limited by start and end positions. After function invocation a new indexes will be
	 * assigned to the start_pos and end_pos variables those defines starting and ending position of a token.
	 *
	 * @param str string where a token has to be find
	 * @param start_pos - starting index of the string that is analyzed
	 * @param end_pos - ending index of the string that is analyzed
	 */
	static inline
	void ReadToken(const uni_char* const uni_str, unsigned& start_pos, unsigned& end_pos)
	{
		OP_ASSERT (start_pos <= end_pos);

		// Find first non tab of space character
		TruncateLeadingWhiteSpaces(uni_str, start_pos, end_pos);

		const unsigned old_end_pos = end_pos;
		end_pos = start_pos;
		while (old_end_pos != end_pos && !CheckWhiteLine(uni_str[end_pos]))
		{
			end_pos++;
		}

		OP_ASSERT (start_pos <= end_pos);
	}
}; // ~ class StringUtility


class InnerParser
{
private:

	const uni_char* const m_str;
	const unsigned m_str_length;

	// indexes of the line [start; end)
	unsigned m_start_line_pos;
	unsigned m_end_line_pos;
	unsigned m_next_start_line_pos;

	unsigned m_start_token_pos;
	unsigned m_end_token_pos;

	unsigned m_next_start_token_pos;

	const BOOL m_is_all_lines;

public:

	explicit InnerParser(const uni_char* const str, unsigned str_size, BOOL is_all_lines)
	: m_str(str)
	, m_str_length(str_size)
	, m_start_line_pos(0)
	, m_end_line_pos(str_size)
	, m_next_start_line_pos(0)
	, m_start_token_pos(static_cast<unsigned>(KNotFound))
	, m_end_token_pos(static_cast<unsigned>(KNotFound))
	, m_next_start_token_pos(static_cast<unsigned>(KNotFound))
	, m_is_all_lines(is_all_lines)
	{
		OP_ASSERT (m_start_line_pos <= m_end_line_pos);
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_start_line_pos);
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_end_line_pos);
	}

	BOOL ReadNextLine()
	{
		OP_ASSERT (m_start_line_pos <= m_end_line_pos);
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_end_line_pos );
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_next_start_line_pos);

		if (m_next_start_line_pos == m_str_length)
			return FALSE;

		// TODO: think about this code!
		const unsigned real_new_line_pos = StringUtility::FindNewLinePosition(m_str, m_str_length,
																				m_next_start_line_pos);

		unsigned next_end_line_pos = m_next_start_line_pos;

		// TODO: think about this code!
		if (static_cast<unsigned> (KNotFound) == real_new_line_pos)
		{
			// If the parser works in the mode 'all lines', then even if there is no next new line
			// in the string, then it is assumed that the actual end of the string may be considered
			// as a new line. However, if this assumption has already been done before, then
			// nothing is done and end_line is equal to the next_start_line_pos, and this means
			// that 0 characters are find in the string
			if (m_is_all_lines && next_end_line_pos != m_str_length)
			{
				next_end_line_pos = m_str_length;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			next_end_line_pos = real_new_line_pos;
		}

		m_start_line_pos = m_next_start_line_pos;
		m_end_line_pos = next_end_line_pos;

		m_start_token_pos = m_start_line_pos;
		m_end_token_pos = m_end_line_pos;

		m_next_start_line_pos = m_end_line_pos != m_str_length ? m_end_line_pos + 1 : m_str_length;
		m_next_start_token_pos = m_start_line_pos;

		return TRUE;
	}

	ManifestParserImpl::LineType AnalyzeLine()
	{
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_start_token_pos);
		OP_ASSERT (static_cast<unsigned>(KNotFound) != m_end_token_pos);

		const ManifestParserImpl::LineType line_type = ManifestParserImpl::AnalyzeLine(m_str, m_start_token_pos,
																						m_end_token_pos);

		OP_ASSERT (m_start_token_pos <= m_end_token_pos);

		return line_type;
	}

	OP_STATUS ReadNextToken(OpString& token)
	{
		OP_ASSERT (m_start_line_pos <= m_end_line_pos );

		unsigned current_end_token_pos = m_end_token_pos;
		StringUtility::ReadToken(m_str, m_next_start_token_pos, current_end_token_pos);
		OP_ASSERT (m_start_token_pos <= current_end_token_pos);
		OP_ASSERT (m_next_start_token_pos <= current_end_token_pos);

		m_start_token_pos = m_next_start_token_pos;
		m_next_start_token_pos = current_end_token_pos;

		RETURN_IF_ERROR (token.Set (m_str + m_start_token_pos, current_end_token_pos - m_start_token_pos));

		return OpStatus::OK;
	}

	int GetParsedLines() const
	{
		return m_next_start_line_pos;
	}
}; // ~ class InnerParser


/**
 * @} end of section of inner classes
 */

/**
 * @section constructor/destructor
 * @{
 */
// TODO: comment what, how, why and compare that this correlates with member variable docs
ManifestParserImpl::ManifestParserImpl(const URL& manifest_url)
	: ManifestParser()
	, m_is_sign_checked(FALSE)
	, m_last_state_success(TRUE)
	, m_last_section_state(Manifest::CACHE)
	, m_manifest(NULL), m_manifest_url(manifest_url)
{
}

OP_STATUS ManifestParserImpl::Construct()
{
	m_hash.reset(CryptoHash::CreateMD5());
	if (!m_hash.get() || OpStatus::IsError(m_hash->InitHash()))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

ManifestParserImpl::~ManifestParserImpl()
{
}

/**
 * @} constructor/destructor
 */

ManifestParserImpl::LineType ManifestParserImpl::AnalyzeLine(const uni_char* const uni_str, unsigned& start_pos, unsigned& end_pos)
{
	OP_ASSERT (static_cast<unsigned>(KNotFound) != start_pos);
	OP_ASSERT (static_cast<unsigned>(KNotFound) != end_pos);
	OP_ASSERT (start_pos <= end_pos);

	StringUtility::TruncateLeadingWhiteSpaces(uni_str, start_pos, end_pos);
	StringUtility::TruncateTrailingWhiteSpaces(uni_str, start_pos, end_pos);

	OP_ASSERT (start_pos <= end_pos);

	if (start_pos < end_pos && SIGN == uni_str[start_pos])
	{
		return COMMENT;
	}

	if (start_pos < end_pos && COLUMN == uni_str[end_pos - 1]) // optimization: check that the string has trailing `:' character
	{
		const uni_char* const section_name = uni_str + start_pos;
		const unsigned section_length = end_pos - start_pos - 1;
		// note, first it is compared the length of the string, then exact string matching
		//   the comparing length helps to solve the case when an empty string will match CACHE sections

		if (SECTION_CACHE_LENGTH == section_length && 0 == uni_strncmp(SECTION_CACHE, section_name, section_length))
		{
			return CACHE_SECTION_DEF;
		}
		else if (SECTION_NETWORK_LENGTH == section_length && 0 == uni_strncmp(SECTION_NETWORK, section_name, section_length))
		{
			return NETWORK_SECTION_DEF;
		}
		else if (SECTION_FALLBACK_LENGTH == section_length && 0 == uni_strncmp(SECTION_FALLBACK, section_name, section_length))
		{
			return FALLBACK_SECTION_DEF;
		}
		else
		{
			return UNKNOWN;
		}
	}

	return ManifestParserImpl::URL_DEF;
}

BOOL ManifestParserImpl::CheckMagicSign(const uni_char* const uni_str, unsigned str_size)
{
	// if the manifest is completely empty - it is an error
	if (MANIFEST_MAGIC_SIGN_LENGTH >= str_size)
	{ // magic sign and one delimiter must be present
		return FALSE;
	}

	const OpStringC magic_sign(uni_str);

	// the Magic Sign is compared case-sensitively
	if (0 != magic_sign.Compare(MANIFEST_MAGIC_SIGN, MANIFEST_MAGIC_SIGN_LENGTH))
	{
		return FALSE;
	}

	// check the trailing character of the magic sign
	const uni_char manifest_sign_delim = uni_str[MANIFEST_MAGIC_SIGN_LENGTH];
	const BOOL manifest_sign_delim_match = LF == manifest_sign_delim || CR == manifest_sign_delim || SPACE
		== manifest_sign_delim || TAB == manifest_sign_delim;
	if (!manifest_sign_delim_match)
	{
		return FALSE;
	}

	// if all checks has been successfully passed - then manifest magic sign is valid
	return TRUE;
}

/**
 * @} static function of ManifestParserImpl
 */

OP_STATUS ManifestParserImpl::Parse(const uni_char* const uni_str, unsigned str_size, BOOL is_last, unsigned& parsed_chars)
{
	OP_ASSERT(uni_str);
	if (!uni_str)
		return OpStatus::ERR;

	// be default, it is assumed that 0 characters have been parsed
	parsed_chars = 0;

	if (!m_last_state_success)
		return OpStatus::ERR;

	BOOL escape_first_line = FALSE; // should be escaped a first line (magic sign) or not

	// first of all, if the magic sign has not been checked yet, then check it
	if (!m_is_sign_checked)
	{

		//      when input data is less then the magic sign length - ignore the string and
		//      wait for a next chunk of data
		if (str_size < MANIFEST_MAGIC_SIGN_LENGTH + 1)
		{

			// if it is a last data, then nothing to do, the magic sign is definitely wrong
			if (is_last)
			{
				m_last_state_success = FALSE;
				return OpStatus::ERR;
			}
			else
			{ // if it is not a last data, then there is an probability that the rest of data are loaded
				// hence, just escape this call
				return OpStatus::OK;
			}
		}
		else if (!CheckMagicSign(uni_str, str_size))
		{
			parsed_chars = MANIFEST_MAGIC_SIGN_LENGTH + 1;

			m_last_state_success = FALSE;
			m_is_sign_checked = TRUE;

			return OpStatus::ERR;
		}
		else
		{
			m_is_sign_checked = TRUE;

			escape_first_line = TRUE;

			m_manifest = OP_NEW (Manifest, (m_manifest_url));
			if (!m_manifest.get())
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	InnerParser parser(uni_str, str_size, is_last);
	if (escape_first_line)
	{
		parser.ReadNextLine();
	}

	while (parser.ReadNextLine())
	{
		OP_ASSERT (NULL != m_manifest.get ());

		const LineType current_line_type = parser.AnalyzeLine();

		switch (current_line_type)
		{
		case CACHE_SECTION_DEF:
			m_last_section_state = Manifest::CACHE;
			break;

		case NETWORK_SECTION_DEF:
			m_last_section_state = Manifest::NETWORK;
			break;

		case FALLBACK_SECTION_DEF:
			m_last_section_state = Manifest::FALLBACK;
			break;

		case COMMENT:
			// ignore comment
			break;

		case UNKNOWN:
			m_last_section_state = Manifest::UNKNOWN;
			break;

		case URL_DEF:
		{
			// check that the parser is in a proper state
			OP_ASSERT (
				Manifest::CACHE == m_last_section_state
				|| Manifest::NETWORK == m_last_section_state
				|| Manifest::FALLBACK == m_last_section_state
				|| Manifest::UNKNOWN == m_last_section_state
			);

			// read a first token, ignore other (for CACHE and NETWROK sections)
			OpString url1;
			RETURN_IF_ERROR (parser.ReadNextToken (url1));

			// if URL is not empty, then continue
			if (url1.HasContent())
			{
				switch (m_last_section_state)
				{
				case Manifest::CACHE:
					RETURN_IF_ERROR (m_manifest->AddCacheUrl (url1));
					break;

				case Manifest::NETWORK:
					// check is the first character a wildcard, escape the rest of characters
					if (WILDCARD == url1[0])
					{
						m_manifest->SetOnlineOpen(TRUE);
					}
					else
					{
						RETURN_IF_ERROR (m_manifest->AddNetworkUrl (url1));
					}
					break;

				case Manifest::FALLBACK:
				{
					OpString url2;
					RETURN_IF_ERROR (parser.ReadNextToken (url2));
					if (url2.HasContent())
					{
						RETURN_IF_ERROR (m_manifest->AddFallbackUrls (url1, url2));
					}
				}
					break;

				case Manifest::UNKNOWN:
					// Do nothing
					break;

				default:
					OP_ASSERT (!"The parser is in an unknown state!");
				} // switch of sections
			} // check for the first token (it must not be empty)
		}
			break;

		default:
			OP_ASSERT (!"The parser may not properly define a line type!");// unknown case!

			m_last_state_success = FALSE;

			return OpStatus::ERR;
		} // switch of line type cases (comment, cache, network, fallback, or URL)
	} // loop of parsed lines

	parsed_chars = parser.GetParsedLines();
	m_hash->CalculateHash(reinterpret_cast<const UINT8*> (uni_str), parsed_chars * sizeof(uni_char) / sizeof(UINT8));

	if (is_last)
	{
		OP_ASSERT(NULL != m_manifest.get ());
	}

	return OpStatus::OK;
}

OP_STATUS ManifestParserImpl::BuildManifest(Manifest*& new_manifest) const
{
	if (NULL == m_manifest.get())
	{
		new_manifest = NULL;
		return OpStatus::ERR;
	}
	else
	{
		Manifest* tmp_new_manifest;

		/* ToDo: Stop using clone. It's not needed to clone the manifest, as we can just give away the pointer
		 * instead. The cloing is a lot of extra uneeded code.
		 */
		RETURN_IF_ERROR (m_manifest->Clone (tmp_new_manifest));
		OpAutoPtr<Manifest> tmp_manifest(tmp_new_manifest);

		const int hash_size = m_hash->BlockSize() + 1; // one extra character for '\0'
		OpString8 hash_value8;

		if (hash_value8.Reserve(hash_size))
		{
			ConvertToHex(const_cast<OpAutoPtr<CryptoHash> &> (m_hash), hash_value8.CStr());
			OpString hash_value16;

			RETURN_IF_ERROR (hash_value16.SetFromUTF8 (hash_value8.CStr ()));
			RETURN_IF_ERROR (tmp_new_manifest->SetHash (hash_value16));
		}
		else
		{
			new_manifest = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		new_manifest = tmp_manifest.release();
	}

	return OpStatus::OK;
}

#endif // APPLICATION_CACHE_SUPPORT
