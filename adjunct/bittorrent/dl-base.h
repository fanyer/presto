// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef DOWNLOADBASE_H
#define DOWNLOADBASE_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#ifndef SIZE_UNKNOWN
#if defined (UNIX) && !defined(__FreeBSD__)
#define SIZE_UNKNOWN UINT64_MAX
#elif defined (_WINDOWS_)
#define SIZE_UNKNOWN	0xFFFFFFFFFFFFFFFF
#else
#define SIZE_UNKNOWN	0xFFFFFFFFFFFFFFFFLL
#endif
#endif
#ifndef UINT32_MAX
#define UINT32_MAX	0xFFFFFFFF
#endif

class DownloadTransfer;
class DownloadTransferBT;
class P2PFilePart;
class P2PBlockFile;
class DownloadSource;
class Transfer;
class BTClientConnector;

#include "bt-info.h"
#include "bt-upload.h"
//#include "bt-packet.h"
//#include "p2p-fileutils.h"
//#include "connection.h"

class DownloadSourceCollection : public Head
{
public:
	DownloadSource*	Last() const { return (DownloadSource*)Head::Last(); }
	DownloadSource*	First() const { return (DownloadSource*)Head::First(); }
};

//********************************************************************
//
//	DownloadBase
//
//********************************************************************

class DownloadBase  
:	public OpTransferManagerListener, public MessageObject
{
// Construction
public:
	DownloadBase();
	virtual ~DownloadBase();
	
	void		Clear();

// Attributes
public:
	INT32		m_runcookie;
	INT32		m_cookie;
	INT32		m_savecookie;
	TRISTATE	m_verify;

public:
	OpString	m_remotename;
	OpString	m_localname;
	UINT64		m_size;			// total size

	UINT64		m_uploaded;
	UINT64		m_downloaded;
	BOOL		m_seeding;
	BOOL		m_paused;
	BOOL		m_boosted;
	BOOL		m_complete;
	time_t		m_saved;
	time_t		m_began;	//The time when this download began trying to download (Started
							//searching, etc) 0 means has not tried this session.
	time_t		m_completedTime;
	OpVector<OpString> m_failedsources;
	OpVector<P2PVirtualFile> m_files;

public:
	SHAStruct	m_peerid;
	BOOL		m_bBTH;
	SHAStruct	m_pBTH;
	BTInfo		m_torrent;

protected:
//	DownloadSource*	m_sourcefirst;
//	DownloadSource*	m_sourcelast;
	DownloadSourceCollection m_sources;
	INT32			m_sourcecount;
	P2PBlockFile*	m_file;
	BOOL			m_need_more_sources;
	BOOL			m_is_transfer_manager_listener;
	P2PUploads		m_Uploads;
	time_t			m_lastProgressUpdate;	// last time progress bar was updated
	time_t				m_received;
	DownloadTransfer*	m_transferfirst;
	DownloadTransfer*	m_transferlast;
	int					m_transfercount;
	time_t				m_transferstart;
	INT32				m_transfer_id;
	OpTransferItem*		m_transfer_item;
	UINT64				m_last_uploaded;
	UINT64				m_last_downloaded;
	BOOL				m_delete_files_when_removed;
	P2PMetaData			m_metadata;
	OpString			m_directory;
	OpString			m_torrent_file;

// Operations
public:
	virtual void	Pause() = 0;
	virtual void	Resume() = 0;
	virtual void	Remove(BOOL bDelete = FALSE) = 0;
	virtual BOOL	IsPaused() = 0;
	virtual BOOL	IsMoving() = 0;
	virtual BOOL	IsCompleted() = 0;
	virtual BOOL	IsStarted() = 0;
	virtual BOOL	IsDownloading() = 0;
	virtual BOOL	IsBoosted() = 0;
	virtual BOOL	IsTrying();
	virtual void	OnRun() = 0;
	inline BOOL		IsSeeding() { return m_seeding; }
	INT32 TransferId() const { return m_transfer_id; }
	void SetTransferId(INT32 transfer_id) { m_transfer_id = transfer_id; }
	virtual void StopTrying() {}

	BOOL			IsRangeUseful(UINT64 offset, UINT64 length);
	virtual UINT64	GetVolumeRemaining();
	virtual UINT64	GetVolumeUploaded();
	UINT64			GetVolumeComplete();
	void			GetDisplayName(OpString& name);
	P2PFilePart*	GetFirstEmptyFragment();
	P2PFilePart*	GetPossibleFragments(P2PFilePart* available, UINT64* pnLargestOffset = NULL, UINT64* pnLargestLength = NULL);
	BOOL			GetFragment(DownloadTransfer* transfer);
	BOOL			IsPositionEmpty(UINT64 offset);
	virtual BOOL	PrepareFile();
	BOOL			SubmitData(UINT64 offset, byte *pData, UINT64 length, UINT32 blocknumber = 0);
	BOOL			MakeComplete();
	BOOL			OpenFile(OpVector<P2PVirtualFile> *files = NULL);
	void			CloseFile();
	void			RemoveSource(DownloadSource* source, BOOL ban);
	INT32			GetSourceCount(BOOL sane = FALSE);
	DownloadTransferBT*	CreateTorrentTransfer(BTClientConnector* pClient);
	BOOL			AddSourceInternal(DownloadSource* pSource);
	time_t			GetStartTimer();
	BOOL			RunFile(time_t tNow);
	void			SetStartTimer();
	virtual BOOL	FindMoreSources() { return TRUE; };
	void			CloseTransfers();
	virtual INT32	GetTransferCount(int state = -1, OpString* pAddress = NULL);
	BOOL			CheckSource(DownloadSource* check);
	BOOL			StartNewTransfer(time_t tNow);
	virtual void	OnDownloaded();
	void			GetFilePath(OpString& path);
	P2PUploads *Uploads() { return &m_Uploads; }
	void			CloseMetaFile() { m_metadata.WriteMetaFile(TRUE); }
	P2PMetaData*	GetMetaData() { return &m_metadata; }

public:
	void			SetModified() { m_cookie++; }

	void FileDownloadInitalizing();
	void FileDownloadBegin(BOOL& transfer_failed);
	void FileProgress(BOOL& transfer_failed);
	void FileDownloadCompleted();
	void FileDownloadFailed();
	void FileDownloadCheckingFiles(UINT64 current, UINT64 total);
	void FileDownloadSharing(BOOL& transfer_failed);
	void FileDownloadStopped();

	// OpTransferManagerListener.
	BOOL OnTransferItemAdded(OpTransferItem* transfer_item, BOOL is_populating = FALSE, double last_speed = 0);
	void OnTransferItemRemoved(OpTransferItem* transfer_item);
	void OnTransferItemStopped(OpTransferItem* transferItem);
	void OnTransferItemResumed(OpTransferItem* transferItem);
	BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem);
	void OnTransferItemDeleteFiles(OpTransferItem* transferItem);

	// Methods.
	OP_STATUS AddTransferItem(UINT64& content_size);
	OpTransferItem* GetTransferItem();

	INT32 NewTransferId() { return m_next_transfer_id++; }
	INT32 GetTransferId();
	void StartListening();

	static INT32 m_next_transfer_id;

protected:
	void			GenerateLocalName();
	void			AddTransfer(DownloadTransfer* transfer);
	void			RemoveTransfer(DownloadTransfer* transfer);

	/// override function from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

public:
	inline DownloadTransfer* GetFirstTransfer() const
	{
		return m_transferfirst;
	}
//	inline DownloadSource* GetFirstSource() const
//	{
//		return m_sourcefirst;
//	}
	
	friend class DownloadTransfer;
	friend class UploadTransfer;
};

class DownloadSource : public Link
{
// Construction
public:
	DownloadSource(DownloadBase* download);
	DownloadSource(DownloadBase* download, SHAStruct* GUID, OpString& address, WORD port);
	virtual ~DownloadSource();
private:
	inline void Construct(DownloadBase *download);

// Attributes
public:
	DownloadBase*		m_download;
	DownloadTransfer*	m_transfer;

	PROTOCOLID			m_protocol;			// usually PROTOCOL_BT
	BOOL				m_bGUID;			// is the GUID used?
	GGUID				m_GUID;				// peer id of the peer
	OpString			m_address;			// outgoing address
	WORD				m_port;				// outgoing port
	OpString			m_useragent;		// user-agent of the peer
	UINT32				m_speed;			// measured speed (not really used)
	BOOL				m_readcontent;		// we can read content from this peer
	time_t				m_lastseen;			// last time this peer was connected
	time_t				m_attempt;			// last attempt
	INT32				m_failures;			// how many failures
	P2PFilePart*		m_available;		// data available from this source
	P2PFilePart*		m_pastfragment;
	BOOL				m_pex_peer;		// TRUE if this host was added using peer exchange
	OpString			m_pex_added_by;			// address of the peer that sent this host
	time_t				m_added_time;		// time this source was added

// Operations
public:
	BOOL		CanInitiate(BOOL established);
	void		Remove(BOOL closetransfer, BOOL ban);
	void		OnFailure(BOOL nondestructive);
	void		OnResume() { m_attempt = 0; }
	void		SetValid();
	void		SetLastSeen();
	BOOL		CheckHash(const SHAStruct* pSHA1);
	void		AddFragment(UINT64 offset, UINT64 length, BOOL merge = FALSE);
	BOOL		HasUsefulRanges();
	BOOL		TouchedRange(UINT64 offset, UINT64 length);
	
	DownloadTransfer*	CreateTransfer();

	DownloadSource*	Suc() const { return (DownloadSource*)Link::Suc(); }
	DownloadSource*	Pred() const { return (DownloadSource*)Link::Pred(); }

// Inlines
public:
	inline BOOL Equals(DownloadSource* source) const
	{
		if ( m_bGUID && source->m_bGUID ) return m_GUID == source->m_GUID;
		
//		if ( m_serverport != source->m_serverport )
//		{
//			return FALSE;
//		}
//		else if ( m_serverport > 0 )
//		{
//			if ( m_serveraddress.Compare(source->m_serveraddress) != 0) return FALSE;
//			if ( m_address.Compare(source->m_address) != 0) return FALSE;
//		}
		else
		{
			if(m_address.Compare(source->m_address) != 0) return FALSE;
			if ( m_port != source->m_port ) return FALSE;
		}
		
		return TRUE;
	}
};

class DownloadTransfer
{
// Construction
public:
	DownloadTransfer(DownloadSource* pSource, PROTOCOLID protocol, OpP2PObserver *observer);
	virtual ~DownloadTransfer();

// Attributes
public:
	PROTOCOLID			m_protocol;
	DownloadBase*		m_download;
	DownloadTransfer*	m_dlPrev;
	DownloadTransfer*	m_dlNext;
	DownloadSource*		m_source;

public:
	int			m_state;
	int			m_queuepos;
	int			m_queuelen;
	OpString	m_queuename;
	OpString	m_host;
	OpString	m_address;	
	
public:
	UINT64		m_offset;
	UINT64		m_length;
	UINT64		m_position;
	UINT64		m_downloaded;
	BOOL		m_has_complete_file;

public:
	BOOL		m_wantbackwards;
	BOOL		m_recvbackwards;
	P2PTransferSpeed	m_transferSpeed;

// Operations
public:
	virtual BOOL	Initiate() = 0;
	virtual void	Close(TRISTATE keepsource);
	virtual OP_STATUS SubtractRequested(P2PFilePart** ppFragments) = 0;
	virtual OP_STATUS UnrequestRange(UINT64 offset, UINT64 length) { return OpStatus::OK; }
	void	SetState(int state) { m_state = state; }
	void	ChunkifyRequest(UINT64* offset, UINT64* length, UINT64 chunk);
};

enum
{
	dtsNull, dtsConnecting, dtsRequesting, dtsHeaders, dtsDownloading,
	dtsFlushing, dtsNoUsed, dtsHashset, dtsMetadata, dtsBusy, dtsEnqueue, dtsQueued,
	dtsTorrent, dtsCountAllWithCompleteFile,

	dtsCountAll = -1,
	dtsCountNotQueued = -2,
	dtsCountNotConnecting = -3,
	dtsCountTorrentAndActive = -4,
	dtsCountInterestingPeers = -5
};


class Downloads
{
// Construction
public:
	Downloads();
	virtual ~Downloads();
	
	// Attributes
public:

protected:
	OpAutoVector<DownloadBase> m_list;
	int				m_runcookie;
	BOOL			m_can_checkfile;
	
// Operations
public:
	BOOL		CanStartCheckingFile() { return m_can_checkfile; }
	void		StartCheckingFile() { m_can_checkfile = FALSE; }
	void		StopCheckingFile() { m_can_checkfile = TRUE; }
	DownloadBase* Add(DownloadBase *download, SHAStruct *bth = NULL);

public:
	INT32		GetSeedCount();
	INT32		GetActiveTorrentCount();
	INT32		GetCount(BOOL bActiveOnly = FALSE);
	INT32		GetTransferCount();
	INT32		GetTryingCount(BOOL bTorrentsOnly = FALSE);
	INT32		GetConnectingTransferCount();
	BOOL		Check(DownloadSource* source);
	BOOL		CheckActive(DownloadBase *download, INT32 scope);
	DownloadBase* FindByBTH(SHAStruct *hash);
	void		Remove(DownloadBase *download);
	void		StartListening();

public:
	void		OnRun();
	void		SetPerHostLimit(OpString& address, INT32 limit);
	void		UpdateAllows(BOOL bNew);

protected:
	BOOL		AllowMoreDownloads();
	BOOL		AllowMoreTransfers(OpString *address = NULL);

// Inlines
public:
	inline BOOL Check(DownloadBase *download) const
	{
		return m_list.Find(download ) != -1;
	}
	friend class DownloadBase;
	friend class DownloadSource;
};

#endif // DOWNLOADBASE_H
