/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen, Keith Hollis, Johan Borg, Morten Stenshorne, Rob Gregson
 */

#include "core/pch.h"

#include "modules/url/url_tools.h"

#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/commcleaner.h"
#include "modules/url/url_dns_man.h"

#include "modules/util/timecache.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/util/handy.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/hardcore/mh/mh.h"

#include "modules/locale/locale-enum.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"

#include "modules/util/gen_str.h"
#include "modules/util/cleanse.h"

#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#ifndef NO_FTP_SUPPORT
#include "modules/url/protocols/ftp.h"
#endif // NO_FTP_SUPPORT
#include "modules/url/url_tools.h"
#include "modules/dochand/docman.h"

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"

#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_RENDEZVOUS_SUPPORT)
#include "modules/webserver/webserver-api.h"
#endif // WEBSERVER_SUPPORT && WEBSERVER_RENDEZVOUS_SUPPORT
#if defined(_EXTERNAL_SSL_SUPPORT_)
#include "modules/externalssl/externalssl_com.h"
#endif

#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)	
#include "modules/webserver/webserver_direct_socket.h"
#endif

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
#ifdef GTK_DOWNLOAD_EXTENSION
#include "base/gdk/GdkSocket.h"
#endif
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef _EMBEDDED_BYTEMOBILE
#include "modules/url/protocols/ebo/zlib_transceive_socket.h"
#endif

#include "modules/url/url_socket_wrapper.h"

#ifdef SOCKS_SUPPORT
	#include "modules/socks/OpSocksSocket.h"
#endif

#ifdef PI_NETWORK_INTERFACE_MANAGER
#include "modules/url/internet_connection_status_checker.h"
#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

#if defined _DEBUG || defined _RELEASE_DEBUG_VERSION
#ifdef YNP_WORK
#define DEBUG_COMM_FILE
#define DEBUG_COMM_FILE_RECV
//#define DEBUG_COMM_WAIT
#define DEBUG_COMM_HEXDUMP
//#define DEBUG_COMM
//#define DEBUG_COMM_MSG
//#define DEBUG_MH
#endif
#endif

#ifdef _RELEASE_DEBUG_VERSION
#define DEBUG_COMM_FILE
#endif

#define RETRY_CONNECT_DELAY     5 // retry after 5 seconds
#define HAPPY_EYEBALLS_TIMEOUT  300 // start alternative connection after 300 milliseconds

#define COMM_CONNECTION_FAILURE 0x02

static const char CommEmptyLocalHost[] = "anyhost.anywhere";

#define RAISE_OOM_CONDITION(result) \
		do { if (result == OpStatus::ERR_NO_MEMORY && mh && mh->GetWindow()) \
				mh->GetWindow()->RaiseCondition(result); \
		} while (0)


OpSocketAddress& LocalHostAddr()
{
    if (g_local_host_addr==NULL)
    {
		OpStatus::Ignore(OpSocketAddress::Create(&g_local_host_addr));
		// FIXME: OOM
		if (g_local_host_addr)
			g_local_host_addr->FromString(UNI_L("127.0.0.1"));// what if IPv6?
    }
    return *g_local_host_addr;
}

Comm::Comm_strings::~Comm_strings()
{
	if(InList())
		Out();
	if(string)
	{
		OP_DELETEA(string);
	}
	string = NULL;
	len = 0;
}

Comm::Comm(
		   MessageHandler* msg_handler,
		   ServerName * _host,
		   unsigned short _port,
		   BOOL connect_only
		   ) : SComm(msg_handler, NULL)
{
	InternalInit(_host, _port, connect_only);
}

void Comm::InternalInit(
			ServerName * _host,
			unsigned short _port,
			BOOL connect_only
			)
{
	m_ConnectAttempts = 0;

#ifdef CORESOCKETACCOUNTING
    m_InstanceNumber=0;
#endif

	op_memset( &info, 0, sizeof(info) );
    info.only_connect = connect_only;
	info.connected = FALSE;
	info.closed = FALSE;
	info.do_not_reconnect = FALSE;
	info.pending_close = FALSE;
	info.sending_in_progress = FALSE;
#ifdef HAS_SET_HTTP_DATA
	info.is_uploading = FALSE;
#endif
#ifdef SOCKS_SUPPORT
	info.use_socks = FALSE;
#endif
	info.is_real_local_host = FALSE;
	info.is_resolving_host = FALSE;
	info.is_managed_connection = FALSE;
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	info.is_direct_embedded_webserver_connection = FALSE;
#endif // WEBSERVER_SUPPORT && WEBSERVER_DIRECT_SOCKET_SUPPORT
	info.resolve_is_from_initload = FALSE;

	m_SocketAddress = NULL;
	m_Socket = NULL;

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	m_local_socket_address = NULL;
#endif // OPSOCKET_GETLOCALSOCKETADDR

	m_alt_socket_address = NULL;
	m_alt_socket = NULL;

	LastError = 0;

    current_buffer = NULL;

#ifdef _EMBEDDED_BYTEMOBILE
	info.zlib_transceive = FALSE;
#endif //_EMBEDDED_BYTEMOBILE

	//m_TriedCount = 0;

	info.name_lookup_only = FALSE;

	m_Host = _host;

	use_nettype = NETTYPE_UNDETERMINED;

	m_max_tcp_connection_established_timeout = 0;

    m_Port = (_port > 0) ? _port : 80;

#ifdef HAS_SET_HTTP_DATA
	info.is_uploading = FALSE;
#endif
	m_RequestMsgMode = m_ConnectMsgMode = m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;

	last_waiting_for_connectionmsg = 0;

#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::Comm() - host=\"%s\", port=%d Tick %lu\n", id, m_Host->Name(), m_Port,(unsigned long) g_op_time_info->GetWallClockMS());
#endif
#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Create obj: %p\n", this);
#endif
}

Comm* Comm::Create(
	MessageHandler* msg_handler,
	ServerName * _host,
	unsigned short _port,
	BOOL connect_only
#ifdef _EXTERNAL_SSL_SUPPORT_	
	, BOOL secure
	, BOOL proxy_request
#endif	
	)
{
#ifdef _EXTERNAL_SSL_SUPPORT_	
	Comm* obj;
	if (secure || proxy_request) // A proxy connection might be upgraded to https connection 
	{
		obj = OP_NEW(External_SSL_Comm, (msg_handler,_host, _port, connect_only, secure));
	}
	else
	{
		 obj = OP_NEW(Comm, (msg_handler,_host, _port, connect_only));
	}
#else
	Comm* obj = OP_NEW(Comm, (msg_handler,_host, _port, connect_only));
#endif	
	
	
	if (!obj)
	{
		return NULL;
	}
	if (OpStatus::IsError(obj->Construct()))
	{
		OP_DELETE(obj);
		return NULL;
	}
	return obj;
}

OP_STATUS Comm::Construct()
{
	m_connection_established_timer.SetTimerListener(this);
	m_happy_eyeballs_timer.SetTimerListener(this);

	RETURN_IF_ERROR(OpSocketAddress::Create(&m_SocketAddress));
#ifdef OPSOCKET_GETLOCALSOCKETADDR
	RETURN_IF_ERROR(OpSocketAddress::Create(&m_local_socket_address));
#endif // OPSOCKET_GETLOCALSOCKETADDR

#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
	const uni_char *linkname = m_Host && m_Host->SocketAddress() ?
		m_Host->SocketAddress()->GetNetworkLinkName() : 0;
	TRAP_AND_RETURN(rc, m_SocketAddress->SetNetworkLinkNameL(linkname));
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT

	if (g_network_connection_status_checker)
		return g_network_connection_status_checker->GetComTable()->Add(this, this);
	return OpStatus::OK;
}

//***************************************************************************

Comm::~Comm()
{
	InternalDestruct();
}

void Comm::InternalDestruct()
{
	Comm *element = this;
	if (g_network_connection_status_checker)
		g_network_connection_status_checker->GetComTable()->Remove(this, &element);
	OP_ASSERT(element == this);

    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    while (lwe)
    {
        if (lwe->comm == this )
        {
            lwe->comm = NULL;
			lwe->status = COMM_WAIT_STATUS_IS_DELETED;
        }
        lwe = (CommWaitElm*) lwe->Suc();
    }
	lwe = (CommWaitElm*) g_ConnectionTempList->First();
    while (lwe)
    {
        if (lwe->comm == this )
        {
            lwe->comm = NULL;
			lwe->status = COMM_WAIT_STATUS_IS_DELETED;
        }
        lwe = (CommWaitElm*) lwe->Suc();
    }

#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm:~Comm() - call_count=%d\n", id, call_count);
#endif
	Stop();

	OP_DELETE(m_alt_socket);
	m_alt_socket = NULL;
	OP_DELETE(m_alt_socket_address);
	m_alt_socket_address = NULL;

	OP_DELETE(m_SocketAddress);
	m_SocketAddress = NULL;

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	OP_DELETE(m_local_socket_address);
	m_local_socket_address = NULL;
#endif // #OPSOCKET_GETLOCALSOCKETADDR

	if (g_url_dns_manager)
		g_url_dns_manager->RemoveListener(m_Host, this);
	OP_DELETE(m_Socket);
	m_Socket = NULL;

	if(info.is_resolving_host)
	{
		SetIsResolvingHost(FALSE);
		NormalCallCount blocker(this);
		TryLoadBlkWaitingComm();
	}

#ifdef DEBUG_MH
	PrintfTofile("msghan.txt", "Delete obj: %p\n", this);
	PrintfTofile("delcomm.txt", "   Delete comm: %p\n", this);
#endif
}
//***************************************************************************

CommState Comm::InitLoad()
{
#ifdef SOCKS_SUPPORT
	info.use_socks = g_opera->socks_module.IsSocksEnabled() &&
                (URL_Manager::UseProxyOnServer(m_Host, m_Port)
#ifdef URL_PER_SITE_PROXY_POLICY
				|| GetForceSOCKSProxy()
#endif
				);
#endif

	if(m_Host == NULL || !m_Host->Name())
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND));
		return COMM_REQUEST_FAILED;
	}

#ifdef PERFORM_SERVERNAME_LENGTH_CHECK
	if( m_Host->HasIllegalLength() )
	{
		CloseSocket();
		if (mh)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_ILLEGAL_URL));
		return COMM_REQUEST_FAILED;
	}
#endif // PERFORM_SERVERNAME_LENGTH_CHECK

	OpString hostname;
	OP_STATUS result = hostname.Set((const char*)m_Host->Name());

	if (result != OpStatus::OK)
	{
		RAISE_OOM_CONDITION(result);
		CloseSocket();
		return COMM_REQUEST_FAILED;
	}

#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	// To avoid load on the alien proxy and to avoid slow connection to local alien webserver,
	// we resolve all request to the local webserver to localhost IP. 
	if (g_webserver && g_webserver->MatchServer(m_Host, m_Port))
		info.is_direct_embedded_webserver_connection = TRUE;
#endif // WEBSERVER_SUPPORT && WEBSERVER_DIRECT_SOCKET_SUPPORT
	
	if (!m_Host->IsHostResolved() &&
#ifdef SOCKS_SUPPORT
			!(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS)) &&
#endif
			(uni_strspn(hostname.CStr(),UNI_L(".0123456789")) == (size_t) hostname.Length()
#ifdef SUPPORT_LITERAL_IPV6_URL
		|| hostname[0] == '['
#endif //SUPPORT_LITERAL_IPV6_URL
		))
	{
#ifdef SUPPORT_LITERAL_IPV6_URL
		if( hostname[0] == '[' )
		{
			if( hostname[hostname.Length()-1] == ']' )
			{
				hostname.Delete(0,1);
				hostname.Delete(hostname.Length()-1);
			}
			else
			{
				CloseSocket();
				if (mh)
					mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_ILLEGAL_URL));
				return COMM_LOADING;
			}
		}
#endif //SUPPORT_LITERAL_IPV6_URL
		OP_STATUS op_err = OpStatus::OK;
		{
			NormalCallCount blocker(this);
			op_err = m_Host->SetHostAddressFromString(hostname);
		}
		if (OpStatus::IsError(op_err))
		{
			RAISE_OOM_CONDITION(op_err);
			CloseSocket();
			return COMM_REQUEST_FAILED;
		}

		if(m_Host->IsHostResolved() && m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()))
		{
			info.is_real_local_host = TRUE;
			m_Host->SetIsLocalHost(TRUE);
		}
	}

	if((!m_Host->IsHostResolved() &&
#ifdef SOCKS_SUPPORT
			!(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS)) &&
#endif
			op_stricmp(m_Host->Name(),"localhost") == 0)
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	|| (info.is_direct_embedded_webserver_connection)
#endif // WEBSERVER_SUPPORT && WEBSERVER_DIRECT_SOCKET_SUPPORT
	)
	{
		OP_STATUS err = m_Host->SocketAddress()->Copy(&LocalHostAddr());
		if (OpStatus::IsError(err))
		{
			RAISE_OOM_CONDITION(err);
			CloseSocket();
			return COMM_REQUEST_FAILED;
		};

		m_Host->SetNetType(m_Host->SocketAddress()->GetNetType());
		m_Host->SetNeverExpire();
		info.is_real_local_host = TRUE;
		m_Host->SetIsLocalHost(TRUE);
	}

	if (!m_Host->IsHostResolved() &&
#ifdef SOCKS_SUPPORT
			!(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS)) &&
#endif
			(uni_strspn(hostname.CStr(),UNI_L(".0123456789")) == (size_t) hostname.Length()
#ifdef SUPPORT_LITERAL_IPV6_URL
		|| hostname.FindFirstOf(':') != KNotFound
#endif //SUPPORT_LITERAL_IPV6_URL
		))
	{
		CloseSocket();
		if (mh)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_ILLEGAL_URL));
		return COMM_LOADING;
	}
	else if (!m_Host->IsHostResolved()
#ifdef SOCKS_SUPPORT
			&& !(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS))
#endif
			)
	{
		if(m_Host->IsResolvingHost())
		{
#ifdef DEBUG_COMM_FILE
			OpString8 temp_hostname;
			temp_hostname.Set(hostname);
			PrintfTofile("winsck.txt","[%d] Comm::InitLoad() - Waiting for resolving of \"%s\" (\"%s\") Time {%lu}\n", id,  m_Host->Name(),temp_hostname.CStr(), op_time(NULL));
#endif
			SetIsResolvingHost(TRUE);
			SetProgressInformation( START_NAME_LOOKUP,0, m_Host->UniName());
			g_url_dns_manager->Resolve(m_Host, this);
			return COMM_LOADING;
		}

		/* get host-information */
#ifdef SYNCHRONOUS_HOST_RESOLVING
		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SyncDNSLookup) &&
			g_op_system_info->HasSyncLookup())
		{
            if (mh)
			{
#ifdef DEBUG_MH
				PrintfTofile("msghan.txt", "Load (call SetCallBack(CALL_SYNC_DNS)) obj: %p\n", this);
#endif
				RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBack(this, CALL_SYNC_DNS, Id()), COMM_REQUEST_FAILED);
				g_main_message_handler->PostMessage(CALL_SYNC_DNS, Id(), 0);
#ifdef DEBUG_MH
				PrintfTofile("msghan.txt", "Load (end PostMessage) obj: %p\n", this);
#endif
				SetIsResolvingHost(TRUE);
				return COMM_WAITING_FOR_SYNC_DNS;
			}
		}
		else
#endif // SYNCHRONOUS_HOST_RESOLVING
		{
			g_url_dns_manager->RemoveListener(m_Host, this);
			SetIsResolvingHost(TRUE);
			SetProgressInformation(START_NAME_LOOKUP, 0, m_Host->UniName());
			info.resolve_is_from_initload = TRUE; // Prevent racecondition
			result = g_url_dns_manager->Resolve(m_Host, this);
			info.resolve_is_from_initload = FALSE; // Prevent racecondition
		}
    }
	else if(m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()))
		info.is_real_local_host = m_Host->GetIsLocalHost();
	else if(info.name_lookup_only)
	{
		mh->PostMessage(MSG_COMM_NAMERESOLVED,Id(),0);
		return COMM_REQUEST_FINISHED;
	}


	return COMM_LOADING;
}

//***************************************************************************

CommState Comm::SetSocket()
{
	OP_MEMORY_VAR BOOL tried_to_remove_idle = FALSE;

	unsigned int max_connections_server = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);
	int max_connections_total = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);

#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "SetSocket (start) obj: %p - debug=%d\n", this, debug);
#endif
	if(m_Socket)
	{
		if(!info.is_managed_connection && m_Host != NULL)
			m_Host->DecConnectCount();
		if(!info.is_managed_connection && g_url_comm_count)
			g_url_comm_count --;
		OP_DELETE(m_alt_socket);
		m_alt_socket = NULL;
		OP_DELETE(m_Socket);
		m_Socket = NULL;
	}

	if(!info.is_managed_connection) 
	{
#ifndef NO_FTP_SUPPORT
		if (m_Host != NULL && (m_Host->GetConnectCount() < max_connections_server &&
			g_url_comm_count >= max_connections_total))
		{
			ftp_Manager->RemoveIdleConnection(TRUE);
		}
		else
		if (m_Host != NULL && m_Host->GetConnectCount() >= max_connections_server)
		{
			ftp_Manager->RemoveIdleConnection(TRUE, m_Host);
		}
#endif // NO_FTP_SUPPORT

		if (m_Host != NULL && (m_Host->GetConnectCount() < max_connections_server &&
			g_url_comm_count >= max_connections_total))
		{
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt", " [%d] SetSocket (removing idle HTTP) : \n", Id());
#endif
			http_Manager->RemoveIdleConnection(TRUE);
		}
		else 
		if (m_Host != NULL && m_Host->GetConnectCount() >= max_connections_server)
		{
			http_Manager->RemoveIdleConnection(TRUE, m_Host);
		}

		if (m_Host != NULL && (m_Host->GetConnectCount() >= max_connections_server ||
			g_url_comm_count >= max_connections_total))
		{
#ifdef DEBUG_COMM_FILE
	     PrintfTofile("winsck.txt","[%d] Comm::SetSocket() - add waiting comm\n", id);
#endif
			AddWaitingComm(COMM_WAIT_STATUS_LOAD);
			if(!last_waiting_for_connectionmsg != g_timecache->CurrentTime())
			{
				SetProgressInformation(WAITING_FOR_CONNECTION,0, m_Host->UniName());
				last_waiting_for_connectionmsg =  g_timecache->CurrentTime();
			}
			return COMM_WAITING;
		}
	}

#ifdef DEBUG_MH
	PrintfTofile("msghan.txt", "SetSocket (call ws_socket) obj: %p - debug=%d\n", this, debug);
#endif

retry_set_socket:;
	OP_DELETE(m_Socket);
	m_Socket = NULL;
	BOOL secure = FALSE;
#ifdef _EXTERNAL_SSL_SUPPORT_
	secure = info.is_secure;
#endif // _EXTERNAL_SSL_SUPPORT_
	OP_STATUS err;
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	if (info.is_direct_embedded_webserver_connection && g_webserver && g_webserver->IsRunning())
	{
		//Check to see if this is loaded through the unite protocol?
		BOOL is_owner = FALSE;
		Window *window = NULL;
		SetProgressInformation(GET_ORIGINATING_CORE_WINDOW, 0, &window);
		DocumentManager *docman = window ? window->DocManager() : NULL;
        URL document_url;
        if (docman)
            document_url = docman->GetCurrentURL(); 

        if (document_url.GetAttribute(URL::KIsUniteServiceAdminURL))
			is_owner = TRUE;

		WebserverDirectClientSocket *direct_socket = NULL;
		//add parameter
		err = WebserverDirectClientSocket::CreateDirectClientSocket(&direct_socket, this, secure, is_owner);
		m_Socket = direct_socket;	
	}
	else
		err = CreateSocket(&m_Socket, secure);
#else
	err = CreateSocket(&m_Socket, secure);
#endif
	
	if (OpStatus::IsError(err))
		return COMM_REQUEST_FAILED;

	if (!mh)
	{
		// loading cancelled (This is the case if socket call blocks - bug in Spry dialer?)
		Stop();
		return COMM_WAITING;
	}

#ifdef DEBUG_MH
	PrintfTofile("msghan.txt", "SetSocket (end ws_socket) obj: %p - debug=%d\n", this, debug);
#endif

	if (m_Socket == NULL)
	{
		//int wsa_err = WSAENOBUFS;
		int err_msg = URL_ERRSTR(SI, ERR_WINSOCK_RESOURCE_FAIL); // GetCommErrorMsg(wsa_err,UNI_L("SetSocket"));
		// Signal ERR_WINSOCK_RESOURCE_FAIL if there are no connection in use by Opera
		if (/*err_msg == URL_ERRSTR(SI, ERR_WINSOCK_RESOURCE_FAIL) && */ g_url_comm_count > 0)
		{
			if(!tried_to_remove_idle)
			{
#ifdef DEBUG_COMM_FILE
				PrintfTofile("winsck.txt","[%d] Comm::SetSocket() - ERR_WINSOCK_RESOURCE_FAIL,trying to free sockets\n", id);
#endif
				if(
#ifndef NO_FTP_SUPPORT
					ftp_Manager->RemoveIdleConnection(TRUE) ||
#endif // NO_FTP_SUPPORT
					http_Manager->RemoveIdleConnection(TRUE))
				{

					tried_to_remove_idle = TRUE;
					goto retry_set_socket;
				}
			}

#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] Comm::SetSocket() - ERR_WINSOCK_RESOURCE_FAIL,add waiting comm\n", id);
#endif
			AddWaitingComm(COMM_WAIT_STATUS_LOAD);
			if(!last_waiting_for_connectionmsg != g_timecache->CurrentTime())
			{
				SetProgressInformation(WAITING_FOR_CONNECTION,0, m_Host->UniName());
				last_waiting_for_connectionmsg = g_timecache->CurrentTime();
			}
			return COMM_WAITING;
		}
		else
		{
			Cleanup();
			if (mh)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), err_msg);
			return COMM_REQUEST_FAILED;
		}
	}

#ifdef DEBUG_COMM_FILE
	else
	{
		PrintfTofile("winsck.txt","[%d] Comm::SetSocket() - socket\n", id);
	}
#endif

	if(!info.is_managed_connection)
	{
	g_url_comm_count++;
	if (m_Host != NULL)
		m_Host->IncConnectCount();
	}

#ifdef DEBUG_MH
	PrintfTofile("msghan.txt", "SetSocket (start ws_setsockopt) obj: %p - debug=%d\n", this, debug);
#endif

#ifdef DEBUG_COMM_FILE
	{
		PrintfTofile("winsck.txt","[%d] Comm:SetSocket() - setsockid ok\n", id, call_count);
	}
#endif

	return COMM_LOADING;
}

OP_STATUS Comm::CreateSocket(OpSocket** aSocket, BOOL secure)
{
#ifdef SOCKS_SUPPORT
	if (m_Host->GetSocksServerName() != NULL) // socks proxy prescribed by autoproxy config;
		if (URL_Manager::UseProxyOnServer(m_Host, m_Port)) // this particular host is not an exempt from proxy-fying;
		{
			OpSocket *nonsocks_socket = NULL;

				// first create an inner non-socks-wrapped socket (which might use other wrappers, eg InternetConnectionWrapper)
			unsigned int flags = SocketWrapper::ALLOW_CONNECTION_WRAPPER;
			if (secure) flags |= SocketWrapper::USE_SSL;

			OP_STATUS s = SocketWrapper::CreateTCPSocket(&nonsocks_socket, this, flags);
			if (OpStatus::IsError(s))
				return s;

				// next, wrap the inner socket with an OpSocksSocket with the specific proxy server
			s = OpSocksSocket::Create(aSocket, *nonsocks_socket, this, secure, m_Host->GetSocksServerName(), m_Host->GetSocksServerPort());
			if (OpStatus::IsError(s))
			{
				OP_DELETE(nonsocks_socket);
				return OpStatus::ERR_NO_MEMORY;
			}
			return s;
		}
		else
			info.use_socks = FALSE; // but this is an exempt
	else if (!m_Host->IsSetNoSocksServer()) // only if there was no autoproxy config the general socks prefs are taken into account:
		info.use_socks = g_opera->socks_module.IsSocksEnabled() &&
                    (URL_Manager::UseProxyOnServer(m_Host, m_Port)
#ifdef URL_PER_SITE_PROXY_POLICY	
					|| GetForceSOCKSProxy()
#endif
					);
#endif

	unsigned int flags = SocketWrapper::ALLOW_CONNECTION_WRAPPER;
#ifdef SOCKS_SUPPORT
	if (info.use_socks) flags |= SocketWrapper::ALLOW_SOCKS_WRAPPER;
#endif // SOCKS_SUPPORT
	if (secure)   flags |= SocketWrapper::USE_SSL;

	return SocketWrapper::CreateTCPSocket(aSocket, this, flags);
}

//***************************************************************************

int Comm::GetCommErrorMsg(OpHostResolver::Error aError, const uni_char *wsa_func)
{
	switch (aError)
	{
	case OpHostResolver::NETWORK_NO_ERROR:
		return (LastError = 0); // not a symbolic name ?
    case OpHostResolver::ERROR_HANDLED:
		return (LastError = ERR_SSL_ERROR_HANDLED);
	case OpHostResolver::CANNOT_FIND_NETWORK:
		return (LastError = URL_ERRSTR(SI, ERR_COMM_NETWORK_UNREACHABLE));
		case OpHostResolver::TIMED_OUT:
		return (LastError = URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED));
	case OpHostResolver::NETWORK_ERROR:
        return (LastError = URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));

	case OpHostResolver::DNS_NOT_FOUND:
	case OpHostResolver::HOST_ADDRESS_NOT_FOUND:
        return (LastError = URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND));

#ifdef NEED_URL_CONNECTION_CLOSED_DETECTION
	case OpHostResolver::INTERNET_CONNECTION_CLOSED:
        //Log("Yngve: Received Connection Down (HostResolver)"); LogNl();
        if(!urlManager->GetPauseStartLoading())
        {
            urlManager->SetPauseStartLoading(TRUE);
	    	g_main_message_handler->PostMessage(MSG_URL_CLOSE_ALL_CONNECTIONS, 0,0);
        }
        return (LastError = URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));
#endif // NEED_URL_CONNECTION_CLOSED_DETECTION
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	case OpHostResolver::OUT_OF_COVERAGE:
		return (LastError = URL_ERRSTR(SI, ERR_OUT_OF_COVERAGE));
#endif
	case OpHostResolver::HOST_NOT_AVAILABLE:
		//return WSAEHOSTDOWN;
	default:
        return (LastError = URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));
	/* Need updated APIs
	case OpHostResolver::UNKNOWN_ERROR:
		{
		if (unknown_error_message.CStr())
			SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, unknown_error_message.CStr());
        return (LastError = URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
		*/
	}
}

int Comm::GetCommErrorMsg(OpSocket::Error aSocketErr, const uni_char *wsa_func)
{
	switch (aSocketErr)
	{
	case OpSocket::NETWORK_NO_ERROR:
		return (LastError = 0); // not a symbolic name ?
	case OpSocket::SOCKET_BLOCKING:
        return (LastError = URL_ERRSTR(SI, ERR_WINSOCK_BLOCKING));
	case OpSocket::CONNECTION_REFUSED:
        return (LastError = URL_ERRSTR(SI, ERR_COMM_CONNECTION_REFUSED));
	case OpSocket::CONNECTION_FAILED:
        return (LastError = URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED));

	case OpSocket::CONNECT_TIMED_OUT:
	case OpSocket::SEND_TIMED_OUT:
	case OpSocket::RECV_TIMED_OUT:
        return (LastError = URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED));

#ifdef _EXTERNAL_SSL_SUPPORT_
	case OpSocket::SECURE_CONNECTION_FAILED:
		return (LastError = Str::S_SSL_FATAL_ERROR_MESSAGE);
#endif
    case OpSocket::ERROR_HANDLED:
		return (LastError = ERR_SSL_ERROR_HANDLED);

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	case OpSocket::OUT_OF_COVERAGE:
		return (LastError = URL_ERRSTR(SI, ERR_OUT_OF_COVERAGE));
#endif
#ifdef NEED_URL_CONNECTION_CLOSED_DETECTION
	case OpSocket::INTERNET_CONNECTION_CLOSED:
        //Log("Yngve: Received Connection Down (Socket)"); LogNl();
        if(!urlManager->GetPauseStartLoading())
        {
            urlManager->SetPauseStartLoading(TRUE);
		    g_main_message_handler->PostMessage(MSG_URL_CLOSE_ALL_CONNECTIONS, 0,0);
        }
#endif //NEED_URL_CONNECTION_CLOSED_DETECTION

	case OpSocket::NETWORK_ERROR:
	default:
        return (LastError = URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));
	/* Need updated APIs
	case OpSocket::UNKNOWN_ERROR:
		{
		if (unknown_error_message.CStr())
			SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, unknown_error_message.CStr());
        return (LastError = URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
		*/
	}

}

//***************************************************************************

CommState Comm::Connect()
{
	OP_NEW_DBG(":Connect", "Comm");

	 if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return COMM_REQUEST_FAILED;

	 info.pending_close = FALSE;

	 // We set a max time for establishing connection
	 if (m_max_tcp_connection_established_timeout > 0)
		 m_connection_established_timer.Start((UINT32) m_max_tcp_connection_established_timeout * 1000);

	OP_DBG(("") << "Starting Happy Eyeballs timer");
	m_happy_eyeballs_timer.Start(HAPPY_EYEBALLS_TIMEOUT);

	 /*
    if (!m_Host->IsHostResolved())  // listening
        return COMM_LOADING;

*/
#ifdef DEBUG_COMM_FILE
    {
        PrintfTofile("winsck.txt","[%d] Comm::Connect() - started\n", id);
    }
#endif

	info.pending_close = FALSE;
	SetProgressInformation( (m_ConnectMsgMode != NO_STATE? m_ConnectMsgMode : START_CONNECT), 0, m_Host->UniName());
#ifdef DEBUG_COMM_FILE
# ifdef CORESOCKETACCOUNTING
        PrintfTofile("winsck.txt","[%d] Comm::Connect() - mh->SetCallBack() done (sock=%d) Tick %lu\n", id, m_InstanceNumber,(unsigned long) g_op_time_info->GetWallClockMS());
# else
        PrintfTofile("winsck.txt","[%d] Comm::Connect() - mh->SetCallBack() done (sock=%p) Tick %lu\n", id, m_Socket,(unsigned long) g_op_time_info->GetWallClockMS());
# endif
#endif

	/* register each unique ip address tried */
	//if(m_TriedCount < MAX_CONNECT_ATTEMPTS)
	//	m_TriedHostAddr[ m_TriedCount++] = m_Host->SocketAddress();
	/* do a connect to host */ 

#ifdef _DEBUG
	OP_DBG(("") << "Host addresses:");
	m_Host->LogAddressInfo();
#endif // _DEBUG

	OpSocketAddress* address = m_Host->GetNextSocketAddress();

	if (!address)
	{
		m_Host->ResetLastTryFailed();

		address = m_Host->GetNextSocketAddress();

		if(!address)
			return COMM_REQUEST_FAILED;
	}


	OP_STATUS err = m_SocketAddress->Copy(address);
	if (OpStatus::IsError(err))
		return COMM_REQUEST_FAILED;

	m_SocketAddress->SetPort(m_Port);

#ifdef _DEBUG
	OpString addr_string;
	m_SocketAddress->ToString(&addr_string);
	OP_DBG(("") << "Main connection, address chosen: " << addr_string);
#endif // _DEBUG

	m_Host->MarkAddressAsConnecting(m_SocketAddress);

#ifdef SOCKS_SUPPORT
	if (info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS) && !m_Host->IsHostResolved())
	{
		err = static_cast<OpSocksSocket*>(m_Socket)->Connect(m_Host->UniName(), m_Port);
	}
	else
#endif // SOCKS_SUPPORT
	err = m_Socket->Connect(m_SocketAddress);

	if (g_network_connection_status_checker->GetNetworkInterfaceStatus(m_Socket) == InternetConnectionStatusChecker::NETWORK_STATUS_IS_OFFLINE)
	{
		m_Host->MarkAddressAsFailed(m_SocketAddress);
		return COMM_REQUEST_FAILED;
	}

	if (OpStatus::IsError(err))
	{
		m_Host->MarkAddressAsFailed(m_SocketAddress);
		return COMM_REQUEST_FAILED;
	}

#ifdef DEBUG_COMM_FILE
	{
//		PrintfTofile("winsck.txt","[%d] Comm::Connect() - connect Tick %lu\n", id,/*op_*/GetTickCount());
	}
#endif

	m_ConnectAttempts++;

	SetProgressInformation(ACTIVITY_REPORT , 0, NULL);

	/* return successfully */
	return COMM_LOADING;
}

CommState Comm::ConnectAltSocket()
{
	OP_NEW_DBG(":ConnectAltSocket", "Comm");

	OP_ASSERT(!g_main_message_handler->HasCallBack(this, MSG_URL_CONNECT_ALT_SOCKET, Id()));

	if (info.do_not_reconnect || g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return COMM_REQUEST_FAILED;

	if (info.pending_close)
		return COMM_REQUEST_FAILED;

#ifdef DEBUG_COMM_FILE
    {
        PrintfTofile("winsck.txt","[%d] Comm::ConnectAltSocket() - started\n", id);
    }
#endif

	BOOL secure = FALSE;
#ifdef _EXTERNAL_SSL_SUPPORT_
	secure = info.is_secure;
#endif // _EXTERNAL_SSL_SUPPORT_

	if (!m_Host)
		return COMM_REQUEST_FAILED;

	if (!m_SocketAddress)
		return COMM_REQUEST_FAILED;

	OP_ASSERT(m_Socket);

	OP_DELETE(m_alt_socket);
	m_alt_socket = NULL;
	OP_DELETE(m_alt_socket_address);
	m_alt_socket_address = NULL;

	OpSocketAddress::Family cur_family = m_SocketAddress->GetAddressFamily();
	OpSocketAddress* address = m_Host->GetAltSocketAddress(cur_family);
	if (!address)
		return COMM_REQUEST_FAILED;

	OP_STATUS err = OpSocketAddress::Create(&m_alt_socket_address);
	if (OpStatus::IsError(err))
		return COMM_REQUEST_FAILED;
	OP_ASSERT(m_alt_socket_address);

	err = m_alt_socket_address->Copy(address);
	if (OpStatus::IsError(err))
		return COMM_REQUEST_FAILED;

	m_alt_socket_address->SetPort(m_Port);

#ifdef _DEBUG
	OpString addr_string;
	m_alt_socket_address->ToString(&addr_string);
	OP_DBG(("") << "Alternative connection, address chosen: " << addr_string);
#endif

	err = CreateSocket(&m_alt_socket, secure);
	if (OpStatus::IsError(err))
	{
		m_alt_socket = NULL;
		OP_DELETE(m_alt_socket_address);
		m_alt_socket_address = NULL;
		return COMM_REQUEST_FAILED;
	}
	OP_ASSERT(m_alt_socket);

	m_Host->MarkAddressAsConnecting(m_SocketAddress);
	err = m_alt_socket->Connect(m_alt_socket_address);

	if (OpStatus::IsError(err))
	{
		m_Host->MarkAddressAsFailed(m_alt_socket_address);
		return COMM_REQUEST_FAILED;
	}

	if (g_network_connection_status_checker->GetNetworkInterfaceStatus(m_alt_socket) == InternetConnectionStatusChecker::NETWORK_STATUS_IS_OFFLINE)
	{
		m_Host->MarkAddressAsFailed(m_alt_socket_address);
		return COMM_REQUEST_FAILED;
	}

	m_ConnectAttempts++;

	/* return successfully */
	return COMM_LOADING;
}

//***************************************************************************

CommState Comm::Load()
{

#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Load obj: %p - debug=%d\n", this, debug);
#endif

	info.name_lookup_only = FALSE;

	return Load_Stage2();
}

void Comm::SetMaxTCPConnectionEstablishedTimeout(UINT timeout)
{
	// If a connection attempt has been started but not yet connected,
	// and no timeout has been set, we set the timeout.
	//
	// We also set the timeout if this timeout is shorter than the previous set.

	if (timeout && (!m_max_tcp_connection_established_timeout || timeout < m_max_tcp_connection_established_timeout))
	{
		if (m_Socket && !info.connected)
		{
			double time_until_firing = - (m_connection_established_timer.TimeSinceFiring() / 1000);
			// Negative value indicates the number of milliseconds left until it fires

			if (time_until_firing == 0 || time_until_firing > timeout) // only set new timeout if it shortens the timeout
				m_connection_established_timer.Start((UINT32) timeout * 1000);
		}

		m_max_tcp_connection_established_timeout = timeout;
	}
}

CommState Comm::Load_Stage2()
{
	{
		NormalCallCount blocker(this);
		RemoveDeletedComm();
	}

#ifdef DEBUG_MH
    PrintfTofile("winsck.txt", "Load (call cleaning) obj: %p - debug=%d\n", this, debug);
#endif

    Stop();
    Clear();

#ifdef DEBUG_MH
    PrintfTofile("winsck.txt", "Load (call InitLoad) obj: %p - debug=%d\n", this, debug);
#endif

	m_ConnectAttempts = 0;
	//m_TriedCount = 0;

	if (m_Host && m_Host->IsHostResolved())
		m_Host->MarkAllAddressesAsUntried();

	if(urlManager->GetPauseStartLoading())
	{
        AddWaitingComm(COMM_WAIT_STATUS_LOAD);
		SetProgressInformation(WAITING_FOR_CONNECTION,0, m_Host->UniName());
		return COMM_LOADING;
	}

    CommState stat = InitLoad();

#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Load (end InitLoad) obj: %p - debug=%d\n", this, debug);
#endif

    if (stat == COMM_WAITING_FOR_SYNC_DNS || stat == COMM_WAITING)
        return COMM_LOADING;
	else if (stat == COMM_LOADING && (
#ifdef SOCKS_SUPPORT
			(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS)) ||
#endif
			m_Host->IsHostResolved()))
    {
		if (use_nettype != NETTYPE_UNDETERMINED &&
			m_Host->GetNetType() < use_nettype &&
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
			// WebServer mixes NETTYPE_LOCALHOST and NETTYPE_PUBLIC nettypes,
			// but we don't want a cross network error when we end up here
			// after there has been a redirect between them
			!info.is_direct_embedded_webserver_connection &&
#endif // WEBSERVER_SUPPORT && WEBSERVER_DIRECT_SOCKET_SUPPORT
			!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, m_Host)) 
		{
			// Local host can connect to anyone
			// private can connect to private and public
			// public can only connect to public.
			CloseSocket();
			if (mh)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, 
					(m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()) && !info.is_real_local_host ?
							ERR_SSL_ERROR_HANDLED : (int) GetCrossNetworkError(use_nettype, m_Host->GetNetType())));
			return COMM_LOADING; // error message will arrive
		}

		stat = SetSocket();
        if (stat == COMM_LOADING)
            stat = Connect();
        else if (stat == COMM_WAITING)
            stat = COMM_LOADING;
    }
#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Load (end) obj: %p - debug=%d\n", this, debug);
#endif
    return stat;
}

CommState Comm::LookUpName(ServerName *name)
{
#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Lookup obj: %p - debug=%d\n", this, debug);
#endif

	m_Host = name;

	info.name_lookup_only = TRUE;

	RemoveDeletedComm();

    Stop();
    Clear();

#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Load (call InitLoad) obj: %p - debug=%d\n", this, debug);
#endif

    CommState stat = InitLoad();

#ifdef DEBUG_MH
    PrintfTofile("msghan.txt", "Load (end InitLoad) obj: %p - debug=%d\n", this, debug);
#endif

    if (stat == COMM_WAITING_FOR_SYNC_DNS || stat == COMM_WAITING)
        return COMM_LOADING;
    else if (stat == COMM_LOADING && m_Host->IsHostResolved())
    {
		mh->PostMessage(MSG_COMM_NAMERESOLVED,Id(),0);
		return COMM_REQUEST_FINISHED;
    }
    return stat;
}

//***************************************************************************

#ifdef DEBUG_COMM_STAT
static time_t current_period = 0;
static unsigned long bytes_sent_count = 0;
static unsigned long bytes_recv_count = 0;
#endif

CommState Comm::SendDataToConnection()
{
	if (m_Socket == NULL || info.closed)
		return COMM_REQUEST_FAILED;

	 if (g_network_connection_status_checker->GetNetworkInterfaceStatus(m_Socket) == InternetConnectionStatusChecker::NETWORK_STATUS_IS_OFFLINE)
	 {
		 if (m_Socket)
			 OnSocketSendError(m_Socket, OpSocket::INTERNET_CONNECTION_CLOSED);
		 return COMM_REQUEST_FAILED;
	 }

	info.sending_in_progress = TRUE;
    while (MoreRequest())
    {
		if(!current_buffer || current_buffer->buffer_sent)
			break; // waiting for data to be sent by socket

#ifdef DEBUG_COMM_HEXDUMP
		{
			OpString8 textbuf;

			textbuf.AppendFormat("Comm::SendDataToConnection Sending data to socket from %d Tick %lu",Id(),(unsigned long) g_op_time_info->GetWallClockMS());
			DumpTofile((unsigned char *) current_buffer->string,(unsigned long) current_buffer->len,textbuf,"winsck.txt");
		}
#endif
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] Comm::SendDataToConnection():  %p\n", Id(), current_buffer->string);
		PrintfTofile("winsck2.txt","[%d] Comm::SendDataToConnection():  %p\n", Id(), current_buffer->string);
#endif

#if defined(DEBUG_COMM_STAT) || defined (DEBUG_COMM_FILE)
		unsigned long buf_len = current_buffer->len;
#endif
#ifdef DEBUG_ENABLE_OPASSERT
		Comm_strings* original = current_buffer;
#endif
		/* send request to host */
		current_buffer->buffer_sent = TRUE;
#ifdef NETWORK_STATISTICS_SUPPORT
		unsigned int send_length = current_buffer->len;
#endif
		OP_STATUS result = m_Socket->Send(current_buffer->string, current_buffer->len);

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
			PrintfTofile("winsck.txt","[%d] Comm::SendDataToConnection(): blocked %p\n", Id(), (current_buffer ? current_buffer->string : NULL));
#endif
			OP_ASSERT(current_buffer == NULL || original == current_buffer);
			if(current_buffer)
				current_buffer->buffer_sent = FALSE;
			if (result == OpStatus::ERR_NO_MEMORY)
				RAISE_OOM_CONDITION(result);
            // Assumption that the only error from a Send could be 'blocking'.
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] Comm::SendDataToConnection() - WSAEWOULDBLOCK, waiting\n", id);
#endif
			info.sending_in_progress = FALSE;
			return COMM_REQUEST_WAITING;
		}

#ifdef NETWORK_STATISTICS_SUPPORT
		if (NETTYPE_PRIVATE != m_Host->GetNetType())
			urlManager->GetNetworkStatisticsManager()->addBytesSent(send_length);
#endif // NETWORK_STATISTICS_SUPPORT

#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] Comm::SendDataToConnection() - sent=%d\n", id, buf_len);
#endif
    }

	SetProgressInformation(ACTIVITY_REPORT , 0, NULL);

    info.sending_in_progress = FALSE;

    return COMM_REQUEST_FINISHED;
}

//***************************************************************************
void Comm::OnTimeOut(OpTimer* timer)
{
	OP_NEW_DBG(":OnTimeOut", "Comm");

	if (timer == &m_connection_established_timer)
	{
		m_connection_established_timer.Stop();
		m_happy_eyeballs_timer.Stop();

		// If this times out, we do not retry connect
		info.do_not_reconnect = TRUE;

		m_Host->ClearSocketAddresses();

		SetProgressInformation(REQUEST_CONNECTION_ESTABLISHED_TIMOUT, (unsigned long)m_max_tcp_connection_established_timeout, m_Host->UniName());

		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));
		NormalCallCount blocker(this);

		if (m_alt_socket)
		{
			OP_DELETE(m_alt_socket);
			m_alt_socket = NULL;

			OP_DELETE(m_alt_socket_address);
			m_alt_socket_address = NULL;
		}

		m_Socket->Close();
		info.do_not_reconnect = TRUE;
		info.connected = FALSE;

		info.pending_close = TRUE;
#ifdef PI_NETWORK_INTERFACE_MANAGER
		g_network_connection_status_checker->CloseAllSocketsOnNetworkInterface(NULL, m_SocketAddress);
#endif // PI_NETWORK_INTERFACE_MANAGER

		EndLoading();
		Stop();
		Clear();
	}
	else if (timer == &m_happy_eyeballs_timer)
	{
		OP_DBG(("") << "Timeout on Happy Eyeballs timer");
		m_happy_eyeballs_timer.Stop();

		OP_DBG(("") << "Starting alternative connection");
		ConnectAltSocket();
	}
	else
		OP_ASSERT(!"Comm::OnTimeOut: unknown timer!");
}

CommState Comm::StartLoading()
{
	RequestMoreData();

	SetProgressInformation((m_RequestMsgMode != NO_STATE ? m_RequestMsgMode : START_REQUEST) , 0, m_Host->UniName());

	/* send request to host */

	CommState send_status = SendDataToConnection();

	/*
	if (send_status != COMM_REQUEST_WAITING && current_buffer)
	{
		delete current_buffer;
		current_buffer = NULL;
	}
	*/

	if (send_status == COMM_REQUEST_FAILED)
		Stop();

	return send_status;
}

//***************************************************************************

BOOL Comm::Closed()
{
	return (info.connected && (m_Socket == NULL || info.closed));
}

BOOL Comm::Connected() const
{
	return info.connected;
}

//***************************************************************************

unsigned Comm::ReadData(char *buf, unsigned bbuf_len)
{
	if (!Connected() || Closed()/* || info.read_data*/)
		return 0;

#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::ReadContent() - buf_len: %d\n", id, bbuf_len);
#endif

	//info.read_data = TRUE;

									__DO_START_TIMING(TIMING_COMM_LOAD);
	unsigned int bytesRecv = 0;
	OP_STATUS result = m_Socket->Recv(buf, bbuf_len, &bytesRecv);
									__DO_STOP_TIMING(TIMING_COMM_LOAD);

	if (OpStatus::IsError(result))
	{
		RAISE_OOM_CONDITION(result);

		int wsa_err = GetLastError();
		if (mh && wsa_err && wsa_err != URL_ERRSTR(SI, ERR_WINSOCK_BLOCKING))
		{
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] Comm::ReadContent() - failed (error=%d)\n", id, wsa_err);
#endif
			if (/*wsa_err != WSAECONNABORTED || */ PendingClose())
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) wsa_err);

			CloseSocket();
		}
#ifdef DEBUG_COMM_FILE
		else
		{
			PrintfTofile("winsck.txt","[%d] Comm::ReadContent() - WSAEWOULDBLOCK\n", id);
		}
#endif
		return 0;
	}

	// PH: -1 used by unixsocket.cpp to indicate EWOULDBLOCK
	if( (int) bytesRecv == ((int)-1) )
		return 0;

	SetProgressInformation(ACTIVITY_REPORT , 0, NULL);

#ifdef NETWORK_STATISTICS_SUPPORT
		if (NETTYPE_PRIVATE != m_Host->GetNetType())
			urlManager->GetNetworkStatisticsManager()->addBytesReceived(bytesRecv);
#endif // NETWORK_STATISTICS_SUPPORT

									__DO_ADD_TIMING_PROCESSED(TIMING_COMM_LOAD, bytesRecv);	{
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::ReadContent() - recv %u bytes, Tick %lu\n", id, bytesRecv, (unsigned long) g_op_time_info->GetWallClockMS());
#ifdef DEBUG_COMM_FILE_RECV
	DumpTofile((unsigned char *) buf, bytesRecv, "Received data", "winsck.txt");
#endif
#endif
#ifdef DEBUG_COMM_STAT
	{
		if(current_period != prefsManager->CurrentTime())
		{
			if(current_period)
				PrintfTofile("statsend.txt", "%lu, %lu, %lu\n",current_period, bytes_sent_count, bytes_recv_count);

			current_period = prefsManager->CurrentTime();
			bytes_recv_count = bytes_sent_count = 0;
		}
		bytes_recv_count += bytesRecv;

	}
#endif
	}

	return (int) bytesRecv;
}

//***************************************************************************

void Comm::CloseSocket()
{
	info.closed = TRUE;
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::CloseSocket() - Marking close Tick %lu\n", id,(unsigned long) g_op_time_info->GetWallClockMS());
#endif
	if (m_Socket && SafeToDelete())
    {
		if(!info.is_managed_connection && g_url_comm_count)
			g_url_comm_count --;

#ifdef NEED_URL_ABORTIVE_CLOSE
		if(m_abortive_close)
		{
			m_Socket->SetAbortiveClose();
			if (m_alt_socket)
				m_alt_socket->SetAbortiveClose();
		}
#endif

		OP_DELETE(m_alt_socket);
		m_alt_socket = NULL;
		OP_DELETE(m_Socket);
		m_Socket = NULL;

        if (m_Host != NULL)
        {
			unsigned int max_connections_server = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);
			int max_connections_total = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);

			if(!info.is_managed_connection)
            m_Host->DecConnectCount();
			if (g_url_comm_count < max_connections_total &&
				m_Host->GetConnectCount() < max_connections_server)
				TryLoadWaitingComm(m_Host);
			else if (g_url_comm_count < max_connections_total)
				TryLoadWaitingComm(NULL);
       }
		else
			TryLoadWaitingComm(m_Host);
    }

    // there may be callbacks registred before socket is set !!! (changed 24/03/96)
    Cleanup();

    RemoveWaitingComm();
}

//***************************************************************************

void Comm::Stop()
{
#ifdef DEBUG_MH
	PrintfTofile("msghan.txt", "Stop() obj: %p\n", this);
#endif
#ifdef DEBUG_COMM_FILE
    {
        PrintfTofile("winsck.txt","[%d] Comm::Stop():  call_count=%d\n", id, call_count);
    }
#endif

	ClearBuffer();

	g_url_dns_manager->RemoveListener(m_Host, this);

	if (info.is_resolving_host)
		SetIsResolvingHost(FALSE);

    CloseSocket();
}

//***************************************************************************

void Comm::SetIsResolvingHost(BOOL flag)
{
	if(info.is_resolving_host && !flag)
	{
		info.is_resolving_host = FALSE;
		m_Host->SetIsResolvingHost(FALSE);
	}
	else if(!info.is_resolving_host && flag)
	{
		info.is_resolving_host = TRUE;
		m_Host->SetIsResolvingHost(TRUE);
	}
#ifdef _DEBUG
	else
	{
		// Should never get here
		OP_ASSERT(0);
	}
#endif
}

void Comm::OnHostResolved(OpHostResolver* aResolver)
{
	NormalCallCount blocker(this);

	SetIsResolvingHost(FALSE);
	if (m_SocketAddress == 0)
	{
		BOOL abort = TRUE;
		if (m_ConnectAttempts++ < MAX_NAME_LOOKUP_ATTEMPTS)
		{
#ifdef DEBUG_MH
			PrintfTofile("msghan.txt", "WINSOCK_HOSTINFO_READY (call InitLoad) obj: %p\n", this);
#endif
			CommState retry_stat = InitLoad();
#ifdef DEBUG_MH
			PrintfTofile("msghan.txt", "WINSOCK_HOSTINFO_READY (end InitLoad) obj: %p\n", this);
#endif
			abort = (retry_stat != COMM_WAITING_FOR_SYNC_DNS && retry_stat != COMM_WAITING);
		}

		if (abort)
		{
			CloseSocket();
			if (mh)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND));
		}
		return;
    }


	if (aResolver)
	{
		OP_STATUS err = aResolver->GetAddress(m_SocketAddress, 0);

		if (OpStatus::IsMemoryError(err))
			RAISE_OOM_CONDITION(OpStatus::ERR_NO_MEMORY);
	}

#ifdef SCOPE_RESOURCE_MANAGER
	OpHostResolver::Error aError = OpHostResolver::NETWORK_NO_ERROR;
	SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_DNS_LOOKUP_END, &aError);
#endif // SCOPE_RESOURCE_MANAGER

	if(m_Host->GetCrossNetwork())
	{
		m_SocketAddress->Copy(m_Host->SocketAddress());
		if(!m_SocketAddress->IsValid())
		{
			CloseSocket();
			if (mh)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, GetCrossNetworkError(m_Host->GetNetType(), m_Host->GetAttemptedNetType()));
			return;
		}
	}

	if(info.name_lookup_only)
	{
		mh->PostMessage(MSG_COMM_NAMERESOLVED,Id(),0);
		return;
	}

	if (use_nettype != NETTYPE_UNDETERMINED &&
		m_Host->GetNetType() < use_nettype && 
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, m_Host)) 
	{
		// Local host can connect to anyone
		// private can connect to private and public
		// public can only connect to public.
		CloseSocket();
		if (mh)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()) && !info.is_real_local_host ?
							ERR_SSL_ERROR_HANDLED : (int) GetCrossNetworkError(use_nettype, m_Host->GetNetType())));
		return;
	}

	if(info.resolve_is_from_initload)
		return;

    CommState stat = SetSocket();
    if (stat == COMM_LOADING)
    {
#ifdef DEBUG_MH
        PrintfTofile("msghan.txt", "Comm::HandleHostInfoReady(): obj: %p\n", this);
#endif
        stat = Connect();
    }

	TryLoadBlkWaitingComm();
}

void Comm::OnHostResolverError(OpHostResolver* aHostResolver,
					           OpHostResolver::Error aError)
{
    if (aError ==OpHostResolver::NETWORK_NO_ERROR)
		return;

#ifdef SCOPE_RESOURCE_MANAGER
	SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_DNS_LOOKUP_END, &aError);
#endif // SCOPE_RESOURCE_MANAGER

	NormalCallCount blocker(this);
	SetIsResolvingHost(FALSE);

    int err_msg = GetCommErrorMsg(aError,UNI_L("HostInfoReady"));
	mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
}

//***************************************************************************

void Comm::OnSocketConnected(OpSocket* aSocket)
{
	OP_NEW_DBG(":OnSocketConnected", "Comm");
	OP_DBG(("") << "aSocket:      " << aSocket);
	OP_DBG(("") << "m_Socket:     " << m_Socket);
	OP_DBG(("") << "m_alt_socket: " << m_alt_socket);

	m_connection_established_timer.Stop();
	m_happy_eyeballs_timer.Stop();

	// If one of these sockets is NULL at this point -
	// we have a serious bug somewhere.
	OP_ASSERT(m_Socket);
	OP_ASSERT(aSocket);

	// If we have alternative socket -
	// let's destroy the socket which didn't connect.
	if (m_alt_socket)
	{
		if (aSocket == m_Socket)
		{
			OP_DELETE(m_alt_socket);
			m_alt_socket = NULL;

			OP_DELETE(m_alt_socket_address);
			m_alt_socket_address = NULL;
		}
		else if (aSocket == m_alt_socket)
		{
			OP_DELETE(m_Socket);
			m_Socket = m_alt_socket;
			m_alt_socket = NULL;

			OP_DELETE(m_SocketAddress);
			m_SocketAddress = m_alt_socket_address;
			m_alt_socket_address = NULL;
		}
		else
			OP_ASSERT(!"Comm::OnSocketConnected: Unknown socket!");
	}

	OP_ASSERT(m_SocketAddress);
	m_Host->MarkAddressAsSucceeded(m_SocketAddress);

	g_main_message_handler->UnsetCallBack(this, MSG_URL_CONNECT_ALT_SOCKET);

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	OpStatus::Ignore(m_Socket->GetLocalSocketAddress(m_local_socket_address));
#endif // #OPSOCKET_GETLOCALSOCKETADDR

	info.connected = TRUE;
	info.closed = FALSE;
	info.pending_close = FALSE;

#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::SocketConnectedL() - Marking open Tick %lu\n", id,(unsigned long) g_op_time_info->GetWallClockMS());
#endif

	NormalCallCount blocker(this);
#ifdef NEED_URL_ASYNC_DATASENT_UPDATE
	OP_STATUS op_err = mh->SetCallBack(this,MSG_COMM_DATA_SENT,Id(),0);
	if( OpStatus::IsError(op_err) )
	{
		Stop();
		RAISE_OOM_CONDITION(op_err);
		mh->PostMessage(MSG_COMM_LOADING_FAILED, id, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return;
	}
#endif

#ifdef OPSOCKET_OPTIONS
	OpStatus::Ignore(aSocket->SetSocketOption(OpSocket::SO_TCP_NODELAY, TRUE));
#endif // OPSOCKET_OPTIONS

    if (info.only_connect && mh)
        mh->PostMessage(MSG_COMM_CONNECTED, id, 0);

    if (ConnectionEstablished() == COMM_CONNECTION_FAILURE)
    {
        Stop();
		mh->PostMessage(MSG_COMM_LOADING_FAILED, id, URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED));
    }
    else
    {
		StartLoading();
	}
}

void Comm::OnSocketDataReady(OpSocket* aSocket)
{
	NormalCallCount blocker(this);

#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::SocketDataReady() \n",id);
    PrintfTofile("loading.txt","Comm::SocketDataReady() %lu\n",(unsigned long) g_op_time_info->GetWallClockMS());
#endif

	if(info.closed)
		return;

    ProcessReceivedData();
}

void Comm::SocketDataSent()
{
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::SocketDataSent(): current_data=%p %d\n", Id(), (current_buffer ? current_buffer->string : NULL), (current_buffer ? current_buffer->buffer_sent : FALSE) );
#endif

	if (current_buffer != NULL && current_buffer->buffer_sent)
	{
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck2.txt","[%d] Comm::SocketDataSent(): current_data=%p\n", Id(), current_buffer->string);
#endif
		OP_DELETE(current_buffer);
		current_buffer = NULL;
	}

	if (!info.sending_in_progress)
	{
		int send_stat = SendDataToConnection();

		if (send_stat == COMM_REQUEST_FAILED)
		{
			Stop();
		}
	}
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::SocketDataSent(): Exit\n", Id());
#endif
}

void Comm::OnSocketClosed(OpSocket* aSocket)
{
	if(info.closed)
		return;

	NormalCallCount blocker(this);

	info.pending_close = TRUE;

	EndLoading();
    Stop();
	mh->PostMessage(MSG_COMM_LOADING_FINISHED, id, 0);
}

void Comm::OnSocketConnectError(OpSocket* aSocket, OpSocket::Error aError)
{
	OP_NEW_DBG(":OnSocketConnectError", "Comm");
	OP_DBG(("") << "aSocket:      " << aSocket);
	OP_DBG(("") << "m_Socket:     " << m_Socket);
	OP_DBG(("") << "m_alt_socket: " << m_alt_socket);

	LastError = 0;
	if (aError==OpSocket::NETWORK_NO_ERROR)
		return;

	// If one of these sockets is NULL at this point -
	// we have a serious bug somewhere.
	OP_ASSERT(m_Socket);
	OP_ASSERT(aSocket);

	// If we have alternative socket -
	// let's drop the erroring socket.
	if (m_alt_socket)
	{
		m_happy_eyeballs_timer.Stop();

		if (aSocket == m_Socket)
		{
			OP_DELETE(m_Socket);
			m_Socket = m_alt_socket;
			m_alt_socket = NULL;

			OP_ASSERT(m_SocketAddress);
			m_Host->MarkAddressAsFailed(m_SocketAddress);

			OP_DELETE(m_SocketAddress);
			m_SocketAddress = m_alt_socket_address;
			m_alt_socket_address = NULL;
		}
		else if (aSocket == m_alt_socket)
		{
			OP_DELETE(m_alt_socket);
			m_alt_socket = NULL;

			OP_ASSERT(m_alt_socket_address);
			m_Host->MarkAddressAsFailed(m_alt_socket_address);

			OP_DELETE(m_alt_socket_address);
			m_alt_socket_address = NULL;
		}
		else
			OP_ASSERT(!"Comm::OnSocketConnectError: Unknown socket!");

		if (!g_main_message_handler->HasCallBack(this, MSG_URL_CONNECT_ALT_SOCKET, Id()))
		{
			g_main_message_handler->PostMessage(MSG_URL_CONNECT_ALT_SOCKET, Id(), 0);
			g_main_message_handler->SetCallBack(this, MSG_URL_CONNECT_ALT_SOCKET, Id());
		}

		return;
	}

	NormalCallCount blocker(this);

    // the only_connect test is special for ftp data connection !!! ???
    if (m_ConnectAttempts++ < MAX_CONNECT_ATTEMPTS && m_Host != NULL &&
		!m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()) && !info.only_connect && !info.do_not_reconnect &&
		(aError == OpSocket::CONNECTION_REFUSED ||
		 aError == OpSocket::CONNECTION_FAILED  ||
		 aError == OpSocket::CONNECT_TIMED_OUT) )
    {
        // THIS IS NOT FULLY IMPLEMENTED: connection only is retried once if
        // MAX_CONNECT_ATTEMPTS is larger than 0 !!!
        // Should we put in waiting list if not empty, or may be use a timer ???

		info.connected = FALSE;
        CloseSocket();
        Clear();

		//m_Host->SetSocketAddress(NULL);//FIXME:OOM (returns OP_STATUS)

        BOOL abort = info.only_connect; // abort if ftp data connection

        if(!abort && !m_Host->GetNextSocketAddress())
            abort = TRUE;

        if (!abort)
        {
            g_main_message_handler->PostMessage(MSG_COMM_RETRY_CONNECT, Id(), 0);
            g_main_message_handler->SetCallBack(this, MSG_COMM_RETRY_CONNECT, Id());
        }

        if (abort && mh)
        {
            int err_msg = GetCommErrorMsg(aError,UNI_L("ConnectReady"));
			mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
        }
	}
    else
    {
        CloseSocket();
		if (mh)
        {
			if((aError == OpSocket::CONNECTION_REFUSED || aError == OpSocket::CONNECTION_FAILED) &&
				m_Host->SocketAddress()->IsHostEqual(&LocalHostAddr()) &&
				!info.is_real_local_host)
			{
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, ERR_SSL_ERROR_HANDLED);
			}
			else
			{
				int err_msg = GetCommErrorMsg(aError,UNI_L("ConnectReady"));
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
			}
        }
    }
}

void Comm::OnSocketReceiveError(OpSocket* aSocket, OpSocket::Error aError)
{
	if(info.closed)
		return;

	LastError = 0;
    if (aError==OpSocket::NETWORK_NO_ERROR || aError == OpSocket::SOCKET_BLOCKING)
		return;

	NormalCallCount blocker(this);
#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::HandleWinsockDataReady(): FD_READ - WSAGETSELECTERROR=%d\n", id, aError);
#endif
    Stop();

    if (mh)
    {
        int err_msg = GetCommErrorMsg(aError,UNI_L("FD_READ"));
		mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
    }
}

void Comm::OnSocketSendError(OpSocket* aSocket, OpSocket::Error aError)
{
	LastError = 0;

	if(info.closed)
		return;

    if (aError==OpSocket::NETWORK_NO_ERROR || aError == OpSocket::SOCKET_BLOCKING)
		return;

	NormalCallCount blocker(this);
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%d] Comm::HandleWinsockDataReady(): FD_WRITE - WSAGETSELECTERROR=%d\n", id, aError);
#endif
	ClearBuffer();
	CloseSocket();
	if (mh)
	{
		int err_msg = GetCommErrorMsg(aError, UNI_L("FD_WRITE"));
		mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
	}
}

void Comm::OnSocketDataSent(OpSocket* aSocket, UINT aBytesSent)
{
#ifdef URL_TRUST_ONSOCKETDATASENT
	if(info.is_uploading
#ifdef WEBSOCKETS_SUPPORT
		|| info.is_full_duplex
#endif
		)
		SetProgressInformation(UPLOADING_PROGRESS , aBytesSent, m_Host->UniName());
#endif // URL_TRUST_ONSOCKETDATASENT
	SetProgressInformation(ACTIVITY_REPORT , 0, NULL);
#ifdef NEED_URL_ASYNC_DATASENT_UPDATE
	// post message to this object so we can handle things in
	// the right order, independantly of underlying OS.
	mh->PostMessage(MSG_COMM_DATA_SENT,Id(),0);
#else
	NormalCallCount blocker(this);
	// Epoc seems to prefer the following line, which breaks Win & Unix:
	SocketDataSent();
#endif
}

#ifndef URL_TRUST_ONSOCKETDATASENT
void Comm::OnSocketDataSendProgressUpdate(OpSocket* aSocket, UINT aBytesSent)
{
#ifdef HAS_SET_HTTP_DATA
	if(info.is_uploading
#ifdef WEBSOCKETS_SUPPORT
		|| info.is_full_duplex
#endif
		)
		SetProgressInformation(UPLOADING_PROGRESS , aBytesSent, m_Host->UniName());
#endif
}
#endif // URL_TRUST_ONSOCKETDATASENT

void Comm::OnSocketConnectionRequest(OpSocket* aSocket)
{
}

void Comm::OnSocketCloseError(OpSocket* aSocket, OpSocket::Error aError)
{
	LastError = 0;

	if(info.closed)
		return;

	NormalCallCount blocker(this);

	if (aError==OpSocket::NETWORK_NO_ERROR && !info.do_not_reconnect && m_ConnectAttempts++ < MAX_CONNECT_ATTEMPTS)
	{
		// special for WinLink tcp/ip ????

#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] Comm::SocketCloseErrorL(): FD_CLOSE - WSAGETSELECTERROR=%d\n", id, aError);
#endif
		Stop();
		Clear();

		CommState stat = SetSocket();
		if (stat == COMM_LOADING)
		{
#ifdef DEBUG_MH
			PrintfTofile("msghan.txt", "Comm::SocketCloseErrorL(): FD_CLOSE obj: %p\n", this);
#endif
			stat = Connect();
		}

		if (stat != COMM_LOADING && stat != COMM_WAITING)
		{
			CloseSocket();
		}
	}
	else
	{
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] Comm::SocketCloseErrorL(): FD_CLOSE - WSAGETSELECTERROR=%d\n", id, aError);
#endif
		Stop();
		if (mh)
		{
			int err_msg = GetCommErrorMsg(aError,UNI_L("FD_CLOSE"));
			mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) err_msg);
		}
	}
}

#ifdef CORESOCKETACCOUNTING
void Comm::OnSocketInstanceNumber(int aInstanceNumber)
{
    if(m_InstanceNumber != 0)
#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::SocketInstanceNumber() INSTANCE NUMBER !=0\n",id);
#endif
    m_InstanceNumber = aInstanceNumber;

#ifdef DEBUG_COMM_FILE
    PrintfTofile("winsck.txt","[%d] Comm::SocketInstanceNumber() Socket Instance Number = %d\n", id,m_InstanceNumber);
#endif

}

#endif
//***************************************************************************

#ifdef SYNCHRONOUS_HOST_RESOLVING
void Comm::HandleSynchronousDNS()
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SyncDNSLookup))
	{
		g_main_message_handler->UnsetCallBack(this, CALL_SYNC_DNS, Id());

		if (m_ConnectAttempts == 0)
			SetProgressInformation(START_NAME_LOOKUP , 0, m_Host->UniName());

		OpString hostname;
		TRAPD(op_err, hostname.SetL(m_Host->Name()));
		if( OpStatus::IsError(op_err) )
		{
			SetIsResolvingHost(FALSE);
			if( op_err == OpStatus::ERR_NO_MEMORY )
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			TryLoadBlkWaitingComm();
			return;
		}

		OpStackAutoPtr<OpHostResolver> resolver(NULL);
		OpHostResolver *resolver1 = NULL;
        OP_STATUS op_err2 = SocketWrapper::CreateHostResolver(&resolver1, NULL);
		resolver.reset(resolver1);

		if(OpStatus::IsError(op_err2))
		{
			SetIsResolvingHost(FALSE);
			if(op_err2 == OpStatus::ERR_NO_MEMORY)
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			TryLoadBlkWaitingComm();
			return;
		}

		OpHostResolver::Error hr_error;
		OP_STATUS err = resolver->ResolveSync(hostname.CStr(), m_SocketAddress, &hr_error);

		SetIsResolvingHost(FALSE);

		if (OpStatus::IsError(err))
		{
			CloseSocket();
			if (mh)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) GetCommErrorMsg(hr_error,UNI_L("SyncGetHostByName")));

			TryLoadBlkWaitingComm();
			return;
		}

		if (OpStatus::IsMemoryError(m_Host->SetSocketAddress(m_SocketAddress)))
			RAISE_OOM_CONDITION(OpStatus::ERR_NO_MEMORY);

		UINT count = resolver->GetAddressCount();
		for (UINT i=1; i < count; ++i)
		{
			OpStackAutoPtr<OpSocketAddress> a(NULL);
			OpSocketAddress *a1;
			RETURN_VOID_IF_ERROR(OpSocketAddress::Create(&a1));
			a.reset(a1);
			RETURN_VOID_IF_ERROR(resolver->GetAddress(a.get(), i));
			RETURN_VOID_IF_ERROR(m_Host->AddSocketAddress(a.release()));
		}
		resolver.reset();

		if(info.name_lookup_only)
		{
			mh->PostMessage(MSG_COMM_NAMERESOLVED,Id(),0);
			return;
		}

		CommState stat = SetSocket();
		if (stat == COMM_LOADING)
		{
#ifdef DEBUG_MH
			PrintfTofile("msghan.txt", "Comm::HandleSynchronousDNS() - CALL_SYNC_DNS obj: %p\n", this);
#endif
			stat = Connect();
		}
		else if (stat == COMM_WAITING)
			stat = COMM_LOADING;

		TryLoadBlkWaitingComm();
    }
}
#endif // SYNCHRONOUS_HOST_RESOLVING

//***************************************************************************

void Comm::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    if (!mh)
    {
        // this means that connection is canceled
        Stop();
        return;
    }
	NormalCallCount blocker(this);

									__DO_START_TIMING(TIMING_COMM_LOAD_CB);
    switch (msg)
    {
#ifdef NEED_URL_ASYNC_DATASENT_UPDATE
	case MSG_COMM_DATA_SENT:
		{
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%d] Comm::HandleCallback(): MSG_COMM_DATA_SENT\n", Id() );
#endif
			SocketDataSent();
			break;
		}
#endif
	case MSG_COMM_RETRY_CONNECT:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_COMM_RETRY_CONNECT, Id());

			CommState stat = COMM_REQUEST_FAILED;
			if(!m_Host->IsHostResolved()
#ifdef SOCKS_SUPPORT
					&& !(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS))
#endif
					)
			{
				stat = InitLoad();
			}
			else
				stat = COMM_LOADING;

			if (stat == COMM_LOADING && (
#ifdef SOCKS_SUPPORT
					(info.use_socks && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS)) ||
#endif
					m_Host->IsHostResolved()))
			{
				stat = SetSocket();

				if (stat == COMM_LOADING)
				{
#ifdef DEBUG_MH
					PrintfTofile("msghan.txt", "Comm::HandleCallback - MSG_COMM_RETRY_CONNECT obj: %p\n", this);
#endif
					stat = Connect();
				}
			}

			if (stat != COMM_LOADING && stat != COMM_WAITING && stat != COMM_WAITING_FOR_SYNC_DNS )
			{
				CloseSocket();
				if (mh)
					mh->PostMessage(MSG_COMM_LOADING_FAILED, id, (unsigned) stat);
			}

			break;
		}

	case MSG_URL_CONNECT_ALT_SOCKET:
		{
			ConnectAltSocket();
			break;
		}

#ifdef SYNCHRONOUS_HOST_RESOLVING
	case CALL_SYNC_DNS:
		{
			HandleSynchronousDNS();

			break;
		}
#endif // SYNCHRONOUS_HOST_RESOLVING

    default:
        break;
    }
									__DO_STOP_TIMING(TIMING_COMM_LOAD_CB);
}

OP_STATUS Comm::SetCallbacks(MessageObject *master, MessageObject *parent)
{
	static const OpMessage messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
		MSG_COMM_LOADING_FAILED,
    };

	return mh->SetCallBackList(parent, Id(), messages, ARRAY_SIZE(messages));
}

//***************************************************************************

//***************************************************************************

#ifdef COMM_LOCALHOSTNAME
#ifdef COMM_LOCALHOSTNAME_IS_SERVERNAME
ServerName *
#else
const char*
#endif
Comm::GetLocalHostName()
{
    if (!CommUseLocalHostInitialized)
    {
        CommUseLocalHostInitialized = TRUE;

		OpString name;
		OpHostResolver* resolver = NULL;
        OP_STATUS op_err = SocketWrapper::CreateHostResolver(&resolver, NULL);

		if(OpStatus::IsSuccess(op_err))
		{
			OpHostResolver::Error resolv_err = OpHostResolver::NETWORK_NO_ERROR;
			op_err = resolver->GetLocalHostName(&name, &resolv_err);

			if (OpStatus::IsSuccess(op_err) && resolv_err == OpHostResolver::NETWORK_NO_ERROR)
			{
				CommUseLocalHost = g_url_api->GetServerName(name,TRUE);
				if(CommUseLocalHost)
					CommUseLocalHost->Increment_Reference();
			}
		}
#ifdef COMM_LOCALHOSTNAME_IS_SERVERNAME
		if(!CommUseLocalHost)
		{
			CommUseLocalHost = g_url_api->GetServerName(CommEmptyLocalHost,TRUE);
			if(CommUseLocalHost)
				CommUseLocalHost->Increment_Reference();
		}
#endif
		OP_DELETE(resolver);
    }
#ifndef COMM_LOCALHOSTNAME_IS_SERVERNAME
	if (CommUseLocalHostInitialized && CommUseLocalHost != NULL)
		return CommUseLocalHost->Name();
    else
        return CommEmptyLocalHost;
#else
	return CommUseLocalHost;
#endif
}
#endif // COMM_LOCALHOSTNAME

void Comm::SetDoNotReconnect(BOOL val)
{
	info.do_not_reconnect = val;
}

BOOL Comm::PendingClose() const
{
	return ((info.connected && (info.pending_close || info.closed)) || IsDeleted());
}

/***************************************************************************
****************************************************************************

	THE FOLLOWING FUNCTIONS ARE FROM COMMGEN.CPP
*/

void CommWaitElm::Init(Comm* c, BYTE state)
{
    comm = c;
    status = state;
	time_set = g_timecache->CurrentTime();
}

void Comm::AddWaitingComm(BYTE status)
{
	if(!g_ConnectionWaitList)
		return;

#ifdef DEBUG_COMM_WAIT
    FILE* lfp = fopen("c:\\klient\\host.txt", "a");
    fprintf(lfp, "  +++ Wait list: ");
    CommWaitElm *we = (CommWaitElm*) g_ConnectionWaitList->First();
    while (we)
    {
        fprintf(lfp, "  , %d (%s)\n", we->comm->Id(),  we->comm->m_Host->Name());
        we = (CommWaitElm*) we->Suc();
    }
    fprintf(lfp, "\n");
    fclose(lfp);
#endif

	RemoveWaitingComm();

	// If already present in g_ConnectionWaitList -
	// do not add the second time.
	CommWaitElm *p = FindInConnectionWaitList();

	if (p)
	{
		// Found. Change status if it's not too late.
		OP_ASSERT(p->comm == this);
		if (p->status != COMM_WAIT_STATUS_IS_DELETED)
			p->status = status;
	}
	else
	{
		// Not found. Create new CommWaitElm.
		OP_ASSERT(g_CommWaitElm_factory);
		p = g_CommWaitElm_factory->Allocate();

		if (p)
		{
			p->Init(this, status);
			p->Into(g_ConnectionWaitList);
			if(g_comm_cleaner)
				g_comm_cleaner->SignalWaitElementActivity();
		}
	}
}

//***************************************************************************

void Comm::RemoveWaitingComm()
{
	if(!g_ConnectionWaitList)
		return;
#ifdef DEBUG_COMM_WAIT
    FILE* lfp = fopen("c:\\klient\\host.txt", "a");
    fprintf(lfp, "  +++ Wait list: ");
    CommWaitElm *we = (CommWaitElm*) g_ConnectionWaitList->First();
    while (we)
    {
        //fprintf(lfp, ", %d (%s)\n", we->comm->Id(),  (we->comm && we->comm->host ? we->comm->m_Host->Name() : ""));
        we = (CommWaitElm*) we->Suc();
    }
    fprintf(lfp, "\n");
    fclose(lfp);
#endif

    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    CommWaitElm *next_lwe;
    while (lwe)
    {
        next_lwe = (CommWaitElm*) lwe->Suc();
        if (lwe->comm == this && lwe->status != COMM_WAIT_STATUS_DELETE)
        {
#ifdef DEBUG_COMM_WAIT
            FILE* fp = fopen("c:\\klient\\host.txt", "a");
            fprintf(fp, "Remove wait: id=%d\n", Id());
            fclose(fp);
#endif
            if (comm_list_call_count > 0)
            {
                lwe->status = COMM_WAIT_STATUS_IS_DELETED;
                lwe->comm = 0;
            }
            else
            {
                lwe->Out();
                OP_DELETE(lwe);
            }
        }
        lwe = next_lwe;
    }
}

//***************************************************************************

int Comm::CountWaitingComm(BYTE flag)
{
	if(!g_ConnectionWaitList)
		return 0;
    int count = 0;
    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    while (lwe)
    {
        if (lwe->status & flag)
            count++;
        lwe = (CommWaitElm*) lwe->Suc();
    }
    return count;
}

CommWaitElm* Comm::FindInConnectionWaitList() const
{
	OP_ASSERT(g_ConnectionWaitList);
	CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
	while (lwe)
	{
		if (lwe->comm == this)
			// Found.
			return lwe;
		lwe = (CommWaitElm*) lwe->Suc();
	}

	// Not found.
	OP_ASSERT(lwe == 0);
	return NULL;
}

BOOL Comm::IsDeleted() const
{
	const CommWaitElm *lwe = FindInConnectionWaitList();
	if (!lwe)
		return FALSE;

	OP_ASSERT(lwe->comm == this);

	return (lwe->status == COMM_WAIT_STATUS_DELETE ||
	        lwe->status == COMM_WAIT_STATUS_IS_DELETED);
}

void Comm::MarkAsDeleted()
{
	AddWaitingComm(COMM_WAIT_STATUS_DELETE);
}

//***************************************************************************

void Comm::RemoveDeletedComm()
{
	/*
	 * If Comm::SafeToDelete() on *this returns true, *this might be
	 * deleted in CommRemoveDeletedComm().  Make sure this never happens,
	 * by increasing the call_count temporarely.
	 */
	NormalCallCount blocker(this);
	CommRemoveDeletedComm();
}

void Comm::CommRemoveDeletedComm()
{
    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    CommWaitElm *next_lwe;
    while (lwe)
    {
#ifdef DEBUG_MH
        FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
        fprintf(fp, "[%d] RemoveDeletedComm check obj: %p (status=%d)\n", comm_list_call_count, lwe->comm, lwe->status);
        fclose(fp);
#endif
        next_lwe = (CommWaitElm*) lwe->Suc();

		if (lwe->status == COMM_WAIT_STATUS_DELETE && lwe->comm && lwe->comm->SafeToDelete())
		{
			lwe->status = COMM_WAIT_STATUS_IS_DELETED;
			Comm *tmp_comm = lwe->comm;
			lwe->comm = 0;
			comm_list_call_count++;
			OP_DELETE(tmp_comm); // delete may cause new calls to HostAddrCache functions
			comm_list_call_count--;
		}

		if (comm_list_call_count == 0 && lwe->status == COMM_WAIT_STATUS_IS_DELETED)
		{
			lwe->Out();
			OP_DELETE(lwe);
		}

        lwe = next_lwe;
    }
	if(!g_ConnectionWaitList->Empty() && g_comm_cleaner)
		g_comm_cleaner->SignalWaitElementActivity();

	SComm::SCommRemoveDeletedComm();
}

int Comm::TryLoadBlkWaitingComm()
{
	if(!g_ConnectionWaitList)
		return 0;

#ifdef DEBUG_COMM_WAIT
    FILE* lfp = fopen("c:\\klient\\host.txt", "a");
    fprintf(lfp, "  +++ Wait list: ");
    CommWaitElm *we = (CommWaitElm*) g_ConnectionWaitList->First();
    while (we)
    {
        fprintf(lfp, ", %d (%s)", we->comm->Id(),  we->comm->m_Host->Name());
        we = (CommWaitElm*) we->Suc();
    }
    fprintf(lfp, "\n");
    fclose(lfp);
#endif

#ifdef DEBUG_COMM_WAIT
    FILE* fp = fopen("c:\\klient\\host.txt", "a");
    fprintf(fp, "TryLoadBlkWaitingComm\n");
    fclose(fp);
#endif

    // move all zero addr elements to a new list,
    // because calling Load() may put comm into list again
    Head &list = *g_ConnectionTempList;
    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    while (lwe)
    {
        CommWaitElm *next_lwe = (CommWaitElm*) lwe->Suc();
		BYTE stat =  lwe->status;

        if (stat == COMM_WAIT_STATUS_LOAD)
        {
            lwe->Out();
            lwe->Into(&list);
        }
        lwe = next_lwe;
    }


	int max_conn = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);
#ifndef NO_FTP_SUPPORT
    if (g_url_comm_count >= max_conn)
    {
		ftp_Manager->RemoveIdleConnection(TRUE);
    }
#endif // NO_FTP_SUPPORT

    if (g_url_comm_count >= max_conn)
    {
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt", " Comm::TryLoadBlkWaitingComm (removing idle HTTP) : \n");
#endif

		http_Manager->RemoveIdleConnection(TRUE);
    }

    if (g_url_comm_count >= max_conn)
	{
		g_ConnectionWaitList->Append(&list);
		return 0;
	}

    lwe = (CommWaitElm*) list.First();
    while (lwe)
    {
        Comm* comm = lwe->comm;
        lwe->Out();
        BYTE stat = lwe->status;
        OP_DELETE(lwe);

        if (stat == COMM_WAIT_STATUS_LOAD)
        {
#ifdef DEBUG_MH
            FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
            fprintf(fp, "TryLoadBlkWaitingComm obj: %p\n", comm);
            fclose(fp);
#endif
            comm_list_call_count++;
#ifdef DEBUG_COMM_WAIT
            int load_stat =
#endif
				comm->Load_Stage2();
            comm_list_call_count--;
#ifdef DEBUG_COMM_WAIT
            {
				FILE* fp = fopen("c:\\klient\\host.txt", "a");
				fprintf(fp, "Try Load: id=%d, COMM_WAIT_STATUS_LOAD, load_stat=%d\n", comm->Id(), load_stat);
				fclose(fp);
            }
#endif
        }

        lwe = (CommWaitElm*) list.First();
    }
	if(!g_ConnectionWaitList->Empty() && g_comm_cleaner)
		g_comm_cleaner->SignalWaitElementActivity();
    return 0;
}


int Comm::TryLoadWaitingComm(ServerName *_host)
{
	if(!g_ConnectionWaitList)
		return 0;
#ifdef DEBUG_COMM_WAIT
	FILE* lfp = fopen("c:\\klient\\host.txt", "a");
	fprintf(lfp, "  +++ Wait list: ");
	CommWaitElm *we = (CommWaitElm*) g_ConnectionWaitList->First();
	while (we)
	{
		//fprintf(lfp, "  , %d (%s)\n", we->comm->Id(), we->comm->m_Host->Name());
		we = (CommWaitElm*) we->Suc();
	}
	fprintf(lfp, "\n");
	fclose(lfp);
#endif

#ifdef DEBUG_COMM_WAIT
	if(_host)
	{
	FILE* fp = fopen("c:\\klient\\host.txt", "a");
	fprintf(fp, "Try: %s\n", m_Host->Name());
	fclose(fp);
	}
#endif
#ifdef DEBUG_COMM_WAIT
	PrintfTofile("http1.txt","Trying to Load waiting [%lu]\n", prefsManager->CurrentTime());
#endif

    CommWaitElm *lwe = (CommWaitElm*) g_ConnectionWaitList->First();
    CommWaitElm *next_lwe = 0;
	CommWaitElm *longest = NULL;
	CommWaitElm *server_sel = NULL;
    while (lwe)
    {
		next_lwe = (CommWaitElm*)lwe->Suc();

		if (lwe->status == COMM_WAIT_STATUS_DELETE)
		{
			if (lwe->comm && lwe->comm->SafeToDelete())
			{
				lwe->status = COMM_WAIT_STATUS_IS_DELETED;
				Comm *tmp_comm = lwe->comm;
				lwe->comm = 0;
				comm_list_call_count++;
				OP_DELETE(tmp_comm); // delete may cause blocking winsock calls ???
				comm_list_call_count--;

				if(longest && longest->comm == NULL)
				{
					longest = NULL;
					lwe = (CommWaitElm*) g_ConnectionWaitList->First();
					continue;
				}
			}
		}
		else if (lwe->status == COMM_WAIT_STATUS_IS_DELETED)
		{
			if (comm_list_call_count == 0)
			{
				lwe->Out();
				OP_DELETE(lwe);
				OP_ASSERT(!longest || longest->comm);
			}
		}
		else if(lwe->comm &&
			(lwe->comm->m_Host == NULL || lwe->comm->m_Host->GetConnectCount() < (unsigned int) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer)) &&
			g_url_comm_count < g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal)
			)
		{
			if(!longest || longest->time_set > lwe->time_set)
				longest = lwe;

			OP_ASSERT(longest && longest->comm);
		}

		lwe = next_lwe;
	}
	OP_ASSERT(!longest || longest->comm);
	if(!g_ConnectionWaitList->Empty() && g_comm_cleaner)
		g_comm_cleaner->SignalWaitElementActivity();
	if (longest || server_sel)
	{
		lwe = server_sel;
		if(longest && (!lwe || lwe->time_set > longest->time_set))
			lwe = longest;

		OP_ASSERT(lwe->comm);

		Comm* comm = lwe->comm;
		lwe->comm = 0;
		lwe->status = COMM_WAIT_STATUS_IS_DELETED;
		if (comm_list_call_count == 0)
		{
			lwe->Out();
			OP_DELETE(lwe);
		}

		OP_ASSERT(comm);
#ifdef DEBUG_COMM_WAIT
		PrintfTofile("http1.txt","Load waiting cnnection %d [%lu]\n", comm->Id(), prefsManager->CurrentTime());
#endif
#ifdef DEBUG_MH
		FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
		fprintf(fp, "TryLoadWaitingComm %d obj: (%p)\n", comm->Id(), comm);
		fclose(fp);
#endif

		OP_ASSERT(comm);

		comm_list_call_count++;
		int load_stat = comm->Load_Stage2();
		comm_list_call_count--;

#ifdef DEBUG_COMM_WAIT
		{
			FILE* fp = fopen("c:\\klient\\host.txt", "a");
			fprintf(fp, "Try load: id=%d, \"%s\", stat=%d\n", comm->Id(), comm->host->Name(), load_stat);
			fclose(fp);
		}
#endif
		return load_stat;
	}
	return -1;
}

void Comm::CleanWaitingComm_List()
{
#ifdef DEBUG_COMM_WAIT
	FILE* lfp = fopen("c:\\klient\\host.txt", "a");
	fprintf(lfp, "\n  Clean Wait list: ");
	CommWaitElm *we = (CommWaitElm*) g_ConnectionWaitList->First();
	while (we)
	{
		fprintf(lfp, ", %d", we->comm->Id());
		we = (CommWaitElm*) we->Suc();
	}
	fprintf(lfp, "\n");
	fclose(lfp);
#endif

	CommRemoveDeletedComm();
}

//***************************************************************************

void Comm::SendData(char *buf, unsigned blen)
{
	if(!buf)
		return;

	if(!blen)
	{
		OP_DELETEA(buf);
		return;
	}

	NormalCallCount blocker(this);

#ifdef _EMBEDDED_BYTEMOBILE
	Comm_strings *last = (Comm_strings *) buffers.Last();
			
	if (last && (last->len + blen) < last->total_len)
	{
		op_memcpy(last->string + last->len, buf, blen);
		last->len += blen;
		OP_DELETEA(buf);
	}
	else
#endif // _EMBEDDED_BYTEMOBILE
	{
		Comm_strings *item = OP_NEW(Comm_strings, (buf, blen));
		if(item == NULL)
			goto oom_cleanup;

#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%d] Comm::SendData():  %p\n", Id(), item->string);
		PrintfTofile("winsck2.txt","[%d] Comm::SendData():  %p\n", Id(), item->string);
#endif

		if (buffers.Empty() && current_buffer == NULL)
			current_buffer = item;
		else
		{
#ifdef _EMBEDDED_BYTEMOBILE
			if (blen<1460)
			{
				item->string = OP_NEWA(char, 35000);
				if (!item->string)
				{
					OP_DELETE(item);
					goto oom_cleanup;
				}
				op_memcpy(item->string, buf, blen);
				item->total_len = 35000;
				OP_DELETEA(buf);
			}
#endif // _EMBEDDED_BYTEMOBILE

			item->Into(&buffers);
		}

	}

	if (!info.sending_in_progress)
	{
		CommState send_stat = SendDataToConnection();

		if (send_stat == COMM_REQUEST_FAILED)
			Stop();
	}
	return;

oom_cleanup:
	OP_DELETEA(buf);
	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
	Stop();
}

//***************************************************************************

void Comm::ClearBuffer()
{
	if (current_buffer != NULL)
	{
		OP_DELETE(current_buffer);
		current_buffer = NULL;
	}

	buffers.Clear();
}

//***************************************************************************

void Comm::Cleanup()
{
	m_connection_established_timer.Stop();
	m_happy_eyeballs_timer.Stop();

	g_main_message_handler->UnsetCallBacks(this);
	g_main_message_handler->RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, Id(), 0);
}

//***************************************************************************

BOOL Comm::MoreRequest()
{
	if (current_buffer)
	{
		return TRUE;
	}

	if (buffers.Empty())
		RequestMoreData();

	if (current_buffer == NULL && !buffers.Empty())
	{
		current_buffer = (Comm_strings *) buffers.First();
		current_buffer->Out();
	}

	if(m_AllDoneMsgMode != NO_STATE && current_buffer==NULL)
	{
		SetProgressInformation(m_AllDoneMsgMode,0, m_Host->UniName());
		SetRequestMsgMode(m_AllDone_requestMsgMode);
		m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;
	}

	return (current_buffer != NULL);
}

//***************************************************************************

#ifdef NETWORK_STATISTICS_SUPPORT
Network_Statistics_Manager::Network_Statistics_Manager()
{
	Reset();
}

void Network_Statistics_Manager::Reset()
{
	dns_delays = 0;
	dns_delay_count = 0;
	total_sent = 0;
	total_received = 0;
	max_transfer_speed_total = 0;
	freeze = FALSE;
	op_memset( &log_points, 0, sizeof(log_points) );
	op_memset( &max_transfer_speed_last_ten_minutes, 0, sizeof(max_transfer_speed_last_ten_minutes) );
}

void Network_Statistics_Manager::Freeze()
{
	freeze = TRUE;
}
void Network_Statistics_Manager::addBytesTransferred(int amount)
{
	time_t current_time = g_timecache->CurrentTime();
	if (log_points[0].last_activity != current_time)
	{
		int i, temp = 0;

		for (i = 0; i < NUM_LOG_POINTS && ((current_time - log_points[i].last_activity) <= NUM_LOG_POINTS); i++)
			temp += log_points[i].transfered;
		
		if ((current_time - max_transfer_speed_last_ten_minutes[0].last_activity)>60)
		{
			for (i = 9; i; i--)
			{
				max_transfer_speed_last_ten_minutes[i].transfered = max_transfer_speed_last_ten_minutes[i-1].transfered;
				max_transfer_speed_last_ten_minutes[i].last_activity = max_transfer_speed_last_ten_minutes[i-1].last_activity;
			}
			max_transfer_speed_last_ten_minutes[0].last_activity = current_time;
			max_transfer_speed_last_ten_minutes[0].transfered = temp;
		
		}
		else if (temp > max_transfer_speed_last_ten_minutes[0].transfered)
		{
			max_transfer_speed_last_ten_minutes[0].transfered = temp;
		}

		if (temp > max_transfer_speed_total)
			max_transfer_speed_total = temp;

		for (i = NUM_LOG_POINTS - 1; i; i--)
		{
			log_points[i].transfered = log_points[i-1].transfered;
			log_points[i].last_activity = log_points[i-1].last_activity;
		}
		log_points[0].last_activity = current_time;
		log_points[0].transfered = amount;
	}
	else
		log_points[0].transfered += amount;
}

void Network_Statistics_Manager::addBytesSent(int amount)
{
	if (freeze)
		return;

	total_sent += amount;
	addBytesTransferred(amount);
}

void Network_Statistics_Manager::addBytesReceived(int amount)
{
	if (freeze)
		return;

	total_received += amount;
	addBytesTransferred(amount);
}

UINT64 Network_Statistics_Manager::getBytesTransferred()
{
	return (total_sent + total_received);
}

UINT64 Network_Statistics_Manager::getBytesSent()
{
	return total_sent;
}

UINT64 Network_Statistics_Manager::getBytesReceived()
{
	return total_received;
}

int Network_Statistics_Manager::getTransferSpeedMaxTotal()
{
	return max_transfer_speed_total / NUM_LOG_POINTS;
}

int Network_Statistics_Manager::getMaxTransferSpeedLastTenMinutes()
{
	time_t current_time = g_timecache->CurrentTime();
	int i, temp = 0;

	for (i = 0; i<10 && ((current_time - max_transfer_speed_last_ten_minutes[i].last_activity) <= 600); i++)
	{
		if (max_transfer_speed_last_ten_minutes[i].transfered > temp)
			temp = max_transfer_speed_last_ten_minutes[i].transfered;
	}
	return temp / NUM_LOG_POINTS;
}

int Network_Statistics_Manager::getBytesTransferredLastTenMinutes()
{
	time_t current_time = g_timecache->CurrentTime();
	int i, res = 0;

	for (i = 0; i<10 && ((current_time - max_transfer_speed_last_ten_minutes[i].last_activity) <= 600); i++)
		res += max_transfer_speed_last_ten_minutes[i].transfered;

	return res;
}

#ifdef OPERA_PERFORMANCE
void Network_Statistics_Manager::WriteReport(URL &url)
{
	url.WriteDocumentDataUniSprintf(UNI_L("<h2>TCP Activity</h2><table><tr><td>Second</td><td>Bytes</td></tr>"));
	int i;
	int max = 0;
	for (i = NUM_LOG_POINTS - 1; i >= 0; i--)
	{
		if (log_points[i].transfered)
		{
			max = i;
			break;
		}
	}
	int seconds = 0;
	int total_transfered = 0;
	time_t next_activity = (int) log_points[max].last_activity;

	for (i = max; i > 0; i--)
	{
		url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%d</td><td>%d</td></tr>"), seconds, log_points[i].transfered);
		total_transfered += log_points[i].transfered;
		seconds++;
		next_activity++;
		while (next_activity != log_points[i-1].last_activity)
		{
			url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%d</td><td>0</td></tr>"), seconds);
			seconds++;
			next_activity++;
		}
	}

	url.WriteDocumentDataUniSprintf(UNI_L("</table><pre>\nTotal transferred : %d bytes\nDNS prefetching saved %.0f milliseconds across %d lookups\n</pre>"), total_transfered, dns_delays, dns_delay_count);
}
#endif // OPERA_PERFORMANCE
#endif // NETWORK_STATISTICS_SUPPORT

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void Comm::PauseLoading()
{
	if (m_Socket == NULL)
		return; // COMM_REQUEST_FAILED;

#if defined(GTK_DOWNLOAD_EXTENSION)
	// gtk team should move to generic implementation at some point

	GdkSocket* gdk_socket = static_cast<GdkSocket*>( m_Socket );

	gdk_socket->Pause();
#else

	m_Socket->PauseRecv();

#endif
}

OP_STATUS Comm::ContinueLoading()
{
	if (m_Socket == NULL)
		return OpStatus::ERR; // COMM_REQUEST_FAILED;

#if defined(GTK_DOWNLOAD_EXTENSION)
	// gtk team should move to generic implementation at some point

	GdkSocket* gdk_socket = static_cast<GdkSocket*>( m_Socket );

	gdk_socket->Continue();
	return OpStatus::OK;
#else

	return m_Socket->ContinueRecv();

#endif
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef OPSOCKET_GETLOCALSOCKETADDR
void Comm::CloseIfInterfaceDown(OpSocketAddress* local_interface_adress, OpSocketAddress* server_address)
{
	if (!local_interface_adress && !server_address)
		return;

	if (server_address && !server_address->IsHostEqual(m_SocketAddress))
		return;

	if (local_interface_adress && !local_interface_adress->IsHostEqual(m_local_socket_address))
		return;

	if (!info.closed && !info.pending_close)
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_NETWORK_PROBLEM));
		NormalCallCount blocker(this);

		if (m_Socket)
			m_Socket->Close();

		info.connected = FALSE;

		info.pending_close = TRUE;

		EndLoading();
		Stop();
		Clear();
	}

	info.do_not_reconnect = TRUE;

	m_Host->ClearSocketAddresses();
}
#endif // OPSOCKET_GETLOCALSOCKETADDR

#ifdef _EMBEDDED_BYTEMOBILE
OP_STATUS Comm::SetZLibTransceive() {
	OP_STATUS status = OpStatus::OK;
	if (!info.zlib_transceive)
	{
		status = ZlibTransceiveSocket::Create(&m_Socket, m_Socket, this, 5000);
		if (OpStatus::IsSuccess(status))
			info.zlib_transceive=TRUE;
		else
			RAISE_OOM_CONDITION(status);
	}
	return status;
}
#endif // _EMBEDDED_BYTEMOBILE


void Comm::SetAllSentMsgMode(ProgressState parm, ProgressState request_parm)
{
	m_AllDoneMsgMode = parm;
	m_AllDone_requestMsgMode = (m_AllDoneMsgMode != NO_STATE ? request_parm : NO_STATE);
	if(m_AllDoneMsgMode != NO_STATE && buffers.Empty() && !current_buffer)
	{
		SetProgressInformation(m_AllDoneMsgMode,0, m_Host->UniName());
		SetRequestMsgMode(m_AllDone_requestMsgMode);
		m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;
	}
}
