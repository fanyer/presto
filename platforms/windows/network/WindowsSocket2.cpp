 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2004 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsSocket2.cpp
  *
  * Windows implementation of the Opera socket.
  *
  * @author Johan Borg, Petter Nilsen, various
  *
  *
  */


#include "core/pch.h"

#include "platforms/windows/network/WindowsSocket2.h"
#include "platforms/windows/CustomWindowMessages.h"

#ifdef YNP_WORK
# include "modules/olddebug/tstdump.h"
# define DEBUG_COMM_FILE
#endif // YNP_WORK

extern BOOL IsSystemWinVista();
extern OP_STATUS WinErrToOpStatus(DWORD error);

// from ws2def.h
#ifndef IOC_IN
#define IOC_IN                      0x80000000      /* copy in parameters */
#endif // IOC_IN
#ifndef IOC_VENDOR
#define IOC_VENDOR                  0x18000000
#endif // IOC_VENDOR
#define _WSAIOW(x,y)                (IOC_IN|(x)|(y))

#define SIO_SET_COMPATIBILITY_MODE  _WSAIOW(IOC_VENDOR,300)

//#define WINSOCK_PROFILING
#ifdef WINSOCK_PROFILING

class WinsockProfiler
{
public:
	WinsockProfiler(uni_char *method)
	{
		m_method = method;
		
		g_op_system_info->GetWallClock(m_startseconds, m_startticks);

		uni_snprintf(m_output_string, 200, UNI_L("START method: %s, time: %d:%d\n"), m_method, m_startseconds, m_startticks);
		OutputDebugString(m_output_string);
	}
	virtual ~WinsockProfiler()
	{
		g_op_system_info->GetWallClock(m_startseconds, m_startticks);

		uni_snprintf(m_output_string, 200, UNI_L("END method: %s, time: %d:%d\n"), m_method, m_startseconds, m_startticks);
		OutputDebugString(m_output_string);
	}

private:
	uni_char	*m_method;
	unsigned long	m_startticks;
	unsigned long	m_startseconds;
	uni_char	m_output_string[200];
};

#endif // WINSOCK_PROFILING

typedef enum _WSA_COMPATIBILITY_BEHAVIOR_ID 
{
	WsaBehaviorAll = 0,
	WsaBehaviorReceiveBuffering,
	WsaBehaviorAutoTuning
} WSA_COMPATIBILITY_BEHAVIOR_ID, *PWSA_COMPATIBILITY_BEHAVIOR_ID;

typedef struct _WSA_COMPATIBILITY_MODE 
{
	WSA_COMPATIBILITY_BEHAVIOR_ID BehaviorId;
	ULONG TargetOsVersion;
} WSA_COMPATIBILITY_MODE, *PWSA_COMPATIBILITY_MODE;

OP_STATUS WindowsSocket2::FixVistaSocket(SOCKET socket)
{
	// On Vista, we need to make sure the TCP Window scale factor is not above 4, so we set it here. 
	// Some Cisco routers don't like Vista's new defaults, so we will try to please them. See
	// https://bugzilla.mozilla.org/show_bug.cgi?id=363997 for more information
	//
	if (IsSystemWinVista())
	{
		WSA_COMPATIBILITY_MODE mode;
		char dummy[4];
		DWORD ret_dummy;
		
		mode.BehaviorId = WsaBehaviorAutoTuning;
		mode.TargetOsVersion = NTDDI_VISTA;

		if (WSAIoctl(socket, SIO_SET_COMPATIBILITY_MODE, (char *)&mode, sizeof(mode), dummy, 4, &ret_dummy, 0, NULL) == SOCKET_ERROR)
		{
			// if error, we should maybe close the socket and return an error here?  But let's test a bit first...
			DWORD error = ::GetLastError();
			if(error)
			{
//				return WinErrToOpStatus(error);
			}
		}
	}
	return OpStatus::OK;
}

extern BOOL IsSystemWinVista();

// TODO: Make sure that the constructors functionality fit with the new API (regarding returning/leaving and so fort....)
// TODO: Make sure that the destructor does the necessary cleanup formerly done by the destroy function in the factory.
// TODO: Make sure that whatever initialization previously done in the factory constructor is not just dropped, but moved somewhere appropriate.

WindowsSocketManager* WindowsSocket2::m_manager = NULL; // Initialize the static member.

OP_STATUS OpSocket::Create(OpSocket** socket, OpSocketListener* listener, BOOL secure)
{
	if (!(WindowsSocket2::m_manager))
	{
		WindowsSocketManager* manager = new WindowsSocketManager();
		WindowsSocket2::m_manager = manager;
		if(manager)
		{
			RETURN_IF_ERROR(manager->Init());
		}
	}
	(*socket) =  new WindowsSocket2(listener); // TODO: Should the secure flag be used????

	return (*socket) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* Winsock2 implementation */
WindowsSocket2::WindowsSocket2(OpSocketListener* const aObserver)
							:iObserver(aObserver)
							,iSocket(INVALID_SOCKET)
							,iLocalAddress(NULL)
							,iSocketAddress(NULL)
							,own_address(FALSE)
							,iCurrentLen(0)
							,iCurrentRestLen(0)
							,iClosing(FALSE)
							,iPaused(FALSE)
{

}

WindowsSocket2::~WindowsSocket2()
{
	// Unset all callbacks
	g_main_message_handler->UnsetCallBacks(this);

	if (iSocket != INVALID_SOCKET)
	{
		WSAAsyncSelect(iSocket, g_main_hwnd, 0, 0);
		/*int error =*/ closesocket(iSocket);

#ifdef _DEBUG
		// If debugging of closesocket() is really essential,
		// uncomment the next line:
		// HandleError(error);
#endif // _DEBUG
		iSocket = INVALID_SOCKET;
	}
	if (own_address)
		OP_DELETE(iSocketAddress);
	OP_DELETE(iLocalAddress);
}

OP_STATUS WindowsSocket2::SetSocket(OpSocketAddress *socket_address)
{
	// Only create a new socket if needed.
	// This assumes that we keep the same IP Family
	if(iSocket != INVALID_SOCKET)
		return OpStatus::OK;

	int type = SOCK_STREAM;
	int proto;

	if(socket_address)
	{
		WindowsSocketAddress2 *aSocketAddress2 = (WindowsSocketAddress2 *) socket_address;
		LPSOCKET_ADDRESS aAddress = (LPSOCKET_ADDRESS) aSocketAddress2->Address();

		iDomain = aAddress->lpSockaddr->sa_family;
	}
	else
		iDomain = AF_INET;

	// Hopefully, we should not need to specify
	// the protocol when type is set correctly.
	proto = 0;

	//iSocket = WSASocket(iDomain,type,proto, Winsock2GetProtocolInfo(iDomain), 0, 0);
	iSocket = socket(iDomain,type,proto);

#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%p] WindowsSocket2::SetSocket(): Created socket %lu\n", this, iSocket);
#endif

	if (iSocket == INVALID_SOCKET)
	{
		return HandleError(WSAGetLastError(), EConnect);
	} 
	else 
	{
		OpStatus::Ignore(FixVistaSocket(iSocket));
		return OpStatus::OK;
	}
}

OP_STATUS WindowsSocket2::Connect(OpSocketAddress* socket_address)
{
	OP_DELETE(iLocalAddress);
	iLocalAddress = NULL;

	iSocketAddress = (WindowsSocketAddress2*)socket_address;
	RETURN_IF_ERROR(SetSocket(iSocketAddress));

    g_main_message_handler->SetCallBack(this, WINSOCK_CONNECT_READY, iSocket);

	// NOTE: The callback message registered with winsock can NOT be the same message as the OpMessage used in the Opera message loop,
	// because of reserved message ranges in Windows. It is necessary to use a message from the WindowsMessage enum for the WinSock
	// callback, and explicitly map the message back to the corresponding OpMessage when the callback arrives and the message is posted
	// in the Opera message queue.
	if (WSAAsyncSelect(iSocket, g_main_hwnd, WM_WINSOCK_CONNECT_READY, FD_CONNECT) != 0)
	{
		return HandleError(WSAGetLastError(), EConnect);
	}

#ifdef DEBUG_COMM_FILE
	{
		OpString tempip;

		iSocketAddress->ToString(&tempip);
		PrintfTofile("winsck.txt","[%p] WindowsSocket2::Connect(): connecting to %s\n", this, make_singlebyte_in_tempbuffer(tempip.CStr(), tempip.Length()));
	}
#endif
	LPSOCKET_ADDRESS aAddress = (LPSOCKET_ADDRESS) iSocketAddress->Address();
	
	WSAHtons(iSocket, iSocketAddress->Port(), (aAddress->lpSockaddr->sa_family == AF_INET ? 
		&((sockaddr_gen *) aAddress->lpSockaddr)->AddressIn.sin_port :
		&((sockaddr_gen *) aAddress->lpSockaddr)->AddressIn6.sin6_port));

	//if (WSAConnect(iSocket, aAddress->lpSockaddr,aAddress->iSockaddrLength, NULL, NULL, NULL, NULL) == 0)
	if (connect(iSocket, aAddress->lpSockaddr,aAddress->iSockaddrLength) == 0)
	{
		return OpStatus::OK;
	}
	else 
	{
		if(WSAGetLastError() == WSAEWOULDBLOCK)
			return OpStatus::OK;

		return HandleError(WSAGetLastError(), EConnect);
	}
}

OP_STATUS WindowsSocket2::SocketSetLocalPort(UINT port)
{
	sockaddr_in localAddr = { 0 };
	int error;

	RETURN_IF_ERROR(SetSocket(NULL));

	// Set up the sockaddr structure
	localAddr.sin_family = iDomain;
	localAddr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces
	localAddr.sin_port = htons(port);

	BOOL optval = TRUE;
	setsockopt(iSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&optval, sizeof(optval));
	error = bind(iSocket,(SOCKADDR*)&localAddr,sizeof(localAddr));
	if (error)
		return HandleError(WSAGetLastError(), EListen);
	else
		return OpStatus::OK;
}


 /* Windows Implementation details:
  * SendTo only makes sense on UDP sockets. With Windows sendto(), 
  * the destination address and length are _ignored_, making it 
  * equivalent to send().
  * SendTo over UDP does _not_ guarantee delivery.
  */
OP_STATUS WindowsSocket2::SendTo(const void* data, UINT length, OpSocketAddress* socket_address)
{	
	return Send(data, length);
}

OP_STATUS WindowsSocket2::RecvFrom(void* buffer, UINT length, OpSocketAddress* socket_address, UINT* bytes_received)
{
	return Recv(buffer, length, bytes_received);
}

OP_STATUS WindowsSocket2::Send(const void* data, UINT length)
{
	// Block if not all data is sent
	if (iCurrentRestLen)
	{
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%p] OpSocket::Send(): blocked adata=%p length=%d\n", this, data, iCurrentRestLen);
#endif
		iObserver->OnSocketSendError(this, OpSocket::SOCKET_BLOCKING);
		return OpStatus::ERR;
	}
	int trial_count =0;

#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%p] OpSocket::Send(): adata=%p aLength=%d\n", this, data, length);
#endif
	iCurrentLen = length;
	iCurrentRestLen = length;
	iCurrentData = (char *)data; // potentially memcpy this to avoit trouble
	iCurrentRestData = (char *)data;

retry_send:;
	unsigned int bytesSent =0;
	// First, try to send the data directly:
	if(SendToSocket((const char*)data, length, bytesSent))
	{
		iObserver->OnSocketDataSendProgressUpdate(this, bytesSent);
		if (bytesSent == (int)length)
		{
			iCurrentRestLen = 0;
			iObserver->OnSocketDataSent(this, length);
			return OpStatus::OK;
		}
		else
		{
			iCurrentRestLen = length - bytesSent;
			iCurrentRestData = (char *)data + bytesSent;
			SendMore();
			return OpStatus::OK;
		}
	}
	else
	{
		int error = WSAGetLastError();

		if(error == WSAENOTCONN)
		{
			if((++trial_count) <4)
			{
				Sleep(1);
				goto retry_send;
			}
			return OpStatus::ERR;
		}
		if(error == WSAEWOULDBLOCK)
		{
			return OpStatus::OK; // Not care about wouldblock for send until it is a problem.
		}
		return HandleError(error, ESend);
	}
}

void WindowsSocket2::SendMore()
{
	int trial_count =0;

	do
	{
		unsigned int bytesSent =0;
		// First, try to send the data directly:
#ifdef DEBUG_COMM_FILE
		PrintfTofile("winsck.txt","[%p] OpSocket::SendMore(): rest_len=%d\n", this, iCurrentRestLen);
#endif
		if(SendToSocket((const char*)iCurrentRestData,iCurrentRestLen, bytesSent))
		{
			iObserver->OnSocketDataSendProgressUpdate(this, bytesSent);
			if (bytesSent == (int)iCurrentRestLen)
			{
				iCurrentRestLen = 0;
				iObserver->OnSocketDataSent(this, iCurrentLen);
				break;
			}
			else
			{
				iCurrentRestLen -= bytesSent;
				iCurrentRestData += bytesSent;
				trial_count = 0;
				continue;
			}
		}
		else
		{
			int error = WSAGetLastError();
			if(error == WSAENOTCONN)
			{
				if((++trial_count) <4)
				{
					Sleep(1);
					continue;
				}
			}
			if (error != WSAEWOULDBLOCK && error != WSAENOTCONN) 
			{
				// Serious error should be reported.
				HandleError(error, ESend);
			}
			else 
			{
				// Would block, so wait for next occasion to send data:
			}
			break;
		}
	} while(1);
}


BOOL WindowsSocket2::SendToSocket(const void* aData, unsigned int aLength, unsigned int &bytesSent)
{
	OP_ASSERT(aData != NULL);
	OP_ASSERT(aLength != NULL);
	OP_ASSERT(iSocket != INVALID_SOCKET);

	DWORD bytesSent0 = 0;
	WSABUF buffer;

	buffer.buf = (char *) aData;
	buffer.len = aLength;
	int error = WSASend(iSocket, &buffer, 1, &bytesSent0, 0, NULL, NULL);
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%p] WindowsSocket2::Send(): %d bytes\n", this, bytesSent0);
	BinaryDumpTofile((unsigned char *) aData, bytesSent0, "winsck1.txt");
#endif
	bytesSent = bytesSent0;
	return (error != SOCKET_ERROR ? TRUE : FALSE);
}

BOOL WindowsSocket2::ReceiveFromSocket(const void* aBuffer, unsigned int aLength, unsigned int &aBytesReceived)
{
	OP_ASSERT(aBuffer != NULL);
	OP_ASSERT(aLength != NULL);
	OP_ASSERT(iSocket != INVALID_SOCKET);

	WSABUF buffer;
	DWORD bytesReceived = 0;
	DWORD flags = 0;

	buffer.buf = (char *) aBuffer;
	buffer.len = aLength;

	int error = WSARecv(iSocket, &buffer, 1, &bytesReceived, &flags, NULL, NULL);

#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%p] WindowsSocket2::Recv(): %d bytes\n", this, bytesReceived);
#endif

	aBytesReceived = bytesReceived;

	return (error != SOCKET_ERROR ? TRUE : FALSE);
}

OP_STATUS WindowsSocket2::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	// If connection is paused, then do nothing
	if (iPaused)
		return OpStatus::OK;

	// If not paused, then start reading
	if(!ReceiveFromSocket(buffer, length, *bytes_received))
	{
		return iClosing ? OpStatus::OK : HandleError(WSAGetLastError(), EReceive);
	}
#ifdef DEBUG_COMM_FILE
	PrintfTofile("winsck.txt","[%p] OpSocket::Recv(): %d bytes\n", this, bytes_received);
#endif
	if (*bytes_received == 0 && !iClosing)
	{
		iObserver->OnSocketClosed(this);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS WindowsSocket2::Accept(OpSocket* socket)
{
	sockaddr_gen srvAddr;
	int addrLen = sizeof(srvAddr);
	WindowsSocket2 *winSocket = (WindowsSocket2*)socket;
	
	winSocket->iSocket = WSAAccept(iSocket,(sockaddr*)&srvAddr,&addrLen, NULL, NULL);

	if(winSocket->iSocket == INVALID_SOCKET)
	{
		return HandleError(WSAGetLastError(), EListen);
	}
	winSocket->iDomain = srvAddr.Address.sa_family;

	OP_DELETE(winSocket->iLocalAddress);
	winSocket->iLocalAddress = NULL;

	winSocket->iSocketAddress = OP_NEW(WindowsSocketAddress2, ());
	winSocket->own_address = TRUE;

	SOCKET_ADDRESS aAddress;
	unsigned short port=0;

	aAddress.lpSockaddr = (sockaddr *) &srvAddr;
	aAddress.iSockaddrLength = sizeof(srvAddr);
	winSocket->iSocketAddress->SetAddress(&aAddress);
	
	WSANtohs(iSocket, (srvAddr.Address.sa_family == AF_INET ? srvAddr.AddressIn.sin_port : srvAddr.AddressIn6.sin6_port), &port);
	winSocket->iSocketAddress->SetPort(port);

	// Included 13 mar 01, tord
	g_main_message_handler->SetCallBack(winSocket, WINSOCK_DATA_READY, winSocket->iSocket);

	// NOTE: The callback message registered with winsock can NOT be the same message as the OpMessage used in the Opera message loop,
	// because of reserved message ranges in Windows. It is necessary to use a message from the WindowsMessage enum for the WinSock
	// callback, and explicitly map the message back to the corresponding OpMessage when the callback arrives and the message is posted
	//in the Opera message queue.	
	if (WSAAsyncSelect(winSocket->iSocket, g_main_hwnd, WM_WINSOCK_DATA_READY, FD_WRITE|FD_READ |FD_CLOSE) != 0)
	{
		return HandleError(WSAGetLastError(), EListen);
	}
	return OpStatus::OK;
}

OP_STATUS WindowsSocket2::SocketListen(UINT queue_size)
{
	if (iSocket == INVALID_SOCKET)
		return OpStatus::ERR;

	g_main_message_handler->SetCallBack(this, WINSOCK_ACCEPT_CONNECTION, iSocket);
	if (WSAAsyncSelect(iSocket, g_main_hwnd, WM_WINSOCK_ACCEPT_CONNECTION, FD_ACCEPT) == 0)
	{
		int retval = listen(iSocket, queue_size);
		if(!retval)
			return OpStatus::OK;
	}
	return HandleError(WSAGetLastError(), EListen);
}


OP_STATUS WindowsSocket2::Listen(OpSocketAddress* socket_address, int queue_size)
{ 
	UINT port = socket_address->Port();
	

	RETURN_IF_ERROR(SocketSetLocalPort(port));
	RETURN_IF_ERROR(SocketListen(queue_size));

	return OpStatus::OK;
}

#ifdef AUTO_UPDATE_SUPPORT
// Implementation of API_URL_TCP_PAUSE_DOWNLOAD

/***********************************************************************************
**
**	PauseRecv()
**  - Pauses a socket untill ContinueRecv() is called
**
***********************************************************************************/
OP_STATUS WindowsSocket2::PauseRecv()
{
	
	iPaused = TRUE;
	return OpStatus::OK;
	
}

/***********************************************************************************
**
**	ContinueRecv()
**  - Resumes a socket that was paused using PauseRecv()
**
***********************************************************************************/
OP_STATUS WindowsSocket2::ContinueRecv()
{
	iPaused = FALSE;
	g_main_message_handler->PostMessage(WINSOCK_DATA_READY, iSocket, FD_READ);
	
	return OpStatus::OK;
}
#endif // AUTO_UPDATE_SUPPORT


OP_STATUS WindowsSocket2::GetSocketAddress(OpSocketAddress* socket_address)
{
	return socket_address->Copy(iSocketAddress);
}

OP_STATUS WindowsSocket2::GetLocalSocketAddress(OpSocketAddress* socket_address)
{
	if (!iLocalAddress)
	{
		if (iSocket == INVALID_SOCKET)
			return OpStatus::ERR;

		sockaddr local_sockaddr;
		SOCKET_ADDRESS local_socket_address = { &local_sockaddr, sizeof(local_sockaddr) };
		if (getsockname(iSocket, local_socket_address.lpSockaddr, &local_socket_address.iSockaddrLength) != 0)
			return OpStatus::ERR;

		iLocalAddress = OP_NEW(WindowsSocketAddress2, ());
		RETURN_OOM_IF_NULL(iLocalAddress);
		OP_STATUS status = iLocalAddress->SetAddress(&local_socket_address);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(iLocalAddress);
			iLocalAddress = NULL;
			return status;
		}
	}
	return socket_address->Copy(iLocalAddress);
}

void WindowsSocket2::Close()
{
	g_main_message_handler->SetCallBack(this, WINSOCK_DATA_READY, iSocket);
	if (WSAAsyncSelect(iSocket, g_main_hwnd, WM_WINSOCK_DATA_READY, FD_WRITE|FD_READ |FD_CLOSE) != 0) // NOTE: The callback message registered with winsock can NOT be the same message as the OpMessage used in the Opera message loop, because of reserved message ranges in Windows. It is necessary to use a message from the WindowsMessage enum for the WinSock callback, and explicitly map the message back to the corresponding OpMessage when the callback arrives and the message is posted in the Opera message queue.
	{
		HandleError(WSAGetLastError(), EClose);
	} else {
		shutdown(iSocket, 0x01 /*SD_SEND*/);
	}
}

void WindowsSocket2::HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
	int error = WSAGETSELECTERROR(lParam);
	int event = WSAGETSELECTEVENT(lParam);

	/*
	if (error != 0)
	{
	//HandleError(error, EConnect);
	return;
	}
	*/
	if (msg == WINSOCK_DATA_READY)
	{
		if (event == FD_WRITE)
		{
			if(error != 0)
			{
				HandleError(error, ESend);
				return;
			}
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%p] OpSocket::HandleCallback(): FD_WRITE, rest_len=%d wsa_err=%d\n", this, iCurrentRestLen, error);
#endif
			if(iCurrentRestLen && !iClosing)
				SendMore();
			return;
		}
		if (event == FD_CLOSE)
		{
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%p] OpSocket::HandleCallback(): FD_CLOSE, wsa_err=%d\n", this, error);
#endif
			if (error != 0)
			{
				HandleError(error, EConnect);
				return;
			}

			iClosing = TRUE;
			iObserver->OnSocketDataReady(this);
			WSAAsyncSelect(iSocket, g_main_hwnd, 0, 0);
			closesocket(iSocket);
			iSocket = INVALID_SOCKET;

			iObserver->OnSocketClosed(this);
		}
		// don't attempt to read from an invalid socket
		if (event == FD_READ && iSocket != INVALID_SOCKET)
		{
			if(error != 0)
			{
				HandleError(error, EReceive);
				return;
			}
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%p] OpSocket::HandleCallback(): FD_READ, wsa_err=%d\n", this, error);
#endif
			if (!iClosing)
				iObserver->OnSocketDataReady(this);
			return;
		}
		return;
	}
	else if (msg == WINSOCK_ACCEPT_CONNECTION)
	{
		if (error != 0)
		{
			//HandleError(error, EListen);
			return;
		}
		if (event == FD_ACCEPT)
		{
			iClosing = FALSE;
			iObserver->OnSocketConnectionRequest(this);
		}
		return;
	}
	else if (msg == WINSOCK_CONNECT_READY)
	{
		if (error != 0)
		{
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%p] OpSocket::HandleCallback(): WINSOCK_CONNECT_READY, wsa_err=%d\n", this, error);
#endif
			HandleError(error, EConnect);
			return;
		}
		if (event == FD_CONNECT)
		{
			g_main_message_handler->UnsetCallBack(this,WINSOCK_CONNECT_READY);
			g_main_message_handler->SetCallBack(this, WINSOCK_DATA_READY, iSocket);
			if (WSAAsyncSelect(iSocket, g_main_hwnd, WM_WINSOCK_DATA_READY, FD_WRITE|FD_READ |FD_CLOSE) != 0) // NOTE: The callback message registered with winsock can NOT be the same message as the OpMessage used in the Opera message loop, because of reserved message ranges in Windows. It is necessary to use a message from the WindowsMessage enum for the WinSock callback, and explicitly map the message back to the corresponding OpMessage when the callback arrives and the message is posted in the Opera message queue.
			{
				HandleError(WSAGetLastError(), EConnect);
				return;
			}
#ifdef DEBUG_COMM_FILE
			PrintfTofile("winsck.txt","[%p] OpSocket::HandleCallback(): FD_CONNECT, wsa_err=%d\n", this, error);
#endif
			iClosing = FALSE;
			iObserver->OnSocketConnected(this);
		}
		return;
	}
	return;
}

void WindowsSocket2::SocketError(OpSocket::Error aSocketErr, ESocketErrorType aErrorType)
{
	switch(aErrorType)
	{
	case EListen:
		iObserver->OnSocketListenError(this, aSocketErr);
		break;
	case EConnect:
		iObserver->OnSocketConnectError(this, aSocketErr);
		break;
	case EReceive:
		iObserver->OnSocketReceiveError(this, aSocketErr);
		break;
	case ESend:
		iObserver->OnSocketSendError(this, aSocketErr);
		break;
	case EClose:
		iObserver->OnSocketCloseError(this, aSocketErr);
		break;
	default:
		iObserver->OnSocketConnectError(this, aSocketErr);
		break;
	}
}

OP_STATUS WindowsSocket2::HandleError(int socketError, ESocketErrorType aErrorType)
{
	if(socketError == 0)
		return OpStatus::OK;
	switch(socketError)
	{
	case WSAEWOULDBLOCK:	// The socket is marked as nonblocking and the connection cannot be completed immediately. 
		SocketError(SOCKET_BLOCKING, aErrorType);
		return OpStatus::ERR;

	case WSAEINPROGRESS:	// A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function. 
		SocketError(SOCKET_BLOCKING, aErrorType);
		return OpStatus::ERR;

	case WSAECONNABORTED:
	case WSAEINTR:			// The (blocking) Windows Socket 1.1 call was canceled through WSACancelBlockingCall. 
		SocketError(CONNECTION_CLOSED, aErrorType);
		return OpStatus::ERR;

	case WSAECONNREFUSED:	// The attempt to connect was forcefully rejected. 
		SocketError(CONNECTION_REFUSED, aErrorType);
		return OpStatus::ERR;

	case WSAEMSGSIZE:		// The buffer isn't large enough!

	case WSAENETDOWN:		// The network subsystem has failed. 
	case WSAENETUNREACH:	// The network cannot be reached from this host at this time. 
		SocketError(NETWORK_ERROR, aErrorType);
		return OpStatus::ERR;

	case WSAETIMEDOUT:		// Attempt to connect timed out without establishing a connection. 
		SocketError(CONNECT_TIMED_OUT, aErrorType);
		return OpStatus::ERR;

	case WSAEADDRINUSE:		// The socket's local address is already in use and the socket was not marked to allow address reuse with SO_REUSEADDR. This error usually occurs when executing bind, but could be delayed until this function if the bind was to a partially wild-card address (involving ADDR_ANY) and if a specific address needs to be committed at the time of this function. 
		SocketError(ADDRESS_IN_USE, aErrorType);
		return OpStatus::ERR;

	case WSAECONNRESET:		// Connection reset by peer
		iObserver->OnSocketClosed(this);
		return OpStatus::ERR;

	case WSAEALREADY:		// A nonblocking connect call is in progress on the specified socket.
	case WSAENOTSOCK:		// The descriptor is not a socket. 
	case WSAENOBUFS:		// No buffer space is available. The socket cannot be connected. 
	case WSAEISCONN:		// The socket is already connected (connection-oriented sockets only). 
	case WSAEINVAL:			// The parameter s is a listening socket, or the destination address specified is not consistent with that of the constrained group the socket belongs to. 
	case WSAEFAULT:			// The name or the namelen parameter is not a valid part of the user address space, the namelen parameter is too small, or the name parameter contains incorrect address format for the associated address family. 
	case WSAEAFNOSUPPORT:	// Addresses in the specified family cannot be used with this socket. 
	case WSAEADDRNOTAVAIL:	// The remote address is not a valid address (such as ADDR_ANY). 
	case WSAEACCES:			// Attempt to connect datagram socket to broadcast address failed because setsockopt option SO_BROADCAST is not enabled.
	case WSANOTINITIALISED:	// A successful WSAStartup must occur before using this function. 
	case WSAEHOSTUNREACH:   // Host could not be reached // Actually unused in Winsock
	case WSAEHOSTDOWN:		// Host could not be reached // Actually unused in Winsock
		SocketError(CONNECTION_FAILED, aErrorType);
		return OpStatus::ERR;
		//	case OUT_OF_MEMORY: // pseudo code
		// If you want to handle out-of-memory, this may be a suitable location to do it (mstensho). 
		// On UNIX the error values (errno) ENOBUFS and ENOMEM represent an out-of-memory condition.
		// return OpStatus::ERR_NO_MEMORY;
	default:
		SocketError(NETWORK_ERROR, aErrorType);
		return OpStatus::ERR;
	}
}


BOOL WindowsSocketLibrary::m_IsWinSock2 = FALSE;
int WindowsSocketLibrary::m_Winsock2ProtocolCount = 0;
#ifdef __SUPPORT_IPV6_REMOVED
BOOL WindowsSocketLibrary::m_HasIPv6= FALSE;
#endif

LPAFPROTOCOLS WindowsSocketLibrary::m_Winsock2Protocols = NULL;

#ifdef __SUPPORT_IPV6_REMOVED
LPWSAPROTOCOL_INFO *WindowsSocketLibrary::m_Winsock2ProtcolInfo = NULL;
#endif

static INT Winsock2SupportedProtocols[] = {
	AF_INET,
#ifdef __IPV6_SUPPORTED
	AF_INET6,
#endif
	0
};

#if defined CHECK_ENUM_PROTO
static INT Winsock2SupportedProtocols2[] = {
	IPPROTO_TCP,
	0
};
#endif

#define WINSOCK2_PROTOCOLCOUNT ARRAY_SIZE(Winsock2SupportedProtocols) 

static AFPROTOCOLS Winsock2ProtocolList[WINSOCK2_PROTOCOLCOUNT-1];

/**
 *	Points into Winsock2ProtocolInfo array 
 *	Ordered as specified by Winsock2ProtocolList
 */
static LPWSAPROTOCOL_INFO Winsock2Protocols[WINSOCK2_PROTOCOLCOUNT-1];

#if defined __SUPPORT_IPV6_REMOVED
static WSAPROTOCOL_INFO Winsock2ProtocolInfo[WINSOCK2_PROTOCOLCOUNT-1*/];
#elif defined CHECK_ENUM_PROTO
static WSAPROTOCOL_INFO Winsock2ProtocolInfo[32];
#endif

void WindowsSocketLibrary::InitWinsock2()
{
#ifdef CHECK_ENUM_PROTO
	int count,i;
	DWORD list_size = sizeof(Winsock2ProtocolInfo);

	count = WSAEnumProtocols(Winsock2SupportedProtocols2, Winsock2ProtocolInfo, &list_size);
	if(count == SOCKET_ERROR)
	{
		OP_ASSERT(0);
	}

	LPWSANAMESPACE_INFO info = (LPWSANAMESPACE_INFO) Winsock2ProtocolInfo;
	list_size = sizeof(Winsock2ProtocolInfo);

	count = WSAEnumNameSpaceProviders(&list_size, info);
	if(count == SOCKET_ERROR)
	{
		OP_ASSERT(0);
	}


#endif
#ifdef __SUPPORT_IPV6_REMOVED
	int count,i;
	DWORD list_size = sizeof(Winsock2ProtocolInfo);

	count = WSAEnumProtocols(Winsock2SupportedProtocols, Winsock2ProtocolInfo, &list_size);
	if(count == SOCKET_ERROR)
	{
		m_IsWinSock2 = FALSE;
		return;
	}

	m_Winsock2ProtocolCount = 0;
	for(i = 0; i< count && m_Winsock2ProtocolCount < ARRAY_SIZE(Winsock2ProtocolList); i++)
	{
		Winsock2ProtocolList[m_Winsock2ProtocolCount].iAddressFamily = Winsock2ProtocolInfo[i].iAddressFamily;
		Winsock2ProtocolList[m_Winsock2ProtocolCount].iProtocol = Winsock2ProtocolInfo[i].iProtocol;
		Winsock2Protocols[m_Winsock2ProtocolCount] = &Winsock2ProtocolInfo[i];
		m_Winsock2ProtocolCount ++;

		if(Winsock2ProtocolInfo[i].iAddressFamily == AF_INET6)
			m_HasIPv6 = TRUE;
	}

	if(m_Winsock2ProtocolCount == 0)
	{
		Winsock2ProtocolList[m_Winsock2ProtocolCount].iAddressFamily = AF_INET;
		Winsock2ProtocolList[m_Winsock2ProtocolCount].iProtocol = IPPROTO_TCP;
		Winsock2Protocols[m_Winsock2ProtocolCount] = NULL;
		m_Winsock2ProtocolCount ++;
	}

	m_Winsock2Protocols = Winsock2ProtocolList;
	m_Winsock2ProtcolInfo = Winsock2Protocols;
#else
	m_Winsock2ProtocolCount = 0;
	Winsock2ProtocolList[m_Winsock2ProtocolCount].iAddressFamily = AF_INET;
	Winsock2ProtocolList[m_Winsock2ProtocolCount].iProtocol = IPPROTO_TCP;
	Winsock2Protocols[m_Winsock2ProtocolCount] = NULL;
	m_Winsock2ProtocolCount ++;
#ifdef __IPV6_SUPPORTED
	Winsock2ProtocolList[m_Winsock2ProtocolCount].iAddressFamily = AF_INET6;
	Winsock2ProtocolList[m_Winsock2ProtocolCount].iProtocol = IPPROTO_TCP;
	Winsock2Protocols[m_Winsock2ProtocolCount] = NULL;
	m_Winsock2ProtocolCount ++;
#endif
	m_Winsock2Protocols = Winsock2ProtocolList;
#endif

}

LPAFPROTOCOLS WindowsSocketLibrary::GetProtocolIDs(DWORD &count)
{
	count = m_Winsock2ProtocolCount;
	return m_Winsock2Protocols;
}

LPWSAPROTOCOL_INFO WindowsSocketLibrary::GetProtocolInfo(int a_Protocol)
{
#ifdef __SUPPORT_IPV6_REMOVED
	int i;
	for(i = 0; i< m_Winsock2ProtocolCount; i++)
	{
		if(m_Winsock2Protocols[i].iAddressFamily == a_Protocol)
		{
			return m_Winsock2ProtcolInfo[i];
		}
	}
#endif

	return NULL;
}

WindowsSocketManager::WindowsSocketManager()
{
}

WindowsSocketManager::~WindowsSocketManager()
{

}

OP_STATUS WindowsSocketManager::Init()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 )
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	if(wsaData.wHighVersion >= 2)
	{
		WindowsSocketLibrary::InitWinsock2();
	}
	else
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	return OpStatus::OK;
}

#ifdef OPUDPSOCKET

OP_STATUS OpUdpSocket::Create(OpUdpSocket** new_object, OpUdpSocketListener* listener, OpSocketAddress* local_address)
{
	if (!(WindowsSocket2::m_manager))
	{
		WindowsSocketManager* manager = new WindowsSocketManager();
		WindowsSocket2::m_manager = manager;
		if(manager)
		{
			RETURN_IF_ERROR(manager->Init());
		}
	}
	*new_object = new WindowsUdpSocket(listener, local_address);

	if(!*new_object)
		return OpStatus::ERR_NO_MEMORY;

	return ((WindowsUdpSocket *)(*new_object))->Init();
}

WindowsUdpSocket::WindowsUdpSocket(OpUdpSocketListener* listener, OpSocketAddress* local_address): 
m_listener(listener),
m_socket(INVALID_SOCKET),
m_local_port_bound(0),
last_broadcast_option(0)
{
	if(local_address)
	{
		m_local_address.Copy(local_address);
	}
}

WindowsUdpSocket::~WindowsUdpSocket()
{
	g_main_message_handler->UnsetCallBacks(this);

	if (m_socket != INVALID_SOCKET)
	{
		::WSAAsyncSelect(m_socket, g_main_hwnd, 0, 0);
		::shutdown(m_socket, SD_SEND | SD_RECEIVE);
		::closesocket(m_socket);
	}
}

OP_STATUS WindowsUdpSocket::Init()
{
	if(m_socket!=INVALID_SOCKET)
		return OpStatus::OK;

	m_socket = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _DEBUG
//	DWORD error = WSAGetLastError();
#endif 

	if (m_socket == INVALID_SOCKET)
		return OpStatus::ERR;

	g_main_message_handler->SetCallBack(this, WINSOCK_DATA_READY, m_socket);

	if (::WSAAsyncSelect(m_socket, g_main_hwnd, WM_WINSOCK_DATA_READY, FD_READ) != 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_NETWORK_STATUS WindowsUdpSocket::Bind(OpSocketAddress* socket_address, OpUdpSocket::RoutingScheme routing_scheme)
{
	OP_ASSERT(m_socket!=INVALID_SOCKET);

	if(m_socket==INVALID_SOCKET)
		return OpStatus::ERR;

	// It is not clear if Bind() for Broadcast makes sense... In any case, Windows does not accept 255.255.255.255 as a valid bind address
	if(routing_scheme==OpUdpSocket::UNICAST || routing_scheme==OpUdpSocket::BROADCAST)
	{
//		OP_ASSERT(!m_local_port_bound);
		OP_ASSERT(socket_address);

		if(m_local_port_bound)
			return OpStatus::ERR;  // Respect the OpUdpSocket contract

		// Bind
		WindowsSocketAddress2* address = (WindowsSocketAddress2*)socket_address;
/*
		if (routing_scheme == BROADCAST) 
		{ 
			char hname[1024]; 
			struct hostent  *host; 
			int err = gethostname(hname, 1024); 
			if (err == 0) 
			{ 
				host = gethostbyname(hname); 
				if (host && host->h_addr_list[0]) 
				{ 
					host->h_addr_list[0][3] = 255; 
				} 
			} 
		} 
*/
		BOOL optval = TRUE;
		setsockopt(m_socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&optval, sizeof(optval));

		return InternalBind(address, address->Port());
	}
	else if(routing_scheme==OpUdpSocket::MULTICAST)
	{
		OP_ASSERT(!m_local_port_bound || m_local_port_bound==socket_address->Port());

		int val = 1;

		if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int)) == SOCKET_ERROR)
			return OpStatus::ERR;

		// Bind (required for multicast subscribing)
		if(!m_local_port_bound)
			RETURN_IF_ERROR(InternalBind(m_local_address.IsValid() ? &m_local_address : NULL, socket_address->Port()));

		// Multicast subscribe
		struct ip_mreq mreq;

		op_memset(&mreq,0,sizeof(mreq));

		mreq.imr_multiaddr.s_addr= inet_addr(((WindowsSocketAddress2 *)socket_address)->ToString8());
		mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

		if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) == SOCKET_ERROR)
		{
#ifdef _DEBUG
			DWORD win_error = WSAGetLastError();
			win_error = win_error;
#endif // _DEBUG
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_NETWORK_STATUS WindowsUdpSocket::Send(const void* data, unsigned int length, OpSocketAddress* socket_address, OpUdpSocket::RoutingScheme routing_scheme)
{
	OP_ASSERT(length>0);

	if(m_socket==INVALID_SOCKET)
		return OpStatus::ERR;

	if(!socket_address)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Manage broadcast
	int val = (routing_scheme == OpUdpSocket::BROADCAST) ? 1 : 0;

	if(val != last_broadcast_option)
	{
		if (::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (const char *)&val, sizeof(int)) == SOCKET_ERROR)
			return OpStatus::ERR;   

		last_broadcast_option = val;
	}

	// Populate the native socket address
	struct sockaddr_in destination_address;

	destination_address.sin_family = AF_INET;
	destination_address.sin_port = htons((unsigned short)socket_address->Port());
	destination_address.sin_addr.s_addr = inet_addr(((WindowsSocketAddress2 *)socket_address)->ToString8());

	// Send the data
	const char* d = (const char*) data;
	int res = ::sendto(m_socket, d, length, 0, (sockaddr*)&destination_address, sizeof(destination_address));

	if (res == (int)length)
	{
		return OpStatus::OK;
	}

	// Let's hope that a partial send cannot happen... even if M$ documentation says that it could...
	OP_ASSERT(res<=0);

	if(res<0 && WSAGetLastError() == WSAEWOULDBLOCK)
		return OpNetworkStatus::ERR_SOCKET_BLOCKING;

	// No support for partial data sent!

	return OpStatus::ERR;
}

OP_NETWORK_STATUS WindowsUdpSocket::Receive(void* buffer, unsigned int length, OpSocketAddress* socket_address, unsigned int* bytes_received)
{
	if(m_socket==INVALID_SOCKET)
		return OpStatus::ERR;

	OP_ASSERT(buffer);
	OP_ASSERT(bytes_received);

	WindowsSocketAddress2* iSocketAddress = (WindowsSocketAddress2 *)socket_address;
	struct sockaddr_in caller_address; // TODO: add two methods to OpSocket to get the IP Address and the Port of the UDP caller
	int size = sizeof(caller_address);

	int b = ::recvfrom(m_socket, (char *)buffer, length, 0, (sockaddr*)&caller_address, &size);

	if (b > 0)
	{
		*bytes_received = b;

		// Set the socket address of the caller
		if(socket_address)
		{
			SOCKET_ADDRESS aAddress;
			sockaddr_gen srvAddr;

			op_memcpy(&srvAddr.AddressIn, &caller_address, sizeof(caller_address));
			aAddress.lpSockaddr = (sockaddr *) &srvAddr;
			aAddress.iSockaddrLength = sizeof(caller_address);

			iSocketAddress->SetAddress(&aAddress);
			iSocketAddress->SetPort(ntohs(caller_address.sin_port));
		}
		return OpStatus::OK;
	}
	*bytes_received=0;

	DWORD win_error = WSAGetLastError();
/*
#ifdef _DEBUG
	char buf[1024];

	sprintf(buf, "WindowsUdpSocket::Receive() error. Address: %s, error: %d, buflen: %d, temp_received: %d\n", 
		iSocketAddress->ToString8(), win_error, length, b);

	OutputDebugStringA(buf);
#endif
*/
	if(win_error == WSAEWOULDBLOCK)
		return OpNetworkStatus::ERR_SOCKET_BLOCKING;

	return OpStatus::ERR;          
}

OP_NETWORK_STATUS WindowsUdpSocket::InternalBind(WindowsSocketAddress2  *address, UINT port)
{
	sockaddr_in localAddr = { 0 };

	// Set up the sockaddr structure
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY); // bind to all interfaces
	localAddr.sin_port = htons(port);

	if(address)
	{
		OpString address_string;

		if(OpStatus::IsSuccess(address->ToString(&address_string)))
		{
			if(address_string.Compare(UNI_L("255.255.255.255")))
			{
				// if different than 255.255.255.255, use it
				localAddr.sin_addr.s_addr = inet_addr(address->ToString8());
			}
		}
	}
	int error = bind(m_socket, (SOCKADDR *)&localAddr, sizeof(localAddr));

	if (error != 0)
	{
		error = WSAGetLastError();

		if(error==WSAEADDRINUSE)
			return OpNetworkStatus::ERR_ADDRESS_IN_USE;

		return OpStatus::ERR;
	}

	if(port)
	{
		m_local_port_bound=port;

		return OpStatus::OK;
	}

	// If the socket was not bound to a specific port, retrieve the port assigned by the OS
	struct sockaddr_in sa;
	int len=sizeof(sa);

	if(!getsockname(m_socket, (sockaddr *)&sa, &len))
	{
		m_local_port_bound=sa.sin_port;

		OP_ASSERT(m_local_port_bound);

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

void WindowsUdpSocket::HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
//	int error = WSAGETSELECTERROR(lParam);
	int socket_event = WSAGETSELECTEVENT(lParam);

	if(m_listener)
	{
		if (msg == WINSOCK_DATA_READY)
		{
			if (socket_event == FD_READ)
			{
				m_listener->OnSocketDataReady(this);
			}
			else if(socket_event == FD_WRITE)
			{
			}
		}
	}
}

#endif // OPUDPSOCKET

