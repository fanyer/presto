/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2005
 *
 * Library for standalone builds of the ECMAScript engine.
 * NOTE!  Some parts are specific to MS Windows.
 */

#include "core/pch.h"
#include "modules/util/uniprntf.h"
#include "modules/util/md5.h"
#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#if defined(__MINGW32__)
#include <sys/time.h>
#define localtime_r( _clock, _result ) \
        ( *(_result) = *localtime( (_clock) ), \
          (_result) )

#endif 

#ifndef OP_ASSERT
void OP_ASSERT( int x )
{
	if (!x)
	{
		assert(x);
	}
}
#endif

void *operator new(size_t size, TLeave)
{
	void *p = ::operator new(size);
	if (p == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return p;
}
#if defined USE_CXX_EXCEPTIONS && defined _DEBUG
void operator delete( void *loc, TLeave ) { /* just to shut up the compiler */ }
#endif

#ifndef _MSC_VER
#include <new>

void *operator new[](size_t size, TLeave)
{
	void *p = ::operator new[](size, std::nothrow);
	if (p == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return p;
}
#endif

/** Convert Unicode encoded as UTF-8 to UTF-16 */
class JSShell_UTF8toUTF16Converter
{
public:
	JSShell_UTF8toUTF16Converter();
    int Convert(const void *src, int len, void *dest, int maxlen, int *read);

private:
	int m_charSplit;    ///< Bytes left in UTF8 character, -1 if in ASCII
	int m_sequence;     ///< Type of current UTF8 sequence (total length - 2)
	UINT32 m_ucs;       ///< Current ucs character being decoded
	UINT16 m_surrogate; ///< Surrogate left to insert in next iteration
};

JSShell_UTF8toUTF16Converter::JSShell_UTF8toUTF16Converter()
	: m_charSplit(-1), m_surrogate(0)
{
}

#ifndef NOT_A_CHARACTER
# define NOT_A_CHARACTER 0xFFFD
#endif // NOT_A_CHARACTER

static inline void
MakeSurrogate(UINT32 ucs, uni_char &high, uni_char &low)
{
	OP_ASSERT(ucs >= 0x10000UL && ucs <= 0x10FFFFUL);

	// Surrogates spread out the bits of the UCS value shifted down by 0x10000
	ucs -= 0x10000;
	high = 0xD800 | uni_char(ucs >> 10);	// 0xD800 -- 0xDBFF
	low  = 0xDC00 | uni_char(ucs & 0x03FF);	// 0xDC00 -- 0xDFFF
}

int
JSShell_UTF8toUTF16Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read_ext)
{
	register const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	const unsigned char *input_start = input;
	const unsigned char *input_end = input_start + len;

	maxlen &= ~1; // Make sure destination size is always even
	if (!maxlen)
	{
		if (read_ext) *read_ext = 0;
		return 0;
	}

	register uni_char *output = reinterpret_cast<uni_char *>(dest);
	uni_char *output_start = output;
	uni_char *output_end =
		reinterpret_cast<uni_char *>(reinterpret_cast<char *>(dest) + maxlen);

	/** Length of UTF-8 sequences for initial byte */
	static const UINT8 utf8_len[256] =
	{
		/* 00-0F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 10-1F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 20-2F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 30-3F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 40-4F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 50-5F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 60-6F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 70-7F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 80-8F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* 90-9F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* A0-AF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* B0-BF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* C0-CF */	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		/* D0-DF */	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		/* E0-EF */	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		/* F0-FF */	4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0,
	};

	/** Bit masks for first UTF-8 byte */
	static const UINT8 utf8_mask[5] =
	{
		0x1F, 0x0F, 0x07, 0x03, 0x01
	};

	/** Low boundary of UTF-8 sequences */
	static const UINT32 low_boundary[5] =
	{
		0x80, 0x800, 0x10000, 0x200000, 0x4000000
	};


	int written = 0;

	if (m_surrogate)
	{
		if (output)
		{
			*(output ++) = m_surrogate;
		}
		else
		{
			written = sizeof (uni_char);
		}
		m_surrogate = 0;
	}

	// Work on the input buffer one byte at a time, and output when
	// we have a completed Unicode character
	if (output)
	{
		// Decode the UTF-8 string; duplicates code below, decoupling
		// them is a speed optimization.
		while (input < input_end && output < output_end)
		{
			register unsigned char current = *input;

			// Check if we are in the middle of an escape (this works
			// across buffers)
			if (m_charSplit >= 0)
			{
				if (m_charSplit > 0 && (current & 0xC0) == 0x80)
				{
					// If current is 10xxxxxx, we continue to shift bits.
					m_ucs <<= 6;
					m_ucs |= current & 0x3F;
					++ input;
					-- m_charSplit;
				}
				else if (m_charSplit > 0)
				{
					// If current is not 10xxxxxx and we expected more input,
					// this is an illegal character, so we trash it and continue.
					*(output ++) = NOT_A_CHARACTER;
					m_charSplit = -1;
				}

				if (0 == m_charSplit)
				{
					// We are finished. We do not consume any more characters
					// until next iteration.
					if (m_ucs < low_boundary[m_sequence] || Unicode::IsSurrogate(m_ucs))
					{
						// Overlong UTF-8 sequences and surrogates are ill-formed,
						// and must not be interpreted as valid characters
						*output = NOT_A_CHARACTER;
					}
					else if (m_ucs >= 0x10000 && m_ucs <= 0x10FFFF)
					{
						// UTF-16 supports this non-BMP range using surrogates
						uni_char high, low;
						MakeSurrogate(m_ucs, high, low);

						*(output ++) = high;

						if (output == output_end)
						{
							m_surrogate = low;
							m_charSplit = -1;
							written = (output - output_start) * sizeof (uni_char);
							if (read_ext) *read_ext = input - input_start;
							return written;
						}

						*output = low;
					}
					else if (m_ucs >> (sizeof(uni_char) * 8))
					{
						// Non-representable character
						*output = NOT_A_CHARACTER;
					}
					else
					{
						*output = static_cast<uni_char>(m_ucs);
					}

					++ output;
					-- m_charSplit; // = -1
				}
			}
			else
			{
				switch (utf8_len[current])
				{
				case 1:
					// This is a US-ASCII character
					*(output ++) = static_cast<uni_char>(*input);
					written += sizeof (uni_char);
					break;

				// UTF-8 escapes all have high-bit set
				case 2: // 110xxxxx 10xxxxxx
				case 3: // 1110xxxx 10xxxxxx 10xxxxxx
				case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				case 5: // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
				case 6: // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
					m_charSplit = utf8_len[current] - 1;
					m_sequence = m_charSplit - 1;
					m_ucs = current & utf8_mask[m_sequence];
					break;

				case 0: // Illegal UTF-8
					*(output ++) = NOT_A_CHARACTER;
					break;
				}
				++ input;
			}
		}

		// Calculate number of bytes written
		written = (output - output_start) * sizeof (uni_char);
	}
	else
	{
		// Just count the number of bytes needed; duplicates code above,
		// decoupling them is a speed optimization.
		while (input < input_end && written < maxlen)
		{
			register unsigned char current = *input;

			// Check if we are in the middle of an escape (this works
			// across buffers)
			if (m_charSplit >= 0)
			{
				if (m_charSplit > 0 && (current & 0xC0) == 0x80)
				{
					// If current is 10xxxxxx, we continue to shift bits.
					++ input;
					-- m_charSplit;
				}
				else if (m_charSplit > 0)
				{
					// If current is not 10xxxxxx and we expected more input,
					// this is an illegal character, so we trash it and continue.
					m_charSplit = -1;
				}

				if (0 == m_charSplit)
				{
					// We are finished. We do not consume any more characters
					// until next iteration.
					if (low_boundary[m_sequence] > 0xFFFF)
					{
						// Surrogate
						// Overestimates ill-formed characters
						written += 2 * sizeof (uni_char);
					}
					else
					{
						written += sizeof (uni_char);
					}
					-- m_charSplit; // = -1
				}
			}
			else
			{
				switch (utf8_len[current])
				{
				case 1:
					// This is a US-ASCII character
					written += sizeof (uni_char);
					break;

				// UTF-8 escapes all have high-bit set
				case 2: // 110xxxxx 10xxxxxx
				case 3: // 1110xxxx 10xxxxxx 10xxxxxx
				case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				case 5: // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
				case 6: // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
					m_charSplit = utf8_len[current] - 1;
					m_sequence = m_charSplit - 1;
					break;

				case 0: // Illegal UTF-8
					written += sizeof (uni_char);
					break;
				}
				++ input;
			}
		}
	}
	if (read_ext) *read_ext = input - input_start;
	return written;
}

OP_STATUS
read_file( const char *filename, TempBuffer *t )
{
	char buf[2048];
    uni_char buf16[2048];

	FILE *input;

#if defined _MSC_VER || defined _MACINTOSH_
	fopen_s(&input, filename, "r");
#else
	input = fopen(filename, "r");
#endif
	if (input == NULL)
		return OpStatus::ERR_FILE_NOT_FOUND;

	t->Clear();
	t->SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	t->SetCachedLengthPolicy(TempBuffer::TRUSTED);

    JSShell_UTF8toUTF16Converter conv;
    int l = 0, r;

	for (;;)
	{
		int k = fread(buf + l, 1, sizeof(buf) - l, input);
		if (k <= 0) break;
        l += k;
        int w = conv.Convert(buf, l, buf16, sizeof buf16, &r);
        if (r != l)
            op_memmove(buf, buf + r, l - r);
        l -= r;
        if (OpStatus::IsError(t->Append(buf16, w / sizeof buf16[0])))
            return OpStatus::ERR_NO_MEMORY;
	}

	fclose( input );

	return OpStatus::OK;
}
void*
OperaModule::operator new(size_t nbytes, OperaModule* m)
{
	return m;
}

void
Opera::InitL()
{
	OperaInitInfo x;
#ifdef STDLIB_MODULE_REQUIRED
	new (&stdlib_module) StdlibModule();
	stdlib_module.InitL(x);
#endif
}

void
Opera::Destroy()
{
#ifdef STDLIB_MODULE_REQUIRED
	stdlib_module.Destroy();
#endif
}

Opera::~Opera()
{
	Destroy();
}

double
OpSystemInfo::GetTimeUTC()
{
#if defined _MACINTOSH_ || !defined _MSC_VER
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#else
	struct _timeb time;
	_ftime_s(&time);
	return time.time * 1000.0 + time.millitm;
#endif
}

#if !defined _MACINTOSH_ && !defined UNIX
static double tick_length_ms = 0.0;
#endif

double
OpSystemInfo::GetRuntimeMS()
{
#if defined _MACINTOSH_ || defined UNIX
 	struct timeval t;
	gettimeofday(&t, NULL);
	return (double)t.tv_sec * 1000.0 + (double)t.tv_usec / 1000.0;
#else
	if (tick_length_ms == 0)
	{
		LARGE_INTEGER freq;

		QueryPerformanceFrequency(&freq);

		tick_length_ms = 1000.0 / (double)freq.QuadPart;
	}
	LARGE_INTEGER count;

	QueryPerformanceCounter(&count);

	return tick_length_ms * (double)count.QuadPart;

// 	return timeGetTime() * 1000.0;
#endif
}
unsigned int
OpSystemInfo::GetRuntimeTickMS()
{
#if defined _MACINTOSH_ || defined UNIX
	struct timeval t;
	gettimeofday(&t, NULL);
	return static_cast<unsigned int>(op_fmod(static_cast<double>(t.tv_sec) * 1000 + static_cast<double>(t.tv_usec) / 1000, UINT_MAX));
#else
	return GetTickCount();
#endif
}

#if defined _MACINTOSH_ || defined UNIX || !defined(_MSC_VER)

/** Find when DST changes between now and then, to within tol seconds.
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
	while (initial || (n > t + tol || n + tol < t))
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

OpSystemInfo::OpSystemInfo()
{
    m_timezone = ComputeTimezone();
}

// private
/** (Re)Compute m_*_dst and the current time-zone, for use as m_timezone. */
long OpSystemInfo::ComputeTimezone() // Seconds west of Greenwich:
{
	const time_t now = time(NULL);
	if (now == -1)
		return 0;

	struct tm when;
	if (localtime_r(&now, &when) != &when)
		return 0;

	m_is_dst = when.tm_isdst;
	m_next_dst = BoundDST(when, now, +1);
	m_last_dst = BoundDST(when, now, -1);

	/* timezone (from <time.h>) is "... the difference, in seconds, between
	 * Coordinated Universl Time (UTC) and local standard time."  The spec
	 * distinguishes between "local standard time" and "daylight savings time",
	 * so I take it the former is local time without any DST, even if DST is
	 * currently in effect [Eddy/2007/March/27].
	 *
	 * Unfortunately, it's a function on FreeBSD:
	 * http://www.freebsd.org/cgi/man.cgi?query=timezone&manpath=FreeBSD+7.0-RELEASE
	 * but we can always use (non-POSIX, but handy) tm_gmtoff when available.
	 */
	return timezone - (when.tm_isdst > 0 ? 3600 : 0);
}

// private
bool OpSystemInfo::IsDST(double t)
{
	// Use cached data if sufficient:
	if (m_last_dst < t && t < m_next_dst)
		return m_is_dst;

	// else compute:
	time_t sec_time = (time_t)(t / 1000);
	struct tm when;
	if (localtime_r(&sec_time, &when) == &when)
		return when.tm_isdst > 0;

	return 0;
}

#else

int OpSystemInfo::DaylightSavingsTimeAdjustmentMS(double t)
{
	const double msPerHour = 1000.0 * 60.0 * 60.0;
	time_t time = time_t(t / 1000);
	struct tm time_p;
	if ( localtime_s(&time_p, &time) == 0 )
		return (int)(msPerHour * (time_p.tm_isdst > 0));
	return 0;
}

int OpSystemInfo::GetTimezone()
{
	// Note the spec of the PI states unequivocally that the return
	// value incorporates dst, if any.
	struct _timeb time;
	_ftime_s(&time);
	double current_time_millis = time.time * 1000.0 + time.millitm;
	long tz;
	_get_timezone(&tz);
	return tz - (long)(DaylightSavingsTimeAdjustmentMS(current_time_millis)/1000);
}

#endif
