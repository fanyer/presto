/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_OPSCREENINFO_H
#define MDE_OPSCREENINFO_H

#include "modules/pi/OpScreenInfo.h"

class MDE_OpScreenInfo : public OpScreenInfo
{
protected:
	OpScreenProperties m_mainScreenProperties;
public:
	MDE_OpScreenInfo();
	virtual ~MDE_OpScreenInfo() {}
	
	// <OpScreenInfo>

	/** Get properties from a screen based on what window and what pixel */
	virtual OP_STATUS GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point = NULL);

	/** Get the DPI propertis only (superfast)*/
	virtual OP_STATUS GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point = NULL);

	/** Makes it simple to register 'main-screen' fallback properties */
	virtual OP_STATUS RegisterMainScreenProperties(OpScreenProperties& properties);

	/** Returns the amount of memory a bitmap of the specified type would need */
	virtual UINT32 GetBitmapAllocationSize(UINT32 width,
										   UINT32 height,
										   BOOL transparent,
										   BOOL alpha,
										   INT32 indexed);

	// </OpScreenInfo>
protected:
	
	/** Helper method that should be the same on all platforms 
	 * Figures out how many bits per pixel your screen (MDE_)format has*/
	static UINT8 GetBitsPerPixelFromMDEFormat(MDE_FORMAT format);
};

#endif // MDE_OPSCREENINFO_H
