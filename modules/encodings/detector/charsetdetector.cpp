/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef ENCODINGS_REGTEST
# include "modules/dochand/win.h"
# include "modules/dochand/winman.h"
# include "modules/hardcore/mem/mem_man.h"
# include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // !ENCODINGS_REGTEST
#include "modules/encodings/utility/charsetnames.h"

#include "modules/encodings/detector/charsetdetector.h"

// Helper macros
#define IN_RANGE(var, low, high) ((var) >= (low) && (var) <= (high))
#define IN_SET3(var, opt1, opt2, opt3) ((var) == (opt1) || (var) == (opt2) || (var) == (opt3))
#define STR2INT16(x,y) ((x << 8) | y)
#define STR2INT24(x,y,z) (((x << 16) | (y << 8)) | z)

// Constructor and destrutor for CharsetDetector ----------------------------
CharsetDetector::CharsetDetector(
	const char *server           /* = NULL */,
	Window *window               /* = NULL */,
	const char *force_encoding   /* = NULL */,
	const char *content_language /* = NULL */,
	int utf8_threshold           /* = 10 */,
	BOOL allowUTF7               /* = FALSE */,
	const char *referrer_charset /* = NULL */)
	: autodetect(autodetect_none)
#ifdef ENCODINGS_HAVE_CHINESE
	, has_5_big5_consecutive(false)
#endif
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	, has_5_euc_consecutive(false)
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
	, has_5_sjis_consecutive(false)
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	, big5_hints(0), euctw_hints(0), gbk_hints(0), gb18030_hints(0), hz_hints(0)
	, big5_penalty(-1000), big5_consecutive(0)
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
	, sjis_invalid(0), sjis_valid(0), sjis_consecutive(0)
# ifndef IMODE_EXTENSIONS
	, sjis_halfwidth_katakana(0)
# endif
#endif
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	, euc_invalid(0), euc_valid(0), euc_penalty(1), euc_consecutive(0)
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
	, iso_8859_5_hints(0), koi_hints(0), windows_1251_hints(0), ibm866_hints(0)
	, iso_8859_5_invalid(0)
#endif
	, utf8_valid(0), utf8_invalid(0), utf8_threshold(utf8_threshold)
#ifdef ENCODINGS_FAST_AUTODETECTION
	, peek(ENCODINGS_BYTES_TO_PEEK)
#endif
	, result(NULL)
	, m_allowUTF7(allowUTF7)
	, utf8_length(0)
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	, iso2022state(iso2022_none)
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	, big5state(big5_none), hzstate(hz_none)
#endif
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	, eucstate(euc_none)
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
	, sjisstate(sjis_none)
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
	, cyrillic_state(nonword)
#endif
#ifndef ENCODINGS_FAST_AUTODETECTION
	, utfbomstate(utf_bom_none)
#endif
{
	// Determine what to auto-detect
	const char *override;

	if (window)
	{
		override = window->GetForceEncoding();
	}
	else if (force_encoding)
	{
		override = force_encoding;
	}
	else
	{
		override = g_pcdisplay->GetForceEncoding();
	}

	autodetect = AutoDetectIdFromString(override);
	OP_ASSERT(autodetect != generic);

	// Enable autodetect based on language if none was enabled
	if (content_language && *content_language && autodetect_none == autodetect)
	{
		autodetect = AutoDetectIdFromLanguage(content_language);
	}

	size_t serverlen = server ? op_strlen(server) : 0;
	if (serverlen > 3)
	{
		if ('.' == server[serverlen - 3])
		{
			AutoDetectMode autodetect_from_domain =
				AutoDetectIdFromWritingSystem(WritingSystem::FromCountryCode(server + serverlen - 2));
			if (autodetect_none == autodetect
#ifdef ENCODINGS_HAVE_CHINESE
				|| (chinese == autodetect &&
				    (chinese_traditional == autodetect_from_domain ||
				     chinese_simplified == autodetect_from_domain))
#endif
			   )
			{
				autodetect = autodetect_from_domain;
			}
		}
	}

#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN || defined ENCODINGS_HAVE_CYRILLIC
	// Enable autodetect based on referrer document's charset
	if (referrer_charset && autodetect == autodetect_none)
	{
		const char *can_ref_charset = g_charsetManager->GetCanonicalCharsetName(referrer_charset);
		if (!can_ref_charset) {
			// Not a valid charset. Cannot deduce autodetect mode
		}
# ifdef ENCODINGS_HAVE_JAPANESE
		else if (op_strcmp(can_ref_charset, "shift_jis") == 0 ||
		         op_strcmp(can_ref_charset, "euc-jp") == 0)
		{
			autodetect = japanese;
		}
# endif
# ifdef ENCODINGS_HAVE_CHINESE
		else if (op_strcmp(can_ref_charset, "big5") == 0 ||
		         op_strcmp(can_ref_charset, "big5-hkscs") == 0 ||
		         op_strcmp(can_ref_charset, "hz-gb-2312") == 0 ||
		         op_strcmp(can_ref_charset, "euc-tw") == 0 ||
		         op_strcmp(can_ref_charset, "gb18030") == 0 ||
		         op_strcmp(can_ref_charset, "gbk") == 0)
		{
			autodetect = chinese;
		}
# endif
# ifdef ENCODINGS_HAVE_KOREAN
		else if (op_strcmp(can_ref_charset, "euc-kr") == 0)
		{
			autodetect = korean;
		}
# endif
# ifdef ENCODINGS_HAVE_CYRILLIC
		else if (op_strcmp(can_ref_charset, "iso-8859-5") == 0 ||
		         op_strcmp(can_ref_charset, "ibm866") == 0 ||
		         op_strcmp(can_ref_charset, "koi8-u") == 0 ||
		         op_strcmp(can_ref_charset, "koi8-r") == 0 ||
		         op_strcmp(can_ref_charset, "windows-1251") == 0)
		{
			autodetect = cyrillic;
		}
# endif
	}
#endif // ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_JAPANESE || ENCODINGS_HAVE_KOREAN || ENCODINGS_HAVE_CYRILLIC

#ifdef ENCODINGS_HAVE_CHINESE
	// Adjust penalty factors to favour simplified Chinese for China
	// and traditional Chinese for Hong Kong and Taiwan
	switch (autodetect)
	{
	case chinese_simplified:
		//euc_penalty = 1;
		big5_penalty = -5000;
		autodetect = chinese;
		break;

	case chinese_traditional:
		euc_penalty = 100;
		big5_penalty = -100;
		autodetect = chinese;
		break;
	}
#endif
}

CharsetDetector::~CharsetDetector()
{
}

// Public class methods of CharsetDetector ----------------------------------

#define is_xml_space(c) (0x20 == (c) || 0x09 == (c) || 0x0D == (c) || 0x0A == (c))

const char *CharsetDetector::GetXMLEncoding(const void *buf, unsigned long len, BOOL allowUTF7 /* = FALSE */)
{
	const char *buffer = reinterpret_cast<const char *>(buf);

	const char *bom = GetUTFEncodingFromBOM(buf, len, allowUTF7);
	if (bom) return bom;
	bom = GetUTFEncodingFromText(buf, len, '<');
	if (bom) return bom;

	if (len < 18)
	{
		// 18 bytes needed for XML declaration (allowing for version to
		// be missing, which is not allowed in the XML spec)
		return NULL;
	}

	// OK, let's assume that it is ASCII-compatible
	// Skip leading whitespace (Bug#53090) (actually not allowed in XML)
	while (is_xml_space(*buffer) && len > 17)
	{
		buffer ++;
		len --;
	}

	if (op_strncmp(buffer, "<?xml", 5) != 0)
	{
		return NULL; // no XML declaration -> use default
	}

	if (!is_xml_space(buffer[5]))
	{
		// Make sure this is the XML declaration, and not something like
		// "xml-stylesheet"
		return NULL;
	}

	// Locate the end of the declaration. Ignore the possibility of a
	// "?>" inside the attribute value.
	const char *bufend = scan_to("?>", buffer, buffer + len);
	if (bufend == NULL)
	{
		bufend = buffer + len;
	}

	// Find the string "encoding"
	buffer = scan_to("encoding", buffer, bufend);

	if (buffer == NULL) // no encoding declaration
	{
		return NULL;
	}

	// Skip any whitespace after the encoding tag
	while (buffer < bufend && is_xml_space(*buffer))
	{
		buffer ++;
	}

	// Now we should be pointing to an equal sign
	if (buffer == bufend || '=' != *buffer)
	{
		return NULL;
	}

	buffer ++;

	// Skip any whitespace following the equal sign
	while (buffer < bufend && is_xml_space(*buffer))
	{
		buffer ++;
	}

	if (buffer == bufend)
	{
		return NULL;
	}

	// Get delimiter character (should be ' or ", but we're not picky)
	char delim = *buffer;
	buffer ++;
	const char *start = buffer;

	buffer = scan_to(delim, buffer, bufend);
	if (buffer == bufend)
	{
		return NULL;
	}

	unsigned int enclen = buffer - start;

	char *enc = reinterpret_cast<char *>(g_memory_manager->GetTempBuf());
	if (enclen >= g_memory_manager->GetTempBufLen())
	{
		enclen = g_memory_manager->GetTempBufLen() - 1;
	}
	op_strncpy(enc, start, enclen);
	enc[enclen]= 0x00;

	if (strni_eq(enc, "UTF-16", 6))
	{
		// That is a lie
		return NULL;
	}

	return enc;
}

static BOOL is_js_space(char ch)
{
	/* Note: the full set of EcmaScript whitespace characters not recognised. */
	return (ch >= 9 && ch <= 13 || ch == 32 || ch == '\xA0');
}

static BOOL
scan_next_comment_line(const char *&comment, unsigned &comment_length, const char *&source)
{
	while (*source)
	{
		while (*source && is_js_space(*source))
			++source;

		if (!*source)
			return FALSE;

		const char *line = source;

		while (*source && *source != 10 && *source != 13)
			++source;

		if (line != source)
		{
			const char *line_end = source;

			while (line_end != line && is_js_space(line_end[-1]))
				--line_end;

			if (line_end - line > 2)
				if (line[0] == '/' && line[1] == '/')
				{
					line += 2;

					while (line != line_end && op_isspace(*line))
						++line;

					comment = line;
					comment_length = line_end - line;

					return TRUE;
				}
		}
	}

	return FALSE;
}

const char *CharsetDetector::GetJSEncoding(const void *buf, unsigned long len, BOOL forUserJS /* = FALSE*/, BOOL allowUTF7 /* = FALSE */)
{
	const char *buffer = reinterpret_cast<const char *>(buf);

	const char *bom = GetUTFEncodingFromBOM(buf, len, allowUTF7);
	if (bom) return bom;

	bom = GetUTFEncodingFromText(buf, len);
	if (bom || !forUserJS) return bom;

	const char *comment;
	unsigned comment_length;
	BOOL in_userscript = FALSE;

	const char *source_iter = buffer;

	const char *enc_start = NULL;
	const char *enc_end = NULL;

	while (scan_next_comment_line(comment, comment_length, source_iter))
	{
		if (!in_userscript)
		{
			if (comment_length == 14 && uni_strncmp(comment, UNI_L("==UserScript=="), 14) == 0)
				in_userscript = TRUE;
		}
		else
		{
			if (comment_length == 15 && uni_strncmp(comment, UNI_L("==/UserScript=="), 15) == 0)
				break;

			const char *comment_end = comment + comment_length;

			enc_start = scan_to("@encoding", comment, comment_end);
			enc_end = NULL;
			if (!enc_start || !is_js_space(enc_start[0]))
				continue;

			while (enc_start < comment_end && is_js_space(*enc_start))
			{
				enc_start++;
			}

			enc_end = enc_start;
			while (enc_end < comment_end && !is_js_space(*enc_end))
			{
				enc_end++;
			}
		}
	}

	if (enc_start && enc_end > enc_start)
    {
		unsigned int enc_len = enc_end - enc_start;
		char *enc = reinterpret_cast<char *>(g_memory_manager->GetTempBuf());
		if (enc_len >= g_memory_manager->GetTempBufLen())
		{
			enc_len = g_memory_manager->GetTempBufLen() - 1;
		}

		op_strncpy(enc, enc_start, enc_len);
		enc[enc_len]= 0x00;

		return enc;
	}
	else
		return NULL;
}

const char *CharsetDetector::GetCSSEncoding(const void *buf, unsigned long len, BOOL allowUTF7 /* = FALSE */)
{
	const char *buffer = reinterpret_cast<const char *>(buf);

	const char *bom = GetUTFEncodingFromBOM(buf, len, allowUTF7);
	if (bom) return bom;
	bom = GetUTFEncodingFromText(buf, len);
	if (bom) return bom;

	if (len < 11)
	{
		// 11 bytes needed for @charset declaration
		return NULL;
	}

	// assume ASCII-compatible, and take it from there
	// No characters are allowed to precede @charset
	if (!strni_eq(buffer, "@CHARSET", 8))
	{
		return NULL; // no @charset, so no info
	}

	const char *bufend = buffer + len;
	buffer += 8;

	// Find string delimiter (skipping any unwanted characters)
	while (buffer < bufend &&
		*buffer != '\'' && *buffer != '"' && *buffer != ';')
	{
		buffer ++;
	}

	if (buffer==bufend || *buffer==';')
	{
		return NULL;
	}

	char delim = *buffer;

	buffer ++;
	const char *start = buffer;

	buffer = scan_to(delim, buffer, bufend);
	if (buffer == bufend)
	{
		return NULL;
	}

	unsigned int enclen = buffer - start;
	char *enc = reinterpret_cast<char *>(g_memory_manager->GetTempBuf());
	if (enclen >= g_memory_manager->GetTempBufLen())
	{
		enclen = g_memory_manager->GetTempBufLen() - 1;
	}

	op_strncpy(enc, start, enclen);
	enc[enclen]= 0x00;

	return enc;
}

const char *CharsetDetector::GetHTMLEncoding(const void *buf, unsigned long len, BOOL allowUTF7 /* = FALSE */)
{
	/* The following implementation tries to follow section 8.2.2.1 of the
	 * HTML5 specification as found on the following webpage:
	 * http://www.w3.org/html/wg/html5/#determining (as of 2008-05-23)
	 *
	 * This code only concerns itself with steps 3 and 4 in the algorithm,
	 * assuming that the surrounding steps are implemented by the code
	 * surrounding the call to this method. Note that the "confidence"
	 * levels mentioned in the spec are not implemented here.
	 *
	 * In the following code, numbered comments indicate references to the
	 * corresponding steps of the algorithm in section 8.2.2.1 of HTML5.
	 */
// Enable the following to follow HTML5 as strictly as possible.
// This will make the us less successful at detecting charsets in corner cases.
// #define STRICT_HTML5

	// 3. Unicode BOM matching.
	const char *utf = GetUTFEncodingFromBOM(buf, len, allowUTF7);
	if (utf) return utf;

#ifndef STRICT_HTML5
	// Non-standard addition: Easily detect UTF-16 from initial bytes of text
	utf = GetUTFEncodingFromText(buf, len, '<');
	if (utf) return utf;
#endif // STRICT_HTML5


	const char *buffer = reinterpret_cast<const char *>(buf);
	const char *bufend =
#ifdef ENCODINGS_FAST_AUTODETECTION
		buffer + MIN(len, ENCODINGS_BYTES_TO_PEEK);
#else
		buffer + len;
#endif

	bool got_pragma = false, need_pragma = true;

#define BUFFER_LEN (bufend - buffer)
	// 4. Start looking at HTML
	while (BUFFER_LEN > 0)
	{
		// 4.1. Examine stuff starting with '<'
		// Find first '<'
		const char *p = reinterpret_cast<const char *>(op_memchr(buffer, '<', BUFFER_LEN));
		if (!p) break; // No '<' found, cannot deduce charset, give up.
		buffer = p; // Buffer points to first '<'

		// 4.1.1. HTML comment (starts with '<!--', ends with '-->')
		if (op_memcmp(buffer, "<!--", MIN(4, BUFFER_LEN)) == 0)
		{
			if (BUFFER_LEN < 4) break; // Need more data
			// We found start of HTML comment. Skip to after '-->'
			do
			{
				p = reinterpret_cast<const char *>(op_memchr(buffer, '>', BUFFER_LEN));
				if (!p) break; // Need more data
				OP_ASSERT(*p == '>');
				buffer = p + 1; // Move buffer to after '>'
			} while (op_memcmp(p - 2, "-->", 3) != 0); // Until we find '-->'
			if (!p) break; // Need more data, propagate break
			OP_ASSERT(op_memcmp(p - 2, "-->", 3) == 0); // We found '-->'
			OP_ASSERT(buffer == p + 1); // buffer points to after '-->'
		}
		// 4.1.2. META tag (case-insensitive '<META' followed by space or slash)
		else if (strni_eq(buffer, "<META", MIN(5, BUFFER_LEN)))
		{
			if (BUFFER_LEN < 6) break; // Need more data
			buffer += 5;
			if (!op_isspace(*buffer) && *buffer != '/') goto non_match;
			// '<META' is indeed followed by space of slash
			const char   *attrname = NULL, *attrvalue = NULL, *charset = NULL;
			unsigned long attrname_len = 0, attrvalue_len = 0, charset_len = 0;
			while (BUFFER_LEN > 0)
			{
				// 4.1.2.2. Get an attribute and its value
				buffer = get_an_attribute(buffer, BUFFER_LEN, &attrname, &attrname_len, &attrvalue, &attrvalue_len);
				if (BUFFER_LEN == 0) break; // Need more data
				// 4.1.2.2. If no attribute found bail out of inner loop
				if (attrname_len == 0) goto non_match;
				// We have successfully parsed an attribute
				OP_ASSERT(BUFFER_LEN > 0);
				OP_ASSERT(attrname >= buf && attrname < buffer);
				OP_ASSERT(attrname_len > 0);

				/* 4.1.2.3. If neither 'charset', 'content' nor 'http-equiv',
				 * skip to next attribute. */
				/* 4.1.2.4. If 'charset', attribute value holds detected
				 * encoding. Set "need pragma" to false. */
				if (strni_eq(attrname, "CHARSET", attrname_len))
				{
					charset = attrvalue;
					charset_len = attrvalue_len;
					need_pragma = false;
				}
				/* 4.1.2.5. If 'content', parse attribute value to get detected
				 * encoding. Set "need pragma" to true (done outside the loop). */
				else if (strni_eq(attrname, "CONTENT", attrname_len) && attrvalue_len)
				{
					OP_ASSERT(attrvalue);
					p = get_encoding_from_content_type(attrvalue, attrvalue_len, &charset, &charset_len);
					OP_ASSERT(p);
				}
				/* 4.1.2.6. If 'http-equiv', if the attribute's value is
				 * "content-type", set "got pragma" to true. */
				else if (strni_eq(attrname, "HTTP-EQUIV", attrname_len) && attrvalue_len)
				{
					OP_ASSERT(attrvalue);
					/* Remove leading and trailing whitespace from the
					 * attribute value. This is not specified anywhere,
					 * but is needed for interoperability. */
					while (attrvalue_len && op_isspace(*attrvalue))
						++ attrvalue, -- attrvalue_len;
					while (attrvalue_len && op_isspace(attrvalue[attrvalue_len - 1]))
						-- attrvalue_len;
					if (strni_eq(attrvalue, "CONTENT-TYPE", attrvalue_len))
						got_pragma = true;
				}

				/* Check if we found a possible charset, and have the
				 * "got pragma" and "need pragma" flags in order. */
				if (charset && (got_pragma || !need_pragma))
				{
					OP_ASSERT(charset_len > 0);
#ifdef STRICT_HTML5
					// CharsetManager::GetCanonicalCharsetName() strips all non-alphanumerics from
					// given charset before canonizing it. This conflicts with the following note
					// from below step 4.1.2.8 in the spec: "Note: Leading and trailing spaces in
					// encoding names are not trimmed, and will result in encodings being treated
					// as unknown."
					// Therefore we detect leading/trailing non-alphanumerics here and disable
					// detection, if found.
					if (!op_isalnum(charset[0]) || !op_isalnum(charset[charset_len - 1]))
					{
						charset = NULL;
						charset_len = 0;
					}
				}
				if (charset) { // Will now fail if we found leading/trailing whitespace
#endif // STRICT_HTML5
					const char *canonical_encoding = g_charsetManager->GetCanonicalCharsetName(charset, charset_len);
					if (canonical_encoding) { // We found a valid charset
						// 4.1.2.6. If charset is a UTF-16 encoding, change it to UTF-8.
						if (op_memcmp(canonical_encoding, "utf-16", 6) == 0)
						{
							canonical_encoding = g_charsetManager->GetCanonicalCharsetName("utf-8");
						}
						// 4.1.2.7. If charset is a supported character encoding, then return the given encoding
						return canonical_encoding;
					}
				}
				// 4.1.2.8. Otherwise, return to 4.1.2.2.
			}
			if (BUFFER_LEN == 0) break; // Need more data, propagate break
		}
		// 4.1.3. Any other regular tag ('<' optionally followed by '/' followed by an ASCII letter)
#define IS_ASCII_LETTER(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z'))
		else if ((BUFFER_LEN > 2 && buffer[1] == '/' && IS_ASCII_LETTER(buffer[2])) || (BUFFER_LEN > 1 && IS_ASCII_LETTER(buffer[1])))
		{
#undef IS_ASCII_LETTER
			OP_ASSERT(*buffer == '<');
			// 4.1.3.1. Advance position to next space or '>'
			while (BUFFER_LEN > 0 && !op_isspace(*buffer) && *buffer != '>') buffer++;
			// 4.1.3.2. Repeatedly get an attribute until no attributes can be found
			const char   *attrname = NULL, *attrvalue = NULL;
			unsigned long attrname_len = 0, attrvalue_len = 0;
			do
			{
				buffer = get_an_attribute(buffer, BUFFER_LEN, &attrname, &attrname_len, &attrvalue, &attrvalue_len);
			} while (BUFFER_LEN > 0 && attrname_len);
			if (BUFFER_LEN == 0) break; // Need more data
		}
		// 4.1.4. Any (non-)tag starting with '<!', '</' or '<?'
		else if (BUFFER_LEN > 1 && (buffer[1] == '!' || buffer[1] == '/' || buffer[1] == '?'))
		{
			// Advance the position to first '>'
			p = reinterpret_cast<const char *>(op_memchr(buffer, '>', BUFFER_LEN));
			if (!p) break; // Need more data
			OP_ASSERT(*p == '>');
			buffer = p + 1; // Move buffer to after '>'
			OP_ASSERT(BUFFER_LEN >= 0);
		}
		// 4.1.5. Anything else
		// 4.2. Repeat 4.1 with next byte in input stream
		else
		{
non_match:		// Did not match anything at the '<' pointed at by buffer
			// If buffer still points at first '<', then increment
			// buffer before resuming search
			if (*p == '<' && buffer == p) buffer++;
		}
	}
#undef BUFFER_LEN

	// No luck there. Some misguided people send XML as HTML, and if so,
	// we probably should check if there's an XML encoding declaration if
	// there wasn't anything in the HTML. (Bug #118957)
	return GetXMLEncoding(buf, len);
}

const char *CharsetDetector::GetUTFEncodingFromBOM(const void *buf, unsigned long len, BOOL allowUTF7 /* = FALSE */)
{
	// Check for BOMs
	switch (MIN(len, 5))
	{
	case 5:
# ifdef ENCODINGS_HAVE_UTF7
		if (StartsWithUTF7BOM(buf) && allowUTF7)
			return "utf-7";
# endif
	case 4:
	case 3:
		if (StartsWithUTF8BOM(buf))
			return "utf-8";
	case 2:
		if (StartsWithUTF16BOM(buf))
			return "utf-16";
	}

	return NULL;
}

const char *CharsetDetector::GetUTFEncodingFromText(const void *buf, unsigned long len, char initial)
{
	const char *buffer = reinterpret_cast<const char *>(buf);

	// Look for signs of UTF-16
	if (len >= 4)
	{
		if (0 == buffer[0] && (initial ? initial == buffer[1] : 0 != buffer[1]) && 0 == buffer[2] && 0 != buffer[3])
		{
			return "utf-16be";
		}
		if ((initial ? initial == buffer[0] : 0 != buffer[0]) && 0 == buffer[1] && 0 != buffer[2] && 0 == buffer[3])
		{
			return "utf-16le";
		}
	}

	return NULL;
}

#ifdef PREFS_HAS_LNG
const char *CharsetDetector::GetLanguageFileEncoding(const void *buf, unsigned long len, BOOL allowUTF7 /* = FALSE */)
{
	const char *buffer = reinterpret_cast<const char *>(buf);
	const char *bufend = buffer + len;

	const char *bom = GetUTFEncodingFromBOM(buf, len, allowUTF7);
	if (bom) return bom;

	if (len < 10)
	{
		return NULL;
	}

	// assume ASCII-compatible, and take it from there
	while (buffer < bufend - 10)
	{
		if (('\n' == *buffer || '\r' == *buffer) &&
		    strni_eq(buffer + 1, "CHARSET=", 8))
		{
			buffer = buffer + 8 + 1;
			while (op_isspace(*reinterpret_cast<const unsigned char *>(buffer)) && buffer < bufend)
				buffer ++;
			if ('"' == *buffer) buffer ++;

			// Scan to end of line, space or quote
			const char *strend = buffer;
			while (strend < bufend && '\n' != *strend &&
			       '\r' != *strend && '"'  != *strend &&
				   !op_isspace(*reinterpret_cast<const unsigned char *>(strend)))
			{
				strend ++;
			}

			unsigned int namelen = strend - buffer;
			char *encoding = reinterpret_cast<char *>(g_memory_manager->GetTempBuf());
			if (namelen >= g_memory_manager->GetTempBufLen())
				namelen = g_memory_manager->GetTempBufLen() - 1;
			op_strncpy(encoding, buffer, namelen);
			encoding[namelen] = 0;
			return encoding;
		}
		buffer ++;
	}

	return NULL;
}
#endif

// Public methods of CharsetDetector ----------------------------------------

void CharsetDetector::PeekAtBuffer(const void *buf, unsigned long len)
{
	// Don't do anything if we already have made up our mind.

#ifdef ENCODINGS_FAST_AUTODETECTION
	// ...or if we have already parsed quite a lot of data without finding
	// any charset.
	if (peek == ENCODINGS_BYTES_TO_PEEK)
	{
		// Check for BOM and terminate immediately if there is one, which
		// means that we can remove the BOM checks below.
		result = GetUTFEncodingFromBOM(buf, len, m_allowUTF7);
	}

	if (result || !peek)
	{
		return;
	}

	if (len > peek)
	{
		len = peek;
	}
#else
	if (result)
		return;
#endif

	for (const unsigned char * pos = reinterpret_cast<const unsigned char *>(buf);
		 len; pos ++, len --)
	{
		// Seems to save a significant amount of CPU, actually, at
		// least on X86.
		register unsigned char cur = *pos;

		// UTF -----------------------------------------------------------
		// Check for BOM (byte-order mark) in UTF-7, UTF-8 and UTF-16.
		// This is normally done in other places as well, but since
		// we might get false alarms if we did not run it here, we do it
		// here as well.
		//
#ifndef ENCODINGS_FAST_AUTODETECTION
		// Run state machine:
		switch (utfbomstate)
		{
		case utf_bom_none: // initial
			switch (cur)
			{
			case 0xFE: // First byte of UTF-16 BE BOM
				utfbomstate = utf16be_bom_second; break;
			case 0xFF: // First byte of UTF-16 LE BOM
				utfbomstate = utf16le_bom_second; break;
			case 0xEF: // First byte of UTF-8 BOM
				utfbomstate = utf8_bom_second; break;
#ifdef ENCODINGS_HAVE_UTF7
			case '+': // First byte of UTF-7 BOM
				if (m_allowUTF7)
				{
					utfbomstate = utf7_bom_second; break;
				}
				// else fall through to default case
#endif
			default: // Unrecognized
				utfbomstate = utf_bom_invalid; break;
			}
			break;

		// UTF-16 LE ---
		case utf16le_bom_second: // expecting second byte of UTF-16 LE BOM
			if (0xFE == cur)
			{
				result = "utf-16le";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

		// UTF-16 BE ---
		case utf16be_bom_second: // expecting second byte of UTF-16 BE BOM
			if (0xFF == cur)
			{
				result = "utf-16be";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

		// UTF-8 ---
		case utf8_bom_second: // expecting second byte of UTF-8 BOM
			if (0xBB == cur)
			{
				utfbomstate = utf8_bom_third;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;
		case utf8_bom_third: // expecting third byte of UTF-8 BOM
			if (0xBF == cur)
			{
				result = "utf-8";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

#ifdef ENCODINGS_HAVE_UTF7
		// UTF-7 ---
		case utf7_bom_second:
			if ('/' == cur && m_allowUTF7)
			{
				utfbomstate = utf7_bom_third;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

		case utf7_bom_third:
			if ('v' == cur && m_allowUTF7)
			{
				utfbomstate = utf7_bom_fourth;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

		case utf7_bom_fourth:
			if ('8' == cur && m_allowUTF7)
			{
				utfbomstate = utf7_bom_fifth;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;

		case utf7_bom_fifth:
			if ('-' == cur && m_allowUTF7)
			{
				result = "utf-7";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				utfbomstate = utf_bom_invalid;
			}
			break;
#endif

		default: break; // nothing to do for invalid cases
		}
#endif // ENCODINGS_FAST_AUTODETECTION

#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
		// ISO-2022-JP/CN -------------------------------------------------
		// If we find one of ISO-2022-JP/CN's <ESC> sequences, we can be sure
		// that we are dealing with an ISO-2022-JP/CN document.
		//
		// Run state machine:
		switch (iso2022state)
		{
		case iso2022_none: // initial
			if (0x1B == cur)
			{
				// <ESC> begins a ISO-2022 sequence
				iso2022state = iso2022_esc;
			}
			else if (cur & 0x80)
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc: // <ESC>
			if ('(' == cur)
			{
				// <ESC>( begins single-byte identifier
				iso2022state = iso2022_esc_paren;
			}
			else if ('$' == cur)
			{
				// <ESC>$ begins double-byte identifier
				iso2022state = iso2022_esc_dollar;
			}
			else
			{
				// Anything else is unacceptable
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc_paren: // <ESC>(
			// <ESC>(B and <ESC>(J are sure signs of ISO-2022-JP
			if ('B' == cur || 'J' == cur)
			{
				result = "iso-2022-jp";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc_dollar: // <ESC>$
			// <ESC>$B and <ESC>$@ are sure signs of ISO-2022-JP
			if ('B' == cur || '@' == cur)
			{
				result = "iso-2022-jp";
				// We do not get out of this state/enter again
				return;
			}
			else if (')' == cur)
			{
				// <ESC>$) begins SO assign in ISO-2022-CN and -KR
				iso2022state = iso2022_esc_dollar_rightparen;
			}
			else if ('(' == cur)
			{
				// <ESC>$( begins G0 character set in ISO-2022-JP-1 and -2
				iso2022state = iso2022_esc_dollar_leftparen;
			}
			else if ('*' == cur)
			{
				// <ESC>$* begins SS2 assign in ISO-2022-CN
				iso2022state = iso2022_esc_dollar_asterisk;
			}
			else
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc_dollar_rightparen: // <ESC>$)
			// <ESC>$)A and <ESC>$)G are sure signs of ISO-2022-CN
			if ('A' == cur || 'G' == cur)
			{
				result = "iso-2022-cn";
				// We do not get out of this state/enter again
				return;
			}
			// <ESC>$)C is a sure sign of ISO-2022-KR
			else if ('C' == cur)
			{
				result = "iso-2022-kr";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc_dollar_leftparen: // <ESC>$(
			// <ESC>$(D is a sure sign of ISO-2022-JP-1 or -2
			if ('D' == cur)
			{
				result = "iso-2022-jp-1";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		case iso2022_esc_dollar_asterisk:
			// <ESC>$*H is a sure sign of ISO-2022-CN
			if ('H' == cur)
			{
				result = "iso-2022-cn";
				// We do not get out of this state/enter again
				return;
			}
			else
			{
				// Invalid
				iso2022state = iso2022_invalid;
			}
			break;

		default: break; // no handling for invalid states.
		}
#endif

#ifdef ENCODINGS_HAVE_CHINESE
		if (chinese == autodetect)
		{
			// Big5 ------------------------------------------------------
			// Big5 follows a pretty consistent pattern. The only bytes
			// with the high bit set are in the range A1..F9 (81..FE if we
			// include user-defined characters), and they should always be
			// followed by bytes in the 40..7E or A1..FE range. The 40..7E
			// range is what distinguishes Big5. However, 81..FE followed by
			// 40..7E is also a common sequence in iso-8859 encoded pages,
			// which why this can not be enabled at all times.
			//
			// Run state machine:
			switch (big5state)
			{
			case big5_none: // initial
				if (IN_RANGE(cur, 0xA1, 0xF9))
				{
					// Remember that this is the first part of a possible
					// Big5 code, so that we do not apply the same test on
					// the second byte, because it is likely to fail in the
					// second test if it is followed by HTML code.
					big5state = big5_second;
				}
				else
				{
					big5_consecutive = 0;
				}
				break;

			case big5_second: // expecting Big5 trail byte
				if (IN_RANGE(cur, 0x40, 0x7E))
				{
					big5_hints ++;
					big5_consecutive ++;
					if (big5_consecutive > 2)
					{
						has_5_big5_consecutive = true;
					}
				}
				else if (!IN_RANGE(cur, 0xA1, 0xFE))
				{
					// This is inconsistent with big5, so we give ourselves
					// a penalty
					big5_hints = big5_penalty;
					big5_consecutive = 0;
				}

				big5state = big5_none;
				break;
			}

			// HZ --------------------------------------------------------
			// HZ encoding is distinguished by its escape sequences
			// starting with a tilde. Since HZ sequences must be closed at
			// least at end or document, or to contain ASCII, we only
			// count when we find a valid HZ string with both opening and
			// closing escape sequences. If we find eight-bit data it is
			// very unlikely that this is HZ.
			//
			// Run state machine:
			switch (hzstate)
			{
			case hz_none: // initial
				if ('~' == cur)
				{
					// ~ initiates a HZ shift sequence
					hzstate = hz_lead_sbcs;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
				}
				break;

			case hz_lead_sbcs: // shift sequence leader found from SBCS
				if ('~' == cur)
				{
					// Double tilde, while being a valid HZ sequence, is not
					// counted for hints
					hzstate = hz_none;
				}
				else if ('{' == cur)
				{
					// "~{" = enter two byte mode
					hzstate = hz_first;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
					hzstate = hz_none;
				}
				else
				{
					// Unused HZ escape sequence
					hz_hints = -100;
					hzstate = hz_none;
				}
				break;

			case hz_lead_dbcs: // shift sequence leader found from MBCS
				if ('}' == cur)
				{
					// "~}" = enter one byte mode
					hzstate = hz_none;
					hz_hints ++;
				}
				else if (13 == cur)
				{
					// "~\n" = line-continuation
					hzstate = hz_cr;
				}
				else if (10 == cur)
				{
					// "~\n" = line-continuation
					hzstate = hz_lf;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
					hzstate = hz_none;
				}
				else
				{
					// Unused HZ escape/encode sequence
					hz_hints = -100;
					hzstate = hz_second;
				}
				break;

			case hz_first: // expecting lead byte
				if ('~' == cur)
				{
					// ~ initiates a HZ shift sequence
					hzstate = hz_lead_dbcs;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
					hzstate = hz_second;
				}
				else
				{
					hzstate = hz_second;
				}
				break;

			case hz_second: // expecting trail byte
				if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
				}
				hzstate = hz_first;
				break;

			case hz_cr: // ~\r, expecting \n or DBCS lead
				if (10 == cur)
				{
					// ~\r\n - next is DBCS lead
					hzstate = hz_first;
				}
				else if ('~' == cur)
				{
					// ~ initiates a HZ shift sequence
					hzstate = hz_lead_dbcs;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
					hzstate = hz_second;
				}
				else
				{
					// This was DBCS lead
					hzstate = hz_second;
				}
				break;

			case hz_lf: // ~\n, expecting \r or DBCS lead
				if (13 == cur)
				{
					// ~\n\r - next is DBCS lead
					hzstate = hz_first;
				}
				else if ('~' == cur)
				{
					// ~ initiates a HZ shift sequence
					hzstate = hz_lead_dbcs;
				}
				else if (cur > 0x7F)
				{
					// HZ does not use high-bit characters
					hz_hints = -100;
					hzstate = hz_second;
				}
				else
				{
					// This was DBCS lead
					hzstate = hz_second;
				}
				break;
			}
		}
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
		// Shift-JIS and EUC-JP are easily distinguishible, but trying to
		// detect them might give false alarm for other pages, so we don't
		// try to detect them unless asked to
		if (japanese == autodetect)
		{
			// Shift-JIS -------------------------------------------------
			// Check for valid and invalid Shift-JIS sequences
			//
			// Run state machine:
			switch (sjisstate)
			{
			case sjis_none: // initial
				if (IN_RANGE(cur, 0xA1, 0xDF))
				{
# ifndef IMODE_EXTENSIONS
					// Halfwidth katakana are counted, because if they are
					// very numerous, we probably have a EUC-JP page, not a
					// Shift-JIS page. (Halfwidth katakana is not very common
					// in documents).
					sjis_halfwidth_katakana ++;
# endif
					sjis_consecutive = 0;
				}
				else if (cur > 0x80 &&
						(IN_RANGE(cur, 0x81, 0x9F) ||
				         IN_RANGE(cur, 0xE0, 0xEE) ||
# ifdef IMODE_EXTENSIONS
				         IN_RANGE(cur, 0xF8, 0xFC)
# else
				         IN_RANGE(cur, 0xFA, 0xFC)
# endif
				                                   ))
				{
					// First byte of a double-byte sequence
					sjisstate = sjis_second;
				}
				else
				{
					sjis_consecutive = 0;
				}
				break;

			case sjis_second: // expecting Shift-JIS trail byte
				// Check if second byte is valid
				if (IN_RANGE(cur, 0x40, 0x7E) ||
				    IN_RANGE(cur, 0x80, 0xFC))
				{
					sjis_valid ++;
					sjis_consecutive ++;
					if (sjis_consecutive > 4)
					{
						has_5_sjis_consecutive = true;
					}
				}
				else
				{
					sjis_invalid ++;
					sjis_consecutive = 0;
				}
				sjisstate = sjis_none;
				break;
			}
		}
#endif

#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
		switch (autodetect) // Should be an if, really
		{
# ifdef ENCODINGS_HAVE_JAPANESE
		case japanese:
# endif
# ifdef ENCODINGS_HAVE_CHINESE
		case chinese:
# endif
# ifdef ENCODINGS_HAVE_KOREAN
		case korean:
# endif
			// EUC -------------------------------------------------------
			// Check for invalid EUC-(CN|JP|TW|KR),GBK sequences
			// If testing Japanese, we test for EUC-JP pecularities
			// If testing Chinese, we test for EUC-TW and GBK pecularities
			// If testing Korean, we only test two-byte EUC-KR
			//
			// Run state machine:
			switch (eucstate)
			{
			case euc_none: // initial
				if (IN_RANGE(cur, 0xA1, 0xFE))
				{
					// First byte valid
					eucstate = euc_second;
				}
# ifdef ENCODINGS_HAVE_CHINESE
				else if (chinese == autodetect)
				{
					if (0x8E == cur)
					{
						eucstate = euctw_plane_or_gbk_second;
					}
					else if (IN_RANGE(cur, 0x81, 0xFE))
					{
						// GBK encoding is a superset of EUC-CN, which is
						// very similar to EUC-TW. The problem that it, just
						// like EUC-TW, uses the value 142 in its encoding.
						// It does, however, use several more bytes as well,
						// so we just check for those bytes.
						eucstate = gbk_second;
					}
					else
					{
						euc_consecutive = 0;
					}
				}
# endif
# ifdef ENCODINGS_HAVE_JAPANESE
				else if (japanese == autodetect)
				{
					if (0x8E == cur)
					{
						// Lead byte for half-width katakana. Not much used,
						// but valid.
						eucstate = eucjp_katakana;
					}
					else if (0x8F == cur)
					{
						// Code set 3 (three-byte code). Not much used, but
						// valid.
						eucstate = euc_first;
					}
					else
					{
						euc_consecutive = 0;
					}
				}
# endif
				else {
					switch (cur)
					{
					// Exception for some ASCII characters that commonly occur in
					// short text segments (e.g. filenames, email addresses, etc.)
					case ' ':
					case '-':
					case '.':
					case '/':
					case ':':
					case ';':
					case '@':
					case '\\':
					case '_':
						break;
					// Otherwise reset the EUC consecutive count to 0
					default:
						euc_consecutive = 0;
					}
				}
				break;

			case euc_first: // expecting EUC lead byte
				if (IN_RANGE(cur, 0xA1, 0xFE))
				{
					// First byte valid
					eucstate = euc_second;
				}
				else
				{
					// First byte invalid
					eucstate = euc_none;
					euc_invalid += euc_penalty;
					euc_consecutive = 0;
				}
				break;

			case euc_second: // expecting EUC trail byte
				if (IN_RANGE(cur, 0xA1, 0xFE))
				{
					// Trail byte valid EUC
					euc_valid ++;
					euc_consecutive ++;
					if (euc_consecutive > 4)
					{
						has_5_euc_consecutive = true;
					}
					eucstate = euc_none;
				}
# ifdef ENCODINGS_HAVE_CHINESE
				else if (chinese == autodetect &&
				         ((IN_RANGE(cur, 0x40, 0x7E)) ||
						  (IN_RANGE(cur, 0x80, 0xA0))))
				{
					// Trail byte valid GBK, but we don't count it.
					eucstate = euc_none;

					// Since this is invalid for regular EUC-CN, we add
					// a single penalty point (100/64=1, 1/64=0) unless
					// we're looking for simplified Chinese.
					euc_invalid += (euc_penalty >> 6);
				}
				else if (chinese == autodetect && (IN_RANGE(cur, 0x30, 0x39)))
				{
					// Second byte valid for GB18030
					eucstate = gb18030_third;
				}
# endif
				else
				{
					euc_invalid += euc_penalty;
					euc_consecutive = 0;
					eucstate = euc_none;
				}
				break;

			case gbk_second: // expecting GBK trail byte
				if ((IN_RANGE(cur, 0x40, 0x7E)) ||
				    (IN_RANGE(cur, 0x80, 0xFE)))
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Trail byte valid GBK.
					gbk_hints ++;
# endif
					eucstate = euc_none;
				}
				else if (IN_RANGE(cur, 0x30, 0x39))
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Second byte valid for GB18030
					eucstate = gb18030_third;
# endif
				}
				else
				{
					euc_invalid += euc_penalty;
					eucstate = euc_none;
				}
				break;

			case euctw_plane_or_gbk_second: // expecting EUC-TW plane indicator
			                                // or GBK trail byte
				// EUC-TW encoding can be differentiated from EUC-CN
				// (GB-2312) encoding by its four-byte sequences, which
				// always start with the code 0x8E. Everything else is
				// compliant with EUC-CN. However, 0x8E is also a valid
				// lead byte in GBK, so this can also be the second byte
				// in a GBK sequence.
				if (IN_RANGE(cur, 0xA1, 0xB0))
				{
					// Next byte is either a EUC-TW lead byte (which would
					// count towards EUC-TW) or anything in GBK. Having
					// this in its own state lets us catch a GBK character
					// looks like a EUC-TW plane indicator followed by
					// HTML code, which is valid GBK, but cannot be valid
					// EUC-TW.
					eucstate = euctw_lead_or_gbk_anything;
				}
				else if (IN_RANGE(cur, 0x40, 0x7E) ||
				         IN_RANGE(cur, 0x80, 0xFE))
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Trail byte valid GBK, but not valid EUC-TW.
					euctw_hints = -100;
					gbk_hints ++;
# endif
					eucstate = euc_none;

					// TODO: Should this count towards euc_valid ?
				}
				else if (IN_RANGE(cur, 0x30, 0x39))
				{
					// Second byte valid for GB18030
					eucstate = gb18030_third;
				}
				else
				{
					// Invalid in both EUC-TW and GBK.
					euc_invalid += euc_penalty;
					eucstate = euc_none;
				}
				break;

			case euctw_lead_or_gbk_anything: // expecting EUC-TW lead byte or
				                             // anything in GBK
				if (IN_RANGE(cur, 0xA1, 0xFE))
				{
					// This is followed by an EUC trail byte
					eucstate = euctw_or_gbk_second;
				}
				else if (IN_RANGE(cur, 0x81, 0xA0))
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// This is a valid GBK lead byte, but not EUC-TW, so this
					// must be GBK, not EUC-TW.
					euctw_hints = -100;
					gbk_hints ++;
# endif

					eucstate = gbk_second;
				}
				else
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// This is a valid GBK single-byte character, but is not
					// a valid EUC-TW character, since we were expecting a
					// EUC double-byte after the plane indicator.
					euctw_hints = -100;
					gbk_hints ++;
# endif

					eucstate = euc_none;
				}
				break;

			case euctw_or_gbk_second: // expecting EUC-TW or GBK trail byte
				if (IN_RANGE(cur, 0xA1, 0xFE))
				{
					// This is valid in both EUC-TW and GBK
					euc_valid ++;
# ifdef ENCODINGS_HAVE_CHINESE
					euctw_hints ++;
					gbk_hints ++;
# endif
				}
# ifdef ENCODINGS_HAVE_CHINESE
				else if (IN_RANGE(cur, 0x40, 0x7E) ||
				         IN_RANGE(cur, 0x80, 0xFE))
				{
					// Trail byte valid GBK, but not EUC-TW.
					gbk_hints ++;
					euctw_hints = -100;
				}
				else
				{
					// This is neither valid GBK, nor EUC-TW.
					gbk_hints = -100;
					euctw_hints = -100;
				}
# endif
				eucstate = euc_none;
				break;


			case eucjp_katakana: // expecting EUC-JP halfwidth katakana
				if (!IN_RANGE(cur, 0xA1, 0xDF))
				{
					// Not a valid halfwidth katakana
					euc_invalid += euc_penalty;
				}
				eucstate = euc_none;
				break;

			case gb18030_third:
				if (IN_RANGE(cur, 0x81, 0xFE))
				{
					// Valid third byte of a GB18030 sequence
					eucstate = gb18030_fourth;
				}
				else
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Inconsistent with GB18030
					gb18030_hints -= 10;
# endif
					euc_consecutive = 0;
					eucstate = euc_none;
				}
				break;

			case gb18030_fourth:
				if (IN_RANGE(cur , 0x30, 0x39))
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Valid fourth byte of a GB18030 sequence
					gb18030_hints ++;
					euc_consecutive ++;
# endif
					if (euc_consecutive > 4)
					{
						has_5_euc_consecutive = true;
					}
				}
				else
				{
# ifdef ENCODINGS_HAVE_CHINESE
					// Inconsistent with GB18030
					gb18030_hints -= 10;
# endif
					euc_consecutive = 0;
				}
				eucstate = euc_none;
			}
		}
#endif

#ifdef ENCODINGS_HAVE_CYRILLIC
		// Cyrillic autodetection ----------------------------------------
		if (cyrillic == autodetect)
		{
			// We make three tests to find out the Cyrillic encoding:
			// 1. Look for the letters U+0430 (cyrillic small letter a),
			//    U+0438 (cyrillic small letter i) and U+043E (cyrillic
			//    small letter o) in isolation. These are Russian
			//    prepositions with different encodings.
			// 2. Look for words that starts with one case and has the
			//    rest of the characters in another case.
			switch (cyrillic_state)
			{
			case nonword:
				// Check for initial word character (ASCII or Cyrillic)
				if (cur >= 0x80 ||
				    IN_RANGE(cur, 'A', 'Z') || IN_RANGE(cur, 'a', 'z'))
				{
					// This is oversimplified, all characters over 0x80 are
					// word characters in some Cyrillic encoding, but not in
					// all
					cyrillic_state = firstletter;

					// Check for the Russian preposition letters
					probable_8859_5_preposition =       IN_SET3(cur, 0xD0, 0xD8, 0xDE);
					probable_koi_preposition =          IN_SET3(cur, 0xC1, 0xC9, 0xCF);
					probable_windows_1251_preposition = IN_SET3(cur, 0xE0, 0xE8, 0xEE);
					probable_ibm866_preposition =       IN_SET3(cur, 0xA0, 0xA8, 0xAE);

					// Check for valid uppercase letters in the encodings
					probable_8859_5_word =       IN_RANGE(cur, 0xB0, 0xCF);
					probable_koi_word =                  (cur >= 0xE0    );
					probable_windows_1251_word = IN_RANGE(cur, 0xC0, 0xDF);
					probable_ibm866_word =       IN_RANGE(cur, 0x80, 0x9F);

					// Note: 8859-5's real range is A1-CF, but A1-AF letters
					// are rare and cause collisions with windows-1251
				}

				break;

			case firstletter:
				if (cur < 'A' || (cur > 'Z' && cur < 'a') ||
				    (cur > 'z' && cur < 0x80))
				{
					cyrillic_state = nonword;
					if (probable_8859_5_preposition)
					{
						iso_8859_5_hints ++;
					}
					else if (probable_koi_preposition)
					{
						koi_hints ++;
					}
					else if (probable_windows_1251_preposition)
					{
						windows_1251_hints ++;
					}
					else if (probable_ibm866_preposition)
					{
						ibm866_hints ++;
					}

					break;
				}
				else
				{
					cyrillic_state = nonfirstletter;
				}
				/* Fall through */
			case nonfirstletter:
				if (cur < 'A' || (cur > 'Z' && cur < 'a') ||
				    (cur > 'z' && cur < 0x80))
				{
					cyrillic_state = nonword;

					// Check for word with initial uppercase letter
					if (probable_8859_5_word)
					{
						iso_8859_5_hints ++;
					}
					else if (probable_koi_word)
					{
						koi_hints ++;
					}
					else if (probable_windows_1251_word)
					{
						windows_1251_hints ++;
					}
					else if (probable_ibm866_word)
					{
						ibm866_hints ++;
					}
				}
				else
				{
					// Check for lowercase letters
					if (probable_8859_5_word)
					{
						probable_8859_5_word = IN_RANGE(cur, 0xD0, 0xEF);
						// Note: Real range is D0-FF, but F0-FF letters are rare
						// and cause collisions with windows-1251
					}
					if (probable_koi_word)
					{
						probable_koi_word = IN_RANGE(cur, 0xC0, 0xDF);
					}
					if (probable_windows_1251_word)
					{
						probable_windows_1251_word = (cur >= 0xE0);
					}
					if (probable_ibm866_word)
					{
						probable_ibm866_word = IN_RANGE(cur, 0xA0, 0xAF) ||
						                       IN_RANGE(cur, 0xE0, 0xF7);
					}
				}
				break;
			}

			// 3. Check for invalid characters. ISO 8859-5 has the area
			//    0x80-0x9F undefined, while the others use it for
			//    printable characters
			if (IN_RANGE(cur, 0x80, 0x9F))
			{
				iso_8859_5_invalid ++;
			}
		}
#endif

		// UTF-8 ---------------------------------------------------------
		// Check for UTF-8 valid and invalid sequences.
		//
		// Run state machine:
		if (utf8_length)
		{
			// We are inside an UTF-8 sequence
			if ((cur & 0xC0) == 0x80)
			{
				// Update state
				utf8_length --;

				// Count if we are done with this sequence
				utf8_valid += !utf8_length;
			}
			else
			{
				// Invalid UTF-8 data
				utf8_length = 0;
				utf8_invalid ++;
			}
		}
		else if (cur & 0x80)
		{
			// Test above for speed.
			if ((cur & 0xE0) == 0xC0)
			{
				// Two byte UTF-8 sequence; seed state machine
				utf8_length = 1;
			}
			else if ((cur & 0xF0) == 0xE0)
			{
				// Three byte UTF-8 sequence; seed state machine
				utf8_length = 2;
			}
			else if ((cur & 0xF8) == 0xF0)
			{
				// Four byte UTF-8 sequence; seed state machine
				utf8_length = 3;
			}
			else if ((cur & 0xFC) == 0xF8)
			{
				// Five byte UTF-8 sequence; seed state machine
				utf8_length = 4;
			}
			else if ((cur & 0xFE) == 0xFC)
			{
				// Six byte UTF-8 sequence; seed state machine
				utf8_length = 5;
			}
			else
			{
				// Invalid UTF-8 octet
				utf8_invalid ++;
			}
		}
	}

#ifdef ENCODINGS_FAST_AUTODETECTION
	peek -= len;
	if (!peek)
	{
		result = GetDetectedCharset();
	}
#endif // ENCODINGS_FAST_AUTODETECTION
}

const char *CharsetDetector::GetDetectedCharset()
{
	// If we are determined already, return what we found
	if (result)
	{
		return result;
	}

	// Otherwise, try to make sense of the results

	switch (autodetect)
	{
#ifdef ENCODINGS_HAVE_CHINESE
	// Chinese -----------------------------------------------------------
	case chinese:
		if (!utf8_invalid && utf8_valid &&
			utf8_valid > big5_hints &&
			utf8_valid > euc_invalid / euc_penalty)
		{
			// We have a page that has valid UTF-8, and its UTF-8 content
			// is higher than its Big5 or EUC content, so we can be
			// reasonably sure that this might actually be a UTF-8 page.
			return "utf-8";
		}
		else if (big5_hints > 0 && big5_hints >= gbk_hints && has_5_big5_consecutive)
		{
			// Big5 is returned if we find anything that is consistent with
			// Big5.
			return "big5";
		}
		else if (hz_hints > 0)
		{
			// Only select HZ if we found both a ~{ and a ~}
			return "hz-gb-2312";
		}
		else if (euc_valid > euc_invalid * 2 && has_5_euc_consecutive)
		{
			// Okay, so this is EUC encoding, the problem now is that it
			// can either be GB18030, GBK/EUC-CN (GB-2312) or EUC-TW.
			// The differing factors here are EUC-TW's and GB-18030's
			// four-byte sequences. EUC-TW's four-byte sequences always
			// start with the byte 142, and GB18030's four-byte sequences
			// are incompatible with both GBK and EUC-TW.
			if (euctw_hints > 0 && gbk_hints <= euctw_hints)
			{
				// Okay, at least one 142 byte was found, and the total
				// number of GBK lead bytes were less than (impossible) or
				// equal to that number, assume EUC-TW (although it could
				// also be GBK with only 142 lead-bytes, but that is not
				// very probable).
				return "euc-tw";
			}
			else if (gb18030_hints > 0)
			{
				// At least one GB18030 four-byte sequence was found, so
				// assume GB18030 (which is a superset of GBK).
				return "gb18030";
			}
			else
			{
				// This is either EUC-CN or GBK. GBK is a superset of EUC-CN,
				// so we simply say GBK here.
				return "gbk";
			}
		}
		break;
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
	// Japanese ----------------------------------------------------------
	case japanese:
		// The logic is like this:
		//  The encoding that has the least invalid sequences wins,
		//  plus that EUC wins if we have a very high amount of
		//  half-width kana in Shift-JIS, since they are seldom used
		//  and are more likely to be EUC codes (except in IMODE,
		//  where half-width kana is quite common)
		//  Also, we have tacked on a test for UTF-8 here, since UTF-8
		//  pages tend to get mis-identified otherwise.
		if (!utf8_invalid && utf8_valid &&
			utf8_valid > sjis_invalid &&
			utf8_valid > euc_invalid)
		{
			return "utf-8";
		}
		else if (euc_valid && (sjis_invalid > euc_invalid ||
# ifndef IMODE_EXTENSIONS
		                       sjis_halfwidth_katakana > sjis_valid ||
# endif
		                       !euc_invalid) && has_5_euc_consecutive)
		{
			return "euc-jp";
		}
		else if (sjis_valid && (euc_invalid > sjis_invalid || !sjis_invalid) &&
		         has_5_sjis_consecutive)
		{
			return "shift_jis";
		}
		break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	// Korean ------------------------------------------------------------
	case korean:
		// ISO-2022-KR will already have been detected, so the only
		// encodings left to check are UTF-8 and EUC-KR. If we find
		// enough evidence of a EUC-KR page, let's just say that it is
		// such.
		if (!utf8_invalid && utf8_valid && utf8_valid > euc_invalid)
		{
			return "utf-8";
		}
		else if (euc_valid > euc_invalid * 2 && has_5_euc_consecutive)
		{
			return "euc-kr";
		}
		break;
#endif

#ifdef ENCODINGS_HAVE_CYRILLIC
	// Cyrillic ----------------------------------------------------------
	case cyrillic:
		if (!utf8_invalid && utf8_valid)
		{
			// If any of the Cyrillic 8-bit encodings are used, we are
			// very likely to have invalid UTF-8 sequences
			return "utf-8";
		}

		// I'm assuming this priority of Cyrillic encodings:
		// 1. Windows-1251
		// 2. KOI8
		// 3. IBM 866
		// 4. ISO 8859-5
		if (!iso_8859_5_invalid &&
		    iso_8859_5_hints >= 5 &&
		    iso_8859_5_hints > windows_1251_hints &&
		    iso_8859_5_hints > koi_hints &&
		    iso_8859_5_hints > ibm866_hints)
		{
			return "iso-8859-5";
		}
		if (ibm866_hints >= 5 &&
		    ibm866_hints > windows_1251_hints &&
		    ibm866_hints > koi_hints)
		{
			return "ibm866";
		}
		if (koi_hints >= 5 &&
		    koi_hints > windows_1251_hints)
		{
			return "koi8-u"; // KOI8-U contains more letters than KOI8-R
		}
		if (windows_1251_hints >= 5)
		{
			return "windows-1251";
		}
		break;
#endif
	}

	// Other -------------------------------------------------------------
	// See if this might be UTF-8
	// TODO: This test is not perfect
	if (!utf8_invalid && utf8_valid > utf8_threshold)
	{
		return "utf-8";
	}

	// Nothing found
	return NULL;

}

// Private class methods of CharsetDetector ---------------------------------

const char *CharsetDetector::scan_to(char to, const char *buffer, const char *bufend)
{
	if (buffer < bufend) {
		const char *p = reinterpret_cast<const char *>(op_memchr(buffer, to, bufend - buffer));
		if (p) return p;
	}
	return bufend;
}

const char *CharsetDetector::scan_to(const char *token, const char *buffer, const char *bufend)
{
	size_t toklen = op_strlen(token);
	bufend -= toklen - 1;

	do
	{
		buffer = scan_to(token[0], buffer, bufend) + 1;
	} while (buffer < bufend && op_strncmp(buffer, token + 1, toklen - 1));

	if (buffer >= bufend)
	{
		return NULL;
	}
	else
	{
		return buffer + toklen - 1;
	}
}

/** @param uctoken Token to search for. Must be uppercase.
  * @param buffer  Start of buffer to scan.
  * @param bufend  End of buffer to scan.
  */
const char *CharsetDetector::scan_case_to(const char *uctoken, const char *buffer, const char *bufend)
{
	size_t toklen = op_strlen(uctoken);
	bufend -= toklen - 1;

	while (buffer < bufend && !strni_eq(buffer, uctoken, toklen))
	{
		buffer ++;
	}

	if (buffer >= bufend)
	{
		return NULL;
	}
	else
	{
		return buffer + toklen;
	}
}

const char *CharsetDetector::get_an_attribute(
	const char  *buf,       unsigned long  buf_len,
	const char **attrname,  unsigned long *attrname_len,
	const char **attrvalue, unsigned long *attrvalue_len)
{
	const char *buffer = buf; // Moved forwards as parsing progresses. Returned when parsing is finished.
	const char *bufend = buf + buf_len; // Returned when more data is needed to properly finish parsing.
#define BUFFER_LEN (bufend - buffer)

	// Reset output parameters
	*attrname  = NULL; *attrname_len  = 0;
	*attrvalue = NULL; *attrvalue_len = 0;

// From http://www.w3.org/html/wg/html5/#get-an (as of 2008-05-23)

//	1. If the byte at position is one of 0x09 (ASCII TAB), 0x0A (ASCII LF),
//	   0x0B (ASCII VT), 0x0C (ASCII FF), 0x0D (ASCII CR),
//	   0x20 (ASCII space), or 0x2F (ASCII '/') then advance position to the
//	   next byte and redo this substep.

	while (BUFFER_LEN > 0 && (op_isspace(*buffer) || *buffer == '/')) buffer++;

//	2. If the byte at position is 0x3E (ASCII '>'), then abort the "get an
//	   attribute" algorithm. There isn't one.

	if (BUFFER_LEN > 0 && *buffer == '>') return buffer;

//	3. Otherwise, the byte at position is the start of the attribute name.
//	   Let attribute name and attribute value be the empty string.

	*attrname = buffer;

//	4. Attribute name: Process the byte at position as follows:

	while (BUFFER_LEN > 0) {

//		- If it is 0x3D (ASCII '='), and the attribute name is longer
//		  than the empty string
//			Advance position to the next byte and jump to the step
//			below labelled [value].

		if (*buffer == '=' && buffer > *attrname) {
			*attrname_len = buffer - *attrname;
			buffer++;
			goto value;
		}

//		- If it is 0x09 (ASCII TAB), 0x0A (ASCII LF), 0x0B (ASCII VT),
//		  0x0C (ASCII FF), 0x0D (ASCII CR), or 0x20 (ASCII space)
//			Jump to the step below labelled [spaces].

		else if (op_isspace(*buffer)) {
			break; // goto spaces;
		}

//		- If it is 0x2F (ASCII '/') or 0x3E (ASCII '>')
//			Abort the "get an attribute" algorithm. The attribute's
//			name is the value of attribute name, its value is the
//			empty string.

		else if (*buffer == '/' || *buffer == '>') {
			*attrname_len = buffer - *attrname;
			return buffer;
		}

//		- If it is in the range 0x41 (ASCII 'A') to 0x5A (ASCII 'Z')
//			Append the Unicode character with codepoint b+0x20 to
//			attribute name (where b is the value of the byte at
//			position).
// ***			*** IMPORTANT: This step is disregarded in favour of
// ***			*** handling the attribute name case-insensitively.
//		- Anything else
//			Append the Unicode character with the same codepoint as
//			the value of the byte at position) to attribute name.
//			(It doesn't actually matter how bytes outside the ASCII
//			range are handled here, since only ASCII characters can
//			contribute to the detection of a character encoding.)
//
//	5. Advance position to the next byte and return to the previous step.

		buffer++;
	}

//	6. [Spaces]. If the byte at position is one of 0x09 (ASCII TAB),
//	   0x0A (ASCII LF), 0x0B (ASCII VT), 0x0C (ASCII FF), 0x0D (ASCII CR),
//	   or 0x20 (ASCII space) then advance position to the next byte, then,
//	   repeat this step.

// spaces:
	*attrname_len = buffer - *attrname; // End attribute name _before_ spaces
	while (BUFFER_LEN > 0 && op_isspace(*buffer)) buffer++;

//	7. If the byte at position is not 0x3D (ASCII '='), abort the "get an
//	   attribute" algorithm. The attribute's name is the value of attribute
//	   name, its value is the empty string.

	if (BUFFER_LEN > 0 && *buffer != '=') {
		return buffer;
	}

//	8. Advance position past the 0x3D (ASCII '=') byte.

	if (BUFFER_LEN > 0) {
		OP_ASSERT(*buffer == '=');
		buffer++;
	}

//	9. [Value]. If the byte at position is one of 0x09 (ASCII TAB),
//	   0x0A (ASCII LF), 0x0B (ASCII VT), 0x0C (ASCII FF), 0x0D (ASCII CR),
//	   or 0x20 (ASCII space) then advance position to the next byte, then,
//	   repeat this step.

value:
	while (BUFFER_LEN > 0 && op_isspace(*buffer)) buffer++;
	*attrvalue = buffer;

//	10. Process the byte at position as follows:
//		- If it is 0x22 (ASCII '"') or 0x27 ("'")
	if (BUFFER_LEN > 0 && (*buffer == '"' || *buffer == '\'')) {
//			1. Let b be the value of the byte at position.
		const char b = *buffer;
//			2. Advance position to the next byte.
		buffer++;
		*attrvalue = buffer;
		while (BUFFER_LEN > 0) {
//			3. If the value of the byte at position is the value of
//			   b, then advance position to the next byte and abort
//			   the "get an attribute" algorithm. The attribute's
//			   name is the value of attribute name, and its value
//			   is the value of attribute value.
			if (*buffer == b) {
				*attrvalue_len = buffer - *attrvalue;
				return ++buffer;
			}
//			4. Otherwise, if the value of the byte at position is
//			   in the range 0x41 (ASCII 'A') to 0x5A (ASCII 'Z'),
//			   then append a Unicode character to attribute value
//			   whose codepoint is 0x20 more than the value of the
//			   byte at position.
// ***			   *** IMPORTANT: This step is disregarded in favour of
// ***			   *** handling the attribute value case-insensitively.
//			5. Otherwise, append a Unicode character to attribute
//			   value whose codepoint is the same as the value of
//			   the byte at position.
//			6. Return to the second step in these substeps.
			buffer++;
		}
	}

//		- If it is 0x3E (ASCII '>')
//			Abort the "get an attribute" algorithm. The attribute's
//			name is the value of attribute name, its value is the
//			empty string.

	else if (BUFFER_LEN > 0 && *buffer == '>') return buffer;

//		- If it is in the range 0x41 (ASCII 'A') to 0x5A (ASCII 'Z')
//			Append the Unicode character with codepoint b+0x20 to
//			attribute value (where b is the value of the byte at
//			position). Advance position to the next byte.
// ***			*** IMPORTANT: This step is disregarded in favour of
// ***			*** handling the attribute value case-insensitively.
//		- Anything else
//			Append the Unicode character with the same codepoint as
//			the value of the byte at position) to attribute value.
//			Advance position to the next byte.

	else if (BUFFER_LEN > 0) buffer++;

//	11. Process the byte at position as follows:

	while (BUFFER_LEN > 0) {

//		- If it is 0x09 (ASCII TAB), 0x0A (ASCII LF), 0x0B (ASCII VT),
//		  0x0C (ASCII FF), 0x0D (ASCII CR), 0x20 (ASCII space),
//		  or 0x3E (ASCII '>')
//			Abort the "get an attribute" algorithm. The attribute's
//			name is the value of attribute name and its value is
//			the value of attribute value.

		if (op_isspace(*buffer) || *buffer == '>') {
			*attrvalue_len = buffer - *attrvalue;
			return buffer;
		}

//		- If it is in the range 0x41 (ASCII 'A') to 0x5A (ASCII 'Z')
//			Append the Unicode character with codepoint b+0x20 to
//			attribute value (where b is the value of the byte at
//			position).
//		- Anything else
//			Append the Unicode character with the same codepoint as
//			the value of the byte at position) to attribute value.
//
//	12. Advance position to the next byte and return to the previous step.

		buffer++;
	}

	OP_ASSERT(BUFFER_LEN == 0);
	return bufend; // Need more data.

#undef BUFFER_LEN
}

const char *CharsetDetector::get_encoding_from_content_type(
	const char  *buf,     unsigned long  buf_len,
	const char **charset, unsigned long *charset_len)
{
	const char *buffer = buf; // Moved forwards as parsing progresses. Returned when parsing is finished.
	const char *bufend = buf + buf_len; // Returned when more data may be needed to properly finish parsing.
#define BUFFER_LEN (bufend - buffer)

	// Reset output parameters
	*charset = NULL; *charset_len = 0;

// From http://www.w3.org/html/wg/html5/#algorithm3 (as of 2008-05-23)

//	1. Find the first seven characters in s that are a case-insensitive
//	   match for the word 'charset'. If no such match is found, return
//	   nothing.

	const char *p = reinterpret_cast<const char *>(op_memchr(buffer, 'c', BUFFER_LEN));
	const char *q = reinterpret_cast<const char *>(op_memchr(buffer, 'C', BUFFER_LEN));
	if (q && q < p) p = q;
	while (p) {
		// p points to next occurrence of 'c'/'C'
		buffer = p;
		if (BUFFER_LEN < 7) return bufend; // not enough room for "charset"
		if (strni_eq(buffer, "CHARSET", 7)) break; // found "charset"

		// keep looking
		p = reinterpret_cast<const char *>(op_memchr(buffer + 1, 'c', BUFFER_LEN - 1));
		q = reinterpret_cast<const char *>(op_memchr(buffer + 1, 'C', BUFFER_LEN - 1));
		if (q && q < p) p = q;
	}
	if (!p) return bufend; // No 'c'/'C' found, cannot find charset, give up

	// buffer points to first occurrence of "charset"
	OP_ASSERT(BUFFER_LEN >= 7);
	OP_ASSERT(strni_eq(buffer, "CHARSET", 7));
	buffer += 7;

//	2. Skip any U+0009, U+000A, U+000B, U+000C, U+000D, or U+0020 characters
//	   that immediately follow the word 'charset' (there might not be any).

	while (BUFFER_LEN > 0 && op_isspace(*buffer)) buffer++;

//	3. If the next character is not a U+003D EQUALS SIGN ('='), return
//	   nothing.

	if (BUFFER_LEN <= 0) return bufend;
	if (*buffer != '=') return buffer;
	buffer++;

//	4. Skip any U+0009, U+000A, U+000B, U+000C, U+000D, or U+0020 characters
//	   that immediately follow the word equals sign (there might not be any).

	while (BUFFER_LEN > 0 && op_isspace(*buffer)) buffer++;

//	5. Process the next character as follows:
//		- If it is a U+0022 QUOTATION MARK ('"') and there is a later
//		  U+0022 QUOTATION MARK ('"') in s
//			Return string between the two quotation marks.
//		- If it is a U+0027 APOSTROPHE ("'") and there is a later
//		  U+0027 APOSTROPHE ("'") in s
//			Return the string between the two apostrophes.
//		- If it is an unmatched U+0022 QUOTATION MARK ('"')
//		- If it is an unmatched U+0027 APOSTROPHE ("'")
//		- If there is no next character
//			Return nothing.
//		- Otherwise
//			Return the string from this character to the first
//			U+0009, U+000A, U+000B, U+000C, U+000D, U+0020, or
//			U+003B character or the end of s, whichever comes first.
	char b = 0;
	if (BUFFER_LEN <= 0) return bufend;
	if (*buffer == '"' || *buffer == '\'') {
		b = *buffer;
		buffer++;
	}
	*charset = buffer;
	while (BUFFER_LEN > 0) {
		if (( b && *buffer == b) ||        // Found terminating quote
		    (!b && (op_isspace(*buffer) || // Found terminating space
		            *buffer == ';')))      // or terminating semicolon
		{
			*charset_len = buffer - *charset;
			return buffer;
		}
		buffer++;
	}
	if (!b) { // Terminate at end of string
		*charset_len = buffer - *charset;
		return buffer;
	}
	// else b is set, and we have an unmatched quote
	*charset = NULL;
	return bufend;

#undef BUFFER_LEN
}

CharsetDetector::AutoDetectMode CharsetDetector::AutoDetectIdFromString(const char *mode)
{
	if (mode && *mode)
	{
		if (strni_eq(mode, "AUTODETECT", 10))
		{
			if (0 == mode[10])
			{
				return generic;
			}
			else if (mode[10] == '-')
			{
				switch (STR2INT16(op_toupper(static_cast<unsigned char>(mode[11])),
								  op_toupper(static_cast<unsigned char>(mode[12]))))
				{
#ifdef ENCODINGS_HAVE_JAPANESE
					case STR2INT16('J', 'P'): // Japanese detection
						return japanese;
#endif
#ifdef ENCODINGS_HAVE_CHINESE
					case STR2INT16('Z', 'H'): // Chinese detection
						return chinese;
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
					case STR2INT16('R', 'U'): // Cyrillic detection
						return cyrillic;
#endif
#ifdef ENCODINGS_HAVE_KOREAN
					case STR2INT16('K', 'O'): // Korean detection
						return korean;
#endif
				}
			}
		}
	}

	return autodetect_none;
}

CharsetDetector::AutoDetectMode CharsetDetector::AutoDetectIdFromWritingSystem(WritingSystem::Script script)
{
	switch (script)
	{
# ifdef ENCODINGS_HAVE_CHINESE
	case WritingSystem::ChineseUnknown: return chinese;
	case WritingSystem::ChineseSimplified: return chinese_simplified;
	case WritingSystem::ChineseTraditional: return chinese_traditional;
# endif
# ifdef ENCODINGS_HAVE_JAPANESE
	case WritingSystem::Japanese: return japanese;
# endif
# ifdef ENCODINGS_HAVE_KOREAN
	case WritingSystem::Korean: return korean;
# endif
# ifdef ENCODINGS_HAVE_CYRILLIC
	case WritingSystem::Cyrillic: return cyrillic;
# endif
	default: return autodetect_none;
	}
}

const char *CharsetDetector::AutoDetectStringFromId(CharsetDetector::AutoDetectMode mode)
{
	switch (mode)
	{
	case generic: return "AUTODETECT";
#ifdef ENCODINGS_HAVE_JAPANESE
	case japanese: return "AUTODETECT-JP";
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	case chinese:
	case chinese_traditional:
	case chinese_simplified:
		return "AUTODETECT-ZH";
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
	case cyrillic: return "AUTODETECT-RU";
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	case korean: return "AUTODETECT-KO";
#endif
	default: return "";
	}
}
