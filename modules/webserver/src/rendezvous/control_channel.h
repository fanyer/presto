/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 * Web server implementation 
 */

#ifndef CONTROL_CHANNEL_H
#define CONTROL_CHANNEL_H

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

#include "modules/hardcore/timer/optimer.h"
#include "modules/formats/hdsplit.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"
#include "modules/opera_auth/opera_account_manager.h"

#include "modules/url/protocols/comm.h"

class WebserverRendezvous;
class ControlMessage;

/** Hack. Should be removed FTIP */ 
class SocketAndBuffer
{

public:
	OpSocket *		m_socket;
private:
	Comm::Comm_strings *current_buffer;
	AutoDeleteHead		buffers;
	BOOL				sending_in_progress;

public:
	SocketAndBuffer(OpSocket *);
	~SocketAndBuffer();

	/** Takes over string */
	OP_STATUS SendData(const OpStringC8 &string, uint32 len);

	OP_STATUS SendDataToConnection();
	void SocketDataSent();
	void Close();
};

class ConnectData : /* FIXME: Turn this into a OpSocket implementation */
	public SocketAndBuffer
{
public:
	static OP_STATUS Make(ConnectData *&connectData, UINT clientID, const OpStringC8 &nonce, const OpStringC8 &client_ip, OpSocket *socket);// { this->clientID = clientID; this->nonce = nonce; }

	~ConnectData();

public:
	UINT m_clientID;
	OpString8 m_nonce;
	OpString8 m_client_ip;

private:
	ConnectData(UINT clientID, OpSocket *socket);
};

class ControlChannel 
	: public MessageObject 
	, public OpSocketListener
	, public OpHostResolverListener
	, public OpTimerListener 
	, public SocketAndBuffer
	, public CoreOperaAccountManager::Listener
{
public:

	enum ReadStatus
	{   RS_ONGOING, RS_READ, RS_OOM};

	// Communication actions
	enum CommAction
	{  
		// Used to resolve the proxy name and then connect / reconnect
		RESOLVE,
		// Connect to the proxy (after RESOLVE)
		CONNECT,
		// Starts the handshake: REGISTER, CHALLENGE, RESPONSE
		REGISTER,
		// Send a keep-alive message
		KEEPALIVE,
		// Send a checkport message for the current open port.
		CHECKPORT
	};

	static OP_STATUS Create(ControlChannel **channel, WebserverRendezvous *owner, const char *shared_secret);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	ControlChannel();

	~ControlChannel();
	
	static OP_STATUS Encrypt(const OpStringC8 &nonce, const OpStringC8 &private_key, OpString8 &output);

	/***************** From OpTimerListener ***********************/
	virtual void OnTimeOut(OpTimer* timer);
	
	/***************** Methods inherited from OpSocketListener ***********************/

	void OnSocketListenError(OpSocket* socket, OpSocket::Error error){}
	
	void OnSocketConnected(OpSocket* socket);
	void OnSocketDataReady(OpSocket* socket);
	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	void OnSocketClosed(OpSocket* socket);
	void OnSocketConnectionRequest(OpSocket* socket){}
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error){} /* FIXMME : Implement this */
	void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent){}
#ifdef _EXTERNAL_SSL_SUPPORT_	
	virtual void OnSecureSocketError(OpSocket* socket, OpSocket::Error error){ OP_ASSERT(!"SHOULD NOT BE HERE"); }
#endif // _EXTERNAL_SSL_SUPPORT_

	// CoreOperaAccountManager::Listener callback
	virtual void OnAccountDeviceRegisteredChange();

	/***************** Methods inherited from OpHostResolverListener ***********************/

	void OnHostResolved(OpHostResolver* host_resolver);

	void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);

	OP_STATUS Rsvp(int id, int error_code);
	
	INTPTR Id(){ return (INTPTR)this;}
	
	BOOL IsConnected();
	OP_STATUS CheckPort(unsigned int port_to_check);

private:

	OP_STATUS KeepAlive();
	void ParseMessage(ControlMessage *msg);
	OP_STATUS RespondToChallenge(const OpStringC8 &nonce);
	void Close();
	OP_STATUS Send(const OpStringC8 &msg, unsigned int len);
	OP_STATUS Connect();
	OP_STATUS Reg();
	ReadStatus ReadMessage(ControlMessage &resultp);
	void ProcessBuffer();

private:

	BOOL m_sendingKeepAlive;
	OpString8 m_sharedSecret;
	WebserverRendezvous *m_owner;
	OpSocketAddress *m_addr_ucp; // Address of the UCP proxy
	OpHostResolver *m_resolver;
	char *m_buffer;
	int m_bufPos; // where to start filling buffer
	
	OpAutoPointerHashTable<OpSocket, ConnectData> m_connectSockets; /* this only stores the ConnectData */

	RendezvousLog *m_logger;
	
	BOOL m_is_connected;
	BOOL m_error_callback_sent;

	OpTimer m_connection_timer;
	BOOL m_waiting_for_200_ok;
	BOOL m_waiting_for_challenge;
	
	BOOL m_has_sent_rendezvous_connected;
	UINT32 m_keepalive_timeout;  // Milliseconds to wait between each keepalive message
};

class ControlMessage
{
friend class ControlChannel;

public:

	enum ParseStatus
	{   PS_OK, PS_PARSE_ERROR, PS_PARSE_VERSION_ERROR, PS_OOM};

	enum MessageType
	{   /** Response phase of the Register/Challenge/Response handshake */
		MT_UCP_RESPONSE,
		/** Challenge phase of the Register/Challenge/Response handshake */
		MT_CHALLENGE,
		/** Invite: a client tries to connect to the Unite Server */
		MT_INVITE,
		/** Upload rate */
		MT_THROTTLE,
		/** The server has been kicked out by another Unite server (takeover) */
		MT_KICKEDBY,
		/* Undefined */
		MT_UNDEFINED
		};

	ControlMessage();

	OP_BOOLEAN CheckProtocolVersion(const char *protocol);
	
	OP_STATUS SetMessage(char* message, int length);

	virtual ~ControlMessage(){ OP_DELETE(m_addr_http); }

	ParseStatus ParseMessage();

	const char *CStr();

	BOOL IsIllegal();
	
	HeaderList *GetHeaderList() { return &m_proxyHeaderList; }

public:

	MessageType m_type;
	UINT m_statusCode;
	UINT m_clientID;
	OpString8 m_nonce;
	OpString8 m_client_ip;
	UINT m_uploadRate;
	
private:

	ParseStatus SetNonce(const char *src);

private:

	HeaderList m_proxyHeaderList;	
	OpString8 m_message;
	OpSocketAddress *m_addr_http; // Address of the HTTP proxy

};

#ifdef DEBUG_ENABLE_OPASSERT
	void DEBUG_VERIFY_SOCKET(OpSocket *socket);
#else
	#define DEBUG_VERIFY_SOCKET(SOCK)
#endif

#endif // WEBSERVER_RENDEZVOUS_SUPPORT
#endif
