/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** $Id$
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRCOMM_H
#define CRCOMM_H

#include "adjunct/m2/src/glue/connection.h"

class GlueFactory;

// ***************************************************************************
//
//	ClientRemoteComm
//
// ***************************************************************************

class ClientRemoteComm
	: public ProtocolConnection::Client
{
public:
	ClientRemoteComm();
	~ClientRemoteComm();

	/** Start a connection
	  * @param hostname Host to connect to
	  * @param service Name of service
	  * @param port Port to connect to
	  * @param ssl Whether to request an SSL connection
	  * @param ssl_V3_handshake
	  * @param timeout_secs If set to a positive number, connection will be closed
	  *        if this many seconds have passed without receiving a message after
	  *        a message has been sent
	  * @param timeout_on_idle Whether to timeout even if we didn't send a message ourself (i.e.
	  *        expect the server to keep the connection alive)
	  */
	OP_STATUS StartLoading(
		const char* hostname,
		const char* service,
		ProtocolConnection::port_t port,
		BOOL ssl,
		BOOL ssl_V3_handshake = FALSE,
		time_t timeout_secs = 0,
		BOOL timeout_on_idle = FALSE);

protected:
	unsigned int ReadData(char* buf, size_t buf_len);
	OP_STATUS ReadDataIntoBuffer(char*& buf, unsigned& buf_length);
	OP_STATUS SendData(char* buf, size_t buf_len);
	OP_STATUS SendData(const OpStringC8& buf);
	BOOL StartTLSConnection();

	char* AllocMem(size_t size);
	void  FreeMem(char* mem);

	OP_STATUS StopLoading();

	GlueFactory* GetFactory();

	ProtocolConnection* m_connection;
	int m_buffer_size;

private:
	ClientRemoteComm(const ClientRemoteComm&);
	ClientRemoteComm& operator =(const ClientRemoteComm&);
};

#endif // CRCOMM_H
