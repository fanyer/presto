/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)
#if defined(VEGA_OPPAINTER_SUPPORT) && defined(MDE_NATIVE_WINDOW_SUPPORT)

#include "platforms/x11api/plugins/plugin_unified_vega.h"
#include "platforms/x11api/pi/x11_client.h"

#include "modules/pi/OpBitmap.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "platforms/utilix/src/x11windowmanager.h"
#include "modules/pi/OpView.h"

PluginUnifiedVega::~PluginUnifiedVega()
{
	if (m_parent)
		m_parent->RemoveChild(this);
	if (m_static_image)
		XDestroyImage(m_static_image);
	OP_DELETE(m_x11_window_manager);
}

OP_STATUS PluginUnifiedVega::Init(X11Types::Display* dpy)
{
	OP_NEW_DBG("PluginUnifiedVega::Init()", "plugix.window");

	RETURN_IF_ERROR(PluginUnifiedWindowed::Init(dpy));

	m_x11_window_manager = OP_NEW(X11WindowManager, (m_display));

	// Move the plugin to its initial position.
	OpPoint initial_pos(m_plugin_rect.x, m_plugin_rect.y);
	initial_pos = m_plugin_parent->ConvertToScreen(initial_pos);
	MoveWindow(initial_pos.x, initial_pos.y);

#ifdef DRAG_SUPPORT
	SetDragListener(m_plugin_parent->GetDragListener(), m_plugin_parent);
#endif // DRAG_SUPPORT

	return OpStatus::OK;
}

void PluginUnifiedVega::Detach()
{
	OP_NEW_DBG("PluginUnifiedVega::Detach()", "plugix.vega");

	// Remove this MDE window from its parent
	if (m_parent)
		m_parent->RemoveChild(this);

	// Detach plugin window from its parent
	PluginUnifiedWindowed::Detach();
}

void PluginUnifiedVega::Hide()
{
	MDENativeWindow::Show(FALSE);
	if (m_parent)
		m_parent->RemoveChild(this);
}

void PluginUnifiedVega::Show()
{
	MDENativeWindow::Show(TRUE);

	if (m_parent)
		return;

	static_cast<MDE_OpView*>(m_plugin_parent)->GetMDEWidget()->AddChild(this);
	SetZ(MDE_Z_TOP);
}

void PluginUnifiedVega::MoveWindow(int x, int y)
{
	OP_NEW_DBG("PluginUnifiedVega::MoveWindow()", "plugix.vega");
	OP_DBG(("this: %p; (%d,%d)", this, x, y));

	if (m_redirect_mode == REDIRECT_WITH_OPACITY)
		m_original_window_pos.Set(x, y);
	else
		PluginUnifiedWindowed::SetPos(x, y);
	m_needs_flush = true;
}

void PluginUnifiedVega::ShowWindow(BOOL show)
{
	OP_NEW_DBG("PluginUnifiedVega::ShowWindow()", "plugix.vega");
	OP_DBG(("ShowWindow()"));

	if (m_redirect_mode == REDIRECT_WITH_STATIC_IMAGE)
	{
		OP_DBG(("PluginUnifiedVega::ShowWindow: REDIRECT_WITH_STATIC_IMAGE (nothing to do...)"));
		/* If we are in static-image mode, the plugin is hidden and we restore
		 * the visibility status in SetRedirected(FALSE) */
		return;
	}

	if (show)
		XMapWindow(m_display, m_mother_window);
	else
		XUnmapWindow(m_display, m_mother_window);
}

void PluginUnifiedVega::SetClipRegion(MDE_Region* region)
{
	OP_NEW_DBG("PluginUnifiedVega::SetClipRegion()", "plugix.vega");
	OP_DBG(("this: %p; clip region with %d rectangles", this, region->num_rects));

	m_clip_rect.Empty();

	XRectangle* rects = 0;
	if (region->num_rects > 0)
	{
		rects = OP_NEWA(XRectangle, region->num_rects);
		if (!rects)
			return;

		MDE_RECT *region_rects = region->rects;
		for (int i = 0; i < region->num_rects; ++i)
		{
			OP_DBG(("%d: (%d,%d)+%dx%d", i, region_rects->x, region_rects->y, region_rects->w, region_rects->h));
			m_clip_rect.UnionWith(OpRect(region_rects->x, region_rects->y, region_rects->w, region_rects->h));

			rects[i].x      = region_rects->x;
			rects[i].y      = region_rects->y;
			rects[i].width  = region_rects->w;
			rects[i].height = region_rects->h;

			++region_rects;
		}
		OP_DBG(("union of clip rects is: (%d,%d)+%dx%d", m_clip_rect.x, m_clip_rect.y, m_clip_rect.width, m_clip_rect.height));
	}
	XShapeCombineRectangles(m_display,
	                        m_mother_window,
	                        ShapeBounding,
	                        0,
	                        0,
	                        rects,
	                        region->num_rects,
	                        ShapeSet,
	                        Unsorted);
	OP_DELETEA(rects);
	m_needs_flush = true;
}

void PluginUnifiedVega::SetRedirected(BOOL redir)
{
	OP_NEW_DBG("PluginUnifiedVega::SetRedirected()", "plugix.vega");
	OP_DBG(("this: %p; redirected: %s->%s", this, (m_redirect_mode != NO_REDIRECT)?"on":"off", redir?"on":"off"));

	if (redir)
	{
		OP_ASSERT(m_redirect_mode == NO_REDIRECT);
		X11Types::Window plugin_window = m_mother_window;

		XWindowAttributes window_attr;
		XGetWindowAttributes(m_display, plugin_window, &window_attr);
		m_original_window_pos.x = window_attr.x;
		m_original_window_pos.y = window_attr.y;

		UpdateImage();
		if (m_x11_window_manager->SupportsWindowOpacity())
		{
			OP_DBG(("support opacity"));
			m_redirect_mode = REDIRECT_WITH_OPACITY;
			unsigned int ignore_child_count = 0;
			X11Types::Window* ignore_children = NULL;
			X11Types::Window ignore_root_return = 0;
			XQueryTree(m_display, plugin_window, &ignore_root_return, &m_original_parent, &ignore_children, &ignore_child_count);
			X11Types::Window root = RootWindow(m_display, DefaultScreen(m_display));
			OP_DBG(("set opacity to 0"));
			m_x11_window_manager->SetWindowOpacity(plugin_window, 0.0);
			m_x11_window_manager->SetWindowType(plugin_window);
			OP_DBG(("place plugin window off screen (%d,%d) -> (10000,10000)",
					m_original_window_pos.x, m_original_window_pos.y));
			PluginUnifiedWindowed::SetPos(10000, 10000);
			XReparentWindow(m_display, plugin_window, root, 10000, 10000);
			m_x11_window_manager->SuppressWindowTaskbar(plugin_window);
			XSync(m_display, False);
		}
		else
		{
			OP_DBG(("no opacity: hide the plugin window"));
			m_redirect_mode = REDIRECT_WITH_STATIC_IMAGE;
			XSync(m_display, False);
			XUnmapWindow(m_display, m_mother_window);
		}
	}
	else
	{
		if (m_static_image)
		{
			XDestroyImage(m_static_image);
			m_static_image = 0;
		}
		switch (m_redirect_mode)
		{
			case REDIRECT_WITH_OPACITY:
				OP_DBG(("REDIRECT_WITH_OPACITY: restore opacity to 1"));
				RestorePluginParent();
				break;
			case REDIRECT_WITH_STATIC_IMAGE:
				OP_DBG(("REDIRECT_WITH_STATIC_IMAGE"));
				XMapWindow(m_display, m_mother_window);
				break;
		}
		m_redirect_mode = NO_REDIRECT;
	}
}

void PluginUnifiedVega::UpdateBackbuffer(OpBitmap* backbuffer)
{
	OP_NEW_DBG("PluginUnifiedVega::UpdateBackbuffer()", "plugix.vega.update");

	OP_ASSERT(backbuffer != NULL);
	OP_ASSERT((m_redirect_mode != NO_REDIRECT) && "this method should only be called in redirected mode.");
	OP_ASSERT(backbuffer->Width() == static_cast<unsigned int>(m_rect.w));
	OP_ASSERT(backbuffer->Height() == static_cast<unsigned int>(m_rect.h));

	/**
	 * If we use an off-screen window, we first get a new XImage from the
	 * window. If the user disabled opacity support, e.g. by disabling the
	 * compiz window manager, we continue with the last available image:
	 */
	if (m_redirect_mode == REDIRECT_WITH_OPACITY)
	{
		if (m_x11_window_manager->SupportsWindowOpacity())
			UpdateImage();
		else
		{	/**
			 * This may happen if e.g. the compiz window manager crashes. The
			 * result is usually that the plugin window is now visible. So hide
			 * the plugin window, restore parent and original position and set
			 * the mode to REDIRECT_WITH_STATIC_IMAGE - though we may not have a
			 * static image to display:
			 */
			OP_DBG(("opacity was disabled; switch to REDIRECT_WITH_STATIC_IMAGE"));
			XUnmapWindow(m_display, m_mother_window);
			RestorePluginParent();
			m_redirect_mode = REDIRECT_WITH_STATIC_IMAGE;
		}
	}

	if (OpStatus::IsError(CopyXImageToOpBitmap(backbuffer)))
	{
		OP_DBG(("Could not copy image to back buffer, so paint it black..."));
		/* This branch is a fallback for the fallback. We might enter here in
		 * the following situations:
		 * - we failed to create an XImage (maybe OOM),
		 * - backbuffer has not the required format (32 bpp)
		 * - XImage has a not yet supported format
		 * - changing the window-manager's opacity capability while being in
		 *   redirected mode.
		 * - setting the plugin in redirect mode before the plugin has been
		 *   started.
		 * If this branch is used for more than 1-2 seconds continuously for a
		 * somewhat normal use case, a bug should be filed. */
		UINT32 black = 0xFF000000;
		backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, NULL);
	}
}

void PluginUnifiedVega::UpdateWindow()
{
	if (m_needs_flush)
	{
		XFlush(m_display);
		m_needs_flush = false;
	}
}

OP_STATUS PluginUnifiedVega::CopyXImageToOpBitmap(OpBitmap* backbuffer)
{
	OP_NEW_DBG("PluginUnifiedVega::CopyXImageToOpBitmap", "plugix.vega.update");
	OP_DBG(("window size: %dx%d", m_rect.w, m_rect.h));

	if (!m_static_image)
	{
		OP_DBG(("PluginUnifiedVega::CopyXImageToOpBitmap: No static image available"));
		return OpStatus::ERR_NULL_POINTER;
	}

	if (/* ...XImage is compatible with OpBitmap */
		m_static_image->depth      != 24 ||
		m_static_image->red_mask   != 0x00FF0000 ||
		m_static_image->green_mask != 0x0000FF00 ||
		m_static_image->blue_mask  != 0x000000FF ||
		m_static_image->width < 0 || m_static_image->height < 0)
	{
		OP_DBG(("PluginUnifiedVega::CopyXImageToOpBitmap: Static image has unsupported format"));
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	if (m_static_image->bits_per_pixel != 32 && m_static_image->bits_per_pixel != 24)
	{
		OP_DBG(("we have an XImage with unsupported format:"));
		OP_DBG((" size: %dx%d", m_static_image->width, m_static_image->height));
		OP_DBG((" xoffset: %d", m_static_image->xoffset));
		OP_DBG((" format: %s (%d)",
				(m_static_image->format == XYBitmap ? "XYBitmap" : m_static_image->format == XYPixmap ? "XYPixmap" : m_static_image->format == ZPixmap ? "ZPixmap" : "unknown"),
				m_static_image->format));
		OP_DBG((" data %p; byte order: %s (%d)", m_static_image->data,
				(m_static_image->byte_order == LSBFirst ? "LSBFirst" : m_static_image->byte_order == MSBFirst ? "MSBFirst" : "unknown"),
				m_static_image->byte_order));
		OP_DBG((" bitmap unit: %d; bit order: %s (%d); pad: %s (%d)", m_static_image->bitmap_unit,
				(m_static_image->bitmap_bit_order == LSBFirst ? "LSBFirst" : m_static_image->bitmap_bit_order == MSBFirst ? "MSBFirst" : "unknown"), m_static_image->bitmap_bit_order,
				(m_static_image->bitmap_pad == 8 ? "8" : m_static_image->bitmap_pad == 16 ? "16" : m_static_image->bitmap_pad == 32 ? "32" : m_static_image->bitmap_pad == XYPixmap ? "XYPixmap" : m_static_image->bitmap_pad == ZPixmap ? "ZPixmap" : "other"), m_static_image->bitmap_pad));
		OP_DBG((" depth: %d; bytes per line: %d;  bits per pixel: %d",
				m_static_image->depth, m_static_image->bytes_per_line, m_static_image->bits_per_pixel));
		OP_DBG((" masks: (%p,%p,%p)", m_static_image->red_mask,
				m_static_image->green_mask, m_static_image->blue_mask));
		OP_ASSERT(!"Plugin window uses an unsupported pixel data format.");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	OP_ASSERT(backbuffer);
	OP_ASSERT(backbuffer->Width() == static_cast<unsigned int>(m_rect.w));
	OP_ASSERT(backbuffer->Height() == static_cast<unsigned int>(m_rect.h));
	OP_ASSERT(m_static_image->format == ZPixmap && "because UpdateImage() created the static image with this format.");
	OP_ASSERT(m_static_image->width == m_static_image_rect.width);
	OP_ASSERT(m_static_image->height == m_static_image_rect.height);

	uint32_t black = 0xFF000000;
	OpRect dest_rect = m_static_image_rect;
	dest_rect.IntersectWith(OpRect(0, 0, backbuffer->Width(), backbuffer->Height()));
	if (m_static_image->bits_per_pixel == 24)
	{
		// paint the top part black:
		if (dest_rect.y > 0)
		{
			OpRect box(0, 0, backbuffer->Width(), dest_rect.y);
			backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
		}

		// paint the bottom part black as well:
		if (dest_rect.Bottom() < 0 || static_cast<unsigned int>(dest_rect.Bottom()) < backbuffer->Height())
		{
			OpRect box(0, dest_rect.Bottom(), backbuffer->Width(), backbuffer->Height() - dest_rect.Bottom());
			backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
		}

		if (backbuffer->Supports(OpBitmap::SUPPORTS_POINTER) &&
			// We can use GetPointer() only if the backbuffer uses 32bpp:
			backbuffer->GetBpp() == 32)
		{	/* This format is used by, for example; Flash 10.x showing normal
			 * vector graphics animations. We need to convert each single pixel
			 * to ARGB with an alpha value of 0xFF: */

			// paint the left margin black:
			if (dest_rect.x > 0)
			{
				OpRect box(0, dest_rect.y, dest_rect.x, dest_rect.height);
				backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
			}

			// paint the right margin black:
			if (dest_rect.Right() < 0 || static_cast<unsigned int>(dest_rect.Right()) < backbuffer->Width())
			{
				OpRect box(dest_rect.Right(), dest_rect.y, backbuffer->Width(), dest_rect.height);
				backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
			}

			// copy the image
			const int height = dest_rect.Bottom();
			uint32_t* rgba_pixels = static_cast<uint32_t*>(backbuffer->GetPointer());
			for (int y = MAX(0, dest_rect.y); y < height; ++y)
			{
				for (int x = 0; x < dest_rect.width; ++x)
					rgba_pixels[dest_rect.x + x] = 0xFF000000 | XGetPixel(m_static_image, x, y);
				rgba_pixels = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(rgba_pixels) + backbuffer->GetBytesPerLine());
			}
			backbuffer->ReleasePointer();
		}
		else
		{   /* Create a local copy with the width of the backbuffer, copy the
			 * image-line into the local copy and (if necessary) add a black
			 * left and right margin: */
			uint32_t* rgba_pixels = OP_NEWA(uint32_t, backbuffer->Width());
			if (!rgba_pixels) return OpStatus::ERR_NO_MEMORY;
			// add a black margin on the left:
			for (int x = 0; x < dest_rect.x; ++x)
				rgba_pixels[x] = black;
			// and add a black margin on the right:
			for (unsigned int x = static_cast<unsigned int>(MAX(0, dest_rect.Right())); x < backbuffer->Width(); ++x)
				rgba_pixels[x] = black;

			const int height = MAX(0, dest_rect.Bottom());
			for (int y = MAX(0, dest_rect.y); y < height; ++y)
			{
				for (int x = 0; x < dest_rect.width; ++x)
					rgba_pixels[dest_rect.x + x] = 0xFF000000 | XGetPixel(m_static_image, x, y);
				backbuffer->AddLine(rgba_pixels, y);
			}
			OP_DELETEA(rgba_pixels);
		}
	}
	else  // m_static_image->bits_per_pixel == 32
	{
		// This format is used by YouTube, for example.
		OP_ASSERT(m_static_image->bytes_per_line == 4 * m_static_image_rect.width);

		if (backbuffer->Supports(OpBitmap::SUPPORTS_POINTER) &&
			// We can use GetPointer() only if the backbuffer uses 32bpp:
			backbuffer->GetBpp() == 32 &&
			m_static_image->bytes_per_line >= 0 &&
			backbuffer->GetBytesPerLine() == static_cast<unsigned int>(m_static_image->bytes_per_line) &&
			m_static_image->width >= 0 &&
			backbuffer->Width() == static_cast<unsigned int>(m_static_image->width) &&
			dest_rect.height >= 0 &&
			backbuffer->Height() == static_cast<unsigned int>(dest_rect.height) &&
			dest_rect.x == 0 && dest_rect.y == 0)
		{   // We can simply copy the whole buffer in one go:
			op_memcpy(backbuffer->GetPointer(), m_static_image->data, m_static_image->bytes_per_line * dest_rect.height);
			backbuffer->ReleasePointer();
		}
		else
		{   /* We call OpBitmap::AddLine() for each line. This is also
			 * possible, if the backbuffer does not use 32bpp internally. */

			// paint the top part black:
			if (dest_rect.y > 0)
			{
				OpRect box(0, 0, backbuffer->Width(), dest_rect.y);
				backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
			}

			// call AddLine() for each image line:
			const int height = MAX(0, dest_rect.Bottom());
			if (m_static_image->width >= 0 &&
				backbuffer->Width() <= static_cast<unsigned int>(m_static_image->width) &&
				dest_rect.x == 0)
			{	/* One line of the static image is wide enough to cover the full
				 * width of the backbuffer, so we can call AddLine() with the
				 * image data directly: */
				char* image_data = m_static_image->data;
				for (int y = MAX(0, dest_rect.y); y < height; ++y)
				{
					RETURN_IF_ERROR(backbuffer->AddLine(image_data, y));
					image_data += m_static_image->bytes_per_line;
				}
			}
			else
			{	/* One line of the static image is smaller than one line of the
				 * backbuffer, so we need to create a local copy with the width
				 * of the backbuffer, copy the image-line into the local copy
				 * and (if necessary) add a black left and right margin: */

				uint32_t* rgba_pixels = OP_NEWA(uint32_t, backbuffer->Width());
				if (!rgba_pixels) return OpStatus::ERR_NO_MEMORY;

				// add a black margin on the left:
				for (int x = 0; x < dest_rect.x; ++x)
					rgba_pixels[x] = black;

				// and add a black margin on the right:
				for (int x = MAX(0, dest_rect.Right()); x < static_cast<int>(backbuffer->Width()); ++x)
					rgba_pixels[x] = black;

				const char* image_data = m_static_image->data;
				for (int y = MAX(0, dest_rect.y); y < height; ++y)
				{
					op_memcpy(rgba_pixels + dest_rect.x, image_data, dest_rect.width * sizeof(uint32_t));
					backbuffer->AddLine(rgba_pixels, y);
					image_data += m_static_image->bytes_per_line;
				}
				OP_DELETEA(rgba_pixels);
			}

			// paint the bottom part black as well:
			if (dest_rect.Bottom() < 0 || static_cast<unsigned int>(dest_rect.Bottom()) < backbuffer->Height())
			{
				OpRect box(0, dest_rect.Bottom(), backbuffer->Width(), backbuffer->Height() - dest_rect.Bottom());
				backbuffer->SetColor(reinterpret_cast<UINT8*>(&black), FALSE, &box);
			}
		}
	}
	return OpStatus::OK;
}

void PluginUnifiedVega::RestorePluginParent()
{
	OP_NEW_DBG("VegaPluginWidget::RestorePluginParent", "plugix.vega");
	OP_ASSERT(m_redirect_mode == REDIRECT_WITH_OPACITY);
	OP_ASSERT(m_original_parent != 0);

	X11Types::Window plugin_window = m_mother_window;
	m_x11_window_manager->SetWindowOpacity(plugin_window, 1.0);
	OP_DBG(("this; %p; reparent plugin window to its original parent and original position: (%d,%d)", this, m_original_window_pos.x, m_original_window_pos.y));

	XReparentWindow(m_display, plugin_window, m_original_parent, m_original_window_pos.x, m_original_window_pos.y);
	XMoveWindow(m_display, m_mother_window, m_original_window_pos.x, m_original_window_pos.y);
	m_original_window_pos.Set(0, 0);
	m_original_parent = 0;
}

void PluginUnifiedVega::UpdateImage()
{
	OP_NEW_DBG("PluginUnifiedVega::UpdateImage()", "plugix.vega.update");

	if (m_static_image)
	{
		XDestroyImage(m_static_image);
		m_static_image = 0;
	}

	X11Types::Window plugin_window = m_mother_window;
	XWindowAttributes window_attr;
	XGetWindowAttributes(m_display, plugin_window, &window_attr);

	if (window_attr.width <= 0 || window_attr.height <= 0)
		m_static_image_rect.Empty();
	else
	{
		m_static_image_rect.Set(0, 0, window_attr.width, window_attr.height);

		if (!m_clip_rect.IsEmpty())
			m_static_image_rect.IntersectWith(m_clip_rect);

		OP_DBG(("PluginUnifiedVega::UpdateImage: Request image for window with clip-rect (%d,%d)+%dx%d",
			m_static_image_rect.x, m_static_image_rect.y, m_static_image_rect.width, m_static_image_rect.height));
		m_static_image = XGetImage(m_display, plugin_window,
		                           m_static_image_rect.x, m_static_image_rect.y,
		                           m_static_image_rect.width, m_static_image_rect.height,
		                           AllPlanes, ZPixmap);
	}

	if (!m_static_image)
	{
		OP_DBG(("PluginUnifiedVega::UpdateImage: Could not get a static image."));
		return;
	}

	OP_DBG(("got image:"));
	OP_DBG((" size: %dx%d", m_static_image->width, m_static_image->height));
	OP_DBG((" xoffset: %d", m_static_image->xoffset));
	OP_DBG((" format: %s (%d)",
			(m_static_image->format == XYBitmap ? "XYBitmap" : m_static_image->format == XYPixmap ? "XYPixmap" : m_static_image->format == ZPixmap ? "ZPixmap" : "unknown"),
			m_static_image->format));
	OP_DBG((" data %p; byte order: %s (%d)", m_static_image->data,
			(m_static_image->byte_order == LSBFirst ? "LSBFirst" : m_static_image->byte_order == MSBFirst ? "MSBFirst" : "unknown"),
			m_static_image->byte_order));
	OP_DBG((" bitmap unit: %d; bit order: %s (%d); pad: %s (%d)", m_static_image->bitmap_unit,
			(m_static_image->bitmap_bit_order == LSBFirst ? "LSBFirst" : m_static_image->bitmap_bit_order == MSBFirst ? "MSBFirst" : "unknown"), m_static_image->bitmap_bit_order,
			(m_static_image->bitmap_pad == 8 ? "8" : m_static_image->bitmap_pad == 16 ? "16" : m_static_image->bitmap_pad == 32 ? "32" : m_static_image->bitmap_pad == XYPixmap ? "XYPixmap" : m_static_image->bitmap_pad == ZPixmap ? "ZPixmap" : "other"), m_static_image->bitmap_pad));
	OP_DBG((" depth: %d; bytes per line: %d;  bits per pixel: %d",
			m_static_image->depth, m_static_image->bytes_per_line, m_static_image->bits_per_pixel));
	OP_DBG((" masks: (%p,%p,%p)", m_static_image->red_mask,
			m_static_image->green_mask, m_static_image->blue_mask));

	// set all alpha pixels to 0xFF:
	if (m_static_image->bits_per_pixel == 32 &&
		m_static_image->red_mask       == 0x00FF0000 &&
		m_static_image->green_mask     == 0x0000FF00 &&
		m_static_image->blue_mask      == 0x000000FF)
	{
		const uint32_t* data_end =
			reinterpret_cast<uint32_t*>(m_static_image->data + m_static_image->bytes_per_line * m_static_image->height);
		for (uint32_t* data = reinterpret_cast<uint32_t*>(m_static_image->data);
			 data < data_end;
			 data = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(data) + m_static_image->bytes_per_line))
			for (int pixel = 0; pixel < m_static_image->width; ++pixel)
				data[pixel] |= 0xFF000000;
	}
}

#endif  // defined(VEGA_OPPAINTER_SUPPORT) && defined(MDE_NATIVE_WINDOW_SUPPORT)
#endif // X11API && NS4P_COMPONENT_PLUGINS
