/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(POSIX_SELECTOR_EPOLL) || defined(POSIX_SELECTOR_KQUEUE)

#include "platforms/posix/net/selector/posix_selector_base.h"

#ifdef POSIX_OK_NETADDR
#include "platforms/posix/net/posix_network_address.h"
#endif // POSIX_OK_NETADDR

PosixSelectorBase::~PosixSelectorBase()
{
#ifdef POSIX_OK_NETADDR
	DetachListeners(m_connecting);
#endif
	DetachListeners(m_watched);
}

void PosixSelectorBase::DetachListeners(List<Watched>& list)
{
	while (Watched *watched = list.First())
	{
		Log(VERBOSE, "%010p: fd: %d; Disconnecting listener %010p\n", this, watched->m_fd, watched->m_listener);
		/* Calling OnDetach() might try to remove this or other items from the
		 * list, so we first remove this one before calling it
		 */
		watched->Out();
		if (watched->IsActive())
			watched->m_listener->OnDetach(watched->m_fd);
		watched->Destroy();
	}
}

OP_STATUS PosixSelectorBase::Watch(int fd, Type mode, unsigned long, PosixSelectListener *listener, const OpSocketAddress* connect)
{
	return Watch(fd, mode, listener, connect, false);
}

OP_STATUS PosixSelectorBase::Watch(int fd, Type mode, PosixSelectListener *listener, const OpSocketAddress* connect, bool)
{
	OP_ASSERT(listener);

	OpAutoPtr<Watched> watched (OP_NEW(Watched, (listener, fd, mode)));
	if (!watched.get())
		return OpStatus::ERR_NO_MEMORY;

#ifdef POSIX_OK_NETADDR
	/* Tries to connect the address the first time, see PosixSelector documentation.
	 * If the connection succeeds, watched->address will be NULL afterwards.
	 */
	if (connect)
		RETURN_IF_ERROR(PrepareSocketAddress(watched->m_address, connect, fd, listener));
#endif // POSIX_OK_NETADDR

	RETURN_IF_ERROR(SetModeInternal(watched.get(), mode));
	watched->SetWatched(true);

#ifdef POSIX_OK_NETADDR
	if (watched->m_address)
	{
		Log(VERBOSE, "%010p: Watching %d with %010p in mode %d - waiting to connect.\n",
			this, fd, listener, (int)mode);
		watched.release()->Into(&m_connecting);
	}
	else
#endif
	{
		Log(VERBOSE, "%010p: Watching %d with %010p in mode %d.\n",
			this, fd, listener, (int)mode);
		watched.release()->Into(&m_watched);
	}

	return OpStatus::OK;
}

bool PosixSelectorBase::Poll(double timeout)
{
#ifdef POSIX_OK_NETADDR
	TryConnecting();
#endif // POSIX_OK_NETADDR

	return PollInternal(timeout);
}

#ifdef POSIX_OK_NETADDR
void PosixSelectorBase::TryConnecting(Watched* watched)
{
	OP_ASSERT(IsConnecting(watched));
	OpStatus::Ignore(ConnectSocketAddress(watched->m_address, watched->m_fd, watched->m_listener));

	if (!watched->m_address && IsConnecting(watched))
	{
		Log(VERBOSE, "%010p: Connected %d with listener %010p.\n",
			this, watched->m_fd, watched->m_listener);
		watched->Out();
		watched->Into(&m_watched);
	}
}

void PosixSelectorBase::TryConnecting()
{
	for (Watched *toconnect = m_connecting.First(), *next; toconnect; toconnect = next)
	{
		next = toconnect->Suc();
		TryConnecting(toconnect);

		// "next" element can be detached during
		// the "TryConnecting(toconnect)" call.
		// If it happened - restart the loop.
		if (next && !m_connecting.HasLink(next))
			next = m_connecting.First();
	}
}
#endif // POSIX_OK_NETADDR

void PosixSelectorBase::Detach(const PosixSelectListener *whom, int fd)
{
#ifdef POSIX_OK_NETADDR
	Detach(m_connecting, whom, fd);
#endif
	Detach(m_watched, whom, fd);
}

void PosixSelectorBase::Detach(List<Watched>& from, const PosixSelectListener* listener, int fd)
{
	for (Watched *watched = from.First(), *next; watched; watched = next)
	{
		next = watched->Suc();
		if (watched->m_listener == listener && (fd == -1 || fd == watched->m_fd))
		{
			Log(VERBOSE, "%010p: Detach %010p listener (file %d=%d).\n",
				this, listener, fd, watched->m_fd);
			DetachInternal(watched);
			watched->SetWatched(false);

			/* Remove from list. If they are not referenced elsewhere (e.g. in
			 * the list of waiting poll events), it is deleted. */
			watched->Out();
			watched->Destroy();
		}
	}
}

void PosixSelectorBase::SetMode(const PosixSelectListener * listener, int fd, Type mode)
{
#ifdef POSIX_OK_NETADDR
	SetMode(m_connecting, listener, fd, mode);
#endif
	SetMode(m_watched, listener, fd, mode);
}

void PosixSelectorBase::SetMode(List<Watched>& from, const PosixSelectListener* listener, int fd, Type mode)
{
	for (Watched *watched = from.First(); watched; watched = watched->Suc())
	{
		if (watched->m_listener == listener && (fd == -1 || fd == watched->m_fd))
		{
			if (watched->m_mode == mode)
				continue;

			Log(SPEW, "%010p: Adjusting %010p listener (file %d=%d) mode %d -> %d\n",
				this, listener, fd, watched->m_fd, (int)watched->m_mode, (int)mode);
			watched->m_mode = mode;
			OpStatus::Ignore(SetModeInternal(watched, mode));
		}
	}
}

PosixSelectorBase::Watched::Watched(PosixSelectListener* listener, int fd, PosixSelector::Type mode)
	: m_ref_count(1), m_is_active(true)
	, m_listener(listener), m_fd(fd), m_mode(mode)
	, m_is_watched(false)
#ifdef POSIX_OK_NETADDR
	, m_address(NULL)
#endif // POSIX_OK_NETADDR
{
}

PosixSelectorBase::Watched::~Watched()
{
#ifdef POSIX_OK_NETADDR
	OP_DELETE(m_address);
#endif // POSIX_OK_NETADDR
}

#endif // defined(POSIX_SELECTOR_EPOLL) || defined(POSIX_SELECTOR_KQUEUE)
