/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef CORS_SUPPORT
#include "modules/security_manager/src/cors/cors_preflight.h"
#include "modules/security_manager/src/cors/cors_request.h"
#include "modules/security_manager/src/cors/cors_utilities.h"
#include "modules/pi/OpSystemInfo.h"

/* static */ OP_STATUS
OpCrossOrigin_PreflightCache::Make(OpCrossOrigin_PreflightCache *&cache, unsigned default_max_age, unsigned max_max_age,unsigned max_size)
{
	RETURN_OOM_IF_NULL(cache = OP_NEW(OpCrossOrigin_PreflightCache, (default_max_age, max_max_age, max_size)));

	return OpStatus::OK;
}

OpCrossOrigin_PreflightCache::~OpCrossOrigin_PreflightCache()
{
	cache_entries.Clear();
}

/* static */ OP_STATUS
OpCrossOrigin_PreflightCache::CacheEntry::Make(CacheEntry *&result, unsigned max_age, const uni_char *origin, const URL &url, BOOL credentials, BOOL is_method, const uni_char *value)
{
	double expiry = g_op_time_info->GetTimeUTC() + max_age * 1000;
	OpCrossOrigin_PreflightCache::CacheEntry *entry = OP_NEW(OpCrossOrigin_PreflightCache::CacheEntry, (expiry, url, max_age, credentials, is_method));
	OpAutoPtr<OpCrossOrigin_PreflightCache::CacheEntry> anchor_entry(entry);

	RETURN_OOM_IF_NULL(entry);
	RETURN_IF_ERROR(entry->origin.Set(origin));
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment, entry->url_string));
	RETURN_IF_ERROR(entry->value.Set(value));

	result = anchor_entry.release();
	return OpStatus::OK;
}

void
OpCrossOrigin_PreflightCache::CacheEntry::UpdateMaxAge(unsigned new_max_age)
{
	expiry_time = g_op_time_info->GetTimeUTC() + new_max_age * 1000;
	max_age = new_max_age;
}

BOOL
OpCrossOrigin_PreflightCache::AllowUpdates(const OpCrossOrigin_Request &request)
{
	/* All preflight responses may currently be used and added,
	   but evict expired entries first. */
	InvalidateCache(FALSE);
	return TRUE;
}

OP_STATUS
OpCrossOrigin_PreflightCache::UpdateMethodCache(OpVector<OpString> &methods, const OpCrossOrigin_Request &request, unsigned max_age)
{
	unsigned count = methods.GetCount();
	for (unsigned i = 0; i < count; i++)
	{
		const uni_char *method = methods.Get(i)->CStr();
		if (CacheEntry *entry = FindMethodMatch(request, method))
			entry->UpdateMaxAge(max_age);
		else
		{
			CacheEntry *new_entry;
			RETURN_IF_ERROR(CacheEntry::Make(new_entry, max_age, request.GetOrigin(), request.GetURL(), request.WithCredentials(), TRUE, method));
			new_entry->Into(&cache_entries);
			cache_entries_count++;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpCrossOrigin_PreflightCache::UpdateHeaderCache(OpVector<OpString> &headers, const OpCrossOrigin_Request &request, unsigned max_age)
{
	unsigned count = headers.GetCount();
	for (unsigned i = 0; i < count; i++)
	{
		const uni_char *header = headers.Get(i)->CStr();
		if (CacheEntry *entry = FindHeaderMatch(request, header))
			entry->UpdateMaxAge(max_age);
		else
		{
			CacheEntry *new_entry;
			RETURN_IF_ERROR(CacheEntry::Make(new_entry, max_age, request.GetOrigin(), request.GetURL(), request.WithCredentials(), FALSE, header));
			new_entry->Into(&cache_entries);
			cache_entries_count++;
		}
	}

	return OpStatus::OK;
}

BOOL
OpCrossOrigin_PreflightCache::MethodMatch(const OpCrossOrigin_Request &request)
{
	return FindMethodMatch(request, request.GetMethod()) != NULL;
}

BOOL
OpCrossOrigin_PreflightCache::HeaderMatch(const OpCrossOrigin_Request &request, const uni_char *header)
{
	return FindHeaderMatch(request, header) != NULL;
}

OpCrossOrigin_PreflightCache::CacheEntry *
OpCrossOrigin_PreflightCache::FindMethodMatch(const OpCrossOrigin_Request &request, const uni_char *method)
{
	double now = g_op_time_info->GetTimeUTC();
	OpString url_string;
	const URL &request_url = request.GetURL();
	for (CacheEntry *entry = cache_entries.First(); entry; entry = entry->Suc())
	{
		BOOL same_origin = entry->origin.Compare(request.GetOrigin()) == 0;
		BOOL same_cred = entry->credentials == request.WithCredentials();
		BOOL same_method = entry->is_method && entry->value.Compare(method) == 0;
		BOOL same_url = entry->url == request.GetURL();
		if (entry->expiry_time > now && entry->is_method && same_origin && same_cred && same_method)
		{
			if (same_url)
				return entry;

			if (url_string.IsEmpty())
				if (OpStatus::IsError(request_url.GetAttribute(URL::KUniName_With_Fragment, url_string)))
					return NULL;
			if (uni_str_eq(entry->url_string, url_string.CStr()))
				return entry;
		}
	}

	return NULL;
}

OpCrossOrigin_PreflightCache::CacheEntry *
OpCrossOrigin_PreflightCache::FindHeaderMatch(const OpCrossOrigin_Request &request, const uni_char *header)
{
	double now = g_op_time_info->GetTimeUTC();
	OpString url_string;
	const URL &request_url = request.GetURL();
	for (CacheEntry *entry = cache_entries.First(); entry; entry = entry->Suc())
	{
		BOOL same_origin = entry->origin.Compare(request.GetOrigin()) == 0;
		BOOL same_cred = entry->credentials == request.WithCredentials();
		BOOL same_header = !entry->is_method && OpCrossOrigin_Utilities::IsEqualASCIICaseInsensitive(entry->value.CStr(), header, UINT_MAX, TRUE);
		BOOL same_url = entry->url == request_url;
		if (entry->expiry_time > now && !entry->is_method && same_origin && same_cred && same_header)
		{
			if (same_url)
				return entry;

			if (url_string.IsEmpty())
				if (OpStatus::IsError(request_url.GetAttribute(URL::KUniName_With_Fragment, url_string)))
					return NULL;
			if (entry->url_string.Compare(url_string) == 0)
				return entry;
		}
	}

	return NULL;
}

void
OpCrossOrigin_PreflightCache::RemoveOriginRequests(const uni_char *origin, const URL &url)
{
	CacheEntry *entry = cache_entries.First();
	while (entry)
	{
		CacheEntry *next = entry->Suc();
		if (entry->origin.Compare(origin) == 0 && entry->url == url)
		{
			entry->Out();
			cache_entries_count--;
			OP_DELETE(entry);
		}
		entry = next;
	}
}

void
OpCrossOrigin_PreflightCache::InvalidateCache(BOOL expire_all)
{
	double now = g_op_time_info->GetTimeUTC();
	CacheEntry *entry = cache_entries.First();
	while (entry)
	{
		CacheEntry *next = entry->Suc();
		if (expire_all || entry->expiry_time < now)
		{
			entry->Out();
			cache_entries_count--;
			OP_DELETE(entry);
		}
		entry = next;
	}
}

#endif // CORS_SUPPORT
