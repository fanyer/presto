/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "modules/selftest/src/testutils.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/desktop_util/version/operaversion.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "adjunct/autoupdate/selftest/src/ST_autoupdater.h"
#include "adjunct/autoupdate/selftest/src/ST_autoupdatexml.h"

ST_AutoUpdater::ST_AutoUpdater(TestType test_type) :
	m_test_type(test_type),
	m_expect_package(TRUE),
	m_expect_spoof(TRUE),
	m_expect_browserjs(TRUE),
	m_expect_hardware_blocklist(TRUE)
{
	m_async_timer = OP_NEW(OpTimer, ());

	// If the internal timer can't be created then just die!
	if (!m_async_timer)
	{
		ST_failed("Unable to create server timeout timer");
	}
	else
	{
		m_async_timer->SetTimerListener(this);
		m_async_timer->Start(500*1000*2);
	}
	AddListener(this);
}

ST_AutoUpdater::~ST_AutoUpdater()
{
	OP_DELETE(m_async_timer);
}

OP_STATUS ST_AutoUpdater::Init()
{
	RETURN_IF_ERROR(AutoUpdater::Init());
	return OpStatus::OK;
}

void ST_AutoUpdater::SetAutoUpdateXML(AutoUpdateXML* au_xml)
{
	OP_ASSERT(au_xml);

	// Note: we don't call Init here since we assume that the selftest has setup the object already
	m_autoupdate_xml = au_xml;
}

OP_STATUS ST_AutoUpdater::ReplaceAutoUpdateXMLForST(ST_AutoUpdateXML** st_autoupdate_xml)
{
	OpAutoPtr<ST_AutoUpdateXML> st_autoupdate_xml_guard(OP_NEW(ST_AutoUpdateXML, ()));
	RETURN_OOM_IF_NULL(st_autoupdate_xml_guard.get());

	RETURN_IF_ERROR(st_autoupdate_xml_guard->Init());
	OP_DELETE(m_autoupdate_xml);

	m_autoupdate_xml = st_autoupdate_xml_guard.get();
	*st_autoupdate_xml = st_autoupdate_xml_guard.release();

	return OpStatus::OK;
}

void ST_AutoUpdater::StatusXMLDownloaded(StatusXMLDownloader* downloader)
{
	OP_ASSERT(m_xml_downloader == downloader);

	AutoUpdater::StatusXMLDownloaded(downloader);

	switch (m_test_type)
	{
		case VersionCheck:
		{
			if(HasPackageInXML() != m_expect_package)
			{
				if(m_expect_package)
					ST_failed("Expected Opera package in XML");
				else
					ST_failed("Did not expect Opera package in XML");
			}
			else
			{
				if(!m_expect_package)
				{
					ST_passed();
				}
				else
				{
					if (HasDownloadedHigherVersion())
						ST_passed();
					else
						ST_failed("Opera package of a higher version not found");
				}
			}
			break;
		}
		case SpoofCheck:
		{
			if(HasSpoofInXML() != m_expect_spoof)
			{
				if(m_expect_spoof)
					ST_failed("Expected spoof file in XML");
				else
					ST_failed("Did not expect spoof file in XML");
			}
			else
			{
				if(!m_expect_spoof)
				{
					ST_passed();
				}
				else
				{
					if (HasMoreRecentSpoofInXML())
						ST_passed();
					else
						ST_failed("Spoof file of a higher version not found");
				}
			}
			break;
		}
		case BrowserJSCheck:
		{
			if(HasBrowserJSInXML() != m_expect_browserjs)
			{
				if(m_expect_browserjs)
					ST_failed("Expected browser js file in XML");
				else
					ST_failed("Did not expect browser js file in XML");
			}
			else
			{
				if(!m_expect_browserjs)
				{
					ST_passed();
				}
				else
				{
					if (HasMoreRecentBrowserJSInXML())
						ST_passed();
					else
						ST_failed("Browser JS of a higher version not found");
				}
			}
			break;
		}
		case HardwareBlocklistCheck:
		{
			if(HasHardwareBlocklistInXML() != m_expect_hardware_blocklist)
			{
				if(m_expect_hardware_blocklist)
					ST_failed("Expected hardware blocklist file in XML");
				else
					ST_failed("Did not expect hardware blocklist file in XML");
			}
			else
			{
				if(!m_expect_hardware_blocklist)
				{
					ST_passed();
				}
				else
				{
					if (HasMoreRecentHardwareBlocklistInXML())
						ST_passed();
					else
						ST_failed("Hardware blocklist of a higher version not found");
				}
			}
			break;
		}
		case XMLCheck:
		{
			int error_count = 0;
			if(HasPackageInXML() != m_expect_package)
			{
				if(m_expect_package)
					ST_failed("Expected Opera package in XML");
				else
					ST_failed("Did not expect Opera package in XML");
				error_count++;
			}
			else
			{
				if(m_expect_package)
				{
					if (!HasDownloadedHigherVersion())
					{
						ST_failed("Opera package of a higher version not found");
						error_count++;
					}
				}
			}

			if(HasSpoofInXML() != m_expect_spoof)
			{
				if(m_expect_spoof)
					ST_failed("Expected spoof file in XML");
				else
					ST_failed("Did not expect spoof file in XML");
				error_count++;
			}
			else
			{
				if(m_expect_spoof)
				{
					if (!HasMoreRecentSpoofInXML())
					{
						ST_failed("Spoof file of a higher version not found");
						error_count++;
					}
				}
			}

			if(HasBrowserJSInXML() != m_expect_browserjs)
			{
				if(m_expect_browserjs)
					ST_failed("Expected browser js file in XML");
				else
					ST_failed("Did not expect browser js file in XML");
				error_count++;
			}
			else
			{
				if(m_expect_browserjs)
				{
					if (!HasMoreRecentBrowserJSInXML())
					{
						ST_failed("Browser JS of a higher version not found");
						error_count++;
					}
				}
			}

			if(HasHardwareBlocklistInXML() != m_expect_hardware_blocklist)
			{
				if(m_expect_hardware_blocklist)
					ST_failed("Expected hardware blocklist file in XML");
				else
					ST_failed("Did not expect hardware blocklist file in XML");
				error_count++;
			}
			else
			{
				if(m_expect_hardware_blocklist)
				{
					if (!HasMoreRecentHardwareBlocklistInXML())
					{
						ST_failed("Hardware blocklist of a higher version not found");
						error_count++;
					}
				}
			}
			
			if(error_count == 0)
				ST_passed();

			break;
		}
			
		case SignatureCheck:
		case DownloadCheck:
		{
			DownloadUpdate();
			break;
		}
//		case None:
		default:
			ST_passed();
		break;
	}
}

void ST_AutoUpdater::StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus status)
{
	OP_ASSERT(m_xml_downloader == downloader);

	AutoUpdater::StatusXMLDownloadFailed(downloader, status);

	ST_failed("XML Download Failed");
}

void ST_AutoUpdater::OnTimeOut( OpTimer *timer ) 
{
	AutoUpdater::OnTimeOut(timer);

	if (m_async_timer == timer)
	{
		ST_failed("Server connection timeout error");
	}
}		

void ST_AutoUpdater::OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL automated)
{
	if(!(m_expect_browserjs || m_expect_spoof || m_expect_hardware_blocklist || m_expect_package) && GetAvailableUpdateType() != UpdatableResource::RTSetting)
	{
		ST_failed("Did not expect available update");
	}
}

void ST_AutoUpdater::OnUpToDate(BOOL automated)
{
	if(m_expect_browserjs || m_expect_spoof || m_expect_hardware_blocklist || m_expect_package)
	{
		ST_failed("Expected update to be available");
	}
}

int ST_AutoUpdater::CalculateTimeOfNextUpdateCheck(ScheduleCheckType schedule_type)
{
	return 1;
}

BOOL ST_AutoUpdater::HasDownloadedHigherVersion()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "package"
			if (dl_elm->GetType() == UpdatableResource::RTPackage)
			{
				UpdatablePackage *up = static_cast<UpdatablePackage*>(dl_elm);

				if(up)
				{
					OpString requested_version, downloaded_version;
					OpString requested_builnum, downloaded_buildnum;
					OpString requested_full, downloaded_full;

					OperaVersion ov_requested, ov_downloaded;

					RETURN_VALUE_IF_ERROR(up->GetAttrValue(URA_VERSION, downloaded_version), FALSE);
					RETURN_VALUE_IF_ERROR(up->GetAttrValue(URA_BUILDNUM, downloaded_buildnum), FALSE);

					ST_AutoUpdateXML* st_autoupdate_xml = static_cast<ST_AutoUpdateXML*>(m_autoupdate_xml);

					RETURN_VALUE_IF_ERROR(st_autoupdate_xml->GetVersion(requested_version), FALSE);
					RETURN_VALUE_IF_ERROR(st_autoupdate_xml->GetBuildNum(requested_builnum), FALSE);

					RETURN_VALUE_IF_ERROR(downloaded_full.AppendFormat(UNI_L("%s.%s"), downloaded_version.CStr(), downloaded_buildnum.CStr()), FALSE);
					RETURN_VALUE_IF_ERROR(requested_full.AppendFormat(UNI_L("%s.%s"), requested_version.CStr(), requested_builnum.CStr()), FALSE);

					RETURN_VALUE_IF_ERROR(ov_downloaded.Set(downloaded_full), FALSE);
					RETURN_VALUE_IF_ERROR(ov_requested.Set(requested_full), FALSE);

					if (ov_downloaded > ov_requested)
						return TRUE;

					return FALSE;
				}
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasPackageInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "package"
			if (dl_elm->GetType() == UpdatableResource::RTPackage)
			{
				return TRUE;
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasSpoofInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "spoof" file
			if (dl_elm->GetType() == UpdatableResource::RTSpoofFile)
			{
				return TRUE;
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasBrowserJSInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "browser js" file
			if (dl_elm->GetType() == UpdatableResource::RTBrowserJSFile)
			{
				return TRUE;
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasHardwareBlocklistInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "hardware blocklist" file
			if (dl_elm->GetType() == UpdatableResource::RTHardwareBlocklist)
			{
				return TRUE;
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasMoreRecentSpoofInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "spoof" file
			if (dl_elm->GetType() == UpdatableResource::RTSpoofFile)
			{
				UpdatableSpoof *us = static_cast<UpdatableSpoof*>(dl_elm);

				if(us)
				{
					int downloaded_timestamp, requested_timestamp;
					RETURN_VALUE_IF_ERROR(us->GetAttrValue(URA_TIMESTAMP, downloaded_timestamp), FALSE);

					ST_AutoUpdateXML* st_xml = static_cast<ST_AutoUpdateXML*>(m_autoupdate_xml);
					requested_timestamp = st_xml->GetTimeStamp(AutoUpdateXML::TSI_OverrideDownloaded);

					return requested_timestamp < downloaded_timestamp;
				}
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasMoreRecentBrowserJSInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "browser js" file
			if (dl_elm->GetType() == UpdatableResource::RTBrowserJSFile)
			{
				UpdatableBrowserJS *ub = static_cast<UpdatableBrowserJS*>(dl_elm);
				
				if(ub)
				{
					int timestamp_downloaded, timestamp_requested;

					RETURN_VALUE_IF_ERROR(ub->GetAttrValue(URA_TIMESTAMP, timestamp_downloaded), FALSE);
					ST_AutoUpdateXML* st_xml = static_cast<ST_AutoUpdateXML*>(m_autoupdate_xml);
					timestamp_requested = st_xml->GetTimeStamp(AutoUpdateXML::TSI_BrowserJS);

					return timestamp_requested < timestamp_downloaded;
				}
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

BOOL ST_AutoUpdater::HasMoreRecentHardwareBlocklistInXML()
{
	// Dig through the downloaded xml to try and find the object
	if (m_xml_downloader)
	{
		UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
		while (dl_elm)
		{
			// Check this is a "hardware blocklist" file
			if (dl_elm->GetType() == UpdatableResource::RTHardwareBlocklist)
			{
				UpdatableHardwareBlocklist *ub = static_cast<UpdatableHardwareBlocklist*>(dl_elm);
				
				if(ub)
				{
					int timestamp_downloaded, timestamp_requested;

					RETURN_VALUE_IF_ERROR(ub->GetAttrValue(URA_TIMESTAMP, timestamp_downloaded), FALSE);
					ST_AutoUpdateXML* st_xml = static_cast<ST_AutoUpdateXML*>(m_autoupdate_xml);
					timestamp_requested = st_xml->GetTimeStamp(AutoUpdateXML::TSI_HardwareBlocklist);

					if (timestamp_requested < timestamp_downloaded)
						return TRUE;
				}
			}
			dl_elm = m_xml_downloader->GetNextResource();
		}
	}
	
	return FALSE;
}

void ST_AutoUpdater::OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size)
{
	AutoUpdater::OnFileDownloadDone(file_downloader, total_size);
}

void ST_AutoUpdater::OnFileDownloadFailed(FileDownloader* file_downloader)
{
	AutoUpdater::OnFileDownloadFailed(file_downloader);
	ST_failed("File download failed.");
}

void ST_AutoUpdater::OnReadyToUpdate()
{
	INT32 file_count = 0;
	INT32 downloaded_file_count = 0;
	INT32 failed_download_count = 0;
	
	GetDownloadStatus(&file_count, &downloaded_file_count, &failed_download_count);	
	
	if(file_count > 0 && file_count == downloaded_file_count)
	{
		// All files are downloaded
		if(m_test_type == DownloadCheck)
		{
			ST_passed();
			return;
		}

		if(m_test_type == SignatureCheck)
		{
			if(CheckAllResources())
			{
				ST_passed();
			}
			else
			{
				ST_failed("File signature check failed.");
			}
		}
	}
}

OP_STATUS ST_AutoUpdater::CheckAndUpdateState()
{
	switch(m_test_type)
	{
		case DownloadCheck:
		case SignatureCheck:
		{
			return AutoUpdater::CheckAndUpdateState();
			break;
		}
	}
	return OpStatus::OK;
}

#endif // SELFTEST
#endif // AUTO_UPDATE_SUPPORT

