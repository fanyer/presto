/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_OPALLOCINFO_H
#define MEMORY_OPALLOCINFO_H

/** \file
 *
 * \brief Include file for OpAllocInfo object
 * 
 * This file declares the OpAllocInfo object, which is created and
 * populated at the various functions used to debug memory allocations.
 *
 * Example: The \c OpMemDebug_OpMalloc() function is called for memory
 * debugging builds when \c op_malloc() is used.  The
 * \c OpMemDebug_OpMalloc() function will create an OpAllocInfo object
 * on the stack, populate it with sensible data, and pass it along
 * to the memory debugging database for safekeeping (copying).
 *
 * The OpAllocInfo class primarily contains the call site info, the
 * stack trace and type and details of the allocation.
 *
 * This class is typically instantiated on the heap by use of the
 * \c OPALLOCINFO(info,type) macro (this makes it possible to
 * recover stack-info where the stack-frame is non-existent or
 * special through assembly or inlined methods).
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifdef ENABLE_MEMORY_DEBUGGING

#ifdef MEMORY_CALLSTACK_DATABASE
#  include "modules/memtools/memtools_callstack.h"
#endif

struct OpMemGuardInfo;

//
// Calculate MEMORY_ALLOCINFO_STACK_DEPTH as the maximum of:
// MEMORY_KEEP_ALLOCSTACK, MEMORY_KEEP_REALLOCSTACK and MEMORY_KEEP_FREESTACK
// using preprocessor only (so we can trust the macro for array declarations).
//
#if MEMORY_KEEP_ALLOCSTACK > 0
#  define MEMORY_ALLOCINFO_STACK_DEPTH MEMORY_KEEP_ALLOCSTACK
#else
#  define MEMORY_ALLOCINFO_STACK_DEPTH 0
#endif
#if MEMORY_KEEP_REALLOCSTACK > MEMORY_ALLOCINFO_STACK_DEPTH
#  undef MEMORY_ALLOCINFO_STACK_DEPTH
#  define MEMORY_ALLOCINFO_STACK_DEPTH MEMORY_KEEP_REALLOCSTACK
#endif
#if MEMORY_KEEP_FREESTACK > MEMORY_ALLOCINFO_STACK_DEPTH
#  undef MEMORY_ALLOCINFO_STACK_DEPTH
#  define MEMORY_ALLOCINFO_STACK_DEPTH MEMORY_KEEP_FREESTACK
#endif

#if MEMORY_ALLOCINFO_STACK_DEPTH <= 0
// Create an OpAllocInfo object for allocation information
#define OPALLOCINFO(info,file,line,obj,type) \
	OpAllocInfo info(file,line,obj,type)
#else
// Create an OpAllocInfo object that includes call-stack info
#define OPALLOCINFO(info,file,line,obj,type) \
	OPGETCALLSTACK(MEMORY_ALLOCINFO_STACK_DEPTH); \
	OpAllocInfo info(file,line,obj,type); \
	OPCOPYCALLSTACK(info.SetStack(), MEMORY_ALLOCINFO_STACK_DEPTH)
#endif

/**
 * \brief Container Object to hold various information about an allocation
 *
 * An object of this type is created when an allocation (or deallocation)
 * happens, and its pointer is passed around to easilly propagate various
 * information regarding the (de-)allocation.
 *
 * Information passed around is: source file and line number of allocation
 * (through __FILE__ and __LINE__ and an OpSrcSite object), the
 * stack-trace of the allocation, the name of the object, what kind of
 * allocation, number of objects in an array allocation, etc.
 *
 * All this information may not be available on all platforms, or stored
 * for later use.
 *
 * This object will typically be allocated on the stack by the
 * \c OPALLOCINFO() macro, which will possibly recover the stack-frame
 * or return-addresses as well as allocating the \c OpAllocInfo object.
 */
class OpAllocInfo
{
public:
	/**
	 * \brief Initialize a created object
	 *
	 * The \c file and \c line arguments are typically initialized
	 * from \c __FILE__ and \c __LINE__ macros, the object name from the
	 * \c \#obj construct of the preprocessor (make something into a string),
	 * and the type is one of the \c MEMORY_FLAG_* defines.
	 *
	 * \param file The filename of the source code file of the allocation
	 * \param line The line number in the soruce code file of the allocation
	 * \param obj The name of the allocated object, or NULL when not available
	 * \param type One of the MEMORY_FLAG_* define values
	 */
	OpAllocInfo(const char* file, int line, const char* obj, UINT32 type);

	/**
	 * \brief Initialize from an existing allocation
	 *
	 * This is used under certain circumstances, e.g. when reporting
	 * an allocation after it has actually taken place.  This happens e.g
	 * during startup, when the log is written slightly later than the
	 * first few allocations.
	 *
	 * \param mgi The memory guard info from which to reconstruct
	 */
	OpAllocInfo(OpMemGuardInfo* mgi);

	/**
	 * \brief Set the number of array elements of the allocation
	 *
	 * When allocating an array (through operator new[] etc.) the size of
	 * the array can be provided from the \c OP_NEWA() macro so it is
	 * available to the memory debugger.
	 *
	 * \param num The number of objects/elements created by new[]
	 */
	void SetArrayElementCount(size_t num) { array_count = num; }

	/**
	 * \brief Set the size of a single element/object
	 *
	 * When allocating an array (through operator new[] etc.) it is
	 * possible to provide 'sizeof(obj)' in the \c OP_NEW() macro so
	 * the size of each element will be available to the memory debugger.
	 *
	 * Note: dividing the size of the allocation (in bytes) by the
	 * number of elements may not yield correct results when extra
	 * memory is set aside during the allocation for book-keeping.
	 *
	 * \param size The size in bytes of a single element/object allocated
	 */
	void SetArrayElementSize(size_t size) { array_elm_size = size; }

	/**
	 * \brief Retriev the flags of the object
	 */
	UINT32 GetFlags(void) { return flags; }

	/**
	 * \brief Set the flags of the object
	 *
	 * Setting the flags of the object it needed sometimes when e.g.
	 * a realloc operation in practice turned out to be an allocation.
	 */
	void SetFlags(UINT32 f) { flags = f; }

	/**
	 * \brief Retrieve the OpSrcSite ID of the allocation site
	 */
	UINT32 GetSiteId(void) const { return allocsite_id; }

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
	/**
	 * \brief Retrieve the call stack of the allocation site
	 *
	 * The macro \c MEMORY_ALLOCINFO_STACK_DEPTH holds the size of
	 * the \c UINTPTR array that contains the stack.  The stack[0]
	 * element is the most leaf-function on the stack, while
	 * stack[MEMORY_ALLOCINFO_STACK_DEPTH-1] is the oldest entry.
	 * There may be 0 values in the array towards the end if the
	 * return address could not be computed (or there is none).
	 *
	 * \returns A pointer to an array of UINTPTR return addresses
	 */
	const UINTPTR* GetStack(void) const { return stack; }

	UINTPTR* SetStack(void) { return stack; }
#endif

#if MEMORY_ALLOCINFO_STACK_DEPTH <= 0
	const UINTPTR* GetStack(void) const { return &zero_addr; }
#endif

#ifdef MEMORY_CALLSTACK_DATABASE
	OpCallStack* GetOpCallStack(int size);
#endif

	const char* GetObject(void) const { return object; }

private:
	size_t array_count;
	size_t array_elm_size;
	UINT32 flags;
	UINT32 allocsite_id;
	const char* object;

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	OpCallStack* callstack;
#  endif
	UINTPTR stack[MEMORY_ALLOCINFO_STACK_DEPTH];
#else
	UINTPTR zero_addr;
#endif
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // MEMORY_OPALLOCINFO_H
