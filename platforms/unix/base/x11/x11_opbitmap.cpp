/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "platforms/unix/base/x11/x11_opbitmap.h"

#include "platforms/unix/base/x11/x11_colormanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_imagemanager.h"
#include "platforms/unix/base/x11/x11_opbitmap_internal.h"
#include "platforms/unix/base/x11/x11_tc16_colormanager.h"
#include "platforms/unix/base/x11/x11utils.h"

#include "platforms/unix/base/common/imagecache.h"

#ifdef XFT_RENDER_SUPPORT
# include "platforms/unix/base/x11/xrenderlib.h"
#endif

#include "platforms/utilix/x11_all.h"

#include "platforms/unix/base/x11/x11_oppainter.h"
#include "platforms/unix/base/x11/x11_oppainter_internal.h"

#include <stdlib.h>
#include <unistd.h>

#include <sys/ipc.h>

#define TILE_LIMIT 1024
#define USE_REVERSIBLE_PREMULTIPLY

namespace
{
	bool s_disable_alpha = false;
}

/*
 * X11OpBitmapInternal implementation:
 */

X11OpBitmapInternal::X11OpBitmapInternal(int width, int height)
	: m_tile(0),
	  m_true_color_image(0),
	  m_screen_depth_image(0),
	  m_transparency_mask(0),
	  m_alpha_buffer(0),
	  m_colormanager(g_x11_colormanager),
	  m_width(width),
	  m_height(height),
	  m_pixmap_image(None),
	  m_pixmap_mask(None),
	  m_transparency_mask_bpl(-1),
	  m_refcount(0),
	  m_pending_opacity(-1),
	  m_oom(false),
	  m_manipulable(true),
	  m_image_data_changed(false),
	  m_pixmap_data_changed(false),
	  m_incompatible_format(false),
	  m_reconvert_screen_depth_image(false),
	  m_reconvert_truecolor_image(false),
	  m_owns_pixmap(true)
#ifdef XFT_RENDER_SUPPORT
	  , m_xrender_image(None)
	  , m_xrender_mask(None)
#endif // XFT_RENDER_SUPPORT
	, m_is_shaped(false)
{

	if (width > 30000 || height > 30000 || width <= 0 || height <= 0)
	{
		// Too large
		m_oom = true;
		return;
	}
	if (g_x11_colormanager->IsChangeableColorModel() || g_x11->GetDefaultPixelDepth() == 8)
	{
		/* Keep copy of 16bit truecolor image, in order to perform dithering,
		   and in case the colorcells change. */
		m_incompatible_format = true;
		m_colormanager = g_x11_colormanager_tc16;
	}
}


X11OpBitmapInternal::X11OpBitmapInternal(int width, int height, const OpRect *area, bool manipulable, X11OpBitmapInternal *orig)
	: m_tile(0),
	  m_true_color_image(0),
	  m_screen_depth_image(0),
	  m_transparency_mask(0),
	  m_alpha_buffer(0),
	  m_colormanager(g_x11_colormanager),
	  m_width(0),
	  m_height(0),
	  m_pixmap_image(None),
	  m_pixmap_mask(None),
	  m_transparency_mask_bpl(-1),
	  m_refcount(0),
	  m_pending_opacity(orig->m_pending_opacity),
	  m_oom(false),
	  m_manipulable(manipulable),
	  m_image_data_changed(false),
	  m_pixmap_data_changed(false),
	  m_incompatible_format(false),
	  m_reconvert_screen_depth_image(false),
	  m_reconvert_truecolor_image(false),
	  m_owns_pixmap(true)
#ifdef XFT_RENDER_SUPPORT
	  , m_xrender_image(None)
	  , m_xrender_mask(None)
#endif // XFT_RENDER_SUPPORT
{
	m_is_shaped = false;

	int dx = 0;
	int dy = 0;
	int dh = height;
	int dw = width;

	if (area)
	{
		dx = area->x;
		dy = area->y;
		dw = area->width;
		dh = area->height;
	}
	m_width = dw;
	m_height = dh;

	OP_ASSERT(width > 0 && height > 0 && dw > 0 && dh > 0);

	if (!orig->PrepareBuffer())
	{
		m_oom = true;
		return;
	}
	orig->CopyPixmapToImage();
	if (!orig->CopyScreenDepthImageToTrueColorImage())
	{
		m_oom = true;
		return;
	}

	if (!m_manipulable && width == orig->Width() && height == orig->Height())
	{
		m_oom = !PrepareServerData(orig, area);
	}
	else
	{
		if (!CreateScreenDepthImage())
		{
			m_oom = true;
			return;
		}

		if( orig->m_true_color_image )
		{
			// If original bitmap uses shared segments we will not have a m_true_color_image
			// in that object. TODO: What do we want to do then?

			if (IsCompatibleWith(orig) )
			{
				//puts("scale image");
				OP_STATUS res = m_colormanager->ScaleImage(
					orig->m_true_color_image, m_screen_depth_image,
					orig->GetBuffer(false), GetBuffer(true),
					orig->Width(), orig->Height(), orig->GetBytesPerLine(false),
					m_width, m_height, GetBytesPerLine(true),
					dx, dy, width, height);
				if (OpStatus::IsError(res))
				{
					m_oom = true;
					return;
				}
			}
			else
			{
				OP_STATUS res = X11Utils::ScaleImage(orig->m_true_color_image, m_screen_depth_image,
													 orig->m_colormanager, m_colormanager, dx, dy,
													 width, height);
				if (OpStatus::IsError(res))
				{
					m_oom = true;
					return;
				}
			}
		}

		ClientDataChanged();

		if (orig->m_transparency_mask)
		{
			if (!CreateTransparencyMask(false))
			{
				m_oom = true;
				return;
			}
			if (!X11Utils::ScaleMask((const uint8_t *)orig->m_transparency_mask,
									 (orig->m_width+7)/8,
									 orig->m_width, orig->m_height,
									 (uint8_t *)m_transparency_mask,
									 (m_width+7)/8, m_width, m_height,
									 dx, dy, width, height))
			{
				m_oom = true;
				return;
			}
		}
		else if (orig->m_alpha_buffer)
		{
			if (!CreateAlphaBuffer(false))
			{
				m_oom = true;
				return;
			}

			OP_STATUS res = X11Utils::ScaleImage((char *)orig->m_alpha_buffer,
									  orig->m_width,
									  orig->m_width, orig->m_height,
									  (char *)m_alpha_buffer,
									  m_width,
									  m_width,
									  m_height,
									  8, 8, false, dx, dy, width, height);
			if (OpStatus::IsError(res))
			{
				m_oom = true;
				return;
			}
		}

		if (!PrepareServerData(this))
		{
			m_oom = true;
			return;
		}
	}

	if (!NeedsClientData() && !m_manipulable)
		FreeClientData();
}

X11OpBitmapInternal::X11OpBitmapInternal(XImage *img)
	: m_tile(0),
	  m_true_color_image(img),
	  m_screen_depth_image(img),
	  m_transparency_mask(0),
	  m_alpha_buffer(0),
	  m_colormanager(g_x11_colormanager),
	  m_width(m_true_color_image->width),
	  m_height(m_true_color_image->height),
	  m_pixmap_image(None),
	  m_pixmap_mask(None),
	  m_transparency_mask_bpl(-1),
	  m_refcount(0),
	  m_pending_opacity(-1),
	  m_oom(false),
	  m_manipulable(true),
	  m_image_data_changed(true),
	  m_pixmap_data_changed(false),
	  m_incompatible_format(false),
	  m_reconvert_screen_depth_image(false),
	  m_reconvert_truecolor_image(false),
	  m_owns_pixmap(true)
#ifdef XFT_RENDER_SUPPORT
	  , m_xrender_image(None)
	  , m_xrender_mask(None)
#endif // XFT_RENDER_SUPPORT
{
	m_is_shaped = false;
}

X11OpBitmapInternal::~X11OpBitmapInternal()
{
	OP_ASSERT(m_refcount == 0);
	m_image_data_changed = m_pixmap_data_changed = false;
	FreeServerData();
	FreeClientData();

	OP_DELETE(m_tile);
}

bool X11OpBitmapInternal::NeedsClientData()
{
	return (m_alpha_buffer && !s_disable_alpha
#ifdef XFT_RENDER_SUPPORT
		&& !g_xrender.get()
#endif
		);
}

bool X11OpBitmapInternal::UseServerData()
{
	if (m_true_color_image || !m_pixmap_image || m_image_data_changed)
		return false;

	CopyImageToPixmap();
	bool ok = PrepareServerData(this); // FIXME: OOM

	return ok;
}

bool X11OpBitmapInternal::PrepareBuffer()
{
	if (m_true_color_image && m_screen_depth_image)
		return true;

	if (m_incompatible_format)
	{
		if (!CreateTrueColorImage())
			return false;
		return CreateScreenDepthImage();
	}
	else
	{
		if (m_true_color_image)
			m_screen_depth_image = m_true_color_image;
		else if (m_screen_depth_image)
			m_true_color_image = m_screen_depth_image;
		else
		{
			if (!CreateScreenDepthImage())
				return false;
			m_true_color_image = m_screen_depth_image;
		}
		return true;
	}
}

void X11OpBitmapInternal::SetPixmap(X11Types::Pixmap pixmap)
{
	FreeServerData();
	m_owns_pixmap = false;
	m_pixmap_image = pixmap;
}

X11Types::Pixmap X11OpBitmapInternal::GetPixmap()
{
	if (m_pixmap_image == None)
	{
		X11Types::Display* display = g_x11->GetDisplay();
		X11Types::Window root = RootWindow(display, g_x11->GetScreen());
		m_pixmap_image = XCreatePixmap(display, root, m_width, m_height, g_x11->GetDefaultPixelDepth());
		if (!m_pixmap_image)
		{
			// FIXME: OOM
			return None;
		}
	}

	CopyImageToPixmap();

	return m_pixmap_image;
}

void X11OpBitmapInternal::SetOpacity(UINT8 opacity)
{
#ifdef XFT_RENDER_SUPPORT
	if (g_xrender.get() && m_pixmap_image && !m_true_color_image && !HasAlphaBuffer())
	{
		/* The image data is only on the server. No need to store anything
		   on the client then. */
		m_pending_opacity = opacity;
	}
	else
#endif
	{
		if (!HasAlphaBuffer())
		{
			if (!CreateAlphaBuffer())
				return; // FIXME: OOM
		}

		unsigned char *buf = GetAlphaBufferLine(0);
		memset(buf, opacity, m_width * m_height);
	}
}

void X11OpBitmapInternal::ColorcellsChanged()
{
	m_reconvert_screen_depth_image = true;
	OP_DELETE(m_tile);
	m_tile = 0;
}

bool X11OpBitmapInternal::CreateTransparencyMask(bool init)
{
	OP_ASSERT(!m_transparency_mask);
	OP_ASSERT(m_pending_opacity == -1);

	m_transparency_mask_bpl = (m_width + 7) >> 3;
	int bytes = m_height * m_transparency_mask_bpl;

	m_transparency_mask = (char *)malloc(bytes);
	if (!m_transparency_mask)
		return false;

	if (!init)
		return true;

	memset(m_transparency_mask, 0xff, bytes);

	return true;
}

bool X11OpBitmapInternal::CreateAlphaBuffer(bool init)
{
	int wi = m_width;
	int he = m_height;
	int bytes = he * wi;

	m_alpha_buffer = (unsigned char *)malloc(bytes);
	if (!m_alpha_buffer)
		return false;

	UINT8 opacity = 0xff;
	if (m_pending_opacity != -1)
	{
		init = true;
		opacity = m_pending_opacity;
		m_pending_opacity = -1;
		m_image_data_changed = true;
	}
	if (init)
	{
		if (m_transparency_mask)
		{
			unsigned char *dest = m_alpha_buffer;
			for (int y=0; y<he; y++)
			{
				const char *src = m_transparency_mask + y * m_transparency_mask_bpl;
				for (int x=0; x<wi; x++)
				{
						*dest++ = (*(src+(x>>3)) & (1 << (x & 7))) ? 0xff : 0;
				}
			}
		}
		else
		{

			memset(m_alpha_buffer, opacity, bytes);
		}
	}

	free(m_transparency_mask);
	m_transparency_mask = 0;

	if (m_pixmap_mask != None)
	{
		XFreePixmap(g_x11->GetDisplay(), m_pixmap_mask);
		m_pixmap_mask = None;
	}

	return true;
}

char *X11OpBitmapInternal::GetTransparencyMaskLine(int line)
{
	OP_ASSERT(line >= 0 && line < m_height);
	if (line < 0 || line >= m_height || !m_transparency_mask)
		return 0;

	return m_transparency_mask + line * m_transparency_mask_bpl;
}

unsigned char *X11OpBitmapInternal::GetAlphaBufferLine(int line)
{
	OP_ASSERT(line >= 0 && line < m_height);
	if (line < 0 || line >= m_height || !m_alpha_buffer)
		return 0;

	return m_alpha_buffer + line * m_width;
}

X11OpBitmapInternal *X11OpBitmapInternal::GetTile(int reqwidth, int reqheight)
{
	if (m_tile)
		return m_tile;

	if (!m_true_color_image && !m_screen_depth_image)
		return this;

	if (!CopyTrueColorImageToScreenDepthImage())
	{
		OP_ASSERT(FALSE);
		return this; // Non-critical OOM
	}

	if (m_width * m_height >= TILE_LIMIT)
		return this;

	int width = m_width;
	int height = m_height;
	while (width * height < TILE_LIMIT)
	{
		// FIXME
		width *= 2;
		height *= 2;
	}

	m_tile = OP_NEW(X11OpBitmapInternal, (width, height, false));
	if (!m_tile || m_tile->m_oom || !m_tile->PrepareBuffer() ||
		(m_alpha_buffer && !m_tile->CreateAlphaBuffer(false)) ||
		(m_transparency_mask && !m_tile->CreateTransparencyMask(false)))
	{
		// Non-critical OOM
		OP_ASSERT(FALSE);
		OP_DELETE(m_tile);
		m_tile = 0;
		return this;
	}

	int w = m_width;
	int h = m_height;

	const char *sbuf = (const char *)GetBuffer(true);
	char *dbuf = (char *)m_tile->GetBuffer(true);
	unsigned char *tmask = (unsigned char *)m_tile->m_transparency_mask;
	int dtbpl = m_tile->m_transparency_mask_bpl;
	unsigned char *abuf = m_tile->m_alpha_buffer;
	int dbpl = m_tile->GetBytesPerLine(true);
	int bytes_per_pixel = GetBytesPerPixel(true);
	int sbpl = GetBytesPerLine(true);

	for (int y=0; y<h; y++)
	{
		memcpy(dbuf+y*dbpl, sbuf+y*sbpl, sbpl);
		if (abuf && m_alpha_buffer)
		{
			memcpy(abuf+y*width, m_alpha_buffer+y*m_width, m_width);
		}
		else if (tmask && m_transparency_mask)
		{
			for (int x=0; x<w; x++)
			{
				uint8_t bit = 1 << (x & 7);
				if (*((uint8_t *)m_transparency_mask + y*m_transparency_mask_bpl+(x>>3)) & bit)
				{
					*(tmask+y*dtbpl+(x>>3)) |= bit;
				}
				else
				{
					*(tmask+y*dtbpl+(x>>3)) &= ~bit;
				}
			}
		}
	}

	while (h < height || w < width)
	{
		int new_w = w * 2;
		int new_h = h * 2;
		if (new_w > width)
			new_w = width;
		if (new_h > height)
			new_h = height;
		int px = new_w - w;
		for (int y=0; y<h; y++)
		{
			char *src = dbuf + y*dbpl;
			unsigned char *tsrc = tmask ? tmask + y * dtbpl : 0;
			unsigned char *asrc = abuf ? abuf + y*width : 0;

			// East:
			memcpy(dbuf+y*dbpl+w*bytes_per_pixel, src, px*bytes_per_pixel);
			if (abuf)
			{
				memcpy(abuf+y*width+w, asrc, px);
			}
			else if (tmask)
			{
				for (int x=0; x<px; x++)
				{
					if (*(tsrc+(x>>3)) & (1 << (x & 7)))
						*(tsrc+((w+x)>>3)) |= 1 << ((w+x) & 7);
					else
						*(tsrc+((w+x)>>3)) &= ~(1 << ((w+x) & 7));
				}
			}

			if (y + h < new_h)
			{
				// South:
				memcpy(dbuf+(y+h)*dbpl, src, w*bytes_per_pixel);
				if (abuf)
				{
					memcpy(abuf+(y+h)*width, asrc, w);
				}
				else if (tmask)
				{
					for (int x=0; x<w; x++)
					{
						if (*(tsrc+(x>>3)) & (1 << (x & 7)))
							*(tmask+(y+h)*dtbpl+(x>>3)) |= 1 << (x & 7);
						else
							*(tmask+(y+h)*dtbpl+(x>>3)) &= ~(1 << (x & 7));
					}
				}

				// South-east:
				memcpy(dbuf+(y+h)*dbpl+w*bytes_per_pixel, src,
					   px*bytes_per_pixel);
				if (abuf)
				{
					memcpy(abuf+(y+h)*width+w, asrc, px);
				}
				else if (tmask)
				{
					for (int x=0; x<px; x++)
					{
						if (*(tsrc+(x>>3)) & (1 << (x & 7)))
						{
							*(tmask+(y+h)*dtbpl+((w+x)>>3)) |=
								1 << ((w+x) & 7);
						}
						else
						{
							*(tmask+(y+h)*dtbpl+((w+x)>>3)) &=
								~(1 << ((w+x) & 7));
						}
					}
				}
			}
		}
		w = new_w;
		h = new_h;
	}

	m_tile->m_image_data_changed = true;
	m_tile->m_reconvert_truecolor_image = true;
	return m_tile;
}


void X11OpBitmapInternal::CopyImageToPixmap()
{
	if (!m_image_data_changed)
		return;

	if (!CopyTrueColorImageToScreenDepthImage())
	{
		OP_ASSERT(FALSE);
		return; // FIXME: OOM
	}

	if (!m_pixmap_image)
		return;

	X11Types::Display *display = g_x11->GetDisplay();
	GC gc = XCreateGC(display, m_pixmap_image, 0, 0);
	if (gc != None)
	{
		bool modified = false;
		int size = m_width*m_height;
		UINT32 *src = (UINT32 *)GetBuffer(true);

		if( CanUseXRender(this) )
		{
#if defined(USE_REVERSIBLE_PREMULTIPLY)
			modified = GetColorManager()->Premultiply(true, src, size);
			XPutImage( display, m_pixmap_image, gc, m_screen_depth_image, 0, 0, 0, 0, m_width, m_height);
			if( modified )
				GetColorManager()->Premultiply(false, src, size);
#else

			// I do not prefer to allocate data like this, but premultiplying
			// is not reversible and we may do this multipe times on the same image
			// buffer when splitting a bitmap into fragments (smaller bitmaps). I still
			// reverse it in case of a memory shortage. The OpBitmap::AddLine() makes it
			// hard to know what part of an image is premultiplied or not (but in can be done).

			UINT32* buf = new UINT32[size];
			if( !buf )
			{
				modified = GetColorManager()->Premultiply(true, src, size);
				XPutImage( display, m_pixmap_image, gc, m_screen_depth_image, 0, 0, 0, 0, m_width, m_height);
				if( modified )
					GetColorManager()->Premultiply(false, src, size);
			}
			else
			{
				memcpy(buf, src, size*4);
				GetColorManager()->Premultiply(true, buf, size);
				m_screen_depth_image->data = (char*)buf;
				XPutImage( display, m_pixmap_image, gc, m_screen_depth_image, 0, 0, 0, 0, m_width, m_height);
				m_screen_depth_image->data = (char*)src;
				delete [] buf;
			}
#endif
		}
		else
		{
			XPutImage(display, m_pixmap_image, gc, m_screen_depth_image, 0, 0, 0, 0, m_width, m_height);
		}

		XFreeGC(display, gc);
	}

	m_image_data_changed = false;
}


void X11OpBitmapInternal::CopyPixmapToImage()
{
	if (!m_pixmap_data_changed || !m_true_color_image)
		return;

	m_pixmap_data_changed = false;

	XImage* new_img = XGetImage(g_x11->GetDisplay(), m_pixmap_image, 0, 0, m_width, m_height, AllPlanes, ZPixmap);
	if (!new_img)
	{
		OP_ASSERT(FALSE); // FIXME: OOM
		return;
	}

	if (m_screen_depth_image)
	{
		if (m_true_color_image == m_screen_depth_image)
			m_true_color_image = new_img;
		XDestroyImage(m_screen_depth_image);
	}
	m_screen_depth_image = new_img;


	if (m_pending_opacity != -1)
	{
		UINT8 o = m_pending_opacity;
		m_pending_opacity = -1;
		if (!HasAlphaBuffer())
		{
			if (!CreateAlphaBuffer(false))
				return; // FIXME: OOM
		}

		memset(GetAlphaBufferLine(0), o, m_width*m_height);
	}

	if (!CopyScreenDepthImageToTrueColorImage())
	{
		OP_ASSERT(FALSE); // FIXME: OOM
		return;
	}
}

void X11OpBitmapInternal::Draw(OpPainter *painter, const OpRect &source, int dx, int dy)
{
	if (source.x+source.width > m_width || source.y+source.height > m_height)
	{
		X11OpBitmapInternal *m_tile = GetTile(source.width, source.height);
		if (m_tile && m_tile != this)
		{
			m_tile->Draw(painter, source, dx, dy);
			return;
		}
	}

	X11OpPainterInternal *pi = ((X11OpPainter *)painter)->GetInternal();
	OP_ASSERT(pi);
	if (!pi)
		return;
	X11Types::Display *display = pi->GetDisplay();
	OP_ASSERT(display);
	if (!display)
		return;
	X11Types::Drawable drawable = pi->GetDrawable();
	GC gc = pi->GetGC();

	if (!PrepareServerData(this))
	{
		// FIXME: OOM
		OP_ASSERT(FALSE);
		return;
	}

	bool can_draw_alpha = m_alpha_buffer || m_pending_opacity != -1
#ifdef XFT_RENDER_SUPPORT
		|| m_xrender_mask != None
#endif
		;


	if ((can_draw_alpha && !s_disable_alpha) )
	{
		DrawAlpha(painter, display, drawable, gc, dx, dy, source.width, source.height, source.x, source.y);
	}
	else
	{
		X11OpBitmap *dest_bitmap = pi->GetBitmap();
		X11OpBitmapInternal *dest_internal = dest_bitmap ? dest_bitmap->GetInternal() : 0;

		if (dest_internal)
		{
			// Reset any alpha values in destination bitmap area
			UINT8 *dest_alpha = (UINT8 *)dest_internal->GetAlphaBufferLine(0);
			if (dest_alpha)
			{
				dest_alpha += dx;
				int dest_width = dest_internal->Width();
				for (int y=dy; y<source.height; y++)
				{
					memset(dest_alpha, 0xff, source.width);
					dest_alpha += dest_width;
				}
			}
		}

		OP_ASSERT(m_pixmap_image != None);
		if (m_pixmap_image == None)
			return; // Give up
		if (m_pixmap_mask != None)
			XSetClipMask(display, gc, m_pixmap_mask);

		bool tiled = source.x + source.width > m_width || source.y + source.height > m_height;
		if (tiled)
		{
			int height = source.height;
			int yy = dy;
			int iw = m_width;
			int ih = m_height;
			int y_offs = source.y;
			while (height > 0)
			{
				int diffy = ih - y_offs;
				if (diffy > height)
					diffy = height;
				int width = source.width;
				int xx = dx;
				int x_offs = source.x;
				while (width > 0)
				{
					int diffx = iw - x_offs;
					if (diffx > width)
						diffx = width;
					if (m_pixmap_mask != None)
						XSetClipOrigin(display, gc, xx-x_offs, yy-y_offs);
					XCopyArea(display, m_pixmap_image, drawable, gc, x_offs, y_offs, diffx, diffy, xx, yy);
					width -= diffx;
					xx += diffx;
					x_offs = 0;
				}
				height -= diffy;
				yy += diffy;
				y_offs = 0;
			}
		}
		else
		{
			if (m_pixmap_mask != None)
			{
				XSetClipOrigin(display, gc, dx-source.x, dy-source.y);
			}
			XCopyArea(display, m_pixmap_image, drawable, gc, source.x, source.y, source.width, source.height, dx, dy);
		}
	}
}

void X11OpBitmapInternal::DrawAlpha(OpPainter *painter,
									X11Types::Display *display, X11Types::Drawable drawable, GC gc,
									int x, int y, int width, int height,
									int sx, int sy)
{
	unsigned int tmp_width=0, tmp_height=0;
	unsigned int dr_bpp=0;
	X11Types::Window dummyroot;
	int dummyx, dummyy;
	unsigned int dummybwidth;
	XGetGeometry(display, drawable, &dummyroot, &dummyx, &dummyy, &tmp_width, &tmp_height, &dummybwidth, &dr_bpp);
	int dr_width = tmp_width;
	int dr_height = tmp_height;
	if (y < 0)
	{
		sy -= y;
		height += y;
		y = 0;
	}
	if (x < 0)
	{
		sx -= x;
		width += x;
		x = 0;
	}
	if (y+height > dr_height)
		height = dr_height - y;
	if (x+width > dr_width)
		width = dr_width - x;

	if (width <= 0 || height <= 0)
		return;

	X11OpPainterInternal* pi = ((X11OpPainter *)painter)->GetInternal();
	OP_ASSERT(pi);
	if (!pi)
		return;


#ifdef XFT_RENDER_SUPPORT
	if ((g_xrender.get() && m_xrender_image && m_xrender_mask) )
	{
		X11Types::Picture dest = pi->GetXrenderHandle();
		OP_ASSERT(dest);
		if (dest)
		{
			const int iw = m_width;
			const int ih = m_height;
			int h = height;
			int yy = y;
			int y_offs = sy;
			while (h > 0)
			{
				int diffy = ih - y_offs;
				if (diffy > h)
					diffy = h;
				int w = width;
				int xx = x;
				int x_offs = sx;

				while (w > 0)
				{
					int diffx = iw - x_offs;
					if (diffx > w)
						diffx = w;
					g_xrender->XRenderComposite(display, PictOpOver, m_xrender_image,
												m_xrender_mask,
												dest, x_offs, y_offs,
												x_offs, y_offs, xx, yy,
												diffx, diffy);
					w -= diffx;
					xx += diffx;
					x_offs = 0;
				}
				h -= diffy;
				yy += diffy;
				y_offs = 0;
			}
			return;
		}
	}
#endif // XFT_RENDER_SUPPORT

	if (!PrepareBuffer())
		return;
	CopyPixmapToImage();

	int dest_bpl;
	UINT8 *dest = 0;
	X11OpBitmap *dest_bitmap = pi->GetBitmap();
	X11OpBitmapInternal *dest_internal = dest_bitmap ? dest_bitmap->GetInternal() : 0;
	UINT8 *dest_alpha = 0;
	int dest_width;
	XImage *bgimg = 0;
	int bpp = GetBytesPerPixel(true);

	if (dest_internal)
	{
		dest_alpha = (UINT8 *)dest_internal->GetAlphaBufferLine(0);
		dest_width = dest_internal->Width();
	}
	else
		dest_width = 0; // apparently uninitialised otherwise.

	if (!dest)
	{
		bgimg = XGetImage(display, drawable, x, y, width, height, AllPlanes, ZPixmap);
		OP_ASSERT(bgimg);
		if (!bgimg)
			return;
		dest_bpl = bgimg->bytes_per_line;
		dest = (UINT8 *)bgimg->data - (y*dest_bpl + x*bpp);
	}

	if (!CopyTrueColorImageToScreenDepthImage())
	{
		OP_ASSERT(FALSE); // FIXME: OOM
		return;
	}
	if (!m_screen_depth_image || !m_alpha_buffer)
	{
		OP_ASSERT(FALSE);
		return;
	}

	X11Utils::ColorError *yerrarr = OP_NEWA(X11Utils::ColorError, width);
	if (!yerrarr)
		return; // FIXME: OOM
	memset(yerrarr, 0, sizeof(X11Utils::ColorError) * width);

	for (int yyy=0; yyy<height; yyy++)
	{
		UINT8 *da = 0;
		if (dest_alpha)
			da = dest_alpha + yyy * dest_width;
		int src_y = (yyy+sy) % m_height;
		const unsigned char *alpha_ptr = GetAlphaBufferLine(src_y);
		const unsigned char *alpha_start = alpha_ptr;
		alpha_ptr += sx;
		int sxx = sx;
		X11Utils::ColorError xerr = { 0, 0, 0 };

		for (int xxx=0; xxx<width; xxx++)
		{
			unsigned char alpha = *alpha_ptr++;
			if (alpha == 0xff)
			{
				XPutPixel(bgimg, xxx, yyy, XGetPixel(m_screen_depth_image, sxx, src_y));
				if (da)
					*da = 0xff;
			}
			else if (alpha != 0)
			{
				unsigned int spixel = XGetPixel(m_screen_depth_image, sxx, src_y);
				int sr, sg, sb, sa_ignore;
				g_x11_colormanager->GetRGB(spixel, sr, sg, sb, sa_ignore);

				unsigned int dpixel = XGetPixel(bgimg, xxx, yyy);
				int dr, dg, db, da_ignore;
				g_x11_colormanager->GetRGB(dpixel, dr, dg, db, da_ignore);

				int trans = 255 - alpha;
				if (da)
				{
					trans = *da * trans / 255;
					*da = (((255-alpha) * *da + alpha * 255) + 127) / 255;
				}

				int r = (sr * alpha + dr * trans + 127) / 255;
				int g = (sg * alpha + dg * trans + 127) / 255;
				int b = (sb * alpha + db * trans + 127) / 255;
				int a = 0xFF;

				xerr.Adjust(r, g, b);
				yerrarr[xxx].Adjust(r, g, b);
				const XColor *xcolor = 0;
				unsigned int pixel = g_x11_colormanager->GetPixelValue(r, g, b, a, &xcolor);
				if (xcolor)
				{
					int r_err = (xcolor->red >> 8) - r;
					int g_err = (xcolor->green >> 8) - g;
					int b_err = (xcolor->blue >> 8) - b;
					int xr = r_err / 2;
					int xg = g_err / 2;
					int xb = b_err / 2;
					xerr.AddError(xr, xg, xb);
					yerrarr[xxx].AddError(r_err-xr, g_err-xg, b_err-xb);
				}

				XPutPixel(bgimg, xxx, yyy, pixel);
			}
			sxx ++;
			if (alpha_ptr - alpha_start >= m_width)
			{
				alpha_ptr = alpha_start;
				sxx = 0;
			}
			if (da)
				da++;
		}
	}
	OP_DELETEA(yerrarr);

	if (bgimg)
	{
		XPutImage(display, drawable, gc, bgimg, 0, 0, x, y, width, height);
		XDestroyImage(bgimg);
	}
}


bool X11OpBitmapInternal::CanUseXRender(X11OpBitmapInternal* bi)
{
#ifdef XFT_RENDER_SUPPORT
	if ((bi->m_alpha_buffer || (bi->m_pending_opacity != -1)) && g_xrender.get() && !s_disable_alpha)
	{
		return true;
	}
	else
#endif
	{
		return false;
	}
}


bool X11OpBitmapInternal::PrepareServerData(X11OpBitmapInternal *bi, const OpRect *area)
{
	if( (m_pixmap_image && (m_pixmap_mask || !m_transparency_mask)
#ifdef XFT_RENDER_SUPPORT
		&& (m_xrender_image || (!m_alpha_buffer && m_pending_opacity == -1))
#endif
		) || NeedsClientData())
	{
		return true;
	}

	if (!bi->CopyTrueColorImageToScreenDepthImage())
		return false;

	int sx, sy, sw, sh;
	if (area)
	{
		sx = area->x;
		sy = area->y;
		sw = area->width;
		sh = area->height;
	}
	else
	{
		sx = sy = 0;
		sw = m_width;
		sh = m_height;
	}

	X11Types::Display *disp = g_x11->GetDisplay();
	X11Types::Window drawable = g_x11->GetAppWindow();

	// Create pixmap (server data) if not present and copy image (client data) to it
	if (!m_pixmap_image)
	{
		m_pixmap_image = XCreatePixmap(disp, drawable, sw, sh, g_x11->GetDefaultPixelDepth());
		OP_ASSERT(m_pixmap_image);
		if (!m_pixmap_image)
			return false;

		if (bi->m_screen_depth_image)
		{
			GC gc = XCreateGC(disp, m_pixmap_image, 0, 0);
			if (gc != None)
			{
				bool modified = false;
				int size = bi->m_width * bi->m_height;
				UINT32 *src = (UINT32 *)bi->GetBuffer(true);

				if( CanUseXRender(bi) )
				{
#if defined(USE_REVERSIBLE_PREMULTIPLY)
					modified = GetColorManager()->Premultiply(true, src, size);
					XPutImage( disp, m_pixmap_image, gc, bi->m_screen_depth_image, sx, sy, 0, 0, sw, sh);
					if( modified )
						GetColorManager()->Premultiply(false, src, size);
#else

					// I do not prefer to allocate data like this, but premultiplying
					// is not reversible and we may do this multipe times on the same image
					// buffer when splitting a bitmap into fragments (smaller bitmaps). I still
					// reverse it in case of a memory shortage. The OpBitmap::AddLine() makes it
					// hard to know what part of an image is premultiplied or not (but in can be done).

					UINT32* buf = new UINT32[size];
					if( !buf )
					{
						modified = GetColorManager()->Premultiply(true, src, size);
						XPutImage( disp, m_pixmap_image, gc, bi->m_screen_depth_image, sx, sy, 0, 0, sw, sh);
						if( modified )
							GetColorManager()->Premultiply(false, src, size);
					}
					else
					{
						memcpy(buf, src, size*4);
						GetColorManager()->Premultiply(true, buf, size);
						bi->m_screen_depth_image->data = (char*)buf;
						XPutImage( disp, m_pixmap_image, gc, bi->m_screen_depth_image, sx, sy, 0, 0, sw, sh);
						bi->m_screen_depth_image->data = (char*)src;
						delete [] buf;
					}
#endif
				}
				else
				{
					XPutImage( disp, m_pixmap_image, gc, bi->m_screen_depth_image, sx, sy, 0, 0, sw, sh);
				}


				XFreeGC(disp, gc);
				m_reconvert_truecolor_image = true;
			}
		}
	}

#ifdef XFT_RENDER_SUPPORT
	// Create the two X render Picture objects if needed
	if (CanUseXRender(bi))
	{
		X11Types::Visual *visual = g_x11->GetVisual();

		if (!m_xrender_image)
		{
			XRenderPictFormat *format_img = g_xrender->XRenderFindVisualFormat(disp, visual);
			OP_ASSERT(format_img);
			if (format_img)
			{
				m_xrender_image = g_xrender->XRenderCreatePicture(disp, m_pixmap_image, format_img, 0, 0);
			}
		}

		if (!m_xrender_mask)
		{
			OP_ASSERT(!m_pixmap_mask);
			m_pixmap_mask = XCreatePixmap(disp, drawable, m_width, m_height, 8);

			XRenderPictFormat req_mask;
			req_mask.type = PictTypeDirect;
			req_mask.depth = 8;
			XRenderPictFormat *format_mask = g_xrender->XRenderFindFormat(disp, PictFormatType | PictFormatDepth, &req_mask, 0);
			OP_ASSERT(format_mask);

			if (format_mask)
			{
				m_xrender_mask = g_xrender->XRenderCreatePicture(disp, m_pixmap_mask, format_mask, 0, 0);
			}

			GC gc_mask = XCreateGC(disp, m_pixmap_mask, 0, 0);

			if( bi->GetColorManager()->HasAlphaSupport() )
			{
				// We use data from image buffer if alpha, otherwise opacity. Opacity has not
				// been written (as alpha data) to image buffer. Perhaps we should do that.
				OP_ASSERT(bi->m_alpha_buffer || bi->m_pending_opacity != -1);
				XSetForeground(disp, gc_mask, bi->m_alpha_buffer ? 0xFF : bi->m_pending_opacity );
				XFillRectangle(disp, m_pixmap_mask, gc_mask, 0, 0, m_width, m_height);
			}
			else if( bi->m_alpha_buffer)
			{
   				char* maskptr = (char *)bi->m_alpha_buffer+sy*bi->m_width+sx;
				XImage* alpha_img = XCreateImage(disp, visual, 8, ZPixmap, 0, maskptr, sw, sh, 8, bi->m_width);

				OP_ASSERT(m_pixmap_image != None && m_pixmap_mask != None && gc_mask != None && alpha_img != None);

				if (m_pixmap_image != None && m_pixmap_mask != None && gc_mask != None)
				{
					XPutImage(disp, m_pixmap_mask, gc_mask, alpha_img, 0, 0, 0, 0, m_width, m_height);
				}
				if (alpha_img)
				{
					alpha_img->data = 0; // We own the data, so we don't want Xlib to destroy it.
					XDestroyImage(alpha_img);
				}
			}
			else
			{
				OP_ASSERT(bi->m_pending_opacity != -1);
				XSetForeground(disp, gc_mask, bi->m_pending_opacity);
				XFillRectangle(disp, m_pixmap_mask, gc_mask, 0, 0, m_width, m_height);
			}

			if (gc_mask != None)
			{
				XFreeGC(disp, gc_mask);
			}
		}

		OP_ASSERT(m_xrender_image && m_xrender_mask);
		m_image_data_changed = false;
		return true;
	}
#endif // XFT_RENDER_SUPPORT

	// Create transparency Pixmap if needed
	if (!m_pixmap_mask &&
		(bi->m_transparency_mask || (bi->m_alpha_buffer && disable_alpha)))
	if (!m_pixmap_mask && (bi->m_transparency_mask || (bi->m_alpha_buffer && s_disable_alpha)))
	{
		/* Alpha-blending is disabled. Convert alpha-buffer to transparency
		   mask, with dithering. */

		int bpl = (sw + 7) >> 3;
		int bytes = sh * bpl;
		char *transpmask = (char *)malloc(bytes);
		if (!transpmask)
			return false;

		if (bi->m_alpha_buffer)
		{
			int balance = 0;
			for (int y=0; y<sh; y++)
			{
				const unsigned char *alphaline = bi->GetAlphaBufferLine(y+sy);
				char *transpline = transpmask + y * bpl;
				if (!alphaline)
				{
					free(transpmask);
					return false;
				}
				for (int x=0; x<sw; x++)
				{
					balance += alphaline[sx+x];
					if (balance > 0x80)
					{
						balance -= 0xff;
						transpline[x>>3] |= 1 << (x & 7);
					}
					else
					{
						transpline[x>>3] &= ~(1 << (x & 7));
					}
				}
			}
		}
		else
		{
			for (int y=0; y<sh; y++)
			{
				const char *src = bi->GetTransparencyMaskLine(y+sy);
				char *dst = transpmask + y * bpl;
				if (!src)
				{
					free(transpmask);
					return false;
				}
				for (int x=0; x<sw; x++)
				{
					int xx = x+sx;
					bool transparent = src[xx>>3] & (1 << (xx & 7));
					if (transparent)
						dst[x>>3] |= 1 << (x & 7);
					else
						dst[x>>3] &= ~(1 << (x & 7));
				}
			}
		}
		m_pixmap_mask = XCreateBitmapFromData(disp, drawable, transpmask, sw, sh);
		free(transpmask);
		if (m_pixmap_mask == None)
			return false;
		m_image_data_changed = false;
	}

	return true;
}

void X11OpBitmapInternal::FreeClientData()
{
	CopyImageToPixmap();

	if (m_screen_depth_image && m_screen_depth_image != m_true_color_image)
	{
		XDestroyImage(m_screen_depth_image);
	}
	if (m_true_color_image)
	{
		XDestroyImage(m_true_color_image);
	}

	m_screen_depth_image = 0;
	m_true_color_image = 0;

	free(m_transparency_mask);
	m_transparency_mask = 0;

	free(m_alpha_buffer);
	m_alpha_buffer = 0;
}

void X11OpBitmapInternal::FreeServerData()
{
	CopyPixmapToImage();

	X11Types::Display *display = g_x11->GetDisplay();

	if (m_pixmap_image != None && m_owns_pixmap)
		XFreePixmap(display, m_pixmap_image);

	m_pixmap_image = None;
	m_owns_pixmap = true;
	if (m_pixmap_mask != None)
	{
		XFreePixmap(display, m_pixmap_mask);
		m_pixmap_mask = None;
	}
#ifdef XFT_RENDER_SUPPORT
	if (m_xrender_image != None)
	{
		g_xrender->XRenderFreePicture(display, m_xrender_image);
		m_xrender_image = None;
	}
	if (m_xrender_mask != None)
	{
		g_xrender->XRenderFreePicture(display, m_xrender_mask);
		m_xrender_mask = None;
	}
#endif // XFT_RENDER_SUPPORT
}


bool X11OpBitmapInternal::CopyTrueColorImageToScreenDepthImage()
{
	if (!m_true_color_image)
		return true;

	if (!m_incompatible_format)
	{
		m_screen_depth_image = m_true_color_image;
		return true;
	}

	if (!RequestScreenDepthImage())
		return false;

	if (!m_reconvert_screen_depth_image)
		return true;

	m_reconvert_screen_depth_image = false;

	X11Utils::ColorError *yerrarr = OP_NEWA(X11Utils::ColorError, m_width);
	if (!yerrarr)
		return false;
	memset(yerrarr, 0, sizeof(X11Utils::ColorError) * m_width);
	for (int y=0; y<m_height; y++)
	{
		X11Utils::ColorError xerr = { 0, 0, 0 };
		for (int x=0; x<m_width; x++)
		{
			int a, r, g, b;
			m_colormanager->GetPixel(m_true_color_image, x, y, r, g, b, a);
			xerr.Adjust(r, g, b);
			yerrarr[x].Adjust(r, g, b);
			const XColor *xcolor = 0;
			g_x11_colormanager->GetPixelValue(r, g, b, a, &xcolor);
			if (xcolor)
			{
				int r_err = (xcolor->red >> 8) - r;
				int g_err = (xcolor->green >> 8) - g;
				int b_err = (xcolor->blue >> 8) - b;
				int xr = r_err / 2;
				int xg = g_err / 2;
				int xb = b_err / 2;
				xerr.AddError(xr, xg, xb);
				yerrarr[x].AddError(r_err-xr, g_err-xg, b_err-xb);
			}
			g_x11_colormanager->PutPixel(m_screen_depth_image, x, y, r, g, b, a);
		}
	}
	OP_DELETEA(yerrarr);

	return true;
}


bool X11OpBitmapInternal::CopyScreenDepthImageToTrueColorImage()
{
	if (!m_screen_depth_image)
		return true;

	if (!m_incompatible_format)
	{
		m_true_color_image = m_screen_depth_image;
		return true;
	}

	if (!RequestTrueColorImage())
		return false;

	if (!m_reconvert_truecolor_image)
		return true;

	m_reconvert_truecolor_image = false;

	int a, r, g, b;
	for (int y=0; y<m_height; y++)
	{
		for (int x=0; x<m_width; x++)
		{
			g_x11_colormanager->GetPixel(m_screen_depth_image, x, y, r, g, b, a);
			m_colormanager->PutPixel(m_true_color_image, x, y, r, g, b, a);
		}
	}

	return true;
}

bool X11OpBitmapInternal::RequestScreenDepthImage()
{
	if (m_screen_depth_image || !m_true_color_image)
		return true;

	if (!m_incompatible_format)
	{
		m_screen_depth_image = m_true_color_image;
		return true;
	}

	return CreateScreenDepthImage();
}

bool X11OpBitmapInternal::RequestTrueColorImage()
{
	if (m_true_color_image || !m_screen_depth_image)
		return true;

	if (!m_incompatible_format)
	{
		m_true_color_image = m_screen_depth_image;
		return true;
	}

	return CreateTrueColorImage();
}

bool X11OpBitmapInternal::CreateScreenDepthImage()
{
	int bytes_per_line = m_width * (g_x11->GetImageBpp() >> 3);
	bytes_per_line = ((bytes_per_line + 3) / 4) * 4; // Word-align
	char *data = (char *)malloc(bytes_per_line * m_height);
	if (!data)
		return false;

	m_screen_depth_image =
		XCreateImage(g_x11->GetDisplay(), g_x11->GetVisual(),
					 g_x11->GetDefaultPixelDepth(), ZPixmap, 0, data,
					 m_width, m_height, 8, bytes_per_line);

	if (!m_screen_depth_image)
	{
		free(data);
		return false;
	}

	return true;
}

bool X11OpBitmapInternal::CreateTrueColorImage()
{
	int bytes_per_line = m_width * 2;
	bytes_per_line = ((bytes_per_line + 3) / 4) * 4; // Word-align
	char *data = (char *)malloc(bytes_per_line * m_height);
	if (!data)
		return false;
	m_true_color_image = XCreateImage(
		g_x11->GetDisplay(), g_x11->GetVisual(), 16, ZPixmap,
		0, data, m_width, m_height, 8, bytes_per_line);

	if (!m_true_color_image)
	{
		free(data);
		return false;
	}

	return true;
}




void X11OpBitmap::SetIsShaped(bool shaped)
{
	m_is_shaped = shaped;
	d->SetIsShaped(m_is_shaped);
}


/*
 * X11OpBitmap implementation:
 */

X11OpBitmap::X11OpBitmap(bool transparent, bool alpha, UINT32 transparent_index)
	: d(0),
	  m_painter(0),
	  m_cached_effect_image(0),
	  m_pointer_buffer(0),
	  m_pointer_access_type(ACCESS_READONLY),
#ifdef SUPPORT_INDEXED_OPBITMAP
	  m_palette(0),
	  m_palette_size(0),
	  m_transparent_index(transparent_index),
#endif
	  //cached_effect(0),
	  //cached_effect_value(0),
	  m_transparent(transparent),
	  m_alpha(alpha),
	  m_invalidate_cache(false),
	  m_is_shaped(false)
{
}

OP_STATUS X11OpBitmap::Construct(int width, int height)
{
	d = OP_NEW(X11OpBitmapInternal, (width, height));
	if (!d)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = d->InitStatus();
	if (OpStatus::IsError(result))
	{
		OP_DELETE(d);
		d = 0;
		return result;
	}

	d->IncRef();
	g_x11_imagemanager->AddImage(this);
	return result;
}

OP_STATUS X11OpBitmap::Construct(X11OpBitmapInternal *bi)
{
	d = bi;
	d->IncRef();
	return OpStatus::OK;
}

BOOL X11OpBitmap::HasSharedMemory() const { return d && d->HasSharedMemory(); }
UINT32 X11OpBitmap::Width() const { return d->Width(); }
UINT32 X11OpBitmap::Height() const { return d->Height(); }
void X11OpBitmap::PixmapChanged() { d->ServerDataChanged(); }

BOOL X11OpBitmap::HasAlpha() const { return m_alpha; }
BOOL X11OpBitmap::IsTransparent() const { return m_transparent; }
INT32 X11OpBitmap::IsIndexed() const { return 0; }
void X11OpBitmap::ForceAlpha() { m_alpha = true; }
// static
void X11OpBitmap::DisableAlphaBlending(bool disable) { s_disable_alpha = disable; }
UINT32 X11OpBitmap::GetBytesPerLine() const { return Width() * 4; }
UINT8 X11OpBitmap::GetBpp() const
{
	/* Core always gives us 32bpp image data, and we always give 32bpp image
	 * data to core. Whatever kind of pixel depth we use internally is
	 * irrelevant. */
	return 32;
}

OP_STATUS X11OpBitmap::Construct(int width, int height, const OpRect *area, X11OpBitmapInternal *bi)
{
	int rw, rh;
	if (area)
	{
		rw = area->width;
		rh = area->height;
	}
	else
	{
		rw = width;
		rh = height;
	}
	/* We keep smaller images as manipulable. This is in case we want to
	   pre-tile the image. Should come up with a better solution than this. */

	bool manipulable = rw * rh < TILE_LIMIT;
	d = OP_NEW(X11OpBitmapInternal, (width, height, area, manipulable, bi));
	if (!d)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = d->InitStatus();
	if (OpStatus::IsError(result))
	{
		OP_DELETE(d);
		d = 0;
		return result;
	}

	d->IncRef();
	return result;
}

X11OpBitmap::~X11OpBitmap()
{
	OP_ASSERT(!m_painter);
	if (InList())
		Out();
	g_imagecache->Remove(this);
	if (d && d->DecRef() == 0)
		OP_DELETE(d);
	if (m_cached_effect_image && m_cached_effect_image->DecRef() == 0)
		OP_DELETE(m_cached_effect_image);
#ifdef SUPPORT_INDEXED_OPBITMAP
	OP_DELETEA(m_palette);
#endif
	OP_DELETEA(m_pointer_buffer);
}



BOOL X11OpBitmap::Supports(SUPPORTS supports) const
{
	switch (supports)
	{
	case SUPPORTS_POINTER:
	case SUPPORTS_PAINTER:
	case SUPPORTS_CREATETILE: // Note: We're kind of lying here...
#ifdef SUPPORT_INDEXED_OPBITMAP
	case SUPPORTS_INDEXED:
#endif
	case SUPPORTS_OPACITY:
		return TRUE;
	default:
		return FALSE;
	};
}

OpBitmap *X11OpBitmap::CreateTile(UINT32 width, UINT32 height)
{
	/* Core has no way of tiling an OpBitmap as efficiently as we do.
	 * Returning the same OpBitmap - this does not comply with the
	 * documentation of OpBitmap::CreateTile() (I think...) */

	X11OpBitmap *local = OP_NEW(X11OpBitmap, ());
	if (local && OpStatus::IsError(local->Construct(d)))
	{
		OP_DELETE(local);
		local = 0;
	}

	return local;
}


void X11OpBitmap::CopyToPointer()
{
	if (!m_pointer_buffer)
		return;

	d->CopyPixmapToImage();

	int width = d->Width();
	int height = d->Height();

	UINT32 *dest = m_pointer_buffer;

	const UINT8 *src = (const UINT8 *)d->GetBuffer(false);
	int srcbpl = d->GetBytesPerLine(false);

	for (int line=0; line<height; line++)
	{
		d->GetColorManager()->GetLine(d->GetImage(), src, dest, line, width, false);
		SetAlphaToImageData(line, dest);

		dest += width;
		src += srcbpl;
	}
}

void X11OpBitmap::CopyFromPointer()
{
	if (!m_pointer_buffer)
		return;

	d->CopyPixmapToImage();

	int width = d->Width();
	int height = d->Height();

	if (m_alpha && !d->GetAlphaBufferLine(0))
		d->CreateAlphaBuffer(false); // FIXME: OOM
	else if (m_transparent && !d->GetTransparencyMaskLine(0))
		d->CreateTransparencyMask(false); // FIXME: OOM

	const UINT32 *src = m_pointer_buffer;
	UINT8 *dest = (UINT8 *)d->GetBuffer(false);
	int destbpl = d->GetBytesPerLine(false);

	for (int line=0; line<height; line++)
	{
		d->GetColorManager()->PutLine(d->GetImage(), dest, src, line, width, false);
		SetAlphaFromImageData(line, src);

		src += width;
		dest += destbpl;
	}

	d->ClientDataChanged();
	m_invalidate_cache = true;
}


#ifdef OPBITMAP_POINTER_ACCESS
void *X11OpBitmap::GetPointer(AccessType access_type)
{
	// TODO: can we gain any advantage by supporting read-only or write-only access ?
	OP_ASSERT(!m_painter);

	if (!d->PrepareBuffer())
		return 0;

	if (m_pointer_buffer)
	{
		// pointer is already grabbed, return same pointer alltough I think we should return 0
		OP_ASSERT(!d->NeedsSync());
		return m_pointer_buffer;
	}

	m_pointer_buffer = OP_NEWA(UINT32, d->Width() * d->Height());
	if (!m_pointer_buffer)
	{
		return 0;
	}

	m_pointer_access_type = access_type;

	CopyToPointer();
	return m_pointer_buffer;
}

void X11OpBitmap::ReleasePointer(BOOL changed /* = TRUE */)
{
	OP_ASSERT(m_pointer_buffer);
	if (!m_pointer_buffer)
	{
		// Pointer is not grabbed, ignore
		return;
	}

	if (changed && (m_pointer_access_type == ACCESS_READWRITE || m_pointer_access_type == ACCESS_WRITEONLY) )
	{
		CopyFromPointer();
	}

	OP_DELETEA(m_pointer_buffer);
	m_pointer_buffer = 0;
}
#else
void *X11OpBitmap::GetPointer() const
{
	OP_ASSERT(!m_painter);

	// ugly type-cast needed
	X11OpBitmap *self = (X11OpBitmap *)this;
	if (!self->d->PrepareBuffer())
		return 0;

	if (self->m_pointer_buffer)
	{
		// pointer is already grabbed, return same pointer alltough I think we should return 0
		OP_ASSERT(!d->NeedsSync());
		return self->m_pointer_buffer;
	}

	self->m_pointer_buffer = OP_NEWA(UINT32, d->Width() * d->Height());
	if (!self->m_pointer_buffer)
		return 0;

	self->CopyToPointer();

	return self->m_pointer_buffer;
}

void X11OpBitmap::ReleasePointer() const
{
	if (!m_pointer_buffer)
	{
		// Pointer is not grabbed, ignore
		return;
	}

	// ugly type-cast needed
	X11OpBitmap *self = (X11OpBitmap *)this;

	self->CopyFromPointer();

	OP_DELETEA(self->m_pointer_buffer);
	self->m_pointer_buffer = 0;
}
#endif // OPBITMAP_POINTER_ACCESS

OpPainter *X11OpBitmap::GetPainter()
{
	OP_ASSERT(!m_pointer_buffer);
	OP_ASSERT(!m_painter); // fails during selftests
	if (m_painter)
	{
		fputs("X11OpBitmap::GetPainter(): Painter exists\n", stderr);
		return m_painter;
	}


	m_painter = OP_NEW(X11OpPainter, ());

	if (!m_painter ||
		OpStatus::IsError(m_painter->Init()) ||
		OpStatus::IsError(m_painter->Begin(this)))
	{
		// FIXME: OOM
		OP_DELETE(m_painter);
		m_painter = 0;
	}

	return m_painter;
}

void X11OpBitmap::ReleasePainter()
{
	OP_ASSERT(m_painter);

	if (m_painter)
	{
		OP_DELETE(m_painter);
		m_painter = 0;
		d->ServerDataChanged();
	}
}

OP_STATUS X11OpBitmap::AddLine(void* data, INT32 line)
{
	OP_ASSERT(!m_pointer_buffer);
	if (line < 0 || line >= d->Height())
	{
		OP_ASSERT(!"Image line number out of range!");
		return OpStatus::ERR;
	}

	if (!d->PrepareBuffer())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	d->CopyPixmapToImage();


	// Detect transparency (incl. alpha)
	if ((m_transparent && !d->HasTransparencyMask() && !d->HasAlphaBuffer()) || (m_alpha && !d->HasAlphaBuffer()))
	{
		UINT32 *src = (UINT32 *)data;
		for (int i=d->Width(); i>0; i--)
		{
			// Only create buffer if we really have to
			if ((*src & 0xff000000) != 0xff000000)
			{
				int alpha = *src >> 24;
				if (alpha == 0 && !d->HasPendingOpacity())
				{
					if (!d->HasTransparencyMask() && !d->HasAlphaBuffer())
					{
						if (!d->CreateTransparencyMask())
						{
							return OpStatus::ERR_NO_MEMORY;
						}
					}
				}
				else
				{
					if (!d->CreateAlphaBuffer())
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					break;
				}
			}
			src ++;
		}
	}


	UINT8 *dest = (UINT8 *)d->GetBuffer(false);
	dest += line * d->GetBytesPerLine(false);

	d->GetColorManager()->PutLine( d->GetImage(), dest, (const UINT32 *)data, line, d->Width(), false);
	SetAlphaFromImageData(line, (UINT32 *)data);


	d->ClientDataChanged();
	m_invalidate_cache = true;

	return OpStatus::OK;
}


void X11OpBitmap::SetAlphaFromImageData(unsigned int line, const UINT32* image_data)
{
	// We must save data to the alpha buffer even if the colormanager supports alpha
	// because that buffer is used when setting up the m_pixmap_mask in
	// X11OpBitmapInternal::PrepareServerData() (we could have extraced data from the
	// image buffer, but that would only work if the colormanager supports alpha).

	unsigned char* alpha_buf = d->GetAlphaBufferLine(line);
	char* transp_mask = d->GetTransparencyMaskLine(line);

	if (alpha_buf || transp_mask)
	{
		int w = d->Width();
		for (int x=0; x<w; x++)
		{
			UINT32 pixel = *image_data++;
			if (alpha_buf)
			{
				*alpha_buf++ = pixel >> 24;
			}
			else if (transp_mask)
			{
				if ((pixel & 0xff000000) != 0xff000000)
					*(transp_mask+(x>>3)) &= ~(1 << (x & 7));
				else
					*(transp_mask+(x>>3)) |= 1 << (x & 7);
			}
		}
	}
}


void X11OpBitmap::SetAlphaToImageData(unsigned int line, UINT32* image_data) const
{
	// Optimize a bit. We do not have to take alpha buffer into account if the
	// colormanager supports alpha by itself. The image data already contains
	// that info in such cases.

	const unsigned char* alpha_buf = d->GetColorManager()->HasAlphaSupport() ? 0 : d->GetAlphaBufferLine(line);
	const char* transp_mask = d->GetTransparencyMaskLine(line);

	if (alpha_buf || transp_mask)
	{
		int w = d->Width();
		for (int x=0; x<w; x++)
		{
			int alpha;
			if (alpha_buf)
				alpha = *alpha_buf++;
			else if (transp_mask)
				alpha = (*(transp_mask+(x>>3)) & (1 << (x & 7))) ? 0xff : 0;
			else
				alpha = 0xff;
			*image_data = (*image_data & 0xffffff) | (alpha << 24);
			image_data++;
		}
	}
}



#ifdef SUPPORT_INDEXED_OPBITMAP
OP_STATUS X11OpBitmap::AddIndexedLine(void* data,  INT32 line)
{
	OP_ASSERT(!m_pointer_buffer);
	if (!m_palette)
		return OpStatus::ERR;

	UINT32 *tcdata = OP_NEWA(UINT32, d->Width());
	if (!tcdata)
		return OpStatus::ERR_NO_MEMORY;

	for (int i=0; i<d->Width(); i++)
	{
		UINT8 index = *((UINT8 *)data+i);
		OP_ASSERT(index < m_palette_size);
		if (index < m_palette_size)
		{
#ifdef NEEDS_RISC_ALIGNMENT
			UINT8* color = m_palette+index*3;
			tcdata[i] = 0xff000000 | (color[0]<<16) | (color[1]<<8) | color[2];
#else // !NEEDS_RISC_ALIGNMENT
			UINT32 temp = *(UINT32 *)(m_palette + index * 3);
			tcdata[i] = 0xff000000 |
#ifdef OPERA_BIG_ENDIAN
			temp >> 8;
#else
			(temp << 16) | (temp & 0xff00) | ((temp >> 16) & 0xff);
#endif // OPERA_BIG_ENDIAN
#endif // !NEEDS_RISC_ALIGNMENT
		}
		else
		{
			tcdata[i] = 0xff000000;
		}

		if (index == m_transparent_index && m_transparent)
		{
			tcdata[i] &= 0x00ffffff;
		}
	}

	OP_STATUS res = AddLine(tcdata, line);

	OP_DELETEA(tcdata);

	return res;
}

BOOL X11OpBitmap::SetPalette(UINT8* pal,  UINT32 num_colors)
{
	/* Since we don't keep the indexed image data, we will fail here if
	   AddIndexedLine() has already been called and the palette is changed */
	OP_ASSERT(!m_palette);

	if (num_colors == 0)
	{
		OP_DELETEA(m_palette);
		m_palette = 0;
		m_palette_size = 0;
		return TRUE;
	}
	int arrsize = num_colors * 3;
	UINT8 *new_palette = OP_NEWA(UINT8, arrsize + (sizeof(UINT32) - arrsize % sizeof(UINT32)));
	if (!new_palette)
		return FALSE;
	OP_DELETEA(m_palette);
	m_palette = new_palette;
	memcpy(m_palette, pal, arrsize);
	m_palette_size = num_colors;
	return TRUE;
}
#endif // SUPPORT_INDEXED_OPBITMAP



BOOL X11OpBitmap::SetColor(UINT8* color, BOOL all_transparent, const OpRect* rect)
{
	if( !all_transparent && !color )
		return TRUE;

	BOOL init_line = rect && rect->width < d->Width();
	OpRect r = rect ? *rect : OpRect(0,0,d->Width(), d->Height());
	if( r.width <= 0 || r.height <= 0 )
		return FALSE;

	UINT32* line = OP_NEWA(UINT32, d->Width());
	if( !line )
		return FALSE;

	// Read the line first if we shall not set all pixels on that line.
	// We do it like this because AddLine() can only add a full line
	// printf("X11OpBitmap::SetColor %d %d %d %d, all_transparent=%d, init_line=%d\n", r.x, r.y, r.width, r.height, all_transparent, init_line );

	for(INT32 i=r.y; i<r.height; i++ )
	{
		if( init_line )
		{
			GetLineData(line, i);
		}

		if( all_transparent )
		{
			for( INT32 j = r.x; j<r.x+r.width; j++ )
			{
				line[j] = 0;
			}
		}
		else
		{
			int pixel = color[0] | (color[1] << 8) | (color[2] << 16) | (color[3] << 24);
			for( INT32 j = r.x; j<r.x+r.width; j++ )
			{
				line[j] = pixel;
			}
		}
		AddLine(line, i);
	}

	OP_DELETEA(line);

	return TRUE;
}

BOOL X11OpBitmap::GetLineData(void* data, UINT32 line) const
{
	OP_ASSERT(!m_pointer_buffer);
	if (line >= (UINT32)d->Height())
	{
		OP_ASSERT(!"Image line number out of range!");
		return FALSE;
	}

	d->PrepareBuffer();
	d->CopyPixmapToImage();
	const UINT8 *src = (const UINT8 *)d->GetBuffer(false);
	src += line * d->GetBytesPerLine(false);

	d->GetColorManager()->GetLine(d->GetImage(), src, (UINT32 *)data, line, d->Width(), false);
	SetAlphaToImageData(line, (UINT32 *)data);

	return TRUE;
}

BOOL X11OpBitmap::GetIndexedLineData(void* data, UINT32 line) const
{
	OP_ASSERT(!"X11OpBitmap::GetIndexedLineData() not implemented!");
	return FALSE;
}

BOOL X11OpBitmap::GetPalette(UINT8* pal) const
{
	OP_ASSERT(!"X11OpBitmap::GetPalette() not implemented!");
	return FALSE;
}

BOOL X11OpBitmap::SetOpacity(UINT8 opacity)
{
	OP_ASSERT(!m_pointer_buffer);
	ForceAlpha();
	d->SetOpacity(opacity);
	return TRUE;
}


/*
 * Implementation specific methods in X11OpBitmap:
 */

void X11OpBitmap::Draw(OpPainter *painter, const OpRect &source, int dx, int dy, int use_w, int use_h)
{
	OP_ASSERT(!m_pointer_buffer);
	if (m_pointer_buffer)
	{
		/* Someone "forgot" to release the pointer. Need to sync the internal
		   data then. This is expensive. */

		CopyFromPointer();
	}

	if (m_invalidate_cache)
	{
		g_imagecache->Remove(this);
		m_invalidate_cache = false;
	}

	int imgw = d->Width();
	int imgh = d->Height();
	if (use_w == imgw && use_h == imgh && (imgw*imgh < TILE_LIMIT || d->NeedsClientData() || d->UseServerData()))
	{
		d->Draw(painter, source, dx, dy);
	}
	else
	{
		bool tile_h = source.x + source.width > use_w;
		bool tile_v = source.y + source.height > use_h;
		if (tile_h || tile_v)
		{
			// Tiled image

			int sx = source.x;
			int sy = source.y;
			int wi = MIN(use_w, sx+source.width);
			int he = MIN(use_h, sy+source.height);
			if (source.width < use_w && source.height < use_h)
			{
				/* Less than one tile is needed in total. Let's not waste time
				   on scaling pixels that we will never display. */

				OpRect tl(sx, sy, wi-sx, he-sy);
				PrepareAndDrawFragments(painter, tl, dx, dy, use_w, use_h);
				OpRect tr(0, sy, source.width-tl.width, he-sy);
				if (tile_h)
				{
					PrepareAndDrawFragments(painter, tr, dx+tl.width, dy, use_w, use_h);
				}
				if (tile_v)
				{
					OpRect bl(sx, 0, wi-sx, source.height-tl.height);
					PrepareAndDrawFragments(painter, bl, dx, dy+tl.height, use_w, use_h);
					if (tile_h)
					{
						OpRect br(0, 0, source.width-tl.width, source.height-tl.height);
						PrepareAndDrawFragments(painter, br, dx+tl.width, dy+tl.height, use_w, use_h);
					}
				}
			}
			else
			{
				bool created_now = false;
				X11OpBitmap *scaled = (X11OpBitmap *)g_imagecache->Find(this, use_w, use_h, 0, 0, use_w, use_h);

				if (!scaled)
				{
					scaled = OP_NEW(X11OpBitmap, ());
					if (!scaled || OpStatus::IsError(scaled->Construct(use_w, use_h, 0, d)))
					{
						// FIXME: OOM
						OP_DELETE(scaled);
						return;
					}
					created_now = true;
				}

				scaled->d->Draw(painter, source, dx, dy);

				if (created_now)
				{
					g_imagecache->Add(this, scaled, use_w, use_h, 0, 0);
					// The image cache took over the ownership of 'scaled' here
				}
			}
		}
		else
		{
			PrepareAndDrawFragments(painter, source, dx, dy, use_w, use_h);
		}
	}
}

void X11OpBitmap::PrepareAndDrawFragments(OpPainter* painter, const OpRect& source, int dx, int dy, int use_w, int use_h)
{
	const int fragsize = 128;
	int sx = source.x;
	int sy = source.y;
	int sw = source.width;
	int sh = source.height;

	{
		// Align position and size to mutipla of fragsize
		int xdiff = sx % fragsize;
		int ydiff = sy % fragsize;
		sx -= xdiff;
		sw += xdiff;
		sy -= ydiff;
		sh += ydiff;

		int wr = sw % fragsize;
		int hr = sh % fragsize;
		if (wr)
			sw += fragsize - wr;
		if (hr)
			sh += fragsize - hr;

		// Limit width and height to what we want to paint
		if (sx+sw > use_w)
			sw = use_w - sx;
		if (sy+sh > use_h)
			sh = use_h - sy;
	}


	int adjusty = source.y - sy;
	int dyy = dy;
	int dh_remain = source.height;
	for (int y=0; y<sh; y+=fragsize)
	{
		int dh_this = MIN(dh_remain, fragsize-adjusty);
		int adjustx = source.x - sx;
		int dxx = dx;
		int dw_remain = source.width;
		int h = MIN(fragsize, use_h-(y+sy));
		for (int x=0; x<sw; x+=fragsize)
		{
			int dw_this = MIN(dw_remain, fragsize-adjustx);
			int w = MIN(fragsize, use_w-(x+sx));
			OpRect req(sx+x, sy+y, w, h);
			bool created_now = false;
			X11OpBitmap *fragment = (X11OpBitmap *)g_imagecache->Find(this, use_w, use_h, req.x, req.y, req.width, req.height);
			if (!fragment)
			{
				fragment = OP_NEW(X11OpBitmap, ());
				if (!fragment || OpStatus::IsError(fragment->Construct(use_w, use_h, &req, d)))
				{
					// FIXME: OOM
					OP_DELETE(fragment);
					return;
				}
				created_now = true;
			}

			OpRect s(adjustx, adjusty, dw_this, dh_this);

			fragment->d->Draw(painter, s, dxx, dyy);

			if (created_now)
			{
				g_imagecache->Add(this, fragment, use_w, use_h, req.x, req.y);
				// The image cache took over the ownership of 'fragment' here.
			}

			dxx += dw_this;
			dw_remain -= dw_this;
			adjustx = 0;
		}
		dyy += dh_this;
		dh_remain -= dh_this;
		adjusty = 0;
	}
}

void X11OpBitmap::ColorcellsChanged()
{
	if (m_cached_effect_image && m_cached_effect_image->DecRef() == 0)
		OP_DELETE(m_cached_effect_image);
	m_cached_effect_image = 0;
	d->ColorcellsChanged();
}

int X11OpBitmap::GetBitmapAllocationSize(int width, int height,
										 bool transparent, bool alpha,
										 int indexed)
{
	int bpp = g_x11->GetImageBpp();
	int pixels = width * height;

	int size = (pixels * bpp + 7) >> 3;
	/* When not using shared pixmaps, images need an XImage and a X11Types::Pixmap.
	   When using shared pixmaps, they just need the X11Types::Pixmap. */
	size *= 2;
	size += sizeof(XImage) + sizeof(X11OpBitmap) + sizeof(X11OpBitmapInternal);

	if (alpha)
	{
		/* Alpha-transparent images need to represent the alpha buffer as an
		   8 bpp X11Types::Pixmap. */
		size += pixels;

		/* TODO: Find out if extra memory is allocated for the two X11Types::Picture
		   handles used in alpha blending. */
	}
	else if (transparent)
	{
		/* Transparent images use a transparency mask array, and a 1 bit X11Types::Pixmap
		   to represent it on the server side. */
		size += ((pixels + 7) >> 3) * 2;
	}

	return size;
}

#endif // !VEGA_OPPAINTER_SUPPORT
