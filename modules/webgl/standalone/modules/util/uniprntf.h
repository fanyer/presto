/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// File name:          uniprintf.h
// Contents:           Declarations for uni_sprintf helper

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

#ifndef __UNIPRNTF_H
#define __UNIPRNTF_H

#if !defined(MSWIN) || defined(_OPERA_STRING_LIBRARY_)

#include <stdarg.h>

/**
 * Support for Unicode-enabled version of printf().
 */

class UniPrintf
{
public:
	/**
	 * Entry to Unicode-enabled version of uni_printf(). Only called
	 * from uni_printf();
	 * @param output  Where to print to.
	 * @param max     Maximum characters to print.
	 * @param format  Format string to print according to.
	 * @param arg_ptr Vararg parameter to sopurce variables.
	 * @return Number of characters printed.
	 */
	static int Output(uni_char *output, int max, const uni_char *format, va_list arg_ptr);

private:
		
	/*
	friend int uni_snprintf(uni_char * buffer, size_t count,
	                        const uni_char * format, ...);
	friend int uni_vsprintf(uni_char * buffer, const uni_char * format,
	                        va_list arg_ptr);

	friend int uni_vsnprintf(uni_char * buffer, size_t count,
	                         const uni_char * format, va_list arg_ptr);
	friend int uni_sprintf(uni_char * buffer, const uni_char * format, ...);
	*/
	

	// Local variables of UniPrintf

	uni_char *m_output;			/**< Where output should go */
	bool m_minus;				/**< Non-zero if `-' flag present */
	bool m_plus;				/**< Non-zero if `+' flag present */
	bool m_blank;				/**< Non-zero if ` ' flag present */
	bool m_hash;				/**< Non-zero if `#' flag present */
	uni_char m_pad;				/**< Pad character (' ' or '0')  */
	int m_width;				/**< Field width (or 0 if none specified) */
	int m_prec;					/**< Precision (or -1 if none specified) */
	int m_count;				/**< Number of characters printed */
	int m_dig;					/**< DBL_DIG / LDBL_DIG for __small_dtoa() */
	int m_maxcharacters;		/**< Maximum allowed characters (snprintf) */

	/** Print the first N characters of the string S. */
	int out_str(const uni_char *s, int n);

	/** Print the first N characters of the singlebyte string S. */

	int out_str_upsize(const char *s, int n);

	/** Print the character C N times. */
	int out_pad(uni_char c, int n);

	/**Perform formatting for the "%c" and "%s" formats.  The meaning of
	   the '0' flag is not defined by ANSI for "%c" and "%s"; we always
	   use blanks for padding. */
	int cvt_str(const uni_char *str, int str_len);

	/**Print and pad a number (which has already been converted into
	   strings).  PFX is "0x" or "0X" for hexadecimal numbers, "0." for
	   floating point numbers in [0.0,1.0) unless using exponential
	   format, the significant digits for large floating point numbers in
	   the "%f" format, or NULL.  Insert LPAD0 zeros between PFX and STR.
	   STR is the number.  Insert RPAD0 zeros between STR and XPS.  XPS is
	   the exponent (unless it is NULL).  IS_SIGNED is non-zero when
	   printing a signed number. IS_NEG is non-zero for negative numbers
	   (the strings don't contain a sign). */

	int cvt_number(const uni_char *pfx,
				   const uni_char *str, const uni_char *xps,
				   int lpad0, int rpad0, int is_signed, int is_neg);

	/**Print and pad an integer (which has already been converted into the
	   string STR).  PFX is "0x" or "0X" for hexadecimal numbers, or NULL.
	   ZERO is non-zero if the number is zero.  IS_NEG is non-zero if the
	   number is negative (the string STR doesn't contain a sign). */

	int cvt_integer(const uni_char *pfx,
					const uni_char *str, int zero, bool is_signed,
					bool is_neg);

	int cvt_hex(uni_char *str, char x, int zero);
	int cvt_hex_32(unsigned n, char x);
	int cvt_oct(const uni_char *str, int zero);
	int cvt_oct_32(unsigned n);
	int cvt_dec_32(unsigned n, bool is_signed, bool is_neg);

	int cvt_longdouble(long double x,
					   const uni_char *fmtbegin,
					   const uni_char *fmtend);
	int cvt_double(double x, const uni_char *fmtbegin,
				   const uni_char *fmtend);

	/** This is the working horse for uni_sprintf() and friends. */
	int uni_output(uni_char *output, int max, const uni_char *format,
			       va_list arg_ptr);

};

#endif

#endif
