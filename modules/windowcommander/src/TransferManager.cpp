/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WINDOW_COMMANDER_TRANSFER

#include "modules/windowcommander/src/TransferManager.h"
#include "modules/url/url_man.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/filefun.h"
#include "modules/url/url_tags.h"
#include "modules/formats/url_dfr.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_int.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/formats/uri_escape.h"

#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
# include "modules/prefs/prefsmanager/collections/pc_core.h"
# ifdef _BITTORRENT_SUPPORT_
#  include "adjunct/bittorrent/bt-download.h"
# endif // _BITTORRENT_SUPPORT_
# ifdef EXTERNAL_APPLICATIONS_SUPPORT
#  include "modules/prefs/prefsmanager/collections/pc_files.h"
# endif // EXTERNAL_APPLICATIONS_SUPPORT
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE

#ifdef WIC_USE_DOWNLOAD_CALLBACK
# include "modules/windowcommander/OpWindowCommander.h" // to define DownloadContext
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif // WIC_USE_DOWNLOAD_CALLBACK

// rg check so all members are initialized!
TransferItem::TransferItem()
   	    : m_listener(&m_nullTransferListener),
		  m_added(FALSE),
		  m_size(0), m_loaded(0), m_lastLoaded(0), m_uploaded(0), m_lastUploaded(0),
		  m_timeLeft(0),
		  m_kibs(0), m_kibsUpload(0),
		  m_lastStatus(OpTransferListener::TRANSFER_INITIAL),
		  m_action(ACTION_UNKNOWN),
		  m_connections_with_complete_file(0), m_connections_with_incomplete_file(0),
		  m_downloadConnections(0), m_uploadConnections(0),
		  m_calc_cps(FALSE), m_is_completed(FALSE),
		  m_total_with_complete_file(0),
		  m_total_leechers(0),
		  m_show_transfer(TRUE),
		  m_started_in_privacy_mode(FALSE),
#ifdef WIC_USE_DOWNLOAD_CALLBACK
		  m_download_context(NULL),
		  m_copy_when_downloaded(FALSE),
		  m_copy_when_downloaded_handler(NULL),
		  m_copy_when_downloaded_observer(this),
#endif
		  m_type(TRANSFERTYPE_DOWNLOAD),
		  m_pointpos(0),
		  m_pointposUpload(0)
{
#ifdef WIC_USE_DOWNLOAD_CALLBACK
	m_viewer.SetAction(VIEWER_NOT_DEFINED);
#endif
	m_timer.Get();
	for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
	{
		m_points[i].used = FALSE;
		m_pointsUpload[i].used = FALSE;
	}
}

TransferItem::~TransferItem()
{
	m_listener->SetTransferItem(0);
	Stop();
#ifdef WIC_USE_DOWNLOAD_CALLBACK
	OP_DELETE(m_download_context);
	UnsetCallbacks();
#endif // WIC_USE_DOWNLOAD_CALLBACK
	Out();	// make sure we're not in any list after this point
}

BOOL TransferItem::ItemAdded()
{
	if( !m_added )
	{
		m_added = TRUE;
		return FALSE;
	}
	return TRUE;
}

OP_STATUS TransferItem::Init(URL& url)
{
	m_url = url;
	m_lastLoaded =0;

	m_url->GetAttribute(URL::KContentLoaded, &m_lastLoaded);

	m_loaded = m_lastLoaded;
	m_uploaded = m_lastUploaded = 0;
	return OpStatus::OK;
}

void TransferItem::Stop()
{
	((TransferManager *)g_transferManager)->BroadcastStop(this);

	if( m_url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING &&
		(m_lastStatus == OpTransferListener::TRANSFER_INITIAL || m_lastStatus == OpTransferListener::TRANSFER_PROGRESS) )
		m_url->StopLoading(0);

	for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
	{
		m_points[i].used = FALSE;
		m_pointsUpload[i].used = FALSE;
	}
}

void TransferItem::AddPoint( double elapsed, OpFileLength bytes )
{
	m_points[m_pointpos].elapsed = elapsed;
	m_points[m_pointpos].bytes   = bytes;
	m_points[m_pointpos].used    = TRUE;

	if( ++m_pointpos >= FLOATING_AVG_SLOTS )
		m_pointpos = 0;
}

void TransferItem::AddPointUpload( double elapsed, OpFileLength bytes )
{
	m_pointsUpload[m_pointposUpload].elapsed = elapsed;
	m_pointsUpload[m_pointposUpload].bytes   = bytes;
	m_pointsUpload[m_pointposUpload].used    = TRUE;

	if( ++m_pointposUpload >= FLOATING_AVG_SLOTS )
		m_pointposUpload = 0;
}

double TransferItem::AverageSpeed()
{
	OpFileLength bytes = 0;
	double over = 0.0;
	for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
	{
		if( m_points[i].used )
		{
			over  += m_points[i].elapsed;
			bytes += m_points[i].bytes;
		}
	}

	// Quickly ramp up the 'kbps' rate once data starts arriving again.
	if( !bytes )
		for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
			m_points[i].used = FALSE;

	if( over == 0.0 )
		over = 1.0;
	return bytes / over;
}

double TransferItem::AverageSpeedUpload()
{
	OpFileLength bytes = 0;
	double over = 0.0;
	for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
	{
		if( m_pointsUpload[i].used )
		{
			over  += m_pointsUpload[i].elapsed;
			bytes += m_pointsUpload[i].bytes;
		}
	}

	// Quickly ramp up the 'kbps' rate once data starts arriving again.
	if( !bytes )
		for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
			m_pointsUpload[i].used = FALSE;

	if( over == 0.0 )
		over = 1.0;
	return bytes / over;
}

BOOL TransferItem::Continue()
{
	m_timer.Get();
	for( int i = 0; i<FLOATING_AVG_SLOTS; i++ )
	{
		m_points[i].used = FALSE;
		m_pointsUpload[i].used = FALSE;
	}
	((TransferManager *)g_transferManager)->BroadcastResume(this);

	if (m_url->GetAttribute(URL::KLoadStatus, TRUE) != URL_LOADING)
	{
		if (g_main_message_handler)
		{
			URL ref =
				m_url->GetAttribute(URL::KReferrerURL);

			if (m_loaded == 0) // this is a reload
			{
				m_url->SetAttribute(URL::KReloadSameTarget, TRUE);
				m_url->Reload( g_main_message_handler, ref, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE);
			}
			else // this is a continue
			{
				m_lastLoaded =0;
				m_url->GetAttribute(URL::KContentLoaded, &m_lastLoaded);

				m_loaded = m_lastLoaded;
				m_url->ResumeLoad(g_main_message_handler, ref, FALSE, TRUE, TRUE);
			}
			return TRUE;
		}
	}
	return m_url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADED;
}

void TransferItem::Clear()
{
	if (m_url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING)
	{
		Stop();
	}
	m_kibs = 0;
	m_kibsUpload = 0;
	m_timeLeft = m_pointpos = 0;
	m_size = m_loaded = m_lastLoaded = m_lastUploaded = m_uploaded = 0;
	m_lastStatus = OpTransferListener::TRANSFER_PROGRESS;
}

OP_STATUS TransferItem::SetFileName(const uni_char* filename)
{
	RETURN_IF_ERROR(m_storage_filename.Set(filename));
	unsigned long stat;
	stat = m_url->SaveAsFile( filename );
	return ConvertUrlStatusToOpStatus(stat);
}

OpTransferListener* TransferItem::SetTransferListener(OpTransferListener* listener)
{
	OpTransferListener* old_listener = NULL;

	if (m_listener != &m_nullTransferListener)
	{
		old_listener = m_listener;
	}

	if (listener == NULL)
	{
		m_listener = &m_nullTransferListener;
	}
	else
	{
		m_listener = listener;
		m_listener->SetTransferItem(this);
	}
	return old_listener;
}

void TransferItem::SetAction(TransferAction action)
{
	if (action == ACTION_UNKNOWN)
	{
		OP_ASSERT(FALSE); // can't be set, only initial state
	}
	else
	{
		m_action = action;
	}
}

void TransferItem::ResetStatus()
{
	m_lastStatus = OpTransferListener::TRANSFER_INITIAL;
	m_listener->OnReset(this);
}


void TransferItem::SetType(TransferItemType t)
{
	m_type = t;
}

const uni_char* TransferItem::GetUrlString()
{
	return m_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
}

void TransferItem::SetUrl(const uni_char* url)
{
	URL empty;
	URL temp_url = g_url_api->GetURL(empty, url, FALSE);
	m_url.SetURL(temp_url);
	if (!temp_url.GetAttribute(URL::KMultimedia))
		urlManager->MakeUnique(m_url.GetURL().GetRep());

	m_url->GetAttribute(URL::KContentLoaded, &m_loaded);

	m_lastLoaded = m_loaded;
}

void TransferItem::NotifyListener()
{
	OpTransferListener::TransferStatus status;
	URLStatus urlstatus;

	if(m_type == TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		switch(m_p2p_url.Status())
		{
			case URL_LOADING_FAILURE:
			case URL_UNLOADED:
 				status = OpTransferListener::TRANSFER_FAILED;
				break;

			case P2P_URL::P2P_CHECKING_FILES:
				status = OpTransferListener::TRANSFER_CHECKING_FILES;
				break;

			case P2P_URL::P2P_SHARING_FILES:
				status = OpTransferListener::TRANSFER_SHARING_FILES;
				break;

			case URL_LOADING_WAITING:
				if(m_is_completed)
				{
					status = OpTransferListener::TRANSFER_DONE;
					break;
				}
				else
				{
	 				status = OpTransferListener::TRANSFER_ABORTED;
				}
				break;

			case URL_LOADING_ABORTED:
 				status = OpTransferListener::TRANSFER_ABORTED;
				break;

			case URL_EMPTY:
			case URL_LOADED:
			case URL_LOADING_FROM_CACHE:
				{
					if(GetHaveSize() < GetSize())
					{
						status = OpTransferListener::TRANSFER_ABORTED;	//sizemismatch
					}
					else
					{
						status = OpTransferListener::TRANSFER_DONE;
					}
				}
				break;

			default:
				status = OpTransferListener::TRANSFER_PROGRESS;
				break;
		}
	}
	else
	{
		urlstatus = static_cast<URLStatus>(m_url->GetAttribute(URL::KLoadStatus));

		switch(urlstatus)
		{
			case URL_LOADING_FAILURE:
			case URL_UNLOADED:
 				status = OpTransferListener::TRANSFER_FAILED;
				break;

			case URL_LOADING_ABORTED:
 				status = OpTransferListener::TRANSFER_ABORTED;
				break;

			case URL_EMPTY:
			case URL_LOADED:
			case URL_LOADING_FROM_CACHE:
				{
					// First check if this is a redirect
					// 'moved_url' will be invalidated in Init() below before we use it (in URL_InUse::SetURL())
					// unless we copy it to a new object first.
					URL url;
					URL moved_url = m_url->GetAttribute(URL::KMovedToURL, TRUE);
					if( !moved_url.IsEmpty() )
					{
						url = moved_url;
						moved_url = url;
					}
					if (!moved_url.IsEmpty())
					{
						URL from_url = m_url;
						Init(moved_url);
						m_listener->OnRedirect(this, &from_url, &moved_url);
						status = OpTransferListener::TRANSFER_INITIAL;
					}
					else if(GetHaveSize() < GetSize())
					{
						status = OpTransferListener::TRANSFER_ABORTED;	//sizemismatch
					}
					else
					{
						status = OpTransferListener::TRANSFER_DONE;
					}
				}
				break;

			default:
				status = OpTransferListener::TRANSFER_PROGRESS;
				break;
		}
	}

	if (status == OpTransferListener::TRANSFER_PROGRESS &&
		status != m_lastStatus &&
		(m_lastStatus == OpTransferListener::TRANSFER_DONE ||
		 m_lastStatus == OpTransferListener::TRANSFER_FAILED ||
		 m_lastStatus == OpTransferListener::TRANSFER_ABORTED))
	{
		// this is the case when the url is reused from the inside.
		m_lastStatus = OpTransferListener::TRANSFER_INITIAL;
	}
	if(m_type == TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		m_lastLoaded = m_loaded;
		m_lastStatus = status;
		m_lastUploaded = m_uploaded;
		m_listener->OnProgress( this, status );
	}
	else if((m_lastLoaded != m_loaded) || (status != m_lastStatus))
	{
		m_lastLoaded = m_loaded;
		m_lastStatus = status;
		m_listener->OnProgress( this, status );
	}
}

double TransferItem::RelTimer::Get()
{
	unsigned long _new_sec, _new_msec;
	long new_sec, new_msec;
	double res;
	g_op_time_info->GetWallClock( _new_sec, _new_msec );
	new_sec = (long)_new_sec; new_msec = (long)_new_msec;
	res = (new_sec - last_sec) + (new_msec-last_msec)/1000.0;
	last_sec = new_sec;	last_msec = new_msec;
	return res;
}

void TransferItem::UpdateStats( )
{
	if(m_type != TRANSFERTYPE_CHAT_UPLOAD &&		// when uploading we set the size ourself with SetLoadedSize and SetCompleteSize
		m_type != TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		m_size = 0;
		m_url->GetAttribute(URL::KContentSize, &m_size);

		// Don't poll something that is done.
		BOOL update_loaded = FALSE;

		if (GetIsFinished())
			update_loaded = (m_loaded < m_size); // m_loaded might not necessarily be updated even if we're finished.
		else
		{
			if (m_type != TRANSFERTYPE_CHAT_DOWNLOAD)
				update_loaded = HeaderLoaded();
			else
				update_loaded = TRUE;
		}

		if (update_loaded)
		{
			m_url->GetAttribute(URL::KContentLoaded_Update, &m_loaded);
		}
	}
	double timer = m_timer.Get();
 	if(m_calc_cps)
	{
		if(m_lastLoaded > m_loaded)
		{
			m_lastLoaded = 0;
			m_loaded = 0;
		}
		AddPoint(timer, (m_loaded - m_lastLoaded));
		m_kibs = AverageSpeed();
		OP_ASSERT(m_kibs >= 0);
		if(m_kibs < 0)
		{
			m_kibs = 0;
		}
		if(m_kibs != 0)
		{
			m_timeLeft = (int)((m_size - m_loaded) / m_kibs);	// kibs is actually bps here.
		}
		m_kibs /= 1024.0;
	}
	AddPointUpload(timer, (m_uploaded - m_lastUploaded));
	m_kibsUpload = AverageSpeedUpload();
	m_kibsUpload /= 1024.0;

	if(m_loaded > 0)
	{
		m_calc_cps = TRUE;
	}
	NotifyListener();
}

void TransferItem::PutURLData(const char *data, unsigned int len)
{
	m_url->WriteDocumentData(URL::KNormal, data, len);
	NotifyListener();
}

void TransferItem::FinishedURLPutData()
{
	m_url->WriteDocumentDataFinished();
	NotifyListener();
}

BOOL TransferItem::HeaderLoaded()
{
	return !!m_url->GetAttribute(URL::KHeaderLoaded);
}

#ifdef WIC_USE_DOWNLOAD_CALLBACK

/*static*/
const OpMessage TransferItem::transferitem_messages[] =
		{
			MSG_URL_LOADING_FAILED,
			MSG_HEADER_LOADED,
			MSG_URL_DATA_LOADED,
			MSG_ALL_LOADED,
			MSG_NOT_MODIFIED,
			MSG_MULTIPART_RELOAD
		};


void TransferItem::CopyHandlerObserver::MessageHandlerDestroyed(MessageHandler* mh)
{
	if (m_transfer_item->m_copy_when_downloaded_handler == mh)
	{
		m_transfer_item->m_copy_when_downloaded_handler = NULL;
	}
}

void TransferItem::SetCallbacks(MessageHandler *handler, MH_PARAM_1 id)
{
	m_copy_when_downloaded_handler = handler;
	handler->AddObserver(&m_copy_when_downloaded_observer);
	handler->SetCallBackList(this, id, transferitem_messages, ARRAY_SIZE(transferitem_messages));
}

void TransferItem::UnsetCallbacks()
{
	if (m_copy_when_downloaded_handler)
	{
		m_copy_when_downloaded_handler->UnsetCallBacks(this);
		m_copy_when_downloaded_handler->RemoveObserver(&m_copy_when_downloaded_observer);
	}
	m_copy_when_downloaded_handler = NULL;
}

void TransferItem::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_LOADING_FAILED:
	case MSG_HEADER_LOADED:
	case MSG_URL_DATA_LOADED:
		{
			URLStatus status = m_url.GetURL().Status(TRUE);

			if( status == URL_LOADED )
			{
				if (m_copy_when_downloaded)
				{
					if (m_copy_to_filename.HasContent())
					{
						m_url.GetURL().SaveAsFile(m_copy_to_filename);
						m_copy_to_filename.Empty();
					}
					m_copy_when_downloaded = FALSE;
					UnsetCallbacks();
				}
			}
		}
		break;
	case MSG_NOT_MODIFIED:
		break;
	case MSG_MULTIPART_RELOAD:
		break;
	default:
		{
			// TODO: analyse why getting here....
		}
		break;
	}
}

URLInformation* TransferItem::CreateURLInformation()
{
	return OP_NEW(TransferManagerDownloadCallback, (NULL, m_url, NULL, ViewActionFlag()));
}

#endif //!WIC_USE_DOWNLOAD_CALLBACK

TransferManager::TransferManager()
		: m_timer(NULL)
{
}

OpTransferItem* TransferManager::GetTransferItem(int index)
{
	TransferItem* item = static_cast<TransferItem*>(itemList.First());

	while (item && index-- > 0)
	{
		item = static_cast<TransferItem*>(item->Suc());
	}

	return item;
}


int TransferManager::GetTransferItemCount()
{
	int count = 0;

	for (TransferItem* item = static_cast<TransferItem*>(itemList.First()); item != NULL; item = static_cast<TransferItem*>(item->Suc()))
	{
		count++;
	}

	return count;
}


TransferManager::~TransferManager()
{
#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
	WriteRescueFile();
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE
	// empty itemlist here
	TransferItem* item = NULL;
	while ((item = (TransferItem*)itemList.Last()) != NULL)
	{
		item->Stop();
		item->Out();
		OP_DELETE(item);
	}

    OP_DELETE(m_timer);
}

void TransferManager::InitL()
{
	m_timer = OP_NEW_L(OpTimer, ());
	m_timer->SetTimerListener(this);
}

OP_STATUS TransferManager::GetNewTransferItem(OpTransferItem** item, const uni_char* url)
{
	return GetTransferItem(item, url, NULL, TRUE);
}

OP_STATUS TransferManager::GetTransferItem(OpTransferItem** item, const uni_char* url, BOOL* already_created, BOOL create_new)
{
	// check if we have this url already, otherwise create a new item.
	if (url == NULL)
	{
		return OpStatus::ERR;
	}

	TransferItem* titem = NULL;

	if (!create_new)
	{
		for (titem = (TransferItem*)itemList.First() ; titem != NULL ; titem = (TransferItem*)titem->Suc())
		{
			// GetUrlString() returns a url-escaped string (see
			// KUniName_With_Fragment_Username_Password_NOT_FOR_UI for
			// details); url may be escaped or not (not for p2p download),
			// so deescape both before comparison:
			const uni_char* str = titem->GetUrlString();
			if (str && UriUnescape::strcmp(url, str, UriUnescape::All) == 0)
			{
				if(already_created)
				{
					*already_created = TRUE;
				}
				else	// do not "reset" the state if we are asking if the url has already been "gotten"
				{
					titem->ResetStatus();
				}
				break;
			}
		}
	}

	if (titem == NULL)
	{
		titem = OP_NEW(TransferItem, ());
		if (titem == NULL)
			return OpStatus::ERR_NO_MEMORY;
		titem->Into(&itemList);
		titem->SetUrl( url );
		if (already_created)
			*already_created = FALSE;
	}

	m_timer->Start( 33 );

	*item = titem;
	return OpStatus::OK;
}

BOOL TransferManager::HaveTransferItem(const uni_char * storage_filename)
{
	if (!storage_filename || !*storage_filename)
		return FALSE;

	TransferItem* titem;
	for (titem = (TransferItem*)itemList.First() ; titem != NULL ; titem = (TransferItem*)titem->Suc())
	{
		if (titem->GetStorageFilename()->Compare(storage_filename, KAll) == 0)
			return TRUE;
	}
	return FALSE;
}

void TransferManager::ReleaseTransferItem(OpTransferItem* item)
{
	TransferItem* t_item = (TransferItem *)item;
	OP_ASSERT(t_item != 0);

	// Notify all listeners.
	// An obvious thing to do for a listener is to remove itself within OnTransferItemRemoved,
	// and to prevent any listeners being skipped in that case, go through the vector backwards
	UINT count = m_listeners.GetCount();
	while (count)
	{
		count--;
		OpTransferManagerListener *listener = m_listeners.Get(count);
		if(listener)
		{
			if(listener->OnTransferItemCanDeleteFiles(t_item))
			{
				listener->OnTransferItemDeleteFiles(t_item);
			}
			listener->OnTransferItemRemoved(t_item);
		}
	}

	t_item->Stop();
	t_item->Out();
	OP_DELETE(t_item);
#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
	WriteRescueFile();
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE
}


OP_STATUS TransferManager::AddTransferManagerListener(OpTransferManagerListener* listener)
{
	OP_ASSERT(m_listeners.Find(listener) == -1);

	RETURN_IF_ERROR(m_listeners.Add(listener));
	return OpStatus::OK;
}


OP_STATUS TransferManager::RemoveTransferManagerListener(OpTransferManagerListener* listener)
{
	RETURN_IF_ERROR(m_listeners.RemoveByItem(listener));
	return OpStatus::OK;
}


void TransferManager::OnTimeOut(OpTimer* timer)
{
	TransferItem *item, *next;

	for(int i=0; i<GetTransferItemCount() && (item = (TransferItem*)GetTransferItem(i)); i++)
	{
		item->UpdateStats(); // Might conceivably cause the item to be destroyed.
	}

	BOOL ongoing_transfer = FALSE;
	item = (TransferItem *)itemList.First();
	while ( item )
	{
		next = (TransferItem *)item->Suc();

		if (g_windowManager->GetNumberOfWindowsInPrivacyMode() == 0 && item->GetIsFinished() && item->GetStartedInPrivacyMode())
			ReleaseTransferItem(item);
		else if (!item->GetIsFinished())
			ongoing_transfer = TRUE;

		item = next;
	}

	if( ongoing_transfer )
	{
		m_timer->Start( 333 ); // ~3 times / second
	}
	else
	{
		m_timer->Stop( );
	}

}

OP_STATUS TransferManager::AddTransferItem( URL& url,
											const uni_char* filename,
											OpTransferItem::TransferAction action,	// = OpTransferItem::ACTION_UNKNOWN
											BOOL is_populating,						// = FALSE
											double last_speed,						// = 0
											TransferItem::TransferItemType type,	// = TransferItem::TRANSFERTYPE_DOWNLOAD
											OpString *meta_file,					// = NULL
											OpString *download_directory,			// = NULL
											BOOL completed,							// = FALSE
											BOOL show_transfer,						// = TRUE
											BOOL started_in_privacy_mode,			// = FALSE
											TransferItem** out_item)				// = NULL
{
	BOOL p2p_download = (type == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD);
	OpString url_str;
	OP_STATUS status = OpStatus::OK;

	if(p2p_download)
		status = url_str.Set(filename);
	else
		status = url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, url_str);

	if (OpStatus::IsError(status))
		return status;

	TransferItem *t_item = GetTransferItemByFilename(filename);
	if (!t_item)
	{
		t_item = OP_NEW(TransferItem, ());
		if (t_item == NULL)
			return OpStatus::ERR_NO_MEMORY;
		t_item->Into(&itemList);
		t_item->SetUrl(url_str.CStr());
	}

	m_timer->Start( 33 );

	// we should never move a target filename of a running transfer. Ref. bug 154370.
	if (t_item->GetStorageFilename()->HasContent() && t_item->GetIsRunning())
		return OpStatus::ERR;

	status = t_item->SetStorageFilename(filename);

	if (OpStatus::IsSuccess(status))
	{
		status = t_item->Init(url);
		t_item->SetType(type);
	}

	if (OpStatus::IsError(status))
	{
		ReleaseTransferItem(t_item);
		return status;
	}

	if(meta_file != NULL)
	{
		t_item->SetMetaFile(*meta_file);
	}
	if(download_directory != NULL)
	{
		t_item->SetDownloadDirectory(*download_directory);
	}
	if (action != OpTransferItem::ACTION_UNKNOWN)
	{
		t_item->SetAction(action);
	}

	t_item->SetCompleted(completed);

	t_item->SetStartedInPrivacyMode(started_in_privacy_mode);

	if (!t_item->ItemAdded())
	{
		t_item->SetShowTransfer(show_transfer);
		// Notify all listeners.
		for (UINT index = 0, count = m_listeners.GetCount(); index < count; ++index)
		{
			 //TODO: does not use the return value, should throw away item if none wants it (returns FALSE).
			m_listeners.Get(index)->OnTransferItemAdded(t_item, is_populating, last_speed);
		}
#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
		WriteRescueFile();
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE
	}

	if (out_item)
	{
		*out_item = t_item;
	}
	return OpStatus::OK;
}

TransferItem* TransferManager::GetTransferItemByFilename(const uni_char* filename)
{
	/* FIXME: Use OpSystemInfo::PathsEqual() instead of regular string
	   comparison (and do so everywhere in this file when comparing file
	   names). */

	for (TransferItem* titem = (TransferItem*) itemList.First(); titem; titem = (TransferItem*) titem->Suc())
		if (titem->GetStorageFilename()->Compare(filename) == 0)
			return titem;

	return NULL;
}

void TransferManager::BroadcastResume(OpTransferItem *item)
{
	UINT index;

	m_timer->Start( 33 );

	// Notify all listeners.
	for(index = 0; index < m_listeners.GetCount(); ++index)
	{
		m_listeners.Get(index)->OnTransferItemResumed(item);
	}
}

void TransferManager::BroadcastStop(OpTransferItem *item)
{
	UINT index;

	// Notify all listeners.
	for(index = 0; index < m_listeners.GetCount(); ++index)
	{
		m_listeners.Get(index)->OnTransferItemStopped(item);
	}
}

void TransferManager::RemovePrivacyModeTransfers()
{
	TransferItem* titem = (TransferItem*) itemList.First();
	while (titem)
	{
		TransferItem* next_titem = (TransferItem*) titem->Suc();
		if (titem->GetStartedInPrivacyMode() && titem->GetIsFinished())
			ReleaseTransferItem(titem);
		titem = next_titem;
	}
}

/*static*/
OpTransferManager* OpTransferManager::CreateL()
{
	TransferManager *manager = OP_NEW_L(TransferManager, ());
	manager->InitL();
	return manager;
}

#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
OP_STATUS TransferManager::WriteRescueFile()
{
	if (m_rescue_filename.IsEmpty())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

    BOOL p2p_download;
    OP_STATUS ret;

    OpString tmp_storage;
    const OpStringC opdir(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
    OpString dtmp;
    dtmp.Set(opdir.CStr());

    if (opdir[opdir.Length()-1] != PATHSEPCHAR)
		dtmp.Append(PATHSEP);

    dtmp.Append(m_rescue_filename);

    OpFile *dlfp = OP_NEW(OpFile, ());
    if (!dlfp)
		return OpStatus::ERR_NO_MEMORY;

    OpStackAutoPtr<OpFile> ap_dlfp(dlfp);

    RETURN_IF_ERROR(dlfp->Construct(dtmp.CStr()));
    RETURN_IF_ERROR(dlfp->Open(OPFILE_WRITE));

    // dload_rescue_file takes ownership of dlfp !!!! DO NOT DELETE !!!!
    DataFile dload_rescue_file(ap_dlfp.release(), CURRENT_CACHE_VERSION, 1, 2);
    TransferItem* OP_MEMORY_VAR transferItem;
    OpStackAutoPtr<DataFile_Record> url_rec(NULL);

    TRAP(ret, dload_rescue_file.InitL());
    RETURN_IF_ERROR(ret);

    for(transferItem = static_cast<TransferItem*>(itemList.First()); transferItem; transferItem = static_cast<TransferItem*>(transferItem->Suc()))
    {
		URL url;

		if (transferItem->GetType() == TransferItem::TRANSFERTYPE_WEBFEED_DOWNLOAD)
			continue;

		if (transferItem->GetStartedInPrivacyMode())
			continue;

		p2p_download = (transferItem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD);

		if(!p2p_download)
		{
			URL *t_url = transferItem->GetURL();
			if (!t_url || t_url->IsEmpty())
			{
				continue;
			}

			url = *t_url;
			URL temp = url.GetAttribute(URL::KMovedToURL);
			if(!temp.IsEmpty())
				url = temp;

			if(url.GetAttribute(URL::KHTTPIsFormsRequest))
			{
				continue;
			}
		}

		DataFile_Record *new_rec = OP_NEW(DataFile_Record, (TAG_DOWNLOAD_RESCUE_FILE_ENTRY));
		if (!new_rec)
			return OpStatus::ERR_NO_MEMORY;

		url_rec.reset(new_rec);
		new_rec->SetRecordSpec(dload_rescue_file.GetRecordSpec());

		if(!p2p_download)
		{
			TRAP(ret, url.WriteCacheDataL(new_rec));
			RETURN_IF_ERROR(ret);
		}

		union {
			double dbl;
			struct { unsigned int start, stop; } tin;
		} speed;

		// TODO: add to TransferItem..
		// speed.dbl = m_transferitems.Get(i)->GetLastKnownSpeed();
		speed.dbl = 0;

		TRAP(ret, new_rec->AddRecordL(TAG_DOWNLOAD_START_TIME, speed.tin.start);
			 new_rec->AddRecordL(TAG_DOWNLOAD_STOP_TIME, speed.tin.stop));
		RETURN_IF_ERROR(ret);

		if(!transferItem->GetShowTransfer())
		{
			TRAP_AND_RETURN(ret, new_rec->AddRecordL(TAG_DOWNLOAD_HIDE_TRANSFER, TRUE));
		}

		if(p2p_download)
		{
			OpString *name = transferItem->GetStorageFilename();

			if(name)
			{
				OpString8 storname;

				RETURN_IF_ERROR(storname.SetUTF8FromUTF16(name->CStr()));

				// reuse a cache tag name for non-ftp/http downloads
				TRAP(ret, new_rec->AddRecordL(TAG_DOWNLOAD_FRIENDLY_NAME, storname.CStr()));
				RETURN_IF_ERROR(ret);
			}

			OpString8 meta_file;
			OpString8 download_directory;
			OpString tmpchar;

			transferItem->GetMetaFile(tmpchar);
			RETURN_IF_ERROR(meta_file.SetUTF8FromUTF16(tmpchar.CStr()));

			transferItem->GetDownloadDirectory(tmpchar);
			RETURN_IF_ERROR(download_directory.SetUTF8FromUTF16(tmpchar.CStr()));

			TRAP(ret,
				 new_rec->AddRecordL(TAG_DOWNLOAD_TRANSFERTYPE, (unsigned int)transferItem->GetType());
				 new_rec->AddRecordL(TAG_DOWNLOAD_METAFILE, meta_file.CStr());
				 new_rec->AddRecordL(TAG_DOWNLOAD_LOCATION, download_directory.CStr());
				 new_rec->AddRecordL(TAG_DOWNLOAD_COMPLETION_STATUS, (unsigned int)transferItem->GetIsCompleted() ? 1 : 0));

			RETURN_IF_ERROR(ret);
		}

		TRAP(ret, new_rec->WriteRecordL(&dload_rescue_file));

		RETURN_IF_ERROR(ret);

		url_rec.reset();
    }

    // dload_rescue_file destructor destroys dlfp

    return OpStatus::OK;
}

OP_STATUS TransferManager::SetRescueFileName(const uni_char * rescue_filename)
{
	RETURN_IF_ERROR(m_rescue_filename.Set(rescue_filename));
	OP_STATUS ret = ReadRescueFile();
	if (ret == OpStatus::ERR_NO_SUCH_RESOURCE)
		return OpStatus::OK; // if no file exists, still return OK

	return ret;
}

OP_STATUS TransferManager::ReadRescueFile()
{
	if (m_rescue_filename.IsEmpty())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

    BOOL p2p_download;
    OP_STATUS ret;
    OpString tmp_storage;
    const OpStringC opdir(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
    OpString dtmp;
    RETURN_IF_ERROR(dtmp.Set(opdir.CStr()));

    if (opdir[opdir.Length()-1] != PATHSEPCHAR)
	{
		RETURN_IF_ERROR(dtmp.Append(PATHSEP));
	}

    RETURN_IF_ERROR(dtmp.Append(m_rescue_filename));

    OpFile *dlfp = OP_NEW(OpFile, ());
    if (!dlfp)
		return OpStatus::ERR_NO_MEMORY;

    OpStackAutoPtr<OpFile> ap_dlfp(dlfp);

    RETURN_IF_ERROR(dlfp->Construct(dtmp.CStr()));
    RETURN_IF_ERROR(dlfp->Open(OPFILE_READ));

    DataFile dload_rescue_file(ap_dlfp.release());

    // dload_rescue_file destructor destroys dlfp

    TRAP(ret, dload_rescue_file.InitL());
    RETURN_IF_ERROR(ret);

    OpStackAutoPtr<DataFile_Record> url_rec(NULL);

    FileName_Store dummy(1);

    RETURN_IF_ERROR(dummy.Construct());

    while(1)
    {
		TRAP(ret, url_rec.reset(dload_rescue_file.GetNextRecordL()));
		RETURN_IF_ERROR(ret);

		if (!url_rec.get())
		{
			break;
		}

		if(url_rec->GetTag() != TAG_DOWNLOAD_RESCUE_FILE_ENTRY)
		{
			continue;
		}

		URL url;
		TransferItem::TransferItemType type;

		url_rec->SetRecordSpec(dload_rescue_file.GetRecordSpec());
		TRAP(ret, url_rec->IndexRecordsL());
		RETURN_IF_ERROR(ret);

		type = (TransferItem::TransferItemType)url_rec->GetIndexedRecordUIntL(TAG_DOWNLOAD_TRANSFERTYPE);
		p2p_download = (type == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD);

		if(!p2p_download)
		{
			url = urlManager->GetURL(url_rec.get(), dummy, TRUE);
			if (url.IsEmpty())
			{
				continue;
			}
			time_t timestamp = 0;
			url.GetAttribute(URL::KVLocalTimeLoaded, &timestamp);
			if (!url.GetAttribute(URL::KDataPresent)
				|| timestamp + g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinLogEntryDaysToLive)*24*60*60 < g_timecache->CurrentTime()
				)
			{
				url.Unload();
				continue;
			}

			if((URLStatus) url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
				url.SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
		}

		OP_MEMORY_VAR union {
			double dbl;
			struct { unsigned int start, stop; } tin;
		} speed;

		speed.tin.start = url_rec->GetIndexedRecordUIntL(TAG_DOWNLOAD_START_TIME);
		speed.tin.stop  = url_rec->GetIndexedRecordUIntL(TAG_DOWNLOAD_STOP_TIME);

		if(!p2p_download)
		{
			OpString tempname;

			TRAP(ret, url.GetAttributeL(URL::KFilePathName_L, tempname));
			RETURN_IF_ERROR(ret);

			BOOL hide = url_rec->GetIndexedRecordUIntL(TAG_DOWNLOAD_HIDE_TRANSFER);

			AddTransferItem(url,
							tempname.CStr(),
							OpTransferItem::ACTION_UNKNOWN,
							TRUE,
							speed.dbl,
							TransferItem::TRANSFERTYPE_DOWNLOAD,
							NULL, // no meta_file
							NULL, // no download directory
							FALSE,// not completed
							!hide);// show transfer unless we have hide tag
		}
		else
		{
			OpString urlstring;
			OpString tempname;
			unsigned int completed = 0;

			OpString metafile, download_directory;
			OpString8 temp;

			TRAP(ret, url_rec->GetIndexedRecordStringL(TAG_DOWNLOAD_FRIENDLY_NAME, temp));
			RETURN_IF_ERROR(ret);
			if (temp.IsEmpty())
			{
				// skip old records which didn't use this tag
				continue;
			}
			RETURN_IF_ERROR(urlstring.SetFromUTF8(temp));

			TRAP(ret, url_rec->GetIndexedRecordStringL(TAG_DOWNLOAD_METAFILE, temp));
			RETURN_IF_ERROR(ret);
			RETURN_IF_ERROR(metafile.SetFromUTF8(temp));

			TRAP(ret, url_rec->GetIndexedRecordStringL(TAG_DOWNLOAD_LOCATION,temp));
			RETURN_IF_ERROR(ret);
			RETURN_IF_ERROR(download_directory.SetFromUTF8(temp));

			completed = url_rec->GetIndexedRecordUIntL(TAG_DOWNLOAD_COMPLETION_STATUS);

			// skip duplicated bittorrent record. some old versions Opera will incorrectly write a extra empty record
			// right after the original one. see DSK-209226 for details.
			if(HaveTransferItem(urlstring) && !metafile.HasContent() && !download_directory.HasContent())
				continue;

#ifdef _BITTORRENT_SUPPORT_
			BTDownload *download = OP_NEW(BTDownload, ());
			if(download)
			{
				g_Downloads->Add(download);
				download->StartListening();

				RETURN_IF_ERROR( AddTransferItem(url,
								   urlstring.CStr(),
								   OpTransferItem::ACTION_UNKNOWN,
								   TRUE,
								   speed.dbl,
								   type,
								   &metafile,
								   &download_directory,
								   completed ? TRUE : FALSE));
			}
#else

			RETURN_IF_ERROR( AddTransferItem(url,
							   urlstring.CStr(),
							   OpTransferItem::ACTION_UNKNOWN,
							   TRUE,
							   speed.dbl,
							   type,
							   &metafile,
							   &download_directory));
#endif // _BITTORRENT_SUPPORT_
		}
    }

#ifdef EXTERNAL_APPLICATIONS_SUPPORT
    /* Then we flush OPFILE_TEMPDOWNLOAD_FOLDER for files
     * that have no entry in the rescue file
     */

	// Check if the OPFILE_TEMPDOWNLOAD_FOLDER is still pointing to it's default location
	OpString tmp_storage2;
	const uni_char *temp_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TEMPDOWNLOAD_FOLDER, tmp_storage2);
	OpString temp_folder_original;
	RETURN_IF_LEAVE(g_pcfiles->GetDefaultDirectoryPrefL(OPFILE_TEMPDOWNLOAD_FOLDER, temp_folder_original));
	if (temp_folder_original.Compare(temp_folder) == 0)
	{
		// delete the temp download folder
		OpFolderLister *dir = OpFile::GetFolderLister(OPFILE_TEMPDOWNLOAD_FOLDER, UNI_L("*"), NULL);
		if (dir)
		{
			while(dir && dir->Next())
			{
				// Get each entry, don't handle . and ..

				if( dir->IsFolder() )
					continue;
				if (uni_strcmp(dir->GetFileName(), UNI_L(".")) == 0)
					continue;
				if (uni_strcmp(dir->GetFileName(), UNI_L("..")) == 0)
					continue;

				OpFile file;
				if (OpStatus::IsError(file.Construct(dir->GetFullPath())))
				{
					continue;
				}
				if (!HaveTransferItem(file.GetFullPath()))
					file.Delete();
			}
			OP_DELETE(dir);
		}
	}
#endif // EXTERNAL_APPLICATIONS_SUPPORT

    return OpStatus::OK;
}
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE

#endif // WINDOW_COMMANDER_TRANSFER
