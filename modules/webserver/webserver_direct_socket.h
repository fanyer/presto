/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef WEBSERVER_DIRECTS_SOCKET_H_
#define WEBSERVER_DIRECTS_SOCKET_H_

#include "modules/pi/network/OpSocket.h"
#include "modules/webserver/src/webservercore/webserver-references.h"
class WebServerConnectionListener;
class WebserverDirectServerSocket;

// number used to check if the socket has not beend eleted...
#define WEB_SERVER_MAGIC_NUMBER 0xF2D54B03

class WebserverDirectClientSocket : public OpSocket, public MessageObject 
{
private:
	//UINT32 m_magic_number;  // Guard against multiple delete
	friend class WebserverDirectServerSocket;
	
public:
	WebserverDirectClientSocket(BOOL is_owner = FALSE);
	virtual ~WebserverDirectClientSocket();

	static OP_STATUS CreateDirectClientSocket(WebserverDirectClientSocket** socket, OpSocketListener* listener, BOOL ssl_connection=FALSE, BOOL is_owner = FALSE);
	
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	INTPTR Id(){return (INTPTR)this; }
	
	virtual OP_STATUS Connect(OpSocketAddress* socket_address);
	virtual OP_STATUS Send(const void* data, UINT length);
	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);

	virtual void Close();
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address); // Will always return 127.0.0.1

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address);
#endif // OPSOCKET_GETLOCALSOCKETADDR
	virtual void OnSocketConnectError(OpSocket::Error error);
	virtual void OnSocketDataReady(WebserverDirectServerSocket* socket);
	virtual void OnSocketDataSent(WebserverDirectServerSocket* socket, UINT bytes_sent);
	virtual void OnSocketClosed(WebserverDirectServerSocket* socket);
	
	virtual void SetListener(OpSocketListener* listener);

#ifdef SOCKET_LISTEN_SUPPORT
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size){ return OpStatus::ERR; }
	virtual OP_STATUS Accept(OpSocket* socket){ OP_ASSERT(!"use Accept(WebserverDirectServerSocket *socket) instead"); return OpStatus::ERR; } 
	virtual OP_STATUS Accept(WebserverDirectServerSocket* socket);
#endif // SOCKET_LISTEN_SUPPORT
	
	
	/* These are not implemented */
#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual OP_STATUS UpgradeToSecure(){ return OpStatus::ERR; }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count){ return OpStatus::ERR; }
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate){ return OpStatus::ERR; }
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize){ return OpStatus::ERR; }
	virtual OP_STATUS SetServerName(const uni_char* server_name){ return OpStatus::ERR; }
	virtual OpCertificate *ExtractCertificate(){ return NULL; }
	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain(){ return NULL; }
	virtual int GetSSLConnectErrors(){ return 0;}
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	virtual void SetAbortiveClose(){ OP_ASSERT(!"NOT YET IMPLEMENTED");}
#endif // SOCKET_NEED_ABORTIVE_CLOSE

#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE){ OP_ASSERT(!"NOT YET IMPLEMENTED");}
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv(){ OP_ASSERT(!"NOT YET IMPLEMENTED"); return OpStatus::ERR;}
	virtual OP_STATUS ContinueRecv(){ OP_ASSERT(!"NOT YET IMPLEMENTED"); return OpStatus::ERR;}
#endif // OPSOCKET_PAUSE_DOWNLOAD	

#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { return OpStatus::OK; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { OP_ASSERT(!"NOT YET IMPLEMENTED"); return OpStatus::ERR; }
#endif // OPSOCKET_OPTIONS

private:
	ReferenceUserSingle<WebServerConnectionListenerReference> m_webserver_connection_listener;
	
	WebserverDirectServerSocket *m_server_socket;
	
	OpSocketListener *m_client_socket_listener;
	MessageHandler *m_mh;
	
	BOOL m_connected;
	
	const void* m_data_from_client;
	UINT  m_data_from_client_length;
	UINT  m_data_from_client_length_sent;
	
	const void* m_data_to_client;
	UINT  m_data_to_client_length;
	UINT  m_data_to_client_length_sent;

	BOOL m_is_owner;
};

class WebserverDirectServerSocket : public OpSocket //, public OpSocketListener //, public MessageObject
{
private:
	//UINT32 m_magic_number;  // Guard against multiple delete
	/// Reference object used on connections
	ReferenceObjectSingle<WebserverDirectServerSocket> ref_obj_conn;

public:
	WebserverDirectServerSocket();
	virtual ~WebserverDirectServerSocket();

	static OP_STATUS CreateDirectServerSocket(WebserverDirectServerSocket** socket, /*OpSocketListener* listener,*/ BOOL secure=FALSE);
	/// Return the referecne object
	ReferenceObject<WebserverDirectServerSocket> *GetReferenceObjectPtrConn() { return &ref_obj_conn; }

	
//	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	INTPTR Id(){return (INTPTR)this; }

	/* Interface for WebserverDirectClientSocket */
	void SendConnectionError(OpSocket::Error error_code);
	void DataFromClient(const void* data, UINT length);
	// Notify againt the client if some data is available
	void NotifyClientAgain();
	void GetDataToClient(const void *&data, UINT &length);
	
	virtual OP_STATUS Connect(OpSocketAddress* socket_address){ OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT"); return OpStatus::ERR; }
	virtual OP_STATUS Send(const void* data, UINT length);

	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);

	virtual void Close();
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address); // Will always return 127.0.0.1

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address);
#endif // OPSOCKET_GETLOCALSOCKETADDR
	
	
	virtual void SetListener(OpSocketListener* listener) { m_server_socket_listener = listener; }
	
	virtual void SetClientSocket(WebserverDirectClientSocket* client_socket) { m_client_socket = client_socket; }
#ifdef SOCKET_LISTEN_SUPPORT
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size){ OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT"); return OpStatus::ERR; }
	virtual OP_STATUS Accept(OpSocket* socket){ OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT"); return OpStatus::ERR; }
#endif // SOCKET_LISTEN_SUPPORT

#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual OP_STATUS UpgradeToSecure(){ return OpStatus::ERR; }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count){ return OpStatus::ERR; }
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate){ return OpStatus::ERR; }
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize){ return OpStatus::ERR; }
	virtual OP_STATUS SetServerName(const uni_char* server_name){ return OpStatus::ERR; }
	virtual OpCertificate *ExtractCertificate(){ return NULL; }
	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain(){ return NULL; }
	virtual int GetSSLConnectErrors(){ return 0;}
#endif // _EXTERNAL_SSL_SUPPORT_


#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	virtual void SetAbortiveClose(){ OP_ASSERT(!"NOT YET IMPLEMENTED");}
#endif // SOCKET_NEED_ABORTIVE_CLOSE

#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE){ OP_ASSERT(!"NOT YET IMPLEMENTED");}
#endif // SOCKET_SUPPORTS_TIMER
//
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv(){ OP_ASSERT(!"NOT YET IMPLEMENTED"); return OpStatus::ERR;}
	virtual OP_STATUS ContinueRecv(){ OP_ASSERT(!"NOT YET IMPLEMENTED"); return OpStatus::ERR;}
#endif // OPSOCKET_PAUSE_DOWNLOAD	

#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR; }
#endif // OPSOCKET_OPTIONS

/* Implementation of OpSocketListener listener */
//	virtual void OnSocketConnected(OpSocket* socket);
//	virtual void OnSocketDataReady(OpSocket* socket);
	void OnSocketDataSent(WebserverDirectClientSocket* socket, UINT bytes_sent);
	void OnSocketClosed(WebserverDirectClientSocket* socket);

#ifdef SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectionRequest(OpSocket* socket){OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT");  }
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error){OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT"); }
#endif // SOCKET_LISTEN_SUPPORT
	
//	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error){ OP_ASSERT(!"DOES NOT MAKE SENSE IN THIS CONTEXT"); }
//	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
//	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
//	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);

#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent) { }
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT	

private:
	WebserverDirectClientSocket *m_client_socket;
	OpSocketListener *m_server_socket_listener;

	BOOL m_connected;
	
	const void* m_data_from_client;
	UINT  m_data_from_client_length;
	UINT  m_data_from_client_length_sent;
	
	const void* m_data_to_client;
	UINT  m_data_to_client_length;
	UINT  m_data_to_client_length_sent;
};

#endif /* WEBSERVER_DIRECTS_SOCKET_H_ */
