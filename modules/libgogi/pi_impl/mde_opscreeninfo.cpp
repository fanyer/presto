/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/mde.h"

#include "modules/libgogi/pi_impl/mde_opscreeninfo.h"

#include "modules/libgogi/pi_impl/mde_opbitmap.h"
#include "modules/libgogi/pi_impl/mde_generic_screen.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#endif

MDE_OpScreenInfo::MDE_OpScreenInfo(){
	//Initialize main screen properties
	//This should however:
	//#1: Not be used
	//#2: Set by platforms trough RegisterMainScreenProperties
	m_mainScreenProperties.width = 640;
	m_mainScreenProperties.height = 480;
#ifdef STATIC_GLOBAL_SCREEN_DPI
	m_mainScreenProperties.vertical_dpi = STATIC_GLOBAL_SCREEN_DPI_VALUE;
	m_mainScreenProperties.horizontal_dpi = STATIC_GLOBAL_SCREEN_DPI_VALUE;
#else //STATIC_GLOBAL_SCREEN_DPI
	m_mainScreenProperties.vertical_dpi = 96;
	m_mainScreenProperties.horizontal_dpi = 96;
#endif //STATIC_GLOBAL_SCREEN_DPI
	m_mainScreenProperties.bits_per_pixel = 32;
	m_mainScreenProperties.number_of_bitplanes = 1;
	m_mainScreenProperties.screen_rect.Set(0,0,m_mainScreenProperties.width,m_mainScreenProperties.height);
	m_mainScreenProperties.workspace_rect.Set(0,0,m_mainScreenProperties.width,m_mainScreenProperties.height);
}

OP_STATUS MDE_OpScreenInfo::GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point)
{
	/*
		SHOULD BE IMPLEMENTED BY PLATFORM.

		The old g_mde_screen is faulty since we often have multiple screens,
		and adaptive zoom also uses a second "screen".
		since "screen"/"mainscreen" etc... is so platform dependant, and
		since no "screen" element is part of PI, this method must be implemented
		by platform

		Simple implementation: 
			Call RegisterMainScreenProperties at startup when main-screen is
			created, then always return this information.

		Better implementation:
			Decide upon what screen we are using based on window and point, and
			then populate 'properties' with this information.

		Best implementation:
			Same as better, but with no fallback to m_mainScreenProperties,
			will however give problems with selftests without context etc...
	*/

	OP_ASSERT(FALSE);
	//Return main screen information, this should
	//be avoided, but is sometimes nesesary
	op_memcpy(properties, &m_mainScreenProperties, sizeof(OpScreenProperties));
	return OpStatus::OK;
}

/** Get the DPI propertis only (superfast)*/
OP_STATUS MDE_OpScreenInfo::GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point){
#ifdef STATIC_GLOBAL_SCREEN_DPI

	*horizontal = STATIC_GLOBAL_SCREEN_DPI_VALUE;
	*vertical = STATIC_GLOBAL_SCREEN_DPI_VALUE;
	return OpStatus::OK;

#else //STATIC_GLOBAL_SCREEN_DPI

	OP_ASSERT(FALSE);
	//Return main screen information, this should
	//be avoided, but is sometimes nesesary
	*horizontal = m_mainScreenProperties.horizontal_dpi;
	*vertical = m_mainScreenProperties.vertical_dpi;
	return OpStatus::OK;

#endif //STATIC_GLOBAL_SCREEN_DPI
}

/** Makes it simple to register 'main-screen' fallback properties */
OP_STATUS MDE_OpScreenInfo::RegisterMainScreenProperties(OpScreenProperties& properties){
	op_memcpy(&m_mainScreenProperties, &properties, sizeof(OpScreenProperties));
	return OpStatus::OK;//not much that can go wrong here :-)
}

UINT32 MDE_OpScreenInfo::GetBitmapAllocationSize(UINT32 width, UINT32 height,
												  BOOL transparent, BOOL alpha, INT32 indexed)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	return VEGAOpBitmap::GetAllocationSize(width, height, transparent, alpha, indexed);
#else // !VEGA_OPPAINTER_SUPPORT
	// FIXME: this is most likely not correct..
	int memsize = width * height;
	if (!indexed)
	{
#ifdef USE_16BPP_OPBITMAP
		if (!transparent && !alpha)
			memsize *= 2;
		else
#endif // USE_16BPP_OPBITMAP
			memsize *= 4;
	}
	return sizeof(MDE_OpBitmap)+sizeof(MDE_BUFFER)+memsize;
	//or+width*height+sizeof(palette)
#endif // !VEGA_OPPAINTER_SUPPORT
}

/* static */
UINT8 MDE_OpScreenInfo::GetBitsPerPixelFromMDEFormat(MDE_FORMAT format){
	return MDE_GetBytesPerPixel(format) * 8;
}
