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

#ifndef URL2_ENCODER_H
#define URL2_ENCODER_H

MIME_Encode_Error MIME_Encode_SetStr(char *&target, int &targetlen, const char *source, int sourcelen, 
		const char *charset, Mime_EncodeTypes encoding);

extern const char Base64_Encoding_chars[];
extern const unsigned char Base64_decoding_table[128]; /* ARRAY OK 2009-04-23 roarl */

/**
 * Decodes base64 encoded ASCII data into binary.
 * 
 * <p>For large inputs, two modes of partial decoding are supported:
 * <ul>
 *   <li>If max_write_len is not 0, decoding stops at the input-quad (4 input
 *       characters) where the resulting 3 output-bytes would exceed the
 *       max_write_len limit. This means that there may be up to 2 bytes less
 *       than max_write_len in the target output buffer.
 *   <li>With an additional decode_buf parameter, exactly max_write_len bytes
 *       may be decoded, with up to 2 extra bytes from the last input-quad
 *       stored in the decode_buf. In that case these extra bytes should be
 *       appended to the output before resuming.
 * </ul>
 *
 * @param source Base64 encoded ASCII input.
 * @param len Number of base64 encoded ASCII characters in soruce.
 * @param read_pos (output) This value is set to the position in source input
 *     where decoding stopped for whatever reason. If read_pos<len, there may
 *     be more data in the input buffer. If decoding is resumed, the new
 *     source should be source+read_pos.
 * @param target (output) Buffer for decoded binary data. The return value
 *     indicates how many valid bytes have been decoded. If this pointer is
 *     NULL, bytes are decoded as normal and discarded.
 * @param warning (output) This value is set to TRUE if the input was not
 *     valid base64 format.
 * @param max_write_len Maximum number of bytes to write to target. If this
 *     value is 0, all bytes decoded from the input are written. In that case,
 *     the size of the target buffer must be ((len+3)/4)*3 bytes. If
 *     decode_buf is NULL, up to 2 bytes less than max_write_len may be
 *     returned even if there is more data in the source input.
 * @param decode_buf (output) If max_write_len is not 0 and this pointer is
 *     not NULL, exactly max_write_len bytes may be written to target if the
 *     input is large enough. In that case up to 2 extra bytes are stored in
 *     this buffer. If decoding is resumed, the bytes in this buffer should be
 *     appended to the output first.
 * @param decode_len (output) Number of extra bytes returned in decode_buf.
 *
 * @return the number of bytes written to target
 */
unsigned long GeneralDecodeBase64(const unsigned char *source, unsigned long len, unsigned long &read_pos, unsigned char *target, BOOL &warning, 
								  unsigned long max_write_len=0, unsigned char *decode_buf=NULL, unsigned int *decode_len=NULL);

#endif // URL2_ENCODER_H
