/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGABACKEND_HW2D_H
#define VEGABACKEND_HW2D_H

#if defined(VEGA_SUPPORT) && defined(VEGA_2DDEVICE)
#include "modules/libvega/src/vegabackend_sw.h"
#include "modules/libvega/src/vegarasterizer.h"

#include "modules/libvega/vega2ddevice.h"
#include "modules/libvega/src/vegarendertargetimpl.h"
#include "modules/libvega/src/vegaswbuffer.h"


class VEGABackend_HW2D :
	public VEGABackend_SW
{
public:
	VEGABackend_HW2D();
	~VEGABackend_HW2D();

	OP_STATUS init(unsigned int w, unsigned int h, unsigned int q);

	bool bind(VEGARenderTarget* rt);
	void unbind();

	void flush(const OpRect* update_rects, unsigned int num_rects);

	void clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform);
	OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil);
	OP_STATUS fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend);
	OP_STATUS invertRect(int x, int y, unsigned int width, unsigned int height);
	OP_STATUS moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy);

	OP_STATUS drawImage(VEGAImage* image, const VEGADrawImageInfo& imginfo);

	OP_STATUS drawString(class VEGAFont* font, int x, int y, const struct ProcessedString* processed_string, VEGAStencil* stencil);
# if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
	OP_STATUS drawString(class VEGAFont* font, int x, int y, const uni_char* text, UINT32 len, INT32 extra_char_space, short word_width, VEGAStencil* stencil);
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS

	OP_STATUS applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region);

	OP_STATUS createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp);
	OP_STATUS createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window);
	OP_STATUS createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h);

	OP_STATUS createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h);

#ifdef VEGA_LIMIT_BITMAP_SIZE
	static unsigned maxBitmapSide() { return 0; }
#endif // VEGA_LIMIT_BITMAP_SIZE
	static OP_STATUS createBitmapStore(VEGABackingStore** store, unsigned w, unsigned h, bool is_indexed);
	virtual bool supportsStore(VEGABackingStore* store);

#ifdef CANVAS3D_SUPPORT
	OP_STATUS createImage(VEGAImage* img, VEGA3dTexture* tex, BOOL isPremultiplied);
#endif // CANVAS3D_SUPPORT

protected:
#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	virtual bool useCachedImages() {return true;}
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES

private:
	VEGA2dDevice* device;
	VEGABackingStore* m_store;
	VEGA2dSurface* m_surface;
};

/* Backing by a VEGA2dSurface - rw */
class VEGABackingStore_Surface : public VEGABackingStore_Lockable
{
public:
	VEGABackingStore_Surface() : m_surface(NULL) {}
	virtual ~VEGABackingStore_Surface();

	OP_STATUS Construct(unsigned width, unsigned height);
	OP_STATUS Construct(VEGA2dWindow* win2d);

	virtual bool IsA(Type type) const { return type == SURFACE; }

	virtual void SetColor(const OpRect& rect, UINT32 color);

	virtual OP_STATUS CopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store);

	virtual unsigned GetWidth() const { return m_surface->getWidth(); }
	virtual unsigned GetHeight() const { return m_surface->getHeight(); }
	virtual unsigned GetBytesPerLine() const { return m_surface->getWidth() * 4; }
	virtual unsigned GetBytesPerPixel() const { return 4; }

	VEGA2dSurface* GetSurface() { return m_surface; }

protected:
	virtual OP_STATUS LockRect(const OpRect& rect, AccessType acc);
	virtual OP_STATUS UnlockRect(BOOL commit);

	struct DeviceState
	{
		VEGA2dSurface* rendertarget;
		unsigned int clip_x, clip_y, clip_w, clip_h;
	};
	DeviceState SaveDevice(VEGA2dDevice* device)
	{
		DeviceState dstate;
		dstate.rendertarget = device->getRenderTarget();
		device->getClipRect(dstate.clip_x, dstate.clip_y, dstate.clip_w, dstate.clip_h);
		return dstate;
	}
	void RestoreDevice(VEGA2dDevice* device, const DeviceState& dstate)
	{
		device->setRenderTarget(dstate.rendertarget);
		device->setClipRect(dstate.clip_x, dstate.clip_y, dstate.clip_w, dstate.clip_h);
	}

	VEGA2dDevice::SurfaceData m_lock_data;
	VEGA2dSurface* m_surface;
};

#endif // VEGA_SUPPORT && VEGA_2DDEVICE
#endif // VEGABACKEND_HW2D_H
