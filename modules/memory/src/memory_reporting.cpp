/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_reporting.h"

OpMemReporting::OpMemReporting(void) :
	OpMemEventListener(MEMORY_EVENTCLASS_ERRORS_MASK
					   + MEMORY_EVENTCLASS_OOM
#ifdef MEMORY_LOG_ALLOCATIONS
					   + MEMORY_EVENTCLASS_ALLOCATE
					   + MEMORY_EVENTCLASS_FREE
#endif
					   )
{
	event_count = 0;
	suppressed_event_count = 0;
	memory_max_error_count = MEMORY_MAX_ERROR_COUNT;
}

extern "C" void op_mem_error(void)
{
	// Place a breakpoint here to debug memory problems.
#ifdef MEMORY_ASSERT_ON_ERROR
	OP_ASSERT(!"Memory error");
#endif // MEMORY_ASSERT_ON_ERROR
}

//
// Fill buf with a maximum of 6 characters
//
void OpMemReporting::EventKeywords(char* buf, OpMemEvent* ev)
{
	buf[0] = 0;

#ifdef USE_POOLING_MALLOC
	switch ( ev->mgi->bits.flags & MEMORY_FLAG_ALLOCATED_MASK )
	{
	case MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD:
		op_strcat(buf, " SMOD");
		break;

	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOT:
		op_strcat(buf, " SMOT");
		break;

	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOP:
		op_strcat(buf, " SMOP");
		break;
	}
#endif
}


#ifdef MEMORY_LOG_ALLOCATIONS

void OpMemReporting::LogAllocate(OpMemEvent* event)
{
	char keywords[128]; // ARRAY OK 2008-08-16 mortenro
	EventKeywords(keywords, event);

	if ( event->realloc_mgi != 0 )
	{
#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0
		OpCallStack* stack;
		UINT32 id = 0;

		stack = event->ai->GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH);

		if ( stack != 0 )
		{
			id = stack->GetID();
			stack->AnnounceCallStack();
		}

		log_printf("MEM: 0x%x realloc(%p,%d) => %p%s\n", id,
				   event->realloc_mgi->GetAddr(), event->mgi->GetSize(),
				   event->mgi->GetAddr(), keywords);
#else
		log_printf("MEM: %p realloc(%p,%d) => %p%s\n",
				   *event->ai->GetStack(),
				   event->realloc_mgi->GetAddr(), event->mgi->GetSize(),
				   event->mgi->GetAddr(), keywords);
#endif
	}
	else
	{
		const char* pref = " {";
		const char* obj_name = event->mgi->GetObject();
		const char* postf = "}";
		const char* arr = "";

		if ( event->mgi->IsArray() )
			arr = "[]";

		if ( obj_name == 0 )
			pref = obj_name = arr = postf = "";

#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0
		OpCallStack* stack;
		UINT32 id = 0;

		stack = event->ai->GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH);

		if ( stack != 0 )
		{
			id = stack->GetID();
			stack->AnnounceCallStack();
		}

		log_printf("MEM: 0x%x malloc(%d) => %p%s%s%s%s%s\n", id,
				   event->mgi->GetSize(), event->mgi->GetAddr(), 
				   keywords, pref, obj_name, arr, postf);
#else
		log_printf("MEM: %p malloc(%d) => %p%s%s%s%s%s\n",
				   *event->ai->GetStack(), event->mgi->GetSize(),
				   event->mgi->GetAddr(), keywords,
				   pref, obj_name, arr, postf);
#endif
	}
}

void OpMemReporting::LogFree(OpMemEvent* event)
{
#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0
	OpCallStack* stack;
	UINT32 id = 0;

	stack = event->ai->GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH);

	if ( stack != 0 )
	{
		id = stack->GetID();
		stack->AnnounceCallStack();
	}

	log_printf("MEM: 0x%x free(%p)\n", id, event->mgi->GetAddr());
#else
	log_printf("MEM: %p free(%p)\n", *event->ai->GetStack(),
			   event->mgi->GetAddr());
#endif
}

#endif // MEMORY_LOG_ALLOCATIONS

void OpMemReporting::MemoryEvent(OpMemEvent* event)
{
	BOOL should_report = TRUE;
	BOOL do_report = FALSE;

	if ( memory_max_error_count != 0 && event_count >= memory_max_error_count )
		should_report = FALSE;

	switch ( event->eventclass )
	{

#ifdef MEMORY_LOG_ALLOCATIONS

	case MEMORY_EVENTCLASS_ALLOCATE:
		LogAllocate(event);
		return;

	case MEMORY_EVENTCLASS_FREE:
		LogFree(event);
		return;

#endif // MEMORY_LOG_ALLOCATIONS

	case MEMORY_EVENTCLASS_OOM:
		if ( should_report )
		{
			dbg_printf("MEM: %p OOM when trying to allocate %z bytes\n",
					   *event->ai->GetStack(), event->oom_size);
			do_report = TRUE;
		}
		break;

	case MEMORY_EVENTCLASS_CORRUPT_BELOW:
		dbg_printf("MEM: Guardbytes before allocation corrupted\n");
		dbg_printf("MEM: Allocation at %p, size %d\n",
				   (void*)event->mgi->GetPhysAddr(), (int)event->mgi->size);
		do_report = TRUE;
		break;

	case MEMORY_EVENTCLASS_CORRUPT_ABOVE:
		dbg_printf("MEM: Guardbytes after allocation corrupted\n");
		dbg_printf("MEM: Allocation at %p, size %d\n",
				   (void*)event->mgi->GetPhysAddr(), (int)event->mgi->size);
		do_report = TRUE;
		break;

	case MEMORY_EVENTCLASS_CORRUPT_EXTERN:
		dbg_printf("MEM: External guardbytes corrupted\n");
		dbg_printf("MEM: Allocation at %p, size %d\n",
				   (void*)event->mgi->GetPhysAddr(), (int)event->mgi->size);
		do_report = TRUE;
		break;

	case MEMORY_EVENTCLASS_DOUBLEFREE:
		if ( should_report )
		{
			dbg_printf("MEM: Double free operation performed\n");
			do_report = TRUE;
		}
		break;

	case MEMORY_EVENTCLASS_MISMATCH:
		if ( should_report )
		{
			dbg_printf("MEM: Mismatched new/new[]/delete/delete[]/malloc/free\n");
			do_report = TRUE;
		}
		break;

	case MEMORY_EVENTCLASS_LATEWRITE:
		dbg_printf("MEM: Object/data was modified after it was released\n");
		do_report = TRUE;
		break;

	case MEMORY_EVENTCLASS_LEAKED:
		if ( should_report )
		{
			dbg_printf("MEM: Live allocation: %u bytes\n",
					   (unsigned int)event->mgi->size);
			do_report = TRUE;
		}
		break;

	default:
		OP_ASSERT(!"Unexpected memory report event");
		do_report = TRUE;
		break;
	}

	if ( event->ai != 0 && do_report )
	{
		OpSrcSite* site = OpSrcSite::GetSite(event->ai->GetSiteId());
		dbg_printf("MEM: Detected at this location: %s:%d\n",
				   site->GetFile(), site->GetLine());

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
		g_mem_guard->ShowCallStack(event->ai->GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH));
#  else
		g_mem_guard->ShowCallStack(event->ai->GetStack(),
								   MEMORY_ALLOCINFO_STACK_DEPTH);
#  endif
#endif
	}

	if ( event->mgi != 0 && do_report )
	{
		const char* obj_name = event->mgi->GetObject();
		if ( obj_name != 0 )
		{
			const char* arr = "";
			if ( event->mgi->IsArray() )
				arr = "[]";
			dbg_printf("MEM: Object: %s%s\n", obj_name, arr);
		}

		dbg_printf("MEM: Address: %x\n", g_mem_guard->InfoToAddr(event->mgi));
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
		dbg_printf("MEM: Tag: %d\n", event->mgi->tag);
#endif // MEMORY_FAIL_ALLOCATION_ONCE

#if MEMORY_KEEP_ALLOCSITE || MEMORY_KEEP_ALLOCSTACK > 0
		dbg_printf("MEM: Allocated at:");
#endif

#if MEMORY_KEEP_ALLOCSITE
		OpSrcSite* as = OpSrcSite::GetSite(event->mgi->allocsiteid);
		dbg_printf(" %s:%d\n", as->GetFile(), as->GetLine());
#else
		dbg_printf("\n");
#endif

#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
		g_mem_guard->ShowCallStack(event->mgi->allocstack_new);
#  else
		g_mem_guard->ShowCallStack(event->mgi->allocstack,
								   MEMORY_KEEP_ALLOCSTACK);
#  endif
#endif

#if MEMORY_KEEP_REALLOCSITE
		if ( event->mgi->bits.flags & MEMORY_FLAG_IS_REALLOCATED )
		{
			OpSrcSite* rs = OpSrcSite::GetSite(event->mgi->reallocsiteid);
			dbg_printf("MEM: Reallocated last at: %s:%d\n",
					   rs->GetFile(), rs->GetLine());
		}
#endif

		UINT32 released = event->mgi->bits.flags & MEMORY_FLAG_RELEASED_MASK;
		if ( released && do_report )
		{
			dbg_printf("MEM: %s at:",
					   released == MEMORY_FLAG_RELEASED_REALLOCATED ?
					   "Vacated" : "Released");

#if MEMORY_KEEP_FREESITE
			OpSrcSite* fs = OpSrcSite::GetSite(event->mgi->freesiteid);
			dbg_printf(" %s:%d\n", fs->GetFile(), fs->GetLine());
#else
			dbg_printf("\n");
#endif

#if MEMORY_KEEP_FREESTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
			g_mem_guard->ShowCallStack(event->mgi->freestack_new);
#  else
			g_mem_guard->ShowCallStack(event->mgi->freestack,
									   MEMORY_KEEP_FREESTACK);
#  endif
#endif
		}
	}

	if ( do_report )
		dbg_printf("\n");
	else
		suppressed_event_count++;

	event_count++;

#ifdef MEMORY_ASSERT_ON_OOM
	if( event->eventclass == MEMORY_EVENTCLASS_OOM )
		OP_ASSERT(!"oom_assert");
	else
#endif // MEMORY_ASSERT_ON_OOM
	op_mem_error();
}

void OpMemReporting::PrintStatistics(void)
{
	if ( event_count > 0 )
	{
		if ( suppressed_event_count == 0 )
		{
			dbg_printf("MEM: Number of memory events: %d\n", event_count);
		}
		else
		{
			dbg_printf("MEM: Number of memory events: %d (only %d shown)\n",
					   event_count, event_count - suppressed_event_count);
		}
	}
}

#endif // ENABLE_MEMORY_DEBUGGING
