/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DISPLAY_WRITINGSYSTEM_H
#define DISPLAY_WRITINGSYSTEM_H

/**
 * Utility class for determining writing systems. This class contains
 * methods for looking up which writing systems to assume given a language,
 * a country, a character encoding standard, or a piece of Unicode text.
 * This can be used for selecting fonts in the font switching engine, or for
 * determining which character encoding autodetection mode to use if there is
 * language information available.
 *
 * Use-cases:
 *
 * - Getting a writing system from a language code: <br>
 *   WritingSystem::Script system = WritingSystem::FromLanguageCode("ja-JP");
 * - Getting a writing system from a character encoding: <br>
 *   WritingSystem::Script system = WritingSystem::FromEncoding("iso-2022-jp");
 * - Getting a writing system from a language code (domain): <br>
 *   WritingSystem::Script system = WritingSystem::FromCountryCode("jp");
 * - Getting a writing system from a Unicode string: <br>
 *   WritingSystem::Script system = WritingSystem::FromUniChars(my_uni_char_string);
 *
 * Instantiation of the class itself is not allowed, if your code previously
 * did instantiate WritingSystem objects, change it to use the
 * WritingSystem::Script enumeration directly, as in the examples above.
 *
 * @author Peter Krefting, Johan Herland
 */
class WritingSystem
{
public:
	/**
	 * Enumeration for the various script types recognized by this class.
	 * The enumeration values do NOT correspond to the Unicode Standard
	 * Annex #24 script types. Rather they are somewhat derived from the
	 * "codepages" reported by some fonts
	 * (see http://www.microsoft.com/typography/OTSpec/os2.htm#cpr for more
	 * information).
	 */
	enum Script
	{
		LatinWestern,          ///< Western European alphabet (windows-1252/ISO-8859-1 based languages)
		LatinEastern,          ///< Eastern European alphabet (windows-1250/ISO-8859-2 based languages)
		LatinUnknown,          ///< Latin-based unknown alphabet

		Cyrillic,              ///< Cyrillic alphabet (windows-1251/ISO-8859-5 based languages)
		Baltic,                ///< Baltic alphabet (windows-1257/ISO-8859-13 based languages)
		Turkish,               ///< Turkish alphabet (windows-1254/ISO-8859-9 based languages)
		Greek,                 ///< Greek alphabet (windows-1253/ISO-8859-7 based languages)
		Hebrew,                ///< Hebrew alphabet (windows-1255/ISO-8859-8 based languages)
		Arabic,                ///< Arabic alphabet (windows-1256/ISO-8859-6 based languages)
		Persian,                ///< Persian alphabet (Persian superset of Arabic)

		IndicDevanagari,       ///< Devanagari script
		IndicBengali,          ///< Bengali script
		IndicGurmukhi,         ///< Gurmukhi script
		IndicGujarati,         ///< Gujarati script
		IndicOriya,            ///< Oriya script
		IndicTamil,            ///< Tamil script
		IndicTelugu,           ///< Telugu script
		IndicKannada,          ///< Kannada script
		IndicMalayalam,        ///< Malayalam script
		IndicSinhala,          ///< Sinhala script
		IndicLao,              ///< Lao script
		IndicTibetan,          ///< Tibetan script
		IndicMyanmar,          ///< Myanmar script
		IndicUnknown,          ///< Unknown Indic script (language using one of the above Indic scripts)

		Thai,                  ///< Thai alphabet (cp874/ISO-8859-11/TIS-620 based languages)
		Vietnamese,            ///< Vietnamese alphabet (windows-1258 based languages)

		ChineseSimplified,     ///< Han ideographics in Chinese simplified style.
		ChineseTraditional,    ///< Han ideographics in Chinese traditional style.
		ChineseUnknown,        ///< Han ideographics in unknown Chinese style.
		Japanese,              ///< Japanese (Japanese Han + katakana + hiragana, etc)
		Korean,                ///< Korean (Korean Han + Hangul).

		Cherokee,              ///< Cherokee script.

		/* Add more here if needed */

		Unknown,               ///< Unknown or undetermined writing system.
		NumScripts,            ///< Meta value: Number of items in this enum.
	};

	/**
	 * Determine a probable script type from a language code. Given a
	 * ISO 639 two-letter language code, possibly qualified with a ISO
	 * 3166 two-letter country code or script variation identifier after
	 * a one-character delimiter, determine the most probable writing
	 * system used for this language.
	 *
	 * This is meant to be used in the font switching code to determine
	 * which kind of font to use for a given language, and for the encoding
	 * autodetection to be set up from a document and/or system language.
	 *
	 * See RFC 4646/4647 for documentation on the format of language codes.
	 * This method does not care about which delimeter is used in the code,
	 * both RFC-style with hyphens and GNU locale-style with underscores
	 * are supported.
	 *
	 * @param langcode A language code on the form "ll", "ll_CC", "ll-CC",
	 *                 "ll_Vvvv", or "ll-Vvvv" where "ll" is language,
	 *                 "CC" is country and "Vvvv" is script variant. If the
	 *                 provided string is NULL or less than two characters,
	 *                 then the writing system cannot be determined and Unknown
	 *                 is returned.
	 * @return A writing system, or Unknown if not known.
	 */
	static WritingSystem::Script FromLanguageCode(const char *langcode);

	/**
	 * FromLanguageCode version that takes a const uni_char*.
	 */
	static WritingSystem::Script FromLanguageCode(const uni_char *langcode);

	/**
	 * Determine a probable script type from an encoding label. Given an
	 * IANA character encoding name, determine the most probable writing
	 * system used for this character encoding.
	 *
	 * This is meant to be used in the font switching code if there is no
	 * language information available for the resource, using a language
	 * code should have preference over using the encoding label.
	 *
	 * See RFC 2978 for details on the IANA charset registry.
	 *
	 * @param encoding An IANA charset label. Need not be in the canonical
	 *                 form. If the provided string is NULL then the writing
	 *                 system cannot be determined and Unknown is returned.
	 * @return A writing system, or Unknown if not known.
	 */
	static WritingSystem::Script FromEncoding(const char *encoding);

	/**
	 * Determine a probable script type from a country code. Given a
	 * ISO 3166 two-letter language code (or Internet top domain),
	 * determine the most probable writing system used in this country.
	 *
	 * This is meant to be used in the font switching and encoding
	 * autodetection code if there is no language or character encoding
	 * information available for the resource, using a language code or
	 * encoding label should have preference over using a country code.
	 *
	 * @param countrycode A country code (or Internet top domain) in the
	 *                    form "cc". If the provided string is NULL or less than
	 *                    two characters long, then the writing system cannot be
	 *                    determined and Unknown is returned.
	 *                    Does not need to be nul-terminated, as only the
	 *                    first two letters of the input string are
	 *                    considered.
	 * @return A writing system, or Unknown if not known.
	 */
	static WritingSystem::Script FromCountryCode(const char *countrycode);

#ifdef WIC_HAVE_WRITINGSYSTEM_FROM_UNICODE
	/**
	 * Determine a probable script type from a string of Unicode
	 * characters. Given a uni_char string, determine the most probable
	 * writing system used to represent this string.
	 *
	 * This is meant to be used in the font switching code if there is no
	 * language information available for the resource, using a language
	 * code, encoding label, or country code should have preference over
	 * using the Unicode string analysis.
	 *
	 * Note that this method is not very reliable, as many commonly used
	 * Unicode characters (esp. punctuation and whitespace characters) are
	 * not associated with a specific writing system.
	 *
	 * @param str A string of Unicode characters.
	 * @param length Number of characters in given string to analyze.
	 *               If negative, the number of characters will be found by
	 *               calling uni_strlen(str)
	 *
	 * @return A writing system, or Unknown if not known.
	 */
	static WritingSystem::Script FromUniChars(
		const uni_char *str,
		int length = -1);
#endif // WIC_HAVE_WRITINGSYSTEM_FROM_UNICODE

private:
	/**
	 * Disallow instantiation of this class.
	 *
	 * Instances of this class were simply trivial wrappers around ::Script.
	 * There is no point to using them instead of using ::Script directly.
	 * Instantiating this class is therefore DEPRECATED in favour of
	 * using WritingSystem::Script direclty instead.
	 */
	WritingSystem(enum Script script); /* has no implementation */
};

#endif // !DISPLAY_WRITINGSYSTEM_H
