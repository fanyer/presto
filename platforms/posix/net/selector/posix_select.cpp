/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#if defined(POSIX_SELECTOR_SELECT) || defined(POSIX_SELECTOR_SELECT_FALLBACK)

#include "platforms/posix/posix_selector.h"				// PosixSelector, PosixSelectListener
#include "platforms/posix/net/posix_network_address.h"	// PosixNetworkAddress
#include "platforms/posix/posix_signal_watcher.h"		// PosixSignalListener
#include "modules/util/simset.h"						// AutoDeleteHead, Link

#ifdef POSIX_OK_NETADDR
#define IF_NETADDR(X) , X
#else
#define IF_NETADDR(X) /* skip it */
#endif // POSIX_OK_NETADDR

class SelectListenerCarrier : public Link
{
public:
	const int m_file;
	PosixSelector::Type m_type;
	PosixSelectListener *m_ear;
#ifdef POSIX_OK_NETADDR
	PosixNetworkAddress *m_address;
#endif

	SelectListenerCarrier(int fd, PosixSelector::Type mode,
						  PosixSelectListener *listener
						  IF_NETADDR(PosixNetworkAddress *connect))
		: m_file(fd), m_type(mode), m_ear(listener)
		  IF_NETADDR(m_address(connect))
		{ OP_ASSERT(m_file >= 0 && m_ear); }

	~SelectListenerCarrier()
	{
		OP_ASSERT(!InList());
#ifdef POSIX_OK_NETADDR
		OP_DELETE(m_address);
#endif
		OP_ASSERT(m_file >= 0);
	}

	SelectListenerCarrier* Suc()
		{ return static_cast<SelectListenerCarrier*>(Link::Suc()); }
	bool Carries(const PosixSelectListener *whom)
		{ return whom == m_ear; }
	bool Carries(const PosixSelectListener *whom, int fd)
		{ return whom == m_ear && fd == m_file; }
};

class SelectData
{
public:
	fd_set m_read;
	fd_set m_write;
	fd_set m_except;
	SelectData() { FD_ZERO(&m_read); FD_ZERO(&m_write); FD_ZERO(&m_except); }

	void Set(int fd, unsigned int mode)
	{
		FD_SET(fd, &m_except);
		if (mode & PosixSelector::READ)
			FD_SET(fd, &m_read);
		if (mode & PosixSelector::WRITE)
			FD_SET(fd, &m_write);
	}

	unsigned int Check(int fd, bool& error)
	{
		unsigned int mode = 0;
		if (FD_ISSET(fd, &m_read))
			mode |= PosixSelector::READ;
		if (FD_ISSET(fd, &m_write))
			mode |= PosixSelector::WRITE;

		error = FD_ISSET(fd, &m_except);
		return mode;
	}
};

class SimplePosixSelector
	: public PosixSelector, public AutoDeleteHead, public PosixLogger
{
private:
	SelectListenerCarrier *First()
		{ return static_cast<SelectListenerCarrier *>(Head::First()); }

	/** Flag set during reporting.
	 *
	 * Set true by Work() to tell Delete() when it's not safe to really do
	 * deletions.  Work() takes care of actually chasing up the deletions
	 * after it's set this back to false.
	 */
	bool m_reporting;

	/** Delete or mark for deletion.
	 *
	 * Used by Detach() to "remove" an item from the list.  However, if called
	 * during reporting of select()'s results, the item is merely marked for
	 * later removal, by clearing its m_ear (which is typically points to an
	 * object that's about to be deleted anyway).  This avoids assorted problems
	 * with entries in the list being deleted as a side-effect of call-backs
	 * while Work() is iterating over the list; instead, such detached entries
	 * are marked for deletion.
	 */
	void Delete(SelectListenerCarrier *whom);

	/** Purge entries marked for deletion while m_reporting was true.
	 */
	void Purge()
	{
		SelectListenerCarrier *run = First();
		while (run)
		{
			SelectListenerCarrier *next = run->Suc();

			if (!run->m_ear)
				Delete(run);

			run = next;
		}
	}

public:
	SimplePosixSelector()
		: PosixLogger(ASYNC), m_reporting(false)
		{ Log(CHATTY, "%010p: Started simple selector\n", this); }
	~SimplePosixSelector();

	// PosixSelector API:
	virtual bool Poll(double timeout);

	virtual void Detach(const PosixSelectListener *whom, int fd);
	virtual void Detach(const PosixSelectListener *whom);
	virtual void SetMode(const PosixSelectListener * listener, Type mode);
	virtual void SetMode(const PosixSelectListener * listener, int fd, Type mode);

	virtual OP_STATUS Watch(int fd, Type mode, PosixSelectListener *listener,
							const class OpSocketAddress *connect=0, bool accepts=false);
	DEPRECATED(virtual OP_STATUS Watch(int fd, Type mode, unsigned long interval,
									   PosixSelectListener *listener,
									   const class OpSocketAddress *connect=0));
};

#ifdef POSIX_SELECTOR_SELECT
// static
OP_STATUS PosixSelector::Create(PosixSelector * &selector)
{
	OP_ASSERT(g_opera && !g_posix_selector); // Expect to be a singleton.
	selector = OP_NEW(SimplePosixSelector, ());
	return selector ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // POSIX_SELECTOR_SELECT

#ifdef POSIX_SELECTOR_SELECT_FALLBACK
// static
OP_STATUS PosixSelector::CreateWithFallback(PosixSelector * &selector)
{
	if (OpStatus::IsError(PosixSelector::Create(selector)))
	{
		OP_ASSERT(g_opera && !g_posix_selector); // Expect to be a singleton.
		selector = OP_NEW(SimplePosixSelector, ());
		return selector ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}
#endif // POSIX_SELECTOR_SELECT_FALLBACK

SimplePosixSelector::~SimplePosixSelector()
{
	Log(CHATTY, "%010p: Shutting down simple selector\n", this);

	// Give all listeners fair warning before we shut down.
	while (SelectListenerCarrier *run = First())
	{
		run->Out(); // makes it safe for OnDetach() to delete or otherwise Detach m_ear
		run->m_ear->OnDetach(run->m_file); // May Out()/delete other list entries.
		OP_DELETE(run);
	}
}

void SimplePosixSelector::Delete(SelectListenerCarrier *whom)
{
	Log(VERBOSE, "%010p: Disconnecting listener %010p\n", this, whom->m_ear);
	whom->m_type = NONE;
	if (m_reporting)
		whom->m_ear = 0;
	else
	{
		whom->Out();
		OP_DELETE(whom);
	}
}

void SimplePosixSelector::Detach(const PosixSelectListener *whom)
{
	OP_ASSERT(whom);
	SelectListenerCarrier *run = First();
	while (run)
	{
		SelectListenerCarrier *next = run->Suc();
		if (run->Carries(whom)) Delete(run);
		run = next;
	}
}

void SimplePosixSelector::Detach(const PosixSelectListener *whom, int fd)
{
	OP_ASSERT(whom && fd >= 0);
	SelectListenerCarrier *run = First();
	while (run)
	{
		SelectListenerCarrier *next = run->Suc();
		if (run->Carries(whom, fd)) Delete(run);
		run = next;
	}
}

void SimplePosixSelector::SetMode(const PosixSelectListener * whom, Type mode)
{
	OP_ASSERT(whom);
	for (SelectListenerCarrier *run = First(); run; run = run->Suc())
		if (run->Carries(whom))
		{
			Log(SPEW, "%010p: Adjusting %010p listener (file %d) mode %d -> %d\n",
				this, whom, run->m_file, (int)run->m_type, (int)mode);
			run->m_type = mode;
		}
}

void SimplePosixSelector::SetMode(const PosixSelectListener * whom, int fd, Type mode)
{
	OP_ASSERT(whom && fd >= 0);
	for (SelectListenerCarrier *run = First(); run; run = run->Suc())
		if (run->Carries(whom, fd))
		{
			Log(SPEW, "%010p: Adjusting %010p listener (file %d=%d) mode %d -> %d\n",
				this, whom, fd, run->m_file, (int)run->m_type, (int)mode);
			run->m_type = mode;
		}
}

OP_STATUS SimplePosixSelector::Watch(int fd, Type mode, unsigned long interval,
									 PosixSelectListener *listener,
									 const OpSocketAddress *connect /* = 0 */)
{
	return Watch(fd, mode, listener, connect);
}

OP_STATUS SimplePosixSelector::Watch(int fd, Type mode,
									 PosixSelectListener *listener,
									 const OpSocketAddress *connect /* = 0 */,
									 bool /* ignored = false */)
{
	Log(VERBOSE, "%010p: Watching %d with %010p in mode %d.\n",
		this, fd, listener, (int)mode);

	if (!listener)
		return OpStatus::ERR_NULL_POINTER;

#ifdef _DEBUG
	/* Check for duplicated listeners.
	 *
	 * From this code's point of view, there's no problem supporting duplicates;
	 * however, particularly if OpSocket::{Pause,Continue}Recv() calls are out
	 * of balance, this very likely is a mistake by the caller.  In particular,
	 * the next call to Detach() shall kill all duplicates, not just the one
	 * most recently added.  That behaviour could easily change, if someone
	 * comes up with a plausible use-case.
	 */
	for (SelectListenerCarrier *run = First(); run; run = run->Suc())
		if (run->Carries(listener, fd))
			OP_ASSERT(!"Duplicating listener on same file handle");
#endif

#ifdef POSIX_OK_NETADDR
	PosixNetworkAddress *local = 0;
	if (connect)
		RETURN_IF_ERROR(PrepareSocketAddress(local, connect, fd, listener));
#else
	OP_ASSERT(connect == 0 || !"Import API_POSIX_NETADDR to "
			  "activate PosixSelector's connect functionality.");
#endif

	SelectListenerCarrier *lug =
		OP_NEW(SelectListenerCarrier, (fd, mode, listener IF_NETADDR(local)));
	if (lug == 0)
	{
#ifdef POSIX_OK_NETADDR
		OP_DELETE(local);
#endif
		return OpStatus::ERR_NO_MEMORY;
	}

	lug->Into(this);
	return OpStatus::OK;
}

bool SimplePosixSelector::Poll(double timeout)
{
	OP_ASSERT(!m_reporting); // Recursive call: not supported.  See documentation.
	if (Empty())
		return false; // nothing to do, I don't care if you do want to wait for it.

	SelectData data;

	SelectListenerCarrier *run;
#ifdef POSIX_OK_NETADDR
	m_reporting = true; // so Delete() merely clears m_ear, if called
	run = First();
	while (run)
	{
		SelectListenerCarrier *next = run->Suc();
		if (run->m_address && run->m_ear)
			OpStatus::Ignore(ConnectSocketAddress(run->m_address, run->m_file, run->m_ear));

		run = next;
	}
	m_reporting = false;
	Purge();
#endif // POSIX_OK_NETADDR
	int max_fd = 0;
	for (run = First(); run; run = run->Suc())
	{
		OP_ASSERT(run->InList() && run->m_file >= 0 && run->m_ear);
		// NB: Set(, 0) ensures still we get error reports, even when mode is NONE.
		data.Set(run->m_file,
#ifdef POSIX_OK_NETADDR
				 /* Pending connections get a write notification: */
				 run->m_address ? WRITE :
#endif
				 run->m_type);

		if (run->m_file > max_fd)
			max_fd = run->m_file;
	}

	timeval wait = { 0, 0 }, *pause = &wait;
	if (timeout < 0)
		pause = NULL;
	else if (timeout > 0)
	{
		wait.tv_sec = timeout / 1000;
		wait.tv_usec = (timeout - 1000 * wait.tv_sec) * 1000;
	}
	int res = select(max_fd + 1, &data.m_read, &data.m_write, &data.m_except, pause);

	if (res < 0 && errno != EINTR)
		PError("select", errno, Str::S_ERR_SELECT_FAILED,
			   "Failed to discover whether data is available");
	else if (res > 0)
	{
		m_reporting = true; // make Delete() merely clear m_ear

		for (run = First(); run; run = run->Suc())
			if (run->m_ear) // It might have been Delete()d by someone else's call-back.
			{
				int err = 0;
				bool has_err;
				unsigned int mode = data.Check(run->m_file, has_err);

				/* NB: run->m_ear->On*() may Detach() list entries or SetMode();
				 * either way, run->m_type changes. */
				if (mode & READ & run->m_type)
					run->m_ear->OnReadReady(run->m_file);

				if (mode & WRITE & run->m_type)
				{
					OP_ASSERT(run->m_ear); // Delete() clears m_type when clearing m_ear.
#ifdef POSIX_OK_NETADDR
					if (run->m_address)
					{
						/* If connect() was non-blocking, i.e. it returned e.g.
						 * EINPROGRESS, we did not delete the address, but
						 * selected the socket for writing. So now we need to
						 * determine if connect was successful. */
						OP_DELETE(run->m_address);
						run->m_address = 0;
						socklen_t len = sizeof(int);
						if (-1 == getsockopt(run->m_file, SOL_SOCKET, SO_ERROR,
											 &err, &len))
							err = errno;
						if (err)
							has_err = true;
						else
							run->m_ear->OnConnected(run->m_file);
					}
					else
#endif // POSIX_OK_NETADDR
						run->m_ear->OnWriteReady(run->m_file);
				}

				if (run->m_ear && has_err)
					// Do we have any way to find out *what* the error was ?
					run->m_ear->OnError(run->m_file, err);
			}

		m_reporting = false; // allow Delete() to do its job properly
		Purge();
		Log(SPEW, "%010p: Processed select notifications.\n", this);

		return true;
	}
	return false;
}
#endif // POSIX_SELECTOR_SELECT || POSIX_SELECTOR_SELECT_FALLBACK
