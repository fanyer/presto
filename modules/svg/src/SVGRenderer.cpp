/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/svgpaintnode.h"

#ifdef SVG_DEBUG_SCREEN_BOXES
#include "modules/svg/src/AttrValueStore.h"
#endif // SVG_DEBUG_SCREEN_BOXES

#include "modules/probetools/probepoints.h"

#define SVG_USE_UPTO_4_PAINTRECTS

#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
CONST_ARRAY(g_svg_errorstrings, uni_char*)
	CONST_ENTRY(UNI_L("Wrong number of arguments.")), // WRONG_NUMBER_OF_ARGUMENTS
	CONST_ENTRY(UNI_L("Invalid argument.")), // INVALID_ARGUMENT
	CONST_ENTRY(UNI_L("Bad parameter.")), // BAD_PARAMETER
	CONST_ENTRY(UNI_L("Missing required argument.")), // MISSING_REQ_ARGUMENT
	CONST_ENTRY(UNI_L("Attribute error.")), // ATTRIBUTE_ERROR
	CONST_ENTRY(UNI_L("Data not loaded.")), // DATA_NOT_LOADED_ERROR
	CONST_ENTRY(UNI_L("Not supported in SVG Tiny.")), // NOT_SUPPORTED_IN_TINY
	CONST_ENTRY(UNI_L("Not yet implemented error.")), // NOT_YET_IMPLEMENTED
	CONST_ENTRY(UNI_L("General document error.")), // GENERAL_DOCUMENT_ERROR
	CONST_ENTRY(UNI_L("Invalid animation.")), // INVALID_ANIMATION
	CONST_ENTRY(UNI_L("Type error.")), // TYPE_ERROR
	CONST_ENTRY(UNI_L("Internal error."))// INTERNAL_ERROR
#ifdef _DEBUG
	,CONST_ENTRY(UNI_L("Element is invisible")),
	CONST_ENTRY(UNI_L("Skip subtree")),
	CONST_ENTRY(UNI_L("Skip element")),
	CONST_ENTRY(UNI_L("Skip children"))
#endif // _DEBUG
CONST_END(g_svg_errorstrings)
#endif // defined(_DEBUG) || defined(SVG_LOG_ERRORS)

SVGRenderer::SVGRenderer()
: m_prev_result(NULL),
  m_doc_ctx(NULL),
  m_scale(1.0),
  m_config_init(0),
  m_listener(NULL)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	m_config.stopped = 1;
#endif // SVG_INTERRUPTABLE_RENDERING
}

SVGRenderer::~SVGRenderer()
{
	Abort();
	m_target.Free();
	OP_DELETE(m_prev_result);
}

void SVGRenderer::RenderTargetInfo::Free()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_DELETE(bitmap);
#endif // VEGA_OPPAINTER_SUPPORT
	VEGARenderTarget::Destroy(rendertarget);
}

unsigned int SVGRenderer::GetMemUsed() 
{ 
	return m_target.rendertarget ? m_target.rendertarget->getWidth() * m_target.rendertarget->getHeight() * 4 : 0;
}

void SVGInvalidState::Translate(int pan_x, int pan_y)
{
	m_areas.Translate(pan_x, pan_y);
	m_extra.OffsetBy(pan_x, pan_y);
}

OP_STATUS SVGInvalidState::Invalidate(const OpRect& rect)
{
	OpRect canvas_rect(m_dimension);
	canvas_rect.IntersectWith(rect);

	if (!canvas_rect.IsEmpty())
	{
		if (!m_areas.IncludeRect(canvas_rect))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		//m_areas.CoalesceRectsIfNeeded();
	}
	return OpStatus::OK;
}

void SVGInvalidState::AddExtraInvalidation(const OpRect& rect)
{
#ifdef SVG_OPTIMIZE_RENDER_MULTI_PASS
	Invalidate(rect);
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS

	m_extra.UnionWith(rect);
}

void SVGInvalidState::GetExtraInvalidation(OpRect& rect)
{
	// The canvas might have changed size since 'm_extra' was set
	m_extra.IntersectWith(m_dimension);

	rect.UnionWith(m_extra);
	m_extra.Empty();
}

OP_STATUS SVGInvalidState::Reset()
{
	m_areas.Empty();

	if (!m_areas.IncludeRect(m_dimension))
		return OpStatus::ERR_NO_MEMORY;

	m_extra.Empty();
	return OpStatus::OK;
}

#ifdef SVG_DEBUG_SCREEN_BOXES
/* static */
void SVGRenderer::PaintScreenBoxes(SVGPainter* painter, HTML_Element* root)
{
	HTML_Element* stop = static_cast<HTML_Element*>(root->NextSibling());
	HTML_Element* elm = root;
	while (elm != stop)
	{
		SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext(elm);
		if (elem_ctx)
		{
			const OpRect& screen_box = elem_ctx->GetScreenBox();
			if (!screen_box.IsEmpty())
			{
				canvas->SetStrokeColorRGB(255,128,0); // orange
	//			canvas->SetStrokeColorRGB(200,0,170); // purple
				canvas->SetFillOpacity(128);
				canvas->EnableFill(SVGCanvasState::USE_NONE);
				canvas->EnableStroke(SVGCanvasState::USE_COLOR);

				OpStatus::Ignore(painter->DrawRect(SVGNumber(screen_box.x), SVGNumber(screen_box.y),
												   SVGNumber(screen_box.width), SVGNumber(screen_box.height),
												   0, 0));
			}
		}
		elm = elm->Next();
	}
}
#endif // SVG_DEBUG_SCREEN_BOXES

#ifdef SVG_DEBUG_RENDER_RECTS
// Draws a border around rect, used to identify areas of interest
/* static */
void SVGRenderer::PaintDebugRect(SVGPainter* painter, const OpRect& rect, UINT32 color)
{
//	canvas->SetStrokeColorRGB(255,128,0); // orange
	canvas->SetStrokeColor(color);
	canvas->SetFillOpacity(25);
	canvas->EnableFill(SVGCanvasState::USE_NONE);
	canvas->EnableStroke(SVGCanvasState::USE_COLOR);

	OpStatus::Ignore(painter->DrawRect(SVGNumber(rect.x), SVGNumber(rect.y),
									   SVGNumber(rect.width), SVGNumber(rect.height),
									   0, 0));
}
#endif // SVG_DEBUG_RENDER_RECTS

#ifdef SVG_USE_UPTO_4_PAINTRECTS
static int GetBorderPos(const OpRect& rect, unsigned int border)
{
	OP_ASSERT(!rect.IsEmpty());
	OP_ASSERT(border < 4);
	switch(border)
	{
	case 0:
		return rect.x;
	case 1:
		return rect.y;
	case 2:
		return rect.x+rect.width-1;
	default:
		OP_ASSERT(FALSE);
		/* Fall-through */
	case 3:
		return rect.y+rect.height-1;
	}
}

static int GetAreaIncrease(const OpRect& target_rect, const OpRect& rect_to_add)
{
	OP_ASSERT(!target_rect.IsEmpty());
	int base_area = target_rect.width * target_rect.height;
	OpRect total_rect = target_rect;
	total_rect.UnionWith(rect_to_add);
	int new_area = total_rect.width * total_rect.height;
	OP_ASSERT(new_area >= base_area);
	return new_area-base_area;
}
#endif // SVG_USE_UPTO_4_PAINTRECTS

static OP_STATUS CoalesceNeighbouringRects(OpVector<OpRect>& unrendered_rects)
{
	// Just make a union of them all. Reduces the number of traverse passes
	// but causes extra pixel painting and maybe bigger traverses.
#ifndef SVG_USE_UPTO_4_PAINTRECTS
	unsigned int count = unrendered_rects.GetCount();
	if (count > 1)
	{
		for (unsigned int i = 1; i < count; i++)
		{
			OpRect* rect = unrendered_rects.Get(i);
			unrendered_rects.Get(0)->UnionWith(*rect);
			OP_DELETE(rect);
		}
        unrendered_rects.Remove(1, count-1);
	}
#else
	// This algorithm has 3 steps while trying to generate as small and as few rects as possible
	//
	// 1. Identify the 4 rects that are most left, up, right and
	//    right and put those at position 0-3 in the vector. ( O(4*n) )
	//
	// 2. For every rect 4-(number of rects-1) find the border
	//    rect it's closest to and add it to that one. The order
	//    of the rects can affect the result here. ( O(4*n) )
	//
	// 3. When we are down to 4 rects (at most) compare them to eachother
	//    and if taking the union of two rects would add at most 40000
	//    extra pixels, use the union of them. ( O(4*3 + 3*2 + 2*1) )
	unsigned int count = unrendered_rects.GetCount();
	if (count > 1)
	{
		// Sort the rects so that the first four contains rects from the left, top, right and bottom
		for (unsigned int state = 0; state < 4 && state < count; state++)
		{
			unsigned int current_extreme = state;
			int current_extreme_value = GetBorderPos(*unrendered_rects.Get(state), state);
			for (unsigned i = state+1; i < count; i++)
			{
				int extreme_value = GetBorderPos(*unrendered_rects.Get(i), state);
				if (state < 2 && extreme_value < current_extreme_value ||
					state >= 2 && extreme_value > current_extreme_value)
				{
					current_extreme = i;
					current_extreme_value = extreme_value;
				}
			}
			if (current_extreme != state)
			{
				// Swap them
				OpRect* temp = unrendered_rects.Get(state);
				unrendered_rects.Replace(state, unrendered_rects.Get(current_extreme));
				unrendered_rects.Replace(current_extreme, temp);
			}
		}

		if (count > 4)
		{
			for (unsigned int i = 4; i < count; i++)
			{
				// Add each extra rect to the one of the four first giving the least addition
				const OpRect& rect_to_add = *unrendered_rects.Get(i);
				int best_target = 0;
				int best_target_increase = GetAreaIncrease(*unrendered_rects.Get(0), rect_to_add);
				for (int candidate_target = 1; candidate_target < 4; candidate_target++)
				{
					int new_increase = GetAreaIncrease(*unrendered_rects.Get(candidate_target), rect_to_add);
					if (new_increase < best_target_increase)
					{
						best_target = candidate_target;
						best_target_increase = new_increase;
					}
				}

				unrendered_rects.Get(best_target)->UnionWith(rect_to_add);
				OP_DELETE(unrendered_rects.Get(i));
			}
			unrendered_rects.Remove(4, count-4);
			count = 4;
		}

		// Now we have at most 4 rects in the different four sides. Let's see if
		// we should concatenate them
		if (count > 1)
		{
			// We can do this at most 4 times
			BOOL did_concatenation;
			do
			{
				// Each run through this loop is at most 4*3 = 12 passes in the inner loop.
				did_concatenation = FALSE;
				int best_concat_target_1 = 0;
				int best_concat_target_2 = 0;
				int best_extra_overhead = INT_MAX;
				for (unsigned int rect_1 = 0; rect_1 < count-1; rect_1++)
				{
					const OpRect& rect1 = *unrendered_rects.Get(rect_1);
					int rect1_area = rect1.width*rect1.height;
					for (unsigned int rect_2 = rect_1+1; rect_2 < count; rect_2++)
					{
						const OpRect& rect2 = *unrendered_rects.Get(rect_2);
						int rect2_area = rect2.width*rect2.height;
						int sum_of_area = rect1_area + rect2_area;
						OpRect union_rect = rect1;
						union_rect.UnionWith(rect2);
						int union_area = union_rect.width*union_rect.height;
						int overhead = union_area - sum_of_area;
						if (overhead < best_extra_overhead)
						{
							best_extra_overhead = overhead;
							best_concat_target_1 = rect_1;
							best_concat_target_2 = rect_2;
						}
					}
				}

				if (best_extra_overhead < 40000)
				{
					// arbitrary number. 40000 pixels extra we won't mind if we can save an extra traversal overhead
					unrendered_rects.Get(best_concat_target_1)->UnionWith(*unrendered_rects.Get(best_concat_target_2));
					OP_DELETE(unrendered_rects.Get(best_concat_target_2));
					unrendered_rects.Remove(best_concat_target_2);
					count--;
					did_concatenation = TRUE;
				}
			}
			while (did_concatenation && count > 1);
		}
	}
#endif // SVG_USE_UPTO_4_PAINTRECTS
	return OpStatus::OK;
}

#ifdef SVG_OPTIMIZE_RENDER_MULTI_PASS
OP_STATUS SVGInvalidState::GetInvalidArea(const OpRect& area,
										  OpVector<OpRect>& area_list)
{
	m_areas.CoalesceRects(); // Does changes that doesn't change the region.
	RETURN_IF_ERROR(m_areas.GetArrayOfIntersectingRects(area, area_list));

	CoalesceNeighbouringRects(area_list);
	return OpStatus::OK;
}
#else
OP_STATUS SVGInvalidState::GetInvalidArea(const OpRect& area, OpRect& subarea)
{
	subarea = m_areas.GetUnionOfIntersectingRects(area);
	return OpStatus::OK;
}
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS

OP_STATUS SVGRenderer::Move(int pixel_pan_x, int pixel_pan_y)
{
	if (!m_target.rendertarget)
		return OpStatus::ERR;

	m_target_area.x += pixel_pan_x;
	m_target_area.y += pixel_pan_y;

	m_invalid.Translate(pixel_pan_x, pixel_pan_y);
	return OpStatus::OK;
}

void SVGRenderer::OnFinished()
{
	if (m_listener)
		m_listener->OnAreaDone(this, m_area);

#ifdef SVG_INTERRUPTABLE_RENDERING
	if (m_config.policy == POLICY_ASYNC)
	{
		OP_DELETE(m_prev_result);
		m_prev_result = NULL;

		g_main_message_handler->UnsetCallBacks(this);
		g_main_message_handler->RemoveDelayedMessage(MSG_SVG_CONTINUE_RENDERING_EVENT, (MH_PARAM_1)this, 0);
	}
#endif // SVG_INTERRUPTABLE_RENDERING
}

#ifdef SVG_INTERRUPTABLE_RENDERING
void SVGRenderer::OnPartial()
{
	if (m_config.partial && m_listener)
	{
		BOOL do_partial_blit =
			(g_op_time_info->GetRuntimeMS() - m_doc_ctx->GetTimeForLastScreenBlit()) > 2000;

		if (do_partial_blit)
			m_listener->OnAreaPartial(this, m_area);
	}
	// FIXME: Ditch previous result?
}
#endif // SVG_INTERRUPTABLE_RENDERING

void SVGRenderer::Validate()
{
	m_invalid.InvalidateAll();
	m_invalid.Validate(m_area);
}

OP_STATUS SVGRenderer::SyncPolicyHandler::Update(SVGRenderer* renderer)
{
	OP_ASSERT(renderer);

	OP_STATUS status = OpStatus::OK;
	while (renderer->HaveSubAreas() && OpStatus::IsSuccess(status))
	{
		const OpRect& unrendered_rect = renderer->GetSubArea();

		renderer->BeforeTraverse(unrendered_rect);

		status = renderer->m_doc_ctx->GetPaintNode()->Paint(&renderer->m_painter);
		status = renderer->AfterTraverse(unrendered_rect, status);

		// Done with this subarea
		renderer->EndSubArea();
	}

	if (OpStatus::IsError(status))
		renderer->Stop();

	renderer->Validate();

	renderer->OnFinished();

	return status;
}

#ifdef SVG_INTERRUPTABLE_RENDERING
OP_STATUS SVGRenderer::OnTimeout()
{
	// Post continue message
	if(!g_main_message_handler->HasCallBack(this, MSG_SVG_CONTINUE_RENDERING_EVENT, (MH_PARAM_1)this))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SVG_CONTINUE_RENDERING_EVENT,
															(MH_PARAM_1)this));
	}
	
	if (!g_main_message_handler->PostDelayedMessage(MSG_SVG_CONTINUE_RENDERING_EVENT, (MH_PARAM_1)this, 0, 1))
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS SVGRenderer::OnError()
{
	if (m_listener)
		return m_listener->OnError();
	return OpStatus::OK;
}

/* virtual */
void SVGRenderer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_SVG_CONTINUE_RENDERING_EVENT && par1 == (MH_PARAM_1)this)
	{
		OP_ASSERT(m_config.policy == POLICY_ASYNC);

		OP_STATUS status = m_async_backend.Update(this);
		if (status == OpSVGStatus::TIMED_OUT)
			OnPartial();
	}
}

OP_STATUS SVGRenderer::AsyncPolicyHandler::Update(SVGRenderer* renderer)
{
	OP_ASSERT(renderer);

	BOOL running = TRUE;
	OP_STATUS status = OpStatus::OK;
	while (running)
	{
		switch (m_state)
		{
		case AREA_SETUP:
			{
				if (!renderer->HaveSubAreas())
				{
					renderer->Validate();
					m_state = DONE;
					break;
				}

				m_state = AREA_RENDER;
			}
			break;

		case AREA_RENDER:
			{
				const OpRect& unrendered_rect = renderer->GetSubArea();

				OP_NEW_DBG("AsyncUpdate", "svg_invalid");
				OP_DBG(("Updating: (%d, %d, %d, %d)",
						unrendered_rect.x,unrendered_rect.y,
						unrendered_rect.width, unrendered_rect.height));

				renderer->BeforeTraverse(unrendered_rect);

				status = renderer->m_doc_ctx->GetPaintNode()->Paint(&renderer->m_painter);

				status = renderer->AfterTraverse(unrendered_rect, status);

				if (OpStatus::IsSuccess(status))
				{
					// Done with this subarea
					renderer->EndSubArea();
					m_state = AREA_SETUP;
				}
				else
				{
					running = FALSE;
					if (status == OpSVGStatus::TIMED_OUT)
						OpStatus::Ignore(renderer->OnTimeout());
					else
						OpStatus::Ignore(renderer->OnError());
				}

				if (renderer->HasPendingStop())
				{
					running = FALSE;
					renderer->Stop();
				}
			}
			break;

		case DONE:
			renderer->OnFinished();
			running = FALSE;
			break;
		}
	}

	return status;
}

void SVGRenderer::AsyncPolicyHandler::Reset()
{
	m_state = AREA_SETUP;
}
#endif // SVG_INTERRUPTABLE_RENDERING

void SVGRenderer::BeforeTraverse(const OpRect& rect)
{
	OP_ASSERT(!rect.IsEmpty());

	m_painter.BeginPaint(&m_renderer, m_target.rendertarget, m_target_area);
	m_painter.SetClipRect(rect);
	m_painter.Clear(0, &rect);
	m_painter.SetFlatness(SVGNumber(m_doc_ctx->GetRenderingQuality()) / 100);
#ifdef SVG_SUPPORT_FILTERS
	m_painter.EnableBackgroundLayers(m_doc_ctx->NeedsBackgroundLayers());
#endif // SVG_SUPPORT_FILTERS

#ifdef PI_VIDEO_LAYER
	m_painter.SetDocumentPos(m_doc_ctx->GetSVGImage()->GetDocumentPos());
#endif // PI_VIDEO_LAYER

	m_config.is_traversing = 1;
	m_config.deferred_stop = 0;
}

// Passing status here so that traversal-wrapping can be consistent
// and still handle timeouts
OP_STATUS SVGRenderer::AfterTraverse(const OpRect& rect, OP_STATUS status)
{
	m_config.is_traversing = 0;

#ifdef SVG_INTERRUPTABLE_RENDERING
	if (status == OpSVGStatus::TIMED_OUT)
		return status; // Will trigger a new paint
#endif // SVG_INTERRUPTABLE_RENDERING

	m_invalid.Validate(rect);

#ifdef SVG_DEBUG_RENDER_RECTS
	PaintDebugRect(&m_painter, rect);
#endif // SVG_DEBUG_RENDER_RECTS

#ifdef SVG_DEBUG_SCREEN_BOXES
	PaintScreenBoxes(&m_painter, m_doc_ctx->GetSVGRoot());
#endif // SVG_DEBUG_SCREEN_BOXES

	m_painter.EndPaint();

#if defined(SVG_LOG_ERRORS) && defined(SVG_LOG_ERRORS_RENDER) && defined(_DEBUG)
	if (OpStatus::IsError(status))
	{
		SVG_NEW_ERROR(m_doc_ctx->GetSVGRoot());
		SVG_REPORT_ERROR((UNI_L("%s"), OpSVGStatus::GetErrorString(status)));
	}
#endif // SVG_LOG_ERRORS && SVG_LOG_ERRORS_RENDER

	return status;
}

OP_STATUS SVGRenderer::CopyToBitmap(const OpRect& rect, OpBitmap **bitmap)
{
	if (!m_target.rendertarget)
		return OpStatus::ERR;

	// Copy result from the render target to the bitmap provided
	OpBitmap* bm = *bitmap;

	OpRect isect(rect);
	isect.IntersectWith(m_target_area);

	OpRect bm_area(rect.x, rect.y, bm->Width(), bm->Height());
	isect.IntersectWith(bm_area);

	if (isect.IsEmpty())
		return OpStatus::OK;

	isect.OffsetBy(-m_target_area.TopLeft());

	RETURN_IF_ERROR(m_target.rendertarget->copyToBitmap(bm, isect.width, isect.height, isect.x, isect.y));

	bm->ForceAlpha();
	*bitmap = bm;
	return OpStatus::OK;
}

OP_STATUS SVGRenderer::GetResult(OpRect& area, OpBitmap** bm)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	if (m_config.policy == POLICY_ASYNC &&
		m_async_backend.m_state != AsyncPolicyHandler::DONE)
	{
		if (area.Equals(m_area))
		{
			area.x = 0;
			area.y = 0;
		}
		else
		{
			// Ohh boy...
			OpRect part_area(area);
			part_area.IntersectWith(m_area);

			part_area.OffsetBy(-m_area.TopLeft());
			if (part_area.IsEmpty())
			{
				*bm = NULL;
				return OpStatus::OK;
			}

			area = part_area;
		}
		*bm = m_prev_result;
		return OpStatus::OK;
	}
#endif // SVG_INTERRUPTABLE_RENDERING
#ifdef VEGA_OPPAINTER_SUPPORT
	// Provide results directly from the render target
	OP_ASSERT(bm && *bm == NULL);

	OpRect isect(area);
	isect.IntersectWith(m_target_area);
	isect.OffsetBy(-m_target_area.TopLeft());

	if (isect.IsEmpty())
		return OpStatus::OK;

	area = isect;

	*bm = m_target.bitmap;
	return OpStatus::OK;
#else
	OP_STATUS status = CopyToBitmap(area, bm);
	area.x = 0;
	area.y = 0;
	return status;
#endif // VEGA_OPPAINTER_SUPPORT
}

#ifdef SVG_INTERRUPTABLE_RENDERING
OP_STATUS SVGRenderer::CreateBackupBitmap()
{
	OP_DELETE(m_prev_result);
	m_prev_result = NULL;

	OP_STATUS status = OpBitmap::Create(&m_prev_result, m_area.width, m_area.height,
										FALSE, TRUE, 0, 0, FALSE);
	RETURN_IF_ERROR(status);

	// There is a really high chance of copying garbage here...
	status = CopyToBitmap(m_area, &m_prev_result);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_prev_result);
		m_prev_result = NULL;
	}
	return status;
}
#endif // SVG_INTERRUPTABLE_RENDERING

OP_STATUS SVGRenderer::CreateRenderTarget(RenderTargetInfo& rtinfo, unsigned rt_width, unsigned rt_height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	RETURN_IF_ERROR(OpBitmap::Create(&rtinfo.bitmap, rt_width, rt_height, FALSE, TRUE, 0, 0, TRUE));
	OP_STATUS status = m_renderer.createBitmapRenderTarget(&rtinfo.rendertarget, rtinfo.bitmap);
	if (OpStatus::IsError(status))
		rtinfo.Free();
	return status;
#else
	return m_renderer.createIntermediateRenderTarget(&rtinfo.rendertarget, rt_width, rt_height);
#endif // VEGA_OPPAINTER_SUPPORT
}

OP_STATUS SVGRenderer::SetTarget(const OpRect& area)
{
	OpRect root_area(area);
	OP_STATUS status = OpStatus::OK;

	if (root_area.IsEmpty())
	{
		OP_ASSERT(!"Attempted to set an empty area");
		return status;
	}

	if (!m_target.rendertarget)
	{
		// No surface yet, so create one
		RETURN_IF_ERROR(m_renderer.Init(root_area.width, root_area.height));

		RenderTargetInfo rtinfo;
		RETURN_IF_ERROR(CreateRenderTarget(rtinfo, root_area.width, root_area.height));
		m_target = rtinfo;

		m_renderer.setRenderTarget(m_target.rendertarget);
		m_renderer.setClipRect(0, 0, root_area.width, root_area.height);
		m_renderer.clear(0, 0, root_area.width, root_area.height, 0);

		m_target_area = root_area;
		return status;
	}

	// Attempt to reuse the old surface as much as possible
	if (m_target_area.Contains(root_area))
		return OpStatus::OK;

	if (m_target_area.width >= root_area.width && m_target_area.height >= root_area.height)
	{
		// The same surface can be used, it just needs to be "repositioned"
		OpRect overlap(root_area.x - (m_target_area.width - root_area.width),
					   root_area.y - (m_target_area.height - root_area.height),
					   2 * m_target_area.width - root_area.width,
					   2 * m_target_area.height - root_area.height);
		overlap.IntersectWith(m_target_area);

		OpPoint old_area_offset = m_target_area.TopLeft();

		if (overlap.IsEmpty())
		{
			m_target_area.x = root_area.x;
			m_target_area.y = root_area.y;
		}
		else
		{
			m_target_area.x = MIN(overlap.x, root_area.x);
			m_target_area.y = MIN(overlap.y, root_area.y);
		}

		// Calculate the area that is shared between the new and the old
		OpRect common_area(overlap);
		common_area.IntersectWith(root_area);

		if (!common_area.IsEmpty())
		{
			m_renderer.setRenderTarget(m_target.rendertarget);
			m_renderer.setClipRect(0, 0, m_target.rendertarget->getWidth(), m_target.rendertarget->getHeight());

			OpStatus::Ignore(m_renderer.moveRect(common_area.x - old_area_offset.x,
												 common_area.y - old_area_offset.y,
												 common_area.width,
												 common_area.height,
												 old_area_offset.x - m_target_area.x,
												 old_area_offset.y - m_target_area.y));
		}
		return OpStatus::OK;
	}

	// Need to grow the VEGARenderer before creating a new (bigger)
	// render target, because otherwise it's not allowed (in the case
	// of a bitmap rt).
	status = m_renderer.Init(root_area.width, root_area.height);
	if (OpStatus::IsSuccess(status))
	{
		RenderTargetInfo new_rtinfo;
		status = CreateRenderTarget(new_rtinfo, root_area.width, root_area.height);

		if (OpStatus::IsSuccess(status))
		{
			m_renderer.setRenderTarget(new_rtinfo.rendertarget);

			// FIXME: Hmm, is this clear(...) really necessary - investigate...
			m_renderer.setClipRect(0, 0, root_area.width, root_area.height);
			m_renderer.clear(0, 0, root_area.width, root_area.height, 0);

			OpRect target_rect(m_target_area);
			target_rect.IntersectWith(root_area);

			if (!target_rect.IsEmpty())
			{
				VEGAFilter* merge;
				if (OpStatus::IsSuccess(m_renderer.createMergeFilter(&merge, VEGAMERGE_REPLACE)))
				{
					merge->setSource(m_target.rendertarget);

					VEGAFilterRegion region;
					region.dx = target_rect.x - root_area.x;
					region.dy = target_rect.y - root_area.y;
					region.sx = target_rect.x - m_target_area.x;
					region.sy = target_rect.y - m_target_area.y;
					region.width = target_rect.width;
					region.height = target_rect.height;

					OpStatus::Ignore(m_renderer.applyFilter(merge, region));

					OP_DELETE(merge);
				}
			}

			m_target.Free();

			m_target = new_rtinfo;
			m_target_area = root_area;
		}
		else
		{
			new_rtinfo.Free();
		}
	}
	return status;
}

OP_STATUS SVGRenderer::Setup(const OpRect& area)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	OP_ASSERT(m_config.policy == POLICY_SYNC ||
			  (m_config.policy == POLICY_ASYNC &&
			   m_async_backend.m_state == AsyncPolicyHandler::AREA_SETUP));
#endif // SVG_INTERRUPTABLE_RENDERING
	OP_ASSERT(!area.IsEmpty());
	m_area = area;

	RETURN_IF_ERROR(SetTarget(m_area));

#ifdef SVG_INTERRUPTABLE_RENDERING
	m_config.stopped = 0;

	if (m_config.policy == POLICY_ASYNC)
		OpStatus::Ignore(CreateBackupBitmap());
#endif // SVG_INTERRUPTABLE_RENDERING
	RETURN_IF_ERROR(SetupSubAreas(m_area));
	return OpStatus::OK;
}

OP_STATUS SVGRenderer::Update()
{
	OP_ASSERT(m_doc_ctx);

	// If we are going to paint, we should have handled all invalidations before.
#ifdef SVG_INTERRUPTABLE_RENDERING
	OP_ASSERT(!m_doc_ctx->IsInvalidationPending()
			  || (m_config.policy == POLICY_ASYNC &&
				  m_async_backend.m_state == AsyncPolicyHandler::DONE));
#else
	OP_ASSERT(!m_doc_ctx->IsInvalidationPending());
#endif // SVG_INTERRUPTABLE_RENDERING

	switch ((RendererPolicy)m_config.policy)
	{
	case POLICY_SYNC:
		return m_sync_backend.Update(this);
#ifdef SVG_INTERRUPTABLE_RENDERING
	case POLICY_ASYNC:
		return m_async_backend.Update(this);
#endif // SVG_INTERRUPTABLE_RENDERING
	default:
		OP_ASSERT(!"No such rendering policy");
	}
	return OpStatus::ERR;
}

void SVGRenderer::Stop()
{
	if (m_config.is_traversing)
	{
		// Deferring Stop,
		// it's not nice to yank the rug from under someones feet
		m_config.deferred_stop = 1;
		return;
	}

	ResetSubAreas();

#ifdef SVG_INTERRUPTABLE_RENDERING
	m_config.stopped = 1;

	if (m_listener)
		m_listener->OnStopped();

	if (m_config.policy == POLICY_ASYNC)
	{
		m_painter.Reset();
		m_async_backend.Reset();

		g_main_message_handler->UnsetCallBacks(this);
		g_main_message_handler->RemoveDelayedMessage(MSG_SVG_CONTINUE_RENDERING_EVENT, (MH_PARAM_1)this, 0);
	}
#endif // SVG_INTERRUPTABLE_RENDERING
}

OP_STATUS SVGRenderer::ScaleChanged(int content_width, int content_height,
									int scaled_width, int scaled_height,
									float scale)
{
	OP_ASSERT(m_doc_ctx);
	OP_ASSERT(!IsActive());
	OP_ASSERT(scaled_width > 0 && scaled_height > 0);

	m_scale = scale;

	m_content_size.width = content_width;
	m_content_size.height = content_height;

	SVGViewportCompositePaintNode* vpnode =
		static_cast<SVGViewportCompositePaintNode*>(m_doc_ctx->GetPaintNode());
	if (vpnode)
		vpnode->SetScale(scale);

	m_invalid.SetDimensions(OpRect(0, 0, scaled_width, scaled_height));

	return m_invalid.Reset();
}

OP_STATUS SVGRenderer::Create(SVGDocumentContext* doc_ctx,
							  int content_width, int content_height,
							  int scaled_width, int scaled_height,
							  float scale)
{
	OP_ASSERT(doc_ctx);

	m_doc_ctx = doc_ctx;
	m_scale = scale;

	m_content_size.width = content_width;
	m_content_size.height = content_height;

    if (m_doc_ctx && scaled_width > 0 && scaled_height > 0)
	{
		m_invalid.SetDimensions(OpRect(0, 0, scaled_width, scaled_height));

		return m_invalid.Reset();
	}

	OP_ASSERT(0);
	return OpStatus::ERR;
}

#endif // SVG_SUPPORT
