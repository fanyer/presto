/*
 *  tldupdater.cpp
 *  Opera
 *
 *  Created by Adam Minchinton
 *  Copyright 2009 Opera ASA. All rights reserved.
 *
 */

#include "core/pch.h"

#include "adjunct/desktop_util/search/tldupdater.h"
#include "adjunct/desktop_util/search/tlddownloader.h"

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
///                                                                                          ///
///  TLDUpdater                                                                             ///
///                                                                                          ///
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

#define GOOGLE_TLD_RETRY_DEFAULT   3600000  // make this 60 mins to start (in ms)

////////////////////////////////////////////////////////////////////////////////////////////////

TLDUpdater::TLDUpdater() :
	m_tld_downloader(NULL),
	m_update_check_timer(NULL),
	m_retry_interval(GOOGLE_TLD_RETRY_DEFAULT)	
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

TLDUpdater::~TLDUpdater()
{
	OP_DELETE(m_tld_downloader);
	OP_DELETE(m_update_check_timer);
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS TLDUpdater::StartDownload()
{
	// Make the object if it doesn't exist
	if (!m_tld_downloader)
	{
		// Start the attempted download of the tld file
		m_tld_downloader = OP_NEW(TLDDownloader, (this));
		if (!m_tld_downloader)
			return OpStatus::ERR_NO_MEMORY;
	}

	return m_tld_downloader->StartDownload();
}

////////////////////////////////////////////////////////////////////////////////////////////////

void TLDUpdater::TLDDownloadFailed(TLDDownloader::DownloadStatus status)
{
	// Download failed so reshedule a new download attempt

	// Make the object if it doesn't exist
	if (!m_update_check_timer)
	{
		// Start the attempted download of the tld file
		m_update_check_timer = OP_NEW(OpTimer, ());
		if (!m_update_check_timer)
			return;

		m_update_check_timer->SetTimerListener(this);
	}
	else
		m_update_check_timer->Stop();

	// Start the timer for the next check
	m_update_check_timer->Start(m_retry_interval);

	// Double the retry length after each start
	m_retry_interval *= 2;

	// Add fuzzing to the next retry interval so it's a bit random and doesn't bash the google server
	// Will add between 0 and 30 minutes to the next retry
	m_retry_interval += (op_rand() % (GOOGLE_TLD_RETRY_DEFAULT / 2));
}

////////////////////////////////////////////////////////////////////////////////////////////////

void TLDUpdater::OnTimeOut(OpTimer *timer)
{
	if (timer == m_update_check_timer)
	{
		StartDownload();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
