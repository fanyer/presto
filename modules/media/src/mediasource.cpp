/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/cache/cache_ctxman_multimedia.h"
#include "modules/cache/url_dd.h"
#include "modules/media/src/coremediaplayer.h"
#include "modules/media/src/mediasource.h"
#include "modules/media/src/mediasourceutil.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/url/tools/content_detector.h"

// the maximum number of bytes to wait for when needing a seek
#define MEDIA_SOURCE_MAX_WAIT (64*1024)

// the minimum number of bytes to fetch (to reduce roundtrips)
// 8500 happens to be the maximum size requested by GstMediaPlayer.
// should perhaps be a tweak?
// MUST be smaller than MEDIA_SOURCE_MAX_WAIT
#define MEDIA_SOURCE_MIN_SIZE 8500

// the limits for pausing and resuming the transfer when using the
// streaming cache, expressed as remaining bytes before overwrite.
#define STREAMING_CACHE_PAUSE_LIMIT (64*1024)
#define STREAMING_CACHE_RESUME_LIMIT (128*1024)

OP_STATUS
MediaSourceManagerImpl::GetSource(MediaSource*& source, const URL& url, const URL& referer_url, MessageHandler* message_handler)
{
	OP_ASSERT(!url.IsEmpty());
	URL use_url = url; // GetUrlWithMediaContext(url);

	// Try getting existing instance.
	MediaSourceImpl* sourceimpl;
	if (OpStatus::IsSuccess(m_sources.GetData(reinterpret_cast<const void*>(use_url.Id()),
											  reinterpret_cast<void**>(&sourceimpl))))
	{
		if (!sourceimpl->IsStreaming())
		{
			// All is well, use this source.
			sourceimpl->m_refcount++;
			source = sourceimpl;
			return OpStatus::OK;
		}
		else
		{
			// A streaming source cannot be shared, make a unique copy
			// of the URL and create a new instance instead.
			use_url = g_url_api->MakeUniqueCopy(use_url);
			OP_ASSERT(use_url.GetAttribute(URL::KUnique));
			if (use_url.IsEmpty())
				return OpStatus::ERR;
		}
	}

	// Create a new instance.

	// Decide what message handler to use. If no message handler is given as input, use
	// the global one. The global one will not work from widgets.
	if (!message_handler)
		message_handler = g_main_message_handler;

	// Create new instance.
	OpAutoPtr<MediaSourceImpl> source_safe(OP_NEW(MediaSourceImpl, (use_url, referer_url, message_handler)));
	if (!source_safe.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(source_safe->Init());

	// Add it to the set of MediaSource instances.
	RETURN_IF_ERROR(m_sources.Add(reinterpret_cast<const void*>(use_url.Id()),
								  reinterpret_cast<void*>(source_safe.get())));

	source_safe->m_refcount++;
	source = source_safe.release();
	return OpStatus::OK;
}

OP_STATUS
MediaSourceManagerImpl::GetSource(MediaSource*& source, OpMediaHandle handle)
{
	// Theoretically OpMediaHandle could also be a different type of
	// MediaPlayer but it is only CoreMediaPlayer instances that use
	// the OpMediaSourceManager. Having a different type of player
	// here would indicate a serious bug.
	CoreMediaPlayer* playerimpl = static_cast<CoreMediaPlayer*>(handle);
	// Get the source from the player and increase refcount.
	source = playerimpl->GetSource();
	if (!source)
	{
		OP_ASSERT(!"Don't use OpMediaSource::Create when OpMediaManager::CanPlayURL returned TRUE");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	static_cast<MediaSourceImpl*>(source)->m_refcount++;
	return OpStatus::OK;
}

void
MediaSourceManagerImpl::PutSource(MediaSource* source)
{
	// We know that this is a MediaSourceImpl instance because we created it.
	MediaSourceImpl* sourceimpl = static_cast<MediaSourceImpl*>(source);
	if (--sourceimpl->m_refcount == 0)
	{
		MediaSourceImpl* removed;
		OP_STATUS status = m_sources.Remove(reinterpret_cast<const void*>(sourceimpl->m_key_url.Id()),
											reinterpret_cast<void**>(&removed));
		OP_ASSERT(OpStatus::IsSuccess(status));
		OpStatus::Ignore(status);
		OP_ASSERT(removed == sourceimpl);
		OP_DELETE(removed);
	}
#ifdef DEBUG_ENABLE_OPASSERT
	else
	{
		// Verify that we actually created this source.
		MediaSourceImpl* existing;
		OP_ASSERT(OpStatus::IsSuccess(m_sources.GetData(reinterpret_cast<const void*>(sourceimpl->m_key_url.Id()),
														reinterpret_cast<void**>(&existing)))
				  && sourceimpl == existing);
	}
#endif // DEBUG_ENABLE_OPASSERT
}

#if 0
URL
MediaSourceManagerImpl::GetUrlWithMediaContext(const URL& url)
{
	// Don't override anything but the default context for now.
	if (url.GetContextId() != 0)
		return url;

	// Create new context manager if it doesn't exists already.
	if (!m_url_context_id)
	{
		m_url_context_id = urlManager->GetNewContextID();
		OpFileFolder media_cache_folder;
		if (OpStatus::IsError(g_folder_manager->AddFolder(OPFILE_CACHE_FOLDER, UNI_L("media"), &media_cache_folder)))
		{
			OP_ASSERT(!"Failed to create folder for dedicated media cache. Falling back to default");
			return url;
		}

		Context_Manager_Multimedia::CreateManager(m_url_context_id, media_cache_folder, media_cache_folder, FALSE, PrefsCollectionNetwork::MediaCacheSize);
	}

	// Create new url with the media context.
	const OpStringC tmp_url_str
					= url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI,
						URL::KNoRedirect);
	return g_url_api->GetURL(tmp_url_str, m_url_context_id);
}
#endif

static const OpMessage source_msgs[] = {
	MSG_MEDIA_SOURCE_ENSURE_BUFFERING,
	MSG_MEDIA_SOURCE_NOTIFY_LISTENER
};

static const OpMessage url_msgs[] = {
	MSG_HEADER_LOADED,
	MSG_URL_DATA_LOADED,
	MSG_URL_LOADING_FAILED,
	MSG_URL_MOVED
};

MediaSourceImpl::MediaSourceImpl(const URL& url, const URL& referer_url, MessageHandler* message_handler)
	: m_refcount(0),
	  m_key_url(url),
	  m_referer_url(referer_url),
	  m_url_dd(NULL),
	  m_message_handler(message_handler),
	  m_state(NONE),
	  m_clamp_request(FALSE),
	  m_pending_ensure(FALSE),
	  m_verified_content_type(FALSE),
	  m_read_offset(0)
{
}

MediaSourceImpl::~MediaSourceImpl()
{
	OP_ASSERT(m_refcount == 0);
	OP_DELETE(m_url_dd);
	StopBuffering();
	if (m_message_handler.get())
		m_message_handler->UnsetCallBacks(this);
}

OP_STATUS
MediaSourceImpl::Init()
{
	m_use_url.SetURL(m_key_url);
	// Purge the resource from cache if it is expired or we are using
	// the streaming cache. Cache invalidation during the lifetime of
	// the MediaSourceImpl instance is not supported, see CORE-27748.
	if (m_use_url->Expired(TRUE) || IsStreaming())
		m_use_url->Unload();
	m_use_url->SetAttribute(URL::KMultimedia, TRUE);
	m_use_url->SetAttribute(URL::KSendAcceptEncoding, FALSE);
	return SetCallBacks();
}

OP_STATUS
MediaSourceImpl::SetCallBacks()
{
	RETURN_IF_ERROR(m_message_handler->SetCallBackList(this, reinterpret_cast<MH_PARAM_1>(this), source_msgs, ARRAY_SIZE(source_msgs)));
	return m_message_handler->SetCallBackList(this, static_cast<MH_PARAM_1>(m_use_url->Id()), url_msgs, ARRAY_SIZE(url_msgs));
}

void
MediaSourceImpl::HandleClientChange()
{
	// Invoke EnsureBuffering asynchronously.
	if (!m_pending_ensure)
	{
		m_message_handler->PostMessage(MSG_MEDIA_SOURCE_ENSURE_BUFFERING,
									   reinterpret_cast<MH_PARAM_1>(this), 0);
		m_pending_ensure = TRUE;
	}
}

#ifdef _DEBUG
static Debug&
operator<<(Debug& d, const MediaByteRange& range)
{
	d << "[";
	if (range.IsEmpty())
	{
		d << "empty";
	}
	else
	{
		d << (long)range.start;
		d << ", ";
		if (range.IsFinite())
			d << (long)range.end;
		else
			d << "Inf";
	}
	d << "]";
	return d;
}
#endif

void
MediaSourceImpl::CalcRequest(MediaByteRange& request)
{
	if (IsStreaming())
	{
		// When streaming we only consider the pending range (if any).
		// When there are no clients, request the entire resource.
		// To support preload together with streaming, care must be
		// taken to not restart a preload request [0,Inf] as soon as
		// data has been evicted and the cached range is e.g.
		// [500,1000499] if there is no pending request in that range.

		if (!m_clients.Empty())
		{
			MediaByteRange preload; // unused
			ReduceRanges(m_clients, request, preload);
			if (!request.IsEmpty())
			{
				request.SetLength(FILE_LENGTH_NONE);
				IntersectWithUnavailable(request, m_use_url);
				if (!request.IsEmpty())
					request.SetLength(FILE_LENGTH_NONE);
			}
		}
		else
		{
			request.start = 0;
		}
		// At this point we should have an unbounded range, but it may
		// be clamped to the resource length below.
		OP_ASSERT(!request.IsFinite());
	}
	else
	{
		// When not streaming (using multiple range disk cache), both
		// pending and preload requests are taken into account.
		//
		// Example 1: Everything should be preloaded, but since there is a
		// pending read, start buffering from that offset first. Also,
		// don't refetch the end of the resource.
		//
		// resource: <---------------------->
		// buffered: <-->               <--->
		// pending:           <---->
		// preload:  <---------------------->
		// request:           <-------->
		//
		// Example 2: The requested preload is already buffered, so the
		// request is the empty range.
		//
		// resource: <---------------------->
		// buffered: <-------->
		// pending:  empty range
		// preload:  <----->
		// request:  empty range

		MediaByteRange pending, preload;
		ReduceRanges(m_clients, pending, preload);
		CombineRanges(pending, preload, request);
		IntersectWithUnavailable(request, m_use_url);
	}

	if (!request.IsEmpty())
	{
		// Extra restrictions if resource length is known (this won't
		// be needed after CORE-30311 is fixed).
		OpFileLength resource_length = GetTotalBytes();
		if (resource_length > 0)
		{
			OP_ASSERT(request.start <= resource_length);

			MediaByteRange resource(0, resource_length - 1);

			// Clamp request to resource.
			request.IntersectWith(resource);

			// Increase size if it is unreasonably small at the end...
			if (!request.IsEmpty() &&
				request.Length() < MEDIA_SOURCE_MIN_SIZE &&
				resource.Length() >= MEDIA_SOURCE_MIN_SIZE &&
				request.end == resource.end)
			{
				// ... but only if nothing in that range is available.
				MediaByteRange cand_request(resource_length - MEDIA_SOURCE_MIN_SIZE, resource_length - 1);
				OP_ASSERT(cand_request.Length() == MEDIA_SOURCE_MIN_SIZE);
				IntersectWithUnavailable(cand_request, m_use_url);
				if (cand_request.Length() == MEDIA_SOURCE_MIN_SIZE)
					request = cand_request;
			}
		}
	}

	OP_NEW_DBG("CalcRequest", "MediaSource");
	OP_DBG(("request: ") << request);
}

void
MediaSourceImpl::EnsureBuffering()
{
	switch (EnsureBufferingInternal())
	{
	case NONE:
		break;
	case STARTED:
		m_state = STARTED;
		break;
	case IDLE:
		if (VerifyContentType())
		{
			m_state = IDLE;
			for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
				m_listeners.GetNext(iter)->OnIdle(this);
		}
		else
		{
			OP_ASSERT(m_state == FAILED);
		}
		break;
	case FAILED:
		LoadFailed();
		break;
	default:
		OP_ASSERT(!"Unexpected state change");
	}
}

MediaSourceImpl::State
MediaSourceImpl::EnsureBufferingInternal()
{
	MediaByteRange request;
	CalcRequest(request);

	if (!request.IsEmpty())
	{
		m_clamp_request = FALSE;

		if (NeedRestart(request))
		{
			StopBuffering();
			return StartBuffering(request);
		}

		// The request wasn't restarted, so it may need to be clamped
		// by aborting it once enough data has become available.
		if (request.IsFinite() && !IsStreaming())
		{
			OpFileLength loading_end = FILE_LENGTH_NONE;
			m_use_url->GetAttribute(URL::KHTTPRangeEnd, &loading_end);
			if (loading_end == FILE_LENGTH_NONE || request.end < loading_end)
				m_clamp_request = TRUE;
		}
	}
	else
	{
		// We have all the data we wanted, so stop buffering if possible.
		switch (m_state)
		{
		case NONE:
			// Already loaded (data: URL or in cache).
			return IDLE;

		case IDLE:
		case FAILED:
			// not loading
			break;

		case STARTED:
		case HEADERS:
		case LOADING:
		case PAUSED:
			// Only stop a load if it's in fact already complete or if
			// it's one that we can later resume. However, when using
			// the streaming cache, continue loading until either the
			// cache fills up and PauseBuffering() is called or (if
			// the request fits in cache) IsLoadedURL() is true.
			if (IsLoadedURL(m_use_url) || (IsResumableURL(m_use_url) && !IsStreaming()))
			{
				StopBuffering();
				return IDLE;
			}
			break;
		}
	}

	return NONE;
}

MediaSourceImpl::State
MediaSourceImpl::StartBuffering(const MediaByteRange& request)
{
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(m_state == NONE || IsResumableURL(m_use_url));
	BOOL available = FALSE;
	OpFileLength dummy;
	GetPartialCoverage(request.start, available, dummy);
	OP_ASSERT(!available);
#endif // DEBUG_ENABLE_OPASSERT

	// Never retry a failed load.
	if (m_state == FAILED)
		return NONE;

	// Treat javascript: URI as an error.
	if (m_use_url->Type() == URL_JAVASCRIPT)
		return FAILED;

	if (m_use_url->Type() == URL_HTTP || m_use_url->Type() == URL_HTTPS) {
		// HTTP byte ranges are inclusive.
		m_use_url->SetAttribute(URL::KHTTPRangeStart, &request.start);
		m_use_url->SetAttribute(URL::KHTTPRangeEnd, &request.end);
		OP_NEW_DBG("StartBuffering", "MediaSource");
		OP_DBG(("") << request);
	}

	URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Full, FALSE, FALSE);
	OP_ASSERT(m_message_handler.get());
	CommState cs = m_use_url->LoadDocument(m_message_handler, m_referer_url, loadpolicy);
	switch (cs)
	{
	default:
		OP_ASSERT(!"Unhandled CommState");
	case COMM_REQUEST_FAILED:
		return FAILED;
	case COMM_LOADING:
		return STARTED;
	case COMM_REQUEST_FINISHED:
		return IDLE;
	}
}

void
MediaSourceImpl::StopBuffering()
{
	OP_NEW_DBG("StopBuffering", "MediaSource");
	OP_DBG((""));
	OP_ASSERT(m_message_handler.get());
	m_use_url->StopLoading(m_message_handler);
}

void
MediaSourceImpl::PauseBuffering()
{
	OP_NEW_DBG("PauseBuffering", "MediaSource");
	OP_DBG((""));

	OP_ASSERT(m_state == LOADING && IsStreaming());

	m_use_url->SetAttribute(URL::KPauseDownload, TRUE);
	m_state = PAUSED;
	for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
		m_listeners.GetNext(iter)->OnIdle(this);
}

void
MediaSourceImpl::ResumeBuffering()
{
	OP_NEW_DBG("ResumeBuffering", "MediaSource");
	OP_DBG((""));

	OP_ASSERT(m_state == PAUSED && IsStreaming());

	m_use_url->SetAttribute(URL::KPauseDownload, FALSE);
	m_state = LOADING;
}

OpFileLength
MediaSourceImpl::GetTotalBytes()
{
	// For non-resumable, non-streaming resources we know the duration
	// for sure once it fully loaded. Otherwise we have to trust the
	// Content-Length header, or return 0 if there is none.
	if (!IsResumableURL(m_use_url) && !IsStreaming() && IsLoadedURL(m_use_url))
		return m_use_url->ContentLoaded();
	return m_use_url->GetContentSize();
}

BOOL
MediaSourceImpl::IsStreaming()
{
	return m_use_url->UseStreamingCache();
}

BOOL
MediaSourceImpl::IsSeekable()
{
	return IsResumableURL(m_use_url) || IsLoadedURL(m_use_url);
}

#ifdef _DEBUG
static Debug&
operator<<(Debug& d, const OpMediaByteRanges* ranges)
{
	UINT32 length = ranges->Length();
	for (UINT32 i = 0; i < length; i++)
	{
		if (i > 0)
			d << ", ";
		d << "[";
		d << (long)ranges->Start(i);
		d << ", ";
		d << (long)ranges->End(i);
		d << ")";
	}
	return d;
}
#endif

OP_STATUS
MediaSourceImpl::GetBufferedBytes(const OpMediaByteRanges** ranges)
{
	if (!ranges)
		return OpStatus::ERR_NULL_POINTER;
	// Note: Multimedia_Storage::GetSortedCoverage requires the output
	// vector to be empty instead of reusing it. See CORE-37943.
	m_buffered.ranges.DeleteAll();
	RETURN_IF_ERROR(m_use_url->GetSortedCoverage(m_buffered.ranges));
	*ranges = &m_buffered;
#ifdef DEBUG_ENABLE_OPASSERT
	// Verify that the ranges are normalized.
	OpFileLength size = GetTotalBytes();
	OpFileLength prev_end = FILE_LENGTH_NONE;
	for (UINT32 i = 0; i < m_buffered.Length(); i++)
	{
		OpFileLength start = m_buffered.Start(i);
		OpFileLength end = m_buffered.End(i);
		OP_ASSERT(start != FILE_LENGTH_NONE);
		OP_ASSERT(end > 0 && end != FILE_LENGTH_NONE);
		OP_ASSERT(prev_end == FILE_LENGTH_NONE || prev_end < start);
		OP_ASSERT(start < end);
		OP_ASSERT(size == 0 || size >= end);
		prev_end = end;
	}
	OP_NEW_DBG("GetBufferedBytes", "MediaSource");
	OP_DBG(("size: %d; buffered: ", size) << *ranges);
#endif // DEBUG_ENABLE_OPASSERT
	return OpStatus::OK;
}

BOOL
MediaSourceImpl::NeedRestart(const MediaByteRange& request)
{
	OP_ASSERT(!request.IsEmpty());
	// Note: this function assumes that request is not in cache

	// If not loading we certainly need to start.
	if (m_state == NONE || m_state == IDLE)
		return TRUE;

	// Only restart resumable resources.
	if (!IsResumableURL(m_use_url))
		return FALSE;

	// Get the currently loading range.
	MediaByteRange loading;
	m_use_url->GetAttribute(URL::KHTTPRangeStart, &loading.start);
	m_use_url->GetAttribute(URL::KHTTPRangeEnd, &loading.end);
	OP_ASSERT(!loading.IsEmpty());

	// When streaming, adjust the loading range to not include what
	// has already been evicted from the cache. Note: This must not be
	// done for a request that was just started, as the cache can then
	// contain data from the previous request which is not relevant.
	if (m_state >= LOADING && IsStreaming())
	{
		BOOL available = FALSE;
		OpFileLength length = 0;
		GetPartialCoverage(loading.start, available, length);
		if (!available && (!loading.IsFinite() || length < loading.Length()))
			loading.start += length;
	}

	// Restart if request is before currently loading range.
	if (request.start < loading.start)
		return TRUE;

	// Restart if request is after currently loading range.
	if (loading.IsFinite() && request.start > loading.end)
		return TRUE;

	// request is now a subset of loading, check how much we have left
	// to load until request.start.
	BOOL available = FALSE;
	OpFileLength length = 0;
	GetPartialCoverage(loading.start, available, length);
	if (!available)
		length = 0;
	if (request.start > loading.start + length)
	{
		// FIXME: calculate download rate and time taken to reach offset (CORE-27952)
		OpFileLength remaining = request.start - (loading.start + length);
		if (remaining > MEDIA_SOURCE_MAX_WAIT)
			return TRUE;
	}

	return FALSE;
}

OP_STATUS
MediaSourceImpl::Read(void* buffer, OpFileLength offset, OpFileLength size)
{
	OP_NEW_DBG("Read", "MediaSource");
	OP_DBG(("offset=%ld; size=%ld", offset, size));

	m_read_offset = offset;

	if (!m_url_dd)
	{
		OP_ASSERT(m_message_handler.get());
		m_url_dd = m_use_url->GetDescriptor(m_message_handler, URL::KNoRedirect, TRUE);

		if (!m_url_dd)
			return OpStatus::ERR;
	}

	// Check if the requested data is already loaded.
	BOOL available = FALSE;
	OpFileLength available_size = 0;
	GetPartialCoverage(offset, available, available_size);
	if (!available || available_size < size)
	{
		if (available)
			OP_DBG(("only %ld bytes available", available_size));
		else
			OP_DBG(("not available (%ld bytes to next)", available_size));
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	OP_ASSERT(available_size > 0);

	m_url_dd->SetPosition(offset);
	OP_ASSERT(m_url_dd->GetPosition() == offset);

	OpFileLength total_read = 0;
	BOOL more = TRUE;
	while (total_read < size && more)
	{
		unsigned long bytes_read = m_url_dd->RetrieveData(more);

		if (bytes_read == 0)
			break;

		OP_ASSERT(bytes_read == m_url_dd->GetBufSize());

		UINT8* dd_buffer = (UINT8*)m_url_dd->GetBuffer();
		OP_ASSERT(dd_buffer);

		// Copy to buffer.
		unsigned long remaining = (unsigned long)(size - total_read);
		if (bytes_read > remaining)
			bytes_read = remaining;
		op_memcpy(&((UINT8*)buffer)[total_read], dd_buffer, bytes_read);
		// Consume data and move along.
		m_url_dd->ConsumeData(bytes_read);
		total_read += bytes_read;

		// FIXME: workaround for CORE-27705
		if (more && total_read >= available_size)
			break;
	}

	if (total_read < size)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (m_state == PAUSED)
	{
		BOOL available;
		OpFileLength remaining;
		GetStreamingCoverage(available, remaining);
		if (available && remaining > STREAMING_CACHE_RESUME_LIMIT)
			ResumeBuffering();
	}

	OP_DBG(("read %ld bytes", total_read));

	return OpStatus::OK;
}

void
MediaSourceImpl::LoadFailed()
{
	StopBuffering();
	m_state = FAILED;
	for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
		m_listeners.GetNext(iter)->OnError(this);
}

BOOL
MediaSourceImpl::VerifyContentType()
{
	if (m_verified_content_type)
		return TRUE;

	OP_ASSERT(m_use_url->GetAttribute(URL::KMultimedia) != FALSE);
	ContentDetector content_detector(m_use_url->GetRep(), TRUE, TRUE);
	OP_STATUS status = content_detector.DetectContentType();
	if (OpStatus::IsSuccess(status) &&
		(content_detector.WasTheCheckDeterministic() || IsLoadedURL(m_use_url)))
	{
		const char* sniffed_type = GetContentType();
		BOOL3 can_play = NO;
		status = g_media_module.CanPlayType(&can_play, sniffed_type);
		if (OpStatus::IsSuccess(status))
		{
			if (can_play == NO)
				status = OpStatus::ERR_NOT_SUPPORTED;
			else
				m_verified_content_type = TRUE;
		}
	}

	if (OpStatus::IsError(status))
	{
		RAISE_IF_MEMORY_ERROR(status);
		LoadFailed();
	}

	return m_verified_content_type;
}

/* virtual */ void
MediaSourceImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_MEDIA_SOURCE_ENSURE_BUFFERING)
	{
		OP_ASSERT(m_pending_ensure);
		EnsureBuffering();
		m_pending_ensure = FALSE;
		return;
	}
	else if (msg == MSG_MEDIA_SOURCE_NOTIFY_LISTENER)
	{
		// The listener may have been removed while the message was
		// pending, so find it in the listeners list before notifying.
		MediaProgressListener* listener = reinterpret_cast<MediaProgressListener*>(par2);
		for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
		{
			if (m_listeners.GetNext(iter) == listener)
			{
				if (m_state == FAILED)
					listener->OnError(this);
				else if (m_state == IDLE)
					listener->OnIdle(this);
				// else: The state has already changed and the
				// listener was notified just like the others.
				break;
			}
		}
		return;
	}

	// URL_DataDescriptor::RetrieveData posts MSG_URL_DATA_LOADED when
	// reading, which we are not interested in when idle or paused.
	// URL messages after an error are irrelevant to us.
	if (m_state == IDLE || m_state == PAUSED || m_state == FAILED)
		return;

	// URL messages in the message queue when calling StopLoading
	// will arrive after starting a new request, at which point they
	// must be ignored. Wait for headers/failure/redirect.
	if (m_state == STARTED && msg == MSG_URL_DATA_LOADED)
		return;

	OP_ASSERT(m_state == STARTED || m_state == HEADERS || m_state == LOADING);
	switch(msg)
	{
	case MSG_HEADER_LOADED:
		OP_ASSERT(m_state == STARTED);
		m_state = HEADERS;
		// If we are streaming, tell all listeners except the first to
		// restart loading, at which point they'll get a new source.
		if (IsStreaming())
		{
			OpListenersIterator iter(m_listeners);
			m_listeners.GetNext(iter);
			while (m_listeners.HasNext(iter))
				m_listeners.GetNext(iter)->OnClientCollision(this);
		}
		break;
	case MSG_URL_DATA_LOADED:
		if (m_state == HEADERS)
			m_state = LOADING;
		OP_ASSERT(m_state == LOADING);
		if (IsSuccessURL(m_use_url))
		{
			// This point will be reached very often while the
			// transfer is progressing normally, so it is important
			// that the following steps are not needlessly wasteful.

			if (!VerifyContentType())
				return;

			// EnsureBuffering does a non-trivial amount of work, so
			// only call it if (a) the request has finished or (b) the
			// current request is "too big" and needs clamping.
			if (m_clamp_request || IsLoadedURL(m_use_url))
			{
				EnsureBuffering();
				if (m_state != LOADING)
					break;
			}

			// The listeners are responsible for throttling the
			// side-effects buffering progress.
			for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
				m_listeners.GetNext(iter)->OnProgress(this);

			if (IsStreaming())
			{
				BOOL available;
				OpFileLength remaining;
				GetStreamingCoverage(available, remaining);
				if (available && remaining < STREAMING_CACHE_PAUSE_LIMIT)
					PauseBuffering();
			}
		}
		else
		{
			LoadFailed();
		}
		break;
	case MSG_URL_LOADING_FAILED:
		// Try to recover from a network errors that happen while
		// loading, but not those that happen before (or after) that.
		if (m_state == LOADING && IsResumableURL(m_use_url))
		{
			StopBuffering();
			m_state = IDLE;
			EnsureBuffering();
		}
		else
		{
			LoadFailed();
		}
		break;
	case MSG_URL_MOVED:
		{
			URL moved_to = m_use_url->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
			if (!moved_to.IsEmpty())
			{
				OP_ASSERT(m_use_url->Id(URL::KFollowRedirect) == (URL_ID)par2 &&
						  moved_to.Id(URL::KNoRedirect) == (URL_ID)par2);
				m_use_url.SetURL(moved_to);
				OP_DELETE(m_url_dd);
				m_url_dd = NULL;
				m_message_handler->UnsetCallBacks(this);
				RAISE_AND_RETURN_VOID_IF_ERROR(SetCallBacks());
			}
		}
		break;
	default:
		OP_ASSERT(FALSE);
	}

	// Ensure that we are notified of further buffering progress.
	if (m_url_dd)
		m_url_dd->ClearPostedMessage();
}

OP_STATUS
MediaSourceImpl::AddProgressListener(MediaProgressListener* listener)
{
	RETURN_IF_ERROR(m_listeners.Add(listener));
	if (m_state == IDLE || m_state == FAILED)
	{
		// Notify the new listener of the state asynchronously.
		m_message_handler->PostMessage(MSG_MEDIA_SOURCE_NOTIFY_LISTENER,
									   reinterpret_cast<MH_PARAM_1>(this),
									   reinterpret_cast<MH_PARAM_2>(listener));
	}
	return OpStatus::OK;
}

OP_STATUS
MediaSourceImpl::RemoveProgressListener(MediaProgressListener* listener)
{
	return m_listeners.Remove(listener);
}

void
MediaSourceImpl::GetStreamingCoverage(BOOL &available, OpFileLength &remaining)
{
	OpFileLength length;
	GetPartialCoverage(m_read_offset, available, length);
	if (available)
	{
		OP_ASSERT(length > 0);
		OpFileLength cache_size;
		m_use_url->GetAttribute(URL::KMultimediaCacheSize, &cache_size);
		OP_ASSERT(length <= cache_size);
		remaining = cache_size - length;
		OP_NEW_DBG("GetStreamingCoverage", "MediaSource");
		OP_DBG((("cached: %d; remaining: %d"), (int)length, (int)remaining));
	}
}

#endif // MEDIA_PLAYER_SUPPORT
