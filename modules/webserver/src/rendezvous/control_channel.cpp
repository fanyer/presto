/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2011
 *
 * Web server implementation 
 */

#include "core/pch.h"

/* FIXME : This code needs a total rewrite. all new RSVP connection sockets up the proxy should be handled by ConnectData and not control channel*/

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

#include "modules/hardcore/mh/mh.h"
#include "modules/idna/idna.h"

#include "modules/libcrypto/include/CryptoHash.h"


#include "modules/url/uamanager/ua.h"
#include "modules/about/operaversion.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/url/url_socket_wrapper.h"

#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"

#include "modules/webserver/src/rendezvous/control_channel.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/webserver/webserver_resources.h" 
#include "modules/about/operaabout.h"

#ifdef CORE_GOGI
#include "platforms/core/aboutopera.h"
#include "modules/pi/OpSystemInfo.h"
#endif

#define RENDEZVOUS_PROTOCOL_VERSION_STRING "UCP/1.0"
#define RENDEZVOUS_PROTOCOL_VERSION (1.0)

//#define DEBUG_COMM_HEXDUMP
//#define DEBUG_COMM_FILE
//#define DEBUG_COMM_FILE_RECV

#include "modules/olddebug/tstdump.h"

static BOOL startsWith(const char *s, const char *with)
{
	return (op_strstr(s, with) == s);
}

// if the result is nonzero then the first char is guaranteed not to be ' '
static const char *secondToken(const char *s)
{
	if (!s)
		return s;
	while (*s == ' ')
		s++; // skip leading space
	while (*s && (*s != ' '))
		s++; // skip first token
	while (*s == ' ')
		s++; // skip space
	if (! *s)
		return 0;
	return s;
}

#ifdef DEBUG_ENABLE_OPASSERT
void DEBUG_VERIFY_SOCKET(OpSocket *socket)
{
	// Just test if the address is valid...
	OpSocketAddress *temp_addr;
	
	if(socket && OpStatus::IsSuccess(OpSocketAddress::Create(&temp_addr)))
	{
		if(OpStatus::IsSuccess(socket->GetSocketAddress(temp_addr)))
		{
			UINT port=temp_addr->Port();
			
			OP_ASSERT(port!=0xFFFF); // Even if this assert pop-up, it is not a problem. A crash is a problem...
		}
	}
}
#endif 

SocketAndBuffer::SocketAndBuffer(OpSocket *socket)
:	m_socket(socket),
	current_buffer(NULL),
	sending_in_progress(FALSE)
{
	DEBUG_VERIFY_SOCKET(m_socket);
}

SocketAndBuffer::~SocketAndBuffer()
{
	OP_DELETE(m_socket);
	m_socket = NULL;
	OP_DELETE(current_buffer);
	current_buffer = NULL;
}

void SocketAndBuffer::Close()
{
	if (m_socket)
	{
		m_socket->Close();
		OP_DELETE(m_socket);
		m_socket = NULL;

	}
}

OP_STATUS SocketAndBuffer::SendData(const OpStringC8 &string, uint32 len)
{
	if(string.IsEmpty() || len == 0)
		return OpStatus::OK;

	char *temp_buffer = OP_NEWA(char, len);
	if(temp_buffer == NULL)
		return OpStatus::ERR_NO_MEMORY;
	op_memcpy(temp_buffer, string.CStr(), len);

	Comm::Comm_strings *item = OP_NEW(Comm::Comm_strings, (temp_buffer, len));
	if(item == NULL)
	{
		OP_DELETEA(temp_buffer);
		return OpStatus::ERR_NO_MEMORY;
	}
	item->Into(&buffers);
	temp_buffer = NULL;

	return SendDataToConnection();
}

OP_STATUS SocketAndBuffer::SendDataToConnection()
{
	if (m_socket == NULL)
		return OpStatus::ERR_NULL_POINTER;

	sending_in_progress = TRUE;
	while (current_buffer || !buffers.Empty())
    {
		if(!current_buffer)
		{
			current_buffer = (Comm::Comm_strings *) buffers.First();
			OP_ASSERT(current_buffer);
			
			if(!current_buffer)
				return OpStatus::ERR_NULL_POINTER;
				
			current_buffer->Out();
		}

		if(current_buffer->buffer_sent)
			break; // waiting for data to be sent by socket

#ifdef DEBUG_COMM_HEXDUMP
		{
			OpString8 textbuf;

			textbuf.AppendFormat("SocketAndBuffer::SendDataToConnection Sending data to socket from %d Tick %lu",(int) this,(unsigned long) g_op_time_info->GetWallClockMS());
			DumpTofile((unsigned char *) current_buffer->string,(unsigned long) current_buffer->len,textbuf,"winsck.txt");
		}
#endif
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] SocketAndBuffer::SendDataToConnection():  %p\n", (int) this, current_buffer->string);
		PrintfTofile("winsck2.txt","[%d] SocketAndBuffer::SendDataToConnection():  %p\n", (int) this, current_buffer->string);
#endif

#if defined(DEBUG_COMM_STAT) || defined (DEBUG_COMM_FILE)
		unsigned long buf_len = current_buffer->len;
#endif
#ifdef DEBUG_ENABLE_OPASSERT
		Comm::Comm_strings* original = current_buffer;
#endif
		/* send request to host */
		current_buffer->buffer_sent = TRUE;
		//unsigned int send_length = current_buffer->len;
		OP_STATUS result = m_socket->Send(current_buffer->string, current_buffer->len);

#ifdef DEBUG_COMM_STAT
		if(current_period != prefsManager->CurrentTime())
		{
			if(current_period)
				PrintfTofile("statsend.txt", "%lu, %lu, %lu\n",current_period, bytes_sent_count, bytes_recv_count);

			current_period = prefsManager->CurrentTime();
			bytes_recv_count = bytes_sent_count = 0;
		}
		if (result == OpStatus::OK)
			bytes_sent_count += buf_len;
#endif

		if (result != OpStatus::OK)
		{
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] SocketAndBuffer::SendDataToConnection(): blocked %p\n", (int) this, (current_buffer ? current_buffer->string : NULL));
#endif
			OP_ASSERT(current_buffer == NULL || original == current_buffer);
			if(current_buffer)
				current_buffer->buffer_sent = FALSE;
            // Assumption that the only error from a Send could be 'blocking'.
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] SocketAndBuffer::SendDataToConnection() - WSAEWOULDBLOCK, waiting\n", (int) this);
#endif
			sending_in_progress = FALSE;
			return result;
		}

#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] SocketAndBuffer::SendDataToConnection() - sent=%d\n", (int) this, buf_len);
#endif
    }

    sending_in_progress = FALSE;

	return OpStatus::OK;
}

void SocketAndBuffer::SocketDataSent()
{
	if(current_buffer)
	{
		OP_DELETE(current_buffer);
		current_buffer = NULL;
	}

	OpStatus::Ignore(SendDataToConnection());
}

ConnectData::ConnectData(UINT clientID, OpSocket *socket) 
	: SocketAndBuffer(socket),
	m_clientID(clientID)
{
}

ConnectData::~ConnectData()
{
}

/* static */
OP_STATUS ConnectData::Make(ConnectData *&connectData, UINT clientID, const OpStringC8 &nonce, const OpStringC8 &client_ip, OpSocket *socket)
{
	connectData = OP_NEW(ConnectData, (clientID, socket));

	if (connectData == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(connectData->m_nonce.Set(nonce));
	RETURN_IF_ERROR(connectData->m_client_ip.Set(client_ip));
	return OpStatus::OK;
}

ControlChannel::ControlChannel()
	: SocketAndBuffer(NULL)
	, m_sendingKeepAlive(FALSE)
	, m_owner(0)
	, m_addr_ucp(0)
	, m_resolver(0)
	, m_buffer(0)
	, m_bufPos(0)
	, m_logger(0)
	, m_is_connected(FALSE)
	, m_error_callback_sent(FALSE)
	, m_waiting_for_200_ok(FALSE)
	, m_waiting_for_challenge(FALSE)
	, m_has_sent_rendezvous_connected(FALSE)
	, m_keepalive_timeout(0)
{
}

/**
 * Maintains a persistent TCP connection to the Alien proxy
 * Periodically sends REGISTER messages
 * Waits for INVITE messages
 * Responds with RSVP messages (to the HTTP proxy)
 *
 */
/* static*/ OP_STATUS ControlChannel::Create(ControlChannel **channel, WebserverRendezvous *owner, const char *shared_secret)
{		
	*channel = NULL;
	
	ControlChannel *cc = OP_NEW(ControlChannel, ());
	
	OpAutoPtr<ControlChannel> temp_cc_anchor(cc);
	
	if (cc == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;		
	}

	cc->m_connection_timer.SetTimerListener(cc);
	
	cc->m_owner = owner;
	
	RETURN_IF_ERROR(RendezvousLog::Make(cc->m_logger));
	
	cc->m_buffer = OP_NEWA(char, WEB_RDV_BUFSIZE + 1);

	if (!cc->m_buffer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	cc->m_buffer[0] = '\0';

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(cc, MSG_WEBSERVER_REGISTER, 0));

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(cc, MSG_WEBSERVER_RDV_DATA_PENDING, 0));

	RETURN_IF_ERROR(SocketWrapper::CreateHostResolver(&cc->m_resolver, cc));
	RETURN_IF_ERROR(OpSocketAddress::Create(&cc->m_addr_ucp));
	
	RETURN_IF_ERROR(cc->m_sharedSecret.Set(shared_secret));

	if (!g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, 0))
		return OpStatus::ERR_NO_MEMORY;
	
	*channel = temp_cc_anchor.release();
	
	RETURN_IF_ERROR(g_opera_account_manager->AddListener(cc));
	
	return OpStatus::OK;
}

void DeleteControlChannelKeyData(const void *key, void * data)
{
	OP_ASSERT(data && ((ConnectData *)data)->m_socket == (OpSocket *)key);
	
	/*if(!data || ((ConnectData *)data)->m_socket!=(OpSocket *)key)
		OP_DELETE((OpSocket *)key);*/
		
	OP_DELETE((ConnectData *)data);
}


ControlChannel::~ControlChannel()
{
	if (g_opera_account_manager) // This may happen after OperaAuthModule::Destroy()
		g_opera_account_manager->RemoveListener(this);
	g_main_message_handler->RemoveCallBacks(this, 0);
	OP_DELETE(m_logger);
	OP_DELETE(m_addr_ucp);

	OP_DELETE(m_resolver);
	OP_DELETEA(m_buffer);
	
	m_connectSockets.ForEach(DeleteControlChannelKeyData);
	m_connectSockets.RemoveAll();
}

BOOL ControlChannel::IsConnected()
{
	return m_is_connected;
}

void ControlChannel::OnAccountDeviceRegisteredChange()
{
	if (IsConnected())
		Close();
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, WEB_RDV_RETRY_PERIOD);
}

void ControlChannel::OnHostResolved(OpHostResolver* resolver)
{
	m_logger->clear(8);
	unsigned short proxyPort = (unsigned short)g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverProxyPort);
	if (OpStatus::IsError(resolver->GetAddress(m_addr_ucp, 0)))
	{
		// FIXME: why would this ever fail?
		goto failed;
	}
	m_logger->clear(7);
	m_addr_ucp->SetPort(proxyPort);
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::CONNECT, 0);
	return;
failed: 
	m_logger->logOnce(7, "out of memory in ControlChannel::OnHostResolved()");
}

OP_STATUS ControlChannel::Rsvp(int id, int error_code)
{
	OpString8 response;

	const char *fmt = "%s %d %d" CRLF CRLF;
	RETURN_IF_ERROR(response.AppendFormat(fmt, RENDEZVOUS_PROTOCOL_VERSION_STRING, error_code, id));
	m_logger->clear(9);
	return Send(response, response.Length());
}

OP_STATUS ControlChannel::CheckPort(unsigned int port_to_check)
{
	if (m_has_sent_rendezvous_connected && port_to_check && m_owner->m_last_checked_port != port_to_check)
	{
		m_owner->m_last_checked_port = port_to_check;
		OpString8 request;
		RETURN_IF_ERROR(request.AppendFormat("CHECKPORT %d %s" CRLF CRLF, port_to_check, RENDEZVOUS_PROTOCOL_VERSION_STRING));
		return Send(request, request.Length());
	}
	return OpStatus::OK;
}

OP_STATUS ControlChannel::KeepAlive()
{
 	OpString8 request;
	OpString8 uriBuf;
	RETURN_IF_ERROR(uriBuf.SetUTF8FromUTF16(g_webserver->GetWebserverUriIdna()));
	RETURN_IF_ERROR(request.AppendFormat("KEEPALIVE %s %s" CRLF CRLF, uriBuf.CStr(), RENDEZVOUS_PROTOCOL_VERSION_STRING));

	if (OpStatus::IsSuccess(Send(request, request.Length())))
	{
		m_connection_timer.Start(30000);
		m_waiting_for_200_ok = TRUE;
		m_waiting_for_challenge = FALSE;

	} /* Ignore error code from send, since send() handle the network errors */

	m_sendingKeepAlive = TRUE;
	
	g_main_message_handler->RemoveDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::KEEPALIVE);
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::KEEPALIVE, (m_keepalive_timeout) ? m_keepalive_timeout : WEB_RDV_KEEPALIVE_PERIOD);
	
	return OpStatus::OK;
}
	
OP_STATUS ControlChannel::Reg()
{
	OpString8 uriBuf;
	OpString8 request;
	OP_STATUS status;

	char *UserAgentStr = (char *) g_memory_manager->GetTempBuf2k();
	int ua_str_len = g_uaManager->GetUserAgentStr(UserAgentStr, g_memory_manager->GetTempBuf2kLen() -1, NULL, NULL,
												UA_Opera);
	UserAgentStr[ua_str_len] = '\0';

	if (
			OpStatus::IsError(status = uriBuf.SetUTF8FromUTF16(g_webserver->GetWebserverUriIdna())) ||
			OpStatus::IsError(status = m_sharedSecret.SetUTF8FromUTF16(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword).CStr())) ||
			OpStatus::IsError(status = request.AppendFormat("REGISTER %s %s\r\nUser-Agent: %s %s\r\n\r\n", uriBuf.CStr(), RENDEZVOUS_PROTOCOL_VERSION_STRING, UserAgentStr, VER_BUILD_IDENTIFIER))
		)
	{
		goto reg_failed;
	}

	m_logger->clear(2);
	
	if (OpStatus::IsError(status = Send(request, request.Length())))
	{
		goto reg_failed;
	}

	m_connection_timer.Start(30000);
	m_waiting_for_200_ok = FALSE;
	m_waiting_for_challenge = TRUE;

	return status;
	
reg_failed: 
	m_logger->logOnce(2, "out of memory in ControlChannel::Reg()");
	return status;
}

OP_STATUS ControlChannel::Encrypt(const OpStringC8 &nonce, const OpStringC8 &private_key, OpString8 &output)
{
	OpAutoPtr<CryptoHash> hash_A1(CryptoHash::CreateMD5());
	if (!hash_A1.get())
		return OpStatus::ERR_NO_MEMORY;

	hash_A1->InitHash();
	OP_ASSERT(private_key.HasContent());

	hash_A1->CalculateHash(private_key.CStr());
	hash_A1->CalculateHash(nonce.CStr());

	const int len = hash_A1->Size() * 2 + 1;
	char *buffer = output.Reserve(len);

	if (buffer == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	const char hexchars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	
	byte digest[16]; /* ARRAY OK 2008-12-12 haavardm */
	OP_ASSERT(hash_A1->Size() == 16);
	
	hash_A1->ExtractHash(digest);
	
	int length = hash_A1->Size(); 
	char *pos = buffer;
	int i;
	for(i = 0; i < length; i++)
	{
		unsigned char val = digest[i];
		*(pos++) = hexchars[(val>>4) & 0x0f];
		*(pos++) = hexchars[val & 0x0f];
	}
	
	*(pos++) = '\0';

	return OpStatus::OK;
}

/* virtual */ void ControlChannel::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_connection_timer)
	{
		if (m_waiting_for_200_ok == TRUE)
		{
			/* If 200 ok has not been returned from proxy after we sent keepalive, there is a network problem */
			Close();
			m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);	
		}
		else if (m_waiting_for_challenge == TRUE)
		{
			/* If a challengehas not been returned from proxy after we sent REGISTER, there is a network problem */
			Close();
			m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);	
		}
	}
}
	
void ControlChannel::OnSocketConnected(OpSocket* sock)
{
	OpString8 crypto;
	OpString8 uriBuf;
	if (sock != m_socket) /* new RSVP connection */
	{
		OpString8 request;
		ConnectData *cd = 0;
		OP_STATUS status = OpStatus::OK;
		if (OpStatus::IsError(status = m_connectSockets.GetData(sock, &cd)))
		{
			OP_ASSERT(!"Socket does not exist");
			return;
		}

		OP_ASSERT(cd);
		OP_ASSERT(cd->m_clientID);
		OP_ASSERT(cd->m_nonce.HasContent());
		
		if (cd->m_nonce.Length() < 20)
			goto fail;

		if (OpStatus::IsError(uriBuf.SetUTF8FromUTF16(g_webserver->GetWebserverUriIdna())))
			goto oom;
		if (OpStatus::IsError(Encrypt(cd->m_nonce, m_sharedSecret, crypto)))
			goto oom;
		if (OpStatus::IsError(request.AppendFormat("RSVP %s %s\r\nId: %d\r\nDigest: %s\r\n\r\n", uriBuf.CStr(), RENDEZVOUS_PROTOCOL_VERSION_STRING, cd->m_clientID, crypto.CStr())))
			goto oom;
		if (OpStatus::IsMemoryError(cd->SendData(request, request.Length())))
		{			
			goto send_error;
		}
		return;
oom: 
		RendezvousLog::log("OOM in OnSocketConnected()");
		goto fail;
send_error: 
		RendezvousLog::log("Send error in OnSocketConnected()");
		goto fail;
fail: 

		OpStatus::Ignore(m_connectSockets.Remove(sock, &cd));
		if (OpStatus::IsError(Rsvp(cd->m_clientID, 510)))
		{
			if (OpStatus::IsMemoryError(status))
			{
				m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
			}
			else
			{
				m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
			}
		}

		OP_DELETE(cd);
	}
	else /* control channel connected */
	{
		// FIXME: make sure we don't have to do anything with m_connectSockets
		m_logger->clear(6);
		m_bufPos = 0;
		OP_STATUS status;
		if (OpStatus::IsError(status = Reg()))
		{
			Close();
			if (OpStatus::IsMemoryError(status))
			{
				m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
			}
			else
			{
				m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
			}
		}
	}
}

void ControlChannel::OnHostResolverError(OpHostResolver* resolver, OpHostResolver::Error error)
{
	OP_ASSERT(resolver == m_resolver);
	m_logger->logOnce(8, "Unite proxy hostname resolution failed");
	
	RendezvousStatus status;
	switch (error) {
		case OpHostResolver::HOST_NOT_AVAILABLE:
		case OpHostResolver::HOST_ADDRESS_NOT_FOUND:
			status = RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN;
			break;

		default:
			status = RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK;
			break;
	}
	m_error_callback_sent = TRUE;
	
	m_owner->SendRendezvousConnectError(status);	
}

void ControlChannel::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	if (socket != m_socket)
	{
		OP_ASSERT(m_connectSockets.Contains(socket) == TRUE);
		ConnectData *connectData;
		m_connectSockets.Remove(socket, &connectData);
		OP_ASSERT(connectData);

		OP_STATUS status;
		if (OpStatus::IsError(status = Rsvp(connectData->m_clientID, 511)))
		{
			if (OpStatus::IsMemoryError(status))
			{
				m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
			}
			else
			{
				m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
			}
			return;
		}

		//OP_DELETE(socket); /* FIXME: Should we delete socket here ?*/
		OP_DELETE(connectData);
	}
	else
	{
		m_is_connected = FALSE;
		m_logger->logOnce(6, "could not connect to Opera Unite proxy");
		m_owner->OnSocketConnectError(socket, error);
	}
	
	m_error_callback_sent = TRUE;	
}

void ControlChannel::OnSocketDataSent(OpSocket* socket, UINT bytes_sent)
{
	if (socket != m_socket)
	{
		OP_ASSERT(m_connectSockets.Contains(socket));

		ConnectData *cd = 0;
		OP_STATUS status = OpStatus::OK;
		if (OpStatus::IsError(status = m_connectSockets.GetData(socket, &cd)))
		{
			OP_ASSERT(!"Socket does not exist");
			return;
		}

		if(cd)
			cd->SocketDataSent();
	}
	else
		SocketDataSent();
}

void ControlChannel::OnSocketSendError(OpSocket *socket, OpSocket::Error error)
{
	if (socket != m_socket)
	{
		
		OP_ASSERT(m_connectSockets.Contains(socket));
		RendezvousLog::log("could not send RSVP request to Opera Unite proxy");
		ConnectData *connectData;
		m_connectSockets.Remove(socket, &connectData);
		OP_ASSERT(connectData);

		//OP_DELETE(socket); /* FIXME: Should we delete socket here?*/
		OP_DELETE(connectData);
				
		m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
	}
	else
	{
		m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
	}
	
	m_error_callback_sent = TRUE;
}

/* returns FALSE if an illegal message was received */
ControlChannel::ReadStatus ControlChannel::ReadMessage(ControlMessage &resultp)
{
	OP_ASSERT(m_bufPos <= WEB_RDV_BUFSIZE);
	m_error_callback_sent = FALSE;

	char *endOfRequest;
	if ((endOfRequest = op_strstr(m_buffer, CRLF CRLF)) == NULL)
	{
		return ControlChannel::RS_ONGOING;
	}
	
	endOfRequest += 4; /* go to end of CRLF CRLF */
	
	int request_length =  endOfRequest - m_buffer;

	if (OpStatus::IsMemoryError(resultp.SetMessage(m_buffer, request_length - 4)))
	{
		return ControlChannel::RS_OOM;
	}
	
	// at this point we have created the message, and we shift the buffer
	int remaining = m_bufPos - request_length;
	
	OP_ASSERT(remaining >= 0);
	
	if (remaining > 0)
	{
		op_memmove(m_buffer, m_buffer + request_length, remaining);
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_RDV_DATA_PENDING, 0, 0, 0);
	}
	m_bufPos = remaining;	
	
	m_buffer[m_bufPos] = '\0';
	
	OP_ASSERT(m_bufPos <= WEB_RDV_BUFSIZE);
	
	return ControlChannel::RS_READ;
}

void ControlChannel::OnSocketClosed(OpSocket* socket)
{
	if (socket != m_socket)
	{
		/* Shouldn't we call the socket listener here ? */
		OP_ASSERT(m_connectSockets.Contains(socket));

		ConnectData *connectData;
		m_connectSockets.Remove(socket, &connectData);
		//OP_DELETE(socket); /* FIXME: Should we delete socket here?*/
		OP_DELETE(connectData);
	}
	else
	{
		RendezvousLog::log("Control channel closed by peer");
		m_logger->clear(1);
		if (m_socket)
		{
			Close();
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
		}
		
	}
}

void ControlChannel::OnSocketDataReady(OpSocket *socket)
{
	if (socket != m_socket)
	{

		OP_ASSERT(m_connectSockets.Contains(socket));

		ConnectData *connectData;
		m_connectSockets.Remove(socket, &connectData);
		OP_ASSERT(connectData);
		// FIXME: pendingConnections should not be a hashtable. Should be a vector instead */
		OP_ASSERT(m_owner->m_pendingConnections.GetCount() == 0);

		OP_STATUS status = OpStatus::OK;
		if (OpStatus::IsError(status = m_owner->m_pendingConnections.Add(socket, connectData)))
		{
			if (OpStatus::IsMemoryError(status))
				g_webserverPrivateGlobalData->SignalOOM();
			
			return; 
		}

		m_owner->m_rendezvousEventListener->OnRendezvousConnectionRequest(m_owner); /* sends OnSocketConnectionRequest to the owner of listener socket */

		if (m_owner->m_pendingConnections.Contains(socket)) /* WebserverRendezvous::Accept() was not called (OOM?) */
		{
			RendezvousLog::log("WebserverRendezvous::Accept() was not called on socket %x, id %d", socket, connectData->m_clientID);
			
			OpStatus::Ignore(m_owner->m_pendingConnections.Remove(socket, &connectData));
			OP_DELETE(connectData);
			//OP_DELETE(socket); /* FIXME: Can we delete this here?*/
		}
	}
	else
	{
		UINT bytesRead;
		OP_STATUS status;
		if (OpStatus::IsError(status = socket->Recv(m_buffer + m_bufPos, WEB_RDV_BUFSIZE - m_bufPos, &bytesRead))) 
		{
			if (OpStatus::IsMemoryError(status))
			{
				Close();
				m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
			}
			else
			{
				if(g_webserver->IsRunning())
					m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
			}
			return;
		}
		
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","ControlChannel::OnSocketDataReady() - recv %u bytes, Tick %lu\n", bytesRead, (unsigned long) g_op_time_info->GetWallClockMS());
#ifdef DEBUG_COMM_FILE_RECV
	DumpTofile((unsigned char *) m_buffer + m_bufPos, bytesRead, "Received data", "winsck.txt");
#endif
#endif
		m_bufPos += bytesRead;
		
		OP_ASSERT(m_bufPos <= WEB_RDV_BUFSIZE);
		
		m_buffer[m_bufPos] = '\0'; /* m_buffer has size WEB_RDV_BUFSIZE + 1, to assure space for '\0'. */
		
		ProcessBuffer();
	}
}

void ControlChannel::Close()
{
	SocketAndBuffer::Close();

	g_main_message_handler->RemoveDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::KEEPALIVE);
	m_has_sent_rendezvous_connected = FALSE;
	RendezvousLog::log("Control channel closed");
	m_connection_timer.Stop();
	m_bufPos = 0;
	m_buffer[0] = '\0';
	m_logger->clear(1);
	m_is_connected = FALSE;
	m_sendingKeepAlive = FALSE;
}

OP_STATUS ControlChannel::Send(const OpStringC8 &msg, unsigned int len)
{
	m_error_callback_sent = FALSE;
	if (!m_socket)
	{
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::CONNECT, WEB_RDV_RETRY_PERIOD);
		return OpStatus::OK;
	}
	
	OP_STATUS status;
	if (OpStatus::IsError(status = SendData(msg, len)))
	{
		RendezvousLog::log("send error on control connection");
		if (OpStatus::IsMemoryError(status))
		{
			Close();
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{		
			m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);	
		}
	}
	
	return status;
}

OP_STATUS ControlChannel::Connect()
{
	OP_STATUS status = OpStatus::OK;
	m_error_callback_sent = FALSE;
	m_logger->clear(5);
	
	if (m_addr_ucp->IsValid() != TRUE)
	{
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, WEB_RDV_RETRY_PERIOD);
		return OpStatus::OK;
	}

	
	if (m_socket == NULL)
	{
		if (OpStatus::IsError(status = SocketWrapper::CreateTCPSocket(&m_socket, this, SocketWrapper::ALLOW_ALL_WRAPPERS)))
		{
			m_logger->logOnce(5, "could not create socket for control channel");
			/* FIXME: Must also setup address */
		}

	}
		
	if (OpStatus::IsError(status = m_socket->Connect(m_addr_ucp)))
	{
		if (OpStatus::IsMemoryError(status) == FALSE && m_error_callback_sent == FALSE)
		{
			m_logger->logOnce(6, "could not connect to Opera Unite proxy");
			g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, WEB_RDV_RETRY_PERIOD);
			return OpStatus::OK;
		}
	}

	return status;
}


void ControlChannel::ProcessBuffer()
{
	m_waiting_for_challenge = FALSE;
	m_waiting_for_200_ok = FALSE;
	m_connection_timer.Stop();
	
	ControlMessage msg;
	ReadStatus rs = ReadMessage(msg);
	switch (rs)
	{
	case RS_ONGOING:
		break;

	case RS_OOM:
		RendezvousLog::log("OOM in ReadMessage()");
		Close();
		m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
		break;
		
	case RS_READ:
		ParseMessage(&msg);
		break;
		
	default:
		OP_ASSERT(rs);
		OP_ASSERT(FALSE);
	}
}

void ControlChannel::ParseMessage(ControlMessage *msg)
{
	OP_STATUS status = OpStatus::OK;
	ControlMessage::ParseStatus ps = msg->ParseMessage();

	if (ps == ControlMessage::PS_OOM)
	{
		RendezvousLog::log("OOM in ParseMessage()");
		Close();
		m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
		return; 
		
	}
	else if (ps == ControlMessage::PS_PARSE_ERROR)
	{
		RendezvousLog::log("parse error in ParseMessage()");
		Close();
		m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL);
		return;
	}
	
	
	OP_ASSERT(ps == ControlMessage::PS_OK);
	switch (msg->m_type)
	{
	
	case ControlMessage::MT_UCP_RESPONSE:
		OP_ASSERT(msg->m_statusCode < 600);

		if (msg->m_statusCode == 200)
		{
			m_logger->clear(0);
			m_logger->logOnce(1, "Connected to Opera Unite proxy");
			m_is_connected = TRUE;
			if (m_has_sent_rendezvous_connected == FALSE)
			{
				m_owner->m_rendezvousEventListener->OnRendezvousConnected(m_owner);
				m_has_sent_rendezvous_connected = TRUE;
				//CheckPort(m_owner->m_local_listening_port);  // TODO: Is this really required?
			}
			
			/* Check if port was open X-Port-Open: */			
			HeaderEntry *x_port_open_header = msg->GetHeaderList()->GetHeader("X-Port-Open");
			HeaderEntry *x_client_address_header = msg->GetHeaderList()->GetHeader("X-Client-Address");
			HeaderEntry *x_timeout = msg->GetHeaderList()->GetHeader("Timeout");
			
			if(x_timeout)
			{
				m_keepalive_timeout=op_atoi(x_timeout->Value())*1000;
			
			#ifdef _DEBUG
				//m_keepalive_timeout=5000; // Just to speed-up testing in debug
			#endif
			}
			
			if (x_port_open_header && x_client_address_header && op_strlen(x_client_address_header->Value()) > 4 && !op_stricmp(x_port_open_header->Value(), "yes"))
			{
				m_owner->SetDirectAccess(TRUE, x_client_address_header->Value());
			}
			
			if (m_sendingKeepAlive == FALSE)
			{
				m_sendingKeepAlive = TRUE;
				g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::KEEPALIVE, (m_keepalive_timeout) ? m_keepalive_timeout : WEB_RDV_KEEPALIVE_PERIOD);
			}
			/*
			else
			{
				m_owner->SetDirectAccess(FALSE, "0:0");
			}*/
		}
		else if (msg->m_statusCode == 400) // Bad request
		{
		}
		else if (msg->m_statusCode == 401)
		{
			Close();
			m_owner->SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_AUTH_FAILURE);
			break;
		}
		else if (msg->m_statusCode == 403) // Version not allowed to connect
		{
			Close();
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION);
		}
		else
		{

			Close();
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL);
			return;
		}
		break;
		
	case ControlMessage::MT_INVITE:
		if (m_connectSockets.GetCount() < (INT32)m_owner->m_backlog)
		{
			OpSocket *newSocket;
			status = SocketWrapper::CreateTCPSocket(&newSocket, this, SocketWrapper::ALLOW_ALL_WRAPPERS); /* FIXME: add this into ConnectData::Make? */
			if (status == OpStatus::OK)
			{
				// Support for the New Proxy: if possible, RSVP will be sent to the
				// HTTP Proxy instead to the UCP Proxy
				if(msg->m_addr_http)
					status = newSocket->Connect(msg->m_addr_http);
				else
					status = newSocket->Connect(m_addr_ucp);
			}
			
			OP_ASSERT(msg->m_nonce.HasContent());
			OP_ASSERT(msg->m_clientID);
			
			if (status)
			{
				RendezvousLog::log("Create/Connect failed for RSVP");
				if (OpStatus::IsError(status = Rsvp(msg->m_clientID, 512)))
				{
					Close();
					if (OpStatus::IsMemoryError(status))
					{
						m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
					}
					else
					{
						m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
					}					
					return;
				}
			}
			else
			{
				ConnectData *data = NULL;
				if (
						OpStatus::IsError(status = ConnectData::Make(data, msg->m_clientID, msg->m_nonce, msg->m_client_ip, newSocket)) ||
						(OpStatus::IsError(status = m_connectSockets.Add(newSocket, data)))
					)
				{
					OP_DELETE(data);
					OP_DELETE(newSocket);
					Close();
					m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
					return;
				}
			}
		}
		else
		{
			if (OpStatus::IsError(Rsvp(msg->m_clientID, 513)))
			{
				Close();
				if (OpStatus::IsMemoryError(status))
				{
					m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);	
				}
				else
				{
					m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
				}				
			}			
			/* FIXME: Error code */
		}
		break;
		
	case ControlMessage::MT_CHALLENGE:
		OP_ASSERT(msg->m_nonce.HasContent());
		status = RespondToChallenge(msg->m_nonce);
		break;
		
	case ControlMessage::MT_THROTTLE:
	//	m_owner->m_listener->SetUploadRate(msg->m_uploadRate); /* FIXME */
		break;
		
	case ControlMessage::MT_KICKEDBY:
		{
			HeaderEntry *x_reason = msg->GetHeaderList()->GetHeader("X-Reason");
			int reason = 0;
			if(x_reason)
			{
				reason = op_atoi(x_reason->Value());
			}

			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT, reason);
		}
		break;
		
	default:
		OP_ASSERT(FALSE);
	}

}

OP_STATUS ControlChannel::RespondToChallenge(const OpStringC8 &nonce)
{
	OpString8 crypto;
	OpString8 request;
	OP_STATUS status;
	
	if (
			OpStatus::IsError(status = Encrypt(nonce, m_sharedSecret, crypto)) ||
			OpStatus::IsError(status = request.AppendFormat("RESPONSE %s %s" CRLF CRLF, crypto.CStr(), RENDEZVOUS_PROTOCOL_VERSION_STRING)) ||
			OpStatus::IsError(status = Send(request, request.Length()))
	)
	{
		goto fail;
	}
	
	return OpStatus::OK;

fail: 
	RendezvousLog::log("OOM in RespondToChallenge");

	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::REGISTER, WEB_RDV_RETRY_PERIOD);
	
	//FIXME: close socket??
	return status;
}

void ControlChannel::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == m_socket);
	RendezvousLog::log("error receiving registration response from Opera Unite proxy");
	// try again later
	m_owner->SendRendezvousConnectionProblem(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::REGISTER, WEB_RDV_RETRY_PERIOD);
}

void ControlChannel::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OpString proxyHost;
	OpString request;
	
	OP_STATUS status = OpStatus::OK;
	m_error_callback_sent = FALSE;
	
	switch (msg)
	{
	
	case MSG_WEBSERVER_REGISTER:
		switch (par2)
		{
		case ControlChannel::KEEPALIVE:
			status = KeepAlive();
			break;

		case ControlChannel::RESOLVE:
		{
			if (m_socket)
				Close();
			// nuke spurious messages
			g_main_message_handler->RemoveDelayedMessage(msg, 0, par2);
			if (OpStatus::IsError(status = m_resolver->Resolve(g_webserver->GetWebserverUriIdna())))
			{
				m_logger->logOnce(8, "Opera Unite proxy host resolution failed");
				if (m_error_callback_sent == FALSE)
					m_owner->RetryConnectToProxy(TIMEOUT_TYPE_CLIENT);
					//g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, WEB_RDV_RETRY_PERIOD);
				break;
			}
			else
			{
				m_logger->clear(8);
			}
			break;
		}
			
		case ControlChannel::CONNECT:
			// nuke spurious messages
			g_main_message_handler->RemoveDelayedMessage(msg, 0, par2);
			m_logger->clear(1);
			m_has_sent_rendezvous_connected = FALSE;
			if (OpStatus::IsError(status = Connect()))
			{
				if (OpStatus::IsMemoryError(status))
				{	
					m_owner->SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
				}
				else
				{	
					m_owner->SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
				}
			}
			else
				m_owner->last_timeout_type=TIMEOUT_TYPE_NONE;
				
			return;

		case ControlChannel::REGISTER:
			// nuke spurious messages
			g_main_message_handler->RemoveDelayedMessage(msg, 0, par2);
			OP_STATUS status;
			if (OpStatus::IsError(status = Reg()))
			{
				Close();
				if (OpStatus::IsMemoryError(status))
				{
					m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
				}
				else
				{
					m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
				}
				return;
			}
			
			break;
			
		default:
			OP_ASSERT(FALSE && par2);
		}
		break;
		
	case MSG_WEBSERVER_RDV_DATA_PENDING:
		ProcessBuffer();
		break;
		
	default:
		OP_ASSERT(FALSE && msg);
	}
	
	if (OpStatus::IsError(status))
	{
		Close();
		if (OpStatus::IsMemoryError(status))
		{	
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY);
		}
		else
		{	
			m_owner->SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
		}
	}
}

ControlMessage::ControlMessage()
	: m_type(ControlMessage::MT_UNDEFINED)
	, m_statusCode(0)
	, m_clientID(0)
	, m_uploadRate(0)
	, m_addr_http(0)
{

}

OP_STATUS ControlMessage::SetMessage(char* message, int length)
{
	RETURN_IF_ERROR(m_message.Set(message, length));
	
	char *headerStart = op_strstr(message, CRLF);		
	if (headerStart != NULL && headerStart[2] != '\x0D' && headerStart[2] != '\x0A' && static_cast<int>(headerStart - message) <= length )
	{
		char *headerEnd =  op_strstr(headerStart, CRLF CRLF);
		
		if (headerEnd == headerStart)
		{
			return OpStatus::OK;
		}
		else if (headerEnd == NULL) 
		{
			return OpStatus::ERR;
		}
		
		headerEnd += 2;
		
		char temp_char = *headerEnd;
		*headerEnd = '\0';
		RETURN_IF_ERROR(m_proxyHeaderList.SetValue(headerStart + 2));
		*headerEnd = temp_char;
	}
	
	return OpStatus::OK;
}

const char *ControlMessage::CStr()
{
	return m_message.CStr();
}

ControlMessage::ParseStatus ControlMessage::SetNonce(const char *src)
{
	m_nonce.Wipe();

	int nonceLength = 0;
	while (src[nonceLength] != ' ' && src[nonceLength] != '\0')
	{
		nonceLength++;
	}

	if (nonceLength < 20)
		return ControlMessage::PS_PARSE_ERROR;

	if (OpStatus::IsError(m_nonce.Set(src, nonceLength)))
	{
		return ControlMessage::PS_OOM;
	}

	return ControlMessage::PS_OK;
}

ControlMessage::ParseStatus ControlMessage::ParseMessage()
{
	const char *protocol = 0;
	const char *buf = CStr();
	if (startsWith(buf, RENDEZVOUS_PROTOCOL_VERSION_STRING))
	{
		const char *code = secondToken(buf);
		if (!code)
			return ControlMessage::PS_PARSE_ERROR;
		m_statusCode = (UINT)op_atoi(code);
		if (!m_statusCode)
			return ControlMessage::PS_PARSE_ERROR;
		m_type = ControlMessage::MT_UCP_RESPONSE;
	}
	else
	{
		if (startsWith(buf, "INVITE"))
		{
			const char *id = secondToken(buf);
			if (!id)
				return ControlMessage::PS_PARSE_ERROR;
			m_clientID = (UINT)op_atoi(id);
			if (!m_clientID)
				return ControlMessage::PS_PARSE_ERROR;
			
			/* Find the nonce header */

			HeaderEntry *header = m_proxyHeaderList.GetHeader("Nonce");
			
			if (!header)
				return ControlMessage::PS_PARSE_ERROR;

			const char *inviteNonce = header->Value();

			if (!inviteNonce)
				return ControlMessage::PS_PARSE_ERROR;

			if (OpStatus::IsError(m_nonce.Set(inviteNonce)))
			{
				return ControlMessage::PS_OOM;
			}

			/* Find the x-forward-fore header */			
			header = m_proxyHeaderList.GetHeader("X-Forwarded-For");
			if (!header)
				return ControlMessage::PS_PARSE_ERROR;

			const char *xForwardedFor = header->Value();

			if (!xForwardedFor)
				return ControlMessage::PS_PARSE_ERROR;

			if (OpStatus::IsError(m_client_ip.Set(xForwardedFor)))
			{
				return ControlMessage::PS_OOM;
			}
			
			protocol = secondToken(id);
			if (!protocol)
				return ControlMessage::PS_PARSE_ERROR;
				
			// Check the (optional mainly for compatibility reasons) Proxy-Address, to redirect to the HTTP proxy
			header = m_proxyHeaderList.GetHeader("Proxy-Address");
			
			const char *proxy_address=(header)?header->Value():NULL;
			
			if(proxy_address && *proxy_address)
			{
				const char *separator=op_strrchr(proxy_address, ':');

				OP_ASSERT(separator);
								
				// The HTTP Proxy Address is in the format address:port. We look for the last ':' to support also IPv6 addresses
				if(!separator)
					return ControlMessage::PS_PARSE_ERROR;
					
				OpString proxy_ip;
				int proxy_port=op_atoi(separator+1);
				
				OP_ASSERT(proxy_port>0 && proxy_port<=65535);
				
				if(proxy_port<=0 || proxy_port>65535)
					return ControlMessage::PS_PARSE_ERROR;
					
				RETURN_VALUE_IF_ERROR(proxy_ip.Set(proxy_address, separator-proxy_address), ControlMessage::PS_PARSE_ERROR);
				
				if(!m_addr_http)
					RETURN_VALUE_IF_ERROR(OpSocketAddress::Create(&m_addr_http), ControlMessage::PS_PARSE_ERROR);
				
				RETURN_VALUE_IF_ERROR(m_addr_http->FromString(proxy_ip.CStr()), ControlMessage::PS_PARSE_ERROR);
				m_addr_http->SetPort((UINT)proxy_port);
			}
			else
			{
				OP_DELETE(m_addr_http);
				m_addr_http=NULL;
			}


			m_type = ControlMessage::MT_INVITE;
		}
		else if (startsWith(buf, "CHALLENGE"))
		{
			const char *challengeNonce = secondToken(buf);
			if (!challengeNonce)
				return ControlMessage::PS_PARSE_ERROR;
			protocol = secondToken(challengeNonce);
			if (!protocol)
				return ControlMessage::PS_PARSE_ERROR;
			
			ParseStatus status = SetNonce(challengeNonce);
			if (status != ControlMessage::PS_OK)
				return status;
			m_type = ControlMessage::MT_CHALLENGE;
		}
		else if (startsWith(buf, "KICKEDBY"))
		{
			const char *new_owner_address = secondToken(buf);
			
			if (!new_owner_address)
				return ControlMessage::PS_PARSE_ERROR;
				
			protocol = secondToken(new_owner_address);
			if (!protocol)
				return ControlMessage::PS_PARSE_ERROR;
				
			const char *separator=op_strrchr(new_owner_address, ':');

			OP_ASSERT(separator);
							
			// The New Onwer Address is in the format address:port. We look for the last ':' to support also IPv6 addresses
			if(!separator)
				return ControlMessage::PS_PARSE_ERROR;
				
			OpString new_owner_ip;
			int new_owner_port=op_atoi(separator+1);
			
			OP_ASSERT(new_owner_port>0 && new_owner_port<=65535);
			
			if(new_owner_port<=0 || new_owner_port>65535)
				return ControlMessage::PS_PARSE_ERROR;
				
			RETURN_VALUE_IF_ERROR(new_owner_ip.Set(new_owner_address, separator-new_owner_address), ControlMessage::PS_PARSE_ERROR);
			
			m_type = ControlMessage::MT_KICKEDBY;
		}
		else
		{
			return ControlMessage::PS_PARSE_ERROR;
		}
		
		if (!startsWith(protocol, "UCP"))
		{
			return ControlMessage::PS_PARSE_ERROR;
		}
		
		if (CheckProtocolVersion(protocol) == OpBoolean::IS_FALSE)
		{
			return ControlMessage::PS_PARSE_VERSION_ERROR;
		}
	}
	
	return ControlMessage::PS_OK;
}

OP_BOOLEAN ControlMessage::CheckProtocolVersion(const char *protocol)
{
	int version_major;
	int version_minor;
	
	if (op_sscanf(protocol, "UCP/%d.%d", &version_major, &version_minor) == 0 )
		return OpStatus::ERR;

	if ((float)version_major+(version_minor/(float)10) < RENDEZVOUS_PROTOCOL_VERSION)
		return OpBoolean::IS_FALSE;

	return OpBoolean::IS_TRUE;
}


#endif //WEBSERVER_RENDEZVOUS_SUPPORT
