/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __COMMON_OPBITMAP_H__
#define __COMMON_OPBITMAP_H__

#include "modules/pi/OpBitmap.h"

/**
 * This is an abstract class that should be inherited by platform
 * implementations of OpBitmap (like QtOpBitmap). Its purpose is to make
 * it possible to use several implementations of OpBitmap simultaneously.
 * This class is tailored for use with QtOpBitmap and QtX11OpBitmap.
 */
class CommonOpBitmap : public OpBitmap
{
public:
	enum BitmapType {
		QT_OPBITMAP,
		X11_OPBITMAP
	};

	virtual BitmapType Type() = 0;
};

#endif // __COMMON_OPBITMAP_H__
