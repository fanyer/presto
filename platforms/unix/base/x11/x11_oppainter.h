/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef __X11_OPPAINTER_H__
#define __X11_OPPAINTER_H__

#include "platforms/unix/base/common/common_oppainter.h"
#include "platforms/unix/base/common/scaling.h"

#ifndef VEGA_OPPAINTER_SUPPORT
#ifdef GADGET_SUPPORT

/********************************************************************
 *
 * struct ShapedWindowData
 *
 *
 ********************************************************************/

struct ShapedWindowData
{
	ShapedWindowData(bool is_tool_shape)
	{
		m_buffer = 0;
		m_buffer_width = 0;
		m_buffer_height = 0;
		m_width = 0;
		m_height = 0;
		m_changed = FALSE;
		m_has_applied_shape = FALSE;
		m_is_tool_shape = is_tool_shape;
		m_transparent_pixel = 0;
		m_init_pixel = 0;
	}

	~ShapedWindowData()
	{
		OP_DELETEA(m_buffer);
	}

	// fill_value = 0x00: becomes transparent
	// fill_value = 0xFF: becomes visible

	void setup( int width, int height, unsigned char fill_value=0x00 )
	{
		if( width > 0 && height > 0 )
		{
			if( m_width != width || height != m_height )
			{
				m_width = width;
				m_height = height;

				m_buffer_width  = m_width + 8 - m_width%8;
				m_buffer_height = m_height;

				OP_DELETEA(m_buffer);
				m_buffer = OP_NEWA(unsigned char, (m_buffer_width*m_buffer_height)>>3);
				memset(m_buffer, fill_value, (m_buffer_width*m_buffer_height)>>3);
			}
		}
		m_changed = FALSE;
	}

	bool isFullyTransparent()
	{
		int size = (m_buffer_width*m_buffer_height)>>3;
		for( int i=0; i<size; i++ )
			if( m_buffer[i] )
				return false;
		return true;
	}

	bool isToolShape() const { return m_is_tool_shape; }

	void apply( int drawable );

	unsigned char* m_buffer;
	INT32 m_buffer_width;
	INT32 m_buffer_height;
	INT32 m_width;
	INT32 m_height;
	BOOL  m_changed;
	BOOL  m_is_tool_shape;
	BOOL  m_has_applied_shape;
	UINT32 m_transparent_pixel; // Background color after all drawing has been made
	UINT32 m_init_pixel;        // Background color gefore all drawing
};
#endif


/********************************************************************
 *
 * X11OpPainter
 *
 *
 ********************************************************************/

class X11OpPainter : public CommonOpPainter
{
private:
	class X11OpPainterInternal *d;

	X11FontSwitcher m_fontswitcher;
	bool            m_doublebuffer;


	/*******************************************
	 *
	 * ClipRect
	 *
	 *******************************************/

	class ClipRect
	{
	public:
		ClipRect *prev;
		OpRect coords;

		ClipRect(const OpRect &coords)
			: coords(coords) { prev = 0; }
	};

	ClipRect *m_cliprect_stack_top;

	int m_x_offs;
	int m_y_offs;
	int m_viewtrans_x;
	int m_viewtrans_y;

	OpRect m_paintrect;

public:
	X11OpPainter();
	~X11OpPainter();

	void GetDPI(UINT32 *horizontal, UINT32 *vertical);

	void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha=255);
	void SetFont(OpFont *font);

	OP_STATUS SetClipRect(const OpRect &rect);
	void RemoveClipRect();
	void GetClipRect(OpRect *rect);

	void DrawRect(const OpRect &rect, UINT32 width = 1);
	void FillRect(const OpRect &rect);
	void DrawEllipse(const OpRect &rect, UINT32 width = 1);
	void FillEllipse(const OpRect &rect);
	virtual void DrawPolygon(const OpPoint* points, int count, UINT32 width = 1);
	void DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width=1);
	void DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width=1);
	void DrawString(const OpPoint &pos, uni_char *text, UINT32 len, INT32 extra_char_spacing=0
#ifdef PI_CAP_LINEAR_TEXT_SCALE
					, short word_width=-1
#endif // PI_CAP_LINEAR_TEXT_SCALE
		);
	void InvertRect(const OpRect &rect);
	void InvertBorderRect(const OpRect &rect, int border);
	void InvertBorderEllipse(const OpRect &rect, int border);
	void InvertBorderPolygon(const OpPoint *point_array, int points, int border);

	void DrawBitmapClipped(const OpBitmap *bitmap, const OpRect &source, OpPoint p);
	void DrawBitmapClippedTransparent(const OpBitmap *bitmap, const OpRect &source, OpPoint p);
	void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect &source, OpPoint p);
	void DrawBitmapScaled(const OpBitmap *bitmap, const OpRect &source, const OpRect &dest);
	void DrawBitmapScaledTransparent(const OpBitmap *bitmap, const OpRect &source, const OpRect &dest);
	void DrawBitmapScaledAlpha(const OpBitmap *bitmap, const OpRect &source, const OpRect &dest);
	OP_STATUS DrawBitmapTiled(const OpBitmap *bitmap, const OpPoint &source, const OpRect &dest, INT32 scale);
	OP_STATUS DrawBitmapTiled(const OpBitmap *bitmap, const OpPoint &source, const OpRect &dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight);

	OpBitmap *CreateBitmapFromBackground(const OpRect &rect);

	BOOL Supports(SUPPORTS supports);

	OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity) { return OpStatus::ERR; }
	void EndOpacity() {}

	UINT32 GetColor();

	PainterType GetPainterType() { return X11_OPPAINTER; }
	DeviceType GetDeviceType() { return SCREEN; }

	// -------- Implementation specific methods: ---------------------------

	class X11OpPainterInternal *GetInternal() { return d; }
	OP_STATUS Init();

	/** Begin the paint operation. Call End() to end the paint operation. */
	void Begin(
		unsigned long drawable, const OpRect &rect, BOOL nobackbuffer,
		OpPoint *view_translation=0,
		BOOL try_argb_visual = FALSE);

	/** Begin the paint operation. Call End() to end the paint operation. */
	OP_STATUS Begin(class X11OpBitmap *bitmap);

	/** End the paint operation. Must be called before a new Begin(), and also
	 * before TakeOverBitmap().
	 * @param flush_buffer If true, copy the double-buffer bitmap to the screen
	 * (when in double-buffering mode) and remove our reference to the pixmap.
	 * If false, just end the paint operation, so that the bitmap can be gotten
	 * with TakeOverBitmap().
	 */
	void End(bool flush_buffer = true);

	/** Have the caller take over the ownership of the bitmap. Must be called
	 * after End().
	 * @return The bitmap used by this painter, if any, and only if the bitmap
	 * is owned by this painter. Returns NULL otherwise.
	 */
	X11OpBitmap *TakeOverBitmap();

	void SetClipRectInternal(const OpRect &rect);

	void UseDoublebuffer(BOOL on) { m_doublebuffer = on; }
	BOOL HasDoublebuffer() const  { return m_doublebuffer; }

	void DrawBitmap(const OpBitmap *bitmap, const OpRect &source,
					const OpPoint &dest, int use_w, int use_h);

	const OpRect& GetPaintRect() { return m_paintrect; }

	UnixFontSwitcher *GetFontSwitcher() { return &m_fontswitcher; }

	void Transform(OpPoint &p) {
		p.x -= m_x_offs;
		p.y -= m_y_offs;
	}

	void Transform(OpRect &r) {
		r.x -= m_x_offs;
		r.y -= m_y_offs;
	}

#ifdef GADGET_SUPPORT
	void ApplyShape(int drawable, ShapedWindowData& shape);
#endif
};
#endif // !VEGA_OPPAINTER_SUPPORT
#endif // __X11_OPPAINTER_H__
