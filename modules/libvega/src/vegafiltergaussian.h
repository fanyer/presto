/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERGAUSSIAN_H
#define VEGAFILTERGAUSSIAN_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafilter.h"
#include "modules/libvega/src/vegapixelformat.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE

/**
 * This filter performs a gaussian blur of the source, and stores the
 * result in the destination.
 *
 * This effect could be performed using the VEGAFilterConvolve, but
 * due to the nature of the gaussian function, it has been special cased
 * to gain performance.
 *
 * For standard deviations >= 2.0, an approximation is used, that performs
 * a number of successive box-blurs instead of using the normal gaussian kernel.
 */
class VEGAFilterGaussian : public VEGAFilter
{
public:
	VEGAFilterGaussian();
	~VEGAFilterGaussian();

	/**
	 * Set the standard deviation on each axis.
	 *
	 * Larger values - more blur, smaller values - less blur
	 */
	OP_STATUS setStdDeviation(VEGA_FIX x, VEGA_FIX y);

	/**
	 * Set wraparound mode.
	 *
	 * Decides how edges should be handled:
	 * setWrap(true):  Data outside of the edges are fetched as if the
	 *				   source data was tiled.
	 * setWrap(false): Data outside of the edges are zero (transparent black)
	 */
	void setWrap(bool wraparound) { wrap = wraparound; }

	/**
	 * Initialize the values of a gaussian blur kernel using the given standard deviation and kernel size.
	 * The kernel size has to be 2*kernel_width+1 and the kernel must be large enough to hold that many values.
	 */
	static void initKernel(VEGA_FIX stdDev, VEGA_FIX* kernel, int kernel_width, int kernel_size);
private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	void setupSourceRect(unsigned int sx, unsigned int sy, unsigned int width, unsigned int height,
						 VEGA3dTexture* tex, VEGA3dDevice::Vega2dVertex* verts);
	void setupDestinationRect(unsigned int dx, unsigned int dy, unsigned int width, unsigned int height,
							  VEGA3dDevice::Vega2dVertex* verts);

	OP_STATUS preparePass(unsigned int passno, struct VEGAMultiPassParams& params);
	OP_STATUS applyRecursive(struct VEGAMultiPassParams& params);

	OP_STATUS applyBoxBlur(VEGABackingStore_FBO* destStore, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	OP_STATUS preparePassRealGauss(unsigned int passno, struct VEGAMultiPassParams& params);
	OP_STATUS preparePassBoxBlur(unsigned int passno, struct VEGAMultiPassParams& params);
#endif // VEGA_3DDEVICE

	OP_STATUS createKernel(VEGA_FIX stdDev, VEGA_FIX** kernel, int& kernel_size);

	void realGaussRow(VEGAPixelPtr dstp, unsigned int dststride,
					  VEGAPixelPtr srcp, unsigned int srcstride,
					  unsigned int count, const VEGA_FIX* kernel,
					  unsigned int half_kl);
	void boxBlurRow(VEGAPixelPtr dstp, unsigned int dststride,
					VEGAPixelPtr srcp, unsigned int srcstride,
					unsigned int count,
					unsigned int left_count, unsigned int right_count);
	void blur(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf);

	void realGaussRow_A(VEGAPixelPtr dstp, unsigned int dststride,
						VEGAPixelPtr srcp, unsigned int srcstride,
						unsigned int count, const VEGA_FIX* kernel,
						unsigned int half_kl);
	void boxBlurRow_A(VEGAPixelPtr dstp, unsigned int dststride,
					  VEGAPixelPtr srcp, unsigned int srcstride,
					  unsigned int count,
					  unsigned int left_count, unsigned int right_count);
	void blur_A(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf);

	void realGaussRow_W(VEGAPixelPtr dstp, unsigned int dststride,
						VEGAPixelPtr srcp, unsigned int srcstride,
						unsigned int count, const VEGA_FIX* kernel,
						unsigned int half_kl);
	void boxBlurRow_W(VEGAPixelPtr dstp, unsigned int dststride,
					  VEGAPixelPtr srcp, unsigned int srcstride,
					  unsigned int count,
					  unsigned int left_count, unsigned int right_count);
	void blur_W(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf);

	void boxBlurRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
					   VEGAPixelPtr srcp, unsigned int srcstride,
					   unsigned int count,
					   unsigned int left_count, unsigned int right_count);
	void realGaussRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
						 VEGAPixelPtr srcp, unsigned int srcstride,
						 unsigned int count, const VEGA_FIX* kernel,
						 unsigned int half_kl);
	void blur_AW(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf);

	bool wrap;
	
	int kernel_w;
	int kernel_h;

	VEGA_FIX* kernel_x;
	VEGA_FIX* kernel_y;

	UINT32* cbuf;
	unsigned char* cbuf_a;
	unsigned int cbuf_mask;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERGAUSSIAN_H

