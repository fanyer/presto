/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_LOGGER_H
#define POSIX_LOGGER_H __FILE__
#include "platforms/posix/posix_native_util.h"
# ifdef POSIX_OK_LOGGER
#  ifdef POSIX_OK_LOG
#include "modules/pi/OpSystemInfo.h"
#  endif
#include "platforms/posix/posix_thread_util.h"

/** Convenience base class for things that might want to do optional logging.
 *
 * This base class is defined even when API_POSIX_LOG isn't imported, so that
 * derived classes within this module can still inherit from it and call Log()
 * without preprocessor conditionals; this should incur no cost, since the
 * implementations are, in this case, inline and empty (the same is not true of
 * PError, for the present).  Arguments to Log() are still evaluated, so
 * \#if-ery is appropriate when their evaluation is not desired as part of
 * un-logged execution.
 *
 * When the API is imported, the selected LoggerType determins which of
 * g_posix_log_boss's listeners gets to (if set) handle log messages.  A second
 * constructor is provided in this case, so that classes outside this module
 * have the option of using the same infrastructure, without involving
 * g_posix_log_boss.
 *
 * This class is not intended for use with translated strings: it is a raw
 * logging mechanism, tacitly using "international" English as its language, to
 * assist users in collecting debug information worth attaching to bug reports.
 * Its only support for Unicode is to emit a simple string, e.g. so as to
 * include a URL in the logging stream.
 */
class PosixLogger
{
public:
	/** Logging category.
	 *
	 * Members of this enumeration correspond to g_posix_log_boss->Get...()
	 * methods.  Each identifies a category of activity within this module,
	 * whose logging may be distinct from the others.
	 */
	// TODO: come up with some more scalable way to handle possibly many categories.
	enum LoggerType {
		ASSERT,		//*< Debug_OpAssert
		ASYNC,		//*< PosixAsyncManager
		LOGDBG,		//*< dbg_logfileoutput
		STREAM,		//*< OpLowLevelFile (and OpDLL)
		// (calling it FILE would mess up OpLowLevelFile's use of a <stdio.h> type !)
		SYSDBG,		//*< dbg_systemoutput
		// Not in use on open_0 (but nothing stops unix-net using them ;-)
		DNS,		//*< OpHostResolver
		SOCKET,		//*< OpSocket
		SOCKADDR,	//*< OpSocketAddress (currently borrows from DNS or SOCKET)
		GENERIC		//*< None of the above, does not have an associated listener
	} m_type;

	/** Control over which messages to actually deliver.
	 *
	 * Each message is reported with a specified loquacity level.  If the
	 * listener to which it is passed is reporting at a level at least as high,
	 * the message is delivered and the Log() method returns true, else false.
	 * Thus a terser message can be emitted only if a more verbose message is
	 * not emitted by simply chaining Log() calls with || between them.
	 */
	enum Loquacity {
		QUIET,		//*< almost silent (unless something realy serious goes wrong)
		TERSE,		//*< not saying much (mostly what went wrong)
		NORMAL,		//*< says a fair bit, but tries not to drown reader
		CHATTY,		//*< says rather a lot, you may wish you could filter it
		VERBOSE,	//*< tells you everything relevant in fussy detail
		SPEW		//*< drowns you in details
	};

#ifdef POSIX_OK_LOG
private:
	inline static class PosixLogListener *TypeToListener(LoggerType type); // inline below

	// Inlined below:
	static bool vaIfy(class PosixLogListener *, Loquacity, const char *, ...);
	static bool Preamble(class PosixLogListener *ear, Loquacity level)
		{ return !g_op_time_info || vaIfy(ear, level, "[%u] ", g_op_time_info->GetRuntimeTickMS()); }

public:
	/** Fatuous destructor.
	 * Provided so that it can be overtly virtual, since derived classes need that.
	 */
	virtual ~PosixLogger() {}

	/** Constructor for this module's classes that may wish to log data.
	 *
	 * @param type Indicates which of g_posix_log_boss's listeners to use, if available.
	 */
	PosixLogger(LoggerType type) : m_type(type) {}
#else
public:
	PosixLogger(LoggerType) {}
#endif // POSIX_OK_LOG

	/** Offer a message up for logging.
	 *
	 * @param level How loquacious a listener needs to be to care about this message.
	 * @param format Format string, as for printf and friends; followed by
	 * suitable arguments to match the formatters.
	 * @return True if message actually emitted.
	 */
	bool Log(Loquacity level, const char *format, ...) const; // inline below

	/** Convenience method to log a Unicode string.
	 *
	 * Depends on listener support for the Unicode version of Report (q.v.).
	 *
	 * @param level How loquacious a listener needs to be to care about this message.
	 * @param text The Unicode string to output (no formatting supported).
	 * @return True if message actually emitted.
	 */
	bool Log(Loquacity level, const uni_char *text) const; // inline below

	/** Offer a message up for logging.
	 *
	 * Static versions of the eponymous instance methods, for use when you don't
	 * have an instance handy (e.g. in static methods).  Each takes an initial
	 * LoggerType enum to select which listener from g_posix_log_boss to use;
	 * remaining parameters are the same.
	 */
	static bool Log(LoggerType, Loquacity, const char *, ...); // inline below
	/**
	 * @overload
	 */
	static bool Log(LoggerType, Loquacity, const uni_char *); // inline below

	/** Emit an error message
	 *
	 * Substitute for POSIX perror, falling back on perror unless
	 * API_POSIX_LOCALE is imported.  Logs to the error stream (presently
	 * stderr) rather than the usual logging stream for this object.  This may
	 * change (and this method may then cease being static).
	 *
	 * @param sysfunc Name of a system function implicated in the error.
	 * @param err The value of errno after the failing system function.
	 * @param message LocaleString indicating context of the error.
	 * @param fallback Plain en/ASCII string describing the error.
	 * @return True if message actually emitted (which it always is, for now).
	 */
	static bool PError(const char *sysfunc, int err,
					   Str::LocaleString message, const char *fallback);
	// Implemented in src/posix_debug.cpp to avoid deprecations warnings.
	// FIXME: see class comment; the LocaleString is inappropriate in this class !
	// FIXME: should log via self's logger, not via stdout/stderr !
	/* Removing the LocaleString will also remove most of this modules' need for
	 * module.strings; simply use the "fallback" message, along with sysfunc and
	 * strerror(); it will then be possible to output via self's logger, too.
	 */
};

# ifdef POSIX_OK_LOG
// #include <stdarg.h>

/** Listener class for logging.
 *
 * Intended for logging to terminal or system-encoded file; generally for debug
 * output, but this may include the kinds of "debug" a release build may support
 * to enable users to collect information we can use in debugging Opera's
 * interaction with the system.
 *
 * Each Report() method corresponds to a Log() method of PosixLogger.  Each
 * returns true if it did emit the string; it would be perfectly reasonable to
 * emit true also if it wouldn't emit the string at any loquacity level, since
 * the purpose of the return is simply to let the caller know whether to bother
 * trying to emit a less loquacious message instead.
 */
class PosixLogListener
{
public:
	virtual ~PosixLogListener() {}

	/** Handle logging message.
	 *
	 * @param level Emit the message if you're at least this loquacious.
	 * @param format Format string in the style of printf() and friends.
	 * @param args Variable argument list as for vprintf() and its kin.
	 * @return True if you did emit this message; else false.
	 */
	virtual bool Report(PosixLogger::Loquacity level,
						const char *format, va_list args) = 0;

	/* Log a Unicode text.
	 *
	 * This method is optional; logging of Unicode is not always practical.
	 * Implementations may use the API_POSIX_NATIVE infrastructure to convert
	 * the string; or see PosixDebugListener::UTF8Report() if assuming UTF8 as
	 * system locale is reasonable.
	 *
	 * @param level Emit the message if you're at least this loquacious.
	 * @param text The Unicode string to output (no formatting supported).
	 * @return True if you did emit the message; else false.
	 */
	virtual bool Report(PosixLogger::Loquacity level, const uni_char *text)
		{ return false; }
};

#  ifdef POSIX_OK_LOG_STDIO
// #include <stdio.h>
/** The obvious implementation of PosixLogListener
 *
 * Logs a standard I/O stream, i.e. a FILE *, such as stdout or stderr.
 */
class PosixLogToStdIO : public PosixLogListener
{
	FILE *const m_out;
	PosixLogger::Loquacity const m_level;
	bool m_toclose;
public:
	/** Constructor for stdout or stderr.
	 *
	 * @param level Optional (default: NORMAL) loquacity threshold level.
	 * @param error Optional (default: false) flag; if true, use stderr, else
	 * use stdout.
	 */
	PosixLogToStdIO(PosixLogger::Loquacity level=PosixLogger::NORMAL, bool error=false)
		: m_out(error ? stderr : stdout), m_level(level), m_toclose(false) {}

	/** Constructor for custom FILE handle.
	 *
	 * @param stream FILE pointer to which to log output.  Must be open for
	 * writing and remain so throughout the lifetime of this logging listener.
	 * @param level Optional (default: NORMAL) loquacity threshold level.
	 * @param toclose Optional flag: if true (default), the new listener is
	 * responsible for calling fclose(stream) when done with it.
	 */
	PosixLogToStdIO(FILE *stream, PosixLogger::Loquacity level=PosixLogger::NORMAL,
					bool toclose=true)
		: m_out(stream), m_level(level), m_toclose(toclose) {}

	/** Destructor closes stream if suitable.
	 */
	virtual ~PosixLogToStdIO() { if (m_toclose) fclose(m_out); }

	/** Emit a Unicode string as UTF8 on a file handle.
	 *
	 * This only uses putc, with a simplistic UTF8 converter, so isn't
	 * guaranteed to be perfect, but it is suitable for simple debug tasks: in
	 * particular, it doesn't involve heap traffic, let alone the encodings
	 * module, so is suitable for debug purposes.
	 */
	static void UTF8Report(const uni_char *text, FILE *fd); // see src/posix_debug.cpp

	virtual bool Report(PosixLogger::Loquacity level, const char *format, va_list args)
	{
		if ((int)level > (int)m_level)
			return false;

		vfprintf(m_out, format, args);
		return true;
	}

	virtual bool Report(PosixLogger::Loquacity level, const uni_char *text)
	{
		if ((int)level > (int)m_level)
			return false;

		UTF8Report(text, m_out);
		return true;
	}
};

#   if defined(POSIX_OK_DEBUG) && defined(DEBUG_ENABLE_OPASSERT)
class PosixAssertLogger : public PosixLogToStdIO
{
#ifdef POSIX_INTERACTIVE_ASSERT
	bool m_stop;
#endif
	friend void Debug_OpAssert(const char* expression, const char* file, int line);
	void Log(PosixLogger::Loquacity level, const char *format, ...);
public:
	PosixAssertLogger(PosixLogger::Loquacity level=PosixLogger::NORMAL, bool error=true)
		: PosixLogToStdIO(level, error)
#ifdef POSIX_INTERACTIVE_ASSERT
		, m_stop(!PosixNativeUtil::AffirmativeEnvVar("OPASSERT_CONTINUE"))
#endif
		{}
	PosixAssertLogger(FILE *stream, PosixLogger::Loquacity level=PosixLogger::NORMAL)
		: PosixLogToStdIO(stream, level)
#ifdef POSIX_INTERACTIVE_ASSERT
		, m_stop(!PosixNativeUtil::AffirmativeEnvVar("OPASSERT_CONTINUE"))
#endif
		{}
	virtual bool Report(PosixLogger::Loquacity level, const char *format, va_list args);
};
#   endif // Support for OpAssert: see src/posix_debug.cpp
#  endif // POSIX_OK_LOG_STDIO

#  ifdef POSIX_GLOBAL_ASSERT_EAR
extern PosixLogListener *posix_assert_ear;
#  endif
/** Manager object for the listeners used by this module's logging.
 *
 * Use g_posix_log_boss to specify the listeners you want (if any) to have take
 * responsibility for this module's internal logging messages.
 *
 * Note that each Set method returns the old listener (if any) it displaced.
 * Callers should verify they get back what they expected, if setting to NULL to
 * clear a listener they put here (which, for example, the listener should do in
 * its destructor).  When providing a listener, you get the option of forwarding
 * messages to the one you displaced (optionally only those beyond your
 * loquacity level, for example), but be wary of remembering it to put back when
 * you retire - if it gets deleted in the mean time, neither you nor it shall
 * know !  Normally, listeners should be set up during Opera::InitL() and
 * cleared away during Opera::Destroy(), so such complications should not arise.
 */
class PosixLogManager
{
	// One listener for each PosixLogger::LoggerType value.
#ifdef POSIX_GLOBAL_ASSERT_EAR
#define m_assert posix_assert_ear // Use a true global instead of a member.
#else
	PosixLogListener *m_assert;
#endif
	PosixLogListener *m_async;
	PosixLogListener *m_log;
	PosixLogListener *m_stream;
	PosixLogListener *m_debug;
	PosixLogListener *m_dns;
	PosixLogListener *m_socket;
	// For now, handle sockaddr as dns, if available, else socket.
public:
	PosixLogManager()
		:
#ifndef POSIX_GLOBAL_ASSERT_EAR
		m_assert(0),
#endif
		m_async(0), m_log(0), m_stream(0), m_debug(0), m_dns(0), m_socket(0)
		{}

#ifdef POSIX_GLOBAL_ASSERT_EAR
	static PosixLogListener *GetAssert() { return m_assert; }
	static PosixLogListener *SetAssert(PosixLogListener *ear) {
		PosixLogListener * old = m_assert;
		m_assert = ear;
		return old;
	}
#endif

	static PosixLogManager* GetInstance()
	{ return g_opera ? g_opera->posix_module.GetLoggingManager() : 0; }
#ifdef POSIX_OK_THREAD
private:
	PosixMutex m_log_mutex;
public:
	/** Returns a pointer to the mutex of the PosixLogManager instance.
	 *
	 * This method may only be called when there is a PosixLogManager instance,
	 * i.e. when GetInstance() != 0.
	 */
	static PosixMutex* GetMutex()
	{
		OP_ASSERT(GetInstance());
		return &(GetInstance()->m_log_mutex);
	}
#endif // POSIX_OK_THREAD

	PosixLogListener *Get(PosixLogger::LoggerType type)
	{
		switch (type)
		{
		case PosixLogger::ASSERT:	return m_assert;
		case PosixLogger::ASYNC:	return m_async;
		case PosixLogger::LOGDBG:	return m_log;
		case PosixLogger::STREAM:	return m_stream;
		case PosixLogger::SYSDBG:	return m_debug;
		case PosixLogger::DNS:		return m_dns;
		case PosixLogger::SOCKET:	return m_socket;
		case PosixLogger::SOCKADDR:	return m_dns ? m_dns : m_socket;
		}
		return 0;
	}

	PosixLogListener *Set(PosixLogger::LoggerType type, PosixLogListener *ear)
	{
		PosixLogListener * old = 0;
		switch (type)
		{
		case PosixLogger::ASSERT:	old = m_assert;	m_assert = ear;	break;
		case PosixLogger::ASYNC:	old = m_async;	m_async = ear;	break;
		case PosixLogger::LOGDBG:	old = m_log;	m_log = ear;	break;
		case PosixLogger::STREAM:	old = m_stream;	m_stream = ear;	break;
		case PosixLogger::SYSDBG:	old = m_debug;	m_debug = ear;	break;
		case PosixLogger::DNS:		old = m_dns;	m_dns = ear;	break;
		case PosixLogger::SOCKET:	old = m_socket;	m_socket = ear;	break;
		// case PosixLogger::SOCKADDR: ???
		}
		return old;
	}
#ifdef POSIX_GLOBAL_ASSERT_EAR
#undef m_assert
#endif
};
#define g_posix_log_boss PosixLogManager::GetInstance()

/* Inline methods of PosixLogger that depend (when relevant) on PosixLogManager
 * and PosixLogListener:
 */
inline PosixLogListener *PosixLogger::TypeToListener(LoggerType type)
{
	return g_opera && g_posix_log_boss ? g_posix_log_boss->Get(type) : 0;
}

#   ifndef POSIX_OK_NATIVE
#include "modules/util/opstring.h"
#include "modules/util/excepts.h"
#   endif // POSIX_OK_NATIVE

inline bool PosixLogger::vaIfy(class PosixLogListener *ear, Loquacity level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	const bool spoke = ear->Report(level, format, args);
	va_end(args);
	return spoke;
}

#  endif // POSIX_OK_LOG

inline bool PosixLogger::Log(Loquacity level, const char *format, ...) const
{
#ifdef POSIX_OK_LOG
	if (PosixLogListener * POSIX_THREAD_VOLATILE const ear = TypeToListener(m_type))
	{
		bool spoke;
		{
			POSIX_MUTEX_LOCK(PosixLogManager::GetMutex());
			va_list args;
			va_start(args, format);
			spoke = Preamble(ear, level) && ear->Report(level, format, args);
			va_end(args);
			POSIX_MUTEX_UNLOCK(PosixLogManager::GetMutex());
		}
		return spoke;
	}
#endif // POSIX_OK_LOG
	return false;
}

inline bool PosixLogger::Log(Loquacity level, const uni_char *text) const
{ return Log(m_type, level, text); }

// static
inline bool PosixLogger::Log(LoggerType type, Loquacity level, const char *format, ...)
{
#ifdef POSIX_OK_LOG
	if (PosixLogListener * POSIX_THREAD_VOLATILE const ear = TypeToListener(type))
	{
		bool spoke;
		{
			POSIX_MUTEX_LOCK(PosixLogManager::GetMutex());
			va_list args;
			va_start(args, format);
			spoke = Preamble(ear, level) && ear->Report(level, format, args);
			va_end(args);
			POSIX_MUTEX_UNLOCK(PosixLogManager::GetMutex());
		}
		return spoke;
	}
#endif // POSIX_OK_LOG
	return false;
}

// static
inline bool PosixLogger::Log(LoggerType type, Loquacity level, const uni_char *text)
{
#ifdef POSIX_OK_LOG
	if (PosixLogListener * POSIX_THREAD_VOLATILE const ear = TypeToListener(type))
	{
		bool spoke;
		{
			POSIX_MUTEX_LOCK(PosixLogManager::GetMutex());
			spoke = Preamble(ear, level) && ear->Report(level, text);
			POSIX_MUTEX_UNLOCK(PosixLogManager::GetMutex());
		}
		return spoke;
	}
#endif
	return false;
}

# endif // POSIX_OK_LOGGER
#endif // POSIX_LOGGER_H
