/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MEM_UTILS_H
#define MEM_UTILS_H

# include "modules/util/simset.h"

#ifdef MEMTEST
# ifndef _BUILDING_OPERA_
#  define _BUILDING_OPERA_
# endif

void oom_add_unanchored_object(char *allocation_address, void *object_address);
void oom_delete_unanchored_object(void *object_address);
void oom_remove_anchored_object(void *object_address);
void oom_report_and_remove_unanchored_objects(char *caller = NULL);
extern int *global_oom_dummy;
#define OOM_REPORT_UNANCHORED_OBJECT() \
    do { \
        int dummy; \
        char *caller_address = 0; \
        if (( (void*) this > (void*) &dummy && (void*) this < (void*) global_oom_dummy) || \
			( (void*) this < (void*) &dummy && (void*) this > (void*) global_oom_dummy)) { \
            GET_CALLER(caller_address); \
            oom_add_unanchored_object(caller_address, this); \
        } \
    } while (0)

class oom_UnanchoredObject : public Link
{
public:
	oom_UnanchoredObject(char* alloc_address, void *object_address, int depth) :
		allocation_address(alloc_address),
		address(object_address),
		trap_depth(depth)
		{}
	char *allocation_address;
	void *address;
	int trap_depth;
};

class oom_TrapInfo : public Link
{
public:
	oom_TrapInfo();
	~oom_TrapInfo();
	
	char* m_call_address;
	BOOL m_is_left;
};

#endif




class MemUtil
{
  public:
#ifdef ADDR2LINE_SUPPORT
	class ModuleMatchList : public Link
	{
	public:

		ModuleMatchList(const char* match_string) { m_match_string = (char*)match_string; }
		~ModuleMatchList() { delete [] m_match_string; }
		char* GetString() { return m_match_string; }
		
	private:
		char* m_match_string;
	};

	static void AddIncludeModule(Link* include_module) { include_module->Into(&s_include_module_list); }
	static void AddExcludeModule(Link* exclude_module) { exclude_module->Into(&s_exclude_module_list); }
	
	static BOOL MatchesModule(void* const stack[], const int stack_levels);
    static BOOL MatchesModule(void* stack);
#endif // ADDR2LINE_SUPPORT

  private:

#ifdef ADDR2LINE_SUPPORT
	static Head s_include_module_list;
	static Head s_exclude_module_list;
#endif // ADDR2LINE_SUPPORT
};

#endif // MEM_UTILS_H
