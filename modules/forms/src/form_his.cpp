/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/forms/form_his.h"

#include "modules/hardcore/unicode/unicode.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

// Needed by Qt that defines an ARRAY_SIZE of its own.
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)      (sizeof(arr)/sizeof(arr[0]))
#endif


/***********************************************************************************
**
**	Dummy FormsHistory class for now just to get autocomplete in forms
**	implemented...  currently only matches against Personal
**	Information content.
**
***********************************************************************************/

static OP_STATUS CompareStringsLocaleSensitive(int& comparison_result, const uni_char *str1, const uni_char *str2, long len=-1, BOOL ignore_case=FALSE)
{
	TRAPD(rc, comparison_result = g_oplocale->CompareStringsL(str1, str2, len, ignore_case));
	return rc;
}

/**
 * Used by qsort in FormsHistory::GetItems
 */
static int CompareAddresses(const void *arg1, const void *arg2)
{
	uni_char* str1 = *((uni_char**)arg1);
	uni_char* str2 = *((uni_char**)arg2);

	int retval = 0;
	OP_STATUS rc = CompareStringsLocaleSensitive(retval, str1, str2);
	if (OpStatus::IsMemoryError(rc))
		g_memory_manager->RaiseCondition(rc);
	return retval;
}

OP_STATUS FormsHistory::GetItems(const uni_char* in_MatchString, uni_char*** out_Items, int* out_ItemCount)
{
	int					len			= uni_strlen(in_MatchString);
	uni_char**			items		= NULL;
	int					itemCount	= 0;


#define PREFS_NAMESPACE PrefsCollectionCore

	const int dummies_count = 2;
	const PREFS_NAMESPACE::stringpref personal_info_source[] =
	{
		PREFS_NAMESPACE::Firstname, PREFS_NAMESPACE::Surname,  PREFS_NAMESPACE::Address,
		PREFS_NAMESPACE::City,      PREFS_NAMESPACE::State,    PREFS_NAMESPACE::Country,
		PREFS_NAMESPACE::Telephone, PREFS_NAMESPACE::Telefax,  PREFS_NAMESPACE::EMail,
		PREFS_NAMESPACE::Home,      PREFS_NAMESPACE::Special1, PREFS_NAMESPACE::Special2,
		PREFS_NAMESPACE::Special3,  PREFS_NAMESPACE::Zip
	};

	OpString personal_info[ARRAY_SIZE(personal_info_source) + dummies_count];
	// dummies_count dummies are used to create "<firstname> <surname>" and "<surname> <firstname>"

	int	personal_info_count = ARRAY_SIZE(personal_info);

	int i = 0;

	while (i < personal_info_count)
	{
		if (i < personal_info_count - dummies_count)
		{
			RETURN_IF_ERROR(personal_info[i].Set(g_pccore->GetStringPref(personal_info_source[i])));
		}
		else
		{
			// Create dummies "<firstname> <surname>" and "<surname> <firstname>"
			const uni_char* firstname = g_pccore->GetStringPref(PREFS_NAMESPACE::Firstname).CStr();
			const uni_char* surname = g_pccore->GetStringPref(PREFS_NAMESPACE::Surname).CStr();
			
			if (firstname && *firstname && surname && *surname)
			{
				if (i == personal_info_count - dummies_count)
				{
					RETURN_IF_ERROR(personal_info[i].SetConcat(firstname, UNI_L(" "), surname));
				}
				else
				{
					OP_ASSERT(i == personal_info_count - dummies_count + 1);
					RETURN_IF_ERROR(personal_info[i].SetConcat(surname, UNI_L(" "), firstname));
				}
			}
		}

		if (!personal_info[i].IsEmpty())
		{
			int comp_result;
			OP_STATUS rc = CompareStringsLocaleSensitive(comp_result, personal_info[i].CStr(), in_MatchString, len, TRUE);
			if (OpStatus::IsMemoryError(rc))
				g_memory_manager->RaiseCondition(rc);
			if (OpStatus::IsSuccess(rc) && len && comp_result == 0 && uni_strcmp(personal_info[i].CStr(), in_MatchString))
			{
				itemCount++;
			}
			else
			{
				personal_info[i].Empty();
			}
		}

		i++;
	}

	if (itemCount > 0)
	{
		items = OP_NEWA(uni_char*, itemCount);
		if (!items)
			return OpStatus::ERR_NO_MEMORY;

		i = 0;
		int j = 0;

		while (i < personal_info_count)
		{
			if (!personal_info[i].IsEmpty())
			{
				items[j] = OP_NEWA(uni_char, personal_info[i].Length() + 1);
				if (!items[j])
				{
					for ( ; j > 0 ; --j )
						OP_DELETEA(items[j-1]);
					OP_DELETEA(items);
					return OpStatus::ERR_NO_MEMORY;
				}
				uni_strcpy(items[j], personal_info[i].CStr());
				j++;
			}
			i++;
		}
		op_qsort(items, itemCount, sizeof(uni_char*), CompareAddresses);
	}

	*out_Items		= items;
	*out_ItemCount	= itemCount;
	return OpStatus::OK;
}

