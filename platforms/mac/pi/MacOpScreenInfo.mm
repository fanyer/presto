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
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/MachOCompatibility.h"

#define BOOL NSBOOL
#import <AppKit/NSArrayController.h>
#import <AppKit/NSScreen.h>
#import <Foundation/NSDictionary.h>
#undef BOOL

#define FAKE_RESOLUTIION
// FIXME: Just return 72 dpi. Ugly, but the CGDisplayScreenSize is horrendously slow.

/////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScreenInfo::Create(OpScreenInfo **newObj)
{
	*newObj = new MacOpScreenInfo();
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpScreenInfo::GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point)
{
	if (!properties)
		return OpStatus::ERR_NO_MEMORY;

	NSScreen *currScreen = (NSScreen *)GetScreen(window, point);
	NSScreen *menuScreen = [[NSScreen screens] objectAtIndex:0];

	// Set width and height
	properties->width	= [currScreen frame].size.width;
	properties->height	= [currScreen frame].size.height;

	// Get the screen number
	NSDictionary* screenInfo = [currScreen deviceDescription]; 
	NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"]; 

	// Convert to screen ID so the CGDirectDisplayxxx functions work
	CGDirectDisplayID displayID = (CGDirectDisplayID)[screenID longValue]; 

	// Calculate the DPI of the screen
#ifdef FAKE_RESOLUTIION
	properties->horizontal_dpi = 72;
	properties->vertical_dpi = 72;
#else
	CGSize screen_size = CGDisplayScreenSize(displayID);
	size_t height_pixels = CGDisplayPixelsHigh(displayID); 
	size_t width_pixels = CGDisplayPixelsWide(displayID);
	properties->horizontal_dpi = height_pixels * 25.4 / screen_size.height;
	properties->vertical_dpi = width_pixels * 25.4 / screen_size.width;
#endif // FAKE_RESOLUTIION

	// Get the bits per pixel
	properties->bits_per_pixel = CGDisplayBitsPerPixel(displayID);	
	properties->number_of_bitplanes = 1;

	// screen_rect is the entire window
	properties->screen_rect.x		= [currScreen frame].origin.x;
	properties->screen_rect.y		= [menuScreen frame].size.height - [currScreen frame].size.height - [currScreen frame].origin.y;
	properties->screen_rect.width	= [currScreen frame].size.width;
	properties->screen_rect.height	= [currScreen frame].size.height;

	// workspace_rect is the visible part of the window without the menu or dock
	properties->workspace_rect.x		= [currScreen visibleFrame].origin.x;
	properties->workspace_rect.y		= [menuScreen frame].size.height - [currScreen frame].size.height - [currScreen visibleFrame].origin.y;
	properties->workspace_rect.width	= [currScreen visibleFrame].size.width;
	properties->workspace_rect.height	= [currScreen visibleFrame].size.height;

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpScreenInfo::GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point)
{
#ifdef FAKE_RESOLUTIION
	*horizontal = 72;
	*vertical = 72;
#else
	NSScreen *currScreen = (NSScreen *)GetScreen(window, point);

	// Get the screen number
	NSDictionary* screenInfo = [currScreen deviceDescription]; 
	NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"]; 

	// Convert to screen ID so the CGDirectDisplayxxx functions work
	CGDirectDisplayID displayID = (CGDirectDisplayID)[screenID longValue]; 

	// Calculate the DPI of the screen
	CGSize screen_size		= CGDisplayScreenSize(displayID);
	size_t height_pixels	= CGDisplayPixelsHigh(displayID); 
	size_t width_pixels		= CGDisplayPixelsWide(displayID);
	*horizontal				= height_pixels * 25.4 / screen_size.height;
	*vertical				= width_pixels * 25.4 / screen_size.width;
#endif // FAKE_RESOLUTIION

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
	// Get the screen number
	NSDictionary* screenInfo = [[NSScreen deepestScreen] deviceDescription]; 
	NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"]; 
	
	// Convert to screen ID so the CGDirectDisplayxxx functions work
	CGDirectDisplayID displayID = (CGDirectDisplayID)[screenID longValue]; 

	int32 bpp = CGDisplayBitsPerPixel(displayID);	
	
	return ((((width * bpp + 31) / 8) & ~3) * height);
}


/////////////////////////////////////////////////////////////////////////////

void *MacOpScreenInfo::GetScreen(OpWindow* window, const OpPoint* point)
{
	NSScreen *currScreen = nil;
	
	// Find the screen the point is on
	if (point)
	{
		NSPoint pt = NSMakePoint(point->x, point->y);
		
		// Loop the screens 
		unsigned i = 0, num_screens = [[NSScreen screens] count];
		for (i = 0; i < num_screens; i++) 
		{
			NSScreen *thisScreen = [[NSScreen screens] objectAtIndex:i];
			
			if (NSPointInRect(pt, [thisScreen frame]))
			{
				currScreen = thisScreen;
				break;
			}
		}	
	}
	else if (window)
	{
		// Find the screen with the most part of the window on it
		INT32	x, y;
		UINT32	w, h;
		double	max_area = 0.0;
		
		// Make a rect for the window passed in
		window->GetOuterPos(&x, &y);
		OpPoint screen_point = ConvertToScreen(OpPoint(x,y), window, NULL);
		window->GetOuterSize(&w, &h);
		NSRect window_rect = NSMakeRect(screen_point.x, screen_point.y, w, h);
		
		// Loop the screens 
		unsigned i = 0, num_screens = [[NSScreen screens] count];
		for (i = 0; i < num_screens; i++) 
		{
			NSScreen *thisScreen = [[NSScreen screens] objectAtIndex:i];
			
			// Check if this window is on the screen
			if (NSIntersectsRect(window_rect, [thisScreen frame]))
			{
				// Find the screen with the greatest intersecting area of the window
				NSRect intersection_rect = NSIntersectionRect(window_rect, [thisScreen frame]);
				double curr_area = intersection_rect.size.width * intersection_rect.size.height;
				if (curr_area > max_area)
				{
					max_area = curr_area;
					currScreen = thisScreen;
				}
			}
		}
	}

	// If no window was found go for the main window
	if (currScreen == nil)
		currScreen = [NSScreen mainScreen];

	return currScreen;
}
/////////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------
//OLD AND DEPRECATED
//---------------------------------------------------------------------


