/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DEBUG_ENABLE_OPTRACER

#include "modules/debug/debug_tracer.h"
#include "modules/memtools/memtools_callstack.h"

// Force OPTRACER_CALLSTACK_SIZE to 0 if callstack support is unavailable
#ifndef OPGETCALLSTACK
#undef  OPTRACER_CALLSTACK_SIZE
#define OPTRACER_CALLSTACK_SIZE 0
#endif

// The same goes for the callstack database, which we rely on
#ifndef MEMTOOLS_CALLSTACK_MANAGER
#undef  OPTRACER_CALLSTACK_SIZE
#define OPTRACER_CALLSTACK_SIZE 0
#endif

//
// Use a wrapper function since OPGETCALLSTACK doesn't work from within
// constructors and destructors on g++, for some reason.
//
static UINT32 GetCallStackID(void)
{
#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE + 1];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE + 1);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE + 1);
	cs = OpCallStackManager::GetCallStack(stack + 1, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	return cs->GetID();
#else
	return 0;
#endif
}

OpTracer::OpTracer(const char* description, OpTracerType type)
	: value(0)
	, type(type)
{
	UINT32 cs_id = GetCallStackID();
	tracer_id = g_dbg_tracer_id++;
	log_printf("TRC: 0x%x Tracer 0x%x created: %s\n",
			   cs_id, tracer_id, description);

	if ( type == OPTRACER_SINGLE )
		InternalSubscribe(cs_id, "");
}

OpTracer::~OpTracer(void)
{
	UINT32 cs_id = GetCallStackID();

	if ( type == OPTRACER_SINGLE )
		InternalUnsubscribe(cs_id, "");

	log_printf("TRC: 0x%x Tracer 0x%x deleted: value=%d\n",
			   cs_id, tracer_id, value);
}

void OpTracer::Subscribe(void)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	InternalSubscribe(cs_id, "");
}

void OpTracer::Subscribe(int handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	char integer[32]; // ARRAY OK 2008-09-22 mortenro
	op_sprintf(integer, "%d", handle);
	InternalSubscribe(cs_id, integer);
}

void OpTracer::Subscribe(const void* handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	char pointer[32]; // ARRAY OK 2008-09-22 mortenro
	op_sprintf(pointer, "%p", handle);
	InternalSubscribe(cs_id, pointer);
}

void OpTracer::Subscribe(const char* handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	InternalSubscribe(cs_id, handle);
}

void OpTracer::Unsubscribe(void)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	InternalUnsubscribe(cs_id, "");
}

void OpTracer::Unsubscribe(int handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	char integer[32]; // ARRAY OK 2008-09-22 mortenro
	op_sprintf(integer, "%d", handle);
	InternalUnsubscribe(cs_id, integer);
}

void OpTracer::Unsubscribe(const void* handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	char pointer[32]; // ARRAY OK 2008-09-22 mortenro
	op_sprintf(pointer, "%p", handle);
	InternalUnsubscribe(cs_id, pointer);
}

void OpTracer::Unsubscribe(const char* handle)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_REFCOUNT);

	InternalUnsubscribe(cs_id, handle);
}

void OpTracer::InternalSubscribe(UINT32 cs_id, const char* handle)
{
	value++;
	log_printf("TRC: 0x%x Tracer 0x%x subscribe: ref=%d %s\n",
			   cs_id, tracer_id, value, handle);
}

void OpTracer::InternalUnsubscribe(UINT32 cs_id, const char* handle)
{
	value--;
	log_printf("TRC: 0x%x Tracer 0x%x unregistered: ref=%d %s\n",
			   cs_id, tracer_id, value, handle);
}

void OpTracer::AddValue(int add)
{
	UINT32 cs_id = 0;

#if OPTRACER_CALLSTACK_SIZE > 0
	OpCallStack* cs;
	UINTPTR stack[OPTRACER_CALLSTACK_SIZE];

	OPGETCALLSTACK(OPTRACER_CALLSTACK_SIZE);
	OPCOPYCALLSTACK(stack, OPTRACER_CALLSTACK_SIZE);
	cs = OpCallStackManager::GetCallStack(stack, OPTRACER_CALLSTACK_SIZE);
	cs->AnnounceCallStack();
	cs_id = cs->GetID();
#endif

	OP_ASSERT(type == OPTRACER_VALUE);

	value += add;

	log_printf("TRC: 0x%x Tracer 0x%x value added: value=%d (%s%d)\n",
			   cs_id, tracer_id, value, add >= 0 ? "+" : "", (int)add);
}

#endif // DEBUG_ENABLE_OPTRACER
