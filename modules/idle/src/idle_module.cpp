/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/idle/idle_module.h"
#include "modules/idle/idle_detector.h"

IdleModule::IdleModule()
	: idle_detector(NULL)
{
}

void
IdleModule::InitL(const OperaInitInfo& info)
{
	idle_detector = OP_NEW_L(OpIdleDetector, ());
}

void
IdleModule::Destroy()
{
	OP_DELETE(idle_detector);
	idle_detector = NULL;
}
