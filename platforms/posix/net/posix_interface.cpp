/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_NETIF
#include "platforms/posix/net/posix_interface.h"
#include <net/if.h>

#ifdef POSIX_USE_LIFREQ_IOCTL
#include <sys/sockio.h>
#define USING_SOCKET_IOCTL
#endif // POSIX_USE_LIFREQ_IOCTL

#ifdef POSIX_USE_IFREQ_IOCTL
#include <sys/ioctl.h>
#define USING_SOCKET_IOCTL
#endif // POSIX_USE_IFREQ_IOCTL

#ifdef POSIX_USE_GETIFADDRS
#include <ifaddrs.h>
#endif

#ifdef POSIX_USE_NETLINK
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#ifdef POSIX_SUPPORT_IPV6 // convenience query:
static bool IsIPv6OK() { return g_opera && g_opera->posix_module.GetIPv6Support(); }
#endif
/* Control whether Enumerate() might SetIPv6Usable().
 *
 * If we don't have a way of discovering any IPv6 networks locally, Enumerate()
 * should not presume that the failure to find a usable one means we can't use
 * IPv6 - we may well have an interface with an IPv6 address we can't see, that
 * we could be using to access remote IPv6 sites.  So each stanza implementing a
 * portion of Enumerate should define POSIX_NETIF_PROBES_IPv6 precisely if its
 * way of finding network addresses would in fact discover any IPv6 addresses.
 * This then enables ProxyStore and Enumerate()'s check for whether we got a
 * sane IPv6 address.  See discussion on ANDMO-264.
 */
#undef POSIX_NETIF_PROBES_IPv6

/* Tweakify this if struct ifreq ever gets fixed.
 *
 * The known struct ifreq implementations use a struct sockaddr member (contrast
 * getifaddrs, which uses a struct sockaddr*, which may well point to a struct
 * sockaddr_storage) to hold their address; this is not big enough to hold an
 * IPv6 address, so we can't possibly get useful results out of the APIs that
 * return address information in this format (see code testing this define,
 * below).  If its .ifr_addr ever gets upgraded to a struct sockaddr_storage, we
 * might be able to get IPv6 out of these interfaces; until then, no such luck.
 *
 * Of course, if such a change happens, it's entirely possible it'll come with a
 * change to the ioctl in some way, meaning more code shall need changed to make
 * the most of it.  I've left (Eddy/2011/Sept) assertions in the code that might
 * have a chance of noticing we have the option, so we'll notice when it's time
 * to research those details.
 */
#undef POSIX_IFREQ_CAN_HOLD_IPv6

#if defined(OPSYSTEMINFO_GETSYSTEMIP) && defined(POSIX_OK_SYS)
#include "modules/pi/network/OpHostResolver.h"
#include "platforms/posix/posix_system_info.h"
#include "modules/url/url_socket_wrapper.h"

/** Listener for QueryLocalIP.
 *
 * Only used if all other ways of discovering local IP address have failed; the
 * last fall-back is an asynchronous DNS look-up.  The listener for that needs
 * to know where to save the answer and must be able to take responsibility for
 * deleting both itself and the host-resolver it serviced, if the latter
 * responds to it asynchronously (i.e. after QueryLocalIP() has returned).
 */
class FallbackDNSListener : public OpHostResolverListener
{
	OpString8 *const m_sys_ip;
	const OpString8 *const m_host_name;
	bool m_async;
	bool m_done;
	void Done(OpHostResolver* dns);
public:
	FallbackDNSListener(OpString8 *sys_ip, const OpString8 *host)
		: m_sys_ip(sys_ip), m_host_name(host), m_async(false), m_done(false) {}
	bool Detach() { m_async = true; return m_done; } // see comment in QueryLocalIP
	virtual void OnHostResolved(OpHostResolver* dns);
	virtual void OnHostResolverError(OpHostResolver* dns, OpHostResolver::Error error);
};

void FallbackDNSListener::Done(OpHostResolver* dns)
{
	m_done = true;
	if (m_async)
	{
		delete dns;
		delete this;
	}
}

void FallbackDNSListener::OnHostResolverError(OpHostResolver* dns,
											  OpHostResolver::Error error)
{
	PosixLogger::Log(PosixLogger::DNS, PosixLogger::TERSE,
					 "Failed (%d) synchronous DNS for local %s",
					 (int)error, m_host_name->CStr());

	Done(dns);
}

/**
 * Address object that remembers the nicest interface it's been asked to Notice().
 *
 * TODO: FIXME: conform to RFC 3484 !
 * http://www.rfc-editor.org/rfc/rfc3484.txt
 */
class NiceAddress : public PosixNetworkAddress
{
	bool m_up;
public:
	NiceAddress() : PosixNetworkAddress(), m_up(false) {}
	OP_STATUS Notice(const PosixNetworkAddress *what, bool up)
	{
		const enum OpSocketAddressNetType type = GetNetType();
		if (type == NETTYPE_UNDETERMINED ||
			(m_up == up ? type < what->GetNetType() : up))
		{
			RETURN_IF_ERROR(Import(what));
			m_up = up;
		}

		return OpStatus::OK;
	}
	bool IsUp() { return m_up; }
};

void FallbackDNSListener::OnHostResolved(OpHostResolver* dns)
{
	UINT index = dns->GetAddressCount();
	if (index > 0)
	{
		PosixLogger::Log(PosixLogger::DNS, PosixLogger::CHATTY,
						 "Successfull DNS lookup[%d] for local %s",
						 dns->GetAddressCount(), m_host_name->CStr());

		NiceAddress best4;
#ifdef POSIX_SUPPORT_IPV6
		NiceAddress best6;
		bool usable = false;
#endif
		while (index-- > 0)
		{
			// Extract a result into carrier, get best4 or best6 to Notice it:
			PosixNetworkAddress carrier;
			if (OpStatus::IsSuccess(dns->GetAddress(&carrier, index)) &&
				/* karlo/2009/May/25th witnessed DNS returning INADDR_ANY as a
				 * local address (maybe multicast-related ?), so filter that out
				 * if we ever see it again: */
				carrier.IsValid())
			{
				/* We don't actually know whether they're up or down, but claim
				 * they're up, anyway: it doesn't matter, as we claim the same
				 * for every one of them. */
#ifdef POSIX_SUPPORT_IPV6
				if (carrier.GetDomain() == PF_INET6)
				{
					best6.Notice(&carrier, true);
					if (carrier.IsUsableIPv6())
						usable = true;
				}
				else
#endif
					best4.Notice(&carrier, true);
			}
		}

#ifdef POSIX_SUPPORT_IPV6
		if (best6.IsValid() && IsIPv6OK())
			g_opera->posix_module.SetIPv6Usable(usable);
		else
			usable = false; // to save repeating test expressions

		if (usable && OpStatus::IsSuccess(best6.Notice(&best4, best4.IsUp())))
		{
			best6.ToString8(m_sys_ip);
		}
		else
#endif
		if (best4.IsValid())
			best4.ToString8(m_sys_ip);
		else
			PosixLogger::Log(PosixLogger::DNS, PosixLogger::TERSE,
							 "Seemingly successful DNS lookup found "
							 "%d invalid records for local %s",
							 dns->GetAddressCount(), m_host_name->CStr());
	}
	else
		PosixLogger::Log(PosixLogger::DNS, PosixLogger::TERSE,
						 "Seemingly successful DNS lookup found no records for local %s",
						 m_host_name->CStr());

	Done(dns);
}

class BestLocalIP : public PosixNetLookup::Store
{
	NiceAddress m_best4;
#ifdef POSIX_SUPPORT_IPV6
	NiceAddress m_best6;
#endif

public:
	virtual OP_STATUS AddPosixNetIF(const PosixNetworkAddress *what,
									const char *, unsigned int, // ignored
									bool up)
	{
		/* Just remember the best we've seen, for IPv4 and IPv6 respectively,
		 * preferring up over down, public over private over local; and assuming
		 * m_best is indeterminate only if (as it was initially) invalid.
		 * Forget all others: we only want one of each type. */
#ifdef POSIX_SUPPORT_IPV6
		if (what->GetDomain() == PF_INET6)
			return m_best6.Notice(what, up);
#endif // POSIX_SUPPORT_IPV6
		return m_best4.Notice(what, up);
	}
	// OpSocketAddress base-class doesn't const-qualify IsValid, GetNetType or ToString :-(
	bool Happy() /* const */
	{
#ifdef POSIX_SUPPORT_IPV6
		if (m_best6.IsValid() && g_opera && g_opera->posix_module.GetIPv6Usable())
			return true;
#endif
		return m_best4.IsValid();
	}
	OP_STATUS Save(OpString8 *into) /* const */
	{
#ifdef POSIX_SUPPORT_IPV6
		if (g_opera && g_opera->posix_module.GetIPv6Usable() &&
			// Use best v4 address if better than v6 address:
			OpStatus::IsSuccess(m_best6.Notice(&m_best4, m_best4.IsUp())))
			return m_best6.ToString8(into);
#endif // POSIX_SUPPORT_IPV6
		return m_best4.ToString8(into);
	}
};

OP_STATUS PosixSystemInfo::QueryLocalIP()
{
	// Assumed by assorted socketry code
	OP_ASSERT(sizeof(struct sockaddr_storage) >= sizeof(sockaddr_in6));

	{ // Narrow scope of tool:
		BestLocalIP tool;
		RETURN_IF_ERROR(PosixNetLookup::Enumerate(&tool, SOCK_STREAM));
		if (tool.Happy())
			return tool.Save(&m_sys_ip);
	}

	/* Fallback.
	 *
	 * First set to 127.0.0.1, then ask DNS if it can help us out - which it
	 * quite likely can't, for example if the available DNS server doesn't
	 * actually know the local machine by the hostname it believes in; this is
	 * probably common among ISP customers (i.e. real users).  If DNS gets a
	 * useful result, it'll substitute that in place of 127.0.0.1.
	 *
	 * Note that a DNS look-up on localhost (or its FQDN) is not only slow but
	 * likely to produce poor or unusable results, at least when the
	 * host-resolver in use is getaddrinfo(): see
	 * http://klickverbot.at/blog/2012/01/getaddrinfo-edge-case-behavior-on-windows-linux-and-osx/
	 * for details.  (Short version: "localhost" only gets local loop-back
	 * addresses; Linux does the same for FQDN; Windows and Mac return all
	 * public interfaces for the FQDN; but the local name (FQDN without domain)
	 * doesn't resolve on Mac.  We typically only have the local name or
	 * "localhost" from uname or gethostname.  See CORE-44333.)  So we only do
	 * this as a last resort, when all other methods of discovering the local
	 * address have failed.
	 */
	RETURN_IF_ERROR(m_sys_ip.Set("127.0.0.1"));
	if (m_host_name.HasContent())
	{
#ifdef POSIX_OK_ASYNC
		// Ensure PosixAsyncManager is initialized (host resolver needs it):
		RETURN_IF_ERROR(g_opera->posix_module.InitAsync());
#endif // POSIX_OK_ASYNC
		FallbackDNSListener *ear = OP_NEW(FallbackDNSListener, (&m_sys_ip, &m_host_name));
		if (!ear)
			return OpStatus::ERR_NO_MEMORY;

		OpHostResolver *solve = NULL;
		OpString host;
		OP_STATUS res = SocketWrapper::CreateHostResolver(&solve, ear);
		if (OpStatus::IsSuccess(res) &&
			OpStatus::IsSuccess(res = host.Set(m_host_name)))
			res = solve->Resolve(host.CStr());

		/* Resolve *might* already have called OnHostResolved, in which case
		 * we're done with both ear and and solve; more likely, however, it
		 * won't do that for a while; so we have to let ear know it's going to
		 * be responsible for tidying itself and solve away.  Fortunately, one
		 * method call can both ask whether it's finished and tell it to tidy up
		 * if not.  It is presumed that, if Resolve() returned an error, solve
		 * shall not be calling ear's methods later. */
		if (OpStatus::IsError(res) || ear->Detach())
		{
			delete solve;
			delete ear;
		}

		return res;
	}
	return OpStatus::OK;
}
#endif // OPSYSTEMINFO_GETSYSTEMIP && POSIX_OK_SYS


#ifdef POSIX_OK_UDP // only depends on this because otherwise it's unused
class EqualLocalIP : public PosixNetLookup::Store
{
	const PosixNetworkAddress *const m_sought;
	unsigned int m_index;
public:
	EqualLocalIP(const PosixNetworkAddress *sought) : m_sought(sought), m_index(0) {}
	virtual OP_STATUS AddPosixNetIF(const PosixNetworkAddress *what,
									const char *, // ignored
									unsigned int index,
									bool) // ignored
	{
		if (m_sought->IsSame(what))
		{
			m_index = index;
			// Use out-of-range to signal "we're done"
			return OpStatus::ERR_OUT_OF_RANGE;
		}
		return OpStatus::OK;
	}
	unsigned int Index() const { return m_index; }
};

unsigned int PosixNetworkAddress::IfIndex() const
{
	EqualLocalIP tool(this);
	/* Enumerate shall return ERR_OUT_OF_RANGE on success, which we ignore; OK
	 * means nothing useful found, ERR means no lookup possible, ERR_NO_MEMORY
	 * has its usual meaning.  However, whichever way it fails, we have no
	 * practical use to an error here, so just returning 0 is as good as we can
	 * do.  So just ignore the error.
	 */
	OpStatus::Ignore(PosixNetLookup::Enumerate(&tool, SOCK_DGRAM));
	return tool.Index();
}
#endif // POSIX_OK_UDP

/**
 * Tool class used to ensure socket tidy-up in functions below.
 */
class TransientSocket
{
	const int m_sock;
public:
	TransientSocket(int family, int type, int protocol = 0) : m_sock(socket(family, type, protocol)) {}
	~TransientSocket() { close(m_sock); }

	OP_STATUS check() const { return m_sock + 1 ? OpStatus::OK : OpStatus::ERR; }
	int get() const { return m_sock; }
};

/* Type-safe packaging of some hairy casting.
 *
 * We have to reinterpret_cast between the various struct sockaddr* variants;
 * but we can at least make sure we don't unwittingly convert from some other
 * type to one of them.  Inline functions do the typechecking macros can't.
 *
 * Some day, when IPv6 is more widely supported, we may see system types using
 * struct sockaddr_storage more widely, in which case we're likely to need new
 * variants on these to accept const pointers to those, too.
 *
 * Don't worry about unused-warnings on these.  Avoiding them would involve
 * monstrous and unmaintainable #if-ery; and they're too small to be a footprint
 * issue !
 */
static inline OP_STATUS StoreIPv4(PosixNetworkAddress &dst, const struct sockaddr* src)
{ return dst.SetFromIPv(reinterpret_cast<const struct sockaddr_in *>(src)->sin_addr); }
#ifdef POSIX_SUPPORT_IPV6
static inline OP_STATUS StoreIPv6(PosixNetworkAddress &dst, const struct sockaddr* src)
{ return dst.SetFromIPv(reinterpret_cast<const struct sockaddr_in6*>(src)->sin6_addr); }
#endif // POSIX_SUPPORT_IPV6

#ifdef POSIX_USE_GETIFADDRS
static OP_STATUS GetIFAddrs(PosixNetLookup::Store * carrier, struct ifaddrs* run) // <ifaddrs.h>
{
	for (unsigned int index = 1; run; run = run->ifa_next, index++)
		if (run->ifa_addr) // a struct sockaddr *; NULL if no actual address on interface
		{
			PosixNetworkAddress local;
			switch (run->ifa_addr->sa_family) // see <bits/socket.h> for values
			{
			case AF_INET: // 2
				RETURN_IF_ERROR(StoreIPv4(local, run->ifa_addr));
				break;
#ifdef POSIX_SUPPORT_IPV6
#define POSIX_NETIF_PROBES_IPv6
			case AF_INET6: // 10
				if (IsIPv6OK())
				{
					RETURN_IF_ERROR(StoreIPv6(local, run->ifa_addr));
					break;
				}
				// else: fall through.
#endif
			default: /* happens ! */ continue; // skip AddPosixNetIF
			}
			RETURN_IF_ERROR(carrier->AddPosixNetIF(&local, run->ifa_name, index,
												   // ifa_flags "as for SIOCGIFFLAGS ioctl":
												   (run->ifa_flags & IFF_UP) != 0));
		}
	return OpStatus::OK;
}
#endif // POSIX_USE_GETIFADDRS

#ifdef POSIX_USE_IFREQ_IOCTL
static OP_STATUS IfreqIoctl(PosixNetLookup::Store * carrier, int sock)
{
	struct ifreq here; // <net/if.h>
	/* AlexeyF, in bug 355225, DSK-232744, reports: "here.ifr_ifindex seems to
	 * change after SIOCGIFADDR" - indeed, it turns out all fields other than
	 * .ifr_name are actually different members of a union, which can only know
	 * one of them at a time, so we must record each before asking for the next.
	 * Each member name we access is actually #defined to an actual struct
	 * member name, a dot and a union member name.  Joy. */
	int index = 0;
	while ((here.ifr_ifindex = ++index), 1 + ioctl(sock, SIOCGIFNAME, &here))
		// http://docsun.cites.uiuc.edu/sun_docs/C/solaris_9/SUNWdev/NETPROTO/p40.html
		if (1 + ioctl(sock, SIOCGIFFLAGS, &here) &&
			(here.ifr_flags & IFF_LOOPBACK) == 0 &&
			(here.ifr_flags & (IFF_BROADCAST | IFF_POINTOPOINT)))
		{
			bool up = (here.ifr_flags & IFF_UP) != 0;
			if (1 + ioctl(sock, SIOCGIFADDR, &here))
			{
				PosixNetworkAddress addr;
				switch (here.ifr_addr.sa_family)
				{
				case AF_INET:
					RETURN_IF_ERROR(StoreIPv4(addr, &here.ifr_addr));
					break;
#ifdef POSIX_SUPPORT_IPV6
				case AF_INET6:
					// To the best of our knowledge, this never happens :-(
# ifdef POSIX_IFREQ_CAN_HOLD_IPv6 // See its documentation, above
#define POSIX_NETIF_PROBES_IPv6
					if (IsIPv6OK())
					{
						RETURN_IF_ERROR(StoreIPv6(addr, &here.ifr_addr));
						break;
					}
# else
					OP_ASSERT(!"Yay !  We *did* get an IPv6 address off SIOCGIFADDR !");
# endif // POSIX_IFREQ_CAN_HOLD_IPv6
					continue;
#endif // POSIX_SUPPORT_IPV6
				default: OP_ASSERT(!"Unrecognized family for local IP address"); continue;
				}
				RETURN_IF_ERROR(carrier->AddPosixNetIF(&addr, here.ifr_name, index, up));
			}
		}
	/* In python's netifaces.c, two more ioctl()s are used with ifreq:
	 * SIOCGIFNETMASK and SIOCGIFBRDADDR, each filling in .ifr_addr; for the
	 * latter, if IFF_POINTTOPOINT is set we get a destination address;
	 * otherwise, a broadcast address.
	 */

	return OpStatus::OK;
}
#endif // POSIX_USE_IFREQ_IOCTL

#ifdef POSIX_USE_NETLINK
#define NETLINK_MSG_BUFFER_SIZE 4096

static OP_STATUS NetLinkSendRequest(int netLinkSocketFd, int inetFamily)
{
	sockaddr_nl remoteNetLinkAddr;
	msghdr msgInfo;
	iovec ioVector;

	struct {
		nlmsghdr	msgReqHeader;
		ifaddrmsg	msgReqIfAddrInfo;
	} netlinkReqest;

	memset(&remoteNetLinkAddr, 0, sizeof(remoteNetLinkAddr));
	remoteNetLinkAddr.nl_family = AF_NETLINK;

	memset(&msgInfo, 0, sizeof(msgInfo));
	msgInfo.msg_name = reinterpret_cast<void*>(&remoteNetLinkAddr);
	msgInfo.msg_namelen = sizeof(remoteNetLinkAddr);

	memset(&netlinkReqest, 0, sizeof(netlinkReqest));
	netlinkReqest.msgReqHeader.nlmsg_len = NLMSG_ALIGN(NLMSG_LENGTH(sizeof(netlinkReqest)));
	netlinkReqest.msgReqHeader.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	netlinkReqest.msgReqHeader.nlmsg_type = RTM_GETADDR;
	netlinkReqest.msgReqHeader.nlmsg_pid = getpid();
	netlinkReqest.msgReqIfAddrInfo.ifa_family = inetFamily;
	netlinkReqest.msgReqIfAddrInfo.ifa_index = 0;

	ioVector.iov_base = reinterpret_cast<void*>(&netlinkReqest.msgReqHeader);
	ioVector.iov_len = netlinkReqest.msgReqHeader.nlmsg_len;
	msgInfo.msg_iov = &ioVector;
	msgInfo.msg_iovlen = 1;

	return sendmsg(netLinkSocketFd, &msgInfo, 0) > 0 ? OpStatus::OK : OpStatus::ERR;
}

static OP_STATUS GetNetLink(PosixNetLookup::Store * carrier, int inetFamily)
{
	int msgLength = 0;
	sockaddr_nl netlinkAddr;

	TransientSocket netLinkSock(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (netLinkSock.get() == -1)
		return OpStatus::ERR;

	memset(&netlinkAddr, 0, sizeof(netlinkAddr));
	netlinkAddr.nl_family = AF_NETLINK;
	netlinkAddr.nl_pid = getpid();
	netlinkAddr.nl_groups = RTMGRP_LINK | (inetFamily == AF_INET ? RTMGRP_IPV4_IFADDR : RTMGRP_IPV6_IFADDR);

	if (bind(netLinkSock.get(), reinterpret_cast<sockaddr*>(&netlinkAddr), sizeof(netlinkAddr)) == -1)
		return OpStatus::ERR;

	RETURN_IF_ERROR(NetLinkSendRequest(netLinkSock.get(), inetFamily));

	char netlinkMsgBuffer[NETLINK_MSG_BUFFER_SIZE] = {0};
	nlmsghdr *netlinkMsgHdr = reinterpret_cast<nlmsghdr*>(netlinkMsgBuffer);

	iovec ioVector;
	memset(&ioVector, 0, sizeof(ioVector));
	ioVector.iov_base = reinterpret_cast<void*>(netlinkMsgHdr);
	ioVector.iov_len = NETLINK_MSG_BUFFER_SIZE;

	msghdr messageHdr;
	memset(&messageHdr, 0, sizeof(messageHdr));
	messageHdr.msg_iov = &ioVector;
	messageHdr.msg_iovlen = 1;

	msgLength = recvmsg(netLinkSock.get(), &messageHdr, 0);
	if (msgLength < 0)
		return OpStatus::ERR;
	else if (msgLength == 0)
		return OpStatus::OK;

	while (NLMSG_OK(netlinkMsgHdr, msgLength) && netlinkMsgHdr->nlmsg_type != NLMSG_DONE)
	{
		if (netlinkMsgHdr->nlmsg_type == RTM_NEWADDR)
		{
			ifaddrmsg *netlinkAddrMsg = reinterpret_cast<ifaddrmsg*>(NLMSG_DATA(netlinkMsgHdr));
			rtattr *netlinkIfAddrAttr = IFA_RTA(netlinkAddrMsg);
			int netlinkPayload = IFA_PAYLOAD(netlinkMsgHdr);
			bool up = (netlinkAddrMsg->ifa_flags & IFF_UP) != 0;
			while (netlinkPayload && RTA_OK(netlinkIfAddrAttr, netlinkPayload))
			{
				char netlinkIfName[IFNAMSIZ] = {0};

				if (netlinkIfAddrAttr->rta_type == IFA_ADDRESS || netlinkIfAddrAttr->rta_type == IFA_LOCAL)
				{

					void* netlinkIfAddr = RTA_DATA(netlinkIfAddrAttr);
					PosixNetworkAddress posixAddr;
					switch (netlinkAddrMsg->ifa_family)
					{
					case AF_INET:
						RETURN_IF_ERROR(posixAddr.SetFromIPv(*reinterpret_cast<ipv4_addr*>(netlinkIfAddr)));
						break;
#ifdef POSIX_SUPPORT_IPV6
					case AF_INET6:
# if 0
						/* TODO - FIXME: this can't possibly work.
						 *
						 * The netlinkIfAddr is an int, so nowhere big enough to
						 * be an IPv6 address.  Likely this means we need to
						 * move some of the code before the switch inside the
						 * AF_INET case and add corresponding code here.
						 */
						if (IsIPv6OK())
						{
							RETURN_IF_ERROR(posixAddr.SetFromIPv(*reinterpret_cast<ipv6_addr*>(netlinkIfAddr)));
							break;
						}
# endif // 0
						continue;
#endif // POSIX_SUPPORT_IPV6
					default:
						OP_ASSERT(!"Unrecognized family for local IP address");
						continue;
					}
					RETURN_IF_ERROR(carrier->AddPosixNetIF(&posixAddr, netlinkIfName, netlinkAddrMsg->ifa_index, up));
				}
				netlinkIfAddrAttr = RTA_NEXT(netlinkIfAddrAttr, netlinkPayload);
			}
		}
		netlinkMsgHdr = NLMSG_NEXT(netlinkMsgHdr, msgLength);
	}

	return OpStatus::OK;
}
#endif // POSIX_USE_NETLINK

#ifdef POSIX_USE_IFCONF_IOCTL
/* Make sure this works on Android, since it can't use the other ioctl()s; see
 * CORE-32777.  May have been made redundant by the netlink code (see above). */

static OP_STATUS DigestIfConf(PosixNetLookup::Store * carrier,
							  int sock, struct ifreq * buffer, size_t space)
{
	struct ifconf ifconf;
	op_memset(&ifconf, 0, sizeof(ifconf));
	ifconf.ifc_buf = (char*) buffer;
	ifconf.ifc_len = space;

	if (1 + ioctl(sock, SIOCGIFCONF, (char*)&ifconf))
		for (int index = ifconf.ifc_len / sizeof(struct ifreq); index-- > 0;)
		{
			struct ifreq *here = buffer + index;
			PosixNetworkAddress addr;
			switch (here->ifr_addr.sa_family)
			{
			case AF_INET:
				RETURN_IF_ERROR(StoreIPv4(addr, &here->ifr_addr));
				break;
#ifdef POSIX_SUPPORT_IPV6
				/* Allegedly no IPv6; see
				 * http://comments.gmane.org/gmane.comp.handhelds.android.ndk/4943
				 */
			case AF_INET6:
# ifdef POSIX_IFREQ_CAN_HOLD_IPv6 // See its documentation, above
#define POSIX_NETIF_PROBES_IPv6
				if (IsIPv6OK())
				{
					RETURN_IF_ERROR(StoreIPv6(addr, &here->ifr_addr));
					break;
				}
# else
				OP_ASSERT(!"Yay !  We *did* get an IPv6 address off SIOCGIFCONF !");
# endif // POSIX_IFREQ_CAN_HOLD_IPv6
				continue;
#endif // POSIX_SUPPORT_IPV6
			default: OP_ASSERT(!"Unrecognised family for local IP address"); continue;
			}
			if (1 + ioctl(sock, SIOCGIFFLAGS, here))
				RETURN_IF_ERROR(carrier->AddPosixNetIF(
									&addr, here->ifr_name, index+1,
									(here->ifr_flags & IFF_UP) != 0));
		}

	return OpStatus::OK;
}

static OP_STATUS IfConfIoctl(PosixNetLookup::Store * carrier, int sock)
{
#if 0 // Don't know if SIOCGIFCOUNT is available
	// Is there something else we can use (that works on Android) ?
	int count; // Not sure what third arg SIOCGIFCOUNT takes; this is just a guess
	if (ioctl(sock, SIOCGIFCOUNT, &count) + 1)
	{
		struct ifreq *buffer = reinterpret_cast<struct ifreq *>(
			op_calloc(count, sizeof(struct ifreq)));
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS res = DigestIfConf(carrier, sock, buffer,
									 count * sizeof(struct ifreq));
		op_free(buffer);
		RETURN_IF_ERROR(res);
	}
#else // Hope that 20 >= number of interfaces !
    struct ifreq ifreqs[20]; // ARRAY OK 2011-02-23 eddy
	RETURN_IF_ERROR(DigestIfConf(carrier, sock, ifreqs, sizeof(ifreqs)));
#endif

	return OpStatus::OK;
}
#endif // POSIX_USE_IFCONF_IOCTL

#ifdef POSIX_USE_LIFREQ_IOCTL
// See: http://docs.sun.com/app/docs/doc/816-5177/if-tcp-7p?a=view
static OP_STATUS DigestLifreq(PosixNetLookup::Store * carrier,
							  int sock4, int type,
							  struct lifreq *data,
							  const struct lifnum &ifn)
{
	struct lifconf ifc;
	ifc.lifc_len = ifn.lifn_count * sizeof(struct lifreq);
	ifc.lifc_buf = (caddr_t)data;
	ifc.lifc_family = ifn.lifn_family;
	ifc.lifc_flags = 0;
	if (1 + ioctl(sock4, SIOCGLIFCONF, &ifc) == 0)
		return OpStatus::ERR;

#ifdef POSIX_SUPPORT_IPV6
	TransientSocket sock6(PF_INET6, type);
#endif

	for (int i = 0; i < ifn.lifn_count; i++)
	{
		struct lifreq &here = data[i];
		PosixNetworkAddress local;
		int sock;
		switch (here.lifr_addr.ss_family)
		{
		case AF_INET:
			RETURN_IF_ERROR(StoreIPv4(local, &here.lifr_addr));
			sock = sock4;
			break;
#ifdef POSIX_SUPPORT_IPV6
#define POSIX_NETIF_PROBES_IPv6
		case AF_INET6:
			if (IsIPv6OK()) {
				RETURN_IF_ERROR(StoreIPv6(local, &here.lifr_addr));
				RETURN_IF_ERROR(sock6.check());
				sock = sock6.get();
				break;
			}
			continue;
#endif
		default: OP_ASSERT(!"Unrecognised family for local IP address"); continue;
		}

		// Solaris has SIOCSLIFNAME but no SIOCGLIFNAME !
		if (1 + ioctl(sock, SIOCGLIFFLAGS, &here) &&
			(here.lifr_flags & IFF_LOOPBACK) == 0 &&
			(here.lifr_flags & (IFF_BROADCAST | IFF_POINTOPOINT)))
		{
			bool up = (here.lifr_flags & IFF_UP) != 0;
			if (1 + ioctl(sock, SIOCGLIFINDEX, &here))
				RETURN_IF_ERROR(carrier->AddPosixNetIF(&local,
													   here.lifr_name,
													   here.lifr_index,
													   up));
		}
	}

	return OpStatus::OK;
}

static OP_STATUS LifreqIoctl(PosixNetLookup::Store * carrier, int sock, int type)
{
	struct lifnum ifn;
#ifdef POSIX_SUPPORT_IPV6
	ifn.lifn_family = AF_UNSPEC;
#else
	ifn.lifn_family = AF_INET;
#endif
	ifn.lifn_flags = 0;
	ifn.lifn_count = 0;
	if (1 + ioctl(sock, SIOCGLIFNUM, &ifn) == 0) // ioctl returns -1 on failure
		return OpStatus::ERR;

	if (ifn.lifn_count > 0)
	{
		struct lifreq * buffer = reinterpret_cast<struct lifreq *>(
			op_calloc(ifn.lifn_count, sizeof(struct lifreq)));
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS res = DigestLifreq(carrier, sock, type, buffer, ifn);
		op_free(buffer);
		RETURN_IF_ERROR(res);
	}

	return OpStatus::OK;
}
#endif // POSIX_USE_LIFREQ_IOCTL

#ifdef POSIX_NETIF_PROBES_IPv6
/**
 * Wrapper round a Store, that delegates AddPosixNetIF to it, but checks each
 * interface in passing so as to know if we have any public IPv6.
 */
class ProxyStore : public PosixNetLookup::Store
{
	PosixNetLookup::Store *const m_wrap;
	bool m_public; // have we seen a public IPv6 address ?
public:
	ProxyStore(PosixNetLookup::Store *wrap) : m_wrap(wrap), m_public(false) {}

	virtual OP_STATUS AddPosixNetIF(const PosixNetworkAddress *what,
									const char *name,
									unsigned int index,
									bool up)
	{
		if (what->IsUsableIPv6())
			m_public = true;
		return m_wrap->AddPosixNetIF(what, name, index, up);
	}
	bool PublicIPv6() { return m_public; }
};
#endif // POSIX_NETIF_PROBES_IPv6

// static
OP_STATUS PosixNetLookup::Enumerate(Store * carrier, int type)
{
#ifdef POSIX_NETIF_PROBES_IPv6
	ProxyStore wrap(carrier);
	carrier = &wrap;
#endif
	OP_STATUS err = OpStatus::OK;
	OpStatus::Ignore(err);

#ifdef POSIX_USE_GETIFADDRS
	struct ifaddrs* interfaces = 0;
	if (0 == getifaddrs(&interfaces))
	{
		err = GetIFAddrs(carrier, interfaces);
		freeifaddrs(interfaces);
	}
#endif // POSIX_USE_GETIFADDRS

#ifdef POSIX_USE_NETLINK
	err = GetNetLink(carrier, AF_INET);
# ifdef POSIX_SUPPORT_IPV6
	if (OpStatus::IsSuccess(err))
		err = GetNetLink(carrier, AF_INET6);
# endif //POSIX_SUPPORT_IPV6
#endif // POSIX_USE_NETLINK

#ifdef USING_SOCKET_IOCTL
	TransientSocket sock(PF_INET, type);
	if (OpStatus::IsSuccess(err))
		err = sock.check();
#endif

#ifdef POSIX_USE_IFCONF_IOCTL
	if (OpStatus::IsSuccess(err))
		err = IfConfIoctl(carrier, sock.get());
#endif

#ifdef POSIX_USE_IFREQ_IOCTL
	if (OpStatus::IsSuccess(err))
		err = IfreqIoctl(carrier, sock.get());
#endif

#ifdef POSIX_USE_LIFREQ_IOCTL
	if (OpStatus::IsSuccess(err))
		err = LifreqIoctl(carrier, sock.get(), type);
#endif

#ifdef POSIX_NETIF_PROBES_IPv6
	/* If we saw any public IPv6, record that; else, if we succeeded in checking
	 * all interfaces that we could - and should have seen IPv6 addresses, had
	 * there been any - record that we saw none.
	 */
	if (IsIPv6OK() && (OpStatus::IsSuccess(err) || wrap.PublicIPv6()))
		g_opera->posix_module.SetIPv6Usable(wrap.PublicIPv6());
#endif

	return err;
}

#endif // POSIX_OK_NETIF
