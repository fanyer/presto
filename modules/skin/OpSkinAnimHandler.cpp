/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef ANIMATED_SKIN_SUPPORT

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinAnimHandler.h"

// == OpSkinAnimationHandler ===========================================================

OpSkinAnimationHandler::OpSkinAnimationHandler() 
	: m_element(NULL)
	, m_image_id(SKINPART_TILE_CENTER)
	, m_vd(NULL)
	, m_posted_message(FALSE)
	, m_init_called(FALSE)
	, m_waiting_for_next_frame(FALSE)
	, m_ref_count(1)
{
	Into(g_opera->skin_module.m_image_animation_handlers);
}

OpSkinAnimationHandler::~OpSkinAnimationHandler()
{
	OpSkinElement::StateElementRef* elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
	OpSkinElement::StateElementRef* next;
	while(elmref)
	{
		next = (OpSkinElement::StateElementRef*) elmref->Suc();

		elmref->Out();
		
		Image image = elmref->m_elm->GetImage(elmref->m_image_id);
		if(!image.IsEmpty())
		{
			image.DecVisible(elmref->m_elm);
		}
		
		OP_DELETE(elmref);

		elmref = next;
	}

	g_main_message_handler->RemoveDelayedMessage(MSG_ANIMATE_IMAGE, (MH_PARAM_1) this, 0);
	g_main_message_handler->UnsetCallBacks(this);
	Out();
}

OpSkinAnimationHandler* 
OpSkinAnimationHandler::GetImageAnimationHandler(OpSkinElement::StateElement *element, VisualDevice *vd, SkinPart image_id)
{
	OpSkinAnimationHandler* ah = NULL;

	ah = (OpSkinAnimationHandler*) g_opera->skin_module.m_image_animation_handlers->First();
	while(ah)
	{
		if (ah->m_element == element && ah->m_image_id == image_id)
			break;
		ah = (OpSkinAnimationHandler*) ah->Suc();
	}
	if (ah == NULL)
	{
		ah = OP_NEW(OpSkinAnimationHandler, ());
		if (!ah)
			return NULL;

		OpSkinElement::StateElementRef* elmref = OP_NEW(OpSkinElement::StateElementRef, (element, vd, image_id));
		if (!elmref)
		{
			OP_DELETE(ah);
			return NULL;
		}
		ah->m_element = element;
		ah->m_vd = vd;
		ah->m_image_id = image_id;
		if (OpStatus::IsError(ah->AddListener(elmref)))
		{
			OP_DELETE(ah);
			OP_DELETE(elmref);
			return NULL;
		}
	}
	else
	{
		ah->IncRef();
	}
	return ah;
}

void OpSkinAnimationHandler::IncRef()
{
	OP_ASSERT(m_ref_count >= 1);
	m_ref_count++;
}

void OpSkinAnimationHandler::DecRef(OpSkinElement::StateElement* elm, VisualDevice *vd, SkinPart image_id)
{
	OP_ASSERT(m_ref_count >= 1);
	m_ref_count--;
	if (m_ref_count == 0)
	{
		OpSkinElement::StateElementRef* elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
		while(elmref)
		{
			if (elmref->m_elm == elm && 
				elmref->m_vd == vd && 
				elmref->m_image_id == image_id)
			{
				elmref->Out();
				
				Image image = elmref->m_elm->GetImage(elmref->m_image_id);
				if(!image.IsEmpty())
				{
					image.DecVisible(elmref->m_elm);
				}
				
				OP_DELETE(elmref);
				break;
			}
			elmref = (OpSkinElement::StateElementRef*) elmref->Suc();
		}

		OP_DELETE(this);
	}
}

OP_STATUS OpSkinAnimationHandler::AddListener(OpSkinElement::StateElementRef* elmref)
{
	OpSkinElement::StateElementRef* running_elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
	if (running_elmref)
	{
		// If we have a image in the list, we should synchronize to it.
		Image image = elmref->m_elm->GetImage(running_elmref->m_image_id);
		if(!image.IsEmpty())
		{
			image.SyncronizeAnimation(elmref->m_elm, running_elmref->m_elm);
		}
	}

	Image image = elmref->m_elm->GetImage(elmref->m_image_id);
	if(!image.IsEmpty())
	{
		image.IncVisible(elmref->m_elm);
	}

	elmref->Into(&m_elm_list);
	if (!m_init_called)
	{
		AnimateToNext();

		if (!g_main_message_handler->HasCallBack(this, MSG_ANIMATE_IMAGE, (MH_PARAM_1) this))
			if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_ANIMATE_IMAGE, (MH_PARAM_1) this)))
			{
				elmref->Out();
				return OpStatus::ERR_NO_MEMORY;
			}

		m_init_called = TRUE;
	}
	return OpStatus::OK;
}
	
void
OpSkinAnimationHandler::OnPortionDecoded(OpSkinElement::StateElement* elm)
{
	if (m_waiting_for_next_frame && m_init_called)
	{
		// We are waiting for the next frame. Try again:

		OpSkinElement::StateElementRef* elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
		if (elm != elmref->m_elm)
			return; // Avoid starting the animation several times (Only one of the synchronized images should).

		Image image = elmref->m_elm->GetImage(elmref->m_image_id);
		INT32 duration = image.GetCurrentFrameDuration(elmref->m_elm);
		if (duration != -1)
		{
			// We have got info about the next frame, so go ahead and animate it.
			m_waiting_for_next_frame = FALSE;
			AnimateToNext();
		}
	}
}

void OpSkinAnimationHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_ANIMATE_IMAGE)
		return;
	
	OP_ASSERT(m_posted_message);
	m_posted_message = FALSE;

	OpSkinElement::StateElementRef* elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
    while(elmref)
	{
		Image image = elmref->m_elm->GetImage(elmref->m_image_id);
		BOOL not_loaded_yet = !image.Animate(elmref->m_elm);
		if (not_loaded_yet)
		{
			m_waiting_for_next_frame = TRUE;
			return;
		}
		OP_ASSERT(elmref->m_elm->m_parent_skinelement);

		if(elmref->m_elm->m_parent_skinelement)
		{
			elmref->m_elm->m_parent_skinelement->OnAnimatedImageChanged();
		}
		elmref = (OpSkinElement::StateElementRef*) elmref->Suc();
	}
	AnimateToNext();
}

void
OpSkinAnimationHandler::AnimateToNext()
{
	OpSkinElement::StateElementRef* elmref = (OpSkinElement::StateElementRef*) m_elm_list.First();
	Image image = elmref->m_elm->GetImage(elmref->m_image_id);
	INT32 duration = image.GetCurrentFrameDuration(elmref->m_elm);
	if (duration == -1)
	{
		m_waiting_for_next_frame = TRUE;
	}
	else
	{
		OP_ASSERT(!m_posted_message);
		g_main_message_handler->PostDelayedMessage(MSG_ANIMATE_IMAGE, (MH_PARAM_1) this, 0, duration * 10);
		m_posted_message = TRUE;
	}
}

#endif // ANIMATED_SKIN_SUPPORT
