// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.


#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeed.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/webfeeds/src/webfeedstorage.h"
#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeedutil.h"

#include "modules/about/operafeeds.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/url_man.h"
#include "modules/xmlutils/xmlparser.h"

#ifdef DOC_PREFETCH_API
# include "modules/doc/prefetch.h"
#endif // DOC_PREFETCH_API

#define MsPerMinute 60000

// ***************************************************************************
//
//	WebFeed
//
// ***************************************************************************


WebFeed::WebFeed() :
	m_stub(NULL), m_reference_count(1), m_marked_for_removal(FALSE),
	m_title(NULL), m_link(WebFeedLinkElement::Alternate), m_author(NULL), m_tagline(NULL),
	m_status(STATUS_OK), m_next_free_entry_id(1), m_max_age(0), 
	m_max_entries(0), m_show_images(TRUE), m_show_permalink(FALSE), m_prefetch_entries(FALSE)
{
	OP_ASSERT(g_webfeeds_api);
}

OP_STATUS WebFeed::Init()
{
	WebFeedsAPI_impl::WebFeedStub* stub = OP_NEW(WebFeedsAPI_impl::WebFeedStub, ());
	
	if (!stub)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = stub->Init();

	if (status == OpStatus::OK)
		status = Init(stub);

	if (OpStatus::IsError(status))
		OP_DELETE(stub);

	return status;
}

OP_STATUS WebFeed::Init(WebFeedsAPI_impl::WebFeedStub* stub)
{
	OP_ASSERT(m_title == NULL && m_author == NULL && m_tagline == NULL && m_stub == NULL);
	
	m_title = OP_NEW(WebFeedContentElement, ());
	m_author = OP_NEW(WebFeedPersonElement, ());
	m_tagline = OP_NEW(WebFeedContentElement, ());
	m_stub = stub;
	
	if (!m_title || !m_author || !m_tagline)
	{
		if (m_title)
			m_title->DecRef();
		OP_DELETE(m_author);
		if (m_tagline)
			m_tagline->DecRef();

		return OpStatus::ERR_NO_MEMORY;
	}

	m_stub->SetFeed(this, TRUE);
	
	return OpStatus::OK;
}

WebFeed::~WebFeed()
{
	OP_ASSERT(m_reference_count == 0 || m_reference_count == 1);

	/* WebFeed and its WebFeedEntries is one system which is deleted
	 * at the same time. The reference count refers to all external
	 * references to either feed or its entries. When no references
	 * exists to either a feed or one of its entries delete all of them */

	// delete all entries:
	m_entries.Clear();
	
	if (m_title)
		m_title->DecRef();
	OP_DELETE(m_author);
	if (m_tagline)
		m_tagline->DecRef();
}

OpFeed* WebFeed::IncRef()
{
	m_reference_count++;
	return this;
}

void WebFeed::DecRef()
{
	OP_ASSERT(m_reference_count > 0);
	if (--m_reference_count == 0)
	{
		OP_DELETE(m_stub);
		OP_DELETE(this);
	}
	else if (m_reference_count == 1 && m_marked_for_removal) // Only reference left is from api, and it has marked it for removal
		m_stub->RemoveFeedFromMemory();
}

ES_Object* WebFeed::GetDOMObject(DOM_Environment* environment)
{
	ES_Object* dom_feed = NULL;
	if (m_dom_objects.GetData(environment, &dom_feed) == OpStatus::OK)
		return dom_feed;
	
	return NULL;
}

void WebFeed::SetDOMObject(ES_Object *dom_obj, DOM_Environment* environment)
{
	ES_Object* dummy;
	// Have to remove entry first, because if it already exists adding it again will result in error, and there is no update method
	// Ignore error from removing something which doesn't exist
	m_dom_objects.Remove(environment, &dummy);
	m_dom_objects.Add(environment, dom_obj);
}

OpFeed::FeedStatus WebFeed::GetStatus() { return m_stub->GetStatus(); }
void WebFeed::SetStatus(OpFeed::FeedStatus status) { m_stub->SetStatus(status); }

double WebFeed::LastUpdated() {
	OP_ASSERT(m_stub);
	return m_stub->GetUpdateTime();
}

void WebFeed::SetLastUpdated(double opdate)
{
	OP_ASSERT(m_stub);
	m_stub->SetUpdateTime(opdate);
}

void WebFeed::Subscribe()
{
	OP_ASSERT(m_stub);
	if (m_stub)
		m_stub->SetSubscribed(TRUE);
}

void WebFeed::UnSubscribe()
{
	OP_ASSERT(m_stub);
	if (m_stub)
		m_stub->SetSubscribed(FALSE);
}

BOOL WebFeed::IsSubscribed()
{
	OP_ASSERT(m_stub);
	if (!m_stub)
		return FALSE;
	return m_stub->IsSubscribed();
}

OP_STATUS WebFeed::Update(OpFeedListener* listener)
{
	return ((WebFeedsAPI_impl*)g_webfeeds_api)->LoadFeed(this, GetURL(), listener);
}

OpFeed* WebFeed::GetPrevious() { return (WebFeed*)Pred(); }
OpFeed* WebFeed::GetNext() { return (WebFeed*)Suc(); }
OpFeedEntry* WebFeed::GetFirstEntry()
{
	WebFeedEntry* first = (WebFeedEntry*)m_entries.First();
	if (first && first->MarkedForRemoval())  // ignore the real first if it's being removed
		first = (WebFeedEntry*)first->GetNext();
	
	return first;
}

OpFeedEntry* WebFeed::GetLastEntry()
{
	WebFeedEntry* last = (WebFeedEntry*)m_entries.Last();
	if (last && last->MarkedForRemoval())  // ignore the real last if it's being removed
		last = (WebFeedEntry*)last->GetPrevious();
	
	return last;
}

OpFeedEntry* WebFeed::GetEntryById(UINT id)
{
	for (OpFeedEntry* entry = GetFirstEntry(); entry; entry = entry->GetNext())
		if (entry->GetId() == id)
			return entry;

	return NULL;
}

OpFeed::FeedId WebFeed::GetId()
{
	if(m_stub)
		return m_stub->GetId();
	else
		return 0;
}

void WebFeed::SetId(FeedId id)
{
	OP_ASSERT(m_stub);
	m_stub->SetId(id);
}

OpFeedEntry::EntryId WebFeed::GetNextFreeEntryId() { return m_next_free_entry_id; }
void WebFeed::SetNextFreeEntryId(OpFeedEntry::EntryId id) { m_next_free_entry_id = id; }

UINT WebFeed::GetMaxAge() { return m_max_age; }
void WebFeed::SetMaxAge(UINT age) { m_max_age = age; }

UINT WebFeed::GetMaxEntries() { return m_max_entries; }
void WebFeed::SetMaxEntries(UINT entries) { m_max_entries = entries; }

UINT WebFeed::GetUpdateInterval() { OP_ASSERT(m_stub); return m_stub ? m_stub->GetUpdateInterval() : 1000; }
void WebFeed::SetUpdateInterval(UINT interval)
{
	OP_ASSERT(m_stub && interval > 0);
	if (interval == 0)
		interval = 1;
	if (m_stub)
		m_stub->SetUpdateInterval(interval);
}

UINT WebFeed::GetMinUpdateInterval() { return m_stub ? m_stub->GetMinUpdateInterval() : 0; }
void WebFeed::SetMinUpdateInterval(UINT interval)
{
	if (m_stub)
		m_stub->SetMinUpdateInterval(interval);
}

BOOL WebFeed::GetShowImages() { return m_show_images; }
void WebFeed::SetShowImages(BOOL show) { m_show_images = show; }

BOOL WebFeed::GetShowPermalink() { return m_show_permalink; }
void WebFeed::SetShowPermalink(BOOL show) { m_show_permalink = show; }

void WebFeed::SetPrefetchPrimaryWhenNewEntries(BOOL prefetch, BOOL prefetch_now)
{
#ifdef DOC_PREFETCH_API
	if (!m_prefetch_entries && prefetch_now)
		for (OpFeedEntry* entry = GetFirstEntry(); entry; entry = entry->GetNext())
		{
			RAISE_AND_RETURN_VOID_IF_ERROR(((WebFeedEntry*)entry)->PrefetchPrimaryLink());
		}
#endif // DOC_PREFETCH_API

	m_prefetch_entries = prefetch;
}


UINT WebFeed::GetTotalCount() { return m_stub->GetTotalCount(); }
UINT WebFeed::GetUnreadCount() { return m_stub->GetUnreadCount(); }
void WebFeed::MarkOneEntryUnread()
{
	m_stub->IncrementUnreadCount();
#ifdef WEBFEEDS_DISPLAY_SUPPORT
//	g_webfeeds_api_impl->UpdateFeedWindow(this);
#endif // WEBFEEDS_DISPLAY_SUPPORT
}

void WebFeed::MarkOneEntryNotUnread()
{
	m_stub->IncrementUnreadCount(-1);
#ifdef WEBFEEDS_DISPLAY_SUPPORT
//	g_webfeeds_api_impl->UpdateFeedWindow(this);
#endif // WEBFEEDS_DISPLAY_SUPPORT
}

void WebFeed::MarkAllEntries(OpFeedEntry::ReadStatus status)
{
	for (OpFeedEntry* entry = GetFirstEntry(); entry; entry = entry->GetNext())
		entry->SetReadStatus(status);

#ifdef WEBFEEDS_REFRESH_FEED_WINDOWS
	g_webfeeds_api_impl->UpdateFeedWindow(this);
#endif // WEBFEEDS_REFRESH_FEED_WINDOWS
}

UINT WebFeed::GetSizeUsed()
{
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	return m_stub->GetDiskSpaceUsed();
#else
	return 0;
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
}

URL& WebFeed::GetURL() { OP_ASSERT(m_stub); return m_stub->GetURL(); }
void WebFeed::SetURL(URL& url) { OP_ASSERT(m_stub); m_stub->SetURL(url); }

OP_STATUS WebFeed::GetFeedIdURL(OpString& res)
{
	return GetStub()->GetFeedIdURL(res);
}

OpFeedContent* WebFeed::GetTitle() { return m_title; }
void WebFeed::SetParsedTitle(WebFeedContentElement* title)
{
	if (m_title == title)
		return;

	if (m_title)
		m_title->DecRef();
	m_title = title;
	m_title->IncRef();

	if (title && !title->IsBinary() && !m_stub->HasTitle())
		m_stub->SetTitle(title->Data());
}

OP_STATUS WebFeed::GetUserDefinedTitle(OpString& title)
{
	if (GetStub()->HasTitle())
		return GetStub()->GetTitle(title);
	else
		return title.Set(m_title->Data());
}

OP_STATUS WebFeed::SetUserDefinedTitle(const uni_char* new_title)
{
	return GetStub()->SetTitle(new_title);	
}

WebFeedLinkElement* WebFeed::FeedLink() { return &m_link; }
WebFeedPersonElement* WebFeed::AuthorElement() { return m_author; }
const uni_char* WebFeed::GetAuthor() { return m_author->Name().CStr(); }
WebFeedContentElement* WebFeed::Tagline() { return m_tagline; }

const uni_char* WebFeed::GetProperty(const uni_char* property)
{
	// TODO	
	return NULL;
}

const uni_char* WebFeed::GetLogo()
{
	// TODO	
	return NULL;
}

const uni_char* WebFeed::GetIcon()
{
	return m_icon.CStr();
}

OP_STATUS WebFeed::SetIcon(const uni_char* icon_url)
{
	return m_icon.Set(icon_url);
}

BOOL WebFeed::AddEntryL(WebFeedEntry* entry, OpFeedEntry::EntryId id/*=0*/, WebFeedEntry** actual_entry /* = NULL */)
{
	if (actual_entry)
		*actual_entry = NULL;
	
	if (id == 0)
		entry->SetId(m_next_free_entry_id++);
	else
		entry->SetId(id);

	double current_time = OpDate::GetCurrentUTCTime();
	double time_to_live = GetMaxAge() ? GetMaxAge() : g_webfeeds_api->GetDefMaxAge();
	double expired_time = current_time - (time_to_live * MsPerMinute);
	
	if (entry->GetPublicationDate() == 0.0)  // none set from the feed, give it current time
		entry->SetPublicationDate(current_time);
	// Don't add entry if it has already expired
	else if (time_to_live && entry->GetPublicationDate() < expired_time)
		return FALSE;

	// If entry list is full, only accept entry if it is newer than oldest entry
	if (GetTotalCount() == GetMaxEntries() || m_stub->HasReachedDiskSpaceLimit())
	{
		OpFeedEntry* last = GetLastEntry();
		if (last && entry->GetPublicationDate() <= last->GetPublicationDate())
			return FALSE;
	}

	// Find if we already have this entry.
	// Have to search entire list since update time might have changed
	WebFeedEntry* e;
	OpString guid; ANCHOR(OpString, guid);
	guid.SetL(entry->GetGuid());
	if (!guid.IsEmpty())
	{
		for (e = (WebFeedEntry*)m_entries.First(); e; e = (WebFeedEntry*)e->Suc())
		{
			const uni_char* eid = e->GetGuid();
			if (eid && (guid == eid))
			{
				if (entry->GetPublicationDate() <= e->GetPublicationDate())
				{
					if (actual_entry)
						*actual_entry = e;
					return FALSE;
				}
				else // if last added is newer then replace old
				{
					m_stub->IncrementTotalCount(-1);
					entry->SetReadStatus(e->GetReadStatus()); // keep read status of old entry
					e->MarkForRemoval();
					break;
				}
			}
		}
	}
	else if (entry->GetTitle())// If the entry doesn't have a guid, then assume entries with same title and timestamp are the same
	{
		OpString link; ANCHOR(OpString, link);
        entry->GetPrimaryLink().GetAttributeL(URL::KUniName_Username_Password_Hidden, link);
		
		const uni_char* title = entry->GetTitle()->Data(); // Only in RSS is guid optional, and title can only be text in RSS (and not binary as in Atom)
		double timestamp = entry->GetPublicationDate();

		if (title || !link.IsEmpty())
		{
			for (e = (WebFeedEntry*)m_entries.First(); e; e = (WebFeedEntry*)e->Suc())
			{
				// Check if link is different, then it can't be same entry
				OpString elink; ANCHOR(OpString, elink);
                e->GetPrimaryLink().GetAttributeL(URL::KUniName_Username_Password_Hidden,elink);
				
				if (!(elink == link))
					continue;

				if (!title || !e->GetTitle())
					continue;
				const uni_char* etitle = e->GetTitle()->Data();

				if (etitle && (uni_strcmp(title, etitle) == 0) && (link == elink) && (timestamp == e->GetPublicationDate()))
				{
					if (actual_entry)
						*actual_entry = e;
					return FALSE;
				}
			}	
		}
	}

	CheckFeedForNumEntries(1);  // make room for one more entry if full

	// Insert into correct position (chronological). As most feeds are already sorted
	// this usually shouldn't have to search very far.
	for (e = (WebFeedEntry*)m_entries.Last(); 
		 e && (e->GetPublicationDate() < entry->GetPublicationDate());
		 e = (WebFeedEntry*)e->Pred()) /* empty body */;

	if (e)
		entry->Follow(e);
	else // insert first if list is empty, or no one is newer
		entry->IntoStart(&m_entries);

	// Add reference to entry from feed
	entry->SetFeed(this);
	entry->IncRef();
	
	m_stub->IncrementTotalCount();
	if (entry->GetReadStatus() == OpFeedEntry::STATUS_UNREAD)
		m_stub->IncrementUnreadCount();

	if (actual_entry)
		*actual_entry = entry;

	return TRUE;
}

void WebFeed::RemoveEntry(WebFeedEntry* entry)
{
	entry->Out();
	if (!entry->MarkedForRemoval())  // marked ones have already been acounted for
		RemoveEntryFromCount(entry);
	
	OP_DELETE(entry);
}

void WebFeed::RemoveEntryFromCount(WebFeedEntry* entry)
{
	m_stub->IncrementTotalCount(-1);
	if (entry->GetReadStatus() == OpFeedEntry::STATUS_UNREAD)
		m_stub->IncrementUnreadCount(-1);
}

void WebFeed::CheckFeedForNumEntries(UINT free_entries_to_reserve)
{
	UINT max_entries = GetMaxEntries() ? GetMaxEntries() : g_webfeeds_api->GetDefMaxEntries();
	if (!max_entries)  // no limit
		return;

	int need_to_free = GetTotalCount() + free_entries_to_reserve - max_entries;
	// Remove from the rear as many as necessary
	WebFeedEntry* entry = (WebFeedEntry*)GetLastEntry();
	while (entry && need_to_free > 0)
	{
		WebFeedEntry* prev_entry = (WebFeedEntry*)entry->GetPrevious();  // have to get this now as entry might be deleted by next few calls
		if (!entry->GetKeep())
		{
			entry->MarkForRemoval();
			need_to_free--;
		}
		entry = prev_entry;
	}
}

void WebFeed::CheckFeedForExpiredEntries()
{
	UINT time_to_live = GetMaxAge() ? GetMaxAge() : g_webfeeds_api->GetDefMaxAge();
	if (!time_to_live)
		return;
	
	double expired_time = OpDate::GetCurrentUTCTime() - (time_to_live * MsPerMinute);
	WebFeedEntry* entry = (WebFeedEntry*)GetLastEntry();
	while (entry && entry->GetPublicationDate() < expired_time)
	{
		WebFeedEntry* prev_entry = (WebFeedEntry*)entry->GetPrevious();  // have to get this now as entry might be deleted by next few calls
		if (!entry->GetKeep())
			entry->MarkForRemoval();
		entry = prev_entry;
	}
}


#ifdef WEBFEEDS_DISPLAY_SUPPORT
# ifdef OLD_FEED_DISPLAY
inline void AddHTMLL(URL& out_url, const uni_char* data)
{
	LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, data));
}
# endif // OLD_FEED_DISPLAY

OP_STATUS WebFeed::WriteFeed(URL& out_url, BOOL complete_document, BOOL write_subscribe_header)
{
#ifndef OLD_FEED_DISPLAY
	if (out_url.IsEmpty())
	{
		out_url = g_url_api->GetURL("opera:feeds");
		out_url.Unload();
	}
	OperaFeeds feed_preview(out_url, this);
	OP_STATUS status = feed_preview.GenerateData();
#else
	TRAPD(status, WriteFeedL(out_url, complete_document, write_subscribe_header));
#endif
	return status;
}

#ifdef OLD_FEED_DISPLAY
void WebFeed::WriteFeedL(URL& out_url, BOOL complete_document, BOOL write_subscribe_header)
{
	BOOL subscribe_header = write_subscribe_header && !IsSubscribed();

	// Create a new URL to write generated data to:
	OpString id_url;  ANCHOR(OpString, id_url);
	LEAVE_IF_ERROR(GetFeedIdURL(id_url));
	if (out_url.IsEmpty())
		out_url = urlManager->GetURL(id_url.CStr());

	if (complete_document)
	{
		// Get address of style file:
		OpString styleurl; ANCHOR(OpString, styleurl);
		g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleWebFeedsDisplay, &styleurl);
		OpString direction;  ANCHOR(OpString, direction);
		switch (g_languageManager->GetWritingDirection)
		{
		case OpLanguageManager::LTR:
		default:
			direction.SetL("ltr");
			break;

		case OpLanguageManager::RTL:
			direction.SetL("rtl");
			break;
		}

		// Write headers
		AddHTMLL(out_url, UNI_L("\xFEFF"));  // Byte order mark
		AddHTMLL(out_url, UNI_L("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\n"));
		out_url.WriteDocumentDataUniSprintf(UNI_L("<html dir=\"%s\" lang=\"%s\">\n<head>\n"), direction.CStr(), g_languageManager->GetLanguage().CStr());
		out_url.WriteDocumentDataUniSprintf(UNI_L("<title>%s</title>\n"), m_stub->GetTitle());
		out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen,projection,tv,handheld,speech\">\n"), styleurl.CStr());
		AddHTMLL(out_url, UNI_L("<script type='text/javascript'>\n"));
		if (subscribe_header)
			out_url.WriteDocumentDataUniSprintf(UNI_L("function subscribe(elem) { var feed = opera.feeds.getFeedById(%d); if(feed) { feed.subscribe(); elem.style.visibility = 'hidden';}}\n"), GetId());
		AddHTMLL(out_url, UNI_L("function markread(elem) { elem.className = 'readentry'; }\n"));
		AddHTMLL(out_url, UNI_L("</script>\n</head>\n<body>\n"));
	}

	// Translated strings
	OpString subscribed_title, unsubscribed_title, subscribe_text, subscribe_button, 
		title_text, author_text, newest_text, every_x_minutes, every_x_hours, 
		every_x_days, update_frequency, address_text;

#if LANGUAGE_DATABASE_VERSION >= 864
	g_languageManager->GetStringL(Str::S_WEBFEEDS_SUBSCRIBED_TITLE, subscribed_title);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_UNSUBSCRIBED_TITLE, unsubscribed_title);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_SUBSCRIBE, subscribe_text);
	g_languageManager->GetStringL(Str::S_MINI_FEED_SUBSCRIBE, subscribe_button);

	g_languageManager->GetStringL(Str::S_WEBFEEDS_FEED_TITLE, title_text);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_FEED_AUTHOR, author_text);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_FEED_NEWEST_ENTRY, newest_text);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_UPDATE_FREQUENCY, update_frequency);
	g_languageManager->GetStringL(Str::SI_LOCATION_TEXT, address_text);
	
	g_languageManager->GetStringL(Str::S_EVERY_X_MINUTES, every_x_minutes);
	g_languageManager->GetStringL(Str::S_EVERY_X_HOURS, every_x_hours);
	g_languageManager->GetStringL(Str::S_WEBFEEDS_UPDATE_EVERY_X_DAYS, every_x_days);
#else
	every_x_minutes.SetL("%d");
	every_x_hours.SetL("%d");
	every_x_days.SetL("%d");
#endif

	if (subscribe_header)
	{
		out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), subscribed_title.CStr());
		out_url.WriteDocumentDataUniSprintf(UNI_L("<h2>%s <button type=\"button\" onclick=\"subscribe(this)\">%s</button></h2>\n"), subscribe_text.CStr(), subscribe_button.CStr());

		AddHTMLL(out_url, UNI_L("<dl>\n"));

		if (m_title->HasValue())
		{
			out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd>"), title_text.CStr());
			m_title->WriteAsStrippedHTMLL(out_url);
			AddHTMLL(out_url, UNI_L("</dd>\n"));
		}

		if (GetAuthor())
			out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd>%s</dd>\n"), author_text.CStr(), GetAuthor());

		if (GetFirstEntry())
		{
			uni_char* time = WebFeedUtil::TimeToString(GetFirstEntry()->GetPublicationDate());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd>%s</dd>\n"), newest_text.CStr(), time);
			OP_DELETEA(time);
		}
		if (UINT update_interval = GetMinUpdateInterval())
		{
			if (update_interval <= 180)
			{
				out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd>"), update_frequency.CStr());
				out_url.WriteDocumentDataUniSprintf(every_x_minutes.CStr(), update_interval);
				AddHTMLL(out_url, UNI_L("</dd>\n"));
			}
			else
			{
				out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd>"), update_frequency.CStr());
				update_interval /= 60;
				if (update_interval < 72)
					out_url.WriteDocumentDataUniSprintf(every_x_hours.CStr(), update_interval);
				else
					out_url.WriteDocumentDataUniSprintf(every_x_days.CStr(), update_interval / 24);

				AddHTMLL(out_url, UNI_L("</dd>\n"));
			}
		}

		const uni_char* feed_url = GetURL().GetUniName(FALSE, PASSWORD_HIDDEN);
		out_url.WriteDocumentDataUniSprintf(UNI_L("<dt>%s:</dt><dd><a href=\"%s\">%s</a></dd>\n"), address_text.CStr(), feed_url, feed_url);

		AddHTMLL(out_url, UNI_L("</dl>"));
	}
	else
		out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), subscribed_title.CStr());

	// Write title (if any):
	AddHTMLL(out_url, UNI_L("<h3>\n"));

	OpString icon; ANCHOR(OpString, icon);
	LEAVE_IF_ERROR(m_stub->GetInternalFeedIcon(icon));
	if (!icon.IsEmpty())
	{
		AddHTMLL(out_url, UNI_L("<img src=\""));
		AddHTMLL(out_url, icon.CStr());
		AddHTMLL(out_url, UNI_L("\" alt=\"\" height=\"16\" width=\"16\">"));
	}

	AddHTMLL(out_url, m_stub->GetTitle());

	// Add last updated time in brackets after title:
	uni_char* short_time = WebFeedUtil::TimeToShortString(LastUpdated(), TRUE);
	if (short_time)
		out_url.WriteDocumentDataUniSprintf(UNI_L(" [%s]"), short_time);
	OP_DELETEA(short_time); short_time = NULL;
	AddHTMLL(out_url, UNI_L("\n</h3>\n"));

	// Write title of all entries:
	AddHTMLL(out_url, UNI_L("<ul>"));
	for (WebFeedEntry* entry = (WebFeedEntry*)GetFirstEntry(); entry; entry = (WebFeedEntry*)entry->GetNext())
	{
		if (entry->GetReadStatus() == OpFeedEntry::STATUS_UNREAD)
			AddHTMLL(out_url, UNI_L("<li><a class=\"unread\" onclick=\"markread(this);\""));
		else
			AddHTMLL(out_url, UNI_L("<li><a "));
		
		if (m_show_permalink)
		{
			URL link = entry->GetPrimaryLink(); ANCHOR(URL, link);
			if (!link.IsEmpty())
				out_url.WriteDocumentDataUniSprintf(UNI_L(" href=\"%s\">"), link.GetUniName(FALSE, PASSWORD_SHOW));
		}
		else
			out_url.WriteDocumentDataUniSprintf(UNI_L(" href=\"opera:feed-id/%08x-%08x\">"), GetId(), entry->GetId());
		((WebFeedContentElement*)entry->GetTitle())->WriteAsStrippedHTMLL(out_url);
		AddHTMLL(out_url, UNI_L("</a></li>\n"));
	}
	AddHTMLL(out_url, UNI_L("</ul>\n"));

	if (complete_document)
		WebFeedUtil::WriteGeneratedPageFooterL(out_url);
}

#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
void WebFeed::WriteSettingsPageL(URL& out_url)
{
	// Create a new URL to write generated data to:
	if (out_url.IsEmpty())
		out_url = urlManager->GetNewOperaURL();

	OpString title; ANCHOR(OpString, title);
	LEAVE_IF_ERROR(title.AppendFormat(UNI_L("%s - %s"), m_stub->GetTitle(), UNI_L("settings")));

	WebFeedUtil::WriteGeneratedPageHeaderL(out_url, title.CStr());

	out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), title.CStr());
	AddHTMLL(out_url, UNI_L("<form id=\"feed-settings\" action=\"\">\n"));
	AddHTMLL(out_url, UNI_L("<dl>\n"));

	AddHTMLL(out_url, UNI_L("<dt>Title</dt>"));  // TODO locallize
	OpString escaped_title; ANCHOR(OpString, escaped_title);
	LEAVE_IF_ERROR(WebFeedUtil::EscapeHTML(m_stub->GetTitle(), escaped_title));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<dd><input name=\"title\" type=\"text\" size=\"50\" value=\"%s\"></dd>\n"), escaped_title.CStr());

	g_webfeeds_api_impl->WriteUpdateAndExpireFormL(out_url, this);

	AddHTMLL(out_url, UNI_L("</dl></form>\n"));

	WebFeedUtil::WriteGeneratedPageFooterL(out_url);
}
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
void WebFeed::SaveOverrideSettingsL(WriteBuffer &buf)
{
	if (m_max_age == g_webfeeds_api->GetDefMaxAge()
		&& m_max_entries == g_webfeeds_api->GetDefMaxEntries()
		&& m_show_images == g_webfeeds_api->GetDefShowImages()
		&& !m_show_permalink)
		return;
	
	buf.Append("<overrides>\n");

	if (m_max_age != g_webfeeds_api->GetDefMaxAge())
		WriteSettingNum(buf, "max-age", m_max_age);

	if (m_max_entries != g_webfeeds_api->GetDefMaxEntries())
		WriteSettingNum(buf, "max-entries", m_max_entries);

	if (m_show_images != g_webfeeds_api->GetDefShowImages())
		WriteSettingBool(buf, "show-images", m_show_images);

	if (m_show_permalink)
		WriteSettingBool(buf, "show-permalink", m_show_permalink);

	buf.Append("</overrides>\n");	
}

void WebFeed::SaveEntriesL(WriteBuffer &buf, int space_available)
{
	UINT pre_space_used = buf.GetBytesWritten();
	
	WebFeedEntry *entry = NULL;
	for (entry = (WebFeedEntry *)m_entries.First(); entry; entry = (WebFeedEntry *)entry->Suc())
		if (entry->GetKeep())
			entry->SaveL(buf);

	// TODO: notify someone if there is no space left at this point?

	m_stub->SetHasReachedDiskSpaceLimit(FALSE);

	// Next pass save as many other entries as we have room for, starting with newest one
	for (entry = (WebFeedEntry *)m_entries.First(); entry; entry = (WebFeedEntry *)entry->Suc())
	{
		if (entry->GetKeep())
			continue;   // already saved

		if (entry->MarkedForRemoval())
			continue;   // removed, should not keep this one

		if (space_available)  // limited disk space
		{
			UINT space_used = buf.GetBytesWritten() - pre_space_used;
			if (space_available < 0 || space_used + 10 + entry->GetApproximateSaveSize() > (UINT)space_available)
			{
				m_stub->SetHasReachedDiskSpaceLimit(TRUE);
				
				// remove the remaining entries:
				while (entry)
				{
					WebFeedEntry* next_entry = (WebFeedEntry *)entry->Suc();
					entry->MarkForRemoval();  // might delete entry
					entry = next_entry;
				}
				
				break;
			}
		}
		
		entry->SaveL(buf);
	}
}


/* static */ void
WebFeed::WriteSettingNum(WriteBuffer &buf, const char *name, UINT num)
{
	buf.Append("  <setting name=\"");
	buf.Append(name);
	buf.Append("\" value=\"");
	buf.Append(num);
	buf.Append("\"/>\n");
}

/* static */ void
WebFeed::WriteSettingBool(WriteBuffer &buf, const char *name, BOOL value)
{
	buf.Append("  <setting name=\"");
	buf.Append(name);
	buf.Append("\" value=\"");
	if (value)
		buf.Append("yes\"/>\n");
	else
		buf.Append("no\"/>\n");
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
#endif  // WEBFEEDS_BACKEND_SUPPORT
