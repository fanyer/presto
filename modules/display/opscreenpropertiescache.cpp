/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/opscreenpropertiescache.h"
#include "modules/pi/OpScreenInfo.h"

OpScreenPropertiesCache::OpScreenPropertiesCache()
	: cachedForWindow(NULL)
	, dirty(TRUE) //we dont have anything yet!
{	
}

OpScreenProperties* OpScreenPropertiesCache::getProperties(OpWindow* window, OpPoint* point)
{
	if(dirty || window != cachedForWindow || point){
		//if window have changed, disconenct from old
		cachedForWindow = window;
		//get properties to cache
		g_op_screen_info->GetProperties(&cachedProperties, cachedForWindow, point);
		dirty = FALSE;
	}
	//return cached properties
	return &cachedProperties;
}

UINT32 OpScreenPropertiesCache::getHorizontalDpi(OpWindow* window, OpPoint* point)
{
#ifdef STATIC_GLOBAL_SCREEN_DPI
	return STATIC_GLOBAL_SCREEN_DPI_VALUE;
#else
	return getProperties(window, point)->horizontal_dpi;
#endif
}

UINT32 OpScreenPropertiesCache::getVerticalDpi(OpWindow* window, OpPoint* point)
{
#ifdef STATIC_GLOBAL_SCREEN_DPI
	return STATIC_GLOBAL_SCREEN_DPI_VALUE;
#else
	return getProperties(window, point)->vertical_dpi;
#endif
}

void OpScreenPropertiesCache::markPropertiesAsDirty()
{
	dirty = TRUE;
}
