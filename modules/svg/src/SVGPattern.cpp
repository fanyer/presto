/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGPattern.h"

#ifdef SVG_SUPPORT_PATTERNS

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegarendertarget.h"

OP_STATUS SVGPattern::GetFill(VEGAFill** vfill, VEGATransform& vfilltrans,
							  SVGPainter* painter, SVGPaintNode* context_node)
{
	if (m_target)
	{
		// GetFill without PutFill (not a good idea)
		VEGARenderTarget::Destroy(m_target);
		m_target = NULL;
	}

	SVGBoundingBox bbox;
	if (m_units == SVGUNITS_OBJECTBBOX ||
		!m_viewbox && m_content_units == SVGUNITS_OBJECTBBOX)
		bbox = context_node->GetBBox();

	SVGRect pattern_rect = m_pattern_rect;

	if (m_units == SVGUNITS_OBJECTBBOX)
	{
		// Scale with bounding box
		// [ (maxx-minx) 0 0 (maxy-miny) minx miny ]
		SVGMatrix objbbox_transform;
		objbbox_transform.SetValues(bbox.maxx - bbox.minx, 0, 0,
									bbox.maxy - bbox.miny, bbox.minx, bbox.miny);

		pattern_rect = objbbox_transform.ApplyToRect(pattern_rect);
	}

	SVGNumber expansion = painter->GetTransform().GetExpansionFactor() * m_transform.GetExpansionFactor();

	// Dimension the surface to draw on
	SVGNumber bitmap_w = pattern_rect.width * expansion;
	SVGNumber bitmap_h = pattern_rect.height * expansion;

	// Sanity check size of pattern surface
	OpRect pattern_pixel_dim(0, 0, bitmap_w.GetIntegerValue(), bitmap_h.GetIntegerValue());

	if (pattern_pixel_dim.width == 0)
		pattern_pixel_dim.width = 1;
	if (pattern_pixel_dim.height == 0)
		pattern_pixel_dim.height = 1;

	// Shrink the bitmap so that it fits in the space available
	OpStatus::Ignore(SVGUtils::ShrinkToFit(pattern_pixel_dim.width, pattern_pixel_dim.height,
										   m_max_res_width, m_max_res_height));

	SVGMatrix pattern_paint_transform;

	// Compensate for the rounding (truncation)
	pattern_paint_transform.LoadScale(SVGNumber(pattern_pixel_dim.width) / pattern_rect.width,
									  SVGNumber(pattern_pixel_dim.height) / pattern_rect.height);

	SVGRect dummy_clip_rect;
	SVGMatrix viewboxtransform;
	SVGRect pattern_vp(0, 0, pattern_rect.width, pattern_rect.height);
	RETURN_IF_ERROR(SVGUtils::GetViewboxTransform(pattern_vp, m_viewbox ? &m_viewbox->rect : NULL,
												  m_aspectratio, viewboxtransform, dummy_clip_rect));

	pattern_paint_transform.PostMultiply(viewboxtransform);

	// Should not apply this if we have a viewbox
	if (!m_viewbox && m_content_units == SVGUNITS_OBJECTBBOX)
	{
		// [ (maxx-minx) 0 0 (maxy-miny) 0 0 ]
		SVGMatrix objbbox_transform;
		objbbox_transform.SetValues(bbox.maxx - bbox.minx, 0, 0,
									bbox.maxy - bbox.miny, 0, 0);
		pattern_paint_transform.PostMultiply(objbbox_transform);
	}

	VEGARenderer pattern_renderer;
	RETURN_IF_ERROR(pattern_renderer.Init(pattern_pixel_dim.width, pattern_pixel_dim.height));
	RETURN_IF_ERROR(pattern_renderer.createIntermediateRenderTarget(&m_target,
																	pattern_pixel_dim.width,
																	pattern_pixel_dim.height));

	SVGPainter pattern_painter;
	RETURN_IF_ERROR(pattern_painter.BeginPaint(&pattern_renderer, m_target, pattern_pixel_dim));
#ifdef PI_VIDEO_LAYER
	pattern_painter.SetAllowOverlay(FALSE);
#endif // PI_VIDEO_LAYER
	pattern_painter.SetTransform(pattern_paint_transform);
	pattern_painter.SetClipRect(pattern_pixel_dim);
	pattern_painter.Clear(0, &pattern_pixel_dim);
	pattern_painter.SetFlatness(painter->GetFlatness());

	if (m_content_node)
		m_content_node->PaintContent(&pattern_painter);

	pattern_painter.EndPaint();

	RETURN_IF_ERROR(m_target->getImage(vfill));

	(*vfill)->setSpread(VEGAFill::SPREAD_REPEAT);
	(*vfill)->setQuality(VEGAFill::QUALITY_LINEAR);

	// vfilltrans = T(pattern.x, pattern.y) * PatternTransform
	m_transform.CopyToVEGATransform(vfilltrans);

	VEGATransform tmp;
	tmp.loadTranslate(pattern_rect.x.GetVegaNumber(), pattern_rect.y.GetVegaNumber());
	vfilltrans.multiply(tmp);

	// Compensate for the rounding (truncation)
	SVGNumberPair inv_pat_scale(pattern_rect.width / pattern_pixel_dim.width,
								pattern_rect.height / pattern_pixel_dim.height);

	tmp.loadScale(inv_pat_scale.x.GetVegaNumber(), inv_pat_scale.y.GetVegaNumber());
	vfilltrans.multiply(tmp);

	return OpStatus::OK;
}

void SVGPattern::PutFill(VEGAFill* vfill)
{
	VEGARenderTarget::Destroy(m_target);
	m_target = NULL;
}

SVGPattern::~SVGPattern()
{
	SVGObject::DecRef(m_viewbox);
	SVGObject::DecRef(m_aspectratio);

	OP_DELETE(m_content_node);
	VEGARenderTarget::Destroy(m_target);
}

//
// m_transform (PatternTransform) = m_transform * S(1 / pat_scale) [or S^-1(pat_scale)]
// Pattern Canvas CTM = S(pat_scale) * ViewboxTransform {* BBOX-dim | no viewbox && content units is OBB}
//
// where pat_scale is: <canvas expansion> * <size compensation>
//

struct SVGPatternParameters
{
	SVGLengthObject* x;
	SVGLengthObject* y;
	SVGLengthObject* width;
	SVGLengthObject* height;
};

OP_STATUS SVGPattern::FetchValues(HTML_Element* pattern_element,
								  SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
								  SVGPatternParameters* params, HTML_Element** content_root)
{
	OP_ASSERT(pattern_element && pattern_element->IsMatchingType(Markup::SVGE_PATTERN, NS_SVG));

	HTML_Element* inh_pattern_element = SVGUtils::FindHrefReferredNode(resolver, doc_ctx, pattern_element);
	if (inh_pattern_element)
	{
		if (inh_pattern_element->IsMatchingType(Markup::SVGE_PATTERN, NS_SVG))
		{
			OP_STATUS status = resolver->FollowReference(inh_pattern_element);
			if (OpStatus::IsSuccess(status))
			{
				doc_ctx->RegisterDependency(pattern_element, inh_pattern_element);

				status = FetchValues(inh_pattern_element, resolver, doc_ctx, params, content_root);

				resolver->LeaveReference(inh_pattern_element);

				RETURN_IF_ERROR(status);
			}
		}
	}

	// "If this element has no children, and the referenced element does (possibly due to its
	//  own href attribute), then this element inherits the children from the referenced element."
	if (SVGUtils::HasChildren(pattern_element))
		*content_root = pattern_element;

	if (AttrValueStore::HasTransform(pattern_element, Markup::SVGA_PATTERNTRANSFORM, NS_IDX_SVG))
	{
		SVGMatrix matrix;
		AttrValueStore::GetMatrix(pattern_element, Markup::SVGA_PATTERNTRANSFORM, matrix);
		SetTransform(matrix);
	}

	if (AttrValueStore::HasObject(pattern_element, Markup::SVGA_PATTERNCONTENTUNITS, NS_IDX_SVG))
	{
		SVGUnitsType units;
		AttrValueStore::GetUnits(pattern_element, Markup::SVGA_PATTERNCONTENTUNITS, units, SVGUNITS_USERSPACEONUSE);
		SetContentUnits(units);
	}

	if (AttrValueStore::HasObject(pattern_element, Markup::SVGA_PATTERNUNITS, NS_IDX_SVG))
	{
		SVGUnitsType units;
		AttrValueStore::GetUnits(pattern_element, Markup::SVGA_PATTERNUNITS, units, SVGUNITS_OBJECTBBOX);
		SetUnits(units);
	}

	SVGRectObject *viewbox = NULL;
	AttrValueStore::GetViewBox(pattern_element, &viewbox);
	if (viewbox)
		SetViewBox(viewbox);

	SVGAspectRatio* ar = NULL;
	AttrValueStore::GetPreserveAspectRatio(pattern_element, ar);
	if (ar)
		SetAspectRatio(ar);

	AttrValueStore::GetLength(pattern_element, Markup::SVGA_X, &params->x, params->x);
	AttrValueStore::GetLength(pattern_element, Markup::SVGA_Y, &params->y, params->y);
	AttrValueStore::GetLength(pattern_element, Markup::SVGA_WIDTH, &params->width, params->width);
	AttrValueStore::GetLength(pattern_element, Markup::SVGA_HEIGHT, &params->height, params->height);

	return OpStatus::OK;
}

void SVGPattern::ResolvePatternParameters(const SVGPatternParameters& params,
										  const SVGValueContext& vcxt)
{
	m_pattern_rect.x = SVGUtils::ResolveLengthWithUnits(params.x, SVGLength::SVGLENGTH_X,
														m_units, vcxt);
	m_pattern_rect.y = SVGUtils::ResolveLengthWithUnits(params.y, SVGLength::SVGLENGTH_Y,
														m_units, vcxt);
	m_pattern_rect.width = SVGUtils::ResolveLengthWithUnits(params.width, SVGLength::SVGLENGTH_X,
															m_units, vcxt);
	m_pattern_rect.height = SVGUtils::ResolveLengthWithUnits(params.height, SVGLength::SVGLENGTH_Y,
															 m_units, vcxt);
}

OP_STATUS SVGPattern::GeneratePatternContent(SVGDocumentContext* doc_ctx,
											 SVGElementResolver* resolver,
											 const SVGValueContext& vcxt,
											 HTML_Element *pattern_element,
											 HTML_Element* content_root,
											 HTML_Element* context_element)
{
	SVGBoundingBox bbox;
	if (m_content_units == SVGUNITS_OBJECTBBOX ||
		m_units == SVGUNITS_OBJECTBBOX)
	{
		// We need the bounding box
		SVGNumberPair vp(vcxt.viewport_width, vcxt.viewport_height);
		RETURN_IF_ERROR(GetElementBBox(doc_ctx, context_element, vp, bbox));
	}

	// Determine limits (consider doing this based on painting
	// context, rt-size, instead).
	int max_width = INT_MAX;
	int max_height = INT_MAX;
	SVGUtils::LimitCanvasSize(doc_ctx->GetDocument(), doc_ctx->GetVisualDevice(),
							  max_width, max_height);
	m_max_res_width = max_width;
	m_max_res_height = max_height;

	SVGRect pattern_rect = m_pattern_rect;

	if (m_units == SVGUNITS_OBJECTBBOX)
	{
		// Scale with bounding box
		// [ (maxx-minx) 0 0 (maxy-miny) minx miny ]
		SVGMatrix objbbox_transform;
		objbbox_transform.SetValues(bbox.maxx - bbox.minx, 0, 0,
									bbox.maxy - bbox.miny, bbox.minx, bbox.miny);

		pattern_rect = objbbox_transform.ApplyToRect(pattern_rect);
	}

	SVGMatrix canvas_transform;
	SVGRect dummy_clip_rect;
	SVGMatrix viewboxtransform;
	SVGRect pattern_vp(0, 0, pattern_rect.width, pattern_rect.height);
	RETURN_IF_ERROR(SVGUtils::GetViewboxTransform(pattern_vp,
												  m_viewbox ? &m_viewbox->rect : NULL,
												  m_aspectratio, viewboxtransform, dummy_clip_rect));

	canvas_transform.PostMultiply(viewboxtransform);

	// Should not apply this if we have a viewbox
	if (!m_viewbox && m_content_units == SVGUNITS_OBJECTBBOX)
	{
		// [ (maxx-minx) 0 0 (maxy-miny) 0 0 ]
		SVGMatrix objbbox_transform;
		objbbox_transform.SetValues(bbox.maxx - bbox.minx, 0, 0,
									bbox.maxy - bbox.miny, 0, 0);
		canvas_transform.PostMultiply(objbbox_transform);
	}

	SVGNullCanvas canvas;
	canvas.SetDefaults(doc_ctx->GetRenderingQuality());
	canvas.ConcatTransform(canvas_transform);

	SVGElementResolverStack resolver_stack(resolver);
	RETURN_IF_ERROR(resolver_stack.Push(content_root));

	SVGNumberPair pattern_viewport;
	// FIXME: Which document context here? External vs. same document.
	// (Under investigation - see: http://lists.w3.org/Archives/Public/www-svg/2010Feb/0082.html)
	RETURN_IF_ERROR(SVGUtils::GetViewportForElement(pattern_element, doc_ctx, pattern_viewport));

	SVGVectorImageNode* pattern_content_node = OP_NEW(SVGVectorImageNode, ());
	if (!pattern_content_node)
		return OpStatus::ERR_NO_MEMORY;

	pattern_content_node->Reset();

	m_content_node = pattern_content_node;

	SVGRenderingTreeChildIterator rtci;
	SVGPaintTreeBuilder pattern_content_builder(&rtci, pattern_content_node);
	pattern_content_builder.SetCurrentViewport(pattern_viewport);
	pattern_content_builder.SetDocumentContext(doc_ctx);
	pattern_content_builder.SetupResolver(resolver);
	pattern_content_builder.SetCanvas(&canvas);

	return SVGTraverser::Traverse(&pattern_content_builder, content_root, NULL);
}

class AutoSVGPattern
{
public:
	AutoSVGPattern(SVGPattern* pat) : m_ref(pat) {}
	~AutoSVGPattern() { SVGPaintServer::DecRef(m_ref); }

	SVGPattern* operator->() { return m_ref; }

	SVGPattern* get() { return m_ref; }
	SVGPattern* release()
	{
		SVGPattern* pat = m_ref;
		m_ref = NULL;
		return pat;
	}

private:
	SVGPattern* m_ref;
};

OP_STATUS SVGPattern::Create(HTML_Element *pattern_element,
							 HTML_Element* context_element, SVGElementResolver* resolver,
							 SVGDocumentContext* doc_ctx, const SVGValueContext& vcxt,
							 SVGPattern **outpat)
{
	OP_ASSERT(outpat);
	OP_ASSERT(pattern_element && pattern_element->IsMatchingType(Markup::SVGE_PATTERN, NS_SVG));

	RETURN_IF_ERROR(resolver->FollowReference(pattern_element));

	OP_STATUS status;
	HTML_Element* content_root = NULL;

	AutoSVGPattern pattern(OP_NEW(SVGPattern, ()));
	if (pattern.get())
	{
		SVGPatternParameters params;

		// This is the default for x, y, width and height
		SVGLengthObject def_dim;
		params.x = params.y = &def_dim;
		params.width = params.height = &def_dim;

		status = pattern->FetchValues(pattern_element, resolver, doc_ctx, &params, &content_root);

		if (OpStatus::IsSuccess(status))
			pattern->ResolvePatternParameters(params, vcxt);
	}
	else
	{
		status = OpStatus::ERR_NO_MEMORY;
	}

	// We remove the pattern element from the visited set here, since
	// we may need to add one of it's parents further down
	resolver->LeaveReference(pattern_element);

	RETURN_IF_ERROR(status);

	// "A negative value is an error.
	//  A value of zero disables rendering of the element
	//  (i.e., no paint is applied).
	//  If the attribute is not specified, the effect is as if a
	//  value of zero were specified."
	//
	// Treating invalid (negative) values as zero also.
	if (pattern->m_pattern_rect.height <= 0 || pattern->m_pattern_rect.width <= 0)
		return OpSVGStatus::COLOR_IS_NONE;

	// If there are no content elements - disable rendering.
	if (!content_root)
		return OpSVGStatus::COLOR_IS_NONE;

	RETURN_IF_ERROR(pattern->GeneratePatternContent(doc_ctx, resolver, vcxt, pattern_element,
													content_root, context_element));

	*outpat = pattern.release();
	return OpStatus::OK;
}

#endif // SVG_SUPPORT_PATTERNS
#endif // SVG_SUPPORT
