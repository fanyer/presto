/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dochand/viewportlisteners.h"
#include "modules/debug/debug.h"
#include "modules/dochand/win.h"

// #define DEBUG_VIEWPORT_LISTENER

/**
 * This macro can be used to add an OpRect to the OP_DBG debug
 * statement. You need to add the debug string formatter
 * "(%d,%d)+%dx%d" at the correct place to use this macro.
 *
 * Example:
 * \code
 *   OpRect my_rect = ...;
 *   OP_DBG((UNI_L("rectangle: (%d,%d)+%dx%d"), _DBG_OP_RECT(my_rect)));
 * \endcode
 * @param rect is a GogiRect instance.
 */
#define _DBG_OP_RECT(rect) rect.x, rect.y, rect.width, rect.height

void DefaultViewportRequestListener::AdjustRenderingViewport(OpViewportController* controller, const OpRect& new_visual_viewport)
{
	OP_NEW_DBG("DefaultViewportRequestListener::AdjustRenderingViewport", "viewport.listener");
	OP_DBG(("new visual viewport: (%d,%d)+%dx%d", _DBG_OP_RECT(new_visual_viewport)));
	// Move rendering viewport if it no longer contains the visual viewport

	OpRect rendering_viewport = controller->GetRenderingViewport();
	OpPoint new_rendering_pos(rendering_viewport.x, rendering_viewport.y);

	BOOL visual_viewport_wider = rendering_viewport.width < new_visual_viewport.width;
	BOOL visual_viewport_higher = rendering_viewport.height < new_visual_viewport.height;

	/* If visual viewport is wider we need to make an opposite comparison
	   (adjust only if the rendering viewport is outside the visual one.
	   The same concerns every edge. */

	if (visual_viewport_wider ? new_visual_viewport.x > rendering_viewport.x : new_visual_viewport.x < rendering_viewport.x)
		new_rendering_pos.x = new_visual_viewport.x;
	else
	{
		INT32 right_edge_diff = new_visual_viewport.Right() - rendering_viewport.Right();
		if (visual_viewport_wider ? right_edge_diff < 0 : right_edge_diff > 0)
			new_rendering_pos.x = new_visual_viewport.Right() - rendering_viewport.width;
	}

	if (visual_viewport_higher ? new_visual_viewport.y > rendering_viewport.y : new_visual_viewport.y < rendering_viewport.y)
		new_rendering_pos.y = new_visual_viewport.y;
	else
	{
		INT32 bottom_edge_diff = new_visual_viewport.Bottom() - rendering_viewport.Bottom();
		if (visual_viewport_higher ? bottom_edge_diff < 0 : bottom_edge_diff > 0)
			new_rendering_pos.y = new_visual_viewport.Bottom() - rendering_viewport.height;
	}

	if (new_rendering_pos.x != rendering_viewport.x || new_rendering_pos.y != rendering_viewport.y)
	{
		unsigned int doc_width, doc_height;
		controller->GetDocumentSize(&doc_width, &doc_height);

		// Keep the new rendering viewport within the document's bounds.

		if (new_rendering_pos.x + rendering_viewport.width > (int) doc_width)
			new_rendering_pos.x = doc_width - rendering_viewport.width;

		if (new_rendering_pos.y + rendering_viewport.height > (int) doc_height)
			new_rendering_pos.y = doc_height - rendering_viewport.height;

		if (new_rendering_pos.x < 0)
			new_rendering_pos.x = 0;

		if (new_rendering_pos.y < 0)
			new_rendering_pos.y = 0;

		OP_DBG(("new rendering viewport position: (%d,%d)", new_rendering_pos.x, new_rendering_pos.y));
		controller->SetRenderingViewportPos(new_rendering_pos);
	}
}

void DefaultViewportRequestListener::OnVisualViewportChangeRequest(OpViewportController* controller, const OpRect& viewport, const OpRect& priority_rect, OpViewportChangeReason reason, DirectionHint* dir_hint)
{
	OP_NEW_DBG("DefaultViewportRequestListener::OnVisualViewportChangeRequest", "viewport.listener");
	OP_DBG(("viewport=(%d,%d)+%dx%d priority_rect=(%d,%d)+%dx%d reason=%d", _DBG_OP_RECT(viewport), _DBG_OP_RECT(priority_rect), reason));

	OpRect new_viewport_adjusted = viewport;

	if (controller->IsVisualViewportSizeLocked())
	{
		/* If visual viewport size is locked, simply ignore the (new) viewport
		   dimension. Perhaps this can be improved to honor the DirectionHint. */

		OpRect cur_visual_viewport = controller->GetVisualViewport();
		new_viewport_adjusted.width = cur_visual_viewport.width;
		new_viewport_adjusted.height = cur_visual_viewport.height;
	}

	controller->SetBufferedMode(TRUE);
	AdjustRenderingViewport(controller, new_viewport_adjusted);
	controller->SetVisualViewport(new_viewport_adjusted);
	controller->SetBufferedMode(FALSE);
}

void DefaultViewportRequestListener::OnVisualViewportEdgeChangeRequest(OpViewportController* controller, const ViewportPosition& pos, OpViewportChangeReason reason, DirectionHint* dir_hint)
{
	OP_NEW_DBG("DefaultViewportRequestListener::OnVisualViewportEdgeChangeRequest", "viewport.listener");
	OP_DBG(("viewport position=(%d,%d) %s-%s reason=%d", pos.point.x, pos.point.y, (pos.IsTopEdge() ? "top":"bottom"), (pos.IsLeftEdge() ? "left":"right"), reason));

	OpRect new_visual_viewport = controller->GetVisualViewport();

	if (pos.hor_edge == EDGE_LEFT)
		new_visual_viewport.x = pos.point.x;
	else
		new_visual_viewport.x = pos.point.x - (new_visual_viewport.width - 1);

	if (pos.vert_edge == EDGE_TOP)
		new_visual_viewport.y = pos.point.y;
	else
		new_visual_viewport.y = pos.point.y - (new_visual_viewport.height - 1);

	controller->SetBufferedMode(TRUE);
	AdjustRenderingViewport(controller, new_visual_viewport);
	controller->SetVisualViewportPos(OpPoint(new_visual_viewport.x, new_visual_viewport.y));
	controller->SetBufferedMode(FALSE);
}

#ifdef DOC_SEND_POIMOVED
void DefaultViewportRequestListener::OnPOIMoved(OpViewportController* controller, const POIData& element_data)
{
	OpRect new_visual_viewport = controller->GetVisualViewport();
	new_visual_viewport.x += element_data.movement.x;
	new_visual_viewport.y += element_data.movement.y;

	controller->SetBufferedMode(TRUE);
	AdjustRenderingViewport(controller, new_visual_viewport);
	controller->SetVisualViewportPos(OpPoint(new_visual_viewport.x, new_visual_viewport.y));
	controller->SetBufferedMode(FALSE);
}
#endif // DOC_SEND_POIMOVED

void DefaultViewportRequestListener::OnZoomLevelChangeRequest(OpViewportController* controller, double zoom_level, const OpRect* priority_rect, OpViewportChangeReason reason)
{
	OP_NEW_DBG("DefaultViewportRequestListener::OnZoomLevelChangeRequest", "viewport.listener");
	OP_DBG(("requested zoom-level: %f", zoom_level));
	switch (reason) {
		// Ignore the initial zoom-level specified in a viewport
		// meta-tag:
	case VIEWPORT_CHANGE_REASON_NEW_PAGE: break;

	case VIEWPORT_CHANGE_REASON_INPUT_ACTION:
		// execute the request for any input action like
		// ctrl+mouse-wheel:
		m_commander->SetScale(static_cast<int>(100 * zoom_level + 0.5));
		// note: if this listener is used, then the visual viewport is
		// locked to the window-size, so there is no need to adjust
		// the size of the visual viewport - and the priority_rect is
		// currently ignored...
		break;

		// ignore any other reason...
	}
}

void NullViewportInfoListener::OnNewPage(OpViewportController* controller, OpViewportChangeReason reason, int id)
{
	OP_NEW_DBG("NullViewportInfoListener::OnNewPage", "viewport.listener");
}

void NullViewportInfoListener::OnNewPageReady(OpViewportController* controller)
{
	OP_NEW_DBG("NullViewportInfoListener::OnNewPageReady", "viewport.listener");
}

void NullViewportInfoListener::OnZoomLevelLimitsChanged(OpViewportController* controller, double min_zoom_level, double max_zoom_level, BOOL user_zoomable)
{
	OP_NEW_DBG("NullViewportInfoListener::OnZoomLevelLimitsChanged", "viewport.listener");
	OP_DBG(("min: %f; max: %f; user_zoomable: %d", min_zoom_level, max_zoom_level, user_zoomable));
}

void NullViewportInfoListener::OnLayoutViewportSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height)
{
	OP_NEW_DBG("NullViewportInfoListener::OnLayoutViewportSizeChanged", "viewport.listener");
	OP_DBG(("layout viewport size: %dx%d", width, height));
}

void NullViewportInfoListener::OnDocumentSizeChanged(OpViewportController* controller, unsigned int width, unsigned int height)
{
	OP_NEW_DBG("NullViewportInfoListener::OnDocumentSizeChanged", "viewport.listener");
	OP_DBG(("document size: %dx%d", width, height));
}

void NullViewportInfoListener::OnDocumentContentChanged(OpViewportController* controller, CONTENT_CHANGED_REASON reason)
{
	OP_NEW_DBG("NullViewportInfoListener::OnDocumentContentChanged", "viewport.listener");
}

#ifdef CONTENT_MAGIC

void NullViewportInfoListener::OnContentMagicFound(OpViewportController* controller, const OpRect& content_magic_rect)
{
	OP_NEW_DBG("NullViewportInfoListener::OnContentMagicFound", "viewport.listener");
	OP_DBG(("content-magic: (%d,%d)+%dx%d", _DBG_OP_RECT(content_magic_rect)));
}

#endif // CONTENT_MAGIC

void NullViewportInfoListener::OnReservedRegionChanged(OpViewportController* controller, const OpRect* rect_array, unsigned int num_rects)
{
	OP_NEW_DBG("NullViewportInfoListener::OnReservedRegionChanged", "viewport.listener");
	OP_DBG(("%d rects", num_rects));
}

#ifdef ADAPTIVE_ZOOM_SUPPORT
void NullViewportInfoListener::OnAreaOfInterestChanged(OpViewportController* controller, const OpRect& primary_rect, const OpRect& secondary_rect)
{
	OP_NEW_DBG("NullViewportInfoListener::OnAreaOfInterestChanged", "viewport.listener.aoi");
	OP_DBG(("primary: (%d,%d)+%dx%d; secondary: (%d,%d)+%dx%d", _DBG_OP_RECT(primary_rect), _DBG_OP_RECT(secondary_rect)));
}
#endif // ADAPTIVE_ZOOM_SUPPORT

void NullViewportInfoListener::OnTrueZoomBaseScaleChanged(OpViewportController* controller, unsigned int scale_percentage)
{
	OP_NEW_DBG("NullViewportInfoListener::OnTrueZoomBaseScaleChanged", "viewport.listener");
	OP_DBG(("base scale: %d", scale_percentage));
}

#ifdef PAGED_MEDIA_SUPPORT
void NullViewportInfoListener::OnPageChanged(int new_page_number, int new_page_count)
{
	OP_NEW_DBG("NullViewportInfoListener::OnPageChanged", "viewport.listener");
	OP_DBG(("new_page_number: %d; new_page_count: %d", new_page_number, new_page_count));
}
#endif // PAGED_MEDIA_SUPPORT

#undef _DBG_OP_RECT
