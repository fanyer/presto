/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the global object for the memtools module
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"
#include "modules/memtools/memtools_module.h"
#include "modules/memtools/memtools_codeloc.h"

void MemtoolsModule::InitL(const OperaInitInfo&)
{
#ifdef MEMTOOLS_ENABLE_CODELOC
  OpCodeLocationManager::Init();
#endif
}

void MemtoolsModule::Destroy(void)
{
}
