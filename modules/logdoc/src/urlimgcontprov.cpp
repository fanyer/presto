/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/urlimgcontprov.h"

#include "modules/logdoc/helistelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/url/url2.h"
#ifdef SVG_SUPPORT
# include "modules/svg/svg_image.h"
# include "modules/svg/svg_workplace.h"
# include "modules/layout/box/box.h"
#endif // SVG_SUPPORT

UrlImageContentProvider::UrlImageContentProvider(const URL& url, HEListElm *dummy)
	: url(url)
	, url_data_desc(NULL)
	, refcount(0)
	, all_read(FALSE)
	, need_reset(FALSE)
	, is_multipart_reload(FALSE)
#ifdef SVG_SUPPORT
	, svgimageref(NULL)
#endif // SVG_SUPPORT
	, security_state_of_source(SECURITY_STATE_UNKNOWN)
	, has_ever_provided_data(FALSE)
	, forced_content_type(URL_UNKNOWN_CONTENT)
	, storage_id(~0u)
	, current_data_len(-1)
	, loading_counter(0)
{
	OP_ASSERT(!FindImageContentProvider(url));
	Into(&g_opera->logdoc_module.m_url_image_content_providers[(unsigned int)url.Id(TRUE) % ImageCPHashCount]);
}

UrlImageContentProvider::~UrlImageContentProvider()
{
	// If you get any of theese asserts, you still have a Image object
	// somewhere which uses this contentprovider. It will crash later.
	OP_ASSERT(image.CountListeners() == 0);
	OP_ASSERT(image.GetRefCount() <= 1);

	OP_DELETE(url_data_desc);
	url_data_desc = NULL;

	UnsetCallbacks();

	Out();
	OP_ASSERT(m_hle_list.First() == NULL);

#ifdef SVG_SUPPORT
	if(svgimageref)
	{
		svgimageref->Free();
	}
#endif // SVG_SUPPORT
}

void UrlImageContentProvider::IncRef()
{
	refcount++;
}

void UrlImageContentProvider::IncRef(HEListElm* hle)
{
	IncRef();

	if (hle)
	{
		HEListElmRef *ref = hle->GetHEListElmRef();

		/* Shouldn't move HEListElm from another content provider
		   without DecRef:ing the other one first. */
		OP_ASSERT(!ref->InList() || m_hle_list.HasLink(ref));

		ref->Out();
		ref->Into(&m_hle_list);
	}
}

void UrlImageContentProvider::DecRef()
{
	/* Someone releasing a reference they didn't own. */
	OP_ASSERT(refcount > 0);
	if (refcount > 1)
		--refcount;
	else
		OP_DELETE(this);
}

void UrlImageContentProvider::DecRef(HEListElm* hle)
{
	if (hle)
	{
		HEListElmRef *ref = hle->GetHEListElmRef();

		/* Arguably doesn't make much sense to unregister a HEListElm
		   from the wrong UrlImageContentProvider.  Probably an error
		   if it actually happens. */
		OP_ASSERT(m_hle_list.HasLink(ref));

		if (m_hle_list.HasLink(ref))
			ref->Out();
	}

	DecRef();
}

void UrlImageContentProvider::ResetAllListElm()
{
	HEListElmRef* hleref = m_hle_list.First();
	while(hleref)
	{
		if (hleref->hle)
			hleref->hle->Reset();
		hleref = hleref->Suc();
	}
}

void
UrlImageContentProvider::MoreDataLoaded()
{
	if (!all_read)
	{
		GetImage().OnMoreData(this);
	}
}

Image
UrlImageContentProvider::GetImage()
{
	if (image.IsEmpty())
		image = imgManager->GetImage(this);
	return image;
}

#ifdef CPUUSAGETRACKING
/* virtual */ CPUUsageTracker*
UrlImageContentProvider::GetCPUUsageTracker()
{
	// Blame the last document in the list of elements. It's not 100% accurate but
	// good enough, and it's the most likely to have triggered an operation.
	HEListElmRef* hleref = m_hle_list.Last();
	while (hleref)
	{
		if (hleref->hle->GetFramesDocument())
			return hleref->hle->GetFramesDocument()->GetWindow()->GetCPUUsageTracker();
		hleref = hleref->Pred();
	}
	return NULL;
}
#endif // CPUUSAGETRACKING

OP_STATUS
UrlImageContentProvider::GetData(const char*& data, INT32& data_len, BOOL& more)
{
	if (need_reset)
	{
		need_reset = FALSE;
		OP_DELETE(url_data_desc);
		url_data_desc = NULL;
	}
	if (!url_data_desc)
	{
		url_data_desc = this->url.GetDescriptor(g_opera->logdoc_module.m_url_image_content_provider_mh, TRUE, TRUE);
		if (url_data_desc)
		{
			security_state_of_source = this->url.GetAttribute(URL::KSecurityStatus);
			SetCallbacks();
			storage_id = url.GetAttribute(URL::KStorageId);
		}
		else
		{
			if (url.Status(TRUE) == URL_LOADING_FAILURE)
				more = FALSE;
			else
				more = TRUE; // Will probably be an url descriptor soon, but it might be an OOM.
		}
	}
	if (url_data_desc)
	{
		TRAPD(err, current_data_len = data_len = url_data_desc->RetrieveDataL(more));
		RETURN_IF_ERROR(err);

		data = url_data_desc->GetBuffer();
		if (data_len && data)
		{
			if (!more)
			{
				all_read = TRUE;
			}
			has_ever_provided_data = TRUE;
			return OpStatus::OK;
		}
		if (!more)
		{
			all_read = TRUE;
			return OpStatus::ERR; // NULL data must be signalled with OpStatus::ERR (see image API:s)
		}
	}
	if (IsLoaded() && url.GetAttribute(URL::KDataPresent, TRUE))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::ERR; // NULL data must be signalled with OpStatus::ERR (see image API:s)
}

OP_STATUS
UrlImageContentProvider::Grow()
{
	OP_ASSERT(url_data_desc != NULL);
	if (url_data_desc)
	{
		return (url_data_desc->Grow() ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
	}
	return OpStatus::ERR;
}

void
UrlImageContentProvider::ConsumeData(INT32 len)
{
	if (url_data_desc)
	{
		if (all_read && len == current_data_len)
		{
			OP_DELETE(url_data_desc);
			url_data_desc = NULL;

			all_read = FALSE;
		}
		else
		{
			url_data_desc->ConsumeData(len);
		}
	}
}

INT32
UrlImageContentProvider::ContentType()
{
	return forced_content_type != URL_UNKNOWN_CONTENT ? forced_content_type : url.ContentType();
}

INT32
UrlImageContentProvider::ContentSize()
{
	return static_cast<INT32>(url.GetContentSize());
}

void
UrlImageContentProvider::SetContentType(INT32 content_type)
{
	forced_content_type = content_type;
}

void
UrlImageContentProvider::Reset()
{
	if (url.Status(TRUE) != URL_LOADING)
	{
		OP_DELETE(url_data_desc);
		url_data_desc = NULL;
	}
	else
		/* This is probably a multipart/reload URL, and then we need
		   to keep the data descriptor around until it's time to load
		   the next resources. */
		need_reset = TRUE;

	all_read = FALSE;
	has_ever_provided_data = FALSE;
}

void
UrlImageContentProvider::Rewind()
{
#if 1
	Reset();
#else // 1
	/* Reset position is broken... */

	if (url_data_desc)
		url_data_desc->ResetPosition();

	all_read = FALSE;
	has_ever_provided_data = FALSE;
#endif // 1
}

BOOL
UrlImageContentProvider::IsEqual(ImageContentProvider* content_provider)
{
	if (content_provider->IsUrlProvider())
	{
		return ((UrlImageContentProvider*)content_provider)->url == url;
	}
	return FALSE;
}

void
UrlImageContentProvider::UrlMoved()
{
	// When a url is moved we need to update the URL on all places to the destination-url.

	URL target_url = url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!target_url.IsEmpty())
	{
		OP_ASSERT(url_data_desc == NULL || !"UrlMoved() called after GetData(), something is wrong");
		OP_DELETE(url_data_desc);
		url_data_desc = NULL;

		url = target_url;

		HEListElmRef* hleref = m_hle_list.First();
		while(hleref)
		{
			if (hleref->hle)
				hleref->hle->UrlMoved(url);
			hleref = hleref->Suc();
		}

		// The URL has changed - update hash table

		Out();
		Into(&g_opera->logdoc_module.m_url_image_content_providers[(unsigned int)url.Id(TRUE) % ImageCPHashCount]);
	}
}

BOOL UrlImageContentProvider::IsImageDisplayed(const URL& url)
{
	UrlImageContentProvider* content_provider = FindImageContentProvider(url);
	if (content_provider)
	{
		HEListElmRef* hleref = content_provider->m_hle_list.First();
		while(hleref)
		{
			HEListElm* hle = hleref->hle;
			if (hle && hle->GetImageVisible())
				return TRUE;

			hleref = hleref->Suc();
		}
	}
	return FALSE;
}

BOOL
UrlImageContentProvider::IsLoaded()
{
	// Return TRUE if all the content provider's content has been loaded from the network
	// Will always TRUE if the content provider is receiving its data from a
	// synchronous source (disk or memory).
	return url.Status(TRUE) == URL_LOADED || url.Status(TRUE) == URL_LOADING_FAILURE; // Aborted too?
}

BYTE
UrlImageContentProvider::GetSecurityStateOfSource(URL& requested_url)
{
	// Check each url in the chain from the requested url to the actual image
	// url, and then add the image's (or its url's) own security state.
	URL url_in_chain = requested_url;
	BYTE security = SECURITY_STATE_UNKNOWN;
	while (!url_in_chain.IsEmpty() && !(url_in_chain == url))
	{
		// Forwarded chain
		security = MIN(security, url_in_chain.GetAttribute(URL::KSecurityStatus));
		url_in_chain = url_in_chain.GetAttribute(URL::KMovedToURL);
	}

	BYTE img_security = image.ImageDecoded() ? security_state_of_source : url.GetAttribute(URL::KSecurityStatus);
	return MIN(security, img_security);
}

UrlImageContentProvider*
UrlImageContentProvider::FindImageContentProvider(URL_ID id, BOOL follow_ref)
{
	UrlImageContentProvider* content_provider =
		(UrlImageContentProvider*) g_opera->logdoc_module.m_url_image_content_providers[(unsigned int)id % ImageCPHashCount].First();
	while (content_provider)
	{
		if (content_provider->GetUrl()->Id(follow_ref) == id)
			return content_provider;
		content_provider = (UrlImageContentProvider*) content_provider->Suc();
	}
	return NULL;
}

Image
UrlImageContentProvider::GetImageFromUrl(const URL& url)
{
	UrlImageContentProvider* content_provider = FindImageContentProvider(url);
	if (content_provider)
		return content_provider->GetImage();

#ifdef HAS_ATVEF_SUPPORT
	if (url.Type() == URL_TV)
	{
		return imgManager->GetAtvefImage();
	}
#endif

	return Image();
}

void UrlImageContentProvider::ResetAndRestartImageFromID(URL_ID id)
{
	UrlImageContentProvider* content_provider = FindImageContentProvider(id);
	if (content_provider)
	{
		imgManager->ResetImage(content_provider);
		content_provider->ResetAllListElm();
	}
}

void
UrlImageContentProvider::ResetImageFromUrl(const URL& url)
{
	UrlImageContentProvider* content_provider = FindImageContentProvider(url);
	if (content_provider)
	{
		content_provider->ResetAllListElm();
		imgManager->ResetImage(content_provider);
	}
}

void
UrlImageContentProvider::UrlMoved(const URL& url)
{
	/* This implementation allows the supplied URL argument to be any
	   of the URLs in the redirection chain/tree, which means that we
	   cannot look it up using the hash value. However, it is still
	   more likely than not that it is actually hashed as the supplied
	   URL, so look there first. It's the only qualified guess we can
	   make. */

	int guess_index = (unsigned int)url.Id(FALSE) % ImageCPHashCount;
	URL_ID id = url.Id(TRUE);

	for (int i = guess_index; i < ImageCPHashCount;)
	{
		UrlImageContentProvider* content_provider = (UrlImageContentProvider*) g_opera->logdoc_module.m_url_image_content_providers[i].First();

		while (content_provider)
		{
			if (content_provider->GetUrl()->Id(TRUE) == id)
			{
				content_provider->UrlMoved();
				return;
			}

			content_provider = (UrlImageContentProvider*) content_provider->Suc();
		}

		if (i == guess_index)
		{
			i = guess_index == 0 ? 1 : 0;
			guess_index = -1;
		}
		else
			i ++;
	}
}

void
UrlImageContentProvider::DecLoadingCounter()
{
	OP_ASSERT(loading_counter != 0);

	if (--loading_counter == 0)
	{
		SetCallbacks();

		MessageHandler *mh = g_opera->logdoc_module.m_url_image_content_provider_mh;
		mh->PostMessage(MSG_CLEANUP_CONTENT_PROVIDER, url.Id(TRUE), 0);
	}
}

void
UrlImageContentProvider::SetCallbacks()
{
	MessageHandler *mh = g_opera->logdoc_module.m_url_image_content_provider_mh;

	UnsetCallbacks();

	if (OpStatus::IsError(mh->SetCallBack(this, MSG_MULTIPART_RELOAD, url.Id(TRUE))) ||
	    OpStatus::IsError(mh->SetCallBack(this, MSG_URL_DATA_LOADED, url.Id(TRUE))) ||
	    OpStatus::IsError(mh->SetCallBack(this, MSG_URL_MOVED, url.Id(TRUE))) ||
	    OpStatus::IsError(mh->SetCallBack(this, MSG_CLEANUP_CONTENT_PROVIDER, url.Id(TRUE))))
		UnsetCallbacks();
}

void
UrlImageContentProvider::UnsetCallbacks()
{
	MessageHandler *mh = g_opera->logdoc_module.m_url_image_content_provider_mh;
	mh->UnsetCallBacks(this);
}

void
UrlImageContentProvider::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_MOVED:
		SetCallbacks();

		OP_ASSERT(!url_data_desc || !"MSG_URL_MOVED received after getting data!");
		OP_DELETE(url_data_desc);
		url_data_desc = NULL;
		need_reset = FALSE;

		break;

	case MSG_MULTIPART_RELOAD:
	case MSG_URL_DATA_LOADED:
		if (msg == MSG_MULTIPART_RELOAD)
			is_multipart_reload = TRUE;
		if (is_multipart_reload)
		{
			/* Posting all these messages from here is possibly bad, and since
			   we really don't need to except in the multi-part reload case,
			   we restrict the badness to that case. */
			HEListElmRef* hleref = m_hle_list.First();
			while(hleref)
			{
				if (hleref->hle)
					hleref->hle->GetFramesDocument()->GetMessageHandler()->PostMessage(msg, par1, par2);
				hleref = hleref->Suc();
			}
		}
		if (need_reset)
		{
			OP_DELETE(url_data_desc);
			url_data_desc = NULL;
			need_reset = FALSE;
		}
		break;

	case MSG_CLEANUP_CONTENT_PROVIDER:
		if (loading_counter == 0 && url_data_desc)
		{
			OP_DELETE(url_data_desc);
			url_data_desc = NULL;

			imgManager->ResetImage(this);
		}
		break;
	}
}

#ifdef SVG_SUPPORT
void
UrlImageContentProvider::ResetImage(Image new_image, BOOL undisplay /* = FALSE */)
{
	if (undisplay)
	{
		HEListElmRef* hleref = m_hle_list.First();
		while (hleref)
		{
			HEListElm* helm = hleref->hle;
			if (helm && helm->GetImageVisible())
				helm->Undisplay();
			hleref = hleref->Suc();
		}
	}

	image = new_image;
}

void
UrlImageContentProvider::UpdateSVGImageRef(LogicalDocument* logdoc)
{
	if (svgimageref && svgimageref->IsDocumentDisconnected())
	{
		svgimageref->Free();
		svgimageref = NULL;
	}

	if (!svgimageref && logdoc)
	{
		SVGImageRef* ref = NULL;
		if (OpStatus::IsSuccess(logdoc->GetSVGWorkplace()->GetSVGImageRef(url, &ref)))
			svgimageref = ref;
	}
}

void
UrlImageContentProvider::SVGFinishedLoading()
{
	HEListElmRef* hleref = m_hle_list.First();
	while(hleref)
	{
		HEListElm* hle = hleref->hle;
		if (hle)
		{
			HTML_Element* he = hle->HElm();
			if(he)
			{
				//he->MarkDirty(hle->GetFramesDocument());
				Box* box = he->GetLayoutBox();
				if(box)
					box->SignalChange(hle->GetFramesDocument(), BOX_SIGNAL_REASON_IMAGE_DATA_LOADED);

				he->SignalSVGIMGResourceFinished();
			}

			OpRect image_bbox = hle->GetImageBBox();
			hle->GetFramesDocument()->GetVisualDevice()->Update(image_bbox.x, image_bbox.y,
																image_bbox.width, image_bbox.height, TRUE);
		}
		hleref = hleref->Suc();
	}
}

BOOL
UrlImageContentProvider::OnSVGImageLoad()
{
	BOOL sent = FALSE;
	HEListElmRef* hleref = m_hle_list.First();
	while (hleref)
	{
		HEListElm* hle = hleref->hle;
		if (hle)
		{
			hle->OnLoad(TRUE);
			sent = sent || hle->GetEventSent();
		}
		hleref = hleref->Suc();
	}

	return sent;
}

void
UrlImageContentProvider::OnSVGImageLoadError()
{
	HEListElmRef* hleref = m_hle_list.First();
	while (hleref)
	{
		HEListElm* hle = hleref->hle;
		if (hle)
			hle->SendOnError();
		hleref = hleref->Suc();
	}
}
#endif // SVG_SUPPORT
