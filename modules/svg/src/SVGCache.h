/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef _SVG_CACHE_
#define _SVG_CACHE_

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/util/simset.h"

class SVGCacheData;

// This number should be more dynamic.
// 40MB is good for some, too small for some and too big for some
#ifndef DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE
# define SVG_CACHED_BITMAPS_MAX_BYTES_SIZE (40*1024*1024)
#endif // DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE

class SVGCache
{
public:
	// Types of cachable objects, to be able to use different keys
	enum CacheType
	{
		RENDERER,
		SURFACE
	};

	SVGCache() {}
	~SVGCache() { Clear(); }

	OP_STATUS		Add(CacheType type, void* key, SVGCacheData* data);
	SVGCacheData*	Get(CacheType type, void* key);
	void			Remove(CacheType type, void* key);
  	void			Clear();

private:
	struct LRUListElm : public Link	
	{
		LRUListElm(CacheType type, void* key);
		~LRUListElm();

		CacheType type;
		void* key;
		SVGCacheData* data;
	};
	
	/**
	 * Returns the cache element owned by the specified key. If no
	 * element was found and create was FALSE, NULL is returned. If no
	 * element was found in the cache and create was TRUE it will be
	 * inserted. If create is TRUE and the method returns NULL, then
	 * it was an OOM.
	 */
  	LRUListElm*		GetCacheElm(CacheType type, void* key, BOOL create = FALSE);

	/**
	 * Determine if the cache is full (based on the number of
	 * elements and the estimated size), and evict the least recently
	 * used element if so.
	 */
	void			EvictIfFull();

	unsigned int	EstimateMemoryUsage(LRUListElm* lru_elm);

	Head			m_lrulist;
};

class SVGCacheData 
{
public:
	virtual ~SVGCacheData() {}
	virtual unsigned int GetMemUsed() = 0;
	virtual BOOL IsEvictable() = 0;
};

#endif // SVG_SUPPORT
#endif // _SVG_CACHE_
