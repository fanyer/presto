/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/hardcore/mh/mh.h"
#include "modules/scope/scope_connection_manager.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_network.h"
#include "modules/scope/src/scope_service.h"

#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/webserver/webserver-api.h"

#include "modules/scope/src/scope_default_message.h"

#define DRAGONFLY_ICON_LOCATION UNI_L("http://dragonfly.opera.com/device-favicon.png")

/* OpScopeDefaultManager */

OpScopeDefaultManager::OpScopeDefaultManager()
	: enabled(TRUE)
	, errors(0)
#ifdef SCOPE_CONNECTION_CONTROL
	, network_client_id(0)
#endif // SCOPE_CONNECTION_CONTROL
	, network_client(NULL)
#ifdef SCOPE_ACCEPT_CONNECTIONS
	, network_host(NULL)
	, network_server(NULL)
	, queued_client(NULL)
#endif
	, default_message_(0, 0, 0 /*field count*/, NULL/*fields*/, NULL/*offsets*/, 0/*has bits*/,
					   OP_PROTO_OFFSETOF(OpScopeDefaultMessage, encoded_size_),
					   "Default",
					   OpProtobufMessageVector<OpScopeDefaultMessage>::Make,
					   OpProtobufMessageVector<OpScopeDefaultMessage>::Destroy)
{
}

OpScopeDefaultManager::~OpScopeDefaultManager()
{
#ifdef SCOPE_ACCEPT_CONNECTIONS
	if (queued_client)
	{
		if (queued_client->GetHost())
			queued_client->DetachHost();
		else
			queued_client->OnHostMissing();
	}
	if (network_server)
		OpStatus::Ignore(network_server->DetachListener(this)); // Avoid callbacks
	OP_DELETE(network_server);
	if (OpScopeNetworkHost *host = network_host)
	{
		OpStatus::Ignore(host->DetachListener(this)); // Avoid callbacks
		ReportDestruction(host);
		OP_DELETE(host);
	}
#endif // SCOPE_ACCEPT_CONNECTIONS

	// Network last, to allow the clients to deregister themselves
	if (OpScopeNetworkClient *client = network_client)
	{
		OpStatus::Ignore(client->DetachListener(this)); // Avoid callbacks
		ReportDestruction(client);
		OP_DELETE(client);
	}

	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_CREATE_CONNECTION);

#ifdef SCOPE_CONNECTION_CONTROL
	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_CLOSE_CONNECTION);
#endif // SCOPE_CONNECTION_CONTROL

#ifdef WEBSERVER_SUPPORT
	OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
}

OP_STATUS
OpScopeDefaultManager::Construct()
{
	RETURN_IF_ERROR(OpScopeHostManager::Construct());

	//
	// Register meta-services
	//

	OpString core_service_version;
	core_service_version.SetL(UNI_L("core"));
	RETURN_IF_ERROR(core_service_version.AppendFormat(UNI_L("-%d-%d"), CORE_VERSION_MAJOR, CORE_VERSION_MINOR));
	RETURN_IF_ERROR(RegisterMetaService(core_service_version));
	RETURN_IF_ERROR(RegisterMetaService(UNI_L("stp-0")));
	RETURN_IF_ERROR(RegisterMetaService(UNI_L("stp-1")));

	//
	// Network first, because it holds the listeners list used by the clients.
	// Don't connect if debugging is not enabled.
	//
	BOOL auto_disconnect = TRUE;
	network_client = OP_NEW_L(OpScopeNetworkClient, (OpScopeTPHeader::ProtocolBuffer, auto_disconnect));
	RAISE_IF_ERROR(network_client->Construct());
	network_client->SetHostManager(this);
	network_client->AttachListener(this);

#ifdef SCOPE_ACCEPT_CONNECTIONS
#endif // SCOPE_ACCEPT_CONNECTIONS

	g_main_message_handler->SetCallBack(this, MSG_SCOPE_CREATE_CONNECTION, 0);

#ifdef SCOPE_CONNECTION_CONTROL
	g_main_message_handler->SetCallBack(this, MSG_SCOPE_CLOSE_CONNECTION, 0);
#endif // SCOPE_CONNECTION_CONTROL

	//
	// Set ourselves up to possibly autoconnect to the network on Opera
	// startup. This allows the calling platform to set up the preferences
	// between Opera::InitL() and starting the message loop.
	//
	g_main_message_handler->PostDelayedMessage(MSG_SCOPE_CREATE_CONNECTION,
											   0, 0, 1000);

	//
	// Create descriptor objects, they are used by message classes in each service
	//

	RETURN_IF_ERROR(CreateServiceDescriptors());

	//
	// Create functional services and register them in the builtin host
	//
	RETURN_IF_ERROR(CreateServices(&this->builtin_host));

	return OpStatus::OK;
}

BOOL
OpScopeDefaultManager::IsConnected()
{
	return network_client != 0 && network_client->GetConnectionState() == OpScopeNetwork::STATE_CONNECTED;
}

void OpScopeDefaultManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1,
									MH_PARAM_2 par2)
{
	if (msg == MSG_SCOPE_CREATE_CONNECTION)
	{
		OpStringC ad;
		int pn;

		if ( network_client->GetConnectionState() != OpScopeNetwork::STATE_CLOSED || !enabled )
		{
#ifdef SCOPE_CONNECTION_CONTROL
			g_main_message_handler->PostMessage(MSG_SCOPE_CONNECTION_STATUS, par1,
											OpScopeConnectionManager::CONNECTION_FAILED,
											0);
#endif // SCOPE_CONNECTION_CONTROL
			return;
		}

		if (par2)
		{
			// Explicitly connect to the port given as a parameter to the
			// message.
			ad = UNI_L("127.0.0.1");
			pn = par2;
		}
		else if (!par1 && !g_pctools->GetIntegerPref(PrefsCollectionTools::ProxyAutoConnect))
			return; // Don't autoconnect.
		else
		{
			ad = g_pctools->GetStringPref(PrefsCollectionTools::ProxyHost).CStr();
			pn = g_pctools->GetIntegerPref(PrefsCollectionTools::ProxyPort);
		}
		if ( ad.Compare("127.0.0.1") == 0 || par1 )
		{
#ifdef SCOPE_CONNECTION_CONTROL
			network_client_id = par1;
#endif // SCOPE_CONNECTION_CONTROL

			OpStatus::Ignore(network_client->Connect(ad, pn));
		}
	}
#ifdef SCOPE_CONNECTION_CONTROL
	else if ( msg == MSG_SCOPE_CLOSE_CONNECTION )
	{
		network_client->Disconnect();
	}
#endif // SCOPE_CONNECTION_CONTROL
	else
		OpScopeHostManager::HandleCallback(msg, par1, par2);
}

/*virtual*/
void
OpScopeDefaultManager::OnConnectionSuccess(OpScopeNetwork *net)
{
	OP_ASSERT(net);

	if (net == network_client)
	{
		builtin_host.AttachClient(network_client);

#ifdef SCOPE_CONNECTION_CONTROL
		g_main_message_handler->PostMessage(MSG_SCOPE_CONNECTION_STATUS, network_client_id,
			OpScopeConnectionManager::CONNECTION_OK, 0);
#endif // SCOPE_CONNECTION_CONTROL
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
}

/*virtual*/
void
OpScopeDefaultManager::OnConnectionFailure(OpScopeNetwork *net, OpSocket::Error error)
{
	OP_ASSERT(net);

	if (net == network_client)
	{
#ifdef SCOPE_CONNECTION_CONTROL
		g_main_message_handler->PostMessage(MSG_SCOPE_CONNECTION_STATUS, network_client_id,
			OpScopeConnectionManager::CONNECTION_FAILED, 0);

		network_client_id = 0;
#endif // SCOPE_CONNECTION_CONTROL

		OnNetworkClientClosed();
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
}

/*virtual*/
void
OpScopeDefaultManager::OnConnectionClosed(OpScopeNetwork *net)
{
	OP_ASSERT(net);

	if (net == network_client)
	{
#ifdef SCOPE_CONNECTION_CONTROL
		g_main_message_handler->PostMessage(MSG_SCOPE_CONNECTION_STATUS, network_client_id,
			OpScopeConnectionManager::DISCONNECTION_OK, 0);

		network_client_id = 0;
#endif

		OnNetworkClientClosed();
	}

#ifdef SCOPE_ACCEPT_CONNECTIONS
	if (net == network_host)
	{
		OnNetworkHostClosed();
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
#endif // SCOPE_ACCEPT_CONNECTIONS
}

/*virtual*/
void
OpScopeDefaultManager::OnConnectionLost(OpScopeNetwork *net)
{
	OP_ASSERT(net);

	if (net == network_client)
	{
#ifdef SCOPE_CONNECTION_CONTROL
		g_main_message_handler->PostMessage(MSG_SCOPE_CONNECTION_STATUS, network_client_id,
			OpScopeConnectionManager::CONNECTION_FAILED, 0);
#endif // SCOPE_CONNECTION_CONTROL

		OnNetworkClientClosed();
	}

#ifdef SCOPE_ACCEPT_CONNECTIONS
	if (net == network_host)
		OnNetworkHostClosed();
#endif // SCOPE_ACCEPT_CONNECTIONS
}

void
OpScopeDefaultManager::OnNetworkClientClosed()
{
	if (network_client->GetHost())
		network_client->DetachHost();
}

#ifdef SCOPE_ACCEPT_CONNECTIONS
void
OpScopeDefaultManager::OnNetworkHostClosed()
{
	if (OpScopeNetworkHost *host = network_host)
	{
		network_host = NULL;
		ScheduleDestruction(host);
	}
}
#endif // SCOPE_ACCEPT_CONNECTIONS

void
OpScopeDefaultManager::CountSilentErrors(OP_STATUS status)
{
	if (OpStatus::IsError(status))
		errors++;
}

void
OpScopeDefaultManager::FilterChanged()
{
	OpScopeServiceManager::ServiceRange range(builtin_host.GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		range.Front()->FilterChanged();
	}
}

void
OpScopeDefaultManager::WindowClosed(Window *window)
{
	OpScopeServiceManager::ServiceRange range(builtin_host.GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		range.Front()->WindowClosed(window);
	}
}

void
OpScopeDefaultManager::Disable()
{
	enabled = FALSE;
	network_client->Disconnect();
}

void
OpScopeDefaultManager::Enable()
{
	enabled = TRUE;
}

/*virtual*/
void
OpScopeDefaultManager::OnHostDestruction(OpScopeHost *host)
{
#ifdef SCOPE_ACCEPT_CONNECTIONS
	if (host == network_host)
		network_host = NULL;
#endif // SCOPE_ACCEPT_CONNECTIONS
}

/*virtual*/
void
OpScopeDefaultManager::OnClientDestruction(OpScopeClient *client)
{
	if (client == network_client)
		network_client = NULL;
#ifdef SCOPE_ACCEPT_CONNECTIONS
	else if (client == queued_client)
	{
		queued_client = NULL;
	}
#endif // SCOPE_ACCEPT_CONNECTIONS
}

#ifdef SCOPE_ACCEPT_CONNECTIONS
void
OpScopeDefaultManager::OnListeningSuccess(OpScopeNetworkServer *server)
{
	if (server == network_server)
	{
#ifdef WEBSERVER_SUPPORT
		UINT port = server->GetPort();
		if (port != 0)
			OpStatus::Ignore(StartUPnPService(port));
#endif //WEBSERVER_SUPPORT
	}
}

void
OpScopeDefaultManager::OnListeningFailure(OpScopeNetworkServer *server)
{
	if (server == network_server)
	{
		if (queued_client)
		{
			if (queued_client->GetHost())
				queued_client->DetachHost();
			else
				queued_client->OnHostMissing();
			queued_client = NULL;
		}
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
}

void
OpScopeDefaultManager::OnListenerClosed(OpScopeNetworkServer *server)
{
	if (server == network_server)
	{
		if (queued_client)
		{
			if (queued_client->GetHost())
				queued_client->DetachHost();
			else
				queued_client->OnHostMissing();
			queued_client = NULL;
		}
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
}

void
OpScopeDefaultManager::OnConnectionRequest(OpScopeNetworkServer *server)
{
	OP_ASSERT(server);

	if (queued_client)
	{
		if (OpStatus::IsError(CreateHostConnection(server)))
		{
			if (OpScopeNetworkHost *host = network_host)
			{
				network_host = NULL;
				ScheduleDestruction(host);
#ifdef WEBSERVER_SUPPORT
				OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
			}
		}
	}
	else
	{
		// No clients waiting, disconnect
		network_server->Disconnect();
#ifdef WEBSERVER_SUPPORT
		OpStatus::Ignore(StopUPnPService());
#endif //WEBSERVER_SUPPORT
	}
}

OP_STATUS
OpScopeDefaultManager::CreateHostConnection(OpScopeNetworkServer *server)
{
	OP_ASSERT(queued_client);

	// Initiate a new network host connection and attach it to the client

	int desired_stp_version = -1;
	BOOL auto_disconnect = TRUE;
	network_host = OP_NEW(OpScopeNetworkHost, (desired_stp_version, auto_disconnect));
	RETURN_OOM_IF_NULL(network_host);
	RETURN_IF_ERROR(network_host->Construct());

	network_host->SetHostManager(this);
	RETURN_IF_ERROR(network_host->AttachListener(this));
#ifdef SCOPE_MESSAGE_TRANSCODING
	network_host->GetTranscoder().SetServiceManager(&builtin_host);
	RETURN_IF_ERROR(OnNewTranscoder());
#endif // SCOPE_MESSAGE_TRANSCODING

	RETURN_IF_ERROR(network_host->AttachClient(queued_client));
	queued_client = NULL;

	RETURN_IF_ERROR(server->Accept(network_host));

	// Close down the listening socket to avoid additional requests to connect
	// It will be re-initialized when a new client is created in AddClient().
	server->Disconnect();

	return OpStatus::OK;
}
#endif // SCOPE_ACCEPT_CONNECTIONS

OP_STATUS
OpScopeDefaultManager::AddClient(OpScopeClient *client, int port)
{
	client->SetHostManager(this);

	if (port == 0)
	{
		// This will attach the client to the host, the host will then initiate the connection by sending the service-list
		RETURN_IF_ERROR(builtin_host.AttachClient(client));
	}
	else
	{
#ifdef SCOPE_ACCEPT_CONNECTIONS
		if (queued_client)
		{
			if (queued_client->GetHost())
				queued_client->DetachHost();
			else
				queued_client->OnHostMissing();
		}
		// Store the client to use when a host is connected, see CreateHostConnection for details
		queued_client = client;

		if (network_server == NULL)
		{
			network_server = OP_NEW(OpScopeNetworkServer, ());
			RETURN_OOM_IF_NULL(network_server);
			network_server->AttachListener(this);
			RETURN_IF_ERROR(network_server->Construct());
		}

		if (!network_server->IsListening())
		{
			RETURN_IF_ERROR(network_server->Listen(port));
		}
#endif // SCOPE_ACCEPT_CONNECTIONS

	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeDefaultManager::RemoveClient(OpScopeClient *client)
{
	if (client)
	{
#ifdef SCOPE_ACCEPT_CONNECTIONS
		if (queued_client == client)
		{
			queued_client = NULL;
			network_server->Disconnect();
		}
#endif // SCOPE_ACCEPT_CONNECTIONS
		client->SetHostManager(NULL);

		// This detaches the host from the client. If the host was the network
		// host connection (network_host) then it will also be deleted in
		// OnNetworkHostClosed().
		if (client->GetHost())
			client->DetachHost();
	}

	return OpStatus::OK;
}

const OpProtobufMessage *
OpScopeDefaultManager::GetDefaultMessageDescriptor()
{
	if (!default_message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
	{
		default_message_.SetIsInitialized(TRUE);
	}
	return &default_message_;
}

#ifdef WEBSERVER_SUPPORT
OP_STATUS
OpScopeDefaultManager::StartUPnPService(int opendragonflyport)
{
	if (g_upnp)	// no use in setting up discovery if no upnp object
	{
		if (!g_dragonflywebserver->IsRunning())
		{
			WebserverListeningMode mode = WEBSERVER_LISTEN_DEFAULT;
			mode |= WEBSERVER_LISTEN_UPNP_DISCOVERY;
			mode |= WEBSERVER_LISTEN_DIRECT;
			unsigned int webport = g_pctools->GetIntegerPref(PrefsCollectionTools::UPnPWebserverPort);

			OpString ip;
			RETURN_IF_ERROR(g_op_system_info->GetSystemIp(ip));

			OpString dragonflyaddress;
			RETURN_IF_ERROR(dragonflyaddress.AppendFormat(UNI_L("http://%s:%d"), ip.CStr(), opendragonflyport));

			g_dragonflywebserver->SetAlwaysListenToAllNetworks(TRUE);
			RETURN_IF_ERROR(g_dragonflywebserver->StartUPnPServer(mode, webport, UPNP_DISCOVERY_DRAGONFLY_SEARCH_TYPE, DRAGONFLY_ICON_LOCATION, dragonflyaddress.CStr(), g_upnp));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeDefaultManager::StopUPnPService()
{
	if (!g_dragonflywebserver)
		return OpStatus::ERR_NULL_POINTER;

	UPnPServicesProvider* upnpserviceprovider = g_dragonflywebserver->GetUPnPServicesProvider();
	if (upnpserviceprovider)
	{
		g_upnp->SendByeBye(upnpserviceprovider);
		upnpserviceprovider->SetDisabled(TRUE);
	}

	return g_dragonflywebserver->Stop();
}
#endif //WEBSERVER_SUPPORT

/* OpScopeServiceFactory */

OpScopeServiceFactory::~OpScopeServiceFactory()
{
	DeleteServices();
	OP_DELETE(descriptors);
}

OpScopeDescriptorSet &
OpScopeServiceFactory::GetDescriptorSet()
{
	OP_ASSERT(descriptors);
	return *descriptors;
}

OP_STATUS
OpScopeServiceFactory::RegisterMetaService(const uni_char *name)
{
	for (MetaService *meta_service = meta_services.First(); meta_service; meta_service = meta_service->Suc())
	{
		if (meta_service->name.Compare(name) == 0)
			return OpStatus::ERR;
	}
	OpAutoPtr<MetaService> service(OP_NEW(MetaService, ()));
	RETURN_OOM_IF_NULL(service.get());
	RETURN_IF_ERROR(service->name.Set(name));
	service->Into(&meta_services);
	service.release();
	return OpStatus::OK;
}

OP_STATUS
OpScopeServiceFactory::CreateServiceDescriptors()
{
	descriptors = OP_NEW(OpScopeDescriptorSet, ());
	RETURN_OOM_IF_NULL(descriptors);
	return descriptors->Construct();
}

OP_STATUS
OpScopeServiceFactory::InitializeShared()
{
	return OpStatus::OK;
}

void
OpScopeServiceFactory::CleanupShared()
{
}

#endif // SCOPE_SUPPORT
