/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
** This class is the implementation of a context manager dedicated to Multimedia content (video tag)
*/

#ifndef CACHE_CTX_MAN_MULTIMEDIA_H
#define CACHE_CTX_MAN_MULTIMEDIA_H

#include "modules/cache/cache_ctxman_disk.h"


// For now, Multimedia support requires a disk
#ifdef DISK_CACHE_SUPPORT

// Context manager dedicated to Multimedia content (video tag)
class Context_Manager_Multimedia: public Context_Manager_Disk
{
#ifdef SELFTEST
public:
#else
private:
#endif
	Context_Manager_Multimedia(URL_CONTEXT_ID a_id, OpFileFolder a_vlink_loc, OpFileFolder a_cache_loc): Context_Manager_Disk(a_id, a_vlink_loc, a_cache_loc)
	{ }

public:
	/** Create a proper Multimedia Context Manager chain and add it to the URL Manager
		@param a_id Context id, usually retrieved using urlManager->GetNewContextID()
		@param a_vlink_loc Visited Link file
		@param a_cache_loc Folder that will contain the cache
		@param a_cache_size_pref Preference used to get the size in KB; <0 means default size (5% of the main cache, set by PrefsCollectionNetwork::DiskCacheSize)
	 */
	static OP_STATUS CreateManager(URL_CONTEXT_ID a_id, OpFileFolder a_vlink_loc, OpFileFolder a_cache_loc, BOOL share_cookies_with_main_context = FALSE, int a_cache_size_pref=-1);
};

#endif // DISK_CACHE_SUPPORT

#endif // CACHE_CTX_MAN_MULTIMEDIA_H
