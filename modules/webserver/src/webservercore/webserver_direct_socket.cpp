/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2009-2010 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)

#include "modules/webserver/webserver_direct_socket.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/webserver-api.h"

// FIXME

WebserverDirectClientSocket::WebserverDirectClientSocket(BOOL is_owner)
	: m_webserver_connection_listener(FALSE, FALSE)
	, m_server_socket(NULL)
	, m_client_socket_listener(NULL)
	, m_mh(g_main_message_handler)
	, m_connected(FALSE)
	, m_data_from_client(NULL)
	, m_data_from_client_length(0)
	, m_data_from_client_length_sent(0)
	, m_data_to_client(NULL)
	, m_data_to_client_length(0)
	, m_data_to_client_length_sent(0)
	, m_is_owner(is_owner)
	//, m_magic_number(WEB_SERVER_MAGIC_NUMBER)
{
	WebServerConnectionListener *lsn=g_webserver?reinterpret_cast<WebServerConnectionListener *>(g_webserver->m_connectionListener.GetPointer()):NULL;
	
	OP_ASSERT(g_webserver);
	
	if(lsn)
	{
		OP_ASSERT(lsn->GetReferenceObjectPtr());
		
		m_webserver_connection_listener.AddReference(lsn->GetReferenceObjectPtr());
	}
}

WebserverDirectClientSocket::~WebserverDirectClientSocket()
{
//	OP_ASSERT(m_magic_number==WEB_SERVER_MAGIC_NUMBER);
	
//	if(m_magic_number!=WEB_SERVER_MAGIC_NUMBER)
//		return;
	
	m_mh->RemoveCallBacks(this, Id());
	m_mh->UnsetCallBacks(this);
	Close();
}

/* static */ OP_STATUS WebserverDirectClientSocket::CreateDirectClientSocket(WebserverDirectClientSocket** socket, OpSocketListener* listener, BOOL ssl_connection, BOOL is_owner)
{	
	OP_ASSERT(ssl_connection == FALSE && "DOES NOT MAKE SENSE TO USE SSL ON DIRECT SOCKET");
	OpAutoPtr<WebserverDirectClientSocket> new_socket(OP_NEW(WebserverDirectClientSocket, (is_owner)));
	
	if (!new_socket.get())
		return OpStatus::ERR_NO_MEMORY;
	
    static const OpMessage messages[] =
    { 
     	 MSG_WEBSERVER_MESSAGE_CONNECT_ERROR,
     	 MSG_WEBSERVER_MESSAGE_DATA_READY,
     	 MSG_WEBSERVER_MESSAGE_DATA_READY_FOR_SENDING,
     	 MSG_WEBSERVER_MESSAGE_ON_DATA_SENT,
     	 MSG_WEBSERVER_MESSAGE_DATA_READY_NOTIFY_AGAIN
    };
    
    new_socket->m_client_socket_listener = listener;
    new_socket->m_mh->SetCallBackList(new_socket.get(), new_socket->Id(), messages, ARRAY_SIZE(messages));
    *socket = new_socket.release();
    return OpStatus::OK;
}

/* virtual */ void WebserverDirectClientSocket::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg) {
		case MSG_WEBSERVER_MESSAGE_DATA_READY_FOR_SENDING:
			if (m_server_socket)
				m_server_socket->DataFromClient(m_data_from_client, m_data_from_client_length);
			break;
			
		case MSG_WEBSERVER_MESSAGE_DATA_READY_NOTIFY_AGAIN:
			if (m_server_socket)
				m_server_socket->NotifyClientAgain();
			break;
		
		case MSG_WEBSERVER_MESSAGE_ON_DATA_SENT:
			if (m_client_socket_listener)
				m_client_socket_listener->OnSocketDataSent(this, static_cast<UINT>(par2));
				break;
				
		case MSG_WEBSERVER_MESSAGE_DATA_READY:
			if (m_client_socket_listener)
				m_client_socket_listener->OnSocketDataReady(this);
				break;

		case MSG_WEBSERVER_MESSAGE_CONNECT_ERROR:
			if (m_client_socket_listener)
				m_client_socket_listener->OnSocketConnectError(this,  static_cast<OpSocket::Error>(par2));

			break;
			
		default:
			break;
	}
	
}	

/* virtual */ OP_STATUS WebserverDirectClientSocket::Connect(OpSocketAddress* socket_address)
{
	/* We might put in a check that address is localhost or alien proxy here */
	
	if (m_client_socket_listener && m_webserver_connection_listener.GetPointer())
		reinterpret_cast<WebServerConnectionListener *>(m_webserver_connection_listener.GetPointer())->OnSocketDirectConnectionRequest(this, m_is_owner);
	return OpStatus::OK;
}

	
/* virtual */ OP_STATUS WebserverDirectClientSocket::Send(const void* data, UINT length)
{
	/* Copy buffer */
	if (m_data_from_client)
	{
		if(!m_client_socket_listener)
			return OpStatus::ERR; // It should not happen... I would like to return OpSocket::SOCKET_BLOCKING, but it violates the interface of OpSocket
			
		m_client_socket_listener->OnSocketSendError(this, OpSocket::SOCKET_BLOCKING);
		
		return OpStatus::OK;
	}
	
	m_data_from_client =  data; 
	m_data_from_client_length = length; 
	m_mh->PostMessage(MSG_WEBSERVER_MESSAGE_DATA_READY_FOR_SENDING, Id(), 0);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS WebserverDirectClientSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	if (m_data_to_client)
	{
		*bytes_received = MIN(length, m_data_to_client_length - m_data_to_client_length_sent);
		op_memcpy(buffer, reinterpret_cast<const char*>(m_data_to_client) + m_data_to_client_length_sent, *bytes_received);
		
		m_data_to_client_length_sent += *bytes_received;
		
		if (m_data_to_client_length_sent >= m_data_to_client_length)
		{
			m_data_to_client_length_sent = 0;
			m_data_to_client_length = 0;
			m_data_to_client = NULL;
		}
		
		if (m_server_socket)
			m_server_socket->OnSocketDataSent(this, *bytes_received);		
	}
	else 
	{
		*bytes_received = 0;
		
		if(!m_client_socket_listener)
			return OpStatus::ERR; // It should not happen... I would like to return OpSocket::SOCKET_BLOCKING, but it violates the interface of OpSocket
			
		// If there is a connection, we assume that the operation is blocking
		if(m_server_socket)
			m_client_socket_listener->OnSocketSendError(this, OpSocket::SOCKET_BLOCKING);
		
		return OpStatus::OK;
	}
		
	
	return OpStatus::OK;
}

/* virtual */ void WebserverDirectClientSocket::Close()
{
	// Coverity detected that m_client_socket_listener->OnSocketClosed() can delete the 'this' pointer...
	//OP_ASSERT(m_magic_number==WEB_SERVER_MAGIC_NUMBER);
	
	//if(m_magic_number!=WEB_SERVER_MAGIC_NUMBER)
	//	return;
	
	m_connected = FALSE;	
	
	if(m_mh)
	{
		m_mh->UnsetCallBacks(this);
		m_mh->RemoveCallBacks(this, Id());
	}
	
	if (m_server_socket)
	{
		WebserverDirectServerSocket *tmp=m_server_socket;
		
		m_server_socket=NULL;
		tmp->OnSocketClosed(this);
	}
	
	if (m_client_socket_listener)
	{
		OpSocketListener *tmp=m_client_socket_listener;
		
		m_client_socket_listener=NULL;
		tmp->OnSocketClosed(this);
	}
}


/* virtual */ OP_STATUS WebserverDirectClientSocket::GetSocketAddress(OpSocketAddress* socket_address)
{
	return socket_address->FromString(UNI_L("127.0.0.1"));
}

#ifdef OPSOCKET_GETLOCALSOCKETADDR
/* virtual */ OP_STATUS WebserverDirectClientSocket::GetLocalSocketAddress(OpSocketAddress* socket_address)
{	
	return OpStatus::ERR_NOT_SUPPORTED;
}
/* virtual */ OP_STATUS WebserverDirectServerSocket::GetLocalSocketAddress(OpSocketAddress* socket_address)
{	
	return OpStatus::ERR_NOT_SUPPORTED;
}

#endif // OPSOCKET_GETLOCALSOCKETADDR

void WebserverDirectClientSocket::OnSocketConnectError(OpSocket::Error error)
{
	m_mh->PostMessage(MSG_WEBSERVER_MESSAGE_CONNECT_ERROR, Id(), error);
}

void WebserverDirectClientSocket::OnSocketDataReady(WebserverDirectServerSocket* socket)
{
	OP_ASSERT(socket == m_server_socket);
	if (m_server_socket)
	{
		m_server_socket->GetDataToClient(m_data_to_client, m_data_to_client_length);
		m_data_to_client_length_sent = 0;
		m_mh->PostMessage(MSG_WEBSERVER_MESSAGE_DATA_READY, Id(), 0);
	}
}

void WebserverDirectClientSocket::OnSocketDataSent(WebserverDirectServerSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(m_data_from_client && m_data_from_client_length); 
	OP_ASSERT(socket == m_server_socket);
	OP_ASSERT(m_data_from_client_length_sent < m_data_from_client_length);
	
	m_data_from_client_length_sent += bytes_sent;
	
	if (m_data_from_client_length_sent >= m_data_from_client_length)
	{
			UINT num_bytes_sent=m_data_from_client_length_sent;
			
			m_data_from_client_length_sent = 0;
			m_data_from_client_length = 0;
			m_data_from_client = NULL;
			
			m_mh->PostMessage(MSG_WEBSERVER_MESSAGE_ON_DATA_SENT, Id(), static_cast<MH_PARAM_2>(num_bytes_sent));
	}
}

void WebserverDirectClientSocket::OnSocketClosed(WebserverDirectServerSocket* socket)
{
	OP_ASSERT(socket == m_server_socket);
	m_server_socket = NULL;
	Close();
}

/* virtual */ void WebserverDirectClientSocket::SetListener(OpSocketListener* listener)
{	
	m_client_socket_listener = listener;
}

#ifdef SOCKET_LISTEN_SUPPORT
/* virtual */ OP_STATUS WebserverDirectClientSocket::Accept(WebserverDirectServerSocket* socket)
{	
	m_server_socket = socket;
	if (m_server_socket)
		m_server_socket->SetClientSocket(this);
	
	m_connected = TRUE;
	
	if (m_client_socket_listener)
		m_client_socket_listener->OnSocketConnected(this);
	
	return OpStatus::OK;
}
#endif // SOCKET_LISTEN_SUPPORT


/* WebserverDirectServerSocket */

WebserverDirectServerSocket::WebserverDirectServerSocket()
	: ref_obj_conn(this)
	, m_client_socket(NULL)
	, m_server_socket_listener(NULL)
	, m_connected(TRUE)
	, m_data_from_client(NULL)
	, m_data_from_client_length(0)
	, m_data_from_client_length_sent(0)
	, m_data_to_client(NULL)
	, m_data_to_client_length(0)
	, m_data_to_client_length_sent(0)
	//, m_magic_number(WEB_SERVER_MAGIC_NUMBER)
{


}

WebserverDirectServerSocket::~WebserverDirectServerSocket()
{
//	OP_ASSERT(m_magic_number==WEB_SERVER_MAGIC_NUMBER);
	
//	if(m_magic_number!=WEB_SERVER_MAGIC_NUMBER)
//		return;
		
	Close();
}

/* static */ OP_STATUS WebserverDirectServerSocket::CreateDirectServerSocket(WebserverDirectServerSocket** socket, /*OpSocketListener* listener,*/ BOOL secure)
{
	OP_ASSERT(secure == FALSE && "DOES NOT MAKE SENSE TO USE SSL ON DIRECT SOCKET");
	OpAutoPtr<WebserverDirectServerSocket> new_socket(OP_NEW(WebserverDirectServerSocket, ()));
	
	if (!new_socket.get())
		return OpStatus::ERR_NO_MEMORY;
    //new_socket->m_server_socket_listener = listener;
    *socket = new_socket.release();
    return OpStatus::OK;
}

void WebserverDirectServerSocket::SendConnectionError(OpSocket::Error error_code)
{
	m_server_socket_listener->OnSocketConnectError(this, error_code);
}

void WebserverDirectServerSocket::DataFromClient(const void* data, UINT length)
{
	m_data_from_client = data;
	m_data_from_client_length = length;
	m_data_from_client_length_sent = 0;
	if(m_server_socket_listener)
		m_server_socket_listener->OnSocketDataReady(this);
}

void WebserverDirectServerSocket::NotifyClientAgain()
{
	if(m_data_from_client && m_server_socket_listener)
		m_server_socket_listener->OnSocketDataReady(this);
}

void WebserverDirectServerSocket::GetDataToClient(const void *&data, UINT &length)
{
	data = m_data_to_client;
	length = m_data_to_client_length;
	m_data_to_client_length_sent = 0;
	/* Must have a callback from client socket when all data has been sent */
}

OP_STATUS WebserverDirectServerSocket::Send(const void* data, UINT length)
{
	/* Copy buffer */
	if (m_data_to_client)
	{
		if(m_server_socket_listener)
			m_server_socket_listener->OnSocketSendError(this, OpSocket::SOCKET_BLOCKING);
		return OpStatus::OK;
	}
	
	m_data_to_client =  data; 
	m_data_to_client_length = length;
	if (m_client_socket)
		m_client_socket->OnSocketDataReady(this);
	return OpStatus::OK;
}

OP_STATUS WebserverDirectServerSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	if (m_data_from_client)
	{
		*bytes_received = MIN(length, m_data_from_client_length - m_data_from_client_length_sent);
		op_memcpy(buffer, reinterpret_cast<const char*>(m_data_from_client) + m_data_from_client_length_sent, *bytes_received);
		
		m_data_from_client_length_sent += *bytes_received;
		
		if (m_data_from_client_length_sent >= m_data_from_client_length)
		{
			m_data_from_client_length_sent = 0;
			m_data_from_client_length = 0;
			m_data_from_client = NULL;
		}
		if (m_client_socket)
		{
			m_client_socket->OnSocketDataSent(this, *bytes_received);		
			
			if(m_data_from_client)
				m_client_socket->m_mh->PostMessage(MSG_WEBSERVER_MESSAGE_DATA_READY_NOTIFY_AGAIN, m_client_socket->Id(), 0);
		}
	}
	else 
	{
		*bytes_received = 0;
		
		if(!m_server_socket_listener)
			return OpStatus::ERR; // It should not happen... I would like to return OpSocket::SOCKET_BLOCKING, but it violates the interface of OpSocket
			
		if(m_client_socket) // If there is a connection, we assume that the operation is blocking
			m_server_socket_listener->OnSocketSendError(this, OpSocket::SOCKET_BLOCKING);
	}
	return OpStatus::OK;	
}

void WebserverDirectServerSocket::Close()
{
	// Coverity detected that m_server_socket_listener->OnSocketClosed() can delete the 'this' pointer...
	//OP_ASSERT(m_magic_number==WEB_SERVER_MAGIC_NUMBER);
	
	//if(m_magic_number!=WEB_SERVER_MAGIC_NUMBER)
	//	return;
	
	m_connected = FALSE;
	
	if (m_client_socket)
	{
		WebserverDirectClientSocket *tmp=m_client_socket;
		
		m_client_socket = NULL;
		tmp->OnSocketClosed(this);
	}
	
	if (m_server_socket_listener)
	{
		OpSocketListener *tmp=m_server_socket_listener;
		
		m_server_socket_listener=NULL;
		tmp->OnSocketClosed(this);
	}
}

OP_STATUS WebserverDirectServerSocket::GetSocketAddress(OpSocketAddress* socket_address)
{
	return socket_address->FromString(UNI_L("127.0.0.1"));	
}

void WebserverDirectServerSocket::OnSocketDataSent(WebserverDirectClientSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(socket == m_client_socket);
	
	m_data_to_client_length_sent += bytes_sent;
	if (m_data_to_client_length_sent >= m_data_to_client_length)
	{
		m_data_to_client_length = 0;
		m_data_to_client_length_sent = 0;
		m_data_to_client = NULL;
	}
	if (m_server_socket_listener)
		m_server_socket_listener->OnSocketDataSent(this, bytes_sent);	
}

void WebserverDirectServerSocket::OnSocketClosed(WebserverDirectClientSocket* socket)
{
	OP_ASSERT(socket == m_client_socket);
	m_client_socket = NULL;
	Close();
}

#endif //  WEBSERVER_DIRECT_SOCKET_SUPPORT
