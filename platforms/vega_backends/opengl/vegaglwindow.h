/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLWINDOW_H
#define VEGAGLWINDOW_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vega3ddevice.h"

class VEGAGlWindow : public VEGA3dWindow
{
public:
	virtual ~VEGAGlWindow() {}

	virtual VEGA3dFramebufferObject* getWindowFBO() = 0;
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLWINDOW_H
