/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/data_types/MasterPasswordLifetime.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

MasterPasswordLifetime &MasterPasswordLifetime::SetValue(int lifetime_in_seconds)
{
	m_value = MAX(lifetime_in_seconds,ALWAYS);

	return *this;
}

OP_STATUS MasterPasswordLifetime::ToString(OpString &result) const
{
	result.Empty();

	if (m_value < 60) //MINUTE
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDM_GET_PASSWORD_ALWAYS_RADIO, result));
	}
	else if (m_value < 60*60) //HOUR
	{
		RETURN_IF_ERROR(StringUtils::GetFormattedLanguageString(result, Str::S_EVERY_X_MINUTES, m_value / 60));
	}
	else if (m_value < 60*60*2) // 2 HOURS
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_EVERY_HOUR, result));
	}
	else if (m_value < 60*60*24) // DAY
	{
		RETURN_IF_ERROR(StringUtils::GetFormattedLanguageString(result, Str::S_EVERY_X_HOURS, m_value / (60 * 60)));
	}
	else
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDM_GET_PASSWORD_ONCE_RADIO, result));
	}

	return OpStatus::OK;
}

void MasterPasswordLifetime::Write() const
{
	TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::SecurityPasswordLifeTime,
			GetValue() / 60));
}

void MasterPasswordLifetime::Read()
{
	SetValue(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SecurityPasswordLifeTime) * 60);
}


