/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Anders Markussen
 */

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the StringConvert as 
//		 it is used outside of core.
//
/////////////////////////////////////////////////////////////////////////

#ifndef STRING_CONVERT_H
#define STRING_CONVERT_H

namespace StringConvert
{
	/**
	 * Converts from UTF8 to UTF16 encoding
	 *
	 * The return pointer MUST released by the caller with delete []
	 *
	 * @param input: UTF8 encoded string to convert
	 *
	 * @return point to the allocated UTF16 string or NULL if it fails
	 */
	uni_char* UTF16FromUTF8(const char* input);

	/**
	 * Converts from UTF16 to UTF8 encoding
	 *
	 * The return pointer MUST released by the caller with delete []
	 *
	 * @param input: UTF16 encoded string to convert
	 *
	 * @return point to the allocated UTF8 string or NULL if it fails
	 */
	char* UTF8FromUTF16(const uni_char* input);

	
	/**
	 * Converts from CR line feeds to CRLF line breaks
	 *
	 * @param input: UTF16 encoded string to convert
	 *
	 * @param input: size of the source buffer
	 * 
	 * @param output: destination buffer
	 *
	 * @param input: size of the destination buffer
	 *
	 * @return the required size for the destination buffer
	 */
	UINT32 ConvertLineFeedsToLineBreaks(const uni_char* src, UINT32 src_size, uni_char* dst, UINT32 dst_size);

};

#endif // STRING_CONVERT_H
