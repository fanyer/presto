/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff / Jan Borsodi
*/

/** \file
 *
 * \brief Declare network handeling code
 *
 * This file declares the classes associated with handling the network
 * connection used by scope.
 */

#ifndef OP_SCOPE_NETWORK_H
#define OP_SCOPE_NETWORK_H

#ifdef SCOPE_SUPPORT

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"

#include "modules/scope/src/scope_tp_writer.h"
#include "modules/scope/src/scope_tp_reader.h"
#include "modules/scope/src/scope_transport.h"

#include "modules/device_api/OpListenable.h"

class OpScopeService;
class OpScopeNetwork;

/**
 * Callback structure for OpScopeNetwork objects.
 */
class OpScopeNetworkCallback
{
public:
	/**
	 * Sent when the network object was successfully connected. This is either
	 * a result of OpScopeNetwork::Connect or OpScopeNetworkHost::Listen
	 *
	 * @param net The network object that was connected.
	 */
	virtual void OnConnectionSuccess(OpScopeNetwork *net) = 0;

	/**
	 * Sent when the network object failed to connect to the
	 * address/port specified in OpScopeNetwork::Connect.
	 *
	 * @param net The network object which failed to connect.
	 * @param error The socket error.
	 */
	virtual void OnConnectionFailure(OpScopeNetwork *net, OpSocket::Error error) = 0;

	/**
	 * Sent when the network object connection was closed.
	 *
	 * @param net The network object that was disconnected.
	 */
	virtual void OnConnectionClosed(OpScopeNetwork *net) = 0;

	/**
	 * Sent when the network object connection was abruptly closed, for instance
	 * if the remote connection closed the socket connection.
	 *
	 * @param net The network object that was disconnected.
	 */
	virtual void OnConnectionLost(OpScopeNetwork *net) = 0;
};

/**
 * @brief Class for communicating over the scope protocol on a network socket.
 *
 * The @c OpScopeNetwork class communicates using the scope protocol,
 * described in <a href="transport-protocol.html">transport-protocol.html</a>.
 * The class takes care of the basic network communication and should be sub-classed
 * to handle the specific communication flow. See either OpScopeNetworkClient
 * or OpScopeNetworkHost.
 *
 * This class reads STP/1 data from the socket using OpScopeTPReader and
 * writes STP/1 data using OpScopeTPWriter.
 *
 * The class also provides a set of callbacks whenever a socket is connected
 * or closes. A receiver of the callback must inherit OpScopeNetworkCallback
 * and implement the virtual methods. Then attach the listener to this
 * network object with AttachListener().
 *
 * Start the connection by calling Connection() with a specified address or
 * an already open socket.
 * Once the connection is made it will write enqueued message to the socket
 * or parse any incoming data. It is possible to control the flow of messages
 * by calling SetWriterEnabled() and SetReaderEnabled().
 * For instance to stop parsing incoming messages call:
 * @code
 * SetReaderEnabled(FALSE);
 * @endcode
 * Incoming data will now be enqueued in a buffer but no parsing will happen.
 * Once the reader is enabled it will continue parsing messages.
 *
 * The same goes for outgoing messages, by calling:
 * @code
 * SetWriterEnabled(FALSE);
 * @endcode
 * the writer will stop processing the outgoing queue, any new messages sent
 * with EnqueueMessage() will stay in the queue until the writer is enabled
 * again.
 *
 * To end the network connection call Disconnect().
 */
class OpScopeNetwork
	: public OpSocketListener
	, protected OpScopeTPWriter
	, protected OpScopeTPReader
	, public MessageObject
	, public OpListenable<OpScopeNetworkCallback>
{
public:
	enum ConnectionState
	{
		/**
		 * Socket is currently closed.
		 */
		STATE_CLOSED,
		/**
		 * Socket is currently connecting.
		 */
		STATE_CONNECTING,
		/**
		 * Socket is connected.
		 */
		STATE_CONNECTED
	};

	OpScopeNetwork();
	/**
	 * If the object is connected to a network socket it will disconnect
	 * the socket and call OpScopeNetworkCallback::OnConnectionClosed.
	 */
	virtual ~OpScopeNetwork();

	/**
	 * Second stage constructor. Passes @a buffer to OpScopeTPReader.
	 * @param buffer The shared buffer which the reader will use to place
	 *               incoming network data in.
	 */
	OP_STATUS Construct(ByteBuffer *buffer);

	/**
	 * Connect to the given address and port.
	 * If it is already connected to a socket it will first call Disconnect()
	 * before connecting to the new address.
	 *
	 * Initiate a TCP/IP connect request to the given address and port.
	 * The address must be in numeric format.
	 *
	 * \param addr The numeric IP address to connect to (e.g. @c "127.0.0.1")
	 * \param port The port number to connect to, e.g. @c 7001
	 * \return A status value signalling success or failure
	 */
	OP_STATUS Connect(OpStringC &addr, int port);

	/**
	 * Connect to an already open socket and initialize the network object.
	 * If it is already connected to a socket it will first call Disconnect()
	 * before connecting to the new socket.
	 *
	 * @return OK for success, ERR if initialization failed.
	 */
	OP_STATUS Connect(OpSocket *new_socket);

	/**
	 * Disconnect the connection to the socket and reset the STP reader/writer,
	 * then call OpScopeNetworkCallback::OnConnectionClosed.
	 * If it was not connected to a socket no callback happens.
	 */
	virtual void Disconnect();

	/**
	 * Query connection status.
	 *
	 * This method can be used to determine if the scope connection is
	 * successfully connected or not.
	 *
	 * @return The connection state enum.
	 */
	ConnectionState GetConnectionState() const;

	/**
	 * Enqueue an STP message to be sent over the STP/1 network protocol.
	 * If the network object is not yet connected or the writer has been
	 * disabled the message is placed in a queue, otherwise the message
	 * will be sent immediately.
	 */
	OP_STATUS EnqueueMessage(OpAutoPtr<OpScopeTPMessage> &msg);

	/**
	 * @return the STP version used in the protocol stream, e.g. 0 for STP/0
	 *         or 1 for STP/1.
	 */
	unsigned int ProtocolVersion() const;
	/**
	 * Sets the STP version to use in the protocol stream.
	 *
	 * @param ver Version number to use, e.g. 0 for STP/0 or 1 for STP/1.
	 * @param force If @c TRUE then the version is switched immediately, otherwise it will wait until the next message is handled.
	 */
	void SetProtocolVersion(unsigned int ver, BOOL force);

	/**
	 * Sets whether the incoming protocol stream is active or not. If disabled
	 * then there is no processing of incoming messages, new network data will
	 * instead be queued in a buffer until it is enabled again.
	 */
	OP_STATUS SetReaderEnabled(BOOL enabled);

	/**
	 * Sets whether the outgoing protocol stream is active or not. If disabled
	 * then there is no processing of outgoing messages. The outgoing queue
	 * will be processed once the writer is enabled again.
	 */
	OP_STATUS SetWriterEnabled(BOOL enabled);

	/**
	 * @return The address used for the socket, e.g. @c "127.0.0.1" or @c "10.0.2.1".
	 */
	const OpString &GetAddress() const { return socket_address; }

	/**
	 * @return The port used for the socket.
	 */
	UINT GetPort() const { return socket_port; }

protected:
	/* virtual */ void OnSocketConnected(OpSocket* socket);
	/* virtual */ void OnSocketClosed(OpSocket* socket);
	/* virtual */ void OnSocketDataReady(OpSocket* socket);
	/* virtual */ void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent);
#ifdef SOCKET_LISTEN_SUPPORT
	/* virtual */ void OnSocketConnectionRequest(OpSocket* socket);
	/* virtual */ void OnSocketListenError(OpSocket* socket, OpSocket::Error error);
#endif
	/* virtual */ void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	/* virtual */ void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	/* virtual */ void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	/* virtual */ void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	/* virtual */ void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);

protected:
	OP_STATUS OnDataReady();
	OP_STATUS EnqueueError(const OpScopeTPMessage &message, const OpScopeTPError &error);

	// For sub-classes to implement

	/**
	 * Called when the socket has been connected and the network object needs
	 * initialize internal states etc. The class must ensure that it is in
	 * a suitable state after this method returns, as it may be start to
	 * receive data right afterwards.
	 */
	virtual OP_STATUS InitializeConnection() = 0;

	// From OpScopeTPWriter
	virtual OP_STATUS SendData(const char *data, size_t len);
	virtual OP_STATUS OnMessageSent(const OpScopeTPMessage &message);

	/**
	 * Reset internal state and delete socket.
	 * Sub-classes should re-implement this method and cleanup their own
	 * state then call this method.
	 *
	 * @code
	 * void
	 * MyNetwork::Reset()
	 * {
	 *    state = NONE;
	 *    OpScopeNework::Reset();
	 * }
	 * @endcode
	 */
	virtual void Reset();
	/**
	 * Reset the state of the STP writer.
	 */
	OP_STATUS ResetWriter();
	/**
	 * Reset the state of the STP reader.
	 */
	OP_STATUS ResetReader();

	// Listener events
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkCallback, ConnectionSuccess, OpScopeNetwork *);
	DECLARE_LISTENABLE_EVENT2(OpScopeNetworkCallback, ConnectionFailure, OpScopeNetwork *, OpSocket::Error);
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkCallback, ConnectionClosed, OpScopeNetwork *);
	DECLARE_LISTENABLE_EVENT1(OpScopeNetworkCallback, ConnectionLost, OpScopeNetwork *);

	OpSocket *GetSocket() const { return socket; }

private:
	// Network state

	/**
	 * The network socket used.
	 *
	 * When NULL, there is no established connection.  When it is not NULL,
	 * a connection is established and running.
	 */
	OpSocket *socket;

	/**
	 * The address of the remote socket
	 */
	OpSocketAddress *sockaddr;

	BOOL is_connected; //< Is the socket connected?
	BOOL in_destruct; //< Is the network object being destructed

	OpString socket_address;
	UINT socket_port;

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/**<
	 * Handles processing of ProcessMessage when send via a message
	 */

	/**
	 * Reset socket state and delete socket and address object.
	 */
	void ResetSocket();
	/**
	 * Reset the STP reader and writer.
	 */
	void ResetSTP();

	/**
	 * Contains information about network data that is being sent.
	 * It contains the socket and number of bytes that was sent and is used
	 * with the MSG_SCOPE_PROCESS_MESSAGE callback system to ensure that the
	 * correct information is passed to OpScopeNetwork::OnDataSent().
	 * Whenever a socket is destroyed any corresponding SentData objects will
	 * be removed.
	 */
	struct SentData : public ListElement<SentData>
	{
		SentData(OpSocket *socket, unsigned bytes_sent)
			: socket(socket)
			, bytes_sent(bytes_sent)
		{}

		OpSocket *socket;
		unsigned bytes_sent;
	};

	/**
	 * A queue of byte data which have been sent to the network socket. The
	 * queue is processed as soon as the MSG_SCOPE_PROCESS_MESSAGE callback
	 * is received.
	 */
	List<SentData> sent_queue;

	/**
	 * Determines if the MSG_SCOPE_PROCESS_MESSAGE has been sent or not.
	 * This is used to avoid having multiple messages posted when sent_queue
	 * already has items.
	 *
	 * @c TRUE if the MSG_SCOPE_PROCESS_MESSAGE message has been sent,
	 * @c FALSE otherwise.
	 */
	BOOL message_sent;
};

/**
 * Class for communicating with a client over the network. It will encode
 * and decode messages to/from STP/1.
 * It also handles the handshake with the remote client.
 *
 * The class acts as a scope client and can be connected to any scope host.
 */
class OpScopeNetworkClient
	: public OpScopeClient
	, public OpScopeNetwork
{
public:
	OpScopeNetworkClient(OpScopeTPMessage::MessageType type, BOOL auto_disconnect = FALSE, OpScopeHostManager *host_manager = NULL);
	virtual ~OpScopeNetworkClient();
	OP_STATUS Construct();

	/**
	 * Disconnect from the remote network client and detach from the host.
	 */
	virtual void Disconnect();

private:
	enum State
	{
		  /**
		   * Service list is sent to the client.
		   */
		  State_SendServiceList
		  /**
		   * The service list has been sent. Waiting for a handshake from the
		   * remote client.
		   */
		, State_Handshake
		  /**
		   * The handshake response is being sent.
		   */
		, State_HandshakeResponse
		  /**
		   * Handshake response has been sent and normal message processing
		   * continues.
		   */
		, State_Normal
	};
	State state;
	int   handshake_remaining; ///< Number of bytes left to send of the handshake
	ByteBuffer incoming; ///< Buffer containing incoming network data, shared with OpScopeNetwork
	BOOL  auto_disconnect; ///< @c TRUE if the client should disconnect the network connection when a host is deatched.

	/**
	 * Switch to a new state and execute the state transition steps.
	 */
	OP_STATUS SwitchState(State new_state);

	OP_STATUS SendSTPHeader();
	void      ResetState();

	// From OpScopeClient

	virtual const uni_char *GetType() const { return UNI_L("network"); }
	virtual OP_STATUS Receive( OpAutoPtr<OpScopeTPMessage> &message );
	virtual OP_STATUS Serialize(OpScopeTPMessage &to, const OpProtobufInstanceProxy &from, OpScopeTPHeader::MessageType type);
	virtual OP_STATUS Parse(const OpScopeTPMessage &from, OpProtobufInstanceProxy &to, OpScopeTPError &error);

	virtual OP_STATUS OnHostAttached(OpScopeHost *host);
	virtual OP_STATUS OnHostDetached(OpScopeHost *host);
	virtual OP_STATUS OnHostMissing();

	// From OpScopeTPReader

	virtual OP_STATUS OnMessageParsed(const OpScopeTPMessage &message);
	virtual OP_STATUS OnHandshakeCompleted(unsigned int version)
	{
		return OpStatus::OK;
	}
	virtual void OnError(OpScopeTPReader::Error error);

	// From OpScopeTPWriter

	virtual OP_STATUS OnMessageSent(const OpScopeTPMessage &message);

	// From OpScopeNetwork

	virtual void Reset();
	virtual OP_STATUS InitializeConnection();

	/* virtual */ void OnSocketClosed(OpSocket* socket);
	/* virtual */ void OnSocketDataReady(OpSocket* socket);
	/* virtual */ void OnSocketDataSent(OpSocket *s, unsigned int bytes_sent);
};

#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_NETWORK_H
