/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegaimage.h"
#include "modules/pi/OpBitmap.h"

#include "modules/libvega/vegarenderer.h"

#include "modules/libvega/src/oppainter/vegaopbitmap.h"

#include "modules/libvega/src/vegacomposite.h"
#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/src/vegasampler.h"

#include "modules/libvega/src/vegabackend_sw.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

#define VEGASampler(NAME, PARAMS) VEGA_PIXEL_FORMAT_CLASS::NAME PARAMS

VEGAImage::VEGAImage() :
	backingstore(NULL), imgbuf_need_free(false),
	is_aligned_and_nonscaled(true), is_simple_translation(true), is_opaque(false),
	bitmap(NULL), int_xlat_x(0), int_xlat_y(0)
#ifdef VEGA_3DDEVICE
	, texture(0)
#endif // VEGA_3DDEVICE
{
	imgbuf.Reset();
}

VEGAImage::~VEGAImage()
{
	VEGARefCount::DecRef(backingstore);

	if (imgbuf_need_free)
		imgbuf.Destroy();
}

/* static */
void VEGAImage::calcTransforms(const VEGADrawImageInfo& imginfo, VEGATransform& trans, VEGATransform& itrans, const VEGATransform* ctm)
{
	if (imginfo.type == VEGADrawImageInfo::REPEAT)
	{
		// tile_offset_x/y is in tile_width/height coordinates
		// so it does not need to be scaled.
		VEGA_FIX d_tx = VEGA_INTTOFIX(imginfo.dstx - imginfo.tile_offset_x);
		VEGA_FIX d_ty = VEGA_INTTOFIX(imginfo.dsty - imginfo.tile_offset_y);
		trans.loadTranslate(d_tx, d_ty);
		if (imginfo.srcw != imginfo.tile_width || imginfo.srch != imginfo.tile_height)
		{
			VEGA_FIX sx = VEGA_INTTOFIX(imginfo.tile_width) / imginfo.srcw;
			VEGA_FIX sy = VEGA_INTTOFIX(imginfo.tile_height) / imginfo.srch;
			itrans.loadScale(sx, sy);
			trans.multiply(itrans);
		}
	}
	else if (imginfo.dstw != imginfo.srcw || imginfo.dsth != imginfo.srch)
	{
		// Calculate forward transform
		// FT = T(-source) * S(dest/source) * T(dest+xlt) [* CTM]
		VEGA_FIX d_tx = VEGA_INTTOFIX(imginfo.dstx);
		VEGA_FIX d_ty = VEGA_INTTOFIX(imginfo.dsty);
		trans.loadTranslate(d_tx, d_ty);

		VEGA_FIX sx = VEGA_INTTOFIX(imginfo.dstw) / imginfo.srcw;
		VEGA_FIX sy = VEGA_INTTOFIX(imginfo.dsth) / imginfo.srch;
		itrans.loadScale(sx, sy);
		trans.multiply(itrans);
		itrans.loadTranslate(VEGA_INTTOFIX(-imginfo.srcx), VEGA_INTTOFIX(-imginfo.srcy));
		trans.multiply(itrans);
	}
	else
	{
		// Simple translation
		trans.loadTranslate(VEGA_INTTOFIX(imginfo.dstx - imginfo.srcx),
							VEGA_INTTOFIX(imginfo.dsty - imginfo.srcy));
	}

	itrans.copy(trans);

	if (ctm)
	{
		trans.copy(*ctm);
		trans.multiply(itrans);
		itrans.copy(trans);
	}

	itrans.invert();
}

#ifdef VEGA_OPPAINTER_SUPPORT

bool VEGAImage::simplifiesToBlit(VEGADrawImageInfo& diinfo, bool tilingSupported)
{
	if (!tilingSupported && (xspread == SPREAD_REPEAT || yspread == SPREAD_REPEAT))
		return false;

	int width, height;
	if (!prepareDrawImageInfo(diinfo, width, height, true))
		return false;

	// Check bounds (this can be improved to handle more cases, like
	// clamp border with a transparent border)
	if (diinfo.srcx < 0 ||
		diinfo.srcy < 0 ||
		diinfo.srcx + diinfo.srcw > (unsigned)width ||
		diinfo.srcy + diinfo.srch > (unsigned)height)
		return false;

	return true;
}

#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
/** Check if this VEGAImage can be sent to the blitter. */
bool VEGAImage::allowBlitter(VEGA_FIX dstx, VEGA_FIX dsty, VEGA_FIX dstw, VEGA_FIX dsth) const
{
	// Allow if the spread in both directions are the same, and is
	// either 'repeat', 'clamp' or 'clamp-to-border' with a transparent
	// border.
	if (!(xspread == yspread &&
		  (xspread == SPREAD_CLAMP ||
		   xspread == SPREAD_REPEAT ||
		   (xspread == SPREAD_CLAMP_BORDER && borderColor == 0))))
		return false;

	// Spread-mode is either 'clamp' or 'clamp-border' (with
	// transparent border). Check that we won't be sampling too
	// far outside the source, since the fast-path can't really
	// deal with that (correctly). This is a bit of a hack (but so
	// is the entire code-path), and might be better suited to a
	// entrypoint of it's own.

	if (backingstore == NULL)
		// Only case when this is true should be for the special
		// texture mode - which hopefully should be able to do without
		// this.
		return false;

	const VEGA_FIX pointFive = VEGA_FIXDIV2(VEGA_INTTOFIX(1));
	VEGA_FIX sx = dstx + pointFive;
	VEGA_FIX sy = dsty + pointFive;
	VEGA_FIX ex = sx + dstw;
	VEGA_FIX ey = sy + dsth;

	invPathTransform.apply(sx, sy);
	sx -= pointFive;
	sy -= pointFive;

	invPathTransform.apply(ex, ey);
	ex -= pointFive;
	ey -= pointFive;

	int isrcsx = VEGA_TRUNCFIXTOINT(sx);
	int isrcsy = VEGA_TRUNCFIXTOINT(sy);
	int isrcex = VEGA_TRUNCFIXTOINT(ex);
	int isrcey = VEGA_TRUNCFIXTOINT(ey);

	// Don't allow source rectangles stray too far outside the image.
	if (isrcsx < -1 || isrcsy < -1)
		return false;
	if (xspread != SPREAD_REPEAT && (isrcex > (int)backingstore->GetWidth() || isrcey > (int)backingstore->GetHeight()))
		return false;

	return true;
}
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

bool VEGAImage::prepareDrawImageInfo(struct VEGADrawImageInfo& diinfo, int& width, int& height, bool checkIfGridAligned)
{
	if (backingstore == NULL)
		// Only case when this is true should be for the special
		// texture mode - which hopefully should be able to do without
		// this.
		return false;

	width = backingstore->GetWidth();
	height = backingstore->GetHeight();

	if (width == 0 || height == 0)
 		return false;

	// Translation and positive scale is what we support
	if (!is_simple_translation &&
		!(invPathTransform.isScaleAndTranslateOnly() && invPathTransform[0] > 0 && invPathTransform[4] > 0))
		return false;

	 // Should have been checked in allowBlitter() that preceeds this call
	OP_ASSERT(xspread==yspread);

	if (xspread == SPREAD_REPEAT)
	{
		diinfo.srcx = 0;
		diinfo.srcy = 0;
		diinfo.srcw = width;
		diinfo.srch = height;

		if (is_simple_translation)
		{
			diinfo.tile_offset_x = diinfo.dstx + int_xlat_x;
			diinfo.tile_offset_y = diinfo.dsty + int_xlat_y;
			diinfo.tile_width  = diinfo.srcw;
			diinfo.tile_height = diinfo.srch;
		}
		else
		{
			const VEGA_FIX pointFive = VEGA_FIXDIV2(VEGA_INTTOFIX(1));
			VEGA_FIX sw = VEGA_INTTOFIX(diinfo.srcw);
			VEGA_FIX sh = VEGA_INTTOFIX(diinfo.srch);
			VEGA_FIX sx = VEGA_INTTOFIX(diinfo.dstx) + pointFive;
			VEGA_FIX sy = VEGA_INTTOFIX(diinfo.dsty) + pointFive;

			// Calculate how big the tile should be
			pathTransform.applyVector(sw, sh);

			// Transform sx and sy to a offset within the image
			invPathTransform.apply(sx, sy);

			// Scale the offset so that it has the same scale as the tile
			pathTransform.applyVector(sx, sy);
			sx -= pointFive;
			sy -= pointFive;

			// Is the source rectangle sufficiently aligned?
			// Note: checkIfGridAligned is not respected here since
			// there are currently no workarounds for when
			// the spread is 'repeat' (like fillFractionalImages).
			// Therefor we always fail if the tile is not grid aligned.
			if (!IsGridAligned(sx, sy) || !IsGridAligned(sx+sw, sy+sh))
				return false;

			diinfo.tile_offset_x = VEGA_TRUNCFIXTOINT(sx);
			diinfo.tile_offset_y = VEGA_TRUNCFIXTOINT(sy);
			diinfo.tile_width  = VEGA_FIXTOINT(sw);
			diinfo.tile_height = VEGA_FIXTOINT(sh);

			if (diinfo.tile_width == 0 || diinfo.tile_height == 0)
				return false;
		}

		// Make sure that tile_offset.x/y is within the tile
		if (diinfo.tile_offset_x < 0)
			diinfo.tile_offset_x += (-diinfo.tile_offset_x/diinfo.tile_width + 1)*diinfo.tile_width;
		else if (diinfo.tile_offset_x >= (int)diinfo.tile_width)
			diinfo.tile_offset_x -= (diinfo.tile_offset_x/diinfo.tile_width)*diinfo.tile_width;

		if (diinfo.tile_offset_y < 0)
			diinfo.tile_offset_y += (-diinfo.tile_offset_y/diinfo.tile_height + 1)*diinfo.tile_height;
		else if (diinfo.tile_offset_y >= (int)diinfo.tile_height)
			diinfo.tile_offset_y -= (diinfo.tile_offset_y/diinfo.tile_height)*diinfo.tile_height;
	}
	else
	{
		if (is_simple_translation)
		{
			diinfo.srcx = diinfo.dstx + int_xlat_x;
			diinfo.srcy = diinfo.dsty + int_xlat_y;
			diinfo.srcw = diinfo.dstw;
			diinfo.srch = diinfo.dsth;
		}
		else
		{
			const VEGA_FIX pointFive = VEGA_FIXDIV2(VEGA_INTTOFIX(1));
			VEGA_FIX sx = VEGA_INTTOFIX(diinfo.dstx) + pointFive;
			VEGA_FIX sy = VEGA_INTTOFIX(diinfo.dsty) + pointFive;
			VEGA_FIX ex = VEGA_INTTOFIX(diinfo.dstx+diinfo.dstw) + pointFive;
			VEGA_FIX ey = VEGA_INTTOFIX(diinfo.dsty+diinfo.dsth) + pointFive;

			invPathTransform.apply(sx, sy);
			sx -= pointFive;
			sy -= pointFive;

			invPathTransform.apply(ex, ey);
			ex -= pointFive;
			ey -= pointFive;

			// Is the source rectangle sufficiently aligned?
			if (checkIfGridAligned &&
				(!IsGridAligned(sx, sy) || !IsGridAligned(ex, ey)))
				return false;

			diinfo.srcx = VEGA_TRUNCFIXTOINT(sx);
			diinfo.srcy = VEGA_TRUNCFIXTOINT(sy);
			diinfo.srcw = VEGA_TRUNCFIXTOINT(ex) - diinfo.srcx;
			diinfo.srch = VEGA_TRUNCFIXTOINT(ey) - diinfo.srcy;
		}
	}

	diinfo.opacity = getFillOpacity();
	diinfo.quality = this->quality;

	if (xspread==SPREAD_REPEAT)
	{
		diinfo.type = VEGADrawImageInfo::REPEAT;
	}
	else if (diinfo.srcw==diinfo.dstw && diinfo.srch==diinfo.dsth)
	{
		diinfo.type = VEGADrawImageInfo::NORMAL;
	}
	else
	{
		diinfo.type = VEGADrawImageInfo::SCALED;
	}

	return true;
}
#endif // VEGA_OPPAINTER_SUPPORT

void VEGAImage::makePremultiplied()
{
	OP_ASSERT(!is_premultiplied_alpha);

	if (imgbuf_need_free)
	{
		imgbuf.Premultiply();
		is_premultiplied_alpha = true;
	}
	else
	{
		OP_ASSERT(backingstore);

		VEGASWBuffer* buffer = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
		if (!buffer)
			return;

		imgbuf = *buffer;

		VEGASWBuffer tmp;
		if (OpStatus::IsSuccess(tmp.Create(imgbuf.width, imgbuf.height)))
		{
			tmp.Premultiply(imgbuf);

			imgbuf = tmp;
			imgbuf_need_free = true;
			is_premultiplied_alpha = true;
		}

		backingstore->EndTransaction(FALSE);
	}
}

#ifdef VEGA_OPPAINTER_SUPPORT
static inline unsigned VEGAImage_GetOpacity(unsigned flags)
{
	return (flags & 0xff);
}

/* static */
void VEGAImage::drawAligned(VEGASWBuffer* buf, int dx, int dy, int width, int height,
							VEGASWBuffer* srcbuf, int sx, int sy, unsigned flags)
{
	if (srcbuf->IsIndexed())
	{
		drawAlignedIndexed(buf, dx, dy, width, height, srcbuf, sx, sy, flags);
		return;
	}

	VEGAConstPixelAccessor data = srcbuf->GetConstAccessor(sx, sy);
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	if (VEGAImage_GetOpacity(flags) != 255)
	{
		UINT32 opacity = VEGAImage_GetOpacity(flags);

		if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				VEGACompOverIn(color.Ptr(), data.Ptr(), opacity, width);

				color += colorPixelStride;
				data += dataPixelStride;
			}
		}
		else if (flags & BUF_PREMULTIPLIED)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				VEGACompOverInPM(color.Ptr(), data.Ptr(), opacity, width);

				color += colorPixelStride;
				data += dataPixelStride;
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				unsigned cnt = width;
				while (cnt-- > 0)
				{
					VEGA_PIXEL src = VEGAPixelPremultiplyFast(data.Load());
					color.Store(VEGACompOverIn(color.Load(), src, opacity));

					++color;
					++data;
				}

				color += colorPixelStride - width;
				data += dataPixelStride - width;
#else
				VEGACompOverIn(color.Ptr(), data.Ptr(), opacity, width);

				color += colorPixelStride;
				data += dataPixelStride;
#endif // USE_PREMULTIPLIED_ALPHA
			}
		}
	}
	else
	{
		if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				color.CopyFrom(data.Ptr(), width);

				color += colorPixelStride;
				data += dataPixelStride;
			}
		}
		else if (flags & BUF_PREMULTIPLIED)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				VEGACompOverPM(color.Ptr(), data.Ptr(), width);

				color += colorPixelStride;
				data += dataPixelStride;
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				unsigned cnt = width;
				while (cnt-- > 0)
				{
					color.Store(VEGACompOver(color.Load(), VEGAPixelPremultiplyFast(data.Load())));

					++color;
					++data;
				}

				color += colorPixelStride - width;
				data += dataPixelStride - width;
#else
				VEGACompOver(color.Ptr(), data.Ptr(), width);

				color += colorPixelStride;
				data += dataPixelStride;
#endif // USE_PREMULTIPLIED_ALPHA
			}
		}
	}
}

static inline void CopyIndirect(VEGAPixelAccessor dst, const UINT8* isrc, const VEGA_PIXEL* pal,
								unsigned cnt)
{
	while (cnt-- > 0)
	{
		dst.Store(pal[*isrc++]);

		++dst;
	}
}

#ifdef USE_PREMULTIPLIED_ALPHA
#define AS_PREMULT(v) v
#define AS_UNPREMULT(v) VEGAPixelPremultiplyFast(v)
#else
#define AS_PREMULT(v) VEGAPixelUnpremultiplyFast(v)
#define AS_UNPREMULT(v) v
#endif // USE_PREMULTIPLIED_ALPHA

/* static */
void VEGAImage::drawAlignedIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
								   VEGASWBuffer* srcbuf, int sx, int sy, unsigned flags)
{
	OP_ASSERT(srcbuf->IsIndexed());

	const UINT8* data = srcbuf->GetIndexedPtr(sx, sy);
	const VEGA_PIXEL* pal = srcbuf->GetPalette();
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	if (VEGAImage_GetOpacity(flags) != 255)
	{
		UINT32 opacity = VEGAImage_GetOpacity(flags);

		if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				unsigned cnt = width;
				while (cnt-- > 0)
				{
					color.Store(VEGACompOverIn(color.Load(), pal[*data], opacity));

					++color;
					++data;
				}

				color += colorPixelStride - width;
				data += dataPixelStride - width;
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
				unsigned cnt = width;
				while (cnt-- > 0)
				{
					color.Store(VEGACompOverIn(color.Load(), pal[*data], opacity));

					++color;
					++data;
				}

				color += colorPixelStride - width;
				data += dataPixelStride - width;
			}
		}
	}
	else
	{
		if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				CopyIndirect(color, data, pal, width);

				color += colorPixelStride;
				data += dataPixelStride;
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
				unsigned cnt = width;
				while (cnt-- > 0)
				{
					color.Store(VEGACompOver(color.Load(), pal[*data]));

					++color;
					++data;
				}

				color += colorPixelStride - width;
				data += dataPixelStride - width;
			}
		}
	}
}

struct VEGAImageSamplerParams
{
	INT32 csx, csy;
	INT32 cdx, cdy;
};

#define SAMPLE_SETUP_SCANLINE(TYPE, PTR)		\
	TYPE ldata = PTR;							\
	int src_line = VEGA_SAMPLE_INT(csy);		\
	if (src_line < 0)							\
		src_line = 0;							\
	else if (src_line >= (int)srcbuf->height)	\
		src_line = srcbuf->height - 1;			\
	ldata = ldata + dataPixelStride * src_line;	\
	INT32 csx = params.csx;						\
	unsigned cnt = width;

#define SAMPLE_LEFT_EDGE(STORE_ARGS)			\
	while (cnt && csx < 0)						\
	{											\
		color.Store(STORE_ARGS);				\
		csx += params.cdx;						\
		color++;								\
		cnt--;									\
	}

#define SAMPLE_PRE_MIDDLE															\
	unsigned isect_cnt = cnt;														\
	INT32 cdx = params.cdx;															\
	if (params.cdx)																	\
		isect_cnt = MIN((VEGA_INTTOSAMPLE(srcbuf->width) - csx) / cdx, cnt);		\
	OP_ASSERT(cnt >= isect_cnt);													\
	cnt -= isect_cnt;

#define SAMPLE_MIDDLE(STORE_ARGS)										\
	SAMPLE_PRE_MIDDLE													\
	while (isect_cnt-- > 0)												\
	{																	\
		color.Store(STORE_ARGS);										\
		csx += params.cdx;												\
		color++;														\
	}

#define SAMPLE_RIGHT_EDGE(SAMPLE_FUNC)			\
	if (cnt)									\
	{											\
		SAMPLE_FUNC;							\
		color += cnt;							\
	}

#define SAMPLE_NEXT_SCANLINE					\
	csy += params.cdy;							\
	color += colorPixelStride;

/* static */
void VEGAImage::drawScaledNearest(VEGASWBuffer* buf, int dx, int dy, int width, int height,
								  VEGASWBuffer* srcbuf, const VEGAImageSamplerParams& params,
								  unsigned flags)
{
	if (srcbuf->IsIndexed())
	{
		drawScaledNearestIndexed(buf, dx, dy, width, height, srcbuf, params, flags);
		return;
	}

	VEGAConstPixelAccessor data = srcbuf->GetConstAccessor(0, 0);
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	colorPixelStride -= width;

#define SAMPLE_INIT(SAMPLER_LEFT)						\
	SAMPLE_SETUP_SCANLINE(VEGAPixelPtr, data.Ptr())		\
	SAMPLE_LEFT_EDGE(SAMPLER_LEFT)						\
	SAMPLE_PRE_MIDDLE

#define SAMPLE_FINISH(SAMPLER_RIGHT)					\
	SAMPLE_RIGHT_EDGE(SAMPLER_RIGHT)					\
	SAMPLE_NEXT_SCANLINE

	INT32 csy = params.csy;

	if (flags & BUF_PREMULTIPLIED)
	{
		if (VEGAImage_GetOpacity(flags) != 255)
		{
			UINT32 opacity = VEGAImage_GetOpacity(flags);

			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGACompOverIn(color.Load(), VEGASamplerNearestPM_1D(ldata, 0), opacity));
				VEGASampler(SamplerNearestX_CompOverConstMask,
							(color, ldata, opacity, isect_cnt, csx, cdx));
				SAMPLE_FINISH(VEGACompOver(color.Ptr(), VEGAPixelModulate(VEGASamplerNearestPM_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), opacity), cnt));
			}
		}
		else if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGASamplerNearestPM_1D(ldata, 0));
				VEGASampler(SamplerNearestX_Opaque,
							(color, ldata, isect_cnt, csx, cdx));
				SAMPLE_FINISH(color.Store(VEGASamplerNearestPM_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGACompOver(color.Load(), VEGASamplerNearestPM_1D(ldata, 0)));
				VEGASampler(SamplerNearestX_CompOver,
							(color, ldata, isect_cnt, csx, cdx));
				SAMPLE_FINISH(VEGACompOver(color.Ptr(), VEGASamplerNearestPM_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
			}
		}
	}
	else
	{
		if (VEGAImage_GetOpacity(flags) != 255)
		{
			UINT32 opacity = VEGAImage_GetOpacity(flags);

			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGACompOverIn(color.Load(), VEGASamplerNearest_1D(ldata, 0), opacity));
				while (isect_cnt-- > 0)
				{
					color.Store(VEGACompOverIn(color.Load(), VEGASamplerNearest_1D(ldata, csx), opacity));
					csx += cdx;
					color++;
				}
				SAMPLE_FINISH(VEGACompOver(color.Ptr(), VEGAPixelModulate(VEGASamplerNearest_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), opacity), cnt));
			}
		}
		else if (flags & BUF_OPAQUE)
		{
			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGASamplerNearest_1D(ldata, 0));
				while (isect_cnt-- > 0)
				{
					color.Store(VEGASamplerNearest_1D(ldata, csx));
					csx += cdx;
					color++;
				}
				SAMPLE_FINISH(color.Store(VEGASamplerNearest_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
			}
		}
		else
		{
			for (int yp = 0; yp < height; ++yp)
			{
				SAMPLE_INIT(VEGACompOver(color.Load(), VEGASamplerNearest_1D(ldata, 0)));
				while (isect_cnt-- > 0)
				{
					color.Store(VEGACompOver(color.Load(), VEGASamplerNearest_1D(ldata, csx)));
					csx += cdx;
					color++;
				}
				SAMPLE_FINISH(VEGACompOver(color.Ptr(), VEGASamplerNearest_1D(ldata, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
			}
		}
	}

#undef SAMPLE_INIT
#undef SAMPLE_FINISH
#undef SAMPLE_EXTERN_FUNC_BEGIN
#undef SAMPLE_EXTERN_FUNC_END
}

static inline VEGA_PIXEL IndexedSamplerNearest_1D(const UINT8* isrc, const VEGA_PIXEL* pal, INT32 csx)
{
	return AS_UNPREMULT(pal[*(isrc + VEGA_SAMPLE_INT(csx))]);
}

static inline VEGA_PIXEL IndexedSamplerNearestPM_1D(const UINT8* isrc, const VEGA_PIXEL* pal, INT32 csx)
{
	return AS_PREMULT(pal[*(isrc + VEGA_SAMPLE_INT(csx))]);
}

#undef AS_PREMULT
#undef AS_UNPREMULT

/* static */
void VEGAImage::drawScaledNearestIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
										 VEGASWBuffer* srcbuf, const VEGAImageSamplerParams& params,
										 unsigned flags)
{
	const UINT8* data = srcbuf->GetIndexedPtr(0, 0);
	const VEGA_PIXEL* pal = srcbuf->GetPalette();
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	colorPixelStride -= width;

#define SAMPLE_BLOCK_INDEXED(SAMPLER_LEFT, SAMPLER_MIDDLE, SAMPLER_RIGHT) \
	for (int yp = 0; yp < height; ++yp)									\
	{																	\
		SAMPLE_SETUP_SCANLINE(const UINT8*, data)						\
		SAMPLE_LEFT_EDGE(SAMPLER_LEFT)									\
		SAMPLE_MIDDLE(SAMPLER_MIDDLE)									\
		SAMPLE_RIGHT_EDGE(SAMPLER_RIGHT)								\
		SAMPLE_NEXT_SCANLINE											\
	}

	INT32 csy = params.csy;

	if (flags & BUF_PREMULTIPLIED)
	{
		if (VEGAImage_GetOpacity(flags) != 255)
		{
			UINT32 opacity = VEGAImage_GetOpacity(flags);

			SAMPLE_BLOCK_INDEXED(VEGACompOverIn(color.Load(), IndexedSamplerNearestPM_1D(ldata, pal, 0), opacity),
								 VEGACompOverIn(color.Load(), IndexedSamplerNearestPM_1D(ldata, pal, csx), opacity),
								 VEGACompOver(color.Ptr(), VEGAPixelModulate(IndexedSamplerNearestPM_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), opacity), cnt));
		}
		else if (flags & BUF_OPAQUE)
		{
			SAMPLE_BLOCK_INDEXED(IndexedSamplerNearestPM_1D(ldata, pal, 0),
								 IndexedSamplerNearestPM_1D(ldata, pal, csx),
								 color.Store(IndexedSamplerNearestPM_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
		}
		else
		{
			SAMPLE_BLOCK_INDEXED(VEGACompOver(color.Load(), IndexedSamplerNearestPM_1D(ldata, pal, 0)),
								 VEGACompOver(color.Load(), IndexedSamplerNearestPM_1D(ldata, pal, csx)),
								 VEGACompOver(color.Ptr(), IndexedSamplerNearestPM_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
		}
	}
	else
	{
		if (VEGAImage_GetOpacity(flags) != 255)
		{
			UINT32 opacity = VEGAImage_GetOpacity(flags);

			SAMPLE_BLOCK_INDEXED(VEGACompOverIn(color.Load(), IndexedSamplerNearest_1D(ldata, pal, 0), opacity),
								 VEGACompOverIn(color.Load(), IndexedSamplerNearest_1D(ldata, pal, csx), opacity),
								 VEGACompOver(color.Ptr(), VEGAPixelModulate(IndexedSamplerNearest_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), opacity), cnt));
		}
		else if (flags & BUF_OPAQUE)
		{
			SAMPLE_BLOCK_INDEXED(IndexedSamplerNearest_1D(ldata, pal, 0),
								 IndexedSamplerNearest_1D(ldata, pal, csx),
								 color.Store(IndexedSamplerNearest_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
		}
		else
		{
			SAMPLE_BLOCK_INDEXED(VEGACompOver(color.Load(), IndexedSamplerNearest_1D(ldata, pal, 0)),
								 VEGACompOver(color.Load(), IndexedSamplerNearest_1D(ldata, pal, csx)),
								 VEGACompOver(color.Ptr(), IndexedSamplerNearest_1D(ldata, pal, VEGA_INTTOSAMPLE(srcbuf->width-1)), cnt));
		}
	}

#undef SAMPLE_BLOCK_INDEXED
}

#undef SAMPLE_NEXT_SCANLINE
#undef SAMPLE_RIGHT_EDGE
#undef SAMPLE_MIDDLE
#undef SAMPLE_LEFT_EDGE
#undef SAMPLE_SETUP_SCANLINE

static inline void IndexedSamplerLerpXPM(VEGAPixelPtr dstdata, const UINT8* src, const VEGA_PIXEL* pal,
										 INT32 csx, INT32 cdx, unsigned dstlen, unsigned srclen)
{
	VEGAPixelAccessor dst(dstdata);

	while (dstlen && csx < 0)
	{
		dst.Store(pal[*src]);

		++dst;
		csx += cdx;
		dstlen--;
	}
	while (dstlen > 0 && csx < static_cast<int>(VEGA_INTTOSAMPLE(srclen-1)))
	{
		INT32 int_px = VEGA_SAMPLE_INT(csx);
		INT32 frc_px = VEGA_SAMPLE_FRAC(csx);

		VEGA_PIXEL c = pal[*(src + int_px)];
		if (frc_px)
		{
			int c0a = VEGA_UNPACK_A(c);
			int c0r = VEGA_UNPACK_R(c);
			int c0g = VEGA_UNPACK_G(c);
			int c0b = VEGA_UNPACK_B(c);

			VEGA_PIXEL nc = pal[*(src + int_px + 1)];

			c0a += (((VEGA_UNPACK_A(nc) - c0a) * frc_px) >> 8);
			c0r += (((VEGA_UNPACK_R(nc) - c0r) * frc_px) >> 8);
			c0g += (((VEGA_UNPACK_G(nc) - c0g) * frc_px) >> 8);
			c0b += (((VEGA_UNPACK_B(nc) - c0b) * frc_px) >> 8);

			c = VEGA_PACK_ARGB(c0a, c0r, c0g, c0b);
#ifndef USE_PREMULTIPLIED_ALPHA
			c = VEGAPixelUnpremultiplyFast(c);
#endif // !USE_PREMULTIPLIED_ALPHA
		}

		dst.Store(c);

		++dst;
		csx += cdx;
		dstlen--;
	}
	while (dstlen > 0)
	{
		dst.Store(pal[*(src + srclen - 1)]);

		++dst;
		csx += cdx;
		dstlen--;
	}
}

static inline VEGA_PIXEL IndexedSamplerNearest(const UINT8* data, const VEGA_PIXEL* pal,
											   unsigned dataPixelStride, INT32 csx, INT32 csy)
{
	return pal[data[VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx)]];
}

static inline VEGA_PIXEL IndexedSamplerBilerpPM(const UINT8* data, const VEGA_PIXEL* pal,
												unsigned dataPixelStride, INT32 csx, INT32 csy)
{
	const UINT8* samp = data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);

	int weight2 = VEGA_SAMPLE_FRAC(csx);
	int yweight2 = VEGA_SAMPLE_FRAC(csy);

	VEGA_PIXEL curr;
	if (weight2)
	{
		// Sample from current line
		int c0a = VEGA_UNPACK_A(pal[*samp]);
		int c0r = VEGA_UNPACK_R(pal[*samp]);
		int c0g = VEGA_UNPACK_G(pal[*samp]);
		int c0b = VEGA_UNPACK_B(pal[*samp]);

		samp++;

		int c1a = VEGA_UNPACK_A(pal[*samp]);
		int c1r = VEGA_UNPACK_R(pal[*samp]);
		int c1g = VEGA_UNPACK_G(pal[*samp]);
		int c1b = VEGA_UNPACK_B(pal[*samp]);

		c0a += (((c1a - c0a) * weight2) >> 8);
		c0r += (((c1r - c0r) * weight2) >> 8);
		c0g += (((c1g - c0g) * weight2) >> 8);
		c0b += (((c1b - c0b) * weight2) >> 8);

		if (yweight2)
		{
			samp += dataPixelStride - 1;

			// Sample from next line
			int c2a = VEGA_UNPACK_A(pal[*samp]);
			int c2r = VEGA_UNPACK_R(pal[*samp]);
			int c2g = VEGA_UNPACK_G(pal[*samp]);
			int c2b = VEGA_UNPACK_B(pal[*samp]);

			samp++;

			int c3a = VEGA_UNPACK_A(pal[*samp]);
			int c3r = VEGA_UNPACK_R(pal[*samp]);
			int c3g = VEGA_UNPACK_G(pal[*samp]);
			int c3b = VEGA_UNPACK_B(pal[*samp]);

			c2a += (((c3a - c2a) * weight2) >> 8);
			c2r += (((c3r - c2r) * weight2) >> 8);
			c2g += (((c3g - c2g) * weight2) >> 8);
			c2b += (((c3b - c2b) * weight2) >> 8);

			// Merge line samples
			c0a += (((c2a - c0a) * yweight2) >> 8);
			c0r += (((c2r - c0r) * yweight2) >> 8);
			c0g += (((c2g - c0g) * yweight2) >> 8);
			c0b += (((c2b - c0b) * yweight2) >> 8);
		}

		curr = VEGA_PACK_ARGB(c0a, c0r, c0g, c0b);
	}
	else if (yweight2)
	{
		// Sample from column
		int c0a = VEGA_UNPACK_A(pal[*samp]);
		int c0r = VEGA_UNPACK_R(pal[*samp]);
		int c0g = VEGA_UNPACK_G(pal[*samp]);
		int c0b = VEGA_UNPACK_B(pal[*samp]);

		samp += dataPixelStride;

		int c2a = VEGA_UNPACK_A(pal[*samp]);
		int c2r = VEGA_UNPACK_R(pal[*samp]);
		int c2g = VEGA_UNPACK_G(pal[*samp]);
		int c2b = VEGA_UNPACK_B(pal[*samp]);

		c0a += (((c2a - c0a) * yweight2) >> 8);
		c0r += (((c2r - c0r) * yweight2) >> 8);
		c0g += (((c2g - c0g) * yweight2) >> 8);
		c0b += (((c2b - c0b) * yweight2) >> 8);

		curr = VEGA_PACK_ARGB(c0a, c0r, c0g, c0b);
	}
	else
	{
		curr = pal[*samp];
	}

#ifndef USE_PREMULTIPLIED_ALPHA
	curr = VEGAPixelUnpremultiplyFast(curr);
#endif // !USE_PREMULTIPLIED_ALPHA
	return curr;
}

/* static */
OP_STATUS VEGAImage::drawScaledBilinearIndexed(VEGASWBuffer* buf, int dx, int dy, int width, int height,
											   VEGASWBuffer* srcbuf, const VEGAImageSamplerParams& params,
											   unsigned flags)
{
	OP_ASSERT(flags & (BUF_PREMULTIPLIED | BUF_OPAQUE));

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	VEGASWBuffer tmp;
	RETURN_IF_ERROR(tmp.Create(width, 3));

	const UINT8* data = srcbuf->GetIndexedPtr(0, 0);
	const VEGA_PIXEL* pal = srcbuf->GetPalette();
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelPtr scanlineCache[2];
	scanlineCache[0] = tmp.GetAccessor(0, 0).Ptr();
	scanlineCache[1] = scanlineCache[0] + tmp.GetPixelStride();
	int scanlineCacheItem[2] = { -1, -1 };

	VEGAPixelPtr ybuf = scanlineCache[1] + tmp.GetPixelStride();

	INT32 csy = params.csy;
	for (int yp = 0; yp < height; ++yp)
	{
		int sl = VEGA_SAMPLE_INT(csy);
		INT32 frc_y = VEGA_SAMPLE_FRAC(csy);

		if (sl < 0)
		{
			sl = 0;
			frc_y = 0;
		}
		else if (sl >= (int)srcbuf->height)
		{
			sl = srcbuf->height - 1;
			frc_y = 0;
		}

		const UINT8* srcdata = data;
		srcdata = srcdata + dataPixelStride * sl;

		if (scanlineCacheItem[0] != sl)
		{
			if (scanlineCacheItem[1] == sl)
			{
				VEGAPixelPtr t = scanlineCache[0];
				scanlineCache[0] = scanlineCache[1];
				scanlineCache[1] = t;

				scanlineCacheItem[1] = -1; // Invalidate
			}
			else
			{
				IndexedSamplerLerpXPM(scanlineCache[0], srcdata, pal, params.csx, params.cdx,
									  width, srcbuf->width);
			}

			scanlineCacheItem[0] = sl;
		}

		VEGAPixelPtr blend_src;
		if (frc_y && sl+1 < (int)srcbuf->height)
		{
			if (scanlineCacheItem[1] != sl+1)
			{
				IndexedSamplerLerpXPM(scanlineCache[1], srcdata + dataPixelStride, pal,
									  params.csx, params.cdx, width, srcbuf->width);

				scanlineCacheItem[1] = sl+1;
			}

			VEGASamplerLerpYPM(ybuf, scanlineCache[0], scanlineCache[1], frc_y, width);

			blend_src = ybuf;
		}
		else
		{
			blend_src = scanlineCache[0];
		}

		if (VEGAImage_GetOpacity(flags) != 255)
			VEGACompOverIn(color.Ptr(), blend_src, VEGAImage_GetOpacity(flags), width);
		else if (flags & BUF_OPAQUE)
			color.CopyFrom(blend_src, width);
		else
			VEGACompOver(color.Ptr(), blend_src, width);

		color += colorPixelStride;
		csy += params.cdy;
	}

	tmp.Destroy();
	return OpStatus::OK;
}

/* static */
OP_STATUS VEGAImage::drawScaledBilinear(VEGASWBuffer* buf, int dx, int dy, int width, int height,
										VEGASWBuffer* srcbuf, const VEGAImageSamplerParams& params,
										unsigned flags)
{
	OP_ASSERT(flags & (BUF_PREMULTIPLIED | BUF_OPAQUE));

	if (srcbuf->IsIndexed())
		return drawScaledBilinearIndexed(buf, dx, dy, width, height, srcbuf, params, flags);

	VEGAPixelAccessor color = buf->GetAccessor(dx, dy);
	unsigned int colorPixelStride = buf->GetPixelStride();

	VEGASWBuffer tmp;
	RETURN_IF_ERROR(tmp.Create(width, 3));

	VEGAConstPixelAccessor data = srcbuf->GetConstAccessor(0, 0);
	unsigned int dataPixelStride = srcbuf->GetPixelStride();

	VEGAPixelPtr scanlineCache[2];
	scanlineCache[0] = tmp.GetAccessor(0, 0).Ptr();
	scanlineCache[1] = scanlineCache[0] + tmp.GetPixelStride();
	int scanlineCacheItem[2] = { -1, -1 };

	VEGAPixelPtr ybuf = scanlineCache[1] + tmp.GetPixelStride();
	bool need_composite = !(flags & BUF_OPAQUE) || VEGAImage_GetOpacity(flags) != 255;

 	INT32 csy = params.csy;
	for (int yp = 0; yp < height; ++yp)
	{
		int sl = VEGA_SAMPLE_INT(csy);
		INT32 frc_y = VEGA_SAMPLE_FRAC(csy);

		if (sl < 0)
		{
			sl = 0;
			frc_y = 0;
		}
		else if (sl >= (int)srcbuf->height)
		{
			sl = srcbuf->height - 1;
			frc_y = 0;
		}

		VEGAPixelPtr srcdata = data.Ptr();
		srcdata = srcdata + dataPixelStride * sl;

		if (scanlineCacheItem[0] != sl)
		{
			if (scanlineCacheItem[1] == sl)
			{
				VEGAPixelPtr t = scanlineCache[0];
				scanlineCache[0] = scanlineCache[1];
				scanlineCache[1] = t;

				scanlineCacheItem[1] = -1; // Invalidate
			}
			else
			{
				VEGASamplerLerpXPM(scanlineCache[0], srcdata, params.csx, params.cdx,
								   width, srcbuf->width);
			}

			scanlineCacheItem[0] = sl;
		}

		VEGAPixelPtr blend_src;
		if (frc_y && sl+1 < (int)srcbuf->height)
		{
			if (scanlineCacheItem[1] != sl+1)
			{
				VEGASamplerLerpXPM(scanlineCache[1], srcdata + dataPixelStride, params.csx, params.cdx,
								   width, srcbuf->width);

				scanlineCacheItem[1] = sl+1;
			}

			blend_src = ybuf;

			if (!need_composite)
				blend_src = color.Ptr();

			VEGASamplerLerpYPM(blend_src, scanlineCache[0], scanlineCache[1], frc_y, width);
		}
		else
		{
			blend_src = scanlineCache[0];

			if (!need_composite)
				color.CopyFrom(blend_src, width);
		}

		if (need_composite)
		{
			if (VEGAImage_GetOpacity(flags) != 255)
				VEGACompOverInPM(color.Ptr(), blend_src, VEGAImage_GetOpacity(flags), width);
			else
				VEGACompOverPM(color.Ptr(), blend_src, width);
		}

		color += colorPixelStride;
		csy += params.cdy;
	}

	tmp.Destroy();
	return OpStatus::OK;
}

/* static */
OP_STATUS VEGAImage::drawImage(VEGASWBuffer* buf, int dx, int dy, int width, int height,
							   VEGASWBuffer* srcbuf, const VEGADrawImageInfo& imginfo,
							   unsigned flags)
{
	OP_ASSERT(VEGAImage_GetOpacity(flags) == 0);

	flags |= imginfo.opacity;

	// IT = [sw/dw  0   -dstx * sw/dw + srcx]
	//      [  0  sh/dh -dsty * sh/dh + srcy]

	if (imginfo.dstw == imginfo.srcw && imginfo.dsth == imginfo.srch)
	{
		// IT * (px+0.5) - (0.5) = ((px - dstx + 0.5) * sw/dw + srcx - 0.5)
		//      (py+0.5)   (0.5)   ((py - dsty + 0.5) * sh/dh + srcy - 0.5)
		// {dw == sw} => (px - dstx + srcx)
		// {dh == sh}    (py - dsty + srcy)
		int sx = imginfo.srcx + dx - imginfo.dstx;
		int sy = imginfo.srcy + dy - imginfo.dsty;

		drawAligned(buf, dx, dy, width, height, srcbuf, sx, sy, flags);
		return OpStatus::OK;
	}

	double cdx = (double)imginfo.srcw / imginfo.dstw;
	double cdy = (double)imginfo.srch / imginfo.dsth;

	VEGAImageSamplerParams params;
	params.cdx = VEGA_DBLTOSAMPLE(cdx);
	params.cdy = VEGA_DBLTOSAMPLE(cdy);

	// IT * (px+0.5) - (0.5) = ((px - dstx + 0.5) * sw/dw + srcx - 0.5)
	//      (py+0.5)   (0.5)   ((py - dsty + 0.5) * sh/dh + srcy - 0.5)
	params.csx = VEGA_INTTOSAMPLE(imginfo.srcx);
	params.csy = VEGA_INTTOSAMPLE(imginfo.srcy);
	params.csx += VEGA_DBLTOSAMPLE((dx - imginfo.dstx + 0.5) * cdx);
	params.csy += VEGA_DBLTOSAMPLE((dy - imginfo.dsty + 0.5) * cdy);

	if (imginfo.quality == QUALITY_LINEAR)
	{
		// Rounding compensation
		params.csx -= 1 << (VEGA_SAMPLER_FRACBITS - 1);
		params.csy -= 1 << (VEGA_SAMPLER_FRACBITS - 1);

		return drawScaledBilinear(buf, dx, dy, width, height, srcbuf, params, flags);
	}

	drawScaledNearest(buf, dx, dy, width, height, srcbuf, params, flags);
	return OpStatus::OK;
}
#endif // VEGA_OPPAINTER_SUPPORT

void VEGAImage::applyAligned(VEGAPixelAccessor color, int sx, int sy, unsigned length)
{
	int pic_x = VEGA_SAMPLE_INT(sx);
	int pic_y = VEGA_SAMPLE_INT(sy);

	if (pic_y >= (int)imgbuf.height)
	{
		switch (yspread)
		{
		case SPREAD_CLAMP:
			pic_y = imgbuf.height-1;
			break;
		case SPREAD_CLAMP_BORDER:
			VEGACompOver(color.Ptr(), ColorToVEGAPixel(borderColor), length);
			return;
		case SPREAD_REPEAT:
		default:
			pic_y = pic_y % imgbuf.height;
			break;
		}
	}

	OP_ASSERT(!imgbuf.IsIndexed());

	OP_ASSERT(pic_x >= 0);
	if (pic_x >= (int)imgbuf.width)
	{
		switch (xspread)
		{
		case SPREAD_CLAMP:
		case SPREAD_CLAMP_BORDER:
			// Skip the copy part below, but make sure the -1 sample
			// is correct in the 'clamp' case.
			pic_x = imgbuf.width;
			break;
		case SPREAD_REPEAT:
		default:
			pic_x = pic_x % imgbuf.width;
			break;
		}
	}

	VEGAConstPixelAccessor src_row = imgbuf.GetConstAccessor(0, pic_y);
	VEGAConstPixelAccessor src(src_row.Ptr());
	src += pic_x;

	if (is_opaque)
	{
		while (length)
		{
			if (pic_x < (int)imgbuf.width)
			{
				unsigned slice_len = MIN(length, imgbuf.width - (unsigned)pic_x);
				length -= slice_len;
				pic_x += slice_len;

				color.CopyFrom(src.Ptr(), slice_len);

				src += slice_len;
				color += slice_len;
			}

			VEGA_PIXEL clamp_color = ColorToVEGAPixel(borderColor);
			switch (xspread)
			{
			case SPREAD_CLAMP:
				clamp_color = src.LoadRel(-1);
			case SPREAD_CLAMP_BORDER:
				VEGACompOver(color.Ptr(), clamp_color, length);
				return;
			case SPREAD_REPEAT:
			default:
				OP_ASSERT(length == 0 || (unsigned)pic_x == imgbuf.width);
				pic_x = 0;
				src.Reset(src_row.Ptr());
				break;
			}
		}
	}
	else if (is_premultiplied_alpha)
	{
		while (length)
		{
			if (pic_x < (int)imgbuf.width)
			{
				unsigned slice_len = MIN(length, imgbuf.width - (unsigned)pic_x);
				length -= slice_len;
				pic_x += slice_len;

#ifdef USE_PREMULTIPLIED_ALPHA
				VEGACompOver(color.Ptr(), src.Ptr(), slice_len);

				src += slice_len;
				color += slice_len;
#else
				while (slice_len-- > 0)
				{
					color.Store(VEGACompOver(color.Load(), VEGAPixelUnpremultiplyFast(src.Load())));

					++color;
					++src;
				}
#endif // USE_PREMULTIPLIED_ALPHA
			}

			VEGA_PIXEL clamp_color = ColorToVEGAPixel(borderColor);
			switch (xspread)
			{
			case SPREAD_CLAMP:
				clamp_color = src.LoadRel(-1);
			case SPREAD_CLAMP_BORDER:
				VEGACompOver(color.Ptr(), clamp_color, length);
				return;
			case SPREAD_REPEAT:
			default:
				OP_ASSERT(length == 0 || (unsigned)pic_x == imgbuf.width);
				pic_x = 0;
				src.Reset(src_row.Ptr());
				break;
			}
		}
	}
	else
	{
		while (length)
		{
			if (pic_x < (int)imgbuf.width)
			{
				unsigned slice_len = MIN(length, imgbuf.width - (unsigned)pic_x);
				length -= slice_len;
				pic_x += slice_len;

#ifdef USE_PREMULTIPLIED_ALPHA
				while (slice_len-- > 0)
				{
					color.Store(VEGACompOver(color.Load(), VEGAPixelPremultiplyFast(src.Load())));

					++color;
					++src;
				}
#else
				VEGACompOver(color.Ptr(), src.Ptr(), slice_len);

				src += slice_len;
				color += slice_len;
#endif // USE_PREMULTIPLIED_ALPHA
			}

			VEGA_PIXEL clamp_color = ColorToVEGAPixel(borderColor);
			switch (xspread)
			{
			case SPREAD_CLAMP:
				clamp_color = src.LoadRel(-1);
				/* fall through */
			case SPREAD_CLAMP_BORDER:
				VEGACompOver(color.Ptr(), clamp_color, length);
				return;
			case SPREAD_REPEAT:
			default:
				// The only time we end up here is when length == 0 or
				// when pic_x >= imgbuf.width. The first case is
				// trivial, and in the second, since we increment by
				// one in the loop, the only time pic_x can be >
				// imgbuf.width is if it starts out that way, and that
				// could be taken care of in advance.
				OP_ASSERT(length == 0 || (unsigned)pic_x == imgbuf.width);
				pic_x = 0;
				src.Reset(src_row.Ptr());
				break;
			}
		}
	}
}

void VEGAImage::apply(VEGAPixelAccessor color, struct VEGASpanInfo& span)
{
	Quality quality = this->quality;
	if (quality == QUALITY_LINEAR)
	{
		// Optimizes the case of images that are aligned on pixelgrid,
		// have scale == 1 and fills one of the criteria:
		//  - rotated n * 90 degrees
		//  - is a horizontal and/or vertical flip
		if (is_aligned_and_nonscaled)
			quality = QUALITY_NEAREST;
		else if (!is_premultiplied_alpha)
			makePremultiplied();
	}

	int firstPixel = span.pos;

	// Calculate start and stop position in texture space
	INT32 sx, sy, dx, dy = 0;

	if (!is_simple_translation)
	{
		VEGA_FIX scan_start_x = VEGA_INTTOFIX(firstPixel);
		VEGA_FIX scan_start_y = VEGA_INTTOFIX(span.scanline);

		const VEGA_FIX pointFive = VEGA_FIXDIV2(VEGA_INTTOFIX(1));
		scan_start_x += pointFive;
		scan_start_y += pointFive;
		invPathTransform.apply(scan_start_x, scan_start_y);
		scan_start_x -= pointFive;
		scan_start_y -= pointFive;

		sx = VEGA_FIXTOSCALEDINT_T(scan_start_x, VEGA_SAMPLER_FRACBITS);
		sy = VEGA_FIXTOSCALEDINT_T(scan_start_y, VEGA_SAMPLER_FRACBITS);

		dx = VEGA_FIXTOSCALEDINT(invPathTransform[0], VEGA_SAMPLER_FRACBITS);
		dy = VEGA_FIXTOSCALEDINT(invPathTransform[3], VEGA_SAMPLER_FRACBITS);
	}
	else
	{
		sx = VEGA_INTTOSAMPLE(firstPixel + int_xlat_x);
		sy = VEGA_INTTOSAMPLE(span.scanline + int_xlat_y);

		dx = VEGA_INTTOSAMPLE(1);
	}

	// Special case for rendering an unscaled and aligned bitmap to the screen
	if (!imgbuf.IsIndexed() && quality == QUALITY_NEAREST &&
		dy == 0 && (dx == VEGA_INTTOSAMPLE(1) || dx == 0) &&
		!span.mask && xspread != SPREAD_REFLECT && sx >= 0 && sy >= 0 && yspread != SPREAD_REFLECT)
	{
		applyAligned(color, sx, sy, span.length);
		return;
	}

	// Use the nearest scanline when using nearest interpolation
	if (quality == QUALITY_NEAREST)
	{
		sx += 1 << (VEGA_SAMPLER_FRACBITS - 1);
		sy += 1 << (VEGA_SAMPLER_FRACBITS - 1);
	}

	int lastPixel = span.pos + span.length - 1;
	const UINT8* alpha = span.mask;

	while (firstPixel <= lastPixel)
	{
		// Find which of the images this would be (in both x an y direction)
		int pic_x = 0;
		int pic_y = 0;
		INT32 csx = sx;
		INT32 cdx = dx;
		INT32 csy = sy;
		INT32 cdy = dy;
		pic_x = VEGA_SAMPLE_INT(csx / (int)imgbuf.width);
		pic_y = VEGA_SAMPLE_INT(csy / (int)imgbuf.height);
		csx -= VEGA_INTTOSAMPLE(imgbuf.width * pic_x);
		csy -= VEGA_INTTOSAMPLE(imgbuf.height * pic_y);
		if (csx < 0)
		{
			csx += VEGA_INTTOSAMPLE(imgbuf.width);
			--pic_x;
		}
		if (csy < 0)
		{
			csy += VEGA_INTTOSAMPLE(imgbuf.height);
			--pic_y;
		}
		OP_ASSERT(csx >= 0);
		OP_ASSERT(csy >= 0);
		if (xspread == SPREAD_REFLECT && pic_x&1)
		{
			csx = VEGA_INTTOSAMPLE(imgbuf.width) - csx;
			cdx = -cdx;
			if (csx >= VEGA_INTTOSAMPLE((int)imgbuf.width))
			{
				csx -= VEGA_INTTOSAMPLE(imgbuf.width);
			}
		}
		if (yspread == SPREAD_REFLECT && pic_y&1)
		{
			csy = VEGA_INTTOSAMPLE(imgbuf.height) - csy;
			cdy = -cdy;
			if (csy >= VEGA_INTTOSAMPLE((int)imgbuf.height))
			{
				csy -= VEGA_INTTOSAMPLE(imgbuf.height);
			}
		}
		if ((xspread == SPREAD_CLAMP || xspread == SPREAD_CLAMP_BORDER) && pic_x != 0)
		{
			csx += VEGA_INTTOSAMPLE(imgbuf.width * pic_x);
		}
		else
			pic_x = 0;
		if ((yspread == SPREAD_CLAMP || yspread == SPREAD_CLAMP_BORDER) && pic_y != 0)
		{
			csy += VEGA_INTTOSAMPLE(imgbuf.height * pic_y);
		}
		else
			pic_y = 0;

		bool wrapx = false;
		bool wrapy = false;
		if (csx >= VEGA_INTTOSAMPLE((int)imgbuf.width-1) && !pic_x)
		{
			// Special case since it is not possible to interpolate this without special case
			if (xspread == SPREAD_REPEAT)
				wrapx = true;
			else
				csx = VEGA_INTTOSAMPLE(imgbuf.width-1);
		}
		if (csy >= VEGA_INTTOSAMPLE((int)imgbuf.height-1) && !pic_y)
		{
			// Special case since it is not possible to interpolate this without special case
			if (yspread == SPREAD_REPEAT)
				wrapy = true;
			else
				csy = VEGA_INTTOSAMPLE(imgbuf.height-1);
		}

		int length = 1+lastPixel-firstPixel;

#ifdef VEGA_OPPAINTER_SUPPORT
		bool is_indexed = imgbuf.IsIndexed();
#else
		bool is_indexed = false;
#endif // VEGA_OPPAINTER_SUPPORT

		unsigned int dataPixelStride = imgbuf.GetPixelStride();
		VEGAConstPixelAccessor data = imgbuf.GetConstAccessor(0, 0);

#ifdef VEGA_OPPAINTER_SUPPORT
		const UINT8* idata = NULL;
		const VEGA_PIXEL* ipal = imgbuf.GetPalette();
		if (is_indexed)
		{
			idata = imgbuf.GetIndexedPtr(0, 0);
		}
#endif // VEGA_OPPAINTER_SUPPORT

		UINT32 borderdata[4]; /* ARRAY OK 2007-04-27 timj */
		// Currently length is set to 1 since we might not intersect with 0 or width-1 in time
		if (wrapx && wrapy)
		{
			VEGASWBuffer tmpbuf;
			tmpbuf.CreateFromData(borderdata, 4*4, 4, 1);
			VEGAPixelAccessor bdata = tmpbuf.GetAccessor(0, 0);
			if (!is_indexed)
			{
				bdata.Store(data.LoadRel((imgbuf.height - 1) * dataPixelStride + imgbuf.width - 1));
				++bdata;
				bdata.Store(data.LoadRel((imgbuf.height - 1) * dataPixelStride));
				++bdata;
				bdata.Store(data.LoadRel(imgbuf.width - 1));
				++bdata;
				bdata.Store(data.Load());
			}
#ifdef VEGA_OPPAINTER_SUPPORT
			else
			{
				bdata.Store(ipal[*(idata + (imgbuf.height - 1) * dataPixelStride + imgbuf.width - 1)]);
				++bdata;
				bdata.Store(ipal[*(idata + (imgbuf.height - 1) * dataPixelStride)]);
				++bdata;
				bdata.Store(ipal[*(idata + imgbuf.width - 1)]);
				++bdata;
				bdata.Store(ipal[*idata]);

				is_indexed = false;
			}
#endif // VEGA_OPPAINTER_SUPPORT

			dataPixelStride = 2;
			data = tmpbuf.GetConstAccessor(0, 0);
			csx &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			csy &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			length = 1;
		}
		else if (wrapx)
		{
			int line = VEGA_SAMPLE_INT(csy);
			if (line < 0)
				line = 0;
			else if (line >= (int)imgbuf.height)
				line = imgbuf.height-1;

			VEGASWBuffer tmpbuf;
			tmpbuf.CreateFromData(borderdata, 4*4, 4, 1);
			VEGAPixelAccessor bdata = tmpbuf.GetAccessor(0, 0);

			if (!is_indexed)
			{
				bdata.Store(data.LoadRel(line * dataPixelStride + imgbuf.width - 1));
				++bdata;
				bdata.Store(data.LoadRel(line * dataPixelStride));
				++bdata;
			}
#ifdef VEGA_OPPAINTER_SUPPORT
			else
			{
				bdata.Store(ipal[*(idata + line * dataPixelStride + imgbuf.width - 1)]);
				++bdata;
				bdata.Store(ipal[*(idata + line * dataPixelStride)]);
				++bdata;
			}
#endif // VEGA_OPPAINTER_SUPPORT

			line = VEGA_SAMPLE_INT(csy)+1;
			if (line < 0)
				line = 0;
			else if (line >= (int)imgbuf.height)
				line = imgbuf.height-1;

			if (!is_indexed)
			{
				bdata.Store(data.LoadRel(line * dataPixelStride + imgbuf.width - 1));
				++bdata;
				bdata.Store(data.LoadRel(line * dataPixelStride));
			}
#ifdef VEGA_OPPAINTER_SUPPORT
			else
			{
				bdata.Store(ipal[*(idata + line * dataPixelStride + imgbuf.width - 1)]);
				++bdata;
				bdata.Store(ipal[*(idata + line * dataPixelStride)]);

				is_indexed = false;
			}
#endif // VEGA_OPPAINTER_SUPPORT

			dataPixelStride = 2;
			data = tmpbuf.GetConstAccessor(0, 0);
			csx &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			csy &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			length = 1;
		}
		else if (wrapy)
		{
			int pix = VEGA_SAMPLE_INT(csx);
			if (pix < 0)
				pix = 0;
			else if (pix >= (int)imgbuf.width)
				pix = imgbuf.width-1;

			VEGASWBuffer tmpbuf;
			tmpbuf.CreateFromData(borderdata, 4*4, 4, 1);
			VEGAPixelAccessor bdata = tmpbuf.GetAccessor(0, 0);

			if (!is_indexed)
			{
				bdata.Store(data.LoadRel((imgbuf.height - 1) * dataPixelStride + pix));
				bdata += 2;
				bdata.Store(data.LoadRel(pix));
			}
#ifdef VEGA_OPPAINTER_SUPPORT
			else
			{
				bdata.Store(ipal[*(idata + (imgbuf.height - 1) * dataPixelStride + pix)]);
				bdata += 2;
				bdata.Store(ipal[*(idata + pix)]);
			}
#endif // VEGA_OPPAINTER_SUPPORT

			pix = VEGA_SAMPLE_INT(csx)+1;
			if (pix < 0)
				pix = 0;
			else if (pix >= (int)imgbuf.width)
				pix = imgbuf.width-1;

			if (!is_indexed)
			{
				bdata += -1;
				bdata.Store(data.LoadRel((imgbuf.height - 1) * dataPixelStride + pix));
				bdata += 2;
				bdata.Store(data.LoadRel(pix));
			}
#ifdef VEGA_OPPAINTER_SUPPORT
			else
			{
				bdata += -1;
				bdata.Store(ipal[*(idata + (imgbuf.height - 1) * dataPixelStride + pix)]);
				bdata += 2;
				bdata.Store(ipal[*(idata + pix)]);

				is_indexed = false;
			}
#endif // VEGA_OPPAINTER_SUPPORT

			dataPixelStride = 2;
			data = tmpbuf.GetConstAccessor(0, 0);
			csx &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			csy &= (1 << VEGA_SAMPLER_FRACBITS)-1;
			length = 1;
		}

		if (lastPixel > firstPixel)
		{
			// Find where the line intersects the image
			// The minimum positive of sx+dx*width = width-1 and sx-dx*width = 0
			int w;
			if (cdx)
			{
				w = 1+(VEGA_INTTOSAMPLE((int)imgbuf.width-1) - csx)/cdx;
				if (w>0 && w < length)
					length = w;
				w = 1+(0-csx)/cdx;
				if (w>0 && w < length)
					length = w;
			}
			if (cdy)
			{
				w = 1+(VEGA_INTTOSAMPLE((int)imgbuf.height-1) - csy)/cdy;
				if (w>0 && w < length)
					length = w;
				w = 1+(0-csy)/cdy;
				if (w>0 && w < length)
					length = w;
			}
		}
		if (pic_x != 0)
		{
			if (xspread == SPREAD_CLAMP_BORDER)
			{
				csx = 0;
				csy = 0;
				cdx = 0;
				cdy = 0;

				VEGASWBuffer tmpbuf;
				tmpbuf.CreateFromData(borderdata, 4*4, 4, 1);
				VEGAPixelAccessor tmp = tmpbuf.GetAccessor(0, 0);
				tmp.Store(ColorToVEGAPixel(borderColor));
				data.Reset(tmp.Ptr());

				is_indexed = false;
			}
			else
			{
				if (pic_x < 0)
					csx = 0;
				else
					csx = VEGA_INTTOSAMPLE(imgbuf.width-1);
				cdx = 0;
			}
		}
		if (pic_y != 0)
		{
			if (yspread == SPREAD_CLAMP_BORDER)
			{
				csx = 0;
				csy = 0;
				cdx = 0;
				cdy = 0;

				VEGASWBuffer tmpbuf;
				tmpbuf.CreateFromData(borderdata, 4*4, 4, 1);
				VEGAPixelAccessor tmp = tmpbuf.GetAccessor(0, 0);
				tmp.Store(ColorToVEGAPixel(borderColor));
				data.Reset(tmp.Ptr());

				is_indexed = false;
			}
			else
			{
				if (pic_y < 0)
					csy = 0;
				else
					csy = VEGA_INTTOSAMPLE(imgbuf.height-1);
				cdy = 0;
			}
		}

		unsigned cnt = length;

		// Perform the blending
#ifdef VEGA_OPPAINTER_SUPPORT
		if (is_indexed)
		{
			if (quality == QUALITY_LINEAR)
			{
				if (span.mask)
				{
					while (cnt-- > 0)
					{
						int a = *alpha++;
						if (a)
						{
							VEGA_PIXEL c = IndexedSamplerBilerpPM(idata, ipal, dataPixelStride, csx, csy);
							color.Store(VEGACompOverIn(color.Load(), c, a));
						}
						csx += cdx;
						csy += cdy;

						color++;
					}
				}
				else if (is_opaque)
				{
					while (cnt-- > 0)
					{
						VEGA_PIXEL c = IndexedSamplerBilerpPM(idata, ipal, dataPixelStride, csx, csy);
						color.Store(c);

						csx += cdx;
						csy += cdy;

						color++;
					}
				}
				else
				{
					while (cnt-- > 0)
					{
						VEGA_PIXEL c = IndexedSamplerBilerpPM(idata, ipal, dataPixelStride, csx, csy);
						color.Store(VEGACompOver(color.Load(), c));

						csx += cdx;
						csy += cdy;

						color++;
					}
				}
			}
			else
			{
				if (cdy == 0)
				{
					idata += VEGA_SAMPLE_INT(csy) * dataPixelStride;
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								VEGA_PIXEL c = IndexedSamplerNearestPM_1D(idata, ipal, csx);
								color.Store(VEGACompOverIn(color.Load(), c, a));
							}

							csx += cdx;

							color++;
						}
					}
					else if (is_opaque)
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = IndexedSamplerNearestPM_1D(idata, ipal, csx);
							color.Store(c);

							csx += cdx;

							color++;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = IndexedSamplerNearestPM_1D(idata, ipal, csx);
							color.Store(VEGACompOver(color.Load(), c));

							csx += cdx;

							color++;
						}
					}
				}
				else
				{
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								VEGA_PIXEL c = IndexedSamplerNearest(idata, ipal, dataPixelStride, csx, csy);
								color.Store(VEGACompOverIn(color.Load(), c, a));
							}

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
					else if (is_opaque)
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = IndexedSamplerNearest(idata, ipal, dataPixelStride, csx, csy);
							color.Store(c);

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = IndexedSamplerNearest(idata, ipal, dataPixelStride, csx, csy);
							color.Store(VEGACompOver(color.Load(), c));

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
				}
			}
		}
		else
#endif // VEGA_OPPAINTER_SUPPORT
		{
			if (quality == QUALITY_LINEAR)
			{
				if (span.mask)
					VEGASampler(SamplerLerpXY_CompOverMask,
								(color, data.Ptr(), alpha,
								 cnt, dataPixelStride, csx, cdx, csy, cdy));
				else if (is_opaque)
					VEGASampler(SamplerLerpXY_Opaque,
								(color, data.Ptr(),
								 cnt, dataPixelStride, csx, cdx, csy, cdy));
				else
					VEGASampler(SamplerLerpXY_CompOver,
								(color, data.Ptr(),
								 cnt, dataPixelStride, csx, cdx, csy, cdy));
			}
			else if (is_premultiplied_alpha)
			{
				if (cdy == 0/* && VEGA_SAMPLE_FRAC(csy) == 0*/)
				{
					data += VEGA_SAMPLE_INT(csy) * dataPixelStride;
					if (span.mask)
						VEGASampler(SamplerNearestX_CompOverMask, (color, data.Ptr(), alpha, cnt, csx, cdx));
					else if (is_opaque)
						VEGASampler(SamplerNearestX_Opaque, (color, data.Ptr(), cnt, csx, cdx));
					else
						VEGASampler(SamplerNearestX_CompOver, (color, data.Ptr(), cnt, csx, cdx));
				}
				else
				{
					if (span.mask)
					{
						VEGASampler(SamplerNearestXY_CompOverMask,
									(color, data.Ptr(), alpha,
									 cnt, dataPixelStride, csx, cdx, csy, cdy));
					}
					else if (is_opaque)
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = VEGASamplerNearestPM(data.Ptr(), dataPixelStride, csx, csy);
							color.Store(c);

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
					else
					{
						VEGASampler(SamplerNearestXY_CompOver,
									(color, data.Ptr(),
									 cnt, dataPixelStride, csx, cdx, csy, cdy));
					}
				}
			}
			else
			{
				if (cdy == 0/* && VEGA_SAMPLE_FRAC(csy) == 0*/)
				{
					data += VEGA_SAMPLE_INT(csy) * dataPixelStride;
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								VEGA_PIXEL c = VEGASamplerNearest_1D(data.Ptr(), csx);
								color.Store(VEGACompOverIn(color.Load(), c, a));
							}

							csx += cdx;

							color++;
						}
					}
					else if (is_opaque)
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = VEGASamplerNearest_1D(data.Ptr(), csx);
							color.Store(c);

							csx += cdx;

							color++;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = VEGASamplerNearest_1D(data.Ptr(), csx);
							color.Store(VEGACompOver(color.Load(), c));

							csx += cdx;

							color++;
						}
					}
				}
				else
				{
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								VEGA_PIXEL c = VEGASamplerNearest(data.Ptr(), dataPixelStride, csx, csy);
								color.Store(VEGACompOverIn(color.Load(), c, a));
							}

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
					else if (is_opaque)
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = VEGASamplerNearest(data.Ptr(), dataPixelStride, csx, csy);
							color.Store(c);

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							VEGA_PIXEL c = VEGASamplerNearest(data.Ptr(), dataPixelStride, csx, csy);
							color.Store(VEGACompOver(color.Load(), c));

							csx += cdx;
							csy += cdy;

							color++;
						}
					}
				}
			}
		}

		// Increase start_x, start_y and firstPixel
		firstPixel += length;
		sx += dx*length;
		sy += dy*length;
	}
}

OP_STATUS VEGAImage::prepare()
{
	OP_ASSERT(backingstore);

	if (!imgbuf_need_free)
	{
		backingstore->Validate();

		VEGASWBuffer* buffer = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;

		// FIXME: With the refactored blitter bits, it should be possible
		// to just use the pointer directly (and possibly 'cache'
		// width/height/et.c)
		imgbuf = *buffer;
	}
	return OpStatus::OK;
}

void VEGAImage::complete()
{
	if (!imgbuf_need_free)
	{
		OP_ASSERT(backingstore);

		backingstore->EndTransaction(FALSE);
	}
}

OP_STATUS VEGAImage::init(VEGABackingStore* store, bool premultiplied)
{
	reset();

	if (imgbuf_need_free)
	{
		OP_ASSERT(imgbuf.IsBacked());
		imgbuf.Destroy();
		imgbuf_need_free = false;
	}
	imgbuf.Reset();

	VEGARefCount::IncRef(store);
	VEGARefCount::DecRef(backingstore);
	backingstore = store;

	is_premultiplied_alpha = premultiplied;

#ifdef VEGA_3DDEVICE
	texture = NULL;
#endif // VEGA_3DDEVICE

	return OpStatus::OK;
}

OP_STATUS VEGAImage::init(OpBitmap* bmp)
{
	is_opaque = !(bmp->HasAlpha() || bmp->IsTransparent());

	bitmap = bmp;

#ifndef VEGA_OPPAINTER_SUPPORT
	VEGABackingStore_OpBitmap* bmp_store = OP_NEW(VEGABackingStore_OpBitmap, ());
	if (!bmp_store)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bmp_store->Construct(bmp);
	if (OpStatus::IsSuccess(status))
		status = init(bmp_store);

	VEGARefCount::DecRef(bmp_store);
	return status;
#else
	VEGABackingStore* bmp_store = static_cast<VEGAOpBitmap*>(bmp)->GetBackingStore();

	return init(bmp_store);
#endif // VEGA_OPPAINTER_SUPPORT
}

#ifdef VEGA_3DDEVICE
OP_STATUS VEGAImage::init(VEGA3dTexture* tex, bool premultiplied)
{
	reset();

	// Given the use case for this version of init(), I think it is
	// unlikely that the buffer is backed. If this triggers, it would
	// seem I was wrong...
	OP_ASSERT(!imgbuf.IsBacked());

	VEGARefCount::DecRef(backingstore);
	backingstore = NULL;

	is_premultiplied_alpha = premultiplied;
	is_opaque = false;

	texture = tex;

	return OpStatus::OK;
}

VEGA3dTexture* VEGAImage::getTexture()
{
	if (backingstore)
		backingstore->Validate();

	if (backingstore && !texture)
	{
		if (backingstore->IsA(VEGABackingStore::TEXTURE))
		{
			texture = static_cast<VEGABackingStore_Texture*>(backingstore)->GetTexture();
		}
		else if (backingstore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
		{
			VEGA3dRenderTarget* fbo = static_cast<VEGABackingStore_FBO*>(backingstore)->GetReadRenderTarget();
			if (fbo->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
				return NULL;

			texture = static_cast<VEGA3dFramebufferObject*>(fbo)->getAttachedColorTexture();
		}
		else
		{
			texture = NULL;
		}
	}
	else if (backingstore && backingstore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
	{
		// This call is required to ensure that multisampled FBOs update their read target before using the texture.
		// Not calling it causes old texture data to be used.
		static_cast<VEGABackingStore_FBO*>(backingstore)->GetReadRenderTarget();
	}
	// Set the correct wrap mode for the texture
	VEGA3dTexture::WrapMode xwrap, ywrap;
	switch (xspread)
	{
	case VEGAFill::SPREAD_REPEAT:
		xwrap = VEGA3dTexture::WRAP_REPEAT;
		break;
	case VEGAFill::SPREAD_REFLECT:
		xwrap = VEGA3dTexture::WRAP_REPEAT_MIRROR;
		break;
	case VEGAFill::SPREAD_CLAMP:
	default:
		xwrap = VEGA3dTexture::WRAP_CLAMP_EDGE;
		break;
	}
	switch (yspread)
	{
	case VEGAFill::SPREAD_REPEAT:
		ywrap = VEGA3dTexture::WRAP_REPEAT;
		break;
	case VEGAFill::SPREAD_REFLECT:
		ywrap = VEGA3dTexture::WRAP_REPEAT_MIRROR;
		break;
	case VEGAFill::SPREAD_CLAMP:
	default:
		ywrap = VEGA3dTexture::WRAP_CLAMP_EDGE;
		break;
	}
	VEGA3dTexture::FilterMode filter;
	switch (quality)
	{
	case QUALITY_NEAREST:
		filter = VEGA3dTexture::FILTER_NEAREST;
		break;
	case QUALITY_LINEAR:
	default:
		filter = VEGA3dTexture::FILTER_LINEAR;
	}
	if (texture)
	{
		texture->setWrapMode(xwrap, ywrap);
		texture->setFilterMode(filter, filter);
	}
	return texture;
}
#endif // VEGA_3DDEVICE

OP_STATUS VEGAImage::limitArea(int left, int top, int right, int bottom)
{
	if (is_premultiplied_alpha)
		return OpStatus::OK;

#ifdef VEGA_OPPAINTER_SUPPORT
	if (!backingstore || !imgbuf.IsBacked())
		return OpStatus::OK;
#endif // VEGA_OPPAINTER_SUPPORT

	// Adjust the rect to make sure the interpolation is valid
	// TODO: the border could potentially be used for clamping, to make sure that drawImage() does not include pixels outside the source rect when scaling
	--top;
	--left;
	++bottom;
	++right;
	if (top < 0)
		top = 0;
	if (left < 0)
		left = 0;

	int bs_width = backingstore->GetWidth();
	int bs_height = backingstore->GetHeight();
	if (bottom > bs_height-1)
		bottom = bs_height-1;
	if (right > bs_width-1)
		right = bs_width-1;

	if (!imgbuf_need_free)
	{
		VEGASWBuffer* buffer = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;

		imgbuf = *buffer;
	}

	VEGASWBuffer dst = imgbuf;
	bool allocated_buf = false;
	if (!imgbuf_need_free)
	{
		OP_STATUS status = dst.Create(imgbuf.width, imgbuf.height);
		if (OpStatus::IsError(status))
		{
			backingstore->EndTransaction(FALSE);
			return status;
		}
		allocated_buf = true;
	}

	dst.PremultiplyRect(imgbuf, left, top, right, bottom);

	if (!imgbuf_need_free)
		backingstore->EndTransaction(FALSE);

	if (allocated_buf)
	{
		imgbuf = dst;

		is_premultiplied_alpha = true;
		imgbuf_need_free = true;
	}
	return OpStatus::OK;
}

#endif // VEGA_SUPPORT
