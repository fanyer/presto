/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file contains the implementation for UpdatableResource with 
 * subclasses.
 * @author Marius Blomli mariusab@opera.com
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/updatablesetting.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "adjunct/desktop_util/resources/ResourceUtils.h"

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
///                                                                                          ///
///  UpdatableSetting                                                                        ///
///                                                                                          ///
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

UpdatableSetting::UpdatableSetting()
{
}

OP_STATUS UpdatableSetting::UpdateResource()
{
	OpString8 key8, section8;
	OpString key, section, data;
	RETURN_IF_ERROR(GetAttrValue(URA_SECTION, section));
	RETURN_IF_ERROR(GetAttrValue(URA_KEY, key));
	RETURN_IF_ERROR(GetAttrValue(URA_DATA, data));

	if (section == "Auto Update" && key == "Country Code")
	{
		if (data == "A6")
		{
			// A6 means "Unknown Country" - see discussion in BINT-76
			data.Empty();
		}
		if (data.HasContent())
		{
			// Save received Country Code. At the next upgrade Opera will inspect the list of saved
			// country codes and may decide to switch region used when selecting default customization
			// files (bookmarks, speed dials, etc.) - see DSK-344623.
			// AddAuCountry must be called even if received country code is the same as
			// current value of the pref, so calling it here insted of from PrefChanged (DSK-366548).
			OpStatus::Ignore(ResourceUtils::AddAuCountry(data));
		}
	}

	RETURN_IF_ERROR(section8.Set(section.CStr()));
	RETURN_IF_ERROR(key8.Set(key.CStr()));
	BOOL ok;
	OpString oldvalue;
	TRAPD(err, ok = g_prefsManager->GetPreferenceL(section8.CStr(), key8.CStr(), oldvalue));
	if(OpStatus::IsSuccess(err) && ok)
	{
		if(oldvalue.Compare(data) == 0)
		{
			// Preference not changed
			return OpStatus::OK;
		}
	}
	TRAP(err, ok = g_prefsManager->WritePreferenceL(section8.CStr(), key8.CStr(), data));
	if(OpStatus::IsError(err))
		return err;

	TRAP(err, g_prefsManager->CommitL());

	if(ok)
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}

BOOL UpdatableSetting::IsUpdateCheckInterval()
{
	OpString section, key;
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_SECTION, section), FALSE);
	RETURN_VALUE_IF_ERROR(GetAttrValue(URA_KEY, key), FALSE);
	if (section.IsEmpty() || key.IsEmpty())
		return FALSE;

	if(section.CompareI("Auto Update") == 0 && key.CompareI("Update Check Interval") == 0)
		return TRUE;

	return FALSE;
}

BOOL UpdatableSetting::VerifyAttributes()
{
	return HasAttrWithContent(URA_FILE) && HasAttrWithContent(URA_SECTION) && HasAttrWithContent(URA_KEY);
}

////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTO_UPDATE_SUPPORT
