// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_CLIENT_H
#define BT_CLIENT_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"
#include "adjunct/bittorrent/dl-base.h"

#define CHECK_CLIENT_INTERVAL 5000	// used for checking of a connection can be taken down

//********************************************************************
//
//	BTClientConnector
//
//  The code that wraps both the peer and server functionality
//
//********************************************************************

#include "bt-util.h"
#include "bt-tracker.h"
#include "bt-globals.h"
#include "connection.h"

class BTPacket;
class DownloadBase;
class DownloadTransferBT;
class UploadTransferBT;
class BTDownload;

class BTDataQueueItem
{
public:

	BTDataQueueItem(OpByteBuffer *buffer)
	:	m_data_buffer(buffer),
		m_data_sent(FALSE)
	{
		BT_RESOURCE_ADD("BTDataQueueItem", this);
	}
	virtual ~BTDataQueueItem()
	{
		OP_DELETE(m_data_buffer);
		BT_RESOURCE_REMOVE(this);
	}
	OpByteBuffer *GetDataBuffer() { return m_data_buffer; }
	BOOL IsDataSent() { return m_data_sent; }
	void SetDataAsSent(BOOL sent) { m_data_sent = sent; }

private:
	OpByteBuffer	*m_data_buffer;		// this class will claim the ownership of this buffer
	BOOL			m_data_sent;		// has the data been sent?

	BTDataQueueItem() {}	// don't want it
};

class BTDataQueue
{
public:

	BTDataQueue()
		: m_size(0)
	{
		BT_RESOURCE_ADD("BTDataQueue", this);
	}
	virtual ~BTDataQueue()
	{
		Empty();
		BT_RESOURCE_REMOVE(this);
	}
	OP_STATUS Append(unsigned char *data, UINT data_size)
	{
		if(data == NULL || data_size == 0)
		{
			return OpStatus::ERR_NULL_POINTER;
		}
		OpByteBuffer *buffer = OP_NEW(OpByteBuffer, ());
		if(!buffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if(OpStatus::IsError(buffer->Append(data, data_size)))
		{
			OP_DELETE(buffer);
			return OpStatus::ERR_NO_MEMORY;
		}
		return Append(buffer);
	}
	OP_STATUS Append(OpByteBuffer *data)
	{
		BTDataQueueItem *item = OP_NEW(BTDataQueueItem, (data));
		if (item)
		{
			OP_STATUS err = m_buffers.AddLast(item);

			if (OpStatus::IsError(err))
				OP_DELETE(item);

			m_size += data->DataSize();

			return err;
		}
		else
		{
			OP_DELETE(data);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	BOOL HasDataBuffer(unsigned char *data_buffer)
	{
		BTDataQueueItem *item = m_buffers.Begin();
		if(item)
		{
			OpByteBuffer *buf = item->GetDataBuffer();
			if(buf)
			{
				if(buf->Buffer() == data_buffer)
				{
					return TRUE;
				}
			}
		}
		return FALSE;
	}
	OpByteBuffer *GetFirstBuffer(BOOL return_sent_buffer, BOOL mark_as_sent)
	{
		BTDataQueueItem *item = m_buffers.Begin();
		while(item)
		{
			OpByteBuffer *buf = item->GetDataBuffer();
			if(buf && buf->DataSize() && item->IsDataSent() == return_sent_buffer)
			{
				if(mark_as_sent)
				{
					item->SetDataAsSent(mark_as_sent);
				}
				return buf;
			}
			item = m_buffers.Next();
		}
		return NULL;
	}
	OP_STATUS Remove(UINT data_size, BOOL only_remove_sent_buffer)
	{
		if(!m_buffers.IsEmpty())
		{
			BTDataQueueItem *item = m_buffers.Begin();
			if(item)
			{
				OpByteBuffer *buf = item->GetDataBuffer();
				if(buf)
				{
					if(buf->DataSize() == data_size)
					{
						if(item->IsDataSent() == only_remove_sent_buffer)
						{
							m_size -= buf->DataSize();

							OP_DELETE(m_buffers.RemoveCurrentItem());
						}
						return OpStatus::OK;
					}
					else
					{
						// only parts of a buffer might have been sent (eg. just one command among several),
						// so only remove the buffer when it is empty
						if(item->IsDataSent() == only_remove_sent_buffer)
						{
							buf->Remove(data_size);
							m_size -= data_size;

							if(buf->DataSize() == 0)
							{
								OP_DELETE(m_buffers.RemoveCurrentItem());
							}
							return OpStatus::OK;
						}
					}
				}
			}
		}
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	UINT32 Size() { return m_size; }
/*
	{
		UINT32 size = 0;
		BTDataQueueItem *item = m_buffers.Begin();
		while(item)
		{
			OpByteBuffer *buf = item->GetDataBuffer();
			if(buf)
			{
				size += buf->DataSize();
			}
			item = m_buffers.Next();
		}
		return size;
	}
*/
	void Empty()
	{
		while(m_buffers.Begin())
		{
			OP_DELETE(m_buffers.RemoveCurrentItem());
		}
		m_size = 0;
	}
	OP_STATUS MergeBuffers()
	{
		if(m_buffers.GetCount() > 1)
		{
			OpByteBuffer *buf = OP_NEW(OpByteBuffer, ());
			if (!buf)
				return OpStatus::ERR_NO_MEMORY;

			BTDataQueueItem *item = m_buffers.Begin();
			while(item)
			{
				OpByteBuffer *tmp = item->GetDataBuffer();
				// only merge buffers not sent
				if(tmp && !item->IsDataSent())
				{
					buf->Append(tmp->Buffer(), tmp->DataSize());
				}
				item = m_buffers.Next();
			}
			Empty();
			return Append(buf);
		}
		return OpStatus::OK;
	}
	BOOL IsEmpty() { return m_buffers.GetCount() == 0; }
	UINT GetCount() { return m_buffers.GetCount(); }

private:
	OpP2PList<BTDataQueueItem>	m_buffers;
	UINT32						m_size;
};

class BTClientConnector : public OpP2PObserver
{
	friend class BTClientConnectors;

// Construction
public:
	BTClientConnector();
	virtual ~BTClientConnector();

// Attributes
public:
	SHAStruct				m_GUID;
	BOOL					m_exchange;
	OpString				m_useragent;
	WORD					m_port;

	enum BTTransferStage
	{
		StageDownload = 0,
		StagePaused = 1
	};

public:
	UploadTransferBT*		m_upload;
	BTDownload*				m_download;
	DownloadTransferBT*		m_downloadtransfer;

protected:
	BOOL					m_shake;
	BOOL					m_online;
	BOOL					m_closing;
	Transfer				*m_transfer;
	BOOL					m_isupload;
	BOOL					m_ready_to_send;
	BTDataQueue				m_send_queue;
	time_t					m_last_keepalive;
	BTTransferStage			m_upload_stage;
	BTTransferStage			m_download_stage;
	BTDataQueue				m_output_commands;		// separate buffer for commands
	BOOL					m_peer_supports_extensions;	// peer supports the extension protocol documented at http://www.rasterbar.com/products/libtorrent/extension_protocol.html
	UINT32					m_pex_message_id;		// message id used for the Peer Exchange Message (PEX)
	time_t					m_pex_last_time;		// last time a PEX msg was sent to this peer

private:
	UINT32					m_refcount;

// Operations
public:
	virtual OP_STATUS Connect(DownloadTransferBT* pDownloadTransfer);
	virtual BOOL	AttachTo(Transfer* pConnection);
	virtual void	Close(BOOL force = FALSE);
	BOOL			IsUploadPaused() { return m_upload_stage == StagePaused; }
	void			SetUploadStage(const BTClientConnector::BTTransferStage stage);
	BOOL			IsDownloadPaused() { return m_download_stage == StagePaused; }
	void			SetDownloadStage(const BTClientConnector::BTTransferStage stage);
	BOOL			IsUpload() { return m_isupload; }

	Transfer		*GetTransfer() { return m_transfer; }

	// OpP2PObserver
	BOOL OnP2PDataReceived(P2PConnection *instance, OpByteBuffer *inputbuffer);
	void OnP2PConnected(P2PConnection *instance);
	void OnP2PDisconnected(P2PConnection *instance);
	void OnP2PDestructing(P2PConnection *instance);
	void OnP2PDataSent(P2PConnection *instance, unsigned char *data_buffer_sent, UINT32 bytes_sent);
	void OnP2PConnectError(P2PConnection *instance, OpSocket::Error socket_error);
	void OnP2PReceiveError(P2PConnection *instance, OpSocket::Error socket_error);

public:
	void			Send(BTPacket* packet, BOOL release = TRUE);
	void			Send(OpByteBuffer *buffer);

protected:
	virtual BOOL	OnRun();
	virtual BOOL	OnConnected();
	virtual void	OnDropped(BOOL bError);
	virtual BOOL	OnWrite();
	virtual BOOL	OnRead();

protected:
	BOOL			SendHandshake(BOOL bPart1, BOOL bPart2);
	BOOL			OnHandshake1();
	BOOL			OnHandshake2();
	BOOL			OnOnline();
	BOOL			OnPacket(BTPacket* pPacket);
	void			SendBeHandshake();
	BOOL			OnBeHandshake(BTPacket* pPacket);
	BOOL			OnSourceRequest(BTPacket* pPacket);
	void			DetermineUserAgent();
	OP_STATUS		OnExtensionPacket(BTPacket *packet);
	OP_STATUS		SendPeerExchange(time_t lastUpdateTime, DownloadSourceCollection& sources);
	OP_STATUS		HandlePeerExtensionList(OpByteBuffer& buffer);

public:
	void			SendKeepAlive();
	void			CloseUpload();
	inline BOOL IsOnline() const { return m_online; }

	INT32 AddRef()
	{
//		DEBUGTRACE8_RES("*** BTCLIENT 0x%08x: AddRef: ", this);
//		DEBUGTRACE8_RES("%d\n", m_refcount+1);
		return ++m_refcount;
	}
	INT32 Release()
	{
//		DEBUGTRACE8_RES("*** BTCLIENT 0x%08x: Release: ", this);
//		DEBUGTRACE8_RES("%d\n", m_refcount-1);
		return --m_refcount;
	}
	BOOL IsClosing()
	{
		return m_closing;
	}
	INT32 GetRefCount()
	{
		return m_refcount;
	}
};

class BTClientConnectors:
	public OpTimerListener
{
// Construction
public:
	BTClientConnectors();
	virtual ~BTClientConnectors();

// Attributes
protected:
	OpVector<BTClientConnector> m_list;

protected:
	BOOL				m_shutdown;
	OpTimer				m_check_timer;

// Operations
public:
	void		Clear();
	static BTClientConnector *OnAccept(Transfer* pConnection, P2PSocket *socket, DownloadBase *download, OpByteBuffer *buffer = NULL);
	void		ShutdownRequests();

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

	OP_STATUS	SendPeerExchange(time_t lastUpdateTime, DownloadSourceCollection& sources);

protected:
	void		Add(BTClientConnector* client);
	void		Remove(BTClientConnector* client);

// List Access
public:
	UINT32		GetCount() { return m_list.GetCount(); }
	BTClientConnector *Get(UINT32 idx) { return m_list.Get(idx); }

	inline BOOL Check(BTClientConnector* client)
	{
		return m_list.Find(client) != -1;
	}

	friend class P2PServerConnector;
	friend class BTClientConnector;
	friend class BTTracker;

};

#endif // BT_CLIENT_H
