/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef __URL_B23TREE_REL_REP_H__
#define __URL_B23TREE_REL_REP_H__

#include "modules/util/b23tree.h"

class URL_RelRep;
#ifdef CACHE_FAST_INDEX
	class DiskCacheEntry;
#endif

class RelRep_Store : public B23Tree_Store
{
public:
	RelRep_Store();

#ifndef RAMCACHE_ONLY
	void WriteCacheDataL(DataFile_Record *rec);
	#ifdef CACHE_FAST_INDEX
		void WriteCacheDataL(DiskCacheEntry *rec);
	#endif
#endif

	void DeleteRep(const char *rel);
	URL_RelRep *FindOrAddRep(const char *rel);
	void FindOrAddRep(URL_RelRep *rel);
};

#endif
