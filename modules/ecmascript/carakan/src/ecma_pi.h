/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995-2005
 *
 * Porting interface abstractions, to be used by the engine
 * Lars T Hansen
 */

#ifndef ECMA_PI_H
#define ECMA_PI_H

class FramesDocument;

class ES_ImportedAPI
{
public:
	static void PostGCMessage(unsigned int timeout = 0);
		/**< Post a message that will eventually trigger a GC. */

	static void PostMaintenanceGCMessage(unsigned int timeout);
		/**< Post a message to trigger the periodic maintenance GC. */

	enum DateFormatSpec
	{
		GET_DATE,
		GET_TIME,
		GET_DATE_AND_TIME
	};

	struct TimeElements
	{
		int year;			///< Year number; the year of Norway's independence from Sweden is '1905'.
		int month;			///< Month number, January is '0'
		int day_of_week;	///< Week day, Sunday is '0' and Saturday is '6'
		int day_of_month;	///< Day of the month, the first day of a month is '1'
		int hour;			///< Hour of the day, the first hour is '0'
		int minute;			///< Minute of the hour, the first minute is '0'
		int second;			///< Second of the minute, the first second is '0'
		int millisecond;	///< Millisecond of the second, the first millisecond is '0'
	};

	static OP_STATUS FormatLocalTime(DateFormatSpec how, uni_char *buf, unsigned length, TimeElements* time);
		/**< Attempt to format a time stamp in a locale-specific fashion.
		     @param how whether to get date, time, or both.
		     @param [out]buf result buffer.
		     @param length character length of the result buffer.
		     @param time time information.
		     @return OpStatus::OK if the formatting succeeded, OpStatus::ERR if the
		             formatting failed and the engine should use a default routine.
		             OpStatus::ERR_NO_MEMORY on OOM. */

#if defined OPERA_CONSOLE
	static void PostToConsole(const uni_char* message, FramesDocument* fd);
		/**< Post an error message to the Opera Console along with information
		     about the originating document.
		     @param message The error message, never NULL. May be quite large,
		            as it may contain a backtrace. Buffer owned by the caller,
		            may be destroyed on return.
		     @param fd The FramesDocument of the document that signalled the error.
		            May be NULL. */
#endif // OPERA_CONSOLE
};
#endif // ECMA_PI_H
