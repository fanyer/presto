/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIASOURCEUTIL_H
#define MEDIASOURCEUTIL_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/mediabyterange.h"
#include "modules/media/src/mediasource.h"

/** Reduce the ranges from a list of clients.
 *
 * For pending, the first non-empty range is used.
 * For preload, the union of all ranges is used.
 */
void ReduceRanges(const List<MediaSourceClient>& clients,
				  MediaByteRange& pending, MediaByteRange& preload);

/** Combine pending and preload into a single request.
 *
 * If pending and preload overlap, request is their union, clamped
 * to pending.start. Otherwise, the first non-empty of pending and
 * preload is used.
 */
void CombineRanges(const MediaByteRange& pending, const MediaByteRange& preload,
				   MediaByteRange& request);

/** Intersect range with the next unavailable range.
 *
 * This is used to clamp a range to the first unavailable range after
 * the original start offset. The result can be an empty range.
 */
void IntersectWithUnavailable(MediaByteRange& range, URL& url);

/** Is the request successful?
 *
 * For HTTP, a request is considered successful for status
 * 200 OK, 206 Partial Content and 304 Not Modified.
 */
bool IsSuccessURL(const URL& url);

/** Will we be able to resume buffering if we stop?
 *
 * For HTTP, a request is considered resumable for status
 * 206 Partial Content.
 */
bool IsResumableURL(const URL& url);

/** Is the request completed?
 *
 * This does not imply that the entire resource is loaded in the case
 * of byte range request.
 */
static inline bool IsLoadedURL(const URL& url) { return url.Status(URL::KNoRedirect) == URL_LOADED; }

#endif // MEDIA_PLAYER_SUPPORT

#endif // MEDIASOURCEUTIL_H
