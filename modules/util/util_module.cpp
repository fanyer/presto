/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/util_module.h"

#ifdef PI_HOST_RESOLVER
#include "modules/util/network/pi_host_resolver.h"
#endif // PI_HOST_RESOLVER

#include "modules/util/opguid.h"

#ifdef UTIL_ZIP_CACHE
#include "modules/util/zipload.h"
#endif // UTIL_ZIP_CACHE

UtilModule::UtilModule() :
	m_unique_id_counter(1),
	m_folder_manager(NULL)
#ifdef UTIL_GET_THIS_YEAR
	, m_thistime(0)
	, m_thisyear(0)
	, m_thismonth(0)
#endif // UTIL_GET_THIS_YEAR
#ifdef PI_HOST_RESOLVER
	, m_host_resolver_manager(NULL)
#endif // PI_HOST_RESOLVER
#ifdef UTIL_GUID_GENERATE_SUPPORT
	, m_opguidmanager(NULL)
#endif
#ifdef UTIL_ZIP_CACHE
	, m_zipcache(NULL)
#endif // UTIL_ZIP_CACHE
{
#ifndef HAS_COMPLEX_GLOBALS
	InitDateFunArrays();
#endif // !HAS_COMPLEX_GLOBALS
}

void
UtilModule::InitL(const OperaInitInfo& info)
{
	m_folder_manager = OP_NEW_L(OpFolderManager, ());
	m_folder_manager->InitL();

	unsigned int i;
	for (i=2; i < OPFILE_FOLDER_COUNT; ++i)
	{
		OpFileFolder folder = (OpFileFolder)i;
		OP_ASSERT(info.default_folders[i].CStr() != NULL); // Don't forget to set the default folders
#ifdef FOLDER_PARENT_SUPPORT
		if (info.default_folder_parents[i] == OPFILE_ABSOLUTE_FOLDER)
			LEAVE_IF_ERROR(m_folder_manager->SetFolderPath(folder, info.default_folders[i], info.default_folder_parents[i]));
#else // FOLDER_PARENT_SUPPORT
		LEAVE_IF_ERROR(m_folder_manager->SetFolderPath(folder, info.default_folders[i].CStr()));
#endif // FOLDER_PARENT_SUPPORT
	}

#ifdef FOLDER_PARENT_SUPPORT
	for (i=2; i < OPFILE_FOLDER_COUNT; ++i)
	{
		OpFileFolder folder = (OpFileFolder)i;
		if (info.default_folder_parents[i] != OPFILE_ABSOLUTE_FOLDER)
			LEAVE_IF_ERROR(m_folder_manager->SetFolderPath(folder, info.default_folders[i], info.default_folder_parents[i]));

	}
#endif // FOLDER_PARENT_SUPPORT

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	OpFileFolder default_locale_folders_parent =
# ifdef FOLDER_PARENT_SUPPORT
		info.default_locale_folders_parent
# else // !FOLDER_PARENT_SUPPORT
		OPFILE_ABSOLUTE_FOLDER
# endif // FOLDER_PARENT_SUPPORT
		;
	for (i=0; i<info.GetDefaultLocaleFoldersCount(); ++i)
	{
		LEAVE_IF_ERROR(m_folder_manager->AddLocaleFolder(default_locale_folders_parent, info.GetDefaultLocaleFolders(i)));
	}
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

#ifdef UTIL_GUID_GENERATE_SUPPORT
	m_opguidmanager = OP_NEW_L(OpGUIDManager, ());
#endif

#ifdef UTIL_ZIP_CACHE
	m_zipcache = OP_NEW_L(ZipCache, ());
#endif // UTIL_ZIP_CACHE
}

void
UtilModule::Destroy()
{
#ifdef PI_HOST_RESOLVER
	OP_DELETE(m_host_resolver_manager);
	m_host_resolver_manager = NULL;
#endif // PI_HOST_RESOLVER

	OP_DELETE(m_folder_manager);
	m_folder_manager = NULL;

#ifdef UTIL_GUID_GENERATE_SUPPORT
	OP_DELETE(m_opguidmanager);
	m_opguidmanager = NULL;
#endif

#ifdef UTIL_ZIP_CACHE
	OP_DELETE(m_zipcache);
	m_zipcache = NULL;
#endif // UTIL_ZIP_CACHE
}
