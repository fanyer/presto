/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
** Lars Thomas Hansen
*/

/* Internal routines for uni_sscanf
 *
 * This code is taken from the EMX run-time library.
 * This code has been modified by Opera Software.
 *
 * _input.c (emx+gcc) -- Copyright (c) 1990-1998 by Eberhard Mattes
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

#ifdef EBERHARD_MATTES_PRINTF

#include "modules/stdlib/src/thirdparty_printf/uni_scanf.h"
#include "modules/stdlib/util/opbitvector.h"

OpScanf::OpScanf(Mode mode)
	: m_mode(mode)
	, m_input(NULL)
	, m_width(0)
	, m_chars(0)
	, m_count(0)
	, m_status(ISOK)
{
}

int
OpScanf::Parse(const uni_char *input, const uni_char * format, va_list arg_ptr)
{
	OP_ASSERT(m_mode == SCANF_UNICODE);
	return inp_main((const void*)input, (const void*) format, arg_ptr);
}

int
OpScanf::Parse(const char *input, const char * format, va_list arg_ptr)
{
	OP_ASSERT(m_mode == SCANF_ASCII);
	return inp_main((const void*)input, (const void*) format, arg_ptr);
}

uni_char OpScanf::get_c(const void *buf) const
{
    uni_char c = 0;
    if(m_mode == SCANF_ASCII)
    {
        const unsigned char *ascii_buf = (const unsigned char *)buf;
        c = ascii_buf[0];
    }
    else
    {
        const uni_char *uni_buf = (const uni_char *)buf;
        c = uni_buf[0];
    }
    return c;
}

#define GETC(s) get_c((s))

#define PrevCPtr(s) ((const void*)((const char*)(s) - m_mode - 1))

#define NextCPtr(s) ((const void*)((const char*)(s) + m_mode + 1))

#define OpScanf_PeekInput() GETC(m_input)

#define OpScanf_NextInput() do { m_input = NextCPtr(m_input); } while(0)

#define OpScanf_PrevInput() do { m_input = PrevCPtr(m_input); } while(0)

#define OpScanf_PeekFormat()            GETC(format)

#define OpScanf_PeekNextFormat()        GETC(NextCPtr(format))

#define OpScanf_PeekNextNextFormat()    GETC(NextCPtr(NextCPtr(format)))

#define OpScanf_NextFormat()            do { format = NextCPtr(format); } while(0)

int OpScanf::skip()
{
	int c;

	do
	{
		c = OpScanf_PeekInput();
		OpScanf_NextInput();
		if (c == 0)
		{
			m_status = INPUT_FAILURE;
			return EOF;
		}
	} while (uni_isspace(c));

	/* We have read one character of the field, therefore we have to
	   decrement `m_width'.  Note that `m_width' is never zero on
	   entry to skip(). */

	--m_width;
	return c;
}

int OpScanf::get()
{
	if (0 == m_width)
		return EOF;

	int c = OpScanf_PeekInput();
	if (c)
		OpScanf_NextInput();
	-- m_width;
	return c ? c : EOF;
}

#define OPSCANF_DEPOSITC(dst, c)				\
	do {										\
		if (m_mode == SCANF_ASCII) {			\
			*(char*)dst = c;					\
			dst = (void*)((char*)dst + 1);		\
		} else {								\
			*(uni_char*)dst = c;				\
			dst = (void*)((uni_char*)dst + 1);	\
		}										\
	} while(0)

void OpScanf::inp_char(void *dst)
{
	int c;
	BOOL ok;

	if (m_status == END_OF_FILE)
	{
		m_status = INPUT_FAILURE;
		return;
	}
	if (!m_width_given)
		m_width = 1;
	ok = FALSE;
	while ((c = get()) != EOF)
	{
		ok = TRUE;
		if (dst != NULL)
			OPSCANF_DEPOSITC(dst, c);
	}
	if (!ok)
		m_status = INPUT_FAILURE;
	else if (dst != NULL)
		++m_count;
}

void OpScanf::inp_str(void *dst)
{
	int c;

	c = skip();
	if (m_status != ISOK)
		return;

	while (c != EOF && !uni_isspace(c))
	{
		if (dst != NULL)
			OPSCANF_DEPOSITC(dst, c);
		c = get();
	}

	if (dst != NULL)
	{
		// Null-terminate if there is space left
		if (m_width)
			OPSCANF_DEPOSITC(dst, 0);
		++m_count;
	}
	if (c != EOF)
		OpScanf_PrevInput();
}

// FIXME: This function does not handle surrogates
void OpScanf::inp_set(const void **pfmt, void *dst)
{
	OpBitvector map;
	char end, done;
	uni_char f;
	const void *format;
	int c, i;

	if (m_status == END_OF_FILE)
	{
		m_status = INPUT_FAILURE;
		return;
	}
	format = *pfmt;
	end = 0;
	OpScanf_NextFormat();
	if (OpScanf_PeekFormat() == '^')
	{
		OpScanf_NextFormat();
		end = 1;
	}
	i = 0;
	done = 0;
	do
	{
		f = OpScanf_PeekFormat();
		switch (f)
		{
		case 0:
			*pfmt = (void*)((char*)format - 1 - m_mode);	/* Avoid skipping past 0 */
			done = 1;
			break;
		case ']':
			if (i > 0)
			{
				*pfmt = format;
				done = 1;
				break;
			}
			/* no break */
		default:
			if (OpScanf_PeekNextFormat() == '-')
			{
				uni_char q = OpScanf_PeekNextNextFormat();
				if (q != 0 && q != ']' && f < q)
				{
					map.SetBits(f, q, 1);
					OpScanf_NextFormat();
					OpScanf_NextFormat();
				}
				else
					map.SetBit(f, 1);
			}
			else
				map.SetBit(f, 1);
			break;
		}
		OpScanf_NextFormat();
		++i;
	} while (!done);

	int chars_read = 0;
	BOOL ok = FALSE;
	c = get();
	while (c != EOF && map.GetBit(c) != end)
	{
		ok = TRUE;
		chars_read++;
		if (dst != NULL)
			OPSCANF_DEPOSITC(dst, c);
		c = get();
	}
	if (dst != NULL)
	{
		OPSCANF_DEPOSITC(dst, 0);
		++m_count;
	}
	if (c != EOF)
		OpScanf_PrevInput();
	if (!ok)
		m_status = INPUT_FAILURE;
}


void OpScanf::inp_int_base(void *dst, int base)
{
	BOOL neg, ok;

#undef LLONG
#ifdef HAVE_LONGLONG
#define LLONG long long
#define ULLONG unsigned long long
#elif defined HAVE_INT64 && defined HAVE_UINT64
#define LLONG INT64
#define ULLONG UINT64
#else
#define LLONG long
#define ULLONG unsigned long
#endif
	ULLONG n;
	int c, digit;

	c = skip();
	if (m_status != ISOK)
		return;
	neg = FALSE;
	if (c == '+')
		c = get();
	else if (c == '-')
	{
		neg = TRUE;
		c = get();
	}

	n = 0;
	ok = FALSE;

	if (base == 0)
	{
		base = 10;
		if (c == '0')
		{
			c = get();
			if (c == 'x' || c == 'X')
			{
				base = 16;
				c = get();
			}
			else
			{
				base = 8;
				ok = TRUE;		/* We've seen a digit! */
			}
		}
	}
	else if (base == 16 && c == '0')
	{
		c = get();
		if (c == 'x' || c == 'X')
			c = get();
		else
			ok = TRUE;			/* We've seen a digit! */
	}

	while (c != EOF)
	{
		if (uni_isdigit(c))
			digit = c - '0';
		else if (uni_isupper(c))
			digit = c - 'A' + 10;
		else if (uni_islower(c))
			digit = c - 'a' + 10;
		else
			break;
		if (digit < 0 || digit >= base)
			break;
		ok = TRUE;
		n = n * base + digit;
		c = get();
	}
	if (!ok)
	{
		if (c != EOF)
			OpScanf_PrevInput();
		m_status = MATCHING_FAILURE;
		return;
	}
	if (neg)
		n = (ULLONG)-(LLONG)n;
	if (dst != NULL)
	{
		switch (m_size)
		{
#ifdef HAVE_LONGLONG
		case 'L':
			*(long long *) dst = n;
			break;
#elif defined HAVE_INT64
		case 'L':
			*(INT64 *) dst = n;
			break;
#endif
		case 'l':
			*(long *) dst = (long) n;
			break;
		case 'h':
			*(short *) dst = (short) n;
			break;
		case 'H':
			*(char *) dst = (char) n;
			break;
		default:
			*(int *) dst = (int) n;
			break;
		}
		++m_count;
	}
	if (c != EOF)
		OpScanf_PrevInput();
}


void OpScanf::inp_int(unsigned char f, void *dst)
{
	switch (f)
	{
	case 'i': inp_int_base(dst, 0); break;
	case 'd': inp_int_base(dst, 10); break;
	case 'u': inp_int_base(dst, 10); break;
	case 'o': inp_int_base(dst, 8); break;
	case 'x':
	case 'X':
	case 'p': inp_int_base(dst, 16); break;
	default:
		/*abort () */ ;
	}
}

#ifndef NO_FLOATS
double OpScanf::inp_float(unsigned char)
{
	double d;

	if (m_mode == SCANF_ASCII)
	{
		char *endptr;
		d = op_strtod((char*)m_input, &endptr);
		m_input = (void*)endptr;
	}
	else
	{
		uni_char *endptr;
		d = uni_strtod((uni_char*)m_input, &endptr);
		m_input = (void*)endptr;
	}
	return d;
}
#endif

/** This is the working horse for uni_sscanf() and friends. */
int OpScanf::inp_main(const void *input, const void *format, va_list arg_ptr)
{
	void *dst;
	uni_char f;
	BOOL assign;

	m_input = input;

	while ((f = OpScanf_PeekFormat()) != 0)
	{
		if (uni_isspace(f))
		{
			do
			{
				OpScanf_NextFormat();
				f = OpScanf_PeekFormat();
			}
			while (uni_isspace(f))
				;
			int c;
			do
			{
				c = OpScanf_PeekInput();
				OpScanf_NextInput();
			} while (uni_isspace(c));
			if (c != EOF)
				OpScanf_PrevInput();
		}
		else if (f != '%')
		{
			int c = OpScanf_PeekInput();
			OpScanf_NextInput();
			if (c == EOF)
				return (m_count == 0 ? EOF : m_count);
			if ((uni_char) c != OpScanf_PeekFormat())
			{
				if (c != EOF)
					OpScanf_PrevInput();
				return m_count;
			}
			OpScanf_NextFormat();
		}
		else
		{
			m_size = 0;
			m_width = INT_MAX;
			assign = TRUE;
			dst = NULL;
			m_width_given = FALSE;
			OpScanf_NextFormat();
			if (OpScanf_PeekFormat() == '*')
			{
				assign = FALSE;
				OpScanf_NextFormat();
			}
			if (uni_isdigit(OpScanf_PeekFormat()))
			{
				m_width = 0;
				uni_char q = OpScanf_PeekFormat();
				while (uni_isdigit(q))
				{
					m_width = m_width * 10 + (q - '0');
					OpScanf_NextFormat();
					q = OpScanf_PeekFormat();
				}

				/* The behavior for a zero width is declared undefined
				   by ISO 9899-1990.  In this implementation, a zero
				   width is equivalent to no width being specified. */

				if (m_width == 0)
					m_width = INT_MAX;
				else
					m_width_given = TRUE;
			}
			uni_char q = OpScanf_PeekFormat();
			if (q == 'h' || q == 'l' || q == 'L')
			{
				m_size = (char) q;
				OpScanf_NextFormat();
				/* c.f. analogous code in uni_printf.cpp; overloading 'L' to
				 * indicate 'll'; strictly bogus, since 'L' should only be used
				 * with floating formats and 'll' only with integral ones; we
				 * should detect any mismatch and treat it as an error. */
				if (q == 'l' && OpScanf_PeekFormat() == 'l')
				{
#if defined HAVE_LONGLONG || defined HAVE_INT64
					m_size = 'L';
#else
					OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
					OpScanf_NextFormat();
				}
				else if (q == 'h' && OpScanf_PeekFormat() == 'h')
				{
					m_size = 'H';
					OpScanf_NextFormat();
				}
			}
			f = OpScanf_PeekFormat();
			switch (f)
			{
			case 'c':
				if (assign)
					dst = va_arg(arg_ptr, uni_char *);
				inp_char((uni_char *) dst);
				break;

			case '[':
				if (assign)
					dst = va_arg(arg_ptr, uni_char *);
				inp_set(&format, (uni_char *) dst);
				break;

			case 's':
				if (assign)
					dst = va_arg(arg_ptr, uni_char *);
				inp_str((uni_char *) dst);
				break;

#ifndef NO_FLOATS
			case 'f':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
				{
					double d = inp_float((unsigned char)f);
					if (assign)
					{
						switch (m_size)
						{
						case 'L':
							*va_arg(arg_ptr, long double *) = (long double)d;
							break;
						case 'l':
							*va_arg(arg_ptr, double *) = d;
							break;
						default:
							*va_arg(arg_ptr, float *) = (float)d;
							break;
						}
						++m_count;
					}
				}
				break;
#endif // NO_FLOATS

			case 'p':
#ifdef SIXTY_FOUR_BIT
				m_size = 'l';
#endif
			case 'i':
			case 'd':
			case 'u':
			case 'o':
			case 'x':
			case 'X':
				if (assign)
					switch (m_size)
					{
#ifdef HAVE_LONGLONG
					case 'L':
						dst = va_arg(arg_ptr, long long *);

						break;
#elif defined HAVE_INT64
					case 'L':
						dst = va_arg(arg_ptr, INT64 *);

						break;
#endif
					case 'l':
						dst = va_arg(arg_ptr, long *);
						break;
					case 'h':
						dst = va_arg(arg_ptr, short *);
						break;
					case 'H':
						dst = va_arg(arg_ptr, char *);
						break;
					default:
						dst = va_arg(arg_ptr, int *);

						break;
					}
				inp_int((unsigned char) f, dst);
				break;

			case 'n':
				if (assign)
				{
					m_chars = static_cast<const char*>(m_input) - static_cast<const char*>(input);
					if (m_mode == SCANF_UNICODE)
					{
						m_chars /= 2;
					}
					switch (m_size)
					{
#ifdef HAVE_LONGLONG
					case 'L':
						*(va_arg(arg_ptr, long long *)) = m_chars;

						break;
#elif defined HAVE_INT64
					case 'L':
						*(va_arg(arg_ptr, INT64 *)) = m_chars;

						break;
#endif
					case 'h':
						*(va_arg(arg_ptr, short *)) = m_chars;

						break;
					case 'H':
						*(va_arg(arg_ptr, char *)) = m_chars;

						break;
					default:
						*(va_arg(arg_ptr, int *)) = m_chars;

						break;
					}
				}
				break;

			default:
				if (f == 0)		/* % at end of string */
					return m_count;
				int c = OpScanf_PeekInput();
				OpScanf_NextInput();
				if (c != f)
					return m_count;
				break;
			}
			switch (m_status)
			{
			case INPUT_FAILURE:
				return ((m_count == 0) ? EOF : m_count);
			case MATCHING_FAILURE:
			case OUT_OF_MEMORY:
				return m_count;
			default:
				break;
			}
			OpScanf_NextFormat();
		}
	}
	return m_count;
}

#undef OpScanf_NextInput
#undef OpScanf_PrevInput
#undef OpScanf_PeekFormat
#undef OpScanf_PeekNextFormat
#undef OpScanf_PeekNextNextFormat
#undef OpScanf_NextFormat

#undef OPSCANF_DEPOSITC

#endif // EBERHARD_MATTES_PRINTF
