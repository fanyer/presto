/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
// TODO: see Herman's comments on DSK-252976

#ifdef POSIX_OK_UDP

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "platforms/posix/net/posix_network_address.h"
#include "platforms/posix/net/posix_base_socket.h"
#include "modules/pi/network/OpUdpSocket.h"
#include "modules/hardcore/mh/mh.h"

class PosixUdpSocket : public OpUdpSocket, public PosixBaseSocket
{
private:
	OpUdpSocketListener* m_ear; // May be NULL
	PosixNetworkAddress m_here; ///< Identifies network interface in case of multicast
	bool m_bound;
	bool m_is_multi;
	int m_fd;

	static OP_NETWORK_STATUS HandleError(int err);
	OP_NETWORK_STATUS PrepareSocketHandle(PosixNetworkAddress *posix, RoutingScheme how);

	class BindListener : public SocketBindListener
	{
		// NB: base class destructor detaches this, so we don't need to think about it.
		PosixUdpSocket *const m_boss;
	public:
		BindListener(PosixUdpSocket* boss, int fd) : m_boss(boss) {}

		// PosixSelectListener API:
		virtual void OnError(int fd, int err)	{ m_boss->OnBindError(fd); }
		virtual void OnDetach(int fd)			{ m_boss->OnEndBind(fd); }
		virtual void OnReadReady(int fd)		{ m_boss->OnBindDataReady(fd); }
	} *m_bind;

	OP_STATUS Listen()
	{
		Unbind();
		m_bind = OP_NEW(BindListener, (this, m_fd));
		if (!m_bind)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS st = g_posix_selector->Watch(m_fd, PosixSelector::READ, m_bind);
		if (OpStatus::IsError(st))
		{
			OP_DELETE(m_bind);
			m_bind = 0;
		}
		else
			m_bound = true;

		return st;
	}

	void DetachAndClose()
	{
		if (m_bind)
			m_bind->DetachAndClose(m_fd);
		else if (m_fd != -1)
			close(m_fd);
		m_fd = -1;
	}
	void Unbind() { OP_DELETE(m_bind); }

public:
	// Implementation details:
	PosixUdpSocket(OpUdpSocketListener* listener)
		: m_ear(listener), // m_here initializes itself to unset.
		  m_bound(false), m_is_multi(false), m_fd(-1), m_bind(0)
	{
		Log(VERBOSE, "%010p: Created UDP socket.\n", this);
	}
	virtual ~PosixUdpSocket()
	{
		Log(VERBOSE, "%010p: Destroyed UDP socket.\n", this);
		DetachAndClose();
		Unbind();
	}

	OP_STATUS Init(OpSocketAddress* here)
	{
		if (here)
		{
			RETURN_IF_ERROR(m_here.Copy(here));
#ifdef POSIX_OK_LOG
			OpString8 where;
			if (OpStatus::IsSuccess(m_here.ToString8(&where)))
				Log(VERBOSE, "%010p: Using local address %s\n", this, where.CStr());
#endif
		}

		return OpStatus::OK;
	}

	// Support for BindListener()'s methods, above:
	void OnBindError(int fd)
	{
		OP_ASSERT(fd == m_fd);
		Log(NORMAL, "%010p: Error on UDP socket %d following bind().\n", this, m_fd);
		DetachAndClose();
		// if (m_ear) m_ear->... no way to report error !
	}

	void OnEndBind(int fd)
	{
		OP_ASSERT(fd == m_fd);
		Log(NORMAL, "%010p: UDP socket %d from bind() prematurely stopped.\n", this, m_fd);
		// DetachAndClose(); // ?
		// if (m_ear) m_ear->... no way to report closure !
	}

	void OnBindDataReady(int fd)
	{
		OP_ASSERT(fd == m_fd);
		Log(SPEW, "%010p: Data ready on UDP socket %d\n", this, m_fd);
		OP_ASSERT(OnCoreThread());
		if (m_ear)
			m_ear->OnSocketDataReady(this);
	}

	// OpUdpSocket API:
	virtual void SetListener(OpUdpSocketListener* listener) { m_ear = listener; }
	virtual OP_NETWORK_STATUS Bind(OpSocketAddress* there,
								   RoutingScheme how = UNICAST);
	virtual OP_NETWORK_STATUS Send(const void* data, unsigned int length,
								   OpSocketAddress* there,
								   RoutingScheme how = UNICAST);
	virtual OP_NETWORK_STATUS Receive(void* buffer, unsigned int length,
									  OpSocketAddress* there,
									  unsigned int* bytes_received);
};

// static
OP_STATUS OpUdpSocket::Create(OpUdpSocket** new_object,
							  OpUdpSocketListener* listener,
							  OpSocketAddress* here /* = NULL */)
{
	*new_object = 0;
	PosixUdpSocket * posix = OP_NEW(PosixUdpSocket, (listener));
	if (posix == 0)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS rs = posix->Init(here);
	if (OpStatus::IsError(rs))
	{
		OP_DELETE(posix);
		return rs;
	}

	*new_object = posix;
	return OpStatus::OK;
}

OP_NETWORK_STATUS PosixUdpSocket::PrepareSocketHandle(PosixNetworkAddress *posix,
													  RoutingScheme how)
{
	errno = 0;
	while (m_fd == -1) // "while" used as "if" from which we can break !
	{
		int domain = posix->GetDomain();
		int err = 0;
		m_fd = OpenSocket(domain, SOCK_DGRAM, 0);
		if (m_fd == -1)
			err = errno;

		else if (PosixBaseSocket::SetSocketFlags(m_fd, &err))
		{
			/* For further possible multicast-related details, see:
			 * http://tldp.org/HOWTO/Multicast-HOWTO-6.html */
			Log(VERBOSE, "%010p: Opened datagram/%d socket %d\n", this, domain, m_fd);
			break;
		}

		return HandleError(err);
	}

	if (!m_is_multi && how == MULTICAST)
	{
		/* The recipient of a multicast send shall reply to the host and port
		 * from which we sent, and the O/S shall try to deliver that to the
		 * socket that sent it, just as if we'd done a Bind().  So Send() and
		 * Bind() both need this set-up - c.f. DSK-259666 - but we need to be
		 * sure we only do it once (hence m_is_multi). */

		if (!m_here.Port())
			m_here.SetPort(posix->Port());

		int reuse = 1;
		if (0 != setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) ||
			0 != posix->JoinMulticast(m_fd, &m_here))
			return HandleError(errno);

		m_is_multi = true;
	}
	return OpStatus::OK;
}

OP_NETWORK_STATUS PosixUdpSocket::Bind(OpSocketAddress* there,
									   RoutingScheme how /* = UNICAST */)
{
	if (m_bound) // Re-binding is forbidden.
		return OpStatus::ERR;

	PosixNetworkAddress posix;
	RETURN_IF_ERROR(posix.Convert(there, how == BROADCAST));
	RETURN_IF_ERROR(PrepareSocketHandle(&posix, how));

	if (0 != posix.Bind(m_fd, true))
		return HandleError(errno);

#ifdef POSIX_OK_LOG
	if (Log(VERBOSE, "%010p: bound %d %scast UDP", this, m_fd,
			how == MULTICAST ? "multi" : how == UNICAST ? "uni" :
			how == BROADCAST ? "broad" : "mystery-"))
	{
		OpString8 where;
		if (OpStatus::IsSuccess(posix.ToString8(&where)))
			Log(VERBOSE, " to %s", where.CStr());
		Log(VERBOSE, "\n");
	}
#endif

	return Listen();
}

OP_NETWORK_STATUS PosixUdpSocket::Send(const void* data, unsigned int length,
									   OpSocketAddress* there,
									   RoutingScheme how /* = UNICAST */)
{
	PosixNetworkAddress posix;
	RETURN_IF_ERROR(posix.Copy(there));
	RETURN_IF_ERROR(PrepareSocketHandle(&posix, how));

	int broadcast = how == BROADCAST;
	// Should we bother to reset this after sending?
	if (setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1)
		return HandleError(errno);

	ssize_t sent = posix.SendTo(m_fd, data, length);
	if (sent < 0)
		return HandleError(errno);

#ifdef POSIX_OK_LOG
	if (Log(SPEW, "%010p: sent %d bytes on %d %scast UDP", this, sent, m_fd,
			how == MULTICAST ? "multi" : how == UNICAST ? "uni" :
			how == BROADCAST ? "broad" : "mystery-"))
	{
		OpString8 where;
		if (OpStatus::IsSuccess(posix.ToString8(&where)))
			Log(SPEW, " to %s", where.CStr());
		Log(SPEW, "\n");
	}
#endif

	/* Given that PosixBaseSocket::SetSocketFlags() set O_NONBLOCK, POSIX
	 * documents sendto() as failing if there isn't space available on the
	 * sending socket to hold the message; so we should either have sent all of
	 * the data or failed. */
	OP_ASSERT((unsigned int) sent == length);
	return m_bound ? OpStatus::OK : Listen();
}

OP_NETWORK_STATUS PosixUdpSocket::Receive(void* buffer, unsigned int length,
										  OpSocketAddress* there,
										  unsigned int* bytes_received)
{
	if (!m_bound)
		/* Socket hasn't been bound.  You're not going to receive anything. */
		return OpStatus::ERR;

	struct sockaddr_storage sa;
	socklen_t sz = sizeof(sa);
	errno = 0;
	ssize_t received = recvfrom(m_fd, buffer, length, 0, reinterpret_cast<struct sockaddr*>(&sa), &sz);

	if (received < 0)
		return HandleError(errno);

	PosixNetworkAddress posix;
	RETURN_IF_ERROR(posix.SetFromSockAddr(sa, sz));
#ifdef POSIX_SUPPORT_IPV6
	if (posix.IsUsableIPv6())
		g_opera->posix_module.SetIPv6Usable(true);
#endif
	if (there)
		RETURN_IF_ERROR(there->Copy(&posix));

#ifdef POSIX_OK_LOG
	if (Log(SPEW, "%010p: received %d bytes on %d UDP", this, received, m_fd))
	{
		OpString8 where;
		if (OpStatus::IsSuccess(posix.ToString8(&where)))
			Log(SPEW, " from %s", where.CStr());
		Log(SPEW, "\n");
	}
#endif
	if (bytes_received)
		*bytes_received = received;

	return OpStatus::OK;
}

// static
OP_NETWORK_STATUS PosixUdpSocket::HandleError(int err)
{
	switch (err)
	{
	case EWOULDBLOCK:
	case EINPROGRESS:
		return OpNetworkStatus::ERR_SOCKET_BLOCKING;

	case EAFNOSUPPORT:
		return OpStatus::ERR_NOT_SUPPORTED;

	case EHOSTUNREACH:
	case EMSGSIZE:
	case ENETDOWN:
	case ENETUNREACH:
		return OpNetworkStatus::ERR_NETWORK;

	case EACCES:
		return OpNetworkStatus::ERR_ACCESS_DENIED;

	case EADDRINUSE:
		return OpNetworkStatus::ERR_ADDRESS_IN_USE;

	case ENOMEM:
	case ENOBUFS:
		return OpStatus::ERR_NO_MEMORY;

	case EINTR:
	case EINVAL:
	case EFAULT:
	case EADDRNOTAVAIL:
	case EBADF:
		return OpStatus::ERR;

	default:
		return OpStatus::ERR;
	}
}

#endif // POSIX_OK_UDP
