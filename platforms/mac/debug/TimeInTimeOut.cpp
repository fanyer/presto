/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _MAC_DEBUG

#ifndef SIXTY_FOUR_BIT

#include "platforms/mac/debug/TimeInTimeOut.h"

unsigned long long TimeInTimeOut::s_end[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

TimeInTimeOut::TimeInTimeOut(int y)
	: mx(0), my(y), m_start(0)
{
	OP_ASSERT(y<200);
	Microseconds((UnsignedWide*)&m_start);
	if (s_end[my])
		DecOnScreen(mx*8+48, my*8, kOnScreenRed, kOnScreenBlack, (m_start-s_end[my/8])/1000);
}

TimeInTimeOut::~TimeInTimeOut()
{
	Microseconds((UnsignedWide*)&s_end[my]);
	DecOnScreen(mx*8, my*8, kOnScreenGreen, kOnScreenBlack, (s_end[my/8]-m_start)/1000);
}

#endif // SIXTY_FOUR_BIT
#endif // _MAC_DEBUG
