/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef REGION_CHECKER_H
#define REGION_CHECKER_H

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/autoupdater.h"


/**
 * Listener interface to be implemented by anyone using CountryChecker - the check is async and this way you will be notified back
 * when it finishes.
 */
class CountryCheckerListener
{
public:
	virtual ~CountryCheckerListener() {}

	/**
	 * Called when the country check is finished, succeeded or not.
	 */
	virtual void CountryCheckFinished() = 0;
};


/**
 * Specialized request to the autoupdate server that retrieves country code based on computer's IP address.
 */
class CountryChecker
: public StatusXMLDownloaderListener
, public OpTimerListener
{
public:
	enum RCStatus {
		/**
		 * The object has not been initialized at all, i.e. Init() was not called.
		 */
		NotInitialized,
		/**
		 * Something went wrong during the Init() call, the object can't be used.
		 */
		InitFailed,
		/**
		 * The object has been initialized and can be used to perform the check.
		 */
		CheckNotPerformed,
		/**
		 * Country code check is in progress, wait until you'll be notified via a CountryCheckerListener callback.
		 */
		CheckInProgress,
		/**
		 * Country code check failed for some reason, network or autoupdate server issue probably.
		 */
		CheckFailed,
		/**
		 * Country code check was cancelled after time out.
		 */
		CheckTimedOut,
		/**
		 * Country code check finished without errors and Opera received nonempty country code.
		 */
		CheckSucceded
	};

	CountryChecker();

	~CountryChecker();

	/**
	 * The CountryChecker object needs to be initialized before it can be used.
	 *
	 * @param listener - listener that will be notified when the country code check is over (may be NULL).
	 *
	 * @returns - OpStatus::OK if the object was initialized correctly, OpStatus::ERR otherwise.
	 */
	OP_STATUS Init(CountryCheckerListener* listener);

	/**
	 * Call this method to send country code request to the autoupdate server.
	 * If this method returns OpStatus::OK, you should wait for the callback via the CountryCheckerListener interface 
	 * before proceeding further, this is due to the time needed to communicate with the autoupdate server.
	 *
	 * @param timeout timeout for the request in ms (0 means no timeout)
	 *
	 * @returns - OpStatus::OK - communication with server in progress, wait for callback,
	 *            OpStatus::ERR - starting the server communication failed, don't expect the callback and proceed now.
	 */
	OP_STATUS CheckCountryCode(UINT32 timeout);

	/**
	 * Get the current status of the object, see the Status enum for description.
	 *
	 * @returns - the current status of the object.
	 */
	RCStatus GetStatus() const { return m_status; }
	
	/**
	 * Get country code received from the autoupdate server.
	 * If GetStatus returns CheckSucceeded then string returned by this function is not empty.
	 *
	 * @returns - country code received from the autoupdate server
	 */
	OpStringC GetCountryCode() { return m_country_code; }

	/**
	 * Implementation of StatusXMLDownloaderListener
	 */
	void StatusXMLDownloaded(StatusXMLDownloader* downloader);
	void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus);

	/**
	 * Implementation of OpTimerListener
	 */
	void OnTimeOut(OpTimer* timer);

private:

	/**
	 * Get address of server to be used for country check.
	 *
	 * @param address gets address of the server
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR if address is unknown.
	 */
	OP_STATUS GetServerAddress(OpString& address);

	/**
	 * The current status of the object.
	 */
	RCStatus m_status;

	/**
	 * The autoupdate request XML construction helper.
	 */
	AutoUpdateXML* m_autoupdate_xml;

	/**
	 * The autoupdate XML communication helper.
	 */
	StatusXMLDownloader* m_status_xml_downloader;

	/**
	 * The listener that will be notified about the succesfull region check.
	 */
	CountryCheckerListener* m_listener;

	/**
	 * Autoupdate server address helper.
	 */
	AutoUpdateServerURL* m_autoupdate_server_url;

	/**
	 * Country code received from the autoupdate server.
	 */
	OpString m_country_code;

	/**
	 * Aborts country check if it times out.
	 */
	OpTimer m_timer;
};

#endif // AUTO_UPDATE_SUPPORT

#endif // REGION_CHECKER_H
