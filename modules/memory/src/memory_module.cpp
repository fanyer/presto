/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the global object for the memory module
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#include "modules/memory/memory_module.h"
#include "modules/memory/src/memory_debug.h"
#include "modules/memory/src/memory_memguard.h"

MemoryModule::MemoryModule(void)
{
#ifdef USE_POOLING_MALLOC
	OpMemoryStateInit();
#endif // USE_POOLING_MALLOC
}

MemoryModule::~MemoryModule(void)
{
}

void MemoryModule::InitL(const OperaInitInfo&)
{
}

void MemoryModule::Destroy(void)
{
}
