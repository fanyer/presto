/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGWorkplaceImpl.h"
#include "modules/svg/src/SVGAttributeParser.h"

#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationarguments.h"
#include "modules/svg/src/parser/svglengthparser.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dom/domenvironment.h"

#include "modules/layout/cascade.h"
#include "modules/layout/layoutprops.h"

OP_STATUS
SVGAnimationInterface::SetAnimationArguments(SVGDocumentContext *doc_ctx,
											 SVGAnimationArguments& animation_arguments,
											 SVGAnimationTarget *&animation_target)
{
	animation_arguments.animation_element_type = m_timed_element->Type();
	animation_arguments.attribute.is_special = FALSE;

	if (animation_arguments.animation_element_type != Markup::SVGE_ANIMATEMOTION)
	{
		// Markup::SVGA_ATTRIBUTENAME required on all animation elements
		// except animateMotion
		SVGString *string_obj = NULL;
		OP_STATUS status = AttrValueStore::GetString(m_timed_element, Markup::SVGA_ATTRIBUTENAME, &string_obj);
		if (OpStatus::IsSuccess(status) && string_obj != NULL)
		{
			const uni_char* string = string_obj->GetString();
			unsigned string_length = string_obj->GetLength();
			OP_STATUS parse_status = SVGAttributeParser::ParseAttributeName(m_timed_element, string, string_length,
																			animation_arguments.attribute.animated_name,
																			animation_arguments.attribute.ns_idx);
			if (OpStatus::IsMemoryError(parse_status))
				return OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsError(parse_status))
				return OpSVGStatus::INVALID_ANIMATION;

			if (animation_arguments.attribute.animated_name == ATTR_XML)
				return OpSVGStatus::INVALID_ANIMATION;

			animation_arguments.attribute.base_name = animation_arguments.attribute.animated_name;

			if (animation_arguments.attribute.base_name == Markup::SVGA_ANIMATED_MARKER_PROP)
				animation_arguments.attribute.base_name = Markup::HA_XML;

			SVGEnum *enum_object = NULL;
			AttrValueStore::GetEnumObject(m_timed_element, Markup::SVGA_ATTRIBUTETYPE, SVGENUM_ATTRIBUTE_TYPE, &enum_object);

			animation_arguments.attribute.type = (enum_object) ?
				(SVGAttributeType)enum_object->EnumValue() :
				SVGATTRIBUTE_AUTO;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else
	{
		animation_arguments.attribute.animated_name = Markup::SVGA_MOTION_TRANSFORM;
		animation_arguments.attribute.base_name = Markup::HA_XML;
		animation_arguments.attribute.ns_idx = NS_IDX_SVG;
		animation_arguments.attribute.type = SVGATTRIBUTE_XML;
	}

	animation_arguments.fill_mode = SVGANIMATEFILL_REMOVE;
	animation_arguments.calc_mode = SVGCALCMODE_LINEAR;
	if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATEMOTION)
		animation_arguments.calc_mode = SVGCALCMODE_PACED;
	animation_arguments.additive_type = SVGADDITIVE_REPLACE;
	animation_arguments.accumulate_type = SVGACCUMULATE_NONE;

	SVGAttributeIterator iter(m_timed_element);

	int attr;
	int ns_idx;
	BOOL is_special;
	SVGObject *svg_object;
	while (iter.GetNext(attr, ns_idx, is_special, svg_object))
	{
		if (!svg_object)
			/* The object can be NULL in case of parse errors. Ignore
			 * such attributes here */

			continue;

		switch (attr)
		{
		case Markup::SVGA_FROM:
			animation_arguments.SetFrom(svg_object);
			break;
		case Markup::SVGA_TO:
			animation_arguments.SetTo(svg_object);
			break;
		case Markup::SVGA_BY:
			animation_arguments.SetBy(svg_object);
			break;

		case Markup::SVGA_VALUES:
			if (svg_object->Type() == SVGOBJECT_VECTOR)
				animation_arguments.SetValues(static_cast<SVGVector *>(svg_object));
			break;

		case Markup::SVGA_FILL:
			if (svg_object->Type() == SVGOBJECT_ENUM)
			{
				OP_ASSERT(static_cast<SVGEnum *>(svg_object)->EnumType() == SVGENUM_ANIMATEFILLTYPE);
				animation_arguments.fill_mode = static_cast<SVGAnimateFillType>(static_cast<SVGEnum *>(svg_object)->EnumValue());
			}
			break;

		case Markup::SVGA_CALCMODE:
			if (svg_object->Type() == SVGOBJECT_ENUM)
			{
				OP_ASSERT(static_cast<SVGEnum *>(svg_object)->EnumType() == SVGENUM_CALCMODE);
				animation_arguments.calc_mode = static_cast<SVGCalcMode>(static_cast<SVGEnum *>(svg_object)->EnumValue());
			}
			break;

		case Markup::SVGA_KEYTIMES:
			if (svg_object->Type() == SVGOBJECT_VECTOR)
				animation_arguments.SetKeyTimes(static_cast<SVGVector *>(svg_object));
			break;

		case Markup::SVGA_KEYSPLINES:
			if (svg_object->Type() == SVGOBJECT_VECTOR)
				animation_arguments.SetKeySplines(static_cast<SVGVector *>(svg_object));
			break;

		case Markup::SVGA_ADDITIVE:
			if (svg_object->Type() == SVGOBJECT_ENUM)
			{
				OP_ASSERT(static_cast<SVGEnum *>(svg_object)->EnumType() == SVGENUM_ADDITIVE);
				animation_arguments.additive_type = static_cast<SVGAdditiveType>(static_cast<SVGEnum *>(svg_object)->EnumValue());
			}
			break;

		case Markup::SVGA_ACCUMULATE:
			if (svg_object->Type() == SVGOBJECT_ENUM)
			{
				OP_ASSERT(static_cast<SVGEnum *>(svg_object)->EnumType() == SVGENUM_ACCUMULATE);
				animation_arguments.accumulate_type = static_cast<SVGAccumulateType>(static_cast<SVGEnum *>(svg_object)->EnumValue());
			}
			break;
		}
	}

	HTML_Element* target_elm = GetDefaultTargetElement(doc_ctx);

	/* Check validity of target element */

	if (!target_elm || SVGUtils::IsAnimationElement(target_elm) ||
		target_elm->GetNsType() != NS_SVG ||
		!AttrValueStore::GetSVGDocumentContext(target_elm) /* external reference not in an svg fragment */)
	{
		if (doc_ctx->GetAnimationWorkplace()->HasTimelineStarted())
		{
			SVG_NEW_ERROR(m_timed_element);
			SVG_REPORT_ERROR_HREF((UNI_L("Unknown target element of animation.")));
		}

		return OpSVGStatus::INVALID_ANIMATION;
	}

	if (SVGElementContext *elm_ctx = AttrValueStore::AssertSVGElementContext(target_elm))
	{
		if ((animation_target = elm_ctx->AssertAnimationTarget(target_elm)) == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	if (animation_arguments.animation_element_type != Markup::SVGE_ANIMATEMOTION)
	{
		/* Check validity of target attribute */

		if (animation_arguments.attribute.type == SVGATTRIBUTE_CSS &&
			g_ns_manager->GetNsTypeAt(target_elm->ResolveNsIdx(animation_arguments.attribute.ns_idx)) == NS_SVG &&
			!SVGUtils::IsPresentationAttribute(animation_arguments.attribute.animated_name))
			return OpSVGStatus::INVALID_ANIMATION;

		if (!IsAnimatable(target_elm,
						  animation_arguments.attribute.animated_name,
						  animation_arguments.attribute.ns_idx))
			return OpSVGStatus::INVALID_ANIMATION;
	}

	/* Do element specific setup for the different animation element
	   types */

	if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATEMOTION)
		animation_arguments.ResetMotionPath();
	else if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATETRANSFORM)
	{
		animation_arguments.SetTransformType((SVGTransformType)AttrValueStore::GetEnumValue(m_timed_element, Markup::SVGA_TYPE,
																							SVGENUM_TRANSFORM_TYPE,
																							SVGTRANSFORM_TRANSLATE));

		if (animation_arguments.GetTransformType() < 0 ||
			animation_arguments.GetTransformType() == SVGTRANSFORM_MATRIX)
		{
			SVG_NEW_ERROR(m_timed_element);
			SVG_REPORT_ERROR((UNI_L("Illegal type attribute for animateTransform")));
			return OpSVGStatus::INVALID_ANIMATION;
		}

		if (animation_arguments.attribute.animated_name == Markup::HA_XML)
			animation_arguments.attribute.animated_name = Markup::SVGA_TRANSFORM;

		if (animation_arguments.attribute.animated_name == Markup::SVGA_TRANSFORM ||
			animation_arguments.attribute.animated_name == Markup::SVGA_ANIMATE_TRANSFORM ||
			animation_arguments.attribute.animated_name == Markup::SVGA_PATTERNTRANSFORM ||
			animation_arguments.attribute.animated_name == Markup::SVGA_GRADIENTTRANSFORM)
		{
			animation_arguments.attribute.base_name = animation_arguments.attribute.animated_name;
			animation_arguments.attribute.animated_name = Markup::SVGA_ANIMATE_TRANSFORM;
		}
		else
		{
			// attribute to animate must be a transform attribute
			SVG_NEW_ERROR(m_timed_element);
			SVG_REPORT_ERROR((UNI_L("Illegal target attribute for animateTransform: %s."),
							  SVG_Lex::GetAttrString(animation_arguments.attribute.animated_name)));
			return OpSVGStatus::INVALID_ANIMATION;
		}
	}
	else if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATECOLOR)
	{
		NS_Type ns = g_ns_manager->GetNsTypeAt(target_elm->ResolveNsIdx(animation_arguments.attribute.ns_idx));
		if (ns != NS_SVG)
			return OpSVGStatus::INVALID_ANIMATION;

		Markup::AttrType svg_attr_name = animation_arguments.attribute.animated_name;
		if(!(svg_attr_name == Markup::SVGA_FILL ||
			 svg_attr_name == Markup::SVGA_STROKE ||
			 svg_attr_name == Markup::SVGA_COLOR ||
			 svg_attr_name == Markup::SVGA_STOP_COLOR ||
			 svg_attr_name == Markup::SVGA_FLOOD_COLOR ||
			 svg_attr_name == Markup::SVGA_LIGHTING_COLOR ||
			 svg_attr_name == Markup::SVGA_SOLID_COLOR ||
			 svg_attr_name == Markup::SVGA_VIEWPORT_FILL))
		{
			return OpSVGStatus::INVALID_ANIMATION; // attribute to animate must be color or paint
		}
	}

	NS_Type ns = g_ns_manager->GetNsTypeAt(target_elm->ResolveNsIdx(animation_arguments.attribute.ns_idx));
	if(ns == NS_SVG && Markup::IsSpecialAttribute(animation_arguments.attribute.animated_name))
	{
		animation_arguments.attribute.is_special = TRUE;
	}

	return OpStatus::OK;
}

#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
// Special-case for Markup::SVGA_ANIMATE_TRANSFORM: A flag is set telling
// the traversal code, when it is calculating the total transformation
// matrix, to include the Markup::SVGA_TRANSFORM attribute or not.
void
SVGAnimationInterface::MarkTransformAsNonAdditive(HTML_Element *animated_element)
{
	animated_element->SetSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE,
									 ITEM_TYPE_NUM,
									 NUM_AS_ATTR_VAL(0),
									 FALSE,
									 SpecialNs::NS_SVG);
}
#endif // SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN

/* static */ BOOL
SVGAnimationInterface::IsAnimatable(HTML_Element* target_element, Markup::AttrType attr_name, int ns_idx)
{
	BOOL3 animatable = MAYBE;
	OP_ASSERT(target_element->GetNsType() == NS_SVG);
	Markup::Type target_element_type = target_element->Type();
	NS_Type ns = g_ns_manager->GetNsTypeAt(target_element->ResolveNsIdx(ns_idx));

	// xmlns [:prefix] not animatable.

	// Attributes common to all elements:

	// id not animatable. But we don't even treat attributes outside
	// the svg or xlink namespace.

	// xml:base not animatable. Not in xlink or svg namespace
	// either. Same with xml:lang and xml:space.

	// The glyphRef element and its attribute is missing from this
	// function and should be added here when it is supported.

	if (ns == NS_XLINK)
	{
		switch(attr_name)
		{
			case Markup::XLINKA_HREF:
				if (target_element_type == Markup::SVGE_MPATH || target_element_type == Markup::SVGE_ALTGLYPH || target_element_type == Markup::SVGE_DISCARD || target_element_type == Markup::SVGE_SCRIPT) // from Markup::SVGE_MPATH, Markup::SVGE_ALTGLYPH, Markup::SVGE_DISCARD: Animatable: no
					animatable = NO;
				else
					animatable = YES; // from Markup::SVGE_USE, Markup::SVGE_IMAGE, Markup::SVGE_COLOR_PROFILE, Markup::SVGE_LINEARGRADIENT, Markup::SVGE_RADIALGRADIENT, Markup::SVGE_PATTERN, Markup::SVGE_FILTER, Markup::SVGE_CURSOR, Markup::SVGE_TREF: Animatable: yes
				break;
			case Markup::XLINKA_TITLE:
			case Markup::XLINKA_SHOW:
			default:
				animatable = NO;
		}
	}
	else if (ns == NS_SVG)
	{
		switch(attr_name)
		{
#ifdef _DEBUG
			case Markup::SVGA_X: // from Markup::SVGE_SVG, Markup::SVGE_USE, Markup::SVGE_IMAGE, Markup::SVGE_RECT, Markup::SVGE_PATTERN, Markup::SVGE_MASK, Markup::SVGE_FILTER, filter primitives, Markup::SVGE_CURSOR, Markup::SVGE_FOREIGNOBJECT, Markup::SVGE_TEXT, Markup::SVGE_TSPAN, Markup::SVGE_ALTGLYPH: Animatable: yes
			case Markup::SVGA_Y: // from Markup::SVGE_SVG, Markup::SVGE_USE, Markup::SVGE_IMAGE, Markup::SVGE_RECT, Markup::SVGE_PATTERN, Markup::SVGE_MASK, Markup::SVGE_FILTER, filter primitives, Markup::SVGE_CURSOR, Markup::SVGE_FOREIGNOBJECT, Markup::SVGE_TEXT, Markup::SVGE_TSPAN, Markup::SVGE_ALTGLYPH: Animatable: yes
			case Markup::SVGA_WIDTH: // from Markup::SVGE_SVG, Markup::SVGE_USE, Markup::SVGE_IMAGE, Markup::SVGE_RECT, Markup::SVGE_PATTERN, Markup::SVGE_MASK, Markup::SVGE_FILTER, filter primitives: Animatable: yes
			case Markup::SVGA_HEIGHT: // from Markup::SVGE_SVG, Markup::SVGE_USE, Markup::SVGE_IMAGE, Markup::SVGE_RECT, Markup::SVGE_PATTERN, Markup::SVGE_MASK, Markup::SVGE_FILTER, filter primitives: Animatable: yes
			case Markup::SVGA_RX: // from Markup::SVGE_RECT, Markup::SVGE_ELLIPSE: Animatable: yes
			case Markup::SVGA_RY: // from Markup::SVGE_RECT, Markup::SVGE_ELLIPSE: Animatable: yes
			case Markup::SVGA_CX: // from Markup::SVGE_CIRCLE, Markup::SVGE_ELLIPSE, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_CY: // from Markup::SVGE_CIRCLE, Markup::SVGE_ELLIPSE, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_X1: // from Markup::SVGE_LINE, Markup::SVGE_LINEARGRADIENT: Animatable: yes
			case Markup::SVGA_Y1: // from Markup::SVGE_LINE, Markup::SVGE_LINEARGRADIENT: Animatable: yes
			case Markup::SVGA_X2: // from Markup::SVGE_LINE, Markup::SVGE_LINEARGRADIENT: Animatable: yes
			case Markup::SVGA_Y2: // from Markup::SVGE_LINE, Markup::SVGE_LINEARGRADIENT: Animatable: yes
			case Markup::SVGA_POINTS: // from Markup::SVGE_POLYLINE, Markup::SVGE_POLYGON: Animatable: yes
			case Markup::SVGA_R: // from Markup::SVGE_CIRCLE, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_CLASS: // from Markup::SVGE_*. Is this used in SVG?
			case Markup::SVGA_ANIMATE_TRANSFORM: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_TRANSFORM: // all animations are done to the Markup::SVGA_ANIMATE_TRANSFORM attribute instead
			case Markup::SVGA_VIEWBOX: // from Markup::SVGE_* (which can be a viewport): Animatable: yes
			case Markup::SVGA_PRESERVEASPECTRATIO: // from Markup::SVGE_* (which can be a viewport, plus image, marker, pattern and view): Animatable: yes
			case Markup::SVGA_PATHLENGTH: // from Markup::SVGE_PATH: Animatable: yes
			case Markup::SVGA_FILL: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_FILL_RULE: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_FILL_OPACITY: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_WIDTH: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_OPACITY: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_LINECAP: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_LINEJOIN: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_MITERLIMIT: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_DASHARRAY: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_STROKE_DASHOFFSET: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_DISPLAY: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_VISIBILITY: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_MARKERUNITS: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_REFX: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_REFY: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_MARKERWIDTH: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_MARKERHEIGHT: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_ORIENT: // from Markup::SVGE_MARKER: Animatable: yes
			case Markup::SVGA_MARKER_START: // from Markup::SVGE_PATH, Markup::SVGE_LINE, Markup::SVGE_POLYGON, Markup::SVGE_POLYLINE: Animatable: yes
			case Markup::SVGA_MARKER_END: // from Markup::SVGE_PATH, Markup::SVGE_LINE, Markup::SVGE_POLYGON, Markup::SVGE_POLYLINE: Animatable: yes
			case Markup::SVGA_MARKER_MID: // from Markup::SVGE_PATH, Markup::SVGE_LINE, Markup::SVGE_POLYGON, Markup::SVGE_POLYLINE: Animatable: yes
			case Markup::SVGA_ANIMATED_MARKER_PROP:
			case Markup::SVGA_COLOR_INTERPOLATION: // from Markup::SVGE_* that are container element, graphical elements and animateColor
			case Markup::SVGA_COLOR_INTERPOLATION_FILTERS: // from Markup::SVGE_* that are filter primitives: Animatable: yes
			case Markup::SVGA_COLOR_RENDERING: // from Markup::SVGE_* that are container element, graphical elements and animateColor
			case Markup::SVGA_SHAPE_RENDERING: // from Markup::SVGE_* that are shapes: Animatable: yes
			case Markup::SVGA_TEXT_RENDERING: // from Markup::SVGE_* that text elements: Animatable: yes
			case Markup::SVGA_IMAGE_RENDERING: // from Markup::SVGE_* that are images: Animatable: yes
			case Markup::SVGA_COLOR: // from Markup::SVGE_* where props fill, stroke, stop-color, flood-color, lightning-color apply: Animatable: yes
			case Markup::SVGA_COLOR_PROFILE: // from Markup::SVGE_* that are image elements: Animatable: yes
			case Markup::SVGA_GRADIENTUNITS: // from Markup::SVGE_LINEARGRADIENT, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_GRADIENTTRANSFORM: // from Markup::SVGE_LINEARGRADIENT, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_SPREADMETHOD: // from Markup::SVGE_LINEARGRADIENT, Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_FX: // from Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_FY: // from Markup::SVGE_RADIALGRADIENT: Animatable: yes
			case Markup::SVGA_OFFSET: // from Markup::SVGE_STOP, Markup::SVGE_FECOMPONENTTRANSFER: Animatable: yes
			case Markup::SVGA_STOP_COLOR: // from Markup::SVGE_STOP: Animatable: yes
			case Markup::SVGA_STOP_OPACITY: // from Markup::SVGE_STOP: Animatable: yes
			case Markup::SVGA_PATTERNUNITS: // from Markup::SVGE_PATTERN: Animatable: yes
			case Markup::SVGA_PATTERNCONTENTUNITS: // from Markup::SVGE_PATTERN: Animatable: yes
			case Markup::SVGA_PATTERNTRANSFORM: // from Markup::SVGE_PATTERN: Animatable: yes
			case Markup::SVGA_OVERFLOW: // from Markup::SVGE_* that are elements which establish a new viewport, 'pattern' elements and 'marker' elements: Animatable: yes
			case Markup::SVGA_CLIP: // from Markup::SVGE_* that are elements which establish a new viewport, 'pattern' elements and 'marker' elements: Animatable: yes
			case Markup::SVGA_CLIP_PATH: // from Markup::SVGE_* that are container elements and graphics elements: Animatable: yes
			case Markup::SVGA_CLIP_RULE: // from Markup::SVGE_* that are graphics elements within a 'clipPath' element
			case Markup::SVGA_MASKUNITS: // from Markup::SVGE_MASK: Animatable: yes
			case Markup::SVGA_MASKCONTENTUNITS: // from Markup::SVGE_MASK: Animatable: yes
			case Markup::SVGA_MASK: // from Markup::SVGE_* that are container elements and graphics elements: Animatable: yes
			case Markup::SVGA_OPACITY: // from Markup::SVGE_* that are container elements and graphics elements: Animatable: yes
			case Markup::SVGA_FILTERUNITS: // from Markup::SVGE_FILTER: Animatable: yes
			case Markup::SVGA_PRIMITIVEUNITS: // from Markup::SVGE_FILTER: Animatable: yes
			case Markup::SVGA_FILTERRES: // from Markup::SVGE_FILTER: Animatable: yes
			case Markup::SVGA_FILTER: // from Markup::SVGE_* that are container elements and graphics elements: Animatable: yes
			case Markup::SVGA_RESULT: // from Markup::SVGE_* that are filter primitives: Animatable: yes
			case Markup::SVGA_IN: // from Markup::SVGE_* that are filter primitives: Animatable: yes
			case Markup::SVGA_AZIMUTH: // from Markup::SVGE_FEDISTANTLIGHT: Animatable: yes
			case Markup::SVGA_ELEVATION: // from Markup::SVGE_FEDISTANTLIGHT: Animatable: yes
			case Markup::SVGA_Z: // from Markup::SVGE_FEPOINTLIGHT, Markup::SVGE_FESPOTLIGHT: Animatable: yes
			case Markup::SVGA_POINTSATX: // from Markup::SVGE_FESPOTLIGHT: Animatable: yes
			case Markup::SVGA_POINTSATY: // from Markup::SVGE_FESPOTLIGHT: Animatable: yes
			case Markup::SVGA_POINTSATZ: // from Markup::SVGE_FESPOTLIGHT: Animatable: yes
			case Markup::SVGA_SPECULAREXPONENT: // from Markup::SVGE_FESPOTLIGHT, Markup::SVGE_FESPECULARLIGHTING: Animatable: yes
			case Markup::SVGA_LIMITINGCONEANGLE: // from Markup::SVGE_FESPOTLIGHT: Animatable: yes
			case Markup::SVGA_LIGHTING_COLOR: // from Markup::SVGE_FEDIFFUSELIGHTING, Markup::SVGE_FESPECULARLIGHTING: Animatable: yes
			case Markup::SVGA_MODE: // from Markup::SVGE_FEBLEND: Animatable: yes
			case Markup::SVGA_IN2: // from Markup::SVGE_FEBLEND, Markup::SVGE_FECOMPOSITE, Markup::SVGE_FEDISPLACEMENTMAP: Animatable: yes
			case Markup::SVGA_VALUES: // from Markup::SVGE_FECOLORMATRIX: Animatable: yes
			case Markup::SVGA_TABLEVALUES: // from Markup::SVGE_FECOMPONENTTRANSFER: Animatable: yes
			case Markup::SVGA_INTERCEPT: // from Markup::SVGE_FECOMPONENTTRANSFER: Animatable: yes
			case Markup::SVGA_AMPLITUDE: // from Markup::SVGE_FECOMPONENTTRANSFER: Animatable: yes
			case Markup::SVGA_EXPONENT: // from Markup::SVGE_FECOMPONENTTRANSFER: Animatable: yes
			case Markup::SVGA_OPERATOR: // from Markup::SVGE_FECOMPOSITE, Markup::SVGE_FEMORPHOLOGY: Animatable: yes
			case Markup::SVGA_K1: // from Markup::SVGE_FECOMPOSITE: Animatable: yes
			case Markup::SVGA_K2: // from Markup::SVGE_FECOMPOSITE: Animatable: yes
			case Markup::SVGA_K3: // from Markup::SVGE_FECOMPOSITE: Animatable: yes
			case Markup::SVGA_K4: // from Markup::SVGE_FECOMPOSITE: Animatable: yes
			case Markup::SVGA_ORDER: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_KERNELMATRIX: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_DIVISOR: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_BIAS: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_TARGETX: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_TARGETY: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_EDGEMODE: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_KERNELUNITLENGTH: // from Markup::SVGE_FECONVOLVEMATRIX, Markup::SVGE_FEDIFFUSELIGHTING: Animatable: yes
			case Markup::SVGA_PRESERVEALPHA: // from Markup::SVGE_FECONVOLVEMATRIX: Animatable: yes
			case Markup::SVGA_SURFACESCALE: // from Markup::SVGE_FEDIFFUSELIGHTING, Markup::SVGE_FESPECULARLIGHTING: Animatable: yes
			case Markup::SVGA_DIFFUSECONSTANT: // from Markup::SVGE_FEDIFFUSELIGHTING: Animatable: yes
			case Markup::SVGA_SCALE: // from Markup::SVGE_FEDISPLACEMENTMAP: Animatable: yes
			case Markup::SVGA_XCHANNELSELECTOR: // from Markup::SVGE_FEDISPLACEMENTMAP: Animatable: yes
			case Markup::SVGA_YCHANNELSELECTOR: // from Markup::SVGE_FEDISPLACEMENTMAP: Animatable: yes
			case Markup::SVGA_FLOOD_COLOR: // from Markup::SVGE_FEFLOOD: Animatable: yes
			case Markup::SVGA_FLOOD_OPACITY: // from Markup::SVGE_FEFLOOD: Animatable: yes
			case Markup::SVGA_STDDEVIATION: // from Markup::SVGE_FEGAUSSIANBLUR: Animatable: yes
			case Markup::SVGA_RADIUS: // from Markup::SVGE_FEMORPHOLOGY: Animatable: yes
			case Markup::SVGA_DX: // from Markup::SVGE_FEOFFSET, Markup::SVGE_TEXT, Markup::SVGE_TSPAN, Markup::SVGE_ALTGLYPH: Animatable: yes
			case Markup::SVGA_DY: // from Markup::SVGE_FEOFFSET, Markup::SVGE_TEXT, Markup::SVGE_TSPAN, Markup::SVGE_ALTGLYPH: Animatable: yes
			case Markup::SVGA_SPECULARCONSTANT: // from Markup::SVGE_FESPECULARLIGHTING: Animatable: yes
			case Markup::SVGA_BASEFREQUENCY: // from Markup::SVGE_FETURBULENCE: Animatable: yes
			case Markup::SVGA_NUMOCTAVES: // from Markup::SVGE_FETURBULENCE: Animatable: yes
			case Markup::SVGA_SEED: // from Markup::SVGE_FETURBULENCE: Animatable: yes
			case Markup::SVGA_STITCHTILES: // from Markup::SVGE_FETURBULENCE: Animatable: yes
			case Markup::SVGA_POINTER_EVENTS: // from Markup::SVGE_* that are graphics elements
			case Markup::SVGA_CURSOR: // from Markup::SVGE_* that are container elements and graphics elements: Animatable: yes
			case Markup::SVGA_TARGET: // from Markup::SVGE_A: Animatable: yes
			case Markup::SVGA_ROTATE: // from Markup::SVGE_TEXT, Markup::SVGE_TSPAN, Markup::SVGE_ALTGLYPH: Animatable: yes
			case Markup::SVGA_TEXTLENGTH: // from Markup::SVGE_TEXT, Markup::SVGE_TSPAN: Animatable: yes
			case Markup::SVGA_LENGTHADJUST: // from Markup::SVGE_TEXT: Animatable: yes
			case Markup::SVGA_TEXT_ANCHOR: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_DOMINANT_BASELINE: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_ALIGNMENT_BASELINE: // from Markup::SVGE_* that are 	'tspan', 'tref', 'altGlyph', 'textPath' elements: Animatable: yes
			case Markup::SVGA_BASELINE_SHIFT: // from  Markup::SVGE_* that are 	'tspan', 'tref', 'altGlyph', 'textPath' elements: Animatable: yes
			case Markup::SVGA_STARTOFFSET: // from Markup::SVGE_TEXTPATH: Animatable: yes
			case Markup::SVGA_METHOD: // from Markup::SVGE_TEXTPATH: Animatable: yes
			case Markup::SVGA_SPACING: // from Markup::SVGE_TEXTPATH: Animatable: yes
			case Markup::SVGA_FONT_SIZE_ADJUST: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_FONT: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_KERNING: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_LETTER_SPACING: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_WORD_SPACING: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_TEXT_DECORATION: // from Markup::SVGE_* that are text content elements: Animatable: yes
			case Markup::SVGA_CLIPPATHUNITS: // from Markup::SVGE_CLIPPATH: Animatable: yes
			case Markup::SVGA_VECTOR_EFFECT: // from Markup::SVGE_* that are graphical elements: Animatable: yes
			case Markup::SVGA_NAV_PREV: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_NEXT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_UP: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_UP_LEFT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_UP_RIGHT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_DOWN: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_DOWN_LEFT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_DOWN_RIGHT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_LEFT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_NAV_RIGHT: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_FOCUSABLE: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_SOLID_COLOR: // from Markup::SVGE_SOLIDCOLOR: Animatable: yes
			case Markup::SVGA_SOLID_OPACITY: // from Markup::SVGE_SOLIDCOLOR: Animatable: yes
			case Markup::SVGA_VIEWPORT_FILL: // from Markup::SVGE_* that create viewports: Animatable: yes
			case Markup::SVGA_VIEWPORT_FILL_OPACITY: // from Markup::SVGE_* that create viewports: Animatable: yes
			case Markup::SVGA_AUDIO_LEVEL: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_EDITABLE: // from Markup::SVGE_TEXT and Markup::SVGE_TEXTAREA: Animatable: yes
			case Markup::SVGA_DISPLAY_ALIGN: // from Markup::SVGE_*: Animatable: yes
			case Markup::SVGA_BUFFERED_RENDERING: // from Markup::SVGE_*: Animatable: yes
				animatable = YES;
				break;
#endif // _DEBUG
			case Markup::SVGA_TYPE:
				if (target_element_type == Markup::SVGE_STYLE || target_element_type == Markup::SVGE_SCRIPT || target_element_type == Markup::SVGE_HANDLER)
					animatable = NO; // from Markup::SVGE_STYLE, Markup::SVGE_SCRIPT : Animatable: no
				else
					animatable = YES; // Markup::SVGE_FECOLORMATRIX, Markup::SVGE_FECOMPONENTTRANSFER, Markup::SVGE_FETURBULENCE: Animatable: yes
				break;
			case Markup::SVGA_D:
				if (target_element_type == Markup::SVGE_PATH)
					animatable = YES; // from Markup::SVGE_PATH: Animatable: yes
				else
					animatable = NO; // from Markup::SVGE_GLYPH: Animatable: yes
				break;
			case Markup::SVGA_SLOPE:
			case Markup::SVGA_FONT_SIZE:
			case Markup::SVGA_FONT_FAMILY:
			case Markup::SVGA_FONT_STYLE:
			case Markup::SVGA_FONT_VARIANT:
			case Markup::SVGA_FONT_WEIGHT:
			case Markup::SVGA_FONT_STRETCH:
				if (target_element_type == Markup::SVGE_FONT_FACE)
					animatable = NO; // from Markup::SVGE_FONT_FACE: Animatable: no
				else
					animatable = YES; // from Markup::SVGE_* that are text content elements: Animatable: yes
				break;

			//case Markup::SVGA_MEDIA: // from Markup::SVGE_STYLE: Animatable: no (not supported)
			//case Markup::SVGA_TITLE: // from Markup::SVGE_STYLE: Animatable: no (not supported)
			case Markup::SVGA_ONFOCUSIN: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONFOCUSOUT: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONACTIVATE: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONCLICK: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONMOUSEDOWN: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONMOUSEUP: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONMOUSEOVER: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONMOUSEMOVE: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONMOUSEOUT: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONUNLOAD: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONLOAD: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONABORT: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONERROR: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONRESIZE: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONSCROLL: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONZOOM: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONBEGIN: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONEND: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_ONREPEAT: // from Markup::SVGE_* (From spec: The event attributes are available on many SVG elements)
			case Markup::SVGA_VERSION: // from Markup::SVGE_SVG: Animatable: no
			case Markup::SVGA_BASEPROFILE: // from Markup::SVGE_SVG: Animatable: no
			case Markup::SVGA_STYLE: // Markup::SVGE_*: Animatable: no
			//case Markup::SVGA_CONTENTSTYLETYPE: // Markup::SVGE_SVG (not supported): Animatable: no
			case Markup::SVGA_CONTENTSCRIPTTYPE: // Markup::SVGE_SVG: Animatable: no
			//case Markup::SVGA_TITLE: // from Markup::SVGE_STYLE (not supported): Animatable: no
			//case Markup::SVGA_LOCAL: // from Markup::SVGE_COLOR_PROFILE: Animatable: no
			case Markup::SVGA_NAME: // from Markup::SVGE_COLOR_PROFILE: Animatable: no
			//case Markup::SVGA_RENDERING_INTENT: // from Markup::SVGE_COLOR_PROFILE: Animatable: no
			case Markup::SVGA_ENABLE_BACKGROUND: // from Markup::SVGE_* that are container elements: Animatable: no
			case Markup::SVGA_ZOOMANDPAN: // from Markup::SVGE_SVG: Animatable: no
			case Markup::SVGA_VIEWTARGET: // from Markup::SVGE_VIEW: Animatable: no
			case Markup::SVGA_UNICODE: // from Markup::SVGE_GLYPH: Animatable: no
			case Markup::SVGA_GLYPH_NAME: // from Markup::SVGE_GLYPH: Animatable: no

#ifdef TOUCH_EVENTS_SUPPORT
			case Markup::SVGA_ONTOUCHSTART:
			case Markup::SVGA_ONTOUCHMOVE:
			case Markup::SVGA_ONTOUCHEND:
			case Markup::SVGA_ONTOUCHCANCEL:
#endif // TOUCH_EVENTS_SUPPORT

			case Markup::SVGA_ORIENTATION: // from Markup::SVGE_GLYPH (not supported): Animatable: no
			case Markup::SVGA_ARABIC_FORM: // from Markup::SVGE_GLYPH: Animatable: no
			case Markup::SVGA_LANG: // from Markup::SVGE_GLYPH: Animatable: no
			case Markup::SVGA_HORIZ_ADV_X: // from Markup::SVGE_GLYPH, Markup::SVGE_FONT: Animatable: no
			//case Markup::SVGA_VERT_ORIGIN_X: // from Markup::SVGE_GLYPH, Markup::SVGE_FONT (not supported): Animatable: no
			//case Markup::SVGA_VERT_ORIGIN_Y: // from Markup::SVGE_GLYPH, Markup::SVGE_FONT (not supported): Animatable: no
			//case Markup::SVGA_VERT_ADV_Y: // from Markup::SVGE_GLYPH, Markup::SVGE_FONT (not supported): Animatable: no

			case Markup::SVGA_HORIZ_ORIGIN_X: // from Markup::SVGE_FONT: Animatable: no
			//case Markup::SVGA_HORIZ_ORIGIN_Y: // from Markup::SVGE_FONT (not supported): Animatable: no
			case Markup::SVGA_U1: // from Markup::SVGE_VKERN, Markup::SVGE_HKERN: Animatable: no
			case Markup::SVGA_U2: // from Markup::SVGE_VKERN, Markup::SVGE_HKERN: Animatable: no
			case Markup::SVGA_G1: // from Markup::SVGE_VKERN, Markup::SVGE_HKERN: Animatable: no
			case Markup::SVGA_G2: // from Markup::SVGE_VKERN, Markup::SVGE_HKERN: Animatable: no
			case Markup::SVGA_K: // from Markup::SVGE_VKERN, Markup::SVGE_HKERN: Animatable: no
			case Markup::SVGA_UNICODE_RANGE: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_UNITS_PER_EM: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_PANOSE_1: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_STEMV: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_STEMH: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_CAP_HEIGHT: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_X_HEIGHT: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_ACCENT_HEIGHT: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_ASCENT: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_DESCENT: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_WIDTHS: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_BBOX: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_IDEOGRAPHIC: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_ALPHABETIC: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_MATHEMATICAL: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_HANGING: // from Markup::SVGE_FONT_FACE: Animatable: no
			//case Markup::SVGA_V_IDEOGRAPHIC: // from Markup::SVGE_FONT_FACE (not supported): Animatable: no
			//case Markup::SVGA_V_ALPHABETIC: // from Markup::SVGE_FONT_FACE (not supported): Animatable: no
			//case Markup::SVGA_V_MATHEMATICAL: // from Markup::SVGE_FONT_FACE (not supported): Animatable: no
			//case Markup::SVGA_V_HANGING: // from Markup::SVGE_FONT_FACE (not supported): Animatable: no
			case Markup::SVGA_UNDERLINE_POSITION: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_UNDERLINE_THICKNESS: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_STRIKETHROUGH_POSITION: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_STRIKETHROUGH_THICKNESS: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_OVERLINE_POSITION: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_OVERLINE_THICKNESS: // from Markup::SVGE_FONT_FACE: Animatable: no
			case Markup::SVGA_WRITING_MODE: // from Markup::SVGE_* that are 'text' elements: Animatable: no
			case Markup::SVGA_GLYPH_ORIENTATION_VERTICAL: // from Markup::SVGE_* that are text content elements: Animatable: no
			case Markup::SVGA_GLYPH_ORIENTATION_HORIZONTAL: // from Markup::SVGE_* that are text content elements: Animatable: no
			case Markup::SVGA_DIRECTION: // from Markup::SVGE_* that are text content elements: Animatable: no
			case Markup::SVGA_UNICODE_BIDI: // from Markup::SVGE_* that are text content elements: Animatable: no
			//case Markup::SVGA_GLYPH_REF: // from Markup::SVGE_ALTGLYPH: (not supported): Animatable: yes
			//case Markup::SVGA_FORMAT: // from Markup::SVGE_ALTGLYPH: (not supported): Animatable: yes
			case Markup::SVGA_FOCUSHIGHLIGHT: // from Markup::SVGE_* (focusable): Animatable: no
			case Markup::SVGA_SNAPSHOTTIME: // from Markup::SVGE_SVG: Animatable: no
#ifdef DRAG_SUPPORT
			case Markup::SVGA_ONDRAG:
			case Markup::SVGA_ONDRAGOVER:
			case Markup::SVGA_ONDRAGENTER:
			case Markup::SVGA_ONDRAGLEAVE:
			case Markup::SVGA_ONDRAGSTART:
			case Markup::SVGA_ONDRAGEND:
			case Markup::SVGA_ONDROP:
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
			case Markup::SVGA_ONCOPY:
			case Markup::SVGA_ONCUT:
			case Markup::SVGA_ONPASTE:
#endif // USE_OP_CLIPBOARD
				animatable = NO;
				break;
		}
	}

	OP_ASSERT(animatable != MAYBE);
	return (animatable != NO);
}

#endif // SVG_SUPPORT
