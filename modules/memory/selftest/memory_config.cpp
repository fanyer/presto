/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Helper file used to test different configurations
 *
 * The directory \c modules/memory/selftest has a \c Makefile that can
 * be used to test-compile and verify that the API-definitions are
 * performing the correct operation.  This is a fairly shallow test, but
 * it is usefull to avoid goof-ups when doing changes to the \#defines and
 * \#ifdef conditional tests in \c modules/memory/memory_fundamentals.h .
 *
 * The \c Makefile will use the \c mkcfgs.pl script to create a number
 * of <code>cfg*.cpp</code> files which all test different configurations.
 *
 * This set of tests are done outside of the complete Opera environment
 * and its selftest system, as recompiling the whole source code would
 * be needed for each configuration tested, which is impractical.
 *
 * The main contents of this file is hidden from DOXYGEN as it would
 * interfere with the real implementations and declarations of the
 * functions that is instrumented and tested here.
 *
 * \b NOTE: the \c memory_config.cpp file is not compiled into Opera
 * itself.  The file is only used for the stand-alone tests.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_NAMESPACE_OP_NEW 1
#define MEMORY_REGULAR_OP_NEW 1
#define MEMORY_INLINED_OP_NEW 0
#define MEMORY_REGULAR_GLOBAL_NEW 1
#define MEMORY_INLINED_GLOBAL_NEW 0
#define MEMORY_REGULAR_NEW_ELEAVE 1
#define MEMORY_INLINED_NEW_ELEAVE 0
#define MEMORY_REGULAR_ARRAY_ELEAVE 1
#define MEMORY_INLINED_ARRAY_ELEAVE 0
#define MEMORY_REGULAR_GLOBAL_DELETE 1
#define MEMORY_INLINED_GLOBAL_DELETE 0
#define MEMORY_PLATFORM_HAS_ELEAVE 0
#define MEMORY_EXTERNAL_DECLARATIONS "modules/memory/src/memory_macros.h"
#define HAVE_MALLOC
#define op_lowlevel_malloc cfg_malloc
#define op_lowlevel_free cfg_free

extern "C" void* cfg_malloc(size_t size);
extern "C" void cfg_free(void* ptr);

#include "modules/memory/memory_fundamentals.h"

#ifndef DOXYGEN_DOCUMENTATION

void run_cfg(void);

char* oper = "<undefined>";
int checked = 0;
int verbose = 0;
const char* current_test = "<unknown>";

class Expect
{
public:
	Expect(const char* str) : str(strdup(str)), next(0) {}
	~Expect(void) { free(str); }
	void* operator new(size_t size) { return malloc(size); }
	void operator delete(void* ptr) { free(ptr); }
	Expect* GetNext(void) { return next; }
	void SetNext(Expect* e) { next = e; }
	char* Get(void) { return str; }

private:
	char* str;
	Expect* next;
};

Expect* expected = 0;
Expect* expected_last = 0;

void expect(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[10240]; // ARRAY OK mortenro 2008-02-24
	vsnprintf(tmp, sizeof(tmp)-1, format, args);
	va_end(args);
	Expect* e = new Expect(tmp);
	if ( expected == 0 )
	{
		expected = expected_last = e;
	}
	else
	{
		expected_last->SetNext(e);
		expected_last = e;
	}
	checked = 0;
}

void operation(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[10240]; // ARRAY OK mortenro 2008-02-24
	vsnprintf(tmp, sizeof(tmp)-1, format, args);
	va_end(args);
	oper = strdup(tmp);
	if ( verbose )
		printf("\n%s: %s\n", current_test, oper);
}

void called(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[10240]; // ARRAY OK mortenro 2008-02-24
	vsnprintf(tmp, sizeof(tmp)-1, format, args);
	va_end(args);

	if ( verbose )
		printf("     -> %s\n", tmp);

	if ( expected == 0 )
	{
		printf("\n");
		printf("FAILED: Configuration %s failed:\n", current_test);
		printf("      doing: %s\n", oper);
		printf("        got: %s\n", tmp);
		printf("   expected: <nothing>\n");
		printf("\n");
		exit(1);
	}

	if ( !strcmp(tmp, expected->Get()) )
	{
		Expect* e = expected;
		expected = e->GetNext();
		delete e;

		if ( expected == 0 )
		{
			expected_last = 0;
			checked = 1;
		}
		return;
	}

	printf("\n");
	printf("FAILED: Configuration %s failed:\n", current_test);
	printf("      doing: %s\n", oper);
	printf("        got: %s\n", tmp);
	printf("   expected: %s\n", expected->Get());
	printf("\n");
	exit(1);
}

void complete(void)
{
	if ( checked )
		return;
	printf("\n");
	printf("FAILED: Configuration %s failed:\n", current_test);
	printf("      doing: %s\n", oper);
	printf("        got: <nothing>\n");
	printf("   expected: %s\n", expected->Get());
	printf("\n");
	exit(1);
}

extern "C" void* cfg_malloc(size_t size)
{
	called("cfg_malloc(%d)", size);
	return malloc(size);
}

extern "C" void cfg_free(void* ptr)
{
	called("cfg_free(%p)", ptr);
}

void* op_meminternal_malloc_leave(size_t size)
{
	called("op_meminternal_malloc_leave(%d)", size);
	return malloc(size);
}

extern "C" void* OpMemDebug_OpMalloc(size_t size, const char* file, int line)
{
	called("OpMemDebug_OpMalloc(%d,\"%s\",%d)", size, file, line);
}

extern "C" void OpMemDebug_OpFree(void* ptr, void** pptr,
				  const char* file, int line)
{
	called("OpMemDebug_OpFree(%p,\"%s\",%d)", ptr, file, line);
}

void* OpMemDebug_New(size_t size, const char* file, int line, const char* obj)
{
	called("OpMemDebug_New(%d,\"%s\",%d,\"%s\")", size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewA(size_t size, const char* file, int line, const char* obj,
					  unsigned int count1a, unsigned int count1b,
					  size_t objsize)
{
	called("OpMemDebug_NewA(%d,\"%s\",%d,\"%s\",%d,%d,%d)",
		   size, file, line, obj, count1a, count1b, objsize);
	return malloc(size);
}

void* OpMemDebug_NewA_L(size_t size, const char* file, int line,
						const char* obj, unsigned int count1a,
						unsigned int count1b, size_t objsize)
{
	called("OpMemDebug_NewA_L(%d,\"%s\",%d,\"%s\",%d,%d,%d)",
		   size, file, line, obj, count1a, count1b, objsize);
	return malloc(size);
}

void* OpMemDebug_New_L(size_t size, const char* file, int line,
					   const char* obj)
{
	called("OpMemDebug_New_L(%d,\"%s\",%d,\"%s\")", size, file, line, obj);
	return malloc(size);
}


void* OpMemDebug_GlobalNew(size_t size)
{
	called("OpMemDebug_GlobalNew(%d)", size);
	return malloc(size);
}

void OpMemDebug_GlobalDelete(void* ptr)
{
	called("OpMemDebug_GlobalDelete(%p)", ptr);
}

void* operator new(size_t size, TOpAllocNewDefault)
{
	called("operator new(%d,TOpAllocNewDefault)", size);
	return malloc(size);
}

void* operator new[](size_t size, TOpAllocNewDefaultA)
{
	called("operator new[](%d,TOpAllocNewDefaultA)", size);
	return malloc(size);
}

void* operator new(size_t size, TOpAllocNewDefault_L)
{
	called("operator new(%d,TOpAllocNewDefault_L)",size);
	return malloc(size);
}

void* operator new[](size_t size, TOpAllocNewDefaultA_L)
{
	called("operator new[](%d,TOpAllocNewDefaultA_L)", size);
	return malloc(size);
}

void* operator new(size_t size, TOpAllocNewDefault,
		   const char* file, int line , const char* objname)
{
	called("operator new(%d,TOpAllocNewDefault,"
		   "\"%s\",%d,\"%s\")", size, file, line, objname);
	return malloc(size);
}

void* operator new[](size_t size, TOpAllocNewDefaultA,
				   const char* file, int line, const char* obj,
				   unsigned int count1a, unsigned int count1b,
				   size_t objsize)
{
	called("operator new[](%d,TOpAllocNewDefaultA,\"%s\",%d,\"%s\","
		   "%d,%d,%d)", size, file, line, obj, count1a, count1b, objsize);
	return malloc(size);
}

void* operator new[](size_t size, TOpAllocNewDefaultA_L,
					 const char* file, int line, const char* obj,
					 unsigned int count1a, unsigned int count1b,
					 size_t objsize)
{
	called("operator new[](%d,TOpAllocNewDefaultA_L,\"%s\",%d,\"%s\","
		   "%d,%d,%d)", size, file, line, obj, count1a, count1b, objsize);
	return malloc(size);
}

void* operator new(size_t size)
{
	called("operator new(%d)", size);
	return malloc(size);
}

void* operator new(size_t size, TLeave)
{
	called("operator new(%d,TLeave)", size);
	return malloc(size);
}

void* operator new[](size_t size, TLeave)
{
	called("operator new[](%d,TLeave)", size);
	return malloc(size);
}

void* operator new[](size_t size)
{
	called("operator new[](%d)", size);
	return malloc(size);
}

void *operator new(size_t size, TOpAllocNewDefault_L, const char* file,
				   int line, const char* obj)
{
	called("operator new(%d,TOpAllocNewDefault_L,\"%s\",%d,\"%s\")",
		   size, file, line, obj);
	return malloc(size);
}

void operator delete(void* ptr)
{
	called("operator delete(%p)", ptr);
}

void OpMemDebug_TrackDelete(const void* ptr, const char* file, int line)
{
	called("OpMemDebug_TrackDelete(%p,\"%s\",%d)", ptr, file, line);
}

extern "C" void OpMemDebug_ZapPointer(void** pptr)
{
	called("OpMemDebug_ZapPointer(%p)", pptr);
}

extern "C" void* OpPooledAlloc(size_t size)
{
	called("OpPooledAlloc(%d)", size);
	return malloc(size);
}

extern "C" void* OpPooledAlloc_L(size_t size)
{
	called("OpPooledAlloc_L(%d)", size);
	return malloc(size);
}

extern "C" void* OpPooledAllocT(size_t size)
{
	called("OpPooledAllocT(%d)", size);
	return malloc(size);
}

extern "C" void* OpPooledAllocT_L(size_t size)
{
	called("OpPooledAllocT_L(%d)", size);
	return malloc(size);
}

extern "C" void* OpPooledAllocP(size_t size)
{
	called("OpPooledAllocP(%d)", size);
	return malloc(size);
}

extern "C" void* OpPooledAllocP_L(size_t size)
{
	called("OpPooledAllocP_L(%d)", size);
	return malloc(size);
}

extern "C" void OpPooledFree(void* ptr)
{
	called("OpPooledFree(%p)", ptr);
}

#define BOOL int

class MemoryManager {
public:
	static void DecDocMemoryCount(size_t size);
	static void IncDocMemoryCount(size_t size, BOOL);
};

void MemoryManager::DecDocMemoryCount(size_t size)
{
	called("DecDocMemoryCount(%d)", size);
}

void MemoryManager::IncDocMemoryCount(size_t size, BOOL flag)
{
	called("IncDocMemoryCount(%d,%d)", size, (int)flag);
}

OpMemGroup::OpMemGroup(void)
{
}

OpMemGroup::~OpMemGroup(void)
{
}

void* OpMemGroup::NewGRO(size_t size, const char* file, int line, const char* obj)
{
	called("OpMemGroup::NewGRO(%d,%p,\"%s\",%d,\"%s\")", size, this, file, line, obj);
	return malloc(size);
}

void* OpMemGroup::NewGRO(size_t size)
{
	called("OpMemGroup::NewGRO(%d)", size);
	return malloc(size);
}

void* OpMemGroup::NewGRO_L(size_t size, const char* file, int line, const char* obj)
{
	called("OpMemGroup::NewGRO_L(%d,%p,\"%s\",%d,\"%s\")", size, this, file, line, obj);
	return malloc(size);
}

void* OpMemGroup::NewGRO_L(size_t size)
{
	called("OpMemGroup::NewGRO_L(%d)", size);
	return malloc(size);
}

void* OpMemDebug_OverloadedNew(size_t size)
{
	called("OpMemDebug_OverloadedNew(%d)", size);
	return malloc(size);
}

void* OpMemDebug_OverloadedNew(size_t size, const char* file, int line,
							   const char* obj)
{
	called("OpMemDebug_OverloadedNew(%d,\"%s\",%d,\"%s\")",
		   size, file, line, obj);
	return malloc(size);
}

void OpMemDebug_PooledDelete(void* ptr)
{
	called("OpMemDebug_PooledDelete(%p)", ptr);
}

void* OpMemDebug_NewSMOD(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOD(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewSMOD_L(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOD_L(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewSMOT(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOT(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewSMOT_L(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOT_L(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewSMOP(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOP(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

void* OpMemDebug_NewSMOP_L(size_t size, const char* file, int line,
						 const char* obj)
{
	called("OpMemDebug_NewSMOP_L(%d,\"%s\",%d,\"%s\")",
           size, file, line, obj);
	return malloc(size);
}

int main(int argc, char* argv)
{
	if ( argc != 1 )
		verbose = 1;

	run_cfg();

	return 0;
}

#endif // DOXYGEN_DOCUMENTATION
