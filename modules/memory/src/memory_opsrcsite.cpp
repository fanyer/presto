/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the OpSrcSite class for allocation tracking
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_opsrcsite.h"

// Should be prime
#define OPSRCSITE_HASHTABLE_SIZE   10007

// Must be large enough...
#define OPSRCSITE_MAX_SITES		   16384

size_t OpSrcSite::m_total_mem_used = 0;

OpSrcSite::OpSrcSite(const char* file, int line, int id ) :
	file(file), next(0), line(line), id(id)
#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
	, memused(0)
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	, callcount(0)
	, hasfailed(false)
#endif // MEMORY_FAIL_ALLOCATION_ONCE
{
}

OpSrcSite* OpSrcSite::GetSite(const char* file, int line)
{
	int k;

	if ( file == 0 )
		return &g_mem_opsrcsite_unknown_site;

	if ( g_mem_opsrcsite_hashtable == 0 )
	{
		if ( g_mem_opsrcsite_id2site == 0 )
		{
			g_mem_opsrcsite_id2site = (OpSrcSite**)
				op_lowlevel_malloc(OPSRCSITE_MAX_SITES*sizeof(OpSrcSite*));

			if ( g_mem_opsrcsite_id2site == 0 )
				return &g_mem_opsrcsite_oom_site;

			for ( k = 0; k < OPSRCSITE_MAX_SITES; k++ )
				g_mem_opsrcsite_id2site[k] = 0;

			g_mem_opsrcsite_id2site[0] = &g_mem_opsrcsite_error_site;
			g_mem_opsrcsite_id2site[1] = &g_mem_opsrcsite_oom_site;
			g_mem_opsrcsite_id2site[2] = &g_mem_opsrcsite_error_site;
			g_mem_opsrcsite_id2site[3] = &g_mem_opsrcsite_unknown_site;
		}

		g_mem_opsrcsite_hashtable = (OpSrcSite**)
			op_lowlevel_malloc(OPSRCSITE_HASHTABLE_SIZE*sizeof(OpSrcSite*));

		if ( g_mem_opsrcsite_hashtable == 0 )
			return &g_mem_opsrcsite_oom_site;

		for ( k = 0; k < OPSRCSITE_HASHTABLE_SIZE; k++ )
			g_mem_opsrcsite_hashtable[k] = 0;
	}

	{
		//
		// Hashing on the linenumber only may be sufficient, and even
		// desirable as we only have to search the one bucket, not all.
		//
		UINTPTR fileaddrbits = reinterpret_cast<UINTPTR>(file);
		UINT32 hashkey = (((UINT32)(fileaddrbits)) ^ (UINT32)line)
			% OPSRCSITE_HASHTABLE_SIZE;

		//
		// Check if a match allready exists
		//
		OpSrcSite* last = 0;
		OpSrcSite* site = g_mem_opsrcsite_hashtable[hashkey];

		while ( site != 0 )
		{
			if ( site->file == file && site->line == line )
				return g_mem_opsrcsite_id2site[site->id];
			last = site;
			site = site->next;
		}

		//
		// Maybe the string is stored in two different places in memory?
		// This search of all previous strings is slow, and we may want to
		// disable it later, when we know for certain that it is not a
		// problem (on various compilers).  We keep it in for the time being.
		//
		OpSrcSite* fresh;
		for ( k = 0; k < OPSRCSITE_HASHTABLE_SIZE; k++ )
		{
			site = g_mem_opsrcsite_hashtable[k];
			while ( site != 0 )
			{
				if ( site->line == line && !op_strcmp(site->file, file) )
				{
					//
					// Might want to keep track of this event somehow...
					// If we get here, it means the same filename is
					// strored at different locations, causing a much
					// larger executable than needed.
					//
					fresh = new OpSrcSite(file, line, site->id);
					if ( fresh == 0 )
						return &g_mem_opsrcsite_oom_site;
					goto site_created;
				}
				site = site->next;
			}
		}

		fresh = new OpSrcSite(file, line, g_mem_opsrcsite_next_id);
		if ( fresh == 0 )
			return &g_mem_opsrcsite_oom_site;

		g_mem_opsrcsite_next_id++;

	site_created:

		if ( last == 0 )
			g_mem_opsrcsite_hashtable[hashkey] = fresh;
		else
			last->next = fresh;

		if ( g_mem_opsrcsite_id2site[fresh->id] == 0 )
			g_mem_opsrcsite_id2site[fresh->id] = fresh;

		return g_mem_opsrcsite_id2site[fresh->id];
	}
}

OpSrcSite* OpSrcSite::GetSite(UINT32 siteid)
{
	if ( siteid == 0 || siteid >= g_mem_opsrcsite_next_id )
	{
		// This is a bogous site-id, return an error
		return &g_mem_opsrcsite_error_site;
	}

	return g_mem_opsrcsite_id2site[siteid];
}

const char* OpSrcSite::GetFile(void) const
{
	const char* search;

	if ( (search = op_strstr(file, "/modules/"))   != 0 ||
		 (search = op_strstr(file, "\\modules\\")) != 0 ||
		 (search = op_strstr(file, "/platforms/"))   != 0 ||
		 (search = op_strstr(file, "\\platforms\\")) != 0 ||
		 (search = op_strstr(file, "/adjunct/"))   != 0 ||
		 (search = op_strstr(file, "\\adjunct\\")) != 0 )
		return search + 1;

	return file;
}

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
void
OpSrcSite::PrintLiveAllocations(void)
{
	static UINT32 nScanCount = 0;
	dbg_printf("======= Live allocations ======= \n");
	int total = 0;

	for ( int k = 0; k < OPSRCSITE_MAX_SITES; k++ )
	{
		OpSrcSite* src = g_mem_opsrcsite_id2site[k];
		if ( src && src->GetMemUsed() != 0 )
		{
			char buffer[12]; /* ARRAY OK 2011-05-09 terjepe */
			int ret = op_snprintf(buffer, 12, "%04d", nScanCount);
			OP_ASSERT(ret>0 && ret<12);
			dbg_printf("MEM scan %s : %s:%d = %d (calls %d)\n", buffer, src->GetFile(), src->GetLine(), src->GetMemUsed(), 
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
				src->GetCallCount()
#else
				0
#endif MEMORY_FAIL_ALLOCATION_ONCE
				);

			total += src->GetMemUsed();
		}
	}
	nScanCount++;
	dbg_printf("================================ \n");
	dbg_printf("Total %d (%d)\n", total, OpMemDebug_GetTotalMemUsed());
	dbg_printf("================================ \n");
}
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
void
OpSrcSite::ClearCallCounts()
{
	for ( int k = 0; k < OPSRCSITE_MAX_SITES; k++ )
	{
		OpSrcSite* src = g_mem_opsrcsite_id2site[k];
		if ( src )
			src->SetCallCount(0);
	}
}
#endif // MEMORY_FAIL_ALLOCATION_ONCE

#endif // ENABLE_MEMORY_DEBUGGING
