/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPPAINTER_H
#define OPPAINTER_H

#include "modules/pi/OpBitmap.h"

class OpFont;


/** @short Paint context for a paint device.
 *
 * A paint device may be an OpView (for screen), an
 * OpPrinterController (for printer), or an OpBitmap (graphical
 * bitmap).
 *
 * A painter is a temporary object used for painting on an OpView,
 * OpPrinterController or OpBitmap. It is provided by calling
 * OpView::GetPainter(), OpPrinterController::GetPainter() or
 * OpBitmap::GetPainter() to start a paint job (e.g. handling of a
 * paint event). To end a paint job, call OpView::ReleasePainter(),
 * OpBitmap::ReleasePainter(), or delete the OpPrinterController.
 * Ending a paint job means flushing the double-buffer (if any) to the
 * screen, printer or graphical bitmap.
 *
 * An implementation is free to keep and provide the same OpPainter
 * object across several subsequent paint jobs, as long as the object
 * that provides it always returns an OpPainter in its initial
 * state. No paint context data is to be re-used from a previous paint
 * job.
 *
 * The painter knows the current color to use when drawing text,
 * lines, rectangles, etc., the current clip rectangle, the current
 * font, etc. The initial color and font are undefined, and MUST be
 * set before attempting to perform an operation that depends on them
 * being set to something reasonable. The initial clip rectangle of a
 * painter covers the whole paint device area, unless otherwise
 * explicitly stated for the method that returns it.
 *
 * Many methods in OpPainter take coordinate arguments. They are
 * always relative to the upper left corner of their paint device,
 * regardless of e.g. the current clip rectangle. If the paint device
 * is an OpView, the coordinates are thus relative to the upper left
 * corner of the rendering viewport, in screen coordinates.
 *
 * GetDPI is removed, and one should use g_op_screen_info to get
 * this information
 */
class OpPainter
{
public:
	enum RenderingHint
	{
		Quality,
		Speed
	};

	virtual ~OpPainter(){};

	/** Set the current color.
	 *
	 * There is no distinction between foreground and background
	 * color.
	 *
	 * @param red Red component, 0-255, 255 is max intensity
	 * @param green Green component, 0-255, 255 is max intensity
	 * @param blue Blue component, 0-255, 255 is max intensity
	 * @param alpha Alpha component, 0-255, 0 is fully transparent,
	 * 255 is fully opaque
	 */
	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha = 255) = 0;

	/**
	 * Sets the pre-alpha value for the painter.
	 *
	 * Any subsequent call to GetPreAlpha() shall return the specified
	 * value. All painting operations shall pre-multiply any alpha
	 * value(s) used in all drawing operations with the specified
	 * pre-set alpha value.
	 * @note The default pre-alpha value is 255 (fully opaque).
	 *
	 * @param alpha is the pre-alpha value to set. 255 is fully opaque,
	 *  0 is fully transparent.
	 */
	virtual void SetPreAlpha(UINT8 alpha) = 0;

	/**
	 * Returns the "global" pre-alpha value for the painter.
	 *
	 * @note This method should return the value of the most recent
	 *  SetPreAlpha() call. The default value, i.e. if SetPreAlpha()
	 *  was not called previously, shall be 255 (fully opaque).
	 */
	virtual UINT8 GetPreAlpha() = 0;

	/** Set the current color.
	 *
	 * There is no distinction between foreground and background
	 * color.
	 *
	 * @param col The new color. The Format is 0xAABBGGRR.
	 */
	inline void SetColor(UINT32 col)
	{
		SetColor(col & 0xff, (col >> 8) & 0xff, (col >> 16) & 0xff, col >> 24);
	}

	/** Set the current font.
	 *
	 * @param font The font
	 */
	virtual void SetFont(OpFont *font) = 0;

	/** Constrain the currently active clip rectangle.
	 *
	 * The parameter-supplied rectangle will be put an a clipping
	 * stack. The currently active clip rectangle will be intersected
	 * with this new rectangle, so that all subsequent draw operations
	 * will be confined to this intersected rectangle.
	 *
	 * To remove the clip rectangle that is at the top of the stack
	 * again, call SetClipRect().
	 *
	 * @param rect The clipping rectangle to intersect with the
	 * current one, relative to the top left corner of the paint
	 * device.
	 */
	virtual OP_STATUS SetClipRect(const OpRect &rect) = 0;

	/** Remove the topmost clip rectangle off the stack.
	 *
	 * The active clip rectangle after this operation becomes the same
	 * as it was before the previous call to SetClipRect().
	 */
	virtual void RemoveClipRect() = 0;

	/** Get the clip rectangle at the top of the stack.
	 *
	 * @param rect (output) Set to the clip rectangle on the top of
	 * the stack. Note that the rectangle returned is not intersected
	 * with previous rectangles on the stack (as opposed to what the
	 * "active" clip rectangle used for drawing operations is -- see
	 * SetClipRect() for more information on that).
	 */
	virtual void GetClipRect(OpRect* rect) = 0;

	/** Draw the outline of a rectangle.
	 *
	 * @param rect The rect where the rectangle should be drawn
	 * @param width The width of the border in pixels
	 */
	virtual void DrawRect(const OpRect &rect, UINT32 width = 1) = 0;

	/** Draw a solid rectangle.
	 *
	 * @param rect The rect where the rectangle should be drawn
	 * @see ClearRect()
	 */
	virtual void FillRect(const OpRect &rect) = 0;

	/** Clear the rect with the current color.
	 *
	 * @param rect The rect where the rectangle should be drawn
	 *
	 * @note The difference between FillRect() and ClearRect() is,
	 * that FillRect() should blend the specified rectangle with the
	 * current color (if it has an alpha value), while ClearRect()
	 * should clear the rectangle with current color.
	 * @see FillRect()
	 */
	virtual void ClearRect(const OpRect &rect) = 0;

	/** Draw the outline of an ellipse.
	 *
	 * @param rect The rect where the ellipse should be drawn
	 * @param width The width of the border in pixels
	 */
	virtual void DrawEllipse(const OpRect &rect, UINT32 width = 1) = 0;

	/** Draw a solid ellipse.
	 *
	 * @param rect The rect where the ellipse should be drawn
	 */
	virtual void FillEllipse(const OpRect &rect) = 0;

	/** Draw the outline of a polygon.
	 *
	 * This method closes polygons' path with a final segment from
	 * points[count-1] to points[0].
	 *
	 * @param points An array of polygon points
	 * @param count Number of points in point_array
	 * @param width Width of the border in pixels
	 */
	virtual void DrawPolygon(const OpPoint* points, int count, UINT32 width = 1) = 0;

	/** Draw a horizontal or vertical line.
	 *
	 * @param from The start position
	 * @param length The length in pixels
	 * @param horizontal Direction - if TRUE, the line will be
	 * horizontal, otherwise it will be vertical
	 * @param width The width in pixels. The width of the line is
	 * spread to the right of a vertical line, and under a horizontal
	 * line.
	 */
	virtual void DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width = 1) = 0;

	/** Draw a line.
	 *
	 * @param from The start position
	 * @param to The end position
	 * @param width The width in pixels
	 */
	virtual void DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width = 1) = 0;

	/** Draw a text string.
	 *
	 * @param pos The upper left corner, where the text should be drawn
	 * @param text The text - does not have to be NUL terminated
	 * @param len The number of characters to draw
	 * @param extra_char_spacing Extra space to insert between characters (pixels)
	 * @param word_width If set (i.e. if this is a positive number), it
	 * specifies the width of the string, in pixels. This overrides whatever
	 * the system's font engine would calculate the width to be. This parameter
	 * is typically set when Opera has calculated the width of a string in one
	 * zoom level, and then expects the text to scale linearly in other zoom
	 * levels. There are different ways to implement this. One way may be to
	 * adjust the inter-character spacing to make it fit. Another way could be
	 * to also scale the characters.
	 */
	virtual void DrawString(const OpPoint &pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing = 0, short word_width = -1) = 0;

	/** Invert the pixels covered by a solid rectangle.
	 *
	 * What's meant by "invert" is implementation-dependant.
	 * True-color systems may invert the color of each pixel. Indexed
	 * color (palette) based systems may invert the color index on
	 * each pixel instead. Regardless of implementation, inverting the
	 * same rectangle twice must result in the same pixels as before
	 * the first inversion.
	 *
	 * @param rect The rect to be inverted
	 */
	virtual void InvertRect(const OpRect &rect) = 0;

	/** Inverts the pixels covered by a rectangle border.
	 *
	 * What's meant by "invert" is implementation-dependant.
	 * True-color systems may invert the color of each pixel. Indexed
	 * color (palette) based systems may invert the color index on
	 * each pixel instead. Regardless of implementation, inverting the
	 * same rectangle twice must result in the same pixels as before
	 * the first inversion.
	 *
	 * @param rect The rectangle to be inverted
	 * @param border Width of the border, in pixels; will grow towards
	 * the center of the rectangle.
	 */
	virtual void InvertBorderRect(const OpRect &rect, int border) = 0;

	/** Invert the pixels covered by an ellipse border.
	 *
	 * What's meant by "invert" is implementation-dependant.
	 * True-color systems may invert the color of each pixel. Indexed
	 * color (palette) based systems may invert the color index on
	 * each pixel instead. Regardless of implementation, inverting the
	 * same rectangle twice must result in the same pixels as before
	 * the first inversion.
	 *
	 * @param rect The rectangle defining the ellipse whose border is
	 * to be inverted
	 * @param border Width of the border, in pixels; will grow towards
	 * the center of the ellipse.
	 */
	virtual void InvertBorderEllipse(const OpRect &rect, int border) = 0;

	/** Invert the pixels covered by a polygon border.
	 *
	 * What's meant by "invert" is implementation-dependant.
	 * True-color systems may invert the color of each pixel. Indexed
	 * color (palette) based systems may invert the color index on
	 * each pixel instead. Regardless of implementation, inverting the
	 * same rectangle twice must result in the same pixels as before
	 * the first inversion.
	 *
	 * This method closes polygons' path with a final segment from
	 * points[count-1] to points[0].
	 *
	 * @param points An array of polygon points
	 * @param count Number of points in point_array
	 * @param border Width of the border
	 */
	virtual void InvertBorderPolygon(const OpPoint* points, int count, int border) = 0;

#if 0
	// Possible replacement for all DrawBitmap*() methods:
	// Spec not complete.

	/** Flags that specify allowed operations in DrawBitmap(). */
	enum BitmapDrawFlags
	{
		/** Draw the image with any inherent transparency (1-bit alpha) information. */
		BITMAP_DRAW_INTRINSIC_TRANSPARENT = 0x01,

		/** Draw the image with any inherent alpha translucency information. */
		BITMAP_DRAW_INTRINSIC_ALPHA = 0x02
	};

	/** Draw an image (or image fragment)
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; The x/y coordinates specify from where
	 * in the image to start drawing. It may also specify scaling: if the
	 * width/height of the rectangle differs from the actual image size, the
	 * image is to be scaled when displayed, and the coordinates specified are
	 * the coordinates after scaling.
	 * @param dest Destination rectangle; The x/y coordinates are relative to
	 * the upper left corner of the paint device, and width and height are the
	 * number of pixels to draw in either dimension. The sum of number of
	 * pixels may exceed the actual number of pixels in the (potentially
	 * scaled) image, in which case the image is to be tiled. Tiling must only
	 * be requested if Supports(SUPPORTS_TILING) returns TRUE.
	 * @param flags BitmapDrawFlags values ORed together
	 * @param opacity Overall opacity. May coexist with BITMAP_INTRINSIC_ALPHA
	 * set in the 'flags' parameter.
	 * @return OK, ERR or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS DrawBitmap(OpBitmap* bitmap, const OpRect& source, const OpRect& dest, int flags, int opacity = 255) = 0;
#endif // 0

	/** Draw an image (or image fragment).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param p Destination coordinate for upper left pixel to draw.
	 */
	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect &source, OpPoint p) = 0;

	// [OPTIONAL DrawBitmap... FUNCTIONS]
	// Should be used only if they are supported. Otherwise they will be done internal. If you get an OP_ASSERT here, then you need to either change your Supports method or implement the asserted method.

	/** Draw an image (or image fragment) with inherent transparency.
	 *
	 * This is an optional method. It should be called only if it is
	 * supported. Query for support by calling Supports(SUPPORTS_TRANSPARENT).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param p Destination coordinate for upper left pixel to draw.
	 */
	virtual void DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p) { OP_ASSERT(FALSE); };

	/** Draw an image (or image fragment) with inherent alpha (translucency).
	 *
	 * This is an optional method. It should be called only if it is
	 * supported. Query for support by calling Supports(SUPPORTS_ALPHA).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param p Destination coordinate for upper left pixel to draw.
	 */
	virtual void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p) { OP_ASSERT(FALSE); };

	/** Draw an image (or image fragment) with fixed opacity.
	 *
	 * All pixels in the bitmap should be drawn using the given
	 * opacity.
	 *
	 * An alternative to this function is to implement
	 * BeginOpacity/EndOpacity. Return TRUE if supported and
	 * successful.
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param p Destination coordinate for upper left pixel to draw.
	 * @param opacity The opacity to use for all pixels; the value can
	 * be between 0 and 254. 255 (fully opaque) should not happen.
	 */
	virtual BOOL DrawBitmapClippedOpacity(const OpBitmap* bitmap, const OpRect &source, OpPoint p, int opacity) { return FALSE; }

	/** Scaled and draw an image (or image fragment).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param dest Destination rectangle. The X and Y coordinates
	 * specify the upper left pixel to draw. Width and height specify
	 * scaling. They are relative to source.width and source.height,
	 * respectively, in such a way that horizontal scale factor is
	 * dest.width divided by source.width, and vertical scale factor
	 * is dest.height divided by source.height.
	 */
	virtual void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) { OP_ASSERT(FALSE); };

	/** Scale and draw an image (or image fragment) with inherent transparency.
	 *
	 * This is an optional method. It should be called only if it is
	 * supported. Query for support by calling Supports(SUPPORTS_TRANSPARENT).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param dest Destination rectangle. The X and Y coordinates
	 * specify the upper left pixel to draw. Width and height specify
	 * scaling. They are relative to source.width and source.height,
	 * respectively, in such a way that horizontal scale factor is
	 * dest.width divided by source.width, and vertical scale factor
	 * is dest.height divided by source.height.
	 */
	virtual void DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) { OP_ASSERT(FALSE); };

	/** Scale and draw an image (or image fragment) with inherent alpha (translucency).
	 *
	 * This is an optional method. It should be called only if it is
	 * supported. Query for support by calling Supports(SUPPORTS_ALPHA).
	 *
	 * @param bitmap The image to draw
	 * @param source Source rectangle; the fragment to draw, or { 0,
	 * 0, bitmap->Width(), bitmap->Height() } for the whole image.
	 * @param dest Destination rectangle. The X and Y coordinates
	 * specify the upper left pixel to draw. Width and height specify
	 * scaling. They are relative to source.width and source.height,
	 * respectively, in such a way that horizontal scale factor is
	 * dest.width divided by source.width, and vertical scale factor
	 * is dest.height divided by source.height.
	 */
	virtual void DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) { OP_ASSERT(FALSE); };

	/** Draw a tiled image, with inherent alpha and transparency if present.
	 *
	 * This is an optional method. It should be called only if it is
	 * supported. Query for support by calling Supports(SUPPORTS_TILING).
	 *
	 * @param bitmap The image to draw
	 * @param offset Start offset into bitmap for tiling. The pixel at
	 * offset.x,offset.y in bitmap will be drawn at dest.x, dest.y
	 * @param dest Destination rectangle dest.x,dest.y specify the destination
	 * of the upper left pixel to draw. dest.width specifies how many pixels to
	 * use from bitmap horizontally, and dest.height specifies how many pixels
	 * to use from bitmap vertically.
	 * @param scale scale percentage (100 means no scaling).
	 */
	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale) { OP_ASSERT(FALSE); return OpStatus::ERR; };

	 /** Scale and draw a tiled image, with inherent alpha and transparency if present.
	  *
	  * This is an optional method. It should be called only if it is
	  * supported. Query for support by calling Supports(SUPPORTS_TILING).
	  *
	  * @param bitmap Bitmap to be drawn
	  * @param offset Scaled offset inside the scaled bitmap of where to start tiling
	  * @param dest Destination rectangle, where the tiled bitmap should be drawn
	  * @param scale The scale of the image
	  * @param bitmapWidth The width of the bitmap pane (after scaling)
	  * @param bitmapHeight The height of the bitmap pane (after scaling)
	  * bitmapWidth, bitmapHeight are not redundant informations, becasue of rounding issues, use them rather than scale
	  */
	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight) { OP_ASSERT(FALSE); return OpStatus::ERR; }

	/** Create and return bitmap of an area on a paint device.
	 *
	 * Read pixels from the paint device (screen, printer, bitmap) and
	 * create a new OpBitmap of them. May be used by core to do
	 * alpha-blending internally, for example.
	 *
	 * @param rect rectangle to create a bitmap from
	 * @return NULL if not implemented, or on error, or the bitmap
	 * otherwise.
	 */
	virtual OpBitmap* CreateBitmapFromBackground(const OpRect& rect) = 0;

	/** Can the painter provide an off-screen OpBitmap that gives
	 * access to the paint area?
	 *
	 * @return TRUE means that GetOffscreenBitmap() can be called.
	 */
	virtual BOOL IsUsingOffscreenbitmap() { return FALSE; };

	/** Returns an off-screen bitmap that gives access to the paint area.
	 *
	 * This may be a portion of the paint device area, not necessarily
	 * the whole thing.
	 */
	virtual OpBitmap* GetOffscreenBitmap() { return NULL; };

	/** Get the position of the bitmap returned from GetOffscreenBitmap().
	 *
	 * @param rect (output) The coordinates set will be relative to
	 * the upper left corner of the paint device. The width and height
	 * will be the same as the width and height of the OpBitmap
	 * returned from GetOffscreenBitmap().
	 */
	virtual void GetOffscreenBitmapRect(OpRect* rect) { (void)rect; }

	inline void DrawBitmap				(OpBitmap* bitmap, OpPoint p) { DrawBitmapClipped(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }
	inline void DrawBitmapTransparent	(OpBitmap* bitmap, OpPoint p) { DrawBitmapClippedTransparent(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }
	inline void DrawBitmapAlpha			(OpBitmap* bitmap, OpPoint p) { DrawBitmapClippedAlpha(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }

	/** Optional features enum. */
	enum SUPPORTS
	{
		/** Can get an area from the screen, i.e. is CreateBitmapFromBackground() supported? */
		SUPPORTS_GETSCREEN,

		/** Is display of images with transparent (1-bit alpha) pixels supported? */
		SUPPORTS_TRANSPARENT,

		/** Is alpha-blending of images supported? */
		SUPPORTS_ALPHA,

		/** Do all other paint operations than displaying images (which is
		 * covered by SUPPORTS_ALPHA) support the alpha component specified
		 * with SetColor()? */
		SUPPORTS_ALPHA_COLOR,

		/** Is bitmap tiling supported? */
		SUPPORTS_TILING,

		/** Is this a platform specific painter (supports platform specific features)? */
		SUPPORTS_PLATFORM
	};

	/** Query the platform about optional features.
	 *
	 * @param supports The feature to check for
	 *
	 * @return TRUE if the feature is supported, FALSE otherwise
	 */
	virtual BOOL Supports(SUPPORTS supports) { (void)supports; return FALSE; };

	/** Initiate an opacity layer, start an opacity operation
	 *
	 * All painting between BeginOpacity() and EndOpacity() should be
	 * done using the specified opacity. The opacity value can be
	 * 0-254. 255 (fully opaque) will be handled by core, and this
	 * method will never be used for handling of that.
	 *
	 * An alternative to this is to implement
	 * OpPainter::DrawBitmapClippedOpacity(). If neither is
	 * implemented, opacity will be done in core, but that may not be
	 * in the most optimal solution for a platform. Note that nested
	 * calls to BeginOpacity() is allowed.
	 *
	 * The platform-independent implementation in core creates a
	 * buffer (OpBitmap) to which all painting between BeginOpacity()
	 * and EndOpacity() is done. When an opacity operation is over,
	 * this buffer is blitted to the screen (or backbuffer) with the
	 * given opacity value.
	 *
	 * If the platform can perform this in a more efficient way, it
	 * should implement BeginOpacity() and EndOpacity().
	 *
	 * @param rect the rectangle to draw with opacity
	 * @param opacity opacity value. 0-254.
	 * @return OpStatus::ERR if the platform wants core to handle
	 * opacity, OpStatus::OK otherwise. OpStatus::ERR_NO_MEMORY on
	 * OOM.
	 */
	virtual OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity) = 0;

	/** End the ongoing opacity operation.
	 *
	 * Note that BeginOpacity() / EndOpacity() calls may be nested.
	 *
	 * See also BeginOpacity().
	 */
	virtual void EndOpacity() = 0;

	/** Get the currently active font. */
	virtual OpFont *GetFont() { return current_font; }

	/** Get the currently active color.
	 *
	 * @return The currently active color. Format: 0xAABBGGRR
	 */
	virtual UINT32 GetColor() = 0;

	/** Set the rendering hint, should take effective immediately for subsequent drawing operation. */
	virtual void SetRenderingHint(RenderingHint) {}

	/** Get the current rendering hint. */
	virtual RenderingHint GetRenderingHint() { return Quality; }

protected:
	OpFont* current_font;
};

#endif // OPPAINTER_H
