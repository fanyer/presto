/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/svgpaintnode.h"
#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/SVGPoint.h"

#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGMarker.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/svgpaintserver.h"

// For SVGRasterImageNode
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/helistelm.h"

// For SVGForeignObject{,Href}Node
#include "modules/dochand/fdelm.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/traverse/traverse.h"

// For SVGVideoNode
#include "modules/media/mediaplayer.h"
#include "modules/svg/src/svgmediamanager.h"

void SVGPaintNode::Reset()
{
#ifdef SVG_SUPPORT_STENCIL
	if (clippath)
	{
		clippath->Free();
		clippath = NULL;
	}

	if (mask)
	{
		mask->Free();
		mask = NULL;
	}
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_FILTERS
	OP_DELETE(filter); // Maybe Detach() should also Reset()?
	filter = NULL;
#endif // SVG_SUPPORT_FILTERS
	opacity = 255;
	visible = TRUE;
}

OP_BOOLEAN SVGPaintNode::ApplyProperties(SVGPainter* painter, PaintPropertySet& propset)
{
	// Apply
	// 1) CTM
	if (ref_transform)
	{
		// Need the root transform
		NodeContext context;
		GetParentContext(context, FALSE);

		// Push a identity matrix to avoid disturbing the transform stack
		SVGMatrix mtx;
		RETURN_IF_ERROR(painter->PushTransform(mtx));

		// Calculate the actual transform and modify the painter in-place
		mtx = context.root_transform;
		mtx.PostMultiply(ustransform);

		painter->SetTransform(mtx);
	}
	else
	{
		RETURN_IF_ERROR(painter->PushTransform(ustransform));
	}

	// 2) opacity
	if (!can_skip_layer && opacity < 255
#ifdef SVG_SUPPORT_STENCIL
		|| mask
#endif // SVG_SUPPORT_STENCIL
		)
	{
		SVGBoundingBox extents;
		GetLocalExtentsWithFilter(extents);

		OP_BOOLEAN status = painter->PushLayer(extents, opacity);

		// OpBoolean::IS_FALSE means the intersection of extents and
		// the current clip extents is empty
		if (status != OpBoolean::IS_TRUE)
			return status;

		propset.Set(PAINTPROP_OPACITY);
	}

#ifdef SVG_SUPPORT_STENCIL
	// 3) clip-path
	if (clippath)
	{
		RETURN_IF_ERROR(clippath->Apply(painter, this));

		propset.Set(PAINTPROP_CLIPPATH);
	}

	// 4) mask
	if (mask)
	{
		RETURN_IF_ERROR(mask->Apply(painter, this));

		// Owned by the opacity layer
	}
#endif // SVG_SUPPORT_STENCIL
	return OpBoolean::IS_TRUE;
}

#ifdef SVG_SUPPORT_FILTERS
BOOL SVGPaintNode::AffectsFilter() const
{
	SVGPaintNode* node_parent = GetParent();
	while (node_parent)
	{
		if (node_parent->filter)
			return TRUE;

		node_parent = node_parent->GetParent();
	}
	return FALSE;
}
#endif // SVG_SUPPORT_FILTERS

OP_STATUS SVGPaintNode::PaintContentWithFilter(SVGPainter* painter)
{
#ifdef SVG_SUPPORT_FILTERS
	if (filter)
		return filter->Apply(painter, this);
#endif // SVG_SUPPORT_FILTERS

	PaintContent(painter);
	return OpStatus::OK;
}

void SVGPaintNode::GetLocalExtentsWithFilter(SVGBoundingBox& extents)
{
#ifdef SVG_SUPPORT_FILTERS
	if (filter)
		extents = filter->GetFilterRegion();
	else
#endif // SVG_SUPPORT_FILTERS
		GetLocalExtents(extents);
}

OP_STATUS SVGPaintNode::Paint(SVGPainter* painter)
{
	PaintPropertySet propset;
	OP_BOOLEAN status = ApplyProperties(painter, propset);
	if (status == OpBoolean::IS_TRUE)
		if (!IsBuffered() || !PaintBuffered(painter))
			PaintContentWithFilter(painter);

	UnapplyProperties(painter, propset);

	RETURN_IF_ERROR(status);
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGPaintNode::Clip(SVGPainter* painter)
{
	PaintPropertySet propset;
	OP_STATUS status = ApplyClipProperties(painter, propset);
	if (OpStatus::IsSuccess(status))
		ClipContent(painter);

	UnapplyClipProperties(painter, propset);
	return status;
}
#endif // SVG_SUPPORT_STENCIL

static OpRect CalculateBufferArea(const SVGMatrix& ctm, SVGRect node_extents)
{
	OpRect buffer_area;
	SVGNumberPair r0(ctm[0], ctm[1]);

	// Calculate X-scale
	SVGNumber sx = SVGNumberPair::Dot(r0, r0).sqrt();
	if (sx.NotEqual(0))
	{
		r0.x /= sx;
		r0.y /= sx;
	}

	SVGNumberPair r1(ctm[2], ctm[3]);

	// Calculate shear/skew
	SVGNumber shear = SVGNumberPair::Dot(r0, r1);

	r1.x = r1.x + r0.x * (-shear);
	r1.y = r1.y + r0.y * (-shear);

	// Calculate Y-scale
	SVGNumber sy = SVGNumberPair::Dot(r1, r1).sqrt();

	if (sx.Close(0) || sy.Close(0))
		return buffer_area;

	SVGNumber estimated_width = node_extents.width * sx;
	SVGNumber estimated_height = node_extents.height * sy;

	buffer_area.width = estimated_width.ceil().GetIntegerValue();
	buffer_area.height = estimated_height.ceil().GetIntegerValue();

	return buffer_area;
}

BOOL SVGPaintNode::PaintBuffered(SVGPainter* painter)
{
	OP_NEW_DBG("PaintBuffered", "svg_bufren");
	SVGRenderBuffer* render_buffer = NULL;
	SVGCache* scache = g_svg_manager_impl->GetCache();
	void* buffer_key = GetBufferKey();

	if (IsBufferPresent())
	{
		// Lookup buffer
		render_buffer = static_cast<SVGRenderBuffer*>(scache->Get(SVGCache::SURFACE, buffer_key));
	}

	if (!render_buffer || !has_clean_br_state)
	{
		SVGBoundingBox local_extents;
		GetLocalExtentsWithFilter(local_extents);
		SVGRect buffer_extents = local_extents.GetSVGRect();

		OpRect buffer_area = CalculateBufferArea(painter->GetTransform(), buffer_extents);

		// Check if we can reuse a previously allocated buffer
		if (render_buffer && !buffer_area.Equals(render_buffer->area))
		{
			SVGCache* scache = g_svg_manager_impl->GetCache();
			scache->Remove(SVGCache::SURFACE, GetBufferKey());
			SetBufferPresent(FALSE);
			render_buffer = NULL;
			OP_DBG(("New dimensions, recreating buffer"));
		}

		BOOL add_to_cache = FALSE;
		if (!render_buffer)
		{
			OP_DBG(("Creating new buffer"));

			render_buffer = OP_NEW(SVGRenderBuffer, ());
			if (!render_buffer)
				return FALSE;

			render_buffer->area = buffer_area;

			add_to_cache = TRUE;
		}

		render_buffer->extents = buffer_extents;

		OP_STATUS gen_status = UpdateBuffer(painter, render_buffer);
		if (add_to_cache && OpStatus::IsSuccess(gen_status))
		{
			SVGCache* scache = g_svg_manager_impl->GetCache();
			gen_status = scache->Add(SVGCache::SURFACE, GetBufferKey(), render_buffer);
			if (OpStatus::IsSuccess(gen_status))
			{
				SetBufferPresent(TRUE);
			}
		}

		if (OpStatus::IsError(gen_status))
		{
			if (add_to_cache)
			{
				OP_DELETE(render_buffer);
				render_buffer = NULL;
			}

			// An error was encountered while trying to generate
			// the buffer - mark this node as unbuffered for the
			// time being (the next layout-pass will reset it).
			ResetBuffering();

			// If it was an OOM error, then disable the buffering
			// until further notice.
			if (OpStatus::IsMemoryError(gen_status))
				DisableBuffering();

			return FALSE;
		}
	}

	OP_ASSERT(render_buffer);
	OP_ASSERT(IsBufferPresent());

	// The buffer is available - use it
	SVGPainter::ImageRenderQuality quality = SVGPainter::IMAGE_QUALITY_NORMAL;
	// FIXME: retrieve quality

	OpRect src_rect(0, 0, render_buffer->area.width, render_buffer->area.height);
	// Using saved extents to avoid any tree changes
	SVGRect dst_rect = render_buffer->extents;

	OP_STATUS status = painter->DrawImage(render_buffer->rendertarget, src_rect, dst_rect, quality);

	return !!OpStatus::IsSuccess(status);
}


OP_STATUS SVGPaintNode::UpdateBuffer(SVGPainter* painter, SVGRenderBuffer* render_buffer)
{
	OP_NEW_DBG("UpdateBuffer", "svg_bufren");

	OP_DBG(("Dim: %dx%d Extents: [%g,%g,%g,%g]",
			render_buffer->area.width,
			render_buffer->area.height,
			render_buffer->extents.x.GetFloatValue(),
			render_buffer->extents.y.GetFloatValue(),
			render_buffer->extents.width.GetFloatValue(),
			render_buffer->extents.height.GetFloatValue()));

	// Creating a new (temporary) renderer for each buffer - the
	// option would be reuse the one from the painter passed in (doing
	// Init(...) on it). This is how it used to work though.
	VEGARenderer renderer;
	RETURN_IF_ERROR(renderer.Init(render_buffer->area.width, render_buffer->area.height));

	if (!render_buffer->rendertarget)
	{
		RETURN_IF_ERROR(renderer.createIntermediateRenderTarget(&render_buffer->rendertarget,
																render_buffer->area.width,
																render_buffer->area.height));
	}

	// Calculate a mapping from device space to 'extents space'
	// S(buffer_w / extents_w, buffer_h / extents_h) * T(-extents.x, -extents.y)
	SVGMatrix buffer_ctm;
	buffer_ctm.LoadScale(SVGNumber(render_buffer->area.width) / render_buffer->extents.width,
						 SVGNumber(render_buffer->area.height) / render_buffer->extents.height);
	buffer_ctm.PostMultTranslation(-render_buffer->extents.x, -render_buffer->extents.y);

	SVGPainter buffer_painter;
	RETURN_IF_ERROR(buffer_painter.BeginPaint(&renderer, render_buffer->rendertarget,
											  render_buffer->area));
	buffer_painter.SetClipRect(render_buffer->area);
	buffer_painter.Clear(0, &render_buffer->area);
	buffer_painter.SetTransform(buffer_ctm);
	buffer_painter.SetFlatness(painter->GetFlatness());

	OP_STATUS status = PaintContentWithFilter(&buffer_painter);

	buffer_painter.EndPaint();

	RETURN_IF_ERROR(status);

	has_clean_br_state = TRUE;
	return status;
}

void SVGPaintNode::UnapplyProperties(SVGPainter* painter, PaintPropertySet& propset)
{
	// Unapply
#ifdef SVG_SUPPORT_STENCIL
	// 4) mask - owned by the opacity layer
	// 3) clip-path
	if (propset.IsSet(PAINTPROP_CLIPPATH))
		painter->RemoveClip();
#endif // SVG_SUPPORT_STENCIL

	// 2) opacity
	if (propset.IsSet(PAINTPROP_OPACITY))
		painter->PopLayer();

	// 1) CTM
	painter->PopTransform();
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGPaintNode::ApplyClipProperties(SVGPainter* painter, PaintPropertySet& propset)
{
	// Apply
	// 1) CTM
	RETURN_IF_ERROR(painter->PushTransform(ustransform));

	// 2) clip-path
	if (clippath)
	{
		RETURN_IF_ERROR(clippath->Apply(painter, this));

		propset.Set(PAINTPROP_CLIPPATH);
	}
	return OpStatus::OK;
}

void SVGPaintNode::UnapplyClipProperties(SVGPainter* painter, PaintPropertySet& propset)
{
	// Unapply
	// 2) clip-path
	if (propset.IsSet(PAINTPROP_CLIPPATH))
		painter->RemoveClip();

	// 1) CTM
	painter->PopTransform();
}
#endif // SVG_SUPPORT_STENCIL

void SVGPaintNode::DrawImagePlaceholder(SVGPainter* painter, const SVGRect& in_rect)
{
	SVGObjectProperties object_props;
	SVGStrokeProperties stroke_props;
	SVGPaintDesc fill;
	SVGPaintDesc stroke;
	object_props.fillrule = SVGFILL_NON_ZERO;
	object_props.filled = TRUE;
	object_props.stroked = FALSE;
	stroke_props.width = 2;
	stroke_props.miter_limit = 4;
	stroke_props.cap = SVGCAP_BUTT;
	stroke_props.join = SVGJOIN_MITER;
	stroke_props.non_scaling = TRUE;
	stroke.pserver = NULL;
	fill.color = 0xFF000000; // black
	fill.opacity = 128;
	fill.pserver = NULL;

	painter->SetObjectProperties(&object_props);
	painter->SetStrokeProperties(&stroke_props);
	painter->SetFillPaint(fill);

	int num_horiz = 8;
	int num_vert = 4;

	SVGRect img_rect = in_rect;
	SVGNumber inset = stroke_props.width/2;
	img_rect.width -= stroke_props.width;
	img_rect.height -= stroke_props.width;
	img_rect.x += inset;
	img_rect.y += inset;

	SVGNumber rect_width = img_rect.width / num_horiz;

	if (rect_width * painter->GetTransform().GetExpansionFactor() < 1.0)
	{
		num_horiz = 2;
		rect_width = img_rect.width / num_horiz;
	}

	SVGNumber num_vert_num = img_rect.height / rect_width + 1;
	num_vert = num_vert_num.GetRoundedIntegerValue();

	SVGNumber iheight = rect_width;
	SVGNumber iwidth = rect_width;
	SVGNumber offset = rect_width / 2;
	SVGNumber ix = img_rect.x;
	SVGNumber iy = img_rect.y;

	for (int j = 0; j < num_vert; j++, iy += iheight)
	{
		SVGNumber tempheight = iheight;

		if (j % 2 == 0)
			ix = img_rect.x - offset;
		else
			ix = img_rect.x + offset;

		if (j == 0)
		{
			iy = img_rect.y;
			tempheight = iheight - offset;
		}

		for (int i = 0; i < num_horiz / 2 + 1; i++, ix += iwidth * 2)
		{
			SVGNumber tempwidth = iwidth;

			// Starting xposition can be outside, so we do "clipping" here
			if (img_rect.x > ix)
			{
				ix = img_rect.x;
				tempwidth = iwidth - offset;
			}

			// Do w/h "clipping"
			if (ix + tempwidth > img_rect.x + img_rect.width)
				tempwidth = img_rect.x + img_rect.width - ix;
			if (iy + tempheight > img_rect.y + img_rect.height)
				tempheight = img_rect.y + img_rect.height - iy;

			if (tempwidth > 0 && tempheight > 0)
			{
				OpStatus::Ignore(painter->DrawRect(ix, iy, tempwidth, tempheight, 0, 0));

				if (iwidth > tempwidth)
					ix -= tempwidth;
			}
		}

		if (j == 0)
			iy -= tempheight;
	}

	stroke.color = 0xFF0000FF; // red
	stroke.opacity = 192;
	object_props.filled = FALSE;
	object_props.stroked = TRUE;
	painter->SetStrokePaint(stroke);

	OpStatus::Ignore(painter->DrawLine(img_rect.x, img_rect.y,
									   img_rect.x+img_rect.width, img_rect.y+img_rect.height));
	OpStatus::Ignore(painter->DrawLine(img_rect.x+img_rect.width, img_rect.y,
									   img_rect.x, img_rect.y+img_rect.height));

	OpStatus::Ignore(painter->DrawRect(img_rect.x, img_rect.y, img_rect.width, img_rect.height, 0, 0));
}

SVGMatrix SVGPaintNode::GetHostInverseTransform()
{
	SVGMatrix host_inv;
	NodeContext context;
	if (OpStatus::IsSuccess(GetParentContext(context, FALSE)))
	{
		// GetNodeContext does some additional work (for clipping)
		// but is probably a bit safer than this alternative:
		//        context.transform.PostMultiply(GetTransform());
		GetNodeContext(context);
		host_inv = context.transform;
		if (!host_inv.Invert())
			host_inv.LoadIdentity();
	}
	return host_inv;
}

SVGGeometryNode::~SVGGeometryNode()
{
	SVGPaintServer::DecRef(paints[0].pserver);
	SVGPaintServer::DecRef(paints[1].pserver);
	SVGResolvedDashArray::DecRef(stroke_props.dash_array);
#ifdef SVG_SUPPORT_MARKERS
	ResetMarkers();
#endif // SVG_SUPPORT_MARKERS
}

#ifdef SVG_SUPPORT_MARKERS
void SVGGeometryNode::ResetMarkers()
{
	if (has_markers)
	{
		if (markers[0])
			markers[0]->Free();
		if (markers[1])
			markers[1]->Free();
		if (markers[2])
			markers[2]->Free();
		has_markers = FALSE;
	}
}

void SVGGeometryNode::SetMarkers(SVGMarkerNode* start, SVGMarkerNode* middle, SVGMarkerNode* end)
{
	ResetMarkers();
	markers[0] = start;
	markers[1] = middle;
	markers[2] = end;
	has_markers = (start || middle || end);
}
#endif // SVG_SUPPORT_MARKERS

void SVGGeometryNode::UpdateState(SVGCanvasState* state)
{
	obj_props.stroked = state->UseStroke();
	obj_props.filled = state->UseFill();
	obj_props.fillrule = state->GetFillRule();
	obj_props.aa_quality = state->GetShapeRenderingVegaQuality();

	if (obj_props.stroked)
	{
		SVGResolvedDashArray::AssignRef(stroke_props.dash_array, state->GetStrokeDashArray());
		stroke_props.dash_offset = state->GetStrokeDashOffset();
		stroke_props.width = state->GetStrokeLineWidth();
		stroke_props.miter_limit = state->GetStrokeMiterLimit();
		stroke_props.cap = state->GetStrokeCapType();
		stroke_props.join = state->GetStrokeJoinType();
		stroke_props.non_scaling = state->GetVectorEffect() == SVGVECTOREFFECT_NON_SCALING_STROKE;

		paints[1].color = state->GetActualStrokeColor();
		paints[1].opacity = state->GetStrokeOpacity();
		SVGPaintServer::AssignRef(paints[1].pserver, state->GetStrokePaintServer());
	}

	if (obj_props.filled)
	{
		paints[0].color = state->GetActualFillColor();
		paints[0].opacity = state->GetFillOpacity();
		SVGPaintServer::AssignRef(paints[0].pserver, state->GetFillPaintServer());
	}
}

static inline SVGPaintDesc MakeModulatedPaint(SVGPaintDesc paint, unsigned opacity)
{
	paint.opacity = (paint.opacity * opacity) / 255;
	return paint;
}

void SVGGeometryNode::TransferObjectProperties(SVGPainter* painter)
{
	// This needs to be done after the geometry has been set, so can't be done in UpdateState.
	if (!IsStrokePaintServerAllowed())
		SVGPaintServer::AssignRef(paints[1].pserver, NULL);

	painter->SetObjectProperties(&obj_props);
	painter->SetStrokeProperties(&stroke_props);

	if (!can_skip_layer || opacity == 255)
	{
		painter->SetFillPaint(paints[0]);
		painter->SetStrokePaint(paints[1]);
	}
	else
	{
		// Set paints with modulated opacity
		painter->SetFillPaint(MakeModulatedPaint(paints[0], opacity));
		painter->SetStrokePaint(MakeModulatedPaint(paints[1], opacity));
	}
}

#ifdef SVG_SUPPORT_MARKERS
void SVGGeometryNode::PaintMarkers(SVGPainter* painter, SVGMarkerPosIterator *iter)
{
	RETURN_VOID_IF_ERROR(iter->First());
	do
	{
		SVGMarkerNode* sel_marker = NULL;

		if (iter->IsStart())
			sel_marker = markers[0];
		else if (iter->IsEnd())
			sel_marker = markers[2];
		else
			sel_marker = markers[1];

		if (sel_marker)
		{
			SVGNumber current_slope = 0;
			if (sel_marker->NeedsSlope())
				current_slope = iter->GetCurrentSlope();

			sel_marker->Apply(painter, current_slope, iter->GetCurrentPosition());
		}

	} while (iter->Next());
}
#endif // SVG_SUPPORT_MARKERS

OP_STATUS SVGPrimitiveNode::PaintPrimitive(SVGPainter* painter)
{
	switch (type)
	{
	case RECTANGLE:
		return painter->DrawRect(data[0], data[1], data[2], data[3], data[4], data[5]);

	case ELLIPSE:
		return painter->DrawEllipse(data[0], data[1], data[2], data[3]);

	case LINE:
		return painter->DrawLine(data[0], data[1], data[2], data[3]);
	}

	OP_ASSERT(!"Unknown primitive (or corrupted paint node)");
	return OpStatus::ERR;
}

void SVGPrimitiveNode::PaintContent(SVGPainter* painter)
{
	TransferObjectProperties(painter);

	painter->BeginDrawing(this);

	OpStatus::Ignore(PaintPrimitive(painter));

	painter->EndDrawing();

#ifdef SVG_SUPPORT_MARKERS
	if (type == LINE && has_markers)
	{
		SVGVector list(SVGOBJECT_POINT);
		SVGPoint p1(data[0], data[1]);
		SVGPoint p2(data[2], data[3]);
		list.Append(SVGObject::Protect(&p1));
		list.Append(SVGObject::Protect(&p2));
		SVGMarkerPointListPosIterator ptlist_iter(&list, FALSE);
		PaintMarkers(painter, &ptlist_iter);
	}
#endif // SVG_SUPPORT_MARKERS
}

#ifdef SVG_SUPPORT_STENCIL
void SVGPrimitiveNode::ClipContent(SVGPainter* painter)
{
	TransferObjectProperties(painter);

	OpStatus::Ignore(PaintPrimitive(painter));
}
#endif // SVG_SUPPORT_STENCIL

void SVGPrimitiveNode::UpdateState(SVGCanvasState* state)
{
	SVGGeometryNode::UpdateState(state);

	// Layer can be skipped if we have only fill or only stroke
	can_skip_layer = !HasFilter() && (type == LINE || (!!obj_props.filled ^ !!obj_props.stroked));
}

#ifdef SVG_SUPPORT_MARKERS
static SVGBoundingBox MarkerExtents(SVGMarkerNode** markers, SVGMarkerPosIterator *iter)
{
	SVGBoundingBox total_extents;
	RETURN_VALUE_IF_ERROR(iter->First(), total_extents);
	do
	{
		SVGMarkerNode* sel_marker = NULL;

		if (iter->IsStart())
			sel_marker = markers[0];
		else if (iter->IsEnd())
			sel_marker = markers[2];
		else
			sel_marker = markers[1];

		if (sel_marker)
		{
			SVGNumber current_slope = 0;
			if (sel_marker->NeedsSlope())
				current_slope = iter->GetCurrentSlope();

			SVGBoundingBox extents;
			sel_marker->GetLocalMarkerExtents(current_slope, iter->GetCurrentPosition(), extents);
			total_extents.UnionWith(extents);
		}

	} while (iter->Next());

	return total_extents;
}
#endif // SVG_SUPPORT_MARKERS

void SVGPrimitiveNode::GetLocalExtents(SVGBoundingBox& extents)
{
	SVGBoundingBox lbbox = GetBBox();

	if (obj_props.stroked)
	{
		SVGMatrix host_inv;
		if (stroke_props.non_scaling)
		{
			host_inv = GetHostInverseTransform();
		}

		SVGNumber ext;
		if (type == LINE)
		{
			/* Only caps apply */
			ext = stroke_props.CapExtents(host_inv);
		}
		else
		{
			/* RECTANGLE: Only joins apply (and all are 90 degrees) */
			/* ELLIPSE:   Neither caps nor joins apply */
			ext = stroke_props.JoinExtents(host_inv);
		}

		lbbox.Extend(ext);
	}

#ifdef SVG_SUPPORT_MARKERS
	if (has_markers && type == LINE)
	{
		SVGVector list(SVGOBJECT_POINT);
		SVGPoint p1(data[0], data[1]);
		SVGPoint p2(data[2], data[3]);
		list.Append(SVGObject::Protect(&p1));
		list.Append(SVGObject::Protect(&p2));
		SVGMarkerPointListPosIterator ptlist_iter(&list, FALSE);
		lbbox.UnionWith(MarkerExtents(markers, &ptlist_iter));
	}
#endif // SVG_SUPPORT_MARKERS

	extents = lbbox;
}

SVGBoundingBox SVGPrimitiveNode::GetBBox()
{
	SVGBoundingBox bbox;

	switch (type)
	{
	case RECTANGLE:
		bbox.minx = data[0];
		bbox.miny = data[1];
		bbox.maxx = data[0] + data[2];
		bbox.maxy = data[1] + data[3];
		break;

	case ELLIPSE:
		bbox.minx = data[0] - data[2];
		bbox.miny = data[1] - data[3];
		bbox.maxx = data[0] + data[2];
		bbox.maxy = data[1] + data[3];
		break;

	case LINE:
		bbox.minx = SVGNumber::min_of(data[0], data[2]);
		bbox.miny = SVGNumber::min_of(data[1], data[3]);
		bbox.maxx = SVGNumber::max_of(data[0], data[2]);
		bbox.maxy = SVGNumber::max_of(data[1], data[3]);
		break;

	default:
		OP_ASSERT(!"Unimplemented");
	}
	return bbox;
}

void SVGPathNode::PaintContent(SVGPainter* painter)
{
	if (!path)
		return;

	TransferObjectProperties(painter);

	painter->BeginDrawing(this);

	OpStatus::Ignore(painter->DrawPath(path, pathlength));

	painter->EndDrawing();

#ifdef SVG_SUPPORT_MARKERS
	if (has_markers)
	{
		SVGMarkerPathPosIterator pathlist_iter(path);
		PaintMarkers(painter, &pathlist_iter);
	}
#endif // SVG_SUPPORT_MARKERS
}

#ifdef SVG_SUPPORT_STENCIL
void SVGPathNode::ClipContent(SVGPainter* painter)
{
	if (!path)
		return;

	TransferObjectProperties(painter);

	OpStatus::Ignore(painter->DrawPath(path, pathlength));
}
#endif // SVG_SUPPORT_STENCIL

void SVGPathNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (!path)
		return;

	extents = GetBBox();

	SVGNumber ext;
	if (obj_props.stroked)
	{
		SVGMatrix host_inv;
		if (stroke_props.non_scaling)
		{
			host_inv = GetHostInverseTransform();
		}

		ext = stroke_props.Extents(host_inv);
		extents.Extend(ext);
	}

#ifdef SVG_SUPPORT_MARKERS
	if (has_markers)
	{
		SVGMarkerPathPosIterator pathlist_iter(path);
		extents.UnionWith(MarkerExtents(markers, &pathlist_iter));
	}
#endif // SVG_SUPPORT_MARKERS
}

SVGBoundingBox SVGPathNode::GetBBox()
{
	if (!has_valid_extents)
	{
		if (path)
			cached_bbox = path->GetBoundingBox();
		else
			cached_bbox.Clear();

		has_valid_extents = TRUE;
	}
	return cached_bbox;
}

void SVGPolygonNode::PaintContent(SVGPainter* painter)
{
	if (!pointlist)
		return;

	TransferObjectProperties(painter);

	painter->BeginDrawing(this);

	OpStatus::Ignore(painter->DrawPolygon(*pointlist, is_closed));

	painter->EndDrawing();

#ifdef SVG_SUPPORT_MARKERS
	if (has_markers)
	{
		SVGMarkerPointListPosIterator ptlist_iter(pointlist, is_closed);
		PaintMarkers(painter, &ptlist_iter);
	}
#endif // SVG_SUPPORT_MARKERS
}

#ifdef SVG_SUPPORT_STENCIL
void SVGPolygonNode::ClipContent(SVGPainter* painter)
{
	if (!pointlist)
		return;

	TransferObjectProperties(painter);

	OpStatus::Ignore(painter->DrawPolygon(*pointlist, is_closed));
}
#endif // SVG_SUPPORT_STENCIL

void SVGPolygonNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (!pointlist)
		return;

	SVGBoundingBox lbbox = GetBBox();

	SVGNumber ext;
	if (obj_props.stroked)
	{
		SVGMatrix host_inv;
		if (stroke_props.non_scaling)
		{
			host_inv = GetHostInverseTransform();
		}

		ext = stroke_props.Extents(host_inv);
		lbbox.Extend(ext);
	}

#ifdef SVG_SUPPORT_MARKERS
	if (has_markers)
	{
		SVGMarkerPointListPosIterator ptlist_iter(pointlist, is_closed);
		lbbox.UnionWith(MarkerExtents(markers, &ptlist_iter));
	}
#endif // SVG_SUPPORT_MARKERS

	extents = lbbox;
}

SVGBoundingBox SVGPolygonNode::GetBBox()
{
	if (!has_valid_extents)
	{
		if (pointlist)
		{
			for (unsigned i = 0; i < pointlist->GetCount(); i++)
			{
				SVGPoint* c = static_cast<SVGPoint*>(pointlist->Get(i));
				if (!c)
					continue;

				cached_bbox.minx = SVGNumber::min_of(cached_bbox.minx, c->x);
				cached_bbox.miny = SVGNumber::min_of(cached_bbox.miny, c->y);
				cached_bbox.maxx = SVGNumber::max_of(cached_bbox.maxx, c->x);
				cached_bbox.maxy = SVGNumber::max_of(cached_bbox.maxy, c->y);
			}
		}
		else
		{
			cached_bbox.Clear();
		}

		has_valid_extents = TRUE;
	}
	return cached_bbox;
}

OP_STATUS SVGRasterImageNode::DrawImageFromURL(SVGPainter* painter)
{
	UrlImageContentProvider	*provider = UrlImageContentProvider::FindImageContentProvider(image_url);
	if (!provider)
		return OpStatus::OK;

#ifdef _PRINT_SUPPORT_
	// Call this magic function (magic to me, at least) that creates a
	// fitting HEListElm for images in print documents that we fetch
	// below.
	if (frames_doc->IsPrintDocument())
		frames_doc->GetHLDocProfile()->AddPrintImageElement(image_element, IMAGE_INLINE, &image_url);
#endif // _PRINT_SUPPORT_

	HEListElm* hle = image_element->GetHEListElm(FALSE);
	if (!hle)
		return OpStatus::ERR;

	OP_STATUS result = OpSVGStatus::DATA_NOT_LOADED_ERROR;

	provider->IncRef();

	// Scope so that the Image object is destroyed before the
	// provider.
	{
		// To force loading - we use a big rectangle to make sure that
		// the image isn't undisplayed by accident when the user
		// scrolls The best rect would be the position and size of the
		// svg in the document, but we don't have access to that here.
		// Triggers an IncVisible that the HEListElm
		// owns. Unfortunately gets the wrong coordinates.
		hle->Display(frames_doc, AffinePos(), 20000, 1000000, FALSE, FALSE);

		Image img = provider->GetImage();

		result = img.OnLoadAll(provider);

		if (OpStatus::IsSuccess(result) && img.ImageDecoded())
		{
			OpBitmap* bitmap = NULL;

			if (img.IsAnimated())
				// Won't find proper frame of animation unless using
				// the HEListElm here
				bitmap = img.GetBitmap(hle);
			else
				bitmap = img.GetBitmap(null_image_listener);

			if (bitmap)
			{
				SVGRect src(0, 0, bitmap->Width(), bitmap->Height());

				SVGMatrix viewboxtransform;
				SVGRect image_rect;
				// This will only return an error if viewbox w/h < 0,
				// which can guarantee won't happen here
				OpStatus::Ignore(SVGUtils::GetViewboxTransform(viewport, &src, aspect,
															   viewboxtransform, image_rect));

				// FIXME: Clipping

				result = painter->PushTransform(viewboxtransform);
				if (OpStatus::IsSuccess(result))
				{
					OpRect bm_rect = image_rect.GetSimilarRect();

					result = painter->DrawImage(bitmap, bm_rect, image_rect, quality);

					painter->PopTransform();
				}

				img.ReleaseBitmap();
			}
		}
		else if (img.IsFailed())
		{
			result = OpStatus::ERR;
		}
	}
	// Here the img object leaves scope and is destroyed so that
	// we can decref the provider.

	provider->DecRef();

	if (OpStatus::IsError(result) && result != OpSVGStatus::DATA_NOT_LOADED_ERROR)
		DrawImagePlaceholder(painter, viewport);

	return result;
}

void SVGRasterImageNode::SetImageAspect(SVGAspectRatio* ar)
{
	SVGObject::IncRef(ar);
	SVGObject::DecRef(aspect);
	aspect = ar;
}

void SVGRasterImageNode::UpdateState(SVGCanvasState* state)
{
	// If the background/viewport-fill is non-transparent, we still need a layer
	can_skip_layer = !HasFilter() && background.IsTransparent();
}

void SVGRasterImageNode::PaintContent(SVGPainter* painter)
{
	if (!background.IsTransparent())
		painter->DrawRectWithPaint(viewport, background);

	if (can_skip_layer)
		painter->SetImageOpacity(opacity);

	OpStatus::Ignore(DrawImageFromURL(painter));

	if (can_skip_layer)
		painter->SetImageOpacity(255);
}

void SVGRasterImageNode::GetLocalExtents(SVGBoundingBox& extents)
{
	extents.UnionWith(viewport);
}

SVGBoundingBox SVGRasterImageNode::GetBBox()
{
	SVGBoundingBox bbox;
	bbox.UnionWith(viewport);
	return bbox;
}

SVGVectorImageNode::~SVGVectorImageNode()
{
	FreeChildren();

	SVGObject::DecRef(aspect);
}

void SVGVectorImageNode::SetImageAspect(SVGAspectRatio* ar)
{
	SVGObject::IncRef(ar);
	SVGObject::DecRef(aspect);
	aspect = ar;
}

void SVGVectorImageNode::PaintContent(SVGPainter* painter)
{
	if (!background.IsTransparent())
		painter->DrawRectWithPaint(viewport, background);

	SVGMatrix vp_xfrm;
	vp_xfrm.LoadTranslation(viewport.x, viewport.y);

	if (OpStatus::IsSuccess(painter->PushTransform(vp_xfrm)))
	{
		SVGCompositePaintNode::PaintContent(painter);

		painter->PopTransform();
	}
}

void SVGVectorImageNode::GetLocalExtents(SVGBoundingBox& extents)
{
	SVGCompositePaintNode::GetLocalExtents(extents);

	if (!extents.IsEmpty())
	{
		extents.minx += viewport.x;
		extents.maxx += viewport.x;
		extents.miny += viewport.y;
		extents.maxy += viewport.y;
	}
}

SVGBoundingBox SVGVectorImageNode::GetBBox()
{
	SVGBoundingBox bbox;
	bbox.UnionWith(viewport);
	return bbox;
}

#ifdef SVG_SUPPORT_FOREIGNOBJECT
SVGBoundingBox SVGForeignObjectNode::GetBBox()
{
	SVGBoundingBox bbox;
	bbox.UnionWith(viewport);
	return bbox;
}

void SVGForeignObjectNode::GetLocalExtents(SVGBoundingBox& extents)
{
	extents.UnionWith(viewport);
}

void SVGForeignObjectNode::UpdateState(SVGCanvasState* state)
{
	// If the background/viewport-fill is non-transparent, we still need a layer
	can_skip_layer = !HasFilter() && background.IsTransparent();
}

void SVGForeignObjectHrefNode::PaintContent(SVGPainter* painter)
{
	const int fx = viewport.x.GetIntegerValue();
	const int fy = viewport.y.GetIntegerValue();
	const int fw = viewport.width.GetIntegerValue();
	const int fh = viewport.height.GetIntegerValue();

	if (frame && (frame->GetWidth() != fw || frame->GetHeight() != fh))
	{
		VisualDevice* frame_vd = frame->GetVisualDevice();

		// FIXME: too many conversions between screen and document coordinates here
		// FIXME: Plug in the real CTM?
		AffinePos frame_pos(frame_vd->ScaleToDoc(fx), frame_vd->ScaleToDoc(fy));
		frame->SetGeometry(frame_pos, frame_vd->ScaleToDoc(fw), frame_vd->ScaleToDoc(fh));
	}

	if (!background.IsTransparent())
		painter->DrawRectWithPaint(viewport, background);

	if (!frame || !frame->GetVisualDevice())
	{
		DrawImagePlaceholder(painter, viewport);
		return;
	}

	// FIXME: is this really needed? - ed
	// Paint it black?
	if (!vis_dev->IsPainting())
		return;

	VisualDeviceBackBuffer* buffer;
	OpRect docrect(0, 0, fw, fh);

	VDState vdstate = vis_dev->PushState();
	VDCTMState vdctmstate = vis_dev->SaveCTM();
	VDStateNoTranslationNoOffset nooffsetstate = vis_dev->BeginNoTranslationNoOffsetPainting();
	VDStateNoScale noscalestate = vis_dev->BeginNoScalePainting(docrect);

	OP_STATUS bb_result = vis_dev->BeginBackbuffer(docrect, 255, FALSE, FALSE, buffer);
	if (OpStatus::IsError(bb_result))
	{
		vis_dev->EndNoScalePainting(noscalestate);
		vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
		vis_dev->RestoreCTM(vdctmstate);
		vis_dev->PopState(vdstate);

		DrawImagePlaceholder(painter, viewport);
		return;
	}

	AffinePos frame_pos;
	frame->SetPosition(frame_pos);
	frame->ShowFrames(); // FIXME: OOM

	VisualDevice* frame_vd = frame->GetVisualDevice();
	frame_vd->GetContainerView()->SetReference(frames_doc, foreign_object_element);
	frame_vd->Paint(docrect, vis_dev);

	SVGRect dst_rect = viewport;

#if 0 // ed-temp
#ifdef SVG_SUPPORT_STENCIL
	SVGContentClipper clipper;
	const HTMLayoutProperties& props = *info.props->GetProps();
	clipper.Begin(m_canvas, dst_rect, props);
#endif // SVG_SUPPORT_STENCIL
#endif // 0 ed-temp

	OpBitmap* bm = buffer->GetBitmap();
	OpRect bm_rect = buffer->GetBitmapRect();

	if (can_skip_layer)
		painter->SetImageOpacity(opacity);

	OpStatus::Ignore(painter->DrawImage(bm, bm_rect, dst_rect, quality));

	if (can_skip_layer)
		painter->SetImageOpacity(255);

	vis_dev->EndBackbuffer(FALSE);
	vis_dev->EndNoScalePainting(noscalestate);
	vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
	vis_dev->RestoreCTM(vdctmstate);
	vis_dev->PopState(vdstate);
}

void SVGForeignObjectNode::PaintContent(SVGPainter* painter)
{
	// FIXME: Traverse/draw the tree using a painter to draw directly to the svg instead of via a bitmap

	// FIXME: is this really needed?
	// Paint it black?
	if (!vis_dev->IsPainting())
		return;

	if (!background.IsTransparent())
		painter->DrawRectWithPaint(viewport, background);

	if (!foreign_object_element)
	{
		return;
	}

	OpRect docrect(0, 0, viewport.width.GetIntegerValue(), viewport.height.GetIntegerValue());

	VDState vdstate = vis_dev->PushState();
	VDCTMState vdctmstate = vis_dev->SaveCTM();
	VDStateNoTranslationNoOffset nooffsetstate = vis_dev->BeginNoTranslationNoOffsetPainting();
	VDStateNoScale noscalestate = vis_dev->BeginNoScalePainting(docrect);

	VisualDeviceBackBuffer* buffer;
	OP_STATUS bb_result = vis_dev->BeginBackbuffer(docrect, 255, FALSE, FALSE, buffer);
	if (OpStatus::IsError(bb_result))
	{
		vis_dev->EndNoScalePainting(noscalestate);
		vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
		vis_dev->RestoreCTM(vdctmstate);
		vis_dev->PopState(vdstate);

		return;
	}

	RECT paint_area;
	paint_area.top = 0;
	paint_area.left = 0;
	paint_area.right = viewport.width.GetIntegerValue();
	paint_area.bottom = viewport.height.GetIntegerValue();

	PaintObject paintobj(frames_doc, vis_dev, paint_area, NULL, NULL,
#ifdef _PRINT_SUPPORT_
						 FALSE,
#endif // _PRINT_SUPPORT_
						 1, 0xFFFFFFFF, 0x0);

	HLDocProfile* hld_profile = frames_doc->GetHLDocProfile();
	Head prop_list;
	if (!hld_profile)
	{
		vis_dev->EndBackbuffer(FALSE);
		vis_dev->EndNoScalePainting(noscalestate);
		vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
		vis_dev->RestoreCTM(vdctmstate);
		vis_dev->PopState(vdstate);
		return; // OpStatus::ERR;
	}

	if (!LayoutProperties::CreateCascade(foreign_object_element, prop_list, LAYOUT_WORKPLACE(hld_profile)))
	{
		vis_dev->EndBackbuffer(FALSE);
		vis_dev->EndNoScalePainting(noscalestate);
		vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
		vis_dev->RestoreCTM(vdctmstate);
		vis_dev->PopState(vdstate);
		return; // OpStatus::ERR_NO_MEMORY;
	}

	// This is to work around an assert in layout, see bug 289828
	LayoutWorkplace* lwp = hld_profile->GetLayoutWorkplace();
	BOOL old_traversing = lwp->IsTraversing();
	lwp->SetIsTraversing(FALSE);
	paintobj.Traverse(foreign_object_element, &prop_list, FALSE);
	lwp->SetIsTraversing(old_traversing);

	prop_list.Clear();

	SVGRect dst_rect = viewport;

	// FIXME: is this clipping part really needed?
#if 0
#ifdef SVG_SUPPORT_STENCIL
	SVGContentClipper clipper;
	const HTMLayoutProperties& props = *info.props->GetProps();
	clipper.Begin(m_canvas, dst_rect, props);
#endif // SVG_SUPPORT_STENCIL
#endif

	OpBitmap* bm = buffer->GetBitmap();
	OpRect bm_rect = buffer->GetBitmapRect();

	if (can_skip_layer)
		painter->SetImageOpacity(opacity);

	OpStatus::Ignore(painter->DrawImage(bm, bm_rect, dst_rect, quality));

	if (can_skip_layer)
		painter->SetImageOpacity(255);

	vis_dev->EndBackbuffer(FALSE);
	vis_dev->EndNoScalePainting(noscalestate);
	vis_dev->EndNoTranslationNoOffsetPainting(nooffsetstate);
	vis_dev->RestoreCTM(vdctmstate);
	vis_dev->PopState(vdstate);
}
#endif // SVG_SUPPORT_FOREIGNOBJECT

#ifdef SVG_SUPPORT_MEDIA
#ifdef PI_VIDEO_LAYER
BOOL SVGVideoNode::CheckOverlay(SVGPainter* painter, const SVGRect& video_rect)
{
	if (allow_overlay && painter->AllowOverlay())
	{
		// User space coordinates -> SVG host coordinates
		OpRect painter_rect = painter->GetTransform().ApplyToRect(video_rect).GetSimilarRect();
		OpRect clipped_painter_rect = painter_rect;

		painter->ApplyClipping(clipped_painter_rect);

		if (mbind->UpdateOverlay(painter_rect, clipped_painter_rect))
			// Clear video area to transparent. Clear works in host coordinates.
			painter->ClearRect(painter_rect, 0);

		return TRUE;
	}
	else
	{
		mbind->DisableOverlay();
		return FALSE;
	}
}
#endif // PI_VIDEO_LAYER

void SVGVideoNode::SetVideoAspect(SVGAspectRatio* ar)
{
	SVGObject::IncRef(ar);
	SVGObject::DecRef(aspect);
	aspect = ar;
}

void SVGVideoNode::PaintContent(SVGPainter* painter)
{
	if (!background.IsTransparent())
		painter->DrawRectWithPaint(viewport, background);

	if (!mbind)
		return;

	MediaPlayer* mplayer = mbind->GetPlayer();
	unsigned video_width, video_height;
	mplayer->GetVideoSize(video_width, video_height);

	if (video_width == 0 || video_height == 0)
		return;

	OP_STATUS status;
	SVGRect video_rect;

	if (is_pinned)
	{
		SVGMatrix pinned_video_mtx;
		status = painter->PushTransform(pinned_video_mtx);

		SVGNumberPair pinned_pos(viewport.x, viewport.y);
		pinned_pos = painter->GetTransform().ApplyToCoordinate(pinned_pos);

		pinned_pos.x = pinned_pos.x.GetIntegerValue();
		pinned_pos.y = pinned_pos.y.GetIntegerValue();

		pinned_video_mtx.LoadTranslation(pinned_pos.x, pinned_pos.y);

		if (pinned_rotation > 0)
		{
			SVGMatrix rot;
			rot.LoadRotation(pinned_rotation * 90);

			pinned_video_mtx.PostMultiply(rot);
		}

		if (OpStatus::IsSuccess(status))
		{
			painter->SetTransform(pinned_video_mtx);

			video_rect.x = -(int)(video_width / 2);
			video_rect.y = -(int)(video_height / 2);
			video_rect.width = video_width;
			video_rect.height = video_height;
		}
	}
	else
	{
		SVGRect src(0, 0, video_width, video_height);

		SVGMatrix viewboxtransform;
		// This will only return an error if viewbox w/h < 0,
		// which we can guarantee won't happen here
		OpStatus::Ignore(SVGUtils::GetViewboxTransform(viewport, &src, aspect,
													   viewboxtransform, video_rect));

		// FIXME: Clipping

		status = painter->PushTransform(viewboxtransform);
	}

	if (OpStatus::IsSuccess(status))
	{
		if (!CheckOverlay(painter, video_rect))
		{
			OpBitmap* video_frame;
			if (OpStatus::IsSuccess(mplayer->GetFrame(video_frame)))
			{
				OpRect bm_rect(0, 0, video_frame->Width(), video_frame->Height());
				OpStatus::Ignore(painter->DrawImage(video_frame, bm_rect, video_rect, quality));
			}
		}

		painter->PopTransform();
	}
}

void SVGVideoNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (is_pinned)
	{
		extents = pinned_extents;
	}
	else
	{
		extents = viewport;
	}
}

SVGBoundingBox SVGVideoNode::GetBBox()
{
	if (is_pinned)
		return pinned_extents;

	SVGBoundingBox bbox;
	bbox.UnionWith(viewport);
	return bbox;
}
#endif // SVG_SUPPORT_MEDIA

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGClipPathNode::Apply(SVGPainter* painter, SVGPaintNode* context_node)
{
	PaintPropertySet propset;

	// Apply nested clipPath
	if (clippath)
	{
		RETURN_IF_ERROR(clippath->Apply(painter, context_node));

		propset.Set(PAINTPROP_CLIPPATH);
	}

	SVGMatrix additional_transform;
	context_node->GetAdditionalTransform(additional_transform);
	OP_STATUS at_status = painter->PushTransform(additional_transform);

	if (is_bbox_relative)
	{
		const SVGBoundingBox& context_bbox = context_node->GetBBox();

		if (!context_bbox.IsEmpty())
		{
			SVGMatrix context_mtrx;
			context_mtrx.SetValues(context_bbox.maxx - context_bbox.minx, 0, 0,
								   context_bbox.maxy - context_bbox.miny,
								   context_bbox.minx, context_bbox.miny);

			OP_STATUS status = painter->PushTransform(context_mtrx);
			if (OpStatus::IsError(status))
			{
				// Pop additional transform.
				if (OpStatus::IsSuccess(at_status))
					painter->PopTransform();

				// Remove any nested clipPath
				if (propset.IsSet(PAINTPROP_CLIPPATH))
					painter->RemoveClip();

				return status;
			}

			propset.Set(PAINTPROP_TRANSFORM);
		}
	}

	OP_STATUS status = painter->PushTransform(ustransform);

	if (OpStatus::IsSuccess(status))
	{
		if (is_rectangle)
		{
			OP_ASSERT(clippath == NULL);

			// FIXME: AddClipRect wants to get aa_quality from somewhere
			status = painter->AddClipRect(cliprect);
		}
		else
		{
			OP_STATUS clip_status = painter->BeginClip();
			if (OpStatus::IsSuccess(clip_status))
				// Open-coding SVGPaintNode::Clip here because we don't
				// wan't to apply any nested clip twice
				ClipContent(painter);

			// Remove any nested clipPath
			if (propset.IsSet(PAINTPROP_CLIPPATH))
				painter->RemoveClip();

			if (OpStatus::IsSuccess(clip_status))
				painter->EndClip();

			if (OpStatus::IsSuccess(status))
				status = clip_status;
		}

		painter->PopTransform();
	}

	// Pop bbox relative transform.
	if (propset.IsSet(PAINTPROP_TRANSFORM))
		painter->PopTransform();

	// Pop additional transform.
	if (OpStatus::IsSuccess(at_status))
		painter->PopTransform();

	return status;
}

void SVGClipPathNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (is_rectangle)
	{
		extents.UnionWith(cliprect);
	}
	else
	{
		SVGCompositePaintNode::GetLocalExtents(extents);
	}
}

// FIXME: Possible detect other cases, such as <path> and <polygon>
// describing a rectangle. Could also want to consider nested
// rectangles (via use).
void SVGClipPathNode::Optimize()
{
	SetIsRectangle(FALSE);

	// Currently can't handle the clip having clipping itself
	if (clippath != NULL)
		return;

	// If child-less this is point-less, and more than 1 child is too many
	if (children.Empty() || children.First() != children.Last())
		return;

	// Check the child
	SVGPaintNode* child_node = children.First();

	// Can't have clipping applied to itself, must be a primitive node and visible
	if (child_node->HasClipPath() ||
		child_node->Type() != PRIMITIVE_NODE ||
		!child_node->IsVisible())
		return;

	// Primitive must be a(n ordinary) rectangle
	SVGPrimitiveNode* prim_node = static_cast<SVGPrimitiveNode*>(child_node);
	if (!prim_node->IsRectangle())
		return;

	SetIsRectangle(TRUE);
	SetRectangle(prim_node->GetRectangle());
	ustransform.PostMultiply(prim_node->GetTransform());
}

OP_STATUS SVGMaskNode::Apply(SVGPainter* painter, SVGPaintNode* context_node)
{
	// The cliprect is resolved during layout and hence already
	// includes the additional transform.
	OP_STATUS cr_status = painter->AddClipRect(cliprect);

	SVGMatrix additional_transform;
	context_node->GetAdditionalTransform(additional_transform);
	OP_STATUS at_status = painter->PushTransform(additional_transform);

	PaintPropertySet propset;

	if (is_bbox_relative)
	{
		const SVGBoundingBox& context_bbox = context_node->GetBBox();

		if (!context_bbox.IsEmpty())
		{
			SVGMatrix context_mtrx;
			context_mtrx.SetValues(context_bbox.maxx - context_bbox.minx, 0, 0,
								   context_bbox.maxy - context_bbox.miny,
								   context_bbox.minx, context_bbox.miny);

			OP_STATUS status = painter->PushTransform(context_mtrx);
			if (OpStatus::IsError(status))
			{
				// Pop additional transform.
				if (OpStatus::IsSuccess(at_status))
					painter->PopTransform();

				// Remove any nested clipPath
				if (OpStatus::IsSuccess(cr_status))
					painter->RemoveClip();

				return status;
			}

			propset.Set(PAINTPROP_TRANSFORM);
		}
	}

	OP_STATUS status = painter->BeginMask();
	if (OpStatus::IsSuccess(status))
	{
		status = Paint(painter);

		painter->EndMask();
	}

	if (OpStatus::IsSuccess(cr_status))
		painter->RemoveClip();

	if (OpStatus::IsSuccess(status))
		status = cr_status;

	// Pop bbox relative transform.
	if (propset.IsSet(PAINTPROP_TRANSFORM))
		painter->PopTransform();

	// Pop additional transform.
	if (OpStatus::IsSuccess(at_status))
		painter->PopTransform();

	return status;
}

void SVGMaskNode::GetLocalExtents(SVGBoundingBox& extents)
{
	SVGCompositePaintNode::GetLocalExtents(extents);

	extents.IntersectWith(cliprect);
}
#endif // SVG_SUPPORT_STENCIL

void SVGCompositePaintNode::Clear()
{
	while (SVGPaintNode* child = children.First())
		child->Detach();
}

void SVGCompositePaintNode::FreeChildren()
{
	while (SVGPaintNode* child = children.First())
		child->Free();
}

void SVGCompositePaintNode::Free()
{
	FreeChildren();

	OP_DELETE(this);
}

void SVGCompositePaintNode::Insert(SVGPaintNode* node, SVGPaintNode* pred)
{
	if (children.HasLink(node) && node->Pred() == pred)
		return;

	node->Out();

	if (pred)
	{
		node->Follow(pred);
	}
	else
	{
		node->IntoStart(&children);
	}

	node->SetParent(this);
}

#ifdef SVG_SUPPORT_FILTERS
OP_STATUS SVGCompositePaintNode::Paint(SVGPainter* painter)
{
	BOOL pushed_background_layer = FALSE;
	if (has_bglayer)
	{
		OpRect host_extents = GetTransformedExtents(painter->GetTransform());
		if (OpStatus::IsSuccess(painter->PushBackgroundLayer(host_extents, new_bglayer, opacity)))
			pushed_background_layer = TRUE;
	}

	OP_STATUS status = SVGPaintNode::Paint(painter);

	if (pushed_background_layer)
		painter->PopBackgroundLayer();

	return status;
}
#endif // SVG_SUPPORT_FILTERS

void SVGCompositePaintNode::PaintContent(SVGPainter* painter)
{
	SVGPaintNode* child = children.First();
	while (child)
	{
		// FIXME/OPT?: Add containment check, to avoid extents
		// calculations/checks if we know that the entire
		// parent/subtree is contained in the visible region
		// (interaction with clips?)

		if (child->IsVisible() && child->GetOpacity() != 0)
		{
			OpRect child_host_extents = child->GetTransformedExtents(painter->GetTransform());
			if (painter->IsVisible(child_host_extents))
				child->Paint(painter);
		}

		child = child->Suc();
	}
}

#ifdef SVG_SUPPORT_STENCIL
void SVGCompositePaintNode::ClipContent(SVGPainter* painter)
{
	SVGPaintNode* child = children.First();
	while (child)
	{
		if (child->IsVisible())
			child->Clip(painter);

		child = child->Suc();
	}
}
#endif // SVG_SUPPORT_STENCIL

SVGMatrix SVGPaintNode::GetRefTransformToParent()
{
	// Fetch the root transform
	NodeContext context;
	GetParentContext(context, FALSE);

	SVGMatrix ctm = context.transform;
	if (!ctm.Invert())
		ctm.LoadIdentity();

	ctm.PostMultiply(context.root_transform);
	ctm.PostMultiply(GetTransform());
	return ctm;
}

void SVGCompositePaintNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (!has_valid_extents)
	{
		cached_extents.Clear();

		SVGPaintNode* child = children.First();
		while (child)
		{
			// FIXME: Factor in the clip, and use it to do branch
			// pruning (if for instance the clip constrains the
			// subtree enough for it to inhibit the extents from
			// growing)

			SVGBoundingBox child_extents;
			child->GetLocalExtentsWithFilter(child_extents);

			if (!child_extents.IsEmpty())
			{
				if (child->HasRefTransform())
				{
					SVGMatrix parent_ref_xfrm = child->GetRefTransformToParent();
					child_extents = parent_ref_xfrm.ApplyToBoundingBox(child_extents);
				}
				else
				{
					const SVGMatrix& child_mtx = child->GetTransform();
					if (!child_mtx.IsIdentity())
						child_extents = child_mtx.ApplyToBoundingBox(child_extents);
				}

				cached_extents.UnionWith(child_extents);
			}

			child = child->Suc();
		}

		has_valid_extents = TRUE;
	}

	extents = cached_extents;
}

SVGBoundingBox SVGCompositePaintNode::GetBBox()
{
	// FIXME: Cache?
	SVGBoundingBox bbox;

	SVGPaintNode* child = children.First();
	while (child)
	{
		SVGBoundingBox child_bbox = child->GetBBox();
		if (!child_bbox.IsEmpty())
		{
			const SVGMatrix& child_mtx = child->GetTransform();
			if (!child_mtx.IsIdentity())
				child_bbox = child_mtx.ApplyToBoundingBox(child_bbox);

			bbox.UnionWith(child_bbox);
		}

		child = child->Suc();
	}
	return bbox;
}

BOOL SVGCompositePaintNode::HasNonscalingStroke()
{
	if (!has_valid_nsstroke)
	{
		cached_nsstroke = FALSE;

		SVGPaintNode* child = children.First();
		while (child)
		{
			if (child->HasNonscalingStroke())
			{
				cached_nsstroke = TRUE;
				break;
			}
			child = child->Suc();
		}

		has_valid_nsstroke = TRUE;
	}

	return cached_nsstroke;
}

void SVGOffsetCompositePaintNode::PaintContent(SVGPainter* painter)
{
	SVGMatrix offset_xfrm;
	offset_xfrm.LoadTranslation(offset.x, offset.y);

	if (OpStatus::IsSuccess(painter->PushTransform(offset_xfrm)))
	{
		SVGCompositePaintNode::PaintContent(painter);

		painter->PopTransform();
	}
}

#ifdef SVG_SUPPORT_STENCIL
void SVGOffsetCompositePaintNode::ClipContent(SVGPainter* painter)
{
	SVGMatrix offset_xfrm;
	offset_xfrm.LoadTranslation(offset.x, offset.y);

	if (OpStatus::IsSuccess(painter->PushTransform(offset_xfrm)))
	{
		SVGCompositePaintNode::ClipContent(painter);

		painter->PopTransform();
	}
}
#endif // SVG_SUPPORT_STENCIL

void SVGOffsetCompositePaintNode::GetLocalExtents(SVGBoundingBox& extents)
{
	SVGCompositePaintNode::GetLocalExtents(extents);

	if (!extents.IsEmpty())
	{
		extents.minx += offset.x;
		extents.miny += offset.y;
		extents.maxx += offset.x;
		extents.maxy += offset.y;
	}
}

SVGBoundingBox SVGOffsetCompositePaintNode::GetBBox()
{
	SVGBoundingBox bbox = SVGCompositePaintNode::GetBBox();
	if (!bbox.IsEmpty())
	{
		bbox.minx += offset.x;
		bbox.miny += offset.y;
		bbox.maxx += offset.x;
		bbox.maxy += offset.y;
	}
	return bbox;
}

void SVGOffsetCompositePaintNode::GetAdditionalTransform(SVGMatrix& addmtx)
{
	addmtx.LoadTranslation(offset.x, offset.y);
}

void SVGOffsetCompositePaintNode::GetNodeContext(NodeContext& context, BOOL for_child)
{
	SVGPaintNode::GetNodeContext(context, for_child);

	if (for_child)
		// If the context is supposed to be used by a child node of
		// this node, then we need to provide the offset as part of
		// the transform.
		context.transform.PostMultTranslation(offset.x, offset.y);
}

OP_STATUS SVGViewportCompositePaintNode::Paint(SVGPainter* painter)
{
	SVGMatrix scale_xfrm;
	scale_xfrm.LoadScale(scale, scale);
	RETURN_IF_ERROR(painter->PushTransform(scale_xfrm));

#ifdef SVG_SUPPORT_STENCIL
	BOOL added_cliprect = FALSE;
	if (cliprect_enabled)
		if (OpStatus::IsSuccess(painter->AddClipRect(cliprect)))
			added_cliprect = TRUE;
#endif // SVG_SUPPORT_STENCIL

	if (!fillpaint.IsTransparent())
		painter->DrawRectWithPaint(cliprect, fillpaint);

	OP_STATUS status = painter->PushTransform(user_transform);
	if (OpStatus::IsSuccess(status))
	{
		status = SVGCompositePaintNode::Paint(painter);

		painter->PopTransform();
	}

#ifdef SVG_SUPPORT_STENCIL
	if (added_cliprect)
		painter->RemoveClip();
#endif // SVG_SUPPORT_STENCIL

	painter->PopTransform();
	return status;
}

void SVGViewportCompositePaintNode::GetNodeContext(NodeContext& context, BOOL for_child)
{
	if (!parent)
	{
		context.transform.LoadScale(scale, scale);
		context.clip = context.transform.ApplyToBoundingBox(cliprect).GetEnclosingRect();

		context.root_transform = context.transform;
		context.root_transform.PostMultiply(ustransform);

		context.transform.PostMultiply(user_transform);
	}

	SVGPaintNode::GetNodeContext(context);
}

OpRect SVGPaintNode::GetTransformedExtents(const SVGMatrix& parent_transform)
{
	SVGBoundingBox extents;
	GetLocalExtentsWithFilter(extents);

	if (extents.IsEmpty())
		return OpRect();

	SVGMatrix ctm;
	if (ref_transform)
	{
		// Need the root transform
		NodeContext context;
		GetParentContext(context, FALSE);

		ctm = context.root_transform;
	}
	else
	{
		ctm = parent_transform;
	}

	const SVGMatrix& mtx = GetTransform();
	if (!mtx.IsIdentity())
		ctm.PostMultiply(mtx);

	return ctm.ApplyToNonEmptyBoundingBox(extents).GetEnclosingRect();
}

OpRect SVGPaintNode::GetPixelExtents(const NodeContext& context)
{
	SVGBoundingBox node_extents;
	GetLocalExtentsWithFilter(node_extents);

	node_extents = context.transform.ApplyToBoundingBox(node_extents);

	return node_extents.GetEnclosingRect();
}

OpRect SVGPaintNode::GetPixelExtentsWithFilter(const NodeContext& context)
{
	SVGBoundingBox node_extents;
	GetLocalExtentsWithFilter(node_extents);

#ifdef SVG_SUPPORT_FILTERS
	if (context.has_filter)
	{
		// Walk the path to the root while applying filter information
		SVGPaintNode* node = GetParent();
		while (node)
		{
			if (SVGFilter* filter = node->filter)
			{
				node_extents = filter->GetFilteredExtents(node_extents, node);

				if (node_extents.IsEmpty())
					return OpRect();
			}

			node = node->GetParent();
		}
	}
#endif // SVG_SUPPORT_FILTERS

	node_extents = context.transform.ApplyToBoundingBox(node_extents);

	return node_extents.GetEnclosingRect();
}

void SVGPaintNode::GetNodeContext(NodeContext& context, BOOL for_child)
{
	// Apply userspace transform
	if (ref_transform)
		context.transform = context.root_transform;

	if (!ustransform.IsIdentity())
		context.transform.PostMultiply(ustransform);

	context.has_filter = context.has_filter || HasFilter();

#ifdef SVG_SUPPORT_STENCIL
	// Apply clip
	if (clippath)
	{
		NodeContext clippath_context = context;
		clippath->GetNodeContext(clippath_context);

		context.clip.IntersectWith(clippath->GetPixelExtents(clippath_context));
	}
#endif // SVG_SUPPORT_STENCIL
}

OP_STATUS SVGPaintNode::GetParentContext(NodeContext& context, BOOL invalidate_extents)
{
	SVGStack<SVGPaintNode> stack;
	SVGPaintNode* node = GetParent();

	// Create path
	while (node)
	{
		RETURN_IF_ERROR(stack.Push(node));
		node = node->GetParent();
	}

	// Get context for parents
	while (!stack.IsEmpty())
	{
		node = stack.Pop();

		node->GetNodeContext(context, TRUE);

		if (invalidate_extents)
		{
			node->has_valid_extents = FALSE;
			node->has_clean_br_state = FALSE;
		}

		if (context.clip.IsEmpty())
			break;
	}
	return OpStatus::OK;
}

BOOL SVGPaintNode::Update(SVGRenderer* renderer)
{
	NodeContext context;
	RETURN_VALUE_IF_ERROR(GetParentContext(context, FALSE), FALSE);

	if (GetParent() && context.clip.IsEmpty())
		return FALSE;

	return AddPixelExtents(context, renderer);
}

OpRect SVGPaintNode::GetPixelExtents()
{
	OpRect pixel_extents;
	NodeContext context;
	RETURN_VALUE_IF_ERROR(GetParentContext(context, FALSE), pixel_extents);

	if (GetParent() && context.clip.IsEmpty())
		return pixel_extents;

	// Get context for this node
	GetNodeContext(context);

	pixel_extents = GetPixelExtentsWithFilter(context);
	pixel_extents.IntersectWith(context.clip);

	return pixel_extents;
}

BOOL SVGPaintNode::AddPixelExtents(const NodeContext& parent_context, SVGRenderer* renderer,
								   OpRect* update_extents /* = NULL */)
{
	NodeContext context = parent_context;

	// Get context for this node
	GetNodeContext(context);

	OpRect pixel_extents = GetPixelExtentsWithFilter(context);
	pixel_extents.IntersectWith(context.clip);

	if (pixel_extents.IsEmpty())
		return FALSE;

	if (update_extents)
		update_extents->UnionWith(pixel_extents);

	renderer->GetInvalidState()->AddExtraInvalidation(pixel_extents);
	return TRUE;
}

OP_STATUS SVGPaintNode::UpdateTransform(SVGRenderer* renderer, const SVGMatrix* transform,
										OpRect& update_extents)
{
	NodeContext parent_context;
	RETURN_IF_ERROR(GetParentContext(parent_context, TRUE));

	// The node is clipped by one of its parents - no need to update
	BOOL is_clipped = GetParent() && parent_context.clip.IsEmpty();

	SVGNumber new_ef = transform ? transform->GetExpansionFactorSquared() : SVGNumber(1);
	SVGNumber old_ef = GetTransform().GetExpansionFactorSquared();
	BOOL changes_expansion_factor = !new_ef.Close(old_ef);
	if (changes_expansion_factor)
	{
		// Find out if there are any dependending child elements
		changes_expansion_factor = HasNonscalingStroke();
	}

	if (!is_clipped)
		AddPixelExtents(parent_context, renderer, &update_extents);

	// Update local transform
	if (transform)
		SetTransform(*transform);
	else
		ResetTransform();

	// A child may invalidate the extents if the scale factor changed
	if (changes_expansion_factor)
	{
		MarkExtentsDirty();
	}

	if (!is_clipped)
		AddPixelExtents(parent_context, renderer, &update_extents);

	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_MARKERS
OP_STATUS SVGMarkerNode::Apply(SVGPainter* painter, SVGNumber current_slope, const SVGNumberPair& marker_pos)
{
	SVGMatrix marker_view_xfrm;

	marker_view_xfrm.Copy(m_marker_base_xfrm);
	PositionAndOrient(marker_view_xfrm, current_slope, marker_pos);

	RETURN_IF_ERROR(painter->PushTransform(marker_view_xfrm));

	BOOL added_cliprect = FALSE;
	// Draw clip
	if (cliprect_enabled)
		if (OpStatus::IsSuccess(painter->AddClipRect(cliprect)))
			added_cliprect = TRUE;

	OP_STATUS status = OpStatus::OK;
	SVGCompositePaintNode::PaintContent(painter);

	if (added_cliprect)
		painter->RemoveClip();

	painter->PopTransform();
	return status;
}

void SVGMarkerNode::GetLocalMarkerExtents(SVGNumber current_slope, const SVGNumberPair& marker_pos, SVGBoundingBox& extents)
{
	SVGMatrix marker_view_xfrm;

	marker_view_xfrm.Copy(m_marker_base_xfrm);
	PositionAndOrient(marker_view_xfrm, current_slope, marker_pos);

	SVGCompositePaintNode::GetLocalExtents(extents); // FIXME: Should this be GetLocalExtentsWithFilter?

	if (!extents.IsEmpty())
	{
		if (cliprect_enabled)
			extents.IntersectWith(cliprect);

		extents = marker_view_xfrm.ApplyToNonEmptyBoundingBox(extents);
	}
}

//
// Marker transforms:
//
// Ttot = { T(marker pos) * R(marker orientation) } * S(strokewidth | 1) *
//        T(-(ViewBox * ref. point)) * ViewBox
//
// The transforms within {...} will vary for each marker
//

// Prepend the marker specific part of Ttot
void SVGMarkerNode::PositionAndOrient(SVGMatrix& xfrm, SVGNumber current_slope, const SVGNumberPair& marker_pos)
{
	// Orient according to 'orient' attribute
	SVGMatrix orient_xfrm;
	if (is_orient_auto)
		orient_xfrm.LoadRotation(current_slope);
	else
		orient_xfrm.LoadRotation(m_angle);

	xfrm.Multiply(orient_xfrm);

	// Translate to position (of vertex)
	xfrm.MultTranslation(marker_pos.x, marker_pos.y);
}
#endif // SVG_SUPPORT_MARKERS

#endif // SVG_SUPPORT
