/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGRect.h"

#include "modules/svg/src/animation/svganimationvalue.h"

SVGTransform::SVGTransform(SVGTransformType type) :
		SVGObject(SVGOBJECT_TRANSFORM),
		m_transform_type(type),
		m_packed_init(0)
{}

#ifdef SELFTEST
BOOL SVGTransform::operator==(const SVGTransform& other) const
{
	for(int i = 0; i < 6; i++)
		if(values[i].NotEqual(other.values[i]))
			return FALSE;
	return TRUE;
}
#endif // SELFTEST

/* virtual */ BOOL
SVGTransform::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_TRANSFORM)
	{
		const SVGTransform& other = static_cast<const SVGTransform&>(obj);
		if (other.m_transform_type == m_transform_type)
		{
			SVGMatrix m1, m2;
			GetMatrix(m1);
			other.GetMatrix(m2);
			return m1 == m2;
		}
	}
	return FALSE;
}

void
SVGTransform::Copy(const SVGTransform& transform)
{
	SVGObject::CopyFlags(transform);

	m_transform_type = transform.m_transform_type;
	m_packed = transform.m_packed;
	for (UINT32 i=0; i<6; ++i)
		values[i] = transform.values[i];
}

/* virtual */ SVGObject *
SVGTransform::Clone() const
{
	SVGTransform* transform_object = OP_NEW(SVGTransform, ());
	if (transform_object)
	{
		transform_object->Copy(*this);
	}
	return transform_object;
}

OP_STATUS SVGTransform::Concat(const SVGTransform& transformation,
									const SVGNumberPair& translation,
									const SVGNumber rot)
{
	m_transform_type = SVGTRANSFORM_MATRIX;
	SVGMatrix m1;
	transformation.GetMatrix(m1);
	SVGMatrix tmat;
	tmat.LoadTranslation(translation.x, translation.y);

	SVGMatrix rmat;
	rmat.LoadRotation(rot);

	m1.Multiply(rmat);
	m1.Multiply(tmat);

	for (UINT32 i=0; i<6; ++i)
		values[i] = m1[i];
	return OpStatus::OK;
}

OP_STATUS SVGTransform::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	OP_STATUS result = OpStatus::OK;

	switch(m_transform_type)
	{
	case SVGTRANSFORM_ROTATE:
		{
			RETURN_IF_ERROR(buffer->Append("rotate("));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[0].GetFloatValue()));
			if(m_packed.a2_set && m_packed.a3_set)
			{
				for (int i = 1; i < 3; i++)
				{
					RETURN_IF_ERROR(buffer->Append(' '));
					RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[i].GetFloatValue()));
				}
			}
			result = buffer->Append(")", 1);
		}
		break;
	case SVGTRANSFORM_TRANSLATE:
	case SVGTRANSFORM_SCALE:
		{
			RETURN_IF_ERROR(buffer->Append(m_transform_type == SVGTRANSFORM_TRANSLATE ? "translate(" : "scale("));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[0].GetFloatValue()));
			if(m_packed.a2_set)
			{
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[1].GetFloatValue()));
			}
			result = buffer->Append(")", 1);
		}
		break;
	case SVGTRANSFORM_SKEWX:
	case SVGTRANSFORM_SKEWY:
		{
			RETURN_IF_ERROR(buffer->Append(m_transform_type == SVGTRANSFORM_SKEWX ? "skewX(" : "skewY("));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[0].GetFloatValue()));
			RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[1].GetFloatValue()));
			result = buffer->Append(')');
		}
		break;
	case SVGTRANSFORM_MATRIX:
		{
			RETURN_IF_ERROR(buffer->Append("matrix("));
			for (int i = 0; i < 6; i++)
			{
				if (i > 0)
					RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[i].GetFloatValue()));
			}
			result = buffer->Append(')');
		}
		break;
	case SVGTRANSFORM_REF:
		{
			RETURN_IF_ERROR(buffer->Append("ref(svg"));
			OpString8 a;
			if (m_packed.a2_set)
			{
				for (int i = 0; i < 2; i++)
				{
					RETURN_IF_ERROR(buffer->Append(' '));
					RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(values[i].GetFloatValue()));
				}
			}
			result = buffer->Append(')');
		}
		break;
	default:
		result = OpStatus::ERR;
		break;
	}

	return result;
}

void SVGTransform::SetMatrix(const SVGMatrix& matrix)
{
	m_transform_type = SVGTRANSFORM_MATRIX;
	for (UINT32 i=0; i<6; ++i)
		values[i] = matrix[i];
}

void SVGTransform::SetMatrix(SVGNumber a1, SVGNumber a2, SVGNumber a3, SVGNumber a4, SVGNumber a5, SVGNumber a6)
{
	m_transform_type = SVGTRANSFORM_MATRIX;
	values[0] = a1;
	values[1] = a2;
	values[2] = a3;
	values[3] = a4;
	values[4] = a5;
	values[5] = a6;
	m_packed.a1_set = m_packed.a2_set = m_packed.a3_set = m_packed.a456_set = TRUE;
}

void SVGTransform::SetValuesA1(SVGNumber a1)
{
	m_packed.a1_set = TRUE;
	values[0] = a1;
}

void SVGTransform::SetValuesA12(SVGNumber a1, SVGNumber a2, BOOL a2isDefaultValue)
{
	m_packed.a1_set = TRUE;
	m_packed.a2_set = !a2isDefaultValue;
	values[0] = a1;
	values[1] = a2;
}

void SVGTransform::SetValuesA123(SVGNumber a1, SVGNumber a2, SVGNumber a3, BOOL a2anda3areDefaultValues)
{
	m_packed.a1_set = TRUE;
	m_packed.a2_set = m_packed.a3_set = !a2anda3areDefaultValues;
	values[0] = a1;
	values[1] = a2;
	values[2] = a3;
}

void SVGTransform::GetMatrix(SVGMatrix& matrix) const
{
	switch(m_transform_type)
	{
		case SVGTRANSFORM_ROTATE:
		{
			SVGNumber rotDegrees = values[0];
			SVGNumber cx = values[1];
			SVGNumber cy = values[2];
			if (cx.NotEqual(0) || cy.NotEqual(0))
			{
				SVGMatrix tempMatrix;
				matrix.LoadTranslation(-cx, -cy);
				tempMatrix.LoadRotation(rotDegrees);
				matrix.Multiply(tempMatrix);
				tempMatrix.LoadTranslation(cx, cy);
				matrix.Multiply(tempMatrix);
			}
			else
			{
				matrix.LoadRotation(rotDegrees);
			}
		}
		break;
		case SVGTRANSFORM_TRANSLATE:
		case SVGTRANSFORM_REF:
		{
			matrix.LoadTranslation(values[0], values[1]);
		}
		break;
		case SVGTRANSFORM_SCALE:
		{
			matrix.LoadScale(values[0], values[1]);
		}
		break;
		case SVGTRANSFORM_SKEWX:
		{
			matrix.LoadSkewX(values[0]);
		}
		break;
		case SVGTRANSFORM_SKEWY:
		{
			matrix.LoadSkewY(values[0]);
		}
		break;
		case SVGTRANSFORM_MATRIX:
		{
			matrix.SetValues(values[0], values[1], values[2],
							 values[3], values[4], values[5]);
		}
		break;
		default:
			matrix.SetValues(SVGNumber(1), SVGNumber(0), SVGNumber(0),
							 SVGNumber(1), SVGNumber(0), SVGNumber(0));
	}
}

void
SVGTransform::AddTransform(const SVGTransform &transform)
{
	if (m_transform_type == transform.m_transform_type)
	{
		switch(m_transform_type)
		{
		case SVGTRANSFORM_ROTATE:
			values[2] += transform.values[2];
			/* Fall-through */
		case SVGTRANSFORM_TRANSLATE:
		case SVGTRANSFORM_SCALE:
			values[1] += transform.values[1];
			/* Fall-through */
		case SVGTRANSFORM_SKEWX:
		case SVGTRANSFORM_SKEWY:
			values[0] += transform.values[0];
		}
	}
}

void
SVGTransform::SetZero()
{
	for (UINT32 i=0; i<6; ++i)
		values[i] = 0;
}

void
SVGTransform::MakeDefaultsExplicit()
{
	switch(m_transform_type)
	{
	case SVGTRANSFORM_TRANSLATE:
		if (!m_packed.a2_set)
		{
			values[1] = 0;
			m_packed.a2_set = 1;
		}
		break;
	case SVGTRANSFORM_SCALE:
		if (!m_packed.a2_set)
		{
			values[1] = values[0];
			m_packed.a2_set = 1;
		}
		break;
	case SVGTRANSFORM_ROTATE:
		OP_ASSERT((m_packed.a2_set && m_packed.a3_set) ||
				  (!m_packed.a2_set && !m_packed.a3_set));
		if (!m_packed.a2_set)
		{
			values[1] = 0;
			values[2] = 0;
			m_packed.a2_set = 1;
			m_packed.a3_set = 1;
		}
		break;
	default:
		break;
	}
}

void
SVGTransform::Interpolate(const SVGTransform &from_transform,
						  const SVGTransform &to_transform,
						  SVG_ANIMATION_INTERVAL_POSITION interval_position)
{
	// When using fixed point arithmetics, the compiler chooses the
	// long-version of the multiply operator, which leads to
	// interval_position being casted to long - which is wrong.
	SVGNumber svg_num_position = SVGNumber(interval_position);

	switch(from_transform.GetTransformType())
	{
		case SVGTRANSFORM_ROTATE:
			values[2] = from_transform.values[2] + (to_transform.values[2] - from_transform.values[2]) * svg_num_position;
			m_packed.a3_set = 1;
			/* Fall-through */
		case SVGTRANSFORM_SCALE:
		case SVGTRANSFORM_TRANSLATE:
			values[1] = from_transform.values[1] + (to_transform.values[1] - from_transform.values[1]) * svg_num_position;
			m_packed.a2_set = 1;
			/* Fall-through */
		case SVGTRANSFORM_SKEWX:
		case SVGTRANSFORM_SKEWY:
			values[0] = from_transform.values[0] + (to_transform.values[0] - from_transform.values[0]) * svg_num_position;
			m_packed.a1_set = 1;
			break;
		case SVGTRANSFORM_MATRIX:
			OP_ASSERT(!"Not possible to interpolate matrices.");
			break;
	}
}

/* virtual */ BOOL
SVGTransform::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_TRANSFORM;
	animation_value.reference.svg_transform = this;
	return TRUE;
}

float
SVGTransform::Distance(const SVGTransform &transform) const
{
	if (GetTransformType() != transform.GetTransformType())
		return 0.0;

	switch(GetTransformType())
	{
	case SVGTRANSFORM_TRANSLATE:
		// ||VaVb|| = dist(v_a0, v_b0) = sqrt((v_a0 [0] - v_b0 [0])^2 + (v_a0 [1] - v_b0 [1])^2))
		// which, in our internal representation, is equivalent to the distance for 'scale'.
	case SVGTRANSFORM_SCALE:
	{
		// ||VaVb|| = sqrt((scaleXa - scaleXb)^2 + (scaleYa - scaleYb)^2)
		SVGNumber dx = values[0] - transform.values[0];
		SVGNumber dy = values[1] - transform.values[1];
		return (dx * dx + dy * dy).sqrt().GetFloatValue();
	}
	case SVGTRANSFORM_ROTATE:
	case SVGTRANSFORM_SKEWX:
	case SVGTRANSFORM_SKEWY:
	{
		// ||VaVb|| = sqrt((angleA - angleB)^2) = abs(angleA - angleB)
		SVGNumber d = values[0] - transform.values[0];
		return d.abs().GetFloatValue();
	}
	case SVGTRANSFORM_MATRIX:
		/* No distance defined */
		break;
	}
	return 0.0f;
}


#endif // SVG_SUPPORT
