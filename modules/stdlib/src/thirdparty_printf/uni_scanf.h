/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPERA_STDLIB_UNI_SCANF_H
#define OPERA_STDLIB_UNI_SCANF_H

#ifdef EBERHARD_MATTES_PRINTF

/*
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

class OpScanf
{
public:
	enum Mode				// Do not change the assigned values
	{
		SCANF_ASCII=0,		///< Scan char input with a char format
		SCANF_UNICODE=1		///< Scan uni_char input with a uni_char format
	};

	/** Construct an input parser for a particular kind of input */
	OpScanf(Mode mode);

	/**
	 * Entry to Unicode-enabled version of uni_sscanf(). Only called
	 * from uni_sscanf();
	 * @param input   Data to scan.
	 * @param format  Format string to scan in.
	 * @param arg_ptr Vararg parameter to result variables.
	 * @return Number of input items assigned. Zero on matching failure.
	 */
	int Parse(const uni_char *input, const uni_char *format, va_list arg_ptr);
	int Parse(const char *input, const char *format, va_list arg_ptr);

private:

	enum states
	{
		ISOK,						/**< Situation normal */
		END_OF_FILE,				/**< End of file (stop at next directive) */
		MATCHING_FAILURE,			/**< Matching failure (stop immediately) */
		INPUT_FAILURE,				/**< Input failure (stop immediately) */
		OUT_OF_MEMORY				/**< Out of memory (stop immediately) */
	};

	const Mode m_mode;
	const void *m_input;		/**< Where input comes from */
	int m_width;					/**< Field width or what's left over of it */
	int m_chars;					/**< Number of characters read */
	int volatile m_count;			/**< Number of fields assigned */
	char m_size;					/**< Size (0, 'h', 'l' or 'L') */
	BOOL m_width_given;				/**< A field width has been given */
	enum states m_status;

	/**Skip leading whitespace of an input field and return the first
	   non-whitespace character.  Hitting EOF is treated as input failure.
	   Don't forget to check `m_status' after calling this function. */
	int skip();

	/** Return the next character of the current input field.  Return EOF
	    if the field width is exhausted or on input failure. */
	int get();

	void inp_char(void *dst);
	void inp_str(void *dst);
	void inp_set(const void **pfmt, void *dst);
	void inp_int_base(void *dst, int base);
	void inp_int(unsigned char f, void *dst);
	double inp_float(unsigned char f);
	int inp_main(const void *input, const void *format, va_list arg_ptr);

	/* get next unsigned char or uni_char from buffer according to the mode */
	uni_char get_c(const void *buf) const;
};

#endif // EBERHARD_MATTES_PRINTF

#endif // OPERA_STDLIB_UNI_SCANF_H
