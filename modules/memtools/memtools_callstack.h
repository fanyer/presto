/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Header file for objects to handle callstacks
 * 
 * This file provides an API towards collecting call-stack information.
 * The call-stack information is stored once, and later referenced
 * through a pointer to the \c OpCallStack object.  A database of such
 * objects is maintained through the \c OpCallStackManager class.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMTOOLS_CALLSTACK_H
#define MEMTOOLS_CALLSTACK_H

#ifdef MEMTOOLS_CALLSTACK_MANAGER

class OpCallStack
{
public:
	OpCallStack(const UINTPTR* stack, int size, int id);

	UINT32 GetID(void) const { return id; }
	int GetSize(void) const { return size; }
	UINTPTR GetAddr(int level) const { return stack[level]; }

	void AnnounceCallStack(void);

	static void* operator new(size_t size) OP_NOTHROW { return op_debug_malloc(size); }
	static void  operator delete(void* ptr) { op_debug_free(ptr); }

	UINTPTR* GetStack(void) const { return stack; }
	UINTPTR  GetSiteAddr(void) const { return stack[0]; }

private:
	friend class OpCallStackManager;

	OpCallStack* next;
	UINTPTR* stack;
	int size;
	UINT32 id;
	UINT32 status;
};

class OpCallStackManager
{
public:
	OpCallStackManager(int hashtable_size);

	static OpCallStack* GetCallStack(const UINTPTR* stack, int size);

	static void* operator new(size_t size) OP_NOTHROW { return op_debug_malloc(size); }
	static void  operator delete(void* ptr) { op_debug_free(ptr); }

	// int GetCount(void) { return next_id - 1; }

private:
	int hashtable_size;
	OpCallStack** hashtable;
	UINT32 next_id;
};

#endif // MEMTOOLS_CALLSTACK_MANAGER

#endif // MEMTOOLS_CALLSTACK_H
