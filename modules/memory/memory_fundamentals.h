/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_FUNDAMENTALS_H
#define MEMORY_FUNDAMENTALS_H

/** \file
 *
 * \brief Fundamental include file for the memory module.
 *
 * This include file must be included very early, typically by core/pch.h
 * just after all the features and capabilities etc. are set correctly.
 * This will allow the functionality provided by this module to be available
 * to e.g. inlined functions in central include files that gets included
 * early.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#if defined(_PTHREADS_) && !defined(MEMORY_USE_LOCKING)
/*
 * This is here to catch mis-configurations for UNIX systems that
 * typically relies on _PTHREADS_ macro to enable thread safety.
 *
 * Have a look in the tweaks documentation for more info.
 *
 * If you *really* want to have no mutex protection for any
 * memory operation (including the lea_malloc module), but you
 * need _PTHREADS_ defined, you have to uncomment this #error.
 */
#  error "_PTHREADS_ defined, but TWEAK_MEMORY_USE_LOCKING == NO"
#endif

#ifdef HAVE_LTH_MALLOC
/*
 * This is here to catch old configurations that are no longer supported.
 * If you still want to define HAVE_LTH_MALLOC - you will have to uncomment
 * the following #error directive, and you are on your own.
 *
 * For alternative functionality, see FEATURE_MEMORY_DEBUGGING.
 */
#error "HAVE_LTH_MALLOC is not supported, see FEATURE_MEMORY_DEBUGGING."
#endif // HAVE_LTH_MALLOC

#ifndef HAVE_MALLOC
/**
 * \name Wrapper macros to call our own heap allocator
 *
 * In case SYSTEM_MALLOC is set to NO, suggesting there is no
 * proper 'malloc' available on the system, we currently will have to
 * rely on our own heap allocator, 'dlmalloc' (lea_malloc module).
 *
 * It is an error to not enable lea_malloc if the system has no proper
 * malloc.
 *
 * When there is no system allocator, set up the following macros to call
 * 'dlmalloc'.
 *
 * @{
 */
#ifndef HAVE_DL_MALLOC
#  error "SYSTEM_MALLOC==NO and FEATURE_3P_LEA_MALLOC==NO are incompatible"
#endif

#include "modules/lea_malloc/malloc.h"

#if defined(USE_DL_PREFIX)
#  define op_lowlevel_malloc dlmalloc
#  define op_lowlevel_calloc dlcalloc
#  define op_lowlevel_realloc dlrealloc
#  define op_lowlevel_free dlfree
#else
#  define op_lowlevel_malloc malloc
#  define op_lowlevel_calloc calloc
#  define op_lowlevel_realloc realloc
#  define op_lowlevel_free free
#endif

/* @} */

#endif // !HAVE_MALLOC

/**
 * Round the given size up to a size that minimizes allocator waste/overhead.
 *
 * This macro can be used by buffer/string classes in order to minimize
 * unnecessary reallocation.
 *
 * The implementation of this function should be tuned to the specific
 * underlying allocator actually used.
 */
#define ROUND_UP_TO_NEXT_MALLOC_QUANTUM(x) (((x) + (2 * sizeof(size_t) - 1)) & ~(2 * sizeof(size_t) - 1))

#ifdef ENABLE_OPERA_SBRK
OP_EXTERN_C void* op_sbrk(long increment);
#endif // ENABLE_OPERA_SBRK

#ifdef ENABLE_OPERA_MMAP
OP_EXTERN_C void* op_mmap(void* ptr, size_t size, int prot, int flags);
OP_EXTERN_C int op_munmap(void* ptr, size_t size);
#endif // ENABLE_OPERA_MMAP

#if defined(ENABLE_OPERA_MMAP) || defined(ENABLE_OPERA_SBRK)
/**
 * \brief Destroy the op_sbrk() and op_mmap() emulation layers
 *
 * Calling this function will forcibly release the memory allocated
 * through any of op_sbrk() and op_mmap(). Doing so will render
 * lea_malloc high and dry without any memory to work with, and any
 * attempt at allocating, deallocating or using already allocated
 * memory will most probably cause a crash since the memory is gone.
 *
 * This function can be called by the platform code at some well
 * defined time when the heap is known to no longer be needed.  The
 * affected memory will be released back to the operating system.
 */
OP_EXTERN_C void op_sbrk_and_mmap_destroy(void);
#endif

#ifdef OPMEMORY_VIRTUAL_MEMORY
OP_EXTERN_C size_t op_get_mempagesize(void);
#endif

#if defined(ENABLE_MEMORY_OOM_EMULATION) && !defined(ENABLE_MEMORY_DEBUGGING)
#   define op_meminternal_malloc  op_limited_heap_malloc
#   define op_meminternal_calloc  op_limited_heap_calloc
#   define op_meminternal_realloc op_limited_heap_realloc
#   define op_meminternal_free    op_limited_heap_free

OP_EXTERN_C_BEGIN

void* op_limited_heap_malloc(size_t size);
void* op_limited_heap_calloc(size_t num, size_t size);
void* op_limited_heap_realloc(void* ptr, size_t size);
void  op_limited_heap_free(void* ptr);

OP_EXTERN_C_END

#else
#  define op_meminternal_malloc  op_lowlevel_malloc
#  define op_meminternal_calloc  op_lowlevel_calloc
#  define op_meminternal_realloc op_lowlevel_realloc
#  define op_meminternal_free    op_lowlevel_free
#endif

/**
 * \brief Macro to decide if any variant of pooling is needed
 *
 * This macro is used in the memory module to enable code which needs
 * to be included when some sort of pooling is activated.
 */
#if defined(MEMORY_SMOPOOL_ALLOCATOR) || defined(MEMORY_ELOPOOL_ALLOCATOR)
#define MEMORY_POOLING_ALLOCATORS
#endif

/**
 * \brief The size of a BLKPOOL
 *
 * This is typically one VM page size, which is the smallest unit
 * of memory that can be allocated on many architectures, or some
 * small power of two multiple of the VM size (like 16K).
 */
#define MEMORY_BLOCKSIZE (1<<MEMORY_BLOCKSIZE_BITS)


/**
 * \brief The default memory alignment
 *
 * This macro is used to provide a default value for the
 * TWEAK_MEMORY_ALIGNMENT tweak.  I.e. when using the MEMORY_ALIGNMENT macro,
 * it is the value of this macro that you get, unless TWEAK_MEMORY_ALIGNMENT
 * tweaked it to something else.
 *
 * The default value will be either 4 or 8, depending on the system
 * settings of SYSTEM_RISC_ALIGNMENT and SYSTEM_64BIT.
 *
 * \b Important: Don't use this macro in any code! Use MEMORY_ALIGNMENT
 * instead.
 */
#if defined(SIXTY_FOUR_BIT) || defined(NEEDS_RISC_ALIGNMENT)
#define DEFAULT_MEMORY_ALIGNMENT 8
#else
#define DEFAULT_MEMORY_ALIGNMENT 4
#endif

#ifdef __cplusplus

#if ! MEMORY_PLATFORM_HAS_ELEAVE
/**
 * \brief Identifying type for "leaving" variants of new.
 *
 * The LEAVE mechanism is borrowed from the Symbian OS and used on all
 * platforms by Opera.  It is similar to C++ exceptions, but much more
 * primitive in that it uses a longjmp() to carry out the exception.
 *
 * This declaration is for platforms that does not have the LEAVE
 * mechanism natively.
 */
enum TLeave { ELeave };
#endif // MEMORY_PLATFORM_HAS_ELEAVE

#include "modules/memory/src/memory_pooling.h"
#include "modules/memory/src/memory_group.h"

// FIXME: These shuld be controlled through a tweak.  The default of not
//        inlining is consistent with previous report memman operator usage.
#define MEM_POOLING_INLINE_NEW
#define MEM_POOLING_INLINE_DELETE

#define MEM_ACCOUNTED_DEC(size) \
	do { MemoryManager::DecDocMemoryCount(size); } while (0)
#define MEM_ACCOUNTED_INC(ptr, size) \
	if ( ptr != 0 ) MemoryManager::IncDocMemoryCount(size, FALSE); \
	return ptr

/**
 * \name Declare pooling or heap to be used for this class
 *
 * \brief Macros used in class declarations to force pooling or heap allocation
 *
 * By placing one of the macros below as the very first macro in a class,
 * this will enable pooling, accounted pooling, heap or accounted heap
 * allocations for the class.
 *
 * It is possible to override the allocator in derived classes
 * if this is desired.
 *
 * \b NOTE: The base class *MUST* have a virtual destructor if mixing
 * heap and pooling allocation between a base class and a derived class.
 *
 * \b NOTE: You also need a second macro to signal what kind of pooling
 * or heap behaviour that is needed.
 *
 * The accounted variants of these macros are inteded for the document
 * handling complex, and will increment/decrement the memory usage for
 * documents through the MemoryManager::IncDocMemoryCount() and
 * MemoryManager::DecDocMemoryCount() methods.
 *
 * It is an error to specify both pooling and heap allocation for a single
 * class.  One of the two strategies must be selected.  If it is really
 * necessary to allocate the same class both on the heap and by using
 * pooling, this can be achieved by creating a derived class that
 * specifies a different allocation strategy.
 *
 * When creating the object with the deviating allocator requirement,
 * create an object of the derived class (remember virtual destructors!).
 * Example:
 *
 * <code>
 * class OpFooBar { OP_ALLOC_POOLING ... };
 * class OpFooBarHeap : public OpFooBar { OP_ALLOC_HEAP ... };
 * OpFooBar* foo = OP_NEW(OpFooBar,());  // Using a pooling allocator
 * OpFooBar* bar = OP_NEW(OpFooBarHeap,());  // Created on the heap
 * </code>
 *
 * @{
 */

#ifdef ENABLE_MEMORY_DEBUGGING

#define MEMORY_SIGNATURE_NEW \
	operator new(size_t size, TOpAllocNewDefault, \
				 const char* file, int line, const char* obj) OP_NOTHROW
#define MEMORY_SIGNATURE_NEW_L \
	operator new(size_t size, TOpAllocNewDefault_L, \
				 const char* file, int line, const char* obj)

#define OP_ALLOC_POOLING \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr) \
	{ if ( ptr != 0 ) { OpMemDebug_PooledDelete(ptr); } }

#define OP_ALLOC_ACCOUNTED_POOLING \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr, size_t size) \
	{ if ( ptr != 0 ) { MEM_ACCOUNTED_DEC(size); \
			OpMemDebug_PooledDelete(ptr); } }

#define OP_ALLOC_POOLING_SMO_DOCUMENT			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpMemDebug_NewSMOD(size, file, line, obj); }

#define OP_ALLOC_POOLING_SMO_DOCUMENT_L			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpMemDebug_NewSMOD_L(size, file, line, obj); }

// FIXME: The additional regular 'operator new' is to be removed
// once OP_NEW is used everywhere:
#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpMemDebug_NewSMOD(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								   \
	MEM_POOLING_INLINE_NEW void*									   \
	operator new(size_t size) OP_NOTHROW \
	{ void* ptr = OpMemDebug_NewSMOD(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpMemDebug_NewSMOD_L(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								\
	MEM_POOLING_INLINE_NEW void* operator new(size_t size, TLeave)	\
	{ void* ptr = OpMemDebug_NewSMOD_L(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_POOLING_SMO_TEMPORARY \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpMemDebug_NewSMOT(size, file, line, obj); }

#define OP_ALLOC_POOLING_SMO_TEMPORARY_L \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpMemDebug_NewSMOT_L(size, file, line, obj); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpMemDebug_NewSMOT(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								   \
	MEM_POOLING_INLINE_NEW void*									   \
	operator new(size_t size) OP_NOTHROW \
	{ void* ptr = OpMemDebug_NewSMOT(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY_L \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpMemDebug_NewSMOT_L(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								\
	MEM_POOLING_INLINE_NEW void* operator new(size_t size, TLeave)	\
	{ void* ptr = OpMemDebug_NewSMOT_L(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_POOLING_SMO_PERSISTENT \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpMemDebug_NewSMOP(size, file, line, obj); }

#define OP_ALLOC_POOLING_SMO_PERSISTENT_L \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpMemDebug_NewSMOP_L(size, file, line, obj); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpMemDebug_NewSMOP(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								   \
	MEM_POOLING_INLINE_NEW void*									   \
	operator new(size_t size) OP_NOTHROW \
	{ void* ptr = OpMemDebug_NewSMOP(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT_L \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpMemDebug_NewSMOP_L(size, file, line, obj); \
		MEM_ACCOUNTED_INC(ptr, size); }								\
	MEM_POOLING_INLINE_NEW void* operator new(size_t size, TLeave)	\
	{ void* ptr = OpMemDebug_NewSMOP_L(size); MEM_ACCOUNTED_INC(ptr, size); }

void* OpMemDebug_NewSMOD(size_t size, const char* file,
						 int line, const char* obj);
void* OpMemDebug_NewSMOD(size_t size);
void* OpMemDebug_NewSMOD_L(size_t size, const char* file,
						   int line, const char* obj);
void* OpMemDebug_NewSMOD_L(size_t size);

void* OpMemDebug_NewSMOT(size_t size, const char* file,
						 int line, const char* obj);
void* OpMemDebug_NewSMOT(size_t size);
void* OpMemDebug_NewSMOT_L(size_t size, const char* file,
						   int line, const char* obj);
void* OpMemDebug_NewSMOT_L(size_t size);

void* OpMemDebug_NewSMOP(size_t size, const char* file,
						 int line, const char* obj);
void* OpMemDebug_NewSMOP(size_t size);
void* OpMemDebug_NewSMOP_L(size_t size, const char* file,
						   int line, const char* obj);
void* OpMemDebug_NewSMOP_L(size_t size);

void OpMemDebug_PooledDelete(void* ptr);

#else // ENABLE_MEMORY_DEBUGGING

#if MEMORY_NAMESPACE_OP_NEW
#define MEMORY_SIGNATURE_NEW operator new(size_t size, TOpAllocNewDefault) OP_NOTHROW
#define MEMORY_SIGNATURE_NEW_L operator new(size_t size, TOpAllocNewDefault_L)
#else // MEMORY_NAMESPACE_OP_NEW
#define MEMORY_SIGNATURE_NEW operator new(size_t size) OP_NOTHROW
#define MEMORY_SIGNATURE_NEW_L operator new(size_t size, TLeave)
#endif // MEMORY_NAMESPACE_OP_NEW

#ifdef USE_POOLING_MALLOC

OP_EXTERN_C void* OpPooledAlloc(size_t size);
OP_EXTERN_C void* OpPooledAlloc_L(size_t size);
OP_EXTERN_C void* OpPooledAllocT(size_t size);
OP_EXTERN_C void* OpPooledAllocT_L(size_t size);
OP_EXTERN_C void* OpPooledAllocP(size_t size);
OP_EXTERN_C void* OpPooledAllocP_L(size_t size);
OP_EXTERN_C void OpPooledFree(void* ptr);

#define OP_ALLOC_POOLING \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr) \
	{ if ( ptr != 0 ) { OpPooledFree(ptr); } }

#define OP_ALLOC_ACCOUNTED_POOLING \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr, size_t size) \
	{ if ( ptr != 0 ) { MEM_ACCOUNTED_DEC(size); OpPooledFree(ptr); } }

#define OP_ALLOC_POOLING_SMO_DOCUMENT			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpPooledAlloc(size); }

#define OP_ALLOC_POOLING_SMO_DOCUMENT_L			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpPooledAlloc_L(size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpPooledAlloc(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpPooledAlloc_L(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_POOLING_SMO_TEMPORARY			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpPooledAllocT(size); }

#define OP_ALLOC_POOLING_SMO_TEMPORARY_L			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpPooledAllocT_L(size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpPooledAllocT(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY_L  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpPooledAllocT_L(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_POOLING_SMO_PERSISTENT			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ return OpPooledAllocP(size); }

#define OP_ALLOC_POOLING_SMO_PERSISTENT_L			  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ return OpPooledAllocP_L(size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT	  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = OpPooledAllocP(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT_L  \
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L	\
	{ void* ptr = OpPooledAllocP_L(size); MEM_ACCOUNTED_INC(ptr, size); }

#else // USE_POOLING_MALLOC

// Don't do anything special if pooling is not enabled and no accounting needed
#define OP_ALLOC_POOLING
#define OP_ALLOC_POOLING_SMO_DOCUMENT
#define OP_ALLOC_POOLING_SMO_DOCUMENT_L
#define OP_ALLOC_POOLING_SMO_TEMPORARY
#define OP_ALLOC_POOLING_SMO_TEMPORARY_L
#define OP_ALLOC_POOLING_SMO_PERSISTENT
#define OP_ALLOC_POOLING_SMO_PERSISTENT_L

#define OP_ALLOC_ACCOUNTED_POOLING \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr, size_t size) \
	{ if ( ptr != 0 ) { MEM_ACCOUNTED_DEC(size); ::operator delete(ptr); } }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT		\
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW \
	{ void* ptr = ::operator new(size); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L	\
	MEM_POOLING_INLINE_NEW void* MEMORY_SIGNATURE_NEW_L \
	{ void* ptr = ::operator new(size, ELeave); MEM_ACCOUNTED_INC(ptr, size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY \
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
#define OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY_L \
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L
#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT \
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
#define OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT_L \
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

#endif // USE_POOLING_MALLOC

#endif // ENABLE_MEMORY_DEBUGGING

#if 0
// ----
#ifdef USE_POOLING_MALLOC

#define OP_ALLOC_POOLING_SMO_DOCUMENT			  \
	MEM_POOLING_INLINE_NEW void*				  \
	operator new(size_t size) OP_NOTHROW		  \
	{ return OpPooledAlloc(size); }

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT	  \
	MEM_POOLING_INLINE_NEW void*				  \
	operator new(size_t size) OP_NOTHROW		  \
	{ void* ptr = OpPooledAlloc(size); MEM_ACCOUNTED_INC(ptr, size); }

#else // USE_POOLING_MALLOC

#define OP_ALLOC_POOLING_SMO_DOCUMENT

#define OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT		\
	MEM_POOLING_INLINE_NEW void*					\
	operator new(size_t size) OP_NOTHROW			\
	{ void* ptr = ::operator new(size); MEM_ACCOUNTED_INC(ptr, size); }

#endif // USE_POOLING_MALLOC
// ---
#endif // 0

#define OP_ALLOC_HEAP \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr) \
	{ ::operator delete(ptr); }

#define OP_ALLOC_ACCOUNTED_HEAP \
	public: \
	MEM_POOLING_INLINE_DELETE void operator delete(void* ptr, size_t size) \
	{ if ( ptr != 0 ) { MEM_ACCOUNTED_DEC(size); ::operator delete(ptr); }

#endif // __cplusplus


/**
 * \brief Utility function to help save footprint
 *
 * This function will use \c op_meminternal_malloc() to allocate memory,
 * but will perform a LEAVE when unable to satisfy the request.
 *
 * \param size Number of bytes to allocate on heap
 *
 * \returns Pointer to allocated memory, and LEAVEs when no memory is
 *          available
 */
void* op_meminternal_malloc_leave(size_t size);


/**
 * \brief Helper function to perform LEAVE(OpStatus::ERR_NO_MEMORY)
 *
 * This function will simply perform a LEAVE. We need a function to do this,
 * since LEAVE() is not #defined at the point where this header file is
 * included, hence we cannot directly LEAVE() from here.
 *
 * Simply performs a LEAVE(OpStatus::ERR_NO_MEMORY);
 */
void* op_meminternal_always_leave(void);


#ifdef __cplusplus

/**
 * \brief Helper function to detect overflow in size_t calculations
 *
 * This function will return FALSE if the given parameters overflow when
 * multiplied. This should be used from allocation macros where the total
 * allocated size is a product of multiple values, hence vulnerable to
 * size_t overflow when given large values.
 *
 * \returns FALSE if multiplying arguments will overflow size_t, TRUE otherwise
 */
inline BOOL representable_product(size_t x, size_t y, size_t z = 1)
{
	return !x || !y || !z || (~(size_t)0) / x / y / z;
}

#endif // __cplusplus


/**
 * \name Identifying enums
 *
 * \brief Enums used to identify the various types of allocators.
 *
 * These enums are used as a dummy parameter to the various different new
 * operators.  They help distinguish the different kinds from one another.
 * In release builds, it is expected that the compiler will optimize away
 * the argument, at least when using inlined operator new.
 *
 * @{
 */
enum TOpAllocNewDefault { OpAllocNewDefault = 0 };
enum TOpAllocNewDefaultA { OpAllocNewDefaultA = 0 };
enum TOpAllocNewDefault_L { OpAllocNewDefault_L = 0 };
enum TOpAllocNewDefaultA_L { OpAllocNewDefaultA_L = 0 };
/** @} */


/*
 * This include will either include an empty default file, or a file
 * provided by the platform, to use platform specific macros:
 */
// FIXME: This include does not work; some pike script does not like it.
#ifdef MEMORY_EXTERNAL_DECLARATIONS
#include MEMORY_EXTERNAL_DECLARATIONS
#endif // MEMORY_EXTERNAL_DECLARATIONS

#ifdef ENABLE_MEMORY_DEBUGGING
/*
 * With memory debugging enabled, there will be special versions of
 * operator new that carry extra information about the allocation.
 * These special versions will be used by the OP_NEW() class of macros.
 *
 * The op_malloc() class of allocation methods will also be declared
 * so that they include extra information about the allocation.
 *
 * The extra allocation typically includes:
 *
 *    __FILE__
 *    __LINE__
 *    Name of object to be created (for new)
 *    Number of objects to be created (for new[])
 *    The size of each object to be created (for new[])
 */

#ifdef __cplusplus
/**
 * \name Wrapper functions
 *
 * \brief These functions are used to reach the \c OpMemDebug class.
 *
 * In order to reduce the complexity of memory_fundamentals.h, which is
 * included in all compilation units, and to avoid full recompilations
 * when working on the OpMemDebug class, a set of wrapper functions are
 * used to reach the memory debugging functionality.
 *
 * This will possibly also help keeping the footprint down.
 *
 * @{
 */

void* OpMemDebug_New(size_t size,
					 const char* file, int line, const char* obj);

void* OpMemDebug_New_L(size_t size,
					   const char* file, int line, const char* obj);

void* OpMemDebug_NewA(size_t size, const char* file, int line,
					  const char* obj, unsigned int count1,
					  unsigned int count2, size_t objsize);

void* OpMemDebug_NewA_L(size_t size, const char* file, int line,
						const char* obj, unsigned int count1,
						unsigned int count2, size_t objsize);

void* OpMemDebug_NewSMO(size_t size, const char* file, int line,
						const char* obj);

void* OpMemDebug_NewSMO_L(size_t size,
						  const char* file, int line, const char* obj);

void* OpMemDebug_NewELO(size_t size, OpMemLife &life,
						const char* file, int line, const char* obj);

void* OpMemDebug_NewELO_L(size_t size,  OpMemLife &life,
						  const char* file, int line, const char* obj);

void* OpMemDebug_NewELSA(size_t size, OpMemLife &life,
						 const char* file, int line, const char* obj,
						 unsigned int count1, unsigned int count2,
						 size_t objsize);

void* OpMemDebug_NewELSA_L(size_t size, OpMemLife &life,
						   const char* file, int line, const char* obj,
						   unsigned int count1, unsigned int count2,
						   size_t objsize);

void OpMemDebug_TrackDelete(const void* ptr, const char* file, int line);
void OpMemDebug_TrackDeleteA(const void* ptr, const char* file, int line);

#if MEMORY_INLINED_GLOBAL_NEW
void* OpMemDebug_GlobalNew(size_t size);
void* OpMemDebug_GlobalNew_L(size_t size);
void* OpMemDebug_GlobalNewA(size_t size);
void* OpMemDebug_GlobalNewA_L(size_t size);
#endif

#if MEMORY_INLINED_GLOBAL_DELETE
void OpMemDebug_GlobalDelete(void* ptr);
void OpMemDebug_GlobalDeleteA(void* ptr);
#endif

#endif // __cplusplus

OP_EXTERN_C_BEGIN

void* OpMemDebug_OpMalloc(size_t size, const char* file, int line);
void* OpMemDebug_OpCalloc(size_t num, size_t size, const char* file, int line);
void* OpMemDebug_OpRealloc(void* ptr, size_t size, const char* file, int line);
void  OpMemDebug_OpFree(void* ptr, void** pptr, const char* file, int line);
char* OpMemDebug_OpStrdup(const char* str, const char* file, int line);
uni_char* OpMemDebug_UniStrdup(const uni_char* str,
							   const char* file, int line);
void  OpMemDebug_OpFreeExpr(void* ptr, const char* file, int line);
void  OpMemDebug_ReportLeaks(void);

void  OpMemDebug_OOM(int countdown);

/**
 * \brief Disregard a specified allocation from being considered a leak
 *
 * If an allocation is not to be considered as a leak for whatever reason, it can be
 * tagged as such with this function. For instance it is useful for allocations used
 * while outputting callstacks.
 *
 * \param ptr - allocation to disregard.
 *
 */

void OpMemDebug_Disregard(void* ptr);

#ifdef MEMTOOLS_CALLSTACK_MANAGER
/**
 * \brief Print the current callstack
 *
 * This is useful if you want to insert a callstack in the log when certain unexpected events occur.
 *
 */

void OpMemDebug_PrintCallstack(void);
#endif // MEMTOOLS_CALLSTACK_MANAGER

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
/**
 * \brief Print all live allocations
 *
 * This is useful for monitoring memory usage over time and possibly revealing undesirable memory usage.

 * \param BOOL - Print full information for each allocation.
 *
 */

void OpMemDebug_PrintLiveAllocations(BOOL full);

/**
 * \brief Return total memory used
 *
 * \returns memory used
 */

int OpMemDebug_GetTotalMemUsed(void);
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

OP_EXTERN_C_END

/** @} */

#ifdef __cplusplus

#if MEMORY_INLINED_OP_NEW

/**
 * \name Operator new
 *
 * \brief Different operator new only used by Opera
 *
 * The signatures of the following operator new will cause these to only
 * be used by Opera.  For release builds, the operator new below will only
 * have two arguments, i.e. the size and the identifier enum value.
 * The remaining arguments are for debugging purposes only.
 *
 * @{
 */

inline void* operator new(size_t size, TOpAllocNewDefault,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_New(size, file, line, obj);
}

inline void* operator new(size_t size, TOpAllocNewDefault_L,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_New_L(size, file, line, obj);
}

inline void* operator new[](size_t size, TOpAllocNewDefaultA,
							const char* file, int line, const char* obj,
							unsigned int count1, unsigned int count2,
							size_t objsize) OP_NOTHROW
{
	return OpMemDebug_NewA(size, file, line, obj, count1, count2, objsize);
}

inline void* operator new[](size_t size, TOpAllocNewDefaultA_L,
							const char* file, int line, const char* obj,
							unsigned int count1, unsigned int count2,
							size_t objsize)
{
	return OpMemDebug_NewA_L(size, file, line, obj, count1, count2, objsize);
}

#if 0
// TBD - Needs discussion before official use

inline void* operator new(size_t size, TOpAllocNewSMO,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_NewSMO(size, file, line, obj);
}

inline void* operator new(size_t size, TOpAllocNewSMO_L,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_NewSMO_L(size, file, line, obj);
}

inline void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_NewELO(size, life, file, line, obj);
}

inline void* operator new(size_t size, TOpAllocNewELO_L, OpMemLife &life,
						  const char* file, int line, const char* obj) OP_NOTHROW
{
	return OpMemDebug_NewELO_L(size, life, file, line, obj);
}

inline void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life,
							const char* file, int line, const char* obj,
							unsigned int cnt1, unsigned int cnt2,
							size_t objsize) OP_NOTHROW
{
	return OpMemDebug_NewELSA(size,life,file,line,obj,cnt1,cnt2,objsize);
}

inline void* operator new[](size_t size, TOpAllocNewELSA_L, OpMemLife &life,
							const char* file, int line, const char* obj,
							unsigned int cnt1, unsigned int cnt2,
							size_t objsize) OP_NOTHROW
{
	return OpMemDebug_NewELSA_L(size,life,file,line,obj,cnt1,cnt2,objsize);
}

#endif // 0

/** @} */

#endif // MEMORY_INLINED_OP_NEW


#if MEMORY_INLINED_GLOBAL_NEW

/**
 * \name Global operator new
 *
 * \brief The global operator new in normal and array/leave versions.
 *
 * In order to allow code that uses the regular \c new and \c delete
 * operators, the global new operator needs to be declared.
 * This is not without its share of problems, like the risk of
 * interfering with, or getting mixed up with, the platform version.
 *
 * By inlining the operator new, it is hoped that interference with
 * the platform may be avoided, but for some compilers, true
 * inlining may not neccessarily take place.
 *
 * The g++ compiler will for instance only do inlining when compiling
 * with optimizations enabled (-O is sufficient for g++ 4.1.1).
 *
 * Not all configurations are dependent on the global operator new
 * being inlined, so the inlining is just an option.
 *
 * @{
 */

inline void* operator new(size_t size) OP_NOTHROW
{
	return OpMemDebug_GlobalNew(size);
}

inline void* operator new[](size_t size) OP_NOTHROW
{
	return OpMemDebug_GlobalNewA(size);
}

#endif // MEMORY_INLINED_GLOBAL_NEW


#if MEMORY_INLINED_NEW_ELEAVE

op_force_inline void* operator new(size_t size, TLeave)
{
	return OpMemDebug_GlobalNew_L(size);
}

#endif // MEMORY_INLINED_NEW_ELEAVE


#if MEMORY_INLINED_ARRAY_ELEAVE

op_force_inline void* operator new[](size_t size, TLeave)
{
	return OpMemDebug_GlobalNewA_L(size);
}

#endif // MEMORY_INLINED_ARRAY_ELEAVE


#if MEMORY_INLINED_GLOBAL_NEW
/** @} */
#endif // MEMORY_INLINED_GLOBAL_NEW


#if MEMORY_REGULAR_OP_NEW

void* operator new(size_t size, TOpAllocNewDefault,
				   const char* file, int line, const char* obj);

void* operator new(size_t size, TOpAllocNewDefault_L,
				   const char* file, int line, const char* obj);

void* operator new[](size_t size, TOpAllocNewDefaultA,
					 const char* file, int line, const char* obj,
					 unsigned int count1, unsigned int count2,
					 size_t objsize);

void* operator new[](size_t size, TOpAllocNewDefaultA_L,
					 const char* file, int line, const char* obj,
					 unsigned int count1, unsigned int count2,
					 size_t objsize);

#if 0
// TBD - Needs discussion before official use

#ifdef MEMORY_SMOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewSMO,
				   const char* file, int line, const char* obj);

void* operator new(size_t size, TOpAllocNewSMO_L,
				   const char* file, int line, const char* obj);

#endif // MEMORY_SMOPOOL_ALLOCATOR

#ifdef MEMORY_ELOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life,
				   const char* file, int line, const char* obj);

void* operator new(size_t size, TOpAllocNewELO_L, OpMemLife &life,
				   const char* file, int line, const char* obj);

void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life,
					 const char* file, int line, const char* obj,
					 unsigned int cnt1, unsigned int cnt2, size_t objsize);

void* operator new[](size_t size, TOpAllocNewELSA_L, OpMemLife &life,
					 const char* file, int line, const char* obj,
					 unsigned int cnt1, unsigned int cnt2, size_t objsize);

#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#endif // MEMORY_REGULAR_OP_NEW


#if MEMORY_REGULAR_GLOBAL_NEW
void* operator new(size_t size);
void* operator new[](size_t size);
#endif // MEMORY_REGULAR_GLOBAL_NEW


#if MEMORY_REGULAR_NEW_ELEAVE
void* operator new(size_t size, TLeave);
#endif // MEMORY_REGULAR_NEW_ELEAVE


#if MEMORY_REGULAR_ARRAY_ELEAVE
void* operator new[](size_t size, TLeave);
#endif // MEMORY_REGULAR_ARRAY_ELEAVE

#ifdef MEMORY_GROPOOL_ALLOCATOR
inline void* operator new(size_t size, OpMemGroup* group, const char* file, int line, const char* obj)
{ return group->NewGRO(size, file, line, obj); }
inline void* operator new(size_t size, TLeave, OpMemGroup* group, const char* file, int line, const char* obj)
{ return group->NewGRO_L(size, file, line, obj); }

inline void* operator new[](size_t size, OpMemGroup* group, const char* file, int line, const char* obj)
{ return group->NewGRO(size, file, line, obj); }
inline void* operator new[](size_t size, TLeave, OpMemGroup* group, const char* file, int line, const char* obj)
{ return group->NewGRO_L(size, file, line, obj); }
#endif // MEMORY_GROPOOL_ALLOCATOR

#endif // __cplusplus


/**
 * \name API definitions
 *
 * \brief Official and recomended methods to allocate memory.
 *
 * The macros below are mandated (\c op_malloc() etc.) or recomended
 * (\c OP_NEW() etc).  All of these macros will take differing values,
 * depending on whether a memory debugging build is made or not.
 * For debugging builds, \c __FILE__, \c __LINE__ and other extra
 * information is provided through the macro from the callsite.
 *
 * This is a valuable tool when trying to debug memory usage, memory
 * corruptions or leaks.
 *
 * @{
 */

/**
 * \brief General 'malloc' for Opera
 *
 * By making the official \c op_malloc allocation method be a macro,
 * It can be tailored to improve memory debugging functionality in
 * debug builds, and it will not introduce any overhead for release
 * builds.
 *
 * Because \c op_malloc is a macro, The address of \c op_malloc should
 * thus not be taken, as this will most surely fail or cause
 * unexpected and inconsistent results.
 *
 * The size argument is guaranteed to only appear once in the expanded
 * macro, so an argument with side effects will work correctly but is
 * discouraged.  The reason for this guarantee is that this macro is
 * generally used as a function, and should therefore act like one.
 *
 * \param size The number of bytes to allocate on the heap.
 *
 * \returns An address to the allocated memory, or NULL on failure
 */
#define op_malloc(size) \
        OpMemDebug_OpMalloc(size, __FILE__, __LINE__)

/**
 * \brief Genral 'calloc' for Opera
 *
 * Allocate memory, just like \c op_malloc(), but specify the number of
 * bytes to allocate differently and initizalize all the memory to zero.
 *
 * \param nmemb Number of records
 * \param size The size of a single record
 *
 * \returns An address to the allocated memory, or NULL on failure
 */
#define op_calloc(nmemb, size) \
		OpMemDebug_OpCalloc(nmemb, size, __FILE__, __LINE__)

/**
 * \brief General 'realloc' for Opera
 *
 * It is important to notice that the reallocation may move the memory,
 * even when shrinking it.  In debugging builds, it will probably always
 * move the data to provoke problems as early as possible.
 *
 * Also notice that a value of NULL is returned, the original data is
 * still allocated (but the size of the allocation couldn't be changed).
 *
 * \param ptr The previous allocation that is to be reallocated
 * \param size The new size of the allocation
 *
 * \returns The address of the reallocated data, or NULL on failure
 */
#define op_realloc(ptr, size) \
		OpMemDebug_OpRealloc(ptr, size, __FILE__, __LINE__)

/**
 * \brief General 'free' for Opera
 *
 * Just as for \c op_malloc(), the \c op_free() macro will allow
 * improved memory debugging, without any overhead present in release
 * builds.
 *
 * Note: For memory debugging builds, the pointer given is set to some
 * invalid address to catch usage of this pointer at some later point in
 * time.  For this reason, the argument to \c op_free() must be an lvalue
 * pointer.  See \c op_free_expr() if this is causes a problem.
 *
 * \param ptr Pointer to memory to be released
 */
#define op_free(ptr) \
		OpMemDebug_OpFree(ptr, (void**)0, __FILE__, __LINE__)

/**
 * \brief General 'strdup' for Opera
 *
 * The \c op_strdup() function allocates memory, and is for this reason
 * handled (at least the macro and the debug version) by the memory module.
 * This function is logged, just like for the other memory allocation
 * functions. The copy is allocated with op_malloc.
 *
 * \param str Pointer to NUL-terminated string to be duplicated on the heap
 * \returns Pointer to the duplicated string (or NULL on failure)
 */
#define op_strdup(str) \
		OpMemDebug_OpStrdup(str, __FILE__, __LINE__)

/**
 * \brief General 'uni_strdup' / 'wcsdup' for Opera
 *
 * The \c uni_strdup() function allocated memory, and is for this reason
 * handled just like the \c op_strdup() function in the memory module. The
 * copy is allocated with op_malloc.
 *
 * \param str Pointer to NUL-terminated uni_char string to be duplicated
 * \returns Pointer to the duplicated string (or NULL on failure)
 */
#define uni_strdup(str) \
		OpMemDebug_UniStrdup(str, __FILE__, __LINE__)

/**
 * \brief General 'free' for Opera that does not require lvalue argument
 *
 * Same behaviour as \c op_free(), but without setting the pointer
 * argument to an invalid value.  This variant is useful to get code
 * that frees memory from a non-lvalue pointer to compile.
 *
 * It should however be noted, that using this variant of \c op_free()
 * is discouraged, as it is less efficient at finding memory management
 * errors like heap corruption.
 */
#define op_free_expr(expr) \
		OpMemDebug_OpFreeExpr(expr, __FILE__, __LINE__)


#ifdef __cplusplus

#ifndef OP_NEW
/**
 * \brief Allocate a single object using the default allocator.
 *
 * This default allocator is either a general heap allocator, or a
 * specialized allocator selected/designed by the designer of the
 * class in question.
 *
 * Example usage:
 *
 * <code>OpMagicTrick* = OP_NEW(OpBlackMagic,("levitation"));</code>
 */
#define OP_NEW(obj, args) \
        new (OpAllocNewDefault, __FILE__, __LINE__, #obj) \
                obj args

#endif // !OP_NEW

#ifndef OP_NEWA
/**
 * \brief Allocate an array of objects using the default allocator.
 *
 * The number of objects are specified as the \c count argument.
 * Since this is a macro, and the \c count argument may appear several
 * times in the macro, the \c count argument should \e not have any side
 * effects.
 *
 * This default allocator is either a general heap allocator, or a
 * specialized allocator as for \c OP_NEW, but fewer options are
 * available to the designer so a general heap allocator is more
 * often used for arrays (in practice).
 *
 * For this reason, arrays of objects should generally be avoided and
 * replaced with arrays of pointers to the objects.  This strategy can
 * make better use of the pooling allocators, and only rely on the
 * general heap for the pointers.
 */
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) \
        new (OpAllocNewDefaultA, __FILE__, __LINE__, #obj, \
                count, count, sizeof(obj)) obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) \
        (representable_product(sizeof(obj), (count)) \
         ? new (OpAllocNewDefaultA, __FILE__, __LINE__, #obj, \
                count, count, sizeof(obj)) obj[count] \
         : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK

#endif // !OP_NEWA


#ifndef OP_NEWAA
/**
 * \brief Allocate a 2D array of objects using the default allocator.
 *
 * The number of objects are specified by the \c width and \c height
 * arguments. Since this is a macro, and the \c width and \c height
 * arguments may appear several times in the macro, these arguments
 * should \e not have any side effects.
 *
 * See \c OP_NEWA for more details.
 *
 * Calling OP_NEWAA(Foo, n1, n2) is equivalent to new Foo[n1][n2]
 */
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
        (representable_product((height), (width)) \
         ? new (OpAllocNewDefaultA, __FILE__, __LINE__, #obj, \
                (width) * (height), (width) * (height), sizeof(obj)) obj[width][height] \
         : NULL)
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
        (representable_product(sizeof(obj), (height), (width)) \
         ? new (OpAllocNewDefaultA, __FILE__, __LINE__, #obj, \
                (width) * (height), (width) * (height), sizeof(obj)) obj[width][height] \
         : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK

#endif // !OP_NEWAA


#ifndef OP_NEW_L
/**
 * \brief Allocate a single object using the default leaving allocator.
 *
 * This macro is identical to \c OP_NEW except it performs a \c LEAVE
 * on OOM condition.
 */
#define OP_NEW_L(obj, args) \
        new (OpAllocNewDefault_L, __FILE__, __LINE__, #obj) \
                obj args

#endif // !OP_NEW_L


#ifndef OP_NEWA_L
/**
 * \brief Allocate an array of objects using the default leaving allocator.
 *
 * This macro is identical to \c OP_NEWA except it performs a \c LEAVE
 * on OOM condition.
 */
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) \
        new (OpAllocNewDefaultA_L, __FILE__, __LINE__, #obj, \
                count, count, sizeof(obj)) obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) \
        (representable_product(sizeof(obj), (count)) \
         ? new (OpAllocNewDefaultA_L, __FILE__, __LINE__, #obj, \
                count, count, sizeof(obj)) obj[count] \
         : (obj *) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK

#endif // !OP_NEWA_L


#ifndef OP_NEWAA_L
/**
 * \brief Allocate a 2D array of objects using the default leaving allocator.
 *
 * This macro is identical to \c OP_NEWAA except it performs a \c LEAVE
 * on OOM condition.
 */
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
        (representable_product((height), (width)) \
         ? new (OpAllocNewDefaultA_L, __FILE__, __LINE__, #obj, \
                (width) * (height), (width) * (height), sizeof(obj)) obj[width][height] \
         : (obj (*)[height]) op_meminternal_always_leave())
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
        (representable_product(sizeof(obj), (height), (width)) \
         ? new (OpAllocNewDefaultA_L, __FILE__, __LINE__, #obj, \
                (width) * (height), (width) * (height), sizeof(obj)) obj[width][height] \
         : (obj (*)[height]) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK

#endif // !OP_NEWAA_L


#ifdef MEMORY_GROPOOL_ALLOCATOR
/**
 * \brief Allocate an object in as part of a set of "Grouped Allocations".
 *
 * This macro is used for Arena-type of allocations, called "Grouped
 * Allocations" in Opera. The allocator takes an OpMemGroup object as
 * argument, and co-locates all allocations made with the same
 * OpMemGroup controlling handle.
 *
 * Allocations done with this allocator must *not* be released
 * individually with OP_DELETE(), op_free() or any other method.
 *
 * All allocations will be released when the controlling OpMemGroup
 * object is destroyed, or its ReleaseAll() method is called.
 *
 * Note: Objects allocated with this allocator will never have their
 * destructors run, so they should preferably not have any to avoid
 * confusion.
 */
#define OP_NEWGRO(obj, args, group) \
	new (group, __FILE__, __LINE__, #obj) obj args
#define OP_NEWGRO_L(obj, args, group) \
	new (ELeave, group, __FILE__, __LINE__, #obj) obj args
#define OP_NEWGROA(obj, count, group) \
	new (group, __FILE__, __LINE__, #obj) obj[count]
#define OP_NEWGROA_L(obj, count, group) \
	new (ELeave, group, __FILE__, __LINE__, #obj) obj[count]

#endif // MEMORY_GROPOOL_ALLOCATOR

#if 0
// TBD: What this should look like

#ifndef OP_NEWSMO
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWSMO(obj, args) \
		new (OpAllocNewSMO, __FILE__, __LINE__, #obj) \
			obj args

#endif // OP_NEWSMO

#ifndef OP_NEWSMO_L
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWSMO_L(obj, args) \
		new (OpAllocNewSMO_L, __FILE__, __LINE__, #obj) \
			obj args

#endif // OP_NEWSMO_L

#ifndef OP_NEWELO
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWELO(obj, args, life) \
		new (OpAllocNewELO, life, __FILE__, __LINE__, #obj) \
			obj args

#endif // OP_NEWELO

#ifndef OP_NEWELO_L
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWELO_L(obj, args, life) \
		new (OpAllocNewELO_L, life, __FILE__, __LINE__, #obj) \
			obj args

#endif // OP_NEWELO_L

#ifndef OP_NEWELSA
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWELSA(obj, count, life) \
		new (OpAllocNewELSA, life, __FILE__, __LINE__, #obj, \
			count, count, sizeof(obj)) obj[count]

#endif // OP_NEWELSA

#ifndef OP_NEWELSA_L
/**
 * \brief Allocate a single object using the SMOPOOL allocator
 *
 * This macro will perform an allocation with the SMOPOOL allocator
 * for the default life expectancy.
 */
#define OP_NEWELSA_L(obj, args, life) \
		new (OpAllocNewELSA_L, life, __FILE__, __LINE__, #obj, \
			count, count, sizeof(obj)) obj[count]

#endif // OP_NEWELSA_L

#endif // 0


#ifndef OP_DELETE
/**
 * \brief Deallocate a single dynamically allocated object.
 *
 * This macro will cause the equivalent of a <code>delete obj;</code>
 * to be executed, assuming \c obj is a pointer to some object.
 *
 * The object to be deallocated may have been allocated by one of the
 * \c OP_NEW* macros, or directly by the \c new operator.  If the
 * object is not in a memory block known to Opera, the global delete
 * operator is called.
 */
#define OP_DELETE(ptr) \
        delete (OpMemDebug_TrackDelete_Templ((ptr), __FILE__, __LINE__))

template<typename T>
inline T* OpMemDebug_TrackDelete_Templ(T* ptr, const char* file, int line)
{
	OpMemDebug_TrackDelete(ptr, file, line);
	return ptr;
}

#endif // !OP_DELETE

#ifndef OP_DELETEA
/**
 * \brief Deallocate an array of dynamically allocated objects.
 *
 * This macro will cause the equivalent of a <code>delete[] obj;</code>
 * to be executed, assuming \c obj is a pointer to an array of objects.
 *
 * Otherwise similar to \c OP_DELETE.
 */
#define OP_DELETEA(ptr) \
        delete[] (OpMemDebug_TrackDeleteA_Templ((ptr), __FILE__, __LINE__))

template<typename T>
inline T* OpMemDebug_TrackDeleteA_Templ(T* ptr, const char* file, int line)
{
	OpMemDebug_TrackDeleteA(ptr, file, line);
	return ptr;
}

#endif // !OP_DELETEA

#endif // __cplusplus

/** @} */

#else // ENABLE_MEMORY_DEBUGGING

/*
 * We are not debugging memory; maybe it is a release build, so we declare
 * much simpler functions/macroes that does not do any debugging.
 * If we don't do pooling either, we can get away with only declaring
 * the OP_NEW* etc. macros to use the heap allocator.
 */

#ifdef __cplusplus

#if MEMORY_INLINED_OP_NEW

#if MEMORY_NAMESPACE_OP_NEW

inline void* operator new(size_t size, TOpAllocNewDefault)
{
	return op_meminternal_malloc(size);
}

inline void* operator new[](size_t size, TOpAllocNewDefaultA)
{
	return op_meminternal_malloc(size);
}

#endif // MEMORY_NAMESPACE_OP_NEW

inline void* operator new(size_t size, TOpAllocNewDefault_L)
{
	return op_meminternal_malloc_leave(size);
}

inline void* operator new[](size_t size, TOpAllocNewDefaultA_L)
{
	return op_meminternal_malloc_leave(size);
}

#if 0
// TBD - Needs discussion before official use

#ifdef MEMORY_SMOPOOL_ALLOCATOR

inline void* operator new(size_t size, TOpAllocNewSMO)
{
	return g_mem_pools.AllocSMO(size);
}

inline void* operator new(size_t size, TOpAllocNewSMO_L)
{
	return g_mem_pools.AllocSMO_L(size);
}

#endif // MEMORY_SMOPOOL_ALLOCATOR


#ifdef MEMORY_ELOPOOL_ALLOCATOR

inline void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life)
{
	return life.IntAllocELO(size, MEMORY_ALIGNMENT-1);
}

inline void* operator new(size_t size, TOpAllocNewELO_L, OpMemLife &life)
{
	return life.IntAllocELO_L(size, MEMORY_ALIGNMENT-1);
}

inline void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life)
{
	return life.IntAllocELO(size, 1);
}

inline void* operator new(size_t size, TOpAllocNewELSA_L, OpMemLife &life)
{
	return life.IntAllocELO_L(size, 1);
}

#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#endif // MEMORY_INLINED_OP_NEW


#if MEMORY_INLINED_GLOBAL_NEW

inline void* operator new(size_t size)
{
	return op_meminternal_malloc(size);
}

inline void* operator new[](size_t size)
{
	return op_meminternal_malloc(size);
}

#endif // MEMORY_INLINED_GLOBAL_NEW


#if MEMORY_INLINED_NEW_ELEAVE

inline void* operator new(size_t size, TLeave)
{
	return op_meminternal_malloc_leave(size);
}

#endif // MEMORY_INLINED_NEW_ELEAVE


#if MEMORY_INLINED_ARRAY_ELEAVE

inline void* operator new[](size_t size, TLeave)
{
	return op_meminternal_malloc_leave(size);
}

#endif // MEMORY_INLINED_ARRAY_ELEAVE


#if MEMORY_REGULAR_OP_NEW

#if MEMORY_NAMESPACE_OP_NEW
void* operator new(size_t size, TOpAllocNewDefault);
void* operator new(size_t size, TOpAllocNewDefault_L);
void* operator new[](size_t size, TOpAllocNewDefaultA);
void* operator new[](size_t size, TOpAllocNewDefaultA_L);
#endif // MEMORY_NAMESPACE_OP_NEW

#if 0
// TBD - Needs discussion before official use

#ifdef MEMORY_SMOPOOL_ALLOCATOR
void* operator new(size_t size, TOpAllocNewSMO);
void* operator new(size_t size, TOpAllocNewSMO_L);
#endif // MEMORY_SMOPOOL_ALLOCATOR

#ifdef MEMORY_ELOPOOL_ALLOCATOR
void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life);
void* operator new(size_t size, TOpAllocNewELO_L, OpMemLife &life);
void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life);
void* operator new(size_t size, TOpAllocNewELSA_L, OpMemLife &life);
#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#endif// MEMORY_REGULAR_OP_NEW


#if MEMORY_REGULAR_GLOBAL_NEW
void* operator new(size_t size);
void* operator new[](size_t size);
#endif // MEMORY_REGULAR_GLOBAL_NEW

#if MEMORY_REGULAR_NEW_ELEAVE
void* operator new(size_t size, TLeave);
#endif // MEMORY_REGULAR_NEW_ELEAVE

#if MEMORY_REGULAR_ARRAY_ELEAVE
void* operator new[](size_t size, TLeave);
#endif // MEMORY_REGULAR_ARRAY_ELEAVE

#ifdef MEMORY_GROPOOL_ALLOCATOR
inline void* operator new(size_t size, OpMemGroup* group)
	{ return group->NewGRO(size); }
inline void* operator new(size_t size, TLeave, OpMemGroup* group)
	{ return group->NewGRO_L(size); }

inline void* operator new[](size_t size, OpMemGroup* group)
	{ return group->NewGRO(size); }
inline void* operator new[](size_t size, TLeave, OpMemGroup* group)
	{ return group->NewGRO_L(size); }
#endif // MEMORY_GROPOOL_ALLOCATOR

#endif // __cplusplus

#define op_malloc(size) op_meminternal_malloc(size)
#define op_calloc(nmemb, size) op_meminternal_calloc(nmemb, size)
#define op_realloc(ptr, size) op_meminternal_realloc(ptr, size)
#define op_free(ptr) op_meminternal_free(ptr)
#define op_strdup(str) op_lowlevel_strdup(str)
#define uni_strdup(str) uni_lowlevel_strdup(str)

#if MEMORY_NAMESPACE_OP_NEW

#ifndef OP_NEW
#define OP_NEW(obj, args) new (OpAllocNewDefault) obj args
#endif

#ifndef OP_NEWA
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) new (OpAllocNewDefaultA) obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) \
	(representable_product(sizeof(obj), (count)) \
	 ? new (OpAllocNewDefaultA) obj[count] \
	 : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWA

#ifndef OP_NEWAA
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
	(representable_product((height), (width)) \
	 ? new (OpAllocNewDefaultA) obj[width][height] \
	 : NULL)
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
	(representable_product(sizeof(obj), (height), (width)) \
	 ? new (OpAllocNewDefaultA) obj[width][height] \
	 : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWAA

#ifndef OP_NEW_L
#define OP_NEW_L(obj, args) new (OpAllocNewDefault_L) obj args
#endif

#ifndef OP_NEWA_L
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) new (OpAllocNewDefaultA_L) obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) \
	(representable_product(sizeof(obj), (count)) \
	 ? new (OpAllocNewDefaultA_L) obj[count] \
	 : (obj *) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWA_L

#ifndef OP_NEWAA_L
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
	(representable_product((height), (width)) \
	 ? new (OpAllocNewDefaultA_L) obj[width][height] \
	 : (obj (*)[height]) op_meminternal_always_leave())
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
	(representable_product(sizeof(obj), (height), (width)) \
	 ? new (OpAllocNewDefaultA_L) obj[width][height] \
	 : (obj (*)[height]) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWAA_L

#else // MEMORY_NAMESPACE_OP_NEW

#ifndef OP_NEW
#define OP_NEW(obj, args) new obj args
#endif

#ifndef OP_NEWA
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) new obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA(obj, count) \
	(representable_product(sizeof(obj), (count)) \
	 ? new obj[count] \
	 : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWA

#ifndef OP_NEWAA
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
	(representable_product((height), (width)) \
	 ? new obj[width][height] \
	 : NULL)
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA(obj, width, height) \
	(representable_product(sizeof(obj), (height), (width)) \
	 ? new obj[width][height] \
	 : NULL)
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWAA

#ifndef OP_NEW_L
#define OP_NEW_L(obj, args) new (ELeave) obj args
#endif

#ifndef OP_NEWA_L
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) new (ELeave) obj[count]
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWA_L(obj, count) \
	(representable_product(sizeof(obj), (count)) \
	 ? new (ELeave) obj[count] \
	 : (obj *) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWA_L

#ifndef OP_NEWAA_L
#ifdef HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
	(representable_product((height), (width)) \
	 ? new (ELeave) obj[width][height] \
	 : (obj (*)[height]) op_meminternal_always_leave())
#else // HAVE_NEWA_OVERFLOW_CHECK
#define OP_NEWAA_L(obj, width, height) \
	(representable_product(sizeof(obj), (height), (width)) \
	 ? new (ELeave) obj[width][height] \
	 : (obj (*)[height]) op_meminternal_always_leave())
#endif // HAVE_NEWA_OVERFLOW_CHECK
#endif // !OP_NEWAA_L

#endif // MEMORY_NAMESPACE_OP_NEW

#ifdef MEMORY_GROPOOL_ALLOCATOR
#define OP_NEWGRO(obj, args, group) \
	new (group) obj args
#define OP_NEWGRO_L(obj, args, group) \
	new (ELeave, group) obj args
#define OP_NEWGROA(obj, count, group) \
	new (group) obj[count]
#define OP_NEWGROA_L(obj, count, group) \
	new (ELeave, group) obj[count]
#endif

#if 0
// TBD - Needs discussion before official use

//
// Make the OP_NEW* macros point to the pooling allocators, if available
//

#ifdef MEMORY_SMOPOOL_ALLOCATOR

#ifndef OP_NEWSMO
#define OP_NEWSMO(obj, args) \
        new (OpAllocNewSMO) obj args
#endif

#ifndef OP_NEWSMO_L
#define OP_NEWSMO_L(obj, args) \
        new (OpAllocNewSMO_L) obj args
#endif

#else // MEMORY_SMOPOOL_ALLOCATOR

#ifndef OP_NEWSMO
#define OP_NEWSMO(obj, args) OP_NEW(obj, args)
#endif

#ifndef OP_NEWSMO_L
#define OP_NEWSMO_L(obj, args) OP_NEW_L(obj, args)
#endif

#endif // MEMORY_SMOPOOL_ALLOCATOR


#ifdef MEMORY_ELOPOOL_ALLOCATOR

#ifndef OP_NEWELO
#define OP_NEWELO(obj, args, life) \
        new (OpAllocNewELO, life) obj args
#endif

#ifndef OP_NEWELO_L
#define OP_NEWELO_L(obj, args, life) \
        new (OpAllocNewELO_L, life) obj args
#endif

#ifndef OP_NEWELSA
#define OP_NEWELSA(obj, count, life) \
        new (OpAllocNewELSA, life) obj[count]
#endif

#ifndef OP_NEWELSA_L
#define OP_NEWELSA_L(obj, count, life) \
        new (OpAllocNewELSA, life) obj[count]
#endif

#else // MEMORY_ELOPOOL_ALLOCATOR

#ifndef OP_NEWELO
#define OP_NEWELO(obj, args, life) OP_NEW(obj, args)
#endif

#ifndef OP_NEWELO_L
#define OP_NEWELO_L(obj, args, life) OP_NEW_L(obj, args)
#endif

#ifndef OP_NEWELSA
#define OP_NEWELSA(obj, count, life) OP_NEWA(obj, count)
#endif

#ifndef OP_NEWELSA_L
#define OP_NEWELSA_L(obj, count, life) OP_NEWA_L(obj, count)
#endif

#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#ifndef OP_DELETE
#define OP_DELETE(ptr) delete ptr
#endif

#ifndef OP_DELETEA
#define OP_DELETEA(ptr) delete[] ptr
#endif

#endif // ENABLE_MEMORY_DEBUGGING

#ifdef __cplusplus

#ifndef OP_NEWA_WH
/**
 * \brief Allocate an array of width * height objects using the default allocator.
 *
 * The number of objects are specified by the \c width and \c height
 * arguments. These are multiplied to produce the actual size of the
 * (one-dimensional) array. Overflow is detected, and results in a
 * NULL return value.
 *
 * Otherwise, this macro is identical to the OP_NEWA macro
 */
#define OP_NEWA_WH(obj, width, height) \
	(representable_product((height), (width)) \
	 ? OP_NEWA(obj, (width) * (height)) \
	 : NULL)
#endif // !OP_NEWA_WH

#ifndef OP_NEWA_WHD
/**
 * \brief Allocate an array of width * height * depth objects using the default allocator.
 *
 * The number of objects are specified by the \c width, \c height and
 * \c depth arguments. These are multiplied to produce the actual size
 * of the (one-dimensional) array. Overflow is detected, and results
 * in a NULL return value.
 *
 * Otherwise, this macro is identical to the OP_NEWA macro
 */
#define OP_NEWA_WHD(obj, width, height, depth) \
	(representable_product((depth), (height), (width)) \
	 ? OP_NEWA(obj, (width) * (height) * (depth)) \
	 : NULL)
#endif // !OP_NEWA_WHD

#ifndef OP_NEWA_WH_L
/**
 * \brief Allocate an array of width * height objects using the default leaving allocator.
 *
 * This macro is identical to \c OP_NEWA_WH except it performs a \c LEAVE
 * on OOM condition.
 */
#define OP_NEWA_WH_L(obj, width, height) \
	(representable_product((height), (width)) \
	 ? OP_NEWA_L(obj, (width) * (height)) \
	 : (obj *) op_meminternal_always_leave())
#endif // !OP_NEWA_WH_L

#ifndef OP_NEWA_WHD_L
/**
 * \brief Allocate an array of width * height * depth objects using the default leaving allocator.
 *
 * This macro is identical to \c OP_NEWA_WHD except it performs a \c LEAVE
 * on OOM condition.
 */
#define OP_NEWA_WHD_L(obj, width, height, depth) \
	(representable_product((depth), (height), (width)) \
	 ? OP_NEWA_L(obj, (width) * (height) * (depth)) \
	 : (obj *) op_meminternal_always_leave())
#endif // !OP_NEWA_WHD_L

#if MEMORY_REGULAR_GLOBAL_DELETE

/**
 * \name Global operator delete
 *
 * \brief The global operator delete in normal and array versions.
 *
 * The global operator delete has the same considerations as the
 * global operator new with respect to inlining and interference with
 * the platform.
 *
 * In order to do memory debugging also for delete operations, the
 * \c OP_DELETE() macro contains code to first log the operation, then
 * call the delete operator to perform the operation.
 *
 * @{
 */

/**
 * \brief Global (possibly inlined) operator delete
 *
 * This inlined \c operator \c delete must be used for all Opera core code
 * and related platform code that allocates on behalf of core code.
 * The action taken is either a direct (unlogged) op_free when pooling
 * is not used, or the pooling free when pooling is used.
 *
 * The memory debugging functionality is taken care of in \c OP_DELETE() and
 * \c OP_DELETEA() macros themselves.  These macroes will call
 * \c OpMemDebug::TrackDelete() methods to help keep track of deletions.
 *
 * \param ptr Pointer to object to be released
 */
void operator delete(void* ptr);

/**
 * \brief Global (possibly inlined) operator array delete
 *
 * This inlined \c operator \c delete for arrays is equivalent to the
 * one for ordinary objects.
 *
 * \param ptr Pointer to memory containg array of objects to bee released
 */
void operator delete[](void* ptr);

/** @} */

#endif // MEMORY_REGULAR_GLOBAL_DELETE


#if MEMORY_INLINED_GLOBAL_DELETE

inline void operator delete(void* ptr)
{
#ifdef MEMORY_POOLING_ALLOCATORS
	OpMemPools::Free(ptr);
#else
#  ifdef ENABLE_MEMORY_DEBUGGING
	OpMemDebug_GlobalDelete(ptr);
#  else
	op_meminternal_free(ptr);
#  endif
#endif
}

inline void operator delete[](void* ptr)
{
#ifdef MEMORY_POOLING_ALLOCATORS
	OpMemPools::Free(ptr);
#else
#  ifdef ENABLE_MEMORY_DEBUGGING
	OpMemDebug_GlobalDeleteA(ptr);
#  else
	op_meminternal_free(ptr);
#  endif
#endif
}

#endif // MEMORY_INLINED_GLOBAL_DELETE

#endif // __cplusplus

/*
 * Globally accessible function to initialise contents of the
 * memory state. Allows command line arguments to define memory
 * behaviour prior to Opera itself being initialised and run.
 *
 * @param limited_heap_size Size of limited heap. Has no effect unless
 * ENABLE_MEMORY_OOM_EMULATION is defined.
 *
 * @param memory_logging Enable or disable memory logging. Has no effect
 * unless MEMORY_LOG_ALLOCATIONS is defined.
 *
 * @param efence_classnames String of comma-separated names of classes
 * that are to be fenced in. Has no effect unless MEMORY_ELECTRIC_FENCE
 * is defined. Contents will be copied and caller retains ownership of
 * argument.
 */
OP_EXTERN_C void OpMemoryInit(size_t limited_heap_size, BOOL memory_logging, const char* efence_classnames);

#ifdef SELFTEST
void* OpMemGuard_Real2Shadow(void* ptr);
#endif // SELFTEST

//
// Patch the call-stack information tweaks if there is no way to obtain
// the call-stack information.  This is useful when the same tweaks are
// used for both debugging builds and release builds, or for different
// architectures (LINGOGI/WINGOGI) where call-stack availability differs.
//
// Let the availability of the OPGETCALLSTACK macro decide if call-stack
// information can be obtained, and use this info to override the tweaks.
//
#ifndef OPGETCALLSTACK
#  undef MEMORY_KEEP_ALLOCSTACK
#  undef MEMORY_KEEP_REALLOCSTACK
#  undef MEMORY_KEEP_FREESTACK
#  define MEMORY_KEEP_ALLOCSTACK	0
#  define MEMORY_KEEP_REALLOCSTACK	0
#  define MEMORY_KEEP_FREESTACK		0
#endif

#if defined(__cplusplus) && MEMORY_INLINED_PLACEMENT_NEW
inline void* operator new(size_t size, void* ptr) { return ptr; }
#endif

#ifdef MEMORY_DEBUGGING_ALLOCATOR

OP_EXTERN_C_BEGIN

void* op_debug_malloc(size_t size);
void  op_debug_free(void* ptr);
char* op_debug_strdup(const char* str);

OP_EXTERN_C_END

#endif // MEMORY_DEBUGGING_ALLOCATOR

/**
 * \brief Macro for retrieving alignment requirements for a given type
 *
 * This macro calculates the alignment requirement for a given type name.
 * The given type name may be a primitive/built-in type, or an aggregate
 * class/struct name. The macro returns a positive integer which indicates at
 * which addresses an object of the given type should be placed: To guarantee
 * proper alignment, place the object at an address that is an integer multiple
 * of the returned integer (i.e. where address % op_strideof(type) == 0).
 *
 * The macro expands to an expression that should be easily constant-folded by
 * the compiler into a single value, thus not affecting runtime or footprint
 * any more than a hardcoded integer.
 *
 * This macro was inspired by the following paper:
 * Computing Memory Alignment Restrictions of Data Types in C and C++
 * (available at http://www.monkeyspeak.com/alignment/)
 *
 * \param T The type name from which to calculate the alignment requirement.
 * \return A positive integer. An object of the given type is guaranteed to
 *         be properly aligned if it is located at an address which is an
 *         integer multiple of the integer returned from this macro.
 */
#ifdef __cplusplus
template <typename T>
struct Tchar {
	T t;
	char c;
};
#define op_strideof(T)                            \
	((sizeof(Tchar<T>) > sizeof(T)) ?             \
	  sizeof(Tchar<T>) - sizeof(T) : sizeof(T))
#else /* __cplusplus */
/* The following is safe, but may be sub-optimal. */
#define op_strideof(T) sizeof(T)
#endif /* __cplusplus */

#ifdef VALGRIND
//
// When valgrind might be used to run Opera, it is possible to
// interact with it in certain ways. For instance, when valgrind is
// run on top of our memory debugger, the memory debugger needs to
// interact with valgrind to make valgrind efficient.
//
// The following functions can be used to query or command valgrind.
// This is only a subset of what is possible; the memory module does
// its interaction with valgrind directly, but other parts of Opera
// might want to use wrapper functions defined here for cleanliness.
//

/**
 * \brief Query if valgrind is being used currently
 *
 * Determine at runtime if the program is being run under the control
 * of valgrind or not. A zero value means valgrind is not used, while
 * nonzero means that valgrind is used.
 *
 * \returns The number of valgrind instances we are running inside, or 0
 */
int op_valgrind_used(void);

/**
 * \brief Force valgrind into treating a region of memory as defined
 *
 * Telling valgrind that an area of memory is defined after all can
 * be useful; it allows us to e.g. use memory that valgrind would
 * otherwise gives warnings about (using uninitialized data).
 *
 * This can be useful when e.g. an algorithm does not require certain
 * data to be initialized; it will magically do the right thing
 * anyway. The search engine has such code, where clearing the memory
 * is an overhead that is not needed.
 *
 * \param addr The address of the first byte to become defined
 * \param bytes The number of bytes to become defined
 */
void op_valgrind_set_defined(void* addr, int bytes);

/**
 * \brief Force valgrind into treating a region of memory as undefined
 *
 * Likewise, telling Valgrind that a region of memory should be regarded as
 * undefined can be occasionally useful. For example, this lets us initialize
 * "uninitialized" memory in a way that makes it easier to detect if it is
 * being used, without having to forego warnings from Valgrind.
 *
 * \param addr The address of the first byte to become undefined
 * \param bytes The number of bytes to become undefined
 */
void op_valgrind_set_undefined(void* addr, int bytes);

#endif // VALGRIND

#ifdef __cplusplus

class OpCallStack;

/**
 * \brief Compare two callstacks and return TRUE if they match
 *
 * This is useful for comparing callstacks.
 *
 * \param s1 - first callstack
 * \param s2 - second callstack
 * \param start - start at callstack position
 * \param length - length of callstack
 *
 * \returns TRUE if callstack matches, FALSE otherwise.
 *
 */

BOOL OpMemDebug_IsCallstacksEqual(OpCallStack* s1, OpCallStack* s2, int start, int length);

/**
 * \brief Return a copy of the current callstack.
 *
 * This can be useful for later comparison with other callstacks or printing it out if certain criterias are met.
 *
 */

#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0
OpCallStack* OpMemDebug_GetCallstack(void);

/**
 * \brief Print the given callstack
 *
 * Sometimes it is useful to record a callstack and print it out later if certain criterias are met.
 *
 * \param s - The callstack to print.
 *
 */
#endif // MEMORY_CALLSTACK_DATABASE && MEMORY_ALLOCINFO_STACK_DEPTH > 0

void OpMemDebug_PrintThisCallstack(OpCallStack* s);

#ifdef MEMORY_FAIL_ALLOCATION_ONCE

// Allocations below this will have a chance of failing.
// Use OpSrcSite::SetOOMFailThreshold() to change it.
#define DEFAULT_OOM_FAIL_THRESHOLD 10*1024
#define DEFAULT_BASE_FAIL_CHANCE 2

/**
 * \brief Enable OOM simulation
 *
 * All allocation sites that are executed will have a chance of failing once.
 * Using this in combination with an url player can uncover missing or incorrect OOM handling.
 *
 * \param b - enable/disable
 *
 * \returns The previous value of this flag.
 *
 */

BOOL OpMemDebug_SetOOMFailEnabled(BOOL b);

/**
 * \brief Set the oom fail threshold above which allocations have a chance of failing
 *
 * The lower the value the more problems are found but the less likely these problems are to trigger
 * under normal circumstances (smaller allocations are more unlikely to fail).
 *
 * \param oom_threshold - Allocations above this number of bytes for which simulated OOM may trigger.
 *
 */

void OpMemDebug_SetOOMFailThreshold(int oom_threshold);

/**
 * \brief Get the oom fail threshold above which allocations have a chance of failing
 *
 * Return the current oom fail threshold so you don't have to store it yourself.
 *
 */

int OpMemDebug_GetOOMFailThreshold();

/**
 * \brief Set a tag number
 *
 * All allocations get assigned the current tag number. This can be used when url playing
 * increasing the tag number for each page so you can go back and find out which page a
 * memory leak was triggered on.
 *
 * \param tag - the number to set
 *
 */

void OpMemDebug_SetTagNumber(int tag);

/**
 * \brief Set the call count at which the next allocation will fail
 *
 * When reproducing crash bugs it can sometimes be impractical to reproduce a crash
 * where an allocation should fail that doesn't occur within a reasonable number of
 * calls. Using this function can be easier.
 *
 * \param callnumber - call # which should fail
 *
 */

void OpMemDebug_SetBreakAtCallNumber(int callnumber);

/**
 * \brief Clear the call count for all allocations
 *
 * Usually it is only interesting to know which call failed after the current page started loading.
 *
 */

void OpMemDebug_ClearCallCounts();

#endif // MEMORY_FAIL_ALLOCATION_ONCE

#endif /* __cplusplus */

#endif // MEMORY_FUNDAMENTALS_H
