/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGManagerImpl.h"

#include "modules/libvega/vegatransform.h"

#define SVG_ROTATION_SNAP_TO_ZERO

BOOL SVGMatrix::IsIdentity() const
{
	return op_memcmp(values, g_svg_identity_matrix.values, 6*sizeof(SVGNumber)) == 0; 
}

void SVGMatrix::LoadIdentity()
{
	// If this shows up in profiles, there is a potential
	// way to make it faster.
	//  * Make a static identity matrix (SVGNumber array)
	//    and do op_memcpy from that one.
	values[0] = 1;
	values[1] = 0;
	values[2] = 0;
	values[3] = 1;
	values[4] = 0;
	values[5] = 0;
}

void SVGMatrix::LoadTranslation(SVGNumber x, SVGNumber y)
{
	values[0] = 1;
	values[1] = 0;
	values[2] = 0;
	values[3] = 1;
	values[4] = x;
	values[5] = y;
}

void SVGMatrix::LoadScale(SVGNumber x, SVGNumber y)
{
	values[0] = x;
	values[1] = 0;
	values[2] = 0;
	values[3] = y;
	values[4] = 0;
	values[5] = 0;
}

void SVGMatrix::LoadRotation(SVGNumber deg)
{
	deg = deg*SVGNumber::pi()/180;
	SVGNumber s(deg.sin());
	SVGNumber c(deg.cos());

#ifdef SVG_ROTATION_SNAP_TO_ZERO
	if(c.Close(0))
		c = 0;
	if(s.Close(0))
		s = 0;
#endif // SVG_ROTATION_SNAP_TO_ZERO

	values[0] = c;
	values[1] = s;
	values[2] = -s;
	values[3] = c;
	values[4] = 0;
	values[5] = 0;
}

void SVGMatrix::LoadSkewX(SVGNumber deg)
{
	SVGNumber t((deg * SVGNumber::pi() / 180).tan());
	values[0] = 1;
	values[1] = 0;
	values[2] = t;
	values[3] = 1;
	values[4] = 0;
	values[5] = 0;
}

void SVGMatrix::LoadSkewY(SVGNumber deg)
{
	SVGNumber t((deg * SVGNumber::pi() / 180).tan());
	values[0] = 1;
	values[1] = t;
	values[2] = 0;
	values[3] = 1;
	values[4] = 0;
	values[5] = 0;
}

void SVGMatrix::Copy(const SVGMatrix& matrix)
{
	OP_ASSERT(sizeof(values) > 8); // It's an array, not a pointer
	op_memcpy(values, matrix.values, sizeof(values));
}

void SVGMatrix::CopyToVEGATransform(VEGATransform& vega_xfrm) const
{
	vega_xfrm[0] = values[0].GetVegaNumber(); // scale x
	vega_xfrm[1] = values[2].GetVegaNumber(); // skew x
	vega_xfrm[2] = values[4].GetVegaNumber(); // translate x
	vega_xfrm[3] = values[1].GetVegaNumber(); // skew y
	vega_xfrm[4] = values[3].GetVegaNumber(); // scale y
	vega_xfrm[5] = values[5].GetVegaNumber(); // translate y
}

void SVGMatrix::MultScale(SVGNumber sx, SVGNumber sy)
{
	// B = S(sx,sy) [sx 0 0 sy 0 0]
	// A = this
	// A = B*A
	values[0] *= sx;
	values[1] *= sy;
	values[2] *= sx;
	values[3] *= sy;
	values[4] *= sx;
	values[5] *= sy;
}

void SVGMatrix::Multiply(const SVGMatrix& other)
{
#ifdef SVG_MATRIX_IDENTITY_CHECKS
	if(other.IsIdentity())
	{
		return;
	}
	else if(IsIdentity())
	{
		Copy(other);
		return;
	}
#endif // SVG_MATRIX_IDENTITY_CHECKS

	SVGNumber a = values[0];
	SVGNumber b = values[1];

	values[0] = a * other.values[0] + b * other.values[2];
	values[1] = a * other.values[1] + b * other.values[3];

	a = values[2];
	b = values[3];

	values[2] = a * other.values[0] + b * other.values[2];
	values[3] = a * other.values[1] + b * other.values[3];

	a = values[4];
	b = values[5];

	values[4] = a * other.values[0] + b * other.values[2] + other.values[4];
	values[5] = a * other.values[1] + b * other.values[3] + other.values[5];
}

void SVGMatrix::PostMultTranslation(SVGNumber tx, SVGNumber ty)
{
	// B = T(tx,ty) [1 0 0 1 tx ty]
	// A = this
	// A = A*B
	values[4] += tx * values[0] + ty * values[2];
	values[5] += tx * values[1] + ty * values[3];
}

void SVGMatrix::PostMultScale(SVGNumber sx, SVGNumber sy)
{
	// B = S(sx,sy) [sx 0 0 sy 0 0]
	// A = this
	// A = A*B
	values[0] *= sx;
	values[1] *= sx;

	values[2] *= sy;
	values[3] *= sy;
}

void SVGMatrix::PostMultiply(const SVGMatrix& other)
{
#ifdef SVG_MATRIX_IDENTITY_CHECKS
	if(other.IsIdentity())
		return;

	if(IsIdentity())
	{
		Copy(other);
		return;
	}
#endif // SVG_MATRIX_IDENTITY_CHECKS

	const SVGNumber* right = other.values;

	UnintializedSVGNumber left[4];
	left[0] = values[0];
	left[1] = values[1];
	left[2] = values[2];
	left[3] = values[3];

	values[0] = right[0] * left[0] + right[1] * left[2];
	values[1] = right[0] * left[1] + right[1] * left[3];

	values[2] = right[2] * left[0] + right[3] * left[2];
	values[3] = right[2] * left[1] + right[3] * left[3];

	values[4] = right[4] * left[0] + right[5] * left[2] + values[4];
	values[5] = right[4] * left[1] + right[5] * left[3] + values[5];
}

void SVGMatrix::SetValues(const SVGNumber a, const SVGNumber b, const SVGNumber c, const SVGNumber d, const SVGNumber e, const SVGNumber f)
{
	values[0] = a;
	values[1] = b;
	values[2] = c;
	values[3] = d;
	values[4] = e;
	values[5] = f;
}

SVGNumber SVGMatrix::GetExpansionFactor() const
{
	return GetExpansionFactorSquared().sqrt();
}

SVGNumber SVGMatrix::GetExpansionFactorSquared() const
{
	SVGNumber result = (values[0] * values[3] - values[1] * values[2]).abs();
	// Make eg. rotate(45) return a correct expansion factor of 1
	if (result.Close(1))
	  result = 1;
	return result;
}

SVGNumberPair SVGMatrix::ApplyToCoordinate(const SVGNumberPair& c) const
{
	SVGNumberPair res;
    res.x = (c.x * values[0]) + (c.y * values[2]) + values[4];
    res.y = (c.x * values[1]) + (c.y * values[3]) + values[5];
    return res;
}

SVGBoundingBox SVGMatrix::ApplyToNonEmptyBoundingBox(const SVGBoundingBox& bbox) const
{
	OP_ASSERT(!bbox.IsEmpty());

	SVGBoundingBox res;
	SVGNumberPair tl = ApplyToCoordinate(bbox.minx, bbox.miny);
	SVGNumberPair bl = ApplyToCoordinate(bbox.minx, bbox.maxy);
	SVGNumberPair tr = ApplyToCoordinate(bbox.maxx, bbox.miny);
	SVGNumberPair br = ApplyToCoordinate(bbox.maxx, bbox.maxy);
	res.minx = SVGNumber::min_of(tl.x, SVGNumber::min_of(bl.x, SVGNumber::min_of(tr.x, br.x)));
	res.miny = SVGNumber::min_of(tl.y, SVGNumber::min_of(bl.y, SVGNumber::min_of(tr.y, br.y)));
	res.maxx = SVGNumber::max_of(tl.x, SVGNumber::max_of(bl.x, SVGNumber::max_of(tr.x, br.x)));
	res.maxy = SVGNumber::max_of(tl.y, SVGNumber::max_of(bl.y, SVGNumber::max_of(tr.y, br.y)));
	return res;
}

SVGRect SVGMatrix::ApplyToRect(const SVGRect& rect) const
{
	SVGRect res;
	SVGBoundingBox bbox;

	/* Use the boundingbox function to transform the rect */
	bbox.minx = rect.x;
	bbox.miny = rect.y;
	bbox.maxx = bbox.minx + rect.width;
	bbox.maxy = bbox.miny + rect.height;

	bbox = ApplyToBoundingBox(bbox);

	res.x = bbox.minx;
	res.y = bbox.miny;
	res.width = bbox.maxx - bbox.minx;
	res.height = bbox.maxy - bbox.miny;

	return res;
}

BOOL SVGMatrix::Invert()
{
	SVGNumber det;
	det = values[0] * values[3];
	det -= values[1] * values[2];
	if (det.Equal(0)) // FIXME: Close()-comparison here?
		return FALSE;

#ifdef SVG_MATRIX_IDENTITY_CHECKS
	if(IsIdentity())
		return TRUE; // Inversion of identity is identity
#endif // SVG_MATRIX_IDENTITY_CHECKS

	// FIXME/NOTE: Using the reciprocal of 'det' would probably be faster in most cases.

	UnintializedSVGNumber oldmat[6];
	oldmat[0] = values[0];
	oldmat[1] = values[1];
	oldmat[2] = values[2];
	oldmat[3] = values[3];
	oldmat[4] = values[4];
	oldmat[5] = values[5];
	values[0] = oldmat[3] / det;
	values[1] = -oldmat[1] / det;
	values[2] = -oldmat[2] / det;
	values[3] = oldmat[0] / det;
	values[4] = ((oldmat[2] * oldmat[5]) - (oldmat[3] * oldmat[4])) / det;
	values[5] = -((oldmat[0] * oldmat[5]) - (oldmat[1] * oldmat[4])) / det;
	return TRUE;
}

/* virtual */ BOOL
SVGMatrix::operator==(const SVGMatrix& other) const
{
	for(int i = 0; i < 6; i++)
		if(values[i].NotEqual(other.values[i]))
			return FALSE;
	return TRUE;
}

BOOL
SVGMatrix::IsAlignedAndNonscaled() const
{
	if (values[3].Equal(0) && values[0].Equal(0))
	{
		if((values[2].Equal(-1) &&
			values[1].Equal(1)) ||
			   (values[2].Equal(1) && values[1].Equal(-1)))
		{
			return( values[4].floor().Equal(values[4].ceil()) && 
					values[5].floor().Equal(values[5].ceil())); 
		}
	}
	else if (values[2].Equal(0) && values[1].Equal(0))
	{
		if((values[3].Equal(-1) && 
			values[0].Equal(-1)) ||
			   (values[3].Equal(1) &&
				values[0].Equal(1)))
		{
			return( values[4].floor().Equal(values[4].ceil()) &&
					values[5].floor().Equal(values[5].ceil()));
		}
	}

	return FALSE;
}

BOOL
SVGMatrix::IsScaledTranslation() const
{
	return
		// No (or almost no) skew factors
		values[1].Close(0) && values[2].Close(0) &&
		// Positive scale
		values[0] >= 0 && values[3] >= 0;
}

/* virtual */ SVGObject *
SVGMatrixObject::Clone() const
{
	SVGMatrixObject* matrix_object = OP_NEW(SVGMatrixObject, ());
	if (matrix_object)
	{
		matrix_object->CopyFlags(*this);
		matrix_object->matrix.Copy(matrix);
	}
	return matrix_object;
}

/* virtual */ BOOL
SVGMatrixObject::IsEqual(const SVGObject& v1) const
{
	if (v1.Type() == SVGOBJECT_MATRIX)
	{
		return (this->matrix == static_cast<const SVGMatrixObject&>(v1).matrix);
	}
	return FALSE;
}

/* virtual */ BOOL
SVGMatrixObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_MATRIX;
	animation_value.reference.svg_matrix = &matrix;
	return TRUE;
}

#endif // SVG_SUPPORT
