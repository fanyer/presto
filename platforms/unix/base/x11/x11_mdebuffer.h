/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_MDEBUFFER_H__
#define __X11_MDEBUFFER_H__

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/libgogi/mde.h"
#include "platforms/utilix/x11_all.h"

class X11Widget;
class OpBitmap;

/** Abstract interface for back buffers.
 *
 * Used with X11MdeBuffer::SetRealBackbuffer() to provide X11MdeBuffer
 * with a means to manipulate the actual back buffer in the cases
 * where the internal buffer of X11MdeBuffer is not used.
 */
class X11Backbuffer
{
public:
	virtual ~X11Backbuffer() {}

	/** Make the given rectangle of the window completely transparent
	 * black.
	 *
	 * @param rect The rectangle (in buffer coordinates) to clear.
	 */
	virtual void Erase(const MDE_RECT &rect) = 0;

	/** Returns the width of the back buffer in pixels.
	 */
	virtual UINT32 GetWidth() = 0;

	/** Returns the height of the back buffer in pixels.
	 */
	virtual UINT32 GetHeight() = 0;
};

class X11MdeBuffer
{
public:

	/**
	 * Sets up the buffer
	 */
	X11MdeBuffer(X11Types::Display *display,
				 int screen,
				 X11Types::Window window,
				 X11Types::VisualPtr visual,
				 UINT32 width,
				 UINT32 height,
				 UINT32 depth,
				 UINT32 bits_per_pixel);

	/**
	 * Removes any allocated resources
	 */
	~X11MdeBuffer();

	/**
	 * Modify the existing image opacity. It is assumed that the currently
	 * stored image data is premultiplied.
	 *
	 * @param opacity The image will become more transparent with smaller values
	 */
	void LowerOpacity(UINT8 opacity);

	/**
	 * Sets or stops shaping of this window. All windows with 32bit
	 * depth will automatically support shaping, while all 24bit windows
	 * do not support it.
	 *
	 * @param use_shaping Enable shaping if TRUE, stops if FALSE
	 */
	void SetUseShape(BOOL use_shaping);

	/**
	 * Repaints the window using the current content of the buffer
	 *
	 * @param rect The area that will be repainted. The rectangle refers to
	 *        the area in the window that will be painted (important to know
	 *        if the buffer is smaller than the window)
	 * @param x Adjusts the offset where data is written to screen. A negative value
	 *        is ignored
	 * @param y Adjusts the offset where data is written to screen. A negative value
	 *        is ignored
	 */
	void Repaint(const OpRect& rect, int x=-1, int y=-1);

	/**
	 * Updates the buffer's shape mask. The mask make it possible to
	 * "click through" a fully transparent part of the buffer
	 *
	 * @param region The area that will be updated
	 */
	void UpdateShape(const OpRect &region);

	/**
	 * Sets up the internal buffer so that painting is possible.
	 *
	 * @param width  Buffer width
	 * @param height Buffer height
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS AllocateBuffer(UINT32 width, UINT32 height);

	/**
	 * In some cases (e.g. when using the OpenGL rendering backend),
	 * the back buffer is not controlled directly by the X11MdeBuffer
	 * itself.  Instead, the X11MdeBuffer will pass any requests to
	 * read or modify the data in the back buffer along to the real
	 * back buffer.  In these cases, this method should be used to
	 * give X11MdeBuffer access to the real back buffer.
	 *
	 * This method may be called several time.  Each time it is
	 * called, the X11MdeBuffer will stop using the old back buffer
	 * and switch to the new one.  This method can also be called with
	 * the NULL object to have the X11MdeBuffer stop using any
	 * external back buffer at all.
	 *
	 * It is currently not possible to switch between using an
	 * external back buffer or the internal buffer.  This
	 * functionality may be added in the future, should it turn out to
	 * be useful.
	 *
	 * @param backbuffer The object that should be used from now on to
	 * actually manipulate the data of the real back buffer.
	 */
	void SetExternalBackbuffer(X11Backbuffer * backbuffer);

	/**
	 * Returns the number of bytes needed to hold a line of the buffer
	 */
	UINT32 GetBytesPerLine() const { return m_bytes_per_line; }

	/**
	 * Returns the width in pixels of the buffer
	 */
	UINT32 GetWidth() const
	{
		if (m_external_backbuffer) return m_external_backbuffer->GetWidth();
		return m_width;
	}

	/**
	 * Returns the height in pixels of the buffer
	 */
	UINT32 GetHeight() const
	{
		if (m_external_backbuffer) return m_external_backbuffer->GetHeight();
		return m_height;
	}

	/**
	 * Returns the depth in bits of the buffer (24 or 32)
	 */
	UINT32 GetDepth() const { return m_depth; }

	/**
	 * Returns the bits per pixel used by the buffer Vega is rendering to.
	 * Vega can't handle all depths so returning something strange might cause Opera to not start.
	 */
	UINT32 GetBitsPerPixel() const { return m_bits_per_pixel; }

	/**
	 * Returns a pointer to the buffer
	 */
	void *GetImageBuffer() const { return m_image ? m_image->data : 0; }

	/**
	 * Returns TRUE if the buffer consists of a client-side
	 * image.
	 *
	 * Returns FALSE otherwise.
	 */
	BOOL UsesXImage() const { return m_image != NULL; }

	/**
	 * Returns TRUE if the buffer consists of a server-side X11
	 * pixmap.
	 *
	 * Returns FALSE otherwise.
	 */
	BOOL UsesDrawable() const { return FALSE; }

	/**
	 * Returns TRUE if the buffer may be able to copy image data
	 * between an X11 drawable and the buffer's image data.
	 *
	 * There may be severe limitations, though.  For example, it may
	 * only work during painting.  So it should only be used as a
	 * last-ditch fallback when no more reliable option exists
	 * (e.g. when UsesXImage() or UsesDrawable() returns TRUE.
	 */
	BOOL CanCopyDrawable() const { return m_external_backbuffer != NULL; };

	/**
	 * Returns TRUE if the pixel format is octet-wise BGRA8888 or BGR888
	 *
	 * By "octet-wise BGRA888", I mean that each pixel is 4 octets
	 * long.  The first octet is the blue component, the second octet
	 * is the green component, the third octet is the red component
	 * and the fourth octet is the alpha component.
	 *
	 * BGR888 behaves the same way, but is three octets and no alpha
	 * component.
	 *
	 * Keep in mind that some parts of opera uses "pixel-wise"
	 * formats.  On big-endian machines, this is equivalent to
	 * "octet-wise" formats, but on little-endian machines it is not.
	 */
	BOOL HasBGRA8888Format() const;

	/**
	 * Returns TRUE if the pixel format is octet-wise BGR565.
	 *
	 * By "octet-wise BGR565", I mean that each pixel is 2 octets
	 * long.  The first octet contains the blue component and half the
	 * green component, and the second octet contains the red
	 * component and the other half of the green component.
	 *
	 * Keep in mind that some parts of opera uses "pixel-wise"
	 * formats.  On big-endian machines, this is equivalent to
	 * "octet-wise" formats, but on little-endian machines it is not.
	 *
	 * WARNING: Libvega's VPSF_RGB565 and VPSF_BGR565 formats have the
	 * component ordering opposite of all other formats.  So if this
	 * method returns TRUE, the format is BGR565, which is equivalent
	 * to vega's RGB565!
	 */
	BOOL HasBGR565Format() const;

	/**
	 * Clears a specified area of the buffer if  @ref GetDepth() returns
	 * 32. The alpha channel is set to 0
	 *
	 * @param rect The rectangle to be cleared
	 */
	void Erase(const MDE_RECT& rect);

	/**
	 * Copy image data to buffer. The internal image buffer gets created if
	 * it does not already exists. Width and height are set to the bitmap size.
	 *
	 * @param bitmap Bitmap with data to copy
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on insuffient
	 *         memory available
	 */
	OP_STATUS SetBitmap(OpBitmap& bitmap);

	/**
	 * Copy data to buffer. The rect argument defines the area to which data is copied
	 * and is automatically clipped to width and height of the mde buffer
	 *
	 * @param bitmap Bitmap with data to copy
	 * @param bitmap_x Horizontal offset in bitmap to copy from
	 * @param bitmap_y Verical offset in bitmap to copy from
	 * @param rect The area to copy to
	 */
	OP_STATUS SetBitmap(OpBitmap& bitmap, UINT32 bitmap_x, UINT32 bitmap_y, const OpRect& rect);

	/** 
	 * Returns the drawable of the buffer. This function will return 0 if
	 * @ref UsesDrawable() returns FALSE
	 */
	uint32_t GetPluginDrawable() const { return 0; }

	/**
	 * Returns the XImage structure of the buffer. This function will return 0 if
	 * @ref UsesXImage() returns FALSE
	 */
	XImage* GetPluginXImage() const { return UsesXImage() ? m_image : 0; }

	/**
	 * Returns the display connetion
	 */
	X11Types::Display* GetDisplay() const { return m_display; }

	/**
	 * Returns a graphical context handle that has been prepared for the window
	 * this buffer represents. The handle is maintained by the buffer and will
	 * be released when the buffer is destroyed.
	 */
	GC GetPluginGC();

	void ReleasePluginDrawable() { /* No-op */ }

	/**
	 * Copies the given area of the buffer to the top-left part of the
	 * X11 Drawable 'drawable'.  'drawable' is assumed to be at least
	 * as large as 'source_rect'.  Any pixels copied from outside the
	 * buffer has undefined value.
	 *
	 * This method may fail for various reasons.  It really shouldn't
	 * be used if there is a more reliable method available (e.g. when
	 * UsesXImage() or UsesDrawable() returns TRUE.)
	 *
	 * @param drawable The X11 Drawable to copy into.
	 * @param source_rect The area of the buffer to copy from.
	 * @param painter Either NULL or an active painter for this buffer.
	 */
	OP_STATUS CopyBufferDataToDrawable(uint32_t drawable, const OpRect &source_rect, OpPainter * painter);

	/**
	 * Copies the image data from the top-left of the X11 Drawable
	 * 'drawable' to the given area of the buffer.  'drawable' is
	 * assumed to be at least as large as 'dest_rect'.  Any pixels
	 * copied from outside 'drawable' will have undefined value.
	 *
	 * This method may fail for various reasons.  It really shouldn't
	 * be used if there is a more reliable method available (e.g. when
	 * UsesXImage() or UsesDrawable() returns TRUE.)
	 *
	 * @param drawable The X11 Drawable to copy from.
	 * @param dest_rect The area of the buffer to overwrite.
	 * @param painter Either NULL or an active painter for this buffer.
	 */
	OP_STATUS CopyDrawableToBufferData(uint32_t drawable, const OpRect &dest_rect, OpPainter * painter);

	bool ScrollPixelsSupported()
	{
		/* If we are using OpenGL rendering, we can not safely update
		 * the front buffer without also updating the back buffer.
		 * And we haven't implemented that.
		 */
		return m_external_backbuffer == NULL;
	}

	void ScrollPixels(const MDE_RECT &rect, int dx, int dy);

	/**
	 * Utility function for premultiplying a buffer
	 *
	 * @param buf The buffer to be premultiplied
	 * @param size  Number of 32-bit elements in the buffer
	 *
	 * @return The buffer
	 */
	static UINT32* Premultiply(UINT32* buf, UINT32 size);

private:

	/* These are the possible back buffers.  No more than one of them
	 * should be non-null at any time.
	 */
	XImage* m_image;
	X11Backbuffer* m_external_backbuffer;

	UINT8* m_shape_buffer;

	UINT32 m_width;
	UINT32 m_height;
	UINT32 m_depth;
	UINT32 m_bits_per_pixel;
	UINT32 m_bytes_per_line;
	UINT32 m_buffer_size; // number of bytes
	BOOL m_use_shaping;

	X11Types::Display* m_display;
	int m_screen;
	X11Types::Window m_window;
	X11Widget* m_widget;
	X11Types::VisualPtr m_visual;
	GC m_gc;
	// For windowless non shared memory plugins
	GC m_plugin_gc;
};

#endif // VEGA_OPPAINTER_SUPPORT

#endif // __X11_MDEBUFFER_H__
