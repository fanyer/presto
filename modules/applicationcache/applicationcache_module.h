/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_APPLICATIONCACHE_MODULE_H
#define MODULES_APPLICATIONCACHE_MODULE_H

#ifdef APPLICATION_CACHE_SUPPORT

class ApplicationCacheManager;
class SelftestAppCacheListener;


class ApplicationcacheModule : public OperaModule
{
public:
	ApplicationcacheModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	ApplicationCacheManager *m_application_cache_manager;

#ifdef SELFTEST
	/* Must only be used by selftests */
	SelftestAppCacheListener *m_application_cache_ui_listener;
#endif // SELFTEST

};

#define g_application_cache_manager g_opera->applicationcache_module.m_application_cache_manager

#ifdef SELFTEST
#define g_application_cache_ui_listener g_opera->applicationcache_module.m_application_cache_ui_listener
#endif // SELFTEST

#define APPLICATIONCACHE_MODULE_REQUIRED

#endif // APPLICATION_CACHE_SUPPORT

#endif // !MODULES_APPLICATIONCACHE_MODULE_H


