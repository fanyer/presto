/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/svgpaintserver.h"
#include "modules/svg/src/svgpainter.h"

#include "modules/layout/layoutprops.h"

SVGCanvasState::SVGCanvasState() :
	m_fill_seq(0)
	,m_stroke_seq(0)
	,m_stroke_dasharray(NULL)
	,m_dasharray_seq(0)
	,m_vector_effect(SVGVECTOREFFECT_NONE)
#ifdef SVG_SUPPORT_PAINTSERVERS
	,m_fill_pserver(NULL)
	,m_stroke_pserver(NULL)
#endif // SVG_SUPPORT_PAINTSERVERS
	,m_has_decoration_paint(FALSE)
	,m_old_underline_color(USE_DEFAULT_COLOR)
	,m_old_overline_color(USE_DEFAULT_COLOR)
	,m_old_linethrough_color(USE_DEFAULT_COLOR)
	,m_shape_rendering(SVGSHAPERENDERING_AUTO)
	,m_fill_opacity(255)
	,m_stroke_opacity(255)
	,m_prev(NULL)
{
	m_current.element = NULL;
	m_last_intersected.element = NULL;
}

SVGCanvasState::~SVGCanvasState()
{
#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer::DecRef(m_fill_pserver);
	SVGPaintServer::DecRef(m_stroke_pserver);
#endif // SVG_SUPPORT_PAINTSERVERS
	SVGResolvedDashArray::DecRef(m_stroke_dasharray);
}

BOOL SVGCanvasState::DecorationsChanged(const HTMLayoutProperties& props)
{
	if (props.underline_color != m_old_underline_color ||
		props.overline_color != m_old_overline_color ||
		props.linethrough_color != m_old_linethrough_color)
	{
		m_old_underline_color = props.underline_color;
		m_old_overline_color = props.overline_color;
		m_old_linethrough_color = props.linethrough_color;
		return TRUE;
	}

	return props.svg->info.has_textdecoration ? TRUE : FALSE;
}

SVGCanvasState* SVGCanvasState::GetDecorationState()
{
	SVGCanvasState* s = this;
	while (s && s->m_has_decoration_paint == FALSE)
		s = s->m_prev;

	return s;
}

OP_STATUS SVGCanvasState::SetDecorationPaint()
{
	SVGCanvasState* decostate = GetDecorationState();
	if (decostate == this)
		return OpStatus::OK;

	if (decostate != NULL)
	{
		SetFillColor(decostate->m_fillcolor);
		switch (decostate->m_use_fill)
		{
#ifdef SVG_SUPPORT_PAINTSERVERS
		case USE_PSERVER:
			SVGPaintServer::IncRef(decostate->m_fill_pserver);
			SetFillPaintServer(decostate->m_fill_pserver);
			break;
#endif // SVG_SUPPORT_PAINTSERVERS
		}
		EnableFill(decostate->m_use_fill);

		SetStrokeColor(decostate->m_strokecolor);
		switch (decostate->m_use_stroke)
		{
#ifdef SVG_SUPPORT_PAINTSERVERS
		case USE_PSERVER:
			SVGPaintServer::IncRef(decostate->m_stroke_pserver);
			SetStrokePaintServer(decostate->m_stroke_pserver);
			break;
#endif // SVG_SUPPORT_PAINTSERVERS
		}
		EnableStroke(decostate->m_use_stroke);
	}
	else
	{
		// Use default paint
		EnableFill(SVGCanvasState::USE_COLOR);
		SetFillColorRGB(0,0,0);
		SetFillOpacity(0xff);

		EnableStroke(SVGCanvasState::USE_NONE);
	}
	return OpStatus::OK;
}

void SVGCanvasState::GetDecorationPaints(SVGPaintDesc* paints)
{
	SVGCanvasState* decostate = GetDecorationState();
	if (decostate)
	{
		paints[0].color = decostate->GetActualFillColor();
		paints[0].opacity = decostate->GetFillOpacity();
#ifdef SVG_SUPPORT_PAINTSERVERS
		SVGPaintServer::AssignRef(paints[0].pserver, decostate->GetFillPaintServer());
#endif // SVG_SUPPORT_PAINTSERVERS

		paints[1].color = decostate->GetActualStrokeColor();
		paints[1].opacity = decostate->GetStrokeOpacity();
#ifdef SVG_SUPPORT_PAINTSERVERS
		SVGPaintServer::AssignRef(paints[1].pserver, decostate->GetStrokePaintServer());
#endif // SVG_SUPPORT_PAINTSERVERS
	}
	else
	{
		// Use default paint
		paints[0].color = 0xff000000; // Black
		paints[0].opacity = 255;
#ifdef SVG_SUPPORT_PAINTSERVERS
		SVGPaintServer::AssignRef(paints[0].pserver, NULL);
#endif // SVG_SUPPORT_PAINTSERVERS

		paints[1].color = 0xff000000; // Black
		paints[1].opacity = 0;
#ifdef SVG_SUPPORT_PAINTSERVERS
		SVGPaintServer::AssignRef(paints[1].pserver, NULL);
#endif // SVG_SUPPORT_PAINTSERVERS
	}
}

// With ref-counted paint servers this is probably not needed any more
void SVGCanvasState::ResetDecorationPaint()
{
	SVGCanvasState* decostate = GetDecorationState();
	if (decostate == NULL || decostate == this)
		return;

	// To avoid deleting paints that were used previously
#ifdef SVG_SUPPORT_PAINTSERVERS
	if (m_use_fill == USE_PSERVER)
	{
		SVGPaintServer::DecRef(m_fill_pserver);
		m_fill_pserver = NULL;
	}
	if (m_use_stroke == USE_PSERVER)
	{
		SVGPaintServer::DecRef(m_stroke_pserver);
		m_stroke_pserver = NULL;
	}
#endif // SVG_SUPPORT_PAINTSERVERS
}

OP_STATUS SVGCanvasState::SetDashes(SVGNumber* dasharray, unsigned arraycount)
{
	SVGResolvedDashArray* new_dasharray = NULL;
	if (dasharray)
	{
		new_dasharray = OP_NEW(SVGResolvedDashArray, ());
		if (!new_dasharray)
		{
			OP_DELETEA(dasharray);
			return OpStatus::ERR_NO_MEMORY;
		}

		new_dasharray->dashes = dasharray;
		new_dasharray->dash_size = arraycount;
	}

	SVGResolvedDashArray::DecRef(m_stroke_dasharray);
	m_stroke_dasharray = new_dasharray;
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_PAINTSERVERS
void SVGCanvasState::SetFillPaintServer(SVGPaintServer* pserver)
{
	SVGPaintServer::DecRef(m_fill_pserver);
	m_fill_pserver = pserver;
}

void SVGCanvasState::SetStrokePaintServer(SVGPaintServer* pserver)
{
	SVGPaintServer::DecRef(m_stroke_pserver);
	m_stroke_pserver = pserver;
}
#endif // SVG_SUPPORT_PAINTSERVERS

BOOL SVGCanvasState::AllowPointerEvents(int type)
{
	if (m_pointer_events == SVGPOINTEREVENTS_NONE)
		return FALSE;

	if (m_pointer_events == SVGPOINTEREVENTS_ALL)
		return TRUE;

	if (m_pointer_events == SVGPOINTEREVENTS_VISIBLE &&
		m_visibility == SVGVISIBILITY_VISIBLE)
		return TRUE;

	if (type & SVGALLOWPOINTEREVENTS_FILL)
	{
		if (m_pointer_events == SVGPOINTEREVENTS_FILL)
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_VISIBLEFILL &&
			m_visibility == SVGVISIBILITY_VISIBLE)
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_PAINTED &&
			UseFill())
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_VISIBLEPAINTED &&
			m_visibility == SVGVISIBILITY_VISIBLE &&
			UseFill())
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_BOUNDINGBOX)
			return TRUE;
	}

	if (type & SVGALLOWPOINTEREVENTS_STROKE)
	{
		if (m_pointer_events == SVGPOINTEREVENTS_STROKE)
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_VISIBLESTROKE &&
			m_visibility == SVGVISIBILITY_VISIBLE)
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_PAINTED &&
			UseStroke())
			return TRUE;

		if (m_pointer_events == SVGPOINTEREVENTS_VISIBLEPAINTED &&
			m_visibility == SVGVISIBILITY_VISIBLE &&
			UseStroke())
			return TRUE;
	}

	/* No match */
	return FALSE;
}

BOOL
SVGCanvasState::AllowDraw(BOOL ignore_pointer_events)
{
	return GetVisibility() == SVGVISIBILITY_VISIBLE || (!ignore_pointer_events && AllowPointerEvents());
}

OP_STATUS SVGCanvasState::SaveState()
{
	SVGCanvasState* saved_state = OP_NEW(SVGCanvasState, ());
	if (saved_state == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Save values
	saved_state->m_transform.Copy(m_transform);
	saved_state->m_use_fill = m_use_fill;
	saved_state->m_fill_seq = m_fill_seq;
	saved_state->m_use_stroke = m_use_stroke;
	saved_state->m_stroke_seq = m_stroke_seq;
	saved_state->m_fillcolor = m_fillcolor;

#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer::IncRef(m_fill_pserver);
	saved_state->m_fill_pserver = m_fill_pserver;
	SVGPaintServer::IncRef(m_stroke_pserver);
	saved_state->m_stroke_pserver = m_stroke_pserver;
#endif // SVG_SUPPORT_PAINTSERVERS

	saved_state->m_strokecolor = m_strokecolor;
	saved_state->m_stroke_linewidth = m_stroke_linewidth;
	saved_state->m_stroke_flatness = m_stroke_flatness;
	saved_state->m_stroke_miter_limit = m_stroke_miter_limit;
	saved_state->m_stroke_captype = m_stroke_captype;
	saved_state->m_stroke_jointype = m_stroke_jointype;
	saved_state->m_stroke_dashoffset = m_stroke_dashoffset;
	SVGResolvedDashArray::IncRef(m_stroke_dasharray);
	saved_state->m_stroke_dasharray = m_stroke_dasharray;
	saved_state->m_dasharray_seq = m_dasharray_seq;

	saved_state->m_vector_effect = m_vector_effect;
	saved_state->m_fillrule = m_fillrule;
	saved_state->m_visibility = m_visibility;
	saved_state->m_pointer_events = m_pointer_events;
	saved_state->m_logfont = m_logfont;

	saved_state->m_has_decoration_paint = m_has_decoration_paint;
	m_has_decoration_paint = FALSE;

	saved_state->m_old_underline_color = m_old_underline_color;
	saved_state->m_old_overline_color = m_old_overline_color;
	saved_state->m_old_linethrough_color = m_old_linethrough_color;
	saved_state->m_shape_rendering = m_shape_rendering;
	saved_state->m_fill_opacity = m_fill_opacity;
	saved_state->m_stroke_opacity = m_stroke_opacity;

	saved_state->m_prev = m_prev;
	m_prev = saved_state;

	return OpStatus::OK;
}

OP_STATUS SVGCanvasState::Copy(SVGCanvasState* copy)
{
	m_transform.Copy(copy->m_transform);
	m_use_fill = copy->m_use_fill;
	m_use_stroke = copy->m_use_stroke;
	m_fillcolor = copy->m_fillcolor;

#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer::AssignRef(m_fill_pserver, copy->m_fill_pserver);
	SVGPaintServer::AssignRef(m_stroke_pserver, copy->m_stroke_pserver);
#endif // SVG_SUPPORT_PAINTSERVERS

	m_strokecolor = copy->m_strokecolor;
	m_stroke_linewidth = copy->m_stroke_linewidth;
	m_stroke_flatness = copy->m_stroke_flatness;
	m_stroke_miter_limit = copy->m_stroke_miter_limit;
	m_stroke_captype = copy->m_stroke_captype;
	m_stroke_jointype = copy->m_stroke_jointype;
	m_stroke_dashoffset = copy->m_stroke_dashoffset;
	SVGResolvedDashArray::IncRef(copy->m_stroke_dasharray);
	m_stroke_dasharray = copy->m_stroke_dasharray;
	m_vector_effect = copy->m_vector_effect;
	m_fillrule = copy->m_fillrule;
	m_visibility = copy->m_visibility;
	m_pointer_events = copy->m_pointer_events;
	m_logfont = copy->m_logfont;
	m_shape_rendering = copy->m_shape_rendering;
	m_fill_opacity = copy->m_fill_opacity;
	m_stroke_opacity = copy->m_stroke_opacity;

	return OpStatus::OK;
}

void SVGCanvasState::RestoreState()
{
	OP_ASSERT(m_prev); // We had an OOM condition in SaveState above or have unbalanced SaveState/RestoreState !
	if (m_prev == NULL)
		return;

	// Pick out the last state on the stack, free it and update current state.
	SVGCanvasState* old_state_stack = m_prev;

	// Restore values
	m_transform.Copy(m_prev->m_transform);
	m_use_fill = m_prev->m_use_fill;
	m_fill_seq = m_prev->m_fill_seq;
	m_use_stroke = m_prev->m_use_stroke;
	m_stroke_seq = m_prev->m_stroke_seq;
	m_fillcolor = m_prev->m_fillcolor;

#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer::AssignRef(m_fill_pserver, m_prev->m_fill_pserver);
	SVGPaintServer::AssignRef(m_stroke_pserver, m_prev->m_stroke_pserver);
#endif // SVG_SUPPORT_PAINTSERVERS

	m_strokecolor = m_prev->m_strokecolor;
	m_stroke_linewidth = m_prev->m_stroke_linewidth;
	m_stroke_flatness = m_prev->m_stroke_flatness;
	m_stroke_miter_limit = m_prev->m_stroke_miter_limit;
	m_stroke_captype = m_prev->m_stroke_captype;
	m_stroke_jointype = m_prev->m_stroke_jointype;
	m_stroke_dashoffset = m_prev->m_stroke_dashoffset;
	SVGResolvedDashArray::AssignRef(m_stroke_dasharray, m_prev->m_stroke_dasharray);
	m_stroke_dasharray = m_prev->m_stroke_dasharray;
	m_dasharray_seq = m_prev->m_dasharray_seq;

	m_vector_effect = m_prev->m_vector_effect;
	m_fillrule = m_prev->m_fillrule;
	m_visibility = m_prev->m_visibility;
	m_pointer_events = m_prev->m_pointer_events;

	m_logfont = m_prev->m_logfont;

	m_has_decoration_paint = m_prev->m_has_decoration_paint;

	m_old_underline_color = m_prev->m_old_underline_color;
	m_old_overline_color = m_prev->m_old_overline_color;
	m_old_linethrough_color = m_prev->m_old_linethrough_color;
	m_shape_rendering = m_prev->m_shape_rendering;
	m_fill_opacity = m_prev->m_fill_opacity;
	m_stroke_opacity = m_prev->m_stroke_opacity;

	m_prev = m_prev->m_prev;
	OP_DELETE(old_state_stack);
}

unsigned int SVGCanvasState::GetShapeRenderingVegaQuality()
{
	switch(m_shape_rendering)
	{
	case SVGSHAPERENDERING_CRISPEDGES:
	case SVGSHAPERENDERING_OPTIMIZESPEED:
		return 0;
	case SVGSHAPERENDERING_AUTO:
	default:
		return VEGA_DEFAULT_QUALITY;
	}
}

void SVGCanvasState::SetDefaults(int rendering_quality)
{
	SetFillRule(SVGFILL_NON_ZERO);
	UINT32 opaqueBlack = 0xFF000000;
	SetFillColor(opaqueBlack);
	SetFillOpacity(0xFF);
	SetFillSeq(0);

	SetLineCap(SVGCAP_BUTT);
	SetLineJoin(SVGJOIN_MITER);
	SetLineWidth(SVGNumber(1));
	SetMiterLimit(SVGNumber(4));

	SetFlatness(SVGNumber(rendering_quality) / 100);

	UINT32 none = 0x00000000;
	SetStrokeColor(none);
	SetStrokeOpacity(0xFF);
	SetStrokeSeq(0);
	SetVectorEffect(SVGVECTOREFFECT_NONE);

	SetDashArraySeq(0);
	SetDashes(NULL, 0);

	m_old_underline_color = m_old_overline_color = m_old_linethrough_color = USE_DEFAULT_COLOR;
	m_has_decoration_paint = FALSE;

#ifdef SVG_SUPPORT_PAINTSERVERS
	SetFillPaintServer(NULL);
	SetStrokePaintServer(NULL);
#endif // SVG_SUPPORT_PAINTSERVERS

	EnableFill(SVGCanvasState::USE_COLOR);
	EnableStroke(SVGCanvasState::USE_NONE);

	SetVisibility(SVGVISIBILITY_VISIBLE);
	SetPointerEvents(SVGPOINTEREVENTS_VISIBLEPAINTED);
	SetShapeRendering(SVGSHAPERENDERING_AUTO);
}

#endif // SVG_SUPPORT
