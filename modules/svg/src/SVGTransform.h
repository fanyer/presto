/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGTRANSFORM_H
#define SVGTRANSFORM_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGMatrix.h"

#include "modules/svg/src/animation/svganimationinterval.h"

/**
 * Transformation element
 * A class representating one transformation element. An element can
 * be a rotation, a rotation around a point, a scaling, a translation
 * or some sort of skewing, in x- or y-axis.
 *
 * These can be concatenated and interpolated. Finally can you take
 * the matrix representating the transformation.
 */
class SVGTransform : public SVGObject
{
public:
#ifdef SELFTEST
	BOOL operator==(const SVGTransform& other) const;
#endif // SELFTEST

	SVGTransform() : SVGObject(SVGOBJECT_TRANSFORM), m_transform_type(SVGTRANSFORM_UNKNOWN), m_packed_init(0) {} ;
	SVGTransform(SVGTransformType type);

	virtual ~SVGTransform() {}

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	/**
	 * Add two transformations together. From and to must NOT be of
	 * the same type.
	 */
	OP_STATUS Concat(const SVGTransform& from,
					 const SVGTransform& to);


	/**
	 * Add a translation and a rotation to a transformation
	 */
	OP_STATUS Concat(const SVGTransform& transformation,
					 const SVGNumberPair& translation,
					 const SVGNumber rot);

	void GetMatrix(SVGMatrix& matrix) const;
	void SetMatrix(const SVGMatrix& matrix);

	void SetTransformType(SVGTransformType type) { m_transform_type = type; }
	void SetMatrix(SVGNumber a1, SVGNumber a2, SVGNumber a3, SVGNumber a4, SVGNumber a5, SVGNumber a6);
	void SetValuesA1(SVGNumber a1);
	void SetValuesA12(SVGNumber a1, SVGNumber a2, BOOL a2isDefaultValue);
	void SetValuesA123(SVGNumber a1, SVGNumber a2, SVGNumber a3, BOOL a2anda3areDefaultValues);

	SVGTransformType GetTransformType() const { return m_transform_type; }

	void Copy(const SVGTransform& transform);
	void SetZero();

	/**
	 * Call MakeDefaultsExplicit before using AddTransform on both
	 * transforms. This function assumes that default values are set.
	 */
	void AddTransform(const SVGTransform &transform);
	void MakeDefaultsExplicit();
	void Interpolate(const SVGTransform &from_transform,
					 const SVGTransform &to_transform,
					 SVG_ANIMATION_INTERVAL_POSITION interval_position);
	float Distance(const SVGTransform &transform) const;

private:
	SVGTransform(const SVGTransform& X);
	void operator=(const SVGTransform& X);

	SVGTransformType m_transform_type;
	SVGNumber values[6];

	// These classes knows the internal structure of SVGTransform
	friend class SVGDOMTransformImpl;
	friend class SVGDOMMatrixTransformImpl;

	union
	{
		struct
		{
			unsigned int a1_set:1;
			unsigned int a2_set:1;
			unsigned int a3_set:1;
			unsigned int a456_set:1;
		} m_packed;
		unsigned int m_packed_init;
	};
};

#ifdef SELFTEST
BOOL operator==(const SVGTransform& t1, const SVGTransform& t2);
#endif // SELFTEST

#endif // SVG_SUPPORT
#endif // SVGTRANSFORM_H
