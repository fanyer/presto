/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

LayoutPool::LayoutPool(unsigned short size) :
	m_pool(NULL),
	m_next(0),
	m_size(size)
{
}

LayoutPool::~LayoutPool()
{
	Destroy();
}

void
LayoutPool::ConstructL()
{
	m_pool = OP_NEWA_L(void*, m_size);
}

void
LayoutPool::Destroy()
{
	/* Destroy() may be called several times if the destruction is
	   done in mutiple steps (first Destroy is called and later the
	   instance is destructed).  The reason for a multi-step
	   destruction is that the construction can be made in steps.  If
	   those steps is made with different allocators, destruction
	   needs to be split into several steps also. */

	OP_DELETEA(m_pool);
	m_pool = NULL;
}

void*
LayoutPool::New(size_t nbytes)
{
	if (m_next == 0)
		return op_malloc(nbytes);
	else
		return m_pool[--m_next];
}

void
LayoutPool::Delete(void* p)
{
	if (p == 0)
		return;

	if (m_next == m_size)
		op_free(p);
	else
		m_pool[m_next++] = p;
}

void
LayoutPool::Clean()
{
	while (m_next > 0)
		op_free(m_pool[--m_next]);
}
