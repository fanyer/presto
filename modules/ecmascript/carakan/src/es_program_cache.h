/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2004
 *
 * class ES_Runtime
 */

#ifndef ES_PROGRAM_CACHE_H
#define ES_PROGRAM_CACHE_H

#ifndef _STANDALONE
#include "modules/hardcore/mh/mh.h"
#endif // _STANDALONE
#include "modules/util/simset.h"

class ES_ProgramCodeStatic;

/**
 * This class makes sure we are able to reuse compiled scripts if the same
 * script is loaded again shortly after the previous usage.
 *
 * It will keep references to a number or recently used programs so that
 * they are not freed. Those references will be released after a small time
 * (currently 5 minutes or if an external source request the cache to shrink)
 * making sure that no long-term memory is consumed by this cache.
 *
 * A program is considered used both when it's created, when it's reused and
 * when it's freed (since freeing a program might mean navigating to a new
 * page on the same site).
 *
 * Future improvements:
 *
 *    * The cache lookup is potentially slow. Linear search through
 *      all programs in memory.
 *
 *    * Tracking/monitoring features.
 *
 *    * Better cache policies that would allow smaller caches with the same or better effect.
 */
class ES_Program_Cache
#ifndef _STANDALONE
	: public MessageObject
#endif // _STANDALONE
{
public:
	static ES_Program_Cache *MakeL();
	/**< Creates an ES_Program_Cache instance. */

	~ES_Program_Cache();

	void AddProgram(ES_ProgramCodeStatic *program);
	/**< Registers a program. It will be added to one of |referenced| and
	     |other|. */

	void RemoveProgram(ES_ProgramCodeStatic *program);
	/**< Drops a program from the lists. Must be called before the
	     program is destroyed if AddProgram has been called for it. */

	void TouchProgram(ES_ProgramCodeStatic *program);
	/**< Makes a program the most recently used program which will put
	     it in the same place in the cache as if it's just been added. */

	void Clear();
	/**< Empties the cache. Used when we run out of memory. */

	ES_ProgramCodeStatic *Find(ES_ProgramText *elements, unsigned elements_count);
	/**< Looks up a program in the cache. Will return NULL if nothing
	     is found. */

#ifdef ES_HEAP_DEBUGGER
	Head *GetCachedPrograms() { return &referenced; }
#endif // ES_HEAP_DEBUGGER

#ifndef _STANDALONE
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/**< From the MessageObject interface. */
#endif // _STANDALONE

private:
	ES_Program_Cache();

	enum
	{
		SMALL_PROGRAM_LIMIT = 20*1024,
		/**< Programs smaller than this aren't cached at all since we can recompile them easily. */

		CACHE_TIMEOUT = 300000,
		/**< The time in milliseconds programs are cached after their last use. */

		SCRIPT_EXPANSION_FACTOR = 20
		/**< The assumed character length -> memory usage expansion. */
	};

	size_t GetMaximumSize();
	/**< Calculates the maximum size of the cache. */

	void UpdateTimeout();
	/**< Makes sure the timeout for cache pruning is correct. */

	static unsigned Weight(ES_ProgramCodeStatic *program);
	/**< Estimates a memory usage for a program. */

	List<ES_ProgramCodeStatic> referenced;
	/**< The programs we cache even though they may (or may not) be
	     unused by nobody is currently using them. These are kept alive
	     by the cache. */

	List<ES_ProgramCodeStatic> other;
	/**< All known programs in memory except those in referenced. These
	     are not kept alive. */

	size_t referenced_total_weight;
	/**< The estimated size of the programs in the |referenced| list. */

	double next_cache_timeout;
	/**< The time we'll receive a message about clearing the
	   cache. Only valid if sent_timeout_message == TRUE. */

	BOOL sent_timeout_message;
	/**< Set when we've registered ourselves as listeners and posted a
	     message. */
};

#endif // ES_PROGRAM_CACHE_H
