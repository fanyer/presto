/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PREFETCH_H
#define PREFETCH_H

/**
 * PreFetcher will load a URL and (if it's a (X)HTML, SVG or WML document)
 * all links found in the document.
 * 
 * The purpose is to make sure they are in cache, so that loading the URL
 * later will be fast.
 * 
 * It is however not perfect.  Among other limitations, scripts will not run, so
 * anything loaded from scripts will not be fetched, and no layout will be run,
 * so that elements which normally won't show (display: none) may still be
 * loaded.  This is a design decision to keep the prefetcher fairly lightweight.
 * 
 * Also to keep it fairly lightweigth it will only load one inline element at a
 * time.  Since it is expected to be run as a background task it shouldn't matter
 * that this takes much longer.
 * 
 * The PreFetcher will delete itself once it's done fetching (don't create one on the stack!)
 */

#if defined DOC_PREFETCH_API && defined LOGDOC_INLINE_FINDER_SUPPORT

#include "modules/hardcore/mh/mh.h"
#include "modules/logdoc/inline_finder.h"
#include "modules/url/url2.h"

class HTML5TokenWrapper;

class PreFetcher : public MessageObject, public InlineFinder::LinkHandler
{
public:
	/**
	 * @param page_url  URL of the document to prefetch
	 * @param window    window (if any) to use for messages (e.g. authentication dialogs)
	 */
	PreFetcher(URL& page_url, Window* window);
	~PreFetcher();

	/** 
	 * Will prefetch the document
	 * @param referer_url  The URL of the document which requested the prefetch (if any)
	 * @return  If OpStatus::OK then it has started fetching and it will delete itself
	 *          when done.  If return value is anything else the caller must delete
	 *          the PreFetcher.
	 */
	OP_STATUS Fetch(URL& referer_url);

	/// Message callback
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/// ParseForLinks callbacks
	virtual void LinkFoundL(URL& link_url, Markup::Type type, HTML5TokenWrapper *token);
	virtual void LinkListFoundL(Head& link_list);
private:
	class AutoDeleteStringSet : public OpGenericStringHashTable
	{
		public:
			AutoDeleteStringSet() : OpGenericStringHashTable(TRUE) {}
			virtual ~AutoDeleteStringSet() { this->DeleteAll(); }
			/// Will take over ownership of key
			OP_STATUS Add(uni_char* key) { return OpGenericStringHashTable::Add(key, key); }  // use key both for key and data, since we need only get the data pointer when deleting
			void Delete(void* data) { delete[] (uni_char*)data; }
	};

	struct LoadURLLink : public Link
	{
		URL m_load_url, m_referer_url;
		LoadURLLink(URL& load_url, URL& referer_url) : m_load_url(load_url), m_referer_url(referer_url) {}
	};

	/// starts loading of specific URL
	OP_STATUS LoadURL(URL& url, URL& referer_url);
	/// this is called once a URL is finished loading.
	OP_STATUS HandleLoadedURL(URL& url);
	/// Will parse a document and start loading of all links in it
	OP_STATUS CrawlPage(URL& page_url);
	/// Just loading the URL should make sure it's cached, but make a function in case we need to refine this
	OP_STATUS CacheLoadedURL(URL& url) { return OpStatus::OK; }

	/// Since we only load one URL at a time, this is called to start loading the next URL once one is done
	OP_STATUS LoadNextInQueue();
	/// Add another URL to the list of URLs to load
	OP_STATUS AddUrlToLoadingQueue(URL& url, URL& referer_url);
	/// List of URLs we've already loaded.  To make sure we only load each once.
	OP_STATUS AddIncludedURL(URL& url);

	URL      m_page_url;             ///< URL of the page which was requested prefetched
	URL		 m_current_loading_url;  ///< we only load one at a time, this is supposed to run in the background
	Window*  m_window;
	MessageHandler* m_message_handler;
	AutoDeleteHead  m_waiting_for_load;
	AutoDeleteStringSet	m_included_urls;
};
#endif // DOC_PREFETCH_API

#endif /*PREFETCH_H*/
