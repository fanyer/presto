/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_CONN_SOCKET_WRAPPER_H_
#define _URL_CONN_SOCKET_WRAPPER_H_

#ifdef PI_INTERNET_CONNECTION

#include "modules/url/url_socket_wrapper.h"
#include "modules/pi/network/OpInternetConnection.h"

	/** An OpSocket wrapper, which checks that a network connection is available 
	 * before trying to talk to the network.
	 */
class ConnectionSocketWrapper : public SocketWrapper, public OpInternetConnectionListener
{
public:
	ConnectionSocketWrapper (OpSocket *socket);
	virtual ~ConnectionSocketWrapper();

	OP_STATUS Connect(OpSocketAddress *socket_address);
	void OnConnectionEvent(BOOL connected);

private:
	OpSocketAddress *m_socket_address;
};

#endif // PI_INTERNET_CONNECTION

#endif // _URL_CONN_SOCKET_WRAPPER_H_
