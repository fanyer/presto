/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_man.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_rel.h"

#ifdef CACHE_FAST_INDEX
#include "modules/cache/simple_stream.h"
#endif

#include "modules/formats/url_dfr.h"

// URL_RelRep constructors

URL_RelRep::URL_RelRep()
{
	last_visited = 0;
	reference_count = 0;
	visited_checked = FALSE;
}

void URL_RelRep::ConstructL(const OpStringC8 &rel_name)
{
	name.SetL(rel_name);
}

#ifdef DISK_CACHE_SUPPORT
void URL_RelRep::ConstructL(DataFile_Record *rec)
{
	rec->GetIndexedRecordStringL(TAG_RELATIVE_NAME, name);
	if(name.CStr() == NULL)
		name.SetL("");
	last_visited = rec->GetIndexedRecordUIntL(TAG_RELATIVE_VISITED);

}
#endif // DISK_CACHE_SUPPORT

void URL_RelRep::CreateL(URL_RelRep **url_relrep, const OpStringC8 &rel_name)
{
	*url_relrep = NULL;

	OpStackAutoPtr<URL_RelRep> temp_rel(OP_NEW_L(URL_RelRep, ()));

	temp_rel->ConstructL(rel_name);

	*url_relrep  = temp_rel.release();
}

#ifdef DISK_CACHE_SUPPORT
void URL_RelRep::CreateL(URL_RelRep **url_relrep, DataFile_Record *rec)
{
	*url_relrep = NULL;

	OpStackAutoPtr<URL_RelRep> temp_rel(OP_NEW_L(URL_RelRep, ()));

	temp_rel->ConstructL(rec);

	*url_relrep  = temp_rel.release();

}
#endif // RAMCACHE_ONLY

URL_RelRep::~URL_RelRep()
{
	if (uni_last_rel_rep == this)
	{
		uni_last_user = NULL;
		uni_last_rel_rep = NULL;
	}
	if (last_rel_rep == this)
	{
		last_user = NULL;
		last_rel_rep = NULL;
	}
}

#ifndef RAMCACHE_ONLY
void URL_RelRep::WriteCacheDataL(DataFile_Record *rec)
{
	DataFile_Record relrec(TAG_RELATIVE_ENTRY);
	ANCHOR(DataFile_Record,relrec);

	relrec.SetRecordSpec(rec->GetRecordSpec());

	relrec.AddRecordL(TAG_RELATIVE_NAME, name);
	relrec.AddRecordL(TAG_RELATIVE_VISITED, (uint32) last_visited);

	relrec.WriteRecordL(rec);
}

#ifdef CACHE_FAST_INDEX
void URL_RelRep::WriteCacheDataL(DiskCacheEntry *entry)
{
	entry->AddRelativeLink(&name, last_visited);
}
#endif
#endif // RAMCACHE_ONLY

const OpStringC &URL_RelRep::UniName()
{
	if (!uni_name.CStr())
	{
		// FIXME: OOM: Returns NULL on OOM, should we do anything else?
		uni_name.SetFromUTF8(name);
	}
	return uni_name;
}

BOOL URL_RelRep::IsVisited()
{
	if(last_visited)
	{
		time_t time_now = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
		if (time_now - last_visited > g_pcnet->GetFollowedLinksExpireTime())
			last_visited = 0;
	}
	return (last_visited ? TRUE : FALSE) ;
}

void URL_RelRep::SetLastVisited(time_t timestamp)
{
#ifdef NEED_URL_VISIBLE_LINKS
	if (urlManager && urlManager->GetVisLinks())
#endif // NEED_URL_VISIBLE_LINKS
		last_visited = timestamp;
}

void URL_RelRep::SetFollowed(BOOL followed)
{
	SetLastVisited(followed ? (time_t) (g_op_time_info->GetTimeUTC()/1000.0) : 0);
}

URL_RelRep *URL_Rep::GetRelativeId(const OpStringC8 &rel)
{
	URL_RelRep *rep = NULL;

	if(rel.CStr())
	{
		rep = relative_names.FindOrAddRep(rel.CStr());
	}
	if(!rep)
		rep = EmptyURL_RelRep;

	if (rep)
		rep->IncRef();

	return rep;
}

void URL_Rep::RemoveRelativeId(const OpStringC8 &rel)
{
	if(rel.CStr())
	{
		relative_names.DeleteRep(rel.CStr());
	}
}
