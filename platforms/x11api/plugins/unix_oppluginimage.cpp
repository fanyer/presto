/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "platforms/x11api/pi/x11_client.h"
#include "platforms/x11api/plugins/unix_oppluginimage.h"

/* static */ OP_STATUS
OpPluginImage::Create(OpPluginImage** out_image, OpPluginBackgroundImage** background, const OpRect&, OpWindow* target_opwindow)
{
	UnixOpPluginBackgroundImage *bg = OP_NEW(UnixOpPluginBackgroundImage, ());
	RETURN_OOM_IF_NULL(bg);

	X11Types::Window toplevel_window = X11Client::Instance()->GetTopLevelWindow(target_opwindow);
	int screen_num = X11Client::Instance()->GetScreenNumber(toplevel_window);
	X11Types::Display *display = X11Client::Instance()->GetDisplay();

	// Create plugin drawable
	*out_image = OP_NEW(UnixOpPluginImage, (bg, display, screen_num));
	if (!*out_image)
	{
		OP_DELETE(bg);
		*background = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	*background = bg;
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginBackgroundImage::Copy(X11Types::Display* dpy, X11Types::Visual* vis, OpBitmap* bitmap, const OpRect& plugin_rect, unsigned int depth)
{
	OP_ASSERT(m_bitmap || !"UnixOpPluginBackgroundImage::Copy: No buffer allocated");

	char* data = reinterpret_cast<char*>(bitmap->GetPointer(OpBitmap::ACCESS_READONLY));

	// If pixel data are not equal, we'll need to do some more work.
	if (op_memcmp(data, m_bitmap, plugin_rect.width * plugin_rect.height * sizeof(uint32_t)) != 0)
	{
		// Copy plugin background to drawable.
		XImage* xim = XCreateImage(dpy, vis, depth, ZPixmap, 0, data,
		                           plugin_rect.width, plugin_rect.height, UnixOpPluginImage::kBitsPerPixel, 0);
		RETURN_OOM_IF_NULL(xim);

		GC gc = XCreateGC(dpy, m_drawable, 0, 0);
		XPutImage(dpy, m_drawable, gc, xim, 0, 0, 0, 0, plugin_rect.width, plugin_rect.height);
		XFreeGC(dpy, gc);

		// Copy new plugin background over the old one.
		op_memcpy(m_bitmap, data, plugin_rect.width * plugin_rect.height * sizeof(uint32_t));

		// Synchronize in order for other processes to "see" it.
		XSync(dpy, False);

		// We set xim->data to zero to because it belongs to OpBitmap. Deleting
		// it here will only cause trouble.
		xim->data = 0;
		XDestroyImage(xim);
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginBackgroundImage::Resize(X11Types::Display* dpy, int screen_number, const OpRect& plugin_rect, unsigned int depth)
{
	if (plugin_rect.width == m_plugin_rect.width && plugin_rect.height == m_plugin_rect.height)
		return OpStatus::OK;

	X11Types::Drawable drawable = XCreatePixmap(dpy, RootWindow(dpy, screen_number), plugin_rect.width, plugin_rect.height, depth);
	if (drawable == None)
		return OpStatus::ERR_NO_MEMORY;

	uint32_t* bitmap = OP_NEWA(uint32_t, plugin_rect.width * plugin_rect.height);
	if (!bitmap)
	{
		XFreePixmap(dpy, drawable);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (m_drawable)
		XFreePixmap(dpy, m_drawable);
	m_drawable = drawable;

	OP_DELETEA(m_bitmap);
	m_bitmap = bitmap;

	m_plugin_rect = plugin_rect;
	return OpStatus::OK;
}

OpPluginImageID UnixOpPluginBackgroundImage::GetGlobalIdentifier()
{
	OP_ASSERT(m_disabled || m_drawable
	          || !"UnixOpPluginBackgroundImage::GetGlobalIdentifier: Image is enabled but is missing a drawable.");
	return m_disabled ? 0 : static_cast<OpPluginImageID>(m_drawable);
}

UnixOpPluginImage::UnixOpPluginImage(UnixOpPluginBackgroundImage* background, X11Types::Display* display, int screen_num)
	: m_display(display),
	  m_screen_num(screen_num),
	  m_drawable(0),
	  m_background(background) { }

UnixOpPluginImage::~UnixOpPluginImage()
{
	if (m_drawable)
		XFreePixmap(m_display, m_drawable);
}

/* virtual */ OP_STATUS
UnixOpPluginImage::Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect)
{
	OpBitmap* bitmap = 0;
	RETURN_IF_ERROR(OpBitmap::Create(&bitmap, paint_rect.width, paint_rect.height, FALSE, FALSE));
	OpAutoPtr<OpBitmap> p_bitmap (bitmap);

	XVisualInfo vis_info;
	if (!XMatchVisualInfo(m_display, m_screen_num, DefaultDepth(m_display, m_screen_num), TrueColor, &vis_info))
		return OpStatus::ERR;

	XImage* ximage = XCreateImage(m_display,
	                              vis_info.visual,
	                              vis_info.depth,
	                              ZPixmap,
	                              0,
	                              reinterpret_cast<char*>(bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY)),
	                              paint_rect.width, paint_rect.height,
	                              UnixOpPluginImage::kBitsPerPixel,
	                              0);
	if (!ximage)
	{
		bitmap->ReleasePointer();
		return OpStatus::ERR_NO_MEMORY;
	}

	if (!XGetSubImage(m_display, m_drawable, paint_rect.x, paint_rect.y, paint_rect.width, paint_rect.height, AllPlanes, ZPixmap, ximage, 0, 0))
	{
		bitmap->ReleasePointer();
		ximage->data = 0;
		XDestroyImage(ximage);
		return OpStatus::ERR;
	}

	/**
	 * Force alpha channel to fully opaque. This is currently required by the
	 * hardware-accelerated backend because it ends up creating an RGBA texture
	 * without touching the alpha bits. Until RGB textures are supported, we'll
	 * have to do this.
	 */
	uint32_t* data = reinterpret_cast<uint32_t*>(ximage->data);
	for (int i = 0; i < paint_rect.width * paint_rect.height; ++data, ++i)
		*data |= 0xff000000;

	// ReleasePointer must follow a GetPointer(ACCESS_WRITEONLY)
	bitmap->ReleasePointer();

	ximage->data = 0;
	XDestroyImage(ximage);

	// Paint rect is relative to the plugin. Offset by the plug-in offset.
	OpRect dirty_rect(paint_rect);
	dirty_rect.OffsetBy(plugin_rect.x, plugin_rect.y);

	painter->DrawBitmapClipped(bitmap, OpRect(0, 0, paint_rect.width, paint_rect.height), OpPoint(dirty_rect.x, dirty_rect.y));

	OP_DELETE(p_bitmap.release());
	return OpStatus::OK;
}

/* virtual */ OpPluginImageID
UnixOpPluginImage::GetGlobalIdentifier()
{
	return static_cast<OpPluginImageID>(m_drawable);
}

OP_STATUS UnixOpPluginImage::Resize(const OpRect& plugin_rect, unsigned int depth)
{
	OP_ASSERT(plugin_rect.width != m_plugin_rect.width || plugin_rect.height != m_plugin_rect.height);

	X11Types::Drawable drawable = XCreatePixmap(m_display, RootWindow(m_display, m_screen_num), plugin_rect.width, plugin_rect.height, depth);
	if (drawable == None)
		return OpStatus::ERR_NO_MEMORY;
	if (m_drawable)
		XFreePixmap(m_display, m_drawable);
	m_drawable = drawable;

	m_plugin_rect = plugin_rect;
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginImage::Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent)
{
	const unsigned int depth = DefaultDepth(m_display, m_screen_num);

	if (!transparent)
	{
		m_background->Disable();
		OP_STATUS status = OpStatus::OK;
		if (plugin_rect.width > m_plugin_rect.width || plugin_rect.height > m_plugin_rect.height)
		{
			status = Resize(plugin_rect, depth);
			XSync(m_display, False);
		}
		return status;
	}
	if (m_background->IsDisabled())
	{
		m_background->Enable();
		RETURN_IF_ERROR(m_background->Resize(m_display, m_screen_num, plugin_rect, depth));
	}

	// Retrieve plugin background from painter.
	// Note: Bitmap seems to point to data only. It does not create a real copy!
	OpBitmap* bitmap = painter->CreateBitmapFromBackground(plugin_rect);
	RETURN_OOM_IF_NULL(bitmap);
	OpAutoPtr<OpBitmap> p_bitmap (bitmap);

	// Resize buffers if necessary
	if (plugin_rect.width > m_plugin_rect.width || plugin_rect.height > m_plugin_rect.height)
		if (OpStatus::IsError(m_background->Resize(m_display, m_screen_num, plugin_rect, depth))
			|| OpStatus::IsError(Resize(plugin_rect, depth)))
			return OpStatus::ERR_NO_MEMORY;

	// Copy background
	OP_STATUS status = m_background->Copy(m_display, DefaultVisual(m_display, m_screen_num), bitmap, plugin_rect, depth);
	OP_DELETE(p_bitmap.release());

	return status;
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
