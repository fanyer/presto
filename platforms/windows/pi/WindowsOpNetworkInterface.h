/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef WINDOWS_OPNETWORKINTERFACE_H
#define WINDOWS_OPNETWORKINTERFACE_H

#ifdef PI_NETWORK_INTERFACE_MANAGER

#include "modules/pi/network/OpNetworkInterface.h"

class WindowsOpNetworkInterface : public OpNetworkInterface
{
friend class WindowsOpNetworkInterfaceManager;

private:
	WindowsOpNetworkInterface(OpNetworkInterfaceStatus status);

	OP_STATUS Init(SOCKET_ADDRESS* address);

public:
	~WindowsOpNetworkInterface();

	virtual OP_STATUS GetAddress(OpSocketAddress* address);
	virtual OpNetworkInterfaceStatus GetStatus();

private:
	OpNetworkInterfaceStatus	m_status;
	OpSocketAddress*			m_address;
};

class WindowsOpNetworkInterfaceManager : public OpNetworkInterfaceManager
{
	friend OP_STATUS OpNetworkInterfaceManager::Create(OpNetworkInterfaceManager** new_object, class OpNetworkInterfaceListener* listener);

private:
	WindowsOpNetworkInterfaceManager(OpNetworkInterfaceListener * listener);

public:
	virtual ~WindowsOpNetworkInterfaceManager();
	virtual void SetListener(OpNetworkInterfaceListener* listener);
	virtual OP_STATUS BeginEnumeration();
	virtual void EndEnumeration();
	virtual OpNetworkInterface* GetNextInterface();

private:
	OpAutoVector<OpNetworkInterface> m_interfaces;
	unsigned int m_current_index;
	bool m_enumerating;

	double m_time_of_last_query; // last time m_interfaces was refreshed
	OVERLAPPED m_notification; // overlapped struct that is registered with NotifyAddrChange
	bool m_notification_registered; // true if m_notification was registered with NotifyAddrChange
	bool m_needs_refresh; // whether m_interfaces should be refreshed

	static const int MIN_QUERY_INTERVAL = 5000; // minimum time between refreshes of m_interfaces; used if NotifyAddrChange fails

	OP_STATUS RefreshIfNeeded();
	OP_STATUS QueryInterfaces();
};

#endif // PI_NETWORK_INTERFACE_MANAGER

#endif // WINDOWS_OPNETWORKINTERFACE_H
