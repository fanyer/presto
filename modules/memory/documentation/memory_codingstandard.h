/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
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
 * \page mem-cs-description General description
 *
 * \section mem-cs-goals Goals
 *
 * The memory module attempts to bring three important aspects together:
 *
 * \li Portability
 * \li Debugging memory
 * \li Memory pooling (yet TBD)
 *
 * Documentation is also a very important aspect, as the memory architecture
 * of a platform may differ significantly from "normal", and is typically a
 * challenge for most new ports.  Documenting how the fundamental memory
 * allocations in Opera are performed will hopefully aid new ports in making
 * the right decisions for memory management.
 *
 * \section mem-cs-what What does the memory module do?
 *
 * The memory module is primarily concerned with providing memory
 * allocation methods to core code (or any code including "core/pch.h")
 * and translate these requests into sensible requests to the operating
 * system.
 *
 * For many operations, like op_malloc(), the memory module may in release
 * builds do as little as implement the operation as a macro that calls
 * op_lowlevel_malloc() to perform the job.  The op_lowlevel_malloc() can
 * be chosen according to what the platform provides, by the platform teams.
 *
 * For other operations, the memory module may have a much more significant
 * implementation to offer, like pooling.  The pooling code will typically
 * have a platform independent implementation in the memory module, and
 * rely on platform specific code to support pooling.
 *
 * Also, the memory module will do a lot of internal housekeeping for
 * memory debugging builds, i.e. builds where the memory module tries to
 * identify illegal uses of memory, the amount of memory used etc.
 */

/**
 * \page mem-cs-highlevelapi High level description of the APIs
 *
 * The memory module provides, or helps provide, functions used to
 * administrate memory usage.  These APIs are intended to be used by
 * virtually all of the opera core code, and possibly platform code,
 * and includes, but are not limited to:
 *
 * \li op_malloc()
 * \li op_calloc()
 * \li op_realloc()
 * \li op_free()
 * \li operator new()
 * \li operator new[]()
 * \li operator new (ELeave)
 * \li operator new[] (ELeave)
 * \li operator delete()
 * \li operator delete[]()
 *
 * There are other APIs that are designed for the administration of
 * the memory module, accessible through the \c OpMemDebug class.
 */

/**
 * \page mem-cs-impl Implementation strategy
 *
 * \section mem-cs-impl-tweaks The usage of tweaks
 *
 * Tweaks are very important to the memory module. This allows flexibility
 * so different projects can use the memory module in different configurations
 * without changing any code (at least in theory).  All the tweaks makes the
 * memory module one of the most complicated ones to configure from scratch,
 * as informed decisions are needed for many of the tweaks, depending on
 * how the memory architecture of the platform is to be utilized and
 * integrated with.
 *
 * Many tweaks have no default values for any profile.  This is intentional
 * as the memory architecture of the target device is independent of the
 * core profile used as a base.  Where there are defaults, they are typically
 * suited for a desktop environment.
 *
 * \section mem-cs-impl-operators The declaration of the memory operators
 *
 * operator new() and operator delete() and related ones may typically
 * be configured by three binary decisions:
 *
 * \li Should operator new() etc. be declared in \c core/pch.h?
 * \li Should operator new() etc. be implemented by the memory module?
 * \li should operator new() etc. be declared inline in \c core/pch.h?
 *
 * This gives flexibility, as a platform may e.g. have operator new()
 * declared in core/pch.h (in \c modules/memory/memory_fundamental.h
 * actually), but decide not to inline or implement it.  The
 * implementation can now be done on the platform itself.
 * Inlining can be used if the versions provided by the memory module
 * are suitable and desirable, but everything can be done by the platform
 * if they so wishes.
 *
 * \b NOTE: Not all combinations of declarations and implementations may
 * make sense (like declaring and declaring inline may cause compile errors).
 *
 * \section mem-cs-impl-malloc op_malloc() etc.
 *
 * The \c op_malloc() and friend functions, that performs the same job
 * as the standard library functions \c malloc() etc. are actually
 * implemented as macros.
 * This allows for both flexibility and performance.
 *
 * \section mem-cs-impl-oom Limited heap emulation
 *
 * By giving the '-mem' option to all core debugging builds, and the
 * phone, smart-phone and TV profiles in release builds, a limited heap
 * is created where allocations are made to emulate a device with a
 * small heap.
 *
 * During memory debugging, the allocations themselves are actually not
 * made on this limited heap, but on the system heap.  This is so that
 * allocations can be padded with guard bytes and extra information like
 * call-site and allocation method.  The allocations may also not be
 * reused right away after freeing, to help catch late writes, double
 * frees and other corruptions.  In this scenario, the limited heap is
 * used to decide if the allocation should succeed or not, but the
 * actual limited allocation is not used (only tracked).
 */

/**
 * \page mem-cs-fullapi Complete APIs for the memory module
 *
 * \section mem-cs-fullapi-official The official memory allocation API
 *
 * The official memory allocation APIs is described in
 * \ref mem-cs-highlevelapi "highlevel description of allocation APIs"
 * and provides op_malloc(), operator new(), operator delete() etc.
 *
 * The official memory allocation API is expected to be extended with
 * support for pooling and co-locating, to help improve performance and
 * reduce memory footprint.  \b NOTE: There are some code in the memory
 * module for these new APIs, but they should \b not be used, yet.
 *
 * \section mem-cs-fullapi-external APIs available externally
 *
 * The OpMemDebug class is the central class for memory debugging, and
 * has public methods to be used by core code or platform code
 * (NOTE: This class also has public methods that should not be called
 * directly, as they provide the functionality for other, official APIs).
 *
 * The OpMemEventListener can be used to create a class that receives
 * OpMemEvent events.  This can be useful for hooking into the memory
 * module to get info about allocations taking place (the internal
 * class OpMemReporting does exactly this).  Be careful that the listener
 * will be called from within the allocation functions themselves, so
 * don't allocate memory or call complicated APIs from it...
 *
 * \section mem-cs-fullapi-internal APIs used internally in memory module
 *
 * The following classes are used for internal housekeeping and should
 * not be accessed from outside the memory module:
 *
 * \li OpMemGuard deals with corruption detection and limited heap
 * \li OpMemGuardInfo holds the info stored alongside each allocation
 * \li OpMemoryState holds the state info for memory and lea_malloc modules
 * \li OpAllocInfo is used to propagate allocation details around
 * \li OpMemReporting is a user of OpMemEventListener for error reporting
 * \li OpMemUpperTen is used for status printouts from the Actions menu
 * \li OpSrcSite identifies a source code line
 */

/**
 * \page mem-cs-memuse Usage of memory by the memory module
 *
 * \section mem-cs-memuse-static Static memory usage
 *
 * The memory state is kept somewhere in memory, pointed at by the
 * \c g_opera_memory_state macro.  This macro may be defined by the
 * memory module if global variables is configured through tweak
 * \c TWEAK_MEMORY_USE_GLOBAL_VARIABLES.
 *
 * The \c lea_malloc module will use the memory state also for its
 * global data needed to administrate the heap.
 *
 * The static memory usage is 1K for 32-bit builds, and 2K for 64-bit
 * builds.  This is more than is actually needed, but some overhead is
 * used to not have to depend on the configuration settings of
 * \c lea_malloc for finding the size (complicates the code).
 * There is instead a function that can be called to assert that enough
 * static memory has been set aside.
 *
 * If OOM emulation is enabled and used (default for phone, smart-phone and
 * TV profiles), the size of the limited heap is allocated on start-up
 * (for the core builds), and stays allocated.  The size of this limited
 * heap is the total size of the configured heap plus 4K of overhead.
 * This one-time allocation which may be huge will not contain any data,
 * so dynamic allocations made by opera will come on top of the limited heap.
 *
 * \section mem-cs-memuse-release Heap memory usage for "release" builds
 *
 * When memory debugging is disabled, the memory module is very shallow,
 * and without any pooling activated, almost no dynamic allocations will
 * happen.  All allocations will be on behalf of the caller.
 *
 * The title has "release" in quotes, as what is meant is whether memory
 * debugging is enabled or not.  Many of the core profiles have memory
 * debugging enabled in release builds.
 *
 * \section mem-cs-memuse-debug Heap memory usage for "debug" builds
 *
 * With memory debugging enabled, the memory module will perform many
 * allocations for housekeeping purposes, like recording call-stacks.
 * All of these house-keeping allocations are performed outside the limited
 * heap (if configured), so the limited heap is not polluted with debugging
 * overhead information.
 *
 * The allocations on the heap will be significantly bigger than the actual
 * object allocated, depending on settings for guard bytes and other
 * meta-data about the allocation (call-stack).
 *
 * When an allocation is released, the memory module may not release the
 * object right away, depending on settings, in order to catch memory
 * corruptions better.  This will cause the observed memory usage to be
 * higher than the actual allocated data, both due to bigger overhead
 * per allocation, and due to more objects allocated (compared to release
 * builds).
 *
 * \section mem-cs-memuse-oompolicy OOM Policy
 *
 * The allocation APIs will either return NULL or perform a LEAVE on OOM,
 * depending on which API function is called.
 *
 * For memory state initialization, a flag is used to inform of OOM
 * to the surrounding system.  For memory debugging builds, it is
 * currently assumed that a few allocations during start-up will not fail.
 * This is not the case for release builds.
 *
 * For some supplemental APIs used to get call-stacks etc., certain
 * objects are statically allocated and handed out as "OOM objects" if
 * the real object could not be allocated.  This will result in e.g.
 * a call-site information that informs of OOM, rather than the real
 * file and line number.
 *
 * \section mem-cs-memuse-oomwho Who is handling OOM?
 *
 * For all the allocation APIs provided by the memory module, the caller
 * is responsible for handling the OOM.
 * For OOM in the internal data-structures used in the memory module,
 * the memory module itself should handle the OOM.
 *
 * \section mem-cs-memuse-codeflow Description of flow
 *
 * Most API functions in the memory module are shallow, and will be
 * called from the outside, with a OOM return either through NULL or
 * LEAVE.
 *
 * When an allocation is demanded, the memory module will first decide
 * if the allocation should succeed (for limited heap scenarios), and
 * then make the allocation with overhead to spear for the meta-info
 * about call-stack, type of allocation etc.
 *
 * The call-stack is allocated dynamically right there and then, if the
 * call-stack has not been seen before.  If the allocation of the call-stack
 * fails, a pre-formatted static call-stack is used instead, that contains
 * data identifying a failed call-stack allocation due to OOM.  The
 * reporting will thus be inaccurate, but no ill effects should be
 * observed.
 *
 * \section mem-cs-memuse-stack Stack memory usage
 *
 * The memory module does not do any recursive calling, and the call-stack
 * is typically shallow, so a few hundred bytes would be the expected
 * maximum stack usage.
 *
 * This usage may increase a bit for memory debugging builds, depending
 * on how the call-stack capture mechanism is implemented.  On Windows
 * and Linux, there will typically be a UINTPTR allocated for each
 * level of call-stack that is configured.  For 32 level call-stack, the
 * increased stack-usage will be about 128 bytes on 32-bit machines.
 *
 * \section mem-cs-memuse-caching Caching and freeing memory
 *
 * The freed objects that are awaiting final destruction could be
 * considered a "cache", but there is currently no way to influence
 * or force a cleaning of these allocations.  Since these objects are
 * on the outside of the limited heap, freeing them will not
 * make room for more allocations in the limited heap anyway.
 *
 * \section mem-cs-memuse-exitfreeing Freeing memory on exit
 *
 * This is currently a bit problematic, as the allocators may be used
 * by the system itself during shutdown (depending on configuration),
 * so there is no precise point in time where deallocations could
 * happen.
 *
 * This is a problem for debugging builds, where memory debugging overhead
 * allocations are very dynamic in nature. For release builds, no such
 * additional dynamic allocations are made so if the Opera core code does
 * not leak, the memory module should not leak either, but this needs to
 * be watched.
 *
 * \section mem-cs-memuse-tempbuffers Temp buffers
 *
 * The memory module does not use any temp buffers.
 *
 * \section mem-cs-memuse-tuning Memory tuning
 *
 * The memory module can tune its memory overhead for memory debugging
 * builds through tweaks.
 *
 * \section mem-cs-memuse-tests Tests
 *
 * The self-tests are very simplistic, but tries to illustrate how the
 * allocation functions should be used and what to expect.
 * As smoke test for the memory module, www.vg.no is used.
 *
 * \section mem-cs-memuse-coverage Coverage
 *
 * Test coverage is typically good for release builds (where test
 * coverage is measured), since the module is very shallow and does
 * not have many entry points.  Actually, just having an empty
 * unit-test will give decent coverage, since the self-test system
 * is depending on the memory module for its doings...
 *
 * For debugging builds, the coverage is probably less favorable, but
 * whether this is a problem or not (for memory debugging builds) is
 * probably open to discussion.
 *
 * \section mem-cs-memuse-decisions Design choices
 *
 * For release builds, minimum overhead and performance comes second
 * only to flexibility (e.g. you may select a configuration that is less
 * efficient than the optimal), but the optimal configuration can't be
 * made more optimal (e.g. no run-time decisions, indirections etc.).
 * We only allocate as much memory as requested...
 *
 * For memory debugging builds, we are not concerned about memory overhead
 * usage.  We are also much less concerned about CPU usage.  It is more
 * important to discover problems and make debugging them easier, rather
 * than use little memory and go fast.
 *
 * \section mem-cs-memuse-suggestions Suggestions for improvements
 *
 * Memory pooling is on the agenda, and will probably have a significant
 * impact for some projects where the system allocator is inefficient.
 *
 * More efficient "grouping" allocators will help reduce CPU consumption
 * during start-up, and reduce fragmentation (use memory more efficiently).
 * This will require analysis and implementation, but might yield
 * decent results.
 */
