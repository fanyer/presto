/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare (most) functionality of the Debug class.
 *
 * Some of the methods declared here must be implemented
 * by the platform somewhere else.
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG_ENABLE_PRINTF
/**
 * \brief Formatted output function for debugging purposes
 *
 * This function will perform a very simplistic 'printf' kind
 * of formatting and output the result to the relevant system console
 * and the opera log-file.
 *
 * The following items can be formatted and printed:
 *
 *  \%d - signed decimal (int)<br>
 *  \%u - unsigned decimal (unsigned int)<br>
 *  \%x - unsigned hexadecimal <br>
 *  \%z - size_t decimal <br>
 *  \%p - pointer <br>
 *  \%c - a single character (16-bit unicode values supported) <br>
 *  \%s - a zero terminated C-string in ISO 8859-1 encoding <br>
 *  \%S - a zero terminated 16-bit unicode string (uni_char*) <br>
 *  \%.*s - Like \%s but give max size as argument befor string pointer <br>
 *  \%\% - print a single '\%' <br>
 *
 * \param format Format string
 */
extern "C" void dbg_printf(const char* format, ...);

/**
 * \brief A varargs version of dbg_printf
 *
 * \see dbg_printf for details on the format string.
 *
 * \param format Format string
 * \param args The varargs list iterator
 */
extern "C" void dbg_vprintf(const char* format, va_list args);

/**
 * \brief Formating output function for debug logging
 *
 * This function performs the exact same operation as \c dbg_printf()
 * but only outputs to the opera log-file.  This function is typically
 * used for larger volumes of data, while dbg_printf() are used for
 * alerts and statistics.
 *
 * \param format Format string with same features as for dbg_printf()
 */
extern "C" void log_printf(const char* format, ...);

#endif // DEBUG_ENABLE_PRINTF

#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT
/**
 * \brief Output a unicode string to the device system console
 *
 * This function needs to be implemented by the platform.
 * It should output to the screen, a debugging console, file
 * or other suitable place.
 *
 * If the platform does not have a suitable output mechanism, the
 * function should still be implemented but does nothing.
 *
 * \param txt The 16-bit unicode string (zero terminated) to print
 */
extern "C" void dbg_systemoutput(const uni_char* txt);

/**
 * \brief Output a utf8 string to the opera log-file
 *
 * This function must be implemented on the platform, possibly as a
 * stub.  The expected behaviour is that the platform will open the
 * logfile at some point, and the debug module will call this function
 * to record data in the file.  The contents written to the file
 * should be a byte-wise copy of the data provided by the debug
 * module.  The debug module will do the utf8 encoding, and this
 * should not be changed, to make all log-files compatible.  This same
 * goes for the Newline character '\n', which should preferably not be
 * rewritten into '\r\n'.
 *
 * The platform may decide to implement a write buffer for this
 * function, to avoid writing many small pieces to the logfile, or to
 * buffer data until some necessary information is available, like
 * the name of the logfile.
 *
 * The debug module will call this function once with a zero length
 * parameter during its initialization; the platform may opt to do
 * lazy initialization at this point, if everything else is ready.
 */
extern "C" void dbg_logfileoutput(const char* txt, int len);

#endif // DEBUG_ENABLE_SYSTEM_OUTPUT

#endif // DEBUG_H
