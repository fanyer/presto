/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url_man.h"

#include "modules/olddebug/timing.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#ifdef USE_SPDY
#include "modules/url/protocols/spdy/spdy_connection.h"
#endif
#include "modules/url/url_dns_man.h"
#include "modules/obml_comm/obml_config.h"
#include "modules/doc/frm_doc.h"
#ifndef NO_FTP_SUPPORT
#include "modules/url/protocols/ftp.h"
#endif // NO_FTP_SUPPORT

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
#ifdef _CERTICOM_SSL_SUPPORT_
#include "product/mathilda/ssl/certicom_ssl.h"
#endif // _CERTICOM_SSL_SUPPORT_
#endif

#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_RENDEZVOUS_SUPPORT)
#include "modules/webserver/webserver-api.h"
#endif // WEBSERVER_SUPPORT && WEBSERVER_RENDEZVOUS_SUPPORT

#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)  
#include "modules/webserver/webserver_direct_socket.h"
#endif

#include "modules/libssl/ssl_api.h"

// http_man.cpp

// HTTP 1.1 connection manager

#if defined _DEBUG || defined _RELEASE_DEBUG_VERSION
#ifdef YNP_WORK
#define DEBUG_HTTP_FILE
#define DEBUG_HTTP_REQDUMP
//unsigned long HTTP_Connection::HTTP_count= 0;
#endif
#endif

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
void HTTP_Manager::RemoveURLReferences()
{
	HTTP_Server_Manager *server = (HTTP_Server_Manager *) connections.First();
	while(server)
	{
		if (server->IsSecure())
		{
			HTTP_Connection *connection = (HTTP_Connection *) server->connections.First();

			while (connection)
			{
				if (connection->conn)
				{
					SComm *scomm = connection->conn->GetSink();
					if (scomm)
						SSL_API::RemoveURLReferences(static_cast<ProtocolComm *>(scomm));
				}

				connection = (HTTP_Connection *) connection->Suc();
			}
		}
		server = (HTTP_Server_Manager *) server->Suc();
	}
}

HTTP_Server_Manager *HTTP_Manager::FindServer(ServerName *name, unsigned short port_num, BOOL sec, BOOL create)
#else
HTTP_Server_Manager *HTTP_Manager::FindServer(ServerName *name, unsigned short port_num, BOOL create)
#endif
{
	HTTP_Server_Manager *item;

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	item = (HTTP_Server_Manager *) Connection_Manager::FindServer(name,port_num,sec);
#else
	item = (HTTP_Server_Manager *) Connection_Manager::FindServer(name,port_num);
#endif

	if(item== NULL && create)
	{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		item =  OP_NEW(HTTP_Server_Manager, (name,port_num,sec));
#else
		item = OP_NEW(HTTP_Server_Manager, (name,port_num));
#endif
		if(item != NULL)
			item->Into(&connections);
	}

	return item;
}

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
HTTP_Server_Manager::HTTP_Server_Manager(ServerName *name, unsigned short port_num, BOOL sec)
	: Connection_Manager_Element(name, port_num, sec)
#else
HTTP_Server_Manager::HTTP_Server_Manager(ServerName *name, unsigned short port_num)
: Connection_Manager_Element(name, port_num)
#endif
{
	http_1_1 = TRUE;
	http_continue = TRUE;
	http_chunking = FALSE;
	http_1_0_keep_alive = FALSE;
	http_1_0_used_connection_header = FALSE;
	http_1_1_pipelined = FALSE;
	http_no_pipeline = FALSE;
	http_trust_content_length = TRUE;
#ifdef WEB_TURBO_MODE
	turbo_enabled = FALSE;
#endif // WEB_TURBO_MODE
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
	http_1_1_used_keepalive = FALSE;
//#endif
	
	nextProtocolNegotiated = NP_NOT_NEGOTIATED;
	tested_http_1_1_pipelinablity = FALSE;
	http_protcol_determined = FALSE;
	http_always_use_continue = FALSE;
	if(name->Name() && *name->Name() &&
	   UNICODE_SIZE(uni_strlen(name->UniName())+50) <= g_memory_manager->GetTempBufLen())
	{
		uni_char *temp = (uni_char *) g_memory_manager->GetTempBuf();

		uni_snprintf(temp, UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()), UNI_L("%s:%u"), name->UniName(), port_num);
		http_always_use_continue = g_pcnet->GetUse100Continue(temp);
	}
	pipeline_problem_count = 0;

#ifdef HTTP_BENCHMARK
	if(bench_active)
	{
		tested_http_1_1_pipelinablity = TRUE;
		http_protcol_determined = TRUE;
		if(benchmark_persistent)
		{
			http_no_pipeline = FALSE;
			http_1_1_pipelined = benchmark_pipelined;
		}
		else
		{
			http_no_pipeline = TRUE;
			http_1_1_pipelined = FALSE;
		}
	}
#endif
}

void HTTP_Server_Manager::SetAlwaysUseHTTP_Continue(BOOL val)
{
#ifndef PREFS_NO_WRITE
	if(val && servername->Name() && *servername->Name())
	{
		uni_char *temp = (uni_char *) g_memory_manager->GetTempBuf();

		uni_snprintf(temp, UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()), UNI_L("%s:%u"), servername->UniName(), port);
		TRAPD(err, g_pcnet->WriteUse100ContinueL(temp, TRUE));
	}
#endif // !PREFS_NO_WRITE
	http_always_use_continue = val;
}


HTTP_Server_Manager::~HTTP_Server_Manager()
{
	g_main_message_handler->UnsetCallBacks(this);
	// Deleted by their owners, or SComm cleanup
	HTTP_Request *req;
	while ((req = (HTTP_Request *)requests.First()) != NULL)
	{
		req->master = NULL;
		req->Out();
	}
}

void HTTP_Server_Manager::NextProtocolNegotiated(HttpRequestsHandler *connection, NextProtocol next_protocol)
{
	nextProtocolNegotiated = next_protocol;
#ifdef USE_SPDY
	BOOL toSpdy = next_protocol == NP_SPDY2 || next_protocol == NP_SPDY3;
	HTTP_Connection *conn = static_cast<HTTP_Connection *>(GetConnection(connection->Id()));
	if (!conn || (conn->IsSpdyConnection() == toSpdy))
		return;

	HttpRequestsHandler *oldConnection = conn->conn;
	HttpRequestsHandler *newConnection = NULL;

	if (toSpdy)
		newConnection = OP_NEW(SpdyConnection, (next_protocol, g_main_message_handler, servername, port, oldConnection->GetOpenInPrivacyMode(), secure));
	else
		newConnection = OP_NEW(HTTP_1_1, (g_main_message_handler, servername, port, oldConnection->GetOpenInPrivacyMode()));

	if (!newConnection)
		return;

	TRAPD(err, newConnection->ConstructL());
	if (OpStatus::IsError(err))
	{
		OP_DELETE(newConnection);
		return;
	}

	SComm *sink = oldConnection->PopSink();
	newConnection->SetNewSink(sink);
	newConnection->SetCallbacks(this, NULL); //TODO: error handling

	newConnection->SetConnElement(conn);
	newConnection->SetManager(this);
	conn->SetIsSpdyConnection(toSpdy);
	conn->conn = newConnection;

	for (HTTP_Request *req = static_cast<HTTP_Request*>(requests.First()); req; req = static_cast<HTTP_Request*>(req->Suc()))
	{
		if (req->GetSink() && (oldConnection->HasId(req->GetSink()->Id()) || req->http_conn == oldConnection))
		{
			oldConnection->RemoveRequest(req);
			if (!newConnection->AddRequest(req))
			{
				req->EndLoading();
				g_main_message_handler->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				break;
			}
		}
	}

	oldConnection->Stop();
	oldConnection->SetConnElement(NULL);
	SComm::SafeDestruction(oldConnection);
#endif // USE_SPDY
}

void HTTP_Server_Manager::IncrementPipelineProblem()
{
	pipeline_problem_count++;
	if(pipeline_problem_count > 10)
		http_no_pipeline = TRUE;
}

void HTTP_Server_Manager::RequestHandled()
{
	if (!delayed_request_list.Empty())
	{
		HTTP_Request *req = static_cast<HTTP_Request*>(delayed_request_list.First());
		req->Out();
		if (AddRequest(req) == COMM_REQUEST_FAILED)
		{
			req->EndLoading();
			g_main_message_handler->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
	}
}

int HTTP_Server_Manager::GetNumberOfIdleConnections(BOOL include_not_yet_connected_connections)
{
	int number_of_idle_connections = 0;
	HTTP_Connection *connection = (HTTP_Connection *) connections.First();
	while(connection)
	{
		if (connection->Idle() || (include_not_yet_connected_connections && connection->conn && !connection->conn->Connected()))
			number_of_idle_connections++;
		connection = (HTTP_Connection *) connection->Suc();
	}
	return number_of_idle_connections;
}

int HTTP_Server_Manager::GetMaxServerConnections(HTTP_Request *req)
{
	unsigned short max_server_conn = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);

	if(HostName() && HostName()->GetConnectionNumRestricted())
		max_server_conn = 1;

	if( http_1_1 || http_1_0_keep_alive )
	{
#ifdef WEB_TURBO_MODE
		if( turbo_enabled && req->request->use_turbo &&
			urlManager->GetWebTurboAvailable() && !req->GetLoadDirect() )
		{
			/*
			* Make sure no new connections are opened until we have received the Turbo Host
			* to send in the X-Opera-Host header. This is necessary to enable the turbo proxy
			* to limit bandwidth per user.
			*/
			if( m_turbo_host.IsEmpty() )
				max_server_conn = 1;
			else
				max_server_conn = 2;
		}
		else
#endif // WEB_TURBO_MODE
			max_server_conn = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxPersistentConnectionsServer);
	}

#ifdef _EMBEDDED_BYTEMOBILE
	if(!req->GetLoadDirect() && urlManager->GetEmbeddedBmOpt())
		max_server_conn = 1;
#endif //_EMBEDDED_BYTEMOBILE
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	if (g_webserver && g_webserver->IsRunning())
	{
		const uni_char *local_webserver_servername = g_webserver->GetWebserverUri();
		if (local_webserver_servername && (uni_stricmp(req->HostName()->UniName(), local_webserver_servername) == 0 && (req->request->connect_port == 80 || req->request->connect_port == g_webserver->GetLocalListeningPort()))) /* Also needs a port range here */
		{
			max_server_conn = 100;
		}
	}
#endif // WEBSERVER_SUPPORT && WEBSERVER_DIRECT_SOCKET_SUPPORT
	return max_server_conn;
}

BOOL HTTP_Server_Manager::BypassDocumentConnections(HTTP_Request *req, unsigned max_server_connections)
{
	// We want to send a css/js request out on a connection where
	// there are no main document being requested. But only if there are
	// existing connections without main-documents or if it is possible
	// to create a new connection.
	unsigned conn_count = 0;
	HTTP_Connection *conn2 = (HTTP_Connection *) connections.First();
	BOOL connection_without_main_document = FALSE;
	if ((req->GetPriority()==FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_CSS_INLINE
		|| req->GetPriority()==FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_DSE
		|| req->GetPriority()==FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_SCRIPT_INLINE_NO_DSE))
	{
		while(conn2)
		{
			if (conn2->AcceptNewRequests()
#ifdef WEBSERVER_SUPPORT
				&& req->request->unite_connection == (BOOL)conn2->conn->IsUniteConnection()
#endif // WEBSERVER_SUPPORT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
				&& conn2->conn->GetLoadDirect() == req->GetLoadDirect()
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
				)
			{
				conn_count++;

				if (!(conn2->conn->HasPriorityRequest(FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT)
					|| conn2->conn->HasPriorityRequest(FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_USER_INITIATED)))
					connection_without_main_document = TRUE;
				break;
			}
			conn2 = (HTTP_Connection *) conn2->Suc();
		}


		// If there are no existing connections without a main document, but we are below the limit,
		// then we still want to check for main documents in the queue. If we do not find such a connection,
		// we will simply create a new connection.
		if (!connection_without_main_document && conn_count < max_server_connections)
			connection_without_main_document = TRUE;
	}
	return connection_without_main_document;
}

CommState HTTP_Server_Manager::AddRequest(HTTP_Request *req)
{
	if (req == NULL)
		return COMM_REQUEST_FAILED;

	if(
#ifdef SOCKS_SUPPORT
	   !req->GetForceSOCKSProxy() &&
#endif
	   !req->info.proxy_request && !req->request->connect_host->IsResolvingHost() && !req->request->connect_host->IsHostResolved())
	{
		g_url_dns_manager->Resolve(req->request->connect_host, NULL);	//DNS prefetch
	}

	if(req->InList())
		req->Out();
	req->Into(&requests);
	req->Clear();
	req->http_conn = NULL;
	unsigned max_server_connections = GetMaxServerConnections(req);
	HTTP_Connection *conn = NULL;

	if(http_1_1 || http_1_0_keep_alive || http_1_0_used_connection_header)
	{
		HTTP_Connection *least_loaded = NULL;
		unsigned least_load_count=0;
		HTTP_Connection *most_loaded = NULL;
		unsigned most_load_count=0;
		unsigned conn_count = 0;
		BOOL connection_without_main_document = BypassDocumentConnections(req, max_server_connections);
		HTTP_Connection *conn2 = (HTTP_Connection *) connections.First();
		while(conn2)
		{
			HTTP_Connection *conn1 = conn2;
			conn2 = (HTTP_Connection *) conn2->Suc();

			if(!conn1->conn)
			{
				OP_DELETE(conn1);
				continue;
			}

#ifdef URL_DISABLE_INTERACTION
			if((req->GetUserInteractionBlocked() && ! conn1->conn->GetUserInteractionBlocked()) ||
				 (!req->GetUserInteractionBlocked() && conn1->conn->GetUserInteractionBlocked()) )
			{
				continue;
			}
#endif

			if(conn1->Idle()
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
				&& conn1->conn->GetLoadDirect() == req->GetLoadDirect()
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef USE_SPDY
				|| conn1->IsSpdyConnection() && conn1->AcceptNewRequests()
#endif // USE_SPDY
				) {
				conn = conn1;
				break;
			}

			if (conn1->AcceptNewRequests())
			{
				conn_count++;

				if( /* Avoid sending out a request for js/css on the same connection that the main document is still being received */
					!(connection_without_main_document && 
					(conn1->conn->HasPriorityRequest(FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT) ||
					conn1->conn->HasPriorityRequest(FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_USER_INITIATED))) &&
#ifdef WEBSERVER_SUPPORT
				req->request->unite_connection == (BOOL)conn1->conn->IsUniteConnection() &&
#endif // WEBSERVER_SUPPORT
					(!(req->GetPriority()==FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_USER_INITIATED && conn1->conn->GetRequestCount()>1))
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
					&& conn1->conn->GetLoadDirect() == req->GetLoadDirect()
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
					)
				{
					unsigned current_load = conn1->conn->GetRequestCount();

					if(!least_loaded || current_load < least_load_count)
					{
						least_loaded = conn1;
						least_load_count = current_load;
					}
					if(!most_loaded || current_load > most_load_count)
					{
						most_loaded = conn1;
						most_load_count = current_load;
					}
				}
			}
		}

		if(!conn)
		{
			BOOL TooManyOpenConnections = urlManager->TooManyOpenConnections(HostName());
			int waitstatusload = Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD);

			if (!TooManyOpenConnections && conn_count < max_server_connections &&
				waitstatusload < g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal))
			{
				//We have not used up our connection limit yet, so lets do that first before queueing behind other requests
				conn = NULL;
			}
			else if(least_loaded && conn_count >= max_server_connections)
			{
				conn = least_loaded;
#ifdef DEBUG_HTTP_REQDUMP
				PrintfTofile("winsck.txt","Added Request %d to HTTP connection %d (current load %d)\n",
					req->Id(), conn->Id(), least_load_count);
				PrintfTofile("httpq.txt","Added Request %d to HTTP connection %d (current load %d)\n",
					req->Id(), conn->Id(), least_load_count);
				OpString8 textbuf1;

				textbuf1.AppendFormat("htpr%04d.txt",req->Id());
				PrintfTofile(textbuf1,"Added Request %d to HTTP connection %d (current load %d)\n",
					req->Id(), conn->Id(), least_load_count);
				OpString8 textbuf2;

				textbuf2.AppendFormat("http%04d.txt",conn->Id());
				PrintfTofile(textbuf2,"Added Request %d to HTTP connection %d (current load %d)\n",
					req->Id(), conn->Id(), least_load_count);
#endif
			}
			else if(most_loaded &&
				(urlManager->TooManyOpenConnections(HostName()) ||
				waitstatusload>
				g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal)
				))
			{
				conn = least_loaded;
			}
		}
#ifdef DEBUG_HTTP_REQDUMP
		else
		{
			PrintfTofile("winsck.txt","Added Request %d to Idle HTTP connection %d (current load %d)\n",
				req->Id(), conn->Id(), least_load_count);
			PrintfTofile("httpq.txt","Added Request %d to Idle HTTP connection %d (current load %d)\n",
				req->Id(), conn->Id(), least_load_count);
			OpString8 textbuf1;

			textbuf1.AppendFormat("htpr%04d.txt",req->Id());
			PrintfTofile(textbuf1,"Added Request %d to HTTP Idle connection %d (current load %d)\n",
				req->Id(), conn->Id(), least_load_count);
			OpString8 textbuf2;

			textbuf2.AppendFormat("http%04d.txt",conn->Id());
			PrintfTofile(textbuf2,"Added Request %d to HTTP Idle connection %d (current load %d)\n",
				req->Id(), conn->Id(), least_load_count);
		}
#endif



	}
	BOOL new_comm = FALSE;

	if(conn && 
#ifdef USE_SPDY
		!conn->IsSpdyConnection() &&
#endif // USE_SPDY
#ifdef _URL_EXTERNAL_LOAD_ENABLED_
	   !req->IsExternal() &&
#endif
	   (req->GetIsFormsRequest() || !GetMethodIsSafe(req->method)))
	{
		// Always use a new connection instead of an exisiting connection
		// when POSTing or sending forms because some servers close connections
		// Opera believes will stay open.
		// Deletes an idle connection to avoid problems with open connections
		if(conn->Idle())
		{
			conn->Stop();
			g_main_message_handler->RemoveCallBacks(this, conn->Id());
			if(conn->InList())
				conn->Out();
			OP_DELETE(conn);
		}
		else
			conn->SetNoNewRequests();
		conn = NULL;
	}

	if (conn && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HoldOnFillingPipelines) && conn->conn->GetRequestCount()>2
#if defined(WEB_TURBO_MODE) || defined(_EMBEDDED_BYTEMOBILE)
		&& !(
#if defined(WEB_TURBO_MODE)
			 (turbo_enabled && req->request->use_turbo && !req->info.bypass_turbo_proxy) ||
#endif // WEB_TURBO_MODE
			  !req->GetLoadDirect())
#endif // WEB_TURBO_MODE || _EMBEDDED_BYTEMOBILE
		)
	{
		if(req->InList())
			req->Out();
		if (delayed_request_list.Empty())
			req->Into(&delayed_request_list);
		else
		{
			Link *it = delayed_request_list.First();
			while (it && static_cast<HTTP_Request*>(it)->GetPriority() > req->GetPriority())
				it = it->Suc();
			if (it)
				req->Precede(it);
			else
				req->Into(&delayed_request_list);
		}
		return COMM_LOADING;
	}

	if(conn == NULL)
	{
		CommState status = CreateNewConnection(req, conn);
		if (status != COMM_LOADING)
			return status;
		new_comm = TRUE;
	}

	if(conn->InList())
		conn->Out();

#ifdef USE_SPDY
	// don't move the spdy connection to the end of the queue, we try to use as few SPDY connections as possible
	if (conn->IsSpdyConnection())
		conn->IntoStart(&connections);
	else
#endif // USE_SPDY
		conn->Into(&connections);

	req->SetProgressInformation(REQUEST_QUEUED, 0, servername->UniName());
	if(!conn->conn->AddRequest(req))
		return COMM_REQUEST_FAILED;

	CommState load_state = COMM_LOADING;

	int number_of_existing_idle_connections_tolerated = 0;

	if (new_comm)
	{
		load_state = conn->conn->Load();

		if (load_state == COMM_REQUEST_FAILED)
		{
			conn->Out();
			OP_DELETE(conn);
			return load_state;
		}

		// Since the new connection will be counted as idle, accept one idle connection when opening the extra idle connection
		number_of_existing_idle_connections_tolerated = 1;
	}

	if (req->m_tcp_connection_established_timout)
		conn->conn->SetMaxTCPConnectionEstablishedTimeout(req->m_tcp_connection_established_timout);

	// If pref is set, open an extra connection to the server for performance reason.
	// For TLS/SSL: Make sure the extra connection is set up after the call to conn->conn->Load():
	//
	// * If the idle connection is started first, it will not have request data for SSL false start, which only
	// happens on the first connection.
	// * Also the second connection will be a little delayed since it needs to wait for session negotiation for
	// the first connection.

	if (load_state != COMM_REQUEST_FAILED && req->info.open_extra_idle_connections && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseExtraIdleConnections))
		AttemptCreateExtraIdleConnection(req, number_of_existing_idle_connections_tolerated);

	return load_state;
}

void HTTP_Server_Manager::AttemptCreateExtraIdleConnection(HTTP_Request *req, int number_of_max_existing_idle_connections)
{
	if (!req || !req->http_conn || req->http_conn->info.connection_failed)
		return;

	PrefsCollectionNetwork *pc_net = g_pcnet;
	int max_connections_total = pc_net->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);
	unsigned max_connections_server = pc_net->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);

	if (!servername->GetIsLocalHost() &&
		// No need for extra idle connections to private networks and some devices on private networks cannot handle them.
		servername->GetNetType() != NETTYPE_PRIVATE &&
		GetNumberOfIdleConnections(TRUE) <= number_of_max_existing_idle_connections &&
		servername->GetConnectCount() < max_connections_server &&
		max_connections_total > 5 && g_url_comm_count < max_connections_total - 5)
	{
		HTTP_Connection *new_idle_connection = NULL;
		CommState status = CreateNewConnection(req, new_idle_connection);
		if (status == COMM_LOADING)
		{
			new_idle_connection->Into(&connections);
			status = new_idle_connection->conn->Load();
		}

		if (status != COMM_LOADING)
			OP_DELETE(new_idle_connection);
	}
}

CommState HTTP_Server_Manager::CreateNewConnection(HTTP_Request *req, HTTP_Connection *&conn)
{
	conn = NULL;
	if(servername->GetConnectCount() >= (UINT) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer) )
	{
#ifndef NO_FTP_SUPPORT
		if(!ftp_Manager->RemoveIdleConnection(TRUE, servername))
#endif //NO_FTP_SUPPORT
			http_Manager->RemoveIdleConnection(TRUE, servername);
	}

	SComm * OP_MEMORY_VAR comm = NULL;

#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(secure && req->method != HTTP_METHOD_CONNECT && servername != req->request->connect_host)
	{
		HTTP_request_st *conn_req = OP_NEW(HTTP_request_st, ());
		HTTP_Request * OP_MEMORY_VAR req2 = NULL;

		if(conn_req != NULL)
		{
			HTTP_request_st *original_req = req->request;
			conn_req->connect_host = original_req->connect_host;
			conn_req->connect_port = original_req->connect_port;
			conn_req->proxy = PROXY_HTTP;
			conn_req->origin_host = original_req->origin_host;
			conn_req->origin_port = original_req->origin_port;

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)  || defined(_CERTICOM_SSL_SUPPORT_)
			req2 = OP_NEW(HTTP_Request, (g_main_message_handler, HTTP_METHOD_CONNECT, conn_req, req->m_tcp_connection_established_timout, FALSE, FALSE));
#else
			req2 = OP_NEW(HTTP_Request, (g_main_message_handler, HTTP_METHOD_CONNECT, conn_req, req->m_tcp_connection_established_timout, FALSE));
#endif
			if (req2)
			{
#ifdef EXTERNAL_ACCEPTLANGUAGE
				req2->SetContextId(req->context_id);
#endif // EXTERNAL_ACCEPTLANGUAGE
				TRAPD(op_err, req2->ConstructL());

				if(OpStatus::IsSuccess(op_err))
				{
					req2->Headers.InsertHeader(req->Headers.FindHeader("Proxy-Authorization", FALSE));
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
					req2->proxy_ntlm_auth_element = req->proxy_ntlm_auth_element;
					if(req2->proxy_ntlm_auth_element)
					{
						req->proxy_ntlm_auth_element = NULL;
						HTTP_Server_Manager *master = req2->master;
						master->SetHTTP_1_1(TRUE);
						master->SetHTTP_1_1_Pipelined(FALSE);
						master->SetHTTP_ProtocolDetermined(TRUE);
					}
#endif
					req2->info.managed_request  = req->info.managed_request;
				}
				else
				{
					OP_DELETE(req2);
					req2  = NULL;
				}
			}
			else
				OP_DELETE(conn_req);
		}
		req->secondary_request = req2;
		if (!req2)
			return COMM_REQUEST_FAILED;

		comm = req2;
	}
	else
#endif
	{
#ifdef _EXTERNAL_SSL_SUPPORT_
		Comm *comm1 = Comm::Create(g_main_message_handler, servername, port, FALSE,
								   secure, req->info.proxy_request);
#else
		Comm *comm1 = Comm::Create(g_main_message_handler, servername, port);
#endif
		if(!comm1)
			return COMM_REQUEST_FAILED;

		if (req->m_tcp_connection_established_timout)
			comm1->SetMaxTCPConnectionEstablishedTimeout(req->m_tcp_connection_established_timout);

#ifdef WEB_TURBO_MODE
		// When in turbo mode we shouldn't try to do reconnects if the turbo server
		// isn't replying. Instead bypass it and go directly to the source.
		if (turbo_enabled)
			comm1->SetDoNotReconnect(TRUE);
#endif //WEB_TURBO_MODE

		if(req->info.managed_request)
			comm1->SetManagedConnection();

		if(!req->info.proxy_request)
			comm1->Set_NetType(req->Get_NetType());

		comm = comm1;
	}

#ifdef _SSL_SUPPORT_
#ifndef _EXTERNAL_SSL_SUPPORT_
	if(secure)
	{
		ProtocolComm *ssl_comm;

		ssl_comm = g_ssl_api->Generate_SSL(g_main_message_handler,  req->request->origin_host, req->request->origin_port, FALSE, TRUE);
		if(ssl_comm == NULL)
		{
			OP_DELETE(comm);
			return COMM_REQUEST_FAILED;
		}
		ssl_comm->SetNewSink(comm);
		comm = ssl_comm;
	}
#else //_EXTERNAL_SSL_SUPPORT_
#ifdef URL_ENABLE_INSERT_TLS
	if(secure &&
#if defined(_USE_HTTPS_PROXY)
	   !req->secondary_request &&
#endif
		!comm->InsertTLSConnection(req->request->origin_host, req->request->origin_port))
	{
		OP_DELETE(comm);
		return COMM_REQUEST_FAILED;
	}
#endif // URL_ENABLE_INSERT_TLS
#endif //_EXTERNAL_SSL_SUPPORT_
#elif defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	if( secure )
	{
		comm->SetSecure(secure);
	}
#endif // _NATIVE_SSL_SUPPORT_ || _CERTICOM_SSL_SUPPORT_
	
	NextProtocol protocol = NP_HTTP;
#ifdef USE_SPDY
#ifdef WEB_TURBO_MODE
	if (turbo_enabled && req->request->use_turbo && !req->GetLoadDirect())
		protocol = g_obml_config->GetTurboProxyProtocol();
	else
#endif // WEB_TURBO_MODE
	if (nextProtocolNegotiated != NP_NOT_NEGOTIATED)
		protocol = nextProtocolNegotiated;
	else if (secure && req->method != HTTP_METHOD_CONNECT && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyAlwaysForSSL, servername) != 0)
		protocol = static_cast<NextProtocol>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyAlwaysForSSL, servername) + 1);
	else if(!secure && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyWithoutSSL, servername) != 0)
		protocol = static_cast<NextProtocol>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyWithoutSSL, servername) + 1);
	else if (req->info.proxy_request && !req->GetLoadDirect() && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSpdyForHTTPProxy, servername) != 0)
		protocol = static_cast<NextProtocol>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSpdyForHTTPProxy, servername) + 1);
#endif // USE_SPDY

	return SetupHttpConnection(req, comm, conn, protocol);
}

CommState HTTP_Server_Manager::SetupHttpConnection(HTTP_Request *req, SComm *comm, HTTP_Connection *&conn, NextProtocol protocol)
{
	HttpRequestsHandler * OP_MEMORY_VAR http_conn;
#ifdef USE_SPDY
	if (protocol == NP_SPDY2 || protocol == NP_SPDY3)
		http_conn = OP_NEW(SpdyConnection, (protocol, g_main_message_handler, req->request->origin_host, req->request->origin_port, req->info.privacy_mode, secure));
	else
#endif // USE_SPDY
		http_conn = OP_NEW(HTTP_1_1, (g_main_message_handler, req->request->origin_host, req->request->origin_port, req->info.privacy_mode));

	if(http_conn)
	{
		TRAPD(op_err, http_conn->ConstructL());
		if(OpStatus::IsError(op_err))
		{
			g_memory_manager->RaiseCondition(op_err);
			OP_DELETE(http_conn);
			http_conn = NULL;
		}
	}

	if(http_conn == NULL)
	{
		OP_DELETE(comm);
		return COMM_REQUEST_FAILED;
	}

#ifdef URL_DISABLE_INTERACTION
	http_conn->SetUserInteractionBlocked(req->GetUserInteractionBlocked());
#endif

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	if (req->GetLoadDirect())
		http_conn->SetLoadDirect();
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
	if (protocol == NP_HTTP && urlManager->GetEmbeddedBmOpt())
		static_cast<HTTP_1_1*>(http_conn)->SetEboOptimized();
#endif //_EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
	if( req->request->use_turbo && !req->GetLoadDirect() )
		http_conn->SetTurboOptimized();
#endif // WEB_TURBO_MODE
#ifdef WEBSERVER_SUPPORT
	if (req->request->unite_connection)
	{
		http_conn->generic_info.unite_connection = req->request->unite_connection;
		http_conn->generic_info.unite_admin_connection = req->request->unite_admin_connection;
	}
#endif // WEBSERVER_SUPPORT

	http_conn->SetNewSink(comm);

	if(OpStatus::IsError(http_conn->SetCallbacks(this, NULL)))
	{
		OP_DELETE(http_conn);
		return COMM_REQUEST_FAILED;
	}

	http_conn->SetManager(this);
#ifdef DEBUG_HTTP_REQDUMP
		PrintfTofile("winsck.txt","Added Request %d to New  connection %d\n",
				req->Id(), http_conn->Id());
			OpString8 textbuf1;

			textbuf1.AppendFormat("htpr%04d.txt",req->Id());
			PrintfTofile(textbuf1,"Added Request %d to New  connection %d\n",
				req->Id(), http_conn->Id());
			OpString8 textbuf2;

			textbuf2.AppendFormat("http%04d.txt",http_conn->Id());
			PrintfTofile(textbuf1,"Added Request %d to New  connection %d\n",
				req->Id(), http_conn->Id());
#endif

	conn = OP_NEW(HTTP_Connection, (http_conn, protocol == NP_SPDY2 || protocol == NP_SPDY3));
	if(conn == NULL)
	{
		OP_DELETE(http_conn);
		return COMM_REQUEST_FAILED;
	}

	if (protocol == NP_HTTP)
		static_cast<HTTP_1_1*>(http_conn)->SetHTTP_1_1_flags(
#ifdef _EMBEDDED_BYTEMOBILE
							//make opera wait for first response before pipelining fully.
							(urlManager->GetEmbeddedBmOpt() && !req->GetLoadDirect()) ||
#endif //_EMBEDDED_BYTEMOBILE
							http_1_1,
#ifdef _EMBEDDED_BYTEMOBILE
							//make opera wait for first response before pipelining fully.
							!(urlManager->GetEmbeddedBmOpt() && !req->GetLoadDirect()) &&
#endif //_EMBEDDED_BYTEMOBILE
							http_protcol_determined,
							http_1_0_keep_alive,
							http_continue,
							http_chunking,
#ifdef _EMBEDDED_BYTEMOBILE
							//make opera wait for first response before pipelining fully.
							(urlManager->GetEmbeddedBmOpt() && !req->GetLoadDirect()) ||
#endif //_EMBEDDED_BYTEMOBILE
							http_1_1_pipelined,
#ifdef _EMBEDDED_BYTEMOBILE
							//make opera wait for first response before pipelining fully.
							(urlManager->GetEmbeddedBmOpt() && !req->GetLoadDirect()) ||
#endif //_EMBEDDED_BYTEMOBILE
							tested_http_1_1_pipelinablity);

#ifdef HTTP_BENCHMARK
	if(bench_active && http_no_pipeline)
	{
		http_conn->SetNoMoreRequests();
	}
#endif
	return COMM_LOADING;
}

void HTTP_Server_Manager::RemoveRequest(HTTP_Request *req)
{
	if(req == NULL)
		return;
	if(req->InList())
		req->Out();
	if(req->http_conn)
	{
#ifdef DEBUG_HTTP_REQDUMP
		PrintfTofile("httpq.txt","Removed Request %d from HTTP connection %d\n",
			req->Id(), req->http_conn->Id());
		OpString8 textbuf1;

		textbuf1.AppendFormat("htpr%04d.txt",req->Id());
		PrintfTofile(textbuf1,"Removed Request %d from HTTP connection %d\n",
			req->Id(), req->http_conn->Id());
		OpString8 textbuf2;

		textbuf2.AppendFormat("http%04d.txt",req->http_conn->Id());
		PrintfTofile(textbuf2,"Removed Request %d from HTTP connection %d\n",
			req->Id(), req->http_conn->Id());
#endif
		HTTP_1_1 *http = req->http_conn;
		http->RemoveRequest(req);
		req->http_conn = NULL;

		req->PopSink();
		{
			HTTP_Connection *conn = (HTTP_Connection *) GetConnection(http->Id());

			if(conn && conn->conn && !conn->conn->HasRequests())
			{
				g_main_message_handler->RemoveCallBacks(this, conn->Id());
				OP_DELETE(conn);
			}
		}
	}
	else if (req->GetSink())
	{
		HTTP_Connection *conn = (HTTP_Connection *) GetConnection(req->GetSink()->Id());
		OP_ASSERT(conn);
		if (conn && conn->conn)
			conn->conn->RemoveRequest(req);
	}
#ifdef DEBUG_HTTP_REQDUMP
	else
	{
		PrintfTofile("httpq.txt","Removed Request %d\n", req->Id());
		OpString8 textbuf1;

		textbuf1.AppendFormat("htpr%04d.txt",req->Id());
		PrintfTofile(textbuf1,"Removed Request %d\n", req->Id());
	}
#endif
	req->Stop();
}

void HTTP_Server_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
									__DO_START_TIMING(TIMING_COMM_LOAD_HT);
	switch (msg)
	{
		case MSG_COMM_LOADING_FAILED :
		case MSG_COMM_LOADING_FINISHED :
			{
				HTTP_Request *the_request_finished_loading = (HTTP_Request *) requests.First();

				while(the_request_finished_loading && the_request_finished_loading->Id() != (unsigned int) par1)
					the_request_finished_loading = (HTTP_Request *) the_request_finished_loading->Suc();

				if(the_request_finished_loading)
				{
					g_main_message_handler->RemoveCallBacks(this, par1);

					if(the_request_finished_loading->InList())
						the_request_finished_loading->Out();
				}

				HTTP_Connection *conn = (HTTP_Connection *) GetConnection(par1);
#ifdef DEBUG_HTTP_FILE
				PrintfTofile("winsck.txt", "   HTTP_Server_Manager:: Closing Connection id: %d. Connection %sfound\n", par1, (par1 != 0 ? "" : "not "));
#endif
				g_main_message_handler->RemoveCallBacks(this, par1);

				if(conn)
				{
					OP_DELETE(conn);
					conn = NULL;
				}
			}
			break;

		case MSG_COMM_DATA_READY:
			{
				HTTP_Request *ping = (HTTP_Request *) requests.First();

				while(ping && ping->Id() != (unsigned int) par1)
				{
					ping = (HTTP_Request *) ping->Suc();
				}

				if(!ping)
					break;

				char *buffer = (char *) g_memory_manager->GetTempBuf2k();
				ping->ReadData(buffer, g_memory_manager->GetTempBuf2kLen());
				op_memset(buffer, 0, g_memory_manager->GetTempBuf2kLen());
			}
			break;
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}


void HTTP_Server_Manager::ForciblyMoveRequest(HTTP_1_1 *to_connection)
{
	HTTP_Connection *conn;
	HTTP_Connection *most_loaded = NULL;
	HTTP_Connection *most_loaded_unsent = NULL;
	unsigned most_load_count=0;
	unsigned most_unsent_count=0;

	conn = (HTTP_Connection *) connections.First();
	while(conn)
	{
		if(conn->conn && conn->conn != to_connection)
		{
			unsigned current_load = conn->conn->GetRequestCount();
			unsigned current_unsent = conn->conn->GetUnsentRequestCount();

			if(!most_loaded_unsent || current_load > most_unsent_count)
			{
				most_loaded_unsent = conn;
				most_unsent_count = current_unsent;
			}
			if(!most_loaded || current_load > most_load_count)
			{
				most_loaded = conn;
				most_load_count = current_load;
			}
		}
		conn = (HTTP_Connection *) conn->Suc();
	}

	conn = NULL;
	if(most_unsent_count > 0)
		conn = most_loaded_unsent;
	else if(most_load_count > 1
#ifdef _BYTEMOBILE
			&& !urlManager->GetByteMobileOpt()
#endif
			)
		conn = most_loaded;


	if(conn)
	{
		HTTP_Request *req = conn->conn->MoveLastRequestToANewConnection();

		if(req && req->method != HTTP_METHOD_CONNECT)
		{
#ifdef OPERA_PERFORMANCE
			/*
            char buffer[32];
			op_itoa(conn->conn->connection_number, buffer, 10);
			OutputDebugStringA("Moving request from connection ");
			OutputDebugStringA(buffer);
			OutputDebugStringA(" to a new connection : ");
			OutputDebugStringA(req->request->connect_host->Name());
			OutputDebugStringA(req->request->path);
			OutputDebugStringA("\n");
			*/
#endif // OPERA_PERFORMANCE
			if(to_connection->AddRequest(req) != COMM_LOADING)
			{
				if(req->Load() == COMM_REQUEST_FAILED)
				{
					req->EndLoading();
					g_main_message_handler->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				}
			}
		}
	}
}

void HTTP_Server_Manager::ForceOtherConnectionsToClose(HTTP_1_1 *caller)
{
	Head connections_to_stop;
	HTTP_Connection *conn, *tempconn;

	while(RemoveIdleConnection(TRUE)) {}
	conn = (HTTP_Connection *) connections.First();
	while(conn)
	{
		tempconn = conn;
		conn = (HTTP_Connection *) conn->Suc();

		if(tempconn->conn && tempconn->conn != caller)
		{
			tempconn->Out();
			tempconn->Into(&connections_to_stop);
		}
	}
	while((conn = (HTTP_Connection *) connections_to_stop.First()) != NULL)
	{
		conn->Out();
		if(conn->conn)
			conn->conn->SetProgressInformation(STOP_FURTHER_REQUESTS, STOP_REQUESTS_THIS_CONNECTION,NULL);
		conn->Into(&connections);
	}
}

HTTP_Connection::HTTP_Connection(HttpRequestsHandler *nconn, BOOL isSpdy): 
#ifdef USE_SPDY
	isSpdyConnection(isSpdy), 
#endif // USE_SPDY
	conn(nconn)
{
	//PrintfTofile("httpcon.txt","Creating  conn %p : %lu\n", this, ++HTTP_count);
	if(conn)
		conn->SetConnElement(this);
}

HTTP_Connection::~HTTP_Connection()
{
	if(InList())
		Out();
	//HTTP_count --;
	//PrintfTofile("httpcon.txt","Destroying conn %p : %lu\n", this,HTTP_count);
	if(conn)
	{
		conn->Stop();
		conn->SetConnElement(NULL);
		SComm::SafeDestruction(conn);
		conn = NULL;
	}
}

#ifdef USE_SPDY
void HTTP_Connection::SetIsSpdyConnection(BOOL isSpdy)
{
	isSpdyConnection = isSpdy;
}
#endif // USE_SPDY

BOOL HTTP_Connection::Idle()
{
	return (conn && conn->Idle());
}

BOOL HTTP_Connection::AcceptNewRequests()
{
	return (conn && conn->AcceptNewRequests());
}

BOOL HTTP_Connection::HasPriorityRequest(UINT priority)
{
	return (conn && conn->HasPriorityRequest(priority));
}

void HTTP_Connection::RestartRequests()
{
	if(conn)
		conn->RestartRequests();
}

BOOL HTTP_Connection::HasId(unsigned int sid)
{
	return (conn && conn->HasId(sid));
}

unsigned int HTTP_Connection::Id()
{
	return conn ? conn->Id() : 0;
}

BOOL HTTP_Connection::SafeToDelete()
{
	return !conn || conn->SafeToDelete();
}

void HTTP_Connection::SetNoNewRequests()
{
	if(conn)
		conn->SetNoMoreRequests();
}

#ifdef _OPERA_DEBUG_DOC_
void HTTP_Manager::generateDebugDocListing(URL *url)
{
	HTTP_Server_Manager *conn = (HTTP_Server_Manager *) connections.First();

	url->WriteDocumentDataUniSprintf(UNI_L("<h1>HTTP Manager info</h1>\r\n"));
	url->WriteDocumentDataUniSprintf(UNI_L("<h5>Currently connected:</h5><pre>\r\n"));

	while(conn)
	{
		BOOL any_requests = FALSE;
		if (conn->requests.First() || conn->connections.First() || !conn->delayed_request_list.Empty())
			url->WriteDocumentDataUniSprintf(UNI_L("<h5>%s</h5>\r\n"), conn->HostName()->UniName());

		if (!conn->delayed_request_list.Empty())
		{
			url->WriteDocumentData(URL::KNormal, UNI_L("<h9>Delayed requests</h9>\r\n"));
			HTTP_Request *req = static_cast<HTTP_Request*>(conn->delayed_request_list.First());
			while (req)
			{
				OpString tmpstr;
				tmpstr.SetFromUTF8(req->GetPath());
				url->WriteDocumentData(URL::KNormal, UNI_L("<h9>url: "));
				url->WriteDocumentData(URL::KHTMLify, tmpstr.CStr());
				url->WriteDocumentData(URL::KNormal, UNI_L("</h9>\r\n"));

				req = (HTTP_Request *) req->Suc();
			}
			if (conn->requests.First())
				url->WriteDocumentData(URL::KNormal, UNI_L("<h9>Queued requests</h9>\r\n"));
		}

		HTTP_Request *req = (HTTP_Request *) conn->requests.First();
		while (req)
		{
			if (!any_requests)
			{
				any_requests = TRUE;
				url->WriteDocumentDataUniSprintf(UNI_L("<h7>Requests waiting to be processed:</h7>\r\n"));
			}
			OpString tmpstr;
			tmpstr.SetFromUTF8(req->GetPath());
			url->WriteDocumentData(URL::KNormal, UNI_L("<h9>url: "));
			url->WriteDocumentData(URL::KHTMLify, tmpstr.CStr());
			url->WriteDocumentData(URL::KNormal, UNI_L("</h9>\r\n"));

			req = (HTTP_Request *) req->Suc();
		}

		
		BOOL any_connections = FALSE;
		HTTP_Connection *connect = (HTTP_Connection *) conn->connections.First();
		int connection_number = 1;
		while(connect && connect->conn)
		{
			if (!any_connections)
			{
				any_connections = TRUE;
				url->WriteDocumentDataUniSprintf(UNI_L("<h7>Connections:</h7><h9>RequestCount: %d</h9>\r\n"), connect->conn->GetRequestCount());
			}

#ifdef USE_SPDY
			if (!connect->IsSpdyConnection())
#endif // USE_SPDY
			{
				HTTP_1_1 *http = static_cast<HTTP_1_1*>(connect->conn);
				url->WriteDocumentDataUniSprintf(UNI_L("<h9>ConnectionActive:%d<br/>HandlingHeader:%d<br/>ContinueAllowed:%d<br/>DisableMoreRequests:%d<br/>IsUploading:%d<br/>UseChunking:%d<br/>WaitingForChunkHeader:%d<br/>ExpectTrailer:%d<br/>TrailerMode:%d<br/>HostIs10:%d<br/>Http10KeepAlive:%d<br/>Http11Pipelined:%d<br/>HttpProtocolDetermined:%d<br/>FirstRequest:%d</h9>\r\n"),
				http->info.connection_active,http->info.handling_header,http->info.continue_allowed,http->generic_info.disable_more_requests,
				http->info.is_uploading, http->info.use_chunking, http->info.waiting_for_chunk_header,http->info.expect_trailer, http->info.trailer_mode,
				http->info.host_is_1_0, http->info.http_1_0_keep_alive, http->info.http_1_1_pipelined, http->info.http_protocol_determined, http->info.first_request);
			}

			url->WriteDocumentDataUniSprintf(UNI_L("<h9>Idle: %d</h9>\r\n"), connect->Idle());
			url->WriteDocumentDataUniSprintf(UNI_L("<h9>AcceptNewRequests: %d</h9>\r\n"), connect->conn->AcceptNewRequests());
			url->WriteDocumentDataUniSprintf(UNI_L("<h9>UnsentRequests: %d</h9>\r\n"), connect->conn->GetUnsentRequestCount());
			
			connection_number++;
			connect = (HTTP_Connection *) connect->Suc();
		}
		
		conn = (HTTP_Server_Manager *) conn->Suc();
	}
	url->WriteDocumentDataUniSprintf(UNI_L("</pre>\r\n"));
}
#endif

#ifdef OPERA_PERFORMANCE
Http_Request_Log_Point::Http_Request_Log_Point(int a_connection_number, int a_request_number, BOOL secure, const char *method, const char *server, const char *path, OpProbeTimestamp &a_time_request_created, int a_priority)
:connection_number(a_connection_number)
,request_number(a_request_number)
,size(0)
,http_code(0)
,priority(a_priority)
{
	request_string.Set(method);
	request_string.Append(secure ? " https://" : " http://");
	request_string.Append(server);
	request_string.Append(path);
	time_request_created = a_time_request_created;
	time_sent.timestamp_now();
	time_header_loaded.timestamp_now();
	time_completed.timestamp_now();
}

Message_Processing_Log_Point::Message_Processing_Log_Point(const char *message, OpProbeTimestamp &start_time, double &a_time)
{
	message_string.Set(message);
	time_start_processing = start_time;
	time = a_time;
}

Message_Delay_Log_Point::Message_Delay_Log_Point(const char *message, const char *old_message, OpProbeTimestamp &start_time, double &a_time)
{
	message_string.Set(message);
	old_message_string.Set(old_message);
	time_start_processing = start_time;
	time = a_time;
}

void HTTP_Manager::OnConnectionCreated(int connection_number, const char *server_name)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Connection_Log_Point *temp = OP_NEW(Http_Connection_Log_Point,(connection_number, server_name));
	if (temp)
		temp->Into(&http_connect_log);
}

void HTTP_Manager::OnConnectionConnected(int connection_number)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Connection_Log_Point *temp = (Http_Connection_Log_Point *)http_connect_log.First();
	while (temp)
	{
		if (connection_number == temp->GetConnectionNumber())
		{
			temp->SetConnected();
			break;
		}
		temp = (Http_Connection_Log_Point*)temp->Suc();
	}
}

void HTTP_Manager::OnConnectionClosed(int connection_number)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Connection_Log_Point *temp = (Http_Connection_Log_Point *)http_connect_log.First();
	while (temp)
	{
		if (connection_number == temp->GetConnectionNumber())
		{
			temp->SetClosed();
			break;
		}
		temp = (Http_Connection_Log_Point*)temp->Suc();
	}
}

void HTTP_Manager::OnPipelineDetermined(int connection_number, BOOL support_pipeline_fully)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Connection_Log_Point *temp = (Http_Connection_Log_Point *)http_connect_log.First();
	while (temp)
	{
		if (connection_number == temp->GetConnectionNumber())
		{
			temp->SetPipelined(support_pipeline_fully);
			break;
		}
		temp = (Http_Connection_Log_Point*)temp->Suc();
	}
}

void HTTP_Manager::OnRequestSent(int a_connection_number, int id, BOOL secure, const char *method, const char* server, const char* path, OpProbeTimestamp &time_request_created, int priority)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Request_Log_Point *temp = OP_NEW(Http_Request_Log_Point,(a_connection_number, id, secure, method, server, path, time_request_created, priority));
	if (temp)
		temp->Into(&http_request_log);
}

void HTTP_Manager::OnHeaderLoaded(int id, int code)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Request_Log_Point *temp = (Http_Request_Log_Point *)http_request_log.First();
	while (temp)
	{
		if (id == temp->GetRequestNumber())
		{
			temp->SetHeaderLoaded();
			temp->SetHttpCode(code);
			break;
		}
		temp = (Http_Request_Log_Point *)temp->Suc();
	}
}

void HTTP_Manager::OnResponseFinished(int id, OpFileLength size, OpProbeTimestamp &request_created)
{
	if (freeze_log || !urlManager->GetPerformancePageEnabled())
		return;

	Http_Request_Log_Point *temp = (Http_Request_Log_Point *)http_request_log.First();
	while (temp)
	{
		if (id == temp->GetRequestNumber())
		{
			temp->SetFinished();
			temp->SetSize(size);
			temp->time_request_created = request_created;
			break;
		}
		temp = (Http_Request_Log_Point *)temp->Suc();
	}
}

void HTTP_Manager::Freeze()
{
	freeze_log = TRUE;
}

void HTTP_Manager::Reset()
{
	while (!http_request_log.Empty())
	{
		Http_Request_Log_Point *temp = (Http_Request_Log_Point *)http_request_log.First();
		temp->Out();
		OP_DELETE(temp);
	}
	while (!http_connect_log.Empty())
	{
		Http_Connection_Log_Point *temp = (Http_Connection_Log_Point*)http_connect_log.First();
		temp->Out();
		OP_DELETE(temp);
	}
	freeze_log = FALSE;
}

HTTP_Manager::~HTTP_Manager()
{
	Reset();
}

void HTTP_Manager::WriteReport(URL &url, OpProbeTimestamp &start_time)
{
	url.WriteDocumentDataUniSprintf(UNI_L("<h2>TCP connections</h2><table><tr><th>Connection</th><th>Created</th><th>Connected</th><th>Closed</th><th>Pipelined</th><th>server</th></tr>"));
	Http_Connection_Log_Point *conn = (Http_Connection_Log_Point *)http_connect_log.First();

	UINT32 number_of_unused_connections = 0;

	while (conn)
	{
		OpString temp_string;
		temp_string.Set(conn->GetServerName());
		OpProbeTimestamp ts = conn->time_connection_created - start_time;
		double pa = ts.get_time();
		ts = conn->time_connected - start_time;
		double pb = ts.get_time();
		ts = conn->time_closed - start_time;
		double pc = ts.get_time();
		url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%i</td><td>%.0f</td><td>%.0f</td><td>%.0f</td><td>%d</td><td>%s</td></tr>"), conn->GetConnectionNumber(), pa, conn->connected ? pb : 0, conn->closed ? pc : 0, conn->GetPipelined(), temp_string.CStr());

		// Find out if any request is using this connection
		Http_Request_Log_Point *req = (Http_Request_Log_Point *)http_request_log.First();
		while (req)
		{
			if (req->GetConnectionNumber() == conn->GetConnectionNumber())
				break; // found, and req != NULL
			req = (Http_Request_Log_Point *)req->Suc();
		}

		// Did not find any requests using this connection
		if (req == NULL)
			number_of_unused_connections++;

		conn = (Http_Connection_Log_Point *)conn->Suc();
	}
	url.WriteDocumentDataUniSprintf(UNI_L("</table>"));


	url.WriteDocumentDataUniSprintf(UNI_L("<h2>HTTP Requests</h2><table><tr><th>Request created time</th><th>Request sent time</th><th>Header loaded time</th><th>Body loaded time</th><th>Body size</th><th>HTTP code</th><th>Connection</th><th>Priority</th><th>url</th></tr>"));
	Http_Request_Log_Point *temp = (Http_Request_Log_Point *)http_request_log.First();
	while (temp)
	{
		OpString temp_string;
		temp_string.Set(temp->GetName());
		OpProbeTimestamp ts = temp->time_sent - start_time;
		double pa = ts.get_time();
		ts = temp->time_header_loaded - start_time;//- temp->time_sent;
		double pb = ts.get_time();
		ts = temp->time_completed - start_time;// - temp->time_header_loaded;
		double pc = ts.get_time();
		ts = temp->time_request_created - start_time;
		double pd = ts.get_time();
		double size = (double)temp->size;
		url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%.0f</td><td>%.0f</td><td>%.0f</td><td>%.0f</td><td>%.0f</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>"), temp->http_code ? pd : 0, temp->http_code ? pa : 0, temp->http_code ? pb : 0, temp->http_code ? pc : 0, size > 0 ? size : 0, temp->http_code, temp->GetConnectionNumber(), temp->priority, temp_string.CStr());
		temp = (Http_Request_Log_Point *)temp->Suc();
	}
	url.WriteDocumentDataUniSprintf(UNI_L("</table>"));

	url.WriteDocumentDataUniSprintf(UNI_L("Number of connections created during page load: %u<br>\n"), http_connect_log.Cardinal());
	url.WriteDocumentDataUniSprintf(UNI_L("Number of unused connections created during page load: %u<br><br>\n"), number_of_unused_connections);

	// Connections that has been used will open an idle connections when closed. We estimate those for the current open connections
	int number_of_open_and_used_connections_at_loadfinish = 0;
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OpenIdleConnectionOnClose))
	{
		HTTP_Server_Manager *server = (HTTP_Server_Manager*) connections.First();
		while (server)
		{
			HTTP_Connection *connection = (HTTP_Connection *) server->connections.First();

			while (connection)
			{
				if (connection->conn->WasUsed())
					number_of_open_and_used_connections_at_loadfinish++;

				connection = (HTTP_Connection *) connection->Suc();
			}

			server = (HTTP_Server_Manager*) server->Suc();
		}
		url.WriteDocumentDataUniSprintf(UNI_L("Number of open and used connections at page load finish: %u<br>\n"), number_of_open_and_used_connections_at_loadfinish);

		// Each connection counted in number_of_open_and_used_connections_at_loadfinish will open a new connection when closed.
		url.WriteDocumentDataUniSprintf(UNI_L("Number of estimated unused connections created during page load and after: %u<br>\n"), number_of_unused_connections + number_of_open_and_used_connections_at_loadfinish);
	}
}
#endif // OPERA_PERFORMANCE
