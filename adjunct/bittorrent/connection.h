/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef P2P_CONNECTIONBASE_H
#define P2P_CONNECTIONBASE_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

# include "modules/pi/network/OpSocket.h"
# include "modules/pi/network/OpSocketAddress.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/buffer.h"
#include "adjunct/m2/src/util/socketserver.h"
#include "bt-globals.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

class BTClientConnector;
class GlueFactory;
class OpFile;
class OpByteBuffer;
class DownloadBase;
class BTClientConnectors;
class P2PConnection;

#define READBUFFERSIZE	4096

//#define BT_MAX_DOWNLOAD_RATE	40		// max download rate, hard coded for now
//#define BT_MAX_UPLOAD_RATE		20		// max upload rate, hard coded for now

typedef struct
{
	DWORD*		pLimit;
	BOOL		bUnscaled;
	DWORD		nTotal;
	DWORD		tLast;
	DWORD		nMeasure;
	DWORD		pHistory[64];
	DWORD		pTimes[64];
	DWORD		nPosition;
	DWORD		tLastAdd;
	DWORD		tLastSlot;
} TCPBandwidthMeter;

class P2PSocket
{
private:
	OpSocket *m_socket;

	P2PSocket& operator=(P2PSocket& rhs) { return *this; }

public:
	P2PSocket();
	virtual ~P2PSocket();
	OpSocket* Release();
	void Attach(OpSocket* ptr);
	OpSocket *Detach();
	OpSocket *Get() const { return m_socket; }
};

class OpP2PObserver
{
public:
	OpP2PObserver() {};
	virtual ~OpP2PObserver() {};

	// receive
	virtual BOOL OnP2PDataReceived(P2PConnection *instance, OpByteBuffer *inputbuffer) = 0;
	virtual void OnP2PReceiveError(P2PConnection *instance, OpSocket::Error socket_error) = 0;
	virtual void OnP2PDataSent(P2PConnection *instance, unsigned char *data_buffer_sent, UINT32 bytes_sent) = 0;

	// connect/disconnect
	virtual void OnP2PConnected(P2PConnection *instance) = 0;
	virtual void OnP2PDisconnected(P2PConnection *instance) = 0;
	virtual void OnP2PConnectError(P2PConnection *instance, OpSocket::Error socket_error) = 0;

	// destructed
	// called when the connection class goes down, so the client can safely cleanup
	virtual void OnP2PDestructing(P2PConnection *instance) = 0;

	virtual INT32 AddRef() = 0;
	virtual INT32 Release() = 0;
};

// dummy observer to use when shutting down the connection
class P2PNullObserver : public OpP2PObserver,
	public OpSocketListener
{
public:
	P2PNullObserver() {};
	virtual ~P2PNullObserver() {};

	// receive
	virtual BOOL OnP2PDataReceived(P2PConnection *instance, OpByteBuffer *inputbuffer) { return TRUE; }
	virtual void OnP2PReceiveError(P2PConnection *instance, OpSocket::Error socket_error) {}

	virtual void OnP2PDataSent(P2PConnection *instance, unsigned char *data_buffer_sent, UINT32 bytes_sent)  {}

	// connect/disconnect
	virtual void OnP2PConnected(P2PConnection *instance) {}
	virtual void OnP2PDisconnected(P2PConnection *instance) {}
	virtual void OnP2PConnectError(P2PConnection *instance, OpSocket::Error socket_error) {}

	// destructed
	// called when the connection class goes down, so the client can safely cleanup
	virtual void OnP2PDestructing(P2PConnection *instance) {}

	virtual INT32 AddRef() { return 0; }
	virtual INT32 Release() { return 0; }

	// MpSocketObserver.
    virtual void OnSocketInstanceNumber(int instance_number) { }
	virtual void OnSocketConnected(OpSocket* socket) { }
	virtual void OnSocketDataReady(OpSocket* socket) { }
	virtual void OnSocketClosed(OpSocket* socket) { }
	virtual void OnSocketConnectionRequest(OpSocket* socket) {};
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {}
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { }
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent) { }
};

class P2PConnection :
	public OpSocketListener
{
public:
	// Constructor / destructor.
	P2PConnection(OpP2PObserver& observer);
	virtual ~P2PConnection();

	virtual OP_STATUS ConnectTo(OpString& pAddress, WORD nPort);
	virtual BOOL	OnRun();
	virtual void	AttachTo(P2PConnection* pConnection);
	virtual void	AcceptFrom(P2PSocket *socket, OpString& address);
	virtual void	Close();
	virtual BOOL	OnRead();
	virtual void	ReadData();
	virtual OP_STATUS OnWrite(OpByteBuffer *sendbuffer);
//	BOOL			DoRun();
	BOOL			IsConnected() { return m_connected; }
	BOOL			ConnectedTime() { return m_connectedTime; }
	BOOL			IsInitiated() { return m_initiated; }
	BOOL			IsInputDataPending() { return m_input->DataSize() > 0; }
	OpByteBuffer	*GetInputDataBuffer() { return m_input; }
	void			PauseReceive(BOOL pause);

	void			StartClosing()
	{
		m_closing = TRUE;
		m_closedTime = op_time(NULL);
	}
	BOOL			IsClosing() { return m_closing; }

	BOOL			CanDelete()
	{
		if(m_closing)
		{
			time_t current = op_time(NULL);

			if((current - m_closedTime) > 120)
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	void			AddRef()
	{
		m_refcount++;
//		DEBUGTRACE8_RES("*** P2PCONNECTION 0x%08x: AddRef: ", this);
//		DEBUGTRACE8_RES("%d\n", m_refcount);
	}
	void			Release()
	{
		m_refcount--;
//		DEBUGTRACE8_RES("*** P2PCONNECTION 0x%08x: Release: ", this);
//		DEBUGTRACE8_RES("%d\n", m_refcount);
	}
	INT32			GetRefCount() { return m_refcount; }
	void			GetGUID(SHAStruct *guid) { memcpy(guid, &m_guid, sizeof(SHAStruct)); }
	void			SetGUID(SHAStruct *guid) { memcpy(&m_guid, guid, sizeof(SHAStruct)); }

protected:
	OpP2PObserver& Observer() { return *m_observer; }

	BOOL		m_connected;
	time_t		m_connectedTime;
	BOOL		m_initiated;
	BOOL		m_closing;
	INT32		m_refcount;
	SHAStruct	m_guid;
	time_t		m_closedTime;

	// OpSocketListener.
    virtual void OnSocketInstanceNumber(int instance_number) { }
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketConnectionRequest(OpSocket* socket) {};
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error);
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {}
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { }
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent);

private:

	// Members.
	UINT64 m_bytes_received;
	UINT64 m_bytes_sent;
	unsigned char m_read_buffer[READBUFFERSIZE];
	OpP2PObserver *m_observer;
	P2PNullObserver m_nullobserver;
	BOOL			m_recv_pause;	// pause in receive of data
	BOOL			m_recv_ready;	// ready to receive
	OpByteBuffer*	m_input;
	unsigned char*	m_last_buffer_sent;

public:
	P2PSocket m_socket;
//	BOOL	m_socket_created;
	OpSocketAddress *m_socket_address;
	OpString	m_useragent;
	OpString	m_host;
	OpString	m_address;
	TCPBandwidthMeter	m_mInput;
	TCPBandwidthMeter	m_mOutput;
//	OpByteBuffer*	m_output;
	BOOL			m_upload;		// used for debug purposes
};

class Transfer : public P2PConnection
{
// Construction
public:
	Transfer(OpP2PObserver& observer);
	virtual ~Transfer();

// Attributes
public:
	UINT32			m_runcookie;

public:
	OpVector<OpString> m_sourcessent;

protected:
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketClosed(OpSocket* socket);

// Operations
public:
	virtual OP_STATUS ConnectTo(OpString& pAddress, WORD nPort);
	virtual void	AttachTo(P2PConnection* connection);
	virtual void	Close();

};

class TransferSpeed
{
public:
	TransferSpeed() :
		m_id(0),
		m_kibs(0),
		m_kibsUpload(0)
	{

	}
	virtual ~TransferSpeed() {}

private:
	UINTPTR m_id;
	double m_kibs, m_kibsUpload;

	friend class Transfers;
};

class Transfers :
	public OpTimerListener
{
// Construction
public:
	Transfers();
	virtual ~Transfers();

// Attributes
public:

	///< Which method to use when restricting bandwidth use. 1 = automatic, 2 = fixed rates
	BOOL IsAutomaticUploadRestrictionActive()
	{
		return (BOOL)(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTBandwidthRestrictionMode) == 1);
	}

	BOOL ThrottleDownload()
	{
		INT32 max = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTMaxDownloadRate);

		if(max > 0)
		{
			return (BOOL)(m_kibs > max);
		}
		else
		{
			return FALSE;
		}
	}
	BOOL ThrottleUpload()
	{
		INT32 max = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTMaxUploadRate);

		if(max > 0)
		{
			return (BOOL)(m_kibsUpload > max);
		}
		else
		{
			return FALSE;
		}
	}
	double GetCalculatedDownloadSpeed()
	{
		return m_kibs;
	}
	double GetCalculatedUploadSpeed()
	{
		return m_kibsUpload;
	}
	OP_STATUS UpdateTransferSpeed(UINTPTR id, double kibs, double uploadkibs)
	{
		TransferSpeed *speed = m_xferspeeds.Begin();

		while(speed != NULL)
		{
			if(speed->m_id == id)
			{
				speed->m_kibs = kibs;
				speed->m_kibsUpload = uploadkibs;

				return OpStatus::OK;
			}
			speed = m_xferspeeds.Next();
		}
		speed = OP_NEW(TransferSpeed, ());

		if(speed == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		speed->m_id = id;
		speed->m_kibs = kibs;
		speed->m_kibsUpload = uploadkibs;

		OP_STATUS err = m_xferspeeds.AddLast(speed);
		if (OpStatus::IsError(err))
			OP_DELETE(speed);

		return err;
	}

protected:

	OpP2PList<TransferSpeed> m_xferspeeds;
	OpP2PList<Transfer> m_list;
	OpP2PList<DownloadBase> m_active_downloads;
	UINT32			m_runcookie;
	OpTimer			m_check_timer;
	time_t			m_last_resource_check;
	double			m_kibs, m_kibsUpload;

	// Floating average calculation code.
#ifndef FLOATING_AVG_SLOTS
#define FLOATING_AVG_SLOTS 200
#endif

	double AverageSpeedUpload();
	double AverageSpeed( );

// Operations
public:
	INT32		GetActiveCount();
	BOOL		IsConnectedTo(OpString& address);
	OP_STATUS	StartTimer(DownloadBase *download);
	OP_STATUS	StopTimer(DownloadBase *download);
	void		SendStop(SHAStruct *guid);
	void		Clear(BOOL force = FALSE);

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

	void		CalculateTransferRates();

	INT32		GetTotalCount() { return m_list.GetCount(); }

protected:
	BOOL		OnRun();


protected:
	void		Add(Transfer* transfer);
	void		Remove(Transfer* transfer);

// List Access
public:
	BOOL Check(Transfer *item) { return m_list.Check(item); }

	friend class Transfer;

};

class P2PServerInfo
{
public:

	P2PServerInfo(DownloadBase *download, INT32 port);
	virtual ~P2PServerInfo();

public:

	INT32			m_port;
	DownloadBase	*m_download;
};

/*
Control flow:

  - BTServerConnector gets called to bind and accept connections on a certain port.
  - BTServerConnector creates a unique P2PServerInfo class for each download
  - When a incoming connection request arrives on the BTServerConnector class,
    the BTServerConnector class will receive the handshake from the connecting
	peer, compare it to the P2PServerInfo to find the download with a SHA1 checksum
	that matches what the connecting peer sends, then call AttachTo on the download
	class. If no matching checksum is found, the incoming connection will be closed.
  - The BTServerConnector has now handed the connection to the download class,
    which in turn will create a unique BTClientConnector class for this particular
	incoming connection and attach the socket to it

*/

class P2PServerConnector :
	public OpSocketListener
{
public:

	P2PServerConnector();
	virtual ~P2PServerConnector();

	virtual BOOL	AcceptConnections(DownloadBase *download, INT32 port);
	virtual void	StopConnections(DownloadBase *download);
	static OP_STATUS TestAcceptConnections(INT32 port, OpSocketListener *observer, P2PSocket& listensocket);
	static void		TestStopConnections(P2PSocket& listensocket);

protected:
	virtual BTClientConnector *VerifyHandshake(OpByteBuffer& buffer, P2PSocket *socket) = 0;	// must return non-NULL for the socket to be passed on to the download class

	// OpSocketListener
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketConnectionRequest(OpSocket* socket);
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketInstanceNumber(int instance_number) { }
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent) { }
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error);
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {}
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error socket_error) { };
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error socket_error) { };
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { };

	OpVector<P2PServerInfo> m_serverinfo_list;
	OpVector<OpSocket> m_incoming_sockets;
	P2PSocket m_listensocket;
	unsigned char m_read_buffer[READBUFFERSIZE];
	P2PNullObserver m_nullobserver;

public:
	BOOL Check(DownloadBase* dl) const
	{
		UINT32 pos;

		for(pos = 0; pos < m_serverinfo_list.GetCount(); pos++)
		{
			P2PServerInfo *info = m_serverinfo_list.Get(pos);
			if(info && info->m_download == dl)
			{
				return TRUE;
			}
		}
		return FALSE;
	}
};

class BTServerConnector : public P2PServerConnector
{
public:
	BTServerConnector();
	virtual ~BTServerConnector();

protected:
	virtual BTClientConnector *VerifyHandshake(OpByteBuffer& buffer, P2PSocket *socket);
};

#endif // P2P_CONNECTIONBASE_H
