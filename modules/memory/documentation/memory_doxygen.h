/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Doxygen documentation file
 *
 * This file is only used for documenting the memory module through
 * use of 'doxygen'.  It should not contain any code to be compiled,
 * and should therefore never be included by any other file.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

/**
 * \page mem-todo Known issues and things to remember/consider
 *
 * \li When building debugging builds, the inlined operators new,
 *     delete etc., will not get inlined (to avoid confusing the
 *     debugger, at lest with some compilers).  This may potentially
 *     mess up the use of new/delete in other parts of the executable,
 *     like in the startup code.
 */

/** \mainpage Opera memory module
 *
 * \section mem-status-doc Status
 *
 * This document is attempted updated continously, but some disruptive
 * changes have happened that has caused (some of) the documentation to
 * lag behind the implementation.
 * In other situations, the documentation is written before the
 * implementation, which is missing.
 *
 * \section mem-debug-errors Debugging memory problems
 *
 * Assuming you have memory debugging enabled for your build, and everything
 * is set up to output the diagnostics somewhere you can see it, there are
 * a few things you probably want to know about debugging memory problems:
 *
 * \li \ref mem-debug-breakpoint "Setting a breakpoint" for memory debugging
 * \li \ref mem-debug-tech "The technology used" for memory debugging
 * \li \ref mem-debug-magics "Magic memory values" used by the debugger
 * \li \ref mem-debug-limitations "Limitations" for memory debugging
 * \li How to interpret the \ref mem-debug-callstack "call stacks"
 *
 * \section mem-cs-doc Coding standard documentation
 *
 * The following documentation is mandated by coding standards:
 *
 * \li \ref mem-cs-description "General description" of the memory module
 * \li \ref mem-cs-highlevelapi "high level API description"
 * \li \ref mem-cs-impl "Overall implementation strategy"
 * \li \ref mem-cs-fullapi "Complete API documentation"
 * \li \ref mem-cs-memuse "Memory usage" by the memory module
 *
 * \section Documentation
 *
 * This module provides a framework for memory management in Opera.
 * Some of the features it provides are:
 *
 * \li Efficient allocators for \ref mem-special-purpose "specific usages"
 * \li A framework for \ref mem-memory-debugging "memory debugging"
 * \li An API that \ref mem-api-flexibility "allows flexibility"
 * \li A framework that does not depend on, or conflict with system allocators
 * \li Extensive documentation and hints and tips of most things memory related
 *
 * If you want to know how you can make use of this module in your own
 * code, either core code or platform code:
 *
 * \li API for \ref mem-api-general "general memory allocations"
 * \li API for \ref mem-api-specialized "specialized memory allocations"
 * \li API for \ref mem-api-classdefault "default allocator for classes"
 * \li Explanation of what to consider when selecting a specialized allocator
 *
 * If you are interested in how things work, you may want to look at:
 *
 * \li How to \ref mem-porting "port the memory module to a new platform"
 * \li Overview of the Opera
 *     \ref mem-alloc-structure "memory allocation structure"
 * \li How the memory module is organized
 * \li how the memory module interacts with other modules
 *
 * There is a TODO page with \ref mem-todo "known issues and reminders."
 *
 * Comments to this documentation is much appreciated, send them
 * to mortenro@opera.com.
 */

/**
 * \page mem-debug-breakpoint Setting a breakpoint to catch memory problems
 *
 * If you get diagnostics output suggesting there is a memory problem,
 * you may want to set the following breakpoint to try catch the problem
 * when it happens so you can investigate in the debugger:
 *
 * File: \c modules/memory/src/memory_reporting.cpp
 * Function: \c op_mem_error()
 *
 * This function is called after any diagnostics output is printed that
 * suggests a problem of some sort.
 *
 * \b NOTE: If a memory corruption or late write problem has been reported,
 * the call-stack leading up to the breakpoint may not be very useful as
 * the problem may have happened some time ago.  This is especially true
 * if the call-stack contains the function
 * \c OpMemGuard::RecycleDeadAllocations() as this function is a clean
 * up function that performs checks (and releases) allocations other
 * than the one that is currently being freed/realloced etc.
 *
 * \b NOTE: A memory corruption problem for a free/delete operation may
 * in fact be a double delete - if the first delete happened long before
 * the second delete, and the allocation has been recycled in the mean
 * time.
 */

/**
 * \page mem-debug-magics Magic memory values used by the debugger
 *
 * The memory debugger uses some magic values, or more specifically,
 * some magic "patterns" to help identify memory problems.  These
 * memory patterns are written into memory at various times to help
 * establish the status of the memory, or try to provoke crashes.
 * The memory values used by default are:
 *
 * \li <b>0xc?c?c?c?</b> for freshly allocated memory (which is not cleared)
 * \li <b>0xd?e?a?d?</b> for memory which is now deallocated (and destroyed)
 * \li <b>0xf?f?f?f?</b> for the front guard pattern
 * \li <b>0xe?e?e?e?</b> for the rear guard pattern
 * \li <b>0xd?d?d?d?</b> for the external guard pattern
 *
 * All the <b>?</b> characters indicate that the hex-digit does not take any
 * particular value.  The reason for this is to help identify more
 * memory errors, by having the memory patterns depend on the address on
 * which it resides.  A memory location copied from e.g. one position to
 * another inside the rear guard would be detected since the written
 * location will (probably) have the incorrect signature.
 *
 * The downside of this is that the magic patterns are harder to
 * identify manually.
 */

/**
 * \page mem-debug-tech The technology behind the memory debugging function
 *
 * In order to detect memory problems, all allocations by the Opera
 * sanctioned allocators are made within a protecting "envelope" that
 * adds debugging information and guard bytes.
 *
 * The memory layout of a protected allocation is:
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
 * The "User Data" is the actual allocation, while the guards are just
 * filled with \ref mem-debug-magics "magic values" to help detect memory
 * tramples outside the allocation.  Any of the guards can be configured
 * to have any size, including zero bytes (i.e. no guard at all).
 *
 * The OpMemGuardInfo field in the allocation envelope holds various
 * information about the allocation, like the call-stack and type of
 * allocation.  The ammount of detail recorded in this structure can
 * also be configured.
 */

/**
 * \page mem-debug-limitations Limitations in the memory debugger
 *
 * There are a couple of limitations in the debugger that it can be
 * useful to know about.  They are:
 *
 * \li Corruptions are identified only if the allocation is not yet recycled
 * \li Memory tramples may use a bit pattern that goes undetected
 * \li Double deletes may be reported as corruptions if recycled
 * \li Memory usage increases with memory debugging enabled
 * \li CPU consumption increases with memory debugging enabled
 * \li The process' memory consumption is larger than actual
 * \li In rare cases, the memory debugger may mask crashers
 * \li The error detection probability is dependent on configuration settings
 */

/**
 * \page mem-debug-callstack Interpreting the call-stacks
 *
 * Call stacks are printed with hexadecimal notation, prefixed with the
 * usual \b 0x and identifies the address in the executable.
 * The call-stacks are printed with the leaf function address first.
 *
 * \section mem-debug-callstack-linux Deciphering the call-stack on Linux
 *
 * If the diagnostics output did not include any information other than
 * the hexadecimal program address, the debugger \b gdb can be used to
 * look up the function/file/line of the address.
 *
 * Example: To look up the address <b>0x09123456</b>, use:
 *
 * <code>(gdb) list *0x09123456</code>
 *
 * ... which will print file/line number and list 10 lines surrounding
 * the line in question.
 */

/** \page mem-special-purpose The benefit of special purpose allocators
 *
 * A general allocator can almost always be improved upon in terms of
 * memory overhead, time overhead or fragmentation if a little bit of
 * information about the allocation pattern for a specific object or
 * group of objects is available.
 *
 * By allocating different objects differently depending on their
 * allocation patterns, possible gains can be achieved.  In order to
 * make successful use of a special purpose allocator, the performance
 * of the modified code should be evaluated.
 *
 * Such an evaluation may reveal a non-optimal situation, and iterative
 * changes may have to be performed with new evaluations along the way.
 * When familiar with the available special purpose allocators, this
 * knowledge may influence decisions on data structures and organization
 * to optimize the code for maximum benefit from the specialized
 * allocators.
 *
 * In order to help evaluating how the Opera code makes use of the
 * available memory allocation mechanisms, the available memory
 * debugger will provide guiding information and numbers regarding
 * memory usage.
 */

/** \page mem-api-classdefault Selecting a default allocator for a class
 *
 * It is typically the author of a class who knows how the class can best
 * be allocated.  It is thus desirable to be able to select an allocator
 * for the class, and provide this as a default.
 *
 * <pre>
 * class Foobar
 * {
 *   public:
 *     Foobar(void);
 *     void OP_CLASSALLOC_SMO(void);  // Made to look like a member
 * };
 * </pre>
 *
 * In the above, the <code>OP_CLASSALLOC_SMO()</code> macro expands to
 * something like:
 *
 * <pre>
 *     __unused_opera_member(void);
 *     inline void* operator new(size_t size, TOpAllocNewDefault,
 *                             const char* file, int line, const char* obj)
 *           { return OpMemDebug::NewSMO(size, file, line, obj); }
 * </pre>
 *
 * And a similar variant for the leaving version.  The stuff about the
 * <code>__unused_opera_member(void)</code> is to allow the entry in the
 * class definition to resemble a normal member function declaration, so
 * syntax highlighting and indentation etc. of IDEs will not get (too badly)
 * messed up.
 *
 * In release builds, the inlined oprator new will only include the
 * size and TOpAllocNewDefault (The TOpAllocNewDefault is used for name
 * space separation, and should be optimized away completely when
 * inlining).
 *
 * Special care for document object allocations should be made; existing
 * code relies on operator new for counting how much memory is used for
 * document data.  This is typically done like the following:
 *
 * <pre>
 *     void* operator new(size_t size) { REPORT_MEMMAN_NEW(size); }
 *     void  operator delete(void* ptr, size_t size)
 *                         { REPORT_MEMMAN_DELETE(ptr, size); }
 * </pre>
 *
 * The above code in a class declaration should be replaced with the following
 * if one of the specialized allocators is to be made default:
 *
 * <pre>
 * \#ifdef MEMORY_SMOPOOL_ALLOCATOR (or similar, TBD)
 *     void OP_CLASSALLOCDOC_SMO(void);
 * \#else
 *     // The old code
 *     void* operator new(size_t size) { REPORT_MEMMAN_NEW(size); }
 *     void  operator delete(void* ptr, size_t size)
 *                         { REPORT_MEMMAN_DELETE(ptr, size); }
 * \#endif
 * </pre>
 *
 * The 'DOC' in OP_CLASSALLOCDOC_SMO signals that this is a document
 * object, and will therefore do the memory counting for document
 * objects as before.
 */

/** \page mem-memory-debugging Memory debugging
 *
 * \section mem-debugging-information Collecting information
 *
 * The framework provided by the memory module allows debugging
 * information to be available at runtime about how a memory allocation or
 * deallocation came about.  This is achieved by making use of macros
 * that includes the \c __FILE__ and \c __LINE__ preprocessor
 * directives, recording the call-sequence leading up to the allocation,
 * logging the type of allocation, name of object, etc.
 *
 * The memory debugger is implemented through the \c OpMemDebug
 * and \c OpMemGuard classes.  Not all functionality may be available
 * on all platforms (e.g. full stack-trace information may be
 * unavailable for builds targetting the real hardware).
 *
 * \section mem-debugging-corruption Detecting bad usage, heap corruption etc.
 *
 * The memory module now includes roughly the same functionality as
 * "happy-malloc".  This functionality is activated when memory
 * debugging is enabled.  This functionality will cause memory
 * allocations and releases to be "difficult" in the sense that
 * op_realloc() will allways return a different address, even though
 * this does not neccessarily happen in release builds.
 * op_free() will trash the memory being released, so illegal code
 * constructs relying on op_free() not messing with the data will
 * be more easilly detected, etc.
 *
 * A new functionality is that of enabling "Electric Fence" functionality
 * for some or all objects on the heap or in pools.  When Electric Fence
 * is activated, the virtual memory system is used to place objects in
 * such a way that accessing memory just past the object will cause a
 * memory protection fault (and stop the program on the offending
 * instruction).  Likewise, releasing memory will cause the page that
 * contains the object to be inaccessible, which will also cause a
 * fault on late reads and writes.
 *
 * \section mem-debugging-oom Simulating OOM conditions on desktop
 *
 * The memory module has the ability to operate on a restricted heap,
 * typically implemented by \c dlmalloc() which has limited resources.
 * This heap is used to determine if an allocation should succeed or
 * not.
 *
 * The actual object being allocated is however not placed on the
 * restricted heap; it is allocated on the general heap with memory
 * guards and info accociated with it --- or the object may be allocated
 * using "Electric Fence" protection.
 *
 * This powerful combination allows accurate testing of OOM situations
 * on the desktop, possibly with full memory protection and tracking.
 */

/** \page mem-api-flexibility Flexibility of using macros for allocation API
 *
 * The decision to use macros for the allocation API, and the
 * resulting syntax, was not easy to make.  Several arguments can be
 * made against selecting such a scheme, with the most important ones
 * against probably being those of aesthetics, and lack of familiarity
 * to developers.
 *
 * One strong argument in favor of using macros, is that we can at our
 * own choosing, make the macros create exactly the same code as
 * before.  Thus, technically, nothing is \e lost (this is not completely
 * true - some constructs involving preprocessor directives inside the
 * 'new' statement will have to be changed to work with macros).
 *
 * But we gain a lot, as we may use the macros very freely to our own
 * uses, including that of supplying \c __FILE__, \c __LINE__ and \c \#obj
 * to improve debugging memory allocations.
 *
 * Making all allocations take place using special versions of
 * \c new so we operate in our own "namespace" is one possibility that
 * may be beneficial on many architectures where we don't want to
 * conflict with the system variants of \c new and \c delete.
 *
 * In other cases, we may be able to do things with the macros that
 * was not originally intended.  For instance, the gcc 3 bug with
 * array new allocations that fail on OOM can be fixed in a simple
 * manner by using g++ specific functionality:
 *
 * <pre>
 * \#define OP_NEWA(obj, count)
 *    ({
 *        obj* x = new (TOpAllocNewDefaultA, __FILE__, __LINE__,
 *                      \#obj, count, count) obj[count];
 *        if ( x == (obj *)4 ) x = 0;
 *        x;
 *    })
 * </pre>
 *
 * (The bug is that array allocations of an object with a destructor
 * will cause an extra integer to be allocated in front of the array.
 * This integer holds the number of elements in the array, so the \c
 * delete[] operation can go through the objects and destruct them.
 * The value returned from new is thus 4 bytes into the allocated
 * memory to skip the integer.  But on OOM, these 4 bytes are still
 * added, causing new to return the value of 4, rather than 0).
 */

/** \page mem-alloc-structure Structure of op_malloc, OP_NEW and friends
 *
 * There are a few macros/functions used in the Opera code to allocate
 * memory.  The ones listed in the table below are the central standard-C
 * allocator in a few versions for different usage:
 *
 * <table>
 *
 * <tr><td>Function</td><td>Usage</td></tr>
 *
 * <tr><td><code>op_malloc() / op_free()</code></td>
 *   <td>The official, general memory allocator to be used by most code
 *   in Core (and possibly by platform code). This is probably a macro
 *   that may expand into something that includes debugging information,
 *   or maps to <code>op_lowlevel_malloc()</code> for release builds.
 *   This macro is controlled by core code, depending on current
 *   settings for memory debugging, release builds etc.</td></tr>
 *
 * <tr><td><code>op_lowlevel_malloc() / op_lowlevel_free()</code></td>
 *   <td>This is the fundamental allocator that Opera uses, and the system
 *   must provide it or Opera provides its own (DL-malloc).  It may be a
 *   define that maps to the native <code>malloc</code>, or it may be a
 *   macro that calls <code>dlmalloc</code> or <code>MALLOC</code> for 
 *   example.  The main goal for this function is to provide a fundament
 *   for <code>OP_NEW</code>, <code>OP_DELETE</code>, <code>op_malloc</code>
 *   along with the various other related functions, e.g. calloc, realloc,
 *   strdup etc. This differentiation between <code>op_malloc()</code> and
 *   <code>op_lowlevel_malloc()</code> is done to allow memory debugging by
 *   core code on top of the system allocator.</td></tr>
 *
 * <tr><td><code>op_lowlevel_malloc_leave()</code></td>
 *   <td>Same as \c op_lowlevel_malloc() except that a <code>LEAVE</code> is
 *   performed upon OOM.  This version is mainly used by the inlined
 *   <code>operator new</code> methods that are expected to leave when it
 *   fails (e.g. \c OP_NEW_L).  This is a utility function intended to help
 *   save footprint.</td></tr>
 *
 * <tr><td><code>op_debug_malloc() / op_debug_free()</code></td>
 *   <td>This will typically be the same as op_malloc() but with some
 *   differences.  In constrained memory builds, this may be mapped
 *   to a different allocator than the constrained one, to keep debugging
 *   data from interfering with the normal operation (e.g. the debugging
 *   data may be huge compared to the constrained heap being tested).
 *   On devices with limited memory, like a phone or game console, some
 *   memory may be set aside for debugging.  This memory would be used
 *   by e.g. scope, the Ecmascript debugger etc.</td></tr>
 *
 * <tr><td><code>system_malloc() / system_free()</code></td>
 *   <td>System allocator (e.g. \e not our own DL-malloc or pooling
 *   allocators).  These are used when we need to use the system allocator,
 *   maybe to allocate something we hand over to some external library that
 *   will free it using the native free() function of the system, or
 *   maybe we use it to allocate (some of) the memory we need to bootstrap
 *   our own DL-malloc.  The usage of these functions are extremely rare,
 *   so unless you know that you desperately need them, don't use
 *   them.</td></tr>
 *
 * </table>
 *
 * The memory hierarchy can best be described through charts.  The
 * following chart illustrates the allocation mechanisms that create
 * objects on the regular heap (no specialized allocators).  Only the
 * \e allocation variant is illustrated; \c op_realloc, \c op_calloc,
 * \c op_free and \c OP_DELETE are similar, but not included here:
 *
 * \dot
 * digraph heap_allocators {
 *	rankdir=TB
 *	node [shape=box, fontname=Helvetica, fontsize=10]
 *
 *
 *  op_malloc [shape="ellipse"];
 *	op_malloc -> op_lowlevel_malloc;
 *	op_malloc -> "OpDebug::OpMalloc";
 *  "OpDebug::OpMalloc" -> "OpMemGuard::OpMalloc";
 *	"OpMemGuard::OpMalloc" -> op_lowlevel_malloc;
 *
 *  OP_NEW_L [shape="ellipse"];
 *	OP_NEW_L -> "operator new (HEAP_L)";
 *  "operator new (HEAP_L)" [style="rounded"];
 *	"operator new (HEAP_L)" -> op_lowlevel_malloc_leave;
 *	"operator new (HEAP_L)" -> "OpMemDebug::Malloc_L";
 *	"OpMemDebug::Malloc_L" -> "OpMemGuard::OpMalloc";
 *	op_lowlevel_malloc_leave -> op_lowlevel_malloc;
 *
 *  OP_NEW [shape="ellipse"];
 *	OP_NEW -> "operator new (HEAP)";
 *  "operator new (HEAP)" [style="rounded"];
 *	"operator new (HEAP)" -> op_lowlevel_malloc;
 *	"operator new (HEAP)" -> "OpMemDebug::Malloc";
 *	"OpMemDebug::Malloc" -> "OpMemGuard::OpMalloc";
 *
 *  op_lowlevel_malloc [shape="ellipse"];
 *  MALLOC [shape="ellipse"];
 *	op_lowlevel_malloc -> MALLOC;
 *	op_lowlevel_malloc -> dlmalloc;
 *	op_lowlevel_malloc -> malloc;
 * }
 * \enddot
 *
 * In the above, the rectangular boxes are functions, the ellipsis are
 * macroes that just expands to whatever they are calling (and are therefore
 * "free" in the sense of calling overhead).  The boxes with rounded edges are
 * inlined, so they are not "free", but efficient.  The \c OpMemDebug methods
 * are not included in the callpath unless debugging memory functionality is
 * enabled (at compile time).
 *
 * The next chart illustrates how the specialized allocators are organized.
 * They are very similar to the heap allocators above (with the same
 * conventions for the chart symbols). The leaving variants are excluded
 * to keep the size down:
 *
 * \dot
 * digraph pooling_allocators {
 *	rankdir=TB
 *	node [shape=box, fontname=Helvetica, fontsize=10]
 *
 *
 *  OP_NEWSMO [shape="ellipse"];
 *	OP_NEWSMO -> "operator new (SMO)";
 *  "operator new (SMO)" [style="rounded"];
 *	"operator new (SMO)" -> "OpMemPools::NewSMO";
 *	"operator new (SMO)" -> "OpMemDebug::NewSMO";
 *	"OpMemDebug::NewSMO" -> "OpMemPools::NewSMO";
 *
 *  OP_NEWELO [shape="ellipse"];
 *	OP_NEWELO -> "operator new (ELO)";
 *  "operator new (ELO)" [style="rounded"];
 *	"operator new (ELO)" -> "OpMemPools::NewELO";
 *	"operator new (ELO)" -> "OpMemDebug::NewELO";
 *	"OpMemDebug::NewELO" -> "OpMemPools::NewELO";
 *
 *  OP_NEWELS [shape="ellipse"];
 *	OP_NEWELS -> "operator new (ELS)";
 *  "operator new (ELS)" [style="rounded"];
 *	"operator new (ELS)" -> "OpMemPools::NewELS";
 *	"operator new (ELS)" -> "OpMemDebug::NewELS";
 *	"OpMemDebug::NewELS" -> "OpMemPools::NewELS";
 *
 *	"OpMemPools::NewSMO" -> BLKPOOL;
 *	"OpMemPools::NewELO" -> BLKPOOL;
 *	"OpMemPools::NewELS" -> BLKPOOL;
 *	MANPOOL -> BLKPOOL;
 *	custom  -> BLKPOOL;
 *
 *	BLKPOOL -> "mmap (UNIX)";
 *	BLKPOOL -> "RHeap (Symbian)";
 *	BLKPOOL -> "VirtualAlloc (Win32)";
 * }
 * \enddot
 */

/** \page mem-api-general API for general allocations
 *
 * General allocations are placed on a heap, with a general allocator
 * like the system \c malloc() or \c dlmalloc().
 *
 * \section mem-api-general-malloc op_malloc - Allocate on the heap
 *
 * The \c op_malloc() macro acts like the standard C library \c malloc()
 * function.  This allocator is best suited for medium to large blocks
 * of memory, or allocations that are dynamic in size and duration, or
 * for few allocations of a given type.
 *
 * Memory allocated this way must be released with op_free().
 *
 * Example:
 * <pre>
 * char* p = op_malloc(100);
 * if ( p != 0 ) {
 *     p[0] = 1;   // OK
 *     p[99] = 1;  // OK
 *     p[100] = 1; // illegal (out of bounds)
 *     op_free(p); // OK
 *     op_free(p); // illegal (double free)
 * } else {
 *     op_free(p); // FIXME: Is this ok?
 * }
 * </pre>
 *
 * \section mem-api-general-new OP_NEW - Create a new object
 *
 * OP_NEW() and OP_NEW_L() will create an object, either on the heap,
 * or with one of the specialized allocators if the class designer has
 * determined that the class is suitable for allocation by one of the
 * specialized allocators, and made provisions for making this the
 * default.
 *
 * Whether the object is created on the heap or not is thus dependent
 * on the object to be created.
 *
 * OP_NEW() will return NULL on failure, while OP_NEW_L() will \c LEAVE.
 *
 * The following syntax is used to allocate two example objects:
 * <pre>
 * OpAuditorium* a = OP_NEW_L(OpAuditorium,());   // LEAVE on failure
 * a->Empty();
 * OpMagicTrick* m = OP_NEW(OpBlackMagic,("invisible")); // return 0 on failure
 * if ( m != 0 )
 *     m->PerformTrick(a);
 * OP_DELETE(m);   // OK, even when m == 0
 * OP_DELETE(a);
 * </pre>
 *
 * \section OP_NEWA
 *
 * The \c OP_NEWA() and \c OP_NEWA_L() macros are similar to \c
 * OP_NEW() above, but designed for allocating arrays.  The arguments
 * to the macro are the type to create, and the number of elements in
 * the array:
 *
 * <pre>
 * OpMozart* va = OP_NEWA(OpMozart, 10);  // OK, Array of 10 OpMozart objects
 * OpMozart* m = OP_NEWA(OpMozart, somefunc(t++));  // illegal (side effects)
 * f[9].Compose();
 * OP_DELETEA(va);   // Don't confuse with OP_DELETE(va)!
 * </pre>
 *
 * \b Note: Do \e not use an expression with side effects for the
 * \c count argument.  This argument may be computed several times inside
 * the macro, the number of times depending on debug configuration
 * etc., so unpredictable results would result in this case.
 *
 * \section OP_NEWAA
 *
 * The \c OP_NEWAA() and \c OP_NEWAA_L() macros are similar to \c
 * OP_NEWA() above, but designed for allocating 2D arrays.  The arguments
 * to the macro are the type to create, and the width and height of the
 * array. The width and height are multiplied to check for overflow in the
 * total number of array elements.
 * If the multiplication overflows, or the allocation fails, NULL is returned
 * (in the _L version, LEAVE(OpStatus::ERR_NO_MEMORY) is invoked):
 *
 * <pre>
 * OpMozart (*va)[5] = OP_NEWAA(OpMozart, 10, 5);  // OK, 2D array of 10 by 5 OpMozart objects
 * OpMozart (*m)[3] = OP_NEWA(OpMozart, (size_t)-1, 3);  // Returns NULL (overflow)
 * va[9][4].Compose();
 * OP_DELETEA(va);   // Don't confuse with OP_DELETE(va)!
 * </pre>
 *
 * \b Note: Do \e not use an expression with side effects for the
 * \c width or \c height arguments. These arguments may be computed
 * several times inside the macro, the number of times depending on
 * debug configuration etc., so unpredictable results would result
 * in this case.
 *
 * \section OP_NEWA_WH
 *
 * The \c OP_NEWA_WH() and \c OP_NEWA_WH_L macros are equivalent to \c
 * OP_NEWA() above, but designed for allocating 1D arrays whose size is
 * the product of two unsigned integers). The arguments to the macro are
 * the type to create, and the width and height of the array. The width
 * and height are multiplied to create the actual number of elements in
 * the array. If the multiplication overflows, NULL is returned
 * (in the _L version, LEAVE(OpStatus::ERR_NO_MEMORY) is invoked):
 *
 * <pre>
 * OpMozart* va = OP_NEWA_WH(OpMozart, 10, 5); // OK, Array of 50 objects
 * OpMozart* m = OP_NEWA_WH(OpMozart, (size_t)-1, 5); // Returns NULL (overflow)
 * va[9].Compose();
 * OP_DELETEA(va);   // Don't confuse with OP_DELETE(va)!
 * </pre>
 *
 * \b Note: Do \e not use an expression with side effects for the
 * \c width or \c height arguments. These argument may be computed
 * several times inside the macro, the number of times depending on
 * debug configuration etc., so unpredictable results would result
 * in this case.
 *
 * \section OP_NEWA_WHD
 *
 * The \c OP_NEWA_WHD() and \c OP_NEWA_WHD_L macros are equivalent to \c
 * OP_NEWA() above, but designed for allocating 1D arrays whose size is
 * the product of three unsigned integers). The arguments to the macro are
 * the type to create, and the width, height and depth of the array. The
 * width, height and depth are multiplied to create the actual number of
 * elements in the array. If the multiplication overflows, NULL is returned
 * (in the _L version, LEAVE(OpStatus::ERR_NO_MEMORY) is invoked):
 *
 * <pre>
 * OpMozart* va = OP_NEWA_WHD(OpMozart, 10, 5, 4); // OK, Array of 200 objects
 * OpMozart* m = OP_NEWA_WHD(OpMozart, (size_t)-1, 2, 2); // Returns NULL (overflow)
 * va[9].Compose();
 * OP_DELETEA(va);   // Don't confuse with OP_DELETE(va)!
 * </pre>
 *
 * \b Note: Do \e not use an expression with side effects for the
 * \c width, \c height or \c depth arguments. These argument may be
 * computed several times inside the macro, the number of times
 * depending on debug configuration etc., so unpredictable results
 * would result in this case.
 */

/** \page mem-api-specialized API for specialized allocations
 *
 * The specialized allocators have different behaviours, and differing
 * semantics.  The specialized allocators are for this reason not
 * always interchangeable without some work.
 *
 * \section mem-api-specialized-abrev Abreviations used for allocators
 *
 * Many abreviations are used for the allocators, the following table
 * list the ones it is useful to know about.
 *
 * <table>
 * <tr><td>Abreviation</td><td>Meaning</td></tr>
 * <tr><td>SMO</td><td>Small to Medium Object (allocator)</td></tr>
 * <tr><td>ELO</td><td>Equal Lifetime Object (allocator)</td></tr>
 * <tr><td>ELSA</td><td>Equal Lifetime String Array (allocator)</td></tr>
 * <tr><td>_L</td><td>LEAVEs on failure, instead of returning NULL</td></tr>
 * <tr><td>T</td><td>Temporary object; short life expectancy</td></tr>
 * <tr><td>P</td><td>Persistent object; long life expectancy</td></tr>
 * <tr><td>A</td><td>Array allocation</td></tr>
 * </table>
 *
 * \section mem-api-smo SMO - Small to medium sized object allocator
 *
 * The OP_NEWSMO() class of allocators can be used very freely.  They
 * don't require special setup.  There is a preconfigured limit (with
 * possible exceptions) to how large objects should be allocated with
 * this allocator.  Typically no more than 256 bytes for a single
 * object.
 *
 * The complete class of allocators include: \c OP_NEWSMO(), \c OP_NEWSMO_L(),
 * \c OP_NEWSMOT(), \c OP_NEWSMOT_L(), \c OP_NEWSMOP(), \c OP_NEWSMOP_L().
 *
 * Note: For extremely short lived allocations that are \e guaranteed
 * to last only a short while, consider using the ELO allocator with a
 * special global lifetime handle instead of using \c OP_NEWSMOT() or
 * \c OP_NEWSMOT_L().
 *
 * Example:
 *
 * <pre>
 * // Opera has a lot of options, and they last a long time
 * OpOption* p = OP_NEWSMOP(OpOption,("-fullscreen"));
 * ...
 * OP_DELETE(p);
 * </pre>
 *
 * \section mem-api-elo ELO - Equal lifetime object allocator
 *
 * The OP_NEWELO() class of allocators can be used very efficiently,
 * but both care and planning is needed.  You need to have an
 * OpMemLifetime object available that you use when allocating
 * objects.  This OpMemLifetime object is used as a handle to identify
 * objects that will live equally long.
 *
 * Example:
 *
 * <pre>
 * {
 *    OpMemLifetime merry_men;
 *    OpMemLifetime bad_guys;
 *
 *    for ( ... ) {
 *       OpFriarTuck* ft = OP_NEWELO(OpFriarTuck,("lace card"), merry_men);
 *       OpRobinHood* rh = OP_NEWELO(OpRobinHood,("card split"), merry_men);
 *       OpSherrif *sn = OP_NEWELO(OpSherrif,(ft,rh), bad_guys);
 *    }
 *    destroy_the_sherrifs();
 * }
 * // time goes by
 * bury_the_good_guys();
 * </pre>
 *
 * Explanation:
 *
 * There are two classes of life expectancy here, the \c merry_men and
 * the \c bad_guys.  It is very important that all objects belonging
 * to each of these two life expectancy classes are indeed of the
 * exact same life expectancy.
 *
 * When all the \c OpFriarTuck and \c OpRobinHood objects gets
 * created, they are co-located in the \c merry_men life expectancy
 * group.  This also means that all the \c OpFriarTuck and \c
 * OpRobinHood objects should be deleted at the same time.
 *
 * The \c OpSherrif objects gets created into the \c bad_guys life
 * expectancy group.  This means that the \c OpSherrif objects can be
 * deleted at another time than the \c OpFriarTuck and \c OpRobinHood
 * objects, but all the \c OpSherrif objects must be deleted at the
 * same time.
 *
 * \b Note: The \c OpMemLifetime object only needs to exist until the
 * allocations are done.  It can then either go out of scope, be
 * deleted or made to have a \c OpMemLifetime::FreshStart().
 * However, a significant ammount of allocations should have taken place
 * during this time.
 *
 * On average, half the memory block size will be wasted
 * (typically 2K wasted), and this should be taken into consideration
 * when deciding to use the ELO or ELS allocator.
 * As a general rule, when allocating objects, at lest 20-50 KB should
 * be allocated with the same lifetime in order to make the most
 * efficient use of the memory blocks.  When allocating strings,
 * a total of 170 strings or more should be allocated in order to be
 * more efficient (on average) than DL-malloc.
 *
 * Special global \c OpMemLifetime handles that can be used by
 * enyone:
 *
 * <pre>
 * OpMemLifetime g_mem_short;     // Extremely short life expectancy
 * OpMemLifetime g_mem_permanent; // Life expectancy of entire Opera runtime
 * </pre>
 *
 * The \c g_mem_short can be used for allocations that will go away
 * very quickly, like during this message being processed.
 *
 * The \c g_mem_permanent life expectancy group, on the other hand, holds
 * objects that are allocated during Opera startup or shortly afterwards,
 * and only gets deleted towards the very end when Opera is shutting down.
 * It is important that no objects, or a very small percentage at the most,
 * gets deleted long before Opera is shut down.
 *
 * The \c SMO allocators include: OP_NEWELO(), OP_NEWELO_L().
 *
 * \section mem-api-els ELS - Equal lifetime string allocator
 *
 * The \c OP_NEWAELS() and \c OP_NEWAELS_L() allocators are close to
 * identical to OP_NEWELO() and OP_NEWELO_L(), except they can only be
 * used for allocation of string arrays:
 *
 * <pre>
 * OpMemLifetime string_collection;
 * char* str1 = OP_NEWELSA(char, 10, string_collection);
 * uni_char* str2 = OP_NEWELSA(uni_char, 10, string_collection);
 * </pre>
 *
 * It is possible, and in most cases desirable, to use the same
 * \c OpMemLifetime for both OP_NEWELO() and OP_NEWELSA().  But keep in
 * mind that every time an object is allocated after a string, some
 * bytes may potentially be lost due to alignment padding. E.g.
 *
 * <pre>
 * OpMemLifetime x;
 * char* a = OP_NEWELSA(char, length_a, x);
 * char* b = OP_NEWELSA(char, length_b, x);
 * OpFoo* f1 = OP_NEWELO(OpFoo,("foo 1", a), x);
 * OpFoo* f2 = OP_NEWELO(OpFoo,("foo 2", b), x);
 * </pre>
 *
 * Is a little bit more efficient than:
 *
 * <pre>
 * OpMemLifetime x;
 * char* a = OP_NEWELSA(char, length_a, x);
 * OpFoo* f1 = OP_NEWELO(OpFoo,("foo 1", a), x);
 * char* b = OP_NEWELSA(char, length_b, x);
 * OpFoo* f2 = OP_NEWELO(OpFoo,("foo 2", b), x);
 * </pre>
 *
 * Because objects and strings are both laid out one after the other
 * in a memory block for the same life expectancy, and in the less
 * efficient example, padding may have to be applied before allocating
 * both f1 and f2.
 */

/** \page mem-config-matrix Configuration implications matrix
 *
 * For the various configurations possible, the following matrix indicates
 * which configuration options affects what behaviour in the memory module.
 *
 * \b NOTE: This table is incomplete!!!
 *
 * <table>
 * <tr><td>1</td><td>TWEAK_MEMORY_NAMESPACE_OP_NEW</td></tr>
 * <tr><td>2</td><td>TWEAK_MEMORY_REGULAR_OP_NEW</td></tr>
 * <tr><td>3</td><td>TWEAK_MEMORY_IMPLEMENT_OP_NEW</td></tr>
 * <tr><td>4</td><td>TWEAK_MEMORY_INLINED_OP_NEW</td></tr>
 * <tr><td>5</td><td>TWEAK_MEMORY_REGULAR_GLOBAL_NEW</td></tr>
 * <tr><td>6</td><td>TWEAK_MEMORY_IMPLEMENT_GLOBAL_NEW</td></tr>
 * <tr><td>7</td><td>TWEAK_MEMORY_INLINED_GLOBAL_NEW</td></tr>
 * <tr><td>8</td><td>TWEAK_MEMORY_REGULAR_NEW_ELEAVE</td></tr>
 * <tr><td>9</td><td>TWEAK_MEMORY_IMPLEMENT_NEW_ELEAVE</td></tr>
 * <tr><td>10</td><td>TWEAK_MEMORY_INLINED_NEW_ELEAVE</td></tr>
 * <tr><td>11</td><td>TWEAK_MEMORY_REGULAR_ARRAY_ELEAVE</td></tr>
 * <tr><td>12</td><td>TWEAK_MEMORY_IMPLEMENT_ARRAY_ELEAVE</td></tr>
 * <tr><td>13</td><td>TWEAK_MEMORY_INLINED_ARRAY_ELEAVE</td></tr>
 * <tr><td>14</td><td>TWEAK_MEMORY_REGULAR_GLOBAL_DELETE</td></tr>
 * <tr><td>15</td><td>TWEAK_MEMORY_IMPLEMENT_GLOBAL_DELETE</td></tr>
 * <tr><td>16</td><td>TWEAK_MEMORY_INLINED_GLOBAL_DELETE</td></tr>
 * <tr><td>17</td><td>TWEAK_MEMORY_INLINED_PLACEMENT_NEW</td></tr>
 * </table>
 *
 * <table>
 * <tr>
 *   <td>Construct</td>
 *   <td>1</td>
 *   <td>2</td>
 *   <td>3</td>
 *   <td>4</td>
 *   <td>5</td>
 *   <td>6</td>
 *   <td>7</td>
 *   <td>8</td>
 *   <td>9</td>
 *   <td>10</td>
 *   <td>11</td>
 *   <td>12</td>
 *   <td>13</td>
 *   <td>14</td>
 *   <td>15</td>
 *   <td>16</td>
 *   <td>17</td>
 * </tr>
 * <tr>
 *   <td>operator new(size_t)</td>
 *      <td>X</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 *      <td>-</td>
 * </tr>
 * </table>
 */

////////////////////////// QA good above this line ////////////////////////////

/* FIXME: Part of old mainpage
 *
 * \section mem-howtouse How to use memory?
 *
 * This section explains what needs to be done to successfully apply
 * memory for some allocations, or firmly decide that memory is
 * not suited for a certain use case.  The process of finding the
 * right use of memory can be an iterative process, and may change
 * over time as object usage changes.
 *
 * \li Consult the \ref mem-classify page to classify your usage.
 * \li Select an API from \ref mem-implementation.
 */

/** \page mem-classify Classifying your object/memory usage
 *
 * To successfully apply specialized memory allocators to your code,
 * you will have to know how you use memory.  The following questions
 * will help determine what kind of allocation is best suited to your
 * usage pattern.
 *
 * \b Note: Every single type of allocation should be investigated
 * separately, and sometimes the same type of allocation may have
 * different characteristics depending on its expected usage.  In
 * these cases, the different usage should be taken into consideration
 * when allocating the object (and possibly allocate it differently
 * depending on usage).
 *
 * \li Are the allocations for data or objects?
 * \li Are the allocations of variable size?
 * \li What is the size of the allocations?
 * \li How many allocations do you perform?
 * \li Is the lifespan for the objects the same?
 * \li Can the allocation be moved about in memory?
 * \li Can the allocations be administrated by the target code?
 *
 * These questions reflect more or less the ones in \ref
 * mem-sutabilitytable.  This table is usefull to decide what
 * allocators may be suitable for a particular allocation pattern.  In
 * addition, it may be interesting to consider:
 *
 * \li What is the expected lifespan of the allocations?
 * \li Should the allocation be made in \ref mem-fastmemory?
 * \li Does your object have a destructor?
 */

/** \page mem-fastmemory Fast memory
 *
 * Some platforms like the Nintendo Wii have "fast memory" available.
 * It is smaller but faster than the normal memory.  Other
 * architectures are known to exist with similar hardware
 * configurations.  This memory may have special requirements like
 * alignment, but by storing timing critical data here, we may be able
 * to speed things up a bit.
 *
 * Allocation of timing critical objects can be marked as such, and
 * thereby allocate the object in the fast memory on devices where
 * this is possible.
 *
 * \c FIXME: There is no API for this yet.
 */

/** \page mem-implementation Specialiced allocators available
 *
 * All fundamental allocators in \c memory are listed below.  There
 * may be different ways to make use of them, some may for example
 * depend on a contex or allocation class to correctly group objects
 * belonging together.
 *
 * \li \ref mem-smopool "SMOPOOL" - Small to Medium sized Object pooling
 * \li \ref mem-elopool "ELOPOOL" - Equal Lifetime Object pooling
 * \li \ref mem-elopool "ELSPOOL" - Equal Lifetime String pooling
 * \li \ref mem-blkpool "BLKPOOL" - Block memory pooling
 * \li \ref mem-manpool "MANPOOL" - Managed memory pooling
 */

/** \page mem-s60problems Symbian S60 system allocator problems
 *
 * It is slooooow with many objects on the heap.  It searches its heap
 * linearly for every malloc and free, and causes massive CPU
 * consumption and cache polution with 50k+ objects on the heap.
 *
 * 
 *
 * A seemingly very bad usage pattern was also observed between the
 * system libraries and Opera, so moving many Opera objects out of
 * the system heap reduced both the CPU consumption and the level
 * of fragmentation on the system heap.
 */

/** \page mem-memoryblock Using a standardized memory block size
 *
 * In order to get the best possible cooperation between modules with
 * differing memory requirements, using a single standardized block
 * size for all memory allocations would be a valuable tool.
 *
 * Deciding on the block size is unfortunately not easy.
 *
 * Some modules may desire large block sizes for efficiency reasons
 * (cache, image decoder, document data,...) while others would enjoy
 * smaller block sizes for fragmentation reasons (e.g. the Ecmascript
 * heap).
 *
 * The initial Symbian S60 pooling implementation used a 16K block
 * size.
 *
 * For instance, most virtual memory implementations uses a 4K memory
 * page size, which on some operating systems can be allocated and
 * deallocated individually with holes (Windows, Linux).  Making use
 * of such a feature would help with freeing memory back to the
 * operating system.
 */

/** \page mem-sutabilitytable Table of suitable allocators
 *
 * This table attempts to predict which allocators will be suitable
 * for a specific case.  Only use this as a guide, your mileage may
 * vary.
 *
 * Interpretation: Try to find an allocator that has the fewest 'No'
 * for the conditions matching your allocation characteristics.
 * For same number of 'No', pick the one with the most 'Yes'.
 *
 * <table>
 *
 * <tr>
 *   <td></td>
 *     <td>STDMALLOC</td>
 *     <td>\ref mem-smopool "SMOPOOL"</td>
 *     <td>\ref mem-elopool "ELOPOOL"</td>
 *     <td>\ref mem-elopool "ELSPOOL"</td>
 *     <td>\ref mem-blkpool "BLKPOOL"</td>
 *     <td>\ref mem-manpool "MANPOOL"</td>
 * </tr>
 *
 * <tr>
 *   <td>Object allocations</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>String allocations</td>
 *     <td>Yes</td>
 *     <td>-</td>
 *     <td>-</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Variably sized allocations</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Small sized allocations</td>
 *     <td>-</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 * </tr>
 *
 * <tr>
 *   <td>Medium sized allocations</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 * </tr>
 *
 * <tr>
 *   <td>Large sized allocations</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Many objects</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Few objects</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Same lifespan</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Exact same lifespan</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 * </tr>
 *
 * <tr>
 *   <td>Fixed address allocation</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>Yes</td>
 *     <td>No</td>
 * </tr>
 *
 * <tr>
 *   <td>Data organized inside blocks</td>
 *     <td>Yes</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>No</td>
 *     <td>Yes</td>
 *     <td>No</td>
 * </tr>
 *
 * </table>
 */

/** \page mem-smopool The SMOPOOL allocator
 *
 * \c SMOPOOL: Small to Medium size Object pooling allocator
 *
 * This allocator will allocate a pool-block, and hand out allocations
 * from it that are all of the same size.
 *
 * The allocation procedure is therefore more or less:
 *
 *  - Look for a pool reserved for \c SMOPOOL use for the
 *    object size requested.
 *  - If one is found with room to spare, allocate from
 *    this pool.
 *  - If no room can be found, reserve a new pool for
 *    use by \c SMOPOOL for the object size requested.
 *
 * When freeing an object allocated with \c SMOPOOL, mark the
 * allocation as available, and if the whole pool is vacated, release
 * the pool.
 */

/** \page mem-elopool The ELOPOOL/ELSPOOL allocator
 *
 * \c ELOPOOL: Equal Lifetime Object allocator \n
 * \c ELSPOOL: Equal Lifetime String allocator
 *
 * This allocator distinguishes itself by allowing to allocate
 * different sized objects in a single pool-block efficiently, but under the
 * one condition that all objects are allocated and freed at the same
 * time.
 *
 * This is suitable for certain objects in locale that stays allocated
 * as long as opera is running, for instance.
 *
 * The pool-block is only freed when all objects have been freed, and
 * objects in the pool-block can't be reused when freed.
 *
 * This limitation dictates the equal lifetime restriction, as this
 * will assure the pool-block is indeed freed in a timely fashion.
 * \b Note: Not a single object must be left behind!
 *
 * In order to assure the lifetime condition, all allocations must use
 * a handle to determine with which other objects it has equal
 * lifetime.
 *
 * Some default handles exists for e.g. the complete lifetime of
 * Opera, for use by permanent data like some in locale, or new
 * handles can be created and updated as needed, on e.g. document load
 * or reflow basis.
 *
 * The \c ELSPOOL allocator is almost identical to \c ELOPOOL, except
 * it guarantees 2 byte alignment suitable for unicode and char
 * strings.  \c ELOPOOL on the other hand guarantees 4 or 8 byte
 * alignment, depending on the requirements of the architecture (and
 * thus may waste a bit more memory if used for string allocations).
 *
 * \b Note: If you do both object and string allocations from the
 * same life expectancy handle, they will be allocated from the same
 * memory block.  Depending on the allocation pattern, padding may be
 * required before allocating an object, which wastes space.
 *
 * In the event that you have more than about 5000 strings that you
 * will allocate together with objects, you may want to use \e two
 * life expectancy handles, one for \c ELOPOOL and one for \c ELSPOOL.
 * This will cause object and string allocations to be made from
 * different memory blocks.
 *
 * The reason why this only should be done for more than 5000 strings
 * is utilization: With fewer strings, the gain is less than the
 * average overhead associated with half-full memory blocks.
 */


/** \page mem-blkpool The BLKPOOL allocator
 *
 * \c BLKPOOL: The block memory allocator.
 *
 * The \c BLKPOOL allocator gives access to the raw memory blocks that
 * all allocators use.  The blocksize that can be allocated is fixed,
 * typically at 4K, 8K or 16K depending on the platform.
 *
 * Memory allocated with the \c BLKPOOL allocator will be of the exact
 * Opera memory block size, which is a power of two.
 *
 * When using the \c BLKPOOL allocator, the user is responsible for
 * organizing the data inside the memory block.  An example usage of
 * this allocator could be the Ecmascript engine that administrates
 * its own objects inside the memory blocks used to contain the
 * Ecmascript heap.
 *
 * The other allocators will use \c BLKPOOL to allocate the raw memory
 * they need for their operation.
 */

/** \page mem-manpool The MANPOOL allocator
 *
 * \c MANPOOL: The Managed memory allocator.
 *
 * The managed memory allocator maintains an arbitrary large block of
 * memory, but access to it may only be performed through an interface
 * that has strict requirements on usage.
 *
 * The reason for this is that the underlying memory blocks used to
 * represent the allocations may move about in memory, and may have
 * non-contigous representation.
 *
 * For this reason, users of this class needs to closely observe the
 * API and adhere strictly to it.  In debugging builds, this allocator
 * may move memory about when possible just to provoke illegal use of
 * its API (keeping pointers for too long or assuming contigous
 * allocations).
 */

/** \page mem-api The API used to access the allocators
 *
 * There are some goals that have been attempted reached with the
 * following API.  Those are:
 *
 * \li Flexibility (can be used without too much hassle)
 * \li Good defaults (defaults can be made for individual classes)
 * \li Added debugging functionality through \c __FILE__ and \c __LINE__
 * \li Better overview through naming of objects and classification
 *
 * \section mem-apioverviewalloc API overview; allocating and deallocating
 *
 * The \c OP_NEW macro is used instead of the regular \c new operator.
 * It has the advantage of allowing Opera to designate a special new
 * operator if needed, and do usefull things with it like:
 *
 * <pre>
 * \#define OP_NEW(obj, args)
 *       new (OpAllocNewDefault, __FILE__, __LINE__, \#obj) obj args
 * </pre>
 *
 * Which will allow:
 *
 * <pre>
 *   OpSomeObject* k = OP_NEW(OpSomeObject,(42, "answer"));
 * </pre>
 *
 * Similarly:
 *
 * <pre>
 * \#define OP_DELETE(x)
 *        do { OpLogDelete(__FILE__, __LINE__); delete x; } while (0)
 * </pre>
 *
 * So one uses <code>OP_DELETE(foobar);</code> instead of <code>delete
 * foobar;</code> There is a macro \c OP_NEW_L that \e leaves on
 * failure, and other variants to specify the allocation in more
 * detail.  One particular complication is for array allocations and
 * deletions:
 *
 * <pre>
 * \#define OP_NEWA(obj, count)
 *       new (OpAllocNewA, __FILE__, __LINE__, \#obj, count, sizeof(obj))
 *          obj[count]
 *
 * \#define OP_DELETEA(x)
 *        do { OpLogDeleteA(__FILE__, __LINE__); delete[] x; } while (0)
 * </pre>
 *
 * This will make it possible to track array allocations accurately.
 *
 * \b Note: Release builds will have a minimum definition for these
 * macros that operates correctly, but without the logging.
 *
 * \section mem-apioverviewtable API overview, different allocators
 *
 * The following macros can be used to allocate memory using various
 * different allocators (and the standard allocator).
 *
 * <table>
 *
 * <tr>
 *   <td></td>
 *     <td>STDMALLOC</td>
 *     <td>\ref mem-smopool "SMOPOOL"</td>
 *     <td>\ref mem-elopool "ELOPOOL"</td>
 *     <td>\ref mem-elopool "ELSPOOL"</td>
 *     <td>Delete with</td>
 * </tr>
 *
 * <tr>
 *   <td>Object allocations</td>
 *     <td><code>OP_NEW</code></td>
 *     <td><code>OP_NEWSMO</code></td>
 *     <td><code>OP_NEWELO</code></td>
 *     <td>-</td>
 *     <td><code>OP_DELETE</code></td>
 * </tr>
 *
 * <tr>
 *   <td>Object array allocations</td>
 *     <td><code>OP_NEWA</code></td>
 *     <td>-</td>
 *     <td>-</td>
 *     <td>-</td>
 *     <td><code>OP_DELETEA</code></td>
 * </tr>
 *
 * <tr>
 *   <td>String allocations</td>
 *     <td><code>OP_NEWS</code></td>
 *     <td>-</td>
 *     <td>-</td>
 *     <td><code>OP_NEWELS</code></td>
 *     <td><code>OP_DELETES</code></td>
 * </tr>
 *
 * </table>
 */

/**
 * \brief The actuall allocator
 *
 * This function or macro is the one that will invoke the real allocator,
 * which could be either the platform \c malloc, the Brew \c MALLOC macro,
 * the Opera internal \c dlmalloc function, or some other.
 *
 * \param size Number of bytes to allocate
 * \returns Pointer to allocated memory, or NULL on error
 */
void* op_lowlevel_malloc(size_t size);

/**
 * \brief The actuall deallocator
 *
 * Memory allocated with \c op_lowlevel_malloc() should be freed by this
 * function.  Trying to free a NULL pointer should be ignored and cause
 * no harm.
 */
void op_lowlevel_free(void* ptr);

/**
 * \brief The native system allocator
 *
 * This function or macro will call the system allocator, e.g. not the
 * \c dlmalloc function.  This can be used in the extremely seldom cases
 * where you need access to the system allocator (maybe a library
 * requires you to use it).
 *
 * Memory allocated by this function/macro must be released with the
 * system_free() function.
 */
void system_malloc(size_t size);

/**
 * \brief The native system deallocator
 *
 * Frees memory allocated with system_malloc().
 */
void system_free(void* ptr);
