/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include "adjunct/bittorrent/bt-util.h"
#include "adjunct/bittorrent/bt-benode.h"
#include "adjunct/bittorrent/bt-client.h"
#include "adjunct/bittorrent/bt-download.h"
#include "adjunct/bittorrent/bt-upload.h"

extern OpString* GetSystemIp(OpString* pIp);

// #define BUFFER_VALIDATION

//////////////////////////////////////////////////////////////////////
// BTClientConnector construction

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::BTClientConnector()"
#endif

BTClientConnector::BTClientConnector() :
	m_upload(NULL),
	m_download(NULL),
	m_downloadtransfer(NULL),
	m_shake(FALSE),
	m_online(FALSE),
	m_closing(FALSE),
	m_isupload(FALSE),
	m_ready_to_send(TRUE),
	m_last_keepalive(0),
	m_upload_stage(BTClientConnector::StageDownload),
	m_download_stage(BTClientConnector::StageDownload),
	m_peer_supports_extensions(FALSE),
	m_pex_message_id(0),
	m_pex_last_time(0),
	m_refcount(0)
{
	ENTER_METHOD;

	m_transfer = OP_NEW(Transfer, (*this));

//	g_Transfers->StartTimer();

	BT_RESOURCE_ADD("BTClientConnector", this);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::~BTClientConnector()"
#endif

BTClientConnector::~BTClientConnector()
{
	ENTER_METHOD;

//	OP_ASSERT( m_downloadtransfer == NULL );
//	OP_ASSERT( m_download == NULL );
//	OP_ASSERT( m_upload == NULL );

	// let the transfer class delete itself when it is ready
	// and has no outstanding requests or callbacks
	if(m_transfer && g_Transfers->Check(m_transfer))
	{
		m_transfer->StartClosing();
		m_transfer = NULL;
	}
	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);

	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector initiate a new connection

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::Connect()"
#endif

OP_STATUS BTClientConnector::Connect(DownloadTransferBT* downloadtransfer)
{
	ENTER_METHOD;

	OP_ASSERT( m_download == NULL );

	// if this happens, we need to reinitialize the class before reconnecting anywhere
	OP_ASSERT(!downloadtransfer->m_interested);
	OP_ASSERT(downloadtransfer->m_choked);

	DownloadSource* source = downloadtransfer->m_source;

	if(m_transfer->ConnectTo(source->m_address, source->m_port) != OpStatus::OK)
	{
		return OpStatus::ERR;
	}
	m_download			= (BTDownload *)downloadtransfer->m_download;
	m_downloadtransfer	= downloadtransfer;

	m_download->ClientConnectors()->Add(this);

	LEAVE_METHOD;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector attach to existing connection

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::AttachTo()"
#endif

BOOL BTClientConnector::AttachTo(Transfer* connection)
{
	ENTER_METHOD;

	if(m_transfer->IsClosing())
	{
		return FALSE;
	}

	BOOL success = FALSE;

	if(connection->m_socket.Get() != NULL)
	{
		connection->m_socket.Get()->SetListener(m_transfer);
	}
	m_transfer->AttachTo(connection);
	m_isupload = TRUE;

	if(OnP2PDataReceived(m_transfer, m_transfer->GetInputDataBuffer()))
	{
		m_transfer->GetInputDataBuffer()->Empty();
		success = TRUE;
	}

	// Note: m_transfer may be now NULL if OnP2PDataReceived failed (DSK-335914)

//	OP_ASSERT(m_download != NULL);
	if(m_download)
	{
		m_download->ClientConnectors()->Add(this);
	}

	LEAVE_METHOD;

	return success;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector close

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::Close()"
#endif

void BTClientConnector::Close(BOOL force)
{
	ENTER_METHOD;

	OP_ASSERT( this != NULL );

	if(m_closing && !force)
	{
		return;
	}
	m_closing = TRUE;

	DEBUGTRACE8_RES(UNI_L("** CLOSING BTClientConnector 0x%08x\n"), this);
	// let the transfer class delete itself when it is ready
	// and has no outstanding requests or callbacks
	if(m_transfer && g_Transfers->Check(m_transfer))
	{
		m_transfer->StartClosing();
//		m_transfer->Close();
		m_transfer = NULL;
	}
	if(m_downloadtransfer != NULL)
	{
		if(g_Downloads->Check(m_download) && m_download != NULL)
		{
			m_downloadtransfer->DecrementAvailableBlockCountOnDisconnect();
		}
		m_downloadtransfer->StartClosing();
		m_downloadtransfer->Close(TS_UNKNOWN);
	}
	m_downloadtransfer = NULL;
	if(g_Downloads->Check(m_download) && m_download != NULL)
	{
		if(m_download->Uploads()->Check(m_upload) && m_upload != NULL)
		{
			CloseUpload();
		}
//		m_download->ClientConnectors()->Remove(this);
		m_download = NULL;
	}

	LEAVE_METHOD;
}

void BTClientConnector::CloseUpload()
{
	if(m_upload)
	{
		m_upload->m_live = FALSE;
		m_upload->Close();
		m_upload->Release();
		m_upload = NULL;
	}
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector send a packet

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::Send() 1"
#endif

void BTClientConnector::Send(BTPacket* packet, BOOL release)
{
	ENTER_METHOD;

	OP_ASSERT( m_online );

	if (!packet)
		return;

	if(m_transfer && m_online && g_Transfers->Check(m_transfer))
	{
		OP_ASSERT(packet->m_protocol == PROTOCOL_BT);

		OpByteBuffer *buf = OP_NEW(OpByteBuffer, ());
		if(buf)
		{
			packet->ToBuffer(buf, TRUE);

			m_output_commands.Append(buf);

			OnWrite();
		}
	}

	if(release)
	{
		packet->Release();
	}

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::Send() 2"
#endif

void BTClientConnector::Send(OpByteBuffer *buffer)
{
	ENTER_METHOD;

	OP_ASSERT( m_online );

	if(m_transfer && m_online && g_Transfers->Check(m_transfer) && buffer)
	{
		if(buffer->DataSize() > BT_REQUEST_SIZE)
		{
			m_send_queue.Append(buffer);
		}
		else
		{
			m_output_commands.Append(buffer);
		}
		OnWrite();
	}

	LEAVE_METHOD;
}
// called when data has been sent successfully to the receiving socket

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PDataSent()"
#endif

void BTClientConnector::OnP2PDataSent(P2PConnection *instance, unsigned char *data_buffer_sent, UINT32 bytes_sent)
{
	ENTER_METHOD;

	m_ready_to_send = TRUE;

	DEBUGTRACE_UP(UNI_L("OnP2PDataSent: %d bytes to host: "), bytes_sent);

	if(m_transfer)
	{
		DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);
	}
//	OP_ASSERT(m_output_commands.HasDataBuffer(data_buffer_sent) || m_send_queue.HasDataBuffer(data_buffer_sent));

	if(m_output_commands.GetCount() && m_output_commands.Size() < BT_REQUEST_SIZE)
	{
		DEBUGTRACE_UP(UNI_L("Removing %d command bytes to host: "), bytes_sent);
		if(m_transfer)
		{
			DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);
		}
		m_output_commands.Remove(bytes_sent, TRUE);
	}
	else
	{
//		OP_ASSERT(m_send_queue.HasDataBuffer(data_buffer_sent));

		OpByteBuffer *buffer = m_send_queue.GetFirstBuffer(TRUE, FALSE);
		if(buffer)
		{
			DEBUGTRACE_UP(UNI_L("Removing %d data bytes to host: "), bytes_sent);
			if(m_transfer)
			{
				DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);
			}
			// might be a command buffer, so if we can't find it, then no problem
			if(m_send_queue.Remove(bytes_sent, TRUE) == OpStatus::OK)
			{
				if(m_upload)
				{
					m_upload->m_transferSpeed.AddPoint(bytes_sent);
				}
			}
		}
	}
	LEAVE_METHOD;

	DEBUGTRACE_UP(UNI_L("QUEUE SIZE %d bytes to host "), m_send_queue.Size());
	DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::SetUploadStage()"
#endif

void BTClientConnector::SetUploadStage(const BTClientConnector::BTTransferStage stage)
{
	ENTER_METHOD;

	m_upload_stage = stage;

	if(m_upload_stage == BTClientConnector::StageDownload)
	{
		DEBUGTRACE_THROTTLE(UNI_L("*** CONTINUE UPLOAD, cps: %.02f\n"), g_Transfers->GetCalculatedUploadSpeed());
		OnWrite();
	}
	else
	{
		DEBUGTRACE_THROTTLE(UNI_L("*** PAUSE UPLOAD, cps: %.02f\n"), g_Transfers->GetCalculatedUploadSpeed());
	}
	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::SetDownloadStage()"
#endif

void BTClientConnector::SetDownloadStage(const BTClientConnector::BTTransferStage stage)
{
	ENTER_METHOD;

	m_download_stage = stage;

	if(g_Transfers->Check(GetTransfer()))
	{
		if(m_download_stage == BTClientConnector::StageDownload)
		{
			GetTransfer()->PauseReceive(FALSE);

			DEBUGTRACE_THROTTLE(UNI_L("*** CONTINUE RECIEVE, cps: %.02f\n"), g_Transfers->GetCalculatedDownloadSpeed());
/*
			if(GetTransfer()->IsInputDataPending())
			{
				if(OnP2PDataReceived(GetTransfer(), GetTransfer()->GetInputDataBuffer()))
				{
					if(GetTransfer())
					{
						GetTransfer()->GetInputDataBuffer()->Empty();
					}
				}
			}
*/
		}
		else if(m_download_stage == BTClientConnector::StagePaused)
		{
			GetTransfer()->PauseReceive(TRUE);

			DEBUGTRACE_THROTTLE(UNI_L("*** PAUSE RECIEVE, cps: %.02f\n"), g_Transfers->GetCalculatedDownloadSpeed());
		}
	}
	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector write event

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnWrite()"
#endif

BOOL BTClientConnector::OnWrite()
{
	ENTER_METHOD;

//	time_t tNow = op_time(NULL);

	if(g_Transfers->Check(m_transfer) == FALSE)
	{
		LEAVE_METHOD;
		return FALSE;
	}
	BOOL result = FALSE;
	if(m_ready_to_send)
	{
		if(m_output_commands.Size())
		{
			DEBUGTRACE_UP(UNI_L("SENDING CMD BUFFER %d bytes to host: "), m_output_commands.Size());
			DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);

			if(OpStatus::IsSuccess(m_output_commands.MergeBuffers()))
			{
				OpByteBuffer *buf = m_output_commands.GetFirstBuffer(FALSE, TRUE);
				if(buf)
				{
					m_ready_to_send = FALSE;
					result = OpStatus::IsSuccess(m_transfer->OnWrite(buf));
				}
			}
		}
		else if(m_send_queue.Size() > 0)
		{
			if(m_upload_stage != BTClientConnector::StagePaused)
			{
				OpByteBuffer *buf = m_send_queue.GetFirstBuffer(FALSE, TRUE);

				if(buf)
				{
					DEBUGTRACE_UP(UNI_L("SENDING QUEUE %d bytes to host: "), buf->DataSize());
					DEBUGTRACE_UP(UNI_L("%s\n"), (uni_char *)m_transfer->m_address);

					m_ready_to_send = FALSE;
					result = OpStatus::IsSuccess(m_transfer->OnWrite(buf));
				}
			}
		}
	}

	LEAVE_METHOD;

	return result;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector run event

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnRun()"
#endif

BOOL BTClientConnector::OnRun()
{
	ENTER_METHOD;

	if(g_Transfers->Check(m_transfer) == FALSE)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	m_transfer->OnRun();

	time_t tNow = op_time(NULL);

	if(!m_transfer->IsConnected())
	{
		if(tNow - m_transfer->ConnectedTime() > 600 )	// timeout, hard coded for now
		{
			Close();
			return FALSE;
		}
	}
	else if(!m_online )
	{
		if ( tNow - m_transfer->ConnectedTime() > 320 )	// timeout handshake, hard coded
		{
			Close();
			return FALSE;
		}
	}
	else
	{
		if ( tNow - m_transfer->m_mInput.tLast > 60 * 2 )	// hard coded
		{
			Close();
			return FALSE;
		}
		else if ( tNow - m_transfer->m_mOutput.tLast > 60 / 2 && m_output_commands.Size() == 0 && m_send_queue.Size() == 0)	// more hardcoding
		{
			UINT32 dwZero = 0;
			OpByteBuffer *buf = OP_NEW(OpByteBuffer, ());
			if(buf)
			{
				buf->Append((unsigned char *)&dwZero, 4);
				m_output_commands.Append(buf);
				OnWrite();
			}
		}
	}
	LEAVE_METHOD;

	m_download->OnRun();
	return TRUE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PDestructing()"
#endif

void BTClientConnector::OnP2PDestructing(P2PConnection *instance)
{
	if(instance == m_transfer)
	{
		m_transfer = NULL;
	}
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector connection establishment event

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PConnected()"
#endif

void BTClientConnector::OnP2PConnected(P2PConnection * /* instance */)
{
	ENTER_METHOD;

	DEBUGTRACE_CONNECT(UNI_L("*** CONNECT (%d peers still connected)\n"), m_download->GetTransferCount());

	OnConnected();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnConnected()"
#endif

BOOL BTClientConnector::OnConnected()
{
	ENTER_METHOD;

	SendHandshake(TRUE, FALSE);

	LEAVE_METHOD;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector connection loss event

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PDisconnected()"
#endif

void BTClientConnector::OnP2PDisconnected(P2PConnection *instance)
{
	ENTER_METHOD;

	DEBUGTRACE_CONNECT(UNI_L("Disconnected: %s (UA: "), (uni_char *)instance->m_address);
	DEBUGTRACE_CONNECT(UNI_L("%s) "), (uni_char *)m_useragent);
	DEBUGTRACE_CONNECT(UNI_L("(%d peers still connected)\n"), m_download ? m_download->GetTransferCount() : 0);

	OnDropped(FALSE);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PConnectError()"
#endif

void BTClientConnector::OnP2PConnectError(P2PConnection * instance, OpSocket::Error socket_error)
{
	ENTER_METHOD;

	if(!m_closing)
	{
		DEBUGTRACE_CONNECT(UNI_L("Connect error: %s\n"), (uni_char *)instance->m_address);
	}
	OnDropped(socket_error != OpSocket::NETWORK_NO_ERROR);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnDropped()"
#endif

void BTClientConnector::OnDropped(BOOL error)
{
	ENTER_METHOD;

	if(!m_transfer->IsConnected())
	{
		// send connect timeout message
	}
	else if (!m_online )
	{
		// send handshake timeout message
	}
	else
	{
		// send client dropped message
	}
	Close();

	LEAVE_METHOD;
}

// this method is called by the socket layer when data has been received. This read might
// have been initiated by BTClientConnector::OnRead().
// If TRUE is returned from this method, it means the data has been handled and the socket
// layer is free to reset the buffer.
// FALSE means the data is not complete and that the socket layer should continue filling
// up the same buffer with data

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PDataReceived()"
#endif

BOOL BTClientConnector::OnP2PDataReceived(P2PConnection *instance, OpByteBuffer *inputbuffer)
{
	ENTER_METHOD;

	BOOL success = TRUE;
	BOOL handled = FALSE;

	if(m_closing)
	{
		return TRUE;
	}
	else if(m_online)
	{
		BTPacket* packet;

		if(IsDownloadPaused())
		{
			handled = FALSE;
		}
		else if(BTPacket::IsValidPiece(inputbuffer))
		{
			DEBUGTRACE(UNI_L("%s\n"), (uni_char *)GetTransfer()->m_address);

			packet = BTPacket::ReadBuffer(inputbuffer);

			while(packet && !m_closing)
			{
				success = OnPacket(packet);

				if(!m_online) success = FALSE;

				packet->Release();
				if(!success) break;

				packet = BTPacket::ReadBuffer(inputbuffer);
			}
			// if we processed the whole buffer, we can clear it. Otherwise, we want to add
			// more data before we process it.
			if(inputbuffer->DataSize() == 0)
			{
				handled = TRUE;
			}
		}
		else
		{
			// Some DOS protection..
			if(inputbuffer->DataSize() > (BT_REQUEST_SIZE + 100))
			{
				DEBUGTRACE(UNI_L("*** UNPROCESSED LARGE BUFFER: %d from "), inputbuffer->DataSize());
				DEBUGTRACE(UNI_L("%s\n"), (uni_char *)GetTransfer()->m_address);
			}
			if(inputbuffer->DataSize() > (BT_REQUEST_SIZE + 100000))
			{
				DEBUGTRACE(UNI_L("*** UNPROCESSED LARGE BUFFER 2: %d from "), inputbuffer->DataSize());
				DEBUGTRACE(UNI_L("%s\n"), (uni_char *)GetTransfer()->m_address);
				Close();
			}
		}
	}
	else
	{
		if(!m_shake && m_transfer->GetInputDataBuffer()->DataSize() >= BT_PROTOCOL_HEADER_LEN + 8 + sizeof(SHAStruct))
		{
			handled = success = OnHandshake1();
		}
		if(success && m_shake && m_transfer->GetInputDataBuffer()->DataSize() >= sizeof(SHAStruct))
		{
			handled = success = OnHandshake2();
		}
/*
		else
		if(success && m_shake && m_isupload)
		{
			OnRead();
		}
*/
	}
	LEAVE_METHOD;

	return handled;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnP2PReceiveError()"
#endif

void BTClientConnector::OnP2PReceiveError(P2PConnection *instance, OpSocket::Error socket_error)
{
	ENTER_METHOD;

	LEAVE_METHOD;
}


//////////////////////////////////////////////////////////////////////
// BTClientConnector read event

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnRead()"
#endif

BOOL BTClientConnector::OnRead()
{
	ENTER_METHOD;

	if(g_Transfers->Check(m_transfer) == FALSE)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	BOOL success = m_transfer->OnRead();

	LEAVE_METHOD;

	return success;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector handshaking

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::SendHandshake()"
#endif

BOOL BTClientConnector::SendHandshake(BOOL part1, BOOL part2)
{
	ENTER_METHOD;

	OP_ASSERT( m_download != NULL );

	OpByteBuffer *output = OP_NEW(OpByteBuffer, ());

	BOOL result = TRUE;

	if(output)
	{
		if(part1)
		{
			unsigned char reserved[8] = {0, 0, 0, 0, 0, 0, 0, 0};

			reserved[5] |= 0x10; // see http://www.rasterbar.com/products/libtorrent/extension_protocol.html

			output->Append((unsigned char *)BT_PROTOCOL_HEADER, 20);
			output->Append((unsigned char *)&reserved, 8);
			output->Append((unsigned char *)&m_download->m_pBTH, sizeof(SHAStruct) );
		}
		if(part2)
		{
			output->Append((unsigned char *)&m_download->m_peerid, sizeof(SHAStruct) );
		}
		DEBUGTRACE_UP(UNI_L("*** sending HANDSHAKE to %s, "), (uni_char *)m_transfer->m_address);
		DEBUGTRACE_UP(UNI_L("%s, "), part1 ? UNI_L("TRUE") : UNI_L("FALSE"));
		DEBUGTRACE_UP(UNI_L("%s\n"), part2 ? UNI_L("TRUE") : UNI_L("FALSE"));
		m_output_commands.Append(output);
		result = OnWrite();
	}

	LEAVE_METHOD;
	return result;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnHandshake1()"
#endif

BOOL BTClientConnector::OnHandshake1()
{
	ENTER_METHOD;

	OP_ASSERT(!m_online );
	OP_ASSERT(!m_shake );

	P2PPacket *packet = g_PacketPool->New();
	if(packet == NULL)
	{
		return FALSE;
	}

	if(OpStatus::IsError(packet->Write(m_transfer->GetInputDataBuffer()->Buffer(), m_transfer->GetInputDataBuffer()->DataSize())))
	{
		packet->Release();
		return FALSE;
	}
	BT_HANDSHAKE_HEADER hdr;

	hdr.pstrlen = packet->ReadByte();

	packet->Read(hdr.pstr, 19);
	packet->Read(hdr.reserved, 8);
	packet->Read(hdr.info_hash, 20);

	const char *greeting = BT_PROTOCOL_HEADER;

	greeting++;

	if(hdr.pstrlen != BT_PROTOCOL_HEADER_LEN || memcmp(hdr.pstr, greeting, BT_PROTOCOL_HEADER_LEN - 1) != 0)
	{
//		OP_ASSERT( FALSE );
		Close();
		packet->Release();
		return FALSE;
	}

	SHAStruct filehash;

	memcpy(&filehash , &hdr.info_hash, sizeof(SHAStruct));

	/*
	The bit selected for the extension protocol is bit 20 from the right (counting starts at 0).
	So (reserved_byte[5] & 0x10) is the expression to use for checking if the client supports extended messaging.
	*/
	if(hdr.reserved[5] & 0x10)
	{
		m_peer_supports_extensions = TRUE;
	}
	// should only be the peer ID left after this:
	m_transfer->GetInputDataBuffer()->Remove(48);

	packet->Release();

	if(m_transfer->IsInitiated())
	{
		OP_ASSERT( m_download != NULL );
//		OP_ASSERT( m_downloadtransfer != NULL );

		if(filehash != m_download->m_pBTH)
		{
			// wrong file, send error message to client
			Close();
			return FALSE;
		}
		else if(!m_download->IsTrying())
		{
			// torrent is not active
			Close();
			return FALSE;
		}
	}
	else
	{
		OP_ASSERT( m_download == NULL );
		OP_ASSERT( m_downloadtransfer == NULL );

		m_download = (BTDownload *)g_Downloads->FindByBTH(&filehash);

		if(m_download == NULL )
		{
			// unknown file
			Close();
			return FALSE;
		}
		else if(!m_download->IsTrying() )
		{
			// torrent is not active
			m_download = NULL;
			Close();
			return FALSE;
		}
		int transfers = m_download->GetTransferCount(dtsCountAll);

		if(transfers >=	BT_MAX_TOTAL_CONNECTIONS)
		{
#ifdef _DEBUG
			DEBUGTRACE8(UNI_L("*** MAXIMUM number of TOTAL connections reached\n"), 0);
#endif
			m_download = NULL;
			Close();
			return FALSE;
		}
	}

	OP_ASSERT( m_download != NULL );
	OP_ASSERT( m_download->m_pBTH == filehash );

	m_transfer->SetGUID(&m_download->m_pBTH);

	BOOL result = TRUE;
	if(!m_transfer->IsInitiated())
	{
		if(m_isupload)
		{
			result = SendHandshake(TRUE, TRUE);
		}
		else
		{
			result = SendHandshake(TRUE, FALSE);
		}
	}
	else
	{
		if(m_isupload == FALSE)
		{
			result = SendHandshake(FALSE, TRUE);
		}
	}
	m_shake = TRUE;

	LEAVE_METHOD;

	return result;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnHandshake2()"
#endif

BOOL BTClientConnector::OnHandshake2()
{
	ENTER_METHOD;

	m_GUID = *(SHAStruct *)m_transfer->GetInputDataBuffer()->Buffer();
	m_transfer->GetInputDataBuffer()->Remove(sizeof(SHAStruct));

	OP_ASSERT( m_download != NULL );

	if(m_transfer->IsInitiated())
	{
//		OP_ASSERT( m_downloadtransfer != NULL );
		if(m_downloadtransfer == NULL)
		{
			return FALSE;
		}
		memcpy( &m_downloadtransfer->m_source->m_GUID, &m_GUID, 16 );
	}
	else if(!m_download->IsMoving() && !m_download->IsPaused())
	{
		OP_ASSERT(m_downloadtransfer == NULL);

		m_downloadtransfer = m_download->CreateTorrentTransfer( this );

		if(m_downloadtransfer == NULL )
		{
			// unknown file
			m_download = NULL;
			Close();
			return FALSE;
		}
	}

	OP_ASSERT( m_upload == NULL );
	m_upload = OP_NEW(UploadTransferBT, (m_download, m_transfer->m_host, m_transfer->m_address, &m_GUID));
	if (m_upload)
		m_upload->AddRef();

	m_online = TRUE;

//	m_useragent.Set("BitTorrent");
	DetermineUserAgent();

	BOOL success = TRUE;
	if(!m_isupload && !m_transfer->IsInitiated())
	{
		success = SendHandshake( FALSE, TRUE );
	}
	if (success)
	{
		success = OnOnline();
	}

	LEAVE_METHOD;

	return success;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector online handler

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::DetermineUserAgent()"
#endif

// See http://wiki.theory.org/BitTorrentSpecification#peer_id for the peer ids
void BTClientConnector::DetermineUserAgent()
{
	ENTER_METHOD;

	if(m_GUID.b[0] == '-' && m_GUID.b[7] == '-' )
	{
		const char *user_agent;
		char ua_short[3];

		ua_short[0] = m_GUID.b[1] & 0xDF;
		ua_short[1] = m_GUID.b[2] & 0xDF;
		ua_short[2] = 0;

		if(ua_short[0] == 'A' && ua_short[1] == 'Z')
		{
			user_agent = "Azureus";
		}
		else if(ua_short[0] == 'M' && ua_short[1] == 'T')
		{
			user_agent = "MoonlightTorrent";
		}
		else if(ua_short[0] == 'L' && ua_short[1] == 'T' )
		{
			user_agent = "libtorrent";
		}
		else if(ua_short[0] == 'B' && ua_short[1] == 'X')
		{
			user_agent = "Bittorrent X";
		}
		else if(ua_short[0] == 'T' && ua_short[1] == 'S' )
		{
			user_agent = "Torrentstorm";
		}
		else if(ua_short[0] == 'S' && ua_short[1] == 'S' )
		{
			user_agent = "Swarmscope";
		}
		else if(ua_short[0] == 'X' && ua_short[1] == 'T' )
		{
			user_agent = "XanTorrent";
		}
		else if(ua_short[0] == 'B' && ua_short[1] == 'B' )
		{
			user_agent = "BitBuddy";
		}
		else if(ua_short[0] == 'T' && ua_short[1] == 'N' )
		{
			user_agent = "TorrentDOTnet";
		}
		else if(ua_short[0] == 'U' && ua_short[1] == 'T' )
		{
			user_agent = "µTorrent";
		}
		else if(ua_short[0] == 'C' && ua_short[1] == 'C' )
		{
			user_agent = "BitComet";
		}
		else //Unknown client using this naming.
		{
			user_agent = ua_short;
		}

		m_useragent.Set(user_agent);
		m_useragent.AppendFormat(UNI_L(" %i.%i.%i.%i"),
			m_GUID.b[3] - '0', m_GUID.b[4] - '0', m_GUID.b[5] - '0', m_GUID.b[6] - '0');
	}
	else if(m_GUID.b[4] == '-' && m_GUID.b[5] == '-' && m_GUID.b[6] == '-' && m_GUID.b[7] == '-' )
	{
		const char *user_agent;
		char ua_short[2];

		ua_short[0] = m_GUID.b[0] & 0xDF;
		ua_short[1] = 0;

		switch (ua_short[0])
		{
			case 'S':
				user_agent = "Shadow";
				break;

			case 'U':
				user_agent = "UPnP NAT BT";
				break;

			case 'T':
				user_agent = "BitTornado";
				break;

			case 'A':
				user_agent = "ABC";
				break;

			default: //Unknown client using this naming.
				user_agent = ua_short;
				break;
		}

		m_useragent.Set(user_agent);
		m_useragent.AppendFormat(UNI_L(" %i%i%i"), m_GUID.b[1] - '0', m_GUID.b[2] - '0', m_GUID.b[3] - '0');
	}
	else if(m_GUID.b[0] == 'O' && m_GUID.b[1] == 'P' )
	{
		// looks like Opera!!
		m_useragent.Set("Opera");

		// "OP7777"

		m_useragent.AppendFormat(UNI_L(" %i%i%i%i"),
			m_GUID.b[2] - '0', m_GUID.b[3] - '0', m_GUID.b[4] - '0', m_GUID.b[5] - '0');
	}
	// Bram's client now uses this style... 'M3-4-2--' or 'M4-20-8-'.
	else if(m_GUID.b[0] == 'M' && m_GUID.b[1] == '3' && m_GUID.b[2] == '-' && m_GUID.b[3] == '9')
	{
		m_useragent.Append("BitTorrent 3.9");
	}
	else if(m_GUID.b[0] == 'M' && (m_GUID.b[1] == '4' || m_GUID.b[1] == '5'))
	{
		m_useragent.AppendFormat(UNI_L("BitTorrent %c.%c.%c"),
			m_GUID.b[1], m_GUID.b[3], m_GUID.b[5]);
	}
	else
	{
		m_useragent.AppendFormat(UNI_L(" %c.%c.%c.%c"),
			m_GUID.b[0], m_GUID.b[1], m_GUID.b[2], m_GUID.b[3]);
	}
	if ( m_downloadtransfer != NULL )
	{
//		m_downloadtransfer->m_useragent.Append(m_useragent);
		if(m_downloadtransfer->m_source != NULL )
			m_downloadtransfer->m_source->m_useragent.Append(m_useragent);
	}
//	if(m_upload != NULL)
//	{
//		m_upload->m_useragent.Append(m_useragent);
//	}
	if(m_useragent.IsEmpty())
	{
		OP_ASSERT(FALSE);
	}
	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector online handler

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnOnline()"
#endif

BOOL BTClientConnector::OnOnline()
{
	ENTER_METHOD;

	OP_ASSERT( m_online );
	OP_ASSERT( m_download != NULL );
	OP_ASSERT( m_upload != NULL );

	// we're online!!
	if(BTPacket* pBitfield = m_download->CreateBitfieldPacket())
	{
		DEBUGTRACE_UP(UNI_L("*** sending BITFIELD to %s\n"), (uni_char *)m_transfer->m_address);
		Send( pBitfield );
	}
	if(m_downloadtransfer && !m_downloadtransfer->OnConnected()) return FALSE;
	if(m_upload && !m_upload->OnConnected()) return FALSE;

	LEAVE_METHOD;

	return TRUE;
}

void BTClientConnector::SendKeepAlive()
{
	time_t tNow = op_time(NULL);

	if(m_online && m_last_keepalive + 60 < tNow)
	{
		m_last_keepalive = tNow;

		Send(BTPacket::New(BT_PACKET_KEEPALIVE));

		// check if we're snubbed by the peer. If we have outstanding requests and have no received
		// any data for 2 minutes, we assume we're snubbed and cancel all outstanding requests
/*
		if(m_downloadtransfer && m_downloadtransfer->m_choked == FALSE && m_downloadtransfer->IsSnubbed() == FALSE)
		{
			if(m_downloadtransfer->m_sourcerequest + 120 < tNow)
			{
				if(m_downloadtransfer->m_requestedCount > 0)
				{
					m_downloadtransfer->OnSnubbed();
					m_downloadtransfer->SetSnubbed(TRUE);
				}
			}
		}
*/
//		m_downloadtransfer->SendRequests();
	}
}

OP_STATUS BTClientConnector::SendPeerExchange(time_t lastUpdateTime, DownloadSourceCollection& sources)
{
	OpByteBuffer compact_string;
	OpByteBuffer flags_string;
	OpByteBuffer output;
	DownloadSource *source;
	BTCompactPeer p;
	UINT32 count = 0;

	if(!g_Transfers->Check(GetTransfer()))
	{
		return OpStatus::OK;
	}
	for(source = sources.First(); source != NULL; source = source->Suc())
	{
		if(m_pex_last_time < source->m_added_time && GetTransfer()->m_address.Compare(source->m_pex_added_by))
		{
			p.m_peer.Set(source->m_address);
			p.m_port = source->m_port;

			RETURN_IF_ERROR(m_download->PeerToString(p, compact_string));

			count++;
		}
	}
	m_pex_last_time = op_time(NULL);

	if(count)
	{
		UINT32 idx;
		unsigned char zero = 0;
		for(idx = 0; idx < count; idx++)
		{
			RETURN_IF_ERROR(flags_string.Append(&zero, 1));
		}
		OpString8 tmp;
		RETURN_IF_ERROR(tmp.AppendFormat("d5:added%d:", compact_string.DataSize()));
		RETURN_IF_ERROR(output.Append((unsigned char *)tmp.CStr(), tmp.Length()));
		RETURN_IF_ERROR(output.Append(compact_string.Buffer(), compact_string.DataSize()));

		tmp.Empty();

		RETURN_IF_ERROR(tmp.AppendFormat("7:added.f%d:", flags_string.DataSize()));
		RETURN_IF_ERROR(output.Append((unsigned char *)tmp.CStr(), tmp.Length()));
		RETURN_IF_ERROR(output.Append(flags_string.Buffer(), flags_string.DataSize()));
		RETURN_IF_ERROR(output.Append((unsigned char *)"7:dropped0:e", 12));

//		HandlePeerExtensionList(output);

		BTPacket *packet = BTPacket::New(BT_PACKET_EXTENSIONS);
		if(packet)
		{
			packet->WriteByte(1);
			packet->Write(output.Buffer(), output.DataSize());

			DEBUGTRACE_PEX(UNI_L("PEX: Sending peer exchange msg to %s"), GetTransfer()->m_address.CStr());
			DEBUGTRACE_PEX(UNI_L(", peers: %d"), count);
			DEBUGTRACE_PEX(UNI_L(", useragent: '%s'"), m_useragent.CStr());
			DEBUGTRACE8_PEX(UNI_L(", msg: '%s'\n"), (char *)output.Buffer());

			Send(packet, TRUE);
		}
	}
	return OpStatus::OK;
}
OP_STATUS BTClientConnector::HandlePeerExtensionList(OpByteBuffer& buffer)
{
	OpString strSystemIP;

	// we don't want to add ourself in the list of peers
	GetSystemIp(&strSystemIP);

	BENode* root = BENode::Decode(&buffer);
	if(root && root->IsType(BENode::beDict))
	{
		BENode* m = root->GetNode("added");
		if(m && m->IsType(BENode::beString))
		{
			BENode* f = root->GetNode("added.f");
			if(f && f->IsType(BENode::beString))
			{
				OpString flags;
				RETURN_IF_ERROR(f->GetString(flags));
				if(flags.HasContent())
				{
				}
			}
			OpString address;
			RETURN_IF_ERROR(m->GetString(address));
			if(address.HasContent())
			{
				OpVector<BTCompactPeer> peers;
				UINT32 i;

				RETURN_VALUE_IF_ERROR(m_download->StringToPeerlist(address, peers), FALSE);

				for(i = 0; i < peers.GetCount(); i++)
				{
					BTCompactPeer *peer = peers.Get(i);
					if(peer)
					{
						if(strSystemIP.Compare(peer->m_peer) == 0 && peer->m_port == (UINT32)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort))
						{
							// it's us! Skip this one :-)
							continue;
						}
						DEBUGTRACE_PEX(UNI_L("PEX: ADDING PEER: %s\n"), peer->m_peer.CStr());

						if(m_download->AddSourceBT(NULL, peer->m_peer, (WORD)peer->m_port, TRUE, GetTransfer()->m_address))
						{
						}
					}
				}
				peers.DeleteAll();
			}
		}
	}
	OP_DELETE(root);

	return OpStatus::OK;
}

// peer exchange format:
// d5:added6:xxxxxx7:added.f1:x
// added is a string containing a compact peer IP and port, 6 bytes per peer
// added.f is a string containing a byte of flags per peer in the above collection, where flags apparently is:
// 0x01 - peer supports encryption
// 0x02 - peer is a seed

OP_STATUS BTClientConnector::OnExtensionPacket(BTPacket *packet)
{
	// peer sent a extensions message

	OpByteBuffer buffer;
	UINT32 len = packet->m_length;
	byte type = packet->ReadByte();

	if(len)
	{
		RETURN_VALUE_IF_ERROR(buffer.SetDataSize(len+1), FALSE);
		packet->Read(buffer.Buffer(), len-1);
		buffer.Buffer()[len-1] = '\0';

		// "d1:ei0e1:md6:ut_pexi1ee1:pi53193e1:v14:µTorrent 1.6.1e"
		if(type == 0)
		{
			// this is a handshake

			BENode* root = BENode::Decode(&buffer);
			if(root && root->IsType(BENode::beDict))
			{
				BENode* m = root->GetNode("m");
				if(m && m->IsType(BENode::beDict))
				{
					BENode* pex = m->GetNode("ut_pex");
					if (pex && pex->IsType(BENode::beInt))
					{
						m_pex_message_id = (UINT32)pex->GetInt();
						DEBUGTRACE_PEX(UNI_L("PEX: Got extension msg from %s"), this->GetTransfer()->m_address.CStr());
						DEBUGTRACE8_PEX(UNI_L(", msg: '%s'\n"), (char *)buffer.Buffer());
					}
				}
				OP_DELETE(root);
			}
			OpString8 encoded_string;
			OpString8 opera_string;
			OpByteBuffer new_buffer;

			RETURN_IF_ERROR(opera_string.AppendFormat("Opera %s", VER_NUM_STR));
			RETURN_IF_ERROR(encoded_string.AppendFormat("d1:ei0e1:md6:ut_pexi1ee1:pi%de1:v%d:%se",
				g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort), opera_string.Length(), opera_string.CStr()));

			new_buffer.Append((unsigned char *)encoded_string.CStr(), encoded_string.Length());

			BTPacket* new_packet = BTPacket::New(BT_PACKET_EXTENSIONS);
			if (!new_packet)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			new_packet->WriteByte(0);	// type = 0 for now
			new_packet->Write(new_buffer.Buffer(), new_buffer.DataSize());

			DEBUGTRACE_PEX(UNI_L("PEX: Sending extension msg to %s"), GetTransfer()->m_address.CStr());
			DEBUGTRACE_PEX(UNI_L(", useragent: '%s'"), m_useragent.CStr());
			DEBUGTRACE_PEX(UNI_L(", msg: '%s'\n"), encoded_string.CStr());

			Send(new_packet, TRUE);
		}
		else if(m_pex_message_id && type == m_pex_message_id)
		{
			// d5:added6:xxxxxx7:added.f1:
			DEBUGTRACE_PEX(UNI_L("PEX: Got peer list from %s"), GetTransfer()->m_address.CStr());
			DEBUGTRACE_PEX(UNI_L(", useragent: '%s'"), m_useragent.CStr());
			DEBUGTRACE8_PEX(UNI_L(", msg: '%s'\n"), (char *)buffer.Buffer());

			RETURN_IF_ERROR(HandlePeerExtensionList(buffer));
		}
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// BTClientConnector packet switch

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnector::OnPacket()"
#endif

BOOL BTClientConnector::OnPacket(BTPacket* packet)
{
	ENTER_METHOD;

	switch(packet->m_type)
	{
		case BT_PACKET_EXTENSIONS:
			{
				return OpStatus::IsSuccess(OnExtensionPacket(packet));
			}
			break;

		case BT_PACKET_KEEPALIVE:
			{
				BOOL transfer_failed;
				m_download->FileProgress(transfer_failed);

				if(m_transfer->m_upload)
				{
//					DEBUGTRACE_UP(UNI_L("KEEP-ALIVE from %s\n"), (uni_char *)m_transfer->m_address);
				}
			}
			break;

		case BT_PACKET_CHOKE:
			{
				DEBUGTRACE_CHOKE(UNI_L("*** CHOKED BY: %s, "), (uni_char *)GetTransfer()->m_address);
				DEBUGTRACE_CHOKE(UNI_L("this: 0x%08x\n"), this);
				if(m_downloadtransfer != NULL && OpStatus::IsError(m_downloadtransfer->OnChoked(packet)))
				{
					return FALSE;
				}
			}
			break;

		case BT_PACKET_UNCHOKE:
			{
				DEBUGTRACE_CHOKE(UNI_L("*** UNCHOKED BY: %s, "), (uni_char *)GetTransfer()->m_address);
				DEBUGTRACE_CHOKE(UNI_L("this: 0x%08x\n"), this);

				if(m_downloadtransfer != NULL && OpStatus::IsError(m_downloadtransfer->OnUnchoked(packet)))
				{
					return FALSE;
				}
			}
			break;

		case BT_PACKET_INTERESTED:
			{
				BOOL transfer_failed;
				m_download->FileProgress(transfer_failed);

				if(!m_upload->OnInterested(packet))
				{
					return FALSE;
				}
			}
			break;

		case BT_PACKET_NOT_INTERESTED:
			{
				BOOL transfer_failed;
				m_download->FileProgress(transfer_failed);

				if(!m_upload->OnUninterested(packet))
				{
					return FALSE;
				}
			}
			break;

		case BT_PACKET_HAVE:
			if(!m_downloadtransfer)
			{
				return FALSE;
			}
			return OpStatus::IsSuccess(m_downloadtransfer->OnHave(packet));

		case BT_PACKET_BITFIELD:
			{
				BOOL transfer_failed;
				m_download->FileProgress(transfer_failed);

				if(!m_downloadtransfer)
				{
					return FALSE;
				}
				return OpStatus::IsSuccess(m_downloadtransfer->OnBitfield(packet));
			}

		case BT_PACKET_REQUEST:
			{
				BOOL transfer_failed;

				if(m_download->IsCompleted())
				{
					m_download->FileDownloadSharing(transfer_failed);
				}
				else
				{
					m_download->FileProgress(transfer_failed);
				}
				OpByteBuffer *buffer = m_upload->OnRequest(packet);
				if(buffer == NULL)
				{
					Close();
				}
				else
				{
					Send(buffer);
				}
			}
			break;

		case BT_PACKET_PIECE:
			{
				BOOL transfer_failed;
				m_download->FileProgress(transfer_failed);

				if(!m_downloadtransfer)
				{
					return FALSE;
				}
				return OpStatus::IsSuccess(m_downloadtransfer->OnPiece(packet));
			}

		case BT_PACKET_CANCEL:
			return m_upload->OnCancel(packet);

		case BT_PACKET_HANDSHAKE:
			if(m_transfer->m_upload)
			{
				DEBUGTRACE_UP(UNI_L("HANDSHAKE from %s\n"), (uni_char *)m_transfer->m_address);
			}
			break;
	}
	LEAVE_METHOD;

	return TRUE;
}

///////////////////////////////////////////////////
///
/// A collection of BTClientConnector classes
///
///////////////////////////////////////////////////
#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::BTClientConnectors()"
#endif

BTClientConnectors::BTClientConnectors() :
	m_shutdown(FALSE)
{
	ENTER_METHOD;
	m_check_timer.SetTimerListener(this);
	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::~BTClientConnectors()"
#endif

BTClientConnectors::~BTClientConnectors()
{
	ENTER_METHOD;

	Clear();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// clear

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::Clear()"
#endif

void BTClientConnectors::Clear()
{
	ENTER_METHOD;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		BTClientConnector *client = m_list.Get(pos);

		client->Close();
	}
	ShutdownRequests();

	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// add and remove

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::Add()"
#endif

void BTClientConnectors::Add(BTClientConnector* client)
{
	ENTER_METHOD;

	INT32 pos = m_list.Find(client);

	OP_ASSERT(pos == -1);

	if(pos == -1)
	{
		m_list.Add(client);
	}

	m_check_timer.Start(CHECK_CLIENT_INTERVAL);

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::Remove()"
#endif

void BTClientConnectors::Remove(BTClientConnector* client)
{
	ENTER_METHOD;

	m_list.RemoveByItem(client);

	LEAVE_METHOD;
}

//////////////////////////////////////////////////////////////////////
// request management
/*
#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::Add() 2"
#endif

void BTClientConnectors::Add(BTTracker * request)
{
	ENTER_METHOD;

//	OP_ASSERT(!m_shutdown);

	m_requests.Add(request);

	m_check_timer.Start(CHECK_CLIENT_INTERVAL);

	LEAVE_METHOD;
}
#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::Remove() 2"
#endif

void BTClientConnectors::Remove(BTTracker* request)
{
	ENTER_METHOD;

	m_requests.RemoveByItem(request);

	LEAVE_METHOD;
}
*/
#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::ShutdownRequests()"
#endif

void BTClientConnectors::ShutdownRequests()
{
	ENTER_METHOD;

	UINT32 pos;

	for(pos = 0; pos < m_list.GetCount(); pos++)
	{
		BTClientConnector *client = m_list.Get(pos);

		if(client != NULL)
		{
//			if(client->IsClosing() && client->GetRefCount() == 0)
			{
				DEBUGTRACE(UNI_L("DESTRUCTING BTClientConnector 0x%08x\n"), client);
				client->Close(TRUE);
				OP_DELETE(client);
			}
		}
	}
	m_list.Clear();

	LEAVE_METHOD;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::OnTimeOut()"
#endif

void BTClientConnectors::OnTimeOut(OpTimer* timer)
{
	ENTER_METHOD;

	UINT32 idx;
	OpVector<BTClientConnector> delete_list;

	for(idx = 0; idx < m_list.GetCount(); idx++)
	{
		BTClientConnector *client = m_list.Get(idx);

		if(client != NULL)
		{
//			DEBUGTRACE_RES(UNI_L("** BTClientConnector 0x%08x has closing set to: "), client);
//			DEBUGTRACE_RES(UNI_L("%d, refcount: "), client->IsClosing() ? 1 : 0);
//			DEBUGTRACE_RES(UNI_L("%d\n"), client->GetRefCount());
			if(client->IsClosing() && client->GetRefCount() == 0)
			{
				delete_list.Add(client);
			}
			else
			{
				client->SendKeepAlive();
			}
		}
	}
	for(idx = 0; idx < delete_list.GetCount(); idx++)
	{
		BTClientConnector *client = delete_list.Get(idx);

		if(client != NULL)
		{
			DEBUGTRACE_RES(UNI_L("DESTRUCTING BTClientConnector 0x%08x\n"), client);
			m_list.RemoveByItem(client);
			OP_DELETE(client);
		}
	}
	delete_list.Clear();
//	DEBUGTRACE_RES(UNI_L("** BTClientConnectors left: %d\n"), m_list.GetCount());

	m_check_timer.Start(CHECK_CLIENT_INTERVAL);

	LEAVE_METHOD;
}

OP_STATUS BTClientConnectors::SendPeerExchange(time_t lastUpdateTime, DownloadSourceCollection& sources)
{
	UINT32 idx;

	for(idx = 0; idx < m_list.GetCount(); idx++)
	{
		BTClientConnector *client = m_list.Get(idx);

		if(client != NULL && client->m_pex_message_id && client->m_peer_supports_extensions)
		{
			RETURN_IF_ERROR(client->SendPeerExchange(lastUpdateTime, sources));
		}
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// accept new connections

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTClientConnectors::OnAccept()"
#endif

/* static */
BTClientConnector *BTClientConnectors::OnAccept(Transfer* connection, P2PSocket *socket, DownloadBase *download, OpByteBuffer *buffer)
{
	ENTER_METHOD;

	BTClientConnector* client = OP_NEW(BTClientConnector, ());
	if(!client)
		return NULL;

	if (!client->GetTransfer())		// OOM
	{
		OP_DELETE(client);
		return NULL;
	}
	if(connection == NULL)
	{
		OpString address;

		connection = OP_NEW(Transfer, (*client));
		if(connection == NULL)
		{
			OP_DELETE(client);
			return NULL;
		}

		OpSocketAddress::Create(&connection->m_socket_address);

		if(connection->m_socket_address == NULL)
		{
			OP_DELETE(client);
			connection->StartClosing();
			connection->Close();
			return NULL;
		}
		socket->Get()->GetSocketAddress(connection->m_socket_address);
		connection->m_socket_address->ToString(&address);

		INT32 offset = address.FindFirstOf(':');
		if(offset != -1)
		{
			address.Delete(offset);
		}
//		connection->m_socket.Attach(socket->Detach());
		connection->AcceptFrom(socket, address);

		if(buffer != NULL)
		{
			connection->GetInputDataBuffer()->Append(buffer->Buffer(), buffer->DataSize());
		}
	}
	if(!client->AttachTo(connection))
	{
		client->Close();
		connection->StartClosing();
		connection->Close();
		OP_DELETE(client);
		client = NULL;
		connection = NULL;
	}
	else
	{
		connection->StartClosing();
		connection->Close();
	}
	LEAVE_METHOD;

	return client;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
