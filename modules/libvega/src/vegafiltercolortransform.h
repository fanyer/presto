/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERCOLORTRANSFORM_H
#define VEGAFILTERCOLORTRANSFORM_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafilter.h"

enum VEGAColorTransformType { 
	VEGACOLORTRANSFORM_MATRIX,
	VEGACOLORTRANSFORM_LUMINANCETOALPHA,
	VEGACOLORTRANSFORM_COMPONENTTRANSFER,
	VEGACOLORTRANSFORM_COLORSPACECONV
};

enum VEGACSConversionType {
	VEGACSCONV_SRGB_TO_LINRGB,
	VEGACSCONV_LINRGB_TO_SRGB
};

/**
 * Filter that can transform colors in a number of different ways:
 *
 * Matrix:
 *
 * The destination will be the result of applying a specified matrix
 * (5x5 with a constant fifth row) to each pixel in the source in the
 * following fashion:
 *
 * <pre>
 *  |dr|   |a00 a01 a02 a03 a04|   |sr|
 *  |dg|   |a10 a11 a12 a13 a14|   |sg|
 *  |db| = |a20 a21 a22 a23 a24| * |sb|
 *  |da|   |a30 a31 a32 a33 a34|   |sa|
 *  |1 |   | 0   0   0   0   1 |   |1 |
 * </pre>
 *
 *
 * Luminance to Alpha:
 *
 * The destination (only alpha channel set) will be the luminance of the source:
 *
 * <pre>
 * da = 0.2125*sr + 0.7154*sg + 0.0721*sb
 * dr = dg = db = 0;
 * </pre>
 *
 *
 * Component Transform:
 *
 * For each pixel in the source, a function is applied per component.
 *
 * <pre>
 * dr = FuncR(sr)
 * dg = FuncG(sg)
 * db = FuncB(sb)
 * da = FuncA(sa)
 * </pre>
 *
 * The available functions are:
 *
 * <ul>
 * <li>identity (no change)</li>
 * <li>table (linear interpolation using a lookup table)</li>
 * <li>discrete (lookup table)</li>
 * <li>linear (linear mapping)</li>
 * <li>gamma (gamma correction)</li>
 * </ul>
 *
 *
 * Colorspace Conversion:
 *
 * A special case of the above, to perform sRGB <-> linearRGB conversions.
 *
 */
class VEGAFilterColorTransform : public VEGAFilter
{
public:
	VEGAFilterColorTransform(VEGAColorTransformType type, VEGA_FIX* mat = NULL);
	~VEGAFilterColorTransform();

	void setMatrix(VEGA_FIX* mat);
	VEGA_FIX* getMatrix() { return &matrix[0]; }

	void setType(VEGAColorTransformType type) { transform_type = type; }

	void setCompIdentity(VEGAComponent comp);
	void setCompTable(VEGAComponent comp, VEGA_FIX* c_table, unsigned int c_table_size);
	void setCompDiscrete(VEGAComponent comp, VEGA_FIX* c_table, unsigned int c_table_size);
	void setCompLinear(VEGAComponent comp, VEGA_FIX slope, VEGA_FIX intercept);
	void setCompGamma(VEGAComponent comp, VEGA_FIX ampl, VEGA_FIX exp, VEGA_FIX offs);
	void setColorSpaceConversion(VEGACSConversionType type);

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** shader, VEGA3dTexture* srcTex);
	void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader);

	VEGA3dTexture* xlat_table; // Used as a LUT for componenttransfer
	bool xlat_table_needs_update;
	VEGACSConversionType csconv_type;
#endif // VEGA_3DDEVICE

	VEGAColorTransformType transform_type;
	VEGA_FIX matrix[20];

	/* Hmm, is this too expensive? (1k) */
	unsigned char lut[4][256];
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERCOLORTRANSFORM_H
