/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/logdoc/helistelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/imganimhandler.h"
#include "modules/hardcore/mh/messages.h"

// == ImageAnimationHandler ===========================================================

#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > LOGDOC_MIN_ANIM_UPDATE_INTERVAL_HIGH
#undef LOGDOC_MIN_ANIM_UPDATE_INTERVAL_HIGH
#define LOGDOC_MIN_ANIM_UPDATE_INTERVAL_HIGH LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > LOGDOC_MIN_ANIM_UPDATE_INTERVAL_HIGH
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0

ImageAnimationHandler::ImageAnimationHandler()
	: m_doc(NULL)
	, m_posted_message(FALSE)
	, m_init_called(FALSE)
	, m_waiting_for_next_frame(FALSE)
	, m_ref_count(0)
	, m_syncronize_id(0)
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
	, m_duration_adjustment(0)
	, m_min_update_time(0)
	, m_was_previously_throttling(FALSE)
	, m_throttling_balancing(FALSE)
	, m_last_msg_delay_multiplier(1)
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
	, activity_img_anim(ACTIVITY_IMGANIM)
{
	Into(g_opera->logdoc_module.m_image_animation_handlers);
}

ImageAnimationHandler::~ImageAnimationHandler()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_ANIMATE_IMAGE, (INTPTR) this, 0);
	g_main_message_handler->UnsetCallBacks(this);
	Out();
}

ImageAnimationHandler*
ImageAnimationHandler::GetImageAnimationHandler(HEListElm* hle, FramesDocument* doc, BOOL syncronized)
{
#ifndef SUPPORT_ANIMATED_BACKGROUNDS
	if (hle->IsBgImageInline())
		return NULL;
#endif

	ImageAnimationHandler* ah = NULL;
	UINT32 syncronize_id = 0;

	if (syncronized)
	{
		syncronize_id = (UINTPTR) hle->GetUrlContentProvider(); // FIXME: not a unique ID unless sizeof(UINT32) >= sizeof(UrlImageContentProvider*) - use UINTPTR

		ah = (ImageAnimationHandler*) g_opera->logdoc_module.m_image_animation_handlers->First();
		while(ah)
		{
			if (ah->m_doc == doc && ah->m_syncronize_id == syncronize_id)
				break;
			ah = (ImageAnimationHandler*) ah->Suc();
		}
	}

	if (ah == NULL)
		ah = OP_NEW(ImageAnimationHandler, ());

	HEListElmRef* hleref = OP_NEW(HEListElmRef, (hle));
	if (ah == NULL || hleref == NULL)
	{
		if (!syncronized)
			OP_DELETE(ah);
		OP_DELETE(hleref);
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return NULL;
	}
	ah->m_syncronize_id = syncronize_id;
	ah->IncRef();
	if (OpStatus::IsError(ah->AddListener(hleref, doc)))
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	return ah;
}

void
ImageAnimationHandler::IncRef()
{
	m_ref_count++;
}

void
ImageAnimationHandler::DecRef(HEListElm* hle)
{
	HEListElmRef* hleref = (HEListElmRef*) m_hle_list.First();
	while(hleref)
	{
		if (hleref->hle == hle)
		{
			hleref->Out();
			OP_DELETE(hleref);
			break;
		}
		hleref = (HEListElmRef*) hleref->Suc();
	}

	m_ref_count--;
	if (m_ref_count == 0)
	{
		OP_DELETE(this);
	}
}

OP_STATUS
ImageAnimationHandler::AddListener(HEListElmRef* hleref, FramesDocument* doc)
{
	HEListElmRef* running_hleref = (HEListElmRef*) m_hle_list.First();
	if (running_hleref)
	{
		// If we have a image in the list, we should syncronize to it.
		Image image = hleref->hle->GetImage();
		image.SyncronizeAnimation(hleref->hle, running_hleref->hle);
	}

	hleref->Into(&m_hle_list);
	if (!m_init_called)
	{
		m_doc = doc;
		AnimateToNext();

		if (!g_main_message_handler->HasCallBack(this, MSG_ANIMATE_IMAGE, (INTPTR) this))
			if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_ANIMATE_IMAGE, (INTPTR) this)))
				return OpStatus::ERR_NO_MEMORY;

		m_init_called = TRUE;
	}

	return OpStatus::OK;
}

void
ImageAnimationHandler::OnPortionDecoded(HEListElm* hle)
{
	if (m_waiting_for_next_frame && m_init_called)
	{
		// We are waiting for the next frame. Try again:

		HEListElmRef* hleref = (HEListElmRef*) m_hle_list.First();
		if (hle != hleref->hle)
			return; // Avoid starting the animation several times (Only one of the syncronized images should).

		Image image = hleref->hle->GetImage();
		INT32 duration = image.GetCurrentFrameDuration(hleref->hle);
		if (duration != -1)
		{
			// We have got info about the next frame, so go ahead and animate it.
			m_waiting_for_next_frame = FALSE;
			AnimateToNext();
		}
	}
}

void ImageAnimationHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_ANIMATE_IMAGE)
		return;

	OP_ASSERT(m_posted_message);
	m_posted_message = FALSE;

#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
	INT32 duration_adjustment = m_duration_adjustment;
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0

	HEListElmRef* hleref = (HEListElmRef*) m_hle_list.First();
    while(hleref)
	{
		Image image = hleref->hle->GetImage();
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
		/* Skip to the next suitable frame. */
		INT32 duration = image.GetCurrentFrameDuration(hleref->hle);
		INT32 sum_duration = (op_abs(duration) * 10) - duration_adjustment;
		for( ; sum_duration <= m_min_update_time * m_last_msg_delay_multiplier;
            duration = image.GetCurrentFrameDuration(hleref->hle),
			duration = ((duration == 0) ? 1 : duration), /* make sure sum_duration will continue to increase even when frame with duration 0 is encountered */
			sum_duration += (op_abs(duration) * 10))
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
		{
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
			if (duration == -1)
			{
				if (sum_duration <= m_min_update_time * m_last_msg_delay_multiplier)
					m_doc->UpdateAnimatedRect(hleref->hle);
			}
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
			BOOL not_loaded_yet = !image.Animate(hleref->hle);
			if (not_loaded_yet)
			{
				m_waiting_for_next_frame = TRUE;
				return;
			}
		}
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
		m_duration_adjustment = (duration * 10) - (sum_duration - m_min_update_time);
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0

#ifdef LOGDOC_DONT_ANIM_OUT_VISUAL_VIEWPORT
		if (m_doc->GetVisualViewport().Intersecting(hleref->hle->GetImageBBox()))
#endif // LOGDOC_DONT_ANIM_OUT_VISUAL_VIEWPORT
			m_doc->UpdateAnimatedRect(hleref->hle);

		hleref = (HEListElmRef*) hleref->Suc();
	}
	AnimateToNext();
}

void
ImageAnimationHandler::AnimateToNext()
{
    BOOL animate = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowAnimation, m_doc->GetURL().GetAttribute(URL::KUniHostName, TRUE).CStr());

	if (!animate)
		return;

	HEListElmRef* hleref = (HEListElmRef*) m_hle_list.First();
	Image image = hleref->hle->GetImage();
	INT32 duration = image.GetCurrentFrameDuration(hleref->hle);
	if (duration == -1)
		m_waiting_for_next_frame = TRUE;
	else
	{
		OP_ASSERT(!m_posted_message);
		INT32 message_delay = duration * 10;

#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
		if (g_opera->logdoc_module.IsThrottlingNeeded(m_throttling_balancing))
		{
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
			if (!m_doc->GetTopDocument()->IsLoaded())
			{
				m_last_msg_delay_multiplier = (duration * 10 / LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING) + 1;
				message_delay = m_last_msg_delay_multiplier * LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING;
				m_min_update_time = LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING;
			}
			else
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING
			{
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
				if (m_was_previously_throttling)
				{
					m_min_update_time = MIN(m_min_update_time + LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW / 2, LOGDOC_MIN_ANIM_UPDATE_INTERVAL_HIGH);
				}
				else
				{
					m_min_update_time = LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW;
				}
				m_last_msg_delay_multiplier = (duration * 10 / m_min_update_time) + 1;
				message_delay = m_last_msg_delay_multiplier * m_min_update_time;
				
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0

				m_was_previously_throttling = TRUE;
			}
		}
		else
		{
			if (m_was_previously_throttling && !m_throttling_balancing && m_doc->GetTopDocument()->IsLoaded())
			{
				m_throttling_balancing = TRUE;
				m_min_update_time = MAX(m_min_update_time - LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW / 2, LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW);
				m_last_msg_delay_multiplier = (duration * 10 / m_min_update_time) + 1;
				message_delay = m_last_msg_delay_multiplier * m_min_update_time;
			}
			else
			{
				m_was_previously_throttling = FALSE;
				m_throttling_balancing = FALSE;
				m_min_update_time = message_delay;
				m_last_msg_delay_multiplier = 1;
			}
		}
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0

		g_main_message_handler->PostDelayedMessage(MSG_ANIMATE_IMAGE, (INTPTR) this, 0, message_delay);
		m_posted_message = TRUE;
	}
}
