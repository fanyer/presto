/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef X11_DEBUG_H
#define X11_DEBUG_H

#ifdef _DEBUG

#include "platforms/utilix/x11types.h"

/** @brief Functions that are useful during debugging
 */
namespace X11Debug
{
	/** Print details of an event to stdout.
	 */
	void DumpEvent(XEvent* event);
};

#endif // _DEBUG

#endif // X11_DEBUG_H
