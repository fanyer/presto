/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOPOPUIINFO_H
#define DESKTOPOPUIINFO_H

#include "modules/pi/ui/OpUiInfo.h"

#define g_desktop_op_ui_info static_cast<DesktopOpUiInfo*>(g_op_ui_info)

/**
 * DesktopOpUiInfo extends the OpUiInfo porting interface
 * with functionality specific for the desktop projects.
 */
class DesktopOpUiInfo : public OpUiInfo
{
public:
	/**
	 * Returns the color a non active persona window is dimmed with
	 * Format: 0xAABBGGRR
	 */
	virtual UINT32 GetPersonaDimColor() const
	{
		return OP_RGBA(255, 255, 255, 76);
	}
	/**
	 * Returns whether touch is available as an input method
	 */
	virtual bool IsTouchInputAvailable() { return false; }

	/**
	 * Returns whether touch events support is wanted on web pages.
	 */
	virtual bool IsTouchEventSupportWanted() { return false; }

	/**
	 * Returns whether we want a touch optimized UI
	 */
	virtual bool IsTouchUIWanted() { return false; }
};

#endif // DESKTOPOPUIINFO_H
