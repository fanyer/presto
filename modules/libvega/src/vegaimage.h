/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAIMAGE_H
#define VEGAIMAGE_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegatransform.h"
#include "modules/libvega/src/vegabackend.h"
#include "modules/libvega/src/vegaswbuffer.h"

enum VEGAImageBufferFlags
{
	// NOTE: An opacity value will be stored in the lower 8 bits, so
	// don't store flags there.
	BUF_NONE			= 0,
	BUF_OPAQUE			= 1 << 8,
	BUF_PREMULTIPLIED	= 1 << 9
};

class VEGABackingStore;

#ifdef USE_PREMULTIPLIED_ALPHA
#define DEFAULT_PREMULTIPLIED true
#else
#define DEFAULT_PREMULTIPLIED false
#endif // USE_PREMULTIPLIED_ALPHA

class VEGAImage : public VEGAFill
{
public:
	VEGAImage();
	~VEGAImage();

	virtual OP_STATUS prepare();
	virtual void apply(VEGAPixelAccessor color, struct VEGASpanInfo& span);
	virtual void complete();

#ifdef VEGA_OPPAINTER_SUPPORT
	static OP_STATUS drawImage(VEGASWBuffer* buf, int dx, int dy, int width, int height,
							   VEGASWBuffer* srcbuf, const struct VEGADrawImageInfo& imginfo,
							   unsigned flags);

	/** Determine if this VEGAImage (in its current configuration) can
	 * be simplified to a blit-operation for the destination rectangle
	 * set in diinfo. */
	bool simplifiesToBlit(struct VEGADrawImageInfo& diinfo, bool tilingSupported);

#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	/** Check if this VEGAImage can be sent to the blitter. */
	bool allowBlitter(VEGA_FIX dstx, VEGA_FIX dsty, VEGA_FIX dstw, VEGA_FIX dsth) const;
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

	/** Fills in the src-rect of diinfo based on the dst-rect and the 
		current ransform. No validation of the src-rect is performed, 
		so it might be zero-sized or point outside of the image. 
		Width and height of this image is returned as well */
	bool prepareDrawImageInfo(struct VEGADrawImageInfo& diinfo, int& width, int& height, bool checkIfGridAligned);
#endif // VEGA_OPPAINTER_SUPPORT

	OP_STATUS init(OpBitmap* bmp);
	OP_STATUS init(VEGABackingStore* store, bool premultiplied = DEFAULT_PREMULTIPLIED);

#ifdef VEGA_3DDEVICE
	/** Mainly for internal usage (creating image from intermediate render target etc). */
	OP_STATUS init(VEGA3dTexture* tex, bool premultiplied = DEFAULT_PREMULTIPLIED);
#endif // VEGA_3DDEVICE

	static void calcTransforms(const struct VEGADrawImageInfo& imginfo, VEGATransform& trans, VEGATransform& itrans, const VEGATransform* ctm = NULL);

	virtual void setTransform(const VEGATransform& pathTrans, const VEGATransform& invPathTrans)
	{
		VEGAFill::setTransform(pathTrans, invPathTrans);

		is_aligned_and_nonscaled = invPathTrans.isAlignedAndNonscaled();
		is_simple_translation = invPathTrans.isIntegralTranslation();

		if (is_simple_translation)
		{
			int_xlat_x = VEGA_TRUNCFIXTOINT(invPathTransform[2]);
			int_xlat_y = VEGA_TRUNCFIXTOINT(invPathTransform[5]);
		}
	}

#ifdef VEGA_3DDEVICE
	virtual VEGA3dTexture* getTexture();
#endif // VEGA_3DDEVICE

	/** Limits the visible area of an image. Rendering
	 * outside this area will give undefined results,
	 * so be carefull when using this. */
	OP_STATUS limitArea(int left, int top, int right, int bottom);

	bool hasPremultipliedAlpha() { return is_premultiplied_alpha; }
	void makePremultiplied();
	bool isImage() const { return true; }
#ifdef VEGA_OPPAINTER_SUPPORT
	bool isOpaque() const { return is_opaque; }
	void setOpaque(bool opaque) { is_opaque = opaque; }
	bool isIndexed() const { return !!backingstore->IsIndexed(); }

	VEGASWBuffer* GetSWBuffer(){return &imgbuf;}
#endif // VEGA_OPPAINTER_SUPPORT

	VEGABackingStore* GetBackingStore() { return backingstore; }

private:
	void applyAligned(VEGAPixelAccessor color, int sx, int sy, unsigned length);
#ifdef VEGA_OPPAINTER_SUPPORT
	static void drawAligned(VEGASWBuffer* buf, int dx, int dy, int width, int height,
							VEGASWBuffer* srcbuf, int sx, int sy, unsigned flags);
	static void drawScaledNearest(VEGASWBuffer* buf, int dx, int dy, int width, int height,
								  VEGASWBuffer* srcbuf, const struct VEGAImageSamplerParams& params,
								  unsigned flags);
	static OP_STATUS drawScaledBilinear(VEGASWBuffer* buf, int dx, int dy, int width, int height,
										VEGASWBuffer* srcbuf, const struct VEGAImageSamplerParams& params,
										unsigned flags);

	static void drawAlignedIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
								   VEGASWBuffer* srcbuf, int sx, int sy,
								   unsigned flags);
	static void drawScaledNearestIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
										 VEGASWBuffer* srcbuf, const struct VEGAImageSamplerParams& params,
										 unsigned flags);
	static OP_STATUS drawScaledBilinearIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
											   VEGASWBuffer* srcbuf, const struct VEGAImageSamplerParams& params,
											   unsigned flags);
#endif // VEGA_OPPAINTER_SUPPORT
	void releaseBuffers();

	VEGABackingStore* backingstore;
	VEGASWBuffer imgbuf;
	bool imgbuf_need_free;
	bool is_premultiplied_alpha;
	bool is_aligned_and_nonscaled;
	bool is_simple_translation;
	bool is_opaque;

	OpBitmap* bitmap;

	int int_xlat_x;
	int int_xlat_y;

#ifdef VEGA_3DDEVICE
	VEGA3dTexture* texture;
#endif // VEGA_3DDEVICE
};

#undef DEFAULT_PREMULTIPLIED


// Sufficient tolerance - should be deduced from the rasterizer accuracy (which is variable :/)
#define VEGA_GRID_TOLERANCE		(VEGA_INTTOFIX(1) / 32)

static inline bool IsGridAligned(VEGA_FIX x, VEGA_FIX y)
{
	return
		VEGA_ABS(x - VEGA_TRUNCFIX(x)) < VEGA_GRID_TOLERANCE &&
		VEGA_ABS(y - VEGA_TRUNCFIX(y)) < VEGA_GRID_TOLERANCE;
}

#endif // VEGA_SUPPORT
#endif // VEGAIMAGE_H
