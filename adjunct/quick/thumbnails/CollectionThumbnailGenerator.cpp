/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/thumbnails/CollectionThumbnailGenerator.h"
#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick/Application.h"

#include "modules/display/pixelscaler.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/skin/OpSkinManager.h"

#define MAX_NUM_OF_THUMBNAILS_REQUEST			5
#define ASPECT_RATIO	((double)g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ThumbnailAspectRatioX)/g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ThumbnailAspectRatioY))
#define BASE_HEIGHT		((double)THUMBNAIL_WIDTH / ASPECT_RATIO)

CollectionThumbnailGenerator::CollectionThumbnailGenerator(CollectionViewPane& view_pane_manager)
	: m_collection_view_pane(view_pane_manager)
	, m_num_of_req_initiated(0)
	, m_base_height(BASE_HEIGHT)
	, m_base_width(THUMBNAIL_WIDTH)
{
	UpdateThumbnailSize(DEFAULT_ZOOM);
}

OP_STATUS CollectionThumbnailGenerator::Init()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GENERATE_THUMBNAILS, 0));
	return g_main_message_handler->SetCallBack(this, MSG_COLLECTION_THUMBNAIL_GENERATED, 0);
}

CollectionThumbnailGenerator::~CollectionThumbnailGenerator()
{
	RemoveMessages();
	g_main_message_handler->UnsetCallBacks(this);
	DeleteAllThumbnails();
}

void CollectionThumbnailGenerator::GenerateThumbnail(INT32 item_id, bool reload /*= false*/)
{
	AddIntoGenerateList(item_id, reload);
	g_main_message_handler->RemoveDelayedMessage(MSG_GENERATE_THUMBNAILS, 0, 0);
	g_main_message_handler->PostDelayedMessage(MSG_GENERATE_THUMBNAILS, 0, 0, 0);
}

void CollectionThumbnailGenerator::CancelThumbnails()
{
	RemoveMessages();
	DeleteAllThumbnails();
}

void CollectionThumbnailGenerator::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
		case MSG_GENERATE_THUMBNAILS:
			 GenerateThumbnails();
			 return;

		case MSG_COLLECTION_THUMBNAIL_GENERATED:  //Spinner
			 RemoveProcessedThumbnailRequest();
			 g_main_message_handler->RemoveDelayedMessage(MSG_GENERATE_THUMBNAILS, 0, 0);
			 g_main_message_handler->PostDelayedMessage(MSG_GENERATE_THUMBNAILS, 0, 0, 0);
			 break;
	}
}

void CollectionThumbnailGenerator::AddIntoGenerateList(INT32 item_id, bool reload)
{
	URL_InUse url_inuse;
	if (!m_item_id_to_thumbnail_listener.Contains(item_id) &&
		OpStatus::IsSuccess(m_collection_view_pane.GetURL(item_id, url_inuse)) &&
		!url_inuse.GetURL().IsEmpty())
	{
		OpAutoPtr<CollectionThumbnailListener>thumbnail_listener(OP_NEW(CollectionThumbnailListener, (*this, item_id, url_inuse.GetURL(), reload)));
		if (thumbnail_listener.get() && OpStatus::IsSuccess(m_item_id_to_thumbnail_listener.Add(item_id, thumbnail_listener.get())))
			thumbnail_listener.release();
	}
}

void CollectionThumbnailGenerator::GenerateThumbnails()
{
	OpHashIterator* it = m_item_id_to_thumbnail_listener.GetIterator();
	if (it)
	{
		Image img;

		for (OP_STATUS s = it->First(); OpStatus::IsSuccess(s); s = it->Next())
		{
			if (m_num_of_req_initiated >= MAX_NUM_OF_THUMBNAILS_REQUEST)
				break;

			CollectionThumbnailListener* thumbnail_listener = static_cast<CollectionThumbnailListener*>(it->GetData());
			OP_ASSERT(thumbnail_listener);

			if (thumbnail_listener->HasRequestProcessed() || thumbnail_listener->HasRequestInitiated())
				continue;

			if (GetFixedImage(thumbnail_listener->GetURL(), img) && img.GetBitmap(&m_null_img_listener))
			{
				NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_SUCCESSFUL, reinterpret_cast<INTPTR>(it->GetKey()), img, true);
				img.ReleaseBitmap();
				img.Empty();
				m_num_of_req_initiated++;
				continue;
			}

			g_thumbnail_manager->SetThumbnailSize(m_base_width, m_base_height, MAX_ZOOM);

			if (OpStatus::IsError(thumbnail_listener->Initiate()))
			{
				Image img;
				NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_FAILED, reinterpret_cast<INTPTR>(it->GetKey()), img);
				continue;
			}

			m_num_of_req_initiated++;
		}

		OP_DELETE(it);
	}
}

void CollectionThumbnailGenerator::RemoveProcessedThumbnailRequest()
{
	CollectionThumbnailListener* thumbnail_listener;
	for (int idx = m_thumbnail_req_processed_item_list.GetCount() - 1; idx >=0; idx--)
	{
		if (m_num_of_req_initiated)
			m_num_of_req_initiated--;

		if (OpStatus::IsSuccess(m_item_id_to_thumbnail_listener.Remove(m_thumbnail_req_processed_item_list.Get(idx), &thumbnail_listener)))
			OP_DELETE(thumbnail_listener);
	}

	m_thumbnail_req_processed_item_list.Clear();
}

void CollectionThumbnailGenerator::DeleteThumbnail(INT32 id)
{
	CollectionThumbnailListener* thumbnail_listener;
	if (OpStatus::IsSuccess(m_item_id_to_thumbnail_listener.Remove(id, &thumbnail_listener)))
	{
		OP_DELETE(thumbnail_listener);

		if (m_num_of_req_initiated)
			m_num_of_req_initiated--;
	}
}

void CollectionThumbnailGenerator::DeleteAllThumbnails()
{
	m_item_id_to_thumbnail_listener.DeleteAll();

	m_thumbnail_req_processed_item_list.Clear();

	m_num_of_req_initiated =  0;
}

void CollectionThumbnailGenerator::RemoveMessages()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_COLLECTION_THUMBNAIL_GENERATED, 0, 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_GENERATE_THUMBNAILS, 0, 0);
}

void CollectionThumbnailGenerator::NotifyThumbnailStatus(ThumbnailStatus status, INT32 id, Image& thumbnail, bool has_fixed_image)
{
	switch(status)
	{
		case STATUS_STARTED:
			 return;

		case STATUS_SUCCESSFUL:
			 m_collection_view_pane.ThumbnailReady(id, thumbnail, has_fixed_image);
			 break;

		case STATUS_FAILED:
			 m_collection_view_pane.ThumbnailFailed(id);
			 break;
	}

	OpStatus::Ignore(m_thumbnail_req_processed_item_list.Add(id));

	g_main_message_handler->RemoveDelayedMessage(MSG_COLLECTION_THUMBNAIL_GENERATED, 0, 0);
	g_main_message_handler->PostMessage(MSG_COLLECTION_THUMBNAIL_GENERATED, 0, 0);
}

const char* CollectionThumbnailGenerator::GetSpecialURIImage(URLType type) const
{
	switch(type)
	{
		case URL_ABOUT:
			 return "URI About Thumbnail Icon";

		case URL_FILE:
			 return "URI File Thumbnail Icon";

		case URL_Gopher:
			 return "URI Gopher Thumbnail Icon";

		case URL_JAVASCRIPT:
			 return "URI Javascript Thumbnail Icon";

		case URL_MAILTO:
			 return "URI Mailto Thumbnail Icon";

		case URL_NEWS:
			 return "URI News Thumbnail Icon";

		case URL_TELNET:
			 return "URI Telnet Thumbnail Icon";

		case URL_TEL:
			 return "URI Tel Thumbnail Icon";

		case URL_OPERA:
			 return "URI Opera Thumbnail Icon";

		case URL_UNKNOWN:
			 return "URI Unknown Thumbnail Icon";
	}

	return NULL;
}

bool CollectionThumbnailGenerator::IsSpecialURL(URL& url)
{
	URLType url_type = (URLType) url.GetAttribute(URL::KType);
	if (url_type > URL_UNKNOWN)
		url_type = URL_UNKNOWN;

	return GetSpecialURIImage(url_type) != NULL;
}

bool CollectionThumbnailGenerator::GetFixedImage(URL& url, Image& img) const
{
	URLType url_type = (URLType) url.GetAttribute(URL::KType);
	if (url_type > URL_UNKNOWN)
		url_type = URL_UNKNOWN;

	const char* image_name = GetSpecialURIImage(url_type);
	RETURN_VALUE_IF_NULL(image_name, false);

	OpString url_str;
	if (url_type == URL_OPERA && OpStatus::IsSuccess(url.GetURLDisplayName(url_str)))
		DocumentView::GetThumbnailImage(url_str.CStr(), img);

	if (img.IsEmpty())
	{
		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(image_name);
		if (skin_elm)
			img = skin_elm->GetImage(0);
	}

	return true;
}

CollectionThumbnailGenerator::CollectionThumbnailListener::CollectionThumbnailListener(CollectionThumbnailGenerator& generator,
	INT32 id,
	URL url,
	bool reload /*= false*/)
	: m_thumbnail_generator(generator)
	, m_url(url)
	, m_id(id)
	, m_req_initiated(false)
	, m_req_processed(false)
	, m_reload_thumbnail(reload)
{

}

OP_STATUS CollectionThumbnailGenerator::CollectionThumbnailListener::Initiate()
{
	if (m_url.IsEmpty())
		return OpStatus::ERR;

	if (!m_req_initiated)
	{
		RETURN_IF_ERROR(g_thumbnail_manager->AddThumbnailRef(m_url));
		if (OpStatus::IsError(g_thumbnail_manager->AddListener(this)))
		{
			g_thumbnail_manager->DelThumbnailRef(m_url);
			return OpStatus::ERR;
		}
	}

	if (m_req_processed)
		RETURN_IF_ERROR(g_thumbnail_manager->AddListener(this));

	m_req_initiated = true;

	OP_STATUS status = GenerateThumbnail();
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(g_thumbnail_manager->RemoveListener(this));
		OpStatus::Ignore(g_thumbnail_manager->DelThumbnailRef(m_url));

		m_req_initiated = false;
		m_req_processed = false;
	}

	return status;
}

CollectionThumbnailGenerator::CollectionThumbnailListener::~CollectionThumbnailListener()
{
	if (m_url.IsEmpty())
		return;

	if (g_thumbnail_manager && m_req_initiated)
	{
		OpStatus::Ignore(g_thumbnail_manager->RemoveListener(this));
		OpStatus::Ignore(g_thumbnail_manager->CancelThumbnail(m_url));
	}
}

void CollectionThumbnailGenerator::CollectionThumbnailListener::OnThumbnailRequestStarted(const URL& url, BOOL reload)
{
	if (m_url.Id() != url.Id())
		return;

	m_thumbnail_generator.NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_STARTED, m_id, m_image);
}

void CollectionThumbnailGenerator::CollectionThumbnailListener::OnThumbnailReady(const URL& url, const Image& thumbnail, const uni_char *title, long preview_refresh)
{
	if (m_url.Id() != url.Id())
		return;

	m_image = thumbnail;

	OnPortionDecoded();

	OpStatus::Ignore(g_thumbnail_manager->RemoveListener(this));

	m_req_processed = true;
}

void CollectionThumbnailGenerator::CollectionThumbnailListener::OnThumbnailFailed(const URL& url, OpLoadingListener::LoadingFinishStatus status)
{
	if (m_url.Id() != url.Id())
		return;

	m_thumbnail_generator.NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_FAILED, m_id, m_image);

	OpStatus::Ignore(g_thumbnail_manager->RemoveListener(this));

	m_req_processed = true;
}

void CollectionThumbnailGenerator::CollectionThumbnailListener::OnPortionDecoded()
{
	if (!m_image.ImageDecoded())
	{
		m_thumbnail_generator.NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_FAILED, m_id, m_image);
		return;
	}

	OpBitmap* buffer = m_image.GetBitmap(this);
	OP_ASSERT(buffer != NULL);

	m_thumbnail_generator.NotifyThumbnailStatus(CollectionThumbnailGenerator::STATUS_SUCCESSFUL, m_id, m_image);

	if (buffer != NULL)
		m_image.ReleaseBitmap();
}

OP_STATUS CollectionThumbnailGenerator::CollectionThumbnailListener::GenerateThumbnail()
{
	if (m_url.IsEmpty())
		return OpStatus::ERR;

	unsigned int target_height, target_width;
	m_thumbnail_generator.GetThumbnailSize(target_width, target_height);
	if (!target_width || !target_height)
		return OpStatus::ERR;

	OpThumbnail::ThumbnailMode mode = OpThumbnail::ViewPort;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	INT32 scale = static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor();
	PixelScaler scaler(scale);
	target_width = TO_DEVICE_PIXEL(scaler, target_width);
	target_height = TO_DEVICE_PIXEL(scaler, target_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	return g_thumbnail_manager->RequestThumbnail(m_url, FALSE, m_reload_thumbnail, mode, target_width, target_height);
}
