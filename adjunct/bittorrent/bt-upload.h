// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_UPLOAD_H
#define BT_UPLOAD_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

class UploadTransfer;
class P2PFilePart;
class P2PFile;
class UploadQueue;
class BTClientConnector;
class DownloadBase;

class UploadFile
{
// Construction
public:
	UploadFile(UploadTransfer* upload, SHAStruct *pSHA1, OpString& name, OpString& path, UINT64 size);
	virtual ~UploadFile();
	
// Attributes
public:
	OpString		m_address;
	BOOL			m_bSHA1;
	SHAStruct		m_SHA1;
	OpString		m_name;
	OpString		m_path;
	UINT64			m_size;
public:
	UINT32			m_requests;
	P2PFilePart*	m_fragments;
public:
	BOOL			m_selected;
protected:
	OpVector<UploadTransfer> m_transfers;

// Operations
public:
//	void				Add(UploadTransfer* upload);
//	BOOL				Remove(UploadTransfer* upload);
//	UploadTransfer*		GetActive() const;
public:
	void				AddFragment(UINT64 offset, UINT64 length);
//	void				Remove();

// Inlines
public:
	inline BOOL IsEmpty() const
	{
		return m_transfers.GetCount() == 0;
	}
};

#define ULA_SLOTS	16

class UploadTransfer// : public Transfer
{
// Construction
public:
	UploadTransfer(PROTOCOLID nProtocol, DownloadBase* download, OpString& host, OpString& address);
	virtual ~UploadTransfer();

// Attributes
public:
	PROTOCOLID		m_protocol;	// Protocol ID
	INT32			m_state;	// Common state code
	P2PFile*		m_diskfile;	// Disk file

public:
	OpString		m_filename;		// Name of requested file
	OpString		m_filepath;		// Path of requested file
	UINT64			m_filebase;		// Base offset in requested file
	UINT64			m_filesize;		// Size of requested file
	BOOL			m_filepartial;	// Partial file flag
	OpString		m_filetags;		// File sharing tags
	OpString		m_host;
	OpString		m_address;	

public:
	BOOL			m_live;			// Live connection tag
	UINT32			m_requestCount;	// Request count
	UINT64			m_uploaded;		// Bytes uploaded
	time_t			m_content;		// Send start timestamp
	UINT64			m_offset;		// Fragment offset
	UINT64			m_length;		// Fragment length
	UINT64			m_position;		// Send position
	DownloadBase*	m_download;
	P2PTransferSpeed	m_transferSpeed;

private:
	UINT32			m_ref_count;

protected:

	time_t			m_rotatetime;
	
// Operations
public:
//	virtual void	Remove(BOOL message = TRUE);
//	virtual void	Close(BOOL message = FALSE);

	virtual UINT32 AddRef()
	{
		return ++m_ref_count;
	}
	virtual void Release()
	{
		if(--m_ref_count == 0)
		{
			OP_ASSERT( this != NULL );

			DEBUGTRACE(UNI_L("**** REMOVING uploadtransfer: 0x%08x\n"), this);

			CloseFile();

			OP_DELETE(this);
		}
	}

protected:
	virtual BOOL	OnRead();
	virtual BOOL	OnWrite();

protected:
	void		ClearHashes();
	BOOL		HashesFromURN(char *URN);
	void		ClearRequest();
	BOOL		RequestPartial(DownloadBase* file);
	void		StartSending(int state);
	void		CloseFile();

};

enum UploadState
{
	upsNull, 
	upsReady,
	upsRequest, 
	upsQueued,
	upsUploading, 
	upsUnchoked,
	upsInterested,

	upsCountAll = -1,
	upsRequestAndUpload = -2
};

class UploadTransferBT : public UploadTransfer
{
// Construction
public:
	UploadTransferBT(DownloadBase* download, OpString& host, OpString& address, SHAStruct *clientSHA);
	virtual ~UploadTransferBT();
	
// Attributes
public:
//	BTClientConnector*	m_client;

public:
	BOOL			m_interested;
	BOOL			m_choked;
//	BOOL			m_choked_by_me;
	INT32			m_randomunchoke;
	time_t			m_randomunchoketime;
	BOOL			m_is_optimistic_unchoke;
	BOOL			m_bSHA1;		// Hash of requested file
	SHAStruct		m_pSHA1;		// ..

public:
	P2PFilePart*	m_requested;
	P2PFilePart*	m_served;
	
// Operations
public:
	BTPacket *		SetChoke(BOOL choke);
	void			Close();
	virtual BOOL	OnConnected();
//	BOOL			IsChokedByMe() { return m_choked_by_me; }
	BOOL			IsChoked() { return m_choked; }
	BOOL			IsInterested() { return m_interested; }
	BOOL			IsOptimisticUnchoke() {	return m_is_optimistic_unchoke && !IsChoked(); }
	void			SetOptimisticUnchoke(BOOL optimistic) {	m_is_optimistic_unchoke = optimistic; }

public:
	BOOL	OnInterested(BTPacket* packet);
	BOOL	OnUninterested(BTPacket* packet);
	OpByteBuffer * OnRequest(BTPacket* packet);
	BOOL	OnCancel(BTPacket* packet);

protected:
	BOOL	OpenFile();
	OpByteBuffer *ServeRequests(OpByteBuffer *input);
};

class P2PUploads  
{
// Construction
public:
	P2PUploads();
	virtual ~P2PUploads();
	
// Attributes
public:
	UINT32		m_count;			// Active count

protected:
	OpVector<UploadTransfer>	m_list;

// Operations
public:
	void		Clear();
	UINT32		GetCount(UploadTransfer* except, INT32 state = -1);
	BOOL		AllowMoreTo(OpString& address);
	UploadTransfer *Get(UINT32 idx);
	void		Add(UploadTransfer* upload);
	void		Remove(UploadTransfer* upload);
	INT32		GetTransferCount(int state);

// List Access
public:
	inline BOOL Check(UploadTransfer* upload)
	{
		return m_list.Find(upload) != -1;
	}
	
	inline INT32 GetTransferCount()
	{
		return GetCount( NULL, -2 );
	}
};

#endif // BT_UPLOAD_H
