/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERMERGE_H
#define VEGAFILTERMERGE_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafilter.h"

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE
#ifdef VEGA_2DDEVICE
#include "modules/libvega/vega2ddevice.h"
#endif // VEGA_2DDEVICE

/**
 * This is a filter that performs a wide range of composition ("merging")
 * and blending operations.
 *
 * The composition operators supported are:
 *
 * <pre>
 *  over, over w/opacity, in, atop, out, xor
 *  (Porter-Duff composite operators)
 * </pre>
 * 
 * and
 *
 * <pre>
 *  arithmetic (dest = k1*src*dest + k2*src + k3*dest + k4)
 * </pre>
 *
 * Supported blending operations are:
 *
 * <pre>
 *  normal (same as over), multiply, screen, darken, lighten
 * </pre>
 *
 *
 * For all types an offset can be applied, to shift the source and
 * the destination relatively to each other. Thus, a merge type of
 * VEGAMERGE_{NORMAL,OVER} combined with an offset will result
 * in an "offset" filter.
 * Merge filters can not be applied to windows.
 */
class VEGAFilterMerge : public VEGAFilter
{
public:
	VEGAFilterMerge();
	VEGAFilterMerge(VEGAMergeType mt);

	/**
	 * Set opacity value to use with the merge type VEGAMERGE_OPACITY
	 */
	void setOpacity(unsigned char o){opacity = o;}

	/**
	 * Set the factors used by the arithmetic merge type
	 */
	void setArithmeticFactors(VEGA_FIX af_k1, VEGA_FIX af_k2, VEGA_FIX af_k3, VEGA_FIX af_k4)
	{ k1 = af_k1; k2 = af_k2; k3 = af_k3; k4 = af_k4; }

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_2DDEVICE
	virtual OP_STATUS apply(VEGA2dSurface* destRT, const VEGAFilterRegion& region);
#endif // VEGA_2DDEVICE
#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	bool setBlendingFactors();
	OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex);
	OP_STATUS setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
								const VEGAFilterRegion& region);
	void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader);
#endif // VEGA_3DDEVICE

#define DECLARE_MERGE_OP(op) void merge##op(const VEGASWBuffer& dstbuf,\
											const VEGASWBuffer& srcbuf)

	DECLARE_MERGE_OP(Arithmetic);
	DECLARE_MERGE_OP(Multiply);
	DECLARE_MERGE_OP(Screen);
	DECLARE_MERGE_OP(Darken);
	DECLARE_MERGE_OP(Lighten);
	DECLARE_MERGE_OP(Plus);
	DECLARE_MERGE_OP(Atop);
	DECLARE_MERGE_OP(Xor);
	DECLARE_MERGE_OP(Out);
	DECLARE_MERGE_OP(In);
	DECLARE_MERGE_OP(Opacity);
	DECLARE_MERGE_OP(Replace);
	DECLARE_MERGE_OP(Over);

#undef DECLARE_MERGE_OP

	VEGA_FIX k1, k2, k3, k4;
	VEGAMergeType mergeType;
	unsigned char opacity;
#ifdef VEGA_3DDEVICE
	VEGA3dTexture* source2Tex;
	VEGA3dRenderState::BlendWeight srcBlend;
	VEGA3dRenderState::BlendWeight dstBlend;
#endif // VEGA_3DDEVICE
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERMERGE_H

