/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/helistelm.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/imganimhandler.h"
#include "modules/dochand/win.h" // for Window
#ifdef HAS_ATVEF_SUPPORT
#include "modules/display/tvmanager.h"
#endif // HAS_ATVEF_SUPPORT
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/layout/box/box.h"

HEListElm::HEListElm(HTML_Element* inline_elm, InlineResourceType inline_type, FramesDocument* fdoc)
 :   animation_handler(NULL)
   , url_imagecontent_provider(NULL)
   , doc(fdoc)
   , ref(this)
   , image_width(0)
   , image_height(0)
   , image_last_decoded_height(0)
   , inline_type(inline_type)
   , handled(FALSE)
   , syncronize_animation(TRUE)
   , event_sent(FALSE)
#ifdef REMOVE_IRRELEVANT_IMAGES
	, signaled_irrelevant(FALSE)
#endif
   , image_active(TRUE)
#ifdef HAS_ATVEF_SUPPORT
   , is_atvef_image(FALSE)
#endif
   , image_visible(FALSE)
   , delay_load(TRUE)
   , from_user_css(FALSE)
#ifdef CORS_SUPPORT
   , is_crossorigin_allowed(FALSE)
#endif // CORS_SUPPORT
{
	if (inline_elm)
	{
		if (inline_type == EXTRA_BGIMAGE_INLINE)
		{
			// We want to insert the reference after the other bgimages
			// to preserve the sequence instead of first in the list.
			HTML_Element::BgImageIterator iter(inline_elm);
			HEListElm *iter_elm = iter.Next();
			if (iter_elm)
			{
				HEListElm *candidate;
				do
				{
					candidate = iter_elm;
					iter_elm = iter.Next();
				} while (iter_elm);

				InsertAfter(candidate, inline_elm);
			}
			else
				SetElm(inline_elm);
		}
		else
			SetElm(inline_elm);
	}

	if (GetElm()->GetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, SpecialNs::NS_LOGDOC))
		event_sent = TRUE;
}

/*virtual*/ HEListElm::~HEListElm()
{
	LoadInlineElm *lie = GetLoadInlineElm();
	if (lie)
	{
		// GetLoadInlineElm() is NULL for disconnected HEListElms and for the print HEListElm list.
		if (InList())
		{
			lie->Remove(this);
			if (lie->GetLoading() && lie->IsUnused() && !GetFramesDocument()->IsBeingFreed())
				GetFramesDocument()->StopLoadingInline(lie);
		}
	}
	else
		Out();

	Undisplay();

	if (url_imagecontent_provider)
		url_imagecontent_provider->DecRef(this);
}

void HEListElm::UrlMoved(URL moved_to_url)
{
	OP_ASSERT(GetLoadInlineElm()); // If we're not connected to a resource, then how did we move?
	if (GetLoadInlineElm())
		GetLoadInlineElm()->UrlMoved(doc, moved_to_url.Id());
}

BOOL HEListElm::UpdateImageUrlIfNeeded(URL* img_url)
{
	if (url_imagecontent_provider
		&& url_imagecontent_provider->GetUrl()->Id(TRUE) == img_url->Id(TRUE))
		return FALSE; // Url was the same.

#ifdef HAS_ATVEF_SUPPORT
	if (img_url->Type() == URL_TV)
	{
		UpdateImageUrl(img_url); // Is this really needed here?
		// The problem is that the URL-id does not seem to stay
		// constant for atvef images, so it's constantly redraw
		// (busyloop).
		return FALSE; // No full-redraw, thanks
	}
#endif

	return UpdateImageUrl(img_url);
}

BOOL HEListElm::UpdateImageUrl(URL* img_url)
{
	BOOL replace_url = FALSE;
	BOOL is_visible = GetImageVisible();

	old_url = URL();

	if (url_imagecontent_provider && url_imagecontent_provider->GetUrl()->Id(TRUE) != img_url->Id(TRUE))
		event_sent = FALSE;

	if (url_imagecontent_provider)
	{
		// We are replacing an imageurl. Make sure it is not syncronized.
		syncronize_animation = FALSE;

		if (animation_handler)
		{
			animation_handler->DecRef(this);
			animation_handler = NULL;
		}
		if (is_visible)
		{
			url_imagecontent_provider->GetImage().DecVisible(this);
		}
		url_imagecontent_provider->DecRef(this);
		url_imagecontent_provider = NULL;
		replace_url = TRUE;
		event_sent = FALSE;
	}

	switch (inline_type)
	{
	case IMAGE_INLINE:
	case BGIMAGE_INLINE:
	case EXTRA_BGIMAGE_INLINE:
	case BORDER_IMAGE_INLINE:
	case VIDEO_POSTER_INLINE:
		OP_ASSERT(img_url);

#ifdef HAS_ATVEF_SUPPORT
		if (img_url->Type() == URL_TV)
		{
			is_atvef_image = TRUE;
			return TRUE;
		}
#endif

		url_imagecontent_provider = UrlImageContentProvider::FindImageContentProvider(*img_url);
		if (url_imagecontent_provider == NULL)
		{
			url_imagecontent_provider = OP_NEW(UrlImageContentProvider, (*img_url));
			if (url_imagecontent_provider == NULL)
				return FALSE;
		}

		if (LoadInlineElm *lie = GetLoadInlineElm())
			lie->SetUrlImageContentProvider(url_imagecontent_provider);

		url_imagecontent_provider->IncRef(this);
		Image img = url_imagecontent_provider->GetImage();
		if (replace_url)
		{
			if (is_visible)
			{
				if (OpStatus::IsMemoryError(img.IncVisible(this)))
					return FALSE;
			}
			// FIXME: This won't have any effect if is_visible was false since
			// the image has no listener then.
			// The current fix for this is to call LoadAll() after SetImagePosition()
			// in ImageContent::Paint().
			if (OpStatus::IsMemoryError(img.OnLoadAll(url_imagecontent_provider)))
				return FALSE;
		}
		break;
	}

	return TRUE;
}

void HEListElm::LoadAll(BOOL decode_absolutely_all)
{
	if (url_imagecontent_provider)
	{
		Image img = url_imagecontent_provider->GetImage();
		if (decode_absolutely_all)
		{
			OpStatus::Ignore(img.OnLoadAll(url_imagecontent_provider)); // Ignored, always returns OK.
		}
		else if (url_imagecontent_provider->IsLoaded())
		{
			double time_start = g_op_time_info->GetRuntimeMS();
			// IsAnimated() implies that the first frame is decoded, good enough for us.
			while (!img.ImageDecoded() && !img.IsFailed() && !img.IsAnimated())
			{
#ifdef LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD
				// Don't load now if the image is too big. The size limit for thumbnail windows may be tweaked separately.
# ifdef LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD_THUMBNAIL
				if (doc->GetWindow()->IsThumbnailWindow() && img.Width() * img.Height() > LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD_THUMBNAIL)
					break;
# endif // LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD_THUMBNAIL
				if (!doc->GetWindow()->IsThumbnailWindow() && img.Width() * img.Height() > LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD)
					break;
#endif // LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD

				// Don't load all if it takes too long.
				if (g_op_time_info->GetRuntimeMS() - time_start > IMAGE_DECODE_TIME_TIMEOUT)
					break;

				img.OnMoreData(url_imagecontent_provider);
			}
		}
	}
}

void HEListElm::LoadSmall()
{
	if (url_imagecontent_provider)
	{
		Image img = url_imagecontent_provider->GetImage();
		if (url_imagecontent_provider->IsLoaded() && !img.ImageDecoded() && !img.IsFailed())
		{
			if (img.Width() && img.Height() && img.Width() * img.Height() < 300)
				img.OnMoreData(url_imagecontent_provider);
		}
	}
}

void HEListElm::Reset()
{
	image_last_decoded_height = 0;
}

BOOL
HEListElm::IsInList(LoadInlineElm* lie)
{
	return GetLoadInlineElm() == lie;
}

void HEListElm::SetDelayLoad(BOOL v)
{
	if (static_cast<BOOL>(delay_load) != v)
	{
		delay_load = v;
		if (InList())
			GetLoadInlineElm()->CheckDelayLoadState();
	}
}

Image HEListElm::GetImage()
{
	OP_ASSERT(IsImageInline());

	LoadInlineElm *lie = GetLoadInlineElm();

	// Update security in case we're going to fetch an image
	// that hasn't yet contributed to the document security.
	//
	// Note: lie is NULL in a small gap between BeforeAttributeChange
	// and AfterAttributeChange when changing the src attribute. This
	// function will be called in that gap but in that case the image
	// won't actually be used so we can ignore the flag in LoadInlineElm.
	if (lie && lie->GetNeedSecurityUpdate())
	{
		URL *img_url = lie->GetUrl();
		URLStatus url_status = img_url->Status(FALSE);
		BYTE security_status = SECURITY_STATE_UNKNOWN;

		if (url_imagecontent_provider && url_imagecontent_provider->HasEverProvidedData())
			security_status = url_imagecontent_provider->GetSecurityStateOfSource(*img_url);
		else if (url_status != URL_UNLOADED && url_status != URL_LOADING_FAILURE)
			security_status = img_url->GetAttribute(URL::KSecurityStatus);

		if (security_status != SECURITY_STATE_UNKNOWN)
		{
			lie->SetNeedSecurityUpdate(FALSE);
			doc->GetWindow()->SetSecurityState(security_status, TRUE, img_url->GetAttribute(URL::KSecurityText).CStr(), img_url);
		}
	}

	if (lie && !image_visible)
	{
		BOOL allowed = !GetAccessForbidden();
		if (allowed && lie->GetLoading() && lie->GetUrl()->Status(TRUE) == URL_LOADED)
		{
			// Might be cross-network but the
			// MSG_URL_LOADING_FAILED message might not have arrived yet. Check here as well
			// to avoid painting images that aren't allowed to be seen.
			GetLoadInlineElm()->CheckAccessForbidden();
			allowed = !GetAccessForbidden();
		}

		if (!allowed)
			// Can't hide the real Image once it's been made visible or we'll get unbalanced calls to IncVisible()/DecVisible().
			return Image();
	}

	if (url_imagecontent_provider)
		return url_imagecontent_provider->GetImage();

#ifdef HAS_ATVEF_SUPPORT
	if (is_atvef_image)
		return imgManager->GetAtvefImage();
#endif

	return Image();
}

OP_STATUS HEListElm::SendLoadEvent()
{
	event_sent = TRUE;

	if (GetElm()->GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL_STYLE)
		return GetElm()->SendEvent(ONLOAD, doc);

	return OpStatus::OK;
}

OP_STATUS HEListElm::OnLoad(BOOL force)
{
	if (!doc || event_sent)
		return OpStatus::OK;

	if (inline_type == IMAGE_INLINE)
	{
		if (url_imagecontent_provider && !GetElm()->IsMatchingType(HE_BODY, NS_HTML))
		{
			Image image = url_imagecontent_provider->GetImage();
			// Check if the image is decoded yet.
			// If IsAnimated() returns TRUE, then we know that we
			// have decoded at least until frame#2 and that is good enough
			// for us, and good enough for the image decoder that will
			// not decode any more.
			if (force || image.ImageDecoded() || image.IsAnimated())
				return SendLoadEvent();
		}
	}
	else if (inline_type == TRACK_INLINE)
		return SendLoadEvent();

	return OpStatus::OK;
}

static BOOL AllowProgressive(VisualDevice* vis_dev, Image& image, int image_w, int image_h)
{
	if (image.IsInterlaced())
		return FALSE;

	// If the image is scaled and has completed loading, we want to do
	// a full update so that a proper filter will be applied (to the
	// entire image).
	if (image.ImageDecoded())
		if (vis_dev->ImageNeedsInterpolation(image, image_w, image_h))
			return FALSE;

	return TRUE;
}

void HEListElm::Update(Image& image)
{
	VisualDevice* vis_dev = doc->GetVisualDevice();
	if (!vis_dev)
		return;

	if (!IsBgImageInline() && AllowProgressive(vis_dev, image, image_width, image_height))
	{
		// Update only the new decoded lines
		UINT32 old_line = image_last_decoded_height;
		UINT32 curr_line = image.GetLastDecodedLine();
		if (curr_line - old_line > 0)
		{
			long height = image.Height();
			if (image_height != height && height)
			{
				// The image is stretched so we have to stretch the
				// values to update the right position.
				old_line = (old_line * image_height) / height;
				curr_line = (curr_line * image_height) / height + 1;
			}

			OpRect repaint_bbox(0, old_line, image_width, curr_line - old_line);
			image_pos.Apply(repaint_bbox);

			// We could use FALSE here to get a much smoother repaint
			// when large images is decoded. The paint itself is very
			// fast since we only repaint the new lines of the image but
			// the traverse for each paint will consume A LOT of cpu.
			vis_dev->Update(repaint_bbox.x, repaint_bbox.y,
							repaint_bbox.width, repaint_bbox.height, TRUE);
		}
#ifdef ASYNC_IMAGE_DECODERS
		// When using the async decoders, we have no idea of how large
		// portion of the image that is actually decoded, so we need to
		// refresh the complete image each time... [2004-09-13] /pw
		SetImageLastDecodedHeight(0);
#else
		SetImageLastDecodedHeight(curr_line);
#endif // ASYNC_IMAGE_DECODERS
	}
	else if (!IsBgImageInline() || image.ImageDecoded())
	{
#ifdef SVG_SUPPORT
		// For images loaded in svg:s the update call will cause a
		// repaint of a very large region. That's not something we
		// want because it kills performance.  Just let svg handle the
		// invalidation (= smaller area invalidated) and also the
		// update-call here shouldn't be necessary because svg only
		// draws fully decoded images anyway.
		if (GetElm()->GetNsType() != NS_SVG)
#endif // SVG_SUPPORT
		{
			OpRect repaint_bbox = GetImageBBox();
			vis_dev->Update(repaint_bbox.x, repaint_bbox.y,
							repaint_bbox.width, repaint_bbox.height, TRUE);
		}
	}
}

// From the ImageListener interface
void HEListElm::OnPortionDecoded()
{
	if (OpStatus::IsMemoryError(OnLoad()))
		if (doc->GetWindow())
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		else
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

	if (GetImageVisible())
	{
		if (url_imagecontent_provider)
		{
			Image image = url_imagecontent_provider->GetImage();
			if (image.IsAnimated() && !animation_handler)
				animation_handler = ImageAnimationHandler::GetImageAnimationHandler(this, doc, syncronize_animation);

			// If image.IsAnimated is TRUE we know that the second frame
			// is being decoded. We don't want to make unnecessary updates
			// when the following frames are decoded.
			if (!image.IsAnimated() ||
				image_last_decoded_height != image.GetLastDecodedLine())
				Update(image);
		}
		if (animation_handler)
			animation_handler->OnPortionDecoded(this);
	}
#ifdef REMOVE_IRRELEVANT_IMAGES
	if (!signaled_irrelevant && url_imagecontent_provider &&
		// We can't let this signal to layout if we are traversing or reflowing.
		!doc->IsReflowing() &&
		!doc->GetVisualDevice()->IsPainting())
	{
		Image image = url_imagecontent_provider->GetImage();
		if (!image.IsContentRelevant() && GetElm()->GetLayoutBox())
		{
			signaled_irrelevant = TRUE;
			if (GetElm()->GetLayoutBox())
				GetElm()->GetLayoutBox()->SignalChange(doc);
		}
	}
#endif
}

void HEListElm::SendOnError()
{
	if (doc && !event_sent)
	{
		event_sent = TRUE;
		if (GetElm()->GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL_STYLE)
			OpStatus::Ignore(GetElm()->SendEvent(ONERROR, doc));
	}
}

// From the ImageListener interface
void HEListElm::OnError(OP_STATUS status)
{
	// No events to background images and poster images, only normal images.
	if (inline_type == IMAGE_INLINE)
		SendOnError();

	// RaiseCondition is defined ONLY on memory errors
	if (OpStatus::IsMemoryError(status))
	{
		if (doc)
			doc->GetWindow()->RaiseCondition(status);
		else
			g_memory_manager->RaiseCondition(status);
	}
}

void HEListElm::MoreDataLoaded()
{
	if (url_imagecontent_provider)
		url_imagecontent_provider->MoreDataLoaded();
}

void HEListElm::Display(FramesDocument* doc, const AffinePos& pos, long width, long height, BOOL background, BOOL expand_area)
{
	Image img = GetImage();

#ifdef HAS_ATVEF_SUPPORT
	if (img.IsAtvefImage())
	{
		VisualDevice* vis_dev = NULL;
		OP_ASSERT(this->doc && this->doc->GetVisualDevice());

		if (this->doc)
			vis_dev = this->doc->GetVisualDevice();

		if (vis_dev && g_tvManager)
		{
			OpRect view_bbox(0, 0, width, height);
			pos.Apply(view_bbox);

			OpRect screen_rect = vis_dev->ScaleToScreen(view_bbox);
			screen_rect = vis_dev->OffsetToContainerAndScroll(screen_rect);
			OpPoint screen_origo(0, 0);
			screen_origo = vis_dev->GetOpView()->ConvertToScreen(screen_origo);
			screen_rect.OffsetBy(screen_origo);

			URL tv_url = GetElm()->GetImageURL(FALSE, doc->GetLogicalDocument());
			g_tvManager->SetDisplayRect(&tv_url, screen_rect);
			g_tvManager->OnTvWindowAvailable(&tv_url, TRUE);
		}

		image_pos = pos;
		image_width = width;
		image_height = height;

		SetImageVisible(TRUE);
		return;
	}

#endif

	if (!GetImageVisible())
	{
		if (!img.IsEmpty())
		{
			if (background)
				GetElm()->SetHasBgImage(TRUE);

			OP_STATUS status = img.IncVisible(this);
			if (OpStatus::IsError(status))
			{
				if (OpStatus::IsMemoryError(status))
					doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return;
			}
		}

		expand_area = FALSE;
	}

	if (expand_area)
	{
		// FIXME: Not the faintest idea how to make this work for transforms.
		OpRect current_bbox = GetImageBBox();
		OpRect expand_bbox(0, 0, width, height);
		pos.Apply(expand_bbox);

		current_bbox.UnionWith(expand_bbox);

		image_pos.SetTranslation(current_bbox.x, current_bbox.y);
		image_width = current_bbox.width;
		image_height = current_bbox.height;
	}
	else
	{
		image_pos = pos;
		image_width = width;
		image_height = height;
	}

	SetImageVisible(TRUE);

	SetHasPendingAnimationInvalidation(FALSE);

	if (doc && img.IsAnimated() && !animation_handler)
		animation_handler = ImageAnimationHandler::GetImageAnimationHandler(this, this->doc, syncronize_animation);
}

void HEListElm::Undisplay()
{
	if (GetImageVisible())
	{
		Image img = GetImage();

#ifdef HAS_ATVEF_SUPPORT
		if (img.IsAtvefImage())
		{
			if (g_tvManager)
			{
				URL tv_url = GetElm()->GetImageURL(FALSE, doc->GetLogicalDocument());
				g_tvManager->OnTvWindowAvailable(&tv_url, FALSE);
			}

			SetImageVisible(FALSE);
			return;
		}
#endif

		if (!img.IsEmpty())
		{
			if (animation_handler)
			{
				animation_handler->DecRef(this);
				animation_handler = NULL;
			}

			image_last_decoded_height = 0;
			img.DecVisible(this);
		}
	}

	SetImageVisible(FALSE);
}

OP_STATUS HEListElm::SendImageFinishedLoadingEvent(FramesDocument* doc)
{
	OP_ASSERT(doc);
	OP_ASSERT(!GetEventSent());

	if (inline_type == IMAGE_INLINE && HElm()->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
	{
		HElm()->SetSpecialBoolAttr(ATTR_JS_DELAYED_ONLOAD, TRUE, SpecialNs::NS_LOGDOC);
		return OpStatus::OK;
	}

	// Since this requires decoding the image, skip it if there are no event listeners
	if (inline_type == IMAGE_INLINE &&
		(GetElm()->HasEventHandler(doc, ONLOAD) || GetElm()->HasEventHandler(doc, ONERROR)))
	{
		LoadInlineElm* lie = GetLoadInlineElm();
		OP_ASSERT(lie);
		URLStatus url_status = lie->GetUrl()->Status(TRUE);
		if (url_status == URL_LOADED)
		{
#ifdef SVG_SUPPORT
			// If it is an svg image wait with sending onerror or
			// onload until it has been parsed.
			if (lie->GetUrl()->ContentType() == URL_SVG_CONTENT)
				return OpStatus::OK;
#endif // SVG_SUPPORT

			Image img = GetImage();
			// IsAnimated() implies that the first frame is decoded which is good enough for us.
			if (img.ImageDecoded() || img.IsAnimated())
				return OnLoad();

			if (GetUrlContentProvider() && !GetImageVisible())
			{
				/* Force image decoding, so that we can send onload or onerror as appropriate. */
				OP_STATUS status = img.IncVisible(this);
				if (OpStatus::IsSuccess(status))
				{
					// Scripts might load image in a hidden place and move
					// on load, in which case we don't want image to be
					// re-decoded.
					ImageManager::GraceTimeLock lock(imgManager);

					// This will send onload or onerror through the ImageListener callbacks (OnError and OnPortionDecoded).
					// except for animated images since only the first frame will be decoded.
					status = img.OnLoadAll(GetUrlContentProvider());
					img.DecVisible(this);
					OP_ASSERT(GetEventSent());
				}
				return status;
			}
		}
		else if (url_status == URL_LOADING_FAILURE)
			SendOnError();
	}
	return OpStatus::OK;
}

OP_STATUS
HEListElm::CreateURLDataDescriptor(URL_DataDescriptor*& url_data_descriptor, MessageHandler* mh, URL::URL_Redirect follow_ref, BOOL get_raw_data/* = FALSE*/, BOOL get_decoded_data /*= TRUE*/, Window *window /*= NULL*/, URLContentType override_content_type /*= URL_UNDETERMINED_CONTENT*/, unsigned short override_charset_id/* = 0*/, BOOL get_original_content /*= FALSE*/, unsigned short parent_charset_id /*= 0*/)
{
	LoadInlineElm* lie = GetLoadInlineElm();
	if (lie)
	{
		if (GetAccessForbidden())
			return OpStatus::ERR_NO_ACCESS;

		url_data_descriptor = lie->GetUrl()->GetDescriptor(mh, follow_ref, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id);
		if (url_data_descriptor)
			return OpStatus::OK;
		// Might also be OOM but there is no way to know for sure.
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

/*virtual*/ void HEListElm::OnDelete(FramesDocument*)
{
	// Image elements that are referenced by script should not be deleted
	// since the DOM code could need to access them.
	// The HEListElm will be deleted again during GC when the script engine
	// is no longer referencing it. GetESElement() will then return NULL.
	if (inline_type == IMAGE_INLINE && HElm()->GetESElement())
		return;

	OP_DELETE(this);
}

BOOL HEListElm::GetAccessForbidden()
{
	if (!GetLoadInlineElm())
		return FALSE;

	if (from_user_css)
	{
		// Resources from user stylesheets have special handling
		// so the cached access in LoadInlineElm is not applicable.
		BOOL allow_inline_url;
		OpSecurityState sec_state;
		RETURN_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::DOC_INLINE_LOAD,
														 OpSecurityContext(doc),
														 OpSecurityContext(*GetLoadInlineElm()->GetUrl(), inline_type, TRUE),
														 allow_inline_url,
														 sec_state));
		OP_ASSERT(!sec_state.suspended || "This security check is done after the inline url is loaded, so it should not suspend.");
		return !allow_inline_url;
	}
	else
		return GetLoadInlineElm()->GetAccessForbidden() || !doc->CheckExternalImagePreference(inline_type, *GetLoadInlineElm()->GetUrl());
}
