/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** $Id$
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPSCREENINFO_H
#define WINDOWSOPSCREENINFO_H

#include "modules/pi/OpScreenInfo.h"

class WindowsOpScreenInfo : public OpScreenInfo
{
public:
	WindowsOpScreenInfo();
	OP_STATUS Init();
	~WindowsOpScreenInfo();
	

	/** Get properties from a screen based on what window and what pixel */
	virtual OP_STATUS	GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point = NULL);

	/** Get the DPI propertis only (superfast)*/
	virtual OP_STATUS	GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point = NULL);

	/** Makes it simple to register 'main-screen' fallback properties */
	virtual OP_STATUS	RegisterMainScreenProperties(OpScreenProperties& properties);

	
	HDC					GetScreenDC()
	{
		return m_screenDC;
	}


	virtual UINT32		GetBitmapAllocationSize(UINT32 width, UINT32 height,
										   BOOL transparent,
										   BOOL alpha,
										   INT32 indexed);
	void				OnSettingsChanged();

private:
	HDC					m_screenDC;
};

#endif // WINDOWSOPSCREENINFO_H

