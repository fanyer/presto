/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef UPLOAD_ENCODER_H
#define UPLOAD_ENCODER_H

/**
 * Count the characters in a string that needs to be escaped when the string
 * is converted to quoted printable, according to
 * NeedQPEscape(c,MIME_NOT_CLEANSPACE).
 * 
 * @param str String to be counted
 * @param len Length of str
 * @return The number of escaped characters
 */
int CountQPEscapes(const char* str, int len);

/**
 * Query if a character needs to be escaped in a quoted printable string.
 * Escaped characters are: 0..8, 11, 12, 14..31, 61 ('='), 127..
 * 
 * @param c character
 * @param flags Logical "or" of the following flags.
 *     MIME_NOT_CLEANSPACE: escape also ' ' (32), '?' (63) and '_' (95).
 *     MIME_IN_COMMENT: escape also '\\t' (9), '\\r' (10) and '\\n' (13).
 *     MIME_8BIT_RESTRICTION: dont escape c>=128.
 * @return TRUE if character needs to be escaped
 */
BOOL NeedQPEscape(const uni_char c, unsigned flags);

/**
 * Encode binary data into base64 encoded unicode ASCII and other formats.
 * 
 * <p><em>NOT IMPLEMENTED</em>.
 * 
 * @param target (output) This pointer is set to a buffer allocated using
 *     "new[]", containing the encoded data. If the encoding type is not a
 *     binary format (i.e. MAIL_BINARY or NO_ENCODING), the buffer will
 *     also contain a terminating null character. If the pointer is not NULL
 *     on entry, the previous pointer is deallocated using "delete[]". If
 *     targetlen equals 0, target will be set to NULL.
 * @param targetlen (output) Set to the length of the allocated target buffer.
 * @param source Binary input.
 * @param sourcelen Number of bytes in source. 
 * @param charset <em>ignored</em>.
 * @param encoding Chosen encoding format.
 * @return Error code if something went wrong or MIME_NO_ERROR
 */
MIME_Encode_Error MIME_Encode_SetStr(uni_char *&target, int &targetlen, const uni_char *source, int sourcelen, 
		const char *charset, Mime_EncodeTypes encoding);

/**
 * Encode binary data into base64 encoded ASCII and other formats.
 * 
 * @param target (output) This pointer is set to a buffer allocated using
 *     "new[]", containing the encoded data. If the encoding type is not a
 *     binary format (i.e. MAIL_BINARY or NO_ENCODING), the buffer will
 *     also contain a terminating null character. If the pointer is not NULL
 *     on entry, the previous pointer is deallocated using "delete[]". If
 *     targetlen equals 0, target will be set to NULL.
 * @param targetlen (output) Set to the length of the allocated target buffer.
 * @param source Binary input.
 * @param sourcelen Number of bytes in source. 
 * @param charset <em>ignored</em>.
 * @param encoding Chosen encoding format.
 * @return Error code if something went wrong or MIME_NO_ERROR
 */
MIME_Encode_Error MIME_Encode_SetStr(char *&target, int &targetlen, const char *source, int sourcelen, 
		const char *charset, Mime_EncodeTypes encoding);

/**
 * Get string containing the hexadecimal characters.
 * @return "0123456789ABCDEF"
 */
const char *GetHexChars();

#endif // UPLOAD_ENCODER_H
