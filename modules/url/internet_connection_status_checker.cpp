/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#ifdef PI_NETWORK_INTERFACE_MANAGER

#include "modules/url/internet_connection_status_checker.h"
#include "modules/pi/network/OpNetworkInterface.h"
#include "modules/pi/network/OpSocketAddress.h"

/*static */ OP_STATUS InternetConnectionStatusChecker::Make(InternetConnectionStatusChecker *&internet_status_checker)
{
	OpAutoPtr<InternetConnectionStatusChecker> checker(OP_NEW(InternetConnectionStatusChecker, ()));
	RETURN_OOM_IF_NULL(checker.get());

	RETURN_IF_ERROR((OpNetworkInterfaceManager::Create(&checker->m_interface_manager, checker.get())));

	checker->m_extra_interface_check_timer.SetTimerListener(checker.get());

	internet_status_checker = checker.release();
	return OpStatus::OK;
}

InternetConnectionStatusChecker::InternetConnectionStatus InternetConnectionStatusChecker::GetNetworkInterfaceStatus(OpSocket *socket)
{
	// Get global network status.
	InternetConnectionStatus global_status = GetNetworkStatus();

	InternetConnectionStatus socket_interface_status = NETWORK_STATUS_MIGHT_BE_ONLINE;
	OpSocketAddress *socket_interface_address;
	OpString socket_interface_address_string;
	if (
			OpStatus::IsSuccess(OpSocketAddress::Create(&socket_interface_address)) &&
			OpStatus::IsSuccess(socket->GetLocalSocketAddress(socket_interface_address)) &&
			OpStatus::IsSuccess(socket_interface_address->ToString(&socket_interface_address_string)) &&
			socket_interface_address->IsHostValid()
		)
	{
		// Special case for localhost.
		if (socket_interface_address->GetNetType() == NETTYPE_LOCALHOST)
			socket_interface_status = NETWORK_STATUS_MIGHT_BE_ONLINE;
		else if (!m_open_network_interfaces.Contains(socket_interface_address_string.CStr()))
			socket_interface_status = NETWORK_STATUS_IS_OFFLINE;
	}
	else
		socket_interface_status = global_status;

	OP_DELETE(socket_interface_address);

	return socket_interface_status;
}

InternetConnectionStatusChecker::InternetConnectionStatus InternetConnectionStatusChecker::GetNetworkStatus()
{
	double current_time = g_op_time_info->GetRuntimeMS();

	// If it's less than MINIMUM_INTERNETCONNECTION_CHECK_INTERVAL_MS milliseconds since last time, use cached value.
	if (current_time - m_last_time_checked < MINIMUM_INTERNETCONNECTION_CHECK_INTERVAL_MS)
	{
		// In case the network has gone down since last time check was performed, check again after one interval.
		m_extra_interface_check_timer.Start(MINIMUM_INTERNETCONNECTION_CHECK_INTERVAL_MS + 500);

		return m_cached_network_status;
	}

	OpAutoStringHashTable<SocketAddressHashItem> updated_open_network_interfaces;

	if (OpStatus::IsSuccess(m_interface_manager->BeginEnumeration()))
	{
		while (OpNetworkInterface* network_interface = m_interface_manager->GetNextInterface())
			OpStatus::Ignore(MaintainInterfaceList(network_interface, updated_open_network_interfaces));

		m_interface_manager->EndEnumeration();
	}

	BOOL at_least_one_network_interface_is_up = FALSE;

	// Go through the cached network interfaces and check if they are still open.
	OpAutoPtr<OpHashIterator> iterator(m_open_network_interfaces.GetIterator());
	if (iterator.get() && OpStatus::IsSuccess(iterator->First()))
	{
		do
		{
			SocketAddressHashItem *item = static_cast<SocketAddressHashItem*>(iterator->GetData());

			if (updated_open_network_interfaces.Contains(item->socket_adress_string.CStr()))
			{
				at_least_one_network_interface_is_up = TRUE;
			}
			else
			{
				// Network interface has gone down. We need to close all sockets on that interface.
				CloseAllSocketsOnNetworkInterface(item->socket_address.get());

				if (OpStatus::IsSuccess(m_open_network_interfaces.Remove(item->socket_adress_string, &item)))
					OP_DELETE(item);
			}

		} while (OpStatus::IsSuccess(iterator->Next()));
	}

	// We add any new interface to the cached list
	iterator.reset(updated_open_network_interfaces.GetIterator());
	if (iterator.get() && OpStatus::IsSuccess(iterator->First()))
	{
		do
		{
			SocketAddressHashItem *item = static_cast<SocketAddressHashItem*>(iterator->GetData());

			if (item && !m_open_network_interfaces.Contains(item->socket_adress_string.CStr()))
			{
				at_least_one_network_interface_is_up = TRUE;

				updated_open_network_interfaces.Remove(item->socket_adress_string, &item);
				m_open_network_interfaces.Add(item->socket_adress_string.CStr(), item);
			}
		} while (OpStatus::IsSuccess(iterator->Next()));
	}

	InternetConnectionStatus new_status;
	if (!at_least_one_network_interface_is_up)
	{
		if (m_cached_network_status == NETWORK_STATUS_MIGHT_BE_ONLINE)
		{
			// Network connection has bee turned off since last time.
			// We need to go in and check if URL has any open connections.
			// If so, we must close them, as we will probably not get any
			// notification from network layer.
		}

		new_status = NETWORK_STATUS_IS_OFFLINE;
	}
	else
	{
		// We probably are online, but we cannot know for sure.
		new_status = NETWORK_STATUS_MIGHT_BE_ONLINE;
	}

#ifdef PI_INTERNET_CONNECTION
	if (new_status != m_cached_network_status)
		g_internet_connection.OnConnectionEvent(at_least_one_network_interface_is_up);
#endif // PI_INTERNET_CONNECTION

	m_cached_network_status = new_status;
	m_last_time_checked = current_time;

	return m_cached_network_status;
}


OpSocketAddress *InternetConnectionStatusChecker::GetSocketAddressFromInterface(OpNetworkInterface *nic)
{
	OpSocketAddress *address;
	RETURN_VALUE_IF_ERROR(OpSocketAddress::Create(&address), NULL);
	if (OpStatus::IsSuccess(nic->GetAddress(address)))
		return address;

	OP_DELETE(address);
	return NULL;
}

void InternetConnectionStatusChecker::CloseAllSocketsOnNetworkInterface(OpSocketAddress *nic, OpSocketAddress *server_address)
{
	OpAutoPtr<OpHashIterator> iterator(m_existing_comm_objects.GetIterator());
	if (iterator.get() && OpStatus::IsSuccess(iterator->First()))	do
	{
		static_cast<Comm*>(iterator->GetData())->CloseIfInterfaceDown(nic , server_address);
	} while (OpStatus::IsSuccess(iterator->Next()));
}

InternetConnectionStatusChecker::~InternetConnectionStatusChecker()
{
	OP_DELETE(m_interface_manager);
}

OP_STATUS InternetConnectionStatusChecker::MaintainInterfaceList(OpNetworkInterface *nic, OpAutoStringHashTable<SocketAddressHashItem> &open_network_interfaces)
{
	OpAutoPtr<OpSocketAddress> address(GetSocketAddressFromInterface(nic));
	RETURN_OOM_IF_NULL(address.get());
	OpString address_string;
	RETURN_IF_ERROR(address->ToString(&address_string));

	if (nic->GetStatus() == NETWORK_LINK_UP && !open_network_interfaces.Contains(address_string.CStr()))
	{
		OpSocketAddressNetType net_type = address->GetNetType();
		if (net_type == NETTYPE_PRIVATE || net_type == NETTYPE_PUBLIC)
		{
			OpAutoPtr<SocketAddressHashItem> item(OP_NEW(SocketAddressHashItem, ()));
			RETURN_OOM_IF_NULL(item.get());

			item->socket_address.reset(address.release()); // Takes over
			address.reset(NULL);

			RETURN_IF_ERROR(item->socket_adress_string.Set(address_string.CStr()));

			RETURN_IF_ERROR(open_network_interfaces.Add(item->socket_adress_string, item.get()));
				item.release();
		}
	}
	else if (nic->GetStatus() == NETWORK_LINK_DOWN)
	{
		if (open_network_interfaces.Contains(address_string.CStr()))
			CloseAllSocketsOnNetworkInterface(address.get());

		SocketAddressHashItem *item;
		if (OpStatus::IsSuccess(open_network_interfaces.Remove(address_string.CStr(), &item)))
			OP_DELETE(item);
	}

	return OpStatus::OK;
}

void InternetConnectionStatusChecker::OnInterfaceAdded(OpNetworkInterface *nic)
{
	OpStatus::Ignore(MaintainInterfaceList(nic, m_open_network_interfaces));
}

void InternetConnectionStatusChecker::OnInterfaceRemoved(OpNetworkInterface *nic)
{
	OpAutoPtr<OpSocketAddress> address(GetSocketAddressFromInterface(nic));
	if (!address.get())
		return;

	CloseAllSocketsOnNetworkInterface(address.get());

	OpString address_string;
	RETURN_VOID_IF_ERROR(address->ToString(&address_string));


	SocketAddressHashItem *item;
	if (OpStatus::IsSuccess(m_open_network_interfaces.Remove(address_string.CStr(), &item)))
		OP_DELETE(item);
}

void InternetConnectionStatusChecker::OnInterfaceChanged(OpNetworkInterface *nic)
{
	OpStatus::Ignore(MaintainInterfaceList(nic, m_open_network_interfaces));
}

void InternetConnectionStatusChecker::OnTimeOut(OpTimer *timer)
{
	OP_ASSERT(timer == &m_extra_interface_check_timer);

	GetNetworkStatus();
}



#endif // PI_NETWORK_INTERFACE_MANAGER
