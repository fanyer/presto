/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef _STANDALONE
# include "modules/unicode/utf8.h"
#endif // !_STANDALONE

// --- Functions used in various macros.

char *make_singlebyte_in_place(char *buf)
{
	char *start = buf;
	if (buf)
	{
		uni_char *unibuf = (uni_char *) buf;

		while (*unibuf != 0x00)
			(*buf++) = (char) (*unibuf++);
		*buf = 0;
	}
	return start;
}

uni_char *make_doublebyte_in_place(uni_char * buf, int len)
{
	uni_char *start = buf;
	if (buf)
	{
		unsigned char *cbuf = (unsigned char *) buf;

		for (int i = len; i >= 0; i--)
			buf[i] = cbuf[i];
	}
	return start;
}

void make_singlebyte_in_buffer(const uni_char *str, int str_len, char *buf, int buf_len)
{
	if( buf_len > 0 )
		uni_cstrlcpy( buf, str, MIN(str_len+1,buf_len) );
}

void make_doublebyte_in_buffer(const char *str, int str_len, uni_char * buf, int buf_len)
{
	int copy_len = str_len;

	if (copy_len >= buf_len)
		copy_len = buf_len - 1;
	if (copy_len > 0)
		for (int i = 0; i < copy_len; i++)
			buf[i] = (unsigned char)str[i];
	buf[copy_len] = 0;
}

#ifndef _STANDALONE

char *make_singlebyte_in_tempbuffer(const uni_char * str, int len)
{
	if (!str)
		return NULL;
	make_singlebyte_in_buffer(str, len, reinterpret_cast<char *>(g_memory_manager->GetTempBufUni()), UNICODE_SIZE(g_memory_manager->GetTempBufUniLenUniChar()));
	return reinterpret_cast<char *>(g_memory_manager->GetTempBufUni());
}

uni_char *make_doublebyte_in_tempbuffer(const char *str, int len)
{
	uni_char *uni_val_tmp = reinterpret_cast<uni_char *>(g_memory_manager->GetTempBufUni());

	make_doublebyte_in_buffer(str, len, uni_val_tmp, g_memory_manager->GetTempBufUniLenUniChar());
	return uni_val_tmp;
}

#endif // !_STANDALONE

/** Converts buf to lower-case in place, destroying the old string. Note
    that this function handles not only English, but also Norwegian, Greek
    Georgian and pretty much everything else. Does not properly handle
    shrinking/expanding conversions (will leave the string length alone).
 
    TODO: Merge with uni_strlwr() */
uni_char *uni_buflwr(uni_char *str, size_t nchars)
{
	for (size_t i = 0; i<nchars; i++ )
		str[i] = Unicode::ToLower( str[i] );
	return str;
}

/** Converts buf to lower-case in place, destroying the old string. Note
    that this function handles not only English, but also Norwegian, Greek
    Georgian and pretty much everything else. Does not properly handle
    shrinking/expanding conversions (will leave the string length alone).

    TODO: Merge with uni_strupr()  */
uni_char *uni_bufupr(uni_char *str, size_t nchars)
{
	for (size_t i = 0; i<nchars; i++ )
		str[i] = Unicode::ToUpper( str[i] );
	return str;
}

#ifndef _STANDALONE

int to_utf8(char* dest, const uni_char* src, int maxlen)
{
	UTF8Encoder converter;

	int written = 0;
	if (dest && maxlen > 0)
	{
		// Actually convert
		int read;
		written = converter.Convert(src, UNICODE_SIZE(uni_strlen(src) + 1), dest, maxlen, &read);
		if (written == maxlen)
		{
			// Always nul-terminate output, also when we are unable
			// to convert the entire input string.
			dest[maxlen - 1] = 0;
		}
	}
	else
	{
		// Just count
		written = converter.Measure(src, UNICODE_SIZE(uni_strlen(src) + 1));
		written = MIN(written, maxlen);
	}
	return written;
}

int from_utf8(uni_char* dest, const char* src, int maxlen)
{
	UTF8Decoder t_converter;
    int written = 0;
	int read;

	maxlen &= ~1; // Always use an even-byte sized buffer
	
	// When passed a NULL pointer, we just calculate the space needed
	written = dest
		? t_converter.Convert(src, op_strlen(src) + 1, dest, maxlen, &read)
		: t_converter.Measure(src, op_strlen(src) + 1, maxlen, &read);

	if (dest && maxlen && written == maxlen)
	{
		// Always nul-terminate output, also when we are unable
		// to convert the entire input string.
		dest[maxlen / 2 - 1] = 0;
	}
    return written;
}

#endif // !_STANDALONE
