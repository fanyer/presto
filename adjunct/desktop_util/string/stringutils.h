/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef STRINGUTILS_H
#define STRINGUTILS_H

class DesktopMenuContext;
class DesktopWindow;
class OpWindowCommander;

#include "modules/xmlutils/xmltypes.h"

// NB NB: Place all new functions inside the StringUtils namespace in this file.

//
//	flags for StrFormatByteSize()
//
#define SFBS_DETAILS		0x01
#define SFBS_FORCEKB		0x02
#define SFBS_NOCOMMA		0x04
#define SFBS_ABBRIVATEBYTES	0x08	// Abbrevaite "bytes" with "B" (does not apply to details)
//..
#define SFBS_DEFAULT	0x00


//
//	flags for FormatTimeStandard
//
enum TimeFormat
{
	TIME_FORMAT_DEFAULT,
	TIME_FORMAT_ONLY_TIME,
	TIME_FORMAT_WEEKDAY_AND_TIME,
	TIME_FORMAT_WEEKDAY_AND_DATE,
	TIME_FORMAT_UNIX_FORMAT
};

/**
 *
 *
 * @param buffer    -
 * @param byte_size -
 * @param flags     -
 *
 * @return
 */
void StrFormattedByteSize( OpString &buffer, OpFileLength byte_size, UINT32 flags );


/**
 *
 *
 * @param buffer -
 * @param nBytes -
 * @param flags  -
 *
 * @return
 */
uni_char * StrFormatByteSize(OpString &buffer, OpFileLength nBytes, unsigned int flags);


/**
 *
 *
 * @param szBuf  -
 * @param max    -
 * @param nBytes -
 * @param flags  -
 *
 * @return
 */
uni_char * StrFormatByteSize(uni_char *szBuf, size_t max, OpFileLength nBytes, unsigned int flags);


/**
 *
 *
 * @param szBuf  -
 * @param nBytes -
 * @param flags  -
 *
 * @return
 */
uni_char * StrFormatByteSize(uni_char *szBuf, OpFileLength nBytes, unsigned int flags);


/**
 *
 *
 * @param nNum_64 -
 * @param szBuf   -
 *
 * @return
 */
uni_char* LongToStr(UINT64 nNum_64, uni_char* szBuf);


/**
 *
 *
 * @param dst -
 * @param len -
 * @param src -
 *
 * @return
 */
unsigned int EscapeAmpersand( uni_char *dst, unsigned int len, const uni_char *src );


/**
 * Extracts an exerpt of maximum length "length" from haystack containing some or all the
 * words in needles
 *
 * @param excerpt  - the resulting excerpt from haystack
 * @param needles  - the words to be searched for
 * @param haystack - the text to be searched
 * @param length   - the maximum length of the excerpt
 *
 * @return OpStatus::OK if successful
 */
OP_STATUS ExtractExcerpt(OpString & excerpt, OpVector<OpString> & needles, OpString & haystack, int length);


/**
 * Extracts an exerpt of maximum length "length" from haystack containing some or all the
 * words in needles and italicises all found words in the excerpt.
 *
 * @param excerpt  - the resulting excerpt from haystack
 * @param words    - the words to be searched for
 * @param haystack - the text to be searched
 * @param length   - the maximum length of the excerpt
 *
 * @return OpStatus::OK if successful
 */
OP_STATUS ExtractFormattedExcerpt(OpString & excerpt, OpVector<OpString> & words, OpString & haystack, int length);


/**
 * Removes the first layer of quotes in a string
 *
 * @param input - the string to be converted
 *
 * @return OpStatus::OK if successful
 *
 * WARNING: This function removes unbalanced quotes and some
 * internal quotes:
 * 'This is "a test' -> 'This is a test'
 * '"This is a test' -> 'This is a test'
 * 'This "is" a test' -> 'This is a test'
 *
 * consider Stringutils::Unquote() instead
 */
OP_STATUS UnQuote(OpString &input);


// Use StringUtils::StartsWith() instead
COREAPI_DEPRECATED(BOOL StartsWith(const uni_char * text, const uni_char * possible_start, BOOL case_insensitive));

/**
 *
 *
 * @param haystack -
 * @param needle   -
 *
 * @return
 */
uni_char* StrRevStr(const uni_char* haystack, const uni_char* needle);


/**
 * Takes a time struct and creates a string from it. The format of the
 * string is specified in a separate parameter.
 *
 * @param result - where the time string should be placed
 * @param format - string specifying the format of the conversion
 * @param time   - the time struct to be converted
 *
 * @return OpStatus::OK if successful
 */
OP_STATUS FormatTime(OpString& result, const uni_char* format, time_t time);


/**
 * Takes a time struct and creates a human friendly string from it.
 *
 * @param result - where the human friendly time string should be placed
 * @param time   - the time struct to be converted
 * @param time_format - time string format
 *
 * @return OpStatus::OK if successful
 */
OP_STATUS FormatTimeStandard(OpString& result, time_t time, int time_format = TIME_FORMAT_DEFAULT);


/**
 * Creates a human-friendly string for a number of bytes
 *
 * @param result - where the human friendly time string should be placed
 * @param bytes  - the number of bytes
 *
 * @return OpStatus::OK if successful
 */
OP_STATUS FormatByteSize(OpString& result, OpFileLength bytes);


/** Converts a string of concatenated 8-bit hexadecimal numbers to an array of bytes
 *     Example: 010B16 becomes {1,11,22}
 *
 * @param hex_string  - String to convert
 * @param byte_buffer - Where the array of bytes should be allocated
 * @param buffer_len  - Length of the allocated array
 *
 * @return
 */
OP_STATUS HexStrToByte(const OpStringC8& hex_string, BYTE*& byte_buffer, unsigned int& buffer_len);


/** Converts an array of bytes to a string of concatenated 8-bit hexadecimal numbers
 *     Example: {1,11,22} becomes 010B16
 *
 * @param byte_buffer - Array of bytes to convert
 * @param buffer_len  - Length of the array
 * @param hex_string  - Result of the conversion
 *
 * @return
 */
OP_STATUS ByteToHexStr(const BYTE* byte_buffer, unsigned int buffer_len, OpString8& hex_string);

unsigned int CalculateHash(const char* text, unsigned int hash_size);
unsigned int CalculateHash(const OpStringC8& text, unsigned int hash_size);


/** Decodes base64 encoded ASCII data into binary.
  * Caller is responsible for releasing the buffer
  *
  * @param data Base64 encoded ASCII input.
  * @param [out] output Pointer to the result when return, owned by caller!!
  * @param [out] len Size of the result
  */
OP_STATUS DecodeBase64(OpStringC8 data, unsigned char*& output, unsigned long& len);

namespace StringUtils
{
	/*!	A function that should really be in OpString. It will find the
	position of a matching substring, just like Find() in OpString does,
	but here you may also pass a starting index of where to search from.
	 */
	int Find(const OpStringC& find_in, const OpStringC& find_what,
			 UINT find_from = 0);

	/*! FindLastOf in OpString will only let you specify one character, not
	several.
	 */
	int FindLastOf(const OpStringC& find_in,
				   const OpStringC& find_what);

	/**
	 * Tests wether all of the words in needles (not respecting order) are present in haystack.
	 *
	 * @param needles  - the words that are to be searched for
	 * @param haystack - the string that is searched
	 *
	 * @return TRUE if all of the words in needles are present in haystack
	 */
	BOOL ContainsAll(const OpStringC& needles, const OpStringC& haystack);

	/*!	A function for replacing content in a string. */
	OP_STATUS Replace(OpString &replace_in,
		const OpStringC& replace_what, OpStringC const &replace_with);
	
	/*!	A function for replacing \n and \r\n with <br> */
	OP_STATUS ReplaceNewLines(OpString &replace_in);

	/** Check whether a string starts with another (literal) string
	  * @param string String to check
	  * @param prefix Prefix to check
	  * @return Whether string starts with prefix
	  */
	template<size_t N> inline bool StartsWith(const OpStringC8& string, const char (&prefix)[N])
		{ return string.IsEmpty() ? N <= 1 : op_strncmp(string.CStr(), prefix, N - 1) == 0; }
	template<size_t N> inline bool StartsWith(const OpStringC& string, const uni_char (&prefix)[N])
		{ return string.IsEmpty() ? N <= 1 : uni_strncmp(string.CStr(), prefix, N - 1) == 0; }

	/*! Find the number located in a string, starting at a given position.
	Example: If the string is "Foo123Bar" and find_from is 3, it will
	return 123.
	 */
	UINT NumberFromString(const OpStringC& find_in,
						  UINT find_from = 0);

	/*! Determine whether a string is a number. */
	BOOL IsNumber(const OpStringC& input);

	/*! A strip function that let's you strip any kind of character. */
	OP_STATUS Strip(OpString& to_strip,
					const OpStringC& strip_characters = UNI_L(" "),
					BOOL strip_leading = TRUE,
					BOOL strip_trailing = TRUE);
	
	/*! Reads a const string, and copies only valid characters to the out name */
	OP_STATUS StripToValidXMLName(XMLVersion version, const uni_char* in_name, OpString& out_name);

	/*! Count number of occurances of one string inside another. */
	UINT SubstringCount(const OpStringC& input,
						const OpStringC& substring);

	/*! Find the nth occurance of a substring. */
	int FindNthOf(UINT n,
				  const OpStringC& input,
				  const OpStringC& substring);

	/*! Convert escaped data (%45) to readable text */
	OP_STATUS DecodeEscapedChars(const uni_char* in, 
								 OpString& result, 
								 BOOL hex_only);

	/**
	 * Escape a string so that it can be used as a URI attribute value
	 * e.g. as {value} in http://www.test.com/test.html?name={value}
	 *
	 * This replaces special characters that can't be used in an attribute
	 * value with their escaped versions, e.g. "?" becomes "%3F"
	 *
	 * @param value Value to escape
	 * @param escaped_value On success contains an escaped version of value
	 */
	OP_STATUS EscapeURIAttributeValue(const OpStringC8& value, OpString8& escaped_value);
	
	/**
	 * Escape a string so that it can be used as a URI attribute value
	 * e.g. as {value} in http://www.test.com/test.html?name={value}
	 *
	 * This replaces special characters that can't be used in an attribute
	 * value with their escaped versions, e.g. "?" becomes "%3F"
	 *
	 * @param value Value to escape
	 * @param escaped_value On success contains an escaped version of value
	 */
	OP_STATUS EscapeURIAttributeValue16(const OpStringC& value, OpString& escaped_value);

	/**
	 * Splits a string into words based on a specified separator, but respecting quotes
	 *
	 * @param list         - the vector into which the words are inserted
	 * @param candidate    - the string to be split
	 * @param sep          - the seperator on which to split
	 * @param keep_strings -
	 */
	OP_STATUS SplitString( OpVector<OpString>& list, const OpStringC &candidate, int sep, BOOL keep_strings=FALSE );

	OP_STATUS AppendToList( OpVector<OpString>& list, const uni_char *str, int len = KAll );

	/**
	 * Finds the domain (with the subdomain) in the given address. If address begins with "www." it is not treated
	 * as a part of the domain.
	 * Examples:
	 * address                            => search result
	 * www.google.pl/something            => google.pl
	 * https://mail.opera.com/?utm_source => mail.opera.com
	 * http://my.opera.com/community      => my.opera.com
	 *
	 * @param [in] address address which should be looked for the domain
	 * @param [out] domain the variable where domain is written (if found)
	 * @param [out] starting_position index of the first letter of the domain (if found)
	 *
	 * @return status of the operation
	 */
	OP_STATUS FindDomain(const OpStringC& address, OpString& domain, unsigned& starting_position);

	OP_STATUS AppendHighlight(OpString& input, const OpStringC& search_terms);

	/**
	 * Appends color tags to given string.
	 *
	 * @param [in] input string which should be colored
	 * @param [in] color decimal value of the color (should be calculated by macro OP_RGB(red, green, blue))
	 * @param [in] starting_pos position of the first letter to be colored
	 * @param [in] length length of substring which should be colored.
	 *             Note: user is responsible for insuring that colored substring does not exceed the string
	 *
	 * @return status of the operation
	 */
	OP_STATUS AppendColor(OpString& input, COLORREF color, unsigned starting_pos, unsigned length);

	OP_STATUS RemoveHighlight(OpString& input);

	/**
	 * Test if a string starts with a sub string
	 *
	 * @param text String to text
	 * @param candidate The sub string
	 * @param case_insensitive Do a case insensitive test if TRUE
	 *
	 * @return TRUE on a match, otherwise FALSE. FALSE is returned if any of
	 *         the strings are empty
	 */
	BOOL StartsWith(const OpStringC& text, const OpStringC& candidate, BOOL case_insensitive);

	/**
	 * Test if a string ends with a sub string
	 *
	 * @param text String to text
	 * @param candidate The sub string
	 * @param case_insensitive Do a case insensitive test if TRUE
	 *
	 * @return TRUE on a match, otherwise FALSE. FALSE is returned if any of
	 *         the strings are empty
	 */
	BOOL EndsWith(const OpStringC& text, const OpStringC& candidate, BOOL case_insensitive);

	/**
	 * Remove a double quote (") pair. The quotes must start and end the string.
	 * Should the string contain space characters before the leading quote and/or 
	 * after the trailing quote the string is not unquoted.
	 *
	 * This function can not fail. Memory is not allocated.
	 *
	 * @param text The string to unquote
	 */
	void Unquote(OpString& text);

	/**
	 * Gets the first word in a string based on a specified separator, respecting quotes
	 * @param[in] string String to search
	 * @param[in] separator Separator on which to split - should not be '\0'
	 * @param[out] word First word
	 * @return Tail of the string after the first word and the separator, or NULL if no word could be found
	 *         If no word could be found, the contents of 'word' are undefined
	 *
	 * @example Splitting Split string 'src' on whitespace:
	 * OpString word;
	 * while ((src = SplitWord(src, ' ', word))) { // do stuff with word }
	 *
	 * @note This function sees quoted strings as whole words, so the string '"test with quotes" test without'
	 * split on spaces will give results 'test with quotes', 'test' and 'without'.
	 */
	const uni_char* SplitWord(const uni_char* string, uni_char separator, OpString& word);

	/**
	 * Converts a input unicode string to a UTF8. This uses core so it's a much more lightweight
	 * version of the method in string_convert.cpp. 
	 *
	 * @param uni_string Text to convert to UTF8. NULL is a valid string in which case
	 *             the return value will be NULL.
	 *
	 * @return A UTF8 string or NULL on OOM or when the input string is NULL. It is the caller's
	 *         responsibility to free the returned string using op_free().
	 */
	char* UniToUTF8(const uni_char *uni_string);

	/**
	 * Truncates text so that is at most max_width pixels width. Removed truncated text 
	 * gets replaced with "..." either in the center of at the end of the modified string
	 *
	 * @param text Text to modify
	 * @param max_width Maximum allowed with in pixels
	 * @param center Place ellipsis in center if TRUE, otherwise at end
	 * @param font Font to be used when computing width
	 *
	 * @return OpStatus::OK, otherwise an error code describing the error
	 */
	OP_STATUS EllipticText( OpString& text, UINT32 max_width, BOOL center, class OpFont* font );

	/**
	 * @return str.CStr() if non null, else UNI_L("")
	 */
	inline const uni_char* GetNonNullString(const OpStringC& str) { return str.CStr() ? str.CStr() : UNI_L(""); }
	inline const char*     GetNonNullString(const OpStringC8& str) { return str.CStr() ? str.CStr() : ""; }

	/** Create a unique ID string with hexadecimal characters
	  * @param outstr Output
	  */
	OP_STATUS GenerateClientID(OpString& outstr);

	/** @return The default writing system in use for the current translation
	  */
	WritingSystem::Script GetDefaultWritingSystem();

	class ExpansionProvider
	{
	public:
		virtual ~ExpansionProvider() {}

		virtual OP_STATUS GetClipboardText(OpString& text) const = 0;
		virtual OP_STATUS GetSelectedText(OpString& text) const = 0;
		virtual OP_STATUS GetCurrentDocFilePath(OpString& path) const = 0;
		virtual OP_STATUS GetCurrentDocUrlName(OpString& name) const = 0;
		virtual OP_STATUS GetCurrentClickedUrlName(OpString& name) const = 0;
	};

	class ExpansionResult
	{
	public:
		virtual ~ExpansionResult() {}

		// Called when expansionprovide encounters a normal character
		virtual OP_STATUS WriteRawChar(uni_char c) = 0;

		// This is a expanded argument
		virtual OP_STATUS WriteArgument(OpStringC arg) = 0;

		// We're done, no more input coming
		virtual OP_STATUS Done() = 0;
	};

	/**
	 * Changes '%%' to '%'
	 * Changes '%[CSLUT]' to '"<expansion>"', '%[cslut]' to '<expansion>'
	 * Changes '%x' where x is any other character, to ''
	 *
	 * @param expander supplies the function with all the possible expansions
	 * @param parameters In string
	 * @param expansion_result where we output the result
	 */
	OP_STATUS ExpandParameters(const ExpansionProvider& expander,
			const uni_char* parameters, ExpansionResult& expansion_result);

	/**
	 * This function use default expander and output to string
	 * @param can_expand_environment_variables If @c TRUE, will call
	 * 		opsysteminfo::ExpandSystemVariablesInString
	 * @param expanded_parameters On return, holds the expanded version of
	 * 		string 'parameters'
	 */
	OP_STATUS ExpandParameters(OpWindowCommander* window_commander
			, const uni_char* parameters
			, OpString& expanded_parameters
			, BOOL can_expand_environment_variables
			, DesktopMenuContext* menu_context = NULL
	);

	// This function use default expander and output to ExpansionResult
	OP_STATUS ExpandParameters(OpWindowCommander* window_commander
		, const uni_char* parameters
		, ExpansionResult& expanded_parameters
		, DesktopMenuContext* menu_context = NULL
	);

	/**
	 * Load language string that has printf-style formatting
	 * @param output Where to place the result
	 * @param string_id Language string
	 * @param ... printf arguments
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS GetFormattedLanguageString(OpString& output, Str::LocaleString string_id, ...);

#ifdef CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

	/**
	 *	- Calculate a MD5 checksum out of a string
	 *	@param str is the string you want to calculate the check sum on
	 *  @param md5_hash will be the final result of the calculation if 
	 *			the function scucceeds. 
	 *	@return OpSusscces if function succeeds, or ERR or ERR_NO_MEMORY
	 *			under different error conditions. md5_hash will only 
	 *			contain a valid hash if the function succeeds. 
	 */
	 OP_STATUS CalculateMD5Checksum(OpString8& str, OpString8& md5_hash);

#endif // CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

	class StringExpansionResult : public ExpansionResult
	{
	public:
		StringExpansionResult(OpString& result):m_result(result){}

		virtual OP_STATUS WriteRawChar(uni_char c)
		{
			return m_result.Append(&c, 1);
		}
		virtual OP_STATUS WriteArgument(OpStringC arg)
		{
			return m_result.Append(arg);
		}
		virtual OP_STATUS Done() {return OpStatus::OK;}

	private:
		OpString& m_result;
	};

	class ArrayExpansionResult : public ExpansionResult
	{
	public:
		ArrayExpansionResult(BOOL expand_environment_variables)
			:m_argv(NULL)
			,m_expand_environment_variables(expand_environment_variables)
			{}

		virtual ~ArrayExpansionResult();

		virtual OP_STATUS WriteRawChar(uni_char c);
		virtual OP_STATUS WriteArgument(OpStringC arg);
		virtual OP_STATUS Done();

		OP_STATUS GetResult(int& argc, const char**& argv);

	private:
		OP_STATUS SaveArg();

		OpAutoVector<OpString8> m_args;
		OpString m_current_arg;
		const char** m_argv;
		BOOL m_expand_environment_variables;
	};

	class DefaultExpansionProvider : public ExpansionProvider
	{
	public:
		DefaultExpansionProvider(DesktopMenuContext* menu_context,
								 OpWindowCommander* w_commander)
			: m_menu_context(menu_context),
			  m_window_commander(w_commander)
			{}

		virtual OP_STATUS GetClipboardText(OpString& text) const;
		virtual OP_STATUS GetSelectedText(OpString& text) const;
		virtual OP_STATUS GetCurrentDocFilePath(OpString& path) const;
		virtual OP_STATUS GetCurrentDocUrlName(OpString& name) const;
		virtual OP_STATUS GetCurrentClickedUrlName(OpString& name) const;

	private:
		DesktopMenuContext* m_menu_context;
		OpWindowCommander* m_window_commander;
	};
}

#endif // STRINGUTILS_H
