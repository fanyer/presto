/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_MEDIA_MANAGER_H
#define SVG_MEDIA_MANAGER_H

#ifdef SVG_SUPPORT_MEDIA

#include "modules/util/OpHashTable.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url2.h"

#include "modules/doc/externalinlinelistener.h"

#include "modules/media/mediaplayer.h"

#ifdef PI_VIDEO_LAYER
#include "modules/display/coreview/coreview.h"
#endif // PI_VIDEO_LAYER

class SVGMediaBinding :
	public MediaPlayerListener
#ifdef PI_VIDEO_LAYER
	, private CoreViewScrollListener
#endif // PI_VIDEO_LAYER
{
public:
	SVGMediaBinding(HTML_Element* elm, MediaPlayer* mp)
		: m_elm(elm), m_player(mp) {}
	virtual ~SVGMediaBinding();

	MediaPlayer* GetPlayer() { return m_player; }

	// MediaPlayerListener
	virtual void OnDurationChange(MediaPlayer* player);
	virtual void OnVideoResize(MediaPlayer* player);
	virtual void OnFrameUpdate(MediaPlayer* player);
	virtual void OnPlaybackEnd(MediaPlayer* player);

#ifdef PI_VIDEO_LAYER
	/** Enable/update the Video Layer overlay and notifying the platform.
	 * @param canvas_rect video area in SVG canvas coordinates
	 * @return TRUE if overlay is visible/active/enabled
	 */
	BOOL UpdateOverlay(const OpRect& canvas_rect, const OpRect& clipped_canvas_rect);
	/** Disable the Video Layer rect and notify the platform. */
	void DisableOverlay();
	/** @return the Video Layer rect (empty if Video Layer is disabled). */
	const OpRect& GetOverlayRect() const { return m_overlay_rect; }

	// CoreViewScrollListener
	virtual void OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);
	virtual void OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);
#endif // PI_VIDEO_LAYER

private:
	HTML_Element* m_elm;
	MediaPlayer* m_player;

#ifdef PI_VIDEO_LAYER
	/** Video area in SVG canvas coordintes,
	 *  empty if Video Layer isn't in use. */
	OpRect m_overlay_rect;
#endif // PI_VIDEO_LAYER
};

class SVGMediaManager
{
public:
	OP_STATUS AddBinding(HTML_Element* elm, const URL& url, Window* window);
	OP_STATUS RemoveBinding(HTML_Element* elm);

	SVGMediaBinding* GetBinding(HTML_Element* elm);

#ifdef PI_VIDEO_LAYER
	/** Clear all Video Layer overlays associated with visdev */
	OP_STATUS ClearOverlays(VisualDevice* visdev);
#endif // PI_VIDEO_LAYER

private:
	OpPointerHashTable<HTML_Element, SVGMediaBinding> m_bindings;
};

#endif // SVG_SUPPORT_MEDIA
#endif // SVG_MEDIA_MANAGER_H
