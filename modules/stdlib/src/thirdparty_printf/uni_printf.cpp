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

/* Converted to joint ascii/unicode printf by lth@opera.com / November 2005.

   The trick is to abstract access to the format string and the production
   of output.  When we produce temporary strings internally, they are always
   unicode (they are always short), and all character values are unicode
   values.  */

#include "core/pch.h"

#ifdef EBERHARD_MATTES_PRINTF

#include "modules/stdlib/include/double_format.h"
#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"
#include "modules/stdlib/stdlib_pi.h"
#include "modules/util/handy.h"

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

int OpPrintf::Format(uni_char *output, int max, const uni_char *format, va_list arg_ptr)
{
	OP_ASSERT(m_mode == PRINTF_UNICODE);
	return uni_output((void*)output, max, (const void*)format, arg_ptr, FALSE);
}

int OpPrintf::Format(char *output, int max, const char *format, va_list arg_ptr)
{
	OP_ASSERT(m_mode == PRINTF_ASCII);
	return uni_output((void*)output, max, (const void*)format, arg_ptr, FALSE);
}

OpPrintf::OpPrintf(Mode mode)
	: m_mode(mode)
{
}

/* All functions in this module return -1 if an error ocurred while
   writing the target file.  Otherwise, _output() returns the number
   of characters written, all other functions return 0.

   The OPPRINTF_CHECK macro is used to return -1 if the expression X
   evaluates to a non-zero value.  This is used to pass up errors. */

#define OPPRINTF_CHECK(X)	\
	do {					\
		if ((X) != 0)		\
			return -1;		\
	} while (0)

#define OPPRINTF_DEPOSITC(C)								\
	do {													\
		uni_char TMPC = C;									\
		if (m_mode == PRINTF_ASCII)							\
			*(char*)m_output = (char)TMPC;					\
		else												\
			*(uni_char*)m_output = TMPC;					\
		m_output = (void*)((char*)m_output + m_mode + 1);	\
	} while (0)

/* Write the character C */

#define OPPRINTF_PUTC(C)						\
	do {										\
		++m_count;								\
		if (m_maxcharacters)					\
		{										\
			--m_maxcharacters;					\
			OPPRINTF_DEPOSITC(C);				\
		}										\
	} while (0)

#define GETC(s) get_c((s))

#define NextCPtr(s) ((const void*)((const char*)(s) + m_mode + 1))

uni_char OpPrintf::get_c(const void *buf) const
{
    uni_char c = 0;
    if(m_mode == PRINTF_ASCII)
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

int OpPrintf::out_str(const uni_char *s, int n)
{
	while (n > 0)
	{
		if (m_maxcharacters)
		{
			--m_maxcharacters;
			OPPRINTF_DEPOSITC(*s);
		}
		s++;
		n--;
		m_count++;
	}
	return 0;
}

/* unused
int OpPrintf::out_str(const char *s, int n)
{
	while (n > 0)
	{
		if (m_maxcharacters)
		{
			--m_maxcharacters;
			OPPRINTF_DEPOSITC(*(const unsigned char*)s);
		}
		s++;
		n--;
		m_count++;
	}
	return 0;
}
*/

int OpPrintf::out_str(const void *s, int n)
{
	uni_char c = 0;
	if(m_mode == PRINTF_ASCII)
    {
		while (n > 0)
		{
			if (m_maxcharacters)
			{
				--m_maxcharacters;
				const unsigned char *ascii_buf = (const unsigned char *)s;
				c = ascii_buf[0];
				OPPRINTF_DEPOSITC(c);
			}
			s = NextCPtr(s);
			n--;
			m_count++;
		}
	}
	else
	{
		while (n > 0)
		{
			if (m_maxcharacters)
			{
				--m_maxcharacters;
				const uni_char *uni_buf = (const uni_char *)s;
				c = uni_buf[0];
				OPPRINTF_DEPOSITC(c);
			}
			s = NextCPtr(s);
			n--;
			m_count++;
		}
	}
	return 0;
}


/* Note the return statement! */

#define OPPRINTF_OUT_STR(S,N)					\
	do {										\
		if (out_str (S, N) != 0)				\
			return -1;							\
	} while (0)


int OpPrintf::out_pad(uni_char c, int n)
{
	while (n > 0)
	{
		if (m_maxcharacters)
		{
			--m_maxcharacters;
			OPPRINTF_DEPOSITC(c);
		}
		n--;
		m_count++;
	}
	return 0;
}


/* Note the return statement! */

#define OPPRINTF_OUT_PAD(C,N)					\
	do {										\
		if (out_pad (C, N) != 0)				\
			return -1;							\
	} while(0)


int OpPrintf::cvt_str(const void *str, int str_len)
{
	uni_char null = 0;

	if (str == NULL)
	{
		/* Print "(null)" for the NULL pointer if the precision is big
		   enough or not specified. */

		if (m_prec < 0 || m_prec >= 6)
		{
			if (m_mode == PRINTF_ASCII)
				str = "(null)";
			else
				str = UNI_L("(null)");
		}
		else
			str = &null;
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
				if ((m_mode == PRINTF_ASCII ? ((char*)str)[i] : ((uni_char*)str)[i]) == 0)
				{
					str_len = i;
					break;
				}
		}
		else
			str_len = (m_mode == PRINTF_ASCII ? op_strlen((char*)str) : uni_strlen((uni_char*)str));
	}
	else if (m_prec >= 0 && m_prec < str_len)
		str_len = m_prec;

	/* Print the string, using blanks for padding. */

	if (str_len < m_width && !m_minus)
		OPPRINTF_OUT_PAD(' ', m_width - str_len);
	OPPRINTF_OUT_STR(str, str_len);
	if (str_len < m_width && m_minus)
		OPPRINTF_OUT_PAD(' ', m_width - str_len);
	return 0;
}


int OpPrintf::cvt_number(const uni_char *pfx, const uni_char *str, const uni_char *xps, int lpad0, int rpad0, int is_signed, int is_neg)
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

	pfx_len = pfx ? uni_strlen(pfx) : 0;
	str_len = uni_strlen(str);
	xps_len = xps ? uni_strlen(xps) : 0;

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
		OPPRINTF_OUT_PAD(' ', m_width - min_len);

	/* Print the number. */

	if (sign != EOF)
		OPPRINTF_PUTC(sign);
	OPPRINTF_OUT_STR(pfx, pfx_len);
	OPPRINTF_OUT_PAD('0', lpad0);
	OPPRINTF_OUT_STR(str, str_len);
	OPPRINTF_OUT_PAD('0', rpad0);
	OPPRINTF_OUT_STR(xps, xps_len);

	/* Pad on the right with blanks. */

	if (min_len < m_width && m_minus)
		OPPRINTF_OUT_PAD(' ', m_width - min_len);
	return 0;
}

int OpPrintf::cvt_integer(const uni_char *pfx, const uni_char *str, int zero, BOOL is_signed, BOOL is_neg)
{
	int lpad0;

	if (zero && m_prec == 0)	/* ANSI */
		return 0;

	if (m_prec >= 0)			/* Ignore `0' if `-' is given for an integer */
		m_pad = ' ';

	/* This should be safe even if sizeof (size_t) > sizeof (int), because
	   the input str comes from a call to uni_ultoa(), and the length of
	   str should then always be < log10(ULONG_MAX).

	   And even if this would overflow, it would just cause the padding
	   of the string to look strange, there is a check to make sure we do
	   not output more characters than in the input string in cvt_string,
	   which is what cvt_number below eventually calls. */
	lpad0 = m_prec - (int) uni_strlen(str);
	return cvt_number(pfx, str, NULL, lpad0, 0, is_signed, is_neg);
}


int OpPrintf::cvt_hex(uni_char *str, char x, int zero)
{
	if (x == 'X')
		uni_strupr(str);
	return cvt_integer(((!zero && m_hash) ? (x == 'X' ? UNI_L("0X") : UNI_L("0x")) : NULL), str, zero, FALSE, FALSE);
}


int OpPrintf::cvt_hex_long(ULLONG n, char x)
{
	// One byte needs 2 characters, plus one for the terminator
	uni_char buf[sizeof(n) * 2 + 1]; // ARRAY OK 2011-03-08 peter

#if defined HAVE_LONGLONG || defined HAVE_UINT64
	uni_ui64toa(n, buf, 16);
#else
	uni_ultoa(n, buf, 16);
#endif
	return cvt_hex(buf, x, n == 0);
}


int OpPrintf::cvt_oct(const uni_char *str, int zero)
{
	size_t len;

	if (m_hash && str[0] != '0')
	{
		len = uni_strlen(str);
		if (m_prec <= (int) len)
			m_prec = (int) len + 1;
	}
	return cvt_integer(NULL, str, zero && !m_hash, FALSE, FALSE);
}


int OpPrintf::cvt_oct_long(ULLONG n)
{
	// One byte needs 8/3 characters, plus one for the rounding, and
	// one for the terminator
	uni_char buf[sizeof(n) * 8 / 3 + 2]; // ARRAY OK 2011-03-08 peter

#if defined HAVE_LONGLONG || defined HAVE_UINT64
	uni_ui64toa(n, buf, 8);
#else
	uni_ultoa(n, buf, 8);
#endif
	return cvt_oct(buf, n == 0);
}


int OpPrintf::cvt_dec_long(ULLONG n, BOOL is_signed, BOOL is_neg)
{
	// One byte needs log(256)/log(10) ~= 53/22 characters, plus one
	// for the rounding, and one for the terminator
	uni_char buf[(sizeof n) * 53 / 22 + 2]; // ARRAY OK 2012-08-15 peter

#if defined HAVE_LONGLONG || defined HAVE_UINT64
	uni_ui64toa(n, buf, 10);
#else
	uni_ultoa(n, buf, 10);
#endif
	return cvt_integer(NULL, buf, n == 0, is_signed, is_neg);
}

#if !defined EBERHARD_MATTES_PRINTF   				    // Need to use sprintf or snprintf in cvt_double?
#  if !defined HAVE_SPRINTF && !defined HAVE_SNPRINTF	// None is defined for us?
#    if defined HAVE_VSNPRINTF							// Synthesize sprintf from vsnprintf?

static int snprintf(char *buf, int max, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int res = vsnprintf(buf, max, fmt, args);	// NOT!! op_vsnprintf
	va_end(args);
	return res;
}

#define HAVE_SNPRINTF

#    elif defined HAVE_VSPRINTF						// Synthesize snprintf from vsprintf?

static int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int res = vsprintf(buf, fmt, args);	// NOT!! op_vsprintf
	va_end(args);
	return res;
}

#define HAVE_SPRINTF

#    endif
#  endif
#endif // !EBERHARD_MATTES_PRINTF

int OpPrintf::cvt_double(double x, uni_char fmt)
{
/* Member variables hold information about how to format:
		m_minus   for '-' flag
		m_plus    for '+' flag
		m_blank   for ' ' flag
		m_hash    for '#' flag
		m_pad     for pad character
		m_width   for field width (if not 0)
		m_prec    for precision (if not -1)  */

#ifdef STDLIB_DTOA_CONVERSION

	/* NaN and infinity are special */

	if (op_isnan(x))
	{
		return cvt_number(NULL, (fmt >= 'E' && fmt <= 'G') ? UNI_L("NAN") : UNI_L("nan"), NULL, 0, 0, op_signbit(x), op_signbit(x));
	}
	if (op_isinf(x))
	{
		return cvt_number(NULL, (fmt >= 'E' && fmt <= 'G') ? UNI_L("INF") : UNI_L("inf"), NULL, 0, 0, op_signbit(x), op_signbit(x));
	}

	uni_char outstring[DBL_MAX_10_EXP + 50]; // ARRAY OK 2011-03-08 peter

	if ((fmt == 'g' || fmt == 'G') && m_prec == 0)
		m_prec = 1;
	if (m_prec < 0)
		m_prec = 6;

	char *r = OpDoubleFormat::PrintfFormat(op_fabs(x), (char*)outstring, LOCAL_ARRAY_SIZE(outstring), (char)fmt, m_prec);
	if (r == NULL)	// OOM?
		return -1;

	make_doublebyte_in_place(outstring, (int) op_strlen((char*)outstring));
	return cvt_number(NULL, outstring, NULL, 0, 0, TRUE, op_signbit(x));	// op_signbit handles -0 too

#else

	/* Use native sprintf or vsprintf */
	/* FIXME: if the native printf does something locale-specifically evil,
	   like using "," for the decimal point, then we must fix that here.  */

	//
	// Build the 'fmtstring' string, piece by piece, to contain the e.g.
	// "%-10.10g" etc. needed for formatting.  The lines below marked with
	// 'Fills fmtstring max: <number>' indicates how many characters are
	// written into the fmtstring at that location. The sum of all these
	// markings, will give the maximum required size of 'fmtstring'.
	//
	// One signed integer is approx. 'sizeof(int)*5/2+1' characters
	// maximum (This is valid for 32, 64 and 128 bit signed ints).
	//
	// The current required minimum size is 8 chars plus two integers.
	// And adding 4 for safety...
	//
	char fmtstring[(sizeof(int)*5+2) + 8 + 4]; // ARRAY OK 2011-03-08 peter

	char outstring[DBL_MAX_10_EXP + 50]; // ARRAY OK 2011-03-08 peter

	char *p = fmtstring;
	*p++ = '%';						// Fills fmtstring max: 1
	if (m_minus) *p++ = '-';		// Fills fmtstring max: 1
	if (m_plus) *p++ = '+';			// Fills fmtstring max: 1
	if (m_blank) *p++ = ' ';		// Fills fmtstring max: 1
	if (m_pad) *p++ = m_pad;		// Fills fmtstring max: 1
	if (m_width)
	{
		op_itoa(m_width, p, 10);	// Fills fmtstring max: int
		while (*p != 0)
			p++;
	}
	if (m_prec >= 0)
	{
		*p++ = '.';					// Fills fmtstring max: 1
		op_itoa(m_prec, p, 10);		// Fills fmtstring max: int
		while (*p != 0)
			p++;
	}
	*p++ = (char)fmt;				// Fills fmtstring max: 1
	*p++ = 0;						// Fills fmtstring max: 1

# ifdef HAVE_SNPRINTF
	// Use native snprintf, NOT op_snprintf (that would loop back here)
	snprintf(outstring, sizeof(outstring), fmtstring, x);
# elif defined HAVE_SPRINTF
	// Use native sprintf, NOT op_sprintf (that would loop back here)
	sprintf(outstring, fmtstring, x);
# else
	// Use module-local porting interface
	StdlibPI::SPrintfDouble(outstring, sizeof(outstring), fmtstring, x);
# endif
	OPPRINTF_OUT_STR(outstring, op_strlen(outstring));

	return 0;

#endif // STDLIB_DTOA_CONVERSION
}

#define OpPrintf_PeekFormat()        (GETC(format))

#define OpPrintf_PeekNextFormat()	 (GETC(NextCPtr(format)))

#define OpPrintf_NextFormat()        do { format = NextCPtr(format); } while (0)

int OpPrintf::uni_output(void *output, int max, const void *format, va_list arg_ptr, BOOL check_length)
{
	uni_char size, c, q;

	/* Initialize variables. */

	if (check_length)
		max = 0;

	m_output = output;
	m_maxcharacters = max;
	m_count = 0;

	/* Interpret the string. */

	while ((c = OpPrintf_PeekFormat()) != 0)
	{
		if (c != '%')
		{
			/* ANSI X3.159-1989, 4.9.6.1: "... ordinary multibyte
			   charcters (not %), which are copied unchanged to the output
			   stream..."

			   Avoid the overhead of calling mblen(). */

			OPPRINTF_PUTC(c);
			OpPrintf_NextFormat();
		}
		else if (OpPrintf_PeekNextFormat() == '%')
		{
			/* ANSI X3.159-1989, 4.9.6.1: "The complete conversion
			   specification shall be %%." */

			OPPRINTF_PUTC('%');
			OpPrintf_NextFormat();
			OpPrintf_NextFormat();
		}
		else
		{
			m_minus = m_plus = m_blank = m_hash = FALSE;
			m_width = 0;
			m_prec = -1;
			size = 0;
			m_pad = ' ';

			/* Flags */

			for (;;)
			{
				OpPrintf_NextFormat();
				switch (OpPrintf_PeekFormat())
				{
				case '-': m_minus = TRUE;	continue;
				case '+': m_plus = TRUE;	continue;
				case '0': m_pad = '0';		continue;
				case ' ': m_blank = TRUE;	continue;
				case '#': m_hash = TRUE;	continue;
				}
				break;
			}

			if (m_minus)
				m_pad = ' ';		/* `-' overrides `0' */

			if (m_plus)
				m_blank = FALSE;	/* '+' overrides ' ' */

			/* Field width */

			if (OpPrintf_PeekFormat() == '*')
			{
				OpPrintf_NextFormat();
				m_width = va_arg(arg_ptr, int);

				if (m_width < 0)
				{
					m_width = -m_width;
					m_minus = TRUE;
				}
			}
			else
			{
				q = OpPrintf_PeekFormat();
				while (q >= '0' && q <= '9')
				{
					m_width = m_width * 10 + (q - '0');
					OpPrintf_NextFormat();
					q = OpPrintf_PeekFormat();
				}
			}

			/* Precision */

			if (OpPrintf_PeekFormat() == '.')
			{
				OpPrintf_NextFormat();
				if (OpPrintf_PeekFormat() == '*')
				{
					OpPrintf_NextFormat();
					m_prec = va_arg(arg_ptr, int);

					if (m_prec < 0)
						m_prec = -1;	/* We don't need this */
				}
				else
				{
					q = OpPrintf_PeekFormat();
					m_prec = 0;
					while (q >= '0' && q <= '9')
					{
						m_prec = m_prec * 10 + (q - '0');
						OpPrintf_NextFormat();
						q = OpPrintf_PeekFormat();
					}
				}
			}

			/* Size */

			/* Not supported here is 'j' -- 'intmax_t' */
			/* Not supported here is 'z' -- 'size_t' */
			/* Not supported here is 't' -- 'ptrdiff_t' */
			/* Note, the code overloads 'll' -> 'L' and can get away with it because
			   L is used only with floating formats and ll with integer formats,
			   but it means error checking will be compromised.  */

			q = OpPrintf_PeekFormat();
			if (q == 'h' || q == 'l' || q == 'L')
			{
				size = q;
				OpPrintf_NextFormat();
				if (size == 'l' && OpPrintf_PeekFormat() == 'l')
				{
#if defined HAVE_LONGLONG || defined HAVE_INT64
					size = 'L';
#else
					OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
					OpPrintf_NextFormat();
				}
				else if (size == 'h' && OpPrintf_PeekFormat() == 'h')
				{
					size = 'H';
					OpPrintf_NextFormat();
				}
			}

			/* Format */

			/* Not supported here are 'a', 'A' */
			/* BUG: %lln not supported */

			switch (OpPrintf_PeekFormat())
			{
			case 0:
				return m_count;

			case 'n':
				OP_ASSERT(!"Don't use %n format due to security issues");
				switch (size)
				{
				case 'H':
					{
						char *ptr = va_arg(arg_ptr, char *);
						*ptr = m_count;
					}
					break;
				case 'h':
					{
						short *ptr = va_arg(arg_ptr, short *);
						*ptr = m_count;
					}
					break;
				case 'l':
					{
						long int *ptr = va_arg(arg_ptr, long int *);
						*ptr = m_count;
					}
					break;
#ifdef HAVE_LONGLONG
				case 'L':
					{
						long long int *ptr = va_arg(arg_ptr, long long int *);
						*ptr = m_count;
					}
					break;
#elif defined HAVE_INT64
				case 'L':
					{
						INT64 *ptr = va_arg(arg_ptr, INT64 *);
						*ptr = m_count;
					}
					break;
#endif
				default:
					{
						int *ptr = va_arg(arg_ptr, int *);

						*ptr = m_count;
					}
					break;
				}
				break;

			case 'c':
				{
					uni_char c1;
					char c2;

					c1 = (uni_char) va_arg(arg_ptr, unsigned);
					c2 = (char)c1;

					m_prec = 1;
					if (m_mode == PRINTF_ASCII)
						OPPRINTF_CHECK(cvt_str((void*)&c2, 1));
					else
						OPPRINTF_CHECK(cvt_str((void*)&c1, 1));
				}
				break;

			case 's':
				OPPRINTF_CHECK(cvt_str(va_arg(arg_ptr, void *), -1));
				break;

			case 'd':
			case 'i':
				{
					LLONG n;
					switch (size)
					{
					case 0:
					default:
						n = va_arg(arg_ptr, int);
						break;

					case 'h':
						n = (short) va_arg(arg_ptr, int);
						break;

					case 'H':
						n = (char) va_arg(arg_ptr, int);
						break;

					case 'l':
						n = va_arg(arg_ptr, long int);
						break;

					case 'L':
#if defined HAVE_LONGLONG || defined HAVE_INT64
						n = va_arg(arg_ptr, LLONG);
#else
						OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
						break;
					}

					if (n < 0)
					{
						ULLONG o = -n;
						OPPRINTF_CHECK(cvt_dec_long(o, TRUE, TRUE));
					}
					else
						OPPRINTF_CHECK(cvt_dec_long(n, TRUE, FALSE));
				}
				break;

			case 'u':
				{
					ULLONG n;
					switch (size)
					{
					case 0:
					default:
						n = va_arg(arg_ptr, unsigned);
						break;

					case 'h':
						n = (unsigned short) va_arg(arg_ptr, unsigned);
						break;

					case 'H':
						n = (unsigned char) va_arg(arg_ptr, unsigned);
						break;

					case 'l':
						n = va_arg(arg_ptr, long unsigned);
						break;

					case 'L':
#if defined HAVE_LONGLONG || defined HAVE_UINT64
						n = va_arg(arg_ptr, ULLONG);
#else
						OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
						break;
					}

					OPPRINTF_CHECK(cvt_dec_long(n, FALSE, FALSE));
				}
				break;

			case 'p':
				m_hash = TRUE;
#ifdef SIXTY_FOUR_BIT
				OPPRINTF_CHECK(cvt_hex_long(va_arg(arg_ptr, UINT64), 'x'));
#else
				OPPRINTF_CHECK(cvt_hex_long(va_arg(arg_ptr, UINT32), 'x'));
#endif

				break;

			case 'x':
			case 'X':
				{
					ULLONG n;
					switch (size)
					{
					case 0:
					default:
						n = va_arg(arg_ptr, unsigned);
						break;

					case 'h':
						n = (unsigned short) va_arg(arg_ptr, unsigned);
						break;

					case 'H':
						n = (unsigned char) va_arg(arg_ptr, unsigned);
						break;

					case 'l':
						n = va_arg(arg_ptr, long unsigned);
						break;

					case 'L':
#if defined HAVE_LONGLONG || defined HAVE_UINT64
						n = va_arg(arg_ptr, ULLONG);
#else
						OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
						break;
					}

					OPPRINTF_CHECK(cvt_hex_long(n, (char) OpPrintf_PeekFormat()));
				}
				break;

			case 'o':
				{
					ULLONG n;
					switch (size)
					{
					case 0:
					default:
						n = va_arg(arg_ptr, unsigned);
						break;

					case 'h':
						n = (unsigned short) va_arg(arg_ptr, unsigned);
						break;

					case 'H':
						n = (unsigned char) va_arg(arg_ptr, unsigned);
						break;

					case 'L':
#if defined HAVE_LONGLONG || defined HAVE_UINT64
						n = va_arg(arg_ptr, ULLONG);
#else
						OP_ASSERT(!"It is a very bad idea to use the ll modifier if your platform doesn't actually support 64-bit integers!");
#endif
						break;

					case 'l':
						n = va_arg(arg_ptr, long unsigned);
						break;
					}

					OPPRINTF_CHECK(cvt_oct_long(n));
				}
				break;

			case 'g':
			case 'G':
			case 'e':
			case 'E':
			case 'f':
			case 'F':
				{
					double x;
					if (size == 'L')
						x = (double)va_arg(arg_ptr, long double);	// No support for long double in the printf yet
					else
						x = va_arg(arg_ptr, double);
					OPPRINTF_CHECK(cvt_double(x, OpPrintf_PeekFormat()));
				}
				break;

			default:

				/* ANSI X3.159-1989, 4.9.6.1: "If a conversion
				   specification is invalid, the behavior is undefined."

				   We print the last letter only. */

				OPPRINTF_OUT_STR(format, 1);
				break;
			}
			OpPrintf_NextFormat();
		}
	}

	/* Make sure output is null terminated unless we weren't allowed to write */
	if (max)
	{
		if (!m_maxcharacters)
			m_output = (void*)((char*)m_output - m_mode - 1);

		if (m_mode == PRINTF_ASCII) *(char*)m_output = 0; else *(uni_char*)m_output = 0;
	}

	/* Return the number of characters printed.  This will be correct even if
	   the buffer could not accomodate all the output, since processing does
	   not stop if the buffer is full -- only outputting stops.  */

	return m_count;
}


#undef OpPrintf_PeekFormat
#undef OpPrintf_PeekNextFormat
#undef OpPrintf_NextFormat

#undef OPPRINTF_CHECK
#undef OPPRINTF_DEPOSITC
#undef OPPRINTF_PUTC
#undef OPPRINTF_OUT_STR
#undef OPPRINTF_OUT_PAD

#endif // EBERHARD_MATTES_PRINTF
