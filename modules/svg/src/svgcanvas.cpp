/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGCanvas.h"

#include "modules/svg/svg_number.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGVector.h"

#include "modules/pi/ui/OpUiInfo.h"

#include "modules/libvega/vegarenderer.h"

/** Destructor. */
SVGIntersectionCanvas::~SVGIntersectionCanvas()
{
#ifdef SVG_SUPPORT_STENCIL
	OP_ASSERT(m_active_clipsets.Empty());
	OP_ASSERT(m_partial_clipsets.Empty());
#endif // SVG_SUPPORT_STENCIL
}

void SVGIntersectionCanvas::Clean()
{
#ifdef SVG_SUPPORT_STENCIL
	m_active_clipsets.Clear();
	m_partial_clipsets.Clear();
#endif // SVG_SUPPORT_STENCIL
}

void SVGInvalidationCanvas::Clean()
{
#ifdef SVG_SUPPORT_STENCIL
	m_active_masks.Clear();
	m_partial_masks.Clear();

	m_active_clips.Clear();
	m_partial_clips.Clear();
#endif // SVG_SUPPORT_STENCIL
}

void SVGIntersectionCanvas::DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len,
											  INT32 extra_char_spacing, int* caret_x)
{
	OP_ASSERT(m_painter_active);
}

void SVGIntersectionCanvas::DrawSelectedStringPainter(const OpRect& selected,
													  COLORREF sel_bg_color, COLORREF sel_fg_color,
													  OpFont* font, const OpPoint& pos, uni_char* text,
													  UINT32 len, INT32 extra_char_spacing)
{
	OP_ASSERT(m_painter_active);
}

void SVGInvalidationCanvas::DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len,
											  INT32 extra_char_spacing, int* caret_x)
{
	OP_ASSERT(m_painter_active);
}

void SVGInvalidationCanvas::DrawSelectedStringPainter(const OpRect& selected,
													  COLORREF sel_bg_color, COLORREF sel_fg_color,
													  OpFont* font, const OpPoint& pos, uni_char* text,
													  UINT32 len, INT32 extra_char_spacing)
{
	OP_ASSERT(m_painter_active);
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGGeometryCanvas::AddClipRect(const SVGRect& cliprect)
{
	// FIXME: Avoid Save/RestoreState if possible
	RETURN_IF_ERROR(BeginClip());

	OP_STATUS status = SaveState();
	if (OpStatus::IsSuccess(status))
	{
		EnableFill(SVGCanvasState::USE_COLOR);
		SetFillOpacity(255);
		SetFillColorRGB(255,255,255);
		EnableStroke(SVGCanvasState::USE_NONE);

		status = DrawRect(cliprect.x, cliprect.y, cliprect.width, cliprect.height, 0, 0);

		RestoreState();
	}

	EndClip();

	return status;
}

OP_STATUS SVGInvalidationCanvas::BeginClip()
{
	OP_NEW_DBG("SVGInvalidationCanvas::BeginClip", "svg_clip");
	OP_DBG(("Starting new clip stencil (draw)"));
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* stencil = OP_NEW(Stencil, (m_clip_mode));
	if (!stencil)
		return OpStatus::ERR_NO_MEMORY;

	stencil->area.Empty();
	stencil->Into(&m_partial_clips);

	m_clip_mode = STENCIL_CONSTRUCT;
	return OpStatus::OK;
}

void SVGInvalidationCanvas::EndClip()
{
	OP_NEW_DBG("SVGInvalidationCanvas::EndClip", "svg_clip");
	OP_DBG(("Ending clip stencil (draw) [depth %d]", m_active_clips.Cardinal() + 1));
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* current_stencil = m_partial_clips.Last();
	if (!current_stencil)
		// If the stack was empty, there is really nothing to do -
		// we most likely have a bug somewhere which caused an
		// unbalanced pair of Begin/End
		return;

	current_stencil->Out();
	current_stencil->Into(&m_active_clips);

	m_clip_mode = current_stencil->prev_mode;
}

void SVGInvalidationCanvas::RemoveClip()
{
	OP_NEW_DBG("SVGInvalidationCanvas::RemoveClip", "svg_clip");
	OP_DBG(("Removing active clip stencil (draw) [%d left]", m_active_clips.Cardinal() - 1));
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* to_delete = m_active_clips.Last();
	if (to_delete)
		to_delete->Out();

	OP_DELETE(to_delete);
}

OP_STATUS SVGIntersectionCanvas::BeginClip()
{
	OP_NEW_DBG("SVGIntersectionCanvas::BeginClip", "svg_clip");
	OP_DBG(("Starting new clip stencil (intersect)"));

	SVGClipPathSet* clipset = OP_NEW(SVGClipPathSet, (m_clip_mode));
	if (!clipset)
		return OpStatus::ERR_NO_MEMORY;

	clipset->Into(&m_partial_clipsets);

	m_clip_mode = STENCIL_CONSTRUCT;

	return OpStatus::OK;
}

void SVGIntersectionCanvas::EndClip()
{
	OP_NEW_DBG("SVGIntersectionCanvas::EndClip", "svg_clip");
	OP_DBG(("Ending clip stencil (intersect)"));

	SVGClipPathSet* clipset = m_partial_clipsets.Last();
	if (!clipset)
		// Same applies here as for DRAW/INVALID_CALC above.
		return;

	clipset->Out();
	m_clip_mode = clipset->prev_mode;

	clipset->Into(&m_active_clipsets);
}

void SVGIntersectionCanvas::RemoveClip()
{
	OP_NEW_DBG("SVGIntersectionCanvas::RemoveClip", "svg_clip");
	OP_DBG(("Removing active clip stencil (intersect) [%d left]", m_active_clipsets.Cardinal() - 1));

	SVGClipPathSet* clipset = m_active_clipsets.Last();
	if (clipset)
		clipset->Out();

	OP_DELETE(clipset);
}

OP_STATUS SVGInvalidationCanvas::BeginMask()
{
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* stencil = OP_NEW(Stencil, (m_mask_mode));
	if (!stencil)
		return OpStatus::ERR_NO_MEMORY;

	stencil->area.Empty();
	stencil->Into(&m_partial_masks);

	m_mask_mode = STENCIL_CONSTRUCT;
	return OpStatus::OK;
}

void SVGInvalidationCanvas::EndMask()
{
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* current_stencil = m_partial_masks.Last();
	if (!current_stencil)
	{
		OP_ASSERT(!"Unbalanced Begin/EndMask");
		return;
	}

	current_stencil->Out();
	current_stencil->Into(&m_active_masks);

	m_mask_mode = current_stencil->prev_mode;
}

void SVGInvalidationCanvas::RemoveMask()
{
	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);

	Stencil* to_delete = m_active_masks.Last();
	if (to_delete)
		to_delete->Out();

	OP_DELETE(to_delete);
}
#endif // SVG_SUPPORT_STENCIL

void SVGGeometryCanvas::SetVegaTransform()
{
	GetTransform().CopyToVEGATransform(m_vtransform);
}

OP_STATUS
SVGGeometryCanvas::DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2)
{
	VEGAPath vpath, vspath;

	// We abort (don't draw line) if:
	// - we shouldn't use stroke and we are NOT in pointer events sensitive mode
	// - we shouldn't use stroke and we are in pointer events sensitive mode,
	//   and we disallow stroke as pointer-event target.
	if (!(UseStroke() || IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE)))
		return OpStatus::OK;

	RETURN_IF_ERROR(vpath.moveTo((x1).GetVegaNumber(), (y1).GetVegaNumber()));
	RETURN_IF_ERROR(vpath.lineTo((x2).GetVegaNumber(), (y2).GetVegaNumber()));

	SetVegaTransform();

	RETURN_IF_ERROR(CreateStrokePath(vpath, vspath));

	VPrimitive prim;
	prim.vpath = &vspath;
	prim.vprim = NULL;

	prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE);
	prim.geom_type = VPrimitive::GEOM_STROKE;

	return ProcessPrimitive(prim);
}

OP_STATUS
SVGGeometryCanvas::DrawPolygon(const SVGVector& list, BOOL closed)
{
	VEGAPath vpath;

	if (list.VectorType() != SVGOBJECT_POINT)
		return OpSVGStatus::BAD_PARAMETER;

	SVGPoint* c = static_cast<SVGPoint*>(list.Get(0));
	if(!c)
		return OpStatus::OK;

	unsigned int prep_count = list.GetCount();
	if (!closed)
		prep_count--;

	RETURN_IF_ERROR(vpath.prepare(prep_count));
	RETURN_IF_ERROR(vpath.moveTo((c->x).GetVegaNumber(),(c->y).GetVegaNumber()));

	for(UINT32 i = 1; i < list.GetCount(); i++)
	{
		c = static_cast<SVGPoint*>(list.Get(i));
		RETURN_IF_ERROR(vpath.lineTo((c->x).GetVegaNumber(),(c->y).GetVegaNumber()));
	}

	if (closed)
		RETURN_IF_ERROR(vpath.close(true));

	SetVegaTransform();

	return FillStrokePath(vpath, VEGA_INTTOFIX(-1));
}

OP_STATUS SVGGeometryCanvas::FillStrokePrimitive(VEGAPrimitive& vprimitive)
{
	BOOL filled = UseFill() || IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_FILL);

	VPrimitive filled_prim;
	if (filled)
	{
		filled_prim.vpath = NULL;
		filled_prim.vprim = &vprimitive;

		filled_prim.geom_type = VPrimitive::GEOM_FILL;
		filled_prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_FILL);
	}

	OP_STATUS status = OpStatus::OK;

	BOOL stroked = UseStroke() || IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE);

	VPrimitive stroked_prim;
	VEGAPath vspath;
	if (stroked)
	{
		VEGAPath path;

		status = path.appendPrimitive(&vprimitive);

		if (OpStatus::IsSuccess(status))
		{
			status = CreateStrokePath(path, vspath);

			stroked_prim.vpath = &vspath;
			stroked_prim.vprim = NULL;

			stroked_prim.geom_type = VPrimitive::GEOM_STROKE;
			stroked_prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE);
		}

		if (OpStatus::IsError(status))
			stroked = FALSE;
	}

	if (filled)
		OpStatus::Ignore(ProcessPrimitive(filled_prim));
	if (stroked)
		status = ProcessPrimitive(stroked_prim);

	return status;
}

OP_STATUS SVGGeometryCanvas::FillStrokePath(VEGAPath& vpath, VEGA_FIX pathLength)
{
	BOOL filled = UseFill() || IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_FILL);
	BOOL stroked = UseStroke() || IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE);

	OP_STATUS status = OpStatus::OK;
	VPrimitive filled_prim;
	if (filled)
	{
		// pathLength == 0 means no fill, but might mean that linecaps
		// should be applied
		if (pathLength != 0)
		{
			filled_prim.vpath = &vpath;
			filled_prim.vprim = NULL;

			filled_prim.geom_type = VPrimitive::GEOM_FILL;
			filled_prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_FILL);
		}
		else
		{
			filled = FALSE;
		}
	}

	VPrimitive stroked_prim;
	VEGAPath vspath;
	if (stroked)
	{
		status = CreateStrokePath(vpath, vspath, pathLength);

		stroked_prim.vpath = &vspath;
		stroked_prim.vprim = NULL;

		stroked_prim.geom_type = VPrimitive::GEOM_STROKE;
		stroked_prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE);
	}

	if (filled)
		OpStatus::Ignore(ProcessPrimitive(filled_prim));
	if (stroked && OpStatus::IsSuccess(status))
		status = ProcessPrimitive(stroked_prim);

	return status;
}

OP_STATUS SVGInvalidationCanvas::ProcessPrimitive(VPrimitive& prim)
{
	// Don't handle primitives here, so convert to path
	VEGAPath path;
	if (prim.vprim)
	{
		RETURN_IF_ERROR(path.appendPrimitive(prim.vprim));

		prim.vpath = &path;
		prim.vprim = NULL;
	}

	OP_ASSERT(m_render_mode == RENDERMODE_INVALID_CALC);
	OP_ASSERT(prim.vpath && !prim.vprim);

	prim.vpath->transform(&m_vtransform);

	// Calculate the screen extents for the primitive
	VEGA_FIX bx, by, bw, bh;
	prim.vpath->getBoundingBox(bx, by, bw, bh);

	if (bx == VEGA_INFINITY || by == VEGA_INFINITY ||
		bw == VEGA_INFINITY || bh == VEGA_INFINITY)
		return OpStatus::OK;

	int ix = VEGA_FIXTOINT(VEGA_FLOOR(bx));
	int iy = VEGA_FIXTOINT(VEGA_FLOOR(by));
	int iwidth = VEGA_FIXTOINT(VEGA_CEIL(bx+bw)) - ix;
	int iheight = VEGA_FIXTOINT(VEGA_CEIL(by+bh)) - iy;

	OpRect prim_extents(ix, iy, iwidth, iheight);

#ifdef SVG_SUPPORT_STENCIL
	if (m_clip_mode == STENCIL_CONSTRUCT || m_mask_mode == STENCIL_CONSTRUCT)
	{
		Stencil* stencil = NULL;
		if (m_clip_mode == STENCIL_CONSTRUCT)
		{
			OP_ASSERT(!m_partial_clips.Empty());
			stencil = m_partial_clips.Last();
		}
		else if (m_mask_mode == STENCIL_CONSTRUCT)
		{
			OP_ASSERT(!m_partial_masks.Empty());
			stencil = m_partial_masks.Last();
		}
		OP_ASSERT(stencil);

		// Calculate the screen area for the clip/mask
		ApplyClipToRegion(prim_extents);

		stencil->area.UnionWith(prim_extents);

		// We must not change the dirty rect when we're creating a
		// stencil/mask since then we might draw too much later.
		return OpStatus::OK;
	}
#endif // SVG_SUPPORT_STENCIL

	m_dirty_area.UnionWith(prim_extents);

	return OpStatus::OK;
}

OP_STATUS SVGIntersectionCanvas::ProcessPrimitive(VPrimitive& prim)
{
	if (!prim.do_intersection_test)
		return OpStatus::OK;

	// Only the RenderPrimitive code-path handles (VEGA) primitives as
	// primitives, so if we are passed a primitive, get its path
	// representation before continuing.
	VEGAPath path;
	if (prim.vprim)
	{
		RETURN_IF_ERROR(path.appendPrimitive(prim.vprim));

		prim.vpath = &path;
		prim.vprim = NULL;
	}

	OP_ASSERT(IsIntersectionMode());
	OP_ASSERT(prim.vpath && !prim.vprim);

#ifdef SVG_SUPPORT_STENCIL
	OP_ASSERT(m_mask_mode == STENCIL_USE); // No masking while intersection testing

	if (m_clip_mode == STENCIL_CONSTRUCT)
	{
		SVGClipPathSet* clipset = m_partial_clipsets.Last();
		if (!clipset)
			// There should be a clipset under construction
			return OpStatus::ERR;

		SVGClipPathInfo* pathinfo = OP_NEW(SVGClipPathInfo, (GetFillRule()));
		if (!pathinfo)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS result = pathinfo->path.copy(prim.vpath);
		if (OpStatus::IsError(result))
		{
			OP_DELETE(pathinfo);
			return result;
		}

		pathinfo->path.transform(&m_vtransform);
		pathinfo->Into(&clipset->paths);
		return OpStatus::OK;
	}

	if (!IntersectClip())
		return OpStatus::OK;
#endif // SVG_SUPPORT_STENCIL

	prim.vpath->transform(&m_vtransform);

	IntersectPath(prim.vpath, GetFillRule());

	return OpStatus::OK;
}

void SVGIntersectionCanvas::IntersectPath(VEGAPath* path, SVGFillRule fillrule)
{
	OP_ASSERT(IsIntersectionMode());

#ifdef SVG_SUPPORT_STENCIL
	// Shouldn't intersect anything during stencil construction
	OP_ASSERT(m_clip_mode == STENCIL_USE && m_mask_mode == STENCIL_USE);
#endif // SVG_SUPPORT_STENCIL

	bool fill_rule_param = (fillrule == SVGFILL_EVEN_ODD) ? true : false;

	switch (m_render_mode)
	{
	case RENDERMODE_INTERSECT_POINT:
		if (path->isPointInside((m_intersection_point.x).GetVegaNumber(),
								(m_intersection_point.y).GetVegaNumber(),
								fill_rule_param))
			SetLastIntersectedElement();
		break;

	case RENDERMODE_INTERSECT_RECT:
		if (path->intersects((m_intersection_rect.x).GetVegaNumber(),
							 (m_intersection_rect.y).GetVegaNumber(),
							 (m_intersection_rect.x+
							  m_intersection_rect.width).GetVegaNumber(),
							 (m_intersection_rect.y+
							  m_intersection_rect.height).GetVegaNumber(),
							 fill_rule_param))
			// Add element to selection
			SelectCurrentElement();
		break;

	case RENDERMODE_ENCLOSURE:
		if (path->isEnclosed((m_intersection_rect.x).GetVegaNumber(),
							 (m_intersection_rect.y).GetVegaNumber(),
							 (m_intersection_rect.x+
							  m_intersection_rect.width).GetVegaNumber(),
							 (m_intersection_rect.y+
							  m_intersection_rect.height).GetVegaNumber()))
			// Add element to selection
			SelectCurrentElement();
		break;

	default:
		OP_ASSERT(0);
		break;
	}
}

#define VNUM(s) ((s).GetVegaNumber())

OP_STATUS
SVGGeometryCanvas::DrawPath(const OpBpath *path, SVGNumber path_length)
{
	VEGAPath vpath;

	UINT32 cmdcount = path->GetCount(TRUE);

	// No work to be done
	if (cmdcount == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(vpath.prepare(cmdcount));

	SVGNumber flatness = 0.25;
	SVGNumber expansion = GetTransform().GetExpansionFactor();
	if (expansion > 0)
	{
		flatness = GetStrokeFlatness() / expansion;
	}

	PathSegListIterator* iter = path->GetPathIterator(TRUE); /* normalized */
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS cmd_status = OpStatus::OK;
	for (unsigned int i = 0; i < cmdcount && OpStatus::IsSuccess(cmd_status); i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();

		if (i == 0 && (cmd->info.type != SVGPathSeg::SVGP_MOVETO_ABS &&
					   cmd->info.type != SVGPathSeg::SVGP_CLOSE))
		{
			OP_DELETE(iter);
			return OpStatus::OK;
		}

		switch(cmd->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
			cmd_status = vpath.moveTo(VNUM(cmd->x), VNUM(cmd->y));
			break;
		case SVGPathSeg::SVGP_LINETO_ABS:
			cmd_status = vpath.lineTo(VNUM(cmd->x), VNUM(cmd->y));
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			cmd_status = vpath.cubicBezierTo(VNUM(cmd->x), VNUM(cmd->y),
											 VNUM(cmd->x1), VNUM(cmd->y1),
											 VNUM(cmd->x2), VNUM(cmd->y2),
											 VNUM(flatness));
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			cmd_status = vpath.quadraticBezierTo(VNUM(cmd->x), VNUM(cmd->y),
												 VNUM(cmd->x1), VNUM(cmd->y1),
												 VNUM(flatness));
			break;
		case SVGPathSeg::SVGP_ARC_ABS:
			cmd_status = vpath.arcTo(VNUM(cmd->x), VNUM(cmd->y),
									 VNUM(cmd->x1), VNUM(cmd->y1),
									 VNUM(cmd->x2),
									 cmd->info.large ? true : false,
									 cmd->info.sweep ? true : false,
									 VNUM(flatness));
			break;
		case SVGPathSeg::SVGP_CLOSE:
			cmd_status = vpath.close(true);
			break;
		}
	}
	OP_DELETE(iter);

	RETURN_IF_ERROR(cmd_status);

	SetVegaTransform();

	return FillStrokePath(vpath, path_length.GetVegaNumber());
}

OP_STATUS SVGGeometryCanvas::DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h,
								  SVGNumber rx, SVGNumber ry)
{
	VEGAPrimitive vprim;
	vprim.transform = NULL;
	VEGA_FIX corners[2];

	if (rx.Equal(SVGNumber(0)))
	{
		// Simple rect
		vprim.type = VEGAPrimitive::RECTANGLE;
		vprim.flatness = 0; // This primitive should not need a valid flatness

		vprim.data.rect.x = VNUM(x);
		vprim.data.rect.y = VNUM(y);
		vprim.data.rect.width = VNUM(w);
		vprim.data.rect.height = VNUM(h);
	}
	else
	{
		// Rounded rect
		SVGNumber flatness = 0.25;
		SVGNumber expansion = GetTransform().GetExpansionFactor();
		if (expansion > 0)
		{
			flatness = GetStrokeFlatness() / expansion;
		}

		vprim.type = VEGAPrimitive::ROUNDED_RECT_UNIFORM;
		vprim.flatness = VNUM(flatness);

		corners[0] = VNUM(rx);
		corners[1] = VNUM(ry);

		vprim.data.rrect.x = VNUM(x);
		vprim.data.rrect.y = VNUM(y);
		vprim.data.rrect.width = VNUM(w);
		vprim.data.rrect.height = VNUM(h);
		vprim.data.rrect.corners = corners;
	}

	SetVegaTransform();

	return FillStrokePrimitive(vprim);
}

OP_STATUS SVGGeometryCanvas::DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry)
{
	VEGAPath vpath;

	SVGNumber flatness = 0.25;
	SVGNumber expansion = GetTransform().GetExpansionFactor();
	if (expansion > 0)
	{
		flatness = GetStrokeFlatness() / expansion;
	}

	VEGA_FIX vf_flat = VNUM(flatness);

#ifdef SVG_SUPPORT_ELLIPTICAL_ARCS
	RETURN_IF_ERROR(vpath.prepare(1 + 2 + 1));

	RETURN_IF_ERROR(vpath.moveTo(VNUM(x+rx), VNUM(y)));
	// Divided in two arcs because when start and end is the same it's ignored
	RETURN_IF_ERROR(vpath.arcTo(VNUM(x-rx), VNUM(y), VNUM(rx), VNUM(ry),
								0, FALSE, TRUE, vf_flat));
	RETURN_IF_ERROR(vpath.arcTo(VNUM(x+rx), VNUM(y), VNUM(rx), VNUM(ry),
								0, FALSE, TRUE, vf_flat));
	RETURN_IF_ERROR(vpath.close(true));
#else
	RETURN_IF_ERROR(vpath.prepare(1 + 4 + 1));

	RETURN_IF_ERROR(vpath.moveTo(VNUM(x+rx), VNUM(y)));
	// Using same approx. as for rounded rects
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x), VNUM(y+ry),
										VNUM(x+rx), VNUM(y+ry*0.552285),
										VNUM(x+rx*0.552285), VNUM(y+ry),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x-rx), VNUM(y),
										VNUM(x-rx*0.552285), VNUM(y+ry),
										VNUM(x-rx), VNUM(y+ry*0.552285),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x), VNUM(y-ry),
										VNUM(x-rx), VNUM(y-ry*0.552285),
										VNUM(x-rx*0.552285), VNUM(y-ry),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x+rx), VNUM(y),
										VNUM(x+rx*0.552285), VNUM(y-ry),
										VNUM(x+rx), VNUM(y-ry*0.552285),
										vf_flat));
	RETURN_IF_ERROR(vpath.close(true));
#endif // SVG_SUPPORT_ELLIPTICAL_ARCS

	SetVegaTransform();

	return FillStrokePath(vpath, VEGA_INTTOFIX(-1));
}

#undef VNUM

#ifdef SVG_SUPPORT_STENCIL
BOOL SVGIntersectionCanvas::IntersectClip()
{
	BOOL clip_hit = TRUE;
	SVGClipPathSet* clipset = m_active_clipsets.First();
	while (clip_hit && clipset)
	{
		BOOL layer_hit = FALSE;
		SVGClipPathInfo* pathinfo = clipset->paths.First();
		while (!layer_hit && pathinfo)
		{
			VEGAPath& path = pathinfo->path;
			bool xor_fill = (pathinfo->clip_rule == SVGFILL_EVEN_ODD);

			switch (m_render_mode)
			{
			case RENDERMODE_INTERSECT_POINT:
				layer_hit = path.isPointInside((m_intersection_point.x).GetVegaNumber(),
											   (m_intersection_point.y).GetVegaNumber(),
											   xor_fill);
				break;
			case RENDERMODE_ENCLOSURE:
			case RENDERMODE_INTERSECT_RECT:
				layer_hit = path.intersects((m_intersection_rect.x).GetVegaNumber(),
											(m_intersection_rect.y).GetVegaNumber(),
											(m_intersection_rect.x+
											 m_intersection_rect.width).GetVegaNumber(),
											(m_intersection_rect.y+
											 m_intersection_rect.height).GetVegaNumber(),
											xor_fill);
				break;
			}

			pathinfo = pathinfo->Suc();
		}
		clip_hit = layer_hit;

		clipset = clipset->Suc();
	}
	return clip_hit;
}

void SVGInvalidationCanvas::ApplyClipToRegion(OpRect& region)
{
	if (Stencil* clip = m_active_clips.Last())
		region.IntersectWith(clip->area);

	if (Stencil* mask = m_active_masks.Last())
		region.IntersectWith(mask->area);
}
#endif // SVG_SUPPORT_STENCIL

void SVGIntersectionCanvas::SetIntersectionMode(const SVGNumberPair& intersection_point)
{
	m_render_mode = RENDERMODE_INTERSECT_POINT;
	m_intersection_point = intersection_point;
	m_intersection_point_i.Set(intersection_point.x.GetIntegerValue(),
							   intersection_point.y.GetIntegerValue());
}

void SVGIntersectionCanvas::SetIntersectionMode(const SVGRect& intersection_rect)
{
	m_render_mode = RENDERMODE_INTERSECT_RECT;
	m_intersection_rect = intersection_rect;
}

void SVGIntersectionCanvas::SetEnclosureMode(const SVGRect& enclosure_rect)
{
	m_render_mode = RENDERMODE_ENCLOSURE;
	m_intersection_rect = enclosure_rect;
}

OP_STATUS SVGGeometryCanvas::ProcessImage(const SVGRect& dest)
{
	VEGAPath rect;
	VEGA_FIX x = dest.x.GetVegaNumber();
	VEGA_FIX y = dest.y.GetVegaNumber();
	VEGA_FIX w = dest.width.GetVegaNumber();
	VEGA_FIX h = dest.height.GetVegaNumber();

	RETURN_IF_ERROR(rect.moveTo(x, y));
	RETURN_IF_ERROR(rect.lineTo(x+w, y));
	RETURN_IF_ERROR(rect.lineTo(x+w, y+h));
	RETURN_IF_ERROR(rect.lineTo(x, y+h));
	RETURN_IF_ERROR(rect.close(true));

	VPrimitive prim;
	prim.vpath = &rect;
	prim.vprim = NULL;

	prim.geom_type = VPrimitive::GEOM_IMAGE;
	prim.do_intersection_test = IsPointerEventsSensitive(SVGALLOWPOINTEREVENTS_STROKE |
														 SVGALLOWPOINTEREVENTS_FILL);

	return ProcessPrimitive(prim);
}

OP_STATUS
SVGGeometryCanvas::DrawImage(OpBitmap* bitmap, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality)
{
#ifdef SVG_SUPPORT_STENCIL
	if (m_clip_mode == STENCIL_CONSTRUCT)
		return OpStatus::OK;
#endif // SVG_SUPPORT_STENCIL

	SetVegaTransform();

	return ProcessImage(dest);
}

OP_STATUS
SVGGeometryCanvas::DrawImage(SVGSurface* bitmap_surface, const OpRect& in_source, const SVGRect& dest, ImageRenderQuality quality)
{
#ifdef SVG_SUPPORT_STENCIL
	if (m_clip_mode == STENCIL_CONSTRUCT)
		return OpStatus::OK;
#endif // SVG_SUPPORT_STENCIL

	SetVegaTransform();

	return ProcessImage(dest);
}

OP_STATUS SVGGeometryCanvas::CreateStrokePath(VEGAPath& vpath, VEGAPath& vstrokepath, VEGA_FIX precompPathLength)
{
	OP_NEW_DBG("SVGGeometryCanvas::CreateStrokePath", "svg_strokepath");
	VEGAPath* stroker = &vpath;

	stroker->setLineWidth(GetStrokeLineWidth().GetVegaNumber());
	stroker->setMiterLimit(GetStrokeMiterLimit().GetVegaNumber());

	VEGALineCap vega_cap_type;
	switch(GetStrokeCapType())
	{
	case SVGCAP_ROUND:
		vega_cap_type = VEGA_LINECAP_ROUND;
		break;
	case SVGCAP_SQUARE:
		vega_cap_type = VEGA_LINECAP_SQUARE;
		break;
	default:
		vega_cap_type = VEGA_LINECAP_BUTT;
		break;
	}
	stroker->setLineCap(vega_cap_type);

	VEGALineJoin vega_join_type;
	switch(GetStrokeJoinType())
	{
	case SVGJOIN_ROUND:
		vega_join_type = VEGA_LINEJOIN_ROUND;
		break;
	case SVGJOIN_MITER:
		vega_join_type = VEGA_LINEJOIN_MITER;
		break;
	default:
		vega_join_type = VEGA_LINEJOIN_BEVEL;
		break;
	}
	stroker->setLineJoin(vega_join_type);

	SVGNumber flatness = 0.25;
	SVGNumber expansion = GetTransform().GetExpansionFactor();
	if (expansion > 0)
	{
		flatness = GetStrokeFlatness() / expansion;
	}

	VEGAPath *shape;
	VEGAPath host_vpath;
	VEGATransform inverse_host_xfrm;
	if (GetVectorEffect() == SVGVECTOREFFECT_NON_SCALING_STROKE)
	{
		// Transform to the "host" coordinate space
		inverse_host_xfrm.copy(m_vtransform);
		if (inverse_host_xfrm.invert() && OpStatus::IsSuccess(host_vpath.copy(&vpath)))
		{
			host_vpath.transform(&m_vtransform);
			shape = &host_vpath;
		}
		else
		{
			inverse_host_xfrm.loadIdentity();
			shape = &vpath;
		}
	}
	else
	{
		shape = &vpath;
	}

	VEGAPath dash;
	const SVGResolvedDashArray* state_dasharray = GetStrokeDashArray();
	if (state_dasharray && state_dasharray->dash_size > 0 && state_dasharray->dashes)
	{
		VEGA_FIX *dasharray = OP_NEWA(VEGA_FIX, state_dasharray->dash_size);
		if (!dasharray)
			return OpStatus::ERR_NO_MEMORY;

		SVGNumber dashlength;
		const SVGNumber* dashes = state_dasharray->dashes;

		for (unsigned int dnum = 0; dnum < state_dasharray->dash_size; ++dnum)
		{
			dashlength += dashes[dnum];
			dasharray[dnum] = (dashes[dnum]).GetVegaNumber();

			// This is a workaround for the case where
			// a dasharray is setup with a zero-length dash
			if (!(dnum & 1) && dasharray[dnum] < VEGA_EPSILON)
				dasharray[dnum] = 100*VEGA_EPSILON;
		}

		if (dashlength*expansion > GetStrokeFlatness())
		{
			if (OpStatus::IsSuccess(shape->createDash(&dash, (GetStrokeDashOffset()).GetVegaNumber(),
													  state_dasharray->dash_size, dasharray,
													  precompPathLength))) // FIXME:OOM
				shape = &dash;
		}

		OP_DELETEA(dasharray);
	}

	shape->createOutline(&vstrokepath, (flatness).GetVegaNumber()); // FIXME:OOM

	if (GetVectorEffect() == SVGVECTOREFFECT_NON_SCALING_STROKE)
	{
		// Transform to the "host" coordinate space
		vstrokepath.transform(&inverse_host_xfrm);
	}

	OP_DBG(("numLines: %d. flatness: %g",
			vstrokepath.getNumLines(),
			flatness.GetFloatValue()));

	return OpStatus::OK;
}

OpRect SVGInvalidationCanvas::GetDirtyRect()
{
	OpRect dr(m_dirty_area);
#ifdef SVG_SUPPORT_STENCIL
	if (!m_dirty_area.IsEmpty())
		// Intersect with active stencils
		ApplyClipToRegion(dr);
#endif // SVG_SUPPORT_STENCIL
	return dr;
}

void SVGInvalidationCanvas::AddToDirtyRect(const OpRect& new_dirty_area)
{
#if 0
	Stencil* stencil = NULL;
	if (m_clip_mode == STENCIL_CONSTRUCT)
	{
		OP_ASSERT(!m_partial_clips.Empty());
		stencil = m_partial_clips.Last();
	}
	else if(m_mask_mode == STENCIL_CONSTRUCT)
	{
		OP_ASSERT(!m_partial_masks.Empty());
		stencil = m_partial_masks.Last();
	}

	if(stencil)
	{
		stencil->area.UnionWith(new_dirty_area);
	}
	else
#endif
	{
		m_dirty_area.UnionWith(new_dirty_area);
	}
}

void SVGInvalidationCanvas::ResetDirtyRect()
{
	m_dirty_area.Empty();
}

#endif // SVG_SUPPORT
