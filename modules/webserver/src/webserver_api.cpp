/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2011
 *
 * Web server implementation -- overall server logic
 */


#include "core/pch.h"
#ifdef WEBSERVER_SUPPORT

#include "modules/util/opguid.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/util/handy.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"

#include "modules/url/protocols/http_met.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_module.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/webserver/src/myopera/upload_service_list.h"
#include "modules/idna/idna.h"
#ifdef OPERA_AUTH_SUPPORT
#include "modules/opera_auth/opera_auth_module.h"
#endif

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#endif // WEBSERVER_RENDEZVOUS_SUPPORT


WebServer::WebServer()
	: m_connectionListener(FALSE, TRUE)
#ifdef WEB_UPLOAD_SERVICE_LIST
	, m_upload_service_list_manager(NULL)
#endif
#ifdef TRANSFERS_TYPE
	, m_bytes_uploaded(0)
	, m_bytes_downloaded(0)
#endif
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	, m_rendezvousConnected(FALSE)
#endif
	, last_service_change(0)
	, m_always_listen_to_all_networks(FALSE)
{
}

WebServer::~WebServer()
{
	g_pcwebserver->UnregisterListener(this);
	
	//OP_DELETE(m_connectionListener);
#ifdef WEB_UPLOAD_SERVICE_LIST	
	OP_DELETE(m_upload_service_list_manager);
#endif
	log_users.DeleteAll();
}

OP_STATUS
WebServer::AddWebserverListener(WebserverEventListener *listener)
{
	if(m_webserverEventListeners.Find(listener) < 0)
	{
		return m_webserverEventListeners.Add(listener);
	}

	return OpStatus::OK;
}

void
WebServer::RemoveWebserverListener(WebserverEventListener *listener)
{
	if(m_webserverEventListeners.Find(listener) > -1)
	{
		m_webserverEventListeners.RemoveByItem(listener);
	}
}

/* static */ OP_STATUS
WebServer::Init()
{
	OP_STATUS status = OpStatus::OK;

	OP_ASSERT(g_webserver == NULL);
	OP_ASSERT(g_webserverPrivateGlobalData == NULL);

	if ( g_webserver )
		return OpStatus::ERR;

	if ((g_webserver = OP_NEW(WebServer, ())) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if ((g_webserverPrivateGlobalData = OP_NEW(WebserverPrivateGlobalData, ())) == NULL)
	{
		OP_DELETE(g_webserver);
		g_webserver = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	
	RETURN_IF_LEAVE(g_pcwebserver->RegisterListenerL(g_webserver));
	
#if defined(OPERA_AUTH_SUPPORT) && defined(WEBSERVER_RENDEZVOUS_SUPPORT)
	g_opera_account_manager->ConditionallySetWebserverUser();
#endif

	return status;
}

#ifdef SCOPE_SUPPORT
/* static */ OP_STATUS
WebServer::InitDragonflyUPnPServer()
{
	if ( g_dragonflywebserver )
		return OpStatus::ERR;

	if ((g_dragonflywebserver = OP_NEW(WebServer, ())) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(g_pcwebserver->RegisterListenerL(g_dragonflywebserver));
	return OpStatus::OK;
}
#endif // SCOPE_SUPPORT

OP_STATUS WebServer::SetServerName()
{
	OpString user; 
	OpString dev;
	OpString proxy;

	RETURN_IF_ERROR(user.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
	RETURN_IF_ERROR(dev.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
	RETURN_IF_ERROR(proxy.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost)));

	//// Manage "admin"
	OpString admin_name;

	if (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		OpString name;
		OpHostResolver* resolver = NULL;

		name.Set(UNI_L("localhost"));

		if (m_always_listen_to_all_networks || g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks))
		{
			OP_STATUS op_err = SocketWrapper::CreateHostResolver(&resolver, NULL);

			if(OpStatus::IsSuccess(op_err))
			{
				OpHostResolver::Error resolv_err = OpHostResolver::NETWORK_NO_ERROR;
				OpStatus::Ignore(resolver->GetLocalHostName(&name, &resolv_err));
			}
			OP_DELETE(resolver);
		}

		RETURN_IF_ERROR(m_webserver_server_name.Set(name));
		RETURN_IF_ERROR(admin_name.Set(name));
	}
	else
	{
		/* Strip, force to lower case and put back in. */
		if (user.HasContent())
			RETURN_IF_LEAVE(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverUser, uni_strlwr(user.Strip().CStr())));
		
		if (dev.HasContent())
			RETURN_IF_LEAVE(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverDevice, uni_strlwr(dev.Strip().CStr())));
		
		if (proxy.HasContent())
			RETURN_IF_LEAVE(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverProxyHost, uni_strlwr(proxy.Strip().CStr())));
		
		
		RETURN_IF_ERROR(m_webserver_server_name.Set(dev));
		RETURN_IF_ERROR(m_webserver_server_name.Append("."));
		RETURN_IF_ERROR(m_webserver_server_name.Append(user));
		RETURN_IF_ERROR(m_webserver_server_name.Append("."));
		RETURN_IF_ERROR(m_webserver_server_name.Append(proxy));
		RETURN_IF_ERROR(admin_name.AppendFormat(UNI_L("admin.%s"), m_webserver_server_name.CStr()));
	}

	
	RETURN_IF_LEAVE(sn_main=g_url_api->GetServerName(m_webserver_server_name.CStr(), TRUE));
	RETURN_IF_LEAVE(sn_admin=g_url_api->GetServerName(admin_name.CStr(), TRUE));
	RETURN_IF_LEAVE(sn_localhost_1=g_url_api->GetServerName("localhost", TRUE));
	RETURN_IF_LEAVE(sn_localhost_2=g_url_api->GetServerName("127.0.0.1", TRUE));

	//if these are not valid names we have to fail startup.
	if ( !(ServerName*) sn_admin || !(ServerName*) sn_main)
		return OpStatus::ERR;


#ifdef TRUST_RATING
	//Set server name properties so that fraud check is skipped
	sn_main->SetTrustRatingBypassed(TRUE);
	sn_admin->SetTrustRatingBypassed(TRUE);
#endif //TRUST_RATING

	if (m_webserver_server_name_idna.Reserve(1025) == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	if (user.HasContent() && dev.HasContent() && proxy.HasContent())
		RETURN_IF_LEAVE(IDNA::ConvertToIDNA_L(m_webserver_server_name.CStr(), m_webserver_server_name_idna.CStr() , m_webserver_server_name_idna.Capacity(), FALSE));

	return OpStatus::OK;
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT	
OP_STATUS
WebServer::Start(WebserverListeningMode mode, unsigned int port, const char *shared_secret UPNP_PARAM_COMMA(UPnP *upnp))
#else
OP_STATUS
WebServer::Start(WebserverListeningMode mode UPNP_PARAM_COMMA(UPnP *upnp))
#endif
{
	OP_ASSERT((IsRunning() == TRUE && GetConnectionListener() != NULL) || ( IsRunning() == FALSE && GetConnectionListener() == NULL));
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

	OpString8 secret;
	secret.Set(shared_secret);
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	/// Create the GUID of the instance
	if(!uuid.HasContent())
	{
		OpGuid guid;

		if(!uuid.Reserve(50))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
		g_opguidManager->ToString(guid, uuid.DataPtr(), 50);
		
		OP_ASSERT(uuid.HasContent());
	}
#endif

	if (mode & WEBSERVER_LISTEN_DEFAULT)
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		secret.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword));
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

		if (!m_always_listen_to_all_networks && (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount) && !g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks)) )
			mode = WEBSERVER_LISTEN_LOCAL;
		else
		{
			mode = WEBSERVER_LISTEN_PROXY_LOCAL;

#ifdef UPNP_SUPPORT
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled))
			{
				mode |= WEBSERVER_LISTEN_OPEN_PORT;
			}
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled))
			{
				mode |= WEBSERVER_LISTEN_UPNP_DISCOVERY;
			}
#endif // UPNP_SUPPORT

			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::RobotsTxtEnabled))
			{
				mode |= WEBSERVER_LISTEN_ROBOTS_TXT;
			}
		}
		mode |= WEBSERVER_LISTEN_DIRECT;
	}
	RETURN_IF_ERROR(SetServerName());

	if (IsRunning())
	{
		return OpStatus::OK;
	}

	WebServerConnectionListener *lsn=NULL;
	OP_STATUS ops=WebServerConnectionListener::Make(
	                                lsn,
									mode,
									this,
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
									secret,
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
									port,
									g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverBacklog),
									g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverUploadRate)
									UPNP_PARAM_COMMA(upnp?upnp:g_upnp)
									);
	RETURN_IF_ERROR(ops);

	OP_ASSERT(lsn);
	m_connectionListener.AddReference(lsn->GetReferenceObjectPtr());
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	if(mode & WEBSERVER_LISTEN_UPNP_DISCOVERY)
	{
		// Create the _upnp service, used for UPnP Service Discovery
		OpString upnp_service_storage_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, upnp_service_storage_folder));
		RETURN_IF_ERROR(upnp_service_storage_folder.Append("/resources/_upnp_gadgets/"));

		WebSubServer *subServer_UPnP=NULL;
		WebserverResourceDescriptor_UPnP *rd_desc=NULL;
		WebserverResourceDescriptor_UPnP *rd_spcd=NULL;

		if(OpStatus::IsError(ops=WebSubServer::Make(subServer_UPnP, this, NO_WINDOW_ID, upnp_service_storage_folder, UNI_L("upnpService"), UNI_L("_upnp"), UNI_L("Opera Software ASA"), UNI_L("UPnP Service Discovery"), UNI_L(""), FALSE, FALSE, FALSE)) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_UPnP::Make(rd_desc, UNI_L("desc"))) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_UPnP::Make(rd_spcd, UNI_L("spcd"))) ||
			OpStatus::IsError(ops=subServer_UPnP->AddSubserverResource(rd_desc)) ||
			OpStatus::IsError(ops=subServer_UPnP->AddSubserverResource(rd_spcd)) ||
			OpStatus::IsError(ops=this->AddSubServer(subServer_UPnP, FALSE)))
		{
			OP_DELETE(subServer_UPnP);
			OP_DELETE(rd_spcd);
			OP_DELETE(rd_desc);
			
			return ops;
		}
		
		subServer_UPnP->visible=FALSE;
	}

#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

	if(mode & WEBSERVER_LISTEN_ROBOTS_TXT)
	{
		// Create the robots.txt file used to block searchengines
		OpString robots_service_storage_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, robots_service_storage_folder));
		RETURN_IF_ERROR(robots_service_storage_folder.Append("/resources/_robots_txt_gadgets/"));
		
		WebSubServer *subServer_Robots=NULL;
		WebserverResourceDescriptor_Robots *rd_robots=NULL;

		if(OpStatus::IsError(ops=WebSubServer::Make(subServer_Robots, this, NO_WINDOW_ID, robots_service_storage_folder, UNI_L("_robots_txt"), UNI_L("robots.txt"), UNI_L("Opera Software ASA"), UNI_L("Search Engine robots.txt"), UNI_L(""), FALSE, FALSE, FALSE)) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_Robots::Make(rd_robots, UNI_L("/"))) ||
			OpStatus::IsError(ops=subServer_Robots->AddSubserverResource(rd_robots)) ||
			OpStatus::IsError(ops=this->AddSubServer(subServer_Robots, FALSE)))
		{
			OP_DELETE(subServer_Robots);
			OP_DELETE(rd_robots);
			
			return ops;
		}

		subServer_Robots->visible=FALSE;
	}

	return ops;
}

#if defined(UPNP_SUPPORT)
OP_STATUS WebServer::StartUPnPServer(WebserverListeningMode mode, unsigned int port, const char* upnpDeviceType, const uni_char* upnpDeviceIcon, const uni_char* upnpPayload UPNP_PARAM_COMMA(UPnP *upnp /* NULL */))
{
	OP_ASSERT((IsRunning() == TRUE && GetConnectionListener() != NULL) || ( IsRunning() == FALSE && GetConnectionListener() == NULL));

#if defined (UPNP_SERVICE_DISCOVERY)
	/// Create the GUID of the instance
	if(!uuid.HasContent())
	{
		OpGuid guid;

		if(!uuid.Reserve(50))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
		g_opguidManager->ToString(guid, uuid.DataPtr(), 50);

		OP_ASSERT(uuid.HasContent());
	}
#endif

	if (mode & WEBSERVER_LISTEN_DEFAULT)
	{
		if (!m_always_listen_to_all_networks && (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount) && !g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks)) )
			mode = WEBSERVER_LISTEN_LOCAL;
		else
		{
			mode = WEBSERVER_LISTEN_PROXY_LOCAL;
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled))
			{
				mode |= WEBSERVER_LISTEN_OPEN_PORT;
			}
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled))
			{
				mode |= WEBSERVER_LISTEN_UPNP_DISCOVERY;
			}
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::RobotsTxtEnabled))
			{
				mode |= WEBSERVER_LISTEN_ROBOTS_TXT;
			}
		}
		mode |= WEBSERVER_LISTEN_DIRECT;
	}
	RETURN_IF_ERROR(SetServerName());

	if (IsRunning())
	{
		return OpStatus::OK;
	}

	WebServerConnectionListener *lsn=NULL;
	OP_STATUS ops=WebServerConnectionListener::Make(
	                                lsn,
									mode,
									this,
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
									NULL,
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
									port,
									g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverBacklog),
									g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverUploadRate)
									UPNP_PARAM_COMMA(upnp?upnp:g_upnp),
									upnpDeviceType,
									upnpDeviceIcon,
									upnpPayload
									);
	RETURN_IF_ERROR(ops);

	OP_ASSERT(lsn);
	m_connectionListener.AddReference(lsn->GetReferenceObjectPtr());
#if defined (UPNP_SERVICE_DISCOVERY)
	if(mode & WEBSERVER_LISTEN_UPNP_DISCOVERY)
	{
		// Create the _upnp service, used for UPnP Service Discovery
		OpString upnp_service_storage_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, upnp_service_storage_folder));
		RETURN_IF_ERROR(upnp_service_storage_folder.Append("/resources/_upnp_gadgets/"));

		WebSubServer *subServer_UPnP=NULL;
		WebserverResourceDescriptor_UPnP *rd_desc=NULL;
		WebserverResourceDescriptor_UPnP *rd_spcd=NULL;

		if(OpStatus::IsError(ops=WebSubServer::Make(subServer_UPnP, this, NO_WINDOW_ID, upnp_service_storage_folder, UNI_L("upnpService"), UNI_L("_upnp"), UNI_L("Opera Software ASA"), UNI_L("UPnP Service Discovery"), UNI_L(""), FALSE, FALSE, FALSE)) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_UPnP::Make(rd_desc, UNI_L("desc"))) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_UPnP::Make(rd_spcd, UNI_L("spcd"))) ||
			OpStatus::IsError(ops=subServer_UPnP->AddSubserverResource(rd_desc)) ||
			OpStatus::IsError(ops=subServer_UPnP->AddSubserverResource(rd_spcd)) ||
			OpStatus::IsError(ops=this->AddSubServer(subServer_UPnP, FALSE)))
		{
			OP_DELETE(subServer_UPnP);
			OP_DELETE(rd_spcd);
			OP_DELETE(rd_desc);

			return ops;
		}

		subServer_UPnP->visible=FALSE;
	}

#endif //  defined (UPNP_SERVICE_DISCOVERY)
	return ops;
}
#endif // UPNP_SUPPORT

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
OP_STATUS WebServer::EnableUPnPServicesDiscovery()
{
	if(!GetConnectionListener())
		return OpStatus::ERR_NULL_POINTER;
		
	return GetConnectionListener()->EnableUPnPServicesDiscovery();
}

UPnPServicesProvider *WebServer::GetUPnPServicesProvider()
{
	return GetConnectionListener();
}

#endif
	
/*OP_STATUS WebServer::GetCredentials(OpString &user, OpString &device)
{
	RETURN_IF_ERROR(user.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
	
	return device.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice));
}*/

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT	
OP_STATUS WebServer::ChangeListeningMode(WebserverListeningMode mode, const char *shared_secret)
#else
OP_STATUS WebServer::ChangeListeningMode(WebserverListeningMode mode)
#endif	
{
	OpString8 secret;
	secret.Set(shared_secret);
	int real_port = 0;

	if (mode & WEBSERVER_LISTEN_DEFAULT)
	{
		secret.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword));
		real_port = g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort);

		if (!m_always_listen_to_all_networks && (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount) && !g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks)) )
			mode = WEBSERVER_LISTEN_LOCAL;
		else
		{
			mode = WEBSERVER_LISTEN_PROXY_LOCAL;

#ifdef UPNP_SUPPORT
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled))
			{
				mode |= WEBSERVER_LISTEN_OPEN_PORT;
			}
			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled))
			{
				mode |= WEBSERVER_LISTEN_UPNP_DISCOVERY;
			}
#endif // UPNP_SUPPORT

			if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::RobotsTxtEnabled))
			{
				mode |= WEBSERVER_LISTEN_ROBOTS_TXT;
			}
		}

		mode |= WEBSERVER_LISTEN_DIRECT;
	}

	RETURN_IF_ERROR(SetServerName());

	if (IsRunning())
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT			
		if (mode & WEBSERVER_LISTEN_PROXY)
		{
			if (secret.IsEmpty())
				return OpStatus::ERR;
			
			RETURN_IF_ERROR(GetConnectionListener()->ReconnectProxy(secret.CStr()));
		}
		else
		{
			GetConnectionListener()->StopListeningProxy();	
		}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

		if (mode & WEBSERVER_LISTEN_DIRECT)
			GetConnectionListener()->StartListeningDirectMemorySocket();
		else
			GetConnectionListener()->StopListeningDirectMemorySocket();

		if (mode & WEBSERVER_LISTEN_LOCAL)
		{
			RETURN_IF_ERROR(GetConnectionListener()->ReconnectLocal(real_port, FALSE));
		}
		else
		{
			GetConnectionListener()->StopListeningLocal();
		}

		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR;
	}
}

#ifdef UPNP_SUPPORT
OP_STATUS WebServer::CloseUPnPPort()
{
	return GetConnectionListener()?GetConnectionListener()->CloseUPnPPort():OpStatus::ERR_NULL_POINTER;
}

UPnP *WebServer::GetUPnP()
{
	if(!GetConnectionListener())
		return NULL;
		
	return GetConnectionListener()->GetUPnP();
}
#endif
	
OP_STATUS
WebServer::Stop(WebserverStatus status)
{
	if (g_webserverPrivateGlobalData == NULL)
		return OpStatus::ERR;
	
	if (IsRunning() == FALSE)
		return OpStatus::OK;

	OP_DELETE(GetConnectionListener());
	
	m_connectionListener.CleanReference();
	
	UINT32 i;
	for(i = 0; i < m_webserverEventListeners.GetCount(); i++)
	{
		m_webserverEventListeners.Get(i)->OnWebserverStopped(status);
	}
	
	return OpStatus::OK;
}

unsigned int 
WebServer::GetLocalListeningPort()
{
	if (IsRunning())
	{
		return GetConnectionListener()->GetLocalListeningPort();
	}
	else
	{
		return g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort);
	}
}

// Get the public IP address of the server (if direct connection is available)
const uni_char *
WebServer::GetPublicIP()
{
	if (IsRunning() && !GetConnectionListener()->public_ip.IsEmpty())
		return GetConnectionListener()->public_ip.CStr();
	
	return NULL;
}
// Get the public port of the server (if direct connection is available)
UINT16
WebServer::GetPublicPort()
{
	if (IsRunning())
		return GetConnectionListener()->public_port;
	
	return 0;
}
	
void 
WebServer::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{

}

void 
WebServer::PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if 	(OpPrefsCollection::Webserver == id && 
			(
				pref == PrefsCollectionWebserver::WebserverUser ||
				pref == PrefsCollectionWebserver::WebserverDevice || 
				pref == PrefsCollectionWebserver::WebserverProxyHost ||
				pref == PrefsCollectionWebserver::WebserverHashedPassword
			)
		)
	{
		OpString value;	
		if (OpStatus::IsSuccess(value.Set(newvalue)))
		{
			value.Strip();
			if (pref != PrefsCollectionWebserver::WebserverHashedPassword)
			{
				value.MakeLower();
			}
			
			if (newvalue.Compare(value))
			{
				/* Put back the stripped preference */
				TRAPD(err, g_pcwebserver->WriteStringL( static_cast<PrefsCollectionWebserver::stringpref>(pref), value.CStr()));
			}
		}
	}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT		
/* virtual */ void
WebServer::OnProxyConnected()
{
	m_rendezvousConnected = TRUE;

	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnProxyConnected();
	}
#ifdef WEB_UPLOAD_SERVICE_LIST
	if (m_upload_service_list_manager && IsRunning())
	{
		OpStatus::Ignore(m_upload_service_list_manager->Upload(GetConnectionListener()->GetWebSubServers(), m_upload_service_token.CStr()));
	}
#endif

}

/* virtual */ void
WebServer::OnProxyConnectionFailed(WebserverStatus status, BOOL retry)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnProxyConnectionFailed(status, retry);
	}
}

/* virtual */ void
WebServer::OnProxyConnectionProblem(WebserverStatus status, BOOL retry)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnProxyConnectionProblem(status, retry);
	}
}

/* virtual */ void
WebServer::OnProxyDisconnected(WebserverStatus status, BOOL retry, int code)
{
	m_rendezvousConnected = FALSE;

	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnProxyDisconnected(status, retry, code);
	}
	
}
	
#endif 	//WEBSERVER_RENDEZVOUS_SUPPORT

/* virtual */ void 
WebServer::OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnNewDOMEventListener(service_name, evt, virtual_path);
	}
}

/* virtual */ void 
WebServer::OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnDirectAccessStateChanged(direct_access, direct_access_address);
	}
}

/* virtual */ void
WebServer::OnWebserverStopped(WebserverStatus status)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverStopped(status);
	}
}

void WebServer::OnWebserverUPnPPortsClosed(UINT16 port)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverUPnPPortsClosed(port);
	}
}

	
/* virtual */ void
WebServer::OnWebserverServiceStarted(const uni_char *service_name, const uni_char *service_path,  BOOL is_root_service)
{
#ifdef WEB_UPLOAD_SERVICE_LIST
	if (m_upload_service_list_manager && IsRunning() && is_root_service == FALSE)
	{
		OpStatus::Ignore(m_upload_service_list_manager->Upload(GetConnectionListener()->GetWebSubServers(), m_upload_service_token.CStr()));	
	}
#endif
	
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverServiceStarted(service_name, service_path, is_root_service);
	}
};

/* virtual */ void
WebServer::OnWebserverListenLocalStarted(unsigned int port)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverListenLocalStarted(port);
	}
}

/* virtual */ void
WebServer::OnWebserverListenLocalStopped()
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverListenLocalStopped();
	}
}
/* virtual */ void
WebServer::OnWebserverListenLocalFailed(WebserverStatus status)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverListenLocalFailed(status);
	}
}

	
/* virtual */ void
WebServer::OnWebserverServiceStopped(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service)
{
#ifdef WEB_UPLOAD_SERVICE_LIST
	if (m_upload_service_list_manager && IsRunning() && is_root_service == FALSE)
	{
		OpStatus::Ignore(m_upload_service_list_manager->Upload(GetConnectionListener()->GetWebSubServers(), m_upload_service_token.CStr()));	
	}
#endif // WEB_UPLOAD_SERVICE_LIST

	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverServiceStopped(service_name, service_path, is_root_service);
	}
}

#ifdef WEB_UPLOAD_SERVICE_LIST
/* virtual */ void
WebServer::UploadServiceList()
{
#ifdef WEB_UPLOAD_SERVICE_LIST
	if (m_upload_service_list_manager && IsRunning())
	{
		OpStatus::Ignore(m_upload_service_list_manager->Upload(GetConnectionListener()->GetWebSubServers(), m_upload_service_token.CStr()));    
	}
#endif // WEB_UPLOAD_SERVICE_LIST
}

/* virtual */ void
WebServer::OnWebserverUploadServiceStatus(UploadServiceStatus status)
{
	for(unsigned int i = m_webserverEventListeners.GetCount(); i-- > 0; )
	{
		m_webserverEventListeners.Get(i)->OnWebserverUploadServiceStatus(status);
	}
}
#endif // WEB_UPLOAD_SERVICE_LIST

WebserverListeningMode 
WebServer::GetCurrentMode()
{
	if (IsRunning())
		return GetConnectionListener()->GetCurrentMode();
	else
		return WEBSERVER_LISTEN_NONE;
}

BOOL WebServer::MatchServer(const ServerName *host, unsigned short port)
{
	if (!IsRunning())
		return FALSE;
	unsigned int localListeningPort = GetConnectionListener()->GetLocalListeningPort();
	if (host==(sn_localhost_1.operator ServerName *()) || host==(sn_localhost_2.operator ServerName *()))
		return (port==localListeningPort);
	return ((host==(sn_main.operator ServerName *()) || host==(sn_admin.operator ServerName *())) && (port==80 || port==localListeningPort));
}

BOOL WebServer::MatchServerAdmin(const ServerName *host, unsigned short port)
{
	if (!IsRunning())
		return FALSE;
	unsigned int localListeningPort = GetConnectionListener()->GetLocalListeningPort();
	if (host==(sn_localhost_1.operator ServerName *()) || host==(sn_localhost_2.operator ServerName *()))
		return (port==localListeningPort);
	return (host==(sn_admin.operator ServerName *()) && (port==80 || port==localListeningPort));
}

BOOL WebServer::MatchServerAdmin(const char *host)
{
	if (IsRunning() && host)
	{
		const char* name_admin;
		const char* port_pos;
		unsigned int port=80;
		unsigned int localListeningPort = GetConnectionListener()->GetLocalListeningPort();
		
		if ((port_pos = op_strchr(host, ':')) != NULL)
		{
			port = (unsigned int)op_atoi(port_pos+1);
			if (port <= 0 || port>0xFFFF)
				port = 80;
		}

		// Match on localhost and 127.0.0.1
		if ((!op_strncmp(host, "localhost", 9) || !op_strncmp(host, "127.0.0.1", 9)) &&
			(host[9]=='\0' || host[9]==':'))
			return port==localListeningPort;

		if ((name_admin=sn_admin->Name())!=NULL)
		{
			int len=op_strlen(name_admin);
			
			// Match on the exact server name but also on the server:port format
			if (!op_strncmp(host, name_admin, len) && (host[len]=='\0' || host[len]==':') &&
				(port==80 || port==localListeningPort))
				return TRUE;
		}
	}
	
	return FALSE;
}
	
void 
WebServer::windowClosed(unsigned long windowId)
{
	if (IsRunning())
		GetConnectionListener()->windowClosed(windowId);		
}


WebSubServer*
WebServer::WindowHasWebserverAssociation(unsigned long windowId)
{
	if (IsRunning())
		return GetConnectionListener()->WindowHasWebserverAssociation(windowId);
	else
		return NULL;
}

int WebServer::NumberOfConnectionsToSubServer(WebSubServer *subServer)
{
	if (IsRunning())
		return GetConnectionListener()->NumberOfConnectionsToSubServer(subServer);
	else
		return 0;
}

OP_STATUS 
WebServer::AddSubServer(WebSubServer *subServer, BOOL notify_upnp)
{
	if (IsRunning() && subServer != NULL)
	{
		if (GetSubServer(subServer->GetSubServerVirtualPath()))
			return OpStatus::ERR;

		return GetConnectionListener()->AddSubServer(subServer, notify_upnp); 
	}
	else
		return OpStatus::ERR;
}

const uni_char* WebServer::StrFileExt(const uni_char* fName)
{
	if( !fName)
		return NULL;

	uni_char *ext = (uni_char*) uni_strrchr( fName, '.');
	
	if(!ext)
		return fName;
		
	return ext+1;
}

OP_STATUS 
WebServer::RemoveSubServers(unsigned long windowId)
{
	if (IsRunning())	
		return GetConnectionListener()->RemoveSubServers(windowId);
	else
		return OpStatus::OK;
}

WebSubServer* 
WebServer::GetSubServer(const OpStringC &subserverVirtualPath)
{
	if (!IsRunning())
		return NULL;
	
	if (subserverVirtualPath.CStr() == NULL)
	{
		return NULL;
	}
		
	if (subserverVirtualPath == UNI_L("_root"))
	{
		return  GetConnectionListener()->GetRootSubServer();
	}
	
	OpAutoVector<WebSubServer> *subservers = NULL;
	if (IsRunning() && NULL!=(subservers = GetConnectionListener()->GetWebSubServers()))
	{

		int n = subservers->GetCount();
		for (int i = 0; i < n; i++)
		{
			WebSubServer *subServer = subservers->Get(i);
		
			if (subServer && subserverVirtualPath == subServer->GetSubServerVirtualPath())
				return subServer;
		}
	}
	

	return NULL;	
}

WebSubServer *
WebServer::GetSubServer(UINT32 idx)
{
	return GetSubServerAtIndex(idx);
}

WebSubServer* 
WebServer::GetSubServerAtIndex(UINT32 idx)
{
	OpAutoVector<WebSubServer> *subservers = NULL;
	if (IsRunning() && (subservers = GetConnectionListener()->GetWebSubServers()))
	{
		return subservers->Get(idx);
	}
	return NULL;
}

void 
WebServer::CloseAllConnectionsToSubServer(WebSubServer* subserver)
{
	if (IsRunning())
	{
		GetConnectionListener()->CloseAllConnectionsToSubServer(subserver);
	}
}

void 
WebServer::CloseAllConnections()
{
	if (IsRunning())
	{
		GetConnectionListener()->CloseAllConnections();
	}
}


UINT32 
WebServer::GetSubServerCount()
{
	OpAutoVector<WebSubServer> *subservers = NULL;
	if (IsRunning() && (subservers = GetConnectionListener()->GetWebSubServers()))
	{
		return subservers->GetCount();
	}
	return 0;
}


OP_STATUS 
WebServer::RemoveSubServer(const OpStringC &serviceVirtualPath)
{
	if (IsRunning())
	{
		return GetConnectionListener()->RemoveSubServer(GetSubServer(serviceVirtualPath));
	}
	else
	{
		return OpStatus::ERR;
	}
}

OP_STATUS 
WebServer::RemoveSubServer(WebSubServer* subserver)
{
	if (IsRunning())
	{
		return GetConnectionListener()->RemoveSubServer(subserver);
	}
	else
	{
		return OpStatus::ERR;
	}
}


OP_STATUS 
WebServer::SetRootSubServer(WebSubServer *root_service)
{
	if (IsRunning())
	{
		return GetConnectionListener()->SetRootSubServer(root_service);
	}
	else
	{
		return OpStatus::ERR;
	}
}

WebSubServer *WebServer::GetRootSubServer()
{
	if (IsRunning())
	{
		return GetConnectionListener()->GetRootSubServer();
	}
	else
	{
		return NULL;
	}	
}

#ifdef WEB_UPLOAD_SERVICE_LIST
OP_STATUS 
WebServer::StartServiceDiscovery(const OpStringC &central_server_uri, const OpStringC8 &token)
{
	if (!IsRunning())
	{
		OP_ASSERT(!"webserver must be running to before starting UploadServiceListManager");
		return OpStatus::ERR;
	}
	
	BOOL already_present=(m_upload_service_list_manager!=NULL);
	
	RETURN_IF_ERROR(m_upload_service_uri.Set(central_server_uri));
	RETURN_IF_ERROR(m_upload_service_token.Set(token));
	
	OP_DELETE(m_upload_service_list_manager);
	RETURN_IF_ERROR(UploadServiceListManager::Make(m_upload_service_list_manager,
	                                      m_upload_service_uri.CStr(), 
	                                      GetWebserverUri(), 
	                                      g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice).CStr(), 
	                                      g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser).CStr(), 
	                                      this
	                                      ));
	
	if(!already_present)
	{                                      
		// Create the _asd service, used to replicate the services in a _ASD like way
		OpString asd_service_storage_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, asd_service_storage_folder));
		RETURN_IF_ERROR(asd_service_storage_folder.Append("/resources/_asd_gadgets/"));
		
		WebSubServer *subServer_ASD=NULL;
		WebserverResourceDescriptor_ASD *rd_services=NULL;
		OP_STATUS ops;
		
		if(OpStatus::IsError(ops=WebSubServer::Make(subServer_ASD, this, NO_WINDOW_ID, asd_service_storage_folder, UNI_L("asdService"), UNI_L("_asd"), UNI_L("Opera Software ASA"), UNI_L("ASD Service"), UNI_L(""), FALSE, FALSE, FALSE)) ||
			OpStatus::IsError(ops=WebserverResourceDescriptor_ASD::Make(rd_services, UNI_L("services"))) ||
			OpStatus::IsError(ops=subServer_ASD->AddSubserverResource(rd_services)) ||
			OpStatus::IsError(ops=this->AddSubServer(subServer_ASD, FALSE)))
		{
			OP_DELETE(subServer_ASD);
			OP_DELETE(rd_services);
			
			// TODO: verify if returning an error really makes sense...
			return ops;
		}
		
		subServer_ASD->visible=FALSE;
	}
	
	return OpStatus::OK;
}

void WebServer::StopServiceDiscovery()
{
	OP_DELETE(m_upload_service_list_manager);
	m_upload_service_list_manager = NULL;
	
	OpString str;
	
	if(OpStatus::IsSuccess(str.Set("_asd")))
		OpStatus::Ignore(this->RemoveSubServer(str));
}

#endif //WEB_UPLOAD_SERVICE_LIST

const uni_char *WebServer::GetWebserverUriIdna()
{ 
	return m_webserver_server_name_idna.CStr();
}

const uni_char *WebServer::GetUserName()
{ 
	if (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
		return UNI_L("administrator");
	else
		return g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser).CStr();
}

const uni_char *WebServer::GetWebserverUri()
{ 
	return m_webserver_server_name.CStr();
}

// Log that a user has requested access to a resource
OP_STATUS WebServer::LogResourceAccess(WebServerConnection *conn, WebserverRequest *request, WebSubServer *sub_server, WebResource *resource)
{
	OP_ASSERT(conn && request && resource && sub_server);
	
	if(!conn || !request || !resource || !sub_server)
		return OpStatus::ERR_NULL_POINTER;
		
	OpString user;
	OpString user_ip;
	OpSocketAddress *addr;
	OP_STATUS ops;

	// Retrieve the IP address
	RETURN_IF_ERROR(OpSocketAddress::Create(&addr));
	
	OP_ASSERT(addr);
	if(!addr)
		return OpStatus::ERR_NO_MEMORY;
	
	if(OpStatus::IsSuccess(ops=conn->GetSocketAddress(addr)))
	{
		ops=addr->ToString(&user_ip);
	}
	
	OP_DELETE(addr);
	RETURN_IF_ERROR(ops);
	
	// If possible, get the user name
	BOOL auth=request->UserAuthenticated();
	
	if(auth)
		RETURN_IF_ERROR(user.Set(request->GetAuthenticatedUsername()));
	else
		RETURN_IF_ERROR(user.Set(user_ip.CStr()));
	
	return LogResourceAccessByUser(user, user_ip, auth, sub_server, resource);
}

OP_STATUS WebServer::LogResourceAccessByUser(const OpString &user, const OpString &user_ip, BOOL auth, WebSubServer *sub_server, WebResource *resource)
{
	OP_ASSERT(sub_server && resource);
	
	if(!sub_server || !resource)
		return OpStatus::ERR_NULL_POINTER;
		
	WebLogAccess *log;
	OP_STATUS ops;
	
	// Try to get the user from the name (if not auth, it is the IP)
	OpStatus::Ignore(log_users.GetData(user.CStr(), &log));
	
	if(log)
		log->IncAccesses();
	
	// If authenticated, try to get from the IP and change the user to the user_name
	if(auth && !log)
	{
		OpStatus::Ignore(log_users.GetData(user_ip.CStr(), &log));
		
		if(log)
		{
			WebLogAccess *old;
			BOOL is_auth;
			
			OpStatus::Ignore(log_users.Remove(log->GetUser(is_auth), &old));
			
			OP_ASSERT(old==log);
			
			if(old!=log)
				OP_DELETE(old); // It should never happen, but if it happens, we try not to leak memory
				
			OpStatus::Ignore(log->Construct(user, auth));
			if(OpStatus::IsError(ops=log_users.Add(log->GetUser(is_auth), log)))
			{
				OP_DELETE(log);
				
				return ops;
			}
			log->IncAccesses();
		}
	}
	
	// If not found, Create a new one
	if(!log)
	{
		log=OP_NEW(WebLogAccess, ());
		
		if(log && OpStatus::IsError(ops=log->Construct(user, auth)))
		{
			OP_DELETE(log);
			
			return ops;
		}
		
		if(!log)
			return OpStatus::ERR_NO_MEMORY;
		
		log->IncAccesses();
		
		BOOL is_auth;
	
		if(OpStatus::IsError(ops=log_users.Add(log->GetUser(is_auth), log)))
		{
			OP_DELETE(log);
			
			return ops;
		}
	}
	else
		log->UpdateTime();
	
	return OpStatus::OK;
}

UINT32 WebServer::GetLastUsersCount(UINT32 seconds)
{
	int num_users=0;
	
	OpHashIterator *iter=log_users.GetIterator();
	time_t threshold=WebLogAccess::GetSystemTime()-seconds;
	
	if(OpStatus::IsSuccess(iter->First()))
	{
		do
		{
			WebLogAccess *log=(WebLogAccess *)iter->GetData();
			
			if(log && log->GetAccessTime()>=threshold)
				num_users++;
		}
		while(OpStatus::IsSuccess(iter->Next()));
	}
	
	OP_DELETE(iter);
	
	return num_users;
}


OP_STATUS WebServer::SetDefaultHeader(const OpStringC16 &header_name, const OpStringC16 &header_value, BOOL append)
{
	DefaultHeader *def=OP_NEW(DefaultHeader, ());
	OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
	
	if(!def || !OpStatus::IsSuccess(ops=def->Construct(header_name, header_value))
	|| !OpStatus::IsSuccess(ops=m_default_headers.Add(def)))
	{
		OP_DELETE(def);
		
		return ops;
	}
	
	return OpStatus::OK;
}
	
OP_STATUS WebServer::SetDefaultResourceHeaders(WebResource *res)
{
	for(int i=0, len=m_default_headers.GetCount(); i<len; i++)
	{
		DefaultHeader *def=m_default_headers.Get(i);
		BOOL found;
		
		if(def)
		{
			RETURN_IF_ERROR(res->HasHeader(def->GetName(), found));
			
			if(!found)
				res->AddResponseHeader(def->GetName(), def->GetValue(), TRUE);  // Put in append just because we know that there is not such a header
		}
	}
	
	return OpStatus::OK;
}

BOOL WebServer::IsListening()
{
	WebServerConnectionListener *lsn=GetConnectionListener();
	
	if(!lsn)
		return FALSE;
		
	return lsn->IsListening();
}

OP_STATUS DefaultHeader::Construct(const OpStringC &header_name, const OpStringC &header_value)
{
	RETURN_IF_ERROR(name.Set(header_name));
	return value.Set(header_value);
}


#endif //WEBSERVER_SUPPORT 
