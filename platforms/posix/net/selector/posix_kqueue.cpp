/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef POSIX_SELECTOR_KQUEUE

#include "platforms/posix/net/selector/posix_selector_base.h"
#include "platforms/posix/posix_logger.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

/** A selector that uses the kqueue() system, usually available on BSD-based
 * systems. See http://www.freebsd.org/cgi/man.cgi?query=kqueue for more
 * information.
 */
class KqueueSelector : public PosixSelectorBase
{
public:
	KqueueSelector(OP_STATUS& status);
	virtual ~KqueueSelector();

protected:
	// From EdgeTriggeredSelector
	virtual OP_STATUS SetModeInternal(Watched* watched, Type mode);
	virtual bool PollInternal(double timeout);
	virtual void DetachInternal(Watched* watched);

private:
	const int m_kqueue;
};

OP_STATUS PosixSelector::Create(PosixSelector*& selector)
{
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	selector = OP_NEW(KqueueSelector, (status));
	if (OpStatus::IsError(status))
		OP_DELETE(selector);
	return status;
}

KqueueSelector::KqueueSelector(OP_STATUS& status)
	: m_kqueue(kqueue())
{
	Log(CHATTY, "%010p: Started kqueue selector\n", this);
	if (m_kqueue == -1)
		status = (errno == ENOMEM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);
	else
		status = OpStatus::OK;
}

KqueueSelector::~KqueueSelector()
{
	Log(CHATTY, "%010p: Shutting down kqueue selector\n", this);
	if (m_kqueue != -1)
		close(m_kqueue);
}

OP_STATUS KqueueSelector::SetModeInternal(Watched* watched, Type mode)
{
	/* We always setup both filters (read and write), but disable/enable according to mode
	 * so that we only have to do one system call in this function.
	 * If the filter didn't exist yet, using EV_ADD will create it.
	 */
	struct kevent eventlist[2];

	unsigned read = (mode & READ) ? EV_ENABLE : EV_DISABLE;
	EV_SET(&eventlist[0], watched->m_fd, EVFILT_READ, EV_ADD | read, 0, 0, watched);

	unsigned write = (mode & WRITE) ? EV_ENABLE : EV_DISABLE;
	EV_SET(&eventlist[1], watched->m_fd, EVFILT_WRITE, EV_ADD | write, 0, 0, watched);

	int status = kevent(m_kqueue, eventlist, 2, NULL, 0, NULL);

	if (status < 0)
		return errno == ENOMEM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

	return OpStatus::OK;
}

bool KqueueSelector::PollInternal(double timeout)
{
	struct kevent eventlist[POSIX_SELECTOR_EVENTS_PER_POLL]; // ARRAY OK 2011-11-10 arjanl
	struct timespec event_timeout = { 0, 0 };

	if (timeout > 0.0)
	{
		event_timeout.tv_sec = timeout / 1e3;
		event_timeout.tv_nsec = (timeout - event_timeout.tv_sec * 1e3) * 1e6;
	}

	// Check pending events. If timeout < 0, we could wait indefinitely here
	int num_events = kevent(m_kqueue, NULL, 0, eventlist, POSIX_SELECTOR_EVENTS_PER_POLL, timeout >= 0.0 ? &event_timeout : NULL);
	if (num_events < 0 && errno != EINTR)
		Log(TERSE, "kevent() failed with error %d\n", errno);

	if (num_events <= 0)
		return false;

	int i;
	for (i = 0; i < num_events; i++)
		static_cast<Watched*>(eventlist[i].udata)->IncRefCount();

	for (i = 0; i < num_events; i++)
	{
		Watched* watched = static_cast<Watched*>(eventlist[i].udata);
		if (!watched->IsActive())
			continue;

		switch (eventlist[i].filter)
		{
		case EVFILT_READ:
			watched->m_listener->OnReadReady(watched->m_fd);
			break;

		case EVFILT_WRITE:
#ifdef POSIX_OK_NETADDR
			if (IsConnecting(watched))
				TryConnecting(watched);
#endif
			watched->m_listener->OnWriteReady(watched->m_fd);
			break;
		}

		if ((eventlist[i].flags & EV_ERROR) && watched->IsActive())
			watched->m_listener->OnError(watched->m_fd, eventlist[i].data);

		watched->DecRefCount();
	}

	return true;
}

void KqueueSelector::DetachInternal(Watched* watched)
{
	// Remove from kqueue
	struct kevent eventlist[2];
	EV_SET(&eventlist[0], watched->m_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	EV_SET(&eventlist[1], watched->m_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(m_kqueue, eventlist, 2, NULL, 0, NULL);
}

#endif // POSIX_SELECTOR_KQUEUE
