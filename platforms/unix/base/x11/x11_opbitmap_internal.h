/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef X11_OPBITMAP_INTERNAL_H
#define X11_OPBITMAP_INTERNAL_H

#include "platforms/unix/base/x11/x11_globals.h"

#include "platforms/utilix/x11_all.h"
#ifndef VEGA_OPPAINTER_SUPPORT

class X11ColorManager;
class OpPainter;

/**
 * Internal class, specific for this OpBitmap implementation. It's declared
 * here, so that files including x11_opbitmap.h don't get their namespace
 * polluted by Xlib.  This header would ideally be inlined in x11_opbitmap.cpp,
 * since nothing else should ever #include it - however, several other things
 * do: we should fix that !
 */
class X11OpBitmapInternal
{
public:
	/** Create an image with undefined pixel values. */
	X11OpBitmapInternal(int width, int height);

	/** Create an image that is a (possibly) scaled version of (possibly) an
	 * excerpt/fragment of 'orig'
	 * @param width total width (not area) of scaled image
	 * @param height total height (not area) of scaled image
	 * @param area fragment of original image that is to be scaled. The
	 * coordinates and dimensions are specified as scaled. NULL-pointer is OK.
	 * @param manipulable if true, client data must be kept, so that core
	 * (or whatever) may manipulate the image.
	 * @param orig the original image
	 */
	X11OpBitmapInternal(int width, int height, const OpRect *area, bool manipulable, X11OpBitmapInternal *orig);

	/**
	 * Initialize an X11OpBitmapInternal object with the Ximage pointer
	 * provided. The X11OpBitmapInternal takes ownership over 'img'.
	 */
	X11OpBitmapInternal(XImage *img);

	~X11OpBitmapInternal();


	int IncRef() { return ++m_refcount; }
	int DecRef() { OP_ASSERT(m_refcount > 0); return --m_refcount; }

	OP_STATUS InitStatus() const { return m_oom ? OpStatus::ERR_NO_MEMORY : OpStatus::OK; }

	/** Toggle image manipulability.
	 * Set a flag that tells whether this image is to be manipulated in any
	 * way (read/write pixels on the client side). Initially, images are
	 * manipulable, but this requires extra memory, so if we know that the
	 * client side no longer needs to read or write pixels to/from this image,
	 * there's a lot of memory to be saved by not keeping the client side
	 * structures (the XImage objects).
	 */
	void SetManipulable(bool m) { m_manipulable = m; }

	/** Return true if this image needs to stay on the client. */
	bool NeedsClientData();

	/** Return true if all image data is already on the server. Sometimes it is
	 * wise to split large client images into smaller server pixmap fragments,
	 * but if the data is already on the server, this is pointless. */
	bool UseServerData();

	X11ColorManager *GetColorManager() const { return m_colormanager; }

	/** @return the XImage pointer for this image. */
	XImage *GetImage() const { return m_true_color_image; }

	/** Ensure that direct pixel access is possible.
	 * @return false If the operation failed
	 */
	bool PrepareBuffer();

	/** Set a pre-initialized pixmap to be used for the server-side data.
	 * @param pixmap The pre-initialized pixmap handle. The handle will NOT be
	 * taken over by X11OpBitmapInternal.
	 */
	void SetPixmap(X11Types::Pixmap pixmap);

	/** @return the X11Types::Pixmap that contains this image. If it doesn't exist, it
	 * will be created.
	 */
	X11Types::Pixmap GetPixmap();

	/** Set the opacity of the entire image. */
	void SetOpacity(UINT8 opacity);

	/** Report that the XImage (and other client data) has been changed.
	 * Call this when the image has been modified, so that the X11Types::Pixmap can
	 * be synced with it, if necessary. This will set the "changed" flag, so
	 * that the image and the pixmap will be synced when
	 * @ref CopyImageToPixmap() is called.
	 */
	void ClientDataChanged() { m_image_data_changed = m_reconvert_screen_depth_image = true; }

	/** Report that the pixmap has been changed.
	 * Call this when the pixmap has been modified, so that the XImage can
	 * be synced with it, if necessary. This will set the "changed" flag, so
	 * that the image and the pixmap will be synced when
	 * @ref CopyPixmapToImage() is called.
	 */
	void ServerDataChanged() { m_pixmap_data_changed = m_reconvert_truecolor_image = true; }

	bool NeedsSync() const { return m_image_data_changed || m_pixmap_data_changed; }

	void ColorcellsChanged();

	/** @return the width of the image. */
	int Width() const { return m_width; }

	/** @return the height of the image. */
	int Height() const { return m_height; }

	void *GetBuffer(bool screen_depth) {
		return (screen_depth ? m_screen_depth_image : m_true_color_image)->data;
	}

	int GetBytesPerLine(bool screen_depth) {
		return (screen_depth ? m_screen_depth_image : m_true_color_image)->bytes_per_line;
	}

	int GetBytesPerPixel(bool screen_depth) {
		return (screen_depth ? m_screen_depth_image : m_true_color_image)->bits_per_pixel >> 3;
	}

	/** Get a pixel value in the image.
	 *
	 * Note that no syncing between X11Types::Pixmap and XImage is done here.
	 * (see @ref SyncClientData()).
	 *
	 * @param x the X coordinate for the pixel
	 * @param y the Y coordinate for the pixel
	 * @return the pixel value at this point
	 */
	unsigned long GetPixel(int x, int y) { return XGetPixel(m_true_color_image, x, y); }

	/** Change a pixel in the image.
	 *
	 * Note that the "changed" flag for the image is not set here
	 * (see @ref ClientDataChanged()).
	 *
	 * @param x the X coordinate for the pixel to change
	 * @param y the Y coordinate for the pixel to change
	 * @param pixel the new pixel value
	 */
	void PutPixel(int x, int y, unsigned long pixel) { XPutPixel(m_true_color_image, x, y, pixel); }

	/** Create a transparency mask.
	 *
	 * @param init if true, initialize the mask to opaque for all pixels.
	 * Otherwise, leave undefined
	 * @return false if something failed
	 */
	bool CreateTransparencyMask(bool init=true);

	/** Create a transparency mask.
	 *
	 * @param init if true, initialize the buffer to opaque for all pixels.
	 * Otherwise, leave undefined
	 * @return false if something failed
	 */
	bool CreateAlphaBuffer(bool init=true);

	/** @return whether the image has a transparency mask or not. */
	bool HasTransparencyMask() const { return !!m_transparency_mask; }

	/** @return whether the image has an alpha buffer or not. */
	bool HasAlphaBuffer() const { return !!m_alpha_buffer; }

	/** @return true if there is a pending opacity value that applies to
	 * all pixels in the image. The pending opacity value will be kept
	 * as long the client doesn't need to read/write alpha/transparency
	 * information, and sent to the server in a cheap way when necessary. */
	bool HasPendingOpacity() const { return m_pending_opacity != -1; }

	/** @return true if the pixmap has been allocated and it uses shared memory */
	bool HasSharedMemory() const { return false; }

	/**
	 * Get a pointer to a line in the transparency mask.
	 * @param line The line number
	 * @return false if line number is out of range
	 */
	char *GetTransparencyMaskLine(int line);

	/** Get a pointer to a line in the alpha buffer.
	 *
	 * @param line The line number
	 * @return false if line number is out of range
	 */
	unsigned char *GetAlphaBufferLine(int line);

	/** Create a tile of this image.
	 *
	 * @param reqwidth requested width of the tile. May not be honored
	 * @param reqwidth requested height of the tile. May not be honored
	 */
	X11OpBitmapInternal *GetTile(int reqwidth, int reqheight);

	/** Make sure that the pixmap and other X server data is up-to-date. */
	void CopyImageToPixmap();

	/** Make sure that the image contains the same data as the pixmap */
	void CopyPixmapToImage();

	bool IsCompatibleWith(X11OpBitmapInternal *with) const { return with->m_colormanager == m_colormanager; }

	/** Draw the image with the specified painter.
	 *
	 * @param source offset and size of the excerpt of the original image
	 * @param dx destination X coordinate
	 * @param dy destination Y coordinate
	 */
	void Draw(OpPainter *painter, const OpRect &source, int dx, int dy);

#ifdef XFT_RENDER_SUPPORT
	bool HasXRenderImage() const { return m_xrender_mask != None && m_xrender_image != None; }
#endif
	bool CanUseXRender(X11OpBitmapInternal* bi);

	void SetIsShaped(bool state) { m_is_shaped=state; }

private:
	void DrawAlpha(OpPainter *painter,
				   X11Types::Display *display, X11Types::Drawable drawable, GC gc,
				   int x, int y, int width, int height,
				   int sx, int sy);

	bool PrepareServerData(X11OpBitmapInternal *bi, const OpRect *area=0);
	void FreeClientData();
	void FreeServerData();
	bool CopyTrueColorImageToScreenDepthImage();
	bool CopyScreenDepthImageToTrueColorImage();
	bool RequestScreenDepthImage();
	bool RequestTrueColorImage();
	bool CreateScreenDepthImage();
	bool CreateTrueColorImage();

private:
	X11OpBitmapInternal* m_tile;
	XImage* m_true_color_image;
	XImage* m_screen_depth_image;
	char* m_transparency_mask;
	unsigned char* m_alpha_buffer;
	X11ColorManager* m_colormanager;
	int m_width;
	int m_height;
	X11Types::Pixmap m_pixmap_image;
	X11Types::Pixmap m_pixmap_mask;
	int m_transparency_mask_bpl; // bytes per line
	int m_refcount;
	int m_pending_opacity;
	bool m_oom;
	bool m_manipulable;
	bool m_image_data_changed;
	bool m_pixmap_data_changed;
	bool m_incompatible_format;
	bool m_reconvert_screen_depth_image;
	bool m_reconvert_truecolor_image;
	bool m_owns_pixmap;
#ifdef XFT_RENDER_SUPPORT
	X11Types::Picture m_xrender_image;
	X11Types::Picture m_xrender_mask;
#endif // XFT_RENDER_SUPPORT
	int m_premultiply_count;
	bool m_is_shaped;
};

#endif // !VEGA_OPPAINTER_SUPPORT
#endif // X11_OPBITMAP_INTERNAL_H
