/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/prefetch.h"

#if defined DOC_PREFETCH_API && defined LOGDOC_INLINE_FINDER_SUPPORT

#include "modules/dochand/win.h"
#include "modules/style/css_save.h"


PreFetcher::PreFetcher(URL& page_url, Window* window)
	: m_page_url(page_url), m_window(window), m_message_handler(NULL)
{
	// Use window's message handler if we have one, otherwise main_message_handler
	if (window)
		m_message_handler = window->GetMessageHandler();

	if (!m_message_handler)
		m_message_handler = g_main_message_handler;
}

PreFetcher::~PreFetcher()
{
	m_message_handler->UnsetCallBacks(this);
}

OP_STATUS PreFetcher::LoadNextInQueue()
{
	if (m_waiting_for_load.Empty())
		return OpStatus::OK;

	LoadURLLink* next_url = (LoadURLLink*)m_waiting_for_load.First();
	next_url->Out();

	OP_STATUS status = LoadURL(next_url->m_load_url, next_url->m_referer_url);

	OP_DELETE(next_url);
	return status;
}

OP_STATUS PreFetcher::AddUrlToLoadingQueue(URL& url, URL& referer)
{
	LoadURLLink* new_url = OP_NEW(LoadURLLink, (url, referer));
	if (!new_url)
		return OpStatus::ERR_NO_MEMORY;

	new_url->Into(&m_waiting_for_load);

	return OpStatus::OK;
}

OP_STATUS PreFetcher::AddIncludedURL(URL& url)
{
	// Make sure we don't load it again
	uni_char* url_string = UniSetNewStr(url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr());
	if (!url_string)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(m_included_urls.Add(url_string)))
	{
		OP_DELETEA(url_string);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS PreFetcher::Fetch(URL& referer_url)
{
	RETURN_IF_ERROR(AddIncludedURL(m_page_url));

	// If it's loaded already then start parsing for links
	if (m_page_url.Status(TRUE) == URL_LOADED)
	{
		RETURN_IF_ERROR(CacheLoadedURL(m_page_url));
		RETURN_IF_ERROR(CrawlPage(m_page_url));
		return LoadNextInQueue();
	}
	else  // have to load it first
		return LoadURL(m_page_url, referer_url);
}

OP_STATUS PreFetcher::LoadURL(URL& url, URL& referer_url)
{
	OpMessage url_messages[] = { MSG_URL_MOVED, MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED };
	RETURN_IF_ERROR(m_message_handler->SetCallBackList(this, 0, url_messages, ARRAY_SIZE(url_messages)));

	CommState state = url.Load(m_message_handler, referer_url);
	if (state == COMM_LOADING)
	{
		m_current_loading_url = url;

		// Give it 30 seconds to start loading
		m_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, url.Id(), ERR_HTTP_TIMEOUT, 30000);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS PreFetcher::CrawlPage(URL& page_url)
{
	return InlineFinder::ParseLoadedURLForInlines(page_url, this, m_window);
}

void PreFetcher::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	BOOL oom = FALSE;

	if (par1 != (MH_PARAM_1) m_current_loading_url.Id())
		return;   // don't care about other loading URLs

	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
		m_message_handler->UnsetCallBack(this, MSG_URL_LOADING_FAILED);

		if (m_current_loading_url.Status(TRUE) == URL_LOADED)
		{
			oom = OpStatus::IsMemoryError(HandleLoadedURL(m_current_loading_url));

			if (!oom)
			{
				// load next URL
				if (m_waiting_for_load.Empty())  // nothing more to load, we're done
				{
					PreFetcher *prefetcher = this;
					OP_DELETE(prefetcher);
					return;
				}
				else  // start loading next url
					oom = OpStatus::IsMemoryError(LoadNextInQueue());
			}
		}

		break;
	case MSG_URL_MOVED:
		{
			URL new_url = m_current_loading_url.GetAttribute(URL::KMovedToURL, TRUE);
			if (!new_url.IsEmpty())
				m_current_loading_url = new_url;
		}
		break;
	case MSG_URL_LOADING_FAILED:
		{
			// load next URL
			if (m_waiting_for_load.Empty())  // nothing more to load, we're done
			{
				PreFetcher *prefetcher = this;
				OP_DELETE(prefetcher);
				return;
			}
			else  	// start loading next url
				oom = OpStatus::IsMemoryError(LoadNextInQueue());
		}
		break;
	default:
		OP_ASSERT(FALSE);  // didn't register for any other messages
	}

	if (oom)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		PreFetcher *prefetcher = this;
		OP_DELETE(prefetcher);
	}
}

OP_STATUS PreFetcher::HandleLoadedURL(URL& url)
{
	// keep URL itself
	CacheLoadedURL(m_current_loading_url);

	// check if URL has other URLs we want to cache as well
	switch (url.ContentType())
	{
	case URL_HTML_CONTENT:
	case URL_XML_CONTENT:
	case URL_WML_CONTENT:
	case URL_SVG_CONTENT:
		{
			RETURN_IF_ERROR(CrawlPage(url));
		}
		break;
	case URL_CSS_CONTENT:
		{
			// Have to search CSS files for URLs to also include
			AutoDeleteHead css_links;

#ifdef STYLE_EXTRACT_URLS
			RETURN_IF_ERROR(ExtractURLs(url, url, &css_links));
#endif // STYLE_EXTRACT_URLS
			RETURN_IF_LEAVE(LinkListFoundL(css_links));
		}
		break;
	}

	return OpStatus::OK;
}

void PreFetcher::LinkFoundL(URL& link_url, Markup::Type type, HTML5TokenWrapper *token)
{
	if (m_included_urls.Contains(link_url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr()))
		return;

	LEAVE_IF_FATAL(AddIncludedURL(link_url));

	/// If it's already loaded then handle it, otherwise load it first
	if (link_url.Status(TRUE) == URL_LOADED)
		LEAVE_IF_FATAL(HandleLoadedURL(link_url));
	else
		LEAVE_IF_FATAL(AddUrlToLoadingQueue(link_url, m_page_url));
}

void PreFetcher::LinkListFoundL(Head& link_list)
{
	for (URLLink* link = (URLLink*)link_list.First(); link; link = (URLLink*)link->Suc())
		LinkFoundL(link->url, Markup::HTE_ANY, NULL);
}
#endif // DOC_PREFETCH_API
