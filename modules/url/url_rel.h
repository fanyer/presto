/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __URL_REL_REP_H__
#define __URL_REL_REP_H__

class DataFile_Record;

#ifdef CACHE_FAST_INDEX
	class DiskCacheEntry;
#endif

#include "modules/util/b23tree.h"

class URL_RelRep : public B23Tree_Item
{
	friend class URL;
	friend class URL_Rep;
	friend class RelRep_Store;
	friend void Terminate();
	friend class UrlModule;

private:

    OpStringS8			name;
	OpStringS			uni_name;
    unsigned int		reference_count;
	time_t				last_visited;
	BOOL				visited_checked;	// set to TRUE when visited has been checked once to avoid checking multiple times, cleared when visited is set.

private:
	void				ConstructL(const OpStringC8 &rel_name);
#ifdef DISK_CACHE_SUPPORT
	void				ConstructL(DataFile_Record *rec);
#endif
	static void			CreateL(URL_RelRep **url_relrep, const OpStringC8 &rel_name);
#ifdef DISK_CACHE_SUPPORT
	static void			CreateL(URL_RelRep **url_relrep, DataFile_Record *rec);
#endif
	static OP_STATUS	Create(URL_RelRep **url_relrep, const OpStringC8 &rel_name){OP_STATUS op_err = OpStatus::OK; TRAP(op_err, CreateL(url_relrep, rel_name)); return op_err;};
public:
						URL_RelRep();
	virtual 			~URL_RelRep();
private:

#ifndef RAMCACHE_ONLY
	void				WriteCacheDataL(DataFile_Record *rec);
	#ifdef CACHE_FAST_INDEX
	void				WriteCacheDataL(DiskCacheEntry *entry);
	#endif
#endif
public:
	void 	IncRef(){reference_count ++;};
	unsigned int		DecRef(){return (reference_count ? --reference_count : 0);};
	unsigned int		GetRefCount() { return reference_count; };

	const OpStringC8	&Name() const { return name; }
	/** Retrieve Unicode version. Returns NULL on OOM. */
	const OpStringC		&UniName();
	BOOL				IsVisited();
	void				SetFollowed(BOOL followed);
	void				SetLastVisited(time_t timestamp);
#ifdef _OPERA_DEBUG_DOC_
	unsigned long		GetMemUsed();
#endif
	virtual int SearchCompare(void *search_param);
	virtual BOOL TraverseActionL(uint32 action, void *params);

	BOOL				GetHasCheckedVisited() { return visited_checked; };
	void				SetHasCheckedVisited(BOOL checked) { visited_checked = checked; };
};

#endif
