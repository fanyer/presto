/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/selftest/src/ST_autoupdatexml.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"

ST_AutoUpdateXML::ST_AutoUpdateXML()
{
	for (int i=0; i<TSI_LAST_ITEM; i++)
		m_overrided_timestamps[i] = -1;
}

OP_STATUS ST_AutoUpdateXML::SetVersion(const OpStringC8 &version)
{
	RETURN_IF_ERROR(m_opera_version.Set(version));
	OpString v;
	RETURN_IF_ERROR(v.Set(m_opera_version));
	RETURN_IF_ERROR(v.Append("."));
	RETURN_IF_ERROR(v.Append(m_build_number));
	AutoUpdater::GetOperaVersion().Set(v);
	return OpStatus::OK;
}

OP_STATUS ST_AutoUpdateXML::SetBuildNum(const OpStringC8 &buildnum)
{
	RETURN_IF_ERROR(m_build_number.Set(buildnum));
	OpString v;
	RETURN_IF_ERROR(v.Set(m_opera_version));
	RETURN_IF_ERROR(v.Append("."));
	RETURN_IF_ERROR(v.Append(m_build_number));
	AutoUpdater::GetOperaVersion().Set(v);
	return OpStatus::OK;
}

int ST_AutoUpdateXML::GetTimeStamp(TimeStampItem item)
{
	switch (item)
	{
	case TSI_BrowserJS:
	case TSI_OverrideDownloaded:
	case TSI_DictionariesXML:
	case TSI_HardwareBlocklist:
	case TSI_LastUpdateCheck:
		if (m_overrided_timestamps[item] >= 0)
			return m_overrided_timestamps[item];
		else
			return AutoUpdateXML::GetTimeStamp(item);
	default:
		OP_ASSERT(!"Unknown item");
	}
	return -1;
}

OP_STATUS ST_AutoUpdateXML::SetTimeStamp(TimeStampItem item, int timestamp)
{
	if (item < TSI_LAST_ITEM)
	{
		m_overrided_timestamps[item] = timestamp;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}


#endif // SELFTEST
#endif // AUTO_UPDATE_SUPPORT
