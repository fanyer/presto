/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT
#ifdef SVG_SUPPORT_MEDIA

#include "modules/svg/src/svgmediamanager.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGTimedElementContext.h"

#ifdef PI_VIDEO_LAYER
#include "modules/layout/layout_workplace.h"
#endif // PI_VIDEO_LAYER

OP_STATUS SVGMediaManager::AddBinding(HTML_Element* elm, const URL& url, Window* window)
{
	SVGMediaBinding* mbind = NULL;
	OP_STATUS status = m_bindings.GetData(elm, &mbind);
	if (OpStatus::IsError(status)) // Create new stream
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if (!doc_ctx)
			return OpStatus::ERR;

		MediaPlayer* new_player;
		RETURN_IF_ERROR(MediaPlayer::Create(new_player, url, doc_ctx->GetURL(), elm->Type() == Markup::SVGE_VIDEO, window));

		mbind = OP_NEW(SVGMediaBinding, (elm, new_player));
		if (!mbind)
		{
			OP_DELETE(new_player);
			return OpStatus::ERR_NO_MEMORY;
		}

		new_player->SetListener(mbind);

		status = m_bindings.Add(elm, mbind);
		if (OpStatus::IsError(status))
			OP_DELETE(mbind);
	}
	return status;
}

OP_STATUS SVGMediaManager::RemoveBinding(HTML_Element* elm)
{
	SVGMediaBinding* mbind = NULL;
	OP_STATUS status = m_bindings.Remove(elm, &mbind);
	OP_DELETE(mbind);
	return status;
}

SVGMediaBinding* SVGMediaManager::GetBinding(HTML_Element* elm)
{
	SVGMediaBinding* mbind = NULL;
	RETURN_VALUE_IF_ERROR(m_bindings.GetData(elm, &mbind), NULL);
	return mbind;
}

#ifdef PI_VIDEO_LAYER
OP_STATUS SVGMediaManager::ClearOverlays(VisualDevice* visdev)
{
	OP_ASSERT(visdev->IsPainting());

	if (m_bindings.GetCount() == 0)
		return OpStatus::OK;

	OpHashIterator* iter = m_bindings.GetIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	COLORREF prev_color = visdev->GetColor();
	UINT8 red, green, blue, alpha;

	for (OP_STATUS status = iter->First();
		 OpStatus::IsSuccess(status); status = iter->Next())
	{
		// Check if this binding is associated with the VisualDevice
		HTML_Element* elm = reinterpret_cast<HTML_Element*>(const_cast<void*>(iter->GetKey()));
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if (!doc_ctx || doc_ctx->GetVisualDevice() != visdev)
			continue;

		// Clear the Video Layer rect, if any.
		SVGMediaBinding* bind = reinterpret_cast<SVGMediaBinding*>(iter->GetData());
		const OpRect& canvas_rect = bind->GetOverlayRect();
		if (!canvas_rect.IsEmpty())
		{
			bind->GetPlayer()->GetVideoLayerColor(&red, &green, &blue, &alpha);
			visdev->SetColor(red, green, blue, alpha);
			OpRect doc_rect = visdev->ScaleToDoc(canvas_rect);
			visdev->ClearRect(doc_rect);
		}
	}

	visdev->SetColor(prev_color);

	OP_DELETE(iter);

	return OpStatus::OK;
}
#endif // PI_VIDEO_LAYER

SVGMediaBinding::~SVGMediaBinding()
{
	OP_DELETE(m_player);
#ifdef PI_VIDEO_LAYER
	Out();
#endif // PI_VIDEO_LAYER
}

void SVGMediaBinding::OnVideoResize(MediaPlayer* player)
{
	OP_ASSERT(m_player == player);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm);
	if (!doc_ctx)
		return;

	OpStatus::Ignore(SVGDynamicChangeHandler::HandleAttributeChange(doc_ctx, m_elm,
																	Markup::SVGA_WIDTH, NS_SVG,
																	FALSE));
}

void SVGMediaBinding::OnFrameUpdate(MediaPlayer* player)
{
	OP_ASSERT(m_player == player);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm);
	if (!doc_ctx)
		return;

	OpStatus::Ignore(SVGDynamicChangeHandler::RepaintElement(doc_ctx, m_elm));
}

void SVGMediaBinding::OnPlaybackEnd(MediaPlayer* player)
{
	OP_ASSERT(m_player == player);

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm);
	if (!doc_ctx)
		return;

	if (SVGTimingInterface* timed_element_context = AttrValueStore::GetSVGTimingInterface(m_elm))
		if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
			if (timed_element_context->AnimationSchedule().IsActive(animation_workplace->DocumentTime()))
				OpStatus::Ignore(player->Play());
}

void SVGMediaBinding::OnDurationChange(MediaPlayer* player)
{
	OP_ASSERT(m_player == player);

	SVGTimingInterface* timed_element_context = AttrValueStore::GetSVGTimingInterface(m_elm);
	OP_ASSERT(timed_element_context);
	if (!timed_element_context)
		return;

	SVG_ANIMATION_TIME old_intr_dur = timed_element_context->GetIntrinsicDuration();
	double duration;
	OpStatus::Ignore(player->GetDuration(duration));
	SVG_ANIMATION_TIME new_intr_dur;

	if (!op_isfinite(duration) || duration < 0)
		new_intr_dur = SVGAnimationTime::Indefinite();
	else
		new_intr_dur = SVGAnimationTime::FromSeconds(duration);

	if (old_intr_dur == new_intr_dur)
		return;

	timed_element_context->SetIntrinsicDuration(new_intr_dur);

	if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm))
		if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
			animation_workplace->UpdateTimingParameters(m_elm);
}

#ifdef PI_VIDEO_LAYER
// Find the element's closest CoreView
static CoreView* GetView(HTML_Element* element, VisualDevice* visdev)
{
	if (CoreView* core_view = LayoutWorkplace::FindParentCoreView(element))
		return core_view;

	return visdev->GetView();
}

BOOL SVGMediaBinding::UpdateOverlay(const OpRect& canvas_rect, const OpRect& clipped_canvas_rect)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm);
	if (!doc_ctx)
		return FALSE;

	VisualDevice* visdev = doc_ctx->GetVisualDevice();
	if (!visdev)
		return FALSE;

	if (!InList())
		visdev->GetView()->AddScrollListener(this);

	m_overlay_rect = clipped_canvas_rect;

	const AffinePos& doc_ctm = doc_ctx->GetSVGImage()->GetDocumentPos();

	// Calculate common offsets
	OpPoint scaled_svg_origo(0, 0);
	doc_ctm.Apply(scaled_svg_origo);
	scaled_svg_origo = visdev->ScaleToScreen(scaled_svg_origo);
	OpPoint screen_origo(0, 0);
	screen_origo = visdev->GetOpView()->ConvertToScreen(screen_origo);

	OpRect scaled_doc_rect = canvas_rect;
	OpRect scaled_clipped_doc_rect = clipped_canvas_rect;

	// SVG canvas coordinates -> scaled document coordinates
	scaled_doc_rect.OffsetBy(scaled_svg_origo);
	scaled_clipped_doc_rect.OffsetBy(scaled_svg_origo);

	// Scaled document coordinates -> global screen coordinates
	OpRect screen_rect = visdev->OffsetToContainerAndScroll(scaled_doc_rect);
	screen_rect.OffsetBy(screen_origo);
	OpRect clipped_screen_rect = visdev->OffsetToContainerAndScroll(scaled_clipped_doc_rect);
	clipped_screen_rect.OffsetBy(screen_origo);

	// Calculate clip rect
	OpRect clip_rect = GetView(m_elm, visdev)->GetVisibleRect();
	clip_rect.OffsetBy(screen_origo);
	clip_rect.IntersectWith(clipped_screen_rect);

	// Notify TvManager
	if (clip_rect.IsEmpty())
	{
		DisableOverlay();
		return FALSE;
	}
	else
	{
		m_player->SetVideoLayerRect(screen_rect, clip_rect);
		return TRUE;
	}
}

void SVGMediaBinding::DisableOverlay()
{
	m_overlay_rect.Empty();

	// Pass the empty rectangles to let the player know there's nothing to paint
	OpRect empty_screen_rect, empty_clip_rect;
	m_player->SetVideoLayerRect(empty_screen_rect, empty_clip_rect);
}

void SVGMediaBinding::OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	// We would need to keep track of a lot of state to be able to
	// calculate new display and clip rects when not painting.
	// The suboptimal solution is to trigger a repaint...
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_elm);
	if (doc_ctx)
		OpStatus::Ignore(SVGDynamicChangeHandler::RepaintElement(doc_ctx, m_elm));
}

void SVGMediaBinding::OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	OnScroll(view, dx, dy, reason);
}
#endif // PI_VIDEO_LAYER

#endif // SVG_SUPPORT_MEDIA
#endif // SVG_SUPPORT
