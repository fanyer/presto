/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __X11_OPPAINTER_INTERNAL_H__
#define __X11_OPPAINTER_INTERNAL_H__

#include "platforms/fontix/src/x11_fontswitcher.h"

#include "platforms/utilix/x11_all.h"

#ifndef VEGA_OPPAINTER_SUPPORT
class X11OpPainterInternal
{
private:
	X11Types::Display*   m_display;
	X11FontSwitcher*     m_fontswitcher;
	X11Types::VisualPtr  m_visual;
	class X11OpBitmap*   m_bitmap;
	X11OpFont*           m_opfont;
	unsigned long        m_color;
	X11Types::Drawable   m_drawable;
	X11Types::Drawable   m_final_dest;
	GC                   m_gc;
	int                  m_screen;

	X11Types::Region     m_xregion;
	XRectangle           m_cliprect;

	int  m_red, m_green, m_blue, m_alpha;
	int  m_linewidth;
	bool m_color_invalidated;
	bool m_clipped;
	bool m_disabled;
	bool m_owns_bitmap;

#ifdef XFT_RENDER_SUPPORT
	void*   m_xftdraw;
	X11Types::Picture m_xrender_handle;
#endif

	void UpdateClipRect();

public:
	X11OpPainterInternal(X11FontSwitcher *fontswitcher);
	~X11OpPainterInternal();

	X11Types::Display *GetDisplay()  { return m_display; }
	X11Types::Drawable GetDrawable() { return m_drawable; }
	X11Types::Drawable GetFinalDestination() { return m_final_dest; }
	GC GetGC() { return m_gc; }
	int GetScreen() { return m_screen; }
	X11Types::VisualPtr GetVisual() { return m_visual; }

#ifdef XFT_RENDER_SUPPORT
	X11Types::Picture GetXrenderHandle();
#endif // XFT_RENDER_SUPPORT
	XRectangle GetClipRect() { return m_cliprect; }

	class X11OpBitmap *GetBitmap() { return m_bitmap; }
	OP_STATUS PrepareBuffer(const OpRect &r, X11Types::Drawable dest);

	OP_STATUS SetBitmap(X11OpBitmap *bitmap);
	OP_STATUS Begin(X11Types::Display *disp, int scr, X11Types::VisualPtr vis, X11Types::Drawable dest);
	void End();
	X11OpBitmap *TakeOverBitmap();
	void Clean();
	void Disable();
	bool IsDisabled() { return m_disabled; }
	void SetClipRect(int x, int y, int w, int h);
	void RemoveClipping();

	/**
	 * Clip the coordinates passed against the current clip rectangle.
	 */
	void Clip(OpRect &r);

	void SetColor(int r, int g, int b, int a);
	void UpdateColor();
	UINT32 GetColor() {
		return ((m_alpha & 0xff) << 24) |
			   ((m_blue & 0xff) << 16) |
			   ((m_green & 0xff) << 8) |
			   ( m_red & 0xff);
	}
	void Invert(bool on);
	void SetLineWidth(int width);
	void SetFont(X11OpFont *font);

	void DrawString(int x, int y, const uni_char *text, int len);
	void DrawStringFragment(int x, int y, const uni_char *text, int len);
};
#endif // !VEGA_OPPAINTER_SUPPORT
#endif // __X11_OPPAINTER_INTERNAL_H__
