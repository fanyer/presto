/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_NETWORK_INTERFACE_H
#define POSIX_NETWORK_INTERFACE_H __FILE__
# ifdef POSIX_OK_NETWORK
#include "modules/pi/network/OpNetworkInterface.h"
#include "modules/util/simset.h"
class PosixNetworkInterface;
# ifdef PI_NETWORK_INTERFACE_INSPECTION
class PosixNetworkAddress;
# endif

/** As much of the network interface manager as POSIX can support.
 *
 * The low-level BSD socket APIs in POSIX do not appear to provide any way (that
 * Eddy/2009/Dec/1 can see) to implement some of the features required by
 * OpNetworkInterface and OpNetworkInterfaceManager.  Platforms may wish to
 * simply implement the whole interface for themselves, in which case they
 * should simply not import API_POSIX_NETWORK (although they may still find
 * PosixNetworkAddress useful).
 *
 * Alternatively, platforms can extend this implementation to include the
 * missing features: a class based on this one should implement the base class's
 * static Create() method, SetListener() and the GetConnectionType() extension
 * (see below).  The implementation of Create() should call
 * PosixNetworkInterfaceManager::Init() if POSIX_NETIF_ITERATE_ONCE is defined.
 *
 * Platforms willing to endure the complete lack of listener support and, when
 * API_PI_NETWORK_INTERFACE_INSPECTION is enabled, lack of network type
 * information (or unable to supplement this class with such support) can enable
 * TWEAK_POSIX_PARTIAL_NETMAN to use this basic implementation as-is.
 */
class PosixNetworkInterfaceManager
	: public OpNetworkInterfaceManager
	, public AutoDeleteHead
{
private:
	PosixNetworkInterface* m_iterator;
	bool m_locked;

public:
	PosixNetworkInterfaceManager() : m_iterator(0), m_locked(false) {}

	// OpNetworkInterfaceManager API:
	virtual void SetListener(OpNetworkInterfaceListener*) {}
	virtual OP_STATUS BeginEnumeration();
	virtual void EndEnumeration();
	virtual OpNetworkInterface* GetNextInterface();

	// There should be capability #if-ery on this one (temporary addition to pi, late 2009):
	DEPRECATED(virtual OP_STATUS GetDefaultHTTPInterface(OpNetworkInterface** netif));

	// Type-safe (AutoDelete)Head API:
	PosixNetworkInterface *First();

	// Extension API (see also SetListener, OpNetworkInterfaceManager::Create):

	/** Initialize list of known interfaces.
	 *
	 * When TWEAK_POSIX_NETIF_LOOKUP_ONCE is enabled, this should be called by
	 * Create(); otherwise, it is called by BeginEnumeration().
	 * @return See OpStatus; OK, ERR_NO_MEMORY or ERR.
	 */
	OP_STATUS Init();

#ifdef PI_NETWORK_INTERFACE_INSPECTION
	/** Determine details of an interface.
	 *
	 * Platforms with the ability to determine details (the early "where to
	 * record" parameters) should implement this method.  (For example,
	 * java.net.NetworkInterface does not provide network connection type, but
	 * android.net.ConnectivityManager can be persuaded to report it.)
	 * Unsupported details should be left unchanged; they are initialized
	 * suitably.
	 *
	 * Later parameters are the same as for PosixNetLookup's
	 * Store::AddPosixNetIF(); in particular, the memory pointed to by name may
	 * be temporary storage, which may be modified or rendered unusable after
	 * this function returns.
	 *
	 * Platforms which cannot perform a synchronous look-up are encouraged to
	 * initiate a look-up on start-up (that will discover the available
	 * interfaces and their types), save its results in a hash (see
	 * OpHashTable.h in modules/util/), keyed on whichever of the data passed to
	 * this method is most convenient, and use that as a source from which to
	 * perform the synchronous lookups needed by this method.
	 *
	 * @param type Where to record an OpNetworkInterface::NetworkType (q.v. for
	 *  available values and their meanings) characterising the type of network
	 *  connection.
	 * @param apn ASCII string in which to record the Access Point Name of this
	 *  interface.
	 * @param ssid ASCII string in which to record the Service set identifier,
	 *  or SSID, is the name that identifies a particular 802.11 wireless LAN.
	 * @param what Address of interface.
	 * @param name Local name of this interface (eth1, usb0, etc.).
	 * @param index Numeric identifier of interface.
	 * @param up True iff the interface is currently enabled.
	 * @return See OpStatus; unavailable data should not normally be an error,
	 *  but failure to store available data (e.g. the ->Set() methods of apn or
	 *  ssid) should be reported.
	 */
	virtual OP_STATUS
	GetConnectionDetails(OpNetworkInterface::NetworkType *type,
						 OpString8 *apn,
						 OpString8 *ssid,
						 const PosixNetworkAddress *what,
						 const char *name,
						 unsigned int index,
						 bool up)
	{
		OP_ASSERT(type && apn && ssid);
		return OpStatus::OK;
	}

#endif // PI_NETWORK_INTERFACE_INSPECTION
};

# endif // POSIX_OK_NETWORK
#endif // POSIX_NETWORK_INTERFACE_H
