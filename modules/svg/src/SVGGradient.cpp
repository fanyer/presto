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
#include "modules/svg/src/SVGGradient.h"

#ifdef SVG_SUPPORT_GRADIENTS

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGCanvasState.h"

#include "modules/svg/src/svgpaintnode.h"

#include "modules/layout/layout_workplace.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/cascade.h"
#include "modules/layout/box/box.h"

#define VNUM(s) ((s).GetVegaNumber())

OP_STATUS SVGGradient::GetFill(VEGAFill** vfill, VEGATransform& vfilltrans,
							   SVGPainter* painter, SVGPaintNode* context_node)
{
	OP_ASSERT(painter && painter->GetRenderer());

	unsigned stop_count = GetNumStops();

	VEGA_FIX* ofs = OP_NEWA(VEGA_FIX, stop_count);
	UINT32* col = OP_NEWA(UINT32, stop_count);
	if (!ofs || !col)
	{
		OP_DELETEA(ofs);
		OP_DELETEA(col);
		return OpStatus::ERR_NO_MEMORY;
	}

	for (unsigned i = 0; i < stop_count; ++i)
	{
		const SVGStop* s = GetStop(i);
		if (!s)
		{
			OP_DELETEA(ofs);
			OP_DELETEA(col);
			return OpStatus::ERR;
		}

		UINT32 color = s->GetColorRGB();
		UINT8 a = GetSVGColorAValue(color);
		UINT8 r = GetSVGColorRValue(color);
		UINT8 g = GetSVGColorGValue(color);
		UINT8 b = GetSVGColorBValue(color);
		color = (a << 24) | (r << 16) | (g << 8) | b;

		ofs[i] = VNUM(s->GetOffset());
		col[i] = color;
	}

	VEGARenderer* renderer = painter->GetRenderer();
	OP_STATUS status = OpStatus::OK;
	if (Type() == SVGGradient::LINEAR)
	{
		VEGA_FIX f1x = VNUM(GetX1());
		VEGA_FIX f1y = VNUM(GetY1());
		VEGA_FIX f2x = VNUM(GetX2());
		VEGA_FIX f2y = VNUM(GetY2());

		status = renderer->createLinearGradient(vfill, f1x, f1y, f2x, f2y, stop_count, ofs, col);
	}
	else
	{
		VEGA_FIX fx = VNUM(GetFx());
		VEGA_FIX fy = VNUM(GetFy());
		VEGA_FIX cx = VNUM(GetCx());
		VEGA_FIX cy = VNUM(GetCy());
		VEGA_FIX r = VNUM(GetR());

		status = renderer->createRadialGradient(vfill, fx, fy, 0, cx, cy, r, stop_count, ofs, col);
	}

	OP_DELETEA(ofs);
	OP_DELETEA(col);

	RETURN_IF_ERROR(status);

	VEGAFill::Spread spr;
	switch (GetSpreadMethod())
	{
	case SVGSPREAD_REFLECT:
		spr = VEGAFill::SPREAD_REFLECT;
		break;

	case SVGSPREAD_REPEAT:
		spr = VEGAFill::SPREAD_REPEAT;
		break;

	case SVGSPREAD_PAD:
	default:
		spr = VEGAFill::SPREAD_CLAMP;
		break;
	}

	(*vfill)->setSpread(spr);

	// vfilltrans = GradientTransform * BBOX

	vfilltrans.loadIdentity();

	if (GetUnits() == SVGUNITS_OBJECTBBOX)
	{
		// Additional transform needed for the gradient transform
		SVGBoundingBox bbox = context_node->GetBBox();
		vfilltrans[0] = VNUM(bbox.maxx - bbox.minx);
		vfilltrans[2] = VNUM(bbox.minx);
		vfilltrans[4] = VNUM(bbox.maxy - bbox.miny);
		vfilltrans[5] = VNUM(bbox.miny);
	}

	// Gradient Transform
	VEGATransform tmp;
	m_transform.CopyToVEGATransform(tmp);
	vfilltrans.multiply(tmp);

	return OpStatus::OK;
}

#undef VNUM

void SVGGradient::PutFill(VEGAFill* vfill)
{
	OP_DELETE(vfill);
}

UINT32 SVGStop::GetColorRGB() const 
{
	unsigned stop_a = GetSVGColorAValue(m_stopcolor);
	stop_a = stop_a * m_stopopacity / 255;
	return (stop_a << 24) | (m_stopcolor & 0x00FFFFFF);
}

/**
 * Sets the boundingbox of the target to fill. Note that this may change as different objects are
 * rendered, and that the box doesn't always equal the actual boundingbox on the canvas level.
 * For example, text shouldn't paint the gradient using glyph bboxes, instead the entire text block
 * should be used.
 *
 * @param x The starting x coordinate (userspace)
 * @param y The starting y coordinate (userspace)
 * @param w The width (userspace)
 * @param h The height (userspace)
*/
void SVGGradient::SetTargetBoundingBox(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h) 
{
	m_bbox.x = x;
	m_bbox.y = y;
	m_bbox.width = w;
	m_bbox.height = h;
}

/**
 * Creates a copy of this gradient.
 *
 * @param outcopy The copy will be allocated and returned here, pass a reference to a pointer.
 * @return OK or ERR_NO_MEMORY
 */
OP_STATUS SVGGradient::CreateCopy(SVGGradient **outcopy) const
{
	SVGGradient *newgrad = OP_NEW(SVGGradient, (m_type));
	if (!newgrad)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i < m_stops.GetCount(); i++)
	{
		SVGStop* s = m_stops.Get(i);
		if (s)
		{
			SVGStop* newstop = OP_NEW(SVGStop, (*s));
			if (!newstop || OpStatus::IsError(newgrad->m_stops.Add(newstop)))
			{
				OP_DELETE(newstop);
				OP_DELETE(newgrad);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	newgrad->m_a = m_a;
	newgrad->m_b = m_b;
	newgrad->m_c = m_c;
	newgrad->m_d = m_d;
	newgrad->m_e = m_e;

	newgrad->m_transform.Copy(m_transform);
	newgrad->m_spread = m_spread;
	newgrad->m_units = m_units;
	newgrad->m_bbox = m_bbox;

	*outcopy = newgrad;
	return OpStatus::OK;
}

struct SVGGradientParameters
{
	SVGLengthObject* x1;
	SVGLengthObject* y1;
	SVGLengthObject* x2;
	SVGLengthObject* y2;

	SVGLengthObject* cx;
	SVGLengthObject* cy;
	SVGLengthObject* fx;
	SVGLengthObject* fy;
	SVGLengthObject* r;
};

void SVGGradient::ResolveGradientParameters(const SVGGradientParameters& params,
											const SVGValueContext& vcxt)
{
	if (m_type == SVGGradient::LINEAR)
	{
		SetX1(SVGUtils::ResolveLengthWithUnits(params.x1, SVGLength::SVGLENGTH_X,
											   GetUnits(), vcxt));
		SetY1(SVGUtils::ResolveLengthWithUnits(params.y1, SVGLength::SVGLENGTH_Y,
											   GetUnits(), vcxt));
		SetX2(SVGUtils::ResolveLengthWithUnits(params.x2, SVGLength::SVGLENGTH_X,
											   GetUnits(), vcxt));
		SetY2(SVGUtils::ResolveLengthWithUnits(params.y2, SVGLength::SVGLENGTH_Y,
											   GetUnits(), vcxt));
	}
	else
	{
		SetCx(SVGUtils::ResolveLengthWithUnits(params.cx, SVGLength::SVGLENGTH_X,
											   GetUnits(), vcxt));
		SetCy(SVGUtils::ResolveLengthWithUnits(params.cy, SVGLength::SVGLENGTH_Y,
											   GetUnits(), vcxt));
		SVGNumber v = SVGUtils::ResolveLengthWithUnits(params.r, SVGLength::SVGLENGTH_OTHER,
													   GetUnits(), vcxt);
		if (v < 0)
			v = 0.5; // Lacuna value

		SetR(v);

		// If (fx, fy) is not set, use cx/cy as default
		if (params.fx)
			SetFx(SVGUtils::ResolveLengthWithUnits(params.fx, SVGLength::SVGLENGTH_X,
												   GetUnits(), vcxt));
		else
			SetFx(GetCx());

		if (params.fy)
			SetFy(SVGUtils::ResolveLengthWithUnits(params.fy, SVGLength::SVGLENGTH_Y,
												   GetUnits(), vcxt));
		else
			SetFy(GetCy());

		// "If the point defined by fx and fy lies outside the circle
		// defined by cx, cy and r, then the user agent shall set the
		// focal point to the intersection of the line from (cx, cy) to
		// (fx, fy) with the circle defined by cx, cy and r."
		SVGNumber cfx = GetFx() - GetCx();
		SVGNumber cfy = GetFy() - GetCy();
		SVGNumber rad = GetR();
		SVGNumber sqr_dist = cfx*cfx + cfy*cfy;
		if (sqr_dist > rad*rad)
		{
			// Calculate intersection
			SVGNumber sf = rad / sqr_dist.sqrt() - SVGNumber::eps();
			SetFx(GetCx() + cfx * sf);
			SetFy(GetCy() + cfy * sf);
		}
	}
}

/* static */
OP_STATUS SVGGradient::Create(HTML_Element *gradient_element,
							  SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
							  const SVGValueContext& vcxt,
							  SVGGradient **outgrad)
{
	OP_ASSERT(outgrad);
	OP_ASSERT(gradient_element->IsMatchingType(Markup::SVGE_LINEARGRADIENT, NS_SVG) ||
			  gradient_element->IsMatchingType(Markup::SVGE_RADIALGRADIENT, NS_SVG));

	*outgrad = NULL;

	RETURN_IF_ERROR(resolver->FollowReference(gradient_element));

	GradientType type = SVGGradient::LINEAR;
	if (gradient_element->Type() == Markup::SVGE_RADIALGRADIENT)
		type = SVGGradient::RADIAL;

	OP_STATUS result;

	SVGGradient* gradient = OP_NEW(SVGGradient, (type));
	if (gradient)
	{
		SVGGradientParameters params;

		// This is the default for x1, y1 and y2
		SVGLengthObject def_lin(SVGNumber(0), CSS_PERCENTAGE);
		// This is the default for x2
		SVGLengthObject def_lin_x2(SVGNumber(100), CSS_PERCENTAGE);

		// Set unresolved defaults for linear parameters
		params.x1 = params.y1 = params.y2 = &def_lin;
		params.x2 = &def_lin_x2;

		// This is the default for cx, cy and r
		SVGLengthObject def_rad(SVGNumber(50), CSS_PERCENTAGE);

		// Set unresolved defaults for radial parameters
		params.cx = params.cy = params.r = &def_rad;
		params.fx = params.fy = NULL;

		// Setup defaults for attributes that needn't be resolved
		gradient->SetTransform(SVGMatrix());
		gradient->SetUnits(SVGUNITS_OBJECTBBOX);
		gradient->SetSpreadMethod(SVGSPREAD_PAD);

		HTML_Element* stop_root = NULL;

		result = gradient->FetchValues(gradient_element, resolver, doc_ctx, &params, &stop_root);

		if (OpStatus::IsSuccess(result))
		{
			gradient->ResolveGradientParameters(params, vcxt);

			if (stop_root)
				result = gradient->FetchGradientStops(stop_root);

			if (OpStatus::IsError(result))
			{
				OP_DELETE(gradient);
			}
			else
			{
				*outgrad = gradient;
			}
		}
		else
		{
			OP_DELETE(gradient);
		}
	}
	else
	{
		result = OpStatus::ERR_NO_MEMORY;
	}

	resolver->LeaveReference(gradient_element);
	return result;
}

OP_STATUS SVGGradient::FetchValues(HTML_Element *gradient_element,
								   SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
								   SVGGradientParameters* params, HTML_Element** stop_root)
{
	OP_ASSERT(gradient_element->IsMatchingType(Markup::SVGE_LINEARGRADIENT, NS_SVG) ||
			  gradient_element->IsMatchingType(Markup::SVGE_RADIALGRADIENT, NS_SVG));

	if (AttrValueStore::HasObject(gradient_element, Markup::XLINKA_HREF, NS_IDX_XLINK))
	{
		// This means we should inherit from the referenced element

		HTML_Element* inh_gradient_element = NULL;

		// Note: use the base url of the document where the gradient
		// element is (and note that it's always a document-local
		// reference)
		if (SVGDocumentContext* element_doc_ctx = AttrValueStore::GetSVGDocumentContext(gradient_element))
			inh_gradient_element = SVGUtils::FindHrefReferredNode(resolver, element_doc_ctx, gradient_element);

		if (inh_gradient_element)
		{
			Markup::Type inh_elm_type = inh_gradient_element->Type();
			if (inh_elm_type == Markup::SVGE_LINEARGRADIENT || inh_elm_type == Markup::SVGE_RADIALGRADIENT)
			{
				OP_STATUS status = resolver->FollowReference(inh_gradient_element);
				if (OpStatus::IsSuccess(status))
				{
					doc_ctx->RegisterDependency(gradient_element, inh_gradient_element);

					status = FetchValues(inh_gradient_element, resolver, doc_ctx, params, stop_root);

					resolver->LeaveReference(inh_gradient_element);

					RETURN_IF_ERROR(status);
				}
			}
		}
	}

	// Should we perhaps check m_type (set in Create), and not fetch if the type differs?
	Markup::Type elm_type = gradient_element->Type();
	if (elm_type == Markup::SVGE_RADIALGRADIENT)
	{
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_CX, &params->cx, params->cx);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_CY, &params->cy, params->cy);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_R, &params->r, params->r);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_FX, &params->fx, params->fx);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_FY, &params->fy, params->fy);
	}
	else /* elm_type == Markup::SVGE_LINEARGRADIENT */
	{
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_X1, &params->x1, params->x1);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_Y1, &params->y1, params->y1);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_X2, &params->x2, params->x2);
		AttrValueStore::GetLength(gradient_element, Markup::SVGA_Y2, &params->y2, params->y2);
	}

	// Check if we have gradientstop children, if not then inherit them
	for (HTML_Element* child = gradient_element->FirstChild(); child; child = child->Suc())
	{
		if (child->IsMatchingType(Markup::SVGE_STOP, NS_SVG))
		{
			// This element has stops - set it as the stop root
			*stop_root = gradient_element;
			break;
		}
	}

	if (AttrValueStore::HasTransform(gradient_element, Markup::SVGA_GRADIENTTRANSFORM, NS_IDX_SVG))
	{
		SVGMatrix matrix;
		AttrValueStore::GetMatrix(gradient_element, Markup::SVGA_GRADIENTTRANSFORM, matrix);
		SetTransform(matrix);
	}
	if (AttrValueStore::HasObject(gradient_element, Markup::SVGA_GRADIENTUNITS, NS_IDX_SVG))
	{
		SVGUnitsType units;
		OP_STATUS status = AttrValueStore::GetUnits(gradient_element, Markup::SVGA_GRADIENTUNITS, units, SVGUNITS_OBJECTBBOX);
		if (OpStatus::IsSuccess(status))
			SetUnits(units);
	}
	if (AttrValueStore::HasObject(gradient_element, Markup::SVGA_SPREADMETHOD, NS_IDX_SVG))
	{
		SVGSpreadMethodType spread = 
			(SVGSpreadMethodType)AttrValueStore::GetEnumValue(gradient_element, Markup::SVGA_SPREADMETHOD,
															  SVGENUM_SPREAD_METHOD_TYPE,
															  SVGSPREAD_PAD);
		SetSpreadMethod(spread);
	}
	return OpStatus::OK;
}

OP_STATUS SVGGradient::FetchGradientStops(HTML_Element* stop_root)
{
	// Create new cascade to ensure inheritance in document order

	SVGDocumentContext* element_doc_ctx = AttrValueStore::GetSVGDocumentContext(stop_root);
	if (!element_doc_ctx)
		return OpStatus::ERR;

	HLDocProfile* hld_profile = element_doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	Head prop_list;
	LayoutProperties* props = LayoutProperties::CreateCascade(stop_root, prop_list,
															  LAYOUT_WORKPLACE(hld_profile));
	if (!props)
		return OpStatus::ERR_NO_MEMORY;

	LayoutInfo info(hld_profile->GetLayoutWorkplace());

	OP_STATUS err = OpStatus::OK;

	// Figure out stops
	SVGStop* last = NULL;
	for (HTML_Element* child = stop_root->FirstChild(); child; child = child->Suc())
	{
		if (!child->IsMatchingType(Markup::SVGE_STOP, NS_SVG))
			continue;

		SVGStop* stop = NULL;
		err = CreateStop(child, props, info, &stop);
		if (OpStatus::IsError(err))
			break;

		err = AddStop(stop);
		if (OpStatus::IsError(err))
		{
			OP_DELETE(stop);
			break;
		}

		// Each gradient offset value is required to be equal to or
		// greater than the previous gradient stop's offset value. If
		// a given gradient stop's offset value is not equal to or
		// greater than all previous offset values, then the offset
		// value is adjusted to be equal to the largest of all
		// previous offset values.
		if (last && stop->GetOffset() < last->GetOffset())
			stop->SetOffset(last->GetOffset());

		last = stop;
	}

	// Remove cascade
	props = NULL;
	prop_list.Clear();

	return err;
}

/* static */
OP_STATUS SVGGradient::CreateStop(HTML_Element *stopelm, LayoutProperties* lprops, LayoutInfo& info,
								  SVGStop **outstop)
{
	OP_ASSERT(outstop);

	SVGStop* stop = OP_NEW(SVGStop, ());
	if (!stop)
		return OpStatus::ERR_NO_MEMORY;

	LayoutProperties* old_cascade = lprops;
#ifdef LAZY_LOADPROPS
	if (stopelm->IsPropsDirty())
	{
		OP_STATUS status = info.workplace->UnsafeLoadProperties(stopelm);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(stop);
			return status;
		}
	}
#endif // LAZY_LOADPROPS

	LayoutProperties* child_cascade = lprops->GetChildCascade(info, stopelm);

	if (!child_cascade)
	{
		OP_DELETE(stop);
		return OpStatus::ERR_NO_MEMORY;
	}

	const HTMLayoutProperties& props = *child_cascade->GetProps();
	const SvgProperties *svg_props = props.svg;

	SVGNumberObject* offset = NULL;
	AttrValueStore::GetNumberObject(stopelm, Markup::SVGA_OFFSET, &offset);
	if (offset)
	{
		stop->SetOffset(offset->value);

		// Gradient offset values less than 0 (or less than 0%) are rounded up to 0%.
		// Gradient offset values greater than 1 (or greater than 100%) are rounded down to 100%.
		if (stop->GetOffset() < 0)
		{
			stop->SetOffset(0);
		}
		else if (stop->GetOffset() > 1)
		{
			stop->SetOffset(1);
		}
	}
	// Otherwise use default value of zero

	const SVGColor& stop_color = svg_props->stopcolor;
	switch (stop_color.GetColorType())
	{
	case SVGColor::SVGCOLOR_RGBCOLOR:
		stop->SetColorRGB(stop_color.GetRGBColor());
		break;
	case SVGColor::SVGCOLOR_RGBCOLOR_ICCCOLOR:
		stop->SetColorRGB(stop_color.GetRGBColor());
		break;
	case SVGColor::SVGCOLOR_CURRENT_COLOR:
		OP_ASSERT(!"Hah! currentColor not resolved");
	default:
		break;
	}

	stop->SetOpacity(svg_props->stopopacity);

	// Reset CSS
	if (child_cascade)
		old_cascade->CleanSuc();

	*outstop = stop;
	return OpStatus::OK;
}

#ifdef SELFTEST
BOOL SVGGradient::operator==(const SVGGradient& other) const
{
	BOOL result = ((m_type == other.m_type) &&
			(m_stops.GetCount() == other.m_stops.GetCount()) &&
			(m_transform == other.m_transform) &&
			(m_spread == other.m_spread) &&
			(m_a.Equal(other.m_a)) &&
			(m_b.Equal(other.m_b)) &&
			(m_c.Equal(other.m_c)) &&
			(m_d.Equal(other.m_d)) &&
			(m_e.Equal(other.m_e)) &&
			(m_units == other.m_units) &&
		    (m_bbox.IsEqual(other.m_bbox)));

	if(result)
	{
		for(UINT32 i = 0; i < m_stops.GetCount(); i++)
		{
			SVGStop* s = m_stops.Get(i);
			SVGStop* os = other.m_stops.Get(i);

			if(!s || !os)
				return FALSE;

			result = result && (s->GetColorRGB() == os->GetColorRGB()) &&
				(s->GetOffset().Equal(os->GetOffset()));
			if(!result)
				break;
		}
	}

	return result;
}
#endif // SELFTEST

#ifdef _DEBUG
void SVGGradient::Print() const
{
	OP_NEW_DBG("SVGGradient::Print()", "svg_gradient");
	OP_DBG(("Type: %s. Units: %s.", 
		m_type == LINEAR ? "linear" : "radial", 
		m_units == SVGUNITS_USERSPACEONUSE ? "userSpaceOnUse" : "objBbox"));
	
	if(m_type == LINEAR)
	{
		OP_DBG(("(x1: %g y1: %g) (x2: %g y2: %g)", 
			GetX1().GetFloatValue(), GetY1().GetFloatValue(), 
			GetX2().GetFloatValue(), GetY2().GetFloatValue()));
	}
	else
	{
		OP_DBG(("(Cx: %g Cy: %g) (Fx: %g Fy: %g) R: %g.", 
			GetCx().GetFloatValue(), GetCy().GetFloatValue(), GetFx().GetFloatValue(), 
			GetFy().GetFloatValue(), GetR().GetFloatValue()));
	}
	OP_DBG(("Spread method: %s.", 
			m_spread == SVGSPREAD_PAD ? "pad" : m_spread == SVGSPREAD_REFLECT ? "reflect" : "repeat"));
	OP_DBG(("Transform: %g %g %g %g %g %g", 
			m_transform[0].GetFloatValue(), m_transform[1].GetFloatValue(),
			m_transform[2].GetFloatValue(), m_transform[3].GetFloatValue(), 
			m_transform[4].GetFloatValue(), m_transform[5].GetFloatValue()));
	
	for(UINT32 i = 0; i < m_stops.GetCount(); i++)
	{
		SVGStop* s = m_stops.Get(i);
		OP_DBG(("SVGStop #%d: stopcolor: 0x%x. offset: %g.", 
			i, s->GetColorRGB(), s->GetOffset().GetFloatValue()));
	}
}
#endif // _DEBUG

#endif // SVG_SUPPORT_GRADIENTS
#endif // SVG_SUPPORT
