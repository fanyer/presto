/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "mem_util.h"


#ifdef MEMTEST
# include "modules/memtools/happy-malloc.h"
# ifdef ADDR2LINE_SUPPORT
#  include "modules/memtools/addr2line.h"
# endif // ADDR2LINE_SUPPORT
int *global_oom_dummy = 0;


struct
{
	Head head;
	int depth;
} oom_trap_list;

Head oom_unanchored_objects_list;

void oom_add_unanchored_object(char *allocation_address, void *object_address)
{
	if (oom_trap_list.head.Empty()) // Not inside a TRAP
	{
		return;
	}
	oom_UnanchoredObject *last_object = (oom_UnanchoredObject*) oom_unanchored_objects_list.Last();
	if (last_object && last_object->address == object_address)
	{
		last_object->allocation_address = allocation_address;
		return; // The object has already been added from a subclass constructor
	}

#ifdef PARTIAL_LTH_MALLOC
	SAVE_MALLOC_STATE();
	enter_non_oom_code();
#endif
	oom_UnanchoredObject *object = new oom_UnanchoredObject(allocation_address, object_address, oom_trap_list.depth);
#ifdef PARTIAL_LTH_MALLOC
	RESET_MALLOC_STATE();
#endif
	if (object)
	{
		object->Into(&oom_unanchored_objects_list);
	}
	else
	{
		fprintf(stderr, "Not enough memory to create oom_UnanchoredObject");
	}
}

void oom_delete_unanchored_object(void *object_address)
{
	oom_UnanchoredObject *object = (oom_UnanchoredObject *) oom_unanchored_objects_list.Last();
	while (object)
	{
		if (object->address == object_address)
		{
			object->Out();
			delete object;
			return;
		}
		object = (oom_UnanchoredObject *) object->Pred();
	}
}

void oom_remove_anchored_object(void *object_address)
{
	oom_UnanchoredObject *object = (oom_UnanchoredObject *) oom_unanchored_objects_list.Last();
	while (object)
	{
		if (object->address == object_address)
		{
			object->Out();
			delete object;
			return;
		}
		object = (oom_UnanchoredObject *) object->Pred();
	}
}

void oom_report_and_remove_unanchored_objects(char *caller /* = NULL */)
{
	BOOL printed_header = FALSE;
	if (caller == NULL)
	{
		GET_CALLER(caller);
	}
	oom_UnanchoredObject *object = (oom_UnanchoredObject *) oom_unanchored_objects_list.Last();
	int depth = oom_trap_list.depth;
	while (object && object->trap_depth == depth)
	{
		if (!printed_header)
		{
			printed_header = TRUE;
			oom_TrapInfo *trap = (oom_TrapInfo*) oom_trap_list.head.Last();
#ifdef ADDR2LINE_SUPPORT
			fprintf(stderr,
					"Leaving trap called from:\n%p %s\n",
					trap->m_call_address, 
					Addr2Line::TranslateAddress(trap->m_call_address));
			fprintf(stderr,
					"Left from:\n%p %s\n",
					caller,
					Addr2Line::TranslateAddress(caller));
#endif // ADDR2LINE_SUPPORT
			fprintf(stderr, "-----------------------------------\n");
		}
#ifdef ADDR2LINE_SUPPORT
		if (MemUtil::MatchesModule(object->allocation_address))
		{
			fprintf(stderr, "Unanchored object: %p, allocated from: %p %s\n", object->address, object->allocation_address, Addr2Line::TranslateAddress(object->allocation_address));
		}
#else
		fprintf(stderr, "Unanchored object: %p, allocated from: %p\n", object->address, object->allocation_address);
#endif
		object->Out();
		delete object;
		object = (oom_UnanchoredObject *) oom_unanchored_objects_list.Last();
	}
	if (printed_header)
		fprintf(stderr, "-----------------------------------\n");
}

void oom_add_trap(oom_TrapInfo *trap)
{
	void *hirr = malloc_lth(10);
	free_lth(hirr);
	oom_trap_list.depth = oom_trap_list.head.Empty() ? 0 : oom_trap_list.depth + 1;
	trap->Into(&oom_trap_list.head);
}

void oom_remove_trap(oom_TrapInfo *trap)
{
	OP_ASSERT((oom_TrapInfo*) oom_trap_list.head.Last() == trap);
	oom_report_and_remove_unanchored_objects();
	--oom_trap_list.depth;
	trap->Out();
}


oom_TrapInfo::oom_TrapInfo() :
	m_is_left(FALSE)
{
	GET_CALLER(m_call_address);
	oom_add_trap(this);
}
oom_TrapInfo::~oom_TrapInfo()
{
	oom_remove_trap(this);
	if (!m_is_left)
	{
	}
}
#endif // MEMTEST


#ifdef ADDR2LINE_SUPPORT
#include "addr2line.h"

Head MemUtil::s_include_module_list;
Head MemUtil::s_exclude_module_list;

BOOL
MemUtil::MatchesModule(void* const stack[], const int stack_levels)
{
	if (!s_include_module_list.First() && !s_exclude_module_list.First())
		return TRUE; // No include or exclude specified

	if (s_exclude_module_list.First())
	{
		for (int i = 0; i < stack_levels; ++i)
		{
			const char *src_file = Addr2Line::TranslateAddress(stack[i]);
			
			ModuleMatchList* exclude_module = (ModuleMatchList*)s_exclude_module_list.First();
			while (exclude_module)
			{
				if (op_strstr(src_file, exclude_module->GetString()) != NULL)
					return FALSE; // Exclude matched
				exclude_module = (ModuleMatchList*)exclude_module->Suc();
			}
		}
	}

	if (s_include_module_list.First())
	{
		for (int i = 0; i < stack_levels; i++)
		{
			const char *src_file = Addr2Line::TranslateAddress(stack[i]);
			
			ModuleMatchList* include_module = (ModuleMatchList*)s_include_module_list.First();
			while (include_module)
			{
				if (op_strstr(src_file, include_module->GetString()) != NULL)
					return TRUE; // Include matched
				include_module = (ModuleMatchList*)include_module->Suc();
			}
		}
		return FALSE; // Include is specified, but did not match
	}
	else
		return TRUE; // No include specified and exclude did not match
}

BOOL
MemUtil::MatchesModule(void* address)
{
    return MatchesModule(&address, 1);
}

#endif // ADDR2LINE_SUPPORT
