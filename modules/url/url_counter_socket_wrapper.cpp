/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef URL_NETWORK_DATA_COUNTERS

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpInternetConnection.h"
#include "modules/url/url_counter_socket_wrapper.h"

OP_STATUS CounterSocketWrapper::Send(const void *data, UINT length)
{
	g_network_counters.sent += length;
	return m_socket->Send(data, length);
}

OP_STATUS CounterSocketWrapper::Recv(void *buffer, UINT length, UINT *bytes_received)
{
	OP_STATUS status = m_socket->Recv(buffer, length, bytes_received);
	g_network_counters.received += *bytes_received;
	return status;
}

#endif // URL_NETWORK_DATA_COUNTERS
