/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegabackend_sw.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegastencil.h"
#include "modules/libvega/vegawindow.h"
#include "modules/libvega/vegapixelstore.h"

#include "modules/libvega/src/vegagradient.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/src/vegarendertargetimpl.h"
#include "modules/libvega/src/vegacomposite.h"
#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/vegarenderer.h"

#include "modules/libvega/src/vegafilterdisplace.h"

#include "modules/mdefont/processedstring.h"

#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libvega/src/oppainter/vegaopfont.h"
# include "modules/libvega/src/oppainter/vegaopbitmap.h"
#else
# include "modules/pi/OpBitmap.h"
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGABackend_SW::init(unsigned int w, unsigned int h, unsigned int q)
{
	RETURN_IF_ERROR(rasterizer.initialize(w, h));

	rasterizer.setConsumer(this);
	rasterizer.setQuality(q);

	maskscratch = rasterizer.getMaskScratch();
	return OpStatus::OK;
}

OP_STATUS VEGABackend_SW::updateQuality()
{
	rasterizer.setQuality(quality);
	return OpStatus::OK;
}

bool VEGABackend_SW::bind(VEGARenderTarget* rt)
{
	VEGABackingStore* bstore = rt->GetBackingStore();
	if (!bstore || !bstore->IsA(VEGABackingStore::SWBUFFER))
		// Would be very surprising if we got a NULL backing store
		return false;

	bstore->Validate();

	buffer = static_cast<VEGABackingStore_SWBuffer*>(bstore)->GetBuffer();
	return true;
}

void VEGABackend_SW::unbind()
{
	buffer = NULL;
}

OP_STATUS VEGABackingStore_SWBuffer::Construct(unsigned width, unsigned height, VEGASWBufferType type, bool opaque)
{
	return m_buffer.Create(width, height, type, opaque);
}

VEGABackingStore_SWBuffer::~VEGABackingStore_SWBuffer()
{
	m_buffer.Destroy();
}

void VEGABackingStore_SWBuffer::SetColor(const OpRect& rect, UINT32 color)
{
	m_buffer.FillRect(rect.x, rect.y, rect.width, rect.height, ColorToVEGAPixel(color));
}

OP_STATUS VEGABackingStore_SWBuffer::CopyRect(const OpPoint& dstp, const OpRect& srcr,
											  VEGABackingStore* store)
{
	if (!store->IsA(VEGABackingStore::SWBUFFER))
		return FallbackCopyRect(dstp, srcr, store);

	VEGABackingStore_SWBuffer* sw_store = static_cast<VEGABackingStore_SWBuffer*>(store);
	VEGASWBuffer* srcbuf = &sw_store->m_buffer;

	VEGAPixelAccessor dst = m_buffer.GetAccessor(dstp.x, dstp.y);
	unsigned dstPixelStride = m_buffer.GetPixelStride();

	if (srcbuf->IsIndexed())
	{
		const VEGA_PIXEL* pal = srcbuf->GetPalette();

		for (int line = 0; line < srcr.height; ++line)
		{
			const UINT8* isrc = srcbuf->GetIndexedPtr(srcr.x, line + srcr.y);

			for (int i = 0; i < srcr.width; i++)
			{
				VEGA_PIXEL px = pal[isrc[i]];

				unsigned a = VEGA_UNPACK_A(px);
				unsigned r = VEGA_UNPACK_R(px);
				unsigned g = VEGA_UNPACK_G(px);
				unsigned b = VEGA_UNPACK_B(px);

				dst.StoreARGB(a, r, g, b);
				dst++;
			}

			dst -= srcr.width;
			dst += dstPixelStride;
		}
	}
	else
	{
		VEGAConstPixelAccessor src = srcbuf->GetConstAccessor(srcr.x, srcr.y);
		unsigned srcPixelStride = srcbuf->GetPixelStride();

		for (int line = 0; line < srcr.height; ++line)
		{
			dst.CopyFrom(src.Ptr(), srcr.width);

			dst += dstPixelStride;
			src += srcPixelStride;
		}
	}
	return OpStatus::OK;
}

#ifdef VEGA_OPPAINTER_SUPPORT
VEGABackingStore_WindowBuffer::~VEGABackingStore_WindowBuffer()
{
	if (m_window && m_buffer.IsBacked())
		m_buffer.Unbind(m_window->getPixelStore());

	m_buffer.Reset();
}

OP_STATUS VEGABackingStore_WindowBuffer::Construct(VEGAWindow* window)
{
	m_window = window;

	UpdateBuffer();
	return OpStatus::OK;
}

unsigned VEGABackingStore_WindowBuffer::GetWidth() const
{
	return m_window->getWidth();
}

unsigned VEGABackingStore_WindowBuffer::GetHeight() const
{
	return m_window->getHeight();
}

void VEGABackingStore_WindowBuffer::Validate()
{
	UpdateBuffer();
}

void VEGABackingStore_WindowBuffer::Flush(const OpRect* update_rects, unsigned int num_rects)
{
	m_buffer.Sync(m_window->getPixelStore(), update_rects, num_rects);
}

void VEGABackingStore_WindowBuffer::UpdateBuffer()
{
	OP_ASSERT(m_window);

	VEGAPixelStore* ps = m_window->getPixelStore();
	if (!m_buffer.IsBacked())
	{
		m_buffer.Bind(ps, true);
	}
	else
	{
		if (ps->width != m_buffer.width || ps->height != m_buffer.height ||
			!m_ps_buf  || ps->buffer != m_ps_buf)
		{
			m_buffer.Unbind(ps);
			m_buffer.Bind(ps, true);
		}
	}
	m_ps_buf = ps->buffer;
}

VEGABackingStore_PixelStore::~VEGABackingStore_PixelStore()
{
	if (m_buffer.IsBacked())
		m_buffer.Unbind(m_pixelStore);

	m_buffer.Reset();
}

OP_STATUS VEGABackingStore_PixelStore::Construct()
{
	return m_buffer.Bind(m_pixelStore);
}

void VEGABackingStore_PixelStore::Validate()
{
	if (!m_buffer.IsBacked())
	{
		m_buffer.Bind(m_pixelStore);
	}
	else
	{
		if (m_pixelStore->width != m_buffer.width || m_pixelStore->height != m_buffer.height)
		{
			m_buffer.Unbind(m_pixelStore);
			m_buffer.Bind(m_pixelStore);
		}
	}
}

void VEGABackingStore_PixelStore::Flush(const OpRect* update_rects, unsigned int num_rects)
{
	m_buffer.Sync(m_pixelStore, update_rects, num_rects);
}

void VEGABackingStore_PixelStore::Invalidate()
{
	m_buffer.Reload(m_pixelStore);
}
#endif // VEGA_OPPAINTER_SUPPORT

#ifndef VEGA_OPPAINTER_SUPPORT
VEGABackingStore_OpBitmap::~VEGABackingStore_OpBitmap()
{
	if (!m_using_pointer)
		return;

	m_buffer.Reset();

	m_bitmap->ReleasePointer();
}

OP_STATUS VEGABackingStore_OpBitmap::Construct(OpBitmap* bitmap)
{
	RETURN_IF_ERROR(m_buffer.CreateFromBitmap(bitmap, m_using_pointer));

	m_bitmap = bitmap;
	return OpStatus::OK;
}

void VEGABackingStore_OpBitmap::Flush(const OpRect* update_rects, unsigned int num_rects)
{
	if (!m_bitmap)
		return;

	if (m_using_pointer)
		return;

	if (!m_buffer.IsBacked())
		return;

	if (update_rects)
	{
		for (unsigned int i = 0; i < num_rects; i++)
			OpStatus::Ignore(m_buffer.CopyToBitmap(m_bitmap, update_rects[i].x, update_rects[i].y,
												   update_rects[i].x, update_rects[i].y,
												   update_rects[i].width, update_rects[i].height));
	}
	else
	{
		OpStatus::Ignore(m_buffer.CopyToBitmap(m_bitmap, 0, 0, 0, 0));
	}
}
#endif // !VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGABackend_SW::createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp)
{
#ifdef VEGA_OPPAINTER_SUPPORT
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (static_cast<VEGAOpBitmap*>(bmp)->isTiled())
	{
		OP_ASSERT(!"this operation is not allowed on tiled bitmaps, check calling code");
		return OpStatus::ERR; // better than crashing
	}
# endif // VEGA_LIMIT_BITMAP_SIZE
	VEGABackingStore* bstore = static_cast<VEGAOpBitmap*>(bmp)->GetBackingStore();
	VEGARefCount::IncRef(bstore);
#else
	VEGABackingStore_OpBitmap* bstore = OP_NEW(VEGABackingStore_OpBitmap, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(bmp);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	VEGABitmapRenderTarget* rend = OP_NEW(VEGABitmapRenderTarget, (bstore));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = rend;
	return OpStatus::OK;
}

#ifdef VEGA_OPPAINTER_SUPPORT
OP_STATUS VEGABackend_SW::createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window)
{
	RETURN_IF_ERROR(window->initSoftwareBackend());

	if (!window->getPixelStore())
		return OpStatus::ERR;

	VEGABackingStore_WindowBuffer* wstore = OP_NEW(VEGABackingStore_WindowBuffer, ());
	if (!wstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = wstore->Construct(window);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(wstore);
		return status;
	}

	VEGAWindowRenderTarget* winrt = OP_NEW(VEGAWindowRenderTarget, (wstore, window));
	if (!winrt)
	{
		VEGARefCount::DecRef(wstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = winrt;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_SW::createBufferRenderTarget(VEGARenderTarget** rt, VEGAPixelStore* ps)
{
	if (!ps)
		return OpStatus::ERR_NULL_POINTER;

	VEGABackingStore_PixelStore* bstore = OP_NEW(VEGABackingStore_PixelStore, (ps));
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct();
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}

	VEGAIntermediateRenderTarget* rend = OP_NEW(VEGAIntermediateRenderTarget, (bstore, VEGARenderTarget::RT_RGBA32));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = rend;
	return OpStatus::OK;
}
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGABackend_SW::createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h)
{
	VEGABackingStore_SWBuffer* bstore = OP_NEW(VEGABackingStore_SWBuffer, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h, VSWBUF_COLOR, false);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}

	VEGAIntermediateRenderTarget* rend = OP_NEW(VEGAIntermediateRenderTarget, (bstore, VEGARenderTarget::RT_RGBA32));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = rend;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_SW::createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h)
{
	VEGABackingStore_SWBuffer* bstore = OP_NEW(VEGABackingStore_SWBuffer, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h, component ? VSWBUF_COLOR : VSWBUF_STENCIL, false);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}

	VEGARenderTarget::RTColorFormat fmt;
	fmt = component ? VEGARenderTarget::RT_RGBA32 : VEGARenderTarget::RT_ALPHA8;

	VEGAIntermediateRenderTarget* rend = OP_NEW(VEGAIntermediateRenderTarget, (bstore, fmt));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*sten = rend;

	bstore->SetColor(OpRect(0, 0, w, h), 0);

	return OpStatus::OK;
}

/* static */
OP_STATUS VEGABackend_SW::createBitmapStore(VEGABackingStore** store,
											unsigned w, unsigned h, bool is_indexed, bool is_opaque)
{
	VEGABackingStore_SWBuffer* sw_store = OP_NEW(VEGABackingStore_SWBuffer, ());
	if (!sw_store)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = sw_store->Construct(w, h, is_indexed ?
										   VSWBUF_INDEXED_COLOR : VSWBUF_COLOR, is_opaque);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(sw_store);
		return status;
	}
	*store = sw_store;
	return OpStatus::OK;
}

bool VEGABackend_SW::supportsStore(VEGABackingStore* store)
{
	return store->IsA(VEGABackingStore::SWBUFFER) ? true : false;
}

#ifdef CANVAS3D_SUPPORT
#include "modules/libvega/vega3ddevice.h"

OP_STATUS VEGABackend_SW::createImage(VEGAImage* img, VEGA3dTexture* tex, bool isPremultiplied)
{
	VEGABackingStore_SWBuffer* sw_store = OP_NEW(VEGABackingStore_SWBuffer, ());
	if (!sw_store)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = sw_store->Construct(tex->getWidth(), tex->getHeight(), VSWBUF_COLOR, false);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(sw_store);
		return status;
	}

	VEGABackingStore* store = sw_store;
	VEGASWBuffer* buffer = store->BeginTransaction(VEGABackingStore::ACC_WRITE_ONLY);
	if (!buffer)
	{
		VEGARefCount::DecRef(store);
		return OpStatus::ERR_NO_MEMORY;
	}

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	VEGA3dFramebufferObject* fbo = NULL;
	status = device->createFramebuffer(&fbo);
	if (OpStatus::IsSuccess(status))
		status = fbo->attachColor(tex);
	if (OpStatus::IsSuccess(status))
		status = device->setRenderTarget(fbo);

	if (OpStatus::IsSuccess(status))
	{
		VEGA3dDevice::FramebufferData imgdata;
		imgdata.x = 0;
		imgdata.y = 0;
		imgdata.w = tex->getWidth();
		imgdata.h = tex->getHeight();

		status = device->lockRect(&imgdata, true);
		if (OpStatus::IsSuccess(status))
		{
			buffer->CopyFromPixelStore(&imgdata.pixels);

			status = img->init(store, !!isPremultiplied);

			device->unlockRect(&imgdata, false);
		}
	}
	VEGARefCount::DecRef(fbo);

	store->EndTransaction(TRUE);

	// VEGAImage references backing store
	VEGARefCount::DecRef(store);

	return status;
}
#endif // CANVAS3D_SUPPORT

OP_STATUS VEGABackend_SW::fillPrimitive(struct VEGAPrimitive* prim, VEGAStencil* stencil)
{
	if (prim->type == VEGAPrimitive::RECTANGLE ||
		prim->type == VEGAPrimitive::ROUNDED_RECT_UNIFORM)
	{
		unsigned mtxcls = VEGA_XFRMCLS_INT_TRANSLATION | VEGA_XFRMCLS_POS_SCALE;
		if (prim->transform)
			mtxcls = prim->transform->classify();

		// Only allow optimization if scaled and/or
		// translated. Disallow negative scaling.
		if ((mtxcls & ~(VEGA_XFRMCLS_SCALE | VEGA_XFRMCLS_POS_SCALE |
						VEGA_XFRMCLS_TRANSLATION | VEGA_XFRMCLS_INT_TRANSLATION)) == 0 &&
			(mtxcls & VEGA_XFRMCLS_POS_SCALE) != 0)
		{
			FillState saved_fillstate = fillstate;
			if (fillstate.fill)
			{
#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
				// Stencils are not supported by the current drawImage
				// functions.
				if (stencil)
					return OpStatus::ERR;

				VEGA_FIX x = prim->data.rect.x;
				VEGA_FIX y = prim->data.rect.y;
				VEGA_FIX w = prim->data.rect.width;
				VEGA_FIX h = prim->data.rect.height;

				if (prim->transform)
				{
					const VEGATransform& t = *prim->transform;

					// Apply transform to values - transform is assumed to be of
					// the form [ a 0 c ; 0 e f ] {a, e > 0} when we get here.
					t.apply(x, y);
					t.applyVector(w, h);
				}

				VEGAImage* img = getDrawableImageFromFill(fillstate.fill, x, y, w, h);
				if (!img)
					return OpStatus::ERR;

				fillstate.fill = img;
#else
				if (fillstate.fill->isImage())
					return OpStatus::ERR;
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
			}

			OP_STATUS stat = OpStatus::ERR;
			if (prim->type == VEGAPrimitive::RECTANGLE)
				stat = fillFractionalRect(prim, stencil);
			else
				stat = fillSlicedRoundedRect(prim, stencil);

			fillstate = saved_fillstate;

			return stat;
		}
	}

	return OpStatus::ERR;
}

void VEGABackend_SW::clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform)
{
	if (transform)
	{
		clearTransformed(x, y, w, h, color, transform);
		return;
	}

	// Modify the clipping rectangle to not be outside the render target
	int cliprect_ex = buffer->width;
	if (this->cliprect_ex < cliprect_ex)
		cliprect_ex = this->cliprect_ex;
	int cliprect_ey = buffer->height;
	if (this->cliprect_ey < cliprect_ey)
		cliprect_ey = this->cliprect_ey;

	int ex = MIN(x+(int)w, cliprect_ex);
	int ey = MIN(y+(int)h, cliprect_ey);

	int sx = MAX(x, cliprect_sx);
	int sy = MAX(y, cliprect_sy);

	if (sx >= ex || sy >= ey)
		return;

	if (color == 0 && buffer->GetBufferType() == VSWBUF_STENCIL)
	{
		VEGAStencil* stencil = static_cast<VEGAStencil*>(renderTarget);

		int d_left, d_right, d_top, d_bottom;
		stencil->getDirtyRect(d_left, d_right, d_top, d_bottom);

		if (d_right < d_left || d_bottom < d_top)
			return;

		ex = MIN(ex, d_right+1);
		ey = MIN(ey, d_bottom+1);
		sx = MAX(sx, d_left);
		sy = MAX(sy, d_top);

		if (sx >= ex || sy >= ey)
			return;

		int new_left = d_left;
		int new_top = d_top;
		int new_right = d_right;
		int new_bottom = d_bottom;

		if (sy == d_top && ey == d_bottom+1)
		{
			if (sx == d_left)
				new_left = ex;
			if (ex == d_right+1)
				new_right = sx-1;
		}
		if (sx == d_left && ex == d_right+1)
		{
			if (sy == d_top)
				new_top = ey;
			if (ey == d_bottom+1)
				new_bottom = sy-1;
		}

		stencil->resetDirtyRect();
		if (new_left <= new_right && new_top <= new_bottom)
			stencil->markDirty(new_left, new_right, new_top, new_bottom);
	}
	else
		renderTarget->markDirty(sx, ex-1, sy, ey-1);

	buffer->FillRect(sx, sy, ex - sx, ey - sy, ColorToVEGAPixel(color));
}

OP_STATUS VEGABackend_SW::applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region)
{
	if (!filter->hasSource())
		return OpStatus::ERR;

	return filter->apply(*buffer, region);
}

#ifdef VEGA_OPPAINTER_SUPPORT
OP_STATUS VEGABackend_SW::drawString(VEGAFont* font, int x, int y, const ProcessedString* processed_string, VEGAStencil* stencil)
{
	OP_ASSERT(processed_string);

	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)buffer->width)
		ex = buffer->width;
	if (ey > (int)buffer->height)
		ey = buffer->height;

	if (fillstate.fill)
		RETURN_IF_ERROR(fillstate.fill->prepare());

	prepareStencil(stencil);

	BOOL draw_simple = (fillstate.fill == NULL) && (stencil == NULL);
	OP_STATUS status = OpStatus::OK;

	unsigned dstPixelStride = buffer->GetPixelStride();
	for (unsigned int c = 0; c < processed_string->m_length; ++c)
	{
		int xpos = x + processed_string->m_processed_glyphs[c].m_pos.x;
		int ypos = y + processed_string->m_processed_glyphs[c].m_pos.y;

		VEGAFont::VEGAGlyph glyph;
		glyph.advance = 0;

		const UINT8* src;
		unsigned int srcStride;
		OP_STATUS s = font->getGlyph(processed_string->m_processed_glyphs[c].m_id, glyph, src, srcStride, processed_string->m_is_glyph_indices);
		if (OpStatus::IsSuccess(s) &&
			glyph.width>0 && glyph.height>0)
		{
			int starty = 0;
			int endy = glyph.height;
			if (ypos-glyph.top < sy)
				starty = sy+glyph.top-ypos;
			if (endy+ypos-glyph.top > ey)
				endy = ey+glyph.top-ypos;

			int startx = 0;
			int endx = glyph.width;
			if (xpos+glyph.left < sx)
				startx = sx-glyph.left-xpos;
			if (endx+xpos+glyph.left > ex)
				endx = ex-glyph.left-xpos;

			if (starty < endy && startx < endx)
			{
				const UINT8* srcp = src;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
				if (font->UseSubpixelRendering())
					srcp += starty * srcStride*4 + startx*4;
				else
#endif // VEGA_SUBPIXEL_FONT_BLENDING
					srcp += starty * srcStride + startx;

				if (draw_simple)
				{
					VEGAPixelAccessor dst = buffer->GetAccessor(startx + xpos + glyph.left,
															 starty + ypos - glyph.top);

					unsigned span_len = endx - startx;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
					if (font->UseSubpixelRendering())
					{
						for (int yp = starty; yp < endy; ++yp)
						{
							for (unsigned xp = 0; xp < span_len; ++xp)
							{
								UINT32 col = ((UINT32*)srcp)[xp];
								
								int da, dr, dg, db;
								dst.LoadUnpack(da, dr, dg, db);
								
								if (da == 255)
								{
									UINT32 r = (col>>16)&0xff;
									UINT32 g = (col>>8)&0xff;
									UINT32 b = col&0xff;
									++r;
									++g;
									++b;
									int fa = VEGA_UNPACK_A(fillstate.ppixel);
									int fr = VEGA_UNPACK_R(fillstate.ppixel);
									int fg = VEGA_UNPACK_G(fillstate.ppixel);
									int fb = VEGA_UNPACK_B(fillstate.ppixel);
									dr = ((fr*r)>>8)+((dr*(256-((r*fa)>>8)))>>8);
									dg = ((fg*g)>>8)+((dg*(256-((g*fa)>>8)))>>8);
									db = ((fb*b)>>8)+((db*(256-((b*fa)>>8)))>>8);
									dst.StoreRGB(dr, dg, db);
								}
								else
								{
									UINT8 a = col>>24;
									VEGACompOverIn(dst.Ptr(), fillstate.ppixel, &a, 1);
								}
								++dst;
							}
							dst += dstPixelStride-span_len;
							srcp += srcStride*4;
						}
					}
					else
#endif
					{
						for (int yp = starty; yp < endy; ++yp)
						{
							VEGACompOverIn(dst.Ptr(), fillstate.ppixel, srcp, span_len);
							dst += dstPixelStride;
							srcp += srcStride;
						}
					}
				}
				else
				{
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
					if (font->UseSubpixelRendering())
					{
						int width = endx - startx;
						int height = endy - starty;
						UINT8* srcptmp = OP_NEWA_WH(UINT8, width, height);
						if (srcptmp)
						{
							for (int yp = starty; yp < endy; ++yp)
							{
								for (int xp = startx; xp < endx; ++xp)
								{
									UINT32 col = ((const UINT32*)src)[yp*srcStride+xp];
									srcptmp[(yp-starty) * width + (xp-startx)] = col>>24;
								}
							}
							rasterizer.rasterRectMask(startx + xpos + glyph.left,
													  starty + ypos - glyph.top,
													  width, height, srcptmp, width);
							OP_DELETEA(srcptmp);
						}
					}
					else
#endif
					{
						rasterizer.rasterRectMask(startx + xpos + glyph.left,
												  starty + ypos - glyph.top,
												  endx - startx, endy - starty, srcp, srcStride);
					}
				}
			}
		}
		else if (OpStatus::IsError(s) && !OpStatus::IsMemoryError(status))
			status = s;
	}

	finishStencil(stencil);

	if (fillstate.fill)
		fillstate.fill->complete();

	return status;
}
#if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
OP_STATUS VEGABackend_SW::drawString(VEGAFont* font, int x, int y, const uni_char* _text, UINT32 len, INT32 extra_char_space, short adjust, VEGAStencil* stencil)
{
#  ifdef VEGA_NATIVE_FONT_SUPPORT
	if (font->nativeRendering())
	{
		OpRect clipRect(cliprect_sx,cliprect_sy,cliprect_ex-cliprect_sx,cliprect_ey-cliprect_sy);
		if (stencil == NULL)
		{
			OpPoint p(x, y);
			VEGAWindow* target_window = renderTarget->getTargetWindow();
			if (target_window && font->DrawString(p, _text, len, extra_char_space, adjust, clipRect, fillstate.color, target_window))
				return OpStatus::OK;
			else if (font->DrawString(p, _text, len, extra_char_space, adjust, clipRect, fillstate.color, buffer))
				return OpStatus::OK;
		}

		clipRect.x -= x;
		clipRect.y -= y;

		unsigned int srcStride, srcPixStride;
		const UINT8* src;
		OpPoint srcSize;
		if (!font->GetAlphaMask(_text, len, extra_char_space, adjust, clipRect, &src, &srcSize, &srcStride, &srcPixStride))
			return OpStatus::ERR;

		unsigned dstPixelStride = buffer->GetPixelStride();

		if (fillstate.fill)
			RETURN_IF_ERROR(fillstate.fill->prepare());

		prepareStencil(stencil);

		BOOL draw_simple = (fillstate.fill == NULL) && (stencil == NULL);

		int sx = cliprect_sx;
		int sy = cliprect_sy;
		int ex = cliprect_ex;
		int ey = cliprect_ey;

		// Modify the clipping rectangle to not be outside the render target
		if (ex > (int)buffer->width)
			ex = buffer->width;
		if (ey > (int)buffer->height)
			ey = buffer->height;

		int starty = 0;
		int endy = srcSize.y;
		if (y < sy)
			starty = sy-y;
		if (endy+y > ey)
			endy = ey-y;

		int startx = 0;
		int endx = srcSize.x;
		if (x < sx)
			startx = sx-x;
		if (endx+x > ex)
			endx = ex-x;

		if (starty < endy && startx < endx)
		{
			if (draw_simple)
			{
				VEGAPixelAccessor dst = buffer->GetAccessor(startx+x, starty+y);
				src += starty * srcStride + startx * srcPixStride;

				for (int yp = starty; yp < endy; ++yp)
				{
					const UINT8* srcp = src;
					for (int xp = startx; xp < endx; ++xp)
					{
						VEGACompOverIn(dst.Ptr(), fillstate.ppixel, srcp, 1);
						srcp += srcPixStride;
						++dst;
					}

					dst += dstPixelStride - (endx - startx);
					src += srcStride;
				}
			}
			else
			{
				UINT8* scratch = rasterizer.getMaskScratch();
				src += starty * srcStride + startx * srcPixStride;

				// We have a scratch buffer the same size as the width
				// of the renderer, so make use of it
				unsigned span_length = endx - startx;
				unsigned spans_per_loop = this->width / span_length;

				int yp = starty;
				while (yp < endy)
				{
					UINT8* mask = scratch;

					unsigned spans_to_copy = MIN(spans_per_loop, (unsigned)(endy - yp));
					unsigned rem_span_cnt = spans_to_copy;
					while (rem_span_cnt--)
					{
						const UINT8* srcp = src;

						unsigned cnt = span_length;
						while (cnt--)
						{
							*mask = *srcp;

							mask++;
							srcp += srcPixStride;
						}
						src += srcStride;
					}

					rasterizer.rasterRectMask(startx + x, yp + y, span_length, spans_to_copy,
											  scratch, span_length);

					yp += spans_to_copy;
				}
			}
		}

		finishStencil(stencil);

		if (fillstate.fill)
			fillstate.fill->complete();

		return OpStatus::OK;
	}
#  endif // VEGA_NATIVE_FONT_SUPPORT
	return OpStatus::ERR;
}
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS

OP_STATUS VEGABackend_SW::drawImage(VEGAImage* img, const VEGADrawImageInfo& imginfo)
{
	// Tiling is not supported by this function so we should never arrive here
	// with that set.
	OP_ASSERT(imginfo.type!=VEGADrawImageInfo::REPEAT);

	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)buffer->width)
		ex = buffer->width;
	if (ey > (int)buffer->height)
		ey = buffer->height;

	// clip the rect
	if (imginfo.dstx > sx)
		sx = imginfo.dstx;
	if (imginfo.dsty > sy)
		sy = imginfo.dsty;
	if (imginfo.dstx + (int)imginfo.dstw < ex)
		ex = imginfo.dstx + imginfo.dstw;
	if (imginfo.dsty + (int)imginfo.dsth < ey)
		ey = imginfo.dsty + imginfo.dsth;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	unsigned buf_flags = 0;
#ifdef USE_PREMULTIPLIED_ALPHA
	buf_flags |= BUF_PREMULTIPLIED;
#else
	if (imginfo.quality == VEGAFill::QUALITY_LINEAR)
	{
		if (!img->hasPremultipliedAlpha() && !img->isIndexed())
			img->makePremultiplied();
	}
	buf_flags |= img->hasPremultipliedAlpha() ? BUF_PREMULTIPLIED : 0;
#endif // USE_PREMULTIPLIED_ALPHA
	buf_flags |= img->isOpaque() ? BUF_OPAQUE : 0;

	RETURN_IF_ERROR(img->prepare());
	OP_STATUS err = VEGAImage::drawImage(buffer, sx, sy, ex - sx, ey - sy,
	                                     img->GetSWBuffer(), imginfo, buf_flags);
	img->complete();
	RETURN_IF_ERROR(err);

	r_minx = sx;
	r_miny = sy;
	r_maxx = ex-1;
	r_maxy = ey-1;

	renderTarget->markDirty(r_minx, r_maxx, r_miny, r_maxy);

	return OpStatus::OK;
}
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VEGABackend_SW::moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy)
{
	// Clip the destination to the clipping rect
	x += dx;
	y += dy;

	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)buffer->width)
		ex = buffer->width;
	if (ey > (int)buffer->height)
		ey = buffer->height;

	// clip the rect
	if (x > sx)
		sx = x;
	if (y > sy)
		sy = y;
	if (x+(int)width < ex)
		ex = x+width;
	if (y+(int)height < ey)
		ey = y+height;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	x = sx-dx;
	y = sy-dy;
	width = ex-sx;
	height = ey-sy;

	// Make sure source is valid
	if (dx > 0 && x < 0)
	{
		width += x;
		x = 0;
	}
	else if (dx < 0 && x+width > buffer->width)
	{
		width = buffer->width-x;
	}
	if (dy > 0 && y < 0)
	{
		height += y;
		y = 0;
	}
	else if (dy < 0 && y+height > buffer->height)
	{
		height = buffer->height-y;
	}

	buffer->MoveRect(x, y, width, height, dx, dy);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_SW::fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend)
{
#ifndef VEGA_OPPAINTER_SUPPORT
	if (!blend)
	{
		// Emulate the 'source' composite operation by first clearing to 0 and then doing 'over' as per normal.
		// "dst = 0; dst = src + (1 - src.a) * dst" simplifies to "dst = src".
		clear(x, y, width, height, 0, NULL);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)buffer->width)
		ex = buffer->width;
	if (ey > (int)buffer->height)
		ey = buffer->height;

	// clip the rect
	if (x > sx)
		sx = x;
	if (y > sy)
		sy = y;
	if (x+(int)width < ex)
		ex = x+width;
	if (y+(int)height < ey)
		ey = y+height;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	x = sx;
	y = sy;
	width = ex-sx;
	height = ey-sy;

	// Fully opaque color
	if (!stencil && !fillstate.fill && fillstate.color>>24 == 0xff)
	{
		buffer->FillRect(x, y, width, height, fillstate.ppixel);

		renderTarget->markDirty(x, x+width-1, y, y+height-1);
		return OpStatus::OK;
	}

	if (fillstate.fill)
		RETURN_IF_ERROR(fillstate.fill->prepare());

	prepareStencil(stencil);

	// FIXME: this can be optimized a lot(?)

	r_minx = width-1;
	r_miny = height-1;
	r_maxx = 0;
	r_maxy = 0;

#ifdef VEGA_OPPAINTER_SUPPORT
	bool saved_opaque = false;
	if (!blend)
	{
		OP_ASSERT(static_cast<VEGAImage*>(fillstate.fill)->isImage());
		saved_opaque = static_cast<VEGAImage*>(fillstate.fill)->isOpaque();
		static_cast<VEGAImage*>(fillstate.fill)->setOpaque(true);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	rasterizer.rasterRect(x, y, width, height);

	finishStencil(stencil);

	if (fillstate.fill)
		fillstate.fill->complete();

#ifdef VEGA_OPPAINTER_SUPPORT
	if (!blend)
		static_cast<VEGAImage*>(fillstate.fill)->setOpaque(saved_opaque);
#endif // VEGA_OPPAINTER_SUPPORT

	if (r_maxx >= r_minx && r_maxy >= r_miny)
		renderTarget->markDirty(r_minx, r_maxx, r_miny, r_maxy);

	return OpStatus::OK;
}

OP_STATUS VEGABackend_SW::invertRect(int x, int y, unsigned int width, unsigned int height)
{
	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)buffer->width)
		ex = buffer->width;
	if (ey > (int)buffer->height)
		ey = buffer->height;

	// clip the rect
	if (x > sx)
		sx = x;
	if (y > sy)
		sy = y;
	if (x+(int)width < ex)
		ex = x+width;
	if (y+(int)height < ey)
		ey = y+height;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	x = sx;
	y = sy;
	width = ex-sx;
	height = ey-sy;

	for (unsigned int yp = 0; yp < height; ++yp)
	{
		VEGAPixelAccessor acc = buffer->GetAccessor(x, y+yp);
		for (unsigned int xp = 0; xp < width; ++xp)
		{
			int r, g, b, a;
			acc.LoadUnpack(a, r, g, b);
#ifdef USE_PREMULTIPLIED_ALPHA
			r = a-r;
			g = a-g;
			b = a-b;
#else
			r = 255-r;
			g = 255-g;
			b = 255-b;
#endif
			acc.StoreARGB(a, r, g, b);
			++acc;
		}
	}

	renderTarget->markDirty(x, x+width-1, y, y+height-1);
	return OpStatus::OK;
}


unsigned VEGABackend_SW::calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy)
{
	return rasterizer.calculateArea(minx, miny, maxx, maxy);
}

OP_STATUS VEGABackend_SW::fillPath(VEGAPath *path, VEGAStencil *stencil)
{
	r_minx = width-1;
	r_miny = height-1;
	r_maxx = 0;
	r_maxy = 0;

	// Modify the clipping rectangle to not be outside the render target
	int cliprect_ex = buffer->width;
	if (this->cliprect_ex < cliprect_ex)
		cliprect_ex = this->cliprect_ex;
	int cliprect_ey = buffer->height;
	if (this->cliprect_ey < cliprect_ey)
		cliprect_ey = this->cliprect_ey;
	int cliprect_sx = this->cliprect_sx;
	int cliprect_sy = this->cliprect_sy;

	if (stencil)
	{
		int left, right, top, bottom;
		stencil->getDirtyRect(left, right, top, bottom);
		if (right < left || bottom < top)
			return OpStatus::OK;
		left += stencil->getOffsetX();
		right += stencil->getOffsetX();
		top += stencil->getOffsetY();
		bottom += stencil->getOffsetY();
		if (left > cliprect_sx)
			cliprect_sx = left;
		if (right+1 < cliprect_ex)
			cliprect_ex = right+1;
		if (top > cliprect_sy)
			cliprect_sy = top;
		if (bottom+1 < cliprect_ey)
			cliprect_ey = bottom+1;
	}
	if (cliprect_ex <= cliprect_sx || cliprect_ey <= cliprect_sy)
		return OpStatus::OK;

	RETURN_IF_ERROR(path->close(false));

	if (fillstate.fill)
		RETURN_IF_ERROR(fillstate.fill->prepare());

	prepareStencil(stencil);

	rasterizer.setXORFill(xorFill);
	rasterizer.setRegion(cliprect_sx, cliprect_sy, cliprect_ex, cliprect_ey);

	RETURN_IF_ERROR(rasterizer.rasterize(path));

	finishStencil(stencil);

	if (fillstate.fill)
		fillstate.fill->complete();

	if (r_maxx >= r_minx && r_maxy >= r_miny)
		renderTarget->markDirty(r_minx, r_maxx, r_miny, r_maxy);

	return OpStatus::OK;
}

void VEGABackend_SW::prepareStencil(VEGAStencil* stencil)
{
	if (stencil)
	{
		context.stencil_buffer = stencil->GetBackingStore()->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);

		context.stencil_ofs_x = stencil->getOffsetX();
		context.stencil_ofs_y = stencil->getOffsetY();
		context.stencil_is_a8 = stencil->getColorFormat() == VEGARenderTarget::RT_ALPHA8;
	}
	else
	{
		context.stencil_buffer = NULL;
	}
}

void VEGABackend_SW::finishStencil(VEGAStencil* stencil)
{
	if (context.stencil_buffer)
		stencil->GetBackingStore()->EndTransaction(FALSE);
}

static inline unsigned ColorToLuminance(VEGA_PIXEL c)
{
	unsigned int lum = VEGA_UNPACK_R(c)*54 + VEGA_UNPACK_G(c)*183 + VEGA_UNPACK_B(c)*18;
#ifdef USE_PREMULTIPLIED_ALPHA
	lum /= 255;
#else
	lum *= VEGA_UNPACK_A(c);
	lum /= 255*255;
#endif // USE_PREMULTIPLIED_ALPHA
	return lum;
}

void VEGABackend_SW::maskOpaqueSpan(VEGASpanInfo& span)
{
	VEGASWBuffer* buf = context.stencil_buffer;
	int ofs_x = (int)span.pos - context.stencil_ofs_x;
	int ofs_y = (int)span.scanline - context.stencil_ofs_y;

	if (context.stencil_is_a8)
	{
		// Calculate offset and point straight into the stencil
		span.mask = buf->GetStencilPtr(ofs_x, ofs_y);
	}
	else
	{
		// Convert component mask to alpha mask
		VEGAConstPixelAccessor sbuf = buf->GetConstAccessor(ofs_x, ofs_y);

		UINT8* tmp = maskscratch;

		unsigned cnt = span.length;
		while (cnt-- > 0)
		{
			*tmp++ = (UINT8)ColorToLuminance(sbuf.Load());
			++sbuf;
		}

		span.mask = maskscratch;
	}
}

void VEGABackend_SW::maskNonOpaqueSpan(VEGASpanInfo& span)
{
	VEGASWBuffer* buf = context.stencil_buffer;
	int ofs_x = (int)span.pos - context.stencil_ofs_x;
	int ofs_y = (int)span.scanline - context.stencil_ofs_y;

	if (context.stencil_is_a8)
	{
		const UINT8 *stenbuf = buf->GetStencilPtr(ofs_x, ofs_y);

		UINT8* tmp = maskscratch;
		const UINT8* mask = span.mask;

		unsigned cnt = span.length;
		while (cnt-- > 0)
		{
			unsigned m = *mask++;
			unsigned s = *stenbuf++;

			*tmp++ = (m * s) / 255;
		}

		span.mask = maskscratch;
	}
	else
	{
		VEGAConstPixelAccessor sbuf = buf->GetConstAccessor(ofs_x, ofs_y);

		UINT8* tmp = maskscratch;
		const UINT8* mask = span.mask;

		unsigned cnt = span.length;
		while (cnt-- > 0)
		{
			unsigned m = *mask++;
			unsigned lum = ColorToLuminance(sbuf.Load());
			++sbuf;

			*tmp++ = (m * lum) / 255;
		}

		span.mask = maskscratch;
	}
}

void VEGABackend_SW::drawSpans(VEGASpanInfo* raster_spans, unsigned int span_count)
{
	// (Mask &) Draw spans
	for (unsigned int span_num = 0; span_num < span_count; ++span_num)
	{
		VEGASpanInfo& span = raster_spans[span_num];

		if (context.stencil_buffer)
		{
			if (span.mask)
				maskNonOpaqueSpan(span);
			else
				maskOpaqueSpan(span);

			// Trim the resulting span
			while (span.length && !*span.mask)
			{
				++span.mask;
				++span.pos;
				span.length--;
			}

			const UINT8* span_last = span.mask + span.length - 1;
			while (span.length && !*span_last)
			{
				--span.length;
				--span_last;
			}
		}

		if (span.length == 0)
			continue;

		if (span.pos < r_minx)
			r_minx = span.pos;
		if (span.pos+span.length-1 > r_maxx)
			r_maxx = span.pos+span.length-1;
		if (span.scanline < r_miny)
			r_miny = span.scanline;
		if (span.scanline > r_maxy)
			r_maxy = span.scanline;

		// Render to non-component-based stencil
		if (renderTarget->getColorFormat() == VEGARenderTarget::RT_ALPHA8)
		{
			UINT8 *stenbuf = buffer->GetStencilPtr(span.pos, span.scanline);

			if (!span.mask)
			{
				op_memset(stenbuf, 0xff, span.length);
			}
			else if (fillstate.alphaBlend)
			{
				for (unsigned int i = 0; i < span.length; ++i)
				{
					unsigned int na = span.mask[i] + stenbuf[i];
					if (na > 0xff)
						na = 0xff;
					stenbuf[i] = (unsigned char)na;
				}
			}
			else /* span.mask && !alphaBlend */
			{
				op_memcpy(stenbuf, span.mask, span.length);
			}
		}
		else
		{
			// Setup the render destination
			VEGAPixelAccessor dst = buffer->GetAccessor(span.pos, span.scanline);

			if (fillstate.alphaBlend)
			{
				VEGAFill* fill = fillstate.fill;
				if (fill)
				{
					if (fill->getFillOpacity() == 0)
						continue;

					if (fill->getFillOpacity() < 0xff)
					{
						if (span.mask)
						{
							int op = fill->getFillOpacity()+1;
							UINT8* tmp = maskscratch;
							for (unsigned int i = 0; i < span.length; ++i)
								*tmp++ = (span.mask[i] * op) >> 8;
						}
						else
						{
							op_memset(maskscratch, fill->getFillOpacity(), span.length);
						}

						span.mask = maskscratch;
					}

					fill->apply(dst, span);
				}
				else
				{
					if (span.mask)
					{
						VEGACompOverIn(dst.Ptr(), fillstate.ppixel, span.mask, span.length);
					}
					else
					{
						VEGACompOver(dst.Ptr(), fillstate.ppixel, span.length);
					}
				}
			}
			else /* !alphaBlend */
			{
				// Blend the dest color to the source color instead.
				// This is used to ignore the alpha value of the source color for clear etc.
				VEGAFill* fill = fillstate.fill;
				if (fill)
				{
					// FIXME: this is not working properly
					UINT32* tmpsl = OP_NEWA(UINT32, span.length);
					if (!tmpsl)
						// While we could propagate OOM here, it will
						// be of little use. We should probably just
						// preallocate a suitably sized (say,
						// width-size) buffer and use it here instead
						// of allocating a new for each and every span
						// we process... But for now, we just skip
						// this span and hope for better luck with the
						// next... (FIXME)
						continue;

					op_memset(tmpsl, 0, span.length * sizeof(*tmpsl));

					VEGASWBuffer tmpbuf;
					tmpbuf.CreateFromData(tmpsl, span.length * sizeof(*tmpsl), span.length, 1);
					VEGAPixelAccessor tmpfill = tmpbuf.GetAccessor(0, 0);

					// BORK BORK(?)
					// Save the mask pointer, and then
					// decieve the fill code into thinking the span is
					// opaque, to avoid applying a potential mask to
					// the fill.
					const UINT8* mask = span.mask;
					span.mask = NULL;

					fill->apply(tmpfill, span);

					if (fill->getFillOpacity() < 0xff)
					{
						int op = fill->getFillOpacity();
						unsigned cnt = span.length;
						while (cnt-- > 0)
						{
							tmpfill.Store(VEGAPixelModulate(tmpfill.Load(), op));
							tmpfill++;
						}

						tmpfill -= span.length;
					}

					if (mask)
					{
						unsigned cnt = span.length;
						while (cnt-- > 0)
						{
							dst.Store(VEGACompOverIn(tmpfill.Load(), dst.Load(), 0xff - *mask));
							++dst;
							++mask;
							++tmpfill;
						}
					}
					else
					{
						dst.CopyFrom(tmpfill.Ptr(), span.length);
					}
					OP_DELETEA(tmpsl);
				}
				else
				{
					if (span.mask)
					{
						const UINT8* mask = span.mask;

						unsigned cnt = span.length;
						while (cnt-- > 0)
						{
							dst.Store(VEGACompOverIn(fillstate.ppixel, dst.Load(), 0xff - *mask));
							++dst;
							++mask;
						}
					}
					else
					{
						dst.Store(fillstate.ppixel, span.length);
					}
				}
			}
		}
	}
}

#endif // VEGA_SUPPORT
