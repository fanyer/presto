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
#include "modules/url/url_rep.h"
#include "modules/cache/url_stor.h"


URL_Store::URL_Store(unsigned int size)  : LinkObjectStore(size)
#ifdef SEARCH_ENGINE_CACHE
	, m_disk_index(this)
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	, m_operator_index(this)
#endif
#endif
{
}

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
CacheIndex *g_disk_index = NULL;
#endif

OP_STATUS URL_Store::Construct(
#ifdef DISK_CACHE_SUPPORT
		OpFileFolder folder
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		// OPFILE_ABSOLUTE_FOLDER means do not use operator cache
		, OpFileFolder opfolder
#endif
#endif
							   )
{
#ifdef SEARCH_ENGINE_CACHE
	RETURN_IF_ERROR(m_disk_index.Open(NULL, folder));
	OP_ASSERT(m_disk_index.CheckConsistency(TRUE) == OpStatus::OK);

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if (opfolder != OPFILE_ABSOLUTE_FOLDER)
	{
		RETURN_IF_ERROR(m_operator_index.Open(NULL, opfolder));
		OP_ASSERT(m_operator_index.CheckConsistency(TRUE) == OpStatus::OK);
	}
#endif  // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_OPERATOR_CACHE_MANAGEMENT

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
	if (g_disk_index == NULL)
		g_disk_index = &m_disk_index;
#endif
#endif
	return LinkObjectStore::Construct(); // May return OpStatus::ERR_NO_MEMORY
}

OP_STATUS URL_Store::Create(URL_Store **url_store,
							unsigned int size
#ifdef DISK_CACHE_SUPPORT
		, OpFileFolder folder
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		// OPFILE_ABSOLUTE_FOLDER means do not use operator cache
		, OpFileFolder opfolder
#endif
#endif
							)
{
	OP_ASSERT(url_store);

	*url_store = OP_NEW(URL_Store, (size));
	if (*url_store == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS op_err = (*url_store)->Construct(
#ifdef DISK_CACHE_SUPPORT
		folder
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		, opfolder
#endif
#endif
		);

	if (OpStatus::IsError(op_err))
	{
		OP_DELETE(*url_store);
		*url_store = NULL;
	}
	return op_err;
}

URL_Store::~URL_Store()
{
	URL_Rep* url_rep = (URL_Rep*) GetFirstLinkObject();
	while(url_rep)
	{
		RemoveLinkObject(url_rep);
		if((url_rep->DecRef()) == 0)
			OP_DELETE(url_rep);
#ifdef _DEBUG
		else
		{
			int stop = 1;
			--stop;
		}
#endif
		url_rep = (URL_Rep *) GetNextLinkObject();
	}
}

URL_Rep* URL_Store::GetURL_Rep(OP_STATUS &op_err, URL_Name_Components_st &url, BOOL create
					, URL_CONTEXT_ID context_id
					)
{
	unsigned int full_index = 0;
	URL_Rep* url_rep = (URL_Rep*) GetLinkObject(url.full_url, &full_index);

#ifdef SEARCH_ENGINE_CACHE
	if (!url_rep)
	{
		url_rep = m_disk_index.Search(url.full_url);

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		if (url_rep == NULL && m_operator_index.IsOpen())
			url_rep = m_operator_index.Search(url.full_url);
#endif

		if (url_rep != NULL)
			AddLinkObject(url_rep, &full_index);
	}
#endif

	if (!url_rep && create)
	{
		op_err = URL_Rep::Create(&url_rep, url
					, context_id
					);

		if(OpStatus::IsError(op_err))
			return NULL;

		AddLinkObject(url_rep, &full_index);
	}

	op_err = OpStatus::OK;
	return url_rep;
}

#ifdef SEARCH_ENGINE_CACHE
OP_STATUS URL_Store::AppendFileName(OpString &name, OpFileLength max_cache_size, BOOL session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		, BOOL operator_cache
#endif
		)
{
	INT32 n;

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if (operator_cache)
		n = m_operator_index.GetFileNumber(FALSE);
	else
#endif
		n = m_disk_index.GetFileNumber(session_only);

	OP_ASSERT(max_cache_size > 0);

	if (session_only)
		return name.AppendFormat(UNI_L("%s%c%.08X"), CACHE_SESSION_FOLDER, PATHSEPCHAR, n);
	return name.AppendFormat(UNI_L("%.04X%c%.08X"), CacheIndex::GetFileFolder(n, max_cache_size), PATHSEPCHAR, n);
}
#endif

URL_Rep*
URL_Store::GetFirstURL_Rep(LinkObjectBookmark &bm)
{
	URL_Rep* link=GetFirstURL_Rep();

	bm.SetIndex(last_idx, FALSE);

	return link;
}

URL_Rep*
URL_Store::GetNextURL_Rep(LinkObjectBookmark &bookmark)
{
	OP_ASSERT(bookmark.GetIndex() == last_idx);

	bookmark.SetIndex(last_idx, FALSE);
	
	HashedLink* link=GetNextLinkObject();

	if(last_idx != bookmark.GetIndex()) // Bucket changed: skip to the first object
		bookmark.SetIndex(last_idx, TRUE);
	else
		bookmark.IncPosition();	// Same Bucket: next object

	return (URL_Rep*) link;
}

URL_Rep* 
URL_Store::JumpToURL_RepBookmark(LinkObjectBookmark &bookmark)
{
	last_idx = bookmark.GetIndex();

	if(last_idx < store_size)
	{
		unsigned int position = bookmark.GetPosition();

		last_link_obj = (HashedLink *) store_array[last_idx].First(); // Quick jump to the right bucket

		// Find the intended position
		while(position && last_idx == bookmark.GetIndex())
		{
			GetNextLinkObject();
			position --;
		}

		if(last_idx != bookmark.GetIndex()) // Position not existent, go to the beginning of the next bucket
		{
			OP_ASSERT(last_idx > bookmark.GetIndex());

			bookmark.SetIndex(last_idx, TRUE);
		}

		return (URL_Rep*)last_link_obj;

	}

	return (URL_Rep*)NULL;
}

