/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_NETWORK_ADDRESS_H
#define POSIX_NETWORK_ADDRESS_H
# ifdef POSIX_OK_NETADDR
#include "platforms/posix/net/posix_socket_address.h"

/** Extend PosixSocketAddress for internal use.
 *
 * Contains special hooks for other classes of, or associated with, this module;
 * provided to avoid having to clutter PosixSocketAddress with too many friends
 * and to keep its API as close to OpSocketAddress as practical.
 *
 * Most of the methods here are wrappers round standard system functions which
 * take a struct sockaddr and its size; saving the address in this form
 * internally is wasteful of space, so these methods take the other arguments
 * and set up a struct sockaddr transiently for the system call.  These methods
 * all simply return the same as the system function they package, setting errno
 * suitably in the process.
 */
class PosixNetworkAddress : public PosixSocketAddress
{
public:
	PosixNetworkAddress() : PosixSocketAddress() {}

#if defined(POSIX_OK_LOG) || (defined(POSIX_OK_SYS) && defined(OPSYSTEMINFO_GETSYSTEMIP))
	/** Returns a plain ASCII representation of the address (without port).
	 *
	 * Caches result (if caching doesn't OOM) so as to make repeated calls
	 * (e.g. for logging) cheap.  The returned string is computed first; OOM is
	 * only reported if it happens during this - OOM during caching the result
	 * doesn't interfere with the return; the result can be cached on a later
	 * call.
	 *
	 * @param result OpString8 to be set to a representation of the address.
	 * @return Status: ERR_NO_MEMORY on OOM, ERR on other errors, else OK.
	 */
	OP_STATUS ToString8(OpString8 *result) const;
#endif

#ifdef POSIX_OK_SELECT
	/** Connect an open socket to this address.
	 *
	 * Packages the system connect() call.
	 *
	 * @param fd The file descriptor of the socket.
	 * @return 0 on success; else -1, in which case errno has been set.
	 */
	int Connect(int fd) const;
#define POSIX_TO_STRUCT_SOCKADDR
#endif // SELECT (for, e.g., SOCKET)

#ifdef POSIX_OK_UDP
# ifdef POSIX_OK_NETIF
private:
	/** The interface index of this local address, else zero.
	 *
	 * When selecting a local interface to use for UDP connections, some system
	 * functions require the local interface index for the interface, not the
	 * network address.  This method consults the global interface manager to
	 * deduce an interface index from its address.  If the address is not
	 * associated with any local interface, an interface index of 0 is returned;
	 * this is understood by relevant system calls as the default interface.
	 *
	 * @return Index of this local address as an interface, if it is one, else 0.
	 */
	unsigned int IfIndex() const; // implemented in posix_interface.cpp
#define POSIX_NET_ISSAME // needed by IfIndex's implementation

public:
# endif // POSIX_OK_NETIF

	/** Extend Copy to handle a special case.
	 *
	 * See PosixUdpSocket::Bind() for the sole caller.
	 *
	 * @param from The address to copy or over-ride.
	 * @param broadcast If false, the behaviour is exactly as Import (q.v.).
	 * Set true when preparing an address to which to bind for UDP broadcast; if
	 * the source address, from, isn't suitable, it will be @em modified to one
	 * that (hopefully) is, to which this shall also be set.
	 * @return See OpStatus; as for OpSocketAddress::Copy().
	 */
	OP_STATUS Convert(OpSocketAddress* from, bool broadcast);

	/** Add fd to a multicast group.
	 *
	 * Packages calls to setsockopt(, IPPROTO_IP, IP_MULTICAST_ADD,,) and (when
	 * relevant) setsockopt(, IPPROTO_IP, IP_MULTICAST_IF,,) with this as the IP
	 * multicast group address.  A second PosixNetworkAddress may be supplied to
	 * specify the local interface; if NULL (or invalid), a system default
	 * interface shall be used.
	 *
	 * @param fd The file descriptor of an open socket.
	 * @param local Local interface address; NULL to use INADDR_ANY.
	 * @return 0 on success; else -1, in which case errno has been set.
	 */
	int JoinMulticast(int fd, const PosixNetworkAddress *local) const;

	/** Send data to a remote address.
	 *
	 * Packages a call to sendto(fd, data, length, flags,,).
	 * If ! IsValid(), fall back on INADDR_ANY as address.
	 *
	 * @param fd The file descriptor of an open socket.
	 * @param data Pointer to the start of the data to be sent.
	 * @param length Number of bytes to send.
	 * @param flags Optional flags; see sendto() documentation (default is 0),
	 * but note that the value passed to sendto() is flags | DefaultSendFlags.
	 * @return 0 on success; else -1, in which case errno has been set.
	 */
	int SendTo(int fd, const void * data, unsigned int length, int flags = 0) const;

#define POSIX_FROM_STRUCT_INADDR
#define POSIX_TO_STRUCT_SOCKADDR
#endif // POSIX_OK_UDP

#ifdef POSIX_OK_NETWORK
#define POSIX_NET_ISSAME
#endif

#ifdef POSIX_NET_ISSAME
	/** Like IsEqual, only with the const qualifications that should have ...
	 *
	 * @param other Another posix socket address.
	 * @return True precisely if other and this describe the same address.
	 */
	bool IsSame(const PosixNetworkAddress *other) const
	{
		if (GetPort() != other->GetPort() ||
			m_family != other->m_family)
			return false;

		switch (m_family)
		{
		case IPv4: return m_addr.a4.s_addr == other->m_addr.a4.s_addr;
#ifdef POSIX_SUPPORT_IPV6
		case IPv6: return op_memcmp(&m_addr.a6, &other->m_addr.a6, sizeof(ipv6_addr)) == 0;
#endif
		}
		return true; // both invalid
	}
#endif // POSIX_NET_ISSAME

#ifdef POSIX_SUPPORT_IPV6
	/** Is this a usable IPv6 address ?
	 *
	 * PosixHostResolver needs to know when to list available IPv6 (destination)
	 * addresses before IPv4 ones; this method mediates the (heuristic) check on
	 * each (outgoing) interface for whether it constitutes a usable route to
	 * remote IPv6.
	 */
	bool IsUsableIPv6() const
	{
		/* TODO (DSK-269385 comments): TommyO says we need to *not* treat
		 * 2002::/16 and 2001::/32 as "public" for these purposes. */
		if (m_family != IPv6 || GetNetworkScope() != NETTYPE_PUBLIC)
			return false;

		if (m_addr.a6.s6_addr[0] == 0x20) //
			/* DSK-269385: some public IPv6 addresses are less usable than others. */
			switch (m_addr.a6.s6_addr[1])
			{
			case 2: return false; // 2002::/16 (6to4)
			case 1: return m_addr.a6.s6_addr[2] || m_addr.a6.s6_addr[3]; // 2001::/32 (Teredo)
			default: break;
			}
		return true;
	}
#endif // POSIX_SUPPORT_IPV6

#if defined(POSIX_OK_NETWORK) && defined(POSIX_SUPPORT_IPV6)
	/** Test whether this address is in a unicast-only range.
	 *
	 * Addresses in the IPv6 fec0::/10 "site-local unicast" and fe80::/64
	 * "link-local unicast" ranges aren't usable for multicast and broadcast.
	 * For now (2009/Jul/14), Eddy assumes the fc00::/7 "unique local unicast"
	 * range (which is also private) suffers the same problems.  Clients of the
	 * OpNetworkInterfaceManager tend to use addresses for these purposes, so it
	 * needs to know to filter these ranges out.  They're of debatable utility
	 * in any case ...
	 *
	 * @return True precisely if this address is in one of the unicast-only ranges.
	 */
	bool IsUnicast() const;
#endif // POSIX_OK_NETWORK && POSIX_SUPPORT_IPV6

#if defined(POSIX_OK_SOCKET) || defined(POSIX_OK_UDP) || defined(POSIX_OK_NETIF)
	/** \brief DefaultSendFlags used to suppress misbehaviour in debugging.
	 *
	 * All calls to send() (see PosixSocket::Send) and sendto() (see SendTo) in
	 * this module combine their given flags with | DefaultSendFlags to get the
	 * flags they pass down to send() or sendto(); this (see CORE-20577) makes
	 * debugging cope more gracefully when remote servers misbehave: instead of
	 * raising SIGPIPE if the remote end closes the connection before
	 * completion, the call to send() or sendto() shall merely return EPIPE.
	 * Platforms lacking MSG_NOSIGNAL and MSG_NOPIPE shall be left to raise
	 * signals as normal. */
	enum
	{
		DefaultSendFlags
#ifdef MSG_NOSIGNAL // linux: <linux/socket.h> or <bits/socket.h>
		= MSG_NOSIGNAL
#elif defined(MSG_NOPIPE)
		= MSG_NOPIPE
#else
		= 0
#endif
	};

	int GetDomain() const
	{
		return
#ifdef POSIX_SUPPORT_IPV6
			m_family == IPv6 ? PF_INET6 :
#endif
			PF_INET;
	}

	/** Bind an open socket to this address.
	 *
	 * Packages the system bind() call.
	 *
	 * @param fd The file descriptor of the socket.
	 * @param slack False (default) to only bind if address is set, else true to
	 * bind to INADDR_ANY if unset.
	 * @return 0 on success; else -1, in which case errno has been set.
	 */
	int Bind(int fd, bool slack=false) const;

#define POSIX_TO_STRUCT_SOCKADDR
#endif // SOCKET || UDP || NETIF

#if defined(SOCKET_LISTEN_SUPPORT) || defined(OPSOCKET_GETLOCALSOCKETADDR) || defined(POSIX_OK_DNS)
#define POSIX_FROM_STRUCT_INADDR
#endif

#ifdef POSIX_FROM_STRUCT_INADDR
	/** Set host data from a struct sockaddr and its size.
	 *
	 * @param sa The socket address structure.
	 * @param sz Its size.
	 * @return See OpStatus: OK on success, else a suitable error code.
	 */
	OP_STATUS SetFromSockAddr(const struct sockaddr_storage &sa, socklen_t sz);

	/** Set host data from ipv4_addr or ipv6_addr.
	 *
	 * @param datum System datum describing the host.
	 * @return OpStatus::OK.
	 */
	OP_STATUS SetFromIPv(const ipv4_addr &datum)
	{
		m_as_string.Empty();
		m_addr.a4 = datum;
		m_family = IPv4;
		return CheckSpecified();
	}
# ifdef POSIX_SUPPORT_IPV6
	/**
	 * @overload
	 */
	OP_STATUS SetFromIPv(const ipv6_addr &datum)
	{
		m_as_string.Empty();
		m_addr.a6 = datum;
		m_family = IPv6;
		return CheckSpecified();
	}
# endif
#endif // POSIX_FROM_STRUCT_INADDR

#ifdef POSIX_TO_STRUCT_SOCKADDR // see above; support for Connect and Bind
private:
	/** Express as a suitable socket address structure.
	 *
	 * @param sa A struct sockaddr_storage to be filled in.
	 * @param len The length of the actual sockaddr variant used.
	 * @param slack Fall back on INADDR_ANY if m_family is Unset (default: false).
	 * @return An errno value on error, else 0.
	 */
	int AsSockAddrS(struct sockaddr_storage &sa, size_t &len, bool slack=false) const;
#endif // POSIX_TO_STRUCT_SOCKADDR
};

# endif // POSIX_OK_NETADDR
#endif // POSIX_NETWORK_ADDRESS_H
