 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsSocket.h
  *
  * Windows version of the standard interface for Opera socket handling. 
  *
  * @author Rob Gregson
  * @author Johan Borg
  *
  * @version $Id$
  */
  

#ifndef __WINDOWSSOCKET2_H
#define __WINDOWSSOCKET2_H

#include "WindowsSocketAddress2.h"

#include "modules/pi/network/OpSocket.h"
#ifdef OPUDPSOCKET
#include "modules/pi/network/OpUdpSocket.h"
#endif // OPUDPSOCKET

class OpSocketListener;
class OpSocketAddress;
class WindowsSocketManager;

class WindowsSocket2 : public OpSocket, public MessageObject
{
public: 

	enum ESocketErrorType { EConnect, EListen, EReceive, ESend, EClose };
	
	// Implementation of OpSocket interface.
	OP_STATUS	Connect(OpSocketAddress* socket_address);
	OP_STATUS	SendTo(const void* data, UINT length, OpSocketAddress* socket_address);
	OP_STATUS	RecvFrom(void* buffer, UINT length, OpSocketAddress* socket_address, UINT* bytes_received);
	OP_STATUS	Accept(OpSocket* socket);
	OP_STATUS	GetSocketAddress(OpSocketAddress* socket_address);
	OP_STATUS	GetLocalSocketAddress(OpSocketAddress* socket_address);
	void		SetListener(OpSocketListener* listener) { iObserver = listener; }
	OP_STATUS	Send(const void* data, UINT length);
	OP_STATUS	Recv(void* buffer, UINT length, UINT* bytes_received);
	OP_STATUS	Listen(OpSocketAddress* socket_address, int queue_size);
	void		Close();
	
#ifdef AUTO_UPDATE_SUPPORT
	// Implementation of API_URL_TCP_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv();
	virtual OP_STATUS ContinueRecv();
#endif // AUTO_UPDATE_SUPPORT

	// Receives messages from the winsock DLL via Opera g_main_message_handler.
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam);

#ifdef _EXTERNAL_SSL_SUPPORT_
	// Overrides WindowsSocket
	virtual BOOL StartSecure();                                                          
	virtual BOOL SetSecureCiphers(const cipherentry_st* ciphers, UINT chipher_count, BOOL tls_v1_enabled);
#endif

	// In order to make sure that the socket libraries are properly initialized
	// this manager must be set and initialized. It is static because only 
	// one instance will ever be needed. (Doing most of what the old factory did.)
	static WindowsSocketManager* m_manager; 
	
private:

	friend class OpSocket;
	
	WindowsSocket2(OpSocketListener* const aObserver);
	~WindowsSocket2();

	OP_STATUS	SocketSetLocalPort(UINT port);
	OP_STATUS	SocketListen(UINT queue_size);

	OP_STATUS	HandleError(int socketError, ESocketErrorType aErrorType);
	void		SocketError(OpSocket::Error aSocketErr, ESocketErrorType aErrorType);

		/** Gets a socket handle from the OS when needed.
	  * Assumes that one socket never changes IP family
	  * after the first host address is known.
	  * @param aIPFamily permits to set the family (AF) before calling socket()
	  * @return TRUE if a socket is either successfully created or reused
	  */
	OP_STATUS SetSocket(OpSocketAddress *socket_address = NULL);

	virtual BOOL SendToSocket(const void* aData, unsigned int aLength, unsigned int &bytesSent);
	virtual BOOL ReceiveFromSocket(const void* aData, unsigned int aLength, unsigned int &bytesSent);

	// Fixes issues with how the defaults for TCP Window Scale size has changed compared to previous versions.
	// This change breaks some routers, so we force it back to the old behavior with this method
	virtual OP_STATUS FixVistaSocket(SOCKET socket);

	// Sends waiting data in internal buffer when OS permits us.
	void SendMore();


	int iDomain;					///< Typically AF_INET
	SOCKET iSocket;					///< Socket handle
	WindowsSocketAddress2 *iLocalAddress; ///< Current local address
	WindowsSocketAddress2 *iSocketAddress;///< Current remote address
	BOOL own_address;
	char *iCurrentData;				///< Current outgoing data
	char *iCurrentRestData;			///< Data still waiting to be sent
	unsigned int iCurrentLen;		///< Length of current outgoing data
	unsigned int iCurrentRestLen;	///< Length of data waiting to be sent
	BOOL iClosing;					///< Tells if the socket is closing down
	OpSocketListener* iObserver;
	BOOL iPaused;					///< Tells if the socket is paused (not handling reads)
};

#ifdef OPUDPSOCKET

/**
Windows implementation for a UDP socket - based on the WinGogi implementation
*/
class WindowsUdpSocket : public OpUdpSocket, public MessageObject
{
public:
	WindowsUdpSocket(OpUdpSocketListener* listener, OpSocketAddress* local_address);
	virtual ~WindowsUdpSocket();

	virtual void SetListener(OpUdpSocketListener* listener) { m_listener = listener; } ;
	virtual OP_NETWORK_STATUS Bind(OpSocketAddress* socket_address, OpUdpSocket::RoutingScheme routing_scheme = OpUdpSocket::UNICAST);
	virtual OP_NETWORK_STATUS Send(const void* data, unsigned int length, OpSocketAddress* socket_address, OpUdpSocket::RoutingScheme routing_scheme = OpUdpSocket::UNICAST);
	virtual OP_NETWORK_STATUS Receive(void* buffer, unsigned int length, OpSocketAddress* socket_address, unsigned int* bytes_received);
	static void HandleSocketEvent(SOCKET socket, int event, int error);
	/** Create the socket, initializing it (also set the IP_MULTICAST_IF, if required) */
	OP_STATUS Init();

	// Receives messages from the winsock DLL via Opera g_main_message_handler.
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam);

private:
	friend class WindowsSocket2;

	OpUdpSocketListener* m_listener;			///< Listener of the events
	SOCKET m_socket;							///< UDP socket
	WindowsSocketAddress2	m_local_address;	///< Local address
//	SOCKET_ADDRESS m_local_address;				///< Local network card address
	UINT m_local_port_bound;					///< Local Port, 0 if ht esocket is unbound
	int last_broadcast_option;					///< Last value used to set the broadcast option, to avoid re-set it every time

	/** Binds the socket to the required port */
	OP_NETWORK_STATUS InternalBind(WindowsSocketAddress2 *socket_address, UINT port);
};

#endif // OPUDPSOCKET

class WindowsSocketManager
{
public:
	WindowsSocketManager();
	~WindowsSocketManager();

	OP_STATUS Init();
};

class WindowsSocketLibrary
{

private:
	friend class WindowsSocketManager;

	/** We loaded Winsock 2 */
	static BOOL m_IsWinSock2;

#ifdef __SUPPORT_IPV6_REMOVED
	/** IPv6 is available */
	static BOOL m_HasIPv6;
#endif

	static int m_Winsock2ProtocolCount;

	static LPAFPROTOCOLS m_Winsock2Protocols;

#ifdef __SUPPORT_IPV6_REMOVED
	static LPWSAPROTOCOL_INFO *m_Winsock2ProtcolInfo;
#endif

public:

	/** 
	*	Initialize Winsock 2 specific data. 
	*	If the operation fails Winsock2 is turned off
	*/
	static void InitWinsock2();

	/** Load Winsock2 functions from DLL */
	static BOOL LoadWinsock2Functions();

	/** Get Winsock2 status */
	static BOOL GetIsWinsock2(){return m_IsWinSock2;}

#ifdef __SUPPORT_IPV6_REMOVED
	/** Get IPv6 Status */
	static BOOL GetHasIPv6(){return m_HasIPv6;}
#endif

	/** Get List of supported protocols */
	static LPAFPROTOCOLS GetProtocolIDs(DWORD &count);

	/** Get the ProtocolInfo for a given protocol */
	static LPWSAPROTOCOL_INFO GetProtocolInfo(int a_Protocol);

#define Winsock2Active() WindowsSocketLibrary::GetIsWinsock2()

#ifdef __SUPPORT_IPV6_REMOVED
#define IPv6Available() WindowsSocketLibrary::GetHasIPv6()
#endif

#define Winsock2GetProtocolIDs(count) WindowsSocketLibrary::GetProtocolIDs(count)
#define Winsock2GetProtocolInfo(prot) WindowsSocketLibrary::GetProtocolInfo(prot)

};

#endif	//	__WINDOWSSOCKET_H
