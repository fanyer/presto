/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* Internal routines for uni_sprintf
 *
 * This code is taken from the EMX run-time library.
 * This code has been modified by Opera Software.
 *
 * _output.c (emx+gcc) -- Copyright (c) 1990-1997 by Eberhard Mattes
 *
 * The emx libraries are not distributed under the GPL.  Linking an
 * application with the emx libraries does not cause the executable
 * to be covered by the GNU General Public License.  You are allowed
 * to change and copy the emx library sources if you keep the copyright
 * message intact.  If you improve the emx libraries, please send your
 * enhancements to the emx author (you should copyright your
 * enhancements similar to the existing emx libraries).
 */

#include "core/pch.h"

#if !(defined(MSWIN) || defined(WIN_CE)) || defined(_OPERA_STRING_LIBRARY_)

#undef  HAVE_LONGLONG

/*
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <emx/io.h>
#include <emx/float.h>
#include <emx/locale.h>
*/

#include "uniprntf.h"

#define DEFAULT_PREC    6

#define BEGIN do {
#define END   } while (0)

/* All functions in this module return -1 if an error ocurred while
   writing the target file.  Otherwise, _output() returns the number
   of characters written, all other functions return 0.

   The CHECK macro is used to return -1 if the expression X evaluates
   to a non-zero value.  This is used to pass up errors. */

#define CHECK(X) BEGIN \
                   if ((X) != 0) \
                     return -1; \
                 END

/* Write the character C.  Note the return statement! */

#define PUTC(C) BEGIN \
                    if (0 == m_maxcharacters) return -1; \
                    *m_output = C; \
					++m_output; --m_maxcharacters; \
                    ++m_count; \
                  END

#define STRLEN(S) (((S) == NULL) ? 0 : uni_strlen (S))

int UniPrintf::out_str(const uni_char *s, int n)
{
	while (n > 0)
	{
		if (0 == m_maxcharacters)
			return -1;
		*m_output = *s;
		m_output++;
		s++;
		n--;
		m_maxcharacters--;
		m_count++;
	}
	return 0;
}

int UniPrintf::out_str_upsize(const char *s, int n)
{
	while (n > 0)
	{
		if (0 == m_maxcharacters)
			return -1;
		*m_output = *s;
		m_output++;
		s++;
		n--;
		m_maxcharacters--;
		m_count++;
	}
	return 0;
}


/* Note the return statement! */

#define OUT_STR(S,N) BEGIN if (out_str (S, N) != 0) return -1; END


int UniPrintf::out_pad(uni_char c, int n)
{
	while (n > 0)
	{
		if (0 == m_maxcharacters)
			return -1;
		*m_output = c;
		m_output++;
		n--;
		m_maxcharacters--;
		m_count++;
	}
	return 0;
}


/* Note the return statement! */

#define OUT_PAD(C,N) BEGIN if (out_pad (C, N) != 0) return -1; END


int UniPrintf::cvt_str(const uni_char *str, int str_len)
{
	if (str == NULL)
	{
		/* Print "(null)" for the NULL pointer if the precision is big
		   enough or not specified. */

		if (m_prec < 0 || m_prec >= 6)
			str = UNI_L("(null)");
		else
			str = UNI_L("");
	}

	/* The precision, if specified, limits the number of characters to
	   be printed.  If the precision is specified, the array pointed to
	   by STR does not need to be null-terminated. */

	if (str_len == -1)
	{
		if (m_prec >= 0)
		{
			str_len = m_prec;
			for (int i = 0; i < m_prec; i++)
				if (str[i] == 0)
				{
					str_len = i;
					break;
				}
		}
		else
			str_len = uni_strlen(str);
	}
	else if (m_prec >= 0 && m_prec < str_len)
		str_len = m_prec;

	/* Print the string, using blanks for padding. */

	if (str_len < m_width && !m_minus)
		OUT_PAD(' ', m_width - str_len);
	OUT_STR(str, str_len);
	if (str_len < m_width && m_minus)
		OUT_PAD(' ', m_width - str_len);
	return 0;
}


int UniPrintf::cvt_number(const uni_char *pfx,
					      const uni_char *str, const uni_char *xps,
					      int lpad0, int rpad0, int is_signed, int is_neg)
{
	int sign, pfx_len, str_len, xps_len, min_len;

	if (!is_signed)				/* No sign for %u, %o and %x */
		sign = EOF;
	else if (is_neg)
		sign = '-';
	else if (m_plus)			/* '+' overrides ' ' */
		sign = '+';
	else if (m_blank)
		sign = ' ';
	else
		sign = EOF;

	pfx_len = STRLEN(pfx);
	str_len = uni_strlen(str);
	xps_len = STRLEN(xps);

	if (lpad0 < 0)
		lpad0 = 0;
	if (rpad0 < 0)
		rpad0 = 0;

	/* Compute the minimum length required for printing the number. */

	min_len = lpad0 + pfx_len + str_len + rpad0 + xps_len;
	if (sign != EOF)
		++min_len;

	/* If padding with zeros is requested, increase LPAD0 to pad the
	   number on the left with zeros.  Note that the `-' flag
	   (left-justify) turns off padding with zeros. */

	if (m_pad == '0' && min_len < m_width)
	{
		lpad0 += m_width - min_len;
		min_len = m_width;
	}

	/* Pad on the left with blanks. */

	if (min_len < m_width && !m_minus)
		OUT_PAD(' ', m_width - min_len);

	/* Print the number. */

	if (sign != EOF)
		PUTC(sign);
	OUT_STR(pfx, pfx_len);
	OUT_PAD('0', lpad0);
	OUT_STR(str, str_len);
	OUT_PAD('0', rpad0);
	OUT_STR(xps, xps_len);

	/* Pad on the right with blanks. */

	if (min_len < m_width && m_minus)
		OUT_PAD(' ', m_width - min_len);
	return 0;
}

int UniPrintf::cvt_integer(const uni_char *pfx,
					       const uni_char *str, int zero, bool is_signed,
					       bool is_neg)
{
	int lpad0;

	if (zero && m_prec == 0)	/* ANSI */
		return 0;

	if (m_prec >= 0)			/* Ignore `0' if `-' is given for an integer */
		m_pad = ' ';

	lpad0 = m_prec - uni_strlen(str);
	return cvt_number(pfx, str, NULL, lpad0, 0, is_signed, is_neg);
}


int UniPrintf::cvt_hex(uni_char *str, char x, int zero)
{
	return cvt_integer(((!zero && m_hash) ? (x == 'X' ? UNI_L("0X") : UNI_L("0x")) : NULL),
	                   str, zero, false, false);
}


int UniPrintf::cvt_hex_32(unsigned n, char x)
{
	uni_char buf[9];

	sprintf( (char*)buf, "%lx", (long)n);
	make_doublebyte_in_place(buf, op_strlen((char*)buf) );
	return cvt_hex(buf, x, n == 0);
}


int UniPrintf::cvt_oct(const uni_char *str, int zero)
{
	size_t len;

	if (m_hash && str[0] != '0')
	{
		len = uni_strlen(str);
		if (m_prec <= (int) len)
			m_prec = len + 1;
	}
	return cvt_integer(NULL, str, zero && !m_hash, false, false);
}


int UniPrintf::cvt_oct_32(unsigned n)
{
	uni_char buf[12];

	sprintf( (char*)buf, "%lx", (long)n);
	make_doublebyte_in_place(buf, op_strlen((char*)buf) );
	return cvt_oct(buf, n == 0);
}


int UniPrintf::cvt_dec_32(unsigned n, bool is_signed, bool is_neg)
{
	uni_char buf[11];

	sprintf( (char*)buf, "%lx", (long)n);
	make_doublebyte_in_place(buf, op_strlen((char*)buf) );
	return cvt_integer(NULL, buf, n == 0, is_signed, is_neg);
}



int UniPrintf::cvt_longdouble(long double x,
						      const uni_char *fmtbegin,
						      const uni_char *fmtend)
{
	/* Use the single-byte library snprintf and copy it over to a
	 * unicode buffer, since numbers are in the ASCII range anyway.
	 * no need to use up too much code
	 */
	char tmpbuf[256]; /* ARRAY OK 2004-01-23 peter */
	char *template_p = new char[fmtend - fmtbegin + 2];
	if (template_p == NULL)
		return -1;
	unsigned char *dst = (unsigned char *) template_p;

	for (const uni_char *src = fmtbegin; src <= fmtend; src++)
	{
		*(dst++) = (unsigned char) *src;
	}
	*dst = 0;
	int len = sprintf(tmpbuf, template_p, x);

	out_str_upsize(tmpbuf, len);

	/* Clean up */
	delete [] template_p;

	return 0;
}


int UniPrintf::cvt_double(double x, const uni_char *fmtbegin,
					      const uni_char *fmtend)
{
	/* This function is a copy of the above, but with another input type */
	/* Use the single-byte library snprintf and copy it over to a
	 * unicode buffer, since numbers are in the ASCII range anyway.
	 * no need to use up too much code
	 */
	char tmpbuf[256]; /* ARRAY OK 2004-01-23 peter */
	char *template_p = new char[fmtend - fmtbegin + 2];
	if (template_p == NULL)
		return -1;
	unsigned char *dst = (unsigned char *) template_p;

	for (const uni_char *src = fmtbegin; src <= fmtend; src++)
	{
		*(dst++) = (unsigned char) *src;
	}
	*dst = 0;
	int len = sprintf(tmpbuf, template_p, x);

	out_str_upsize(tmpbuf, len);

	/* Clean up */
	delete [] template_p;

	return 0;
}

int UniPrintf::Output(uni_char *output, int max, const uni_char *format, va_list arg_ptr)
{
	UniPrintf v;
	return v.uni_output(output, max, format, arg_ptr);
}

int UniPrintf::uni_output(uni_char *output, int max, const uni_char *format,
			              va_list arg_ptr)
{
	uni_char size, c;
	const uni_char *float_format_begin;
	bool cont;

	/* Initialize variables. */

	m_output = output;
	m_maxcharacters = max;
	m_count = 0;

	/* Interpret the string. */

	while ((c = *format) != 0)
		if (c != '%')
		{
			/* ANSI X3.159-1989, 4.9.6.1: "... ordinary multibyte
			   charcters (not %), which are copied unchanged to the output
			   stream..."

			   Avoid the overhead of calling mblen(). */

			PUTC(c);
			++format;
		}
		else if (format[1] == '%')
		{
			/* ANSI X3.159-1989, 4.9.6.1: "The complete conversion
			   specification shall be %%." */

			PUTC('%');
			format += 2;
		}
		else
		{
			m_minus = m_plus = m_blank = m_hash = false;
			m_width = 0;
			m_prec = -1;
			size = 0;
			m_pad = ' ';
			float_format_begin = format;
			cont = true;
			do
			{
				++format;
				switch (*format)
				{
				case '-':
					m_minus = true;
					break;
				case '+':
					m_plus = true;
					break;
				case '0':
					m_pad = '0';
					break;
				case ' ':
					m_blank = true;
					break;
				case '#':
					m_hash = true;
					break;
				default:
					cont = false;
					break;
				}
			} while (cont);

			/* `-' overrides `0' */

			if (m_minus)
				m_pad = ' ';

			/* Field width */

			if (*format == '*')
			{
				++format;
				m_width = va_arg(arg_ptr, int);

				if (m_width < 0)
				{
					m_width = -m_width;
					m_minus = true;
				}
			}
			else
				while (*format >= '0' && *format <= '9')
				{
					m_width = m_width * 10 + (*format - '0');
					++format;
				}

			/* Precision */

			if (*format == '.')
			{
				++format;
				if (*format == '*')
				{
					++format;
					m_prec = va_arg(arg_ptr, int);

					if (m_prec < 0)
						m_prec = -1;	/* We don't need this */
				}
				else
				{
					m_prec = 0;
					while (*format >= '0' && *format <= '9')
					{
						m_prec = m_prec * 10 + (*format - '0');
						++format;
					}
				}
			}

			/* Size */

			if (*format == 'h' || *format == 'l' || *format == 'L')
			{
				size = *format++;
				if (size == 'l' && *format == 'l')
				{
					size = 'L';
					++format;
				}
			}

			/* Format */

			switch (*format)
			{
			case 0:
				return m_count;

			case 'n':
				if (size == 'h')
				{
					short *ptr = va_arg(arg_ptr, short *);

					*ptr = m_count;
				}
				else
				{
					int *ptr = va_arg(arg_ptr, int *);

					*ptr = m_count;
				}
				break;

			case 'c':
				{
					uni_char c;

					c = (uni_char) va_arg(arg_ptr, unsigned);

					m_prec = 1;
					CHECK(cvt_str(&c, 1));
				}
				break;

			case 's':
				CHECK(cvt_str(va_arg(arg_ptr, const uni_char *), -1));

				break;

			case 'd':
			case 'i':
				{
					int n = va_arg(arg_ptr, int);

					if (size == 'h')
						n = (short) n;
					if (n < 0)
					{
						unsigned o = -n;
						CHECK(cvt_dec_32(o, true, true));
					}
					else
						CHECK(cvt_dec_32(n, true, false));
				}
				break;

			case 'u':
				{
					unsigned n = va_arg(arg_ptr, unsigned);

					if (size == 'h')
						n = (unsigned short) n;
					CHECK(cvt_dec_32(n, false, false));
				}
				break;

			case 'p':
				m_hash = true;
				CHECK(cvt_hex_32(va_arg(arg_ptr, unsigned), 'x'));

				break;

			case 'x':
			case 'X':
				{
					unsigned n = va_arg(arg_ptr, unsigned);

					if (size == 'h')
						n = (unsigned short) n;
					CHECK(cvt_hex_32(n, (char) *format));
				}
				break;

			case 'o':
				{
					unsigned n = va_arg(arg_ptr, unsigned);

					if (size == 'h')
						n = (unsigned short) n;
					CHECK(cvt_oct_32(n));
				}
				break;

			case 'g':
			case 'G':
			case 'e':
			case 'E':
			case 'f':
			case 'F':
				if (size == 'L')
				{
					long double x = va_arg(arg_ptr, long double);

					CHECK(cvt_longdouble
						  (x, float_format_begin, format));
				}
				else
				{
					double x = va_arg(arg_ptr, double);

					CHECK(cvt_double(x, float_format_begin, format));
				}
				break;

			default:

				/* ANSI X3.159-1989, 4.9.6.1: "If a conversion
				   specification is invalid, the behavior is undefined."

				   We print the last letter only. */

				OUT_STR(format, 1);
				break;
			}
			++format;
		}

	/* Make sure output is null terminated unless we weren't allowed to write */
	if (m_maxcharacters > 0)
		*m_output = 0;
	else if ( /* m_maxcharacters == 0 && */ m_count > 0)
		*(m_output - 1) = 0;
	/* else we entered with max=0 */

	/* Return the number of characters printed. */

	return m_count;
}

#endif
