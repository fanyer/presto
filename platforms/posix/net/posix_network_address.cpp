/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_NETADDR

#include "platforms/posix/net/posix_network_address.h"
#ifdef POSIX_UDP_BROADCAST_NOBIND
#include "modules/pi/OpSystemInfo.h"
#endif

#if defined(POSIX_OK_LOG) || (defined(POSIX_OK_SYS) && defined(OPSYSTEMINFO_GETSYSTEMIP))
OP_STATUS PosixNetworkAddress::ToString8(OpString8 *result) const
{
	if (m_as_string.HasContent())
		return result->Set(m_as_string);

	RETURN_IF_ERROR(ComputeAsString(*result));
	OP_ASSERT(result->HasContent());

	if (OpStatus::IsError(m_as_string.Set(*result)))
		m_as_string.Empty(); // non-fatal.

	return OpStatus::OK;
}
#endif // LOG or (SYS && GETSYSTEMIP)

#undef ZERO_FILL
#define ZERO_FILL(s) op_memset(&(s), 0, sizeof(s))

#ifdef POSIX_TO_STRUCT_SOCKADDR
int PosixNetworkAddress::AsSockAddrS(struct sockaddr_storage &sa, size_t &len, bool slack) const
{
	ZERO_FILL(sa);
	switch (m_family)
	{
	case UnknownFamily:
		if (!slack)
			return EINVAL;
		// else fall through:
	case IPv4: {
		len = sizeof(sockaddr_in);
		sockaddr_in &sa4 = *reinterpret_cast<sockaddr_in*>(&sa);
		sa4.sin_family = AF_INET;
		sa4.sin_port = op_htons(GetPort());
		if (m_family == UnknownFamily)
			sa4.sin_addr.s_addr = INADDR_ANY;
		else
			sa4.sin_addr = m_addr.a4;
	}	break;
#ifdef POSIX_SUPPORT_IPV6
	case IPv6: {
		len = sizeof(sockaddr_in6);
		sockaddr_in6 &sa6 = *reinterpret_cast<sockaddr_in6*>(&sa);
		sa6.sin6_family = AF_INET6;
		sa6.sin6_port = op_htons(GetPort());
		sa6.sin6_addr = m_addr.a6;
	}	break;
#endif
	default: return EINVAL;
	}
	return 0;
}
#endif // POSIX_TO_STRUCT_SOCKADDR

#if defined(POSIX_OK_SOCKET) || defined(POSIX_OK_UDP)
int PosixNetworkAddress::Bind(int fd, bool slack) const
{
	size_t len;
	struct sockaddr_storage sa;
	errno = AsSockAddrS(sa, len, slack);
	if (errno)
		return -1;

	return bind(fd, reinterpret_cast<struct sockaddr*>(&sa), len);
}
#endif // POSIX_OK_SOCKET

#if defined(POSIX_OK_NETWORK) && defined(POSIX_SUPPORT_IPV6)
bool PosixNetworkAddress::IsUnicast() const
{
	if (m_family == IPv6)
	{
		UINT8 first = m_addr.a6.s6_addr[0];
		if (first == 0xfe) // fe00::/8
			switch (m_addr.a6.s6_addr[1] >> 6) // fe?0::/10
			{
			case 0xc0 >> 6: return true; // fec0::/10, site-local unicast
			case 0x80 >> 6: // fe80::/10
#ifdef DEBUG_ENABLE_OPASSERT
			{
				/* Assuming that fe80::/10 == fe80::/64.
				 *
				 * All of fe80::/10 is reserved for link-local uses, with only
				 * fe80::/64 having a defined meaning, unicast.  No other
				 * addresses in this range should appear, so we shouldn't need
				 * to check; every fe80::/10 should be an fe80::/64, at least
				 * for the present (as at 2011/Feb).
				 *
				 * When this assert triggers, it's time to go and find what RFC
				 * has given meaning to some more ranges within fe80::/10 and
				 * refine this function.  For now, we tacitly assume they're
				 * unicast.
				 */
				bool subnet = m_addr.a6.s6_addr[1] & 0x37;
				for (int i = 8; !subnet && i-- > 2; )
					subnet = m_addr.a6.s6_addr[i];

				if (subnet)
					OP_ASSERT(!"Address is in (possibly) non-unicast part of link-local range");
			}
#endif
				return true; // fe80::/64, link-local unicast
		}
		else if (0 == ((first ^ 0xfc) >> 1)) // fc00::/7, unique local unicast
			return true;
	}

	return false;
}
#endif // POSIX_OK_NETWORK && POSIX_SUPPORT_IPV6

#ifdef POSIX_OK_UDP
OP_STATUS PosixNetworkAddress::Convert(OpSocketAddress* from, bool broadcast)
{
	RETURN_IF_ERROR(Import(from));
#ifdef POSIX_UDP_BROADCAST_NOBIND // TWEAK_POSIX_UDP_BCAST_NOBIND
	if (broadcast && m_family == IPv4 &&
		m_addr.a4.s_addr == INADDR_BROADCAST)
	{
		/* TODO: FIXME: need proper broadcast address for an interface.
		 *
		 * Using GetSystemIp() isn't really suitable; and simply changing the
		 * last byte of the address doesn't necessarily give a broadcast
		 * address, even if it does in the cases we've tested ...
		 *
		 * The network manager can sensibly obtain broadcast addresses, at least
		 * from some of its system queries, per interface.
		 */
		OpString system;
		RETURN_IF_ERROR(g_op_system_info->GetSystemIp(system));
		RETURN_IF_ERROR(FromString(system.CStr()));
		reinterpret_cast<UINT8*>(&m_addr.a4)[3] = 0xff;
		RETURN_IF_ERROR(from->Copy(this));
	}
#endif // POSIX_UDP_BROADCAST_NOBIND
	return OpStatus::OK;
}

int PosixNetworkAddress::JoinMulticast(int fd, const PosixNetworkAddress *local) const
{
	/* See man 7 ip on Linux. */
	errno = 0;
	if (local && local->m_family == UnknownFamily) local = 0; // ignore if unset
#ifdef POSIX_OK_NETIF // i.e. we can look up an interface index.
# if defined(POSIX_SUPPORT_IPV6) && defined(IPPROTO_IPV6) && defined(IPV6_JOIN_GROUP)
	if (m_family == IPv6)
	{
		struct ipv6_mreq mreq; ZERO_FILL(mreq);
		mreq.ipv6mr_multiaddr = m_addr.a6;
		mreq.ipv6mr_interface = local ? local->IfIndex() : 0;
		return setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq));
		/* RFC 2553 also specifies an IPV6_MULTICAST_IF with similar purpose to
		 * IP_MULTICAST_IF (see below), taking the interface index instead of
		 * address; but indicates that setting this flag is not necessary. */
	}
# endif // IPv6 supported as specified in RFC 2553, section 5.2.
#endif // POSIX_OK_NETIF

#ifdef POSIX_USE_MREQN // implies POSIX_OK_NETIF, so index *is* declared.
	ip_mreqn mreq; ZERO_FILL(mreq);
	mreq.imr_ifindex = local ? local->IfIndex() : 0;
#else
	ip_mreq mreq; ZERO_FILL(mreq);
#endif

	// Set IP multicast address of group:
	switch (m_family)
	{
	case UnknownFamily: mreq.imr_multiaddr.s_addr = INADDR_ANY; break;
	case IPv4:          mreq.imr_multiaddr = m_addr.a4;         break;
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	// Set local IP address of interface:
	switch (local ? local->m_family : UnknownFamily)
	{
	case UnknownFamily: mreq.imr_interface.s_addr = INADDR_ANY; break;
	case IPv4:          mreq.imr_interface = local->m_addr.a4;  break;
#ifdef POSIX_SUPPORT_IPV6
	case IPv6: // Not supported by IP_ADD_MEMBERSHIP
		OP_ASSERT(local);
# ifdef POSIX_USE_MREQN
		/* TODO: is it sufficient to set the index ?
		 *
		 * If not, setting INADDR_ANY probably isn't much of an improvement,
		 * but it's the sanest thing I can think to do in case setting index
		 * does suffice. */
		if (mreq.imr_ifindex)
		{
			mreq.imr_interface.s_addr = INADDR_ANY;
			break;
		}
# endif // POSIX_OK_NETIF
		// Deliberate fall-through:
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	int res = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	if (res < 0 || !local)
		return res;

	/* The following flies in the face of the man 7 ip documentation, which
	 * claims IP_MULTICAST_IF wants mreq as parameter, as for IP_ADD_MEMBERSHIP,
	 * but experiment supports passing just a struct in_addr and we prefer
	 * software that works over software that does what's documented, when these
	 * differ.  Eddy (and Anders), 2010/Feb/1st. */
	return setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
					  &(mreq.imr_interface), sizeof(mreq.imr_interface));
}

int PosixNetworkAddress::SendTo(int fd,
								const void * data, unsigned int length,
								int flags) const
{
	size_t len;
	struct sockaddr_storage sa;
	errno = AsSockAddrS(sa, len, true);
	if (errno)
		return -1;

	return sendto(fd, data, length, flags | DefaultSendFlags,
				  reinterpret_cast<struct sockaddr*>(&sa), len);
}
#endif // POSIX_OK_UDP

#ifdef POSIX_FROM_STRUCT_INADDR
OP_STATUS PosixNetworkAddress::SetFromSockAddr(const struct sockaddr_storage &sa, socklen_t sz)
{
	m_as_string.Empty();
	switch (sa.ss_family)
	{
	case AF_INET: {
		OP_ASSERT(sz > 0 && (size_t)sz >= sizeof(sockaddr_in));
		const sockaddr_in &sa4 = *reinterpret_cast<const sockaddr_in*>(&sa);
		RETURN_IF_ERROR(SetFromIPv(sa4.sin_addr));
		SetPort(op_ntohs(sa4.sin_port));
		m_family = IPv4;
	}   break;
#ifdef POSIX_SUPPORT_IPV6
	case AF_INET6: {
		OP_ASSERT(sz >= sizeof(sockaddr_in6));
		const sockaddr_in6 &sa6 = *reinterpret_cast<const sockaddr_in6*>(&sa);
		RETURN_IF_ERROR(SetFromIPv(sa6.sin6_addr));
		SetPort(op_ntohs(sa6.sin6_port));
		m_family = IPv6;
	} break;
#endif // POSIX_SUPPORT_IPV6
	default:
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}
#endif // POSIX_FROM_STRUCT_INADDR

#ifdef POSIX_OK_SELECT
int PosixNetworkAddress::Connect(int fd) const
{
	size_t len;
	struct sockaddr_storage sa;
	errno = AsSockAddrS(sa, len);
	if (errno)
		return -1;

	return connect(fd, reinterpret_cast<struct sockaddr*>(&sa), len);
}
#endif // POSIX_OK_SELECT

#endif // POSIX_OK_NETADDR
