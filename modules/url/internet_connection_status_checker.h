/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef NET_STATUS_POLLER_H_
#define NET_STATUS_POLLER_H_

#ifdef PI_NETWORK_INTERFACE_MANAGER

#include "modules/hardcore/timer/optimer.h"
#include "modules/util/OpHashTable.h"
#include "modules/pi/network/OpNetworkInterface.h"
#include "modules/url/protocols/comm.h"

// If it's less than MINIMUM_INTERNETCONNECTION_CHECK_INTERVAL_MS milliseconds
// since GetNetworkStatus() was called, we use a cached value.

#define MINIMUM_INTERNETCONNECTION_CHECK_INTERVAL_MS 1000

/**
 * @usage API for checking network status.
 *
 * Check status using g_network_connection_status_checker->GetNetworkStatus();
 *
 *
 * Note that we can only report offline accurately. Thus we report NETWORK_STATUS_MIGHT_BE_ONLINE
 * when we are most likely online.
 */

class InternetConnectionStatusChecker
	: public OpNetworkInterfaceListener
	, public OpTimerListener


{
public:
	enum InternetConnectionStatus
	{
		NETWORK_STATUS_IS_OFFLINE,      // We are definitely offline.
		NETWORK_STATUS_MIGHT_BE_ONLINE  // Maybe online, but we really cannot know for sure.
	};

	/**
	 * Check if this device is offline.
	 *
	 * NETWORK_STATUS_IS_OFFLINE means that there are no network interfaces
	 * connected to a network.
	 *
	 * NETWORK_STATUS_MIGHT_BE_ONLINE means that there are one or more interfaces
	 * connected to a network.
	 *
	 * LOCALHOST is not counted as a network interface.
	 *
	 * @return NETWORK_STATUS_IS_OFFLINE if device is offline, or
	 *         NETWORK_STATUS_MIGHT_BE_ONLINE if device might be online.
	 */
	InternetConnectionStatus GetNetworkStatus();

	/**
	 * Check if a socket is connected through a network interface that is
	 * still up.
	 *
	 * Note that this function does not check if the socket is connected.
	 * If the socket is not connected through an interface, GetNetworkStatus()
	 * above will be returned.
	 *
	 * If socket is connected to LOCALHOST, we define that as an online
	 * network interface, and will return NETWORK_STATUS_MIGHT_BE_ONLINE.
	 *
	 * @param  socket The socket whose network interface to check.
	 * @return NETWORK_STATUS_IS_OFFLINE if the interface the is
	 *         going through has gone down, or NETWORK_STATUS_MIGHT_BE_ONLINE
	 *         otherwise.
	 */
	InternetConnectionStatus GetNetworkInterfaceStatus(OpSocket *socket);

	virtual ~InternetConnectionStatusChecker();

private:
	static OP_STATUS Make(InternetConnectionStatusChecker *&internet_status_checker);

	OpPointerHashTable<Comm, Comm> *GetComTable(){ return &m_existing_comm_objects; }


	InternetConnectionStatusChecker() : m_last_time_checked(0), m_cached_network_status(NETWORK_STATUS_MIGHT_BE_ONLINE) {}

	/** Caller must delete the returned object. */
	OpSocketAddress *GetSocketAddressFromInterface(OpNetworkInterface *nic);

	virtual void OnInterfaceAdded(OpNetworkInterface *nic);
	virtual void OnInterfaceRemoved(OpNetworkInterface *nic);
	virtual void OnInterfaceChanged(OpNetworkInterface *nic);


	void CloseAllSocketsOnNetworkInterface(OpSocketAddress *nic, OpSocketAddress *server_address = NULL);

	virtual void OnTimeOut(OpTimer *timer);

	double  m_last_time_checked;

	struct SocketAddressHashItem
	{
		OpString socket_adress_string;
		OpAutoPtr<OpSocketAddress> socket_address;
	};

	InternetConnectionStatus m_cached_network_status;
	OpAutoStringHashTable<SocketAddressHashItem> m_open_network_interfaces;

	OP_STATUS MaintainInterfaceList(OpNetworkInterface* nic, OpAutoStringHashTable<SocketAddressHashItem> &open_network_interfaces);

	OpNetworkInterfaceManager *m_interface_manager;

	OpPointerHashTable<Comm, Comm> m_existing_comm_objects; // Only Comm objects are allowed to add or remove themselves. Used for checking only.

	OpTimer m_extra_interface_check_timer;

	friend class Comm;
	friend class UrlModule;
};
#endif // PI_NETWORK_INTERFACE_MANAGER

#endif /* NET_STATUS_POLLER_H_ */
