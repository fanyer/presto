#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/version_checker.h"
#include "adjunct/autoupdate/updater/audatafile_reader.h"
#include "adjunct/autoupdate/updatablefile.h"

VersionChecker::VersionChecker():
	m_status(VCSNotInitialized),
	m_autoupdate_xml(NULL),
	m_status_xml_downloader(NULL),
	m_listener(NULL),
	m_autoupdate_server_url(NULL)
{
}

VersionChecker::~VersionChecker()
{
	OP_DELETE(m_status_xml_downloader);
	OP_DELETE(m_autoupdate_server_url);
	OP_DELETE(m_autoupdate_xml);
}

OP_STATUS VersionChecker::Init(VersionCheckerListener* listener)
{
	if (!listener)
		return OpStatus::ERR;

	if (m_status != VCSNotInitialized)
		return OpStatus::ERR;

	m_listener = listener;
	m_status = VCSInitFailed;

	OpAutoPtr<AutoUpdateXML> autoupdate_xml_guard(OP_NEW(AutoUpdateXML, ()));
	OpAutoPtr<StatusXMLDownloader> status_xml_downloader_guard(OP_NEW(StatusXMLDownloader, ()));
	OpAutoPtr<AutoUpdateServerURL> autoupdate_server_url_guard(OP_NEW(AutoUpdateServerURL, ()));

	RETURN_OOM_IF_NULL(autoupdate_xml_guard.get());
	RETURN_OOM_IF_NULL(status_xml_downloader_guard.get());
	RETURN_OOM_IF_NULL(autoupdate_server_url_guard.get());

	RETURN_IF_ERROR(autoupdate_xml_guard->Init());
	RETURN_IF_ERROR(status_xml_downloader_guard->Init(StatusXMLDownloader::CheckTypeOther, this));
	RETURN_IF_ERROR(autoupdate_server_url_guard->Init());

	RETURN_IF_ERROR(m_data_file_reader.Init());
	RETURN_IF_ERROR(m_data_file_reader.Load());

	m_autoupdate_xml = autoupdate_xml_guard.release();
	m_status_xml_downloader = status_xml_downloader_guard.release();
	m_autoupdate_server_url = autoupdate_server_url_guard.release();

	if (!CheckIsNeeded())
		m_status = VCSCheckNotNeeded;
	else
	{
		RETURN_IF_ERROR(ReadVersionOfExistingUpdate());
		m_status = VCSCheckNotPerformed;
	}

	return OpStatus::OK;
}

OP_STATUS VersionChecker::GetDataFileLastModificationTime(time_t& result)
{
	uni_char* updateFile = m_data_file_reader.GetUpdateFile();

	RETURN_VALUE_IF_NULL(updateFile, OpStatus::ERR);

	OP_STATUS retcode = OpStatus::OK;
	OpFile opFile;
	retcode = opFile.Construct(updateFile);
	if (OpStatus::IsSuccess(retcode))
		retcode = opFile.GetLastModified(result);

	// AUDataFileReader uses a plain old "new []" to allocate the strings
	delete [] updateFile;
	return retcode;;
}

bool VersionChecker::CheckIsNeeded()
{
	time_t fileModificationTime = 0;

	RETURN_VALUE_IF_ERROR(GetDataFileLastModificationTime(fileModificationTime), true);
	time_t currentTime = static_cast<UINT32>(g_op_time_info->GetTimeUTC() / 1000.0);

	// It can happen when user modified date in the system
	if (currentTime < fileModificationTime)
		return true;

	time_t updateAgeInDays = (currentTime - fileModificationTime) / (60 * 60 * 24);
	if (static_cast<unsigned int>(updateAgeInDays) < MaxDownloadedPackageAgeDays)
		return false;

	return true;
}

OP_STATUS VersionChecker::ReadVersionOfExistingUpdate()
{
	uni_char* versionString = m_data_file_reader.GetVersion();
	uni_char* buildNumberString = m_data_file_reader.GetBuildNum();

	// OperaVersion::Set() will return error if any of the pointers passed to the method is NULL
	OP_STATUS result = m_downloaded_version.Set(versionString, buildNumberString);

	// AUDataFileReader uses a plain old "new []" to allocate the strings.
	delete [] versionString;
	delete [] buildNumberString;

	return result;
}

OP_STATUS VersionChecker::CheckRemoteVersion()
{
	if (m_status != VCSCheckNotPerformed)
		return OpStatus::ERR;

	OP_ASSERT(m_autoupdate_server_url);
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(m_status_xml_downloader);

	m_status = VCSCheckVersionFailed;

	OpString url_string;
	OpString8 xml_string;

	RETURN_IF_ERROR(m_autoupdate_server_url->GetCurrentURL(url_string));
	m_autoupdate_xml->ClearRequestItems();
	// RT_Other ensures that this request is not identified as the "main" autoupdate, see DSK-344690
	RETURN_IF_ERROR(m_autoupdate_xml->GetRequestXML(xml_string, AutoUpdateXML::UpdateLevelUpgradeCheck, AutoUpdateXML::RT_Other));
	RETURN_IF_ERROR(m_status_xml_downloader->StartXMLRequest(url_string, xml_string));

	m_status = VCSCheckInProgress;
	return OpStatus::OK;
}

void VersionChecker::StatusXMLDownloaded(StatusXMLDownloader* downloader)
{
	OP_ASSERT(m_listener);
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_status_xml_downloader);
	OP_ASSERT(m_status == VCSCheckInProgress);

	UpdatablePackage* package = NULL;

	UpdatableResource* resource = downloader->GetFirstResource();
	while (resource)
	{
		if (resource->GetType() == UpdatableResource::RTPackage)
		{
			package = static_cast<UpdatablePackage*>(resource);
			break;
		}
		resource = downloader->GetNextResource();
	}

	if (package)
	{
		OperaVersion package_version;
		if (OpStatus::IsError(package->GetPackageVersion(package_version)))
			m_status = VCSCheckVersionFailed;
		else
		{
			if (package_version < m_downloaded_version)
				m_status = VCSOlderUpdateAvailable;
			else if (package_version > m_downloaded_version)
				m_status = VCSNewerUpdateAvailable;
			else if (package_version == m_downloaded_version)
				m_status = VCSSameUpdateAvailable;
			else
				m_status = VCSCheckVersionFailed;
		}
	}
	else
		m_status = VCSNoUpdateAvailable;

	m_listener->VersionCheckFinished();
}

void VersionChecker::StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus)
{
	OP_ASSERT(m_listener);
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_status_xml_downloader);
	OP_ASSERT(m_status == VCSCheckInProgress);

	m_status = VCSCheckVersionFailed;

	m_listener->VersionCheckFinished();
}

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT
