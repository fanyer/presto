/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SKINANIMHANDLER_H
#define SKINANIMHANDLER_H

#ifdef ANIMATED_SKIN_SUPPORT

#include "modules/hardcore/timer/optimer.h"
#include "modules/util/simset.h"
#include "modules/skin/OpSkinElement.h"

/**
	Handles animation of skin images
*/

class OpSkinAnimationHandler : public Link, public MessageObject
{
private:
	OpSkinAnimationHandler();

public:
	~OpSkinAnimationHandler();

	static OpSkinAnimationHandler* GetImageAnimationHandler(OpSkinElement::StateElement* element, VisualDevice *vd, SkinPart image_id);

	OP_STATUS AddListener(OpSkinElement::StateElementRef* elmref);

	void IncRef();
	void DecRef(OpSkinElement::StateElement* elm, VisualDevice *vd, SkinPart image_id);

	void OnPortionDecoded(OpSkinElement::StateElement* elm);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	INT32 GetRefCount() { return m_ref_count; }

private:
	void AnimateToNext();

	OpSkinElement::StateElement* m_element;
	SkinPart m_image_id;
	VisualDevice *m_vd;
	Head m_elm_list;
	BOOL m_posted_message;
	BOOL m_init_called;
	BOOL m_waiting_for_next_frame;
	INT32 m_ref_count;
};
#endif // ANIMATED_SKIN_SUPPORT
#endif // SKINANIMHANDLER_H
