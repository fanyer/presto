/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef COMMON_SOCKET_ADDRESS_H
#define COMMON_SOCKET_ADDRESS_H __FILE__

#include "modules/pi/network/OpSocketAddress.h"

/** A re-usable base-class for OpSocketAddress implementations.
 *
 * This base-class implements the obvious management of the port number and
 * provides the implementation-neutral implementation of Copy(), needed for
 * TWEAK_POSIX_MIXED_SOCKADDR (q.v.).  It also provides for being able to use
 * const OpSocketAddress* wherever reasonable, unlike the PI.
 */
class CommonSocketAddress : public OpSocketAddress
{
	// Ideally in_port_t, but requires #include <netinet/in.h>, which declares it 16-bit.
	UINT16 m_port;
protected:
	CommonSocketAddress() : m_port(0) {}
	// Let const instances of derived classes know m_port:
	UINT16 GetPort() const { return m_port; }
public:
	virtual void SetPort(UINT port)
		{ OP_ASSERT(port < 0x10000); m_port = static_cast<UINT16>(port); }
	virtual UINT Port() { return static_cast<UINT>(m_port); }
	virtual BOOL IsEqual(OpSocketAddress* socket_address)
		{ return Port() == socket_address->Port() && IsHostEqual(socket_address); }

#ifdef MIXED_SOCKETRY
	OP_STATUS Import(const OpSocketAddress* socket_address)
	{
		OpString text;
		RETURN_IF_ERROR(socket_address->ToString(&text)); // doesn't include port
		RETURN_IF_ERROR(FromString(&text)); // does not set port
		SetPort(socket_address->Port());
		return OpStatus::OK;
	}
	virtual OP_STATUS Copy(OpSocketAddress* socket_address)
		{ return Import(static_cast<const OpSocketAddress*>(socket_address)); }
#endif
};

#endif // COMMON_SOCKET_ADDRESS_H
