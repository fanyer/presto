/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*
*	Per Arne Vollan (pav)
*/

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/file_downloader.h"
#include "adjunct/autoupdate/statusxmldownloader.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/url/url2.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/util/opfile/opfile.h"

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
///                                                                                          ///
///  FileDownloader                                                                          ///
///                                                                                          ///
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

FileDownloader::FileDownloader()
: m_transferItem(NULL),
  m_listener(NULL),
  m_url_status(TRANSFER_INITIAL),
  m_download_started(FALSE)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

FileDownloader::~FileDownloader()
{
	if (m_transferItem) 
	{
		m_transferItem->SetTransferListener(NULL);
	}
	g_main_message_handler->UnsetCallBack(this, MSG_HEADER_LOADED);
	g_main_message_handler->UnsetCallBack(this, MSG_URL_MOVED);
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS FileDownloader::StartDownload(const OpStringC &url, const OpStringC &target_filename)
{
	OpStatus::Ignore(m_url_string.Set(url));

	// Hold the downloaded filename
	m_file_name.Set(target_filename);

	// Set initial status
	m_url_status = TRANSFER_INITIAL;
	
	if(m_transferItem) 
	{
		if(m_transferItem->GetURL() && m_transferItem->GetURL()->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADED)
		{
			m_url_status = TRANSFER_DONE;
			OnProgress(m_transferItem, TRANSFER_DONE);
			return OpStatus::OK;
		}
		BOOL ret = m_transferItem->Continue();
		return ret? OpStatus::OK : OpStatus::ERR;
	}

	// Create the transfer object
	BOOL already_created = FALSE;
	RETURN_IF_ERROR(g_transferManager->GetTransferItem(&m_transferItem, url.CStr(), &already_created));
	if (m_transferItem)
	{
		static_cast<TransferItem*>(m_transferItem)->SetShowTransfer(FALSE);
		
		URL* url = m_transferItem->GetURL();
		if (url)
		{
			// Set the listener and start the download
			m_transferItem->SetTransferListener(this);
			
			m_download_url = *url;

			url->SetAttribute(URL::KOverrideRedirectDisabled, TRUE);
			g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, url->Id());
			g_main_message_handler->SetCallBack(this, MSG_URL_MOVED, url->Id());

			m_download_started = TRUE;

			if(already_created)
			{
				if(m_transferItem->GetURL()->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADED)
				{
					m_url_status = TRANSFER_DONE;
					OnProgress(m_transferItem, TRANSFER_DONE);
					return OpStatus::OK;
				}
				static_cast<TransferItem*>(m_transferItem)->ResetStatus();
				BOOL ret = m_transferItem->Continue();
				if(ret)
				{
					return OpStatus::OK;
				}
			}

			DeleteDownloadedFile();

			if(url->Load(g_main_message_handler, URL()) == COMM_LOADING)
			{
				return OpStatus::OK;
			}
		}
	}
	
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS FileDownloader::StopDownload()
{
	if(m_transferItem)
	{
		m_transferItem->Stop();
	}
	m_download_started = FALSE;
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL FileDownloader::Downloaded() const
{
	return m_url_status == TRANSFER_DONE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL FileDownloader::DownloadFailed() const
{
	return m_url_status == TRANSFER_ABORTED || m_url_status == TRANSFER_FAILED;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OpFileLength FileDownloader::GetFileSize()
{
	OP_ASSERT(m_transferItem);

	// The downloaded file size may come from the server that is hosting it, the size is then sent in 
	// the HTTP headers. It may also be overrided by a class that implements the GetOverridedFileLength()
	// method.
	// In case we get a size from the HTTP server, we prefer that, since we trust it is valid. However in case
	// there are no HTTP headers with the size, we try to use the size coming from the override.
	OpFileLength size_from_url = m_transferItem->GetSize();
	if (0 == size_from_url)
		return GetOverridedFileLength();
	else
		return size_from_url;
}

OpFileLength FileDownloader::GetHaveSize()
{
	if (m_transferItem)
		return m_transferItem->GetHaveSize();

	return 0;
}

void FileDownloader::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	OP_ASSERT(transferItem == m_transferItem);

	m_url_status = status;

	switch (status)
	{
		case TRANSFER_PROGRESS:
		{
			OpFileLength total_size = GetFileSize();
			OpFileLength downloaded_size = transferItem->GetHaveSize();

			// It may happen that the size returned by GetFileSize() comes from the override and that the
			// overrided size is smaller than the download size. In that case we return the greater value 
			// so that the caller never receives an update about a progress that is greater than 100%.
			if (downloaded_size > total_size)
				total_size = downloaded_size;

			double kbps = transferItem->GetKbps();
			unsigned long time_estimate = transferItem->GetTimeEstimate();
			if(m_listener)
			{
				m_listener->OnFileDownloadProgress(this, total_size, downloaded_size, kbps, time_estimate);
			}
			break;
		}
		case TRANSFER_DONE:
		{
			// Reset the KOverrideRedirectDisabled
			transferItem->GetURL()->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);
						
			if(m_listener)
			{
				OpFileLength total_size = GetFileSize();
				OpFileLength downloaded_size = transferItem->GetHaveSize();

				// It may happen that the size returned by GetFileSize() comes from the override and that the
				// overrided size is smaller than the download size. In that case we return the greater value 
				// so that the caller never receives an update about a progress that is greater than 100%.

				if (downloaded_size > total_size)
					total_size = downloaded_size;
			
				double kbps = transferItem->GetKbps();
				unsigned long time_estimate = 0;
				m_listener->OnFileDownloadProgress(this, total_size, downloaded_size, kbps, time_estimate);

				m_listener->OnFileDownloadDone(this, total_size);
			}
			break;
		}
			
		case TRANSFER_ABORTED:
		{
			if(m_listener)
			{
				m_listener->OnFileDownloadAborted(this);
			}
			break;
		}
		case TRANSFER_FAILED:
		{
			// Reset the KOverrideRedirectDisabled
			transferItem->GetURL()->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);

			if(m_listener)
			{
				m_listener->OnFileDownloadFailed(this);
			}
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FileDownloader::OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FileDownloader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
		case MSG_URL_MOVED:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_HEADER_LOADED);
			g_main_message_handler->UnsetCallBack(this, MSG_URL_MOVED);
			g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, par2);
			g_main_message_handler->SetCallBack(this, MSG_URL_MOVED, par2);
			URL moved_url;
			moved_url = m_download_url.GetAttribute(URL::KMovedToURL, TRUE);
			m_download_url = moved_url;

			break;
		}
		case MSG_HEADER_LOADED:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_HEADER_LOADED);
			g_main_message_handler->UnsetCallBack(this, MSG_URL_MOVED);
			m_download_url.LoadToFile(m_file_name);
			OpString url;
			url.Set(m_download_url.GetUniName(TRUE, PASSWORD_HIDDEN));
			if(m_url_string.Compare(url) != 0)
			{
				StatusXMLDownloaderHolder::ReplaceRedirectedFileURLs(m_url_string, url);
			}
			break;
		}
	}	
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FileDownloader::CleanUp()
{
	if(m_transferItem)
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS FileDownloader::DeleteDownloadedFile()
{
	OpFile f;
	RETURN_IF_ERROR(f.Construct(m_file_name.CStr(), OPFILE_ABSOLUTE_FOLDER));
	return f.Delete(FALSE);
}

////////////////////////////////////////////////////////////////////////////////////////////////

#endif //AUTO_UPDATE_SUPPORT
