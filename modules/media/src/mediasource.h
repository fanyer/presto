/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/mediabyterange.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/windowcommander/OpMediaSource.h"

class MediaSource;
class MediaSourceImpl;
class MessageHandler;

class MediaSourceManager
{
public:
	virtual ~MediaSourceManager() { }

	/** Get a MediaSource from a URL.
	 *
	 * This will create a new MediaSource if none existed.
	 * Use PutSource(source) when done.
	 */
	virtual OP_STATUS GetSource(MediaSource*& source, const URL& url, const URL& referer_url,
								MessageHandler* message_handler = NULL) = 0;

	/** Get a MediaSource from a handle.
	 *
	 * This is used to get a source after a roundtrip to the platform
	 * and will fail if the source doesn't already exist.
	 * Use PutSource(source) when done.
	 */
	virtual OP_STATUS GetSource(MediaSource*& source, OpMediaHandle handle) = 0;

	/** Give back a MediaSource to the manager (for possible deletion).
	 *
	 * @param source A MediaSource instance returned from this
	 *               MediaSourceManager instance's GetSource method.
	 */
	virtual void PutSource(MediaSource* source) = 0;
};

class MediaSourceManagerImpl :
	public MediaSourceManager
{
public:
	MediaSourceManagerImpl() : m_url_context_id(0) {}
	~MediaSourceManagerImpl() { OP_ASSERT(m_sources.GetCount() == 0); }
	OP_STATUS GetSource(MediaSource*& source, const URL& url, const URL& referer_url,
						MessageHandler* message_handler = NULL);
	OP_STATUS GetSource(MediaSource*& source, OpMediaHandle handle);
	void PutSource(MediaSource* source);

#ifdef SELFTEST
	INT32 GetCount() const { return m_sources.GetCount(); }
#endif

private:
	URL GetUrlWithMediaContext(const URL& url);

	OpHashTable m_sources;
	URL_CONTEXT_ID m_url_context_id;
};

class MediaProgressListener
{
public:
	virtual ~MediaProgressListener() {}

	/** Buffering is progressing normally. */
	virtual void OnProgress(MediaSource* src) = 0;

	/** Buffering has stopped due to network error. */
	virtual void OnError(MediaSource* src) = 0;

	/** Buffering has stopped due to buffering strategy. */
	virtual void OnIdle(MediaSource* src) = 0;

	/** The MediaSource is held exclusively by another client.
	 *
	 * When multiple clients cannot be served by the same MediaSource,
	 * this callback is sent to all listeners except one. The listener
	 * must release this source and request a new one.
	 */
	virtual void OnClientCollision(MediaSource* src) = 0;
};

/** Indicate which byte ranges have pending reads and need preload.
 *
 * Clients of MediaSource should inherit from MediaSourceClient and
 * call MediaSource::HandleClientChange() after changing m_pending or
 * m_preload.
 */
class MediaSourceClient :
	public ListElement<MediaSourceClient>
{
public:
	const MediaByteRange& GetPending() const { return m_pending; }
	const MediaByteRange& GetPreload() const { return m_preload; }
protected:
	// Initially, there is no pending read and the entire resource
	// should be preloaded.
	MediaSourceClient() : m_preload(0) {}
	MediaByteRange m_pending;
	MediaByteRange m_preload;
};

class MediaSource
{
public:
	virtual void AddClient(MediaSourceClient* client) = 0;
	virtual void RemoveClient(MediaSourceClient* client) = 0;

	/** A client was added or removed.
	 *
	 * The buffering strategy is updated accordingly.
	 */
	virtual void HandleClientChange() = 0;

	// For OpMediaSourceImpl
	virtual const char* GetContentType() = 0;
	virtual BOOL IsSeekable() = 0;
	virtual OpFileLength GetTotalBytes() = 0;
	virtual OP_STATUS GetBufferedBytes(const OpMediaByteRanges** ranges) = 0;
	virtual OP_STATUS Read(void* buffer, OpFileLength offset, OpFileLength size) = 0;

	/** Add/remove listeners to receive progress callbacks. */
	virtual OP_STATUS AddProgressListener(MediaProgressListener* listener) = 0;
	virtual OP_STATUS RemoveProgressListener(MediaProgressListener* listener) = 0;
	/** Get the original URL for security purposes. */
	virtual URL GetOriginURL() = 0;

protected:
	virtual ~MediaSource() {}
};

class MediaSourceImpl :
	public MediaSource,
	private MessageObject
{
public:
	void AddClient(MediaSourceClient* client)
	{
		client->Into(&m_clients);
		HandleClientChange();
	}

	void RemoveClient(MediaSourceClient* client)
	{
		OP_ASSERT(m_clients.HasLink(client));
		client->Out();
		HandleClientChange();
	}

	void HandleClientChange();

	const char* GetContentType() { return m_use_url->GetAttribute(URL::KMIME_Type).CStr(); }

	BOOL IsSeekable();
	OpFileLength GetTotalBytes();
	OP_STATUS GetBufferedBytes(const OpMediaByteRanges** ranges);

	OP_STATUS Read(void* buffer, OpFileLength offset, OpFileLength size);

	OP_STATUS AddProgressListener(MediaProgressListener* listener);
	OP_STATUS RemoveProgressListener(MediaProgressListener* listener);

	URL GetOriginURL() { return m_use_url; }

private:
	friend class MediaSourceManagerImpl;
	friend class OpAutoPtr<MediaSourceImpl>;

	/** The loading state. */
	enum State
	{
		NONE,    /**< initial state */
		STARTED, /**< waiting for headers */
		HEADERS, /**< have headers */
		LOADING, /**< loading (have data) */
		IDLE,    /**< not loading (possibly done) */
		FAILED,  /**< network error */
		PAUSED   /**< paused transfer */
	};

	/** Constructor.
	 *
	 * @param url the URL to load.
	 * @param referer_url URL of the loading document.
	 * @param message_handler MessageHandler for the URL load.
	 */
	MediaSourceImpl(const URL& url, const URL& referer_url,
					MessageHandler* message_handler);

	virtual ~MediaSourceImpl();

	OP_STATUS Init();
	OP_STATUS SetCallBacks();

	/** Calculate the requested range.
	 *
	 * This is based on the clients' preload requirements and what is
	 * already cached.
	 */
	void CalcRequest(MediaByteRange& request);

	/** Start or stop buffering as necessary.
	 *
	 * Delegates to EnsureBufferingInternal(), updates the internal
	 * state and calls LoadFailed() or VerifyContentType() if needed.
	 */
	void EnsureBuffering();

	/** Start or stop buffering as necessary.
	 *
	 * @return STARTED, IDLE or FAILED if buffering started, finished
	 * or failed respectively, or NONE if nothing was done.
	 */
	State EnsureBufferingInternal();

	/** Start buffering the requested range.
	 *
	 * @return The new state, or NONE if it did not change.
	 */
	State StartBuffering(const MediaByteRange& request);

	/** Stop buffering by aborting the request. */
	void StopBuffering();

	/** Pause buffering without aborting the request. */
	void PauseBuffering();

	/** Resume a previously paused connection. */
	void ResumeBuffering();

	/** Need to start/stop buffering to get the requested data? */
	BOOL NeedRestart(const MediaByteRange& request);

	/** Stop buffering and notify all listeners of the error. */
	void LoadFailed();

	/** Is the streaming cache used? */
	BOOL IsStreaming();

	/** Sniff the Content-Type and verify that we support it.
	 *
	 * When the Content-Type is known to not be supported,
	 * LoadFailed is called internally.
	 *
	 * @return TRUE if the Content-Type could be determined and is
	 * supported, otherwise FALSE. OOM is handled internally.
	 */
	BOOL VerifyContentType();

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** See URL::GetPartialCoverage */
	void GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length) { m_use_url->GetPartialCoverage(position, available, length, TRUE); }

	/** Get the streaming cache coverage.
	 *
	 * This is the number of remaining bytes until the current read
	 * offset would be overwritten by new data in the ring buffer.
	 */
	void GetStreamingCoverage(BOOL &available, OpFileLength &remaining);

	UINT32 m_refcount;

	/** The URL originally used to create the instance.
	 *
	 * The Id() of this URL is the key in the source manager's
	 * instance hash table.
	 */
	URL m_key_url;
	/** The URL of the document which included this resource. */
	URL m_referer_url;
	/** The (possibly redirected) URL which is actually used. */
	URL_InUse m_use_url;
	URL_DataDescriptor* m_url_dd;
	OpListeners<MediaProgressListener> m_listeners;
	List<MediaSourceClient> m_clients;
	TwoWayPointer<MessageHandler> m_message_handler;

	/** The internal loading state.
	 *
	 * Modified by HandleCallback, EnsureBuffering, PauseBuffering,
	 * ResumeBuffering and LoadFailed.
	 */
#ifdef _DEBUG
	State m_state;
#else
	unsigned m_state:4;
#endif // _DEBUG

	/** The ongoing request needs to be forcibly clamped to what is
	 *  being requested by the clients by stopping it prematurely. */
	unsigned m_clamp_request:1;

	/** A MSG_MEDIA_SOURCE_ENSURE_BUFFERING is pending. */
	unsigned m_pending_ensure:1;

	/** Internal state of VerifyContentType(). */
	unsigned m_verified_content_type:1;

	/** The current read offset, used to determine when to
	 *  pause/resume the transfer. */
	OpFileLength m_read_offset;

	class MediaByteRanges :
		public OpMediaByteRanges
	{
	public:
		virtual ~MediaByteRanges() {}
		virtual UINT32 Length() const
		{
			return ranges.GetCount();
		}
		virtual OpFileLength Start(UINT32 idx) const
		{
			OP_ASSERT(idx < ranges.GetCount());
			StorageSegment* seg = ranges.Get(idx);
			return seg->content_start;
		}
		virtual OpFileLength End(UINT32 idx) const
		{
			OP_ASSERT(idx < ranges.GetCount());
			StorageSegment* seg = ranges.Get(idx);
			return seg->content_start + seg->content_length;
		}
		OpAutoVector<StorageSegment> ranges;
	};
	MediaByteRanges m_buffered;
};

#endif // MEDIA_PLAYER_SUPPORT

#endif // MEDIASOURCE_H
