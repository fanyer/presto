/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2011
 *
 * Web server implementation 
 */


#include "core/pch.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

#include "modules/hardcore/mh/mh.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"

#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#include "modules/webserver/src/rendezvous/control_channel.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"

WebserverRendezvous::WebserverRendezvous(RendezvousEventListener *rendezvousEventListener) 
	: m_tp_client(TIMEOUT_TYPE_CLIENT)
	, m_tp_proxy(TIMEOUT_TYPE_PROXY)
	, last_timeout_type(TIMEOUT_TYPE_NONE)
	, m_backlog(5)
	, m_pendingConnectionsIterator(NULL)
	, m_controlChannel(NULL)
	, m_rendezvousEventListener(rendezvousEventListener)
	, m_local_listening_port(0)
	, m_last_checked_port(0)
	, m_direct_access(FALSE)
{
	m_tp_client.AddFiniteStep(5, 12, 0);		// Every 5 seconds for a minute
	m_tp_client.AddFiniteStep(10, 30, 0);		// Every 10 seconds for 5 minutes
	m_tp_client.AddInfiniteStep(60, 0);		// Every minute forever
	
	m_tp_proxy.AddFiniteStep(20, 3, 30);		// 20+random(30) seconds, 3 times
	m_tp_proxy.AddFiniteStep(60, 1, 120);		// 60+random(120) seconds, 1 time
	m_tp_proxy.AddFiniteStep(120, 1, 120);	// 120+random(120) seconds, 1 time
	m_tp_proxy.AddFiniteStep(300, 1, 120);	// 300+random(120) seconds, 1 time
	m_tp_proxy.AddInfiniteStep(600, 120);		// 600+random(120) seconds, 1 time
}

WebserverRendezvous::~WebserverRendezvous()
{
	g_main_message_handler->RemoveCallBacks(this, 0);
	
	OP_DELETE(m_controlChannel);
	OP_DELETE(m_pendingConnectionsIterator);
}

OP_STATUS WebserverRendezvous::Connect(OpSocketAddress *addr)
{
	OP_ASSERT(FALSE && "listening socket only!");
	return OpStatus::ERR;
}

OP_STATUS WebserverRendezvous::Send(const void* data, UINT length)
{
	OP_ASSERT(FALSE && "listening socket only!");
	return OpStatus::ERR;
}

OP_STATUS WebserverRendezvous::Recv(void* buffer, UINT length, UINT *bytes_received)
{
	OP_ASSERT(FALSE && "listening socket only!");
	return OpStatus::ERR;
}

void WebserverRendezvous::Close()
{
	OP_ASSERT(TRUE);
}

OP_STATUS WebserverRendezvous::GetSocketAddress(OpSocketAddress* address)
{
	OP_ASSERT(FALSE && "makes no sense in the context of rendezvous");
	return OpStatus::ERR;
}

void WebserverRendezvous::SetListener(OpSocketListener* listener)
{
	OP_ASSERT(FALSE && "makes no sense in the context of rendezvous");
}

OP_STATUS WebserverRendezvous::Accept(OpSocket** sock, OpSocketListener *connection, OpString8 &client_socket_ip)
{
	*sock = NULL;

	RETURN_IF_ERROR(m_pendingConnectionsIterator->First());

	ConnectData *connectData = static_cast<ConnectData*>(m_pendingConnectionsIterator->GetData());
	
	OP_ASSERT(connectData);
	OP_ASSERT(m_controlChannel);
	
	if(!connectData || !m_controlChannel)
		return OpStatus::ERR_NULL_POINTER;

	connectData->m_socket->SetListener(connection);
	//DEBUG_VERIFY_SOCKET(connectData->m_socket);	

	RETURN_IF_ERROR(m_pendingConnections.Remove(connectData->m_socket, &connectData));
	
	//DEBUG_VERIFY_SOCKET(connectData->m_socket);	
	
	OP_STATUS status;
	if (OpStatus::IsError(status = client_socket_ip.Set(connectData->m_client_ip))
		//|| OpStatus::IsError(status = m_controlChannel->Rsvp(connectData->m_clientID, 200))
		)
	{
		OP_DELETE(connectData);
		return status;
	}
	
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_RDV_DATA, (MH_PARAM_1)connectData->m_socket, (MH_PARAM_2)connection, 0);

	*sock = connectData->m_socket;
	connectData->m_socket = NULL;
	OP_DELETE(connectData);
	return OpStatus::OK;

}

OP_STATUS WebserverRendezvous::Listen(OpSocketAddress* socket_address, int queue_size)
{
	
	OP_ASSERT(socket_address == NULL);
	m_backlog = queue_size;
	
	return ControlChannel::Create(&m_controlChannel, this, m_shared_secret.CStr());
}

void WebserverRendezvous::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OpSocketListener *connection;
	switch (msg)
	{
//	case MSG_WEBSERVER_CONFIG_READY:
//		// this msg is received after autoconfiguration happens
//		// if autoconfiguration happens before Create() is called, this msg should never be received
//		if (OpStatus::IsMemoryError(Init()))
//		{
//			g_webserverPrivateGlobalData->SignalOOM();
//		}
//		break;
		
	case MSG_WEBSERVER_RDV_DATA:
		connection = (OpSocketListener*)par2;
		OP_ASSERT(connection);
//		OP_ASSERT(connection->socket);
		connection->OnSocketDataReady((OpSocket*)par1);
		break;
		
	case MSG_WEBSERVER_RDV_CLOSE_CONTROL_CHANNEL:
		OP_DELETE(m_controlChannel);
		m_controlChannel = NULL;
		return;
		break;
		
	default:
		OP_ASSERT(FALSE && msg);
	}
}

#ifdef OPSOCKET_GETLOCALSOCKETADDR
OP_STATUS WebserverRendezvous::GetLocalSocketAddress(OpSocketAddress* socket_address)
{
	return OpStatus::ERR_NOT_SUPPORTED;	
}
#endif

void WebserverRendezvous::OnSocketListenError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(FALSE);
}

void WebserverRendezvous::OnSocketConnected(OpSocket* socket)
{
	OP_ASSERT(FALSE);
}

void WebserverRendezvous::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(FALSE);
}

void WebserverRendezvous::OnSocketDataSent(OpSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(FALSE);
}
	
void WebserverRendezvous::OnSocketClosed(OpSocket* socket)
{
	OP_ASSERT(socket == this);
	SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT);
}

void WebserverRendezvous::OnSocketConnectionRequest(OpSocket* socket)
{
	OP_ASSERT(socket == this);
	m_rendezvousEventListener->OnRendezvousConnectionRequest(this);
}

void WebserverRendezvous::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	BOOL retry = FALSE;
	//BOOL random = TRUE;

	switch (error)
	{
		case HOST_ADDRESS_NOT_FOUND:
			retry = FALSE;
			SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN);
			break;
			
		case CONNECTION_FAILED:
			retry = TRUE;
			//random = TRUE;
			SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE);
			break;
			
		case CONNECTION_REFUSED:
			retry = TRUE;
			//random = TRUE;
			SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_DOWN);
			break;
			
		default:
			retry = TRUE;
			//random = TRUE;			
			SendRendezvousConnectError(RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK);
	}
	
	if (retry)
	{
		OP_NEW_DBG("WebserverRendezvous::OnSocketConnectError", "webserver");
		OP_DBG((UNI_L("*** Reconnected (%s) because of error %d\n"), /*(random)?*/UNI_L("proxy")/*:UNI_L("client")*/, error));
		
		RetryConnectToProxy(/*random?*/TIMEOUT_TYPE_PROXY/*:TIMEOUT_TYPE_CLIENT*/);
	}
	
}


void WebserverRendezvous::RetryConnectToProxy(TimeoutType suggested_type)
{
	TimeoutPolicy *tp=NULL;
	TimeoutType new_type;
	
	OP_ASSERT(suggested_type==TIMEOUT_TYPE_CLIENT || suggested_type==TIMEOUT_TYPE_PROXY);
	
	// If a timeout policy is already in place, it will keep going, else the suggested one is kept...
	// When the problem is solved, the policy will hopefully switch to TIMEOUT_TYPE_NONE
	if(last_timeout_type==TIMEOUT_TYPE_NONE)
		new_type=suggested_type;   // new policy
	else
		new_type=last_timeout_type; // keep the old policy
	
	if(new_type==TIMEOUT_TYPE_CLIENT)
		tp=&m_tp_client;
	else if(new_type==TIMEOUT_TYPE_PROXY)
		tp=&m_tp_proxy;
	else
	{
		OP_ASSERT(FALSE);
		
		tp=&m_tp_proxy;  // fallback... that is not supposed to happen...
	}
	
	OP_ASSERT(tp);
	
	if(new_type!=last_timeout_type)
		tp->ResetStep();  // A chenge of policy, means a reset
		
	last_timeout_type=new_type;
	
	UINT32 wait_time=tp->GetAutoWaitTimeMS();
	
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, wait_time);
	
	/*if (random == TRUE)
	{
		int randomDelay = op_rand();		
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, 20000 + (randomDelay % 30000));
	}
	else
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_REGISTER, 0, ControlChannel::RESOLVE, WEB_RDV_RETRY_PERIOD);*/
}

TimeoutType WebserverRendezvous::GetReconnectionPolicy(RendezvousStatus error, RendezVousError error_type)
{
	switch (error)
	{
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN: 	 // The proxy address is unkown. Rendezvous will try to reconnect
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_DOWN:				 // Connection was refused. Rendezvous will try to reconnect
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE:		 // Rendezvous will try to reconnect
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_DENIED:
	 		return TIMEOUT_TYPE_PROXY;

	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK:					 // Rendezvous will try to reconnect
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY:
	 		return TIMEOUT_TYPE_CLIENT;
	 		
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_AUTH_FAILURE:				 // Invalid username/password
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED: // The device has already been registered
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION:	 // The proxy has been updated, and webserver is still using the old protocol
		case RENDEZVOUS_PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION:

	 		if(error_type==RENDEZVOUS_ERROR_PROBLEM) // Always try to reconnect in this case
	 			return TIMEOUT_TYPE_PROXY;
	 			
			return TIMEOUT_TYPE_NONE;  // No retry
			
	 	case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL:					 // Serious protocol error. Probably homebrew proxy.
	 		if(error_type==RENDEZVOUS_ERROR_CLOSE)
	 			return TIMEOUT_TYPE_CLIENT;
	 		
	 		return TIMEOUT_TYPE_PROXY;
	 		
	 	case RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT:					 // Logged out from the proxy. Rendezvous will try to reconnect
	 		if(error_type==RENDEZVOUS_ERROR_CLOSE)
	 			return TIMEOUT_TYPE_NONE;  // No retry

	 		return TIMEOUT_TYPE_PROXY;
	}
	
	OP_ASSERT(FALSE);
	 
	return TIMEOUT_TYPE_PROXY;
}

void WebserverRendezvous::SendRendezvousConnectError(RendezvousStatus error)
{
	TimeoutType policy=GetReconnectionPolicy(error, RENDEZVOUS_ERROR_CONNECT);
	BOOL retry=(policy!=TIMEOUT_TYPE_NONE);

	if (retry)
	{
		OP_NEW_DBG("WebserverRendezvous::SendRendezvousConnectError", "webserver");
		OP_DBG((UNI_L("*** Reconnected (%s) because of error %d\n"), (policy==TIMEOUT_TYPE_PROXY)?UNI_L("proxy"):UNI_L("client"), error));
		
		RetryConnectToProxy(policy);
	}

	m_rendezvousEventListener->OnRendezvousConnectError(this, error, retry);
}

void WebserverRendezvous::SendRendezvousCloseError(RendezvousStatus error, int code)
{
	TimeoutType policy=GetReconnectionPolicy(error, RENDEZVOUS_ERROR_CLOSE);
	BOOL retry=(policy!=TIMEOUT_TYPE_NONE);

	if (retry)
	{
		OP_NEW_DBG("WebserverRendezvous::SendRendezvousCloseError", "webserver");
		OP_DBG((UNI_L("*** Reconnected (%s) because of error %d\n"), (policy==TIMEOUT_TYPE_PROXY)?UNI_L("proxy"):UNI_L("client"), error));
			
		RetryConnectToProxy(policy);
	}
	
	m_rendezvousEventListener->OnRendezvousClosed(this, error, retry, code);	
}

void WebserverRendezvous::SendRendezvousConnectionProblem(RendezvousStatus error)
{
	TimeoutType policy=GetReconnectionPolicy(error, RENDEZVOUS_ERROR_PROBLEM);
	BOOL retry=(policy!=TIMEOUT_TYPE_NONE);
	
	OP_ASSERT(retry);  // This case is always supposed to retry
	
	if (retry)
	{
		OP_NEW_DBG("WebserverRendezvous::SendRendezvousConnectionProblem", "webserver");
		OP_DBG((UNI_L("*** Reconnected (%s) because of error %d\n"), (policy==TIMEOUT_TYPE_PROXY)?UNI_L("proxy"):UNI_L("client"), error));
	}

	if (retry)
		RetryConnectToProxy(policy);
		
	m_rendezvousEventListener->OnRendezvousConnectionProblem(this, error, retry);
}

void WebserverRendezvous::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(FALSE);
}

void WebserverRendezvous::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(FALSE);
}

void WebserverRendezvous::OnSocketCloseError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == this);
	
	SendRendezvousCloseError(RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT);
}

void WebserverRendezvous::OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(FALSE);
}

/*static*/
BOOL WebserverRendezvous::IsConfigured()
{
	OpStringC user = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser);
	
	OpStringC dev = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice);
		
	OpStringC pw = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword);
		
	return !(user == "" || dev == "" || pw == "");
}

BOOL WebserverRendezvous::IsConnected()
{
	return m_controlChannel && m_controlChannel->IsConnected();
}

/*static */ OP_STATUS WebserverRendezvous::Create(WebserverRendezvous **rendezvous, RendezvousEventListener *rendezvousEventListener, const char *shared_secret, unsigned int local_listening_port)
{
	*rendezvous = NULL;

	OP_STATUS result = OpStatus::ERR_NO_MEMORY;

	if (IsConfigured() == FALSE)
	{
		return OpStatus::ERR;
	}
	
	WebserverRendezvous *temp_rendezvous = OP_NEW(WebserverRendezvous, (rendezvousEventListener));
	if (!temp_rendezvous || OpStatus::IsError(temp_rendezvous->m_shared_secret.Set(shared_secret)))
	{
		OP_DELETE(temp_rendezvous);
		return OpStatus::ERR_NO_MEMORY;
	}

	temp_rendezvous->m_pendingConnectionsIterator = temp_rendezvous->m_pendingConnections.GetIterator();

	if (temp_rendezvous->m_pendingConnectionsIterator == NULL)
	{
		OP_DELETE(temp_rendezvous);
		return OpStatus::ERR_NO_MEMORY;
	}
	else if 
	(
			OpStatus::IsError(result = g_main_message_handler->SetCallBack(temp_rendezvous, MSG_WEBSERVER_RDV_DATA, 0)) ||
			OpStatus::IsError(result = g_main_message_handler->SetCallBack(temp_rendezvous, MSG_WEBSERVER_RDV_CLOSE_CONTROL_CHANNEL, 0))
	)
	{
		OP_DELETE(temp_rendezvous);
		return result;
	}
	
	temp_rendezvous->m_local_listening_port = local_listening_port;
	*rendezvous = temp_rendezvous;
	return OpStatus::OK;
}

void WebserverRendezvous::CheckPort(unsigned int port_to_check)
{
	if (m_controlChannel)
		m_controlChannel->CheckPort(port_to_check);
}

// TODO: update this when NAT traversal is implemented
BOOL WebserverRendezvous::isDirect()
{
	return FALSE;
}

OP_STATUS TimeoutPolicy::AddFiniteStep(UINT32 wait, UINT32 num, UINT32 random)
{
	OP_ASSERT(num_steps<10);
	
	if(num_steps>=10)
		return OpStatus::ERR_OUT_OF_RANGE;
	
	// Check that only the last step is infinite
	OP_ASSERT(num_steps==0 || num!=TIMEOUT_INFINITE_STEP || steps[num_steps-1].num_attempts!=TIMEOUT_INFINITE_STEP);
	
	steps[num_steps].wait_seconds=wait;
	steps[num_steps].num_attempts=num;
	steps[num_steps].random_seconds=random;
	
	num_steps++;
	
	return OpStatus::OK;
}

UINT32 TimeoutPolicy::GetWaitTimeMS(UINT32 step)
{
	UINT32 done=0;
	
	for(UINT32 i=0; i<num_steps; i++)
	{
		if(step<done+steps[i].num_attempts)
		{
			if(!steps[i].random_seconds)
				return steps[i].wait_seconds*1000;
				
			int random_delay = op_rand();		
		
			return steps[i].wait_seconds*1000 + (random_delay % steps[i].random_seconds)*1000;
		}
		
		done+=steps[i].num_attempts;
	}
	
	return TIMEOUT_NEVER;  // No more timeouts
}

#endif // WEBSERVER_RENDEZVOUS_SUPPORT
