/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef POSIX_OK_CLOCK
#include "platforms/posix/posix_native_util.h"

#undef MONOTONIC_TIMER // To simplify #if-ery later in file
#ifdef POSIX_USE_TIMERS
/** Use *_FAST variants on timers where available.
 *
 * The function-calls to access these are much faster, at the expense of a loss
 * of precision (not resolution) of order "10 ms" (not clear if that's micro- or
 * milli-).  See MANTLE-911 and:
 * http://fxr.watson.org/fxr/source/sys/time.h#L269
 */
# ifdef CLOCK_MONOTONIC_FAST
#  define MONOTONIC_TIMER CLOCK_MONOTONIC_FAST
# elif defined(CLOCK_MONOTONIC)
#  define MONOTONIC_TIMER CLOCK_MONOTONIC
# endif
# ifdef CLOCK_REALTIME_FAST
#  define REALTIME_TIMER CLOCK_REALTIME_FAST
# elif defined(CLOCK_REALTIME)
#  define REALTIME_TIMER CLOCK_REALTIME
# endif
#endif // POSIX_USE_TIMERS

#ifndef MONOTONIC_TIMER
#include <float.h> // For DBL_EPSILON, potentially used in POSIX_CREDIBLE_TIME_QUANTUM.
#endif


#include "modules/pi/OpTimeInfo.h"

#  ifdef POSIX_USE_TIMERS
#include <time.h> // defines CLOCK_MONOTONIC, if available.
#  else // POSIX_USE_TIMERS
#include <sys/time.h>
#  endif // POSIX_USE_TIMERS
# ifdef POSIX_OK_TIME_ZONE
#include "platforms/posix/posix_time_zone.h"
# endif // POSIX_OK_TIME_ZONE

class PosixTimeInfo : public OpTimeInfo
{
public:
	PosixTimeInfo();
private:
	const double m_resolution; ///< Cached GetWallClockResolution()
	static double ComputeResolution(); ///< Compute m_resolution.

#if defined(POSIX_USE_TIMERS) && defined(CLOCK_MONOTONIC)
	const double m_started; ///< Reference value of RunTimeMSNow().
#else
	const double m_quantum; ///< Cached value of POSIX_CREDIBLE_TIME_QUANTUM.
	double m_last_ms; ///< Last GetTimeUTC seen by RunTimeMSNow
	double m_started; ///< Reference value of RunTimeMSNow().
#endif
	unsigned int m_last_tick;

	/** Implementation of GetRuntimeMS.
	 *
	 * Called by GetRuntimeMS and used by constructor to initialize m_started.
	 * When CLOCK_MONOTONIC is available, it's used.  Otherwise, GetTimeUTC is
	 * used adjusted to make it monotonic; see TWEAK_POSIX_TIME_NOJUMP and
	 * TWEAK_POSIX_TIME_QUANTUM for details of how it's adjusted.
	 *
	 * @return A suitable time value, with a possibly unsuitable offset.
	 */
	double RunTimeMSNow();
	/** Cacheing the rounded return from GetRuntimeMS().
	 *
	 * Called by each (non-failing) return from RunTimeMSNow(), as a simple
	 * pass-through of the value returned, to let us cache its rounded value for
	 * use by GetRuntimeTickMS (q.v.).
	 */
	double UpdateTick(double val)
	{
		OP_ASSERT(0 <= val);
		/* By tracking the value passing through GetRuntimeMS, we ensure val is
		 * positive - it counts ms since start-up, so we get up to 49 days
		 * before it'll overflow UINT_MAX. */
#ifdef HAVE_UINT64
		/* Assume 64-bit int is big enough to hold val (ms since start-up; takes
		 * more than a quarter giga-year to reach 1<<63) without overflow, hence
		 * simply by truncation.  The subsequent integral cast will duly wrap as
		 * needed. */
		m_last_tick = static_cast<unsigned int>(static_cast<UINT64>(val));
#else // HAVE_UINT64
		/* Casting double to int, if out of range, may clip to the range of the
		 * integral type in question - ANSI X3.159-1989 section 3.2.1.3 says the
		 * behaviour is undefined.  So use fmod to reduce it suitably into range
		 * (after the first fifty days). */
		m_last_tick = static_cast<unsigned int>(op_fmod(val, UINT_MAX));
#endif // HAVE_UINT64
		return val;
	}
public:
	virtual void   GetWallClock( unsigned long& seconds, unsigned long& milliseconds );
	// NB double GetWallClockMS(void) packages that nicely ;-)
	virtual double GetWallClockResolution() { return m_resolution; }

	virtual double GetTimeUTC(); // ms since 1970-01-01 00:00 GMT
	// Monotonically increasing, not fussy about origin, but measured in ms:
	virtual double GetRuntimeMS() { return UpdateTick(RunTimeMSNow() - m_started); }

	/* Note: this implementation relies on GetRuntimeMS() being called at least
	 * once per millisecond, since it simply caches the rounded number of whole
	 * milliseconds returned by the last such call.  If we ever replace enough
	 * of our calls to GetRuntimeMS() with calls to this, we may need to come up
	 * with a better strategy !  However, this approach is very cheap ;-)
	 *
	 * Note that Linux's clock_gettime() supports a CLOCK_MONOTONIC_COARSE that
	 * we could possibly use, as an alternative.
	 */
	virtual unsigned int GetRuntimeTickMS() { return m_last_tick; }

#ifdef POSIX_OK_TIME_ZONE
	PosixTimeZone m_zone;
	virtual long GetTimezone() { return m_zone.GetTimeZone(); }
	virtual double DaylightSavingsTimeAdjustmentMS(double t) { return m_zone.IsDST(t) ? 36e5 : 0; }
#endif // POSIX_OK_TIME_ZONE
};

PosixTimeInfo::PosixTimeInfo()
	: m_resolution(ComputeResolution())
# ifdef MONOTONIC_TIMER
	, m_started(RunTimeMSNow() - 1000)
# else
	, m_quantum(POSIX_CREDIBLE_TIME_QUANTUM) // may be a non-trivial expression ...
	, m_last_ms(GetTimeUTC()) // can't call RunTimeMSNow until this has been initialized !
	, m_started(m_last_ms - 1000) // nominal start time 1s earlier
# endif // MONOTONIC_TIMER
	, m_last_tick(0)
{
#ifdef POSIX_CREDIBLE_TIME_QUANTUM
	// Assumed in RunTimeMSNow():
	OP_ASSERT(POSIX_CREDIBLE_TIME_QUANTUM > 0);
#endif // POSIX_CREDIBLE_TIME_QUANTUM
}

OP_STATUS
OpTimeInfo::Create(OpTimeInfo** new_timeinfo)
{
	PosixTimeInfo* linux_timeinfo = OP_NEW(PosixTimeInfo, ());
	if(linux_timeinfo)
	{
		*new_timeinfo = linux_timeinfo;
		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(linux_timeinfo);
		return OpStatus::ERR_NO_MEMORY;
	}
}

# ifndef POSIX_USE_TIMERS
inline
# endif
double PosixTimeInfo::ComputeResolution()
{
	/* LukaszB on CORE-24586: "According to POSIX.1-2004, localtime() is
	 * required to behave as though tzset() was called, while localtime_r() does
	 * not have this requirement.  For portable code tzset() should be called
	 * before localtime_r()."  Since this method is called before anything else,
	 * and is only ever called on construction, now is a good time to do that.
	 */
	tzset();

#ifdef POSIX_USE_TIMERS
	struct timespec res;
	if (clock_getres(REALTIME_TIMER, &res) == 0)
		return res.tv_sec + 1e-9 * res.tv_nsec;

# if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
	PosixNativeUtil::Perror(Str::S_ERR_REAL_CLOCK, "clock_getres", errno);
# else
	perror("Real [clock_getres]");
# endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS
	// Fall back on grungy guess:
#endif // POSIX_USE_TIMERS
	return POSIX_TIMER_RESOLUTION; // see TWEAK_POSIX_TIMER_RESOLUTION
}

void PosixTimeInfo::GetWallClock(unsigned long& seconds, unsigned long& milliseconds)
{
#ifdef POSIX_USE_TIMERS
	struct timespec when;
	if (clock_gettime(REALTIME_TIMER, &when) == 0)
	{
		seconds = when.tv_sec - m_zone.GetTimeZone();
		milliseconds = when.tv_nsec / 1000000; // million nano-seconds per milli-second.
		return;
	}
# if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
	PosixNativeUtil::Perror(Str::S_ERR_REAL_CLOCK, "clock_gettime", errno);
# else
	perror("Real [clock_gettime]");
# endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS
#else
	struct timeval t;
	if (gettimeofday(&t, NULL) == 0)
	{
		seconds = t.tv_sec - m_zone.GetTimeZone();
		milliseconds = t.tv_usec / 1000;
		return;
	}
	// TODO: report error
#endif
	seconds = milliseconds = 0; // ouch
}
// NB double GetWallClockMS(void) packages that nicely ;-)

/** Current time, in milliseconds since 1970-01-01 00:00 GMT. */
double PosixTimeInfo::GetTimeUTC()
{
	// NB: CLOCK imports TIME_ZONE, so m_zone *is* available :-)
#ifdef POSIX_USE_TIMERS
	struct timespec when;
	if (clock_gettime(REALTIME_TIMER, &when) == 0)
		return m_zone.Update(when.tv_sec * 1e3 + 1e-6 * when.tv_nsec);

# if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
	PosixNativeUtil::Perror(Str::S_ERR_UTC_CLOCK, "clock_gettime", errno);
# else
	perror("UTC [clock_gettime]");
# endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS
#else
	struct timeval t;
	if (gettimeofday(&t, NULL) == 0)
		return m_zone.Update(t.tv_sec * 1e3 + t.tv_usec * 1e-3);

	// TODO: report error
#endif // POSIX_USE_TIMERS
	return -1.0;
}

// private
double PosixTimeInfo::RunTimeMSNow()
{
	/* Use a true monotonic timer if available; else GetTimeUTC() is suitable,
	 * subject to bodges in case system clock gets frobbed (e.g. by NTP). */
#ifdef MONOTONIC_TIMER
	struct timespec now;
	if (clock_gettime(MONOTONIC_TIMER, &now) == 0)
		return now.tv_sec * 1e3 + now.tv_nsec * 1e-6;

# if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
	PosixNativeUtil::Perror(Str::S_ERR_RUN_CLOCK, "clock_gettime", errno);
# else
	perror("Run [clock_gettime]");
# endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS
#else // !MONOTONIC_TIMER
	double now = GetTimeUTC();
	if (now > 0)
	{
		/* Check whether time has gone backwards; correct if so: */
		if (m_last_ms > now + m_quantum
#if 0 < POSIX_IMPLAUSIBLE_ELAPSED_TIME // measured in minutes
			/* ... and likewise for implausibly big leaps forward in time: */
			|| m_last_ms + POSIX_IMPLAUSIBLE_ELAPSED_TIME * 6e4 < now
#endif
			)
		{
			double error = now - m_last_ms;
			double rest = error < 0 ? op_fmod(-error, -3.6e3) : op_fmod(error, 3.6e3);
			OP_ASSERT(rest >= 0 && rest < 3.6e3); // else broken fmod
			/* If error is just a whole number of hours (backwards or forwards)
			 * plus a small step forward, rest, then this step is probably
			 * actually the true change in time, so use it; otherwise, fall back
			 * on our arbitrary compile-time guess (also used in defining how
			 * big "small" is). */
			if (rest <= 0 || rest > 0x10000 * m_quantum)
				rest = m_quantum;
			m_started += error - rest;
		}

		m_last_ms = now;
		return now;
	}
	// TODO: report error
#endif // MONOTONIC_TIMER
	return -1.0;
}
#endif // POSIX_OK_CLOCK
