/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERCONVOLVE_H
#define VEGAFILTERCONVOLVE_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafilter.h"


/**
 * This is a standard 2D-convolution filter.
 *
 * A kernel of a certain dimension (NxM) is specified,
 * together with its center (cx,cy), bias and divisor.
 * The filter is applied using:
 *
 * <pre>
 * dest(x,y) = (SUM i=0 to [M-1] {
 * 					SUM j=0 to [N-1] {
 * 						src[x-cx+j, y-cy+i] * kernel[N-j-1, M-i-1]
 *                  }
 *              } ) / divisor + bias
 * </pre>
 *
 * The filter can be applied to color channels only (excluding the alpha channel),
 * or to all channels.
 *
 * A number of edge modes can be specified:
 * <ul>
 * <li>VEGACONVEDGE_DUPLICATE: duplicate the edge pixels</li>
 * <li>VEGACONVEDGE_WRAP: copy pixels from the opposite edge</li>
 * <li>VEGACONVEDGE_NONE: extend with "zero" (transparent black)</li>
 * </ul>
 */
class VEGAFilterConvolve : public VEGAFilter
{
public:
	VEGAFilterConvolve();
	~VEGAFilterConvolve() { OP_DELETEA(kernel); }

	void setEdgeMode(VEGAFilterEdgeMode em) { edge_mode = em; }
	void setPreserveAlpha(BOOL pa) { preserve_alpha = pa; }

	void setDivisor(VEGA_FIX divis) { divisor = divis; }
	void setBias(VEGA_FIX bia) { bias = bia; }
	void setKernelCenter(int kcx, int kcy) { kern_cx = kcx; kern_cy = kcy; }

	OP_STATUS setKernel(VEGA_FIX* kern_data, int kern_width, int kern_height);

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	OP_STATUS getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex);
	void putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader);

	OP_STATUS setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
								const VEGAFilterRegion& region);
	unsigned int srcx, srcy, srcw, srch;
#endif // VEGA_3DDEVICE

	int kern_w, kern_h; /* Kernel size */
	int kern_cx, kern_cy; /* Kernel center point */

	VEGA_FIX* kernel;

	VEGA_FIX divisor;
	VEGA_FIX bias;

	VEGAFilterEdgeMode edge_mode;

	BOOL preserve_alpha;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERCONVOLVE_H
