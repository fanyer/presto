/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file for OpMemGuard and OpMemGuardInfo objects
 *
 * When doing allocations, it is possible to make the system pad all
 * allocations before and after the actual data that gets used.
 * Later, this padding or "guard" bytes can be checked to decide if they
 * have survived intact.  If they have not, we have an indication that
 * some object nearby, probably the one we guard, have gone outside its
 * allocation.
 *
 * This can be a valuable tool in tracking down heap corruption.
 *
 * Auxilary data can also be stored in the guard-bytes surrounding the
 * object, like allocation sequence number, file and line number,
 * a linked list to track all allocations, type of allocation, status
 * of allocation.
 */

#ifndef _MEMORY_GUARD_H
#define _MEMORY_GUARD_H

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_debug.h"
#include "modules/memory/src/memory_opallocinfo.h"
#include "modules/memory/src/memory_mmap.h"

#include "modules/util/adt/opvector.h"

#ifdef MEMORY_CALLSTACK_DATABASE
#  include "modules/memtools/memtools_callstack.h"
#endif

class OpMemReporting;
class OpMemEfenceClassname;

/**
 * \name Memory filler values for wrong memory usage detection
 *
 * The following defines lists what values are written into memory to
 * help detect illegal usages of memory.
 *
 * The value written into memory is perturbed by using the address
 * where the guard word is to be written to create a modifier value
 * that is bitwize and'ed by 0x0f0f0f0f and then xor'ed with patterns
 * below.  This causes consequtive guard words to differ, which gives
 * better protection from assignments from bad memory to bad memory.
 *
 * @{
 */

/**
 * \brief Values filled into freshly allocated memory
 *
 * When calling \c op_malloc(), \c OP_NEW() etc., the memory returned
 * will have the following value pre-initialized.  The value is
 * deliberately not zero, or the previous content, to help provoke
 * crashes for illegal uses.
 */
#define MEMORY_NEW_PATTERN				0xc0c0c0c0

/**
 * \brief Values filled into released memory
 *
 * When memory is released, and should thus not be used any longer,
 * the following value is written to the released memory region to
 * catch late uses of the memory.
 *
 * The pattern 'd?e?a?d?' (dead) will turn up as a consequence.
 */
#define MEMORY_FREE_PATTERN				0xd0e0a0d0

/**
 * \brief Front guard protection value
 *
 * The front guard is the memory set aside right before the allocation
 * to detect corruptions right below the allocation.  This define holds
 * the value used for the front guard.
 */
#define MEMORY_FRONT_PATTERN			0xf0f0f0f0

/**
 * \brief Rear guard protection value
 *
 * Just like the front guard protection value, except it is placed right
 * after the allocation.  Note: This value is placed on a 32-bit word
 * boundary, independently of the size of the allocation.  If the allocation
 * is not a multiple of 4 bytes, only parts of the value is used to
 * guard the bytes just above the allocation.
 */
#define MEMORY_REAR_PATTERN				0xe0e0e0e0

/**
 * \brief External guard protection value
 *
 * Right before the front guard, there is an info area that contains
 * information about the allocation, like size and allocation site.
 * Before this info, the external guard is located.  Its job is to detect
 * overwrites from below, which may fx. be caused by a malloc meta-data
 * corruption.
 */
#define MEMORY_EXTERNAL_PATTERN			0xd0d0d0d0

/* @} */

#define MEMORY_MEMGUARD_HEADER_MAGIC	0x6cebe34e

/**
 * \brief Allocation info associated with a given allocation
 *
 * Whenever the OpMemGuard class is active to provide safeguards agains
 * memory corruption, the allocations have a layout in memory like the
 * following:
 *
 * \dot
 * digraph structs {
 *   node [shape=record];
 *
 *   area1 [label="External guard|<f1>OpMemGuardInfo|Front guard|\
 *          <f2>User Data|Rear guard"];
 * }
 * \enddot
 *
 * Any of the External, Front and Rear guards may be of zero size, in
 * which case they don't offer any effective guard, but there will be
 * less memory overhead which may be desirable for small devices.
 *
 * The OpMemGuardInfo can be configured, and its size will thus also
 * depend on the configuration.  All OpMemGuardInfo objects are chained
 * up in one of the doubly linked list anchors \c live_allocations
 * and \c dead_allocations, in the OpMemGuard class.
 *
 */
struct OpMemGuardInfo
{
	//
	// This union is here only to have an unused double in the
	// OpMemGuardInfo structure, since this will assure the size to be
	// a multiple of 8 for architectures that require this for
	// doubles, hence causing the data area also be aligned suitably
	// to hold doubles.
	//

	union {
		struct {
			UINT32 magic;
			UINT32 flags;
		} bits;

		double alignment_guarantee;
	};

	OpMemGuardInfo* prev;
	OpMemGuardInfo* next;

	const char* object;

#ifdef ENABLE_MEMORY_OOM_EMULATION
	void* constrained_allocation;
#endif

#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	OpCallStack* allocstack_new;
#  else
	UINTPTR allocstack[MEMORY_KEEP_ALLOCSTACK];
#  endif
#endif

#if MEMORY_KEEP_REALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	OpCallStack* reallocstack_new;
#  else
	UINTPTR reallocstack[MEMORY_KEEP_REALLOCSTACK];
#  endif
#endif

#if MEMORY_KEEP_FREESTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	OpCallStack* freestack_new;
#  else
	UINTPTR freestack[MEMORY_KEEP_FREESTACK];
#  endif
#endif

#if MEMORY_KEEP_ALLOCSITE
	UINT32 allocsiteid;
#endif
#if MEMORY_KEEP_REALLOCSITE
	UINT32 reallocsiteid;
#endif
#if MEMORY_KEEP_FREESITE
	UINT32 freesiteid;
#endif

	UINT32 size;

	void InsertBefore(OpMemGuardInfo* target);
	void Unlink(void);
	UINTPTR GetAddr(void) const;
	UINTPTR GetPhysAddr(void) const;
	UINT32 GetSize(void) const { return size; }
	const char* GetObject(void) const { return object; }

	BOOL IsArray(void) const;

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	int tag;
#endif // MEMORY_FAIL_ALLOCATION_ONCE
};

/** \name Flags identifying memory allocation operation and history
 *
 * Most info regarding the type of allocation and its status is
 * stored in a single 32-bit flag variable.  This flag is located in
 * the OpMemGuardInfo object associated with each allocation.
 *
 * The flag is also provided through the OpAllocInfo object which
 * identifies the origins of the allocation (e.g. malloc vs. new).
 *
 * For the usage tagging of MEMORY_FLAG_ALLOCATED_* and
 * MEMORY_FLAG_RELEASED_* an enumeration scheme is selected that
 * uses one of the following values:
 *
 * 0x07, 0x0b, 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
 * 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x31, 0x32, 0x34, 0x38.
 *
 * These values have the properties that bitwise or'ing any
 * two of these values will cause a value different from all
 * of the above.  This will help catch problems as or'ing is
 * used to accumulate information into the flag variable.
 * (The values are all values of a 6 bit field where 3 bits
 * are 1 and 3 bits are 0).
 * @{
 */

#define MEMORY_FLAG_REALLOCATED_SHRUNK			0x00000001
#define MEMORY_FLAG_REALLOCATED_SAME			0x00000002
#define MEMORY_FLAG_REALLOCATED_GROWN			0x00000004
#define MEMORY_FLAG_IS_REALLOCATED				0x00000007
#define MEMORY_FLAG_REALLOCATED_MULTIPLE		0x00000008
#define MEMORY_FLAG_REALLOCATED_FAILURE			0x00000010
#define MEMORY_FLAG_EFENCE_ALLOCATION			0x00000020

#define MEMORY_FLAG_ALLOCATED_OP_MALLOC			0x00700000
#define MEMORY_FLAG_ALLOCATED_OP_NEW			0x00b00000
#define MEMORY_FLAG_ALLOCATED_OP_NEWA			0x00d00000
#define MEMORY_FLAG_ALLOCATED_SYSTEM_MALLOC		0x00e00000
#define MEMORY_FLAG_ALLOCATED_GLOBAL_NEW		0x01300000
#define MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA		0x01500000
#define MEMORY_FLAG_ALLOCATED_OP_NEWSMOT		0x01600000
#define MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD		0x01900000
#define MEMORY_FLAG_ALLOCATED_OP_NEWSMOP		0x01c00000
#define MEMORY_FLAG_ALLOCATED_OP_NEWELO			0x02300000
#define MEMORY_FLAG_ALLOCATED_OP_NEWELSA		0x02500000
#define MEMORY_FLAG_ALLOCATED_OP_CALLOC			0x02600000
#define MEMORY_FLAG_ALLOCATED_OP_STRDUP			0x02900000
#define MEMORY_FLAG_ALLOCATED_UNI_STRDUP		0x02a00000
#define MEMORY_FLAG_ALLOCATED_MASK				0x03f00000

#define MEMORY_FLAG_RELEASED_OP_FREE			0x00007000
#define MEMORY_FLAG_RELEASED_DELETE				0x0000b000
#define MEMORY_FLAG_RELEASED_DELETEA			0x0000d000
#define MEMORY_FLAG_RELEASED_SYSTEM_FREE		0x0000e000
#define MEMORY_FLAG_RELEASED_REALLOCATED		0x00013000
#define MEMORY_FLAG_RELEASED_POOLED_DELETE		0x00015000
#define MEMORY_FLAG_RELEASED_MASK				0x0003f000

#define MEMORY_FLAG_IS_REPORTED					0x00040000
#define MEMORY_FLAG_STATUS_TMP1					0x00080000

#define MEMORY_FLAG_MARKER1						0x10000000
#define MEMORY_FLAG_MARKER2						0x20000000
#define MEMORY_FLAG_MARKER3						0x40000000
#define MEMORY_FLAG_MARKER4						0x80000000

/* @} */


/** \name Eventclass for different memory events
 *
 * All memory events belong to exactly one of the eventclasses below.
 * The mask values can be used to identify subset(s) of classes.
 *
 * @{
 */

#define MEMORY_EVENTCLASS_ALLOCATE			0x00000001
#define MEMORY_EVENTCLASS_FREE				0x00000002
#define MEMORY_EVENTCLASS_OOM				0x00000004
#define MEMORY_EVENTCLASS_MISMATCH			0x00000008
#define MEMORY_EVENTCLASS_LATEWRITE			0x00000010
#define MEMORY_EVENTCLASS_CORRUPT_BELOW		0x00000020
#define MEMORY_EVENTCLASS_CORRUPT_ABOVE		0x00000040
#define MEMORY_EVENTCLASS_CORRUPT_EXTERN	0x00000080
#define MEMORY_EVENTCLASS_DOUBLEFREE		0x00000100
#define MEMORY_EVENTCLASS_LEAKED			0x00000200

#define MEMORY_EVENTCLASS_ERRORS_MASK		0x000003f8

/* @} */

#ifdef MEMORY_ELECTRIC_FENCE
#define EFENCE_MMAP_SEGMENT_LIMIT			128
#define EFENCE_MMAP_SEGMENT_SIZE			(128 * 1024 * 1024)
#endif

class OpMmapSegment;

/**
 * \brief Administrate allocations with guards and supplemental info
 *
 * When \c ENABLE_MEMORY_DEBUGGING functionality is enabled, all allocations
 * will go through an OpMemGuard object for special treatment.  Basically,
 * allocations will be made on the heap, with extra room to spare, so
 * extra information and memory guards can be set up and maintained (See
 * OpMemGuardInfo class for more info).
 *
 * The OpMemGuard class will also make it possible to have "shadow
 * allocations" where the actual allocation (happening on the heap), has
 * a shadow allocation done either on a different heap of a limited size
 * or through the pooling allocators.
 *
 * By having the shadow allocations, we will be able to observe realistic
 * fragmentation behaviour, out of memory (OOM) behaviour, or pooling
 * co-location efficiency etc. while at the same time also have all
 * allocations guarded by memory guards and full debugging information
 * available.
 */
 class OpMemGuard
{
public:
	OpMemGuard(void);
	void* operator new(size_t size) OP_NOTHROW;

	void Destroy(void);

	void* OpMalloc(size_t size, OpAllocInfo* ai);
	void* OpRealloc(void* ptr, size_t size, OpAllocInfo* ai);
	void  OpFree(void* ptr, OpAllocInfo* ai);
	void* ShadowAlloc(void* ptr, size_t size, OpAllocInfo* info);
	void* Real2Shadow(void* ptr);
	void  ReportLeaks(void);

#ifdef MEMORY_CALLSTACK_DATABASE
	void  ShowCallStack(OpCallStack* callstack);
#else
	void  ShowCallStack(const UINTPTR* stack, int size);
#endif

	void  SetMarker(UINT32 setflags);
	int   Action(OpMemDebugActionInteger action, int value);
	int   Action(OpMemDebugAction action, void* misc);
	void* InfoToAddr(const OpMemGuardInfo* info) const;
	BOOL  GetRandomOOM(void) { return random_oom; }
	void  SetRandomOOM(BOOL value) { random_oom = value; }
	BOOL  IsMemoryLogging(void) { return log_allocations; }

#if defined(ENABLE_MEMORY_OOM_EMULATION) && defined(ENABLE_MEMORY_DEBUGGING)
	void PopulateLimitedHeap(void);
#endif

	void CheckAlloc(void* ptr);

	/** \name Global operator delete tracking API
	 *
	 * The nature of \c operator \c delete makes it impossible to provide
	 * an API that can do the work for us, rather we have to rely on the
	 * \c OP_DELETE() macro calling one of the following methods to let
	 * it be known to the memory debugger that the object in question is
	 * about to get deleted (so when it happens, we have the details
	 * of where and how).
	 *
	 * @{
	 */
#if MEMORY_KEEP_FREESITE
	void TrackDelete(const void* ptr, const char* file, int line);
	void TrackDeleteA(const void* ptr, const char* file, int line);
#endif
	/* @} */

#ifdef MEMORY_ELECTRIC_FENCE
	/**
	 * Request that a class of objects be fenced in.
	 *
	 * @param classname Explicit name of C++ class. Contents will
	 * be copied and caller retains ownership of argument.
	 */
	void AddEfenceClassname(const char *classname);
#endif // MEMORY_ELECTRIC_FENCE

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
	int GetTotalMemUsed() const { return OpSrcSite::GetTotalMemUsed(); }
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

	UINT32 GetAllocSiteID(void* ptr);
	void PrintSiteInfo(UINT32 id);
	UINT32 GetAllocatedAmount(void* ptr);

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	bool IsOOMFailEnabled() { return failenabled; }
	bool SetOOMFailEnabled(bool enable) { bool tmp = failenabled; failenabled = enable; return tmp; }
	void SetOOMFailThreshold(size_t failthreshold) { oomfailthreshold = failthreshold; }
	size_t GetOOMFailThreshold() { return oomfailthreshold; }
	void SetCurrentTag(int tag);
	void SetBreakAtCallNumber(int n) { callnumber = n; }
	void ClearCallCounts();
#endif // MEMORY_FAIL_ALLOCATION_ONCE


private:
#ifdef ENABLE_MEMORY_OOM_EMULATION
	void* AllocateShadow(size_t size, OpAllocInfo* ai);
	void FreeShadow(void* ptr, OpAllocInfo* ai);
#endif

	void* Allocate(size_t size, OpAllocInfo* ai, void* ptr = 0);
	size_t GetTotalSize(size_t size, BOOL use_efence);
	size_t GetRearGuardSize(size_t size, BOOL use_efence);
	OpMemGuardInfo* SetupGuard(void* area, size_t size, BOOL use_efence);
	BOOL CheckIntegrity(OpMemGuardInfo* info, OpAllocInfo* ai = 0);
	void InstallGuard(char* mem, size_t size, UINT32 pattern);
	BOOL CheckGuard(char* mem, size_t size, UINT32 pattern);
	BOOL IsMatchingFree(UINT32 flags);

	void* InfoToArea(OpMemGuardInfo* info);
	OpMemGuardInfo* AddrToInfo(const void* ptr);
	size_t GetAllocSize(void* ptr);
	OpMemGuardInfo* GetGuardInfo(void* ptr);
	void RecycleDeadAllocations(void);
	void RecycleDeadAllocation(void);
	int CountNotM3(void);
	void Retire(OpMemGuardInfo* mgi);

	/**
	 * Decide whether or not to perform allocation with electric fence.
	 *
	 * Make local changes to the definition of this function in order
	 * to track down memory issues. See also the -memfence argument to
	 * coregogi.
	 *
	 * @param size Size of allocation.
	 * @param ai Information on the location and type of the allocation.
	 *
	 * @return TRUE if electric fence should be applied.
	 */
	BOOL UseEfence(size_t size, OpAllocInfo* ai);

#ifdef MEMORY_ELECTRIC_FENCE
	void* try_efence_malloc(OpMmapSegment* mmap, size_t size);
	void* efence_malloc(size_t size);
	void efence_free(void* ptr);
#endif // MEMORY_ELECTRIC_FENCE

	/**
	 * \brief Anchor all currently live allocations
	 *
	 * This is the anchor object for the double linked list containing
	 * all allocations that have not yet been released (are live).
	 *
	 * The double linked list forms a cycle, where this object is part
	 * of the cycle
	 */
	OpMemGuardInfo live_allocations;

	/**
	 * \brief Anchor dead allocations not yet recycled
	 *
	 * This is the anchor object for the double linked list containing
	 * all allocations that have been released, and are put on "hold"
	 * for the sake of detecting double frees, late uses and the like.
	 *
	 * Allocations that gets released will be pulled out of the
	 * 'live_allocations' list, and inserted \e before the 'dead_allocations'
	 * object.
	 *
	 * When time comes to recycle the released objects, they are collected
	 * from \e after the 'dead_allocations' object.
	 */
	OpMemGuardInfo dead_allocations;

	int count_dead_allocations;
	unsigned int bytes_dead_allocations;

	BOOL random_oom;
	BOOL log_allocations;

	int oom_countdown;

	/**
	 * \brief Base flag value for new allocations
	 *
	 * All new allocations will have the value of this variable bitwise
	 * or'ed into the allocation flag variable.  This is used e.g. to
	 * disable leak-checking during parts of selftest internal code.
	 */
	UINT32 initial_flags;

	OpMemReporting* reporting;

	// FIXME: This should be a thread-safe queue of events for threading use
	const void* tracked_delete_ptr;
	const char* tracked_delete_file;
	int tracked_delete_line;

#ifdef MEMORY_ELECTRIC_FENCE
	size_t memory_page_size; ///< Memory page size (typically 4K or 8K)
	size_t memory_page_mask; ///< Mask bits (memory_page_size - 1)

	///< Current OpMmapSegment for electric fence
	int mmap_current;
	///< Electric fence memory segments
	const OpMemory::OpMemSegment* mseg[EFENCE_MMAP_SEGMENT_LIMIT];
	///< Electric fence allocations using mmap
	OpMmapSegment* mmap[EFENCE_MMAP_SEGMENT_LIMIT];

	///< List of object classes to be fenced in
	List<OpMemEfenceClassname> mem_efence_class_names;
#endif

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	bool failenabled;
	size_t oomfailthreshold;
	int currenttag;
	int callnumber;
#endif // MEMORY_FAIL_ALLOCATION_ONCE
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // _MEMORY_GUARD_H
