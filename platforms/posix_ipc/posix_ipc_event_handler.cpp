/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc_event_handler.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "platforms/posix_ipc/posix_ipc.h"

#define RETURN_IF_SOURCE_ERROR(source, status) do { \
		OP_STATUS __tmp_status = status; \
		if (HandleSourceStatus(source, __tmp_status)) return __tmp_status; \
	} while(0)

bool PosixIpcEventHandler::HandleSourceStatus(IpcHandle* source, OP_STATUS& status)
{
	if (!OpStatus::IsError(status))
		return false;

	if (m_listener && !OpStatus::IsMemoryError(status))
	{
		m_listener->OnSourceError(source);
		status = OpStatus::OK;
	}
	return true;
}

OP_STATUS PosixIpcEventHandler::HandleEvents(const double* timeout, bool& stop_running)
{
	double current = g_component_manager->GetRuntimeMS();

	int nfds = 0;
	for (unsigned i = 0; i < m_sources.GetCount(); i++)
	{
		nfds = max(nfds, m_sources.Get(i)->GetReadPipe() + 1);
		nfds = max(nfds, m_sources.Get(i)->GetWritePipe() + 1);
	}

	while (!stop_running)
	{
		unsigned waitms;

		// If we're given a timeout, use it;
		// otherwise we call RunSlice to process a message and use the time we get from that
		if (timeout)
			waitms = *timeout < 0 ? UINT_MAX : (unsigned)max(*timeout - current, 0.0);
		else
		{
			waitms = g_component_manager->RunSlice();
			if (stop_running)
				break;
		}

		struct timeval select_timeout = { waitms / 1000, (waitms % 1000) * 1000 };

		fd_set readset;
		FD_ZERO(&readset);
		fd_set writeset;
		FD_ZERO(&writeset);

		for (unsigned i = 0; i < m_sources.GetCount(); ++i)
		{
			FD_SET(m_sources.Get(i)->GetReadPipe(), &readset);
			if (m_sources.Get(i)->WantSend())
				FD_SET(m_sources.Get(i)->GetWritePipe(), &writeset);
		}

		int ret = select(nfds, &readset, &writeset, NULL, waitms == UINT_MAX ? NULL : &select_timeout);

		// Handle input
		if (ret > 0)
		{
			for (unsigned i = 0; i < m_sources.GetCount(); i++)
			{
				IpcHandle* source = m_sources.Get(i);
				if (FD_ISSET(source->GetReadPipe(), &readset))
				{
					RETURN_IF_SOURCE_ERROR(source, source->Receive());

					while (source->MessageAvailable())
					{
						OpTypedMessage* msg = NULL;
						RETURN_IF_SOURCE_ERROR(source, source->ReceiveMessage(msg));
						RETURN_IF_ERROR(g_component_manager->DeliverMessage(msg));
					}
				}

				if (FD_ISSET(source->GetWritePipe(), &writeset))
				{
					RETURN_IF_SOURCE_ERROR(source, source->Send());
				}
			}
		}
		else if (ret < 0)
		{
			if (errno == EINTR)
				return OpStatus::OK; // killed

			fprintf(stderr, "opera ipc [%d]: Error occured during select\n", getpid());
			return OpStatus::ERR;
		}

		// If we were using a timeout, exit the loop after receiving an event
		if (timeout)
			break;
	}
	return OpStatus::OK;
}

#undef RETURN_IF_SOURCE_ERROR
