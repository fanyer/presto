/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/OpWindow.h"
#include "platforms/mac/pi/MacOpScreenInfo.h"
#include "platforms/mac/util/UTempRegion.h"

/////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScreenInfo::Create(OpScreenInfo **newObj)
{
	*newObj = new MacOpScreenInfo();
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/////////////////////////////////////////////////////////////////////////////

PixMapHandle MacOpScreenInfo::GetMainDevicePixMap()
{
	GDHandle gd = LMGetMainDevice();
	if (gd)
		return (*gd)->gdPMap;
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpScreenInfo::GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point)
{
	if (!properties)
		return OpStatus::ERR_NO_MEMORY;
	
	PixMapHandle pm = NULL;
	GDHandle device = GetMainDevice();
//	OP_ASSERT(window);

	if (!window)
	{
		pm = GetMainDevicePixMap();
	}
	else
	{
		Rect bounds;
	
		if (point)
		{
			bounds.top = point->y;
			bounds.left = point->x;
			bounds.bottom = point->y+1;
			bounds.right = point->x+1;
		}
		else
		{
			INT32 x, y;
			UINT32 w, h;
			window->GetOuterPos(&x, &y);
			window->GetOuterSize(&w, &h);
			bounds.top = y;
			bounds.left = x;
			bounds.bottom = y + h;
			bounds.right = x + w;
		}
		device = GetMaxDevice(&bounds);
		if (!device)
			device = GetMainDevice();
		pm = (*device)->gdPMap;
	}

	// Set width and height
	properties->width	= (pm ? (*pm)->bounds.right - (*pm)->bounds.left : 640);
	properties->height	= (pm ? (*pm)->bounds.bottom - (*pm)->bounds.top : 480);

//#warning "Needs some work!"
// FIXME: Needs some work!
	properties->horizontal_dpi = 96;
	properties->vertical_dpi = 96;

	properties->bits_per_pixel = (pm ? (*pm)->pixelSize : 32);
	properties->number_of_bitplanes = 1;
	
	if (pm)
	{
		Rect bounds = (*pm)->bounds;
		properties->screen_rect.x		= bounds.left;
		properties->screen_rect.y		= bounds.top;
		properties->screen_rect.width	= bounds.right - bounds.left;
		properties->screen_rect.height	= bounds.bottom - bounds.top;
		
		GetAvailableWindowPositioningBounds(device, &bounds);
		properties->workspace_rect.x		= bounds.left;
		properties->workspace_rect.y		= bounds.top;
		properties->workspace_rect.width	= bounds.right - bounds.left;
		properties->workspace_rect.height	= bounds.bottom - bounds.top;
	}
	else
	{
		properties->screen_rect.x		= properties->workspace_rect.x		= 0;
		properties->screen_rect.y		= properties->workspace_rect.y		= 0;
		properties->screen_rect.width	= properties->workspace_rect.width	= properties->width;
		properties->screen_rect.height	= properties->workspace_rect.height = properties->height;
	}

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpScreenInfo::GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point)
{
//#warning "Needs some work!"
// FIXME: Needs some work!
	*horizontal = 96;
	*vertical = 96;

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpScreenInfo::RegisterMainScreenProperties(OpScreenProperties& properties)
{
	// Not required on Mac
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

UINT32 MacOpScreenInfo::GetBitmapAllocationSize(UINT32 width, UINT32 height,
									   BOOL transparent,
									   BOOL alpha,
									   INT32 indexed)
{
//#warning "Needs some work!"
// FIXME: Needs some work!
	
	PixMapHandle pm = GetMainDevicePixMap();
	int32 bpp = (pm ? (*pm)->pixelSize : 32);
	
	return ((((width * bpp + 31) / 8) & ~3) * height);
}

/////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------
//OLD AND DEPRECATED
//---------------------------------------------------------------------


