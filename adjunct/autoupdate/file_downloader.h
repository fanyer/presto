/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*
*	Per Arne Vollan (pav)
*/

#ifndef FILE_DOWNLOADER_H
#define FILE_DOWNLOADER_H

#ifdef AUTO_UPDATE_SUPPORT

#include "modules/windowcommander/OpTransferManager.h"

class FileDownloader;

/**
 * Listener class for downloading files
 */
class FileDownloadListener
{
public:
	virtual ~FileDownloadListener() {}
	/**
	 * Called when file download is done
	 */
	virtual void OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size) = 0;
	/**
	 * Called when file download failed
	 */
	virtual void OnFileDownloadFailed(FileDownloader* file_downloader) = 0;
	/**
	 * Called when file download was aborted
	 */
	virtual void OnFileDownloadAborted(FileDownloader* file_downloader) = 0;
	/**
	 * Called when download progresses
	 */
	virtual void OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate) = 0;
};


/**
 * Class for downloading files
 */
class FileDownloader : public OpTransferListener, public MessageObject
{
public:
	FileDownloader();
	~FileDownloader();

	void		SetFileDownloadListener(FileDownloadListener* listener) { m_listener = listener; }

	OP_STATUS	StartDownload(const OpStringC &url, const OpStringC &target_filename);
	OP_STATUS	StopDownload();

	BOOL		DownloadStarted() const { return m_download_started; }
	BOOL		Downloaded() const;
	BOOL		DownloadFailed() const;

	OP_STATUS	GetTargetFilename(OpString &target_filename) { return target_filename.Set(m_file_name); }

	OpFileLength GetFileSize();
	OpFileLength GetHaveSize();

protected:
	void		CleanUp();	
	OP_STATUS	DeleteDownloadedFile();

	virtual OpFileLength GetOverridedFileLength() { return 0; }

	OpString	m_file_name;							///< Name of the file being downloaded

private:
	// OpTransferListener
	// The method handles progress messages from the transfer that the file downloader has initiated.
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem) {}
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);

	// MessageObject override
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OpTransferItem*			m_transferItem;							///< A reference to the TransferItem that represents the transfer initiated by this class.
	FileDownloadListener*	m_listener;								///< Reference to the listener object
	TransferStatus			m_url_status;							///< Status of url loading
	BOOL					m_download_started;						///< Set if the download has started
	URL						m_download_url;							///< Download url
	OpString				m_url_string;							///< String of url to be loaded
};
#endif //AUTO_UPDATE_SUPPORT
#endif //FILE_DOWNLOADER_H
