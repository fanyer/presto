/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_H
#define MEDIA_H

#ifdef MEDIA_SUPPORT

#include "modules/logdoc/htm_elm.h"

class Media : public ComplexAttr
{
public:
	Media(HTML_Element* elm) : ComplexAttr(), m_element(elm) {}

	virtual ~Media() {}

	/** The aspect ratio adjusted width of the video. */
	virtual UINT32 GetIntrinsicWidth() = 0;

	/** The aspect ratio adjusted height of the video. */
	virtual UINT32 GetIntrinsicHeight() = 0;

	/** Is this image-like content?
	 *
	 * If so, it may not be painted when images are turned off.
	 */
	virtual BOOL IsImageType() const = 0;

	/** Paint the current video frame, if any.
	 *
	 * Aspect ratio and positioning is controlled by MediaContent::Paint.
	 *
	 * @param vis_dev	the current visual device.
	 * @param video	position and size of the video.
	 * @param content	position and size of the content box. Used to position controls at the bottom of the box.
	 */
	virtual OP_STATUS Paint(VisualDevice* vis_dev, OpRect video, OpRect content) = 0;

	/** Handle mouse event. */
	virtual void OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y) = 0;

	/** Is the media element focusable? */
	virtual BOOL IsFocusable() const = 0;

	/** Focus next internal media control. */
	virtual BOOL FocusNextInternalTabStop(BOOL forward) = 0;

	/** Focus first internal media control when tab navigation enters element. */
	virtual void FocusInternalEdge(BOOL forward) = 0;

	/** Stop playback and free as many resources as possible.
	 *
	 * As much state as possible is saved so that Resume() results in
	 * the same user-visible state as if Suspend() was never called.
	 *
	 * @param removed If TRUE, the element has already been removed
	 * and we need not call FramesDocument::RemoveMedia.
	 */
	virtual void Suspend(BOOL removed) = 0;

	/** Resume state from before Suspend was called. */
	virtual OP_STATUS Resume() = 0;

	/** Is the media element's delaying-the-load-event flag set? */
	virtual BOOL GetDelayingLoad() const = 0;

	/** Should controls be displayed? */
	virtual BOOL ControlsNeeded() const = 0;

protected:
	HTML_Element* m_element;
};

#endif // MEDIA_SUPPORT

#endif //MEDIA_H
