/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#if defined(CORE_THUMBNAIL_SUPPORT) && defined(THUMBNAILS_LOGO_FINDER)
#include "modules/doc/frm_doc.h"
#include "modules/layout/cascade.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/thumbnails/src/thumbnaillogofinder.h"
#include "modules/thumbnails/src/thumbnail.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

ThumbnailLogoFinder::ThumbnailLogoFinder()
	: m_frm_doc(NULL)
{
}

ThumbnailLogoFinder::~ThumbnailLogoFinder()
{
	// Deletes the content
	m_candidates.DeleteAll();
}

OP_STATUS ThumbnailLogoFinder::Construct(FramesDocument* frm_doc)
{
	OP_ASSERT(frm_doc);

	if(!OpThumbnail::DocumentOk(frm_doc))
		return OpStatus::ERR;

	m_frm_doc = frm_doc;

	m_site_url = m_frm_doc->GetURL();

	// Get the "site name" from the URL, i.e. www.opera.com -> opera
	ServerName *server_name = m_site_url.GetServerName();
	if (NULL == server_name)
		return OpStatus::ERR;

	UINT32 count = server_name->GetNameComponentCount();
	if (count < 1)
		return OpStatus::ERR;
	return m_site_name.Set(server_name->GetNameComponent(count==1?0:1));
}

void ThumbnailLogoFinder::FindLogoRect(long thumbnail_width, long thumbnail_height, OpRect* logo_rect)
{
	// We'll go though all HTML elements in the document to check if they are images or if they contain a
	// background CSS image. Start from document root.
	HTML_Element *cur_el = m_frm_doc->GetLogicalDocument()->GetRoot();

	// No root might occur according to OpThumbnail::DocumentOK()
	OP_ASSERT(cur_el);

	while (cur_el)
	{
		// Check if the element is an <IMG> and score the image if available
		if (OpStatus::IsError(CheckHTMLElementIMG(cur_el)))
			return;

		// Check if the element has a CSS background and score the image if available
		if (OpStatus::IsError(CheckHTMLElementCSS(cur_el)))
			return;

		cur_el = cur_el->NextActual();
	}

	// Get the best logo candidate, will return NULL if no logo found
	LogoCandidate* best = GetBestCandidate();

	// The candidate will contain the image dimensions as found on page
	if (logo_rect && best)
		*logo_rect = best->position;
}

OP_STATUS ThumbnailLogoFinder::AddImageCandidate(const OpStringC& image_path, const OpStringC& image_alt, OpRect op_rect)
{
	OP_NEW_DBG("ThumbnailLogoFinder::AddImageCandidate()", "thumbnail-logo");

	const int logo_score_threshold = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_THRESHOLD);

	int pos_score = ScoreImagePos(op_rect);
	// If the image got negative score for position then ignore it completely
	if (pos_score < 0)
		return OpStatus::OK;

	int image_score = ScoreImage(image_path, image_alt);
	int final_score = image_score + pos_score;

	// If the image failed to score above the threshold, ignore it
	if (final_score < logo_score_threshold)
		return OpStatus::OK;

	// Add a new logo candidate
	LogoCandidate* candidate = OP_NEW(LogoCandidate, ());
	if (NULL == candidate)
		return OpStatus::ERR_NO_MEMORY;

	candidate->score = final_score;
	candidate->position = op_rect;
	if (OpStatus::IsError(m_candidates.Add(candidate)))
	{
		OP_DELETE(candidate);
		return OpStatus::ERR;
	}

	OP_DBG(("name:'%S';ALT:'%S';POS:(%d,%d)->(%d,%d)=>im:%d;pos:%d;sum:%d", image_path.CStr(), image_alt.CStr(), op_rect.x, op_rect.y, op_rect.width, op_rect.height, image_score, pos_score, final_score));
	return OpStatus::OK;
}

ThumbnailLogoFinder::LogoCandidate *ThumbnailLogoFinder::GetBestCandidate()
{
	OP_NEW_DBG("ThumbnailLogoFinder::GetBestCandidate()", "thumbnail-logo");

	// Looks the logo candidate that scored highest, returns NULL if no logo candidate is available
	LogoCandidate *cur = NULL, *best = NULL;

	for (UINT32 i=0; i<m_candidates.GetCount(); i++)
	{
		cur = m_candidates.Get(i);
		if (NULL == best)
		{
			best = cur;
			continue;
		}
		if (cur->score > best->score)
			best = cur;
	}
	if (best)
		OP_DBG(("Winner logo: POS:(%d,%d)x(%d,%d)=>score:%d", best->position.Left(), best->position.Top(), best->position.width, best->position.height, best->score));
	else
		OP_DBG(("No logo found"));
	return best;
}

int ThumbnailLogoFinder::ScoreImagePos(OpRect op_rect)
{
	int score = 0;

	// Should probably be hardcoded once we find the correct values
	const int logo_pos_max_x = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_POS_MAX_X);
	const int logo_pos_max_y = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_POS_MAX_Y);
	const int logo_size_min_x = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SIZE_MIN_X);
	const int logo_size_min_y = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SIZE_MIN_Y);
	const int logo_size_max_x = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SIZE_MAX_X);
	const int logo_size_max_y = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SIZE_MAX_Y);
	const int logo_score_x = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_X);
	const int logo_score_y = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_Y);

	long x = op_rect.x, y = op_rect.y, w = op_rect.width, h = op_rect.height;

	// Check if the image fits the basic size and position limits
	if (x < 0 || y < 0 || op_rect.IsEmpty() ||
		x > logo_pos_max_x || y > logo_pos_max_y ||
		w < logo_size_min_x || h < logo_size_min_y ||
		w > logo_size_max_x || h >logo_size_max_y)
		return -1;

	// Score the image based on its position
	score += ((logo_pos_max_x - x) * logo_score_x) / logo_pos_max_x;
	score += ((logo_pos_max_y - y) * logo_score_y) / logo_pos_max_y;

	return score;
}

int ThumbnailLogoFinder::ScoreImage(const OpStringC &image_path, const OpStringC &image_alt)
{
	int score = 0;

	// Should probably be hardcoded once we find the correct values
	const int logo_score_banner = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_BANNER);
	const int logo_score_logo_url = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_LOGO_URL);
	const int logo_score_logo_alt = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_LOGO_ALT);
	const int logo_score_site_url = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_SITE_URL);
	const int logo_score_site_alt = OpThumbnail::GetSettingValue(OpThumbnail::LOGO_SCORE_SITE_ALT);

	// Penalty for the word "banner" in image address or image ALT
	if (image_path.FindI("banner") != KNotFound || image_alt.FindI("banner") != KNotFound)
		score -= logo_score_banner;

	// Score for the word "logo" in image address
	if (image_path.FindI("logo") != KNotFound)
		return score + logo_score_logo_url;

	// Score for the word "logo" in image ALT
	if (image_alt.FindI("logo") != KNotFound)
		return score + logo_score_logo_alt;

	// Score for site name in image address
	if (image_alt.FindI(m_site_name) != KNotFound)
		return score + logo_score_site_alt;

	// Score for site name in image ALT
	if (image_path.FindI(m_site_name) != KNotFound)
		return score + logo_score_site_url;

	// The image failed to score anything here
	return score;
}

OP_STATUS ThumbnailLogoFinder::CheckHTMLElementIMG(HTML_Element* element)
{
	OP_ASSERT(element);

	// Only return error on OOM, we don't really care otherwise.
	if (m_frm_doc->GetLogicalDocument() &&
		element && element->GetLayoutBox() && element->GetLayoutBox()->GetContent() && element->GetLayoutBox()->GetContent()->IsImage())
	{
		RECT rect;
		URL url = element->GetImageURL(TRUE, m_frm_doc->GetLogicalDocument());
		OpString alt_text;
		RETURN_IF_ERROR(alt_text.Set(element->GetIMG_Alt()));
		OpString url_text;
		RETURN_IF_ERROR(url.GetAttribute(URL::KPath, url_text));
		Box* box = element->GetLayoutBox();
		if (!box || !box->GetRect(m_frm_doc, BORDER_BOX, rect))
			return OpStatus::OK;

		OpRect op_rect;
		op_rect.x = rect.left;
		op_rect.y = rect.top;
		op_rect.width = rect.right - rect.left;
		op_rect.height = rect.bottom - rect.top;

		return AddImageCandidate(url_text, alt_text, op_rect);
	}
	return OpStatus::OK;
}

OP_STATUS ThumbnailLogoFinder::CheckHTMLElementCSS(HTML_Element* element)
{
	OP_ASSERT(element);

	HEListElm *list_element = element->GetHEListElm(TRUE);
	if (!list_element)
		return OpStatus::OK;

	UrlImageContentProvider *provider = list_element->GetUrlContentProvider();
	if (!provider)
		return OpStatus::OK;

	URL *purl = provider->GetUrl();
	if (!purl)
		return OpStatus::OK;

	OpString url_text;
	RETURN_IF_ERROR(purl->GetAttribute(URL::KPath, url_text));

	RECT rect;
	Box* box = element->GetLayoutBox();
	if (!box || !box->GetRect(m_frm_doc, BORDER_BOX, rect))
		return OpStatus::OK;

	OpRect op_rect;
	op_rect.x = rect.left;
	op_rect.y = rect.top;
	op_rect.width = rect.right - rect.left;
	op_rect.height = rect.bottom - rect.top;

	OpString empty_alt_text;
	return AddImageCandidate(url_text, empty_alt_text, op_rect);
}

#endif // defined(CORE_THUMBNAIL_SUPPORT) && defined(THUMBNAILS_LOGO_FINDER)
