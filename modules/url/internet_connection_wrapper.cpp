/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PI_INTERNET_CONNECTION

#include "modules/url/internet_connection_wrapper.h"

#ifdef PI_NETWORK_INTERFACE_MANAGER
# include "modules/url/internet_connection_status_checker.h"
#endif // PI_NETWORK_INTERFACE_MANAGER

InternetConnectionWrapper::InternetConnectionWrapper() : OpInternetConnectionListener(), m_internet_connection(NULL), m_is_connected(FALSE) { }

OP_STATUS InternetConnectionWrapper::Init()
{
	OP_STATUS result = OpStatus::OK;
	if (!m_internet_connection)
		result = OpInternetConnection::Create(&m_internet_connection, this);

	return result;
}

InternetConnectionWrapper::~InternetConnectionWrapper()
{
	OP_ASSERT(m_listeners.Empty()); // the list should be empty
	OP_DELETE(m_internet_connection);
}

void InternetConnectionWrapper::AddListener(OpInternetConnectionListener *listener)
{
	OP_ASSERT(m_internet_connection); // this object should have been previously initialised
	OP_ASSERT(!m_listeners.HasLink(listener)); // the listener should not already be in the list
	listener->Into(&m_listeners);
}

void InternetConnectionWrapper::RemoveListener(OpInternetConnectionListener *listener)
{
	OP_ASSERT(m_internet_connection); // this object should have been previously initialised
	listener->Out();
}

OP_STATUS InternetConnectionWrapper::RequestConnection(OpInternetConnectionListener *listener)
{
	OP_ASSERT(m_internet_connection); // this object should have been previously initialised
	AddListener(listener);
	return m_internet_connection->RequestConnection();
}

BOOL InternetConnectionWrapper::IsConnected()
{
	OP_ASSERT(m_internet_connection); // this object should have been previously initialised
#ifdef PI_NETWORK_INTERFACE_MANAGER
	return m_is_connected && g_network_connection_status_checker->GetNetworkStatus() == InternetConnectionStatusChecker::NETWORK_STATUS_MIGHT_BE_ONLINE;
#else
	return m_is_connected;
#endif
}

void InternetConnectionWrapper::OnConnectionEvent(BOOL connected)
{
	OP_ASSERT(m_internet_connection); // this object should have been previously initialised

	m_is_connected = connected;

	OpInternetConnectionListener *icl = m_listeners.First();
	while (icl)
	{
		OpInternetConnectionListener *current = icl;

		// OnConnectionEvent may delete the current listener so
		// fetch the next in line before proceeding.
		icl = icl->Suc();

		RemoveListener(current);
		current->OnConnectionEvent(connected);
	}
}
#endif // PI_INTERNET_CONNECTION
