/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 * \brief Declare OpCodeLocation and OpCodeLocationManager
 *
 * The set of classes declared in this file can be used to translate an
 * address in the executable file into a sourcecode/line number reference.
 *
 * This depends on support from the platform, and parts of these classes
 * will have to be implemented by the platform.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMTOOLS_CODELOC_H
#define MEMTOOLS_CODELOC_H

#ifdef MEMTOOLS_ENABLE_CODELOC

/**
 * \brief Size of the hashtable used to hold addresses in the executable
 *
 * This number probably should be prime, and big enough to not cause
 * too many collisions.
 */
#define CODE_LOCATION_HASHTABLE_SIZE 100003

/**
 * \brief Class to represent a single location in the source code.
 *
 * During debugging, this class can be used to represent or identify
 * a specific location in the source file, based on the address in
 * the executable (typically obtained by looking at the call stack).
 *
 * Use OpCodeLocationManager::GetCodeLocation() to obtain the
 * OpCodeLocation object.  The object may not be fully initialized at
 * that point, so calling the Get-methods on it right away may be a
 * severe performance penalty.
 *
 * OpCodeLocation objects can only be created and destroyed
 * through the OpCodeLocationManager class.
 */
class OpCodeLocation
{
public:
	const char* GetFileLine(void);
	const char* GetFunction(void);
	UINTPTR GetAddr(void) const { return addr; }
	OpCodeLocation* GetInternalNext(void) const { return internal_next; }

	void SetFunction(const char* fn) { func = fn; }
	void SetFileLine(const char* fl) { file = fl; }
	void SetInternalNext(OpCodeLocation* loc) { internal_next = loc; }

	OpCodeLocation(UINTPTR address)
	{
		addr = address;
		file = 0;
		func = 0;
		// hash_next and internal_next is taken care of elsewhere
	}

	static void* operator new(size_t size) OP_NOTHROW { return op_debug_malloc(size); }
	static void  operator delete(void* ptr) { op_debug_free(ptr); }

private:
	friend class OpCodeLocationManager;

	UINTPTR addr;
	const char* file;
	const char* func;
	OpCodeLocation* hash_next;
	OpCodeLocation* internal_next;
};

/**
 * \brief Class to manage OpCodeLocation objects.
 *
 * During debugging, this class can be used to create and
 * manage OpCodeLocation objects.
 *
 * OpCodeLocation objects that are created (and returned by the
 * GetCodeLocation() method) may not be initialized until later
 * due to implementation details.
 *
 * All objects will be placed on a special "initialized" list
 * once they are properly initialized, and can be used.
 * This list can be inspected by calling the GetNextInitialized()
 * method, which will return the initialized objects one by one.
 *
 * This can be used to e.g. create a map-file for all locations
 * encountered previously in an asynchronous manner (the reason
 * for the asynchronous API is e.g. that on Linux, a slave
 * process is used to look up the symbols, with some latency).
 */
class OpCodeLocationManager
{
public:
	OpCodeLocationManager(void);
	virtual ~OpCodeLocationManager(void);

	static OpCodeLocation* GetCodeLocation(UINTPTR addr);
	static OpCodeLocation* GetNextInitialized(void);

	void Sync(void);

	static OP_STATUS Init(void);
	static OP_STATUS InitAndSync();
	static BOOL AnnounceCodeLocation(void);
	static void Flush(void);
	static void Forked(void);

	static void SetDelayedSymbolLookup(BOOL delay_lookup);

	static OpCodeLocationManager* create(void);  // Implemented by platform

protected:
	virtual OP_STATUS initialize(void) = 0;
	virtual OpCodeLocation* create_location(UINTPTR addr) = 0;  // Impl. platf.
	virtual void start_lookup(OpCodeLocation* loc);
	virtual int poll(void);
	virtual void do_sync(void);
	virtual void forked(void);

	BOOL init_ok;
	BOOL delay_init;
	BOOL lazy_init;
	OpCodeLocation* initialized_list;
	OpCodeLocation* initialized_list_last;
	OpCodeLocation* uninitialized_list;
	OpCodeLocation* uninitialized_list_last;
	OpCodeLocation* hashtable[CODE_LOCATION_HASHTABLE_SIZE];

private:
	static OP_STATUS internal_initialize(void);
};

#endif // MEMTOOLS_ENABLE_CODELOC

#endif // MEMTOOLS_CODELOC_H
