/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPERA_STDLIB_UNI_PRINTF_H
#define OPERA_STDLIB_UNI_PRINTF_H

#ifdef EBERHARD_MATTES_PRINTF

/*
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

class OpPrintf
{
public:

	enum Mode				// Do not change the values here!
	{ 
		PRINTF_ASCII=0,		///< This printer is for 'char' data
		PRINTF_UNICODE=1	///< This printer is for 'uni_char' data
	};

	/** Construct a printer for char or uni_char data */
	OpPrintf(Mode mode);

	/** Produce formatted output from a 'char' type format string, with
	 * char strings as string arguments.
	 * 
	 * @param   output   where to print to.  May be NULL if max is zero
	 * @param   max      maximum characters to print
	 * @param   format   format specifier
	 * @param   arg_ptr  arguments list
	 * @return  number of characters that would be printed, assuming the
	 *          output buffer could hold all the output.  If the return
	 *          value is less than max, then the output is correct.  Otherwise,
	 *          the output is truncated early.
	 *
	 * Unless max is zero, the output is always terminated by a NUL byte.
	 */
	int Format(char *output, int max, const char *format, va_list arg_ptr);

	/** Produce formatted output from a 'uni_char' type format string, with
	 * uni_char strings as string arguments.
	 */
	int Format(uni_char *output, int max, const uni_char *format, va_list arg_ptr);

private:
	
	// Local variables of UniPrintf

	const Mode m_mode;			/**< Once set, it never changes */
	void *m_output;				/**< Where output should go */
	BOOL m_minus;				/**< Non-zero if `-' flag present */
	BOOL m_plus;				/**< Non-zero if `+' flag present */
	BOOL m_blank;				/**< Non-zero if ` ' flag present */
	BOOL m_hash;				/**< Non-zero if `#' flag present */
	char m_pad;					/**< Pad character (' ' or '0')  */
	int m_width;				/**< Field width (or 0 if none specified) */
	int m_prec;					/**< Precision (or -1 if none specified) */
	int m_count;				/**< Number of characters printed */
	int m_maxcharacters;		/**< Maximum allowed characters (snprintf) */

	/* Print the first N characters of the string S. */
	int out_str(const uni_char *s, int n);

	/* Print the first N characters of the singlebyte string S. */

//	int out_str(const char *s, int n);

	int out_str(const void *s, int n);

	/* Print the character C N times. */
	int out_pad(uni_char c, int n);

	/* Perform formatting for the "%c" and "%s" formats.  The meaning of
	   the '0' flag is not defined by ANSI for "%c" and "%s"; we always
	   use blanks for padding. */
	int cvt_str(const void *str, int str_len);

	/* Print and pad a number (which has already been converted into
	   strings).  PFX is "0x" or "0X" for hexadecimal numbers, "0." for
	   floating point numbers in [0.0,1.0) unless using exponential
	   format, the significant digits for large floating point numbers in
	   the "%f" format, or NULL.  Insert LPAD0 zeros between PFX and STR.
	   STR is the number.  Insert RPAD0 zeros between STR and XPS.  XPS is
	   the exponent (unless it is NULL).  IS_SIGNED is non-zero when
	   printing a signed number. IS_NEG is non-zero for negative numbers
	   (the strings don't contain a sign). */

	int cvt_number(const uni_char *pfx, const uni_char *str, const uni_char *xps, int lpad0, int rpad0, int is_signed, int is_neg);

	/* Print and pad an integer (which has already been converted into the
	   string STR).  PFX is "0x" or "0X" for hexadecimal numbers, or NULL.
	   ZERO is non-zero if the number is zero.  IS_NEG is non-zero if the
	   number is negative (the string STR doesn't contain a sign). */

	int cvt_integer(const uni_char *pfx, const uni_char *str, int zero, BOOL is_signed, BOOL is_neg);

	int cvt_hex(uni_char *str, char x, int zero);
	int cvt_oct(const uni_char *str, int zero);
#ifdef HAVE_LONGLONG
	int cvt_hex_long(unsigned long long n, char x);
	int cvt_oct_long(unsigned long long n);
	int cvt_dec_long(unsigned long long n, BOOL is_signed, BOOL is_neg);
#elif defined HAVE_INT64 && defined HAVE_UINT64
	int cvt_hex_long(UINT64 n, char x);
	int cvt_oct_long(UINT64 n);
	int cvt_dec_long(UINT64 n, BOOL is_signed, BOOL is_neg);
#else
	int cvt_hex_long(unsigned long n, char x);
	int cvt_oct_long(unsigned long n);
	int cvt_dec_long(unsigned long n, BOOL is_signed, BOOL is_neg);
#endif

	int cvt_double(double x, uni_char fmt);

	/* This is the work horse for uni_sprintf() and friends. */

	int uni_output(void *output, int max, const void *format, va_list arg_ptr, BOOL check_length);

	/* get next unsigned char or uni_char from buffer according to the mode */
    uni_char get_c(const void *buf) const;
};

#endif // EBERHARD_MATTES_PRINTF

#endif // OPERA_STDLIB_UNI_PRINTF_H
