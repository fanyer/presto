/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"
#include "modules/url/url_socket_wrapper.h"

#if defined(WIN32) && !defined(WIN_CE)
#include <sys/timeb.h>
#endif

#include "bt-util.h"
#include "bt-info.h"
#include "bt-upload.h"
#include "bt-client.h"
#include "bt-download.h"
#include "dl-base.h"
#include "bt-globals.h"
#include "connection.h"
#include "p2p-fileutils.h"
#include "bt-packet.h"

//#if !defined(_NO_GLOBALS_)
//extern MpSocketFactory* g_socket_factory;
//extern MpSocketAddressFactory* g_sockaddr_factory;
//#endif

#define CHECK_INTERVAL		10000	// used for checking if a connection can be taken down
#define CHECK_INTERVAL_BT	500		// used for checking if a new outgoing connections are needed

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::P2PConnection()"
#endif

P2PConnection::P2PConnection(OpP2PObserver& observer)
:
	m_connected(FALSE),
	m_connectedTime(0),
	m_initiated(FALSE),
	m_closing(FALSE),
	m_refcount(0),
	m_closedTime(0),
	m_bytes_received(0),
	m_bytes_sent(0),
	m_observer(&observer),
	m_recv_pause(FALSE),
	m_recv_ready(FALSE),
	m_input(NULL),
	m_last_buffer_sent(NULL),
	m_socket_address(NULL),
//	m_output(NULL),
	m_upload(FALSE)
{
	ENTER_METHOD;

	BT_RESOURCE_ADD("P2PConnection", this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::~P2PConnection()"
#endif

P2PConnection::~P2PConnection()
{
	ENTER_METHOD;

//	Observer().OnP2PDestructing(this);

	DEBUGTRACE8_SOCKET(UNI_L("DELETING SOCKET: 0x%08x\n"), m_socket.Get());

	if (m_socket_address)
	{
//			BT_RESOURCE_REMOVE(m_socket_address);
			OP_DELETE(m_socket_address);
	}

	OP_DELETE(m_input);

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::ConnectTo()"
#endif

OP_STATUS P2PConnection::ConnectTo(OpString& address, WORD port)
{
	ENTER_METHOD;

	if (address.IsEmpty())
		return OpStatus::ERR;

	m_input = OP_NEW(OpByteBuffer, ());
	if (!m_input)
		return OpStatus::ERR_NO_MEMORY;

	// Create a socket address to connect to.
	RETURN_IF_ERROR(OpSocketAddress::Create(&m_socket_address));

//	BT_RESOURCE_ADD("socket_address", m_socket_address);

	m_socket_address->FromString(address.CStr());
	if (!m_socket_address->IsValid())
		return OpStatus::ERR;

	m_socket_address->SetPort(port);

	// Create a socket.
	OpSocket *socket;
	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, this, 0));

	m_socket.Attach(socket);

//	BT_RESOURCE_ADD("socket", socket);

	RETURN_IF_ERROR(socket->Connect(m_socket_address));

	DEBUGTRACE_CONNECT(UNI_L("CONNECTING TO... %s\n"), (uni_char *)address);

	m_address.Set(address);

	LEAVE_METHOD;

	return OpStatus::OK;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::AttachTo()"
#endif

void P2PConnection::AttachTo(P2PConnection* connection)
{
	ENTER_METHOD;

	if(m_closing)
	{
		// we are closing, do nothing
		return;
	}
	OP_ASSERT(m_socket_address == 0);
	OP_ASSERT(connection != NULL);

	m_socket.Attach(connection->m_socket.Detach());
	m_socket_address = connection->m_socket_address;
	m_useragent.Set(connection->m_useragent);
	m_host.Set(connection->m_host);
	m_address.Set(connection->m_address);
	m_input = connection->m_input;
//	m_output = connection->m_output;
	m_observer = &connection->Observer();
	m_upload = connection->m_upload;

	SHAStruct guid;

	connection->GetGUID(&guid);
	SetGUID(&guid);

	connection->m_socket_address = NULL;
	connection->m_input	= NULL;
//	connection->m_output	= NULL;

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::AcceptFrom"
#endif

void P2PConnection::AcceptFrom(P2PSocket *socket, OpString& address)
{
	ENTER_METHOD;

	m_address.Set(address);
	m_socket.Attach(socket->Detach());
	m_input			= OP_NEW(OpByteBuffer, ());
	m_initiated		= FALSE;
	m_connected		= TRUE;
	m_connectedTime	= op_time(NULL);
	m_upload		= TRUE;

	DEBUGTRACE_CONNECT(UNI_L("ACCEPT: %s, socket: "), (uni_char *)m_address);
	DEBUGTRACE_CONNECT(UNI_L("0x%08x\n"), m_socket.Get());

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::OnSocketConnected()"
#endif

void P2PConnection::OnSocketConnected(OpSocket* socket)
{
	ENTER_METHOD;

	AddRef();

	if(m_closing)
	{
		// we are closing, do nothing
//		m_observer = &m_nullobserver;
		if(m_socket.Get() != NULL)
		{
//			m_socket.Get()->SetObserver(&m_nullobserver);
		}
		Release();
		return;
	}
	DEBUGTRACE_CONNECT(UNI_L("CONNECTED TO: %s, "), (uni_char *)m_address);
	DEBUGTRACE8_CONNECT(UNI_L("this: 0x%08x, socket: "), this);
	DEBUGTRACE8_CONNECT(UNI_L("0x%08x\n"), socket);

	m_initiated = TRUE;
	m_connectedTime = op_time(NULL);
	m_connected = TRUE;

	Observer().AddRef();
	Observer().OnP2PConnected(this);
	Observer().Release();

	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::SocketConnectErrorL()"
#endif

void P2PConnection::OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error)
{
	ENTER_METHOD;

	AddRef();

	if(m_closing == FALSE)
	{
		Observer().AddRef();
		Observer().OnP2PConnectError(this, socket_error);
		Observer().Release();
	}
	m_socket.Release();

	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::ReadData()"
#endif

void P2PConnection::ReadData()
{
	ENTER_METHOD;

	if(m_closing)
	{
		// we are closing, do nothing
//		m_observer = &m_nullobserver;
		if(m_socket.Get() != NULL)
		{
//			m_socket.Get()->SetObserver(&m_nullobserver);
		}
		m_recv_ready = FALSE;
		return;
	}
	unsigned int bytes_received = 0;
	BOOL datarecv = FALSE;
	unsigned int debug_received = 0;

	m_socket.Get()->Recv(m_read_buffer, READBUFFERSIZE, &bytes_received);

	while((int)bytes_received > 0)
	{
		m_bytes_received += bytes_received;
		debug_received += bytes_received;

		m_input->Append(m_read_buffer, bytes_received);

		datarecv = TRUE;

		// Read more data, if any.
		bytes_received = 0;
		m_socket.Get()->Recv(m_read_buffer, READBUFFERSIZE, &bytes_received);

		if(m_recv_pause)
		{
			break;
		}
	}
	if(datarecv)
	{
		if(m_upload && debug_received < 50)
		{
			DEBUGTRACE_UP(UNI_L("UL: Read %d bytes from host: "), debug_received);
			DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_address);
		}
		// Notify observer.
		if(!m_closing)
		{
			Observer().AddRef();
			if(Observer().OnP2PDataReceived(this, m_input))
			{
				// method returned TRUE, we can empty the buffer
				if(m_upload)
				{
					m_initiated = TRUE;
					DEBUGTRACE_UP(UNI_L("Processed %d bytes from host: "), debug_received);
					DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_address);
				}
				m_input->Empty();
			}
			Observer().Release();
		}
		m_recv_ready = FALSE;
	}
	LEAVE_METHOD;
}

void P2PConnection::OnSocketSendError(OpSocket* socket, OpSocket::Error socket_error)
{
	if(socket_error == OpSocket::SOCKET_BLOCKING)
	{
//		if(m_output->DataSize())
//		{
//			m_send_queue.Append(m_output->Buffer(), m_output->DataSize());
//		}
	}
	DEBUGTRACE8_SOCKET(UNI_L("**** SOCKET SEND ERROR: %d"), socket_error);
	DEBUGTRACE_SOCKET(UNI_L(", to: %s\n"), (uni_char *)m_address);
}

void P2PConnection::OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent)
{
//	OP_ASSERT(m_socket == socket);

	if(m_closing)
	{
		// we are closing, do nothing
		return;
	}
	AddRef();

	Observer().AddRef();
	Observer().OnP2PDataSent(this, m_last_buffer_sent, bytes_sent);
	Observer().Release();

	Release();
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::OnWrite()"
#endif

OP_STATUS P2PConnection::OnWrite(OpByteBuffer *sendbuffer)
{
	ENTER_METHOD;

	AddRef();

	if(m_closing)
	{
		// we are closing, do nothing
		Release();
//		m_observer = &m_nullobserver;
		if(m_socket.Get() != NULL)
		{
//			m_socket.Get()->SetObserver(&m_nullobserver);
		}
		return OpStatus::OK;
	}
	if(sendbuffer == NULL)
	{
//		OP_ASSERT(FALSE);
//		sendbuffer = m_output;
		return OpStatus::ERR_NULL_POINTER;
	}
	if(sendbuffer->DataSize() == 0)
	{
		Release();

		return OpStatus::OK;
	}
	OP_STATUS status;
	unsigned char* buffer;
	UINT32 buffersize;

	buffer = sendbuffer->Buffer();
	buffersize = sendbuffer->DataSize();

	if(m_upload)
	{
		DEBUGTRACE_UP(UNI_L("UL: WRITING %d bytes to host: "), buffersize);
		DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_address);
	}
	else
	{
		DEBUGTRACE(UNI_L("DL: WRITING %d bytes to host: "), buffersize);
		DEBUGTRACE(UNI_L("%s\n"), (uni_char *)m_address);
	}
	m_last_buffer_sent = buffer;
	status = m_socket.Get()->Send((void *)buffer, buffersize);
	if(status != OpStatus::OK)
	{
	}
	else
	{
	}

	LEAVE_METHOD;

	Release();

	return status;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::PauseReceive()"
#endif

void P2PConnection::PauseReceive(BOOL pause)
{
	m_recv_pause = pause;

	if(!m_recv_pause && m_recv_ready)
	{
		AddRef();

		if(m_closing)
		{
			// we are closing, do nothing
			Release();
//			m_observer = &m_nullobserver;
			if(m_socket.Get() != NULL)
			{
//				m_socket.Get()->SetObserver(&m_nullobserver);
			}
			return;
		}
		OP_ASSERT(m_input != NULL);

		ReadData();

		Release();
	}
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::SocketDataReadyL()"
#endif

void P2PConnection::OnSocketDataReady(OpSocket* socket)
{
	ENTER_METHOD;

	m_recv_ready = TRUE;

	AddRef();

	if(m_closing)
	{
		// we are closing, do nothing
		Release();
//		m_observer = &m_nullobserver;
		if(m_socket.Get() != NULL)
		{
//			m_socket.Get()->SetObserver(&m_nullobserver);
		}
		return;
	}
	OP_ASSERT(m_input != NULL);


	ReadData();

	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::OnSocketClosed()"
#endif

void P2PConnection::OnSocketClosed(OpSocket* socket)
{
	ENTER_METHOD;

	AddRef();

	m_connected = FALSE;

	if(m_closing == FALSE)
	{
		Observer().AddRef();
		Observer().OnP2PDisconnected(this);
		Observer().Release();
	}
	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::SocketReceiveErrorL()"
#endif

void P2PConnection::OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error)
{
	ENTER_METHOD;

	AddRef();

	// blocking is not an error
	if (m_closing == FALSE &&
		socket_error != OpSocket::SOCKET_BLOCKING)
	{
		Observer().AddRef();
		Observer().OnP2PReceiveError(this, socket_error);
		Observer().Release();
		StartClosing();
		m_socket.Release();
	}
	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::OnRun()"
#endif

BOOL P2PConnection::OnRun()
{
	ENTER_METHOD;

	AddRef();

	ReadData();

	Release();

	LEAVE_METHOD;

	return TRUE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::Close()"
#endif

void P2PConnection::Close()
{
	ENTER_METHOD;

	AddRef();

	if(m_socket.Get())
	{
		DEBUGTRACE_CONNECT(UNI_L("Closing socket to host: %s, refcount: "), (uni_char *)m_address);
		DEBUGTRACE_CONNECT(UNI_L("%d\n"), m_refcount);

		StartClosing();
		m_connected = FALSE;
//		m_socket.Get()->SetObserver(&m_nullobserver);
		DEBUGTRACE8_RES(UNI_L("DELETING SOCKET 2: 0x%08x\n"), m_socket.Get());
		m_socket.Release();
	}
	Release();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::OnRead()"
#endif

BOOL P2PConnection::OnRead()
{
	ENTER_METHOD;

	OnRun();

	LEAVE_METHOD;

	return TRUE;
}


#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PConnection::DoRun()"
#endif
/*
BOOL P2PConnection::DoRun()
{
	ENTER_METHOD;

	AddRef();

	if(m_socket.Get() == NULL)
	{
		BOOL success = OnRun();
		Release();

		return success;
	}
	if(!OnRun())
	{
		Release();
		return FALSE;
	}
	LEAVE_METHOD;

	Release();

	return TRUE;
}
*/

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::Transfer()"
#endif

Transfer::Transfer(OpP2PObserver& observer) : P2PConnection(observer),
	m_runcookie(0)
{
	ENTER_METHOD;

//	if(g_Transfers->Check(this) == FALSE)
	{
		g_Transfers->Add(this);
	}
	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::~Transfer()"
#endif

Transfer::~Transfer()
{
	ENTER_METHOD;

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::ConnectTo()"
#endif

OP_STATUS Transfer::ConnectTo(OpString& address, WORD port)
{
	ENTER_METHOD;

	// TODO: Handle connections errors in the callback
	OP_STATUS status = P2PConnection::ConnectTo(address, port);

	LEAVE_METHOD;

	return status;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::SocketConnectedL()"
#endif

void Transfer::OnSocketConnected(OpSocket* socket)
{
	ENTER_METHOD;

	P2PConnection::OnSocketConnected(socket);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::OnSocketClosed()"
#endif

void Transfer::OnSocketClosed(OpSocket* socket)
{
	ENTER_METHOD;

	P2PConnection::OnSocketClosed(socket);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::AttachTo()"
#endif

void Transfer::AttachTo(P2PConnection* pConnection)
{
	ENTER_METHOD;

	P2PConnection::AttachTo( pConnection );
//	if(gUNI_Lransfers->Check(this) == FALSE)
//	{
//		gUNI_Lransfers->Add(this);
//	}

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfer::Close()"
#endif

void Transfer::Close()
{
	ENTER_METHOD;

	P2PConnection::Close();

	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// Transfers construction

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::Transfers()"
#endif

Transfers::Transfers() :
	m_runcookie(0),
	m_last_resource_check(0),
	m_kibs(0), m_kibsUpload(0)
{
	ENTER_METHOD;

	BT_RESOURCE_ADD("Transfers", this);

	m_check_timer.SetTimerListener(this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::~Transfers()"
#endif

Transfers::~Transfers()
{
	ENTER_METHOD;

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	/* Force delete of all connections */
	Clear(TRUE);

	TransferSpeed *speed = m_xferspeeds.Begin();

	while(speed != NULL)
	{
		OP_DELETE(speed);

		speed = m_xferspeeds.Next();
	}
	BT_RESOURCE_REMOVE(this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::StartTimer()"
#endif

OP_STATUS Transfers::StartTimer(DownloadBase *download)
{
	ENTER_METHOD;

	m_check_timer.Start(CHECK_INTERVAL_BT);

	if(!m_active_downloads.Check(download))
	{
		m_active_downloads.AddLast(download);
	}

	LEAVE_METHOD;

	return OpStatus::OK;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::StopTimer()"
#endif

OP_STATUS Transfers::StopTimer(DownloadBase *download)
{
	ENTER_METHOD;

	m_active_downloads.RemoveItem(download);

	if(m_active_downloads.GetCount() == 0)
	{
		m_check_timer.Stop();

		/* Force delete of all connections */
		Clear(TRUE);
	}

	LEAVE_METHOD;

	return OpStatus::OK;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::Clear()"
#endif

void Transfers::Clear(BOOL force)
{
	OpP2PList<Transfer> m_delete_list;

	PROFILE(FALSE);

	Transfer *connection = m_list.Begin();

	while(connection != NULL)
	{
		if(force)
		{
			m_delete_list.AddLast(connection);
		}
		else if(connection->IsClosing())
		{
			m_delete_list.AddLast(connection);

		}
		connection = m_list.Next();
	}
	connection = m_delete_list.Begin();

	while(connection != NULL)
	{
		if(force)
		{
			DEBUGTRACE8_RES(UNI_L("WITH FORCE DESTRUCTING P2PConnection 0x%08x, "), connection);
		}
		else if(connection->IsClosing())
		{
			if (connection->GetRefCount() == 0)
			{
				DEBUGTRACE8_RES(UNI_L("DESTRUCTING P2PConnection 0x%08x, "), connection);
			}
			else if(connection->CanDelete())
			{
				DEBUGTRACE8_RES(UNI_L("WITH FORCE DESTRUCTING P2PConnection 0x%08x, "), connection);
			}
			else
			{
				DEBUGTRACE8_RES(UNI_L("NOT DESTRUCTING P2PConnection 0x%08x, "), connection);
				DEBUGTRACE8_RES(UNI_L("refcount: %d\n"), connection->GetRefCount());
				connection = m_delete_list.Next();
				continue;
			}
		}
		m_list.RemoveItem(connection);
		DEBUGTRACE8_RES(UNI_L("%d connections left\n"), m_list.GetCount());
		connection->Close();
		OP_DELETE(connection);

		connection = m_delete_list.Next();
	}
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::OnTimeOut()"
#endif

void Transfers::OnTimeOut(OpTimer* timer)
{
	ENTER_METHOD;

	OnRun();

	time_t now = op_time(NULL);

	if(now > m_last_resource_check + 5)
	{
		// do resource cleanup every 5 seconds
		Clear();
		m_last_resource_check = now;
	}

	CalculateTransferRates();

	BT_RESOURCE_CHECKPOINT(FALSE);

	m_check_timer.Start(CHECK_INTERVAL_BT);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::CalculateTransferRates()"
#endif

void Transfers::CalculateTransferRates()
{
	ENTER_METHOD;

	if(m_xferspeeds.GetCount())
	{
		m_kibs = m_kibsUpload = 0;

		TransferSpeed *speed = m_xferspeeds.Begin();

		while(speed != NULL)
		{
			m_kibs += speed->m_kibs;
			m_kibsUpload += speed->m_kibsUpload;

			speed = m_xferspeeds.Next();
		}
	}
    LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::SendStop()"
#endif

void Transfers::SendStop(SHAStruct *guid)
{
	P2PConnection *connection = m_list.Begin();

	while(connection != NULL)
	{
		SHAStruct sha;

		connection->GetGUID(&sha);

		if(sha == *guid)
		{
			DEBUGTRACE8_RES(UNI_L("SENDING CLOSE TO P2PConnection 0x%08x\n"), connection);

			connection->StartClosing();
			connection->Close();
		}
		connection = m_list.Next();
	}
}

//////////////////////////////////////////////////////////////////////
// Transfers list tests

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::GetActiveCount()"
#endif

INT32 Transfers::GetActiveCount()
{
	ENTER_METHOD;

//	INT32 cnt = g_Downloads.GetCount(TRUE) + g_Uploads.GetTransferCount();
	INT32 cnt = -1;

	LEAVE_METHOD;

	return cnt;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::IsConnectedTo()"
#endif

BOOL Transfers::IsConnectedTo(OpString& address)
{
	ENTER_METHOD;

	P2PConnection *connection = m_list.Begin();

	while(connection != NULL)
	{
		if(connection->m_address.Compare(address) == 0)
		{
			return TRUE;
		}
		connection = m_list.Next();
	}
	LEAVE_METHOD;

	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// Transfers registration

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::Add()"
#endif

void Transfers::Add(Transfer* transfer)
{
	ENTER_METHOD;

	BOOL exists = Check(transfer);
	if(exists == FALSE)
	{
		m_list.AddLast(transfer);

		// start timer when first transfer has been added
//		if(m_timer_ref_count == 0)
//		{
//			StartTimer();
//		}
	}
	else
	{
		DEBUGTRACE8_RES(UNI_L("**** NOT ADDING Transfer 0x%08x\n"), transfer);
		OP_ASSERT(FALSE);
	}

//	OnRun();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::Remove()"
#endif

void Transfers::Remove(Transfer* transfer)
{
	ENTER_METHOD;

	m_list.RemoveItem(transfer);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Transfers::OnRun()"
#endif

BOOL Transfers::OnRun()
{
	ENTER_METHOD;

	g_Downloads->OnRun();

//	g_P2PFiles.CommitDeferred();

	LEAVE_METHOD;

	return TRUE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::P2PServerConnector()"
#endif

P2PServerConnector::P2PServerConnector()
{
	ENTER_METHOD;

	BT_RESOURCE_ADD("P2PServerConnector", this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::~P2PServerConnector()"
#endif

P2PServerConnector::~P2PServerConnector()
{
	ENTER_METHOD;

	if(m_listensocket.Get() != NULL)
	{
		m_listensocket.Get()->SetListener(&m_nullobserver);

		DEBUGTRACE8_SOCKET(UNI_L("DELETING LISTEN SOCKET: 0x%08x\n"), m_listensocket.Get());
	}

	m_serverinfo_list.DeleteAll();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);

	LEAVE_METHOD;
}

// used by BTConfigDialog

OP_STATUS P2PServerConnector::TestAcceptConnections(INT32 port, OpSocketListener *observer, P2PSocket& listensocket)
{
	// Create a socket.
	OpSocket *socket;
	OpSocketAddress *socket_address = NULL;

	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, observer, 0));

	BT_RESOURCE_ADD("socket", socket);

	listensocket.Attach(socket);

	RETURN_IF_ERROR(OpSocketAddress::Create(&socket_address));

	socket_address->SetPort(port);

	OP_STATUS status = socket->Listen(socket_address, 1);

	OP_DELETE(socket_address);

	return status;
}

// used by BTConfigDialog

void P2PServerConnector::TestStopConnections(P2PSocket& listensocket)
{
	listensocket.Release();
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::AcceptConnections()"
#endif

BOOL P2PServerConnector::AcceptConnections(DownloadBase *download, INT32 port)
{
	ENTER_METHOD;

#ifndef BT_UPLOAD_DISABLED
	if(m_serverinfo_list.GetCount() == 0)
	{
		// only create socket on the first connection
//		OP_ASSERT(m_listensocket == NULL);

		// Create a socket.
		OpSocket *listensocket;
		RETURN_VALUE_IF_ERROR(SocketWrapper::CreateTCPSocket(&listensocket, this, 0), FALSE);

//		BT_RESOURCE_ADD("socket", listensocket);

		m_listensocket.Attach(listensocket);

		OpSocketAddress* address;
		if(OpStatus::IsError(OpSocketAddress::Create(&address)))
		{
			LEAVE_METHOD;
			return FALSE;
		}
		address->SetPort(port);

		OP_STATUS status = listensocket->Listen(address, 1);

		OP_DELETE(address);

		if(OpStatus::IsSuccess(status))
		{
			DEBUGTRACE_SOCKET(UNI_L("SERVER: Listen active on port %d\n"), port);
		}
		else
		{
			OP_ASSERT(FALSE);
			LEAVE_METHOD;
			return FALSE;
		}
	}
	P2PServerInfo *srvinfo = OP_NEW(P2PServerInfo, (download, port));

	if(srvinfo)
	{
		if (OpStatus::IsError(m_serverinfo_list.Add(srvinfo)))
			OP_DELETE(srvinfo);
	}

#endif // BT_UPLOAD_DISABLED
	LEAVE_METHOD;

	return TRUE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::StopConnections()"
#endif

void P2PServerConnector::StopConnections(DownloadBase *download)
{
	ENTER_METHOD;

//	OP_ASSERT(m_list.GetCount() != 0);

#ifndef BT_UPLOAD_DISABLED
	for(UINT32 pos = 0; pos < m_serverinfo_list.GetCount(); pos++)
	{
		P2PServerInfo *srvinfo = m_serverinfo_list.Get(pos);

		if(srvinfo->m_download == download)
		{
			OP_DELETE(m_serverinfo_list.Remove(pos));
			break;
		}
	}
	if(m_serverinfo_list.GetCount() == 0)
	{
		// no more downloads in the queue, delete the socket

		if(m_listensocket.Get() != NULL)
		{
			m_listensocket.Get()->SetListener(&m_nullobserver);

			DEBUGTRACE8_SOCKET(UNI_L("DELETING LISTEN SOCKET 2: 0x%08x\n"), m_listensocket.Get());

			m_listensocket.Release();

			DEBUGTRACE_SOCKET(UNI_L("SERVER: Listen socket 0x%08x destroyed\n"), m_listensocket.Get());
		}
		m_incoming_sockets.DeleteAll();
	}
#endif // BT_UPLOAD_DISABLED

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::OnSocketConnected()"
#endif

void P2PServerConnector::OnSocketConnected(OpSocket* socket)
{
	ENTER_METHOD;

//	BT_RESOURCE_ADD("socket", socket);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::OnSocketDataReady()"
#endif

void P2PServerConnector::OnSocketDataReady(OpSocket* socket)
{
	ENTER_METHOD;

	UINT32 bytes_received = 0;
	BOOL datarecv = FALSE;
	OpByteBuffer buffer;

	socket->Recv(m_read_buffer, READBUFFERSIZE, &bytes_received);

	while((int)bytes_received > 0)
	{
		DEBUGTRACE_UP(UNI_L("Read %d bytes from host\n"), bytes_received);

		buffer.Append(m_read_buffer, bytes_received);

		datarecv = TRUE;

		// Read more data, if any.
		bytes_received = 0;
		socket->Recv(m_read_buffer, READBUFFERSIZE, &bytes_received);
	}

	if (m_incoming_sockets.Find(socket) < 0)
	{
		// connection was closed and socket was deleted, abort
		return;
	}

	if(datarecv)
	{
		P2PSocket tmpsocket;

		tmpsocket.Attach(socket);
		m_incoming_sockets.RemoveByItem(socket);

		BTClientConnector *client = VerifyHandshake(buffer, &tmpsocket);
		if(client != NULL)
		{
			// let the download class handle the connection from here
			if(client->m_download == NULL)
			{
				client->m_download = (BTDownload *)g_Downloads->FindByBTH(&client->m_GUID);
				if(client->m_download == NULL)
				{
					// we don't want this connection
					DEBUGTRACE_CONNECT(UNI_L("DROPPING incoming connection 0x%08x 2\n"), tmpsocket.Get());
					OP_DELETE(client);
					return;
				}
				else
				{
					// is this download accepting connections at this time?
					UINT32 cnt;
					BOOL accepting = FALSE;

					for(cnt = 0; cnt < m_serverinfo_list.GetCount(); cnt++)
					{
						P2PServerInfo *info = m_serverinfo_list.Get(cnt);
						if(info && info->m_download == client->m_download)
						{
							accepting = TRUE;
							break;
						}
					}
					if(!accepting)
					{
						DEBUGTRACE_CONNECT(UNI_L("DROPPING incoming connection to torrent %s\n"), client->m_download->m_localname.CStr());
						OP_DELETE(client);
						return;
					}
					DEBUGTRACE_CONNECT(UNI_L("ACCEPTING incoming connection on torrent %s\n"), client->m_download->m_localname.CStr());
					client->m_download->ClientConnectors()->Add(client);
				}
			}
			else
			{
				// is this download accepting connections at this time?
				UINT32 cnt;
				BOOL accepting = FALSE;

				for(cnt = 0; cnt < m_serverinfo_list.GetCount(); cnt++)
				{
					P2PServerInfo *info = m_serverinfo_list.Get(cnt);
					if(info && info->m_download == client->m_download)
					{
						accepting = TRUE;
						break;
					}
				}
				if(!accepting)
				{
					DEBUGTRACE_CONNECT(UNI_L("DROPPING incoming connection to torrent %s\n"), client->m_download->m_localname.CStr());
					client->m_download = NULL;
					client->Close();
					return;
				}
			}
		}
		else
		{
			// we don't want this connection
			DEBUGTRACE_CONNECT(UNI_L("DROPPING incoming connection 0x%08x\n"), tmpsocket.Get());
			return;
		}
	}

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::OnSocketConnectionRequest()"
#endif

void P2PServerConnector::OnSocketConnectionRequest(OpSocket* socket)
{
	ENTER_METHOD;

	OP_ASSERT(socket == m_listensocket.Get());

	// We have an incoming connection request.
	OpSocket *connected_socket;
	if (OpStatus::IsError(SocketWrapper::CreateTCPSocket(&connected_socket, this, 0)))
	{
		LEAVE_METHOD;
		return;
	}
	m_incoming_sockets.Add(connected_socket);
	socket->Accept(connected_socket);

//	BT_RESOURCE_ADD("socket", connected_socket);

#ifdef DEBUG_BT_UPLOAD
	// Retrieve the ip address of the host connecting.
	OpString connecting_host;

	OpSocketAddress *socket_address;

	if (OpStatus::IsError(OpSocketAddress::Create(&socket_address)))
	{
		LEAVE_METHOD;
		return;
	}
	connected_socket->GetSocketAddress(socket_address);

	socket_address->ToString(&connecting_host);

	OP_DELETE(socket_address);

	DEBUGTRACE_UP(UNI_L("INCOMING connection from: %s\n"), (uni_char *)connecting_host);
#endif

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::OnSocketClosed()"
#endif

void P2PServerConnector::OnSocketClosed(OpSocket* socket)
{
	ENTER_METHOD;

	socket->Close();

	m_incoming_sockets.RemoveByItem(socket);

	if (m_listensocket.Get() == socket)
		m_listensocket.Detach();

	OP_DELETE(socket);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PServerConnector::OnSocketReceiveError()"
#endif

void P2PServerConnector::OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error)
{
	ENTER_METHOD;

	LEAVE_METHOD;
}


void P2PServerConnector::OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error)
{
	m_incoming_sockets.RemoveByItem(socket);

	if (m_listensocket.Get() == socket)
		m_listensocket.Detach();

	OP_DELETE(socket);
}


#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTServerConnector::BTServerConnector()"
#endif

BTServerConnector::BTServerConnector()
{
	ENTER_METHOD;

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTServerConnector::~BTServerConnector()"
#endif

BTServerConnector::~BTServerConnector()
{
	ENTER_METHOD;

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTServerConnector::VerifyHandshake()"
#endif

BTClientConnector *BTServerConnector::VerifyHandshake(OpByteBuffer& buffer, P2PSocket *socket)
{
	ENTER_METHOD;

	if(buffer.DataSize() < 7)
	{
		return FALSE;
	}
	// Check for BitTorrent handshake

	if(buffer.DataSize() >= BT_PROTOCOL_HEADER_LEN &&
		 memcmp(buffer.Buffer(), BT_PROTOCOL_HEADER, BT_PROTOCOL_HEADER_LEN ) == 0)
	{
		return BTClientConnectors::OnAccept(NULL, socket, NULL, &buffer);
	}
	LEAVE_METHOD;

	return NULL;
}

P2PSocket::P2PSocket()
	: m_socket(NULL)
{
	BT_RESOURCE_ADD("P2PSocket", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PSocket::~P2PSocket()"
#endif

P2PSocket::~P2PSocket()
{
	OP_DELETE(m_socket);

	if(m_socket)
	{
//		BT_RESOURCE_REMOVE(m_socket);
	}
	BT_RESOURCE_REMOVE(this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PSocket::Release()"
#endif

OpSocket* P2PSocket::Release()
{
	BT_RESOURCE_REMOVE(m_socket);
	OP_DELETE(m_socket);

	m_socket = NULL;

	return NULL;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PSocket::Attach()"
#endif

void P2PSocket::Attach(OpSocket* ptr)
{
	OP_ASSERT(m_socket == NULL);
	OP_ASSERT(ptr != m_socket);

	if(m_socket && ptr != m_socket)
	{
//		BT_RESOURCE_REMOVE(m_socket);
		OP_DELETE(m_socket);
	}
	m_socket = ptr;
}

OpSocket *P2PSocket::Detach()
{
	OpSocket* tmp = m_socket;

	m_socket = NULL;

	return tmp;
}

//
// Contains each instance of the server

P2PServerInfo::P2PServerInfo(DownloadBase *download, INT32 port)
{
	m_port = port;
	m_download = download;

	BT_RESOURCE_ADD("P2PServerInfo", this);
}
P2PServerInfo::~P2PServerInfo()
{
	BT_RESOURCE_REMOVE(this);
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
