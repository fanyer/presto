/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

class DataFile_Record;

#include "modules/hardcore/mem/mem_man.h"

#include "modules/url/url_brel.h"
#include "modules/url/url_rel.h"

BOOL URL_RelRep::TraverseActionL(uint32 action, void *params)
{
#ifndef RAMCACHE_ONLY
	if(!last_visited)
		return TRUE;

	if(action == 0)
		WriteCacheDataL((DataFile_Record *) params);
	#ifdef CACHE_FAST_INDEX
	else if(action == 100)
		WriteCacheDataL((DiskCacheEntry *) params);
	#endif

#endif
	return TRUE;
}

int URL_RelRep::SearchCompare(void *search_param)
{
	const char *rel = (const char *) search_param;

	if(name.HasContent())
	{
		return rel ? name.Compare(rel) : +1;
	}
	else
	{
		return (rel && *rel) ? -1 : 0;
	}
}

/* return positive if first is greater than second */
int RelRep_Compare(const B23Tree_Item *first, const B23Tree_Item *second)
{
	URL_RelRep *item1 = (URL_RelRep *)first;
	URL_RelRep *item2 = (URL_RelRep *)second;

	if(item1 && item2)
	{
		return item1->Name().Compare(item2->Name());
	}
	else
	{
		return (item1 ? +1 : (item2 ? -1 : 0));
	}
}

RelRep_Store::RelRep_Store()
: B23Tree_Store(RelRep_Compare)
{
}

#ifndef RAMCACHE_ONLY
void RelRep_Store::WriteCacheDataL(DataFile_Record *rec)
{
	TraverseL(0, rec);
}

#ifdef CACHE_FAST_INDEX
void RelRep_Store::WriteCacheDataL(DiskCacheEntry *entry)
{
	TraverseL(100, entry);
}
#endif
#endif

void RelRep_Store::DeleteRep(const char *rel)
{
	URL_RelRep *rep;

	if(rel == NULL)
		return;

	rep = (URL_RelRep *) Search((void *) rel);
	if(rep != NULL)
		Delete(rep);
}

URL_RelRep *RelRep_Store::FindOrAddRep(const char *rel)
{
	URL_RelRep *rep;

	if(rel == NULL)
		return NULL;

	rep = (URL_RelRep *) Search((void *)rel);
	if(rep == NULL)
	{
		OP_STATUS op_err = URL_RelRep::Create(&rep, rel);
		if(OpStatus::IsError(op_err))
		{
			g_memory_manager->RaiseCondition(op_err);
			OP_DELETE(rep);
			return NULL;
		}

		TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, InsertL(rep), NULL);
	}

	return rep;
}

void RelRep_Store::FindOrAddRep(URL_RelRep *rel)
{
	if(rel == NULL)
		return;

	OP_STATUS op_err;
	TRAP_AND_RETURN_VOID_IF_ERROR(op_err, InsertL(rel));
}
