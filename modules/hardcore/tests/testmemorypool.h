/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef TESTMEMORYPOOL_H
#define TESTMEMORYPOOL_H

#include "modules/hardcore/component/OpMemoryPool.h"

/**
 * Declares an OpMemoryPool which is used for various selftests. The
 * pool-instance is member of the MemoryManager class and the
 * OP_USE_MEMORY_POOL_IMPL is added to mem/mem_man.cpp.
 */
class TestPool
{
public:
	class TestPoolElement
	{
		OP_USE_MEMORY_POOL_DECL;

	private:
		const char* m_name;

	public:
		TestPoolElement(const char* name) : m_name(name) {}
		const char* Name() const { return m_name; }
	};

	typedef OpMemoryPool<sizeof(TestPoolElement)> PoolType;
};

#endif // TESTMEMORYPOOL_H
