/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
** Luca Venturi
**
** This file contains some classes used for debugging the cache
** Even if only partially related to this file, this is the current list of "debugging keys" (for tracing or debugging code) that can be enabled with debug.txt:
** cache						// More generic log, above all for containers and File_Storage
** cache.ctx				  	// Logs creation of cache contexts
** cache.del                  	// Logs when a cache file is deleted
** cache.dbg 					// Used to enable slow debug code in Context_Manager_Disk::CheckInvariants()
** cache.index                	// Track URLs added (and not added) to the index
** cache.name                 	// Logs file names associated with the cache
** cache.limit				    // Logs the actions changing the disk/ram/oem cache size. It could be useful to activate it with cache.storage
** cache.storage				// Monitor RetrieveData(), StoreData() and OpenFile() in Cache_Storage
** multimedia_cache.read        // Logs read operations in the Multimedia Cache
** multimedia_cache.write 		// Logs write operations in the Multimedia Cache
*/

#ifndef CACHE_DEBUG_H

#define CACHE_DEBUG_H

class CacheTester;
#include "modules/selftest/testutils.h"
#include "modules/url/url_rep.h"

#ifdef SELFTEST
class URL_Rep;

/// Class used to debug FreeUnusedResources (check also CORE-24745)
class DebugFreeUnusedResources
{
friend class Context_Manager;
friend class Cache_Manager;

private:
	UINT32 mng_finished;		///< Number of managers that were able to finish
	UINT32 mng_processed;		///< Number of managers that were processed (should be man_finished or man_finished+1)
	UINT32 url_processed;		///< Number of total URLs processed
	UINT32 url_deleted;			///< Number of URLs that have been removed and deleted
	UINT32 url_skipped;			///< URLs skipped by FreeUnusedResource()
	UINT32 url_restart;			///< How many times FreeUnusedResources() started again from the beginning
	UINT32 ms_cache_chosen;		///< Number of chosen iterations before checking the time
	double time;				///< Approx Time used by FreeUnusedResources()
	double time_skip;			///< Approx Time used to skip resources

public:
	DebugFreeUnusedResources(UINT32 simulated_loops=1): mng_finished(0), mng_processed(0), url_processed(0), url_deleted(0), url_skipped(0), url_restart(0), ms_cache_chosen(0), time(0), time_skip(0){ }

	/// Output data for selftests
	void OutputData()
	{
		output("mng %d/%d - URLs: %d (%d deleted, %dK%s skip in %d ms - total: %dK - resets: %d) - time: %d\n", mng_finished, mng_processed, url_processed, url_deleted, url_skipped / 1000, (url_restart) ? "???" : "", (UINT32)time_skip, (url_skipped + url_processed) / 1000, url_restart, (UINT32) time);
	}
};
#endif // SELFTEST

// Debugging code macros
#if defined (DEBUG_ENABLE_OPASSERT) && defined(_DEBUG)
	#define DEBUGGING

	#define DEBUG_CODE_BEGIN(KEY) if(Debug::DoDebugging(KEY)) {
	#define DEBUG_CODE_END }
#endif
#endif // CACHE_DEBUG_H
