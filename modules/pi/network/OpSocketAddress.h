/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OP_SOCKET_ADDRESS_H
#define OP_SOCKET_ADDRESS_H

/** Type of network. */
enum OpSocketAddressNetType
{
	/** An address representing the local host.
	 *
	 * For IPv4 addresses, this is an address that starts with "127."
	 */
	NETTYPE_LOCALHOST,

	/** An address representing a host on a private network.
	 *
	 * For IPv4 addresses, this is an address that starts with "10.", "172.16."
	 * or "192.168.".
	 */
	NETTYPE_PRIVATE,

	/** An address representing a host on a public network (aka. the internet). */
	NETTYPE_PUBLIC,

	/** It could not be determined what kind of network this host is on. */
	NETTYPE_UNDETERMINED
};

/** @short A network address, in some cases, including port number.
 *
 * This will typically be an IPv4 or IPv6 address, but this interface puts no
 * limit on which address family to support. Address families can be added
 * to enum OpSocketAddress::Family. Some kind of support code for a given
 * address family is usually required on both the core and platform side,
 * though.
 *
 * This class may be used to represent the following:
 *
 * - Regular socket address. This includes port number, and such an instance is
 * passed to OpSocket when attempting to establish a new connection, or when
 * listening for incoming connections on a specific network interface.
 *
 * - Host address. Port number is set to 0. Such an instance is returned from
 * the host resolver.
 *
 * - Port number for listening on the default network interface, with the
 * address part left empty. Such an instance may be used for listening. See
 * also above; the API user may wish to specify an actual address part as well,
 * to bind the listening socket to a specific network interface, instead of
 * using the default one.
 *
 * Initial value of an OpSocketAddress is no address, represented as an IPv4
 * 0.0.0.0 if ToString() is called, and IsHostValid() will return
 * FALSE. Initial port number is 0.
 */
class OpSocketAddress
{
public:
	/** Socket address family. */
	enum Family
	{
		UnknownFamily,
		IPv4,
		IPv6
	};

	/** Create a new OpSocketAddress.
	 *
	 * @param socket_address (out) Created socket address.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::ERR or OpStatus::OK
	 * otherwise.
	 */
	static OP_STATUS Create(OpSocketAddress** socket_address);

	virtual ~OpSocketAddress() {}

	/** Get the address represented as a string.
	 *
	 * Necessary e.g. to display the actual address of the host.
	 *
	 * @param result (out) An OpString set to the string representation of this
	 * address. This shall not include the port number, just the textual
	 * representation of the address, e.g. "10.20.30.40" for IPv4, or
	 * "2001:200::8002:203:47ff:fea5:3085" for IPv6.
	 */
	virtual OP_STATUS ToString(OpString* result) = 0;

	/** Set the address from its string representation.
	 *
	 * The platform code must recognize a valid IPv4 dotted decimal address
	 * string (e.g. "10.20.30.40"). Future versions of OpSocketAddress may
	 * define additional formats to be supported, or provide a means of
	 * querying the platform about what it supports. If the supplied argument
	 * is not recognized as an address, it should set the address to initial
	 * address (e.g. no address -- see main documentation of
	 * OpSocketAddress). The port number should never be affected by a call to
	 * this method.
	 *
	 * @param address_string The string representation of an address, not
	 * including port number. The caller should avoid leading and/or trailing
	 * whitespace and other cruft, as it is up to the platform implementation
	 * how to deal with it (and whether or not it makes it an "unrecognized
	 * address format").
	 *
	 * @return OK, ERR (unrecognized address format in string) or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS FromString(const uni_char* address_string) = 0;

	/** Is the host part of the socket address valid? Port number is irrelevant.
	 *
	 * An IPv4 address is invalid if it is 0.0.0.0. In its initial state an
	 * OpSocketAddress is defined as invalid.
	 *
	 * Note to callers and implementors: In a future version of the
	 * OpSocketAddress API, this method will either go away, or change
	 * semantics, due to the fact that it currently only considers the address
	 * part (and ignores the port number) of an OpSocketAddress. API users
	 * should call IsHostValid() (which should become a virtual method in the
	 * future) to check if the address part is valid.
	 *
	 * @return TRUE if the address is valid, or FALSE if not.
	 */
	virtual BOOL IsValid() = 0;

	/** Is the host part of the socket address valid? Port number is irrelevant.
	 *
	 * An IPv4 address is invalid if it is 0.0.0.0. In its initial state an
	 * OpSocketAddress is defined as invalid.
	 *
	 * @return TRUE if the address is valid, or FALSE if not.
	 */
	inline BOOL IsHostValid() { return IsValid(); }

	/** Set the port number.
	 *
	 * @param port The port number
	 */
	virtual void SetPort(UINT port)=0;

	/** Get the port number.
	 *
	 * @return The port number (or 0 if not set)
	 */
	virtual UINT Port() = 0;

	/** Copy the provided socket address into this object.
	 *
	 * @param socket_address address to copy into this object.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, or OpStatus::OK.
	 */
	virtual OP_STATUS Copy(OpSocketAddress* socket_address) = 0;

	/** Compare this address and port to another address and port.
	 *
	 * Two addresses in their initial state (see OpSocketAddress main
	 * documentation) are defined as equal.
	 *
	 * @param socket_address The other socket address (and port) with which to
	 * compare this socket address (and port).
	 *
	 * @return TRUE if the socket addresses and ports are equal.
	 */
	virtual BOOL IsEqual(OpSocketAddress* socket_address) = 0;

	/** Compare two host addresses, ignoring port number.
	 *
	 * Two addresses in their initial state (see OpSocketAddress main
	 * documentation) are defined as equal.
	 *
	 * @param socket_address The other socket address with which to compare
	 * this socket address.
	 *
	 * @return TRUE if the addresses are equal.
	 */
	virtual BOOL IsHostEqual(OpSocketAddress* socket_address) = 0;

	/** Get the socket address family.
	 *
	 * @return socket address family. This method can return UnknownFamily,
	 * if the implementation can't determine the socket family, for example
	 * if a valid address hasn't been set yet (using FromString() or Copy()).
	 * If this method returns a value other than UnknownFamily - the
	 * address is surely valid, i.e. IsHostValid() would return TRUE.
	 */
	virtual OpSocketAddress::Family GetAddressFamily() const = 0;

	/** Get the network type for this address.
	 *
	 * @return Type of network on which the host at this address
	 * is. NETTYPE_UNDETERMINED is only returned if this is an invalid host
	 * (i.e. when IsHostValid() would return FALSE).
	 */
	virtual OpSocketAddressNetType GetNetType() = 0;
};

#endif // !OP_SOCKET_ADDRESS_H
