/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGATRANSFORM_H
#define VEGATRANSFORM_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"

enum VEGATransformClass
{
	// These are the 'basic' features
	VEGA_XFRMCLS_TRANSLATION		= 0x01,
	VEGA_XFRMCLS_SCALE				= 0x02,
	VEGA_XFRMCLS_SKEW				= 0x04,

	// These are 'additional' features that further specialize the
	// above 'basic' features
	VEGA_XFRMCLS_INT_TRANSLATION	= 0x08,
	VEGA_XFRMCLS_POS_SCALE			= 0x10
};

/** A transform is used to transform an x and a y value. When a transform is
  * created its values are undefined.
  * The transform is row based (row major). */
class VEGATransform
{
public:
	/** Load the identity matrix. */
	void loadIdentity();

	/** Load a scale matrix. The x and y value will be multiplied by the
	  * factors.
	  * @param xscale the factor to multiply the x coordinate with.
	  * @param yscale the factor to multiply the y coordinate with. */
	void loadScale(VEGA_FIX xscale, VEGA_FIX yscale);

	/** Load a translate matrix. The translation values will be added to
	  * the coordinate.
	  * @param xtrans the translation of the x coordinate.
	  * @param ytrans the translation of the y coordinate. */
	void loadTranslate(VEGA_FIX xtrans, VEGA_FIX ytrans);

	/** Load a rotation matrix. The coordinates will be rotated around
	  * the origin counter-clockwise by the amount specified.
	  * @param angle the angle to rotate be specified in degrees. */
	void loadRotate(VEGA_FIX angle);

	/** Multiply with a different transform. The transform you multiply with
	  * (not this transform but "the other" will be applied first when
	  * transforming using this transform.
	  * @param other the transform to multiply with. */
	void multiply(const VEGATransform &other);

	/** Invert the transform.
	  * @returns false if no inverse exists, true otherwise. */
	bool invert();

	/** Apply this transform to a coordinate.
	  * @param x the x coord.
	  * @param y the y coord. */
	void apply(VEGA_FIX &x, VEGA_FIX &y) const;

	/** Apply this transform to a vector.
	  * @param x the x coord.
	  * @param y the y coord. */
	void applyVector(VEGA_FIX &x, VEGA_FIX &y) const;

	/** Copy the values of a transform to this transform.
	  * @param other the transform to copy. */
	void copy(const VEGATransform &other);

	/** [] operator used to access (read-write) the values of the matrix. */
	VEGA_FIX &operator[](int index){return matrix[index];}

	/** [] operator used to access (read-only) the values of the matrix. */
	VEGA_FIX operator[](int index) const {return matrix[index];}

	/** Perform a classification of the transform, using the classes
	 * defined by VEGATransformClass.
	 *  @returns A bitmask with the
	 *  applicable classes set. */
	unsigned classify() const;

	/** Checks if the transform is composed of an integral translation only. */
	bool isIntegralTranslation() const
	{
		return classify() == (VEGA_XFRMCLS_TRANSLATION |
							  VEGA_XFRMCLS_INT_TRANSLATION |
							  VEGA_XFRMCLS_POS_SCALE);
	}

	/** Checks if the transform is a rotation n*(pi/2) or a x/y-flip, has
	 * x/y-scale = 1 and if any translation can be represented by integers. */
	bool isAlignedAndNonscaled() const;

	/** Checks if the transform consists of scaling and translation only. */
	bool isScaleAndTranslateOnly() const;

private:
	VEGA_FIX matrix[6];
};

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
/** A 3-dimensional transform used by the 3d device. This can be used to build
 * world transforms to apply to 3d rendering. */
class VEGATransform3d
{
public:
	/** Load the identity matrix. */
	void loadIdentity();

	/** Load a scale matrix. The x and y value will be multiplied by the
	  * factors.
	  * @param xscale the factor to multiply the x coordinate with.
	  * @param yscale the factor to multiply the y coordinate with. */
	void loadScale(VEGA_FIX xscale, VEGA_FIX yscale, VEGA_FIX zscale);

	/** Load a translate matrix. The translation values will be added to
	  * the coordinate.
	  * @param xtrans the translation of the x coordinate.
	  * @param ytrans the translation of the y coordinate. */
	void loadTranslate(VEGA_FIX xtrans, VEGA_FIX ytrans, VEGA_FIX ztrans);

	/** Load a rotation matrix. The coordinates will be rotated around
	  * the origin counter-clockwise by the amount specified.
	  * @param angle the angle to rotate be specified in degrees. */
	void loadRotateX(VEGA_FIX angle);
	void loadRotateY(VEGA_FIX angle);
	void loadRotateZ(VEGA_FIX angle);

	/** Multiply with a different transform. The transform you multiply with
	  * (not this transform but "the other" will be applied first when
	  * transforming using this transform.
	  * @param other the transform to multiply with. */
	void multiply(const VEGATransform3d &other);

	/** Copy the values of a transform to this transform and optionally transpose it.
	  * @param other the transform to copy. 
	  * @param transpose set to true to transpose the copy. */
	void copy(const VEGATransform3d& other, bool transpose = false);

	/** Copy a 2d transform to this 3d transform.
	 * @param 2d transform to copy. */
	void copy(const VEGATransform& other);

	/** [] operator used to access (read-write) the values of the matrix. */
	VEGA_FIX &operator[](int index){return matrix[index];}

	/** [] operator used to access (read-only) the values of the matrix. */
	const VEGA_FIX &operator[](int index) const {return matrix[index];}
private:
	VEGA_FIX matrix[16];
};
#endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT

#endif // VEGA_SUPPORT
#endif // VEGATRANSFORM_H
