/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/plugin_watchdog.h"

#include <time.h>
#include <unistd.h>

#ifdef DEBUG_ENABLE_OPASSERT
// Expression is either zero (success) or an errno value:
# define ZERO_OR_ERRNO(e) do { int const res = e; OP_ASSERT(!res); } while(0)
#else
# define ZERO_OR_ERRNO(e) e
#endif

PluginWatchdog::~PluginWatchdog()
{
	if (!m_shared_data)
		return;

	Stop();
	ZERO_OR_ERRNO(pthread_mutex_destroy(&m_shared_data->mutex));
	ZERO_OR_ERRNO(pthread_cond_destroy(&m_shared_data->cond));
	OP_DELETE(m_shared_data);
}

inline OP_STATUS PluginWatchdog::Evaluate(int status)
{
	switch (status)
	{
		case ENOMEM:
			return OpStatus::ERR_NO_MEMORY;
		case 0:
			return OpStatus::OK;
		default:
			return OpStatus::ERR;
	}
}

OP_STATUS PluginWatchdog::Init()
{
	OpAutoPtr<SharedData> shared (OP_NEW(SharedData, ()));
	RETURN_OOM_IF_NULL(shared.get());

	pthread_condattr_t attr;
	RETURN_IF_ERROR(Evaluate(pthread_condattr_init(&attr)));

	struct CondAttr {
		CondAttr(pthread_condattr_t& attr) : attr(attr) {}
		~CondAttr() { ZERO_OR_ERRNO(pthread_condattr_destroy(&attr)); }
		pthread_condattr_t& attr;
	} condattr_holder(attr);

	RETURN_IF_ERROR(Evaluate(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)));
	RETURN_IF_ERROR(Evaluate(pthread_cond_init(&shared->cond, &attr)));
	OP_STATUS ret = Evaluate(pthread_mutex_init(&shared->mutex, NULL));
	if (OpStatus::IsError(ret))
	{
		ZERO_OR_ERRNO(pthread_cond_destroy(&shared->cond));
		return ret;
	}

	m_shared_data = shared.release();
	return OpStatus::OK;
}

OP_STATUS PluginWatchdog::Start(unsigned timeout)
{
	OP_ASSERT(timeout != 0);
	OP_ASSERT(m_shared_data->timeout == 0 || !"Starting watchdog twice!");

	m_shared_data->timeout = timeout;

	return Evaluate(pthread_create(&m_thread_id, NULL, &WatchLoop, m_shared_data));
}

void* PluginWatchdog::WatchLoop(void* data)
{
	SharedData* shared = static_cast<SharedData*>(data);

	while (true)
	{
		struct timespec timeout;
		if (clock_gettime(CLOCK_MONOTONIC, &timeout) != 0)
			return NULL;

		UINT64 timeout_ms = timeout.tv_sec * 1000 + timeout.tv_nsec / 1000000;

		ZERO_OR_ERRNO(pthread_mutex_lock(&shared->mutex));

		if (shared->timeout == 0)
		{
			ZERO_OR_ERRNO(pthread_mutex_unlock(&shared->mutex));
			break;
		}

		timeout_ms += shared->timeout;
		timeout.tv_sec = timeout_ms / 1000;
		timeout.tv_nsec = (timeout_ms % 1000) * 1000000;

		int result = pthread_cond_timedwait(&shared->cond, &shared->mutex, &timeout);
		unsigned new_timeout = shared->timeout;

		ZERO_OR_ERRNO(pthread_mutex_unlock(&shared->mutex));

		if (new_timeout == 0)
			break;
		else if (result == ETIMEDOUT)
			_exit(1);
		else if (result != 0)
			break;
	}

	return NULL;
}

void PluginWatchdog::Stop()
{
	ZERO_OR_ERRNO(pthread_mutex_lock(&m_shared_data->mutex));
	unsigned timeout = m_shared_data->timeout;
	m_shared_data->timeout = 0;
	ZERO_OR_ERRNO(pthread_cond_signal(&m_shared_data->cond));
	ZERO_OR_ERRNO(pthread_mutex_unlock(&m_shared_data->mutex));

	if (timeout != 0)
		ZERO_OR_ERRNO(pthread_join(m_thread_id, NULL));
}

void PluginWatchdog::Kick()
{
	ZERO_OR_ERRNO(pthread_cond_signal(&m_shared_data->cond));
}

#endif // defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)
