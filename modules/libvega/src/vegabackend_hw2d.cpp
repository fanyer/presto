/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_2DDEVICE)
#include "modules/libvega/src/vegabackend_hw2d.h"
#include "modules/libvega/src/vegabackend_sw.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegastencil.h"

#include "modules/libvega/src/vegagradient.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/src/vegarendertargetimpl.h"
#include "modules/libvega/src/vegacomposite.h"
#include "modules/libvega/src/vegapixelformat.h"

#include "modules/libvega/src/vegafilterdisplace.h"

#include "modules/mdefont/processedstring.h"

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/vegawindow.h"

VEGABackend_HW2D::VEGABackend_HW2D()
{
	device = g_vegaGlobals.vega2dDevice;

	has_image_funcs = true;
	supports_image_tiling = true;
}

VEGABackend_HW2D::~VEGABackend_HW2D()
{
}

OP_STATUS VEGABackend_HW2D::init(unsigned int w, unsigned int h, unsigned int q)
{
	if (!device)
	{
		OP_STATUS status = VEGA2dDevice::Create(&g_vegaGlobals.vega2dDevice, w, h,
												g_vegaGlobals.mainNativeWindow);
		if (OpStatus::IsError(status))
		{
			return status;
		}

		device = g_vegaGlobals.vega2dDevice;
	}
	else if (w > device->getWidth() || h > device->getHeight())
	{
		RETURN_IF_ERROR(device->resize(MAX(w, device->getWidth()), MAX(h, device->getHeight())));
	}

	device->setRenderTarget(NULL);

	RETURN_IF_ERROR(VEGABackend_SW::init(w, h, q));
	return OpStatus::OK;
}

bool VEGABackend_HW2D::bind(VEGARenderTarget* rt)
{
	VEGABackingStore* bstore = rt->GetBackingStore();
	if (bstore)
	{
		bstore->Validate();

		m_store = bstore;

		if (bstore->IsA(VEGABackingStore::SURFACE))
		{
			m_surface = static_cast<VEGABackingStore_Surface*>(bstore)->GetSurface();
			return true;
		}
		else if (bstore->IsA(VEGABackingStore::SWBUFFER))
		{
			buffer = static_cast<VEGABackingStore_SWBuffer*>(bstore)->GetBuffer();
			return true;
		}
	}
	return false;
}

void VEGABackend_HW2D::unbind()
{
	m_surface = NULL;
	m_store = NULL;
	buffer = NULL;
}

void VEGABackend_HW2D::flush(const OpRect* update_rects, unsigned int num_rects)
{
	m_store->Validate();

	device->setRenderTarget(m_surface);
	device->flush(update_rects, num_rects);
}

OP_STATUS VEGABackingStore_Surface::Construct(unsigned width, unsigned height)
{
	VEGA2dDevice* device = g_vegaGlobals.vega2dDevice;
	RETURN_IF_ERROR(device->createSurface(&m_surface, width, height, VEGA2dSurface::FORMAT_RGBA));
	return OpStatus::OK;
}

OP_STATUS VEGABackingStore_Surface::Construct(VEGA2dWindow* win2d)
{
	m_surface = win2d;
	return OpStatus::OK;
}

VEGABackingStore_Surface::~VEGABackingStore_Surface()
{
	// Avoid flushing when the device has disappeared
	if (g_vegaGlobals.vega2dDevice)
		FlushTransaction();

	OP_DELETE(m_surface);
}

void VEGABackingStore_Surface::SetColor(const OpRect& rect, UINT32 color)
{
	FlushTransaction();

	VEGA2dDevice* device = g_vegaGlobals.vega2dDevice;

	DeviceState dstate = SaveDevice(device);

	device->setRenderTarget(m_surface);
	device->setClipRect(rect.x, rect.y, rect.width, rect.height);

	VEGA_PIXEL packed_color = ColorToVEGAPixel(color);
	device->setColor(VEGA_UNPACK_R(packed_color), VEGA_UNPACK_G(packed_color),
					 VEGA_UNPACK_B(packed_color), VEGA_UNPACK_A(packed_color));
	device->clear(rect.x, rect.y, rect.width, rect.height);

	RestoreDevice(device, dstate);
}

OP_STATUS VEGABackingStore_Surface::CopyRect(const OpPoint& dstp, const OpRect& srcr,
											 VEGABackingStore* store)
{
	FlushTransaction();

	if (!store->IsA(VEGABackingStore::SURFACE))
		return FallbackCopyRect(dstp, srcr, store);

	store->Validate();

	VEGA2dDevice* device = g_vegaGlobals.vega2dDevice;

	DeviceState dstate = SaveDevice(device);

	device->setRenderTarget(m_surface);
	device->setClipRect(0, 0, m_surface->getWidth(), m_surface->getHeight());

	VEGABackingStore_Surface* srf_store = static_cast<VEGABackingStore_Surface*>(store);

	device->copy(srf_store->m_surface, srcr.x, srcr.y, srcr.width, srcr.height, dstp.x, dstp.y);

	RestoreDevice(device, dstate);
	return OpStatus::OK;
}

OP_STATUS VEGABackingStore_Surface::LockRect(const OpRect& rect, AccessType acc)
{
	VEGA2dDevice* device = g_vegaGlobals.vega2dDevice;

	device->setRenderTarget(m_surface);

	OP_ASSERT(OpRect(0, 0, m_surface->getWidth(), m_surface->getHeight()).Contains(rect));

	m_lock_data.x = rect.x;
	m_lock_data.y = rect.y;
	m_lock_data.w = rect.width;
	m_lock_data.h = rect.height;

	unsigned use_intent = 0;
	use_intent |= (acc == ACC_READ_ONLY || acc == ACC_READ_WRITE) ? VEGA2dDevice::READ_INTENT : 0;
	use_intent |= (acc == ACC_WRITE_ONLY || acc == ACC_READ_WRITE) ? VEGA2dDevice::WRITE_INTENT : 0;

	RETURN_IF_ERROR(device->lockRect(&m_lock_data, use_intent));

	OP_STATUS status = m_lock_buffer.Bind(&m_lock_data.pixels);
	if (OpStatus::IsError(status))
		device->unlockRect(&m_lock_data);

	return status;
}

OP_STATUS VEGABackingStore_Surface::UnlockRect(BOOL commit)
{
	VEGA2dDevice* device = g_vegaGlobals.vega2dDevice;

	if (commit)
		m_lock_buffer.Sync(&m_lock_data.pixels);

	m_lock_buffer.Unbind(&m_lock_data.pixels);
	device->setRenderTarget(m_surface);
	device->unlockRect(&m_lock_data);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp)
{
	VEGABackingStore* bstore = static_cast<VEGAOpBitmap*>(bmp)->GetBackingStore();

	OP_ASSERT(bstore->IsA(VEGABackingStore::SURFACE));

	VEGABitmapRenderTarget* rend = OP_NEW(VEGABitmapRenderTarget, (bstore));
	if (!rend)
		return OpStatus::ERR_NO_MEMORY;

	VEGARefCount::IncRef(bstore);

	*rt = rend;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window)
{
	VEGA2dWindow* win2d;
	RETURN_IF_ERROR(device->createWindow(&win2d, window));

	OP_STATUS status = window->initHardwareBackend(win2d);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(win2d);
		return status;
	}

	VEGABackingStore_Surface* wstore = OP_NEW(VEGABackingStore_Surface, ());
	if (!wstore)
	{
		OP_DELETE(win2d);
		return OpStatus::ERR_NO_MEMORY;
	}

	OpStatus::Ignore(wstore->Construct(win2d));

	VEGA2dWindowRenderTarget* winrt = OP_NEW(VEGA2dWindowRenderTarget, (wstore, win2d));
	if (!winrt)
	{
		VEGARefCount::DecRef(wstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = winrt;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h)
{
	VEGABackingStore_Surface* bstore = OP_NEW(VEGABackingStore_Surface, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h);
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

OP_STATUS VEGABackend_HW2D::createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h)
{
	if (component)
	{
		VEGARenderTarget* rend;
		RETURN_IF_ERROR(createIntermediateRenderTarget(&rend, w, h));

		rend->GetBackingStore()->SetColor(OpRect(0, 0, w, h), 0);

		*sten = static_cast<VEGAStencil*>(rend);

		return OpStatus::OK;
	}

	return VEGABackend_SW::createStencil(sten, component, w, h);
}

/* static */
OP_STATUS VEGABackend_HW2D::createBitmapStore(VEGABackingStore** store, unsigned w, unsigned h, bool is_indexed)
{
	VEGABackingStore_Surface* bstore = OP_NEW(VEGABackingStore_Surface, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}
	*store = bstore;
	return OpStatus::OK;
}

bool VEGABackend_HW2D::supportsStore(VEGABackingStore* store)
{
	return store->IsA(VEGABackingStore::SURFACE);
}

#ifdef CANVAS3D_SUPPORT
#include "modules/libvega/vega3ddevice.h"

OP_STATUS VEGABackend_HW2D::createImage(VEGAImage* img, VEGA3dTexture* tex, BOOL isPremultiplied)
{
	VEGABackingStore_Surface* srf_store = OP_NEW(VEGABackingStore_Surface, ());
	if (!srf_store)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = srf_store->Construct(tex->getWidth(), tex->getHeight());
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(srf_store);
		return status;
	}

	VEGABackingStore* store = srf_store;
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

			status = img->init(store, isPremultiplied);

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

void VEGABackend_HW2D::clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform)
{
	if (buffer)
		return VEGABackend_SW::clear(x, y, w, h, color, transform);

	if (transform)
	{
		clearTransformed(x, y, w, h, color, transform);
		return;
	}

	m_store->Validate();

	VEGA2dSurface* surf = m_surface;

	// Modify the clipping rectangle to not be outside the render target
	int cliprect_ex = surf->getWidth();
	if (this->cliprect_ex < cliprect_ex)
		cliprect_ex = this->cliprect_ex;
	int cliprect_ey = surf->getHeight();
	if (this->cliprect_ey < cliprect_ey)
		cliprect_ey = this->cliprect_ey;

	int ex = MIN(x+(int)w, cliprect_ex);
	int ey = MIN(y+(int)h, cliprect_ey);

	int sx = MAX(x, cliprect_sx);
	int sy = MAX(y, cliprect_sy);

	if (sx >= ex || sy >= ey)
		return;

	device->setRenderTarget(surf);

	VEGA_PIXEL packed_color = ColorToVEGAPixel(color);
	device->setColor(VEGA_UNPACK_R(packed_color), VEGA_UNPACK_G(packed_color), VEGA_UNPACK_B(packed_color), VEGA_UNPACK_A(packed_color));

	device->clear(sx, sy, ex - sx, ey - sy);

	renderTarget->markDirty(sx, ex-1, sy, ey-1);
}

OP_STATUS VEGABackend_HW2D::applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region)
{
	if (buffer)
		return VEGABackend_SW::applyFilter(filter, region);

	if (!filter->hasSource())
		return OpStatus::ERR;

	m_store->Validate();

	return filter->apply(m_surface, region);
}

OP_STATUS VEGABackend_HW2D::drawString(class VEGAFont* font, int x, int y, const struct ProcessedString* processed_string, VEGAStencil* stencil)
{
	BOOL draw_simple = (fillstate.fill == NULL) && (stencil == NULL);

	if (buffer)
	{
		return VEGABackend_SW::drawString(font, x, y, processed_string, stencil);
	}
	else if (!draw_simple)
	{
		VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!mapped_buffer)
			return OpStatus::ERR_NO_MEMORY;

		buffer = mapped_buffer;

		OP_STATUS status = VEGABackend_SW::drawString(font, x, y, processed_string, stencil);

		buffer = NULL;

		m_store->EndTransaction(TRUE);
		return status;
	}

	m_store->Validate();

	OP_ASSERT(m_surface);

	device->setRenderTarget(m_surface);

	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	// It could also make sense to make the device-impl. handle it,
	// but this is playing it safe.
	unsigned surface_w = m_surface->getWidth();
	unsigned surface_h = m_surface->getHeight();
	if (ex > (int)surface_w)
		ex = surface_w;
	if (ey > (int)surface_h)
		ey = surface_h;

	VEGA2dDevice::GlyphInfo ginfos[16]; /* ARRAY OK 2012-03-05 wonko */
	unsigned ginfo_count = 0;

	const unsigned batch_size = MIN(ARRAY_SIZE(ginfos), font->getGlyphCacheSize());

	if (draw_simple)
	{
		device->setColor(VEGA_UNPACK_R(fillstate.ppixel), VEGA_UNPACK_G(fillstate.ppixel), VEGA_UNPACK_B(fillstate.ppixel), VEGA_UNPACK_A(fillstate.ppixel));
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_COLORIZE);
	}

	device->setClipRect(sx, sy, ex - sx, ey - sy);

	OP_STATUS status = OpStatus::OK;

	for (unsigned int c = 0; c < processed_string->m_length; ++c)
	{
		int xpos = x + processed_string->m_processed_glyphs[c].m_pos.x;
		int ypos = y + processed_string->m_processed_glyphs[c].m_pos.y;

		VEGAFont::VEGAGlyph glyph;
		glyph.advance = 0;

		const UINT8* src;
		unsigned int srcStride;

		OP_STATUS s = font->getGlyph(processed_string->m_processed_glyphs[c].m_id, glyph,
									 src, srcStride, processed_string->m_is_glyph_indices);
		if (OpStatus::IsError(s))
		{
			if (!OpStatus::IsMemoryError(status))
				status = s;

			continue;
		}

		if (glyph.width <= 0 || glyph.height <= 0)
			continue; // Skip

		OP_ASSERT(OpStatus::IsSuccess(s) && glyph.width > 0 && glyph.height > 0);

		if (draw_simple)
		{
			ginfos[ginfo_count].x = xpos + glyph.left;
			ginfos[ginfo_count].y = ypos - glyph.top;
			ginfos[ginfo_count].w = glyph.width;
			ginfos[ginfo_count].h = glyph.height;

			ginfos[ginfo_count].buffer = src;
			ginfos[ginfo_count].buffer_stride = srcStride;

			if (++ginfo_count == batch_size)
			{
				device->drawGlyphs(ginfos, ginfo_count);

				ginfo_count = 0;
			}
		}
	}

	if (draw_simple)
	{
		// Flush remaining
		if (ginfo_count > 0)
			device->drawGlyphs(ginfos, ginfo_count);

		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OVER);
	}

	return status;
}
#if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
OP_STATUS VEGABackend_HW2D::drawString(VEGAFont* font, int x, int y, const uni_char* _text, UINT32 len, INT32 extra_char_space, short adjust, VEGAStencil* stencil)
{
	BOOL draw_simple = (fillstate.fill == NULL) && (stencil == NULL);

	if (buffer)
	{
		return VEGABackend_SW::drawString(font, x, y, _text, len, extra_char_space, adjust, stencil);
	}
	else if (!draw_simple)
	{
		VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!mapped_buffer)
			return OpStatus::ERR_NO_MEMORY;

		buffer = mapped_buffer;

		OP_STATUS status = VEGABackend_SW::drawString(font, x, y, _text, len, extra_char_space, adjust, stencil);

		buffer = NULL;

		m_store->EndTransaction(TRUE);
		return status;
	}

	m_store->Validate();

	return OpStatus::ERR;
}
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS

OP_STATUS VEGABackend_HW2D::drawImage(VEGAImage* image, const VEGADrawImageInfo& imginfo)
{
	// No need to draw the image if opacity==0
	if (imginfo.opacity == 0)
		return OpStatus::OK;

	if (buffer)
		return VEGABackend_SW::drawImage(image, imginfo);

	if (!image->GetBackingStore()->IsA(VEGABackingStore::SURFACE))
	{
		VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!mapped_buffer)
			return OpStatus::ERR_NO_MEMORY;

		buffer = mapped_buffer;

		OP_STATUS status = VEGABackend_SW::drawImage(image, imginfo);

		buffer = NULL;

		m_store->EndTransaction(TRUE);
		return status;
	}

	m_store->Validate();

	VEGA2dSurface* surf = m_surface;

	// Clipping and crap
	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)surf->getWidth())
		ex = surf->getWidth();
	if (ey > (int)surf->getHeight())
		ey = surf->getHeight();

	VEGABackingStore* img_store = image->GetBackingStore();

	img_store->Validate();

	VEGA2dSurface* img_surf = static_cast<VEGABackingStore_Surface*>(img_store)->GetSurface();

	device->setRenderTarget(surf);
	device->setClipRect(sx, sy, ex - sx, ey - sy);

	// Use the source operator if we know the bitmap is opaque.
	bool is_opaque = image->isOpaque();
	bool use_opacity = (imginfo.opacity != 255);

	if (use_opacity)
	{
		device->setColor(imginfo.opacity, imginfo.opacity, imginfo.opacity, imginfo.opacity);
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OPACITY);
	}
	else if (is_opaque)
	{
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC);
	}

	if (imginfo.type == VEGADrawImageInfo::SCALED)
	{
		VEGA2dDevice::InterpolationMethod method = VEGA2dDevice::INTERPOLATE_NEAREST;
		if (imginfo.quality == VEGAFill::QUALITY_LINEAR)
			method = VEGA2dDevice::INTERPOLATE_BILINEAR;

		device->setInterpolationMethod(method);
		device->stretchBlit(img_surf,
							imginfo.srcx, imginfo.srcy, imginfo.srcw, imginfo.srch,
							imginfo.dstx, imginfo.dsty, imginfo.dstw, imginfo.dsth);
	}
	else if (imginfo.type == VEGADrawImageInfo::REPEAT)
	{
		OpRect src_rect(0, 0, img_surf->getWidth(), img_surf->getHeight());
		OpRect tile_area(imginfo.dstx, imginfo.dsty, imginfo.dstw, imginfo.dsth);
		OpPoint offset(imginfo.tile_offset_x, imginfo.tile_offset_y);

		VEGA2dDevice::InterpolationMethod method = VEGA2dDevice::INTERPOLATE_NEAREST;
		if (imginfo.quality == VEGAFill::QUALITY_LINEAR)
			method = VEGA2dDevice::INTERPOLATE_BILINEAR;
		device->setInterpolationMethod(method);

		device->tileBlit(img_surf, src_rect, tile_area, offset, imginfo.tile_width, imginfo.tile_height);
	}
	else
	{
		device->blit(img_surf,
					 imginfo.srcx, imginfo.srcy, imginfo.srcw, imginfo.srch,
					 imginfo.dstx, imginfo.dsty);
	}

	if (is_opaque || use_opacity)
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OVER);

	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy)
{
	if (buffer) // Highly unlikely but...
		return VEGABackend_SW::moveRect(x, y, width, height, dx, dy);

	m_store->Validate();

	device->setRenderTarget(m_surface);
	device->copy(m_surface, x, y, width, height, x+dx, y+dy);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend)
{
	if (!blend)
	{
		// Emulate the 'source' composite operation by first clearing to 0 and then doing 'over' as per normal.
		// "dst = 0; dst = src + (1 - src.a) * dst" simplifies to "dst = src".
		clear(x, y, width, height, 0, NULL);
	}

	if (buffer)
	{
		return VEGABackend_SW::fillRect(x, y, width, height, stencil, blend);
	}
	else if (stencil || fillstate.fill)
	{
#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
		if (!stencil)
		{
			FractRect fr;
			fr.x = x;
			fr.y = y;
			fr.w = width;
			fr.h = height;
			fr.weight = 255;

			VEGAImage* img = getDrawableImageFromFill(fillstate.fill,
													  VEGA_INTTOFIX(fr.x), VEGA_INTTOFIX(fr.y),
													  VEGA_INTTOFIX(fr.w), VEGA_INTTOFIX(fr.h));
			if (img)
			{
				FillState saved_fillstate = fillstate;
				fillstate.fill = img;
				OP_STATUS status = fillFractImages(&fr, 1);
				fillstate = saved_fillstate;
				if (OpStatus::IsSuccess(status))
					return OpStatus::OK;
			}
		}
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

		VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!mapped_buffer)
			return OpStatus::ERR_NO_MEMORY;

		buffer = mapped_buffer;

		OP_STATUS status = VEGABackend_SW::fillRect(x, y, width, height, stencil, blend);

		buffer = NULL;

		m_store->EndTransaction(TRUE);
		return status;
	}

	m_store->Validate();

	// No stencil, simple color
	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	// Modify the clipping rectangle to not be outside the render target
	if (ex > (int)renderTarget->getWidth())
		ex = renderTarget->getWidth();
	if (ey > (int)renderTarget->getHeight())
		ey = renderTarget->getHeight();

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

	device->setRenderTarget(m_surface);
	device->setClipRect(x, y, width, height);

	UINT8 r = VEGA_UNPACK_R(fillstate.ppixel);
	UINT8 g = VEGA_UNPACK_G(fillstate.ppixel);
	UINT8 b = VEGA_UNPACK_B(fillstate.ppixel);
	UINT8 a = VEGA_UNPACK_A(fillstate.ppixel);

	if (a == 255)
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC);

	device->setColor(r, g, b, a);
	device->fillRect(x, y, width, height);

	if (a == 255)
		device->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OVER);

	renderTarget->markDirty(x, x+width-1, y, y+height-1);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW2D::invertRect(int x, int y, unsigned int width, unsigned int height)
{
	if (buffer)
		return VEGABackend_SW::invertRect(x, y, width, height);

	VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!mapped_buffer)
		return OpStatus::ERR_NO_MEMORY;

	buffer = mapped_buffer;

	OP_STATUS status = VEGABackend_SW::invertRect(x, y, width, height);

	buffer = NULL;

	m_store->EndTransaction(TRUE);
	return status;
}

OP_STATUS VEGABackend_HW2D::fillPath(VEGAPath *path, VEGAStencil *stencil)
{
	if (buffer)
		return VEGABackend_SW::fillPath(path, stencil);

	VEGASWBuffer* mapped_buffer = m_store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!mapped_buffer)
		return OpStatus::ERR_NO_MEMORY;

	buffer = mapped_buffer;

	OP_STATUS status = VEGABackend_SW::fillPath(path, stencil);

	buffer = NULL;

	m_store->EndTransaction(TRUE);
	return status;
}

#endif // VEGA_SUPPORT && VEGA_2DDEVICE
