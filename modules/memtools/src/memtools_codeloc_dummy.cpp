/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 * \brief Implement dummy OpCodeLocation and OpCodeLocationManager
 *
 * A dummy implementation of platform code that is needed to support the
 * \c OpCodeLocation and \c OpCodeLocationManager classes.
 *
 * This stub implementation was originally provided to make the module
 * compile, even when the OpCodeLocation functionality was enabled for
 * platforms other than Linux (where this functionality was first
 * supprted).
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMTOOLS_ENABLE_CODELOC

#define MEMTOOLS_CODELOC_NEEDS_DUMMY

// Don't provide dummy implementation if this is configured explicitely:
#if defined(MEMTOOLS_NO_DEFAULT_CODELOC)
#  undef MEMTOOLS_CODELOC_NEEDS_DUMMY
#endif

// Don't provide a dummy for UNIX/LINGOGI - which has a proper default
#if defined(UNIX) || defined(LINGOGI)
#  undef MEMTOOLS_CODELOC_NEEDS_DUMMY
#endif

#ifdef MEMTOOLS_CODELOC_NEEDS_DUMMY

#include "modules/memtools/memtools_codeloc.h"

class DummyCodeLocationManager : public OpCodeLocationManager
{
public:
	DummyCodeLocationManager(void);
	virtual ~DummyCodeLocationManager(void);

	static void* operator new(size_t size) { return op_debug_malloc(size); }
	static void  operator delete(void* ptr) { op_debug_free(ptr); }

protected:
	virtual OP_STATUS initialize(void);
	virtual OpCodeLocation* create_location(UINTPTR addr);
};

OpCodeLocationManager* OpCodeLocationManager::create(void)
{
	return new DummyCodeLocationManager();
}

DummyCodeLocationManager::DummyCodeLocationManager(void)
{
}

DummyCodeLocationManager::~DummyCodeLocationManager(void)
{
}

OP_STATUS DummyCodeLocationManager::initialize(void)
{
	return OpStatus::OK;
}

OpCodeLocation* DummyCodeLocationManager::create_location(UINTPTR addr)
{
	OpCodeLocation* loc = new OpCodeLocation(addr);
	loc->SetFunction("??");
	loc->SetFileLine("??:0");
	return loc;
}

#endif // MEMTOOLS_CODELOC_NEEDS_DUMMY

#endif // MEMTOOLS_ENABLE_CODELOC
