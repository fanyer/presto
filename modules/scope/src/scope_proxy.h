/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff / Jan Borsodi
*/

#ifdef SCOPE_ACCEPT_CONNECTIONS

#ifndef OP_SCOPE_PROXY_H
#define OP_SCOPE_PROXY_H

#include "modules/scope/src/scope_network.h"
#include "modules/util/adt/opvector.h"
#include "modules/scope/scope_internal_proxy.h"
#include "modules/scope/src/scope_transcoder.h"

class ClientConnection;
class HostConnection;
class OpScopeTPMessage;
class OpProtobufMessage;
class OpProtobufField;
class OpScopeNetworkHost;
class OpScopeNetworkServer;

#ifndef SOCKET_LISTEN_SUPPORT
#error "Need SOCKET_LISTEN_SUPPORT"
#endif

/**
* Callback structure for OpScopeNetworkServer objects.
*/
class OpScopeNetworkServerCallback
{
public:
	/**
	 * Sent when the network server was successful in setting up a listening
	 * socket.
	 *
	 * @param net The network server which was successful.
	 */
	virtual void OnListeningSuccess(OpScopeNetworkServer *server) = 0;

	/**
	 * Sent when the network server failed to setup a listening socket.
	 * address/port specified in OpScopeNetwork::Connect.
	 *
	 * @param net The network server which failed.
	 */
	virtual void OnListeningFailure(OpScopeNetworkServer *server) = 0;

	/**
	 * Sent when a new socket connection was made to the network server.
	 *
	 * @param net The network server which received a connection.
	 */
	virtual void OnConnectionRequest(OpScopeNetworkServer *server) = 0;

	/**
	* Sent when the network server stops listening for incoming connections.
	*
	* @param net The network server which closed down.
	*/
	virtual void OnListenerClosed(OpScopeNetworkServer *server) = 0;
};

/**
 * This class is responsible for accepting connections from a remote host.
 * It acts as a socket server by listening on a given address/port until
 * a socket is connected.
 *
 * To start listening to an address/port call one of the Listen() methods.
 * To stop listening call Disconnect().
 *
 * To receive notifications the caller should inherit OpScopeNetworkServerCallback
 * and call AttachListener() to register itself as a listener.
 *
 * When a connection is made it sends the notification event
 * OpScopeNetworkServerCallback::OnConnectionRequest.
 * The listener will also receive other events such as:
 * - OpScopeNetworkServerCallback::OnListeningSuccess
 * - OpScopeNetworkServerCallback::OnListeningFailure
 * - OpScopeNetworkServerCallback::OnListenerClosed
 *
 * Once a connection has been received the receiver should call Accept()
 * to initialize the OpScopeNetwork object with the connection.
 */
class OpScopeNetworkServer
	: public OpSocketListener
	, public OpListenable<OpScopeNetworkServerCallback>
{
public:
	enum State
	{
		/**
		 * Server is not listening for incoming connections.
		 */
		STATE_IDLE,
		/**
		 * Server is listening for incoming connections.
		 */
		STATE_LISTENING
	};
	OpScopeNetworkServer();
	~OpScopeNetworkServer();

	/**
	 * Second stage constructor.
	 */
	OP_STATUS Construct();

	/**
	 * Listen for incoming socket connections.
	 * @note It will Disconnect() the current socket if it is already listening before continuing.
	 *
	 * @param address The address to listen for incoming connections.
	 * @param port to listen to (0 means negotiate a port automatically)
	 * @return ERR if already listening
	 */
	OP_STATUS Listen(OpStringC &address, int port);

	/**
	 * Listen for incoming socket connections on a specified port. The address
	 * will be automatically decided, either it uses @c "0.0.0.0" for a port
	 * value higher than 0, for 0 it uses @c "127.0.0.1".
	 * @note It will Disconnect() the current socket if it is already listening before continuing.
	 *
	 * If the listener was successfully setup the notification event
	 * OpScopeNetworkServerCallback::OnListeningSuccess will be called,
	 * otherwise it calls OpScopeNetworkServerCallback::OnListeningFailure.
	 *
	 * @param port to listen to (0 means negotiate a port automatically)
	 * @return ERR if already listening
	 */
	OP_STATUS Listen(int port);

	/**
	 * Stops listening for incoming connections. The socket used for listening
	 * will be closed and deleted.
	 *
	 * If the listener was successfully setup the notification event
	 * OpScopeNetworkServerCallback::OnListeningSuccess will be called,
	 * otherwise it calls OpScopeNetworkServerCallback::OnListeningFailure.
	 *
	 * If the server was listening then it will call the notification event
	 * OpScopeNetworkServerCallback::OnListenerClosed.
	 */
	void Disconnect();

	/**
	 * Accept the current incoming connection and initialize the network object
	 * with it.
	 * @note This should only be called after a callback from
	 * OpScopeNetworkServerCallback::OnConnectionRequest.
	 *
	 * @return OK if the network object was initialized, ERR if something failed.
	 */
	OP_STATUS Accept(OpScopeNetwork *network);

	/**
	 * Query listener status
	 *
	 * This method can be used to determine if the socket listener is
	 * active or not.
	 */
	State GetListenerState() const;

	/**
	 * @return @c TRUE if server is listening for incoming connections.
	 */
	BOOL IsListening() const { return is_listening; }

	/**
	 * @return The address used for the listening socket, e.g. @c "127.0.0.1" or @c "0.0.0.0".
	 */
	const OpString &GetAddress() const { return listen_address; }

	/**
	 * @return The port used for the listening socket.
	 */
	UINT GetPort() const { return listen_port; }

	/**
	 * Check the result of the previous Listen() call. If the call failed
	 * because the address/port is already in use then this method will return
	 * @c TRUE.
	 * @return @c TRUE if the address is currently in use, @c FALSE otherwise.
	 */
	BOOL IsAddressInUse() const { return is_address_in_use; }

private:
	/**
	 * Socket which is listening to incoming connections or @c NULL if it is
	 * not listening.
	 */
	OpSocket *listening_socket;
	/**
	 * The address of the remote socket
	 */
	OpSocketAddress *sockaddr;
	/**
	 * Set to @TRUE while a listening socket is active, @c FALSE otherwise.
	 */
	BOOL is_listening;
	/**
	 * Used for port number negotiation.
	 */
	BOOL is_address_in_use;
	/**
	 * Network addresss we are listening on.
	 */
	OpString listen_address;
	/**
	 * Port we are listening to (@c 0 if not listening).
	 */
	UINT listen_port;

	OP_STATUS SetupListener();
	void ResetSocket();

	// From OpSocket

	/* virtual */ void OnSocketConnected(OpSocket*) {}
	/* virtual */ void OnSocketClosed(OpSocket* closing_socket);
	/* virtual */ void OnSocketDataReady(OpSocket*) {}
	/* virtual */ void OnSocketDataSent(OpSocket*, unsigned int) {}
#ifdef SOCKET_LISTEN_SUPPORT
	/* virtual */ void OnSocketConnectionRequest(OpSocket* socket);
	/* virtual */ void OnSocketListenError(OpSocket* socket, OpSocket::Error error);
#endif
	/* virtual */ void OnSocketConnectError(OpSocket*, OpSocket::Error) {}
	/* virtual */ void OnSocketReceiveError(OpSocket*, OpSocket::Error) {}
	/* virtual */ void OnSocketSendError(OpSocket*, OpSocket::Error) {}
	/* virtual */ void OnSocketCloseError(OpSocket*, OpSocket::Error) {}
	/* virtual */ void OnSocketDataSendProgressUpdate(OpSocket*, UINT) {}

protected:
	// Listener events
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkServerCallback, ListeningSuccess, OpScopeNetworkServer *);
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkServerCallback, ListeningFailure, OpScopeNetworkServer *);
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkServerCallback, ConnectionRequest, OpScopeNetworkServer *);
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkServerCallback, ListenerClosed, OpScopeNetworkServer *);
};

/**
 * This class is responsible for maintaining a connection to a connected
 * (incoming) host. It acts as a scope host relaying any messages from a
 * client to the remote host, and receiving remote message and sending them
 * to the client.
 */
class OpScopeNetworkHost
	: public OpScopeNetwork
	, public OpScopeHost
#ifdef SCOPE_MESSAGE_TRANSCODING
	, public OpScopeTranscoderHost
#endif // SCOPE_MESSAGE_TRANSCODING
{
public:
	OpScopeNetworkHost(int desired_stp_version = -1, BOOL auto_disconnect = FALSE, OpScopeHostManager *host_manager = NULL);
	~OpScopeNetworkHost();
	OP_STATUS Construct();

	// From OpScopeNetwork
	virtual OP_STATUS OnMessageParsed(const OpScopeTPMessage &message);
	virtual void OnError(OpScopeTPReader::Error error);

	// From OpScopeHost
	virtual const uni_char *GetType() const;
	virtual int GetVersion() const;
	virtual OP_STATUS Receive( OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &message );
	virtual OP_STATUS ConfigureClient( OpScopeClient *client, unsigned int tag, BOOL &immediate);

	virtual void Disconnect();

	// From OpScopeHost

	virtual OP_STATUS OnClientAttached(OpScopeClient *client);
	virtual OP_STATUS OnClientDetached(OpScopeClient *client);

#ifdef SCOPE_MESSAGE_TRANSCODING
	OpScopeTranscoder &GetTranscoder() { return transcoder; }

	// From OpScopeTranscoderHost
	virtual OP_STATUS SendMessage(OpAutoPtr<OpScopeTPMessage> &msg) { return EnqueueMessage(msg); }
	virtual unsigned GetSTPVersion() const { return state == State_Stp1 ? 1 : 0; }
#endif // SCOPE_MESSAGE_TRANSCODING

private:
	BOOL auto_disconnect; ///< If @c TRUE then the host connection should disconnect when a client is detached, default is @c FALSE

	enum State
	{
		  State_None
		, State_Handshake
		, State_HandshakeResponse
		, State_HandshakeVersion
		, State_Stp0
		, State_Stp1
	};
	enum SetupState
	{
		  SetupState_Waiting //< Initial state for setup phase
		, SetupState_Transcoder //< Runs steps in the transcoder
		, SetupState_SendServiceList //< Sending of service list to client, this activates the client
		, SetupState_Done //< Setup phase is complete
	};
	State state;
	/**
	 * Sub-state for State_Stp1, controls the setup phase which occurs
	 * after stp/1 is enabled but before the client is notified.
	 */
	SetupState setup_state;
	int   desired_stp_version;
	OpAutoPtr<OpScopeTPMessage> service_message;
	ByteBuffer incoming;

#  ifdef SCOPE_MESSAGE_TRANSCODING
	OpScopeTranscoder transcoder;
#  endif // SCOPE_MESSAGE_TRANSCODING

	void ResetState();
	virtual void Reset();

	OP_STATUS InitiateHandshake(unsigned int ver);
	virtual OP_STATUS OnHandshakeCompleted(unsigned int version);
	OP_STATUS ProcessSetup(const OpScopeTPMessage *message);
	BOOL ParseVersion(char terminator, unsigned int &version);

	// From OpScopeNetwork

	virtual OP_STATUS InitializeConnection();

	// From OpSocket

	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketClosed(OpSocket* closing_socket);
};

#endif // OP_SCOPE_PROXY_H

#endif // SCOPE_ACCEPT_CONNECTIONS
