/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_MANAGER_H
#define OP_SCOPE_MANAGER_H

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/generated/g_scope_manager.h"
#include "modules/scope/src/scope_protocol_service.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_transcoder.h"
#include "modules/scope/src/scope_default_message.h"
#include "modules/scope/src/scope_network.h"
#include "modules/scope/scope_client.h"

#ifdef SCOPE_ACCEPT_CONNECTIONS
#include "modules/scope/src/scope_proxy.h"
#endif

#include "modules/dochand/docman.h"
#include "modules/doc/frm_doc.h"
#include "modules/scope/src/scope_idtable.h"

class OpScopeNetworkClient;  // private to the module

class OpScopeDefaultManager
	: public OpScopeHostManager
	, public OpScopeNetworkCallback
#ifdef SCOPE_ACCEPT_CONNECTIONS
	, public OpScopeNetworkServerCallback
#endif // SCOPE_ACCEPT_CONNECTIONS
	, public OpScopeServiceFactory
#ifdef SCOPE_MESSAGE_TRANSCODING
	, public OpScopeTranscodeManager
#endif // SCOPE_MESSAGE_TRANSCODING
{
public:
	OpScopeDefaultManager();
	virtual ~OpScopeDefaultManager();

	OP_STATUS Construct();

	/** @return TRUE if we're connected to a proxy, otherwise FALSE */
	BOOL IsConnected();

	/** Prevent scope from connecting to a proxy, and disconnect if already connected.
	 *  This is functionally the same as turning off all scope features, but without
	 *  the benefit of smaller footprint. Only use this function if you cannot decide
	 *  at compile time if you need scope or not. Default is ENABLED.
	 */
	void Disable();

	/** Enable scope to connect to a proxy. This is by default the case, so the
	 *  function is only needed if Disable() was called previously. Before starting to
	 *  use this function, check if it cannot be solved with message passing to scope
	 *  instead.
	 */
	void Enable();

	/// Called by services for silently ignored errors, so they can be noticed later on
	/// (e.g. when there is more memory available on the device)
	void CountSilentErrors(OP_STATUS status);

	/// Called by OpScopeWindowManager to notify a change in the filter
	/// This is then propagated to all services.
	void FilterChanged();

	/// Called by OpScopeWindowManager to notify all services that a Window
	/// was just closed.
	void WindowClosed(Window *window);

	/**
	 * Connects the client to either the builtin host or to a remote host.
	 * If @a port is set to 0 it attaches the client to the builtin host,
	 * otherwise it sets up a socket server and waits for an incoming socket
	 * connection. When a remote host is connected it connects the client to
	 * the remote host via the STP/1 protocol.
	 *
	 * @note The remote host must connect to the listening socket on the specified
	 *       port. This can be done by using opera:debug in the remote host.
	 */
	OP_STATUS AddClient(OpScopeClient *client, int port);
	/**
	 * Disconnects the client from the host it is currently connected to.
	 */
	OP_STATUS RemoveClient(OpScopeClient *client);

	/**
	 * @return The Descriptor object of the default/void message object.
	 */
	const OpProtobufMessage *GetDefaultMessageDescriptor();

private:
	BOOL enabled; //< Is scope allowed to make external connections. TRUE by default.
	unsigned errors;
#ifdef SCOPE_CONNECTION_CONTROL
	MH_PARAM_1 network_client_id; ///< Connection ID for the network client
#endif // SCOPE_CONNECTION_CONTROL

	/**
	 * Handles the following callback messages:
	 * - MSG_SCOPE_CREATE_CONNECTION
	 * - MSG_SCOPE_CLOSE_CONNECTION
	 *
	 * @see OpScopeManagerInterface::HandleCallback for additional messages
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// From OpScopeHostManager

	virtual void OnHostDestruction(OpScopeHost *host);
	virtual void OnClientDestruction(OpScopeClient *client);

	// Network events

	virtual void OnConnectionSuccess(OpScopeNetwork *net);
	virtual void OnConnectionFailure(OpScopeNetwork *net, OpSocket::Error error);
	virtual void OnConnectionClosed(OpScopeNetwork *net);
	virtual void OnConnectionLost(OpScopeNetwork *net);

#ifdef SCOPE_ACCEPT_CONNECTIONS
	// Network server events

	virtual void OnListeningSuccess(OpScopeNetworkServer *server);
	virtual void OnListeningFailure(OpScopeNetworkServer *server);
	virtual void OnConnectionRequest(OpScopeNetworkServer *server);
	virtual void OnListenerClosed(OpScopeNetworkServer *server);

	OP_STATUS CreateHostConnection(OpScopeNetworkServer *server);
#endif // SCOPE_ACCEPT_CONNECTIONS

	/**
	 * Handles shutdown of the network client by detaching it from the builtin
	 * host.
	 */
	void OnNetworkClientClosed();

#ifdef SCOPE_ACCEPT_CONNECTIONS
	/**
	 * Handles shutdown of the network host by detaching it from the client
	 * and deleting the object.
	 */
	void OnNetworkHostClosed();
#endif // SCOPE_ACCEPT_CONNECTIONS

	OpScopeNetworkClient *network_client;

#ifdef SCOPE_MESSAGE_TRANSCODING
	// From OpScopeTranscodeManager
	/**
	* @return The transcoder of the active network host or @c NULL if no network host is active.
	*/
	virtual OpScopeTranscoder *GetTranscoder() const { return network_host ? &(network_host->GetTranscoder()) : NULL; }
#endif // SCOPE_MESSAGE_TRANSCODING
public:
	// Should only be used by generated code
	OpScopeDefaultMessage default_message_instance;

#ifdef SCOPE_ACCEPT_CONNECTIONS
	/**
	 * @return The current socket server or @c NULL if there is no socket server listening on an address/port
	 */
	OpScopeNetworkServer *GetNetworkServer() const { return network_server;}
#endif

	/**
	 * Starts a webserver and configures it to serve requests for UPnP discovery.
	 */
	OP_STATUS StartUPnPService(int opendragonflyport);

	/**
	 * Stops the webserver serving UPnP discovery requests.
	 */
	OP_STATUS StopUPnPService();

private:
#ifdef SCOPE_ACCEPT_CONNECTIONS
	OpScopeNetworkHost *network_host;
	OpScopeNetworkServer *network_server;
	OpScopeClient *queued_client; //< The client which should be used for the next incoming connection
#endif
	OpScopeBuiltinHost builtin_host;

	OpProtobufMessage default_message_;
};

#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_MANAGER_H
