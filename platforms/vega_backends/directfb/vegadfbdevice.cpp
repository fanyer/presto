/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_DIRECTFB
#include "modules/libvega/vegaconfig.h"

#ifdef VEGA_2DDEVICE
#include "platforms/vega_backends/directfb/vegadfbdevice.h"
#include "modules/libvega/src/vegapixelformat.h"

#include <directfb_version.h>

#ifdef DEBUG_ENABLE_OPASSERT
static inline VEGADFBSurface* GetVEGADFBSurface(VEGA2dSurface* s)
{
	OP_ASSERT(s);

	if (s->getType() == VEGA2dSurface::VEGA2D_WINDOW)
		s = static_cast<VEGADFBWindow*>(s)->getWindowSurface();

	return static_cast<VEGADFBSurface*>(s);
}

static inline void AssertSurfaceUnlocked(VEGA2dSurface* s)
{
	VEGADFBSurface* vsurf = GetVEGADFBSurface(s);

	OP_ASSERT(!vsurf->locked);
}
#else
static inline void AssertSurfaceUnlocked(VEGA2dSurface* s) {}
#endif // DEBUG_ENABLE_OPASSERT

VEGADFBSurface::~VEGADFBSurface()
{
	OP_ASSERT(!locked);
	VEGA2dDevice* dev2d = g_vegaGlobals.vega2dDevice;
	if (dev2d && this == dev2d->getRenderTarget())
		dev2d->setRenderTarget(NULL);
	
	if (dfb_surface && owns_surface)
		dfb_surface->Release(dfb_surface);
}

VEGADFBDevice::VEGADFBDevice() :
	compositeOp(COMPOSITE_SRC_OVER), interpMethod(INTERPOLATE_BILINEAR),
	dfb_if(NULL), target(NULL), dfb_target(NULL),
	dev_width(0), dev_height(0)
{
	col_r = col_g = col_b = 0;
	col_a = 255;
}

void VEGADFBDevice::flush(const OpRect* update_rects, unsigned int num_rects)
{
	if (!target)
		return;

	AssertSurfaceUnlocked(target);

// 	dfb_if->WaitIdle(dfb_if);
	if (target->getType() == VEGA2dSurface::VEGA2D_WINDOW)
		static_cast<VEGA2dWindow*>(target)->present(update_rects, num_rects);
}

OP_STATUS VEGADFBDevice::resize(unsigned int width, unsigned int height)
{
	dev_width = width;
	dev_height = height;

	setClipRect(0, 0, width, height);
	return OpStatus::OK;
}

void VEGADFBDevice::setColor(UINT8 r, UINT8 g, UINT8 b, UINT8 a)
{
	col_r = r;
	col_g = g;
	col_b = b;
	col_a = a;
}

void VEGADFBDevice::setClipRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	dfb_clip.x1 = x;
	dfb_clip.y1 = y;
	dfb_clip.x2 = x+w-1;
	dfb_clip.y2 = y+h-1;
}

void VEGADFBDevice::getClipRect(unsigned int& x, unsigned int& y, unsigned int& w, unsigned int& h)
{
	x = dfb_clip.x1;
	y = dfb_clip.y1;
	w = dfb_clip.x2 - dfb_clip.x1 + 1;
	h = dfb_clip.y2 - dfb_clip.y1 + 1;
}

void VEGADFBDevice::clear(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	AssertSurfaceUnlocked(target);

	DFBRegion tmp_cr;
	tmp_cr.x1 = x;
	tmp_cr.y1 = y;
	tmp_cr.x2 = x+w-1;
	tmp_cr.y2 = y+h-1;

	// FIXME: Optimize 'entire surface' case
	dfb_target->SetClip(dfb_target, &tmp_cr);
	dfb_target->Clear(dfb_target, col_r, col_g, col_b, col_a);
}

static const DFBSurfacePorterDuffRule g_compop_to_dfbrule[] = {
	DSPD_SRC		/* COMPOSITE_SRC */,
	DSPD_SRC_OVER	/* COMPOSITE_SRC_OVER */,
	DSPD_SRC		/* COMPOSITE_SRC_CLEAR - filler */,
	DSPD_SRC_OVER	/* COMPOSITE_COLORIZE */,
	DSPD_SRC_OVER	/* COMPOSITE_SRC_OPACITY */
};

static const unsigned g_compop_to_dfbdraw[] = {
	DSDRAW_NOFX		/* COMPOSITE_SRC */,
	DSDRAW_BLEND	/* COMPOSITE_SRC_OVER */,
	DSDRAW_NOFX		/* COMPOSITE_SRC_OVER */,
	DSDRAW_BLEND	/* COMPOSITE_COLORIZE */,
	DSDRAW_BLEND	/* COMPOSITE_SRC_OPACITY */
};

static const unsigned g_compop_to_dfbblit[] = {
	DSBLIT_NOFX					/* COMPOSITE_SRC */,
	DSBLIT_BLEND_ALPHACHANNEL	/* COMPOSITE_SRC_OVER */,
	DSBLIT_NOFX					/* COMPOSITE_SRC_CLEAR */,
	DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE /* COMPOSITE_COLORIZE */,
	DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA /* COMPOSITE_SRC_OPACITY */
};

static inline DFBSurfaceDrawingFlags GetDrawingFlags(VEGA2dDevice::CompositeOperator op)
{
	int flags = g_compop_to_dfbdraw[op];

#ifndef USE_PREMULTIPLIED_ALPHA
	if (flags != DSDRAW_NOFX)
		flags |= DSDRAW_SRC_PREMULTIPLY | DSDRAW_DST_PREMULTIPLY | DSDRAW_DEMULTIPLY;
#endif // USE_PREMULTIPLIED_ALPHA

	return (DFBSurfaceDrawingFlags)flags;
}

static inline DFBSurfaceBlittingFlags GetBlittingFlags(VEGA2dDevice::CompositeOperator op)
{
	int flags = g_compop_to_dfbblit[op];

#ifdef USE_PREMULTIPLIED_ALPHA
	if (op == VEGA2dDevice::COMPOSITE_SRC_OPACITY)
		flags |= DSBLIT_SRC_PREMULTCOLOR;
#else
	if (flags != DSBLIT_NOFX)
		flags |= DSBLIT_SRC_PREMULTIPLY | DSBLIT_DST_PREMULTIPLY | DSBLIT_DEMULTIPLY;
#endif // USE_PREMULTIPLIED_ALPHA

	return (DFBSurfaceBlittingFlags)flags;
}

OP_STATUS VEGADFBDevice::fillRect(int x, int y, unsigned int w, unsigned int h)
{
	AssertSurfaceUnlocked(target);

	dfb_target->SetPorterDuff(dfb_target, g_compop_to_dfbrule[compositeOp]);
	dfb_target->SetDrawingFlags(dfb_target, GetDrawingFlags(compositeOp));

	dfb_target->SetClip(dfb_target, &dfb_clip);
	dfb_target->SetColor(dfb_target, col_r, col_g, col_b, col_a);
	dfb_target->FillRectangle(dfb_target, x, y, w, h);
	return OpStatus::OK;
}

static inline IDirectFBSurface* GetDFBSurface(VEGA2dSurface* s)
{
	if (s->getType() == VEGA2dSurface::VEGA2D_WINDOW)
		s = static_cast<VEGADFBWindow*>(s)->getWindowSurface();

	return static_cast<VEGADFBSurface*>(s)->dfb_surface;
}

OP_STATUS VEGADFBDevice::copy(VEGA2dSurface* src,
							  unsigned int src_x, unsigned int src_y,
							  unsigned int src_w, unsigned int src_h,
							  unsigned int dst_x, unsigned int dst_y)
{
	AssertSurfaceUnlocked(src);
	AssertSurfaceUnlocked(target);

	DFBRectangle src_rect;
	src_rect.x = src_x;
	src_rect.y = src_y;
	src_rect.w = src_w;
	src_rect.h = src_h;

	IDirectFBSurface* dfb_src = GetDFBSurface(src);

	dfb_target->SetBlittingFlags(dfb_target, DSBLIT_NOFX);

	dfb_target->SetClip(dfb_target, NULL);
	dfb_target->Blit(dfb_target, dfb_src, &src_rect, dst_x, dst_y);
	return OpStatus::OK;
}

OP_STATUS VEGADFBDevice::blit(VEGA2dSurface* src,
							  unsigned int src_x, unsigned int src_y,
							  unsigned int src_w, unsigned int src_h,
							  int dst_x, int dst_y)
{
	AssertSurfaceUnlocked(src);
	AssertSurfaceUnlocked(target);

	IDirectFBSurface* dfb_src = GetDFBSurface(src);

	if (compositeOp == COMPOSITE_COLORIZE || compositeOp == COMPOSITE_SRC_OPACITY)
		dfb_target->SetColor(dfb_target, col_r, col_g, col_b, col_a);

	dfb_target->SetPorterDuff(dfb_target, g_compop_to_dfbrule[compositeOp]);
	dfb_target->SetBlittingFlags(dfb_target, GetBlittingFlags(compositeOp));
	dfb_target->SetClip(dfb_target, &dfb_clip);

	DFBRectangle src_rect;
	src_rect.x = src_x;
	src_rect.y = src_y;
	src_rect.w = src_w;
	src_rect.h = src_h;

	dfb_target->Blit(dfb_target, dfb_src, &src_rect, dst_x, dst_y);

	return OpStatus::OK;
}

#if DIRECTFB_MAJOR_VERSION >= 1 && DIRECTFB_MINOR_VERSION >= 2
// # define DFB_HAS_SETSOURCEMASK // Has SetSourceMask - version dependent - 1.2 (1.1.1?) and onwards
#endif // DirectFB version >= 1.2

OP_STATUS VEGADFBDevice::blit(VEGA2dSurface* src, VEGA2dSurface* mask,
							  unsigned int src_x, unsigned int src_y,
							  unsigned int src_w, unsigned int src_h,
							  int dst_x, int dst_y)
{
	AssertSurfaceUnlocked(src);
	AssertSurfaceUnlocked(mask);
	AssertSurfaceUnlocked(target);

	OP_ASSERT(mask->getFormat() == VEGA2dSurface::FORMAT_INTENSITY8);
	OP_ASSERT(src->getWidth() == mask->getWidth());
	OP_ASSERT(src->getHeight() == mask->getHeight());

	IDirectFBSurface* dfb_src = GetDFBSurface(src);

	if (compositeOp == COMPOSITE_COLORIZE || compositeOp == COMPOSITE_SRC_OPACITY)
		dfb_target->SetColor(dfb_target, col_r, col_g, col_b, col_a);

	DFBRectangle src_rect;
	src_rect.x = src_x;
	src_rect.y = src_y;
	src_rect.w = src_w;
	src_rect.h = src_h;

	if (compositeOp == COMPOSITE_SRC_CLEAR)
	{
		dfb_target->SetBlittingFlags(dfb_target, GetBlittingFlags(COMPOSITE_SRC_OVER));

		// dst = dst out mask ["clear dst where the mask is non-transparent"]
		dfb_target->SetPorterDuff(dfb_target, DSPD_DST_OUT);
		dfb_target->Blit(dfb_target, GetDFBSurface(mask), &src_rect, dst_x, dst_y);

		// dst = dst over src
		dfb_target->SetPorterDuff(dfb_target, DSPD_DST_OVER);
		dfb_target->Blit(dfb_target, GetDFBSurface(src), &src_rect, dst_x, dst_y);
		return OpStatus::OK;
	}

	// Apply mask
	int blitflags = GetBlittingFlags(compositeOp);
#ifdef DFB_HAS_SETSOURCEMASK
	dfb_target->SetSourceMask(dfb_target, GetDFBSurface(mask), 0, 0, DSMF_NONE);

	blitflags |= DSBLIT_SRC_MASK_ALPHA;
#else
	VEGA2dSurface* masked_src = NULL;
	RETURN_IF_ERROR(createSurface(&masked_src, src_w, src_h, src->getFormat()));

	IDirectFBSurface* dfb_msrc = GetDFBSurface(masked_src);
	dfb_msrc->SetBlittingFlags(dfb_msrc, DSBLIT_NOFX);
	dfb_msrc->Blit(dfb_msrc, dfb_src, &src_rect, 0, 0);

	dfb_msrc->SetPorterDuff(dfb_msrc, DSPD_DST_IN);
	dfb_msrc->SetBlittingFlags(dfb_msrc, GetBlittingFlags(COMPOSITE_SRC_OVER));
	dfb_msrc->Blit(dfb_msrc, GetDFBSurface(mask), &src_rect, 0, 0);

	dfb_src = dfb_msrc;

	src_rect.x = 0;
	src_rect.y = 0;
#endif // DFB_HAS_SETSOURCEMASK

	dfb_target->SetPorterDuff(dfb_target, g_compop_to_dfbrule[compositeOp]);
	dfb_target->SetBlittingFlags(dfb_target, (DFBSurfaceBlittingFlags)blitflags);
	dfb_target->SetClip(dfb_target, &dfb_clip);

	dfb_target->Blit(dfb_target, dfb_src, &src_rect, dst_x, dst_y);

#ifndef DFB_HAS_SETSOURCEMASK
	OP_DELETE(masked_src);
#endif // !DFB_HAS_SETSOURCEMASK
	return OpStatus::OK;
}

static inline void ApplyClipRect(const DFBRegion* clip, DFBRectangle* dest)
{
	OpRect opclip(clip->x1, clip->y1, clip->x2-clip->x1+1, clip->y2-clip->y1+1);
	OpRect opdest(dest->x, dest->y, dest->w, dest->h);
	opdest.IntersectWith(opclip);
	dest->x = opdest.x;
	dest->y = opdest.y;
	dest->w = opdest.width;
	dest->h = opdest.height;
}

// This is basically the stretchblit clip function in directfb with the
// important difference in that it clamps the src width and height to 1.
// In directfb this is not done and since the source width/height easily
// can end up being zero (1) the hardware blitter will bail out and
// directfb will fall back to its software rendering.
// (1) Ie, if the following expression is less than one: src->w * clipped_dst->w / dst->w
static inline void ApplyStretchBlitClip(const DFBRegion* clip, DFBRectangle *src_rect, DFBRectangle *dst_rect)
{
	DFBRectangle orig_dst = *dst_rect;
	DFBRectangle orig_src = *src_rect;
	ApplyClipRect(clip, dst_rect);

	if (dst_rect->x != orig_dst.x)
		src_rect->x += (int)((dst_rect->x - orig_dst.x) * (src_rect->w / (float)orig_dst.w));

	if (dst_rect->y != orig_dst.y)
		src_rect->y += (int)((dst_rect->y - orig_dst.y) * (src_rect->h / (float)orig_dst.h));

	if (dst_rect->w != orig_dst.w)
		src_rect->w = (int)(src_rect->w * (dst_rect->w / (float)orig_dst.w));

	if (dst_rect->h != orig_dst.h)
		src_rect->h = (int)(src_rect->h * (dst_rect->h / (float)orig_dst.h));

	// Clamp to 1 if the original width and height was above zero.
	if (src_rect->w == 0 && orig_src.w > 0)
		src_rect->w = 1;

	if (src_rect->h == 0 && orig_src.h > 0)
		src_rect->h = 1;
}

OP_STATUS VEGADFBDevice::stretchBlit(VEGA2dSurface* src,
									 unsigned int src_x, unsigned int src_y,
									 unsigned int src_w, unsigned int src_h,
									 int dst_x, int dst_y, unsigned int dst_w, unsigned int dst_h)
{
	AssertSurfaceUnlocked(src);
	AssertSurfaceUnlocked(target);

	DFBRectangle src_rect;
	src_rect.x = src_x;
	src_rect.y = src_y;
	src_rect.w = src_w;
	src_rect.h = src_h;
	DFBRectangle dst_rect;
	dst_rect.x = dst_x;
	dst_rect.y = dst_y;
	dst_rect.w = dst_w;
	dst_rect.h = dst_h;

	IDirectFBSurface* dfb_src = GetDFBSurface(src);

	if (compositeOp == COMPOSITE_COLORIZE || compositeOp == COMPOSITE_SRC_OPACITY)
		dfb_target->SetColor(dfb_target, col_r, col_g, col_b, col_a);

	dfb_target->SetPorterDuff(dfb_target, g_compop_to_dfbrule[compositeOp]);
	dfb_target->SetBlittingFlags(dfb_target, GetBlittingFlags(compositeOp));

	// Do clipping ourself to avoid DirectFB using the software renderer
	// due to bad precision handling in the clip calculation in directfb.
	// See comments in the ApplyStretchBlit function.
	dfb_target->SetClip(dfb_target, NULL);
	ApplyStretchBlitClip(&dfb_clip, &src_rect, &dst_rect);

#if DIRECTFB_MAJOR_VERSION > 1 || (DIRECTFB_MAJOR_VERSION == 1 && DIRECTFB_MINOR_VERSION >= 2)
	if (!DirectFBCheckVersion (1, 2, 0))
	{
		int ropts = DSRO_NONE;
		if (interpMethod == INTERPOLATE_BILINEAR)
			ropts = DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE;

		dfb_target->SetRenderOptions(dfb_target, (DFBSurfaceRenderOptions)ropts);
	}
#endif

	dfb_target->StretchBlit(dfb_target, dfb_src, &src_rect, &dst_rect);

	// Restore clip to what is expected to have been used.
	dfb_target->SetClip(dfb_target, &dfb_clip);
	return OpStatus::OK;
}

OP_STATUS VEGADFBDevice::tileBlit(VEGA2dSurface* src,
					   			  const OpRect& src_rect,
					   			  const OpRect& tile_area,
					   			  const OpPoint& offset,
					   			  unsigned tile_w, unsigned tile_h)
{
	AssertSurfaceUnlocked(src);
	AssertSurfaceUnlocked(target);

	if (src_rect.IsEmpty() || tile_area.IsEmpty() || tile_w == 0 || tile_h == 0)
		return OpStatus::OK;

	// Use clipping to constrain the blitting to the tiling_area.
	DFBRegion clip;
	clip.x1 = tile_area.x;
	clip.y1 = tile_area.y;
	clip.x2 = tile_area.x + tile_area.width-1;
	clip.y2 = tile_area.y + tile_area.height-1;

	// Intersect it with the current clip rectangle.
	clip.x1 = MAX(clip.x1, dfb_clip.x1);
	clip.x2 = MIN(clip.x2, dfb_clip.x2);
	clip.y1 = MAX(clip.y1, dfb_clip.y1);
	clip.y2 = MIN(clip.y2, dfb_clip.y2);

	// DirectFB does not handle negative clip rects very well, at least not in TileBlit.
	if (clip.x2 < clip.x1 || clip.y2 < clip.y1)
		return OpStatus::OK;

	// Offset the tiling if requested.
	int dst_x = tile_area.x - offset.x;
	int dst_y = tile_area.y - offset.y;

	DFBRectangle dfb_sr;
	dfb_sr.x = src_rect.x;
	dfb_sr.y = src_rect.y;
	dfb_sr.w = src_rect.width;
	dfb_sr.h = src_rect.height;

	// DirectFB does not support scaling while tiling so before tiling can proceed
	// we must create a separate scaled version of the src surface.
	VEGA2dSurface* tile_surface = NULL;
	if (tile_w != (unsigned)src_rect.width || tile_h != (unsigned)src_rect.height)
	{
		RETURN_IF_ERROR(createSurface(&tile_surface, tile_w, tile_h, VEGA2dSurface::FORMAT_RGBA));

		// Save the old states.
		VEGA2dSurface* old_rt = getRenderTarget();
		CompositeOperator old_compOp = getCompositeOperator();
		DFBRegion old_clipRect = dfb_clip;

		// Prepare for stretchblit.
		setRenderTarget(tile_surface);
		setClipRect(0, 0, tile_w, tile_h);
		setCompositeOperator(COMPOSITE_SRC);

		OP_STATUS stat = stretchBlit(src, src_rect.x, src_rect.y, src_rect.width, src_rect.height, 0, 0, tile_w, tile_h);

		// Restore states.
		setRenderTarget(old_rt);
		setCompositeOperator(old_compOp);
		dfb_clip = old_clipRect;

		if (OpStatus::IsError(stat))
		{
			OP_DELETE(tile_surface);
			return stat;
		}

		// Adjust source rectangle to the tile surface.
		dfb_sr.x = 0;
		dfb_sr.y = 0;
		dfb_sr.w = tile_w;
		dfb_sr.h = tile_h;

		// Use the tile surface as source from now on.
		src = tile_surface;
	}

	if (dfb_sr.w != 1 && dfb_sr.h != 1)
	{
		// Regular tiling.
		IDirectFBSurface* dfb_src = GetDFBSurface(src);

		if (compositeOp == COMPOSITE_COLORIZE || compositeOp == COMPOSITE_SRC_OPACITY)
			dfb_target->SetColor(dfb_target, col_r, col_g, col_b, col_a);

		dfb_target->SetPorterDuff(dfb_target, g_compop_to_dfbrule[compositeOp]);
		dfb_target->SetBlittingFlags(dfb_target, GetBlittingFlags(compositeOp));

		dfb_target->SetClip(dfb_target, &clip);

		dfb_target->TileBlit(dfb_target, dfb_src, &dfb_sr, dst_x, dst_y);
	}
	else
	{
		// Stretch if possible, at least one side should be 1px if we end up here.
		if (dfb_sr.w == 1)
			tile_w = tile_area.width + offset.x;
		if (dfb_sr.h == 1)
			tile_h = tile_area.height + offset.y;

		// Adjust dst_x/y so that the first tile is within the clip rect.
		if (dst_x < clip.x1)
			dst_x += ((clip.x1 - dst_x) / tile_w) * tile_w;
		if (dst_y < clip.y1)
			dst_y += ((clip.y1 - dst_y) / tile_h) * tile_h;

		DFBRegion old_clipRect = dfb_clip;
		InterpolationMethod	old_interpMethod = interpMethod;
		dfb_clip = clip;
		interpMethod = INTERPOLATE_NEAREST;

		for (int x = dst_x; x <= clip.x2; x += tile_w)
			for (int y = dst_y; y <= clip.y2; y += tile_h)
				stretchBlit(src, dfb_sr.x, dfb_sr.y, dfb_sr.w, dfb_sr.h, x, y, tile_w, tile_h);

		dfb_clip = old_clipRect;
		interpMethod = old_interpMethod;
	}

	OP_DELETE(tile_surface);

	return OpStatus::OK;
}

OP_STATUS VEGADFBDevice::drawGlyphs(const GlyphInfo* glyphs, unsigned glyph_count)
{
	SurfaceData sdata;
	sdata.x = 0;
	sdata.y = 0;
	sdata.w = target->getWidth();
	sdata.h = target->getHeight();

	OP_ASSERT(compositeOp == COMPOSITE_COLORIZE);

	RETURN_IF_ERROR(lockRect(&sdata, READ_INTENT | WRITE_INTENT));

	OP_ASSERT(sdata.pixels.format == VPSF_BGRA8888);

	VEGAPixelFormat_BGRA8888::Pointer dstptr;
	dstptr.rgba = (UINT32*)sdata.pixels.buffer;

	unsigned dststride = sdata.pixels.stride / 4;
	UINT32 srccol = (col_a << 24) | (col_r << 16) | (col_g << 8) | col_b;

	for (unsigned idx = 0; idx < glyph_count; ++idx)
	{
		const GlyphInfo& cglyph = glyphs[idx];

		int minx = MAX(cglyph.x, dfb_clip.x1);
		int miny = MAX(cglyph.y, dfb_clip.y1);
		int maxx = MIN(cglyph.x + cglyph.w - 1, dfb_clip.x2);
		int maxy = MIN(cglyph.y + cglyph.h - 1, dfb_clip.y2);

		if (minx > maxx || miny > maxy)
			continue;

		VEGAPixelFormat_BGRA8888::Pointer cur_dstptr = dstptr + miny * dststride + minx;

		const UINT8* srcp = cglyph.buffer + (miny - cglyph.y) * cglyph.buffer_stride + (minx - cglyph.x);
		unsigned span_len = maxx - minx + 1;
		for (int yp = miny; yp <= maxy; ++yp)
		{
			VEGAPixelFormat_BGRA8888::CompositeOverIn(cur_dstptr, srccol, srcp, span_len);

			cur_dstptr = cur_dstptr + dststride;
			srcp += cglyph.buffer_stride;
		}
	}

	unlockRect(&sdata);
	return OpStatus::OK;
}

static inline DFBSurfacePixelFormat VEGAToDFBFormat(VEGA2dSurface::Format fmt)
{
	if (fmt == VEGA2dSurface::FORMAT_INTENSITY8)
		return DSPF_A8;

	OP_ASSERT(fmt == VEGA2dSurface::FORMAT_RGBA);
	return DSPF_ARGB;
}

OP_STATUS VEGADFBDevice::createSurface(VEGA2dSurface** surface, unsigned int width, unsigned int height,
									   VEGA2dSurface::Format format)
{
	VEGADFBSurface* vsurf = allocateSurface(width, height);
	if (!vsurf)
		return OpStatus::ERR_NO_MEMORY;

	DFBSurfaceDescription desc;
	op_memset(&desc, 0, sizeof(desc));
	desc.flags = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
	desc.width = width;
	desc.height = height;
	desc.pixelformat = VEGAToDFBFormat(format);

	desc.flags = (DFBSurfaceDescriptionFlags)(desc.flags | DSDESC_CAPS);
	desc.caps = DSCAPS_NONE;
#if 0 //def USE_PREMULTIPLIED_ALPHA
//	desc.caps = (DFBSurfaceCapabilities)(desc.caps | DSCAPS_PREMULTIPLIED);
#endif // USE_PREMULTIPLIED_ALPHA

	desc.caps = (DFBSurfaceCapabilities)(desc.caps | DSCAPS_VIDEOONLY);

	IDirectFBSurface* dfb_surf = NULL;
	DFBResult dres = dfb_if->CreateSurface(dfb_if, &desc, &dfb_surf);

#ifdef VEGA_BACKENDS_LAZY_DIRECTFB_ALLOCATION
	if (dres == DFB_OK)
	{
		void* surf_data;
		int pitch;
		dres = dfb_surf->Lock(dfb_surf, (DFBSurfaceLockFlags)(DSLF_WRITE), &surf_data, &pitch);
		if(dres == DFB_OK)
			dfb_surf->Unlock(dfb_surf);
		else
			dfb_surf->Release(dfb_surf); // the if() below will provide fallback mechanism		
	}
#endif // VEGA_BACKENDS_LAZY_DIRECTFB_ALLOCATION
	if (dres != DFB_OK)
	{
		// Failed to create a surface in VRAM, try getting a software surface
		// instead.
		// NOTE: We might want to fail here and use our software rendering instead of
		// directfb's internal fallback, or try to free up some VRAM by asking Opera
		// to trim the image cache.
		desc.caps = (DFBSurfaceCapabilities)(desc.caps & (~DSCAPS_VIDEOONLY));
		dres = dfb_if->CreateSurface(dfb_if, &desc, &dfb_surf);
		if (dres != DFB_OK)
		{
			// Signal OOM?
			OP_DELETE(vsurf);
			return OpStatus::ERR;
		}
	}

// 	dfb_surf->DisableAcceleration(dfb_surf, DFXL_ALL);

	vsurf->dfb_surface = dfb_surf;
	vsurf->vfmt = format;

	*surface = vsurf;
	return OpStatus::OK;
}

OP_STATUS VEGADFBDevice::createSurface(VEGA2dSurface** surface, IDirectFBSurface* dfb_surface)
{
	if (!dfb_surface)
		return OpStatus::ERR;

	int width;
	int height;

	DFBResult dres = dfb_surface->GetSize(dfb_surface, &width, &height);
	if (dres != DFB_OK)
		return OpStatus::ERR;

	VEGADFBSurface* vsurf = allocateSurface(width, height);
	if (!vsurf)
		return OpStatus::ERR_NO_MEMORY;

	vsurf->dfb_surface = dfb_surface;
	vsurf->vfmt = VEGA2dSurface::FORMAT_RGBA;
	vsurf->owns_surface = false;

	*surface = vsurf;
	return OpStatus::OK;
}

OP_STATUS VEGADFBDevice::setRenderTarget(VEGA2dSurface* rt)
{
	if (target == rt)
		return OpStatus::OK;

	target = rt;
	dfb_target = rt ? GetDFBSurface(rt) : NULL;

	return OpStatus::OK;
}

static inline DFBSurfaceLockFlags TranslateLockFlags(unsigned intent)
{
	int flags = 0;
	if (intent & VEGA2dDevice::WRITE_INTENT)
		flags |= DSLF_WRITE;
	if (intent & VEGA2dDevice::READ_INTENT)
		flags |= DSLF_READ;

	return (DFBSurfaceLockFlags)flags;
}

OP_STATUS VEGADFBDevice::lockRect(SurfaceData* data, unsigned intent)
{
	int stride;
	void* bufptr;

	DFBSurfacePixelFormat pix_fmt;
	DFBResult dres = dfb_target->GetPixelFormat(dfb_target, &pix_fmt);
	if (dres != DFB_OK || pix_fmt != DSPF_ARGB)
		return OpStatus::ERR;

	dres = dfb_target->Lock(dfb_target, TranslateLockFlags(intent), &bufptr, &stride);
	if (dres != DFB_OK)
		return OpStatus::ERR;

	OP_ASSERT(stride > 0);

	unsigned pix_bpp = DFB_BYTES_PER_PIXEL(pix_fmt);
	UINT8* buf8 = (UINT8*)bufptr + data->y * stride + data->x * pix_bpp;
	data->pixels.buffer = (UINT32*)buf8;
	data->pixels.stride = stride;
	data->pixels.format = VPSF_BGRA8888;
	data->pixels.width = data->w;
	data->pixels.height = data->h;
#ifdef DEBUG_ENABLE_OPASSERT
	data->privateData = dfb_target;
	GetVEGADFBSurface(target)->locked=true;
#endif // DEBUG_ENABLE_OPASSERT

	return OpStatus::OK;
}

void VEGADFBDevice::unlockRect(SurfaceData* data)
{
	OP_ASSERT(data->privateData == dfb_target);

	dfb_target->Unlock(dfb_target);

#ifdef DEBUG_ENABLE_OPASSERT
	GetVEGADFBSurface(target)->locked=false;
#endif // DEBUG_ENABLE_OPASSERT
}

VEGADFBSurface* VEGADFBDevice::allocateSurface(unsigned int width, unsigned int height)
{
	return OP_NEW(VEGADFBSurface, (width, height));
}


#endif // VEGA_2DDEVICE
#endif // VEGA_BACKEND_DIRECTFB
