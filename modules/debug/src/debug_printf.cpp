/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DEBUG_ENABLE_PRINTF

#include "modules/pi/OpSystemInfo.h"

typedef void(*output_chars_fn)(const char*, int);
typedef void(*output_unichars_fn)(const uni_char*);

static void dbg_out_dec(output_chars_fn output_chars, unsigned long val)
{
	char buffer[32];		// ARRAY OK 2008-05-10 mortenro
	char* digs = &buffer[30];
	char* digs_end = digs;

	*digs = 0;
	do
	{
		unsigned long div10 = val / 10;
		*--digs = (char)('0' + (val - div10 * 10));
		val = div10;
    }
	while ( val != 0 );

	output_chars(digs, digs_end - digs);
}

static void dbg_out_hexpointer(output_chars_fn output_chars, UINTPTR val)
{
	unsigned int k;
	char buffer[32];		// ARRAY OK 2008-05-10 mortenro
	char* digs = &buffer[30];
	char* digs_end = digs;

	*digs = 0;
	for ( k = 0; k < (sizeof(UINTPTR)*2); k++ )
	{
		char d = val & 0xf;
		if ( d >= 10 )
			d += ('a' - '0' - 10);
		*--digs = d + '0';
		val >>= 4;
    }
	*--digs = 'x';
	*--digs = '0';

	output_chars(digs, digs_end - digs);
}

static void dbg_out_hex(output_chars_fn output_chars, unsigned long val)
{
	char buffer[32];		// ARRAY OK 2008-05-10 mortenro
	char* digs = &buffer[30];
	char* digs_end = digs;

	*digs = 0;
	do
	{
		char d = (char)(val & 0xf);
		if ( d >= 10 )
			d += ('a' - '0' - 10);
		*--digs = d + '0';
		val >>= 4;
    }
	while ( val != 0 );

	output_chars(digs, digs_end - digs);
}

static void dbg_out_sign(output_chars_fn output_chars, long* val)
{
	if ( *val < 0 )
	{
		output_chars("-", 1);
		*val = -*val;
    }
}

static void dbg_out_memory(output_chars_fn output, const void* ptr, int bytes)
{
	const char* table =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char buffer[256];	// ARRAY OK 2008-05-10 mortenro
	const unsigned char* src = (unsigned char*)ptr;

	char* dst;
	char* dst_end = buffer + 250;

	unsigned char b0;
	unsigned char b1;
	unsigned int index;

	while ( bytes > 0 )
	{
		dst = buffer;

		while ( bytes > 0 && dst < dst_end )
		{
			dst[2] = '=';
			dst[3] = '=';
			b1 = 0;

			// Determine 'dst[0]' - always present
			b0 = *src++;
			index = b0 >> 2;
			dst[0] = table[index];

			if ( bytes > 1 )
				b1 = *src++;

			// Determine 'dst[1]' - always present
			index = ((b0 << 4) | (b1 >> 4)) & 0x3f;
			dst[1] = table[index];

			unsigned char b2;
			if ( bytes > 2 )
				b2 = *src++;
			else
				b2 = 0;

			// Determine 'dst[2]' - only present for bytes > 1
			if ( bytes > 1 )
			{
				index = ((b1 << 2) | (b2 >> 6)) & 0x3f;
				dst[2] = table[index];
			}

			// Determine 'dst[3]' - only present for bytes > 2
			if ( bytes > 2 )
			{
				index = b2 & 0x3f;
				dst[3] = table[index];
			}

			bytes -= 3;
			dst += 4;
		}
		output(buffer, dst - buffer);
	}
}

static inline void create_utf8_character(unsigned char* &ptr, uni_char ch)
{
	unsigned char* tmp = ptr;

	if ( ch <= 0x7f )
	{
		*tmp++ = (unsigned char)ch;
	}
	else if ( ch <= 0x7ff )
	{
		*tmp++ = 0xc0 | (ch >> 6);
		*tmp++ = 0x80 | (ch & 0x3f);
	}
	else
	{
		*tmp++ = 0xe0 | (ch >> 12);
		*tmp++ = 0x80 | ((ch >> 6) & 0x3f);
		*tmp++ = 0x80 | (ch & 0x3f);
	}

	ptr = tmp;
}

static void dbg_unichar_logoutput(const uni_char* str)
{
	unsigned char buffer[1024]; // ARRAY OK 2008-05-10 mortenro
	unsigned char* wptr;
	unsigned char* eptr;

	while ( *str != 0 )
	{
		wptr = buffer;
		eptr = buffer + 1020;

		while ( *str != 0 && wptr < eptr )
			create_utf8_character(wptr, *str++);
		*wptr = 0;
		dbg_logfileoutput((char*)buffer, wptr - buffer);
	}
}

static void dbg_char_logoutput(const char* str, int len)
{
	unsigned char buffer[1024]; // ARRAY OK 2008-05-10 mortenro
	unsigned char* wptr;
	unsigned char* wptr_end;

	if ( len == 0 )
	{
		// Allow 'log_printf("")' to trigger a call to dbg_logfileoutput
		dbg_logfileoutput(str, 0);
		return;
	}

	const char* str_end = str + len;

	while ( str < str_end )
	{
		wptr = buffer;
		wptr_end = buffer + 1020;

		while ( str < str_end && wptr < wptr_end )
		{
			unsigned char ch = (unsigned char)*str++;
			create_utf8_character(wptr, (uni_char)ch);
		}
		*wptr = 0;
		dbg_logfileoutput((char*)buffer, wptr - buffer);
	}
}

static void dbg_char_sysoutput(const char* str, int len)
{
	dbg_char_logoutput(str, len);	// To the logfile also

	uni_char buffer[512];		// ARRAY OK 2008-05-10 mortenro
	int k;

	while ( len > 0 )
	{
		for ( k = 0; len > 0 && k < 510; k++ )
		{
			buffer[k] = (uni_char)*str++;
			len--;
		}
		buffer[k] = 0;
		dbg_systemoutput(buffer);
	}
}

static void dbg_unichar_sysoutput(const uni_char* str)
{
	dbg_unichar_logoutput(str);		// To the logfile also
	dbg_systemoutput(str);			// To the system debug console
}

static int dbg_strnlen(const char* string, size_t maxlen)
{
	for ( size_t k = 0; k < maxlen; k++ )
		if ( string[k] == 0 )
			return k;

	return maxlen;
}

void dbg_base_printf(output_unichars_fn output_unichars,
					 output_chars_fn output_chars,
					 const char* format, va_list args)
{
	const char* s = format;

	if ( *s == 0 )
	{
		// In case of empty format string, call platform output function
		// anyway, to get it started (called with empty format string
		// from debug module initialization).
		output_chars("",0);
		return;
	}

	bool zero_padding = false;
	int num_zeros = 0;

	for (;;)
	{
		switch ( s[0] ) {

		case '%':
			if ( s != format )
				output_chars(format, s - format);

		next_token:
			switch ( s[1] ) {

			case '0':
			{
				if (op_isdigit(s[2]))
				{
					zero_padding = true;
					const char *p = &s[2];
					char *q;
					num_zeros = op_strtoul(p, &q, 10);
					OP_ASSERT(q >= p);
					s += (q - p) + 1;
				}
				else
					s++;
				goto next_token;
			}
			case 'd':
			{
				long val = va_arg(args, int);
				dbg_out_sign(output_chars, &val);
				if ( zero_padding )
				{
					char tmp[32]; // ARRAY OK 2011-12-22 peter
					int written = op_snprintf(tmp, 32, "%ld", val);
					OP_ASSERT(written < 32);
					int zeros = num_zeros - op_strlen(tmp);
					while (zeros-- > 0)
						output_chars("0", 1);
					zero_padding = false;
				}
				dbg_out_dec(output_chars, val);
				break;
			}

			case 'x':
				dbg_out_hex(output_chars,
							(unsigned long)(va_arg(args, unsigned int)));
				break;

			case 'p':
				dbg_out_hexpointer(output_chars,
								   (UINTPTR)(va_arg(args, void*)));
				break;

			case 'u':
				dbg_out_dec(output_chars,
							(unsigned long)va_arg(args, unsigned int));
				break;

			case 'z':
				OP_ASSERT(sizeof(size_t) <= sizeof(unsigned long));
				dbg_out_dec(output_chars,(unsigned long)va_arg(args, size_t));
				break;

			case 'c':
			{
				uni_char ch[2];		// ARRAY OK 2008-05-10 mortenro
				ch[0]= va_arg(args, unsigned int);
				ch[1] = 0;
				output_unichars(ch);
				break;
			}

			case 's':
			{
				const char* str = va_arg(args, const char*);
				if ( str == 0 )
					str = "(null)";
				output_chars(str, op_strlen(str));
				break;
			}

			case 'S':
			{
				const uni_char* str = va_arg(args, uni_char*);
				if ( str == 0 )
					str = UNI_L("(null)");
				output_unichars(str);
				break;
			}

			case 'B':
			{
				const void* ptr = va_arg(args, const void*);
				int bytes = va_arg(args, int);
				dbg_out_memory(output_chars, ptr, bytes);
				break;
			}

			case '.':
			{
				//
				// Accept a few limited "sophisticated" formatting options:
				//
				//    %.*s  (specify size of string)
				//
				if ( !op_strncmp(s, "%.*s", 4) )
				{
					int count = va_arg(args, int);
					const char* str = va_arg(args, const char*);
					if ( str == 0 )
					{
						str = "(null)";
						count = 6;
					}

					int len = dbg_strnlen(str, count);
					if ( len < count )
						count = len;

					output_chars(str, count);
					s += 2; // Adjust s for the extra '.' and '*'
					break;
				}

				OP_ASSERT(!"dbg_printf format not supported");
				break;
			}

			case '%':
				output_unichars(UNI_L("%"));
				break;

			default:
				OP_ASSERT(!"dbg_printf format not supported");
				output_chars(s, 2);
				break;
			}

			s += 2;
			format = s;
			break;

		case '\0':
			if ( s != format )
				output_chars(format, s - format);
			return;

		default:
			s++;
		}
	}
}

extern "C" void dbg_printf(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	dbg_base_printf(dbg_unichar_sysoutput, dbg_char_sysoutput, format, args);
	va_end(args);
}

extern "C" void dbg_vprintf(const char* format, va_list args)
{
	dbg_base_printf(dbg_unichar_sysoutput, dbg_char_sysoutput, format, args);
}

extern "C" void dbg_print_timestamp(void)
{
	double now = g_op_time_info->GetTimeUTC() / 1000.0;
	now -= g_op_time_info->GetTimezone();
	unsigned int hours = static_cast<unsigned int>(now / 3600.0) % 24;
	unsigned int minutes = static_cast<unsigned int>(now / 60.0) % 60;
	unsigned int seconds = static_cast<unsigned int>(now) % 60;
	dbg_printf("%02d:%02d:%02d ", hours, minutes, seconds);
}

extern "C" void log_printf(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	dbg_base_printf(dbg_unichar_logoutput, dbg_char_logoutput, format, args);
	va_end(args);
}

#ifdef SELFTEST
//
// Provide a special selftest version of printf, that can be used from
// selftests to verify the format operations.  Two special output
// methods are needed, and a buffer in which to store the result.
//

static void test_unichar_output(const uni_char* str)
{
	while ( *str )
		g_dbg_printf_selftest_data[g_dbg_printf_selftest_data_size++] = *str++;
	g_dbg_printf_selftest_data[g_dbg_printf_selftest_data_size] = 0;
}

static void test_char_output(const char* str, int len)
{
	uni_char* dst = g_dbg_printf_selftest_data + g_dbg_printf_selftest_data_size;

	for ( int k = 0; k < len; k++ )
		*dst++ = (uni_char)*str++;

	*dst = 0;
	g_dbg_printf_selftest_data_size += len;
}

extern "C" void test_printf(const char* format, ...)
{
	va_list args;

	if ( g_dbg_printf_selftest_data == 0 )
		g_dbg_printf_selftest_data = (uni_char*)op_malloc(16384);

	va_start(args, format);
	dbg_base_printf(test_unichar_output, test_char_output, format, args);
	va_end(args);
}

extern "C" int verify_printf(const uni_char* expected)
{
	int rc = 1;

	if ( uni_strcmp(expected, g_dbg_printf_selftest_data) != 0 )
	{
		log_printf("verify_selftest('%S') failed, got '%S' (len: %d %d)\n",
				   expected, g_dbg_printf_selftest_data, uni_strlen(expected),
				   uni_strlen(g_dbg_printf_selftest_data));
		rc = 0;
	}

	g_dbg_printf_selftest_data_size = 0;

	return rc;
}

#endif // SELFTEST

#endif // DEBUG_ENABLE_PRINTF
