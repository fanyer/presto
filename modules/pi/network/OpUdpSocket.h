/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPUDPSOCKET_H
#define OPUDPSOCKET_H

#ifdef OPUDPSOCKET

#include "modules/pi/network/OpNetworkStatus.h"

class OpSocketAddress;
class OpUdpSocketListener;

/** @short A UDP socket (connection-less), an endpoint for [network] communication.
 *
 * A socket object has a listener, which the platform code uses to notify core
 * about events (errors, data ready for reading). Which listener method calls
 * are allowed from a given method in this class is documented explicitly for
 * each individual method. If nothing is mentioned about allowed listener
 * method calls in the documentation of a method, it means that no listener
 * method calls are allowed from that method.
 *
 * When an out-of-memory condition has been reported by an OpUdpSocket object,
 * it is in an undefined state, meaning that any further operation, except for
 * deleting it, is pointless.
 */
class OpUdpSocket
{
public:
	/** @short Routing scheme.
	 *
	 * Used in conjuction with an OpSocketAddress, to specify the routing
	 * scheme of an operation (send a packet, bind a socket).
	 */
	enum RoutingScheme
	{
		UNICAST,
		MULTICAST,
		BROADCAST
	};

private:
	/** @short Create and return an OpUdpSocket object.
	 *
	 * @param[out] new_object Set to the socket created
	 * @param listener The listener of the socket (see
	 * OpUdpSocketListener). May be NULL. A listener is only really needed when
	 * binding a UDP socket, using Bind().
	 * @param local_address Local address that may be used to select which
	 * network interface to use. The implementation must not use this socket
	 * address object after this method has returned. The pointer may be NULL,
	 * which means that the default network interface will be used: Normally,
	 * the operating system selects the right network interface to use based on
	 * the remote host address in Send() or the local host address in Bind(),
	 * since part of an address actually identifies the network (for example,
	 * the IPv4 address 10.20.30.40 means the network 10.0.0.0 (if we assume
	 * that the netmask is 255.0.0.0)). However, in cases such as multicast,
	 * the address itself does not identify the network. For multicast, the
	 * default interface to use is configured in the operating system, but may
	 * be overridden here, and it will affect both sending and binding.
	 *
	 * NOTE: this method should only be called by SocketWrapper::CreateUDPSocket.
	 * All core code should use SocketWrapper::CreateUDPSocket instead of calling
	 * this directly.
	 *
	 * @return ERR_NO_MEMORY, ERR_NOT_SUPPORTED, ERR or OK
	 */
	static OP_STATUS Create(OpUdpSocket** new_object, OpUdpSocketListener* listener, OpSocketAddress* local_address = NULL);

		// This is the only class that should call OpUdpSocket::Create
	friend class SocketWrapper;

public:
	/** @short Destructor.
	 *
	 * Unbind the socket. If a listener was specified, its method(s) is never
	 * again to be called.
	 */
	virtual ~OpUdpSocket() {}

	/** @short Change the listener for this socket.
	 *
	 * @param listener New listener to set. NULL is allowed.
	 */
	virtual void SetListener(OpUdpSocketListener* listener) = 0;

	/** @short Bind the socket to the specified local/broadcast address and port.
	 *
	 * Calling this method makes it possible for this host to receive data from
	 * anyone on a given address+port.
	 *
	 * When a socket has been bound, it may not be bound again, i.e. it is not
	 * legal to call this method more than once per object. OpStatus::ERR will
	 * be returned if that is done.
	 *
	 * It is up to the implementation whether or not to support binding more
	 * than one socket to the same address+port. If it is not allowed,
	 * OpNetworkStatus::ERR_ADDRESS_IN_USE is returned if this is attempted.
	 *
	 * OpNetworkStatus::ERR_ACCESS_DENIED is returned when attempting to bind
	 * to an address+port to which the user doesn't have permission (e.g. port
	 * numbers less than 1024 on UNIX systems).
	 *
	 * @param socket_address A local unicast or broadcast address, or a
	 * multicast address. For local socket addresses, the actual address part
	 * may be used to identify on which network interface (loopback, ethernet
	 * interface 1, ethernet interface 2, etc.) to bind the socket. If no
	 * address is set (i.e. if socket_address->IsAddressValid() would return
	 * FALSE if called), the socket will be bound to all network interfaces on
	 * the system. The port number must always be set. The implementation must
	 * not use this socket address object after this method has returned. If
	 * the address is a multicast address (for IPv4, these are in the range
	 * 224.x.x.x to 239.x.x.x range), 'routing_scheme' must be set to MULTICAST
	 * as well. To override the default network interface to use for multicast,
	 * see 'local_address' parameter in the Create() method. It is also
	 * possible to bind to a broadcast address, either for a specific network
	 * interface, or for all (Ipv4: 255.255.255.255). In that case,
	 * 'routing_scheme' must be set to BROADCAST.
	 * @param routing_scheme Specifies the routing scheme of 'socket_address'.
	 *
	 * @return OK, ERR, ERR_NO_MEMORY, ERR_NETWORK, ERR_ADDRESS_IN_USE,
	 * ERR_ACCESS_DENIED, ERR_INTERNET_CONNECTION_CLOSED, ERR_OUT_OF_COVERAGE.
	 */
	virtual OP_NETWORK_STATUS Bind(OpSocketAddress* socket_address, RoutingScheme routing_scheme = UNICAST) = 0;

    /** @short Send a datagram packet to a specified remote host.
	 *
	 * This is a synchronous non-blocking operation; the entire packet must
	 * have been sent when this method has returned, or not sent at all.
	 *
	 * To override the default network interface to use for multicast, see
	 * 'local_address' parameter in the Create() method.
	 *
	 * @param data The data to send
	 * @param length The length of the data
	 * @param socket_address The destination socket address. May be a unicast,
	 * multicast or broadcast address, as specified by 'routing_scheme'.
	 * @param routing_scheme Specifies the routing scheme of 'socket_address'.
	 *
	 * @return OK, ERR, ERR_NO_MEMORY, ERR_NETWORK,
	 * ERR_INTERNET_CONNECTION_CLOSED, ERR_OUT_OF_COVERAGE.
	 */
	virtual OP_NETWORK_STATUS Send(const void* data, unsigned int length, OpSocketAddress* socket_address, RoutingScheme routing_scheme = UNICAST) = 0;

	/** @short Receive a datagram packet from a remote host.
	 *
	 * The socket needs to be bound (see Bind()) before anything can be
	 * received. If it is not bound, OpStatus::ERR will be returned.
	 *
	 * If the datagram packet in the queue doesn't fit in 'buffer', it will be
	 * truncated so that only the the first bytes that can fit will be put
	 * there, and 'bytes_received' will be set to that number. The part of the
	 * packet that did not fit, will be lost (i.e. it cannot be retrieved by
	 * calling Receive() again or something like that).
	 *
	 * This is a synchronous non-blocking operation. A packet may consist of no
	 * data. Calling this method when there is no packet waiting will cause an
	 * error (OpNetworkStatus::ERR_SOCKET_BLOCKING).
	 *
	 * @param[out] buffer A buffer - the buffer to write received data
	 * into. Must be ignored by the caller unless OpStatus::OK is
	 * returned.
	 * @param length Maximum number of bytes to read.
	 * @param[out] bytes_received Set to the number of bytes actually
	 * read. Must be ignored by the caller unless OpStatus::OK is
	 * returned.
	 * @param[out] socket_address The sender socket address (always a unicast
	 * address). Must be ignored by the caller unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR, ERR_NO_MEMORY, ERR_NETWORK, ERR_SOCKET_BLOCKING (no
	 * packets waiting), ERR_INTERNET_CONNECTION_CLOSED, ERR_OUT_OF_COVERAGE.
	 */
	virtual OP_NETWORK_STATUS Receive(void* buffer, unsigned int length, OpSocketAddress* socket_address, unsigned int* bytes_received) = 0;
};

/** @short UDP socket listener - implemented in core code, called from platform code.
 *
 * An object of this class is notified about any state changes or events that
 * occur on the socket to which it subscribes.
 *
 * Note: Unless explicitly mentioned, any method call on an object of
 * this class may cause the calling socket to be deleted. Therefore,
 * make sure that there is nothing on the call stack referencing the
 * socket after the listener method has returned.
 *
 * As an object could conceivably observe more than one socket at a
 * time, sockets calling these methods pass a pointer to the
 * signalling socket.
 */
class OpUdpSocketListener
{
public:
	virtual ~OpUdpSocketListener() {}

	/** @short There is data ready to be received on the socket.
	 *
	 * This listener method is called (typically from the event loop on the
	 * platform side) as long as there is any data at all waiting to be
	 * read. There doesn't necessarily have to be any new data since the
	 * previous call. This means that core should read some (as much as
	 * possible) data (Recv() or RecvFrom() in OpUdpSocket) when this method is
	 * called, to prevent it from throttling.
	 *
	 * @param socket The socket that is ready for reading
	 */
	virtual void OnSocketDataReady(OpUdpSocket* socket) = 0;
};

#endif // OPUDPSOCKET

#endif // !OPUDPSOCKET_H
