/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpNetworkInterface.h"
#include "platforms/windows/network/WindowsSocketAddress2.h"
#include <iptypes.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

WindowsOpNetworkInterface::WindowsOpNetworkInterface(OpNetworkInterfaceStatus status)
	: m_status(status)
	, m_address(NULL)
{
}

WindowsOpNetworkInterface::~WindowsOpNetworkInterface()
{
	OP_DELETE(m_address);
}

OP_STATUS WindowsOpNetworkInterface::Init(SOCKET_ADDRESS* address)
{
	if(!m_address)
	{
		RETURN_IF_ERROR(OpSocketAddress::Create(&m_address));

		SOCKET_ADDRESS socket_address;
		socket_address.iSockaddrLength = address->iSockaddrLength;
		socket_address.lpSockaddr = address->lpSockaddr;

		static_cast<WindowsSocketAddress2 *>(m_address)->SetAddress(&socket_address);
	}
	return OpStatus::OK;
}

OP_STATUS WindowsOpNetworkInterface::GetAddress(OpSocketAddress* address)
{
	return address->Copy(m_address);
}

OpNetworkInterfaceStatus WindowsOpNetworkInterface::GetStatus()
{
	return m_status;
}

/* static */
OP_STATUS OpNetworkInterfaceManager::Create(OpNetworkInterfaceManager** new_object, class OpNetworkInterfaceListener* listener)
{
	*new_object = OP_NEW(WindowsOpNetworkInterfaceManager, (listener));
	return *new_object ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

WindowsOpNetworkInterfaceManager::WindowsOpNetworkInterfaceManager(OpNetworkInterfaceListener* listener)
	: m_current_index(0)
	, m_enumerating(false)
	, m_time_of_last_query(0)
	, m_notification_registered(false)
	, m_needs_refresh(true)
{
	op_memset(&m_notification, 0, sizeof(m_notification));
}

WindowsOpNetworkInterfaceManager::~WindowsOpNetworkInterfaceManager()
{
	if (m_notification_registered)
	{
		CancelIPChangeNotify(&m_notification);
	}
	if (m_notification.hEvent)
	{
		WSACloseEvent(m_notification.hEvent);
	}
}

#define WINDOWS_NETWORK_WORKING_BUFFER_SIZE 15000
#define MAX_TRIES							3

OP_STATUS WindowsOpNetworkInterfaceManager::QueryInterfaces()
{
	m_interfaces.DeleteAll();

	IP_ADAPTER_ADDRESSES* adapter_addresses = NULL;
	IP_ADAPTER_ADDRESSES* next_adapter = NULL;
	ULONG len = WINDOWS_NETWORK_WORKING_BUFFER_SIZE;
	ULONG error;
	unsigned int iterations = 0;
	OP_STATUS status = OpStatus::OK;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

	do
	{
		op_free(adapter_addresses);
		adapter_addresses = static_cast<IP_ADAPTER_ADDRESSES *>(op_malloc(len));
		RETURN_OOM_IF_NULL(adapter_addresses);

		error = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapter_addresses, &len);
		iterations++;

	} while ((error == ERROR_BUFFER_OVERFLOW) && (iterations < MAX_TRIES));

	if(error == NO_ERROR)
	{
		// copy the info to our internal class
		next_adapter = adapter_addresses;
		while(next_adapter)
		{
			PIP_ADAPTER_UNICAST_ADDRESS_XP address = next_adapter->FirstUnicastAddress;

			while (address)
			{
				OpAutoPtr<WindowsOpNetworkInterface> adapter(OP_NEW(WindowsOpNetworkInterface, (next_adapter->OperStatus == IfOperStatusUp ? NETWORK_LINK_UP : NETWORK_LINK_DOWN)));
				if (!adapter.get())
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				if (OpStatus::IsError(adapter->Init(&address->Address)))
				{
					address = address->Next;
					continue;
				}

				if (OpStatus::IsError(m_interfaces.Add(adapter.get())))
				{
					address = address->Next;
					continue;
				}
				adapter.release();
				address = address->Next;
			}
			next_adapter = next_adapter->Next;
		}
	}

	op_free(adapter_addresses);
	return status;
}

OP_STATUS WindowsOpNetworkInterfaceManager::RefreshIfNeeded()
{
	if (!m_needs_refresh)
	{
		if (m_notification_registered)
		{
			m_needs_refresh = (WaitForSingleObject(m_notification.hEvent, 0) == WAIT_OBJECT_0);
			if (m_needs_refresh)
			{
				// after event is signalled we have to register again for notifications
				WSAResetEvent(m_notification.hEvent);
				m_notification_registered = false;
			}
		}
		else
		{
			double now = g_op_time_info->GetRuntimeMS();
			m_needs_refresh = (now - m_time_of_last_query >= MIN_QUERY_INTERVAL);
		}
	}

	if (!m_notification_registered)
	{
		if (!m_notification.hEvent)
		{
			m_notification.hEvent = WSACreateEvent();
		}
		if (m_notification.hEvent)
		{
			HANDLE handle = NULL; // not used
			DWORD status = NotifyAddrChange(&handle, &m_notification);
			m_notification_registered = (status == ERROR_IO_PENDING);
		}
	}

	if (m_needs_refresh)
	{
		RETURN_IF_ERROR(QueryInterfaces());
		m_needs_refresh = false;
		m_time_of_last_query = g_op_time_info->GetRuntimeMS();
	}

	return OpStatus::OK;
}


void WindowsOpNetworkInterfaceManager::SetListener(OpNetworkInterfaceListener* listener)
{
}

OP_STATUS WindowsOpNetworkInterfaceManager::BeginEnumeration()
{
	if (m_enumerating)
		return OpStatus::ERR;

	RETURN_IF_ERROR(RefreshIfNeeded());

	m_enumerating = true;
	m_current_index = 0;

	return OpStatus::OK;
}

void WindowsOpNetworkInterfaceManager::EndEnumeration()
{
	m_enumerating = false;
}

OpNetworkInterface* WindowsOpNetworkInterfaceManager::GetNextInterface()
{
	if(m_enumerating && m_current_index < m_interfaces.GetCount())
	{
		OpNetworkInterface* retval = m_interfaces.Get(m_current_index);
		++m_current_index;
		return retval;
	}
	return NULL;
}
