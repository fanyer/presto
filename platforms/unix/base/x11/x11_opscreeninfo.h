/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPSCREENINFO_H_
#define __X11_OPSCREENINFO_H_

#include "modules/pi/OpScreenInfo.h"

class X11OpScreenInfo : public OpScreenInfo
{
public:

	OP_STATUS	GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point/* = NULL*/);
	OP_STATUS	GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point/* = NULL*/);
	OP_STATUS	RegisterMainScreenProperties(OpScreenProperties& properties);
	UINT32		GetBitmapAllocationSize(UINT32 width, UINT32 height,
										BOOL transparent, BOOL alpha,
										INT32 indexed);
};

#endif // __X11_OPSCREENINFO_H_
