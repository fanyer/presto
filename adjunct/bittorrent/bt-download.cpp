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
#include "modules/url/url_man.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
//#include "modules/ecmascript/ecma_pi.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"
#include "modules/console/opconsoleengine.h"
#include "modules/memtools/happy-malloc.h"
#include "modules/locale/oplanguagemanager.h"

#if defined(WIN32) && !defined(WIN_CE)
#include <sys/timeb.h>
#endif

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "adjunct/quick/Application.h"

#include "bt-util.h"
#include "bt-info.h"
#include "bt-download.h"
#include "bt-tracker.h"
#include "bt-client.h"
#include "bt-upload.h"
#include "dl-base.h"
#include "bt-globals.h"
#include "p2p-fileutils.h"

OP_STATUS P2PGlobalData::Init()
{
	if (
#if defined(_DEBUG) && defined (MSWIN) && defined(DEBUG_BT_RESOURCE_TRACKING)
		(m_BTResourceTracker = OP_NEW(BTResourceTracker, ())) &&
#endif
		(m_P2PFilePartPool = OP_NEW(P2PFilePartPool, ())) &&
		(m_P2PFiles = OP_NEW(P2PFiles, ())) &&
		(m_PacketPool = OP_NEW(BTPacketPool, ())) &&
		(m_Transfers = OP_NEW(Transfers, ())) &&
		(m_BTServerConnector = OP_NEW(BTServerConnector, ())) &&
		(m_Downloads = OP_NEW(Downloads, ())))
	{
		return OpStatus::OK;
	}
	Destroy();
	return OpStatus::ERR_NO_MEMORY;
}
void P2PGlobalData::Destroy()
{
	OP_DELETE(m_Downloads);
	OP_DELETE(m_BTServerConnector);
	OP_DELETE(m_Transfers);
	OP_DELETE(m_PacketPool);
	OP_DELETE(m_P2PFilePartPool);
	OP_DELETE(m_P2PFiles);
	m_PendingInfos.DeleteAll();

#if defined(_DEBUG) && defined (MSWIN) && defined(DEBUG_BT_RESOURCE_TRACKING)
	OP_DELETE(m_BTResourceTracker);
#endif
}

//////////////////////////////////////////////////////////////////////
// BTDownload construction

BTDownload::BTDownload() : DownloadBase(),
	m_torrentTracker(0),
	m_torrentPeerExchange(0),
	m_torrentUploaded(0),
	m_torrentDownloaded(0),
	m_torrentEndgame(FALSE),
	m_trackerError(FALSE),
	m_blocknumber(0), 
	m_blocklength(0),
	m_totalSeeders(0),
	m_totalPeers(0),
	m_tracker_state(BTTracker::TrackerNone),

	m_volume(0), m_total(0),
	m_torrentBlock(NULL),
	m_torrentBlockSize(0),
	m_torrentSize(0),
	m_torrentSuccess(0),
	m_seeding_stage(NotSeeding),
	m_create_files(FALSE),
	m_create_offset(0),
	m_checking_file(FALSE),
	m_checking_file_queued(FALSE),
	m_first_block(TRUE),
	m_checking_delay(SEED_TIMER_INTERVAL),
	m_available_block_count(NULL),

	m_torrentChoke(0),
	m_torrentChokeRefresh(0),
	m_torrentSources(0),
	m_verifyfile(NULL),
	m_verify_block_file(NULL),
	m_initial_ramp_up(FALSE)
{
	BT_RESOURCE_ADD("BTDownload", this);

	op_srand(op_time(NULL));
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTDownload::~BTDownload()"
#endif

BTDownload::~BTDownload()
{
	if(m_tracker_state != BTTracker::TrackerNone)
	{
		m_torrent.GetImpl()->GetTracker()->SendStopped(this, TRUE);
	}
	g_BTServerConnector->StopConnections(this);
	CloseTorrentUploads();

	// have to clear those explicitely, or their auto-deletion at the end of this destructor will cause
	// m_available_block_count to be recreated and thus leaked
	m_BTClientConnectors.Clear();

	if(m_verifyfile)
	{
		m_verifyfile->Release(m_create_files);
	}
	if(m_verify_block_file)
	{
		m_verify_block_file->Release(FALSE);
	}
	OP_DELETEA(m_torrentBlock);
	OP_DELETEA(m_available_block_count);

#if defined(DEBUG_BT_RESOURCE_TRACKING)
	for (UINT32 i = 0; i < m_files.GetCount(); ++i)
	{
		BT_RESOURCE_REMOVE(m_files.Get(i));
	}
#endif // DEBUG_BT_RESOURCE_TRACKING
	m_files.DeleteAll();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

OP_STATUS BTDownload::CreateCheckTimer()
{
	// Create the timer.
	if(m_seed_timer.get() == 0)
	{
		OpTimer *timer = OP_NEW(OpTimer, ());
		if (!timer)
		{
			OP_ASSERT(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}
		m_seed_timer = timer;
		m_seed_timer->SetTimerListener(this);
	}
	m_seed_timer->Start(m_checking_delay);

	return OpStatus::OK;
}

void BTDownload::ResetFileCheckState()
{
	m_first_block = TRUE;
	m_seeding_stage = NotSeeding;
}

OP_STATUS BTDownload::CheckExistingFile()
{
	m_complete = FALSE;
	m_create_files = FALSE;

	if(g_Downloads->CanStartCheckingFile() == FALSE)
	{
		m_checking_file_queued = TRUE;

		OP_STATUS status = CreateCheckTimer();
		if(status != OpStatus::OK)
		{
			return status;
		}
		return OpStatus::OK;
	}
	if(!(m_seeding_stage & PostSeeding))
	{
		m_seeding_stage = PreSeeding;
	}
	BOOL done = VerifyTorrentFiles(m_first_block);
	m_first_block = FALSE;

	if(done == FALSE)
	{
		OP_STATUS status = CreateCheckTimer();
		if(status != OpStatus::OK)
		{
			return status;
		}
		if(!(m_seeding_stage & PostSeeding))
		{
			m_seeding_stage = PreSeeding;
		}
	}
	else
	{
		g_Downloads->StopCheckingFile();
		m_checking_file = FALSE;
		m_checking_file_queued = FALSE;
		m_first_block = TRUE;

		if(m_seeding_stage & PreSeeding)
		{
			OpenFile();
		}
		m_seeding_stage = NotSeeding | PreSeedingDone;

		if(m_seeding_stage & NotSeeding)
		{
			OnRun();
		}
		else
		{
			m_seeding_stage = (PostSeedingDone | NotSeeding);
		}
	}
	return OpStatus::OK;
}

void BTDownload::OnTimeOut(OpTimer* timer)
{
	BOOL done = FALSE;

	if(m_paused)
	{
		m_seeding_stage = NotSeeding;
		m_seed_timer->Stop();
		g_Downloads->StopCheckingFile();
		m_checking_file = FALSE;
		m_checking_file_queued = FALSE;
		return;
	}
	OP_ASSERT(timer == m_seed_timer.get());

	if(m_checking_file == FALSE)
	{
		if(g_Downloads->CanStartCheckingFile() == FALSE)
		{
			m_checking_file_queued = TRUE;
			CreateCheckTimer();
			return;
		}
		else
		{
			if(!(m_seeding_stage & PostSeeding))
			{
				m_seeding_stage = PreSeeding;
			}
			m_checking_file = TRUE;
			m_checking_file_queued = FALSE;
		}
	}
	g_Downloads->StartCheckingFile();

	if(m_seeding_stage & PostSeeding || m_seeding_stage & PreSeeding)
	{
		done = VerifyTorrentFiles(m_first_block);
		m_first_block = FALSE;
	}
	if(done == FALSE)
	{
		m_seed_timer->Start(m_checking_delay);
	}
	else
	{
		g_Downloads->StopCheckingFile();
		m_checking_file = FALSE;
		m_checking_file_queued = FALSE;
		m_first_block = TRUE;

		SetStartTimer();

		if(OpStatus::IsError(m_metadata.WriteMetaFile(TRUE)))
		{
			// not really fatal
			OP_ASSERT(FALSE);
		}
		if(m_seeding_stage & PostSeeding)
		{
			m_seeding_stage = (PostSeedingDone | NotSeeding);
		}
		if(m_seeding_stage & PreSeeding)
		{
			OpenFile();
			m_seeding_stage = NotSeeding | PreSeedingDone;
		}
		if((m_seeding_stage & NotSeeding) && !(m_seeding_stage & PostSeedingDone))
		{
			OnRun();
		}
	}
}

BOOL BTDownload::VerifyTorrentBlock(UINT32 block)
{
	BOOL success = FALSE;
	// file handle should be cached
	OpString target_directory;

	m_torrent.GetImpl()->GetTargetDirectory(target_directory);

	if(!m_verify_block_file)
	{
		 m_verify_block_file = g_P2PFiles->Open(m_localname, FALSE, FALSE, target_directory, &m_metadata, &m_files, m_torrent.GetImpl()->GetBlockSize(), TRUE);
	}
	if(!m_verify_block_file)
	{
		return FALSE;
	}
	if(block < m_torrent.GetImpl()->GetBlockCount())
	{
		UINT64 read;

		m_volume = (UINT64)block * (UINT64)m_torrentSize;

		UINT64 readlen = min(m_torrent.GetImpl()->GetBlockSize(), (m_total - m_volume));

		m_torrent.GetImpl()->BeginBlockTest();

		byte* pBuffer	= OP_NEWA(byte, (int)readlen);

#ifdef _DEBUG
//		memset(pBuffer, 'Æ', (int)readlen);
#endif // _DEBUG

		if(pBuffer && m_verify_block_file->Read((UINT64)block * (UINT64)m_torrentSize, pBuffer, readlen, &read, TRUE))
		{
			m_torrent.GetImpl()->AddToTest(pBuffer, (UINT32)readlen);

			if(m_torrent.GetImpl()->FinishBlockTest(block))
			{
				m_torrentBlock[block] = TS_TRUE;
				m_torrentSuccess++;

				success = TRUE;

				m_metadata.SetBlockAsDone(block);
			}
			else
			{
				m_metadata.SetBlockAsDirty(block);
			}
		}
		OP_DELETEA(pBuffer);
	}

	return success;
}

BOOL BTDownload::VerifyTorrentFiles(BOOL first)
{
	BOOL done = FALSE;
	P2PFilePart* corrupted = NULL;

	if(first)	// first time we enter this loop
	{
		OpString path, target_directory;

		m_torrentSuccess = 0;

		GenerateLocalName();

		m_torrent.GetImpl()->GetTargetDirectory(target_directory);

		if(m_file)
		{
			if(m_file->IsOpen())
			{
				m_file->Close();
			}
		}
		BOOL files_changed = FALSE;

		m_verifyfile = g_P2PFiles->Open(m_localname, FALSE, FALSE, target_directory, &m_metadata, &m_files, m_torrent.GetImpl()->GetBlockSize(), FALSE);

		m_total = m_torrent.GetImpl()->GetTotalSize();
		m_blocknumber = 0;
		m_blocklength = m_torrent.GetImpl()->GetBlockSize();
		m_volume = 0;

		if(m_verifyfile == NULL)
		{
			// we'll create and expand them in a loop
			m_verifyfile = g_P2PFiles->Open(m_localname, TRUE, TRUE, target_directory, &m_metadata, &m_files, m_torrent.GetImpl()->GetBlockSize(), FALSE);
			m_create_files = TRUE;
			m_create_offset = 0;
		}
		if(m_verifyfile)
		{
			UINT32 cnt;
			OpString fullpath;
			time_t now = time(NULL);
			OpVector<P2PVirtualFile>* files = m_verifyfile->GetFiles();

			for(cnt = 0; files && cnt < files->GetCount(); cnt++)
			{
				P2PVirtualFile *file = files->Get(cnt);

				if(file)
				{
					if(OpStatus::IsSuccess(file->GetFullFilePath(fullpath)))
					{
						BOOL changed;

						if(m_metadata.HasFileChanged(fullpath, changed, file->GetLength()) == OpStatus::ERR_FILE_NOT_FOUND)
						{
							// file is not in the metadata, probably because this is the first time
							m_metadata.UpdateFileData(fullpath, now, file->GetLength());
							files_changed |= TRUE;
						}
						else
						{
							files_changed |= changed;
						}
					}
					else
					{
						files_changed |= TRUE;
					}
				}
				else
				{
					files_changed |= TRUE;
				}
			}
			if(files->GetCount() == 0)
			{
				files_changed |= TRUE;
			}
		}
		if(!files_changed)
		{
			if(m_verifyfile)
			{
				m_verifyfile->Release(m_create_files);	// will close the files
				m_verifyfile = NULL;
			}
			m_blocknumber = 0;

			OP_ASSERT(m_file);
			if(m_file)
			{
				m_file->SetTotal(m_torrent.GetImpl()->GetTotalSize());
			}
			while(m_blocknumber < m_torrent.GetImpl()->GetBlockCount())
			{
				UINT64 readlen = min(m_blocklength, (m_total - m_volume));

				if(m_metadata.IsBlockDone(m_blocknumber))
				{
					m_torrentBlock[m_blocknumber] = TS_TRUE;
					m_torrentSuccess++;
				}
				else
				{
					m_torrentBlock[m_blocknumber] = TS_FALSE;

					corrupted = P2PFilePart::New();
					if (corrupted)
					{
						corrupted->m_offset	= (UINT64)m_blocknumber * (UINT64)m_torrentSize;
						corrupted->m_length	= min(m_torrentSize, m_size - corrupted->m_offset);
					}
				}
				if(corrupted != NULL && m_file != NULL)
				{
					SubtractHelper(&corrupted, m_torrentBlock, m_torrentBlockSize, m_torrentSize);

					for(P2PFilePart* range = corrupted ; range != NULL; range = range->m_next)
					{
						m_file->InvalidateRange(range->m_offset, range->m_length);
						RemoveOverlappingSources(range->m_offset, range->m_length);
					}
					corrupted->DeleteChain();
					corrupted = NULL;
				}
				m_volume += readlen;

				m_blocknumber++;
			}
			return TRUE;
		}
		if(m_verifyfile == NULL || m_create_files)
		{
			OP_ASSERT(m_file);
			if(m_file)
			{
				m_file->SetTotal(m_torrent.GetImpl()->GetTotalSize());
			}
			while(m_blocknumber < m_torrent.GetImpl()->GetBlockCount())
			{
				m_torrentBlock[m_blocknumber++] = TS_FALSE;
			}

			corrupted = P2PFilePart::New();
			if (corrupted)
			{
				corrupted->m_offset	= 0; // (UINT64)m_blocknumber * (UINT64)m_torrentSize;
				corrupted->m_length	= m_total;

				if(m_file != NULL)
				{
					if(m_torrentBlock != NULL)
					{
						SubtractHelper(&corrupted, m_torrentBlock, m_torrentBlockSize, m_torrentSize);
					}
					for(P2PFilePart* range = corrupted ; range != NULL; range = range->m_next)
					{
						m_file->InvalidateRange(range->m_offset, range->m_length);
						RemoveOverlappingSources(range->m_offset, range->m_length);
					}
				}
				corrupted->DeleteChain();
			}
			if(m_create_files == FALSE)
			{
				return TRUE;
			}
		}
		m_file->SetTotal(m_torrent.GetImpl()->GetTotalSize());
	}
	double tmp_time_before = g_op_time_info->GetWallClockMS();

	if(m_create_files)
	{
		while(m_create_offset < m_torrent.GetImpl()->GetTotalSize())
		{
			UINT32 data[8192];
			UINT64 written;

			memset(&data, 0, sizeof(data));

			m_create_offset += FILE_CREATE_STEPS;
			m_create_offset = min(m_create_offset, m_torrent.GetImpl()->GetTotalSize());

			if(m_verifyfile != NULL)
			{
				if(m_verifyfile->Write(m_create_offset, (byte *)&data, (UINT64)sizeof(data), &written, FALSE) == FALSE)
				{
					m_verifyfile->Release(m_create_files);	// will close the files
					m_verifyfile = NULL;
					return TRUE;
				}
				FileDownloadCheckingFiles(m_create_offset, m_total);
			}
			break;
		}
		done = (m_create_offset >= m_torrent.GetImpl()->GetTotalSize());
		if(done)
		{
			if(m_verifyfile)
			{
				m_verifyfile->Release(m_create_files);	// will close the files
			}
			m_verifyfile = NULL;
			m_create_files = FALSE;

			DEBUGTRACE8_UP(UNI_L("**** FILE CREATE: %lld bytes created"), (UINT64)(m_torrent.GetImpl()->GetTotalSize()));
			DEBUGTRACE8_UP(UNI_L("(%d blocks)\n"), m_torrentSuccess);
		}
	}
	else
	{
		while(m_blocknumber < m_torrent.GetImpl()->GetBlockCount())
		{
			UINT64 read;
			UINT64 readlen = min(m_blocklength, (m_total - m_volume));

			m_torrent.GetImpl()->BeginBlockTest();

			byte* pBuffer	= OP_NEWA(byte, (int)readlen);

			if (pBuffer)
			{
				if(m_verifyfile->Read((UINT64)m_blocknumber * (UINT64)m_torrentSize, pBuffer, readlen, &read, TRUE))
					m_torrent.GetImpl()->AddToTest(pBuffer, (UINT32)readlen);

				OP_DELETEA(pBuffer);
			}

			if(m_torrent.GetImpl()->FinishBlockTest(m_blocknumber))
			{
				DEBUGTRACE8_UP(UNI_L("%s"), UNI_L("*"));

				m_torrentBlock[m_blocknumber] = TS_TRUE;
				m_torrentSuccess++;

				m_metadata.SetBlockAsDone(m_blocknumber);
			}
			else
			{
				DEBUGTRACE8_UP(UNI_L("%s"), UNI_L("."));
				m_torrentBlock[m_blocknumber] = TS_FALSE;

				corrupted = P2PFilePart::New();
				if (corrupted)
				{
					corrupted->m_offset	= (UINT64)m_blocknumber * (UINT64)m_torrentSize;
					corrupted->m_length	= min(m_torrentSize, m_size - corrupted->m_offset);
				}
				m_metadata.SetBlockAsDirty(m_blocknumber);
			}
#ifdef _DEBUG
			if((m_blocknumber % 50) == 0)
			{
				DEBUGTRACE8_UP(UNI_L(" (%d)\n"), m_blocknumber);
			}
#endif
			if(corrupted != NULL && m_file != NULL)
			{
				SubtractHelper(&corrupted, m_torrentBlock, m_torrentBlockSize, m_torrentSize);

				for(P2PFilePart* range = corrupted ; range != NULL; range = range->m_next)
				{
					m_file->InvalidateRange(range->m_offset, range->m_length);
					RemoveOverlappingSources(range->m_offset, range->m_length);
				}
				corrupted->DeleteChain();
			}
			m_blocknumber++;
			m_volume += readlen;

			FileDownloadCheckingFiles(m_volume, m_total);

			break;	// this is intentional, we will trigger run 2+ with a timer
		}

		done = (m_blocknumber == m_torrent.GetImpl()->GetBlockCount());
		if(done)
		{
			m_verifyfile->Release(m_create_files);	// will close the files
			m_verifyfile = NULL;

			DEBUGTRACE8_UP(UNI_L("**** RESULT: %lld bytes downloaded already "), (UINT64)(m_torrentSuccess * m_blocklength));
			DEBUGTRACE8_UP(UNI_L("(%d blocks)\n"), m_torrentSuccess);
		}
	}
	double interval;
	double tmp_time_after = g_op_time_info->GetWallClockMS();

	interval = tmp_time_after - tmp_time_before;
//	interval += 100;
	m_checking_delay = (UINT32)interval;

	return done;
}

void BTDownload::SubtractHelper(P2PFilePart** ppCorrupted, byte* block, UINT64 nBlock, UINT64 size)
{
	UINT64 offset = 0;

	while(nBlock-- && *ppCorrupted)
	{
		if(*block++ == TS_TRUE)
		{
			P2PFilePart::Subtract(ppCorrupted, offset, size );
		}
		offset += size;
	}
}

//////////////////////////////////////////////////////////////////////
// remove overlapping sources

void BTDownload::RemoveOverlappingSources(UINT64 offset, UINT64 length)
{
	DownloadSource* source;
	for(source = m_sources.First(); source != NULL ; )
	{
		DownloadSource* next = source->Suc();

		if(source->TouchedRange(offset, length))
		{
			DEBUGTRACE(UNI_L("Removing source %s "), (uni_char *)source->m_address);
			DEBUGTRACE8(UNI_L("due to overlap at offset %d"), offset);
			DEBUGTRACE8(UNI_L(", end %d\n"), offset + length - 1);

			source->Out();
			source->Remove( TRUE, TRUE );
		}
		source = next;
	}
}

OP_STATUS BTDownload::SetTorrent(BTInfoImpl* torrent)
{
	if(torrent == NULL) return OpStatus::ERR_NULL_POINTER;
//	if(m_torrent.IsAvailable()) return OpStatus::ERR_OUT_OF_RANGE;
	if(!torrent->IsAvailable()) return OpStatus::ERR_OUT_OF_RANGE;

	m_torrent.GetImpl()->Copy( torrent );

	m_bBTH = TRUE;
	m_pBTH = m_torrent.GetImpl()->GetInfoSHA();

//	when stopping and resuming a torrent, m_torrentBlock needs to be reset
	OP_DELETEA(m_torrentBlock);

	m_torrentSize	= m_torrent.GetImpl()->GetBlockSize();
	m_torrentBlockSize	= m_torrent.GetImpl()->GetBlockCount();
	m_torrentBlock	= OP_NEWA(byte, m_torrentBlockSize);
	if (!m_torrentBlock)
	{
		m_torrentSize = m_torrentBlockSize = 0;
		return OpStatus::ERR_NO_MEMORY;
	}
	memset(m_torrentBlock, 0, sizeof(byte) * m_torrentBlockSize);

	m_size = m_torrent.GetImpl()->GetTotalSize();

	m_torrent.GetImpl()->GetName(m_remotename);

	SetModified();

	OpString target_directory;

	m_torrent.GetImpl()->GetTargetDirectory(target_directory);

	// - create torrent directory, separate directory from download perhaps?
	// - save torrent file here
	m_torrent.GetImpl()->SaveTorrentFile(target_directory);

	FileDownloadInitalizing();

	g_Transfers->StartTimer(this);	// kick start the transfers immediately

	// get the array of files
	BTInfoImpl::BTFile *files = m_torrent.GetImpl()->GetFiles();

	for(int nFile = 0; nFile < m_torrent.GetImpl()->GetFileCount(); nFile++)
	{
		P2PVirtualFile *virt_file = OP_NEW(P2PVirtualFile, (files[nFile].m_sPath, files[nFile].m_nSize, files[nFile].m_nOffset));

		BT_RESOURCE_ADD("P2PVirtualFile-SetTorrent", virt_file);
		m_files.Add(virt_file);
	}
	OpStatus::Ignore(m_metadata.Init(m_torrentBlockSize, m_remotename));

	return OpStatus::OK;
}

BOOL BTDownload::RunTorrent(time_t tNow)
{
	if(!m_torrent.GetImpl()->IsAvailable())
	{
		return TRUE;
	}
	if(tNow > m_torrentChoke)
	{
		ChokeTorrent(tNow);
	}
	if(m_file != NULL && m_file->IsOpen() == FALSE)
	{
		if(!PrepareFile())
		{
			return FALSE;
		}
	}

	BOOL live = !IsPaused();

	if(!(m_seeding_stage & PreSeeding) && live && (m_tracker_state <= BTTracker::TrackerStartSent) && !m_checking_file)
	{
		if(m_tracker_state == BTTracker::TrackerNone)
		{
			GenerateTorrentDownloadID();
			g_BTServerConnector->AcceptConnections(this, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort));
		}
		if(tNow > m_torrentTracker )
		{
			m_tracker_state = BTTracker::TrackerStartSent;
			m_torrentTracker	= tNow + 120;
			m_torrentSources	= m_torrentTracker;

			m_torrent.GetImpl()->GetTracker()->SendStarted( this );
			m_torrent.GetImpl()->GetTracker()->SendScrape(this);
		}
	}
	else if(!live && m_tracker_state > BTTracker::TrackerNone)
	{
		m_torrent.GetImpl()->GetTracker()->SendStopped( this );

		m_tracker_state = BTTracker::TrackerNone;

		m_torrentTracker = 0;
		m_torrentSources = 0;
		memset(m_peerid.n, 0, 20);

		g_BTServerConnector->StopConnections(this);
	}
	if((m_tracker_state == BTTracker::TrackerStart || m_tracker_state == BTTracker::TrackerUpdate) &&
		tNow > m_torrentTracker )
	{
		m_tracker_state = BTTracker::TrackerUpdate;

		m_torrentTracker = tNow  + 120;
		m_torrentSources	= m_torrentTracker;

		m_torrent.GetImpl()->GetTracker()->SendUpdate( this );
		m_torrent.GetImpl()->GetTracker()->SendScrape(this);
	}
	if(!m_torrent.GetImpl()->IsPrivate() && m_torrentPeerExchange < tNow)
	{
		OpStatus::Ignore(m_BTClientConnectors.SendPeerExchange(m_torrentPeerExchange, m_sources));
		m_torrentPeerExchange = tNow + 60;
	}
	return TRUE;
}

void BTDownload::OnTrackerEvent(BOOL success,  OpString& tracker_url, char *reason)
{
	OpString languagestring;

	m_trackerError = !success;
	m_torrentTrackerError.Empty();

	if (reason != NULL)
	{
		m_torrentTrackerError.Set(reason);
	}
	else if ( m_trackerError )
	{
		g_languageManager->GetString(Str::S_BITTORRENT_TRACKER_ERROR, m_torrentTrackerError);

		m_torrentTrackerError.Append(tracker_url);
	}
	if(m_torrentTrackerError.HasContent())
	{
		if (g_console)
		{
			// FIXME: Severity?
			OpConsoleEngine::Message msg(OpConsoleEngine::BitTorrent, OpConsoleEngine::Error);

			g_languageManager->GetString(Str::S_BITTORRENT_TRACKER_ERROR_CONTEXT, msg.context);
			msg.context.Append(" ");
			msg.context.Append(m_torrent.GetImpl()->GetName());
			msg.context.Append(":");

			msg.message.Set(m_torrentTrackerError);

			TRAPD(rc, g_console->PostMessageL(&msg));
		}
	}
}

void BTDownload::OnFinishedTorrentBlock(UINT32 block)
{
	DownloadTransferBT* transfer = (DownloadTransferBT*)GetFirstTransfer();
	while (transfer)
	{
		DownloadTransferBT* next = (DownloadTransferBT*)transfer->m_dlNext;
		// transfer may be removed from the list and deleted if send fails
		transfer->SendFinishedBlock(block);
		transfer = next;
	}
	m_metadata.SetBlockAsDone(block);
	if(OpStatus::IsError(m_metadata.WriteMetaFile()))
	{
		// not really fatal
		OP_ASSERT(FALSE);
	}
}
/*
void BTDownload::AddUpload(UploadTransferBT* upload)
{
	DEBUGTRACE8_UP("UL adding upload: 0x%08x, ", upload);
	DEBUGTRACE8_UP("download: 0x%08x, ", this);
	DEBUGTRACE8_UP("uploads: 0x%08x, ", m_torrentUploads);
	DEBUGTRACE8_UP("choked: %d\n", upload->m_choked);

//	if(m_torrentUploads.Find( upload ) == -1)
//		m_torrentUploads.Add( upload );
}

void BTDownload::RemoveUpload(UploadTransferBT* upload)
{
//	m_torrentUploads.RemoveByItem(upload);
}
*/
void BTDownload::CloseTorrentUploads()
{
	UINT32 pos;

	for(pos = 0; pos < m_BTClientConnectors.GetCount(); pos++)
	{
		BTClientConnector *client = m_BTClientConnectors.Get(pos);
		if(client != NULL)
		{
			client->CloseUpload();
		}
	}
/*
	for (UINT32 pos = 0; pos < m_torrentUploads.GetCount(); pos++)
	{
		UploadTransferBT* upload = (UploadTransferBT*)m_torrentUploads.Get(pos);
		if(upload != NULL)
		{
			m_Uploads.Remove(upload);
			upload->Close();
		}
	}
	m_torrentUploads.Clear();
*/
}

BOOL BTDownload::IsPeerInterestedInMe(BTClientConnector *client_in)
{
	UINT32 idx;

	for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
	{
		BTClientConnector *client = m_BTClientConnectors.Get(idx);
		if(client)
		{
			if(client->m_GUID == client_in->m_GUID && client != client_in)
			{
				if(client->m_upload && client->m_upload->m_interested && client->m_upload->m_live)
				{
					return TRUE;
				}
				break;
			}
		}
	}
	return FALSE;
}

void BTDownload::RefreshInterest()
{
	UINT32 idx;

	for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
	{
		BTClientConnector *client = m_BTClientConnectors.Get(idx);
		if(client)
		{
			if(client->m_downloadtransfer != NULL &&
				client->m_download != NULL)
			{
				client->m_downloadtransfer->ShowInterest();
			}
		}
	}
}

BOOL BTDownload::IsUnchokable(UploadTransferBT *transfer, BOOL allow_snubbed, BOOL allow_not_interested)
{
	UINT32 idx, total_clients = 0;

	for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
	{
		BTClientConnector *client = m_BTClientConnectors.Get(idx);
		if(client)
		{
			if(client->m_GUID == transfer->m_pSHA1) // same client ID?
			{
				if(
					client->m_downloadtransfer != NULL &&
					client->m_download != NULL &&
//					client->m_downloadtransfer->m_state == dtsDownloading &&
					client->m_downloadtransfer->m_has_complete_file == FALSE &&
					transfer->m_live &&
					(client->m_downloadtransfer->IsSnubbed() == FALSE || allow_snubbed))
				{
					if(allow_not_interested || transfer->m_interested)
					{
						DEBUGTRACE_CHOKE(UNI_L("UNCHOKABLE PEER: %s\n"), transfer->m_address.CStr());
						return TRUE;
					}
				}
				else
				{
/*
					DEBUGTRACE_CHOKE(UNI_L("Not unchokable: %.10s, "), transfer->m_address.CStr());
					DEBUGTRACE_CHOKE(UNI_L("interested: %d, "), transfer->m_interested ? 1 : 0);
					if(client->m_downloadtransfer != NULL)
					{
						DEBUGTRACE_CHOKE(UNI_L("seeder: %d, "), client->m_downloadtransfer->m_has_complete_file ? 1 : 0);
						DEBUGTRACE_CHOKE(UNI_L("snubbed: %d "), client->m_downloadtransfer->IsSnubbed() ? 1 : 0);
						DEBUGTRACE_CHOKE(UNI_L("(snubbed allowed: %d)"), allow_snubbed ? 1 : 0);
					}
					DEBUGTRACE_CHOKE(UNI_L("\n"), 0);
*/
				}
			}
			total_clients++;
		}
	}
//	DEBUGTRACE_CHOKE(UNI_L("TOTAL clients in loop: %d\n"), total_clients);
	return FALSE;
}

void BTDownload::GetChokes(OpVector<UploadTransferBT> *chokes)
{
	UINT32 pos;

	for(pos = 0; pos < m_chokes.GetCount(); pos++)
	{
		chokes->Add(m_chokes.Get(pos));
	}
}

void BTDownload::GetUnchokes(OpVector<UploadTransferBT> *unchokes)
{
	UINT32 pos;

	for(pos = 0; pos < m_unchokes.GetCount(); pos++)
	{
		unchokes->Add(m_unchokes.Get(pos));
	}
}

/**
* Choose the next peer, optimistically, that should be unchoked.
* factor_reciprocated - if true, factor in how much (if any) this peer has reciprocated when choosing
* allow_snubbed - allow the picking of snubbed-state peers as last resort
* returns the next peer to optimistically unchoke, or null if there are no peers available
*/
UploadTransferBT *BTDownload::GetNextOptimisticPeer(BOOL factor_reciprocated, BOOL allow_snubbed, BOOL allow_not_interested)
{
	//find all potential optimistic peers

	UINT32 pos;
	OpP2PVector<UploadTransferBT> optimistics;

	for(pos = 0; pos < m_Uploads.GetCount(NULL); pos++)
	{
		UploadTransferBT* transfer = (UploadTransferBT*)m_Uploads.Get(pos);

		if(transfer)
		{
			if(transfer->IsChoked() && IsUnchokable(transfer, FALSE))
			{
				optimistics.AddIfNotExists(transfer);
			}
		}
	}
//	DEBUGTRACE_CHOKE(UNI_L("OPTIMISTIC COUNT(1): %d\n"), optimistics.GetCount());

	if(optimistics.GetCount() == 0 && allow_snubbed)
	{
		for(pos = 0; pos < m_Uploads.GetCount(NULL); pos++)
		{
			UploadTransferBT* transfer = (UploadTransferBT*)m_Uploads.Get(pos);

			if(transfer)
			{
				if(transfer->IsChoked() && IsUnchokable(transfer, TRUE))
				{
					optimistics.AddIfNotExists(transfer);
				}
			}
		}
	}
//	DEBUGTRACE_CHOKE(UNI_L("OPTIMISTIC COUNT(2): %d\n"), optimistics.GetCount());

	if(optimistics.GetCount() == 0)
	{
		//no unchokable peers avail
		return NULL;
	}

	//factor in peer reciprocation ratio when picking optimistic peers
	if(factor_reciprocated)
	{
		OpVector<UploadTransferBT> ratioed_peers;
		OpVector<double> ratios;
		UINT32 i;

		//order by upload ratio
		for(i = 0; i < optimistics.GetCount(); i++ )
		{
			UploadTransferBT *peer = optimistics.Get(i);

			UINT32 idx;

			for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
			{
				BTClientConnector *client = m_BTClientConnectors.Get(idx);
				if(client)
				{
					if(client->m_downloadtransfer &&
						client->m_upload &&
						client->m_upload->m_live &&
						client->m_GUID == peer->m_pSHA1) // same client ID?
					{
						// score of >0 means we've uploaded more, <0 means we've downloaded more
						long score = (long)(peer->m_uploaded - client->m_downloadtransfer->m_downloaded);

						// higher value = worse score
						UpdateLargestValueFirstSort((double)score, &ratios, peer, &ratioed_peers, 0);
						break;
					}
				}
			}
		}
        // The fragment added to 4.0 is necessarily at least 1, so this ratio is at most 1.
		double factor = 5.0 / (4.0 + (double)RAND_MAX / (double)rand()); // map to sorted list using a logistic curve

		int pos = (int)(factor * ratioed_peers.GetCount());

	    UploadTransferBT *peer = (UploadTransferBT *)ratioed_peers.Get(pos);

//		DEBUGTRACE_CHOKE(UNI_L("OPTIMISTIC peer(1): %s\n"), peer->m_address.CStr());

		return peer;
	}
	int rand_pos = rand();

	rand_pos = rand_pos % optimistics.GetCount();

	UploadTransferBT *peer = optimistics.Get(rand_pos);

//	DEBUGTRACE_CHOKE(UNI_L("OPTIMISTIC peer(2): %s\n"), peer->m_address.CStr());

	return peer;
}

/**
* Update (if necessary) the given list with the given value while maintaining a largest-value-first (as seen so far) sort order.
* new_value - to use
* values - existing values array
* new_item - to insert
* items - existing items
* start_pos - index at which to start compare
*/
void BTDownload::UpdateLargestValueFirstSort(double new_value, OpVector<double> *values, UploadTransferBT *new_item, OpVector<UploadTransferBT> *items, int start_pos )
{
	UINT32 pos;

	for(pos = start_pos; pos < values->GetCount(); pos++)
	{
		if(new_value >= *values->Get(pos))
		{
			values->Insert(pos, &new_value);
			items->Insert(pos, new_item);
			return;
		}
	}
	if(values->GetCount() == 0)
	{
		values->Add(&new_value);
		items->Add(new_item);
	}
	return;
}

/**
* Update (if necessary) the given list with the given value while maintaining a smallest-value-first (as seen so far) sort order.
* new_value - to use
* values - existing values array
* new_item - to insert
* items - existing items
* start_pos - index at which to start compare
*/
void BTDownload::UpdateSmallestValueFirstSort(double new_value, OpVector<double> *values, UploadTransferBT *new_item, OpVector<UploadTransferBT> *items, int start_pos )
{
	UINT32 pos;

	for(pos = start_pos; pos < values->GetCount(); pos++)
	{
		if(new_value <= *values->Get(pos))
		{
			values->Insert(pos, &new_value);
			items->Insert(pos, new_item);
			return;
		}
	}
	if(values->GetCount() == 0)
	{
		values->Add(&new_value);
		items->Add(new_item);
	}
	return;
}

void BTDownload::GetImmediateUnchokes(UINT32 max_to_unchoke, OpVector<UploadTransferBT> *to_unchoke)
{
	//count all the currently unchoked peers
    int num_unchoked = GetUploads()->GetTransferCount(upsUnchoked);

    //if not enough unchokes
	int needed = max_to_unchoke - num_unchoked;
	BOOL seeding = m_complete;

	if(needed > 0)
	{
		int i;

		for(i = 0; i < needed; i++)
		{
			UploadTransferBT *peer = GetNextOptimisticPeer(seeding ? FALSE : TRUE, seeding ? FALSE : TRUE);
			if(peer == NULL)
			{
				break;  //no more new unchokes avail
			}
			to_unchoke->Add(peer);
			peer->SetOptimisticUnchoke(TRUE);
		}
	}
}

void BTDownload::CalculateUnchokes(UINT32 max_to_unchoke, BOOL force_refresh)
{
	BOOL seeding = m_complete;
	UINT32 pos;
	OpP2PVector<UploadTransferBT> optimistic_unchokes;
	OpP2PVector<UploadTransferBT> best_peers;
	OpP2PVector<double> bests;

    //get all the currently unchoked peers
	for(pos = 0; pos < m_Uploads.GetCount(NULL); pos++)
	{
		UploadTransferBT* transfer = (UploadTransferBT*)m_Uploads.Get(pos);

		if(transfer)
		{
			if(!transfer->IsChoked()) // is it unchoked?
			{
				if(seeding)
				{
					DEBUGTRACE_CHOKE(UNI_L("PEER %s is UNCHOKED\n"), transfer->m_address.CStr());

					if(((BTDownload *)transfer->m_download)->IsUnchokable(transfer, FALSE))
					{
						m_unchokes.AddIfNotExists(transfer);
					}
					else
					{
						//should be immediately choked
						m_chokes.AddIfNotExists(transfer);
					}
				}
				else
				{
					DEBUGTRACE_CHOKE(UNI_L("PEER %s is UNCHOKED\n"), transfer->m_address.CStr());

					if(((BTDownload *)transfer->m_download)->IsUnchokable(transfer, TRUE))
					{
						m_unchokes.AddIfNotExists(transfer);

						if(transfer->IsOptimisticUnchoke())
						{
							optimistic_unchokes.AddIfNotExists(transfer);
						}
					}
					else
					{
						//should be immediately choked
						m_chokes.AddIfNotExists(transfer);
					}
				}
			}
		}
	}
	if(seeding)
	{
	    //if too many unchokes
		while(m_unchokes.GetCount() > max_to_unchoke)
		{
			m_chokes.AddIfNotExists(m_unchokes.Remove(m_unchokes.GetCount() - 1));
		}
		if(force_refresh)
		{
			//we only recalculate the uploads when we're force to refresh the optimistic unchokes
			UINT32 max_optimistic = ((max_to_unchoke - 1) / 5) + 1;  //one optimistic unchoke for every 5 upload slots
//			UINT32 max_optimistic = 8;  //one optimistic unchoke for every 5 upload slots

			//we need to make room for new opt unchokes by finding the "worst" peers
			OpVector<UploadTransferBT> peers_ordered_by_rate;
			OpVector<UploadTransferBT> peers_ordered_by_uploaded;
			OpVector<double> rates;
			OpVector<double> uploaded;

			//calculate factor rankings
			for(pos = 0; pos < m_unchokes.GetCount(); pos++)
			{
				UploadTransferBT *transfer = m_unchokes.Get(pos);

				if(transfer != NULL)
				{
					// force update of transfer speed
					transfer->m_transferSpeed.AddPoint(0);

					double rate = transfer->m_transferSpeed.GetKbps();

//					DEBUGTRACE_CHOKE(UNI_L("UPLOAD TRANSFER RATE FOR: %s, "), (uni_char *)transfer->m_address);
//					DEBUGTRACE_CHOKE(UNI_L("%.2f\n"), (float)rate);

					//filter out really slow peers
					if(rate > 3)
					{
						UpdateSmallestValueFirstSort(rate, &rates, transfer, &peers_ordered_by_rate, 0);

						//calculate order by the total number of bytes we've uploaded to them
						UpdateLargestValueFirstSort((double)(INT64)transfer->m_uploaded, &uploaded, transfer, &peers_ordered_by_uploaded, 0);
					}
				}
			}
			OpVector<UploadTransferBT> peers_ordered_by_rank;
			OpVector<double> ranks;

			// combine factor rankings to get best
			for(pos = 0; pos < m_unchokes.GetCount(); pos++)
			{
				UploadTransferBT *transfer = m_unchokes.Get(pos);

				if(transfer != NULL)
				{
					INT32 rate_factor = peers_ordered_by_rate.Find(transfer);
					INT32 upload_factor = peers_ordered_by_uploaded.Find(transfer);

					if(rate_factor == -1)
					{
						// wasn't download fast enough, skip add so it'll be choked automatically
						continue;
					}
					INT32 rank_factor = rate_factor - upload_factor;

					BTDownload::UpdateLargestValueFirstSort((double)rank_factor, &ranks, transfer, &peers_ordered_by_rank, 0);
				}
			}
			//make space for new optimistic unchokes
			while(peers_ordered_by_rank.GetCount() > max_to_unchoke - max_optimistic)
			{
				peers_ordered_by_rank.Remove(peers_ordered_by_rank.GetCount() - 1);
			}
			OpP2PVector<UploadTransferBT> to_unchoke;
			OpP2PVector<UploadTransferBT> removes;

			//update choke list with drops and unchoke list with optimistic unchokes

			for(pos = 0; pos < m_unchokes.GetCount(); pos++)
			{
				UploadTransferBT *transfer = m_unchokes.Get(pos);

				if(transfer != NULL)
				{
					transfer->SetOptimisticUnchoke(FALSE);

					if(peers_ordered_by_rank.Find(transfer) == -1)	// should be choked
					{
						//we assume that any/all chokes are to be replace by optimistics
						UploadTransferBT *optimistic_peer = GetNextOptimisticPeer(FALSE, FALSE);

						if(optimistic_peer != NULL)	// only choke if we've got a peer to replace it with
						{
							m_chokes.AddIfNotExists(transfer);
							removes.AddIfNotExists(transfer);
							to_unchoke.AddIfNotExists(optimistic_peer);
							optimistic_peer->SetOptimisticUnchoke(TRUE);
						}
					}
				}
			}
			for(pos = 0; pos < removes.GetCount(); pos++)
			{
				UploadTransferBT *transfer = removes.Get(pos);

				if(transfer != NULL)
				{
					INT32 idx = m_unchokes.Find(transfer);

					if(idx != -1)
					{
						m_unchokes.Remove(idx);
					}
				}
			}
			for(pos = 0; pos < to_unchoke.GetCount(); pos++)
			{
				m_unchokes.AddIfNotExists(to_unchoke.Get(pos));
			}
		}
	}
	else
	{
		UINT32 max_optimistic = ((max_to_unchoke - 1) / 4) + 1;  // 2 optimistic unchokes for every 10 upload slots

	    if(!force_refresh) //ensure current optimistic unchokes remain unchoked
		{
			for(pos = 0; pos < optimistic_unchokes.GetCount(); pos++)
			{
				UploadTransferBT *transfer = optimistic_unchokes.Get(pos);
				if(transfer != NULL)
				{
					if(pos < max_optimistic)
					{
						//add them to the front of the "best" list
						best_peers.Insert(0, transfer);
					}
					else
					{
						// too many optimistic
						transfer->SetOptimisticUnchoke(FALSE);
					}
				}
			}
		}
		int start_pos = best_peers.GetCount();

		//fill slots with peers who we are currently downloading the fastest from
		for(pos = 0; pos < m_Uploads.GetCount(NULL); pos++)
		{
			UploadTransferBT* transfer = (UploadTransferBT*)m_Uploads.Get(pos);

			if(transfer)
			{
				UINT32 idx;

				for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
				{
					BTClientConnector *client = m_BTClientConnectors.Get(idx);

					if(client)
					{
						if(client->m_GUID == transfer->m_pSHA1)	// same checksum?
						{
							if(
								client->m_downloadtransfer != NULL &&
								client->m_download != NULL &&
								client->m_downloadtransfer->m_interested &&
								client->m_download->IsUnchokable(transfer, FALSE) &&
								best_peers.Find(transfer) == -1)	// viable peer found
							{
								// force update of transfer speed
								client->m_downloadtransfer->m_transferSpeed.AddPoint(0);

								double rate = client->m_downloadtransfer->m_transferSpeed.GetKbps();

//								DEBUGTRACE_CHOKE(UNI_L("TRANSFER RATE FOR: %s, "), (uni_char *)transfer->m_address);
//								DEBUGTRACE_CHOKE(UNI_L("%.2f\n"), (float)rate);
								// filter out really slow peers
								if(rate > 3)
								{
									BTDownload::UpdateLargestValueFirstSort(rate, &bests, transfer, &best_peers, start_pos);
								}
							}
#ifdef _DEBUG
/*
							else if(client->m_downloadtransfer)
							{
								// force update of transfer speed
								client->m_downloadtransfer->m_transferSpeed.AddPoint(0);
								double rate = client->m_downloadtransfer->m_transferSpeed.GetKbps();

//								if(client->m_downloadtransfer->m_has_complete_file == FALSE)
								if(rate > 0)
								{
//									DEBUGTRACE_CHOKE(UNI_L("NOT COUNTED TRANSFER RATE FOR: %s, "), (uni_char *)transfer->m_address);
//									DEBUGTRACE_CHOKE(UNI_L("%.2f, seed: "), (float)rate);
//									DEBUGTRACE_CHOKE(UNI_L("%d\n"), client->m_downloadtransfer->m_has_complete_file ? 1 : 0);
								}
							}
*/
#endif
							break;
						}
					}
				}
			}
		}
//		DEBUGTRACE_CHOKE(UNI_L("BEST PEERS COUNT: %d\n"), best_peers.GetCount());

		if(force_refresh)
		{
			  //make space for new optimistic unchokes
			while(best_peers.GetCount() > max_to_unchoke - max_optimistic)
			{
				best_peers.Remove(best_peers.GetCount() - 1);
			}
		}
		//if we still have remaining slots
		while(best_peers.GetCount() < max_to_unchoke)
		{
			UploadTransferBT *transfer = GetNextOptimisticPeer(TRUE, TRUE, TRUE);
			if(transfer == NULL)
			{
				break;
			}
			if(best_peers.Find(transfer) == -1)
			{
				best_peers.AddIfNotExists(transfer);
				transfer->SetOptimisticUnchoke(TRUE);
			}
			else
			{
				//we're here because the given optimistic peer is already "best", but is choked still,
				//which means it will continually get picked by the getNextOptimisticPeer() method,
				//and we'll loop forever if there are no other peers to choose from
				BTPacket *packet = transfer->SetChoke(FALSE); //send unchoke immediately, so it won't get picked optimistically anymore
				if(packet != NULL)
				{
					UINT32 idx;
					for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
					{
						BTClientConnector *client = m_BTClientConnectors.Get(idx);
						if(client)
						{
							if(client->m_upload == transfer)
							{
								DEBUGTRACE_CHOKE(UNI_L("UL SetChoke on: %s, "), (uni_char *)transfer->m_address);
								DEBUGTRACE8_CHOKE(UNI_L("interested: %d, "), transfer->m_interested);
								DEBUGTRACE8_CHOKE(UNI_L("choke: %d\n"), 0);
								client->Send(packet, FALSE);
								break;
							}
						}
					}
					packet->Release();
				}
			}
		}
		OpP2PVector<UploadTransferBT> removes;

		// update chokes
		for(pos = 0; pos < m_unchokes.GetCount(); pos++)
		{
			UploadTransferBT *transfer = m_unchokes.Get(pos);

			if(transfer != NULL)
			{
				if(best_peers.Find(transfer) == -1)	// should be choked
				{
					//but there are still slots needed (no optimistics avail), so don't bother choking them
					if(best_peers.GetCount() < max_to_unchoke)
					{
						best_peers.AddIfNotExists(transfer);
					}
					else
					{
						m_chokes.AddIfNotExists(transfer);
						removes.AddIfNotExists(transfer);
					}
				}
			}
		}
		for(pos = 0; pos < removes.GetCount(); pos++)
		{
			UploadTransferBT *transfer = removes.Get(pos);

			if(transfer != NULL)
			{
				INT32 idx = m_unchokes.Find(transfer);

				if(idx != -1)
				{
					m_unchokes.Remove(idx);
				}
			}
		}
		// update unchokes
		for(pos = 0; pos < best_peers.GetCount(); pos++)
		{
			UploadTransferBT *transfer = best_peers.Get(pos);

			if(transfer != NULL)
			{
				m_unchokes.AddIfNotExists(transfer);
			}
		}
	}
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTDownload::ChokeTorrent()"
#endif

void BTDownload::ChokeTorrent(time_t tNow)
{
	UINT32 pos;
	UINT32 max_to_unchoke = MAXUPLOADS;
	OpVector<UploadTransferBT> peers_to_choke;
	OpVector<UploadTransferBT> peers_to_unchoke;

	PROFILE(FALSE);

	if(((tNow - m_torrentChoke) % UNCHOKE_DELAY_10) == 0)
	{
		BOOL refresh = (tNow - m_torrentChokeRefresh) > UNCHOKE_DELAY_30;

		m_torrentChoke = tNow;
		if(refresh)
		{
			m_torrentChokeRefresh = tNow;
//			RefreshInterest();
		}

		m_chokes.Clear();
		m_unchokes.Clear();

		CalculateUnchokes(max_to_unchoke, refresh);

		GetChokes(&peers_to_choke);
		GetUnchokes(&peers_to_unchoke);

		for(pos = 0; pos < peers_to_unchoke.GetCount(); pos++)
		{
			UploadTransferBT *transfer = peers_to_unchoke.Get(pos);

			INT32 pos2 = peers_to_choke.Find(transfer);
			if(pos2 > -1)
			{
				peers_to_choke.Remove(pos2);
			}

			if(transfer && transfer->IsChoked())
			{
				BTPacket *packet = transfer->SetChoke(TRUE);
				if(packet != NULL)
				{
					UINT32 idx;

					for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
					{
						BTClientConnector *client = m_BTClientConnectors.Get(idx);
						if(client)
						{
							if(client->m_upload == transfer)
							{
								DEBUGTRACE_CHOKE(UNI_L("UL SetChoke(2) on: %s, "), (uni_char *)transfer->m_address);
								DEBUGTRACE8_CHOKE(UNI_L("interested: %d, "), transfer->m_interested);
								DEBUGTRACE8_CHOKE(UNI_L("choke: %d\n"), 1);
								client->Send(packet, FALSE);
								break;
							}
						}
					}
					packet->Release();
				}
			}
		}
		for(pos = 0; pos < peers_to_choke.GetCount(); pos++)
		{
			UploadTransferBT *transfer = peers_to_choke.Get(pos);

			if(transfer && !transfer->IsChoked())
			{
				BTPacket *packet = transfer->SetChoke(TRUE);
				if(packet != NULL)
				{
					UINT32 idx;

					for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
					{
						BTClientConnector *client = m_BTClientConnectors.Get(idx);
						if(client)
						{
							if(client->m_upload == transfer)
							{
								DEBUGTRACE_CHOKE(UNI_L("UL SetChoke(1) on: %s, "), (uni_char *)transfer->m_address);
								DEBUGTRACE8_CHOKE(UNI_L("interested: %d, "), transfer->m_interested);
								DEBUGTRACE8_CHOKE(UNI_L("choke: %d\n"), 1);
								client->Send(packet, FALSE);
								break;
							}
						}
					}
					packet->Release();
				}
			}
		}
	}
	else if(((tNow - m_torrentChoke) % UNCHOKE_DELAY_1) == 0) // do a quick unchoke check every 1 sec
	{
		OpVector<UploadTransferBT> peers_to_unchoke;

		m_chokes.Clear();
		m_unchokes.Clear();

		GetImmediateUnchokes(max_to_unchoke, &peers_to_unchoke);

		//do unchokes
		for(pos = 0; pos < peers_to_unchoke.GetCount(); pos++)
		{
			UploadTransferBT *transfer = peers_to_unchoke.Get(pos);

			if(transfer && transfer->IsChoked())
			{
				BTPacket *packet = transfer->SetChoke(FALSE);
				if(packet != NULL)
				{
					UINT32 idx;

					for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
					{
						BTClientConnector *client = m_BTClientConnectors.Get(idx);
						if(client)
						{
							if(client->m_upload == transfer)
							{
								DEBUGTRACE_CHOKE(UNI_L("UL SetChoke(3) on: %s, "), (uni_char *)transfer->m_address);
								DEBUGTRACE8_CHOKE(UNI_L("interested: %d, "), transfer->m_interested);
								DEBUGTRACE8_CHOKE(UNI_L("choke: %d\n"), 0);
								client->Send(packet, FALSE);
								break;
							}
						}
					}
					packet->Release();
				}
			}
		}
	}

}

BOOL BTDownload::FindMoreSources()
{
	time_t currenttime = op_time(NULL);

	if ( m_file != NULL && m_tracker_state > BTTracker::TrackerStart)
	{
		OP_ASSERT( m_torrent.GetImpl()->IsAvailable() );

		if(currenttime - m_torrentSources > 10*60)
		{
			m_torrentTracker = currenttime + 60*10;
			m_torrentSources = currenttime;

			DEBUGTRACE_TRACKER(UNI_L("**** SENDING TRACKER REQUEST FOR MORE SOURCES ***\n"), 0);

			m_torrent.GetImpl()->GetTracker()->SendUpdate( this );
			m_torrent.GetImpl()->GetTracker()->SendScrape( this );
			return TRUE;
		}
	}

	return FALSE;
}

void BTDownload::CloseTorrent()
{
	if (m_tracker_state >= BTTracker::TrackerStartSent)
	{
		m_torrent.GetImpl()->GetTracker()->SendStopped(this, TRUE);
	}
	m_tracker_state = BTTracker::TrackerNone;
	m_torrentTracker = 0;

	g_BTServerConnector->StopConnections(this);
}

BTPacket* BTDownload::CreateBitfieldPacket()
{
	OP_ASSERT( m_torrent.GetImpl()->IsAvailable() );

	BTPacket* packet = BTPacket::New( BT_PACKET_BITFIELD );
	if (!packet)
		return NULL;

	int count = 0;

	DEBUGTRACE8_UP(UNI_L("%s: "), UNI_L("BITFIELD"));

	for(UINT64 block = 0 ; block < m_torrentBlockSize; )
	{
		byte nByte = 0;

		for(INT32 bit = 7 ; bit >= 0 && block < m_torrentBlockSize ; bit--, block++ )
		{
			if(m_torrentBlock[ block ] == TS_TRUE)
			{
				nByte |= ( 1 << bit );
				count++;
//				DEBUGTRACE8_UP("%d", 1);
			}
			else
			{
//				DEBUGTRACE8_UP("%d", 0);
			}
		}
		packet->WriteByte( nByte );
	}
	DEBUGTRACE8_UP(UNI_L(" (%d blocks)\n"), m_torrentBlockSize);

	// let's always send the bitfield packet, even if we don't have any data
//	if( count > 0 )
	{
		return packet;
	}
/*	else
	{
		packet->Release();

		return NULL;
	}
*/
}


float BTDownload::GetRatio() const
{
	if ( m_torrentUploaded == 0 || m_torrentDownloaded == 0 ) return 0;
	return (float)((INT64)(m_torrentUploaded / m_torrentDownloaded));
}

// 	//Generate Peer ID
BOOL BTDownload::GenerateTorrentDownloadID()
{
	// generate a unique peer id of 20 bytes
	time_t tmp_time = op_time(NULL);

    UINT time_hash = (UINT)tmp_time;	// Seconds since 1970
    UINT ext_time_hash;					// milliseconds

#if defined(WIN32) && !defined(WIN_CE)
    ext_time_hash = (UINT)(g_op_time_info->GetTimeUTC())&0x03FF;
#else
    op_srand(time_hash);
    ext_time_hash = (UINT)(rand()&0x03FF); //Just generate a random millisecond
#endif

	char base36_time[8];
    char tmp_base36;
	int i;
    for (i=0; i<8; i++)
    {
        if (i==2)
			ext_time_hash = time_hash;

		tmp_base36 = ext_time_hash%36;
		ext_time_hash/=36;

        base36_time[7-i] = (tmp_base36<26)? 'a'+tmp_base36 : '0'+(tmp_base36-26);
    }

    OpString8 md5;

    OpMisc::CalculateMD5Checksum(base36_time, 8, md5);

	sprintf((char *)m_peerid.n, "%.6s", OPERA_VERSION_STRING_BITTORRENT);

	size_t buildlen = sizeof(VER_BUILD_NUMBER_STR)-1;
	buildlen = min(buildlen, size_t(6));

	memcpy(&m_peerid.n[6 - buildlen], VER_BUILD_NUMBER_STR, buildlen);
	memcpy(&m_peerid.n[6], md5.CStr(), 14);

	return TRUE;
}

BOOL BTDownload::AddSourceBT(SHAStruct* pGUID, OpString& pAddress, WORD nPort, BOOL from_pex, OpString& pex_address)
{
	DownloadSource *source = OP_NEW(DownloadSource, ((DownloadBase *)this, pGUID, pAddress, nPort));
	if (!source)
		return FALSE;

	if(from_pex)
	{
		source->m_pex_peer = TRUE;
		RETURN_VALUE_IF_ERROR(source->m_pex_added_by.Set(pex_address), FALSE);
	}
	return AddSourceInternal(source);
}

//////////////////////////////////////////////////////////////////////
// CDownload state checks

void BTDownload::Pause()
{
	if(/*m_complete || */m_paused) return;

	StopTrying();

	m_paused = TRUE;
}

void BTDownload::Resume()
{
	m_paused = FALSE;
	SetStartTimer();
}

void BTDownload::Remove(BOOL bDelete)
{
	Pause();

	g_Downloads->Remove( this );
}

//////////////////////////////////////////////////////////////////////
// Stop trying

void BTDownload::StopTrying()
{
//	if(m_complete) return;
	m_began = 0;

	g_Transfers->StopTimer(this);

	// send stop to all transfer socket wrapper classes
	g_Transfers->SendStop(&m_pBTH);

	// close incoming connections, send stopped to tracker
	CloseTorrent();

	m_BTClientConnectors.Clear();

	// close upload transfer classes
//	CloseTorrentUploads();
	memset(m_peerid.n, 0, 20);

	// close download transfer classes
//	CloseTransfers();

	// close files
	CloseFile();

	if(m_verifyfile != NULL)
	{
		m_verifyfile->Release(m_create_files);	// will close the files
		m_verifyfile = NULL;
		ResetFileCheckState();
	}
	if(m_verify_block_file)
	{
		m_verify_block_file->Release(FALSE);
		m_verify_block_file = NULL;
	}
	SetModified();
#ifdef HAVE_LTH_MALLOC
	FILE *file = fopen("c:\\bt-debug-log.log", "w+");
	if(file)
	{
		summarize_allocation_sites(file);
		fclose(file);
	}
#endif // HAVE_LTH_MALLOC

#if defined(DEBUG_BT_RESOURCE_TRACKING)
	for (UINT32 i = 0; i < m_files.GetCount(); ++i)
	{
		BT_RESOURCE_REMOVE(m_files.Get(i));
	}
#endif // DEBUG_BT_RESOURCE_TRACKING
	m_files.DeleteAll();

	CloseMetaFile();

	BT_RESOURCE_CHECKPOINT(TRUE);
}

//////////////////////////////////////////////////////////////////////
// consider starting more transfers

BOOL BTDownload::StartTransfersIfNeeded(time_t tNow)
{
	if(tNow == 0)
	{
		tNow = op_time(NULL);
	}
	m_transferstart = tNow;

	int transfers = GetTransferCount(dtsCountAll);

	if(transfers == 0 && !(m_seeding_stage & PostSeedingDone) && !m_complete)
	{
		BOOL transfer_failed;

		FileDownloadBegin(transfer_failed);
	}

	if(transfers < BT_MAX_OUTGOING_CONNECTIONS)
	{
		if(StartNewTransfer(tNow))
		{
			return TRUE;
		}
		if(m_need_more_sources)
		{
			if(FindMoreSources())
			{
				m_need_more_sources = FALSE;
			}
		}
	}
#ifdef _DEBUG
	else
	{
		DEBUGTRACE8(UNI_L("*** MAXIMUM number of connections reached\n"), 0);
	}
#endif
	return FALSE;
}

// method to send pause/continue to the various BTClientConnector classes
void BTDownload::RestrictUploadBandwidth()
{
	UINT32 idx;

	if(g_Transfers->IsAutomaticUploadRestrictionActive())
	{
#ifdef BT_UPLOAD_DESTRICTION
		for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
		{
			BTClientConnector *client = m_BTClientConnectors.Get(idx);

			if(client != NULL && client->IsOnline() && !client->IsClosing())
			{
				client->SetUploadStage(client->IsUploadPaused() ? BTClientConnector::StageDownload : BTClientConnector::StagePaused);
			}
		}
#endif
	}
	else
	{
#ifdef BT_UPLOAD_FIXED_RESTRICTION
		OpTransferItem* item = GetTransferItem();

		OP_ASSERT(item != 0);
		if(item != 0)
		{
			TransferItem* transfer_item = (TransferItem *)(item);

			transfer_item->SetCalculateKbps(TRUE);

			g_Transfers->UpdateTransferSpeed((UINTPTR)this, transfer_item->GetKbps(), transfer_item->GetKbpsUpload());
			g_Transfers->CalculateTransferRates();
		}
		BOOL uploadthrottle = g_Transfers->ThrottleUpload();
		BOOL downloadthrottle = g_Transfers->ThrottleDownload();

		for(idx = 0; idx < m_BTClientConnectors.GetCount(); idx++)
		{
			BTClientConnector *client = m_BTClientConnectors.Get(idx);

			if(client != NULL && client->IsOnline() && !client->IsClosing())
			{
				if(uploadthrottle)
				{
					client->SetUploadStage(BTClientConnector::StagePaused);
				}
				else
				{
					client->SetUploadStage(BTClientConnector::StageDownload);
				}
				if(downloadthrottle)
				{
					client->SetDownloadStage(BTClientConnector::StagePaused);
				}
				else
				{
					client->SetDownloadStage(BTClientConnector::StageDownload);
				}
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////
// run handler

void BTDownload::OnRun()
{
	time_t tNow = op_time(NULL);

	if(!m_paused)
	{
		if(IsTrying())
		{
			if(m_seeding_stage & NotSeeding && m_checking_file_queued == FALSE)
			{
				if(RunTorrent(tNow))
				{
					if(GetVolumeComplete() == m_size && !m_complete)
					{
// || m_seeding_stage & PreSeedingDone
						if(!(m_seeding_stage & PostSeeding || m_seeding_stage & PostSeedingDone))
						{
							m_seeding_stage = PostSeeding;
							m_file->Flush();
							CheckExistingFile();
						}
						if(!(m_seeding_stage & PostSeeding))
						{
//							m_torrentSuccess = m_torrentBlockSize;
							OnDownloaded();
						}
					}
					if(m_file != NULL)
					{
/*
						if(m_initial_ramp_up)
						{
							int cnt;

							for(cnt = 0; cnt < 20; cnt++)
							{
								tNow = op_time(NULL);
								StartTransfersIfNeeded( tNow );
							}
							m_initial_ramp_up = FALSE;
						}
*/
						RestrictUploadBandwidth();

						StartTransfersIfNeeded( tNow );
					}
				}
			}
		}
	}
}
/*
btblock_compare( (void *) elem1, (void *) elem2 );

The routine must compare the elements, then return one of the following values:

Return Value Description
< 0 elem1 less than elem2
0 elem1 equivalent to elem2
> 0 elem1 greater than elem2

*/
int btblock_compare( const void *elem1, const void *elem2 )
{
	BTBlock *block1 = (BTBlock *)elem1;
	BTBlock *block2 = (BTBlock *)elem2;

	if(block1->block_count < block2->block_count)
	{
		return -1;
	}
	else if(block1->block_count == block2->block_count)
	{
		return 0;
	}
	return 1;
}

void BTDownload::SortAvailableBlockCount()
{
	if(m_available_block_count)
	{
		qsort((void *)&m_available_block_count[0], (size_t)m_torrentBlockSize, sizeof(BTBlock), btblock_compare);
	}
}

/*
Used to keep a count of the total number of occurrences in the swarm on each block, so
this array can be used to find the rarest blocks to requests first.
*/
OP_STATUS BTDownload::UpdateAvailableBlockCount(UINT32 block, BOOL increment)
{
	block++;	// convert to 1-based

	if(m_available_block_count == NULL)
	{
		if(m_torrentBlockSize == 0)
		{
			return OpStatus::ERR_OUT_OF_RANGE;
		}
		m_available_block_count = OP_NEWA(BTBlock, m_torrentBlockSize);
		if(!m_available_block_count)
			return OpStatus::ERR_NO_MEMORY;

		memset(m_available_block_count, 0, m_torrentBlockSize * sizeof(BTBlock));

		if(increment == FALSE)
		{
			// can't decrement a empty array
			return OpStatus::OK;
		}
	}
	UINT32 cnt;
	BTBlock *match = NULL;

	for(cnt = 0; cnt < m_torrentBlockSize; cnt++)
	{
		if(m_available_block_count[cnt].block == block)
		{
			match = &m_available_block_count[cnt];
			break;
		}
	}
	if(match == NULL)
	{
		for(cnt = 0; cnt < m_torrentBlockSize; cnt++)
		{
			if(m_available_block_count[cnt].block == 0)
			{
				match = &m_available_block_count[cnt];
				break;
			}
		}
	}
	if(match)
	{
		if(increment)
		{
			match->block_count++;
		}
		else if (match->block_count > 0)
		{
			match->block_count--;
        }

		match->block = block;
	}

//	DEBUGTRACE_BLOCK(UNI_L("BLOCK COUNT for %d: "), block);
//	DEBUGTRACE_BLOCK(UNI_L("%d\n"), match->block_count);

	return OpStatus::OK;
}

void BTDownload::GetRarestBlocks(BTBlock *dest_blocks, UINT32 block_count)
{
	UINT32 cnt, offset;

	memset(dest_blocks, 0, block_count * sizeof(BTBlock));

	for(cnt = 0, offset = 0; offset < block_count && cnt < m_torrentBlockSize; )
	{
		// if no-one has a block, no need to add it to the rare block list
		if(m_available_block_count[cnt].block_count)
		{
			dest_blocks[offset].block = m_available_block_count[cnt].block - 1;	// convert to 0-based array
			dest_blocks[offset].block_count = m_available_block_count[cnt].block_count;
			offset++;
		}
		cnt++;
	}
}

void BTDownload::OnDownloaded()
{
	if(m_complete == FALSE && m_torrentBlockSize > 0 && m_torrentSuccess >= m_torrentBlockSize )
	{
		if(m_downloaded != 0)
		{
			m_torrent.GetImpl()->GetTracker()->SendCompleted( this );
			m_torrent.GetImpl()->GetTracker()->SendScrape( this );
		}

		DownloadBase::OnDownloaded();

		BOOL transfer_failed;

		CloseFile();
		OpenFile();

		FileDownloadSharing(transfer_failed);
	}
}

////////////////////////////////////////////////////
////
//// The code handling a particular transfer for BT
////
////////////////////////////////////////////////////


DownloadTransferBT::DownloadTransferBT(DownloadSource* source, BTClientConnector *client)
:	DownloadTransfer(source, PROTOCOL_BT, client),
	m_client(client),
	m_choked(TRUE),
	m_interested(FALSE),
	m_available(NULL),
	m_requested(NULL),
	m_requestedCount(0),
	m_runthrottle(0),
	m_sourcerequest(op_time(NULL)),
	m_rareblocks(NULL),
	m_snubbed(FALSE),
	m_sending(false),
	m_close_requested(false),
	m_close_arg(TS_UNKNOWN)
{
	OP_ASSERT( m_download->m_bBTH );
//	OP_ASSERT( m_download->m_size != SIZE_UNKNOWN ); // doesn't compile

//	m_useragent.Append("BitTorrent");
	m_state = client ? dtsConnecting : dtsNull;

//	if(client) client->AddRef();

	BT_RESOURCE_ADD("DownloadTransferBT", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "DownloadTransferBT::~DownloadTransferBT()"
#endif

DownloadTransferBT::~DownloadTransferBT()
{
	OP_ASSERT(!m_sending);

	OP_ASSERT( m_client == NULL );
	m_requested->DeleteChain();

	OP_DELETEA(m_available);
	OP_DELETEA(m_rareblocks);

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// DownloadTransferBT initiate

BOOL DownloadTransferBT::Initiate()
{
	OP_ASSERT( m_client == NULL );
	OP_ASSERT( m_state == dtsNull );

	m_client = OP_NEW(BTClientConnector, ());

	if (!m_client || !m_client->GetTransfer()) // OOM
	{
		OP_DELETE(m_client);
		m_client = NULL;
		Close( TS_FALSE );
		return FALSE;
	}

//	m_client->AddRef();
	if((m_client->Connect(this)) != OpStatus::OK)
	{
		OP_DELETE(m_client);
		m_client = NULL;

		Close( TS_FALSE );
		return FALSE;
	}
	SetState( dtsConnecting );
//	m_connectedTime	= op_time(NULL);

	// FIX: Look into why transfer is NULL in some cases

	if(m_client->GetTransfer() == NULL)
	{
		return FALSE;
	}
	m_client->AddRef();
	m_host.Set(m_client->GetTransfer()->m_host);
	m_address.Set(m_client->GetTransfer()->m_address);
	m_client->Release();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// close

void DownloadTransferBT::Close(TRISTATE bKeepSource)
{
	if (m_sending)
	{
		// delay closing until stack unwinds to the point where it is safe (DSK-340876)
		m_close_requested = true;
		m_close_arg = bKeepSource;
		return;
	}

	if(m_client != NULL && ((BTDownload *)m_download)->ClientConnectors()->Check(m_client) == TRUE)
	{
//		m_source->Remove(FALSE, FALSE);
		m_client->m_downloadtransfer = NULL;

		if(m_client->IsOnline())
		{
//			m_client->Release();
//			m_client->Send(BTPacket::New(BT_PACKET_NOT_INTERESTED));
		}
		else
		{
//			m_client->Release();
			m_client->Close();
		}
		m_client = NULL;
	}
	DownloadTransfer::Close( bKeepSource );
}

void DownloadTransferBT::StartClosing()
{
	if (m_client)
	{
		if (m_sending)
		{
			m_client->Release();
		}
		m_client = NULL;
	}
}

//////////////////////////////////////////////////////////////////////
// bandwidth control

void DownloadTransferBT::Boost()
{
	if(m_client == NULL) return;
	m_client->GetTransfer()->m_mInput.pLimit = NULL;
}

UINT32 DownloadTransferBT::GetAverageSpeed()
{
	UINT32 speed = GetMeasuredSpeed();
	m_source->m_speed = speed;
	return speed;
}

UINT32 DownloadTransferBT::GetMeasuredSpeed()
{
	if(m_client == NULL) return 0;
//	m_client->Measure();
	return m_client->GetTransfer()->m_mInput.nMeasure;
}

//////////////////////////////////////////////////////////////////////
// send packet helper

void DownloadTransferBT::Send(BTPacket* packet, BOOL release)
{
	OP_ASSERT(m_sending == false && m_close_requested == false);
	m_sending = true;
	m_client->AddRef();
	m_client->Send(packet, release);
	if (m_client) // Send could result in StartClosing (DSK-329271)
	{
		m_client->Release();
	}
	m_sending = false;
}

void DownloadTransferBT::Send(OpByteBuffer *buffer)
{
	OP_ASSERT(m_sending == false && m_close_requested == false);
	m_sending = true;
	m_client->AddRef();
	m_client->Send(buffer);
	if (m_client) // Send could result in StartClosing (DSK-329271)
	{
		m_client->Release();
	}
	m_sending = false;
}

//////////////////////////////////////////////////////////////////////
// connection established event

BOOL DownloadTransferBT::OnConnected()
{
	OP_ASSERT( m_client != NULL );
	OP_ASSERT( m_source != NULL );

	SetState( dtsTorrent );

	m_client->AddRef();
	m_host.Set(m_client->GetTransfer()->m_host);
	m_address.Set(m_client->GetTransfer()->m_address);
	m_client->Release();

	m_source->SetLastSeen();

	// connected for download now

	if(!m_download->PrepareFile())
	{
//		Close( TS_TRUE );
//		return FALSE;
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// bit fields

OP_STATUS DownloadTransferBT::OnBitfield(BTPacket* packet)
{
	UINT32 avail_block_count = 0;
	UINT64 blocksize	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
	UINT32 blockcount	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount();

	m_source->m_available->DeleteChain();
	m_source->m_available = NULL;

	OP_DELETEA(m_available);
	m_available = NULL;

	if(blocksize == 0 || blockcount == 0)
	{
		return OpStatus::OK;
	}
	DEBUGTRACE(UNI_L("*** Received BITFIELD from: %s\n"), (uni_char *)m_address);

	m_available = OP_NEWA(byte, blockcount);

	if (!m_available)
		return OpStatus::ERR_NO_MEMORY;

	memset(m_available, 0, blockcount);

	for(UINT32 block = 0 ; block < blockcount && packet->GetRemaining(); )
	{
		byte nByte = packet->ReadByte();

		for(int nBit = 7 ; nBit >= 0 && block < blockcount ; nBit--, block++ )
		{
			if(nByte & (1 << nBit))
			{
				UINT64 offset = blocksize * block;
				UINT64 length = min(blocksize, m_download->m_size - offset);
				P2PFilePart::AddMerge(&m_source->m_available, offset, length );
				m_available[block] = TRUE;
				avail_block_count++;

				((BTDownload *)m_download)->UpdateAvailableBlockCount(block);
			}
		}
	}
	if(avail_block_count == blockcount)
	{
		m_has_complete_file = TRUE;
	}
	((BTDownload *)m_download)->SortAvailableBlockCount();

	ShowInterest();

	return OpStatus::OK;
}

void DownloadTransferBT::DecrementAvailableBlockCountOnDisconnect()
{
	UINT32 blockcount	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount();

	for(UINT32 block = 0 ; block < blockcount; block++)
	{
		if(m_available && m_available[block])
		{
			((BTDownload *)m_download)->UpdateAvailableBlockCount(block, FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// have block updates

void DownloadTransferBT::SendFinishedBlock(UINT32 block)
{
	if(m_client == NULL || ((BTDownload *)m_download)->ClientConnectors()->Check(m_client) == FALSE)
	{
		return;
	}
	if(!m_client->IsOnline())
	{
		return;
	}
	BTPacket* packet = BTPacket::New( BT_PACKET_HAVE );
	packet->WriteLongBE(block);

	DEBUGTRACE8_HAVE(UNI_L("*** Sending HAVE: %d"), block);
	DEBUGTRACE_HAVE(UNI_L(" to %s\n"), (uni_char *)m_address);

	Send(packet);
	if (m_close_requested) // DSK-340876
	{
		Close(m_close_arg);
	}
}

OP_STATUS DownloadTransferBT::OnHave(BTPacket* packet)
{
	if(packet->GetRemaining() != sizeof(INT32)) return OpStatus::ERR;

	UINT64 blocksize	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
	UINT32 blockcount	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount();
	UINT32 block		= packet->ReadLongBE();

	DEBUGTRACE8_HAVE(UNI_L("Received HAVE %d from "), block);
	DEBUGTRACE_HAVE(UNI_L("%s\n"), (uni_char *)m_address);

	if(block >= blockcount)
	{
		return OpStatus::OK;
	}
	UINT64 offset = blocksize * block;
	UINT64 length = min(blocksize, m_download->m_size - offset);
	P2PFilePart::AddMerge(&m_source->m_available, offset, length );

	if(m_available == NULL)
	{
		m_available = OP_NEWA(byte, blockcount);
		if (!m_available)
			return OpStatus::ERR_NO_MEMORY;

		memset(m_available, 0, blockcount );
	}

	m_available[block] = TRUE;

	((BTDownload *)m_download)->UpdateAvailableBlockCount(block);
	((BTDownload *)m_download)->SortAvailableBlockCount();

	UINT32 cnt;
	UINT32 avail_block_count = 0;

	for(cnt = 0; cnt < blockcount; cnt++)
	{
		if(m_available[cnt] == TRUE)
		{
			avail_block_count++;
		}
	}
	if(avail_block_count == blockcount)
	{
		m_has_complete_file = TRUE;
	}
	ShowInterest();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// interest control

void DownloadTransferBT::ShowInterest()
{
	BOOL interested = FALSE;

	if(m_available == NULL)
	{
		// Never interested if we don't know what they have
	}
	else if(UINT64 blocksize = ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize())
	{
		for(P2PFilePart* fragment = m_download->GetFirstEmptyFragment();
				fragment != NULL;
				fragment = fragment->m_next )
		{
			UINT32 block = (UINT32)( fragment->m_offset / blocksize );
			UINT64 length;

			for(length = fragment->m_length ; ; block++, length -= blocksize)
			{
				if(m_available[block])
				{
					interested = TRUE;
					break;
				}
				if(length <= blocksize) break;
			}
			if(interested) break;
		}
	}
	if(interested != m_interested )
	{
		m_interested = interested;

		DEBUGTRACE8_CHOKE(UNI_L("*** sending %sINTERESTED"), interested ? UNI_L("") : UNI_L("NOT"));
		DEBUGTRACE_CHOKE(UNI_L(" to %s\n"), (uni_char *)m_address);

		Send(BTPacket::New( interested ? BT_PACKET_INTERESTED : BT_PACKET_NOT_INTERESTED));
		if (m_close_requested) // DSK-340876
		{
			Close(m_close_arg);
			return;
		}

		if(!interested)
		{
			m_requested->DeleteChain();
			m_requested = NULL;
			m_requestedCount = 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// choking

OP_STATUS DownloadTransferBT::OnChoked(BTPacket* /*packet*/)
{
	if(m_choked ) return OpStatus::OK;
	m_choked = TRUE;

	SetState( dtsTorrent );

	for(P2PFilePart* fragment = m_requested; fragment != NULL ; fragment = fragment->m_next )
	{
		BTPacket* packet = BTPacket::New( BT_PACKET_CANCEL );
		if (packet)
		{
			OpByteBuffer *queue = OP_NEW(OpByteBuffer, ());
			if(queue)
			{
				packet->WriteLongBE((UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((UINT32)(fragment->m_offset % ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((DWORD)fragment->m_length);

				DEBUGTRACE_UP(UNI_L("*** sending CANCEL %d"), (UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				DEBUGTRACE_UP(UNI_L(" to %s\n"), (uni_char *)m_address);

				packet->ToBuffer(queue, TRUE);
				packet->Release();

				Send(queue);
				if (m_close_requested) // DSK-340876
				{
					Close(m_close_arg);
					return OpStatus::ERR;
				}
			}
			else
				packet->Release();
		}
	}
	m_requested->DeleteChain();
	m_requested = NULL;
	m_requestedCount = 0;

	DEBUGTRACE_CHOKE(UNI_L("CHOKED by: %s\n"), (uni_char *)m_address);

	return OpStatus::OK;
}

OP_STATUS DownloadTransferBT::OnSnubbed()
{
	SetState( dtsTorrent );

	for(P2PFilePart* fragment = m_requested; fragment != NULL ; fragment = fragment->m_next )
	{
		BTPacket* packet = BTPacket::New( BT_PACKET_CANCEL );
		if (packet)
		{
			OpByteBuffer *queue = OP_NEW(OpByteBuffer, ());
			if(queue)
			{
				packet->WriteLongBE((UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((UINT32)(fragment->m_offset % ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((DWORD)fragment->m_length);

				DEBUGTRACE_UP(UNI_L("*** sending CANCEL %d"), (UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				DEBUGTRACE_UP(UNI_L(" to %s\n"), (uni_char *)m_address);

				packet->ToBuffer(queue, TRUE);
				packet->Release();

				Send(queue);
				if (m_close_requested) // DSK-340876
				{
					Close(m_close_arg);
					return OpStatus::ERR;
				}
			}
			else
				packet->Release();
		}
	}
	m_requested->DeleteChain();
	m_requested = NULL;
	m_requestedCount = 0;

	DEBUGTRACE_CHOKE(UNI_L("SNUBBED by: %s\n"), (uni_char *)m_address);

	return OpStatus::OK;
}

OP_STATUS DownloadTransferBT::OnUnchoked(BTPacket* packet)
{
	m_choked = FALSE;
	SetState( dtsTorrent );

	m_requested->DeleteChain();
	m_requested = NULL;
	m_requestedCount = 0;

	DEBUGTRACE(UNI_L("UNCHOKED by: %s\n"), (uni_char *)m_address);

	return SendRequests();
}

//////////////////////////////////////////////////////////////////////
// request pipe

OP_STATUS DownloadTransferBT::SendRequests()
{
	OP_ASSERT( m_state == dtsTorrent || m_state == dtsRequesting || m_state == dtsDownloading );

	if(m_choked || !m_interested )
	{
		if(m_requestedCount == 0 ) SetState( dtsTorrent );
		return OpStatus::OK;
	}
	if(m_requestedCount >= 10)	// hard coded pipe
	{
		if(m_state != dtsDownloading ) SetState( dtsRequesting );
		return OpStatus::OK;
	}
	if(m_download == NULL || m_client == NULL)
	{
		return OpStatus::ERR;
	}

	UINT32 blocksize = ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
	OP_ASSERT(blocksize != 0);
	if(blocksize == 0) return OpStatus::OK;

	P2PFilePart* possible = m_download->GetFirstEmptyFragment()->CreateCopy();
	OpByteBuffer *queue = OP_NEW(OpByteBuffer, ());
	if (!queue)
		return OpStatus::ERR_NO_MEMORY;

	if(!((BTDownload *)m_download)->m_torrentEndgame )
	{
		for(DownloadTransfer* transfer = m_download->GetFirstTransfer();
				transfer && possible;
				transfer = transfer->m_dlNext )
		{
			transfer->SubtractRequested(&possible);
		}
	}
	while(m_requestedCount < (int) 10)	// hard coded max pipe
	{
		UINT64 offset, length;

		if(SelectFragment(possible, &offset, &length) == OpStatus::OK)
		{
			ChunkifyRequest(&offset, &length, BT_REQUEST_SIZE);	// hard coded request size

			P2PFilePart::Subtract(&possible, offset, length);

			P2PFilePart* request = P2PFilePart::New(NULL, m_requested, offset, length );

			if (request)
			{
				if(m_requested != NULL )
				{
					m_requested->m_previous = request;
				}
				m_requested = request;
				m_requestedCount++;
			}
			else
				break;

/*
			int type = (m_downloadedCount == 0 || (offset % blocksize ) == 0 )
						? MSG_DEFAULT : MSG_DEBUG;
*/
			if(m_client->GetTransfer()->m_upload)
			{
				DEBUGTRACE8_UP(UNI_L("UL: Requesting piece, offset: %lld"), offset);
				DEBUGTRACE8_UP(UNI_L(", %lld from: "), offset + length - 1);
				DEBUGTRACE_UP(UNI_L("%s\n"), m_address.CStr());

				DEBUGTRACE8_UP(UNI_L("**** REMAINING: %lld"), m_download->GetVolumeRemaining());
				DEBUGTRACE8_UP(UNI_L(", DOWNLOADED: %lld"), m_download->GetVolumeComplete());
				DEBUGTRACE8_UP(UNI_L(", UPLOADED: %lld\n"), m_download->GetVolumeUploaded());
			}
			else
			{
				DEBUGTRACE8_UP(UNI_L("Requesting piece, offset: %lld"), offset);
				DEBUGTRACE8_UP(UNI_L(", %lld from: "), offset + length - 1);
				DEBUGTRACE_UP(UNI_L("%s\n"), m_address.CStr());

				DEBUGTRACE8_UP(UNI_L("**** REMAINING: %lld"), m_download->GetVolumeRemaining());
				DEBUGTRACE8_UP(UNI_L(", DOWNLOADED: %lld"), m_download->GetVolumeComplete());
				DEBUGTRACE8_UP(UNI_L(", UPLOADED: %lld\n"), m_download->GetVolumeUploaded());
			}

#ifdef _DEBUG
			UINT32 ndBlock1 = (DWORD)( offset / blocksize );
			UINT32 ndBlock2 = (DWORD)( ( offset + length - 1 ) / blocksize );
			OP_ASSERT( ndBlock1 < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());
			OP_ASSERT( ndBlock1 == ndBlock2 );
			OP_ASSERT( length <= blocksize );
#endif

			BTPacket* packet = BTPacket::New( BT_PACKET_REQUEST );
			if(packet)
			{
				packet->WriteLongBE((UINT32)(offset / blocksize));
				packet->WriteLongBE((UINT32)(offset % blocksize));
				packet->WriteLongBE((UINT32)length);
				packet->ToBuffer(queue, TRUE);

				m_sourcerequest = op_time(NULL);

				packet->Release();
			}
		}
		else
		{
			break;
		}
	}
	if(possible == NULL && ((BTDownload *)m_download)->m_torrentEndgame == FALSE )
	{
		DEBUGTRACE8(UNI_L("ENTERING ENDGAME, remaining: %d\n"), m_download->GetVolumeRemaining());
		((BTDownload *)m_download)->m_torrentEndgame = TRUE;
	}
	possible->DeleteChain();

	if(queue->DataSize())
	{
		Send(queue);
		if (m_close_requested) // DSK-340876
		{
			Close(m_close_arg);
			return OpStatus::ERR;
		}
	}
	else
	{
		OP_DELETE(queue);
	}
	if(m_requestedCount > 0 && m_state != dtsDownloading)
	{
		SetState(dtsRequesting);
	}
	if(m_requestedCount == 0)
	{
		SetState(dtsTorrent);
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// fragment selection

OP_STATUS DownloadTransferBT::SelectFragment(P2PFilePart* possible, UINT64 * offset, UINT64* length)
{
	OP_ASSERT( offset != NULL && length != NULL );

	if(possible == NULL)
	{
		return OpStatus::ERR;
	}
	UINT32 blocksize = ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
	P2PFilePart* complete = NULL;
	UINT32 block;

	OP_ASSERT(blocksize != 0);

	for ( ; possible != NULL; possible = possible->m_next)
	{
		if(possible->m_offset % blocksize)
		{
			// the start of a block is complete, but part is missing

			block = (UINT32)(possible->m_offset / blocksize);
			OP_ASSERT(block < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());

			if(m_available == NULL || m_available[block])
			{
				*offset = possible->m_offset;
				*length = min( blocksize * ((UINT64)block+1) - *offset, possible->m_length );
				OP_ASSERT(*length <= blocksize);

				complete->DeleteChain();
				return OpStatus::OK;
			}
		}
		else if((possible->m_length % blocksize) &&
				(possible->m_offset + possible->m_length < m_download->m_size))
		{
			// the end of a block is complete, but part is missing

			block = (UINT32)((possible->m_offset + possible->m_length ) / blocksize );
			OP_ASSERT(block < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());

			if(m_available == NULL || m_available[block])
			{
				*offset = blocksize * (UINT64)block;
				*length = possible->m_offset + possible->m_length - *offset;
				OP_ASSERT( *length <= blocksize );

				complete->DeleteChain();
				return OpStatus::OK;
			}
		}
		else
		{
			// this fragment contains one or more aligned empty blocks

			block = (UINT32)(possible->m_offset / blocksize );
			*length = possible->m_length;
			OP_ASSERT( *length != 0 );

			for( ;; block++, *length -= blocksize )
			{
				OP_ASSERT(block < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());

				if(m_available == NULL || m_available[block])
				{
					P2PFilePart* new_complete = P2PFilePart::New(NULL, complete, possible->m_offset, possible->m_length);
//					P2PFilePart* new_complete = P2PFilePart::New(NULL, complete, (UINT64)block, 0);
					if (!new_complete)
					{
						complete->DeleteChain();
						return OpStatus::ERR_NO_MEMORY;
					}
					complete = new_complete;
				}
				if(*length <= blocksize)
				{
					break;
				}
			}
		}
	}
	// if we came this far, we didn't find any half-finished blocks to finish downloading on, so
	// we need to check the rarest blocks in the swarm and see if this peer has any of them

	// first make sure the first 10 blocks are random to get decent speed in the beginning
	if(m_downloaded > (blocksize * 10))
	{
		if(m_rareblocks == NULL)
		{
			m_rareblocks = OP_NEWA(BTBlock, ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());
			if(m_rareblocks == NULL)
			{
				complete->DeleteChain();
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		((BTDownload *)m_download)->GetRarestBlocks((BTBlock *)m_rareblocks, ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());

		UINT32 offs;
		P2PFilePart *orgcomplete = complete;

		for(offs = 0; offs < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount() && m_rareblocks[offs].block != 0; offs++)
		{
			for ( ; complete != NULL; complete = complete->m_next)
			{
				block = (UINT32)((complete->m_offset + complete->m_length ) / blocksize );
				OP_ASSERT(block < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount() );

				if(m_available[block] && block == m_rareblocks[offs].block)
				{
					*offset = complete->m_offset;
					*length = min(blocksize, m_download->m_size - *offset);

					orgcomplete->DeleteChain();
					return OpStatus::OK;
				}
			}
		}
		complete = orgcomplete;
	}
	P2PFilePart* random = complete->GetRandom();

	if(random != NULL)
	{
		*offset = random->m_offset;
		*length = min(blocksize, m_download->m_size - *offset);

		complete->DeleteChain();
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(complete == NULL);
		return OpStatus::ERR;
	}
}

//////////////////////////////////////////////////////////////////////
// multi-source fragment handling

OP_STATUS DownloadTransferBT::SubtractRequested(P2PFilePart** ppFragments)
{
	if(m_requestedCount == 0 || m_choked )
	{
		return OpStatus::ERR;
	}
	P2PFilePart::Subtract( ppFragments, m_requested);
	return OpStatus::OK;
}

OP_STATUS DownloadTransferBT::UnrequestRange(UINT64 offset, UINT64 length)
{
	if(m_requestedCount == 0)
	{
		return OpStatus::ERR;
	}
	OP_ASSERT(((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize() != 0);
	if(((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize() == 0) return FALSE;

	P2PFilePart** ppPrevious = &m_requested;
	BOOL match = FALSE;

	for(P2PFilePart* fragment = *ppPrevious ; fragment ; )
	{
		P2PFilePart* next = fragment->m_next;

		if(offset < fragment->m_offset + fragment->m_length &&
			 offset + length > fragment->m_offset)
		{
			BTPacket* packet = BTPacket::New( BT_PACKET_CANCEL );
			if(packet != NULL)
			{
				packet->WriteLongBE((UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((UINT32)(fragment->m_offset % ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				packet->WriteLongBE((UINT32)fragment->m_length);

				DEBUGTRACE8_UP(UNI_L("*** sending CANCEL %d"), (UINT32)(fragment->m_offset / ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize()));
				DEBUGTRACE_UP(UNI_L(" to %s\n"), (uni_char *)m_address);

				Send(packet);
				if (m_close_requested) // DSK-340876
				{
					Close(m_close_arg);
					return OpStatus::ERR;
				}
			}

			*ppPrevious = next;
			if(next != NULL)
			{
				next->m_previous = fragment->m_previous;
			}
			fragment->DeleteThis();
			m_requestedCount--;
			match = TRUE;
		}
		else
		{
			ppPrevious = &fragment->m_next;
		}
		fragment = next;
	}
//	m_client->Send(NULL);	// send queued data
	return match ? OpStatus::OK : OpStatus::ERR;
}

BOOL DownloadTransferBT::HasCompleteBlock(UINT32 block)
{
	UINT64 length	= ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
	UINT64 offset = (UINT64)block * length;

	OP_ASSERT(length != 0);
	if(length == 0) return FALSE;

	OP_ASSERT(block < ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockCount());

	if(m_download->IsRangeUseful(offset, length))
	{
		return FALSE;
	}
	return TRUE;
}

void DownloadTransferBT::OnReceivedPiece(UINT64 offset, UINT64 length)
{
	m_transferSpeed.AddPoint(length);

	DEBUGTRACE_PEERSPEED(UNI_L("PEER: %s"), m_address.CStr());
	DEBUGTRACE_PEERSPEED(UNI_L(", speed: %.2f\n"), m_transferSpeed.GetKbps());

	m_downloaded += length;
	m_download->m_downloaded += length;
	((BTDownload *)m_download)->m_torrentDownloaded += length;

	m_source->AddFragment(offset, length);
	m_source->SetValid();

	P2PFilePart::Subtract( &m_requested, offset, length );
	m_requestedCount = m_requested->GetCount();

	DownloadTransfer* transfer = m_download->GetFirstTransfer();
	while (transfer)
	{
		DownloadTransfer* next = transfer->m_dlNext;
		// transfer may be removed from the list and deleted if Send fails in UnrequestRange
		transfer->UnrequestRange(offset, length);
		transfer = next;
	}
	SetSnubbed(FALSE);
}

//////////////////////////////////////////////////////////////////////
// piece reception

OP_STATUS DownloadTransferBT::OnPiece(BTPacket* packet)
{
	OP_ASSERT(m_client != NULL);

	if(packet->GetRemaining() < 8)
	{
		return OpStatus::OK;
	}
	if(m_state != dtsRequesting && m_state != dtsDownloading)
	{
		return OpStatus::OK;
	}
	SetState(dtsDownloading);

	UINT32 block	= packet->ReadLongBE();
	UINT64 offset	= packet->ReadLongBE();
	UINT64 length	= packet->GetRemaining();

	offset += (UINT64)block * ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();

	if(m_download->SubmitData(offset, packet->m_buffer + packet->m_position, length, block) == FALSE)
	{
		return OpStatus::ERR_NO_DISK;
	}

	if(HasCompleteBlock(block))
	{
		if(((BTDownload *)m_download)->VerifyTorrentBlock(block))
		{
			OnReceivedPiece(offset, length);

			((BTDownload *)m_download)->OnFinishedTorrentBlock(block);
		}
		else
		{
			// block didn't pass the check!
			// Do the following:
			// - Mark all peers we got data in this block from as suspicious

			DEBUGTRACE_FILE(UNI_L("***** BLOCK %d failed CRC, client: "), block);
			DEBUGTRACE_FILE(UNI_L("%s, address: "), m_source->m_useragent.CStr());
			DEBUGTRACE_FILE(UNI_L("%s\n"), this->m_address.CStr());

			// - Invalidate the data in our structures
			UINT64 length = ((BTDownload *)m_download)->m_torrent.GetImpl()->GetBlockSize();
			UINT64 realoffset = (UINT64)(block * m_download->m_torrent.GetImpl()->GetBlockSize());

			((BTDownload *)m_download)->m_file->InvalidateRange(realoffset, length);

			// - Reduce the download count
			m_downloaded -= length;
//			m_download->m_downloaded -= length;
//			((BTDownload *)m_download)->m_torrentDownloaded -= length;
		}
	}
	else
	{
		OnReceivedPiece(offset, length);
	}

	ShowInterest();

	// try to pipeline multiple requests
	if(m_requestedCount < 5)
	{
		return SendRequests();
	}
	return OpStatus::OK;
}

INT32 BTDownload::GetTransferCount(int state, OpString* address)
{
	INT32 count = 0;

	if(state == dtsCountInterestingPeers)
	{
		for(DownloadTransferBT* transfer = (DownloadTransferBT*)m_transferfirst ;
						transfer != NULL;
						transfer = (DownloadTransferBT*)transfer->m_dlNext)
		{
			if(address == NULL || address->IsEmpty() || address->Compare(transfer->m_host) == 0)
			{
				if(transfer->m_client && transfer->m_interested)
				{
					count++;
				}
			}
		}
	}
	else
	{
		count = DownloadBase::GetTransferCount(state, address);
	}
	return count;
}

// will convert the given data to a compact string containing 4 bytes with the IP and 2 bytes with the port, all in network byte order
OP_STATUS BTDownload::PeerToString(BTCompactPeer& peer, OpByteBuffer& compact_peer)
{
	unsigned char tmpbuf[6];
	UINT32 ip1, ip2, ip3, ip4;

	if(uni_sscanf(peer.m_peer.CStr(), UNI_L("%d.%d.%d.%d"), &ip1, &ip2, &ip3, &ip4) != 4)
	{
		return OpStatus::ERR;
	}
	tmpbuf[0] = ip1 & 0xFF;
	tmpbuf[1] = ip2 & 0xFF;
	tmpbuf[2] = ip3 & 0xFF;
	tmpbuf[3] = ip4 & 0xFF;
	tmpbuf[4] = (peer.m_port & 0xFF00) >> 8;
	tmpbuf[5] = peer.m_port & 0xFF;

	return compact_peer.Append(tmpbuf, 6);
}

// will convert the given network order 6 bytes string to a IPv4 ip address and port
OP_STATUS BTDownload::CompactStringToPeer(unsigned short* compact_peer, OpString& peer, UINT32& port)
{
	UINT32 ipno = (UINT32)(compact_peer[0] << 24) | (compact_peer[1] << 16) | (compact_peer[2] << 8) | compact_peer[3];

	port = (UINT32)(compact_peer[4] << 8) | compact_peer[5];

	RETURN_IF_ERROR(peer.AppendFormat(UNI_L("%d.%d.%d.%d"),
		ipno >> 24,
		(ipno >> 16) & 0xFF,
		(ipno >> 8)  & 0xFF,
		ipno & 0xFF));

	return OpStatus::OK;
}

OP_STATUS BTDownload::StringToPeerlist(OpString& compact_peers, OpVector<BTCompactPeer>& peers)
{
	OpString peer_string;
	UINT32 port;
	int total, offset = 0;

	total = compact_peers.Length()-6;		// 6 bytes per entry

	while(offset <= total)
	{
		unsigned short *tmp = (unsigned short *)(compact_peers.CStr() + offset);

		RETURN_IF_ERROR(CompactStringToPeer(tmp, peer_string, port));

		BTCompactPeer *peer = OP_NEW(BTCompactPeer, (peer_string, port));
		if(!peer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_STATUS s = peers.Add(peer);
		if(OpStatus::IsError(s))
		{
			OP_DELETE(peer);
			return s;
		}
		peer_string.Empty();
		offset += 6;
	}
	return OpStatus::OK;
}

#endif // _BITTORRENT_SUPPORT_
#endif //M2_SUPPORT
