/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _TLDDOWNLOADER_H_INCLUDED
#define _TLDDOWNLOADER_H_INCLUDED

#include "modules/windowcommander/OpTransferManager.h"

class TLDUpdater;

class TLDDownloader : public OpTransferListener
{
public:
	// Enum of states, can be used to tell what went wrong if download or parsing fails
	enum DownloadStatus
	{
		READY,
		INPROGRESS,
		NO_TRANSFERITEM,
		NO_URL,
		LOAD_FAILED,
		DOWNLOAD_FAILED,
		DOWNLOAD_ABORTED,
		PARSE_ERROR,
		WRONG_XML,
		OUT_OF_MEMORY,
		SUCCESS
	};

	/**
	 * Create the downloader for the tdl document. Supply
	 * a pointer to the TLDUpdater that will receive the callback 
	 * when the download is done.
	 */
	TLDDownloader(TLDUpdater* tld_updater);
	~TLDDownloader();

	/**
	 * Call this to start downloading of the tld file. 
	 *
	 */
	OP_STATUS StartDownload();

	/**
	 * Call this function to stop the request
	 *
	 */
	OP_STATUS StopRequest();	
	
private:
	TLDUpdater*			m_tld_updater;		// Pointer to the TLDUpdater oject for callback on download
	OpTransferItem*		m_transferItem;		// Transfer item
	DownloadStatus		m_status;			// Status of the download

	// OpTransferListener implementation
	virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status);					
	virtual void OnReset(OpTransferItem* transferItem) {}											
	virtual void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {}	

};

#endif // _TLDDOWNLOADER_H_INCLUDED
