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
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/quick-widget-names.h"
#include "modules/locale/oplanguagemanager.h"

OP_STATUS OP_RenameFile(const uni_char *oldfilename,const uni_char *newfilename);

#include "bt-benode.h"
#include "bt-download.h"
#include "bt-info.h"
#include "dl-base.h"
#include "bt-client.h"
#include "bt-util.h"
#include "bt-upload.h"
#include "bt-globals.h"
#include "p2p-fileutils.h"
#include "bt-packet.h"

INT32 DownloadBase::m_next_transfer_id = 0;

#define BT_PROGRESS_DELAY	1		// only update once every second

#ifndef MSG_BITTORRENT_DELETE_DOWNLOAD
#define MSG_BITTORRENT_DELETE_DOWNLOAD MSG_CLEAN_UP_CALLBACKS
#endif

double P2PTransferSpeed::RelTimer::Get()
{
	unsigned long _new_sec, _new_msec;
	long new_sec, new_msec;
	double res;

	g_op_time_info->GetWallClock( _new_sec, _new_msec );

	new_sec = (long)_new_sec;
	new_msec = (long)_new_msec;

	res = (new_sec - last_sec) + (new_msec-last_msec)/1000.0;

	last_sec = new_sec;
	last_msec = new_msec;

	return res;
}

P2PTransferSpeed::P2PTransferSpeed() :
	m_loaded(0),
	m_lastLoaded(0),
	m_last_elapsed(0),
	m_kibs(0),
	m_pointpos(0),
	m_points(NULL)
{
	BT_RESOURCE_ADD("P2PTransferSpeed", this);
}

P2PTransferSpeed::~P2PTransferSpeed()
{
	OP_DELETEA(m_points);

	BT_RESOURCE_REMOVE(this);
}

// bytes is the number of bytes downloaded since the last call to this
// method
void P2PTransferSpeed::AddPoint(UINT64 bytes)
{
	if (!m_points)
	{
		m_points = OP_NEWA(P2PSpeedCheckpoint, P2P_FLOATING_AVG_SLOTS);
		if (!m_points)
			return;
		for( int i = 0; i < P2P_FLOATING_AVG_SLOTS; i++ )
		{
			m_points[i].used = false;
		}
	}

	double elapsed = m_timer.Get();

	if(m_lastLoaded > m_loaded)
	{
		m_lastLoaded = 0;
		m_loaded = 0;
	}
	m_loaded += bytes;

	m_kibs = CalculateAverageSpeed();

	OP_ASSERT(m_kibs >= 0);

	if(m_kibs < 0)
	{
		m_kibs = 0;
	}
	m_kibs /= 1024.0;

	m_points[m_pointpos].elapsed = elapsed;
	m_points[m_pointpos].bytes   = bytes;
	m_points[m_pointpos].used    = true;

	if(++m_pointpos >= P2P_FLOATING_AVG_SLOTS)
	{
		m_pointpos = 0;
	}
}

double P2PTransferSpeed::CalculateAverageSpeed()
{
	if (!m_points)
	{
		return 0;
	}
	UINT64 bytes = 0;
	double over = 0.0;
	for( int i = 0; i < P2P_FLOATING_AVG_SLOTS; i++ )
	{
		if(m_points[i].used)
		{
			over  += m_points[i].elapsed;
			bytes += m_points[i].bytes;
		}
	}
	// Quickly ramp up the 'kbps' rate once data starts arriving again.
	if( !bytes )
	{
		for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
		{
			m_points[i].used = false;
		}
	}
	if( over == 0.0 )
	{
		over = 1.0;
	}
	return (INT64)bytes / over;
}

DownloadBase::DownloadBase()
{
	Clear();

//	g_Downloads->Add(this);
}

void DownloadBase::Clear()
{
	m_cookie		= 1;
	m_runcookie		= 1;
	m_savecookie	= 1;
	m_size			= SIZE_UNKNOWN;
	m_bBTH			= FALSE;
	m_uploaded = 0;
	m_downloaded = 0;
	m_file					= OP_NEW(P2PBlockFile, ());
	m_received				= op_time(NULL);
	m_transferfirst	= NULL;
	m_transferlast		= NULL;
	m_transfercount	= 0;
	m_transferstart	= 0;
	m_paused		= FALSE;
	m_boosted		= FALSE;
	m_complete		= FALSE;
	m_completedTime = 0;
	m_saved			= 0;
	m_began			= 0;
//	m_sourcefirst	= NULL;
//	m_sourcelast	= NULL;
	m_verify		= TS_UNKNOWN;
	m_sourcecount	= 0;
	m_transfer_id	= -1;
	m_seeding		= FALSE;
	m_need_more_sources = FALSE;
	m_transfer_item	= NULL;
	m_is_transfer_manager_listener = FALSE;
	m_lastProgressUpdate = op_time(NULL);
	m_last_uploaded	= 0;
	m_last_downloaded = 0;
	m_delete_files_when_removed = FALSE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "DownloadBase::~DownloadBase()"
#endif

DownloadBase::~DownloadBase()
{
	DownloadSource *source, *next;

	if(m_is_transfer_manager_listener)
	{
		g_transferManager->RemoveTransferManagerListener(this);
	}
	OP_DELETE(m_file);

	source = m_sources.First();
	while(source)
	{
		next = source->Suc();

		source->Out();
		OP_DELETE(source);

		source = next;
	}

//	g_Downloads->Remove(this);

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);
}

void DownloadBase::StartListening()
{
	if(!m_is_transfer_manager_listener)
	{
		g_transferManager->AddTransferManagerListener(this);
		m_is_transfer_manager_listener = TRUE;
	}
}


void DownloadBase::FileDownloadInitalizing()
{
	UINT64 content_size = 0;
	if(m_transfer_item == NULL)
	{
		AddTransferItem(content_size);
	}
}

void DownloadBase::FileDownloadCheckingFiles(UINT64 current, UINT64 total)
{
	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->SetCalculateKbps(FALSE);
		transfer_item->SetCurrentSize(current);
		transfer_item->SetCompleteSize(total);
		transfer_item->Get_P2P_URL()->ForceStatus(P2P_URL::P2P_CHECKING_FILES);
	}
}

void DownloadBase::FileDownloadBegin(BOOL& transfer_failed)
{
	transfer_failed = FALSE;

	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		// Update the status to loading.
//		transfer_item->SetCalculateKbps(FALSE);
		transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADING);
		transfer_item->SetCurrentSize(GetVolumeComplete());
		transfer_item->SetCompleteSize(m_torrent.GetImpl()->GetTotalSize());

//		OpVector<UploadTransferBT> *uploads = ((BTDownload *)this)->GetUploads();
//		UINT32 idx;
		UINT32 activeuploads = 0;


		activeuploads += m_Uploads.GetTransferCount(upsRequestAndUpload);

		UINT32 seeds = GetTransferCount(dtsCountAllWithCompleteFile);
		UINT32 all_peers = GetTransferCount(dtsCountAll);
		UINT32 downloading = GetTransferCount(dtsDownloading);

		all_peers -= seeds;
		transfer_item->SetConnectionsWithCompleteFile(seeds);
		transfer_item->SetConnectionsWithIncompleteFile(all_peers);
		transfer_item->SetDownloadConnections(downloading);
		transfer_item->SetUploadConnections(activeuploads);
		transfer_item->SetTotalWithCompleteFile(max(((BTDownload *)this)->m_totalSeeders, seeds));
		transfer_item->SetTotalDownloaders(max(((BTDownload *)this)->m_totalPeers, all_peers));
	}
	else
		transfer_failed = TRUE;
}

void DownloadBase::FileProgress(BOOL& transfer_failed)
{
	transfer_failed = FALSE;

	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		UINT64 downloaded = GetVolumeComplete();
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->SetCalculateKbps(TRUE);
		// If the user have pressed stop, the status won't be loading anymore.
//		if(transfer_item->Get_P2P_URL()->Status() == URL_LOADING)
		if(downloaded != m_torrent.GetImpl()->GetTotalSize())
		{
			transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADING);
			if(downloaded >= m_last_downloaded)
			{
				transfer_item->SetCurrentSize(downloaded);
			}
			transfer_item->SetCompleteSize(m_torrent.GetImpl()->GetTotalSize());
			transfer_item->SetCompleted(FALSE);
		}
		UINT64 uploaded = GetVolumeUploaded();

		if(uploaded > m_last_uploaded)
		{
			transfer_item->SetUploaded(uploaded);

//			g_Transfers->SetUploaded(uploaded - m_last_uploaded);
		}
		m_last_uploaded = uploaded;


		UINT32 activeuploads = 0;

		if(m_lastProgressUpdate != 0)
		{
			time_t now = op_time(NULL);

			if(now < m_lastProgressUpdate + BT_PROGRESS_DELAY)
			{
				return;
			}
			m_lastProgressUpdate = now;

			activeuploads = m_Uploads.GetTransferCount(upsRequestAndUpload);

			UINT32 seeds = GetTransferCount(dtsCountAllWithCompleteFile);
			UINT32 all_peers = GetTransferCount(dtsCountAll);
			UINT32 downloading = GetTransferCount(dtsDownloading);

			all_peers -= seeds;
			transfer_item->SetConnectionsWithCompleteFile(seeds);
			transfer_item->SetConnectionsWithIncompleteFile(all_peers);
			transfer_item->SetDownloadConnections(downloading);
			transfer_item->SetUploadConnections(activeuploads);
			transfer_item->SetTotalWithCompleteFile(max(((BTDownload *)this)->m_totalSeeders, seeds));
			transfer_item->SetTotalDownloaders(max(((BTDownload *)this)->m_totalPeers, all_peers));
		}
		if(downloaded > m_last_downloaded)
		{
			m_last_downloaded = downloaded;
		}
	}
	else
		transfer_failed = TRUE;
}

void DownloadBase::FileDownloadSharing(BOOL& transfer_failed)
{
	transfer_failed = FALSE;

	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->SetCalculateKbps(TRUE);
		transfer_item->Get_P2P_URL()->ForceStatus(P2P_URL::P2P_SHARING_FILES);

		transfer_item->SetCurrentSize(GetVolumeComplete());
		transfer_item->SetCompleteSize(m_torrent.GetImpl()->GetTotalSize());
		transfer_item->SetUploaded(GetVolumeUploaded());

		if(GetVolumeComplete() == m_torrent.GetImpl()->GetTotalSize())
		{
			transfer_item->SetCompleted(TRUE);
		}
		g_Transfers->UpdateTransferSpeed((UINTPTR)this, transfer_item->GetKbps(), transfer_item->GetKbpsUpload());

//		OpVector<UploadTransferBT> *uploads = ((BTDownload *)this)->GetUploads();
//		UINT32 idx;
		UINT32 activeuploads = 0;

		if(m_lastProgressUpdate != 0)
		{
			time_t now = op_time(NULL);

			if(now < m_lastProgressUpdate + BT_PROGRESS_DELAY)
			{
				return;
			}
			m_lastProgressUpdate = now;

			activeuploads = m_Uploads.GetTransferCount(upsRequestAndUpload);

			UINT32 seeds = GetTransferCount(dtsCountAllWithCompleteFile);
			UINT32 all_peers = GetTransferCount(dtsCountAll);
			UINT32 downloading = GetTransferCount(dtsDownloading);

			all_peers -= seeds;
			transfer_item->SetConnectionsWithCompleteFile(seeds);
			transfer_item->SetConnectionsWithIncompleteFile(all_peers);
			transfer_item->SetDownloadConnections(downloading);
			transfer_item->SetUploadConnections(activeuploads);
			transfer_item->SetTotalWithCompleteFile(max(((BTDownload *)this)->m_totalSeeders, seeds));
			transfer_item->SetTotalDownloaders(max(((BTDownload *)this)->m_totalPeers, all_peers));
		}
	}
	else
		transfer_failed = TRUE;
}

void DownloadBase::FileDownloadCompleted()
{
	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADED);
		transfer_item->SetCompleted(TRUE);

		g_Transfers->UpdateTransferSpeed((UINTPTR)this, 0, 0);

		transfer_item->SetConnectionsWithCompleteFile(0);
		transfer_item->SetConnectionsWithIncompleteFile(0);
		transfer_item->SetDownloadConnections(0);
		transfer_item->SetUploadConnections(0);
		transfer_item->SetTotalWithCompleteFile(0);
		transfer_item->SetTotalDownloaders(0);
	}
}

void DownloadBase::FileDownloadFailed()
{
	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADING_FAILURE);
		transfer_item->SetConnectionsWithCompleteFile(0);
		transfer_item->SetConnectionsWithIncompleteFile(0);
		transfer_item->SetDownloadConnections(0);
		transfer_item->SetUploadConnections(0);
		transfer_item->SetTotalWithCompleteFile(0);
		transfer_item->SetTotalDownloaders(0);

		StopTrying();
	}
}

void DownloadBase::FileDownloadStopped()
{
	OpTransferItem* item = GetTransferItem();
	if(item != 0)
	{
		TransferItem* transfer_item = (TransferItem *)(item);
		OP_ASSERT(transfer_item != 0);

		if(GetVolumeComplete() == m_torrent.GetImpl()->GetTotalSize())
		{
			((TransferItem *)transfer_item)->SetCompleted(TRUE);
			transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADED);
		}
		else
		{
			transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADING_ABORTED);
		}

		g_Transfers->UpdateTransferSpeed((UINTPTR)this, 0, 0);
		((TransferItem *)transfer_item)->SetCalculateKbps(FALSE);
		transfer_item->SetConnectionsWithCompleteFile(0);
		transfer_item->SetConnectionsWithIncompleteFile(0);
		transfer_item->SetDownloadConnections(0);
		transfer_item->SetUploadConnections(0);
		transfer_item->SetTotalWithCompleteFile(0);
		transfer_item->SetTotalDownloaders(0);

		m_last_downloaded = 0;
		m_last_uploaded = 0;
	}
}

BOOL DownloadBase::OnTransferItemAdded(OpTransferItem* transfer_item,
	BOOL is_populating, double last_speed)
{
	if(m_transfer_item == NULL)
	{
		m_transfer_item = transfer_item;
	}
	return FALSE;
}

void DownloadBase::OnTransferItemStopped(OpTransferItem* transfer_item)
{
	if(transfer_item == m_transfer_item)
	{
		Pause();
		FileDownloadStopped();
		g_Transfers->UpdateTransferSpeed((UINTPTR)this, 0, 0);
		((TransferItem *)transfer_item)->SetCalculateKbps(FALSE);
		((TransferItem *)transfer_item)->SetConnectionsWithCompleteFile(0);
		((TransferItem *)transfer_item)->SetConnectionsWithIncompleteFile(0);
		((TransferItem *)transfer_item)->SetDownloadConnections(0);
		((TransferItem *)transfer_item)->SetUploadConnections(0);
		((TransferItem *)transfer_item)->SetTotalWithCompleteFile(0);
		((TransferItem *)transfer_item)->SetTotalDownloaders(0);
	}
}

void DownloadBase::OnTransferItemResumed(OpTransferItem* transfer_item)
{
	if(transfer_item == m_transfer_item)
	{
		if(IsTrying() == FALSE)
		{
			// FIX: memory leak
			BTInfo *info = OP_NEW(BTInfo, ());

			if(info != NULL)
			{
				OpString /*filename,*/ target_dir;

				//((TransferItem *)transfer_item)->GetMetaFile(filename);
				((TransferItem *)transfer_item)->GetDownloadDirectory(target_dir);

				info->GetImpl()->SetTargetDirectory(target_dir);
				info->GetImpl()->AttachToDownload(this);

				Resume();

				info->LoadTorrentFile(transfer_item);
			}
		}
	}
}

BOOL DownloadBase::OnTransferItemCanDeleteFiles(OpTransferItem* transferItem)
{
	if(transferItem == m_transfer_item)
	{
		if(!((TransferItem *)transferItem)->GetIsCompleted())
		{
			return TRUE;
		}
	}
	return FALSE;
}

void DownloadBase::OnTransferItemDeleteFiles(OpTransferItem* transfer_item)
{
	if(transfer_item == m_transfer_item)
	{
		m_delete_files_when_removed = FALSE;
	
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,
			(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION,
			 WINDOW_NAME_TRANSFER_DELETE,Str::D_DELETE_BT_FILES, Str::DI_IDM_DELETE));
		if (!controller)
			return;

		SimpleDialogController::DialogResultCode retval;
		controller->SetBlocking(retval);
		OP_STATUS stat = ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow(TRUE));

		if(OpStatus::IsSuccess(stat) && retval == SimpleDialogController::DIALOG_RESULT_OK)
		{
			m_delete_files_when_removed = TRUE;
		}
	}
}

void DownloadBase::OnTransferItemRemoved(OpTransferItem* transfer_item)
{
	if(transfer_item == m_transfer_item)
	{
		BOOL delete_files = m_delete_files_when_removed;

		Pause();
		g_transferManager->RemoveTransferManagerListener(this);
		m_transfer_item->SetTransferListener(NULL);
		m_is_transfer_manager_listener = FALSE;
		g_Transfers->UpdateTransferSpeed((UINTPTR)this, 0, 0);

		OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_BITTORRENT_DELETE_DOWNLOAD, (MH_PARAM_1)this));

		if(delete_files)
		{
			((TransferItem *)m_transfer_item)->GetDownloadDirectory(m_directory);

			int idx = m_directory.FindLastOf(PATHSEPCHAR);

			if(idx != KNotFound)
			{
				m_directory.Append(PATHSEP);
			}
			m_directory.Append( ((TransferItem *)m_transfer_item)->GetStorageFilename()->CStr() );

			if(m_directory.Length() < 2) // extra security in case GetStorageFilename() returns an empty string
				m_directory.Empty();
			else
				((TransferItem *)m_transfer_item)->GetMetaFile(m_torrent_file);
		}
		((BTDownload *)this)->m_metadata.DeleteMetaFile();

		g_main_message_handler->PostDelayedMessage(MSG_BITTORRENT_DELETE_DOWNLOAD, (MH_PARAM_1)this, 0, 60);
	}
}

void DownloadBase::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	g_main_message_handler->UnsetCallBack(this, MSG_BITTORRENT_DELETE_DOWNLOAD);

	OpFile opfile;

	if(m_torrent_file.HasContent() && OpStatus::IsSuccess(opfile.Construct(m_torrent_file.CStr())))
	{
		OpStatus::Ignore(opfile.Delete());
	}
	if(m_directory.HasContent() && OpStatus::IsSuccess(opfile.Construct(m_directory.CStr())))
	{
		OpStatus::Ignore(opfile.Delete());
	}

	Remove(FALSE);
	// *this is a deleted pointer at this point!
}

OP_STATUS DownloadBase::AddTransferItem(UINT64& content_size)
{
	OP_ASSERT(GetTransferItem() == 0);

	// Start to listen on the transfer manager if needed.
	if(!m_is_transfer_manager_listener)
	{
		RETURN_IF_ERROR(g_transferManager->AddTransferManagerListener(this));
		m_is_transfer_manager_listener = TRUE;
	}

	OpString target_dir;
	m_torrent.GetImpl()->GetTargetDirectory(target_dir);

	int len = target_dir.Length();
	if (len && target_dir[--len] == PATHSEPCHAR)
		target_dir.Delete(len, 1);

	OpString torrent_name;
	m_torrent.GetImpl()->GetName(torrent_name);

	URL url = urlManager->GetURL(torrent_name, NULL, TRUE);

	OpString meta_file;

	RETURN_IF_ERROR(meta_file.Set(m_torrent.GetImpl()->GetSaveFilename()));

	RETURN_IF_ERROR(((TransferManager*)g_transferManager)->AddTransferItem(url, torrent_name.CStr(), OpTransferItem::ACTION_UNKNOWN, FALSE, 0, TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD, &meta_file, &target_dir));
	OpTransferItem* op_transfer_item;
	RETURN_IF_ERROR(((TransferManager*)g_transferManager)->GetTransferItem(&op_transfer_item, torrent_name.CStr()));

	TransferItem* transfer_item = (TransferItem *)(op_transfer_item);
	OP_ASSERT(transfer_item != NULL);

	transfer_item->SetType(TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD);
	transfer_item->Clear();
	transfer_item->Get_P2P_URL()->SetContentSize(m_size);
	transfer_item->Get_P2P_URL()->ForceStatus(URL_LOADING_WAITING);

	if(m_torrent.GetImpl()->GetFileCount() == 1)
	{
		torrent_name.Set(m_torrent.GetImpl()->GetFiles()[0].m_sPath);
	}
	transfer_item->SetStorageFilename(torrent_name.CStr());

//	transfer_item->GetURL()->GetRep()->CheckStorage();
// 	transfer_item->GetURL()->GetRep()->GetDataStorage()->SetContentSize((unsigned long)m_size);
 	transfer_item->GetURL()->ForceStatus(URL_LOADING_WAITING);

	content_size = transfer_item->Get_P2P_URL()->GetContentLoaded();

	SetTransferId(NewTransferId());

	return OpStatus::OK;
}

OpTransferItem* DownloadBase::GetTransferItem()
{
	return m_transfer_item;
}

INT32 DownloadBase::GetTransferId()
{
	return m_transfer_id;
}

void DownloadBase::SetStartTimer()
{
	m_began = op_time(NULL);
	SetModified();
}

void DownloadBase::GetFilePath(OpString& path)
{
	m_file->GetFilePath(path);
}

//////////////////////////////////////////////////////////////////////
// DownloadBase modified

BOOL DownloadBase::OpenFile(OpVector<P2PVirtualFile> *files)
{
	if(m_file == NULL || m_remotename.IsEmpty() || m_size == SIZE_UNKNOWN) return FALSE;
	if(m_file->IsOpen()) return TRUE;

	OpVector<P2PVirtualFile> tmpfiles;

	SetModified();

	OpString target_directory;

	m_torrent.GetImpl()->GetTargetDirectory(target_directory);

	m_file->SetTargetDirectory(target_directory);

	if(files == NULL)
	{
		// get the array of files
		BTInfoImpl::BTFile *btfiles = m_torrent.GetImpl()->GetFiles();

		for(int nFile = 0; nFile < m_torrent.GetImpl()->GetFileCount(); nFile++)
		{
			P2PVirtualFile *virt_file = OP_NEW(P2PVirtualFile, (btfiles[nFile].m_sPath, btfiles[nFile].m_nSize, btfiles[nFile].m_nOffset));

			if (virt_file)
			{
				BT_RESOURCE_ADD("P2PVirtualFile-OpenFile", virt_file);
				tmpfiles.Add(virt_file);
			}
		}
		files = &tmpfiles;
	}
	if(m_file->Open(m_localname, &m_metadata, files, m_complete ? FALSE : TRUE, m_torrent.GetImpl()->GetBlockSize()))
	{
#if defined(DEBUG_BT_RESOURCE_TRACKING)
		UINT32 cnt;

		for(cnt = 0; cnt < tmpfiles.GetCount(); cnt++)
		{
			P2PVirtualFile *virt_file = tmpfiles.Get(cnt);

			BT_RESOURCE_REMOVE(virt_file);

			OP_DELETE(virt_file);
		}
		tmpfiles.Clear();
#else
		tmpfiles.DeleteAll();
#endif
		if(m_file->IsValid())
		{
//			if(m_file->Open(m_localname))
//			{
				return TRUE;
//			}
			// FIX: add logging for error here
		}
	}
	// FIX: Add checking for disk space on the download disk
	else
	{
		OpString strLocalName;

		strLocalName.Append(m_localname);
		m_localname.Empty();

		GenerateLocalName();

		if(m_file->CreateAll(m_localname, m_size, &m_metadata, files, m_torrent.GetImpl()->GetBlockSize()))
		{
			return TRUE;
		}
		m_localname.Set(strLocalName);

#if defined(DEBUG_BT_RESOURCE_TRACKING)
		UINT32 cnt;

		for(cnt = 0; cnt < tmpfiles.GetCount(); cnt++)
		{
			P2PVirtualFile *virt_file = tmpfiles.Get(cnt);

			BT_RESOURCE_REMOVE(virt_file);

			OP_DELETE(virt_file);
		}
		tmpfiles.Clear();
#else
		tmpfiles.DeleteAll();
#endif
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// start a new transfer

BOOL DownloadBase::StartNewTransfer(time_t tNow)
{
	INT32 srccnt = 0;

	if(tNow == 0)
	{
		tNow = op_time(NULL);
	}
	DownloadSource* connecthead = NULL;
	DownloadSource* source;

	for(source = m_sources.First(); source != NULL;)
	{
		DownloadSource* next = source->Suc();

		if(source->m_transfer != NULL)
		{
			// Already has a transfer
			srccnt++;
		}
		else
		{
			if(source->m_attempt == 0)
			{
				if(source->CanInitiate(FALSE))
				{
					DownloadTransfer* transfer = source->CreateTransfer();
					return transfer != NULL && transfer->Initiate();
				}
			}
			else if(source->m_attempt > 0 && source->m_attempt <= tNow)
			{
				if(connecthead == NULL)
				{
					if(source->CanInitiate(FALSE))
					{
						connecthead = source;
					}
				}
			}
		}
		source = next;
	}

	if(connecthead != NULL)
	{
		DownloadTransfer* transfer = connecthead->CreateTransfer();
		return transfer != NULL && transfer->Initiate();
	}
	if(srccnt == GetSourceCount() && srccnt != 0 && GetSourceCount() < 3)
	{
		m_need_more_sources = TRUE;
	}
	return FALSE;
}

void DownloadBase::CloseFile()
{
	if(m_file != NULL)
	{
		m_file->Close();
	}
}

void DownloadBase::CloseTransfers()
{
//	BOOL backup = g_Downloads->m_closing;
//	g_Downloads->m_closing = TRUE;

	for(DownloadTransfer* transfer = m_transferfirst ; transfer != NULL;)
	{
		DownloadTransfer* next = transfer->m_dlNext;

		transfer->Close(TS_TRUE);
		transfer = next;
	}
	m_transferfirst = NULL;

	OP_ASSERT(m_transfercount == 0);

//	g_Downloads->m_closing = backup;
}


//////////////////////////////////////////////////////////////////////
// DownloadBase local name

void DownloadBase::GenerateLocalName()
{
	if(m_localname.HasContent()) return;

	OpString strHash;

	if(m_remotename.HasContent())
	{
		m_localname.Set(m_remotename);
	}

	if(m_localname.HasContent())
	{
		// FIX
//		CreateDirectory(Settings.Downloads.IncompletePath, NULL);
//		m_localname = Settings.Downloads.IncompletePath + _T("\\") + m_localname;
	}
}

//////////////////////////////////////////////////////////////////////
// check if a range would "help"

BOOL DownloadBase::IsRangeUseful(UINT64 offset, UINT64 length)
{
	if(m_file == NULL || ! m_file->IsValid())
	{
		 return 0;
	}
	return m_file->DoesRangeOverlap(offset, length);
}

UINT64 DownloadBase::GetVolumeComplete()
{
	if(m_file != NULL)
	{
		if(m_file->IsValid())
		{
			return m_file->GetCompleted();
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return m_size;
	}
}

UINT64 DownloadBase::GetVolumeRemaining()
{
	if(m_file != NULL)
	{
		if(m_file->IsValid())
		{
			return m_file->GetRemaining();
		}
		else if(m_size != SIZE_UNKNOWN)
		{
			return m_size;
		}
	}
	return 0;
}

UINT64 DownloadBase::GetVolumeUploaded()
{
	return m_uploaded;
}

void DownloadBase::GetDisplayName(OpString& name)
{
	if(m_remotename.HasContent())
	{
		name.Append(m_remotename);
	}
}

P2PFilePart* DownloadBase::GetFirstEmptyFragment()
{
	return m_file ? m_file->GetFirstEmptyFragment() : NULL;
}

//////////////////////////////////////////////////////////////////////
// Get a list of possible download fragments

P2PFilePart* DownloadBase::GetPossibleFragments(P2PFilePart* available, UINT64* pnLargestOffset, UINT64* pnLargestLength)
{
	if(!PrepareFile()) return NULL;

	P2PFilePart* possible;

	if(available != NULL)
	{
		possible = m_file->GetFirstEmptyFragment();
		possible = possible->CreateAnd(available);
	}
	else
	{
		possible = m_file->CopyFreeFragments();
	}

	if(possible == NULL) return NULL;

	if(pnLargestOffset && pnLargestLength)
	{
		if(P2PFilePart* largest = possible->GetLargest())
		{
			*pnLargestOffset = largest->m_offset;
			*pnLargestLength = largest->m_length;
		}
		else
		{
			OP_ASSERT(FALSE);	// should never happen
			return NULL;
		}
	}

	for (DownloadTransfer* transfer = GetFirstTransfer() ; transfer && possible ; transfer = transfer->m_dlNext)
	{
		transfer->SubtractRequested(&possible);
	}

	return possible;
}

//////////////////////////////////////////////////////////////////////
// Select a fragment for a transfer

BOOL DownloadBase::GetFragment(DownloadTransfer* transfer)
{
	if(!PrepareFile()) return FALSE;

	UINT64 largestoffset = SIZE_UNKNOWN, largestlength = SIZE_UNKNOWN;

	P2PFilePart* possible = GetPossibleFragments(
		transfer->m_source->m_available,
		&largestoffset, &largestlength);

	if(largestoffset == SIZE_UNKNOWN)
	{
		OP_ASSERT(possible == NULL);
		return FALSE;
	}

	if(possible != NULL)
	{
		P2PFilePart* random = possible;
		random = possible->GetRandom(TRUE);
		if(random == NULL) return FALSE;

		transfer->m_offset = random->m_offset;
		transfer->m_length = random->m_length;

		possible->DeleteChain();

		return TRUE;
	}
	else
	{
		DownloadTransfer* existing = NULL;

		for(DownloadTransfer* other = GetFirstTransfer(); other != NULL; other = other->m_dlNext)
		{
			if(other->m_recvbackwards)
			{
				if(other->m_offset + other->m_length - other->m_position
					 != largestoffset + largestlength) continue;
			}
			else
			{
				if(other->m_offset + other->m_position != largestoffset) continue;
			}
			existing = other;
			break;
		}
		if(existing == NULL)
		{
			transfer->m_offset = largestoffset;
			transfer->m_length = largestlength;
			return TRUE;
		}

		if(largestlength < 32)
		{
			return FALSE;
		}
		UINT64 length	= largestlength / 2;

		if(existing->m_recvbackwards)
		{
			transfer->m_offset			= largestoffset;
			transfer->m_length			= length;
			transfer->m_wantbackwards	= FALSE;
		}
		else
		{
			transfer->m_offset			= largestoffset + largestlength - length;
			transfer->m_length			= length;
			transfer->m_wantbackwards	= TRUE;
		}
		return TRUE;
	}
}

//////////////////////////////////////////////////////////////////////
// check if a byte position is empty

BOOL DownloadBase::IsPositionEmpty(UINT64 offset)
{
	if(m_file == NULL || !m_file->IsValid())
	{
		return FALSE;
	}
	return m_file->IsPositionRemaining(offset);
}

//////////////////////////////////////////////////////////////////////
// submit data

BOOL DownloadBase::SubmitData(UINT64 offset, byte *pData, UINT64 length, UINT32 block)
{
	SetModified();

	m_received = op_time(NULL);

	DEBUGTRACE8(UNI_L("*** WRITING offset: %lld, "), offset);
	DEBUGTRACE8(UNI_L("length: %d\n"), length);
	return m_file != NULL && m_file->WriteRange(offset, pData, length, block);
}

//////////////////////////////////////////////////////////////////////
// make the file appear complete

BOOL DownloadBase::MakeComplete()
{
	if(m_localname.IsEmpty()) return FALSE;
	if(!PrepareFile()) return FALSE;
	return m_file->MakeComplete();
}

//////////////////////////////////////////////////////////////////////
// add and remove transfers

void DownloadBase::AddTransfer(DownloadTransfer* transfer)
{
	m_transfercount++;
	transfer->m_dlPrev = m_transferlast;
	transfer->m_dlNext = NULL;

	if(m_transferlast != NULL)
	{
		m_transferlast->m_dlNext = transfer;
		m_transferlast = transfer;
	}
	else
	{
		m_transferfirst = m_transferlast = transfer;
	}
}

void DownloadBase::RemoveTransfer(DownloadTransfer* transfer)
{
	OP_ASSERT(m_transfercount > 0);
	m_transfercount--;

	if(transfer->m_dlPrev != NULL)
	{
		transfer->m_dlPrev->m_dlNext = transfer->m_dlNext;
	}
	else
	{
		m_transferfirst = transfer->m_dlNext;
	}
	if(transfer->m_dlNext != NULL)
	{
		transfer->m_dlNext->m_dlPrev = transfer->m_dlPrev;
	}
	else
	{
		m_transferlast = transfer->m_dlPrev;
	}
	OP_DELETE(transfer);
}

BOOL DownloadBase::PrepareFile()
{
	BOOL success;

	success = OpenFile(&m_files) && m_file->GetRemaining() > 0;

	return success;
}

//////////////////////////////////////////////////////////////////////
// remove a source

void DownloadBase::RemoveSource(DownloadSource* source, BOOL ban)
{
//	if(ban && source->m_URL.Length())
//	{
//		m_failedsources.Add(&source->m_URL);
//	}

	OP_ASSERT(m_sourcecount > 0);
	m_sourcecount--;

	source->Out();
	OP_DELETE(source);

	SetModified();
}

//////////////////////////////////////////////////////////////////////
// list access

int DownloadBase::GetSourceCount(BOOL sane)
{
	if(!sane) return m_sourcecount;

	time_t tNow = op_time(NULL);

	INT32 count = 0;

	DownloadSource* source;

	for(source = m_sources.First(); source != NULL; source = source->Suc())
	{
		if(!sane ||
			 source->m_attempt < tNow ||
			 source->m_attempt - tNow <= 900)
		{
			count++;
		}
	}
	return count;
}

BOOL DownloadBase::IsTrying()
{
	return(m_began != 0);
}

DownloadTransferBT *DownloadBase::CreateTorrentTransfer(BTClientConnector* client)
{
	if(IsMoving() || IsPaused()) return NULL;

	DownloadSource* source = NULL;

	for(source = m_sources.First() ; source ; source = source->Suc())
	{
		if(memcmp(&source->m_GUID, &client->m_GUID, 16) == 0)
			break;
	}
	if(source == NULL)
	{
		source = OP_NEW(DownloadSource, ((DownloadBase *)this, &client->m_GUID,
			client->GetTransfer()->m_address, client->m_port));

		if(!source || !AddSourceInternal(source)) return NULL;
	}

	if(source->m_transfer != NULL)
	{
//		return (DownloadTransferBT*)source->m_transfer;
		return NULL;
	}
	source->m_transfer = OP_NEW(DownloadTransferBT, (source, client));

	return (DownloadTransferBT*)source->m_transfer;
}

//////////////////////////////////////////////////////////////////////
// internal source adder

BOOL DownloadBase::AddSourceInternal(DownloadSource* source)
{
	//Check/Reject if source is invalid
	//TODO: Reject if source is the local IP/port?

	DownloadSource* existing;

	for(existing = m_sources.First(); existing ; existing = existing->Suc())
	{
		if(existing->Equals(source))
		{
			if(existing->m_transfer != NULL)
			{
				OP_DELETE(source);
				return FALSE;
			}
/*
			else
			{
				source->m_attempt = existing->m_attempt;
				existing->Remove(TRUE, FALSE);
				break;
			}
*/
		}
	}

	m_sourcecount ++;

	source->Into(&m_sources);

	SetModified();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// GetStartTimer

time_t DownloadBase::GetStartTimer()
{
	return(m_began);
}

BOOL DownloadBase::RunFile(time_t tNow)
{
	if(m_file->IsOpen())
	{
		if(m_file->GetRemaining() == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

INT32 DownloadBase::GetTransferCount(int state, OpString *address)
{
	INT32 count = 0;

	for(DownloadTransfer* transfer = m_transferfirst ; transfer != NULL; transfer = transfer->m_dlNext)
	{
		if(address == NULL || address->IsEmpty() || address->Compare(transfer->m_host) == 0)
		{
			if(state == dtsCountAll)
			{
				count++;
			}
			else if(state == dtsCountAllWithCompleteFile)
			{
				if(transfer->m_has_complete_file)
				{
					count++;
				}
			}
			else if(state == dtsCountNotQueued)
			{
				if(transfer->m_state == dtsTorrent)
				{
					DownloadTransferBT* pBT = (DownloadTransferBT*)transfer;
					if(!pBT->m_choked) count++;
				}
				else if(transfer->m_state != dtsQueued)
				{
					count++;
				}
			}
			else if(state == dtsCountNotConnecting)
			{
				if(transfer->m_state > dtsConnecting) count++;
			}
			else if(state == dtsCountTorrentAndActive)
			{
				if((transfer->m_state == dtsTorrent) ||
					 (transfer->m_state == dtsRequesting) ||
					 (transfer->m_state == dtsDownloading))
					count++;
			}
			else
			{
				if(transfer->m_state == state) count++;
			}
		}
	}
	return count;
}

BOOL DownloadBase::CheckSource(DownloadSource* check)
{
	DownloadSource* source;

	for(source = m_sources.First() ; source != NULL; source = source->Suc())
	{
		if(source == check)
		{
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// download complete handler

void DownloadBase::OnDownloaded()
{
	OP_ASSERT(m_complete == FALSE);

	m_completedTime = op_time(NULL);
	m_complete = TRUE;

	CloseTransfers();
/*
	if(m_file != NULL)
	{
		m_file->Close();
		OP_DELETE(m_file);
		m_file = NULL;
	}
*/
//	ClearSources();

	SetModified();
}


/////////////////////////////////////////////////////////////////
///
/// DownloadSource - one instance per download, a wrapper around the protocol
///
/////////////////////////////////////////////////////////////////


DownloadSource::DownloadSource(DownloadBase* download)
{
	Construct(download);
}

void DownloadSource::Construct(DownloadBase* download)
{
	OP_ASSERT(download != NULL);

	m_download		= download;
	m_transfer		= NULL;

	m_protocol		= PROTOCOL_NULL;
	m_bGUID			= FALSE;
	m_port			= 0;

	m_speed			= 0;
	m_readcontent	= FALSE;

	m_attempt		= 0;
	m_failures		= 0;

	m_available	= NULL;
	m_pastfragment = NULL;

	m_added_time = m_lastseen = op_time(NULL);

	m_pex_peer = FALSE;

	BT_RESOURCE_ADD("DownloadSource", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "DownloadSource::~DownloadSource()"
#endif

DownloadSource::~DownloadSource()
{
	if(m_available) m_available->DeleteChain();
	if(m_pastfragment) m_pastfragment->DeleteChain();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// DownloadSource construction from BitTorrent

DownloadSource::DownloadSource(DownloadBase* download, SHAStruct* GUID, OpString& address, WORD port)
{
	Construct(download);

	m_protocol	= PROTOCOL_BT;		// only protocol supported for the moment
	m_address.Set(address);
	m_port		= port;

	if(m_protocol == PROTOCOL_BT && GUID != NULL)
	{
		memcpy(&m_GUID, GUID, 16);
	}
	m_bGUID		= GUID != NULL;
}

DownloadTransfer* DownloadSource::CreateTransfer()
{
	OP_ASSERT(m_transfer == NULL);

	if(m_protocol == PROTOCOL_BT)	// only protocol supported so far
	{
		return(m_transfer = OP_NEW(DownloadTransferBT, (this, NULL)));
	}
	else
	{
		return NULL;
	}
}

BOOL DownloadSource::CanInitiate(BOOL established)
{
	return established || g_Downloads->AllowMoreTransfers(&m_address);
}


void DownloadSource::Remove(BOOL closetransfer, BOOL ban)
{
	if(m_transfer != NULL)
	{
		if(closetransfer)
		{
			m_transfer->Close(TS_TRUE);
			m_transfer = NULL;
		}
		else
		{
			m_transfer->m_source = NULL;
			m_transfer = NULL;
		}
	}
	m_download->RemoveSource(this, ban);
}

#define RETRYDELAY	60

void DownloadSource::OnFailure(BOOL nondestructive)
{
	if(m_transfer != NULL)
	{
		m_transfer->SetState(dtsNull);
		m_transfer->m_source = NULL;
		m_transfer = NULL;
	}

	INT32 delay = ((UINT32)RETRYDELAY) << m_failures;

	if(m_failures < 20)
	{
		if(delay > 3600000) delay = 3600000;
	}
	else if (delay > 86400000 || m_failures > 25)
        // if m_failures > 25, the calculation of delay shall have overflowed !
        delay = 86400000;

//	nDelay += GetTickCount();

	int maxfailures = (m_readcontent ? 40 : 3);
	if(maxfailures < 20 && m_download->GetSourceCount() > 20) maxfailures = 0;

	m_download->SetModified();

	if(nondestructive || (++m_failures < maxfailures))
	{
		m_attempt = max(m_attempt, delay);
	}
	else
	{
		// source is bad, remove it
		m_download->RemoveSource(this, TRUE);
	}
}

void DownloadSource::SetValid()
{
	m_readcontent = TRUE;
	m_failures = 0;
	m_download->SetModified();
}

void DownloadSource::SetLastSeen()
{
	m_lastseen = op_time(NULL);
	m_download->SetModified();
}

BOOL DownloadSource::CheckHash(const SHAStruct* SHA1)
{
	if(m_download->m_torrent.GetImpl()->IsAvailable()) return TRUE;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// past fragments

void DownloadSource::AddFragment(UINT64 offset, UINT64 length, BOOL bMerge)
{
	m_readcontent = TRUE;
	P2PFilePart::AddMerge(&m_pastfragment, offset, length);
	m_download->SetModified();
}

//////////////////////////////////////////////////////////////////////
// range intersection test

BOOL DownloadSource::HasUsefulRanges()
{
	if(m_available == NULL)
	{
		return m_download->IsRangeUseful(0, m_download->m_size);
	}
	for(P2PFilePart* fragment = m_available ; fragment ; fragment = fragment->m_next)
	{
		if(m_download->IsRangeUseful(fragment->m_offset, fragment->m_length))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// range intersection

BOOL DownloadSource::TouchedRange(UINT64 offset, UINT64 length)
{
	if(m_transfer != NULL && m_transfer->m_state == dtsDownloading)
	{
		if(m_transfer->m_offset + m_transfer->m_length > offset &&
			 m_transfer->m_offset < offset + length)
		{
			return TRUE;
		}
	}
	for(P2PFilePart* fragment = m_pastfragment ; fragment ;
			fragment = fragment->m_next)
	{
		if(fragment->m_offset >= offset + length) continue;
		if(fragment->m_offset + fragment->m_length <= offset) continue;
		return TRUE;
	}
	return FALSE;
}
/////////////////////////////////////////////////////////////////
///
/// DownloadTransfer - one instance per download, a wrapper around the protocol
///
/////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// construction

DownloadTransfer::DownloadTransfer(DownloadSource* source, PROTOCOLID protocol, OpP2PObserver* observer)// : Transfer(*observer)
{
	m_has_complete_file = FALSE;
	m_protocol		= protocol;
	m_download		= source->m_download;
	m_dlPrev		= NULL;
	m_dlNext		= NULL;
	m_source		= source;

	m_state		= dtsNull;

	m_queuepos		= 0;
	m_queuelen		= 0;

	m_offset		= SIZE_UNKNOWN;
	m_length		= 0;
	m_position		= 0;
	m_downloaded	= 0;

	m_wantbackwards = m_recvbackwards = FALSE;

	m_download->AddTransfer(this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "DownloadTransfer::~DownloadTransfer()"
#endif

DownloadTransfer::~DownloadTransfer()
{
	OP_ASSERT(m_source == NULL);

//	m_download->RemoveTransfer(this);

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

}

//////////////////////////////////////////////////////////////////////
// close

void DownloadTransfer::Close(TRISTATE keepsource)
{
	SetState(dtsNull);

//	Transfer::Close();

	if(m_source != NULL)
	{
		switch (keepsource)
		{
		case TS_TRUE:
			m_source->OnFailure(TRUE);
			break;
		case TS_UNKNOWN:
			m_source->OnFailure(FALSE);
			break;
		case TS_FALSE:
			m_source->Remove(FALSE, TRUE);
			break;
		}

		m_source = NULL;
	}

	OP_ASSERT(m_download != NULL);
	DEBUGTRACE8(UNI_L("**** REMOVING downloadtransfer: 0x%08x\n"), this);
	m_download->RemoveTransfer(this);
}

//////////////////////////////////////////////////////////////////////
// fragment size management

void DownloadTransfer::ChunkifyRequest(UINT64* offset, UINT64* length, UINT64 chunk)
{
	OP_ASSERT(offset != NULL && length != NULL);

//	if(m_source->m_closeconn) return;

	if(chunk == 0 || *length <= chunk) return;

	if(m_download->GetVolumeComplete() == 0 || *offset == 0)
	{
		*length = chunk;
	}
	else if(m_wantbackwards)
	{
		*offset = *offset + *length - chunk;
		*length = chunk;
	}
	else
	{
		UINT64 count = *length / chunk;
		if(*length % chunk) count++;
		count = rand() % count;

		UINT64 start = *offset + chunk * count;
		*length = min(chunk, *offset + *length - start);
		*offset = start;
	}
}

//////////////////////////////////////////////////////////////////////
// Downloads construction

Downloads::Downloads()
{
//	m_transfers	= 0;
	m_runcookie	= 0;
//	m_closing		= FALSE;
//	m_lastconnect	= 0;
//	m_allowmoredownloads = TRUE;
//	m_allowmoretransfers = TRUE;
	m_can_checkfile = TRUE;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "Downloads::~Downloads()"
#endif

Downloads::~Downloads()
{
	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);
}

//////////////////////////////////////////////////////////////////////
// add download

DownloadBase* Downloads::Add(DownloadBase *download, SHAStruct *bth)
{
	if(!download)
	{
		download = FindByBTH(bth);

		if(download != NULL)
		{
			// already being downloaded

			return download;
		}
	}
	else
	{
		m_list.Add(download);

		// FIX: Hardcoded max 3 torrents
		if((download->m_bBTH/* && (GetTryingCount(TRUE)  < 3)*/))
		{
			download->SetStartTimer();
			download->FindMoreSources();
		}
	}
	return download;
}

//////////////////////////////////////////////////////////////////////
// Downloads list access

int Downloads::GetSeedCount()
{
	int count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);

		if(download->IsSeeding())
		{
			count++;
		}
	}
	return count;
}

int Downloads::GetActiveTorrentCount()
{
	int count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);

		if(download->IsDownloading() && download->m_bBTH &&
			!download->IsSeeding()	&& !download->IsCompleted() &&
			!download->IsMoving()	&& !download->IsPaused())
				count++;
	}
	return count;
}

int Downloads::GetCount(BOOL activeonly)
{
	if(!activeonly)
	{
		return m_list.GetCount();
	}
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);

		if(!download->IsMoving() && !download->IsPaused() &&
			 download->GetSourceCount(TRUE) > 0)
				count++;
	}
	return count;
}

int Downloads::GetTransferCount()
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		count += m_list.Get(pos)->GetTransferCount();
	}
	return count;
}

int Downloads::GetTryingCount(BOOL torrentsonly)
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);

		if((download->IsTrying()) && (!download->IsCompleted()) && (!download->IsPaused()))
		{
			if((download->m_bBTH) || (!torrentsonly))
				count++;
		}
	}
	return count;
}

int Downloads::GetConnectingTransferCount()
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);

		count += download->GetTransferCount(dtsConnecting);
	}
	return count;
}

void Downloads::Remove(DownloadBase* download)
{
	if(m_list.RemoveByItem(download) == OpStatus::OK)
	{
		OP_DELETE(download);
	}
}

//////////////////////////////////////////////////////////////////////
// Downloads find by pointer or hash

BOOL Downloads::Check(DownloadSource* source)
{
	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		if(m_list.Get(pos)->CheckSource(source))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL Downloads::CheckActive(DownloadBase* download, INT32 scope)
{
	for(INT32 pos = m_list.GetCount() - 1; pos > 0 && scope > 0; pos--)
	{
		DownloadBase* test = m_list.Get(pos);
		BOOL active = test->IsPaused() == FALSE && test->IsCompleted() == FALSE;

		if(download == test)
		{
			return active;
		}
		if(active)
		{
			scope--;
		}
	}
	return FALSE;
}

DownloadBase* Downloads::FindByBTH(SHAStruct *pBTH)
{
	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);
		if(download->m_bBTH && download->m_pBTH == *pBTH)
		{
			return download;
		}
	}
	return NULL;
}

#ifdef UNUSED_CODE
DownloadBase *Downloads::GetDownloadFromTransferItem(OpTransferItem *item)
{
	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);
		if(download->GetTransferItem() == item)
		{
			return download;
		}
	}
	return NULL;
}
#endif

//////////////////////////////////////////////////////////////////////
// Downloads limiting tests

void Downloads::UpdateAllows(BOOL bNew)
{
	INT32 downloads	= 0;
	INT32 transfers	= 0;

/*	if(bNew)
	{
		m_lastconnect = op_time(NULL);
	}*/
	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		DownloadBase* download = m_list.Get(pos);
		INT32 temp = download->GetTransferCount();

		if(temp)
		{
			downloads ++;
			transfers += temp;
		}
	}
//	m_allowmoredownloads = downloads < DOWNLOAD_MAXFILES;
//	m_allowmoretransfers = transfers < DOWNLOAD_MAXTRANSFERS;
}

BOOL Downloads::AllowMoreDownloads()
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		if(m_list.Get(pos)->GetTransferCount())
		{
			count++;
		}
	}
	return count < DOWNLOAD_MAXFILES;
}

BOOL Downloads::AllowMoreTransfers(OpString *address)
{
	INT32 count = 0;

	for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
	{
		count += m_list.Get(pos)->GetTransferCount(dtsCountAll, address);
	}
	if(address == NULL)
	{
		return count < DOWNLOAD_MAXTRANSFERS;
	}
/*
FIX: Implement
	if(m_hostlimits.Lookup((LPVOID)pAddress->S_un.S_addr, (void*&)nLimit))
	{
		return(count < limit);
	}
	else
	{
		return(count == 0);
	}
*/
	return TRUE;
}

void Downloads::SetPerHostLimit(OpString& address, INT32 limit)
{
//	m_pHostLimits.SetAt(address, (LPVOID)nLimit);
}

//////////////////////////////////////////////////////////////////////
// Downloads run callback (threaded)

void Downloads::OnRun()
{
	UINT32 activedownloads	= 0;
	UINT32 activetransfers	= 0;
	UINT32 totaltransfers	= 0;

//	m_validation = 0;
	m_runcookie ++;

	while(TRUE)
	{
		BOOL worked = FALSE;

		for(UINT32 pos = 0; pos < m_list.GetCount(); pos++)
		{
			DownloadBase* download = m_list.Get(pos);
			if(download->m_runcookie == m_runcookie) continue;

			download->m_runcookie = m_runcookie;
			download->OnRun();

			int temp = 0;

			for(DownloadTransfer* transfer = download->GetFirstTransfer() ; transfer != NULL; transfer = transfer->m_dlNext)
			{
				if(transfer->m_protocol == PROTOCOL_BT)
				{
					DownloadTransferBT *pBT = (DownloadTransferBT*)transfer;
					if(pBT->m_state == dtsTorrent && pBT->m_choked) continue;
				}
				temp ++;

				if(transfer->m_state == dtsDownloading)
				{
					activetransfers ++;
				}
			}
			if(temp)
			{
				activedownloads ++;
				totaltransfers += temp;
			}
			worked = TRUE;
			break;
		}
		if(!worked) break;
	}
//	m_transfers = activetransfers;

//	m_allowmoredownloads = activedownloads < (UINT32)DOWNLOAD_MAXFILES;
//	m_allowmoretransfers = totaltransfers < (UINT32)DOWNLOAD_MAXTRANSFERS;
}


#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
