/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// File name:          datefun.h
// Contents:           Generic date functions
//
// This file contains functions for working with dates.

#ifndef MODULES_UTIL_DATEFUN_H
#define MODULES_UTIL_DATEFUN_H

#ifdef UTIL_GET_THIS_YEAR
/**
 * GetThisYear
 *
 * Calculates which year it is.
 *
 * @param[out] month \c NULL or a pointer to where to store the month number,
 * from 1 (for January) to 12 (for December).
 * @return Current year (includes century).
 */
int		GetThisYear(unsigned int *month = NULL);
#endif // UTIL_GET_THIS_YEAR

#ifdef UTIL_MAKE_DATE3
/**
 * MakeDate3
 *
 * Given the number of seconds since the epoch it produces a string on
 * the form: Thu, 01 Jan 1970 00:00:00.
 *
 * @param t The number of seconds since the epoch.
 * @param buf Room for the produced string.
 * @param buf_len The maximum length of the produced string.
 * @return TRUE on success, FALSE on failure.
 */
BOOL	MakeDate3(time_t* t, uni_char* buf, size_t buf_len);
#endif // UTIL_MAKE_DATE3

#ifdef UTIL_FMAKE_DATE
/**
 * FMakeDate
 *
 * Constructs a date string from a tm struct on a specified format.
 * The format can contain the following special characters preceded
 * by a \247: w - weekday as a string, D - day as a number, M - month
 * as a number, n - month as a string, h - hours, m - minutes, s -
 * seconds, Y - year as a four digit number, y - year as a two digit
 * number.
 *
 * @param gt a tm struct specifying the date to construct the date from.
 * @param format a string specifying the format of the date ("\247w
 * \247D/\247M-\247y \247h:\247m:\247s" gives for example Wed 29/11-06
 * 15:50:17).
 * @param buf the user supplied buffer to contain the date string.
 * @param buf_len the size of the user supplied buffer.
 * @return TRUE on success, FALSE on failure.
 */
BOOL 	FMakeDate(struct tm gt, const char* format, uni_char* buf, size_t buf_len);
#endif // UTIL_FMAKE_DATE

#ifdef UTIL_GET_TM_DATE
/**
 * GetTmDate
 *
 * Given a date string GetTmDate produces a tm struct corresponding to
 * the string.  Supports an eclectic diversity of formats.
 *
 * @param date_str the date string.
 * @param gmt_time passed by reference contains the date upon success.
 * @return 0 on failure and 1 otherwise.
 */
BOOL	GetTmDate(const uni_char* date_str, struct tm &gmt_time);
#endif // UTIL_GET_TM_DATE

#ifdef UTIL_GET_DATE
/**
 * GetDate
 *
 * Given a date string GetDate returns the number of seconds since
 * the epoch to the specified date.
 *
 * @param date_str the date string.
 * @return Number of seconds since the epoch to the specified date.
 */
time_t	GetDate(const char* date_str);
time_t	GetDate(const uni_char* date_str);
#endif // UTIL_GET_DATE

#ifdef UTIL_CHECKED_STRFTIME
/**
 * Calls op_strftime after having checked date and time.
 *
 * @return OpStatus::ERR_OUT_OF_RANGE for illegal date or
 * time. OpStatus::ERR if buffer too small.
 */
OP_STATUS CheckedStrftime(uni_char *date_buf, unsigned int buf_len, const uni_char *fmt, struct tm *tm);
#endif // UTIL_CHECKED_STRFTIME

#endif // !MODULES_UTIL_DATEFUN_H
