/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __COMMON_OPPAINTER_H__
#define __COMMON_OPPAINTER_H__

#include "modules/pi/OpPainter.h"

/**
 * This is an abstract class that should be inherited by platform
 * implementations of OpPainter (like QtOpPainter). Its purpose is to make
 * it possible to use several implementations of OpPainter simultaneously.
 * This class is tailored for use with QtOpPainter and QtX11OpPainter.
 */
class CommonOpPainter : public OpPainter
{
public:
	enum PainterType {
		QT_OPPAINTER,
		X11_OPPAINTER
	};

	enum DeviceType
	{
		SCREEN,
		PRINTER
	};

	virtual PainterType GetPainterType() = 0;

#ifdef _PRINT_SUPPORT_
	virtual DeviceType GetDeviceType() = 0;
#endif // _PRINT_SUPPORT_

	virtual void End() = 0;
	virtual void UseDoublebuffer(BOOL on) = 0;
	virtual class UnixFontSwitcher *GetFontSwitcher() = 0;
};

#endif // __COMMON_OPPAINTER_H__
