/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_SELECT

#include "platforms/posix/posix_selector.h"			// PosixSelector, PosixSelectListener
#include "platforms/posix/net/posix_network_address.h"	// PosixNetworkAddress
#include "platforms/posix/net/posix_base_socket.h"

PosixSelectListener::~PosixSelectListener()
{
	Detach();
}

int PosixSelectListener::DetachAndClose(int fd)
{
	Detach(fd);
	return close(fd);
}

void PosixSelectListener::Detach(int fd /* = -1 */)
{
	if (g_opera && g_posix_selector)
		g_posix_selector->Detach(this, fd);
}

#ifdef POSIX_OK_NETADDR
// static
OP_STATUS PosixSelector::PrepareSocketAddress(PosixNetworkAddress *&posix,
											  const OpSocketAddress *given,
											  int fd, PosixSelectListener *listener)
{
	PosixNetworkAddress *local = OP_NEW(PosixNetworkAddress, ());
	if (local == 0)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS st = local->Import(given);
	if (OpStatus::IsError(st))
	{
		OP_DELETE(local);
		return st;
	}

	RETURN_IF_ERROR(ConnectSocketAddress(local, fd, listener));

	posix = local;
	return OpStatus::OK;
}

// static
OP_STATUS PosixSelector::ConnectSocketAddress(PosixNetworkAddress*& address,
											  int fd, PosixSelectListener *listener)
{
	// Connect() clears errno before calling connect().
	if (address->Connect(fd) == 0 || errno == EISCONN)
	{
		/* Successful call to connect(), or follow-up call after async
		* connect has completed; no need to do it again. */
		OP_DELETE(address);
		address = NULL;
		listener->OnConnected(fd);
	}
	else
		switch (errno)
		{
		case EINPROGRESS:
		case EALREADY:
		case EINTR:
			// That's OK, we'll catch up with it later.
			break;

		case ENOMEM: case ENOBUFS:
			OP_DELETE(address);
			address = NULL;
			return OpStatus::ERR_NO_MEMORY;

		default:
			OP_DELETE(address);
			address = NULL;
			listener->OnError(fd, errno); // May delete listener.
			return OpStatus::ERR;
		}

	return OpStatus::OK;
}
#endif // POSIX_OK_NETADDR

class ButtonGroup
	: public PosixBaseSocket
	, public List<PosixSelector::Button>
	, private Link
{
	const int m_read;
	const int m_write;
	class Ear : public SocketBindListener
	{
		ButtonGroup *const m_boss;
	public:
		Ear(ButtonGroup *boss) : m_boss(boss) {}
		// PosixSelectListener API
		virtual void OnReadReady(int fd) { m_boss->OnReadReady(fd); }
		virtual void OnError(int fd, int err=0) { m_boss->OnDetach(fd); }
		virtual void OnDetach(int fd) { m_boss->OnDetach(fd); }
	} m_ear;

	ButtonGroup(int read, int write)
		: PosixBaseSocket(ASYNC), m_read(read), m_write(write), m_ear(this) {}

	static ButtonGroup *FirstGroup()
	{
		return g_opera && g_posix_selector
			? static_cast<ButtonGroup *>(
				g_opera->posix_module.GetSelectButtonGroups()->First())
			: NULL;
	}
	ButtonGroup *Suc()  const { return static_cast<ButtonGroup *>(Link::Suc()); }
	ButtonGroup *Pred() const { return static_cast<ButtonGroup *>(Link::Pred()); }
	// Find a currently un-used channel, if any
	bool ClearChannel(unsigned char *clear) const;

public:
	~ButtonGroup();
	static OP_STATUS Create(ButtonGroup ** group);
	static char GetChannel(ButtonGroup ** group, OP_STATUS * err);
	bool Write(const char &byte) { errno = 0; return 1 == write(m_write, &byte, 1); }
	void Finish(const char channel);

	void OnReadReady(int fd);
	void OnDetach(int fd) { OP_ASSERT(fd == m_read); Out(); OP_DELETE(this); }
};

OP_STATUS ButtonGroup::Create(ButtonGroup ** group)
{
	if (!g_opera || !g_posix_selector)
		return OpStatus::ERR_NULL_POINTER;

	int fds[2];
	errno = 0;
	// NB: socketpair may be usable in place of pipe.
	if (pipe(fds) == 0)
	{
		ButtonGroup *ans = OP_NEW(ButtonGroup, (fds[0], fds[1]));
		int err;
		OP_STATUS stat = ans
			? (ans->SetSocketFlags(fds[0], &err) && ans->SetSocketFlags(fds[1], &err)
			   ? g_posix_selector->Watch(fds[0], PosixSelector::READ, &(ans->m_ear))
			   : OpStatus::ERR)
			: OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(stat))
			*group = ans;
		else
		{
			OP_DELETE(ans);
			close(fds[0]);
			close(fds[1]);
		}

		return stat;
	}

	const int err = errno;
	switch (err)
	{
		// Documented errno values:
	case EMFILE: // process ran out of file handles
	case ENFILE: // system ran out of file handles
		PosixLogger::PError("pipe", err, Str::S_ERR_FILE_NO_HANDLE,
							"Failed (check ulimit) to create file-handle pair");
		break;
	default:
		OP_ASSERT(!"Non-POSIX error from pipe()");
		// PosixLogger::PError("pipe", err, 0, "failed to create pipe() file-handle pair");
		break;
	}
	return OpStatus::ERR;
}

bool ButtonGroup::ClearChannel(unsigned char *clear) const
{
	// Easy case:
	int i = UCHAR_MAX + 1;
	for (PosixSelector::Button * old = First(); old; old = old->Suc())
		if (int chan = old->m_channel)
			if (chan < i) i = chan;
	if (i > 1) // No channel < i is busy:
	{
		*clear = i - 1;
		return true;
	}

	// Look for a gap in the active channels:
	const int word = sizeof(unsigned int) * CHAR_BIT;
	const int bigenough = (UCHAR_MAX + word - 1) / word;
	unsigned int flags[bigenough] = { 1 }; // ARRAY OK, Eddy/2010/Feb/18th
	/* Channel '\0' is never available, all other channels' bits are implicitly
	 * initialized clear.  In principle, we now need to mark as unavailable any
	 * bits representing channels above UCHAR_MAX: */
	if (bigenough * word > UCHAR_MAX + 1) // unlikely
		for (i = (UCHAR_MAX + 1) % word; i < word; i++)
			flags[bigenough-1] |= 1 << i;

	for (PosixSelector::Button * old = First(); old; old = old->Suc())
		if (old->m_channel) // channel in use
		{
			unsigned char chan = old->m_channel;
			flags[chan / word] |= 1 << (chan % word);
		}

	for (i = bigenough; i-- > 0;)
		if (~flags[i]) // i.e. not all bits set
		{
			unsigned char chan = word;
			while (chan-- > 0)
				if (!(flags[i] & (1 << chan))) // busy channel
				{
					*clear = i * word + chan;
					return true;
				}
			OP_ASSERT(!"We shouldn't get to here; at least one bit was allegedly unset !");
		}

	return false;
}

// static
char ButtonGroup::GetChannel(ButtonGroup ** group, OP_STATUS * err)
{
	for (ButtonGroup * run = FirstGroup(); run; run = run->Suc())
	{
		unsigned char chan;
		if (run->ClearChannel(&chan))
		{
			*group = run;
			return chan;
		}
		// Full button group; try next.
	}
	// No space in an existing button group; try creating another.
	OP_STATUS status = Create(group);
	if (OpStatus::IsSuccess(status))
	{
		OP_ASSERT(*group && g_opera && g_posix_selector); // See Create().
		(*group)->Into(g_opera->posix_module.GetSelectButtonGroups());
		return UCHAR_MAX;
	}

	*err = status;
	return '\0';
}

void ButtonGroup::OnReadReady(int fd)
{
	OP_ASSERT(fd = m_read);
	char buff[32];				// ARRAY OK 2010-08-12 markuso
	ssize_t got;
	while (errno = 0, (got = read(m_read, buff, sizeof(buff))) > 0)
		while (got-- > 0)
			if (buff[got])
			{
				for (PosixSelector::Button * run = First(); run; run = run->Suc())
					if (run->m_channel == buff[got])
					{
						run->OnPressed();
						break;
					}

				// avoid duplicates:
				for (int i = 0; i < got; i++)
					if (buff[i] == buff[got])
						buff[i] = '\0';
			}

#if 0
	switch (errno)
	{
	case EINTR: case AGAINCASE: case ENOBUFS: case ENOMEM: break; // worry about it later
	case EBADF: case EOVERFLOW: // bad news
	case EIO: ENXIO: case EBADMSG: case EINVAL: case EISDIR: // impossible
	case ETIMEDOUT: case ECONNRESET: case ENOTCONN: // socket !
	}
#endif
}

ButtonGroup::~ButtonGroup()
{
	// Called on PosixModule's destruction, also by Finish() and OnDetach()
	for (PosixSelector::Button * run = First(); run; run = run->Suc())
	{
		run->OnDetach();
		run->Out();
	}
}

void ButtonGroup::Finish(const char /* channel */)
{
	if (!Empty()) return;
	size_t spare = 0;
	for (ButtonGroup * run = Suc(); run; run = run->Suc())
		spare += UCHAR_MAX - run->Cardinal();

	for (ButtonGroup * run = Pred(); run; run = run->Pred())
		spare += UCHAR_MAX - run->Cardinal();

	if (spare > UCHAR_MAX / 8) // somewhat arbitrary cut-off
	{
		// Spare empty group; purge !
		Out();
		OP_DELETE(this);
	}
	else if (Suc()) // move to end of list
	{
		Head * was = GetList();
		Out();
		Into(was);
	}
}

PosixSelector::Button::Button()
	: m_channel(ButtonGroup::GetChannel(&d.m_group, &d.m_cause))
{
	if (m_channel)
	{
		OP_ASSERT(d.m_group);
		Into(d.m_group);
	}
	else
		OP_ASSERT(OpStatus::IsError(d.m_cause));
}

PosixSelector::Button::~Button()
{
	if (m_channel)
	{
		if (InList())
		{
			Out();
			OP_ASSERT(d.m_group);
			d.m_group->Finish(m_channel); // *after* Out()
		}
		d.m_group = 0;
	}
}

OP_STATUS PosixSelector::Button::Ready()
{
	return g_opera && g_posix_selector && InList()
		? m_channel ? OpStatus::OK : d.m_cause
		: OpStatus::ERR_NULL_POINTER;
}

bool PosixSelector::Button::Press()
{
	if (m_channel)
		return d.m_group->Write(m_channel);

	OP_ASSERT(!"This button should not be in use;"
			  " its creator should have noticed it isn't Ready() !");
	errno = ENODEV;
	return false;
}

#endif // POSIX_OK_SELECT
