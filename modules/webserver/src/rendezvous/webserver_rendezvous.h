/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation 
 */

#ifndef WEBSERVER_RENDEZVOUS_H
#define WEBSERVER_RENDEZVOUS_H
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

#include "modules/pi/network/OpSocket.h"
#include "modules/webserver/src/rendezvous/control_channel.h"

class ControlChannel;
class ConnectData;
class Hashtable;
class WebserverRendezvous;


enum RendezvousStatus
{
	 RENDEZVOUS_PROXY_CONNECTION_LOGGED_OUT						 // Logged out from the proxy. Rendezvous will NOT try to reconnect
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_DOWN				 // Connection was refused. Rendezvous will try to reconnect
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE		 // Rendezvous will try to reconnect
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_NETWORK					 // Rendezvous will try to reconnect
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_DENIED

	,RENDEZVOUS_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN 	 // The proxy address is unkown. Rendezvous will NOT try to reconnect
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_AUTH_FAILURE				 // Invalid username/password
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED // The device has already been registered
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION	 // The proxy has been updated, and webserver is still using the old protocol
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION  // Unsecure version
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_PROTOCOL					 // Serious protocol error. Probably homebrew proxy.
	,RENDEZVOUS_PROXY_CONNECTION_ERROR_MEMORY

};

class RendezvousEventListener
{
public:
	virtual ~RendezvousEventListener(){};

	/* Successfully connected to the proxy */
	virtual void OnRendezvousConnected(WebserverRendezvous *socket) = 0;

	/* Could not connect to the proxy */
	virtual void OnRendezvousConnectError(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry) = 0;
	
	/* Connection is open, but there is a problem sending (lost local connection ?) */
	virtual void OnRendezvousConnectionProblem(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry) = 0;
	
	virtual void OnRendezvousConnectionRequest(WebserverRendezvous *socket) = 0;
	
	/* Disconnected from the proxy */
	virtual void OnRendezvousClosed(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry, int code = 0) = 0;
	
	virtual void OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address) = 0;
};


/// Configuration of a single Timeout attempt step.
/// Let's suppose that wait_seconds is 5 seconds and num_attempts is 12 and random_seconds is 0,
/// The client will try to reconnect every 5 seconds for a minute, then it will switch to another step
struct TimeoutStep
{
	UINT32 wait_seconds;	 // Number of seconds to wait
	UINT32 num_attempts;	 // Number of attempts performed with this configuration
	UINT32 random_seconds; // Number of random seconds to add to wait_seconds
};

// Possible types of timeout
enum TimeoutType
{
	TIMEOUT_TYPE_NONE,		// Timeout unknown
	TIMEOUT_TYPE_CLIENT,	// Timeout for client errors
	TIMEOUT_TYPE_PROXY		// Timeout for proxy errors
};

enum RendezVousError
{
	// Error connecting
	RENDEZVOUS_ERROR_CONNECT,
	// The connection has been closed
	RENDEZVOUS_ERROR_CLOSE,
	// Connection problem
	RENDEZVOUS_ERROR_PROBLEM
};

#define TIMEOUT_INFINITE_STEP 0x6FFFFFFF
#define TIMEOUT_NEVER 0x6FFFFFFF
/// Class that manage a policy of multiple Timeouts (used for example to reconnect to the Unite proxy)
class TimeoutPolicy
{
private:
	TimeoutStep steps[10];  // Steps to perform
	UINT num_steps;			// Real number of steps
	UINT auto_step;			// Step used in "auto mode". Be aware to mix the modes...
	int type;				// Type of timeout (application dependent)
	
public:
	TimeoutPolicy(int timeout_type) { num_steps=0; auto_step=0; type=timeout_type; }
	
	/// Add a step to the policy, repeated num times
	OP_STATUS AddFiniteStep(UINT32 wait, UINT32 num, UINT32 random);
	/// Add a step to the policy, repeated an infinite amount of times
	OP_STATUS AddInfiniteStep(UINT32 wait, UINT32 random) { return AddFiniteStep(wait, TIMEOUT_INFINITE_STEP, random); }
	/// Get the time to wait in the current step (in ms); RECONNECT_NEVER means that every reconnect attempt has to be stopped
	UINT32 GetWaitTimeMS(UINT32 step);
	/// Get the timeout based on the automatic step, and increment it
	UINT32 GetAutoWaitTimeMS() { return GetWaitTimeMS(auto_step++); }
	/// Reset the automatic step
	void ResetStep(UINT32 step=0) { auto_step=0; }
	/// Get the type
	int GetType() const { return type; }
};

class WebserverRendezvous 
	: public OpSocket
	, public MessageObject
	, public OpSocketListener
{
private:
	TimeoutPolicy m_tp_client;	// Errors originated on the client PC
	TimeoutPolicy m_tp_proxy;		// Errors originated on the proxy
	TimeoutType last_timeout_type;		// Last Timeout type used, to understand when to reset
public:
	~WebserverRendezvous();

	/***************** Methods inherited from OpSocket ***********************/

	OP_STATUS Connect(OpSocketAddress* socket_address);

	OP_STATUS Send(const void* data, UINT length);

	OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	/* WebserverRendezvous is a listening socket only, so this makes no sense here */
	virtual OP_STATUS PauseRecv() { return OpStatus::ERR; }
	virtual OP_STATUS ContinueRecv() { return OpStatus::ERR; }
#endif // OPSOCKET_PAUSE_DOWNLOAD	

#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
#endif // OPSOCKET_OPTIONS

#ifdef _EXTERNAL_SSL_SUPPORT_
	/** Start secure session. */
	virtual OP_STATUS UpgradeToSecure(){ OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; };
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate){ OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count){ OP_ASSERT(!"NOT IMPLEMENTED");return OpStatus::ERR; };
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize){ OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
	virtual OP_STATUS SetServerName(const uni_char* server_name){ OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
	virtual void SetCertificateDialogMode(BOOL aUnAttendedDialog){ OP_ASSERT(!"NOT IMPLEMENTED"); return;}
	virtual OpCertificate* ExtractCertificate(){  OP_ASSERT(!"NOT IMPLEMENTED");return  NULL; }
	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain(){ OP_ASSERT(!"NOT IMPLEMENTED");return  NULL; }
	virtual int GetSSLConnectErrors(){ OP_ASSERT(!"NOT IMPLEMENTED");return OpSocket::SSL_NO_ERROR; }
#endif // _EXTERNAL_SSL_SUPPORT_
	
	void Close();

	OP_STATUS GetSocketAddress(OpSocketAddress* socket_address);

	void SetListener(OpSocketListener* listener);

	OP_STATUS Accept(OpSocket* socket)
	{
		return OpStatus::ERR;
	}

	/***************** Methods inherited from OpSocket ***********************/
	OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size);

	/***************** Methods inherited from OpSocketListener ***********************/

#ifdef OPSOCKET_GETLOCALSOCKETADDR
     /**
       * Returns the address of the local end of the socket connection
       *
       * @param socket_address a socket address to be filled in
       */
	OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address);
#endif // OPSOCKET_GETLOCALSOCKETADDR

	void OnSocketListenError(OpSocket* socket, OpSocket::Error error);

	void OnSocketConnected(OpSocket* socket);
	
	void OnSocketDataReady(OpSocket* socket);

	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	void OnSocketClosed(OpSocket* socket);
	
	void OnSocketConnectionRequest(OpSocket* socket);
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);
#ifdef _EXTERNAL_SSL_SUPPORT_	
	virtual void OnSecureSocketError(OpSocket* socket, OpSocket::Error error){ OP_ASSERT(!"SHOULD NOT BE HERE"); }
	virtual BOOL OnSocketSecurityProblem(OpSocket* socket, /* problem, certificate, cipher, */ BOOL &later){ OP_ASSERT(!"NOT IMPLEMENTED");return FALSE; }	
#endif // _EXTERNAL_SSL_SUPPORT_

	/********************* Other methods *************************************/
	void SendRendezvousConnectError(RendezvousStatus status);
	void SendRendezvousCloseError(RendezvousStatus status, int code = 0);
	void SendRendezvousConnectionProblem(RendezvousStatus status);
	
	/// Post a message to reconnect to the proxy after the amount of time specified by the policy (based on the step)
	void RetryConnectToProxy(TimeoutType suggested_type);
	
	// inherited from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS Accept(OpSocket **socket, OpSocketListener *connection, OpString8 &client_socket_ip);


	BOOL isDirect();

	INTPTR Id(){ return (INTPTR)this;}

	BOOL IsConnected();
	
	/********************* Static methods ************************************/

	static OP_STATUS Create(WebserverRendezvous** socket, RendezvousEventListener *eventListener, const char *shared_secret, unsigned int local_listening_port);

	static BOOL IsConfigured();
		
	BOOL HasDirectAccess(){ return m_direct_access; }

	/// Asks to the proxy to check if the port is really available
	void CheckPort(unsigned int port_to_check);
private:
	void SetDirectAccess(BOOL direct_access, const char* direct_access_address){ if (m_direct_access != direct_access)  m_rendezvousEventListener->OnDirectAccessStateChanged(direct_access, direct_access_address); m_direct_access = direct_access; }
	WebserverRendezvous(RendezvousEventListener *rendezvousEventListener);
	// Get the reconnection policy to use in this case
	TimeoutType GetReconnectionPolicy(RendezvousStatus error, RendezVousError error_type);


	unsigned int m_backlog; // max # of incoming connections pending (subsequent connections will be rejected)

	OpAutoPointerHashTable<OpSocket, ConnectData> m_pendingConnections; /* this only stores the ConnectData */
	OpHashIterator* m_pendingConnectionsIterator;

	ControlChannel *m_controlChannel;

	RendezvousEventListener *m_rendezvousEventListener;

	OpString8 m_shared_secret;
	friend class ControlChannel;
	unsigned int m_local_listening_port;
	unsigned int m_last_checked_port;
	BOOL m_direct_access; /* If port has been opened */
};

#endif // WEBSERVER_RENDEZVOUS_SUPPORT
#endif
