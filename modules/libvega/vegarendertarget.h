/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGARENDERTARGET_H
#define VEGARENDERTARGET_H

class VEGAFill;
class VEGARenderer;
class VEGABackingStore;
class VEGAWindow;

/** A render target used as the bitmap output for the renderer. */
class VEGARenderTarget
{
public:
	// Needed for optimized buffer swapping in WebGL
	friend class CanvasContextWebGL;
	VEGARenderTarget() : renderer(NULL), format(RT_RGBA32) {}

	/**
	   Destroy and delete target.
	 */
	static void Destroy(VEGARenderTarget* renderTarget);

	/** Flush the pending operations and present the result. Usually only
	 * needs to be implemented for software windows. */
	virtual void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0){}

	/** Indicate that the underlying backing store has changed. Usually only
	 * needs to be implemented by externally backed render targets. */
	virtual void invalidate();

	/** Copy this render target, or a part of it, to an OpBitmap. If trying
	 * to read outside the render target or bitmap, an error will be
	 * returned and nothing will be copied.
	 * @param bmp the bitmap to copy to.
	 * @param width the width of the block to copy, 0 will copy the entire
	 * render target.
	 * @param height the height of the block to copy, 0 will copy the
	 * entire render target.
	 * @param srcx the offset in the render target to start copy from.
	 * @param srcy the offset in the render target to start copy from.
	 * @param dstx the offset in the bitmap to start copy to.
	 * @param dsty the offset in the bitmap to start copy to. */
	virtual OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width = 0, unsigned int height = 0, unsigned int srcx = 0, unsigned int srcy = 0, unsigned int dstx = 0, unsigned int dsty = 0) = 0;

	/** Retrieve the width of the render target. */
	virtual unsigned int getWidth() = 0;

	/** Retrieve the height of the render target. */
	virtual unsigned int getHeight() = 0;

	/** Render target color format. This affects how internal data is
	 * stored, and how the render target can be used. */
	enum RTColorFormat
	{
		RT_RGBA32,
		RT_ALPHA8
	};

	/** Get the color format used by this render target.
	 * Color format will for now be RGBA32 for all targets
	 * except non component based stencil buffers which will
	 * be format ALPHA8. */
	RTColorFormat getColorFormat() { return format; }

	/** Get a fill object which can be used to render this render target
	 * onto another render target. The render target owns the image, so do
	 * not delete it. */
	virtual OP_STATUS getImage(VEGAFill** fill) = 0;

	/** For internal use only!
	 *
	 * WARNING! This is for internal use by the VEGA rendering
	 * infrastructure.  The dirty rect is only kept up to date to the
	 * extent that it is needed by the VEGA rendering infrastructure.
	 * Calling this method from "the outside" WILL give you the wrong
	 * answer.  This is not a bug.
	 *
	 * Get the dirty rectangle.  Everything outside this area is fully
	 * transparent black.  (NOTE: the 'right' and 'bottom' values are
	 * considered to be part of the dirty rect.  So the width of the
	 * dirty rect is "right - left + 1".)
	 *
	 * The "dirty rect" is an optimization primarily used for avoiding
	 * some specific, potentially costly operations on stencil
	 * buffers.
	 *
	 * Returns true if there is a "dirty rect".  If this method
	 * returns false, the whole rendertarget should be considered
	 * "dirty", and the return arguments will have undefined values. */
	virtual bool getDirtyRect(int& left, int& right, int& top, int& bottom)
	{
		return false;
	}

	/** Mark a section of the render target as dirty.  See
	 * getDirtyRect() for details. */
	virtual void markDirty(int left, int right, int top, int bottom){}

	/** Mark a section of the render target as "not dirty".  See
	 * getDirtyRect() for details. */
	virtual void unmarkDirty(int left, int right, int top, int bottom) {}

	/** Empty the dirty rect (i.e. mark the whole render target as
	 * "not dirty").  See getDirtyRect() for details. */
	virtual void resetDirtyRect() {}

#ifdef VEGA_OPPAINTER_SUPPORT
	virtual VEGAWindow* getTargetWindow(){return NULL;}
#endif // VEGA_OPPAINTER_SUPPORT

protected:
	virtual ~VEGARenderTarget();

	void OnBind(VEGARenderer* r)
	{
		OP_ASSERT(renderer == NULL); // Handle or worry?
		renderer = r;
	}
	void OnUnbind() { renderer = NULL; }

	friend class VEGARenderer;
	friend class VEGABackend_SW;
#ifdef VEGA_2DDEVICE
	friend class VEGABackend_HW2D;
#endif // VEGA_2DDEVICE
#ifdef VEGA_3DDEVICE
	friend class VEGABackend_HW3D;
#endif // VEGA_3DDEVICE
	friend class VEGAFilter;
	// FIXME: temporary until get/put pixel is fixed
	friend class CanvasContext2D;
#ifdef VEGA_OPPAINTER_SUPPORT
	friend class VEGAOpBitmap;
#endif // VEGA_OPPAINTER_SUPPORT
	friend class Canvas;

	VEGABackingStore* GetBackingStore() { return backingstore; }

	VEGABackingStore* backingstore;

	// If the render target is bound, this is the renderer it is bound to
	VEGARenderer* renderer;

	RTColorFormat format;
};

class VEGAAutoRenderTarget
{
public:
	VEGAAutoRenderTarget(VEGARenderTarget* r = NULL) : m_renderTarget(r) {}
	~VEGAAutoRenderTarget()
	{
		VEGARenderTarget::Destroy(m_renderTarget);
	}
	VEGARenderTarget* get() { return m_renderTarget; }
	void reset(VEGARenderTarget* s)
	{
		if (s != m_renderTarget)
		{
			VEGARenderTarget::Destroy(m_renderTarget);
			m_renderTarget = s;
		}
	}
	VEGARenderTarget* operator->() { return get(); }

private:
	VEGARenderTarget* m_renderTarget;
};

#endif // !VEGARENDERTARGET_H

