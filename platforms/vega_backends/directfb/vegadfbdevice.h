/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef VEGADFBDEVICE_H
#define VEGADFBDEVICE_H
# ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vega2ddevice.h"

#include <directfb.h>

class VEGADFBSurface : public VEGA2dSurface
{
public:
	VEGADFBSurface(unsigned w, unsigned h) :
		dfb_surface(NULL), width(w), height(h), owns_surface(true), locked(false) {}
	virtual ~VEGADFBSurface();

	virtual Format getFormat() { return vfmt; }

	virtual unsigned int getWidth() { return width; }
	virtual unsigned int getHeight() { return height; }

#ifdef CANVAS3D_SUPPORT
	virtual VEGA3dFramebufferObject* lock3d() { return NULL; } // Not supported
	virtual void unlock3d() {}
#endif // CANVAS3D_SUPPORT

	IDirectFBSurface* dfb_surface;

	Format vfmt;
	unsigned width;
	unsigned height;

	bool owns_surface;
	bool locked;
};

class VEGADFBWindow : public VEGA2dWindow
{
public:
	virtual ~VEGADFBWindow() {}

	virtual VEGA2dSurface* getWindowSurface() = 0;

#ifdef CANVAS3D_SUPPORT
	virtual VEGA3dFramebufferObject* lock3d() { return NULL; } // Not supported
	virtual void unlock3d() {}
#endif // CANVAS3D_SUPPORT
};

class VEGADFBDevice : public VEGA2dDevice
{
public:
	VEGADFBDevice();

	OP_STATUS Construct(IDirectFB* dfb, unsigned int w, unsigned int h)
	{
		dfb_if = dfb;
		return resize(w, h);
	}

	void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);

	unsigned int getWidth() { return dev_width; }
	unsigned int getHeight() { return dev_height; }

	OP_STATUS resize(unsigned int width, unsigned int height);

	void setColor(UINT8 r, UINT8 g, UINT8 b, UINT8 a);

	void setClipRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h);
	void getClipRect(unsigned int& x, unsigned int& y, unsigned int& w, unsigned int& h);

	virtual const uni_char* getDescription() const { return UNI_L("DirectFB"); }

	bool supportsCompositeOperator(CompositeOperator op)
	{
		return
			op == COMPOSITE_SRC ||
			op == COMPOSITE_SRC_OVER ||
			op == COMPOSITE_SRC_CLEAR ||
			op == COMPOSITE_COLORIZE ||
			op == COMPOSITE_SRC_OPACITY;
	}
	void setCompositeOperator(CompositeOperator op)
	{
		OP_ASSERT(supportsCompositeOperator(op));
		compositeOp = op;
	}
	CompositeOperator getCompositeOperator() { return compositeOp; }

	void setInterpolationMethod(InterpolationMethod method) { interpMethod = method; }

	void clear(unsigned int x, unsigned int y, unsigned int w, unsigned int h);

	OP_STATUS fillRect(int x, int y, unsigned int w, unsigned int h);

	OP_STATUS copy(VEGA2dSurface* src,
				   unsigned int src_x, unsigned int src_y, unsigned int src_w, unsigned int src_h,
				   unsigned int dst_x, unsigned int dst_y);

	OP_STATUS blit(VEGA2dSurface* src,
				   unsigned int src_x, unsigned int src_y, unsigned int src_w, unsigned int src_h,
				   int dst_x, int dst_y);
	OP_STATUS blit(VEGA2dSurface* src, VEGA2dSurface* mask,
				   unsigned int src_x, unsigned int src_y, unsigned int src_w, unsigned int src_h,
				   int dst_x, int dst_y);
	OP_STATUS stretchBlit(VEGA2dSurface* src,
						  unsigned int src_x, unsigned int src_y, unsigned int src_w, unsigned int src_h,
						  int dst_x, int dst_y, unsigned int dst_w, unsigned int dst_h);
	OP_STATUS tileBlit(VEGA2dSurface* src,
					   const OpRect& src_rect,
					   const OpRect& tile_area,
					   const OpPoint& offset,
					   unsigned tile_w, unsigned tile_h);

	OP_STATUS drawGlyphs(const GlyphInfo* glyphs, unsigned glyph_count);

	OP_STATUS createSurface(VEGA2dSurface** surface, unsigned int width, unsigned int height,
							VEGA2dSurface::Format format);

	OP_STATUS createSurface(VEGA2dSurface** surface, IDirectFBSurface* dfb_surface);

	OP_STATUS setRenderTarget(VEGA2dSurface* rt);
	VEGA2dSurface* getRenderTarget() { return target; }

	OP_STATUS lockRect(SurfaceData* data, unsigned intent);
	void unlockRect(SurfaceData* data);

	virtual VEGADFBSurface* allocateSurface(unsigned int width, unsigned int height);

protected:
	DFBRegion dfb_clip;
	CompositeOperator compositeOp;
	InterpolationMethod interpMethod;

	IDirectFB* dfb_if;
	VEGA2dSurface* target;
	IDirectFBSurface* dfb_target;

	unsigned int dev_width;
	unsigned int dev_height;
	UINT8 col_r, col_g, col_b, col_a;
};

# endif // VEGA_SUPPORT
#endif // VEGADFBDEVICE_H
