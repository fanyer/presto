/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef VIEWPORTLISTENERS_H
#define VIEWPORTLISTENERS_H

#include "modules/windowcommander/OpViewportController.h"

class OpWindowCommander;

class DefaultViewportRequestListener : public OpViewportRequestListener
{
private:
	OpWindowCommander* m_commander;
	void AdjustRenderingViewport(OpViewportController* controller, const OpRect& new_visual_viewport);

public:
	DefaultViewportRequestListener(OpWindowCommander* commander) : m_commander(commander) {}
	virtual ~DefaultViewportRequestListener() {}

	// Implementing OpViewportRequestListener:
	void OnVisualViewportChangeRequest(OpViewportController* controller, const OpRect& viewport, const OpRect& priority_rect, OpViewportChangeReason reason, DirectionHint* dir_hint = NULL);
	void OnVisualViewportEdgeChangeRequest(OpViewportController* controller, const ViewportPosition& pos, OpViewportChangeReason reason, DirectionHint* dir_hint = NULL);
#ifdef DOC_SEND_POIMOVED
	void OnPOIMoved(OpViewportController* controller, const POIData& element_data);
#endif // DOC_SEND_POIMOVED
	virtual void OnZoomLevelChangeRequest(OpViewportController* controller, double zoom_level, const OpRect* priority_rect, OpViewportChangeReason reason);
};

class NullViewportInfoListener : public OpViewportInfoListener
{
public:
	// Implementing OpViewportInfoListener:
	void OnNewPage(OpViewportController* controller, OpViewportChangeReason reason, int id);
	void OnNewPageReady(OpViewportController* controller);
	void OnZoomLevelLimitsChanged(OpViewportController* controller, double min_zoom_level, double max_zoom_level, BOOL user_zoomable);
	void OnLayoutViewportSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height);
	void OnDocumentSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height);
	void OnDocumentContentChanged(OpViewportController* controller, CONTENT_CHANGED_REASON reason);
#ifdef CONTENT_MAGIC
	void OnContentMagicFound(OpViewportController* controller, const OpRect& content_magic_rect);
#endif // CONTENT_MAGIC
	void OnReservedRegionChanged(OpViewportController* controller, const OpRect* rect_array, unsigned int num_rects);
#ifdef ADAPTIVE_ZOOM_SUPPORT
	void OnAreaOfInterestChanged(OpViewportController* controller, const OpRect& primary_rect, const OpRect& secondary_rect);
#endif // ADAPTIVE_ZOOM_SUPPORT
	void OnTrueZoomBaseScaleChanged(OpViewportController* controller, unsigned int scale_percentage);
#ifdef PAGED_MEDIA_SUPPORT
	void OnPageChanged(int new_page_number, int new_page_count);
#endif // PAGED_MEDIA_SUPPORT
};

#endif // VIEWPORTLISTENERS_H
