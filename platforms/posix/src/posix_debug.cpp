/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef POSIX_OK_LOGGER
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif
#include "platforms/posix/posix_logger.h"

// static
bool PosixLogger::PError(const char *sysfunc, int err,
						 Str::LocaleString message, const char *fallback)
{
	// TODO: use an error logger, that defaults to stderr, for this.
#ifdef POSIX_OK_NATIVE
	PosixNativeUtil::Perror(message, sysfunc, err);
#else
	errno = err;
	perror(fallback);
	errno = 0;
#endif
	return true;
}

#ifdef POSIX_OK_LOG_STDIO

// static
void PosixLogToStdIO::UTF8Report(const uni_char *text, FILE *fd)
{
	while (const uni_char c = *text++)
	{
		// Simple conversion to utf8:
		if ( c < 0x80 )
			putc(c, fd);

		else if ( c < 0x800 )
		{
			putc(0xc0 | (c >> 6), fd);
			putc(0x80 | (c & 0x3f), fd);
		}
		else
		{
			putc(0xe0 | (c >> 12), fd);
			putc(0x80 | ((c >> 6) & 0x3f), fd);
			putc(0x80 | (c & 0x3f), fd);
		}
	}
}
#endif // POSIX_OK_LOG_STDIO

#ifdef POSIX_OK_DEBUG // Rest of file

#define PosixLoggerOfType(type) \
	(g_opera && g_posix_log_boss ? g_posix_log_boss->Get(PosixLogger::type) : 0)
# ifdef POSIX_GLOBAL_ASSERT_EAR
PosixLogListener *posix_assert_ear = 0;
# else
#define posix_assert_ear PosixLoggerOfType(ASSERT)
# endif

#include "modules/debug/debug.h"				// declarations to be consistent with
# ifdef SELFTEST
#include "modules/selftest/optestsuite.h"		// TestSuite
# endif

# ifdef DEBUG_ENABLE_SYSTEM_OUTPUT
extern "C" void dbg_systemoutput(const uni_char* str)
{
	PosixLogger vox(PosixLogger::SYSDBG);
	if (!vox.Log(PosixLogger::NORMAL, str))
		PosixLogToStdIO::UTF8Report(str, stdout);
}

extern "C" void dbg_logfileoutput(const char* txt, int len)
{
	if (len)
	{
		// '%', '.', digits, 's', '\0'
		char format[INT_STRSIZE(len) + 4]; // ARRAY OK 2010-08-12 markuso
		op_sprintf(format, "%%.%ds", len);
		PosixLogger::Log(PosixLogger::LOGDBG, PosixLogger::NORMAL, format, txt);
	}
}
# endif // DEBUG_ENABLE_SYSTEM_OUTPUT

# ifdef _DEBUG
#include <sys/time.h>
void Debug::GetTime(DebugTime& time)
{
	// TODO: use clock_gettime if available
	struct timeval tp;
	struct timezone tzp;
	gettimeofday(&tp, &tzp);
	time.sec = tp.tv_sec;
	time.msec = (UINT16)(tp.tv_usec / 1000);
}
# endif // _DEBUG

# ifdef DEBUG_ENABLE_OPASSERT
inline void PosixAssertLogger::Log(PosixLogger::Loquacity level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	PosixLogToStdIO::Report(level, format, args);
	va_end(args);
}

bool PosixAssertLogger::Report(PosixLogger::Loquacity level,
							   const char *format,
							   va_list args)
{
	if (!PosixLogToStdIO::Report(level, format, args))
		return false;

#ifdef SELFTEST
	if (g_opera)
	{
		const char *module, *group;
		TestSuite::GroupInfo(-1, group, module);

		if (module || group)
			Log(level, ", selftest: %s:%s",
				module ? module : "???", group ? group : "???");
	}
#endif // SELFTEST
	Log(level, "\n");

#ifdef POSIX_INTERACTIVE_ASSERT
	if (m_stop)
	{
		/* NB: asking user what to do next via stdout/stdin, not the stream used
		 * for reporting errors.  This is deliberate.
		 */
		const bool ephemeral = !(this == posix_assert_ear);

		if (ephemeral)
			fputs("### Assertion failure handled by transient listener:"
				  " hit return when ready.\n> ", stdout);
		else
			fputs("### What do you want to do when assertions fail ?\n"
				  "### \tC: Continue (default), but ask again each time\n"
				  "### \tP: Print future assertion failures and continue from now on\n"
				  "### \t\texport OPASSERT_CONTINUE=1\n"
				  "### \t   acts like P with no interaction required\n"
				  "> ", stdout);

		char str[50]; // ARRAY OK 2010-08-12 markuso
		fgets(str, 49, stdin);

		if (!ephemeral)
			switch (str[0])
			{
			case 'P': case 'p':
				fputs("Report future assertions without stopping\n\n", stdout);
				m_stop = false;
				break;

			default:
				fputs("Continuing ...\n\n", stdout);
				break;
			}
	}
#endif // POSIX_INTERACTIVE_ASSERT
	return true;
}

extern "C"
void Debug_OpAssert(const char* expression, const char* file, int line)
{
	const char format[] = "Failed OP_ASSERT(%s) at %s:%d";
	PosixLogger vox(PosixLogger::ASSERT);
	if (!vox.Log(PosixLogger::NORMAL, format, expression, file, line))
	{
		PosixAssertLogger voice;
		voice.Log(PosixLogger::NORMAL, format, expression, file, line);
	}
}
# endif // DEBUG_ENABLE_OPASSERT
#endif // POSIX_OK_DEBUG

#endif // POSIX_OK_LOGGER
