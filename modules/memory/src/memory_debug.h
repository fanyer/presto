/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file for OpMemDebug object
 * 
 * You must include this file if you want to access any of the
 * memory debugger functions directly.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_DEBUG_H
#define MEMORY_DEBUG_H

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_opallocinfo.h"

enum OpMemDebugAction
{
	MEMORY_ACTION_SET_MARKER_T1,
	MEMORY_ACTION_SET_MARKER_T2,
	MEMORY_ACTION_SET_MARKER_M3_ALL,
	MEMORY_ACTION_SET_MARKER_M3_SINGLE,
	MEMORY_ACTION_SET_MARKER_M3_AUTO,
	MEMORY_ACTION_SET_MARKER_M3_NOAUTO,
	MEMORY_ACTION_SET_MARKER_M4_ALL,
	MEMORY_ACTION_SET_MARKER_M4_SINGLE,
	MEMORY_ACTION_SHOW_ALL,
	MEMORY_ACTION_SHOW_BEFORE_T1,
	MEMORY_ACTION_SHOW_BETWEEN_T1T2,
	MEMORY_ACTION_SHOW_AFTER_T2,
	MEMORY_ACTION_COUNT_NOT_M3,
	MEMORY_ACTION_ENABLE_LOGGING,
	MEMORY_ACTION_FORKED,
	MEMORY_ACTION_MEMORY_DUMP
};

enum OpMemDebugActionInteger
{
	MEMORY_ACTION_SET_OOM_COUNT,
	MEMORY_ACTION_SET_MEMORY_MAX_ERROR_COUNT
};

/**
 * \brief The central memory debugger API
 *
 * This class defines the memory debugging API, and also glues a lot
 * of the allocation logic together.  Much of the actual memory debugger
 * operations happen in OpMemGuard which tries to guard the memory and
 * warn when there are problems.
 *
 * There are also quite a bit of glue-code in \c memory_fundamentals.cpp
 * which acts as a wrapper for the OpMemDebug class.  This is needed
 * since the memory API needs to be simple, so it can be made available
 * through \c memory_fundamentals.h very early during compilation.
 */
class OpMemDebug
{
public:
	void* operator new(size_t size) OP_NOTHROW { return op_lowlevel_malloc(size); }
	void operator delete(void* ptr) { op_lowlevel_free(ptr); }

	/** \name Ordinary C-style allocation debugging API
	 *
	 * The following methods will perform the actions of the ordinary
	 * "C-like" allocation functions of \c op_malloc(), \c op_calloc(),
	 * \c op_realloc(), \c op_free() and \c op_strdup().
	 *
	 * @{
	 */
	void* OpMalloc(size_t size, OpAllocInfo* info);
	void* OpMalloc_L(size_t size, OpAllocInfo* info);
	void* OpCalloc(size_t num, size_t size, OpAllocInfo* info);
	void* OpRealloc(void* ptr, size_t size, OpAllocInfo* info);
	void  OpFree(void* ptr, void** pptr, OpAllocInfo* info);
	void  OpFreeExpr(void* ptr, OpAllocInfo* info);
	char* OpStrdup(const char* str, OpAllocInfo* info);
	uni_char* UniStrdup(const uni_char* str, OpAllocInfo* info);
	/* @} */

	/** \name Pooling allocator debugging API
	 *
	 * The following methods will cause the correspoinding pooling
	 * allocator or other suitable allocator(s) to be used to create
	 * the objects.
	 *
	 * @{
	 */
	void* NewSMO(size_t size, OpAllocInfo* info);
	void* NewELO(size_t size, OpMemLife &life, OpAllocInfo* info);
	void* NewELO_L(size_t size, OpMemLife &life, OpAllocInfo* info);
	void* NewELSA(size_t size, OpMemLife &life, OpAllocInfo* info);
	void* NewELSA_L(size_t size, OpMemLife &life, OpAllocInfo* info);
	/* @} */


	/** \name Regular global operator new debugging API
	 *
	 * When the default global operator new allocates memory on the heap,
	 * these methods should be used to track the allocations correctly.
	 *
	 * @{
	 */
	void* GlobalNew(size_t size);
	void* GlobalNew_L(size_t size);
	void* GlobalNewA(size_t size);
	void* GlobalNewA_L(size_t size);
	/* @} */

	int Action(enum OpMemDebugActionInteger action, int value);
	int Action(enum OpMemDebugAction action, void* misc = 0);

	BOOL GetRandomOOM(void);
	void SetRandomOOM(BOOL value);

private:
	int dummy;  // Non-zero sized object

	void* SetupMemGuard(void* ptr, size_t size, OpAllocInfo* info);
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // MEMORY_DEBUG_H
