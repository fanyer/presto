/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */

#ifndef MANIFEST_PARSER_IMPL_H
#define MANIFEST_PARSER_IMPL_H

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/url/url2.h"

#include "modules/applicationcache/src/manifest_parser.h"
#include "modules/libcrypto/include/CryptoHash.h"

/**
 *
 */
class ManifestParserImpl : public ManifestParser
{

public:

	ManifestParserImpl (const URL& manifest_url);
	OP_STATUS Construct();

	~ManifestParserImpl ();

	virtual OP_STATUS Parse (const uni_char*const uni_str, unsigned str_size, BOOL is_last, unsigned& parsed_chars);

	virtual OP_STATUS BuildManifest (Manifest*& new_manifest) const;

	/**
	 * Line Type
	 *
	 * Enum enlist all possible section types that might be found in the manifest file.
	 */
	enum LineType
	{
		URL_DEF, // one or a couple of URLs

		CACHE_SECTION_DEF,
		NETWORK_SECTION_DEF,
		FALLBACK_SECTION_DEF,

		UNKNOWN,

		COMMENT
	};

	/**
	 * @section inner classes
	 * @{
	 */

	friend class InnerParser;

	/**
	 * @} end of section of inner classes
	 */

private:

	/**
	 * The Sign Check Flag
	 *
	 * The flag shows validation of the magic sign.
	 *
	 * According to HTML 5 specification, the parser may fail (logically)
	 * only during parsing manifest sign (the Magic Sign `CACHE MANIFEST '
	 * at the first line), then the parser must try to parse any input value.
	 *
	 * By default, when the parser is just created, the value is FALSE.
	 */
	BOOL m_is_sign_checked;

	/**
	 *	Is true as long as the parser has not encountered any
	 *	Is errors. Will never go from FALSE to TRUE
	 *
	 * @note default value is TRUE
	 */
	BOOL m_last_state_success;

	/**
	 * Last Section Type
	 *
	 * The last section type from which data are read.
	 * It may happen that the section type is declared in one chunk of data,
	 * while its content may be defined in another. In order to properly place parsed
	 * data, this variable is used.
	 *
	 * @note The default state is @link {CACHE}
	 */
	Manifest::SectionType m_last_section_state;

	/**
	 * Manifest
	 *
	 * The object that stores all parsed data.
	 * Note, the manifest is not normalized. It must not be used directly, instead, `GetManifest'
	 * function should be used.
	 */
	OpAutoPtr<Manifest> m_manifest;

	const URL m_manifest_url;

	OpAutoPtr<CryptoHash> m_hash;

	/**
	 * @section static methods
	 * @{
	 */

public: // public methods for unit-tests

	/**
	 * analyze Line
	 *
	 *
	 * @param uni_str   string to analyze
	 * @param start_pos start position
	 * @param end_pos   end position
	 *
	 * @return  type of the line according to the section name
	 */
	static
	LineType AnalyzeLine (const uni_char*const uni_str, unsigned& start_pos, unsigned& end_pos);

	/**
	 * Check Magic Sign
	 *
	 * The function checks that the first line of the manifest has a valid magic sign.
	 * According to HTML 5 specification, the magic sign has to be
	 * CACHE MANIFEST([LF]|[CR]|[SPACE]|[TAB])
	 *
	 *
	 * NB: OpStringC8 has to cope with BOM (Byte Order Mark) itself,
	 *  so there is no validation of presence of BOM at all.
	 *
	 * @param uni_str string to check for a magic sign
	 * @param str_size size of the string
	 */
	static
	BOOL CheckMagicSign (const uni_char*const uni_str, unsigned str_size);

	/**
	 * @} end of section of static methods
	 */
};

#endif // APPLICATION_CACHE_SUPPORT
#endif // MANIFEST_PARSER_IMPL_H
