/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "modules/util/simset.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "crcomm.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/glue/factory.h"

class CommGlue;

// ***************************************************************************
//
//	ClientRemoteComm
//
// ***************************************************************************

ClientRemoteComm::ClientRemoteComm()
:	m_connection(NULL)
  , m_buffer_size(-1)
{
}


ClientRemoteComm::~ClientRemoteComm()
{
	MessageEngine::GetInstance()->GetGlueFactory()->DeleteCommGlue(m_connection);
}


OP_STATUS ClientRemoteComm::StartLoading(const char* host, const char* service,
	ProtocolConnection::port_t port, BOOL ssl, BOOL ssl_V3_handshake, time_t timeout_secs, BOOL timeout_on_idle)
{
	m_connection = MessageEngine::GetInstance()->GetGlueFactory()->CreateCommGlue();
	if (m_connection == NULL)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_connection->SetClient(this));
	RETURN_IF_ERROR(m_connection->Connect(host, service, port, ssl, ssl_V3_handshake));

	OpStatus::Ignore(m_connection->SetTimeout(timeout_secs, timeout_on_idle));

	return OpStatus::OK;
}


unsigned int ClientRemoteComm::ReadData(char* buf, size_t buflen)
{
	if (!m_connection)
        return 0;

	return m_connection->Read(buf, buflen);
}

OP_STATUS ClientRemoteComm::ReadDataIntoBuffer(char*& buf, unsigned& buf_length)
{
	if (!m_connection)
		return OpStatus::ERR_NULL_POINTER;

	// Retrieve network buffer size if necessary
	if (m_buffer_size == -1)
	{
		m_buffer_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
		if (m_buffer_size <= 1)
			m_buffer_size = 1024;
	}

	// Allocate buffer
	buf = OP_NEWA(char, m_buffer_size + 1);
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	// Read data
	buf_length = m_connection->Read(buf, m_buffer_size);

	// Terminate data
	buf[buf_length] = '\0';

	return OpStatus::OK;
}

/*
OP_STATUS ClientRemoteComm::ReadDataIntoBuffer()
{
	// Create buffer if necessary
	if (m_reply_buf.Capacity() == 0)
	{
		int buffer_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
		if (buffer_size <= 1)
			buffer_size = 1024; // TODO: Is this check really necessary?
		if (!m_reply_buf.Reserve(buffer_size))
			return OpStatus::ERR_NO_MEMORY;
	}

	char* reply_buf_ptr = m_reply_buf.CStr();
	m_reply_buf_length = ReadData(reply_buf_ptr, m_reply_buf.Capacity() - 1);

	// Handle empty replies
	if (m_reply_buf_length == 0)
	{
		m_empty_replies++;
		if (m_empty_replies > GetMaxEmptyNetwork())
			return OnEmptyNetworkBuffer();
	}
	else
	{
		m_empty_replies = 0;
	}

	// Terminate the data
	m_reply_buf[m_reply_buf_length] = 0;

	return OpStatus::OK;
}


void ClientRemoteComm::RemoveNullFromBuffer()
{
	char* reply_buf = m_reply_buf.CStr();
	for (UINT32 i = 0; i < m_reply_buf_length; i++)
	{
		if (reply_buf[i] == '\0')
			reply_buf[i] = ' ';
	}
}

*/

OP_STATUS ClientRemoteComm::SendData(char* buf, size_t buflen)
{
	if (!m_connection)
		return OpStatus::ERR_NULL_POINTER;

	return m_connection->Send(buf, buflen);
}


OP_STATUS ClientRemoteComm::SendData(const OpStringC8& buf)
{
	if (!m_connection)
		return OpStatus::ERR_NULL_POINTER;

	int buflen = buf.Length();

	// Make a copy of the buffer, since Send will take ownership
	char* buf_copy = OP_NEWA(char, buflen + 1);
	if (!buf_copy)
		return OpStatus::ERR_NO_MEMORY;

	op_strcpy(buf_copy, buf.CStr());

	return m_connection->Send(buf_copy, buflen);
}


BOOL ClientRemoteComm::StartTLSConnection()
{
	if (!m_connection)
		return FALSE;

	return (m_connection->StartTLS() == OpStatus::OK);
}


char* ClientRemoteComm::AllocMem(size_t size)
{
	if (!m_connection)
		return NULL;

	return m_connection->AllocBuffer(size);
}


void ClientRemoteComm::FreeMem(char* mem)
{
	if (m_connection)
		m_connection->FreeBuffer(mem);
}


OP_STATUS ClientRemoteComm::StopLoading()
{
    MessageEngine::GetInstance()->GetGlueFactory()->DeleteCommGlue(m_connection);
    m_connection = NULL;

	return OpStatus::OK;
}


GlueFactory* ClientRemoteComm::GetFactory()
{
	GlueFactory* factory = 0;

	MessageEngine* engine = MessageEngine::GetInstance();
	if (engine != 0)
		factory = engine->GetGlueFactory();

	OP_ASSERT(factory != 0);
	return factory;
}

#endif //M2_SUPPORT
