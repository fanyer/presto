/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAFILTERMORPHOLOGY_H
#define VEGAFILTERMORPHOLOGY_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafilter.h"
#include "modules/libvega/src/vegapixelformat.h"

/**
 * Perform a morphology operation.
 *
 * Supported morphology operations are
 * dilate ("fattening"):
 *
 * <pre>
 * dest = component-wise maximum of src in an 2*x-radius x 2*y-radius environment
 * </pre>
 *
 * erode ("thinning"):
 *
 * <pre>
 * dest = component-wise minimum of src in an 2*x-radius x 2*y-radius environment
 * </pre>
 */
class VEGAFilterMorphology : public VEGAFilter
{
public:
	VEGAFilterMorphology();
	VEGAFilterMorphology(VEGAMorphologyType mt);

	void setMorphologyType(VEGAMorphologyType mt) { morphType = mt; }
	void setRadius(int rx, int ry) { rad_x = rx; rad_y = ry; }
	void setWrap(bool wraparound) { wrap = wraparound; }

private:
	virtual OP_STATUS apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region);

#ifdef VEGA_3DDEVICE
	virtual OP_STATUS apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame);

	OP_STATUS preparePass(unsigned int passno, struct VEGAMultiPassParamsMorph& params);
	OP_STATUS applyRecursive(struct VEGAMultiPassParamsMorph& params);
#endif // VEGA_3DDEVICE

	void dilateRow(VEGAPixelPtr dstp, unsigned int dststride,
				   VEGAPixelPtr srcp, unsigned int srcstride,
				   unsigned length, unsigned radius, unsigned buf_mask);
	void dilateRow_A(VEGAPixelPtr dstp, unsigned int dststride,
					 VEGAPixelPtr srcp, unsigned int srcstride,
					 unsigned length, unsigned radius, unsigned buf_mask);
	void dilateRow_W(VEGAPixelPtr dstp, unsigned int dststride,
					 VEGAPixelPtr srcp, unsigned int srcstride,
					 unsigned length, unsigned radius, unsigned buf_mask);
	void dilateRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
					  VEGAPixelPtr srcp, unsigned int srcstride,
					  unsigned length, unsigned radius, unsigned buf_mask);
	void erodeRow(VEGAPixelPtr dstp, unsigned int dststride,
				  VEGAPixelPtr srcp, unsigned int srcstride,
				  unsigned length, unsigned radius, unsigned buf_mask);
	void erodeRow_A(VEGAPixelPtr dstp, unsigned int dststride,
					VEGAPixelPtr srcp, unsigned int srcstride,
					unsigned length, unsigned radius, unsigned buf_mask);
	void erodeRow_W(VEGAPixelPtr dstp, unsigned int dststride,
					VEGAPixelPtr srcp, unsigned int srcstride,
					unsigned length, unsigned radius, unsigned buf_mask);
	void erodeRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
					 VEGAPixelPtr srcp, unsigned int srcstride,
					 unsigned length, unsigned radius, unsigned buf_mask);

	VEGA_PIXEL* rbuf;
	UINT8* rbuf_a;

	VEGAMorphologyType morphType;

	bool wrap;
	
	int rad_x;
	int rad_y;
};

#endif // VEGA_SUPPORT
#endif // VEGAFILTERMORPHOLOGY_H

