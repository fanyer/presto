// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_DOWNLOAD_H
#define BT_DOWNLOAD_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#include "bt-info.h"
#include "bt-util.h"
#include "dl-base.h"
#include "bt-client.h"
#include "bt-globals.h"
#include "connection.h"
#include "bt-packet.h"
#include "bt-tracker.h"

class DownloadTransfer;
class DownloadTransferBT;
class UploadTransferBT;
class BTClient;
class BTPacket;
class P2PBlockFile;
class P2PFilePart;
class P2PFile;

#define HASH_NULL		0
#define HASH_TORRENT	1

//  these represent bits
#define	NotSeeding			(1 << 1)		// not currently seeding/checking files
#define	PreSeeding			(1 << 2)		// seeding before any download/upload is initiated
#define	PostSeeding			(1 << 3)		// seeding after download has been completed
#define	PostSeedingDone		(1 << 4)		// post seeding done if bit is set
#define PreSeedingDone		(1 << 5)		// set when the pre-seeding is done

struct BTBlock
{
	UINT32 block;			// block number
	UINT32 block_count;		// number of occurences of this block in the swarm
};

class BTCompactPeer
{
public:
	BTCompactPeer() {}
	BTCompactPeer(OpString& peer, UINT32 port)
	{
		OpStatus::Ignore(m_peer.Set(peer.CStr()));
		m_port = port;
	}

	OpString	m_peer;
	UINT32		m_port;
};

class BTDownload : 
	public DownloadBase,
	public OpTimerListener
{
// Construction
public:
	BTDownload();
	virtual ~BTDownload();

public:
//	BOOL		m_torrentRequested;
//	BOOL		m_torrentStarted;
	time_t		m_torrentTracker;
	time_t		m_torrentPeerExchange;
	UINT64		m_torrentUploaded;
	UINT64		m_torrentDownloaded;
	BOOL		m_torrentEndgame;
	BOOL		m_trackerError;
	OpString	m_torrentTrackerError;
	UINT32		m_blocknumber;
	UINT32		m_blocklength;
	UINT32		m_totalSeeders;		// total number of peers with the complete file ("seeders")
	UINT32		m_totalPeers;		// total number of peers without the complete file ("leechers")
	BTTracker::TrackerState	m_tracker_state;

protected:
	UINT64		m_volume;
	UINT64		m_total;
	byte*		m_torrentBlock;
	UINT32		m_torrentBlockSize;
	UINT32		m_torrentSize;
	UINT32		m_torrentSuccess;
	OpAutoPtr<OpTimer> m_seed_timer;
	UINT32		m_seeding_stage;		// are we seeding or not
	BOOL		m_create_files;
	UINT64		m_create_offset;
	BOOL		m_checking_file;
	BOOL		m_checking_file_queued;
	BOOL		m_first_block;				// set to TRUE on the first block checked
	UINT32		m_checking_delay;			// delay to use for the next checking cycle
	BTBlock		*m_available_block_count;	// array with a count of the available block in the swarm
	OpP2PVector<UploadTransferBT> m_unchokes;
	OpP2PVector<UploadTransferBT> m_chokes;

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

private:
//	OpVector<UploadTransferBT>	m_torrentUploads;
	time_t		m_torrentChoke;
	time_t		m_torrentChokeRefresh;
	time_t		m_torrentSources;
	P2PFile		*m_verifyfile;
	P2PFile		*m_verify_block_file;
	BOOL		m_initial_ramp_up;		// if TRUE, we want to connect to 10+ peers fast to ramp up
										// the download speed in the beginning
	BTClientConnectors m_BTClientConnectors;

public:
	BTClientConnectors * ClientConnectors() { return &m_BTClientConnectors; }
//	OpVector<UploadTransferBT> *GetUploads() { return &m_torrentUploads; }

	OP_STATUS CheckExistingFile();
	void	StartRampUp() { m_initial_ramp_up = TRUE; }

	void	Clear();

	void	Pause();
	void	Resume();
	void	Remove(BOOL bDelete = FALSE);
	BOOL	IsPaused() { return m_paused; }
	BOOL	IsMoving() { return ( m_file == NULL ); }
	BOOL	IsCompleted() { return m_complete; }
	BOOL	IsStarted() { return(GetVolumeComplete() > 0); }
	BOOL	IsDownloading() { return(GetTransferCount() > 0); }
	BOOL	IsBoosted() { return m_boosted; }
	BOOL	CheckSource(DownloadSource* pSource) const;
	void	ClearSources();
	BOOL	AddSourceBT(SHAStruct* pGUID, OpString& pAddress, WORD nPort, BOOL from_pex, OpString& pex_address);
	void			ChokeTorrent(time_t tNow = 0);
	BTPacket*		CreateBitfieldPacket();
	OP_STATUS		SetTorrent(BTInfoImpl* pTorrent);
	BOOL			RunTorrent(time_t tNow);
	void			OnTrackerEvent(BOOL success, OpString& tracker_url, char *reason = NULL);
	void			OnRun();
	BOOL			StartTransfersIfNeeded(time_t tNow);
	void			SubtractHelper(P2PFilePart** ppCorrupted, byte* block, UINT64 nBlock, UINT64 size);
	void			OnDownloaded();
	BOOL			VerifyTorrentFiles(BOOL first);
	virtual UINT64	GetVolumeUploaded() { return m_torrentUploaded; }
	BOOL			VerifyTorrentBlock(UINT32 block);
	OP_STATUS		UpdateAvailableBlockCount(UINT32 block, BOOL increment = TRUE);
	void			GetRarestBlocks(BTBlock *dest_blocks, UINT32 block_count);
	void			SortAvailableBlockCount();
	BOOL			IsUnchokable(UploadTransferBT *transfer, BOOL allow_snubbed, BOOL allow_not_interested = FALSE);
	void			ResetFileCheckState();
	OP_STATUS		StringToPeerlist(OpString& compact_peers, OpVector<BTCompactPeer>& peers);
	OP_STATUS		CompactStringToPeer(unsigned short* compact_peer, OpString& peer, UINT32& port);
	OP_STATUS		PeerToString(BTCompactPeer& peer, OpByteBuffer& compact_peer);

	virtual INT32	GetTransferCount(int state = -1, OpString* pAddress = NULL);
	/**
	* Get any unchokes that should be performed immediately.
	* 
	* max_to_unchoke - maximum number of peers allowed to be unchoked
	* all_peers - list of peers to choose from
	* returns peers to unchoke
	*/
	void			GetImmediateUnchokes(UINT32 max_to_unchoke, OpVector<UploadTransferBT> *unchoke_peers);
	/**
	* Perform peer choke, unchoke and optimistic calculations
	*
	* max_to_unchoke - maximum number of peers allowed to be unchoked
	* all_peers - list of peers to choose from
	* force_refresh - force a refresh of optimistic unchokes
	*/
	void			CalculateUnchokes( UINT32 max_to_unchoke, BOOL force_refresh);
	void			GetChokes(OpVector<UploadTransferBT> *chokes);
	void			GetUnchokes(OpVector<UploadTransferBT> *unchokes);
	static void		UpdateLargestValueFirstSort(double new_value, OpVector<double> *values, UploadTransferBT *new_item, OpVector<UploadTransferBT> *items, int start_pos );
	static void		UpdateSmallestValueFirstSort(double new_value, OpVector<double> *values, UploadTransferBT *new_item, OpVector<UploadTransferBT> *items, int start_pos );
	UploadTransferBT *GetNextOptimisticPeer(BOOL factor_reciprocated, BOOL allow_snubbed, BOOL allow_not_interested = FALSE);
	BOOL			IsPeerInterestedInMe(BTClientConnector *client);
	P2PUploads		*GetUploads() { return &m_Uploads; }
	void			RefreshInterest();

protected:
	virtual BOOL	FindMoreSources();
//	BOOL			SeedTorrent(char *target);
	void			CloseTorrent();
	inline BOOL		IsSeeding() const { return m_seeding; }
	float			GetRatio() const;
	void			SortSource(DownloadSource* pSource, BOOL bTop);
	void			SortSource(DownloadSource* pSource);
	void			RemoveOverlappingSources(UINT64 nOffset, UINT64 nLength);
	void			StopTrying();
	void			RestrictUploadBandwidth();

protected:
	BOOL			GenerateTorrentDownloadID();	//Generate Peer ID
	void			OnFinishedTorrentBlock(UINT32 nBlock);
	void			CloseTorrentUploads();
	OP_STATUS		CreateCheckTimer();

public:
	friend class DownloadSource;
	friend class DownloadTransferBT;
	friend class Downloads;
};

class DownloadTransferBT : public DownloadTransfer
{
// Construction
public:
	DownloadTransferBT(DownloadSource* source, BTClientConnector* client);
	virtual ~DownloadTransferBT();
	
// Attributes
public:
	BTClientConnector*	m_client;
	BOOL			m_choked;
	BOOL			m_interested;

public:
	byte*			m_available;
	P2PFilePart*	m_requested;
	INT32			m_requestedCount;
	time_t			m_runthrottle;
	time_t			m_sourcerequest;

private:
	BTBlock			*m_rareblocks;
	BOOL			m_snubbed;
	bool			m_sending;
	bool			m_close_requested; // true -> Close when Send returns
	TRISTATE		m_close_arg;

// Operations
public:
	virtual BOOL	Initiate();
	virtual void	Close(TRISTATE bKeepSource);
	virtual void	Boost();
	virtual UINT32	GetAverageSpeed();
	virtual UINT32	GetMeasuredSpeed();
//	virtual BOOL	OnRun();
	virtual BOOL	OnConnected();
	virtual OP_STATUS	SubtractRequested(P2PFilePart** ppFragments);
	virtual OP_STATUS	UnrequestRange(UINT64 offset, UINT64 length);
	BOOL			HasCompleteBlock(UINT32 block);
	void			OnReceivedPiece(UINT64 offset, UINT64 length);
	void			StartClosing();
	BOOL			IsSnubbed() { return m_snubbed; }
	void			SetSnubbed(BOOL snubbed) { m_snubbed = snubbed; }

public:
	OP_STATUS	OnBitfield(BTPacket* packet);
	OP_STATUS	OnHave(BTPacket* packet);
	OP_STATUS	OnChoked(BTPacket* packet);
	OP_STATUS	OnUnchoked(BTPacket* packet);
	OP_STATUS	OnPiece(BTPacket* packet);
	void		SendFinishedBlock(UINT32 block);
	OP_STATUS	SendRequests();
	OP_STATUS	OnSnubbed();
	void		DecrementAvailableBlockCountOnDisconnect();

protected:
	void		Send(BTPacket* packet, BOOL release = TRUE);
	void		Send(OpByteBuffer *buffer);
	void		ShowInterest();
	OP_STATUS	SelectFragment(P2PFilePart* possible, UINT64* offset, UINT64* length);

	friend class BTDownload;
};

#endif // BT_DOWNLOAD_H
