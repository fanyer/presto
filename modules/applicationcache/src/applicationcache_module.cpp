/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/applicationcache_module.h"
#include "modules/applicationcache/application_cache_manager.h"

ApplicationcacheModule::ApplicationcacheModule()
	: m_application_cache_manager(NULL)
{
}

/* virtual */ void
ApplicationcacheModule::InitL(const OperaInitInfo& info)
{
	// FixMe: Temporary global variable, until we have a cache module class.
	LEAVE_IF_NULL(g_application_cache_manager = OP_NEW_L(ApplicationCacheManager, ()));
	LEAVE_IF_ERROR(g_application_cache_manager->Construct());
}

/* virtual */ void
ApplicationcacheModule::Destroy()
{
	OpStatus::Ignore(g_application_cache_manager->DeleteAllApplicationCacheGroups(TRUE)); /* will also save the cache index file and delete older caches. Only the newest caches in each group are kept. */
	g_application_cache_manager->Destroy();
}

#endif // APPLICATION_CACHE_SUPPORT
