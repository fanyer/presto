/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */
 
#ifndef MDE_OPPAINTER_H
#define MDE_OPPAINTER_H

#include "modules/pi/OpPainter.h"

#if !defined(MDE_PLATFORM_IMPLEMENTS_OPPAINTER) && !defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/libgogi/mde.h"

class MDE_OpPainter : public OpPainter
{
public:
	MDE_OpPainter();

	virtual ~MDE_OpPainter();
	
	void SetPreAlpha(UINT8 alpha) { m_pre_alpha = alpha; }
	UINT8 GetPreAlpha() { return m_pre_alpha; }

	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha = 255);
	virtual void SetFont(OpFont *font);

	virtual OP_STATUS SetClipRect(const OpRect &rect);
	virtual void RemoveClipRect();
	virtual void GetClipRect(OpRect* rect);

	virtual void DrawRect(const OpRect &rect, UINT32 width = 1);
	virtual void FillRect(const OpRect &rect);
	virtual void ClearRect(const OpRect &rect);
	virtual void DrawEllipse(const OpRect &rect, UINT32 width = 1);
	virtual void FillEllipse(const OpRect &rect);
	virtual void DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width = 1);
	virtual void DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width = 1);
	virtual void DrawString(const OpPoint &pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing = 0, short word_width = -1);
	virtual void DrawPolygon(const OpPoint* point_array, int points, UINT32 width = 1);
	virtual void InvertRect(const OpRect &rect);
	virtual void InvertBorderRect(const OpRect &rect, int border);
	virtual void InvertBorderEllipse(const OpRect &rect, int border);
	virtual void InvertBorderPolygon(const OpPoint* point_array, int points, int border);

	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect &source, OpPoint p);

	// [OPTIONAL FUNCTIONS]
	// Should be used only if they are supported. Otherwise they will be done internal. If you get an OP_ASSERT here, then you need to either change your Supports method or implement the asserted method.

	virtual void DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	virtual void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	virtual BOOL DrawBitmapClippedOpacity(const OpBitmap* bitmap, const OpRect &source, OpPoint p, int opacity);

	virtual void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);

	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale) { OP_ASSERT(FALSE); return OpStatus::ERR; };

	/** Creates a bitmap from a rectangle on screen. */
	virtual OpBitmap* CreateBitmapFromBackground(const OpRect& rect);

	/** It is painting on a OpBitmap wich we may get bitspointer access to */
	virtual BOOL IsUsingOffscreenbitmap() { return FALSE; };

	/** Returns the offscreenbitmap */
	virtual OpBitmap* GetOffscreenBitmap() { return NULL; };

	/**
	   Returns the rect to where the offscreenbuffer is located. If f.eks. a paintevent
	   repaints a part i the middle of the document, the painting to the offscreenbitmap must
	   be moved to match that offset.
	*/
	virtual void GetOffscreenBitmapRect(OpRect* rect) {};

	inline void DrawBitmap				(OpBitmap* bitmap, OpPoint p) { DrawBitmapClipped(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }
	inline void DrawBitmapTransparent	(OpBitmap* bitmap, OpPoint p) { DrawBitmapClippedTransparent(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }
	inline void DrawBitmapAlpha			(OpBitmap* bitmap, OpPoint p) { DrawBitmapClippedAlpha(bitmap,
													OpRect(0, 0, bitmap->Width(), bitmap->Height()), p); }

	/**
	   Finds out what the painter supports.
	   @return FALSE if it not has the feature.
	*/
	virtual BOOL Supports(SUPPORTS supports);

	/**
	   Sets the color. Takes an UINT32 instead of separate parameters for red, green, blue.
	   Format should be 0xRRGGBB
	*/
#ifdef OPERA_BIG_ENDIAN
	inline void SetColor(UINT32 col) { SetColor(((UINT8*)&col)[3], ((UINT8*)&col)[2], ((UINT8*)&col)[1]); };
#else
	inline void SetColor(UINT32 col) { SetColor(((UINT8*)&col)[0], ((UINT8*)&col)[1], ((UINT8*)&col)[2]); };
#endif

	UINT32 GetColor();

	virtual OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity) { return OpStatus::ERR; }
	virtual void EndOpacity() {}

	void beginPaint(MDE_BUFFER *s);
	void endPaint();

	MDE_BUFFER* GetScreen() { return screen; }

private:

	void DrawBitmapClippedInternal(MDE_BUFFER* buf, const OpRect &source, OpPoint p);
	void DrawBitmapScaledInternal(MDE_BUFFER* buf, const OpRect &source, const OpRect& dest);
	void DrawBitmapClippedTransparentInternal(MDE_BUFFER* buf, const OpRect& source, OpPoint p);
	void DrawBitmapClippedAlphaInternal(MDE_BUFFER* buf, const OpRect& source, OpPoint p);
	void DrawBitmapScaledTransparentInternal(MDE_BUFFER* buf, const OpRect& source, const OpRect& dest);
	void DrawBitmapScaledAlphaInternal(MDE_BUFFER* buf, const OpRect& source, const OpRect& dest);

	MDE_BUFFER *screen;

	int paint_count;
	struct MdeOpClipRect {
		MDE_RECT rect;
		struct MdeOpClipRect *prev;
	};
	MdeOpClipRect *cliprect_stack;

	UINT8 m_pre_alpha;
};

#endif // !MDE_PLATFORM_IMPLEMENTS_OPPAINTER && !VEGA_OPPAINTER_SUPPORT

#endif // MDE_OPPAINTER_H

