// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef WEBFEED_LOADING_FEED_H_
#define WEBFEED_LOADING_FEED_H_

#include "modules/webfeeds/webfeeds_api.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeedparser.h"

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/opvector.h"

class WebFeed;
class WebFeedParser;

/**
 * This class is used to load (and update) feeds.
 * 
 * This class is used internally by the module and should be used for all
 * feed loading.  It will only load a limited number of feeds at a time, to 
 * restrict resource usage.  If updating a feed it will keep it in disk storage
 * until it is needed in memory to limit memory usage.
 */

class WebFeedLoadManager
{
protected:
	/** This is a class used from someone requests a feed to be loaded until
	 *  all listeners have been notified.  It contains all sorts of meta
	 *  information needed during loading.
	 */
	class LoadingFeed : public Link, public WebFeedParserListener, public MessageObject {
	public:
		LoadingFeed();
		LoadingFeed(URL& feed_url, WebFeed* feed, OpFeedListener* listener);
		LoadingFeed(URL& feed_url, OpFeed::FeedId, OpFeedListener* listener);  // Uses id to load feed object on demand (right before parsing)
		OP_STATUS Init(BOOL return_unchanged_feeds, time_t last_modified, const char* etag);
		~LoadingFeed();

		WebFeed* GetFeed();
		URL& GetURL() { return m_feed_url; }

		OP_STATUS AddListener(OpFeedListener*);
		OP_STATUS StartLoading();  ///< must delete this object yourself if method returns an error; otherwise don't delete
		void LoadingFinished();    ///< cleans up and notifies all that all loading for this feed is done (but does not notify listeners)
		void NotifyListeners(WebFeed*, OpFeed::FeedStatus);  ///< Notifies feed listeners
		void NotifyListeners(OpFeedEntry*, OpFeedEntry::EntryStatus, BOOL new_entry); ///< Notifies entry listeners
		OP_STATUS RemoveListener(OpFeedListener* listener); ///< Make sure this listener won't receive any more callbacks

		BOOL IsUpdatingFeed() { return m_feed_id != 0; }

		// Callbacks from parser:
		void OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus, BOOL new_entry);
		void ParsingDone(WebFeed*, OpFeed::FeedStatus);
		void ParsingStarted();

		/// override function from MessageObject
		void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	protected:
		OP_STATUS HandleDataLoaded();   ///< Handle Icon data loaded. Finished loading if OP_STATUS is OK, otherwise caller has to do LoadingFinished

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
		BOOL LoadIcon(WebFeed* feed);  ///< Starts loading favicon of feed, returns FALSE if icon shouldn't/doesn't need to be loaded
#endif
		void UnRegisterCallbacks();

		OpVector<OpFeedListener>	m_listeners;
		URL							m_feed_url;
		URL							m_icon_url;
		WebFeed*					m_feed;
		WebFeedParser*				m_parser;
		OpFeed::FeedId				m_feed_id;
		time_t						m_last_modified;
		OpString8					m_etag;
		BOOL						m_return_unchanged_feeds;
	};

	
public:
	friend class LoadingFeed;
	
	WebFeedLoadManager();
	~WebFeedLoadManager();
	OP_STATUS		Init();	
	
	OP_STATUS		LoadFeed(WebFeed* feed, URL& feed_url, OpFeedListener* listener, BOOL return_unchanged_feeds, time_t last_modified, const char* etag);
	/// Schedule a feed for update, calls UpdatesDone when ALL feeds scheduled have finished updating
	/// Assumes at most one update is running, WebFeedsAPI_impl should make sure of that.
	OP_STATUS		UpdateFeed(URL& feed_url, OpFeed::FeedId, OpFeedListener* listener, BOOL return_unchanged_feeds, time_t last_modified, const char* etag);
	OP_STATUS		AbortLoading(URL& feed_url);
	OP_STATUS		AbortAllFeedLoading(BOOL stop_updating_only);
	
	OP_STATUS		RemoveListener(OpFeedListener* listener); ///< Make sure this listener won't receive any more callbacks
	LoadingFeed*	IsLoadingOrQueued(URL& feed_url);

protected:

	OP_STATUS		LoadFeed(LoadingFeed*);  // Takes over ownership of parameter

	/* Callbacks by LoadingFeed */
	void			AddLoadingFeed(LoadingFeed*);  ///< this feed has started loading
	OP_STATUS 		RemoveLoadingFeed(LoadingFeed* lf);  ///< this feed has finished one way or another

	/// List of parsers currently loading. They will remove & delete themselves 
	/// after last callback, but if any are still loading when api is deleted 
 	/// then the autodelete list should take care of that.
	AutoDeleteHead	m_loading_feeds;
	AutoDeleteHead  m_waiting_feeds; ///< Used by LoadFeed, RemoveLoadingParser for not loading too many feeds at once
	UINT			m_max_loading_feeds;
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif /*WEBFEED_LOADING_FEED_H_*/
