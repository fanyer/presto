/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_icon.h"
#include "modules/pi/OpBitmap.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpWidgetImage.h"

#include "platforms/unix/product/x11quick/iconsource.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"

using X11Types::Atom;

X11Icon::X11Icon(int screen)
	:m_bitmap(0)
	,m_pixmap(0)
	,m_mask(0)
	,m_screen(screen < 0 || screen >= g_x11->GetScreenCount() ? g_x11->GetDefaultScreen() : screen)
	,m_width(0)
	,m_height(0)
	,m_depth(0)
	,m_dither_level(-1)
	,m_buffer(0)
	,m_background_buffer(0)
	,m_has_background_color(FALSE)
	,m_background_color(0)
{
}

X11Icon::~X11Icon()
{
	Reset();
}

void X11Icon::Reset()
{
	m_width  = 0;
	m_height = 0;

	OP_DELETE(m_bitmap);
	m_bitmap = 0;

	OP_DELETEA(m_buffer);
	m_buffer = 0;

	OP_DELETEA(m_background_buffer);
	m_background_buffer = 0;

	X11Types::Display* dpy = g_x11 ? g_x11->GetDisplay() : 0;
	if (dpy)
	{
		if (m_pixmap)
		{
			XFreePixmap( dpy, m_pixmap );
			m_pixmap = 0;
		}
		if (m_mask)
		{
			XFreePixmap( dpy, m_mask );
			m_mask = 0;
		}
	}
}


OP_STATUS X11Icon::SetBuffer(OpBitmap* bitmap)
{
	OP_DELETE(m_bitmap);
	m_bitmap = 0;

	if (m_width == 0)
		m_width = bitmap->Width();
	if (m_height == 0)
		m_height = bitmap->Height();

	OP_STATUS rc = MakeBuffer(bitmap);
	MakeMask( TRUE, 0 );
	return rc;
}


OP_STATUS X11Icon::SetBuffer(OpWidgetImage* image)
{
	INT32 source_width, source_height;

	image->GetSize(&source_width, &source_height);
	if (source_width <= 0 || source_height <= 0)
		return OpStatus::ERR;

	if (m_width == 0)
		m_width = source_width;
	if (m_height == 0)
		m_height = source_height;

	OpBitmap* bitmap = 0;
	RETURN_IF_ERROR(CreateOpBitmapFromOpWidgetImage(image, &bitmap, m_width, m_height, 0));
	OP_STATUS rc = SetBuffer(bitmap);
	OP_DELETE(bitmap);
	return rc;
}


OP_STATUS X11Icon::SetBuffer(const UINT8* buffer, UINT32 num_bytes)
{
	SimpleFileImage fi(buffer, num_bytes);
	if (fi.GetImage().Width() <= 0 || fi.GetImage().Height() <= 0)
		return OpStatus::ERR;

	OpWidgetImage widget_image;
	Image image = fi.GetImage();
	widget_image.SetBitmapImage(image);
	return SetBuffer(&widget_image);
}


OP_STATUS X11Icon::SetPixmap(OpBitmap* bitmap, BOOL make_mask, UINT32 depth, UINT32 color)
{
	if (m_width == 0)
		m_width = bitmap->Width();
	if (m_height == 0)
		m_height = bitmap->Height();
	m_depth = depth;

	char* src = (char*)bitmap->GetPointer(OpBitmap::ACCESS_READONLY);
	if (src)
	{
		X11Types::Display* dpy = g_x11->GetDisplay();
		X11Types::Window win = RootWindow(dpy, m_screen);

		XImage img;
		img.width = m_width;
		img.height = m_height;
		img.xoffset = 0;
		img.format = ZPixmap;
		img.data = src;
#ifdef OPERA_BIG_ENDIAN
		img.byte_order = MSBFirst;
#else
		img.byte_order = LSBFirst;
#endif
		img.bitmap_unit = 8;
		img.bitmap_bit_order = LSBFirst; // don't care
		img.bitmap_pad = 8;
		if (m_depth == 32)
			img.depth = 32;
		else
			img.depth = 24;
		img.bytes_per_line = m_width * 4;
		img.bits_per_pixel = 32;
		img.red_mask = 0xff0000;
		img.green_mask = 0xff00;
		img.blue_mask = 0xff;
		img.obdata = 0;
		XInitImage(&img);

		m_pixmap = XCreatePixmap(dpy, win, m_width, m_height, m_depth);
		if (m_pixmap != None)
		{
			GC gc = XCreateGC(dpy, m_pixmap, 0, 0);
			XPutImage(dpy, m_pixmap, gc, &img, 0, 0, 0, 0, m_width, m_height);
		}

		img.data = 0;

		bitmap->ReleasePointer(FALSE);
	}

	if (make_mask && depth != 32)
		RETURN_IF_ERROR(MakeMask(FALSE, color));
	return OpStatus::OK;
}



OP_STATUS X11Icon::SetPixmap(OpWidgetImage* image, BOOL make_mask, UINT32 depth, UINT32 color)
{
	INT32 source_width, source_height;

	image->GetSize(&source_width, &source_height);
	if (source_width <= 0 || source_height <= 0)
		return OpStatus::ERR;

	if (m_width == 0)
		m_width = source_width;
	if (m_height == 0)
		m_height = source_height;
	m_depth = depth;

	OpBitmap* bitmap = 0;
	RETURN_IF_ERROR(CreateOpBitmapFromOpWidgetImage(image, &bitmap, m_width, m_height, m_depth == 32 ? 0 : color));
	OP_STATUS rc = SetPixmap(bitmap, make_mask, depth, color);
	OP_DELETE(bitmap);
	return rc;
}


OP_STATUS X11Icon::SetPixmap(const UINT8* buffer, UINT32 num_bytes, BOOL make_mask, UINT32 depth, UINT32 color)
{
	SimpleFileImage fi(buffer, num_bytes);
	if (fi.GetImage().Width() <= 0 || fi.GetImage().Height() <= 0)
		return OpStatus::ERR;

	OpWidgetImage widget_image;
	Image image = fi.GetImage();
	widget_image.SetBitmapImage(image);
	return SetPixmap(&widget_image, make_mask, depth, color);
}


OP_STATUS X11Icon::SetBackground( UINT32 color )
{
	if (!m_has_background_color || m_background_color != color)
	{
		OP_DELETEA(m_background_buffer);
		m_background_buffer = 0;
 
		m_has_background_color = TRUE;
		m_background_color = color;

		if (m_bitmap)
		{
			OP_DELETE(m_bitmap);
			m_bitmap = 0;

			return MakeBitmap();
		}
	}

	return OpStatus::OK;
}



OP_STATUS X11Icon::SetBackground( X11Types::Drawable handle, OpRect& rect, UINT32 depth )
{
	m_has_background_color = FALSE;

	X11Types::Display* dpy = g_x11->GetDisplay();

	OP_DELETEA(m_background_buffer);
	m_background_buffer = 0;

	GC gc = XCreateGC(dpy, handle, 0, 0);
	X11Types::Pixmap pixmap = XCreatePixmap(dpy, handle, rect.width, rect.height, depth);
	XCopyArea(dpy, handle, pixmap, gc, rect.x, rect.y, rect.width, rect.height, 0, 0);
	XFreeGC(dpy, gc);

	XImage* ximage = XGetImage( dpy, pixmap, 0, 0, rect.width, rect.height, AllPlanes, ZPixmap );
	if (!ximage)
		return OpStatus::ERR_NO_MEMORY;

	int size = 2 + (rect.width*rect.height);
	m_background_buffer = OP_NEWA(UINT32, size);
	if (!m_background_buffer)
	{
		XDestroyImage(ximage);
		return OpStatus::ERR_NO_MEMORY;
	}

	m_background_buffer[0] = rect.width;
	m_background_buffer[1] = rect.height;
	for (INT32 y=0; y<rect.height; y++)
	{
		for (INT32 x=0; x<rect.width; x++)
		{
			m_background_buffer[2 + (y*rect.width+x)] = (depth == 32 ? 0 : 0xFF000000) | XGetPixel(ximage, x, y);
		}
	}

	XDestroyImage(ximage);

	OP_STATUS rc = ScaleBuffer(rect.width, rect.height);
	if (OpStatus::IsSuccess(rc))
	{
		if (m_bitmap)
		{
			OP_DELETE(m_bitmap);
			m_bitmap = 0;

			rc = MakeBitmap();
		}
	}

	return rc;
}


OP_STATUS X11Icon::CreateOpBitmapFromOpWidgetImage(OpWidgetImage * image, OpBitmap ** ret_bmp, UINT32 width, UINT32 height, COLORREF bg_color)
{
	*ret_bmp = 0;

	OpBitmap* bmp;
	RETURN_IF_ERROR(OpBitmap::Create(&bmp, m_width, m_height, FALSE, TRUE, 0, 0, TRUE));

	// Fill with background colour
	OP_ASSERT((bg_color & 0x80000000) == 0); // High bit is magic in COLORREF

	UINT32 * bmp_data = (UINT32*)bmp->GetPointer(OpBitmap::ACCESS_WRITEONLY);
	if (bmp_data)
	{
		UINT32 size = m_width * m_height;

		if (bg_color == 0)
		{
			// Common case, fully transparent black
			memset(bmp_data, 0, size * 4);
		}
		else
		{
			UINT8 a = OP_GET_A_VALUE(bg_color);
			UINT8 r = OP_GET_R_VALUE(bg_color);
			UINT8 g = OP_GET_G_VALUE(bg_color);
			UINT8 b = OP_GET_B_VALUE(bg_color);
			UINT32 bmp_color = (a << 24) | (r << 16) | (g << 8) | b;

			for (UINT32 i = 0; i < size; i++)
				bmp_data[i] = bmp_color;
		};
		bmp->ReleasePointer();
	}

	// Paint the image
	OpRect rect(0, 0, m_width, m_height);
	VisualDevice vd;
	vd.BeginPaint(bmp->GetPainter(), rect, rect);
	image->Draw(&vd, rect);
	vd.EndPaint();
	bmp->ReleasePainter();

	*ret_bmp = bmp;

	return OpStatus::OK;
};



OP_STATUS X11Icon::ScaleBuffer(UINT32 width, UINT32 height)
{
	if (!m_buffer)
		return OpStatus::ERR;

	if (width == m_buffer[0] && height == m_buffer[1])
		return OpStatus::OK;

	UINT32 w = m_buffer[0];
	UINT32 h = m_buffer[1];
	OpBitmap* src = 0;
	RETURN_IF_ERROR(OpBitmap::Create(&src, w, h));

	UINT32* line = &m_buffer[2];
	for (UINT32 i=0; i<h; i++)
	{
		src->AddLine(line, i);
		line += w;
	}

	OpBitmap* dest = 0;
	OP_STATUS rc = OpBitmap::Create(&dest, width, height, FALSE, TRUE, 0, 0, TRUE);
	if (OpStatus::IsSuccess(rc))
	{
		dest->GetPainter()->DrawBitmapScaled(src, OpRect(0,0,w,h), OpRect(0,0,width,height));
		rc = MakeBuffer(dest);
	}

	OP_DELETE(src);
	OP_DELETE(dest);

	return rc;
}



OP_STATUS X11Icon::MakeBitmap()
{
	if (!m_bitmap && m_buffer)
	{
		UINT32 w = m_buffer[0];
		UINT32 h = m_buffer[1];
		RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap, w, h));

		if (m_has_background_color)
		{
			UINT32* buf = OP_NEWA(UINT32,w);
			if (!buf)
			{
				OP_DELETE(m_bitmap);
				m_bitmap = 0;
				return OpStatus::ERR_NO_MEMORY;
			}

			UINT32* fg = &m_buffer[2];
			for (UINT32 i=0; i<h; i++)
			{
				for (UINT32 j=0; j<w; j++)
					buf[j] = m_background_color;

				BlendLine( fg, buf, w );
				m_bitmap->AddLine(buf, i);
				fg += w;
			}
			OP_DELETEA(buf);
		}
		else if (m_background_buffer)
		{
			UINT32* buf = OP_NEWA(UINT32,w);
			if (!buf)
			{
				OP_DELETE(m_bitmap);
				m_bitmap = 0;
				return OpStatus::ERR_NO_MEMORY;
			}

			UINT32* bg = &m_background_buffer[2];
			UINT32* fg = &m_buffer[2];

			for (UINT32 i=0; i<h; i++)
			{
				memcpy(buf, bg, w*sizeof(UINT32));
				BlendLine( fg, buf, w );
				m_bitmap->AddLine(buf, i);
				bg += w;
				fg += w;
			}
			OP_DELETEA(buf);
		}
		else
		{
			UINT32* line = &m_buffer[2];
			for (UINT32 i=0; i<h; i++)
			{
				m_bitmap->AddLine(line, i);
				line += w;
			}
		}
	}
	return OpStatus::OK;
}




OP_STATUS X11Icon::MakeMask( BOOL use_buffer, UINT32 mask_color )
{
	X11Types::Display* dpy = g_x11->GetDisplay();
	X11Types::Window win = RootWindow(dpy, m_screen);

	int bw = m_width; // /*+ 8*/ - m_width%8;
	int bh = m_height;
	int size = (bw*bh)/8;

	unsigned char* buffer = OP_NEWA(unsigned char, size);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;
	memset(buffer, 0, size);

	int o = 0;
	int off = 0;

	if (use_buffer)
	{
		UINT32* src = GetBuffer();
		if (!src)
		{
			OP_DELETEA(buffer);
			return OpStatus::ERR;
		}

		src+=2;

		for (UINT32 y=0; y<m_height; y++)
		{
			o = y * bw;
			for (UINT32 x=0; x<m_width; x++)
			{
				off = o/8;
				int bit = 1 << (o%8);

				unsigned long pixel = src[(y*m_width)+x];
				if (pixel == 0)
					buffer[off] |= bit;
			}
		}
	}
	else
	{
		if (!m_pixmap)
		{
			OP_DELETEA(buffer);
			return OpStatus::ERR;
		}

		XImage* ximage = XGetImage( dpy, m_pixmap, 0, 0, m_width, m_height, 0xFFFFFF, ZPixmap );
		if (!ximage)
		{
			OP_DELETEA(buffer);
			return OpStatus::ERR_NO_MEMORY;
		}

		mask_color &= 0x00FFFFFF;

		for (UINT32 y=0; y<m_height; y++)
		{
			o = y * bw;
			for (UINT32 x=0; x<m_width; x++)
			{
				off = o/8;
				int bit = 1 << (o%8);

				unsigned long pixel = XGetPixel( ximage, x, y);

				if (pixel == mask_color)
					buffer[off] |= bit;
				o++;
			}
		}
		XDestroyImage(ximage);
	}

	m_mask = XCreatePixmapFromBitmapData(dpy, win, (char*)buffer, bw, bh, BlackPixel(dpy,m_screen), WhitePixel(dpy,m_screen), 1);
	OP_DELETEA(buffer);

	return OpStatus::OK;
}


OP_STATUS X11Icon::MakeBuffer(OpBitmap* bitmap)
{
	OP_DELETEA(m_buffer);
	m_buffer = 0;

    UINT32 width  = bitmap->Width();
	UINT32 height = bitmap->Height();
	UINT32 size   = 2 + (width*height);
 
	m_buffer = OP_NEWA(UINT32, size);
	if (!m_buffer)
		return OpStatus::ERR_NO_MEMORY;

	UINT32 *line = &m_buffer[2];

	m_buffer[0] = width;
	m_buffer[1] = height;

	for (UINT32 i=0; i<height; i++)
	{
		bitmap->GetLineData(line, i);
		line += width;
	}

	return OpStatus::OK;
}


void X11Icon::Apply(X11Types::Window window)
{
	X11Types::Display* dpy = g_x11->GetDisplay();

	if (GetBuffer())
	{
		// Xlib assumes the buffer to be of type long if the format is 32
		// On 64-bit systems that means 64-bit integers. Keep it clean, convert in all cases
		UINT32 size = GetBufferSize();
		unsigned long* buffer = OP_NEWA(unsigned long, size);
		if (buffer)
		{
			for (UINT32 i=0; i<size; i++)
				buffer[i] = GetBuffer()[i];

			XChangeProperty(dpy, window, ATOMIZE(_NET_WM_ICON), XA_CARDINAL, 32,
							PropModeReplace, (unsigned char*)buffer, size);
			OP_DELETEA(buffer);
		}
	}

	if (GetPixmap() != None)
	{
		XWMHints* hints = XGetWMHints( dpy, window );
		XWMHints  wm_hints;

		bool got_hints = !!hints;
		if (!got_hints)
		{
			hints = &wm_hints;
			hints->flags = 0;
		}

		hints->icon_pixmap = GetPixmap();
		hints->flags |= IconPixmapHint;

		if (GetMask() != None)
		{
			hints->icon_mask = GetMask();
			hints->flags |= IconMaskHint;
		}

		XSetWMHints(dpy, window, hints);

		if (got_hints)
			XFree((char*)hints);
	}
}


void X11Icon::BlendLine( const UINT32* src, UINT32* dest, UINT32 size )
{
	while (size > 0)
	{
		int o = *src >> 24;
		switch (o)
		{
			case 0:
				break;

			case 255:
				*dest = *src;
				break;

			default:
			{
				int t = 255 - o;

				UINT32 s = *src;
				UINT32 d = *dest;

				int da = d >> 24;
				t = t * da / 255;
				int ta = ((((255 - o) * da) + o * 255) + 127) / 255;

				*dest =
					(ta << 24) |
					((((((s & 0xff0000) >> 16) * o + ((d & 0xff0000) >> 16) * t) + 127) / 255) << 16) |
					((((((s & 0xff00) >> 8) * o + ((d & 0xff00) >> 8) * t) + 127) / 255) << 8) |
					((((s & 0xff) * o + (d & 0xff) * t) + 127) / 255);
				break;
			}
		}
		src++;
		dest++;
		size--;
	}
}

// static
void X11Icon::SetApplicationIcon(X11Widget* widget)
{
	X11Icon* icon = OP_NEW(X11Icon,());
	if (icon)
	{
		UINT8* buffer;
		UINT32 size;
		UINT32 unit = 48;

		if (IconSource::GetApplicationIcon(unit, buffer, size))
		{
			icon->SetBuffer(buffer, size);
			// For older window managers that do not not support NET_WM (DSK-310375)
			// White background and no shape.
			icon->SetPixmap(buffer, size, FALSE, 24, 0x7FFFFFFF);
		}

		widget->SetIcon(icon);
	}
}




/*
void X11Icon::DumpBuffer()
{
	if( m_buffer )

		for( UINT32 y=0; y<GetHeight(); y++ )
		{
			for( UINT32 x=0; x<GetWidth(); x++ )
			{
				printf("%08x ", m_buffer[2+(y*GetWidth()+x)]);
			}
			puts("");
		}
	}
}
*/
