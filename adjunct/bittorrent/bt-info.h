// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_INFO_H
#define BT_INFO_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

class OpByteBuffer;
class BENode;
class DownloadBase;
class BTInfo;
class DownloadItem;
class BTDownloadInfo;

#include "modules/hardcore/base/op_types.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/opstring.h"
#include "modules/util/excepts.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "bt-util.h"
#include "bt-tracker.h"
#include "bt-benode.h"
#include "bt-globals.h"
#include "connection.h"
#include "p2p-fileutils.h"
#include "bt-packet.h"

class BTInfoImpl
{
// Construction
public:
	BTInfoImpl();
	~BTInfoImpl();

// Subclass
public:
	class BTFile
	{
	public:
		BTFile();
		~BTFile();
		void	Copy(BTFile* pSource);
	public:
		OpString	m_sPath;
		UINT64		m_nSize;
		UINT64		m_nOffset;
		BOOL		m_bSHA1;
		SHAStruct	m_pSHA1;
	};

	class BTInfoListener
	{
	public:
		virtual ~BTInfoListener() {};

		virtual void OnFinished() = 0;
	};

// Operations
public:
	void		Clear();
	void		Copy(BTInfoImpl* pSource);
public:
	BOOL		LoadTorrentFile(OpString& file);
	void		LoadTorrentFile(OpTransferItem * transfer_item);
	BOOL		LoadTorrentBuffer(OpByteBuffer* pBuffer);
	BOOL		LoadTorrentTree(BENode* pRoot);
	BOOL		SaveTorrentFile(OpString& targetdirectory);
	BOOL		ProcessTrackerInfo(BENode *root);
	void		SetTargetDirectory(OpString& dir) { m_targetdirectory.Set(dir); }
	void		GetTargetDirectory(OpString& dir) { dir.Set(m_targetdirectory); }
	void		AttachToDownload(DownloadBase *download) { m_attachtodownload = download; }
	static		void Escape(SHAStruct *pSHA1, OpString8& strEscaped);
	void		GetInfoHash(SHAStruct *pSHA1);
	BTMultiTracker* GetTracker() { return m_multi_tracker; }
	void		InitiateDownload(BTDownloadInfo * btdinfo);

	// Access properties
	BOOL IsAvailable() const { return m_bValid; }
	BOOL GetName(OpString& name) { name.Set(m_sName); return IsAvailable(); }
	const uni_char* GetName() { return m_sName.CStr(); }
	UINT32 GetBlockSize()	{ return m_nBlockSize; }
	UINT64 GetTotalSize()	{ return m_nTotalSize; }
	UINT32 GetBlockCount()	{ return m_nBlockCount; }
	SHAStruct GetInfoSHA()	{ return m_pInfoSHA1; }
	INT32	GetFileCount()	{ return m_nFiles;	}
	BTFile* GetFiles()		{ return m_pFiles; }
	BOOL	IsPrivate()		{ return m_private_torrent;	}	// if set, don't do any PEX or DHT
	const uni_char* GetSaveFilename() { return m_savefilename.CStr(); }

	void SetListener(BTInfoListener *listener) { m_listener = listener; }

public:
	void		BeginBlockTest();
	void		AddToTest(void *pInput, UINT32 nLength);
	BOOL		FinishBlockTest(UINT32 nBlock);
protected:
	BOOL		CheckFiles();
	OP_STATUS	AddTorrentToDownload();

	// Attributes
private:
	BOOL			m_bValid;
	SHAStruct		m_pInfoSHA1;
	BOOL			m_bDataSHA1;
	SHAStruct		m_pDataSHA1;
	UINT64			m_nTotalSize;
	UINT32			m_nBlockSize;
	UINT32			m_nBlockCount;
	SHAStruct*		m_pBlockSHA1;
	OpString		m_sName;
	INT32			m_nFiles;
	BTFile*			m_pFiles;
	OpString		m_sLoadedFromPath;		// path/url this torrent was loaded from
	OpString		m_savefilename;			// where the torrent was saved to
	BOOL			m_private_torrent;		// if set, don't do any PEX or DHT
	BTSHA			m_pTestSHA1;
	UINT32			m_nTestByte;
	OpByteBuffer	m_pSource;
	OpString		m_targetdirectory;		// where we should save the files downloaded
	DownloadBase*	m_attachtodownload;		// if set, attach the torrent to this existing DownloadBase instance
	BTMultiTracker*	m_multi_tracker;		// all the trackers defined for this torrent
	BTInfoListener*	m_listener;
};

class BTInfo :
	public BTInfoImpl::BTInfoListener
{
public:
	BTInfo() : m_info_impl(NULL), m_download_item(NULL)
	{
		m_info_impl = OP_NEW(BTInfoImpl, ());
		if(m_info_impl)
		{
			m_info_impl->SetListener(this);
		}
		BT_RESOURCE_ADD("BTInfo", this);
	};
	virtual ~BTInfo()
	{
		OP_DELETE(m_info_impl);

		BT_RESOURCE_REMOVE(this);
	};
	BTInfoImpl* GetImpl() { OP_ASSERT(m_info_impl); return m_info_impl; }

	// Deprecated
	OP_STATUS	LoadTorrentFileFromURL(OpString& file, BOOL force_dialog = FALSE) { return OpStatus::ERR_NOT_SUPPORTED; } // To be removed when dochand is patched.

	BOOL		LoadTorrentFile(OpString& filename);
	void		LoadTorrentFile(OpTransferItem* transfer_item);
	void		SetDownloadItem(DownloadItem *download_item) { m_download_item = download_item; }

	/* We have a url with content downloaded, this call is to be used by core, not ui code */
	void		OnTorrentAvailable(URL& torrent_url);

	/* callbacks from gui */
	void		Cancel(BTDownloadInfo * btdinfo);
	void		InitiateDownload(BTDownloadInfo * btdinfo);

	// BTInfoImpl::BTInfoListener
	void OnFinished()
	{
		OP_DELETE(this);
	}

private:

	BTInfoImpl		*m_info_impl;		// the main implementation of the code
	DownloadItem	*m_download_item;
};

#endif // BT_INFO_H
