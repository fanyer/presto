/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#undef mktime // suppress lingogi's dont_use_mktime - we need it !
#ifdef POSIX_OK_TIME_ZONE
#include "platforms/posix/posix_time_zone.h"
# ifdef POSIX_OK_NATIVE
#include "platforms/posix/posix_native_util.h"
# endif
// For testing: see (lib)faketime ...

bool PosixTimeZone::ComputeDST(double t)
{
	time_t sec_time = (time_t)(t / 1000);
	struct tm when;
	if (localtime_r(&sec_time, &when) == &when)
		return when.tm_isdst > 0;

#if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
	PosixNativeUtil::Perror(Str::S_ERR_DAYLIGHT_SAVING, "localtime_r", errno);
#else
	perror("DST [localtime_r]");
#endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS
	return 0;
}

/** Find the DST change between now and then, to within tol seconds.
 *
 * Uses binary chop on the interval between now and then, so number of calls to
 * localtime_r and mktime grows as the log of the interval divided by tol.
 * However, the algorithm also tweaks towards selecting the last and first
 * seconds of an hour, or minute, if such a second falls in the interval to be
 * bisected; this may slow the convergence, but should tend to give more
 * accurate answers.
 *
 * @param now Describes the time (roughly) now
 * @param then Describes a later or earlier time with distinct .tm_isdst
 * @param tol Accuracy level sought for DST change-over betweeen now and then
 * @return A time, in milliseconds since the epoch, within tol seconds of a
 * moment between now and then when DST changes; the indicated moment is always
 * strictly on then's side (future or past) of now, never equal to now; the
 * returned time has the same DST as now unless it is the exact moment of the
 * change (in which case the time one second closer to now has the same DST as
 * now).
 */
static double NarrowInterval(struct tm now, struct tm &then, time_t tol)
{
	OP_ASSERT(tol > 0 && now.tm_isdst != then.tm_isdst);
	/* NB (see bug 361242): stdlib's op_mktime needs to know timezone, which it
	 * can only know once we've been here; so we must use a real mktime here.
	 */
	time_t n = mktime(&now), t = mktime(&then);
	bool initial = true;
	while (initial || n > t + tol || t > n + tol)
	{
		OP_ASSERT(t != n);
		time_t m = t / 2 + n / 2 + (n % 2 && t % 2); // != (n+t)/2 due to overflow !
		if (m == n || m == t) // We've found the *exact* second before a change !
		{
			OP_ASSERT(t > n ? n + 1 == t : n == t + 1);
			n = t;
			break;
		}

		// Round m up to end of minute or hour, or down to start, if practical:
		if (t > m)
		{
			if (m/60 > n/60)
			{
				if (m/3600 > n/3600)
					m -= m % 3600;
				else
					m -= m % 60;
			}
			else if (m/60 < t/60)
			{
				if (m/3600 < t/3600)
					m += 3599 - m % 3600;
				else
					m += 59 - m % 60;
			}
		}
		else if (n > m)
		{
			if (m/60 > t/60)
			{
				if (m/3600 > t/3600)
					m -= m % 3600;
				else
					m -= m % 60;
			}
			else if (m/60 < n/60)
			{
				if (m/3600 < n/3600)
					m += 3599 - m % 3600;
				else
					m += 59 - m % 60;
			}
		}

		struct tm mid;
		if (localtime_r(&m, &mid) != &mid)
		{
			// meh - do this by hand ... fortunately, mktime allows non-normal values
			time_t delta = n - m;
			op_memset(&mid, 0, sizeof(mid));
			mid.tm_isdst = -1;
			mid.tm_sec = now.tm_sec + delta % 60;
			delta /= 60;
			mid.tm_min = now.tm_min + delta % 60;
			delta /= 60;
			mid.tm_hour = now.tm_hour + delta % 24;
			mid.tm_mday = now.tm_mday + delta / 24;
			mid.tm_mon = now.tm_mon;
			mid.tm_year = now.tm_year;
			// Input tm_wday and tm_yday are ignored.
			m = mktime(&mid); // normalizes all fields
			OP_ASSERT((n < m && m < t) || (n > m && m > t));
		}

		if (mid.tm_isdst == now.tm_isdst)
		{
			now = mid;
			initial = false;
			n = mktime(&now);
		}
		else
		{
			then = mid;
			t = mktime(&then);
		}
	}
	return n * 1e3;
}

/** Find (to adequate accuracy) when DST last changed or shall next change.
 *
 * The further from now the relevant change happened or shall happen, the
 * broader the error-bar allowed on its value.
 *
 * @param when Describes the time (approximately) now.
 * @param now Number of seconds since epoch, should equal mktime(&when).
 * @param sign Indicates future if > 0 or past if < 0; should be +1 or -1.
 * @return A time, in milliseconds since the epoch, reasonably close to when DST
 * changes from when.tm_isdst, on the selected side of now; any moment strictly
 * closer to now than the returned time has the same DST as now.
 */
static double BoundDST(const struct tm &when, time_t now, int sign)
{
	const time_t day = 24 * 60 * 60; // time_t is in seconds
	time_t then = now + sign * day;
	struct tm what;
	if (localtime_r(&then, &what) == &what &&
		what.tm_isdst != when.tm_isdst)
		return NarrowInterval(when, what, sign < 0 ? 60 * 60 : 60);

	else if (then = now + 14 * sign * day, localtime_r(&then, &what) == &what &&
			 what.tm_isdst != when.tm_isdst)
		return NarrowInterval(when, what, day);

	// Assumption: DST changes happen > 4 months apart
	else if (then = now + 122 * sign * day, // year/3 away
			 localtime_r(&then, &what) == &what &&
			 what.tm_isdst != when.tm_isdst)
		return NarrowInterval(when, what, 14 * day);

	else if (then = now + 244 * sign * day, // year * 2/3 away
			 localtime_r(&then, &what) == &what &&
			 what.tm_isdst != when.tm_isdst)
		return NarrowInterval(when, what, 14 * day);

	// else, well, at least that's a long interval on this side of now :-)
	return then * 1e3;
}

/** (Re)Compute m_*_dst and the current time-zone.
 *
 * The time-zone is expressed in seconds west of Greenwich.
 * Its value is cached as m_timezone for the sake of OpSystemInfo.
 */
#if POSIX_TZ_VARY_INTERVAL > 0
int PosixTimeZone::ComputeTimeZone(bool dst /* = true */)
#else
int PosixTimeZone::ComputeTimeZone()
#endif
{
#if defined(POSIX_OK_NATIVE) && !defined(NO_CORE_COMPONENTS)
#define ReportFail(func) PosixNativeUtil::Perror(Str::S_ERR_TIME_ZONE, func, errno)
#else
#define ReportFail(func) perror("TZ [" func "]")
#endif // POSIX_OK_NATIVE && !NO_CORE_COMPONENTS

	const time_t now = op_time(NULL);
	if (now == -1)
	{
		ReportFail("time");
		return 0;
	}
#if POSIX_TZ_VARY_INTERVAL > 0
	tzset();
#endif

	struct tm when;
	if (localtime_r(&now, &when) != &when)
	{
		ReportFail("localtime_r");
		return 0;
	}
#if POSIX_TZ_VARY_INTERVAL > 0
	if (dst || m_is_dst != (bool)when.tm_isdst)
#endif
	{
		m_is_dst = when.tm_isdst;
		m_next_dst = BoundDST(when, now, +1);
		m_last_dst = BoundDST(when, now, -1);
	}
#ifdef POSIX_USE_GMTOFF
	return -when.tm_gmtoff;

#elif defined(POSIX_MISTRUST_SYSTEM_TIMEZONE)
	/* Inferring time-zone by comparing localtime and gmtime.
	 *
	 * Note that mktime and localtime are mutually inverse; strictly, mktime is
	 * a left-inverse for localtime and localtime is monic but mktime isn't, so
	 * mktime(localtime(t)) == t but localtime(mktime(w)) is not generally equal
	 * to what w was before the call (although it *is* equal to what w is
	 * *after* the call, since mktime is specified to normalise its (non-const)
	 * input).
	 *
	 * First, consider the difference between outputs of mktime and localtime at
	 * one moment during DST and one moment not during it; for astronomical
	 * reasons I here chose noon on approximately each solstice.  Since mktime
	 * ignores tm_wday and tm_yday, it suffices to describe each relevant struct
	 * tm as a display of form { Y, M, 21, H, 0, 0, I }, where M is month (12 or
	 * 6), Y is year, I is 1 for DST, else 0, and H is the hour.  (For present
	 * purposes, treat time-zones with fractional hour offsets as using
	 * non-whole values for H in some cases !)  Let the bare time-zone offset,
	 * without DST, be Z; we wish to return (Z-I)*3600.
	 *
	 * With W = month number at non-DST solstice,
	 *		gmtime(mktime({Y, W, 21, 12, 0, 0, 0}))
	 * shall be {Y, W, 21, 12+Z, 0, 0, 0}; with S as the DST solstice month,
	 *		gmtime(mktime({Y, S, 21, 12, 0, 0, 1}))
	 * shall be {Y, S, 21, 11+Z, 0, 0, 0}.  This last is equivalent, via mktime,
	 * to {Y, S, 21, 12+Z, 0, 0, 1}.  In each case, if we feed the output to
	 * mktime and subtract what mktime mapped the input to, we'll get Z*3600;
	 * but, if we set the output's I to match that of the input, before doing
	 * this, we get the desired (Z-I)*3600.
	 *
	 * Any time we are well away from a DST transition, we should get the same
	 * result as if we'd used solstice noon, so we can use when in place of the
	 * solstice-noon struct tm, with now as mktime(&when).  If we are within a
	 * day of a transition, however, we may get a wrong answer.  Given how this
	 * class caches time-zone data, however, this function is more likely than
	 * you might expect to be called exactly when this problem applies.  So take
	 * the trouble to check for this case and do the computation at a moment
	 * away from the transition.
	 */
	OP_ASSERT(now == mktime(&when)); // mktime and localtime are mutually inverse
	OP_ASSERT(m_last_dst / 1e3 < now && now < m_next_dst / 1e3);
	// If too near either boundary, move towards the other:
	const time_t probe =
		CredibleTZ(now - m_last_dst / 1e3) ? now / 2 + m_next_dst / 2e3 :
		CredibleTZ(m_next_dst / 1e3 - now) ? now / 2 + m_last_dst / 2e3 :
		now;

	// That shouldn't have changed .tm_isdst:
	OP_ASSERT(localtime_r(&probe, &when) == &when && when.tm_isdst == (m_is_dst ? 1 : 0));

    struct tm wich; // Green wich, to be specific
	if (gmtime_r(&probe, &wich) != &wich)
	{
		ReportFail("gmtime_r");
		return 0;
	}

    wich.tm_isdst = when.tm_isdst;
    return mktime(&wich) - probe; // i.e. mktime(&wich) - mktime(&when)
#else
	/* timezone (from <time.h>) is "... the difference, in seconds, between
	 * Coordinated Universal Time (UTC) and local standard time."  The spec
	 * distinguishes between "local standard time" and "daylight savings time",
	 * so I take it the former is local time without any DST, even if DST is
	 * currently in effect [Eddy/2007/March/27].
	 *
	 * Unfortunately, it's a function on FreeBSD:
	 * http://www.freebsd.org/cgi/man.cgi?query=timezone&manpath=FreeBSD+7.0-RELEASE
	 * but we can always use (non-POSIX, but handy) tm_gmtoff when available.
	 */
	return timezone - (when.tm_isdst > 0 ? 3600 : 0);
#endif
}
#endif // POSIX_OK_TIME_ZONE
