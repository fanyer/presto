/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
 */

#ifndef _ST_AUTOUPDATER_H_INCLUDED_
#define _ST_AUTOUPDATER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/selftest/src/ST_autoupdatexml.h"

/**
 * This class is the base class for the auto update functionallity.
 *
 * It should contain all code to do any platform/product independent 
 * functionallity such as version comparisons, xml parsing, 
 * downloading of files etc.
 *
 * @see platformupdater.h For the interface used to manage updates on the platform.
 * @see updatenotifier.h For the interface used to manage UI on the platform.
 *
*/
class ST_AutoUpdater : public AutoUpdater, public AutoUpdater::AutoUpdateListener
{
public:
	enum TestType
	{
		Basic,
		VersionCheck,
		SpoofCheck,
		BrowserJSCheck,
		HardwareBlocklistCheck,
		XMLCheck,
		DownloadCheck,
		SignatureCheck
	};

	ST_AutoUpdater(TestType test_type = Basic);
	virtual ~ST_AutoUpdater();

	// Overridden functions from AutoUpdater
	virtual OP_STATUS Init();

	OP_STATUS ReplaceAutoUpdateXMLForST(ST_AutoUpdateXML** st_autoupdate_xml);

	virtual int CalculateTimeOfNextUpdateCheck(ScheduleCheckType schedule_type);

	virtual void StatusXMLDownloaded(StatusXMLDownloader* downloader);
	virtual void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus);

	virtual OP_STATUS CheckAndUpdateState();

	virtual void OnTimeOut( OpTimer *timer );

	virtual BOOL HasDownloadedNewerVersion() { return FALSE; }

	// AutoUpdateListener
	virtual void OnUpToDate(BOOL automated);
	virtual void OnChecking(BOOL automated) {}
	virtual void OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL automated);

	virtual void OnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL automated) {}
	virtual void OnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent) {}
	virtual void OnDownloadingFailed(BOOL automated) {}
	virtual void OnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL automated) {}
	virtual void OnVerifying() {}
	virtual void OnReadyToUpdate();
	virtual void OnUpdating() {}
	virtual void OnUnpacking() {}
	virtual void OnFinishedUpdating() {}
	virtual void OnReadyToInstallNewVersion(const OpString& version, BOOL automated) {}
	virtual void OnError(AutoUpdateError error, BOOL automated) {}
	
	/**
	 * Used to change the internal AutoUpdateURL object to an external one.
	 * Should only be used for testing.
	 *
	 * Note: we don't call Init after assigning the pointer since we assume 
	 * that the caller setup this AutoUpdateURL object as they desired
	 *
	 * Note the AutoUpdateURL pointer passed in must have been created with
	 * a called to OP_NEW() and should NOT be deleted by the caller
	 *
	 * @param url External AutoUpdateURL object to use
	 */
	void SetAutoUpdateXML(AutoUpdateXML *url);

	/**
	 * Tests if a full package of a version greater than passed in
	 * has been downloaded. This will test against the version that is held
	 * within the AutoUpdateURL object that was used to retrieve the XML
	 */
	BOOL HasDownloadedHigherVersion();

	/**
	 * Set to indicate that we expect a package in the xml from the server
	 */
	void SetExpectPackageInXML(BOOL expect_package) { m_expect_package = expect_package; }

	/**
	 * Set to indicate that we expect a spoof file in the xml from the server
	 */
	void SetExpectSpoofInXML(BOOL expect_spoof) { m_expect_spoof = expect_spoof; }

	/**
	 * Set to indicate that we expect a browser JS file in the xml from the server
	 */
	void SetExpectBrowserJSInXML(BOOL expect_browserjs) { m_expect_browserjs = expect_browserjs; }

	/**
	 * Set to indicate that we expect a hardware blocklist file in the xml from the server
	 */
	void SetExpectHardwareBlocklistInXML(BOOL expect_hardware_blocklist) { m_expect_hardware_blocklist = expect_hardware_blocklist; }

protected:
	virtual void OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size);
	virtual void OnFileDownloadFailed(FileDownloader* file_downloader);	

private:
	/**
	 * Tests if there is a package in the XML
	 */
	BOOL HasPackageInXML();

	/**
	 * Tests if there is a spoof file in the XML
	 */
	BOOL HasSpoofInXML();

	/**
	 * Tests if there is a browser js file in the XML
	 */
	BOOL HasBrowserJSInXML();

	/**
	 * Tests if there is a hardware blocklist file in the XML
	 */
	BOOL HasHardwareBlocklistInXML();

	/**
	 * Tests if there is a more recent spoof file in the XML
	 */
	BOOL HasMoreRecentSpoofInXML();
	
	/**
	 * Tests if there is a more recent browser js file in the XML
	 */
	BOOL HasMoreRecentBrowserJSInXML();

	/**
	 * Tests if there is a more recent hardware blocklist file in the XML
	 */
	BOOL HasMoreRecentHardwareBlocklistInXML();
		
	TestType	m_test_type;					///< Holds the type of test to run
	
	OpTimer		*m_async_timer;					///< OpTimer to timeout the async test 
	BOOL		m_expect_package;				///< Set to indicate that we expect a package in the xml from the server
	BOOL		m_expect_spoof;					///< Set to indicate that we expect a spoof file in the xml from the server
	BOOL		m_expect_browserjs;				///< Set to indicate that we expect a browser JS file in the xml from the server
	BOOL		m_expect_hardware_blocklist;	///< Set to indicate that we expect a hardware blocklist file in the xml from the server
};

#endif // SELFTEST
#endif // AUTO_UPDATE_SUPPORT

#endif // _ST_AUTOUPDATER_H_INCLUDED_
