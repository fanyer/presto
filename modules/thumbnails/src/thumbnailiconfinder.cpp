/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#if defined(CORE_THUMBNAIL_SUPPORT)

#include "modules/doc/frm_doc.h"
#include "modules/thumbnails/src/thumbnailiconfinder.h"

ThumbnailIconFinder::IconCandidate::IconCandidate(URL url)
	: m_provider(NULL)
	, m_bitmap(NULL)
	, m_finder(NULL)
{
	OP_ASSERT(!url.IsEmpty());
	m_core_url = url;
}

ThumbnailIconFinder::IconCandidate::~IconCandidate()
{
	if (m_bitmap)
		m_image.ReleaseBitmap();

	if (!m_image.IsEmpty())
		m_image.DecVisible(this);

	if (m_provider)
		m_provider->DecRef();
}


void ThumbnailIconFinder::IconCandidate::OnPortionDecoded()
{
	OP_ASSERT(m_finder);
	OP_ASSERT(m_finder->m_listener);

	if (m_image.ImageDecoded())
	{
		if (m_bitmap)
			m_image.ReleaseBitmap();

		// To be clear that we are now not responsible for the bitmap anymore
		m_bitmap = NULL;

		m_bitmap = m_image.GetBitmap(this);
		OP_ASSERT(m_bitmap);
		m_finder->m_listener->OnIconCandidateChosen(this);
	}
}

void ThumbnailIconFinder::IconCandidate::OnError(OP_STATUS status)
{
	OP_ASSERT(m_finder);

	m_finder->m_listener->OnIconCandidateChosen(NULL);
}

OP_STATUS ThumbnailIconFinder::IconCandidate::SetProviderFromURL()
{
	OP_ASSERT(!m_core_url.IsEmpty());
	m_provider = UrlImageContentProvider::FindImageContentProvider(m_core_url);
	if (NULL == m_provider)
		m_provider = OP_NEW(UrlImageContentProvider, (m_core_url));

	// If we still didn't get a provider then we have trouble
	OP_ASSERT(m_provider);
	if (m_provider)
		m_provider->IncRef();
	else
		return OpStatus::ERR;

	return OpStatus::OK;
}

int	ThumbnailIconFinder::IconCandidate::GetWidth()
{
	if (m_provider)
		return m_provider->GetImage().Width();
	return -1;
}

int ThumbnailIconFinder::IconCandidate::GetHeight()
{
	if (m_provider)
		return m_provider->GetImage().Height();
	return -1;
}

void ThumbnailIconFinder::IconCandidate::DecodeAndNotify(ThumbnailIconFinder* finder)
{
	OP_ASSERT(m_provider);
	OP_ASSERT(finder);
	OP_ASSERT(finder->m_listener);

	m_image = m_provider->GetImage();
	m_image.IncVisible(this);
	m_finder = finder;
	OnPortionDecoded();
}

ThumbnailIconFinder::ThumbnailIconFinder()
	: m_window(NULL)
	, m_is_busy(FALSE)
	, m_min_icon_width(0)
	, m_min_icon_height(0)
	, m_pending_link_count(0)
	, m_listener(NULL)
{
}

ThumbnailIconFinder::~ThumbnailIconFinder()
{
	for (UINT32 i=0;i<m_candidates.GetCount();i++)
	{
		IconCandidate *candidate = m_candidates.Get(i);
		OP_ASSERT(candidate);
		if (candidate)
		{
			OP_ASSERT(!candidate->GetCoreURL().IsEmpty());

			candidate->GetCoreURL().StopLoading(g_main_message_handler);
			RemoveCallBacksForUrlID(candidate->GetCoreURL().Id());
		}
	}
	m_candidates.DeleteAll();
}

OP_STATUS ThumbnailIconFinder::StartIconFinder(ThumbnailIconFinderListener *listener, Window* window, unsigned int min_icon_width, unsigned int min_icon_height)
{
	OP_ASSERT(min_icon_width > 0);
	OP_ASSERT(min_icon_height > 0);

	// Don't start a new icon scan when another one is still active
	if (m_is_busy)
		return OpStatus::ERR_YIELD;

	m_is_busy = TRUE;
	m_listener = listener;
	m_window = window;
	m_min_icon_width = min_icon_width;
	m_min_icon_height = min_icon_height;

	if (NULL == m_listener)
		return OpStatus::ERR;

	return ScanDocForCandidates();
}

BOOL ThumbnailIconFinder::IsBusy()
{
	return m_is_busy;
}

ThumbnailIconFinder::IconCandidate* ThumbnailIconFinder::GetBestCandidate()
{
	IconCandidate* best = NULL, *cur = NULL;
	// Go through all the elements
	for (UINT32 i=0; i<m_candidates.GetCount(); i++)
	{
		cur = m_candidates.Get(i);

		// GetWidth() / GetHeight() return -1 if something went wrong with reading the image from the URL
		if (cur->GetWidth() < m_min_icon_width || cur->GetHeight() < m_min_icon_height)
			// The minimum size is not met, drop this element
			continue;

		if (NULL == best)
		{
			// First candidate
			best = cur;
			continue;
		}
		if ((cur->GetWidth() > best->GetWidth()) && (cur->GetHeight() > best->GetHeight()))
			// Found a better element
			best = cur;
	}

	return best;
}

OP_STATUS ThumbnailIconFinder::ScanDocForCandidates()
{
	FramesDocument* doc = m_window->GetActiveFrameDoc();
	const LinkElementDatabase* link_db = m_window->GetLinkDatabase();

	OP_ASSERT(link_db);
	if (NULL == link_db)
		return OpStatus::ERR;

	UINT32 link_count = link_db->GetSubLinkCount(), ignore_sub_index = 0;
	const LinkElement *link = NULL;

	for (UINT32 i=0;i<link_count;i++)
	{
		link = link_db->GetFromSubLinkIndex(i, ignore_sub_index);

		const URL* link_url = link->GetHRef(doc->GetLogicalDocument());

		if (NULL == link_url)
			continue;

#ifdef _DEBUG
		OpString icon_url_string;
		icon_url_string.Set(link_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI));
#endif

		URL_ID new_id = link_url->Id(TRUE);

		// We are only interested in a limited range of link kinds.
		// If we have found a link of such a kind, go on with processing it.
		if ((link->GetKinds() & LINK_TYPE_ICON) ||
			(link->GetKinds() & LINK_TYPE_APPLE_TOUCH_ICON) ||
			(link->GetKinds() & LINK_TYPE_IMAGE_SRC) ||
			(link->GetKinds() & LINK_TYPE_APPLE_TOUCH_ICON_PRECOMPOSED))
		{
			BOOL continue_outer_loop = FALSE;

			// Don't start loading same URL a couple of times, it's one and the same icon anyway
			for (UINT32 i=0;i<m_candidates.GetCount();i++)
				if (!m_candidates.Get(i)->GetCoreURL().IsEmpty() && m_candidates.Get(i)->GetCoreURL().Id(TRUE) == new_id)
				{
					continue_outer_loop = TRUE;
					break;
				}

			if (continue_outer_loop)
				continue;

			OpAutoPtr<IconCandidate> link(OP_NEW(IconCandidate, (*link_url)));

			if (NULL == link.get())
				return OpStatus::ERR_NO_MEMORY;

			URL_LoadPolicy load_policy;
			// Inlines will have KCachePolicy_AlwaysCheckNeverExpiredQueryURLs set to false
			load_policy.SetInlineElement(FALSE);
			// Some inter/intranet switch checks?
			load_policy.SetUserInitiated(TRUE);
			load_policy.SetReloadPolicy(URL_Reload_Full);

			URL zero_context_url;
			OpString url_string;

			RETURN_IF_ERROR(url_string.Set(doc->GetURL().GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI)));
			zero_context_url = g_url_api->GetURL(url_string.CStr());

			if (OpStatus::IsSuccess(AddCallBacksForUrlID(link_url->Id(TRUE))))
			{
				CommState st = const_cast<URL*>(link_url)->LoadDocument(g_main_message_handler, zero_context_url, load_policy);
				if (COMM_LOADING == st)
				{
					// Maybe we should take more CommState values for success here?

					if (OpStatus::IsSuccess(m_candidates.Add(link.get())))
					{
						// m_candidates now owns the icon candidate, forget about it
						link.release();
					}
					else
					{
						// Probably OOM, something went very wrong.
						RemoveCallBacksForUrlID(link_url->Id(TRUE));
						return OpStatus::ERR_NO_MEMORY;
					}
				}
				else
				{
					RemoveCallBacksForUrlID(link_url->Id(TRUE));
				}
			}
			else
			{
				// Probably OOM, something went very wrong.
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	m_pending_link_count = m_candidates.GetCount();

	if (m_pending_link_count == 0)
	{
		m_is_busy = FALSE;
		// No candidates at all, notify listener ASAP.
		m_listener->OnIconCandidateChosen(NULL);
	}
	return OpStatus::OK;
}

OP_STATUS ThumbnailIconFinder::AddCallBacksForUrlID(URL_ID id)
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_TIMEOUT, id));
	return OpStatus::OK;
}

void ThumbnailIconFinder::RemoveCallBacksForUrlID(URL_ID id)
{
	g_main_message_handler->UnsetCallBack(this, MSG_URL_DATA_LOADED, id);
	g_main_message_handler->UnsetCallBack(this, MSG_URL_LOADING_FAILED, id);
	g_main_message_handler->UnsetCallBack(this, MSG_URL_TIMEOUT, id);
}

void ThumbnailIconFinder::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	IconCandidate* candidate = NULL;

	for (UINT32 i=0;i<m_candidates.GetCount();i++)
	{
		candidate = m_candidates.Get(i);
		if (candidate->GetCoreURL().Id(TRUE) == (URL_ID)par1)
			break;
	}

	if (NULL == candidate)
	{
		OP_ASSERT(!"Callback for unknown candidate received!");
		return;
	}

	URL candidate_url = candidate->GetCoreURL();
	URLStatus status = (URLStatus)candidate_url.GetAttribute(URL::KLoadStatus);

	switch(msg)
	{
	case MSG_URL_DATA_LOADED:
		if (URL_LOADED == status)
		{
			OP_ASSERT(m_pending_link_count > 0);
			m_pending_link_count--;
			RemoveCallBacksForUrlID(candidate_url.Id(TRUE));

			// If we couldn't acquire an UrlImageContentProvider for this candidate then there is not much we can do.
			// GetHeight() and GetWidth() will return -1 later on, so not a big trouble here.
			OpStatus::Ignore(candidate->SetProviderFromURL());
		}
		break;
	case MSG_URL_LOADING_FAILED:
	case MSG_URL_TIMEOUT:
		RemoveCallBacksForUrlID(candidate_url.Id(TRUE));

		OP_ASSERT(m_pending_link_count > 0);
		m_pending_link_count--;
		break;
	default:
		OP_ASSERT(!"Unknown message!");
		break;
	};

	if (0 == m_pending_link_count)
		if (m_listener)
		{
			m_is_busy = FALSE;
			// Get the best candidate and pass it on to the listener
			IconCandidate* best = GetBestCandidate();

			if (best)
				best->DecodeAndNotify(this);
			else
				m_listener->OnIconCandidateChosen(NULL);
		}
		else
			OP_ASSERT(!"ThumbnailIconFinder: No listener to notify about candidate chosen!");
}

#endif // defined(CORE_THUMBNAIL_SUPPORT)
