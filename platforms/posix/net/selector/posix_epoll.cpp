/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef POSIX_SELECTOR_EPOLL

#include "modules/otl/map.h"
#include "platforms/posix/net/selector/posix_selector_base.h"
#include "platforms/posix/posix_logger.h"

#include <sys/epoll.h>

/** A selector that uses the epoll() system, usually available on Linux-based
 * systems. See http://kernel.org/doc/man-pages/online/pages/man7/epoll.7.html
 * for more information.
 */
class EpollSelector : public PosixSelectorBase
{
public:
	EpollSelector(OP_STATUS& status);
	virtual ~EpollSelector();

protected:
	// From PosixSelectorBase
	virtual OP_STATUS SetModeInternal(Watched* watched, Type mode);
	virtual bool PollInternal(double timeout);
	virtual void DetachInternal(Watched* watched);

private:
	const int m_epollfd;
	/* This hash table maps file descriptors to their Watched objects.
	 * It also doubles as a list of live Watched objects, thus
	 * protecting against accessing dead Watched objects.  This is
	 * necessary, since epoll_wait() can give us phantom events, see
	 * DSK-375109.
	 */
	OtlMap<int, Watched*> m_active_fds;
};

OP_STATUS PosixSelector::Create(PosixSelector*& selector)
{
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	selector = OP_NEW(EpollSelector, (status));
	if (OpStatus::IsError(status))
		OP_DELETE(selector);
	return status;
}

EpollSelector::EpollSelector(OP_STATUS& status)
	/* the size argument is ignored since Linux 2.6.8. For older versions, we
	 * provide the initial value of 20, which seems a reasonable amount of
	 * sockets and components to start with but is of no particular importance -
	 * even when the argument wasn't ignored, it only served as a hint at how
	 * much to expect, not a hard commitment.
	 */
	: m_epollfd(epoll_create(20))
{
	Log(CHATTY, "%010p: Started epoll selector\n", this);
	if (m_epollfd == -1)
		status = (errno == ENOMEM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);
	else
		status = OpStatus::OK;
}

EpollSelector::~EpollSelector()
{
	Log(CHATTY, "%010p: Shutting down epoll selector\n", this);
	if (m_epollfd != -1)
		close(m_epollfd);
}

OP_STATUS EpollSelector::SetModeInternal(Watched* watched, Type mode)
{
	struct epoll_event event;

#ifdef VALGRIND
	// Valgrind complains if not all bits in event.data are initialized
	op_memset(&event.data, 0, sizeof(event.data));
#endif

	RETURN_IF_ERROR(m_active_fds.Insert(watched->m_fd, watched));

	event.events = EPOLLRDHUP;
	event.data.fd = watched->m_fd;

	if (mode & READ)
		event.events |= EPOLLIN | EPOLLPRI;
	if (mode & WRITE)
		event.events |= EPOLLOUT;

	int operation = watched->IsWatched() ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
	if (epoll_ctl(m_epollfd, operation, watched->m_fd, &event) == -1)
		return errno == ENOMEM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

	return OpStatus::OK;
}

bool EpollSelector::PollInternal(double timeout)
{
	struct epoll_event eventlist[POSIX_SELECTOR_EVENTS_PER_POLL]; // ARRAY OK 2011-11-10 arjanl

	// Check pending events. If timeout < 0, we could wait indefinitely here
	int num_events = epoll_wait(m_epollfd, eventlist, POSIX_SELECTOR_EVENTS_PER_POLL, timeout < 0 ? -1 : (int)timeout);
	if (num_events < 0 && errno != EINTR)
		Log(TERSE, "epoll_wait() failed with error %d\n", errno);

	if (num_events <= 0)
		return false;

	for (int i = 0; i < num_events; i++)
	{
		OtlMap<int,Watched*>::Iterator watched_iter = m_active_fds.Find(eventlist[i].data.fd);
		if (watched_iter == m_active_fds.End())
			continue;
		Watched* watched = watched_iter->second;
		if (!watched)
			continue; // Shouldn't happen, but just in case
		watched->IncRefCount();

		/* Calling listeners may detach, so we have to test IsDetached() repeatedly. */

		const int disconnected = EPOLLHUP | EPOLLRDHUP;

#ifdef POSIX_OK_NETADDR
		if ((watched->m_mode & WRITE) &&
			(eventlist[i].events & EPOLLOUT) &&
			(eventlist[i].events & disconnected) == 0 &&
			watched->IsActive() &&
			IsConnecting(watched))
			// This may be the signal that we can connect
			TryConnecting(watched);
#endif

		if ((watched->m_mode & READ) && (eventlist[i].events & (EPOLLIN | EPOLLPRI | disconnected)) && watched->IsActive())
			watched->m_listener->OnReadReady(watched->m_fd);

		if ((watched->m_mode & WRITE) && (eventlist[i].events & (EPOLLOUT | disconnected)) && watched->IsActive())
			watched->m_listener->OnWriteReady(watched->m_fd);

		if ((eventlist[i].events & (EPOLLERR | disconnected)) && watched->IsActive())
			watched->m_listener->OnError(watched->m_fd, ECONNRESET);

		watched->DecRefCount();
	}

	return true;
}

void EpollSelector::DetachInternal(Watched* watched)
{
	/* Due to a bug in Linux kernels < 2.6.9, epoll_ctl requires an event
	 * pointer even though the argument is ignored. See
	 * http://kernel.org/doc/man-pages/online/pages/man2/epoll_ctl.2.html#BUGS
	 */
	m_active_fds.Erase(watched->m_fd);
	struct epoll_event event;
#ifdef DEBUG_ENABLE_OPASSERT
	int res =
#endif
		epoll_ctl(m_epollfd, EPOLL_CTL_DEL, watched->m_fd, &event);
	OP_ASSERT(res == 0 || !"Please ensure to call Detach() before close()!");
}

#endif // POSIX_SELECTOR_EPOLL
