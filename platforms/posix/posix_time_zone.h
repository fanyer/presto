/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_TIME_ZONE_H
#define POSIX_TIME_ZONE_H __FILE__
# ifdef POSIX_OK_TIME_ZONE

/** Class to track time-zone and daylight-savings (DST) adjustments.
 *
 * Caches information about DST in a (hopefully many months-wide) interval about
 * the present, as long as a relevant clock's results are passed through
 * Update() reasonably frequently.  When the input to Update() is outside the
 * interval in which the DST cache is valid, it triggers recomputation of
 * timezone - which, in turn, computes the new DST offset and its interval of
 * validity.  In principle, if a single Opera session spans several DST
 * transitions, this should ensure correct results throughout.
 *
 * Moments in time are specified (as doubles) by the number of milli-seconds
 * by which the given moment was after "the epoch", 1970/Jan/1 00:00 UTC.
 */
class PosixTimeZone
{
	enum {
		/** Largest possible time-zone offset magnitude, in hours.
		 *
		 * The international date line is twelve hours each side of UTC, with
		 * some dents reaching across the true anti-Greenwich meridian, which
		 * might give a time-zone up to 13 hours from UTC; which might have DST
		 * to push one more hour.  But if we ever compute a time-zone offset
		 * greater than 14 hours, something has gone wildly wrong !  If the
		 * assertions using this trigger and you have TWEAK_POSIX_USE_GMTOFF
		 * enabled (as it is by default), your platform's tm_gmtoff support may
		 * be broken; try turning the tweak off; you may also need to mistrust
		 * such a system's timezone variable.  See CORE-24586, MNDRK5-8.
		 */
		MAX_CREDIBLE_TZ_HOURS = 14
	};
	static inline bool CredibleTZ(int tz)
		{ return op_abs(tz) / 3600 <= MAX_CREDIBLE_TZ_HOURS; }

	double m_last_dst; ///< Last change to DST
	double m_next_dst; ///< Next change to DST
	bool m_is_dst; ///< Does DST apply between last and next ?
	int m_timezone; ///< GetTimeZone() cache: seconds west of UTC, DST-adjusted.
#if POSIX_TZ_VARY_INTERVAL > 0
	double m_next_tz; ///< Next check for change in base TZ

	/** Update m_*_isdst, return value for m_timezone.
	 *
	 * Computes and caches the current timezone and, when dst is true, updates
	 * information relating to DST.  Note that timezone update is normally,
	 * after the first call, fatuous unless TWEAK_POSIX_TIMEZONE_VARIES (q.v.)
	 * is enabled.
	 *
	 * @param dst True if we also need to update DST info; false to only update
	 * timezone.
	 */
	int ComputeTimeZone(bool dst=true);
#else
	int ComputeTimeZone(); ///< Update m_*_isdst, return value for m_timezone.
#endif

	bool ComputeDST(double when); ///< Determines whether DST applies to time when/ms.

	/** Test whether when/ms is in the interval between last and next */
	bool Spans(double when) { return m_last_dst <= when && when <= m_next_dst; }
public:
	/** Constructor: auto-initializes members suitably. */
	PosixTimeZone() // m_*_dst are initialized as a side-effect of:
		: m_timezone(ComputeTimeZone())
#if POSIX_TZ_VARY_INTERVAL > 0
		  /* This is a rather poor value for next time-zone update; it'll
		   * usually always force the first Update() to ComputeTimeZone(false)
		   * mainly so as to set m_next_tz to a sensible value; but m_last_dst
		   * is the only time we can currently access in our past. */
		, m_next_tz(m_last_dst)
#endif
		{ OP_ASSERT(CredibleTZ(m_timezone)); }

	/** Currently cached time-zone offset.
	 *
	 * The return from this function is only \em reliably valid immediately
	 * following a call to Update(); as long as this happens sufficiently often,
	 * the result shall only ever be out-of-date for a very brief period of
	 * time.
	 *
	 * @return Time-zone offset, seconds west of UTC, DST-adjusted.
	 */
	int GetTimeZone() { return m_timezone; }

	/** Value of DST at a selected time.
	 *
	 * @param when A UTC time/ms since epoch.
	 * @return True precisely if daylight-savings time { was | is | will be } in
	 * effect at the specified time.
	 */
	bool IsDST(double when) { return Spans(when) ? m_is_dst : ComputeDST(when); }

	/** Notice an update to the current time.
	 *
	 * If the UTC time represented by now (in ms since the epoch) lies outside
	 * the interval for which our cached value is valid, cached data is updated.
	 * The rest of the time, it is cheap to call this function.  It returns the
	 * value it is given, for ease of calling by OpSystemInfo::GetTimeUTC()
	 * implementations - where they would otherwise return expr; they can
	 * instead return m_zone.Update(expr); if they have an instance of this
	 * class as member m_zone.
	 *
	 * @param now Current UTC time in ms since the epoch.
	 * @return The value of now.
	 */
	double Update(const double now)
	{
#ifdef THREAD_SUPPORT
		if (!g_opera || // CORE-42186: opera is not initialized the first time this is called.
            g_opera->posix_module.OnCoreThread())
#endif // it would be a bad idea to update globals from two threads at once ...
		{
			if (!Spans(now))
				m_timezone = ComputeTimeZone(); // Updates m_*_dst
#if POSIX_TZ_VARY_INTERVAL > 0
			else if (now > m_next_tz)
				m_timezone = ComputeTimeZone(false);
#endif
			else
				return now;

#if POSIX_TZ_VARY_INTERVAL > 0
			m_next_tz = now + POSIX_TZ_VARY_INTERVAL * 6e4;
#endif // One minute is 6e4 milliseconds
			OP_ASSERT(CredibleTZ(m_timezone));
		}
		return now;
	}
};

# endif // POSIX_OK_TIME_ZONE
#endif // POSIX_TIME_ZONE_H
