/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_SOCKADDR

#include "platforms/posix/net/posix_socket_address.h"
#include "platforms/posix/posix_native_util.h"
#ifdef UTIL_HAVE_NET_TYPE
#include "modules/util/network/network_type.h"
#endif
/** Socket address handling.
 *
 * See RFC 4291 for details of IPv6 addresses, which are 128 bits long.
 * For details of network address types, see bug 253152.
 *
 * http://www.rfc-editor.org/rfc/rfc4291.txt
 * https://bugs.opera.com/show_bug.cgi?id=253152
 */
#ifdef POSIX_DNS_NP
// inet_ntop(), inet_pton() conforms to POSIX.1-2001
#include <arpa/inet.h>
#endif // POSIX_DNS_NP

# ifndef MIXED_SOCKETRY
OP_STATUS OpSocketAddress::Create(OpSocketAddress** socket_address)
{
	*socket_address = OP_NEW(PosixSocketAddress, ());
	return *socket_address ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
# endif // MIXED_SOCKETRY

// NB: this is slated to be renamed IsHostValid()
BOOL PosixSocketAddress::IsValid()
{
	// if (Port() >= 0x10000) return FALSE; // ?
	switch (m_family)
	{
	case IPv4: case IPv6: return TRUE;
	default: OP_ASSERT(!"Bogus address type noticed.");
	case UnknownFamily: break;
	}
	m_as_string.Empty();
	return FALSE;
}

BOOL PosixSocketAddress::IsHostEqual(OpSocketAddress* socket_address)
{
	PosixSocketAddress &local = *static_cast<PosixSocketAddress*>(socket_address);
	if (local.m_family == m_family)
		switch (m_family)
		{
		case IPv4: return op_memcmp(&(m_addr.a4), &(local.m_addr.a4), sizeof(ipv4_addr)) == 0;
#ifdef POSIX_SUPPORT_IPV6
		case IPv6: return op_memcmp(&(m_addr.a6), &(local.m_addr.a6), sizeof(ipv6_addr)) == 0;
#endif
		case UnknownFamily: return TRUE;
		default: OP_ASSERT(!"Bogus address type noticed.");
		}

	return FALSE;
}

OpSocketAddressNetType PosixSocketAddress::GetNetworkScope() const
{
	// See bug 253152 and http://t/security/feature/public-private/ for testing.
	switch (m_family)
	{
	case UnknownFamily: return NETTYPE_UNDETERMINED; // can only be used on invalid host
#ifdef POSIX_SUPPORT_IPV6
# ifdef UTIL_HAVE_NET_TYPE_IPV6
	case IPv6: return IPv6type(m_addr.a6.s6_addr);
# else // Need util >= freshkills_2_beta_70
#error "Please import API_UTIL_NETWORK_TYPE_IPV6"
# endif
#endif // POSIX_SUPPORT_IPV6
#ifdef UTIL_HAVE_NET_TYPE
	case IPv4: return IPv4type(reinterpret_cast<const UINT8*>(&(m_addr.a4)));
#else // Need util >= freshkills_2_beta_70
#error "Please import API_UTIL_NETWORK_TYPE"
#endif // UTIL_HAVE_NET_TYPE
	default: OP_ASSERT(!"Bogus address type noticed.");
	}
	OP_ASSERT(!"Unhandled address family.");
	return NETTYPE_PUBLIC;
}

OP_STATUS PosixSocketAddress::ComputeAsString(OpString8 &result) const
{
	// Mandated to *not* include any port number
#ifdef POSIX_DNS_NP
	size_t len;
	int family;
#else
	result.Empty(); // Ready for AppendFormat().
#endif

	switch (m_family)
	{
	case IPv4:
#ifdef POSIX_DNS_NP
		len = INET_ADDRSTRLEN;
		family = AF_INET;
		break;
#else
		{
			const UINT8 *addr = reinterpret_cast<const UINT8 *>(&(m_addr.a4));
			return result.AppendFormat("%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
		}
#endif // POSIX_DNS_NP

#ifdef POSIX_SUPPORT_IPV6
	case IPv6:
# ifdef POSIX_DNS_NP
		len = INET6_ADDRSTRLEN;
		family = AF_INET6;
		break;
# else // !POSIX_DNS_NP
		{
#define ADDR(i) (unsigned int)op_ntohs(m_addr.a6.s6_addr16[i])
			return result.AppendFormat("%x:%x:%x:%x:%x:%x:%x:%x",
									   ADDR(0), ADDR(1), ADDR(2), ADDR(3),
									   ADDR(4), ADDR(5), ADDR(6), ADDR(7));
#undef ADDR
		}
# endif // POSIX_DNS_NP
#endif

	case UnknownFamily: return result.Set("0.0.0.0");
	default: return OpStatus::ERR;
	}
#undef Cast

#ifdef POSIX_DNS_NP
	if (result.Reserve(len) == 0)
		return OpStatus::ERR_NO_MEMORY;

	const char *ans = inet_ntop(family, reinterpret_cast<const void*>(&m_addr), result.CStr(), len);
	if (ans)
	{
		OP_ASSERT(ans == result.CStr());
		return OpStatus::OK;
	}
#endif // POSIX_DNS_NP
	return OpStatus::ERR;
}

OP_STATUS PosixSocketAddress::ToString(OpString* result)
{
	if (m_as_string.IsEmpty())
	{
		OP_STATUS ret = ComputeAsString(m_as_string);
		if (OpStatus::IsError(ret))
		{
			m_as_string.Empty();
			return ret;
		}
	}
	OP_ASSERT(m_as_string.HasContent());

	return result->Set(m_as_string);
}

OP_STATUS PosixSocketAddress::CheckSpecified()
{
	switch (m_family)
	{
	case IPv4:
		if (m_addr.a4.s_addr == INADDR_ANY)
			m_family = UnknownFamily;
		break;
#ifdef POSIX_SUPPORT_IPV6
	case IPv6:
		if (
# ifdef IN6_IS_ADDR_UNSPECIFIED
			IN6_IS_ADDR_UNSPECIFIED(&(m_addr.a6))
# else
			POSIX_IN6ADDR_ANY && // see TWEAK_POSIX_IN6ADDR_ANY
			op_memcmp(&(m_addr.a6), POSIX_IN6ADDR_ANY, sizeof(ipv6_addr)) == 0
# endif
			)
			m_family = UnknownFamily;
		break;
#endif // POSIX_SUPPORT_IPV6
	default:
		m_family = UnknownFamily;
		OP_ASSERT(!"Bad family value");
		// fall through
	case UnknownFamily:
		return OpStatus::ERR;
	}

	return OpStatus::OK; // Even if we've just marked Unknown because it's an "any" address.
}

OP_STATUS PosixSocketAddress::FromString(const uni_char* address_string)
{
	if (!address_string)
		return OpStatus::ERR_NULL_POINTER;

	m_as_string.Empty(); // invalidated by this method.

	// The address_string is mandated to *not* include port number.
	const int family =
#ifdef POSIX_SUPPORT_IPV6
		uni_strchr(address_string, ':') ? AF_INET6 :
#endif // POSIX_SUPPORT_IPV6
		uni_strchr(address_string, '.') ? AF_INET : 0;

	m_family = UnknownFamily;
	if (!family)
	{
		/* No dot and no colon found, so it is not an IPv6 address and not a
		 * dotted decimal IPv4 address then.  Assume that it is a 32-bit unsigned
		 * integer string, representing a network byte-order IPv4 address. */
		uni_char *end = 0;
		const unsigned long ipv4addr = uni_strtoul(address_string, &end, 0);
		if (end && *end == '\0') // we read the whole string
		{
			*reinterpret_cast<uint32_t*>(&(m_addr.a4)) = op_htonl(ipv4addr);
			m_family = IPv4;
		}
	}
#ifdef POSIX_DNS_NP
	else
	{
# ifdef POSIX_OK_NATIVE
		PosixNativeUtil::NativeString native(address_string);
		RETURN_IF_ERROR(native.Ready());
		const char *const text = native.get();
# else
		// All valid characters in address_string are ASCII, so just downcode them.
		OpString8 native;
		RETURN_IF_ERROR(native.Set(address_string));
		const char *const text = native.CStr();
# endif
		OP_ASSERT(text);

		switch (family == AF_INET
				? inet_aton(text, &m_addr.a4) != 0 // map all non-zero to 1
				: inet_pton(family, text, reinterpret_cast<void*>(&m_addr)))
		{
		case 1:
			switch (family)
			{
			case AF_INET6: m_family = IPv6; break;
			case AF_INET:  m_family = IPv4; break;
			default: OP_ASSERT(!"Unsupported family");
				return OpStatus::ERR;
			}
			break;
		default:
			OP_ASSERT(!"Unrecognized return from inet_pton");
		case -1: // unsupported family
		case 0: // invalid string
			// We don't care about the difference.
			return OpStatus::ERR;
		}
	}
#else // ! POSIX_DNS_NP
	/** ... OK, let's do this the hard way, then !
	 *
	 * This implementation only handles IPv4 in four dotted decimal terms or one
	 * of the three IPv6 formats described on the man 3posix inet_pton page:
	 *
	 * \li "x:x:x:x:x:x:x:x" -- each x is the hex value of a 16-bit part of the
	 * address -- this is the preferred form (used by ToString).
	 *
	 * \li One string of contiguous zero fields in the preferred form may be
	 * replaced with "::" -- the all-zero address can simply be written as "::".
	 *
	 * \li The last two fields may be written in dotted decimal form, yielding a
	 * result of form "x:x:x:x:x:x:d.d.d.d".
	 *
	 * The same man page refers to RFC 2373 for full details; RFC 3513 appears
	 * to supersede that; and is, in turn, obsoleted by RFC 4291.
	 */
# ifdef POSIX_SUPPORT_IPV6
	else if (family == AF_INET6)
	{
		// Cope with the three formats stipulated by: man 3posix inet_pton
		const uni_char *start = address_string;
		const uni_char *end = (start[0] == ':' && start[1] == ':') ? start++ : 0;

		UINT16 * hex = m_addr.a6.s6_addr16;
		int i = 0;
		while (i < 8)
		{
			if (start == end + 1 && *start == ':')
			{
				// :: stands for arbitrarily long :0:0:0:0:...; count how many more tokens remain.
				int n = 8 - i;
				for (end = start + 1; *end; end++)
					if (*end == ':') n--;
					else if (!uni_isxdigit(*end))
						break;

				if (*end == '.')
				{
					int d = 2;
					while (*++end)
						if (*end == '.') d--;
						else if (!uni_isdigit(*end))
							break;

					if (d == 0 && *end == 0)
						n -= 2;
				}

				if (*end)
				{
					Log(NORMAL, "Bogus network address contains :: token.\n");
					return OpStatus::ERR;
				}
				else if (n <= 0)
					Log(NORMAL, "Ignoring :: token in IPv6 address with enough numbers.\n");
				else
				{
					while (n-- > 0)
						hex[i++] = 0;
				}
				start++; // Move over the ':' that got us here.
			}
			uni_char *e = 0;
			const unsigned long chunk = uni_strtoul(start, &e, 16);
			end = e;
			if (end && end > start && (*end == ':' || *end == 0))
			{
				OP_ASSERT(chunk < 0x10000);
				hex[i++] = op_htons(chunk);
				if (*end == 0)
					break;
				start = end + 1;
			}
			else break;
		}

		if (i == 8 && end && *end == 0)
			m_family = IPv6;
		else if (i == 6 && end && end > start && *end == '.')
		{
			// "Mixed notation" x:x:x:x:x:x:d.d.d.d
			UINT8 *tail = reinterpret_cast<UINT8 *>(hex);
			i = 0;
			while (i < 4)
			{
				uni_char *e = 0;
				const unsigned long chunk = uni_strtoul(start, &e, 10);
				end = e;
				if (end && end > start && (*end == '.' || *end == 0))
				{
					OP_ASSERT(chunk < 0x100);
					tail[i++] = chunk;
					if (*end == 0) break;
					start = end + 1;
				}
			}
			if (i == 4)
				m_family = IPv6;
		}
	}
# endif // POSIX_SUPPORT_IPV6
	else
	{
		/* http://docsrv.sco.com/cgi-bin/man/man?inet+3N asserts that
		 * 8bit.8bit.16bit and 8bit.24bit formats also exist (as well as the
		 * 32-bit format handled above and the 8bit.8bit.8bit.8bit handled
		 * here); and that the individual numbers may be in hex, decimal or
		 * octal, in accord with C number format.  This is only for pure IPv4;
		 * it says nothing of this kind for IPv6 "mixed notation".  It also says
		 * that inet_pton only supports the simple d.d.d.d format.  The
		 * following should handle all but the 32-bit handled earlier. -
		 * Eddy/2011/Jul/14. */
		const uni_char *start = address_string;
		UINT8 * dec = reinterpret_cast<UINT8 *>(&(m_addr.a4));
		int i = 0;
		while (i < 4)
		{
			uni_char *end;
			unsigned long chunk = uni_strtoul(start, &end, 0);
			if (end && end > start && (*end == '.' || *end == '\0'))
			{
				if (*end == 0)
				{
					/* Last chunk: may actually be several rolled together.
					 *
					 * Its least significant byte is, in any case, the last byte
					 * of our address; with more significant bytes as the
					 * earlier bytes.  So we can unpack it into the tail of our
					 * array, although it *must* all fit after dec[i] since
					 * that's already been filled.
					 */
					for (int j = 4; j-- > i;)
					{
						dec[j] = chunk & 0xff;
						chunk >>= 8;
					}
					OP_ASSERT(chunk == 0); // i.e. it was originally < (1<<(8*(4-i))).
					i = 4;
					break;
				}
				// Otherwise, we (should) just have one byte:

				OP_ASSERT(chunk < 0x100);
				dec[i++] = chunk;
				start = end + 1;
			}
			else // format error
				break;
		}
		if (i == 4)
			m_family = IPv4;
	}
#endif // POSIX_DNS_NP

	return CheckSpecified();
}
#endif // POSIX_OK_SOCKADDR
