/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegatransform.h"

void VEGATransform::apply(VEGA_FIX &x, VEGA_FIX &y) const{
	VEGA_FIX tx, ty;
	tx = VEGA_FIXMUL(x,matrix[0]) + VEGA_FIXMUL(y,matrix[1]) + matrix[2];
	ty = VEGA_FIXMUL(x,matrix[3]) + VEGA_FIXMUL(y,matrix[4]) + matrix[5];
	x = tx;
	y = ty;
}

void VEGATransform::applyVector(VEGA_FIX &x, VEGA_FIX &y) const
{
	VEGA_FIX tx = VEGA_FIXMUL(x, matrix[0]) + VEGA_FIXMUL(y, matrix[1]);
	VEGA_FIX ty = VEGA_FIXMUL(x, matrix[3]) + VEGA_FIXMUL(y, matrix[4]);
	x = tx;
	y = ty;
}

/** The matrix you multiply with (other) will be applied before this matrib when transforming a vertex */
void VEGATransform::multiply(const VEGATransform &other){
	VEGA_FIX newmat[6];
	newmat[0] = VEGA_FIXMUL(other.matrix[0], matrix[0]) + VEGA_FIXMUL(other.matrix[3],matrix[1]);
	newmat[1] = VEGA_FIXMUL(other.matrix[1], matrix[0]) + VEGA_FIXMUL(other.matrix[4],matrix[1]);
	newmat[2] = VEGA_FIXMUL(other.matrix[2], matrix[0]) + VEGA_FIXMUL(other.matrix[5],matrix[1]) + matrix[2];
	newmat[3] = VEGA_FIXMUL(other.matrix[0], matrix[3]) + VEGA_FIXMUL(other.matrix[3],matrix[4]);
	newmat[4] = VEGA_FIXMUL(other.matrix[1], matrix[3]) + VEGA_FIXMUL(other.matrix[4],matrix[4]);
	newmat[5] = VEGA_FIXMUL(other.matrix[2], matrix[3]) + VEGA_FIXMUL(other.matrix[5],matrix[4]) + matrix[5];
	matrix[0] = newmat[0];
	matrix[1] = newmat[1];
	matrix[2] = newmat[2];
	matrix[3] = newmat[3];
	matrix[4] = newmat[4];
	matrix[5] = newmat[5];
}

void VEGATransform::loadIdentity(){
	matrix[0] = VEGA_INTTOFIX(1);
	matrix[1] = VEGA_INTTOFIX(0);
	matrix[2] = VEGA_INTTOFIX(0);
	matrix[3] = VEGA_INTTOFIX(0);
	matrix[4] = VEGA_INTTOFIX(1);
	matrix[5] = VEGA_INTTOFIX(0);
}

void VEGATransform::loadScale(VEGA_FIX xscale, VEGA_FIX yscale){
	matrix[0] = xscale;
	matrix[1] = VEGA_INTTOFIX(0);
	matrix[2] = VEGA_INTTOFIX(0);
	matrix[3] = VEGA_INTTOFIX(0);
	matrix[4] = yscale;
	matrix[5] = VEGA_INTTOFIX(0);
}

void VEGATransform::loadTranslate(VEGA_FIX xtrans, VEGA_FIX ytrans){
	matrix[0] = VEGA_INTTOFIX(1);
	matrix[1] = VEGA_INTTOFIX(0);
	matrix[2] = xtrans;
	matrix[3] = VEGA_INTTOFIX(0);
	matrix[4] = VEGA_INTTOFIX(1);
	matrix[5] = ytrans;
}

void VEGATransform::loadRotate(VEGA_FIX angle){
	matrix[0] = VEGA_FIXCOS(angle);
	matrix[1] = -VEGA_FIXSIN(angle);
	matrix[2] = VEGA_INTTOFIX(0);	
	matrix[3] = VEGA_FIXSIN(angle);
	matrix[4] = VEGA_FIXCOS(angle);
	matrix[5] = VEGA_INTTOFIX(0);	
}

bool VEGATransform::invert()
{
	VEGA_DBLFIX det;
	det = VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(matrix[0], matrix[4]), VEGA_FIXMUL_DBL(matrix[3], matrix[1]));
	if (VEGA_DBLFIXEQ(det, VEGA_INTTODBLFIX(0)))
		return false;

	VEGA_FIX oldmat[6];
	oldmat[0] = matrix[0];
	oldmat[1] = matrix[1];
	oldmat[2] = matrix[2];
	oldmat[3] = matrix[3];
	oldmat[4] = matrix[4];
	oldmat[5] = matrix[5];

	matrix[0] =  VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(oldmat[4]), det));
	matrix[1] = -VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(oldmat[1]), det));
	matrix[2] =  VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(oldmat[1], oldmat[5]),
																VEGA_FIXMUL_DBL(oldmat[4], oldmat[2])), det));
	matrix[3] = -VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(oldmat[3]), det));
	matrix[4] =  VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(oldmat[0]), det));
	matrix[5] = -VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(oldmat[0], oldmat[5]),
																VEGA_FIXMUL_DBL(oldmat[3], oldmat[2])), det));
	return true;
}

void VEGATransform::copy(const VEGATransform &other)
{
	matrix[0] = other.matrix[0];
	matrix[1] = other.matrix[1];
	matrix[2] = other.matrix[2];
	matrix[3] = other.matrix[3];
	matrix[4] = other.matrix[4];
	matrix[5] = other.matrix[5];
}

bool VEGATransform::isAlignedAndNonscaled() const
{
	if (VEGA_EQ(matrix[4], 0) && VEGA_EQ(matrix[0], 0))
	{
		if (!(VEGA_EQ(matrix[1], VEGA_INTTOFIX(-1)) && VEGA_EQ(matrix[3], VEGA_INTTOFIX(1)) ||
			  VEGA_EQ(matrix[1], VEGA_INTTOFIX(1)) && VEGA_EQ(matrix[3], VEGA_INTTOFIX(-1))))
			return false;
	}
	else if (VEGA_EQ(matrix[1], 0) && VEGA_EQ(matrix[3], 0))
	{
		if (!(VEGA_EQ(VEGA_ABS(matrix[0]), VEGA_INTTOFIX(1)) && VEGA_EQ(VEGA_ABS(matrix[4]), VEGA_INTTOFIX(1))))
			return false;
	}
	else
	{
		return false;
	}

	return
		VEGA_EQ(VEGA_FLOOR(matrix[2]), VEGA_CEIL(matrix[2])) &&
		VEGA_EQ(VEGA_FLOOR(matrix[5]), VEGA_CEIL(matrix[5]));
}

bool VEGATransform::isScaleAndTranslateOnly() const
{
	return VEGA_EQ(matrix[1], 0) && VEGA_EQ(matrix[3], 0);
}

unsigned VEGATransform::classify() const
{
	unsigned class_bits = VEGA_XFRMCLS_TRANSLATION | VEGA_XFRMCLS_SCALE | VEGA_XFRMCLS_SKEW;

	if (VEGA_EQ(matrix[1], 0) && VEGA_EQ(matrix[3], 0))
	{
		class_bits &= ~VEGA_XFRMCLS_SKEW; // No skew

		if (matrix[0] > 0 && matrix[4] > 0)
		{
			class_bits |= VEGA_XFRMCLS_POS_SCALE; // Positive scaling

			if (VEGA_EQ(matrix[0], VEGA_INTTOFIX(1)) && VEGA_EQ(matrix[4], VEGA_INTTOFIX(1)))
				class_bits &= ~VEGA_XFRMCLS_SCALE; // No scale
		}
	}

	if (VEGA_EQ(matrix[2], VEGA_TRUNCFIX(matrix[2])) &&
		VEGA_EQ(matrix[5], VEGA_TRUNCFIX(matrix[5])))
		class_bits |= VEGA_XFRMCLS_INT_TRANSLATION; // Integer translation

	// If we were ever interrested in detecting (0, 0) translation (or
	// identity), we would add a test for that here...

	return class_bits;
}

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)
void VEGATransform3d::loadIdentity()
{
	matrix[0] = VEGA_INTTOFIX(1);
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = 0;
	matrix[4] = 0;
	matrix[5] = VEGA_INTTOFIX(1);
	matrix[6] = 0;
	matrix[7] = 0;
	matrix[8] = 0;
	matrix[9] = 0;
	matrix[10] = VEGA_INTTOFIX(1);
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}
void VEGATransform3d::loadScale(VEGA_FIX xscale, VEGA_FIX yscale, VEGA_FIX zscale)
{
	matrix[0] = xscale;
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = 0;
	matrix[4] = 0;
	matrix[5] = yscale;
	matrix[6] = 0;
	matrix[7] = 0;
	matrix[8] = 0;
	matrix[9] = 0;
	matrix[10] = zscale;
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}
void VEGATransform3d::loadTranslate(VEGA_FIX xtrans, VEGA_FIX ytrans, VEGA_FIX ztrans)
{
	matrix[0] = VEGA_INTTOFIX(1);
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = xtrans;
	matrix[4] = 0;
	matrix[5] = VEGA_INTTOFIX(1);
	matrix[6] = 0;
	matrix[7] = ytrans;
	matrix[8] = 0;
	matrix[9] = 0;
	matrix[10] = VEGA_INTTOFIX(1);
	matrix[11] = ztrans;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}
void VEGATransform3d::loadRotateX(VEGA_FIX angle)
{
	matrix[0] = VEGA_INTTOFIX(1);
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = 0;
	matrix[4] = 0;
	matrix[5] = VEGA_FIXCOS(angle);
	matrix[6] = -VEGA_FIXSIN(angle);
	matrix[7] = 0;
	matrix[8] = 0;
	matrix[9] = -matrix[6];
	matrix[10] = matrix[5];
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}
void VEGATransform3d::loadRotateY(VEGA_FIX angle)
{
	matrix[0] = VEGA_FIXCOS(angle);
	matrix[1] = 0;
	matrix[2] = VEGA_FIXSIN(angle);
	matrix[3] = 0;
	matrix[4] = 0;
	matrix[5] = VEGA_INTTOFIX(1);
	matrix[6] = 0;
	matrix[7] = 0;
	matrix[8] = -matrix[2];
	matrix[9] = 0;
	matrix[10] = matrix[0];
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}
void VEGATransform3d::loadRotateZ(VEGA_FIX angle)
{
	matrix[0] = VEGA_FIXCOS(angle);
	matrix[1] = -VEGA_FIXSIN(angle);
	matrix[2] = 0;
	matrix[3] = 0;
	matrix[4] = -matrix[1];
	matrix[5] = matrix[0];
	matrix[6] = 0;
	matrix[7] = 0;
	matrix[8] = 0;
	matrix[9] = 0;
	matrix[10] = VEGA_INTTOFIX(1);
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}

void VEGATransform3d::multiply(const VEGATransform3d &other)
{
	VEGA_FIX tmpmat[16];
	op_memcpy(tmpmat, matrix, sizeof(VEGA_FIX)*16);

	matrix[0] = VEGA_FIXMUL(tmpmat[0], other.matrix[0])+VEGA_FIXMUL(tmpmat[1], other.matrix[4])+
		VEGA_FIXMUL(tmpmat[2], other.matrix[8])+VEGA_FIXMUL(tmpmat[3], other.matrix[12]);
	matrix[1] = VEGA_FIXMUL(tmpmat[0], other.matrix[1])+VEGA_FIXMUL(tmpmat[1], other.matrix[5])+
		VEGA_FIXMUL(tmpmat[2], other.matrix[9])+VEGA_FIXMUL(tmpmat[3], other.matrix[13]);
	matrix[2] = VEGA_FIXMUL(tmpmat[0], other.matrix[2])+VEGA_FIXMUL(tmpmat[1], other.matrix[6])+
		VEGA_FIXMUL(tmpmat[2], other.matrix[10])+VEGA_FIXMUL(tmpmat[3], other.matrix[14]);
	matrix[3] = VEGA_FIXMUL(tmpmat[0], other.matrix[3])+VEGA_FIXMUL(tmpmat[1], other.matrix[7])+
		VEGA_FIXMUL(tmpmat[2], other.matrix[11])+VEGA_FIXMUL(tmpmat[3], other.matrix[15]);

	matrix[4] = VEGA_FIXMUL(tmpmat[4], other.matrix[0])+VEGA_FIXMUL(tmpmat[5], other.matrix[4])+
		VEGA_FIXMUL(tmpmat[6], other.matrix[8])+VEGA_FIXMUL(tmpmat[7], other.matrix[12]);
	matrix[5] = VEGA_FIXMUL(tmpmat[4], other.matrix[1])+VEGA_FIXMUL(tmpmat[5], other.matrix[5])+
		VEGA_FIXMUL(tmpmat[6], other.matrix[9])+VEGA_FIXMUL(tmpmat[7], other.matrix[13]);
	matrix[6] = VEGA_FIXMUL(tmpmat[4], other.matrix[2])+VEGA_FIXMUL(tmpmat[5], other.matrix[6])+
		VEGA_FIXMUL(tmpmat[6], other.matrix[10])+VEGA_FIXMUL(tmpmat[7], other.matrix[14]);
	matrix[7] = VEGA_FIXMUL(tmpmat[4], other.matrix[3])+VEGA_FIXMUL(tmpmat[5], other.matrix[7])+
		VEGA_FIXMUL(tmpmat[6], other.matrix[11])+VEGA_FIXMUL(tmpmat[7], other.matrix[15]);

	matrix[8] = VEGA_FIXMUL(tmpmat[8], other.matrix[0])+VEGA_FIXMUL(tmpmat[9], other.matrix[4])+
		VEGA_FIXMUL(tmpmat[10], other.matrix[8])+VEGA_FIXMUL(tmpmat[11], other.matrix[12]);
	matrix[9] = VEGA_FIXMUL(tmpmat[8], other.matrix[1])+VEGA_FIXMUL(tmpmat[9], other.matrix[5])+
		VEGA_FIXMUL(tmpmat[10], other.matrix[9])+VEGA_FIXMUL(tmpmat[11], other.matrix[13]);
	matrix[10] = VEGA_FIXMUL(tmpmat[8], other.matrix[2])+VEGA_FIXMUL(tmpmat[9], other.matrix[6])+
		VEGA_FIXMUL(tmpmat[10], other.matrix[10])+VEGA_FIXMUL(tmpmat[11], other.matrix[14]);
	matrix[11] = VEGA_FIXMUL(tmpmat[8], other.matrix[3])+VEGA_FIXMUL(tmpmat[9], other.matrix[7])+
		VEGA_FIXMUL(tmpmat[10], other.matrix[11])+VEGA_FIXMUL(tmpmat[11], other.matrix[15]);
	
	matrix[12] = VEGA_FIXMUL(tmpmat[12], other.matrix[0])+VEGA_FIXMUL(tmpmat[13], other.matrix[4])+
		VEGA_FIXMUL(tmpmat[14], other.matrix[8])+VEGA_FIXMUL(tmpmat[15], other.matrix[12]);
	matrix[13] = VEGA_FIXMUL(tmpmat[12], other.matrix[1])+VEGA_FIXMUL(tmpmat[13], other.matrix[5])+
		VEGA_FIXMUL(tmpmat[14], other.matrix[9])+VEGA_FIXMUL(tmpmat[15], other.matrix[13]);
	matrix[14] = VEGA_FIXMUL(tmpmat[12], other.matrix[2])+VEGA_FIXMUL(tmpmat[13], other.matrix[6])+
		VEGA_FIXMUL(tmpmat[14], other.matrix[10])+VEGA_FIXMUL(tmpmat[15], other.matrix[14]);
	matrix[15] = VEGA_FIXMUL(tmpmat[12], other.matrix[3])+VEGA_FIXMUL(tmpmat[13], other.matrix[7])+
		VEGA_FIXMUL(tmpmat[14], other.matrix[11])+VEGA_FIXMUL(tmpmat[15], other.matrix[15]);
}

void VEGATransform3d::copy(const VEGATransform3d& other, bool transpose)
{
	if (transpose)
	{
		matrix[0] = other.matrix[0];
		matrix[1] = other.matrix[4];
		matrix[2] = other.matrix[8];
		matrix[3] = other.matrix[12];
		matrix[4] = other.matrix[1];
		matrix[5] = other.matrix[5];
		matrix[6] = other.matrix[9];
		matrix[7] = other.matrix[13];
		matrix[8] = other.matrix[2];
		matrix[9] = other.matrix[6];
		matrix[10] = other.matrix[10];
		matrix[11] = other.matrix[14];
		matrix[12] = other.matrix[3];
		matrix[13] = other.matrix[7];
		matrix[14] = other.matrix[11];
		matrix[15] = other.matrix[15];
	}
	else
	{
		matrix[0] = other.matrix[0];
		matrix[1] = other.matrix[1];
		matrix[2] = other.matrix[2];
		matrix[3] = other.matrix[3];
		matrix[4] = other.matrix[4];
		matrix[5] = other.matrix[5];
		matrix[6] = other.matrix[6];
		matrix[7] = other.matrix[7];
		matrix[8] = other.matrix[8];
		matrix[9] = other.matrix[9];
		matrix[10] = other.matrix[10];
		matrix[11] = other.matrix[11];
		matrix[12] = other.matrix[12];
		matrix[13] = other.matrix[13];
		matrix[14] = other.matrix[14];
		matrix[15] = other.matrix[15];
	}
}

void VEGATransform3d::copy(const VEGATransform& other)
{
	matrix[0] = other[0];
	matrix[1] = other[1];
	matrix[2] = 0;
	matrix[3] = other[2];
	matrix[4] = other[3];
	matrix[5] = other[4];
	matrix[6] = 0;
	matrix[7] = other[5];
	matrix[8] = 0;
	matrix[9] = 0;
	matrix[10] = VEGA_INTTOFIX(1);
	matrix[11] = 0;
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = VEGA_INTTOFIX(1);
}

#endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT


#endif // VEGA_SUPPORT
