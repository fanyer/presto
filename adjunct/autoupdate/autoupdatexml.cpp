/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/autoupdatexml.h"
#include "adjunct/autoupdate/additionchecker.h"
#include "adjunct/autoupdate/pi/opautoupdatepi.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/version/operaversion.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "platforms/vega_backends/vega_blocklist_file.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT
#	include "modules/spellchecker/opspellcheckerapi.h"
#endif // INTERNAL_SPELLCHECK_SUPPORT

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

const uni_char* AutoUpdateXML::AUTOUPDATE_SCHEMA_VERSION = UNI_L("1.0");
const uni_char* AutoUpdateXML::AUTOUPDATE_REQUEST_FILE_NAME = UNI_L("autoupdate_request.xml");
const OpFileFolder AutoUpdateXML::AUTOUPDATE_REQUEST_FILE_FOLDER = OPFILE_TEMPDOWNLOAD_FOLDER;

AutoUpdateXML::AutoUpdateXML()
	:m_timestamp_browser_js(0)
	,m_timestamp_override_downloaded_ini(0)
	,m_timestamp_dictionaries_xml(0)
	,m_timestamp_hardware_blocklist(0)
	,m_timestamp_handlers_ignore(0)
	,m_time_since_last_check(0)
	,m_download_all_snapshots(0)
	,m_inited(FALSE)
	,m_dirty(0)
{
}

AutoUpdateXML::~AutoUpdateXML()
{
}

OP_STATUS AutoUpdateXML::Init()
{
	RETURN_IF_ERROR(CollectPlatformData());
	RETURN_IF_ERROR(CollectBuildData());
	RETURN_IF_ERROR(CheckResourceFiles());

	EnsureStringsHaveBuffers();

	m_inited = TRUE;
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::CollectPlatformData()
{
	// Collect platform data
	OpAutoPtr<OpAutoUpdatePI> autoupdate_pi(OpAutoUpdatePI::Create());
	RETURN_OOM_IF_NULL(autoupdate_pi.get());

	RETURN_IF_ERROR(autoupdate_pi->GetOSName(m_os_name));
	RETURN_IF_ERROR(autoupdate_pi->GetOSVersion(m_os_version));
	RETURN_IF_ERROR(autoupdate_pi->GetArchitecture(m_architecture));
	RETURN_IF_ERROR(autoupdate_pi->GetPackageType(m_package_type));
	RETURN_IF_ERROR(autoupdate_pi->GetGccVersion(m_gcc_version));
	RETURN_IF_ERROR(autoupdate_pi->GetQTVersion(m_qt_version));

	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::CollectBuildData()
{
	// Get the currently running Opera version, allowing it to be overriden by autoupdate.ini.
	OperaVersion version;
	version.AllowAutoupdateIniOverride();

	RETURN_IF_ERROR(m_opera_version.AppendFormat(UNI_L("%d.%02d"), version.GetMajor(), version.GetMinor()));
	RETURN_IF_ERROR(m_build_number.AppendFormat(UNI_L("%04d"), version.GetBuild()));
	RETURN_IF_ERROR(m_edition.Set(g_pcnet->GetStringPref(PrefsCollectionNetwork::IspUserAgentId)));

	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::CheckResourceFiles()
{
	// Check if browser js file exists.
	OpFile browserjs;
	if(OpStatus::IsSuccess(browserjs.Construct(UNI_L("browser.js"), OPFILE_HOME_FOLDER)))
	{
		BOOL exists = FALSE;
		if(OpStatus::IsSuccess(browserjs.Exists(exists)) && !exists)
		{
			TRAP_AND_RETURN(err, g_pcui->WriteIntegerL(PrefsCollectionUI::BrowserJSTime, 0));
			TRAP_AND_RETURN(err, g_prefsManager->CommitL());
		}
	}

	// Check if spoof file exists.
	OpFile spoof;
	if(OpStatus::IsSuccess(spoof.Construct(UNI_L("override_downloaded.ini"), OPFILE_HOME_FOLDER)))
	{
		BOOL exists = FALSE;
		if(OpStatus::IsSuccess(spoof.Exists(exists)) && !exists)
		{
			TRAP_AND_RETURN(err, g_pcui->WriteIntegerL(PrefsCollectionUI::SpoofTime, 0));
			TRAP_AND_RETURN(err, g_prefsManager->CommitL());
		}
	}

	// Check if dictionaries file exists.
	OpFile dictionaries;
	if(OpStatus::IsSuccess(dictionaries.Construct(UNI_L("dictionaries.xml"), OPFILE_DICTIONARY_FOLDER)))
	{
		BOOL exists = FALSE;
		if(OpStatus::IsSuccess(dictionaries.Exists(exists)) && !exists)
		{
			TRAP_AND_RETURN(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DictionaryTime, 0));
			TRAP_AND_RETURN(err, g_prefsManager->CommitL());
		}
	}

	// Check if hardware blocklist files exist.
	const VEGABlocklistDevice::BlocklistType hardware_blocklist_types[] = {
#if defined(MSWIN)
		VEGABlocklistDevice::D3d10, VEGABlocklistDevice::WinGL
#elif defined(UNIX)
		VEGABlocklistDevice::UnixGL
#elif defined(_MACINTOSH_)
		VEGABlocklistDevice::MacGL
#endif
	};

	for (size_t i = 0; i < ARRAY_SIZE(hardware_blocklist_types); i++)
	{
		OpFile hardware_blocklist;
		if(OpStatus::IsSuccess(hardware_blocklist.Construct(VEGABlocklistFile::GetName(hardware_blocklist_types[i]), VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER)))
		{
			BOOL exists = FALSE;
			if(OpStatus::IsSuccess(hardware_blocklist.Exists(exists)) && !exists)
			{
				TRAP_AND_RETURN(err, g_pcui->WriteIntegerL(PrefsCollectionUI::HardwareBlocklistTime, 0));
				TRAP_AND_RETURN(err, g_prefsManager->CommitL());
				break;
			}
		}
	}

	// Check if handlers-ignore file exist.
	OpFileFolder ignore;
	const uni_char* prefs_filename = 0;
	g_pcfiles->GetDefaultFilePref(PrefsCollectionFiles::HandlersIgnoreFile, &ignore, &prefs_filename);

	OpFile handlersignore;
	if(prefs_filename && OpStatus::IsSuccess(handlersignore.Construct(prefs_filename, OPFILE_HOME_FOLDER)))
	{
		BOOL exists = FALSE;
		if(OpStatus::IsSuccess(handlersignore.Exists(exists)) && !exists)
		{
			TRAP_AND_RETURN(err, g_pcui->WriteIntegerL(PrefsCollectionUI::HandlersIgnoreTime, 0));
			TRAP_AND_RETURN(err, g_prefsManager->CommitL());
		}
	}
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::UpdateFragileData()
{
	RETURN_IF_ERROR(m_language.Set(g_languageManager->GetLanguage().CStr()));
	
	m_timestamp_browser_js				= GetTimeStamp(TSI_BrowserJS);
	m_timestamp_override_downloaded_ini	= GetTimeStamp(TSI_OverrideDownloaded);
	m_timestamp_dictionaries_xml		= GetTimeStamp(TSI_DictionariesXML);
	m_timestamp_hardware_blocklist		= GetTimeStamp(TSI_HardwareBlocklist);
	m_timestamp_handlers_ignore			= GetTimeStamp(TSI_HandlersIgnore);

	int time_of_last_check = GetTimeStamp(TSI_LastUpdateCheck);
	if (time_of_last_check > 0)
	{
		m_time_since_last_check = g_timecache->CurrentTime() - time_of_last_check;

		if (m_time_since_last_check <= 0)
			SetDirty(DF_TSLC);
	}
	else
		m_time_since_last_check = 0;

	if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		m_download_all_snapshots = g_pcui->GetIntegerPref(PrefsCollectionUI::DownloadAllSnapshots);

	return OpStatus::OK;
}

int AutoUpdateXML::GetTimeStamp(TimeStampItem item)
{
	switch (item)
	{
	case TSI_BrowserJS:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::BrowserJSTime);
	case TSI_OverrideDownloaded:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::SpoofTime);
	case TSI_DictionariesXML:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::DictionaryTime);
	case TSI_HardwareBlocklist:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::HardwareBlocklistTime);
	case TSI_HandlersIgnore:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::HandlersIgnoreTime);
	case TSI_LastUpdateCheck:
		return g_pcui->GetIntegerPref(PrefsCollectionUI::TimeOfLastUpdateCheck);
	default:
		OP_ASSERT(!"Unknown item");
	}
	return -1;
}

OP_STATUS AutoUpdateXML::SetPref(int which, int value)
{
	TRAP_AND_RETURN(st, g_pcui->WriteIntegerL(PrefsCollectionUI::integerpref(which), value));
	TRAP_AND_RETURN(st, g_prefsManager->CommitL());
	return OpStatus::OK;
}

void AutoUpdateXML::ClearRequestItems()
{
	m_items.Clear();
}

OP_STATUS AutoUpdateXML::AddRequestItem(AdditionCheckerItem* item)
{
	return m_items.Add(item);
}

OP_STATUS AutoUpdateXML::GetRequestXML(OpString8 &xml_string, AutoUpdateLevel level, RequestType request_type, AutoUpdateLevel* used_level)
{
	OP_ASSERT(m_inited);
	XMLFragment fragment;

	// Clear the dirty flags that might have been set by the last pass through this method
	ClearDirty();

	// This method goes through all the places that are suspected to cause problems and tries to fix them, i.e.
	// checking if the "First Version Run" preference value is empty and filling it with some default string if it is.
	// It also sets appropiate flags stored in m_dirty, so that they can be sent to the server using the <dirty> XML
	// element.
	CheckDirtyness();

	RETURN_IF_ERROR(AdjustUpdateLevel(level));
	if (used_level)
		*used_level = level;

	RETURN_IF_ERROR(UpdateFragileData());
	RETURN_IF_ERROR(fragment.OpenElement(XMLCompleteName(UNI_L("urn:OperaAutoupdate"), UNI_L("oau"), UNI_L("versioncheck"))));
	RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("schema-version"), AUTOUPDATE_SCHEMA_VERSION));
	
	// Adjust the level to not send requests for packages if the preference to disable it is on (i.e. MacAppStore builds)
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate) && (level == UpdateLevelUpgradeCheck || level == UpdateLevelDefaultCheck))
		level = UpdateLevelResourceCheck;

	OpString message;
	message.AppendFormat("Performing an update check (%d).", level);
	Console::WriteMessage(message.CStr());
	
	RETURN_IF_ERROR(fragment.SetAttributeFormat(UNI_L("update-level"), UNI_L("%d"), level));
	// See DSK-344690 for the "main" attribute description
	RETURN_IF_ERROR(fragment.SetAttributeFormat(UNI_L("main"), UNI_L("%d"), (request_type == RT_Main)?1:0));
	RETURN_IF_ERROR(AppendXMLOpera(fragment, level));
	RETURN_IF_ERROR(AppendXMLOperatingSystem(fragment, level));
	if (level != UpdateLevelCountryCheck)
		RETURN_IF_ERROR(AppendXMLPackage(fragment));
	fragment.CloseElement();

	RETURN_IF_ERROR(EncodeXML(fragment, xml_string));
	RETURN_IF_ERROR(DumpXML(xml_string));
	return OpStatus::OK;
}

BOOL AutoUpdateXML::NeedsResourceCheck()
{
	// If any of these timestamps is 0, we need to make a resource check ASAP in order to get
	// the newest resources.
	// See the notes found by the declaration of this method. Try to make sure that any resources
	// added to this array WILL be sent by the server in response to a resource update check
	// request, we WILL end up asking for resources on each startup otherwise.
	// The autoupdate servers might suffer from the load in such a case.
	TimeStampItem items[] = {
		 TSI_BrowserJS
		,TSI_OverrideDownloaded
		,TSI_DictionariesXML
	};

	for (int i=0; i<ARRAY_SIZE(items); i++)
	{
		if (GetTimeStamp(items[i]) == 0)
		{
			OpString message;
			message.AppendFormat("Resource check needed (%d).", i);
			Console::WriteMessage(message.CStr());
			return TRUE;
		}
	}

	Console::WriteMessage(UNI_L("Resource check not needed."));
	return FALSE;
}

OP_STATUS AutoUpdateXML::AdjustUpdateLevel(AutoUpdateLevel& level)
{
	UINT32 item_count = m_items.GetCount();
	UpdatableResource::UpdatableResourceType item_type = UpdatableResource::RTEmpty;

	/*
	 Determine:
	 a) if the addition checked items held currently in the m_items vector are consistent according to their type (i.e. the types can't be mixed);
	 b) the type of the addition items;
	 */
	for (UINT32 i=0; i<item_count; i++)
	{
		AdditionCheckerItem* item = m_items.Get(i);
		OP_ASSERT(item);
		if (item_type == 0)
		{
			/*
			The type of the first found addition checker item will be the type that we expect all the remaining items to have and it's also the type
			we'll use to determine the autoupdate level later on.
			*/
			item_type = item->GetType();
			continue;
		}
		else
		{
			/*
			We only allow one type of resources per one XML request. This limitation comes from the way the autoupdate server
			works today, it could be well possible to check for any kind of resources using the ResourceCheck update level, however 
			the way the server works is having different update levels for dictionaries and plugins, both being completely different
			from "resource check" meaning updating the browser.js, override_downloaded.ini, dictionaries.xml and settings.
			This could perhaps be changed with future development of the autoupdate server however is not possible currently due to 
			the amount of time needed to have virtually any change related to the autoupdate server implemented.
			*/
			if (!item->IsType(item_type))
				return OpStatus::ERR;
		}
	}

	if (UpdateLevelChooseAuto == level)
	{
		if (0 == item_count)
		{
			// In case there are no request items and the update level is set to auto, choose the default level (i.e. browser and resources update check)
			level = UpdateLevelDefaultCheck;
		}
		else
		{
			// In case the update level is set to auto and there are any addition checker items, we want to choose the level basing on the items type.
			switch (item_type)
			{
			case UpdatableResource::RTDictionary:
				level = UpdateLevelInstallDictionaries;
				break;
			case UpdatableResource::RTPlugin:
				level = UpdateLevelInstallPlugins;
				break;
			default:
				// Did you add a new resource type? You need to add a case here.
				OP_ASSERT(!"Don't know how to choose autoupdate level for this resource type!");
				break;
			}
		}
		if (UpdateLevelChooseAuto == level)
			// If were not able to determine the update level then we fail right here.
			return OpStatus::ERR;
		else
			return OpStatus::OK;
	}
	else
	{
		// An update level was chosen explicitly, verify it against the m_items contents.
		switch (level)
		{
		case UpdateLevelUpgradeCheck:
		case UpdateLevelResourceCheck:
		case UpdateLevelDefaultCheck:
		case UpdateLevelCountryCheck:
			/*
			For update levels 1-3,6 the m_items *must* be empty. We can't request any addition check with the browser/resources update, the autoupdate
			server doesn't work this way.
			*/
			if (item_count == 0)
				return OpStatus::OK;
			break;
		/*
		Check if the update level is in pair with the type of the items found in the m_items vector.
		*/
		case UpdateLevelInstallPlugins:
			if (item_type == UpdatableResource::RTPlugin)
				return OpStatus::OK;
			break;
		case UpdateLevelInstallDictionaries:
			if (item_type == UpdatableResource::RTDictionary)
				return OpStatus::OK;
			break;
		default:
			// Also add a new update level here if you added it higher.
			OP_ASSERT(!"Unknown autoupdate level!");
			break;
		};
		return OpStatus::ERR;
	}
}

OP_STATUS AutoUpdateXML::DumpXML(OpString8& xml_string)
{
	// If the option is set dump the XML file to disk
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::SaveAutoUpdateXML))
	{
		OpFile xml_request_dump_file;
		RETURN_IF_ERROR(xml_request_dump_file.Construct(AUTOUPDATE_REQUEST_FILE_NAME, AUTOUPDATE_REQUEST_FILE_FOLDER));
		RETURN_IF_ERROR(xml_request_dump_file.Open(OPFILE_WRITE));
		RETURN_IF_ERROR(xml_request_dump_file.Write(xml_string.CStr(), xml_string.Length()));
		RETURN_IF_ERROR(xml_request_dump_file.Close());

#ifdef _DEBUG
		// Open the XML request in a tab
		OpString url_string;
		OpString resolved;
		BOOL successful;

		RETURN_IF_ERROR(url_string.Set(UNI_L("file:")));
		RETURN_IF_ERROR(url_string.Append(xml_request_dump_file.GetFullPath()));
		TRAP_AND_RETURN(err, successful = g_url_api->ResolveUrlNameL(url_string, resolved));

		if (successful && resolved.Find("file://") == 0)
		{
			URL url = g_url_api->GetURL(resolved.CStr());
			URL ref_url_empty = URL();

			Window* feedback_window = windowManager->GetAWindow(TRUE, YES, YES);
			RETURN_VALUE_IF_NULL(feedback_window, OpStatus::ERR);
			RETURN_IF_ERROR(feedback_window->OpenURL(url, DocumentReferrer(ref_url_empty), TRUE, FALSE, -1, TRUE));
		}
#endif // _DEBUG
	}
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::EncodeXML(XMLFragment& fragment, OpString8& xml_string)
{
	UINT32 chunk = 0;
	ByteBuffer buffer;
	unsigned int bytes = 0;
	
	RETURN_IF_ERROR(fragment.GetEncodedXML(buffer, TRUE, "utf-8", FALSE));	
	for (chunk=0; chunk<buffer.GetChunkCount(); chunk++)
	{
		char *chunk_ptr = buffer.GetChunk(chunk, &bytes);
		if (chunk_ptr)
			RETURN_IF_ERROR(xml_string.Append(chunk_ptr, bytes));
	}
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLOperatingSystem(XMLFragment& fragment, AutoUpdateLevel level)
{
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("operating-system")));
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("platform")));
	RETURN_IF_ERROR(fragment.AddText(m_os_name.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("version")));
	RETURN_IF_ERROR(fragment.AddText(m_os_version.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("architecture")));
	RETURN_IF_ERROR(fragment.AddText(m_architecture.CStr()));
	fragment.CloseElement();

	if (level != UpdateLevelCountryCheck)
	{
		// send country code so that we can collect stats on region settings (DSK-347797)
		OpDesktopResources *dr_temp = NULL;
		RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
		OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);
		OpString country_code;
		RETURN_IF_ERROR(desktop_resources->GetCountryCode(country_code));
		if (country_code.IsEmpty())
		{
			RETURN_IF_ERROR(country_code.Set(UNI_L("empty")));
		}
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("country")));
		RETURN_IF_ERROR(fragment.AddText(country_code.CStr()));
		fragment.CloseElement();
	}

	fragment.CloseElement();
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLPackage(XMLFragment& fragment)
{
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("package")));
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("package-type")));
	RETURN_IF_ERROR(fragment.AddText(m_package_type.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("gcc")));
	RETURN_IF_ERROR(fragment.AddText(m_gcc_version.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("qt")));
	RETURN_IF_ERROR(fragment.AddText(m_qt_version.CStr()));
	fragment.CloseElement();
	fragment.CloseElement();
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLOpera(XMLFragment& fragment, AutoUpdateLevel level)
{
	OpString product, string_time_since_last_check, download_all_snapshots, first_version;
	OpString string_timestamp_browser_js, string_timestamp_override_downloaded_ini, string_timestamp_dictionaries_xml, string_timestamp_hardware_blocklist, string_timestamp_handlers_ignore;

	RETURN_IF_ERROR(string_timestamp_browser_js.AppendFormat(UNI_L("%d"), m_timestamp_browser_js));
	RETURN_IF_ERROR(string_timestamp_override_downloaded_ini.AppendFormat(UNI_L("%d"), m_timestamp_override_downloaded_ini));
	RETURN_IF_ERROR(string_timestamp_dictionaries_xml.AppendFormat(UNI_L("%d"), m_timestamp_dictionaries_xml));
	RETURN_IF_ERROR(string_timestamp_hardware_blocklist.AppendFormat(UNI_L("%d"), m_timestamp_hardware_blocklist));
	RETURN_IF_ERROR(string_timestamp_handlers_ignore.AppendFormat(UNI_L("%d"), m_timestamp_handlers_ignore));

	DesktopProductType product_type = g_desktop_product->GetProductType();

	if (PRODUCT_TYPE_OPERA == product_type)
		RETURN_IF_ERROR(product.Set("Opera"));
	else if (PRODUCT_TYPE_OPERA_NEXT == product_type)
		RETURN_IF_ERROR(product.Set("Opera Next"));
	else if (PRODUCT_TYPE_OPERA_LABS == product_type)
	{
		OpString labs_name_wide;
		RETURN_IF_ERROR(labs_name_wide.Set(g_desktop_product->GetLabsProductName()));
		RETURN_IF_ERROR(product.AppendFormat("Opera Labs %s", labs_name_wide.CStr()));
	}
	else
	{
		OP_ASSERT(!"Unknown product type!");
		RETURN_IF_ERROR(product.Set("Opera"));
	}

	RETURN_IF_ERROR(first_version.Set(g_pcui->GetStringPref(PrefsCollectionUI::FirstVersionRun)));
	RETURN_IF_ERROR(download_all_snapshots.AppendFormat(UNI_L("%d"), m_download_all_snapshots));
	RETURN_IF_ERROR(string_time_since_last_check.AppendFormat(UNI_L("%d"), m_time_since_last_check));
	int first_run_timestamp = g_pcui->GetIntegerPref(PrefsCollectionUI::FirstRunTimestamp);

	if (m_opera_version.IsEmpty() ||
		m_build_number.IsEmpty() ||
		m_language.IsEmpty()
	   )
		return OpStatus::ERR;

	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("opera")));
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("product")));
	RETURN_IF_ERROR(fragment.AddText(product.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("version")));
	RETURN_IF_ERROR(fragment.AddText(m_opera_version.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("build-number")));
	RETURN_IF_ERROR(fragment.AddText(m_build_number.CStr()));
	fragment.CloseElement();

	if (level != UpdateLevelCountryCheck)
	{
		if (first_version.HasContent())
		{
			RETURN_IF_ERROR(fragment.OpenElement(UNI_L("first-version")));
			RETURN_IF_ERROR(fragment.AddText(first_version.CStr()));
			fragment.CloseElement();
		}
		if (first_run_timestamp > 0)
		{
			OpString string_timestamp_first_run;
			RETURN_IF_ERROR(string_timestamp_first_run.AppendFormat(UNI_L("%d"), first_run_timestamp));
			RETURN_IF_ERROR(fragment.OpenElement(UNI_L("first-run")));
			RETURN_IF_ERROR(fragment.AddText(string_timestamp_first_run.CStr()));
			fragment.CloseElement();
		}

		// send country code and region prefs so that we can collect stats on region settings (DSK-347797)
		OpStringC region_pref;
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("user-country")));
		region_pref = g_pcui->GetStringPref(PrefsCollectionUI::CountryCode);
		RETURN_IF_ERROR(fragment.AddText(region_pref.HasContent() ? region_pref.CStr() : UNI_L("empty")));
		fragment.CloseElement();
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("detected-country")));
		region_pref = g_pcui->GetStringPref(PrefsCollectionUI::DetectedCountryCode);
		RETURN_IF_ERROR(fragment.AddText(region_pref.HasContent() ? region_pref.CStr() : UNI_L("empty")));
		fragment.CloseElement();
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("active-country")));
		region_pref = g_region_info->m_country;
		RETURN_IF_ERROR(fragment.AddText(region_pref.HasContent() ? region_pref.CStr() : UNI_L("empty")));
		fragment.CloseElement();
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("active-region")));
		region_pref = g_region_info->m_region;
		RETURN_IF_ERROR(fragment.AddText(region_pref.HasContent() ? region_pref.CStr() : UNI_L("empty")));
		fragment.CloseElement();
	}

	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("language")));
	RETURN_IF_ERROR(fragment.AddText(m_language.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("edition")));
	RETURN_IF_ERROR(fragment.AddText(m_edition.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("time-since-last-check")));
	RETURN_IF_ERROR(fragment.AddText(string_time_since_last_check.CStr()));
	fragment.CloseElement();

	if (level != UpdateLevelCountryCheck)
	{
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("should-serve-all")));
		RETURN_IF_ERROR(fragment.AddText(download_all_snapshots.CStr()));
		fragment.CloseElement();

		// See DSK-344128 for information about the <autoupdate-first-response> XML element
		if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AutoUpdateResponded))
		{
			RETURN_IF_ERROR(fragment.OpenElement(UNI_L("autoupdate-first-response")));
			RETURN_IF_ERROR(fragment.AddText(UNI_L("1")));
			fragment.CloseElement();
		}

		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("dirty")));
		OpString dirty_value;
		RETURN_IF_ERROR(dirty_value.AppendFormat("%d", m_dirty));
		RETURN_IF_ERROR(fragment.AddText(dirty_value));
		fragment.CloseElement();
	}

	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("modules")));

	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("browserjs-timestamp")));
	RETURN_IF_ERROR(fragment.AddText(string_timestamp_browser_js.CStr()));
	fragment.CloseElement();
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("spoof-timestamp")));
	RETURN_IF_ERROR(fragment.AddText(string_timestamp_override_downloaded_ini.CStr()));
	fragment.CloseElement();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("dictionary-timestamp")));
	RETURN_IF_ERROR(fragment.AddText(string_timestamp_dictionaries_xml.CStr()));
	fragment.CloseElement();

	RETURN_IF_ERROR(AppendXMLDictionaries(fragment, level));

	if (UpdateLevelInstallPlugins == level)
		RETURN_IF_ERROR(AppendXMLPlugins(fragment, level));
#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (level != UpdateLevelCountryCheck)
	{
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("blocklist-timestamp")));
		RETURN_IF_ERROR(fragment.AddText(string_timestamp_hardware_blocklist.CStr()));
		fragment.CloseElement();
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("handlers-ignore-timestamp")));
		RETURN_IF_ERROR(fragment.AddText(string_timestamp_handlers_ignore.CStr()));
		fragment.CloseElement();
	}

	fragment.CloseElement(); // modules

	fragment.CloseElement(); // opera
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLDictionaries(XMLFragment& fragment, AutoUpdateLevel level)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("dictionary")));

	if (level != UpdateLevelCountryCheck)
		RETURN_IF_ERROR(AppendXMLExistingDictionaries(fragment, level));

	if (UpdateLevelInstallDictionaries == level)
		RETURN_IF_ERROR(AppendXMLNewDictionaries(fragment, level));

	fragment.CloseElement();
#endif
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLPlugins(XMLFragment& fragment, AutoUpdateLevel level)
{
#ifdef PLUGIN_AUTO_INSTALL
	OpString version, key;
	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("plugins")));

	// Plugin version is always set to 0 for plugin installation
	RETURN_IF_ERROR(version.Set(UNI_L("0")));
	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		AdditionCheckerItem* item = m_items.Get(i);
		OP_ASSERT(item);
		OP_ASSERT(item->IsType(UpdatableResource::RTPlugin));

		RETURN_IF_ERROR(item->GetKey(key));
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("plugin")));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("mime-type"), key.CStr()));
		RETURN_IF_ERROR(fragment.SetAttributeFormat(UNI_L("version"), version.CStr()));
		fragment.CloseElement();
	}
	fragment.CloseElement();
#endif
	return OpStatus::OK;
}

OP_STATUS AutoUpdateXML::AppendXMLExistingDictionaries(XMLFragment& fragment, AutoUpdateLevel level)
{
#if defined(INTERNAL_SPELLCHECK_SUPPORT) && defined(SPELLCHECKER_CAP_GET_DICTIONARY_VERSION) && defined(WIC_CAP_GET_DICTIONARY_VERSION)
	// To update new Dictionaries
	OpSpellUiSessionImpl session(0);
	int number_of_dictionaries = session.GetInstalledLanguageCount();

	for (int i = 0; i < number_of_dictionaries; i++)
	{
		int language_version = 0;
		language_version = session.GetInstalledLanguageVersionAt(i);

		if (language_version > 0)
		{
			OpString language;
			OpString version;
			
			RETURN_IF_ERROR(language.Set(session.GetInstalledLanguageStringAt(i)));
			RETURN_IF_ERROR(version.AppendFormat(UNI_L("%d.0"), language_version));

			RETURN_IF_ERROR(fragment.OpenElement(UNI_L("language")));
			RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("version"), version.CStr()));
			RETURN_IF_ERROR(fragment.AddText(language));

			fragment.CloseElement();
		}
	}
	return OpStatus::OK;
#else
	OP_ASSERT(!"Can't list dictionaries with this source configuration!");
	return OpStatus::ERR;
#endif
}

OP_STATUS AutoUpdateXML::AppendXMLNewDictionaries(XMLFragment& fragment, AutoUpdateLevel level)
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OpString version, key;
	RETURN_IF_ERROR(version.Set(UNI_L("0")));

	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		AdditionCheckerItem* item = m_items.Get(i);
		OP_ASSERT(item);
		OP_ASSERT(item->IsType(UpdatableResource::RTDictionary));

		RETURN_IF_ERROR(item->GetKey(key));
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("language")));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("version"), version.CStr()));
		RETURN_IF_ERROR(fragment.AddText(key.CStr()));
		fragment.CloseElement();
	}
#endif
	return OpStatus::OK;
}

void AutoUpdateXML::ClearDirty()
{
	m_dirty = 0;
}

void AutoUpdateXML::SetDirty(DirtyFlag flag)
{
	m_dirty |= flag;
}

void AutoUpdateXML::CheckDirtyness()
{
	// See if we can actually change the value of the "Time Of Last Upgrade Check" preference
	const uni_char* section = UNI_L("Auto Update");
	const uni_char* pref = UNI_L("Time Of Last Upgrade Check");
	PrefsFile* reader = const_cast<PrefsFile*>(g_prefsManager->GetReader());
	BOOL allowed = FALSE;
	OP_STATUS err = OpStatus::OK;

	if (reader)
	{
		TRAP(err, allowed = reader->AllowedToChangeL(section, pref));
	}
	if (OpStatus::IsError(err) || !allowed)
		// Not possible to change a pref value, remember that
		SetDirty(DF_OPERAPREFS);

	// Check if the first run data is set, try to set it to fallback values in case it is
	const uni_char* first_version_run_fallback = UNI_L("Old Installation");
	const int first_run_timestamp_fallback = 1;

	// Try to set the empty values to some default ones, ignore errors but signal them by setting the flags
	if (g_pcui->GetStringPref(PrefsCollectionUI::FirstVersionRun).IsEmpty())
	{
		SetDirty(DF_FIRSTRUNVER);
		TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::FirstVersionRun, first_version_run_fallback));
		TRAP(err, g_prefsManager->CommitL());
	}

	if (g_pcui->GetIntegerPref(PrefsCollectionUI::FirstRunTimestamp) <= 0)
	{
		SetDirty(DF_FIRSTRUNTS);
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::FirstRunTimestamp, first_run_timestamp_fallback));
		TRAP(err, g_prefsManager->CommitL());
	}

#if defined(MSWIN)
	// Check and see if the current Opera binary is not located under the system temp path.
	// This might occur when there are problems with synchronisation of the different
	// processes run when applying an update, see DSK-345746.
	OpDesktopResources *resources = NULL;
	OpAutoPtr<OpDesktopResources> resources_guard;

	if (OpStatus::IsSuccess(OpDesktopResources::Create(&resources)))
	{
		resources_guard = resources;
		OpString binary_folder, temp_folder;

		RETURN_VOID_IF_ERROR(resources->GetBinaryResourceFolder(binary_folder));
		RETURN_VOID_IF_ERROR(resources->GetTempFolder(temp_folder));

		if (binary_folder.FindI(temp_folder.CStr()) != KNotFound)
			SetDirty(DF_RUNFROMTEMP);
	}
#endif // MSWIN
}

void AutoUpdateXML::EnsureStringsHaveBuffers()
{
	OpString* strings[] = {
		&m_opera_version,
		&m_build_number,
		&m_language,
		&m_edition,
		&m_os_name,
		&m_os_version,
		&m_architecture,
		&m_package_type,
		&m_gcc_version,
		&m_qt_version
	};

	int count = ARRAY_SIZE(strings);

	for (int i=0; i<count; i++)
	{
		OP_ASSERT(strings[i]);
		if (NULL == strings[i]->CStr())
			strings[i]->Set(UNI_L(""));
	}
}

#endif // AUTO_UPDATE_SUPPORT
