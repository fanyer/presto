// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef _WEBFEED_H_
#define _WEBFEED_H_

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/webfeeds_api.h"
#include "modules/webfeeds/src/webfeedentry.h"
#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeedutil.h"

#include "modules/url/url2.h"

#ifndef UINT_MAX
# define UINT_MAX ((UINT)~0L)
#endif

class WriteBuffer;

/**
 * \brief Class for representing a parsed WebFeed.
 * 
 * The most interesting methods to use externally would be:
 * \li Title, Author, FeedLink and TagLink to extract those pieces of information
 *   from the feed.
 * \li GetFirstEntry to iterate through the entries of the feed.
 * \li WriteFeed to genereate a XML/HTML page with a title and links
 *   to all the entries in the feed.
 */

class WebFeed : public OpFeed, public Link
{
public:
	WebFeed();
	~WebFeed();
	OP_STATUS		Init(WebFeedsAPI_impl::WebFeedStub* stub);
	OP_STATUS		Init();

	OpFeed*			IncRef();
	void			DecRef();
	UINT			RefCount() { return m_reference_count; }

	virtual ES_Object* GetDOMObject(DOM_Environment* environment);
	virtual void	SetDOMObject(ES_Object *dom_obj, DOM_Environment* environment);

	/** This will mark this feed for removal, which means it will be purged
	 *  from memory when there are no references left to it (i.e. when all
	 *  have done DecRef()).
	 *  This might delete the object imediatly (unless the caller has a reference)
	 *  so the object should be considered deleted memory after a call to MarkForRemoval.
	 *  If the Feed object is needed again, use any of the methods in WebFeedsAPI which
	 *  returns a feed. */
	void			MarkForRemoval(BOOL remove) { m_marked_for_removal = remove; }
	
	WebFeedsAPI_impl::WebFeedStub* GetStub() { return m_stub; }
	void			SetStub(WebFeedsAPI_impl::WebFeedStub* stub) { m_stub = stub; }
	
	FeedStatus		GetStatus();
	void			SetStatus(OpFeed::FeedStatus);

	void			Subscribe();
	void			UnSubscribe();
	BOOL			IsSubscribed();
	OP_STATUS		Update(OpFeedListener*);

	OpFeed*			GetPrevious();
	OpFeed*			GetNext();
	OpFeedEntry*	GetFirstEntry();
	OpFeedEntry*	GetLastEntry();
	OpFeedEntry*	GetEntryById(UINT id);

	FeedId			GetId();
	void			SetId(FeedId);

	OpFeedEntry::EntryId
					GetNextFreeEntryId();
	void			SetNextFreeEntryId(OpFeedEntry::EntryId id);

	UINT			GetMaxAge();
	void			SetMaxAge(UINT age);

	UINT			GetMaxEntries();
	void			SetMaxEntries(UINT items);

	UINT			GetUpdateInterval();
	void			SetUpdateInterval(UINT interval);
	UINT			GetMinUpdateInterval();
	void			SetMinUpdateInterval(UINT interval);

	BOOL			GetShowImages();
	void			SetShowImages(BOOL show);
	BOOL			GetShowPermalink();
	void			SetShowPermalink(BOOL show);

	BOOL			PrefetchPrimaryWhenNewEntries() { return m_prefetch_entries; }
	void			SetPrefetchPrimaryWhenNewEntries(BOOL prefetch, BOOL prefetch_now = FALSE);

	UINT			GetTotalCount();
	UINT			GetUnreadCount();

	UINT			GetSizeUsed();

	URL&			GetURL();
	void			SetURL(URL& feed_url);
	OP_STATUS		GetFeedIdURL(OpString&);
	
	double			LastUpdated();
	void			SetLastUpdated(double opdate);

	/** \return The title of the feed */
	OpFeedContent*			GetTitle();
	void					SetParsedTitle(WebFeedContentElement* title);

	OP_STATUS				GetUserDefinedTitle(OpString& title);
	OP_STATUS				SetUserDefinedTitle(const uni_char* new_title);

	/** \return The link of the feed */
	WebFeedLinkElement*		FeedLink();
	/** \return The author of the feed */
	const uni_char*			GetAuthor();
	WebFeedPersonElement*	AuthorElement();
	/** \return The Tagline of the feed */
	WebFeedContentElement*	Tagline();
	/// Returns the IRI string of the logo image of the feed
	const uni_char*			GetLogo();
	/// Returns the IRI string of the favicon of the feed
	const uni_char*			GetIcon();
	OP_STATUS				SetIcon(const uni_char* icon_url);
	const uni_char*			GetProperty(const uni_char* property);
	
	/** Add feed entry to feed. Called by the parser.
	 *  	\param entry to add. Should not be NULL. Does NOT transfer ownership, caller must still do DecRef when done with pointer
	 *		\param id id of feed. Uses next free if 0.
	 * 		\return TRUE if a new entry or changed entry was added, FALSE otherwise */
	virtual BOOL	AddEntryL(WebFeedEntry* entry, OpFeedEntry::EntryId id = 0, WebFeedEntry** = NULL);

	/** Call this when it's time to actually remove an entry (i.e. no one 
	 * 	references it yet.  For internal use, if you just want to remove an
	 *  entry call MarkForRemoval() on the entry object and it will be removed
	 * 	when no one needs it anymore. */
	void 			RemoveEntry(WebFeedEntry* entry);
	
	void 			RemoveEntryFromCount(WebFeedEntry* entry);

	void			CheckFeedForNumEntries() { CheckFeedForNumEntries(0); }
	void			CheckFeedForNumEntries(UINT free_entries_to_reserve);
	void		 	CheckFeedForExpiredEntries();
	
#ifdef WEBFEEDS_DISPLAY_SUPPORT
	/// Write Feed as XML to URL, which then can be displayed by OpenURL
	/// Will generate an XML page with links to all items in feed
	OP_STATUS		WriteFeed(URL& write_in, BOOL complete_document=TRUE, BOOL write_subscribe_header=FALSE);
	void			WriteFeedL(URL& write_in, BOOL complete_document=TRUE, BOOL write_subscribe_header=FALSE);
#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
	void			WriteSettingsPageL(URL& write_in);
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	void			SaveOverrideSettingsL(WriteBuffer &buf);
	void			SaveEntriesL(WriteBuffer &buf, int space_available = 0);
	static void		WriteSettingNum(WriteBuffer &buf, const char *name, UINT num);
	static void		WriteSettingBool(WriteBuffer &buf, const char *name, BOOL value);
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

	time_t			GetHttpLastModified() { return m_stub->GetHttpLastModified(); }
	void			SetHttpLastModified(time_t last_modified) { m_stub->SetHttpLastModified(last_modified); }
	const char*		GetHttpETag() { return m_stub->GetHttpETag(); }
	OP_STATUS		SetHttpETag(const char* etag) { return m_stub->SetHttpETag(etag); }

	void MarkAllEntries(OpFeedEntry::ReadStatus status);
	// These two called by WebFeedEntry whenever its read status changes
	void MarkOneEntryUnread();
	void MarkOneEntryNotUnread();
protected:

	WebFeedsAPI_impl::WebFeedStub* m_stub;
	UINT		m_reference_count;
	BOOL		m_marked_for_removal;

	EnvToESObjectHash  m_dom_objects;

	OpString	m_icon;		   ///< URL of feed's favicon (either DATA URL or remote URL)

	WebFeedContentElement*	m_title;
	WebFeedLinkElement		m_link;
	WebFeedPersonElement*	m_author;
	WebFeedContentElement*	m_tagline;

	FeedStatus	m_status;
	Head		m_entries;           ///< Entries sorted by date, newest first
	OpFeedEntry::EntryId	m_next_free_entry_id;
	UINT		m_max_age;           ///< Maximum age to keep entries in this feed in minutes
	UINT		m_max_entries;       ///< Maximum number of entries to keep in this feed
	BOOL		m_show_images;
	BOOL		m_show_permalink;	 ///< Load entry from permalink instead of just description
	BOOL		m_prefetch_entries;  ///< Prefetch linked article for all new entries
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif //_WEBFEED_H_
