/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationvalue.h"
#include "modules/svg/src/animation/svganimationvaluecontext.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"

#include "modules/pi/OpScreenInfo.h"

#include "modules/layout/cascade.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layout_workplace.h"

SVGAnimationValueContext::SVGAnimationValueContext() :
	element(NULL),
	location(NULL),
	parent_props(NULL),
	props(NULL),
	font_height(0.0f),
	ex_height(0.0f),
	root_font_height(0.0f),
	current_color(0x0),
 	viewport_width(0.0f),
 	viewport_height(0.0f),
 	containing_block_width(0),
 	containing_block_height(0),
 	percentage_of(0),
	inherit_rgb_color(0x0),
	resolved_props(0),
	resolved_viewport(0),
	resolved_percentage(0),
	resolved_paint_inheritance(0) {}

SVGAnimationValueContext::~SVGAnimationValueContext()
{
	prop_list.Clear();
}

void
SVGAnimationValueContext::ResolveProps()
{
	SVGDocumentContext *element_doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	OP_ASSERT(element_doc_ctx);

	TempPresentationValues tmp_pres_values(FALSE);

	// The cascade is done in the document of the element to animate,
	// which must not be the same document as of the animation
	// workplace. In the external-use use-case, the situation is
	// exactly like that.
	HLDocProfile* element_hld_profile = element_doc_ctx->GetHLDocProfile();
	LayoutProperties* layout_props = LayoutProperties::CreateCascade(element,
																	 prop_list,
																	 LAYOUT_WORKPLACE(element_hld_profile));

	if (layout_props)
	{
		const HTMLayoutProperties& parent_props = *layout_props->Pred()->GetProps();
		const SvgProperties *svg_props = parent_props.svg;

		font_height = (svg_props != NULL) ?
			svg_props->fontsize.GetFloatValue() : LayoutFixedToFloat(parent_props.decimal_font_size);

		if (element_hld_profile)
			root_font_height = LayoutFixedToFloat(element_hld_profile->GetLayoutWorkplace()->GetDocRootProperties().GetRootFontSize());

		ex_height = SVGUtils::GetExHeight(element_doc_ctx->GetVisualDevice(), SVGNumber(font_height), parent_props.font_number).GetFloatValue();

		this->parent_props = &parent_props;

		const HTMLayoutProperties& props = *layout_props->GetProps();

		current_color = props.font_color;
		this->props = &props;
	}

	layout_props = NULL;

	resolved_props = 1;
}

void
SVGAnimationValueContext::ResolveViewport()
{
	SVGDocumentContext *element_doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	OP_ASSERT(element_doc_ctx);

	SVGNumberPair viewport;
	// If this fails, we'll be on thin ice, so make do with what we have
	OpStatus::Ignore(SVGUtils::GetViewportForElement(element,
													 element_doc_ctx,
													 viewport, NULL, NULL));
	viewport_width = viewport.x.GetFloatValue();
	viewport_height = viewport.y.GetFloatValue();

	HTML_Element* svg_root = element_doc_ctx->GetSVGRoot();

	// FIXME: Viewport for things within <animation>
	if (Box *box = svg_root->GetLayoutBox())
	{
		BOOL is_positioned = box->IsPositionedBox();
		BOOL is_fixed = box->IsFixedPositionedBox();

		if (HTML_Element* containing_element = Box::GetContainingElement(svg_root, is_positioned, is_fixed))
		{
			if (Box *parent_box = containing_element->GetLayoutBox())
			{
				if (Content *content = parent_box->GetContent())
				{
					containing_block_width = content->GetWidth();
					containing_block_height = content->GetHeight();
				}
			}
		}
	}

	resolved_viewport = 1;
}

void
SVGAnimationValueContext::ResolvePercentageOf()
{
	if (!resolved_viewport)
		ResolveViewport();

	// Set default
	percentage_of = 0.0f;

	NS_Type ns = location->is_special ? NS_SPECIAL : g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(location->ns_idx));
	SVGLength::LengthOrientation orientation = SVGUtils::GetLengthOrientation(location->animated_name, ns);

	if (orientation == SVGLength::SVGLENGTH_X)
	{
		percentage_of = viewport_width;
	}
	else if (orientation == SVGLength::SVGLENGTH_Y)
	{
		percentage_of = viewport_height;
	}
	else
	{
		float cw = viewport_width / 100.0f;
		float ch = viewport_height / 100.0f;
		percentage_of = (float)op_sqrt(cw*cw + ch*ch) / 1.414214f;
	}

	// We override percentage_of on the outermost svg element to mean
	// the dimensions of the containing block (as computed by layout)
	if (element->IsMatchingType(Markup::SVGE_SVG, NS_SVG) &&
		element == SVGUtils::GetTopmostSVGRoot(element) &&
		ns == NS_SVG)
	{
		if (location->animated_name == Markup::SVGA_WIDTH)
		{
			percentage_of = (float)containing_block_width;
		}
		else if (location->animated_name == Markup::SVGA_HEIGHT)
		{
			percentage_of = (float)containing_block_height;
		}
	}

	resolved_percentage = 1;
}

void
SVGAnimationValueContext::ResolvePaintInheritance()
{
	AssertProps();

	NS_Type ns = location->is_special ? NS_SPECIAL : g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(location->ns_idx));

	// Support for inherit when animating fill and stroke.
	if (parent_props && ns == NS_SVG && parent_props->svg)
	{
		const SvgProperties *svg_props = parent_props->svg;

		if (location->animated_name == Markup::SVGA_STROKE)
		{
			const SVGPaint *parent_paint = svg_props->GetStroke();
			if (parent_paint && parent_paint->GetPaintType() == SVGPaint::RGBCOLOR)
			{
				inherit_rgb_color = parent_paint->GetRGBColor();
			}
		}
		else if (location->animated_name == Markup::SVGA_FILL)
		{
			const SVGPaint *parent_paint = svg_props->GetFill();
			if (parent_paint && parent_paint->GetPaintType() == SVGPaint::RGBCOLOR)
			{
				inherit_rgb_color = parent_paint->GetRGBColor();
			}
		}
	}

	resolved_paint_inheritance = 1;
}

/* static */ OP_STATUS
SVGAnimationValue::Interpolate(SVGAnimationValueContext &context,
							   SVG_ANIMATION_INTERVAL_POSITION interval_position,
							   const SVGAnimationValue &from_value,
							   const SVGAnimationValue &to_value,
							   ExtraOperation extra_operation,
							   SVGAnimationValue &animation_value)
{
    if (from_value.value_type != to_value.value_type &&
		from_value.value_type != VALUE_EMPTY)
    {
		return OpStatus::ERR_NOT_SUPPORTED;
    }

	switch(to_value.value_type)
	{
	case VALUE_NUMBER:
	case VALUE_PERCENTAGE:
    	{
			animation_value.value_type = to_value.value_type;
			float a = (from_value.value_type == VALUE_EMPTY) ? 0.0f : from_value.value.number;
			float b = to_value.value.number;
			if (extra_operation == EXTRA_OPERATION_TREAT_TO_AS_BY)
			{
				b += a;
			}

			animation_value.value.number = a + (b - a) * interval_position;
			return Transfer(animation_value);
		}
		break;
	case VALUE_COLOR:
    	{
			UINT32 from_color = (from_value.value_type == VALUE_EMPTY) ? 0x0 : from_value.value.color;
			animation_value.value.color = InterpolateColors(interval_position,
															from_color,
															to_value.value.color,
															extra_operation);
			return Transfer(animation_value);
		}
		break;
	case VALUE_EMPTY:
		{
			if (from_value.reference_type == REFERENCE_SVG_POINT &&
				to_value.reference_type == REFERENCE_SVG_POINT &&
				animation_value.reference_type == REFERENCE_SVG_POINT)
			{
				InterpolateSVGPoints(interval_position,
									 from_value.reference.svg_point,
									 to_value.reference.svg_point,
									 extra_operation,
									 animation_value.reference.svg_point);
				return OpStatus::OK;
			}
			else if (from_value.reference_type == REFERENCE_SVG_RECT &&
					 to_value.reference_type == REFERENCE_SVG_RECT &&
					 animation_value.reference_type == REFERENCE_SVG_RECT)
			{
				InterpolateSVGRects(interval_position,
									from_value.reference.svg_rect,
									to_value.reference.svg_rect,
									extra_operation,
									animation_value.reference.svg_rect);
				return OpStatus::OK;
			}
			else if (from_value.reference_type == REFERENCE_SVG_VECTOR &&
					 to_value.reference_type == REFERENCE_SVG_VECTOR &&
					 animation_value.reference_type == REFERENCE_SVG_VECTOR)
			{
				return InterpolateSVGVectors(context, interval_position,
											 from_value.reference.svg_vector,
											 to_value.reference.svg_vector,
											 extra_operation,
											 animation_value.reference.svg_vector);
			}
			else if (from_value.reference_type == REFERENCE_SVG_OPBPATH &&
					 to_value.reference_type == REFERENCE_SVG_OPBPATH &&
					 animation_value.reference_type == REFERENCE_SVG_OPBPATH)
			{
				return InterpolateSVGOpBpaths(interval_position,
											  from_value.reference.svg_path,
											  to_value.reference.svg_path,
											  extra_operation,
											  animation_value.reference.svg_path);
			}
			else if (from_value.reference_type == REFERENCE_SVG_TRANSFORM &&
					 to_value.reference_type == REFERENCE_SVG_TRANSFORM &&
					 animation_value.reference_type == REFERENCE_SVG_TRANSFORM)
			{
				return InterpolateSVGTransforms(interval_position,
												*from_value.reference.svg_transform,
												*to_value.reference.svg_transform,
												extra_operation,
												*animation_value.reference.svg_transform);
			}
			else if (from_value.reference_type == REFERENCE_SVG_VECTOR &&
					 to_value.reference_type == REFERENCE_SVG_TRANSFORM &&
					 animation_value.reference_type == REFERENCE_SVG_TRANSFORM)
			{
				SVGTransform from_transform;
				from_value.reference.svg_vector->GetTransform(from_transform);
				return InterpolateSVGTransforms(interval_position,
												from_transform,
												*to_value.reference.svg_transform,
												extra_operation,
												*animation_value.reference.svg_transform);
			}
			else if (to_value.reference_type == REFERENCE_SVG_TRANSFORM &&
					 animation_value.reference_type == REFERENCE_SVG_TRANSFORM)
			{
				SVGTransform from_transform;
				return InterpolateSVGTransforms(interval_position,
												from_transform,
												*to_value.reference.svg_transform,
												extra_operation,
												*animation_value.reference.svg_transform);
			}
		}
	}

    return OpStatus::ERR_NOT_SUPPORTED;
}

/* static */ OP_STATUS
SVGAnimationValue::AddBasevalue(const SVGAnimationValue &base_value,
								SVGAnimationValue &animation_value)
{
    if ((base_value.value_type == VALUE_NUMBER &&
		 animation_value.value_type == VALUE_NUMBER) ||
		(base_value.value_type == VALUE_PERCENTAGE &&
		 animation_value.value_type == VALUE_PERCENTAGE))
    {
		animation_value.value.number += base_value.value.number;
		RETURN_IF_ERROR(Transfer(animation_value));
    }
    else if (base_value.value_type == VALUE_COLOR && animation_value.value_type == VALUE_COLOR)
    {
		animation_value.value.color = AddBasevalueColor(base_value.value.color,
														animation_value.value.color);
		RETURN_IF_ERROR(Transfer(animation_value));
    }
	else if (base_value.reference_type == REFERENCE_SVG_TRANSFORM &&
			 animation_value.reference_type == REFERENCE_SVG_TRANSFORM)
	{
		SVGMatrix product_matrix;
		animation_value.reference.svg_transform->GetMatrix(product_matrix);
		SVGMatrix factor_matrix;
		base_value.reference.svg_transform->GetMatrix(factor_matrix);
		product_matrix.Multiply(factor_matrix);
		animation_value.reference.svg_transform->SetMatrix(product_matrix);
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGAnimationValue::AddAccumulationValue(const SVGAnimationValue &accumulation_value,
										const SVGAnimationValue &accumulation_value_base,
										SVG_ANIMATION_INTERVAL_REPETITION repetition,
										SVGAnimationValue &animation_value)
{
    if (accumulation_value.value_type != animation_value.value_type)
	{
		return OpStatus::OK;
	}

	if (accumulation_value_base.value_type != VALUE_EMPTY &&
		accumulation_value_base.value_type != accumulation_value.value_type)
	{
		return OpStatus::OK;
	}

    if (accumulation_value.value_type == VALUE_NUMBER ||
		accumulation_value.value_type == VALUE_PERCENTAGE)
    {
		float accumulate_number = accumulation_value.value.number;
		if (accumulation_value_base.value_type == accumulation_value.value_type)
		{
			accumulate_number += accumulation_value_base.value.number;
		}

		animation_value.value.number += accumulate_number * repetition;
		RETURN_IF_ERROR(Transfer(animation_value));
    }
    else if (accumulation_value.value_type == VALUE_COLOR)
    {
		UINT32 base_color = 0x0;
		if (accumulation_value_base.value_type == VALUE_COLOR)
			base_color = accumulation_value_base.value.color;

		animation_value.value.color = AddAccumulationValueColor(accumulation_value.value.color,
																base_color,
																repetition,
																animation_value.value.color);
		RETURN_IF_ERROR(Transfer(animation_value));
    }
	else if (accumulation_value.value_type == VALUE_EMPTY)
	{
		if (accumulation_value.reference_type == REFERENCE_SVG_TRANSFORM &&
			animation_value.reference_type == REFERENCE_SVG_TRANSFORM)
		{
			accumulation_value.reference.svg_transform->MakeDefaultsExplicit();

			if (accumulation_value_base.reference_type == REFERENCE_SVG_TRANSFORM)
			{
				accumulation_value_base.reference.svg_transform->MakeDefaultsExplicit();
				accumulation_value.reference.svg_transform->AddTransform(*accumulation_value_base.reference.svg_transform);
			}

			SVGTransform original_accumulation_transform;
			original_accumulation_transform.Copy(*accumulation_value.reference.svg_transform);

			SVGTransform &accumulation_transform = *animation_value.reference.svg_transform;

			accumulation_transform.MakeDefaultsExplicit();
			accumulation_transform.AddTransform(original_accumulation_transform);
			for (SVG_ANIMATION_INTERVAL_REPETITION i=1;i<repetition;i++)
				accumulation_transform.AddTransform(original_accumulation_transform);
		}
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGAnimationValue::Assign(SVGAnimationValueContext &context,
						  const SVGAnimationValue &rvalue, SVGAnimationValue &lvalue)
{
    if (rvalue.reference_type != lvalue.reference_type)
    {
		return OpStatus::ERR;
    }

    switch(lvalue.reference_type)
    {
    case REFERENCE_SVG_FONT_SIZE:
	{
	    *lvalue.reference.svg_font_size = *rvalue.reference.svg_font_size;
	}
	break;
    case REFERENCE_SVG_BASELINE_SHIFT:
	{
	    AssignSVGBaselineShifts(*rvalue.reference.svg_baseline_shift,
								*lvalue.reference.svg_baseline_shift);
	}
	break;
    case REFERENCE_SVG_LENGTH:
	{
	    *lvalue.reference.svg_length = *rvalue.reference.svg_length;
	}
	break;
    case REFERENCE_SVG_PAINT:
	{
	    RETURN_IF_MEMORY_ERROR(AssignSVGPaints(*rvalue.reference.svg_paint,
											   *lvalue.reference.svg_paint));
	}
	break;
    case REFERENCE_SVG_COLOR:
	{
	    AssignSVGColors(*rvalue.reference.svg_color, *lvalue.reference.svg_color);
	}
	break;
    case REFERENCE_SVG_RECT:
	{
		lvalue.reference.svg_rect->Set(*rvalue.reference.svg_rect);
	}
	break;
    case REFERENCE_SVG_VECTOR:
	{
		lvalue.reference.svg_vector->Copy(context, *rvalue.reference.svg_vector);
	}
	break;
    case REFERENCE_SVG_OPBPATH:
	{
		RETURN_IF_MEMORY_ERROR(lvalue.reference.svg_path->Copy(*rvalue.reference.svg_path));
	}
	break;
    case REFERENCE_SVG_POINT:
	{
		lvalue.reference.svg_point->x = rvalue.reference.svg_point->x;
		lvalue.reference.svg_point->y = rvalue.reference.svg_point->y;
	}
	break;
	case REFERENCE_SVG_ENUM:
	{
		lvalue.reference.svg_enum->Copy(*rvalue.reference.svg_enum);
	}
	break;
	case REFERENCE_SVG_NUMBER:
	{
		*lvalue.reference.svg_number = *rvalue.reference.svg_number;
	}
	break;
	case REFERENCE_SVG_TRANSFORM:
	{
		lvalue.reference.svg_transform->Copy(*rvalue.reference.svg_transform);
	}
	break;
	case REFERENCE_SVG_STRING:
	{
		SVGString *string_object = rvalue.reference.svg_string;
		OP_ASSERT(string_object->GetString() != NULL);
		RETURN_IF_ERROR(lvalue.reference.svg_string->SetString(string_object->GetString(),
															   string_object->GetLength()));
	}
	break;
	case REFERENCE_SVG_URL:
	{
		RETURN_IF_ERROR(lvalue.reference.svg_url->Copy(*rvalue.reference.svg_url));
	}
	break;
	case REFERENCE_SVG_ASPECT_RATIO:
	{
		lvalue.reference.svg_aspect_ratio->Copy(*rvalue.reference.svg_aspect_ratio);
	}
	break;
	case REFERENCE_SVG_NAVREF:
	{
		lvalue.reference.svg_nav_ref->Copy(*rvalue.reference.svg_nav_ref);
	}
	break;
	case REFERENCE_SVG_ORIENT:
	{
		RETURN_IF_ERROR(lvalue.reference.svg_orient->Copy(*rvalue.reference.svg_orient));
	}
	break;
	case REFERENCE_SVG_CLASSOBJECT:
	{
		RETURN_IF_ERROR(lvalue.reference.svg_classobject->Copy(*rvalue.reference.svg_classobject));
	}
	break;
    default:
	{
	    OP_ASSERT(!"All reference types should be assignable");
	}
    }

    switch(lvalue.value_type)
    {
    case VALUE_NUMBER:
    case VALUE_PERCENTAGE:
	{
	    lvalue.value.number = rvalue.value.number;
	}
	break;
    case VALUE_COLOR:
	{
	    lvalue.value.color = rvalue.value.color;
	}
	break;
	case VALUE_EMPTY:
		/* Nothing to do */
		break;
	default:
		OP_ASSERT(!"Not reached");
    }

    return OpStatus::OK;
}

static const SVGObjectType s_reference_to_object_type[] = {
	SVGOBJECT_NUMBER,
	SVGOBJECT_LENGTH,
	SVGOBJECT_STRING,
	SVGOBJECT_BASELINE_SHIFT,
	SVGOBJECT_COLOR,
	SVGOBJECT_PAINT,
	SVGOBJECT_FONTSIZE,
	SVGOBJECT_POINT,
	SVGOBJECT_RECT,
	SVGOBJECT_VECTOR,
	SVGOBJECT_PATH,
	SVGOBJECT_MATRIX,
	SVGOBJECT_TRANSFORM,
	SVGOBJECT_ENUM,
	SVGOBJECT_URL,
	SVGOBJECT_ASPECT_RATIO,
	SVGOBJECT_NAVREF,
	SVGOBJECT_ORIENT,
	SVGOBJECT_CLASSOBJECT
};

/* static */ SVGObjectType
SVGAnimationValue::TranslateToSVGObjectType(ReferenceType reference_type)
{
	return (reference_type < REFERENCE_EMPTY) ?
		s_reference_to_object_type[reference_type] :
		SVGOBJECT_UNKNOWN;
}

/* static */ float
SVGAnimationValue::CalculateDistance(SVGAnimationValueContext &context,
									 const SVGAnimationValue &from_value,
									 const SVGAnimationValue &to_value)
{
    if (from_value.value_type != to_value.value_type)
    {
		return 0.0f;
    }

    if (from_value.value_type == VALUE_NUMBER ||
		from_value.value_type == VALUE_PERCENTAGE)
    {
		float a = from_value.value.number;
		float b = to_value.value.number;
		return (float)op_fabs(b - a);
    }
    else if (from_value.value_type == VALUE_COLOR)
    {
		return CalculateDistanceColors(from_value.value.color,
									   to_value.value.color);
    }
    else if (from_value.value_type == VALUE_EMPTY)
    {
		switch(from_value.reference_type)
		{
		case REFERENCE_SVG_VECTOR:
			return CalculateDistanceSVGVector(context, *from_value.reference.svg_vector,
											  *to_value.reference.svg_vector);
		case REFERENCE_SVG_POINT:
			return CalculateDistanceSVGPoint(context, *from_value.reference.svg_point,
											 *to_value.reference.svg_point);
		case REFERENCE_SVG_TRANSFORM:
			return CalculateDistanceSVGTransform(*from_value.reference.svg_transform,
												 *to_value.reference.svg_transform);
		case REFERENCE_SVG_OPBPATH:
			return CalculateDistanceSVGPath(*from_value.reference.svg_path,
											*to_value.reference.svg_path);
		}
    }

	return 0.0f;
}

/* static */ UINT32
SVGAnimationValue::InterpolateColors(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									 UINT32 from, UINT32 to, ExtraOperation extra_operation)
{
    int fb = GetBValue(from);
    int tb = GetBValue(to);

    int fr = GetRValue(from);
    int tr = GetRValue(to);

    int fg = GetGValue(from);
    int tg = GetGValue(to);

    if (extra_operation == EXTRA_OPERATION_TREAT_TO_AS_BY)
    {
		tr += fr;
		tg += fg;
		tb += fb;
    }

    int cb = fb + (int)(interval_position * (tb-fb));
    int cr = fr + (int)(interval_position * (tr-fr));
    int cg = fg + (int)(interval_position * (tg-fg));

    return OP_RGB(cr, cg, cb);
}

/* static */ float
SVGAnimationValue::CalculateDistanceColors(UINT32 from, UINT32 to)
{
    int fr, fg, fb, tr, tb, tg;

    fr = GetRValue(from);
    fg = GetGValue(from);
    fb = GetBValue(from);

    tr = GetRValue(to);
    tg = GetGValue(to);
    tb = GetBValue(to);

    int dr = fr - tr;
    int dg = fg - tg;
    int db = fb - tb;

    return (float)op_sqrt((double)(dr*dr + dg*dg + db*db));
}

/* static */ UINT32
SVGAnimationValue::AddBasevalueColor(UINT32 base, UINT32 anim)
{
    int fr, fg, fb, tr, tb, tg;

    fr = GetRValue(base);
    fg = GetGValue(base);
    fb = GetBValue(base);

    tr = GetRValue(anim);
    tg = GetGValue(anim);
    tb = GetBValue(anim);

    return OP_RGB(MIN(255, fr + tr),
				  MIN(255, fg + tg),
				  MIN(255, fb + tb));
}

/* static */ UINT32
SVGAnimationValue::AddAccumulationValueColor(UINT32 accum,
											 UINT32 base,
											 SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
											 UINT32 anim)
{
    int fr, fg, fb, tr, tb, tg;

    fr = GetRValue(accum) + GetRValue(base);
    fg = GetGValue(accum) + GetGValue(base);
    fb = GetBValue(accum) + GetBValue(base);

    tr = GetRValue(anim);
    tg = GetGValue(anim);
    tb = GetBValue(anim);

    return OP_RGB(MIN(255, (fr * interval_repetition) + tr),
				  MIN(255, (fg * interval_repetition) + tg),
				  MIN(255, (fb * interval_repetition) + tb));
}

/* static */ void
SVGAnimationValue::Setup(SVGAnimationValue &animation_value,
						 SVGAnimationValueContext &context)
{
    switch(animation_value.reference_type)
    {
    case REFERENCE_SVG_BASELINE_SHIFT:
	{
	    SVGBaselineShift &baseline_shift = *animation_value.reference.svg_baseline_shift;

	    if (baseline_shift.GetShiftType() == SVGBaselineShift::SVGBASELINESHIFT_VALUE)
	    {
			SetAnimationValueFromLength(animation_value, context, baseline_shift.GetValueRef());
	    }
		else
		{
			animation_value.value_type = VALUE_EMPTY;
		}
	}
	break;
    case REFERENCE_SVG_FONT_SIZE:
	{
	    SVGFontSize &font_size = *animation_value.reference.svg_font_size;

	    if (font_size.Mode() == SVGFontSize::MODE_LENGTH)
	    {
			SVGLength &length = font_size.Length();
			if (length.GetUnit() == CSS_PERCENTAGE)
			{
				animation_value.value_type = VALUE_NUMBER;
				animation_value.value.number =
					length.GetScalar().GetFloatValue() * context.GetFontHeight() / 100;
			}
			else
			{
				SetAnimationValueFromLength(animation_value, context, font_size.Length());
			}

	    }
		else if (font_size.Mode() == SVGFontSize::MODE_ABSOLUTE)
		{
			animation_value.value_type = VALUE_NUMBER;
			animation_value.value.number = (float)font_size.AbsoluteFontSize();
		}
		else if (font_size.Mode() == SVGFontSize::MODE_RELATIVE)
		{
			animation_value.value_type = VALUE_NUMBER;
			if (font_size.RelativeFontSize() == SVGRELATIVEFONTSIZE_SMALLER)
				animation_value.value.number = (float)0.8*context.GetFontHeight();
			else if (font_size.RelativeFontSize() == SVGRELATIVEFONTSIZE_LARGER)
				animation_value.value.number = (float)1.2*context.GetFontHeight();
			else
			{
				OP_ASSERT(!"Not reached");
				animation_value.value.number = 1.0;
			}
		}
		else if (font_size.Mode() == SVGFontSize::MODE_RESOLVED)
		{
			animation_value.value_type = VALUE_NUMBER;
			animation_value.value.number = font_size.ResolvedLength().GetFloatValue();
		}
		else
		{
			// Default font-size:
			animation_value.value_type = VALUE_NUMBER;
			animation_value.value.number = 16.0;
		}
	}
	break;
    case REFERENCE_SVG_NUMBER:
	{
	    animation_value.value_type = VALUE_NUMBER;
	    animation_value.value.number = animation_value.reference.svg_number->GetFloatValue();
	}
	break;
    case REFERENCE_SVG_ORIENT:
	{
		SVGOrient &orient = *animation_value.reference.svg_orient;
		if (orient.GetOrientType() == SVGORIENT_ANGLE)
		{
			SVGAngle &angle = *orient.GetAngle();
			SVGNumber angle_in_deg = angle.GetAngleInUnits(SVGANGLE_DEG);
			animation_value.value_type = VALUE_NUMBER;
			animation_value.value.number = angle_in_deg.GetFloatValue();
		}
	}
	break;
    case REFERENCE_SVG_LENGTH:
	{
		SetAnimationValueFromLength(animation_value, context, *animation_value.reference.svg_length);
	}
	break;
    case REFERENCE_SVG_PAINT:
	{
	    SVGPaint &svg_paint = *animation_value.reference.svg_paint;
	    if (svg_paint.GetPaintType() == SVGPaint::RGBCOLOR ||
			svg_paint.GetPaintType() == SVGPaint::RGBCOLOR_ICCCOLOR)
	    {
			animation_value.value_type = VALUE_COLOR;
			animation_value.value.color = svg_paint.GetRGBColor();
	    }
		else if (svg_paint.GetPaintType() == SVGPaint::CURRENT_COLOR)
		{
			animation_value.value_type = VALUE_COLOR;
			animation_value.value.color = context.GetCurrentColor();
		}
		else if (svg_paint.GetPaintType() == SVGPaint::INHERIT)
		{
			animation_value.value_type = VALUE_COLOR;
			animation_value.value.color = context.GetInheritRGBColor();
		}
		else
		{
			animation_value.value_type = VALUE_EMPTY;
		}
	}
	break;
    case REFERENCE_SVG_COLOR:
	{
	    SVGColor &svg_color = *animation_value.reference.svg_color;
	    if (svg_color.GetColorType() == SVGColor::SVGCOLOR_RGBCOLOR ||
			svg_color.GetColorType() == SVGColor::SVGCOLOR_RGBCOLOR_ICCCOLOR)
	    {
			animation_value.value_type = VALUE_COLOR;
			animation_value.value.color = svg_color.GetRGBColor();
	    }
		else if (svg_color.GetColorType() == SVGColor::SVGCOLOR_CURRENT_COLOR)
		{
			animation_value.value_type = VALUE_COLOR;
			animation_value.value.color = context.GetCurrentColor();
		}
#if 0 // Not sure how to deal with colors and inherit
		else if (svg_color.GetColorType() == SVGColor::SVGCOLOR_INHERIT)
		{
			animation_value.value_type = VALUE_COLOR;
			const HTMLayoutProperties* parent_props = context.GetParentProps();
			if (parent_props)
				animation_value.value.color = parent_props->font_color;
		}
#endif
		else
		{
			animation_value.value_type = VALUE_EMPTY;
		}
	}
	break;
    default:
		animation_value.value_type = VALUE_EMPTY;
		break;
    }
}

/* static */ BOOL
SVGAnimationValue::Initialize(SVGAnimationValue &animation_value,
							  SVGObject *svg_object,
							  SVGAnimationValueContext &context)
{
    if (svg_object == NULL)
    {
		return FALSE;
    }

    if (!svg_object->InitializeAnimationValue(animation_value))
		return FALSE;

    Setup(animation_value, context);

    return TRUE;
}

/* static */ OP_STATUS
SVGAnimationValue::Transfer(SVGAnimationValue &animation_value)
{
    switch(animation_value.reference_type)
    {
    case REFERENCE_SVG_BASELINE_SHIFT:
    case REFERENCE_SVG_LENGTH:
	{
	    SVGLength *length = NULL;
	    if (animation_value.reference_type == REFERENCE_SVG_BASELINE_SHIFT)
	    {
			SVGBaselineShift &baseline_shift = *animation_value.reference.svg_baseline_shift;
			baseline_shift.SetShiftType(SVGBaselineShift::SVGBASELINESHIFT_VALUE);
			length = &baseline_shift.GetValueRef();
	    }
	    else
	    {
			length = animation_value.reference.svg_length;
	    }

	    if (animation_value.value_type == VALUE_PERCENTAGE)
			length->SetUnit(CSS_PERCENTAGE);
		else
			length->SetUnit(CSS_NUMBER);

		length->SetScalar(animation_value.value.number);
	}
	break;
    case REFERENCE_SVG_FONT_SIZE:
	{
		SVGFontSize &font_size = *animation_value.reference.svg_font_size;
		if (animation_value.value_type == VALUE_PERCENTAGE)
		{
			font_size.SetLengthMode();
			SVGLength &length = font_size.Length();
			length.SetScalar(animation_value.value.number);
			length.SetUnit(CSS_PERCENTAGE);
		}
		else
		{
			font_size.Resolve(animation_value.value.number);
		}
	}
	break;
    case REFERENCE_SVG_PAINT:
	{
	    SVGPaint *paint = animation_value.reference.svg_paint;
	    paint->SetPaintType(SVGPaint::RGBCOLOR);
	    paint->SetColorRef(animation_value.value.color);
	}
	break;
    case REFERENCE_SVG_COLOR:
	{
	    SVGColor &color = *animation_value.reference.svg_color;
	    color.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
	    color.SetColorRef(animation_value.value.color);
	}
	break;
	case REFERENCE_SVG_NUMBER:
		*animation_value.reference.svg_number = animation_value.value.number;
		break;
	case REFERENCE_SVG_ORIENT:
	{
		SVGOrient &orient = *animation_value.reference.svg_orient;
		RETURN_IF_ERROR(orient.SetToAngleInDeg(animation_value.value.number));
	}
	break;
	default:
		break;
    }
	return OpStatus::OK;
}

/* static */ float
SVGAnimationValue::ResolveLength(float value, int css_unit, SVGAnimationValueContext& context)
{
    switch(css_unit)
    {
    case CSS_EM:
		return value * context.GetFontHeight();
	case CSS_REM:
		return value * context.GetRootFontHeight();
    case CSS_EX:
		return value * context.GetExHeight();
    case CSS_PX:
		return value;
    case CSS_PT:
		return (value * CSS_PX_PER_INCH) / 72.0f;
    case CSS_PC:
		return (value * CSS_PX_PER_INCH) / 6.0f;
    case CSS_CM:
		return (value * CSS_PX_PER_INCH) / 2.54f;
    case CSS_MM:
		return (value * CSS_PX_PER_INCH) / 25.4f;
    case CSS_IN:
		return value * CSS_PX_PER_INCH;
    case CSS_PERCENTAGE:
		return (value / 100.0f) * context.GetPercentageOf();
    default:
		return value;
    }
}

/* static */ void
SVGAnimationValue::InterpolateSVGRects(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									   SVGRect *from_rect,
									   SVGRect *to_rect,
									   ExtraOperation extra_operation,
									   SVGRect *target_rect)
{
	/* Ignores extra_operation and treats rects as non-additive */
	target_rect->Interpolate(*from_rect, *to_rect, interval_position);
}

/* static */ OP_STATUS
SVGAnimationValue::InterpolateSVGVectors(SVGAnimationValueContext &context,
										 SVG_ANIMATION_INTERVAL_POSITION interval_position,
										 SVGVector *from_vector,
										 SVGVector *to_vector,
										 ExtraOperation extra_operation,
										 SVGVector *target_vector)
{
	/* Ignores extra_operation and treats vectors as non-additive.
	 * This one actually calls back into
	 * SVGAnimationValue::Interpolate to interpolate the objects it
	 * holds.
	 */
	return target_vector->Interpolate(context, *from_vector, *to_vector, interval_position);
}

/* static */ OP_STATUS
SVGAnimationValue::InterpolateSVGOpBpaths(SVG_ANIMATION_INTERVAL_POSITION interval_position,
										  OpBpath *from_path,
										  OpBpath *to_path,
										  ExtraOperation extra_operation,
										  OpBpath *target_path)
{
	OP_STATUS status = from_path->SetUsedByDOM(TRUE);

	if (OpStatus::IsSuccess(status))
		status = to_path->SetUsedByDOM(TRUE);

	RETURN_IF_ERROR(status);

	/* Ignores extra_operation and treats paths as non-additive */
	return target_path->Interpolate(*from_path, *to_path, interval_position);
}

/* static */ void
SVGAnimationValue::InterpolateSVGPoints(SVG_ANIMATION_INTERVAL_POSITION interval_position,
										SVGPoint *from_point,
										SVGPoint *to_point,
										ExtraOperation extra_operation,
										SVGPoint *target_point)
{
	/* Ignores extra_operation and treats points as non-additive */
	target_point->Interpolate(*from_point, *to_point, interval_position);
}

/* static */ OP_STATUS
SVGAnimationValue::InterpolateSVGTransforms(SVG_ANIMATION_INTERVAL_POSITION interval_position,
											const SVGTransform &from_transform,
											const SVGTransform &to_transform,
											ExtraOperation extra_operation,
											SVGTransform &target_transform)
{
	SVGTransform modified_from_transform;
	SVGTransform modified_to_transform;

	modified_from_transform.Copy(from_transform);
	modified_to_transform.Copy(to_transform);

	if (modified_from_transform.GetTransformType() == SVGTRANSFORM_UNKNOWN)
	{
		modified_from_transform.Copy(to_transform);
		modified_from_transform.SetZero();
	}
	else if (modified_to_transform.GetTransformType() == SVGTRANSFORM_UNKNOWN)
	{
		modified_to_transform.Copy(from_transform);
		modified_from_transform.SetZero();
	}

	if (modified_from_transform.GetTransformType() == SVGTRANSFORM_UNKNOWN ||
		modified_from_transform.GetTransformType() != modified_to_transform.GetTransformType())
	{
		modified_from_transform.Copy(modified_to_transform);
		modified_from_transform.SetZero();
	}

	modified_from_transform.MakeDefaultsExplicit();
	modified_to_transform.MakeDefaultsExplicit();

    if (extra_operation == EXTRA_OPERATION_TREAT_TO_AS_BY)
    {
		modified_to_transform.AddTransform(modified_from_transform);
	}

	target_transform.SetTransformType(modified_from_transform.GetTransformType());
	target_transform.Interpolate(modified_from_transform, modified_to_transform, interval_position);
	return OpStatus::OK;
}

/* static */ void
SVGAnimationValue::SetCSSProperty(SVGAnimationAttributeLocation &location,
								  SVGAnimationValue &animation_value,
								  SVGAnimationValueContext &context)
{
	const HTMLayoutProperties* props = context.GetProps();
	if (!props)
		return;

	const SvgProperties *svg_props = props->svg;

	switch(location.animated_name)
	{
	case Markup::SVGA_STROKE_DASHOFFSET:
	case Markup::SVGA_STROKE_WIDTH:
	{
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		const SVGLength &length = location.animated_name == Markup::SVGA_STROKE_DASHOFFSET ?
			svg_props->dashoffset :
			svg_props->linewidth;
		animation_value.value.number = SVGAnimationValue::ResolveLength(length.GetScalar().GetFloatValue(),
																		length.GetUnit(), context);
	}
	break;
	case Markup::SVGA_FILL_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = props->opacity/255.0f;
		break;
	case Markup::SVGA_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = props->opacity/255.0f;
		break;
	case Markup::SVGA_FLOOD_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->floodopacity/255.0f;
		break;
	case Markup::SVGA_STOP_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->stopopacity/255.0f;
		break;
	case Markup::SVGA_STROKE_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->strokeopacity/255.0f;
		break;
	case Markup::SVGA_SOLID_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->solidopacity/255.0f;
		break;
	case Markup::SVGA_VIEWPORT_FILL_OPACITY:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->viewportfillopacity/255.0f;
		break;
	case Markup::SVGA_STROKE_MITERLIMIT:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->miterlimit.GetFloatValue();
		break;
	case Markup::SVGA_LETTER_SPACING:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = (float)props->letter_spacing;
		break;
	case Markup::SVGA_WORD_SPACING:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = props->word_spacing_i / 256.0f;
		break;

	case Markup::SVGA_FILL:
		animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_PAINT;

		// We make a const_cast here since we don't have
		// SVGAnimationValues with const references (yet). Since it is
		// a base value, we will just read from it anyway.
		animation_value.reference.svg_paint = const_cast<SVGPaint *>(svg_props->GetFill());
		SVGAnimationValue::Setup(animation_value, context);
		break;
	case Markup::SVGA_STROKE:
		animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_PAINT;

		// We make a const_cast here since we don't have
		// SVGAnimationValues with const references (yet). Since it is
		// a base value, we will just read from it anyway.
		animation_value.reference.svg_paint = const_cast<SVGPaint *>(svg_props->GetStroke());
		SVGAnimationValue::Setup(animation_value, context);
		break;

	case Markup::SVGA_COLOR:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = props->font_color;
		break;
	case Markup::SVGA_FLOOD_COLOR:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = svg_props->floodcolor.GetRGBColor();
		break;
	case Markup::SVGA_LIGHTING_COLOR:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = svg_props->lightingcolor.GetRGBColor();
		break;
	case Markup::SVGA_STOP_COLOR:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = svg_props->stopcolor.GetRGBColor();
		break;
	case Markup::SVGA_SOLID_COLOR:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = svg_props->solidcolor.GetRGBColor();
		break;
	case Markup::SVGA_VIEWPORT_FILL:
		animation_value.value_type = SVGAnimationValue::VALUE_COLOR;
		animation_value.value.color = svg_props->viewportfill.GetRGBColor();
		break;
	case Markup::SVGA_FONT_SIZE:
		animation_value.value_type = SVGAnimationValue::VALUE_NUMBER;
		animation_value.value.number = svg_props->fontsize.GetFloatValue();
		break;
	}
}

/* static */ float
SVGAnimationValue::CalculateDistanceSVGVector(SVGAnimationValueContext &context, const SVGVector &from, const SVGVector &to)
{
	return SVGVector::CalculateDistance(from, to, context);
}

/* static */ float
SVGAnimationValue::CalculateDistanceSVGPoint(SVGAnimationValueContext &context, const SVGPoint &from, const SVGPoint &to)
{
	SVGNumber dx = from.x - to.x;
	SVGNumber dy = from.y - to.y;
	return ((dx * dx) + (dy * dy)).sqrt().GetFloatValue();
}

/* static */ float
SVGAnimationValue::CalculateDistanceSVGTransform(const SVGTransform &from,
												 const SVGTransform &to)
{
	return from.Distance(to);
}

/* static */ float
SVGAnimationValue::CalculateDistanceSVGPath(const OpBpath &from,
											const OpBpath &to)
{
	// UNIMPLEMENTED. The 1.2 spec has a crazy definition (in my
	// opinion). We will see how it turns out. SVG 1.1 does not say
	// how to calculate paced interpolation of paths.
	return 0.0;
}

/* static */
SVGAnimationValue SVGAnimationValue::Empty()
{
	SVGAnimationValue v;
	v.reference_type = REFERENCE_EMPTY;
	v.value_type = VALUE_EMPTY;
	return v;
}

/* static */ BOOL
SVGAnimationValue::IsEmpty(SVGAnimationValue &animation_value)
{
	return (animation_value.reference_type == REFERENCE_EMPTY &&
			animation_value.value_type == VALUE_EMPTY);
}


/* static */ void
SVGAnimationValue::SetAnimationValueFromLength(SVGAnimationValue &animation_value,
											   SVGAnimationValueContext &context,
											   SVGLength &length)
{
	if (length.GetUnit() == CSS_PERCENTAGE)
	{
		animation_value.value_type = VALUE_PERCENTAGE;
		animation_value.value.number = length.GetScalar().GetFloatValue();
	}
	else
	{
		float n = ResolveLength(length.GetScalar().GetFloatValue(),
								length.GetUnit(), context);

		animation_value.value_type = VALUE_NUMBER;
		animation_value.value.number = n;
	}
}

#endif // SVG_SUPPORT
