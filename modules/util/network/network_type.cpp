/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef UTIL_HAVE_NET_TYPE
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/console/opconsoleengine.h"

OpSocketAddressNetType IPv4type(const UINT8 addr[4])
{
	// http://www.rfc-editor.org/rfc/rfc1918.txt
	switch (addr[0])
	{
	case 127: return NETTYPE_LOCALHOST; // localhost loopback address
		// 10/8, 172.16/12, 192.168/16: private
	case 10: return NETTYPE_PRIVATE; // 10/8 class A private network
	case 172: // 0xac.0x10
		if ((addr[1] & 0xf0) == 0x10) // 172.16/12 class B private network
			return NETTYPE_PRIVATE;
		break;
	case 192: // 0xc0.0xa8
		if (addr[1] == 168) // 192.168/16 class C private network
			return NETTYPE_PRIVATE;
		break;
	}
	return NETTYPE_PUBLIC;
}

# ifdef UTIL_HAVE_NET_TYPE_IPV6
#  ifdef OPERA_CONSOLE
static void ReservedGrumble(const char *msg, ...)
{
	if (g_opera && g_console && g_console->IsLogging())
	{
		va_list ap;
		OpString8 msg8;

		va_start(ap, msg);
		msg8.AppendVFormat(msg, ap);

		OpConsoleEngine::Message console_message(OpConsoleEngine::Network, OpConsoleEngine::Information);
		OpStatus::Ignore(console_message.context.Set("IPv6"));
		OpStatus::Ignore(console_message.message.Set(msg8));

		TRAPD(rc, g_console->PostMessageL(&console_message));
		OpStatus::Ignore(rc);

		va_end(ap);
	}
}
#   else
static inline void ReservedGrumble(const char *msg, ...) {}
#   endif // OPERA_CONSOLE

OpSocketAddressNetType IPv6type(const UINT8 addr[16])
{
	/* See RFC 4291, bug 253152 and (definitively)
	 * http://www.iana.org/assignments/ipv6-address-space
	 */
	if ((addr[0] >> 5) == 1) // 2000::/3
		/* http://www.iana.org/assignments/ipv6-address-space
		 * asserts (2011-02-01) that "IANA unicast address assignments are
		 * currently limited to the IPv6 unicast address range of 2000::/3."
		 * So this single case is worth testing for first, as it'll be by far
		 * the most common case (and it's a cheap test). */
		return NETTYPE_PUBLIC;

	switch (addr[0])
	{
		/* Comments indicate which IN6_IS_ADDR_* macros platforms should use, if
		 * available, when implementing OpSocketAddress::GetNetType themselves.
		 */
	case 0:
		for (int i = 1; i < 10; i++)
			if (addr[i])
			{
				ReservedGrumble("Reserved ::/%x IPv6 address being used.", 8 * i);
				return NETTYPE_UNDETERMINED;
			}

		if (addr[10] == 0 && addr[11] == 0)
		{
			if (addr[12] == 0 && addr[13] == 0 && addr[14] == 0 && addr[15] < 2)
				// ::1/128, IN6_IS_ADDR_LOOPBACK; or ::/128, IN6_IS_ADDR_UNSPECIFIED
				return addr[15] ? NETTYPE_LOCALHOST : NETTYPE_UNDETERMINED;

#ifdef UTIL_IPV6_DEPRECATED_IPV4
			return IPv4type(&(addr[12]));
#endif
		}
		else if (addr[10] == 0xff && addr[11] == 0xff)
			return IPv4type(&(addr[12]));

		ReservedGrumble("Reserved ::/80 IPv6 address being used.");
		break;

	case 0xfc: case 0xfd: // FC00::/7, Unique local unicast, RFC 4193
		/* "[These addresses] are not expected to be routable on the global
		 * Internet.  They are routable inside of a more limited area such as a
		 * site.  They may also be routed between a limited set of sites."
		 *
		 * Not strictly-speaking private, but between localhost and public,
		 * nearer private than either.
		 */
		return NETTYPE_PRIVATE;

	case 0xfe:
		switch (addr[1] & 0xc0) // FE?0:://10
		{
		case 0: case 0x40: // FE00://9
			ReservedGrumble("Reserved fe00::/9 IPv6 address being used.");
			break;

		case 0x80:
			/* FE80://10 link-local, i.e. on the immediately same network, not
			 * accessed via any router, but possibly via a switch (IEUC).  A
			 * stronger condition than private, but less local than loop-back;
			 * for our purposes, it's private. */
#ifdef OPERA_CONSOLE
		{
			/* Use of the link-local range is only specified (thus far) with a
			 * "sub-net ID" of 54 bits all zero.  If we see any FE80::/10 that
			 * aren't FE80::/64, we need to find out what it means and adjust
			 * this, plus possibly platform code, to take account of it.
			 */
			bool subnet = (addr[1] & 0x3f) != 0; // are any bits set after 0xc0 ?
			for (int i = 2; !subnet && i < 8; i++)
				subnet = addr[i] != 0;

			if (subnet)
				ReservedGrumble("Reserved fe80::/10 IPv6 private address used with non-zero subnet ID.");
		}
#endif // OPERA_CONSOLE
			// FE80::/64 "link-local unicast" (private), IN6_IS_ADDR_LINKLOCAL:
			return NETTYPE_PRIVATE;

		case 0xc0:
#ifdef UTIL_IPV6_DEPRECATED_SITE // Deprecated by RFC 3879, September 2004
			// FEC0::/10 "site-local unicast" (private) IN6_IS_ADDR_SITELOCAL
			return NETTYPE_PRIVATE;
#else
			ReservedGrumble("Reserved fec0::/10 IPv6 address being used.");
			// ... but turning on TWEAK_UTIL_IPV6_DEPRECATED_SITE will fix this !
#endif
			break;

		default: OP_ASSERT(!"The above cases cover all four possibilities !"); break;
		}
		break;

	case 0xff: // multicast, IN6_IS_ADDR_MULTICAST
		switch (addr[1]) // the "scope" of the address
		{
		case 1: return NETTYPE_LOCALHOST; // interface-local, IN6_IS_ADDR_MC_NODELOCAL
		case 2: // link-local, IN6_IS_ADDR_MC_LINKLOCAL
		case 4: // admin-local
		case 5: // site-local, IN6_IS_ADDR_MC_SITELOCAL
		case 8: // organization-local, IN6_IS_ADDR_MC_ORGLOCAL
			return NETTYPE_PRIVATE;

		case 0xe: return NETTYPE_PUBLIC; // global, IN6_IS_ADDR_MC_GLOBAL
		default: // 0, 3, f are reserved; administrators may assign the others
			ReservedGrumble("Reserved or unassigned scope used in IPv6 multicast address");
			break;
		}
		break;

	default:
		ReservedGrumble("IPv6 address is in a range reserved by the IETF");
		break;
	}
	return NETTYPE_UNDETERMINED; // ... and undeterminable.
}
# endif // UTIL_HAVE_NET_TYPE_IPV6
#endif // UTIL_HAVE_NET_TYPE
