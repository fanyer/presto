/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#include "adjunct/desktop_util/search/tlddownloader.h"
#include "adjunct/desktop_util/search/tldupdater.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"

////////////////////////////////////////////////////////////////////////////////////////////////

TLDDownloader::TLDDownloader(TLDUpdater* tld_updater) : 
	m_tld_updater(tld_updater),
	m_transferItem(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

TLDDownloader::~TLDDownloader()
{
	// Clean up the transferitem
	if (m_transferItem && g_transferManager)
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS TLDDownloader::StartDownload()
{
	// If the request is already running no need to run it again
	if (m_status == INPROGRESS)
		return OpStatus::OK;
	
	if (m_transferItem)
	{
		// An instance of this class can only hold one transferitem, 
		// so it will refuse to start a second download until the first 
		// transferitem is released.
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Read preference
	OpStringC post_address = g_pcui->GetStringPref(PrefsCollectionUI::GoogleTLDServer);
	
	// Initiate transfer and listen in.
	RETURN_IF_ERROR(g_transferManager->GetTransferItem(&m_transferItem, post_address.CStr()));
	if (!m_transferItem)
	{
		m_status = NO_TRANSFERITEM;
		return OpStatus::ERR;
	}
	
	URL* url = m_transferItem->GetURL();
	if (!url)
	{
		m_status = NO_URL;
		return OpStatus::ERR;
	}
	
	url->SetHTTP_Method(HTTP_METHOD_GET);

	// Turn off UI interaction if the certificate is invalid.
	// This means that invalid certificates will not show a UI dialog but fail instead.
	RETURN_IF_ERROR(url->SetAttribute(URL::KBlockUserInteraction, TRUE));
	
	m_transferItem->SetTransferListener(this);

	URL tmp;
	if (url->Load(g_main_message_handler, tmp, TRUE, FALSE) != COMM_LOADING)
	{
		m_status = LOAD_FAILED;
		return OpStatus::ERR;
	}

	m_status = INPROGRESS;

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS TLDDownloader::StopRequest()
{
	if (m_transferItem)
	{
		m_transferItem->Stop();
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;		
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void TLDDownloader::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	switch (status)
	{
		case TRANSFER_ABORTED:
		{
			m_status = DOWNLOAD_ABORTED;
			break;
		}
		case TRANSFER_FAILED:
		{
			m_status = DOWNLOAD_FAILED;
			break;
		}
		case TRANSFER_DONE:
		{
			URL* url = transferItem->GetURL();
			if (url)
			{
				OpAutoPtr<URL_DataDescriptor> url_data_desc(url->GetDescriptor(NULL, TRUE));

				if (url_data_desc.get())
				{
					// Extract the data from the data descriptor.
					OP_MEMORY_VAR unsigned long len;
					BOOL more = FALSE;
					TRAPD(err, len = url_data_desc->RetrieveDataL(more));
					if (OpStatus::IsSuccess(err))
					{
						OpString data;
						
						data.Set((uni_char*)url_data_desc->GetBuffer(), UNICODE_DOWNSIZE(len));

						// Do a check to make sure the string looks ok has a .google. and isn't too long
						if (data.Find(".google.") != KNotFound && data.Length() < 30)
						{
							// Save to the prefs
							TRAPD(err1, g_pcui->WriteStringL(PrefsCollectionUI::GoogleTLDDefault, data.CStr()));
							TRAPD(err2, g_pcui->WriteIntegerL(PrefsCollectionUI::GoogleTLDDownloaded, 1));
							
							m_status = SUCCESS;
							break;
						}
					}
				}
			}

			m_status = DOWNLOAD_FAILED;
			break;
		}

		case TRANSFER_PROGRESS:
		default:
		{
			break;
		}
	}

	if (m_status == INPROGRESS)
	{
		// Transfer not complete, wait for a useful callback.
		return; 
	}
	else
	{
		// Transfer done
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;

		// Report an error so a new request can be scheduled
		if (m_status != SUCCESS)
			m_tld_updater->TLDDownloadFailed(m_status);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

