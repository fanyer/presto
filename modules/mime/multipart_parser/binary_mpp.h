/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johan Herland
**
*/

#ifndef BINARY_MPP_H
#define BINARY_MPP_H

#ifdef WBMULTIPART_MIXED_SUPPORT

#include "modules/mime/multipart_parser/abstract_mpp.h" // AbstractMultiPartParser

class StringBuffer;

/** Binary Multipart Parser. Performs low-level parsing of binary multipart documents. */
class BinaryMultiPartParser : public AbstractMultiPartParser {
public:

/* Public interface */

	/** Constructor.
	 *
	 * @param listener (optional) This object will notified about any events, e.g. warnings that occur during multipart parsing.
	 */
	BinaryMultiPartParser (MultiPartParser_Listener* listener = 0);

	/// Destructor.
	virtual ~BinaryMultiPartParser ();

/* The rest of the public interface is inherited from AbstractMultiPartParser. */

protected:

/* Private subclasses and types */

	/// Return type used to indicate results in subparser methods.
	typedef enum {
		PARSE_RESULT_OK,         ///< Parsing successful.
		PARSE_RESULT_SKIPPED,    ///< Parsing unsuccessful because of unrecognized/unsupported value. However, the value length was recognized, and offset was updated.
		PARSE_RESULT_MORE_DATA,  ///< Need more data in buffer to parse successfully. Retry later.
		PARSE_RESULT_FAILURE,    ///< Parsing failed.

		/// The following values are only returned by parseToken()
		PARSE_RESULT_IS_LENGTH,  ///< A length value was found and has been stored in the value output parameter.
		PARSE_RESULT_IS_STRING,  ///< A string value was found. NOTHING has been stored to the value output parameter, and offset has NOT been updated.
		PARSE_RESULT_IS_WKNOWN   ///< A well-known 7-bit encoded value was found, and its value (i.e. minus the 8th bit) has been stored in the value output parameter.
	} ParseResult;

/* Private constants */

	const char* const* WellknownHeaderName;       ///< Lookup array of Well-known header names.
	unsigned int MaxWellknownHeaderName;          ///< Maximum index in the above array.

	const char* const* WellknownCacheControl;     ///< Lookup array of Well-known Cache-Control values.
	unsigned int MaxWellknownCacheControl;        ///< Maximum index in the above array.

	const char* const* WellknownContentType;      ///< Lookup array of Well-known Content-Type values.
	unsigned int MaxWellknownContentType;         ///< Maximum index in the above array.

	const char* const* WellknownParameter;        ///< Lookup array of Well-known Parameters.
	unsigned int MaxWellknownParameter;           ///< Maximum index in the above array.

/* Private methods */

	/** Convenience function that returns true iff parsing can NOT continue after receiving the given ParseResult
	 *
	 * @param result The ParseResult received from the last parse operation
	 * @return TRUE iff the result indicates that one may NOT continue parsing from the current offset immediately because the parser/document is in an invalid state.
	 */
	virtual BOOL stopParsing (ParseResult result) { return (result == PARSE_RESULT_MORE_DATA || result == PARSE_RESULT_FAILURE); }

	/** Try to parse the next logical chunk of data.
	 *
	 * @return TRUE iff something was successfully parsed and parserOffset was updated; FALSE otherwise.
	 */
	virtual BOOL parse ();

/* Subparser methods. The following methods should only be called from parse() or eachother. */

	/** Parse the main header of the multipart document.
	 *
	 * parserOffset must point to the start of the multipart document.
	 *
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseMultipartHeader ();

	/** Parse the header portion of the current part in the multipart document.
	 *
	 * parserOffset must point to the start of the current part.
	 *
	 * @return (see ParseResult)
	 */
	virtual ParseResult parsePartHeader ();
	
	/** Parse the content portion of the current part in the multipart document.
	 *
	 * parserOffset must point to somewhere in the content portion of the current part.
	 *
	 * @return (see ParseResult)
	 */
	virtual ParseResult parsePartContent ();

	/** Parse the Content-Type value starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the Content-Type value.
	 * If parsing fails, offset is not incremented and nothing is added to the header list.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param headers List of headers for the current part. The parsed Content-Type will be added to this header list.
	 * @param maxLength The maximum number of bytes (starting from the given offset) in the raw multpart data that may contain Content-Type.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseContentType (unsigned int& offset, HeaderList& headers, unsigned int maxLength);

	/** Parse the headers starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed headers.
	 * If parsing fails, offset is not incremented and nothing is added to the header list.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param headers List of headers for the current part. The parsed headers will be added to this header list.
	 * @param length The number of bytes (starting from the given offset) in the raw multpart data that contains headers.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseHeaders (unsigned int& offset, HeaderList& headers, unsigned int length);

	/** Parse the header starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed header.
	 * If parsing fails, offset is not incremented.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param maxLength If no end point is found within this #bytes from offset, return PARSE_RESULT_FAILURE.
	 * @param headers The parsed header is added to this header list.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseHeader (unsigned int& offset, unsigned int maxLength, HeaderList& headers);

	/** Parse the header value (in the context of the given well-known header name) starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed header.
	 * If parsing fails, offset is not incremented.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param nameToken Well-known header name token (valid index into WellknownHeaderName[]).
	 * @param maxLength If no end point is found within this #bytes from offset, return PARSE_RESULT_FAILURE.
	 * @param value This parameter will be assigned a pointer to the start of the parsed header value.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseWellknownHeaderValue (unsigned int& offset, unsigned int nameToken, unsigned int maxLength, StringBuffer& value);

	/** Parse the composite header value (in the context of the given well-known header name) of the given length starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed header.
	 * If parsing fails, offset is not incremented.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param nameToken Well-known header name token (valid index into WellknownHeaderName[]).
	 * @param length The length of the composite header value.
	 * @param value This parameter will be assigned a pointer to the start of the parsed header value.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseWellknownCompositeHeaderValue (unsigned int& offset, unsigned int nameToken, unsigned int length, StringBuffer& value);

	/** Parse the token of the same name as the method, as defined in the WSP specification, starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed header.
	 * If parsing fails, offset is not incremented.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param length/maxLength The (maximum) length of the WSP token (#bytes in buffer starting at offset).
	 * @param value This parameter will be assigned a pointer to the start of the parsed header value.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseMediaType        (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseParameter        (unsigned int& offset, unsigned int maxLength,                          StringBuffer& value);
	virtual ParseResult parseCacheDirective   (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseUntypedValue     (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseTypedValue       (unsigned int& offset, unsigned int    length, unsigned int paramToken, StringBuffer& value);
	virtual ParseResult parseFieldname        (unsigned int& offset, unsigned int maxLength,                          StringBuffer& value);
	virtual ParseResult parseWellknownCharset (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseDateValue        (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseString           (unsigned int& offset, unsigned int maxLength,                          StringBuffer& value);
	virtual ParseResult parseMultipleOctets   (unsigned int& offset, unsigned int    length,                          StringBuffer& value);
	virtual ParseResult parseWellknownParam   (unsigned int paramToken, unsigned int& offset, unsigned int length,    StringBuffer& value);

	/** Parse the Multi-octet-integer consisting of the given number of octets starting at the given offset.
	 *
	 * The given offset is incremented to the position past the the end of the parsed value.
	 * If parsing fails, offset is not incremented.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param length Number of octets to parse.
	 * @param value This parameter will be assigned a the parsed value of the Multi-octet-integer.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseMultiOctetInteger (unsigned int& offset, unsigned int length, unsigned long& value);

	/** Parse the variable-length unsigned integer starting at the given offset and store the parsed value into the given value parameter.
	 *
	 * The given offset is incremented to the position past the the end of this uintvar.
	 * If parsing fails, offset is not incremented and nothing is stored to the value parameter.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param value The parsed value is stored into this parameter.
	 * @return (see ParseResult)
	 */
	virtual ParseResult parseUintvar (unsigned int& offset, unsigned int& value);

	/** Parse the token starting at the given offset and store the parsed value into the given value parameter.
	 *
	 * The token starting at offset is parsed according to the following rules (parser result in parentheses):
	 * If first byte is
	 *   < 31:          Length value that will be stored into value (PARSE_RESULT_IS_LENGTH).
	 *   == 31:         Following bytes constitute a uintvar length value that will be stored into value (PARSE_RESULT_IS_LENGTH).
	 *   > 31 && < 128: Text string that will NOT be parsed. offset is reset and nothing is stored into value (PARSE_RESULT_IS_STRING).
	 *   >= 128:        (Hopefully) well-known 7-bit encoded value whose value (not including the 8th bit) is stored into value (PARSE_RESULT_IS_WKNOWN).
	 *
	 * The given offset is incremented to the position past the the end of the parsed token.
	 * If parsing fails, offset is not incremented and nothing is stored to the value parameter.
	 *
	 * @param offset offset into buffer at which point parsing should start. This is updated iff parsing is succssful.
	 * @param value The parsed value, if applicable, is stored into this parameter.
	 * @return (see ParseResult, especially PARSE_RESULT_IS_LENGTH, PARSE_RESULT_IS_STRING and PARSE_RESULT_IS_WKNOWN)
	 */
	virtual ParseResult parseToken (unsigned int& offset, unsigned int& value);

	/** Convert the given well-known header value token (interpreted in the context of the given well-known header name token) to a string.
	 *
	 * @param nameToken Well-known header name token value between 0 and 127.
	 * @param valueToken Well-known header name token value between 0 and 127.
	 * @return The string corresponding to valueToken interpreted in the context of nameToken, or NULL if no conversion could be found.
	 */
	virtual const char *getSingleWellknownHeaderValue(unsigned int nameToken, unsigned int valueToken);


/* Member variables */

	unsigned int contentLength;   ///< Remaining #bytes in the content portion of the current part.
};


/** Holds a pointer to a string, and its length. The string may be allocated locally (in mem), or it may be owned by someone else. */
class StringBuffer {
public:
	/// Constructor. Create an empty string segment.
	StringBuffer () : s(0), s_len(0), mem(0), mem_len(0) {}

	/// Constructor. Create a string segment pointing to the given null-terminated string.
	StringBuffer (const char *string) : s(string), s_len(0), mem(0), mem_len(0) {
		if (s) s_len = (unsigned int)op_strlen(s);
		mem_len = s_len + 1; // Include null-terminator
	}

	/** Constructor. Create a string segment pointing to outside memory with the
	 *  given length, and (optionally) the given length of valid data in the
	 *  buffer pointed to by s.
	 */
	StringBuffer (const char *string, unsigned int length, unsigned int memLength = 0) : s(string), s_len(length), mem(0), mem_len(memLength) {
		if (mem_len < s_len) mem_len = s_len;
	}

	/// Destructor. Delete internal memory, if allocated.
	~StringBuffer () { if (mem) OP_DELETEA(mem); }

	/// Return TRUE iff this string is empty.
	BOOL empty () const { return s == 0 || s_len == 0; }

	/// Return pointer to string data.
	const char *string () const { return s; }

	/// Return length of string.
	unsigned int length () const { return s_len; }

	/// Return TRUE iff this object owns its own string.
	BOOL owned () const { return mem != 0; }

	/// Transfer ownership for the given string into this object. It is now the responsibility of this object to delete[] the memory allocated to the given string.
	void attach (char *string, unsigned int length, unsigned int memLength = 0);

	/// Transfer ownership for the string out of this object. It now becomes the responsibility of the caller to delete[] the returned pointer.
	char *release ();

	/// Append the first length chars of the given string to this object. This object will be reallocated if necessary.
	BOOL append (const char *string, unsigned int length, unsigned int memLength = 0);

	/// Append the given null-terminated string to this object. This object will be reallocated if necessary.
	BOOL append (const char *string) { return append(string, (unsigned int)op_strlen(string)); }

	/// Append the given StringBuffer's string to this object. This object will be reallocated if necessary.
	BOOL append (const StringBuffer& sb) { return append(sb.string(), sb.length()); }

	/// Ensure that the string s is null-terminated. This may cause a reallocation of the string.
	BOOL nullTerminate ();

	/// Ensure that this object owns its string, and that (if given) there are at least 'size' #bytes allocated to it.
	BOOL takeOwnership (unsigned int size = 0);

private:
	const char *s;        ///< The string pointed to by this object.
	unsigned int s_len;   ///< The length of the above string.
	char *mem;            ///< Pointer to memory owned by this object. It's this object's responsibility to delete[] this memory.
	unsigned int mem_len; ///< Length of allocated memory iff owned, otherwise length of known valid data in the buffer pointed to by s. mem_len >= s_len.
};

#endif // WBMULTIPART_MIXED_SUPPORT

#endif // BINARY_MPP_H
