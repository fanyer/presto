/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Jens Lindstrom (jl) / Petter Nilsen (pettern)
*/

#include "core/pch.h"

#include "modules/pi/system/OpMemory.h"

#if defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)

#include "modules/memtools/memtools_codeloc.h"
#include "platforms/windows/pi/WindowsOpDLL.h"

class WindowsCodeLocationManager: public OpCodeLocationManager
{
public:
	virtual void start_lookup(OpCodeLocation*);
	virtual OP_STATUS initialize(void);
	virtual OpCodeLocation* create_location(UINTPTR addr);
	static OpCodeLocationManager* create(void);
};

static WindowsCodeLocationManager* g_windows_code_location_manager;

OpCodeLocationManager* OpCodeLocationManager::create(void)
{
	OP_ASSERT(g_windows_code_location_manager == NULL && "There shall only be one WindowsCodeLoacationManager instance!");
	g_windows_code_location_manager = new WindowsCodeLocationManager();
	OpMemDebug_Disregard(g_windows_code_location_manager);
	return g_windows_code_location_manager;
}

void free_WindowsCodeLocationManager()
{
	delete g_windows_code_location_manager;
	g_windows_code_location_manager = NULL;
}

OpCodeLocation* WindowsCodeLocationManager::create_location(UINTPTR addr)
{
	return new OpCodeLocation(addr);
}

OP_STATUS WindowsCodeLocationManager::initialize()
{
	if(!HASSymSetOptions() || !HASSymInitialize() || !HASSymFromAddr() || !HASSymGetLineFromAddr64())
		return OpStatus::ERR;

	OPSymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

	if (!OPSymInitialize(GetCurrentProcess(), NULL, TRUE))
		return OpStatus::ERR;
	return OpStatus::OK;
}

void WindowsCodeLocationManager::start_lookup(OpCodeLocation*loc)
{
	if (!loc)
		return;
	DWORD64  dwDisplacement = 0;
	DWORD64  dwAddress = loc->GetAddr();

	// Based on: http://msdn.microsoft.com/en-us/library/ms680578(v=vs.85).aspx
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]; /* ARRAY OK 2011-05-09 terjepe */
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	if(OPSymFromAddr(GetCurrentProcess(), dwAddress, &dwDisplacement, pSymbol))
	{
		OpString8 str;
		str.Set(pSymbol->Name);
		if (!str.HasContent())
			loc->SetFunction("No mem for symbol.");
		else
		{
			char *copy = op_strdup(str.CStr());
			if (copy)
			{
				OpMemDebug_Disregard(copy);
				loc->SetFunction(copy);
			}
			else
				loc->SetFunction("OOM!");
		}
	}
	else
	{
		loc->SetFunction("invalid address?");
	}

	DWORD dwDisplacement2;
	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	if(OPSymGetLineFromAddr64(GetCurrentProcess(), dwAddress, &dwDisplacement2, &line))
	{
		OpString8 str;
		str.AppendFormat("%s (%d)",line.FileName,line.LineNumber);
		if (!str.CStr())
			loc->SetFunction("No mem for symbol.");
		else
		{
			char *copy = op_strdup(str.CStr());
			if (copy)
			{
				OpMemDebug_Disregard(copy);
				loc->SetFileLine(copy);
			}
			else
				loc->SetFunction("OOM!");
		}
	}
	else
	{
		loc->SetFunction("File not found");
	}
}
#endif // defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)

#ifdef OPMEMORY_VIRTUAL_MEMORY

//
// Implement the virtual memory porting interface
//

static BOOL s_system_info_available = FALSE;
static SYSTEM_INFO s_sys_info;

size_t OpMemory::GetPageSize(void)
{
	if(!s_system_info_available)
	{
		GetSystemInfo(&s_sys_info);
		s_system_info_available = TRUE;
	}
	return s_sys_info.dwPageSize;
}

BOOL OpMemory::VirtualAlloc(const OpMemSegment* seg, void* ptr, size_t size)
{
#ifdef DEBUG_ENABLE_OPASSERT
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg->type == MEMORY_SEGMENT_VIRTUAL);
	OP_ASSERT((((UINTPTR)ptr) & (GetPageSize()-1)) == 0);
	OP_ASSERT((size & (GetPageSize()-1)) == 0);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));
#endif

	void* rc = ::VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);

	if(rc == NULL)
		return FALSE;

	OP_ASSERT(rc == ptr);

	return TRUE;
}

void OpMemory::VirtualFree(const OpMemSegment* seg, void* ptr, size_t size)
{
	DWORD free_flag = MEM_DECOMMIT;
	if (ptr == seg->address)
		free_flag = MEM_RELEASE;

#ifdef DEBUG_ENABLE_OPASSERT
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg->type == MEMORY_SEGMENT_VIRTUAL);
	OP_ASSERT((((UINTPTR)ptr) & (GetPageSize()-1)) == 0);
	OP_ASSERT((size & (GetPageSize()-1)) == 0);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

	BOOL rc =
#endif
		::VirtualFree(ptr, free_flag == MEM_RELEASE ? 0 : size, free_flag);
	OP_ASSERT(rc);
}

#define MAX_WINDOWS_MEMORY_SEGMENTS 256

struct WindowsMemSegment : public OpMemory::OpMemSegment
{
	WindowsMemSegment(void) { type = OpMemory::MEMORY_SEGMENT_UNUSED; }
};

static WindowsMemSegment handles[MAX_WINDOWS_MEMORY_SEGMENTS];

const OpMemory::OpMemSegment* OpMemory::CreateVirtualSegment(size_t minimum)
{
	OpMemory::OpMemSegment* mseg = 0;

	for ( int k = 0; k < MAX_WINDOWS_MEMORY_SEGMENTS; k++ )
	{
		if ( handles[k].type == MEMORY_SEGMENT_UNUSED )
		{
			const size_t granularity = (1<<23); // 8MB
			size_t size = (minimum + granularity - 1) & ~(granularity - 1);

			void* ptr = ::VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);

			if ( ptr != 0 )
			{
				mseg = &handles[k];
				mseg->address = ptr;
				mseg->size = size;
				mseg->type = MEMORY_SEGMENT_VIRTUAL;
			}

			break;
		}
	}
	return mseg;
}

#endif // OPMEMORY_VIRTUAL_MEMORY

HANDLE g_executableMemoryHeap;

#ifdef OPMEMORY_EXECUTABLE_SEGMENT

/* static */
const OpMemory::OpMemSegment* OpMemory::CreateExecSegment(size_t minimum)
{
	if (!g_executableMemoryHeap)
	{
		g_executableMemoryHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE | HEAP_NO_SERIALIZE, 0, 0);

		if (!g_executableMemoryHeap)
			return NULL;
	}

	OpMemory::OpMemSegment* block = OP_NEW(OpMemory::OpMemSegment, );

	if (!block)
		return NULL;

	block->address = HeapAlloc(g_executableMemoryHeap, 0, minimum);
	block->size = minimum;
	block->type = OpMemory::MEMORY_SEGMENT_EXECUTABLE;

	if (!block->address)
	{
		OP_DELETE(block);
		return NULL;
	}
	return block;
}

void* OpMemory::WriteExec(const OpMemory::OpMemSegment* , void* ptr , size_t ) 
{ 
	return ptr; 
}

OP_STATUS OpMemory::WriteExecDone(const OpMemory::OpMemSegment* , void* , size_t )
{ 
	return OpStatus::OK; 
}

#ifdef OPMEMORY_ALIGNED_SEGMENT
struct WindowsAlignedMemSegment : public OpMemory::OpMemSegment
{
	void* virtual_alloc_address;
};
#endif // OPMEMORY_ALIGNED_SEGMENT

void OpMemory::DestroySegment(const OpMemory::OpMemSegment* seg)
{
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg != 0);

	switch ( mseg->type )
	{
#ifdef OPMEMORY_EXECUTABLE_SEGMENT
	case MEMORY_SEGMENT_EXECUTABLE:
	{
		HeapFree(g_executableMemoryHeap, 0, mseg->address);
		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_EXECUTABLE_SEGMENT

#ifdef OPMEMORY_VIRTUAL_MEMORY
	case MEMORY_SEGMENT_VIRTUAL:
	{
		::VirtualFree(mseg->address, 0, MEM_RELEASE);
		mseg->type = MEMORY_SEGMENT_UNUSED;
		break;
	}
#endif // OPMEMORY_VIRTUAL_MEMORY

#ifdef OPMEMORY_STACK_SEGMENT
	case MEMORY_SEGMENT_STACK:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int rc =
#endif
			::VirtualFree(static_cast<char *>(mseg->address) - 4096, 0, MEM_RELEASE);

		OP_ASSERT(rc);

		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT
	case MEMORY_SEGMENT_ALIGNED:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
			::VirtualFree(static_cast<WindowsAlignedMemSegment*>(mseg)->virtual_alloc_address, 0, MEM_RELEASE);
		OP_ASSERT(res);
		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_ALIGNED_SEGMENT

	default:
		OP_ASSERT(!"Unsupported OpMemSegment type for DestroySegment");
	}
}

#endif // OPMEMORY_EXECUTABLE_SEGMENT

#ifdef OPMEMORY_EXECUTABLE_MEMORY

OP_STATUS OpMemory::AllocateExecutableMemory(OpMemExecutableBlock*& block, unsigned size)
{
	if (!g_executableMemoryHeap)
	{
		g_executableMemoryHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE | HEAP_NO_SERIALIZE, 0, 0);

		if (!g_executableMemoryHeap)
			return OpStatus::ERR_NO_MEMORY;
	}

	block = OP_NEW(OpMemExecutableBlock, ());

	if (!block)
		return OpStatus::ERR_NO_MEMORY;

	block->address = HeapAlloc(g_executableMemoryHeap, 0, size);
	block->size = size;

	if (!block->address)
	{
		OP_DELETE(block);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS OpMemory::FinalizeExecutableMemory(OpMemExecutableBlock* block, unsigned offset, unsigned length)
{
	return OpStatus::OK;
}

void OpMemory::FreeExecutableMemory(OpMemExecutableBlock* block)
{
	HeapFree(g_executableMemoryHeap, 0, block->address);
	OP_DELETE(block);
}

#endif // OPMEMORY_EXECUTABLE_MEMORY

#ifdef OPMEMORY_STACK_SEGMENT
const OpMemory::OpMemSegment* OpMemory::CreateStackSegment(size_t size)
{
	/* API requirement: size must be power of two. */
	OP_ASSERT(((size - 1) & size) == 0);

	/* API requirement: size must be at least 4096. */
	OP_ASSERT(size >= 4096);

	OpMemSegment* mseg = OP_NEW(OpMemSegment, ());

	if ( mseg != 0 )
	{
		/* Allocate requested size plus one page. */
		void* ptr =
			::VirtualAlloc(NULL, size + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if ( ptr == 0 )
		{
			OP_DELETE(mseg);
			mseg = 0;
		}
		else
		{
			/* Make the extra page allocated completely inaccessible
			   so that we crash instantly on stack overflows. */
			unsigned long op;
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL rc =
#endif
				::VirtualProtect(ptr, 1, PAGE_NOACCESS, &op);

			OP_ASSERT(rc);

			mseg->address = static_cast<char *>(ptr) + 4096;
			mseg->size = size;
			mseg->type = MEMORY_SEGMENT_STACK;
		}
	}

	return mseg;
}

#ifdef DEBUG_ENABLE_OPASSERT
static BOOL IsSegmentCurrentStack(const OpMemory::OpMemSegment* segment)
{
	int placeholder;

	/* More or less accurate. */
	char *stack_pointer = reinterpret_cast<char *>(&placeholder);

	/* Segment boundaries. */
	char *segment_start = static_cast<char *>(segment->address);
	char *segment_end = segment_start + segment->size;

	return segment_start < stack_pointer && stack_pointer < segment_end;
}
#endif // DEBUG_ENABLE_OPASSERT

void OpMemory::SignalStackSwitch(StackSwitchOperation operation, const OpMemSegment* current_stack, const OpMemSegment* new_stack)
{
	/* We needn't do anything here.  Just double-check that the
	   arguments are sane. */

	switch (operation)
	{
	case OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM:
		OP_ASSERT(current_stack == NULL);
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack == NULL);
		break;
	}
}
#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateAlignedSegment(size_t size, size_t alignment)
{
	WindowsAlignedMemSegment* mseg = OP_NEW(WindowsAlignedMemSegment, ());
	if (mseg)
	{
		// Our strategy to allocate an aligned block is divided into three phases identified
		// by attempt == 0, attempt == MAXATTEMPT and finally everything in between.
		// attempt = 0: Make an exact VirtualAlloc and hope for some luck. That luck is quite common
		//              because Windows normally return blocks with very high alignment.
		// attempt 1 - MAXATTEMPT: Reserve a too big block, find an aligned segment, free and then make
		//                         a targetted assignment at that address. Extremely likely to work.
		// attempt = MAXATTEMPT: Reserve and commit a big enough block and return an aligned sub-section of it.
		UINTPTR alignment_mask = ~static_cast<UINTPTR>(alignment-1);
		const int MAXATTEMPT = 10;
		for (int attempt = 0; attempt <= MAXATTEMPT; attempt++)
		{
			// First try exact size to really give the system a chance at minimizing fragmentation.
			size_t alloc_size = attempt == 0 ? size : size + alignment;
			char* ptr = static_cast<char*>(::VirtualAlloc(NULL, alloc_size, MEM_RESERVE, PAGE_READWRITE));
			if (!ptr)
			{
				OP_DELETE(mseg);
				mseg = NULL;
				break;
			}
			else
			{
				UINTPTR p = reinterpret_cast<UINTPTR>(ptr);
				char* aligned = reinterpret_cast<char*>((p + alignment - 1) & alignment_mask);

				mseg->address = aligned;
				mseg->size = size;
				mseg->type = MEMORY_SEGMENT_ALIGNED;
				mseg->virtual_alloc_address = ptr;
				if (attempt == 0 || attempt == MAXATTEMPT)
				{
					if (attempt == 0 && aligned != ptr)
					{
#ifdef DEBUG_ENABLE_OPASSERT
						BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
							::VirtualFree(ptr, 0, MEM_RELEASE);
						OP_ASSERT(res);
						continue;
					}
					// Either use the perfectly aligned block (if attempt == 0) or
					// give up and accept some slack (if attempt == MAXATTEMPT)
					void* res = ::VirtualAlloc(aligned, size, MEM_COMMIT, PAGE_READWRITE);
					if (!res)
					{
						DestroySegment(mseg);
						mseg = NULL;
					}
					break;
				}

#ifdef DEBUG_ENABLE_OPASSERT
				BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
					::VirtualFree(ptr, 0, MEM_RELEASE);
				OP_ASSERT(res);
				ptr = static_cast<char*>(::VirtualAlloc(aligned, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
				if (ptr)
				{
					OP_ASSERT(ptr == aligned);
					mseg->virtual_alloc_address = ptr;
					break;
				}
			}
		}
	}

	return mseg;
}

#endif // OPMEMORY_ALIGNED_SEGMENT
