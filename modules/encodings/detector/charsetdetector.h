/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Character set autodetection implementation for Opera

#ifndef CHARSETDETECTOR_H
#define CHARSETDETECTOR_H

class Window;

#include "modules/windowcommander/WritingSystem.h"

/**
 * The CharsetDetector class is used to find out which character set is used
 * in a document. It has a number of class methods that do state-less
 * checking, as well as object methods that perform state-requiring checks
 * on an entire document in steps.
 */
class CharsetDetector
{
public:
	/**
	 * Create the encoding detection object.
	 *
	 * @param server Name of server this document comes from, NULL if not
	 *               defined. Used to enable certain autodetections.
	 * @param window Pointer to document's window, or NULL if none is
	 *               available. Used to check forced encoding.
	 * @param force_encoding Name of forced encoding to use if Window
	 *               pointer is not available.
	 * @param content_language Language of the document (if known).
	 * @param utf8_threshold The number of UTF-8 characters that must be
	 *                       exceeded in a document for it to be considered.
	 * @param allowUTF7 If FALSE (the default), support for detecting UTF-7
	 *                  will be disabled. See bug #313069 for details.
	 * @param referrer_charset If given, indicates the charset of the
	 *                         referring document. This is used as a hint
	 *                         to enable certain autodetections.
	 */
	CharsetDetector(const char *server = NULL,
	                Window *window = NULL,
	                const char *force_encoding = NULL,
	                const char *content_language = NULL,
	                int utf8_threshold = 10,
	                BOOL allowUTF7 = FALSE,
	                const char *referrer_charset = NULL);

	/**
	 * Dispose of the encoding detection object.
	 */
	virtual ~CharsetDetector();

	/**
	 * Given the beginning of an XML document, this function will peek at
	 * the contents and return the name of the encoding, or NULL if it was
	 * not able to determine the encoding. Note that the returned string
	 * is volatile and cannot be guaranteed to persist. Callers need not
	 * worry about releasing the string.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7.
	 * @return Detected character set.
	 */
	static const char *GetXMLEncoding(const void *buf, unsigned long len, BOOL allowUTF7 = FALSE);

	/**
	 * Given the beginning of a CSS stylesheet, this function will peek at
	 * the contents and return the name of the encoding, or NULL if it was
	 * not able to determine the encoding. Note that the returned string
	 * is volatile and cannot be guaranteed to persist. Callers need not
	 * worry about releasing the string.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7.
	 * @return Detected character set.
	 */
	static const char *GetCSSEncoding(const void *buf, unsigned long len, BOOL allowUTF7 = FALSE);

	/**
	 * Given the beginning of an HTML document, this function will peek at
	 * the contents and return the name of the encoding, or NULL if it was
	 * not able to determine the encoding. Note that the returned string
	 * is volatile and cannot be guaranteed to persist. Callers need not
	 * worry about releasing the string.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7.
	 * @return Detected character set.
	 */
	static const char *GetHTMLEncoding(const void *buf, unsigned long len, BOOL allowUTF7 = FALSE);

#ifdef PREFS_HAS_LNG
	/**
	 * Given the beginning of a language file, this function will peek at
	 * the contents and return the name of the encoding, or NULL if it was
	 * not able to determine the encoding. Note that the returned string
	 * is volatile and cannot be guaranteed to persist. Callers need not
	 * worry about releasing the string.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7.
	 * @return Detected character set.
	 */
	static const char *GetLanguageFileEncoding(const void *buf, unsigned long len, BOOL allowUTF7 = FALSE);
#endif

	/**
	 * Given the beginning of JS source code, this function will peek at
	 * the contents and return the name of the encoding, or NULL if it was
	 * not able to determine the encoding. Note that the returned string
	 * is volatile and cannot be guaranteed to persist. Callers need not
	 * worry about releasing the string.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param forUserJS If TRUE and encoding not otherwise determined, scan for UserJS @encoding decl in comment header.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7.
	 * @return Detected character set.
	 */
	static const char *GetJSEncoding(const void *buf, unsigned long len, BOOL forUserJS = FALSE, BOOL allowUTF7 = FALSE);

	/**
	 * Given the beginning of a data stream, this function will peek at
	 * the contents and return the name of the encoding if there is a
	 * valid BOM, or NULL if none was found.
	 *
	 * This function can only find UTF encodings that have BOMs. For all
	 * other kinds of data, NULL is returned.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param allowUTF7 If FALSE (the default), refuse to detect UTF-7 BOM.
	 * @return Detected character set.
	 */
	static const char *GetUTFEncodingFromBOM(const void *buf, unsigned long len, BOOL allowUTF7 = FALSE);

	/**
	 * Given the beginning of a data stream, this function will peek at
	 * the contents and check if it is probable that the text is encoded
	 * with UTF-16 in little or big endian form, and return the name of
	 * the encoding.
	 *
	 * This function can only guess encoding if the first two characters
	 * in the strings are in the basic latin ("ASCII") range. If the
	 * format of the document is known, and it always begins with a specific
	 * character (such as '<' for HTML and XML), specifying the "initial"
	 * parameter will make prediction better.
	 *
	 * @param buf Data buffer to peek at.
	 * @param len Length, in bytes, of valid bytes in buf.
	 * @param initial
	 *    Character that the stream must start with, or 0 if it can be any
	 *    basic latin character (reduces accuracy).
	 * @return Predicted character set.
	 */
	static const char *GetUTFEncodingFromText(const void *buf, unsigned long len, char initial = 0);

	/**
	 * Scan through a buffer looking for clues as to what the encoding
	 * might be. Findings are recorded in the object and can be retrieved
	 * using the GetDetectedCharset method. Please note that state is
	 * remembered between calls, so the object can not be re-used.
	 *
	 * Normal usage of this method would be something like this:
	 * <pre>
	 *  CharsetDetector d(server);
	 *  while (buffer = newdata())
	 *  {
	 *   d.PeekAtBuffer(buffer, length);
	 *  }
	 *  const char *charset = d.GetDetectedCharset();
	 * </pre>
	 *
	 * @param buf Data buffer to look in.
	 * @param len Length, in bytes, of valid bytes in buf.
	 */
	void PeekAtBuffer(const void *buf, unsigned long len);

	/**
	 * Return character set detected by PeekAtBuffer.
	 *
	 * @return Detected character set, or NULL if no decision can be drawn.
	 */
	const char *GetDetectedCharset();

	/**
	 * Return TRUE if the charset cannot be UTF-8
	 *
	 * In some cases where the CharsetDetector cannot decide on a detected
	 * charset (and thus GetDetectedCharset() returns NULL), it can still
	 * determine (based on finding invalid UTF-8 byte sequences) that UTF-8
	 * is not an alternative. This information can sometimes be useful to
	 * the caller.
	 *
	 * This method should probably only be called after
	 * GetDetectedCharset() has returned NULL.
	 *
	 * This is an ugly solution to CORE-15862, and should be deprecated in
	 * a future redesign of this API.
	 */
	BOOL GetDetectedCharsetIsNotUTF8() { return utf8_invalid > 0; }

	/**
	 * Determine if buffer starts with a UTF-16 BOM (byte order mark)
	 * character. Please note that it will report a false positive
	 * for the UTF-32LE BOM (UTF-32 is no longer supported by this module).
	 *
	 * @param buffer Buffer to check for UTF-16 BOM in.
	 * @return Whether or not there is a UTF-16 BOM there.
	 */
	inline static bool StartsWithUTF16BOM(const void *buffer)
	{
#ifdef NEEDS_RISC_ALIGNMENT
		return (static_cast<char>(0xFE) == reinterpret_cast<const char *>(buffer)[0] &&
		        static_cast<char>(0xFF) == reinterpret_cast<const char *>(buffer)[1]) ||
		       (static_cast<char>(0xFE) == reinterpret_cast<const char *>(buffer)[1] &&
		        static_cast<char>(0xFF) == reinterpret_cast<const char *>(buffer)[0]);
#else
		return 0xFEFF == *reinterpret_cast<const UINT16 *>(buffer) ||
		       0xFFFE == *reinterpret_cast<const UINT16 *>(buffer);
#endif
	};

	/**
	 * Determine if buffer starts with a UTF-8 BOM (byte order mark)
	 * character.
	 *
	 * @param buffer Buffer to check for UTF-8 BOM in.
	 * @return Whether or not there is a UTF-8 BOM there.
	 */
	inline static bool StartsWithUTF8BOM(const void *buffer)
	{
		return (reinterpret_cast<const unsigned char *>(buffer)[0] == 0xEF &&
		        reinterpret_cast<const unsigned char *>(buffer)[1] == 0xBB &&
		        reinterpret_cast<const unsigned char *>(buffer)[2] == 0xBF);
	};

#ifdef ENCODINGS_HAVE_UTF7
	/**
	 * Determine if buffer starts with a UTF-7 BOM (byte order mark)
	 * character.
	 *
	 * @param buffer Buffer to check for UTF-7 BOM in.
	 * @return Whether or not there is a UTF-7 BOM there.
	 */
	inline static bool StartsWithUTF7BOM(const void *buffer)
	{
		return !op_memcmp(reinterpret_cast<const char *>(buffer), "+/v8-", 5);
	};
#endif // ENCODINGS_HAVE_UTF7

	/**
	 * Enumeration describing the autodetection modes supported by
	 * CharsetDetector. These are usually represented as strings on the
	 * form "AUTODETECT-xx" when used side-by-side to charset identifiers.
	 */
	enum AutoDetectMode
	{
		autodetect_none,			///< No autodetection used.
# ifdef ENCODINGS_HAVE_JAPANESE
		japanese,					///< Japanese autodetection used.
# endif
# ifdef ENCODINGS_HAVE_CHINESE
		chinese,					///< Chinese autodetection used.
		chinese_simplified,			///< Simplified Chinese autodetection used.
		chinese_traditional,		///< Traditional Chinese autodetection used.
# endif
# ifdef ENCODINGS_HAVE_KOREAN
		korean,						///< Korean autodetection used.
# endif
# ifdef ENCODINGS_HAVE_CYRILLIC
		cyrillic,					///< Cyrillic autodetection used.
# endif
		generic						///< Generic autodetection used.
	};

	/**
	 * Convert an autodetect mode string to a numeric id.
	 *
	 * @param mode String representation of mode.
	 * @return Numeric representation of mode.
	 * @see AutoDetectMode
	 */
	static AutoDetectMode AutoDetectIdFromString(const char *mode);

	/**
	 * Get an autodetect mode id from a language identifier.
	 *
	 * @param lang String representation of language.
	 * @return Autodetect mode to use for this language.
	 * @see AutoDetectMode
	 */
	inline static AutoDetectMode AutoDetectIdFromLanguage(const char *lang)
	{ return AutoDetectIdFromWritingSystem(WritingSystem::FromLanguageCode(lang)); }
	inline static AutoDetectMode AutoDetectIdFromLanguage(const uni_char *lang)
	{ return AutoDetectIdFromWritingSystem(WritingSystem::FromLanguageCode(lang)); }

	/**
	 * Get an autodetect mode id from a writing system id.
	 *
	 * @param script Writing system identifier.
	 * @return Autodetect mode to use for this writing system.
	 * @see AutoDetectMode
	 */
	static AutoDetectMode AutoDetectIdFromWritingSystem(WritingSystem::Script script);

	/**
	 * Convert an autodetect mode id to a string representation.
	 *
	 * @param mode Numeric representation of mode.
	 * @return String representation of mode.
	 * @see AutoDetectMode
	 */
	static const char *AutoDetectStringFromId(AutoDetectMode mode);

	/**
	 * Extracts charset/encoding from a Content-Type, using the algorithm
	 * specified in section 4.9.5 of the HTML5 specification:
	 * http://www.w3.org/html/wg/html5/#algorithm3 (as of 2008-05-23)
	 *
	 * @param str Content-Type string to check.
	 * @param[out] len Length of the encoding string.
	 * @return Pointer (insigned Content-Type string) to the encoding,
	 *         or NULL if none was found.
	 */
	inline static const char *EncodingFromContentType(const char *str, unsigned long *len)
	{
		const char *retval;
		get_encoding_from_content_type(str, op_strlen(str), &retval, len);
		return retval;
	}

protected:
	/** Internal helper function. */
	static const char *scan_to(char to, const char *buffer, const char *bufend);
	/** Internal helper function. */
	static const char *scan_to(const char *token, const char *buffer, const char *bufend);
	/** Internal helper function. */
	static const char *scan_case_to(const char *uctoken, const char *buffer, const char *bufend);

	/**
	 * Parses an attribute as specified by the "get an attribute" algorithm
	 * in section 8.2.2.1 of the HTML5 specification:
	 * http://www.w3.org/html/wg/html5/#get-an (as of 2008-05-23)
	 *
	 * @param buf Input buffer; must be at start of attribute (possibly with leading whitespace)
	 * @param buf_len Length of input buffer
	 * @param attrname Pointer (within input buffer) to attribute name is stored here
	 * @param attrname_len Length of attribute name is stored here
	 * @param attrvalue Pointer (within input buffer) to attribute value is stored here
	 * @param attrvalue_len Length of attribute name is stored here
	 * @returns Pointer (within input buffer) to immediately after the parsed attribute
	 *          If returned pointer equals buf + buf_len, then more data is needed in order to produce a correct result.
	 */
	static const char *get_an_attribute(
		const char  *buf,   unsigned long  buf_len,
		const char **attrname, unsigned long *attrname_len,
		const char **attrvalue,unsigned long *attrvalue_len);

	/**
	 * Extracts charset/encoding from a Content-Type, using the algorithm
	 * specified in section 4.9.5 of the HTML5 specification:
	 * http://www.w3.org/html/wg/html5/#algorithm3 (as of 2008-05-23)
	 *
	 * @param buf Input buffer; must be at start of Content-Type value
	 * @param buf_len Length of input buffer
	 * @param charset Pointer (within input buffer) to parsed charset is stored here
	 * @param[out] charset_len Length of parsed charset is stored here
	 * @return Pointer (within input buffer) to immediately after the parsed charset
	 *         If returned pointer equals buf + buf_len, then more data may be needed in order to produce a correct result.
	 */
	static const char *get_encoding_from_content_type(
		const char  *buf,     unsigned long  buf_len,
		const char **charset, unsigned long *charset_len);

private:
	// Select what to autodetect
	enum AutoDetectMode autodetect;

	// Internal data for the autodetection
#ifdef ENCODINGS_HAVE_CYRILLIC
	bool probable_8859_5_preposition:1, probable_koi_preposition:1,
	     probable_windows_1251_preposition:1, probable_ibm866_preposition:1,
	     probable_8859_5_word:1, probable_koi_word:1,
	     probable_windows_1251_word:1, probable_ibm866_word:1;
#endif
#ifdef ENCODINGS_HAVE_CHINESE
	bool has_5_big5_consecutive:1;
#endif
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	bool has_5_euc_consecutive:1;
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
	bool has_5_sjis_consecutive:1;
#endif

#ifdef ENCODINGS_HAVE_CHINESE
	int big5_hints, euctw_hints, gbk_hints, gb18030_hints, hz_hints,
		big5_penalty, big5_consecutive;
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
	int sjis_invalid, sjis_valid, sjis_consecutive;
# ifndef IMODE_EXTENSIONS
	int sjis_halfwidth_katakana;
# endif
#endif
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	int euc_invalid, euc_valid, euc_penalty, euc_consecutive;
#endif
#ifdef ENCODINGS_HAVE_CYRILLIC
	int iso_8859_5_hints, koi_hints, windows_1251_hints,
		ibm866_hints, iso_8859_5_invalid;
#endif
	int utf8_valid, utf8_invalid, utf8_threshold;

#ifdef ENCODINGS_FAST_AUTODETECTION
	/** Number of bytes to look at */
	size_t peek;
#endif
	
	/** Detected character set */
	const char *result;

	/** Whether or not we allow detection of UTF-7 */
	BOOL m_allowUTF7;

	// State data for the detection state machines

	/** UTF-8 state machine */
	int utf8_length;

#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	/** ISO-2022-JP/CN/KR state machine */
	enum
	{
		iso2022_none, iso2022_esc, iso2022_esc_paren, iso2022_esc_dollar,
		iso2022_esc_dollar_leftparen, iso2022_esc_dollar_rightparen,
		iso2022_esc_dollar_asterisk,
		iso2022_invalid
	} iso2022state;
#endif

#ifdef ENCODINGS_HAVE_CHINESE
	/** Big5 state machine */
	enum
	{
		big5_none, big5_second
	} big5state;
#endif

#ifdef ENCODINGS_HAVE_CHINESE
	/** HZ state machine */
	enum
	{
		hz_none, hz_lead_sbcs, hz_lead_dbcs, hz_first, hz_second, hz_cr, hz_lf
	} hzstate;
#endif

#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	/** EUC state machine, EUC-(CN,JP,TW,KR), GBK and GB18030 */
	enum
	{
		// Standard EUC
		euc_none, euc_first, euc_second,
		// EUC-TW and GBK
		gbk_second, euctw_plane_or_gbk_second,
		euctw_lead_or_gbk_anything, euctw_or_gbk_second,
		// GB18030
		gb18030_third, gb18030_fourth,
		// EUC-JP
		eucjp_katakana
	} eucstate;
#endif

#ifdef ENCODINGS_HAVE_JAPANESE
	/** Shift-JIS state machine */
	enum
	{
		sjis_none, sjis_second
	} sjisstate;
#endif

#ifdef ENCODINGS_HAVE_CYRILLIC
	/* Cyrillic state machine (ISO 8859-5, KOI8, Windows 1251, IBM 866) */
	enum
	{
		nonword, firstletter, nonfirstletter
	} cyrillic_state;
#endif

#ifndef ENCODINGS_FAST_AUTODETECTION
	/** Unicode BOM state machine */
	enum
	{
		utf_bom_none, utf_bom_invalid,
		// UTF-16 BOM
		utf16le_bom_second, utf16be_bom_second,
		// UTF-8 BOM
		utf8_bom_second, utf8_bom_third,
		// UTF-7 BOM
		utf7_bom_second, utf7_bom_third, utf7_bom_fourth, utf7_bom_fifth
	} utfbomstate;
#endif
};

#endif // CHARSETDETECTOR_H
