/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11_opscreeninfo.h"

#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"

OP_STATUS X11OpScreenInfo::GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point/* = NULL*/)
{
	int screen = g_x11->GetDefaultScreen();

	if (window)
	{
		if (window->GetStyle() == OpWindow::STYLE_BITMAP_WINDOW)
		{
			// FIXME maybe default is ok? looks like this is happening on win and mac
		}
		else
		{
			// Get screen from root window. That window must be an X11OpWindow
			OpWindow* w = window->GetRootWindow();
			if (!w) 
				w = window;
			if (((X11OpWindow*)w)->GetWidget())
				screen = ((X11OpWindow*)w)->GetWidget()->GetScreen();
		}
	}

	properties->screen_rect.x = 0;
	properties->screen_rect.y = 0;
	properties->screen_rect.width = g_x11->GetScreenWidth(screen);
	properties->screen_rect.height = g_x11->GetScreenHeight(screen);

	if (!g_x11->GetWorkSpaceRect(screen, properties->workspace_rect))
		properties->workspace_rect = properties->screen_rect;

	// If there is more than one physical screen that makes up a continuous deskop
	// then we want to limit the reported screen and workspace rectangles to the
	// rectangle of the physical screen that contains the point coordinate.
	// This allows popups, tooltips, thumbnails and notifiers to when shown within
	// one screen and not partly on two.
	if (point)
	{
		OpRect rect = g_x11->GetWorkRect(screen, *point);
		if (!rect.IsEmpty())
		{
			properties->screen_rect.IntersectWith(rect);
			properties->workspace_rect.IntersectWith(rect);
		}
	}

	properties->width = g_x11->GetScreenWidth(screen);
	properties->height = g_x11->GetScreenHeight(screen);
	properties->horizontal_dpi = g_x11->GetOverridableDpiX(screen);
	properties->vertical_dpi = g_x11->GetOverridableDpiY(screen);
	properties->bits_per_pixel = g_x11->GetX11Visual(screen).depth;
	properties->number_of_bitplanes = 1;

	return OpStatus::OK;
}

OP_STATUS X11OpScreenInfo::GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point/* = NULL*/)
{
	int screen = g_x11->GetDefaultScreen();

	// We might want the toplevel window and its x11 widget
	if (window && ((X11OpWindow*)window)->GetWidget())
	{
		screen = ((X11OpWindow*)window)->GetWidget()->GetScreen();
	}

	if (horizontal)
		*horizontal = g_x11->GetOverridableDpiX(screen);
	if (vertical)
		*vertical = g_x11->GetOverridableDpiY(screen);

	return OpStatus::OK;
}

OP_STATUS X11OpScreenInfo::RegisterMainScreenProperties(OpScreenProperties& properties)
{
	return OpStatus::OK;
}



UINT32 X11OpScreenInfo::GetBitmapAllocationSize(UINT32 width, UINT32 height,
												BOOL transparent, BOOL alpha,
												INT32 indexed)
{
	return width*height*4;
}

