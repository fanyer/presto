/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/SVGManager.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGImageImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/svgdom/svgdomlengthimpl.h"

#include "modules/pi/OpScreenInfo.h"

#include "modules/layout/cascade.h"
#include "modules/layout/box/box.h"

/* static */ SVGDOMLength::UnitType
SVGDOMLengthImpl::CSSUnitToUnitType(int css_unit)
{
	switch(css_unit)
	{
		case CSS_EM: return SVG_DOM_LENGTHTYPE_EMS;
		case CSS_REM: return SVG_DOM_LENGTHTYPE_REMS;
		case CSS_EX: return SVG_DOM_LENGTHTYPE_EXS;
		case CSS_PX: return SVG_DOM_LENGTHTYPE_PX;
		case CSS_CM: return SVG_DOM_LENGTHTYPE_CM;
		case CSS_MM: return SVG_DOM_LENGTHTYPE_MM;
		case CSS_IN: return SVG_DOM_LENGTHTYPE_IN;
		case CSS_PT: return SVG_DOM_LENGTHTYPE_PT;
		case CSS_PC: return SVG_DOM_LENGTHTYPE_PC;
		case CSS_PERCENTAGE: return SVG_DOM_LENGTHTYPE_PERCENTAGE;
		case CSS_NUMBER: return SVG_DOM_LENGTHTYPE_NUMBER;
		default:     return SVG_DOM_LENGTHTYPE_UNKNOWN;
	}
}

/* static */ int
SVGDOMLengthImpl::UnitTypeToCSSUnit(SVGDOMLength::UnitType unit_type)
{
	switch(unit_type)
	{
		case SVG_DOM_LENGTHTYPE_EMS: return CSS_EM;
		case SVG_DOM_LENGTHTYPE_REMS: return CSS_REM;
		case SVG_DOM_LENGTHTYPE_EXS: return CSS_EX;
		case SVG_DOM_LENGTHTYPE_PX:  return CSS_PX;
		case SVG_DOM_LENGTHTYPE_CM:  return CSS_CM;
		case SVG_DOM_LENGTHTYPE_MM:  return CSS_MM;
		case SVG_DOM_LENGTHTYPE_IN:  return CSS_IN;
		case SVG_DOM_LENGTHTYPE_PT:  return CSS_PT;
		case SVG_DOM_LENGTHTYPE_PC:  return CSS_PC;
		case SVG_DOM_LENGTHTYPE_PERCENTAGE: return CSS_PERCENTAGE;
		case SVG_DOM_LENGTHTYPE_NUMBER: return CSS_NUMBER;
		default: return CSS_PX;
	}
}

SVGDOMLengthImpl::SVGDOMLengthImpl(SVGLengthObject* length) :
		m_length(length)
{
	SVGObject::IncRef(m_length);
}

/* virtual */
SVGDOMLengthImpl::~SVGDOMLengthImpl()
{
	SVGObject::DecRef(m_length);
}

/* virtual */ const char*
SVGDOMLengthImpl::GetDOMName()
{
	return "SVGLength";
}

/* virtual */ SVGDOMLength::UnitType
SVGDOMLengthImpl::GetUnitType()
{
	return CSSUnitToUnitType(m_length->GetUnit());
}

/* virtual */ OP_BOOLEAN
SVGDOMLengthImpl::SetValue(double v)
{
	RETURN_FALSE_IF(m_length->GetScalar() == SVGNumber(v) &&
					m_length->GetUnit() == CSS_NUMBER);

	m_length->SetScalar(SVGNumber(v));
	m_length->SetUnit(CSS_NUMBER);
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMLengthImpl::GetValue(HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns)
{
	if (m_length->GetUnit() == CSS_NUMBER || m_length->GetUnit() == CSS_PX)
	{
		return m_length->GetScalar().GetFloatValue();
	}
	else
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
			return 0;

		HLDocProfile* hld_profile = doc_ctx->GetHLDocProfile();
		if (!hld_profile)
			return 0;

		Head prop_list;
		LayoutProperties* length_props = LayoutProperties::CreateCascade(elm,
																		 prop_list,
																		 LAYOUT_WORKPLACE(hld_profile));
		if (!length_props)
			return 0;
		const HTMLayoutProperties& props = *length_props->GetProps();
		const SvgProperties *svg_props = props.svg;

		SVGNumber fontheight(svg_props->fontsize);
		int font_number = props.font_number;
		length_props = NULL;
		prop_list.Clear();

		SVGNumberPair viewport(100,100);
		// If length belongs to element, then get a viewport (if necessary)
		if (m_length->GetUnit() == CSS_PERCENTAGE)
		{
#ifdef _DEBUG
			OP_STATUS status = 
#endif // _DEBUG
				SVGUtils::GetViewportForElement(elm,
												doc_ctx,
												viewport);
#ifdef _DEBUG
			OpStatus::Ignore(status);
#endif // _DEBUG
		}

		SVGLength::LengthOrientation orientation = SVGUtils::GetLengthOrientation(attr_name, ns);
		SVGValueContext value_ctx(viewport.x, viewport.y, fontheight,
								  SVGUtils::GetExHeight(doc_ctx->GetVisualDevice(), fontheight, font_number),
								  LayoutFixedToSVGNumber(hld_profile->GetLayoutWorkplace()->GetDocRootProperties().GetRootFontSize()));

		return SVGUtils::ResolveLength(m_length->GetLength(), orientation, value_ctx).GetFloatValue();
	}
}

/* virtual */ double
SVGDOMLengthImpl::GetValueInSpecifiedUnits()
{
	return m_length->GetScalar().GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMLengthImpl::SetValueInSpecifiedUnits(double v)
{
	RETURN_FALSE_IF(m_length->GetScalar() == SVGNumber(v));
	m_length->SetScalar(SVGNumber(v));
	return OpBoolean::IS_TRUE;
}

/* virtual */ const uni_char*
SVGDOMLengthImpl::GetValueAsString()
{
	TempBuffer* buf = g_svg_manager_impl->GetEmptyTempBuf();
	m_length->GetStringRepresentation(buf);
	return buf->GetStorage();
}

/* virtual */ OP_BOOLEAN
SVGDOMLengthImpl::SetValueAsString(const uni_char* v)
{
	SVGLengthObject* new_len = NULL;
	OP_STATUS status = SVGAttributeParser::ParseLength(v, uni_strlen(v), &new_len);
	if (OpStatus::IsSuccess(status))
	{
		m_length->Copy(*new_len);
		OP_DELETE(new_len);
		return OpBoolean::IS_TRUE;
	}
	else if (OpStatus::IsMemoryError(status))
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpBoolean::IS_FALSE;
}

/* virtual */ OP_BOOLEAN
SVGDOMLengthImpl::NewValueSpecifiedUnits(SVGDOMLength::UnitType unit_type, double value_in_specified_units)
{
	RETURN_FALSE_IF(m_length->GetScalar() == SVGNumber(value_in_specified_units) &&
					m_length->GetUnit() == UnitTypeToCSSUnit(unit_type));

	m_length->SetScalar(value_in_specified_units);
	m_length->SetUnit(UnitTypeToCSSUnit(unit_type));
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMLengthImpl::ConvertToSpecifiedUnits(SVGDOMLength::UnitType unit_type, HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns)
{
	SVGValueContext vcxt;
	SVGLength::LengthOrientation orientation = SVGLength::SVGLENGTH_OTHER;
	int current_unit = m_length->GetUnit();

	if (elm != NULL)
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		OP_ASSERT(doc_ctx); // XXX Or can elm be outside the document?
		if (!doc_ctx)
			return OpBoolean::IS_FALSE;

		BOOL new_unit_needs_font_size = unit_type == SVG_DOM_LENGTHTYPE_EMS || unit_type == SVG_DOM_LENGTHTYPE_EXS;

		if (new_unit_needs_font_size || current_unit == CSS_EM || current_unit == CSS_EX)
		{
			HLDocProfile* hldoc_profile = doc_ctx->GetHLDocProfile();
			if (hldoc_profile)
			{
				AutoDeleteHead prop_list;
				LayoutProperties* layout_props = LayoutProperties::CreateCascade(elm,
																				 prop_list,
																				 hldoc_profile);
				if (layout_props)
				{
					const HTMLayoutProperties& props = *layout_props->GetProps();
					if (props.font_size == 0 && new_unit_needs_font_size)
						// We can't perform a conversion (division by zero would occur).
						return OpBoolean::IS_FALSE;
					vcxt.fontsize = props.font_size;
					if (unit_type == SVG_DOM_LENGTHTYPE_EXS || current_unit == CSS_EX)
					{
						vcxt.ex_height = SVGUtils::GetExHeight(doc_ctx->GetVisualDevice(), vcxt.fontsize, props.font_number);
						OP_ASSERT(vcxt.ex_height != 0);
					}
				}
				else
					return OpBoolean::IS_FALSE;
			}
			else
				return OpBoolean::IS_FALSE;
		}

		if (unit_type == SVG_DOM_LENGTHTYPE_REMS || current_unit == CSS_REM)
		{
			LayoutWorkplace* lwp = doc_ctx->GetLayoutWorkplace();
			if (lwp)
				vcxt.root_font_size = LayoutFixedToSVGNumber(lwp->GetDocRootProperties().GetRootFontSize());

			if (vcxt.root_font_size == 0 && unit_type == SVG_DOM_LENGTHTYPE_REMS)
				return OpBoolean::IS_FALSE;
		}

		if (unit_type == SVG_DOM_LENGTHTYPE_PERCENTAGE || current_unit == CSS_PERCENTAGE)
		{
			SVGNumberPair viewport;
			OP_STATUS status = SVGUtils::GetViewportForElement(elm, doc_ctx, viewport);
			RETURN_IF_MEMORY_ERROR(status);
			if (OpStatus::IsError(status))
				return OpBoolean::IS_FALSE;

			orientation = SVGUtils::GetLengthOrientation(attr_name, ns);
			if (orientation == SVGLength::SVGLENGTH_X && viewport.x == 0 ||
				orientation == SVGLength::SVGLENGTH_Y && viewport.y == 0 ||
				orientation == SVGLength::SVGLENGTH_OTHER && (viewport.x == 0 || viewport.y == 0))
				return OpBoolean::IS_FALSE;

			vcxt.viewport_width = viewport.x;
			vcxt.viewport_height = viewport.y;
		}
	}
	else if (current_unit == CSS_EM || current_unit == CSS_REM || current_unit == CSS_EX || current_unit == CSS_PERCENTAGE ||
			 unit_type == SVG_DOM_LENGTHTYPE_EMS || unit_type == SVG_DOM_LENGTHTYPE_EXS ||
			 unit_type == SVG_DOM_LENGTHTYPE_REMS || unit_type == SVG_DOM_LENGTHTYPE_PERCENTAGE)
		/* We are trying to convert from or to one of the relative units.
		   But without elm, we don't have correct base values. */
		return OpBoolean::IS_FALSE;

	m_length->ConvertToUnit(UnitTypeToCSSUnit(unit_type), orientation, vcxt);

	return OpBoolean::IS_TRUE;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
