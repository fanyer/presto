/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_UTIL_MODULE_H
#define MODULES_UTIL_UTIL_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/timecache.h"

#ifdef UTIL_ZIP_CACHE
class ZipCache;
#endif // UTIL_ZIP_CACHE

class UtilModule : public OperaModule
{
public:
	UtilModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	INT32 m_unique_id_counter;
	OpFolderManager* m_folder_manager;
#ifdef USE_UTIL_OPSTRING_LIST
	OpString m_empty_opstring;
#endif // USE_UTIL_OPSTRING_LIST
	TimeCache m_timecache;
#ifdef UTIL_GET_THIS_YEAR
	time_t m_thistime;
	int m_thisyear;
	char m_thismonth;
#endif // UTIL_GET_THIS_YEAR
#ifdef PI_HOST_RESOLVER
	class PiOpHostResolverManager* m_host_resolver_manager;
#endif // PI_HOST_RESOLVER
#ifdef UTIL_GUID_GENERATE_SUPPORT
	class OpGUIDManager *m_opguidmanager;
#endif

#ifndef HAS_COMPLEX_GLOBALS
	void InitDateFunArrays();
	const uni_char* m_wkday[7];
	const uni_char* m_weekday[7];
	const uni_char* m_month[12];
#endif // !HAS_COMPLEX_GLOBALS

#ifdef UTIL_ZIP_CACHE
	ZipCache *m_zipcache;
#endif // UTIL_ZIP_CACHE
};

#define g_folder_manager g_opera->util_module.m_folder_manager
#define g_unique_id_counter g_opera->util_module.m_unique_id_counter
#ifdef USE_UTIL_OPSTRING_LIST
#define g_empty_opstring (&(g_opera->util_module.m_empty_opstring))
#endif // USE_UTIL_OPSTRING_LIST
#define g_timecache (&(g_opera->util_module.m_timecache))
#ifdef UTIL_GET_THIS_YEAR
#define g_thistime g_opera->util_module.m_thistime
#define g_thisyear g_opera->util_module.m_thisyear
#define g_thismonth g_opera->util_module.m_thismonth
#endif // UTIL_GET_THIS_YEAR
#ifdef PI_HOST_RESOLVER
#define g_pi_host_resolver_manager g_opera->util_module.m_host_resolver_manager
#endif // PI_HOST_RESOLVER

#ifndef HAS_COMPLEX_GLOBALS
#define g_wkday g_opera->util_module.m_wkday
#define g_weekday g_opera->util_module.m_weekday
#define g_month g_opera->util_module.m_month
#endif // !HAS_COMPLEX_GLOBALS

#define UTIL_MODULE_REQUIRED

#endif // !MODULES_UTIL_UTIL_MODULE_H
