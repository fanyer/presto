// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.


#ifndef WEBFEEDS_API_H_
#define WEBFEEDS_API_H_

#include "modules/url/url2.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

class OpFeedListener;
class OpFeed;
class OpFeedContent;
class OpFeedLinkElement;
class OpFeedEntry;
class WebFeedsAPI;
class DOM_Environment;
class ES_Object;

/**
 * \mainpage WebFeeds API
 * 
 * \author Arne Martin Guettler - arneh@opera.com
 * 
 * The external API of the WebFeeds module goes through the WebFeedsAPI
 * class.
 * 
 * USE-CASES
 * Load a feed, subscribed or not:
 *  - Call WebFeedsAPI::LoadFeed() with the URL of the feed
 *
 * Subscribe to a feed:
 *  - Call OpFeed::Subscribe() on the feed you want to subscribe
 *
 * Unsubscribe a feed:
 *  - Call OpFeed::UnSubscribe() on the feed you want to unsubscribe
 * 
 * List subscribed feeds:
 *  - call WebFeedsAPI::GetFirstFeed() to get the first feed object
 *  - if feed is not NULL write out info about it
 *  - get next feed in the list by calling OpFeed::GetNext()
 *  - repeat the two steps above until feed is NULL
 *
 * List entrys in a feed:
 *  - call OpFeed::GetFirstEntry() to get the first entry
 *  - if entry is not NULL write out info about it
 *  - get next entry in the list by calling OpFeedEntry::GetNext()
 *  - repeat the two steps above until entry is NULL
 * 
 * Example of loading a feed (without error checking for simplicity):
 * \code
 * class ExampleClass : public OpFeedListener
 * {
 * 	void Load()
 * 	{
 * 		URL feed_url = g_url_api->GetURL("http://www.example.com/feed/rss");
 * 		g_webfeeds_api->LoadFeed(feed_url, this);
 * 	}
 *  
 * 	void OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus status)
 * 	{
 * 		if (!IsFailureStatus(status))
 * 		{
 * 			URL output_url;
 *	 		feed->WriteFeed(output_url);
 * 			g_windowManager->OpenURL(output_url, URL(), FALSE, FALSE);
 * 		}
 * 		else
 * 		{
 * 			// Unthinkable Mayhem!
 * 		}
 * 	}
 * 
 *	void OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus status)
 * 	{
 * 	}
 * };
 * \endcode
 * 
 * \see WebFeedsAPI 
 * \see WebFeedListener */

/**
 * \brief Interface to WebFeeds module
 * 
 * You will normally access this API through the g_webfeeds_api pointer.
 * 
 * You can get a WebFeed object by calling LoadFeed to start loading a feed, and 
 * this will call the WebFeedListener when the feed is ready (or has failed to load/parse).
 * Or if it's a subscribed feed you can do GetFeedById() or GetFeedByUrl().
 */
class WebFeedsAPI
{
public:
	virtual ~WebFeedsAPI() {}

	/** Load a WebFeed from an URL
	 * 
	 * If used with the last_modified or etag parameters then the listener will be called
	 * with STATUS_NOT_MODIFIED if the feed is unchanged since from what it was when
	 * does headers were read (with OpFeed::GetHttpLastModified() and
	 * OpFeed::GetHttpETag()).
	 * 
	 * \param[in]  feed_url URL to a RSS or Atom feed.
	 * \param[in]  listener This listener will be signaled when the feed is ready, or 
	 *        when loading/parsing fails. Note that the listener might be called 
	 *        immediately (i.e. before this call has returned) if the feed is already
	 *        loaded and minimum time since last reload has not passed. In that case
	 *        the status of the listener will be STATUS_REFRESH_POSTPONED.
	 * \param[in]  return_unchanged_feed if FALSE the feed parameter to the listener
	 *        will be NULL if the status is STATUS_NOT_MODIFIED or STATUS_REFRESH_POSTPONED.
	 *        Saves having to load it from disk if it is not already in memory
	 * \param[in]  last_modified LastModified feed must be newer than to update.
	 *        Or 0 if you don't care.
	 * \param[in]  etag ETAG feed must be different from to update.
	 *        Or NULL if you don't care.
	 * \return OpStatus::OK if loading has started, OpStatus::ERR_NO_MEMORY for 
	 * 		  OOM, or OpStatus::ERR if it failed to start loading for any other 
	 *        reason
	 */
	virtual OP_STATUS		LoadFeed(URL& feed_url, OpFeedListener* listener, BOOL return_unchanged_feed = TRUE, time_t last_modified = 0, const char* etag = NULL) = 0;

	/** Forces an update of the subscribed feeds.
	 * 	The listener will be called for feeds (but check status if it was 
	 *  actually updated, and it may not always be called for all feeds (if UpdateFeeds
	 *  is called again before a previous update has finished, the listener for
	 *  the second update won't get called for feeds already updated by previous update)).
	 * 
	 *  Feeds will be updated in an arbitrary order, so don't rely on feeds
	 *  being updated in any specific order.
	 * 
	 *  When all feeds have been (tried to) updated 
	 *  OnUpdateFinished in the listener will be called 
	 * 
	 * \param return_unchanged_feeds If TRUE the listener will return a feed
	 *    object also for feeds which have not been updated. If FALSE the listener
	 *    will be called with a NULL feed if the feed was not updated. This is preferable
	 *    if you don't need anything from the feed unless it is changed, as it doesn't
	 *    require loading the feed into memory.
	 * 
	 * \param force_update Update feeds even if update interval has not yet elapsed.
	 *    (but feeds will not be updated if the *minimum* interval has not elapsed,
	 *    even when forced). Forcing is most useful for manual updates, while not
	 *    forcing for automatic updates.
	 * */
	virtual OP_STATUS		UpdateFeeds(OpFeedListener *listener, BOOL return_unchanged_feeds = FALSE, BOOL force_update=TRUE) = 0;

	/** Aborts loading of the feed with the selected URL
	 *  If the URL is not currently loading or waiting to be loaded this 
	 *  function has no effect.
	 *  All listeners to the aborted feeds will get a callback with a NULL
	 *  feed object and a status of OpFeed::STATUS_ABORTED
	 * 
	 *  \param feed_url  The URL from which the feed that is to be aborted is loaded from
	 * */
	virtual OP_STATUS		AbortLoading(URL& feed_url) = 0;
	
	/** Aborts loading of all feeds that are currently loading or are waiting
	 *  to start loading.
	 *  All listeners to the aborted feeds will get a callback with a NULL
	 *  feed object and a status of OpFeed::STATUS_ABORTED
	 * 
	 *  \param updating_feeds_only If TRUE then only feeds being loaded because
	 *         they are being updated (either by automatic update or because
	 *         of call to UpdateFeeds) will be aborted. I.e. not feeds which have
	 *         been specifically loaded with LoadFeed. */
	virtual OP_STATUS		AbortAllFeedLoading(BOOL updating_feeds_only=FALSE) = 0;
	
	/** Turns on or off automatic updates of feeds.
	 *  When turned on feeds will be updated at regular intervals
	 *  (defined by the UpdateInterval setting in OpFeed). */
#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	virtual void			DoAutomaticUpdates(BOOL) = 0;
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT
	
	/** Each omniscient listener gets notified about ALL listener events.
	 *  Probably most useful for the UI, if it needs to do some UI updates
	 *  for everything which happens.
	 * 
	 *  Be careful about exposing this functionallity, particulary to
	 *  javascript, as it can expose privacy information */
	virtual OP_STATUS		AddOmniscientListener(OpFeedListener*) = 0;

	/* Remove a listener (omniscient or other). The listener will not receive
	 * any callbacks after this (unless it adds itself as a listener again
	 * in some way). */
	virtual OP_STATUS		RemoveListener(OpFeedListener* listener) = 0;
	
	/// Returns the feed that has been fetched by the feed_url.
	/// Remember to do IncRef() on object if you plan to keep using it.
	virtual OpFeed*			GetFeedByUrl(const URL& feed_url) = 0;

	/// Returns the feed with the id.
	/// Remember to do IncRef() on object if you plan to keep using it.
	virtual OpFeed*			GetFeedById(UINT id) = 0;

	/** Returns an array containing all feed-ids we know about.
	 *
	 * \param numfeeds		  returns number of feed IDs in the array
	 * \param subscribed_only If true only returns ids for feeds which are
	 *        subscribed.
	 * 
	 * Can be used to iterate through all feeds, by calling GetFeedById
	 * on each one. Remember to do IncRef on objects returned by GetFeedById
	 * if you want to keep a reference to them.
	 * 
	 * An id of 0 will signify that the end of the array is reached.
	 * The array is allocated and must be freed with delete [].
	 * Returns NULL on OOM.
	 *
	 * The ids returned are valid upon return, but other calls to
	 * the module might change feeds, and so the id list might not
	 * be complete or valid any more. */
	virtual UINT* GetAllFeedIds(UINT& numfeeds, BOOL subscribed_only=TRUE) = 0;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	enum FeedIdURLType
	{
		FeedIdURL,
		EntryIdURL,
		SubscriptionListURL,
		FeedSettingsURL,
		GlobalSettingsURL
	};
	/** Get feed and entry from a a opera:feed-id URL
	 *  \param[in]  feed_id_url the feed-id url
	 *  \param[out] feed        the OpFeed object the URL corresponds to
	 *  \param[out] entry       the OpFeedEntry object the URL corresponds to
	 *  \param[out] type        the type of feed-id URL this is
	 *  \return                 OpStatus::OK if it was a valid feed-id URL, 
	 * 							OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR else */
	virtual OP_STATUS		GetFeedFromIdURL(const char* feed_id_url, OpFeed*& feed, OpFeedEntry*& entry, FeedIdURLType& type) = 0;  // last three are out parameters with result

	enum OpFeedWriteStatus
	{
		Written,
		Queued,
		IllegalId,
		UnknownId,
		WriteError
	};
	/** Write a to a URL by its id.
	 *  Used by URL when accessing opera:feedid/xxxxxxxx-xxxxxxxx URLs
	 *  \param[in] feed_id a string on the form "opera:feedid/xxxxxxxx" or 
	 *                     "opera:feedid/xxxxxxxx-xxxxxxxx" (x being hex digits)
	 *  \param[out] out_url the URL where the feed will be written
	 *  \return Written if the URL was written successfully by this call
	 *          Queued if the URL needs more data until it is finished writing
	 *          IllegalId if the id is of illegal format
	 *          UnknownId if the id is legal but unknown */
	virtual OpFeedWriteStatus	WriteByFeedId(const char* feed_id, URL& out_url) = 0;

	/** Generate a page of subscribed feeds, including some status information
	 *  and checkboxes for running commands on selected feeds (see next functions
	 *  below).
	 *  This function is fairly redundant, as you can get the same page by
	 *  reguesting the URL opera:feed-id/all-subscribed.
	 * 
	 *  \param[out]  out_url The url which contains the generated page.
	 *        (i.e. opera:feed-id/all-subscribed).  The URL object is mostly 
	 *        here for your convenience.
	 *        If you supply a URL with a different address that will be used
	 *        for the generated page, but other parts of the code assumes
	 *        the address is the default, so don't do that. */
	virtual OP_STATUS		WriteSubscriptionList(URL& out_url) = 0;

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
	/** Generate a page for changing the global feed settings, and default
	 *  settings for feeds.
	 * 
	 *  \param out_url[out]   The URL where the settings page has been generated.
	 *                        Can be opened to display the page. */
	virtual OP_STATUS		WriteGlobalSettingsPage(URL& out_url) = 0;
	
	/** Saves the settings from the global or a feed settings page
	 *  (generated with WriteGlobalSettingsPage or OpFeed::WriteSettingsPage
	 *   or simply opened through it's URL).
	 * 
	 *  \param frm_doc	The frames document of the settings page. */
	virtual OP_STATUS 		SaveSettingsFromPage(FramesDocument* frm_doc) = 0;

	/** Returns true if the FramesDocument is the subscription list, and
	 *  at least one checkbox is checked.
	 *  \param[in] FramesDocument The document which should contain the subscription
	 *             list (i.e. opera:feed-id/all-subscribed).
	 *  \param[out] contains_any_checked TRUE if the document has any checked boxes */
	virtual OP_STATUS		ContainsAnyCheckedFeeds(FramesDocument*, BOOL& contains_any_checked) = 0;
	
	enum SubscriptionListCommand  ///< Command to run in RunCommandOnChecked
	{
		UnsubscribeMarked,        ///< Unsubscribed all selected feeds
		MarkAllRead              ///< Mark all entries in selected feeds as read
	};
	/** Runs the selected command on all feeds checked in the form on 
	 *  opera:feed-id/all-subscribed.
	 * 
	 *  \param[in] FramesDocument the document which contains the 
	 *             opera:feed-id/all-subscribed open and checked.
	 *  \param[in] SubscriptionListCommand Command to run on selected feeds. */
	virtual OP_STATUS		RunCommandOnChecked(FramesDocument*, SubscriptionListCommand) = 0;  // TODO: select which command to run
#endif // TWEAK_WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

	/// Returns the maximum size on disk allowed for all the feeds
	virtual UINT			GetMaxSize() = 0;
	/** The size the number of bytes of *disk space* the WebFeeds modules can use
	 *  Note that this is not a hard limit, and the module might at times
	 *  use a little more than this.  But averaged over time it should not
	 *  use more than this.
	 * 
	 *  Also the module might not be able to keep to this limit if the user 
	 *  subscribes to excessive number of feeds, or marks too many entries with
	 *  the KEEP flag.
	 * 
	 *  It does *not* limit the amount of *memory* used by the module, see the tweaks
	 *  TWEAK_WEBFEEDS_MAX_FEEDS_IN_MEMORY for tuning memory use */
	virtual void			SetMaxSize(UINT size) = 0;

	/// Get the default max age value used if not explicitly specified for a feed. Time in minutes
	virtual UINT			GetDefMaxAge() = 0;
	/// Sets the default max age value used if not explicitly specified for a feed. Time in minutes
	virtual void			SetDefMaxAge(UINT age) = 0;

	/// Get the default value for max number of entrys used if not
	/// explicitly specified for a feed
	virtual UINT			GetDefMaxEntries() = 0;
	/// Sets the default value for max number of entrys used if not
	/// explicitly specified for a feed
	virtual void			SetDefMaxEntries(UINT entries) = 0;

	/// Get the default update interval used if not explicitly specified for a feed
	virtual UINT			GetDefUpdateInterval() = 0;
	/// Sets the default update interval used if not explicitly specified for a feed
	virtual void			SetDefUpdateInterval(UINT interval) = 0;

	/// Get the default value for showing images used if not
	/// explicitly specified for a feed
	virtual BOOL			GetDefShowImages() = 0;
	/// Sets the default value for showing images used if not
	/// explicitly specified for a feed
	virtual void			SetDefShowImages(BOOL show) = 0;

	/** Deletes entries above the maximum allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself keeps the number of entries
	 *  below maximum when doing updates.
	 *  Note that this is a fairly heavy operation which requires the 
	 *  module to load all feeds from disk.  Which is why it's not 
	 *  done by default when changing MaxEntries. */
	virtual OP_STATUS		CheckAllFeedsForNumEntries() = 0;

	/** Deletes entries older than allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself expires entries
	 *  when doing updates.
	 *  Note that this is a fairly heavy operation which requires the 
	 *  module to load all feeds from disk.  Which is why it's not 
	 *  done by default when changing MaxAge. */
	virtual OP_STATUS	 	CheckAllFeedsForExpiredEntries() = 0;

#ifdef WEBFEEDS_EXTERNAL_READERS
	/** Check the number of external feed readers known to the webfeeds module
	 *
	 * \return Number of feed readers known */
	virtual unsigned		GetExternalReaderCount() const = 0;

	/** Get the unique, persistent ID of an external feed reader
	 *
	 * \param index[in] Index of feed reader to check, must be less than GetExternalReaderCount()
	 * \return Unique, persistent ID of feed reader, can be used in functions below */
	virtual unsigned		GetExternalReaderIdByIndex(unsigned index) const = 0;

	/** Get the human-readable name of an external feed reader
	 *
	 * \param id[in] ID of feed reader to get name for
	 * \param name[out] Human-readable name, for example 'Google Reader' */
	virtual OP_STATUS		GetExternalReaderName(unsigned id, OpString& name) const = 0;

	/** Get a URL to use to subscribe to a feed using an external feed reader
	 *
	 * \param id[in] ID of feed reader with which to subscribe
	 * \param feed_url[in] Feed to subscribe to
	 * \param target_url[out] URL that can be used to subscribe to feed_url in external feed reader */
	virtual OP_STATUS		GetExternalReaderTargetURL(unsigned id, const URL& feed_url, URL& target_url) const = 0;

#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	/** Check if an externa feed reader can be modified (and deleted) by user
	 *
	 * \param id[in] ID of feed reader to add
	 * \param can_edit[out] TRUE if the reader can be modified, otherwise FALSE */
	virtual OP_STATUS 		GetExternalReaderCanEdit(unsigned id, BOOL& can_edit) const = 0;

	/** Return an identifier that is not in use. Can be used for \ref AddExternalReader */
	virtual unsigned		GetExternalReaderFreeId() const = 0;

	/** Add external feed reader
	 *
	 * \param id[in] ID of feed reader to add
	 * \param feed_url[in] The url of an external feed reader
	 * \param feed_name[in] The name of an external feed reader */
	virtual OP_STATUS		AddExternalReader(unsigned id, const uni_char *feed_url, const uni_char *feed_name) const = 0;

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
	virtual BOOL			HasExternalReader(const uni_char *feed_url, unsigned *id = NULL) const = 0;

	/** Deletes external feed reader
	 *
	 * \param id[in] ID of feed reader to delete */
	virtual OP_STATUS		DeleteExternalReader(unsigned id) const = 0;
#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
#endif // WEBFEEDS_EXTERNAL_READERS

	virtual OP_STATUS Init() = 0; ///< Init is called by WebfeedsModule, no one else should call this
};



class OpFeedEntry
{
public:
	typedef UINT EntryId;

	/// TODO: decide all the different statuses we need and define and
	/// document them well.
	enum ReadStatus
	{
		STATUS_UNREAD,
		STATUS_READ,
		STATUS_DELETED
	};
	
	enum EntryStatus
	{
		STATUS_OK,
		STATUS_ERROR
	};

	/// Call this to make sure the OpFeedEntry-object isn't deleted.
	/// Call DecRef when you're done with it
	virtual OpFeedEntry*		IncRef() = 0;
	/// Call this when you're done with a OpFeedEntry-object
	/// (but only if you did IncRef first!)
	virtual void				DecRef() = 0;

	virtual ReadStatus			GetReadStatus() = 0;
	virtual void 				SetReadStatus(ReadStatus status) = 0;

	/// Returns the previous entry in the feed the entry belongs to, NULL if first
	virtual OpFeedEntry*		GetPrevious(BOOL unread_only=FALSE) = 0;
	/// Returns the next entry in the feed the entry belongs to, NULL if last
	virtual OpFeedEntry*		GetNext(BOOL unread_only=FALSE) = 0;

	/// Returns the feed that this entry belongs to.
	virtual OpFeed*				GetFeed() = 0;

	/// Returns the unique id of this entry
	virtual EntryId				GetId() = 0;
	/// Returns the globally unique identifier string specified by the feed
	virtual const uni_char* 	GetGuid() = 0;

	/// Returns TRUE if the entry has been marked by the user to not be thrown
	/// out when cleaning feeds.
	virtual BOOL				GetKeep() = 0;
	/// Sets whether the entry should be thrown out when cleaning feeds.
	///\param[in] keep TRUE if the entry should not be thrown out
	virtual void				SetKeep(BOOL keep) = 0;

	/// Returns the title of the entry
	virtual OpFeedContent*		GetTitle() = 0;
	/// Returns the author of the feed
	virtual const uni_char*		GetAuthor() = 0;
	/// Returns the email address of the author of the feed
	virtual const uni_char*		GetAuthorEmail() = 0;
	/// Returns the content of the entry as HTML
	virtual OpFeedContent*		GetContent() = 0;
	/** Returns the value of the property if it exists in the entry, otherwise NULL
	 *  \param[in] property name of the property to get the value of */
	virtual const uni_char*		GetProperty(const uni_char* property) = 0;
	/// Date feed was published, or last updated if it has later been updated (returns time in format used by stdlib/util/opdate.h)
	virtual double				GetPublicationDate() = 0;
	
	/** Number of links
	 * 	\return number of links, can be used with GetLink */
	virtual unsigned LinkCount() const = 0;

	/** Return link at index
	 *  index should be less then LinkCount() */
	virtual OpFeedLinkElement* GetLink(UINT32 index) const = 0;

	/// URL to webpage where this entry is posted. Will return empty URL if no such URL has been set.
	virtual URL					GetPrimaryLink() = 0;
	/// Returns the DOM object if referenced by ECMAScript, otherwise NULL
	virtual ES_Object*			GetDOMObject(DOM_Environment*) = 0;
	/// Used to set the DOM object when referenced by ECMAScript
	virtual void				SetDOMObject(ES_Object* dom_entry, DOM_Environment*) = 0;

	// TODO: add all the properties we support.

#ifdef WEBFEEDS_DISPLAY_SUPPORT
# ifdef OLD_FEED_DISPLAY
	/** Write Entry as (X)HTML to URL, which then can be displayed by OpenURL
	 *  \param[out] write_in the url which will contain the output.
	 *  \param[in] complete_document if true then a complete HTML document will
	 *            be generated. Use this if you want to open the URL directly,
	 *            or set to FALSE if you want to embed the generated HTML in some 
	 *            other document */
	virtual OP_STATUS WriteEntry(URL& write_in, BOOL complete_document=TRUE, BOOL mark_read=TRUE) = 0;
# endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

protected:
	/// Don't delete OpFeedEntry objects, use DecRef()
	virtual 					~OpFeedEntry() {}
};


class OpFeedContent
{
protected:
	virtual 				~OpFeedContent() {}  ///< Use DecRef when done with object, don't delete
public:
	/// Increment the reference counter
	virtual OpFeedContent*	IncRef() = 0;
	/// Decrement the reference counter
	virtual void			DecRef() = 0;

	/// Returns the DOM object if referenced by ECMAScript, otherwise NULL
	virtual ES_Object*	GetDOMObject(DOM_Environment*) = 0;
	/// Used to set the DOM object when referenced by ECMAScript
	virtual void			SetDOMObject(ES_Object* dom_content, DOM_Environment*) = 0;

	/// Returns the content as a uni_char string. Only a valid return if IsBinary() returns FALSE
	virtual	const uni_char* Data() const = 0;
	/// Returns content as a buffer and length. Useful when content is binary. Use MIME_Type to figure out what to do with data
	virtual void			Data(const unsigned char*& buffer, UINT& buffer_length) const = 0;

	/// Returns MIME type of content as reported by feed
	virtual const uni_char*	MIME_Type() const = 0;
	/// Returns TRUE if MIME type is text/plain
	virtual BOOL			IsPlainText() const = 0;
	/// Returns TRUE if MIME type is (X)HTML
	virtual BOOL			IsMarkup() const = 0;
	/// Returns TRUE if MIME type is neither plain text or markup text.
	/// Check MIME_Type to figure out what it really is.
	virtual BOOL			IsBinary() const = 0;
	
	/// Returns the base URI used for resolving URLs in the content,
	/// or NULL if no base URI has been defined.
	virtual const uni_char*	GetBaseURI() const = 0;
};


class OpFeedLinkElement
{
public:
	// Link relationship.
	enum LinkRel
	{
		Alternate,
		Related,
		Via,
		Enclosure
	};

	OpFeedLinkElement() {}
	virtual ~OpFeedLinkElement() {}

	virtual const OpStringC& Title() = 0;
	virtual const OpStringC& Type() = 0;
	virtual URL& URI() = 0;
	virtual LinkRel Relationship() = 0;

	virtual BOOL HasTitle() = 0;
	virtual BOOL HasType() = 0;
	virtual BOOL HasURI() = 0;

private:
	// No copy or assignment.
	OpFeedLinkElement(const OpFeedLinkElement& other);
	OpFeedLinkElement& operator=(const OpFeedLinkElement& other);
};

class OpFeed
{
public:
	typedef UINT FeedId;
	enum FeedStatus
	{
		STATUS_OK,        ///< Feed loaded ok
		STATUS_ABORTED,	  ///< Feed loading was aborted (usually by user)
		STATUS_REFRESH_POSTPONED, ///< Not loaded, due to too little time passed since last 
		                          ///< update. Feed object is still valid, but will only contain older data
		STATUS_NOT_MODIFIED,      ///< Server returned not modified, feed unchanged since last update
		STATUS_OOM,       ///< OOM during loading.  When memory has been freed up, you can retry loading.
		                  ///< Opera may still be OOM if a callback is called with this status, so don't allocate any more memory.
		STATUS_SERVER_TIMEOUT,   ///< No answer from server
		STATUS_LOADING_ERROR,    ///< Loading failed
		STATUS_PARSING_ERROR     ///< Feed loaded, but contained uncorrectable errors (usually misformed XML)
	};

	/// Returns TRUE if the parameter is a FeedStatus which is some sort of failure
	static BOOL IsFailureStatus(FeedStatus);

	/// Call this to make sure the OpFeedEntry-object isn't deleted.
	/// Call DecRef when you're done with it
	virtual OpFeed*			IncRef() = 0;
	/// Call this when you're done with a OpFeedEntry-object
	/// (but only if you did IncRef first!)
	virtual void			DecRef() = 0;

	virtual FeedStatus		GetStatus() = 0;
	
	/// Mark all entries with the same read status
	void MarkAllEntries(OpFeedEntry::ReadStatus status);

	/// Adds the feed from the subscription list
	virtual void			Subscribe() = 0;
	/// Removes the feed from the subscription list
	virtual void			UnSubscribe() = 0;
    /// Returns TRUE if the feed is subscribed
    virtual BOOL            IsSubscribed() = 0;
	
	/// Updates feed by fetching it from feed URL
	virtual OP_STATUS		Update(OpFeedListener*) = 0;
	
	/// Last time this feed was updated from source (returns time in format used by stdlib/util/opdate.h)
	virtual double			LastUpdated() = 0;
	
	/// Returns the title of the feed as parsed from the feed itself
	virtual OpFeedContent*	GetTitle() = 0;
	
	/// Returns	the title of the feed.  If the title has been changed by the user the user defined title will be returned
	virtual OP_STATUS		GetUserDefinedTitle(OpString& title) = 0;
	/// Sets the user defined title
	virtual OP_STATUS		SetUserDefinedTitle(const uni_char* new_title) = 0;

	/// Returns the author of the feed
	virtual const uni_char*	GetAuthor() = 0;
	/// Returns the IRI string of the logo image of the feed
	virtual const uni_char*	GetLogo() = 0;
	/// Returns the IRI string of the favicon of the feed
	virtual const uni_char*	GetIcon() = 0;
	/// Returns the value of the property if it exists in the entry, otherwise NULL
	virtual const uni_char*	GetProperty(const uni_char* property) = 0;
	
	/// Returns the unique id of this entry
	virtual FeedId			GetId() = 0;

	/// Returns the available first entry (i.e. the newest entry)
	virtual OpFeedEntry*	GetFirstEntry() = 0;
	/// Returns the available last entry (i.e. the oldest we still have)
	virtual OpFeedEntry*	GetLastEntry() = 0;
	/// Get an entry with a specified id
	virtual OpFeedEntry*	GetEntryById(FeedId id) = 0;

	/// Returns URL the feed is downloaded from
	virtual URL& GetURL() = 0;
	/// Returns the feed-id URL used for displaying the feed
	virtual OP_STATUS		GetFeedIdURL(OpString&) = 0;

	/// Returns time to live in minutes
	virtual UINT			GetMaxAge() = 0;
	/// Set the time to live for entries in the feed
	virtual void			SetMaxAge(UINT age) = 0;

	/// Returns the maximum number of allowed messages in the feed
	virtual UINT			GetMaxEntries() = 0;
	/// Sets the maximum number of allowed messages in the feed
	virtual void			SetMaxEntries(UINT entrys) = 0;

	/// Returns the update interval for the feed in minutes.
	virtual UINT			GetUpdateInterval() = 0;
	/// Returns the minimum update interval (as specified by the server) in minutes.
	virtual UINT			GetMinUpdateInterval() = 0;
	/// Sets the update interval for the feed in minutes.
	virtual void			SetUpdateInterval(UINT interval) = 0;

	/// Returns whether images should be shown when viewing entrys from the feed.
	virtual BOOL			GetShowImages() = 0;
	/// Sets whether images should be shown when viewing entrys from the feed.
	virtual void			SetShowImages(BOOL show) = 0;

	/** Returns TRUE if this feed will prefetch the linked article each time it sees a new entry
	 *
	 *  The linked article is the one referenced either with a <link rel=alternate ..> element
	 *  or a guid-element with the permaLink attribute set to true */
	virtual BOOL			PrefetchPrimaryWhenNewEntries() = 0;
	/**
	 *  @param prefetch      Enable or disable prefetching of linked article for new entries
	 *  @param prefetch_now  If enabeling prefetching and this parameter is TRUE 
	 *                       it will start a prefetch of all current entries in feed */
	virtual void			SetPrefetchPrimaryWhenNewEntries(BOOL prefetch, BOOL prefetch_now = FALSE) = 0;

	/// Returns the total number of messages in the feed
	virtual UINT			GetTotalCount() = 0;
	/// Returns the number of unread messages in the feed
	virtual UINT			GetUnreadCount() = 0;

	/// Returns the storage space used by the feed
	virtual UINT			GetSizeUsed() = 0;

	/// Returns the HTTP LastModified header the feed returned on the last update
	/// If the feed had no such header NULL is returned
	virtual time_t			GetHttpLastModified() = 0;

	/// Returns the HTTP ETag header the feed returned on the last update
	/// If the feed had no such header NULL is returned
	virtual const char*		GetHttpETag() = 0;

	/// Returns the DOM object if referenced by ECMAScript, otherwise NULL
	virtual ES_Object* 		GetDOMObject(DOM_Environment* environment) = 0;

	/// Used to set the DOM object when referenced by ECMAScript
	virtual void			SetDOMObject(ES_Object *dom_obj, DOM_Environment* environment) = 0;
	
	/** Deletes entries above the maximum allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself keeps the number of entries
	 *  below maximum when doing updates
	 *  @param */
	virtual void			CheckFeedForNumEntries() = 0;
	
	/** Deletes entries older than allowed for each feed
	 *  Only necessary to call if you've changed the maximum allowed
	 *  to something less and you want the change to take effect
	 *  immediately.  The module itself expires entries
	 *  when doing updates */
	virtual void		 	CheckFeedForExpiredEntries() = 0;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	/** Write Feed as XML to URL, which then can be displayed by OpenURL
	 *  Will generate an XML page with links to all entries in feed
	 *		\param[out] write_in The URL where the output is generated. Load this to display titles
	 * 		\param[in]  complete_document if TRUE will generate complete document with all
	 *			necessary headers. Set to false if you want to embed the output
	 * 			into another document. */
	virtual OP_STATUS WriteFeed(URL& write_in, BOOL complete_document=TRUE, BOOL write_subscribe_header=FALSE) = 0;
#endif // WEBFEEDS_DISPLAY_SUPPORT
protected:
	/// Don't delete OpFeed objects, use DecRef
	virtual ~OpFeed() {}
};


class OpFeedListener
{
public:
	virtual 			~OpFeedListener() {}

	/** Called when all feeds have finished updating.
	 *  The listener will not be used after this call and can be safely 
	 *  deleted. */
	virtual void 		OnUpdateFinished() = 0;

	/** Called when one feed has finished loading. Override if you want to
	 *  be notified for each feed, otherwise just override OnUpdateFinished
	 *  and wait for all feeds to finish loading.
	 *  If the status != STATUS_OK or STATUS_REFRESH_POSTPONED the entry might be invalid
	 *  or incomplete.
	 *
	 *  The feed pointer might be NULL which would mean that the status is just
	 *  informative and applies to some feed but details are not available at the
	 *  moment.
	 *
	 *  If you want to keep using the feed object after OnFeedLoaded returns you must
	 *  call IncRef on it, and then DecRef when you're done with it.
	 *  If you loaded only one feed (i.e. not updated all subscriptions)
	 *  then the listener won't be used after this call and can be safely deleted. */
	virtual void		OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus) = 0;

	/** Called when one entry has finished loading. Override if you want to
	 *  be notified for each entry, otherwise just override OnFeedLoaded
	 *  and wait for entire feed to finish loading.
	 * 
	 *  This will be called for every entry in the feed file, even if the
	 *  feed has been loaded previously.  If you only want to be notified of
	 *  new entries since last load/update, use the method below.
	 * 
	 *  If the status != ENTRY_STATUS_OK the entry might be invalid or incomplete
	 *  If you want to keep using the entry object after OnEntryLoaded returns you must
	 *  call IncRef on it, and then DecRef when you're done with it */
	virtual void		OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus) = 0;

	/** Called when a new entry has finished loading. Override if you want to
	 *  be notified for each entry, otherwise just override OnFeedLoaded
	 *  and wait for entire feed to finish loading.
	 * 
	 *  This one is only called for new entries, which have not been in the feed
	 *  on previous loads.
	 * 
	 *  If the status != ENTRY_STATUS_OK the entry might be invalid or incomplete
	 *  If you want to keep using this object after OnEntryLoaded returns you must
	 *  call IncRef on it, and then DecRef when you're done with it */
	virtual void		OnNewEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus) = 0;
};


#endif // WEBFEEDS_BACKEND_SUPPORT

#endif /*WEBFEEDS_API_H_*/
