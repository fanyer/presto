/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include "modules/url/protocols/connman.h"

class WebSocketProtocol;

class WebSocket_Connection : public Connection_Element
{
public:
	WebSocket_Connection(WebSocketProtocol* a_socket);
	virtual ~WebSocket_Connection();

	WebSocketProtocol* Socket() { return m_socket; }

	/* From Connection_Element */
	virtual BOOL SafeToDelete();
	virtual BOOL AcceptNewRequests(){ return TRUE; }
	virtual void SetNoNewRequests() {}

private:
	WebSocket_Connection();
	WebSocketProtocol* m_socket;
};

class WebSocket_Server_Manager : public Connection_Manager_Element
{
public:
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	WebSocket_Server_Manager(ServerName* name, unsigned short port, BOOL secure)
		: Connection_Manager_Element(name, port, secure), m_conns_waiting(FALSE) { }
#else
	WebSocket_Server_Manager(ServerName *name, unsigned short port)
		: Connection_Manager_Element(name, port) { }
#endif

	virtual ~WebSocket_Server_Manager();

	OP_STATUS AddSocket(WebSocketProtocol* a_websocket, MessageHandler* a_mh, ServerName* proxy, unsigned short proxyPort);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual BOOL Preserve() { return TRUE; }

	void StartNextSocket();

	int GetActiveWebSockets();

private:
	BOOL m_conns_waiting;
};


class WebSocket_Manager : public Connection_Manager
{
public:
	WebSocket_Server_Manager* FindServer(ServerName* name, unsigned short port_num, BOOL sec = FALSE, BOOL create = FALSE);

	int GetActiveWebSockets();
};


#endif // WEBSOCKET_MANAGER_H
