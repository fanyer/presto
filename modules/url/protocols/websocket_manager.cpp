/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#include "core/pch.h"

#ifdef WEBSOCKETS_SUPPORT
#include "modules/autoproxy/autoproxy_public.h"
#include "modules/libssl/ssl_api.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/protocols/websocket_manager.h"
#include "modules/url/protocols/websocket_protocol.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_man.h"


#define WS_NEW_DBG OP_NEW_DBG(__FUNCTION__, "url_websocket")


WebSocket_Server_Manager::~WebSocket_Server_Manager()
{
	WS_NEW_DBG;
	g_main_message_handler->UnsetCallBacks(this);
}


OP_STATUS WebSocket_Server_Manager::AddSocket(WebSocketProtocol* a_websocket, MessageHandler* a_mh, ServerName* proxy, unsigned short proxyPort)
{
	WS_NEW_DBG;
	Comm* new_comm = NULL;
	if (proxy)
		new_comm = Comm::Create(a_mh, proxy, proxyPort);
	else
		new_comm = Comm::Create(a_mh, servername, port);
	if (new_comm == NULL)
		return OpStatus::ERR_NO_MEMORY;
	new_comm->SetIsFullDuplex(TRUE);
	new_comm->SetManagedConnection();

	OpStackAutoPtr<SComm> comm(new_comm);

	if (a_websocket->GetSecure())
	{
#ifdef _SSL_SUPPORT_
# ifndef _EXTERNAL_SSL_SUPPORT_
		ProtocolComm* ssl_comm;

		ssl_comm = g_ssl_api->Generate_SSL(a_mh, &(*servername), port, FALSE, TRUE);
		if (ssl_comm == NULL)
			return OpStatus::ERR_NO_MEMORY;

		ssl_comm->SetNewSink(comm.release());
		comm.reset(ssl_comm);
# else //_EXTERNAL_SSL_SUPPORT_
#  ifdef URL_ENABLE_INSERT_TLS
#   ifdef _USE_HTTPS_PROXY
		if (!proxy)
#   endif
			if (!comm->InsertTLSConnection(&(*servername), port))
				return OpStatus::ERR_NO_MEMORY;
#  endif // URL_ENABLE_INSERT_TLS
# endif //_EXTERNAL_SSL_SUPPORT_
#elif defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		comm->SetSecure(TRUE);
#endif // _NATIVE_SSL_SUPPORT_ || _CERTICOM_SSL_SUPPORT_
	}

	a_websocket->SetNewSink(comm.release());
	RETURN_IF_ERROR(a_websocket->SetCallbacks(this, NULL));

	WebSocket_Connection* conn = OP_NEW(WebSocket_Connection, (a_websocket));
	RETURN_VALUE_IF_NULL(conn, OpStatus::ERR_NO_MEMORY);
	conn->Into(&connections);

	OP_DBG(("Number of connections: %d \n", connections.Cardinal()));

	WebSocket_Connection* current = static_cast<WebSocket_Connection*>(connections.First());
	while (current)
	{
		if (current != conn && current->Socket()->GetState() == OpWebSocket::WS_CONNECTING)
				return OpStatus::OK;
		current = static_cast<WebSocket_Connection*>(current->Suc());
	}

	CommState state = a_websocket->Load();
	if (state == COMM_REQUEST_FAILED)
		return OpStatus::ERR;

	OP_ASSERT(state == COMM_LOADING || state == COMM_WAITING || state == COMM_REQUEST_WAITING);

	a_websocket->SetUrlLoadStatus(URL_LOADING);
	a_websocket->SetState(WebSocketProtocol::WS_CONNECTING);

	return OpStatus::OK;
}


void WebSocket_Server_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	WS_NEW_DBG;
	OP_DBG(("Message: %d, par1: %x, par2: %x", msg, par1, par2));

	switch (msg)
	{
	case MSG_URL_LOAD_NOW:
		{
			WebSocket_Connection* current = static_cast<WebSocket_Connection*>(connections.First());
			while (current)
			{
				if (current->Socket()->GetState() == WebSocketProtocol::WS_INITIALIZING)
				{
					CommState state = current->Socket()->Load();
					if (state == COMM_REQUEST_FAILED)
					{
						WebSocket_Connection* close_connection = static_cast<WebSocket_Connection*>(current->Suc());

						current = static_cast<WebSocket_Connection*>(current->Suc());
						close_connection->Socket()->Close(0, NULL);
						continue;
					}
					else
					{
						OP_ASSERT(state == COMM_LOADING || state == COMM_WAITING || state == COMM_REQUEST_WAITING);
						current->Socket()->SetUrlLoadStatus(URL_LOADING);
						current->Socket()->SetState(WebSocketProtocol::WS_CONNECTING);
					}
					break;
				}
				current = static_cast<WebSocket_Connection*>(current->Suc());
			}
		}
		break;
	case MSG_COMM_LOADING_FINISHED:
		{
			OP_DBG(("MSG_COMM_LOADING_FINISHED"));
		}
		break;
	case MSG_COMM_LOADING_FAILED:
		{
			OP_DBG(("MSG_COMM_LOADING_FAILED"));
		}
		break;
	default:
		OP_ASSERT(!"This should not happen");
	}
}


WebSocket_Server_Manager* WebSocket_Manager::FindServer(ServerName* name, unsigned short port, BOOL secure, BOOL create)
{
	WS_NEW_DBG;
	WebSocket_Server_Manager* srv_mgr;

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	srv_mgr = (WebSocket_Server_Manager*) Connection_Manager::FindServer(name, port, secure);
#else
	srv_mgr = (WebSocket_Server_Manager*) Connection_Manager::FindServer(name, port);
#endif

	if(srv_mgr == NULL && create)
	{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		srv_mgr =  OP_NEW(WebSocket_Server_Manager, (name, port, secure));
#else
		srv_mgr = OP_NEW(WebSocket_Server_Manager, (name, port_num));
#endif
		if(srv_mgr != NULL)
			srv_mgr->Into(&connections);
	}

	return srv_mgr;
}

int WebSocket_Manager::GetActiveWebSockets()
{
	WS_NEW_DBG;
	int count = 0;
	for (WebSocket_Server_Manager *elem = static_cast<WebSocket_Server_Manager *>(connections.First()); elem != NULL; elem = static_cast<WebSocket_Server_Manager *>(elem->Suc()))
		count += elem->GetActiveWebSockets();

	return count;
}

int WebSocket_Server_Manager::GetActiveWebSockets()
{
	WS_NEW_DBG;
	return connections.Cardinal();
}

WebSocket_Connection::WebSocket_Connection(WebSocketProtocol* a_socket)
: m_socket(a_socket)
{
	OP_ASSERT(m_socket);
	m_socket->SetConnElement(this);
}

WebSocket_Connection::~WebSocket_Connection()
{
	WS_NEW_DBG;
	if(InList())
		Out();

	m_socket = NULL;
}

BOOL WebSocket_Connection::SafeToDelete()
{
	return !m_socket || m_socket->SafeToDelete();
}

#ifdef AUTOPROXY_PER_CONTEXT
	extern BOOL UseAutoProxyForContext(URL_CONTEXT_ID);
#endif
#ifdef EXTERNAL_PROXY_DETERMINATION_BY_URL
	extern const uni_char* GetExternalProxy(URL &url);
#endif


OP_STATUS WebSocketProtocol::DetermineProxy()
{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
#ifdef AUTOPROXY_PER_CONTEXT
	URL_CONTEXT_ID contextId = m_target.GetContextId();
	if (UseAutoProxyForContext(contextId) && g_pcnet->IsAutomaticProxyOn())
#else
	if (g_pcnet->IsAutomaticProxyOn())
#endif
	{
		m_autoProxyLoadHandler = CreateAutoProxyLoadHandler(m_target.GetRep(), g_main_message_handler);
		if(m_autoProxyLoadHandler != NULL)
		{
			static const OpMessage messages[] =
			{
				MSG_COMM_PROXY_DETERMINED,
				MSG_COMM_LOADING_FAILED
			};

			g_main_message_handler->SetCallBackList(this, m_autoProxyLoadHandler->Id(), messages, ARRAY_SIZE(messages));
			return m_autoProxyLoadHandler->Load();
		}
	}
#endif

#ifdef EXTERNAL_PROXY_DETERMINATION_BY_URL
	const uni_char* proxy = GetExternalProxy(m_target);
#else // EXTERNAL_PROXY_DETERMINATION_BY_URL
	const uni_char *proxy = urlManager->GetProxy(m_targetHost, m_info.secure ? URL_WEBSOCKET_SECURE : URL_WEBSOCKET);
#endif // EXTERNAL_PROXY_DETERMINATION_BY_URL

	if(proxy && urlManager->UseProxyOnServer(m_targetHost, m_targetPort))
	{
		OP_STATUS err;
		m_proxy = (ServerName *)urlManager->GetServerName(err, proxy, m_proxyPort, TRUE);
		RETURN_IF_ERROR(err);
	}

	return OpStatus::OK;
}



#endif // WEBSOCKETS_SUPPORT
