/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_MATRIX_H
#define SVG_MATRIX_H

#include "modules/svg/svg_number.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGNumberPair.h"

class SVGBoundingBox;

class VEGATransform;

#define SVG_MATRIX_IDENTITY_CHECKS

/**
 * The SVGMatrix class describes an affine transformation matrix.
 * The matrix is 3x3 and looks like:
 * [a c e]
 * [b d f]
 * [0 0 1]
 *
 * The values of [a b c d e f] are stored in this class.
 */
class SVGMatrix
{
public:
	SVGMatrix() { LoadIdentity(); }

	/**
	 * Set this transform to the identity transform, [1 0 0 0 1 0]
	 */
	void LoadIdentity();

	/**
	 * Set this transform to translate to (x,y), [1 0 x 0 1 y]
	 * @param x The x coordinate to translate to
	 * @param y The y coordinate to translate to
	 */
	void LoadTranslation(SVGNumber x, SVGNumber y);

	/**
	 * Set this transform to scale, scale can be different for the x- and y-axis.
	 * @param x Scale for the x-axis
	 * @param y Scale for the y-axis
	 */
	void LoadScale(SVGNumber x, SVGNumber y);

	/**
	 * Set this transform to rotate by deg degrees
	 * @param deg Degrees
	 */
	void LoadRotation(SVGNumber deg);

	/**
	 * Set this transform to rotate by deg degrees around the point (x, y)
	 * @param deg Degrees
	 */
	void LoadRotation(SVGNumber deg, SVGNumber x, SVGNumber y);

	/**
	 * Set this transform to skew the x-axis by deg degrees.
	 * @param deg Degrees
	 */
	void LoadSkewX(SVGNumber deg);

	/**
	 * Set this transform to skew the y-axis by deg degrees.
	 * @param deg Degrees
	 */
	void LoadSkewY(SVGNumber deg);

	/**
	 * Multiply by the translation matrix T(tx,ty).
	 *
	 * Same as:
	 * SVGMatrix m;
	 * m.LoadTranslation(tx, ty);
	 * this->Multiply(&m);
	 */
	void MultTranslation(SVGNumber tx, SVGNumber ty)
	{
		// B = T(tx,ty) [1 0 0 1 tx ty]
		// A = this
		// A = B*A
		values[4] += tx;
		values[5] += ty;
	}

	/**
	 * Multiply by the scaling matrix S(x,y).
	 *
	 * Same as:
	 * SVGMatrix m;
	 * m.LoadScale(x, y);
	 * this->Multiply(&m);
	 */
	void MultScale(SVGNumber x, SVGNumber y);

	/**
	 * Multiplicate this matrix with another from the left.
	 * this = A
	 * other = B
	 * A = BA
	 */
	void Multiply(const SVGMatrix& other);

	/**
	 * Post-Multiply by the translation matrix T(x,y).
	 *
	 * Same as:
	 * SVGMatrix m;
	 * m.LoadTranslation(x, y);
	 * this->PostMultiply(&m);
	 */
	void PostMultTranslation(SVGNumber x, SVGNumber y);

	/**
	 * Post-Multiply by the scaling matrix S(x,y).
	 *
	 * Same as:
	 * SVGMatrix m;
	 * m.LoadScale(x, y);
	 * this->PostMultiply(&m);
	 */
	void PostMultScale(SVGNumber x, SVGNumber y);

	/**
	 * Multiplicate this matrix with another from the right.
	 * this = A
	 * other = B
	 * A = AB
	 */
	void PostMultiply(const SVGMatrix &other);

	void SetValues(const SVGNumber a, const SVGNumber b, const SVGNumber c, const SVGNumber d, const SVGNumber e, const SVGNumber f);

	/**
	 * Copy another matrix
	 */
	void Copy(const SVGMatrix& matrix);

	/**
	 * Copy to a VEGATransform
	 */
	void CopyToVEGATransform(VEGATransform& vega_xfrm) const;

	SVGNumber operator[](int idx) const { OP_ASSERT(idx >= 0 && idx < 6); return values[idx]; }
	SVGNumber& operator[](int idx) { OP_ASSERT(idx >= 0 && idx < 6); return values[idx]; }

	/**
	 * Apply the transform to a coordinate.
	 * @param c The coordinate to transform (most often in user space)
	 * @return The transformed coordinate (most often in viewport space)
	 */	
	SVGNumberPair ApplyToCoordinate(const SVGNumberPair& c) const;

	/**
	 * Apply the transform to a coordinate.
	 * @param x The x coordinate to transform (most often in user space)
	 * @param y The y coordinate to transform (most often in user space)
	 * @return The transformed coordinate (most often in viewport space)
	 */	
	SVGNumberPair ApplyToCoordinate(SVGNumber x, SVGNumber y) const { return ApplyToCoordinate(SVGNumberPair(x,y)); }
	
	void ApplyToCoordinate(SVGNumber* x, SVGNumber* y) const { SVGNumberPair t = ApplyToCoordinate(SVGNumberPair(*x,*y)); *x = t.x; *y = t.y; }

	/**
	 * Applies the transform to the boundingbox and returns the new boundingbox.
	 * This method doesn't check if the boundingbox is empty, that's the responsibility
	 * of the caller.
	 * @param bbox The boundingbox (in user space)
	 * @return The transformed boundingbox
	 */
	SVGBoundingBox ApplyToNonEmptyBoundingBox(const SVGBoundingBox& bbox) const;

	/**
	 * Applies the transform to the boundingbox and returns the new boundingbox.
	 * @param bbox The boundingbox (in user space)
	 * @return The transformed boundingbox
	 */
	SVGBoundingBox ApplyToBoundingBox(const SVGBoundingBox& bbox) const {
		if (bbox.IsEmpty())
			return bbox;
		else
			return ApplyToNonEmptyBoundingBox(bbox);
	}

	/**
	 * Applies the transform to the rectangle and returns the new rectangle.
	 * @param rect The rectangle (in user space)
	 * @return The transformed rect
	 */
	SVGRect ApplyToRect(const SVGRect& rect) const;

	/**
	 * Gets the expansion factor, i.e. the square root of the factor
	 * by which the affine transform affects area. In an affine transform
	 * composed of scaling, rotation, shearing, and translation, returns
	 * the amount of scaling.
	 *
	 * This is useful for setting linewidths for stroke operations since
	 * some implementations scale the linewidth value according to the currently
	 * used transformation matrix. 
	 *
	 * @return the expansion factor.
	 */
	SVGNumber GetExpansionFactor() const;

	/**
	 * Gets the expansion factor squared.
	 *
	 * @return the expansion factor^2
	 */
	SVGNumber GetExpansionFactorSquared() const;

	BOOL operator==(const SVGMatrix& other) const;

	/**
	 * Invert the matrix.
	 *
	 * @return TRUE if the matrix was invertable, otherwise FALSE (leaves the matrix unmodified)
	 */
	BOOL Invert();

	BOOL IsIdentity() const;

	/** 
	 * Checks if the transform is a rotation n*(pi/2), has x/y-scale = 1
	 * and if any translation can be represented by integers. 
	 */
	BOOL IsAlignedAndNonscaled() const;

	/**
	 * Check if the transform is a translation and (positive) scale
	 */
	BOOL IsScaledTranslation() const;

private:
	// The values will be explicitly set in the constructor so
	// we want to avoid setting the values twice.
	struct UnintializedSVGNumber : public SVGNumber
	{
		UnintializedSVGNumber() : SVGNumber(SVGNumber::UNINITIALIZED_SVGNUMBER) {}
		void operator=(int value) { SVGNumber::operator=(value); }
		void operator=(SVGNumber value) { SVGNumber::operator=(value); }
	};

	UnintializedSVGNumber values[6];
};

class SVGMatrixObject : public SVGObject
{
public:
	SVGMatrixObject() : SVGObject(SVGOBJECT_MATRIX) {}

	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const { OP_ASSERT(0); return OpStatus::ERR; }
	virtual BOOL IsEqual(const SVGObject& other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	SVGMatrix matrix;
};

#endif // SVG_MATRIX_H
