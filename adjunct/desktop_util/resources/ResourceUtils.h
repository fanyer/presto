/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef RESOURCE_UTILS_H
#define RESOURCE_UTILS_H

#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

class OperaInitInfo;
class PrefsFile;
class OpDesktopResources;

namespace ResourceUtils
{
	/**
	 * Initialises all of the resources Opera needs, sets up
	 * all of the default files and upgrades any resources
	 * changes from previous versions
	 *
	 * @param pinfo  pointer to the opera init info
	 * @param profile_name  pointer to profile folder to use instead of the
	 *						standard "Opera" one
	 *
	 * @return OpStatus::OK everything initalises OK
	 */
    OP_STATUS Init(OperaInitInfo *pinfo, const uni_char *profile_name = NULL);

	/**
	 * Determines the run type by looking at the MaxVersionRun
	 * Note: this function will not determine first runs
	 *
	 * @param pf  prefs file to check for the last run version
	 *
	 * @return OpStatus::OK if the run type is determined or already set
	 */
	OP_STATUS DetermineRunType(PrefsFile *pf);


	const size_t AU_COUNTRY_MAX_LINES = 30; //< maximum number of lines in DESKTOP_RES_AU_REGION file

	// additional conditions that country code received from autoupdate server must meet
	// to be chosen over country code reported by the OS
	const size_t AU_COUNTRY_MIN_COUNT = 10;      //< minimum number of times country code was received from autoupdate server
	const size_t AU_COUNTRY_MIN_PERCENTAGE = 70; //< minimum percentage of times country code was received from autoupdate server (max. 100)
	const size_t AU_COUNTRY_MIN_DAYS = 28;       //< minimum number of days between first and last time country code was reported
	const size_t AU_COUNTRY_MAX_DAYS_FROM_LAST_REPORT = 7; //< maximum number of days since country code was reported last time

	// more aggresive settings for Opera Next (DSK-347631)
	const size_t AU_COUNTRY_MIN_COUNT_PRODUCT_NEXT = 2;
	const size_t AU_COUNTRY_MIN_DAYS_PRODUCT_NEXT = 5;
	const size_t AU_COUNTRY_MIN_PERCENTAGE_PRODUCT_NEXT = 55;

	/**
	 * Adds country code received from the Autoupdate server to the list in a file
	 *
	 * @param file where to add country
	 * @param country country code received from the server
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, IO errors
	 */
	OP_STATUS AddAuCountry(OpFileDescriptor& file, OpStringC country);

	/**
	 * Adds country code received from the autoupdate server to the list
	 * in the DESKTOP_RES_AU_REGION file
	 *
	 * @param country country code received from the server
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, IO errors
	 */
	inline OP_STATUS AddAuCountry(OpStringC country)
	{
		OpFile file;
		RETURN_IF_ERROR(file.Construct(DESKTOP_RES_AU_REGION, OPFILE_HOME_FOLDER));
		return AddAuCountry(file, country);
	}

	/**
	 * Determines user's country code using a list of country codes received
	 * from the Autoupdate server. Received country codes are read from 
	 * the DESKTOP_RES_AU_REGION file.
	 * This function uses current product type to select rules used to
	 * determine country code.
	 *
	 * @param pinfo pointer to the opera init info
	 * @param country gets country code or empty string
	 *
	 * @return OK if there were no errors
	 */
	OP_STATUS DetermineAuCountry(OperaInitInfo *pinfo, OpString& country);

	/**
	 * Determines user's country code using a list of country codes received
	 * from the Autoupdate server.
	 * Rules used to determine country code are different for different
	 * product types.
	 *
	 * @param product_type type of Opera product
	 * @param file file with country codes received from the Autoupdate server
	 * @param country gets country code or empty string
	 *
	 * @return OK if there were no errors
	 */
	OP_STATUS DetermineAuCountry(DesktopProductType product_type, OpFileDescriptor& file, OpString& country);

	/**
	 * Returns region code and default language for given country code.
	 * Values returned by this function are read from region_file.
	 * If the file does not specify region code for given country code then
	 * this function returns country code as region code.
	 * If the file does not specify default language for given country code
	 * then this function returns empty string as default language.
	 *
	 * @param region_file INI file with region definitions
	 * @param country country code in ISO 3166 alpha-2 format
	 * @param region returns name of the region (always lower-case)
	 * @param default_language returns default language or empty string
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS CountryCodeToRegion(OpFileDescriptor& region_file, OpStringC country, OpString& region, OpString& default_language);

	/**
	 * Returns region code and default language for given country code.
	 * Values returned by this function are read from DESKTOP_RES_REGION_CONFIG file.
	 *
	 * @see CountryCodeToRegion(OpFileDescriptor&, OpStringC, OpString&, OpString&)

	 * @param pinfo pointer to the init info
	 * @param country country code in ISO 3166 alpha-2 format
	 * @param region returns name of the region (always lower-case)
	 * @param default_language returns default language or empty string
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS CountryCodeToRegion(OperaInitInfo *pinfo, OpStringC country, OpString& region, OpString& default_language);

	/**
	 * Returns region code and default language for given country code.
	 * Values returned by this function are read from DESKTOP_RES_REGION_CONFIG file.
	 * This function requires that Core is initialized.
	 *
	 * @see CountryCodeToRegion(OpFileDescriptor&, OpStringC, OpString&, OpString&)
	 *
	 * @param country country code in ISO 3166 alpha-2 format
	 * @param region returns name of the region (always lower-case)
	 * @param default_language returns default language or empty string
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	inline OP_STATUS CountryCodeToRegion(OpStringC country, OpString& region, OpString& default_language)
	{
		OpFile region_file;
		RETURN_IF_ERROR(region_file.Construct(DESKTOP_RES_REGION_CONFIG, OPFILE_REGION_ROOT_FOLDER));
		return CountryCodeToRegion(region_file, country, region, default_language);
	}

	/**
	 * Determines country code, name of the region of the world, and default language.
	 * Updates region and country information in prefs file passed in pf argument.
	 *
	 * @param pf prefs file to check for region used in previous session and for user's override
	 * @param pinfo pointer to the OperaInitInfo
	 * @param dekstop_resources pointer to the OpDesktopResources
	 * @param region_info returns country code, region name and whether region is different than in previous session
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS DetermineRegionInfo(PrefsFile *pf, OperaInitInfo *pinfo, OpDesktopResources *desktop_resources, RegionInfo& region_info);

	/**
	 * Updates attributes of g_region_info object and related prefs using given country_code.
	 * Prefs collections in Core must be initialized before this function is called.
	 *
	 * @param country_code new country code
	 * @param detected whether country_code was detected by Opera or provided by user 
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS UpdateRegionInfo(OpStringC country_code, bool detected = true);
};

#endif // RESOURCE_UTILS_H
