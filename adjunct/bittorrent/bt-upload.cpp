/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
//#if defined(UNIX) && !defined(__STDC_LIMIT_MACROS)
//#define __STDC_LIMIT_MACROS
//#endif

#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "modules/url/url_man.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include <cassert>

#include "bt-info.h"
#include "dl-base.h"
#include "bt-util.h"
#include "bt-upload.h"
#include "bt-client.h"
#include "bt-download.h"
#include "bt-globals.h"
#include "connection.h"
#include "p2p-fileutils.h"
#include "bt-packet.h"

//UploadQueue g_UploadQueues;
//Uploads g_Uploads;
//UploadFiles g_UploadFiles;

#define MAXPERHOST	2	// max uploads per host

//////////////////////////////////////////////////////////////////////

UploadFile::UploadFile(UploadTransfer* upload, SHAStruct* pSHA1, OpString& name, OpString& path, UINT64 size)
:	m_bSHA1(pSHA1 != NULL),
    m_size(size),
	m_requests(0),
	m_fragments(NULL),
	m_selected(FALSE)
{
//	m_address.Set(upload->m_address);
	m_name.Set(name);
	m_path.Set(path);

	if(m_bSHA1)
	{
		m_SHA1 = *pSHA1;
	}
	upload->AddRef();
	m_transfers.Add(upload);

	BT_RESOURCE_ADD("UploadFile", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "UploadFile::~UploadFile()"
#endif

UploadFile::~UploadFile()
{
	m_fragments->DeleteChain();

	UINT32 cnt;

	for(cnt = 0; cnt < m_transfers.GetCount(); cnt++)
	{
		UploadTransfer *upload = m_transfers.Get(cnt);

		upload->Release();
	}

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// transfer operations
/*
void UploadFile::Add(UploadTransfer* upload)
{
	if(m_transfers.Find(upload) == -1)
	{
		m_transfers.Add(upload);
	}
}

BOOL UploadFile::Remove(UploadTransfer* upload)
{
	m_transfers.RemoveByItem(upload);

	return IsEmpty();
}

UploadTransfer* UploadFile::GetActive() const
{
	if(IsEmpty()) return NULL;

	for(UINT32 pos = 0; pos < m_transfers.GetCount(); pos++)
	{
		UploadTransfer* upload = (UploadTransfer*)m_transfers.Get( pos );

		if(upload->m_state != upsNull)
		{
			return upload;
		}
	}
	// return last item
	return (UploadTransfer *)m_transfers.Get(m_transfers.GetCount() - 1);
}

void UploadFile::Remove()
{
	for(UINT32 pos = 0; pos < m_transfers.GetCount(); pos++)
	{
		UploadTransfer* upload = (UploadTransfer*)m_transfers.Get( pos );

		upload->Remove();
	}
}
*/
//////////////////////////////////////////////////////////////////////
// fragments

void UploadFile::AddFragment(UINT64 offset, UINT64 length)
{
	P2PFilePart::AddMerge( &m_fragments, offset, length );
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// UploadTransfer
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

UploadTransfer::UploadTransfer(PROTOCOLID protocol, DownloadBase* download, OpString& host, OpString& address)
:	m_protocol(protocol),
	m_state(upsNull),
	m_diskfile(NULL),
	m_live(TRUE),
	m_requestCount(0),
	m_uploaded(0),
	m_download(download),
	m_ref_count(0),
	m_rotatetime(0)
{
	ClearRequest();

	m_address.Set(address);
	m_host.Set(host);

	OP_ASSERT(download != NULL);

	if(m_download)
	{
		m_download->Uploads()->Add(this);
	}

	BT_RESOURCE_ADD("UploadTransfer", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "UploadTransfer::~UploadTransfer()"
#endif

UploadTransfer::~UploadTransfer()
{
	if(m_download)
	{
		m_download->Uploads()->Remove(this);
	}

	CloseFile();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// UploadTransfer remove record
/*
void UploadTransfer::Remove(BOOL message)
{
	OP_ASSERT( this != NULL );

	DEBUGTRACE(UNI_L("**** REMOVING uploadtransfer: 0x%08x\n"), this);

	if(message && m_filename.HasContent() )
	{
		// send message about upload being removed
	}
//	m_uploaded = 1;
//	Close( FALSE );

	delete this;	// self-destruct
}

//////////////////////////////////////////////////////////////////////
// UploadTransfer close connection

void UploadTransfer::Close(BOOL message)
{
	if(m_state == upsNull ) return;
	m_state = upsNull;

//	Transfer::Close();
//	g_UploadQueues.Dequeue( this );
	CloseFile();

	if(message )
	{
		// send message about upload being closed
	}
//	if(m_uploaded == 0)
	{
		Remove(FALSE);
	}
}
*/
//////////////////////////////////////////////////////////////////////
// UploadTransfer read and write handlers

BOOL UploadTransfer::OnRead()
{
//	if(!Transfer::OnRead())
//	{
//		return FALSE;
//	}
	return TRUE;
}

BOOL UploadTransfer::OnWrite()
{
//	if(!Transfer::OnWrite(NULL)) return FALSE;
	return TRUE;
}


//////////////////////////////////////////////////////////////////////
// UploadTransfer hash utilities

void UploadTransfer::ClearHashes()
{
//	m_bSHA1 = FALSE;
}

//////////////////////////////////////////////////////////////////////
// UploadTransfer request utilities

void UploadTransfer::ClearRequest()
{
	m_filename.Empty();
	m_filepath.Empty();
	m_filetags.Empty();

	m_filebase		= 0;
	m_filesize		= 0;
	m_filepartial	= FALSE;

	m_offset		= 0;
	m_length		= SIZE_UNKNOWN;
	m_position		= 0;
	m_requestCount++;

	ClearHashes();
}

BOOL UploadTransfer::RequestPartial(DownloadBase *file)
{
	OP_ASSERT(file != NULL);

	m_filename.Set(file->m_remotename);
	m_filepath.Set(file->m_localname);
	m_filebase	= 0;
	m_filesize	= file->m_size;
	m_filepartial = TRUE;
	m_filetags.Empty();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// UploadTransfer file utilities

void UploadTransfer::StartSending(int state)
{
	m_state	= state;
	m_position	= 0;
//	m_content	= m_output.tLast = op_time(NULL);

//	Transfer::OnWrite(NULL);
}

void UploadTransfer::CloseFile()
{
	if(m_diskfile)
	{
		m_diskfile->Release(FALSE);
		m_diskfile = NULL;
	}
}


//////////////////////////////////////////////////////////////////////
// UploadTransferBT construction

UploadTransferBT::UploadTransferBT(DownloadBase * download, OpString& host, OpString& address, SHAStruct *clientSHA)
:	UploadTransfer(PROTOCOL_BT, download, host, address),

//	m_client(client),
	m_interested(FALSE),
	m_choked(TRUE),
//	m_choked_by_me(TRUE),
	m_randomunchoke(0),
	m_randomunchoketime(0),
	m_is_optimistic_unchoke(FALSE),
    m_bSHA1(clientSHA != 0),
	m_requested(NULL),
	m_served(NULL)
{
//	OP_ASSERT(client != NULL );
	OP_ASSERT(download != NULL );

//	m_host.Set(client->GetTransfer()->m_host);
//	m_address.Set(client->GetTransfer()->m_address);
//	m_useragent.Set("BitTorrent");

	m_state = upsReady;

//	client->AddRef();

	if(m_bSHA1)
	{
		m_pSHA1 = *clientSHA;
	}
	RequestPartial( m_download );
//	((BTDownload *)m_download)->AddUpload( this );
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "UploadTransferBT::~UploadTransferBT()"
#endif

UploadTransferBT::~UploadTransferBT()
{
//	OP_ASSERT( m_client == NULL );
//	OP_ASSERT( m_download == NULL );
	OP_ASSERT( m_requested == NULL );
	OP_ASSERT( m_served == NULL );

	if(m_diskfile != NULL)
	{
		CloseFile();
	}
	if(m_requested)
	{
		m_requested->DeleteChain();
		m_requested = NULL;
	}
	if(m_served)
	{
		m_served->DeleteChain();
		m_served = NULL;
	}

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT choking

BTPacket *UploadTransferBT::SetChoke(BOOL choke)
{
	if(m_choked == choke)
	{
		return NULL;
	}
	m_choked = choke;

	m_requested->DeleteChain();
	m_requested = NULL;
	m_served->DeleteChain();
	m_served = NULL;

	if(choke)
	{
//		m_choked_by_me = TRUE;
		m_state = upsReady;
	}
	else
	{
//		m_choked_by_me = FALSE;
	}
	return BTPacket::New(choke ? BT_PACKET_CHOKE : BT_PACKET_UNCHOKE);
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT close

void UploadTransferBT::Close()
{
//	if(m_client != NULL )
//	{
//		m_client->Release();
//		m_client->m_upload = NULL;
//		m_client->Close();
//		m_client = NULL;
//	}

	if(m_diskfile != NULL)
	{
		CloseFile();
	}
	if(m_requested)
	{
		m_requested->DeleteChain();
		m_requested = NULL;
	}
	if(m_served)
	{
		m_served->DeleteChain();
		m_served = NULL;
	}

//	UploadTransfer::Close(message);
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT connection event

BOOL UploadTransferBT::OnConnected()
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT interest flag

BOOL UploadTransferBT::OnInterested(BTPacket* packet)
{
	if(m_interested)
	{
		return TRUE;
	}
	m_interested = TRUE;

	DEBUGTRACE_CHOKE(UNI_L("INTERESTED: %s, "), (uni_char *)m_address);
	DEBUGTRACE_CHOKE(UNI_L("this: 0x%08x\n"), this);
	return TRUE;
}

BOOL UploadTransferBT::OnUninterested(BTPacket* packet)
{
	if(!m_interested)
	{
		return TRUE;
	}
	m_interested = FALSE;
	m_state = upsReady;

	DEBUGTRACE_CHOKE(UNI_L("NOTINTERESTED: %s, "), (uni_char *)m_address);
	DEBUGTRACE_CHOKE(UNI_L("this: 0x%08x\n"), this);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT request management

OpByteBuffer *UploadTransferBT::OnRequest(BTPacket* packet)
{
	if(packet->GetRemaining() < 4 * 3 )
	{
		return NULL;
	}
	if(m_choked)
	{
		DEBUGTRACE_UP(UNI_L("UL REQUEST from CHOKED connection: %s"), (uni_char *)m_address);
		DEBUGTRACE_UP(UNI_L(", this: 0x%08x\n"), this);
		return NULL;
	}
	UINT64 index	= packet->ReadLongBE();
	UINT64 offset	= packet->ReadLongBE();
	UINT64 length	= packet->ReadLongBE();

	offset += index * ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();

	DEBUGTRACE_UP(UNI_L("UL REQUEST: %s, "), (uni_char *)m_address);
	DEBUGTRACE_UP(UNI_L("offset: %lld, "), offset);
	DEBUGTRACE_UP(UNI_L("length: %d\n"), length);

	// hard coded max packet size. If a client requests more than this, the specs recommend
	// an immediate disconnect
	if(length > 131072)
	{
		DEBUGTRACE_UP(UNI_L("ILLEGAL length: %d\n"), length);
		// error, too large request
		Close();
		return FALSE;
	}
	if(offset + length > m_filesize )
	{
		DEBUGTRACE_UP(UNI_L("ILLEGAL size: %d\n"), offset + length);
		// error, size larger than file
		Close();
		return FALSE;
	}

	for(P2PFilePart* fragment = m_requested ; fragment != NULL; fragment = fragment->m_next )
	{
		// find a fragment that matches the offset and size. If we do, no need to
		// create a new fragment
		if(fragment->m_offset == offset && fragment->m_length == length)
		{
			DEBUGTRACE_UP(UNI_L("MATCHING fragment: %d\n"), offset);
			return NULL;
		}
	}

	P2PFilePart* pNew = P2PFilePart::New(NULL, m_requested, offset, length);

	if (pNew)
	{
		if(m_requested != NULL)
		{
			m_requested->m_previous = pNew;
		}
		m_requested = pNew;
	}

	if(m_state == upsReady )
	{
		m_state = upsRequest;
	}
	OpByteBuffer input;

	packet->ToBuffer(&input);

	return ServeRequests(&input);
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT serving
//
// input - The buffer with the requests received
//
// returns a buffer with the data to be sent
//
OpByteBuffer *UploadTransferBT::ServeRequests(OpByteBuffer *input)
{
	OP_ASSERT( m_state == upsRequest || m_state == upsUploading );
	OP_ASSERT( m_length == SIZE_UNKNOWN );

	if(m_choked)
	{
		return NULL;
	}
	if(input->DataSize() > BT_REQUEST_SIZE / 3 )	// hard coded request size
	{
		DEBUGTRACE8_UP(UNI_L("ILLEGAL request size: %d\n"), input->DataSize());
		return NULL;
	}
	while(m_requested != NULL && m_length == SIZE_UNKNOWN)
	{
		P2PFilePart* fragment = m_requested;
		m_requested = fragment->m_next;

		if(m_requested != NULL)
		{
			m_requested->m_previous = NULL;
		}
		P2PFilePart* old;
		for(old = m_served ; old != NULL ; old = old->m_next )
		{
			if(old->m_offset == fragment->m_offset && old->m_length == fragment->m_length)
				break;
		}

		if(old == NULL && fragment->m_offset < m_filesize &&
			 fragment->m_offset + fragment->m_length <= m_filesize )
		{
			m_offset	= fragment->m_offset;
			m_length	= fragment->m_length;
			m_position	= 0;
		}
		fragment->DeleteThis();
	}

	if(m_length < SIZE_UNKNOWN)
	{
		if(!OpenFile())
		{
			DEBUGTRACE8_UP(UNI_L("OpenFile() failed %s\n"), "");
			return NULL;
		}
		OpByteBuffer* buffer = OP_NEW(OpByteBuffer, ());

		if(buffer == NULL)
		{
			return NULL;
		}

		if (OpStatus::IsError( buffer->SetDataSize((int)m_length + 13) ))
		{
			OP_DELETE(buffer);
			return NULL;
		}		

		// header first...
		UINT32 tmplen;
		UINT64 readlen;
		BT_PIECE_HEADER header[1];

		// read straight into our buffer
		if(!m_diskfile->Read(m_offset + m_position, &buffer->Buffer()[13], m_length, &readlen))
		{
			OP_DELETE(buffer);
			return NULL;
		}
		OP_ASSERT(m_length == readlen);

		tmplen = header->length	= 9 + (UINT32)readlen;
		header->type	= BT_PACKET_PIECE;
		header->piece	= (UINT32)(m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize());
		header->offset	= (UINT32)(m_offset % ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize());
#ifdef OPERA_BIG_ENDIAN
		header->length	= header->length;
		header->piece	= header->piece;
		header->offset	= header->offset;
#else
		header->length	= SWAP_LONG(header->length);
		header->piece	= SWAP_LONG(header->piece);
		header->offset	= SWAP_LONG(header->offset);
#endif

		DEBUGTRACE8_UP(UNI_L("*** SENDING PIECE offset: %lld"), (UINT64)m_offset);
		DEBUGTRACE8_UP(UNI_L(", len: %d "), tmplen);
		DEBUGTRACE8_UP(UNI_L(" (%d)"), header->length);
		DEBUGTRACE_UP(UNI_L(" to %s, upl this: "), (uni_char *)m_address);
		DEBUGTRACE8_UP(UNI_L("%d, total: "), m_uploaded + m_length);
		DEBUGTRACE8_UP(UNI_L("%lld\n"), (UINT64)((BTDownload *)m_download)->m_torrentUploaded + m_length);

		unsigned char *tmpbuf = buffer->Buffer();
		*((UINT32 *)&tmpbuf[0]) = (UINT32)header->length;
		*((byte *)&tmpbuf[4]) = (byte)header->type;
		*((UINT32 *)&tmpbuf[5]) = (UINT32)header->piece;
		*((UINT32 *)&tmpbuf[9]) = (UINT32)header->offset;

		m_position += readlen;
		m_uploaded += readlen;
		((BTDownload *)m_download)->m_torrentUploaded += readlen;

		m_served = P2PFilePart::New( NULL, m_served, m_offset, readlen );

		m_state		= upsUploading;
		m_length	= SIZE_UNKNOWN;

		return buffer;
	}
	else
	{
		DEBUGTRACE8_UP(UNI_L("ILLEGAL length: %d\n"), m_length);
		m_state = upsRequest;
	}

	return NULL;
}

BOOL UploadTransferBT::OnCancel(BTPacket* packet)
{
	if(packet->GetRemaining() < 4 * 3)
	{
		return TRUE;
	}
	UINT64 index	= packet->ReadLongBE();
	UINT64 offset	= packet->ReadLongBE();
	UINT64 length	= packet->ReadLongBE();

	offset += index * ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();

	DEBUGTRACE_UP(UNI_L("UL CANCEL: %s, "), (uni_char *)m_address);
	DEBUGTRACE8_UP(UNI_L("offset: %lld, "), offset);
	DEBUGTRACE8_UP(UNI_L("length: %d\n"), length);

	P2PFilePart::Subtract( &m_requested, offset, length );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// UploadTransferBT file access

BOOL UploadTransferBT::OpenFile()
{
	OP_ASSERT( m_state == upsRequest || m_state == upsUploading );

	if(m_diskfile != NULL)
	{
		return TRUE;
	}
	OpString target_directory;

	m_download->m_torrent.GetImpl()->GetTargetDirectory(target_directory);

	m_diskfile = g_P2PFiles->Open(m_filepath, FALSE, FALSE, target_directory, m_download->GetMetaData(), &m_download->m_files, m_download->m_torrent.GetImpl()->GetBlockSize());

	if(m_diskfile != NULL)
	{
		return TRUE;
	}
//	Close();
	return FALSE;
}


//////////////////////////////////////////////////////////////////////
// Uploads construction

P2PUploads::P2PUploads()
:	m_count(0)
{

	BT_RESOURCE_ADD("P2PUploads", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PUploads::~P2PUploads()"
#endif

P2PUploads::~P2PUploads()
{
	Clear();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// Uploads clear

void P2PUploads::Clear()
{
//	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
//	{
//		m_list.Get(pos)->Close();
//	}
	m_list.Clear();
	OP_ASSERT( GetCount( NULL, -1 ) == 0 );
}

//////////////////////////////////////////////////////////////////////
// Uploads counting

UINT32 P2PUploads::GetCount(UploadTransfer* except, INT32 state)
{
	if(except == NULL && state == -1)
	{
		return m_list.GetCount();
	}
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		UploadTransfer* upload = m_list.Get(pos);

		if(upload != except)
		{
			switch(state)
			{
				case -1:
					count++;
					break;
				case -2:
					if(upload->m_state > upsNull)
					{
						count++;
					}
					break;

				default:
					if(upload->m_state == state)
					{
						count++;
					}
					break;
			}
		}
	}
	return count;
}

//////////////////////////////////////////////////////////////////////
// Uploads per-host limiting

BOOL P2PUploads::AllowMoreTo(OpString& address)
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		UploadTransfer* upload = m_list.Get(pos);

		if(upload->m_state == upsUploading || upload->m_state == upsQueued )
		{
			if(upload->m_host.Compare(address) == 0)
				count++;
		}
	}
	return(count <= MAXPERHOST);	// FIX: Hard coded
}

//////////////////////////////////////////////////////////////////////
// Uploads add and remove

void P2PUploads::Add(UploadTransfer* upload)
{
	INT32 pos = m_list.Find(upload);
	if(pos != -1)
	{
		m_list.RemoveByItem(upload);
	}
	m_list.Add(upload);
}

void P2PUploads::Remove(UploadTransfer* upload)
{
	INT32 pos = m_list.Find(upload);
//	OP_ASSERT(pos != -1);
	if(pos != -1)
	{
		m_list.RemoveByItem(upload);
	}
}

UploadTransfer *P2PUploads::Get(UINT32 idx)
{
	if(m_list.GetCount() < idx)
	{
		return NULL;
	}
	return m_list.Get(idx);
}

INT32 P2PUploads::GetTransferCount(int state)
{
	UploadTransfer *upload;
	INT32 count = 0;
	UINT32 idx;

//	upsNull, upsReady, upsConnecting,
//	upsRequest, upsQueued,
//	upsUploading, upsResponse,
//	upsBrowse, upsMetadata, upsPreQueue

	for(idx = 0; idx < m_list.GetCount(); idx++)
	{
		upload = m_list.Get(idx);
		if(upload->m_live)
		{
			if(state == upsCountAll)
			{
				count++;
			}
			else if(state == upsRequestAndUpload)
			{
				if(upload->m_state == upsRequest || upload->m_state == upsUploading)
				{
					count++;
				}
			}
			else if(state == upsUnchoked)
			{
				if(!((UploadTransferBT *)upload)->IsChoked())
				{
					count++;
				}
			}
			else if(state == upsInterested)
			{
				if(((UploadTransferBT *)upload)->IsInterested())
				{
					count++;
				}
			}
			else
			{
				if(upload->m_state == state)
				{
					count++;
				}
			}
		}
	}
	return count;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
