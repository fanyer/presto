/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef __THUMBNAIL_MANAGER_H__
#define __THUMBNAIL_MANAGER_H__

#ifdef SUPPORT_GENERATE_THUMBNAILS

#include "modules/img/image.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/dochand/fdelm.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/thumbnails/src/thumbnail.h"

#ifndef MSG_THUMBNAIL_LOADING_TIMEOUT
# define MSG_THUMBNAIL_LOADING_TIMEOUT MSG_COMM_TIMEOUT
#endif

const int THUMBNAIL_LOADING_TIMEOUT = 60000;

class ThumbnailManagerListener
{
public:
	virtual ~ThumbnailManagerListener() {} // class with virtual methods needs virtual destructor
	virtual void OnThumbnailRequestStarted(const URL &document_url, BOOL reload) = 0;
	virtual void OnThumbnailReady(const URL &document_url, const Image &thumbnail, const uni_char *title, long preview_refresh) = 0;
	virtual void OnThumbnailFailed(const URL &document_url, OpLoadingListener::LoadingFinishStatus status) = 0;
	virtual void OnInvalidateThumbnails() = 0;
};

/**
 * The thumbnail manager
 *
 * This singleton object keeps the thumbnails of pages identified by
 * their URLs. It maintains a cache of thumbnails in memory and on
 * disk.
 */
class ThumbnailManager : public MessageObject
{
	friend class SpeedDial; // Temporary hack for speed dial
public:

	struct ThumbnailSize
	{
	public:
		ThumbnailSize():
			base_thumbnail_width(-1),
			base_thumbnail_height(-1),
			max_zoom(-1)
		{};

		/**
		 * If the current size falls below these values,
		 * then the view-mode:minimized thumbnails will start scaling
		 * instead of changing the viewport size.
		 */
		int base_thumbnail_width;
		int base_thumbnail_height;

		double max_zoom;
	};

	/** First phase constructor */
	ThumbnailManager():
		m_cache(0),
#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
		m_active(FALSE),
#endif
		m_cached_images(0),
		m_global_access_stamp(0),
		m_index_dirty(FALSE),
		m_timer_count(0)
	{}

	~ThumbnailManager();

	/** Second phase constructor */
	OP_STATUS Construct();

	/** @short Retrieve a thumbnail for a window
	 *
	 * Retrieve a thumbnail for the document in the specified window.
	 * This call is synchronous. The thumbnail is returned from the
	 * cache if available, unless force_rebuild is TRUE.
	 *
	 * @param window The window to retrieve the thumbnail for
	 * @param[out] thumbnail The resulting thumbnail
	 * @param force_rebuild Not used
	 * @param mode Type of thumbnail to generate
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS GetThumbnailForWindow(Window *window, Image &thumbnail, BOOL force_rebuild, OpThumbnail::ThumbnailMode mode = OpThumbnail::ViewPort);

	/** @short Request a thumbnail for a URL
	 *
	 * Request a thumbnail for the specified URL. This call is
	 * asynchronous, i.e. the caller should register a listener to be
	 * notified of completion. The thumbnail is returned from the
	 * cache if available, unless a reload happens.
	 *
	 * @param document_url The URL to retrieve the thumbnail for
	 * @param check_if_expired If true, expiration check is performed
	 * @param reload If true, the document is forced to reload
	 * @param mode Type of thumbnail to generate
	 * @param target_width The desired width of the thumbnail
	 * @param target_height The desired height of the thumbnail
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS RequestThumbnail(const URL &document_url, BOOL check_if_expired, BOOL reload, OpThumbnail::ThumbnailMode mode = OpThumbnail::ViewPort, int target_width = THUMBNAIL_WIDTH, int target_height = THUMBNAIL_HEIGHT);

	void SetThumbnailSize(int base_width, int base_height, double max_zoom);
	ThumbnailSize& GetThumbnailSize() { return m_thumbnail_size; }

	/** @short Cancel thumbnail request
	 *
	 * Cancel a thumbnail request in progress for the specified URL.
	 * Listeners will receive OnThumbnailFailed. Ignored if there is
	 * no such request in progress.
	 *
	 * @param document_url The URL to cancel the request for
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS CancelThumbnail(const URL &document_url);

	/** @short Increment reference counter
	 *
	 * Increment reference count for the thumbnail of the specified
	 * URL. A nonzero reference count makes the thumbnail eligible for
	 * persistent storage.
	 *
	 * @param document_url The URL to increment reference count for
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS AddThumbnailRef(const URL &document_url);

	/** @short increment reference counter
	 *
	 * Decrement reference count for the thumbnail of the specified
	 * URL. A zero reference count doesn't prevent a thumbnail from
	 * being cached in memory.
	 *
	 * @param document_url The URL to decrement reference count for
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS DelThumbnailRef(const URL &document_url);

	/**
	 * Makes changes from AddThumbnailRef() and DelThumbnailRef()
	 * persistent.
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS Flush();

	/**
	 * Clear the memory cache (but not the disk cache). Should be
	 * called for "Delete private data".
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS Purge();

	/** @short Add thumbnail listener
	 *
	 * @param listener Listener to be added
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS AddListener(ThumbnailManagerListener* listener) { return m_listeners.Add(listener); }

	/** @short Remove thumbnail listener
	 *
	 * @param listener Listener to be removed
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS RemoveListener(ThumbnailManagerListener* listener) { return m_listeners.Remove(listener); }

#ifndef DOXYGEN_DOCUMENTATION
	// callback for the message object
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif

private:
	class Task;
	friend class Task;

	// Make CacheEntry friend so that the ADS compiler can access Task, which is private.
	struct CacheEntry;
	friend struct CacheEntry;

	// Make friends so that the BREW compiler doesn't complain
	class ThumbnailResizer;
	friend class ThumbnailResizer;

	class ImageDecodedListener
	{
	public:
		virtual void OnImageDecoded() = 0;
		virtual void OnImageDecodeFailed() = 0;
	};

	struct CacheEntry
#ifdef SUPPORT_SAVE_THUMBNAILS
		: public ImageContentProvider
#endif // SUPPORT_SAVE_THUMBNAILS
		, public ImageListener
	{
		CacheEntry(): decoded(FALSE), task(0), widget_window(0), view(0), access_stamp(0)
#ifdef SUPPORT_SAVE_THUMBNAILS
				, refcount(0), data(0), pos(0), datalen(0), preview_refresh(0), mode(OpThumbnail::ViewPort), target_width(0), target_height(0)
#endif // SUPPORT_SAVE_THUMBNAILS
				, m_image_decoded_listener(NULL)
				, free_msg_count(0)
		{}
		~CacheEntry();

#ifdef SUPPORT_SAVE_THUMBNAILS
		// ImageContentProvider implementation
		virtual OP_STATUS GetData(const char*& buf, INT32& len, BOOL& more);
		virtual OP_STATUS Grow() { return OpStatus::OK; }
		virtual void ConsumeData(INT32 len) { pos += len; }
		virtual INT32 ContentType() { return URL_PNG_CONTENT; }
		virtual INT32 ContentSize() { if (!data) ReadData(); return INT32(datalen); }
		virtual void SetContentType(INT32 content_type) {}
		virtual void Reset() { pos = 0; OP_DELETEA(data); data = 0; }
		virtual void Rewind() { pos = 0; }
		virtual BOOL IsEqual(ImageContentProvider* content_provider) { return this == content_provider; }
		virtual BOOL IsLoaded() { return TRUE; }
		OP_STATUS ReadData();
#endif // SUPPORT_SAVE_THUMBNAILS
		virtual void OnPortionDecoded();
		virtual void OnError(OP_STATUS status);
		void SetImageDecodedListener(ImageDecodedListener* listener) { m_image_decoded_listener = listener; }

		Image c_image;
		BOOL decoded;
		OpString key;
		Task *task;
		WidgetWindow *widget_window;
		class OpThumbnailView *view;
		Image image;
		Image resized_image;
		OpString title;
		int access_stamp;
		BOOL error_page;
#ifdef SUPPORT_SAVE_THUMBNAILS
		unsigned int refcount; // this is not a memory allocation aid but a counter of how many SD entries are using the image
		OpString filename;
		char *data;
		OpFileLength pos, datalen;
		long preview_refresh;
		OpThumbnail::ThumbnailMode mode;
		unsigned int target_width;
		unsigned int target_height;
		OpRect logo_rect;
#endif // SUPPORT_SAVE_THUMBNAILS

		int free_msg_count;

		ImageDecodedListener* m_image_decoded_listener;
	};

	class ThumbnailResizer:
		public ImageDecodedListener
	{
	public:

		static OP_STATUS Create(ThumbnailResizer** resizer, ThumbnailManager* manager, const URL &document_url, ThumbnailManager::CacheEntry* cache_entry, int target_width, int target_height);
		virtual ~ThumbnailResizer();

		virtual void OnImageDecoded();
		virtual void OnImageDecodeFailed();

	private:
		ThumbnailResizer(ThumbnailManager* manager, const URL& document_url, CacheEntry* cache_entry, int target_width, int target_height);
		OpBitmap* GetResizedThumbnailBitmap();
		BOOL StartDecoding();
		void DeleteThis();

		URL	m_document_url;
		CacheEntry* m_cache_entry;
		ThumbnailManager* m_thumbnail_manager;

		UINT32 m_retry_count;
		int m_target_width;
		int m_target_height;
	};

	OpVector<ThumbnailResizer>	m_resizers;





	OP_STATUS StartTimeout(CacheEntry* cache_entry)
	{
		if (!g_main_message_handler->HasCallBack(this, MSG_THUMBNAIL_LOADING_TIMEOUT, (MH_PARAM_1)cache_entry))
		{
			RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_THUMBNAIL_LOADING_TIMEOUT, (MH_PARAM_1)cache_entry));
		}
		g_main_message_handler->PostDelayedMessage(MSG_THUMBNAIL_LOADING_TIMEOUT, (MH_PARAM_1)cache_entry, 0, THUMBNAIL_LOADING_TIMEOUT);

		return OpStatus::OK;
	}
	void StopTimeout(CacheEntry* cache_entry)
	{
		if (g_main_message_handler->HasCallBack(this, MSG_THUMBNAIL_LOADING_TIMEOUT, (MH_PARAM_1)cache_entry))
		{
			g_main_message_handler->UnsetCallBack(this, MSG_THUMBNAIL_LOADING_TIMEOUT, (MH_PARAM_1)cache_entry);
		}
	}

#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	// Sequential generate helper functions
	void AddSuspendedTask(Task* task);
	void ResumeNextSuspendedTask();
#endif // THUMBNAILS_SEQUENTIAL_GENERATE

#ifdef SUPPORT_SAVE_THUMBNAILS
	enum { INDEX_FILE_VERSION = 1 };

	// The values in this enum are important! Do _not_ change the order or in
	// any other way modify the current values.
	enum {
		TAG_THUMBNAIL_ENTRY,
		TAG_BLOCK_URL,
		TAG_BLOCK_REFCOUNT, // obsolete
		TAG_BLOCK_FILENAME,
		TAG_BLOCK_TITLE,
		TAG_BLOCK_PREVIEW_REFRESH,
		TAG_BLOCK_MODE,
		TAG_BLOCK_TARGET_WIDTH,
		TAG_BLOCK_TARGET_HEIGHT,
		TAG_BLOCK_LOGO_RECT_X,
		TAG_BLOCK_LOGO_RECT_Y,
		TAG_BLOCK_LOGO_RECT_W,
		TAG_BLOCK_LOGO_RECT_H
	};
#endif // SUPPORT_SAVE_THUMBNAILS

	void OnThumbnailFinished(const URL &url, OpBitmap *buffer, long preview_refresh, int target_width, int target_height, OpThumbnail::ThumbnailMode mode, OpRect logo_rect);
	void OnThumbnailFailed(const URL &url, OpLoadingListener::LoadingFinishStatus status);
	void OnThumbnailCleanup(const URL &url);

	void NotifyThumbnailRequestStarted(const URL &url, BOOL reload);
	void NotifyThumbnailReady(const URL &url, const Image &thumbnail, const uni_char *title, long preview_refresh);
	void NotifyThumbnailFailed(const URL &url, OpLoadingListener::LoadingFinishStatus status);
	void NotifyInvalidateThumbnails();

	CacheEntry * CacheNew(const URL &url);
	CacheEntry * CacheLookup(const URL &url);
	OP_STATUS CachePut(const URL &url, CacheEntry * cache_entry);
	const Image & CacheGetImage(CacheEntry * cache_entry);
	OP_STATUS CachePutImage(CacheEntry * cache_entry, const Image &image);
	OP_STATUS CachePutTitle(CacheEntry * cache_entry, const uni_char *title);
	void CachePutPreviewRefresh(CacheEntry* cache_entry, long preview_refresh);
	void CachePutThumbnailMode(CacheEntry* cache_entry, OpThumbnail::ThumbnailMode mode);
	void CachePutRequestTargetSize(CacheEntry* cache_entry, int target_widht, int target_height);
	void CachePutLogoRect(CacheEntry* cache_entry, OpRect logo_rect);

	OP_STATUS LoadIndex();
	OP_STATUS SaveIndex();

private:
	void FreeCache();

private:

	OpStringHashTable<CacheEntry>* m_cache;
	friend class OpAutoStringHashTable<CacheEntry>;
#ifdef THUMBNAILS_SEQUENTIAL_GENERATE
	Head m_suspended_tasks;
	BOOL m_active;
#endif // THUMBNAILS_SEQUENTIAL_GENERATE
	OpListeners<ThumbnailManagerListener> m_listeners;
	int m_cached_images, m_global_access_stamp;
	BOOL m_index_dirty;
	UINT32 m_timer_count;		// count on how thumbnails we are running the timer for

	ThumbnailSize m_thumbnail_size;
};

#endif // SUPPORT_GENERATE_THUMBNAILS

#endif // __THUMBNAIL_MANAGER_H__
