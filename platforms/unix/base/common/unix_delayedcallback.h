/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef UNIX_DELAYEDCALLBACK_H
#define UNIX_DELAYEDCALLBACK_H

#include "modules/hardcore/timer/optimer.h"

class UnixDelayedCallback : public OpTimerListener
{
public:
	static bool Create(int ms, void (*cb)(void * data), void *data);
	UnixDelayedCallback() : m_cb(0), m_data(0) {}

private:
	bool Start(int ms, void (*cb)(void *data), void *data);

	// Implementation of OpTimerListener:
	void OnTimeOut(OpTimer *timer);

	void (*m_cb)(void *data);
	void *m_data;
};

#endif // UNIX_DELAYEDCALLBACK_H
