/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef URI_ESCAPE_H
#define URI_ESCAPE_H

#ifdef FORMATS_URI_ESCAPE_SUPPORT

/**
 * Class facilitating %%-escaping (aka URI-escaping) of unsafe characters in strings such as URLs
 * @since core-2-3
 */
class UriEscape
{
public:
	/**
	 * Flags controlling how escaping is performed
	 */
	enum EscapeFlags
	{
		// Base escaping flags:
		NonSpaceCtrl		= 0x00000001,	///< Escape control characters that are not whitespace: 0x00-0x08, 0x0b-0x0c, 0x0e-0x1f, 0x7f
		SpaceCtrl			= 0x00000002,	///< Escape whitespace control characters: 0x09-0x0a, 0x0d
		Slash				= 0x00000004,	///< Escape the slash character: "/"
		Backslash			= 0x00000008,	///< Escape the backslash character: "\\"
		Hash				= 0x00000010,	///< Escape the hash character: "#"
		Percent				= 0x00000020,	///< Escape the percent character: "%"
		Equals				= 0x00000040,	///< Escape the equals character: "="
		Asterisk			= 0x00000080,	///< Escape the asterisk character: "*"
		AmpersandQmark		= 0x00000100,	///< Escape the ampersand and question mark characters: "&?"
		ColonSemicolon		= 0x00000200,	///< Escape the colon and semicolon characters: ":;"
		URIExcluded			= 0x00000400,	///< Escape characters that are excluded from URIs according to RFC2396, 2.4.3: " \"<>^`{|}" (except "\\#%[]")
		FormUnsafePrintable	= 0x00000800,	///< Escape all printable characters unsafe in form submit: " !\"#$%&'()+,:;<=>?@[\\]^`{|}~" (excluding Slash)
		WMLUnsafe			= 0x00001000,	///< Escape characters unsafe in WML: Ctrl + " \"#$%&+,/:;<=>?@[\\]^`{|}" + 0x80-0xff
		SearchUnsafe		= 0x00002000,	///< Escape characters unsafe in search queries: "#%&+;"
		RFC2231Unsafe		= 0x00004000,	///< Escape characters unsafe in RFC2231 parameters: Ctrl + " *'%()<>@,;:\\\"/[]?=" + 0x80-0xff
		Range_80_9f			= 0x00008000,	///< Escape the range: 0x80-0x9f
		Range_a0_ff			= 0x00010000,	///< Escape the range: 0xa0-0xff
		URIQueryExcluded    = 0x00020000,   ///< Escape characters that are excluded from the query part of a URL (subset of URIExcluded): " \"<>`"

		UsePlusForSpace		= 0x80000000,	///< Translate space to '+'
#ifdef _USE_PLUS_FOR_SPACE_
		UsePlusForSpace_DEF	= 0x40000000,	///< Translate space to '+' if _USE_PLUS_FOR_SPACE_ macro is defined
#else
		UsePlusForSpace_DEF	= 0x00000000,	///< Translate space to '+' if _USE_PLUS_FOR_SPACE_ macro is defined
#endif
		PrefixPercent		= 0x00000000,	///< Use "%" as the escape prefix (default)
		PrefixBackslash		= 0x20000000,	///< Use "\" as the escape prefix
		PrefixBackslashX	= 0x10000000,	///< Use "\x" as the escape prefix

		Ctrl				= NonSpaceCtrl | SpaceCtrl, ///< Escape all control characters: 0x00-0x1f, 0x7f
		UniCtrl				= Ctrl | Range_80_9f, ///< Escape all unicode control characters: 0x00-0x1f, 0x7f-0x9f
		Range_80_ff			= Range_80_9f | Range_a0_ff, ///< Escape the range: 0x80-0xff
		UnsafePrintable		= FormUnsafePrintable | Asterisk,	///< Escape all unsafe printable characters: " !\"#$%&'()*+,:;<=>?@[\\]^`{|}~" (but not Slash)
		AllUnsafe			= Ctrl | UnsafePrintable | Slash | Range_80_ff,///< Escape all unsafe characters, i.e. everything except "-.0-1A-Z_a-z": 0x00-0x2c, 0x2f, 0x3a-0x40, 0x5b-0x5e, 0x60, 0x7a-0xff
		StandardUnsafe		= Ctrl | URIExcluded | Backslash | Range_80_ff, ///< Escape the most common set of unsafe characters: Ctrl, URIExcluded, "\", 0x80-0xff
		QueryStringUnsafe	= Ctrl | URIQueryExcluded | Range_80_ff, ///< Escape characters that are unsafe in the query part of a URL: Ctrl, URIQueryExcluded, 0x80-0xff
		QueryUnsafe			= Equals | AmpersandQmark, ///< Escape characters that are unsafe in query names and values: "&=?"
		FormUnsafe			= Ctrl | FormUnsafePrintable | Slash | Range_80_ff, ///< Escape all characters unsafe in form submit
#if PATHSEPCHAR == '\\'
		Filename			= Ctrl | URIExcluded | Hash | Percent ///< StandardUnsafe + HashPercent - PATHSEPCHAR - 0x80-0xff
#else
		Filename			= Ctrl | URIExcluded | Backslash | Hash | Percent ///< StandardUnsafe + HashPercent - PATHSEPCHAR - 0x80-0xff
#endif
	};

	/**
	 * Determine whether a character needs escaping according to the escape flags.
	 * Depending on the escape flags, an escape sequence may need two or three extra
	 * characters after conversion.
	 *
	 * @param ch The character to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return TRUE if ch needs escaping, otherwise FALSE
	 */
	static BOOL NeedEscape(uni_char ch, int escape_flags);

	/**
	 * @overload
	 */
	static BOOL NeedEscape(char ch, int escape_flags) { return NeedEscape((uni_char)(unsigned char)ch, escape_flags); }

	/**
	 * Determine whether a character needs modification or escaping according to the
	 * escape flags. Modification may be e.g. conversion from space to "+",
	 * it does not necessarily mean that the conversion needs extra characters.
	 *
	 * @param ch The character to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return TRUE if ch needs escaping, otherwise FALSE
	 */
	static BOOL NeedModification(uni_char ch, int escape_flags);

	/**
	 * @overload
	 */
	static BOOL NeedModification(char ch, int escape_flags) { return NeedModification((uni_char)(unsigned char)ch, escape_flags); }

	/**
	 * Count the number of characters in a string that needs escaping according to the
	 * escape flags. The result may be used to calculate the size of an output
	 * string. If the return value is 0, it does not necessarily mean that you don't
	 * need to run the string through escaping, as the escape flags may e.g. require
	 * conversion from space to "+".
	 *
	 * @param str The null-terminated string to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The number of characters in str that needs escaping
	 */
	static int CountEscapes(const char* str, int escape_flags);

	/**
	 * @overload
	 */
	static int CountEscapes(const uni_char* str, int escape_flags);

	/**
	 * Count the number of characters in a string that needs escaping according to the
	 * escape flags. The result may be used to calculate the size of an output
	 * string. If the return value is 0, it does not necessarily mean that you don't
	 * need to run the string through escaping, as the escape flags may e.g. require
	 * conversion from space to "+".
	 *
	 * @param str The string to check (null will not be considered the end of the string)
	 * @param len The length of the string to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The number of characters in str that needs escaping
	 */
	static int CountEscapes(const char* str, int len, int escape_flags);

	/**
	 * @overload
	 */
	static int CountEscapes(const uni_char* str, int len, int escape_flags);

	/**
	 * Get the total length of a string as it will be after escaping according to the
	 * escape flags. If the return value is the length of the input string, it
	 * does not necessarily mean that you don't need to run the string through escaping,
	 * as the escape flags may e.g. require conversion from space to "+".
	 *
	 * @param str The null-terminated string to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The total length of the output string after escaping (excluding null-terminator)
	 */
	static int GetEscapedLength(const char* str, int escape_flags);

	/**
	 * @overload
	 */
	static int GetEscapedLength(const uni_char* str, int escape_flags);

	/**
	 * Get the total length of a string as it will be after escaping according to the
	 * escape flags. If the return value is the length of the input string, it
	 * does not necessarily mean that you don't need to run the string through escaping,
	 * as the escape flags may e.g. require conversion from space to "+".
	 *
	 * @param str The string to check (null will not be considered the end of the string)
	 * @param len The length of the string to check
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The total length of the output string after escaping
	 */
	static int GetEscapedLength(const char* str, int len, int escape_flags);

	/**
	 * @overload
	 */
	static int GetEscapedLength(const uni_char* str, int len, int escape_flags);

	/**
	 * Generate the first escape code for an input character.
	 * @param ch The character to encode
	 * @return The first escape code for encoding ch
	 */
	static inline char EscapeFirst(char ch)
	{
		unsigned char cc = (((unsigned char) ch) >> 4) & 0x0f;
		return (char) (cc + ((cc>9) ? 'A' - 10 : '0'));
	}

	/**
	 * Generate the second escape code for an input character.
	 * @param ch The character to encode
	 * @return The second escape code for encoding ch
	 */
	static inline char EscapeLast(char ch)
	{
		unsigned char cc = ((unsigned char) ch) & 0x0f;
		return (char) (cc + ((cc>9) ? 'A' - 10 : '0'));
	}

	/**
	 * Store a character in the destination buffer, escaped or converted
	 * if needed according to the escape flags.
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param ch The character to be stored
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The number of characters added to the destination buffer
	 */
	static int EscapeIfNeeded(char* dst, char ch, int escape_flags);

	/**
	 * @overload
	 */
	static int EscapeIfNeeded(uni_char* dst, uni_char ch, int escape_flags);

	/**
	 * Store a string in the destination buffer, escaped and converted
	 * according to the escape flags.
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param src The null-terminated source string to be escaped
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The number of characters added to the destination buffer (excluding null-terminator)
	 */
	static int Escape(char* dst, const char* src, int escape_flags);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, const char* src, int escape_flags);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, const uni_char* src, int escape_flags);

	/**
	 * Store a string in the destination buffer, escaped and converted
	 * according to the escape flags.
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param src The source string to be escaped (null will not be considered the end of the string)
	 * @param src_len The length of the source string
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return The number of characters added to the destination buffer
	 */
	static int Escape(char* dst, const char* src, int src_len, int escape_flags);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, const char* src, int src_len, int escape_flags);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, const uni_char* src, int src_len, int escape_flags);

	/**
	 * Store a string in the destination buffer, escaped and converted
	 * according to the escape flags, not exceeding a specified output length.
	 * @param dst Destination buffer
	 * @param dst_maxlen Maximum length of the output buffer
	 * @param src The source string to be escaped (null will not be considered the end of the string)
	 * @param src_len The length of the source string
	 * @param escape_flags Flags controlling how escaping is performed
	 * @param src_consumed If not NULL, the number of characters consumed from the source
	 *     string will be written to this pointer
	 * @return The number of characters added to the destination buffer
	 */
	static int Escape(char* dst, int dst_maxlen, const char* src, int src_len, int escape_flags, int* src_consumed=NULL);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, int dst_maxlen, const char* src, int src_len, int escape_flags, int* src_consumed=NULL);

	/**
	 * @overload
	 */
	static int Escape(uni_char* dst, int dst_maxlen, const uni_char* src, int src_len, int escape_flags, int* src_consumed=NULL);

	/**
	 * Append a character to the destination, escaped or converted
	 * if needed according to the escape flags.
	 * @param dst Destination string
	 * @param ch The character to be appended
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return OK if the destination could be expanded without error
	 */
	static OP_STATUS AppendEscaped(OpString8& dst, char ch,  int escape_flags);

	/**
	 * @overload
	 */
	static OP_STATUS AppendEscaped(OpString& dst, uni_char ch,  int escape_flags);

	/**
	 * Append a string to the destination, escaped or converted
	 * if needed according to the escape flags.
	 * @param dst Destination string
	 * @param src The null-terminated source string to be appended
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return OK if the destination could be expanded without error
	 */
	static OP_STATUS AppendEscaped(OpString8& dst, const char* src, int escape_flags);

	/**
	 * @overload
	 */
	static OP_STATUS AppendEscaped(OpString& dst, const char* src, int escape_flags);

	/**
	 * @overload
	 */
	static OP_STATUS AppendEscaped(OpString& dst, const uni_char* src, int escape_flags);

	/**
	 * Append a string to the destination, escaped or converted
	 * if needed according to the escape flags.
	 * @param dst Destination string
	 * @param src The source string to be appended (null will not be considered the end of the string)
	 * @param src_len The length of the source string
	 * @param escape_flags Flags controlling how escaping is performed
	 * @return OK if the destination could be expanded without error
	 */
	static OP_STATUS AppendEscaped(OpString8& dst, const char* src, int src_len, int escape_flags);

	/**
	 * @overload
	 */
	static OP_STATUS AppendEscaped(OpString& dst, const char* src, int src_len, int escape_flags);

	/**
	 * @overload
	 */
	static OP_STATUS AppendEscaped(OpString& dst, const uni_char* src, int src_len, int escape_flags);

protected:
	static const int need_escape_masks[256]; /* ARRAY OK 2009-04-23 roarl */
};

/**
 * Class facilitating unescaping of %%-escaped (aka URI-escaped) characters in strings such as URLs
 * @since core-2-3
 */
class UriUnescape
{
public:
	/**
	 * Flags controlling how unescaping is performed
	 */
	enum UnescapeFlags
	{
		// Base unescaping flags:
		All					= 0x00,			///< Unescape all characters (empty exception set)
		ExceptUnsafeCtrl	= 0x01,			///< Unescape all except control characters that are not whitespace or Esc: 0x00-0x08, 0x0e-0x1a, 0x1c-0x1f, WHAT ABOUT 0x7f ???
		ExceptSpaceCtrl		= 0x02,			///< Unescape all except whitespace control characters: 0x09-0x0d
		ExceptEsc			= 0x04,			///< Unescape all except Esc: 0x1b
		ExceptSpace			= 0x08,			///< Unescape all except space: 0x20
		ExceptNbsp			= 0x10,			///< Unescape all except nonbreaking space: 0xa0
		ExceptUnsafe		= 0x20,			///< Unescape all except unsafe characters:	"\0\"#$%&+,/:;<=>?@[\\]^{|}": 0x00, 0x22-0x26, 0x2b-0x2c, 0x2f, 0x3a-0x40, 0x5b-0x5e, 0x7b-0x7d
		Except_80_9f		= 0x40,			///< Unescape all except unicode high control characters: 0x80-0x9f
		Except_a0_ff		= 0x80,			///< Unescape all except the range: 0xa0-0xff
		ExceptUnsafeHigh	= 0x00010000,	///< Unescape all except unsafe high characters: 0x115F, 0x1160, 0x2000-0x200B, 0x202E, 0x3000, 0x3164
		ConvertUtf8			= 0x00020000,	///< While unescaping, convert UTF-8 sequences
											///< If output is 8-bit, valid UTF-8 escape sequences are converted to plain UTF-8
		ConvertUtf8IfPref	= 0x00040000,	///< While unescaping, convert UTF-8 sequences if the "UseUTF8Urls" preference is set
		TranslatePathSepChar= 0x00080000,	///< While unescaping, translate "/" to "\" on windows
		TranslateDriveChar	= 0x00100000,	///< While unescaping, translate "|" to ":" on windows
		TranslatePlusToSpace= 0x00200000,   ///< While unescaping, translate '+' to space
		StopAfterQuery		= 0x00400000,	///< While unescaping UTF-8 sequences, stop after the query marker '?'
		// Combined unescaping flags:
		ExceptCtrl			= ExceptUnsafeCtrl | ExceptSpaceCtrl | ExceptEsc,	///< Unescape all except control charactes
		ExceptSpaces		= ExceptSpaceCtrl | ExceptSpace | ExceptNbsp,		///< Unescape all except whitespace characters
		Except_80_ff		= Except_80_9f | Except_a0_ff,						///< Unescape all except except the range: 0x80-0xff
		NonCtrlAndEsc		= ExceptUnsafeCtrl | ExceptSpaceCtrl,				///< Unescape all characters except control characters, but unescape Esc (0x1b)
		TranslatePathChars	= TranslatePathSepChar | TranslateDriveChar,		///< While unescaping, translate "/|" to "\:" on windows
		Safe				= ExceptCtrl | ExceptUnsafe | Except_80_9f,			///< Unescape all characters including >= 0xA0, except control and unsafe characters
		AllUtf8				= ConvertUtf8IfPref,								///< Unescape all characters, converting UTF-8 sequences depending on "UseUTF8Urls" pref
		SafeUtf8			= ConvertUtf8IfPref | Safe | ExceptSpace | Except_80_ff | ExceptUnsafeHigh | StopAfterQuery,	///< Unescape all safe characters except space, 0x80-0xff and unsafe "high" characters before '?', converting UTF-8 depending on pref
		MailUtf8			= ConvertUtf8IfPref | ExceptUnsafeCtrl | ExceptEsc | ExceptUnsafeHigh | StopAfterQuery,	///< Unescape all characters before '?', except control and unsafe "high" characters, but including whitespace, converting UTF-8 depending on pref
		MailUrlBodyUtf8		= ConvertUtf8IfPref | ExceptUnsafeHigh | StopAfterQuery,	///< Unescape all characters before '?', except unsafe "high" characters, converting UTF-8 depending on pref
		LocalfileUtf8		= ConvertUtf8 | TranslatePathChars | ExceptCtrl | ExceptUnsafeHigh,	///< Unescape and convert Utf8 sequences regardless of pref, except control and unsafe "high" characters, translating "/|" to "\:" on windows
		LocalfileAllUtf8	= ConvertUtf8 | TranslatePathChars | ExceptCtrl,	///< Unescape and convert Utf8 sequences regardless of pref, except control characters, translating "/|" to "\:" on windows
		LocalfileUrlUtf8	= ConvertUtf8 | ExceptCtrl | ExceptUnsafeHigh		///< Unescape and convert Utf8 sequences regardless of pref, except control and unsafe "high" characters
	};

	/**
	 * Decode a character from the given escape codes
	 * @param c1 first escape code
	 * @param c2 second escape code
	 * @return The decoded character
	 */
	static inline char Decode(char c1, char c2)
	{
		// Make sure your input is valid before calling this function.
		OP_ASSERT(op_isxdigit(c1));
		OP_ASSERT(op_isxdigit(c2));
		char cc1 = (c1 | 0x20) - '0';
		if (cc1 > 9) cc1 -= 0x27;
		char cc2 = (c2 | 0x20) - '0';
		if (cc2 > 9) cc2 -= 0x27;
		return (cc1 << 4) | cc2;
	}

	/**
	 * Store a string in the destination buffer, unescaped and converted
	 * according to the unescape flags.
	 * Since the input/output is null-terminated, %00 will never be unescaped.
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param src The null-terminated source string to be unescaped
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return The number of characters added to the destination buffer (excluding null-terminator)
	 */
	static int Unescape(char* dst, const char* src, int unescape_flags);

	/**
	 * @overload
	 */
	static int Unescape(uni_char* dst, const char* src, int unescape_flags);

	/**
	 * @overload
	 */
	static int Unescape(uni_char* dst, const uni_char* src, int unescape_flags);

	/**
	 * Store a string in the destination buffer, unescaped and converted
	 * according to the unescape flags.
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param src The source string to be unescaped (null will not be considered the end of the string)
	 * @param src_len The length of the source string
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return The number of characters added to the destination buffer (excluding null-terminator)
	 */
	static int Unescape(char* dst, const char* src, int src_len, int unescape_flags);

	/**
	 * @overload
	 */
	static int Unescape(uni_char* dst, const char* src, int src_len, int unescape_flags);

	/**
	 * @overload
	 */
	static int Unescape(uni_char* dst, const uni_char* src, int src_len, int unescape_flags);

	/**
	 * From the given UTF-8 encoded string, unescape the percent-encoded occurrences
	 * as UTF-8 octets. Convert that UTF-8 sequence into UTF-16, but do not map
	 * illegal UTF-8 sequences into U+FFFD, but promote their individual octets
	 * to codepoints of their own. This is required for compatible unescaping of
	 * javascript: escaped code.
	 * @param dest (output) The result string.
	 * @param str The null-terminated UTF-8 encoded string to unescape.
	 * @return OpStatus::ERR_NO_MEMORY if OOM is encountered, OpStatus::OK otherwise.
	 */
	static OP_STATUS UnescapeJavascriptURL(OpString &dest, const char* str);

	/**
	 * Replace escaped characters in-place in the given string according
	 * to the unescape flags.
	 * Since the input/output is null-terminated, %00 will never be unescaped.
	 * @param str The null-terminated string to unescape
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return The number of characters in str after unescaping (excluding null-terminator)
	 */
	static int ReplaceChars(char* str, int unescape_flags);

	/**
	 * @overload
	 */
	static int ReplaceChars(uni_char* str, int unescape_flags);

	/**
	 * Replace escaped characters in-place in the given string according
	 * to the unescape flags.
	 * @param str The string to unescape (null will not be considered the end of the string)
	 * @param len The length of str, modified to match the result
	 * @param unescape_flags Flags controlling how unescaping is performed
	 */
	static void ReplaceChars(char* str, int& len, int unescape_flags);

	/**
	 * @overload
	 */
	static void ReplaceChars(uni_char* str, int& len, int unescape_flags);

	/**
	 * Compare strings after unescaping according to the unescape flags.
	 * @param s1 The first null-terminated string to compare
	 * @param s2 The second null-terminated string to compare
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return Comparison result according to the same scheme as op_strcmp
	 */
	static int strcmp(const char* s1, const char* s2, int unescape_flags);

	/**
	 * @overload
	 */
	static int strcmp(const uni_char* s1, const uni_char* s2, int unescape_flags);

	/**
	 * Compare strings case insensitively after unescaping according to the unescape flags.
	 * @param s1 The first null-terminated string to compare
	 * @param s2 The second null-terminated string to compare
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return Comparison result according to the same scheme as op_strcmp
	 */
	static int stricmp(const char* s1, const char* s2, int unescape_flags);

	/**
	 * @overload
	 */
	static int stricmp(const uni_char* s1, const uni_char* s2, int unescape_flags);

	/**
	 * Compare strings after unescaping according to the unescape flags.
	 * @param s1 The first string to compare
	 * @param n1 The length of s1
	 * @param s2 The second string to compare
	 * @param n2 The length of s2
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return Comparison result according to the same scheme as op_strcmp
	 */
	static int strncmp(const char* s1, int n1, const char* s2, int n2, int unescape_flags);

	/**
	 * @overload
	 */
	static int strncmp(const uni_char* s1, int n1, const uni_char* s2, int n2, int unescape_flags);

	/**
	 * Compare strings case insensitively after unescaping according to the unescape flags.
	 * @param s1 The first string to compare
	 * @param n1 The length of s1
	 * @param s2 The second string to compare
	 * @param n2 The length of s2
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return Comparison result according to the same scheme as op_strcmp
	 */
	static int strnicmp(const char* s1, int n1, const char* s2, int n2, int unescape_flags);

	/**
	 * @overload
	 */
	static int strnicmp(const uni_char* s1, int n1, const uni_char* s2, int n2, int unescape_flags);

	/**
	 * Determine whether one string is a prefix of the other after unescaping
	 * both strings according to the unescape flags.
	 * @param prefix The null-terminated prefix
	 * @param s The null-terminated string to check
	 * @param unescape_flags Flags controlling how unescaping is performed
	 * @return TRUE if prefix is found at the start of s
	 */
	static BOOL isstrprefix(const char* prefix, const char* s, int unescape_flags);

	/**
	 * @overload
	 */
	static BOOL isstrprefix(const uni_char* prefix, const uni_char* s, int unescape_flags);

	/**
	 * Determine whether a string contains escape sequences that encodes valid
	 * UTF-8 characters, and that it does not contain any invalid escape
	 * sequences. The result is a strong heuristic that can be used to
	 * determine whether a string that normally does not contain escaped
	 * characters should be unescaped with UTF-8 decoding.
	 * @param str The string to check, must be null-terminated if len==KAll
	 * @param len The length of str, or KAll for null-terminated input
	 * @return TRUE if valid escaped UTF-8 sequences are found
	 */
	static BOOL ContainsValidEscapedUtf8(const char* str, int len = KAll);

	/**
	 * @overload
	 */
	static BOOL ContainsValidEscapedUtf8(const uni_char* str, int len = KAll);
};


#if PATHSEPCHAR != '/'
#  define AND_NOT_PATHSEP(x) * ((x)-(int)'/')
#else
#  define AND_NOT_PATHSEP(x)
#endif
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
#  define AND_NOT_DRIVE(x) * ((x)-(int)'|')
#else
#  define AND_NOT_DRIVE(x)
#endif
#define NOT_SPECIAL(x) (((x)-(int)'%') * ((x)-(int)'?') * ((x)-(int)'+') AND_NOT_PATHSEP(x) AND_NOT_DRIVE(x) != 0)
#define NOT_SPECIAL_UNI(x) (NOT_SPECIAL(x) && !Unicode::IsSurrogate(x))

/**
 * For internal use only
 */
class UriUnescapeIteratorBase
{
public:
	/**
	 * Query current status after iteration.
	 * @param illegal_escaping_found set to TRUE if illegal escape sequences were found.
	 * @param utf8_escaping_found set to TRUE if utf8 sequences were found with a ConvertUtf8* flag set
	 */
	void GetStatus(BOOL* illegal_escaping_found, BOOL* utf8_escaping_found)
		{ *illegal_escaping_found = m_illegal_escaping_found; *utf8_escaping_found = m_utf8_escaping_found; }

protected:
	UriUnescapeIteratorBase(const void* str, int len, int unescape_flags)
		: m_str(str)
		, m_length(len >= 0 ? len : INT_MAX)
		, m_unescape_flags(OptimizedFlags(unescape_flags))
		, m_in_query(FALSE)
		, m_illegal_escaping_found(FALSE)
		, m_utf8_escaping_found(FALSE)
		, m_unescape_prevent_count(0)
		, m_unescape_enforce_count(0)
		{}
	virtual ~UriUnescapeIteratorBase() {}

	BOOL IsExcepted(uni_char ch);
	UnicodePoint UnescapeAndAdvance(uni_char current, BOOL output_is_8bit = FALSE);
	static int OptimizedFlags(int unescape_flags);
	virtual uni_char Get(int i) { return (uni_char)((const unsigned char*)m_str)[i]; }
	virtual void Advance(int i) { m_length -= i; m_str = (const char*)m_str + i; }
	virtual BOOL IsNullTerminated() { return TRUE; }

	const void* m_str;
	int m_length;
	const int m_unescape_flags;
	unsigned m_in_query               : 1;
	unsigned m_illegal_escaping_found : 1;
	unsigned m_utf8_escaping_found    : 1;
	unsigned m_unescape_prevent_count : 2;
	unsigned m_unescape_enforce_count : 2;
	static const unsigned char unescape_exception_masks[256]; /* ARRAY OK 2009-04-23 roarl */
};

/**
 * For internal use only
 */
class UriUnescapeIteratorBase_Uni: public UriUnescapeIteratorBase
{
protected:
	UriUnescapeIteratorBase_Uni(const void* str, int len, int unescape_flags)
		: UriUnescapeIteratorBase(str, len, unescape_flags) {}
	virtual uni_char Get(int i) { return ((const uni_char*)m_str)[i]; }
	virtual void Advance(int i) { m_length -= i; m_str = (const uni_char*)m_str + i; }
};

/**
 * Iterator for walking through a null-terminated string and returning
 * characters unescaped or converted according to the unescape flags.
 * @since core-2-3
 */
class UriUnescapeIterator : public UriUnescapeIteratorBase
{
public:
	/**
	 * Create an unescaping iterator
	 * @param str The null-terminated string to iterate through
	 * @param unescape_flags Flags controlling how unescaping is performed
	 */
	UriUnescapeIterator(const char* str, int unescape_flags)
		: UriUnescapeIteratorBase(str, -1, unescape_flags) {}

	/**
	 * Query whether this unescaping iterator has more output
	 * @return TRUE if there is more
	 */
	inline BOOL More() { return *(const char*)m_str != 0; }

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first.
	 * @return The next unescaped character
	 */
	inline char Next()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()

		// The following two lines are just as efficient as *s++
		char ch = *(const char*)m_str;
		m_str = (const char*)m_str + 1;

		return NOT_SPECIAL(ch) ? ch : (char)UnescapeAndAdvance(ch, TRUE);
	}

	/**
	 * @return current iterator position.
	 */
	inline const char *Current() { return (const char *)m_str; }

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first. This version returns
	 * UnicodePoint. Use it when up-converting from char to uni_char, especially
	 * when combined with the ConvertUtf8* flags.
	 * @return The next unescaped unicode character
	 */
	inline UnicodePoint NextUni()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()

		// The following two lines are just as efficient as *s++
		char ch = *(const char*)m_str;
		m_str = (const char*)m_str + 1;

		return NOT_SPECIAL(ch) ? (UnicodePoint)(unsigned char)ch : UnescapeAndAdvance(ch);
	}
};

/**
 * Iterator for walking through a null-terminated unicode string and returning
 * characters unescaped or converted according to the unescape flags.
 * @since core-2-3
 */
class UriUnescapeIterator_Uni : public UriUnescapeIteratorBase_Uni
{
public:
	/**
	 * Create an unescaping iterator
	 * @param str The null-terminated string to iterate through
	 * @param unescape_flags Flags controlling how unescaping is performed
	 */
	UriUnescapeIterator_Uni(const uni_char* str, int unescape_flags)
		: UriUnescapeIteratorBase_Uni(str, -1, unescape_flags) {}

	/**
	 * Query whether this unescaping iterator has more output
	 * @return TRUE if there is more
	 */
	inline BOOL     More() { return *(const uni_char*)m_str != 0; }

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first.
	 * @return The next unescaped character
	 */
	inline UnicodePoint Next()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()

		// The following two lines are just as efficient as *s++
		uni_char ch = *(const uni_char*)m_str;
		m_str = (const uni_char*)m_str + 1;

		return NOT_SPECIAL_UNI(ch) ? UnicodePoint(ch) : UnescapeAndAdvance(ch);
	}
};

/**
 * Iterator for walking through a string of a specific length and returning
 * characters unescaped or converted according to the unescape flags.
 * @since core-2-3
 */
class UriUnescapeIterator_N : public UriUnescapeIteratorBase
{
public:
	/**
	 * Create an unescaping iterator
	 * @param str The string to iterate through (null will not be considered the end of the string)
	 * @param len The length of the string
	 * @param unescape_flags Flags controlling how unescaping is performed
	 */
	UriUnescapeIterator_N(const char* str, int len, int unescape_flags)
		: UriUnescapeIteratorBase(str, len, unescape_flags) { OP_ASSERT(len >= 0); }

	/**
	 * Query whether this unescaping iterator has more output
	 * @return TRUE if there is more
	 */
	inline BOOL More() { return m_length > 0; }

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first.
	 * @return The next unescaped character
	 */
	inline char Next()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()
		m_length--;

		// The following two lines are just as efficient as *s++
		char ch = *(const char*)m_str;
		m_str = (const char*)m_str + 1;

		return NOT_SPECIAL(ch) ? ch : (char)UnescapeAndAdvance(ch, TRUE);
	}

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first. This version returns
	 * uni_char. Use it when up-converting from char to uni_char, especially
	 * when combined with the ConvertUtf8* flags.
	 * @return The next unescaped unicode character
	 */
	inline UnicodePoint NextUni()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()
		m_length--;

		// The following two lines are just as efficient as *s++
		char ch = *(const char*)m_str;
		m_str = (const char*)m_str + 1;

		return NOT_SPECIAL(ch) ? (UnicodePoint)(unsigned char)ch : UnescapeAndAdvance(ch);
	}

protected:
	virtual BOOL IsNullTerminated() { return FALSE; }
};

/**
 * Iterator for walking through a unicode string of a specific length and returning
 * characters unescaped or converted according to the unescape flags.
 * @since core-2-3
 */
class UriUnescapeIterator_Uni_N : public UriUnescapeIteratorBase_Uni
{
public:
	/**
	 * Create an unescaping iterator
	 * @param str The string to iterate through (null will not be considered the end of the string)
	 * @param len The length of the string
	 * @param unescape_flags Flags controlling how unescaping is performed
	 */
	UriUnescapeIterator_Uni_N(const uni_char* str, int len, int unescape_flags)
		: UriUnescapeIteratorBase_Uni(str, len, unescape_flags) { OP_ASSERT(len >= 0); }

	/**
	 * Query whether this unescaping iterator has more output
	 * @return TRUE if there is more
	 */
	inline BOOL     More() { return m_length > 0; }

	/**
	 * Get the next character from the input string, unescaped or converted
	 * according to the unescape flags. You should always check that the
	 * iterator has more data by calling More() first.
	 * @return The next unescaped character
	 */
	inline UnicodePoint Next()
	{
		OP_ASSERT(More()); // You must check with More() before calling Next()
		m_length--;

		// The following two lines are just as efficient as *s++
		uni_char ch = *(const uni_char*)m_str;
		m_str = (const uni_char*)m_str + 1;

		return NOT_SPECIAL_UNI(ch) ? UnicodePoint(ch) : UnescapeAndAdvance(ch);
	}

protected:
	virtual BOOL IsNullTerminated() { return FALSE; }
};

#undef AND_NOT_PATHSEP
#undef AND_NOT_DRIVE
#undef NOT_SPECIAL
#undef NOT_SPECIAL_UNI

#endif // FORMATS_URI_ESCAPE_SUPPORT
#endif // URI_ESCAPE_H
