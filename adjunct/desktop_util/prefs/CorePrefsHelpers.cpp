/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/prefs/CorePrefsHelpers.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"

void CorePrefsHelpers::ApplyMemoryManagerPrefs()
{
	unsigned long fig_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::ImageRAMCacheSize);
	unsigned long doc_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::DocumentCacheSize);

	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache))
	{
		// Try to take up to 1/10 of system memory
		UINT32 size_in_MB = g_op_system_info->GetPhysicalMemorySizeMB();
		fig_cache_size = (size_in_MB * 1024) / 10;
		doc_cache_size = (size_in_MB * 1024) / 10;

		// Cache should be at least 2 MB
		fig_cache_size = max(fig_cache_size, (unsigned long) 2000);
		doc_cache_size = max(doc_cache_size, (unsigned long) 2000);

		// Cache should be at most QUICK_MAX_IMAGE_CACHE / QUICK_MAX_DOCUMENT_CACHE
		fig_cache_size = min(fig_cache_size, (unsigned long) QUICK_MAX_IMAGE_CACHE * 1024);
		doc_cache_size = min(doc_cache_size, (unsigned long) QUICK_MAX_DOCUMENT_CACHE * 1024);
	}

	g_memory_manager->SetMaxImgMemory(fig_cache_size * 1024);
	g_memory_manager->SetMaxDocMemory(doc_cache_size * 1024);
}
