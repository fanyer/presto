/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#include "platforms/unix/base/common/unix_delayedcallback.h"

bool UnixDelayedCallback::Create(
	int ms, void (*cb)(void *data), void *data)
{
	UnixDelayedCallback *callback = OP_NEW(UnixDelayedCallback, ());
	return callback->Start(ms, cb, data);
}

bool UnixDelayedCallback::Start(int ms, void (*cb)(void *data), void *data)
{
	OP_ASSERT(!m_cb && !m_data);
	m_cb = cb;
	m_data = data;

    OpTimer* timer;
	OP_STATUS st;

	timer = OP_NEW(OpTimer, ());
	st = timer ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(st))
	{
		OP_DELETE(this);
		return false;
	}

	timer->SetTimerListener(this);
	timer->Start(ms);
	return true;
}

void UnixDelayedCallback::OnTimeOut(OpTimer *timer)
{
	m_cb(m_data);
	OP_DELETE(timer);
	OP_DELETE(this);
}
