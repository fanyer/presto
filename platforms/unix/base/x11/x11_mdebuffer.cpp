/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#ifdef VEGA_OPPAINTER_SUPPORT

#include "x11_mdebuffer.h"
#include "x11_globals.h"
#include "platforms/unix/base/x11/x11_vegawindow.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/utilix/x11_all.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

X11MdeBuffer::X11MdeBuffer(X11Types::Display *display,
						   int screen,
						   X11Types::Window window,
						   X11Types::VisualPtr visual,
						   UINT32 width,
						   UINT32 height,
						   UINT32 depth,
						   UINT32 bits_per_pixel) 
	:m_image(NULL)
	,m_external_backbuffer(NULL)
	,m_shape_buffer(0)
	,m_width(width)
	,m_height(height)
	,m_depth(depth)
	,m_bits_per_pixel(bits_per_pixel)
	,m_bytes_per_line(0)
	,m_buffer_size(0)
	,m_use_shaping(FALSE)
	,m_display(display)
	,m_screen(screen)
	,m_window(window)
	,m_widget(g_x11->GetWidgetList()->FindWidget(window))
	,m_visual(visual)
	,m_gc(None)
	,m_plugin_gc(None)
{
}


X11MdeBuffer::~X11MdeBuffer()
{
	OP_DELETEA(m_shape_buffer);

	if (m_gc != None)
		XFreeGC(m_display, m_gc);
	if (m_image)
	{
		free(m_image->data);
		m_image->data = 0;
		XDestroyImage(m_image);
	}

	if (m_plugin_gc)
		XFreeGC(m_display, m_plugin_gc);
}

BOOL X11MdeBuffer::HasBGRA8888Format() const
{
	if (m_depth != 32 && m_depth != 24)
		return FALSE;

	static BOOL is_bgra8888 = FALSE;
	static bool is_valid = false;
	if (is_valid)
		return is_bgra8888;

	unsigned int mask[4] = {0,0,0,0};
	XImage* image = XCreateImage(m_display, m_visual, m_depth, ZPixmap, 0, 0, 1, 1, 8, 4);
	if (!image)
		return FALSE; // Arbitrary choice

	mask[0] = image->red_mask;
	mask[1] = image->green_mask;
	mask[2] = image->blue_mask;
	mask[3] = image->byte_order;
	XDestroyImage(image);

	is_valid  = true;

	if (mask[3] == MSBFirst)
	{
		if (mask[0] == 0xff00 &&
			mask[1] == 0xff0000 &&
			mask[2] == 0xff000000)
		{
			is_bgra8888 = TRUE;
		}
	}
	else
	{
		if (mask[0] == 0xff0000 &&
			mask[1] == 0xff00 &&
			mask[2] == 0xff)
		{
			is_bgra8888 = TRUE;
		}
	}

	return is_bgra8888;
}

BOOL X11MdeBuffer::HasBGR565Format() const
{
	if (m_depth != 16)
		return FALSE;
	static BOOL is_bgra565 = FALSE;
	static bool is_valid = false;
	if (is_valid)
		return is_bgra565;
	unsigned int mask[4] = {0,0,0,0};
	XImage* image = XCreateImage(m_display, m_visual, m_depth, ZPixmap, 0, 0, 1, 1, 8, 4);
	if (!image)
		return FALSE; // Arbitrary choice

	mask[0] = image->red_mask;
	mask[1] = image->green_mask;
	mask[2] = image->blue_mask;
	mask[3] = image->byte_order;
	XDestroyImage(image);

	is_valid = true;

	if (mask[3] == MSBFirst)
	{
		if (mask[0] == 0x1f &&
			mask[1] == 0x7e0 &&
			mask[2] == 0xf800)
		{
			is_bgra565 = TRUE;
		}
	}
	else
	{
		if (mask[0] == 0xf800 &&
			mask[1] == 0x7e0 &&
			mask[2] == 0x1f)
		{
			is_bgra565 = TRUE;
		}
	}

	return is_bgra565;
}


void X11MdeBuffer::Erase(const MDE_RECT &rect)
{
	/* Only do anything when the buffer has an alpha channel.
	 * Otherwise it is impossible to erase to transparent anyway.
	 */
	if (m_depth == 32 && m_external_backbuffer)
	{
		m_external_backbuffer->Erase(rect);
	}
	else if (m_depth == 32 && m_image && m_image->data)
	{
		if (rect.x == 0 && rect.y == 0 && (UINT32)rect.w == m_width && (UINT32)rect.h == m_height)
		{
			int bytes_needed = m_bytes_per_line * m_height;
			op_memset(m_image->data, 0, bytes_needed);
		}
		else
		{
			int x = MAX(rect.x,0);
			int y = MAX(rect.y,0);
			int w = MIN((UINT32)rect.w,m_width);
			int h = MIN((UINT32)rect.h,m_height);

			int bytes_needed = w*4;			
			char* data = (char*)m_image->data + x*4 + m_bytes_per_line*y;
			for (INT32 i=0; i<h; i++)
			{
				op_memset(data, 0, bytes_needed);
				data += m_bytes_per_line;
			}
		}

	}
}


void X11MdeBuffer::Repaint(const OpRect& rect, int x, int y)
{
	if (m_external_backbuffer)
	{
		/* If this object does not have access to the back buffer
		 * data, it can not refresh the display buffer from the back
		 * buffer.
		 */
		OP_ASSERT(!"X11MdeBuffer::Repaint() is not currently supported for OpenGL rendering");
		return;
	}

	OP_ASSERT(m_image || !"Why try to repaint an empty buffer?");
	if (!m_image)
		return;

	if (m_gc == None)
	{
		m_gc = XCreateGC(m_display, m_window, 0, 0);
		if (m_gc == None)
		{
			fprintf(stderr, "opera: Failed to create GC\n");
			return;
		}
	}

	if (m_depth == 32 && g_x11->IsXRenderAvailable(m_screen))
	{
		XRenderPictFormat *format_img = XRenderFindVisualFormat(m_display, m_visual);
		if (format_img)
		{
			X11Types::Pixmap pix = XCreatePixmap(m_display, m_window, m_width, m_height, m_depth);
			if (pix != None)
			{

				XPutImage(m_display, pix, m_gc, m_image, 0, 0, 0, 0, m_width, m_height);

				X11Types::Picture src = XRenderCreatePicture(m_display, pix, format_img, 0, 0);
				X11Types::Picture dst = XRenderCreatePicture(m_display, m_window, format_img, 0, 0);

				if (x < 0)
					x = 0;
				if (y < 0)
					y = 0;

				OpRect r(rect);

				if (x > r.x)
					r.x = x;
				if (y > r.y)
					r.y = y;

				XRenderComposite(m_display, PictOpSrc, src, None, dst, r.x-x, r.y-y, r.x-x, r.y-y, r.x, r.y, rect.width, rect.height);

				XFreePixmap(m_display, pix);
				XRenderFreePicture(m_display, src);
				XRenderFreePicture(m_display, dst);
			}
		}
	}
	else
	{
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;

		OpRect r(rect);

		if (x > r.x)
			r.x = x;
		if (y > r.y)
			r.y = y;

		XPutImage(m_display, m_window, m_gc, m_image, r.x-x, r.y-y, r.x, r.y, r.width, r.height);
	}
}

void X11MdeBuffer::UpdateShape(const OpRect &region)
{
	if (m_use_shaping)
	{
		UINT32 shape_width  = m_width + 8 - m_width%8;
		UINT32 shape_height = m_height;
		if (!m_shape_buffer)
		{
			m_shape_buffer = OP_NEWA(UINT8, (shape_width*shape_height)>>3);
			if (!m_shape_buffer)
				return;
			op_memset(m_shape_buffer, 0x00, (shape_width*shape_height)>>3);
		}

		int o = 0;
		int off = 0;
		BOOL changed = FALSE;

		// Keep the 32bit depth mode separate to minimize speed penalty
		if (m_depth == 32)
		{
			for (UINT32 y=0; y<m_height; y++)
			{
				o = y * shape_width;
				UINT32* buf = (UINT32*)&m_image->data[m_bytes_per_line*y];
				
				for (UINT32 x=0; x<m_width; x++)
				{
					off = o >> 3;
					int bit = 1 << (o%8);

					if (buf[x] & 0xFF000000)
					{
						if (!(m_shape_buffer[off] & bit))
						{
							m_shape_buffer[off] |= bit;
							changed = TRUE;
						}
					}
					else
					{
						if (m_shape_buffer[off] & bit)
						{
							m_shape_buffer[off] &= ~bit;
							changed = TRUE;
						}
					}
					o++;
				}
			}
		}
		else
		{
			for (UINT32 y=0; y<m_height; y++)
			{
				o = y * shape_width;
				UINT32* buf = (UINT32*)&m_image->data[m_bytes_per_line*y];
				
				for (UINT32 x=0; x<m_width; x++)
				{
					off = o >> 3;
					int bit = 1 << (o%8);

					if ( ((buf[x] & 0xFF000000) >> 24) >= g_startup_settings.shape_alpha_level)
					{
						if (!(m_shape_buffer[off] & bit))
						{
							m_shape_buffer[off] |= bit;
							changed = TRUE;
						}
					}
					else
					{
						if (m_shape_buffer[off] & bit)
						{
							m_shape_buffer[off] &= ~bit;
							changed = TRUE;
						}
					}
					o++;
				}
			}
		}

		if (changed)
		{
			X11Types::Pixmap pixmap = XCreatePixmapFromBitmapData(
				m_display, m_window,
				(char*)m_shape_buffer, shape_width, shape_height,
				WhitePixel(m_display,m_screen), BlackPixel(m_display,m_screen), 1);

			XShapeCombineMask(m_display, m_window,
							  ShapeBounding, 0, 0, pixmap, ShapeSet);

			XFreePixmap(m_display, pixmap);
		}
	}

}


OP_STATUS X11MdeBuffer::SetBitmap(OpBitmap& bitmap)
{
	RETURN_IF_ERROR(AllocateBuffer(bitmap.Width(), bitmap.Height()));

	if (m_image && m_image->data)
	{
		UINT32* src = (UINT32*)bitmap.GetPointer(OpBitmap::ACCESS_READONLY);
		if (src)
		{
			UINT32* dest = (UINT32*)m_image->data;

			UINT32 size = m_bytes_per_line * m_height / 4;
			for (UINT32 i=0; i<size; i++)
			{
				dest[i]=src[i];
			}
			bitmap.ReleasePointer(FALSE);
		}
	}

	return OpStatus::OK;

}

OP_STATUS X11MdeBuffer::SetBitmap(OpBitmap& bitmap, UINT32 src_x, UINT32 src_y, const OpRect& rect)
{
	if (!m_image)
	{
		// TODO: This shall use m_external_backbuffer instead 
		return OpStatus::ERR;
	}

	if (src_x >= bitmap.Width() || src_y >= bitmap.Height())
		return OpStatus::ERR;

	UINT32* src = static_cast<UINT32*>(bitmap.GetPointer(OpBitmap::ACCESS_READONLY));
	if (src)
	{
		UINT32* dest = reinterpret_cast<UINT32*>(m_image->data);
		OpRect b(src_x, src_y, bitmap.Width()-src_x, bitmap.Height()-src_y);
		OpRect r(0, 0, m_width, m_height);
		r.IntersectWith(rect);

		if (b.width > r.width)
			b.width = r.width;
		if (b.height > r.height)
			b.height = r.height;

		dest = dest + r.x + m_width * r.y;
		src  = src  + b.x + bitmap.Width() * b.y;

		for (INT32 i=0; i<b.height; i++)
		{
			memcpy(dest, src, b.width*4);
			dest += m_width;
			src  += bitmap.Width();
		}
		bitmap.ReleasePointer();
	}

	return OpStatus::OK;
}


void X11MdeBuffer::LowerOpacity(UINT8 opacity)
{
	if (m_depth == 32 && m_image && m_image->data)
	{
		UINT32* p = (UINT32*)m_image->data;
		UINT32 size = m_width*m_height;

		for (UINT32 i=0; i<size; i++ )
		{
			UINT32 a = (p[i] & 0xFF000000) >> 24;
			UINT32 r = (p[i] & 0xFF0000) >> 16;
			UINT32 g = (p[i] & 0xFF00) >> 8;
			UINT32 b = (p[i] & 0xFF);

			// Correct is to divide by 255, but shifting is somewhat more efficient
			a = (a*opacity) >> 8;
			r = (r*opacity) >> 8;
			g = (g*opacity) >> 8;
			b = (b*opacity) >> 8;

			p[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
}


void X11MdeBuffer::SetUseShape(BOOL use_shaping)
{
	m_use_shaping = use_shaping && m_bits_per_pixel == 32;
	// Might want to update here we if start to use this function
	// when there is content
}


OP_STATUS X11MdeBuffer::AllocateBuffer(UINT32 width, UINT32 height)
{
	OP_ASSERT(!m_external_backbuffer);
	m_width = width;
	m_height = height;

	OP_DELETEA(m_shape_buffer);
	m_shape_buffer = 0;

	if (m_width == 0 || m_height == 0)
		return OpStatus::OK;

	m_bytes_per_line = m_width * m_bits_per_pixel >> 3;
	m_bytes_per_line = ((m_bytes_per_line + 3) / 4) * 4; // Word-align
	UINT32 bytes_needed = m_bytes_per_line * m_height;
	char *imagedata = 0;

	if (m_image)
	{
		if (bytes_needed > m_buffer_size)
			free(m_image->data);
		else
			imagedata = m_image->data;
		m_image->data = 0;
		XDestroyImage(m_image);
		m_image = 0;
	}

	if (!imagedata)
	{
		imagedata = (char *)malloc(bytes_needed);
		if (!imagedata)
		{
			fprintf(stderr, "Failed to allocate XImage data\n");
			return OpStatus::ERR_NO_MEMORY;
		}
		m_buffer_size = bytes_needed;
	}
	
	m_image = XCreateImage(m_display, m_visual, m_depth, ZPixmap, 0, imagedata, m_width, m_height, 8, m_bytes_per_line);
	if (!m_image)
	{
		fprintf(stderr, "Failed to allocate XImage\n");
		free(imagedata);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


void X11MdeBuffer::SetExternalBackbuffer(X11Backbuffer * backbuffer)
{
	OP_ASSERT(!m_image);
	m_external_backbuffer = backbuffer;
}


GC X11MdeBuffer::GetPluginGC()
{
	if(m_plugin_gc == None)
		m_plugin_gc = XCreateGC(m_display, m_window, 0, 0);

	return m_plugin_gc;
}


OP_STATUS X11MdeBuffer::CopyBufferDataToDrawable(uint32_t drawable, const OpRect &source_rect, OpPainter * painter)
{
	if (!m_external_backbuffer)
	{
		OP_ASSERT(!"CopyBufferDataToDrawable() only implemented for OpenGL back buffer");
		return OpStatus::ERR;
	}
	if (!painter)
	{
		OP_ASSERT(!"CopyBufferDataToDrawable() without painter not implemented");
		return OpStatus::ERR;
	}

	if (!painter->Supports(OpPainter::SUPPORTS_GETSCREEN))
	{
		OP_ASSERT(!"CopyBufferDataToDrawable() without SUPPORTS_GETSCREEN not implemented");
		return OpStatus::ERR;
	}

	OpRect actual_source_rect = source_rect;
	/* We get the position relative to the X window's top left, but
	 * the painter has its origin at the page's top left.  (The window
	 * starts above the address bar and the tab bar, but below the
	 * menu bar).
	 */
	VEGAOpPainter* vegapainter = static_cast<VEGAOpPainter*>(painter);
	actual_source_rect.x -= vegapainter->GetTranslationX();
	actual_source_rect.y -= vegapainter->GetTranslationY();

	OpBitmap * bitmap = painter->CreateBitmapFromBackground(actual_source_rect);
	if (!bitmap)
	{
		OP_ASSERT(!"painter->CreateBitmapFromBackground() failed");
		return OpStatus::ERR;
	}

	XImage image;
	image.width = actual_source_rect.width;
	image.height = actual_source_rect.height;
	image.xoffset = 0;
	image.format = ZPixmap;
	image.data = NULL;
#ifdef OPERA_BIG_ENDIAN
	image.byte_order = MSBFirst;
#else
	image.byte_order = LSBFirst;
#endif
	image.bitmap_unit = 32;
	image.bitmap_bit_order = MSBFirst; //don't care
	image.bitmap_pad = 32;
	image.depth = 24;
	image.bytes_per_line = actual_source_rect.width * 4;
	image.bits_per_pixel = 32;
	image.red_mask = 0xff0000;
	image.green_mask = 0xff00;
	image.blue_mask = 0xff;

	XInitImage(&image);

	image.data = (char*)bitmap->GetPointer(OpBitmap::ACCESS_READONLY);
	if (!image.data)
	{
		OP_ASSERT(!"bitmap->GetPointer() failed");
		OP_DELETE(bitmap);
		return OpStatus::ERR;
	}

	GC gc = XCreateGC(GetDisplay(), drawable, 0, NULL);
	XPutImage(GetDisplay(), drawable, gc, &image, 0, 0, 0, 0, actual_source_rect.width, actual_source_rect.height);
	XFreeGC(GetDisplay(), gc);

	image.data = NULL;
	bitmap->ReleasePointer();
	OP_DELETE(bitmap);

	// No need to explicitly destroy 'image' or any of its members.

	return OpStatus::OK;
}

OP_STATUS X11MdeBuffer::CopyDrawableToBufferData(uint32_t drawable, const OpRect &dest_rect, OpPainter * painter)
{
	if (!m_external_backbuffer)
	{
		OP_ASSERT(!"CopyDrawableToBufferData() only implemented for OpenGL back buffer");
		return OpStatus::ERR;
	}
	if (!painter)
	{
		OP_ASSERT(!"CopyDrawableToBufferData() without painter not implemented");
		return OpStatus::ERR;
	}

	OpRect actual_dest_rect = dest_rect;
	/* We get the position relative to the X window's top left, but
	 * the painter has its origin at the page's top left.  (The window
	 * starts above the address bar and the tab bar, but below the
	 * menu bar).
	 */
	VEGAOpPainter* vegapainter = static_cast<VEGAOpPainter*>(painter);
	actual_dest_rect.x -= vegapainter->GetTranslationX();
	actual_dest_rect.y -= vegapainter->GetTranslationY();

	OpBitmap * bitmap = 0;
	OpBitmap::Create(&bitmap, actual_dest_rect.width, actual_dest_rect.height, FALSE, FALSE);

	XImage image;
	image.width = actual_dest_rect.width;
	image.height = actual_dest_rect.height;
	image.xoffset = 0;
	image.format = ZPixmap;
	image.data = NULL;
#ifdef OPERA_BIG_ENDIAN
	image.byte_order = MSBFirst;
#else
	image.byte_order = LSBFirst;
#endif
	image.bitmap_unit = 32;
	image.bitmap_bit_order = MSBFirst; //don't care
	image.bitmap_pad = 32;
	image.depth = 24;
	image.bytes_per_line = actual_dest_rect.width * 4;
	image.bits_per_pixel = 32;
	image.red_mask = 0xff0000;
	image.green_mask = 0xff00;
	image.blue_mask = 0xff;

	XInitImage(&image);

	image.data = (char*)bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);

	XGetSubImage(GetDisplay(),
				 drawable,           // src
				 0,                    // src.x
				 0,                    // src.y
				 actual_dest_rect.width,  // src.width
				 actual_dest_rect.height, // src.height
				 AllPlanes,            // plane mask
				 ZPixmap,              // format
				 &image,     // dest
				 0,      // dest.x
				 0);     // dest.y


/* Even though 'bitmap' should not care about the alpha values (since
 * that's what I asked for in OpBitmap::Create, it seems we need to
 * set them anyway.
 */
#if 1
	/* The XGetSubImage() call above will typically clear the
	 * alpha values of all the relevant pixels.  This code sets
	 * those pixels to fully opaque.
	 */
	int start = 0;
	if (image.byte_order == LSBFirst)
		start += 3;
	char * data = image.data;
	for (int y = 0; y < image.height; y++)
	{
		int next = start;
		for (int x = 0; x < image.width; x++)
		{
			data[next] = 0xff;
			next += 4;
		};
		start += image.bytes_per_line;
	};
#endif

	image.data = NULL;
	bitmap->ReleasePointer();

	painter->DrawBitmap(bitmap, OpPoint(actual_dest_rect.x, actual_dest_rect.y));

	OP_DELETE(bitmap);

	// No need to explicitly destroy 'image' or any of its members.

	return OpStatus::OK;
}


void X11MdeBuffer::ScrollPixels(const MDE_RECT &rect, int dx, int dy)
{
	if (m_gc == None)
		return;

	if (m_widget != 0)
		m_widget->CopyArea(m_gc, rect.x - dx, rect.y - dy, rect.w, rect.h, rect.x, rect.y);
	else
		XCopyArea(m_display, m_window, m_window, m_gc,
				  rect.x - dx, rect.y - dy, rect.w, rect.h, rect.x, rect.y);
}


// static
UINT32* X11MdeBuffer::Premultiply(UINT32* buf, UINT32 size)
{
#if defined(USE_PREMULTIPLIED_ALPHA)
	UINT32 val, g, alpha;
	for (UINT32 i=0; i<size; i++)
	{
		val = buf[i];
		if ((val&0xFF000000) != 0xFF000000)
		{
			alpha = (val >> 24);
			if (alpha == 0)
			{
				buf[i] = 0;
			}
			else
			{
				g = val & 0x0000FF00; // green
				val &= 0x00FF00FF;    // red and blue
				alpha++;
				g *= alpha;
				g >>= 8;
				val *= alpha;
				val >>= 8;
				alpha--;
				buf[i] = (alpha << 24) | (val & 0x00FF00FF) | (g & 0x0000FF00);
			}
		}
	}
#endif
	return buf;

}

#endif // VEGA_OPPAINTER_SUPPORT
