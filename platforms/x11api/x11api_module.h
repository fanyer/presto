/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLATFORMS_X11API_MODULE_H_
#define PLATFORMS_X11API_MODULE_H_

#ifdef X11API
#define X11API_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"

class X11apiModule : public OperaModule
{
public:
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
};

#endif  // X11API
#endif  // PLATFORMS_X11API_MODULE_H_
