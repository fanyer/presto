/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef VEGA2DDEVICE_H
#define VEGA2DDEVICE_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegapixelstore.h"
#include "modules/libvega/vega3ddevice.h"
class VEGAWindow;

class VEGA2dSurface
{
public:
	virtual ~VEGA2dSurface() {}

	enum Format
	{
		FORMAT_RGBA,
		FORMAT_INTENSITY8
	};

	virtual Format getFormat() = 0;

	enum Type
	{
		VEGA2D_SURFACE,
		VEGA2D_WINDOW
	};

	virtual Type getType() { return VEGA2D_SURFACE; }

	virtual unsigned int getWidth() = 0;
	virtual unsigned int getHeight() = 0;
#ifdef CANVAS3D_SUPPORT
	/** Returns a FBO that can be used by the 3ddevice to render 3d content
		onto this surface, or NULL if not supported. The FBO is valid until
		unlock3d has been called and only then can the render result be
		expected to be made visible. */
	virtual VEGA3dFramebufferObject* lock3d() = 0;

	/** Called when the 3D rendering is done and the resulting rendering on
		the FBO should be made visible on the surface.
		This function should usually do these things if the 3d device is
		opengl es based:
		1. Call eglSwapBuffers or glFinish
		2. Copy the FBO contents to the surface if needed
		3. Flush the renderqueue if needed, (Mali requires a call to glClear
		   if pixmaps are used as rendertargets) */
	virtual void unlock3d() = 0;
#endif // CANVAS3D_SUPPORT
};

/** Abstract interface to a window with 2d rendering capabilities.
 * A 2dWindow is a render target which has a present function. */
class VEGA2dWindow : public VEGA2dSurface
{
public:
	virtual ~VEGA2dWindow() {}

	/** Make the rendered content visible. Swap for double buffering. */
	virtual void present(const OpRect* update_rects, unsigned int num_rects) = 0;

	virtual Type getType() { return VEGA2D_WINDOW; }
};

class VEGA2dDevice
{
public:
	/** Create a 2d device. This is only called once to create the global 2d
	 * device instance.
	 * @param dev the device to create
	 * @param width, height the maximum size of rendertargets used in this
	 * device.
	 * @param nativeWin the native window which is used by VegaOpPainter.
	 * This is sent to make it possible to use it for initializing the 2d
	 * context. */
	static OP_STATUS Create(VEGA2dDevice** dev, unsigned int width, unsigned int height,
							VEGAWindow* nativeWin = NULL);

	virtual ~VEGA2dDevice() {}
	virtual void Destroy() {}

	/** Flush makes sure the current drawing operations are finished
	 * and presented. */
	virtual void flush(const OpRect* update_rects, unsigned int num_rects) = 0;

	/** Report the width and height of the device. This is the maximum
	 * size of a render target. If a canvas is resized to something bigger
	 * than this, the device will be resized. */
	virtual unsigned int getWidth() = 0;
	virtual unsigned int getHeight() = 0;

	/** Resize the 2d device. Should be called if creating a render target
	 * bigger than previous maximum size. */
	virtual OP_STATUS resize(unsigned int width, unsigned int height) = 0;

	/** Set the color to be used for filling, clearing and opacity
	 * (alpha channel) */
	virtual void setColor(UINT8 r, UINT8 g, UINT8 b, UINT8 a) = 0;

	/** get/set the cliprect for the device */
	virtual void setClipRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h) = 0;
	virtual void getClipRect(unsigned int& x, unsigned int& y, unsigned int& w, unsigned int& h) = 0;

	/** @returns a string used to describe this renderer on opera:gpu.
	 * For example "DirectFB". */
	virtual const uni_char* getDescription() const = 0;

	enum CompositeOperator
	{
		COMPOSITE_SRC = 0,		///< Porter-Duff 'src'
		COMPOSITE_SRC_OVER,		///< Porter-Duff 'source-over'
		COMPOSITE_SRC_CLEAR,	///< (dst OUT mask) OVER src
		COMPOSITE_COLORIZE,		///< (color IN src) OVER dst
		COMPOSITE_SRC_OPACITY	///< Opacity blending - (src IN opacity) OVER dst
	};

	/** Query the device for support of a certain composite operator */
	virtual bool supportsCompositeOperator(CompositeOperator op) = 0;
	/** Set the composite operator to use for drawing operations */
	virtual void setCompositeOperator(CompositeOperator op) = 0;
	/** Query the composite operator currently used */
	virtual CompositeOperator getCompositeOperator() = 0;

	enum InterpolationMethod
	{
		INTERPOLATE_NEAREST = 0,
		INTERPOLATE_BILINEAR
	};

	/** Set the method to use for interpolation (used by stretchBlit) */
	virtual void setInterpolationMethod(InterpolationMethod method) = 0;

	/** Clear the current rendertarget */
	virtual void clear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) = 0;

	/** Fill a rectangle with a color. */
	virtual OP_STATUS fillRect(int x, int y, unsigned int w, unsigned int h) = 0;

	/** Copy rectangle from src_<x,y,w,h> (from src) to dst_<x,y> on
	 * current render target. Disregards any clipping rectangle that
	 * is currently set. Must support overlapping operations (src == render target). */
	virtual OP_STATUS copy(VEGA2dSurface* src,
						   unsigned int src_x, unsigned int src_y,
						   unsigned int src_w, unsigned int src_h,
						   unsigned int dst_x, unsigned int dst_y) = 0;

	/** Blit the rectangle src_<x,y,w,h> from the source surface to
	 * dst_<x,y> in the destination. */
	virtual OP_STATUS blit(VEGA2dSurface* src,
						   unsigned int src_x, unsigned int src_y,
						   unsigned int src_w, unsigned int src_h,
						   int dst_x, int dst_y) = 0;

	/** Same as blit, but also takes a mask that will be used to
	 * modulate the source. The src and mask surfaces are assumed to
	 * be of matching size, and aligned. */
	virtual OP_STATUS blit(VEGA2dSurface* src, VEGA2dSurface* mask,
						   unsigned int src_x, unsigned int src_y,
						   unsigned int src_w, unsigned int src_h,
						   int dst_x, int dst_y) = 0;

	/** Stretch blit from source rectangle src_<x,y,w,h> to the
	 * destination rectangle dst_<x,y,w,h>. */
	virtual OP_STATUS stretchBlit(VEGA2dSurface* src,
								  unsigned int src_x, unsigned int src_y,
								  unsigned int src_w, unsigned int src_h,
								  int dst_x, int dst_y, unsigned int dst_w, unsigned int dst_h) = 0;

	/** Tile blits the source rectangle to the tile area rectangle.
	 * @param src Source surface to read from.
	 * @param src_rect Rectangle within src to use for tiling.
	 * @param tile_area Area to tile within.
	 * @param offset Point within src_rect that should be positioned at tile_area.x/y.
	 * @param tile_h The src_rect will be scaled to this width when painted.
	 * @param tile_w The src_rect will be scaled to this height when painted.
	 */
	virtual OP_STATUS tileBlit(VEGA2dSurface* src,
							   const OpRect& src_rect,
							   const OpRect& tile_area,
							   const OpPoint& offset,
							   unsigned tile_w, unsigned tile_h) = 0;

	struct GlyphInfo
	{
		const UINT8* buffer;
		unsigned buffer_stride;

		int x, y, w, h;
	};

	/** Draw a number of glyphs, as described by the GlyphInfo structures. */
	virtual OP_STATUS drawGlyphs(const GlyphInfo* glyphs, unsigned glyph_count) = 0;

	/** Create a surface. */
	virtual OP_STATUS createSurface(VEGA2dSurface** surface,
									unsigned int width, unsigned int height,
									VEGA2dSurface::Format format) = 0;

#ifdef VEGA_OPPAINTER_SUPPORT
	/** Create a window surface. */
	virtual OP_STATUS createWindow(VEGA2dWindow** window, VEGAWindow* win) = 0;
#endif // VEGA_OPPAINTER_SUPPORT

	/** Set the current render target. */
	virtual OP_STATUS setRenderTarget(VEGA2dSurface* rt) = 0;

	/** @returns the current render target. */
	virtual VEGA2dSurface* getRenderTarget() = 0;

	/** Surface data parameters representing an area of the render target. */
	struct SurfaceData
	{
		VEGAPixelStore pixels;

		unsigned int x;
		unsigned int y;
		unsigned int w;
		unsigned int h;

		void* privateData;
	};

	enum UsageIntent
	{
		WRITE_INTENT = 0x01,
		READ_INTENT = 0x02
	};

	/** Lock a rect in the render target. The data parameter passed to this
	 * function must contain x, y, w and h parameters to specify which area
	 * of the render target to get.
	 * @param data area to get from the render target and resulting data
	 * array.
	 * @param intent A mask specifiy how the returned data is intended to be used:
	 *      WRITE_INTENT: Data will be written.
	 *       READ_INTENT: Data will be read.
	 * This is a hint, that allows the device to perform certain
	 * optimizations with regards to surface migration.
	 * @returns OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS lockRect(SurfaceData* data, unsigned intent) = 0;

	/** Unlock a rectangle locked by lockRect.
	 * @param data the buffer to unlock. */
	virtual void unlockRect(SurfaceData* data) = 0;
};

#endif // VEGA_SUPPORT
#endif // VEGA2DDEVICE_H
