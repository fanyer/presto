/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_IDLE_IDLE_MODULE_H
#define MODULES_IDLE_IDLE_MODULE_H

#include "modules/hardcore/opera/module.h"

// Forward declarations.
class OpIdleDetector;

class IdleModule : public OperaModule
{
public:
	IdleModule();

	void InitL(const OperaInitInfo &info);
	void Destroy();

	OpIdleDetector *idle_detector;
};

#define g_idle_detector g_opera->idle_module.idle_detector

#define IDLE_MODULE_REQUIRED

#endif // MODULES_IDLE_IDLE_MODULE_H
