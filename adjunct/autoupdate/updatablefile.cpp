/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/updatablefile.h"

#include "adjunct/autoupdate/signed_file.h"
#include "adjunct/autoupdate/autoupdate_key.h"
#include "adjunct/desktop_util/version/operaversion.h"
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
# include "adjunct/autoupdate/updater/pi/aufileutils.h"
# include "adjunct/autoupdate/pi/opautoupdatepi.h"
# include "adjunct/autoupdate/updater/audatafile.h"
#endif
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/quick/dialogs/TrustedProtocolDialog.h" // Defines TrustedProtocolList

#include "modules/url/url2.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/opfile/opfile.h"
#include "modules/dom/domenvironment.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/spellchecker/opspellcheckerapi.h"
#include "platforms/vega_backends/vega_blocklist_file.h"
#ifdef PLUGIN_AUTO_INSTALL
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/desktop_pi/launch_pi.h"
#endif // PLUGIN_AUTO_INSTALL

UpdatableFile::UpdatableFile()
{
}

BOOL UpdatableFile::CheckResource()
{
	OpString signature;
	OpString8 signature8;
	OpString target_filename;

	RETURN_VALUE_IF_ERROR(GetTargetFilename(target_filename), FALSE);
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_CHECKSUM, signature), FALSE);
	RETURN_VALUE_IF_ERROR(signature8.Set(signature), FALSE);

	if(OpStatus::IsSuccess(CheckFileSignature(target_filename, signature8)))
	{
		return TRUE;
	}
	else
	{
		OpString message;
		message.AppendFormat(UNI_L("Signature verification failed for %s"), target_filename.CStr());
		Console::WriteError(message.CStr());
		return FALSE;
	}
}

OP_STATUS UpdatableFile::GetDownloadFullName(OpString& full_filename)
{
	OpString file_name;
	RETURN_IF_ERROR(GetAttrValue(URA_NAME, file_name));
	RETURN_IF_ERROR(GetDownloadFolder(full_filename));
	RETURN_IF_ERROR(full_filename.Append(file_name));
	return OpStatus::OK;
}

OP_STATUS UpdatableFile::GetDownloadFolder(OpString& download_folder)
{
	return g_folder_manager->GetFolderPath(OPFILE_TEMPDOWNLOAD_FOLDER, download_folder);
}

OP_STATUS UpdatableFile::StartDownloading(FileDownloadListener* listener)
{
	OP_ASSERT(listener);
	if (!listener)
		return OpStatus::ERR;

	OpString url;
	OpString target_filename;

	// The empty HasContent() check below is enough
	RETURN_IF_ERROR(GetAttrValue(URA_URL, url));
	RETURN_IF_ERROR(GetDownloadFullName(target_filename));
	if (url.HasContent() && target_filename.HasContent())
	{
		SetFileDownloadListener(listener);
		return StartDownload(url, target_filename);
	}
	else
		return OpStatus::ERR;
}

OpFileLength UpdatableFile::GetOverridedFileLength()
{
	int size = 0;
	if (OpStatus::IsError(GetAttrValue(URA_SIZE, size)))
		return 0;

	return size;
}

BOOL UpdatablePackage::VerifyAttributes()
{
	URAttr mandatory[] = 
	{
		URA_NAME,
		URA_VERSION,
		URA_BUILDNUM,
		URA_URL,
		URA_TYPE,
		URA_CHECKSUM
	};

	int count = ARRAY_SIZE(mandatory);
	for (int i=0; i<count; i++)
		if (!HasAttrWithContent(mandatory[i]))
			return FALSE;

	OpString version, buildnum;

	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_VERSION, version), FALSE);
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_BUILDNUM, buildnum), FALSE);

	// Check that provided upgrade version is higher than current
	OperaVersion upgrade_version;
	OpString version_string;
	RETURN_VALUE_IF_ERROR(version_string.Set(version), FALSE);
	RETURN_VALUE_IF_ERROR(version_string.Append("."), FALSE);
	RETURN_VALUE_IF_ERROR(version_string.Append(buildnum), FALSE);	
	RETURN_VALUE_IF_ERROR(upgrade_version.Set(version_string), FALSE);
	
	// Check that downloaded version is higher
	if(AutoUpdater::GetOperaVersion() >= upgrade_version)
	{
		Console::WriteError(UNI_L("Current version is higher than provided version."));
		return FALSE;
	}
	return TRUE;
}

BOOL UpdatablePackage::GetShowNotification()
{
	OpString string;
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_SHOWDIALOG, string), FALSE);
	return string.CompareI(UNI_L("true")) == 0;
}

OP_STATUS UpdatablePackage::UpdateResource()
{
#ifndef AUTOUPDATE_PACKAGE_INSTALLATION
	return OpStatus::ERR_NOT_SUPPORTED;
#else
	uni_char* exe_path = NULL;
	
	AUFileUtils* file_utils = AUFileUtils::Create();
	if (!file_utils)
		return OpStatus::ERR_NO_MEMORY;
	
	if(file_utils->GetExePath(&exe_path) != AUFileUtils::OK)
	{
		OP_DELETE(file_utils);
		return OpStatus::ERR;
	}
	
	OpString exe_path_str;
	RETURN_IF_ERROR(exe_path_str.Set(exe_path));
	OP_DELETEA(exe_path);

	uni_char* upgrade_folder = NULL;
	if(file_utils->GetUpgradeFolder(&upgrade_folder) != AUFileUtils::OK)
	{
		OP_DELETE(file_utils);
		return OpStatus::ERR;
	}
	
	// Delete the upgrade folder
	OpFile upgrade_path;
	if(OpStatus::IsSuccess(upgrade_path.Construct(upgrade_folder, OPFILE_ABSOLUTE_FOLDER)))
	{
		OpStatus::Ignore(upgrade_path.Delete(TRUE));
	}

	// Copy downloaded package to upgrade folder
	OpFile source_package;
	RETURN_IF_ERROR(source_package.Construct(m_file_name, OPFILE_ABSOLUTE_FOLDER));

	OpString dest_name;
	RETURN_IF_ERROR(dest_name.Set(upgrade_folder));
	RETURN_IF_ERROR(dest_name.Append(source_package.GetName()));

	OpFile dest_package;
	RETURN_IF_ERROR(dest_package.Construct(dest_name, OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(dest_package.CopyContents(&source_package, FALSE));

	AUDataFile au_datafile;

	OpString version, buildnum;

	RETURN_IF_ERROR(GetAttrValue(URA_VERSION, version));
	RETURN_IF_ERROR(GetAttrValue(URA_BUILDNUM, buildnum));

	if(OpStatus::IsError(au_datafile.Init()) ||
	   OpStatus::IsError(au_datafile.SetInstallPath(exe_path_str.CStr())) ||
	   OpStatus::IsError(au_datafile.SetUpdateFile(dest_package.GetFullPath(), AUDataFile::Full)) ||
	   OpStatus::IsError(au_datafile.SetVersion(version)) ||
	   OpStatus::IsError(au_datafile.SetBuildnum(buildnum)) ||
	   OpStatus::IsError(au_datafile.Write()))
	{
		OP_DELETE(file_utils);
		return OpStatus::ERR;
	}
			
	// Copy the internal Update.exe to Update.exe is the special folder
	uni_char*	update_source_path = NULL;
	OpString	update_name_from;
				
	// Get the name of the update
	if(file_utils->GetUpgradeSourcePath(&update_source_path) != AUFileUtils::OK)
	{
		OP_DELETE(file_utils);
		OP_DELETEA(update_source_path);
		return OpStatus::ERR;
	}
	OP_STATUS rtn = update_name_from.Set(update_source_path);
	OP_DELETEA(update_source_path);
	RETURN_IF_ERROR(rtn);

	// Build the to name
	uni_char*	update_path = NULL;
	OpString	update_name_to;
				
	// Get the name of the update
	if(file_utils->GetUpdatePath(&update_path) != AUFileUtils::OK)
	{
		OP_DELETE(file_utils);
		OP_DELETEA(update_path);
		return OpStatus::ERR;
	}
	rtn = update_name_to.Set(update_path);
	OP_DELETEA(update_path);
	RETURN_IF_ERROR(rtn);

	// Copy the locations which could be a file or folder
	OpFile target, src;

	target.Construct(update_name_to.CStr());
	src.Construct(update_name_from.CStr());

	rtn = DesktopOpFileUtils::Copy(target, src, TRUE, FALSE);
	if(OpStatus::IsError(rtn))
	{
		au_datafile.Delete();
		return rtn;
	}

	OP_DELETE(file_utils);

	OpAutoUpdatePI* autoupdate_pi = OpAutoUpdatePI::Create();
	if(!autoupdate_pi)
		return OpStatus::ERR_NO_MEMORY;
	// Extract the files needed to launch the installation
	if (autoupdate_pi->ExtractInstallationFiles(dest_name.CStr()) != AUFileUtils::OK)
	{
		OP_DELETE(autoupdate_pi);
		return OpStatus::ERR;
	}
	OP_DELETE(autoupdate_pi);

	return OpStatus::OK;
#endif
}

BOOL UpdatablePackage::UpdateRequiresUnpacking()
{
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	OpAutoUpdatePI* autoupdate_pi = OpAutoUpdatePI::Create();
	if(!autoupdate_pi)
		return FALSE;
	BOOL ret = autoupdate_pi->ExtractionInBackground();
	OP_DELETE(autoupdate_pi);
	return ret;
#else
	return FALSE;
#endif
}

BOOL UpdatablePackage::CheckResource()
{
	OpString version_attr, buildnum_attr;
	OperaVersion version;
	OpString version_string;

	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_VERSION, version_attr), FALSE);
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_BUILDNUM, buildnum_attr), FALSE);
	RETURN_VALUE_IF_ERROR(version_string.Set(version_attr), FALSE);
	RETURN_VALUE_IF_ERROR(version_string.Append("."), FALSE);
	RETURN_VALUE_IF_ERROR(version_string.Append(buildnum_attr), FALSE);

	RETURN_VALUE_IF_ERROR(version.Set(version_string), FALSE);

	// Check that downloaded version is higher
	if(AutoUpdater::GetOperaVersion() >= version)
	{
		Console::WriteError(UNI_L("Current version is higher than downloaded version."));
		return FALSE;
	}
	
	return UpdatableFile::CheckResource();
}

OP_STATUS UpdatablePackage::GetPackageVersion(OperaVersion& version)
{
	OpString version_attr, buildno_atrr;
	RETURN_IF_ERROR(GetAttrValue(URA_VERSION, version_attr));
	RETURN_IF_ERROR(GetAttrValue(URA_BUILDNUM, buildno_atrr));
	return version.Set(version_attr.CStr(), buildno_atrr.CStr());
}

OP_STATUS UpdatablePackage::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

OP_STATUS UpdatableSpoof::UpdateResource()
{
	OpString url_string;
	OpString resolved;
	BOOL successful = FALSE;
	int timestamp = 0;

	RETURN_IF_ERROR(url_string.Set(UNI_L("file:")));
	RETURN_IF_ERROR(url_string.Append(m_file_name));
	RETURN_IF_LEAVE(successful = g_url_api->ResolveUrlNameL(url_string, resolved));

	if (!successful || resolved.Find("file://") != 0)
		return OpStatus::ERR;
	
	URL url = g_url_api->GetURL(resolved.CStr());
	RETURN_IF_ERROR(g_PrefsLoadManager->InitLoader(url, OP_NEW(EndChecker,)));
	RETURN_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp));
	TRAP_AND_RETURN(res, g_pcui->WriteIntegerL(PrefsCollectionUI::SpoofTime, timestamp));
	TRAP_AND_RETURN(res, g_prefsManager->CommitL());

	return OpStatus::OK;
}

BOOL UpdatableSpoof::CheckResource()
{
	int timestamp;
	time_t current_timestamp = g_pcui->GetIntegerPref(PrefsCollectionUI::SpoofTime);

	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp), FALSE);
	if(current_timestamp >= timestamp)
	{
		Console::WriteError(UNI_L("Invalid timestamp for spoof file."));
		return FALSE;
	}
	
	// Disabled for now, spoof files are generated on the fly so no checksum yet.
	//return UpdatableFile::CheckResource();

	return TRUE;
}

OP_STATUS UpdatableSpoof::Cleanup()
{
	// The loading of the spoof is done asynchrous so we cant delete it before its finished
	FileDownloader::CleanUp();
	return OpStatus::OK;
}

BOOL UpdatableSpoof::VerifyAttributes()
{
	// Check if all the needed attributes were sent with the response XML for this file resource

	// We need the filename
	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	// Either the URL or the alternative URL needs to be present
	if (!HasAttrWithContent(URA_URL) && !HasAttrWithContent(URA_URLALT))
		return FALSE;

	// The timestamp has to be present
	if (!HasAttrWithContent(URA_TIMESTAMP))
		return FALSE;

	return TRUE;
}

OP_STATUS UpdatableBrowserJS::UpdateResource()
{
	// Copy downloaded browser.js file to home folder.
	OpFile local_file;
	OpFile temp_browser_js_file;
	OpString local_file_name;
	int timestamp = 0;

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, local_file_name));

	if(local_file_name.Length() <= 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp));
	RETURN_IF_ERROR(local_file_name.Append("browser.js"));
	RETURN_IF_ERROR(local_file.Construct(local_file_name.CStr()));
	RETURN_IF_ERROR(temp_browser_js_file.Construct(m_file_name.CStr()));
	RETURN_IF_ERROR(local_file.CopyContents(&temp_browser_js_file, FALSE));
	// Write the setting
	TRAP_AND_RETURN(res, g_pcjs->WriteIntegerL(PrefsCollectionJS::BrowserJSSetting, 2));
	TRAP_AND_RETURN(res, g_pcui->WriteIntegerL(PrefsCollectionUI::BrowserJSTime, timestamp));
	TRAP_AND_RETURN(res, g_prefsManager->CommitL());

	return OpStatus::OK;
}

BOOL UpdatableBrowserJS::CheckResource()
{
	int timestamp;
	time_t current_timestamp = g_pcui->GetIntegerPref(PrefsCollectionUI::BrowserJSTime);
	
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp), FALSE);
	if(current_timestamp >= timestamp)
	{
		Console::WriteError(UNI_L("Invalid timestamp for browser js file."));
		return FALSE;
	}

	// Check if the intermediate file is ok
	OpFile temp_browser_js_file;
	RETURN_VALUE_IF_ERROR(temp_browser_js_file.Construct(m_file_name.CStr()), FALSE);
	if(!DOM_Environment::CheckBrowserJSSignature(temp_browser_js_file))
	{
		Console::WriteError(UNI_L("Browser js signature verification failed."));
		return FALSE;
	}

	return UpdatableFile::CheckResource();
}

OP_STATUS UpdatableBrowserJS::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

BOOL UpdatableBrowserJS::VerifyAttributes()
{
	if (g_pcjs->GetIntegerPref(PrefsCollectionJS::BrowserJSSetting) <= 0)
		return FALSE;

	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	if (!HasAttrWithContent(URA_URL) && !HasAttrWithContent(URA_URLALT))
		return FALSE;

	if (!HasAttrWithContent(URA_TIMESTAMP))
		return FALSE;

	return TRUE;
}

OP_STATUS UpdatableDictionary::UpdateResource()
{
	OpFile dictionary_file;
	OpString file_name;

	RETURN_IF_ERROR(GetAttrValue(URA_NAME, file_name));
	RETURN_IF_ERROR(dictionary_file.Construct(file_name, OPFILE_DICTIONARY_FOLDER));
	OpFile downloaded_file;
	RETURN_IF_ERROR(downloaded_file.Construct(m_file_name.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(dictionary_file.CopyContents(&downloaded_file, FALSE));
	
	if (OpStatus::IsError(g_internal_spellcheck->RefreshInstalledLanguages()))
		return OpStatus::ERR;
	
	return OpStatus::OK;
}

BOOL UpdatableDictionary::CheckResource()
{
	// Checksum test
	return UpdatableFile::CheckResource();
}

OP_STATUS UpdatableDictionary::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

BOOL UpdatableDictionary::VerifyAttributes()
{
	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	if (!HasAttrWithContent(URA_URL) && !HasAttrWithContent(URA_URLALT))
		return FALSE;

	if (!HasAttrWithContent(URA_ID))
		return FALSE;

	return TRUE;
}

OP_STATUS UpdatableDictionaryXML::UpdateResource()
{
	OpString file_name;
	OpFile dictionary_xml_file;
	OpFile downloaded_xml_file;
	int timestamp = 0;

	// Copy the file
	RETURN_IF_ERROR(GetAttrValue(URA_NAME, file_name));
	RETURN_IF_ERROR(dictionary_xml_file.Construct(file_name, OPFILE_DICTIONARY_FOLDER));
	RETURN_IF_ERROR(downloaded_xml_file.Construct(m_file_name.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(dictionary_xml_file.CopyContents(&downloaded_xml_file, FALSE));

	// Write the timestamp
	RETURN_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp));
	TRAP_AND_RETURN(res, g_pcui->WriteIntegerL(PrefsCollectionUI::DictionaryTime, timestamp));
	TRAP_AND_RETURN(res, g_prefsManager->CommitL());

	return OpStatus::OK;
}

BOOL UpdatableDictionaryXML::CheckResource()
{
	int timestamp;
	int current_timestamp = g_pcui->GetIntegerPref(PrefsCollectionUI::DictionaryTime);

	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp), FALSE);
	if(current_timestamp >= timestamp)
	{
		Console::WriteError(UNI_L("Invalid timestamp for dictionary file."));
		return FALSE;
	}
	
	// Checksum test
	return UpdatableFile::CheckResource();
}

OP_STATUS UpdatableDictionaryXML::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

BOOL UpdatableDictionaryXML::VerifyAttributes()
{
	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	if (!HasAttrWithContent(URA_URL) && !HasAttrWithContent(URA_URLALT))
		return FALSE;

	if (!HasAttrWithContent(URA_TIMESTAMP))
		return FALSE;

	return TRUE;
}

#ifdef PLUGIN_AUTO_INSTALL

UpdatablePlugin::UpdatablePlugin():
	m_async_process_runner(NULL)
{
	// Need to have the process runner per platform
# if defined(MSWIN)
	m_async_process_runner = OP_NEW(PIM_WindowsAsyncProcessRunner, ());
# elif defined(_MACINTOSH_)
	m_async_process_runner = OP_NEW(PIM_MacAsyncProcessRunner, ());
# else
	m_async_process_runner = NULL;
	// uncomment mock implementation in PluginInstallManager.h to use this:
	// m_async_process_runner = OP_NEW(PIM_MockAsyncProcessRunner, ());
# endif // MSWIN
	if(m_async_process_runner)
		m_async_process_runner->SetListener(this);
}

UpdatablePlugin::~UpdatablePlugin()
{
	if(m_async_process_runner)
	{
		m_async_process_runner->SetListener(NULL);
		OP_DELETE(m_async_process_runner);
		m_async_process_runner = NULL;
	}
}

void UpdatablePlugin::OnProcessFinished(DWORD exit_code)
{
	OpString mime_type;
	RETURN_VOID_IF_ERROR(GetAttrValue(URA_MIME_TYPE, mime_type));

	if(0 == exit_code)
		g_plugin_install_manager->Notify(PIM_INSTALL_OK, mime_type);
	else
		g_plugin_install_manager->Notify(PIM_INSTALL_FAILED, mime_type);
}

OP_STATUS UpdatablePlugin::UpdateResource()
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

BOOL UpdatablePlugin::CheckResource()
{
	/* DSK-336700:
		The autoupdate server can request two types of binary file verification here.
		If the XML with plugin information contains the <checksum> element, it's value is treated as a SHA-256 checksum hash of the
		file being verified and is compared with a hash calculated from the downloaded file.
		If the XML contains the <vendor-name> element, it's value is treated as a vendor name and is compared with the vendor name
		read from the digital signature of the file. The check is performed using the operating system API, see LaunchPI::VerifyExecutable().

		Warning:
		If both XML elements are present in the XML data sent from the server, the downloaded binary will be verified both ways and the final
		check result will only be TRUE if *both* verification methods return TRUE.
		If both XML elements are missing from the XML sent from the server, the file verification will FAIL. The file has to be verified with
		at least one verification method in order for this function to return TRUE.
	*/

	OpString signature, vendor_name;

	OpStatus::Ignore(GetAttrValue(URA_CHECKSUM, signature));
	OpStatus::Ignore(GetAttrValue(URA_VENDOR_NAME, vendor_name));

	bool has_checksum = signature.HasContent()?true:false;
	bool has_vendor = vendor_name.HasContent()?true:false;

	// We need to be able to check at least one of them
	if (!has_checksum && !has_vendor)
		return false;

	if (has_checksum)
	{
		// Standard SHA-256 checksum test
		if (!UpdatableFile::CheckResource())
			// If the checksum was present and the check failed, don't try anything else but rather fail right away
			return false;

		// Continue to the vendor name check or return true right away if no vendor name has been sent from the server
	}

	if (has_vendor)
	{
		OpAutoPtr<LaunchPI> launch_pi(LaunchPI::Create());
		if (!launch_pi.get())
			// Something went terribly wrong
			return false;

		OpString target_filename;
		if (OpStatus::IsError(GetTargetFilename(target_filename)))
			return false;

		// If the vendor test fails then forget the true result that might have come from the checksum check
		if (!launch_pi->VerifyExecutable(target_filename.CStr(), vendor_name.CStr()))
			return false;
	}

	// At this point we know we have been sent at least one of the checksum and vendor XML elements and that no test
	// failed for the elements that have been sent.
	return true;
}

OP_STATUS UpdatablePlugin::Cleanup()
{
	OP_STATUS ret = OpStatus::OK;
	FileDownloader::CleanUp();
	if(!m_file_name.IsEmpty())
		ret = DeleteDownloadedFile();
	return ret;
}

/***
 * Run the installer with or without the silent install_params.
 * The install_params string comes from the autoupdate server.
 */
OP_STATUS UpdatablePlugin::InstallPlugin(BOOL a_silent)
{
	if(NULL == m_async_process_runner)
	{
		OP_ASSERT(!"No async process runner for plugin");
		return OpStatus::ERR;
	}

	if(m_async_process_runner->IsRunning())
	{
		OP_ASSERT(!"Async process runner busy");
		return OpStatus::ERR;
	}

	OP_STATUS	async_run_status;
	OpString	install_params, empty_params;
	RETURN_IF_ERROR(GetAttrValue(URA_INSTALL_PARAMS, install_params));
	install_params.Strip();

	if(a_silent)
	{
		if(install_params.IsEmpty())
			return OpStatus::ERR;

		async_run_status = m_async_process_runner->RunProcess(m_file_name, install_params);
	}
	else
		async_run_status = m_async_process_runner->RunProcess(m_file_name, empty_params);

	return async_run_status;
}

OP_STATUS UpdatablePlugin::CancelPluginInstallation()
{
	if (NULL == m_async_process_runner)
	{
		OP_ASSERT(!"No async process runner for plugin");
		return OpStatus::ERR;
	}

	if (!m_async_process_runner->IsRunning())
	{
		OP_ASSERT(!"Async process runner NOT busy - nothing to cancel really");
		return OpStatus::ERR;
	}
	// This is synchronous operation, we'll get the result right back
	return m_async_process_runner->KillProcess();
}

BOOL UpdatablePlugin::VerifyAttributes()
{
	// List of mandatory attributes with a non-empty value
	// All values are stored as strings
	URAttr mandatory_attrs[] = 
	{
		URA_TYPE,
		URA_MIME_TYPE,
		URA_HAS_INSTALLER
	};

	// List of attributes that must be present and non-empty when the plugin
	// has an installer
	URAttr mandatory_attrs_has_installer[] = 
	{
		URA_NAME,
		URA_URL,
		URA_URLALT,
		URA_VERSION,
		URA_SIZE,
		URA_EULA_URL,
		URA_PLUGIN_NAME,
		URA_INSTALLS_SILENTLY
	};

	int mandatory_size = ARRAY_SIZE(mandatory_attrs);
	int installer_size = ARRAY_SIZE(mandatory_attrs_has_installer);

	for (int i=0; i<mandatory_size; i++)
		if (!HasAttrWithContent(mandatory_attrs[i]))
			return FALSE;

	bool has_installer;
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_HAS_INSTALLER, has_installer), FALSE);
	if (has_installer)
	{
		for (int i=0; i<installer_size; i++)
			if (!HasAttrWithContent(mandatory_attrs_has_installer[i]))
				return FALSE;

		// The plugin XML description must contain at least one of these
		if (!HasAttrWithContent(URA_CHECKSUM) && !HasAttrWithContent(URA_VENDOR_NAME))
			return FALSE;
	}

	return TRUE;
}

#endif // PLUGIN_AUTO_INSTALL

OP_STATUS UpdatableHardwareBlocklist::UpdateResource()
{
	OpString filename;
	RETURN_IF_ERROR(GetAttrValue(URA_NAME, filename));

	OpFile hardware_blocklist_file, downloaded_file;
	RETURN_IF_ERROR(downloaded_file.Construct(m_file_name, OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(hardware_blocklist_file.Construct(filename, VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER));
	RETURN_IF_ERROR(hardware_blocklist_file.CopyContents(&downloaded_file, FALSE));

	// Update the setting
	int timestamp = 0;
	RETURN_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp));
	if (timestamp > g_pcui->GetIntegerPref(PrefsCollectionUI::HardwareBlocklistTime)) {
		RETURN_IF_LEAVE(
			g_pcui->WriteIntegerL(PrefsCollectionUI::HardwareBlocklistTime, timestamp);
			g_prefsManager->CommitL();
		);
	}

	// Make sure we do not use a blocked web handler
	TrustedProtocolList list;
	RETURN_IF_ERROR(list.ResetBlockedWebHandlers());

	return OpStatus::OK;
}

BOOL UpdatableHardwareBlocklist::CheckResource()
{
	// Checksum test
	if (!UpdatableFile::CheckResource())
		return FALSE;

	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(m_file_name.CStr()), FALSE);

	if (OpStatus::IsError(VEGABlocklistFile::CheckFile(&file)))
	{
		OpString target_filename;
		RETURN_VALUE_IF_ERROR(GetTargetFilename(target_filename), FALSE);
		OpString message;
		message.AppendFormat(UNI_L("Hardware blocklist %s failed to parse"), target_filename.CStr());
		Console::WriteError(message.CStr());
		return FALSE;
	}

	return TRUE;
}

OP_STATUS UpdatableHardwareBlocklist::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

BOOL UpdatableHardwareBlocklist::VerifyAttributes()
{
	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	if (!HasAttrWithContent(URA_URL) && !HasAttrWithContent(URA_URLALT))
		return FALSE;

	if (!HasAttrWithContent(URA_TIMESTAMP))
		return FALSE;

	return TRUE;
}

OP_STATUS UpdatableHandlersIgnore::UpdateResource()
{
	OpFile handlers_ignore_file, downloaded_file;
	RETURN_IF_ERROR(downloaded_file.Construct(m_file_name, OPFILE_ABSOLUTE_FOLDER));

	// Save to file specified by prefs as code elsewhere expects a file with that name
	OpFileFolder ignore;
	const uni_char* prefs_filename = 0;
	g_pcfiles->GetDefaultFilePref(PrefsCollectionFiles::HandlersIgnoreFile, &ignore, &prefs_filename);
	OpStringC filename = prefs_filename;
	if (filename.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(handlers_ignore_file.Construct(filename, OPFILE_HOME_FOLDER));
	RETURN_IF_ERROR(handlers_ignore_file.CopyContents(&downloaded_file, FALSE));

	// Update the setting
	int timestamp = 0;
	RETURN_IF_ERROR(GetAttrValue(URA_TIMESTAMP, timestamp));
	if (timestamp > g_pcui->GetIntegerPref(PrefsCollectionUI::HandlersIgnoreTime)) {
		RETURN_IF_LEAVE(
			g_pcui->WriteIntegerL(PrefsCollectionUI::HandlersIgnoreTime, timestamp);
			g_prefsManager->CommitL();
		);
	}

	return OpStatus::OK;
}

OP_STATUS UpdatableHandlersIgnore::Cleanup()
{
	FileDownloader::CleanUp();
	return DeleteDownloadedFile();
}

BOOL UpdatableHandlersIgnore::VerifyAttributes()
{
	if (!HasAttrWithContent(URA_NAME))
		return FALSE;

	if (!HasAttrWithContent(URA_URL))
		return FALSE;

	if (!HasAttrWithContent(URA_TIMESTAMP))
		return FALSE;

	return TRUE;
}


#endif // AUTO_UPDATE_SUPPORT
