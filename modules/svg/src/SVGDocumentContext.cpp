/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGDependencyGraph.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/svg/src/animation/svganimationworkplace.h"

#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"

#include "modules/layout/box/box.h"
#include "modules/pi/OpScreenInfo.h"

#include "modules/style/css_svgfont.h"

unsigned int SVGFrameTimeModel::GetEstimatedPaintLatency()
{
	unsigned int paint_lat = m_invalid_to_paint_end_delay.GetMedian();
	OP_ASSERT(paint_lat <= INT_MAX);
	return paint_lat;
}

unsigned int SVGFrameTimeModel::CalculateDelay()
{
	unsigned int paint_lat = GetEstimatedPaintLatency();

	// The time required to get a frame painted is less than the frame
	// interval, so simply schedule a new frame at the interval. The
	// case of the latency equaling the frame interval could be
	// treated this way too, but since it's likely to be more jittery
	// we treat it as the animation engine being overloaded.
	if (paint_lat < m_optimal_frame_interval)
		return m_optimal_frame_interval;

	// The time required to get a frame painted is larger or equal
	// than the frame interval that is set - the animation engine is
	// overloaded. This means the target frame rate can not be
	// met. Instead schedule a frame (almost) ASAP.
	return 1;
}

void SVGFrameTimeModel::SamplePaintEnd()
{
	double t = g_op_time_info->GetRuntimeMS();

	if (!op_isnan(m_last_invalid_call))
	{
		OP_ASSERT(t >= m_last_invalid_call);
		unsigned int inv_to_paint_delay = (unsigned int)(t - m_last_invalid_call);
		if (inv_to_paint_delay > 0)
		{
			m_invalid_to_paint_end_delay.AddSample(inv_to_paint_delay);
		}
	}
}

OP_STATUS SVGFrameTimeModel::Initialize()
{
	// Target framerate is already initialized from constructor if there's no pref support
	int tf = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SVGTargetFrameRate);
	if(tf >= 1)
		m_optimal_frame_interval = 1000/tf;

	return m_invalid_to_paint_end_delay.Create(20);
}

SVGDocumentContext::SVGDocumentContext(HTML_Element* e, LogicalDocument* logdoc) :
	SVGSVGElement(e),
#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
	m_scale(1),
	m_translate(NULL),
	m_rotate(0),
#endif // SVG_DOM || SVG_SUPPORT_PANNING
#ifdef SVG_DOM
	m_viewport(NULL),
#endif // SVG_DOM
#ifdef SVG_INTERRUPTABLE_RENDERING
	m_time_for_last_screen_blit(0.0),
#endif // SVG_INTERRUPTABLE_RENDERING
	m_forced_w(0),
	m_forced_h(0),
	m_forced_viewbox(NULL),
	m_svg_image(this, logdoc),
	m_packed_init(0),
	m_elm(e), // FIXME: Remove
	m_animation_workplace(NULL)
{
	m_packed.invalidation_pending = TRUE;
	m_quality = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SVGRenderingQuality);
}

OP_STATUS
SVGDocumentContext::SendDOMEvent(DOM_EventType type, HTML_Element* target)
{
	DOM_Environment* dom_environment = 	GetDOMEnvironment();
	if (dom_environment && dom_environment->IsEnabled())
	{
		DOM_Environment::EventData data;

		data.type = type;
		data.target = target;

		RETURN_IF_ERROR(dom_environment->HandleEvent(data));
	}
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_PANNING
/* Check bounds
 *
 * -----|-------------|--->
 *      0             l
 *   d1                 d2
 * <---->             <---->
 * |------ image ----------|
 *
 * This function checks that d1 and d2 are bigger-or-equal to zero.
 *
 * The distance between '0' and 'l' are the visible area. This
 * distance is:
 *
 *  l := |layoutbox|
 *
 *  d1 = -t
 *  d2 = t + image - l
 *  image = |canvas| * 'user scale' / 'visual device scale' =>
 *    [canvas = l * 'visual device scale'] => image = l * 'user scale'
 *
 * So the conditions are:
 *
 *  d1 > 0  <=> t < 0
 *  d2 > 0  <=> t + image - l >= 0
 */
/* static */ BOOL
SVGDocumentContext::CheckBounds(SVGNumber lb, SVGNumber t, SVGNumber s)
{
	return t <= 0 && (t + lb * (s - 1)) > 0;
}

void
SVGDocumentContext::BeginPanning(const OpPoint& view_pos)
{
	m_packed.is_panning = TRUE;
	last_view_panning_position = view_pos;
}

void
SVGDocumentContext::EndPanning()
{
#ifndef MOUSELESS
	m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_DEFAULT_ARROW);
#endif // !MOUSELESS
	m_packed.is_panning = FALSE;
}

void
SVGDocumentContext::Pan(int doc_pan_x, int doc_pan_y)
{
	m_translate->x -= doc_pan_x;
	m_translate->y -= doc_pan_y;

	m_svg_image.Pan(doc_pan_x, doc_pan_y);
}

void
SVGDocumentContext::ResetPan()
{
	BOOL x_changed = m_translate->x.NotEqual(0);
	BOOL y_changed = m_translate->y.NotEqual(0);
	m_translate->x = 0;
	m_translate->y = 0;

	if (x_changed || y_changed)
	{
		UpdateUserTransform();
		RETURN_VOID_IF_ERROR(SendDOMEvent(SVGSCROLL, m_elm));
	}
}

OP_STATUS
SVGDocumentContext::UpdateZoomAndPan(const SVGManager::EventData& data)
{
	if (!IsZoomAndPanAllowed())
		return OpStatus::OK;

	// Events can be sent when we are being disconnect from document
	if (!GetDocument() || !GetDocument()->GetDocManager())
		return OpStatus::OK;

	BOOL is_pan = (data.modifiers == (SVG_PAN_KEYMASK));
	BOOL is_zoom_in = (data.modifiers == (SVG_ZOOM_IN_KEYMASK));
	BOOL is_zoom_out = (data.modifiers == (SVG_ZOOM_OUT_KEYMASK));

	// No panning in stand-alone svgs. These are enlarged, so no
	// panning is necessary.
	if (IsStandAlone() && is_pan)
		return OpStatus::OK;

	if (data.type == ONMOUSEOVER)
	{
#ifndef MOUSELESS
		if(!m_packed.is_panning)
		{
			if(is_pan)
			{
				m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_MOVE);
			}
#if 0
			else if(is_zoom_in)
			{
				m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_N_RESIZE);
			}
			else if(is_zoom_out)
			{
				m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_S_RESIZE);
			}
#endif
		}
#endif // !MOUSELESS
	}
	else if (data.type == ONMOUSEOUT)
	{
#ifndef MOUSELESS
		m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_DEFAULT_ARROW);
#endif // !MOUSELESS
	}
	else if (data.type == ONMOUSEDOWN && !m_packed.is_panning)
	{
		if(is_pan)
		{
			BeginPanning(OpPoint(data.clientX, data.clientY));
		}
		if(is_zoom_out || is_zoom_in)
		{
			Box* layoutbox = m_elm->GetLayoutBox();
			if (!layoutbox)
				return OpStatus::ERR;

			VisualDevice* vis_dev = GetVisualDevice();
			if (!vis_dev)
				return OpStatus::ERR;

			SVGNumber zoom_factor = is_zoom_in ? SVGNumber(1) / 4 : SVGNumber(-1) / 4;

			RECT offset_rect = {0,0,0,0};
			m_svg_image.GetContentRect(offset_rect);

			// Local point
			OpPoint local_point(data.clientX + vis_dev->GetRenderingViewX() - offset_rect.left,
								data.clientY + vis_dev->GetRenderingViewY() - offset_rect.top);

			return ZoomAroundPointBy(local_point, zoom_factor);
		}
	}
	else if (data.type == ONMOUSEMOVE)
	{
		if (m_packed.is_panning)
		{
			Box* layoutbox = m_elm->GetLayoutBox();
			SVGRenderer* renderer = GetRenderingState();
			if (layoutbox && renderer)
			{
				OpPoint point = OpPoint(data.clientX, data.clientY);
				int pan_delta_x = point.x - last_view_panning_position.x;
				int pan_delta_y = point.y - last_view_panning_position.y;

				if (CheckBounds(int(layoutbox->GetWidth()), m_translate->x + pan_delta_x, m_scale))
				{
					last_view_panning_position.x = point.x;
				}
				else
				{
					pan_delta_x = 0;
				}

				if (CheckBounds(int(layoutbox->GetHeight()), m_translate->y + pan_delta_y, m_scale))
				{
					last_view_panning_position.y = point.y;
				}
				else
				{
					pan_delta_y = 0;
				}

				if (pan_delta_x || pan_delta_y)
				{
					Pan(-pan_delta_x, -pan_delta_y);

					RETURN_IF_ERROR(SendDOMEvent(SVGSCROLL, m_elm));
#ifndef MOUSELESS
					m_svg_image.GetDocument()->GetWindow()->SetCursor(CURSOR_MOVE);
#endif // !MOUSELESS
				}
			}
		}
	}
	else if (data.type == ONMOUSEUP)
	{
		if(m_packed.is_panning)
		{
			EndPanning();
		}
	}
	return OpStatus::OK;
}

void
SVGDocumentContext::FitRectToViewport(OpRect doc_rect)
{
	OpRect viewport = m_viewport->rect.GetEnclosingRect();
	if (viewport.Contains(doc_rect))
		return;

	SVGNumber zoom_factor = m_scale;

	// If zoom-and-pan is allowed, then zoom out so that the element
	// fits inside the viewport
	if (m_svg_image.IsZoomAndPanAllowed() &&
		(viewport.width < doc_rect.width || viewport.height < doc_rect.height))
	{
		zoom_factor = MIN(m_viewport->rect.width / doc_rect.width,
						  m_viewport->rect.height / doc_rect.height);

		doc_rect.x = (zoom_factor * doc_rect.x).GetIntegerValue();
		doc_rect.y = (zoom_factor * doc_rect.y).GetIntegerValue();
		doc_rect.width = (zoom_factor * doc_rect.width).GetIntegerValue();
		doc_rect.height = (zoom_factor * doc_rect.height).GetIntegerValue();
	}

	BOOL perform_zoom = !zoom_factor.Close(m_scale);

	OpPoint rect_center = doc_rect.Center();
	OpPoint viewport_center = viewport.Center();

	int pan_delta_x = viewport_center.x - rect_center.x;
	int pan_delta_y = viewport_center.y - rect_center.y;

	m_translate->x += pan_delta_x;
	m_translate->y += pan_delta_y;

	if (pan_delta_x || pan_delta_y)
	{
		if (!perform_zoom)
			m_svg_image.Pan(-pan_delta_x, -pan_delta_y);

		OpStatus::Ignore(SendDOMEvent(SVGSCROLL, m_elm));
	}

	if (perform_zoom)
		UpdateZoom(zoom_factor);
}
#endif // SVG_SUPPORT_PANNING

OP_STATUS
SVGDocumentContext::ZoomAroundPointBy(const OpPoint& local_point, SVGNumber zoom_factor)
{
	SVGNumber current_zoom = GetCurrentScale();

	// Zoom while keeping the clicked point static. This
	// is achieved by the regular "translate to point,
	// scale, and then translate back" sequence. The value
	// we're after here though is the translation value to
	// add to the current one to get the resulting
	// transform. The expression simplifies to the
	// following.

	SVGNumber rel_zoom; // Defaults to zero, meaning no translation
						// adjustment if zoom == 0
	if (current_zoom.NotEqual(0))
		rel_zoom = (-zoom_factor) / current_zoom;

	SVGNumber translation_dx = rel_zoom * (SVGNumber(local_point.x) - m_translate->x);
	SVGNumber translation_dy = rel_zoom * (SVGNumber(local_point.y) - m_translate->y);

	m_translate->x += translation_dx;
	m_translate->y += translation_dy;

	if (translation_dx.NotEqual(0) || translation_dy.NotEqual(0))
		RETURN_IF_ERROR(SendDOMEvent(SVGSCROLL, m_elm));

	return UpdateZoom(current_zoom + zoom_factor);
}

/* static */ OP_STATUS
SVGDocumentContext::Make(SVGDocumentContext*& ctx, HTML_Element* e, LogicalDocument* logdoc)
{
	ctx = OP_NEW(SVGDocumentContext, (e, logdoc));
	if (!ctx)
		return OpStatus::ERR_NO_MEMORY;
#if defined(SVG_DOM) || defined (SVG_SUPPORT_PANNING)
	ctx->m_translate = OP_NEW(SVGPoint, ());
	if (!ctx->m_translate)
	{
		OP_DELETE(ctx);
		return OpStatus::ERR_NO_MEMORY;
	}
	SVGObject::IncRef(ctx->m_translate);
#endif // SVG_DOM || SVG_SUPPORT_PANNING

#ifdef SVG_DOM
	ctx->m_viewport = OP_NEW(SVGRectObject, ());
	if (!ctx->m_viewport)
	{
		OP_DELETE(ctx);
		return OpStatus::ERR_NO_MEMORY;
	}
	SVGObject::IncRef(ctx->m_viewport);
#endif // SVG_DOM

	e->SetSVGContext(ctx);
	return ctx->m_frametime_model.Initialize();
}

SVGDocumentContext::~SVGDocumentContext()
{
	OP_DELETE(m_animation_workplace);

#ifdef SVG_SUPPORT_TEXTSELECTION
	m_textselection.ClearSelection(FALSE);
#endif // SVG_SUPPORT_TEXTSELECTION

	m_svg_image.Out();

#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
	SVGObject::DecRef(m_translate);
#endif // SVG_DOM || SVG_SUPPORT_PANNING

#if defined(SVG_DOM)
	SVGObject::DecRef(m_viewport);
#endif // SVG_DOM

	// The SVG manager could have already been destroyed when the
	// HTML_Element is destroyed
	if (g_svg_manager_impl)
	{
		// No need to keep old bitmap cached
		g_svg_manager_impl->GetCache()->Remove(SVGCache::RENDERER, this);
	}

	g_font_cache->ClearSVGFonts(); // Not sure this is good for all cases, but it seems to prevent some crashes (bad html element pointer in SVGFontImpl destructor)
}

OP_STATUS
SVGDocumentContext::AddSVGFont(HTML_Element* font_elm, OpFontInfo* fontinfo)
{
	OP_NEW_DBG("AddSVGFont", "svg_font_loading");
	OP_ASSERT(font_elm);
	OP_ASSERT(fontinfo);

	// We should not add fonts that don't have a font-family name
	if (!fontinfo->GetFace())
		return OpStatus::ERR_NULL_POINTER;

	HLDocProfile* hld_profile = GetHLDocProfile();
	CSSCollection* coll = hld_profile ? hld_profile->GetCSSCollection() : NULL;
	if (coll)
	{
		CSSCollectionElement* removed_font = coll->RemoveCollectionElement(font_elm);
		OP_DELETE(removed_font);
		CSS_SvgFont* svg_font = CSS_SvgFont::Make(GetDocument(), font_elm, fontinfo);
		if (!svg_font)
			return OpStatus::ERR_NO_MEMORY;
		coll->AddCollectionElement(svg_font);
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR;
	}
}

OP_STATUS SVGDocumentContext::GetExternalFrameElement(URL fullurl, HTML_Element* parent, SVGFrameElementContext* ctx)
{
	if(!ctx)
	{
		OP_ASSERT(!"This code is invalid, please fix.");
		return OpStatus::ERR_NULL_POINTER;
	}

	SVGDocumentContext *parent_doc_ctx = NULL;
	if (parent->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_IMAGE, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_FEIMAGE, NS_SVG))
	{
		ctx->frame_element = parent;
		ctx->frm_doc = GetDocument(); // we're in this documentcontext
	}
	else if (IsExternalResource() && (parent_doc_ctx = GetParentDocumentContext()))
	{
		return parent_doc_ctx->GetExternalFrameElement(fullurl, parent, ctx);
	}
	else
	{
		SVGWorkplaceImpl* svg_workplace = GetSVGImage()->GetSVGWorkplace();
		if(!svg_workplace)
			return OpStatus::ERR;

		HTML_Element* proxy_element = svg_workplace->GetProxyElement(fullurl);
		if(!proxy_element)
		{
			if(OpStatus::IsError(svg_workplace->CreateProxyElement(fullurl, &proxy_element)))
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		ctx->frame_element = proxy_element;
		ctx->frm_doc = svg_workplace->GetDocument();
	}

	OP_ASSERT(ctx->frm_doc);
	return OpStatus::OK;
}

HTML_Element*
SVGDocumentContext::GetExternalFrameElement(URL fullurl, HTML_Element* parent)
{
	SVGDocumentContext *parent_doc_ctx = NULL;
	if (parent->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_IMAGE, NS_SVG) ||
		parent->IsMatchingType(Markup::SVGE_FEIMAGE, NS_SVG))
	{
		return parent;
	}
	else if (IsExternalResource() && (parent_doc_ctx = GetParentDocumentContext()))
	{
		return parent_doc_ctx->GetExternalFrameElement(fullurl, parent);
	}
	else
	{
		SVGWorkplaceImpl* svg_workplace = GetSVGImage()->GetSVGWorkplace();
		if(!svg_workplace)
			return NULL;

		HTML_Element* proxy_element = svg_workplace->GetProxyElement(fullurl);
		if(!proxy_element)
		{
			if(OpStatus::IsError(svg_workplace->CreateProxyElement(fullurl, &proxy_element)))
			{
				// FIXME: Raise OOM flags on failure?
			}
		}

		return proxy_element;
	}
}

void SVGDocumentContext::SubtreeRemoved(HTML_Element* subroot)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	m_textselection.HandleRemovedSubtree(subroot);
#endif // SVG_SUPPORT_TEXTSELECTION

	if (m_animation_workplace)
	{
		m_animation_workplace->HandleRemovedSubtree(subroot);
	}

	SVGWorkplaceImpl* workplace = m_svg_image.GetSVGWorkplace();
	if (workplace != NULL)
	{
		workplace->HandleRemovedSubtree(subroot);
	}

#if defined(SVG_SUPPORT_EDITABLE) || (defined(SVG_SUPPORT_MEDIA) && defined(PI_VIDEO_LAYER))
	HTML_Element* stop_it = subroot->NextSiblingActual();
	HTML_Element* it = subroot;
	while (it != stop_it)
	{
#ifdef SVG_SUPPORT_EDITABLE
		if (SVGUtils::IsTextRootElement(it) && SVGUtils::IsEditable(it))
		{
			if (SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(it))
				if (SVGTextRootContainer* text_root_cont = elm_ctx->GetAsTextRootContainer())
					if (SVGEditable* e = text_root_cont->GetEditable(FALSE))
						e->SetParentInputContext(NULL);
		}
#endif // SVG_SUPPORT_EDITABLE
#if defined(SVG_SUPPORT_MEDIA) && defined(PI_VIDEO_LAYER)
		if (it->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG))
		{
			SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
			SVGMediaBinding* mbind = mm->GetBinding(it);
			if (mbind)
				mbind->DisableOverlay();
		}
#endif // defined(SVG_SUPPORT_MEDIA) && defined(PI_VIDEO_LAYER)
		it = it->NextActual();
	}
#endif // defined(SVG_SUPPORT_EDITABLE) || (defined(SVG_SUPPORT_MEDIA) && defined(PI_VIDEO_LAYER))
}

#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
void
SVGDocumentContext::GetCurrentMatrix(SVGMatrix& matrix)
{
	// User agent transformation:
	// T(currentTranslate) * S(currentScale) * R(currentRotate)
	if (m_rotate.NotEqual(0))
	{
		matrix.LoadRotation(m_rotate);
		matrix.PostMultScale(m_scale, m_scale);
	}
	else
	{
		matrix.LoadIdentity();
		matrix[0] = m_scale;
		matrix[3] = m_scale;
	}

	matrix[4] = m_translate->x;
	matrix[5] = m_translate->y;
}
#endif // SVG_DOM || SVG_SUPPORT_PANNING

void SVGDocumentContext::UpdateUserTransform()
{
	SVGViewportCompositePaintNode* vpnode = static_cast<SVGViewportCompositePaintNode*>(GetPaintNode());

	if (vpnode && !IsInvalidationPending())
	{
		if (SVGRenderer* renderer = GetRenderingState())
		{
			vpnode->Update(renderer);

			SVGMatrix user_transform;
			GetCurrentMatrix(user_transform);
			vpnode->SetUserTransform(user_transform);

			vpnode->Update(renderer);

			m_svg_image.QueueInvalidate(OpRect());
			return;
		}
	}

	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(this);
}

OP_STATUS
SVGDocumentContext::SetCurrentRotate(SVGNumber user_rotate_deg)
{
	m_rotate = user_rotate_deg;

	UpdateUserTransform();

	// Check for doc manager. If we are being disconnected from
	// document, this may be NULL.
	if (!GetDocument() || !GetDocument()->GetDocManager())
		return OpStatus::OK;

	if (IsStandAlone())
	{
		// If we are stand-alone the rotate may affect our size. Let
		// layout know about this.
		m_elm->MarkDirty(LOGDOC_DOC(GetDocument()), TRUE, TRUE);
	}

	return OpStatus::OK;
}

OP_STATUS
SVGDocumentContext::UpdateZoom(SVGNumber user_zoom_level)
{
	m_scale = user_zoom_level;

	UpdateUserTransform();

	// Check for doc manager. If we are being disconnected from
	// document, this may be NULL.
	if (!GetDocument() || !GetDocument()->GetDocManager())
		return OpStatus::OK;

	if (IsStandAlone())
	{
		// If we are stand-alone the zoom-level affects our size. Let
		// layout know about this.
		m_elm->MarkDirty(LOGDOC_DOC(GetDocument()), TRUE, TRUE);
	}

	RETURN_IF_ERROR(SendDOMEvent(SVGZOOM, m_elm));
	return OpStatus::OK;
}

void SVGDocumentContext::RegisterDependency(HTML_Element* element, HTML_Element* target)
{
	OP_ASSERT(element != NULL);
	OP_ASSERT(target != NULL);

	SVGWorkplaceImpl* workplace = m_svg_image.GetSVGWorkplace();
	if (workplace != NULL)
	{
		if (workplace->GetDependencyGraph() == NULL)
			RETURN_VOID_IF_ERROR(workplace->CreateDependencyGraph());

		OpStatus::Ignore(workplace->GetDependencyGraph()->AddDependency(element, target));
		// OOM -> not something we can do anything about, do the best with what we already know
	}
}

void SVGDocumentContext::RegisterUnresolvedDependency(HTML_Element* element)
{
	OP_ASSERT(element != NULL);

	SVGWorkplaceImpl* workplace = m_svg_image.GetSVGWorkplace();
	if (workplace != NULL)
	{
		if (workplace->GetDependencyGraph() == NULL)
			RETURN_VOID_IF_ERROR(workplace->CreateDependencyGraph());

		OpStatus::Ignore(workplace->GetDependencyGraph()->AddUnresolvedDependency(element));
	}
}

void
SVGDocumentContext::ForceSize(short width, long height)
{
	m_forced_w = width;
	m_forced_h = height;
	m_packed.force_size = 1;
}

BOOL
SVGDocumentContext::IsStandAlone()
{
	if (SVG_DOCUMENT_CLASS *doc = GetDocument())
		if (DocumentManager *doc_manager = doc->GetDocManager())
		{
			FramesDocElm* fd_elm = doc_manager->GetFrame();
			BOOL is_embedded_svg = fd_elm && fd_elm->IsInlineFrame();
			HTML_Element *root = GetSVGRoot();
			BOOL standalone_svg = (!is_embedded_svg && root->ParentActual() &&
									   root->ParentActual()->Type() == HE_DOC_ROOT);
			return standalone_svg;
		}

	// If we have no document manager, we assume to be stand-alone,
	// but does it matter in that case?
	return TRUE;
}

BOOL
SVGDocumentContext::IsEmbeddedByIFrame()
{
	if (SVG_DOCUMENT_CLASS *doc = GetDocument())
		if (DocumentManager *doc_manager = doc->GetDocManager())
			if (FramesDocElm* fd_elm = doc_manager->GetFrame())
				return (fd_elm->IsInlineFrame() && fd_elm->GetHtmlElement() && fd_elm->GetHtmlElement()->Type() == Markup::HTE_IFRAME);

	return FALSE;
}

SVGRenderer*
SVGDocumentContext::GetRenderingState()
{
	return g_svg_manager_impl ? static_cast<SVGRenderer*>(g_svg_manager_impl->GetCache()->Get(SVGCache::RENDERER, this)) : NULL;
}

SVGAnimationWorkplace*
SVGDocumentContext::AssertAnimationWorkplace()
{
	if (m_animation_workplace == NULL)
		m_animation_workplace = OP_NEW(SVGAnimationWorkplace, (this));
	return m_animation_workplace;
}

void
SVGDocumentContext::DestroyAnimationWorkplace()
{
	OP_DELETE(m_animation_workplace);
	m_animation_workplace = NULL;
}

SVGDocumentContext *
SVGDocumentContext::GetParentDocumentContext()
{
	HTML_Element* frame_element = GetReferencingElement();
	if (frame_element)
	{
		return AttrValueStore::GetSVGDocumentContext(frame_element);
	}
	else
	{
		return NULL;
	}
}

HTML_Element *
SVGDocumentContext::GetReferencingElement()
{
	if (!IsExternal() || !GetDocument())
		return NULL;

	DocumentManager *doc_manager = GetDocument()->GetDocManager();
	if (doc_manager && doc_manager->GetFrame())
	{
		return doc_manager->GetFrame()->GetHtmlElement();
	}
	else
	{
		return NULL;
	}
}

SVGDependencyGraph *
SVGDocumentContext::GetDependencyGraph()
{
	if (IsExternalResource())
	{
		DocumentManager *doc_manager = GetDocument()->GetDocManager();
		if (doc_manager)
		{
			HTML_Element *frame_element = doc_manager->GetFrame()->GetHtmlElement();
			if (frame_element && SVGUtils::IsExternalProxyElement(frame_element))
			{
				FramesDocument* parent_doc = GetDocument()->GetParentDoc();
				if(parent_doc)
				{
					// We are in a pseudo-document, call home!
					SVGWorkplaceImpl* workplace = (SVGWorkplaceImpl*)parent_doc->GetLogicalDocument()->GetSVGWorkplace();
					return workplace->GetDependencyGraph();
				}
			}
		}
	}

	SVGWorkplaceImpl* workplace = m_svg_image.GetSVGWorkplace();
	if (workplace != NULL)
	{
		return workplace->GetDependencyGraph();
	}

	return NULL;
}

SVG_ANIMATION_TIME SVGDocumentContext::GetDefaultSyncTolerance()
{
	SVG_ANIMATION_TIME tolerance;
	OpStatus::Ignore(AttrValueStore::GetAnimationTime(GetSVGRoot(), Markup::SVGA_SYNCTOLERANCEDEFAULT,
													  tolerance, SVGAnimationTime::Indefinite()));

	// 'indefinite' means 'inherit' in this context
	if (tolerance == SVGAnimationTime::Indefinite())
	{
		if (SVGDocumentContext* parent_doc_ctx = GetParentDocumentContext())
			tolerance = parent_doc_ctx->GetDefaultSyncTolerance();
		else
			// SMIL 2.1 says: "If there is no parent element, the
			// value is implementation dependent but should be no
			// greater than two seconds."; So feel free to change this
			// if necessary.
			tolerance = 500; // 0.5s
	}
	return tolerance;
}

SVGSyncBehavior SVGDocumentContext::GetDefaultSyncBehavior()
{
	SVGSyncBehavior syncbehavior =
		(SVGSyncBehavior)AttrValueStore::GetEnumValue(GetSVGRoot(), Markup::SVGA_SYNCBEHAVIORDEFAULT,
													  SVGENUM_SYNCBEHAVIOR,
													  SVGSYNCBEHAVIOR_INHERIT);

	if (syncbehavior == SVGSYNCBEHAVIOR_INHERIT)
	{
		if (SVGDocumentContext* parent_doc_ctx = GetParentDocumentContext())
			syncbehavior = parent_doc_ctx->GetDefaultSyncBehavior();
		else
			// SMIL 2.1 says: "If there is no parent element, the
			// value is implementation dependent."; So feel free to
			// change this if necessary.
			syncbehavior = SVGSYNCBEHAVIOR_CANSLIP;
	}
	return syncbehavior;
}

#ifdef SVG_DOM
void
SVGDocumentContext::UpdateViewport(const SVGRect &viewport)
{
	m_viewport->rect = viewport;
	m_packed.has_set_viewport = 1;
}
#endif // SVG_DOM

HTML_Element *
SVGDocumentContext::GetElementByReference(SVGElementResolver* resolver, HTML_Element *element, URL url)
{
	HTML_Element *target = NULL;
	if (url == GetURL() || url.Type() == URL_OPERA)
	{
		OP_ASSERT(url.UniRelName() != NULL);
		target = SVGUtils::FindElementById(GetLogicalDocument(), url.UniRelName());
	}
	else
	{
		SVGDocumentContext::SVGFrameElementContext fedata;
		OP_STATUS err = GetExternalFrameElement(url, element, &fedata);
		OP_ASSERT(fedata.frm_doc);
		if (OpStatus::IsSuccess(err))
		{
			target = SVGUtils::FindURLReferredNode(resolver, fedata.frm_doc, fedata.frame_element, url.UniRelName());
			if (target == NULL)
				RegisterDependency(element, fedata.frame_element);
		}
	}

	if (target && resolver && resolver->IsLoop(target))
	{
		target = NULL;
	}

	return target;
}

#endif // SVG_SUPPORT
