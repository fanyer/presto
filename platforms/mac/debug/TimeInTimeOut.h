/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef _MAC_DEBUG
#ifndef _TIME_IN_TIME_OUT
#define _TIME_IN_TIME_OUT

#include "platforms/mac/debug/OnScreen.h"

class TimeInTimeOut
{
public:
	TimeInTimeOut(int y=0);
	~TimeInTimeOut();
private:
	int mx, my;
	unsigned long long m_start;
	static unsigned long long s_end[200];
};

#endif // _TIME_IN_TIME_OUT

#endif // _MAC_DEBUG
