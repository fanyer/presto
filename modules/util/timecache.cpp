/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAVE_TIMECACHE

#include "modules/util/timecache.h"
#include "modules/pi/OpSystemInfo.h"

TimeCache::TimeCache() :
	m_last_check(DBL_MIN)
{
}

time_t
TimeCache::CurrentTime()
{
	double now = g_op_time_info->GetRuntimeMS();
	if (now - m_last_check > 500.0)
	{
		m_current_time = op_time(NULL);
		m_last_check = now;
	}

	return m_current_time;
}

#endif // HAVE_TIMECACHE
