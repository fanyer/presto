/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

OP_STATUS delete_file(const uni_char *name);
OP_STATUS copy_file(const uni_char *src, const uni_char *dst);

void op_msleep(unsigned msec);

#if defined _WINDOWS || defined MSWIN
#define MKPATH(d, s) UNI_L(d) UNI_L(s)
#else
#define MKPATH(d, s) UNI_L(d s)
#endif

#if defined _MSC_VER && defined _DEBUG && !defined ENABLE_MEMORY_DEBUGGING && !defined WINCE

#include <crtdbg.h>

class _CrtLeakCheck
{
	_CrtMemState memstate1, memstate2;

public:
	_CrtLeakCheck(void)
	{
		 _CrtMemCheckpoint(&memstate1);
	}

	~_CrtLeakCheck(void)
	{
		_CrtMemState memstatediff;

		_CrtMemCheckpoint(&memstate2);

		if (_CrtMemDifference(&memstatediff, &memstate1, &memstate2))
		{
			_CrtMemDumpStatistics(&memstatediff);
			_CrtMemDumpAllObjectsSince(&memstate1);
			//_CrtDumpMemoryLeaks();
		}
	}
};

#define LEAKCHECK _CrtLeakCheck _crt_leak_check

#else

#define LEAKCHECK

#endif

#endif  // TEST_UTIL_H
