// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.



#ifndef WEBFEEDS_API_IMPL_H_
#define WEBFEEDS_API_IMPL_H_

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/webfeeds_api.h"

#include "modules/forms/src/formiterator.h"
#include "modules/util/adt/opvector.h"

#ifndef CORE_2
# define CORE_2
#endif

#ifndef g_webfeeds_api_impl  // So we don't have to cast the g_webfeeds_api pointer all the time... 
# define g_webfeeds_api_impl ((WebFeedsAPI_impl*)g_webfeeds_api) 
#endif 

#define WEBFEEDS_SUBSCRIBED_LIST_URL "opera:feed-id/all-subscribed"
#define WEBFEEDS_SETTINGS_URL "opera:feed-id/settings"

class WebFeed;
class WebFeedEntry;
class OpHashTable;
class WebFeedStorage;
class WriteBuffer;
class WebFeedLoadManager;
#ifdef WEBFEEDS_EXTERNAL_READERS
class WebFeedReaderManager;
#endif // WEBFEEDS_EXTERNAL_READERS



class WebFeedFormIterator
{
public:
	WebFeedFormIterator() {}
	OP_STATUS Init(FramesDocument* frm_doc, const uni_char* id);

	HTML_Element* GetNext();
protected:
	OpAutoPtr<FormIterator> m_form_iterator;
};


/**
 * Feed objects are contained in a hash table (with FeedIds as key).
 * 
 * The table actually contains WebFeedStub-objects, not WebFeed objects, 
 * see WebFeedStub documentation.
 * In retrospect it would probably be better to have the WebFeed class be the
 * proxy, and let it contain some storage class for the disk-data which can
 * be loaded from and written to disk transparently.
 */


class WebFeedsAPI_impl : public WebFeedsAPI, public MessageObject
{
public:
	/**
	 * All requests for feeds (which goes through the API) goes through 
	 * this proxy class.
	 * 
	 * This is so that the whole feed doesn't need to be in memory,
	 * only the stub is in memory at all time.  When the feed is needed
	 * the stub will load it from disk.
	 * 
	 * The WebFeedsAPI_impl class has a array of the feeds being
	 * kept in memory.  This array is expected to remain short, as only
	 * a few feeds will be in memory at the same time.  Most will be fetched
	 * from disk when they are needed.
	 * 
	 * The algoritm for selecting which feeds to keep in memory is currently
	 * very simple.  An array contains the N last
	 * added feeds, and WebFeedsAPI_impl::AddFeed throws out the feed which
	 * was added longest ago and inserts the new one if all the slots in 
	 * the array are filled already.
	 * */
	class WebFeedStub
	{
	public:
		WebFeedStub();
		~WebFeedStub();

		OP_STATUS		Init();
		OP_STATUS		Init(const uni_char* title, double update_time,
		                     UINT update_interval, UINT min_update_interval, 
		                     URL& feed_url, BOOL subscribed);

		/** Frees the memory used for the feed object (and its entries).
		 *  The stub object will remain.
		 *  It will be reloaded (from cached file version) by doing GetFeed()
		 *  on the stub.
		 *  Might not actually remove the feed, if there are others having
		 *  references to the feed, but it will be removed once those references
		 *  disappear (i.e. do DecRef on the feed object)
		 * 
		 *  Only WebFeedsAPI_impl::AddFeed should call this, or at least consult
		 *  with arneh@opera.com and change this comment if someone else changes
		 *  it, to be sure of preserving the logic. */
		OP_STATUS		RemoveFeedFromMemory();

		/// Deletes stub immediately if it has no feed object, otherwise
		/// it will be deleted with the feed, once the last reference
		/// to the feed is gone
		void			StoreDestroyed();
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
		/// Saves the feed to disk if it is in memory.
		/// If it is not in memory it has already been saved.
		void			SaveL(WriteBuffer &buf, BOOL is_store);
#endif
		/// Get the feed object. If it is not in memory, it will be loaded
		/// from disk
		WebFeed*		GetFeed();
		void			SetFeed(WebFeed* feed, BOOL webfeed_init=FALSE);
		
		OpFeed::FeedId	GetId() { return m_id; }
		void			SetId(OpFeed::FeedId id) { m_id = id; }
		
		OpFeed::FeedStatus GetStatus() { return m_status; }
		void			SetStatus(OpFeed::FeedStatus status) { m_status = status; }
		
		/// This is cleaned up version of the title (with removed markup) for use
		/// in the UI
		OP_STATUS		GetTitle(OpString& title);
		BOOL			HasTitle() { return !m_title.IsEmpty(); }
		/// Title escaped for all HTML special characters, so that it can be inserted into HTML without problems
		OP_STATUS		GetEscapedTitle(OpString& escaped_title);
		/// Sets the title used in the UI. Should be plain text, i.e. NOT markup
		OP_STATUS		SetTitle(const uni_char* title) { return m_title.Set(title); }
		
		/// Last time feed was updated
		double			GetUpdateTime() { return m_update_time; }
		void			SetUpdateTime(double time) { m_update_time = time; }
		UINT			GetUpdateInterval();
		void			SetUpdateInterval(UINT interval);
		/// Minimum update time allowed by the feed/server
		UINT			GetMinUpdateInterval() { return m_min_update_interval; }
		void			SetMinUpdateInterval(UINT interval) { m_min_update_interval = interval; }
		URL&			GetURL() { return m_feed_url; }
		void			SetURL(URL& url) { m_feed_url = url; }

		BOOL			IsSubscribed() { return m_packed.is_subscribed; }
		void			SetSubscribed(BOOL subscribed) { m_packed.is_subscribed = subscribed; }
		/// Returns true when feeds are bein updated. At most one update should run at any time (UpdateFeeds() honours that)
		BOOL			IsUpdating() { return m_packed.is_updating; }
		void			SetIsUpdating(BOOL updating) { m_packed.is_updating = updating; }

		/// URL to a file:: URL for a favicon for the feed (if any). Used in the UI.
		/// Returns an empty string if there is no favicon for the feed
		OP_STATUS		GetInternalFeedIcon(OpString& icon_url);
		BOOL			HasInternalFeedIcon() { return !m_feed_icon.IsEmpty(); }
		/// The filename of the internal favicon of the feed
		const uni_char*	GetInternalFeedIconFilename() { return m_feed_icon.CStr(); }
		/// The filename of the internal favicon of the feed
		OP_STATUS		SetInternalFeedIconFilename(const uni_char* icon_url) { return m_feed_icon.Set(icon_url); }

		/// Returns a opera:feed-id URL to a generated page where the contents of the feed can be seen.
		OP_STATUS		GetFeedIdURL(OpString& result_string);
		void			IncRef();  ///< Only WebFeedsAPI_impl should use this.  Apart from the API and the parser during parsing no one should hold references to stub objects

		/// Number of entries in feed
		UINT			GetTotalCount() { return m_total_entries; }
		/// Number of unread entries in feed
		UINT			GetUnreadCount() { return m_unread_entries; }
		void			IncrementTotalCount(int increment_by=1) { m_total_entries += increment_by; }
		void			IncrementUnreadCount(int increment_by=1) { m_unread_entries += increment_by; }

		/// Has this feed reached it's disk space quota
		BOOL			HasReachedDiskSpaceLimit() { return m_packed.space_limited; }
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
		void			SetHasReachedDiskSpaceLimit(BOOL limit_reached) { m_packed.space_limited = limit_reached; }
		/// Has this feed been changed since the last time it was saved
		BOOL			FeedNeedSaving() { return m_feed != NULL; }

		/// Space in bytes the feed uses on disk
		UINT			GetDiskSpaceUsed();
		void			SetDiskSpaceUsed(UINT size) { m_disk_space_used = size; }
		/// Disk space used by the feeds favicon 
		UINT			GetIconDiskSpace() { return m_packed.icon_disk_size; }
		void			SetIconDiskSpace(UINT size) { m_packed.icon_disk_size = size; }
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

		/// Get what these HTTP headers were at last load, and set them to only load if feed changed
		time_t			GetHttpLastModified() { return m_http_last_modified; }
		void			SetHttpLastModified(time_t last_modified) { m_http_last_modified = last_modified; }
		char*			GetHttpETag() { return m_http_etag.CStr(); }
		OP_STATUS		SetHttpETag(const char* etag) { return m_http_etag.Set(etag); }
	private:
		WebFeed*		m_feed;
		OpFeed::FeedId	m_id;
		OpFeed::FeedStatus m_status;
		OpString		m_title;
		double			m_update_time;

		UINT			m_update_interval;
		UINT			m_min_update_interval;
		URL				m_feed_url;
		OpString		m_feed_icon;
		
		time_t			m_http_last_modified;
		OpString8		m_http_etag;

		UINT			m_total_entries;     ///< Total number of entries in feed
		UINT			m_unread_entries;    ///< Number of unread entries in feed
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
		UINT			m_disk_space_used;
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

		union
		{
			struct
			{
				UINT is_subscribed:1;
				UINT is_updating:1;
				UINT space_limited:1;
				UINT icon_disk_size:16;
			} m_packed;
			UINT m_packed_init;
		};
	};

	WebFeedsAPI_impl();
	virtual ~WebFeedsAPI_impl();
	OP_STATUS 		Init();
	
	OP_STATUS		LoadFeed(URL& feed_url, OpFeedListener* listener, BOOL return_unchanged_feeds = TRUE, time_t last_modified = 0, const char* etag = NULL);
	OP_STATUS 		LoadFeed(WebFeed* existing_feed, URL& feed_url, OpFeedListener*, BOOL return_unchanged_feed = TRUE, time_t last_modified = 0, const char* etag = NULL);

	OP_STATUS		AbortLoading(URL& feed_url);
	OP_STATUS		AbortAllFeedLoading(BOOL updating_feeds_only=FALSE);
	
	/** Does NOT transfer ownership (caller must still do DecRef when done with pointer)
	 * This variant assumes that the feed object has already been created, mostly
	 * useful to call from the parser which makes a feed object during parsing.
	 *
	 * Note that LastUpdated time is not actually stored in the feed object
	 * (but rather in the stub object), so can't be set until after call to
	 * AddFeed */
	OP_STATUS		AddFeed(WebFeed*);

	/** Use this one if you have created a stub already.  Note that if the stub
	 *  has already been added to the store, the you must call this method with
	 *  the same id as the old one was added with (id's should never change, so 
	 *  this shouldn't be hard).
	 *  Assumes ownership of the stub */
	OP_STATUS		AddFeed(WebFeedStub*); //, OpFeed::FeedId id=0);

	OP_STATUS		AddOmniscientListener(OpFeedListener*);
	OpVector<OpFeedListener>*	GetAllOmniscientListeners();
	OP_STATUS		RemoveListener(OpFeedListener* listener);
	
	OP_STATUS		UpdateFeeds(OpFeedListener *listener, BOOL return_unchanged_feeds = FALSE, BOOL force_update=TRUE);
	void			FeedFinished(OpFeed::FeedId);  // called by load manager when a feed has finished loading, used to keep track of updating feeds

#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	void			DoAutomaticUpdates(BOOL);
	BOOL			GetAutoUpdate() { return m_do_automatic_updates; }
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT

	OpFeed*			GetFeedByUrl(const URL& feed_url);
	OpFeed*			GetFeedById(OpFeed::FeedId id);
	UINT*			GetAllFeedIds(UINT& length, BOOL subscribed_only=TRUE);
	UINT			GetNumberOfFeeds();

#ifdef WEBFEEDS_DISPLAY_SUPPORT

	OP_STATUS		GetFeedFromIdURL(const char* feed_id_url, OpFeed*&, OpFeedEntry*&, FeedIdURLType&);  // last three are out parameters with result
	OpFeedWriteStatus	WriteByFeedId(const char* feed_id, URL& out_url);
	OP_STATUS		WriteSubscriptionList(URL& out_url) { TRAPD(status, WriteSubscriptionListL(out_url)); return status; }
	void			WriteSubscriptionListL(URL& out_url, BOOL complete_document=TRUE);

#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
	void			WriteGlobalSettingsPageL(URL& out_url);
	OP_STATUS		WriteGlobalSettingsPage(URL& out_url);
	void			WriteUpdateAndExpireFormL(URL& out_url, WebFeed* feed);
	OP_STATUS		ContainsAnyCheckedFeeds(FramesDocument*, BOOL& checked);
	OP_STATUS		RunCommandOnChecked(FramesDocument*, SubscriptionListCommand);
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#ifdef WEBFEEDS_REFRESH_FEED_WINDOWS
	OP_STATUS		UpdateFeedWindow(WebFeed* feed);  ///< Refreshed all windows containing info from the feed
#endif
#endif // WEBFEEDS_DISPLAY_SUPPORT

	void			SetMaxSize(UINT size);
	UINT			GetMaxSize();
	
	UINT			GetDefMaxAge();
	void			SetDefMaxAge(UINT age);
	UINT			GetDefMaxEntries();
	void			SetDefMaxEntries(UINT entrys);
	UINT			GetDefUpdateInterval();
	void			SetDefUpdateInterval(UINT interval);
	BOOL			GetDefShowImages();
	void			SetDefShowImages(BOOL show);

	void			SetNextFreeFeedId(OpFeed::FeedId id);

	WebFeedLoadManager* GetLoadManager() { return m_load_manager.get(); }
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	UINT			SaveL(WriteBuffer &buf);
	WebFeedStorage* GetStorage() { return m_storage.get(); }
	void			AddToInternalFeedCache(WebFeedStub* stub);
	/// Space used by store file, i.e. NOT including all the feed files
	UINT			GetDiskSpaceUsed() { return m_disk_space_used; }
	/// Number of bytes available for each feed. Returns 0 if unlimited
	UINT			GetFeedDiskSpaceQuota();

	/// Deletes all unused feed and icon files in feeds folder
	OP_STATUS		DeleteUnusedFiles();

	/// We dynamicly adjust the space factor to try to use no more than
	/// MaxSize disk space. If we use too much we adjust the factor down
	/// so that on the next save it should use less, and vice versa.
	double			GetMaxSizeSpaceFactor() { return m_store_space_factor; }
	void			SetMaxSizeSpaceFactor(double factor) { m_store_space_factor = factor; }
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

	/** Deletes entries above the maximum allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself keeps the number of entries
	 *  below maximum when doing updates */
	OP_STATUS		CheckAllFeedsForNumEntries();

	/** Deletes entries older than allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself expires entries
	 *  when doing updates */
	OP_STATUS	 	CheckAllFeedsForExpiredEntries();

#ifdef WEBFEEDS_EXTERNAL_READERS
	/** Check the number of external feed readers known to the webfeeds module
	 *
	 * \return Number of feed readers known */
	unsigned		GetExternalReaderCount() const;

	/** Get the unique, persistent ID of an external feed reader
	 *
	 * \param index[in] Index of feed reader to check, must be less than GetExternalReaderCount()
	 * \return Unique, persistent ID of feed reader, can be used in functions below */
	unsigned		GetExternalReaderIdByIndex(unsigned index) const;

	/** Get the human-readable name of an external feed reader
	 *
	 * \param id[in] ID of feed reader to get name for
	 * \param name[out] Human-readable name, for example 'Google Reader' */
	OP_STATUS		GetExternalReaderName(unsigned id, OpString& name) const;

	/** Get a URL to use to subscribe to a feed using an external feed reader
	 *
	 * \param id[in] ID of feed reader with which to subscribe
	 * \param feed_url[in] Feed to subscribe to
	 * \param target_url[out] URL that can be used to subscribe to feed_url in external feed reader */
	OP_STATUS		GetExternalReaderTargetURL(unsigned id, const URL& feed_url, URL& target_url) const;

#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	/** Check if an externa feed reader can be modified (and deleted) by user
	 *
	 * \param id[in] ID of feed reader to add
	 * \param can_edit[out] TRUE if the reader can be modified, otherwise FALSE */
	OP_STATUS 		GetExternalReaderCanEdit(unsigned id, BOOL& can_edit) const;

	/** Return an identifier that is not in use. Can be used for \ref AddExternalReader */
	unsigned		GetExternalReaderFreeId() const;

	/** Add external feed reader
	 *
	 * \param id[in] ID of feed reader to add
	 * \param feed_url[in] The url of an external feed reader
	 * \param feed_name[in] The name of an external feed reader */
	OP_STATUS		AddExternalReader(unsigned id, const uni_char *feed_url, const uni_char *feed_name) const;

	/** Check if an external feed reader is already known
	 *
	 * \param feed_url[in] The url of an external feed reader
	 * \param id[out] If not NULL and the function returns TRUE
	 * it contains the id of the known feed reader. Otherwise it's irrelevant.
	 *
	 * @note This function does string comparison and
	 * the same URL may have different string representation depending on
	 * e.g. the name attribute variant used. It's up to a caller to take care of
	 * passing proper url string form in order to get the proper answer.
	 * It basically needs to be the same as passed to AddExternalReader().
	 * \see AddExternalReader() */
	BOOL			HasExternalReader(const uni_char *feed_url, unsigned *id = NULL) const;

	/** Deletes external feed reader
	 *
	 * \param id[in] ID of feed reader to delete */
	OP_STATUS			DeleteExternalReader(unsigned id) const;
#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
#endif // WEBFEEDS_EXTERNAL_READERS

	/// MessageHandler Callback
	void 			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
protected:
	static void 	HashDelete(const void*, void*);

	WebFeedStub*	GetStubById(OpFeed::FeedId);
	WebFeedStub*	GetStubByUrl(const URL& feed_url);

	OP_STATUS		AddUpdateListener(OpFeedListener*);
	void			UpdateFinished();
	OP_STATUS		UpdateSingleFeed(WebFeedStub* stub, OpFeedListener *listener, BOOL return_unchanged_feeds, BOOL force_update);
#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	/// Figures out how long until the shortest time until a feed is supposed
	/// to be updated next, and schedules an update for that time
	void			ScheduleNextUpdate();
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT

	OP_STATUS		RegisterCallbacks();
	void			UnRegisterCallbacks();

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	void			ReplaceOldestInCache(WebFeedStub* stub); ///< Throws out first feed from memory cache and inserts the parameter last. Only to be called by AddToInternalFeedCache
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
	
	OpFeed::FeedId	m_next_free_feedid;
	MessageHandler* mh;
	AutoDeleteHead	m_loading_urls;  ///< Used by LoadIcon/HandleCallback to keep track of which icons are currently loading

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	WebFeedStub*	m_feeds_memory_cache[WEBFEEDS_MAX_FEEDS_IN_MEMORY]; ///< Array of feeds currently in memory. Expected to remain small, we keep most feeds on disk. Value decided by tweak.

	OpAutoPtr<WebFeedStorage>	m_storage;
	double			m_store_space_factor;  ///< space which will be shared evenly
										 ///< between feeds, adjustet adaptivly to not use more then max size
	UINT			m_disk_space_used;  ///< Amount of space used at last save
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
	OpAutoPtr<WebFeedLoadManager> m_load_manager;
	
	OpVector<OpFeedListener>	m_omniscient_listeners;
	OpVector<OpFeedListener> 	m_update_listeners;
	OpVector<OpFeedListener> 	m_postponed_listeners;
	UINT						m_number_updating_feeds;

#ifdef WEBFEEDS_EXTERNAL_READERS
	OpAutoPtr<WebFeedReaderManager> m_external_reader_manager;
#endif // WEBFEEDS_EXTERNAL_READERS

	UINT			m_max_size;          ///< Amount of space use we're trying to stay below
	UINT			m_max_age;
	UINT			m_max_entries;
	UINT			m_update_interval;
	BOOL			m_do_automatic_updates;
	double			m_next_update;
	BOOL			m_show_images;
	BOOL			m_skin_feed_icon_exists;
	
	OpHashTable*	m_feeds; ///< WebFeedStub object for all feeds we know about.
	                         ///< Will have to delete all stub objects as well when
	                         ///< exiting, so do it in destructors.
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif /*WEBFEEDS_API_IMPL_H_*/
