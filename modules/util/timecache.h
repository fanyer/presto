/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_TIMECACHE_H
#define MODULES_UTIL_TIMECACHE_H

/**
 * Cache the current time. This class is used because on some systems, the
 * call to op_time(NULL) is expensive, and since we need to do it quite a lot,
 * we cache its value, running updates once a second.
 *
 * Now if I could only do this IRL as well, and retrieve my saved time
 * whenever I needed it...
 *
 * @author Peter Karlsson.
 */

class TimeCache
{
#ifdef HAVE_TIMECACHE
public:
	TimeCache();

	/** Get current time. */
	time_t CurrentTime();

private:
	double m_last_check;
	time_t m_current_time;

#else // !HAVE_TIMECACHE
public:
	/** Get current time. */
	static time_t CurrentTime() { return op_time(NULL); };
#endif
};

#endif // !MODULES_UTIL_TIMECACHE_H
