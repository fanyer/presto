/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#ifdef USE_COMMON_RESOURCES

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/resources/ResourceUtils.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "adjunct/desktop_util/resources/ResourceUpgrade.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/version/operaversion.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"

#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/prefsfile/prefsfile.h"

#include "modules/util/opfile/opfile.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUtils::Init(OperaInitInfo *pinfo, const uni_char *profile_name)
{
	OP_PROFILE_METHOD("Setting up resources");
	// Make sure we have a valid object pointer
	if (!pinfo)
		return OpStatus::ERR;

	CommandLineArgument* settings_argument = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::Settings);

	BOOL has_settings_argument = settings_argument != NULL && settings_argument->m_string_value.HasContent();

	// Reset the run type
	g_run_type->m_type = StartupType::RUNTYPE_NOT_SET;

	// Create the PI interface object
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
	OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);

	// Setup default folders
	{
		OP_PROFILE_METHOD("Set up default folders");

		RETURN_IF_ERROR(ResourceFolders::SetFolders(pinfo, desktop_resources.get(), profile_name));
	}

	if (!has_settings_argument)
	{
		OP_PROFILE_METHOD("Load old and default prefs");
		// Copy old prefs if applicable;
		RETURN_IF_ERROR(ResourceUpgrade::UpdatePref(DESKTOP_RES_OLD_OPERA_PREFS, DESKTOP_RES_OPERA_PREFS, pinfo));

		// Copy over default operaprefs.ini file
		RETURN_IF_ERROR(ResourceSetup::CopyDefaultFile(DESKTOP_RES_OPERA_PREFS, DESKTOP_RES_OPERA_PREFS, pinfo));
	}

	// Load prefs (inc global/fixed)
	// Global ini file
	OpString	filename_temp;

	// Create a user prefs file object
	OpFile user_prefs;
	if (!has_settings_argument)
	{
		RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_USERPREFS_FOLDER, pinfo, filename_temp));
		RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS));
		RETURN_IF_ERROR(user_prefs.Construct(filename_temp.CStr()));
	}
	else
		RETURN_IF_ERROR(user_prefs.Construct(settings_argument->m_string_value.CStr()));

	// Create a global prefs file object
	OpFile global_prefs;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_GLOBALPREFS_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS_GLOBAL));
    RETURN_IF_ERROR(global_prefs.Construct(filename_temp.CStr()));

	// Create a fixed prefs file object
	OpFile fixed_prefs;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_FIXEDPREFS_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS_FIXED));
	RETURN_IF_ERROR(fixed_prefs.Construct(filename_temp.CStr()));

	OpFile disable_prefs;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_INI_CUSTOM_PACKAGE_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS_DISABLED));
	RETURN_IF_ERROR(disable_prefs.Construct(filename_temp.CStr()));

	// Create temporary prefs file object for accessing global and user's operaprefs.ini.
	// Access to global prefs is needed to determine product type on Windows.
	PrefsFile pf_tmp(PREFS_INI, 1, 0);
	{
		OP_PROFILE_METHOD("Load user prefs");
		RETURN_IF_LEAVE(
			pf_tmp.ConstructL();	  \
			pf_tmp.SetFileL(&user_prefs);		\
			pf_tmp.SetGlobalFileL(&global_prefs, 0);	\
			pf_tmp.LoadAllL()
		);
	}

	// Determine the runtype
	RETURN_IF_ERROR(ResourceUtils::DetermineRunType(&pf_tmp));

	// Initialize product information
	RETURN_IF_ERROR(g_desktop_product->Init(pinfo, &pf_tmp));

	// Determine region and language for customization files (search.ini, bookmarks.adr, etc.)
	OpString region_language;
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(region_language, OpDesktopResources::REGION));

	OpString region;
	RETURN_IF_ERROR(DetermineRegionInfo(&pf_tmp, pinfo, desktop_resources.get(), RegionInfo::GetInstance()));

	OpString default_region_language;
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(default_region_language, OpDesktopResources::LOCALE, RegionInfo::GetInstance().m_default_language));

	RETURN_IF_ERROR(ResourceFolders::SetRegionFolders(pinfo, RegionInfo::GetInstance().m_region, region_language, default_region_language));

	// Copy over custom folder if it needs to be
	{
		OP_PROFILE_METHOD("Copy custom folders");

		RETURN_IF_ERROR(ResourceSetup::CopyCustomFolders(pinfo));
	}

	// Create a locale prefs file object
	OpFile locale_prefs;
	{
		OP_PROFILE_METHOD("Set up default language folder");
		RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_LANGUAGE_FOLDER, pinfo, filename_temp));
		RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS_LOCALE));
		RETURN_IF_ERROR(locale_prefs.Construct(filename_temp.CStr()));
	}

	// Create a region prefs file object
	{
		BOOL exists = FALSE;

		// First try for the preference in the ini file
		OpStringC au_country_code;
		RETURN_IF_LEAVE(au_country_code = pf_tmp.ReadStringL("Auto Update", "Country Code"));
		if (au_country_code.HasContent())
		{
			OP_PROFILE_METHOD("Setting up region settings from user pref");

			OpString region, ignored;
			RETURN_IF_ERROR(CountryCodeToRegion(pinfo, au_country_code, region, ignored));

			RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_REGION_ROOT_FOLDER, pinfo, filename_temp));
			RETURN_IF_ERROR(filename_temp.Append(region));
			RETURN_IF_ERROR(filename_temp.Append(PATHSEP));

			// Check if the folder exists
			OpFile region_folder;
			RETURN_IF_ERROR(region_folder.Construct(filename_temp));
			RETURN_IF_ERROR(region_folder.Exists(exists));
		}

		// If no folder was found by the pref or there was no pref then we go for the region folder itself
		if (!exists)
		{
			OP_PROFILE_METHOD("Set up default region folder");
			RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_REGION_FOLDER, pinfo, filename_temp));
		}
	}
	OpFile region_prefs;
	RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS_REGION));
	RETURN_IF_ERROR(region_prefs.Construct(filename_temp.CStr()));

	// Create the custom folder prefs file object
	OpFile custom_prefs;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_INI_CUSTOM_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(filename_temp.Append(DESKTOP_RES_OPERA_PREFS));
	RETURN_IF_ERROR(custom_prefs.Construct(filename_temp.CStr()));
	
	// Create the PrefsFile object
	OpAutoPtr<PrefsFile> pf(OP_NEW(PrefsFile, (PREFS_INI, 5, 1)));
	if (!pf.get())
		return OpStatus::ERR_NO_MEMORY;

	{
		OP_PROFILE_METHOD("Load prefs");
		RETURN_IF_LEAVE(
			pf->ConstructL();	   \
			pf->SetFileL(&user_prefs);		  \
			pf->SetGlobalFileL(&disable_prefs, 0);	\
			pf->SetGlobalFileL(&custom_prefs, 1);	\
			pf->SetGlobalFileL(&region_prefs, 2);	\
			pf->SetGlobalFileL(&locale_prefs, 3);	\
			pf->SetGlobalFileL(&global_prefs, 4);	\
			if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest)) pf->SetGlobalFixedFileL(&fixed_prefs); \
			pf->LoadAllL()
		);
	}

	// Upgrade and path creation
	{
		OP_PROFILE_METHOD("Copy old resources");

		RETURN_IF_ERROR(ResourceUpgrade::CopyOldResources(pinfo));
	}
	// Copy over all other default files (e.g. bookmark.adr, speeddial_defaults.ini etc)
	
	{
		OP_PROFILE_METHOD("Copy default browserjs");
		RETURN_IF_ERROR(ResourceSetup::CopyDefaultFile(DESKTOP_RES_PACKAGE_BROWSERJS, DESKTOP_RES_BROWSERJS, pinfo));
	};

	{
		OP_PROFILE_METHOD("Remove incompatible languages");
		// Delete any incompatiable languages from the ini file
		// This needs to be done last in case the custom files copies files to new locations
		RETURN_IF_ERROR(ResourceUpgrade::RemoveIncompatibleLanguageFile(pf.get()));
	}

	// Set the prefs reader in the info object
	pinfo->prefs_reader = pf.release();

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUtils::DetermineRunType(PrefsFile *pf)
{
	if (!pf)
		return OpStatus::ERR;

	// Only determine the runtype if it's not already set
	if (g_run_type->m_type == StartupType::RUNTYPE_NOT_SET)
	{
		g_run_type->m_added_custom_folder = false;
		// Check that the key even exists first
		if (!pf->IsKey("Install", "Newest Used Version"))
		{
			// If there is no key then this HAS to be the first run
			g_run_type->m_type = StartupType::RUNTYPE_FIRSTCLEAN;
		}
		else
		{
			OperaVersion	current_version;
			OpString		max_version_run;

			// Read the MaxVersion run preference
			RETURN_IF_LEAVE(pf->ReadStringL("Install", "Newest Used Version", max_version_run));
			if (OpStatus::IsError(g_run_type->m_previous_version.Set(max_version_run.CStr())))
			{
				g_run_type->m_type = StartupType::RUNTYPE_NORMAL;	// Something serious went wrong parsing the version number
																	// so just say this is a normal start.
				
				// Update the max version to current version
				OperaVersion current_version;
				TRAPD(err, pf->WriteStringL("Install", "Newest Used Version", current_version.GetFullString()));
#ifdef VER_BETA
				TRAP(err, pf->WriteStringL("Install", "Newest Used Beta Name", VER_BETA_NAME));
#else
				TRAP(err, pf->WriteStringL("Install", "Newest Used Beta Name", UNI_L("")));
#endif
				TRAP(err, pf->CommitL());
			}
			else
			{

				if (!pf->IsKey("Install", "Newest Used Beta Name"))
				{
					// If there is no key then this HAS to be first update after introducing the Newest Used Beta Name
					g_run_type->m_type = StartupType::RUNTYPE_FIRST;
					return OpStatus::OK;
				}

				OpString newest_beta_name;
				RETURN_IF_LEAVE(pf->ReadStringL("Install", "Newest Used Beta Name", newest_beta_name));
#ifndef VER_BETA
				if (newest_beta_name.HasContent()) // Changed from beta to final case (final version has no beta name)
#else
				if (newest_beta_name != VER_BETA_NAME)
#endif
				{
					g_run_type->m_type = StartupType::RUNTYPE_FIRST; // First time running Opera after beta name change
					return OpStatus::OK;
				}

				if (g_run_type->m_previous_version.GetFullVersionNumber() < current_version.GetFullVersionNumber())
				{
					g_run_type->m_type = StartupType::RUNTYPE_FIRST; // First time running Opera after a major or a minor upgrade
				}
				else if (g_run_type->m_previous_version < current_version)
				{
					g_run_type->m_type = StartupType::RUNTYPE_FIRST_NEW_BUILD_NUMBER; // First time running Opera after only build number change
				}
				else
				{
					g_run_type->m_type = StartupType::RUNTYPE_NORMAL; // None of the above applies, so this must be a normal run.
				}
			}
		}
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ResourceUtils
{
/**
 * Represents all entries for single country code read from DESKTOP_RES_AU_REGION file
 */
struct AuCountry
{
	OpString8 country;  //< 2-letter ISO 3166-1 country code
	time_t first_date;  //< first time this country code was received from autoupdate server
	time_t last_date;   //< last time this country code was received
	unsigned int count; //< total number of times this country code was received
};

/**
 * Parse single line from DESKTOP_RES_AU_REGION file
 *
 * @param line input line; should contain timestamp and country code separated by single space
 * @param timestamp gets timestamp from the line
 * @param country gets country code from the line
 *
 * @return OK on success, ERR_PARSING_FAILED is line is invalid, ERR_NO_MEMORY on OOM
 */
OP_STATUS ParseAuCountryLine(OpStringC8 line, time_t& timestamp, OpString8& country)
{
	char* cptr = 0;
	timestamp = time_t(op_strtol(line.CStr(), &cptr, 10));
	if (timestamp == 0 || *cptr != ' ')
		return OpStatus::ERR_PARSING_FAILED;
	++ cptr;
	RETURN_IF_ERROR(country.Set(cptr));
	country.Strip();
	if (country.IsEmpty())
		return OpStatus::ERR_PARSING_FAILED;
	op_strlwr(country.CStr());
	return OpStatus::OK;
}
} // namespace


OP_STATUS ResourceUtils::DetermineAuCountry(DesktopProductType product_type, OpFileDescriptor& file, OpString& out_country)
{
	OpAutoString8HashTable<AuCountry> au_countries; // country codes received from autoupdate server
	AuCountry* candidate = NULL; // element from au_countries with largest count
	AuCountry* last_entry = NULL; // last country code entry in the file
	size_t count = 0; // number of valid (parsed) lines in autoupdate_region.ini

	out_country.Empty();

	time_t now = g_timecache->CurrentTime();

	RETURN_VALUE_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT), OpStatus::OK);

	while (!file.Eof())
	{
		OpString8 line;
		if (OpStatus::IsError(file.ReadLine(line)))
			break;
		time_t timestamp;
		OpString8 country;
		OP_STATUS status = ParseAuCountryLine(line, timestamp, country);
		RETURN_IF_MEMORY_ERROR(status);
		if (OpStatus::IsError(status) || timestamp > now)
			continue;

		AuCountry* au_cc;
		if (OpStatus::IsSuccess(au_countries.GetData(country, &au_cc)))
		{
			if (au_cc->first_date > timestamp)
				au_cc->first_date = timestamp;
			else if (au_cc->last_date < timestamp)
				au_cc->last_date = timestamp;
			++ au_cc->count;
		}
		else
		{
			au_cc = OP_NEW(AuCountry, ());
			RETURN_OOM_IF_NULL(au_cc);
			au_cc->first_date = au_cc->last_date = timestamp;
			RETURN_IF_ERROR(au_cc->country.TakeOver(country));
			au_cc->count = 1;
			OP_STATUS status = au_countries.Add(au_cc->country, au_cc);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(au_cc);
				return status;
			}
		}
		if (!candidate || candidate->count < au_cc->count)
			candidate = au_cc;
		last_entry = au_cc;
		++ count;
	}

	if (candidate)
	{
		// there is a candidate, check some additional conditions to decide if we can trust it more than country code reported by OS

		size_t min_count, min_days, min_percentage;
		if (product_type == PRODUCT_TYPE_OPERA_NEXT)
		{
			// conditions are relaxed for Opera Next to get more testing opportunities (DSK-347631)
			min_count = AU_COUNTRY_MIN_COUNT_PRODUCT_NEXT;
			min_days = AU_COUNTRY_MIN_DAYS_PRODUCT_NEXT;
			min_percentage = AU_COUNTRY_MIN_PERCENTAGE_PRODUCT_NEXT;
		}
		else
		{
			min_count = AU_COUNTRY_MIN_COUNT;
			min_days = AU_COUNTRY_MIN_DAYS;
			min_percentage = AU_COUNTRY_MIN_PERCENTAGE;
		}

		OP_ASSERT(candidate->last_date >= candidate->first_date);
		if (candidate->count >= min_count &&
			candidate->count * 100 >= count * min_percentage &&
			size_t(candidate->last_date - candidate->first_date) >= min_days * 24*60*60 &&
			size_t(now - candidate->last_date) <= AU_COUNTRY_MAX_DAYS_FROM_LAST_REPORT * 24*60*60)
		{
			RETURN_IF_ERROR(out_country.SetFromUTF8(candidate->country));
		}

		// for users upgrading from Opera 12 or older the condition is relaxed to only check
		// whether last entry in the file is at least min_days old (DSK-366548)
		OperaVersion twelve;
		RETURN_IF_ERROR(twelve.Set(UNI_L("12.00.1467")));
		if (g_run_type->m_type == StartupType::RUNTYPE_FIRST &&
			g_run_type->m_previous_version <= twelve &&
			size_t(now - last_entry->last_date) >= min_days * 24*60*60)
		{
			RETURN_IF_ERROR(out_country.SetFromUTF8(last_entry->country));
		}
	}

	return OpStatus::OK;
}

OP_STATUS ResourceUtils::DetermineAuCountry(OperaInitInfo *pinfo, OpString& country)
{
	OpString filename;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_HOME_FOLDER, pinfo, filename));
	RETURN_IF_ERROR(filename.Append(DESKTOP_RES_AU_REGION));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename));
	return DetermineAuCountry(g_desktop_product->GetProductType(), file, country);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ResourceUtils
{
/**
 * Single line in a list of lines read from file with country codes received from the Autoupdate server
 */
class AuCountryLineListElement : public CountedListElement<AuCountryLineListElement>
{
public:
	OpString8 line;
};
} // namespace


OP_STATUS ResourceUtils::AddAuCountry(OpFileDescriptor& file, OpStringC country)
{
	OpString8 country8;
	RETURN_IF_ERROR(country8.SetUTF8FromUTF16(country));

	OpString8 new_line;
	RETURN_IF_ERROR(new_line.AppendFormat("%lu %s\n", g_timecache->CurrentTime(), country8.CStr()));

	// if file doesn't exist then create it and write new line
	BOOL exists = FALSE;
	RETURN_IF_ERROR(file.Exists(exists));
	if (!exists)
	{
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE | OPFILE_TEXT));
		return file.Write(new_line.CStr(), new_line.Length());
	}

	// if file exists then read its content and if new line is sufficiently different
	// from the last one add it to the list and write everything back to the file

	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	CountedAutoDeleteList<AuCountryLineListElement> lines;

	while (!file.Eof())
	{
		OpString8 line_in;
		if (OpStatus::IsError(file.ReadLine(line_in)))
			break;
		if (line_in.HasContent())
		{
			AuCountryLineListElement* new_element;
			if (lines.Cardinal() + 1 >= AU_COUNTRY_MAX_LINES) // don't let it grow endlessly
			{
				new_element = lines.First();
				new_element->Out();
			}
			else
			{
				new_element = OP_NEW(AuCountryLineListElement, ());
				RETURN_OOM_IF_NULL(new_element);
			}
			RETURN_IF_ERROR(new_element->line.TakeOver(line_in));
			new_element->Into(&lines);
		}
	}

	time_t now = g_timecache->CurrentTime();

	// new_line is only added to the file if country is different than last added or if at least
	// 1 day passed since previous entry was added
	while (lines.Cardinal() > 0)
	{
		time_t last_timestamp;
		OpString8 last_country;
		AuCountryLineListElement* last = lines.Last();
		OP_STATUS status = ParseAuCountryLine(last->line, last_timestamp, last_country);
		RETURN_IF_MEMORY_ERROR(status);
		if (OpStatus::IsError(status))
		{
			last->Out();
			OP_DELETE(last);
		}
		else if (last_country.CompareI(country8) == 0 && last_timestamp + 24*60*60 > now)
		{
			return OpStatus::OK;
		}
		else break;
	}

	// reopen to write & truncate
	RETURN_IF_ERROR(file.Close());
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE | OPFILE_TEXT));

	for (AuCountryLineListElement* list_elm = lines.First(); list_elm; list_elm = list_elm->Suc())
	{
		RETURN_IF_ERROR(file.Write(list_elm->line.CStr(), list_elm->line.Length()));
		RETURN_IF_ERROR(file.Write("\n", 1));
	}

	return file.Write(new_line.CStr(), new_line.Length());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUtils::CountryCodeToRegion(OpFileDescriptor& region_file, OpStringC country, OpString& region, OpString& default_language)
{
	// Check if region.ini defines region for given country
	PrefsFile region_prefs(PREFS_INI, 0, 0);
	{
		OP_PROFILE_METHOD("Load region.ini");
		RETURN_IF_LEAVE(
			region_prefs.ConstructL(); \
			region_prefs.SetFileL(&region_file); \
			region_prefs.LoadAllL()
		);
	}

	OpString alias;
	RETURN_IF_LEAVE(region_prefs.ReadStringL(UNI_L("Region"), country, alias));
	if (alias.HasContent())
	{
		RETURN_IF_ERROR(region.TakeOver(alias));
	}
	else
	{
		// By default region=country
		RETURN_IF_ERROR(region.Set(country));
	}
	// Folders inside "region" folder are lower-case
	region.MakeLower();
	RETURN_IF_LEAVE(region_prefs.ReadStringL(UNI_L("Language"), country, default_language));
	return OpStatus::OK;
}

OP_STATUS ResourceUtils::CountryCodeToRegion(OperaInitInfo *pinfo, OpStringC country, OpString& region, OpString& default_language)
{
	OpString filename;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_REGION_ROOT_FOLDER, pinfo, filename));
	RETURN_IF_ERROR(filename.Append(DESKTOP_RES_REGION_CONFIG));

	OpFile region_file;
	RETURN_IF_ERROR(region_file.Construct(filename));

	return CountryCodeToRegion(region_file, country, region, default_language);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUtils::DetermineRegionInfo(PrefsFile *pf, OperaInitInfo *pinfo, OpDesktopResources *desktop_resources, RegionInfo& region_info)
{
	bool prefs_changed = false;
	bool region_changed = false, language_changed = false;
	bool from_os = false;

	bool upgrade = g_run_type->m_type == StartupType::RUNTYPE_FIRST || g_run_type->m_type == StartupType::RUNTYPE_FIRST_NEW_BUILD_NUMBER;

	// Read country, region, and default language used in previous session
	OpString prev_cc, prev_region, prev_language;
	RETURN_IF_LEAVE(pf->ReadStringL("State", "Active Country Code", prev_cc));
	RETURN_IF_LEAVE(pf->ReadStringL("State", "Active Region", prev_region));
	RETURN_IF_LEAVE(pf->ReadStringL("State", "Active Default Language", prev_language));

	// Determine country and region for current session
	OpString country_code;
	RETURN_IF_LEAVE(pf->ReadStringL("User Prefs", "Country Code", country_code)); // user's override
	if (country_code.IsEmpty())
	{
		// User didn't set country code, so we need to detect it.
		// Initially Opera uses country code preconfigured by installer or country code set in OS.
		// Esp. country code from OS may be wrong so Opera has an option to correct it using
		// country codes received over time from the autoupdate server. This however can only
		// happen on browser's upgrade to avoid switching customizations in random session.

		OpString prev_detected_cc;
		RETURN_IF_LEAVE(pf->ReadStringL("State", "Detected Country Code", prev_detected_cc));

		if (upgrade)
		{
			RETURN_IF_MEMORY_ERROR(DetermineAuCountry(pinfo, country_code));
		}

		if (country_code.IsEmpty())
		{
			if (prev_detected_cc.HasContent())
			{
				// If country code was detected previously then stick to that value - 
				// we don't want to switch customizations every time user changes
				// location setting in OS.
				RETURN_IF_ERROR(country_code.Set(prev_detected_cc));
			}
			else
			{
				// Country code retrieved by installer (DSK-349444)
				RETURN_IF_LEAVE(pf->ReadStringL("Auto Update", "Country Code", country_code));

				if (country_code.IsEmpty())
				{
					RETURN_IF_ERROR(desktop_resources->GetCountryCode(country_code));
					if (country_code.IsEmpty())
					{
						// There is no region/undefined folder so setting this to "undefined"
						// means Opera will load customizations from locale folder.
						// This also means that if OS evantually starts reporting country code
						// (e.g. after user fixes LC_ALL environment variable) Opera will not
						// suddenly reload partner customizations from region folder.
						RETURN_IF_ERROR(country_code.Set(UNI_L("undefined")));
					}
					from_os = true;
				}
			}
		}
		if (prev_detected_cc != country_code)
		{
			TRAPD(err, pf->WriteStringL("State", "Detected Country Code", country_code));
			RETURN_IF_MEMORY_ERROR(err);
			prefs_changed = true;
		}
	}

	if (country_code != prev_cc)
	{
		TRAPD(err, pf->WriteStringL("State", "Active Country Code", country_code));
		RETURN_IF_MEMORY_ERROR(err);
		prefs_changed = true;
	}

	OpString region, language;
	if (country_code != prev_cc || prev_region.IsEmpty() || prev_language.IsEmpty() || upgrade)
	{
		RETURN_IF_ERROR(CountryCodeToRegion(pinfo, country_code, region, language));

		if (region != prev_region)
		{
			TRAPD(err, pf->WriteStringL("State", "Active Region", region));
			RETURN_IF_MEMORY_ERROR(err);
			prefs_changed = true;
			region_changed = true;
		}
		if (language != prev_language)
		{
			TRAPD(err, pf->WriteStringL("State", "Active Default Language", language));
			RETURN_IF_MEMORY_ERROR(err);
			prefs_changed = true;
			language_changed = true;
		}
	}
	else
	{
		RETURN_IF_ERROR(region.TakeOver(prev_region));
		RETURN_IF_ERROR(language.TakeOver(prev_language));
	}

	if (prefs_changed)
	{
		TRAPD(err, pf->CommitL());
		RETURN_IF_MEMORY_ERROR(err);
	}

	RETURN_IF_ERROR(region_info.m_country.TakeOver(country_code));
	RETURN_IF_ERROR(region_info.m_region.TakeOver(region));
	RETURN_IF_ERROR(region_info.m_default_language.TakeOver(language));
	// country code is not checked for the flag, because it is not used to initialize paths
	// to customization folders
	region_info.m_changed = region_changed || language_changed;
	region_info.m_from_os = from_os;
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUtils::UpdateRegionInfo(OpStringC country_code, bool detected /*= true*/)
{
	OpString new_cc, new_region, new_default_language;
	if (country_code.HasContent())
	{
		if (g_region_info->m_country == country_code)
			return OpStatus::OK;
		RETURN_IF_ERROR(new_cc.Set(country_code));
	}
	else
	{
		if (g_region_info->m_country == UNI_L("undefined"))
			return OpStatus::OK;
		RETURN_IF_ERROR(new_cc.Set(UNI_L("undefined")));
	}
	RETURN_IF_ERROR(CountryCodeToRegion(new_cc, new_region, new_default_language));
	bool region_changed = false, language_changed = false;
	if (g_region_info->m_region != new_region)
	{
		region_changed = true;
		RETURN_IF_ERROR(g_region_info->m_region.TakeOver(new_region));
	}
	if (g_region_info->m_default_language != new_default_language)
	{
		language_changed = true;
		RETURN_IF_ERROR(g_region_info->m_default_language.TakeOver(new_default_language));
	}
	RETURN_IF_ERROR(g_region_info->m_country.TakeOver(new_cc));
	g_region_info->m_from_os = false; // assuming that only ResourceUtils::DetermineRegionInfo asks OS for country code

	// If this flag is already true (e.g. set by DetermineRegionInfo) it should stay true for the whole session,
	// so it is never set to false by this function.
	if (region_changed || language_changed)
		g_region_info->m_changed = true;

	TRAPD(status, g_pcui->WriteStringL(PrefsCollectionUI::ActiveCountryCode, g_region_info->m_country); \
		          if (detected) \
					  g_pcui->WriteStringL(PrefsCollectionUI::DetectedCountryCode, g_region_info->m_country); \
				  if (region_changed) \
					  g_pcui->WriteStringL(PrefsCollectionUI::ActiveRegion, g_region_info->m_region); \
				  if (language_changed) \
					  g_pcui->WriteStringL(PrefsCollectionUI::ActiveDefaultLanguage, g_region_info->m_default_language));
	// Signal only fatal errors to the caller - it should probably abort anyway in case of OOM.
	// Other errors are ignored, because prefs are not important for this session and Opera will just have
	// to reinitialize g_region_info from scratch on next start if prefs are not saved.
	RETURN_IF_MEMORY_ERROR(status);

	return OpStatus::OK;
}

#endif // USE_COMMON_RESOURCES
