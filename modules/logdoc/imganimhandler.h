/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef IMGANIMHANDLER_H
#define IMGANIMHANDLER_H

#include "modules/hardcore/timer/optimer.h"
#include "modules/idle/idle_detector.h"

class HEListElm;
class FramesDocument;

#include "modules/util/simset.h"

class HEListElmRef;

/**
	Handles animation of images. It will handle several image_listeners with the same timer to keep them syncronized.
	Several instances of the same imageurl should default be syncronized within the same document, but not for different documents.
	But when replacing a url by script, it should no longer be syncronized to the other in the same document.
*/

class ImageAnimationHandler : public Link, public MessageObject
{
public:
	ImageAnimationHandler();
	~ImageAnimationHandler();

	static ImageAnimationHandler* GetImageAnimationHandler(HEListElm* hle, FramesDocument* doc, BOOL syncronized);

	OP_STATUS AddListener(HEListElmRef* hleref, FramesDocument* doc);

	void IncRef();
	void DecRef(HEListElm* hle);

	void OnPortionDecoded(HEListElm* hle);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	void AnimateToNext();

	List<HEListElmRef> m_hle_list;
	FramesDocument* m_doc;
	BOOL m_posted_message;
	BOOL m_init_called;
	BOOL m_waiting_for_next_frame;
	INT32 m_ref_count;
	UINT32 m_syncronize_id;
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0
	/** Stores duration time adjustment value in milliseconds (as update may hit somwhere in the middle of a frame -
	 * not its beginning as without throttling - frame duration has to be adjusted with the offset
	 * taken into account).
	 */
	INT32 m_duration_adjustment;
	/** Minimal update interval (in milliseconds) */
	INT32 m_min_update_time;
	/** Flag indicating whether trottling was on when last checked */
	BOOL m_was_previously_throttling;
	/** Flag indicating whether thottling is tried to be balanced */
	BOOL m_throttling_balancing;
	/** If frame duration time is longer then minimal interval, to not to trigger update just for painting the same frame once again,
	 *  minimal interval is multiplied until next frame is reached. This variable stores minimal interval multiplier value.
	 */
	BYTE m_last_msg_delay_multiplier;
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0

	/** Keeps a local 'is active' state for all imganimhandlers. Used to detect if opera is idle, important for testing */
	OpAutoActivity activity_img_anim;
};

#endif // IMGANIMHANDLER_H
