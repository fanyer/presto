/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2004 - 2011
 *
 * Web server implementation -- overall server logic
 */

#include "core/pch.h"
#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/util/opfile/opfolder.h"
#include "modules/webserver/src/resources/webserver_file.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"

#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
#include "modules/webserver/webserver_direct_socket.h"
#endif

#ifdef GADGET_SUPPORT
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#endif

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#endif

WebServerConnectionListener::WebServerConnectionListener(UPNP_PARAM(UPnP *upnp))
	: m_backlog(0)
	, m_rootSubServer(NULL)
	, m_uploadRate(0)
	, m_currentWindowSize(0)
	, m_bytesSentThisSample(0)
	, m_prevClockCheck(0)
	, m_nowThrottling(FALSE)
    , m_kBpsUp(0)
	, m_listening_socket_local(NULL)
	, m_listeningMode(WEBSERVER_LISTEN_LOCAL)
	, m_prefered_listening_port(0)
	, m_real_listening_port(0)
	, public_port(0)
	, m_hasPostedSetupListeningsocketMessage(FALSE)
	, m_webserverEventListener(NULL)
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	, m_listening_socket_rendezvous(NULL)
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

#ifdef UPNP_SUPPORT
	, m_upnp_port_opener(NULL)
	, m_upnp(upnp)
	, m_opened_port(0)
	, m_opened_device(NULL)
	, m_port_checking_enabled(FALSE)
#endif
	, upnp_advertised(FALSE)
{
#if defined(UPNP_SUPPORT) && defined(UPNP_SERVICE_DISCOVERY)
	ref_obj_lsn.EnableDelete(FALSE);
	advertisement_enabled=FALSE;
#endif // defined(UPNP_SUPPORT) && defined(UPNP_SERVICE_DISCOVERY)
}


/* static */ OP_STATUS
WebServerConnectionListener::Make(WebServerConnectionListener *&connection_listener,
								  WebserverListeningMode listeningMode,
								  WebServer *eventListener,
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
								  const char *shared_secret,
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
								  unsigned int port,
								  unsigned int backlog,
								  int uploadRate
								  UPNP_PARAM_COMMA(UPnP *upnp),
								  const char *upnpDeviceType,
								  const uni_char *upnpDeviceIcon,
								  const uni_char *upnpPayload
								  )
{
	OP_ASSERT(eventListener);
	connection_listener = NULL;
	WebServerConnectionListener *temp_connection_listener =  OP_NEW(WebServerConnectionListener, (UPNP_PARAM(upnp)));
	if (temp_connection_listener == 0 )
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(temp_connection_listener->m_upnpSearchType.Set(upnpDeviceType));
	OpAutoPtr<WebServerConnectionListener> connection_listener_anchor(temp_connection_listener);

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	RETURN_IF_ERROR(temp_connection_listener->m_shared_secret.Set(shared_secret));
	temp_connection_listener->m_shared_secret.Strip();

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(temp_connection_listener, MSG_WEBSERVER_DELETE_RENDESVOUZ_CONNECTION, 0));
#endif

	temp_connection_listener->m_listeningMode = listeningMode;
	temp_connection_listener->m_webserverEventListener = eventListener;

	temp_connection_listener->m_real_listening_port = port;
	temp_connection_listener->m_prefered_listening_port = port;
	temp_connection_listener->m_backlog = backlog;
	temp_connection_listener->m_uploadRate = uploadRate;

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(temp_connection_listener, MSG_WEBSERVER_DELETE_CONNECTION, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(temp_connection_listener, MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0));

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
	RETURN_IF_ERROR(temp_connection_listener->InstallNativeRootService(eventListener));
#endif //NATIVE_ROOT_SERVICE_SUPPORT

	RETURN_IF_LEAVE(g_pcwebserver->RegisterListenerL(temp_connection_listener));

	g_main_message_handler->PostMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0);

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	// Initialize the device description

	RETURN_IF_ERROR(temp_connection_listener->temp_service.Construct());
	RETURN_IF_ERROR(temp_connection_listener->UPnPServicesProvider::Construct());

	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(DEVICE_TYPE, upnpDeviceType ? upnpDeviceType : UPNP_DISCOVERY_OPERA_SEARCH_TYPE));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MANUFACTURER, "Opera Software ASA"));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MANUFACTURER_URL, "http://www.opera.com/"));

	OpString str_temp;

	RETURN_IF_ERROR(str_temp.AppendFormat(UNI_L("Opera Unite Version %s"), VER_NUM_STR_UNI));

// In debug, also add the build number
/*#if defined(_DEBUG)
	RETURN_IF_ERROR(str_temp.AppendFormat(UNI_L(" - build %s"), VER_BUILD_IDENTIFIER_UNI));
#endif*/

	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MODEL_DESCRIPTION, str_temp.CStr()));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MODEL_NAME, "Opera Unite"));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MODEL_NUMBER, VER_NUM_STR_UNI));

	if(upnpDeviceIcon)
		RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(DEVICE_ICON_RESOURCE, upnpDeviceIcon));

	if(upnpPayload)
		RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(PAYLOAD, upnpPayload));

	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(MODEL_URL, "http://www.opera.com/"));

	//OpString uuid;
	OpString user;
	OpString dev;
	OpString proxy;
	OpString friendly;
	
	// Generate an unique UUID; to better support IP change and takeover, the Unique ID basically is the Unite address of the user,
	//int rnd=op_rand();
	RETURN_IF_ERROR(user.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
	RETURN_IF_ERROR(dev.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
	RETURN_IF_ERROR(proxy.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost)));

	//RETURN_IF_ERROR(uuid.AppendFormat(UNI_L("uuid:opera-unite-1-%d-%s-%s"), rnd, user.CStr(), dev.CStr()));
	//RETURN_IF_ERROR(uuid.AppendFormat(UNI_L("uuid:opera-unite-1-%s-%s-%s"), dev.CStr(), user.CStr(), proxy.CStr()));
	RETURN_IF_ERROR(friendly.AppendFormat(UNI_L("Opera Unite - %s @ %s"), user.CStr(), dev.CStr()));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(FRIENDLY_NAME, friendly.CStr()));

	OpString8 tmp_uuid;

	RETURN_IF_ERROR(tmp_uuid.AppendFormat("uuid:%s", (eventListener)?eventListener->GetUUID():NULL));
	RETURN_IF_ERROR(temp_connection_listener->SetAttributeValue(UDN, tmp_uuid.CStr()));

	//RETURN_IF_ERROR(temp_connection_listener->upnp->AddServicesProvider(ptr));
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

	connection_listener = connection_listener_anchor.release();

	return OpStatus::OK;
}

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
OP_STATUS WebServerConnectionListener::RetrieveService(int index, UPnPService *&service)
{
	WebSubServer *wss=m_webserverEventListener->GetSubServerAtIndex(index);

	if(!wss)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	temp_service.ResetAttributesValue();
	const uni_char *virtual_path=wss->GetSubServerVirtualPath().CStr();
	OpString8 temp_type;
	OpString8 temp_id;
	OpString path_url;
	OpString path_scpd;
	OpString8 service_name;

	if(!virtual_path || virtual_path[0]=='_' || !wss->IsVisibleUPNP())
		temp_service.SetVisible(FALSE);
	else
		temp_service.SetVisible(TRUE);

	path_url.AppendFormat(UNI_L("/%s"), virtual_path);
	path_scpd.AppendFormat(UNI_L("/_upnp/scpd"), virtual_path);

	RETURN_IF_ERROR(service_name.Set(wss->GetSubServerName().CStr()));

	//RETURN_IF_ERROR(temp_type.AppendFormat("urn:opera-com:service:%s%d:1", "OperaUniteService", index+1));
	RETURN_IF_ERROR(temp_type.AppendFormat("urn:opera-com:service:%s:1", service_name.CStr()));
	//RETURN_IF_ERROR(temp_id.AppendFormat("urn:opera-com:serviceId:OperaUniteService%d", index+1));
	RETURN_IF_ERROR(temp_id.AppendFormat("urn:opera-com:serviceId:%s-%d", service_name.CStr(), index+1));

	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::SERVICE_TYPE, temp_type.CStr()));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::SERVICE_ID, temp_id.CStr()));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::SCPD_URL, path_scpd.CStr()));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::CONTROL_URL, path_url.CStr()));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::EVENT_URL, "/event"));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::NAME, wss->GetSubServerName().CStr()));
	RETURN_IF_ERROR(temp_service.SetAttributeValue(UPnPService::DESCRIPTION, wss->GetSubServerDescription().CStr()));

	service=&temp_service;

	return OpStatus::OK;
}

UINT32 WebServerConnectionListener::GetNumberOfServices()
{
	return m_webserverEventListener->GetSubServerCount();
}

OP_STATUS WebServerConnectionListener::UpdateValues(OpString8 &challenge)
{
	OpStringC user = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser);
	OpStringC dev = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice);

	RETURN_IF_ERROR(SetAttributeValue(UNITE_USER, user.CStr()));
	RETURN_IF_ERROR(SetAttributeValue(UNITE_DEVICE, dev.CStr()));

	if(challenge.Length()>0)
	{
		OpString8 expected;
		OpString8 pk;

		// First authentication: "Opera"
		RETURN_IF_ERROR(pk.Set(UPNP_OPERA_PRIVATE_KEY));
		RETURN_IF_ERROR(UPnP::Encrypt(challenge, pk, expected)); // MD5(PK+nonce1)
		RETURN_IF_ERROR(SetAttributeValue(CHALLENGE_RESPONSE, expected.CStr()));
		
		// Second authentication: shared secret
		OpString8 pk2;
		OpString8 nonce2;
		OpString8 expected2;
		
		RETURN_IF_ERROR(pk2.AppendFormat("%s%s", m_shared_secret.CStr(), challenge.CStr()));
		RETURN_IF_ERROR(nonce2.AppendFormat("%u", (UINT32)op_rand()));
		RETURN_IF_ERROR(UPnP::Encrypt(nonce2, pk2, expected2)); // MD5(SharedSecret+nonce1+nonce2)
		RETURN_IF_ERROR(SetAttributeValue(CHALLENGE_RESPONSE_AUTH, expected2.CStr()));
		RETURN_IF_ERROR(SetAttributeValue(NONCE_AUTH, nonce2.CStr()));
	}
	else
		RETURN_IF_ERROR(SetAttributeValue(CHALLENGE_RESPONSE, ""));


	return OpStatus::OK;
}

OP_STATUS WebServerConnectionListener::UpdateDescriptionURL(OpSocketAddress *addr)
{
	// Updte the description URL
	OpString addr_str;
	OpString desc_url;

	OP_ASSERT(addr);

	if(addr)
		RETURN_IF_ERROR(addr->ToString(&addr_str));

	RETURN_IF_ERROR(desc_url.AppendFormat(UNI_L("http://%s:%d"), addr_str.CStr(), m_real_listening_port));
	RETURN_IF_ERROR(SetAttributeValue(PRESENTATION_URL, desc_url.CStr()));

	RETURN_IF_ERROR(desc_url.AppendFormat(UNI_L("/_upnp/desc")));

	return SetDescriptionURL(desc_url);
}
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
OP_STATUS WebServerConnectionListener::ReconnectProxy(const char *shared_secret)
{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	RETURN_IF_ERROR(m_shared_secret.Set(shared_secret));
	m_shared_secret.Strip();
#endif

	OP_DELETE(m_listening_socket_rendezvous);
	m_listening_socket_rendezvous = NULL;

	m_listeningMode |= WEBSERVER_LISTEN_PROXY;

	if (m_hasPostedSetupListeningsocketMessage == FALSE)
	{
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0, 5000);
		m_hasPostedSetupListeningsocketMessage = TRUE;
	}

	return OpStatus::OK;
}


void WebServerConnectionListener::StopListeningProxy()
{
	if (m_listening_socket_rendezvous && m_listening_socket_rendezvous->IsConnected())
		m_webserverEventListener->OnProxyDisconnected(PROXY_CONNECTION_LOGGED_OUT_BY_CALLER, FALSE);

	OP_DELETE(m_listening_socket_rendezvous);
	m_listening_socket_rendezvous = NULL;

	m_listeningMode &= ~((unsigned int)WEBSERVER_LISTEN_PROXY);
}
#endif //WEBSERVER_RENDEZVOUS_SUPPORT

OP_STATUS WebServerConnectionListener::ReconnectLocalImmediately()
{
	OP_STATUS status;

	m_hasPostedSetupListeningsocketMessage = FALSE;

	if (OpStatus::IsError(status = SetUpListenerSocket()))
	{
		if (OpStatus::IsMemoryError(status))
		{
			g_webserverPrivateGlobalData->SignalOOM();
		}
	}

	return status;
}

OP_STATUS WebServerConnectionListener::ReconnectLocal(unsigned int port, BOOL reconnect_immediately)
{
	OP_DELETE(m_listening_socket_local);
	m_listening_socket_local = NULL;
	if (port != 0)
	{
		m_real_listening_port = port;
		m_prefered_listening_port = port;
	}

	m_listeningMode |= WEBSERVER_LISTEN_LOCAL;

	if(reconnect_immediately)
		return ReconnectLocalImmediately();
	else
	{
		if (m_hasPostedSetupListeningsocketMessage == FALSE)
		{
			g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0, 5000);
			m_hasPostedSetupListeningsocketMessage = TRUE;
		}
	}

	return OpStatus::OK;
}

void WebServerConnectionListener::StopListeningLocal()
{
	BOOL was_listening = m_listening_socket_local ? TRUE : FALSE;
	
	OP_DELETE(m_listening_socket_local);
	m_listening_socket_local = NULL;
	m_listeningMode &= ~((unsigned int)WEBSERVER_LISTEN_LOCAL);

	m_listeningMode &= ~((unsigned int)WEBSERVER_LISTEN_OPEN_PORT);
	
	if (was_listening)
		m_webserverEventListener->OnWebserverListenLocalStopped();

	/*FIXME: Must close port here */
}

void WebServerConnectionListener::StartListeningDirectMemorySocket()
{
	m_listeningMode |= WEBSERVER_LISTEN_DIRECT;
}

void WebServerConnectionListener::StopListeningDirectMemorySocket()
{
	m_listeningMode &= ~((unsigned int)WEBSERVER_LISTEN_DIRECT);
}

WebserverListeningMode WebServerConnectionListener::GetCurrentMode()
{
	WebserverListeningMode mode = WEBSERVER_LISTEN_NONE;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (m_listening_socket_rendezvous && m_listening_socket_rendezvous->IsConnected())
	{
		mode |= WEBSERVER_LISTEN_PROXY;
	}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	if (m_listening_socket_local)
	{
		mode |= WEBSERVER_LISTEN_LOCAL;
	}

	if (m_listeningMode & WEBSERVER_LISTEN_DIRECT)
	{
		mode |= WEBSERVER_LISTEN_DIRECT;
	}

	return mode;
}

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
void WebServerConnectionListener::OnSocketDirectConnectionRequest(WebserverDirectClientSocket* listening_socket, BOOL is_owner)
{
	if (!(m_listeningMode & WEBSERVER_LISTEN_DIRECT))
	{
		listening_socket->OnSocketConnectError(OpSocket::CONNECTION_FAILED);
		return;
	}

	WebserverDirectServerSocket *server_socket = NULL;
	WebServerConnection *http_connection = OP_NEW(WebServerConnection, ());

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (
			http_connection == NULL ||
			OpStatus::IsError(status = WebserverDirectServerSocket::CreateDirectServerSocket(&server_socket)) ||
			OpStatus::IsError(status = listening_socket->Accept(server_socket)) ||
			OpStatus::IsError(status = http_connection->Construct(server_socket, this, is_owner, TRUE
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
			                                                      , FALSE, "127.0.0.1")) /* Takes over server_socket */
#endif
		)
	{
		OP_DELETE(http_connection);
		OP_DELETE(server_socket);
		if (OpStatus::IsMemoryError(status))
			g_webserverPrivateGlobalData->SignalOOM();

		return;
	}
	/*We must put the new WebServerConnection into a list*/
	http_connection->Into(&m_connectionList);
	server_socket->SetListener(http_connection);

	return;
}
#endif

void WebServerConnectionListener::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (OpPrefsCollection::Webserver == id &&
	    PrefsCollectionWebserver::WebserverUploadRate == pref)
	{
		m_uploadRate = newvalue;
	}
}

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
OP_STATUS WebServerConnectionListener::EnableUPnPServicesDiscovery()
{
	OP_ASSERT(m_upnp);

	RETURN_IF_ERROR(UPnP::CreateUniteDiscoveryObject(m_upnp, this));

	//UPnP::SetDOMUPnpObject(m_upnp);

	return OpStatus::OK;
}
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

OP_STATUS WebServerConnectionListener::SetUpListenerSocket()
{
	OP_STATUS status = OpStatus::OK;
#ifdef UPNP_SUPPORT
	if (m_listeningMode & WEBSERVER_LISTEN_OPEN_PORT && m_listeningMode & WEBSERVER_LISTEN_LOCAL )
	{
		OP_ASSERT(m_upnp);

		if (!m_upnp_port_opener)
		{
			if (OpStatus::IsError(status = UPnP::CreatePortOpeningObject(m_upnp, m_upnp_port_opener, m_prefered_listening_port, m_prefered_listening_port + 40)))
			{
				//OP_DELETE(m_upnp);
				//m_upnp = NULL;
				m_upnp_port_opener = NULL; //Owned and deleted by m_upnp;

				return status;
			}

			m_upnp_port_opener->SetPolicy(UPnPPortOpening::UPNP_CARDS_OPEN_FIRST);

			if (OpStatus::IsSuccess(m_upnp_port_opener->StartPortOpening()))
			{
				OpStatus::Ignore(m_upnp_port_opener->AddListener(this));
			}
		}
	}
	else
	{
		DisableUPnPPortForwarding();
	}

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	if(m_listeningMode & WEBSERVER_LISTEN_UPNP_DISCOVERY)
		EnableUPnPServicesDiscovery();
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
#endif // UPNP_SUPPORT

	//if (!(m_listeningMode & WEBSERVER_LISTEN_OPEN_PORT))
	{
		OP_STATUS local_socket_status = OpStatus::OK;
		OP_STATUS rendezvous_status = OpStatus::OK;

		if (m_listeningMode & WEBSERVER_LISTEN_LOCAL) // Needs to wait with local listening until port opening is done.
		{
			if (m_listening_socket_local == NULL && !(m_listeningMode & WEBSERVER_LISTEN_OPEN_PORT))
				local_socket_status = StartListening();
		}
		else
		{
			OP_DELETE(m_listening_socket_local);
			m_listening_socket_local = NULL;
		}

		if (m_listeningMode & WEBSERVER_LISTEN_PROXY)
			rendezvous_status = StartListeningRendezvous();

		if (
				OpStatus::IsError(status = local_socket_status)
	#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
				|| OpStatus::IsError(status = rendezvous_status)
	#endif
			)
		{
			if (m_hasPostedSetupListeningsocketMessage == FALSE)
			{
				g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0, 4000);
				m_hasPostedSetupListeningsocketMessage = TRUE;
			}
		}
	}
	return status;
}

OP_STATUS WebServerConnectionListener::StartListening()
{
	// Advertising here is a mistake. We need to wait the end of the UPnP process

	// The first time, advertise UPnP
	/*if(!upnp_advertised)
	{
		upnp_advertised=TRUE;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		advertisement_enabled=TRUE;
#endif
		AdvertiseUPnP(TRUE);
	}*/

	OP_STATUS local_socket_status = OpStatus::OK;
	OpSocketAddress *socket_address = NULL;

	if (	m_listening_socket_local == NULL &&
			OpStatus::IsError(local_socket_status = OpSocketAddress::Create(&socket_address)) ||
			(
				!socket_address || // shut up Coverity
				((GetWebServer()->GetAlwaysListenToAllNetworks() || g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks) || OpStatus::IsSuccess(socket_address->FromString(UNI_L("127.0.0.1"))), FALSE) ||
				(socket_address->SetPort(m_real_listening_port), FALSE))
			) ||
			OpStatus::IsError(local_socket_status = SocketWrapper::CreateTCPSocket(&(m_listening_socket_local), this, SocketWrapper::NO_WRAPPERS)) ||
			(
				!m_listening_socket_local || // shut up Coverity
				OpStatus::IsError(local_socket_status = m_listening_socket_local->Listen(socket_address, m_backlog))
			)
		)
	{
		 if (!OpStatus::IsMemoryError(local_socket_status))
		 {
			 m_webserverEventListener->OnWebserverListenLocalFailed(WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN);
			 m_real_listening_port++;
		 }
		 else if(OpStatus::IsSuccess(local_socket_status))
			local_socket_status = OpStatus::ERR;

		 OP_DELETE(m_listening_socket_local);
		 m_listening_socket_local = NULL;
	}
	else if (m_listening_socket_local)
	{
#ifdef UPNP_SUPPORT
		if (m_listening_socket_rendezvous && m_port_checking_enabled)
			m_listening_socket_rendezvous->CheckPort(m_real_listening_port);
#endif
		m_webserverEventListener->OnWebserverListenLocalStarted(m_real_listening_port);
	}

	OP_DELETE(socket_address);

	OP_ASSERT((m_listening_socket_local && OpStatus::IsSuccess(local_socket_status)) ||
		(!m_listening_socket_local && OpStatus::IsError(local_socket_status)));

	RETURN_IF_ERROR(StartListeningRendezvous());

	return local_socket_status;
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
OP_STATUS WebServerConnectionListener::StartListeningRendezvous()
{
	// Advertising when the connection with the proxy is available, is a mistake
	// We really need a local listener or it will not work, above all if we advertise a transtional port
	// The first time, advertise UPnP
	/*if(!upnp_advertised)
	{
		upnp_advertised=TRUE;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		advertisement_enabled=TRUE;
#endif
		AdvertiseUPnP(TRUE);
	}*/

	OP_STATUS rendezvous_status = OpStatus::OK;
	if (m_listeningMode & WEBSERVER_LISTEN_PROXY)
	{
		if (m_listening_socket_rendezvous == NULL)
		{
			if (WebserverRendezvous::IsConfigured() == FALSE)
			{
				m_webserverEventListener->OnProxyConnectionFailed(PROXY_CONNECTION_DEVICE_NOT_CONFIGURED, FALSE);
			}
			else if
			(
				OpStatus::IsError(rendezvous_status = WebserverRendezvous::Create(&m_listening_socket_rendezvous, this, m_shared_secret.Strip().CStr(), m_real_listening_port)) ||
				OpStatus::IsError(rendezvous_status = m_listening_socket_rendezvous->Listen(NULL, m_backlog))
			)
			{
				OP_DELETE(m_listening_socket_rendezvous);
				m_listening_socket_rendezvous = NULL;
			}
		}
	}
	else
	{
		OP_DELETE(m_listening_socket_rendezvous);
		m_listening_socket_rendezvous = NULL;
	}
	return rendezvous_status;
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

WebServerConnectionListener::~WebServerConnectionListener()
{
#if defined(UPNP_SUPPORT) && defined(UPNP_SERVICE_DISCOVERY)
	if(m_upnp && (m_listeningMode & WEBSERVER_LISTEN_UPNP_DISCOVERY))
		m_upnp->SendByeBye(this);
#endif // defined(UPNP_SUPPORT) && defined(UPNP_SERVICE_DISCOVERY)

	g_main_message_handler->RemoveCallBacks(this, 0);
	g_pcwebserver->UnregisterListener(this);
	OP_DELETE(m_listening_socket_local);
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_DELETE(m_listening_socket_rendezvous);
#endif

#ifdef UPNP_SUPPORT
	if (m_upnp_port_opener)
	{
		OpStatus::Ignore(m_upnp_port_opener->RemoveListener(this));
		OP_DELETE(m_upnp_port_opener);
		m_upnp_port_opener=NULL;
	}
	//OP_DELETE(m_upnp); // m_upnp in NOT owned by the Connection Listener
#endif // UPNP_SUPPORT
	OP_DELETE(m_rootSubServer);

	// Can't rely on auto-delete of m_subServers, because it does not remove
	// entries while deleting, and when deleting the second, we may access the
	// first (deleted) WebSubServer through a long chain of calls starting from
	// g_webserver->CloseAllConnectionsToSubServer(...)
	while (m_subServers.GetCount() != 0)
	{
		WebSubServer *wss=m_subServers.Remove(0);

		OP_DELETE(wss);
	}
}

OpAutoVector<WebSubServer> *WebServerConnectionListener::GetWebSubServers()
{
	return &m_subServers;
}


void WebServerConnectionListener::CloseAllConnectionsToSubServer(WebSubServer *subserver)
{
	if (subserver == NULL)
		return;

	if (m_connectionList.Cardinal()>0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc())
		{
			if (subserver == connection->GetCurrentSubServer())
			{
				connection->KillService(); /*This will post a message back to this class to clean up meassages and kill the connection*/
			}
		}
	}
}

void WebServerConnectionListener::CloseAllConnections()
{
	for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc()) 
	{
		connection->KillService(); /*This will post a message back to this class to clean up meassages and kill the connection*/		
	}	
		
}


void WebServerConnectionListener::CloseAllConnectionsToWindow(unsigned int windowId)
{
	if (m_connectionList.Cardinal()>0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc())
		{
			WebSubServer *subserver = connection->GetCurrentSubServer();
			if (subserver && subserver->GetWindowId() == windowId)
			{
				connection->KillService(); /*This will post a message back to this class to clean up meassages and kill the connection*/
			}
		}
	}
}

int WebServerConnectionListener::NumberOfConnectionsToSubServer(WebSubServer *subserver)
{
	if (subserver == NULL)
		return 0;

	int numberOfConnections = 0;
	if (m_connectionList.Cardinal() > 0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc())
		{
			if (subserver == connection->GetCurrentSubServer())
			{
				numberOfConnections++;
			}
		}
	}

	return numberOfConnections;
}

int WebServerConnectionListener::NumberOfConnectionsInWindow(unsigned int windowId)
{
	int numberOfConnections = 0;
	if (m_connectionList.Cardinal() > 0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc())
		{
			WebSubServer *subserver = connection->GetCurrentSubServer();
			if (subserver && subserver->GetWindowId() == windowId)
			{
				numberOfConnections++;
			}
		}
	}

	return numberOfConnections;
}

int WebServerConnectionListener::NumberOfConnectionsWithRequests()
{
	int numberOfConnections = 0;
	if (m_connectionList.Cardinal() > 0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection = (WebServerConnection *) connection->Suc())
		{
			if (connection->GetCurrentRequest() && !connection->ClientIsOwner())
			{
				numberOfConnections++;
			}
		}
	}

	return numberOfConnections;
}

void WebServerConnectionListener::windowClosed(unsigned long windowId)
{
	CloseAllConnectionsToWindow(windowId);
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	BOOL b=advertisement_enabled;
	
	advertisement_enabled=FALSE;
#endif
	
	if (OpStatus::IsMemoryError(RemoveSubServers(windowId)))
	{
		g_webserverPrivateGlobalData->SignalOOM();
	}

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	advertisement_enabled=b;
#endif
}


WebSubServer *WebServerConnectionListener::WindowHasWebserverAssociation(unsigned long windowId)
{
	if (windowId != NO_WINDOW_ID)
	{
		if (m_rootSubServer && windowId == m_rootSubServer->GetWindowId() && m_rootSubServer->AllowEcmaScriptSupport() == TRUE)
		{
			return m_rootSubServer;
		}

		int n = m_subServers.GetCount();
		for (int i =0; i < n; i++)
		{
			WebSubServer *subServer = m_subServers.Get(i);
			if (subServer && windowId == subServer->GetWindowId() && subServer->AllowEcmaScriptSupport() == TRUE )
			{
				return subServer;
			}
		}
	}
	return NULL;
}

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
void WebServerConnectionListener::AdvertiseUPnP(BOOL delay)
{
	if(m_upnp && (m_listeningMode & WEBSERVER_LISTEN_UPNP_DISCOVERY) && advertisement_enabled)
	{
		OpStatus::Ignore(m_upnp->AdvertiseServicesProvider(this, TRUE, delay));
		// Updates are now disabled. When a control point will request a description (WebResource_UPnP_Service_Discovery::Make()), then they
		// will be actuvated again; this should reduce the number of messages generated
		advertisement_enabled=FALSE;
	}

	upnp_advertised = TRUE;

}
#endif

OP_STATUS WebServerConnectionListener::AddSubServer(WebSubServer *subServer, BOOL notify_upnp)
{
	/* FixMe : here we should check if a subserver with the same name (and window Id?) already exists */
	if (subServer == NULL)
		return OpStatus::ERR;

	if (subServer->GetSubServerVirtualPath().Compare("_", 1) == 0 
		&& subServer->GetSubServerVirtualPath().Compare("_upnp") != 0
		&& subServer->GetSubServerVirtualPath().Compare("_asd") != 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_subServers.Add(subServer));

	m_webserverEventListener->last_service_change=WebLogAccess::GetSystemTime();

	m_webserverEventListener->OnWebserverServiceStarted(subServer->GetSubServerName().CStr(), subServer->GetSubServerVirtualPath().CStr());

	if(notify_upnp && upnp_advertised) // The first notify is very critical, so we need to wait for it to be sent
		AdvertiseUPnP(TRUE);

	return OpStatus::OK;
}

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
OP_STATUS WebServerConnectionListener::InstallNativeRootService(WebServer *server)
{
	if (m_rootSubServer != NULL)
	{
		RETURN_IF_ERROR(RemoveSubServer(m_rootSubServer));
	}

	OpString webserver_private;

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, webserver_private));
	RETURN_IF_ERROR(webserver_private.Append("/resources/_root/"));

	WebSubServer *subserver = NULL;
	OP_STATUS status;

	if (OpStatus::IsError(status = WebSubServer::Make(subserver, server, NO_WINDOW_ID, webserver_private,  UNI_L("/"), UNI_L("Native Root Service"), UNI_L(""), UNI_L("My Server"), UNI_L(""), FALSE, FALSE)))
	{
		OP_DELETE(subserver);
		return status;
	}

	subserver->m_serviceType = WebSubServer::SERVICE_TYPE_BUILT_IN_ROOT_SERVICE;

	if (OpStatus::IsError(status = SetRootSubServer(subserver)))
	{
		OP_DELETE(subserver);
		return status;
	}

	return OpStatus::OK;
}

#endif //NATIVE_ROOT_SERVICE_SUPPORT

OP_STATUS WebServerConnectionListener::SetRootSubServer(WebSubServer *root_service)
{
	if (m_rootSubServer)
	{
		m_webserverEventListener->OnWebserverServiceStopped(m_rootSubServer->GetSubServerName().CStr(), m_rootSubServer->GetSubServerVirtualPath().CStr(), TRUE);
		OP_DELETE(m_rootSubServer);
		m_rootSubServer = NULL;
	}

	RETURN_IF_ERROR(root_service->m_subServerVirtualPath.Set("_root")); /* we force the root service to have _root as uri-path (virtual path) */

	m_rootSubServer = root_service;

	if (root_service->m_serviceType == WebSubServer::SERVICE_TYPE_SERVICE)
	{
		root_service->m_serviceType = WebSubServer::SERVICE_TYPE_CUSTOM_ROOT_SERVICE;
	}

	WebserverResourceDescriptor_AccessControl *authElement;
	RETURN_IF_ERROR(WebserverResourceDescriptor_AccessControl::Make(authElement, UNI_L("authresource"), UNI_L("Authentication needed"), FALSE));

	if (authElement == NULL || 	root_service->AddSubServerAccessControlResource(authElement) != OpBoolean::IS_FALSE)
	{
		OP_DELETE(authElement);
		return OpStatus::ERR;
	}

	m_webserverEventListener->OnWebserverServiceStarted(root_service->GetSubServerName().CStr(), root_service->GetSubServerVirtualPath().CStr(), TRUE);

	return OpStatus::OK;
}

OP_STATUS WebServerConnectionListener::RemoveSubServer(WebSubServer *subServer, BOOL notify_upnp)
{
	if (subServer == NULL)
	{
		return OpStatus::ERR;
	}
	else if (subServer == m_rootSubServer)
	{
#ifdef NATIVE_ROOT_SERVICE_SUPPORT
		if (m_rootSubServer->GetServiceType()  == WebSubServer::SERVICE_TYPE_BUILT_IN_ROOT_SERVICE) /* Don't remove the default native */
		{
			return OpStatus::OK;
		}
#endif //NATIVE_ROOT_SERVICE_SUPPORT

		m_webserverEventListener->last_service_change=WebLogAccess::GetSystemTime();
		m_webserverEventListener->OnWebserverServiceStopped(m_rootSubServer->GetSubServerName().CStr(), m_rootSubServer->GetSubServerVirtualPath().CStr(), TRUE);
		OP_DELETE(m_rootSubServer);
		m_rootSubServer = NULL;

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
		return InstallNativeRootService(subServer->m_webserver);
#else
		return OpStatus::OK;
#endif //NATIVE_ROOT_SERVICE_SUPPORT

	}
	else
	{
		OpString subserver_name;
		OpString subserver_virtual_path;
		RETURN_IF_ERROR(subserver_name.Set(subServer->GetSubServerName()));
		RETURN_IF_ERROR(subserver_virtual_path.Set(subServer->GetSubServerVirtualPath()));
		RETURN_IF_ERROR(m_subServers.Delete(subServer));
		m_webserverEventListener->last_service_change=WebLogAccess::GetSystemTime();
		m_webserverEventListener->OnWebserverServiceStopped(subserver_name.CStr(), subserver_virtual_path.CStr());

		if(notify_upnp && upnp_advertised) // Wait the first notification, to avoid triggering the anti-flood protection
			AdvertiseUPnP(TRUE);

		return OpStatus::OK;
	}
}


OP_STATUS WebServerConnectionListener::RemoveSubServers(unsigned long windowId)
{
#ifdef NATIVE_ROOT_SERVICE_SUPPORT
	if (m_rootSubServer && m_rootSubServer->GetServiceType()  != WebSubServer::SERVICE_TYPE_BUILT_IN_ROOT_SERVICE) /* Don't remove the default native */
#endif //NATIVE_ROOT_SERVICE_SUPPORT
	{
		if (m_rootSubServer && windowId == m_rootSubServer->GetWindowId())
		{
			m_webserverEventListener->last_service_change=WebLogAccess::GetSystemTime();
			m_webserverEventListener->OnWebserverServiceStopped(m_rootSubServer->GetSubServerName().CStr(), m_rootSubServer->GetSubServerVirtualPath().CStr(), TRUE);
			OP_DELETE(m_rootSubServer);
			m_rootSubServer = NULL;
		}
	}

	int i = 0;
	WebSubServer *subServer = m_subServers.Get(0);
	WebSubServer *nextSubServer;

	while (subServer)
	{
		nextSubServer = m_subServers.Get(i+1);
		if (windowId == subServer->GetWindowId())
		{
			OpStatus::Ignore(RemoveSubServer(subServer, FALSE)); // Only one UPnP notification
		}
		else
		{
			i++;
		}
		subServer = nextSubServer;
	}

	if(upnp_advertised)
		AdvertiseUPnP(TRUE);

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
	/*if (m_rootSubServer == NULL)
	{
		return InstallNativeRootService();
	}*/
#endif //NATIVE_ROOT_SERVICE_SUPPORT

	return OpStatus::OK;
}

void WebServerConnectionListener::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	WebServerConnection *connection;
	switch (msg)
	{
	case MSG_WEBSERVER_DELETE_CONNECTION:
	{
		/*We find and delete the connection given by par2*/
		OP_ASSERT( par1 == (INTPTR)this);

		connection = FindConnectionById(par2);
		if (connection)
		{
			connection->Out();
			OP_DELETE(connection);
		}
		break;
	}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	case MSG_WEBSERVER_DELETE_RENDESVOUZ_CONNECTION:
	{
		StopListeningProxy();
		break;
	}
#endif //WEBSERVER_RENDEZVOUS_SUPPORT

	case MSG_WEBSERVER_SET_UP_LISTENING_SOCKET:
	{
		OpStatus::Ignore(ReconnectLocalImmediately());

		break;
	}
	default:
		OP_ASSERT(!"We do not listen to this");
		break;
	}
}

WebServerConnection *WebServerConnectionListener::FindConnectionById(INTPTR connectionId)
{
	for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection= (WebServerConnection *) connection->Suc()) {
		if(connection->Id() == connectionId)
			return connection;
	}
	return 0;
}

OP_STATUS
WebServerConnectionListener::CreateNewService(OpSocket *listening_socket)
{
	OP_STATUS status;
	WebServerConnection *http_connection = OP_NEW(WebServerConnection, ());
	if (http_connection == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpSocket *socket = NULL;
	if (listening_socket == m_listening_socket_local)
	{
		RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&(socket), http_connection, SocketWrapper::NO_WRAPPERS));

		if (OpStatus::IsError(status = m_listening_socket_local->Accept(socket)))
		{
			OP_DELETE(http_connection);
			OP_DELETE(socket);
			return status;
		}
	}

	if (OpStatus::IsError(status = http_connection->Construct(socket, this)))
	{
		OP_DELETE(http_connection);
		OP_DELETE(socket);
		return status;
	}

	/*We must put the new WebServerConnection into a list*/
	http_connection->Into(&m_connectionList);
	return status;

}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
/*virtual*/ void
WebServerConnectionListener::OnRendezvousConnected(WebserverRendezvous *socket)
{
	m_webserverEventListener->OnProxyConnected();
}

/*virtual*/ void
WebServerConnectionListener::OnRendezvousConnectError(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry)
{
	WebserverStatus webserverStatus;

	switch (status)
	{
		case RENDEZVOUS_PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED:
			webserverStatus = PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_AUTH_FAILURE:
			webserverStatus = PROXY_CONNECTION_ERROR_AUTH_FAILURE;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY:
			webserverStatus = WEBSERVER_ERROR_MEMORY;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE:
			webserverStatus = PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK:
			webserverStatus = PROXY_CONNECTION_ERROR_NETWORK;
			break;


		case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_DOWN:
			webserverStatus = PROXY_CONNECTION_ERROR_PROXY_DOWN;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN:
			webserverStatus = PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN;
			break;

		default:
			webserverStatus = PROXY_CONNECTION_ERROR_NETWORK;
	}


	if (retry == FALSE) /* Rendezvous will try again */
	{
		g_main_message_handler->PostMessage(MSG_WEBSERVER_DELETE_RENDESVOUZ_CONNECTION, 0, 0);
	}

	m_webserverEventListener->OnProxyConnectionFailed(webserverStatus, retry);
}

/* virtual */ void
WebServerConnectionListener::OnRendezvousConnectionProblem(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry)
{
	WebserverStatus webserverStatus;
	switch (status)
	{
		case RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK:
			webserverStatus = PROXY_CONNECTION_ERROR_NETWORK;
			break;

		case RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY:
			webserverStatus = WEBSERVER_ERROR_MEMORY;
			break;

		default:
			webserverStatus = PROXY_CONNECTION_ERROR_NETWORK;
	}

	m_webserverEventListener->OnProxyConnectionProblem(webserverStatus, retry);
}

/*virtual*/ void
WebServerConnectionListener::OnRendezvousConnectionRequest(WebserverRendezvous *listening_socket)
{

	OP_ASSERT(listening_socket == m_listening_socket_rendezvous);

	OpSocket *socket = NULL;
	WebServerConnection *http_connection = OP_NEW(WebServerConnection, ());
	if (http_connection == NULL)
	{
		g_webserverPrivateGlobalData->SignalOOM();
		return;
	}

	OP_STATUS status;
	OpString8 socket_ip;
	if (OpStatus::IsError(status = m_listening_socket_rendezvous->Accept(&socket, http_connection, socket_ip)))
	{
		OP_DELETE(http_connection);
		OP_DELETE(socket);
		socket = NULL;
		g_webserverPrivateGlobalData->SignalOOM();
		return;
	}

	if (OpStatus::IsError(status = http_connection->Construct(socket, this
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
	                                                          , FALSE
	                                                          , FALSE
#endif
	                                                          , TRUE, socket_ip)
	                                                          )
	)

	{
		OP_DELETE(http_connection);
		OP_DELETE(socket);
		g_webserverPrivateGlobalData->SignalOOM();
		return;
	}

	/*We must put the new WebServerConnection into a list*/
	http_connection->Into(&m_connectionList);
}

/*virtual*/ void
WebServerConnectionListener::OnRendezvousClosed(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry, int code)
{
	OP_ASSERT(socket == m_listening_socket_rendezvous);
	if (socket == m_listening_socket_rendezvous)
	{
		WebserverStatus webserverStatus;

		switch (status)
		{
			case RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT:
				webserverStatus = PROXY_CONNECTION_LOGGED_OUT;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION:
				webserverStatus = PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION:
				webserverStatus = PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK:
				webserverStatus = PROXY_CONNECTION_ERROR_NETWORK;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY:
				webserverStatus = WEBSERVER_ERROR_MEMORY;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_DENIED:
				webserverStatus = PROXY_CONNECTION_ERROR_DENIED;
				break;

			case RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL:
				webserverStatus = PROXY_CONNECTION_ERROR_PROTOCOL;
				break;

			default:
				OP_ASSERT(!"UKNOWN ERROR CODE");
				webserverStatus = WEBSERVER_ERROR;
				break;
		}


		if (retry == FALSE)	/* Rendezvous will not try again */
		{
			g_main_message_handler->PostMessage(MSG_WEBSERVER_DELETE_RENDESVOUZ_CONNECTION, 0, 0);
		}

		m_webserverEventListener->OnProxyDisconnected(webserverStatus, retry);
	}
};

/* virtual */ void
WebServerConnectionListener::OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address)
{
	const char *separator=NULL;

	// direct_access_address is in the format address:port. We look for the last ':' to support also IPv6 addresses
	if(direct_access && (separator=op_strrchr(direct_access_address, ':'))!=NULL)
	{
		public_ip.Set(direct_access_address, separator-direct_access_address);
		public_port=op_atoi(separator+1);
	}
	else
	{
		public_ip.Empty();
		public_port=0;
	}
	m_webserverEventListener->OnDirectAccessStateChanged(direct_access, direct_access_address);
}

#endif // WEBSERVER_RENDEZVOUS_SUPPORT

void
WebServerConnectionListener::OnSocketConnectionRequest(OpSocket* socket)
{

	if (OpStatus::IsMemoryError(CreateNewService(socket)))
	{
		 g_webserverPrivateGlobalData->SignalOOM();
	}
}

void
WebServerConnectionListener::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(!"This is the connection listener. The listener should not receive this method");
}

void
WebServerConnectionListener::OnSocketDataSent(OpSocket* socket, UINT32 bytesSent)
{
	OP_ASSERT(!"This is the connection listener. The listener should not receive this method");
}

void
WebServerConnectionListener::OnSocketClosed(OpSocket* socket)
{
	OP_DELETE(m_listening_socket_local);
	m_listening_socket_local = NULL;

	if (m_connectionList.Cardinal()>0)
	{
		for (WebServerConnection * connection = (WebServerConnection *) m_connectionList.First(); connection; connection= (WebServerConnection *) connection->Suc())
		{
			connection->KillService(); /*This will post a message back to this class to clean up meassages and kill the connection*/

		}
	}

	if (m_hasPostedSetupListeningsocketMessage == FALSE)
	{
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0, 4000);
		m_hasPostedSetupListeningsocketMessage = TRUE;
	}
}

void
WebServerConnectionListener::OnSocketReceiveError(OpSocket* socket, OpSocket::Error socketErr)
{
	OP_ASSERT(!"This is the connection listener. The listener should not receive this method");

}

void
WebServerConnectionListener::OnSocketSendError(OpSocket* socket, OpSocket::Error socketErr)
{
	OP_ASSERT(!"This is the connection listener. The listener should not receive this method");
}

void
WebServerConnectionListener::OnSocketCloseError(OpSocket* socket, OpSocket::Error socketErr)
{

	OP_ASSERT(!"should not have been here");
}

void
WebServerConnectionListener::OnSocketDataSendProgressUpdate(OpSocket* socket, UINT32 bytesSent)
{
	OP_ASSERT(!"should not have been here");
}

void
WebServerConnectionListener::OnSocketConnected(OpSocket* socket)
{
	OP_ASSERT(!"should not have been here");
}

void
WebServerConnectionListener::OnSocketListenError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(m_listening_socket_local == socket);

	if (m_listening_socket_local == socket)
	{
		g_webserver->OnWebserverListenLocalFailed(WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN);
	}
}

void
WebServerConnectionListener::OnSocketConnectError(OpSocket* socket, OpSocket::Error socketErr)
{
	OP_ASSERT(!"Should not have been here");
}

#ifdef UPNP_SUPPORT
OP_STATUS WebServerConnectionListener::CloseUPnPPort()
{
	if(m_upnp_port_opener && m_opened_port)
	{
		RETURN_IF_ERROR(ReEnableUPnPPortForwarding());

		return m_upnp_port_opener->QueryDisablePort(m_opened_device, m_opened_port);
	}

	return OpStatus::ERR_NULL_POINTER;
}

void WebServerConnectionListener::DisableUPnPPortForwarding()
{
	if (m_upnp_port_opener)
	{
		OpStatus::Ignore(m_upnp_port_opener->RemoveListener(this));
		m_upnp_port_opener->Disable();
		//OP_DELETE(m_upnp_port_opener);
		//m_upnp_port_opener = NULL;
	}
}

OP_STATUS WebServerConnectionListener::ReEnableUPnPPortForwarding()
{
	if (m_upnp_port_opener)
	{
		OpStatus::Ignore(m_upnp_port_opener->AddListener(this));
		return m_upnp_port_opener->ReEnable();
	}

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS WebServerConnectionListener::Failure(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic)
{
	/// There was a failure during the process, on the specified device
	// We want to disable UPnP port forwarding without removing UPnP Service discovery
	OP_ASSERT(upnp && m_upnp==upnp);

	m_port_checking_enabled=TRUE;
	
	if (m_real_listening_port != m_prefered_listening_port)
	{
		m_real_listening_port = m_prefered_listening_port;
		OP_DELETE(m_listening_socket_local);
		m_listening_socket_local = NULL;
	}

	m_listeningMode &= ~((unsigned int)(WEBSERVER_LISTEN_OPEN_PORT)); // Turn off WEBSERVER_LISTEN_OPEN_PORT;

	DisableUPnPPortForwarding();

	//if(m_upnp)
	//	m_upnp->SetSelfDelete(TRUE);
	//m_upnp = NULL;				// Deleted later by SetSelfDelete()
	//m_upnp_port_opener = NULL; // Deleted by m_upnp

	if(!upnp_advertised)
	{
		upnp_advertised=TRUE;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		advertisement_enabled=TRUE;
#endif
		AdvertiseUPnP(TRUE);
	}
	
	//if (m_listening_socket_rendezvous)
	//	m_listening_socket_rendezvous->CheckPort(m_real_listening_port);

	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_SET_UP_LISTENING_SOCKET, 0, 0, 0);

	return OpStatus::OK;
}

/// It was possible to open the specified external port
OP_STATUS WebServerConnectionListener::Success(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port)
{
	/* At this point the webserver is listening on the port m_real_listening_port and UPnP is working */
	m_port_checking_enabled=TRUE;
	m_opened_device=device;
	m_opened_port=internal_port;
	DisableUPnPPortForwarding(); // Stop trying to open more ports...

	if(m_webserverEventListener)
		m_webserverEventListener->OnPortOpened(device, internal_port, external_port);

	if(!upnp_advertised)
	{
		upnp_advertised=TRUE;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		advertisement_enabled=TRUE;
#endif
		AdvertiseUPnP(TRUE);
	}
	
	OP_ASSERT(m_real_listening_port==m_opened_port);
	
	if (m_listening_socket_rendezvous)
		m_listening_socket_rendezvous->CheckPort(m_opened_port);

	return StartListeningRendezvous();
}

OP_STATUS WebServerConnectionListener::ArePortsAcceptable(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port, BOOL &ports_acceptable)
{
	m_real_listening_port = internal_port;
	OP_STATUS ops = OpStatus::OK;
	if (m_listening_socket_local != NULL)
	{
		if (GetLocalListeningPort() != internal_port)
		{
			StopListeningLocal();
			// Immediate reconnection, to allocate the port
			ops = ReconnectLocal(internal_port, TRUE);
		}
	}
	else
	{
		ops = StartListening();
	}

	// Listening: port acceptabled
	if(OpStatus::IsSuccess(ops))
	{
		OP_ASSERT(m_listening_socket_local);
		ports_acceptable=TRUE;

		return OpStatus::OK;
	}

	ports_acceptable=FALSE;

	// Memory error: stop the port opening process
	if(OpStatus::IsMemoryError(ops))
		return ops;

	// Other errors (usually port already in use): continue the process

	return OpStatus::OK;
}

OP_STATUS WebServerConnectionListener::PortClosed(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 port, BOOL success)
{
	DisableUPnPPortForwarding();

	OP_ASSERT(port==m_opened_port);

	if(port==m_opened_port)
		m_opened_port=0;

	if (m_listening_socket_local)
		m_webserverEventListener->OnWebserverUPnPPortsClosed(port);

	return OpStatus::OK;
}

OP_STATUS WebServerConnectionListener::PortsRefused(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port)
{
	//RETURN_IF_ERROR(StartListeningRendezvous());/* Connect to proxy even if port wasn't open */
	if(m_listening_socket_local)
	{
		m_listening_socket_local->Close();

		OP_DELETE(m_listening_socket_local);
		m_listening_socket_local=NULL;
	}

	return OpStatus::OK;
}
#endif // UPNP_SUPPORT

#endif //WEBSERVER_SUPPORT
