/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SOCKET_ADDRESS_H
#define POSIX_SOCKET_ADDRESS_H __FILE__
# ifdef POSIX_OK_SOCKADDR

#include "platforms/posix/common_socket_address.h"	// CommonSocketAddress
#include "platforms/posix/posix_logger.h"			// PosixLogger
#include <sys/socket.h>								// PF_INET{,6}
#include <netinet/in.h>								// struct in{,6}_addr

/* DSK-246123: gcc 4.0 barfs on some uses of struct in template parameters (see
 * posix_host_resolver.cpp); so use typedefs.
 * Besides, these names are clearer: */
typedef struct in_addr  ipv4_addr;
#ifdef POSIX_SUPPORT_IPV6
typedef struct in6_addr ipv6_addr;
#endif

/** Implementation of OpSocketAddress
 *
 * Inherits from CommonSocketAddress in support of mixed socketry (see
 * TWEAK_POSIX_MIXED_SOCKADDR).
 *
 * Uses in_addr and in6_addr to record address information; this is the most we
 * can get from hostent, returned by getipnodebyname, gethostbyname and the
 * latter's bastard offspring.  It's also all we can get FromString().
 */
class PosixSocketAddress : public CommonSocketAddress, public PosixLogger
{
protected: // so PosixNetworkAddress can mess around with the same bits
	/** Compute internal cached string representation (without port).
	 *
	 * Lazily-evaluates (and caches) a plain ASCII string representation, whose
	 * .CStr() is suitable for use in logging messages.  This makes the ToString
	 * APIs cheap to call repeatedly (as they use the cached value).
	 *
	 * @param result OpString8 to be set to a representation of the address.
	 * @return Status: ERR_NO_MEMORY on OOM, ERR on other errors, else OK.
	 */
	OP_STATUS ComputeAsString(OpString8 &result) const;

	/** Implement GetNetType.
	 *
	 * ... but without obliging its caller to cast away any const-ness of this.
	 * Duly bypasses vtable games while it's at it.  See the pi module's
	 * documentation of OpSocketAddress::GetNetType() for all other details.
	 */
	OpSocketAddressNetType GetNetworkScope() const;

	/** Internal lazily-evaluated string representation.
	 *
	 * Avoid re-computing string representation by cacheing its value here;
	 * methods that change data in ways that would change representation must be
	 * sure to .Empty() this.  Note that this member is mutable, so may be
	 * changed (usually: evaluated and set) by const methods.
	 */
	mutable OpString8 m_as_string;

	union
	{
		ipv4_addr a4;
#ifdef POSIX_SUPPORT_IPV6
		ipv6_addr a6;
#endif
	} m_addr; // selected by m_family
	OpSocketAddress::Family m_family : 2;
	OP_STATUS CheckSpecified();

	friend class OpSocketAddress; // so Create() can call this constructor:
	/** Constructor.
	 *
	 * Primary user is OpSocketAddress::Create(); however, PosixHostResolver and
	 * PosixSocket also need to instantiate this class.
	 */
	PosixSocketAddress()
		: CommonSocketAddress(), PosixLogger(SOCKADDR), m_family(UnknownFamily) {}
public:
	// virtual ~PosixSocketAddress(); // not needed ;->

	// The OpSocketAddress API:

	virtual void SetPort(UINT port)
		{ m_as_string.Empty(); CommonSocketAddress::SetPort(port); }
	virtual OP_STATUS ToString(OpString* result);
	virtual BOOL IsValid();
	virtual BOOL IsHostEqual(OpSocketAddress* socket_address);
	virtual OpSocketAddress::Family GetAddressFamily() const { return m_family; }
	virtual OpSocketAddressNetType GetNetType() { return GetNetworkScope(); }
	/* Over-load GetNetType() with a version that works for a const instance.
	 * Although, generally, extensions belong in PosixNetworkAddress, this
	 * ensures that the const version isn't part of the vtable (where it would
	 * be, over-riding the non-const one, if added in the derived class), so can
	 * be called directly (and actually benefit from inlining).
	 */
	OpSocketAddressNetType GetNetType() const { return GetNetworkScope(); }

	virtual OP_STATUS FromString(const uni_char* address_string); // NB: no port number

#ifndef MIXED_SOCKETRY
	OP_STATUS Import(const OpSocketAddress* socket_address)
	{
		const PosixSocketAddress &local =
			*static_cast<const PosixSocketAddress*>(socket_address);
		SetPort(static_cast<UINT>(local.GetPort()));
		switch (m_family = local.m_family)
		{
		case IPv4: m_addr.a4 = local.m_addr.a4; break;
#ifdef POSIX_SUPPORT_IPV6
		case IPv6: m_addr.a6 = local.m_addr.a6; break;
#endif
		}
		// If m_as_string is available, copy it too:
		if (OpStatus::IsError(m_as_string.Set(local.m_as_string)))
			m_as_string.Empty(); // No problem.

		return OpStatus::OK;
	}
	virtual OP_STATUS Copy(OpSocketAddress* socket_address)
		{ return Import(static_cast<const OpSocketAddress*>(socket_address)); }
	// #else: CommonSocketAddress::Copy() clears m_as_string via FromString().
#endif // !MIXED_SOCKETRY
};
# endif // POSIX_OK_SOCKADDR
#endif // POSIX_SOCKET_ADDRESS_H
