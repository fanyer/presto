/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IRC_IDENTD_H
#define IRC_IDENTD_H

#include "adjunct/m2/src/util/socketserver.h"

class GlueFactory;

//********************************************************************
//
//	IdentdServer
//
//********************************************************************

class IdentdServer
:	public SocketServer::SocketServerListener
{
public:
	// Listener.
	class IdentdListener
	{
	public:
		virtual ~IdentdListener() {}
		virtual void OnConnectionEstablished(const OpStringC &connection_to) { }
		virtual void OnIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response) { }

	protected:
		// Construction.
		IdentdListener() { }
	};

	// Construction / destruction.
	IdentdServer(IdentdListener &listener);
	virtual ~IdentdServer();

	OP_STATUS Init(const OpStringC &ident_as);

private:
	// Methods.
	void SendResponseIfPossible();

	// SocketServer::SocketServerListener.
	virtual void OnSocketConnected(const OpStringC &connected_to);
	virtual void OnSocketDataAvailable();

	// Members.
	IdentdListener &m_listener;
	OpAutoPtr<SocketServer> m_socket_server;

	OpString8 m_ident_as;
	OpString8 m_request;
};

#endif
