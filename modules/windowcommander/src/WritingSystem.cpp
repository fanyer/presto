/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johan Herland
*/

#include "core/pch.h"


#include "modules/windowcommander/WritingSystem.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/unicode/unicode.h"

/** Pack two chars in an int. */
#define STR2INT16(x,y) (((x) << 8) | (y))

/** Pack three chars in an int. */
#define STR2INT24(x,y,z) (((x) << 16) | ((y) << 8) | (z))

/* static */
WritingSystem::Script WritingSystem::FromLanguageCode(const char *langcode)
{
	// An empty langcode or a single-char langcode is a invalid langcode string
	if (!langcode || !langcode[0] || !langcode[1])
		return Unknown;

	int lcode = 0;
	if (langcode[2] == 0 || langcode[2] == '-' || langcode[2] == '_')
	{
		lcode = STR2INT16(op_toupper((unsigned char) langcode[0]), op_toupper((unsigned char) langcode[1]));
	}
	else if (langcode[3] == 0 || langcode[3] == '-' || langcode[3] == '_')
	{
		lcode = STR2INT24(op_toupper((unsigned char) langcode[0]), op_toupper((unsigned char) langcode[1]), op_toupper((unsigned char) langcode[2]));
	}

	size_t langcodelen;

	// Since it's a two-letter code, it's faster to compare it as a
	// 16-bit integer
	switch (lcode)
	{
	// The following are covered by ISO-8859-1, ISO-8859-15, windows-1252,
	// or us-ascii (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-1)
	case STR2INT16('A', 'F'): // Afrikaans
	case STR2INT16('B', 'R'): // Breton
	case STR2INT16('C', 'A'): // Catalan
	case STR2INT16('C', 'O'): // Corsican
	case STR2INT16('D', 'A'): // Danish
	case STR2INT16('D', 'E'): // German
	case STR2INT16('E', 'N'): // English
	case STR2INT16('E', 'S'): // Spanish
	case STR2INT16('E', 'T'): // Estonian (this is not considered 'Baltic')
	case STR2INT16('E', 'U'): // Basque
	case STR2INT16('F', 'I'): // Finnish
	case STR2INT16('F', 'O'): // Faroese
	case STR2INT16('F', 'R'): // French
	case STR2INT16('G', 'A'): // Irish
	case STR2INT16('G', 'D'): // Scottish Gaelic
	case STR2INT16('G', 'L'): // Galician
	case STR2INT16('G', 'S'): // Probably GSW: Alsatian
	case STR2INT16('I', 'S'): // Icelandic
	case STR2INT16('I', 'T'): // Italian
	case STR2INT16('K', 'U'): // Kurdish
	case STR2INT16('L', 'A'): // Latin
	case STR2INT16('L', 'B'): // Luxembourgish/Letzeburgesch
	case STR2INT16('N', 'B'): // Norwegian Bokm&aring;l
	case STR2INT16('N', 'L'): // Dutch/Flemish
	case STR2INT16('N', 'N'): // Norwegian Nynorsk
	case STR2INT16('N', 'O'): // Norwegian
	case STR2INT16('O', 'C'): // Occitan
	case STR2INT16('P', 'T'): // Portuguese
	case STR2INT16('R', 'M'): // Rhaeto-Romanic/Romansh
	case STR2INT16('S', 'Q'): // Albanian
	case STR2INT16('S', 'V'): // Swedish
	case STR2INT16('S', 'W'): // Swahili
	case STR2INT16('W', 'A'): // Walloon
		return LatinWestern;

	// The following are covered by ISO-8859-2, ISO-8859-16, or
	// windows-1250 (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-2)
	case STR2INT16('B', 'S'): // Bosnian
	case STR2INT16('C', 'S'): // Czech
	case STR2INT16('H', 'R'): // Croatian
	case STR2INT16('H', 'U'): // Hungarian
	case STR2INT16('P', 'L'): // Polish
	case STR2INT16('R', 'O'): // Romanian
	case STR2INT16('S', 'K'): // Slovak
	case STR2INT16('S', 'L'): // Slovenian
	case STR2INT16('T', 'K'): // Turkmen (also written in Cyrillic)
	case STR2INT16('M', 'O'): // Moldovan (uses the same alphabet as Romanian)
		return LatinEastern;

	// The entries marked [*] may rather belong in the LatinWestern section,
	// according to http://www.microsoft.com/globaldev/nlsweb/default.mspx

	// The following are covered by ISO-8859-3
	// (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-3)
	case STR2INT16('E', 'O'): // Esperanto
	case STR2INT16('M', 'T'): // Maltese
	// The following are covered by ISO-8859-4, ISO-8859-10, or
	// windows-sami-2 (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-4)
	// However, http://www.microsoft.com/globaldev/nlsweb/default.mspx claims
	// they should be mapped to windows-1252 (= LatinWestern)
	case STR2INT16('K', 'L'): // Kalaallisut/Greenlandic [*]
	case STR2INT16('S', 'E'): // Sami [*]
	// The following is covered by ISO-8859-14
	case STR2INT16('C', 'Y'): // Welsh [*]
	// The following languages claim to be Latin-compatible
	case STR2INT16('F', 'J'): // Fijian
	case STR2INT16('F', 'Y'): // Frisian [*]
	case STR2INT16('G', 'N'): // Guarani
	case STR2INT16('G', 'V'): // Manx
	case STR2INT16('H', 'A'): // Hausa (Nigeria) [*]
	case STR2INT16('H', 'T'): // Haitian Creole
	case STR2INT16('I', 'A'): // Interlingua
	case STR2INT16('I', 'D'): // Indonesian [*]
	case STR2INT16('I', 'E'): // Occidental
	case STR2INT16('I', 'G'): // Igbo (Nigeria) [*]
	case STR2INT16('I', 'N'): // Indonesian (deprecated) [*]
	case STR2INT16('L', 'N'): // Lingala
	case STR2INT16('M', 'G'): // Malagasy
	case STR2INT16('M', 'H'): // Marshallese
	case STR2INT16('M', 'S'): // Malay [*]
	case STR2INT16('N', 'A'): // Nauruan
	case STR2INT16('N', 'S'): // Probably NSO: Sesotho sa Leboa [*]
	case STR2INT16('O', 'M'): // Oromo
	case STR2INT16('Q', 'U'): // Probably QUT or QUZ: K'iche or Quechua [*]
	case STR2INT16('R', 'W'): // Kinyarwanda [*]
	case STR2INT16('S', 'M'): // Samoan or Sami (smn, smj, sms, or sma) [*]
	case STR2INT16('S', 'O'): // Somali
	case STR2INT16('T', 'M'): // Probably TMZ: Tamazight [*]
	case STR2INT16('T', 'N'): // Setswana [*]
	case STR2INT16('T', 'O'): // Tongan (Tonga Islands)
	case STR2INT16('T', 'S'): // Tsonga
	case STR2INT16('V', 'E'): // Venda
	case STR2INT16('W', 'E'): // Probably WEE or WEN: Lower/Upper Sorbian [*]
	case STR2INT16('W', 'O'): // Wolof (Senegal) [*]
	case STR2INT16('X', 'H'): // Xhosa
	case STR2INT16('Y', 'O'): // Yoruba (Nigeria) [*]
	case STR2INT16('Z', 'U'): // Zulu
		return LatinUnknown;

	case STR2INT16('I', 'U'): // Inuktitut [*]
		// written in Latin or with a custom syllabic alphabet.
		// According to http://www.microsoft.com/globaldev/nlsweb/default.mspx
		// "iu-Latn-CA" is Latin, while "iu-Cans-CA" is Syllabic.
		langcodelen = op_strlen(langcode);
		if (langcodelen >= 7) {
			if (op_strnicmp("cans", langcode, 4))
				return Unknown;
		}
		return LatinUnknown;

	// The following are covered by ISO-8859-5, KOI8-R/U, windows-1251, or
	// CP866 (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-5)
	case STR2INT16('A', 'B'): // Abkhazian
	case STR2INT16('B', 'A'): // Bashkir
	case STR2INT16('B', 'E'): // Belarussian
	case STR2INT16('B', 'G'): // Bulgarian
	case STR2INT16('C', 'V'): // Chuvash
	case STR2INT16('K', 'K'): // Kazakh
	case STR2INT16('K', 'V'): // Komi
	case STR2INT16('K', 'Y'): // Kirghiz
	case STR2INT16('M', 'K'): // Macedonian
	case STR2INT16('M', 'N'): // Mongolian (also occurs in the Mongolian script)
	case STR2INT16('R', 'U'): // Russian
	case STR2INT16('S', 'A'): // Probably SAH: Yakut
	case STR2INT16('S', 'R'): // Serbian
	case STR2INT16('T', 'G'): // Tajik
	case STR2INT16('T', 'T'): // Tatar
	case STR2INT16('U', 'K'): // Ukranian
		return Cyrillic;

	case STR2INT16('A', 'Z'): // Azerbaijani/Azeri
	case STR2INT16('U', 'Z'): // Uzbek
		// Commonly written in Latin/Turkish or Cyrillic (or even Arabic).
		// According to http://www.microsoft.com/globaldev/nlsweb/default.mspx
		// "az" alone or "az-Latn-AZ" is Latin/Turkish, while "az-Cyrl-AZ" is
		// Cyrillic. Same goes for "uz"/"uz-Latn-UZ"/"uz-Cyrl-UZ".
		langcodelen = op_strlen(langcode);
		if (langcodelen >= 7) {
			if (op_strnicmp("latn", langcode, 4))
				return Turkish;
			else if (op_strnicmp("cyrl", langcode, 4))
				return Cyrillic;
		}
		return Turkish;

	// The following are covered by ISO-8859-13, or windows-1257
	// (cf. http://en.wikipedia.org/wiki/Windows-1257)
	case STR2INT16('L', 'T'): // Lithuanian
	case STR2INT16('L', 'V'): // Latvian
		return Baltic;

	// The following is covered by ISO-8859-9, or windows-1254
	// (cf. http://en.wikipedia.org/wiki/Windows-1254)
	case STR2INT16('T', 'R'): // Turkish
		return Turkish;

	// The following is covered by ISO-8859-7, or windows-1253
	// (cf. http://en.wikipedia.org/wiki/Windows-1253)
	case STR2INT16('E', 'L'): // Modern Greek
		return Greek;

	// The following are covered by ISO-8859-8, or windows-1255
	// (cf. http://en.wikipedia.org/wiki/Windows-1255)
	case STR2INT16('H', 'E'): // Hebrew
	case STR2INT16('I', 'W'): // Hebrew (deprecated)
	case STR2INT16('J', 'I'): // Yiddish (deprecated)
	case STR2INT16('Y', 'I'): // Yiddish
		return Hebrew;

	// The following are covered by ISO-8859-6, or windows-1256
	// (cf. http://en.wikipedia.org/wiki/Windows-1256)
	case STR2INT16('A', 'R'): // Arabic
	case STR2INT16('P', 'R'): // Probably PRS: Dari
	case STR2INT16('P', 'S'): // Pushto
	case STR2INT16('U', 'G'): // Uighur
	case STR2INT16('U', 'R'): // Urdu
		return Arabic;

	case STR2INT16('F', 'A'): // Persian
		return Persian;

	case STR2INT16('H', 'I'): // Hindi (Devanagari script)
	case STR2INT16('M', 'R'): // Marathi (Devanagari script)
	case STR2INT16('N', 'E'): // Nepali (Devanagari script)
		return IndicDevanagari;

	case STR2INT16('A', 'S'): // Assamese (Bengali script)
	case STR2INT16('B', 'N'): // Bengali (Bengali script)
		return IndicBengali;

	case STR2INT16('P', 'A'): // Punjabi (Gurmukhi script)
		return IndicGurmukhi;

	case STR2INT16('G', 'U'): // Gujarati (Gujarati script)
		return IndicGujarati; 

	case STR2INT16('O', 'R'): // Oriya (Oriya script)
		return IndicOriya;

	case STR2INT16('T', 'A'): // Tamil (Tamil script)
		return IndicTamil;

	case STR2INT16('T', 'E'): // Telugu (Telugu script)
		return IndicTelugu;

	case STR2INT16('K', 'N'): // Kannada (Kannada script)
		return IndicKannada;

	case STR2INT16('M', 'L'): // Malayalam (Malayalam script)
		return IndicMalayalam;

	case STR2INT16('S', 'I'): // Sinhalese (Sinhala script)
		return IndicSinhala;

	case STR2INT16('L', 'O'): // Lao (Lao script)
		return IndicLao;

	case STR2INT16('D', 'Z'): // Dzongkha (Tibetan script)
		return IndicTibetan;

	case STR2INT16('M', 'Y'): // Burmese (Myanmar script)
		return IndicMyanmar;

	// The following is covered by ISO-8859-11
	// (cf. http://en.wikipedia.org/wiki/ISO/IEC_8859-11)
	case STR2INT16('T', 'H'): // Thai
		return Thai;

	// The following is covered by windows-1258, tcvn, viscii, or x-vps
	// (cf. http://en.wikipedia.org/wiki/Windows-1258)
	case STR2INT16('V', 'I'): // Vietnamese
		return Vietnamese;

	case STR2INT16('Z', 'H'): // Chinese
		{
			// Check for variation of Chinese.
			langcodelen = op_strlen(langcode);

			// With country code: Mainland China uses simplified, whereas
			// Hong Kong and Taiwan uses traditional.
			if (langcodelen >= 5)
			{
				switch (STR2INT16(op_toupper((unsigned char) langcode[3]),
								  op_toupper((unsigned char) langcode[4])))
				{
				case STR2INT16('C', 'N'): // China
				case STR2INT16('M', 'O'): // Macao
				case STR2INT16('S', 'G'): // Singapore
					return ChineseSimplified;

				case STR2INT16('H', 'K'): // Hong Kong
				case STR2INT16('T', 'W'): // Taiwan
					return ChineseTraditional;

				case STR2INT16('H', 'A'):
					// With script code: "Hans" means simplified and "Hant" means
					// traditional.
					if (langcodelen >= 7)
					{
						switch (STR2INT16(op_toupper((unsigned char) langcode[5]),
										  op_toupper((unsigned char) langcode[6])))
						{
						case STR2INT16('N', 'S'):
							return ChineseSimplified;
						case STR2INT16('N', 'T'):
							return ChineseTraditional;
						}
					}
				}
			}

			// Unrecognized.
			return ChineseUnknown;
		}

	case STR2INT16('J', 'A'): // Japanese
		return Japanese;

	case STR2INT16('K', 'O'): // Korean
		return Korean;

	case STR2INT24('M', 'O', 'H'): // Mohawk
		// Mohawk is some kind of latin [*]
		return LatinUnknown;

	case STR2INT24('C', 'H', 'R'): // Cherokee
		return Cherokee;
	}

	return Unknown;
}

/* static */
WritingSystem::Script WritingSystem::FromLanguageCode(const uni_char *langcode)
{
	// 16 is a size big enough to hold a lang-region format
	if (langcode && langcode[0] && langcode[1])
	{
		char buf[16]; // ARRAY OK 2011-08-23 ulfma
		uni_cstrlcpy(buf, langcode, ARRAY_SIZE(buf));
		return FromLanguageCode(buf);
	}
	else
		return Unknown;
}

WritingSystem::Script WritingSystem::FromEncoding(const char *encoding)
{
	if (!encoding)
		return Unknown;

	if (strni_eq(encoding, "autodetect", 10)) {
		CharsetDetector::AutoDetectMode mode =
			CharsetDetector::AutoDetectIdFromString(encoding);
		switch (mode)
		{
#ifdef ENCODINGS_HAVE_JAPANESE
		case CharsetDetector::japanese:
			return Japanese;
#endif
#ifdef ENCODINGS_HAVE_CHINESE
		case CharsetDetector::chinese_simplified:
			return ChineseSimplified;
		case CharsetDetector::chinese_traditional:
			return ChineseTraditional;
		case CharsetDetector::chinese:
			return ChineseUnknown;
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		case CharsetDetector::korean:
			return Korean;
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
		case CharsetDetector::cyrillic:
			return Cyrillic;
#endif
		default:
			return Unknown;
		}
	}

	const char *canon = g_charsetManager->GetCanonicalCharsetName(encoding);
	if (!canon) // Unrecognized encoding name
		return Unknown;

	if ((0 == op_strcmp(canon, "iso-8859-1"))
	 || (0 == op_strcmp(canon, "iso-8859-15"))
	 || (0 == op_strcmp(canon, "windows-1252"))
	 || (0 == op_strcmp(canon, "us-ascii")))
		return LatinWestern;

	if ((0 == op_strcmp(canon, "iso-8859-2"))
	 || (0 == op_strcmp(canon, "iso-8859-16"))
	 || (0 == op_strcmp(canon, "windows-1250")))
		return LatinEastern;

	if ((0 == op_strcmp(canon, "iso-8859-3"))
	 || (0 == op_strcmp(canon, "iso-8859-4"))
	 || (0 == op_strcmp(canon, "iso-8859-10"))
	 || (0 == op_strcmp(canon, "iso-8859-14"))
	 || (0 == op_strcmp(canon, "windows-1252")))
		return LatinUnknown;

	if ((0 == op_strcmp(canon, "iso-8859-5"))
	 || (0 == op_strcmp(canon, "koi8-r"))
	 || (0 == op_strcmp(canon, "koi8-u"))
	 || (0 == op_strcmp(canon, "windows-1251"))
	 || (0 == op_strcmp(canon, "ibm866")))
		return Cyrillic;

	if ((0 == op_strcmp(canon, "iso-8859-13"))
	 || (0 == op_strcmp(canon, "windows-1257")))
		return Baltic;

	if ((0 == op_strcmp(canon, "iso-8859-9"))
	 || (0 == op_strcmp(canon, "windows-1254")))
		return Turkish;

	if ((0 == op_strcmp(canon, "iso-8859-7"))
	 || (0 == op_strcmp(canon, "windows-1253")))
		return Greek;

	if ((0 == op_strcmp(canon, "iso-8859-8"))
	 || (0 == op_strcmp(canon, "iso-8859-8-i"))
	 || (0 == op_strcmp(canon, "windows-1255")))
		return Hebrew;

	if ((0 == op_strcmp(canon, "iso-8859-6"))
	 || (0 == op_strcmp(canon, "windows-1256")))
		return Arabic;

	if (0 == op_strcmp(canon, "iso-8859-11"))
		return Thai;

	if ((0 == op_strcmp(canon, "windows-1258"))
	 || (0 == op_strcmp(canon, "tcvn"))
	 || (0 == op_strcmp(canon, "viscii"))
	 || (0 == op_strcmp(canon, "x-vps")))
		return Vietnamese;

	if ((0 == op_strcmp(canon, "big5"))
	 || (0 == op_strcmp(canon, "big5-hkscs"))
	 || (0 == op_strcmp(canon, "euc-tw")))
		return ChineseTraditional;

	if ((0 == op_strcmp(canon, "gbk"))
	 || (0 == op_strcmp(canon, "gb2312"))
	 || (0 == op_strcmp(canon, "gb18030"))
	 || (0 == op_strcmp(canon, "hz-gb-2312")))
		return ChineseSimplified;

	if (0 == op_strcmp(canon, "iso-2022-cn"))
		return ChineseUnknown; // encodes both simplified and traditional

	if ((0 == op_strcmp(canon, "euc-jp"))
	 || (0 == op_strcmp(canon, "iso-2022-jp"))
	 || (0 == op_strcmp(canon, "iso-2022-jp-1"))
	 || (0 == op_strcmp(canon, "shift_jis")))
		return Japanese;

	if ((0 == op_strcmp(canon, "euc-kr"))
	 || (0 == op_strcmp(canon, "iso-2022-kr")))
		return Korean;

	return Unknown;
}

WritingSystem::Script WritingSystem::FromCountryCode(const char *countrycode)
{
	if (!countrycode || !countrycode[0] || !countrycode[1])
		return Unknown;

	// Since we only look at exactly two letters, it's faster to compare
	// them as a 16-bit integer
	switch (STR2INT16(op_toupper((unsigned char) countrycode[0]),
	                  op_toupper((unsigned char) countrycode[1])))
	{
	case STR2INT16('A', 'D'): // Andorra
	case STR2INT16('A', 'G'): // Antigua and Barbuda
	case STR2INT16('A', 'L'): // Albania
	case STR2INT16('A', 'R'): // Argentina
	case STR2INT16('A', 'T'): // Austria
	case STR2INT16('A', 'U'): // Australia
	case STR2INT16('B', 'E'): // Belgium
	case STR2INT16('B', 'O'): // Bolivia
	case STR2INT16('B', 'R'): // Brazil
	case STR2INT16('C', 'A'): // Canada
	case STR2INT16('C', 'H'): // Switzerland
	case STR2INT16('C', 'L'): // Chile
	case STR2INT16('C', 'O'): // Colombia
	case STR2INT16('C', 'R'): // Costa Rica
	case STR2INT16('C', 'U'): // Cuba
	case STR2INT16('D', 'E'): // Germany
	case STR2INT16('D', 'K'): // Denmark
	case STR2INT16('E', 'C'): // Ecuador
	case STR2INT16('E', 'E'): // Estonia
	case STR2INT16('E', 'S'): // Spain
	case STR2INT16('F', 'I'): // Finland
	case STR2INT16('F', 'O'): // Faroe Islands
	case STR2INT16('F', 'R'): // France
	case STR2INT16('G', 'B'): // United Kingdom
	case STR2INT16('G', 'G'): // Guernsey
	case STR2INT16('G', 'T'): // Guatemala
	case STR2INT16('H', 'N'): // Honduras
	case STR2INT16('I', 'E'): // Ireland
	case STR2INT16('I', 'M'): // Isle of Man
	case STR2INT16('I', 'S'): // Iceland
	case STR2INT16('I', 'T'): // Italy
	case STR2INT16('J', 'E'): // Jersey
	case STR2INT16('J', 'M'): // Jamaica
	case STR2INT16('K', 'E'): // Kenya
	case STR2INT16('L', 'I'): // Liechtenstein
	case STR2INT16('L', 'U'): // Luxembourg
	case STR2INT16('M', 'C'): // Monaco
	case STR2INT16('M', 'X'): // Mexico
	case STR2INT16('N', 'I'): // Nicaragua
	case STR2INT16('N', 'L'): // Netherlands
	case STR2INT16('N', 'O'): // Norway
	case STR2INT16('P', 'A'): // Panama
	case STR2INT16('P', 'E'): // Peru
	case STR2INT16('P', 'R'): // Puerto Rico
	case STR2INT16('P', 'T'): // Portugal
	case STR2INT16('S', 'E'): // Sweden
	case STR2INT16('S', 'M'): // San Marino
	case STR2INT16('T', 'T'): // Trinidad and Tobago
	case STR2INT16('U', 'S'): // USA
	case STR2INT16('U', 'Y'): // Uruguay
	case STR2INT16('V', 'A'): // Vatican
	case STR2INT16('V', 'E'): // Venezuela
	case STR2INT16('U', 'K'): // United Kingdom
		return LatinWestern;

	case STR2INT16('C', 'Z'): // Czech
	case STR2INT16('H', 'R'): // Croatia
	case STR2INT16('H', 'U'): // Hungary
	case STR2INT16('P', 'L'): // Poland
	case STR2INT16('R', 'O'): // Romania
	case STR2INT16('S', 'I'): // Slovenia
	case STR2INT16('S', 'K'): // Slovakia
		return LatinEastern;

	case STR2INT16('I', 'D'): // Indonesian
	case STR2INT16('K', 'I'): // Kiribati
	case STR2INT16('M', 'H'): // Marshall Islands
	case STR2INT16('M', 'T'): // Malta
	case STR2INT16('M', 'Y'): // Malaysia
	case STR2INT16('N', 'R'): // Nauru
	case STR2INT16('N', 'Z'): // New Zealand
	case STR2INT16('P', 'Y'): // Paraguay
	case STR2INT16('T', 'O'): // Tonga Islands
	case STR2INT16('V', 'U'): // Vanuatu
	case STR2INT16('Z', 'A'): // South Africa
		return LatinUnknown;

	case STR2INT16('B', 'G'): // Bulgaria
	case STR2INT16('B', 'Y'): // Belarus
	case STR2INT16('K', 'G'): // Kyrgyzstan
	case STR2INT16('K', 'Z'): // Kazakhstan
	case STR2INT16('M', 'K'): // Macedonia
	case STR2INT16('R', 'U'): // Russia
	case STR2INT16('T', 'J'): // Tajikistan
	case STR2INT16('U', 'A'): // Ukraine
		return Cyrillic;

	case STR2INT16('L', 'T'): // Lithuania
	case STR2INT16('L', 'V'): // Latvia
		return Baltic;

	case STR2INT16('G', 'R'): // Greece
		return Greek;

	case STR2INT16('T', 'R'): // Turkey
		return Turkish;

	case STR2INT16('A', 'E'): // United Arab Emirates
	case STR2INT16('A', 'F'): // Afghanistan
	case STR2INT16('B', 'H'): // Bahrain
	case STR2INT16('D', 'Z'): // Algeria
	case STR2INT16('E', 'G'): // Egypt
	case STR2INT16('I', 'R'): // Iran
	case STR2INT16('J', 'O'): // Jordan
	case STR2INT16('K', 'W'): // Kuwait
	case STR2INT16('L', 'B'): // Lebanon
	case STR2INT16('L', 'Y'): // Libya
	case STR2INT16('M', 'A'): // Morocco
	case STR2INT16('O', 'M'): // Oman
	case STR2INT16('Q', 'A'): // Qatar
	case STR2INT16('S', 'A'): // Saudi Arabia
	case STR2INT16('S', 'Y'): // Syria
	case STR2INT16('T', 'N'): // Tunisia
	case STR2INT16('Y', 'E'): // Yemen
		return Arabic;

	case STR2INT16('N', 'P'): // Nepal (Devanagari script)
		return IndicDevanagari;

	case STR2INT16('B', 'D'): // Bangladesh (Bengali script)
		return IndicBengali;

	case STR2INT16('L', 'A'): // Laos (Lao script)
		return IndicLao;

	case STR2INT16('B', 'T'): // Bhutan (Tibetan script)
		return IndicTibetan;

	case STR2INT16('M', 'M'): // Myanmar (Myanmar script)
		return IndicMyanmar;

	case STR2INT16('L', 'K'): // Sri Lanka (Sinhala and Tamil scripts)
		return IndicUnknown;

	case STR2INT16('T', 'H'): // Thailand
		return Thai;

	case STR2INT16('V', 'N'): // Vietnam
		return Vietnamese;

	case STR2INT16('C', 'N'): // China
	case STR2INT16('M', 'O'): // Macao
		return ChineseSimplified;

	case STR2INT16('H', 'K'): // Hong Kong
	case STR2INT16('T', 'W'): // Taiwan
		return ChineseTraditional;

	case STR2INT16('J', 'P'): // Japan
		return Japanese;

	case STR2INT16('K', 'P'): // Korea, North
	case STR2INT16('K', 'R'): // Korea, South
		return Korean;

	// The following are explicitly unknown because several writing system
	// are in common use in those countries
	case STR2INT16('A', 'Z'): // Azerbaijan (Latin, Cyrillic, Arabic)
	case STR2INT16('B', 'A'): // Bosnia and Herzegovina (Latin, Cyrillic)
	case STR2INT16('C', 'Y'): // Cyprus (Greek, Turkish)
	case STR2INT16('I', 'L'): // Israel (Hebrew, Arabic, Latin)
	case STR2INT16('I', 'N'): // India (English, all Indic scripts)
	case STR2INT16('M', 'D'): // Moldova (Latin, Cyrillic)
	case STR2INT16('M', 'E'): // Montenegro (Latin, Cyrillic)
	case STR2INT16('M', 'R'): // Mauritania (Arabic, Latin)
	case STR2INT16('P', 'K'): // Pakistan (Arabic, English, some Indic scripts)
	case STR2INT16('R', 'S'): // Serbia (Cyrillic, Latin)
	case STR2INT16('S', 'G'): // Singapore (English, Malay, Mandarin, Tamil)
	case STR2INT16('S', 'O'): // Somalia (Latin, Arabic)
	case STR2INT16('T', 'M'): // Turkmenistan (Cyrillic, Latin/Turkish)
	case STR2INT16('U', 'Z'): // Uzbekistan (Cyrillic, Latin, Arabic?)
	// The following are explicitly unknown because we don't recognize that
	// script
	case STR2INT16('K', 'H'): // Cambodia (Khmer script)
		return Unknown;
	}

	return Unknown;
}
