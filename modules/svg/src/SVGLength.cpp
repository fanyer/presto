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
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/SVGUtils.h"

#include "modules/svg/src/animation/svganimationvalue.h"

#include "modules/util/tempbuf.h"

SVGLengthObject::SVGLengthObject(SVGNumber number /* = 0 */, int unit /* = CSS_NUMBER */) :
		SVGObject(SVGOBJECT_LENGTH),
		m_length(number, unit)
{
}

SVGLengthObject::SVGLengthObject(const SVGLength* length) :
		SVGObject(SVGOBJECT_LENGTH)
{
	if (length)
	{
		m_length.SetScalar(length->GetScalar());
		m_length.SetUnit(length->GetUnit());
	}
}

BOOL
SVGLength::operator==(const SVGLength& other) const
{
	return (m_scalar == other.m_scalar &&
			m_unit == other.m_unit);
}

OP_STATUS
SVGLength::GetStringRepresentation(TempBuffer* buffer) const
{
	const char* unit = NULL;
	switch(m_unit)
	{
		case CSS_IN:         unit = "in"; break;
		case CSS_EX:         unit = "ex"; break;
		case CSS_PERCENTAGE: unit = "%"; break;
		case CSS_MM:         unit = "mm"; break;
		case CSS_CM:         unit = "cm"; break;
		case CSS_EM:         unit = "em"; break;
		case CSS_PC:         unit = "pc"; break;
		case CSS_PT:         unit = "pt"; break;
		case CSS_PX:         unit = "px"; break;
		default:             unit = NULL; break;
	}

	OP_STATUS result = buffer->AppendDoubleToPrecision(m_scalar.GetFloatValue());
	if(unit && OpStatus::IsSuccess(result))
	{
		result = buffer->Append(unit, op_strlen(unit));
	}
	return result;
}

void
SVGLengthObject::Copy(const SVGLengthObject& v1)
{
	CopyFlags(v1);
	m_length = v1.m_length;
}

/* virtual */ SVGObject *
SVGLengthObject::Clone() const
{
	SVGLengthObject *len_obj = OP_NEW(SVGLengthObject, ());
	if (len_obj)
		len_obj->Copy(*this);
	return len_obj;
}

/* virtual */ OP_STATUS
SVGLengthObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return m_length.GetStringRepresentation(buffer);
}

void
SVGLengthObject::ConvertToUnit(int css_unit, SVGLength::LengthOrientation type, const SVGValueContext& vcxt)
{
	if(css_unit != m_length.GetUnit())
	{
		if(m_length.GetScalar().Equal(0))
		{
			m_length.SetUnit(css_unit);
		}
		else if(OpStatus::IsSuccess(SVGUtils::ConvertToUnit(this, css_unit, type, vcxt)))
		{
			m_length.SetUnit(css_unit);
		}
	}
}

/* virtual */
BOOL SVGLengthObject::IsEqual(const SVGObject& other) const
{
	if (other.Type() == SVGOBJECT_LENGTH)
	{
		const SVGLengthObject& other_length =
			static_cast<const SVGLengthObject&>(other);

		return other_length.GetLength() == GetLength();
	}
	return FALSE;
}

/* virtual */ BOOL
SVGLengthObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_LENGTH;
	animation_value.reference.svg_length = &m_length;
	return TRUE;
}

#endif // SVG_SUPPORT
