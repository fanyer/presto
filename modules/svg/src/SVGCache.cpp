/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGCache.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

OP_STATUS SVGCache::Add(CacheType type, void* key, SVGCacheData* data)
{
	if(!data)
		return OpStatus::ERR_NULL_POINTER;

	LRUListElm* lru_elm = GetCacheElm(type, key, TRUE);
	if (!lru_elm)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(!lru_elm->data || !"Possible memory leak");
	lru_elm->data = data;
	return OpStatus::OK;
}

SVGCacheData* SVGCache::Get(CacheType type, void* key)
{
	LRUListElm* lru_elm = GetCacheElm(type, key);
	return lru_elm ? lru_elm->data : NULL;
}

void SVGCache::Remove(CacheType type, void* key)
{
	LRUListElm* lru_elm = GetCacheElm(type, key);
	if (lru_elm)
	{
		lru_elm->Out();
		OP_DELETE(lru_elm);
	}
}

SVGCache::LRUListElm::LRUListElm(CacheType type, void* key)
	: type(type), key(key), data(NULL)
{
}

SVGCache::LRUListElm::~LRUListElm()
{
	OP_DELETE(data);
}

void SVGCache::EvictIfFull()
{
	if (m_lrulist.Empty())
		return;

	unsigned int count = 0;
	unsigned int cache_size = 0;

	for (Link* l = m_lrulist.First(); l; l = l->Suc())
	{
		LRUListElm *lru_elm = static_cast<LRUListElm*>(l);

		count++;
		cache_size += EstimateMemoryUsage(lru_elm);
	}

#ifdef DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE
	unsigned int cache_limit_bytes = g_pccore->GetIntegerPref(PrefsCollectionCore::SVGRAMCacheSize) * 1024;
#endif // DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE

	if (count >= SVG_CACHED_BITMAPS_MAX ||
#ifdef DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE
		cache_size >= cache_limit_bytes
#else
		cache_size >= SVG_CACHED_BITMAPS_MAX_BYTES_SIZE
#endif // DEFAULT_SVG_CACHED_BITMAPS_MAX_KILOBYTES_SIZE
		)
	{
		// FIXME: Prioritize between surface cache and renderer cache

		LRUListElm *l = static_cast<LRUListElm*>(m_lrulist.Last());
		while(!l->data->IsEvictable() && l->Pred())
		{
			l = static_cast<LRUListElm*>(l->Pred());
		}

		if (l->data->IsEvictable())
		{
			l->Out();
			OP_DELETE(l);
		}
	}
}

SVGCache::LRUListElm* SVGCache::GetCacheElm(CacheType type, void* key, BOOL create /* = FALSE */)
{
	for (Link* l = m_lrulist.First(); l; l = l->Suc())
	{
		LRUListElm *lru_elm = static_cast<LRUListElm*>(l);

		if (type == lru_elm->type && key == lru_elm->key)
		{
			lru_elm->Out();
			lru_elm->IntoStart(&m_lrulist);
			return lru_elm;
		}
	}

	if (!create)
		return NULL;

	// Ok, no existing value
	// If we've filled the cache, chop down the oldest tree.
	EvictIfFull();
		
	LRUListElm* new_lru_elm = OP_NEW(LRUListElm, (type, key));
	if (!new_lru_elm)
		return NULL;

	new_lru_elm->IntoStart(&m_lrulist);
	return new_lru_elm;
}

unsigned int SVGCache::EstimateMemoryUsage(LRUListElm* lru_elm)
{
	unsigned int size = 0;

	if (lru_elm->data)
	{
		size += 1024; // Or something
		size += lru_elm->data->GetMemUsed();
	}
	return size;
}

void SVGCache::Clear()
{
#if 0
	// Eeeeek This is not very efficient since every remove call
	// will shift the whole vector one step.
	// Left as an example how to not do it.
	for(UINT32 i = 0; i < m_cache.GetCount(); i++)
	{
		SVGCacheElement* cacheElm = m_cache.Remove(i);
		OP_DELETE(cacheElm);
	}
#endif // 0
	m_lrulist.Clear();
}

#endif // SVG_SUPPORT
