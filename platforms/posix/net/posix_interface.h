/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_INTERFACE_H
#define POSIX_INTERFACE_H __FILE__
# ifdef POSIX_OK_NETIF
#include "platforms/posix/net/posix_network_address.h"

/** Namepsace for network interface lookup functions.
 *
 * This is a wrapper around a hairy mess of ioctl() and simimlarly grungy system
 * calls, with intense platform-dependencies.
 *
 * Some compilers we support don't support namespaces, so this namespace is
 * disguised as a class.
 */
class PosixNetLookup
{
public:
	/** Call-back class for Enumerate.
	 *
	 * Each client of Enumerate should derive a class from this that handles
	 * collecting together the addresses passed in to the AddPosixNetIF() method
	 * by Enumerate.  Of course, if it just wants the best of many, it need not
	 * collect them all together; it can simply remember the best seen so far,
	 * or do whatever suits its needs.  If Enumerate runs into an error, its
	 * caller is at liberty to make do with whatever Store has seen prior to the
	 * error.
	 */
	class Store
	{
	public:
		virtual ~Store() {}

		/** Notifier called with each network interface.
		 *
		 * This API doesn't actually care if you use some OpStatus "error" value
		 * to signify something other than an error, as long as Enumerate()'s
		 * caller and AddPosixNetIF() agree on its meaning.  However, be aware
		 * that Enumerate may return some errors of its own accord, with their
		 * usual meanings; so it would be unwise to use one of these (see
		 * Enumerate() for a list) to mean that AddPosixNetIF() lost interest in
		 * further interfaces.
		 *
		 * Some network addresses may be passed to this method more than once
		 * during a single call to Enumerate.  It is up to the Store to keep
		 * track of this and avoid propagating such duplication to its client.
		 *
		 * The memory in which name is passed is a read-only buffer of size at
		 * most IF_NAMESIZE bytes (including terminating '\\0') which may be
		 * over-written once this method returns; implementations MUST NOT
		 * retain a reference to this buffer.  Make a copy of name instead.
		 *
		 * @param what The network interface, as a socket address.
		 * @param name Local name of this interface (eth0, usb0, etc.).
		 * @param index The index of this interface, a small integer > 0.
		 * @param up True precisely if this interface is currently usable.
		 * @return OK if Enumerate should continue iterating over interfaces;
		 * any other return (see OpStatus) shall be returned by Enumerate.
		 */
		virtual OP_STATUS AddPosixNetIF(const PosixNetworkAddress *what,
										const char *name,
										unsigned int index,
										bool up) = 0;
	};

	/** Report all known network interfaces to a Store object.
	 *
	 * Note that, if there are no interfaces, OK shall be returned without
	 * carrier->AddPosixNetIF() having ever been called.  Also, if several of
	 * the tweaks controlling network interface enquires are enabled, some
	 * interfaces may be found repeatedly.
	 *
	 * @param carrier A Store object to which to ->AddPosixNetIF() a socket
	 * address describing each interface.
	 * @param type Connection-type: SOCK_STREAM or SOCK_DGRAM
	 * @return OK after successfully passing all interfaces to
	 * carrier->AddPosixNetIF(); ERR_NO_MEMORY if OOM is encountered, ERR on
	 * other errors noticed by Enumerate; else anything, other than OK, that
	 * carrier->AddPosixNetIF() happens to return.
	 */
	static OP_STATUS Enumerate(Store * carrier, int type);
};

# endif // POSIX_OK_NETIF
#endif // POSIX_INTERFACE_H
