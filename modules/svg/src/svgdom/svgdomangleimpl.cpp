/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/svgdom/svgdomangleimpl.h"

SVGDOMAngleImpl::SVGDOMAngleImpl(SVGAngle* angle) : 
		m_angle(angle)
{
	SVGObject::IncRef(m_angle);
}

/* virtual */
SVGDOMAngleImpl::~SVGDOMAngleImpl()
{
	SVGObject::DecRef(m_angle);
}

/* static */ SVGDOMAngle::UnitType
SVGDOMAngleImpl::SVGAngleTypeToDOMAngleType(SVGAngleType svg_angle_type)
{
	switch(svg_angle_type)
	{
		case SVGANGLE_DEG:  return SVG_ANGLETYPE_DEG;
		case SVGANGLE_RAD:  return SVG_ANGLETYPE_RAD;
		case SVGANGLE_GRAD: return SVG_ANGLETYPE_GRAD;
		default: return SVG_ANGLETYPE_UNSPECIFIED;
	}
}

/* static */ SVGAngleType
SVGDOMAngleImpl::DOMAngleTypeToSVGAngleType(SVGDOMAngle::UnitType dom_angle_type)
{
	switch(dom_angle_type)
	{
		case SVG_ANGLETYPE_DEG: return SVGANGLE_DEG;
		case SVG_ANGLETYPE_RAD: return SVGANGLE_RAD;
		case SVG_ANGLETYPE_GRAD: return SVGANGLE_GRAD;
		default: return SVGANGLE_UNSPECIFIED;
	}
}

/* virtual */ const char*
SVGDOMAngleImpl::GetDOMName()
{
	return "SVGAngle";
}

/* virtual */ SVGDOMAngle::UnitType
SVGDOMAngleImpl::GetUnitType()
{
	return SVGAngleTypeToDOMAngleType(m_angle->GetAngleType());
}

/* virtual */ OP_BOOLEAN
SVGDOMAngleImpl::SetValue(double v)
{
	RETURN_FALSE_IF(m_angle->GetAngleValue() == SVGNumber(v) &&
					m_angle->GetAngleType() == SVGANGLE_DEG);
	m_angle->SetAngle(SVGNumber(v), SVGANGLE_DEG);
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMAngleImpl::GetValue()
{
	return m_angle->GetAngleInUnits(SVGANGLE_DEG).GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMAngleImpl::SetValueInSpecifiedUnits(double v)
{
	RETURN_FALSE_IF(m_angle->GetAngleValue() == SVGNumber(v));
	m_angle->SetAngleValue(SVGNumber(v));
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMAngleImpl::GetValueInSpecifiedUnits()
{
	return m_angle->GetAngleValue().GetFloatValue();
}

/* virtual */ OP_STATUS
SVGDOMAngleImpl::SetValueAsString(const uni_char* v)
{
	SVGOrient* parsed_orient = NULL;
	OP_STATUS status = SVGAttributeParser::ParseOrient(v, uni_strlen(v), parsed_orient);
	if (OpStatus::IsSuccess(status) &&
		(parsed_orient && parsed_orient->GetOrientType() == SVGORIENT_ANGLE))
	{
		m_angle->Copy(*parsed_orient->GetAngle());
	}

	// Delete parsed object
	OP_DELETE(parsed_orient);

	RETURN_IF_MEMORY_ERROR(status);
	return OpStatus::IsSuccess(status) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

/* virtual */ const uni_char*
SVGDOMAngleImpl::GetValueAsString()
{
	TempBuffer* buf = g_svg_manager_impl->GetEmptyTempBuf();
	m_angle->GetStringRepresentation(buf);
	return buf->GetStorage();
}

/* virtual */ OP_BOOLEAN
SVGDOMAngleImpl::NewValueSpecifiedUnits(SVGDOMAngle::UnitType unit_type, double value_in_specified_units)
{
	RETURN_FALSE_IF(m_angle->GetAngleValue() == SVGNumber(value_in_specified_units) &&
					m_angle->GetAngleType() == DOMAngleTypeToSVGAngleType(unit_type));
	m_angle->SetAngle(SVGNumber(value_in_specified_units),
					  DOMAngleTypeToSVGAngleType(unit_type));
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMAngleImpl::ConvertToSpecifiedUnits(SVGDOMAngle::UnitType unit_type)
{
	RETURN_FALSE_IF(m_angle->GetAngleType() == DOMAngleTypeToSVGAngleType(unit_type));
	SVGAngleType utype = DOMAngleTypeToSVGAngleType(unit_type);
	m_angle->SetAngle(m_angle->GetAngleInUnits(utype), utype);
	return OpBoolean::IS_TRUE;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
