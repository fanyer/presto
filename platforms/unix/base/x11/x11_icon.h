/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef _X11_ICON_H_
#define _X11_ICON_H_

#include "platforms/utilix/x11_all.h"

class OpBitmap;

class X11Icon
{
public:
	X11Icon(int screen=-1);
	~X11Icon();

	OP_STATUS SetBuffer(OpBitmap* bitmap);
	OP_STATUS SetBuffer(class OpWidgetImage* image);
	OP_STATUS SetBuffer(const UINT8* buffer, UINT32 num_bytes);
	OP_STATUS SetPixmap(OpBitmap* bitmap, BOOL make_mask, UINT32 depth = 24, UINT32 color = 0x00FFFFFF);
	OP_STATUS SetPixmap(class OpWidgetImage* image, BOOL make_mask, UINT32 depth = 24, UINT32 color = 0x00FFFFFF);
	OP_STATUS SetPixmap(const UINT8* buffer, UINT32 num_bytes, BOOL make_mask, UINT32 depth = 24, UINT32 color = 0x00FFFFFF);

	/**
	 * Install icon on window.
	 *
	 * @param window Window to receive icon data
	 */
    void Apply(X11Types::Window window);

	/**
	 * Clears allocated data. Use this if reusing the icon object
	 */
	void Reset();

	/**
	 * Modified dithering when paiting pixmap on bitmap with no alpha
	 */
	void SetDitherLevel(INT32 level) { m_dither_level = level; }

	/**
	 * Sets size of returned icon. Using this can introduce scaling on the applied
	 * image
	 */
	void SetSize(UINT32 width, UINT32 height) { m_width = width; m_height = height; }

	/**
	 *  Sets the background to a single color
	 */
	OP_STATUS SetBackground( UINT32 color );

	/**
	 * Sets the background using handle as the source
	 */
	OP_STATUS SetBackground( X11Types::Drawable handle, OpRect& rect, UINT32 depth );

	/**
	 * Returns bitmaps if SetBuffer() has been used, otherwise NULL. Bitmap is valid
	 * until next call to SetBuffer()
	 */
	OpBitmap* GetBitmap() { MakeBitmap(); return m_bitmap; }

	/**
	 * Returns pixmap if SetPixmap() has been used, otherwise None
	 */
	X11Types::Pixmap GetPixmap() const { return m_pixmap; }

	/**
	 * Returns pixmap mask if SetPixmap has been used with make_mask parameter
	 * set, otherwise None
	 */
	X11Types::Pixmap GetMask() const { return m_mask; }

	/**
	 * Returns buffer if SetBuffer() has been used, otherwise NULL
	 * Returns 32 bit elements. Format <width><height><data>...<data>
	 * <data> is ARGB
	 */
	UINT32* GetBuffer() const { return m_buffer; }

	/**
	 * Returns size of buffer if SetBuffer() has been used. Otherwise 0
	 */
	UINT32 GetBufferSize() const { return m_buffer ? 2 + (m_buffer[0]*m_buffer[1]) : 0; }

	/**
	 * Returns icon width
	 */
	UINT32 GetWidth() const { return m_width; }

	/**
	 * Returns icon height
	 */
	UINT32 GetHeight() const { return m_height; }

	/**
	 * Returns icon depth
	 */
	UINT32 GetDepth() const { return m_depth; }

	/**
	 * Blends two lines together
	 */
	void BlendLine( const UINT32* src, UINT32* dest, UINT32 size );

	/**
	 * Utility function for setting standard application icon on
	 * the window that belongs to 'widget'. It only makes sense
	 * to set it on top level windows.
	 */
	static void SetApplicationIcon( class X11Widget* widget );

	//void DumpBuffer(BOOL c_format);

private:
	OP_STATUS ScaleBuffer(UINT32 width, UINT32 height);
	OP_STATUS MakeBitmap();
	OP_STATUS MakeMask( BOOL use_buffer, UINT32 mask_color );
	OP_STATUS MakeBuffer(OpBitmap* bitmap);

	/** Create an OpBitmap as a copy of the image in an OpWidgetImage.
	 *
	 * This method will return a newly created OpBitmap in 'ret_bmp'.
	 * This pixmap will have size 'width' x 'height'.  It will be
	 * filled with 'bg_color' and then 'image' will be painted into
	 * it.
	 *
	 * If the function does not return OpStatus::OK, '*ret_bmp' will
	 * be 0 on return.  Otherwise, '*ret_bmp' will be a valid OpBitmap
	 * that must be deleted by the caller (using OP_DELETE).
	 *
	 * @param image The image to copy.
	 * @param ret_bmp Returns the newly created OpBitmap.
	 * @param width The width of the newly created OpBitmap.
	 * @param height The height of the newly created OpBitmap.
	 * @param bg_color The background colour to use in opera's
	 *        COLORREF format.  Must be a pure colour, not a magic value.
	 * @return Success or error status.
	 */
	OP_STATUS CreateOpBitmapFromOpWidgetImage(OpWidgetImage * image, OpBitmap ** ret_bmp, UINT32 width, UINT32 height, COLORREF bg_color);

private:
	OpBitmap*         m_bitmap;
	X11Types::Pixmap  m_pixmap;
	X11Types::Pixmap  m_mask;
	int     m_screen;
	UINT32  m_width;
	UINT32  m_height;
	UINT32  m_depth;
	INT32   m_dither_level;
	UINT32* m_buffer;
	UINT32* m_background_buffer;
	BOOL    m_has_background_color;
	UINT32  m_background_color;
};


#endif
