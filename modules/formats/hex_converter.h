/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HEX_CONVERT_H
#define HEX_CONVERT_H

/**
 * Class facilitating conversion to/from hex (aka base16)
 * @since core-2-5
 */
class HexConverter
{
public:
	/**
	 * Flags controlling how hex conversion is performed
	 */
	enum HexFlags
	{
		UseUppercase		= 0x00,		///< Use uppercase when converting to hex (default)
		UseLowercase		= 0x01,		///< Use lowercase when converting to hex
		NullTerminate		= 0x00,		///< Null-terminate strings in ToHex (default)
		DontNullTerminate	= 0x02,		///< Don't null-terminate strings in ToHex
	};

	static const char hexDigitsUppercase[16]; /* ARRAY OK 2010-04-13 peter */
	static const char hexDigitsLowercase[16]; /* ARRAY OK 2010-04-13 peter */

	/**
	 * Store two characters in the destination buffer, representing the input byte
	 * converted to hex according to the flags.
	 * @param dst Destination buffer, must be 2 chars long,
	 *   or 3 if the NullTerminate flag is set (for this function it is not the default)
	 * @param aByte The source byte to be converted. Only the 8 LSB of the value is used.
	 *   (Using the 'int' type removes the need for casting in most cases)
	 * @param flags Flags controlling how the output is generated
	 * @return The number of characters added to the destination buffer, excluding null-terminator, i.e. 2
	 */
	static int ToHex(char* dst, int aByte, int flags = UseUppercase|DontNullTerminate);

	/**
	 * @overload
	 */
	static int ToHex(uni_char* dst, int aByte, int flags = UseUppercase|DontNullTerminate);

	/**
	 * Store a string in the destination buffer, representing the input data
	 * converted to hex according to the flags.
	 * @param dst Destination buffer, must be large enough to hold the output, i.e. 2*src_len,
	 *   +1 if the NullTerminate flag is set (which is the default)
	 * @param src The source data to be converted
	 * @param src_len The length of the source data in bytes
	 * @param flags Flags controlling how the output is generated
	 * @return The number of characters added to the destination buffer, excluding null-terminator, i.e. 2*src_len
	 */
	static int ToHex(char* dst, const void* src, int src_len, int flags = UseUppercase|NullTerminate);

	/**
	 * @overload
	 */
	static int ToHex(uni_char* dst, const void* src, int src_len, int flags = UseUppercase|NullTerminate);

	/**
	 * Append a string to the destination, representing the input data
	 * converted to hex according to the flags.
	 * @param dst Destination string
	 * @param src The source data to be converted
	 * @param src_len The length of the source data in bytes
	 * @param flags Flags controlling how the output is generated
	 * @return OK if the destination could be expanded without error
	 */
	static OP_STATUS AppendAsHex(OpString8& dst, const void* src, int src_len, int flags = UseUppercase);

	/**
	 * @overload
	 */
	static OP_STATUS AppendAsHex(OpString& dst, const void* src, int src_len, int flags = UseUppercase);

	/**
	 * Decode a byte from the given hex codes
	 * @param c1 first hex code
	 * @param c2 second hex code
	 * @return The decoded byte
	 */
	static unsigned char FromHex(int c1, int c2);

	/**
	 * Decode hex data into the destination buffer. Decoding stops at the first character pair
	 * that does not contain two hex digits ('0'-'9', 'a'-'f', 'A'-'F').
	 * @param dst Destination buffer, must be large enough to hold the output
	 * @param src The source string to be decoded
	 * @param src_len The length of the source string, or KAll if the source string is null-terminated
	 * @return The number of bytes added to the destination buffer
	 */
	static int FromHex(void* dst, const char* src, int src_len = KAll);

	/**
	 * @overload
	 */
	static int FromHex(void* dst, const uni_char* src, int src_len = KAll);
};

#endif // HEX_CONVERT
