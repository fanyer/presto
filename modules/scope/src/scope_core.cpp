/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_CORE

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_core.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

OpScopeCore::OpScopeCore()
	: OpScopeCore_SI()
{
}

/* virtual */ OP_STATUS
OpScopeCore::OnServiceEnabled()
{
	g_idle_detector->AddListener(this);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
OpScopeCore::OnServiceDisabled()
{
	g_idle_detector->RemoveListener(this);
	return OpStatus::OK;
}

/* virtual */
OpScopeCore::~OpScopeCore()
{
}

/* virtual */ OP_STATUS
OpScopeCore::OnActivityStateChanged(OpActivityState state)
{
	if (!IsEnabled())
		return OpStatus::OK;

	switch (state)
	{
	case ACTIVITY_STATE_IDLE:
		return SendOnIdle();
	case ACTIVITY_STATE_BUSY:
		return SendOnActive();
	case ACTIVITY_STATE_PRE_IDLE:
		// ignore
		return OpStatus::OK;
	default:
		OP_ASSERT(!"Unknown case!");
		return OpStatus::OK;
	}
}

static int
ConvertClearFlag(enum OpScopeCore_SI::ClearFlags flag)
{
	switch (flag)
	{
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_ALL:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_ALL;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_VISITED_LINKS:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_VISITED_LINKS;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_DISK_CACHE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_DISK_CACHE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_IMAGE_CACHE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_IMAGE_CACHE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_MEMORY_CACHE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_MEMORY_CACHE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_SENSITIVE_DATA:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_SENSITIVE_DATA;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_SESSION_COOKIES:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_SESSION_COOKIES;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_ALL_COOKIES:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_ALL_COOKIES;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_GLOBAL_HISTORY:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_GLOBAL_HISTORY;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_CONSOLE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_CONSOLE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_THUMBNAILS:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_THUMBNAILS;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_WEBDATABASES:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_WEBDATABASES;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_WEBSTORAGE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_WEBSTORAGE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_APPCACHE:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_APPCACHE;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_GEOLOCATION_PERMISSIONS:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_GEOLOCATION_PERMISSIONS;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_CAMERA_PERMISSIONS:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_CAMERA_PERMISSIONS;
	case OpScopeCore_SI::CLEARFLAGS_CLEAR_SITE_PREFS:
		return OpWindowCommanderManager::WIC_OPERA_CLEAR_SITE_PREFS;
	}

	OP_ASSERT(!"Unknown clear flag used");
	return 0;
}

OP_STATUS
OpScopeCore::DoClearPrivateData(const ClearPrivateDataArg &in)
{
	if (!g_windowCommanderManager)
		return OpStatus::ERR;

	int clear_flags = 0;

	unsigned count = in.GetClearList().GetCount();

	for (unsigned int i = 0; i < count; ++i)
		clear_flags |= ConvertClearFlag(in.GetClearListItem(i));

	if (!clear_flags)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No known flag specified"));

	g_windowCommanderManager->ClearPrivateData(clear_flags);

	return OpStatus::OK;
}

OP_STATUS
OpScopeCore::DoGetBrowserInformation(BrowserInformation &out)
{
	RETURN_IF_ERROR(out.SetCoreVersion(GetManager()->GetCoreVersion()));
	RETURN_IF_ERROR(out.SetOperatingSystem(g_op_system_info->GetPlatformStr()));

	char ua_str[256]; // ARRAY OK 2008-11-14 jborsodi
	g_uaManager->GetUserAgentStr(ua_str, sizeof(ua_str), (const uni_char*)0);

	RETURN_IF_ERROR(out.SetUserAgent(ua_str));

#ifdef OPSYSTEMINFO_GETBINARYPATH
	RETURN_IF_ERROR(g_op_system_info->GetBinaryPath(&out.GetBinaryPathRef()));
#endif // OPSYSTEMINFO_GETBINARYPATH

#ifdef OPSYSTEMINFO_GETPID
	out.SetProcessID(g_op_system_info->GetCurrentProcessId());
#endif // OPSYSTEMINFO_GETPID

	RETURN_IF_ERROR(out.SetProduct(OP_PRODUCT_NAME));

	return OpStatus::OK;
}

#endif // SCOPE_CORE
