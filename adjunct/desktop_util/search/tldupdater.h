/*
 *  tldupdater.h
 *  Opera
 *
 *  Created by Adam Minchinton 
 *  Copyright 2008 Opera ASA. All rights reserved.
 *
 */

#ifndef _TLDUPDATER_H_INCLUDED_
#define _TLDUPDATER_H_INCLUDED_

#include "modules/hardcore/timer/optimer.h"
#include "adjunct/desktop_util/search/tlddownloader.h"

class TLDDownloader;

//////////////////////////////////////////////////////////////////////

class TLDUpdater : public OpTimerListener
{
public:
	TLDUpdater();
	virtual ~TLDUpdater();

	OP_STATUS StartDownload();

	void TLDDownloadFailed(TLDDownloader::DownloadStatus status);

protected:

	// OpTimerListener implementation
	virtual void OnTimeOut(OpTimer *timer);
	
private:
	TLDDownloader*			m_tld_downloader;		///< Holds the downloader that manages downloading of the tld.
	time_t					m_time_of_last_check;	///< Holds the time of the last call
	OpTimer*				m_update_check_timer;	///< Timer for rescheduling update check
	UINT32					m_retry_interval;		///< Holds the length before the next retry
};

#endif // _TLDUPDATER_H_INCLUDED_
