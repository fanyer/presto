/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/mediasourceutil.h"

void ReduceRanges(const List<MediaSourceClient>& clients,
				  MediaByteRange& pending, MediaByteRange& preload)
{
	OP_ASSERT(preload.IsEmpty());
	MediaSourceClient* client = clients.First();
	if (client)
	{
		for ( ; client; client = client->Suc())
		{
			// take the first non-empty pending
			if (pending.IsEmpty())
				pending = client->GetPending();
			// preload the union of all ranges
			preload.UnionWith(client->GetPreload());
		}
	}
	else
	{
		// with no clients, preload the whole resource
		preload.start = 0;
	}
}

void CombineRanges(const MediaByteRange& pending, const MediaByteRange& preload,
				   MediaByteRange& request)
{
	OP_ASSERT(request.IsEmpty());
	// If pending and preload overlap, let request be their union,
	// clamped to pending.start.
	if (!pending.IsEmpty() && !preload.IsEmpty())
	{
		request = pending;
		request.IntersectWith(preload);
		if (!request.IsEmpty())
		{
			// non-empty intersection => overlap
			request = pending;
			request.UnionWith(preload);
			request.IntersectWith(MediaByteRange(pending.start));
			OP_ASSERT(!request.IsEmpty());
		}
	}
	// Otherwise, pick the first non-empty of pending and preload.
	if (request.IsEmpty())
	{
		if (!pending.IsEmpty())
			request = pending;
		else
			request = preload;
	}
}

void IntersectWithUnavailable(MediaByteRange& range, URL& url)
{
	if (range.IsEmpty())
		return;

	// find the first unavailable range on or after range.start
	MediaByteRange unavail;
	BOOL available = FALSE;
	OpFileLength length = 0;
	url.GetPartialCoverage(range.start, available, length, TRUE);
	if (available)
	{
		OP_ASSERT(length > 0);
		unavail.start = range.start + length;
		length = 0;
		url.GetPartialCoverage(unavail.start, available, length, TRUE);
		OP_ASSERT(!available);
	}
	else
		unavail.start = range.start;

	// length is now the number of unavailable bytes, or 0 if unknown
	if (length > 0)
		unavail.SetLength(length);

	range.IntersectWith(unavail);
}

bool IsSuccessURL(const URL& url)
{
	if (url.IsEmpty())
		return false;

	switch (url.Type())
	{
	case URL_HTTP:
	case URL_HTTPS:
		switch (url.GetAttribute(URL::KHTTP_Response_Code))
		{
		case HTTP_OK:
		case HTTP_PARTIAL_CONTENT:
		case HTTP_NOT_MODIFIED:
			return true;
		default:
			return false;
		}
	case URL_FTP:
	case URL_FILE:
	case URL_DATA:
	case URL_WIDGET:
		return true;
	default:
		return false;
	}
}

bool IsResumableURL(const URL& url)
{
	OP_ASSERT(url.GetAttribute(URL::KMultimedia) != FALSE);
	// Not using URL::KResumeSupported as it is set to Probably_Resumable
	// when the Accept-Ranges: bytes header is present. Since we always
	// make byte range requests, the response code is a better indicator.
	int response = url.GetAttribute(URL::KHTTP_Response_Code, URL::KFollowRedirect);
	return response == HTTP_PARTIAL_CONTENT;
}

#endif // MEDIA_PLAYER_SUPPORT
